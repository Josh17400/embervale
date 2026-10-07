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
EndlessSource::Impl::PlatePt EndlessSource::Impl::platePoint(int32_t cx, int32_t cy) {
  const int32_t PC = 1536, PJ = 1152, PM = (PC - PJ) / 2;
  const uint64_t h = cellSeed(seed, tag("p.cell"), cx, cy);
  return PlatePt{cx * PC + PM + (int32_t)((h & 0xFFFFFFFFull) % (uint64_t)PJ), cy * PC + PM + (int32_t)((h >> 32) % (uint64_t)PJ), h};
}

void EndlessSource::Impl::plates(int32_t x, int32_t y, int32_t& ridge, int32_t& rift, uint64_t* pair) {
  const int32_t PC = 1536;
  int32_t wx = x, wy = y;
  warpQ(wx, wy, 9, 170, mix64(seed ^ tag("p.warp")));
  const int32_t cx0 = floorDiv(wx, PC), cy0 = floorDiv(wy, PC);
  int64_t d1 = INT64_MAX, d2 = INT64_MAX;
  int32_t ax = 0, ay = 0, bx = 0, by = 0;
  uint64_t ha = 0, hb = 0;
  uint64_t ka = 0, kb = 0;
  for (int32_t cy = cy0 - 1; cy <= cy0 + 1; cy++)
    for (int32_t cx = cx0 - 1; cx <= cx0 + 1; cx++) {
      const PlatePt pp = platePoint(cx, cy);
      const uint64_t h = pp.h;
      int32_t px = pp.x, py = pp.y;
      int64_t d = dist2(wx, wy, px, py);
      if (d < d1) { d2 = d1; bx = ax; by = ay; hb = ha; kb = ka; d1 = d; ax = px; ay = py; ha = h; ka = key2(cx, cy); }
      else if (d < d2) { d2 = d; bx = px; by = py; hb = h; kb = key2(cx, cy); }
    }
  if (pair) *pair = ka < kb ? mix64(ka) ^ kb : mix64(kb) ^ ka;
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
    b->bx = bx; b->by = by;
    for (int j = -1; j <= BLK + 1; j++)
      for (int i = -1; i <= BLK + 1; i++) {
        Coarse& cs = b->s[(j + 1) * BS + (i + 1)];
        const int32_t px = (bx * BLK + i) * CG, py = (by * BLK + j) * CG;
        cs = coarse(px, py);
        for (int k = 0; k < ECH_N; k++) cs.ch[k] = (uint16_t)std::min(65535, ecoChan(px, py, k));   // (M3c)
      }
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
        // (moved to the middle of the next level, not onto its edge: a sample sitting exactly on a level boundary
        // made the contour run along the coarse grid line, a dead-straight cliff 16 tiles long)
        if (lv > hi && lv >= 1) b->eClean[idx] = levelFloor(lv) - H_STEP / 2;
        else if (lv < lo) b->eClean[idx] = levelFloor(lv + 1) + H_STEP / 2;
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

// ---- M3c Wildlands: the biome classifier (VISION_PLAN 15.15). The family (Biome) comes from the climate as before,
//      plus the wondrous lands that override it (ash fields on basalt, crystal barrens on the heights, petrified
//      forests in dry country); the eco (the biome proper) is chosen inside the family from the climate, the relief,
//      the coast, the province rock (geology), the eco noise channels (slow value noise sampled with the coarse
//      fields), the dragons' lairs (blight) and the elven cultures (silverwood). Pure, integer maths only.
namespace {
// the eco noise channels: scale (log2 of the lattice) and tag. Rare lands read the big lattices with high thresholds
// (patches near the lattice's high points: small, organic and far apart, ~one per few lattice cells)
const int kChanShift[ECH_N] = {9, 9, 9, 10, 9, 8, 8, 7, 7, 6, 8};
const uint64_t kChanTag[ECH_N] = {tag("e.ash"), tag("e.crystal"), tag("e.petrified"), tag("e.chA"), tag("e.chB"), tag("e.chC"),
                                  tag("e.chD"), tag("e.chE"), tag("e.chF"), tag("e.chG"), tag("e.mesa")};
inline bool rockyCoast(Rock r) { return r == Rock::Granite || r == Rock::Slate || r == Rock::Basalt; }
}  // namespace

// (M3c fixer) one octave of bilinear value noise cut at a high threshold drew the rare lands as squares and diamonds
// with straight edges (contours near a lattice peak are near-straight): the domain is warped (up to 3/16 of the
// lattice, at a quarter of its scale) and a finer octave adds a ragged rim, so every patch and ecotone comes out organic.
int32_t EndlessSource::Impl::ecoChan(int32_t x, int32_t y, int k) {
  const int sh = kChanShift[k];
  const uint64_t s = mix64(seed ^ kChanTag[k]);
  int32_t wx = x, wy = y;
  warpQ(wx, wy, sh - 2, 3 << (sh - 4), mix64(s ^ 0xC4A1u));
  int32_t v = vnoiseQ(wx, wy, sh, s);
  v = 32768 + (v - 32768) * 31 / 32 + (vnoiseQ(wx, wy, sh - 2, mix64(s + 0x77u)) - 32768) / 14;
  return clampi(v, 0, 65535);
}
void EndlessSource::Impl::ecoChannels(EcoIn& in) { for (int k = 0; k < ECH_N; k++) in.ch[k] = ecoChan(in.x, in.y, k); }

Rock EndlessSource::Impl::rockIn(const EcoIn& in) {
  if (!in.blk) return provinceRock(in.x, in.y);
  const int bi = clampi(in.bi, -1, BLK + 1), bj = clampi(in.bj, -1, BLK + 1);
  uint8_t& r = in.blk->rockC[(bj + 1) * BS + (bi + 1)];
  if (r == 0xFF) { Nest nest(*this); r = (uint8_t)provinceRock((in.blk->bx * BLK + bi) * CG, (in.blk->by * BLK + bj) * CG); }
  return (Rock)r;
}

// an elven culture cell (the family's archetype: Sylvan wood elves, Starspire high elves). The family of the cell, not
// the kingdom's dialect: a pure function of the coordinates that needs no kingdom (and no start plan)
bool EndlessSource::Impl::elvenAt(int32_t x, int32_t y) {
  const uint64_t k = key2(floorDiv(x, CCELL), floorDiv(y, CCELL));
  auto it = elvenMemo.find(k);
  if (it != elvenMemo.end()) return it->second;
  Nest nest(*this);   // (a culture family is built once: not the chunk's own cost)
  const cult::Culture& c = atlas().family(floorDiv(x, CCELL), floorDiv(y, CCELL));
  const bool elf = c.archetype == cult::Archetype::Sylvan || c.archetype == cult::Archetype::Starspire ||
                   (c.isolated && (c.archetype2 == cult::Archetype::Sylvan || c.archetype2 == cult::Archetype::Starspire));
  if (elvenMemo.size() > 4096) elvenMemo.clear();
  elvenMemo.emplace(k, elf);
  return elf;
}

// The dragons' blight: the land within 80..200 tiles of a lair (the radius wanders with the angle and the ground) is
// sickened. Only once the start plan exists (the lairs know their cells then), never within 300 tiles of the start
// village, and only round a lair the region planner keeps (no forced settlement crowds it).
bool EndlessSource::Impl::blightIn(const EcoIn& in) {
  if (!started || forced.empty()) return false;
  if (dist2(in.x, in.y, forced[0].x, forced[0].y) < 300ll * 300ll) return false;
  std::vector<LairPt> local;
  const std::vector<LairPt>* L = &local;
  auto gather = [&](int32_t x0, int32_t y0, int32_t x1, int32_t y1, std::vector<LairPt>& out) {
    const int32_t M = 230;
    for (int32_t ly = floorDiv(y0 - M + 512, 2048); ly <= floorDiv(y1 + M + 512, 2048); ly++)
      for (int32_t lx = floorDiv(x0 - M + 512, 2048); lx <= floorDiv(x1 + M + 512, 2048); lx++) {
        int32_t ax, ay;
        uint32_t sd;
        if (!lairOf(lx, ly, ax, ay, sd)) continue;
        if (ax < x0 - M || ay < y0 - M || ax > x1 + M || ay > y1 + M) continue;
        bool live = true;
        for (const Node& n : forced)
          if (dist2(n.x, n.y, ax, ay) < (int64_t)(nominalR(n.type) + 60) * (nominalR(n.type) + 60)) live = false;
        if (live) out.push_back(LairPt{ax, ay, sd});
      }
    for (const SitePlan& f : forcedSites)
      if (f.type == SiteType::DragonLair && f.ex >= x0 - M && f.ey >= y0 - M && f.ex <= x1 + M && f.ey <= y1 + M)
        out.push_back(LairPt{f.ex, f.ey, f.seed});
  };
  if (in.blk) {
    if (!in.blk->lairsReady) {
      Nest nest(*this);
      const int32_t x0 = in.blk->bx * REGION, y0 = in.blk->by * REGION;
      gather(x0 - 16, y0 - 16, x0 + REGION + 16, y0 + REGION + 16, in.blk->lairs);
      in.blk->lairsReady = true;
    }
    L = &in.blk->lairs;
  } else {
    gather(in.x, in.y, in.x, in.y, local);
  }
  for (const LairPt& p : *L) {
    const int64_t d2 = dist2(in.x, in.y, p.x, p.y);
    if (d2 > 212ll * 212ll) continue;
    const uint64_t ls = mix64(seed ^ p.seed ^ tag("blight"));
    const int32_t R = 80 + (int32_t)(((int64_t)vnoiseQ(in.x, in.y, 6, ls) * 120) >> 16) +
                      (int32_t)(((int64_t)(vnoiseQ(in.x, in.y, 3, ls ^ 0x55u) - 32768) * 12) >> 16);
    if (d2 < (int64_t)R * R) return true;
  }
  return false;
}

// Badlands mesas and terraces: in hot, dry country (where the mesa channel is high) flat-topped mesas stand one level
// above the land, the big ones with a second step on their cores. Each is a wobbly disc (radius 12-22 tiles) round a
// point of a jittered 48-tile lattice, chosen whole by the climate at its point, so there are no slivers of mesa at a
// climate fringe; and each has a stairway up its south side (mesaStair: the chunk's relief turns those faces into
// ramps), so no mesa top is walled in. natLevel adds the lift.
const EndlessSource::Impl::Mesa& EndlessSource::Impl::mesaCell(int32_t ci, int32_t cj) {
  const uint64_t k = key2(ci, cj);
  auto it = mesaMemo.find(k);
  if (it != mesaMemo.end()) return it->second;
  if (mesaMemo.size() > 60000) mesaMemo.clear();
  Nest nest(*this);
  Mesa M;
  const uint64_t h = cellSeed(seed, tag("mesa.cell"), ci, cj);
  if (hq(h) < Q(0.62)) {
    M.x = ci * 48 + 10 + (int32_t)((h >> 8) % 28);
    M.y = cj * 48 + 10 + (int32_t)((h >> 20) % 28);
    M.r = 12 + (int32_t)((h >> 32) % 11);
    // the climate at its point (the coarse fields themselves: no block is touched from inside natLevel)
    const Coarse cc = coarse(M.x, M.y);
    const int32_t t = cc.t, m = cc.m, e = cc.e, ch = ecoChan(M.x, M.y, ECH_MESA);
    M.ok = t > Q(0.69) && m < Q(0.45) && ch > Q(0.46) && e > ELEV_SEA + Q(0.04) && levelOf(e) <= 3;
  }
  return mesaMemo.emplace(k, M).first->second;
}

int EndlessSource::Impl::mesaLift(int32_t x, int32_t y, const Block& B, int i, int j, int32_t fx, int32_t fy) {
  (void)fx; (void)fy;
  const Coarse &a = B.at(i, j), &b = B.at(i + 1, j), &c = B.at(i, j + 1), &d = B.at(i + 1, j + 1);
  // a cheap early out: none of the four samples is hot and dry
  if (std::max({a.t, b.t, c.t, d.t}) < Q(0.66) || std::min({a.m, b.m, c.m, d.m}) > Q(0.48)) return 0;
  int lift = 0;
  const int32_t ci0 = floorDiv(x, 48), cj0 = floorDiv(y, 48);
  int32_t wob = -1;
  for (int32_t cj = cj0 - 1; cj <= cj0 + 1; cj++)
    for (int32_t ci = ci0 - 1; ci <= ci0 + 1; ci++) {
      const Mesa& M = mesaCell(ci, cj);
      if (!M.ok) continue;
      const int64_t d2 = dist2(x, y, M.x, M.y);
      if (d2 > (int64_t)(M.r + 6) * (M.r + 6)) continue;
      if (wob < 0) wob = vnoiseQ(x, y, 4, mix64(seed ^ tag("mesa.wob"))) * 3 / 4 + vnoiseQ(x, y, 3, mix64(seed ^ tag("mesa.wob2"))) / 4;
      const int32_t R = M.r * (52 + (int32_t)((wob * 24) >> 16)) / 64;   // 0.81 .. 1.19 r
      if (d2 >= (int64_t)R * R) continue;
      const int32_t R2 = R * 9 / 20;
      lift = std::max(lift, M.r >= 16 && d2 < (int64_t)R2 * R2 ? 2 : 1);
    }
  return lift;
}

bool EndlessSource::Impl::mesaStair(int32_t x, int32_t y) {
  const int32_t ci0 = floorDiv(x, 48), cj0 = floorDiv(y, 48);
  for (int32_t cj = cj0 - 1; cj <= cj0; cj++)
    for (int32_t ci = ci0 - 1; ci <= ci0 + 1; ci++) {
      const Mesa& M = mesaCell(ci, cj);
      if (M.ok && std::abs(x - M.x) <= 1 && y > M.y && y <= M.y + M.r + 6) return true;
    }
  return false;
}

Biome EndlessSource::Impl::classifyIn(const EcoIn& in, bool sea) {
  if (sea) return Biome::Ocean;
  const int32_t e = in.e, t = in.t, m = in.m;
  if (e < ELEV_SEA + Q(0.010)) return Biome::Beach;
  // the wondrous lands (rare patches that override the climate's family)
  if (in.ch[ECH_ASH] > Q(0.86) && t > Q(0.30) && ((in.ch[ECH_ASH] > Q(0.93) && t > Q(0.50)) || rockIn(in) == Rock::Basalt)) return Biome::Desert;
  if (in.ch[ECH_CRYSTAL] > Q(0.922) && in.lv >= 2) return Biome::Desert;
  if (in.ch[ECH_PETRIFIED] > Q(0.895) && m < Q(0.54) && t > Q(0.40)) return Biome::Desert;
  if (t < Q(0.19)) return Biome::Snow;
  if (t < Q(0.31)) return Biome::Taiga;
  if (t > Q(0.69) && m < Q(0.43)) return Biome::Desert;
  if (m > Q(0.68) && e < Q(0.45) && fbmQ(in.x, in.y, 9, 2, mix64(seed ^ tag("m.swamp"))) > Q(0.47)) return Biome::Swamp;
  if (m > Q(0.44) && t < Q(0.58) && fbmQ(in.x, in.y, 8, 3, mix64(seed ^ tag("m.autumn"))) > Q(0.63)) return Biome::Autumn;
  if (m > Q(0.52)) return Biome::Forest;
  return Biome::Plains;
}

Biome EndlessSource::Impl::classify(int32_t e, int32_t t, int32_t m, int32_t x, int32_t y, bool sea) {
  if (sea) return Biome::Ocean;
  EcoIn in;
  in.e = e; in.t = t; in.m = m; in.x = x; in.y = y; in.lv = levelOf(e); in.far = true;
  in.ch[ECH_ASH] = ecoChan(x, y, ECH_ASH);
  in.ch[ECH_CRYSTAL] = ecoChan(x, y, ECH_CRYSTAL);
  in.ch[ECH_PETRIFIED] = ecoChan(x, y, ECH_PETRIFIED);
  return classifyIn(in, false);
}

// the eco inside its family
Eco EndlessSource::Impl::ecoIn(Biome fam, const EcoIn& in) {
  const int32_t e = in.e, t = in.t, m = in.m, ridge = in.ridge;
  const int lv = in.lv;
  const int32_t* ch = in.ch;
  switch (fam) {
    case Biome::Ocean: return Eco::Ocean;
    case Biome::Mountain: return Eco::Mountain;
    case Biome::Beach: {
      // sea cliffs and fjords where relief meets the sea (a raised shore, a ridge, a hard rock coast in the cold)
      if (lv >= 1 || ridge > Q(0.12)) return Eco::SeaCliffs;
      if (t < Q(0.44) && ch[ECH_D] > Q(0.62) && rockyCoast(rockIn(in))) return Eco::SeaCliffs;
      if (t > Q(0.62) && m > Q(0.40)) return ch[ECH_F] > Q(0.86) ? Eco::SandBeach : Eco::CoralCoast;   // warm seas: white coral sand
      if (t < Q(0.36)) return ch[ECH_F] > Q(0.28) ? Eco::Shingle : Eco::SandBeach;                    // cold seas: shingle
      if (ch[ECH_F] > Q(0.74) || (ch[ECH_F] > Q(0.55) && rockyCoast(rockIn(in)))) return Eco::Shingle;
      return Eco::SandBeach;
    }
    case Biome::Plains: {
      if (lv <= 3 && ch[ECH_A] > Q(0.875)) return Eco::StonePlains;
      if (lv >= 6 || (lv >= 5 && t < Q(0.46))) return Eco::AlpineMeadow;
      if (m > Q(0.49) && lv <= 3 && ch[ECH_B] > Q(0.70)) return Eco::LakeDistrict;
      if (t > Q(0.60) && m < Q(0.54)) return Eco::Savanna;
      if (m < Q(0.385) && t < Q(0.50)) return Eco::Steppe;
      if (lv >= 1 && m > Q(0.38) && m < Q(0.64) && t > Q(0.36) && t < Q(0.62) && ch[ECH_D] > Q(0.36)) {
        const Rock r = rockIn(in);
        if (r == Rock::Limestone || r == Rock::Marble) return Eco::ChalkDowns;
      }
      if (t < Q(0.50) && m > Q(0.46) && (lv >= 2 || ch[ECH_C] > Q(0.58))) return Eco::Heath;
      if (m < Q(0.435)) return Eco::Prairie;
      if (ch[ECH_G] > Q(0.62)) return Eco::FlowerMeadow;
      return Eco::Meadow;
    }
    case Biome::Forest: {
      if (m > Q(0.56) && ch[ECH_B] > Q(0.89)) return Eco::MushroomForest;
      if (ch[ECH_C] > Q(0.74) && !in.far && elvenAt(in.x + ((ch[ECH_G] - 32768) >> 10), in.y + ((ch[ECH_E] - 32768) >> 10))) return Eco::Silverwood;
      if (m > Q(0.50) && ch[ECH_A] > Q(0.80)) return Eco::DarkForest;
      if (t > Q(0.64) && m > Q(0.63)) return Eco::Jungle;
      if (t > Q(0.56) && m > Q(0.56) && ch[ECH_E] > Q(0.60)) return Eco::BambooForest;
      if (m > Q(0.64) && t > Q(0.38) && ch[ECH_D] > Q(0.56)) return Eco::GiantForest;
      if (t < Q(0.45) && ch[ECH_F] > Q(0.36)) return Eco::BirchWood;
      if (t > Q(0.46) && t < Q(0.63) && ch[ECH_E] < Q(0.24)) return Eco::BlossomGrove;
      return Eco::MixedForest;
    }
    case Biome::Autumn: return Eco::AutumnWood;
    case Biome::Taiga: return m > Q(0.56) && lv <= 2 && ch[ECH_B] > Q(0.50) ? Eco::TaigaBog : Eco::Taiga;
    case Biome::Snow:
      if (t < Q(0.06) || lv >= 7) return Eco::Glacier;
      if (lv <= 5 && ch[ECH_A] > Q(0.57)) return Eco::FrozenLakes;
      if (lv <= 5 && (m < Q(0.52) || ch[ECH_D] > Q(0.46))) return Eco::Tundra;
      return Eco::SnowField;
    case Biome::Swamp:
      if (t > Q(0.60) && e < ELEV_SEA + Q(0.05)) return Eco::Mangrove;
      if (t < Q(0.45) || (t < Q(0.55) && ch[ECH_C] > Q(0.68))) return Eco::PeatBog;
      if (ch[ECH_B] > Q(0.52)) return Eco::FloodedForest;
      return Eco::ReedMarsh;
    case Biome::Desert: {
      // the wondrous lands first (the same tests classifyIn made)
      if (ch[ECH_ASH] > Q(0.86) && t > Q(0.30) && ((ch[ECH_ASH] > Q(0.93) && t > Q(0.50)) || rockIn(in) == Rock::Basalt)) return Eco::AshFields;
      if (ch[ECH_CRYSTAL] > Q(0.922) && lv >= 2) return Eco::CrystalBarrens;
      if (ch[ECH_PETRIFIED] > Q(0.895) && m < Q(0.54) && t > Q(0.40)) return Eco::PetrifiedForest;
      if (m > Q(0.27) && lv <= 2 && ch[ECH_E] > Q(0.86)) return Eco::Oasis;
      if (lv >= 2 || ridge > Q(0.20)) return Eco::Badlands;
      if (ch[ECH_MESA] > Q(0.50) && rockIn(in) == Rock::Sandstone) return Eco::Badlands;
      if (m < Q(0.34) && lv <= 1 && ch[ECH_B] > Q(0.54)) return Eco::SaltFlats;
      if (m > Q(0.37)) return Eco::Scrubland;
      if (ch[ECH_C] > Q(0.54)) return Eco::StonyDesert;
      { const Rock r = rockIn(in); if (r == Rock::Granite || r == Rock::Basalt || r == Rock::Slate) return Eco::StonyDesert; }
      return Eco::Dunes;
    }
    default: return ecoOfFamily(fam);
  }
}

// the coarse form (maps, habitability, cultures, the far estimates): channels and context drawn at the point
Eco EndlessSource::Impl::ecoFor(Biome fam, int32_t e, int32_t t, int32_t m, int32_t ridge, int lv, int32_t x, int32_t y, bool cultures) {
  EcoIn in;
  in.e = e; in.t = t; in.m = m; in.ridge = ridge; in.lv = lv; in.x = x; in.y = y; in.far = !cultures;
  ecoChannels(in);
  return ecoIn(fam, in);
}

Eco EndlessSource::Impl::ecoFar(Biome fam, const Coarse& c, int32_t x, int32_t y) {
  const bool water = c.e < ELEV_SEA;
  EcoIn in;
  in.e = c.e; in.t = c.t; in.m = c.m; in.ridge = c.ridge; in.lv = water ? 0 : levelOf(c.e); in.x = x; in.y = y;
  ecoChannels(in);
  if ((fam == Biome::Plains || fam == Biome::Forest || fam == Biome::Autumn || fam == Biome::Taiga) && blightIn(in)) return Eco::Blight;
  return ecoIn(fam, in);
}

// The ash fields' lava rifts: on a jittered 20-tile lattice about half the cells hold one crack, a straight-ish cut 7
// to 13 tiles long at its own angle, a tile or two wide with a wavering edge. Each crack is short and the cracks are
// far apart, so they never join into a ring that walls off land.
bool EndlessSource::Impl::lavaRift(int32_t x, int32_t y) {
  const int32_t ci0 = floorDiv(x, 20), cj0 = floorDiv(y, 20);
  for (int32_t cj = cj0 - 1; cj <= cj0 + 1; cj++)
    for (int32_t ci = ci0 - 1; ci <= ci0 + 1; ci++) {
      const uint64_t h = cellSeed(seed, tag("g.rift"), ci, cj);
      if (hq(h) >= Q(0.50)) continue;
      const int32_t cx = ci * 20 + 4 + (int32_t)((h >> 8) % 12), cy = cj * 20 + 4 + (int32_t)((h >> 16) % 12);
      const int32_t half = 3 + (int32_t)((h >> 24) % 4);
      if (std::abs(x - cx) > half + 2 || std::abs(y - cy) > half + 2) continue;
      const int32_t ang = (int32_t)((h >> 32) & 1023);
      const int32_t dx = icosR(ang, 1024), dy = isinR(ang, 1024);
      const int32_t rx = x - cx, ry = y - cy;
      const int32_t along = rx * dx + ry * dy, perp = rx * dy - ry * dx;   // 1024ths of a tile
      if (std::abs(along) > half * 1024) continue;
      // a crack narrows to its tips and wavers along its length
      const int32_t tip = half * 1024 - std::abs(along);
      const int32_t w = std::min<int32_t>(620 + (int32_t)(((int64_t)vnoiseQ(x, y, 2, h ^ 0x77u) * 420) >> 16), 300 + tip / 2);
      const int32_t bend = (int32_t)(((int64_t)(vnoiseQ(along >> 10, (int32_t)(h & 255), 2, h ^ 0x99u) - 32768) * 900) >> 16);
      if (std::abs(perp - bend) <= w) return true;
    }
  return false;
}

// the ground of an eco (EcoInfo::ground plus its variety): ice on the frozen lakes and glacier fields, lava rifts in
// the ash fields (short broken cracks, never a closed ring), pools in the bogs, channels in the mangroves and flooded
// woods, ponds in the lake district and at an oasis' heart. Water and lava come in small separate pieces, so they
// never wall off a stretch of land.
Ground EndlessSource::Impl::groundFor(Eco eco, int32_t el, int32_t t, int32_t x, int32_t y) {
  auto n = [&](int shift, const char* nm) { return vnoiseQ(x, y, shift, mix64(seed ^ tag(nm))); };
  switch (eco) {
    case Eco::Ocean: return el < ELEV_SEA - Q(0.035) ? Ground::DeepWater : Ground::Water;
    case Eco::SandBeach: case Eco::Shingle: return t < Q(0.25) ? Ground::Snow : Ground::Sand;
    case Eco::CoralCoast: return Ground::Sand;
    case Eco::SeaCliffs: return t < Q(0.25) ? Ground::Snow : Ground::Grass;
    case Eco::Mountain: return Ground::Rock;
    case Eco::SnowField: return Ground::Snow;
    case Eco::Tundra: return Ground::Tundra;
    case Eco::Glacier:
      // blue ice fields between the snow drifts
      return n(5, "g.glacier") * 3 / 4 + n(3, "g.glacier2") / 4 > Q(0.56) ? Ground::Ice : Ground::Snow;
    case Eco::FrozenLakes: {
      // frozen lakes: rounded sheets of ice 10-30 tiles across in the snow (a pond's shore wobbles)
      const int32_t v = n(5, "g.frozen") * 3 / 4 + n(3, "g.frozen2") / 4;
      return v > Q(0.53) ? Ground::Ice : Ground::Snow;
    }
    case Eco::Taiga: return Ground::Tundra;
    case Eco::TaigaBog: {
      const int32_t v = n(3, "g.tbog");
      return v > Q(0.76) ? Ground::Water : v > Q(0.62) ? Ground::Swamp : Ground::Tundra;
    }
    case Eco::ReedMarsh: return vnoiseQ(x * 4, y * 4, 4, mix64(seed ^ tag("g.swamp"))) > Q(0.74) ? Ground::Water : Ground::Swamp;
    case Eco::PeatBog: {
      // dark pools among the hummocks
      const int32_t v = n(3, "g.peat") * 2 / 3 + n(2, "g.peat2") / 3;
      return v > Q(0.70) ? Ground::Water : Ground::Swamp;
    }
    case Eco::Mangrove: case Eco::FloodedForest: {
      // channels: the thin middles of a ridged field, broken into short reaches by a second field (they never close
      // into a ring), and a few round pools
      const int32_t a = n(4, eco == Eco::Mangrove ? "g.mchan" : "g.fchan"), gate = n(5, "g.chgate");
      const int32_t r = std::abs(2 * a - Q_ONE);
      if (r < Q(0.10) && gate > (eco == Eco::Mangrove ? Q(0.50) : Q(0.58))) return Ground::Water;
      if (n(3, "g.pool") > (eco == Eco::Mangrove ? Q(0.80) : Q(0.83))) return Ground::Water;
      return Ground::Swamp;
    }
    case Eco::LakeDistrict: {
      // small lakes and tarns 6-20 tiles across among the meadows
      const int32_t v = n(5, "g.lakes") * 2 / 3 + n(3, "g.lakes2") / 3;
      return v > Q(0.655) ? (v > Q(0.75) ? Ground::DeepWater : Ground::Water) : (fbmQ(x, y, 4, 2, mix64(seed ^ tag("g.meadow"))) > Q(0.62) ? Ground::Meadow : Ground::Grass);
    }
    case Eco::Oasis: {
      // the pond at its heart (where the oasis channel peaks), palms and grass round it
      const int32_t v = ecoChan(x, y, ECH_E) + (int32_t)((n(2, "g.oasis") - 32768) / 16);
      return v > Q(0.925) ? Ground::Water : Ground::Grass;
    }
    case Eco::AshFields: return lavaRift(x, y) ? Ground::Lava : Ground::Sand;
    case Eco::Dunes: case Eco::StonyDesert: case Eco::Badlands: case Eco::SaltFlats: case Eco::Scrubland:
    case Eco::CrystalBarrens: case Eco::PetrifiedForest: return Ground::Sand;
    case Eco::AutumnWood: return Ground::Autumn;
    case Eco::BlossomGrove: return Ground::Meadow;
    case Eco::MixedForest: case Eco::BirchWood: case Eco::GiantForest: case Eco::DarkForest: case Eco::BambooForest: case Eco::Jungle:
    case Eco::MushroomForest: case Eco::Silverwood: return Ground::ForestFloor;
    case Eco::FlowerMeadow: case Eco::AlpineMeadow: return fbmQ(x, y, 4, 2, mix64(seed ^ tag("g.meadow"))) > Q(0.40) ? Ground::Meadow : Ground::Grass;
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
  f.t = bil(a.t, b.t, c.t, d.t, fx, fy);
  f.m = bil(a.m, b.m, c.m, d.m, fx, fy);
  f.ridge = bil(a.ridge, b.ridge, c.ridge, d.ridge, fx, fy);
  int32_t rock = bil(a.rock, b.rock, c.rock, d.rock, fx, fy);
  // coast detail (water decisions only; relief reads the clean field)
  e += qm(centered(vnoiseQ(x, y, 3, mix64(seed ^ tag("t.d1"))), Q(1.0)), Q(0.010)) +
       qm(centered(vnoiseQ(x, y, 1, mix64(seed ^ tag("t.d2"))), Q(1.0)), Q(0.004));
  f.e = e;
  f.sea = e < ELEV_SEA;
  f.h = f.sea ? 0 : (uint8_t)natLevel(x, y);
  // (the noise moves the threshold by at most 0.12: below 0.38 no tile is rock, and the noise need not be drawn)
  f.rock = !f.sea && rock > Q(0.38) && rock > Q(0.5) + qm(centered(vnoiseQ(x, y, 2, mix64(seed ^ tag("t.rk"))), Q(1.0)), Q(0.12));
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
  // (M3c) the classifier's view of the tile: the jittered climate, the channels read bilinearly from the block, the
  // coarse sample whose rock it takes (nearest, nudged by the jitter so province edges wander off the 16-tile grid)
  EcoIn in;
  in.e = std::max(e, ELEV_SEA + Q(0.011)); in.t = f.t + tj; in.m = f.m + mj; in.ridge = f.ridge; in.lv = f.h; in.x = x; in.y = y;
  for (int k = 0; k < ECH_N; k++) in.ch[k] = bil(a.ch[k], b.ch[k], c.ch[k], d.ch[k], fx, fy);
  in.blk = &B;
  in.bi = i + (fx + tj * 10 >= Q(0.5) ? 1 : 0);
  in.bj = j + (fy + mj * 10 >= Q(0.5) ? 1 : 0);
  if (f.rock) f.biome = Biome::Mountain;
  else if (f.sea) f.biome = Biome::Ocean;
  else if (beach) f.biome = in.t > Q(0.62) && in.m > Q(0.60) && in.ch[ECH_D] > Q(0.40) ? Biome::Swamp : Biome::Beach;   // warm wet shores: mangroves
  else f.biome = classifyIn(in, false);
  const Biome fam = f.biome;
  if ((fam == Biome::Plains || fam == Biome::Forest || fam == Biome::Autumn || fam == Biome::Taiga) && blightIn(in)) {
    f.biome = Biome::Plains;
    f.eco = Eco::Blight;
  } else {
    f.eco = ecoIn(fam, in);
  }
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

// The relief level reads the clean field through one more warp. The bilinear coarse field alone draws the level
// contours as straight segments along the 16-tile grid (a cliff running dead straight north-south through open
// plain), and the shared t.warp (axis-aligned value noise, flat along its own lattice lines) barely bends them. This
// warp samples value noise on rotated lattices (3-4-5 and 12-5-13 rotations, wavelengths about 26 and 39 tiles) so it
// has no preferred axis. It moves the contours by up to 4 + 6 tiles; its slope stays under 1 (about 3A / wavelength
// per octave: 0.47 + 0.46), so it is a smooth deformation of the plane: it bends contours but can never make a new
// plateau or pit, and the relief keeps the ramp guarantee of the unwarped field. (M3c) Badlands mesas stand on top.
int EndlessSource::Impl::natLevel(int32_t x, int32_t y) {
  int32_t wx = x, wy = y;
  warpQ(wx, wy, 5, 5, mix64(seed ^ tag("t.warp")));
  const uint64_t s1 = mix64(seed ^ tag("t.rw1")), s2 = mix64(seed ^ tag("t.rw2"));
  const int32_t a1 = 3 * x + 4 * y, b1 = 4 * x - 3 * y, a2 = 12 * x + 5 * y, b2 = 5 * x - 12 * y;
  wx += (int32_t)(((int64_t)(vnoiseQ(a1, b1, 7, s1) - 32768) * 8) >> 16) + (int32_t)(((int64_t)(vnoiseQ(a2, b2, 9, s2) - 32768) * 12) >> 16);
  wy += (int32_t)(((int64_t)(vnoiseQ(a1, b1, 7, s1 ^ 0x77u) - 32768) * 8) >> 16) + (int32_t)(((int64_t)(vnoiseQ(a2, b2, 9, s2 ^ 0x77u) - 32768) * 12) >> 16);
  const int32_t bx = wx >> REGION_SHIFT, by = wy >> REGION_SHIFT;
  const Block& B = *block(bx, by);
  const int32_t lx = wx - bx * REGION, ly = wy - by * REGION;
  const int i = lx >> CG_SHIFT, j = ly >> CG_SHIFT;
  const int32_t fx = (lx & (CG - 1)) << (16 - CG_SHIFT), fy = (ly & (CG - 1)) << (16 - CG_SHIFT);
  int32_t ec = bil(B.clean(i, j), B.clean(i + 1, j), B.clean(i, j + 1), B.clean(i + 1, j + 1), fx, fy);
  const int lv = levelOf(ec);
  // (mesas only on dry land well above the sea, and never above level 4)
  if (ec < ELEV_SEA + Q(0.03) || lv > 4) return lv;
  return std::min(5, lv + mesaLift(x, y, B, i, j, fx, fy));
}

// (M2) the clean elevation natLevel reads (the same warps), for peaks: a crest is where this has a local maximum
int32_t EndlessSource::Impl::reliefE(int32_t x, int32_t y) {
  int32_t wx = x, wy = y;
  warpQ(wx, wy, 5, 5, mix64(seed ^ tag("t.warp")));
  const uint64_t s1 = mix64(seed ^ tag("t.rw1")), s2 = mix64(seed ^ tag("t.rw2"));
  const int32_t a1 = 3 * x + 4 * y, b1 = 4 * x - 3 * y, a2 = 12 * x + 5 * y, b2 = 5 * x - 12 * y;
  wx += (int32_t)(((int64_t)(vnoiseQ(a1, b1, 7, s1) - 32768) * 8) >> 16) + (int32_t)(((int64_t)(vnoiseQ(a2, b2, 9, s2) - 32768) * 12) >> 16);
  wy += (int32_t)(((int64_t)(vnoiseQ(a1, b1, 7, s1 ^ 0x77u) - 32768) * 8) >> 16) + (int32_t)(((int64_t)(vnoiseQ(a2, b2, 9, s2 ^ 0x77u) - 32768) * 12) >> 16);
  const int32_t bx = wx >> REGION_SHIFT, by = wy >> REGION_SHIFT;
  const Block& B = *block(bx, by);
  const int32_t lx = wx - bx * REGION, ly = wy - by * REGION;
  const int i = lx >> CG_SHIFT, j = ly >> CG_SHIFT;
  const int32_t fx = (lx & (CG - 1)) << (16 - CG_SHIFT), fy = (ly & (CG - 1)) << (16 - CG_SHIFT);
  return bil(B.clean(i, j), B.clean(i + 1, j), B.clean(i, j + 1), B.clean(i + 1, j + 1), fx, fy);
}

// (M2) the biome tile() gives a land tile, without its beach band and rock cores (the ecotone ring round a chunk)
Biome EndlessSource::Impl::biomeLite(int32_t x, int32_t y, Eco* ecoOut) {
  int32_t wx = x, wy = y;
  warpQ(wx, wy, 5, 5, mix64(seed ^ tag("t.warp")));
  const int32_t bx = wx >> REGION_SHIFT, by = wy >> REGION_SHIFT;
  const Block& B = *block(bx, by);
  const int32_t lx = wx - bx * REGION, ly = wy - by * REGION;
  const int i = lx >> CG_SHIFT, j = ly >> CG_SHIFT;
  const int32_t fx = (lx & (CG - 1)) << (16 - CG_SHIFT), fy = (ly & (CG - 1)) << (16 - CG_SHIFT);
  const Coarse &a = B.at(i, j), &b = B.at(i + 1, j), &c = B.at(i, j + 1), &d = B.at(i + 1, j + 1);
  int32_t e = bil(a.e, b.e, c.e, d.e, fx, fy);
  e += qm(centered(vnoiseQ(x, y, 3, mix64(seed ^ tag("t.d1"))), Q(1.0)), Q(0.010)) +
       qm(centered(vnoiseQ(x, y, 1, mix64(seed ^ tag("t.d2"))), Q(1.0)), Q(0.004));
  if (e < ELEV_SEA) { if (ecoOut) *ecoOut = Eco::Ocean; return Biome::Ocean; }
  const int32_t t = bil(a.t, b.t, c.t, d.t, fx, fy), m = bil(a.m, b.m, c.m, d.m, fx, fy);
  const int32_t tj = qm(centered(vnoiseQ(x, y, 5, mix64(seed ^ tag("t.tj"))), Q(1.0)), Q(0.03)) +
                     qm(centered(vnoiseQ(x, y, 3, mix64(seed ^ tag("t.tj2"))), Q(1.0)), Q(0.01));
  const int32_t mj = qm(centered(vnoiseQ(x, y, 5, mix64(seed ^ tag("t.mj"))), Q(1.0)), Q(0.035)) +
                     qm(centered(vnoiseQ(x, y, 3, mix64(seed ^ tag("t.mj2"))), Q(1.0)), Q(0.012));
  EcoIn in;
  in.e = std::max(e, ELEV_SEA + Q(0.011)); in.t = t + tj; in.m = m + mj; in.x = x; in.y = y;
  in.ridge = bil(a.ridge, b.ridge, c.ridge, d.ridge, fx, fy);
  // (the relief level only where a classifier test can read it: always, as the families' eco tests use it)
  in.lv = natLevel(x, y);
  for (int k = 0; k < ECH_N; k++) in.ch[k] = bil(a.ch[k], b.ch[k], c.ch[k], d.ch[k], fx, fy);
  in.blk = &B;
  in.bi = i + (fx + tj * 10 >= Q(0.5) ? 1 : 0);
  in.bj = j + (fy + mj * 10 >= Q(0.5) ? 1 : 0);
  Biome fam = classifyIn(in, false);
  Eco eco;
  if ((fam == Biome::Plains || fam == Biome::Forest || fam == Biome::Autumn || fam == Biome::Taiga) && blightIn(in)) { fam = Biome::Plains; eco = Eco::Blight; }
  else eco = ecoIn(fam, in);
  if (ecoOut) *ecoOut = eco;
  return fam;
}


MacroSample EndlessSource::Impl::macro(int32_t x, int32_t y) {
  TileF f = tile(x, y);
  MacroSample m;
  m.elev = f.e;
  m.temp = f.t;
  m.moist = f.m;
  m.biome = f.biome;
  m.eco = f.eco;
  m.height = f.h;
  m.water = f.sea || waterAt(x, y);
  m.kingdom = kingdomAt(x, y);
  return m;
}

}  // namespace ew
