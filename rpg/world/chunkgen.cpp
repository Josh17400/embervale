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
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include "rpg/world/gen.h"
#include "rpg/world/town_gen.h"

namespace ew {
using namespace gen;
using art::Monster;
using art::Prop;

namespace {
constexpr int BR = 2;                         // base border
constexpr int BW = CHUNK + 2 * BR;            // 36
constexpr int RW = CHUNK + 2;                 // relief / road grid (1-tile border): 34
inline bool waterG(uint8_t g) { return g == (uint8_t)Ground::Water || g == (uint8_t)Ground::DeepWater; }
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
  }
  for (int y = std::max(0, yFrom); y < std::min(h, yTo); y++)
    for (int x = 0; x < w; x++) {
      TileF f = tile(x0 + x, y0 + y);
      size_t i = (size_t)y * w + x;
      B.biome[i] = (uint8_t)f.biome;
      B.ground[i] = (uint8_t)groundFor(f.biome, f.e, f.t, x0 + x, y0 + y);
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
        const uint8_t lakeLv = (uint8_t)natLevel(L.x, L.y);
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
                                  : groundFor(bio, Q(0.5), Q(0.5), gx, gy));
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
    ctx.rx = J.D->plan.rx; ctx.ry = J.D->plan.ry;
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
        g = groundFor(f.biome, f.e, f.t, gx, gy);
        b = f.biome;
      }
      bool inside = gx >= pp->gx - 8 && gy >= pp->gy - 8 && gx < pp->gx + pp->w + 8 && gy < pp->gy + pp->h + 8;
      h = (uint8_t)(inside ? flat : natLevel(gx, gy));
      // the town stands on dry land: marsh pools in its footprint are drained (a river still runs through, bridged)
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
  return false;
}

