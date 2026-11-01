// L2b, the chunk pipeline (VISION_PLAN 2.5), in a fixed order: base land, rivers and lakes, settlement buffers (cached),
// site and den stamps, roads and tracks (bridges, fords), relief (levels, cliff faces, ramps), vegetation. WORLD lane.
//
// Every step reads only plans and pure functions, so chunks come out the same in any order. The base land is computed
// for the chunk plus a 2-tile border, roads and relief bits for a 1-tile border: decisions that look at neighbours
// (cliffs, ramps, keeping trees off ramps and roads) see the same picture on both sides of a chunk edge, so there are
// no seams.
//
// Relief contract with the VIEW lane (VISION_PLAN 11): every tile on the LOWER side of a level step (4-neighbour) gets
// Map::HEIGHT_CLIFF (not walkable) unless it is a ramp (Map::HEIGHT_RAMP, walkable). Ramps sit on a global lattice:
// along a face running east-west (the higher tile north or south) a ramp is where (x + offset(y / 16)) mod 16 < 3;
// along a face running north-south the same with x and y swapped. Every run of 14 or more tiles gets a 3-tile ramp,
// stacked cliffs line up into stairways, and roads and tracks turn every cliff they touch into a ramp. Settlements
// and sites stand on one level: the land is flattened under them and terraced back to its own level around them.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>
#ifndef __EMSCRIPTEN__
#include <mutex>
#endif
#include "rpg/world/gen.h"
#include "rpg/world/town_gen.h"

