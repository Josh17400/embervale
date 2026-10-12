// M7b Cities (VISION_PLAN 15.23, 15.24): the planned city and its urban fabric (rpg/world/urban.h, Map::urb). CITY lane.
//
// The plan (cities only; a town only tidies its main street and square, a village never changes):
//  - GATES where the roads arrive (on the wall ring's straight runs, as cityWall will open them), a three-wide throat
//    through each, then an AVENUE from every gate to the market square: five tiles (a central drain, two carriage
//    lanes, a kerbed sidewalk either side), straight legs with 45-degree chamfers at the bends, the legs following the
//    heart's row (the city's long axis) and the gate's own line;
//  - the SEAT: a capital's palace square before its compound gate, any other city's seat in a precinct of the noble
//    quarter (its plot behind a square, a lane round the plot), an avenue from the market to that square; the chief
//    TEMPLE likewise in a precinct of the temple quarter behind its close;
//  - the PARK: a block of lawns with gravel paths, a pavilion, benches, trees and hedges, a lane all round it;
//  - CROSS STREETS (organic plans) every blockP rows between the avenues, three tiles wide (a sidewalk either side of a
//    carriage lane), dropped every other row in the rich quarter (deep gardens), dirt alleys in the poor quarter with
//    alleys between; LANES through the blocks north-south, WELL-SQUARES at some of their junctions; the outskirts
//    inside the wall keep winding lanes (organic edges). The other layouts (grid, compound, radial, linear, terraced,
//    stilt) keep their own lattice, ordered by the avenues;
//  - ROW HOUSES (homes, before the frontage sweep): terraces along every front, the units abutting on one front row
//    (Bldg::row ROW_W / ROW_E on each joint), widths, depths and storeys varied by quarter, walled back yards behind
//    (ROW_YARD, K_ROWYARD: gardens in the rich quarter), corner houses closing the ends;
//  - the dress: yard walls and their trees, the park, the temple close's lawns and flower beds, tree pits and lamps
//    along the avenues' open walks, corner stones at junctions and gate throats, planters along rich fronts, the dune
//    souk's awnings over its market avenue.
// No draw from the generator's Rng anywhere in the plan (its choices are hashed from the plan's seed); the buildings it
// makes draw their seeds and roofs as every building does (putBldg), in a fixed order. Integer and dmath maths only.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>
#include "rpg/culture/society.h"
#include "rpg/world/town_gen.h"
#include "rpg/world/urban.h"