std::shared_ptr<SettlementOut> EndlessSource::Impl::town(const SitePlan& p, std::shared_ptr<const RegionData> D) {
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
      if (towns.count(p.id)) continue;
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
        const bool face = natLevel(x, y - 1) > natLevel(x, y);
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
            if (face && natLevel(xx, yy) < natLevel(x, y - 1)) continue;
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
      if (S.in(x, y + 1) && (tileHash(p.seed, x, y) & 1)) S.g(x, y + 1, Ground::Dirt);
      S.p(x + 2, y + 1, (p.seed >> 3) & 1 ? Prop::Bones : Prop::Boulder);
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
      // inside: two rows of columns (some standing, some snapped off, some lying in pieces), braziers by the door
      const int32_t c0 = wy + 2 + rr.irange(2), c1 = c0 + 3;
      for (int32_t cy : {c0, c1})
        for (int32_t cx : {rx + 1, rx + 7}) {
          const uint32_t h = (uint32_t)(tileHash(rs ^ 0x44u, cx, cy) & 255);
          if (h < 190) S.p(cx, cy, Prop::RuinColumn);
          else S.p(cx, cy, (h & 1) ? Prop::Rock : Prop::MossRock);
        }
      S.p(dx0 - 2, wy + 1, Prop::Brazier); S.p(dx0 + 2, wy + 1, Prop::Brazier);
      const int layout = rr.irange(3);
      if (layout == 0) { S.p(dx0 - 1, wy + 1, Prop::Statue); S.p(dx0 + 1, wy + 1, Prop::Statue); }
      else if (layout == 1) { S.p(rx + 2 + rr.irange(2) * 4, hy1 - 1, Prop::Gravestone); S.p(rx, c0 + 1, Prop::Urn); S.p(rx + 8, c1 - 1, Prop::Bones); }
      else { S.p(rx, c0 + 1, Prop::Gravestone); S.p(rx + 8, c0 + 2, Prop::Gravestone); S.p(rx + 2, c1 + 1, Prop::Coffin); S.p(rx + 6, c0 + 1, Prop::SkullPile); }
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
      // the camp's bandits (the whole site's, so they arrive with any chunk the camp touches)
      Rng r(p.seed ^ 0xBA4Du);
      int n = 4 + r.irange(3);
      for (int k = 0; k < n; k++) {
        SpawnPlan s; s.siteId = p.id;
        s.sp.bandit = true; s.sp.slot = k;
        s.sp.x = bx + 3 + r.irange(5); s.sp.y = by + 3 + r.irange(3);
        S.c.spawns.push_back(s);
      }
      SpawnPlan chief; chief.siteId = p.id;
      chief.sp.bandit = true; chief.sp.boss = true; chief.sp.slot = 9; chief.sp.x = bx + 6; chief.sp.y = by + 3;
      S.c.spawns.push_back(chief);
      break;
    }
    case SiteType::Shrine: {
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
        S.p(x - 3 - lr.irange(2), y - 2 + lr.irange(2), Prop::SkullPile); S.p(x + 3 + lr.irange(2), y + 1, Prop::Bones); S.p(x - 5, y + 2 + lr.irange(2), Prop::Bones);
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

// ------------------------------------------------------------------ the chunk
void EndlessSource::Impl::chunk(int32_t cx, int32_t cy, ChunkData& c) {
  makeStart();
  auto t0 = Clock::now();
  const double nested0 = nestedMs;
  c = ChunkData();
  c.cx = cx; c.cy = cy;
  const int32_t x0 = cx * CHUNK, y0 = cy * CHUNK;
  // 1-2: base land, rivers, lakes (with a 2-tile border)
  BaseRect B;
  baseRect(x0 - BR, y0 - BR, BW, BW, B);
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) {
      size_t bi = (size_t)(ly + BR) * BW + (lx + BR);
      c.ground[c.at(lx, ly)] = B.ground[bi];
      c.biome[c.at(lx, ly)] = B.biome[bi];
    }
  std::vector<uint8_t> reserved((size_t)ChunkData::N, 0);
  std::vector<uint8_t> used(RW * RW, 0), road(RW * RW, 0), entrance(RW * RW, 0);
  auto rin = [&](int32_t gx, int32_t gy) { return gx >= x0 - 1 && gy >= y0 - 1 && gx < x0 + CHUNK + 1 && gy < y0 + CHUNK + 1; };
  auto ri = [&](int32_t gx, int32_t gy) { return (size_t)(gy - (y0 - 1)) * RW + (gx - (x0 - 1)); };
  Stamp S{c, x0, y0, reserved};
  const int32_t rx0 = regionOf(x0), ry0 = regionOf(y0);
  std::shared_ptr<const RegionData> RD[9];
  for (int k = 0; k < 9; k++) RD[k] = regionData(rx0 - 1 + k % 3, ry0 - 1 + k / 3);
  const RegionData& own = *RD[4];
  // 3: settlement buffers
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites) {
      if (!isSettlement(p.type)) continue;
      const int margin = 32;
      if (p.gx - margin >= x0 + CHUNK + 1 || p.gy - margin >= y0 + CHUNK + 1 || p.gx + p.w + margin <= x0 - 1 || p.gy + p.h + margin <= y0 - 1) continue;
      std::shared_ptr<SettlementOut> T = town(p, RD[k]);
      const Map& b = T->buf;
      for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
        for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++) {
          int32_t bx = gx - T->gx, by = gy - T->gy;
          if (!b.in(bx, by)) continue;
          size_t bi = (size_t)by * b.w + bx;
          if (!T->used.empty() && !T->used[bi]) continue;   // outside the town: the wild land stays
          used[ri(gx, gy)] = 1;
          if (!S.in(gx, gy)) continue;
          int i = S.idx(gx, gy);
          c.ground[i] = b.ground[bi];
          c.prop[i] = b.prop[bi];
          c.wall[i] = b.wall[bi];
          if (!b.biome.empty()) c.biome[i] = b.biome[bi];
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
      for (auto& gt : T->gates) c.gates.push_back(GTile{gt.first + T->gx, gt.second + T->gy});
      for (IRect r : T->wallGaps) { r.x += T->gx; r.y += T->gy; c.wallGaps.push_back(r); }
    }
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
  // 5: roads (this region's graph roads) and tracks (every nearby site's spur), clipped at settlement footprints
  struct Clip { Gid id; int32_t x0, y0, x1, y1; };
  std::vector<Clip> clips;
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites) {
      int pad = isSettlement(p.type) ? 0 : 1;
      // (M1) a cave's track runs right up to the trodden ground before its mouth (it stopped at the footprint, a few
      // tiles short): only the rows down to the mouth's are kept clear
      const int32_t yEnd = p.type == SiteType::Cave ? std::min(p.gy + p.h + pad, p.ey + 1) : p.gy + p.h + pad;
      clips.push_back(Clip{p.id, p.gx - pad, p.gy - pad, p.gx + p.w + pad, yEnd});
    }
  std::vector<Clip> keeps;   // wayside places and wonders whose props the roads leave standing
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites)
      if (p.type == SiteType::Wonder || (p.type == SiteType::Vignette && p.kind != (uint8_t)VignetteKind::TollBridge))
        if (p.gx <= x0 + CHUNK && p.gy <= y0 + CHUNK && p.gx + p.w >= x0 && p.gy + p.h >= y0) keeps.push_back(Clip{p.id, p.gx, p.gy, p.gx + p.w, p.gy + p.h});
  auto clipOf = [&](Gid id) -> const Clip* {
    for (const Clip& cl : clips) if (cl.id == id) return &cl;
    return nullptr;
  };
  auto drawRoad = [&](const RoadPlan& rp) {
    const Clip* ca = rp.a ? clipOf(rp.a) : nullptr;
    const Clip* cb = rp.b ? clipOf(rp.b) : nullptr;
    const uint8_t code = rp.cls == 0 ? 3 : rp.cls == 1 ? 2 : 1;
    const int hi = rp.cls == 0 ? 1 : 0;
    for (size_t n = 0; n + 1 < rp.pts.size(); n++) {
      const GTile &a = rp.pts[n], &b = rp.pts[n + 1];
      if (std::max(a.x, b.x) + 2 < x0 - 1 || std::min(a.x, b.x) - 2 > x0 + CHUNK || std::max(a.y, b.y) + 2 < y0 - 1 || std::min(a.y, b.y) - 2 > y0 + CHUNK) continue;
      walk4(a.x, a.y, b.x, b.y, [&](int32_t x, int32_t y) {
        for (int oy = 0; oy <= hi; oy++)
          for (int ox = 0; ox <= hi; ox++) {
            int32_t gx = x + ox, gy = y + oy;
            if (!rin(gx, gy)) continue;
            if (ca && gx >= ca->x0 && gy >= ca->y0 && gx < ca->x1 && gy < ca->y1) continue;
            if (cb && gx >= cb->x0 && gy >= cb->y0 && gx < cb->x1 && gy < cb->y1) continue;
            size_t i = ri(gx, gy);
            if (used[i]) continue;
            road[i] = std::max(road[i], code);
          }
      });
    }
  };
  for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
    for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++)
      if (B.bridge[B.at(gx, gy)] && !used[ri(gx, gy)]) road[ri(gx, gy)] = 1;
  for (const RoadPlan& rp : own.plan.roads) if (rp.cls != 2) drawRoad(rp);
  for (int k = 0; k < 9; k++)
    for (const RoadPlan& rp : RD[k]->plan.roads) if (rp.cls == 2) drawRoad(rp);
  // (M2) where a troll keeps a toll, the road crosses even a stream by a bridge (never a ford)
  std::vector<GTile> tolls;
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites)
      if (p.type == SiteType::Vignette && p.kind == (uint8_t)VignetteKind::TollBridge && p.ex > x0 - 12 && p.ey > y0 - 12 && p.ex < x0 + CHUNK + 12 &&
          p.ey < y0 + CHUNK + 12)
        tolls.push_back(GTile{p.ex, p.ey});
  // (M2 fixer round 2) a toll crossing is one straight bridge: from the head (two tiles from the post toward the road)
  // straight over the water the way the road steps onto it, then a short stretch of road on the far bank to the road's
  // own line there. The road's other wet tiles near it (a slanting road's staircase of deck pieces) stay water. All of it
  // is judged from the plans (waterAtPlan, the roads' rasters), so every chunk lays the same bridge.
  {
    static const int TDX[4] = {1, 0, -1, 0}, TDY[4] = {0, 1, 0, -1};
    for (int k = 0; k < 9; k++)
      for (const SitePlan& p : RD[k]->plan.sites) {
        if (p.type != SiteType::Vignette || p.kind != (uint8_t)VignetteKind::TollBridge) continue;
        if (p.ex < x0 - 16 || p.ey < y0 - 16 || p.ex >= x0 + CHUNK + 16 || p.ey >= y0 + CHUNK + 16) continue;
        const int side = (int)((p.seed >> 24) & 3), along = (int)((p.seed >> 26) & 3);
        const int32_t hx = p.ex - TDX[side] * 2, hy = p.ey - TDY[side] * 2;
        std::vector<GTile> deck;
        int32_t lx = 0, ly = 0;
        bool landed = false;
        for (int s = 1; s <= 8; s++) {
          const int32_t tx = hx + TDX[along] * s, ty = hy + TDY[along] * s;
          if (waterAtPlan(tx, ty)) deck.push_back(GTile{tx, ty});
          else { lx = tx; ly = ty; landed = true; break; }
        }
        if (!landed || deck.empty()) continue;
        // the road's line on the far bank: the nearest dry raster tile of any road within 6 tiles of the landing,
        // beyond the water (the roads' rasters walked from their plans, as drawRoad lays them)
        int32_t rx = lx, ry = ly;
        int64_t best = INT64_MAX;
        uint8_t rcode = 1;
        const int64_t far0 = (int64_t)(lx - hx) * TDX[along] + (int64_t)(ly - hy) * TDY[along];
        for (int q = 0; q < 9; q++)
          for (const RoadPlan& rp : RD[q]->plan.roads)
            for (size_t n = 0; n + 1 < rp.pts.size(); n++) {
              const GTile &a = rp.pts[n], &b = rp.pts[n + 1];
              if (std::max(a.x, b.x) < lx - 6 || std::min(a.x, b.x) > lx + 6 || std::max(a.y, b.y) < ly - 6 || std::min(a.y, b.y) > ly + 6) continue;
              walk4(a.x, a.y, b.x, b.y, [&](int32_t qx, int32_t qy) {
                if (std::abs(qx - lx) > 6 || std::abs(qy - ly) > 6) return;
                if ((int64_t)(qx - hx) * TDX[along] + (int64_t)(qy - hy) * TDY[along] < far0) return;
                if (waterAtPlan(qx, qy)) return;
                const int64_t d = dist2(qx, qy, lx, ly) * 4096 + (int64_t)(qy - ly + 8) * 64 + (qx - lx + 8);
                if (d < best) { best = d; rx = qx; ry = qy; rcode = rp.cls == 0 ? 3 : rp.cls == 1 ? 2 : 1; }
              });
            }
        // the slanting road's own wet tiles round the crossing are dropped (the straight deck replaces them)
        for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
          for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++) {
            if (!road[ri(gx, gy)]) continue;
            bool nearDeck = false, onDeck = false;
            for (const GTile& t : deck) {
              if (t.x == gx && t.y == gy) onDeck = true;
              if (std::abs(t.x - gx) <= 3 && std::abs(t.y - gy) <= 3) nearDeck = true;
            }
            if (nearDeck && !onDeck && waterAtPlan(gx, gy)) road[ri(gx, gy)] = 0;
          }
        for (const GTile& t : deck) if (rin(t.x, t.y)) road[ri(t.x, t.y)] = std::max<uint8_t>(road[ri(t.x, t.y)], 1);
        walk4(lx, ly, rx, ry, [&](int32_t qx, int32_t qy) {
          if (rin(qx, qy) && !waterAtPlan(qx, qy) && !used[ri(qx, qy)]) road[ri(qx, qy)] = std::max(road[ri(qx, qy)], rcode);
        });
      }
  }
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) {
      uint8_t code = road[ri(x0 + lx, y0 + ly)];
      if (!code) continue;
      int i = c.at(lx, ly);
      size_t bi = (size_t)(ly + BR) * BW + (lx + BR);
      Ground g = (Ground)c.ground[i];
      if (g == Ground::Water || g == Ground::DeepWater) {
        bool ford = B.riverW[bi] == 1 && !B.lake[bi];
        for (const GTile& t : tolls) if (dist2(t.x, t.y, x0 + lx, y0 + ly) <= 7 * 7) ford = false;
        c.ground[i] = (uint8_t)(ford ? Ground::Dirt : Ground::Bridge);
      } else if (g == Ground::Bridge || g == Ground::Road) {
        // keep
      } else {
        c.ground[i] = (uint8_t)(code >= 2 ? Ground::Road : Ground::Dirt);
      }
      // (M1 round 3) a road that runs up to a dungeon's door leads to it, it does not pave it over
      // (M2) nor does a road or another place's track that crosses a wayside place or a wonder wipe its stamped props
      // (the lone grave's cairn, the camp's fire): it runs between them. A toll bridge's road is its own.
      bool keepProp = c.prop[i] == (uint8_t)((int)Prop::IronDoor + 1) || c.prop[i] == (uint8_t)((int)Prop::CaveEntrance + 1);
      for (size_t k = 0; !keepProp && c.prop[i] && k < keeps.size(); k++)
        keepProp = x0 + lx >= keeps[k].x0 && y0 + ly >= keeps[k].y0 && x0 + lx < keeps[k].x1 && y0 + ly < keeps[k].y1;
      if (!keepProp) c.prop[i] = 0;
      reserved[(size_t)i] = 1;
    }
  // 6: relief. Settlements and sites stand on one level, terraced back to the land's own level around them.
  // the flat zone is a rounded rectangle around the footprint with a wobbling edge, so the terraces around a town
  // follow the land's grain instead of drawing a box
  // (M2) The levels are worked on a wider grid (LB = 5 tiles round the chunk): the base rect's levels where it has
  // them, the natural level (and the lakes' level) on the outer ring. After the flats, three passes take out the relief
  // slivers (owner note 3: cliffs on open ground that read as thin lines): a tile with three or four neighbours on one
  // other level (a 1-wide finger, notch, island or pit) joins them, a 1-tile ridge or channel joins its sides, and a
  // 1-tile-deep terrace is raised into its upper step (a stacked cliff). Each pass needs one more tile of border, so
  // the levels are exact out to 2 tiles round the chunk, as the cliff and ramp bits need.
  constexpr int LB = 5, LW = CHUNK + 2 * LB;
  std::vector<uint8_t> lev((size_t)LW * LW), lakeL((size_t)LW * LW, 0);
  {
    const int32_t lx0 = x0 - LB, ly0 = y0 - LB;
    std::vector<uint8_t> lakeMin((size_t)LW * LW, 255);
    for (int32_t ry = regionOf(ly0 - 4); ry <= regionOf(ly0 + LW + 3); ry++)
      for (int32_t rx = regionOf(lx0 - 4); rx <= regionOf(lx0 + LW + 3); rx++) {
        std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
        for (const Lake& Lk : H->lakes) {
          const int32_t R = 2 * Lk.r + 2;
          if (Lk.x + R < lx0 || Lk.x - R >= lx0 + LW || Lk.y + R < ly0 || Lk.y - R >= ly0 + LW) continue;
          if (lakeUnderSettlement(Lk)) continue;
          const uint8_t lakeLv = (uint8_t)natLevel(Lk.x, Lk.y);
          for (int32_t y = std::max(ly0, Lk.y - R); y < std::min(ly0 + LW, Lk.y + R + 1); y++)
            for (int32_t x = std::max(lx0, Lk.x - R); x < std::min(lx0 + LW, Lk.x + R + 1); x++) {
              const int32_t rr = lakeRadius(Lk, x, y);
              if (dist2(x, y, Lk.x, Lk.y) >= (int64_t)rr * rr) continue;
              uint8_t& m = lakeMin[(size_t)(y - ly0) * LW + (x - lx0)];
              m = std::min(m, lakeLv);
            }
        }
      }
    for (int y = 0; y < LW; y++)
      for (int x = 0; x < LW; x++) {
        const int32_t gx = lx0 + x, gy = ly0 + y;
        const size_t i = (size_t)y * LW + x;
        if (B.in(gx, gy)) {
          lev[i] = B.level[B.at(gx, gy)];
          lakeL[i] = B.lake[B.at(gx, gy)];
        } else {
          lakeL[i] = lakeMin[i] != 255;
          // (the sea is level 0, as tile() makes it; land of level 2 or more is never sea, so only low land asks)
          if (lakeL[i]) lev[i] = lakeMin[i];
          else {
            const int nl = natLevel(gx, gy);
            lev[i] = (uint8_t)(nl == 0 || (nl == 1 && waterE(gx, gy) < ELEV_SEA) ? 0 : nl);
          }
        }
      }
  }
  struct Flat { int32_t cx2, cy2, hw2, hh2, rc; int lv, step; uint64_t wob; };   // centre and half sizes doubled
  std::vector<Flat> flats;
  for (int k = 0; k < 9; k++) {
    const RegionData& D = *RD[k];
    for (size_t s = 0; s < D.plan.sites.size(); s++) {
      int lv = D.flatLevel[s].first;
      if (lv < 0) continue;
      const SitePlan& p = D.plan.sites[s];
      const bool st = isSettlement(p.type);
      const int pad = st ? 10 : 3;
      Flat f{2 * p.gx + p.w, 2 * p.gy + p.h, p.w + 2 * pad, p.h + 2 * pad, 0, lv, st ? 14 : 6, mix64(p.id ^ 0xF1A7u)};
      f.rc = std::min(f.hw2, f.hh2) * (st ? 7 : 10) / 10;   // corner radius (doubled units); small sites: a stadium
      const int32_t reach = 4 * f.step + 14;
      if ((f.cx2 - f.hw2) / 2 - reach > x0 + CHUNK + LB || (f.cy2 - f.hh2) / 2 - reach > y0 + CHUNK + LB ||
          (f.cx2 + f.hw2) / 2 + reach < x0 - LB || (f.cy2 + f.hh2) / 2 + reach < y0 - LB)
        continue;
      flats.push_back(f);
    }
  }
  if (!flats.empty())
    for (int y = 0; y < LW; y++)
      for (int x = 0; x < LW; x++) {
        const int32_t gx = x0 - LB + x, gy = y0 - LB + y;
        int lo = 0, hi = 7, bestD = INT32_MAX, bestLv = -1;
        bool any = false;
        for (const Flat& f : flats) {
          // distance (tiles) outside the rounded rectangle, plus a slow wobble of +-5 tiles
          int32_t qx = std::max(0, std::abs(2 * gx + 1 - f.cx2) - (f.hw2 - f.rc)), qy = std::max(0, std::abs(2 * gy + 1 - f.cy2) - (f.hh2 - f.rc));
          int32_t out2 = (int32_t)isqrt((uint64_t)qx * qx + (uint64_t)qy * qy) - f.rc;   // doubled units
          if (out2 / 2 - 12 >= 4 * f.step) continue;   // (beyond its terraces whatever the wobble: skip the noise)
          // (two octaves, +-12 tiles: with one gentle octave the terraces ran as long straight lines parallel to
          // the town's rectangle, the "grid crack" through open plain)
          int32_t wob = (int32_t)(((int64_t)(vnoiseQ(gx, gy, 5, f.wob) - 32768) * 18) >> 16) +
                        (int32_t)(((int64_t)(vnoiseQ(gx, gy, 3, f.wob ^ 0x5A5Au) - 32768) * 6) >> 16);
          int32_t d = std::max(0, out2 / 2 + wob);
          int dev = d / f.step;
          if (dev >= 4) continue;
          any = true;
          lo = std::max(lo, f.lv - dev);
          hi = std::min(hi, f.lv + dev);
          if (d < bestD) { bestD = d; bestLv = f.lv; }
        }
        if (!any) continue;
        uint8_t& L = lev[(size_t)y * LW + x];
        if (lo <= hi) L = (uint8_t)std::clamp((int)L, lo, hi);
        else L = (uint8_t)bestLv;
      }
  // cliff and ramp bits on the 1-tile border grid
  std::vector<uint8_t> bits(RW * RW, 0);
  // the sliver passes (pass k is exact for tiles within LB - k of the chunk)
  for (int pass = 1; pass <= LB - 2; pass++) {
    std::vector<uint8_t> nl = lev;
    for (int y = pass; y < LW - pass; y++)
      for (int x = pass; x < LW - pass; x++) {
        const size_t i = (size_t)y * LW + x;
        if (lakeL[i]) continue;
        const int l = lev[i], n = lev[i - LW], s = lev[i + LW], w = lev[i - 1], e = lev[i + 1];
        const int nb[4] = {n, s, w, e};
        int hi = 0, lo = 0, minHi = 99, maxLo = -1;
        for (int k = 0; k < 4; k++) {
          if (nb[k] > l) { hi++; minHi = std::min(minHi, nb[k]); }
          if (nb[k] < l) { lo++; maxLo = std::max(maxLo, nb[k]); }
        }
        int to = l;
        if (hi >= 3) to = minHi;                                         // a notch, a pit
        else if (lo >= 3) to = maxLo;                                    // a finger, an island
        else if ((n > l && s < l) || (n < l && s > l)) to = std::max(n, s);   // a 1-deep terrace: one stacked cliff
        else if ((w > l && e < l) || (w < l && e > l)) to = std::max(w, e);
        else if (n > l && s > l) to = std::min(n, s);                   // a 1-wide channel
        else if (w > l && e > l) to = std::min(w, e);
        // a 1-wide ridge is lowered only where it ends (a neck joining two parts of a plateau along its line stays: lowering
        // it would cut the plateau in two, and the part beyond might keep no ramp)
        else if (n < l && s < l && !(w >= l && e >= l)) to = std::max(n, s);
        else if (w < l && e < l && !(n >= l && s >= l)) to = std::max(w, e);
        nl[i] = (uint8_t)to;
      }
    lev.swap(nl);
  }
  auto L = [&](int32_t gx, int32_t gy) { return (int)lev[(size_t)(gy - (y0 - LB)) * LW + (gx - (x0 - LB))]; };
  const uint64_t rampSeed = seed ^ tag("relief.ramp");
  for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
    for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++) {
      const int l = L(gx, gy);
      const int ln = L(gx, gy - 1), ls = L(gx, gy + 1), lw = L(gx - 1, gy), le = L(gx + 1, gy);
      uint8_t b = (uint8_t)l;
      size_t bi = (size_t)(gy - (y0 - BR)) * BW + (gx - (x0 - BR));
      const bool wet = !B.bridge[bi] && (B.lake[bi] || B.riverW[bi] || waterG(B.ground[bi]));
      if (ln > l || ls > l || lw > l || le > l) {
        bool ramp = false;
        if (!wet && !entrance[ri(gx, gy)]) {
          if (ln > l || ls > l) {
            int32_t off = (int32_t)(tileHash(rampSeed, 0, gy >> 4) & 15);
            ramp = ((gx + off) & 15) < 3;
          } else {
            int32_t off = (int32_t)(tileHash(rampSeed, 1, gx >> 4) & 15);
            ramp = ((gy + off) & 15) < 3;
          }
          // roads and tracks climb by stairs
          if (road[ri(gx, gy)]) ramp = true;
          else
            for (int k = 0; k < 4 && !ramp; k++) {
              static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
              int32_t nx = gx + dx[k], ny = gy + dy[k];
              if (rin(nx, ny) && road[ri(nx, ny)]) ramp = true;
            }
        }
        if (entrance[ri(gx, gy)]) b = (uint8_t)l;
        else b |= ramp ? Map::HEIGHT_RAMP : Map::HEIGHT_CLIFF;
      }
      bits[ri(gx, gy)] = b;
    }
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) c.height[c.at(lx, ly)] = bits[ri(x0 + lx, y0 + ly)];
  // 6b (M2): ecotones (VISION_PLAN 11.6). Along a border between two land biomes, a band 4 to 8 tiles wide (by the
  // pair) carries the other biome and its weight in Map::blend: bits 0-3 the other biome, 4-7 its weight, 4 at the
  // border falling to 0 at the band's edge, so both sides meet half and half. Read from the biome field on a grid 4
  // tiles wider than the chunk (the base rect's biomes, then biomeLite on the outer ring), so chunk seams agree. The
  // view dithers the ground with it; the vegetation below takes the other side's flora at the same odds.
  {
    constexpr int EB = 4, EW = CHUNK + 2 * EB;
    // (a chunk whose base rect holds one biome has no border within 2 tiles of it: only the outermost tiles of a
    // neighbour's band would reach in, at weight 1, so the ring is not worth building)
    bool mixed = false;
    for (size_t k = 1; k < B.biome.size() && !mixed; k++) if (B.biome[k] != B.biome[0]) mixed = true;
    std::vector<uint8_t> eb;
    if (mixed) {
      eb.resize((size_t)EW * EW);
      for (int y = 0; y < EW; y++)
        for (int x = 0; x < EW; x++) {
          const int32_t gx = x0 - EB + x, gy = y0 - EB + y;
          eb[(size_t)y * EW + x] = B.in(gx, gy) ? B.biome[B.at(gx, gy)] : (uint8_t)biomeLite(gx, gy);
        }
    }
    if (mixed) {
      auto eco = [](uint8_t b) {
        const Biome q = (Biome)b;
        return q == Biome::Plains || q == Biome::Forest || q == Biome::Autumn || q == Biome::Taiga || q == Biome::Snow || q == Biome::Swamp || q == Biome::Desert;
      };
      // half the band's width (tiles) for a pair of biomes
      auto halfW = [](Biome a, Biome b) {
        auto is = [&](Biome p, Biome q) { return (a == p && b == q) || (a == q && b == p); };
        auto wood = [](Biome q) { return q == Biome::Forest || q == Biome::Autumn; };
        if (a == Biome::Swamp || b == Biome::Swamp) return 2;
        if (is(Biome::Plains, Biome::Desert)) return 4;
        if ((wood(a) && b == Biome::Plains) || (wood(b) && a == Biome::Plains)) return 3;
        if (is(Biome::Taiga, Biome::Snow) || is(Biome::Plains, Biome::Taiga) || is(Biome::Plains, Biome::Snow)) return 3;
        if ((wood(a) && b == Biome::Taiga) || (wood(b) && a == Biome::Taiga)) return 3;
        if (a == Biome::Desert || b == Biome::Desert) return 3;
        return 2;
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
          const uint8_t self = eb[(size_t)(ly + EB) * EW + lx + EB];
          if (!eco(self)) continue;
          if (used[ri(x0 + lx, y0 + ly)]) continue;   // town ground keeps its own look
          for (const Off& of : offs) {
            const uint8_t o = eb[(size_t)(ly + EB + of.dy) * EW + lx + EB + of.dx];
            if (o == self || !eco(o)) continue;
            const int hw = halfW((Biome)self, (Biome)o);
            const int32_t dd = (int32_t)isqrt((uint64_t)of.d2 * 100);   // tenths of a tile
            const int32_t w = (4 * (hw * 10 - dd + 10) + hw * 5) / (hw * 10);   // 4 beside the border .. 0 past hw
            if (w > 0) c.blend[c.at(lx, ly)] = (uint8_t)((std::min<int32_t>(w, 8) << 4) | (o & 15));
            break;
          }
        }
    }
  }
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
    auto fits = [&](int32_t ax, int32_t ay, Prop p, bool strict) {
      int fw = 1, fh = 1;
      art::wildFootprint(p, fw, fh);
      const int lx0 = ax - fw / 2 - x0, lx1 = ax + fw / 2 - x0, ly0 = ay - fh + 1 - y0, ly1 = ay - y0;
      if (lx0 < 0 || ly0 < 0 || lx1 >= CHUNK || ly1 >= CHUNK) return false;
      const int lv = c.height[c.at(ax - x0, ay - y0)] & Map::HEIGHT_LEVEL;
      for (int ly = ly0; ly <= ly1; ly++)
        for (int lx = lx0; lx <= lx1; lx++) {
          const int ii = c.at(lx, ly);
          const int32_t gx = x0 + lx, gy = y0 + ly;
          const uint8_t hb = c.height[ii];
          if ((hb & (Map::HEIGHT_CLIFF | Map::HEIGHT_RAMP)) || (hb & Map::HEIGHT_LEVEL) != lv) return false;
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
              if ((hb & (Map::HEIGHT_CLIFF | Map::HEIGHT_RAMP)) || (hb & Map::HEIGHT_LEVEL) != lv) { if (foot || (hb & Map::HEIGHT_RAMP)) ok = false; continue; }
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
              if (reserved[(size_t)ii] || c.prop[ii] || (c.height[ii] & (Map::HEIGHT_CLIFF | Map::HEIGHT_RAMP)) || (c.height[ii] & Map::HEIGHT_LEVEL) != lv ||
                  groundSolid((Ground)c.ground[ii]) || waterG(c.ground[ii]) || c.ground[ii] == (uint8_t)Ground::Road || c.ground[ii] == (uint8_t)Ground::Dirt)
                continue;
              c.prop[ii] = (uint8_t)((int)(snowy ? ((h >> 4) & 1 ? Prop::SnowRock : Prop::Rock) : ((h >> 4) & 1 ? Prop::Boulder : Prop::Rock)) + 1);
              reserved[(size_t)ii] = 1;
            }
          }
        }
    }
  }
  // 7: vegetation by per-tile hash (off reserved tiles, roads, cliffs, ramps and their approaches)
  const uint64_t vs = mix64(seed ^ tag("veg"));
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) {
      const int i = c.at(lx, ly);
      if (reserved[(size_t)i] || c.prop[i] || c.wall[i] || c.bldg[i]) continue;
      const int32_t x = x0 + lx, y = y0 + ly;
      if (bits[ri(x, y)] & (Map::HEIGHT_CLIFF | Map::HEIGHT_RAMP)) continue;
      bool nearRamp = false, nearRoad = false, nearWater = false, nearCliff = false;
      for (int k = 0; k < 8; k++) {
        static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1}, dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        int32_t nx = x + dx[k], ny = y + dy[k];
        uint8_t nb = bits[ri(nx, ny)];
        if (nb & Map::HEIGHT_RAMP) nearRamp = true;
        if ((nb & Map::HEIGHT_CLIFF) && k < 4) nearCliff = true;
        if (road[ri(nx, ny)]) nearRoad = true;
        size_t bi = (size_t)(ny - (y0 - BR)) * BW + (nx - (x0 - BR));
        if (k < 4 && (B.lake[bi] || B.riverW[bi])) nearWater = true;
      }
      if (nearRamp) continue;
      Ground g = (Ground)c.ground[i];
      if (g == Ground::Road || g == Ground::Bridge || g == Ground::Plaza || g == Ground::Farmland || g == Ground::StoneFloor || g == Ground::Dirt) continue;
      int32_t r = hq(tileHash(vs, x, y));
      int32_t dens = fbmQ(x, y, 3, 3, vs ^ 0xD3u);
      int32_t q = hq(tileHash(vs ^ 0x400u, x, y));
      uint64_t pk = tileHash(vs ^ 0x300u, x, y);
      auto pick = [&](std::initializer_list<Prop> l) { return *(l.begin() + (size_t)(pk % l.size())); };
      const Biome b = (Biome)c.biome[i];
      const int lv = bits[ri(x, y)] & Map::HEIGHT_LEVEL;
      // highland thins the woods; (M2) above the tree line (levels 6 and 7) only a few stunted trees among the rocks
      const int32_t thin = lv >= 7 ? Q(0.16) : lv >= 6 ? Q(0.32) : lv >= 5 ? Q(0.55) : lv >= 4 ? Q(0.8) : Q_ONE;
      Prop p = Prop::COUNT;
      // (M2) the ecotone's flora: at the blend's odds a tile grows the transition between the two sides (lone trees and
      // bushes from forest to plains, dry grass and scrub toward the desert, snow patches and dwarf pines toward the snow)
      {
        const uint8_t bl = c.blend[i];
        const int ew = bl >> 4;
        const Biome ob = (Biome)(bl & 15);
        if (ew && g != Ground::Water && hq(tileHash(vs ^ 0x7E0u, x, y)) < ew * Q(0.125)) {
          auto wood = [](Biome q) { return q == Biome::Forest || q == Biome::Autumn || q == Biome::Taiga; };
          bool set = true;
          if (b == Biome::Plains && wood(ob)) {   // the forest's outliers: a lone tree, bushes, ferns
            if (r < qm(Q(0.045), thin)) p = ob == Biome::Taiga ? pick({Prop::PineTree, Prop::PineTree2}) : ob == Biome::Autumn ? Prop::AutumnTree : pick({Prop::OakTree, Prop::OakTree2, Prop::BirchTree});
            else if (r < Q(0.085)) p = pick({Prop::Bush, Prop::BerryBush, Prop::Fern});
            else if (r < Q(0.20)) p = Prop::TallGrass;
          } else if (wood(b) && ob == Biome::Plains) {   // the wood thins out: glades of grass and flowers
            if (r < qm(Q(0.035), thin)) p = b == Biome::Taiga ? pick({Prop::PineTree, Prop::PineTree2}) : b == Biome::Autumn ? Prop::AutumnTree : pick({Prop::OakTree, Prop::BirchTree});
            else if (r < Q(0.09)) p = pick({Prop::Bush, Prop::Flowers1, Prop::Flowers2});
            else if (r < Q(0.24)) p = Prop::TallGrass;
          } else if (b == Biome::Plains && ob == Biome::Desert) {   // dry grass and scrub
            if (r < Q(0.010)) p = Prop::DeadTree;
            else if (r < Q(0.045)) p = Prop::Bush;
            else if (r < Q(0.06)) p = Prop::Rock;
            else if (r < Q(0.22)) p = Prop::TallGrass;
          } else if (b == Biome::Desert && ob == Biome::Plains) {   // the first grass and scrub on the sand
            if (r < Q(0.03)) p = Prop::Bush;
            else if (r < Q(0.11)) p = Prop::TallGrass;
            else if (r < Q(0.115)) p = Prop::Cactus;
          } else if (b == Biome::Taiga && ob == Biome::Snow) {   // snow patches and dwarf pines
            if (r < qm(Q(0.05), thin)) p = Prop::SnowPine;
            else if (r < Q(0.09)) p = pick({Prop::SnowBush, Prop::SnowRock});
          } else if (b == Biome::Snow && ob == Biome::Taiga) {   // the last pines and the moss under the snow
            if (r < qm(Q(0.06), thin)) p = pick({Prop::PineTree2, Prop::SnowPine});
            else if (r < Q(0.09)) p = pick({Prop::SnowBush, Prop::MossRock});
          } else set = false;
          if (set) {
            if (p == Prop::COUNT) continue;
            if ((nearRoad || nearCliff) && propSolid(p)) continue;
            c.prop[i] = (uint8_t)((int)p + 1);
            continue;
          }
        }
      }
      if (g == Ground::Water) {
        if (b == Biome::Swamp && r < Q(0.12)) p = Prop::LilyPad;
        else if (r < Q(0.025) && (Biome)c.biome[i] != Biome::Ocean && !B.riverW[(size_t)(ly + BR) * BW + lx + BR]) p = Prop::LilyPad;
      } else if (groundSolid(g)) {
        continue;
      } else if (nearWater && b != Biome::Desert && b != Biome::Snow && r < Q(0.30)) {
        p = q < Q(0.6) ? Prop::Reeds : Prop::TallGrass;
      } else switch (b) {
        case Biome::Plains:
          if (r < qm(Q(0.010) + std::max(0, (dens - Q(0.60)) / 2), thin)) p = pick({Prop::OakTree, Prop::OakTree2, Prop::BirchTree});
          else if (r < Q(0.07)) p = pick({Prop::Flowers1, Prop::Flowers2, Prop::Flowers3});
          else if (r < Q(0.15)) p = Prop::TallGrass;
          else if (r < Q(0.162)) p = pick({Prop::Bush, Prop::BerryBush});
          else if (r < Q(0.168) + (lv >= 4 ? Q(0.01) : 0)) p = pick({Prop::Rock, Prop::Boulder});
          break;
        case Biome::Forest:
          if (r < qm(Q(0.10) + qm(dens, Q(0.32)), thin)) p = pick({Prop::OakTree, Prop::OakTree2, Prop::OakTree, Prop::BirchTree});
          else if (r < Q(0.50)) { if (q < Q(0.12)) p = Prop::Fern; else if (q < Q(0.15)) p = pick({Prop::Bush, Prop::Mushrooms}); else if (q < Q(0.17)) p = pick({Prop::Stump, Prop::Log, Prop::MossRock}); else if (q < Q(0.25)) p = Prop::TallGrass; }
          break;
        case Biome::Autumn:
          if (r < qm(Q(0.08) + qm(dens, Q(0.30)), thin)) p = pick({Prop::AutumnTree, Prop::AutumnTree, Prop::BirchTree});
          else if (r < Q(0.5)) { if (q < Q(0.08)) p = Prop::Fern; else if (q < Q(0.12)) p = pick({Prop::Bush, Prop::Mushrooms, Prop::Flowers3}); else if (q < Q(0.13)) p = Prop::Stump; }
          break;
        case Biome::Taiga:
          if (r < qm(Q(0.06) + qm(dens, Q(0.28)), thin)) p = pick({Prop::PineTree, Prop::PineTree2});
          else if (r < Q(0.45)) { if (q < Q(0.04)) p = Prop::Fern; else if (q < Q(0.06)) p = pick({Prop::MossRock, Prop::Boulder, Prop::Stump}); else if (q < Q(0.1)) p = Prop::TallGrass; }
          break;
        case Biome::Snow:
          if (r < qm(Q(0.03) + qm(dens, Q(0.18)), thin)) p = Prop::SnowPine;
          else if (r < Q(0.4)) { if (q < Q(0.03)) p = Prop::SnowRock; else if (q < Q(0.05)) p = Prop::SnowBush; }
          break;
        case Biome::Swamp:
          if (r < Q(0.03) + qm(dens, Q(0.10))) p = pick({Prop::WillowTree, Prop::WillowTree, Prop::DeadTree});
          else if (r < Q(0.5)) { if (q < Q(0.2)) p = Prop::Reeds; else if (q < Q(0.24)) p = Prop::Mushrooms; else if (q < Q(0.28)) p = Prop::TallGrass; }
          break;
        case Biome::Desert:
          if (r < Q(0.012)) p = Prop::Cactus;
          else if (r < Q(0.018)) p = pick({Prop::Rock, Prop::DeadTree});
          break;
        case Biome::Beach:
          if (r < Q(0.025)) p = Prop::Rock;
          else if (r < Q(0.05)) p = Prop::Reeds;
          break;
        default: break;
      }
      // (M2) the high ground's scree: rocks and boulders strewn over the mountains' shoulders and plateaus
      if (p == Prop::COUNT && lv >= 6 && b != Biome::Swamp && g != Ground::Water && r > Q(0.80) && r < Q(0.80) + (lv >= 7 ? Q(0.022) : Q(0.012)))
        p = b == Biome::Snow || b == Biome::Taiga ? pick({Prop::SnowRock, Prop::SnowRock, Prop::Boulder}) : pick({Prop::Rock, Prop::Boulder, Prop::MossRock});
      if (p == Prop::COUNT) continue;
      // keep the road verges and cliff feet free of anything solid
      if ((nearRoad || nearCliff) && propSolid(p)) continue;
      c.prop[i] = (uint8_t)((int)p + 1);
    }
  // (M2 fixer round 2) the low summer flora (grass tufts, flowers, ferns, mushrooms) never stands on paving or on snow,
  // whoever put it there (a town's dressing before its streets were paved, a vignette's sprinkle, the scatter on ground
  // the view covers with snow: taiga and snowfields from level 4, mountains from 5). A leafy bush on snow wears snow.
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) {
      const int i = c.at(lx, ly);
      const int pr = c.prop[i];
      if (!pr) continue;
      const Prop pp = (Prop)(pr - 1);
      const bool flora = pp == Prop::Flowers1 || pp == Prop::Flowers2 || pp == Prop::Flowers3 || pp == Prop::TallGrass || pp == Prop::Fern ||
                         pp == Prop::Mushrooms;
      const bool leafy = pp == Prop::Bush || pp == Prop::BerryBush;
      if (!flora && !leafy) continue;
      const Ground g = (Ground)c.ground[i];
      const Biome b = (Biome)c.biome[i];
      const int lv = c.height[i] & Map::HEIGHT_LEVEL;
      const bool paved = g == Ground::Road || g == Ground::Plaza || g == Ground::StoneFloor || g == Ground::Bridge || g == Ground::WoodFloor;
      const bool snowy = g == Ground::Snow || g == Ground::Ice || ((b == Biome::Snow || b == Biome::Taiga) && lv >= 4) || (b == Biome::Mountain && lv >= 5);
      if (flora && (paved || snowy)) c.prop[i] = 0;
      else if (leafy && snowy) c.prop[i] = (uint8_t)((int)Prop::SnowBush + 1);
    }
  double ms = msSince(t0) - (nestedMs - nested0);
  stats.chunks++;
  stats.chunkMs += ms;
  stats.maxChunkMs = std::max(stats.maxChunkMs, ms);
}

}  // namespace ew
