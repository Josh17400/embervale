// L0.5 hydrology (VISION_PLAN 2.5): lakes and spring-traced rivers. WORLD lane.
//
// Springs sit on a 384-tile jittered lattice on high ground. Each traces steepest descent on a 32-tile grid of
// E' = E + 0.15 C (the C term tilts every landmass toward its coast, which removes most noise pits). A trace stops at
// the sea or a lattice lake; at a pit it fills a lake and spills over its lowest rim (at most twice). Node jitter is a
// function of the grid cell, so traces that reach the same cell run the same course from there: tributaries and
// confluences come for free. Widths grow with the distance from the spring (1 to 4 tiles). Everything depends only on
// the pure elevation; traces are cached by spring, and each region keeps an index of the segments and lakes touching
// it plus a per-8-tile-cell river width map (roads, archetypes, sites).
#include <algorithm>
#include <cstdint>
#include <tuple>
#include "rpg/world/gen.h"

namespace ew {
using namespace gen;

namespace {
constexpr int32_t HG = 32;          // routing grid (tiles)
constexpr int32_t SPRING_CELL = 384;
constexpr int32_t LAKE_CELL = 640;
constexpr int MAX_STEPS = 128;      // 4096 tiles
constexpr int32_t REACH = MAX_STEPS * HG + 64;
}  // namespace

int32_t EndlessSource::Impl::hydroE(int32_t i, int32_t j) {
  uint64_t k = key2(i, j);
  auto it = hydroMemo.find(k);
  if (it != hydroMemo.end()) return it->second;
  if (hydroMemo.size() > 400000) hydroMemo.clear();
  int32_t C = 0;
  const int32_t x = i * HG + HG / 2, y = j * HG + HG / 2;
  int32_t e = elevation(x, y, &C, nullptr, 1);   // the smooth land: few noise pits
  // the sea is decided by the full field (the coast the tiles draw), so a river runs on to the real shore
  bool sea = false;
  if (e < ELEV_SEA + Q(0.06)) sea = elevation(x, y, nullptr, nullptr, 4) < ELEV_SEA;
  // sea cells are marked by an E' below every land value; the C term tilts the land toward its coast
  int32_t ep = sea ? INT32_MIN / 2 + e : e + qm(Q(0.30), C);
  hydroMemo.emplace(k, ep);
  return ep;
}

bool EndlessSource::Impl::latticeLake(int32_t cx, int32_t cy, Lake& out) {
  uint64_t mk = key2(cx, cy);
  auto it = lakeMemo.find(mk);
  if (it != lakeMemo.end()) { out = it->second.second; return it->second.first; }
  if (lakeMemo.size() > 100000) lakeMemo.clear();
  Lake L;
  bool ok = latticeLakeRaw(cx, cy, L);
  lakeMemo.emplace(mk, std::make_pair(ok, L));
  out = L;
  return ok;
}

bool EndlessSource::Impl::latticeLakeRaw(int32_t cx, int32_t cy, Lake& out) {
  uint64_t h = cellSeed(seed, tag("h.lake"), cx, cy);
  if (hq(h) >= Q(0.40)) return false;
  int32_t x = cx * LAKE_CELL + 120 + (int32_t)((mix64(h ^ 1) >> 33) % 400);
  int32_t y = cy * LAKE_CELL + 120 + (int32_t)((mix64(h ^ 2) >> 33) % 400);
  int32_t r = 12 + (int32_t)((mix64(h ^ 3) >> 33) % 22);
  int32_t ridge = 0;
  int32_t e = elevation(x, y, nullptr, &ridge);
  if (e < ELEV_SEA + Q(0.03) || e > Q(0.70) || ridge > Q(0.25)) return false;
  // one relief level under and around the lake (raw field: approximate, the chunk forces the lake surface level)
  int lv = levelOf(e);
  for (int k = 0; k < 8; k++) {
    static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1}, dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    int32_t rr = (k < 4 ? r + 6 : (r + 6) * 7 / 10);
    int32_t e2 = elevation(x + dx[k] * rr, y + dy[k] * rr, nullptr, nullptr);
    if (e2 < ELEV_SEA + Q(0.01) || levelOf(e2) != lv) return false;
  }
  out.x = x; out.y = y; out.r = r; out.seed = (uint32_t)(h >> 20);
  return true;
}

