// L0 macro fields (VISION_PLAN 2.5): continents, plates with ridges and passes, elevation, climate with the temperate
// origin and the rain shadow, biomes, rock cores and relief levels. WORLD lane.
//
// Everything here is a pure function of (seed, x, y) in integer / Q16 maths. coarse() is the slow full evaluation;
// tiles read it through region-sized blocks of 8-tile samples (bilinear, at slightly warped coordinates so contours
// and coasts are not grid-straight). Blocks are a cache of pure values.
#include <algorithm>
#include <cstdint>
#include "rpg/world/gen.h"

namespace ew {
using namespace gen;

EndlessSource::Impl::Impl(uint64_t s) : seed(s) {
  blocks.cap = 96;
  traces.cap = 1024;
  hydros.cap = 96;
  regions.cap = 96;
  edges.cap = 768;
}

// ---- continents: Continent (55 %), Archipelago (20 %) or Ocean (25 %) per 6144-tile cell; the origin cell is a
//      continent centred on (0, 0) with R = 3200. C = max over the 3x3 cells, plus coast detail.
int32_t EndlessSource::Impl::continent(int32_t x, int32_t y) {
  int32_t wx = x, wy = y;
  warpQ(wx, wy, 11, 1000, mix64(seed ^ tag("c.warp")));
  warpQ(wx, wy, 9, 380, mix64(seed ^ tag("c.warp2")));
  const int32_t cx0 = floorDiv(wx + LCELL / 2, LCELL), cy0 = floorDiv(wy + LCELL / 2, LCELL);
  int32_t best = 0;
  for (int32_t cy = cy0 - 1; cy <= cy0 + 1; cy++)
    for (int32_t cx = cx0 - 1; cx <= cx0 + 1; cx++) {
      uint64_t h = cellSeed(seed, tag("c.cell"), cx, cy);
      int kind;
      int32_t px, py, R;
      if (cx == 0 && cy == 0) { kind = 0; px = 0; py = 0; R = 3200; }
      else {
        int32_t q = hq(h);
        kind = q < Q(0.55) ? 0 : q < Q(0.75) ? 1 : 2;
        px = cx * LCELL + (int32_t)((mix64(h ^ 1) >> 33) % 2001) - 1000;
        py = cy * LCELL + (int32_t)((mix64(h ^ 2) >> 33) % 2001) - 1000;
        R = 1800 + (int32_t)((mix64(h ^ 3) >> 33) % 1201);
      }
      if (kind == 2) continue;
      int64_t d = idist(wx, wy, px, py);
      if (d > (int64_t)R * 2) continue;
      int32_t r = (int32_t)((d << 16) / R);
      int32_t v;
      if (kind == 0) v = Q_ONE - sstep(Q(0.75), Q(1.15), r);
      else {
        if (r > Q(1.15)) continue;
        int32_t f = fbmQ(x, y, 9, 3, mix64(h ^ 0xA7u));
        int32_t fade = Q_ONE - sstep(Q(0.65), Q(1.15), r);
        v = qm(Q(0.5) + (f - Q(0.57)) * 4, fade);
      }
      best = std::max(best, v);
    }
  // coast detail: big bays and peninsulas (1024-tile scale), capes and inlets (512, 256), then the fine wiggle
  best += qm(centered(vnoiseQ(x, y, 10, mix64(seed ^ tag("c.d0"))), Q(1.6)), Q(0.30));
  best += qm(centered(vnoiseQ(x, y, 9, mix64(seed ^ tag("c.d9"))), Q(1.6)), Q(0.13));
  best += qm(centered(fbmQ(x, y, 8, 3, mix64(seed ^ tag("c.d1"))), Q(2.0)), Q(0.08));
  best += qm(centered(fbmQ(x, y, 6, 2, mix64(seed ^ tag("c.d2"))), Q(2.0)), Q(0.04));
  return best;
}

// ---- plates on a 1536-tile jittered lattice. Convergent boundaries raise ridges (long ranges), divergent ones sink
//      rifts. The distance to the boundary is the exact distance to the bisector of the two nearest plate points; a
//      pass field knocks notches out of the ridges so every range has passes.
void EndlessSource::Impl::plates(int32_t x, int32_t y, int32_t& ridge, int32_t& rift) {
  const int32_t PC = 1536, PJ = 1152, PM = (PC - PJ) / 2;
  int32_t wx = x, wy = y;
  warpQ(wx, wy, 9, 170, mix64(seed ^ tag("p.warp")));
  const int32_t cx0 = floorDiv(wx, PC), cy0 = floorDiv(wy, PC);
  int64_t d1 = INT64_MAX, d2 = INT64_MAX;
  int32_t ax = 0, ay = 0, bx = 0, by = 0;
  uint64_t ha = 0, hb = 0;
  for (int32_t cy = cy0 - 1; cy <= cy0 + 1; cy++)
    for (int32_t cx = cx0 - 1; cx <= cx0 + 1; cx++) {
      uint64_t h = cellSeed(seed, tag("p.cell"), cx, cy);
      int32_t px = cx * PC + PM + (int32_t)((h & 0xFFFFFFFFull) % (uint64_t)PJ);
      int32_t py = cy * PC + PM + (int32_t)((h >> 32) % (uint64_t)PJ);
      int64_t d = dist2(wx, wy, px, py);
      if (d < d1) { d2 = d1; bx = ax; by = ay; hb = ha; d1 = d; ax = px; ay = py; ha = h; }
      else if (d < d2) { d2 = d; bx = px; by = py; hb = h; }
    }
  ridge = 0; rift = 0;
  int32_t ab = std::max<int32_t>(1, idist(ax, ay, bx, by));
  int64_t db = (d2 - d1) / (2 * (int64_t)ab);   // tiles to the boundary
  const int32_t W = 120;
  if (db >= W) return;
  int32_t tq = (int32_t)((db << 16) / W);
  int32_t b = Q_ONE - qm(tq, tq);
  b = qm(b, b);   // (1 - t^2)^2
  // drift vectors and the boundary normal
  int32_t vax = hq(mix64(ha ^ 11)) * 2 - Q_ONE, vay = hq(mix64(ha ^ 12)) * 2 - Q_ONE;
  int32_t vbx = hq(mix64(hb ^ 11)) * 2 - Q_ONE, vby = hq(mix64(hb ^ 12)) * 2 - Q_ONE;
  int32_t nx = (int32_t)(((int64_t)(bx - ax) << 16) / ab), ny = (int32_t)(((int64_t)(by - ay) << 16) / ab);
  int32_t conv = qm(vax - vbx, nx) + qm(vay - vby, ny);
  int32_t up = sstep(Q(0.10), Q(0.80), conv), down = sstep(Q(0.35), Q(1.0), -conv);
  if (!up && !down) return;
  // crest texture: ridged noise, so the range has peaks, saddles and spurs
  int32_t r1 = vnoiseQ(x, y, 7, mix64(seed ^ tag("p.r1"))), r2 = vnoiseQ(x, y, 5, mix64(seed ^ tag("p.r2")));
  int32_t c1 = Q_ONE - std::abs(2 * r1 - Q_ONE), c2 = Q_ONE - std::abs(2 * r2 - Q_ONE);
  int32_t tex = Q(0.50) + qm(Q(0.34), c1) + qm(Q(0.16), c2);
  // passes: notches where a slow noise field is low (about one pass per 300-500 tiles of range)
  int32_t pn = vnoiseQ(wx, wy, 8, mix64(seed ^ tag("p.pass")));
  int32_t passMul = sstep(Q(0.20), Q(0.36), pn);
  ridge = qm(qm(qm(up, b), tex), passMul);
  rift = qm(down, b);
}

int32_t EndlessSource::Impl::elevation(int32_t x, int32_t y, int32_t* cOut, int32_t* ridgeOut, int hillOct) {
  int32_t C = continent(x, y);
  int32_t ridge = 0, rift = 0;
  plates(x, y, ridge, rift);
  int32_t land = sstep(Q(0.48), Q(0.68), C);
  int32_t rough = Q(0.45) + qm(Q(0.9), sstep(Q(0.30), Q(0.70), fbmQ(x, y, 11, 2, mix64(seed ^ tag("e.rough")))));
  int32_t hills = centered(fbmQ(x, y, 8, hillOct, mix64(seed ^ tag("e.hills"))), Q(1.7));
  int32_t e = Q(0.30) + qm(Q(0.55), C - Q(0.5)) + qm(qm(Q(0.105), hills), rough) + qm(Q(0.44), qm(ridge, land)) -
              qm(Q(0.08), qm(rift, land));
  if (cOut) *cOut = C;
  if (ridgeOut) *ridgeOut = qm(ridge, land);
  return e;
}

EndlessSource::Impl::Coarse EndlessSource::Impl::coarse(int32_t x, int32_t y) {
  Coarse c;
  c.e = elevation(x, y, &c.c, &c.ridge);
  // temperature: a broad field, pulled toward temperate near the origin, minus the lapse with height
  int32_t t0 = Q(0.55) + qm(Q(0.36), centered(fbmQ(x, y, 12, 4, mix64(seed ^ tag("m.temp"))), Q(1.6)));
  int32_t d = idist(x, y, 0, 0);
  int32_t pull = d < 1500 ? Q(0.6) : d < 2500 ? (int32_t)((int64_t)Q(0.6) * (2500 - d) / 1000) : 0;
  t0 += qm(Q(0.55) - t0, pull);
  c.t = t0 - qm(Q(1.25), std::max(0, c.e - Q(0.60)));
  // moisture: a broad field, wetter near coasts, drier in the rain shadow east of ranges (fixed westerly wind)
  int32_t mf = centered(fbmQ(x, y, 11, 5, mix64(seed ^ tag("m.moist"))), Q(1.6));
  int32_t coastProx = Q_ONE - sstep(Q(0.50), Q(0.78), c.c);
  int32_t shR = 0, shRift = 0;
  plates(x - 200, y, shR, shRift);
  int32_t shadow = std::max(0, shR - c.ridge);
  c.m = Q(0.535) + qm(Q(0.30), mf) + qm(Q(0.16), coastProx) - qm(Q(0.30), shadow);
  // rock: only the steep cores of high ridges
  c.rock = qm(sstep(Q(0.89), Q(0.95), c.e), sstep(Q(0.55), Q(0.80), c.ridge));
  return c;
}

std::shared_ptr<const Block> EndlessSource::Impl::block(int32_t bx, int32_t by) {
  if (lastBlock && bx == lastBx && by == lastBy) return lastBlock;
  uint64_t k = key2(bx, by);
  std::shared_ptr<Block> b = blocks.get(k);
  if (!b) {
    Nest nest(*this);
    auto t0 = Clock::now();
    b = std::make_shared<Block>();
    for (int j = -1; j <= BLK + 1; j++)
      for (int i = -1; i <= BLK + 1; i++) b->s[(j + 1) * BS + (i + 1)] = coarse((bx * BLK + i) * CG, (by * BLK + j) * CG);
    // relief cleanup: a sample whose level stands above (below) all 8 neighbours is lowered (raised) one level, so
    // no plateau or pit is smaller than about two samples (16 tiles) across
    for (int j = -1; j <= BLK + 1; j++)
      for (int i = -1; i <= BLK + 1; i++) {
        int idx = (j + 1) * BS + (i + 1);
        int32_t e = b->s[idx].e;
        b->eClean[idx] = e;
        if (i < 0 || j < 0 || i > BLK || j > BLK || e < ELEV_SEA) continue;
        int lv = levelOf(e), hi = -1, lo = 99;
        for (int dj = -1; dj <= 1; dj++)
          for (int di = -1; di <= 1; di++) {
            if (!di && !dj) continue;
            int l2 = levelOf(b->s[(j + 1 + dj) * BS + (i + 1 + di)].e);
            hi = std::max(hi, l2);
            lo = std::min(lo, l2);
          }
        if (lv > hi && lv >= 1) b->eClean[idx] = levelFloor(lv) - 1;
        else if (lv < lo) b->eClean[idx] = levelFloor(lv + 1);
      }
    stats.blocks++;
    stats.blockMs += msSince(t0);
    blocks.put(k, b);
  }
  lastBlock = b;
  lastBx = bx; lastBy = by;
  return b;
}

namespace {
inline int32_t bil(int32_t a, int32_t b, int32_t c, int32_t d, int32_t fx, int32_t fy) {
  int32_t ab = a + qm(b - a, fx), cd = c + qm(d - c, fx);
  return ab + qm(cd - ab, fy);
}
}  // namespace

Biome EndlessSource::Impl::classify(int32_t e, int32_t t, int32_t m, int32_t x, int32_t y, bool sea) {
  if (sea) return Biome::Ocean;
  if (e < ELEV_SEA + Q(0.010)) return Biome::Beach;
  if (t < Q(0.19)) return Biome::Snow;
  if (t < Q(0.31)) return Biome::Taiga;
  if (t > Q(0.72) && m < Q(0.44)) return Biome::Desert;
  if (m > Q(0.68) && e < Q(0.45) && fbmQ(x, y, 9, 2, mix64(seed ^ tag("m.swamp"))) > Q(0.47)) return Biome::Swamp;
  if (m > Q(0.44) && t < Q(0.58) && fbmQ(x, y, 8, 3, mix64(seed ^ tag("m.autumn"))) > Q(0.63)) return Biome::Autumn;
  if (m > Q(0.52)) return Biome::Forest;
  return Biome::Plains;
}

Ground EndlessSource::Impl::groundFor(Biome b, int32_t e, int32_t t, int32_t x, int32_t y) {
  switch (b) {
    case Biome::Ocean: return e < ELEV_SEA - Q(0.035) ? Ground::DeepWater : Ground::Water;
    case Biome::Beach: return t < Q(0.25) ? Ground::Snow : Ground::Sand;
    case Biome::Mountain: return Ground::Rock;
    case Biome::Snow: return Ground::Snow;
    case Biome::Taiga: return Ground::Tundra;
    case Biome::Desert: return Ground::Sand;
    case Biome::Swamp: return vnoiseQ(x * 4, y * 4, 4, mix64(seed ^ tag("g.swamp"))) > Q(0.74) ? Ground::Water : Ground::Swamp;
    case Biome::Autumn: return Ground::Autumn;
    case Biome::Forest: return Ground::ForestFloor;
    default: return fbmQ(x, y, 4, 3, mix64(seed ^ tag("g.meadow"))) > Q(0.60) ? Ground::Meadow : Ground::Grass;
  }
}

TileF EndlessSource::Impl::tile(int32_t x, int32_t y) {
  int32_t wx = x, wy = y;
  warpQ(wx, wy, 5, 5, mix64(seed ^ tag("t.warp")));
  const int32_t bx = wx >> REGION_SHIFT, by = wy >> REGION_SHIFT;
  const Block& B = *block(bx, by);
  const int32_t lx = wx - bx * REGION, ly = wy - by * REGION;
  const int i = lx >> CG_SHIFT, j = ly >> CG_SHIFT;
  const int32_t fx = (lx & (CG - 1)) << (16 - CG_SHIFT), fy = (ly & (CG - 1)) << (16 - CG_SHIFT);
  const Coarse &a = B.at(i, j), &b = B.at(i + 1, j), &c = B.at(i, j + 1), &d = B.at(i + 1, j + 1);
  TileF f;
  int32_t e = bil(a.e, b.e, c.e, d.e, fx, fy);
  int32_t ec = bil(B.clean(i, j), B.clean(i + 1, j), B.clean(i, j + 1), B.clean(i + 1, j + 1), fx, fy);
  f.t = bil(a.t, b.t, c.t, d.t, fx, fy);
  f.m = bil(a.m, b.m, c.m, d.m, fx, fy);
  f.ridge = bil(a.ridge, b.ridge, c.ridge, d.ridge, fx, fy);
  int32_t rock = bil(a.rock, b.rock, c.rock, d.rock, fx, fy);
  // coast detail (water decisions only; relief reads the clean field)
  e += qm(centered(vnoiseQ(x, y, 3, mix64(seed ^ tag("t.d1"))), Q(1.0)), Q(0.010)) +
       qm(centered(vnoiseQ(x, y, 1, mix64(seed ^ tag("t.d2"))), Q(1.0)), Q(0.004));
  f.e = e;
  f.sea = e < ELEV_SEA;
  f.h = f.sea ? 0 : (uint8_t)levelOf(ec);
  f.rock = !f.sea && rock > Q(0.5) + qm(centered(vnoiseQ(x, y, 2, mix64(seed ^ tag("t.rk"))), Q(1.0)), Q(0.12));
  // ecotones: the climate edges interleave over a few tiles instead of running along the interpolation
  int32_t tj = qm(centered(vnoiseQ(x, y, 5, mix64(seed ^ tag("t.tj"))), Q(1.0)), Q(0.03)) +
               qm(centered(vnoiseQ(x, y, 3, mix64(seed ^ tag("t.tj2"))), Q(1.0)), Q(0.01));
  int32_t mj = qm(centered(vnoiseQ(x, y, 5, mix64(seed ^ tag("t.mj"))), Q(1.0)), Q(0.035)) +
               qm(centered(vnoiseQ(x, y, 3, mix64(seed ^ tag("t.mj2"))), Q(1.0)), Q(0.012));
  // beaches: a band a few tiles wide along the sea (not an elevation band, which runs for hundreds of tiles across
  // flat coastal plains)
  bool beach = false;
  if (!f.sea && e < ELEV_SEA + Q(0.05)) {
    const int32_t r = 3 + (int32_t)((vnoiseQ(x, y, 4, mix64(seed ^ tag("t.beach"))) * 5) >> 16);   // 3..7 tiles
    static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1}, dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    for (int k = 0; k < 8 && !beach; k++) {
      const int32_t rr = k < 4 ? r : r * 7 / 10;
      if (waterE(x + dx[k] * rr, y + dy[k] * rr) < ELEV_SEA) beach = true;
    }
  }
  if (f.rock) f.biome = Biome::Mountain;
  else if (f.sea) f.biome = Biome::Ocean;
  else if (beach) f.biome = Biome::Beach;
  else f.biome = classify(std::max(e, ELEV_SEA + Q(0.011)), f.t + tj, f.m + mj, x, y, false);
  return f;
}