namespace ew {

const char* urbName(Urb u) {
  static const char* n[] = {"none", "avenue", "street", "lane", "alley", "sidewalk", "gutter", "square",
                            "close", "park", "parkpath", "garden", "court", "quay"};
  return (int)u < (int)Urb::COUNT ? n[(int)u] : "?";
}

namespace town {

using art::Building;
using art::Prop;

namespace {
const int D4X[4] = {0, 1, -1, 0}, D4Y[4] = {1, 0, 0, -1};
int ifl(float v) { return (int)std::floor(v); }
int irn(float v) { return (int)std::floor(v + 0.5f); }
int sgn(int v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }
// distance from (px, py) to the segment a-b
float segDist(float px, float py, float ax, float ay, float bx, float by) {
  const float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
  const float l2 = vx * vx + vy * vy;
  float t = l2 > 0.0f ? (wx * vx + wy * vy) / l2 : 0.0f;
  t = std::clamp(t, 0.0f, 1.0f);
  const float dx = wx - t * vx, dy = wy - t * vy;
  return std::sqrt(dx * dx + dy * dy);
}
// how a plan class ranks where two bands cross: carriage beats walk (a street's mouth cuts through the kerb), the
// drain runs on through a junction
int urbRank(uint8_t c) {
  switch ((Urb)c) {
    case Urb::Gutter: return 6;
    case Urb::Avenue: return 5;
    case Urb::Street: return 4;
    case Urb::Lane: return 3;
    case Urb::Alley: return 2;
    case Urb::Sidewalk: return 1;
    default: return 0;
  }
}
using Pts = std::vector<std::pair<int, int>>;
}  // namespace

// ------------------------------------------------------------------------------------------------ painting the plan
bool Gen::bandFree(int x, int y) const {
  if (!in(x, y) || x < 1 || y < 1 || x >= W - 1 || y >= H - 1) return false;
  const size_t i = I(x, y);
  if (!keep.empty() && keep[i]) return false;
  if (inCompound(x, y, 1)) return false;
  const uint8_t k = mask[i];
  return k != K_FIELD && k != K_PARK && k != K_ROWYARD && k != K_LOT && k != K_COMPOUND;
}

// Every tile whose centre lies within hw of the polyline becomes street of `kind` (paintStreet: bridges over water),
// classed by its distance from the centre line: an avenue band (cls Avenue, hw >= 2.4) has its drain on the line, the
// carriage either side and the walks at its edges; a street band (cls Street, hw >= 1.4) a carriage lane between two
// walks; narrower bands are all carriage. Squares, the park, yards, fields, the compound and kept plots are left alone;
// a walled city's band stops at its ring.
void Gen::paintBand(const Pts& pts, float hw, uint8_t cls, uint8_t kind, Ground g) {
  if (pts.size() < 2) return;
  const size_t n = (size_t)W * H;
  if (plan.size() != n) plan.assign(n, 0);
  for (size_t s = 0; s + 1 < pts.size(); s++) {
    const float ax = pts[s].first + 0.5f, ay = pts[s].second + 0.5f, bx = pts[s + 1].first + 0.5f, by = pts[s + 1].second + 0.5f;
    const int x0 = ifl(std::min(ax, bx) - hw - 1), x1 = ifl(std::max(ax, bx) + hw + 1);
    const int y0 = ifl(std::min(ay, by) - hw - 1), y1 = ifl(std::max(ay, by) + hw + 1);
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        if (!in(x, y)) continue;
        const float d = segDist(x + 0.5f, y + 0.5f, ax, ay, bx, by);
        if (d > hw + 0.01f) continue;
        if (walled && !ins(x, y)) continue;
        if (!bandFree(x, y)) continue;
        const uint8_t cur = mask[I(x, y)];
        if (cur == K_SQUARE) continue;
        paintStreet(x, y, kind, g);
        const uint8_t now = mask[I(x, y)];
        if (now != K_MAIN && now != K_LANE) continue;
        uint8_t c = cls;
        if ((Urb)cls == Urb::Avenue && hw >= 2.4f) c = (uint8_t)(d < 0.5f ? Urb::Gutter : (d <= 1.5f ? Urb::Avenue : Urb::Sidewalk));
        else if ((Urb)cls == Urb::Street && hw >= 1.4f) c = (uint8_t)(d < 0.5f ? Urb::Street : Urb::Sidewalk);
        uint8_t& p = plan[I(x, y)];
        if (urbRank(c) > urbRank(p)) p = c;
      }
  }
  // (fixer M7b r2, review: "the diagonal avenue's sidewalks are stacked square kerb blocks, each stepping a full tile")
  // where the band runs on a slant its walk tiles touch only corner to corner: the tile outside the band in each such
  // notch joins the walk, so the walk is one unbroken run whose kerb the view can lay on the slant
  if (!(((Urb)cls == Urb::Avenue && hw >= 2.4f) || ((Urb)cls == Urb::Street && hw >= 1.4f))) return;
  bool slant = false;
  for (size_t s = 0; s + 1 < pts.size(); s++) if (pts[s].first != pts[s + 1].first && pts[s].second != pts[s + 1].second) slant = true;
  if (!slant) return;
  int bx0 = W, by0 = H, bx1 = -1, by1 = -1;
  for (const auto& p : pts) { bx0 = std::min(bx0, p.first); bx1 = std::max(bx1, p.first); by0 = std::min(by0, p.second); by1 = std::max(by1, p.second); }
  const int R = (int)hw + 2;
  auto walkAt = [&](int x, int y) { return in(x, y) && (Urb)plan[I(x, y)] == Urb::Sidewalk && (mask[I(x, y)] == K_MAIN || mask[I(x, y)] == K_LANE); };
  auto carrAt = [&](int x, int y) {
    if (!in(x, y)) return false;
    const Urb u = (Urb)plan[I(x, y)];
    return u == Urb::Avenue || u == Urb::Gutter || u == Urb::Street;
  };
  std::vector<std::pair<int, int>> add;
  for (int y = std::max(1, by0 - R); y <= std::min(H - 2, by1 + R); y++)
    for (int x = std::max(1, bx0 - R); x <= std::min(W - 2, bx1 + R); x++) {
      const size_t i = I(x, y);
      if (plan[i] != 0 || mask[i] != K_NONE || !bandFree(x, y) || (walled && !ins(x, y)) || water[i]) continue;
      // (only along the band's slanting legs: a block's corner at a junction or a square bend is not a notch)
      float d = 1e9f;
      for (size_t s = 0; s + 1 < pts.size(); s++) {
        if (pts[s].first == pts[s + 1].first || pts[s].second == pts[s + 1].second) continue;
        d = std::min(d, segDist(x + 0.5f, y + 0.5f, pts[s].first + 0.5f, pts[s].second + 0.5f, pts[s + 1].first + 0.5f, pts[s + 1].second + 0.5f));
      }
      if (d > hw + 0.9f) continue;
      bool notch = false;
      for (int hx : {-1, 1})
        for (int vy : {-1, 1})
          if (walkAt(x + hx, y) && walkAt(x, y + vy) && carrAt(x + hx, y + vy)) notch = true;
      if (notch) add.push_back({x, y});
    }
  for (const auto& a : add) {
    paintStreet(a.first, a.second, kind, g);
    const uint8_t now = mask[I(a.first, a.second)];
    if (now == K_MAIN || now == K_LANE) plan[I(a.first, a.second)] = (uint8_t)Urb::Sidewalk;
  }
}

// An avenue from (sx, sy), leaving along (dx, dy), to (ex, ey): of a few octilinear routes (the gate's own line first
// then the heart's row, or the other way round; a 45-degree jog when the gate stands near the axis) the one that keeps
// out of the compound, the kept plots and the water, and is shortest. Bends are chamfered.
bool Gen::routeAvenue(int sx, int sy, int dx, int dy, int ex, int ey, float hw) {
  auto chamfer = [](Pts p) {
    // each inner corner cut by a 45-degree piece a few tiles long
    Pts out;
    out.push_back(p[0]);
    for (size_t i = 1; i + 1 < p.size(); i++) {
      const int ax = p[i].first - p[i - 1].first, ay = p[i].second - p[i - 1].second;
      const int bx = p[i + 1].first - p[i].first, by = p[i + 1].second - p[i].second;
      const int la = std::max(std::abs(ax), std::abs(ay)), lb = std::max(std::abs(bx), std::abs(by));
      const bool straightA = ax == 0 || ay == 0, straightB = bx == 0 || by == 0;
      const int k = std::min(4, std::min(la, lb) / 3);
      if (k >= 1 && straightA && straightB && (sgn(ax) != sgn(bx) || sgn(ay) != sgn(by))) {
        out.push_back({p[i].first - sgn(ax) * k, p[i].second - sgn(ay) * k});
        out.push_back({p[i].first + sgn(bx) * k, p[i].second + sgn(by) * k});
      } else out.push_back(p[i]);
    }
    out.push_back(p.back());
    return out;
  };
  std::vector<Pts> cands;
  const int s3x = sx + dx * 3, s3y = sy + dy * 3;   // a short straight stub on the way it leaves
  if (style == cult::Layout::Radial) {
    // the radial city's avenues are its spokes: (fixer M7b r2, review: "the steppe's diagonal avenue is a sawtooth
    // staircase of kerb blocks") laid octilinear, a straight run and a true 45-degree run (no spoke at an odd slope:
    // the kerbs, walks and drain of a 45-degree run join corner to corner), the straight run from the gate or into
    // the heart, whichever keeps clearer
    const int ddx = ex - sx, ddy = ey - sy, mm = std::min(std::abs(ddx), std::abs(ddy));
    if (mm == 0 || std::abs(ddx) == std::abs(ddy)) cands.push_back({{sx, sy}, {ex, ey}});
    else {
      cands.push_back({{sx, sy}, {ex - sgn(ddx) * mm, ey - sgn(ddy) * mm}, {ex, ey}});
      cands.push_back({{sx, sy}, {sx + sgn(ddx) * mm, sy + sgn(ddy) * mm}, {ex, ey}});
    }
  } else {
    // (fixer M7b r2, review: "kerbed islands and a T-shaped kerb stub in the middle of the avenue") an avenue whose gate
    // stands one row off the heart's row jogs onto it (a 45-degree step) instead of running on one row beside the
    // avenues that do arrive on the heart's row (two bands a tile apart: a doubled drain and walks cut into islands)
    const int offA = dx == 0 ? ex - sx : ey - sy;
    if (offA == 0) cands.push_back({{sx, sy}, dx == 0 ? std::make_pair(sx, ey) : std::make_pair(ex, sy)});
    else if (std::abs(offA) == 1) {
      const int run = dx == 0 ? ey - sy : ex - sx;
      if (std::abs(run) >= 8) {
        const int sr = sgn(run), at = 4;
        if (dx == 0) cands.push_back({{sx, sy}, {sx, sy + sr * at}, {ex, sy + sr * (at + 1)}, {ex, ey}});
        else cands.push_back({{sx, sy}, {sx + sr * at, sy}, {sx + sr * (at + 1), ey}, {ex, ey}});
      }
    }
    cands.push_back(chamfer({{sx, sy}, {s3x, s3y}, {dx == 0 ? s3x : ex, dx == 0 ? ey : s3y}, {ex, ey}}));   // own line, then the axis
    cands.push_back(chamfer({{sx, sy}, {s3x, s3y}, {dx == 0 ? ex : s3x, dx == 0 ? s3y : ey}, {ex, ey}}));   // across first
    // a jog: along its line, a 45-degree shift onto the heart's line, on along it
    const int off = dx == 0 ? ex - sx : ey - sy, run = dx == 0 ? ey - sy : ex - sx;
    if (std::abs(off) >= 2 && std::abs(off) <= 14 && std::abs(run) >= std::abs(off) + 10) {
      const int sr = sgn(run), at = (std::abs(run) - std::abs(off)) / 2;
      if (dx == 0) cands.push_back({{sx, sy}, {sx, sy + sr * at}, {ex, sy + sr * (at + std::abs(off))}, {ex, ey}});
      else cands.push_back({{sx, sy}, {sx + sr * at, sy}, {sx + sr * (at + std::abs(off)), ey}, {ex, ey}});
    }
  }
  int best = -1;
  float bestS = 1e30f;
  for (size_t c = 0; c < cands.size(); c++) {
    const Pts& p = cands[c];
    float sc = 0;
    for (size_t s = 0; s + 1 < p.size(); s++) {
      const int ax = p[s].first, ay = p[s].second, bx = p[s + 1].first, by = p[s + 1].second;
      const int len = std::max(std::abs(bx - ax), std::abs(by - ay));
      sc += (float)len * 0.05f + 1.0f;
      for (int t = 0; t <= len; t++) {
        const int x = len ? ax + (bx - ax) * t / len : ax, y = len ? ay + (by - ay) * t / len : ay;
        // (fixer M7b r2) never one or two tiles beside an avenue already laid, running along it (a doubled band)
        if (!plan.empty() && in(x, y) && plan[I(x, y)] != (uint8_t)Urb::Gutter && (ax == bx || ay == by)) {
          const int qx = ay == by ? 0 : 1, qy = ay == by ? 1 : 0;
          for (int k : {-2, -1, 1, 2})
            if (in(x + qx * k, y + qy * k) && plan[I(x + qx * k, y + qy * k)] == (uint8_t)Urb::Gutter) { sc += 3.0f; break; }
        }
        for (int oy = -2; oy <= 2; oy++)
          for (int ox = -2; ox <= 2; ox++) {
            const int qx = x + ox, qy = y + oy;
            if (!in(qx, qy)) { sc += 20; continue; }
            const size_t i = I(qx, qy);
            if (inCompound(qx, qy, 1)) sc += 60;
            else if (!keep.empty() && keep[i]) sc += 40;
            else if (mask[i] == K_PARK) sc += 40;
            else if (walled && !ins(qx, qy)) sc += 30;
            else if (water[i]) sc += 1.5f;
            else if (terraced && lvl[i] != lvl[I(x, y)]) sc += 0.3f;
          }
      }
    }
    if (sc < bestS) { bestS = sc; best = (int)c; }
  }
  if (best < 0) return false;
  if (sylvan()) {
    // (the elven city: its avenues wind a little between the trees) each long leg bowed by a gentle offset
    Pts p = cands[(size_t)best], q;
    q.push_back(p[0]);
    for (size_t s = 0; s + 1 < p.size(); s++) {
      const int ax = p[s].first, ay = p[s].second, bx = p[s + 1].first, by = p[s + 1].second;
      const int len = std::max(std::abs(bx - ax), std::abs(by - ay));
      if (len >= 16 && (ax == bx || ay == by)) {
        const int nx = ay == by ? 0 : 1, ny = ay == by ? 1 : 0;
        const int bow = (int)(hashAt(ax, ay, 0xB0Bu) % 3u) - 1;
        const int amp = bow * (2 + (int)(hashAt(bx, by, 0xB0Cu) % 2u));
        q.push_back({ax + (bx - ax) / 3 + nx * amp, ay + (by - ay) / 3 + ny * amp});
        q.push_back({ax + 2 * (bx - ax) / 3 + nx * amp, ay + 2 * (by - ay) / 3 + ny * amp});
      }
      q.push_back(p[s + 1]);
    }
    paintBand(q, hw, (uint8_t)Urb::Avenue, K_MAIN, mainG);
  } else paintBand(cands[(size_t)best], hw, (uint8_t)Urb::Avenue, K_MAIN, mainG);
  return true;
}

// A precinct for the seat (role 1) or the chief temple (role 3), a bw x bh building: its square (the building's door
// on the square's north edge) and the plot behind it, near the angle a0 at 0.38 .. 0.62 of the way out, on dry level
// land of the town clear of the compound, the heart and every street. The plot is kept (keep) and a lane runs round
// its west, north and east sides; the square joins the squares. (qx, qy): the square's top middle.
bool Gen::planPrecinct(int role, int bw, int bh, float a0, int& qx, int& qy) {
  const int sqW = std::max(bw + 6, role == 1 ? 13 : 11), sqH = role == 1 ? 6 : 5;
  const int pw = bw + 4, ph = bh + 5;
  static const float offs[9] = {0.0f, 0.22f, -0.22f, 0.44f, -0.44f, 0.66f, -0.66f, 0.88f, -0.88f};
  for (int k = 0; k < 9; k++) {
    const float a = a0 + offs[k];
    for (float d = 0.40f; d <= 0.62f; d += 0.03f) {
      int x = cx + irn(dcos(a) * rx * d), y = cy + irn(dsin(a) * ry * d);
      if (cultureIs((int)cult::Archetype::Jade) && role == 1 && k == 0) x = cx;   // (the jade city's axis)
      const int px0 = x - pw / 2, py0 = y - ph, sx0 = x - sqW / 2;
      // (on terraces the building's front rows and its square share one terrace; its back may climb the face behind)
      const int mg = terraced ? 1 : 3;
      const int bx0 = std::min(px0, sx0) - mg, bx1 = std::max(px0 + pw, sx0 + sqW) + mg, by0 = py0 - mg, by1 = y + sqH + mg;
      if (!in(bx0, by0) || !in(bx1, by1)) continue;
      const int lv = lvl[I(x, y)];
      bool ok = true;
      for (int yy = by0; yy <= by1 && ok; yy++)
        for (int xx = bx0; xx <= bx1; xx++) {
          const size_t i = I(xx, yy);
          const bool level = !terraced || yy >= y - bh - 2;
          if (water[i] || mask[i] != K_NONE || inCompound(xx, yy, 3) || keep[i] || !roomy[i] || groundSolid(M.at(xx, yy)) || (level && lvl[i] != lv)) { ok = false; break; }
        }
      if (!ok) continue;
      // clear of the heart's square and market
      if (dist(x, y) < 0.33f || (mktZone.w > 0 && IRect{mktZone.x - 4, mktZone.y - 4, mktZone.w + 8, mktZone.h + 8}.overlaps(IRect{bx0, by0, bx1 - bx0 + 1, by1 - by0 + 1}))) continue;
      // the square
      const District dd = districtAt(x, y + sqH / 2);
      for (int yy = y; yy < y + sqH; yy++)
        for (int xx = sx0; xx < sx0 + sqW; xx++) {
          if ((xx == sx0 || xx == sx0 + sqW - 1) && (yy == y + sqH - 1)) continue;   // its south corners rounded
          set(xx, yy, K_SQUARE);
          M.setG(xx, yy, Ground::Plaza);
          M.setP(xx, yy, 0);
          plan[I(xx, yy)] = (uint8_t)(role == 3 ? Urb::Close : Urb::Square);
        }
      squares.push_back(Square{x, y + sqH / 2, sqH * 0.5f, dd, (uint8_t)role});
      // the plot, kept; the lane round it
      for (int yy = py0; yy < y; yy++)
        for (int xx = px0; xx < px0 + pw; xx++) keep[I(xx, yy)] = 1;
      paintBand({{px0 - 1, y - 1}, {px0 - 1, py0 - 1}, {px0 + pw, py0 - 1}, {px0 + pw, y - 1}}, 0.5f, (uint8_t)Urb::Lane, K_LANE, laneG == Ground::Dirt ? Ground::Road : laneG);
      precincts.push_back(Precinct{IRect{px0, py0, pw, ph}, IRect{sx0, y, sqW, sqH}, role, 0});
      qx = x; qy = y;
      return true;
    }
  }
  return false;
}

// A lane from a piece of the plan (its tiles: from) over open ground to the nearest street that is not part of it (up
// to 40 tiles), so no square, precinct or park stands cut off; nothing when it already touches one
void Gen::joinStreets(const std::vector<int>& from) {
  if (from.empty()) return;
  std::vector<uint8_t> own((size_t)W * H, 0);
  for (int c : from) own[(size_t)c] = 1;
  std::vector<int> prev((size_t)W * H, -2), dep((size_t)W * H, 0), q;
  for (int c : from) { prev[(size_t)c] = -1; q.push_back(c); }
  int found = -1;
  for (size_t h = 0; h < q.size() && found < 0; h++) {
    const int c = q[h], x = c % W, y = c / W;
    if (dep[(size_t)c] > 40) continue;
    for (int d = 0; d < 4; d++) {
      const int nx = x + D4X[d], ny = y + D4Y[d];
      if (!in(nx, ny) || nx < 1 || ny < 1 || nx >= W - 1 || ny >= H - 1 || prev[I(nx, ny)] != -2) continue;
      const size_t j = I(nx, ny);
      if (isStreet(nx, ny) && !own[j]) {
        if (own[(size_t)c]) return;   // it already touches a street of its own
        prev[j] = c; found = (int)j; break;
      }
      if (own[j] || mask[j] != K_NONE || keep[j] || water[j] || groundSolid(M.at(nx, ny)) || inCompound(nx, ny, 1) || (walled && (!inside[j] || nearRing(nx, ny, 2)))) continue;
      prev[j] = c;
      dep[j] = dep[(size_t)c] + 1;
      q.push_back((int)j);
    }
  }
  if (found < 0 || prev[(size_t)found] < 0) return;
  for (int c = prev[(size_t)found]; c >= 0 && prev[(size_t)c] != -1; c = prev[(size_t)c]) {
    const int x = c % W, y = c / W;
    paintStreet(x, y, K_LANE, laneG == Ground::Dirt ? Ground::Road : laneG);
    if (get(x, y) == K_LANE && urbRank(plan[(size_t)c]) < urbRank((uint8_t)Urb::Lane)) plan[(size_t)c] = (uint8_t)Urb::Lane;
  }
}

// the rows a building's sprite rises over (behind its footprint) hold no avenue, drain, walk or cross street: a planned
// city's main ways are never hidden behind a roof (a back lane or an alley may be)
bool Gen::roofOverStreet(const IRect& r, int rise) const {
  if (plan.empty()) return false;
  for (int y = r.y - rise; y < r.y; y++)
    for (int x = r.x; x < r.x + r.w; x++) {
      // (fixer M7b r2, review: "a stream ends against the roof of a house") nor a stream: a river never runs on behind
      // a roof as if it ended in it
      if (in(x, y) && water[I(x, y)] && y >= r.y - std::min(rise, 3)) return true;
      if (!in(x, y) || !isStreet(x, y)) continue;
      const Urb u = (Urb)plan[I(x, y)];
      if (u == Urb::Avenue || u == Urb::Gutter || u == Urb::Sidewalk || u == Urb::Street) return true;
    }
  return false;
}

// ------------------------------------------------------------------------------------------------ phase 2: the avenues
void Gen::cityStreets() {
  const size_t n = (size_t)W * H;
  plan.assign(n, 0);
  keep.assign(n, 0);
  // the town's roomy ground: inside the ring and five tiles clear of it (precincts and the park keep off the wall)
  roomy.assign(n, 0);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) roomy[I(x, y)] = (!walled || (inside[I(x, y)] && !nearRing(x, y, 5))) ? 1 : 0;
  avenueHW = (sylvan() || stilt) ? 1.5f : 2.5f;
  blockP = densityF > 1.2f ? 11 : (densityF < 0.8f ? 13 : 12);
  cityPick();   // (M7c: the city's kind of plan and its block pitches; town_city_grid.cpp)
  cityMade.assign(n, 0);
  if (sylvan()) laneG = Ground::Dirt;   // (the elven city: little paving off its avenues)
  // --- the seat: a capital's palace square before the compound gate; any other city's seat precinct in the noble
  // quarter (a radial city's seat faces its plaza, a merchant republic's its market square)
  int seatX = -1, seatY = -1;
  if (compound.w > 0) {
    const int gx = compound.x + compound.w / 2 - 1, gy = compound.y + compound.h - 1;
    const int sx0 = gx + 1 - 6;
    for (int yy = gy + 1; yy <= gy + 6; yy++)
      for (int xx = sx0; xx < sx0 + 13; xx++) {
        if (!in(xx, yy) || water[I(xx, yy)] || (walled && !ins(xx, yy)) || inCompound(xx, yy) || nearRing(xx, yy, 2)) continue;
        if ((xx == sx0 || xx == sx0 + 12) && yy == gy + 6) continue;
        set(xx, yy, K_SQUARE);
        M.setG(xx, yy, Ground::Plaza);
        M.setP(xx, yy, 0);
        plan[I(xx, yy)] = (uint8_t)Urb::Square;
      }
    squares.push_back(Square{gx + 1, gy + 4, 3.0f, District::Noble, 1});
    precincts.push_back(Precinct{compound, IRect{sx0, gy + 1, 13, 6}, 1, 0});
    seatX = gx + 1; seatY = gy + 7;
    // (M7c) a lane round the palace grounds' walls, two tiles off them (the blocks round the compound meet a street, not
    // its blank wall), joined to the palace square on either side
    {
      const int x0 = compound.x - 2, x1 = compound.x + compound.w + 1, y0 = compound.y - 2, y1 = compound.y + compound.h + 1;
      const Ground lg = laneG == Ground::Dirt ? Ground::Road : laneG;
      paintBand({{x0, y1}, {x0, y0}, {x1, y0}, {x1, y1}}, 0.5f, (uint8_t)Urb::Lane, K_LANE, lg);
      for (int x = x0; x <= x1; x++) {   // (the south side: from each corner along to the square)
        if (in(x, y1) && get(x, y1) == K_SQUARE) break;
        paintBand({{x0, y1}, {x, y1}}, 0.5f, (uint8_t)Urb::Lane, K_LANE, lg);
      }
      for (int x = x1; x >= x0; x--) {
        if (in(x, y1) && get(x, y1) == K_SQUARE) break;
        paintBand({{x1, y1}, {x, y1}}, 0.5f, (uint8_t)Urb::Lane, K_LANE, lg);
      }
    }
  } else if (style != cult::Layout::Radial && !(hasSoc && soc.seat == cult::Seat::GuildExchange)) {
    int bw = 9, bh = 4;
    if (hasSoc)
      for (const cult::BuildingReq& q : cult::requiredBuildings(soc, *C.culture, tier, (int)P.archetype, P.seed))
        if (q.civic & bld::CIVIC_SEAT) { bw = q.wTiles ? q.wTiles : 4; bh = q.hTiles ? q.hTiles : 3; break; }
    int qx = 0, qy = 0;
    if (planPrecinct(1, bw, bh, cultureIs((int)cult::Archetype::Jade) ? -D_PI / 2 : sector0, qx, qy)) { seatX = qx; seatY = qy + 6; }
  }
  // --- the chief temple's close in the temple quarter
  {
    int bw = 7, bh = 5;
    if (hasSoc)
      for (const cult::BuildingReq& q : cult::requiredBuildings(soc, *C.culture, tier, (int)P.archetype, P.seed))
        if (q.purpose == Building::Temple) { bw = q.wTiles ? q.wTiles : 4; bh = q.hTiles ? q.hTiles : 3; break; }
    int k = 1;
    for (int s = 0; s < 4; s++) if (sectorKind[s] == District::Temple) k = s;
    int qx = 0, qy = 0;
    if (!(capital && seatKind == (int)cult::Seat::TempleComplex)) planPrecinct(3, bw, bh, sector0 + k * (D_PI / 2), qx, qy);
  }
  // --- the gates: for every road, the opening cityWall will make (the nearest straight run of the ring within 11 tiles
  // of where the road's bearing crosses it and within 23 degrees of it, a horizontal run preferred; else a breach)
  gatePlan.clear();
  if (!ringT.empty()) {
    auto R = [&](int x, int y) { return in(x, y) && ringT[I(x, y)] != 0; };
    auto wetNear = [&](int x, int y) {
      for (int oy = -3; oy <= 3; oy++)
        for (int ox = -3; ox <= 3; ox++) {
          if (in(x + ox, y + oy)) {
            if (water[I(x + ox, y + oy)] || M.at(x + ox, y + oy) == Ground::Bridge) return true;
          } else if (C.base) {
            Ground g = Ground::Grass;
            Biome b = Biome::Plains;
            uint8_t h = 0;
            C.base(O.gx + x + ox, O.gy + y + oy, g, b, h);
            if (groundWater(g) || g == Ground::Bridge) return true;
          }
        }
      return false;
    };
    auto sH = [&](int x, int y) {
      for (int k = -2; k <= 2; k++) if (!R(x + k, y)) return false;
      for (int k = -1; k <= 1; k++) if (R(x + k, y - 1) || R(x + k, y + 1)) return false;
      return !wetNear(x, y);
    };
    auto sV = [&](int x, int y) {
      for (int k = -2; k <= 2; k++) if (!R(x, y + k)) return false;
      for (int k = -1; k <= 1; k++) if (R(x - 1, y + k) || R(x + 1, y + k)) return false;
      return !wetNear(x, y);
    };
    std::vector<std::pair<int, int>> ringTiles;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (R(x, y)) ringTiles.push_back({x, y});
    for (int b = 0; b < (int)bearings.size(); b++) {
      const float a = bearings[(size_t)b];
      // where the bearing crosses the ring
      int px = -1, py = -1;
      for (float t = 1; t < (float)(W + H) && px < 0; t += 0.5f) {
        const int x = ifl(cx + 0.5f + dcos(a) * t), y = ifl(cy + 0.5f + dsin(a) * t);
        if (!in(x, y)) break;
        if (R(x, y)) { px = x; py = y; }
      }
      if (px < 0) continue;
      auto nearPlanned = [&](int x, int y, int d) {
        for (const GatePlan& g : gatePlan) if (std::abs(g.x - x) <= d && std::abs(g.y - y) <= d) return true;
        return false;
      };
      int bx = -1, by = -1, bd = 1 << 30;
      bool bH = true;
      for (int oy = -11; oy <= 11; oy++)
        for (int ox = -11; ox <= 11; ox++) {
          const int x = px + ox, y = py + oy;
          if (!R(x, y) || nearPlanned(x, y, 8)) continue;
          if (std::fabs(dwrap(datan2((float)(y - cy), (float)(x - cx)) - a)) > 0.40f) continue;
          const bool h = sH(x, y), v = !h && sV(x, y);
          if (!h && !v) continue;
          const int d = ox * ox + oy * oy + (h ? 0 : 30);
          if (d < bd) { bd = d; bx = x; by = y; bH = h; }
        }
      if (bx < 0) {
        // a breach where the bearing crosses (a stepped run, a river at the foot of the wall)
        if (nearPlanned(px, py, 8)) continue;
        bx = px; by = py; bH = std::fabs(dsin(a)) > std::fabs(dcos(a));
      }
      GatePlan g{bx, by, 0, 0, b, bH};
      if (bH) g.dy = (in(bx, by + 2) && inside[I(bx, by + 2)] && !R(bx, by + 2)) ? 1 : -1;
      else g.dx = (in(bx + 2, by) && inside[I(bx + 2, by)] && !R(bx + 2, by)) ? 1 : -1;
      gatePlan.push_back(g);
    }
  }
  // --- every gate's throat (three wide through the ring) and its avenue to the market square
  for (const GatePlan& g : gatePlan) {
    const int tx = g.x + g.dx * 4, ty = g.y + g.dy * 4;
    paintBand({{g.x - g.dx, g.y - g.dy}, {tx, ty}}, 1.5f, (uint8_t)Urb::Avenue, K_MAIN, mainG);
    routeAvenue(tx, ty, g.dx, g.dy, cx, cy, avenueHW);
  }
  // --- the way from the market to the seat
  if (seatX >= 0) routeAvenue(seatX, seatY, 0, 1, cx, cy, avenueHW);
  // --- the park (planned before the lattice, so the lanes keep out of it): a block of the roomy ground of the rich or
  // the temple quarter (any quarter when they have none), clear of the streets, the squares and the market
  {
    const int SW = W + 1;
    std::vector<int> hard((size_t)SW * (H + 1), 0), soft((size_t)SW * (H + 1), 0);
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        const size_t i = I(x, y);
        // (a marsh city drains its green out of the reeds and pools)
        const bool marsh = stilt && (M.at(x, y) == Ground::Water || M.at(x, y) == Ground::Swamp);
        const bool h = water[i] || inCompound(x, y, 2) || keep[i] || !roomy[i] || (groundSolid(M.at(x, y)) && !marsh) || mask[i] == K_SQUARE;
        const bool f = mask[i] != K_NONE;
        hard[(size_t)(y + 1) * SW + x + 1] = (h ? 1 : 0) + hard[(size_t)y * SW + x + 1] + hard[(size_t)(y + 1) * SW + x] - hard[(size_t)y * SW + x];
        soft[(size_t)(y + 1) * SW + x + 1] = (f ? 1 : 0) + soft[(size_t)y * SW + x + 1] + soft[(size_t)(y + 1) * SW + x] - soft[(size_t)y * SW + x];
      }
    auto area = [&](const std::vector<int>& t, int x0, int y0, int x1, int y1) {   // [x0, x1) x [y0, y1)
      return t[(size_t)y1 * SW + x1] - t[(size_t)y0 * SW + x1] - t[(size_t)y1 * SW + x0] + t[(size_t)y0 * SW + x0];
    };
    const int pw0 = 22 + (int)(hashAt(1, 2, 0x9A3Bu) % 7u), ph0 = 14 + (int)(hashAt(3, 4, 0x9A3Cu) % 4u);
    bool done = false;
    for (int pass = 0; pass < 4 && !done; pass++) {
      const int pw = pass < 2 ? pw0 : (pass == 2 ? 15 : 11), ph = pass < 2 ? ph0 : (pass == 2 ? 11 : 8);
      std::vector<std::pair<uint32_t, int>> cands;
      for (int y = 3; y + ph + 3 < H; y++)
        for (int x = 3; x + pw + 3 < W; x++) {
          const int mx = x + pw / 2, my = y + ph / 2;
          const float d = dist(mx, my);
          if (d < 0.30f || d > 0.74f) continue;
          const District dd = districtAt(mx, my);
          if (pass == 0 && dd != District::Noble && dd != District::Temple) continue;
          if (area(hard, x - 2, y - 2, x + pw + 2, y + ph + 2) || area(soft, x, y, x + pw, y + ph)) continue;
          if (mktZone.w > 0 && IRect{mktZone.x - 3, mktZone.y - 3, mktZone.w + 6, mktZone.h + 6}.overlaps(IRect{x, y, pw, ph})) continue;
          cands.push_back({hashAt(x, y, 0x9A3Du), y * W + x});
        }
      std::sort(cands.begin(), cands.end());
      for (const auto& cd : cands) {
        const int x0 = cd.second % W, y0 = cd.second / W;
        const int lv = lvl[I(x0 + pw / 2, y0 + ph / 2)];
        bool flat = true;
        for (int yy = y0 - 1; yy <= y0 + ph && flat; yy++)
          for (int xx = x0 - 1; xx <= x0 + pw; xx++) if (lvl[I(xx, yy)] != lv) { flat = false; break; }
        if (!flat) continue;
        park = IRect{x0, y0, pw, ph};
        for (int yy = y0; yy < y0 + ph; yy++)
          for (int xx = x0; xx < x0 + pw; xx++) {
            set(xx, yy, K_PARK);
            keep[I(xx, yy)] = 1;
            M.setP(xx, yy, 0);
            M.setG(xx, yy, base == Ground::Grass ? Ground::Meadow : base);
            plan[I(xx, yy)] = (uint8_t)Urb::Park;
          }
        // the lane round it, joined to the streets
        paintBand({{x0 - 1, y0 - 1}, {x0 + pw, y0 - 1}, {x0 + pw, y0 + ph}, {x0 - 1, y0 + ph}, {x0 - 1, y0 - 1}}, 0.5f, (uint8_t)Urb::Lane, K_LANE,
                  laneG == Ground::Dirt ? Ground::Road : laneG);
        std::vector<int> ring;
        for (int xx = x0 - 1; xx <= x0 + pw; xx++) { ring.push_back((int)I(xx, y0 - 1)); ring.push_back((int)I(xx, y0 + ph)); }
        for (int yy = y0; yy < y0 + ph; yy++) { ring.push_back((int)I(x0 - 1, yy)); ring.push_back((int)I(x0 + pw, yy)); }
        joinStreets(ring);
        done = true;
        break;
      }
    }
  }
  // the precincts' squares and lanes joined to the streets too (the lattice will meet them as well)
  for (const Precinct& pc : precincts) {
    if (pc.role != 3 && compound.w > 0) continue;
    std::vector<int> from;
    for (int yy = pc.sq.y; yy < pc.sq.y + pc.sq.h; yy++)
      for (int xx = pc.sq.x; xx < pc.sq.x + pc.sq.w; xx++) if (in(xx, yy) && isStreet(xx, yy)) from.push_back((int)I(xx, yy));
    for (int yy = pc.plot.y - 1; yy < pc.sq.y; yy++)
      for (int xx : {pc.plot.x - 1, pc.plot.x + pc.plot.w}) if (in(xx, yy) && isStreet(xx, yy)) from.push_back((int)I(xx, yy));
    for (int xx = pc.plot.x - 1; xx <= pc.plot.x + pc.plot.w; xx++) if (in(xx, pc.plot.y - 1) && isStreet(xx, pc.plot.y - 1)) from.push_back((int)I(xx, pc.plot.y - 1));
    joinStreets(from);
  }
  // --- (M7c) the plan's own frame before the lattice (cityBlocks): the rings, the spine along the water, the lane round
  // the inside of the wall, a lane along every bank (town_city_grid.cpp)
  if (cp.kind == CK_RINGS) cityRingStreets();
  if (cp.kind == CK_HARBOUR || cp.kind == CK_RIVER) citySpine();
  cityWallLane();
  cityBankStreets();
  bridgeCrossings();
}

