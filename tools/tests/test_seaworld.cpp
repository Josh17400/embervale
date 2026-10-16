// rpg_test --seaworld [--seeds A..B] [--quick] [--ascii]: M10 Beyond the Mist, the WORLD lane's sea (VISION_PLAN 2.12,
// 13 M10 lane A, 15.4, 15.31). Per seed, with numbers printed:
//   - the lands: the mainland at the origin; continent 2 (not hidden) whose nearest coast lies 1500-4000 tiles of open
//     deep sea from the mainland's coast, on another landmass; a hidden land within ~9000 tiles of the start with open
//     water from the mainland side to its Mist ring; every hidden land set apart from all other land by MIST_RING + 200
//     tiles of open water; archipelagos of many small islands; every land named;
//   - the hidden peoples: the cultures on hidden lands are hidden (and isolated), their sites SPF_HIDDEN, their masterwork
//     alloy needs a reagent only hidden lands yield;
//   - the harbours: every port's pier head is a pier tile (Ground::Bridge) joined to the land, its berth deep water; a
//     harbour within ~2500 tiles of the start;
//   - the sea places: wrecks, lighthouses, islet POIs, sea caves (mouth on the water), pirate coves, the chart places and
//     trials of every hidden land / kingdom, merrow dens;
//   - streaming at sea: all-ocean chunks are cheap;
//   - timings of landAt / mistAt / seaAt / hiddenLandAt / portsIn; determinism across two sources and orders.
// Integer maths only in the checks (the world under test is integer / Q16).
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/world/biomes.h"
#include "rpg/world/coords.h"
#include "rpg/world/dmath.h"
#include "rpg/world/poi.h"
#include "rpg/world/settlement.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

using Clock = std::chrono::steady_clock;
double msSince(Clock::time_point t0) { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); }
int64_t d2(int32_t ax, int32_t ay, int32_t bx, int32_t by) { const int64_t dx = (int64_t)ax - bx, dy = (int64_t)ay - by; return dx * dx + dy * dy; }
int32_t idist(int32_t ax, int32_t ay, int32_t bx, int32_t by) { return (int32_t)ew::isqrt((uint64_t)d2(ax, ay, bx, by)); }

int g_bad = 0;
#define SW_FAIL(...) do { out("FAIL: seaworld: " __VA_ARGS__); g_bad++; } while (0)

struct Pt { int32_t x, y; };

// the coast points of a land: grid samples of the land whose 4-neighbour (step tiles away) is not that land
std::vector<Pt> coastOf(ew::EndlessSource& src, const ew::LandPlan& L, int32_t step) {
  const int32_t reach = L.hidden ? L.r * 112 / 100 : L.r * 118 / 100 + 1400;
  std::vector<Pt> out;
  const int32_t x0 = L.cx - reach, y0 = L.cy - reach, n = 2 * reach / step + 1;
  std::vector<uint8_t> in((size_t)n * n, 0);
  for (int j = 0; j < n; j++)
    for (int i = 0; i < n; i++) in[(size_t)j * n + i] = src.landAt(x0 + i * step, y0 + j * step) == L.id ? 1 : 0;
  for (int j = 0; j < n; j++)
    for (int i = 0; i < n; i++) {
      if (!in[(size_t)j * n + i]) continue;
      const bool edge = i == 0 || j == 0 || i == n - 1 || j == n - 1 || !in[(size_t)j * n + i - 1] || !in[(size_t)j * n + i + 1] ||
                        !in[(size_t)(j - 1) * n + i] || !in[(size_t)(j + 1) * n + i];
      if (edge) out.push_back({x0 + i * step, y0 + j * step});
    }
  return out;
}
// the nearest pair of points between two sets
int32_t nearest(const std::vector<Pt>& a, const std::vector<Pt>& b, Pt* pa = nullptr, Pt* pb = nullptr) {
  int64_t best = INT64_MAX;
  for (const Pt& p : a)
    for (const Pt& q : b) {
      const int64_t d = d2(p.x, p.y, q.x, q.y);
      if (d < best) { best = d; if (pa) *pa = p; if (pb) *pb = q; }
    }
  return best == INT64_MAX ? -1 : (int32_t)ew::isqrt((uint64_t)best);
}