namespace ew {
using namespace gen;
using art::Monster;
using art::Prop;

namespace {
constexpr int BR = ChunkCtx::BR;              // base border
constexpr int BW = ChunkCtx::BW;              // 36
constexpr int RW = ChunkCtx::RW;              // relief / road grid (1-tile border): 34

// (M3 towns lane, carry-over 2) Settlements shared between the sources of one process. Natively the prefetcher's
// worker has its own EndlessSource (one instance is not thread-safe), so every town near the player was built twice,
// once per source, and the worker's first city on a cold source (20-40 ms) is what let the first shifts of a walk fall
// behind. A finished town is immutable and a pure function of (world seed, site), so the sources hand finished ones to
// each other through this small locked store (LRU, 24 towns). The web has one source and no threads: no store.
#ifndef __EMSCRIPTEN__
struct SharedTowns {
  std::mutex mu;
  struct E { std::shared_ptr<SettlementOut> out; uint64_t used = 0; };
  std::unordered_map<uint64_t, E> m;
  uint64_t clock = 0;
};
SharedTowns& sharedTowns() {
  static SharedTowns s;
  return s;
}
uint64_t sharedTownKey(uint64_t seed, Gid id) { return mix64(seed * 0x9E3779B97F4A7C15ull ^ id ^ 0x70A5E7ull); }
std::shared_ptr<SettlementOut> sharedTownGet(uint64_t seed, Gid id) {
  SharedTowns& s = sharedTowns();
  std::lock_guard<std::mutex> lk(s.mu);
  auto it = s.m.find(sharedTownKey(seed, id));
  if (it == s.m.end()) return nullptr;
  it->second.used = ++s.clock;
  return it->second.out;
}
void sharedTownPut(uint64_t seed, Gid id, std::shared_ptr<SettlementOut> out) {
  SharedTowns& s = sharedTowns();
  std::lock_guard<std::mutex> lk(s.mu);
  if (s.m.size() >= 24) {
    auto oldest = s.m.begin();
    for (auto i = s.m.begin(); i != s.m.end(); ++i) if (i->second.used < oldest->second.used) oldest = i;
    s.m.erase(oldest);
  }
  s.m[sharedTownKey(seed, id)] = SharedTowns::E{std::move(out), ++s.clock};
}
#endif


// (M3c) half the ecotone band's width (tiles) between two ecos: wide and soft between grasslands and toward the
// deserts, narrow at marsh edges, middling between woods and open land (at most 4: the chunk's eco grid reaches 4)
int ecoHalfW(Eco a, Eco b) {
  const Biome fa = ecoFamily(a), fb = ecoFamily(b);
  if (fa == Biome::Swamp || fb == Biome::Swamp) return 2;
  const bool wa = ecoHas(a, EF_WET) && fa != Biome::Plains, wb = ecoHas(b, EF_WET) && fb != Biome::Plains;
  if (wa || wb) return 2;
  if (ecoHas(a, EF_RARE) || ecoHas(b, EF_RARE)) return 3;
  // (M4, owner carry-over: the heath "read as flat green blobs") the moor meets the grass along a tighter, ragged line:
  // a wide band scattered islands of bright turf over the dark heather
  if (a == Eco::Heath || b == Eco::Heath) return 2;
  const bool ga = fa == Biome::Plains, gb = fb == Biome::Plains;
  if (ga && gb) return 4;
  if ((ga && fb == Biome::Desert) || (gb && fa == Biome::Desert)) return 4;
  if (fa == Biome::Desert && fb == Biome::Desert) return 3;
  const bool woodA = ecoHas(a, EF_WOODED), woodB = ecoHas(b, EF_WOODED);
  if (woodA && woodB) return 3;
  if (woodA != woodB) return 3;
  if (fa == Biome::Desert || fb == Biome::Desert) return 3;
  return 2;
}
}  // namespace

// ------------------------------------------------------------------ steps 1-2: base land, rivers, lakes
void EndlessSource::Impl::baseRect(int32_t x0, int32_t y0, int w, int h, BaseRect& B) {
  baseRectRows(x0, y0, w, h, B, 0, h);
  baseRectWater(B);
}

void EndlessSource::Impl::baseRectRows(int32_t x0, int32_t y0, int w, int h, BaseRect& B, int yFrom, int yTo) {
  if (yFrom == 0) {
    B.x0 = x0; B.y0 = y0; B.w = w; B.h = h;
    const size_t n = (size_t)w * h;
    B.ground.assign(n, 0); B.biome.assign(n, 0); B.level.assign(n, 0); B.riverW.assign(n, 0); B.lake.assign(n, 0); B.bridge.assign(n, 0);
    B.eco.assign(n, 0);
  }
  for (int y = std::max(0, yFrom); y < std::min(h, yTo); y++)
    for (int x = 0; x < w; x++) {
      TileF f = tile(x0 + x, y0 + y);
      size_t i = (size_t)y * w + x;
      B.biome[i] = (uint8_t)f.biome;
      B.eco[i] = (uint8_t)f.eco;
      B.ground[i] = (uint8_t)groundFor(f.eco, f.e, f.t, x0 + x, y0 + y);
      B.level[i] = f.h;
    }
}

void EndlessSource::Impl::baseRectWater(BaseRect& B) {
  const int32_t x0 = B.x0, y0 = B.y0;
  const int w = B.w, h = B.h;
  const size_t n = (size_t)w * h;
  std::vector<uint8_t> deep(n, 0), path(n, 0);
  const int32_t x1 = x0 + w, y1 = y0 + h;
  for (int32_t ry = regionOf(y0 - 4); ry <= regionOf(y1 + 3); ry++)
    for (int32_t rx = regionOf(x0 - 4); rx <= regionOf(x1 + 3); rx++) {
      std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
      for (const RiverSeg& s : H->segs) {
        if (std::max(s.x0, s.x1) + 3 < x0 || std::min(s.x0, s.x1) - 3 >= x1 || std::max(s.y0, s.y1) + 3 < y0 || std::min(s.y0, s.y1) - 3 >= y1) continue;
        const int lo = s.w >= 3 ? -1 : 0, hi = s.w == 1 ? 0 : s.w == 4 ? 2 : 1;
        walk4(s.x0, s.y0, s.x1, s.y1, [&](int32_t x, int32_t y) {
          for (int oy = lo; oy <= hi; oy++)
            for (int ox = lo; ox <= hi; ox++) {
              if (!B.in(x + ox, y + oy)) continue;
              size_t i = B.at(x + ox, y + oy);
              if (B.biome[i] == (uint8_t)Biome::Ocean) continue;
              B.riverW[i] = std::max(B.riverW[i], s.w);
              if (s.w >= 3 && ox == 0 && oy == 0) deep[i] = 1;
            }
        });
      }
      for (const Lake& L : H->lakes) {
        const int32_t R = 2 * L.r + 2;
        if (L.x + R < x0 || L.x - R >= x1 || L.y + R < y0 || L.y - R >= y1) continue;
        if (lakeUnderSettlement(L)) continue;
        const uint8_t lakeLv = (uint8_t)fineLevel(L.x, L.y);
        for (int32_t y = std::max(y0, L.y - R); y < std::min(y1, L.y + R + 1); y++)
          for (int32_t x = std::max(x0, L.x - R); x < std::min(x1, L.x + R + 1); x++) {
            int32_t rr = lakeRadius(L, x, y);
            int64_t d2 = dist2(x, y, L.x, L.y);
            if (d2 >= (int64_t)rr * rr) continue;
            size_t i = B.at(x, y);
            if (B.biome[i] == (uint8_t)Biome::Ocean) continue;
            if (!B.lake[i] || lakeLv < B.level[i]) B.level[i] = lakeLv;
            B.lake[i] = 1;
            if (d2 < (int64_t)(rr / 2) * (rr / 2)) deep[i] = 1;
          }
      }
    }
  // footbridges: rivers cannot be swum, so every few pieces of a river's course a plank bridge crosses it (about one
  // every 40-50 tiles; roads and tracks add their own). Chosen by a hash of the piece, so every rect agrees.
  for (int32_t ry = regionOf(y0 - 14); ry <= regionOf(y1 + 13); ry++)
    for (int32_t rx = regionOf(x0 - 14); rx <= regionOf(x1 + 13); rx++) {
      std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
      for (const RiverSeg& s : H->segs) {
        if (std::max(s.x0, s.x1) + 14 < x0 || std::min(s.x0, s.x1) - 14 >= x1 || std::max(s.y0, s.y1) + 14 < y0 || std::min(s.y0, s.y1) - 14 >= y1) continue;
        if (mix64(key2(s.x0, s.y0) ^ mix64(key2(s.x1, s.y1)) ^ seed ^ tag("b.bridge")) % 6 != 0) continue;
        int32_t len = idist(s.x0, s.y0, s.x1, s.y1);
        if (len < 5) continue;
        int32_t mx = floorDiv(s.x0 + s.x1, 2), my = floorDiv(s.y0 + s.y1, 2);
        // (M2 fixer round 2) no free footbridge within 28 tiles of a troll's toll bridge (it made the toll pointless)
        {
          bool toll = false;
          for (int k = 0; k < 9 && !toll; k++) {
            std::shared_ptr<const RegionData> TD = regionData(regionOf(mx) - 1 + k % 3, regionOf(my) - 1 + k / 3);
            for (const SitePlan& p : TD->plan.sites)
              if (p.type == SiteType::Vignette && p.kind == (uint8_t)VignetteKind::TollBridge && dist2(p.ex, p.ey, mx, my) <= 28ll * 28ll) { toll = true; break; }
          }
          if (toll) continue;
        }
        int32_t px = -(s.y1 - s.y0), py = s.x1 - s.x0, reach = s.w + 2;
        // (M3b fixer) the deck runs straight along the grid, the way nearest the river's normal (a slanted footbridge was a
        // staircase of planks jogging a tile mid-deck); it reaches as far as the slant would have
        if (std::abs(px) >= std::abs(py)) { px = px >= 0 ? len : -len; py = 0; reach = reach + (s.w + 1) / 2; }
        else { py = py >= 0 ? len : -len; px = 0; reach = reach + (s.w + 1) / 2; }
        walk4(mx - px * reach / len, my - py * reach / len, mx + px * reach / len, my + py * reach / len, [&](int32_t x, int32_t y) {
          if (!B.in(x, y)) return;
          size_t i = B.at(x, y);
          if (B.riverW[i] && !B.lake[i] && B.biome[i] != (uint8_t)Biome::Ocean) B.bridge[i] = 1;
        });
        // (M1 round 3) a trodden path leads on from both ends of the footbridge (it stood alone in the grass)
        const int32_t far = reach + 5;
        walk4(mx - px * far / len, my - py * far / len, mx + px * far / len, my + py * far / len, [&](int32_t x, int32_t y) {
          if (!B.in(x, y)) return;
          size_t i = B.at(x, y);
          if (!B.riverW[i] && !B.lake[i] && B.biome[i] != (uint8_t)Biome::Ocean) path[i] = 1;
        });
      }
    }
  for (size_t i = 0; i < n; i++) {
    if (path[i] && !B.bridge[i] && !B.lake[i] && !B.riverW[i] && !groundSolid((Ground)B.ground[i]) && B.ground[i] != (uint8_t)Ground::Sand)
      B.ground[i] = (uint8_t)Ground::Dirt;
    if (B.bridge[i]) B.ground[i] = (uint8_t)Ground::Bridge;
    else if (B.lake[i] || B.riverW[i]) B.ground[i] = (uint8_t)(deep[i] ? Ground::DeepWater : Ground::Water);
  }
  // lake shores: sandy patches on open land (only where the whole neighbourhood is inside the rect)
  for (int y = 1; y < h - 1; y++)
    for (int x = 1; x < w - 1; x++) {
      size_t i = (size_t)y * w + x;
      if (B.lake[i] || B.riverW[i] || waterG(B.ground[i])) continue;
      bool shore = false;
      for (int dy = -1; dy <= 1 && !shore; dy++)
        for (int dx = -1; dx <= 1; dx++) if (B.lake[(size_t)(y + dy) * w + x + dx]) { shore = true; break; }
      if (!shore) continue;
      Biome b = (Biome)B.biome[i];
      if (b != Biome::Plains && b != Biome::Desert && b != Biome::Beach && b != Biome::Forest && b != Biome::Autumn) continue;
      if (hq(tileHash(seed ^ tag("b.shore"), x0 + x, y0 + y)) < Q(0.62)) B.ground[i] = (uint8_t)Ground::Sand;
    }
}

// ------------------------------------------------------------------ settlements (cached buffers)
// A settlement being built: the land under it, then the town generator's phases (town::Gen::step). town() runs a job
// to the end at once; prepareChunk (the web's streaming, no threads) runs it a phase at a time within a frame budget.
struct EndlessSource::Impl::TownJob {
  Gid id = 0;
  std::shared_ptr<const RegionData> D;   // keeps the plan (ctx.plan points into it) alive
  const SitePlan* p = nullptr;
  int flat = 0;
  std::shared_ptr<BaseRect> B;
  SettlementCtx ctx;
  PortPlan port;                         // (M10) a port's harbour (ctx.harbour points here)
  std::shared_ptr<SettlementOut> out;
  std::unique_ptr<town::Gen> gen;
  int stage = 0;                         // 0 the land, 1 the generator, 2 done
  int row = 0;                           // stage 0: the land's rows made so far (a city's land is ~40k tiles)
  double ms = 0;
};

std::shared_ptr<EndlessSource::Impl::TownJob> EndlessSource::Impl::townJobFor(const SitePlan& p, std::shared_ptr<const RegionData> D) {
  if (townJob && townJob->id == p.id) return townJob;
  auto J = std::make_shared<TownJob>();
  J->id = p.id;
  J->D = std::move(D);
  J->p = &p;
  for (size_t k = 0; k < J->D->plan.sites.size(); k++)
    if (J->D->plan.sites[k].id == p.id) { J->p = &J->D->plan.sites[k]; J->flat = J->D->flatLevel[k].first; }
  J->out = std::make_shared<SettlementOut>();
  return J;
}

// (M1) A river that ENDS inside a settlement's footprint (it fed a lake there, and lakeUnderSettlement drops such
// lakes) would leave a dead-end pool across the streets. Its reaches inside the footprint, followed back from the
// end until the river leaves the footprint, are drained on the town's land. A river that runs through stays (bridged).
void EndlessSource::Impl::drainDeadEndRivers(const SitePlan& p, BaseRect& B) {
  const int32_t ax0 = p.gx - 8, ay0 = p.gy - 8, ax1 = p.gx + p.w + 8, ay1 = p.gy + p.h + 8;
  auto inA = [&](int32_t x, int32_t y) { return x >= ax0 && y >= ay0 && x < ax1 && y < ay1; };
  std::vector<RiverSeg> segs;
  for (int32_t ry = regionOf(ay0); ry <= regionOf(ay1); ry++)
    for (int32_t rx = regionOf(ax0); rx <= regionOf(ax1); rx++) {
      std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
      for (const RiverSeg& s : H->segs) {
        if (!inA(s.x0, s.y0) && !inA(s.x1, s.y1)) continue;
        bool dup = false;
        for (const RiverSeg& o : segs) if (o.x0 == s.x0 && o.y0 == s.y0 && o.x1 == s.x1 && o.y1 == s.y1) { dup = true; break; }
        if (!dup) segs.push_back(s);
      }
    }
  if (segs.empty()) return;
  // endpoints shared by no other piece are the river's ends (or springs)
  auto uses = [&](int32_t x, int32_t y) {
    int n = 0;
    for (const RiverSeg& s : segs) n += (s.x0 == x && s.y0 == y) + (s.x1 == x && s.y1 == y);
    return n;
  };
  std::vector<uint8_t> dry(segs.size(), 0);
  for (size_t i = 0; i < segs.size(); i++) {
    const RiverSeg& s = segs[i];
    if (!inA(s.x0, s.y0) || !inA(s.x1, s.y1)) continue;
    if (uses(s.x0, s.y0) == 1 || uses(s.x1, s.y1) == 1) dry[i] = 1;
  }
  for (bool grew = true; grew;) {
    grew = false;
    for (size_t i = 0; i < segs.size(); i++) {
      if (dry[i] || !inA(segs[i].x0, segs[i].y0) || !inA(segs[i].x1, segs[i].y1)) continue;
      for (size_t j = 0; j < segs.size() && !dry[i]; j++) {
        if (!dry[j]) continue;
        const RiverSeg &a = segs[i], &b = segs[j];
        if ((a.x0 == b.x0 && a.y0 == b.y0) || (a.x0 == b.x1 && a.y0 == b.y1) || (a.x1 == b.x0 && a.y1 == b.y0) || (a.x1 == b.x1 && a.y1 == b.y1)) {
          dry[i] = 1;
          grew = true;
        }
      }
    }
  }
  for (size_t i = 0; i < segs.size(); i++) {
    if (!dry[i]) continue;
    const RiverSeg& s = segs[i];
    const int lo = s.w >= 3 ? -1 : 0, hi = s.w == 1 ? 0 : s.w == 4 ? 2 : 1;
    walk4(s.x0, s.y0, s.x1, s.y1, [&](int32_t x, int32_t y) {
      for (int oy = lo - 1; oy <= hi + 1; oy++)
        for (int ox = lo - 1; ox <= hi + 1; ox++) {
          const int32_t gx = x + ox, gy = y + oy;
          if (!B.in(gx, gy) || !inA(gx, gy)) continue;
          const size_t k = B.at(gx, gy);
          if (!B.riverW[k] || B.lake[k]) continue;
          B.riverW[k] = 0;
          B.bridge[k] = 0;
          const Biome bio = (Biome)B.biome[k];
          B.ground[k] = (uint8_t)(bio == Biome::Swamp ? Ground::Swamp : bio == Biome::Beach ? Ground::Sand : bio == Biome::Ocean ? (Ground)B.ground[k]
                                  : groundFor((Eco)B.eco[k], Q(0.5), Q(0.5), gx, gy));
        }
    });
  }
}

// one step of the job (false: finished, and the town is cached)
bool EndlessSource::Impl::townJobStep(TownJob& J) {
  Nest nest(*this);
  auto t0 = Clock::now();
  const SitePlan& p = *J.p;
  if (J.stage == 0) {
    // the land the town is built on: base terrain with its rivers and lakes, at the town's flattened level
    SettlementCtx& ctx = J.ctx;
    ctx.plan = &p;
    ctx.kingdom = kingdom(p.kingdom);
    ctx.culture = &atlas().get(p.culture ? p.culture : cultureAt(p.ex, p.ey));   // M3
    ctx.eco = tile(p.ex, p.ey).eco;                                                // M3c
    ctx.ecoAt = [this](int32_t gx, int32_t gy) { return tile(gx, gy).eco; };      // (M7c fixer: the waste land)
    ctx.worldSeed = seed;
    ctx.rx = J.D->plan.rx; ctx.ry = J.D->plan.ry;
    // (M10) a port town or city keeps its harbour's landing clear and joins it to its streets (sea_ports.cpp)
    if ((p.flags & SPF_PORT) && isSettlement(p.type) && harbourOf(Node{p.id, p.type, p.ex, p.ey, p.seed, p.flags}, J.port) && J.port.harbour)
      ctx.harbour = &J.port;
    auto bi = J.D->bearings.find(p.id);
    if (bi != J.D->bearings.end()) ctx.roadBearings = bi->second;
    // (M2 fixer round 3) the land is made a band of rows per step (a whole city's took ~18 ms in one web frame)
    const int pad = 48, bw = p.w + 2 * pad, bh = p.h + 2 * pad;
    if (J.row == 0) J.B = std::make_shared<BaseRect>();
    if (J.row < bh) {
      const int rows = std::max(8, 6000 / std::max(1, bw));
      baseRectRows(p.gx - pad, p.gy - pad, bw, bh, *J.B, J.row, J.row + rows);
      J.row += rows;
      J.ms += msSince(t0);
      return true;
    }
    baseRectWater(*J.B);
    std::shared_ptr<BaseRect> B = J.B;
    drainDeadEndRivers(p, *B);
    const int flat = J.flat;
    const SitePlan* pp = &p;
    ctx.base = [this, B, flat, pp](int32_t gx, int32_t gy, Ground& g, Biome& b, uint8_t& h) {
      if (gx > B->x0 && gy > B->y0 && gx < B->x0 + B->w - 1 && gy < B->y0 + B->h - 1) {
        size_t i = B->at(gx, gy);
        g = (Ground)B->ground[i];
        b = (Biome)B->biome[i];
      } else {
        TileF f = tile(gx, gy);
        g = groundFor(f.eco, f.e, f.t, gx, gy);
        b = f.biome;
      }
      bool inside = gx >= pp->gx - 8 && gy >= pp->gy - 8 && gx < pp->gx + pp->w + 8 && gy < pp->gy + pp->h + 8;
      h = (uint8_t)(inside ? flat : natLevel(gx, gy));
      // the town stands on dry land: marsh pools in its footprint are drained (a river still runs through, bridged)
      if (inside && g == Ground::Lava) g = Ground::Sand;   // (M3c) nor on an ash field's lava
      if (inside && (g == Ground::Water || g == Ground::DeepWater) && b != Biome::Ocean) {
        bool river = false;
        if (gx > B->x0 && gy > B->y0 && gx < B->x0 + B->w - 1 && gy < B->y0 + B->h - 1) river = B->riverW[B->at(gx, gy)] != 0;
        if (!river) g = b == Biome::Swamp ? Ground::Swamp : b == Biome::Beach ? Ground::Sand : Ground::Grass;
      }
    };
    // what buildSettlement does before its generator runs
    *J.out = SettlementOut();
    J.out->gx = p.gx - town::MARGIN;
    J.out->gy = p.gy - town::MARGIN;
    J.gen.reset(new town::Gen(J.ctx, *J.out));
    J.stage = 1;
  } else if (J.stage == 1) {
    if (!J.gen->step()) { J.gen.reset(); J.stage = 2; }
  }
  J.ms += msSince(t0);
  if (J.stage < 2) return true;
  stats.settlements++;
  stats.settlementMs += J.ms;
  stats.maxSettlementMs = std::max(stats.maxSettlementMs, J.ms);
  if (towns.size() >= 24) {
    auto oldest = towns.begin();
    for (auto i = towns.begin(); i != towns.end(); ++i) if (i->second.used < oldest->second.used) oldest = i;
    towns.erase(oldest);
  }
  towns[J.id] = TownEntry{J.out, ++townClock};
#ifndef __EMSCRIPTEN__
  sharedTownPut(seed, J.id, J.out);
#endif
  return false;
}

// (M3 towns lane) a town another source of this process has finished goes into this source's cache too (a template
// on the source's Impl, so gen.h needs no new member)
template <class ImplT>
static bool haveTown(ImplT& S, Gid id) {
  if (S.towns.count(id)) return true;
#ifndef __EMSCRIPTEN__
  if (std::shared_ptr<SettlementOut> o = sharedTownGet(S.seed, id)) {
    if (S.towns.size() >= 24) {
      auto oldest = S.towns.begin();
      for (auto i = S.towns.begin(); i != S.towns.end(); ++i) if (i->second.used < oldest->second.used) oldest = i;
      S.towns.erase(oldest);
    }
    S.towns[id] = {std::move(o), ++S.townClock};
    return true;
  }
#endif
  return false;
}

std::shared_ptr<SettlementOut> EndlessSource::Impl::town(const SitePlan& p, std::shared_ptr<const RegionData> D) {
  haveTown(*this, p.id);
  auto it = towns.find(p.id);
  if (it != towns.end()) { it->second.used = ++townClock; return it->second.out; }
  // (a phase-at-a-time build of this very town already under way is finished rather than started over)
  std::shared_ptr<TownJob> J = townJobFor(p, std::move(D));
  while (townJobStep(*J)) {}
  if (townJob == J) townJob.reset();
  return J->out;
}

bool EndlessSource::Impl::prepareChunk(int32_t cx, int32_t cy, double budgetMs) {
  auto t0 = Clock::now();
  makeStart();
  const int32_t x0 = cx * CHUNK, y0 = cy * CHUNK;
  const int32_t rx0 = regionOf(x0), ry0 = regionOf(y0);
  std::shared_ptr<const RegionData> RD[9];
  for (int k = 0; k < 9; k++) {
    if (msSince(t0) >= budgetMs) return false;   // (one region plan is one piece of work: a few ms at most)
    RD[k] = regionData(rx0 - 1 + k % 3, ry0 - 1 + k / 3);
  }
  // (M3 fixer round 2) and the regions a lake by this chunk may ask about (lakeUnderSettlement looks ~260 tiles round
  // a lake), so chunk() never plans a region inside itself
  for (int32_t ry = regionOf(y0 - 336); ry <= regionOf(y0 + CHUNK + 336); ry++)
    for (int32_t rx = regionOf(x0 - 336); rx <= regionOf(x0 + CHUNK + 336); rx++) {
      if (regions.get(key2(rx, ry))) continue;
      if (msSince(t0) >= budgetMs) return false;
      regionData(rx, ry);
    }
  // a town left half-built by an earlier call comes first (it is wanted now, or soon will be)
  if (townJob) {
    while (msSince(t0) < budgetMs)
      if (!townJobStep(*townJob)) { townJob.reset(); break; }
    if (townJob) return false;
  }
  // the settlements chunk() asks town() for
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites) {
      if (!isSettlement(p.type)) continue;
      const int margin = 32;
      if (p.gx - margin >= x0 + CHUNK + 1 || p.gy - margin >= y0 + CHUNK + 1 || p.gx + p.w + margin <= x0 - 1 || p.gy + p.h + margin <= y0 - 1) continue;
      if (haveTown(*this, p.id)) continue;
      // (M3 fixer round 2) the town's land reads the lakes round it, and each lake asks lakeUnderSettlement, which plans
      // every region within ~260 tiles of it: up to six region plans inside one unsplittable town step (14 ms on the
      // desktop's main thread, the web's walking hitch). Plan those regions first, one budget check apiece.
      for (int32_t ry = regionOf(p.gy - 336); ry <= regionOf(p.gy + p.h + 336); ry++)
        for (int32_t rx = regionOf(p.gx - 336); rx <= regionOf(p.gx + p.w + 336); rx++) {
          if (regions.get(key2(rx, ry))) continue;
          if (msSince(t0) >= budgetMs) return false;
          regionData(rx, ry);
        }
      townJob = townJobFor(p, RD[k]);
      for (;;) {
        if (msSince(t0) >= budgetMs) return false;
        if (!townJobStep(*townJob)) { townJob.reset(); break; }
      }
    }
  return true;
}

