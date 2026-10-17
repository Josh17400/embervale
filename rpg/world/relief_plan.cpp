// M11 "High Ground" (VISION_PLAN 11.3, 13 M11, 15.32), WORLD lane: the summit shrines and their trails.
//
// At every named summit (a Peak landmark of size 60: the region's highest point) and at the labelled crest of every
// range (a Range landmark), a SiteType::Shrine with SPF_SUMMIT stands on the top level: the summit shrine, prayer flags
// and a cairn on a small stone floor (chunkgen.cpp stampSite). It keeps clear of the massif (the great peak and its
// spires stand round the label point: chunkgen.cpp step 6b), just below it on dry ground; its flat raises the crown
// to the top level (5 at least, 3 over the land round the mountain), terraced down every 6 tiles. A trail (a
// track, RoadPlan cls 2, whose `a` is the shrine) climbs to it from the land below in switchbacks: legs of about 11
// tiles zig-zag across the fall line until the trail has walked 60+ tiles and dropped 3+ levels (or the land runs
// out), then a straight track runs on to the nearest road within 80 tiles. Every step the trail crosses is built
// stone stairs (the road rule of chunkgen step 6; a stacked face gets a stairway in two flights), and cairns stand
// beside it every 14 tiles (chunkgen step 6d). A pure function of the region's landmarks and the relief field, so
// every region and chunk agrees.
#include <algorithm>
#include <cstdint>
#include <vector>
#include "rpg/world/gen.h"

