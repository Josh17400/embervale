// M7c the city wall's outline (VISION_PLAN 15.M7c "every city unique: wall outline (round, oval, polygonal, squeezed
// along a river or coast, following the land)"). EDGE lane. The outline is a normalised radius per normalised bearing
// round the heart (EdgeData::rho, EDGE_RAYS samples): the provisional inside of land(), the octilinear ring of
// wallRing and every "inside the wall less a margin" test of the layout read it, so the streets, the wall street and
// the ring agree whatever the shape. Hill-fort towns (and any settlement without a profile) keep the classic wobbly
// blob, byte for byte.
//   Round    a circle in tiles (the core's area), barely wobbling
//   Oval     the classic wobbly ellipse of the footprint (M7b)
//   Polygon  four to eight straight curtains between corner towers (the imperial and jade peoples' rectangle with its
//            corners bevelled, the others' irregular polygons), scaled to the oval's area
//   River    squeezed along a river or lake bank that bounds one side of the city (the wall runs along the bank; the
//            other sides swell a little so the walled area holds)
//   Coast    the same along the sea shore
//   Land     following the land: the wall keeps to the brow of the ground it stands on (each curtain snapped to the
//            nearest step of relief across its line)
// Every choice is hashed from the plan's seed and read from the land: no Rng draw. Integer and + - * / sqrt maths and
// the dmath trigonometry only.
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include "rpg/world/town_edge.h"
#include "rpg/world/town_gen.h"