// ------------------------------------------------------------------ site stamps
void EndlessSource::Impl::stampSite(const SitePlan& p, Stamp& S) {
  const int32_t x = p.ex, y = p.ey;
  for (int32_t yy = p.gy - 2; yy < p.gy + p.h + 2; yy++)
    for (int32_t xx = p.gx - 2; xx < p.gx + p.w + 2; xx++) S.reserve(xx, yy);
  switch (p.type) {
    case SiteType::Cave: {
      // the mouth in a real cliff face (relief): the entrance tile stays walkable, its approach clear. Where the land
      // has no face here (a start-plan cave on the flat), the mouth opens in a rocky knoll.
      // (M1) the mouth is always set into rock: where a face exists, a crag of rock rises behind it on the level above
      // (M1 round 3) the crag is an outcrop, not a box: broad at its foot, narrowing row by row to a peak that leans
      // one way or the other, each flank ragged on its own (it was an 11x5 block with straight sides)
      {
        const bool face = fineLevel(x, y - 1) > fineLevel(x, y);
        const int32_t yBot = face ? y - 1 : y;
        Rng cr(p.seed ^ 0xC4A6u);
        const int32_t hgt = 5 + cr.irange(3);
        const int lean = cr.irange(3) - 1;
        const int baseL = 5 + cr.irange(3), baseR = 5 + cr.irange(3);
        for (int32_t yy = std::max(y - 7, yBot - hgt); yy <= yBot; yy++) {
          const int32_t r = yBot - yy;                                   // rows above the foot
          const int32_t cx = x + (lean * r) / 3;
          const int jl = (int)(tileHash(p.seed ^ 0x51u, 0, yy) % 3) - 1, jr = (int)(tileHash(p.seed ^ 0x52u, 0, yy) % 3) - 1;
          const int32_t hl = std::max<int32_t>(1, baseL - (baseL - 1) * r / hgt + jl);
          const int32_t hr = std::max<int32_t>(1, baseR - (baseR - 1) * r / hgt + jr);
          for (int32_t xx = std::max(x - 8, cx - hl); xx <= std::min(x + 8, cx + hr); xx++) {
            // (M2 fixer) where the mouth is in a face, the crag stands on the level above it only: rock on the low
            // land below the face drew a second face under the first (two stacked ledges beside the mouth)
            if (face && fineLevel(xx, yy) < fineLevel(x, y - 1)) continue;
            S.g(xx, yy, Ground::Rock); S.clear(xx, yy);
          }
        }
      }
      S.g(x, y, Ground::Dirt);
      S.p(x, y, Prop::CaveEntrance);
      for (int k = -1; k <= 1; k++)
        for (int j = 1; j <= 2; j++) {
          S.clear(x + k, y + j);
          Ground gg = S.at(x + k, y + j);
          if (gg == Ground::Rock || gg == Ground::Swamp) S.g(x + k, y + j, Ground::Dirt);
        }
      if (S.in(x, y + 1) && (tileHash(p.seed, x, y) & 1) && !(p.flags & SPF_SEACAVE)) S.g(x, y + 1, Ground::Dirt);
      // (M10) a sea cave's mouth opens on the water: no bones or boulder in the sea before it
      if (!(p.flags & SPF_SEACAVE)) S.p(x + 2, y + 1, (p.seed >> 3) & 1 ? Prop::Bones : Prop::Boulder);
      break;
    }
    case SiteType::Ruin: {
      // (M1 round 3) the remains of an old hall: the door down into the vaults stands in what is left of its north
      // wall (always on the site's entrance tile, so World never has to paint a second one), broken walls run round a
      // floor of cracked flags that grass and earth are taking back, columns stand or lie snapped off in two rows, the
      // south wall has mostly fallen (the way in), and flags and rubble lie scattered outside the walls. Every wall
      // tile and column is drawn with its own broken top (the view picks a variant per tile).
      const int32_t rx = p.gx, ry = p.gy;
      const int32_t dx0 = p.ex, wy = p.ey;                  // the door, in the north wall's line
      const int32_t hx0 = rx - 1, hx1 = rx + 9, hy1 = wy + 7;  // walls: x hx0 / hx1, rows wy (north) and hy1 (south)
      const uint64_t rs = p.seed ^ 0x52u;
      for (int32_t yy = ry - 3; yy <= hy1 + 3; yy++)
        for (int32_t xx = hx0 - 2; xx <= hx1 + 2; xx++) S.reserve(xx, yy);
      // the floor and the ground under the walls
      for (int32_t yy = wy; yy <= hy1; yy++)
        for (int32_t xx = hx0; xx <= hx1; xx++) {
          S.clear(xx, yy);
          const int32_t n = vnoiseQ(xx, yy, 2, mix64(rs ^ 0x9Eu));
          const uint32_t h = (uint32_t)(tileHash(rs, xx, yy) & 255);
          const bool wallLine = xx == hx0 || xx == hx1 || yy == wy || yy == hy1;
          Ground g = Ground::StoneFloor;
          if (!wallLine) {
            if (n > 47000 && h < 200) g = Ground::Grass;
            else if (n > 40000 || h < 22) g = Ground::Dirt;
          }
          S.g(xx, yy, g);
        }
      // behind the north wall and round the outside: trodden earth and rubble fallen from the walls
      for (int32_t yy = ry - 2; yy <= hy1 + 2; yy++)
        for (int32_t xx = hx0 - 2; xx <= hx1 + 2; xx++) {
          if (xx >= hx0 && xx <= hx1 && yy >= wy && yy <= hy1) continue;
          const Ground at = S.at(xx, yy);
          if (groundSolid(at) || at == Ground::Bridge || at == Ground::Road) continue;
          const int dist = std::max({hx0 - xx, xx - hx1, wy - yy, yy - hy1, 0});
          const uint32_t h = (uint32_t)(tileHash(rs ^ 0x11u, xx, yy) & 255);
          const bool behind = yy < wy && xx >= hx0 && xx <= hx1;   // the vault's mound behind the door wall
          if (behind ? h < 170u : h < (dist <= 1 ? 120u : 50u)) { S.clear(xx, yy); S.g(xx, yy, Ground::Dirt); }
          if ((behind && h >= 200) || (!behind && h >= 236 && dist <= 2))
            S.p(xx, yy, (h & 1) ? Prop::Rock : (h & 2) ? Prop::MossRock : Prop::Boulder);
        }
      // the walls: the north one stands best, one side has fallen further than the other, the south one is mostly gone
      Rng rr(p.seed ^ 0x2B1Fu);
      const bool westWorse = rr.irange(2) != 0;
      auto rubble = [&](int32_t xx, int32_t yy, uint32_t h) {
        if (h & 64) S.p(xx, yy, (h & 1) ? Prop::Rock : (h & 2) ? Prop::MossRock : Prop::Boulder);
        else S.g(xx, yy, (h & 128) ? Ground::Dirt : Ground::StoneFloor);
      };
      for (int32_t yy = wy; yy <= hy1; yy++)
        for (int32_t xx = hx0; xx <= hx1; xx++) {
          const bool n = yy == wy, so = yy == hy1, w = xx == hx0, e = xx == hx1;
          if (!n && !so && !w && !e) continue;
          if (xx == dx0 && n) continue;                                 // the door
          const uint32_t h = (uint32_t)(tileHash(rs ^ 0x33u, xx, yy) & 255);
          if ((n || so) && (w || e)) {                                  // corners: a column, a wall end or a heap
            if (h < 150) S.p(xx, yy, Prop::RuinColumn);
            else if (h < 215) S.p(xx, yy, Prop::RuinWall);
            else rubble(xx, yy, h);
            continue;
          }
          uint32_t keep = 200;                                          // out of 256
          if (so) keep = std::abs(xx - dx0) <= 1 ? 0 : 96;              // the way in
          else if (w || e) keep = (w == westWorse) ? 90 : 185;
          else if (std::abs(xx - dx0) == 1) keep = 256;                 // the door's jambs always stand
          if (h < keep) S.p(xx, yy, Prop::RuinWall);
          else rubble(xx, yy, h);
        }
      S.g(dx0, wy, Ground::StoneFloor);
      S.p(dx0, wy, Prop::IronDoor);
      // inside: columns (some standing, some snapped off, some lying in pieces), braziers by the door
      // (M7 LAND, owner leftover "ruin entrances repeat across seeds") the hall is laid out its own way from the site's
      // seed and id (every draw from its own hash, so nothing else moves): two rows of columns (the old hall), a nave of
      // three pairs close to the aisle, pilasters engaged in the side walls, or no columns at all and a heap where the
      // roof came down; one corner fallen in for good (the walls and the floor there gone back to earth and grass); the
      // braziers by the door or a pair of statues; and what lies inside (graves, an urn and bones, a toppled statue,
      // coffins and skulls, a fallen column across the floor)
      const uint64_t ls = mix64(p.seed ^ (uint64_t)p.id * 0x9E3779B97F4A7C15ull ^ 0x7255u);
      const uint32_t L0 = (uint32_t)(ls & 0xFFFFFFFFu), L1 = (uint32_t)(ls >> 32);
      const int colPlan = (int)(L0 % 4u), corner = (int)((L0 >> 4) % 5u), dress = (int)((L0 >> 8) % 5u);
      const int32_t c0 = wy + 2 + rr.irange(2), c1 = c0 + 3;
      auto column = [&](int32_t cx, int32_t cy) {
        if (cx == dx0 || (std::abs(cx - dx0) == 1 && cy <= wy + 1)) return;   // never in the aisle to the door
        const uint32_t h = (uint32_t)(tileHash(rs ^ 0x44u, cx, cy) & 255);
        if (h < 190) S.p(cx, cy, Prop::RuinColumn);
        else S.p(cx, cy, (h & 1) ? Prop::Rock : (h & 2) ? Prop::MossRock : Prop::Boulder);
      };
      if (colPlan == 0) {
        for (int32_t cy : {c0, c1}) for (int32_t cx : {rx + 1, rx + 7}) column(cx, cy);
      } else if (colPlan == 1) {
        for (int32_t cy = wy + 2; cy <= hy1 - 2; cy += 2) for (int32_t cx : {dx0 - 2, dx0 + 2}) column(cx, cy);
      } else if (colPlan == 2) {
        for (int32_t cy = wy + 2; cy <= hy1 - 1; cy += 2) for (int32_t cx : {hx0, hx1}) column(cx, cy);
      } else {
        // the roof's fall: a heap of stone off the aisle and earth round it
        const int32_t fx = (L1 & 1) ? rx + 2 : rx + 6, fy = wy + 3 + (int32_t)((L1 >> 1) % 2u);
        for (int32_t yy = fy - 1; yy <= fy + 1; yy++)
          for (int32_t xx = fx - 1; xx <= fx + 1; xx++) {
            if (std::abs(xx - dx0) <= 1) continue;
            const uint32_t h = (uint32_t)(tileHash(rs ^ 0x45u, xx, yy) & 255);
            S.g(xx, yy, h < 128 ? Ground::Dirt : Ground::StoneFloor);
            if (h < 150) S.p(xx, yy, (h & 1) ? Prop::Rock : (h & 2) ? Prop::Rubble : Prop::Boulder);
          }
      }
      if (corner < 4) {   // a corner fallen in: its walls down, the ground taken back
        const bool east = (corner & 1) != 0, south = (corner & 2) != 0;
        const int32_t kx = east ? hx1 : hx0, ky = south ? hy1 : wy + 1;
        for (int32_t yy = ky - 2; yy <= ky + 2; yy++)
          for (int32_t xx = kx - 2; xx <= kx + 2; xx++) {
            if (yy <= wy || yy > hy1 || xx < hx0 || xx > hx1) continue;
            if (std::abs(xx - kx) + std::abs(yy - ky) > 3 || std::abs(xx - dx0) <= 1) continue;
            const uint32_t h = (uint32_t)(tileHash(rs ^ 0x46u, xx, yy) & 255);
            S.clear(xx, yy);
            S.g(xx, yy, h < 110 ? Ground::Grass : Ground::Dirt);
            if (h >= 210) S.p(xx, yy, (h & 1) ? Prop::MossRock : Prop::Rock);
          }
      }
      if (dress == 0) { S.p(dx0 - 1, wy + 1, Prop::Statue); S.p(dx0 + 1, wy + 1, Prop::Statue); }
      else { S.p(dx0 - 2, wy + 1, Prop::Brazier); S.p(dx0 + 2, wy + 1, Prop::Brazier); }
      auto put = [&](int32_t xx, int32_t yy, Prop q) {   // inside the walls, off the aisle, on a free floor tile
        if (xx <= hx0 || xx >= hx1 || yy <= wy || yy >= hy1 || std::abs(xx - dx0) <= 1) return;
        if (S.in(xx, yy) && S.c.prop[S.idx(xx, yy)]) return;
        S.p(xx, yy, q);
      };
      const int layout = rr.irange(3);
      const int pick = (layout + dress) % 5;
      if (pick == 0) { put(rx + 2 + (int32_t)(L1 % 2u) * 4, hy1 - 1, Prop::Gravestone); put(rx, c0 + 1, Prop::Urn); put(rx + 8, c1 - 1, Prop::Bones); }
      else if (pick == 1) { put(rx, c0 + 1, Prop::Gravestone); put(rx + 8, c0 + 2, Prop::Gravestone); put(rx + 2, c1 + 1, Prop::Coffin); put(rx + 6, c0 + 1, Prop::SkullPile); }
      else if (pick == 2) { put((L1 & 4) ? rx + 2 : rx + 6, c0 + 1, Prop::ToppledStatue); put((L1 & 4) ? rx + 7 : rx + 1, hy1 - 1, Prop::Urn); }
      else if (pick == 3) { for (int32_t xx = rx + 1; xx <= rx + 3; xx++) put(xx, c1, Prop::RuinColumn); put(rx + 7, c0, Prop::Bones); }
      else { put(rx + 1, hy1 - 1, Prop::Coffin); put(rx + 7, hy1 - 1, Prop::Coffin); put(rx + 4 + ((L1 & 8) ? 2 : -2), c0 + 1, Prop::SkullPile); }
      // the way from the fallen south wall to the door stays open
      for (int32_t yy = wy + 1; yy <= hy1 + 1; yy++)
        for (int32_t xx = dx0 - 1; xx <= dx0 + 1; xx++) {
          const int pr = S.in(xx, yy) ? S.c.prop[S.idx(xx, yy)] : 0;
          if (pr && propSolid((Prop)(pr - 1)) && !(yy == wy + 1 && xx != dx0)) S.clear(xx, yy);
        }
      break;
    }
    case SiteType::BanditCamp: {
      const int32_t bx = p.gx, by = p.gy;
      // (M1) a trampled clearing with a ragged edge: the dirt and the cleared ground follow noisy ovals instead of
      // the old rectangle
      for (int32_t yy = by; yy < by + 9; yy++)
        for (int32_t xx = bx; xx < bx + 11; xx++) {
          int32_t dx = 2 * (xx - (bx + 5)), dy = 2 * (yy - (by + 4));
          const int32_t d = dx * dx + (dy * dy * 169) / 100;
          const int32_t j = (int32_t)(tileHash(p.seed ^ 0xCA3Bu, xx, yy) % 28);
          const bool inner = xx > bx && yy > by && xx < bx + 10 && yy < by + 8;
          if (d < 70 + j || (inner && groundSolid(S.at(xx, yy)))) S.clear(xx, yy);
          if (d < 46 + j) S.g(xx, yy, Ground::Dirt);
          else if (inner && groundSolid(S.at(xx, yy))) S.g(xx, yy, Ground::Dirt);
        }
      S.p(bx + 5, by + 4, Prop::Campfire);
      S.p(bx + 2, by + 2, Prop::Tent); S.p(bx + 8, by + 2, Prop::Tent);
      S.p(bx + 2, by + 6, Prop::Crate); S.p(bx + 3, by + 7, Prop::Barrel);
      S.p(bx + 8, by + 6, Prop::Chest); S.p(bx + 9, by + 7, Prop::Woodpile);
      S.p(bx + 1, by + 4, Prop::Torch); S.p(bx + 9, by + 4, Prop::Torch);
      // (M10) a pirate cove: the sea's plunder about the fire (an anchor, cargo, nets) instead of the woodpile
      const bool pirate = (p.flags & SPF_PIRATE) != 0;
      if (pirate) {
        S.p(bx + 9, by + 7, Prop::Anchor); S.p(bx + 2, by + 6, Prop::CargoPile); S.p(bx + 5, by + 1, Prop::NetRack);
      }
      // the camp's bandits (the whole site's, so they arrive with any chunk the camp touches)
      Rng r(p.seed ^ 0xBA4Du);
      int n = 4 + r.irange(3);
      for (int k = 0; k < n; k++) {
        SpawnPlan s; s.siteId = p.id;
        s.sp.bandit = true; s.sp.slot = k;
        if (pirate) s.sp.role = Role::Pirate;
        s.sp.x = bx + 3 + r.irange(5); s.sp.y = by + 3 + r.irange(3);
        S.c.spawns.push_back(s);
      }
      SpawnPlan chief; chief.siteId = p.id;
      chief.sp.bandit = true; chief.sp.boss = true; chief.sp.slot = 9; chief.sp.x = bx + 6; chief.sp.y = by + 3;
      if (pirate) chief.sp.role = Role::Pirate;
      S.c.spawns.push_back(chief);
      break;
    }
    case SiteType::Shrine: {
      if (p.flags & SPF_SUMMIT) {
        // (M11) a summit shrine: a 7 x 5 floor on the top level (its flat), the shrine (3 x 2) at the back on a paved
        // court, prayer flags either side of it, a cairn by the front where the trail arrives (at x, y + 3)
        // (M11 fixer r1) the floor's bare rock or bog takes the ground of the land round it (snow among snow and ice:
        // a block of tundra on a glacier read as a pasted rectangle); the ring is read from the plan's own tiles
        Ground fill = Ground::Tundra;
        {
          int cold = 0, warm = 0;
          for (int32_t k = -1; k <= p.w; k++)
            for (int32_t yy : {p.gy - 2, p.gy + p.h + 1}) {
              const TileF f = tile(p.gx + k, yy);
              const Ground q = groundFor(f.eco, f.e, f.t, p.gx + k, yy);
              if (q == Ground::Snow || q == Ground::Ice) cold++; else if (!groundSolid(q)) warm++;
            }
          if (cold > warm) fill = Ground::Snow;
        }
        for (int32_t yy = p.gy; yy < p.gy + p.h; yy++)
          for (int32_t xx = p.gx; xx < p.gx + p.w; xx++) {
            S.clear(xx, yy);
            const Ground g0 = S.at(xx, yy);
            if (groundSolid(g0) || g0 == Ground::Swamp) S.g(xx, yy, fill);
          }
        // (M11 fixer r1) no paved court: a flat slab of paving with a ragged edge read as pasted on. The shrine's own
        // stepped dais is its foundation; before it the ground is trodden earth, the trail's last tiles running up to
        // its steps (the trail is clipped at the floor's edge)
        for (int32_t xx = x - 1; xx <= x + 1; xx++) S.g(xx, y + 1, Ground::Dirt);
        for (int32_t yy = y + 2; yy <= y + 3; yy++) S.g(x, yy, Ground::Dirt);
        for (int32_t yy = y - 1; yy <= y; yy++)
          for (int32_t xx = x - 1; xx <= x + 1; xx++) S.p(xx, yy, Prop::Filler);
        S.p(x, y, Prop::SummitShrine);
        S.p(x - 3, y - 1, Prop::PrayerFlags);
        S.p(x + 3, y - 1, Prop::PrayerFlags);
        S.p((p.seed & 2) ? x - 2 : x + 2, y + 2, Prop::Cairn);
        break;
      }
      const int32_t sx = p.gx, sy = p.gy;
      for (int32_t yy = sy; yy < sy + 5; yy++) for (int32_t xx = sx; xx < sx + 5; xx++) { S.clear(xx, yy); if (groundSolid(S.at(xx, yy))) S.g(xx, yy, Ground::Grass); }
      for (int32_t yy = sy + 1; yy < sy + 4; yy++) for (int32_t xx = sx + 1; xx < sx + 4; xx++) S.g(xx, yy, Ground::Plaza);
      S.p(sx + 2, sy + 1, Prop::Shrine);
      S.p(sx + 1, sy + 3, Prop::Flowers2); S.p(sx + 3, sy + 3, Prop::Flowers3);
      break;
    }
    case SiteType::DragonLair: {
      // a scorched eyrie on the ridge: a fire-blackened floor round an old dais, bones and rubble strewn, and crags of
      // bare rock standing round its back and sides (open to the south, where the track climbs in)
      // (M1 round 3) the scorch fades into the land's own ground (snow, tundra) along a noisy edge instead of a tan
      // blob with a hard rim, and the crags really ring the summit (the old ring lay almost wholly outside its box)
      const uint64_t ls = mix64(p.seed ^ 0x1A11u);
      for (int32_t yy = y - 7; yy <= y + 7; yy++)
        for (int32_t xx = x - 8; xx <= x + 8; xx++) {
          int32_t dx = xx - x, dy = yy - y;
          int32_t d2 = dx * dx * 100 + dy * dy * 132;
          int32_t lim = 76 + (int32_t)(tileHash(p.seed ^ 61u, xx, yy) % 15);
          if (d2 >= lim * lim) continue;
          S.clear(xx, yy);
          const uint32_t fh = (uint32_t)(tileHash(p.seed ^ 66u, xx, yy) & 15);
          const int32_t q = d2 * 100 / (lim * lim);                    // 0 at the heart .. 100 at the rim
          const int32_t n = vnoiseQ(xx, yy, 2, ls) >> 8;               // 0..255
          const bool dais = std::abs(dx) <= 2 - (dy == -6 || dy == -3 ? 1 : 0) && dy >= -6 && dy <= -3 && fh > 2;
          const Ground nat = S.at(xx, yy);
          if (dais) S.g(xx, yy, Ground::StoneFloor);
          else if (q < 30 || n * 100 > (q - 20) * 330 || groundSolid(nat) || nat == Ground::Grass || nat == Ground::Meadow) S.g(xx, yy, Ground::Dirt);
          // else: the land's own snow or tundra, swept by the scorch's edge
          if (d2 > 52 * 52 && (tileHash(p.seed ^ 62u, xx, yy) % 11) == 0) {
            const uint32_t k = (uint32_t)(tileHash(p.seed ^ 63u, xx, yy) % 5);
            S.p(xx, yy, k == 0 ? Prop::Boulder : k == 1 ? Prop::Bones : k == 2 ? Prop::SnowRock : k == 3 ? Prop::SkullPile : Prop::Rock);
          }
        }
      for (int32_t yy = y - 12; yy <= y + 4; yy++)
        for (int32_t xx = x - 13; xx <= x + 13; xx++) {
          int32_t dx = xx - x, dy = yy - y;
          int32_t d2 = dx * dx * 100 + dy * dy * 132;
          int32_t lim = 76 + (int32_t)(tileHash(p.seed ^ 61u, xx, yy) % 15);
          const int32_t outer = 100 + (vnoiseQ(xx, yy, 2, ls ^ 0x77u) >> 11);   // 100..131: the crags' ragged back
          if (d2 < (lim + 6) * (lim + 6) || d2 > outer * outer) continue;
          if (dy > 1 && std::abs(dx) < 9) continue;                              // the way in from the south
          if (dy > 3) continue;
          S.clear(xx, yy);
          S.g(xx, yy, Ground::Rock);
        }
      {
        Rng lr(p.seed ^ 0x1A12u);
        const int32_t ax = x, sp = 2 + lr.irange(2);
        const int32_t skY = y - 2 + lr.irange(2);   // (draws sequenced: the right argument's first, as MSVC/GCC)
        const int32_t skX = x - 3 - lr.irange(2);
        S.p(skX, skY, Prop::SkullPile); S.p(x + 3 + lr.irange(2), y + 1, Prop::Bones); S.p(x - 5, y + 2 + lr.irange(2), Prop::Bones);
        S.p(ax + sp, y - 4, Prop::Brazier); S.p(ax - sp, y - 4, Prop::Brazier); S.p(ax, y - 5, Prop::Altar);
      }
      break;
    }
    case SiteType::Vignette: stampVignette(p, S); break;   // (M2, vignettes.cpp)
    case SiteType::Wonder: stampWonder(p, S); break;
    default: break;
  }
}