namespace ew {
using namespace gen;

void EndlessSource::Impl::planSummits(int32_t rx, int32_t ry, RegionData& D, const std::vector<Node>& nodes,
                                      const std::vector<std::shared_ptr<const Edge>>& near) {
  RegionPlan& R = D.plan;
  const int32_t x0 = rx * REGION, y0 = ry * REGION, x1 = x0 + REGION, y1 = y0 + REGION;
  const size_t nLm = R.landmarks.size();
  int made = 0;
  for (size_t li = 0; li < nLm && made < 8; li++) {
    const LandmarkPlan l = R.landmarks[li];
    if (!(l.kind == LandmarkKind::Range || (l.kind == LandmarkKind::Peak && l.size == 60))) continue;
    // (a range's crest is labelled by every region that sees it: the region holding the label point builds its shrine)
    if (l.x < x0 || l.y < y0 || l.x >= x1 || l.y >= y1) continue;
    // dry, open, high ground away from the water
    auto dryAt = [&](int32_t x, int32_t y) {
      if (waterE(x, y) < ELEV_SEA + Q(0.02)) return false;
      if (riverWidthCell(x, y) || lakeNear(x, y, 6)) return false;
      return true;
    };
    // the spot: below the massif on the summit (the great peak and the range's spires stand within 16 tiles of the
    // label across and from 8 above to 4 below it), 10 tiles south of the label, else 14 tiles east or west of that,
    // else 14 tiles north of the label: the first on dry ground. (A fixed choice, cheap enough for every region plan:
    // the shrine's own flat makes its top level, below.)
    static const int kSpot[4][2] = {{0, 10}, {14, 10}, {-14, 10}, {0, -14}};
    int32_t bx = INT32_MIN, by = 0;
    for (auto& o : kSpot)
      if (dryAt(l.x + o[0], l.y + o[1])) { bx = l.x + o[0]; by = l.y + o[1]; break; }
    if (bx == INT32_MIN) continue;
    int bestLv = fineLevel(bx, by);
    if (bestLv < 3) continue;
    // the summit stands 3 levels or more over the land round the mountain (the lowest of 8 points 50 tiles out) and on
    // level 5 at least: where the relief alone does not, the shrine's flat raises its crown, terraced down every 6
    // tiles (chunkgen step 6), and its trail climbs the terraces
    {
      int ring = bestLv;
      for (int k = 0; k < 8; k++) ring = std::min(ring, fineLevel(bx + icosR(k * 128, 50), by + isinR(k * 128, 50)));
      bestLv = std::min(7, std::max(std::max(bestLv, ring + 3), 5));
    }
    // clear of the settlements and the other places
    bool ok = true;
    for (const Node& n : nodes) if (dist2(n.x, n.y, bx, by) < (int64_t)(nominalR(n.type) + 30) * (nominalR(n.type) + 30)) { ok = false; break; }
    for (const SitePlan& p : R.sites) if (ok && dist2(p.ex, p.ey, bx, by) < 20ll * 20ll) ok = false;
    if (!ok) continue;
    SitePlan p;
    p.type = SiteType::Shrine;
    p.flags = SPF_SUMMIT;
    p.id = makeId(rx, ry, IdKind::Site, 0x340u + (uint32_t)made);
    p.ex = bx; p.ey = by;
    p.w = 7; p.h = 5; p.gx = bx - 3; p.gy = by - 2;
    p.seed = (uint32_t)(mix64(seed ^ tag("summit") ^ (uint64_t)p.id) >> 16);
    Rng nr(p.seed ^ 0xA5A5u);
    p.name = poiName(nr, SiteType::Shrine, bx, by, tile(bx, by + 2).biome);
    // the trail: from the floor's front edge down the fall line in switchbacks
    std::vector<GTile> pts;
    pts.push_back(GTile{bx, by + 3});
    {
      // the fall line: the lowest of the 9 directions 24 tiles out on the shrine's front side (east through south to
      // west: the trail leaves from the floor's front edge)
      int32_t fx = 0, fy = 1, lowE = INT32_MAX;
      for (int k = 0; k <= 8; k++) {
        const int32_t ax = icosR(k * 64, 24), ay = isinR(k * 64, 24);
        const int32_t e = reliefE(bx + ax, by + 3 + ay);
        if (e < lowE) { lowE = e; fx = ax; fy = ay; }
      }
      // legs: 7 tiles down the fall line and 9 across, the side alternating (the first by the seed)
      const int32_t fl = std::max<int32_t>(1, idist(0, 0, fx, fy));
      const int32_t ux = fx * 7 / fl, uy = fy * 7 / fl, px = -fy * 9 / fl, py = fx * 9 / fl;
      int side = (p.seed & 1) ? 1 : -1;
      int32_t walked = 0;
      for (int leg = 0; leg < 14; leg++) {
        const GTile& a = pts.back();
        const GTile b{a.x + ux + px * side, a.y + uy + py * side};
        if (waterE(b.x, b.y) < ELEV_SEA + Q(0.02) || riverWidthCell(b.x, b.y)) break;   // (the legs: no sea, no river)
        pts.push_back(b);
        walked += idist(a.x, a.y, b.x, b.y);
        side = -side;
        if (walked >= 60 && fineLevel(b.x, b.y) <= bestLv - 3) break;
      }
    }
    // on to the nearest road within 80 tiles (a straight track)
    {
      const GTile e = pts.back();
      int64_t bd = 80ll * 80ll + 1;
      GTile at{0, 0};
      bool any = false;
      for (auto& ed : near) {
        if (ed->x1 < e.x - 80 || ed->x0 > e.x + 80 || ed->y1 < e.y - 80 || ed->y0 > e.y + 80) continue;
        for (const GTile& g : ed->pts) {
          const int64_t d = dist2(g.x, g.y, e.x, e.y);
          if (d < bd) { bd = d; at = g; any = true; }
        }
      }
      if (any && bd > 4) pts.push_back(at);
    }
    if (pts.size() < 3) continue;   // no room for a trail: no shrine
    // the trail stays within the regions round its own (the chunks read the 3 x 3)
    bool inside = true;
    for (const GTile& g : pts)
      if (g.x < x0 - REGION + 4 || g.y < y0 - REGION + 4 || g.x >= x1 + REGION - 4 || g.y >= y1 + REGION - 4) inside = false;
    if (!inside) continue;
    const int lvl = bestLv;
    R.sites.push_back(p);
    D.flatLevel.push_back({lvl, 0});
    SitePlan& q = R.sites.back();
    q.level = danger(q.ex, q.ey);
    q.kingdom = kingdomAt(q.ex, q.ey);
    q.culture = cultureAt(q.ex, q.ey);
    if (hiddenShapeAt(q.ex, q.ey)) q.flags |= SPF_HIDDEN;
    RoadPlan rp;
    rp.id = makeId(rx, ry, IdKind::Edge, 0x800u | (idLocal(p.id) & 0x7FFu));   // (the spurs' scheme: summits take no spur)
    rp.cls = 2;
    rp.a = p.id;
    rp.pts = std::move(pts);
    R.roads.push_back(std::move(rp));
    made++;
  }
}

}  // namespace ew