// (fixer M7b r2, review: "the avenue runs over the canal as a plain band of flagstones, the two halves of the canal a tile
// apart") where an avenue crosses water on a slant (a stream stepping under it), its deck was a staircase of bridge
// tiles with paving between: the crossing becomes one deck, the band's whole width over every row (or column) the water
// takes within it, so the bridge spans the stream in one piece with its parapets over the water on either side
void Gen::bridgeCrossings() {
  if (plan.empty()) return;
  auto band = [&](int x, int y) {
    if (!in(x, y)) return false;
    const Urb u = (Urb)plan[I(x, y)];
    return (u == Urb::Avenue || u == Urb::Gutter || u == Urb::Sidewalk) && get(x, y) == K_MAIN;
  };
  auto wet = [&](int x, int y) { return in(x, y) && (water[I(x, y)] || M.at(x, y) == Ground::Bridge); };
  std::vector<uint8_t> done((size_t)W * H, 0);
  for (int y = 2; y < H - 2; y++)
    for (int x = 2; x < W - 2; x++) {
      if (!band(x, y) || !wet(x, y) || done[I(x, y)]) continue;
      // the band's way: its cross-section is the shorter run of band tiles through this one
      int xa = x, xb = x, ya = y, yb = y;
      while (band(xa - 1, y) && x - xa < 8) xa--;
      while (band(xb + 1, y) && xb - x < 8) xb++;
      while (band(x, ya - 1) && y - ya < 8) ya--;
      while (band(x, yb + 1) && yb - y < 8) yb++;
      const bool ns = (xb - xa) < (yb - ya);   // the band runs north-south: the deck spans rows
      if ((ns ? xb - xa : yb - ya) < 2) continue;
      // the rows (columns) the water takes inside the cross-section, within a few tiles
      int lo = 1 << 20, hi = -1;
      for (int k = -5; k <= 5; k++)
        for (int t = ns ? xa : ya; t <= (ns ? xb : yb); t++) {
          const int qx = ns ? t : x + k, qy = ns ? y + k : t;
          if (wet(qx, qy) && band(qx, qy)) { lo = std::min(lo, k); hi = std::max(hi, k); }
        }
      if (hi < lo || hi - lo > 6) continue;
      for (int k = lo; k <= hi; k++)
        for (int t = ns ? xa : ya; t <= (ns ? xb : yb); t++) {
          const int qx = ns ? t : x + k, qy = ns ? y + k : t;
          if (!in(qx, qy)) continue;
          done[I(qx, qy)] = 1;
          if (!band(qx, qy) && !wet(qx, qy)) continue;
          M.setG(qx, qy, Ground::Bridge);
          M.setP(qx, qy, 0);
        }
    }
}

// services(): the seat (a city's lord; a capital's seat is its compound's) faces its precinct's square, the chief
// temple its close: their doors on the square's north edge (faceSquare), the plot behind kept for them
void Gen::facePrecincts(std::vector<Want>& want) {
  for (Precinct& pc : precincts) {
    if (pc.want || (pc.role == 1 && compound.w > 0)) continue;
    for (Want& w : want) {
      const bool seat = pc.role == 1 && ((w.civic & bld::CIVIC_SEAT) || (!hasSoc && w.b == Building::Keep));
      const bool temple = pc.role == 3 && w.b == Building::Temple;
      if ((!seat && !temple) || w.face) continue;
      w.px = pc.sq.x + pc.sq.w / 2;
      w.py = pc.sq.y;
      w.face = true;
      w.north = false;
      w.d = District::COUNT;
      w.w = std::min(w.w, pc.plot.w - 2);
      w.h = std::min(w.h, pc.plot.h - 3);
      pc.want = 1;
      break;
    }
  }
}

// ------------------------------------------------------------------------------------------------ phase 3: the blocks
void Gen::cityBlocks() {
  if (cp.kind != CK_LEGACY) {
    // (M7c) the street network: every section's block streets and lanes, the role blocks (town_city_grid.cpp); then the
    // elves' great trees and the well-squares at the lanes' feet as before
    cityGrid();
    cityRiverLinks();   // (M7c fixer: a far bank cut off by the river gets its bridge)
    cityTrees();
    cityWells(cityFeet);
    return;
  }
  if (style == cult::Layout::Terraced) { contourLanes(); }
  // the grid (imperial, and others' alternate): straight north-south lanes a block apart first, then its cross streets
  // with their walks between them; the organic plan: cross streets off the avenues, its lanes through the blocks after
  const bool grid = style == cult::Layout::Grid;
  const bool lattice = (style == cult::Layout::Organic && !sylvan()) || grid;
  std::vector<std::pair<int, int>> laneFoot;   // where a block lane meets the street south of it (well-squares)
  if (lattice) {
    const int Pp = grid ? std::max(11, blockP) : blockP;
    if (grid) {
      const int SX = gridSX;
      for (int kx = -(W / SX) - 1; kx <= W / SX + 1; kx++) {
        const int x = cx + kx * SX + SX / 2;
        if (x < 2 || x >= W - 2) continue;
        int y0 = -1;
        for (int y = 1; y <= H - 1; y++) {
          const bool ok = y < H - 1 && laneOk(x, y, true) && (!walled || ins(x, y));
          if (ok && y0 < 0) y0 = y;
          if (!ok && y0 >= 0) {
            if (y - y0 >= 4) paintBand({{x, y0}, {x, y - 1}}, 0.5f, (uint8_t)Urb::Lane, K_LANE, laneG);
            y0 = -1;
          }
        }
      }
    }
    struct Seg { int y, x0, x1; };   // a stretch of cross street: the row its lanes leave from, its ends
    std::vector<Seg> segs;
    auto quarterAt = [&](int x, int y) { return districtAt(x, y); };
    auto street = [&](int x, int y) { return isStreet(x, y); };
    // a tile of a cross street's band may be painted here
    auto tileOk = [&](int x, int y) {
      if (!bandFree(x, y) || water[I(x, y)]) return false;
      if (walled && (!ins(x, y) || nearRing(x, y, 3))) return false;
      if (terraced && lvl[I(x, y)] != lvl[I(cx, cy)]) return false;
      return true;
    };
    // one row of the lattice: segments of it between two streets it meets, every tile between free; w: 3 (a street
    // with its two walks), 1 (an alley); the rows either side kept clear of parallel streets
    auto rowPass = [&](int y, int width, bool poorOnly, int k) {
      if (y < 4 || y >= H - 4) return;
      const int half = width / 2;
      std::vector<uint8_t> hit((size_t)W, 0), ok((size_t)W, 0);
      for (int x = 1; x < W - 1; x++) {
        hit[(size_t)x] = street(x, y) ? 1 : 0;
        bool cross = false;
        for (int dy = -half; dy <= half; dy++) if (street(x, y + dy)) cross = true;
        bool o = true;
        for (int dy = -half; dy <= half && o; dy++) if (!street(x, y + dy) && !tileOk(x, y + dy)) o = false;
        // never alongside a street running the same way (a street crossing on the slant is met, not run along)
        auto runH = [&](int qy) { return street(x - 1, qy) && street(x, qy) && street(x + 1, qy); };
        if (o && !cross && (runH(y - half - 1) || runH(y + half + 1))) o = false;
        ok[(size_t)x] = o ? 1 : 0;
      }
      // a stretch [a, b] (each end a street, or a tile touching one that runs alongside), every tile between free
      auto emit = [&](int a0, int b0) {
        const int mx = (a0 + b0) / 2;
        const District q = quarterAt(mx, y);
        if (poorOnly && q != District::Poor) return;
        // the rich quarter's blocks run deep (walled gardens): its odd rows mostly left out
        if (!poorOnly && q == District::Noble && (k & 1) && hfAt(a0, y, 0x51C4u) < 0.5f) return;
        const bool alley = poorOnly || q == District::Poor;
        // the planned part (near the heart): a straight street with its walks; the outskirts: lanes wandering on from
        // its ends to the next street (the city's organic edge)
        int i0 = -1, i1 = -1;
        if (!alley && width == 3)
          for (int t = a0 + 1; t < b0; t++) if (grid || dist(t, y) < 0.64f) { if (i0 < 0) i0 = t; i1 = t; }
        if (i0 >= 0 && i1 - i0 >= 6) {
          paintBand({{i0 == a0 + 1 ? a0 : i0, y}, {i1 == b0 - 1 ? b0 : i1, y}}, 1.5f, (uint8_t)Urb::Street, K_LANE, Ground::Road);
          segs.push_back(Seg{y - 2, i0, i1});
          // (a wandering lane that finds no street: the way runs on straight instead, so the street never stops short)
          if (i0 > a0 + 1 && !walkStreet(i0 - 0.5f, y + 0.5f, D_PI, D_PI, i0 - a0 + 8, 1, K_LANE, laneG, 2))
            paintBand({{a0, y}, {i0, y}}, 0.5f, (uint8_t)Urb::Lane, K_LANE, laneG);
          if (i1 < b0 - 1 && !walkStreet(i1 + 1.5f, y + 0.5f, 0.0f, 0.0f, b0 - i1 + 8, 1, K_LANE, laneG, 2))
            paintBand({{i1, y}, {b0, y}}, 0.5f, (uint8_t)Urb::Lane, K_LANE, laneG);
        } else if (alley || width == 1) {
          paintBand({{a0, y}, {b0, y}}, 0.5f, (uint8_t)(alley ? Urb::Alley : Urb::Lane), K_LANE, alley ? Ground::Dirt : laneG);
          segs.push_back(Seg{y - 1, a0, b0});
        } else {
          if (!walkStreet(a0 + 0.5f, y + 0.5f, 0.0f, 0.0f, b0 - a0 + 8, 1, K_LANE, laneG, 2) && hfAt(a0, y, 0x0E70u) < 0.5f)
            paintBand({{a0, y}, {b0, y}}, 0.5f, (uint8_t)Urb::Lane, K_LANE, laneG);
        }
      };
      // where the row meets a street running alongside it (the wall street's slow curve, a lane's last stretch), it
      // ends against that street (its walk touches it) or starts off it
      // (a street also runs down to the water: it ends at the bank, where the quays are laid)
      std::vector<uint8_t> along((size_t)W, 0);
      for (int x = 1; x < W - 1; x++) {
        if (hit[(size_t)x] || ok[(size_t)x]) continue;
        bool cross = false, wet = false;
        for (int dy = -half; dy <= half; dy++) {
          if (street(x, y + dy)) cross = true;
          if (in(x, y + dy) && water[I(x, y + dy)]) wet = true;
        }
        auto runH = [&](int qy) { return street(x - 1, qy) && street(x, qy) && street(x + 1, qy); };
        along[(size_t)x] = (!cross && (runH(y - half - 1) || runH(y + half + 1))) || (wet && !stilt) ? 1 : 0;
      }
      int last = -1;
      bool fresh = false;   // the stretch begins off a street alongside
      for (int x = 1; x < W - 1; x++) {
        if (hit[(size_t)x]) {
          if (last >= 0 && x - last >= 7) emit(last, x);
          last = x; fresh = false;
          continue;
        }
        if (!ok[(size_t)x]) {
          if (along[(size_t)x] && last >= 0 && x - 1 - last >= 7) emit(last, x - 1);
          last = -1;
          fresh = along[(size_t)x] != 0;
          continue;
        }
        if (fresh && last < 0) { last = x; fresh = false; }
      }
    };
    const int k0 = -(H / Pp) - 1, k1 = H / Pp + 1;
    // the main rows nearest the heart first (they set the junctions the outer rows meet)
    std::vector<int> ks;
    for (int k = k0; k <= k1; k++) ks.push_back(k);
    std::stable_sort(ks.begin(), ks.end(), [](int a, int b) { return std::abs(a) < std::abs(b); });
    for (int k : ks) rowPass(cy + k * Pp, 3, false, k);
    // the poor quarter's alleys between its streets
    for (int k : ks) rowPass(cy + k * Pp + Pp / 2, 1, true, k);
    // the lanes north through the blocks above every stretch, a block-length apart (offset row to row), from the walk
    // up to the next street, never beside another lane
    for (const Seg& s : segs) {
      if (grid) {   // (the grid has its lanes: their feet on this stretch's walk)
        for (int x = s.x0 + 1; x < s.x1; x++)
          if (street(x, s.y - 1) && !street(x - 1, s.y - 1) && !street(x + 1, s.y - 1)) laneFoot.push_back({x, s.y});
        continue;
      }
      int lx = s.x0 + 7 + (int)(hashAt(s.x0, s.y, 0x1A7Eu) % 9u);
      while (lx <= s.x1 - 6) {
        const int y0 = s.y;
        int top = -1;
        for (int t = 0; t <= Pp + 3; t++) {
          const int yy = y0 - t;
          if (!in(lx, yy)) break;
          if (street(lx, yy)) { top = yy; break; }
          auto runV = [&](int qx) { return street(qx, yy - 1) && street(qx, yy) && street(qx, yy + 1); };
          if (!tileOk(lx, yy) || runV(lx - 1) || runV(lx + 1)) break;
        }
        if (top >= 0 && y0 - top >= 3) {
          const bool poorL = quarterAt(lx, y0 - 2) == District::Poor;
          paintBand({{lx, y0}, {lx, top}}, 0.5f, (uint8_t)(poorL ? Urb::Alley : Urb::Lane), K_LANE, poorL ? Ground::Dirt : laneG);
          laneFoot.push_back({lx, y0});
          lx += 17 + (int)(hashAt(lx, s.y, 0x1A7Fu) % 11u);
        } else lx += 3;
      }
    }
  } else if (style == cult::Layout::Organic || style == cult::Layout::Stilt) {
    fabric();   // (the elven and the marsh city: winding lanes between the trees and over the pools)
  }
  // the outskirts inside the wall stay organic: lanes wander off the streets of the outer ring
  {
    const int n = lattice ? 18 : 12;
    for (int k = 0; k < n && !allStreet.empty(); k++) {
      const auto s = allStreet[(size_t)rng.irange((int)allStreet.size())];
      const float a = (float)(hashAt(s.first, s.second, 0x0E6Eu) % 628u) * 0.01f;
      const int len = 8 + (int)(hashAt(s.first, s.second, 0x0E6Fu) % 14u);
      if (M.at(s.first, s.second) == Ground::Bridge || (walled && !ins(s.first, s.second))) continue;
      if (lattice && dist(s.first, s.second) < 0.70f) continue;
      walkStreet(s.first + 0.5f, s.second + 0.5f, a, a, len, 1, K_LANE, laneG, 2);
    }
  }
  cityTrees();
  cityWells(laneFoot);
}