void EndlessSource::Impl::stampDen(const DenPlan& d, Stamp& S) {
  Rng r(mix64(seed ^ d.id));
  for (int oy = -3; oy <= 3; oy++) for (int ox = -3; ox <= 3; ox++) if (ox * ox + oy * oy <= 10) S.reserve(d.x + ox, d.y + oy);
  // (M10) a merrow den lies in the sea by a wreck: no lair props (the wreck is their hoard)
  if (d.mon == Monster::Merrow) return;
  Biome b = S.in(d.x, d.y) ? (Biome)S.c.biome[S.idx(d.x, d.y)] : tile(d.x, d.y).biome;
  bool snowy = b == Biome::Snow;
  Prop rock = snowy ? Prop::SnowRock : (b == Biome::Forest || b == Biome::Swamp || b == Biome::Taiga) ? Prop::MossRock : Prop::Boulder;
  std::vector<Prop> big, small;
  switch (d.mon) {
    case Monster::Wolf: case Monster::IceWolf: big = {rock, Prop::Boulder, Prop::DeadTree}; small = {Prop::Bones, Prop::SkullPile, Prop::Bones}; break;
    case Monster::Goblin: big = {Prop::Tent, Prop::Crate, Prop::Woodpile}; small = {Prop::Bones, Prop::Barrel}; break;
    case Monster::Skeleton: big = {Prop::Gravestone, Prop::Gravestone, Prop::DeadTree}; small = {Prop::SkullPile, Prop::Bones, Prop::Bones}; break;
    case Monster::Spider: case Monster::FrostSpider: big = {Prop::DeadTree, rock}; small = {Prop::Cobweb, Prop::Cobweb, Prop::Bones}; break;
    case Monster::Troll: big = {rock, Prop::Boulder, rock}; small = {Prop::SkullPile, Prop::Bones, Prop::Bones}; break;
    default: big = {rock, Prop::Boulder, Prop::Log}; small = {Prop::Bones, Prop::Bones}; break;
  }
  if (d.mon == Monster::Goblin) S.p(d.x, d.y, Prop::Campfire);
  static const int chestAt[3][2] = {{0, -2}, {1, -2}, {-1, -2}};
  const int* ch = chestAt[r.irange(3)];
  S.p(d.x + ch[0], d.y + ch[1], Prop::Chest);
  static const int bigAt[3][2] = {{-2, -2}, {2, -1}, {-3, 0}};
  for (size_t k = 0; k < big.size() && k < 3; k++) S.p(d.x + bigAt[k][0], d.y + bigAt[k][1], big[k]);
  static const int smallAt[4][2] = {{1, 1}, {-1, 1}, {2, 2}, {-2, 1}};
  for (int k = 0; k < 4; k++) if (r.next() % 10 < 7) S.p(d.x + smallAt[k][0], d.y + smallAt[k][1], small[(size_t)r.irange((int)small.size())]);
}

