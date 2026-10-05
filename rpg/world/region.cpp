// L1 (VISION_PLAN 2.5 L1a, L1b, 2.6, 7.1, 15.8): kingdom cells and their capitals, the start plan, settlement lattices
// at the owner's scale, wilderness POIs (caves on real cliff faces, ruins, camps, shrines, dens, a lair per 2x2
// kingdom cells), names, danger, and the region plans that gather it all. WORLD lane.
//
// Spacing (VISION_PLAN 15.8: settlements much further apart, a real walk through wilderness between them):
//   cities   one per 1024-tile kingdom cell (its capital), jittered in [300, 724] of the cell: >= 600 apart
//   towns    one candidate per 448-tile cell in [112, 336]: >= 224 apart; clear of capitals by 330
//   villages one candidate per 256-tile cell (= one region) in [76, 180]: >= 152 apart; clear of towns by 200,
//            capitals by 250
// Lower classes test the raw candidates of higher classes (pure, so suppression never chains). Every settlement id is
// makeId(region, Site, 1 city / 2 town / 3 village) (one of each class per region at most); the start plan's sites use
// local ids 0xF00.. and POIs 0x100.. (ids never depend on a region plan having been built).
#include <algorithm>
#include <cstdint>
#include <string>
#include "rpg/world/gen.h"

