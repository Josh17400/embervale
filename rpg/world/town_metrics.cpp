// M7c the planner's checks (rpg/world/town_plan.h): the structure score, the plan signature and similarity, the village
// character. Read-only on a built SettlementOut (its rank layer, buildings, walls and ground); no generator internals,
// so the planners cannot tune what they are measured by. SURVEY lane (phase B).
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include "rpg/world/biomes.h"
#include "rpg/world/dmath.h"
#include "rpg/world/settlement.h"
#include "rpg/world/town_plan.h"
#include "rpg/world/town_rules.h"

namespace ew {

const char* rankName(Rank r) {
  static const char* n[] = {"none", "main", "street", "lane", "footpath", "square"};
  return (int)r < (int)Rank::COUNT ? n[(int)r] : "?";
}
const char* blockRoleName(BlockRole r) {
  static const char* n[] = {"none", "homes", "seat", "temple", "market", "park", "orchard", "kitchen-garden", "craft-yard",
                            "compound", "barracks", "harbour", "faubourg", "farm"};
  return (int)r < (int)BlockRole::COUNT ? n[(int)r] : "?";
}

namespace {

struct Grid {
  int w = 0, h = 0;
  const SettlementOut* o = nullptr;
  bool in(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
  size_t I(int x, int y) const { return (size_t)y * w + x; }
  Rank rank(int x, int y) const { return in(x, y) && !o->rank.empty() ? (Rank)o->rank[I(x, y)] : Rank::None; }
  bool street(int x, int y) const { return rankStreet(rank(x, y)); }
  bool way(int x, int y) const { return rankWay(rank(x, y)); }
  bool wall(int x, int y) const { return in(x, y) && !o->buf.wall.empty() && o->buf.wall[I(x, y)] != 0; }
  bool wet(int x, int y) const { return in(x, y) && groundWater(o->buf.at(x, y)); }
  int bldg(int x, int y) const { return in(x, y) ? o->buf.bldgAt[I(x, y)] : -1; }
};

// the core the score is taken over: the wall ring's box (walled), else the buildings' box, a little margin round it
IRect coreOf(const Grid& g) {
  int x0 = g.w, y0 = g.h, x1 = -1, y1 = -1;
  for (int y = 0; y < g.h; y++)
    for (int x = 0; x < g.w; x++)
      if (g.wall(x, y)) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
  if (x1 < 0) {
    for (const Bldg& b : g.o->buf.bldgs) {
      x0 = std::min(x0, b.r.x); y0 = std::min(y0, b.r.y);
      x1 = std::max(x1, b.r.x + b.r.w - 1); y1 = std::max(y1, b.r.y + b.r.h - 1);
    }
    if (x1 < 0) return IRect{0, 0, g.w, g.h};
    x0 -= 2; y0 -= 2; x1 += 2; y1 += 3;
  }
  x0 = std::max(0, x0); y0 = std::max(0, y0); x1 = std::min(g.w - 1, x1); y1 = std::min(g.h - 1, y1);
  return IRect{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
}

// per street tile: the longest straight run of street tiles through it (horizontal, vertical or either diagonal), and
// the vertical run alone (north-south streets, for side frontage)
void runs(const Grid& g, std::vector<uint8_t>& best, std::vector<uint8_t>& vert) {
  best.assign((size_t)g.w * g.h, 0);
  vert.assign((size_t)g.w * g.h, 0);
  static const int DX[4] = {1, 0, 1, 1}, DY[4] = {0, 1, 1, -1};
  for (int d = 0; d < 4; d++) {
    for (int y = 0; y < g.h; y++)
      for (int x = 0; x < g.w; x++) {
        if (!g.street(x, y) || g.street(x - DX[d], y - DY[d])) continue;   // the start of a run
        int n = 0;
        while (g.street(x + DX[d] * n, y + DY[d] * n)) n++;
        const uint8_t v = (uint8_t)std::min(n, 255);
        for (int k = 0; k < n; k++) {
          const size_t i = g.I(x + DX[d] * k, y + DY[d] * k);
          best[i] = std::max(best[i], v);
          if (d == 1) vert[i] = v;
        }
      }
  }
}

// (SURVEY hardening) the street NETWORK: the way tiles (streets and squares) in 4-connected pieces of NET_MIN tiles or
// more (a piece touching the buffer's edge always: a road leaving the place). Loose scraps of street and 1-tile
// slivers laid along house fronts without joining a street are no street a building fronts and no straight street.
// (Pruning every dead-end spur as well was tried and dropped: it cost the M7b baseline 7 to 8 points per type, the
// legitimate cul-de-sac lanes with them, while a sliver that joins the streets is a lane the review sees.)
constexpr int NET_MIN = 40;
void networkOf(const Grid& g, std::vector<uint8_t>& net) {
  const size_t n = (size_t)g.w * g.h;
  net.assign(n, 0);
  for (int y = 0; y < g.h; y++)
    for (int x = 0; x < g.w; x++) if (g.way(x, y)) net[g.I(x, y)] = 1;
  // pieces under NET_MIN tiles go (loose scraps, a sliver's little loop)
  std::vector<int> lab(n, -1), st, members;
  for (size_t s0 = 0; s0 < n; s0++) {
    if (!net[s0] || lab[s0] >= 0) continue;
    members.clear();
    st.assign(1, (int)s0);
    lab[s0] = 1;
    while (!st.empty()) {
      const int i = st.back();
      st.pop_back();
      members.push_back(i);
      const int x = i % g.w, y = i / g.w;
      static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; k++) {
        const int qx = x + dx[k], qy = y + dy[k];
        if (!g.in(qx, qy)) continue;
        const size_t j = g.I(qx, qy);
        if (net[j] && lab[j] < 0) { lab[j] = 1; st.push_back((int)j); }
      }
    }
    bool edge = false;
    for (int i : members) {
      const int x = i % g.w, y = i / g.w;
      if (x == 0 || y == 0 || x == g.w - 1 || y == g.h - 1) { edge = true; break; }
    }
    if ((int)members.size() < NET_MIN && !edge) for (int i : members) net[(size_t)i] = 0;
  }
}
// (SURVEY hardening) a building counts for frontage only with a real footprint (2 x 2 tiles or more): a 1-tile sliver
// building strung along a street is no house fronting it
inline bool frontageBldg(const Bldg& b) { return b.r.w >= 2 && b.r.h >= 2; }

struct Blocks {
  int enclosed = 0;          // blocks of 16 tiles or more cut off by streets, walls and water
  float rectSum = 0, areaSum = 0;
  int bldgTilesIn = 0, bldgTiles = 0;
};

Blocks blocksOf(const Grid& g, const IRect& core) {
  Blocks B;
  std::vector<int> lab((size_t)g.w * g.h, -1);
  std::vector<int> stack;
  for (const Bldg& b : g.o->buf.bldgs) B.bldgTiles += b.r.w * b.r.h;
  auto land = [&](int x, int y) { return g.in(x, y) && !g.way(x, y) && !g.wall(x, y) && !g.wet(x, y); };
  int next = 0;
  for (int y = core.y; y < core.y + core.h; y++)
    for (int x = core.x; x < core.x + core.w; x++) {
      if (!land(x, y) || lab[g.I(x, y)] >= 0) continue;
      // flood the component (over the whole buffer: a piece reaching the buffer's edge is the open country)
      int area = 0, bx0 = x, by0 = y, bx1 = x, by1 = y, u0 = x + y, u1 = x + y, v0 = x - y, v1 = x - y, btiles = 0, rim = 0, rimWay = 0;
      bool edge = false;
      stack.clear();
      stack.push_back((int)g.I(x, y));
      lab[g.I(x, y)] = next;
      while (!stack.empty()) {
        const int i = stack.back();
        stack.pop_back();
        const int px = i % g.w, py = i / g.w;
        area++;
        if (g.bldg(px, py) >= 0) btiles++;
        bx0 = std::min(bx0, px); bx1 = std::max(bx1, px); by0 = std::min(by0, py); by1 = std::max(by1, py);
        u0 = std::min(u0, px + py); u1 = std::max(u1, px + py); v0 = std::min(v0, px - py); v1 = std::max(v1, px - py);
        if (px == 0 || py == 0 || px == g.w - 1 || py == g.h - 1) edge = true;
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
          const int qx = px + dx[k], qy = py + dy[k];
          if (!land(qx, qy)) { rim++; if (g.way(qx, qy)) rimWay++; continue; }
          if (lab[g.I(qx, qy)] >= 0) continue;
          lab[g.I(qx, qy)] = next;
          stack.push_back((int)g.I(qx, qy));
        }
      }
      next++;
      if (edge || area < 16 || area > 700 || rimWay * 2 < rim) continue;   // (open country, a sliver, a field, a pond's shore)
      const float axis = (float)((bx1 - bx0 + 1) * (by1 - by0 + 1));
      const float diag = (float)((u1 - u0 + 1) * (v1 - v0 + 1)) * 0.5f;
      const float rect = std::min(1.0f, std::max(area / axis, area / std::max(1.0f, diag)));
      B.enclosed++;
      B.rectSum += rect * area;
      B.areaSum += (float)area;
      B.bldgTilesIn += btiles;
    }
  return B;
}

float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

}  // namespace

StructureScore structureScore(const SettlementOut& o) {
  StructureScore S;
  Grid g;
  g.o = &o;
  g.w = o.buf.w;
  g.h = o.buf.h;
  if (o.rank.size() != (size_t)g.w * g.h || g.w <= 0) return S;
  const IRect core = coreOf(g);
  std::vector<uint8_t> best, vert, net;
  runs(g, best, vert);
  networkOf(g, net);
  auto straight = [&](int x, int y) { return g.in(x, y) && g.street(x, y) && net[g.I(x, y)] && best[g.I(x, y)] >= 9; };
  auto straightNS = [&](int x, int y) { return g.in(x, y) && g.street(x, y) && net[g.I(x, y)] && vert[g.I(x, y)] >= 9; };
  // a neighbour on the same front row a tile or less away (a continuous frontage)
  const std::vector<Bldg>& bs = o.buf.bldgs;
  auto joined = [&](size_t i) {
    const Bldg& b = bs[i];
    for (size_t j = 0; j < bs.size(); j++) {
      if (j == i || !frontageBldg(bs[j])) continue;
      const Bldg& c = bs[j];
      if (std::abs((c.r.y + c.r.h) - (b.r.y + b.r.h)) > 0) continue;
      const int gap = c.r.x >= b.r.x + b.r.w ? c.r.x - (b.r.x + b.r.w) : b.r.x - (c.r.x + c.r.w);
      if (gap >= 0 && gap <= 1) return true;
    }
    return false;
  };
  // frontage (a walled place: the buildings inside the ring's box only; the faubourgs outside its gates ribbon along
  // the approach roads organically by design, 15.M7c "outside the walls", and are not the planned core)
  bool walledCore = false;
  for (size_t i = 0; i < o.buf.wall.size() && !walledCore; i++) walledCore = o.buf.wall[i] != 0;
  float front = 0;
  for (size_t bi = 0; bi < bs.size(); bi++) {
    const Bldg& b = bs[bi];
    if (!frontageBldg(b)) continue;
    if (walledCore && (b.r.cx() < core.x || b.r.cx() >= core.x + core.w || b.r.y + b.r.h - 1 < core.y || b.r.y >= core.y + core.h)) continue;
    S.bldgs++;
    float f = 0;
    for (int y = b.r.y + b.r.h; y <= b.r.y + b.r.h + 1 && f < 1; y++)
      for (int x = b.r.x; x < b.r.x + b.r.w; x++) {
        if (straight(x, y)) { f = 1; break; }
        if (g.rank(x, y) == Rank::Square) f = std::max(f, 0.5f);
      }
    if (f < 1) {   // a side on a north-south street (at least half the rows, a tile or two away)
      int west = 0, east = 0;
      for (int y = b.r.y; y < b.r.y + b.r.h; y++) {
        if (straightNS(b.r.x - 1, y) || straightNS(b.r.x - 2, y)) west++;
        if (straightNS(b.r.x + b.r.w, y) || straightNS(b.r.x + b.r.w + 1, y)) east++;
      }
      if (std::max(west, east) * 2 >= b.r.h) f = 1;
    }
    if (f >= 1) S.fronting++;
    if (f > 0 && !joined(bi)) f *= 0.5f;   // (standing alone: half)
    front += f;
  }
  S.frontage = S.bldgs ? front / S.bldgs : 0;
  // blocks
  const Blocks B = blocksOf(g, core);
  S.blocks = B.enclosed;
  const float rectMean = B.areaSum > 0 ? B.rectSum / B.areaSum : 0;
  const float inShare = B.bldgTiles > 0 ? (float)B.bldgTilesIn / B.bldgTiles : 0;
  S.regularity = rectMean * inShare;
  // the street tiles of the core, the coarse graph's cells (4 x 4 tiles, a street cell holds 3 or more street tiles)
  // (a main road's tile counts a quarter: the avenues are straight in any city, the streets off them tell the plan)
  int streetTiles = 0, landTiles = 0;
  float wAll = 0, wStraight = 0;
  for (int y = core.y; y < core.y + core.h; y++)
    for (int x = core.x; x < core.x + core.w; x++) {
      if (g.street(x, y)) {
        streetTiles++;
        const float w = g.rank(x, y) == Rank::Main ? 0.25f : 1.0f;
        wAll += w;
        if (best[g.I(x, y)] >= 9 && net[g.I(x, y)]) wStraight += w;   // (a straight run of the network)
      } else if (!g.way(x, y) && !g.wet(x, y) && !g.wall(x, y)) landTiles++;
    }
  S.streetTiles = streetTiles;
  S.straightness = wAll > 0 ? wStraight / wAll : 0;
  const int CW = (core.w + 3) / 4, CH = (core.h + 3) / 4;
  std::vector<uint8_t> cell((size_t)CW * CH, 0);
  for (int j = 0; j < CH; j++)
    for (int i = 0; i < CW; i++) {
      int n = 0;
      for (int y = core.y + j * 4; y < std::min(core.y + core.h, core.y + j * 4 + 4); y++)
        for (int x = core.x + i * 4; x < std::min(core.x + core.w, core.x + i * 4 + 4); x++) if (g.way(x, y)) n++;
      cell[(size_t)j * CW + i] = n >= 3 ? 1 : 0;
    }
  auto C = [&](int i, int j) { return i >= 0 && j >= 0 && i < CW && j < CH && cell[(size_t)j * CW + i]; };
  static const int NX[8] = {1, 1, 0, -1, -1, -1, 0, 1}, NY[8] = {0, 1, 1, 1, 0, -1, -1, -1};   // round the cell, in order
  for (int j = 1; j < CH - 1; j++)
    for (int i = 1; i < CW - 1; i++) {
      if (!C(i, j)) continue;
      S.streetCells++;
      int n = 0, arcs = 0;
      for (int k = 0; k < 8; k++) {
        if (C(i + NX[k], j + NY[k])) n++;
        if (C(i + NX[k], j + NY[k]) && !C(i + NX[(k + 7) % 8], j + NY[(k + 7) % 8])) arcs++;
      }
      // a dead end: one neighbour, or its neighbours all in one short arc (two or three cells side by side)
      if (n <= 1 || (arcs == 1 && n <= 3)) S.ends++;
    }
  S.loops = B.enclosed;
  S.deadEnds = 1.0f - (float)S.ends / (float)std::max(1, S.blocks + S.ends);
  S.connectivity = clamp01((float)S.blocks * 260.0f / (float)std::max(1, landTiles));
  if (!S.blocks && !S.ends) S.deadEnds = 0;   // (no streets at all)
  S.total = 30.0f * S.frontage + 20.0f * S.straightness + 20.0f * S.regularity + 10.0f * S.deadEnds + 20.0f * S.connectivity;
  return S;
}

// ---------------------------------------------------------------- the plan signature
PlanSig planSignature(const SettlementOut& o) {
  PlanSig P;
  Grid g;
  g.o = &o;
  g.w = o.buf.w;
  g.h = o.buf.h;
  if (o.rank.size() != (size_t)g.w * g.h || g.w <= 0) return P;
  const IRect c = coreOf(g);
  P.valid = true;
  P.aspect = (float)c.w / (float)std::max(1, c.h);
  std::vector<float> n((size_t)SIG_N * SIG_N, 0);
  for (int y = c.y; y < c.y + c.h; y++)
    for (int x = c.x; x < c.x + c.w; x++) {
      // (SURVEY hardening) a tile's cell by its centre ((2t + 1) N / 2w): the cells fall the same way under a mirror
      // (by its corner, a mirrored city resampled up to a cell column apart and scored ~0.87 against itself)
      const int i = std::min(SIG_N - 1, (2 * (x - c.x) + 1) * SIG_N / (2 * c.w)), j = std::min(SIG_N - 1, (2 * (y - c.y) + 1) * SIG_N / (2 * c.h));
      const size_t k = (size_t)j * SIG_N + i;
      n[k] += 1;
      if (g.way(x, y)) P.street[k] += 1;
      else if (g.bldg(x, y) >= 0) P.bldg[k] += 1;
      else if (!g.wet(x, y) && !g.wall(x, y)) P.open[k] += 1;
    }
  for (size_t k = 0; k < n.size(); k++)
    if (n[k] > 0) { P.street[k] /= n[k]; P.bldg[k] /= n[k]; P.open[k] /= n[k]; }
  // the wall's radial profile round its centroid (the buildings' when unwalled)
  double sx = 0, sy = 0;
  int cnt = 0;
  for (int y = c.y; y < c.y + c.h; y++)
    for (int x = c.x; x < c.x + c.w; x++) if (g.wall(x, y)) { sx += x; sy += y; cnt++; }
  if (!cnt) { sx = c.x + c.w * 0.5; sy = c.y + c.h * 0.5; cnt = 1; } else { sx /= cnt; sy /= cnt; }
  const bool walled = cnt > 1;
  for (int y = c.y; y < c.y + c.h; y++)
    for (int x = c.x; x < c.x + c.w; x++) {
      if (walled ? !g.wall(x, y) : g.bldg(x, y) < 0) continue;
      const float dx = (float)(x - sx), dy = (float)(y - sy), r = std::sqrt(dx * dx + dy * dy);
      const float a = datan2(dy, dx);
      int bin = (int)((a + D_PI) / D_TAU * SIG_RAYS);
      bin = std::clamp(bin, 0, SIG_RAYS - 1);
      P.ray[bin] = std::max(P.ray[bin], r);
    }
  float mean = 0;
  for (float r : P.ray) mean += r;
  mean /= SIG_RAYS;
  if (mean > 0) for (float& r : P.ray) r /= mean;
  return P;
}

// (SURVEY hardening) the street and building overlaps are also tried with b's cells moved up to SIG_SHIFT cells either
// way: the same grid laid half a block further on is the same plan (offsetting a grid scored 0.53 against itself)
constexpr int SIG_SHIFT = 2;
float planSimilarity(const PlanSig& a, const PlanSig& b) {
  if (!a.valid || !b.valid) return 0;
  float bestSim = 0;
  // the cells as bit rows (bit i of row j: cell (i, j) mostly street / mostly building)
  static_assert(SIG_N == 32, "a signature row is one 32-bit word");
  uint32_t sa[SIG_N] = {}, ba[SIG_N] = {};
  for (int j = 0; j < SIG_N; j++)
    for (int i = 0; i < SIG_N; i++) {
      if (a.street[j * SIG_N + i] >= 0.35f) sa[j] |= 1u << i;
      if (a.bldg[j * SIG_N + i] >= 0.35f) ba[j] |= 1u << i;
    }
  for (int t = 0; t < 8; t++) {
    const bool swap = (t & 4) != 0, fx = (t & 1) != 0, fy = (t & 2) != 0;
    // b's cells under the transform
    uint32_t sb[SIG_N] = {}, bb[SIG_N] = {};
    for (int j = 0; j < SIG_N; j++)
      for (int i = 0; i < SIG_N; i++) {
        int bi = fx ? SIG_N - 1 - i : i, bj = fy ? SIG_N - 1 - j : j;
        if (swap) std::swap(bi, bj);
        const size_t kb = (size_t)bj * SIG_N + bi;
        if (b.street[kb] >= 0.35f) sb[j] |= 1u << i;
        if (b.bldg[kb] >= 0.35f) bb[j] |= 1u << i;
      }
    float simS = 0, simB = 0;
    for (int oy = -SIG_SHIFT; oy <= SIG_SHIFT; oy++)
      for (int ox = -SIG_SHIFT; ox <= SIG_SHIFT; ox++) {
        int iS = 0, uS = 0, iB = 0, uB = 0;
        for (int j = 0; j < SIG_N; j++) {
          // b's row j + oy moved so that its cell i + ox lands on i
          const int qj = j + oy;
          uint32_t rs = qj >= 0 && qj < SIG_N ? sb[qj] : 0u, rb = qj >= 0 && qj < SIG_N ? bb[qj] : 0u;
          if (ox > 0) { rs >>= ox; rb >>= ox; } else if (ox < 0) { rs <<= -ox; rb <<= -ox; }
          iS += std::popcount(sa[j] & rs); uS += std::popcount(sa[j] | rs);
          iB += std::popcount(ba[j] & rb); uB += std::popcount(ba[j] | rb);
        }
        const float s0 = uS ? (float)iS / uS : 1.0f, b0 = uB ? (float)iB / uB : 1.0f;
        // (one shift for both: the streets and the buildings of one plan move together)
        if (0.5f * s0 + 0.2f * b0 > 0.5f * simS + 0.2f * simB) { simS = s0; simB = b0; }
      }
    // the ray profile under the same transform: a ray at angle a goes to (fx: pi - a), (fy: -a), (swap: pi/2 - a)
    float dr = 0;
    for (int k = 0; k < SIG_RAYS; k++) {
      const float ang = ((k + 0.5f) / SIG_RAYS) * D_TAU - D_PI;
      float x = dcos(ang), y = dsin(ang);
      if (fx) x = -x;
      if (fy) y = -y;
      if (swap) std::swap(x, y);
      int kb = (int)((datan2(y, x) + D_PI) / D_TAU * SIG_RAYS);
      kb = std::clamp(kb, 0, SIG_RAYS - 1);
      dr += std::fabs(a.ray[k] - b.ray[kb]);
    }
    const float simR = clamp01(1.0f - (dr / SIG_RAYS) / 0.5f);
    const float aspB = swap ? 1.0f / std::max(0.01f, b.aspect) : b.aspect;
    const float simA = std::min(a.aspect, aspB) / std::max(0.01f, std::max(a.aspect, aspB));
    bestSim = std::max(bestSim, 0.5f * simS + 0.2f * simB + 0.2f * simR + 0.1f * simA);
  }
  return bestSim;
}

// ---------------------------------------------------------------- the village character
VillageCharacter villageCharacter(const SettlementOut& o) {
  VillageCharacter V;
  Grid g;
  g.o = &o;
  g.w = o.buf.w;
  g.h = o.buf.h;
  if (o.rank.size() != (size_t)g.w * g.h || g.w <= 0) { V.why = "no rank layer"; return V; }
  const StructureScore S = structureScore(o);
  V.structure = S.total;
  V.blocks = S.blocks;
  for (uint8_t u : o.buf.urb) if (u) V.cityFabric++;
  for (const Bldg& b : o.buf.bldgs) if (b.row) V.cityFabric++;
  // the main-road arms through the heart: runs of main-road tiles crossing a ring round it (radius 9)
  const int hx = o.ex - o.gx, hy = o.ey - o.gy;
  {
    const int R = 9, NS = 96;
    int arms = 0;
    bool prev = false, first = false;
    for (int k = 0; k <= NS; k++) {
      const float a = (float)(k % NS) / NS * D_TAU;
      const int x = hx + (int)std::floor(dcos(a) * R + 0.5f), y = hy + (int)std::floor(dsin(a) * R + 0.5f);
      const bool m = g.rank(x, y) == Rank::Main;
      if (k == 0) first = m;
      else if (m && !prev && !(k == NS && first)) arms++;
      prev = m;
    }
    if (first && arms == 0) arms = 1;
    V.spines = (arms + 1) / 2;
  }
  // the homes' setbacks from their street and how many stand in a row with a neighbour
  std::vector<float> sb;
  int aligned = 0;
  const std::vector<Bldg>& bs = o.buf.bldgs;
  for (size_t i = 0; i < bs.size(); i++) {
    const Bldg& b = bs[i];
    if (!townIsHome(b.type)) continue;
    V.homes++;
    int d = -1;
    for (int k = 0; k <= 8 && d < 0; k++)
      for (int x = b.r.x - k; x < b.r.x + b.r.w + k && d < 0; x++)
        for (int y = b.r.y + b.r.h - 1 - k; y <= b.r.y + b.r.h + k && d < 0; y++)
          if ((x < b.r.x || x >= b.r.x + b.r.w || y >= b.r.y + b.r.h) && g.way(x, y)) d = k;
    if (d >= 0) sb.push_back((float)d);
    for (size_t j = 0; j < bs.size(); j++) {
      if (j == i || !townIsHome(bs[j].type)) continue;
      const Bldg& c = bs[j];
      if (c.r.y + c.r.h != b.r.y + b.r.h) continue;
      const int gap = c.r.x >= b.r.x + b.r.w ? c.r.x - (b.r.x + b.r.w) : b.r.x - (c.r.x + c.r.w);
      if (gap >= 0 && gap <= 1) { aligned++; break; }
    }
  }
  if (!sb.empty()) {
    float m = 0;
    for (float v : sb) m += v;
    m /= (float)sb.size();
    float var = 0;
    for (float v : sb) var += (v - m) * (v - m);
    V.setbackVar = std::sqrt(var / (float)sb.size());
  }
  V.aligned = V.homes ? (float)aligned / V.homes : 0;
  V.ok = true;
  if (V.cityFabric) { V.ok = false; V.why = "city fabric (urb or row houses) in a village"; }
  else if (V.structure > STRUCT_VILLAGE_MAX) { V.ok = false; V.why = "structure score above a village's"; }
  else if (V.blocks > VILLAGE_BLOCKS) { V.ok = false; V.why = "street blocks (a village is no grid)"; }
  else if (V.spines > VILLAGE_SPINES_MAX) { V.ok = false; V.why = "too many main roads through the heart"; }
  else if (V.homes >= 6 && V.setbackVar < VILLAGE_SETBACK_VAR) { V.ok = false; V.why = "even setbacks (houses in a line)"; }
  else if (V.aligned > VILLAGE_ALIGNED_MAX) { V.ok = false; V.why = "houses standing in rows"; }
  return V;
}

}  // namespace ew