bool EndlessSource::Impl::lakeNear(int32_t x, int32_t y, int32_t pad) {
  int32_t cx0 = floorDiv(x - 60 - pad, LAKE_CELL), cx1 = floorDiv(x + 60 + pad, LAKE_CELL);
  int32_t cy0 = floorDiv(y - 60 - pad, LAKE_CELL), cy1 = floorDiv(y + 60 + pad, LAKE_CELL);
  for (int32_t cy = cy0; cy <= cy1; cy++)
    for (int32_t cx = cx0; cx <= cx1; cx++) {
      Lake L;
      if (latticeLake(cx, cy, L) && dist2(L.x, L.y, x, y) < (int64_t)(L.r + pad) * (L.r + pad)) return true;
    }
  return false;
}

bool EndlessSource::Impl::spring(int32_t cx, int32_t cy, int32_t& x, int32_t& y) {
  uint64_t h = cellSeed(seed, tag("h.spring"), cx, cy);
  if (hq(h) >= Q(0.85)) return false;
  x = cx * SPRING_CELL + 64 + (int32_t)((mix64(h ^ 1) >> 33) % 256);
  y = cy * SPRING_CELL + 64 + (int32_t)((mix64(h ^ 2) >> 33) % 256);
  int32_t C = 0;
  int32_t e = elevation(x, y, &C, nullptr);
  return e >= Q(0.50) && e < Q(0.88) && C > Q(0.58);
}

