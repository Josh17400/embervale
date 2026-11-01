// L2b, the chunk pipeline's step 6 (SOLID 3.2: moved from chunkgen.cpp, unchanged): relief. The level grid (LB tiles
// round the chunk), the rivers' beds, the places' flats and terraces, then the sliver passes and the cliff, ramp,
// stair, slope and fall bits. WORLD lane; gen::ChunkCtx carries the chunk's state between the stages.

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
constexpr int BR = ChunkCtx::BR;
constexpr int BW = ChunkCtx::BW;
constexpr int RW = ChunkCtx::RW;
constexpr int LB = ChunkCtx::LB, LW = ChunkCtx::LW;
}  // namespace

// 6: the level grid: the base rect's levels, the natural level and the lakes' on the outer ring
void EndlessSource::Impl::chunkLevels(ChunkCtx& X) {
  const int32_t x0 = X.x0;
  const int32_t y0 = X.y0;
  const BaseRect& B = X.B;
  std::vector<uint8_t>& lev = X.lev;
  std::vector<uint8_t>& lakeL = X.lakeL;
  // 6: relief. Settlements and sites stand on one level, terraced back to the land's own level around them.
  // the flat zone is a rounded rectangle around the footprint with a wobbling edge, so the terraces around a town
  // follow the land's grain instead of drawing a box
  // (M2) The levels are worked on a wider grid (LB = 6 tiles round the chunk): the base rect's levels where it has
  // them, the natural level (and the lakes' level) on the outer ring. After the flats, four passes take out the relief
  // slivers (owner note 3: cliffs on open ground that read as thin lines): a tile with three or four neighbours on one
  // other level (a 1-wide finger, notch, island or pit) joins them, a 1-tile ridge or channel joins its sides, and a
  // 1-tile-deep terrace is raised into its upper step (a stacked cliff). Each pass needs one more tile of border, so
  // the levels are exact out to 3 tiles round the chunk, so the final fix-up below is exact out to 2, as the cliff and
  // ramp bits (and the landings, which read 2 tiles out) need. (M11 integration: LB 6 and a fourth pass, from 5 and
  // three: the stacked tiers left 9-10 slivers a window on seeds 6 and 8. M11 fixer r1: LB 7, the same four passes, so
  // the fix-up covers the full 2-tile ring: it ran within 1 tile, and the chunks either side of a seam disagreed on a
  // landing 2 tiles out, leaving a level step with no step bit on its lower tile.)
  lev.assign((size_t)LW * LW, 0);
  lakeL.assign((size_t)LW * LW, 0);
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
          const uint8_t lakeLv = (uint8_t)fineLevel(Lk.x, Lk.y);
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
            const int nl = fineLevel(gx, gy);
            lev[i] = (uint8_t)(nl == 0 || (nl == 1 && waterE(gx, gy) < ELEV_SEA) ? 0 : nl);
          }
        }
      }
  }
}

