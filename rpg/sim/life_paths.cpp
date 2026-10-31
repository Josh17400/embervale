// LifeOps (life_ops.h, TOWNSFOLK lane): outdoor paths.
// A* over the window (findPath, cached by remember), walking a route (walkOut) and the stuck watchdog.
// Moved verbatim from life_game.cpp (SOLID 1.2); the Game::life* hooks that call these stay in life_game.cpp.
#include "rpg/sim/life_ops.h"

// ---------------------------------------------------------------- paths (outdoors)
int LifeOps::stepCost(const Map& m, int x, int y) {
  switch (m.at(x, y)) {
    case Ground::Road: case Ground::Plaza: case Ground::Bridge: return 10;
    case Ground::Dirt: return 12;
    case Ground::Farmland: return 17;
    default: return 14;
  }
}

// inView: (P1 fixer r2: the cutpurse's run) the open street costs as usual and a hidden tile (openTile) five times
// over, so the chase stays where it is seen and the catch never happens behind a stall
bool LifeOps::findPath(Game& g, int sx, int sy, int tx, int ty, std::vector<int32_t>& out, bool inView) {
  RT& R = rt(g);
  const Map& m = g.world.over;
  out.clear();
  if (!m.in(sx, sy) || !m.in(tx, ty)) return false;
  const uint64_t key = ew::mix64(((uint64_t)(uint32_t)(g.world.ox + sx) << 32 | (uint32_t)(g.world.oy + sy)) * 31ull ^
                                 ((uint64_t)(uint32_t)(g.world.ox + tx) << 32 | (uint32_t)(g.world.oy + ty))) ^ (inView ? 0x0BE11ull : 0ull);
  auto it = R.cache.find(key);
  if (it != R.cache.end()) { out = it->second; R.stats.pathCacheHits++; return !out.empty(); }
  R.stats.pathRequests++;
  const int pad = 18;
  const int x0 = std::max(0, std::min(sx, tx) - pad), y0 = std::max(0, std::min(sy, ty) - pad);
  const int x1 = std::min(m.w - 1, std::max(sx, tx) + pad), y1 = std::min(m.h - 1, std::max(sy, ty) + pad);
  const int W = x1 - x0 + 1, H = y1 - y0 + 1;
  if (W > 200 || H > 200) { R.stats.pathFails++; return false; }
  const size_t N = (size_t)W * H;
  R.gScore.assign(N, INT32_MAX);
  R.came.assign(N, -1);
  R.closed.assign(N, 0);
  auto L = [&](int x, int y) { return (y - y0) * W + (x - x0); };
  auto pass = [&](int x, int y) {
    if (x == tx && y == ty) return true;
    const size_t i = (size_t)y * m.w + x;
    return !m.blocked(x, y) && m.bldgAt[i] < 0;
  };
  struct Node { int f, i; };
  std::vector<Node> heap;
  heap.reserve(512);
  auto push = [&](int f, int i) { heap.push_back({f, i}); std::push_heap(heap.begin(), heap.end(), [](const Node& a, const Node& b) { return a.f > b.f; }); };
  const int s = L(sx, sy), goal = L(tx, ty);
  R.gScore[(size_t)s] = 0;
  push(0, s);
  static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  bool found = false;
  int expanded = 0;
  while (!heap.empty()) {
    std::pop_heap(heap.begin(), heap.end(), [](const Node& a, const Node& b) { return a.f > b.f; });
    const Node n = heap.back();
    heap.pop_back();
    if (R.closed[(size_t)n.i]) continue;
    R.closed[(size_t)n.i] = 1;
    if (n.i == goal) { found = true; break; }
    if (++expanded > 24000) break;
    const int cx = n.i % W + x0, cy = n.i / W + y0;
    for (int k = 0; k < 4; k++) {
      const int nx = cx + dx[k], ny = cy + dy[k];
      if (nx < x0 || ny < y0 || nx > x1 || ny > y1 || !pass(nx, ny)) continue;
      const int li = L(nx, ny);
      if (R.closed[(size_t)li]) continue;
      const int gs = R.gScore[(size_t)n.i] + stepCost(m, nx, ny) * (inView && !openTile(m, nx, ny) ? 5 : 1);
      if (gs >= R.gScore[(size_t)li]) continue;
      R.gScore[(size_t)li] = gs;
      R.came[(size_t)li] = n.i;
      push(gs + 10 * (std::abs(nx - tx) + std::abs(ny - ty)), li);
    }
  }
  if (!found) { R.stats.pathFails++; remember(R, key, out); return false; }
  std::vector<int> rev;
  for (int i = goal; i != s && i >= 0; i = R.came[(size_t)i]) rev.push_back(i);
  for (size_t k = rev.size(); k-- > 0;) {
    out.push_back(g.world.ox + rev[k] % W + x0);
    out.push_back(g.world.oy + rev[k] / W + y0);
  }
  remember(R, key, out);
  return true;
}