namespace ew {
namespace town {

EdgeData& edgeOf(Gen& g) {
  if (!g.edge) g.edge = std::make_shared<EdgeData>();
  return *g.edge;
}

namespace {

float classicRho(const Gen& g, float a) {
  return g.wallR + (vnoise(dcos(a) * 1.7f + 5, dsin(a) * 1.7f + 5, g.wseed) - 0.5f) * 0.32f;
}
float rayAngle(int k) { return -D_PI + (k + 0.5f) * (D_TAU / EDGE_RAYS); }
// tiles per normalised unit along normalised bearing a
float tileScale(const Gen& g, float a) {
  const float x = dcos(a) * g.rx, y = dsin(a) * g.ry;
  return std::sqrt(x * x + y * y);
}
void smooth(std::vector<float>& r, int passes, int span) {
  const int n = (int)r.size();
  for (int p = 0; p < passes; p++) {
    std::vector<float> t(r.size());
    for (int k = 0; k < n; k++) {
      float s = 0;
      for (int o = -span; o <= span; o++) s += r[(size_t)((k + o + n) % n)];
      t[(size_t)k] = s / (2 * span + 1);
    }
    r.swap(t);
  }
}
float area(const std::vector<float>& r) {
  float s = 0;
  for (float v : r) s += v * v;
  return s;
}

}  // namespace

float edgeWallRho(const Gen& g, float a) {
  const EdgeData* E = g.edge.get();
  if (!E || E->rho.size() != (size_t)EDGE_RAYS) return classicRho(g, a);
  float t = (a + D_PI) * (EDGE_RAYS / D_TAU) - 0.5f;
  int k0 = (int)std::floor(t);
  const float f = t - (float)k0;
  k0 = ((k0 % EDGE_RAYS) + EDGE_RAYS) % EDGE_RAYS;
  const int k1 = (k0 + 1) % EDGE_RAYS;
  return E->rho[(size_t)k0] * (1 - f) + E->rho[(size_t)k1] * f;
}

bool edgeInWall(const Gen& g, int x, int y, float inset) {
  const EdgeData* E = g.edge.get();
  if (!E || E->rho.size() != (size_t)EDGE_RAYS) return g.blobLess(x, y, g.cx, g.cy, g.rx, g.ry, g.wseed, g.wallR - inset);
  const float dx = (x - g.cx) / g.rx, dy = (y - g.cy) / g.ry;
  const float r = std::sqrt(dx * dx + dy * dy);
  return r < edgeWallRho(g, datan2(dy, dx)) - inset;
}

void edgeWallShape(Gen& g) {
  EdgeData& E = edgeOf(g);
  E.rho.clear();
  E.corners.clear();
  E.shape = WallShape::None;
  if (!g.city) return;   // (a hill-fort town keeps its classic ring)
  const int N = EDGE_RAYS;
  const uint32_t h = hash32(g.P.seed ^ 0x3A11F0E5u);
  std::vector<float> base((size_t)N);
  for (int k = 0; k < N; k++) base[(size_t)k] = classicRho(g, rayAngle(k));
  const float A0 = area(base);
  auto at = [&](float a, float r, int& x, int& y) {
    x = (int)std::floor(g.cx + 0.5f + dcos(a) * g.rx * r);
    y = (int)std::floor(g.cy + 0.5f + dsin(a) * g.ry * r);
  };
  // --- water bounding one side: per ray, the first water inside the oval, and how much land lies beyond it (a river
  // through the city has its far bank; the sea or a lake has nothing but water to the rim)
  std::vector<float> wetR((size_t)N, -1.0f);
  int wetRays = 0, sea = 0;
  for (int k = 0; k < N; k++) {
    const float a = rayAngle(k), sc = tileScale(g, a), r0 = base[(size_t)k] + 0.06f, step = 0.5f / sc;
    float r1 = -1;
    int beyond = 0, land = 0;
    for (float r = 0.22f; r < r0; r += step) {
      int x, y;
      at(a, r, x, y);
      if (!g.in(x, y)) break;
      const bool w = g.water[g.I(x, y)] != 0;
      if (r1 < 0) { if (w) r1 = r; continue; }
      beyond++;
      if (!w) land++;
    }
    if (r1 < 0 || land * 100 > beyond * 35) continue;
    wetR[(size_t)k] = r1;
    wetRays++;
    int x, y;
    at(a, std::min(r0, r1 + 4.0f / sc), x, y);
    if (g.in(x, y) && g.M.biomeAt(x, y) == Biome::Ocean) sea++;
  }
  std::vector<float> r = base;
  WallShape shape = WallShape::Oval;
  const int pick = (int)(h % 100u);
  const bool ordered = g.cultureIs((int)cult::Archetype::Imperial) || g.cultureIs((int)cult::Archetype::Jade);
  if (wetRays >= 8) {
    // squeezed along the bank: the wall stands two and a half tiles back from the water on the wet rays (never under
    // 0.5 of the oval), the turn onto the bank eased, the dry side swollen so the walled area holds
    shape = sea * 2 >= wetRays || g.arch == Archetype::Port || g.arch == Archetype::Fishing ? WallShape::Coast : WallShape::River;
    std::vector<uint8_t> wet((size_t)N, 0);
    for (int k = 0; k < N; k++) {
      if (wetR[(size_t)k] < 0) continue;
      const float a = rayAngle(k);
      r[(size_t)k] = std::max(base[(size_t)k] * 0.5f, std::min(base[(size_t)k], wetR[(size_t)k] - 2.5f / tileScale(g, a)));
      wet[(size_t)k] = 1;
    }
    // the turn: a ray beside a wet one takes the lower of itself and its neighbours (two each way), then a soft blur
    std::vector<float> m = r;
    for (int k = 0; k < N; k++)
      for (int o = -2; o <= 2; o++) m[(size_t)k] = std::min(m[(size_t)k], r[(size_t)((k + o + N) % N)] + 0.04f * std::abs(o));
    r = m;
    smooth(r, 2, 1);
    float wetA = 0, dryA = 0;
    for (int k = 0; k < N; k++) {
      bool nearWet = false;
      for (int o = -4; o <= 4 && !nearWet; o++) nearWet = wet[(size_t)((k + o + N) % N)] != 0;
      if (nearWet) wetA += r[(size_t)k] * r[(size_t)k];
      else dryA += r[(size_t)k] * r[(size_t)k];
    }
    if (dryA > 0) {
      const float f = std::min(1.16f, std::sqrt(std::max(1.0f, (A0 - wetA) / dryA)));
      std::vector<float> s((size_t)N, 1.0f);
      for (int k = 0; k < N; k++) {
        int dW = 99;
        for (int o = -8; o <= 8; o++) if (wet[(size_t)((k + o + N) % N)]) dW = std::min(dW, std::abs(o));
        // full swell away from the water, easing in over the eight rays next to it
        const float t = dW >= 8 ? 1.0f : (dW <= 4 ? 0.0f : (dW - 4) / 4.0f);
        s[(size_t)k] = 1.0f + (f - 1.0f) * t;
      }
      for (int k = 0; k < N; k++) r[(size_t)k] *= s[(size_t)k];
    }
  } else if ((ordered && pick < 65) || (!ordered && pick >= 30 && pick < 52)) {
    // the polygon: four corners (the ordered peoples' bevelled rectangle) or five to eight (an irregular trace)
    shape = WallShape::Polygon;
    const int nc = ordered && pick < 65 ? 4 : 5 + (int)((h >> 8) % 4u);
    const float a0 = nc == 4 ? -D_PI * 0.75f + (((int)((h >> 12) % 5u)) - 2) * 0.04f : hashf(7, 3, g.wseed) * D_TAU;
    // area of a regular polygon inscribed in the unit circle: (nc / 2) sin(2 pi / nc); scaled to the oval's
    const float inscribed = 0.5f * nc * dsin(D_TAU / nc);
    const float meanR = std::sqrt(A0 / N);
    const float rv = meanR * std::sqrt(D_PI / inscribed);
    std::vector<std::pair<float, float>> cs;
    for (int k = 0; k < nc; k++) {
      const float jA = nc == 4 ? 0.0f : (hashf(k, 11, g.wseed) - 0.5f) * (D_TAU / nc) * 0.35f;
      const float jR = 1.0f + (hashf(k, 13, g.wseed) - 0.5f) * (nc == 4 ? 0.04f : 0.12f);
      cs.push_back({dwrap(a0 + k * (D_TAU / nc) + jA), rv * jR});
    }
    std::sort(cs.begin(), cs.end());
    // the ray's crossing of the corner-to-corner edge it falls between (normalised plane)
    for (int k = 0; k < N; k++) {
      const float a = rayAngle(k), dx = dcos(a), dy = dsin(a);
      float best = rv;
      for (int c = 0; c < nc; c++) {
        const auto& p = cs[(size_t)c];
        const auto& q = cs[(size_t)((c + 1) % nc)];
        const float px = dcos(p.first) * p.second, py = dsin(p.first) * p.second;
        const float qx = dcos(q.first) * q.second, qy = dsin(q.first) * q.second;
        const float ex = qx - px, ey = qy - py;
        const float den = dx * ey - dy * ex;
        if (std::fabs(den) < 1e-6f) continue;
        const float t = (px * ey - py * ex) / den, s = (px * dy - py * dx) / den;
        if (t > 0 && s >= -1e-4f && s <= 1.0001f) { best = t; break; }
      }
      r[(size_t)k] = best;
    }
    E.corners = cs;
  } else if (pick < 30 || g.cultureIs((int)cult::Archetype::Steppe)) {
    // round: a circle in tiles of the oval's area (it stands a little proud of the core's short axis, into the belt)
    shape = WallShape::Round;
    float A = 0;
    for (int k = 0; k < N; k++) A += base[(size_t)k] * base[(size_t)k];
    const float R = std::sqrt(A / N) * std::sqrt(g.rx * g.ry);
    for (int k = 0; k < N; k++) {
      const float a = rayAngle(k);
      const float wob = 1.0f + (vnoise(dcos(a) * 1.2f + 3, dsin(a) * 1.2f + 3, g.wseed + 7u) - 0.5f) * 0.05f;
      r[(size_t)k] = R * wob / tileScale(g, a);
    }
  } else if (pick >= 52 && pick < 80) {
    // following the land: each ray snapped to the nearest step of relief across it (the wall keeps to the brow it
    // stands on), when enough of the ring finds one; else the oval
    const int L0 = g.lvl[g.I(g.cx, g.cy)];
    int snapped = 0;
    std::vector<float> s = base;
    for (int k = 0; k < N; k++) {
      const float a = rayAngle(k), sc = tileScale(g, a), b = base[(size_t)k];
      float bestR = -1, bestD = 1e9f;
      int prevL = -1;
      for (float rr = b * 0.78f; rr < b * 1.12f; rr += 0.5f / sc) {
        int x, y;
        at(a, rr, x, y);
        if (!g.in(x, y)) break;
        const int L = g.lvl[g.I(x, y)];
        if (prevL >= 0 && L != prevL) {
          // the wall on the heart's side of the step (on the upper ground where the town climbs, on its own level
          // where the land falls away)
          const float cand = rr - 1.5f / sc;
          const float d = std::fabs(cand - b) + (prevL == L0 ? 0.0f : 0.05f);
          if (d < bestD) { bestD = d; bestR = cand; }
        }
        prevL = L;
      }
      if (bestR > 0) { s[(size_t)k] = bestR; snapped++; }
    }
    if (snapped * 4 >= N) {
      shape = WallShape::Land;
      smooth(s, 2, 2);
      // the area kept: a ring pulled in by the land swells a little elsewhere
      const float f = std::min(1.10f, std::sqrt(std::max(1.0f, A0 / std::max(1e-3f, area(s)))));
      for (float& v : s) v *= f;
      r = s;
    }
  }
  // never past the buffer's room: three tiles in from its edge
  for (int k = 0; k < N; k++) {
    const float a = rayAngle(k);
    const float lim = std::min(dcos(a) > 0 ? (g.W - 4 - g.cx) / (std::fabs(dcos(a)) * g.rx + 1e-4f) : (g.cx - 3) / (std::fabs(dcos(a)) * g.rx + 1e-4f),
                               dsin(a) > 0 ? (g.H - 4 - g.cy) / (std::fabs(dsin(a)) * g.ry + 1e-4f) : (g.cy - 3) / (std::fabs(dsin(a)) * g.ry + 1e-4f));
    r[(size_t)k] = std::min(r[(size_t)k], lim);
  }
  E.rho = r;
  E.shape = shape;
}

}  // namespace town
}  // namespace ew