// 6: the rivers' beds (a river never climbs a step)
void EndlessSource::Impl::chunkRiverBeds(ChunkCtx& X) {
  const int32_t x0 = X.x0;
  const int32_t y0 = X.y0;
  const BaseRect& B = X.B;
  std::vector<uint8_t>& lev = X.lev;
  std::vector<uint8_t>& lakeL = X.lakeL;
  std::vector<uint8_t>& rivG = X.rivG;
  std::vector<uint8_t>& flowG = X.flowG;
  // (M11, VISION_PLAN 11.3 and 15.32: a river never climbs a step) the rivers' beds: every hydro segment near the
  // grid is walked from its start with its surface level (RiverSeg::lv) falling to the relief level of each tile it
  // passes (fineLevel, the base level: a pure function, so every chunk walks a segment alike); the river's tiles take
  // the lower of their own level and the river's. Where the land rises across a river's course the bed is cut down
  // through it (a gorge with the banks as faces); where the river drops, its water pours over the step (a fall).
  // rivG: the river's tiles on the grid (the sliver passes leave them alone); flowG: the flow there (1 +x, 2 -x,
  // 3 +y, 4 -y: the segment's dominant axis).
  rivG.assign((size_t)LW * LW, 0);
  flowG.assign((size_t)LW * LW, 0);
  {
    const int32_t lx0 = x0 - LB, ly0 = y0 - LB;
    std::vector<uint8_t> rl((size_t)LW * LW, 255);
    auto baseLv = [&](int32_t gx, int32_t gy) {
      if (gx >= lx0 && gy >= ly0 && gx < lx0 + LW && gy < ly0 + LW) {
        const size_t i = (size_t)(gy - ly0) * LW + (gx - lx0);
        if (!lakeL[i]) return (int)lev[i];
      }
      return fineLevel(gx, gy);
    };
    std::vector<GTile> P;
    std::vector<uint8_t> Ps;
    for (int32_t ry = regionOf(ly0 - 4); ry <= regionOf(ly0 + LW + 3); ry++)
      for (int32_t rx = regionOf(lx0 - 4); rx <= regionOf(lx0 + LW + 3); rx++) {
        std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
        for (const RiverSeg& s : H->segs) {
          if (std::max(s.x0, s.x1) + 3 < lx0 || std::min(s.x0, s.x1) - 3 >= lx0 + LW || std::max(s.y0, s.y1) + 3 < ly0 || std::min(s.y0, s.y1) - 3 >= ly0 + LW) continue;
          const int lo = s.w >= 3 ? -1 : 0, hi = s.w == 1 ? 0 : s.w == 4 ? 2 : 1;
          const int32_t ddx = s.x1 - s.x0, ddy = s.y1 - s.y0;
          const uint8_t fl = (uint8_t)(std::abs(ddx) >= std::abs(ddy) ? (ddx >= 0 ? 1 : 2) : (ddy >= 0 ? 3 : 4));
          // every way the water runs here (high nibble: 1 +x, 2 -x, 4 +y, 8 -y): the main axis, and the other where the
          // segment slants by a third or more
          uint8_t dirs = 0;
          if (3 * std::abs(ddx) >= std::abs(ddy) && ddx) dirs |= ddx > 0 ? 1 : 2;
          if (3 * std::abs(ddy) >= std::abs(ddx) && ddy) dirs |= ddy > 0 ? 4 : 8;
          const uint8_t flw = (uint8_t)(fl | dirs << 4);
          // the surface down the segment: falling with the land from lv, never below lvEnd (the next segment's start)
          P.clear();
          walk4(s.x0, s.y0, s.x1, s.y1, [&](int32_t x, int32_t y) { P.push_back(GTile{x, y}); });
          Ps.assign(P.size(), 0);
          int cur = s.lv;
          for (size_t k = 0; k < P.size(); k++) {
            if (cur > s.lvEnd) cur = std::max<int>(s.lvEnd, std::min(cur, baseLv(P[k].x, P[k].y)));
            Ps[k] = (uint8_t)cur;
          }
          // two drops within 6 tiles become one fall (taller) at the first: no pool shut in between two falls
          {
            int nd = (int)P.size(), ndv = s.lvEnd;
            if (Ps.empty() || ndv >= Ps.back()) nd = -1;
            for (int k = (int)P.size() - 1; k >= 1; k--) {
              if (Ps[(size_t)k] >= Ps[(size_t)k - 1]) continue;
              if (nd >= 0 && nd - k < 6) {
                for (int q = k; q < nd; q++) Ps[(size_t)q] = (uint8_t)ndv;
              } else ndv = Ps[(size_t)k];
              nd = k;
            }
            if (nd >= 0 && nd < 6 && !Ps.empty()) {   // (a drop at the start: the segment before carries it)
              for (int q = 0; q < nd; q++) Ps[(size_t)q] = (uint8_t)ndv;
            }
          }
          for (size_t k = 0; k < P.size(); k++) {
            const int32_t x = P[k].x, y = P[k].y;
            for (int oy = lo; oy <= hi; oy++)
              for (int ox = lo; ox <= hi; ox++) {
                const int32_t gx = x + ox, gy = y + oy;
                if (gx < lx0 || gy < ly0 || gx >= lx0 + LW || gy >= ly0 + LW) continue;
                const size_t i = (size_t)(gy - ly0) * LW + (gx - lx0);
                // (the sea is the sea: tile()'s own test, so the ring beyond the base rect agrees with it)
                if (B.in(gx, gy) ? B.biome[B.at(gx, gy)] == (uint8_t)Biome::Ocean : (lev[i] == 0 && waterE(gx, gy) < ELEV_SEA)) continue;
                if (Ps[k] < rl[i]) { rl[i] = Ps[k]; flowG[i] = flw; }
                else if (Ps[k] == rl[i]) flowG[i] |= (uint8_t)(dirs << 4);
                rivG[i] = 1;
              }
          }
        }
      }
    // the river's surface on its tiles; a lake keeps its own level where a river runs into it or out of it, and the
    // river within 2 tiles of a lake runs at the lake's level at least (a lake spills over in a fall a little way down
    // its outflow, never sideways along a shore)
    for (size_t i = 0; i < rl.size(); i++)
      if (rivG[i] && rl[i] != 255 && !lakeL[i]) lev[i] = rl[i];
    for (int y = 0; y < LW; y++)
      for (int x = 0; x < LW; x++) {
        const size_t i = (size_t)y * LW + x;
        if (!rivG[i] || lakeL[i]) continue;
        int lk = -1;
        for (int oy = -2; oy <= 2; oy++)
          for (int ox = -2; ox <= 2; ox++) {
            const int xx = x + ox, yy = y + oy;
            if (xx < 0 || yy < 0 || xx >= LW || yy >= LW) continue;
            const size_t j = (size_t)yy * LW + xx;
            if (lakeL[j]) lk = std::max(lk, (int)lev[j]);
          }
        if (lk > (int)lev[i]) lev[i] = (uint8_t)lk;
      }
  }
}