// the apron's eco at (x, y) for land of family fam, or Eco::COUNT outside every apron
Eco ChunkCtx::apronEco(int32_t x, int32_t y, Biome fam) const {
  if (aprons.empty() || (fam != Biome::Plains && fam != Biome::Forest && fam != Biome::Autumn)) return Eco::COUNT;
  for (const DuneApron& a : aprons) {
    const int64_t dx = x - a.cx, dy = y - a.cy;
    if (std::abs(dx) > a.rx + 6 || std::abs(dy) > a.ry + 6) continue;
    const int64_t d = dx * dx * 1024 / ((int64_t)a.rx * a.rx) + dy * dy * 1024 / ((int64_t)a.ry * a.ry);
    const int64_t T = 800 + (int64_t)fbmQ(x, y, 3, 2, apronSeed) * 480 / 65536;   // a ragged edge (0.80 .. 1.27)
    if (d < T) return d < T * 2 / 5 ? Eco::Dunes : Eco::Scrubland;
  }
  return Eco::COUNT;
}

// ------------------------------------------------------------------ the chunk
void EndlessSource::Impl::chunk(int32_t cx, int32_t cy, ChunkData& c) {
  makeStart();
  auto t0 = Clock::now();
  const double nested0 = nestedMs;
  c = ChunkData();
  c.cx = cx; c.cy = cy;
  // (M10) the open sea: deep water through and through (oceanChunk: every coarse sample the tiles read lies well
  // below the deep-water line, and nothing of the 3x3 regions round it reaches it), made without the land's pipeline
  if (oceanFast && oceanChunk(cx, cy)) {
    for (int i = 0; i < ChunkData::N; i++) {
      c.ground[i] = (uint8_t)Ground::DeepWater;
      c.biome[i] = (uint8_t)Biome::Ocean;
      c.eco[i] = c.ecoNb[i] = (uint8_t)Eco::Ocean;
    }
  } else {
    // every other chunk: the land's pipeline, its stages in a fixed order (each reads only plans, pure functions and
    // what the stages before it wrote into the context)
    ChunkCtx X(c, cx, cy);
    chunkBase(X);         // 1-2: base land, rivers, lakes; the dune aprons
    chunkTowns(X);        // 3: settlement buffers
    chunkHarbours(X);     // 3b: the harbours
    chunkStamps(X);       // 4: site and den stamps
    chunkRoads(X);        // 5: roads and tracks (chunk_roads.cpp)
    chunkLevels(X);       // 6: relief, the level grid (chunk_relief.cpp)
    chunkRiverBeds(X);    //    the rivers' beds
    chunkFlats(X);        //    the places' flats and terraces
    chunkReliefBits(X);   //    the cliff, ramp, stair, slope and fall bits
    chunkEcotones(X);     // 6b: ecotones
    chunkBoardwalks(X);   // 6a: the marsh boardwalks
    chunkMassifs(X);      // 6b: massifs
    chunkPeaks(X);        // 6c: peaks
    chunkCairns(X);       // 6d: cairns
    chunkFlora(X);        // 7: vegetation (chunk_flora.cpp)
  }
  const double ms = msSince(t0) - (nestedMs - nested0);
  stats.chunks++;
  stats.chunkMs += ms;
  stats.maxChunkMs = std::max(stats.maxChunkMs, ms);
}

// 1-2: the base land, rivers and lakes (with a 2-tile border), the masks, the regions round the chunk and
// (M3c fixer round 2) the dune aprons
void EndlessSource::Impl::chunkBase(ChunkCtx& X) {
  ChunkData& c = X.c;
  const int32_t x0 = X.x0;
  const int32_t y0 = X.y0;
  BaseRect& B = X.B;
  auto apronEco = [&X](int32_t x, int32_t y, Biome fam) { return X.apronEco(x, y, fam); };
  // 1-2: base land, rivers, lakes (with a 2-tile border)
  baseRect(x0 - BR, y0 - BR, BW, BW, B);
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) {
      size_t bi = (size_t)(ly + BR) * BW + (lx + BR);
      c.ground[c.at(lx, ly)] = B.ground[bi];
      c.biome[c.at(lx, ly)] = B.biome[bi];
      c.eco[c.at(lx, ly)] = c.ecoNb[c.at(lx, ly)] = B.eco[bi];
    }
  X.reserved.assign((size_t)ChunkData::N, 0);
  X.used.assign(RW * RW, 0);
  X.road.assign(RW * RW, 0);
  X.entrance.assign(RW * RW, 0);
  X.trailT.assign(RW * RW, 0);   // (M11 fixer r2) a summit trail's tiles: its ways up are stairs
  // (M3 towns lane) a terraced town's own relief (its height byte + 1; 0: none): it cut its terraces, faces and
  // stairs itself, so its tiles keep them (its outer band is the plan's flat level, which the land meets); every other
  // town keeps the relief made here
  X.townH.assign(RW * RW, 0);
  const int32_t rx0 = regionOf(x0), ry0 = regionOf(y0);
  std::shared_ptr<const RegionData>* RD = X.RD;
  for (int k = 0; k < 9; k++) RD[k] = regionData(rx0 - 1 + k % 3, ry0 - 1 + k / 3);
  // (M3c fixer round 2, review: "a dune capital stands in green oak woodland while the HUD says The Dune Sea") the dune
  // folk carry their desert with them: round a dune-culture settlement that stands in green land (plains, forest, the
  // autumn woods), the land within about 12 tiles of its outline, with a ragged edge, is THEIR land: open sand dunes
  // near the walls, rocky scrubland further out, meeting the woods through the ordinary ecotone. The base layers change
  // before anything reads them (the flora, the ecotones, the HUD), so the trees round the town are the desert's.
  // A pure function of the tile and the site plans (every chunk computes the same answer); water, rock, marsh, snow
  // and coasts are left as they are.
  using DuneApron = ChunkCtx::DuneApron;
  std::vector<DuneApron>& aprons = X.aprons;
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites) {
      if (!isSettlement(p.type) || !p.culture) continue;
      const int32_t ax = p.gx + p.w / 2, ay = p.gy + p.h / 2, arx = p.w / 2 + 12, ary = p.h / 2 + 12;
      if (ax + arx + 6 < x0 - 8 || ax - arx - 6 >= x0 + CHUNK + 8 || ay + ary + 6 < y0 - 8 || ay - ary - 6 >= y0 + CHUNK + 8) continue;
      if (atlas().get(p.culture).archetype != cult::Archetype::Dune) continue;
      const Biome hb = tile(p.ex, p.ey).biome;
      if (hb == Biome::Snow || hb == Biome::Taiga) continue;   // a dune town on the snow is a snow town (town_layout)
      aprons.push_back(DuneApron{ax, ay, std::max<int32_t>(1, arx), std::max<int32_t>(1, ary)});
    }
  X.apronSeed = mix64(seed ^ tag("dune.apron"));
  if (!aprons.empty()) {
    for (int32_t y = B.y0; y < B.y0 + B.h; y++)
      for (int32_t x = B.x0; x < B.x0 + B.w; x++) {
        const size_t bi = B.at(x, y);
        const Ground g = (Ground)B.ground[bi];
        if (g != Ground::Grass && g != Ground::Meadow && g != Ground::ForestFloor && g != Ground::Autumn) continue;
        const Eco e = apronEco(x, y, (Biome)B.biome[bi]);
        if (e == Eco::COUNT) continue;
        B.eco[bi] = (uint8_t)e;
        B.biome[bi] = (uint8_t)Biome::Desert;
        B.ground[bi] = (uint8_t)Ground::Sand;
      }
    for (int ly = 0; ly < CHUNK; ly++)
      for (int lx = 0; lx < CHUNK; lx++) {
        size_t bi = (size_t)(ly + BR) * BW + (lx + BR);
        c.ground[c.at(lx, ly)] = B.ground[bi];
        c.biome[c.at(lx, ly)] = B.biome[bi];
        c.eco[c.at(lx, ly)] = c.ecoNb[c.at(lx, ly)] = B.eco[bi];
      }
  }
}