// the elven city: its great trees stand where they grew, between the lanes (no house, roof or crown over another's front:
// the trunk, its crown's reach and a tile round them are kept clear of building)
void Gen::cityTrees() {
  if (sylvan()) {
    for (int y = 6; y < H - 6; y += 9)
      for (int x = 6; x < W - 6; x += 11) {
        const uint32_t h = hashAt(x, y, 0x5E1Au);
        const int tx = x + (int)(h % 5u) - 2, ty = y + (int)((h >> 4) % 5u) - 2;
        if (!in(tx - 2, ty - 4) || !in(tx + 2, ty + 2) || !roomy[I(tx, ty)]) continue;
        bool ok = true;
        for (int oy = -4; oy <= 1 && ok; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            const size_t i = I(tx + ox, ty + oy);
            if (mask[i] != K_NONE || keep[i] || water[i] || M.prop[i] || groundSolid(M.at(tx + ox, ty + oy)) || noBuild[i]) { ok = false; break; }
          }
        if (!ok) continue;
        M.setProp(tx, ty, (h >> 8) % 3u == 0 ? Prop::GiantTree : townTree(bio, eco, h));
        set(tx, ty, K_YARD);
        for (int oy = -4; oy <= 1; oy++)
          for (int ox = -1; ox <= 1; ox++) noBuild[I(tx + ox, ty + oy)] |= 2;
      }
  }
}

// well-squares at some of the block lanes' feet (not in the rich quarter; a dozen tiles apart at least)
void Gen::cityWells(const std::vector<std::pair<int, int>>& laneFoot) {
  {
    std::vector<std::pair<int, int>> made;
    for (const auto& f : laneFoot) {
      if ((int)made.size() >= (capital ? 5 : 4)) break;
      const int lx = f.first, y0 = f.second;
      if (hashAt(lx, y0, 0x3E11u) % 3u) continue;
      if (districtAt(lx, y0) == District::Noble || dist(lx, y0) < 0.25f) continue;
      bool far = true;
      for (const auto& m : made) if (std::abs(m.first - lx) + std::abs(m.second - y0) < 26) far = false;
      if (!far) continue;
      // beside the lane, north of the street: five wide, four deep, east or west of the lane
      for (int side : {1, -1}) {
        const int x0 = side > 0 ? lx + 1 : lx - 5, ytop = y0 - 3;
        bool ok = true;
        for (int yy = ytop - 1; yy <= y0 && ok; yy++)
          for (int xx = x0 - 1; xx <= x0 + 5; xx++) {
            const bool inner = xx >= x0 && xx < x0 + 5 && yy >= ytop;
            if (!in(xx, yy)) { ok = false; break; }
            const size_t i = I(xx, yy);
            if (inner && (mask[i] != K_NONE || keep[i] || water[i] || lvl[i] != lvl[I(lx, y0)] || groundSolid(M.at(xx, yy)))) { ok = false; break; }
            if (!inner && (mask[i] == K_SQUARE || keep[i])) { ok = false; break; }
          }
        if (!ok) continue;
        for (int yy = ytop; yy <= y0; yy++)
          for (int xx = x0; xx < x0 + 5; xx++) {
            if ((xx == x0 || xx == x0 + 4) && yy == ytop) continue;   // its back corners rounded
            set(xx, yy, K_SQUARE);
            M.setG(xx, yy, Ground::Plaza);
            M.setP(xx, yy, 0);
            plan[I(xx, yy)] = (uint8_t)Urb::Square;
          }
        squares.push_back(Square{x0 + 2, ytop + 2, 2.0f, districtAt(x0 + 2, y0), 2});
        made.push_back({lx, y0});
        break;
      }
    }
  }
}

// ------------------------------------------------------------------------------------------------ the row houses
// Terraces along the fronts: every street tile with buildable land north of it is a front; the fronts of a row form
// runs, taken nearest the heart first. Along a run the units are laid side by side on one front row, each a house of
// its quarter's width, depth and storeys (the rich quarter's wide stone houses, the centre's tall narrow ones, the
// craft quarter's workshops, the poor quarter's cottages), every unit's door on the front. Each unit is checked against
// the town as it stood before its terrace (the V5 rules: its footprint free and level, its sprite over nobody's front,
// its front under nobody's sprite), so the only fronts a unit's sprite meets are its own terrace's, across a shared
// wall (Bldg::row: ROW_W / ROW_E). A terrace ends after four to seven units or where a unit cannot stand; a single unit
// is a house of its own. Behind each, a walled back yard (ROW_YARD) as deep as the block allows.
int Gen::rowHouses(int want) {
  if (want <= 0) return 0;
  const bool cottages = cultureIs((int)cult::Archetype::Steppe) || sylvan() || stilt;
  // (tents, the elves' houses among the trees, stilt houses: no terraces) (M7c) but in rows all the same: each its own
  // house a tile from the next along the street's front, no party wall, no walled yard
  const bool detached = cottages;
  struct Run { int y, xa, xb; float d; };
  std::vector<Run> runs;
  auto frontOk = [&](int x, int y) {
    if (!in(x, y - 1) || !isStreet(x, y) || water[I(x, y)] || M.at(x, y) == Ground::Bridge) return false;
    if (M.prop[I(x, y)] && propSolid((Prop)(M.prop[I(x, y)] - 1))) return false;
    const size_t j = I(x, y - 1);
    if (mask[j] != K_NONE || M.bldgAt[j] >= 0 || keep[j] || noBuild[j]) return false;
    if (walled && !ins(x, y - 1)) return false;
    // ((fixer M7b r1) the poor quarter's cottages crowd on out to the wall's lanes)
    // ((M7c) the street network runs out to the wall lane: every block street inside it is fronted)
    if (cp.kind != CK_LEGACY) return true;
    return dist(x, y) < (districtAt(x, y - 1) == District::Poor ? 0.95f : 0.86f);
  };
  for (int y = 3; y < H - 3; y++) {
    int xa = -1;
    for (int x = 1; x < W; x++) {
      const bool f = x < W - 1 && frontOk(x, y);
      if (f && xa < 0) xa = x;
      if (!f && xa >= 0) {
        if (x - xa >= 3) {
          // (fixer M7b r1, review: "poor quarters are empty paved blocks") the poor quarter's cottages are built
          // before the outer fronts of the better quarters (it is the densest), the rich quarter's last (its blocks
          // keep their gardens), instead of strictly from the heart out (the outskirts' poor got none)
          const District rq = districtAt((xa + x - 1) / 2, y - 1);
          const float k = rq == District::Poor ? 0.5f : (rq == District::Noble ? 1.12f : 1.0f);
          runs.push_back(Run{y, xa, x - 1, dist((xa + x - 1) / 2, y) * k + hfAt(xa, y, 0x7E44u) * 0.08f});
        }
        xa = -1;
      }
    }
  }
  std::stable_sort(runs.begin(), runs.end(), [](const Run& a, const Run& b) { return a.d < b.d; });
  const int rise = 4;   // (a house's sprite: the V5 budget of a two-storey house)
  auto unitOk = [&](const IRect& r) {
    if (!in(r.x - 1, r.y - rise - 1) || !in(r.x + r.w, r.y + r.h + 2)) return false;
    const int need = lvl[I(r.x, r.y + r.h - 1)];
    for (int y = r.y - 1; y <= r.y + r.h; y++)
      for (int x = r.x - 1; x <= r.x + r.w; x++) {
        const size_t i = I(x, y);
        if (M.bldgAt[i] >= 0 || M.wall[i] || noBuild[i]) return false;
        const bool foot = x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
        if (!foot) continue;
        if (mask[i] != K_NONE || keep[i] || M.prop[i] || front[i] || water[i]) return false;
        const Ground g = M.at(x, y);
        const bool marsh = stilt && !water[i] && (g == Ground::Water || g == Ground::Swamp);   // ((M7c) a stilt house over the pools)
        if (g == Ground::Road || g == Ground::Plaza || g == Ground::Bridge || ((g == Ground::Swamp || groundSolid(g)) && !marsh)) return false;
        if (lvl[i] != need || (walled && !inside[i]) || (!reserved.empty() && reserved[i])) return false;
        if (terraced)
          for (int d = 0; d < 4; d++) if (in(x + D4X[d], y + D4Y[d]) && lvl[I(x + D4X[d], y + D4Y[d])] > lvl[i]) return false;
      }
    // the door on the front, its step walkable and on the level
    const int dx = r.x + r.w / 2, fy = r.y + r.h;
    if (!isStreet(dx, fy) || lvl[I(dx, fy)] != need || groundSolid(M.at(dx, fy)) || water[I(dx, fy)]) return false;
    // the sprite over nobody's front; the front under nobody's sprite
    for (int y = r.y - rise; y <= r.y + r.h; y++)
      for (int x = r.x - 1; x <= r.x + r.w; x++) {
        if (!in(x, y)) continue;
        const size_t i = I(x, y);
        if (front[i] || (noBuild[i] & 6)) return false;
      }
    for (int x = r.x; x < r.x + r.w; x++) if (cover[I(x, r.y + r.h - 1)]) return false;
    if (roofOverStreet(r, rise)) return false;
    for (int y = fy; y <= fy + 1; y++)
      for (int x = dx - 1; x <= dx + 1; x++) if (cover[I(x, y)] || M.bldgAt[I(x, y)] >= 0 || M.wall[I(x, y)]) return false;
    if (walled || compound.w > 0) {
      for (int y = fy; y <= fy + 2; y++)
        for (int x = r.x - 1; x <= r.x + r.w; x++) if (wallAt(x, y)) return false;
      for (int y = r.y - 2; y <= r.y - 1; y++)
        for (int x = r.x - 1; x <= r.x + r.w; x++) if (wallAt(x, y)) return false;
    }
    return true;
  };
  struct Unit { IRect r; Building t; int st; };
  struct TP { Building t; int st, hh; };
  TP tp{Building::House, 1, 2};   // the terrace being laid: its first unit's kind, storeys and depth
  int made = 0;
  std::vector<Unit> units;
  auto flush = [&]() {
    if (units.empty()) return;
    const size_t first = M.bldgs.size();
    for (size_t k = 0; k < units.size(); k++) {
      const Unit& u = units[k];
      putBldg(u.t, u.r, Role::Villager, u.st);
      made++;
    }
    if (units.size() >= 2) {
      terraces++;
      // (fixer M7b r3, review: "row-house terraces look like separate roof sprites butted together") one terrace, one
      // build: every unit takes the first one's roof, materials and wealth (its own facade wash, windows, door and
      // foundation stay its own), so the eave, the ridge and the roof's courses run on across every party wall
      const Bldg lead = M.bldgs[first];
      uint32_t off = hashAt(lead.r.x, lead.r.y, 0x7E48u) % 128u;   // (the row's first front, by its place)
      // (M7c integration, rpg_test --builder: two rows a street apart drew the same fronts, clones across the street)
      // the row's numbering steps on until none of its fronts repeats one of an earlier row's houses of the same size
      // within 14 tiles (the builder's 32 paints: variant - 1 mod 32); no draw, the hash's own walk
      {
        int bestBad = 1 << 30;
        uint32_t bestOff = off;
        for (uint32_t t = 0; t < 32u && bestBad > 0; t++) {
          const uint32_t o2 = (off + t * 37u) % 128u;
          int badN = 0;
          for (size_t k = 0; k < units.size(); k++) {
            const IRect& ur = M.bldgs[first + k].r;
            const uint32_t v = (o2 + (uint32_t)k * 5u) % 32u;
            for (size_t j = 0; j < first; j++) {
              const Bldg& o = M.bldgs[j];
              if (!o.variant || o.r.w != ur.w || o.r.h != ur.h || (uint32_t)(o.variant - 1) % 32u != v) continue;
              const int dx = o.r.cx() - ur.cx(), dy = o.r.cy() - ur.cy();
              if (dx * dx + dy * dy <= 14 * 14) badN++;
            }
          }
          if (badN < bestBad) { bestBad = badN; bestOff = o2; }
        }
        off = bestOff;
      }
      for (size_t k = 0; k < units.size(); k++) {
        Bldg& b = M.bldgs[first + k];
        if (k > 0) b.row |= bld::ROW_W;
        if (k + 1 < units.size()) b.row |= bld::ROW_E;
        b.variant = (uint8_t)(1u + (off + (uint32_t)k * 5u) % 128u);   // each front its own (builder terrace())
        if (k > 0) {
          const uint8_t variant = b.arch.variant;
          const art::Foundation fo = b.arch.foundation;
          const bool stl = b.arch.stilts;
          b.roof = lead.roof;
          b.wealth = lead.wealth;
          b.arch = lead.arch;
          b.styled = lead.styled;
          b.arch.variant = variant;
          b.arch.foundation = fo;
          b.arch.stilts = stl;
        }
        rowsMade++;
      }
    }
    // the back yards: as deep as the land behind allows (two rows at least), reserved now (no other house, path or
    // garden takes them); their walls and planting are the dress (cityDress)
    for (size_t k = 0; k < units.size(); k++) {
      Bldg& b = M.bldgs[first + k];
      const District q = districtAt(b.r.cx(), b.r.y);
      const int maxY = terraced || detached ? 0 : (q == District::Noble ? 8 : (q == District::Poor ? 3 : 5));   // (a terrace keeps its narrow shelf for houses)
      int depth = 0;
      for (int t = 1; t <= maxY; t++) {
        const int y = b.r.y - t;
        bool ok = in(b.r.x, y) && in(b.r.x + b.r.w - 1, y);
        for (int x = b.r.x; x < b.r.x + b.r.w && ok; x++) {
          const size_t i = I(x, y);
          bool bySquare = false;   // (a yard keeps a tile off every square: its wall never shuts a market's table in)
          for (int oy = -1; oy <= 1 && !bySquare; oy++)
            for (int ox = -1; ox <= 1; ox++) if (get(x + ox, y + oy) == K_SQUARE) { bySquare = true; break; }
          if (bySquare || mask[i] != K_NONE || M.bldgAt[i] >= 0 || M.wall[i] || front[i] || water[i] || keep[i] || M.prop[i] || (noBuild[i] & 6) ||
              lvl[i] != lvl[I(b.r.x, b.r.y)] || groundSolid(M.at(x, y)) || M.at(x, y) == Ground::Road || M.at(x, y) == Ground::Plaza ||
              (!reserved.empty() && reserved[i]) || (walled && !inside[i]))
            ok = false;
        }
        if (!ok) break;
        depth = t;
      }
      if (depth < 2) continue;
      b.row |= bld::ROW_YARD;
      for (int t = 1; t <= depth; t++)
        for (int x = b.r.x; x < b.r.x + b.r.w; x++) set(x, b.r.y - t, K_ROWYARD);
    }
    units.clear();
  };
  for (const Run& R : runs) {
    if (made >= want) break;
    int x = R.xa;
    // (a terrace's end stands back from the run's end where a lane comes down beside it: the corner house)
    int len = 0, maxLen = 5 + (int)(hashAt(R.xa, R.y, 0x7E45u) % 5u);
    while (x + 2 <= R.xb && made + (int)units.size() < want) {
      const District q = districtAt(x, R.y - 1);
      const uint32_t h = hashAt(x, R.y, 0x7E46u);
      int w = 3, hh = 2;
      Building t = Building::House;
      int st = 1;
      // ((fixer M7b r1) the better quarters' houses a tile wider: their fronts take fewer of the city's homes, which go
      // to the poor quarter's narrow cottages)
      switch (q) {
        // ((fixer M7b r2) a capital's rich houses a tile narrower: its palace grounds take the room; (fixer M7b r3) the
        // case's own kind and storeys had slipped into that comment, so a rich house fell through to the centre's)
        case District::Noble: w = (capital ? 4 : 5) + (int)(h % 3u); hh = 3; t = (h >> 4) % 5u ? Building::StoneHouse : Building::House; st = (h >> 8) % 3u ? 2 : 1; break;
        case District::Centre: w = 4 + (int)(h % 2u); hh = 2 + (int)((h >> 4) % 2u); t = (h >> 5) % 100u < (capital ? 80u : 55u) ? Building::StoneHouse : Building::House; st = (h >> 8) % 4u ? 2 : 1; break;
        case District::Crafts: w = 4 + (int)(h % 2u); hh = 2 + (int)((h >> 4) % 3u == 0 ? 1 : 0); t = (h >> 5) % 10u < 3 ? Building::StoneHouse : Building::House; st = (h >> 8) % 3u == 0 ? 2 : 1; break;
        case District::Temple: w = (capital ? 3 : 4) + (int)(h % 2u); hh = 2 + (int)((h >> 4) % 2u); t = (h >> 5) % 100u < 55u ? Building::StoneHouse : Building::House; st = (h >> 8) % 2u ? 2 : 1; break;
        default: w = 3; hh = 2; t = (h >> 5) % 4u == 0 && !capital ? Building::Hut : Building::House; st = 1; break;   // the poor quarter
      }
      // (fixer M7b r3) one terrace is one build: its depth, storeys and kind are its first unit's (only the widths
      // vary), so the eave and ridge run level along it (corner houses no longer step up a storey at the ends)
      if (!units.empty()) { t = tp.t; st = tp.st; hh = tp.hh; }
      if (detached) {   // (M7c) the cottage peoples' houses: their own size, one storey (the elves' a little larger)
        t = Building::House;
        w = sylvan() ? 4 + (int)(h % 2u) : 3 + (int)(h % 2u);
        hh = sylvan() && (h >> 6) % 3u == 0 ? 3 : 2;
        st = 1;
      }
      w = std::min(w, R.xb - x + 1);
      if (w < 3) break;
      if (t == Building::Hut) { w = 3; hh = 2; st = 1; }
      if (t == Building::House && w < 4) {   // (a narrow house's exterior paints one storey: test_arch)
        if (!units.empty() && tp.st >= 2) t = Building::StoneHouse;   // (a terrace's narrow unit keeps its storeys, in stone)
        else st = 1;
      }
      if (!units.empty() && st != tp.st) { flush(); len = 0; }   // (a unit that cannot carry the row's storeys starts the next)
      if (len >= maxLen) {   // the terrace ends; a passage to the yards behind before the next
        flush();
        len = 0;
        maxLen = 5 + (int)(hashAt(x, R.y, 0x7E47u) % 5u);
        x += 1;
        continue;
      }
      const IRect r{x, R.y - hh, w, hh};
      if (unitOk(r)) {
        if (units.empty()) tp = TP{t, st, hh};
        units.push_back(Unit{r, t, st});
        x += w;
        len++;
        if (detached) { flush(); len = 0; x += 1; }   // (a tile to the next house)
      } else { flush(); len = 0; x++; }
    }
    flush();
  }
  return made;
}