// 6: the summits' level, the places' flats and their terraces
void EndlessSource::Impl::chunkFlats(ChunkCtx& X) {
  const int32_t x0 = X.x0;
  const int32_t y0 = X.y0;
  const std::shared_ptr<const RegionData>* RD = X.RD;
  std::vector<uint8_t>& lev = X.lev;
  std::vector<uint8_t>& lakeL = X.lakeL;
  std::vector<uint8_t>& rivG = X.rivG;
  std::vector<uint8_t>& terr = X.terr;
  std::vector<uint8_t>& coreG = X.coreG;
  // (M11 fixer r2) level 7 is the summits' own: the land itself tops out at level 6, and only a summit shrine's crown
  // (or a place's flat the plans set on the top level) stands on 7. The big ranges' crests used to run out as broad
  // level-7 fields, and a summit shrine on one could get no crown at all (its tiers cannot rise past 7): it stood on a
  // flat white plain. Now every summit stands at least one tier over the top of its mountain. (Clamped after the
  // rivers and lakes, so a river, its banks and a lake on the crest stay level with each other.)
  for (uint8_t& v : lev) if (v > 6) v = 6;
  struct Flat { int32_t cx2, cy2, hw2, hh2, rc; int lv, step; uint64_t wob; bool crown = false; };   // centre and half sizes doubled
  std::vector<Flat> flats;
  // (M3c) the tiles inside some flat's terraces: their faces get a ramp twice as often (every 8 tiles, not 16), so a
  // knoll the terraces trim down to a small pocket of its old level is never left walled in by cliffs
  terr.assign((size_t)LW * LW, 0);
  // (M11) the tiles on some place's own flat (its footprint and the first terrace): no waterfall there (a town drains
  // the rivers that end in it, so a fall at its edge would pour into a stub)
  coreG.assign((size_t)LW * LW, 0);
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
      // (M11 fixer r1) a summit shrine stands on a crown: its floor two levels over the flat, the first terrace one
      // (never above level 7), so the last of the climb is a stairway up two tiers to the shrine
      f.crown = p.type == SiteType::Shrine && (p.flags & SPF_SUMMIT) != 0;
      if (f.crown) f.step = 3;   // (its terraces 3 tiles apart: a stepped top, not a broad flat 20 tiles across)
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
        int lo = 0, hi = 7, bestD = INT32_MAX, bestLv = -1, crownLv = -1;
        bool any = false, core = false;
        for (const Flat& f : flats) {
          // distance (tiles) outside the rounded rectangle, plus a slow wobble of +-5 tiles
          int32_t qx = std::max(0, std::abs(2 * gx + 1 - f.cx2) - (f.hw2 - f.rc)), qy = std::max(0, std::abs(2 * gy + 1 - f.cy2) - (f.hh2 - f.rc));
          int32_t out2 = (int32_t)isqrt((uint64_t)qx * qx + (uint64_t)qy * qy) - f.rc;   // doubled units
          if (out2 / 2 - 12 >= 4 * f.step) continue;   // (beyond its terraces whatever the wobble: skip the noise)
          // (two octaves, +-12 tiles: with one gentle octave the terraces ran as long straight lines parallel to
          // the town's rectangle, the "grid crack" through open plain)
          int32_t wob = (int32_t)(((int64_t)(vnoiseQ(gx, gy, 5, f.wob) - 32768) * 18) >> 16) +
                        (int32_t)(((int64_t)(vnoiseQ(gx, gy, 3, f.wob ^ 0x5A5Au) - 32768) * 6) >> 16);
          if (f.crown) wob /= 3;   // (the summit's tiers keep close to its floor)
          int32_t d = std::max(0, out2 / 2 + wob);
          int dev = d / f.step;
          if (dev >= 4) continue;
          any = true;
          if (dev == 0) core = true;
          // (the crown's own tiers: tight round the floor, its edge wobbling a little: the top tier reaches 3 tiles
          // past the floor's pad, the next 4 more, so both stand in view of the shrine)
          if (f.crown) {
            // (M11 fixer r2) the crown reaches level 7 whatever the mountain's top (the land stops at 6): two tiers
            // over a crest of level 5 or lower, one over a crest of 6 (a floor and a first tier both on 7 made a
            // broad flat field instead of a crown)
            const int32_t cd = std::max(0, out2 / 2 + wob / 4);
            const int top = std::min(7, f.lv + 2);
            if (cd < 3) crownLv = std::max(crownLv, top);
            else if (cd < 7 && top - 1 > f.lv) crownLv = std::max(crownLv, top - 1);
          }
          lo = std::max(lo, f.lv - dev);
          hi = std::min(hi, f.lv + dev);
          if (d < bestD) { bestD = d; bestLv = f.lv; }
        }
        if (!any) continue;
        terr[(size_t)y * LW + x] = 1;
        coreG[(size_t)y * LW + x] = core;
        uint8_t& L = lev[(size_t)y * LW + x];
        // (M11) a river's bed is never raised by a place's terraces (it would climb onto them); on the place's own
        // flat it runs at the flat's level (its bridges stay level with the streets)
        if (rivG[(size_t)y * LW + x] && !core) { if (lo <= hi) L = (uint8_t)std::min((int)L, hi); }
        else if (lo <= hi) L = (uint8_t)std::clamp((int)L, lo, hi);
        else L = (uint8_t)bestLv;
        if (crownLv > (int)L && !rivG[(size_t)y * LW + x] && !lakeL[(size_t)y * LW + x]) L = (uint8_t)crownLv;
      }
}