std::shared_ptr<const Trace> EndlessSource::Impl::trace(int32_t cx, int32_t cy) {
  uint64_t k = key2(cx, cy);
  if (auto t = traces.get(k)) return t;
  auto t0 = Clock::now();
  auto T = std::make_shared<Trace>();
  int32_t sx, sy;
  if (spring(cx, cy, sx, sy)) {
    stats.rivers++;
    struct P { int32_t x, y; uint8_t w; };
    std::vector<P> pts;
    std::vector<std::pair<int32_t, int32_t>> cells;
    int32_t i = floorDiv(sx, HG), j = floorDiv(sy, HG);
    pts.push_back({sx, sy, 1});
    cells.push_back({i, j});
    int spills = 0;
    bool toSea = false, ended = false;
    for (int step = 0; step < MAX_STEPS; step++) {
      int32_t e0 = hydroE(i, j);
      if (e0 < INT32_MIN / 4) { toSea = true; ended = true; stats.tracesToSea++; break; }
      if (step > 0 && lakeNear(i * HG + HG / 2, j * HG + HG / 2, 0)) { ended = true; stats.tracesToLake++; break; }
      static const int dx[8] = {1, 0, -1, 0, 1, -1, -1, 1}, dy[8] = {0, 1, 0, -1, 1, 1, -1, -1};
      int bi = -1;
      int32_t be = e0;
      for (int n = 0; n < 8; n++) {
        int32_t ni = i + dx[n], nj = j + dy[n];
        int32_t en = hydroE(ni, nj);
        if (en < be) {
          bool seen = false;
          for (auto& c : cells) if (c.first == ni && c.second == nj) { seen = true; break; }
          if (!seen) { be = en; bi = n; }
        }
      }
      if (bi < 0) {
        // a pit: the river spills over the lowest unvisited rim; a deep pit holds a lake on the way
        if (++spills > 5) break;
        int32_t low = INT32_MAX;
        for (int n = 0; n < 8; n++) {
          int32_t ni = i + dx[n], nj = j + dy[n];
          bool seen = false;
          for (auto& c : cells) if (c.first == ni && c.second == nj) { seen = true; break; }
          if (seen) continue;
          int32_t en = hydroE(ni, nj);
          if (en < low) { low = en; bi = n; }
        }
        if (bi < 0) break;
        if (low - e0 > Q(0.012)) {
          Lake L;
          L.x = i * HG + HG / 2; L.y = j * HG + HG / 2;
          uint64_t lh = tileHash(seed ^ tag("h.pit"), i, j);
          L.r = 11 + (int32_t)((lh >> 40) % 14);
          L.seed = (uint32_t)(lh >> 8);
          T->lakes.push_back(L);
        }
      }
      i += dx[bi]; j += dy[bi];
      cells.push_back({i, j});
      uint64_t jh = tileHash(seed ^ tag("h.jit"), i, j);
      int32_t jx = (int32_t)(jh & 31) - 16, jy = (int32_t)((jh >> 8) & 31) - 16;   // +-16 tiles: winding courses
      int k2 = (int)cells.size() - 1;
      uint8_t w = (uint8_t)(1 + std::min(3, k2 / 18));
      pts.push_back({i * HG + HG / 2 + jx * 5 / 8, j * HG + HG / 2 + jy * 5 / 8, w});
    }
    if (!ended && pts.size() >= 2) {
      // a river that found neither the sea nor a lake within reach ends in a lake of its own (never in the middle of
      // a field)
      Lake L;
      L.x = i * HG + HG / 2; L.y = j * HG + HG / 2;
      uint64_t lh = tileHash(seed ^ tag("h.end"), i, j);
      L.r = 13 + (int32_t)((lh >> 40) % 12);
      L.seed = (uint32_t)(lh >> 8);
      T->lakes.push_back(L);
      stats.tracesToLake++;
    }
    if (toSea && pts.size() >= 2) {
      // run on into the sea so the mouth is open water
      const P& a = pts[pts.size() - 2];
      const P& b = pts.back();
      pts.push_back({b.x + (b.x - a.x), b.y + (b.y - a.y), b.w});
    }
    if (pts.size() >= 2) {
      // two Chaikin passes (integer, floor division): smooth bends
      for (int pass = 0; pass < 2; pass++) {
        std::vector<P> q;
        q.push_back(pts.front());
        for (size_t n = 0; n + 1 < pts.size(); n++) {
          const P &a = pts[n], &b = pts[n + 1];
          q.push_back({floorDiv(3 * a.x + b.x, 4), floorDiv(3 * a.y + b.y, 4), a.w});
          q.push_back({floorDiv(a.x + 3 * b.x, 4), floorDiv(a.y + 3 * b.y, 4), b.w});
        }
        q.push_back(pts.back());
        pts.swap(q);
      }
      T->x0 = T->y0 = INT32_MAX; T->x1 = T->y1 = INT32_MIN;
      for (size_t n = 0; n + 1 < pts.size(); n++) {
        const P &a = pts[n], &b = pts[n + 1];
        if (a.x == b.x && a.y == b.y) continue;
        T->segs.push_back(RiverSeg{a.x, a.y, b.x, b.y, std::max(a.w, b.w)});
        T->x0 = std::min({T->x0, a.x - 4, b.x - 4}); T->x1 = std::max({T->x1, a.x + 4, b.x + 4});
        T->y0 = std::min({T->y0, a.y - 4, b.y - 4}); T->y1 = std::max({T->y1, a.y + 4, b.y + 4});
      }
      for (const Lake& L : T->lakes) {
        T->x0 = std::min(T->x0, L.x - L.r - 12); T->x1 = std::max(T->x1, L.x + L.r + 12);
        T->y0 = std::min(T->y0, L.y - L.r - 12); T->y1 = std::max(T->y1, L.y + L.r + 12);
      }
      if (T->segs.empty() && T->lakes.empty()) { T->x0 = T->y0 = 0; T->x1 = T->y1 = -1; }
    } else {
      T->x1 = T->y1 = -1;
    }
  } else {
    T->x1 = T->y1 = -1;
  }
  stats.traces++;
  stats.traceMs += msSince(t0);
  traces.put(k, T);
  return T;
}

