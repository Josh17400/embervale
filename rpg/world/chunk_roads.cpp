// L2b, the chunk pipeline's step 5 (SOLID 3.2: moved from chunkgen.cpp, unchanged): the roads and tracks near a chunk,
// their bridges and fords, the toll bridges. WORLD lane; gen::ChunkCtx carries the chunk's state between the stages.

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
}  // namespace

// 5: the roads and tracks, clipped at settlement footprints; bridges, fords and the toll bridges
void EndlessSource::Impl::chunkRoads(ChunkCtx& X) {
  ChunkData& c = X.c;
  const int32_t x0 = X.x0;
  const int32_t y0 = X.y0;
  const BaseRect& B = X.B;
  std::vector<uint8_t>& reserved = X.reserved;
  std::vector<uint8_t>& used = X.used;
  std::vector<uint8_t>& road = X.road;
  std::vector<uint8_t>& trailT = X.trailT;
  const std::shared_ptr<const RegionData>* RD = X.RD;
  const RegionData& own = *X.RD[4];
  auto rin = [&X](int32_t gx, int32_t gy) { return X.rin(gx, gy); };
  auto ri = [&X](int32_t gx, int32_t gy) { return X.ri(gx, gy); };
  // 5: roads (this region's graph roads) and tracks (every nearby site's spur), clipped at settlement footprints
  struct Clip { Gid id; int32_t x0, y0, x1, y1; };
  std::vector<Clip> clips;
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites) {
      // (M11 fixer r1: a summit shrine's trail runs up to its floor, the trodden earth before the shrine)
      int pad = isSettlement(p.type) || (p.type == SiteType::Shrine && (p.flags & SPF_SUMMIT)) ? 0 : 1;
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
    // (M3b fixer) the road's raster near the chunk, in unbroken pieces, with a wide margin so a crossing is judged
    // whole (every chunk sees the same crossing and lays the same deck)
    const int32_t MG = 64;
    std::vector<std::vector<GTile>> pieces;
    bool open = false;
    for (size_t n = 0; n + 1 < rp.pts.size(); n++) {
      const GTile &a = rp.pts[n], &b = rp.pts[n + 1];
      if (std::max(a.x, b.x) + 2 < x0 - MG || std::min(a.x, b.x) - 2 > x0 + CHUNK + MG || std::max(a.y, b.y) + 2 < y0 - MG ||
          std::min(a.y, b.y) - 2 > y0 + CHUNK + MG) { open = false; continue; }
      if (!open) { pieces.emplace_back(); open = true; }
      std::vector<GTile>& P = pieces.back();
      walk4(a.x, a.y, b.x, b.y, [&](int32_t x, int32_t y) {
        if (P.empty() || P.back().x != x || P.back().y != y) P.push_back(GTile{x, y});
      });
    }
    // (M3b fixer) a bridge runs straight: where the road crosses water (a pond, a lake, a stream) on a slant, the turns
    // are taken on dry land at the banks and the deck laid straight over the water (a staircase of deck tiles jogged
    // mid-deck, or broke into stubs where the bank is ragged). Only where no straight line with dry banks exists does
    // the slanting deck stay (the view draws a true diagonal as one span).
    // the water a road meets, from the plans and the land's own ground (a marsh's pools, the sea): a pure function of
    // the global tile, so every chunk judges a crossing alike
    std::unordered_map<uint64_t, bool> wetMemo;
    auto wetAt = [&](int32_t x, int32_t y) {
      const uint64_t key = (uint64_t)(uint32_t)x << 32 | (uint32_t)y;
      auto it = wetMemo.find(key);
      if (it != wetMemo.end()) return it->second;
      bool w = waterAtPlan(x, y);
      if (!w) { const TileF f = tile(x, y); const Ground gg = groundFor(f.eco, f.e, f.t, x, y); w = gg == Ground::Water || gg == Ground::DeepWater; }
      wetMemo.emplace(key, w);
      return w;
    };
    auto dryFoot = [&](int32_t x, int32_t y) {
      for (int oy = 0; oy <= hi; oy++)
        for (int ox = 0; ox <= hi; ox++) if (wetAt(x + ox, y + oy)) return false;
      return true;
    };
    // the deck from bank tile e to bank tile x on one line across the water, its turns on dry land (W: the tiles after
    // e up to and including x)
    auto crossing = [&](const GTile e, const GTile x, std::vector<GTile>& W) {
      const int32_t dx = x.x - e.x, dy = x.y - e.y;
      const int32_t adx = std::abs(dx), ady = std::abs(dy), sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
      const bool horiz = adx >= ady;
      // the line nearest the middle (rows between the banks for a crossing that runs east-west, columns for one that
      // runs north-south) whose legs on both banks are dry
      const int32_t lo = horiz ? std::min(e.y, x.y) : std::min(e.x, x.x), hiR = horiz ? std::max(e.y, x.y) : std::max(e.x, x.x);
      const int32_t mid = lo + (hiR - lo) / 2;
      for (int32_t off = 0; off <= hiR - lo; off++)
        for (int side = 0; side < 2; side++) {
          const int32_t r = side == 0 ? mid - off : mid + off + 1;
          if (r < lo || r > hiR || (off == 0 && side == 1 && mid + 1 > hiR)) continue;
          bool ok = true;
          W.clear();
          if (horiz) {
            for (int32_t y = e.y; y != r && ok; y += r > e.y ? 1 : -1) { const int32_t ny = y + (r > e.y ? 1 : -1); ok = dryFoot(e.x, ny); W.push_back(GTile{e.x, ny}); }
            for (int32_t k = 1; k <= adx && ok; k++) W.push_back(GTile{e.x + sx * k, r});
            ok = ok && dryFoot(x.x, r);
            for (int32_t y = r; y != x.y && ok; y += x.y > r ? 1 : -1) { const int32_t ny = y + (x.y > r ? 1 : -1); ok = dryFoot(x.x, ny); W.push_back(GTile{x.x, ny}); }
          } else {
            for (int32_t xx = e.x; xx != r && ok; xx += r > e.x ? 1 : -1) { const int32_t nx = xx + (r > e.x ? 1 : -1); ok = dryFoot(nx, e.y); W.push_back(GTile{nx, e.y}); }
            for (int32_t k = 1; k <= ady && ok; k++) W.push_back(GTile{r, e.y + sy * k});
            ok = ok && dryFoot(r, x.y);
            for (int32_t xx = r; xx != x.x && ok; xx += x.x > r ? 1 : -1) { const int32_t nx = xx + (x.x > r ? 1 : -1); ok = dryFoot(nx, x.y); W.push_back(GTile{nx, x.y}); }
          }
          if (ok) return true;
        }
      return false;
    };
    // (M11) a summit's trail (relief_plan.cpp: its legs end on dry ground on the mountain, and it carries the spurs' id
    // scheme with the shrine's local id, 0x340 ..) is laid as planned: no crossing to straighten
    const bool summitTrail = rp.cls == 2 && rp.a && (idLocal(rp.id) & 0x7F8u) == 0x340u;
    for (std::vector<GTile>& P : pieces) {
      if (summitTrail) break;
      for (size_t i = 1; i + 1 < P.size(); i++) {
        if (dryFoot(P[i].x, P[i].y) || !dryFoot(P[i - 1].x, P[i - 1].y)) continue;
        size_t j = i;
        while (j + 1 < P.size() && !dryFoot(P[j].x, P[j].y)) j++;
        if (!dryFoot(P[j].x, P[j].y)) break;   // no dry far bank in this piece
        const GTile e0 = P[i - 1], x0t = P[j];   // the last dry tile before, the first dry tile after
        const int32_t adx = std::abs(x0t.x - e0.x), ady = std::abs(x0t.y - e0.y);
        bool straight = true;   // is the run already a straight line?
        for (size_t k = i; k < j && straight; k++) if (P[k].x != e0.x && P[k].y != e0.y) straight = false;
        if (straight || std::min(adx, ady) == 0) { i = j; continue; }
        // a marsh's bank is ragged: when no line works between the banks' first dry tiles, the turns may move a few
        // tiles further onto the land either side
        std::vector<GTile> W;
        size_t ia = 0, jb = 0;
        bool found = false;
        for (size_t ext = 0; ext <= 4 && !found; ext++) {
          if (i - 1 < ext || j + ext >= P.size()) break;
          ia = i - 1 - ext; jb = j + ext;
          if (!dryFoot(P[ia].x, P[ia].y) || !dryFoot(P[jb].x, P[jb].y)) continue;
          found = crossing(P[ia], P[jb], W);
        }
        if (!found) { i = j; continue; }
        std::vector<GTile> Q(P.begin(), P.begin() + (long)ia + 1);
        for (const GTile& t : W) if (t.x != P[jb].x || t.y != P[jb].y) Q.push_back(t);
        const size_t resume = Q.size();
        Q.insert(Q.end(), P.begin() + (long)jb, P.end());
        P.swap(Q);
        i = resume;
      }
    }
    for (const std::vector<GTile>& P : pieces)
      for (const GTile& t : P) {
        if (t.x + 2 < x0 - 1 || t.x - 2 > x0 + CHUNK || t.y + 2 < y0 - 1 || t.y - 2 > y0 + CHUNK) continue;
        for (int oy = 0; oy <= hi; oy++)
          for (int ox = 0; ox <= hi; ox++) {
            int32_t gx = t.x + ox, gy = t.y + oy;
            if (!rin(gx, gy)) continue;
            if (ca && gx >= ca->x0 && gy >= ca->y0 && gx < ca->x1 && gy < ca->y1) continue;
            if (cb && gx >= cb->x0 && gy >= cb->y0 && gx < cb->x1 && gy < cb->y1) continue;
            size_t ii = ri(gx, gy);
            if (used[ii]) continue;
            road[ii] = std::max(road[ii], code);
            if (summitTrail) trailT[ii] = 1;
          }
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
}

}  // namespace ew