namespace ew {
using namespace gen;
using art::Monster;

namespace {
constexpr int32_t TOWN_CELL = 448, TOWN_M = 112;
constexpr int32_t VIL_CELL = 256, VIL_M = 76;
constexpr int32_t POI_CELL = 128, POI_M = 20;
constexpr int32_t DEN_CELL = 64, DEN_M = 10;
constexpr int32_t LAIR_CELL = 2048;
constexpr uint32_t LOCAL_CITY = 1, LOCAL_TOWN = 2, LOCAL_VILLAGE = 3, LOCAL_LAIR = 0xF0, LOCAL_FORCED = 0xF00;

uint64_t typeKey(SiteType t, int32_t cx, int32_t cy) { return mix64(key2(cx, cy) ^ ((uint64_t)t * 0x9E3779B97F4A7C15ull)); }

// D_base (VISION_PLAN 7.1): 1 + 11.5 ln(1 + r / 350) as a table
int dangerBase(int32_t r) {
  static const int32_t R[] = {0, 100, 200, 350, 600, 1000, 1500, 2000, 3000, 6000, 10000, 30000};
  static const int32_t D[] = {1, 4, 6, 9, 12, 17, 20, 23, 27, 34, 40, 52};
  if (r >= 30000) return std::min(60, 52 + (r - 30000) / 2500);
  for (int i = 0; i < 11; i++)
    if (r < R[i + 1]) return D[i] + (int)((int64_t)(D[i + 1] - D[i]) * (r - R[i]) / (R[i + 1] - R[i]));
  return 52;
}

const char* kPrefix[] = {"UPPER ", "LOWER ", "OLD ", "NEW ", "EAST ", "WEST ", "NORTH ", "SOUTH ", "GREAT ", "LITTLE "};
}  // namespace

// ============================================================== habitability (sparse samples; pure)
int EndlessSource::Impl::habitability(SiteType t, int32_t x, int32_t y, int* levelOut) {
  const int32_t R = isSettlement(t) ? nominalR(t) : 8;
  const int n = t == SiteType::City ? 5 : 3;
  int lmin = 99, lmax = -1;
  for (int gy = 0; gy < n; gy++)
    for (int gx = 0; gx < n; gx++) {
      int32_t px = x + (2 * gx - (n - 1)) * R / (n - 1), py = y + (2 * gy - (n - 1)) * R / (n - 1);
      Coarse c = coarse(px, py);
      if (c.e < ELEV_SEA + Q(0.012) || c.rock > Q(0.25)) return 0;
      int lv = levelOf(c.e);
      lmin = std::min(lmin, lv);
      lmax = std::max(lmax, lv);
    }
  Coarse cc = coarse(x, y);
  if (levelOut) *levelOut = levelOf(cc.e);
  int score = 256;
  if (lmax - lmin > 2) return 0;
  if (lmax - lmin == 2) score = score * 55 / 100;
  else if (lmax - lmin == 1) score = score * 85 / 100;
  switch (classify(cc.e, cc.t, cc.m, x, y, false)) {
    case Biome::Snow: score = score * 35 / 100; break;
    case Biome::Taiga: score = score * 70 / 100; break;
    case Biome::Desert: score = score * 50 / 100; break;
    case Biome::Swamp: score = score * 45 / 100; break;
    case Biome::Beach: return 0;
    default: break;
  }
  return std::max(1, score);
}

// ============================================================== settlement lattices
EndlessSource::Impl::Cand EndlessSource::Impl::settleCand(SiteType t, int32_t cx, int32_t cy) {
  uint64_t k = typeKey(t, cx, cy);
  auto it = cands.find(k);
  if (it != cands.end()) return it->second;
  if (cands.size() > 200000) cands.clear();
  const bool town = t == SiteType::Town;
  const int32_t S = town ? TOWN_CELL : VIL_CELL, m = town ? TOWN_M : VIL_M;
  uint64_t h = cellSeed(seed, town ? tag("l.town") : tag("l.village"), cx, cy);
  Cand c;
  c.x = cx * S + m + (int32_t)((h & 0xFFFFFFFFull) % (uint64_t)(S - 2 * m));
  c.y = cy * S + m + (int32_t)((h >> 32) % (uint64_t)(S - 2 * m));
  c.seed = (uint32_t)(mix64(h ^ 0x5EEDu) >> 16);
  int32_t accept = town ? Q(0.66) : Q(0.74);
  if (hq(mix64(h ^ 5)) < accept) {
    int hab = habitability(t, c.x, c.y, nullptr);
    c.ok = hab > 0 && hq(mix64(h ^ 6)) < hab * 256;
  }
  cands.emplace(k, c);
  return c;
}

bool EndlessSource::Impl::settleNode(SiteType t, int32_t cx, int32_t cy, Node& out) {
  uint64_t k = typeKey(t, cx, cy) ^ 0x7777u;
  auto it = nodeMemo.find(k);
  if (it != nodeMemo.end()) { out = it->second.second; return it->second.first; }
  if (nodeMemo.size() > 200000) nodeMemo.clear();
  Node n;
  bool ok = false;
  Cand c = settleCand(t, cx, cy);
  if (c.ok) {
    ok = true;
    const int32_t capR = t == SiteType::Town ? 330 : 250;
    // capitals (kingdom cells near) and the start plan's settlements
    for (int32_t ky = floorDiv(c.y - capR + 512, KCELL); ky <= floorDiv(c.y + capR + 512, KCELL) && ok; ky++)
      for (int32_t kx = floorDiv(c.x - capR + 512, KCELL); kx <= floorDiv(c.x + capR + 512, KCELL) && ok; kx++) {
        KCell kc = kcell(kx, ky);
        if (kc.capital && dist2(kc.x, kc.y, c.x, c.y) < (int64_t)capR * capR) ok = false;
      }
    for (const Node& f : forced) {
      int32_t r = f.type == SiteType::City ? capR : t == SiteType::Town ? 240 : 200;
      if (dist2(f.x, f.y, c.x, c.y) < (int64_t)r * r) ok = false;
    }
    for (const SitePlan& f : forcedSites)
      if (dist2(f.ex, f.ey, c.x, c.y) < 70ll * 70ll) ok = false;
    if (ok && t == SiteType::Village) {
      // villages keep clear of the raw town candidates
      for (int32_t ty = floorDiv(c.y - 200, TOWN_CELL); ty <= floorDiv(c.y + 200, TOWN_CELL) && ok; ty++)
        for (int32_t tx = floorDiv(c.x - 200, TOWN_CELL); tx <= floorDiv(c.x + 200, TOWN_CELL) && ok; tx++) {
          Cand o = settleCand(SiteType::Town, tx, ty);
          if (o.ok && dist2(o.x, o.y, c.x, c.y) < 200ll * 200ll) ok = false;
        }
    }
    if (ok) {
      n.type = t;
      n.x = c.x; n.y = c.y;
      n.seed = c.seed;
      n.id = makeId(regionOf(c.x), regionOf(c.y), IdKind::Site, t == SiteType::Town ? LOCAL_TOWN : LOCAL_VILLAGE);
    }
  }
  nodeMemo.emplace(k, std::make_pair(ok, n));
  out = n;
  return ok;
}

void EndlessSource::Impl::nodesIn(int32_t x0, int32_t y0, int32_t x1, int32_t y1, std::vector<Node>& out) {
  auto inside = [&](int32_t x, int32_t y) { return x >= x0 && y >= y0 && x < x1 && y < y1; };
  for (int32_t ky = floorDiv(y0 + 512, KCELL); ky <= floorDiv(y1 + 512, KCELL); ky++)
    for (int32_t kx = floorDiv(x0 + 512, KCELL); kx <= floorDiv(x1 + 512, KCELL); kx++) {
      KCell kc = kcell(kx, ky);
      if (!kc.capital || !inside(kc.x, kc.y)) continue;
      Node n;
      n.type = SiteType::City;
      n.x = kc.x; n.y = kc.y;
      n.seed = kc.seed;
      n.flags = SPF_CAPITAL;
      n.id = kc.id;
      for (const Node& f : forced) if (f.id == kc.id) n.flags = f.flags;
      out.push_back(n);
    }
  for (const Node& f : forced)
    if (f.type != SiteType::City && inside(f.x, f.y)) out.push_back(f);
  for (int32_t cy = floorDiv(y0, TOWN_CELL); cy <= floorDiv(y1, TOWN_CELL); cy++)
    for (int32_t cx = floorDiv(x0, TOWN_CELL); cx <= floorDiv(x1, TOWN_CELL); cx++) {
      Node n;
      if (settleNode(SiteType::Town, cx, cy, n) && inside(n.x, n.y)) out.push_back(n);
    }
  for (int32_t cy = floorDiv(y0, VIL_CELL); cy <= floorDiv(y1, VIL_CELL); cy++)
    for (int32_t cx = floorDiv(x0, VIL_CELL); cx <= floorDiv(x1, VIL_CELL); cx++) {
      Node n;
      if (settleNode(SiteType::Village, cx, cy, n) && inside(n.x, n.y)) out.push_back(n);
    }
}

// ============================================================== kingdoms
EndlessSource::Impl::KCell EndlessSource::Impl::kcell(int32_t kx, int32_t ky) {
  uint64_t k = key2(kx, ky);
  auto it = kcells.find(k);
  if (it != kcells.end()) return it->second;
  if (kcells.size() > 50000) kcells.clear();
  KCell kc;
  kc.x = kx * KCELL; kc.y = ky * KCELL;   // the cell's centre (cells are offset by half a cell: the origin is a centre)
  bool done = false;
  for (const Node& f : forced)
    if (f.type == SiteType::City && floorDiv(f.x + 512, KCELL) == kx && floorDiv(f.y + 512, KCELL) == ky) {
      kc.capital = true; kc.x = f.x; kc.y = f.y; kc.seed = f.seed; kc.id = f.id;
      done = true;
    }
  if (!done) {
    uint64_t h = cellSeed(seed, tag("k.cell"), kx, ky);
    if (hq(h) >= Q(0.18)) {   // 18 % wildlands
      int best = 0;
      for (int n = 0; n < 10; n++) {
        uint64_t hn = mix64(h ^ (0x100u + (uint64_t)n));
        int32_t x = kx * KCELL + (int32_t)((hn & 0xFFFFFFFFull) % 425) - 212;
        int32_t y = ky * KCELL + (int32_t)((hn >> 32) % 425) - 212;
        bool clear = true;
        for (const Node& f : forced)
          if (dist2(f.x, f.y, x, y) < (f.type == SiteType::City ? 620ll * 620ll : 330ll * 330ll)) clear = false;
        for (const SitePlan& f : forcedSites)
          if (dist2(f.ex, f.ey, x, y) < 150ll * 150ll) clear = false;
        if (!clear) continue;
        int hab = habitability(SiteType::City, x, y, nullptr);
        if (!hab) continue;
        int score = hab * 256 + (int)(hn >> 56);
        if (score > best) { best = score; kc.capital = true; kc.x = x; kc.y = y; kc.seed = (uint32_t)(hn >> 16); }
      }
      if (kc.capital) kc.id = makeId(regionOf(kc.x), regionOf(kc.y), IdKind::Site, LOCAL_CITY);
    }
  }
  kcells.emplace(k, kc);
  return kc;
}

Gid EndlessSource::Impl::kingdomAt(int32_t x, int32_t y) {
  // weighted Voronoi of the seats (each realm has a reach of 0.8..1.2), on warped coordinates: borders curve and
  // wander like real ones instead of tracing the lattice
  int32_t wx = x, wy = y;
  warpQ(wx, wy, 10, 180, mix64(seed ^ tag("k.warp")));
  warpQ(wx, wy, 7, 40, mix64(seed ^ tag("k.warp2")));
  const int32_t kx0 = floorDiv(wx + 512, KCELL), ky0 = floorDiv(wy + 512, KCELL);
  int64_t bd = INT64_MAX;
  bool cap = false;
  int32_t bkx = 0, bky = 0;
  for (int32_t ky = ky0 - 1; ky <= ky0 + 1; ky++)
    for (int32_t kx = kx0 - 1; kx <= kx0 + 1; kx++) {
      KCell kc = kcell(kx, ky);
      int32_t reach = 205 + (int32_t)(cellSeed(seed, tag("k.reach"), kx, ky) % 102);   // /256: 0.8 .. 1.2
      int64_t d = dist2(wx, wy, kc.x, kc.y) * 65536 / ((int64_t)reach * reach);
      if (d < bd) { bd = d; cap = kc.capital; bkx = kx; bky = ky; }
    }
  if (!cap || bd > 950ll * 950ll) return 0;
  return makeId(bkx, bky, IdKind::Kingdom, 0);
}

const KingdomPlan* EndlessSource::Impl::kingdom(Gid id) {
  if (!id || idKind(id) != IdKind::Kingdom) return nullptr;
  auto it = kingdoms.find(id);
  if (it != kingdoms.end()) return &it->second;
  const int32_t kx = idRx(id), ky = idRy(id);
  KCell kc = kcell(kx, ky);
  if (!kc.capital) return nullptr;
  KingdomPlan k;
  k.id = id;
  Rng r(cellSeed(seed, tag("kingdom"), kx, ky));
  k.name = makeTownName(r);
  static const uint32_t fields[] = {rgba(150, 32, 36), rgba(36, 64, 140), rgba(28, 100, 60), rgba(110, 40, 120),
                                    rgba(180, 120, 30), rgba(30, 110, 120), rgba(60, 60, 64), rgba(170, 70, 30)};
  static const uint32_t trims[] = {rgba(232, 214, 160), rgba(240, 240, 232), rgba(220, 180, 60), rgba(30, 28, 32)};
  k.color = fields[r.irange(8)];
  k.color2 = trims[r.irange(4)];
  k.emblem = (uint8_t)r.irange(8);
  k.capital = kc.id;
  k.gx = kc.x; k.gy = kc.y;
  return &kingdoms.emplace(id, k).first->second;
}

// ============================================================== the start plan (VISION_PLAN 2.6, adjusted for 15.8)
void EndlessSource::Impl::makeStart() {
  if (started || starting) return;
  starting = true;
  // a spot is on the start's landmass if the straight line to it stays on land (coarse samples every 24 tiles)
  auto lineLand = [&](int32_t ax, int32_t ay, int32_t bx, int32_t by) {
    int32_t L = idist(ax, ay, bx, by);
    for (int32_t s = 0; s <= L; s += 24) {
      int32_t x = ax + (int32_t)((int64_t)(bx - ax) * s / std::max(1, L)), y = ay + (int32_t)((int64_t)(by - ay) * s / std::max(1, L));
      if (coarse(x, y).e < ELEV_SEA + Q(0.005)) return false;
    }
    return true;
  };
  auto seaFar = [&](int32_t x, int32_t y, int32_t r) {
    for (int k = 0; k < 16; k++) {
      int32_t a = k * (1024 / 16);
      for (int32_t rr : {r / 2, r}) {
        int32_t px = x + icosR(a, rr), py = y + isinR(a, rr);
        if (coarse(px, py).e < ELEV_SEA + Q(0.005)) return false;
      }
    }
    return true;
  };
  // 1. the start village: the raw village candidate nearest the origin in plains or forest, 80+ tiles from the sea
  Node sv;
  bool have = false;
  int64_t bd = INT64_MAX;
  // a temperate home: plains or woods well inside the temperate band (no taiga or desert edge), clear of the marsh
  auto temperate = [&](int32_t x, int32_t y) {
    Coarse cc = coarse(x, y);
    Biome b = classify(cc.e, cc.t, cc.m, x, y, false);
    if (b != Biome::Plains && b != Biome::Forest && b != Biome::Autumn) return false;
    if (cc.t < Q(0.37) || cc.t > Q(0.66)) return false;
    return !(cc.m > Q(0.60) && cc.e < Q(0.48));
  };
  // the village candidates of the cells around the origin, then (rarely needed) of a wider ring of cells
  for (int32_t ring = 0; ring < 2 && !have; ring++)
    for (int32_t cy = -2 - 2 * ring; cy <= 1 + 2 * ring; cy++)
      for (int32_t cx = -2 - 2 * ring; cx <= 1 + 2 * ring; cx++) {
        if (ring && cx >= -2 && cx <= 1 && cy >= -2 && cy <= 1) continue;
        Cand c = settleCand(SiteType::Village, cx, cy);
        if (!c.ok || !temperate(c.x, c.y) || !seaFar(c.x, c.y, 90)) continue;
        int64_t d = dist2(c.x, c.y, 0, 0);
        if (d < bd) { bd = d; have = true; sv.x = c.x; sv.y = c.y; sv.seed = c.seed; }
      }
  // no candidate: any habitable temperate spot out from the origin (the climate test too: the hero never starts in
  // the frozen north or the desert), and only then any habitable one
  for (int pass = 0; pass < 2 && !have; pass++)
    for (int32_t r = 0; r < 3000 && !have; r += 24)
      for (int k = 0; k < 8 && !have; k++) {
        int32_t x = (k % 3 - 1) * r, y = (k / 3 - 1) * r;
        if (k == 4 && r) continue;
        if ((pass == 1 || temperate(x, y)) && habitability(SiteType::Village, x, y, nullptr) && seaFar(x, y, 90)) {
          have = true; sv.x = x; sv.y = y; sv.seed = (uint32_t)(mix64(seed ^ 0x5717ull) >> 16);
        }
      }
  sv.type = SiteType::Village;
  sv.flags = SPF_START;
  sv.id = makeId(regionOf(sv.x), regionOf(sv.y), IdKind::Site, LOCAL_FORCED + 0);
  forced.push_back(sv);
  const int32_t hx = sv.x, hy = sv.y;
  // 2. the story city (the start kingdom's capital, with the keep): 160-280 tiles away on the same land
  {
    Node sc;
    bool found = false;
    const int32_t a0 = (int32_t)(hq(mix64(seed ^ tag("start.cap"))) & 1023);
    // (within 200 tiles first: the story city is one of the start's six kinds of place within reach)
    for (int32_t dist : {190, 170, 160, 180, 200, 215, 240, 265, 280}) {
      int bestHab = 0;
      for (int k = 0; k < 32; k++) {
        int32_t a = a0 + k * 1024 / 32;
        int32_t x = hx + icosR(a, dist), y = hy + isinR(a, dist);
        int hab = habitability(SiteType::City, x, y, nullptr);
        if (hab <= bestHab || !lineLand(hx, hy, x, y)) continue;
        bestHab = hab; found = true; sc.x = x; sc.y = y;
      }
      if (found) break;
    }
    if (!found) {   // nothing habitable: take the flattest dry spot at about 200 tiles
      for (int k = 0; k < 64 && !found; k++) {
        int32_t a = a0 + k * 1024 / 64;
        int32_t x = hx + icosR(a, 200), y = hy + isinR(a, 200);
        if (coarse(x, y).e >= ELEV_SEA + Q(0.02) && lineLand(hx, hy, x, y)) { found = true; sc.x = x; sc.y = y; }
      }
      if (!found) { sc.x = hx + 200; sc.y = hy; }
    }
    sc.type = SiteType::City;
    sc.flags = SPF_CAPITAL | SPF_STORY;
    sc.seed = (uint32_t)(mix64(seed ^ tag("start.capital.seed")) >> 16);
    sc.id = makeId(regionOf(sc.x), regionOf(sc.y), IdKind::Site, LOCAL_FORCED + 1);
    forced.push_back(sc);
  }
  const Node& city = forced[1];
  // open ground for a small site: land, no rock, one relief level, no lake or river
  auto open = [&](int32_t x, int32_t y, int32_t r) {
    Coarse c = coarse(x, y);
    if (c.e < ELEV_SEA + Q(0.015) || c.rock > Q(0.2)) return false;
    int lv = levelOf(c.e);
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      Coarse c2 = coarse(x + dx[k] * r, y + dy[k] * r);
      if (c2.e < ELEV_SEA + Q(0.015) || c2.rock > Q(0.2) || levelOf(c2.e) != lv) return false;
    }
    return !lakeNear(x, y, r + 4) && !riverWidthCell(x, y);
  };
  auto nearSettleCand = [&](int32_t x, int32_t y, int32_t r) {
    for (SiteType t : {SiteType::Town, SiteType::Village}) {
      int32_t S = t == SiteType::Town ? TOWN_CELL : VIL_CELL;
      for (int32_t cy = floorDiv(y - r, S); cy <= floorDiv(y + r, S); cy++)
        for (int32_t cx = floorDiv(x - r, S); cx <= floorDiv(x + r, S); cx++) {
          Cand c = settleCand(t, cx, cy);
          if (c.ok && dist2(c.x, c.y, x, y) < (int64_t)r * r) return true;
        }
    }
    return false;
  };
  // 3. three shard ruins at rising distance (so rising danger), 4. the lair on a ridge 480-620 tiles out
  const int32_t shardD[3] = {186, 290, 400};
  for (int k = 0; k < 3; k++) {
    SitePlan p;
    bool found = false;
    const int32_t a0 = (int32_t)(hq(mix64(seed ^ (tag("start.shard") + (uint64_t)k))) & 1023);
    for (int32_t dd : {0, 8, -4, 20, 40, -20}) {
      for (int n = 0; n < 32 && !found; n++) {
        int32_t a = a0 + n * 1024 / 32;
        int32_t dist = shardD[k] + dd;
        int32_t x = hx + icosR(a, dist), y = hy + isinR(a, dist);
        if (dist2(x, y, city.x, city.y) < 150ll * 150ll) continue;
        bool clear = true;
        for (const SitePlan& f : forcedSites) if (dist2(f.ex, f.ey, x, y) < 130ll * 130ll) clear = false;
        if (!clear || !open(x, y, 8) || nearSettleCand(x, y, 75) || !lineLand(hx, hy, x, y)) continue;
        found = true; p.ex = x; p.ey = y;
      }
      if (found) break;
    }
    if (!found) { p.ex = hx + shardD[k]; p.ey = hy + 40 * k; }
    p.type = SiteType::Ruin;
    p.w = 9; p.h = 7;
    p.gx = p.ex - 4; p.gy = p.ey - 2;
    p.flags = SPF_MAINQUEST;
    p.seed = (uint32_t)(mix64(seed ^ (tag("start.shard.seed") + (uint64_t)k)) >> 16);
    p.id = makeId(regionOf(p.ex), regionOf(p.ey), IdKind::Site, LOCAL_FORCED + 2 + (uint32_t)k);
    forcedSites.push_back(p);
  }
  {
    SitePlan p;
    int32_t bestE = INT32_MIN;
    const int32_t a0 = (int32_t)(hq(mix64(seed ^ tag("start.lair"))) & 1023);
    for (int32_t dist : {480, 520, 560, 600}) {
      for (int n = 0; n < 48; n++) {
        int32_t a = a0 + n * 1024 / 48;
        int32_t x = hx + icosR(a, dist), y = hy + isinR(a, dist);
        Coarse c = coarse(x, y);
        if (c.e < ELEV_SEA + Q(0.02) || c.rock > Q(0.15)) continue;
        int32_t score = c.e + qm(c.ridge, Q(0.3));
        if (score <= bestE) continue;
        if (!open(x, y, 9) || !lineLand(hx, hy, x, y)) continue;
        bool clear = dist2(x, y, city.x, city.y) > 260ll * 260ll;
        for (const SitePlan& f : forcedSites) if (dist2(f.ex, f.ey, x, y) < 150ll * 150ll) clear = false;
        if (!clear) continue;
        bestE = score; p.ex = x; p.ey = y;
      }
    }
    if (bestE == INT32_MIN) { p.ex = hx - 520; p.ey = hy; }
    p.type = SiteType::DragonLair;
    p.w = 17; p.h = 15;
    p.gx = p.ex - 8; p.gy = p.ey - 7;
    p.seed = (uint32_t)(mix64(seed ^ tag("start.lair.seed")) >> 16);
    p.id = makeId(regionOf(p.ex), regionOf(p.ey), IdKind::Site, LOCAL_FORCED + 5);
    forcedSites.push_back(p);
  }
  // 5. the first hour: a bandit camp, a wayside shrine, a cave in a real cliff and a wolf den within easy reach, so
  //    every kind of place is close to home whatever the lattices drew
  auto clearOf = [&](int32_t x, int32_t y, int32_t r) {
    if (dist2(x, y, hx, hy) < 80ll * 80ll || dist2(x, y, city.x, city.y) < 150ll * 150ll) return false;
    for (const SitePlan& f : forcedSites) if (dist2(f.ex, f.ey, x, y) < (int64_t)r * r) return false;
    return !nearSettleCand(x, y, 60);
  };
  auto ring = [&](uint64_t tg, int32_t d0, int32_t d1, int32_t r, int32_t& ox, int32_t& oy) {
    const int32_t a0 = (int32_t)(hq(mix64(seed ^ tg)) & 1023);
    for (int32_t dist = d0; dist <= d1; dist += 15)
      for (int n = 0; n < 24; n++) {
        int32_t a = a0 + n * 1024 / 24;
        int32_t x = hx + icosR(a, dist), y = hy + isinR(a, dist);
        if (!clearOf(x, y, 50) || !open(x, y, r) || !lineLand(hx, hy, x, y)) continue;
        ox = x; oy = y;
        return true;
      }
    return false;
  };
  {
    int32_t x, y;
    if (ring(tag("start.camp"), 110, 180, 8, x, y)) {
      SitePlan p;
      p.type = SiteType::BanditCamp; p.w = 11; p.h = 9; p.ex = x; p.ey = y; p.gx = x - 5; p.gy = y - 4;
      p.seed = (uint32_t)(mix64(seed ^ tag("start.camp.seed")) >> 16);
      p.id = makeId(regionOf(x), regionOf(y), IdKind::Site, LOCAL_FORCED + 6);
      forcedSites.push_back(p);
    }
    if (ring(tag("start.shrine"), 70, 160, 5, x, y)) {
      SitePlan p;
      p.type = SiteType::Shrine; p.w = 5; p.h = 5; p.ex = x; p.ey = y; p.gx = x - 2; p.gy = y - 2;
      p.seed = (uint32_t)(mix64(seed ^ tag("start.shrine.seed")) >> 16);
      p.id = makeId(regionOf(x), regionOf(y), IdKind::Site, LOCAL_FORCED + 7);
      forcedSites.push_back(p);
    }
    // a cave: the first real south-facing cliff found walking out in rings
    bool cave = false;
    const int32_t a0 = (int32_t)(hq(mix64(seed ^ tag("start.cave"))) & 1023);
    for (int32_t dist = 60; dist <= 190 && !cave; dist += 26)
      for (int n = 0; n < 20 && !cave; n++) {
        int32_t a = a0 + n * 1024 / 20;
        int32_t px = hx + icosR(a, dist), py = hy + isinR(a, dist);
        int32_t cxp, cyp;
        uint32_t sd = (uint32_t)(mix64(seed ^ tag("start.cave.seed")) >> 16);
        if (!caveSpot(px, py, sd, cxp, cyp) || !clearOf(cxp, cyp, 40) || dist2(cxp, cyp, hx, hy) > 198ll * 198ll) continue;
        SitePlan p;
        p.type = SiteType::Cave; p.w = 5; p.h = 4; p.ex = cxp; p.ey = cyp; p.gx = cxp - 2; p.gy = cyp - 1;
        p.seed = sd;
        p.id = makeId(regionOf(cxp), regionOf(cyp), IdKind::Site, LOCAL_FORCED + 8);
        forcedSites.push_back(p);
        cave = true;
      }
    if (!cave && ring(tag("start.cave2"), 90, 180, 6, x, y)) {
      // no cliff near home: the cave opens in a rocky knoll instead (stampSite raises it)
      SitePlan p;
      p.type = SiteType::Cave; p.w = 5; p.h = 4; p.ex = x; p.ey = y; p.gx = x - 2; p.gy = y - 1;
      p.seed = (uint32_t)(mix64(seed ^ tag("start.cave.seed")) >> 16);
      p.id = makeId(regionOf(x), regionOf(y), IdKind::Site, LOCAL_FORCED + 8);
      forcedSites.push_back(p);
    }
    if (ring(tag("start.den"), 80, 160, 4, x, y)) {
      DenPlan d;
      d.id = makeId(regionOf(x), regionOf(y), IdKind::Den, 0x3F0);
      d.x = x; d.y = y;
      d.mon = Monster::Wolf;
      d.pack = 3;
      forcedDens.push_back(d);
    }
  }
  sp.village = forced[0].id;
  sp.capital = forced[1].id;
  for (int k = 0; k < 3; k++) sp.shards[k] = forcedSites[(size_t)k].id;
  sp.lair = forcedSites[3].id;
  sp.spawn = GTile{hx, hy + 1};
  // the memos above saw no forced sites; drop what depends on them
  nodeMemo.clear();
  kcells.clear();
  started = true;
  starting = false;
}