// 3: the settlement buffers near the chunk (their ground, props, walls and records)
void EndlessSource::Impl::chunkTowns(ChunkCtx& X) {
  ChunkData& c = X.c;
  const int32_t x0 = X.x0;
  const int32_t y0 = X.y0;
  std::vector<uint8_t>& reserved = X.reserved;
  std::vector<uint8_t>& used = X.used;
  std::vector<uint8_t>& townH = X.townH;
  Stamp& S = X.S;
  const std::shared_ptr<const RegionData>* RD = X.RD;
  auto ri = [&X](int32_t gx, int32_t gy) { return X.ri(gx, gy); };
  // 3: settlement buffers
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites) {
      if (!isSettlement(p.type)) continue;
      const int margin = 32;
      if (p.gx - margin >= x0 + CHUNK + 1 || p.gy - margin >= y0 + CHUNK + 1 || p.gx + p.w + margin <= x0 - 1 || p.gy + p.h + margin <= y0 - 1) continue;
      std::shared_ptr<SettlementOut> T = town(p, RD[k]);
      const Map& b = T->buf;
      // (M3) a terraced town (the only kind with cliff faces of its own) brings its relief along
      bool terraced = false;
      for (size_t q = 0; q < b.height.size() && !terraced; q++) terraced = (b.height[q] & Map::HEIGHT_CLIFF) != 0;
      // (M3c fixer, review: "a near-black hole inside a brick capital") a yard or garden inside a town that lies on a
      // harsh land (a bog, the dark forest, the blight, the ash...) is laid as the town's civic ground: the eco and
      // family of the town's heart, or that family's classic land where the heart itself is harsh
      auto harsh = [](Eco e) {
        switch (e) {
          case Eco::PeatBog: case Eco::TaigaBog: case Eco::ReedMarsh: case Eco::Mangrove: case Eco::FloodedForest: case Eco::DarkForest:
          case Eco::Blight: case Eco::AshFields: case Eco::CrystalBarrens: case Eco::MushroomForest: case Eco::PetrifiedForest:
          case Eco::SaltFlats: case Eco::Glacier: return true;
          default: return false;
        }
      };
      Biome civB = Biome::Plains;
      Eco civE = Eco::COUNT;
      {
        const int hx = std::clamp(p.gx + p.w / 2 - T->gx, 0, b.w - 1), hy = std::clamp(p.gy + p.h / 2 - T->gy, 0, b.h - 1);
        if (!b.biome.empty()) civB = (Biome)b.biome[(size_t)hy * b.w + hx];
        if (!b.eco.empty()) civE = (Eco)b.eco[(size_t)hy * b.w + hx];
        if (civB == Biome::Swamp || civB == Biome::Ocean || civB == Biome::Mountain || civB == Biome::Beach) civB = Biome::Plains;
        if (civE == Eco::COUNT || harsh(civE) || ecoFamily(civE) != civB) civE = ecoOfFamily(civB);
      }
      for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
        for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++) {
          int32_t bx = gx - T->gx, by = gy - T->gy;
          if (!b.in(bx, by)) continue;
          size_t bi = (size_t)by * b.w + bx;
          if (!T->used.empty() && !T->used[bi]) continue;   // outside the town: the wild land stays
          used[ri(gx, gy)] = 1;
          if (terraced) townH[ri(gx, gy)] = (uint8_t)(b.height[bi] + 1);
          if (!S.in(gx, gy)) continue;
          int i = S.idx(gx, gy);
          c.ground[i] = b.ground[bi];
          c.prop[i] = b.prop[bi];
          c.wall[i] = b.wall[bi];
          if (!b.biome.empty()) c.biome[i] = b.biome[bi];
          // (M3c) a town buffer with its own eco layer brings it; else the land's eco stays, unless the town changed
          // the tile's family (then that family's classic eco)
          if (!b.eco.empty()) c.eco[i] = c.ecoNb[i] = b.eco[bi];
          else if (ecoFamily((Eco)c.eco[i]) != (Biome)c.biome[i]) c.eco[i] = c.ecoNb[i] = (uint8_t)ecoOfFamily((Biome)c.biome[i]);
          if (harsh((Eco)c.eco[i]) && !groundWater((Ground)c.ground[i])) {
            c.eco[i] = c.ecoNb[i] = (uint8_t)civE;
            c.biome[i] = (uint8_t)civB;
            if ((Ground)c.ground[i] == Ground::Swamp) c.ground[i] = (uint8_t)Ground::Grass;
          }
          if (!b.blend.empty()) c.blend[i] = b.blend[bi];   // (M3 fixer) the paving / boardwalk mark (Map::PAVE_MARK)
          if (!b.urb.empty()) c.urb[i] = b.urb[bi];         // (M7b) the urban fabric (rpg/world/urban.h)
          reserved[(size_t)i] = 1;
        }
      if (p.gx - 24 >= x0 + CHUNK || p.gy - 24 >= y0 + CHUNK || p.gx + p.w + 24 <= x0 || p.gy + p.h + 24 <= y0) continue;
      // the whole site's records travel with every chunk it touches, so a site's buildings arrive together
      for (const Bldg& bl : b.bldgs) {
        Bldg g = bl;
        g.r.x += T->gx; g.r.y += T->gy;
        g.site = -1;
        c.bldgs.push_back(g);
        c.bldgSite.push_back(p.id);
        int bidx = (int)c.bldgs.size();
        for (int yy = g.r.y; yy < g.r.y + g.r.h; yy++)
          for (int xx = g.r.x; xx < g.r.x + g.r.w; xx++)
            if (S.in(xx, yy)) c.bldg[S.idx(xx, yy)] = (uint16_t)bidx;
      }
      for (const Spawn& sp0 : b.spawns) {
        SpawnPlan sp;
        sp.sp = sp0;
        sp.sp.x += T->gx; sp.sp.y += T->gy;
        sp.siteId = p.id;
        c.spawns.push_back(sp);
      }
      if (p.flags & SPF_PORT) {   // (M10) the harbour's keepers travel with the town's records
        PortPlan PP;
        if (harbourOf(Node{p.id, p.type, p.ex, p.ey, p.seed, p.flags}, PP)) stampHarbour(PP, T.get(), S, &c.spawns);
      }
      for (auto& gt : T->gates) c.gates.push_back(GTile{gt.first + T->gx, gt.second + T->gy});
      for (IRect r : T->wallGaps) { r.x += T->gx; r.y += T->gy; c.wallGaps.push_back(r); }
      for (const PlotPlan& lp : T->plots) c.plots.push_back(lp);   // (M7) its lots for sale (global tiles)
    }
}

// 3b: the harbours
void EndlessSource::Impl::chunkHarbours(ChunkCtx& X) {
  const int32_t x0 = X.x0;
  const int32_t y0 = X.y0;
  Stamp& S = X.S;
  const std::shared_ptr<const RegionData>* RD = X.RD;
  // 3b (M10): the harbours (sea_ports.cpp): the dredged basin and channel, the mole and its T-head, the dress
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites) {
      if (!(p.flags & SPF_PORT) || !isSettlement(p.type)) continue;
      PortPlan PP;
      if (!harbourOf(Node{p.id, p.type, p.ex, p.ey, p.seed, p.flags}, PP) || !PP.harbour) continue;
      const int32_t reach = PP.pierLen + 12 + harbourDredgeLen(PP);
      if (PP.rootX + reach < x0 - 4 || PP.rootX - reach >= x0 + CHUNK + 4 || PP.rootY + reach < y0 - 4 || PP.rootY - reach >= y0 + CHUNK + 4) continue;
      const int margin = 32;
      const bool nearTown = !(p.gx - margin >= x0 + CHUNK + 1 || p.gy - margin >= y0 + CHUNK + 1 || p.gx + p.w + margin <= x0 - 1 || p.gy + p.h + margin <= y0 - 1);
      std::shared_ptr<SettlementOut> T = nearTown ? town(p, RD[k]) : nullptr;
      stampHarbour(PP, T.get(), S, nullptr);
    }
}

// 4: the site and den stamps
void EndlessSource::Impl::chunkStamps(ChunkCtx& X) {
  const int32_t x0 = X.x0;
  const int32_t y0 = X.y0;
  std::vector<uint8_t>& entrance = X.entrance;
  Stamp& S = X.S;
  const std::shared_ptr<const RegionData>* RD = X.RD;
  auto rin = [&X](int32_t gx, int32_t gy) { return X.rin(gx, gy); };
  auto ri = [&X](int32_t gx, int32_t gy) { return X.ri(gx, gy); };
  // 4: site and den stamps
  for (int k = 0; k < 9; k++) {
    for (const SitePlan& p : RD[k]->plan.sites) {
      if (isSettlement(p.type)) continue;
      if (p.type == SiteType::Cave && rin(p.ex, p.ey)) entrance[ri(p.ex, p.ey)] = 1;
      if (p.gx - 6 >= x0 + CHUNK || p.gy - 6 >= y0 + CHUNK || p.gx + p.w + 6 <= x0 || p.gy + p.h + 6 <= y0) continue;
      stampSite(p, S);
    }
    for (const DenPlan& d : RD[k]->plan.dens) {
      if (d.x + 5 < x0 || d.y + 5 < y0 || d.x - 5 >= x0 + CHUNK || d.y - 5 >= y0 + CHUNK) continue;
      stampDen(d, S);
    }
  }
}

// 6b: the ecotones
void EndlessSource::Impl::chunkEcotones(ChunkCtx& X) {
  ChunkData& c = X.c;
  const int32_t x0 = X.x0;
  const int32_t y0 = X.y0;
  const BaseRect& B = X.B;
  std::vector<uint8_t>& used = X.used;
  auto ri = [&X](int32_t gx, int32_t gy) { return X.ri(gx, gy); };
  auto apronEco = [&X](int32_t x, int32_t y, Biome fam) { return X.apronEco(x, y, fam); };
  // 6b (M2): ecotones (VISION_PLAN 11.6). Along a border between two land biomes, a band 4 to 8 tiles wide (by the
  // pair) carries the other biome and its weight in Map::blend: bits 0-3 the other biome, 4-7 its weight, 4 at the
  // border falling to 0 at the band's edge, so both sides meet half and half. Read from the eco field on a grid 4
  // tiles wider than the chunk (the base rect's ecos, then biomeLite on the outer ring), so chunk seams agree. The
  // view dithers the ground with it; the vegetation takes the other side's flora at the same odds.
  // (M3c) Between ANY two ecos, also two of one family (meadow and prairie, birch wood and the mixed forest): ecoNb is
  // the partner eco, the blend's family nibble its family (the tile's own family for a same-family pair), and the
  // band's width is the pair's (ecoHalfW: wide and soft between grasslands, narrow at the marsh edges).
  {
    constexpr int EB = 4, EW = CHUNK + 2 * EB;
    // (a chunk whose base rect holds one eco has no border within 2 tiles of it: only the outermost tiles of a
    // neighbour's band would reach in, at weight 1, so the ring is not worth building)
    bool mixed = false;
    for (size_t k = 1; k < B.eco.size() && !mixed; k++) if (B.eco[k] != B.eco[0]) mixed = true;
    if (mixed) {
      std::vector<uint8_t> ee((size_t)EW * EW);
      for (int y = 0; y < EW; y++)
        for (int x = 0; x < EW; x++) {
          const int32_t gx = x0 - EB + x, gy = y0 - EB + y;
          const size_t k = (size_t)y * EW + x;
          if (B.in(gx, gy)) ee[k] = B.eco[B.at(gx, gy)];
          else {
            Eco e = Eco::Meadow;
            biomeLite(gx, gy, &e);
            const Eco ae = apronEco(gx, gy, ecoFamily(e));   // (fixer r2) a dune town's apron, as the base rect has it
            ee[k] = (uint8_t)(ae != Eco::COUNT ? ae : e);
          }
        }
      // land ecos that blend (the sea, the beaches and bare mountain rock keep their hard edges, as before)
      auto blends = [](uint8_t v) {
        const Biome q = ecoFamily((Eco)v);
        return q == Biome::Plains || q == Biome::Forest || q == Biome::Autumn || q == Biome::Taiga || q == Biome::Snow || q == Biome::Swamp || q == Biome::Desert;
      };
      // the disc of offsets nearest first (built once; a function-local static is initialised thread-safely)
      struct Off { int dx, dy, d2; };
      static const std::vector<Off> offs = [] {
        std::vector<Off> v;
        for (int dy = -EB; dy <= EB; dy++)
          for (int dx = -EB; dx <= EB; dx++)
            if ((dx || dy) && dx * dx + dy * dy <= EB * EB) v.push_back(Off{dx, dy, dx * dx + dy * dy});
        std::stable_sort(v.begin(), v.end(), [](const Off& a, const Off& b) { return a.d2 < b.d2; });
        return v;
      }();
      for (int ly = 0; ly < CHUNK; ly++)
        for (int lx = 0; lx < CHUNK; lx++) {
          const uint8_t self = ee[(size_t)(ly + EB) * EW + lx + EB];
          if (!blends(self)) continue;
          if (used[ri(x0 + lx, y0 + ly)]) continue;   // town ground keeps its own look
          for (const Off& of : offs) {
            const uint8_t o = ee[(size_t)(ly + EB + of.dy) * EW + lx + EB + of.dx];
            if (o == self || !blends(o)) continue;
            const int hw = ecoHalfW((Eco)self, (Eco)o);
            const int32_t dd = (int32_t)isqrt((uint64_t)of.d2 * 100);   // tenths of a tile
            const int32_t w = (4 * (hw * 10 - dd + 10) + hw * 5) / (hw * 10);   // 4 beside the border .. 0 past hw
            if (w > 0) {
              c.blend[c.at(lx, ly)] = (uint8_t)((std::min<int32_t>(w, 8) << 4) | ((int)ecoFamily((Eco)o) & 15));
              c.ecoNb[c.at(lx, ly)] = o;
            }
            break;
          }
        }
    }
  }
}

