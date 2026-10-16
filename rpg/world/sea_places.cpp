// M10 Beyond the Mist (VISION_PLAN 2.12, 6.4, 7.6, 15.31): the sea's places. WORLD lane.
//
// The sea pass of a region plan (planSea, after the land planner and the spurs: sea places take no track):
//   - UNCHARTED ISLETS: every small island wholly inside the region (a land component of 2..150 samples on an 8-tile
//     grid, the region's border untouched) gets one place: a wreck on its beach, a cache cairn (a chest by the cairn),
//     a pirate cove (SiteType::BanditCamp, SPF_PIRATE) or a sea cave (SiteType::Cave, SPF_SEACAVE: the mouth opens on
//     the water, the crag behind it); SPF_ISLET marks them;
//   - SHIPWRECKS on the beaches and sandbars of a coast (about one coastal region in three), a chest from the hold
//     beside the hull, MERROW dens in the water by every wreck (DenPlan Monster::Merrow);
//   - LIGHTHOUSES: one per harbour, on the most exposed headland 70..220 tiles from the port's heart, with its keeper;
//   - the CHART PLACES of every hidden land (chartPlaces: the chain's wreck, step 2, and its sea cave, step 3, both on
//     the mainland side 1500..5000 tiles out from its Mist) and the TRIALS of every hidden kingdom (trialOf: two ruins
//     near its capital, VISION_PLAN 6.4), each in the region holding its heart.
// Every decision reads pure functions (waterE: the chunks' own sea; natLevel; the settlement lattice), so the plans
// never depend on which chunk or region was asked first.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include "rpg/world/gen.h"
#include "rpg/world/wildlife.h"