// ============================================================== danger (VISION_PLAN 7.1)
int EndlessSource::Impl::danger(int32_t x, int32_t y) {
  const Node& home = forced[0];
  int d = dangerBase(idist(x, y, home.x, home.y));
  if (!kingdomAt(x, y)) d += 3;
  // dread zone: within 200 tiles of a dragon lair
  bool dread = false;
  for (const SitePlan& f : forcedSites)
    if (f.type == SiteType::DragonLair && dist2(f.ex, f.ey, x, y) < 200ll * 200ll) dread = true;
  for (int32_t ly = floorDiv(y - 200 + 512, LAIR_CELL); ly <= floorDiv(y + 200 + 512, LAIR_CELL) && !dread; ly++)
    for (int32_t lx = floorDiv(x - 200 + 512, LAIR_CELL); lx <= floorDiv(x + 200 + 512, LAIR_CELL) && !dread; lx++) {
      int32_t ax, ay; uint32_t s;
      if (lairOf(lx, ly, ax, ay, s) && dist2(ax, ay, x, y) < 200ll * 200ll) dread = true;
    }
  if (dread) d += 6;
  // heartland: close to a capital
  bool heart = false;
  for (int32_t ky = floorDiv(y - 150 + 512, KCELL); ky <= floorDiv(y + 150 + 512, KCELL) && !heart; ky++)
    for (int32_t kx = floorDiv(x - 150 + 512, KCELL); kx <= floorDiv(x + 150 + 512, KCELL) && !heart; kx++) {
      KCell kc = kcell(kx, ky);
      if (kc.capital && dist2(kc.x, kc.y, x, y) < 150ll * 150ll) heart = true;
    }
  if (heart) d -= 2;
  return std::clamp(d, 1, 60);
}

