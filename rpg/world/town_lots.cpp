// M7 "Home" (VISION_PLAN 8.2, 15.22): a settlement's lots for sale (LAND lane). Villages and towns get 1 to 3 fenced
// empty lots on their outskirts beside a street: the lot lies along the street where it can (its long side on the
// frontage), its gate on the street side, either right on the street or at the end of its own short footpath (a lot
// set back from the road, as the outskirts' cottages are), and a FOR SALE sign inside by the gate. Where a river or a
// lake runs along the outskirts, a lot takes the bank (a long side on the water: PLOT_RIVERSIDE). The layout contract is
// rpg/world/plots.h; the HOMESTEAD lane's buyLot turns the ring into the player's own fence and gate.
//
// How: every tile a lot may not cover (anything built, walled, paved, wet, farmed, a door's front, a roof's shadow, a
// gate's approach, another lot and its margin) is marked and summed in prefix tables, so every rectangle of the three
// sizes (both ways round) is tested in O(1). A footpath flood from the streets the heart reaches (over open ground only,
// up to PATH_MAX tiles) says how far each tile is from a street; a lot's gate is the border tile whose outside step is
// nearest a street. The best lot by frontage (the street running along its gate side), outskirts, the bank, the path's
// length and the spread round the heart wins. A lot that would cut a door off from the heart is refused.
// Determinism: hashes of the plan's seed and the tile only (never the shared rng: the rest of the town is unchanged by
// the lots' choices), integer tests; dist() is the generator's own IEEE-exact float distance.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "rpg/world/gen.h"
#include "rpg/world/town_gen.h"