std::shared_ptr<const RegionHydro> EndlessSource::Impl::hydro(int32_t rx, int32_t ry) {
  uint64_t k = key2(rx, ry);
  if (auto h = hydros.get(k)) return h;
  Nest nest(*this);
  auto H = std::make_shared<RegionHydro>();
  const int32_t x0 = rx * REGION, y0 = ry * REGION, x1 = x0 + REGION, y1 = y0 + REGION, M = 12;
  const int32_t sc0x = floorDiv(x0 - REACH, SPRING_CELL), sc1x = floorDiv(x1 + REACH, SPRING_CELL);
  const int32_t sc0y = floorDiv(y0 - REACH, SPRING_CELL), sc1y = floorDiv(y1 + REACH, SPRING_CELL);
  std::vector<std::tuple<int32_t, int32_t, int32_t, int32_t, uint8_t>> segs;
  std::vector<std::tuple<int32_t, int32_t, int32_t, uint32_t>> lakes;
  for (int32_t cy = sc0y; cy <= sc1y; cy++)
    for (int32_t cx = sc0x; cx <= sc1x; cx++) {
      std::shared_ptr<const Trace> T = trace(cx, cy);
      if (T->x1 < T->x0 || T->x1 < x0 - M || T->x0 >= x1 + M || T->y1 < y0 - M || T->y0 >= y1 + M) continue;
      for (const RiverSeg& s : T->segs) {
        if (std::max(s.x0, s.x1) + 3 < x0 - M || std::min(s.x0, s.x1) - 3 >= x1 + M) continue;
        if (std::max(s.y0, s.y1) + 3 < y0 - M || std::min(s.y0, s.y1) - 3 >= y1 + M) continue;
        segs.emplace_back(s.x0, s.y0, s.x1, s.y1, s.w);
      }
      for (const Lake& L : T->lakes)
        if (L.x + L.r + 12 >= x0 - M && L.x - L.r - 12 < x1 + M && L.y + L.r + 12 >= y0 - M && L.y - L.r - 12 < y1 + M)
          lakes.emplace_back(L.x, L.y, L.r, L.seed);
    }
  for (int32_t cy = floorDiv(y0 - 80, LAKE_CELL); cy <= floorDiv(y1 + 80, LAKE_CELL); cy++)
    for (int32_t cx = floorDiv(x0 - 80, LAKE_CELL); cx <= floorDiv(x1 + 80, LAKE_CELL); cx++) {
      Lake L;
      if (!latticeLake(cx, cy, L)) continue;
      if (L.x + L.r + 12 >= x0 - M && L.x - L.r - 12 < x1 + M && L.y + L.r + 12 >= y0 - M && L.y - L.r - 12 < y1 + M)
        lakes.emplace_back(L.x, L.y, L.r, L.seed);
    }
  // merged traces share segments: dedupe (keeping the widest), in a fixed order
  std::sort(segs.begin(), segs.end());
  for (size_t n = 0; n < segs.size(); n++) {
    if (n + 1 < segs.size() && std::get<0>(segs[n]) == std::get<0>(segs[n + 1]) && std::get<1>(segs[n]) == std::get<1>(segs[n + 1]) &&
        std::get<2>(segs[n]) == std::get<2>(segs[n + 1]) && std::get<3>(segs[n]) == std::get<3>(segs[n + 1]))
      continue;   // the next one has the same ends and a width >= this one (sorted)
    H->segs.push_back(RiverSeg{std::get<0>(segs[n]), std::get<1>(segs[n]), std::get<2>(segs[n]), std::get<3>(segs[n]), std::get<4>(segs[n])});
  }
  std::sort(lakes.begin(), lakes.end());
  lakes.erase(std::unique(lakes.begin(), lakes.end()), lakes.end());
  for (auto& l : lakes) H->lakes.push_back(Lake{std::get<0>(l), std::get<1>(l), std::get<2>(l), std::get<3>(l)});
  for (const RiverSeg& s : H->segs)
    walk4(s.x0, s.y0, s.x1, s.y1, [&](int32_t x, int32_t y) {
      for (int32_t oy = -2; oy <= 2; oy++)
        for (int32_t ox = -2; ox <= 2; ox++) {
          int32_t tx = x + ox, ty = y + oy;
          if (tx < x0 || ty < y0 || tx >= x1 || ty >= y1) continue;
          uint8_t& c = H->riverW[((ty - y0) >> 3) * HN + ((tx - x0) >> 3)];
          c = std::max(c, s.w);
        }
    });
  for (const Lake& L : H->lakes)
    for (int j = 0; j < HN; j++)
      for (int i = 0; i < HN; i++)
        if (dist2(x0 + i * HC + 4, y0 + j * HC + 4, L.x, L.y) < (int64_t)(L.r + 6) * (L.r + 6)) H->lakeCell[j * HN + i] = 1;
  hydros.put(k, H);
  return H;
}