// ------------------------------------------------------------------------------------------------ the dress
void Gen::cityDress() {
  const Ground lawn = base == Ground::Grass ? Ground::Meadow : base;
  const bool winter = bio == Biome::Snow;
  auto openTile = [&](int x, int y) {
    if (!in(x, y)) return false;
    const size_t i = I(x, y);
    return M.bldgAt[i] < 0 && !M.wall[i] && !M.prop[i] && !front[i] && !water[i] && !groundSolid(M.at(x, y)) && (reserved.empty() || !reserved[i]);
  };
  // a market's ground: an aisle or a keeper's post, or within two tiles of a stall, a table or a cloth (nothing of the
  // street's furniture may shut one in)
  auto nearMarket = [&](int x, int y) {
    if (!reserved.empty() && in(x, y) && reserved[I(x, y)]) return true;
    for (int oy = -2; oy <= 2; oy++)
      for (int ox = -2; ox <= 2; ox++) {
        const int pr = in(x + ox, y + oy) ? M.prop[I(x + ox, y + oy)] : 0;
        if (!pr) continue;
        const Prop pp = (Prop)(pr - 1);
        if (art::isVendorProp(pp) || pp == Prop::Filler || pp == Prop::MarketTable || pp == Prop::GroundCloth || pp == Prop::Fountain) return true;
      }
    return false;
  };
  // (fixer M7b r3, review: "the rich quarter stands on bare dirt with weeds, its compounds bare dirt yards: it reads as
  // a village") the rich quarter is kept: its open ground and its yards laid to lawn, the weeds and wild flowers cut
  // (its trees, bushes and beds stay). Not in the snow or the desert (their own ground is what a garden there is).
  if (!winter && bio != Biome::Desert && bio != Biome::Beach)
    for (int y = 1; y < H - 1; y++)
      for (int x = 1; x < W - 1; x++) {
        const size_t i = I(x, y);
        const uint8_t k = mask[i];
        if ((k != K_NONE && k != K_YARD && k != K_ROWYARD && k != K_COMPOUND) || M.bldgAt[i] >= 0 || M.wall[i] || water[i]) continue;
        const Ground g = M.at(x, y);
        if (g != Ground::Dirt && g != Ground::Grass && g != Ground::Sand) continue;
        if (districtAt(x, y) != District::Noble) continue;   // (the costly test last)
        M.setG(x, y, Ground::Meadow);
        const int pr = M.prop[i];
        if (pr == (int)Prop::TallGrass + 1 || pr == (int)Prop::Flowers1 + 1) M.setP(x, y, 0);
      }
  // --- the back yards: a garden wall round each (shared between neighbours), the yard's own ground and a tree, a bed
  // or the household's clutter
  for (size_t bi = 0; bi < M.bldgs.size(); bi++) {
    const Bldg b = M.bldgs[bi];
    if (!(b.row & bld::ROW_YARD)) continue;
    int depth = 0;
    while (depth < 10 && get(b.r.x, b.r.y - depth - 1) == K_ROWYARD) depth++;
    if (depth < 2) continue;
    const District q = districtAt(b.r.cx(), b.r.y);
    const int top = b.r.y - depth;
    const bool eastEnd = get(b.r.x + b.r.w, b.r.y - 1) != K_ROWYARD;
    const Ground yg = q == District::Noble || q == District::Temple ? lawn : Ground::Dirt;
    // the wall: along the back, down the west side (a neighbour's east side), down the east side where no yard is
    for (int y = top; y < b.r.y; y++)
      for (int x = b.r.x; x < b.r.x + b.r.w; x++) {
        if (get(x, y) != K_ROWYARD) continue;
        M.setG(x, y, yg);
        const bool wallT = y == top || x == b.r.x || (x == b.r.x + b.r.w - 1 && get(x + 1, y) != K_ROWYARD);
        if (wallT && !M.prop[I(x, y)]) M.setProp(x, y, Prop::GardenWall);
      }
    // inside: the tree at the back (its crown over the roofs), a bed or the clutter by the house
    const int ix0 = b.r.x + 1, ix1 = b.r.x + b.r.w - (eastEnd ? 2 : 1);
    if (ix1 < ix0) continue;
    const uint32_t h = hashAt(b.r.x, b.r.y, 0x7A2Du);
    const int tx = ix0 + (int)(h % (uint32_t)(ix1 - ix0 + 1)), ty = top + 1;
    if (ty < b.r.y && !M.prop[I(tx, ty)] && treesF > 0.3f && (q == District::Noble || q == District::Temple || (h >> 4) % 3u == 0)) {
      const Prop tree = winter ? townTree(bio, eco, h) : ((h >> 8) % 3u ? Prop::FruitTree : townTree(bio, eco, h));
      M.setProp(tx, ty, tree);
    }
    const int cxn = tx == ix0 ? ix1 : ix0, cyn = b.r.y - 1;
    if (cyn > top && !M.prop[I(cxn, cyn)]) {
      Prop p = Prop::Barrel;
      if (q == District::Noble) p = winter ? Prop::SnowBush : Prop::FlowerBed;
      else if (q == District::Crafts) p = (h >> 12) % 2u ? Prop::Woodpile : Prop::Crate;
      else if (q == District::Poor) p = (h >> 12) % 2u ? Prop::Crate : Prop::Barrel;
      else p = (h >> 12) % 3u ? Prop::Barrel : Prop::Woodpile;
      M.setProp(cxn, cyn, p);
    }
  }
  // --- the park (fixer M7b r1, review: "every city park is the same template in every culture and seed"): one of four
  // landscapes, picked by the plan's hash (the culture leaning it), laid out organically from the gates the lane ring
  // gives it:
  //   0 the landscape garden: a pond to one side, a serpentine walk between two gates round it, a branch from a third
  //   1 the pond loop: a pond in the middle, an oval walk round it, spokes out to the gates
  //   2 the formal green: an oval walk with a broad axis through a round plaza, hedges along two sides, paired beds
  //   3 the grove meadow: two curving walks crossing off-centre, groves of trees in the corners, wild flowers
  // then the pavilion beside a walk, benches facing the walks, trees in clumps (never a lone grid), all integer maths
  if (park.w > 0) {
    const int x0 = park.x, y0 = park.y, pw = park.w, ph = park.h;
    const int mx = x0 + pw / 2, my = y0 + ph / 2;
    const uint32_t ps = hashAt(x0 * 3 + 1, y0 * 5 + 2, 0x9A41u);
    int style = (int)(ps % 4u);
    if ((ps >> 8) % 3u == 0) {   // a lean per people: the jade and the elves love water, the steppe its open grass
      using A = cult::Archetype;
      if (cultureIs((int)A::Jade) || cultureIs((int)A::Sylvan) || cultureIs((int)A::Marsh)) style = (ps >> 10) % 2u ? 0 : 1;
      else if (cultureIs((int)A::Imperial) || cultureIs((int)A::Dune) || cultureIs((int)A::SunTemple)) style = 2;
      else if (cultureIs((int)A::Steppe) || cultureIs((int)A::Highland)) style = 3;
    }
    if (winter && style <= 1 && pw < 15) style = 3;   // (a small snowbound green: no frozen puddle)
    auto inPark = [&](int x, int y) { return in(x, y) && get(x, y) == K_PARK; };
    auto isPath = [&](int x, int y) { return inPark(x, y) && plan[I(x, y)] == (uint8_t)Urb::ParkPath; };
    auto isPond = [&](int x, int y) { return inPark(x, y) && (M.at(x, y) == Ground::Water || M.at(x, y) == Ground::Ice); };
    auto path = [&](int x, int y) {
      if (!inPark(x, y) || isPond(x, y)) return;
      M.setG(x, y, Ground::Dirt);
      M.setP(x, y, 0);
      plan[I(x, y)] = (uint8_t)Urb::ParkPath;
    };
    // a walk along a quadratic curve a -> c (control b), sampled finely (integer: t = k / N), 8-connected
    auto curve = [&](int ax, int ay, int bx, int by, int cx2, int cy2) {
      const int N = 4 * (std::abs(cx2 - ax) + std::abs(cy2 - ay) + std::abs(bx - ax) + std::abs(by - ay)) + 8;
      const int64_t NN = (int64_t)N * N;
      for (int k = 0; k <= N; k++) {
        const int64_t u = N - k;
        const int64_t qx = u * u * ax + 2 * u * k * bx + (int64_t)k * k * cx2, qy = u * u * ay + 2 * u * k * by + (int64_t)k * k * cy2;
        path((int)((qx * 2 + NN) / (2 * NN)), (int)((qy * 2 + NN) / (2 * NN)));
      }
    };
    // an oval walk round (cx, cy), radii rx, ry (tiles): the ring between the ellipse and the one a tile outside it
    auto oval = [&](int cx2, int cy2, int rx, int ry) {
      auto inside = [&](int dx, int dy, int ax, int ay) { return (int64_t)dx * dx * ay * ay + (int64_t)dy * dy * ax * ax <= (int64_t)ax * ax * ay * ay; };
      for (int y = cy2 - ry - 1; y <= cy2 + ry + 1; y++)
        for (int x = cx2 - rx - 1; x <= cx2 + rx + 1; x++)
          if (inside(x - cx2, y - cy2, rx + 1, ry + 1) && !inside(x - cx2, y - cy2, rx, ry)) path(x, y);
    };
    // the gates: on the park's edge (the lane ring runs round it), one per side, a hashed way along the side
    struct Gate { int x, y; };
    auto gateOn = [&](int side, uint32_t salt) {
      const uint32_t h = hashAt(x0 + side * 7, y0 + side * 3, salt);
      if (side == 0) return Gate{x0 + 3 + (int)(h % (uint32_t)std::max(1, pw - 6)), y0};            // north
      if (side == 1) return Gate{x0 + 3 + (int)(h % (uint32_t)std::max(1, pw - 6)), y0 + ph - 1};   // south
      if (side == 2) return Gate{x0, y0 + 2 + (int)(h % (uint32_t)std::max(1, ph - 4))};            // west
      return Gate{x0 + pw - 1, y0 + 2 + (int)(h % (uint32_t)std::max(1, ph - 4))};                   // east
    };
    const Gate gN = gateOn(0, 0x9A42u), gS = gateOn(1, 0x9A43u), gW = gateOn(2, 0x9A44u), gE = gateOn(3, 0x9A45u);
    const bool flip = (ps >> 4) & 1u, flipV = (ps >> 5) & 1u;
    // the pond: a wobbly oval of open water (ice in the snow lands), never on the park's edge
    int pcx = mx, pcy = my, prx = 0, pry = 0;
    auto pond = [&](int cx2, int cy2, int rx, int ry) {
      pcx = cx2; pcy = cy2; prx = rx; pry = ry;
      for (int y = y0 + 2; y < y0 + ph - 2; y++)
        for (int x = x0 + 2; x < x0 + pw - 2; x++) {
          if (!inPark(x, y) || M.prop[I(x, y)]) continue;
          if (blob(x, y, cx2, cy2, (float)rx, (float)ry, ps ^ 0x90DD0Du) < 0.92f) { M.setG(x, y, winter ? Ground::Ice : Ground::Water); M.setP(x, y, 0); }
        }
      // no spurs or notches: a water tile with fewer than two wet sides goes back to lawn, a lawn tile with three or
      // four wet sides fills in (twice: the shoreline the view rounds off)
      for (int pass = 0; pass < 2; pass++)
        for (int y = y0 + 1; y < y0 + ph - 1; y++)
          for (int x = x0 + 1; x < x0 + pw - 1; x++) {
            if (!inPark(x, y) || M.prop[I(x, y)]) continue;
            const int n = (int)isPond(x - 1, y) + (int)isPond(x + 1, y) + (int)isPond(x, y - 1) + (int)isPond(x, y + 1);
            if (isPond(x, y) && n < 2) M.setG(x, y, lawn);
            else if (!isPond(x, y) && n >= 3 && x > x0 + 1 && y > y0 + 1 && x < x0 + pw - 2 && y < y0 + ph - 2) M.setG(x, y, winter ? Ground::Ice : Ground::Water);
          }
    };
    switch (style) {
      case 0: {   // the landscape garden
        const int qx = flip ? x0 + pw / 3 : x0 + pw - 1 - pw / 3, qy = flipV ? y0 + ph / 3 + 1 : y0 + ph - 1 - ph / 3 - 1;
        pond(qx, qy, std::max(2, pw / 6), std::max(2, ph / 5));
        // the walk from the west gate to the east, bowed away from the pond; a branch from the far side's gate to it
        const int by = qy < my ? std::min(y0 + ph - 2, my + ph / 2) : std::max(y0 + 1, my - ph / 2);
        curve(gW.x, gW.y, mx, by, gE.x, gE.y);
        const Gate g3 = qy < my ? gS : gN;
        curve(g3.x, g3.y, (g3.x + mx) / 2 + (flip ? 2 : -2), (g3.y + by) / 2, (mx + (flip ? -pw / 6 : pw / 6)), (by + my) / 2);
        // a little walk round the pond's near side
        const int ry = std::max(2, ph / 5) + 2, rx = std::max(2, pw / 6) + 2;
        curve(qx - rx, qy + (qy < my ? 1 : -1), qx, qy + (qy < my ? ry + 1 : -ry - 1), qx + rx, qy + (qy < my ? 1 : -1));
        break;
      }
      case 1: {   // the pond loop
        const int rx = std::max(3, pw / 2 - 4), ry = std::max(2, ph / 2 - 3);
        pond(mx, my, std::max(2, rx - 2), std::max(1, ry - 2));
        oval(mx, my, rx, ry);
        for (const Gate& g2 : {gN, gS, flip ? gW : gE}) curve(g2.x, g2.y, (g2.x * 2 + mx) / 3, (g2.y * 2 + my) / 3, (g2.x + mx * 2) / 3, (g2.y + my * 2) / 3);
        break;
      }
      case 2: {   // the formal green
        const int rx = std::max(3, pw / 2 - 3), ry = std::max(2, ph / 2 - 3);
        oval(mx, my, rx, ry);
        for (int x = x0; x < x0 + pw; x++) path(x, my);
        for (int y = my - 2; y <= my + 2; y++)
          for (int x = mx - 3; x <= mx + 3; x++) {
            const int dx = x - mx, dy = y - my;
            if (dx * dx * 4 + dy * dy * 9 <= 36) path(x, y);   // the round plaza
          }
        curve(gN.x, gN.y, (gN.x + mx) / 2, (gN.y + my) / 2, mx, my - 2);
        break;
      }
      default: {   // the grove meadow
        const int ox = flip ? -pw / 6 : pw / 6, oy = flipV ? -1 : 1;
        curve(gW.x, gW.y, mx + ox / 2, y0 + (flipV ? ph - 2 : 1), gE.x, gE.y);
        curve(gN.x, gN.y, mx + ox, my + oy * 2, gS.x, gS.y);
        break;
      }
    }
    // the hedge (the formal green only, along its north and south sides; open at the walks, in runs of three or more)
    if (style == 2) {
      auto hedgeOk = [&](int x, int y) { return inPark(x, y) && !isPath(x, y) && !isPond(x, y) && !front[I(x, y)] && !M.prop[I(x, y)] && !isPath(x, y + 1) && !isPath(x, y - 1); };
      for (int y : {y0, y0 + ph - 1}) {
        int run = 0;
        for (int x = x0 + 1; x <= x0 + pw - 1; x++) {
          const bool ok = x < x0 + pw - 1 && hedgeOk(x, y) && !isPath(x - 1, y) && !isPath(x + 1, y);
          if (ok) { run++; continue; }
          if (run >= 3) for (int k = 1; k <= run; k++) M.setProp(x - k, y, winter ? Prop::SnowBush : Prop::Hedge);
          run = 0;
        }
      }
    }
    // the pavilion (3 x 2: the prop on the bottom middle, Filler on the rest): on lawn beside a walk, as near the
    // style's place for it as the ground allows (by the pond, at the plaza's north side, at the walks' crossing)
    {
      const int wantX = style == 2 ? mx : (style <= 1 ? pcx : mx), wantY = style == 2 ? my - 3 : (style <= 1 ? pcy - pry - 2 : my - 1);
      int best = -1, bx = 0, by = 0;
      for (int y = y0 + 2; y < y0 + ph - 1; y++)
        for (int x = x0 + 2; x < x0 + pw - 2; x++) {
          bool ok = true;
          for (int yy = y - 1; yy <= y && ok; yy++)
            for (int xx = x - 1; xx <= x + 1; xx++)
              if (!inPark(xx, yy) || M.prop[I(xx, yy)] || isPath(xx, yy) || isPond(xx, yy)) { ok = false; break; }
          if (!ok || !isPath(x, y + 1)) continue;   // its steps come down onto a walk
          const int d = std::abs(x - wantX) * 2 + std::abs(y - wantY) * 3 + (int)(hashAt(x, y, 0x9A46u) % 3u);
          if (best < 0 || d < best) { best = d; bx = x; by = y; }
        }
      if (best >= 0) {
        for (int y = by - 1; y <= by; y++) for (int x = bx - 1; x <= bx + 1; x++) M.setProp(x, y, Prop::Filler);
        M.setProp(bx, by, Prop::Pavilion);
        // flower beds either side of its steps (the formal green and the landscape garden)
        if (style == 0 || style == 2)
          for (int s : {-2, 2})
            if (inPark(bx + s, by) && !M.prop[I(bx + s, by)] && !isPath(bx + s, by) && !isPond(bx + s, by)) M.setProp(bx + s, by, winter ? Prop::SnowBush : Prop::FlowerBed);
      }
    }
    // the formal green's paired beds along its axis
    if (style == 2)
      for (int s : {-1, 1})
        for (int k = 1; k <= 2; k++) {
          const int fx = mx + s * (3 + k * 3);
          for (int fy : {my - 1, my + 1})
            if (inPark(fx, fy) && !M.prop[I(fx, fy)] && !isPath(fx, fy) && !isPond(fx, fy)) M.setProp(fx, fy, winter ? Prop::SnowBush : Prop::FlowerBed);
        }
    // the pond's margin: reeds at its edge, lily pads on open water, a few mossy stones
    for (int y = y0 + 1; y < y0 + ph - 1; y++)
      for (int x = x0 + 1; x < x0 + pw - 1; x++) {
        if (!inPark(x, y) || M.prop[I(x, y)]) continue;
        const uint32_t h = hashAt(x, y, 0x9A47u);
        if (isPond(x, y)) {
          if (!winter && h % 9u == 0 && isPond(x - 1, y) && isPond(x + 1, y) && isPond(x, y - 1) && isPond(x, y + 1)) M.setProp(x, y, Prop::LilyPad);
          continue;
        }
        if (isPath(x, y)) continue;
        const bool shore = isPond(x - 1, y) || isPond(x + 1, y) || isPond(x, y - 1) || isPond(x, y + 1);
        if (shore && !winter && h % 3u == 0) M.setProp(x, y, Prop::Reeds);
        else if (shore && h % 11u == 1) M.setProp(x, y, winter ? Prop::SnowRock : Prop::MossRock);
      }
    // benches beside the walks: on lawn with a walk to the south (a bench faces south), spaced along
    for (int y = y0 + 1; y < y0 + ph - 1; y++)
      for (int x = x0 + 1; x < x0 + pw - 1; x++) {
        if (!inPark(x, y) || isPath(x, y) || isPond(x, y) || M.prop[I(x, y)]) continue;
        if (!isPath(x, y + 1) || hashAt(x, y, 0x9A48u) % 4u) continue;
        bool near = false;
        for (int oy = -1; oy <= 1 && !near; oy++)
          for (int ox = -3; ox <= 3; ox++)
            if (in(x + ox, y + oy) && M.prop[I(x + ox, y + oy)] == (int)Prop::Bench + 1) { near = true; break; }
        if (!near) M.setProp(x, y, Prop::Bench);
      }
    // trees in clumps: two to four groves (their places hashed), dense within, sparse elsewhere, a tile clear of the
    // walks, the water and each other; the grove meadow plants most
    const Prop tree = townTree(bio, eco, 11u), tree2 = townTree(bio, eco, 11u + (ps >> 16) % 7u);
    const int groves = 2 + (int)((ps >> 12) % 3u);
    int gx[4], gy[4];
    for (int k = 0; k < groves; k++) {
      const uint32_t h = hashAt(x0 + k * 13, y0 + k * 7, 0x9A49u);
      // (into the corners and along the edges, away from the middle where the walks meet)
      gx[k] = (k & 1) ? x0 + 2 + (int)(h % (uint32_t)std::max(1, pw / 3)) : x0 + pw - 3 - (int)(h % (uint32_t)std::max(1, pw / 3));
      gy[k] = (k & 2) ? y0 + 2 + (int)((h >> 8) % (uint32_t)std::max(1, ph / 3)) : y0 + ph - 3 - (int)((h >> 8) % (uint32_t)std::max(1, ph / 3));
    }
    const uint32_t dense = style == 3 ? 2u : 3u, sparse = style == 2 ? 40u : 16u;
    for (int y = y0 + 1; y < y0 + ph - 1; y++)
      for (int x = x0 + 1; x < x0 + pw - 1; x++) {
        if (!inPark(x, y) || isPath(x, y) || isPond(x, y) || M.prop[I(x, y)]) continue;
        bool clear = true;
        for (int oy = -1; oy <= 1 && clear; oy++)
          for (int ox = -1; ox <= 1; ox++)
            if (!in(x + ox, y + oy) || isPath(x + ox, y + oy) || isPond(x + ox, y + oy) || (M.prop[I(x + ox, y + oy)] && M.prop[I(x + ox, y + oy)] != (int)Prop::Reeds + 1)) { clear = false; break; }
        if (!clear) continue;
        int near = 99;
        for (int k = 0; k < groves; k++) near = std::min(near, std::abs(x - gx[k]) + std::abs(y - gy[k]));
        const uint32_t h = hashAt(x, y, 0x7AEEu);
        if (h % (near <= 3 ? dense : sparse)) continue;
        M.setProp(x, y, (hashAt(x, y, 0x7AEFu) % 5u) ? ((h >> 9) % 3u == 0 ? tree2 : tree) : (winter ? tree : Prop::FruitTree));
      }
    // bushes under the groves and wild flowers in the meadow
    for (int y = y0 + 1; y < y0 + ph - 1; y++)
      for (int x = x0 + 1; x < x0 + pw - 1; x++) {
        if (!inPark(x, y) || isPath(x, y) || isPond(x, y) || M.prop[I(x, y)]) continue;
        if (isPath(x, y + 1) || isPath(x, y - 1) || isPath(x - 1, y) || isPath(x + 1, y)) continue;
        int near = 99;
        for (int k = 0; k < groves; k++) near = std::min(near, std::abs(x - gx[k]) + std::abs(y - gy[k]));
        const uint32_t h = hashAt(x, y, 0x9A4Au);
        if (near <= 4 && h % 7u == 0) M.setProp(x, y, winter ? Prop::SnowBush : ((h >> 8) % 3u ? Prop::Bush : Prop::BerryBush));
        else if (!winter && style != 2 && h % 9u == 1) M.setProp(x, y, (h >> 8) % 3u == 0 ? Prop::Flowers1 : ((h >> 8) % 3u == 1 ? Prop::Flowers2 : Prop::Flowers3));
      }
  }
  // --- the market square's market cross, by the stalls (where the square's centrepiece is something else)
  if (!squares.empty() && mktZone.w > 0) {
    bool have = false;
    for (int y = mktZone.y - 6; y < mktZone.y + mktZone.h + 6 && !have; y++)
      for (int x = mktZone.x - 6; x < mktZone.x + mktZone.w + 6; x++)
        if (in(x, y) && M.prop[I(x, y)] == (int)Prop::MarketCross + 1) { have = true; break; }
    const int mx = mktZone.x + mktZone.w / 2, my = mktZone.y + mktZone.h / 2;
    for (int r = 0; r <= 12 && !have; r++)
      for (int oy = -r; oy <= r && !have; oy++)
        for (int ox = -r; ox <= r && !have; ox++) {
          if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
          const int x = mx + ox, y = my + oy;
          if (!in(x, y) || get(x, y) != K_SQUARE || M.prop[I(x, y)] || front[I(x, y)] || cover[I(x, y)] || nearMarket(x, y)) continue;
          bool clear = true;   // a tile of open paving round it, no roof over its shaft
          for (int qy = y - 4; qy <= y + 1 && clear; qy++)
            for (int qx = x - 1; qx <= x + 1; qx++) {
              if (!in(qx, qy)) { clear = false; break; }
              const size_t j = I(qx, qy);
              if (M.bldgAt[j] >= 0 || front[j] || (qy >= y - 1 && (M.prop[j] || get(qx, qy) != K_SQUARE))) { clear = false; break; }
            }
          if (clear && putSolid(x, y, Prop::MarketCross)) have = true;
        }
  }
  // --- the city's notice board on its square, a few tiles from the centrepiece (as rpg/story/places.cpp would stand one
  // at runtime: open paving round it, a tile to read it from, nothing over its roof), so it stands there from the start
  // and nobody idles on its tile when it goes up
  if (!squares.empty()) {
    int best = -1, bx = 0, by = 0;
    for (int y = cy - 12; y <= cy + 12; y++)
      for (int x = cx - 12; x <= cx + 12; x++) {
        if (!in(x - 2, y - 3) || !in(x + 2, y + 2) || M.at(x, y) != Ground::Plaza || get(x, y) != K_SQUARE) continue;
        const int d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
        if (d2 < 9 || nearMarket(x, y)) continue;
        bool ok = true;
        for (int oy = -1; oy <= 1 && ok; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            const size_t j = I(x + ox, y + oy);
            if (M.prop[j] || M.bldgAt[j] >= 0 || M.wall[j] || front[j] || groundSolid(M.at(x + ox, y + oy)) || get(x + ox, y + oy) != K_SQUARE) { ok = false; break; }
          }
        for (int ox : {-2, 2}) if (ok && (M.prop[I(x + ox, y)] || M.bldgAt[I(x + ox, y)] >= 0)) ok = false;
        if (ok && (M.prop[I(x, y + 2)] || M.bldgAt[I(x, y + 2)] >= 0 || front[I(x, y + 2)])) ok = false;
        for (int oy = -3; oy <= -2 && ok; oy++)
          for (int ox = -1; ox <= 1; ox++) if (M.prop[I(x + ox, y + oy)] || M.bldgAt[I(x + ox, y + oy)] >= 0) { ok = false; break; }
        if (!ok) continue;
        const int d = (int)std::sqrt((float)d2);
        const int score = 400 - std::abs(d - 5) * 30 + (int)(hashAt(x, y, 0xB0A2u) % 13u);
        if (score > best) { best = score; bx = x; by = y; }
      }
    if (best >= 0) putSolid(bx, by, Prop::NoticeBoard);
  }
  // --- the temple close: lawn beds either side of the way to the door, edged with flower beds
  for (const Precinct& pc : precincts) {
    if (pc.role != 3) continue;
    const IRect& s = pc.sq;
    int doorX = s.x + s.w / 2;
    for (const Bldg& b : M.bldgs)
      if (b.r.y + b.r.h == s.y && b.doorX() >= s.x && b.doorX() < s.x + s.w) { doorX = b.doorX(); break; }
    for (int y = s.y + 1; y < s.y + s.h - 1; y++)
      for (int x = s.x + 1; x < s.x + s.w - 1; x++) {
        if (std::abs(x - doorX) <= 2 || get(x, y) != K_SQUARE || M.prop[I(x, y)] || front[I(x, y)] || nearMarket(x, y)) continue;
        M.setG(x, y, winter ? Ground::Snow : Ground::Grass);
        const bool rim = y == s.y + 1 || y == s.y + s.h - 2 || std::abs(x - doorX) == 3 || x == s.x + 1 || x == s.x + s.w - 2;
        if (rim && (x + y) % 2 == 0) M.setProp(x, y, winter ? Prop::SnowBush : Prop::FlowerBed);
      }
  }
  // --- the avenues' open walks: tree pits and lamps where no house front is (a crown never hides a facade), a corner
  // stone at every junction corner of the better quarters' walks, planters along the rich quarter's fronts
  auto cls = [&](int x, int y) { return in(x, y) && isStreet(x, y) ? (Urb)plan[I(x, y)] : Urb::None; };
  auto carriage = [&](int x, int y) { const Urb u = cls(x, y); return u == Urb::Avenue || u == Urb::Gutter || u == Urb::Street || u == Urb::Lane || u == Urb::Alley; };
  for (int y = 2; y < H - 2; y++)
    for (int x = 2; x < W - 2; x++) {
      if (cls(x, y) != Urb::Sidewalk) continue;
      const size_t i = I(x, y);
      if (M.prop[i] || front[i] || (noBuild[i] & 2)) continue;
      const District q = districtAt(x, y);
      // the outer corner of a walk at a junction: carriage on two sides and between them
      bool corner = false;
      for (int k = 0; k < 4 && !corner; k++) {
        const int ax = (k & 1) ? 1 : -1, ay = (k & 2) ? 1 : -1;
        // (fixer M7b r2, review: "a bollard on every step of the diagonal avenue's walk") a junction's corner: the walk
        // runs on two tiles or more along both streets (a slanting walk's step has a one-tile arm)
        if (carriage(x + ax, y) && carriage(x, y + ay) && carriage(x + ax, y + ay) && cls(x - ax, y) == Urb::Sidewalk && cls(x - 2 * ax, y) == Urb::Sidewalk &&
            cls(x, y - ay) == Urb::Sidewalk && cls(x, y - 2 * ay) == Urb::Sidewalk)
          corner = true;
      }
      if (nearMarket(x, y)) continue;
      if (corner) {
        if (q != District::Poor) putSolid(x, y, Prop::Bollard);
        continue;
      }
      bool avenueSide = false;
      for (int d = 0; d < 4; d++) if (cls(x + D4X[d], y + D4Y[d]) == Urb::Avenue) avenueSide = true;
      if (!avenueSide || cover[i]) continue;
      bool clear = true;   // no house front, door or stall within reach of a crown
      for (int oy = -3; oy <= 1 && clear; oy++)
        for (int ox = -1; ox <= 1; ox++) {
          const int qx = x + ox, qy = y + oy;
          if (!in(qx, qy)) continue;
          const size_t j = I(qx, qy);
          if (M.bldgAt[j] >= 0 || front[j] || (M.prop[j] && (qx != x || qy != y) && propSolid((Prop)(M.prop[j] - 1)))) { clear = false; break; }
        }
      if (!clear) continue;
      const int ph = (x + y) % 12;
      // (fixer M7b r3, review: "street lamps one tile apart, posts stacked"; a slanting walk keeps x + y on every step,
      // so its phase put a lamp or a tree on each one) a lamp keeps three tiles from every other lamp and two from a
      // tree, a tree three from another: lamps and trees alternate down the avenue
      auto spaced = [&](bool lamp) {
        for (int oy = -3; oy <= 3; oy++)
          for (int ox = -3; ox <= 3; ox++) {
            if (!in(x + ox, y + oy) || (!ox && !oy)) continue;
            const int pr = M.prop[I(x + ox, y + oy)];
            if (!pr) continue;
            const Prop pp = (Prop)(pr - 1);
            const int dd = std::max(std::abs(ox), std::abs(oy));
            if (pp == Prop::Lamppost && (lamp || dd <= 2)) return false;
            if (art::isTreeProp(pp) && (!lamp || dd <= 2)) return false;
          }
        return true;
      };
      // (a tree stands back from a kerb's face: never on a walk's front row with the carriageway right below it, where
      // its trunk and pit sat on the kerb stones)
      const bool kerbFace = carriage(x, y + 1) || cls(x, y + 1) == Urb::Gutter;
      if (ph == 0 && treesF > 0.2f && !kerbFace && spaced(false)) putSolid(x, y, winter ? Prop::SnowPine : townTree(bio, eco, (uint32_t)(x * 7 + y * 3)));
      else if (ph == 6 && spaced(true)) putSolid(x, y, Prop::Lamppost);
    }
  for (const Bldg& b : M.bldgs) {
    if (!b.row && !(townIsHome(b.type) && b.wealth >= 3)) continue;
    if (districtAt(b.r.cx(), b.r.cy()) != District::Noble || b.r.w < 4) continue;
    const int fy = b.r.y + b.r.h;
    for (int x : {b.r.x, b.r.x + b.r.w - 1}) {
      if (std::abs(x - b.doorX()) < 2 || !in(x, fy) || cls(x, fy) != Urb::Sidewalk || M.prop[I(x, fy)] || front[I(x, fy)] || nearMarket(x, fy)) continue;
      if (hashAt(x, fy, 0x91A7u) % 2u) continue;
      putSolid(x, fy, Prop::Planter);
    }
  }
  // (fixer M7b r2, review: "lamp posts stand on bare dirt beside the sidewalk instead of on it") a street lamp the town
  // set on open ground right behind a walk steps onto the walk (else it goes)
  for (int y = 2; y < H - 2; y++)
    for (int x = 2; x < W - 2; x++) {
      if (M.prop[I(x, y)] != (int)Prop::Lamppost + 1 || isStreet(x, y)) continue;
      int wx = -1, wy = -1;
      for (int d = 0; d < 4 && wx < 0; d++) {
        const int nx = x + D4X[d], ny = y + D4Y[d];
        if (cls(nx, ny) == Urb::Sidewalk && !M.prop[I(nx, ny)] && !front[I(nx, ny)] && !nearMarket(nx, ny)) { wx = nx; wy = ny; }
      }
      bool byWalk = wx >= 0;
      for (int d = 0; d < 4 && !byWalk; d++) byWalk = cls(x + D4X[d], y + D4Y[d]) == Urb::Sidewalk;
      if (!byWalk) continue;
      M.setP(x, y, 0);
      if (wx >= 0) putSolid(wx, wy, Prop::Lamppost);
      // (M7c fixer, review: "yellow-green diamond patches by lamp posts") the lamp's old tile was the scrap of turf it
      // stood on, one tile among the paved yards behind the walk: it drew as a lone green diamond on the paving. It
      // takes the paving round it (two paved sides or more)
      {
        const Ground g0 = M.at(x, y);
        if (g0 != Ground::Road && g0 != Ground::Plaza && !water[I(x, y)] && M.bldgAt[I(x, y)] < 0 && !M.wall[I(x, y)]) {
          int nRoad = 0, nPlaza = 0;
          for (int d = 0; d < 4; d++) {
            const Ground q = M.at(x + D4X[d], y + D4Y[d]);
            if (q == Ground::Road) nRoad++;
            else if (q == Ground::Plaza) nPlaza++;
          }
          if (nRoad + nPlaza >= 2) M.setG(x, y, nPlaza > nRoad ? Ground::Plaza : Ground::Road);
        }
      }
    }
  // (fixer M7b r3) no two street lamps of a walk within three tiles of each other (the town's own lamps, moved onto a
  // walk above, met the avenue's: two posts stacked one tile apart)
  for (int y = 2; y < H - 2; y++)
    for (int x = 2; x < W - 2; x++) {
      if (M.prop[I(x, y)] != (int)Prop::Lamppost + 1 || !isStreet(x, y)) continue;
      bool near = false;
      for (int oy = -3; oy <= 0 && !near; oy++)
        for (int ox = -3; ox <= 3; ox++) {
          if ((oy == 0 && ox >= 0) || !in(x + ox, y + oy)) continue;
          if (M.prop[I(x + ox, y + oy)] == (int)Prop::Lamppost + 1 && isStreet(x + ox, y + oy)) { near = true; break; }
        }
      if (near) M.setP(x, y, 0);
    }
  // corner stones flanking every gate's throat, just inside
  for (const GatePlan& g : gatePlan) {
    for (int s : {-2, 2}) {
      const int x = g.dx == 0 ? g.x + s : g.x + g.dx * 2, y = g.dx == 0 ? g.y + g.dy * 2 : g.y + s;
      if (openTile(x, y) && !isStreet(x, y) && !wallAt(x, y) && !nearMarket(x, y)) putSolid(x, y, Prop::Bollard);
    }
  }
  // --- the dune city's souk (fixer M7b r1, review: "a jumbled stripy mass"): awnings strung over the carriageway of its
  // market avenues near the heart in bays of six or seven tiles, every bay one cloth across the whole street on its
  // poles, an open bay to the sky between them (the drain's line sets the street's way); never over a junction, a
  // stall's aisle or a bend
  if (cultureIs((int)cult::Archetype::Dune)) {
    auto gut = [&](int x, int y) { return in(x, y) && cls(x, y) == Urb::Gutter; };
    for (int y = 2; y < H - 2; y++)
      for (int x = 2; x < W - 2; x++) {
        if (!gut(x, y) || dist(x, y) > 0.42f || dist(x, y) < 0.12f) continue;
        const bool ew = gut(x - 1, y) && gut(x + 1, y), ns = gut(x, y - 1) && gut(x, y + 1);
        if (ew == ns) continue;   // (a bend, a junction or a two-tile band's end: open)
        const int along = ew ? x : y;
        if (((along % 7) + 7) % 7 == 0) continue;   // the open bay
        // the band across the street: the drain and the carriage either side of it
        const int px = ew ? 0 : 1, py = ew ? 1 : 0;
        bool ok = true;
        for (int k = -1; k <= 1 && ok; k++) {
          const int tx = x + px * k, ty = y + py * k;
          const Urb u = cls(tx, ty);
          if ((u != Urb::Avenue && u != Urb::Gutter) || M.prop[I(tx, ty)] || nearMarket(tx, ty)) { ok = false; break; }
          for (int d = 0; d < 4; d++) {
            const Urb v = cls(tx + D4X[d], ty + D4Y[d]);
            if (v == Urb::Street || v == Urb::Lane || v == Urb::Alley || get(tx + D4X[d], ty + D4Y[d]) == K_SQUARE) { ok = false; break; }
          }
        }
        if (!ok) continue;
        for (int k = -1; k <= 1; k++) M.setProp(x + px * k, y + py * k, Prop::Canopy);
      }
    // a bay cut down to a tile or two by a junction is taken down (no stub of cloth)
    for (int y = 2; y < H - 2; y++)
      for (int x = 2; x < W - 2; x++) {
        if (M.prop[I(x, y)] != (int)Prop::Canopy + 1 || !gut(x, y)) continue;
        const bool ew = gut(x - 1, y) && gut(x + 1, y);
        auto canopy = [&](int qx, int qy) { return in(qx, qy) && M.prop[I(qx, qy)] == (int)Prop::Canopy + 1; };
        int n = 1;
        for (int k = 1; k < 8 && canopy(x + (ew ? k : 0), y + (ew ? 0 : k)); k++) n++;
        for (int k = 1; k < 8 && canopy(x - (ew ? k : 0), y - (ew ? 0 : k)); k++) n++;
        if (n >= 3) continue;
        for (int k = -1; k <= 1; k++) {
          const int tx = x + (ew ? 0 : k), ty = y + (ew ? k : 0);
          if (canopy(tx, ty)) M.setP(tx, ty, 0);
        }
      }
  }
}