namespace ew {
namespace town {

using art::Prop;

namespace {
const int D4X[4] = {0, 1, -1, 0}, D4Y[4] = {1, 0, 0, -1};   // 0 south, 1 east, 2 west, 3 north
// the three lot sizes (home::lotInfo: small 10x8, medium 13x10, large 16x12)
const int LOT_W[3] = {10, 13, 16}, LOT_H[3] = {8, 10, 12};
constexpr int PATH_MAX = 9;   // the longest footpath from a street to a lot's gate

// a 2D prefix sum over a byte mask (count of set tiles in a rectangle)
struct Sum {
  int W = 0, H = 0;
  std::vector<int> s;
  void build(const std::vector<uint8_t>& m, int w, int h) {
    W = w; H = h;
    s.assign((size_t)(W + 1) * (H + 1), 0);
    for (int y = 0; y < H; y++) {
      int row = 0;
      for (int x = 0; x < W; x++) {
        row += m[(size_t)y * W + x] ? 1 : 0;
        s[(size_t)(y + 1) * (W + 1) + x + 1] = s[(size_t)y * (W + 1) + x + 1] + row;
      }
    }
  }
  // tiles set in [x0, x1) x [y0, y1) (outside the buffer counts as set)
  int count(int x0, int y0, int x1, int y1) const {
    int out = 0;
    if (x0 < 0 || y0 < 0 || x1 > W || y1 > H) out = 1;
    x0 = std::max(0, x0); y0 = std::max(0, y0); x1 = std::min(W, x1); y1 = std::min(H, y1);
    if (x1 <= x0 || y1 <= y0) return out;
    return out + s[(size_t)y1 * (W + 1) + x1] - s[(size_t)y0 * (W + 1) + x1] - s[(size_t)y1 * (W + 1) + x0] + s[(size_t)y0 * (W + 1) + x0];
  }
};
}  // namespace

void Gen::lots() {
  O.plots.clear();
  if (!village && !town) return;   // (cities: their homes come for sale instead, VISION_PLAN 8.1)
  const uint32_t ls = P.seed * 0x9E3779B1u ^ 0x10775A1Eu;
  // how many: a village one to three, a town two or three
  const uint32_t hn = hash32(ls ^ 0x51u);
  const int want = village ? 1 + (hn % 100u < 45u ? 1 : 0) + (hn % 100u < 12u ? 1 : 0) : 2 + ((hn >> 8) % 100u < 45u ? 1 : 0);

  // ---- what the heart reaches on foot (the gate's street must be one of these tiles)
  auto flood = [&](std::vector<uint8_t>& seen) {
    seen.assign((size_t)W * H, 0);
    std::vector<int> q;
    for (int oy = -3; oy <= 3; oy++)
      for (int ox = -3; ox <= 3; ox++) {
        const int x = cx + ox, y = cy + oy;
        if (walkable(x, y) && !seen[I(x, y)]) { seen[I(x, y)] = 1; q.push_back((int)I(x, y)); }
      }
    for (size_t h = 0; h < q.size(); h++) {
      const int x = q[h] % W, y = q[h] / W;
      for (int d = 0; d < 4; d++) {
        const int nx = x + D4X[d], ny = y + D4Y[d];
        if (!in(nx, ny) || seen[I(nx, ny)] || !walkable(nx, ny)) continue;
        seen[I(nx, ny)] = 1;
        q.push_back((int)I(nx, ny));
      }
    }
  };
  std::vector<uint8_t> reach;
  flood(reach);
  auto doorsReached = [&](const std::vector<uint8_t>& seen) {
    int n = 0;
    for (const Bldg& b : M.bldgs) {
      const int ax = b.doorX(), ay = b.r.y + b.r.h;
      if (in(ax, ay) && seen[I(ax, ay)]) n++;
    }
    return n;
  };
  const int doors0 = doorsReached(reach);

  // ---- the masks: bad (no lot tile here), ringBad (not even beside a lot: built things, walls, doors' fronts, roofs)
  std::vector<uint8_t> bad((size_t)W * H, 0), ringBad((size_t)W * H, 0), bank((size_t)W * H, 0);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const size_t i = I(x, y);
      const Ground g = M.at(x, y);
      bank[i] = groundWater(g) && M.biomeAt(x, y) != Biome::Ocean ? 1 : 0;   // a river or a lake (not the sea)
      const bool built = M.bldgAt[i] >= 0 || M.wall[i] || front[i] || cover[i] || inCompound(x, y, 1);
      ringBad[i] = built ? 1 : 0;
      bool b = built || mask[i] != K_NONE || M.prop[i] || water[i] || groundSolid(g) || g == Ground::Bridge || g == Ground::Road ||
               g == Ground::Plaza || g == Ground::Farmland || noBuild[i] || face(x, y) || (!reserved.empty() && reserved[i]);
      // within two tiles of the footprint (World::siteAt(.., 2) finds the settlement there: the view draws the fence in
      // its people's style, the lot is priced and sold as the settlement's; the lot and the ring round it are the town's
      // used ground, which the chunks' roads, tracks and wild growth keep off), and never by the wall's ring
      if (x < MARGIN - 2 || y < MARGIN - 2 || x >= W - MARGIN + 2 || y >= H - MARGIN + 2) b = true;
      if (walled && nearRing(x, y, 2)) b = true;
      bad[i] = b ? 1 : 0;
    }
  // the relief: a lot stands on one level (a count per level)
  std::vector<uint8_t> lvlMask((size_t)W * H, 0);
  Sum lvlSum[8];
  bool lvlUsed[8] = {};
  for (size_t i = 0; i < lvl.size(); i++) lvlUsed[std::min<int>(lvl[i], 7)] = true;
  for (int L = 0; L < 8; L++) {
    if (!lvlUsed[L]) continue;
    for (size_t i = 0; i < lvl.size(); i++) lvlMask[i] = std::min<int>(lvl[i], 7) == L ? 1 : 0;
    lvlSum[L].build(lvlMask, W, H);
  }
  Sum badS, ringS, ringS2;
  badS.build(bad, W, H);
  ringS.build(ringBad, W, H);
  {   // (the last resort, a crowded palisade village) a ring that may run behind a house, under its roof's overhang
    std::vector<uint8_t> r2((size_t)W * H, 0);
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        const size_t i = I(x, y);
        r2[i] = (uint8_t)(M.bldgAt[i] >= 0 || M.wall[i] || front[i] || inCompound(x, y, 1) ? 1 : 0);
      }
    ringS2.build(r2, W, H);
  }
  auto streetTile = [&](int x, int y) {
    if (!in(x, y) || !reach[I(x, y)]) return false;
    const uint8_t k = get(x, y);
    const Ground g = M.at(x, y);
    if (g == Ground::Bridge) return stilt && k != K_NONE;   // (a stilt town's boardwalks are its streets; a river bridge is no frontage)
    if (k == K_MAIN || k == K_LANE) return true;
    // a square or a green only where it is paved or trodden (a gate opens onto the way, not onto the green's lawn)
    return k == K_SQUARE && (g == Ground::Dirt || g == Ground::Road || g == Ground::Plaza);
  };