// 6: the knobs, the sliver passes, the cliff and ramp bits, the knolls' ways up, the plunge pools, the
// mountains' rock and the towns' own relief
void EndlessSource::Impl::chunkReliefBits(ChunkCtx& X) {
  ChunkData& c = X.c;
  const int32_t cx = X.cx;
  const int32_t cy = X.cy;
  const int32_t x0 = X.x0;
  const int32_t y0 = X.y0;
  const BaseRect& B = X.B;
  std::vector<uint8_t>& reserved = X.reserved;
  std::vector<uint8_t>& used = X.used;
  std::vector<uint8_t>& road = X.road;
  std::vector<uint8_t>& entrance = X.entrance;
  std::vector<uint8_t>& trailT = X.trailT;
  std::vector<uint8_t>& townH = X.townH;
  std::vector<uint8_t>& lev = X.lev;
  std::vector<uint8_t>& lakeL = X.lakeL;
  std::vector<uint8_t>& rivG = X.rivG;
  std::vector<uint8_t>& flowG = X.flowG;
  std::vector<uint8_t>& terr = X.terr;
  std::vector<uint8_t>& coreG = X.coreG;
  std::vector<uint8_t>& bits = X.bits;
  auto rin = [&X](int32_t gx, int32_t gy) { return X.rin(gx, gy); };
  auto ri = [&X](int32_t gx, int32_t gy) { return X.ri(gx, gy); };
  // cliff and ramp bits on the 1-tile border grid
  bits.assign(RW * RW, 0);
  // (M11) a chunk whose levels are one level all through (with a tile round it) has no step: the relief passes below
  // (knobs, landings, knolls) skip it (the sliver passes never make a step where there was none)
  bool anyStep = false;
  {
    const int l0 = lev[(size_t)(LB - 2) * LW + (LB - 2)];
    for (int32_t gy = y0 - 2; gy < y0 + CHUNK + 2 && !anyStep; gy++)
      for (int32_t gx = x0 - 2; gx < x0 + CHUNK + 2; gx++) if ((int)lev[(size_t)(gy - (y0 - LB)) * LW + (gx - (x0 - LB))] != l0) { anyStep = true; break; }
  }
  // (M11, 11.4: the plateau minimum) a raised knob of under 24 tiles (one level, every tile round it lower, off the
  // water) lying wholly inside this chunk at least 2 tiles from its edges is flattened into its surroundings (the
  // highest level round it), before the sliver passes (which then tidy what it leaves). No other chunk reads those
  // tiles' levels, so the decision is this chunk's alone; a knob nearer an edge keeps its level and gets a way up
  // below (the knoll pass).
  if (anyStep) {
    auto LI0 = [&](int32_t gx, int32_t gy) { return (size_t)(gy - (y0 - LB)) * LW + (gx - (x0 - LB)); };
    auto L0 = [&](int32_t gx, int32_t gy) { return (int)lev[LI0(gx, gy)]; };
    static const int DX4[4] = {1, -1, 0, 0}, DY4[4] = {0, 0, 1, -1};
    std::vector<uint8_t> seenK(ChunkData::N, 0);
    std::vector<int> comp;
    for (int sy = 2; sy < CHUNK - 2; sy++)
      for (int sx = 2; sx < CHUNK - 2; sx++) {
        if (seenK[(size_t)sy * CHUNK + sx]) continue;
        const int l = L0(x0 + sx, y0 + sy);
        if (l == 0) continue;
        comp.assign(1, sy * CHUNK + sx);
        seenK[(size_t)sy * CHUNK + sx] = 1;
        bool ok = true;
        int around = -1;
        for (size_t h = 0; h < comp.size() && ok; h++) {
          const int qx = comp[h] % CHUNK, qy = comp[h] / CHUNK;
          if (qx < 2 || qy < 2 || qx >= CHUNK - 2 || qy >= CHUNK - 2 || comp.size() >= 24 || lakeL[LI0(x0 + qx, y0 + qy)] ||
              rivG[LI0(x0 + qx, y0 + qy)] || B.lake[B.at(x0 + qx, y0 + qy)] || B.riverW[B.at(x0 + qx, y0 + qy)] || waterG(B.ground[B.at(x0 + qx, y0 + qy)])) { ok = false; break; }
          for (int k = 0; k < 4; k++) {
            const int nx = qx + DX4[k], ny = qy + DY4[k];
            const int ln = L0(x0 + nx, y0 + ny);
            if (ln > l) { ok = false; break; }
            if (ln < l) { around = std::max(around, ln); continue; }
            if (!seenK[(size_t)ny * CHUNK + nx]) { seenK[(size_t)ny * CHUNK + nx] = 1; comp.push_back(ny * CHUNK + nx); }
          }
        }
        if (!ok || around < 0) continue;
        for (int q : comp) lev[LI0(x0 + q % CHUNK, y0 + q / CHUNK)] = (uint8_t)around;
      }
  }
  // the sliver passes (pass k is exact for tiles within LB - k of the chunk)
  for (int pass = 1; pass <= LB - 3; pass++) {
    std::vector<uint8_t> nl = lev;
    for (int y = pass; y < LW - pass; y++)
      for (int x = pass; x < LW - pass; x++) {
        const size_t i = (size_t)y * LW + x;
        if (lakeL[i] || rivG[i]) continue;
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
        // (M11) a 1-deep terrace running on the diagonal (higher on two adjacent sides, lower on the other two): it
        // joins its upper step, a stacked face, like the straight ones
        else if (hi == 2 && lo == 2 && !(n > l && s > l) && !(w > l && e > l)) to = minHi;
        nl[i] = (uint8_t)to;
      }
    lev.swap(nl);
  }
  // (M11) a pass changes every tile at once, so a 1-deep terrace raised into its upper step while that step's tip was
  // lowered leaves a 1-tile island (or pit) behind: within 2 tiles of the chunk, where the levels round it are exact,
  // it joins its surroundings (the highest level round an island, the lowest round a pit)
  {
    std::vector<std::pair<size_t, uint8_t>> fix;
    for (int y = LB - 2; y < LB + CHUNK + 2; y++)
      for (int x = LB - 2; x < LB + CHUNK + 2; x++) {
        const size_t i = (size_t)y * LW + x;
        if (lakeL[i] || rivG[i]) continue;
        const int l = lev[i], n = lev[i - LW], s = lev[i + LW], w = lev[i - 1], e = lev[i + 1];
        if (n < l && s < l && w < l && e < l) fix.push_back({i, (uint8_t)std::max(std::max(n, s), std::max(w, e))});
        else if (n > l && s > l && w > l && e > l) fix.push_back({i, (uint8_t)std::min(std::min(n, s), std::min(w, e))});
        else {
          // (M11 integration) the last pass's own leftovers, seen by --endless on seeds 6 and 8 (10 slivers a window):
          // a 1-tile notch or finger (three sides on one other level) joins them, a 1-deep terrace its upper step
          const int nb[4] = {n, s, w, e};
          int hi = 0, lo = 0, minHi = 99, maxLo = -1;
          for (int k = 0; k < 4; k++) {
            if (nb[k] > l) { hi++; minHi = std::min(minHi, nb[k]); }
            if (nb[k] < l) { lo++; maxLo = std::max(maxLo, nb[k]); }
          }
          if (hi >= 3) fix.push_back({i, (uint8_t)minHi});
          else if (lo >= 3) fix.push_back({i, (uint8_t)maxLo});
          else if ((n > l && s < l) || (n < l && s > l)) fix.push_back({i, (uint8_t)std::max(n, s)});
          else if ((w > l && e < l) || (w < l && e > l)) fix.push_back({i, (uint8_t)std::max(w, e)});
        }
      }
    for (const auto& f : fix) lev[f.first] = f.second;
  }
  auto LI = [&](int32_t gx, int32_t gy) { return (size_t)(gy - (y0 - LB)) * LW + (gx - (x0 - LB)); };
  auto L = [&](int32_t gx, int32_t gy) { return (int)lev[LI(gx, gy)]; };
  const uint64_t rampSeed = seed ^ tag("relief.ramp");
  static const int DX4[4] = {1, -1, 0, 0}, DY4[4] = {0, 0, 1, -1};
  // the base rect's water at a tile of the 1-tile border grid: a river, a lake or the land's own water (not a footbridge)
  auto wetAtB = [&](int32_t gx, int32_t gy) {
    const size_t bi = B.at(gx, gy);
    return !B.bridge[bi] && (B.lake[bi] || B.riverW[bi] || waterG(B.ground[bi]));
  };
  // the ramp lattice (11.4): a south / north face's ways up at x = hash(row block) mod 16 (every 8 inside a place's
  // terraces, and (M11) on the mountains' upper tiers, level 5 and up, where the crests are small), an east / west
  // face's at y = hash(column block) mod 16; 3 tiles wide
  auto lattice = [&](int32_t gx, int32_t gy, bool ns) {
    const int per = terr[LI(gx, gy)] || L(gx, gy) >= 5 ? 7 : 15;
    if (ns) return ((gx + (int32_t)(tileHash(rampSeed, 0, gy >> 4) & 15)) & per) < 3;
    return ((gy + (int32_t)(tileHash(rampSeed, 1, gx >> 4) & 15)) & per) < 3;
  };
  // (M11, 11.3 and 11.4: stairs and switchbacks) a way up a stacked face (2 levels in one tile row) is a stairway in two
  // flights with a landing: the way-up tile (a lattice point, or a road or track crossing the face) is raised one level,
  // so it climbs the upper level from the landing and the tile below it (forced to a ramp) climbs to the landing. A
  // pure function of the levels within 2 tiles, which are exact here, so the chunks on both sides agree.
  // (after the sliver passes, which may carry a step a few tiles in from the ring: the test again for the passes below;
  // anyStacked: a face of 2 levels or more within 2 tiles (the landings); maxL: the highest level there (the knolls'
  // pass runs on the high ground, level 4 and up, where the ridges and the stacked tiers make knolls the lattice misses)
  bool anyStacked = false;
  int maxL = 0;
  {
    const int l0 = L(x0 - 2, y0 - 2);
    anyStep = false;
    for (int32_t gy = y0 - 2; gy < y0 + CHUNK + 2; gy++)
      for (int32_t gx = x0 - 2; gx < x0 + CHUNK + 2; gx++) {
        const int l = L(gx, gy);
        maxL = std::max(maxL, l);
        if (l != l0) anyStep = true;
        if (gx + 1 < x0 + CHUNK + 2 && std::abs(L(gx + 1, gy) - l) >= 2) anyStacked = true;
        if (gy + 1 < y0 + CHUNK + 2 && std::abs(L(gx, gy + 1) - l) >= 2) anyStacked = true;
      }
  }
  std::vector<uint8_t> forceRamp(RW * RW, 0);
  if (anyStacked) {
    std::vector<std::pair<size_t, uint8_t>> raise;
    for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
      for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++) {
        const int l = L(gx, gy);
        if (entrance[ri(gx, gy)] || wetAtB(gx, gy) || rivG[LI(gx, gy)]) continue;
        for (int k = 0; k < 4; k++) {
          const int32_t hx = gx + DX4[k], hy = gy + DY4[k];
          if (L(hx, hy) < l + 2) continue;
          const bool roadHere = road[ri(gx, gy)] != 0;
          if (!roadHere && !lattice(gx, gy, k >= 2)) continue;
          // the tile below the landing must be dry land of this level, clear of the face
          const int32_t sx = gx - DX4[k], sy = gy - DY4[k];
          if (L(sx, sy) != l || rivG[LI(sx, sy)] || (B.in(sx, sy) && wetAtB(sx, sy))) continue;
          raise.push_back({LI(gx, gy), (uint8_t)(l + 1)});
          if (rin(sx, sy)) forceRamp[ri(sx, sy)] = 1;
          forceRamp[ri(gx, gy)] = 1;
          break;
        }
      }
    for (const auto& r : raise) lev[r.first] = r.second;
  }
  // the way the water runs at a grid tile (from flowG), as a step
  auto flowDX = [&](size_t i) { const uint8_t f = flowG[i] & 15; return f == 1 ? 1 : f == 2 ? -1 : 0; };
  auto flowDY = [&](size_t i) { const uint8_t f = flowG[i] & 15; return f == 3 ? 1 : f == 4 ? -1 : 0; };
  // does the water at grid tile i run one step (dx, dy)?
  auto flowsTo = [&](size_t i, int dx, int dy) {
    const uint8_t d = flowG[i] >> 4;
    return (dx > 0 && (d & 1)) || (dx < 0 && (d & 2)) || (dy > 0 && (d & 4)) || (dy < 0 && (d & 8));
  };
  // the river runs on below a fall for 4 tiles at least (its own course, tile by tile; or a lake takes it): a river that
  // ends just below a step (a stub) pours into no fall
  auto runsOn = [&](int32_t gx, int32_t gy) {
    size_t i = LI(gx, gy);
    int fx = flowDX(i), fy = flowDY(i);
    for (int s = 0; s < 4; s++) {
      if (!fx && !fy) return false;
      gx += fx; gy += fy;
      i = LI(gx, gy);
      if (lakeL[i]) return true;
      if (!rivG[i]) return false;
      if (flowG[i]) { fx = flowDX(i); fy = flowDY(i); }
    }
    return true;
  };
  // the hillside test (11.2: "a slope is where H changes by 1 and the 4-tile slope is below the threshold"), judged
  // once per 6 x 6 cell of the land at the cell's middle, so a hillside runs on unbroken instead of flickering
  // between bank and slope tile by tile: the clean elevation climbs less than 0.4 of a level over 6 tiles
  struct GentleMemo { int32_t cx = INT32_MIN, cy = 0; bool v = false; };
  GentleMemo gm[16];
  // (M11 fixer r1) judged per 16 x 16 cell over 16 tiles (the same gradient threshold): on 6-tile cells a bank changed
  // between face and hillside every few tiles and read as a row of separate logs and mounds
  auto gentle = [&](int32_t gx, int32_t gy) {
    const int32_t cxx = floorDiv(gx, 16), cyy = floorDiv(gy, 16);
    GentleMemo& m = gm[(size_t)((cxx & 3) | (cyy & 3) << 2)];
    if (m.cx != cxx || m.cy != cyy) {
      const int32_t mx = cxx * 16 + 8, my = cyy * 16 + 8;
      const int32_t ex = std::abs(reliefE(mx + 8, my) - reliefE(mx - 8, my)), ey = std::abs(reliefE(mx, my + 8) - reliefE(mx, my - 8));
      m.cx = cxx; m.cy = cyy; m.v = ex + ey < H_STEP * 16 / 15;
    }
    return m.v;
  };
  for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
    for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++) {
      const int l = L(gx, gy);
      const int ln = L(gx, gy - 1), ls = L(gx, gy + 1), lw = L(gx - 1, gy), le = L(gx + 1, gy);
      uint8_t b = (uint8_t)l;
      size_t bi = (size_t)(gy - (y0 - BR)) * BW + (gx - (x0 - BR));
      const bool water = wetAtB(gx, gy);
      // (M11) a road's or track's deck over the water (a bridge, a ford) is a way, not water: where it meets a step it
      // climbs by stairs like the road on land
      const bool deck = water && road[ri(gx, gy)] != 0;
      const bool wet = water && !deck;
      if (ln > l || ls > l || lw > l || le > l) {
        bool ramp = false;
        if (!wet && !entrance[ri(gx, gy)]) {
          ramp = lattice(gx, gy, ln > l || ls > l);
          // (M3c) a mesa's stairway up its south side
          if (!ramp && ln > l && mesaStair(gx, gy)) ramp = true;
          // roads and tracks climb by stairs: the road's own tiles only (M11 fixer r1: the flanking tiles took stairs
          // too, so a flight ran three times the road's width and the road met it at a corner; a 4-connected road
          // always crosses a step on a road tile of its own)
          if (road[ri(gx, gy)]) ramp = true;
          if (forceRamp[ri(gx, gy)] || B.bridge[bi]) ramp = true;   // (a landing's flights; a footbridge's deck)
          // (M11 fixer r1, 11.2) a way up climbs one level: a ramp beside a tile 2 or more levels higher (a lattice
          // point, a road's flank or a stair's end whose landing could not be raised above) would let a walker skip
          // the landing, so it stays a face
          if (ramp && std::max(std::max(ln, ls), std::max(lw, le)) >= l + 2) ramp = false;
        }
        // (M11, VISION_PLAN 15.32) a road's or track's way up is built stone stairs (HEIGHT_STAIR); a gentle one-level
        // step of the lowlands and hills, away from any settlement's terraces, is a walkable hillside (HEIGHT_SLOPE)
        // where the land is gentle (below); a river's water pouring over the step
        // into this tile is a waterfall (HEIGHT_FALL, with the cliff: never under a bridge, a ford or a road, never on
        // the sea)
        bool roadUp = false, slope = false, fall = false;
        // (M11 fixer r2) built stone stairs where a paved road or a summit's trail climbs; a plain dirt track's way up
        // is a worn earth ramp (its own trodden earth: a second stone flight on a loose dirt track two tiles from the
        // road's stair read as a stair-bank-stair gate)
        if (ramp) roadUp = road[ri(gx, gy)] >= 2 || trailT[ri(gx, gy)] != 0 || (road[ri(gx, gy)] && (deck || B.bridge[bi]));
        const int hiN = std::max(std::max(ln, ls), std::max(lw, le));
        if (!ramp && !wet && !deck && !entrance[ri(gx, gy)] && hiN == l + 1 && hiN <= 3 && !terr[LI(gx, gy)]) {
          slope = gentle(gx, gy);
        }
        if (wet && B.biome[bi] != (uint8_t)Biome::Ocean && (B.riverW[bi] || B.lake[bi]) && !coreG[LI(gx, gy)]) {
          const size_t ti = LI(gx, gy);
          for (int k = 0; k < 4 && !fall; k++) {
            const int32_t ux = gx + DX4[k], uy = gy + DY4[k];
            const size_t ui = LI(ux, uy);
            if (lev[ui] <= l || !B.in(ux, uy)) continue;
            const size_t ub = B.at(ux, uy);
            if (B.bridge[ub] || (rin(ux, uy) && road[ri(ux, uy)])) continue;   // (no fall from under a bridge or a ford)
            if (B.lake[ub] || !B.riverW[ub] || !rivG[ui]) continue;            // the water above: the river's own course
            // the river above flows into this tile, and runs on below it (a lake takes it, or 4 tiles of river)
            fall = flowsTo(ui, -DX4[k], -DY4[k]) && (B.lake[bi] || (rivG[ti] && runsOn(gx, gy)));
          }
        }
        if (entrance[ri(gx, gy)]) b = (uint8_t)l;
        else if (slope) b |= Map::HEIGHT_SLOPE;
        else {
          b |= ramp ? Map::HEIGHT_RAMP : Map::HEIGHT_CLIFF;
          if (roadUp) b |= Map::HEIGHT_STAIR;
          if (fall && !ramp) b |= Map::HEIGHT_FALL;
        }
      }
      bits[ri(gx, gy)] = b;
    }
  // (M11, 11.4: every plateau reachable) a knoll or crest the lattice missed: a raised component (one level, every
  // tile round it lower) that lies within 4 tiles of this chunk and has no way up (no ramp or hillside on the tiles
  // below its edge that this chunk knows) gets one: its edge tile in this chunk with the lowest hash becomes a ramp.
  // Each chunk decides for its own tiles only, so a knoll across a chunk edge may get a way up from both sides; the
  // levels 3 and 4 tiles out are not exact here, which can only add a needless ramp or miss one the lattice (every 8
  // tiles on the upper tiers) all but always gives.
  if (anyStep && maxL >= 4) {
    constexpr int E = 4, EW = CHUNK + 2 * E;
    std::vector<uint8_t> seenK((size_t)EW * EW, 0);
    std::vector<int32_t> comp, edgeT;
    for (int sy = 0; sy < EW; sy++)
      for (int sx = 0; sx < EW; sx++) {
        if (seenK[(size_t)sy * EW + sx]) continue;
        const int32_t gx0 = x0 - E + sx, gy0 = y0 - E + sy;
        const int l = L(gx0, gy0);
        if (l == 0) continue;
        comp.assign(1, sy * EW + sx);
        seenK[(size_t)sy * EW + sx] = 1;
        bool whole = true, raised = true, way = false;
        edgeT.clear();
        for (size_t h = 0; h < comp.size() && raised; h++) {
          const int qx = comp[h] % EW, qy = comp[h] / EW;
          const int32_t gx = x0 - E + qx, gy = y0 - E + qy;
          // (water: an island, a lake's level, not a knoll; the base rect holds 2 tiles round the chunk, the grid's
          // lake and river marks the rest)
          if (B.in(gx, gy) ? wetAtB(gx, gy) : (lakeL[LI(gx, gy)] || rivG[LI(gx, gy)])) { raised = false; break; }
          for (int k = 0; k < 4; k++) {
            const int nx = qx + DX4[k], ny = qy + DY4[k];
            const int32_t ngx = gx + DX4[k], ngy = gy + DY4[k];
            const int ln = L(ngx, ngy);
            if (ln > l) { raised = false; break; }
            if (ln == l) {
              if (nx < 0 || ny < 0 || nx >= EW || ny >= EW) { whole = false; continue; }
              if (!seenK[(size_t)ny * EW + nx]) { seenK[(size_t)ny * EW + nx] = 1; comp.push_back(ny * EW + nx); }
              continue;
            }
            // a tile below the edge: its step bit is a way up, or a candidate for one
            if (rin(ngx, ngy)) {
              const uint8_t nb = bits[ri(ngx, ngy)];
              if (nb & (Map::HEIGHT_RAMP | Map::HEIGHT_SLOPE)) way = true;
              else if ((nb & Map::HEIGHT_CLIFF) && !(nb & Map::HEIGHT_FALL) && !wetAtB(ngx, ngy) && !entrance[ri(ngx, ngy)] &&
                       ngx >= x0 && ngy >= y0 && ngx < x0 + CHUNK && ngy < y0 + CHUNK &&
                       // (M11 fixer r1) a way up climbs one level: never beside a tile 2 levels higher
                       std::max(std::max(L(ngx + 1, ngy), L(ngx - 1, ngy)), std::max(L(ngx, ngy + 1), L(ngx, ngy - 1))) == ln + 1)
                edgeT.push_back(ri(ngx, ngy));
            }   // (beyond the 1-tile border its bit is not known here: another way up there only adds one)
          }
        }
        if (!raised || !whole || way || edgeT.empty() || comp.size() > 400) continue;
        const uint64_t ks = rampSeed ^ 0x6B4Eu;
        int32_t best = edgeT[0];
        uint64_t bh = UINT64_MAX;
        for (int32_t t : edgeT) {
          const int32_t tx = x0 - 1 + t % RW, ty = y0 - 1 + t / RW;
          const uint64_t hh = tileHash(ks, tx, ty);
          if (hh < bh) { bh = hh; best = t; }
        }
        bits[(size_t)best] = (uint8_t)((bits[(size_t)best] & ~Map::HEIGHT_CLIFF) | Map::HEIGHT_RAMP);
      }
  }
  // (M11) the plunge pool: the river tile a fall pours into, and its neighbours across the stream, are open deep water
  // (never a bridge, a ford or a road)
  for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
    for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++) {
      if (!(bits[ri(gx, gy)] & Map::HEIGHT_FALL)) continue;
      const size_t ti = LI(gx, gy);
      const int fx = flowDX(ti), fy = flowDY(ti);
      if (!fx && !fy) continue;
      for (int side = -1; side <= 1; side++) {
        const int32_t px = gx + fx + side * fy, py = gy + fy + side * fx;
        if (px < x0 || py < y0 || px >= x0 + CHUNK || py >= y0 + CHUNK) continue;
        const size_t pb = B.at(px, py);
        if (!B.riverW[pb] || B.bridge[pb] || road[ri(px, py)] || (bits[ri(px, py)] & (Map::HEIGHT_FALL | Map::HEIGHT_RAMP))) continue;
        const int pi = c.at(px - x0, py - y0);
        if (groundWater((Ground)c.ground[pi])) c.ground[pi] = (uint8_t)Ground::DeepWater;
      }
    }
  // (M11, 11.2: rock only on the steep cores, drawn as stacked cliffs) the mountains' rock stays only on their stacked
  // faces (macro.cpp groundFor makes the rest of their high ground walkable scree, turf and snow)
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) {
      const int32_t gx = x0 + lx, gy = y0 + ly;
      const uint8_t hb = bits[ri(gx, gy)];
      if (!(hb & Map::HEIGHT_CLIFF) || (hb & Map::HEIGHT_FALL)) continue;
      const int i = c.at(lx, ly);
      if (c.eco[i] != (uint8_t)Eco::Mountain || reserved[(size_t)i] || used[ri(gx, gy)] || groundWater((Ground)c.ground[i])) continue;
      const int l = hb & Map::HEIGHT_LEVEL;
      bool stacked = false;
      for (int k = 0; k < 4 && !stacked; k++) stacked = L(gx + DX4[k], gy + DY4[k]) >= l + 2;
      if (stacked && l >= 4) c.ground[i] = (uint8_t)Ground::Rock;
    }
  // (M3) the towns' own relief on their tiles (the terraces of a terraced town; elsewhere identical)
  for (size_t k = 0; k < townH.size(); k++) if (townH[k]) bits[k] = (uint8_t)(townH[k] - 1);
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) c.height[c.at(lx, ly)] = bits[ri(x0 + lx, y0 + ly)];
}

}  // namespace ew