// ------------------------------------------------------------------------------------------------ the fabric layer
// Every tile's class, upkeep and quarter, from the finished plan. A planned city's streets keep the class their band
// painted; its squares, the park, the yards and gardens theirs. A town's main street is a Street (its fronted edge a
// Sidewalk where the street is three wide), its squares Squares. ALWAYS empty in a village.
void Gen::urbanFabric() {
  if (village) { M.urb.clear(); return; }
  const size_t n = (size_t)W * H;
  M.urb.assign(n, 0);
  auto quarterAt = [&](int x, int y) {
    switch (districtAt(x, y)) {
      case District::Noble: return Quarter::Rich;
      case District::Crafts: return Quarter::Craft;
      case District::Poor: return Quarter::Poor;
      default: return Quarter::Centre;
    }
  };
  auto upkeepOf = [&](Quarter q) {
    int k = q == Quarter::Rich ? 3 : (q == Quarter::Centre ? 2 : (q == Quarter::Craft ? 1 : 0));
    if (town) k = 2;   // (fixer M7b r2, review: "the town's square and street covered in dirt stains, kerbs left on bare dirt": a town keeps its high street and square swept)
    if (townWealth >= 3) k++;
    else if (townWealth == 0) k--;
    return std::clamp(k, 0, 3);
  };
  auto carriage = [&](int x, int y) { return in(x, y) && get(x, y) == K_MAIN && !water[I(x, y)] && M.bldgAt[I(x, y)] < 0; };
  // (a town) a main-street tile that a frontage borders (a building, its doorstep or apron, a yard or footpath)
  auto fronted = [&](int x, int y, int& side) {
    for (int d = 0; d < 4; d++) {
      const int nx = x + D4X[d], ny = y + D4Y[d];
      if (!in(nx, ny)) continue;
      const size_t j = I(nx, ny);
      if (M.bldgAt[j] >= 0 || front[j] || mask[j] == K_YARD) { side = d; return true; }
    }
    return false;
  };
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const size_t i = I(x, y);
      if (!O.used[i] || water[i] || M.bldgAt[i] >= 0 || M.wall[i]) continue;
      if (walled && !ins(x, y)) continue;   // the country beyond the ring
      const Quarter q = city ? quarterAt(x, y) : Quarter::Centre;
      Urb u = Urb::None;
      if (city) {
        const uint8_t pc = planned && !plan.empty() ? plan[i] : 0;
        switch (mask[i]) {
          case K_MAIN:
            u = pc ? (Urb)pc : (planned ? Urb::Street : Urb::Avenue);
            break;
          case K_LANE:
            if (pc) u = (Urb)pc;
            else u = (M.at(x, y) == Ground::Dirt || q == Quarter::Poor) ? Urb::Alley : Urb::Lane;
            break;
          case K_SQUARE: u = (Urb)pc == Urb::Close ? Urb::Close : Urb::Square; break;
          case K_COMPOUND:
            if (M.at(x, y) == Ground::Plaza || M.at(x, y) == Ground::Road) u = Urb::Court;
            break;
          case K_PARK: u = M.at(x, y) == Ground::Dirt ? Urb::ParkPath : Urb::Park; break;
          case K_ROWYARD: u = (q == Quarter::Rich || M.at(x, y) != Ground::Dirt) ? Urb::Garden : Urb::Court; break;
          case K_FIELD: u = Urb::Garden; break;   // (inside the wall: orchards, allotments, flower gardens)
          case K_YARD: {
            // the quays: paving along the water
            if (M.at(x, y) == Ground::Plaza)
              for (int d = 0; d < 4; d++) if (in(x + D4X[d], y + D4Y[d]) && water[I(x + D4X[d], y + D4Y[d])]) u = Urb::Quay;
            break;
          }
          default: break;
        }
      } else if (town) {
        switch (mask[i]) {
          case K_MAIN: {
            u = Urb::Street;
            int side = -1;
            if (carriage(x, y) && fronted(x, y, side)) {
              const int ox = x - D4X[side], oy = y - D4Y[side], ox2 = x - 2 * D4X[side], oy2 = y - 2 * D4Y[side];
              int sb = -1;
              if (carriage(ox, oy) && carriage(ox2, oy2) && !fronted(ox, oy, sb)) u = Urb::Sidewalk;   // (three wide: the walk on the fronted edge)
            }
            break;
          }
          case K_SQUARE: u = Urb::Square; break;
          default: break;
        }
      }
      if (u != Urb::None) M.urb[i] = urbPack(u, upkeepOf(q), q);
    }
  // (fixer M7b r2, review: "L-shaped kerb stubs in the town that bound nothing") a town's walk is a run along its high
  // street's front: a scrap of walk (under six tiles) is street
  if (town) {
    std::vector<uint8_t> seen(n, 0);
    for (int y = 1; y + 1 < H; y++)
      for (int x = 1; x + 1 < W; x++) {
        const size_t i0 = I(x, y);
        if (seen[i0] || urbClass(M.urb[i0]) != Urb::Sidewalk) continue;
        std::vector<int> comp{(int)i0};
        seen[i0] = 1;
        for (size_t h = 0; h < comp.size(); h++) {
          const int qx = comp[h] % W, qy = comp[h] / W;
          for (int d = 0; d < 4; d++) {
            const int nx = qx + D4X[d], ny = qy + D4Y[d];
            if (!in(nx, ny)) continue;
            const size_t j = I(nx, ny);
            if (!seen[j] && urbClass(M.urb[j]) == Urb::Sidewalk) { seen[j] = 1; comp.push_back((int)j); }
          }
        }
        if (comp.size() < 6)
          for (int c : comp) M.urb[(size_t)c] = urbPack(Urb::Street, urbUpkeep(M.urb[(size_t)c]), urbQuarter(M.urb[(size_t)c]));
      }
  }
  // (fixer M7b r1, review: "10 sidewalk tiles beside no carriageway or square") a walk whose carriageway the plan lost
  // (a compound's wall, a house or the water took it) is no kerbed walk: it is paved lane, flush with the ground
  if (city) {
    std::vector<size_t> lone;
    for (int y = 1; y + 1 < H; y++)
      for (int x = 1; x + 1 < W; x++) {
        const size_t i = I(x, y);
        if (urbClass(M.urb[i]) != Urb::Sidewalk) continue;
        bool nb = false;
        for (int oy = -1; oy <= 1 && !nb; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            const Urb v = urbClass(M.urb[I(x + ox, y + oy)]);
            if (urbCarriage(v) || v == Urb::Gutter || urbOpen(v)) { nb = true; break; }
          }
        if (!nb) lone.push_back(i);
      }
    for (size_t i : lone) M.urb[i] = urbPack(Urb::Lane, urbUpkeep(M.urb[i]), urbQuarter(M.urb[i]));
    // (fixer M7b r2, review: "small kerbed islands and a T-shaped kerb stub standing loose in the middle of the cobbled
    // avenue") where two bands meet, a walk tile with carriageway on two opposite sides, or a little run of walk (three
    // tiles or fewer) fronting nothing, is carriageway: no kerbed scrap stands loose in the street
    auto carrU = [&](int x, int y) { const Urb v = urbClass(M.urb[I(x, y)]); return urbCarriage(v) || v == Urb::Gutter; };
    auto carrOf = [&](int x, int y) {
      Urb best = Urb::None;
      for (int d = 0; d < 4; d++) {
        const Urb v = urbClass(M.urb[I(x + D4X[d], y + D4Y[d])]);
        if (urbCarriage(v) && (best == Urb::None || urbRank((uint8_t)v) > urbRank((uint8_t)best))) best = v;
      }
      return best == Urb::None ? Urb::Street : best;
    };
    for (int pass = 0; pass < 3; pass++) {
      std::vector<std::pair<size_t, Urb>> fix;
      std::vector<uint8_t> seen(n, 0);
      for (int y = 2; y + 2 < H; y++)
        for (int x = 2; x + 2 < W; x++) {
          const size_t i = I(x, y);
          if (urbClass(M.urb[i]) != Urb::Sidewalk) continue;
          if ((carrU(x - 1, y) && carrU(x + 1, y)) || (carrU(x, y - 1) && carrU(x, y + 1))) { fix.push_back({i, carrOf(x, y)}); continue; }
          if (seen[i]) continue;
          // the run of walk this tile belongs to (4-connected), and whether it fronts anything
          std::vector<int> comp{(int)i};
          seen[i] = 1;
          bool fronts = false;
          for (size_t h = 0; h < comp.size(); h++) {
            const int qx = comp[h] % W, qy = comp[h] / W;
            for (int d = 0; d < 4; d++) {
              const int nx = qx + D4X[d], ny = qy + D4Y[d];
              if (!in(nx, ny)) continue;
              const size_t j = I(nx, ny);
              if (M.bldgAt[j] >= 0 || front[j] || mask[j] == K_SQUARE) fronts = true;
              if (!seen[j] && urbClass(M.urb[j]) == Urb::Sidewalk) { seen[j] = 1; comp.push_back((int)j); }
            }
          }
          if (comp.size() <= 3 && !fronts)
            for (int c : comp) fix.push_back({(size_t)c, carrOf(c % W, c / W)});
        }
      if (fix.empty()) break;
      for (const auto& f : fix) M.urb[f.first] = urbPack(f.second, urbUpkeep(M.urb[f.first]), urbQuarter(M.urb[f.first]));
    }
    // (fixer M7b r1, review: "orphan quay blocks along the sylvan city stream") a quay is a run along the water: a lone
    // paved doorstep or yard by a stream (no quay beside it) keeps its plain paving, not a stranded block of coping
    std::vector<size_t> orphan;
    for (int y = 1; y + 1 < H; y++)
      for (int x = 1; x + 1 < W; x++) {
        const size_t i = I(x, y);
        if (urbClass(M.urb[i]) != Urb::Quay) continue;
        int n = 0;
        for (int d = 0; d < 4; d++) n += urbClass(M.urb[I(x + D4X[d], y + D4Y[d])]) == Urb::Quay;
        if (n == 0) orphan.push_back(i);
      }
    for (size_t i : orphan) M.urb[i] = 0;
  }
  // (fixer M7b r3, review: "where an avenue's drain meets a square: a staggered double grate, a kinked channel, a
  // notched kerb") an avenue opens into its square on one straight line across all its bands: where the square's
  // ragged edge lets one band (a drain row, a carriageway, a walk) run a tile further than the bands beside it, that
  // slice of the band is laid as the square (visual only: the ground and its props are the plan's)
  {
    auto band = [&](int x, int y) {
      if (!in(x, y)) return false;
      const Urb v = urbClass(M.urb[I(x, y)]);
      return v == Urb::Avenue || v == Urb::Gutter || v == Urb::Sidewalk;
    };
    auto paved = [&](int x, int y) {
      if (!in(x, y)) return false;
      const Urb v = urbClass(M.urb[I(x, y)]);
      return v == Urb::Square || v == Urb::Close || v == Urb::Court;
    };
    for (int pass = 0; pass < 3; pass++) {
      bool changed = false;
      for (int d = 0; d < 4; d++) {
        const int ax = D4X[d], ay = D4Y[d];   // the way into the square
        const int qx = ay != 0 ? 1 : 0, qy = ax != 0 ? 1 : 0;   // across the band
        for (int y = 2; y < H - 2; y++)
          for (int x = 2; x < W - 2; x++) {
            // the start of a run across the band at (x, y): a band tile whose predecessor across is not one
            if (!paved(x + ax, y + ay) || !band(x, y) || band(x - qx, y - qy)) continue;
            int len = 0;
            bool ahead = true;
            while (band(x + qx * len, y + qy * len)) { ahead = ahead && paved(x + qx * len + ax, y + qy * len + ay); len++; }
            if (!ahead || len > 6) continue;
            // the run behind it, across the same line: a longer run that holds this one means this slice sticks out
            const int bx = x - ax, by = y - ay;
            if (!band(bx, by)) continue;
            int b0 = 0, b1 = 0;
            while (band(bx - qx * (b0 + 1), by - qy * (b0 + 1))) b0++;
            while (band(bx + qx * (b1 + 1), by + qy * (b1 + 1))) b1++;
            if (b0 + b1 + 1 <= len || b1 + 1 < len) continue;
            if (!paved(x - qx, y - qy) && !paved(x + qx * len, y + qy * len)) continue;   // (a square on one flank at least)
            for (int k = 0; k < len; k++) {
              const size_t i = I(x + qx * k, y + qy * k);
              M.urb[i] = urbPack(Urb::Square, urbUpkeep(M.urb[i]), urbQuarter(M.urb[i]));
            }
            changed = true;
          }
      }
      if (!changed) break;
    }
  }
}