// 6a: the marsh boardwalks
void EndlessSource::Impl::chunkBoardwalks(ChunkCtx& X) {
  ChunkData& c = X.c;
  const int32_t x0 = X.x0;
  const int32_t y0 = X.y0;
  const BaseRect& B = X.B;
  std::vector<uint8_t>& reserved = X.reserved;
  std::vector<uint8_t>& used = X.used;
  std::vector<uint8_t>& road = X.road;
  auto rin = [&X](int32_t gx, int32_t gy) { return X.rin(gx, gy); };
  auto ri = [&X](int32_t gx, int32_t gy) { return X.ri(gx, gy); };
  // 6a (M4, owner carry-over: the marsh boardwalks): a road or track over the open water of a marsh (the Swamp family's
  // pools, not a river) is a boardwalk on posts, not a river bridge: its tiles carry Map::BOARDWALK_MARK with the
  // marsh's plank paving (4), so the view draws them as the stilt towns' walks and joins them seamlessly. Where the road
  // steps diagonally, the inner corner of each step on its south side is decked too (water with the walk to its north,
  // to one side and on the diagonal between), so the staircase is a walk two tiles wide that the view lays as one
  // straight diagonal deck with every part of it walkable; a turn gets its inner corner the same way. Decided from the
  // plan's roads and the base water only (the road array and the base rect cover the chunk's 1-tile margin), so the
  // chunks on either side of an edge agree.
  {
    auto marshWater = [&](int32_t gx, int32_t gy) {
      if (!B.in(gx, gy)) return false;
      const size_t b = B.at(gx, gy);
      const Ground g0 = (Ground)B.ground[b];
      return (g0 == Ground::Water || g0 == Ground::DeepWater) && ecoFamily((Eco)B.eco[b]) == Biome::Swamp && B.riverW[b] == 0 && !B.bridge[b];
    };
    auto deck = [&](int32_t gx, int32_t gy) { return rin(gx, gy) && road[ri(gx, gy)] && !used[ri(gx, gy)] && marshWater(gx, gy); };
    const uint8_t mark = (uint8_t)(Map::BOARDWALK_MARK | 4);
    for (int ly = 0; ly < CHUNK; ly++)
      for (int lx = 0; lx < CHUNK; lx++) {
        const int32_t gx = x0 + lx, gy = y0 + ly;
        const int i = c.at(lx, ly);
        if (deck(gx, gy)) {
          if (c.ground[i] != (uint8_t)Ground::Bridge) continue;
          c.blend[i] = mark;
          c.ecoNb[i] = c.eco[i];
          continue;
        }
        if (!rin(gx, gy) || road[ri(gx, gy)] || used[ri(gx, gy)] || !marshWater(gx, gy)) continue;
        if (!groundWater((Ground)c.ground[i])) continue;
        const bool fill = deck(gx, gy - 1) && ((deck(gx + 1, gy) && deck(gx + 1, gy - 1)) || (deck(gx - 1, gy) && deck(gx - 1, gy - 1)));
        if (!fill) continue;
        c.ground[i] = (uint8_t)Ground::Bridge;
        c.prop[i] = 0;
        c.blend[i] = mark;
        c.ecoNb[i] = c.eco[i];
        reserved[(size_t)i] = 1;
      }
  }
}

// 6b: the massifs
void EndlessSource::Impl::chunkMassifs(ChunkCtx& X) {
  ChunkData& c = X.c;
  const int32_t cx = X.cx;
  const int32_t cy = X.cy;
  const int32_t x0 = X.x0;
  const int32_t y0 = X.y0;
  std::vector<uint8_t>& reserved = X.reserved;
  std::vector<uint8_t>& used = X.used;
  std::vector<uint8_t>& road = X.road;
  const std::shared_ptr<const RegionData>* RD = X.RD;
  auto rin = [&X](int32_t gx, int32_t gy) { return X.rin(gx, gy); };
  auto ri = [&X](int32_t gx, int32_t gy) { return X.ri(gx, gy); };
  // 6b (M2 fixer round 2): massifs. Every named summit (a Peak landmark: the region's highest point) and every range's
  // labelled crest stands as a mountain you can see: a GreatPeak (7 x 3) at the label point with lesser spires on its
  // flanks and behind it (a range's crest gets a longer chain). The layout is a pure function of the label point; the
  // label point is first nudged so the great peak's footprint lies inside one chunk; each spire is placed by the chunk
  // that holds its whole footprint, and only on level, dry, open ground (a spire that does not fit is left out, and
  // the great peak tries a few nearby spots in a fixed order).
  {
    struct Spire { int dx, dy; bool great; };
    static const Spire kSummit[] = {{0, 0, true}, {-6, 1, false}, {6, 0, false}, {-4, -3, false}, {4, -4, false}};
    static const Spire kRange[] = {{0, 0, true}, {-6, 1, false}, {6, 0, false}, {-4, -3, false}, {4, -4, false},
                                   {-10, -1, false}, {10, -2, false}, {-13, 1, false}, {13, 1, false}};
    // (M11 fixer r1) a summit shrine's floor and the ground before it (where its trail arrives) stay open: a spire
    // stood right in front of the shrine, hiding the last of the climb
    std::vector<std::array<int32_t, 4>> summitKeep;
    for (int k = 0; k < 9; k++)
      for (const SitePlan& sp0 : RD[k]->plan.sites)
        if (sp0.type == SiteType::Shrine && (sp0.flags & SPF_SUMMIT))
          summitKeep.push_back({sp0.gx - 3, sp0.gy - 1, sp0.gx + sp0.w + 3, sp0.gy + sp0.h + 6});
    auto fits = [&](int32_t ax, int32_t ay, Prop p, bool strict) {
      int fw = 1, fh = 1;
      art::wildFootprint(p, fw, fh);
      const int lx0 = ax - fw / 2 - x0, lx1 = ax + fw / 2 - x0, ly0 = ay - fh + 1 - y0, ly1 = ay - y0;
      if (lx0 < 0 || ly0 < 0 || lx1 >= CHUNK || ly1 >= CHUNK) return false;
      for (const auto& r : summitKeep)
        if (ax + fw / 2 >= r[0] && ax - fw / 2 < r[2] && ay >= r[1] && ay - fh + 1 < r[3]) return false;
      const int lv = c.height[c.at(ax - x0, ay - y0)] & Map::HEIGHT_LEVEL;
      for (int ly = ly0; ly <= ly1; ly++)
        for (int lx = lx0; lx <= lx1; lx++) {
          const int ii = c.at(lx, ly);
          const int32_t gx = x0 + lx, gy = y0 + ly;
          const uint8_t hb = c.height[ii];
          if ((hb & Map::HEIGHT_STEP) || (hb & Map::HEIGHT_LEVEL) != lv) return false;
          if (reserved[(size_t)ii] || c.prop[ii] || c.wall[ii] || c.bldg[ii] || used[ri(gx, gy)] || road[ri(gx, gy)]) return false;
          const Ground gg = (Ground)c.ground[ii];
          if (groundSolid(gg) || waterG(c.ground[ii]) || gg == Ground::Bridge || gg == Ground::Road || gg == Ground::Dirt || gg == Ground::Plaza ||
              gg == Ground::Farmland)
            return false;
        }
      if (strict)   // the way over the range (a road) stays open: no spire within 3 tiles of one
        for (int ly = ly0 - 3; ly <= ly1 + 3; ly++)
          for (int lx = lx0 - 3; lx <= lx1 + 3; lx++) {
            const int32_t gx = x0 + lx, gy = y0 + ly;
            if (rin(gx, gy) && road[ri(gx, gy)]) return false;
          }
      return true;
    };
    auto place = [&](int32_t ax, int32_t ay, Prop p) {
      int fw = 1, fh = 1;
      art::wildFootprint(p, fw, fh);
      for (int32_t y = ay - fh + 1; y <= ay; y++)
        for (int32_t x = ax - fw / 2; x <= ax + fw / 2; x++) {
          const int ii = c.at(x - x0, y - y0);
          c.prop[ii] = (uint8_t)((int)(x == ax && y == ay ? p : Prop::Filler) + 1);
          reserved[(size_t)ii] = 1;
        }
    };
    for (int k = 0; k < 9; k++)
      for (const LandmarkPlan& l : RD[k]->plan.landmarks) {
        const bool range = l.kind == LandmarkKind::Range;
        if (!range && !(l.kind == LandmarkKind::Peak && l.size == 60)) continue;   // (a wonder's label is a Peak of size 24)
        if (l.x < x0 - 24 || l.x >= x0 + CHUNK + 24 || l.y < y0 - 12 || l.y >= y0 + CHUNK + 12) continue;
        // the great peak's footprint inside one chunk: x - 3 .. x + 3, y - 2 .. y
        int32_t gx = l.x, gy = l.y + 2;
        const int32_t mx = floorMod(gx, CHUNK), my = floorMod(gy, CHUNK);
        if (mx < 3) gx += 3 - mx; else if (mx > CHUNK - 4) gx -= mx - (CHUNK - 4);
        if (my < 2) gy += 2 - my;
        // a few spots round it in a fixed order: the first that fits (judged by the chunk that holds it) carries the
        // massif. Every chunk walks the same order, and a spot in another chunk is that chunk's to judge: this chunk
        // only places what lies inside it, so a massif is never doubled (the spots are a chunk's width apart or inside
        // the same chunk: the nudge keeps them all in the great peak's own chunk).
        static const int kTry[][2] = {{0, 0}, {-2, 0}, {2, 0}, {0, 2}, {-2, 2}, {2, 2}, {0, -2}};
        const int32_t cxg = floorDiv(gx, CHUNK), cyg = floorDiv(gy, CHUNK);
        if (cxg != cx || cyg != cy) {
          // the great peak's chunk is another one: only the spires that fall in this chunk, around the spot that
          // chunk would choose. Without its data the choice is not knowable here, so the flanks are laid only round the
          // first spot (a pure choice), and only where they fit.
          const Spire* sp = range ? kRange : kSummit;
          const int ns = range ? (int)(sizeof(kRange) / sizeof(kRange[0])) : (int)(sizeof(kSummit) / sizeof(kSummit[0]));
          for (int s = 1; s < ns; s++) {
            const int32_t ax = gx + sp[s].dx, ay = gy + sp[s].dy;
            if (fits(ax, ay, Prop::Peak, true)) place(ax, ay, Prop::Peak);
          }
          continue;
        }
        int32_t bx = INT32_MIN, by = 0;
        for (auto& t : kTry) {
          const int32_t tx = gx + t[0], ty = gy + t[1];
          if (floorDiv(tx - 3, CHUNK) != cx || floorDiv(tx + 3, CHUNK) != cx || floorDiv(ty - 2, CHUNK) != cy || floorDiv(ty, CHUNK) != cy) continue;
          if (fits(tx, ty, Prop::GreatPeak, true)) { bx = tx; by = ty; break; }
        }
        if (bx == INT32_MIN) { bx = gx; by = gy; }   // no room for the great one: its flanks still mark the summit
        else place(bx, by, Prop::GreatPeak);
        const Spire* sp = range ? kRange : kSummit;
        const int ns = range ? (int)(sizeof(kRange) / sizeof(kRange[0])) : (int)(sizeof(kSummit) / sizeof(kSummit[0]));
        for (int s = 1; s < ns; s++) {
          const int32_t ax = gx + sp[s].dx, ay = gy + sp[s].dy;
          if (fits(ax, ay, Prop::Peak, true)) place(ax, ay, Prop::Peak);
        }
        (void)by;
      }
  }
}