void landsOf(ew::EndlessSource& src, uint64_t seed, bool quick) {
  const ew::StartPlan& sp = src.start();
  const int32_t hx = sp.spawn.x, hy = sp.spawn.y;
  const uint32_t oid = src.landAt(0, 0);
  const ew::LandPlan O = src.land(oid);
  if (!oid || !O.origin || O.hidden) { SW_FAIL("seed %llu: no mainland at the origin\n", (unsigned long long)seed); return; }
  const std::vector<ew::LandPlan> lands = src.landsIn(-16000, -16000, 16000, 16000);
  int nHidden = 0, nArchi = 0, unnamed = 0;
  for (const ew::LandPlan& L : lands) {
    nHidden += L.hidden;
    nArchi += L.kind == ew::LandKind::Archipelago;
    if (L.name.empty()) unnamed++;
  }
  if (unnamed) SW_FAIL("seed %llu: %d lands without a name\n", (unsigned long long)seed, unnamed);
  const int32_t step = quick ? 48 : 32;
  const std::vector<Pt> coastO = coastOf(src, O, step);
  // ---- continent 2: the nearest non-hidden continent other than the mainland
  int32_t bestGap = -1;
  ew::LandPlan C2;
  Pt pa{0, 0}, pb{0, 0};
  for (const ew::LandPlan& L : lands) {
    if (L.origin || L.hidden || L.kind != ew::LandKind::Continent) continue;
    if (idist(L.cx, L.cy, 0, 0) > 13000) continue;
    const std::vector<Pt> c = coastOf(src, L, step);
    if (c.empty()) continue;
    Pt a, b;
    const int32_t g = nearest(coastO, c, &a, &b);
    if (g < 0) continue;
    if (g >= 1500 - step && g <= 4000 + step && (bestGap < 0 || std::abs(g - 2700) < std::abs(bestGap - 2700))) { bestGap = g; C2 = L; pa = a; pb = b; }
  }
  if (bestGap < 0) {
    SW_FAIL("seed %llu: no continent 2 with its coast 1500-4000 tiles from the mainland's\n", (unsigned long long)seed);
    for (const ew::LandPlan& L : lands) {
      if (L.origin || L.hidden || idist(L.cx, L.cy, 0, 0) > 13000) continue;
      const std::vector<Pt> c = coastOf(src, L, step);
      out("  land %08x %s r %d at %d,%d: %zu coast samples, gap %d\n", L.id, L.kind == ew::LandKind::Archipelago ? "isles" : "continent", L.r,
          L.cx, L.cy, c.size(), c.empty() ? -1 : nearest(coastO, c));
    }
  }
  else {
    // the strait: open sea all the way (no land, deep water beyond the coastal shallows)
    int landN = 0, shallowN = 0, deepN = 0, stormN = 0;
    const int32_t L = idist(pa.x, pa.y, pb.x, pb.y);
    for (int32_t s = 64; s <= L - 64; s += 16) {
      const int32_t x = pa.x + (int32_t)((int64_t)(pb.x - pa.x) * s / L), y = pa.y + (int32_t)((int64_t)(pb.y - pa.y) * s / L);
      const ew::SeaKind k = src.seaAt(x, y);
      if (k == ew::SeaKind::Land) landN++;
      else if (k == ew::SeaKind::Shallow) shallowN++;
      else if (k == ew::SeaKind::Deep) deepN++;
      else stormN++;
    }
    // (the landmass of each side a little inland of its coast point: a coast sample may be a speck of its own)
    const int32_t ix = (int32_t)((int64_t)(pb.x - pa.x) * 400 / std::max(1, L)), iy = (int32_t)((int64_t)(pb.y - pa.y) * 400 / std::max(1, L));
    const uint32_t mo = src.landmass(pa.x - ix, pa.y - iy), m2 = src.landmass(pb.x + ix, pb.y + iy);
    out("seaworld: seed %llu: continent 2 '%s' (r %d) coast %d tiles from the mainland's (%d,%d)->(%d,%d); strait land %d shallow %d deep %d storm %d; landmass %08x / %08x\n",
        (unsigned long long)seed, C2.name.c_str(), C2.r, bestGap, pa.x, pa.y, pb.x, pb.y, landN, shallowN, deepN, stormN, mo, m2);
    if (landN > 2) SW_FAIL("seed %llu: %d land samples in the strait to continent 2\n", (unsigned long long)seed, landN);
    if (deepN < (landN + shallowN + deepN + stormN) / 2) SW_FAIL("seed %llu: the strait to continent 2 is not open deep sea\n", (unsigned long long)seed);
    if (mo && mo == m2) SW_FAIL("seed %llu: continent 2 shares the mainland's landmass (%08x)\n", (unsigned long long)seed, mo);
  }
  // ---- hidden lands: one within ~9000 tiles of the start; every one set apart by open water; its ring reachable
  int nearHidden = 0;
  for (const ew::LandPlan& L : lands) {
    if (!L.hidden) continue;
    const int32_t dStart = idist(L.cx, L.cy, hx, hy) - L.r;
    if (dStart <= 9000) nearHidden++;
    // separation: no land tile from 1.12 r out to the Mist ring's end + 200
    int landRing = 0, own = 0;
    const int32_t rIn = L.r * 112 / 100 + 8, rOut = L.mistOut + 200;
    for (int32_t r = rIn; r <= rOut; r += quick ? 40 : 24) {
      const int n = std::max(16, (int)(r * 6283 / 1000 / (quick ? 48 : 32)));
      for (int k = 0; k < n; k++) {
        const int32_t a = (int32_t)((int64_t)k * 1024 / n);
        const int32_t x = L.cx + ew::icosR(a, r), y = L.cy + ew::isinR(a, r);
        if (src.landAt(x, y) || src.seaAt(x, y) == ew::SeaKind::Land) landRing++;
      }
    }
    for (int32_t y = L.cy - L.r; y <= L.cy + L.r; y += 64)
      for (int32_t x = L.cx - L.r; x <= L.cx + L.r; x += 64) own += src.landAt(x, y) == L.id;
    // the approach from the mainland: from the ring's end toward the origin, open water until the mainland
    int32_t dx = -L.cx, dy = -L.cy;
    const int32_t dl = std::max<int32_t>(1, idist(0, 0, dx, dy));
    int approachLand = 0, approachDeep = 0, approachN = 0;
    for (int32_t s = L.mistOut + 20; s < dl; s += 32) {
      const int32_t x = L.cx + (int32_t)((int64_t)dx * s / dl), y = L.cy + (int32_t)((int64_t)dy * s / dl);
      const uint32_t la = src.landAt(x, y);
      if (la == oid) break;
      approachN++;
      if (la) approachLand++;
      else if (src.seaAt(x, y) != ew::SeaKind::Shallow) approachDeep++;
    }
    out("seaworld: seed %llu: hidden '%s' (%s r %d) at %d,%d, %d tiles from the start: own land %d samples, land within its guard %d, approach %d samples (%d land, %d deep)\n",
        (unsigned long long)seed, L.name.c_str(), L.kind == ew::LandKind::Archipelago ? "isles" : "continent", L.r, L.cx, L.cy, dStart, own,
        landRing, approachN, approachLand, approachDeep);
    if (landRing) SW_FAIL("seed %llu: hidden land %u has %d land samples within MIST_RING + 200 of its coast\n", (unsigned long long)seed, L.id, landRing);
    if (own < 20) SW_FAIL("seed %llu: hidden land %u has almost no land (%d samples)\n", (unsigned long long)seed, L.id, own);
    if (dStart <= 9000 && approachLand > 0) SW_FAIL("seed %llu: land between the mainland and hidden land %u's Mist\n", (unsigned long long)seed, L.id);
    const cult::Culture& C = src.culture(src.cultureAt(L.cx, L.cy));
    bool reagent = false;
    for (const cult::Alloy& A : C.arms.alloys)
      for (const cult::Ingredient& g : A.recipe) reagent |= g.ore < 0 && cult::isHiddenReagent(g.reagent);
    if (own >= 20 && (!C.hidden || C.hiddenLand != L.id || !C.isolated || !reagent))
      SW_FAIL("seed %llu: hidden land %u's people are not hidden (%d) / isolated (%d) / have no hidden reagent (%d)\n", (unsigned long long)seed, L.id, C.hidden, C.isolated, reagent);
  }
  if (!nearHidden) SW_FAIL("seed %llu: no hidden land within 9000 tiles of the start\n", (unsigned long long)seed);
  // ---- archipelagos: islands per archipelago (components on a 32-tile grid)
  int islandsTotal = 0, islets = 0, archiDone = 0;
  for (const ew::LandPlan& L : lands) {
    if (L.kind != ew::LandKind::Archipelago || L.hidden || archiDone >= (quick ? 1 : 2)) continue;
    if (idist(L.cx, L.cy, 0, 0) > 14000) continue;
    archiDone++;
    const int32_t g = 32, reach = L.r * 118 / 100 + 1400, x0 = L.cx - reach, y0 = L.cy - reach, n = 2 * reach / g + 1;
    std::vector<uint8_t> in((size_t)n * n);
    for (int j = 0; j < n; j++)
      for (int i = 0; i < n; i++) in[(size_t)j * n + i] = src.landAt(x0 + i * g, y0 + j * g) == L.id;
    std::vector<int> q;
    int comps = 0, small = 0, biggest = 0;
    for (int s0 = 0; s0 < n * n; s0++) {
      if (!in[(size_t)s0]) continue;
      q.assign(1, s0);
      in[(size_t)s0] = 0;
      for (size_t h = 0; h < q.size(); h++) {
        const int c = q[h], i = c % n, j = c / n;
        const int nb[4] = {i > 0 ? c - 1 : -1, i < n - 1 ? c + 1 : -1, j > 0 ? c - n : -1, j < n - 1 ? c + n : -1};
        for (int m : nb) if (m >= 0 && in[(size_t)m]) { in[(size_t)m] = 0; q.push_back(m); }
      }
      comps++;
      if (q.size() <= 40) small++;
      biggest = std::max(biggest, (int)q.size());
    }
    islandsTotal += comps;
    islets += small;
    out("seaworld: seed %llu: archipelago '%s' at %d,%d r %d: %d islands (%d small, the biggest %d cells of 32x32)\n", (unsigned long long)seed,
        L.name.c_str(), L.cx, L.cy, L.r, comps, small, biggest);
    if (comps < 8) SW_FAIL("seed %llu: archipelago %u has only %d islands\n", (unsigned long long)seed, L.id, comps);
  }
  out("seaworld: seed %llu: %zu lands within 16000 tiles (%d hidden, %d archipelagos; %d hidden within 9000 of the start), %d islands in %d archipelagos (%d islets)\n",
      (unsigned long long)seed, lands.size(), nHidden, nArchi, nearHidden, islandsTotal, archiDone, islets);
}