// ------------------------------------------------------------------------------------------------ M7c: the homes' hooks
// (M7c phase A: moved here unchanged from Gen::homesBegin / homesFinish, now rpg/world/town_homes.cpp, so the planned
// city's homes are the CITY lane's and the shared sweep the EDGE lane's)

// (fixer M7b r1) a planned city aims a little higher (its poor quarter packed with cottages), within the scale and the
// buildings' cap (the services and works already standing, a margin for what is still to come)
void Gen::cityHomesWant() {
  const TownScale sc = townScale(P.type);
  const int others = (int)M.bldgs.size() - hHave;
  homesWant = std::max(homesWant, std::min({sc.homesMax - 6, homesWant + 14, sc.bldgMax - 12 - others}));
}

// (M7b) the planned city: the jade city's walled family compounds among its terraces, then the row houses along every
// front (the frontage sweep fills in after them: the outskirts, the lanes, the odd corners). (M7c) the faubourgs' share
// (faubourgWant, the EDGE lane's) is left for outside the walls.
int Gen::plannedHomes() {
  int made = 0;
  if (cultureIs((int)cult::Archetype::Jade) && !terraced && style != cult::Layout::Compound) made += compoundHomes((homesWant - hHave) / 3);   // (a terrace is too narrow for a court; (fixer M7b r2) a compound city has laid its compounds already)
  made += rowHouses(homesWant - (hHave + made) - 6 - faubourgWant);
  return made;
}