  if (const char* dbg = std::getenv("EMB_LOTS_DEBUG")) {   // (debugging aid) the masks as a PGM: bad 255, ring-only 128
    if (FILE* f = std::fopen(dbg, "wb")) {
      std::fprintf(f, "P5 %d %d 255 ", W, H);
      for (size_t i = 0; i < bad.size(); i++) { const uint8_t v = (uint8_t)(ringBad[i] ? 60 : (bad[i] ? (mask[i] == K_YARD ? 160 : (mask[i] != K_NONE ? 110 : 255)) : 0)); std::fputc(v, f); }
      std::fclose(f);
    }
  }
  struct Cand { int x, y, w, h, gx, gy, side, size, score, pathD; bool river; };
  std::vector<Cand> chosen;
  std::vector<int> pathDist, pathPrev;
  int refused = 0;
  // a trodden way: packed earth or paving (a yard's lawn or flower bed is not a path to branch off)
  auto pathGround = [&](Ground g) { return g == Ground::Dirt || g == Ground::Road || g == Ground::Plaza || (stilt && g == Ground::Bridge); };
  // ---- the footpath flood: how far each open tile is from a street the heart reaches (0 on the street), up to maxPath
  auto pathFlood = [&](int maxPath) {
    pathDist.assign((size_t)W * H, -1);
    pathPrev.assign((size_t)W * H, -1);
    {
      std::vector<int> q;
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          // a street, or a yard path the heart reaches (a lot's own path may branch off a cottage's, as lanes do)
          if (streetTile(x, y) || (get(x, y) == K_YARD && reach[I(x, y)] && !front[I(x, y)] && pathGround(M.at(x, y)))) {
            pathDist[I(x, y)] = 0;
            q.push_back((int)I(x, y));
          }
      for (size_t h = 0; h < q.size(); h++) {
        const int x = q[h] % W, y = q[h] / W, d0 = pathDist[(size_t)q[h]];
        if (d0 >= maxPath) continue;
        for (int d = 0; d < 4; d++) {
          const int nx = x + D4X[d], ny = y + D4Y[d];
          if (!in(nx, ny) || pathDist[I(nx, ny)] >= 0 || bad[I(nx, ny)] || ringBad[I(nx, ny)]) continue;
          pathDist[I(nx, ny)] = d0 + 1;
          pathPrev[I(nx, ny)] = q[h];
          q.push_back((int)I(nx, ny));
        }
      }
    }
  };
  for (int k = 0; k < want; k++) {
    // the size asked (a smaller one when nothing that big fits)
    const uint32_t hs = hash32(ls ^ (0x5123u + (uint32_t)k * 0x9E37u));
    const int size0 = village ? (hs % 100u < 50u ? 0 : (hs % 100u < 85u ? 1 : 2)) : (hs % 100u < 25u ? 0 : (hs % 100u < 65u ? 1 : 2));
    Cand best{};
    int nPrefix = 0, nLevel = 0, nGate = 0, nPath = 0;
    best.score = -1000000;
    bool found = false;
    // a footpath of up to PATH_MAX tiles; where none reaches open ground big enough, a longer one (a lot behind the houses)
    // (pass 2: a track across the outskirts to open ground beyond a palisade; pass 3: the ring may pass behind a house)
    for (int pass = 0; pass < 4 && !found; pass++) {
    pathFlood(pass == 0 ? PATH_MAX : (pass == 1 ? 2 * PATH_MAX : 5 * PATH_MAX));
    const Sum& RS = pass < 3 ? ringS : ringS2;
    for (int size = size0; size >= 0 && !found; size--)
      for (int orient = 0; orient < 2; orient++) {
        const int lw = orient ? LOT_H[size] : LOT_W[size], lh = orient ? LOT_W[size] : LOT_H[size];
        for (int y0 = 3; y0 + lh <= H - 3; y0++)
          for (int x0 = 3; x0 + lw <= W - 3; x0++) {
            const int x1 = x0 + lw, y1 = y0 + lh;
            if (badS.count(x0, y0, x1, y1) || RS.count(x0 - 1, y0 - 1, x1 + 1, y1 + 1)) continue;
            const int L = std::min<int>(lvl[I(x0, y0)], 7);
            nPrefix++;
            if (lvlSum[L].count(x0, y0, x1, y1) != lw * lh) continue;
            nLevel++;
            // the gate: the border tile (two or more from a corner) whose outside step is nearest a street, nearest the
            // middle of its side on a tie
            int gx = -1, gy = -1, gside = -1, gd = 1 << 20;
            for (int side = 0; side < 4; side++) {   // the outward direction of the border: 0 south .. 3 north
              const bool horiz = side == 0 || side == 3;
              const int len = horiz ? lw : lh;
              for (int t = 2; t < len - 2; t++) {
                const int bx = horiz ? x0 + t : (side == 1 ? x1 - 1 : x0), by = horiz ? (side == 0 ? y1 - 1 : y0) : y0 + t;
                const int ox = bx + D4X[side], oy = by + D4Y[side];
                if (!in(ox, oy) || pathDist[I(ox, oy)] < 0) continue;
                // the outside step on the same level as the gate (or the street's ramp)
                if (lvl[I(ox, oy)] != lvl[I(bx, by)] && !(M.height.size() && (M.height[I(ox, oy)] & Map::HEIGHT_RAMP))) continue;
                const int cost = pathDist[I(ox, oy)] * 64 + std::abs(t - len / 2);
                if (cost < gd) { gd = cost; gx = bx; gy = by; gside = side; }
              }
            }
            if (gx < 0) continue;
            nGate++;
            const int pd = gd / 64;
            // a path through the lot itself (the flood ran before the lot was there) is no path
            {
              bool through = false;
              for (int at = pathPrev[I(gx + D4X[gside], gy + D4Y[gside])]; at >= 0 && !through; at = pathPrev[(size_t)at]) {
                const int px = at % W, py = at / W;
                through = px >= x0 - 1 && px <= x1 && py >= y0 - 1 && py <= y1;
              }
              if (through) continue;
              nPath++;
            }
            // the frontage: the street along the gate's side (within three rows beyond it), the lot following it
            const bool gh = gside == 0 || gside == 3;
            const int along = gh ? lw : lh;
            int run = 0;
            for (int t = 0; t < along; t++) {
              bool st = false;
              for (int o = 1; o <= 3 && !st; o++) {
                const int sx = gh ? x0 + t : (gside == 1 ? x1 - 1 + o : x0 - o), sy = gh ? (gside == 0 ? y1 - 1 + o : y0 - o) : y0 + t;
                st = in(sx, sy) && (get(sx, sy) == K_MAIN || get(sx, sy) == K_LANE);
              }
              run += st;
            }
            // the bank: a long side (not the gate's) with a river or lake within four tiles beyond it along half of it or more
            bool river = false;
            for (int bs = 0; bs < 4 && !river; bs++) {
              if (bs == gside) continue;
              const bool horiz = bs == 0 || bs == 3;
              const int len = horiz ? lw : lh;
              if (len != std::max(lw, lh)) continue;
              int wet = 0;
              for (int t = 0; t < len; t++) {
                const int bx = horiz ? x0 + t : (bs == 1 ? x1 - 1 : x0), by = horiz ? (bs == 0 ? y1 - 1 : y0) : y0 + t;
                const int ox = D4X[bs], oy = D4Y[bs];
                bool w = false;
                for (int o = 1; o <= 4 && !w; o++) w = in(bx + o * ox, by + o * oy) && bank[I(bx + o * ox, by + o * oy)];
                wet += w;
              }
              if (wet * 2 >= len) river = true;
            }
            const int ccx = x0 + lw / 2, ccy = y0 + lh / 2;
            const float dc = dist(ccx, ccy);
            int score = run * 40 / along + (along == std::max(lw, lh) ? 16 : 0) - (int)(std::abs(dc - (village ? 0.8f : 0.85f)) * 90.0f) - pd * 5;
            // a bank lot is the prize of a river town: the first lot takes one wherever the water allows
            bool haveRiver = false;
            for (const Cand& c : chosen) haveRiver = haveRiver || c.river;
            if (river) score += haveRiver ? 20 : 150;
            for (const Cand& c : chosen) {
              const int dd = std::max(std::abs((c.x + c.w / 2) - ccx), std::abs((c.y + c.h / 2) - ccy));
              if (dd < 30) score -= (30 - dd) * 3;
            }
            score += (int)(hashAt(x0, y0, ls ^ 0xC0FFEEu) % 12u);
            if (score > best.score) {
              best = Cand{x0, y0, lw, lh, gx, gy, gside, size, score, pd, river};
              found = true;
            }
          }
      }
    }
    if (std::getenv("EMB_LOTS_DEBUG")) {
      int ns = 0, np = 0;
      for (int i = 0; i < W * H; i++) { ns += pathDist[(size_t)i] == 0; np += pathDist[(size_t)i] > 0; }
      std::fprintf(stderr, "lots k %d: streets %d path tiles %d, rects ok %d level %d gate %d path %d found %d\n", k, ns, np, nPrefix, nLevel, nGate, nPath, (int)found);
    }
    if (!found) break;
    // try it: the ring and the sign, then a flood from the heart must still reach every door it reached
    const Cand& c = best;
    std::vector<std::pair<size_t, uint8_t>> undoProp;
    auto put = [&](int x, int y, Prop p) { undoProp.push_back({I(x, y), M.prop[I(x, y)]}); M.setProp(x, y, p); };
    for (int y = c.y; y < c.y + c.h; y++)
      for (int x = c.x; x < c.x + c.w; x++) {
        const bool edgeY = y == c.y || y == c.y + c.h - 1, edgeX = x == c.x || x == c.x + c.w - 1;
        if (!edgeX && !edgeY) continue;
        if (x == c.gx && y == c.gy) continue;
        put(x, y, edgeY ? Prop::FenceH : Prop::FenceV);
      }
    // the sign: two steps inside the gate and one to the side (clear of the way in, and standing free of the fence: a
    // step inside it stood against the wall's face, drawn over it)
    {
      const int ix = c.gx - 2 * D4X[c.side], iy = c.gy - 2 * D4Y[c.side];
      const int lat = (hashAt(c.gx, c.gy, 0x5161u) & 1u) ? 1 : -1;
      put(ix + (D4Y[c.side] ? lat : 0), iy + (D4X[c.side] ? lat : 0), Prop::ForSaleSign);
    }
    std::vector<uint8_t> after;
    flood(after);
    if (doorsReached(after) < doors0 || !after[I(c.gx, c.gy)]) {
      for (auto it = undoProp.rbegin(); it != undoProp.rend(); ++it) M.prop[it->first] = it->second;
      // never again here: the lot's tiles are marked bad and the search goes on
      for (int y = c.y; y < c.y + c.h; y++) for (int x = c.x; x < c.x + c.w; x++) bad[I(x, y)] = 1;
      badS.build(bad, W, H);
      k--;
      if (++refused > 6) break;   // (a guard: never loops for long)
      continue;
    }
    // keep it: the lot's ground is the town's (K_LOT), and the footpath from the street to its gate is laid (packed
    // earth, a yard path to the town: the chunks keep it)
    for (int y = c.y; y < c.y + c.h; y++)
      for (int x = c.x; x < c.x + c.w; x++) set(x, y, K_LOT);
    // (M7 fix r3) the trodden earth runs in under the gate itself and onto its outside step (a one-tile track that
    // stopped short of the gate drew as a blob of earth hidden under whoever stood there, grass up to the gate)
    for (const int at : {(int)I(c.gx, c.gy), (int)I(c.gx + D4X[c.side], c.gy + D4Y[c.side])}) {
      const Ground vg = M.at(at % W, at / W);
      if (vg != Ground::Sand && vg != Ground::Snow && !groundSolid(vg) && vg != Ground::Bridge) M.setG(at % W, at / W, Ground::Dirt);
    }
    int pathEnd = (int)I(c.gx + D4X[c.side], c.gy + D4Y[c.side]);
    for (int at = pathEnd; at >= 0 && pathDist[(size_t)at] > 0; at = pathPrev[(size_t)at]) {
      const int px = at % W, py = at / W;
      set(px, py, K_YARD);
      const Ground vg = M.at(px, py);
      if (vg != Ground::Sand && vg != Ground::Snow) M.setG(px, py, Ground::Dirt);
      pathEnd = pathPrev[(size_t)at];
    }
    // (M7 fix r3, review: "the village lot's gate has no footpath out to the road") the path ends on a street tile of
    // the plan, but a lane's verge is often still grass where its trodden earth runs a tile or two off: the path goes
    // on over the open ground to the nearest packed earth or paving (a short flood, never through a lot or its ring)
    if (pathEnd >= 0 && !pathGround(M.at(pathEnd % W, pathEnd / W))) {
      std::vector<int> prev((size_t)W * H, -2);
      std::vector<int> q{pathEnd};
      prev[(size_t)pathEnd] = -1;
      int found = -1;
      for (size_t h = 0; h < q.size() && found < 0; h++) {
        int d = 0;
        for (int at = q[h]; prev[(size_t)at] >= 0; at = prev[(size_t)at]) d++;
        if (d >= 5) continue;
        const int x = q[h] % W, y = q[h] / W;
        for (int k2 = 0; k2 < 4 && found < 0; k2++) {
          const int nx = x + D4X[k2], ny = y + D4Y[k2];
          if (!in(nx, ny) || prev[I(nx, ny)] != -2) continue;
          if (nx >= c.x - 1 && nx <= c.x + c.w && ny >= c.y - 1 && ny <= c.y + c.h) continue;   // the lot and its ring
          if (get(nx, ny) == K_LOT || !walkable(nx, ny) || M.prop[I(nx, ny)] || M.bldgAt[I(nx, ny)] >= 0) continue;
          prev[I(nx, ny)] = q[h];
          if (pathGround(M.at(nx, ny))) { found = (int)I(nx, ny); break; }
          q.push_back((int)I(nx, ny));
        }
      }
      if (found >= 0)
        for (int at = prev[(size_t)found]; at >= 0; at = prev[(size_t)at]) {
          const int px = at % W, py = at / W;
          if (get(px, py) == K_NONE) set(px, py, K_YARD);
          const Ground vg = M.at(px, py);
          if (vg != Ground::Sand && vg != Ground::Snow) M.setG(px, py, Ground::Dirt);
        }
    }
    PlotPlan pp;
    const uint32_t sl = idLocal(P.id), slot = sl < 0x40u ? sl : 0x40u + (sl & 0x3Fu);
    pp.id = makeId(idRx(P.id), idRy(P.id), IdKind::Plot, (slot * 8u + (uint32_t)chosen.size()) & 0x7FFu);
    pp.site = P.id;
    pp.gx = O.gx + c.x; pp.gy = O.gy + c.y;
    pp.w = (uint8_t)c.w; pp.h = (uint8_t)c.h;
    pp.size = (uint8_t)c.size;
    pp.flags = (uint8_t)(PLOT_FENCED | (c.river ? PLOT_RIVERSIDE : 0));
    pp.gateX = O.gx + c.gx; pp.gateY = O.gy + c.gy;
    O.plots.push_back(pp);
    chosen.push_back(c);
    // the next lot keeps a margin of two tiles from this one (and off this one's path)
    for (int y = c.y - 3; y < c.y + c.h + 3; y++)
      for (int x = c.x - 3; x < c.x + c.w + 3; x++)
        if (in(x, y)) bad[I(x, y)] = 1;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) if (get(x, y) == K_YARD) bad[I(x, y)] = 1;
    badS.build(bad, W, H);
    reach.swap(after);
  }
}

}  // namespace town
}  // namespace ew