// ============================================================== names (unique within about 3x3 regions)
std::string EndlessSource::Impl::siteName(const Node& n) {
  auto base = [](const Node& m) { Rng r(m.seed ^ 0xA5A5u); return makeTownName(r); };
  std::string nm = base(n);
  std::vector<Node> near;
  nodesIn(n.x - 800, n.y - 800, n.x + 800, n.y + 800, near);
  bool clash = false;
  for (const Node& o : near)
    if (o.id != n.id && o.id < n.id && base(o) == nm) clash = true;
  if (!clash) return nm;
  uint64_t h = mix64(n.id ^ 0x4E414D45ull);
  for (int k = 0; k < 10; k++) {
    std::string cand = std::string(kPrefix[(h + (uint64_t)k) % 10]) + nm;
    bool used = false;
    for (const Node& o : near) if (o.id != n.id && base(o) == cand) used = true;
    if (!used) return cand;
  }
  return nm;
}

// ============================================================== POIs
// a cave mouth on a real south-facing cliff (relief levels): the tile below a face at least three tiles wide, with a
// flat, open approach. Searched around the lattice point in a fixed order; false if the land there has no such face.
bool EndlessSource::Impl::caveSpot(int32_t px, int32_t py, uint32_t sd, int32_t& ox, int32_t& oy) {
  int bestScore = -1;
  for (int32_t r = 0; r <= 24; r += 2)
    for (int32_t dy = -r; dy <= r; dy += 2)
      for (int32_t dx = -r; dx <= r; dx += 2) {
        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
        const int32_t x = px + dx, y = py + dy;
        int L = natLevel(x, y);
        int up = natLevel(x, y - 1);
        if (up <= L) continue;
        if (natLevel(x - 1, y - 1) <= L || natLevel(x + 1, y - 1) <= L) continue;
        if (natLevel(x - 1, y) != L || natLevel(x + 1, y) != L) continue;
        bool ok = true;
        for (int32_t j = 1; j <= 3 && ok; j++)
          for (int32_t i = -1; i <= 1 && ok; i++) if (natLevel(x + i, y + j) != L) ok = false;
        if (!ok) continue;
        gen::TileF f = tile(x, y + 2);
        if (f.sea || f.rock || f.biome == Biome::Swamp) continue;
        if (tile(x, y).sea || riverWidthCell(x, y + 2) || riverWidthCell(x, y)) continue;
        int score = (natLevel(x, y - 3) - L) * 4 + (up - L) * 2 + (int)((tileHash(sd, x, y) >> 60) & 3);
        if (score > bestScore) { bestScore = score; ox = x; oy = y; }
      }
  return bestScore >= 0;
}