// 6c: the peaks
void EndlessSource::Impl::chunkPeaks(ChunkCtx& X) {
  ChunkData& c = X.c;
  const int32_t x0 = X.x0;
  const int32_t y0 = X.y0;
  std::vector<uint8_t>& reserved = X.reserved;
  std::vector<uint8_t>& used = X.used;
  std::vector<uint8_t>& road = X.road;
  std::vector<uint8_t>& bits = X.bits;
  auto rin = [&X](int32_t gx, int32_t gy) { return X.rin(gx, gy); };
  auto ri = [&X](int32_t gx, int32_t gy) { return X.ri(gx, gy); };
  // 6c (M2): peaks (VISION_PLAN 11.3): rock spires (Prop::Peak, 3 x 2 with Filler) along the crests of the high ground,
  // so a range reads as a range. The land is cut into 8 x 8 cells (aligned to chunks, so a cell never straddles two);
  // a cell's candidate is its highest point of the relief field among the tiles of level 6 or more (a pure choice), and
  // it stands if it is a crest (the relief falls away on both sides along some direction) and no neighbouring cell's
  // candidate within 5 tiles stands higher (both chunks compute the same pure answers). The chunk then vetoes a spire
  // whose footprint (and a tile round it) is not level, dry and clear: cliffs, ramps, roads, anything built or stamped.
  {
    bool high = false;
    for (int ly = 0; ly < CHUNK && !high; ly++)
      for (int lx = 0; lx < CHUNK; lx++) if ((c.height[c.at(lx, ly)] & Map::HEIGHT_LEVEL) >= 6) { high = true; break; }
    if (high) {
      constexpr int NC = CHUNK / 8 + 2;   // the chunk's cells and one round them
      struct PC { int32_t x = 0, y = 0, e = INT32_MIN; bool crest = false; };
      PC pc[NC * NC];
      const int32_t ci0 = x0 / 8 - 1, cj0 = y0 / 8 - 1;
      const uint64_t ts = seed ^ tag("peak.tie");
      for (int j = 0; j < NC; j++)
        for (int i = 0; i < NC; i++) {
          PC& p = pc[j * NC + i];
          const int32_t bx = (ci0 + i) * 8, by = (cj0 + j) * 8;
          for (int32_t y = by + 2; y <= by + 6; y++)
            for (int32_t x = bx + 1; x <= bx + 6; x++) {
              const int32_t er = reliefE(x, y);
              if (levelOf(er) < 6) continue;   // (natLevel is levelOf(reliefE))
              const int32_t e = er + (int32_t)(tileHash(ts, x, y) & 63);
              if (e > p.e) { p.e = e; p.x = x; p.y = y; }
            }
          if (p.e == INT32_MIN) continue;
          // the spine of a range (the plate ridge field strong here): every cell's high point is a peak, so the range
          // stands as a wall of spires; elsewhere a crest: some direction along which the relief falls on both sides
          if (tile(p.x, p.y).ridge >= Q(0.42)) { p.crest = true; continue; }
          const int32_t e0 = reliefE(p.x, p.y), dlt = Q(0.0035);
          static const int dd[4][2] = {{5, 0}, {0, 5}, {4, 4}, {4, -4}};
          for (int k = 0; k < 4 && !p.crest; k++)
            if (reliefE(p.x + dd[k][0], p.y + dd[k][1]) < e0 - dlt && reliefE(p.x - dd[k][0], p.y - dd[k][1]) < e0 - dlt) p.crest = true;
          // (M2 fixer) and the land must fall a level close by (within 4 tiles): a spire in the middle of a flat
          // high snowfield stood there like a cone set down on the ground
          if (p.crest) {
            const int lv0 = levelOf(e0);
            bool edge = false;
            static const int rd[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
            for (int k = 0; k < 8 && !edge; k++)
              for (int r = 2; r <= 4 && !edge; r++)
                if (levelOf(reliefE(p.x + rd[k][0] * r, p.y + rd[k][1] * r)) < lv0) edge = true;
            p.crest = edge;
          }
        }
      for (int j = 1; j < NC - 1; j++)
        for (int i = 1; i < NC - 1; i++) {
          const PC& p = pc[j * NC + i];
          if (!p.crest) continue;
          bool yield = false;
          for (int oj = -1; oj <= 1 && !yield; oj++)
            for (int oi = -1; oi <= 1; oi++) {
              if (!oi && !oj) continue;
              const PC& q = pc[(j + oj) * NC + i + oi];
              if (q.crest && (q.e > p.e || (q.e == p.e && (q.x < p.x || (q.x == p.x && q.y < p.y)))) && std::abs(q.x - p.x) <= 5 && std::abs(q.y - p.y) <= 5) { yield = true; break; }
            }
          if (yield) continue;
          const int bx = p.x - x0, by = p.y - y0;
          const int lv = c.height[c.at(bx, by)] & Map::HEIGHT_LEVEL;
          if (lv < 6) continue;
          bool ok = true;
          for (int oy = -2; oy <= 1 && ok; oy++)
            for (int ox = -2; ox <= 2 && ok; ox++) {
              const int lx = bx + ox, ly = by + oy;
              const int32_t gx = x0 + lx, gy = y0 + ly;
              if (!rin(gx, gy)) { ok = false; break; }
              const uint8_t hb = bits[ri(gx, gy)];
              const bool foot = oy >= -1 && oy <= 0 && ox >= -1 && ox <= 1;
              if ((hb & Map::HEIGHT_STEP) || (hb & Map::HEIGHT_LEVEL) != lv) { if (foot || (hb & Map::HEIGHT_RAMP)) ok = false; continue; }
              if (road[ri(gx, gy)] || used[ri(gx, gy)]) ok = false;
              if (!foot) continue;
              const int ii = c.at(lx, ly);
              if (reserved[(size_t)ii] || c.prop[ii] || c.wall[ii] || c.bldg[ii] || groundSolid((Ground)c.ground[ii]) || waterG(c.ground[ii]) ||
                  c.ground[ii] == (uint8_t)Ground::Bridge || c.ground[ii] == (uint8_t)Ground::Road || c.ground[ii] == (uint8_t)Ground::Dirt)
                ok = false;
            }
          // never on a pass: a road within 4 tiles means the way over the range runs here
          for (int oy = -4; oy <= 4 && ok; oy++)
            for (int ox = -4; ox <= 4 && ok; ox++)
              if (rin(p.x + ox, p.y + oy) && road[ri(p.x + ox, p.y + oy)]) ok = false;
          if (!ok) continue;
          for (int oy = -1; oy <= 0; oy++)
            for (int ox = -1; ox <= 1; ox++) { c.prop[c.at(bx + ox, by + oy)] = (uint8_t)((int)Prop::Filler + 1); reserved[(size_t)c.at(bx + ox, by + oy)] = 1; }
          c.prop[c.at(bx, by)] = (uint8_t)((int)Prop::Peak + 1);
          // (M2 fixer) scree at its foot: a few rocks fallen from the spire tie it to the ground
          {
            const Biome pb = (Biome)c.biome[c.at(bx, by)];
            const bool snowy = pb == Biome::Snow || pb == Biome::Taiga || pb == Biome::Mountain;
            static const int sc[6][2] = {{-2, 0}, {2, 0}, {-1, 1}, {1, 1}, {-2, -1}, {2, 1}};
            for (int k = 0; k < 6; k++) {
              const uint32_t h = (uint32_t)tileHash(ts ^ 0x5C2Eu, p.x + sc[k][0], p.y + sc[k][1]);
              if (h % 3 == 0) continue;
              const int lx = bx + sc[k][0], ly = by + sc[k][1];
              if (lx < 0 || ly < 0 || lx >= CHUNK || ly >= CHUNK) continue;
              const int ii = c.at(lx, ly);
              if (reserved[(size_t)ii] || c.prop[ii] || (c.height[ii] & Map::HEIGHT_STEP) || (c.height[ii] & Map::HEIGHT_LEVEL) != lv ||
                  groundSolid((Ground)c.ground[ii]) || waterG(c.ground[ii]) || c.ground[ii] == (uint8_t)Ground::Road || c.ground[ii] == (uint8_t)Ground::Dirt)
                continue;
              c.prop[ii] = (uint8_t)((int)(snowy ? ((h >> 4) & 1 ? Prop::SnowRock : Prop::Rock) : ((h >> 4) & 1 ? Prop::Boulder : Prop::Rock)) + 1);
              reserved[(size_t)ii] = 1;
            }
          }
        }
    }
  }
}

// 6d: the cairns along the summit trails
void EndlessSource::Impl::chunkCairns(ChunkCtx& X) {
  ChunkData& c = X.c;
  const int32_t x0 = X.x0;
  const int32_t y0 = X.y0;
  std::vector<uint8_t>& reserved = X.reserved;
  std::vector<uint8_t>& used = X.used;
  std::vector<uint8_t>& road = X.road;
  const std::shared_ptr<const RegionData>* RD = X.RD;
  std::vector<uint8_t>& bits = X.bits;
  auto ri = [&X](int32_t gx, int32_t gy) { return X.ri(gx, gy); };
  // 6d (M11): cairns along the summit trails (relief_plan.cpp): every 14 tiles of a trail's raster, two tiles to one
  // side of it (by a hash of the place), on free, level, dry, open ground. Decided from the plan's raster, so every
  // chunk agrees on where a cairn may stand; the chunk that holds the tile places it.
  for (int k = 0; k < 9; k++)
    for (const RoadPlan& rp : RD[k]->plan.roads) {
      // (a summit's trail carries the spurs' id scheme: 0x800 | the shrine's local id, 0x340 ..; relief_plan.cpp)
      if (rp.cls != 2 || !rp.a || (idLocal(rp.id) & 0x7F8u) != 0x340u) continue;
      const SitePlan* sp = nullptr;
      for (const SitePlan& s : RD[k]->plan.sites) if (s.id == rp.a) { sp = &s; break; }
      if (!sp || !(sp->flags & SPF_SUMMIT)) continue;
      int32_t bx0 = INT32_MAX, by0 = INT32_MAX, bx1 = INT32_MIN, by1 = INT32_MIN;
      for (const GTile& t : rp.pts) { bx0 = std::min(bx0, t.x); by0 = std::min(by0, t.y); bx1 = std::max(bx1, t.x); by1 = std::max(by1, t.y); }
      if (bx1 + 3 < x0 || by1 + 3 < y0 || bx0 - 3 >= x0 + CHUNK || by0 - 3 >= y0 + CHUNK) continue;
      int n = 0;
      for (size_t q = 0; q + 1 < rp.pts.size(); q++) {
        const GTile &a = rp.pts[q], &b = rp.pts[q + 1];
        const bool horiz = std::abs(b.x - a.x) >= std::abs(b.y - a.y);
        walk4(a.x, a.y, b.x, b.y, [&](int32_t tx, int32_t ty) {
          if (n++ % 14 != 9) return;
          const int side = (tileHash(sp->seed ^ 0xCA12u, tx, ty) & 1) ? 1 : -1;
          const int32_t cxx = horiz ? tx : tx + 2 * side, cyy = horiz ? ty + 2 * side : ty;
          if (cxx < x0 || cyy < y0 || cxx >= x0 + CHUNK || cyy >= y0 + CHUNK) return;
          const int ii = c.at(cxx - x0, cyy - y0);
          if (reserved[(size_t)ii] || c.prop[ii] || c.wall[ii] || c.bldg[ii] || road[ri(cxx, cyy)] || used[ri(cxx, cyy)]) return;
          if (c.height[ii] & (Map::HEIGHT_STEP | Map::HEIGHT_FALL)) return;
          for (int d = 0; d < 4; d++)   // (never in the way of a ramp)
            if (bits[ri(cxx + (d == 0) - (d == 1), cyy + (d == 2) - (d == 3))] & Map::HEIGHT_RAMP) return;
          const Ground gg = (Ground)c.ground[ii];
          if (groundSolid(gg) || waterG(c.ground[ii]) || gg == Ground::Bridge || gg == Ground::Road || gg == Ground::Dirt) return;
          c.prop[ii] = (uint8_t)((int)Prop::Cairn + 1);
          reserved[(size_t)ii] = 1;
        });
      }
    }
}

}  // namespace ew