namespace ew {
using namespace gen;
using art::Monster;

namespace {
constexpr int IG = 8;                 // the islet grid (tiles)
constexpr int IN = REGION / IG;       // samples a side
constexpr int MAX_ISLETS = 3;
}

bool EndlessSource::Impl::seaLand(int32_t x, int32_t y) { return waterE(x, y) >= ELEV_SEA; }

// a beached wreck's 3x2 on low, level land, the sea within a few tiles of its foot
bool EndlessSource::Impl::wreckFits(int32_t x, int32_t y) {
  const int lv = natLevel(x, y);
  if (lv > 1) return false;
  for (int32_t yy = y - 1; yy <= y; yy++)
    for (int32_t xx = x - 1; xx <= x + 1; xx++)
      if (!seaLand(xx, yy) || natLevel(xx, yy) != lv) return false;
  if (riverWidthCell(x, y) || tile(x, y).rock) return false;
  for (int k = 1; k <= 5; k++)
    if (!seaLand(x, y + k) || !seaLand(x + k + 1, y) || !seaLand(x - k - 1, y) || !seaLand(x, y - 1 - k)) return true;
  return false;
}
// a sea cave's mouth: land with the crag's land behind (north) and the open water before it (south)
bool EndlessSource::Impl::seaCaveFits(int32_t x, int32_t y) {
  if (!seaLand(x, y) || seaLand(x, y + 1) || seaLand(x, y + 2) || seaLand(x - 1, y + 2) || seaLand(x + 1, y + 2)) return false;
  for (int32_t yy = y - 3; yy <= y - 1; yy++)
    for (int32_t xx = x - 2; xx <= x + 2; xx++)
      if (!seaLand(xx, yy)) return false;
  return !riverWidthCell(x, y);
}
// a lighthouse's headland: land standing out into the sea (about half the ring round it is water)
bool EndlessSource::Impl::headlandFits(int32_t x, int32_t y) {
  for (int32_t yy = y - 1; yy <= y + 2; yy++)
    for (int32_t xx = x - 2; xx <= x + 2; xx++)
      if (!seaLand(xx, yy)) return false;
  int wet = 0;
  for (int k = 0; k < 16; k++) {
    const int32_t a = k * 64;
    if (!seaLand(x + icosR(a, 10), y + isinR(a, 10))) wet++;
  }
  return wet >= 7 && wet <= 12 && !riverWidthCell(x, y) && !tile(x, y).rock;
}

bool EndlessSource::Impl::lighthouseOf(const Node& n, int32_t& ox, int32_t& oy) {
  auto it = lightMemo.find(n.id);
  if (it != lightMemo.end()) { ox = it->second.second.first; oy = it->second.second.second; return it->second.first; }
  if (lightMemo.size() > 4096) lightMemo.clear();
  bool ok = false;
  PortPlan P;
  if (harbourOf(n, P) && P.harbour) {
    const uint64_t h = mix64(seed ^ tag("sea.light") ^ n.id);
    const int32_t a0 = (int32_t)(h % 1024);
    for (int32_t r = 70; r <= 220 && !ok; r += 12)
      for (int k = 0; k < 40 && !ok; k++) {
        const int32_t a = a0 + k * 1024 / 40;
        const int32_t x = n.x + icosR(a, r), y = n.y + isinR(a, r);
        if (dist2(x, y, P.rootX, P.rootY) < 30ll * 30ll) continue;
        if (!headlandFits(x, y)) continue;
        ox = x; oy = y; ok = true;
      }
  }
  lightMemo[n.id] = {ok, {ox, oy}};
  return ok;
}

// ---- a hidden land's chart places: rays from its centre over the mainland side, the first fitting shore
bool EndlessSource::Impl::chartPlaces(uint32_t landId, SitePlan& wreck, SitePlan& cave) {
  auto it = chartMemo.find(landId);
  if (it != chartMemo.end()) { wreck = it->second.second.first; cave = it->second.second.second; return it->second.first; }
  if (chartMemo.size() > 1024) chartMemo.clear();
  const int32_t lcx = landCellX(landId), lcy = landCellY(landId);
  const LandDraw L = landDraw(lcx, lcy);
  bool okW = false, okC = false;
  if (L.kind != 2 && L.hidden) {
    const int32_t mOut = L.R + L.R * 15 / 100 + MIST_RING;
    // toward the mainland: the origin for a land within reach of it, else the nearest land that is not hidden
    int32_t tx = -L.px, ty = -L.py;
    if (idist(L.px, L.py, 0, 0) > 16000) {
      int64_t bd = INT64_MAX;
      for (int dj = -1; dj <= 1; dj++)
        for (int di = -1; di <= 1; di++) {
          const LandDraw& N = landDraw(lcx + di, lcy + dj);
          if (N.kind == 2 || N.hidden || (!di && !dj)) continue;
          const int64_t d = dist2(L.px, L.py, N.px, N.py);
          if (d < bd) { bd = d; tx = N.px - L.px; ty = N.py - L.py; }
        }
    }
    // the bearing (1024 a turn) whose unit vector points most nearly along (tx, ty): integer maths
    int32_t a0 = 0;
    {
      int64_t best = INT64_MIN;
      for (int32_t k = 0; k < 1024; k++) {
        const int64_t dot = (int64_t)icosR(k, 4096) * tx + (int64_t)isinR(k, 4096) * ty;
        if (dot > best) { best = dot; a0 = k; }
      }
    }
    std::vector<Node> nodes;
    auto clearOfTowns = [&](int32_t x, int32_t y) {
      nodes.clear();
      nodesIn(x - 160, y - 160, x + 160, y + 160, nodes);
      for (const Node& nd : nodes) if (dist2(nd.x, nd.y, x, y) < (int64_t)(nominalR(nd.type) + 40) * (nominalR(nd.type) + 40)) return false;
      return true;
    };
    int32_t wreckA = 0;
    for (int pass = 0; pass < 2 && !(okW && okC); pass++)
      for (int k = 0; k < 120 && !(okW && okC); k++) {
        const int32_t off = (k + 1) / 2 * (pass ? 9 : 3) * ((k & 1) ? 1 : -1);   // 3/1024 of a turn a ray (pass 2: the whole round)
        const int32_t a = a0 + off;
        if (okW && std::abs(a - wreckA) < 24) continue;                         // the cave somewhere else along the coast
        // the first shore past 1500 tiles out from the Mist
        int32_t prevX = 0, prevY = 0;
        bool prevSea = true;
        for (int32_t r = mOut + 1500; r <= mOut + 5000; r += 32) {
          const int32_t x = L.px + icosR(a, r), y = L.py + isinR(a, r);
          const bool land = landTile(x, y);   // (cache-free on the long rays: the blocks only near the shore found)
          if (land && prevSea && r > mOut + 1500) {
            if (hiddenShapeAt(x, y)) break;
            // the shore tile: back along the ray to the last land tile
            int32_t sx = x, sy = y;
            for (int s = 0; s < 32; s++) {
              const int32_t qx = prevX + (x - prevX) * s / 32, qy = prevY + (y - prevY) * s / 32;
              if (seaLand(qx, qy)) { sx = qx; sy = qy; break; }
            }
            if (!okW) {
              for (int j = 0; j < 25 && !okW; j++) {
                const int32_t wx = sx + (j % 5) - 2, wy = sy + (j / 5) - 2;
                if (!wreckFits(wx, wy) || !clearOfTowns(wx, wy)) continue;
                wreck = SitePlan();
                wreck.type = SiteType::Vignette;
                wreck.kind = (uint8_t)VignetteKind::Shipwreck;
                wreck.ex = wx; wreck.ey = wy;
                wreck.gx = wx - 3; wreck.gy = wy - 3; wreck.w = 7; wreck.h = 5;
                wreck.seed = (uint32_t)(mix64(seed ^ tag("chart.wreck") ^ landId) >> 32);
                wreck.flags = SPF_CHART;
                wreck.chartLand = landId;
                wreck.id = makeId(regionOf(wx), regionOf(wy), IdKind::Site, LOCAL_SEA + 0x80 + (uint32_t)(((lcx & 7) << 3) | (lcy & 7)));
                okW = true;
                wreckA = a;
              }
            } else if (!okC) {
              for (int j = 0; j < 49 * 9 && !okC; j++) {
                const int32_t cx = sx + (j % 21) - 10, cy = sy + (j / 21) - 10;
                if (!seaCaveFits(cx, cy) || !clearOfTowns(cx, cy) || dist2(cx, cy, wreck.ex, wreck.ey) < 200ll * 200ll) continue;
                cave = SitePlan();
                cave.type = SiteType::Cave;
                cave.ex = cx; cave.ey = cy;
                cave.w = 5; cave.h = 4; cave.gx = cx - 2; cave.gy = cy - 1;
                cave.seed = (uint32_t)(mix64(seed ^ tag("chart.cave") ^ landId) >> 32);
                cave.flags = SPF_CHART | SPF_SEACAVE;
                cave.chartLand = landId;
                cave.theme = Monster::Merrow;
                cave.id = makeId(regionOf(cx), regionOf(cy), IdKind::Site, LOCAL_SEA + 0xC0 + (uint32_t)(((lcx & 7) << 3) | (lcy & 7)));
                okC = true;
              }
            }
            break;
          }
          prevSea = !land; prevX = x; prevY = y;
        }
      }
  }
  const bool ok = okW && okC;
  chartMemo[landId] = {ok, {wreck, cave}};
  return ok;
}

// ---- a hidden kingdom's two trials (VISION_PLAN 6.4): ruins on its own land, a day's walk from its capital
bool EndlessSource::Impl::trialOf(Gid kingdomId, int which, SitePlan& out) {
  if (which < 0 || which > 1) return false;
  const uint64_t key = ((uint64_t)kingdomId << 1) | (uint64_t)which;
  auto it = trialMemo.find(key);
  if (it != trialMemo.end()) { out = it->second.second; return it->second.first; }
  if (trialMemo.size() > 1024) trialMemo.clear();
  const KingdomPlan* K = kingdom(kingdomId);
  bool ok = false;
  SitePlan T;
  if (K && K->hiddenLand) {
    SitePlan first;
    const bool haveFirst = which == 1 && trialOf(kingdomId, 0, first);
    const uint64_t h = mix64(seed ^ tag("trial") ^ kingdomId ^ (uint64_t)which);
    const int32_t a0 = (int32_t)(h % 1024);
    std::vector<Node> nodes;
    nodesIn(K->gx - 800, K->gy - 800, K->gx + 800, K->gy + 800, nodes);
    for (int32_t r = 170 + which * 120; r <= 640 && !ok; r += 24)
      for (int k = 0; k < 24 && !ok; k++) {
        const int32_t a = a0 + k * 1024 / 24;
        const int32_t x = K->gx + icosR(a, r), y = K->gy + isinR(a, r);
        if (hiddenShapeAt(x, y) != K->hiddenLand || !seaLand(x, y)) continue;
        const int L = natLevel(x, y);
        bool fit = natLevel(x - 6, y) == L && natLevel(x + 6, y) == L && natLevel(x, y - 4) == L && natLevel(x, y + 6) == L;
        for (int32_t yy = y - 4; yy <= y + 9 && fit; yy += 3)
          for (int32_t xx = x - 7; xx <= x + 7 && fit; xx += 3) fit = seaLand(xx, yy);
        if (!fit || riverWidthCell(x, y) || lakeNear(x, y, 10) || coarse(x, y).rock > Q(0.2)) continue;
        for (const Node& nd : nodes) if (dist2(nd.x, nd.y, x, y) < (int64_t)(nominalR(nd.type) + 60) * (nominalR(nd.type) + 60)) fit = false;
        if (haveFirst && dist2(first.ex, first.ey, x, y) < 120ll * 120ll) fit = false;
        if (!fit) continue;
        T.type = SiteType::Ruin;
        T.ex = x; T.ey = y;
        T.w = 9; T.h = 7; T.gx = x - 4; T.gy = y - 2;
        T.seed = (uint32_t)(h >> 32);
        T.flags = SPF_TRIAL | SPF_HIDDEN;
        T.kingdom = kingdomId;
        static const Monster themes[] = {Monster::Wraith, Monster::Draugr, Monster::Skeleton};
        T.theme = themes[(h >> 20) % 3];
        T.id = makeId(regionOf(x), regionOf(y), IdKind::Site, LOCAL_SEA + 0x40 + (uint32_t)(((idRx(kingdomId) & 3) << 3) | ((idRy(kingdomId) & 3) << 1) | which));
        T.name = which == 0 ? "THE TRIAL OF THE THRESHOLD" : "THE SECOND TRIAL";
        ok = true;
      }
  }
  trialMemo[key] = {ok, T};
  if (ok) out = T;
  return ok;
}

void EndlessSource::Impl::planSea(int32_t rx, int32_t ry, RegionData& D, const std::vector<Node>& nodes) {
  RegionPlan& R = D.plan;
  const int32_t x0 = rx * REGION, y0 = ry * REGION, x1 = x0 + REGION, y1 = y0 + REGION;
  auto inR = [&](int32_t x, int32_t y) { return x >= x0 && y >= y0 && x < x1 && y < y1; };
  auto addSite = [&](SitePlan p, int flat, int levelUp) {
    p.level = danger(p.ex, p.ey) + levelUp;
    if (!p.kingdom) p.kingdom = kingdomAt(p.ex, p.ey);
    p.culture = cultureAt(p.ex, p.ey);
    if (hiddenShapeAt(p.ex, p.ey)) p.flags |= SPF_HIDDEN;
    R.sites.push_back(p);
    D.flatLevel.push_back({flat, 0});
  };
  // the places already planned keep their room; settlements theirs; the roads theirs
  auto clear = [&](int32_t x, int32_t y, int32_t pad) {
    for (const Node& n : nodes)
      if (dist2(n.x, n.y, x, y) < (int64_t)(nominalR(n.type) + pad) * (nominalR(n.type) + pad)) return false;
    for (const SitePlan& s : R.sites) if (dist2(s.ex, s.ey, x, y) < 36ll * 36ll) return false;
    for (const RoadPlan& rp : R.roads)
      for (const GTile& g : rp.pts) if (dist2(g.x, g.y, x, y) < 9ll * 9ll) return false;
    for (const SitePlan& f : forcedSites) if (dist2(f.ex, f.ey, x, y) < 60ll * 60ll) return false;
    return !nearWonder(x, y, 40);
  };
  int denN = 0;
  auto merrowDen = [&](int32_t x, int32_t y, uint32_t sd) {
    for (int32_t r = 5; r <= 10; r++)
      for (int k = 0; k < 8; k++) {
        const int32_t a = (int32_t)(sd % 1024) + k * 128;
        const int32_t dx = x + icosR(a, r), dy = y + isinR(a, r);
        if (seaLand(dx, dy) || !inR(dx, dy)) continue;
        DenPlan d;
        d.id = makeId(rx, ry, IdKind::Den, 0xE0u + (uint32_t)denN++);
        d.x = dx; d.y = dy;
        d.mon = Monster::Merrow;
        d.pack = (uint8_t)(2 + (sd >> 9) % 2);
        R.dens.push_back(d);
        return;
      }
  };
  auto wreckSite = [&](int32_t x, int32_t y, uint32_t local, uint32_t sd, uint16_t flags) {
    SitePlan p;
    p.type = SiteType::Vignette;
    p.kind = (uint8_t)VignetteKind::Shipwreck;
    p.id = makeId(rx, ry, IdKind::Site, LOCAL_SEA + local);
    p.ex = x; p.ey = y; p.gx = x - 3; p.gy = y - 3; p.w = 7; p.h = 5;
    p.seed = sd;
    p.flags = flags;
    p.name = vignetteTitle(VignetteKind::Shipwreck, x, y, sd & 0xFFFFFFu, 0, (int)(local & 1));
    addSite(p, -1, 0);
    merrowDen(x, y, sd);
  };
  // ---- the region's land and sea on the islet grid
  uint8_t land[IN * IN];
  int nLand = 0;
  for (int j = 0; j < IN; j++)
    for (int i = 0; i < IN; i++) {
      land[j * IN + i] = seaLand(x0 + i * IG + IG / 2, y0 + j * IG + IG / 2) ? 1 : 0;
      nLand += land[j * IN + i];
    }
  const bool coastal = nLand > 0 && nLand < IN * IN;
  const uint64_t rh = cellSeed(seed, tag("sea.region"), rx, ry);
  // ---- uncharted islets
  if (coastal) {
    uint8_t seen[IN * IN] = {};
    std::vector<int> q;
    int islet = 0;
    for (int s0 = 0; s0 < IN * IN && islet < MAX_ISLETS; s0++) {
      if (!land[s0] || seen[s0]) continue;
      q.assign(1, s0);
      seen[s0] = 1;
      bool border = false;
      int bi0 = IN, bj0 = IN, bi1 = -1, bj1 = -1;
      for (size_t h = 0; h < q.size(); h++) {
        const int c = q[h], i = c % IN, j = c / IN;
        if (i == 0 || j == 0 || i == IN - 1 || j == IN - 1) border = true;
        bi0 = std::min(bi0, i); bj0 = std::min(bj0, j); bi1 = std::max(bi1, i); bj1 = std::max(bj1, j);
        const int nb[4] = {i > 0 ? c - 1 : -1, i < IN - 1 ? c + 1 : -1, j > 0 ? c - IN : -1, j < IN - 1 ? c + IN : -1};
        for (int m : nb) if (m >= 0 && land[m] && !seen[m]) { seen[m] = 1; q.push_back(m); }
      }
      if (border || q.size() < 2 || q.size() > 150) continue;
      // an islet: its kind, then a spot on it that suits the kind (else a cache cairn, which fits anywhere)
      const uint64_t ih = mix64(rh ^ (uint64_t)s0 * 0x9E37u);
      int kind = (int)(ih % 4);                  // 0 wreck, 1 cache, 2 pirate cove, 3 sea cave
      if (kind == 2 && q.size() < 14) kind = 1;  // (a camp wants room)
      const int32_t tx0 = x0 + bi0 * IG, ty0 = y0 + bj0 * IG, tx1 = x0 + (bi1 + 1) * IG, ty1 = y0 + (bj1 + 1) * IG;
      const int32_t mx = (tx0 + tx1) / 2, my = (ty0 + ty1) / 2;
      bool placed = false;
      for (int attempt = 0; attempt < 2 && !placed; attempt++) {
        const int kk = attempt ? 1 : kind;
        // the tiles of its bounding box, nearest its middle first (a spiral of rings)
        for (int32_t r = 0; r <= std::max(tx1 - tx0, ty1 - ty0) / 2 + 2 && !placed; r++)
          for (int32_t e = -r; e <= r && !placed; e++)
            for (int side = 0; side < 4 && !placed; side++) {
              if (r && (e == -r || e == r) && side >= 2) continue;
              const int32_t x = side == 0 ? mx + e : side == 1 ? mx + e : side == 2 ? mx - r : mx + r;
              const int32_t y = side == 0 ? my - r : side == 1 ? my + r : my + e;
              if (r == 0 && side) continue;
              if (!inR(x, y) || !clear(x, y, 40)) continue;
              bool roomy = true;   // (two islets' places never crowd each other)
              for (const SitePlan& o : R.sites) if ((o.flags & SPF_ISLET) && dist2(o.ex, o.ey, x, y) < 70ll * 70ll) roomy = false;
              if (!roomy) continue;
              if (kk == 0 && wreckFits(x, y)) {
                wreckSite(x, y, 0x00u + (uint32_t)islet, (uint32_t)(ih >> 32), SPF_ISLET);
                placed = true;
              } else if (kk == 1 && seaLand(x, y) && seaLand(x + 1, y) && seaLand(x, y - 1) && natLevel(x, y) == natLevel(x + 1, y)) {
                SitePlan p;
                p.type = SiteType::Vignette;
                p.kind = (uint8_t)VignetteKind::LoneGrave;
                p.id = makeId(rx, ry, IdKind::Site, LOCAL_SEA + 0x00u + (uint32_t)islet);
                p.ex = x; p.ey = y; p.gx = x - 2; p.gy = y - 2; p.w = 5; p.h = 4;
                p.seed = (uint32_t)(ih >> 32);
                p.flags = SPF_ISLET | SPF_CACHE;
                p.name = vignetteTitle(VignetteKind::LoneGrave, x, y, p.seed & 0xFFFFFFu, 0, 1);
                addSite(p, -1, 0);
                placed = true;
              } else if (kk == 2) {
                const int L = natLevel(x, y);
                bool fit = true;
                for (int32_t yy = y - 4; yy <= y + 4 && fit; yy += 2)
                  for (int32_t xx = x - 5; xx <= x + 5 && fit; xx += 2) fit = seaLand(xx, yy) && natLevel(xx, yy) == L;
                if (!fit) continue;
                SitePlan p;
                p.type = SiteType::BanditCamp;
                p.id = makeId(rx, ry, IdKind::Site, LOCAL_SEA + 0x00u + (uint32_t)islet);
                p.ex = x; p.ey = y; p.w = 11; p.h = 9; p.gx = x - 5; p.gy = y - 4;
                p.seed = (uint32_t)(ih >> 32);
                p.flags = SPF_ISLET | SPF_PIRATE;
                Rng nr(p.seed ^ 0xA5A5u);
                const std::string who = poiName(nr, SiteType::BanditCamp, x, y, Biome::Beach);
                const size_t ap = who.find("'S ");
                p.name = ap != std::string::npos ? who.substr(0, ap) + "'S COVE" : "PIRATES' COVE";
                addSite(p, L, 0);
                placed = true;
              } else if (kk == 3 && seaCaveFits(x, y)) {
                SitePlan p;
                p.type = SiteType::Cave;
                p.id = makeId(rx, ry, IdKind::Site, LOCAL_SEA + 0x00u + (uint32_t)islet);
                p.ex = x; p.ey = y; p.w = 5; p.h = 4; p.gx = x - 2; p.gy = y - 1;
                p.seed = (uint32_t)(ih >> 32);
                p.flags = SPF_ISLET | SPF_SEACAVE;
                p.theme = Monster::Merrow;
                Rng nr(p.seed ^ 0xA5A5u);
                p.name = poiName(nr, SiteType::Cave, x, y, Biome::Beach);
                addSite(p, -1, 0);
                placed = true;
              }
            }
      }
      if (placed) islet++;
    }
  }
  // ---- a wreck on a beach of the coast (about one coastal region in three)
  // (one region in each 3x3 block of regions may hold one, three blocks in five: wrecks lie a day's sail apart, never
  // two on one beach)
  const uint64_t wb = cellSeed(seed, tag("sea.wreck"), floorDiv(rx, 3), floorDiv(ry, 3));
  const bool wreckRegion = floorMod(rx, 3) == (int32_t)(wb % 3) && floorMod(ry, 3) == (int32_t)((wb >> 2) % 3) && (wb >> 4) % 5 < 3;
  if (coastal && wreckRegion) {
    const int start = (int)((rh >> 8) % (IN * IN));
    bool done = false;
    for (int t = 0; t < IN * IN && !done; t += 7) {
      const int c = (start + t) % (IN * IN), i = c % IN, j = c / IN;
      if (!land[c] || i == 0 || j == 0 || i == IN - 1 || j == IN - 1) continue;
      int dirI = 0, dirJ = 0;
      if (!land[c + 1]) dirI = 1; else if (!land[c - 1]) dirI = -1; else if (!land[c + IN]) dirJ = 1; else if (!land[c - IN]) dirJ = -1;
      if (!dirI && !dirJ) continue;
      // walk to the shore and stand the hull a tile or two inland of it
      int32_t x = x0 + i * IG + IG / 2, y = y0 + j * IG + IG / 2;
      for (int s = 0; s < IG + 2 && seaLand(x + dirI, y + dirJ); s++) { x += dirI; y += dirJ; }
      x -= dirI; y -= dirJ;
      if (dirJ == 1) y -= 0; else if (dirJ == -1) y += 1;
      if (!inR(x, y) || !clear(x, y, 50) || !wreckFits(x, y)) continue;
      wreckSite(x, y, 0x10u, (uint32_t)(rh >> 32), 0);
      done = true;
    }
  }
  // ---- lighthouses of the harbours round the region
  for (const Node& n : nodes) {
    if (n.type == SiteType::Village) continue;
    int32_t lx, ly;
    if (!lighthouseOf(n, lx, ly) || !inR(lx, ly)) continue;
    bool dup = false;
    for (const SitePlan& s : R.sites) if (dist2(s.ex, s.ey, lx, ly) < 12ll * 12ll) dup = true;
    if (dup) continue;
    SitePlan p;
    p.type = SiteType::Vignette;
    p.kind = (uint8_t)VignetteKind::Lighthouse;
    p.id = makeId(rx, ry, IdKind::Site, LOCAL_SEA + 0x20u + (uint32_t)(idLocal(n.id) & 0xF));
    p.ex = lx; p.ey = ly; p.gx = lx - 2; p.gy = ly - 3; p.w = 5; p.h = 5;
    p.seed = (uint32_t)(mix64(seed ^ tag("sea.light") ^ n.id) >> 32);
    p.name = vignetteTitle(VignetteKind::Lighthouse, lx, ly, p.seed & 0xFFFFFFu, 0, 0);
    addSite(p, -1, 0);
  }
  // ---- the chart places of the hidden lands whose search reaches the region
  {
    const int32_t reach = 3600 * 115 / 100 + MIST_RING + 5200;
    for (int32_t cy = floorDiv(y0 - reach + LCELL / 2, LCELL); cy <= floorDiv(y1 + reach + LCELL / 2, LCELL); cy++)
      for (int32_t cx = floorDiv(x0 - reach + LCELL / 2, LCELL); cx <= floorDiv(x1 + reach + LCELL / 2, LCELL); cx++) {
        const LandDraw& Ld = landDraw(cx, cy);
        if (Ld.kind == 2 || !Ld.hidden) continue;
        const int32_t far = Ld.R + Ld.R * 15 / 100 + MIST_RING + 5100;
        if (Ld.px + far < x0 || Ld.px - far >= x1 || Ld.py + far < y0 || Ld.py - far >= y1) continue;
        SitePlan w, c;
        if (!chartPlaces(landIdOf(cx, cy), w, c)) continue;
        if (inR(w.ex, w.ey)) {
          w.name = vignetteTitle(VignetteKind::Shipwreck, w.ex, w.ey, w.seed & 0xFFFFFFu, 0, 1);
          addSite(w, -1, 0);
          merrowDen(w.ex, w.ey, w.seed);
        }
        if (inR(c.ex, c.ey)) {
          Rng nr(c.seed ^ 0xA5A5u);
          c.name = poiName(nr, SiteType::Cave, c.ex, c.ey, Biome::Beach);
          addSite(c, -1, 2);
        }
      }
  }
  // ---- the trials of the hidden kingdoms whose capital stands near the region
  for (int32_t ky = EndlessSource::kcellOf(y0 - 900); ky <= EndlessSource::kcellOf(y1 + 900); ky++)
    for (int32_t kx = EndlessSource::kcellOf(x0 - 900); kx <= EndlessSource::kcellOf(x1 + 900); kx++) {
      const Gid kid = makeId(kx, ky, IdKind::Kingdom, 0);
      if (!hiddenNear(kx * KCELL, ky * KCELL, 1200)) continue;   // (cheap: only near a hidden land)
      const KingdomPlan* K = kingdom(kid);
      if (!K || !K->hiddenLand) continue;
      for (int which = 0; which < 2; which++) {
        SitePlan t;
        if (!trialOf(kid, which, t) || !inR(t.ex, t.ey)) continue;
        addSite(t, natLevel(t.ex, t.ey), which ? 6 : 2);
      }
    }
}

}  // namespace ew