EndlessSource::Impl::Poi EndlessSource::Impl::poiRaw(SiteType t, int32_t cx, int32_t cy) {
  uint64_t k = typeKey(t, cx, cy) ^ 0x9011u;
  auto it = poiMemo.find(k);
  if (it != poiMemo.end()) return it->second;
  if (poiMemo.size() > 200000) poiMemo.clear();
  Poi p;
  p.type = t;
  uint64_t tg = t == SiteType::Cave ? tag("l.cave") : t == SiteType::Ruin ? tag("l.ruin") : t == SiteType::BanditCamp ? tag("l.camp") : tag("l.shrine");
  int32_t accept = t == SiteType::Cave ? Q(0.92) : t == SiteType::Ruin ? Q(0.45) : t == SiteType::BanditCamp ? Q(0.55) : Q(0.40);
  uint64_t h = cellSeed(seed, tg, cx, cy);
  p.seed = (uint32_t)(mix64(h ^ 0x51u) >> 16);
  // the first hour (VISION_PLAN 2.6): every kind of place shows up near the start village
  if (!forced.empty() && dist2(cx * POI_CELL + POI_CELL / 2, cy * POI_CELL + POI_CELL / 2, forced[0].x, forced[0].y) < 240ll * 240ll)
    accept = Q(0.97);
  if (hq(h) < accept) {
    int32_t x = cx * POI_CELL + POI_M + (int32_t)((mix64(h ^ 1) >> 33) % (POI_CELL - 2 * POI_M));
    int32_t y = cy * POI_CELL + POI_M + (int32_t)((mix64(h ^ 2) >> 33) % (POI_CELL - 2 * POI_M));
    if (t == SiteType::Cave) {
      int32_t ox, oy;
      if (caveSpot(x, y, p.seed, ox, oy)) { p.ok = true; p.x = ox; p.y = oy; }
    } else {
      Coarse c = coarse(x, y);
      int32_t r = t == SiteType::BanditCamp ? 8 : t == SiteType::Ruin ? 7 : 5;
      bool ok = c.e >= ELEV_SEA + Q(0.015) && c.rock < Q(0.2);
      if (ok) {
        int lv = levelOf(c.e);
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int n = 0; n < 4 && ok; n++) {
          Coarse c2 = coarse(x + dx[n] * r, y + dy[n] * r);
          if (c2.e < ELEV_SEA + Q(0.015) || c2.rock > Q(0.2) || levelOf(c2.e) != lv) ok = false;
        }
      }
      if (ok && (lakeNear(x, y, r + 4) || riverWidthCell(x, y))) ok = false;
      if (ok) { p.ok = true; p.x = x; p.y = y; }
    }
  }
  poiMemo.emplace(k, p);
  return p;
}

bool EndlessSource::Impl::lairOf(int32_t lx, int32_t ly, int32_t& ox, int32_t& oy, uint32_t& sd) {
  uint64_t k = key2(lx, ly);
  auto it = lairMemo.find(k);
  if (it == lairMemo.end()) {
    if (lairMemo.size() > 20000) lairMemo.clear();
    Poi p;
    bool ok = false;
    // the start lair stands for its own lair cell
    bool startCell = false;
    for (const SitePlan& f : forcedSites)
      if (f.type == SiteType::DragonLair && floorDiv(f.ex + 512, LAIR_CELL) == lx && floorDiv(f.ey + 512, LAIR_CELL) == ly) startCell = true;
    if (!startCell) {
      // the highest ridge point of the cell's interior (coarse grid, then refined)
      const int32_t bx = lx * LAIR_CELL - 512, by = ly * LAIR_CELL - 512;
      int32_t best = INT32_MIN, bxp = 0, byp = 0;
      for (int j = 1; j < 12; j++)
        for (int i = 1; i < 12; i++) {
          int32_t x = bx + i * (LAIR_CELL / 12), y = by + j * (LAIR_CELL / 12);
          Coarse c = coarse(x, y);
          if (c.e < ELEV_SEA + Q(0.03) || c.ridge < Q(0.30) || c.rock > Q(0.2)) continue;
          int32_t s = c.e + qm(c.ridge, Q(0.2));
          if (s > best) { best = s; bxp = x; byp = y; }
        }
      if (best != INT32_MIN) {
        int32_t fx = bxp, fy = byp;
        best = INT32_MIN;
        for (int32_t dy = -80; dy <= 80; dy += 16)
          for (int32_t dx = -80; dx <= 80; dx += 16) {
            int32_t x = bxp + dx, y = byp + dy;
            Coarse c = coarse(x, y);
            if (c.e < ELEV_SEA + Q(0.03) || c.rock > Q(0.15)) continue;
            bool flat = true;
            int lv = levelOf(c.e);
            static const int ddx[4] = {1, -1, 0, 0}, ddy[4] = {0, 0, 1, -1};
            for (int n = 0; n < 4 && flat; n++) {
              Coarse c2 = coarse(x + ddx[n] * 9, y + ddy[n] * 9);
              if (c2.rock > Q(0.15) || std::abs(levelOf(c2.e) - lv) > 1) flat = false;
            }
            if (!flat) continue;
            int32_t s = c.e + qm(c.ridge, Q(0.2));
            if (s > best) { best = s; fx = x; fy = y; }
          }
        if (best != INT32_MIN) { ok = true; p.x = fx; p.y = fy; p.seed = (uint32_t)(cellSeed(seed, tag("lair"), lx, ly) >> 16); }
      }
    }
    it = lairMemo.emplace(k, std::make_pair(ok, p)).first;
  }
  if (!it->second.first) return false;
  ox = it->second.second.x; oy = it->second.second.y; sd = it->second.second.seed;
  return true;
}

