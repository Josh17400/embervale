// M2: geological provinces (rpg/world/geology.h, VISION_PLAN 15.11). WORLD lane.
//
// Two kinds of province, both following the macro fields:
//   - belts: the mountain belt of a converging plate boundary (the plate ridge field above BELT_R) and the floor of a
//     rift (the rift field above RIFT_R) are provinces of their own, one per plate pair, so a range is one geology from
//     end to end (granite, slate or marble; basalt in rifts);
//   - basins: everywhere else, the cells of a jittered lattice (PCELL tiles) seen through a domain warp, so their
//     borders wander; their rock comes from the macro fields averaged over five samples round the seed point (the
//     wet lowlands limestone, dry country sandstone, highlands granite or slate, old wet hills marble, some basalt).
// A province's values depend on its key alone (memoised by it), so every tile of it agrees whichever asked first.
// Ores (owner 2026-10-05): every province has a primary ore >= 160 (>= 190 in rugged ones), at most two others above
// 96, the rest low; copper and tin rarely strong together (bronze needs trade); the rare metals only in remote rugged
// provinces (about one in ten). Integer maths only.
#include "rpg/world/gen.h"

namespace ew {

namespace {
constexpr int32_t PCELL = 320;   // basin lattice spacing (tiles): about a region and a quarter across
constexpr int32_t BELT_R = gen::Q(0.42), RIFT_R = gen::Q(0.50);

// the seed point of lattice cell (i, j): jittered inside the cell's middle
void provincePoint(uint64_t seed, int32_t i, int32_t j, int32_t& px, int32_t& py) {
  uint64_t h = cellSeed(seed, tag("geo.cell"), i, j);
  px = i * PCELL + 40 + (int32_t)((h >> 8) % (uint64_t)(PCELL - 80));
  py = j * PCELL + 40 + (int32_t)((h >> 32) % (uint64_t)(PCELL - 80));
}

// base affinities per rock: copper, tin, iron, coal, silver, rare
const uint8_t kBase[(int)Rock::COUNT][(int)Ore::COUNT] = {
    {60, 155, 150, 20, 110, 10},   // granite (tin lodes in old granite, as in the real world; iron beside it)
    {150, 30, 120, 30, 40, 40},    // basalt
    {60, 40, 90, 170, 30, 5},      // limestone
    {180, 60, 50, 40, 20, 5},      // sandstone
    {40, 140, 150, 40, 60, 10},    // slate
    {70, 50, 40, 20, 170, 30},     // marble
};
}  // namespace

// (M3c) the province's rock alone (the biome classifier's question: chalk downs on limestone, ash fields on basalt,
// badlands on sandstone). The same rule as geology() below, without the ore pass, which asks for the kingdoms (and
// the kingdoms' capitals ask for the land's biomes: geology() inside the classifier would recurse). Memoised.
Rock EndlessSource::Impl::provinceRock(int32_t x, int32_t y) {
  int32_t ridge = 0, rift = 0;
  uint64_t pair = 0;
  plates(x, y, ridge, rift, &pair);
  const bool belt = ridge > BELT_R, riftB = !belt && rift > RIFT_R;
  uint64_t key;
  int32_t bi = 0, bj = 0, bpx = 0, bpy = 0;
  if (belt || riftB) key = mix64(pair ^ (belt ? 0xBE17ull : 0x21F7ull)) | 1ull;
  else {
    int32_t wx = x, wy = y;
    warpQ(wx, wy, 7, 56, mix64(seed ^ tag("geo.warp")));
    const int32_t ci = floorDiv(wx, PCELL), cj = floorDiv(wy, PCELL);
    int64_t bd = INT64_MAX;
    for (int dj = -1; dj <= 1; dj++)
      for (int di = -1; di <= 1; di++) {
        int32_t px, py;
        provincePoint(seed, ci + di, cj + dj, px, py);
        int64_t d = gen::dist2(wx, wy, px, py);
        if (d < bd) { bd = d; bi = ci + di; bj = cj + dj; bpx = px; bpy = py; }
      }
    key = gen::key2(bi, bj) << 1;
  }
  auto it = rockMemo.find(key);
  if (it != rockMemo.end()) return it->second;
  const uint64_t h = belt || riftB ? mix64(key ^ seed ^ tag("geo.belt")) : cellSeed(seed, tag("geo.rock"), bi, bj);
  Rock rk;
  if (belt) { const uint32_t q = (uint32_t)(h % 10); rk = q < 5 ? Rock::Granite : q < 8 ? Rock::Slate : Rock::Marble; }
  else if (riftB) rk = Rock::Basalt;
  else {
    int64_t e = 0, t = 0, m = 0, rg = 0;
    for (int k = 0; k < 5; k++) {
      const int32_t sx = bpx + (k == 1 ? 60 : k == 2 ? -60 : 0), sy = bpy + (k == 3 ? 60 : k == 4 ? -60 : 0);
      const Coarse c = coarse(sx, sy);
      e += c.e; t += c.t; m += c.m; rg += c.ridge;
    }
    e /= 5; t /= 5; m /= 5; rg /= 5;
    const int lv = gen::levelOf((int32_t)e);
    const bool sea = e < ELEV_SEA;
    if (lv >= 3 || rg > gen::Q(0.25)) rk = m > gen::Q(0.56) ? Rock::Marble : ((h & 3) == 0 ? Rock::Slate : Rock::Granite);
    else if (t > gen::Q(0.62) && m < gen::Q(0.40)) rk = Rock::Sandstone;
    else if ((h >> 3) % 7 == 0 || (sea && (h & 1))) rk = Rock::Basalt;
    else rk = m > gen::Q(0.50) ? Rock::Limestone : ((h >> 5) & 1 ? Rock::Sandstone : Rock::Limestone);
  }
  if (rockMemo.size() > 8192) rockMemo.clear();
  rockMemo[key] = rk;
  return rk;
}

Geology EndlessSource::Impl::geology(int32_t x, int32_t y) {
  // 1. a belt: the ranges and rifts of the plate boundaries
  int32_t ridge = 0, rift = 0;
  uint64_t pair = 0;
  plates(x, y, ridge, rift, &pair);
  const bool belt = ridge > BELT_R, riftB = !belt && rift > RIFT_R;
  uint64_t key;
  int32_t bi = 0, bj = 0, bpx = 0, bpy = 0;
  if (belt || riftB) key = mix64(pair ^ (belt ? 0xBE17ull : 0x21F7ull)) | 1ull;   // odd: never a basin's key
  else {
    // 2. a basin: the nearest seed point to the warped tile
    int32_t wx = x, wy = y;
    warpQ(wx, wy, 7, 56, mix64(seed ^ tag("geo.warp")));
    const int32_t ci = floorDiv(wx, PCELL), cj = floorDiv(wy, PCELL);
    int64_t bd = INT64_MAX;
    for (int dj = -1; dj <= 1; dj++)
      for (int di = -1; di <= 1; di++) {
        int32_t px, py;
        provincePoint(seed, ci + di, cj + dj, px, py);
        int64_t d = gen::dist2(wx, wy, px, py);
        if (d < bd) { bd = d; bi = ci + di; bj = cj + dj; bpx = px; bpy = py; }
      }
    key = gen::key2(bi, bj) << 1;   // even
  }
  auto it = geoMemo.find(key);
  if (it != geoMemo.end()) return it->second;

  Geology G;
  const uint64_t h = belt || riftB ? mix64(key ^ seed ^ tag("geo.belt")) : cellSeed(seed, tag("geo.rock"), bi, bj);
  G.province = (uint32_t)(h >> 20) | 1u;
  bool rugged = false, remote = false;
  if (belt) {
    // an old range: granite, slate, or marble where it is wet
    const uint32_t q = (uint32_t)(h % 10);
    G.rock = q < 5 ? Rock::Granite : q < 8 ? Rock::Slate : Rock::Marble;
    rugged = true;
    remote = ((h >> 12) % 3) == 0;
  } else if (riftB) {
    G.rock = Rock::Basalt;
    rugged = ((h >> 9) & 1) != 0;
    remote = ((h >> 12) % 3) == 0;
  } else {
    // the macro fields averaged round the seed point
    int64_t e = 0, t = 0, m = 0, rg = 0;
    int lvMax = 0, n = 0;
    for (int k = 0; k < 5; k++) {
      const int32_t sx = bpx + (k == 1 ? 60 : k == 2 ? -60 : 0), sy = bpy + (k == 3 ? 60 : k == 4 ? -60 : 0);
      const Coarse c = coarse(sx, sy);
      e += c.e; t += c.t; m += c.m; rg += c.ridge;
      lvMax = std::max(lvMax, gen::levelOf(c.e));
      n++;
    }
    e /= n; t /= n; m /= n; rg /= n;
    const int lv = gen::levelOf((int32_t)e);
    const bool sea = e < ELEV_SEA;
    if (lv >= 3 || rg > gen::Q(0.25)) G.rock = m > gen::Q(0.56) ? Rock::Marble : ((h & 3) == 0 ? Rock::Slate : Rock::Granite);
    else if (t > gen::Q(0.62) && m < gen::Q(0.40)) G.rock = Rock::Sandstone;
    else if ((h >> 3) % 7 == 0 || (sea && (h & 1))) G.rock = Rock::Basalt;
    else G.rock = m > gen::Q(0.50) ? Rock::Limestone : ((h >> 5) & 1 ? Rock::Sandstone : Rock::Limestone);
    rugged = lvMax >= 4 || rg > gen::Q(0.30);
    remote = !kingdomAt(bpx, bpy) || gen::idist(bpx, bpy, 0, 0) > 2500;
  }
  int v[(int)Ore::COUNT];
  for (int o = 0; o < (int)Ore::COUNT; o++) {
    const int jitter = (int)((h >> (8 + o * 6)) & 63) - 32;
    v[o] = std::clamp((int)kBase[(int)G.rock][o] + jitter, 0, 255);
  }
  // the rare metals of the culture alloys: a few remote, rugged provinces
  const int ra = (int)Ore::Rare;
  v[ra] = std::min(v[ra], 60);
  if (rugged && remote && (mix64(h ^ tag("geo.rare")) % 3) == 0) v[ra] = 200 + (int)(h % 40);
  // copper and tin rarely together (bronze needs trade): one in twelve provinces may hold both
  const int cu = (int)Ore::Copper, sn = (int)Ore::Tin;
  if (v[cu] > 96 && v[sn] > 96 && (mix64(h ^ tag("geo.bronze")) % 12) != 0) { if (v[cu] >= v[sn]) v[sn] = 60 + (int)(h % 30); else v[cu] = 60 + (int)(h % 30); }
  // a clear primary (>= 160, >= 190 where the land is rugged) and at most two others above 96
  int order[(int)Ore::COUNT];
  for (int o = 0; o < (int)Ore::COUNT; o++) order[o] = o;
  std::sort(order, order + (int)Ore::COUNT, [&](int a, int b) { return v[a] != v[b] ? v[a] > v[b] : a < b; });
  v[order[0]] = std::max(v[order[0]], rugged ? 190 : 160);
  for (int k = 3; k < (int)Ore::COUNT; k++) v[order[k]] = std::min(v[order[k]], 90);
  for (int o = 0; o < (int)Ore::COUNT; o++) G.ore[o] = (uint8_t)v[o];
  if (geoMemo.size() > 4096) geoMemo.clear();
  geoMemo[key] = G;
  return G;
}

// ---- the landmass rule (VISION_PLAN 2.11: fast travel stays on one landmass). Land is judged on a 128-tile lattice of
// macro samples; a landmass is a 4-connected component of land samples, named by its first cell in (j, i) order, so
// the id is the same whichever tile asked first. A component bigger than LAND_CAP cells (a supercontinent) is 1
// wherever it is asked from: every flood of it hits the cap. Memoised per cell.
namespace {
constexpr int32_t LCELL_T = 128;
constexpr size_t LAND_CAP = 30000;
}

uint32_t EndlessSource::Impl::landmass(int32_t x, int32_t y) {
  if (coarse(x, y).e < ELEV_SEA) return 0;
  const int32_t ci = floorDiv(x + LCELL_T / 2, LCELL_T), cj = floorDiv(y + LCELL_T / 2, LCELL_T);
  auto landCell = [&](int32_t i, int32_t j) { return coarse(i * LCELL_T, j * LCELL_T).e >= ELEV_SEA; };
  // the tile is land but its lattice point is not (a coast): the nearest land lattice point of the four round it
  int32_t si = ci, sj = cj;
  if (!landCell(si, sj)) {
    bool found = false;
    for (int k = 0; k < 4 && !found; k++) {
      const int32_t ti = floorDiv(x, LCELL_T) + (k & 1), tj = floorDiv(y, LCELL_T) + (k >> 1);
      if (landCell(ti, tj)) { si = ti; sj = tj; found = true; }
    }
    if (!found) return 0x7F000000u | (uint32_t)(gen::key2(ci, cj) % 0xFFFFFFu);   // a speck of its own
  }
  const uint64_t k0 = gen::key2(si, sj);
  auto it = landMemo.find(k0);
  if (it != landMemo.end()) return it->second;
  std::vector<std::pair<int32_t, int32_t>> q{{si, sj}};
  std::unordered_map<uint64_t, uint8_t> seen{{k0, 1}};
  bool capped = false;
  for (size_t h = 0; h < q.size(); h++) {
    if (q.size() > LAND_CAP) { capped = true; break; }
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int d = 0; d < 4; d++) {
      const int32_t ni = q[h].first + dx[d], nj = q[h].second + dy[d];
      const uint64_t nk = gen::key2(ni, nj);
      if (seen.count(nk)) continue;
      seen.emplace(nk, 1);
      if (landCell(ni, nj)) q.push_back({ni, nj});
    }
  }
  uint32_t id = 1;
  if (!capped) {
    auto best = q.front();
    for (auto& c : q) if (c.second < best.second || (c.second == best.second && c.first < best.first)) best = c;
    id = (uint32_t)(mix64(seed ^ gen::key2(best.first, best.second)) >> 33) | 2u;
  }
  if (landMemo.size() > 400000) landMemo.clear();
  for (auto& c : q) landMemo[gen::key2(c.first, c.second)] = id;
  return id;
}

Geology EndlessSource::geology(int32_t gx, int32_t gy) {
  d_->makeStart();
  return d_->geology(gx, gy);
}

}  // namespace ew