void LifeOps::remember(RT& R, uint64_t key, const std::vector<int32_t>& p) {
  if (R.cache.size() >= 192) {
    for (int k = 0; k < 32 && !R.cacheOrder.empty(); k++) { R.cache.erase(R.cacheOrder.front()); R.cacheOrder.erase(R.cacheOrder.begin()); }
  }
  R.cache[key] = p;
  R.cacheOrder.push_back(key);
}

// walk the actor's route outdoors (requesting it when needed). true: arrived at the target tile
bool LifeOps::walkOut(Game& g, Actor& a, RT::ARt& A, float speed, float dt) {
  RT& R = rt(g);
  const Map& m = g.world.over;
  const int lx = A.gx - g.world.ox, ly = A.gy - g.world.oy;
  const Vec2 dest = tileCentre(lx, ly);
  if (len2(dest - a.p) < 3.0f * 3.0f) return true;
  if (A.path.empty() || A.want) {
    // in plain sight: walk straight
    if (len2(dest - a.p) < (8.0f * TILE) * (8.0f * TILE) && clearLine(g, a.p, dest, a.radius)) {
      const Vec2 before = a.p;
      stepToward(g, a, dest, speed, dt);
      A.want = false;
      // (a spot a body cannot quite reach, a pillar's corner, a fence post: close enough counts as there)
      if (len2(a.p - before) < 0.0004f) {
        A.stuckT += dt;
        if (A.stuckT > 1.2f) { A.stuckT = 0; return true; }
      } else A.stuckT = std::max(0.0f, A.stuckT - dt);
      return false;
    }
    if (R.pathBudget <= 0) { a.st = AState::Idle; return false; }
    R.pathBudget--;
    A.want = false;
    std::vector<int32_t> p;
    if (!findPath(g, tileX(a.p), tileY(a.p), lx, ly, p)) {
      A.fails++;
      R.street.legFails[std::min<int>(A.task, 7)]++;
      a.st = AState::Idle;
      return false;
    }
    A.path = std::move(p);
    A.pi = 0;
  }
  // look ahead along the route: cut to the farthest of the next few waypoints in plain sight
  A.lookT -= dt;
  if (A.lookT <= 0) {
    A.lookT = 0.4f;
    for (size_t k = std::min(A.path.size() / 2, A.pi + 5); k > A.pi + 1; k--) {
      const Vec2 w = tileCentre(A.path[(k - 1) * 2] - g.world.ox, A.path[(k - 1) * 2 + 1] - g.world.oy);
      if (clearLine(g, a.p, w, a.radius)) { A.pi = k - 1; break; }
    }
  }
  while (A.pi < A.path.size() / 2) {
    const Vec2 w = tileCentre(A.path[A.pi * 2] - g.world.ox, A.path[A.pi * 2 + 1] - g.world.oy);
    if (len2(w - a.p) < 2.5f * 2.5f) { A.pi++; continue; }
    stepToward(g, a, w, speed, dt);
    // progress toward the waypoint (a body sliding to and fro on a corner moves every frame yet gets nowhere)
    if (A.wpIdx != A.pi) { A.wpIdx = A.pi; A.wpBest = 1e9f; }
    const float dw = len(w - a.p);
    if (dw >= A.wpBest - 0.05f) {
      A.stuckT += dt;
      if (A.stuckT > 1.2f) {   // wedged: plan again (and, wedged again, step back to the middle of a walkable tile)
        A.stuckT = 0; A.path.clear(); A.fails++;
        A.wpIdx = (size_t)-1;
        if (A.fails >= 2) unwedge(g, a);
      }
    } else { A.wpBest = dw; A.stuckT = std::max(0.0f, A.stuckT - dt); }
    (void)m;
    return false;
  }
  const Vec2 before = a.p;
  stepToward(g, a, dest, speed, dt);
  if (len2(a.p - before) < 0.0004f) {   // (the last stretch blocked by a corner: close enough)
    A.stuckT += dt;
    if (A.stuckT > 1.2f) { A.stuckT = 0; return true; }
  }
  return len2(dest - a.p) < 3.0f * 3.0f;
}
