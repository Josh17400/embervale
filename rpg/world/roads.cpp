// L1c roads (VISION_PLAN 2.5): a Gabriel graph over the settlements, each edge routed by A* on a 16-tile cost grid inside
// a corridor around the straight line, then smoothed (Chaikin) into winding roads. WORLD lane.
//
// The Gabriel test for an edge AB looks only at nodes inside the circle on AB, and edges are capped at 600 tiles, so the
// graph is computable from a bounded neighbourhood anywhere (no global pass). Costs come only from pure fields
// (elevation, relief levels, sea, lakes, rivers, rock), so every region computes the same road. Edges are cached by
// their endpoint ids. Gabriel graphs contain the Euclidean MST: settlements on one landmass stay connected unless the
// sea or a lake blocks the corridor.
#include <algorithm>
#include <cstdint>
#include <queue>
#include <utility>
#include "rpg/world/gen.h"

namespace ew {
using namespace gen;

namespace {
constexpr int32_t LMAX = 600;        // longest graph edge (tiles)
constexpr int32_t RG = 16;           // A* grid (tiles)
}  // namespace

namespace gen {
void chaikin(std::vector<GTile>& pts, int passes) {
  for (int p = 0; p < passes && pts.size() >= 3; p++) {
    std::vector<GTile> q;
    q.reserve(pts.size() * 2);
    q.push_back(pts.front());
    for (size_t n = 0; n + 1 < pts.size(); n++) {
      const GTile &a = pts[n], &b = pts[n + 1];
      q.push_back(GTile{floorDiv(3 * a.x + b.x, 4), floorDiv(3 * a.y + b.y, 4)});
      q.push_back(GTile{floorDiv(a.x + 3 * b.x, 4), floorDiv(a.y + 3 * b.y, 4)});
    }
    q.push_back(pts.back());
    pts.swap(q);
  }
  // drop repeats
  std::vector<GTile> u;
  for (const GTile& g : pts) if (u.empty() || u.back().x != g.x || u.back().y != g.y) u.push_back(g);
  pts.swap(u);
}
}  // namespace gen

bool EndlessSource::Impl::roadPath(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t corridor, std::vector<GTile>& out) {
  const int32_t gx0 = floorDiv(std::min(ax, bx) - corridor, RG), gx1 = floorDiv(std::max(ax, bx) + corridor, RG);
  const int32_t gy0 = floorDiv(std::min(ay, by) - corridor, RG), gy1 = floorDiv(std::max(ay, by) + corridor, RG);
  const int W = gx1 - gx0 + 1, H = gy1 - gy0 + 1;
  if (W <= 0 || H <= 0 || (int64_t)W * H > 400000) return false;
  const int si = (floorDiv(ax, RG) - gx0) + (floorDiv(ay, RG) - gy0) * W;
  const int gi = (floorDiv(bx, RG) - gx0) + (floorDiv(by, RG) - gy0) * W;
  // per-cell cost (lazy): -1 impassable; level for the climb cost
  std::vector<int16_t> cost((size_t)W * H, -2);
  std::vector<int8_t> lev((size_t)W * H, 0);
  const int64_t vx = bx - ax, vy = by - ay, L2 = std::max<int64_t>(1, vx * vx + vy * vy);
  auto cellCost = [&](int idx) -> int {
    if (cost[(size_t)idx] != -2) return cost[(size_t)idx];
    int32_t cx = gx0 + idx % W, cy = gy0 + idx / W;
    int32_t x = cx * RG + RG / 2, y = cy * RG + RG / 2;
    int c = 10;
    // inside the corridor around the straight line
    int64_t wx = x - ax, wy = y - ay, t = wx * vx + wy * vy, d2;
    if (t <= 0) d2 = wx * wx + wy * wy;
    else if (t >= L2) d2 = dist2(x, y, bx, by);
    else { int64_t cr = wx * vy - wy * vx; d2 = cr * cr / L2; }
    if (d2 > (int64_t)corridor * corridor && idx != si && idx != gi) c = -1;
    if (c > 0) {
      int32_t ridge = 0;
      int32_t e = elevation(x, y, nullptr, &ridge);
      lev[(size_t)idx] = (int8_t)levelOf(e);
      if (idx != si && idx != gi) {
        int rw = riverWidthCell(x, y);
        if (e < ELEV_SEA + Q(0.008) || rw >= 8) c = -1;   // sea or lake
        else {
          if (rw) c += 40 + 18 * rw;                     // a bridge or a ford
          if (e > Q(0.86) && ridge > Q(0.5)) c += 60;     // cutting through rock
          else if (ridge > Q(0.4)) c += 8;
          // rough ground (a slow noise field) the road prefers to skirt: gentle meanders instead of ruler lines
          c += (int)((int64_t)vnoiseQ(x, y, 7, mix64(seed ^ tag("r.rough"))) * 14 >> 16);
        }
      }
    }
    cost[(size_t)idx] = (int16_t)c;
    return c;
  };
  if (cellCost(si) < 0 || cellCost(gi) < 0) return false;
  std::vector<int32_t> g((size_t)W * H, INT32_MAX);
  std::vector<int32_t> from((size_t)W * H, -1);
  using QE = std::pair<int64_t, int32_t>;   // (f, cell): a total order, so the search is deterministic
  std::priority_queue<QE, std::vector<QE>, std::greater<QE>> open;
  const int gxg = gi % W, gyg = gi / W;
  auto heur = [&](int idx) {
    int dx = std::abs(idx % W - gxg), dy = std::abs(idx / W - gyg);
    return (int64_t)(10 * std::max(dx, dy) + 4 * std::min(dx, dy));
  };
  g[(size_t)si] = 0;
  open.push({heur(si), si});
  static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
  bool found = false;
  int expanded = 0;
  while (!open.empty()) {
    QE top = open.top();
    open.pop();
    const int cur = top.second;
    if (top.first - heur(cur) > g[(size_t)cur]) continue;   // stale
    if (cur == gi) { found = true; break; }
    if (++expanded > 60000) break;
    const int cx = cur % W, cy = cur / W;
    for (int k = 0; k < 8; k++) {
      int nx = cx + DX[k], ny = cy + DY[k];
      if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
      int ni = ny * W + nx;
      int c = cellCost(ni);
      if (c < 0) continue;
      int step = k < 4 ? c : c * 14 / 10;
      step += 22 * std::abs(lev[(size_t)ni] - lev[(size_t)cur]);   // climbing costs (ramps and stairs)
      int32_t ng = g[(size_t)cur] + step;
      if (ng < g[(size_t)ni]) {
        g[(size_t)ni] = ng;
        from[(size_t)ni] = cur;
        open.push({ng + heur(ni), ni});
      }
    }
  }
  if (!found) return false;
  std::vector<GTile> pts;
  for (int c = gi; c >= 0; c = from[(size_t)c]) {
    int32_t cx = gx0 + c % W, cy = gy0 + c / W;
    uint64_t h = tileHash(seed ^ tag("r.jit"), cx, cy);
    pts.push_back(GTile{cx * RG + RG / 2 + (int32_t)(h & 7) - 3, cy * RG + RG / 2 + (int32_t)((h >> 8) & 7) - 3});
    if (c == si) break;
  }
  std::reverse(pts.begin(), pts.end());
  pts.front() = GTile{ax, ay};
  pts.back() = GTile{bx, by};
  chaikin(pts, 3);
  out.swap(pts);
  return true;
}

std::shared_ptr<const Edge> EndlessSource::Impl::edge(const Node& a0, const Node& b0) {
  const Node& a = a0.id < b0.id ? a0 : b0;
  const Node& b = a0.id < b0.id ? b0 : a0;
  uint64_t k = mix64(a.id ^ mix64(b.id));
  if (auto e = edges.get(k)) return e;
  auto t0 = Clock::now();
  auto E = std::make_shared<Edge>();
  E->key = k;
  E->a = a.id; E->b = b.id;
  E->ta = a.type; E->tb = b.type;
  E->cls = (a.type != SiteType::Village && b.type != SiteType::Village) ? 0 : 1;
  if (!roadPath(a.x, a.y, b.x, b.y, 112, E->pts)) roadPath(a.x, a.y, b.x, b.y, 260, E->pts);
  if (!E->pts.empty()) {
    E->x0 = E->y0 = INT32_MAX; E->x1 = E->y1 = INT32_MIN;
    for (const GTile& g : E->pts) {
      E->x0 = std::min(E->x0, g.x - 2); E->y0 = std::min(E->y0, g.y - 2);
      E->x1 = std::max(E->x1, g.x + 2); E->y1 = std::max(E->y1, g.y + 2);
    }
  } else {
    E->x0 = E->y0 = 0; E->x1 = E->y1 = -1;
  }
  double ms = msSince(t0);
  stats.roadEdges++;
  stats.roadMs += ms;
  stats.maxRoadMs = std::max(stats.maxRoadMs, ms);
  edges.put(k, E);
  return E;
}

void EndlessSource::Impl::graphEdges(int32_t x0, int32_t y0, int32_t x1, int32_t y1, std::vector<std::shared_ptr<const Edge>>& out) {
  const int32_t M = LMAX + 260 + 10;   // endpoints and Gabriel blockers of every edge whose corridor reaches the rect
  std::vector<Node> nodes;
  nodesIn(x0 - M, y0 - M, x1 + M, y1 + M, nodes);
  std::sort(nodes.begin(), nodes.end(), [](const Node& a, const Node& b) { return a.id < b.id; });
  for (size_t i = 0; i < nodes.size(); i++)
    for (size_t j = i + 1; j < nodes.size(); j++) {
      const Node &a = nodes[i], &b = nodes[j];
      int64_t ab2 = dist2(a.x, a.y, b.x, b.y);
      if (ab2 > (int64_t)LMAX * LMAX) continue;
      // quick reject: the straight line (plus the widest corridor) must come near the rectangle
      if (std::max(a.x, b.x) + 260 < x0 || std::min(a.x, b.x) - 260 > x1 || std::max(a.y, b.y) + 260 < y0 || std::min(a.y, b.y) - 260 > y1) continue;
      // Gabriel: no other node inside the circle with diameter AB (doubled coordinates: exact integers)
      bool gab = true;
      const int64_t mx = (int64_t)a.x + b.x, my = (int64_t)a.y + b.y;
      for (size_t c = 0; c < nodes.size() && gab; c++) {
        if (c == i || c == j) continue;
        int64_t dx = 2 * (int64_t)nodes[c].x - mx, dy = 2 * (int64_t)nodes[c].y - my;
        if (dx * dx + dy * dy < ab2) gab = false;
      }
      // (M2 start guarantee) the road from the start village to the story city is always built
      const bool story = ((a.flags & SPF_START) && (b.flags & SPF_STORY)) || ((b.flags & SPF_START) && (a.flags & SPF_STORY));
      if (!gab && !story) continue;
      std::shared_ptr<const Edge> e = edge(a, b);
      if (e->x1 < e->x0 || e->x1 < x0 || e->x0 > x1 || e->y1 < y0 || e->y0 > y1) continue;
      out.push_back(e);
    }
}

}  // namespace ew