// ---- the harbours: within reach of the start; every port's pier and berth as the chunks lay them
void harbours(ew::EndlessSource& src, uint64_t seed, bool quick) {
  const ew::StartPlan& sp = src.start();
  const int32_t hx = sp.spawn.x, hy = sp.spawn.y;
  const std::vector<ew::PortPlan> ports = src.portsIn(hx - 6000, hy - 6000, hx + 6000, hy + 6000);
  int32_t nearest = -1;
  const ew::PortPlan* np = nullptr;
  int nHarb = 0, nJetty = 0, dredged = 0;
  int townJetty = 0;
  for (const ew::PortPlan& P : ports) {
    if (!P.harbour) { nJetty++; townJetty += P.type != SiteType::Village; continue; }
    nHarb++;
    dredged += P.dredged;
    const int32_t d = idist(P.x, P.y, hx, hy);
    if (nearest < 0 || d < nearest) { nearest = d; np = &P; }
  }
  out("seaworld: seed %llu: %d harbours and %d fishing berths (%d of towns without a mole) within 6000 tiles of the start (%d dredged); the nearest harbour %d tiles away%s%s\n",
      (unsigned long long)seed, nHarb, nJetty, townJetty, dredged, nearest, np ? " at " : "", np ? np->name.c_str() : "");
  if (nearest < 0 || nearest > 2600) {
    SW_FAIL("seed %llu: no harbour within ~2500 tiles of the start (nearest %d)\n", (unsigned long long)seed, nearest);
    for (const ew::SettlementNode& n : src.settlementsIn(hx - 4000, hy - 4000, hx + 4000, hy + 4000))
      if (ew::idLocal(n.id) == 0xF09) {
        const ew::PortPlan* pp = nullptr;
        for (const ew::PortPlan& P : ports) if (P.site == n.id) pp = &P;
        out("  the start's port %d,%d (%d tiles): %s\n", n.x, n.y, idist(n.x, n.y, hx, hy), pp ? (pp->harbour ? "a harbour" : "no mole") : "not a port");
      }
  }
  // the chunks: the pier head is a pier tile joined to the landing, the berth (and its room) deep water, the moorings deep
  int checked = 0;
  for (const ew::PortPlan& P : ports) {
    if (!P.harbour || checked >= (quick ? 2 : 4)) continue;
    checked++;
    std::map<std::pair<int32_t, int32_t>, std::shared_ptr<ew::ChunkData>> cache;
    auto ground = [&](int32_t x, int32_t y) {
      const int32_t cx = ew::floorDiv(x, ew::CHUNK), cy = ew::floorDiv(y, ew::CHUNK);
      auto& c = cache[{cx, cy}];
      if (!c) { c = std::make_shared<ew::ChunkData>(); src.chunk(cx, cy, *c); }
      return (Ground)c->ground[c->at(x - cx * ew::CHUNK, y - cy * ew::CHUNK)];
    };
    int bad = 0;
    if (ground(P.pierX, P.pierY) != Ground::Bridge) { SW_FAIL("seed %llu: %s's pier head is not a pier tile\n", (unsigned long long)seed, P.name.c_str()); bad++; }
    for (int k = 1; k <= P.pierLen; k++)
      if (ground(P.rootX + P.dirX * k, P.rootY + P.dirY * k) != Ground::Bridge) { bad++; break; }
    if (groundWater(ground(P.rootX, P.rootY)) || groundSolid(ground(P.rootX, P.rootY))) bad++;
    for (int dy = -2; dy <= 2; dy++)
      for (int dx = -2; dx <= 2; dx++)
        if (ground(P.berthX + dx, P.berthY + dy) != Ground::DeepWater) { bad++; dy = 3; break; }
    int deepMoor = 0;
    for (const ew::GTile& m : P.moorings) deepMoor += ground(m.x, m.y) == Ground::DeepWater;
    out("seaworld: seed %llu: harbour %s: pier %d tiles from %d,%d (%+d,%+d), head %d,%d, berth %d,%d%s, moorings %d/%zu deep%s\n",
        (unsigned long long)seed, P.name.c_str(), P.pierLen, P.rootX, P.rootY, P.dirX, P.dirY, P.pierX, P.pierY, P.berthX, P.berthY,
        P.dredged ? " (dredged)" : "", deepMoor, P.moorings.size(), bad ? " BAD" : "");
    if (bad) SW_FAIL("seed %llu: harbour %s is not laid as planned (%d faults)\n", (unsigned long long)seed, P.name.c_str(), bad);
  }
}