int32_t EndlessSource::Impl::waterE(int32_t x, int32_t y) {
  int32_t wx = x, wy = y;
  warpQ(wx, wy, 5, 5, mix64(seed ^ tag("t.warp")));
  const int32_t bx = wx >> REGION_SHIFT, by = wy >> REGION_SHIFT;
  const Block& B = *block(bx, by);
  const int32_t lx = wx - bx * REGION, ly = wy - by * REGION;
  const int i = lx >> CG_SHIFT, j = ly >> CG_SHIFT;
  const int32_t fx = (lx & (CG - 1)) << (16 - CG_SHIFT), fy = (ly & (CG - 1)) << (16 - CG_SHIFT);
  int32_t e = bil(B.at(i, j).e, B.at(i + 1, j).e, B.at(i, j + 1).e, B.at(i + 1, j + 1).e, fx, fy);
  return e + qm(centered(vnoiseQ(x, y, 3, mix64(seed ^ tag("t.d1"))), Q(1.0)), Q(0.010)) +
         qm(centered(vnoiseQ(x, y, 1, mix64(seed ^ tag("t.d2"))), Q(1.0)), Q(0.004));
}

int EndlessSource::Impl::natLevel(int32_t x, int32_t y) {
  int32_t wx = x, wy = y;
  warpQ(wx, wy, 5, 5, mix64(seed ^ tag("t.warp")));
  const int32_t bx = wx >> REGION_SHIFT, by = wy >> REGION_SHIFT;
  const Block& B = *block(bx, by);
  const int32_t lx = wx - bx * REGION, ly = wy - by * REGION;
  const int i = lx >> CG_SHIFT, j = ly >> CG_SHIFT;
  const int32_t fx = (lx & (CG - 1)) << (16 - CG_SHIFT), fy = (ly & (CG - 1)) << (16 - CG_SHIFT);
  int32_t ec = bil(B.clean(i, j), B.clean(i + 1, j), B.clean(i, j + 1), B.clean(i + 1, j + 1), fx, fy);
  return levelOf(ec);
}

MacroSample EndlessSource::Impl::macro(int32_t x, int32_t y) {
  TileF f = tile(x, y);
  MacroSample m;
  m.elev = f.e;
  m.temp = f.t;
  m.moist = f.m;
  m.biome = f.biome;
  m.height = f.h;
  m.water = f.sea || waterAt(x, y);
  m.kingdom = kingdomAt(x, y);
  return m;
}

}  // namespace ew