// (M7b) a planned city's wide streets: only the tiles open land borders can front a house; ((fixer M7b r1) a planned
// city's poor quarter is its densest: its plots come before the outskirts of the others)
bool Gen::cityFront(int x, int y, float& weight) const {
  bool open = false;
  for (int oy = -2; oy <= 2 && !open; oy++)
    for (int ox = -1; ox <= 1; ox++) if (get(x + ox, y + oy) == K_NONE) { open = true; break; }
  if (!open) return false;
  weight = districtAt(x, y) == District::Poor ? 0.62f : 1.0f;
  return true;
}

// (M7b) a planned city still short after two top-up rounds: its row houses give up their back yards for cottages
void Gen::cityHomesRound(int round) {
  if (round != 2) return;
  for (Bldg& b : M.bldgs) {
    if (!(b.row & bld::ROW_YARD)) continue;
    b.row = (uint8_t)(b.row & ~bld::ROW_YARD);
    for (int y = b.r.y - 10; y < b.r.y; y++)
      for (int x = b.r.x; x < b.r.x + b.r.w; x++) if (get(x, y) == K_ROWYARD) set(x, y, K_NONE);
  }
}

// (fixer M7b r2, review: "cities under the housing floor on seeds past CI's") a planned city still short: its wide row
// houses (six tiles or more along the street) are let as two, a narrow house of three tiles and the rest, each with its
// own door on the street and a party wall between them (the terrace runs on); no new ground is taken. homesNow: the
// homes standing (as rpg_test --towns counts them). Returns the homes made.
int Gen::cityHomesLast(int homesNow) {
  const TownScale need = townScale(P.type);
  int made = 0;
  if (homesNow < need.homesMin + 5) {
    const size_t nb = M.bldgs.size();
    for (size_t bi = 0; bi < nb && homesNow < need.homesMin + 5; bi++) {
      const Bldg b0 = M.bldgs[bi];
      if (!townIsHome(b0.type) || b0.r.w < 6 || b0.type == Building::Hut) continue;
      const IRect left{b0.r.x, b0.r.y, 3, b0.r.h}, right{b0.r.x + 3, b0.r.y, b0.r.w - 3, b0.r.h};
      const int fy = b0.r.y + b0.r.h, ldx = left.x + left.w / 2, rdx = right.x + right.w / 2;
      // (a detached house: both new doorsteps walkable ground the old one's way reaches, beside its own)
      auto step = [&](int x) {
        if (!in(x, fy) || M.bldgAt[I(x, fy)] >= 0 || M.wall[I(x, fy)] || water[I(x, fy)] || groundSolid(M.at(x, fy)) || get(x, fy) == K_FIELD) return false;
        if (M.prop[I(x, fy)] && propSolid((Prop)(M.prop[I(x, fy)] - 1))) return false;
        return isStreet(x, fy) || get(x, fy) == K_YARD;
      };
      if (!step(ldx) || !step(rdx)) continue;
      // and no other building's sprite over the new doorsteps and aprons (the V5 rule)
      bool covered = false;
      for (size_t oj = 0; oj < M.bldgs.size() && !covered; oj++) {
        if (oj == bi) continue;
        const Bldg& o = M.bldgs[oj];
        const int orise = townRiseTiles(o.type, o.storeys);
        // (a terrace neighbour across the shared wall: its sprite stops at its own footprint on that side)
        const bool joinW = (o.row & bld::ROW_E) && (b0.row & bld::ROW_W) && o.r.x + o.r.w == b0.r.x && o.r.y + o.r.h == fy;
        const bool joinE = (b0.row & bld::ROW_E) && (o.row & bld::ROW_W) && o.r.x == b0.r.x + b0.r.w && o.r.y + o.r.h == fy;
        const int sx0 = o.r.x - (joinE ? 0 : 1), sx1 = o.r.x + o.r.w - (joinW ? 1 : 0);
        for (int dx : {ldx, rdx})
          for (int y = fy; y <= fy + 1 && !covered; y++)
            for (int x = dx - 1; x <= dx + 1; x++)
              if (x >= sx0 && x <= sx1 && y >= o.r.y - orise && y <= o.r.y + o.r.h) { covered = true; break; }
      }
      if (covered) continue;
      // the left unit keeps the building (narrowed), the right one is a new house beside it
      Bldg& L = M.bldgs[bi];
      L.r = left;
      L.row = (uint8_t)(b0.row | bld::ROW_E);
      // (fixer M7b r3) a terrace's unit keeps the terrace's storeys (its eave runs level with the neighbours'): a
      // narrow house that would paint one storey is built in stone instead
      const bool inRow = (b0.row & (bld::ROW_W | bld::ROW_E)) != 0;
      if (L.type == Building::House) {
        if (inRow && L.storeys >= 2) L.type = Building::StoneHouse;
        else L.storeys = 1;   // (a narrow house paints one storey: test_arch)
      }
      // (two halves of one width are told apart: the right one in the other kind of house, so no twin stands beside it)
      Building rt = right.w == left.w ? (b0.type == Building::StoneHouse ? Building::House : Building::StoneHouse) : b0.type;
      if (inRow && rt == Building::House && right.w < 4 && b0.storeys >= 2) rt = Building::StoneHouse;
      const int st = (rt == Building::House && right.w < 4) ? 1 : (int)b0.storeys;
      const int ri = putBldg(rt, right, b0.owner, st);
      M.bldgs[(size_t)ri].row = (uint8_t)((b0.row & ~bld::ROW_W) | bld::ROW_W);
      if (inRow) {   // one terrace, one roof: the new unit takes its neighbour's roof, materials and wealth
        Bldg& Rb = M.bldgs[(size_t)ri];
        const uint8_t variant = Rb.arch.variant;
        Rb.roof = b0.roof; Rb.wealth = b0.wealth; Rb.arch = b0.arch; Rb.styled = b0.styled; Rb.arch.variant = variant;
      }
      for (int y = fy; y <= fy + 1; y++)
        for (int x = ldx - 1; x <= ldx + 1; x++) if (in(x, y)) front[I(x, y)] = 1;
      made++;
      homesNow++;
      rowsMade++;
    }
  }
  return made;
}

}  // namespace town
}  // namespace ew