// ---- the sea's places round the start (the regions within `R` regions of it) and every hidden land's chain places
void seaPlaces(ew::EndlessSource& src, uint64_t seed, bool quick) {
  const ew::StartPlan& sp = src.start();
  const int32_t srx = ew::floorDiv(sp.spawn.x, ew::REGION), sry = ew::floorDiv(sp.spawn.y, ew::REGION);
  const int R = quick ? 9 : 13;
  int wrecks = 0, isletWrecks = 0, lights = 0, caches = 0, coves = 0, caves = 0, merrow = 0, islets = 0, chartSites = 0, trialSites = 0;
  int harbours = 0, lit = 0;
  std::vector<ew::SitePlan> caveList;
  for (int ry = sry - R; ry <= sry + R; ry++)
    for (int rx = srx - R; rx <= srx + R; rx++) {
      const ew::RegionPlan P = src.region(rx, ry);   // (a copy)
      for (const ew::SitePlan& s : P.sites) {
        if (s.flags & ew::SPF_ISLET) islets++;
        if (s.flags & ew::SPF_CHART) chartSites++;
        if (s.flags & ew::SPF_TRIAL) trialSites++;
        if (s.type == SiteType::Vignette && s.kind == (uint8_t)ew::VignetteKind::Shipwreck) { wrecks++; isletWrecks += (s.flags & ew::SPF_ISLET) != 0; }
        if (s.type == SiteType::Vignette && s.kind == (uint8_t)ew::VignetteKind::Lighthouse) lights++;
        if (s.flags & ew::SPF_CACHE) caches++;
        if (s.flags & ew::SPF_PIRATE) coves++;
        if (s.flags & ew::SPF_SEACAVE) { caves++; if (caveList.size() < 6) caveList.push_back(s); }
        if ((s.flags & ew::SPF_PORT) && (s.type == SiteType::Town || s.type == SiteType::City)) harbours++;
      }
      for (const ew::DenPlan& d : P.dens) merrow += d.mon == art::Monster::Merrow;
    }
  // every harbour with its lighthouse? (a headland near it is not always there)
  for (const ew::PortPlan& Pp : src.portsIn(sp.spawn.x - R * ew::REGION, sp.spawn.y - R * ew::REGION, sp.spawn.x + R * ew::REGION, sp.spawn.y + R * ew::REGION)) {
    if (!Pp.harbour) continue;
    bool any = false;
    for (int ry = ew::floorDiv(Pp.y - 240, ew::REGION); ry <= ew::floorDiv(Pp.y + 240, ew::REGION) && !any; ry++)
      for (int rx = ew::floorDiv(Pp.x - 240, ew::REGION); rx <= ew::floorDiv(Pp.x + 240, ew::REGION) && !any; rx++)
        for (const ew::SitePlan& s : src.region(rx, ry).sites)
          if (s.type == SiteType::Vignette && s.kind == (uint8_t)ew::VignetteKind::Lighthouse && d2(s.ex, s.ey, Pp.x, Pp.y) < 240ll * 240ll) any = true;
    lit += any;
  }
  // sea caves: the mouth on the water (the chunks)
  int mouthsWet = 0;
  for (const ew::SitePlan& s : caveList) {
    ew::ChunkData c;
    const int32_t x = s.ex, y = s.ey + 1, cx = ew::floorDiv(x, ew::CHUNK), cy = ew::floorDiv(y, ew::CHUNK);
    src.chunk(cx, cy, c);
    mouthsWet += groundWater((Ground)c.ground[c.at(x - cx * ew::CHUNK, y - cy * ew::CHUNK)]);
  }
  out("seaworld: seed %llu: sea places within %d regions of the start: %d wrecks (%d on islets), %d lighthouses (%d of %d harbours lit), %d islet places "
      "(%d caches, %d pirate coves, %d sea caves in all), %d merrow dens, %d chart places, %d trials; sea caves opening on the water %d/%zu\n",
      (unsigned long long)seed, R, wrecks, isletWrecks, lights, lit, harbours, islets, caches, coves, caves, merrow, chartSites, trialSites, mouthsWet,
      caveList.size());
  if (!wrecks || !merrow) SW_FAIL("seed %llu: the sea round the start has no wreck / merrow den\n", (unsigned long long)seed);
  if (mouthsWet != (int)caveList.size()) SW_FAIL("seed %llu: a sea cave's mouth does not open on the water\n", (unsigned long long)seed);
  // every hidden land within reach: its chain's wreck and sea cave on the mainland side, 1500-5000 tiles out from its
  // Mist, in their region plans; its kingdoms' two trials on its own land
  for (const ew::LandPlan& L : src.landsIn(sp.spawn.x - 14000, sp.spawn.y - 14000, sp.spawn.x + 14000, sp.spawn.y + 14000)) {
    if (!L.hidden) continue;
    ew::SitePlan w, c;
    if (!src.chartPlaces(L.id, w, c)) { SW_FAIL("seed %llu: hidden land %u (%s) has no chart places\n", (unsigned long long)seed, L.id, L.name.c_str()); continue; }
    const int32_t dw = idist(w.ex, w.ey, L.cx, L.cy) - L.mistOut, dc = idist(c.ex, c.ey, L.cx, L.cy) - L.mistOut;
    bool inPlans = false, cInPlans = false;
    for (const ew::SitePlan& s : src.region(ew::floorDiv(w.ex, ew::REGION), ew::floorDiv(w.ey, ew::REGION)).sites)
      if (s.id == w.id && (s.flags & ew::SPF_CHART) && s.chartLand == L.id) inPlans = true;
    for (const ew::SitePlan& s : src.region(ew::floorDiv(c.ex, ew::REGION), ew::floorDiv(c.ey, ew::REGION)).sites)
      if (s.id == c.id && (s.flags & ew::SPF_CHART) && (s.flags & ew::SPF_SEACAVE) && s.chartLand == L.id) cInPlans = true;
    out("seaworld: seed %llu: hidden '%s': chart wreck %d tiles past its Mist at %d,%d%s, sea cave %d tiles past at %d,%d%s\n", (unsigned long long)seed,
        L.name.c_str(), dw, w.ex, w.ey, inPlans ? "" : " (NOT PLANNED)", dc, c.ex, c.ey, cInPlans ? "" : " (NOT PLANNED)");
    if (dw < 1500 || dw > 5000 || dc < 1500 || dc > 5000 || !inPlans || !cInPlans || src.hiddenLandAt(w.ex, w.ey) || src.hiddenLandAt(c.ex, c.ey))
      SW_FAIL("seed %llu: hidden land %u's chart places are out of place\n", (unsigned long long)seed, L.id);
    // its kingdoms' trials (kingdom cells over its shape)
    int kingdoms = 0, trials = 0;
    std::set<ew::Gid> seen;
    for (int32_t y = L.cy - L.r; y <= L.cy + L.r; y += 256)
      for (int32_t x = L.cx - L.r; x <= L.cx + L.r; x += 256) {
        const ew::Gid k = src.kingdomAt(x, y);
        if (!k || !seen.insert(k).second) continue;
        const ew::KingdomPlan* K = src.kingdom(k);
        if (!K || K->hiddenLand != L.id) continue;
        kingdoms++;
        for (int which = 0; which < 2; which++) {
          ew::SitePlan t;
          if (!src.trialOf(k, which, t)) continue;
          if (src.landAt(t.ex, t.ey) != L.id || !(t.flags & ew::SPF_TRIAL)) { SW_FAIL("seed %llu: a trial of kingdom %llx is off its land\n", (unsigned long long)seed, (unsigned long long)k); continue; }
          trials++;
        }
      }
    out("seaworld: seed %llu: hidden '%s': %d kingdoms, %d trials\n", (unsigned long long)seed, L.name.c_str(), kingdoms, trials);
    if (kingdoms && trials < kingdoms * 2) SW_FAIL("seed %llu: hidden land %u: %d trials for %d kingdoms\n", (unsigned long long)seed, L.id, trials, kingdoms);
    // its places are SPF_HIDDEN and its settlements' people hidden
    int sites = 0, unmarked = 0, mainlandFolk = 0;
    const int32_t rcx = ew::floorDiv(L.cx, ew::REGION), rcy = ew::floorDiv(L.cy, ew::REGION);
    for (int ry = rcy - 1; ry <= rcy + 1; ry++)
      for (int rx = rcx - 1; rx <= rcx + 1; rx++)
        for (const ew::SitePlan& s : src.region(rx, ry).sites) {
          if (src.landAt(s.ex, s.ey) != L.id) continue;
          sites++;
          if (!(s.flags & ew::SPF_HIDDEN)) unmarked++;
          if ((s.type == SiteType::Town || s.type == SiteType::Village || s.type == SiteType::City) && !src.culture(s.culture).hidden) mainlandFolk++;
        }
    if (unmarked || mainlandFolk) SW_FAIL("seed %llu: hidden land %u: %d of %d sites not SPF_HIDDEN, %d settlements of a mainland people\n", (unsigned long long)seed, L.id, unmarked, sites, mainlandFolk);
  }
}