// ============================================================== region plans
std::shared_ptr<const RegionData> EndlessSource::Impl::regionData(int32_t rx, int32_t ry) {
  uint64_t k = key2(rx, ry);
  if (auto r = regions.get(k)) return r;
  Nest nest(*this);
  auto t0 = Clock::now();
  auto D = std::make_shared<RegionData>();
  buildRegion(rx, ry, *D);
  double ms = msSince(t0);
  stats.regions++;
  stats.regionMs += ms;
  stats.maxRegionMs = std::max(stats.maxRegionMs, ms);
  regions.put(k, D);
  return D;
}

void EndlessSource::Impl::buildRegion(int32_t rx, int32_t ry, RegionData& D) {
  RegionPlan& R = D.plan;
  R.rx = rx; R.ry = ry;
  const int32_t x0 = rx * REGION, y0 = ry * REGION, x1 = x0 + REGION, y1 = y0 + REGION;
  // roads near the region (spurs look a little further)
  std::vector<std::shared_ptr<const Edge>> near;
  graphEdges(x0 - 100, y0 - 100, x1 + 100, y1 + 100, near);
  for (auto& e : near)
    if (e->x1 >= x0 - 32 && e->x0 < x1 + 32 && e->y1 >= y0 - 32 && e->y0 < y1 + 32) D.edges.push_back(e);
  auto nearestRoad = [&](int32_t x, int32_t y, int32_t maxD, GTile& at) {
    int64_t bd = (int64_t)maxD * maxD + 1;
    bool any = false;
    for (auto& e : near)
      for (const GTile& g : e->pts) {
        int64_t d = dist2(g.x, g.y, x, y);
        if (d < bd) { bd = d; at = g; any = true; }
      }
    return any;
  };
  std::vector<Node> nodes;
  nodesIn(x0 - 170, y0 - 170, x1 + 170, y1 + 170, nodes);
  uint32_t bldgNext = 0;
  auto add = [&](SitePlan p, int flat) {
    p.bldgBase = (uint16_t)std::min<uint32_t>(bldgNext, 4095);
    bldgNext += p.bldgCap;
    p.level = danger(p.ex, p.ey);
    p.kingdom = kingdomAt(p.ex, p.ey);
    R.sites.push_back(p);
    D.flatLevel.push_back({flat, 0});
  };
  // ---- settlements (in id order: city, town, village, then the start plan's)
  std::vector<Node> mine;
  for (const Node& n : nodes) if (n.x >= x0 && n.y >= y0 && n.x < x1 && n.y < y1) mine.push_back(n);
  std::sort(mine.begin(), mine.end(), [](const Node& a, const Node& b) { return a.id < b.id; });
  for (const Node& n : mine) {
    SitePlan p;
    p.id = n.id;
    p.type = n.type;
    p.ex = n.x; p.ey = n.y;
    p.seed = n.seed;
    p.flags = n.flags;
    // archetype from the context (VISION_PLAN 2.5)
    const int32_t R0 = nominalR(n.type);
    bool coast = false, river = false, ridge = false, hill = false;
    for (int k = 0; k < 12 && !coast; k++) {
      int32_t a = k * (1024 / 12);
      int32_t px = n.x + icosR(a, R0 + 18), py = n.y + isinR(a, R0 + 18);
      if (tile(px, py).sea) coast = true;
    }
    // (a lake under the footprint is drained when the town is built (hydro.cpp lakeUnderSettlement), so only a river
    // counts here; a lake never makes a fishing place it could not keep)
    for (int32_t dy = -R0 / 2; dy <= R0 / 2 && !river; dy += 8)
      for (int32_t dx = -R0 / 2; dx <= R0 / 2 && !river; dx += 8)
        if (riverWidthCell(n.x + dx, n.y + dy) & 7) river = true;
    int Lc = natLevel(n.x, n.y), lower = 0, higher = 0;
    int32_t ridgeMax = 0;
    for (int k = 0; k < 8; k++) {
      int32_t a = k * (1024 / 8);
      int32_t px = n.x + icosR(a, 70), py = n.y + isinR(a, 70);
      const int32_t rg = coarse(px, py).ridge;
      if (rg > Q(0.35)) ridge = true;
      ridgeMax = std::max(ridgeMax, rg);
      const int lv = natLevel(px, py);
      if (lv < Lc) lower++;
      if (lv > Lc) higher++;
    }
    hill = lower >= 7;
    int degree = 0;
    for (auto& e : D.edges) if (e->a == n.id || e->b == n.id) degree++;
    gen::TileF tf = tile(n.x, n.y);
    if (coast) { p.archetype = n.type == SiteType::Village ? Archetype::Fishing : Archetype::Port; if (n.type != SiteType::Village) p.flags |= SPF_PORT; }
    else if (river) p.archetype = Archetype::RiverCrossing;
    else if (ridge) p.archetype = Archetype::Mining;
    else if (hill) p.archetype = Archetype::HillFort;
    // a market town is a real crossroads of trade: six roads, or four and a town's luck (villages: rarely, at five);
    // (every third settlement has three roads, so three alone made most of the world a "market")
    else if (n.type != SiteType::Village ? (degree >= 6 || (degree >= 4 && ((n.seed >> 9) % 3u) == 0)) : (degree >= 5 && ((n.seed >> 9) % 5u) == 0))
      p.archetype = Archetype::Market;
    else if (tf.m > Q(0.52)) p.archetype = Archetype::Farming;
    else p.archetype = Archetype::Plain;
    // the specialisation (owner 2026-10-05, rpg/world/economy.h): what the land round it offers. The sea: fishing; a
    // ridge or a rocky hilltop: mining; deep woods on every side: lumber (a clearing at the forest's edge farms or
    // herds like anywhere else); a river: fishing or farming with a watermill; dry open grass, the north and the hills:
    // herding; good wet soil: farming
    {
      int forest = 0, open = 0;
      for (int k = 0; k < 12; k++) {
        int32_t a = k * (1024 / 12);
        const Biome b = tile(n.x + icosR(a, R0 + 12), n.y + isinR(a, R0 + 12)).biome;
        if (b == Biome::Forest || b == Biome::Taiga || b == Biome::Autumn) forest++;
        if (b == Biome::Plains || b == Biome::Snow || b == Biome::Desert) open++;
      }
      const Biome hb = tf.biome;
      const bool inWood = hb == Biome::Forest || hb == Biome::Taiga || hb == Biome::Autumn;
      const bool deepWoods = inWood && forest >= 10;
      const uint32_t coin = (n.seed >> 11) % 100u;
      Specialty sp;
      if (coast) sp = Specialty::Fishing;
      else if (ridge || (hill && (hb == Biome::Mountain || hb == Biome::Snow || coin < 45)) ||
               ((ridgeMax > Q(0.2) || higher >= 3) && coin < 40)) sp = Specialty::Mining;   // high ground over the houses
      else if (deepWoods && coin < 60) sp = Specialty::Lumber;
      else if (river) sp = (n.seed >> 7) & 1 ? Specialty::Fishing : Specialty::Farming;
      else if (hill || hb == Biome::Snow || hb == Biome::Desert || hb == Biome::Mountain || (tf.m < Q(0.40) && open >= 5)) sp = Specialty::Herding;
      else if (inWood && forest >= 7 && coin < 20) sp = Specialty::Lumber;   // a forest village that still lives by the axe
      else if ((tf.m < Q(0.45) && coin >= 55) || coin >= 85) sp = Specialty::Herding;   // drier grass: sheep and cattle
      else sp = Specialty::Farming;
      p.special = sp;
      p.produces = specialtyProduces(sp, n.type != SiteType::Village);
      p.needs = specialtyNeeds(sp, n.type != SiteType::Village);
    }
    settlementFootprint(n.type, p.archetype, n.seed, p.w, p.h);
    p.gx = n.x - p.w / 2; p.gy = n.y - p.h / 2;
    p.bldgCap = n.type == SiteType::City ? 640 : n.type == SiteType::Town ? 200 : 48;
    p.name = siteName(n);
    add(p, Lc);
    // road bearings: where each incident road leaves the footprint (strongest class first)
    std::vector<std::pair<int, float>> bs;
    for (auto& e : D.edges) {
      if (e->a != n.id && e->b != n.id) continue;
      const bool fromA = e->a == n.id;
      const int np = (int)e->pts.size();
      for (int q = 0; q < np; q++) {
        const GTile& g = e->pts[(size_t)(fromA ? q : np - 1 - q)];
        if (g.x >= p.gx - 2 && g.y >= p.gy - 2 && g.x < p.gx + p.w + 2 && g.y < p.gy + p.h + 2) continue;
        bs.push_back({e->cls, datan2((float)(g.y - n.y), (float)(g.x - n.x))});
        break;
      }
    }
    std::stable_sort(bs.begin(), bs.end(), [](const std::pair<int, float>& a, const std::pair<int, float>& b) { return a.first < b.first; });
    std::vector<float>& out = D.bearings[n.id];
    for (auto& b : bs) out.push_back(b.second);
  }
  // ---- the start plan's other sites in this region
  for (const SitePlan& f : forcedSites)
    if (f.ex >= x0 && f.ey >= y0 && f.ex < x1 && f.ey < y1) {
      SitePlan p = f;
      Rng nr(p.seed ^ 0xA5A5u);
      p.name = makeDungeonName(nr, p.type, tile(p.ex, p.ey + 2).biome);
      if (p.type == SiteType::Ruin) {
        static const Monster themes[] = {Monster::Draugr, Monster::Skeleton, Monster::Wraith};
        p.theme = themes[p.seed % 3];
      } else if (p.type == SiteType::DragonLair) p.theme = Monster::Dragon;
      else if (p.type == SiteType::Cave) {
        Biome b = tile(p.ex, p.ey + 2).biome;
        static const Monster themes[] = {Monster::Spider, Monster::Goblin, Monster::Bat};
        p.theme = b == Biome::Snow ? Monster::IceWolf : b == Biome::Desert ? Monster::Sandworm : themes[p.seed % 3];
      }
      SitePlan q = p;
      add(q, p.type == SiteType::Cave ? -1 : natLevel(p.ex, p.ey));
      // the three shard ruins rise in danger, whatever the land around them adds or takes
      if (p.flags & SPF_MAINQUEST) {
        int lv = 0;
        for (int k = 0; k < 3; k++) {
          lv = std::max(danger(forcedSites[(size_t)k].ex, forcedSites[(size_t)k].ey), lv + (k ? 1 : 0));
          if (forcedSites[(size_t)k].id == p.id) break;
        }
        R.sites.back().level = lv;
      }
    }
  // ---- wilderness POIs: caves, ruins, bandit camps, shrines (in that order of precedence)
  const SiteType poiOrder[4] = {SiteType::Cave, SiteType::Ruin, SiteType::BanditCamp, SiteType::Shrine};
  std::vector<std::pair<int32_t, int32_t>> placed;   // final POI hearts (dens keep clear)
  for (int oi = 0; oi < 4; oi++) {
    const SiteType t = poiOrder[oi];
    for (int32_t cy = floorDiv(y0, POI_CELL); cy <= floorDiv(y1 - 1, POI_CELL); cy++)
      for (int32_t cx = floorDiv(x0, POI_CELL); cx <= floorDiv(x1 - 1, POI_CELL); cx++) {
        Poi raw = poiRaw(t, cx, cy);
        if (!raw.ok) continue;
        int32_t x = raw.x, y = raw.y;
        // camps and shrines move beside a road when one passes near (bandits watch the roads; wayside shrines)
        if (t == SiteType::BanditCamp || t == SiteType::Shrine) {
          GTile at;
          if (nearestRoad(x, y, 70, at)) {
            int32_t dx = x - at.x, dy = y - at.y;
            int32_t L = std::max(1, idist(x, y, at.x, at.y));
            int32_t off = 13 + (int32_t)(raw.seed % 5);
            if (L < 2) { dx = (raw.seed & 1) ? 1 : -1; dy = 0; L = 1; }
            int32_t nx = at.x + dx * off / L, ny = at.y + dy * off / L;
            Coarse c = coarse(nx, ny);
            if (c.e >= ELEV_SEA + Q(0.015) && c.rock < Q(0.2) && !riverWidthCell(nx, ny)) { x = nx; y = ny; }
          } else if (t == SiteType::BanditCamp && (raw.seed & 3) == 0) continue;
        }
        if (x < x0 || y < y0 || x >= x1 || y >= y1) continue;
        bool ok = true;
        for (const Node& n : nodes)
          if (dist2(n.x, n.y, x, y) < (int64_t)(nominalR(n.type) + 45) * (nominalR(n.type) + 45)) { ok = false; break; }
        for (const SitePlan& f : forcedSites) if (ok && dist2(f.ex, f.ey, x, y) < 60ll * 60ll) ok = false;
        // lower classes keep clear of the raw candidates of higher ones
        for (int hj = 0; hj < oi && ok; hj++)
          for (int32_t qy = floorDiv(y - 40, POI_CELL); qy <= floorDiv(y + 40, POI_CELL) && ok; qy++)
            for (int32_t qx = floorDiv(x - 40, POI_CELL); qx <= floorDiv(x + 40, POI_CELL) && ok; qx++) {
              Poi o = poiRaw(poiOrder[hj], qx, qy);
              if (o.ok && dist2(o.x, o.y, x, y) < 40ll * 40ll) ok = false;
            }
        // off the roads
        for (auto& e : near) {
          if (!ok) break;
          if (x < e->x0 - 10 || x > e->x1 + 10 || y < e->y0 - 10 || y > e->y1 + 10) continue;
          for (const GTile& g : e->pts) if (dist2(g.x, g.y, x, y) < 8ll * 8ll) { ok = false; break; }
        }
        if (!ok) continue;
        SitePlan p;
        p.type = t;
        uint32_t cellIdx = (uint32_t)((cy - floorDiv(y0, POI_CELL)) * 2 + (cx - floorDiv(x0, POI_CELL)));
        p.id = makeId(rx, ry, IdKind::Site, 0x100u + (uint32_t)oi * 0x10u + cellIdx);
        p.ex = x; p.ey = y;
        p.seed = raw.seed;
        int flat = natLevel(x, y);
        switch (t) {
          case SiteType::Cave: p.w = 5; p.h = 4; p.gx = x - 2; p.gy = y - 1; flat = -1; break;
          case SiteType::Ruin: p.w = 9; p.h = 7; p.gx = x - 4; p.gy = y - 2; break;
          case SiteType::BanditCamp: p.w = 11; p.h = 9; p.gx = x - 5; p.gy = y - 4; break;
          default: p.w = 5; p.h = 5; p.gx = x - 2; p.gy = y - 2; break;
        }
        Rng nr(p.seed ^ 0xA5A5u);
        p.name = makeDungeonName(nr, t, tile(x, y + 2).biome);
        if (t == SiteType::Cave) {
          Biome b = tile(x, y + 2).biome;
          static const Monster themes[] = {Monster::Spider, Monster::Troll, Monster::Goblin, Monster::Bat, Monster::Skeleton};
          p.theme = b == Biome::Snow ? ((p.seed & 1) ? Monster::FrostSpider : Monster::IceWolf) : b == Biome::Desert ? Monster::Sandworm : themes[p.seed % 5];
        } else if (t == SiteType::Ruin) {
          static const Monster themes[] = {Monster::Draugr, Monster::Skeleton, Monster::Wraith};
          p.theme = themes[p.seed % 3];
        }
        add(p, flat);
        placed.push_back({x, y});
      }
  }
  // ---- the dragon lair of this 2x2-kingdom-cell block, if it stands in this region
  for (int32_t ly = floorDiv(y0 + 512, LAIR_CELL); ly <= floorDiv(y1 - 1 + 512, LAIR_CELL); ly++)
    for (int32_t lx = floorDiv(x0 + 512, LAIR_CELL); lx <= floorDiv(x1 - 1 + 512, LAIR_CELL); lx++) {
      int32_t ax, ay; uint32_t s;
      if (!lairOf(lx, ly, ax, ay, s) || ax < x0 || ay < y0 || ax >= x1 || ay >= y1) continue;
      bool clear = true;
      for (const Node& n : nodes) if (dist2(n.x, n.y, ax, ay) < (int64_t)(nominalR(n.type) + 60) * (nominalR(n.type) + 60)) clear = false;
      if (!clear) continue;
      SitePlan p;
      p.type = SiteType::DragonLair;
      p.id = makeId(rx, ry, IdKind::Site, LOCAL_LAIR);
      p.ex = ax; p.ey = ay;
      p.w = 17; p.h = 15; p.gx = ax - 8; p.gy = ay - 7;
      p.seed = s;
      p.theme = Monster::Dragon;
      Rng nr(p.seed ^ 0xA5A5u);
      p.name = makeDungeonName(nr, p.type, tile(ax, ay + 2).biome);
      add(p, natLevel(ax, ay));
      placed.push_back({ax, ay});
    }
  // ---- spurs: a track from every wilderness site to the nearest road within 90 tiles
  for (const SitePlan& p : R.sites) {
    if (isSettlement(p.type)) continue;
    int32_t sx = p.ex, sy = p.type == SiteType::Cave ? p.ey + 2 : p.gy + p.h;
    GTile at;
    if (!nearestRoad(sx, sy, 90, at)) continue;
    int32_t L = idist(sx, sy, at.x, at.y);
    if (L < 6) continue;
    uint64_t h = mix64(p.id ^ 0x5B0Bu);
    int32_t side = (int32_t)(h % 41) - 20;   // percent of the length, sideways
    int32_t mx = (sx + at.x) / 2 - (at.y - sy) * side / 100, my = (sy + at.y) / 2 + (at.x - sx) * side / 100;
    std::vector<GTile> pts = {GTile{sx, sy}, GTile{mx, my}, GTile{at.x, at.y}};
    chaikin(pts, 2);
    RoadPlan rp;
    rp.id = makeId(rx, ry, IdKind::Edge, 0x800u | (idLocal(p.id) & 0x7FFu));
    rp.cls = 2;
    rp.a = p.id;
    rp.pts = std::move(pts);
    R.roads.push_back(std::move(rp));
  }
  // ---- graph roads touching the region (public copy for the map and the chunks)
  for (auto& e : D.edges) {
    RoadPlan rp;
    rp.id = makeId(regionOf(e->pts.front().x), regionOf(e->pts.front().y), IdKind::Edge, (uint32_t)(e->key & 0x7FF));
    rp.cls = e->cls;
    rp.a = e->a; rp.b = e->b;
    rp.pts = e->pts;
    R.roads.push_back(std::move(rp));
  }
  // ---- rivers and lakes (public copy)
  {
    std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
    for (const RiverSeg& s : H->segs) R.rivers.push_back(RiverPlan{GTile{s.x0, s.y0}, GTile{s.x1, s.y1}, s.w});
    for (const Lake& l : H->lakes) R.lakes.push_back(LakePlan{l.x, l.y, l.r});
  }
  // ---- dens (VISION_PLAN 2.5: about a dozen per region) on their own lattice
  for (int32_t cy = floorDiv(y0, DEN_CELL); cy <= floorDiv(y1 - 1, DEN_CELL); cy++)
    for (int32_t cx = floorDiv(x0, DEN_CELL); cx <= floorDiv(x1 - 1, DEN_CELL); cx++) {
      uint64_t s = cellSeed(seed, tag("lat.den"), cx, cy);
      if (hq(s) >= Q(0.55)) continue;
      uint64_t h2 = mix64(s ^ 0x77ull);
      int32_t span = DEN_CELL - 2 * DEN_M;
      int32_t x = cx * DEN_CELL + DEN_M + (int32_t)((h2 & 0xFFFFFFFFull) % (uint64_t)span);
      int32_t y = cy * DEN_CELL + DEN_M + (int32_t)((h2 >> 32) % (uint64_t)span);
      gen::TileF m = tile(x, y);
      if (m.sea || m.rock || m.biome == Biome::Beach || m.biome == Biome::Mountain) continue;
      int L = natLevel(x, y);
      bool ok = natLevel(x - 4, y) == L && natLevel(x + 4, y) == L && natLevel(x, y - 4) == L && natLevel(x, y + 4) == L;
      if (!ok || riverWidthCell(x, y) || lakeNear(x, y, 6)) continue;
      for (const Node& n : nodes) if (dist2(n.x, n.y, x, y) < (int64_t)(nominalR(n.type) + 50) * (nominalR(n.type) + 50)) { ok = false; break; }
      for (const SitePlan& f : forcedSites) if (ok && dist2(f.ex, f.ey, x, y) < 50ll * 50ll) ok = false;
      for (auto& pp : placed) if (ok && dist2(pp.first, pp.second, x, y) < 30ll * 30ll) ok = false;
      for (auto& e : near) {
        if (!ok) break;
        if (x < e->x0 - 10 || x > e->x1 + 10 || y < e->y0 - 10 || y > e->y1 + 10) continue;
        for (const GTile& g : e->pts) if (dist2(g.x, g.y, x, y) < 10ll * 10ll) { ok = false; break; }
      }
      // raw POIs of the neighbouring regions too (pure), so a den never crowds a site across a region edge
      for (int oi = 0; oi < 4 && ok; oi++)
        for (int32_t qy = floorDiv(y - 30, POI_CELL); qy <= floorDiv(y + 30, POI_CELL) && ok; qy++)
          for (int32_t qx = floorDiv(x - 30, POI_CELL); qx <= floorDiv(x + 30, POI_CELL) && ok; qx++) {
            Poi o = poiRaw(poiOrder[oi], qx, qy);
            if (o.ok && dist2(o.x, o.y, x, y) < 26ll * 26ll) ok = false;
          }
      if (!ok) continue;
      DenPlan d;
      d.id = makeId(rx, ry, IdKind::Den, (uint32_t)((cy - floorDiv(y0, DEN_CELL)) * 4 + (cx - floorDiv(x0, DEN_CELL))));
      d.x = x; d.y = y;
      int32_t q = hq(mix64(s ^ 0x99ull));
      switch (m.biome) {
        case Biome::Plains: d.mon = q < Q(0.5) ? Monster::Wolf : q < Q(0.85) ? Monster::Goblin : Monster::Skeleton; break;
        case Biome::Forest: d.mon = q < Q(0.45) ? Monster::Wolf : q < Q(0.75) ? Monster::Spider : Monster::Bear; break;
        case Biome::Autumn: d.mon = q < Q(0.4) ? Monster::Goblin : q < Q(0.7) ? Monster::Spider : Monster::Bear; break;
        case Biome::Taiga: d.mon = q < Q(0.5) ? Monster::Wolf : q < Q(0.8) ? Monster::Bear : Monster::Troll; break;
        case Biome::Snow: d.mon = q < Q(0.55) ? Monster::IceWolf : q < Q(0.8) ? Monster::FrostSpider : Monster::Troll; break;
        case Biome::Swamp: d.mon = q < Q(0.6) ? Monster::Spider : Monster::Skeleton; break;
        case Biome::Desert: d.mon = q < Q(0.6) ? Monster::Goblin : Monster::Skeleton; break;
        default: continue;
      }
      switch (d.mon) {
        case Monster::Wolf: case Monster::IceWolf: d.pack = (uint8_t)(3 + (q & 1)); break;
        case Monster::Goblin: d.pack = (uint8_t)(3 + ((q >> 1) & 1)); break;
        case Monster::Skeleton: d.pack = 3; break;
        case Monster::Spider: case Monster::FrostSpider: d.pack = 2; break;
        default: d.pack = 1; break;
      }
      R.dens.push_back(d);
    }
  for (const DenPlan& d : forcedDens)
    if (d.x >= x0 && d.y >= y0 && d.x < x1 && d.y < y1) R.dens.push_back(d);
  uint32_t fp = 2166136261u;
  for (const SitePlan& s : R.sites) {
    fp = (fp ^ (uint32_t)s.id) * 16777619u;
    fp = (fp ^ (uint32_t)s.ex) * 16777619u;
    fp = (fp ^ (uint32_t)s.ey) * 16777619u;
  }
  R.fingerprint = fp;
}

}  // namespace ew