int EndlessSource::Impl::riverWidthCell(int32_t x, int32_t y) {
  std::shared_ptr<const RegionHydro> H = hydro(regionOf(x), regionOf(y));
  int i = (x - regionOf(x) * REGION) >> 3, j = (y - regionOf(y) * REGION) >> 3;
  return H->riverW[j * HN + i] + (H->lakeCell[j * HN + i] ? 8 : 0);
}

// (M1) Lakes and settlements are planned independently: a lake whose shore would reach into a settlement's footprint
// (a capital with a lake in its market and ponds across its streets) is dropped. Only the chunk land and waterAt
// read this, never the region planner (which would recurse into itself).
bool EndlessSource::Impl::lakeUnderSettlement(const Lake& L) {
  const uint64_t k = key2(L.x, L.y) ^ ((uint64_t)L.seed << 7);
  auto it = lakeTownMemo.find(k);
  if (it != lakeTownMemo.end()) return it->second;
  const int32_t R = (L.r * 17) / 10 + 2;   // the lobed shore never passes 1.6 r
  bool under = false;
  for (int32_t ry = regionOf(L.y - R - 200); ry <= regionOf(L.y + R + 200) && !under; ry++)
    for (int32_t rx = regionOf(L.x - R - 200); rx <= regionOf(L.x + R + 200) && !under; rx++) {
      std::shared_ptr<const RegionData> D = regionData(rx, ry);
      for (const SitePlan& p : D->plan.sites) {
        if (!isSettlement(p.type)) continue;
        const int32_t m = 6;
        if (L.x + R >= p.gx - m && L.x - R < p.gx + p.w + m && L.y + R >= p.gy - m && L.y - R < p.gy + p.h + m) { under = true; break; }
      }
    }
  lakeTownMemo.emplace(k, under);
  return under;
}

bool EndlessSource::Impl::waterAt(int32_t x, int32_t y) {
  std::shared_ptr<const RegionHydro> H = hydro(regionOf(x), regionOf(y));
  for (const Lake& L : H->lakes) {
    int32_t rr = lakeRadius(L, x, y);
    if (dist2(x, y, L.x, L.y) < (int64_t)rr * rr && !lakeUnderSettlement(L)) return true;
  }
  int i = (x - regionOf(x) * REGION) >> 3, j = (y - regionOf(y) * REGION) >> 3;
  if (!H->riverW[j * HN + i]) return false;
  for (const RiverSeg& s : H->segs) {
    // distance from the segment, in tiles (squared, integer projection)
    int64_t vx = s.x1 - s.x0, vy = s.y1 - s.y0, wx = x - s.x0, wy = y - s.y0;
    int64_t L2 = vx * vx + vy * vy, t = wx * vx + wy * vy;
    int64_t d2;
    if (t <= 0) d2 = wx * wx + wy * wy;
    else if (t >= L2) d2 = dist2(x, y, s.x1, s.y1);
    else { int64_t cr = wx * vy - wy * vx; d2 = cr * cr / L2; }
    int64_t r = (s.w + 1) / 2;
    if (d2 <= r * r) return true;
  }
  return false;
}

}  // namespace ew