// ---- streaming at sea: a galleon (11 tiles/s) sails 3000 tiles out from the mainland across open water; the chunks
//      of a 7 x 5 window round its bow are made as it goes (a fresh source: cold caches, as at sea). Every new chunk
//      must be ready long before the bow reaches the window's edge (a column of chunks every 32 / 11 s), and no single
//      chunk may stall a frame.
void streaming(uint64_t seed) {
  ew::EndlessSource src(seed);
  const ew::StartPlan& sp = src.start();
  // out to sea along the bearing of the nearest open water from the start
  int32_t bx = sp.spawn.x, by = sp.spawn.y, dx = 1, dy = 0;
  {
    int32_t best = INT32_MAX;
    static const int D[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
    for (const auto& d : D) {
      int32_t r = 0;
      while (r < 8000 && src.landAt(bx + d[0] * r, by + d[1] * r)) r += 32;
      if (r < best) { best = r; dx = d[0]; dy = d[1]; }
    }
    bx += dx * best; by += dy * best;
  }
  std::set<std::pair<int32_t, int32_t>> made;
  double worst = 0, total = 0, colWorst = 0, oceanMs = 0, oceanWorst = 0, seaColWorst = 0;
  int n = 0, oceanN = 0, compared = 0;
  const int steps = 3000 / 32 + 1;
  for (int s = 0; s < steps; s++) {
    const int32_t x = bx + dx * s * 32, y = by + dy * s * 32;
    const int32_t ccx = ew::floorDiv(x, ew::CHUNK), ccy = ew::floorDiv(y, ew::CHUNK);
    double col = 0;
    for (int j = -2; j <= 2; j++)
      for (int i = -3; i <= 3; i++) {
        if (!made.insert({ccx + i, ccy + j}).second) continue;
        ew::ChunkData c;
        const auto t0 = Clock::now();
        src.chunk(ccx + i, ccy + j, c);
        const double ms = msSince(t0);
        bool ocean = true;
        for (int q = 0; q < ew::ChunkData::N && ocean; q++) ocean = c.ground[q] == (uint8_t)Ground::DeepWater;
        oceanN += ocean;
        if (ocean && n % 9 == 0) {   // the fast path's chunk is the full pipeline's, byte for byte
          ew::ChunkData full;
          src.oceanFastPath(false);
          src.chunk(ccx + i, ccy + j, full);
          src.oceanFastPath(true);
          const bool same = !memcmp(c.ground, full.ground, sizeof c.ground) && !memcmp(c.prop, full.prop, sizeof c.prop) &&
                            !memcmp(c.biome, full.biome, sizeof c.biome) && !memcmp(c.blend, full.blend, sizeof c.blend) &&
                            !memcmp(c.eco, full.eco, sizeof c.eco) && !memcmp(c.ecoNb, full.ecoNb, sizeof c.ecoNb) &&
                            !memcmp(c.height, full.height, sizeof c.height) && !memcmp(c.wall, full.wall, sizeof c.wall) &&
                            !memcmp(c.urb, full.urb, sizeof c.urb) && full.spawns.empty() && full.bldgs.empty();
          if (!same) { SW_FAIL("seed %llu: the open sea's fast chunk %d,%d differs from the full pipeline's\n", (unsigned long long)seed, ccx + i, ccy + j); }
          compared++;
        }
        if (ocean) { oceanMs += ms; oceanWorst = std::max(oceanWorst, ms); }
        worst = std::max(worst, ms);
        total += ms;
        col += ms;
        n++;
      }
    colWorst = std::max(colWorst, col);
    if (s >= 10) seaColWorst = std::max(seaColWorst, col);
  }
  const double budget = 32.0 / 11.0 * 1000.0;   // ms between columns at galleon speed
  out("seaworld: seed %llu: streaming at sea: %d chunks (%d all deep water: avg %.3f ms, worst %.2f ms) over 3000 tiles: avg %.2f ms, worst %.1f ms, the worst column %.1f ms of its %.0f ms\n",
      (unsigned long long)seed, n, oceanN, oceanN ? oceanMs / oceanN : 0.0, oceanWorst, n ? total / n : 0.0, worst, colWorst, budget);
  out("seaworld: seed %llu: at sea (past the first 320 tiles: the coast's towns and regions are the land's cost): the worst column %.1f ms; the open sea's fast chunks matched the full pipeline on %d compared\n",
      (unsigned long long)seed, seaColWorst, compared);
  if (seaColWorst > budget / 4) SW_FAIL("seed %llu: a column of chunks at sea took %.1f ms (galleon budget %.0f ms)\n", (unsigned long long)seed, seaColWorst, budget);
}

void timings(ew::EndlessSource& src, uint64_t seed) {
  const int N = 20000;
  uint64_t h = seed * 0x9E3779B97F4A7C15ull;
  auto rnd = [&]() { h ^= h << 13; h ^= h >> 7; h ^= h << 17; return (int32_t)(h % 24000) - 12000; };
  volatile uint32_t sink = 0;
  auto t0 = Clock::now();
  for (int i = 0; i < N; i++) sink = sink + src.landAt(rnd(), rnd());
  const double tLand = msSince(t0) * 1000.0 / N;
  t0 = Clock::now();
  for (int i = 0; i < N; i++) sink = sink + (uint32_t)src.mistAt(rnd(), rnd());
  const double tMist = msSince(t0) * 1000.0 / N;
  t0 = Clock::now();
  for (int i = 0; i < N; i++) sink = sink + src.hiddenLandAt(rnd(), rnd());
  const double tHid = msSince(t0) * 1000.0 / N;
  // seaAt along a boat's course (the blocks round it warm, as at sea)
  t0 = Clock::now();
  const int M = 4000;
  for (int i = 0; i < M; i++) sink = sink + (uint32_t)src.seaAt(4000 + i, 1500 + i / 3);
  const double tSea = msSince(t0) * 1000.0 / M;
  t0 = Clock::now();
  size_t ports = 0;
  for (int k = 0; k < 8; k++) ports += src.portsIn(-6000 + k * 1500, -1000, -4500 + k * 1500, 500).size();
  const double tPorts = msSince(t0) / 8;
  out("seaworld: seed %llu: timings: landAt %.2f us, mistAt %.3f us, hiddenLandAt %.3f us, seaAt %.2f us (along a course), portsIn %.1f ms per 1500x1500 box (%zu ports)\n",
      (unsigned long long)seed, tLand, tMist, tHid, tSea, tPorts, ports);
  if (tMist > 5.0 || tHid > 5.0) SW_FAIL("seed %llu: mistAt / hiddenLandAt too dear for a boat step\n", (unsigned long long)seed);
  (void)sink;
}

void determinism(uint64_t seed) {
  ew::EndlessSource a(seed), b(seed);
  // b asks in another order (the far lands first)
  (void)b.landsIn(-30000, -30000, 30000, 30000);
  (void)b.region(20, -14);
  uint64_t ha = 1469598103934665603ull, hb = ha;
  for (int32_t y = -12000; y <= 12000; y += 997)
    for (int32_t x = -12000; x <= 12000; x += 1009) {
      const uint32_t va[3] = {a.landAt(x, y), a.hiddenLandAt(x, y), (uint32_t)a.mistAt(x, y)};
      for (uint32_t v : va) { ha ^= v; ha *= 1099511628211ull; }
    }
  for (int32_t y = 12000; y >= -12000; y -= 997)
    (void)y;
  for (int32_t y = -12000; y <= 12000; y += 997)
    for (int32_t x = -12000; x <= 12000; x += 1009) {
      const uint32_t vb[3] = {b.landAt(x, y), b.hiddenLandAt(x, y), (uint32_t)b.mistAt(x, y)};
      for (uint32_t v : vb) { hb ^= v; hb *= 1099511628211ull; }
    }
  if (ha != hb) SW_FAIL("seed %llu: the lands differ between two sources asked in different orders\n", (unsigned long long)seed);
}

// --png DIR: the lands round the origin, one pixel per `step` tiles (mainland green, other lands tan, hidden lands
// violet, the Mist grey, the sea blue; ports red, the start yellow)
void landsPng(ew::EndlessSource& src, uint64_t seed, const char* dir, int32_t R, int32_t step, int32_t ox = 0, int32_t oy = 0) {
  const int n = 2 * R / step;
  std::vector<uint32_t> px((size_t)n * n);
  const uint32_t oid = src.landAt(0, 0);
  for (int j = 0; j < n; j++)
    for (int i = 0; i < n; i++) {
      const int32_t x = ox - R + i * step, y = oy - R + j * step;
      const uint32_t id = src.landAt(x, y);
      uint32_t c;
      if (id) {
        const ew::LandPlan L = src.land(id);
        if (id == oid) c = 0xFF3C8C46u;
        else if (L.hidden) c = 0xFF9A4C8Cu;
        else c = 0xFF5AA0B4u;
      } else {
        const int32_t m = src.mistAt(x, y);
        c = m ? 0xFFB0A8A0u : 0xFF8A4A1Eu;
      }
      px[(size_t)j * n + i] = c;
    }
  auto dot = [&](int32_t x, int32_t y, uint32_t c, int r) {
    const int ci = (x - ox + R) / step, cj = (y - oy + R) / step;
    for (int dj = -r; dj <= r; dj++)
      for (int di = -r; di <= r; di++)
        if (ci + di >= 0 && cj + dj >= 0 && ci + di < n && cj + dj < n) px[(size_t)(cj + dj) * n + ci + di] = c;
  };
  for (const ew::PortPlan& P : src.portsIn(ox - R, oy - R, ox + R, oy + R)) dot(P.x, P.y, P.harbour ? 0xFF0000FFu : 0xFF6060C0u, P.harbour ? 3 : 1);
  dot(src.start().spawn.x, src.start().spawn.y, 0xFF00FFFFu, 3);
  char path[512];
  std::snprintf(path, sizeof path, "%s/seaworld_s%llu.png", dir, (unsigned long long)seed);
  writePng(path, n, n, px);
  out("seaworld: seed %llu: wrote %s\n", (unsigned long long)seed, path);
}

int seaworldCmd(int argc, char** argv) {
  uint64_t a = 1, b = 5;
  bool quick = false;
  const char* pngDir = nullptr;
  int32_t pngX = 0, pngY = 0, pngR = 12000, pngStep = 32;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--quick")) quick = true;
    else if (!strcmp(argv[i], "--png") && i + 1 < argc) pngDir = argv[++i];
    else if (!strcmp(argv[i], "--png-at") && i + 1 < argc) { pngX = atoi(argv[++i]); const char* c = strchr(argv[i], ','); pngY = c ? atoi(c + 1) : 0; }
    else if (!strcmp(argv[i], "--png-r") && i + 1 < argc) pngR = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--png-step") && i + 1 < argc) pngStep = atoi(argv[++i]);
  }
  g_bad = 0;
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    ew::EndlessSource src(s);
    if (pngDir) { landsPng(src, s, pngDir, pngR, pngStep, pngX, pngY); continue; }
    landsOf(src, s, quick);
    harbours(src, s, quick);
    seaPlaces(src, s, quick);
    streaming(s);
    timings(src, s);
    determinism(s);
  }
  out("seaworld: %d failures\n", g_bad);
  return g_bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--seaworld", "M10 WORLD: the lands (continent 2 across open sea, hidden lands set apart, archipelagos), hidden peoples and metals, harbours, sea places, streaming at sea, timings, determinism [--seeds A..B] [--quick]", seaworldCmd);
