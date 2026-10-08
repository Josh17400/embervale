// EMBERVALE art: the M1 economy's market stalls by trade and the specialisations' yard props (owner 2026-10-05: "the
// market stalls look TERRIBLE"; rpg/world/economy.h). See rpg/art.h for the conventions and rpg/art/art_internal.h for
// the shared helpers.
//
// (stall facings, owner 2026-10-05: "the stalls look like I'm head-level looking at a picture on a wall, and they all
// face the screen") A stall, an open table, a ground cloth and a cart are each a small 3D model painted in the SAME
// projection as the buildings (art_building.cpp): the ground's depth runs 1:1 up the screen and every height rises
// 1:1 above it (screen row = y - z), so from the camera's ~45-55 degrees the roofs' and counters' tops dominate, the
// south faces are short, east and west faces are edge-on. The model is drawn through a depth buffer (nearer = larger
// y + z), lit from the top-left like everything else, and turned to any of four facings (art_props.h StallFacing):
// the customers' side south (the open front), north (we see the back cloth or planks), east or west (side profiles:
// the roof's slope side-on, the counter's end, the goods along one edge). The trade's goods are little sprites lying
// ON the counter top (seen from above) and hanging from the rail under the roof's front edge.
#include "rpg/art/art_internal.h"

namespace art {

namespace {

struct Awn { uint32_t a, b; bool striped; };
// M3: a culture's awnings for the stall being painted (marketStallStyled); inactive: the classic cloths
struct AwnStyle { bool on = false; int kind = 0; uint32_t a = 0, b = 0; };
thread_local AwnStyle g_awn;
Awn awningOfClassic(int v);
Awn awningOf(int v) {
  if (!g_awn.on) return awningOfClassic(v);
  const Awn cl = awningOfClassic(v);
  // the culture's two colours, nudged a little toward the classic cloth of this index so a row never repeats one cloth
  const uint32_t a = mix(g_awn.a, cl.a, 0.18f), b = mix(g_awn.b, cl.b, 0.12f);
  switch (g_awn.kind) {
    case 1: return {a, mix(a, rgba(40, 30, 30), 0.45f), false};                  // plain dyed cloth, a darker hem
    case 2: return {mix(rgba(198, 170, 108), a, 0.15f), rgba(120, 88, 52), false};   // reed matting
    case 4: return {a, b, (v & 1) == 0};                                          // silk: stripes and plain by turns
    case 5: return {mix(rgba(150, 104, 68), a, 0.12f), rgba(92, 60, 42), false};    // hide
    default: return {a, b, true};
  }
}
Awn awningOfClassic(int v) {
  static const Awn t[kStallAwnings] = {
      {rgba(184, 52, 48), rgba(238, 226, 198), true},    // red and cream
      {rgba(52, 92, 168), rgba(236, 228, 204), true},    // blue and cream
      {rgba(56, 128, 76), rgba(232, 222, 184), true},    // green and cream
      {rgba(214, 156, 52), rgba(124, 74, 40), false},    // ochre, a brown trim
      {rgba(126, 66, 140), rgba(236, 224, 206), true},   // purple and cream
      {rgba(214, 196, 156), rgba(168, 58, 50), false},   // natural canvas, a red trim
      {rgba(44, 124, 128), rgba(232, 216, 160), false},  // teal, a cream trim
      {rgba(150, 92, 52), rgba(222, 172, 74), true},     // brown and ochre
  };
  return t[((v % kStallAwnings) + kStallAwnings) % kStallAwnings];
}

void hl(Canvas& c, int x0, int x1, int y, uint32_t col) { for (int x = std::min(x0, x1); x <= std::max(x0, x1); x++) c.set(x, y, col); }
void vl(Canvas& c, int x, int y0, int y1, uint32_t col) { for (int y = std::min(y0, y1); y <= std::max(y0, y1); y++) c.set(x, y, col); }

// a small wicker basket (front rim at rows y..y+3) holding a heap of round produce of ramp R (the yard's baskets)
void basketOf(Canvas& c, int x0, int w, int y, const Ramp& R, uint32_t seed, float ballR = 1.6f) {
  const Ramp Wk = ramp(rgba(170, 120, 62), 0.75f);
  for (int i = 0; i < w / 2 + 1; i++) {
    float bx = x0 + 1.5f + i * ((w - 3) / (float)std::max(1, w / 2)) + (hashf(i, 1, seed) - 0.5f);
    float by = y - 0.5f - (hashf(i, 2, seed) * 1.5f);
    ball(c, bx, by, ballR, ballR * 0.9f, R, 0.06f);
  }
  for (int i = 0; i < w / 3; i++) ball(c, x0 + 3.0f + i * 3.2f, y - 2.6f, ballR * 0.9f, ballR * 0.8f, R, 0.06f);
  for (int j = 0; j < 4; j++)
    for (int i = 0; i < w; i++) {
      int k = j == 0 ? 4 : ((i + j) % 2 ? 2 : 3);
      if (i == w - 1) k = 1;
      if (j == 3) k = std::min(k, 1);
      c.set(x0 + i, y + j, Wk[k]);
    }
}

// ================================================================================================ the 3/4 model painter
// Model space ("stall space"): u runs along the counter (0..48, three tiles), v from the customers' edge of the counter
// (0) in toward the back (the keeper's row, to about 36), z up. The facing turns it onto the world (px from the prop
// tile's top-left, y south); the canvas pixel of world (x, y, z) is (x - X0, y - z - Y0).
struct M3 {
  Canvas c;
  std::vector<float> zb;
  int X0, Y0, f;
  M3(int w, int h, int x0, int y0, int facing) : c(w, h), zb((size_t)w * h, -1e9f), X0(x0), Y0(y0), f(facing) {}
  void world(float u, float v, float& x, float& y) const {
    switch (f) {
      case StallN: x = 32.0f - u; y = v; break;
      case StallE: x = 16.0f - v; y = 16.0f - u; break;
      // (stalls fixer round 3) W is E mirrored, not turned: both side profiles keep the model's u = 0 end at the south,
      // so a column's roofs and posts stand the same way whichever side the customers are on
      case StallW: x = v; y = 16.0f - u; break;
      default: x = u - 16.0f; y = 16.0f - v; break;
    }
  }
  void dir(float du, float dv, float& dx, float& dy) const {
    switch (f) {
      case StallN: dx = -du; dy = dv; break;
      case StallE: dx = -dv; dy = -du; break;
      case StallW: dx = dv; dy = -du; break;
      default: dx = du; dy = -dv; break;
    }
  }
  // a canvas pixel at world screen position (sx, sy) with depth key k (the nearer wins)
  void pix(int sx, int sy, float k, uint32_t col) {
    sx -= X0; sy -= Y0;
    if (sx < 0 || sy < 0 || sx >= c.w || sy >= c.h || !chA(col)) return;
    float& d = zb[(size_t)sy * c.w + sx];
    if (k < d) return;
    d = k;
    c.set(sx, sy, col);
  }
  void plotW(float x, float y, float z, uint32_t col, float bias = 0) { pix((int)std::floor(x), (int)std::floor(y - z), y + z + bias, col); }
  void plot(float u, float v, float z, uint32_t col, float bias = 0) {
    float x, y;
    world(u, v, x, y);
    plotW(x, y, z, col, bias);
  }
};

// the light on a surface whose model-space normal is (nu, nv, nz): about 0.4 (in shade) .. 1 (full light), the light
// from the top-left and above like the buildings'; -9 when the surface faces away from the camera or is edge-on
float lit(const M3& m, float nu, float nv, float nz) {
  float nx, ny;
  m.dir(nu, nv, nx, ny);
  const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
  if ((ny + nz) / len <= 0.04f) return -9.0f;
  return 0.52f + 0.48f * (nx * -0.55f + ny * 0.06f + nz * 0.83f) / len;
}
int litK(float l, int a, int b);
// a cloth's light: the brightest band kept for its lit edges, so stripes never wash out on a sunny slope
int litC(float l, int a, int b) { return std::min(3, litK(l - 0.03f, a, b)); }
int litK(float l, int a, int b) {
  l += (bayer(a, b) - 0.5f) * 0.02f;
  return l > 0.94f ? 4 : (l > 0.79f ? 3 : (l > 0.56f ? 2 : (l > 0.40f ? 1 : 0)));
}

// a parallelogram p0 + s a + t b (s, t in 0..1, model space) whose outward normal is n; col(s, t, light) gives each
// sample's colour (0 leaves a hole)
// (fixer M5 r3, review: "market stalls in a snowy village have no snow") while a snowy prop is painted, every surface
// that faces up at or above this height (model z) lies under snow (setMarketSnow; 1e9: none)
thread_local float g_snowZ = 1e9f;
template <class F>
void quad(M3& m, float u0, float v0, float z0, float au, float av, float az, float bu, float bv, float bz, float nu, float nv, float nz, F&& col,
          float bias = 0) {
  const float l = lit(m, nu, nv, nz);
  if (l < -1) return;
  const float la = std::sqrt(au * au + av * av + az * az), lb = std::sqrt(bu * bu + bv * bv + bz * bz);
  const int ns = std::max(1, (int)std::ceil(la * 2.6f)), nt = std::max(1, (int)std::ceil(lb * 2.6f));
  const bool snowy = g_snowZ < 1e8f && nz > 0.42f * std::sqrt(nu * nu + nv * nv + nz * nz);
  for (int i = 0; i <= ns; i++)
    for (int j = 0; j <= nt; j++) {
      const float s = (float)i / ns, t = (float)j / nt;
      uint32_t cc = col(s, t, l);
      if (!chA(cc)) continue;
      if (snowy && z0 + s * az + t * bz >= g_snowZ) {
        // the snow's lie: lit like the surface under it, a ragged thin edge where the cloth or boards show through
        const float uu = u0 + s * au + t * bu, vv = v0 + s * av + t * bv;
        const bool edge = s < 0.05f || s > 0.95f || t < 0.06f || t > 0.94f;
        if (!(edge && hash3((int)std::floor(uu * 1.7f), (int)std::floor(vv * 1.7f), 7717) % 3u == 0)) {
          // the slope's own light (the lit slope near white, the shaded one blue-grey), a soft drift texture
          int k = std::clamp(litK(l - 0.04f, i, j), 1, 4);
          if (k > 1 && hash3((int)std::floor(uu * 0.9f), (int)std::floor(vv * 1.3f), 7719) % 6u == 0) k--;
          cc = kSnow[k];
        }
      }
      m.plot(u0 + s * au + t * bu, v0 + s * av + t * bv, z0 + s * az + t * bz, cc, bias);
    }
}

// an axis-aligned box in model space, every face that turns to the camera in ramp R (lit), col(face, s, t, k) may
// recolour a face (face: 0 top, 1 -v (the customers' side), 2 +v, 3 -u, 4 +u; k the lit ramp index)
template <class F>
void boxM(M3& m, float u0, float u1, float v0, float v1, float z0, float z1, const Ramp& R, F&& col) {
  const float du = u1 - u0, dv = v1 - v0, dz = z1 - z0;
  auto face = [&](int fc) {
    return [&, fc](float s, float t, float l) -> uint32_t {
      const int k = litK(l, (int)(s * 37), (int)(t * 29));
      return col(fc, s, t, k, R[k]);
    };
  };
  quad(m, u0, v0, z1, du, 0, 0, 0, dv, 0, 0, 0, 1, face(0));
  quad(m, u0, v0, z0, du, 0, 0, 0, 0, dz, 0, -1, 0, face(1));
  quad(m, u0, v1, z0, du, 0, 0, 0, 0, dz, 0, 1, 0, face(2));
  quad(m, u0, v0, z0, 0, dv, 0, 0, 0, dz, -1, 0, 0, face(3));
  quad(m, u1, v0, z0, 0, dv, 0, 0, 0, dz, 1, 0, 0, face(4));
}
void boxM(M3& m, float u0, float u1, float v0, float v1, float z0, float z1, const Ramp& R) {
  boxM(m, u0, u1, v0, v1, z0, z1, R, [](int, float, float, int, uint32_t c) { return c; });
}

// a little sprite standing on model point (u, v, z) (its bottom-centre there; hang: its top-centre), at the depth of
// that point (+ bias)
void put(M3& m, const Canvas& s, float u, float v, float z, bool hang = false, float bias = 0.6f) {
  float x, y;
  m.world(u, v, x, y);
  const int sx0 = (int)std::floor(x) - s.w / 2;
  const int sy0 = hang ? (int)std::floor(y - z) : (int)std::floor(y - z) - s.h + 1;
  const float k = y + z + bias;
  for (int j = 0; j < s.h; j++)
    for (int i = 0; i < s.w; i++) {
      const uint32_t p = s.get(i, j);
      if (chA(p) > 127) m.pix(sx0 + i, sy0 + j, k, opaque(p));
    }
}

// the ground shadow of a set of model points: each point (x, y, z) throws its shade to (x + 0.5 z, y + 0.22 z) (the
// light from the top-left), the footprint itself in shade too. A soft, dithered edge.
struct Shade {
  Canvas c;
  std::vector<uint8_t> a;
  int X0, Y0;
  Shade(int x0, int y0, int w, int h) : c(w, h), a((size_t)w * h, 0), X0(x0), Y0(y0) {}
  void cast(float x, float y, float z, uint8_t k) {
    const int sx = (int)std::floor(x + z * 0.5f) - X0, sy = (int)std::floor(y + z * 0.22f) - Y0;
    if (sx < 0 || sy < 0 || sx >= c.w || sy >= c.h) return;
    uint8_t& d = a[(size_t)sy * c.w + sx];
    d = std::max(d, k);
  }
  void castM(const M3& m, float u, float v, float z, uint8_t k) {
    float x, y;
    m.world(u, v, x, y);
    cast(x, y, z, k);
    cast(x, y, 0, (uint8_t)(k * 2 / 3));
  }
  // a model rectangle at height z (its footprint and its cast shade), sampled every half pixel
  void rectM(const M3& m, float u0, float u1, float v0, float v1, float z0, float z1, uint8_t k) {
    for (float u = u0; u <= u1; u += 0.5f)
      for (float v = v0; v <= v1; v += 0.5f) castM(m, u, v, z0 + (z1 - z0) * (v - v0) / std::max(0.01f, v1 - v0), k);
  }
  Canvas done() {
    // fill pinholes, soften the rim (a neighbour count), dither to the game's pixel look
    std::vector<uint8_t> b = a;
    for (int y = 1; y < c.h - 1; y++)
      for (int x = 1; x < c.w - 1; x++) {
        int s = 0, n = 0;
        for (int dy = -1; dy <= 1; dy++)
          for (int dx = -1; dx <= 1; dx++) { s += a[(size_t)(y + dy) * c.w + x + dx]; n += a[(size_t)(y + dy) * c.w + x + dx] ? 1 : 0; }
        if (!a[(size_t)y * c.w + x] && n >= 5) b[(size_t)y * c.w + x] = (uint8_t)(s / n);
        else if (a[(size_t)y * c.w + x] && n < 9) b[(size_t)y * c.w + x] = (uint8_t)(a[(size_t)y * c.w + x] * 3 / 4);
      }
    for (int y = 0; y < c.h; y++)
      for (int x = 0; x < c.w; x++) {
        const int k = b[(size_t)y * c.w + x];
        if (k) c.set(x, y, withA(rgba(26, 16, 44), k));
      }
    return c;
  }
};

// ================================================================================================ the goods (sprites)
// Small sprites in the same view: seen from ~45 degrees above, a top and a short front, lit from the top-left.
const Ramp kWicker = ramp(rgba(176, 126, 64), 0.75f);

// a wicker basket seen from above, heaped with round produce
Canvas basketTop(int w, const Ramp& R, uint32_t seed, float br = 1.5f) {
  const int h = w * 3 / 4 + 2;
  Canvas c(w, h);
  const float cx = w * 0.5f, cy = h - 4.0f, rx = w * 0.5f - 0.5f, ry = w * 0.27f;
  // the side: wicker courses under the rim
  for (int y = (int)cy; y < h; y++)
    for (int x = 0; x < w; x++) {
      const float dx = (x + 0.5f - cx) / rx;
      if (dx * dx > 1.0f - (y - cy) * 0.06f) continue;
      int k = ((x + y) & 1) ? 2 : 3;
      if (x + 0.5f > cx + rx * 0.5f) k--;
      if (y == h - 1) k = 1;
      c.set(x, y, kWicker[k]);
    }
  // the rim and the inside
  ellipse(c, cx, cy, rx, ry, kWicker[4]);
  ellipse(c, cx, cy, rx - 1.0f, ry - 0.8f, kWicker[1]);
  // the heap
  const int n = std::max(3, w / 2);
  for (int i = 0; i < n; i++) {
    const float bx = cx - rx + 1.6f + (rx * 2 - 3.2f) * (i + 0.5f) / n + (hashf(i, 1, seed) - 0.5f) * 0.8f;
    const float by = cy - 0.6f - std::sin(3.14159f * (i + 0.5f) / n) * 1.2f;
    ball(c, bx, by, br, br * 0.9f, R, 0.05f);
  }
  for (int i = 0; i < n / 2; i++) ball(c, cx - rx * 0.5f + i * (rx / std::max(1, n / 2 - 1 + (n / 2 == 1))), cy - 2.2f, br * 0.95f, br * 0.85f, R, 0.05f);
  return c;
}

// a wooden crate or box seen from above: a top of d rows inside its frame, a front of fh rows; fill: what shows in it
Canvas crateTop(int w, int d, int fh, const Ramp* fill, uint32_t seed) {
  Canvas c(w, d + fh);
  for (int y = 0; y < d; y++)
    for (int x = 0; x < w; x++) {
      const bool rim = x == 0 || x == w - 1 || y == 0 || y == d - 1;
      c.set(x, y, rim ? kWood[y == 0 || x == 0 ? 4 : 3] : kWoodDark[1]);
    }
  if (fill)
    for (int i = 0; i < w / 2; i++) ball(c, 1.5f + i * ((w - 3.0f) / std::max(1, w / 2 - 1)), d * 0.5f - 0.3f + (hashf(i, 3, seed) - 0.5f), 1.4f, 1.2f, *fill, 0.05f);
  for (int y = d; y < d + fh; y++)
    for (int x = 0; x < w; x++) {
      int k = (y - d) == fh / 2 ? 1 : 2;
      if (x == 0) k = 3;
      if (x == w - 1 || y == d + fh - 1) k = 1;
      c.set(x, y, kWood[k]);
    }
  return c;
}

// a fish lying on its side (seen from above: its side), head to the left (flip: right), length len
Canvas fishTop(int len, const Ramp& R, bool flip) {
  Canvas c(len + 2, 5);
  for (int i = 0; i < len; i++) {
    const float t = (float)i / (len - 1);
    const float half = 1.5f * std::sin(t * 3.14159f) + 0.4f;
    const int x = flip ? len - i : i + 1;
    for (int j = -(int)std::ceil(half); j <= (int)std::ceil(half); j++) {
      const int k = j < 0 ? 4 - (j == -(int)std::ceil(half) ? 1 : 0) : (j == 0 ? 3 : 1);
      c.set(x, 2 + j, R[k]);
    }
  }
  const int tx = flip ? 0 : len + 1;
  c.set(tx, 0, R[2]); c.set(tx, 4, R[1]); c.set(tx, 1, R[1]); c.set(tx, 3, R[1]);
  c.set(flip ? len - 1 : 2, 1, kInk);   // the eye
  return c;
}

// a roll of cloth lying along the counter (a bolt), its end toward the light
Canvas boltTop(int len, uint32_t col) {
  const Ramp R = ramp(col, 0.7f);
  Canvas c(len, 5);
  for (int x = 0; x < len; x++)
    for (int y = 0; y < 5; y++) {
      int k = y == 0 ? 3 : (y == 1 ? 4 : (y == 2 ? 3 : (y == 3 ? 2 : 1)));
      if (x == len - 1) k = std::max(0, k - 1);
      c.set(x, y, R[k]);
    }
  // the rolled end
  c.set(0, 1, R[4]); c.set(0, 2, R[2]); c.set(1, 2, R[1]); c.set(0, 3, R[3]);
  return c;
}

// a folded stack of cloth: a top of the uppermost fold and its layered front
Canvas foldTop(int w, int layers, uint32_t seed) {
  static const uint32_t cl[6] = {rgba(64, 92, 176), rgba(190, 58, 58), rgba(228, 196, 96), rgba(64, 140, 96), rgba(150, 76, 156), rgba(214, 120, 64)};
  Canvas c(w, 4 + layers);
  const Ramp T = ramp(cl[seed % 6], 0.7f);
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < w; x++) c.set(x, y, T[y == 0 ? 4 : (x == w - 1 ? 2 : 3)]);
  for (int l = 0; l < layers; l++) {
    const Ramp R = ramp(cl[(seed + l * 2 + 1) % 6], 0.7f);
    for (int x = 0; x < w; x++) c.set(x, 4 + l, R[x == 0 ? 3 : (x == w - 1 ? 1 : 2)]);
  }
  return c;
}

// a jug: a round body, a neck, its dark mouth seen from above, a handle
Canvas jugTop(const Ramp& R, bool big) {
  const int w = big ? 8 : 6, h = big ? 10 : 8;
  Canvas c(w + 1, h);
  const float cx = w * 0.5f;
  ball(c, cx, h - (big ? 3.6f : 2.8f), big ? 3.4f : 2.6f, big ? 3.2f : 2.5f, R, 0.04f);
  box(c, (int)cx - 1, 1, (int)cx + (big ? 1 : 0), 3, R[2]);
  ellipse(c, cx, 1.2f, big ? 2.0f : 1.6f, 1.0f, R[3]);
  c.set((int)cx, 1, kInk); if (big) c.set((int)cx - 1, 1, kInk);
  c.set(w, h - (big ? 6 : 5), R[1]); c.set(w, h - (big ? 5 : 4), R[1]);
  return c;
}
// a stack of bowls seen from above: their nested rims
Canvas bowlsTop(const Ramp& R) {
  Canvas c(9, 7);
  for (int b = 0; b < 3; b++) {
    hl(c, 1 + b, 7 - b, 5 - b * 2, R[1 + (b == 2)]);
    hl(c, b, 8 - b, 4 - b * 2, R[2 + (b == 2)]);
  }
  ellipse(c, 4.5f, 1.5f, 2.6f, 1.2f, R[3]);
  ellipse(c, 4.5f, 1.7f, 1.6f, 0.7f, R[1]);
  return c;
}
// a round thing in 3/4: a cylinder with its top ellipse (cheese wheels, pots, tubs, buckets)
Canvas drumTop(int w, int h, const Ramp& side, const Ramp& top, bool hollow) {
  Canvas c(w, h + w / 3 + 1);
  const float cx = w * 0.5f, ry = w / 6.0f + 0.4f, ty = ry;
  for (int y = (int)ty; y < c.h; y++)
    for (int x = 0; x < w; x++) {
      const float dx = (x + 0.5f - cx) / (w * 0.5f);
      if (dx * dx > 1.0f) continue;
      const float bottom = ty + h + std::sqrt(std::max(0.0f, 1 - dx * dx)) * ry;
      if (y > bottom) continue;
      const int k = dx < -0.45f ? 3 : (dx < 0.35f ? 2 : 1);
      c.set(x, y, side[k]);
    }
  ellipse(c, cx, ty, w * 0.5f, ry, top[hollow ? 3 : 4]);
  if (hollow) ellipse(c, cx, ty + 0.3f, w * 0.5f - 1.2f, ry - 0.6f, top[0]);
  else ellipse(c, cx + 0.4f, ty + 0.2f, w * 0.5f - 1.4f, ry - 0.5f, top[3]);
  return c;
}

// the counter's goods and what hangs from the rail, by trade: (sprite, u, v) laid on the counter top, (sprite, u) hung
struct Good { Canvas s; float u, v; };
struct Goods { std::vector<Good> lay, hang; };

Goods tradeGoods(int trade, uint32_t seed) {
  Goods g;
  auto lay = [&](Canvas s, float u, float v) { g.lay.push_back({std::move(s), u, v}); };
  auto hang = [&](Canvas s, float u) { g.hang.push_back({std::move(s), u, 0}); };
  const Ramp apples = ramp(rgba(208, 48, 44), 0.8f), oranges = ramp(rgba(240, 150, 44), 0.75f), cabb = ramp(rgba(112, 170, 70), 0.8f),
             plums = ramp(rgba(120, 60, 140), 0.8f), lemons = ramp(rgba(236, 212, 70), 0.7f), pears = ramp(rgba(170, 196, 74), 0.75f);
  switch (trade % kStallTrades) {
    case 0: {   // produce: baskets of apples, cabbages, oranges, plums, lemons; strings of onions, a bunch of herbs
      const Ramp* sets[6] = {&apples, &cabb, &oranges, &plums, &lemons, &pears};
      for (int i = 0; i < 5; i++) lay(basketTop(8, *sets[(i + seed) % 6], seed + i * 7, i % 2 ? 1.8f : 1.4f), 6.0f + i * 9.0f, 5.5f + (i & 1));
      for (int s = 0; s < 3; s++) {
        Canvas on(3, 9);
        const Ramp R = s == 1 ? kBone : ramp(rgba(196, 140, 74));
        vl(on, 1, 0, 1, kWood[1]);
        for (int k = 0; k < 3; k++) ball(on, 1.5f + (k & 1) * 0.4f, 2.8f + k * 2.0f, 1.4f, 1.2f, R, 0.04f);
        hang(on, s == 2 ? 41.0f : 5.0f + s * 5.0f);
      }
      Canvas herb(5, 7);
      for (int k = 0; k < 5; k++) vl(herb, k, 0, 4 + (k & 1) + (k == 2), k % 2 ? kLeaf[2] : kLeaf[3]);
      hl(herb, 1, 3, 0, kWood[1]);
      hang(herb, 36.0f);
      break;
    }
    case 1: {   // fish laid across a bed of ice (the counter's top), a big salmon in the middle; dried fish hung by the tail
      const Ramp silver = ramp5(rgba(46, 60, 92), rgba(84, 108, 138), rgba(140, 162, 182), rgba(196, 210, 220), rgba(240, 246, 250));
      const Ramp mack = ramp5(rgba(30, 60, 70), rgba(52, 100, 108), rgba(108, 150, 150), rgba(176, 204, 196), rgba(232, 240, 230));
      const Ramp salmon = ramp(rgba(226, 120, 96), 0.75f);
      for (int i = 0; i < 7; i++) {
        if (i == 3) continue;
        lay(fishTop(7, i % 3 == 1 ? mack : silver, (i + seed) % 2 != 0), 4.5f + i * 6.4f, 4.0f + (i & 1) * 3.0f);
      }
      lay(fishTop(11, salmon, false), 24.0f, 6.0f);
      const Ramp dried = ramp(rgba(176, 140, 92), 0.75f);
      for (int s = 0; s < 4; s++) {
        Canvas f(4, 9);
        f.set(1, 0, dried[1]); f.set(2, 0, dried[1]);
        for (int j = 1; j < 9; j++) {
          const int w = j < 2 ? 1 : (j < 7 ? 2 : 1);
          for (int i = 2 - w; i < 2 + w; i++) f.set(i, j, dried[i < 1 ? 3 : (i == 1 ? 3 : (i == 2 ? 2 : 1))]);
        }
        hang(f, s < 2 ? 5.0f + s * 5.0f : 36.0f + (s - 2) * 6.0f);
      }
      break;
    }
    case 2: {   // cloth: bolts lying in a stack, folded lengths, more bolts; lengths of cloth hanging in folds
      static const uint32_t cl[6] = {rgba(64, 92, 176), rgba(190, 58, 58), rgba(228, 196, 96), rgba(64, 140, 96), rgba(150, 76, 156), rgba(214, 120, 64)};
      for (int b = 0; b < 3; b++) lay(boltTop(11, cl[(b + seed) % 6]), 9.0f, 3.5f + b * 2.6f);
      lay(foldTop(8, 3, seed + 1), 21.0f, 6.0f);
      lay(foldTop(8, 4, seed + 3), 30.0f, 5.5f);
      for (int b = 0; b < 2; b++) lay(boltTop(10, cl[(b * 3 + seed + 2) % 6]), 40.0f, 4.5f + b * 3.0f);
      for (int s = 0; s < 4; s++) {
        const Ramp R = ramp(cl[(s + seed + 4) % 6], 0.7f);
        const int len = 9 + (int)((s * 5 + seed) % 3);
        Canvas d(5, len);
        for (int j = 0; j < len; j++)
          for (int i = 0; i < 5; i++) {
            if (j == len - 1 && (i == 0 || i == 4)) continue;
            d.set(i, j, R[i == 0 ? 3 : (i == 4 ? 1 : ((i + j / 3) % 2 ? 2 : 3))]);
          }
        hang(d, s < 2 ? 5.0f + s * 6.0f : 37.0f + (s - 2) * 6.0f);
      }
      break;
    }
    case 3: {   // pottery: big jugs and stacked bowls; little jugs hung by their handles
      const Ramp terra = ramp(rgba(190, 102, 60), 0.8f), glaze = ramp(rgba(60, 110, 170), 0.8f), green = ramp(rgba(96, 140, 88), 0.8f),
                 cream = ramp(rgba(220, 204, 170), 0.6f);
      const Ramp* rs[4] = {&terra, &glaze, &green, &cream};
      for (int i = 0; i < 6; i++) {
        const Ramp& R = *rs[(i + seed) % 4];
        if (i % 2 == 0) lay(jugTop(R, i != 2), 5.0f + i * 7.6f, 6.0f);
        else lay(bowlsTop(R), 5.0f + i * 7.6f, 5.0f);
      }
      for (int s = 0; s < 3; s++) {
        Canvas j = jugTop(*rs[(s + seed + 1) % 4], false);
        hang(j, s == 2 ? 40.0f : 6.0f + s * 6.0f);
      }
      break;
    }
    case 4: {   // meat: a board of cuts, a block with its cleaver, a coil of sausages, a cheese wheel; hams and sausages hung
      const Ramp ham = ramp(rgba(176, 72, 60), 0.8f), saus = ramp(rgba(150, 70, 54), 0.7f);
      Canvas board(14, 6);
      for (int y = 0; y < 6; y++) for (int x = 0; x < 14; x++) board.set(x, y, kWood[y == 5 ? 1 : (y == 0 ? 4 : 3)]);
      for (int i = 0; i < 3; i++) {
        box(board, 1 + i * 4, 1, 3 + i * 4, 3, ham[3]);
        hl(board, 1 + i * 4, 3 + i * 4, 1, kBone[3]);
        board.set(3 + i * 4, 3, ham[1]);
      }
      lay(board, 9.0f, 5.5f);
      Canvas block = crateTop(7, 4, 4, nullptr, seed);
      for (int y = 1; y < 3; y++) for (int x = 1; x < 6; x++) block.set(x, y, kWood[3]);
      box(block, 3, 0, 5, 1, kIron[3]); block.set(2, 0, kWood[2]);
      lay(block, 22.0f, 6.0f);
      Canvas coil(8, 5);
      for (int a = 0; a < 24; a++) {
        const float ang = a * 0.2618f, r = 1.0f + a * 0.11f;
        coil.set(4 + (int)std::lround(std::cos(ang) * r), 2 + (int)std::lround(std::sin(ang) * r * 0.6f), saus[a % 3 == 0 ? 3 : 2]);
      }
      lay(coil, 30.5f, 6.0f);
      lay(drumTop(7, 3, ramp(rgba(214, 150, 62), 0.7f), ramp(rgba(240, 206, 110), 0.6f), false), 39.0f, 5.5f);
      for (int s = 0; s < 2; s++) {
        Canvas h(7, 10);
        h.set(3, 0, kIron[3]);
        ball(h, 3.5f, 5.5f, 3.0f, 4.0f, ham, 0.04f);
        h.set(3, 1, kBone[3]); h.set(4, 1, kBone[2]);
        hang(h, s ? 41.0f : 6.0f);
        Canvas ss(3, 9);
        for (int k = 0; k < 3; k++) ball(ss, 1.5f, 1.5f + k * 2.6f, 1.2f, 1.4f, saus, 0.03f);
        hang(ss, s ? 35.0f : 12.0f);
      }
      break;
    }
    case 5: {   // bread: a basket of rolls, round cobs, long loaves, another basket, a sack of flour; pretzels hung
      const Ramp crust = ramp(rgba(204, 138, 64), 0.8f), dark = ramp(rgba(150, 92, 50), 0.75f);
      lay(basketTop(9, crust, seed, 1.6f), 6.0f, 6.0f);
      for (int i = 0; i < 2; i++) {
        Canvas cob(7, 5);
        ball(cob, 3.5f, 2.6f, 3.2f, 2.2f, (i + seed) % 2 ? dark : crust, 0.04f);
        hl(cob, 2, 4, 2, crust[4]);
        lay(cob, 15.0f + i * 6.0f, 4.0f + i * 3.0f);
      }
      for (int l = 0; l < 3; l++) {
        Canvas lf(11, 3);
        for (int x = 0; x < 11; x++) { lf.set(x, 0, crust[x == 0 ? 2 : 3]); lf.set(x, 1, crust[2]); lf.set(x, 2, crust[1]); }
        for (int k = 0; k < 3; k++) lf.set(2 + k * 3, 0, crust[4]);
        lay(lf, 30.0f + (l & 1), 3.5f + l * 2.4f);
      }
      lay(basketTop(8, dark, seed + 5, 1.7f), 39.5f, 6.0f);
      Canvas sack(6, 8);
      ball(sack, 3.0f, 4.5f, 2.8f, 3.4f, kCloth, 0.05f);
      hl(sack, 2, 4, 1, kWood[1]);
      lay(sack, 45.0f, 7.5f);
      for (int s = 0; s < 3; s++) {
        Canvas p(7, 7);
        p.set(3, 0, kWood[2]);
        for (int a = 0; a < 16; a++) {
          const float ang = a * 0.3927f;
          p.set(3 + (int)std::lround(std::cos(ang) * 2.6f), 4 + (int)std::lround(std::sin(ang) * 2.2f), crust[a < 8 ? 2 : 3]);
        }
        p.set(3, 4, crust[1]);
        hang(p, s == 2 ? 41.0f : 6.0f + s * 6.0f);
      }
      break;
    }
    case 7: {   // timber: a stack of planks, bundles of kindling, a log lying with its rings; an axe, a bow saw, a rope coil hung
      const Ramp pale = ramp(rgba(214, 176, 120), 0.7f), bark = ramp(rgba(112, 78, 52), 0.75f), rope = ramp(rgba(196, 168, 112), 0.7f);
      Canvas pl(15, 8);
      for (int j = 0; j < 4; j++)
        for (int x = 0; x < 15; x++) {
          const int y = 1 + j;
          pl.set(x, y, pale[j == 0 ? 4 : 3]);
        }
      for (int j = 0; j < 3; j++) for (int x = 0; x < 15; x++) pl.set(x, 5 + j, pale[x == 0 ? 3 : (j == 1 ? 1 : 2)]);
      for (int x = 0; x < 15; x++) pl.set(x, 0, pale[3]);
      lay(pl, 10.0f, 6.0f);
      for (int b = 0; b < 2; b++) {
        Canvas k(6, 6);
        for (int i = 0; i < 6; i++)
          for (int j = 0; j < 6; j++) k.set(i, j, pale[(i + j + (int)seed) % 3 == 0 ? 2 : (j < 2 ? 4 : 3)]);
        vl(k, 2, 0, 5, rope[1]);
        lay(k, 22.0f + b * 7.0f, 5.0f + b * 2.0f);
      }
      Canvas lg(12, 7);
      for (int x = 0; x < 12; x++)
        for (int y = 0; y < 7; y++) lg.set(x, y, bark[y <= 1 ? 3 : (y >= 5 ? 1 : 2)]);
      ellipse(lg, 1.5f, 3.5f, 2.5f, 3.4f, pale[3]);
      lg.set(1, 3, pale[1]);
      lay(lg, 39.0f, 7.0f);
      Canvas ax(6, 12);
      vl(ax, 3, 0, 11, kWood[3]);
      box(ax, 0, 1, 2, 4, kIron[2]); vl(ax, 0, 1, 4, kIron[4]);
      hang(ax, 6.0f);
      Canvas saw(11, 7);
      for (int a = 0; a <= 10; a++) saw.set(a, 3 - (int)std::lround(std::sin(a * 0.314f) * 3.0f), kWood[2]);
      hl(saw, 0, 10, 5, kIron[3]);
      for (int x = 0; x <= 10; x += 2) saw.set(x, 6, kIron[1]);
      vl(saw, 0, 2, 5, kWood[3]); vl(saw, 10, 2, 5, kWood[1]);
      hang(saw, 14.0f);
      Canvas co(7, 8);
      co.set(3, 0, kWood[2]);
      for (int a = 0; a < 18; a++) {
        const float ang = a * 0.349f;
        co.set(3 + (int)std::lround(std::cos(ang) * 3.0f), 4 + (int)std::lround(std::sin(ang) * 2.6f), rope[a < 9 ? 3 : 1]);
      }
      hang(co, 40.0f);
      break;
    }
    default: {   // tools: horseshoes, knives on a cloth, an iron pot, a box of nails; hammer, axe, sickle, pick hung
      Canvas hs(12, 5);
      for (int i = 0; i < 3; i++)
        for (int a = 0; a < 7; a++) hs.set(2 + i * 4 + (int)std::lround(std::cos(a * 0.52f) * 1.6f), 3 - (int)std::lround(std::sin(a * 0.52f) * 1.6f), kIron[a < 3 ? 4 : 2]);
      lay(hs, 8.0f, 5.0f);
      Canvas kn(11, 6);
      for (int y = 0; y < 6; y++) for (int x = 0; x < 11; x++) kn.set(x, y, kCloth[y == 0 ? 4 : 3]);
      for (int i = 0; i < 3; i++) { hl(kn, 1 + i * 3, 2 + i * 3, 2, kIron[4]); hl(kn, 1 + i * 3, 2 + i * 3, 3, kIron[2]); kn.set(1 + i * 3, 4, kWood[1]); }
      lay(kn, 19.0f, 6.0f);
      lay(drumTop(8, 4, kIron, kIron, true), 30.0f, 6.0f);
      Canvas nb = crateTop(7, 4, 3, &kIron, seed);
      lay(nb, 40.0f, 6.0f);
      auto tool = [&](int kind) {
        Canvas t(6, 12);
        vl(t, 3, 1, 11, kWood[3]);
        if (kind == 0) { box(t, 1, 0, 5, 2, kIron[3]); hl(t, 1, 5, 0, kIron[4]); }
        else if (kind == 1) { box(t, 4, 0, 5, 4, kIron[2]); vl(t, 5, 0, 4, kIron[4]); }
        else if (kind == 2) { for (int a = 0; a < 8; a++) t.set(3 + (int)std::lround(std::cos(a * 0.4f) * 2.6f), 3 - (int)std::lround(std::sin(a * 0.4f) * 2.6f), kIron[3]); }
        else { hl(t, 0, 5, 1, kIron[3]); t.set(0, 2, kIron[2]); t.set(5, 2, kIron[2]); }
        return t;
      };
      hang(tool(0), 5.0f); hang(tool(1), 10.0f); hang(tool(2), 37.0f); hang(tool(3), 42.0f);
      break;
    }
  }
  return g;
}

// ================================================================================================ the stall
// model constants (model space px). The roof's front edge stands over the counter's back edge, high enough that the
// keeper (behind the counter) shows from the chest up under it; the counter's top is a broad, lit shelf of goods.
constexpr float CT = 11.0f;                     // the counter's top
constexpr float CU0 = 1.0f, CU1 = 47.0f;        // the counter's ends
constexpr float CV0 = 1.0f, CV1 = 12.0f;        // the counter's front (customers) .. back (keeper) edges
constexpr float VH = 3.5f, VT = 1.6f;           // the valance: its drop, and how far its foot blows out
// the roof's geometry. Front and back views (S / N): the roof stands over the keeper's half, its front edge over the
// counter's back edge, high enough that the keeper shows under it. (stalls fixer round 1, "the side stalls read as tall
// banners") Side profiles (E / W): seen from above-and-south the roof is all we see of the stall's length, so it is
// as wide on screen as the stall is deep, lower (its south end shows only a short end face over the keeper's head),
// and short of the counter's ends so two stalls in a column keep a gap between their roofs. (stalls fixer round 2, "the
// side stalls still read as a tall striped strip or a hanging curtain") A side roof is steeply pitched (12 px from its
// low customers' edge to its high back: the slope shows as a clear shear on screen and in the narrow end panel at its
// south end), stops short of the counter's front so the counter and its goods stand out past its low edge in the sun,
// and its front posts stand on the counter (short), not on the ground
struct Geo {
  bool side;
  float ru0, ru1;        // the roof's ends (u)
  float rvf, rvb;        // the cloth / timber roof's front and back edges (v)
  float rzf, rzb;        // the cloth roof's heights there (a mono-pitch, high at the back)
  float tvf, tvr, tvb;   // the tent: its front eave, ridge and back eave (v)
  float tze, tzr;        // its eaves' and ridge's heights
  float bvf, bzf, bzb;   // the timber booth: its front eave (v), its heights front and back
};
// (stalls fixer round 3, owner: "why are the vendors standing on the end?") The keeper stands behind the middle of the
// counter in every facing:
//  - side profiles (E / W): (awnings fixer, owner: "now the awnings are too small. Don't worry if it covers the NPC")
//    the side roof is a full awning again, as wide on screen as the stall is deep: from just behind the counter's
//    customers' strip (v 9, the booth's eave v 7; low, z 16) over the rest of the counter and the keeper's column to
//    past their floor (v 33, high, z 25), with the visual weight of a front-facing stall's roof. It covers the keeper
//    (who stays in the middle of their column, v 24, and sorts before the stall: art_props.h stallSortY); the
//    counter's customers' strip and the goods drawn up to its lip lie in the sun past the low edge's scalloped flap.
//    It runs from the counter's south end to 13 px short of its north end (u -0.4 .. 34): its north edge, lifted by
//    its height (u + z <= 60, the outline included), stops a couple of px short of the next stall's counter and the
//    feet of its south posts up the column (a side row steps four tiles), so no roof slices a neighbour's counter or
//    hides a neighbour's post;
//  - the back view (N): the keeper stands in their row a little behind the counter (v 21), the back of the roof is
//    pulled in (cloth and booth v 28, the tent's back eave v 24.5 and raised) and the back wall left off, so we see
//    them from behind under the roof's high back edge;
//  - the front view (S) is unchanged: the keeper shows from the chest up under the roof's front edge.
// (M2 fixer round 3, review: "side-facing stalls render as an unreadable jumble: a tall capsule or a plank panel")
// In this projection a roof that falls east or west shears into a slanted panel, and the side stall's long cloth read
// as a door or a book standing on end. A side profile's cloth awning is now nearly flat (z 22 to 23: its widths run
// across the screen, its scalloped valance hangs across its south end, the one face we see square-on); the tent's
// ridge stands over the middle of the stall's depth (v 21) with one slope lit and one in shade; the timber booth
// has a gabled roof along the counter (sideBoothRoof: level eaves at z 19, the ridge 6 px higher, a boarded gable
// end to the south).
Geo geoOf(int facing) {
  if (facing == StallE || facing == StallW) return {true, -0.4f, 34.0f, 9.0f, 33.0f, 22.0f, 23.0f, 9.0f, 21.0f, 33.0f, 19.0f, 24.0f, 7.0f, 19.0f, 19.0f};
  if (facing == StallN) return {false, -1.0f, 49.0f, 10.0f, 28.0f, 30.0f, 36.0f, 10.0f, 18.0f, 24.5f, 31.0f, 36.5f, 9.5f, 30.0f, 37.0f};
  return {false, -1.0f, 49.0f, 10.0f, 35.0f, 30.0f, 36.0f, 10.0f, 23.0f, 36.0f, 29.0f, 36.5f, 9.5f, 30.0f, 37.0f};
}
// the posts of a stall's form (model boxes u0 u1 v0 v1 z0 z1): every one stands on the ground under a roof corner (or
// the tent's gables). Shared by the painter and the ground shadow, so each post's foot has its own little shade
struct Post { float u0, u1, v0, v1, z0, z1; };
std::vector<Post> stallPosts(int form, int facing) {
  const Geo g = geoOf(facing);
  std::vector<Post> P;
  if (g.side) {   // the corners of the roof: the south pair stands clear of the counter's end, the north pair under the roof
    const float t = form == 2 ? 2.2f : (form == 1 ? 1.4f : 1.5f);
    const float zf = form == 1 ? g.tze : (form == 2 ? g.bzf : g.rzf), zb = form == 1 ? g.tze : (form == 2 ? g.bzb : g.rzb);
    const float vf = form == 1 ? g.tvf : (form == 2 ? g.bvf : g.rvf), vb = form == 1 ? g.tvb : g.rvb;
    for (float u : {g.ru0, g.ru1 - t}) {
      P.push_back({u, u + t, vf - t * 0.5f, vf + t * 0.5f, 0, zf});
      P.push_back({u, u + t, vb - t, vb, 0, zb});
      if (form == 1) P.push_back({u, u + t, g.tvr - t * 0.5f, g.tvr + t * 0.5f, 0, g.tzr + 1.0f});   // the gables' ridge poles
    }
    return P;
  }
  switch (form) {
    case 1:
      for (float pu : {g.ru0 + 2.0f, g.ru1 - 3.0f}) {
        P.push_back({pu, pu + 1.4f, g.tvf + 0.6f, g.tvf + 2.0f, 0, g.tze});
        P.push_back({pu, pu + 1.4f, g.tvb - 2.0f, g.tvb - 0.6f, 0, g.tze});
      }
      for (float pu : {g.ru0 + 0.4f, g.ru1 - 1.8f}) P.push_back({pu, pu + 1.4f, g.tvr - 0.7f, g.tvr + 0.7f, 0, g.tzr + 1.0f});
      break;
    case 2:
      for (float pu : {g.ru0 + 2.0f, g.ru1 - 4.5f}) {
        P.push_back({pu, pu + 2.5f, g.bvf + 1.0f, g.bvf + 3.5f, 0, g.bzf});
        P.push_back({pu, pu + 2.5f, g.rvb - 2.5f, g.rvb, 0, g.bzb});
      }
      break;
    default:
      for (float pu : {g.ru0 + 2.5f, g.ru1 - 4.0f}) {
        P.push_back({pu, pu + 1.6f, g.rvf + 1.0f, g.rvf + 2.4f, 0, g.rzf});
        P.push_back({pu, pu + 1.6f, g.rvb - 1.6f, g.rvb - 0.2f, 0, g.rzb});
      }
      break;
  }
  return P;
}
// a post: its box, and a darker foot where it meets the ground (a side profile's posts also wear a little cap)
void paintPosts(M3& m, int form) {
  for (const Post& p : stallPosts(form, m.f)) {
    boxM(m, p.u0, p.u1, p.v0, p.v1, p.z0, p.z1, kWood, [&](int fc, float, float t, int k, uint32_t c) -> uint32_t {
      if (fc != 0 && t * (p.z1 - p.z0) < 1.2f) return kWoodDark[std::max(0, k - 2)];   // the foot, dark on the paving
      return c;
    });
  }
}
constexpr float RAIL = 11.0f;                   // the rail the goods hang from (v)
constexpr float KV = 25.0f;                     // the keeper's row (v), the stool and the crates

struct StallCanvas { int x0, y0, w, h; };
StallCanvas stallCanvas(int facing) {
  switch (facing) {
    // (stalls fixer round 3) a margin all round: the cloth booth's valance ends and their outline reached x0 before
    case StallN: return {-23, -26, 62, 68};
    case StallE: return {-24, -64, 46, 85};
    case StallW: return {-6, -64, 46, 85};
    default: return {-23, -62, 62, 82};
  }
}

// the counter: a box of boards (form 0, 2), or a trestle under a linen cloth falling to a hem (form 1). trade: the
// fish stall's top is a bed of crushed ice
void stallCounter(M3& m, const Awn& A, int form, int trade, bool closed) {
  const Ramp linen = ramp(A.striped ? A.b : rgba(232, 222, 196), 0.6f), trim = ramp(A.striped ? A.a : A.b, 0.8f);
  const bool skirt = form == 0 && (trade == 2 || trade == 5 || trade == 3);
  const Ramp sk = ramp(A.striped ? A.a : A.b, 0.8f);
  const bool ice = !closed && trade == 1;
  boxM(m, CU0, CU1, CV0, CV1, 0, CT, kWood, [&](int fc, float s, float t, int k, uint32_t c) -> uint32_t {
    const float u = CU0 + s * (CU1 - CU0);
    if (fc == 0) {   // the top
      const float v = CV0 + t * (CV1 - CV0);
      if (ice) {
        int kk = ((int)(u * 1.7f) * 5 + (int)(v * 1.3f) * 3) % 7 == 0 ? 4 : 3;
        if (v > CV1 - 2.5f) kk = 2;
        if (v < CV0 + 0.6f) kk = 2;
        return kSnow[kk];
      }
      if (form == 1) return linen[v < CV0 + 0.8f ? 4 : (v > CV1 - 2.5f ? 2 : 3)];
      int kk = k;
      if (v < CV0 + 0.8f) kk = 4;                           // the lit front edge
      else if (((int)std::floor(v) % 4) == 0) kk = k - 1;   // the boards' joints, running along
      if (v > CV1 - 2.5f) kk = std::min(kk, 2);             // under the roof's edge
      return kWood[kk];
    }
    if (fc == 1 || fc == 3 || fc == 4) {   // the front and the ends
      const float z = t * CT, along = fc == 1 ? u : CV0 + s * (CV1 - CV0);
      if (form == 1) {   // the cloth falls in folds to a coloured hem and a fringe; the legs show under it
        if (z < 1.6f) return ((int)along % 9 == 2 || (int)along % 9 == 3) ? kWoodDark[2] : 0;
        if (z < 2.6f) return ((int)along % 3) ? trim[1] : 0;
        const int f = (int)along % 7;
        int kk = f == 0 ? 1 : (f < 3 ? 3 : (f < 5 ? 2 : 1));
        if (fc != 1) kk = std::max(1, kk - 1);
        if (z > CT - 0.9f) kk = 1;
        return z < 5.2f ? trim[kk >= 2 ? 2 : 1] : linen[kk];
      }
      if (skirt && fc == 1 && z > CT - 6.5f && z < CT - 0.8f) {
        if (z < CT - 5.5f && ((int)u % 4) >= 2) return kWood[1];   // a zigzag hem
        return sk[z < CT - 5.5f ? 1 : (((int)u / 4) % 2 ? 2 : 3)];
      }
      int kk = ((int)along % 5 == 0) ? 1 : 2;
      if (fc == 3 || fc == 4) kk = std::max(1, kk);
      if (z > CT - 0.9f) kk = 1;     // under the top's lip
      if (z < 1.2f) kk = 0;          // the dark foot
      return kWood[kk];
    }
    if (fc == 2 && form != 1) {
      // (fixer M5 r3, review: "the counter's back is one flat brown slab") the keeper's side, seen from behind (N): a
      // frame of posts between bays, two open shelves of stock under the top - jugs, baskets, sacks and bottles in the
      // shade of the counter - each shelf's front lip catching a little light
      const float z = t * CT, u = CU0 + s * (CU1 - CU0);
      const float bu = std::fmod(u - CU0, 15.33f);
      if (u < CU0 + 1.3f || u > CU1 - 1.3f || bu < 1.1f) return kWood[u < CU0 + 0.6f || bu < 0.5f ? 2 : 1];   // the posts
      if (z < 1.2f) return kWood[0];                       // the dark foot
      if (z > CT - 1.4f) return z > CT - 0.6f ? kWood[2] : kWood[1];   // the top's board, seen edge on
      if ((z >= 1.2f && z < 2.0f) || (z >= 5.4f && z < 6.3f)) return z < 2.0f ? kWood[2] : kWood[3];   // the shelves' lips
      // the stock: a thing per 4 px along each shelf (some gaps), its kind and colour by hash; deep shade behind it
      const bool upper = z >= 6.3f;
      const float z0 = upper ? 6.3f : 2.0f, hz = z - z0;
      const int cell = (int)std::floor((u - CU0) / 4.0f);
      const float cu = (u - CU0) - cell * 4.0f;   // 0..4 across the cell
      const uint32_t h = hash3(cell, upper ? 7 : 3, 5381u + (uint32_t)trade * 31u);
      const int kind = (int)(h % 6u);
      const uint32_t shade = kWoodDark[hz > 2.6f ? 0 : 1];
      if (kind == 5 || cu < 0.5f || cu > 3.6f) return shade;   // a gap on the shelf
      static const uint32_t jug[3] = {rgba(112, 52, 36), rgba(160, 82, 50), rgba(196, 116, 70)};
      static const uint32_t bas[3] = {rgba(110, 74, 38), rgba(150, 106, 56), rgba(188, 144, 80)};
      static const uint32_t sack[3] = {rgba(120, 100, 76), rgba(164, 142, 108), rgba(204, 186, 148)};
      static const uint32_t bot[3] = {rgba(34, 70, 52), rgba(52, 104, 72), rgba(110, 160, 118)};
      const uint32_t* R = kind == 0 || kind == 1 ? jug : kind == 2 ? bas : kind == 3 ? sack : bot;
      const float top = kind == 4 ? 3.0f : (kind == 2 ? 2.0f : 2.6f);   // its height on the shelf
      if (hz > top) return shade;
      const float cx = (cu - 2.05f) / 1.55f;   // -1..1 across it
      if (kind == 4 && std::fabs(cx) > 0.45f && hz > 1.7f) return shade;   // a bottle's neck
      if ((kind == 0 || kind == 1 || kind == 3) && hz > top - 0.7f && std::fabs(cx) > 0.7f) return shade;   // round shoulders
      if (kind == 2 && hz > top - 0.6f) return bas[2];                  // the basket's rim
      if (kind == 2 && ((int)std::floor(cu * 2.0f + hz * 2.0f) & 1)) return bas[0];   // the weave
      return cx < -0.35f ? R[2] : (cx > 0.45f ? R[0] : R[1]);      // lit on its west side
    }
    return c;
  });
}

// the trade's goods on the counter and under the rail
void stallGoods(M3& m, int trade, uint32_t seed) {
  Goods g = tradeGoods(trade, seed);
  for (Good& x : g.lay) {
    // what lies in the roof's shade (the counter's back strip) takes a little of it
    // (side profiles: the whole counter stands under the roof)
    // (side profiles: what lies under the roof, past its low edge; the customers' strip is in the sun)
    // (awnings fixer) a side profile's full awning hides every column past its low edge from the camera, so each good
    // is drawn up to the counter's customers' lip: its near edge on the lip, most of it in the sunlit strip
    if (geoOf(m.f).side) x.v = std::min(x.v, CV0 + x.s.w * 0.5f);
    const bool shadeG = geoOf(m.f).side ? x.v > geoOf(m.f).rvf - 1.0f : x.v > CV1 - 3.0f;
    if (shadeG)
      for (auto& p : x.s.px) if (chA(p)) p = darken(p, x.v > CV1 - 3.0f ? 0.18f : 0.1f);
    put(m, x.s, x.u, x.v, CT, false, 0.8f);
  }
  // (a side profile's rail runs under its roof, out of sight; its goods would only hang in front of the keeper)
  if (!geoOf(m.f).side)
    for (Good& x : g.hang) {
      for (auto& p : x.s.px) if (chA(p)) p = darken(p, 0.16f);
      put(m, x.s, x.u, RAIL, geoOf(m.f).rzf - 1.5f, true, 0.4f);
    }
}

// the keeper's corner under the roof: a stool, stacked crates, a sack (seen from behind, below the back cloth)
void stallBackStock(M3& m, int trade, uint32_t seed) {
  // the stool: a round seat on three splayed legs (fixer M5 r3: "crude boxes" - the seat is a thin disc of a cushion
  // pad now, its legs thin and dark under it)
  boxM(m, 6.2f, 6.9f, KV - 0.6f, KV + 0.1f, 0, 5.0f, kWoodDark);
  boxM(m, 9.1f, 9.8f, KV - 0.6f, KV + 0.1f, 0, 5.0f, kWoodDark);
  boxM(m, 7.6f, 8.3f, KV + 2.2f, KV + 2.9f, 0, 5.0f, kWoodDark);
  boxM(m, 6.6f, 9.4f, KV + 0.8f, KV + 1.4f, 2.2f, 2.8f, kWoodDark);   // a rung
  boxM(m, 5.4f, 10.6f, KV - 1.2f, KV + 3.4f, 5.0f, 5.9f, kWood, [](int fc, float s, float t, int k, uint32_t c) -> uint32_t {
    if (fc == 0) {   // the round seat: its corners cut off, a lit rim on the west, worn smooth in the middle
      const float a = (s - 0.5f) * 2.0f, b = (t - 0.5f) * 2.0f;
      if (a * a + b * b > 1.25f) return 0;
      return a < -0.55f || b < -0.6f ? kWood[4] : (a > 0.55f || b > 0.6f ? kWood[2] : kWood[3]);
    }
    return (s < 0.12f || s > 0.88f) ? 0 : c;
  });
  // crates side by side, slatted, and a smaller one on them; a sack by them
  auto crate = [](int fc, float s, float t, int k, uint32_t c) -> uint32_t {
    (void)k;
    if (fc == 0) {   // the open top: slats across, the stock's darkness between them, a lit rim
      if (s < 0.09f || s > 0.91f || t < 0.12f || t > 0.88f) return kWood[s < 0.09f || t < 0.12f ? 4 : 3];
      return ((int)std::floor(t * 5.0f) & 1) ? kWoodDark[1] : kWood[2];
    }
    // the sides: corner posts, two slats with a dark gap, the maker's brace
    if (s < 0.1f || s > 0.9f) return kWoodDark[2];
    if ((t > 0.44f && t < 0.56f) || t < 0.08f) return kWoodDark[1];
    if (t > 0.9f) return kWood[3];
    return c;
  };
  boxM(m, 36.0f, 42.0f, KV + 2.0f, KV + 8.0f, 0, 6.5f, kWood, crate);
  boxM(m, 42.6f, 47.0f, KV + 2.5f, KV + 7.5f, 0, 5.5f, kWood, crate);
  boxM(m, 37.5f, 41.5f, KV + 3.0f, KV + 7.0f, 6.5f, 10.5f, kWood, crate);
  Canvas sack(7, 9);
  const bool flour = trade == 5 || trade == 0;
  ball(sack, 3.5f, 5.0f, 3.0f, 3.6f, flour ? kCloth : ramp(rgba(150, 120, 86), 0.6f), 0.05f);
  hl(sack, 2, 5, 1, kWood[1]);
  put(m, sack, 32.0f, KV + 6.0f, 0, false, 0.5f);
  (void)seed;
}

// form 0: the cloth booth. A striped (or plain) awning on four posts, sloping down to the customers, a scalloped
// valance round it; a back cloth hung from the back edge to above the keeper's head
void clothRoof(M3& m, const Awn& A, int awning) {
  const Geo g = geoOf(m.f);
  const Ramp RA = ramp(A.a, 0.85f), RB = ramp(A.b, 0.7f);
  const bool zig = (awning & 2) != 0;
  auto cloth = [&](float u) -> const Ramp& {
    const int stripe = (int)std::floor((u + 1.0f) / 4.0f) & 1;
    return A.striped ? (stripe ? RB : RA) : RA;
  };
  // the posts, every one on the ground (stallPosts)
  paintPosts(m, 0);
  // the top: stripes run down the slope; the back edge catches the light, the cloth sags between the spars and rolls
  // over into the valance at the front
  quad(m, g.ru0, g.rvf, g.rzf, g.ru1 - g.ru0, 0, 0, 0, g.rvb - g.rvf, g.rzb - g.rzf, 0, -(g.ru1 - g.ru0) * (g.rzb - g.rzf), (g.ru1 - g.ru0) * (g.rvb - g.rvf),
       [&](float s, float t, float l) -> uint32_t {
         const float u = g.ru0 + s * (g.ru1 - g.ru0);
         float ll = l - std::sin(t * 3.14159f * 2.0f) * 0.035f;   // a sag over the middle spar
         if (t > 0.94f) ll += 0.12f;
         if (t < 0.05f) ll -= 0.14f;
         if (s < 0.02f || s > 0.98f) ll -= 0.05f;
         if (!A.striped) {   // a plain cloth: sewn widths (seams down the slope) and a hem in the trim colour
           // (a side profile: the hem on the low edge only, so the long cloth never reads as a framed panel)
           if ((!g.side && (s < 0.035f || s > 0.965f)) || t < 0.08f) return RB[t < 0.04f ? 1 : 2];
           if (((int)std::floor(u + 1.0f)) % 8 == 0) ll -= g.side ? 0.16f : 0.09f;
           // (a side profile) the widths sag a little between the seams: a soft shade by each seam, the light between
           if (g.side) ll -= 0.06f * std::fabs(std::cos((u + 1.0f) * 3.14159f / 8.0f));
         }
         return cloth(u)[t > 0.95f ? 4 : litC(ll, (int)(s * 50), (int)(t * 25))];
       });
  // the valance along the front and round the ends and back, its foot blowing out a little (so it shows from above,
  // the scallops too, whichever way the stall faces): scallops (or zigzag points) along its foot
  auto valance = [&](float along, float zDown) -> uint32_t {   // zDown: 0 at the top .. VH at the foot
    const int inC = ((int)std::floor(along + 100.0f)) % 4;
    if (zDown > VH - 1.4f) {
      if (zig ? (zDown > VH - 0.7f ? (inC == 0 || inC == 3) : inC == 3) : (zDown > VH - 0.7f ? (inC != 1 && inC != 2) : inC == 3)) return 0;
    }
    const Ramp& R = A.striped ? cloth(along) : (zDown > 1.0f && zDown < 2.0f ? RB : RA);
    return R[zDown < 0.8f ? 3 : (zDown > VH - 1.5f ? 1 : 2)];
  };
  // (a side profile's long low edge is all of its valance we see from above: its foot blows out further, so the row of
  // scallops runs down the customers' side and the cloth reads as an awning, not a hanging)
  // (stalls fixer round 2) a side profile's customers' edge is the strongest hem of all: a broad flap with deep
  // scallops in the trim colour, so the low edge (east or west) reads as the stall's front
  // (awnings fixer: a broad flap again, three px out from the low edge, so its scallops read from above; the goods lie
  // on the sunlit strip in front of it)
  const float vtF = g.side ? 3.0f : VT;
  auto sideHem = [&](float along, float zDown) -> uint32_t {
    // (awnings fixer) one round scallop (or point) per stripe, centred on it, so the customers' edge reads as a row of
    // tongues down the stall's length, as on a front-facing valance
    const int inC = ((int)std::floor(along + 100.0f)) % 4;
    if (zDown > VH - 0.8f && (zig ? inC != 1 : (inC == 0 || inC == 3))) return 0;
    if (zDown > VH - 1.6f && inC == 3) return 0;
    if (zDown < 0.7f) return (A.striped ? cloth(along) : RA)[3];                                  // the roll of the cloth
    if (zDown < 1.3f) return kWoodDark[1];                                                        // a dark seam line
    return (A.striped ? cloth(along) : RB)[zDown > VH - 0.8f ? 1 : (zDown > VH - 1.6f ? 2 : 3)];   // the stripes run on into the hem
  };
  quad(m, g.ru0, g.rvf - vtF, g.rzf - VH, g.ru1 - g.ru0, 0, 0, 0, vtF, VH, 0, -VH, vtF, [&](float s, float t, float) {
    const float a = g.ru0 + s * (g.ru1 - g.ru0);
    return g.side ? sideHem(a, VH * (1 - t)) : valance(a, VH * (1 - t));
  });
  // (stalls fixer round 1) the back edge (the near edge of a stall seen from behind) is the high one: a straight hem
  // there, no scallops (they hang on the customers' side)
  quad(m, g.ru0, g.rvb + VT * 0.6f, g.rzb - 2.0f, g.ru1 - g.ru0, 0, 0, 0, -VT * 0.6f, 2.0f, 0, 2.0f, VT * 0.6f, [&](float s, float t, float) -> uint32_t {
    const float u = g.ru0 + s * (g.ru1 - g.ru0);
    return (A.striped ? cloth(u) : RB)[t > 0.6f ? 2 : 1];
  });
  for (int e = 0; e < 2; e++) {
    const float u = e ? g.ru1 : g.ru0, o = e ? VT : -VT;
    if (g.side) {
      // (M2 fixer round 3) a side profile's south end is the one face of its cloth we see square-on: the scalloped
      // valance runs across it (the stripes as across a front stall's flap), under a roof pitched gently enough that
      // its top reads as one awning seen from above, its widths running down the slope
      quad(m, u + o, g.rvf, g.rzf - VH, 0, g.rvb - g.rvf, g.rzb - g.rzf, -o, 0, VH, e ? VH : -VH, 0, VT,
           [&](float s, float t, float) { return valance(g.rvf + s * (g.rvb - g.rvf), VH * (1 - t)); });
      continue;
    }
    quad(m, u + o, g.rvf, g.rzf - VH, 0, g.rvb - g.rvf, g.rzb - g.rzf, -o, 0, VH, e ? VH : -VH, 0, VT,
         [&](float s, float t, float) { return valance(g.rvf + s * (g.rvb - g.rvf), VH * (1 - t)); });
  }
  // the back cloth, from the back edge down to above the keeper's head (its outside: the back, seen from the north).
  // (stalls fixer round 3) Left off in the back view, where it would hang between the camera and the keeper
  if (m.f == StallN) return;
  const Ramp BC = ramp(mix(A.striped ? A.a : A.b, rgba(120, 100, 96), 0.25f), 0.7f);
  quad(m, CU0, g.rvb - 0.4f, 19.0f, CU1 - CU0, 0, 0, 0, 0, g.rzb - 19.0f - VH, 0, 1, 0, [&](float s, float t, float l) -> uint32_t {
    const float u = CU0 + s * (CU1 - CU0);
    const int f = ((int)u) % 6;
    int k = f < 2 ? 3 : (f < 4 ? 2 : 1);
    if (t < 0.08f && ((int)u % 6) == 4) return 0;   // the hem lifts between the folds
    if (t < 0.14f) k = std::min(k, 1);
    (void)l;
    return (A.striped ? cloth(u) : BC)[k];
  });
}

// form 1: the canvas tent. A ridge roof on poles over a trestle, a gable at each end, a hem along the eaves, the
// back walled in canvas down to above the keeper's head
void tentRoof(M3& m, const Awn& A) {
  const Geo g = geoOf(m.f);
  const Ramp RA = ramp(A.a, 0.85f), RB = ramp(A.b, 0.7f);
  auto cloth = [&](float u) -> const Ramp& {
    const int stripe = (int)std::floor((u + 1.0f) / 5.0f) & 1;
    return A.striped ? (stripe ? RB : RA) : RA;
  };
  // the poles: at the corners, and the tall ridge poles at the gables, every one on the ground (stallPosts)
  paintPosts(m, 1);
  // the two slopes; stripes run down them
  // (M2 fixer round 3) a side profile: the slope turned west on screen (to the light) clearly lit, the other clearly
  // in shade, and a plain canvas's seams drawn plainly, so a side tent reads as a ridge roof and not a flat capsule
  float fnx = 0, fny = 0;
  m.dir(0, -1, fnx, fny);
  auto slope = [&](bool back) {
    const float sideL = !g.side ? 0.0f : (((fnx < 0) != back) ? 0.1f : -0.14f);
    return [&, back, sideL](float s, float t, float l) -> uint32_t {
      const float u = g.ru0 + s * (g.ru1 - g.ru0);
      float ll = l + sideL;
      const float fromEave = back ? 1 - t : t;   // 0 at the eave .. 1 at the ridge (front: t runs eave -> ridge)
      if (fromEave > 0.93f) ll += 0.1f;
      if (fromEave < 0.06f) ll -= 0.12f;
      if (((int)std::floor(u + 1.0f)) % 5 == 0) ll -= (g.side && !A.striped) ? 0.16f : 0.06f;   // the seams
      return cloth(u)[fromEave > 0.95f ? 4 : litC(ll, (int)(s * 50), (int)(t * 25))];
    };
  };
  quad(m, g.ru0, g.tvf, g.tze, g.ru1 - g.ru0, 0, 0, 0, g.tvr - g.tvf, g.tzr - g.tze, 0, -(g.tzr - g.tze), g.tvr - g.tvf, slope(false));
  quad(m, g.ru0, g.tvr, g.tzr, g.ru1 - g.ru0, 0, 0, 0, g.tvb - g.tvr, g.tze - g.tzr, 0, g.tzr - g.tze, g.tvb - g.tvr, slope(true));
  // the gables: a triangle of canvas at each end over the eaves' line
  for (int e = 0; e < 2; e++) {
    const float u = e ? g.ru1 : g.ru0;
    quad(m, u, g.tvf, g.tze - 3.0f, 0, g.tvb - g.tvf, 0, 0, 0, g.tzr - g.tze + 3.0f, e ? 1.0f : -1.0f, 0, 0, [&](float s, float t, float l) -> uint32_t {
      const float v = g.tvf + s * (g.tvb - g.tvf), z = g.tze - 3.0f + t * (g.tzr - g.tze + 3.0f);
      const float top = g.tze + (g.tzr - g.tze) * (1.0f - std::fabs(v - g.tvr) / (g.tvr - g.tvf));
      if (z > top + 0.4f) return 0;
      if (z < g.tze) return (((int)v) % 3) ? cloth(v)[1] : 0;   // the hem's fringe
      const int k = litC(l + 0.05f, (int)(s * 40), (int)(t * 20));
      return (A.striped ? (((int)std::floor(v / 5.0f)) & 1 ? RB : RA) : RA)[std::max(1, z > top - 0.9f ? k + 1 : k)];
    });
  }
  // the hem along the eaves (front and back): a band and a fringe. (stalls fixer round 2) A side profile's front eave
  // (the customers' edge) blows out broad, scalloped, so the low long edge reads as the tent's open front
  for (int e = 0; e < 2; e++) {
    const float v = e ? g.tvb : g.tvf, ht = (g.side && !e) ? 3.0f : VT;
    quad(m, g.ru0, v + (e ? ht : -ht), g.tze - 3.0f, g.ru1 - g.ru0, 0, 0, 0, e ? -ht : ht, 3.0f, 0, e ? 3.0f : -3.0f, ht, [&](float s, float t, float) -> uint32_t {
      const float u = g.ru0 + s * (g.ru1 - g.ru0);
      if (g.side && !e) {
        const int inC = ((int)std::floor(u + 100.0f)) % 5;
        if (t < 0.4f && (t < 0.2f ? (inC == 0 || inC == 4) : inC == 4)) return 0;
        if (t > 0.8f) return (A.striped ? RA : RB)[3];
        return RB[t > 0.55f ? 2 : 1];
      }
      if (t < 0.34f) return (((int)u) % 3) ? (A.striped ? RA : RB)[1] : 0;
      return (A.striped ? RA : RB)[t > 0.75f ? 3 : 2];
    });
  }
  // the ridge pole along the top (a side profile: none, the ridge reads by the light on its two slopes, never as one
  // long brown line down the stall)
  if (!g.side) boxM(m, g.ru0, g.ru1, g.tvr - 0.6f, g.tvr + 0.6f, g.tzr, g.tzr + 1.0f, kWood);
  // the back wall of canvas down to above the keeper's head (not in the back view: the keeper stands behind it)
  if (m.f == StallN) return;
  quad(m, CU0, g.tvb - 0.5f, 15.0f, CU1 - CU0, 0, 0, 0, 0, g.tze - 3.0f - 15.0f, 0, 1, 0, [&](float s, float t, float l) -> uint32_t {
    const float u = CU0 + s * (CU1 - CU0);
    const int f = ((int)u) % 7;
    int k = f < 2 ? 3 : (f < 4 ? 2 : 1);
    if (t < 0.1f) { if (((int)u) % 7 == 5) return 0; k = 1; }
    (void)l;
    return cloth(u)[k];
  });
}

// (M2 fixer round 3, review: "side-facing stalls render as an unreadable jumble") a side profile's timber booth: a
// gabled roof with its ridge along the counter, so from above it shows two slopes - the one turned to the light and
// the one in shade - with a timber gable end to the south, instead of one flat panel of shingle courses. The eaves are
// level (Geo bzf = bzb) and the ridge rises 6 px over the middle of the stall's depth.
// (M3 fixer round 2) a culture's booth roof is its cloth (dyed, striped, matting, hide), never grey slates; a culture
// whose stalls are tiled lean-tos (awning style 3) keeps the shingle courses, in fired-clay colours
bool clothRoof() { return g_awn.on && g_awn.kind != 3; }
void sideBoothRoof(M3& m, int awning, const Ramp& S) {
  const Geo g = geoOf(m.f);
  const float ve0 = g.bvf, ve1 = g.rvb, vr = (ve0 + ve1) * 0.5f, ze = g.bzf, zr = g.bzf + 6.0f;
  // which slope is turned west on screen (toward the light): it is lit clearly, the other clearly in shade
  float fnx = 0, fny = 0;
  m.dir(0, -1, fnx, fny);
  auto slope = [&](bool back) {
    const float run = back ? ve1 - vr : vr - ve0, len = std::sqrt(run * run + (zr - ze) * (zr - ze));
    const float side = ((fnx < 0) != back) ? 0.1f : -0.12f;
    return [&, back, len, side](float s, float t, float l0) -> uint32_t {
      const float l = l0 + side;
      const float u = g.ru0 + s * (g.ru1 - g.ru0);
      const float fromRidge = (back ? t : 1 - t) * len;   // down the slope from the ridge
      const int row = (int)std::floor(fromRidge / 3.0f);
      const float inRow = fromRidge - row * 3.0f;
      float ll = l + (inRow < 1.0f ? 0.08f : (inRow > 2.2f ? -0.1f : 0.0f));
      if (((int)std::floor(u + row * 3.0f)) % 6 == 0 && inRow > 0.8f) ll -= 0.16f;   // the joints
      if (hashf((int)u, row + (back ? 50 : 0), 71u + (uint32_t)awning) < 0.1f) ll -= 0.07f;
      if (fromRidge < 0.9f) ll += 0.14f;                                              // the ridge catches the light
      if (clothRoof()) {   // (M3 fixer round 2) a culture's cloth over the booth, not grey slates: soft folds, its stripes
        const Awn A = awningOf(awning);
        const bool alt = A.striped && (((int)std::floor(u)) / 4) % 2 == 1;
        float lc = l + (fromRidge < 0.9f ? 0.12f : 0.0f) + (((int)std::floor(fromRidge)) % 5 == 4 ? -0.06f : 0.0f);
        return (alt ? ramp(A.b) : ramp(A.a))[litK(lc, (int)(s * 50), (int)(t * 25))];
      }
      return S[litK(ll, (int)(s * 50), (int)(t * 25))];
    };
  };
  quad(m, g.ru0, ve0, ze, g.ru1 - g.ru0, 0, 0, 0, vr - ve0, zr - ze, 0, -(zr - ze), vr - ve0, slope(false));
  quad(m, g.ru0, vr, zr, g.ru1 - g.ru0, 0, 0, 0, ve1 - vr, ze - zr, 0, zr - ze, ve1 - vr, slope(true));
  // the ridge board
  boxM(m, g.ru0, g.ru1, vr - 0.7f, vr + 0.7f, zr - 0.2f, zr + 0.8f, kWood);
  // the fascia boards along both eaves
  auto fascia = [&](float s, float t, float l) -> uint32_t { (void)s; return kWood[t > 0.66f ? 3 : (t > 0.33f ? 2 : 1) + (l > 0.85f ? 1 : 0)]; };
  quad(m, g.ru0, ve0, ze - 2.0f, g.ru1 - g.ru0, 0, 0, 0, 0, 2.0f, 0, -1, 0, fascia);
  quad(m, g.ru0, ve1, ze - 2.0f, g.ru1 - g.ru0, 0, 0, 0, 0, 2.0f, 0, 1, 0, fascia);
  // the gable ends: a triangle of upright boards over a beam across the eaves (the south one faces the camera)
  for (int e = 0; e < 2; e++) {
    const float u = e ? g.ru1 : g.ru0;
    quad(m, u, ve0 - 0.6f, ze - 2.0f, 0, ve1 - ve0 + 1.2f, 0, 0, 0, zr - ze + 2.6f, e ? 1.0f : -1.0f, 0, 0, [&](float s, float t, float l) -> uint32_t {
      const float v = ve0 - 0.6f + s * (ve1 - ve0 + 1.2f), z = ze - 2.0f + t * (zr - ze + 2.6f);
      const float top = ze + (zr - ze) * (1.0f - std::fabs(v - vr) / (vr - ve0)) + 0.6f;
      if (z > top) return 0;
      if (z < ze) return kWood[z < ze - 1.2f ? 1 : 3];                    // the tie beam
      if (z > top - 0.9f) return kWoodDark[3];                            // the bargeboards' shade line
      const int k = ((int)std::floor(v + 40.0f)) % 3 == 0 ? 1 : (l > 0.8f ? 3 : 2);
      return kWoodDark[k];
    });
  }
}

// form 2: the timber booth. A shingled mono-pitch roof on thick corner posts with braces, a fascia board along the
// eaves, plank back wall down to above the keeper's head; the trade's painted sign
void boothRoof(M3& m, int awning) {
  const Geo g = geoOf(m.f);
  const bool slate = (awning % 4) == 1 || (awning % 4) == 2;
  static const Ramp kClayTiles = ramp(rgba(170, 84, 56));
  const Ramp& S = g_awn.on && g_awn.kind == 3 ? kClayTiles : (slate ? kStone : kWoodDark);
  // thick posts (every one on the ground: stallPosts) and a brace up to the eave at the front corners
  paintPosts(m, 2);
  if (g.side) { sideBoothRoof(m, awning, S); return; }
  // the shingles: courses three px deep, overlapping toward the eave, the joints staggered course by course
  const float len = std::sqrt((g.rvb - g.bvf) * (g.rvb - g.bvf) + (g.bzb - g.bzf) * (g.bzb - g.bzf));
  quad(m, g.ru0, g.bvf, g.bzf, g.ru1 - g.ru0, 0, 0, 0, g.rvb - g.bvf, g.bzb - g.bzf, 0, -(g.bzb - g.bzf), g.rvb - g.bvf, [&](float s, float t, float l) -> uint32_t {
    const float u = g.ru0 + s * (g.ru1 - g.ru0), d = (1 - t) * len;   // d: from the back (ridge) down the slope
    const int row = (int)std::floor(d / 3.0f);
    const float inRow = d - row * 3.0f;
    float ll = l + (inRow < 1.0f ? 0.1f : (inRow > 2.2f ? -0.12f : 0.0f));
    if (((int)std::floor(u + row * 3.0f)) % 6 == 0 && inRow > 0.8f) ll -= 0.18f;   // the joints
    if (hashf((int)u, row, 71u + (uint32_t)awning) < 0.1f) ll -= 0.08f;
    if (t > 0.95f) ll += 0.1f;
    if (clothRoof()) {   // (M3 fixer round 2) the culture's cloth, as on its awnings
      const Awn A = awningOf(awning);
      const bool alt = A.striped && (((int)std::floor(u)) / 4) % 2 == 1;
      return (alt ? ramp(A.b) : ramp(A.a))[litK(l + (t > 0.95f ? 0.1f : 0.0f), (int)(s * 50), (int)(t * 25))];
    }
    return S[litK(ll, (int)(s * 50), (int)(t * 25))];
  });
  // the fascia: boards along the eave, the ends and the back
  auto fascia = [&](float s, float t, float l) -> uint32_t { (void)s; return kWood[t > 0.66f ? 3 : (t > 0.33f ? 2 : 1) + (l > 0.85f ? 1 : 0)]; };
  quad(m, g.ru0, g.bvf, g.bzf - 3.0f, g.ru1 - g.ru0, 0, 0, 0, 0, 3.0f, 0, -1, 0, fascia);
  quad(m, g.ru0, g.rvb, g.bzb - 3.0f, g.ru1 - g.ru0, 0, 0, 0, 0, 3.0f, 0, 1, 0, fascia);
  for (int e = 0; e < 2; e++) {
    const float u = e ? g.ru1 : g.ru0;
    quad(m, u, g.bvf, g.bzf - 3.0f, 0, g.rvb - g.bvf, g.bzb - g.bzf, 0, 0, 3.0f, e ? 1.0f : -1.0f, 0, 0, fascia);
  }
  // braces from the front posts up under the eave (a side profile's short posts on the counter need none)
  for (int k = 0; k < 6 && !g.side; k++) {
    boxM(m, g.ru0 + 4.5f + k, g.ru0 + 5.5f + k, g.bvf + 1.0f, g.bvf + 2.0f, g.bzf - 9.0f + k, g.bzf - 8.0f + k, kWood);
    boxM(m, g.ru1 - 5.5f - k, g.ru1 - 4.5f - k, g.bvf + 1.0f, g.bvf + 2.0f, g.bzf - 9.0f + k, g.bzf - 8.0f + k, kWood);
  }
  // the plank back wall down to above the keeper's head (not in the back view: the keeper stands behind it)
  if (m.f == StallN) return;
  quad(m, CU0, g.rvb - 0.6f, 13.0f, CU1 - CU0, 0, 0, 0, 0, g.bzb - 3.0f - 13.0f, 0, 1, 0, [&](float s, float t, float l) -> uint32_t {
    const float u = CU0 + s * (CU1 - CU0);
    int k = ((int)u % 6 == 0) ? 1 : 2;
    if (l > 0.6f && (int)u % 6 == 1) k = 3;
    if (t < 0.06f) k = 0;
    return kWoodDark[k + 1];
  });
}
// the trade's painted sign: a board with the trade's glyph, hung on two chains
Canvas boothSign(const Awn& A, int trade) {
  Canvas c(14, 9);
  const Ramp R = ramp(A.a, 0.8f);
  c.set(2, 0, kIron[2]); c.set(11, 0, kIron[2]);
  for (int y = 1; y < 9; y++)
    for (int x = 0; x < 14; x++) {
      const bool rim = y == 1 || y == 8 || x == 0 || x == 13;
      c.set(x, y, rim ? kWood[y == 1 ? 3 : 1] : R[y == 2 ? 3 : 2]);
    }
  static const uint8_t glyph[kStallTrades][4] = {
      {0x18, 0x3C, 0x3C, 0x18}, {0x08, 0x7E, 0xFC, 0x48}, {0x7E, 0x42, 0x7E, 0x42}, {0x3C, 0x18, 0x3C, 0x3C},
      {0x38, 0x7C, 0x7E, 0x0C}, {0x3C, 0x7E, 0x7E, 0x00}, {0x7E, 0x18, 0x18, 0x18}, {0x66, 0xFF, 0xFF, 0x66},
  };
  const uint32_t ink = A.striped ? lighten(A.b, 0.2f) : lighten(A.b, 0.4f);
  for (int r = 0; r < 4; r++)
    for (int b = 0; b < 8; b++)
      if (glyph[trade % kStallTrades][r] & (0x80 >> b)) c.set(3 + b, 3 + r, ink);
  return c;
}

// packed up for the night: the stock under a tied cover along the counter, the front closed (a curtain let down from
// the roof's front edge, or the booth's shutter)
void closedStall(M3& m, const Awn& A, int form) {
  const Geo g = geoOf(m.f);
  const Ramp S = ramp(rgba(196, 180, 146), 0.7f);
  // the cover: lumps of the stock showing through, cords across, its edge falling over the counter's front lip
  for (float u = CU0 + 1.0f; u <= CU1 - 1.0f; u += 0.4f) {
    const float a = (u - CU0) / (CU1 - CU0);
    const bool cord = ((int)u == 12 || (int)u == 24 || (int)u == 36);
    for (float v = CV0 + 0.6f; v <= CV1 - 1.5f; v += 0.4f) {
      const float hump = std::max(0.4f, 1.6f + std::sin(a * 18.0f) * 1.0f + std::sin(a * 7.0f) * 0.7f + std::sin((v - CV0) * 0.35f) * 1.2f);
      const float h = std::min(hump, 0.5f + (v - CV0 - 0.6f) * 1.6f);
      const int k = (v < CV0 + 2.0f) ? 2 : (std::cos(a * 18.0f) > 0.55f ? 2 : 3);
      m.plot(u, v, CT + h, cord ? kWood[1] : S[k + (h > hump - 0.3f && v > CV0 + 3.0f && std::sin(a * 18.0f) > 0.3f ? 1 : 0)]);
    }
    for (float z = CT - 2.0f; z <= CT + 0.5f; z += 0.4f) m.plot(u, CV0 + 0.4f, z, cord ? kWood[1] : S[1], 0.3f);
  }
  if (form == 2) {   // the booth's shutter: boards across the opening, the hasp
    const float zt = g.bzf - 3.0f;
    quad(m, CU0 + 2.0f, g.bvf + 0.6f, CT, CU1 - CU0 - 4.0f, 0, 0, 0, 0, zt - CT, 0, -1, 0, [&](float s, float t, float l) -> uint32_t {
      const float z = CT + t * (zt - CT), u = CU0 + 2.0f + s * (CU1 - CU0 - 4.0f);
      const int pl = (int)z % 4;
      int k = pl == 0 ? 1 : (pl == 1 ? 3 : 2);
      if (u > 22.0f && u < 26.0f && z > CT + 4 && z < CT + 7) return kIron[z > CT + 6 ? 4 : 2];
      (void)l;
      return kWood[k];
    });
    return;
  }
  // the curtain: let down from the roof's front edge to the counter, in folds
  const Ramp C = ramp(darken(A.a, 0.2f), 0.75f);
  const float vf = form == 1 ? g.tvf + 0.6f : g.rvf + 0.6f, zt = form == 1 ? g.tze - 3.0f : g.rzf - 5.0f;
  quad(m, CU0 + 1.0f, vf, CT + 0.5f, CU1 - CU0 - 2.0f, 0, 0, 0, 0, zt - CT - 0.5f, 0, -1, 0, [&](float s, float t, float) -> uint32_t {
    const float u = CU0 + 1.0f + s * (CU1 - CU0 - 2.0f);
    const int f = ((int)u) % 6;
    if (t < 0.12f && f == 4) return 0;
    return C[f < 2 ? 3 : (f < 4 ? 2 : 1)];
  });
}

void paintStallModel(M3& m, int trade, int awning, int form, bool closed) {
  const Geo g = geoOf(m.f);
  const Awn A = awningOf(awning);
  const uint32_t seed = (uint32_t)(trade * 31 + awning * 7);
  const int t = trade % kStallTrades;
  stallBackStock(m, t, seed);
  stallCounter(m, A, form, t, closed);
  if (!closed) stallGoods(m, t, seed);
  else closedStall(m, A, form);
  switch (form) {
    case 1: tentRoof(m, A); break;
    case 2: {
      boothRoof(m, awning);
      // the sign: under the eave's middle at the front; on the back planks from behind; on the near post from the side
      const Canvas sg = boothSign(A, t);
      // (stalls fixer round 3) never in front of the keeper: up on the front eave's board (S), hung off the back eave
      // beside them (N), on the south end of the eave (E, W)
      if (m.f == StallS) put(m, sg, 24.0f, g.bvf - 0.4f, g.bzf + 6.5f, true, 1.0f);
      else if (m.f == StallN) put(m, sg, 10.0f, g.rvb + 0.4f, g.bzb - 3.0f, true, 1.0f);
      // (awnings fixer) E / W: in from the low corner, clear of the lantern on the south front post (stallLantern)
      else put(m, sg, g.ru0 + 0.5f, g.bvf + 9.0f, g.bzf - 1.0f, true, 30.0f);
      break;
    }
    default: clothRoof(m, A, awning); break;
  }
}

Canvas stallShadowFor(int form, int facing) {
  const StallCanvas sc = stallCanvas(facing);
  M3 m(sc.w, sc.h, sc.x0, sc.y0, facing);
  const Geo g = geoOf(facing);
  Shade sh(kVendorShadowX, kVendorShadowY, 96, 96);
  const float zf = form == 1 ? g.tze : (form == 2 ? g.bzf : g.rzf), zb = form == 1 ? g.tze : (form == 2 ? g.bzb : g.rzb);
  sh.rectM(m, g.ru0, g.ru1, form == 1 ? g.tvf : g.rvf, form == 1 ? g.tvb : g.rvb, zf, zb, 92);
  if (form == 1) {
    sh.rectM(m, g.ru0, g.ru1, g.tvf, g.tvr, g.tze, g.tzr, 92);
    sh.rectM(m, g.ru0, g.ru1, g.tvr, g.tvb, g.tzr, g.tze, 92);
  }
  sh.rectM(m, CU0, CU1, CV0, CV1, CT, CT, 80);
  // (stalls fixer round 3) every post throws its own thin shade from its foot, so it reads as standing on the paving
  for (const Post& p : stallPosts(form, facing))
    for (float z = 0; z <= p.z1; z += 0.5f) {
      sh.castM(m, (p.u0 + p.u1) * 0.5f, (p.v0 + p.v1) * 0.5f, z, 96);
      sh.castM(m, p.u0, p.v0, z, 96);
    }
  if (!g.side) return sh.done();
  // (stalls fixer round 3) a side stall's keeper stands on a floor of boards laid in their column beside the counter
  // (between the stool and the crates): it lies on the ground under everything, so it belongs with the shadow, and
  // makes the keeper's strip read as part of the stall beside its narrow roof. The shade falls over the boards.
  Canvas shd = sh.done();
  Canvas c(shd.w, shd.h);
  const float fu0 = 3.0f, fu1 = 45.0f, fv0 = 19.0f, fv1 = 30.0f;
  for (float u = fu0; u <= fu1; u += 0.5f)
    for (float v = fv0; v <= fv1; v += 0.5f) {
      float x, y;
      m.world(u, v, x, y);
      const int px = (int)std::floor(x) - kVendorShadowX, py = (int)std::floor(y) - kVendorShadowY;
      const int board = (int)std::floor((v - fv0) / 2.75f), along = (int)std::floor(u + board * 7.0f);
      int k = (board & 1) ? 2 : 3;
      if (std::fmod(v - fv0, 2.75f) < 0.5f) k = 1;                // the gaps between the boards
      if (along % 14 == 0) k = 1;                                  // their butt joints, staggered
      if (u < fu0 + 0.6f || u > fu1 - 0.6f || v < fv0 + 0.5f || v > fv1 - 0.5f) k = 0;   // the edges, on the paving
      if (hashf((int)u, (int)v, 2203u) < 0.08f) k = std::max(1, k - 1);
      c.set(px, py, kWood[k]);
    }
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const uint32_t s0 = shd.get(x, y), b = c.get(x, y);
      const int a = (int)(s0 >> 24);
      if (!chA(b)) { c.set(x, y, s0); continue; }
      if (a) c.set(x, y, opaque(mix(b, rgba(26, 16, 44), a / 255.0f * 0.75f)));
    }
  return c;
}

// ================================================================================================ open tables, cloths
// The trestle table: two tiles wide, standing on its WEST tile (world px from that tile's top-left: the table spans x
// 1..31); the seller stands behind it (north). Its goods lie on the top; a square sunshade on two poles behind may
// shade it. The ground cloth: a cloth spread on the paving over the two tiles, its goods laid out on it.
constexpr int TBX0 = -4, TBY0 = -46, TBW = 40, TBH = 63;   // the table's canvas
constexpr int GCX0 = -4, GCY0 = -14, GCW = 40, GCH = 31;   // the cloth's
constexpr float TT = 10.0f;                                // the table's top
uint32_t shadeCloth(int shade) {
  static const uint32_t s[kTableShades] = {0, rgba(226, 214, 184), rgba(176, 62, 54), rgba(64, 120, 92)};
  return s[((shade % kTableShades) + kTableShades) % kTableShades];
}
// the table's model is in world px (facing S: u = x + 16, v = 16 - y)
inline float TU(float x) { return x + 16.0f; }
inline float TV(float y) { return 16.0f - y; }

void tableModel(M3& m, int shade, bool closed) {
  // the top: boards running along, the lit front edge, a thick lip
  boxM(m, TU(2.0f), TU(30.0f), TV(14.0f), TV(3.0f), TT - 2.0f, TT, kWood, [](int fc, float s, float t, int k, uint32_t c) -> uint32_t {
    if (fc == 0) {
      int kk = 3;
      if (t < 0.1f) kk = 4;
      else if (((int)(t * 11.0f)) % 4 == 3) kk = 2;
      if (((int)(s * 28.0f)) % 10 == 9) kk = 2;
      return kWood[kk];
    }
    (void)k;
    return fc == 1 ? kWood[t > 0.5f ? 2 : 1] : c;
  });
  // the trestles: splayed legs at each end (seen end-on, from the front) and a stretcher low between them
  for (float lx : {5.0f, 26.0f}) {
    for (float z = 0; z <= TT - 2.0f; z += 0.3f) {
      const float d = (TT - 2.0f - z) * 0.45f;
      m.plotW(lx - d + 0.0f, 12.5f, z, kWood[3]); m.plotW(lx - d + 1.0f, 12.5f, z, kWood[2]);
      m.plotW(lx + d + 0.0f, 12.5f, z, kWood[2]); m.plotW(lx + d + 1.0f, 12.5f, z, kWood[1]);
      m.plotW(lx - d + 0.5f, 4.5f, z, kWoodDark[2]); m.plotW(lx + d + 0.5f, 4.5f, z, kWoodDark[1]);
    }
  }
  boxM(m, TU(6.0f), TU(26.0f), TV(9.0f), TV(8.0f), 2.5f, 3.5f, kWood);
  if (!shade) return;
  // the sunshade: two poles behind the table and a square of cloth on a frame, tilted a little to the front
  const Ramp R = ramp(shadeCloth(shade), 0.75f), T = ramp(shade == 1 ? rgba(176, 62, 54) : rgba(232, 222, 190), 0.7f);
  if (closed) {
    // (stalls fixer round 1: "a bare upright frame at night") the poles are down: the shade furled round them and
    // tied, the bundle laid along the table's back edge, the poles' ends showing past the cloth
    boxM(m, TU(-1.5f), TU(33.5f), TV(4.4f), TV(3.6f), TT + 0.4f, TT + 1.2f, kWood);
    boxM(m, TU(1.0f), TU(31.0f), TV(5.2f), TV(2.8f), TT, TT + 2.4f, R, [&](int fc, float s, float t, int k, uint32_t c) -> uint32_t {
      const float x = 1.0f + s * 30.0f;
      if (std::fabs(x - 8.0f) < 0.6f || std::fabs(x - 16.0f) < 0.6f || std::fabs(x - 24.0f) < 0.6f) return kWood[1];   // the ties
      if (fc == 0) return R[t < 0.35f ? 3 : 2];
      (void)k;
      return c;
    });
    return;
  }
  for (float px : {1.5f, 29.5f}) boxM(m, TU(px), TU(px + 1.2f), TV(1.0f), TV(0.0f), 0, 31.0f, kWood);
  quad(m, TU(-1.0f), TV(10.0f), 27.0f, 34.0f, 0, 0, 0, 18.0f, 6.0f, 0, -6.0f, 18.0f, [&](float s, float t, float l) -> uint32_t {
    const float x = -1.0f + s * 34.0f;
    const bool stripe = shade == 1 && (((int)std::floor((x + 1.0f) / 4.0f)) & 1);
    float ll = l;
    if (t > 0.93f) ll += 0.1f;
    if (shade != 1 && (s < 0.05f || s > 0.95f || t < 0.09f || t > 0.94f)) return T[t < 0.05f ? 1 : 2];   // the trim
    if (shade != 1 && ((int)std::floor(x + 1.0f)) % 9 == 0) ll -= 0.09f;                                // the seams
    const int k = litC(ll, (int)(s * 34), (int)(t * 18));
    return stripe ? T[k] : R[k];
  });
  // the fringe along its front edge
  quad(m, TU(-1.0f), TV(10.0f) - 1.0f, 24.5f, 34.0f, 0, 0, 0, 1.0f, 2.5f, 0, -2.5f, 1.0f, [&](float s, float t, float) -> uint32_t {
    const float x = -1.0f + s * 34.0f;
    if (t < 0.45f && ((int)std::floor(x + 2.0f)) % 4 == 3) return 0;
    return t < 0.45f ? T[1] : R[1];
  });
}

// the table's goods, lying and standing on its top (x px across it, y the row on the top)
void tableGoodsM(M3& m, int goods) {
  auto lay = [&](const Canvas& s, float x, float y) { put(m, s, TU(x), TV(y), TT, false, 0.8f); };
  switch (((goods % kTableGoods) + kTableGoods) % kTableGoods) {
    case 0: {   // cheeses: wheels, one cut, a wedge on a board, a knife
      const Ramp ch = ramp(rgba(236, 196, 92), 0.7f), rind = ramp(rgba(196, 120, 52), 0.7f);
      lay(drumTop(8, 3, rind, ch, false), 7.0f, 9.0f);
      lay(drumTop(7, 3, rind, ch, false), 14.0f, 7.0f);
      lay(drumTop(8, 3, rind, ch, false), 10.5f, 13.0f);
      Canvas bd(11, 6);
      for (int y = 0; y < 6; y++) for (int x = 0; x < 11; x++) bd.set(x, y, kWood[y == 0 ? 4 : (y == 5 ? 1 : 3)]);
      for (int k = 0; k < 3; k++) hl(bd, 1, 5 - k, 3 - k, ch[k == 2 ? 4 : 3]);
      hl(bd, 7, 10, 2, kIron[4]); bd.set(6, 2, kWood[1]);
      lay(bd, 24.0f, 11.0f);
      break;
    }
    case 1: {   // eggs in two baskets, a brace of fowl laid out
      const Ramp egg = ramp(rgba(236, 226, 206), 0.5f), brown = ramp(rgba(206, 156, 104), 0.6f), fowl = ramp(rgba(236, 196, 170), 0.6f);
      lay(basketTop(9, egg, 3, 1.2f), 7.0f, 10.0f);
      lay(basketTop(8, brown, 5, 1.2f), 16.0f, 12.0f);
      Canvas fw(10, 5);
      ball(fw, 4.0f, 2.5f, 3.4f, 2.0f, fowl, 0.04f);
      fw.set(8, 2, fowl[1]); fw.set(9, 1, fowl[1]); fw.set(0, 3, rgba(236, 160, 60));
      lay(fw, 25.0f, 9.0f);
      break;
    }
    case 2: {   // spices in little open sacks, a brass scale
      static const uint32_t sp[5] = {rgba(200, 64, 40), rgba(236, 186, 40), rgba(176, 120, 60), rgba(110, 150, 70), rgba(70, 54, 50)};
      for (int i = 0; i < 4; i++) {
        Canvas s(6, 6);
        for (int j = 2; j < 6; j++) hl(s, 0, 5, j, kCloth[j == 2 ? 4 : (j == 5 ? 1 : 2)]);
        ellipse(s, 3.0f, 2.2f, 2.6f, 1.5f, sp[i]);
        s.set(2, 1, lighten(sp[i], 0.4f));
        lay(s, 5.0f + (i % 2) * 7.0f + (i / 2) * 2.0f, 9.0f + (i / 2) * 4.0f);
      }
      Canvas sc(10, 9);
      vl(sc, 5, 1, 8, kBrass[2]); hl(sc, 1, 9, 1, kBrass[3]);
      ellipse(sc, 2.0f, 4.0f, 2.0f, 0.8f, kBrass[3]); ellipse(sc, 8.0f, 4.0f, 2.0f, 0.8f, kBrass[2]);
      hl(sc, 3, 7, 8, kBrass[1]);
      lay(sc, 24.0f, 10.0f);
      break;
    }
    case 3: {   // flowers in wooden buckets
      static const uint32_t fl[6] = {rgba(220, 60, 80), rgba(248, 210, 70), rgba(170, 90, 200), rgba(250, 240, 230), rgba(240, 130, 50), rgba(90, 120, 220)};
      for (int i = 0; i < 3; i++) {
        Canvas b = drumTop(6, 4, kWood, kWood, true);
        Canvas f(8, b.h + 5);
        blit(f, b, 1, 5);
        for (int k = 0; k < 10; k++) {
          const int fx = 1 + (k * 5) % 6, fy = 1 + (k * 3) % 5;
          f.set(fx, fy + 1, kLeaf[2]);
          f.set(fx, fy, fl[(i * 2 + k) % 6]);
          f.set(fx + 1, fy, darken(fl[(i * 2 + k) % 6], 0.3f));
        }
        lay(f, 6.0f + i * 8.0f, 10.0f + (i & 1) * 2.0f);
      }
      break;
    }
    case 4: {   // honey jars, bundles of candles, a cake of wax
      const Ramp honey = ramp(rgba(226, 150, 40), 0.8f);
      for (int i = 0; i < 4; i++) {
        Canvas j(5, 7);
        ball(j, 2.5f, 4.0f, 2.3f, 2.8f, honey, 0.04f);
        hl(j, 0, 4, 0, kCloth[4]); hl(j, 0, 4, 1, kCloth[2]);
        lay(j, 5.0f + (i % 2) * 5.5f, 9.5f + (i / 2) * 3.5f);
      }
      Canvas cd(8, 9);
      for (int b = 0; b < 2; b++) for (int k = 0; k < 3; k++) vl(cd, 1 + b * 4 + k, 1 + (k & 1), 8, kBone[k == 0 ? 4 : 3]);
      cd.set(2, 0, kGlow[3]);
      lay(cd, 20.0f, 10.0f);
      Canvas wx(7, 5);
      box(wx, 0, 1, 6, 4, rgba(232, 200, 110)); hl(wx, 0, 6, 1, rgba(250, 230, 160));
      lay(wx, 27.0f, 12.0f);
      break;
    }
    case 5: {   // baskets and wickerwork for sale, stacked
      for (int i = 0; i < 3; i++) {
        Canvas b = drumTop(7, i == 1 ? 6 : 4, kWicker, kWicker, true);
        lay(b, 6.0f + i * 8.0f, 10.0f + (i & 1) * 2.0f);
      }
      break;
    }
    case 6: {   // wool: skeins in a heap, a folded fleece, a drop spindle
      static const uint32_t yc[5] = {rgba(190, 60, 60), rgba(60, 90, 170), rgba(226, 196, 90), rgba(230, 226, 214), rgba(80, 130, 80)};
      for (int i = 0; i < 6; i++) {
        Canvas s(6, 4);
        ball(s, 3.0f, 2.0f, 2.8f, 1.8f, ramp(yc[i % 5], 0.6f), 0.05f);
        lay(s, 4.0f + (i % 3) * 5.0f + (i / 3) * 2.5f, 8.0f + (i / 3) * 3.5f);
      }
      Canvas fl(12, 6);
      furBall(fl, 6.0f, 3.0f, 5.5f, 2.6f, ramp(rgba(226, 218, 200), 0.5f), 0.4f, 9);
      lay(fl, 24.0f, 11.0f);
      break;
    }
    default: {   // apples by the basket, a cask of cider on its side
      const Ramp ap = ramp(rgba(200, 52, 44), 0.8f), gr = ramp(rgba(150, 190, 70), 0.8f);
      lay(basketTop(9, ap, 7, 1.6f), 6.0f, 10.0f);
      lay(basketTop(8, gr, 9, 1.6f), 15.0f, 12.0f);
      Canvas ck(10, 8);
      for (int x = 0; x < 10; x++)
        for (int y = 0; y < 8; y++) ck.set(x, y, kWood[y <= 1 ? 4 : (y >= 6 ? 1 : (y == 2 || y == 5 ? 3 : 2))]);
      vl(ck, 2, 0, 7, kIron[2]); vl(ck, 7, 0, 7, kIron[2]);
      ellipse(ck, 9.0f, 4.0f, 1.0f, 3.6f, kWood[3]);
      ck.set(9, 4, kWoodDark[0]);
      lay(ck, 25.0f, 11.0f);
      break;
    }
  }
}

uint32_t groundClothCol(int cloth) {
  static const uint32_t s[4] = {rgba(170, 56, 50), rgba(58, 74, 140), rgba(200, 150, 60), rgba(70, 120, 84)};
  return s[((cloth % 4) + 4) % 4];
}
void clothModel(M3& m, int cloth, int goods, bool closed) {
  const Ramp R = ramp(groundClothCol(cloth), 0.7f), T = ramp(rgba(232, 214, 170), 0.6f);
  if (closed) {   // folded into a bundle, tied, waiting for morning
    Canvas b(18, 11);
    ball(b, 9.0f, 6.0f, 8.0f, 4.6f, R, 0.06f);
    vl(b, 9, 1, 10, kWood[1]);
    b.set(8, 0, kWood[2]); b.set(10, 0, kWood[2]);
    put(m, b, TU(16.0f), TV(11.0f), 0, false, 0.5f);
    return;
  }
  // the cloth: flat on the paving, a woven border, a few creases
  quad(m, TU(1.0f), TV(14.5f), 0.4f, 30.0f, 0, 0, 0, 11.5f, 0, 0, 0, 1, [&](float s, float t, float l) -> uint32_t {
    const float x = 1.0f + s * 30.0f, y = 14.5f - t * 11.5f;
    const bool border = x < 2.2f || x > 29.8f || y > 13.3f || y < 4.2f;
    int k = ((int)(x + y * 2) % 7 == 0) ? 2 : 3;
    if (((int)x) % 9 == 4) k = 2;
    (void)l;
    return border ? T[(((int)x + (int)y) & 1) ? 2 : 3] : R[k];
  });
  auto lay = [&](const Canvas& s, float x, float y) { put(m, s, TU(x), TV(y), 0.4f, false, 0.8f); };
  switch (((goods % kClothGoods) + kClothGoods) % kClothGoods) {
    case 0: {   // pots and jugs, a stack of bowls
      const Ramp terra = ramp(rgba(190, 104, 62), 0.8f), dark = ramp(rgba(120, 76, 60), 0.7f), glaze = ramp(rgba(70, 120, 170), 0.8f);
      lay(jugTop(terra, true), 6.0f, 11.0f);
      lay(jugTop(glaze, false), 12.5f, 9.0f);
      lay(jugTop(dark, true), 17.0f, 12.5f);
      lay(bowlsTop(terra), 25.0f, 10.5f);
      break;
    }
    case 1: {   // rolled rugs and a folded stack
      static const uint32_t rc[3] = {rgba(160, 50, 50), rgba(60, 80, 150), rgba(200, 150, 60)};
      for (int i = 0; i < 3; i++) lay(boltTop(14, rc[i]), 10.0f, 7.0f + i * 2.8f);
      lay(foldTop(8, 3, 2), 24.0f, 11.0f);
      break;
    }
    case 2: {   // gourds, pumpkins, turnips, a cabbage
      const Ramp pump = ramp(rgba(226, 124, 40), 0.8f), turn = ramp(rgba(226, 214, 220), 0.6f), cab = ramp(rgba(110, 166, 70), 0.8f);
      Canvas p(9, 7); ball(p, 4.5f, 3.8f, 4.0f, 3.0f, pump, 0.05f); p.set(4, 0, kLeaf[2]); p.set(4, 1, kLeaf[1]);
      lay(p, 6.0f, 11.0f);
      Canvas q(7, 6); ball(q, 3.5f, 3.2f, 3.2f, 2.6f, pump, 0.05f); q.set(3, 0, kLeaf[2]);
      lay(q, 13.0f, 8.0f);
      for (int i = 0; i < 3; i++) { Canvas t(4, 5); ball(t, 2.0f, 3.0f, 1.7f, 1.7f, turn, 0.04f); t.set(2, 1, rgba(150, 60, 140)); t.set(2, 0, kLeaf[2]); lay(t, 18.0f + i * 3.0f, 12.0f - (i & 1) * 2.0f); }
      Canvas cb(6, 5); ball(cb, 3.0f, 2.6f, 2.7f, 2.3f, cab, 0.05f);
      lay(cb, 27.0f, 10.0f);
      break;
    }
    default: {   // furs and pelts laid flat
      const Ramp fur = ramp(rgba(140, 96, 62), 0.7f), grey = ramp(rgba(150, 150, 150), 0.6f), fox = ramp(rgba(214, 112, 52), 0.7f);
      Canvas a(14, 8); furBall(a, 7.0f, 4.0f, 6.5f, 3.4f, fur, 0.6f, 3); lay(a, 8.0f, 12.0f);
      Canvas b(12, 7); furBall(b, 6.0f, 3.5f, 5.5f, 3.0f, grey, 0.6f, 5); lay(b, 18.0f, 9.0f);
      Canvas c(9, 6); furBall(c, 4.5f, 3.0f, 4.0f, 2.4f, fox, 0.5f, 7); c.set(8, 3, kWhite); lay(c, 26.0f, 12.5f);
      break;
    }
  }
}

// ================================================================================================ the cart
// A two-wheeled market cart standing on its tile in 3/4: the bed's floor and sides (its load seen from above), the
// near wheel full face (the far one behind the bed), the shafts reaching over the next tile resting on a prop.
constexpr int CAX0 = -24, CAY0 = -28, CAW = 72, CAH = 46;
void cartModel(M3& m, int v) {
  const bool west = (v & 4) != 0;
  auto X = [&](float x) { return west ? 16.0f - x : x; };   // the model is built shafts-east and mirrored
  auto B = [&](float x0, float x1, float y0, float y1, float z0, float z1, const Ramp& R) {
    const float a = X(x0), b = X(x1);
    boxM(m, TU(std::min(a, b)), TU(std::max(a, b)), TV(y1), TV(y0), z0, z1, R);
  };
  // the far wheel (behind the bed), the axle
  for (float y : {2.6f}) {
    for (float a = 0; a < 6.2832f; a += 0.05f)
      for (float r = 5.2f; r <= 6.4f; r += 0.4f) m.plotW(X(4.0f + std::cos(a) * r), y, 6.5f + std::sin(a) * r, kWoodDark[1]);
  }
  // the bed: the floor, the sides, the boards
  B(-12.0f, 15.0f, 2.0f, 14.0f, 6.0f, 7.5f, kWood);
  B(-12.0f, 15.0f, 13.0f, 14.0f, 7.5f, 12.0f, kWood);
  B(-12.0f, 15.0f, 2.0f, 3.0f, 7.5f, 12.0f, kWoodDark);
  B(-12.0f, -11.0f, 3.0f, 13.0f, 7.5f, 12.0f, kWoodDark);
  B(14.0f, 15.0f, 3.0f, 13.0f, 7.5f, 11.0f, kWood);
  // the load
  const int load = v & 3;
  const Ramp sack = ramp(rgba(206, 184, 140), 0.7f), hay = kThatch;
  for (int i = 0; i < 4; i++) {
    const float x = -8.0f + i * 6.5f, y = 6.0f + (i & 1) * 4.0f;
    Canvas s;
    if (load == 0) { s = Canvas(8, 7); ball(s, 4.0f, 3.6f, 3.6f, 3.0f, sack, 0.05f); hl(s, 3, 5, 0, kWood[1]); }
    else if (load == 1) s = crateTop(7, 4, 4, i & 1 ? &kLeaf : nullptr, (uint32_t)i);
    else if (load == 2) { s = drumTop(6, 6, kWood, kWood, false); for (int x = 0; x < 6; x++) { s.set(x, 4, kIron[2]); s.set(x, 8, kIron[2]); } }
    else { s = Canvas(9, 7); furBall(s, 4.5f, 3.6f, 4.2f, 3.0f, hay, 0.5f, (uint32_t)(3 + i)); }
    put(m, s, TU(X(x)), TV(y), 7.5f, false, 0.6f);
  }
  // the axle and the near wheel, full face to the camera: rim, spokes, hub
  for (float a = 0; a < 6.2832f; a += 0.03f) {
    for (float r = 5.0f; r <= 6.6f; r += 0.3f) {
      const float lx = std::cos(a), lz = std::sin(a);
      const int k = (lx * -0.6f + lz * 0.8f) > 0.3f ? 3 : ((lx * -0.6f + lz * 0.8f) > -0.4f ? 2 : 1);
      m.plotW(X(4.0f + lx * r), 14.6f, 6.5f + lz * r, r > 6.2f ? kWood[1] : kWood[k], 1.0f);
    }
    if (std::fmod(a + 0.05f, 0.785f) < 0.1f)
      for (float r = 1.0f; r < 5.0f; r += 0.3f) m.plotW(X(4.0f + std::cos(a) * r), 14.6f, 6.5f + std::sin(a) * r, kWood[2], 1.0f);
  }
  for (float r = 0; r < 1.4f; r += 0.3f)
    for (float a = 0; a < 6.2832f; a += 0.2f) m.plotW(X(4.0f + std::cos(a) * r), 14.8f, 6.5f + std::sin(a) * r, kIron[r < 0.6f ? 4 : 2], 1.2f);
  // the shafts: from the bed's front over the next tile, down to a prop at their tips
  for (float y : {4.0f, 12.0f})
    for (float x = 15.0f; x <= 32.0f; x += 0.3f) {
      const float z = 9.0f - (x - 15.0f) * 0.28f;
      m.plotW(X(x), y, z, kWood[3]);
      m.plotW(X(x), y + 1.0f, z, kWood[1]);
    }
  boxM(m, TU(X(31.0f) - 0.5f), TU(X(31.0f) + 0.7f), TV(12.0f), TV(4.0f), 4.0f, 5.0f, kWood);
  for (float z = 0; z < 4.5f; z += 0.3f) m.plotW(X(31.0f), 8.0f, z, kWood[2]);
}

// ---- the market cross: a stepped octagonal base, a shaft, a carved head
void marketCross(Canvas& c) {
  const int cx = c.w / 2, b = c.h - 2;
  const Ramp& S = kStoneWarm;
  struct Step { int rx, h; };
  const Step st[3] = {{14, 4}, {10, 4}, {7, 3}};
  int top = b;
  for (const Step& s : st) {
    const int ry = std::max(2, s.rx / 3);
    // the step's front face (lit on the left), then its top (an ellipse)
    for (int y = top - s.h; y <= top; y++)
      for (int x = cx - s.rx; x <= cx + s.rx; x++) {
        const float u = (x - cx) / (float)s.rx;
        int k = u < -0.4f ? 3 : (u < 0.3f ? 2 : 1);
        if (y == top) k = 0;
        if (((x - cx + 40) % 5) == 0 && y > top - s.h) k = std::max(0, k - 1);   // the faces of the octagon
        c.set(x, y, S[k]);
      }
    ellipse(c, cx + 0.5, top - s.h, s.rx + 0.5, ry, S[3]);
    ellipse(c, cx - 1.5, top - s.h - 0.5, s.rx * 0.6, ry * 0.5, S[4]);
    top -= s.h + ry - 1;
  }
  // the shaft, a moulded knop halfway
  const int knop = (top + 12) / 2;
  for (int y = 12; y <= top; y++) {
    const int hw = y > knop ? 3 : 2;
    for (int x = cx - hw; x <= cx + hw - 1; x++) {
      const float u = (x - cx + 0.5f) / hw;
      c.set(x, y, S[u < -0.3f ? 4 : (u < 0.4f ? 3 : 1)]);
    }
  }
  hl(c, cx - 4, cx + 3, knop, S[4]);
  hl(c, cx - 4, cx + 3, knop + 1, S[1]);
  // the head: a cross with flared arms
  for (int x = cx - 7; x <= cx + 6; x++) { c.set(x, 8, S[4]); c.set(x, 9, S[3]); c.set(x, 10, S[1]); }
  for (int y = 2; y <= 12; y++) { c.set(cx - 1, y, S[4]); c.set(cx, y, S[2]); }
  c.set(cx - 8, 9, S[3]); c.set(cx + 7, 9, S[1]); c.set(cx - 1, 1, S[4]); c.set(cx, 1, S[3]);
  // moss in the steps' corners
  for (int i = 0; i < 6; i++) c.set(cx - 12 + i * 5, b - 1 - (i % 2), kMoss[2 + (i % 2)]);
}

// ---- livestock: a sheep and a cow, grazing (frames 0-4 head down, 5-7 head up)
void sheep(Canvas& c, int frame) {
  const bool up = frame >= 5;
  const Ramp wool = ramp5(rgba(150, 140, 136), rgba(196, 188, 176), rgba(226, 220, 204), rgba(242, 238, 224), rgba(252, 250, 242));
  const uint32_t face = rgba(52, 44, 50), leg = rgba(60, 50, 54);
  const int b = c.h - 1;
  for (int lx : {6, 8, 13, 15}) vl(c, lx, b - 4, b, leg);
  furBall(c, 11.0f, b - 7.5f, 7.0f, 4.6f, wool, 0.55f, 17);
  // the head
  const int hx = 2, hy = up ? b - 11 : b - 6;
  ball(c, 5.5f, up ? b - 8.5f : b - 6.5f, 2.4f, 2.2f, wool, 0.05f);   // the wool on its neck
  for (int y = 0; y < 4; y++) for (int x = 0; x < 3; x++) c.set(hx + x, hy + y, face);
  c.set(hx, hy + 1, rgba(84, 74, 78));
  c.set(hx + 3, hy, face); c.set(hx + 3, hy - 1, wool[1]);   // the ear
  c.set(hx + 1, hy + 1, kWhite);
  if (!up) { c.set(hx, hy + 4, kLeaf[3]); c.set(hx + 1, hy + 4, kLeaf[2]); }   // a mouthful of grass
  c.set(18, b - 8 + (frame == 6 ? 1 : 0), wool[2]);   // the tail
}
void cow(Canvas& c, int frame) {
  const bool up = frame >= 5;
  const Ramp hide = ramp5(rgba(70, 40, 34), rgba(108, 62, 44), rgba(146, 88, 58), rgba(178, 116, 74), rgba(204, 146, 98));
  const int b = c.h - 1;
  for (int lx : {8, 10, 20, 22}) { vl(c, lx, b - 6, b, hide[1]); c.set(lx, b, kInk); }
  ball(c, 15.5f, b - 10.0f, 10.0f, 5.6f, hide, 0.08f);
  // a white blaze on its flank, a pale belly
  const Ramp pale = ramp(rgba(232, 226, 212), 0.45f);
  for (int x = 11; x <= 19; x++) c.set(x, b - 5, pale[2]);
  for (int y = b - 14; y <= b - 8; y++)   // a pale patch over the flank
    for (int x = 13; x <= 21; x++) {
      const float dx = (x + 0.5f - 17.0f) / 4.2f, dy = (y + 0.5f - (b - 11.0f)) / 3.0f;
      if (dx * dx + dy * dy + (hashf(x, y, 13) - 0.5f) * 0.5f < 1.0f) c.set(x, y, pale[y < b - 11 ? 3 : 2]);
    }
  ball(c, 17.0f, b - 4.5f, 1.6f, 1.0f, ramp(rgba(230, 160, 160), 0.5f), 0.0f);   // the udder
  // the head
  const int hx = 1, hy = up ? b - 16 : b - 9;
  box(c, hx, hy, hx + 5, hy + 5, hide[2]);
  box(c, hx, hy + 4, hx + 3, hy + 6, rgba(214, 170, 150));   // the muzzle
  hl(c, hx + 1, hx + 5, hy, hide[3]);
  c.set(hx + 2, hy + 2, kInk);
  c.set(hx + 5, hy - 1, kBone[3]); c.set(hx + 6, hy - 2, kBone[4]); c.set(hx, hy - 1, kBone[3]);   // the horns
  c.set(hx + 6, hy + 1, hide[1]);   // the ear
  if (!up) { c.set(hx + 1, hy + 7, kLeaf[3]); c.set(hx + 2, hy + 7, kLeaf[2]); }
  // the tail, swishing
  const int sw = frame == 6 ? 1 : (frame == 7 ? -1 : 0);
  vl(c, 26 + sw, b - 12, b - 5, hide[1]);
  c.set(26 + sw, b - 4, kInk); c.set(27 + sw, b - 4, kInk);
}

// ---- a lean-to for the flock: plank back wall, a sloping roof on front posts, a hay rack and hay
void penShelter(Canvas& c) {
  const int b = c.h - 2;
  // the back wall in the shade
  for (int y = 12; y <= b - 2; y++)
    for (int x = 3; x <= 44; x++) c.set(x, y, kWoodDark[(x - 3) % 5 == 0 ? 0 : (y < 18 ? 0 : 1)]);
  // the rack on the wall, hay in it
  for (int x = 8; x <= 38; x += 3)
    for (int k = 0; k < 8; k++) c.set(x + k / 3, 18 + k, kWood[1]);
  for (int x = 8; x <= 40; x++)
    for (int y = 17; y <= 20; y++) if (hashf(x, y, 5) < 0.7f) c.set(x, y, kThatch[2 + (y == 17)]);
  // hay heaped on the floor, a pail
  ball(c, 33.0f, b - 3.5f, 9.0f, 4.0f, kThatch, 0.12f);
  ball(c, 12.0f, b - 2.0f, 5.0f, 2.2f, kThatch, 0.12f);
  for (int y = b - 5; y <= b - 1; y++) hl(c, 20, 24, y, kWood[y == b - 5 ? 4 : 2]);
  hl(c, 20, 24, b - 3, kIron[2]);
  // the roof: thatch sloping from the back down to the eave, overhanging
  for (int y = 2; y <= 12; y++) {
    const float t = (y - 2) / 10.0f;
    const int xa = (int)std::lround(2 - 2 * t), xb = (int)std::lround(45 + 2 * t);
    for (int x = xa; x <= xb; x++) {
      int k = ((x * 3 + y) % 7 == 0) ? 1 : (t < 0.3f ? 4 : (t < 0.7f ? 3 : 2));
      if (x > xb - 10) k = std::max(1, k - 1);
      c.set(x, y, kThatch[k]);
    }
  }
  hl(c, 0, 47, 13, kThatch[1]);
  for (int x = 0; x < 48; x += 3) c.set(x, 14, kThatch[1]);
  // the posts
  for (int x : {2, 23, 44})
    for (int y = 13; y <= b; y++) { c.set(x, y, kWood[3]); c.set(x + 1, y, kWood[1]); }
}

// ---- (M1 fixer round 2) the mine hill: a grassy knoll whose south face is a cliff of layered rock, the timbered adit
// cut into its foot, the rails running out of it, boulders and scree at its shoulders. 80x76, standing on the adit's
// tile; the generator fills the hill's footprint (five tiles across, four deep) with Filler. variant 0..3: its shape,
// bushes and stones; land 0 green, 1 snow, 2 dry grass
void paintMineHill(Canvas& c, int variant, int land) {
  const int W = c.w, b = c.h - 1, cx = W / 2;
  const uint32_t s = 911u + (uint32_t)variant * 37u;
  const Ramp top = land == 1 ? kSnow
                 : (land == 2 ? ramp5(rgba(110, 92, 52), rgba(150, 126, 66), rgba(186, 160, 84), rgba(212, 188, 108), rgba(234, 216, 142))
                              : ramp5(rgba(38, 70, 48), rgba(52, 98, 52), rgba(72, 128, 58), rgba(98, 152, 66), rgba(132, 178, 80)));
  const Ramp& R = kStone;
  // the outline: a mound four tiles high in the middle, sloping to the ground at its shoulders
  int topY[96], brow[96];
  for (int x = 0; x < W; x++) {
    const float u = (x + 0.5f - cx) / (W * 0.5f);
    const float h = 60.0f * std::pow(std::max(0.0f, 1.0f - u * u), 0.42f) + (vnoise(x * 0.16f, 3.0f, s) - 0.5f) * 8.0f;
    topY[x] = b - (int)h;
    // the brow: where the grassy top breaks into the cliff (only across the middle; the shoulders stay grass)
    const float f = std::fabs(u);
    brow[x] = f < 0.8f ? b - 36 - (int)((vnoise(x * 0.3f, 9.0f, s + 3) - 0.5f) * 8.0f) + (int)(f * f * f * 40.0f) : b + 1;
    if (brow[x] < topY[x] + 6) brow[x] = topY[x] + 6;
  }
  // the grassy top and shoulders, lit from the top-left
  for (int x = 0; x < W; x++)
    for (int y = std::max(0, topY[x]); y <= b; y++) {
      if (y >= brow[x]) break;
      const float u = (x + 0.5f - cx) / (W * 0.5f);
      const float t = (float)(y - topY[x]) / std::max(1, b - topY[x]);
      float l = 0.75f - u * 0.45f - t * 0.55f + (hashf(x / 2, y / 2, s) - 0.5f) * 0.35f;
      if (y == topY[x]) l += 0.35f;
      int k = l > 0.8f ? 4 : (l > 0.45f ? 3 : (l > 0.1f ? 2 : 1));
      if (std::fabs(u) > 0.66f && y > b - 5) k = std::max(1, k - 1);   // the shoulders' foot in shade
      c.set(x, y, top[k]);
    }
  // tufts of darker grass on the top
  if (land != 1)
    for (int i = 0; i < 26; i++) {
      const int x = 6 + (int)(hashf(i, 1, s) * (W - 12)), y = topY[x] + 2 + (int)(hashf(i, 2, s) * std::max(1, brow[std::min(W - 1, x)] - topY[x] - 4));
      if (y < brow[x] - 1 && y > topY[x]) { c.set(x, y, top[1]); c.set(x + 1, y - 1, top[2]); }
    }
  // the cliff: courses of rock, lit on the left, cracks and ledges, darker toward its foot
  for (int x = 0; x < W; x++)
    for (int y = brow[x]; y <= b; y++) {
      const float u = (x + 0.5f - cx) / (W * 0.5f);
      const int course = (y - brow[x] + (int)(vnoise(x * 0.2f, 1.0f, s + 5) * 3.0f)) % 6;
      float l = 0.55f - u * 0.6f - (float)(y - brow[x]) / 60.0f;
      if (course == 0) l += 0.35f;          // the lit top of each course
      if (course == 5) l -= 0.35f;          // the shadow under it
      if (hash3(x / 3, (y + x / 5) / 4, s) % 9 == 0) l -= 0.4f;   // a crack
      int k = lightIndex(l, x, y, 0.15f);
      if (y == brow[x]) k = 4;
      c.set(x, y, R[k]);
    }
  // moss and grass hanging over the brow
  for (int x = 0; x < W; x++)
    if (brow[x] <= b && hashf(x, 7, s) < 0.55f) {
      const int n = 1 + (int)(hashf(x, 8, s) * 3.0f);
      for (int k = 0; k < n; k++) c.set(x, brow[x] + k, land == 1 ? kSnow[3] : top[k == 0 ? 2 : 1]);
    }
  // crags of the rock breaking through the turf on the top
  for (int i = 0; i < 2 + (variant >> 1); i++) {
    const int x = 16 + (int)(hashf(i, 41, s) * (W - 32));
    const int y = topY[x] + 8 + (int)(hashf(i, 42, s) * 8.0f);
    if (y + 4 < brow[x]) rock(c, (float)x, (float)y, 5.0f + hashf(i, 43, s) * 3.0f, 3.5f, R, s + (uint32_t)i, 6);
  }
  // a bush or two and a few stones on the top
  for (int i = 0; i < 2 + (variant & 1); i++) {
    const int x = 14 + (int)(hashf(i, 11, s) * (W - 28));
    const int y = topY[x] + 6 + (int)(hashf(i, 12, s) * 6.0f);
    if (y + 3 < brow[x]) ball(c, x, y, 4.0 + hashf(i, 13, s) * 2.0, 3.0, land == 1 ? kPine : kLeafDark, 0.12f);
  }
  for (int i = 0; i < 4; i++) {
    const int x = 10 + (int)(hashf(i, 21, s) * (W - 20));
    const int y = topY[x] + 4 + (int)(hashf(i, 22, s) * 10.0f);
    if (y + 2 < brow[x]) ball(c, x, y, 2.0, 1.5, R, 0.1f);
  }
  // boulders and scree at the foot of the cliff, either side of the adit
  for (int sd : {-1, 1}) {
    const float bx = cx + sd * (18.0f + hashf(sd + 2, 31, s) * 6.0f);
    ball(c, bx, b - 4.0, 6.0, 4.5, R, 0.12f);
    ball(c, bx + sd * 7.0f, b - 2.0, 3.5, 2.5, R, 0.12f);
    for (int i = 0; i < 6; i++) c.set((int)bx + sd * (2 + i * 2), b - (i & 1), R[1 + (i % 3)]);
  }
  // the adit: a dark mouth going back into the rock, framed by two props and a lintel, the rails running out of it
  const int ax0 = cx - 7, ax1 = cx + 7, atop = b - 19;
  for (int y = atop; y <= b; y++)
    for (int x = ax0; x <= ax1; x++) {
      const int depth = y - atop;
      c.set(x, y, depth < 2 ? rgba(52, 42, 54) : (depth < 5 && (x < ax0 + 3 || x > ax1 - 3) ? rgba(40, 32, 44) : kInk));
    }
  for (int y = atop - 2; y <= b; y++) {
    c.set(ax0 - 2, y, kWood[3]); c.set(ax0 - 1, y, kWood[2]); c.set(ax1 + 1, y, kWood[2]); c.set(ax1 + 2, y, kWood[0]);
    if (y % 5 == 0) { c.set(ax0 - 2, y, kWood[2]); c.set(ax1 + 2, y, kWood[1]); }
  }
  for (int x = ax0 - 4; x <= ax1 + 4; x++) { c.set(x, atop - 4, kWood[4]); c.set(x, atop - 3, kWood[3]); c.set(x, atop - 2, kWood[1]); }
  for (int y = b - 8; y <= b; y++) { c.set(cx - 3 - (b - y) / 4, y, kIron[3]); c.set(cx + 3 + (b - y) / 4, y, kIron[2]); }
  for (int y = b - 7; y <= b; y += 2) hl(c, cx - 4 - (b - y) / 4, cx + 4 + (b - y) / 4, y, kWood[1]);
  // the lantern hung from the lintel
  c.set(cx + 5, atop - 1, kIron[2]); c.set(cx + 5, atop, kGlow[4]); c.set(cx + 5, atop + 1, kGlow[3]);
  c.set(cx + 4, atop, kIron[1]); c.set(cx + 6, atop, kIron[1]);
}

// ---- a tile of mine track: two iron rails on wooden sleepers
void railTile(Canvas& c, int joins) {
  const bool n = joins & 1, e = joins & 2, s = joins & 4, w = joins & 8;
  const int cnt = n + e + s + w;
  const bool corner = cnt == 2 && !(n && s) && !(e && w);
  if (corner) {
    // a curve round the corner: two sleepers fanned across it, the rails sweeping over them (a lit top, a dark side)
    const float ox = e ? 16.0f : 0.0f, oy = s ? 16.0f : 0.0f, sx = e ? -1.0f : 1.0f, sy = s ? -1.0f : 1.0f;
    for (float ang : {0.26f, 0.79f, 1.31f})
      for (float r = 3.0f; r <= 13.0f; r += 0.5f) {
        const int x = (int)std::floor(ox + sx * std::cos(ang) * r), y = (int)std::floor(oy + sy * std::sin(ang) * r);
        if (x >= 0 && x < 16 && y >= 0 && y < 16) { c.set(x, y, kWoodDark[2]); if (y + 1 < 16) c.set(x, y + 1, kWoodDark[1]); }
      }
    for (int a = 0; a <= 64; a++) {
      const float ang = a * (1.5708f / 64.0f);
      const float cs = std::cos(ang), sn = std::sin(ang);
      for (int k = 0; k < 2; k++) {
        const float r = k ? 10.0f : 5.0f;
        const int x = (int)std::floor(ox + sx * cs * r), y = (int)std::floor(oy + sy * sn * r);
        const int x2 = (int)std::floor(ox + sx * cs * (r + 1.0f)), y2 = (int)std::floor(oy + sy * sn * (r + 1.0f));
        if (x2 >= 0 && x2 < 16 && y2 >= 0 && y2 < 16) c.set(x2, y2, kIron[1]);
        if (x >= 0 && x < 16 && y >= 0 && y < 16) c.set(x, y, kIron[3]);
      }
    }
    return;
  }
  const bool any = cnt > 0;
  const bool vert = !any || n || s, horiz = e || w;
  if (vert) {
    const int y0 = (!any || n) ? 0 : 5, y1 = (!any || s) ? 15 : 10;
    for (int y = y0 + 1; y <= y1; y += 4) { hl(c, 3, 12, y, kWoodDark[2]); hl(c, 3, 12, y + 1 > y1 ? y : y + 1, kWoodDark[1]); }
    for (int y = y0; y <= y1; y++) { c.set(5, y, kIron[3]); c.set(10, y, kIron[3]); c.set(6, y, kIron[1]); c.set(11, y, kIron[1]); }
  }
  if (horiz) {
    const int x0 = w ? 0 : 5, x1 = e ? 15 : 10;
    for (int x = x0 + 1; x <= x1; x += 4) { vl(c, x, 3, 12, kWoodDark[2]); vl(c, x + 1 > x1 ? x : x + 1, 3, 12, kWoodDark[1]); }
    for (int x = x0; x <= x1; x++) { c.set(x, 5, kIron[4]); c.set(x, 10, kIron[4]); c.set(x, 6, kIron[1]); c.set(x, 11, kIron[1]); }
  }
}

// ---------------------------------------------------------------- yard props
void sacks(Canvas& c) {
  const Ramp S = ramp(rgba(206, 184, 140), 0.7f);
  const int b = c.h - 2;
  ball(c, 6.5, b - 4.0, 4.6, 4.0, S, 0.06f);
  ball(c, 13.5, b - 4.0, 4.6, 4.0, S, 0.06f);
  ball(c, 10.0, b - 8.5, 4.4, 3.8, S, 0.06f);
  for (float x : {6.5f, 13.5f}) { hl(c, (int)x - 1, (int)x + 1, b - 8, S[1]); c.set((int)x, b - 9, S[3]); }
  hl(c, 9, 11, b - 12, S[1]);
  c.set(5, b - 5, rgba(150, 110, 70)); c.set(12, b - 4, rgba(150, 110, 70));   // the stencil marks
}

void baskets(Canvas& c) {
  basketOf(c, 1, 9, c.h - 6, ramp(rgba(208, 48, 44), 0.8f), 11);
  basketOf(c, 10, 9, c.h - 5, ramp(rgba(112, 170, 70), 0.8f), 23, 2.0f);
}

void dryingRack(Canvas& c) {
  const int b = c.h - 2;
  for (int x : {2, c.w - 4}) { vl(c, x, 6, b, kWood[3]); vl(c, x + 1, 6, b, kWood[1]); }
  hl(c, 1, c.w - 2, 6, kWood[3]);
  hl(c, 1, c.w - 2, 7, kWood[1]);
  hl(c, 3, c.w - 4, 17, kWood[2]);
  const Ramp dried = ramp(rgba(184, 150, 100), 0.75f), silver = ramp(rgba(150, 170, 186), 0.75f);
  for (int r = 0; r < 2; r++)
    for (int s = 0; s < 6; s++) {
      const int x = 6 + s * 4, y0 = r == 0 ? 8 : 18;
      const Ramp& R = (s + r) % 3 == 0 ? silver : dried;
      for (int j = 0; j < 7; j++) {
        const int w = j < 1 ? 0 : (j < 5 ? 1 : 0);
        for (int i = -w; i <= w; i++) c.set(x + i, y0 + j, R[i < 0 ? 3 : (i == 0 ? 2 : 1)]);
      }
    }
}

void hideRack(Canvas& c) {
  const int b = c.h - 2;
  // the frame: two posts and two bars, the hide laced into it with thongs
  for (int x : {2, c.w - 4}) { vl(c, x, 3, b, kWood[3]); vl(c, x + 1, 3, b, kWood[1]); }
  hl(c, 1, c.w - 2, 3, kWood[3]); hl(c, 1, c.w - 2, 4, kWood[1]);
  hl(c, 2, c.w - 3, b - 5, kWood[2]);
  const Ramp H = ramp(rgba(184, 136, 92), 0.7f);
  for (int y = 7; y <= b - 8; y++) {
    const float t = (y - 7) / (float)(b - 15);
    const float hw = 6.5f + std::sin(t * 3.14159f) * 2.5f + (y % 4 == 0 ? -1 : 0);
    for (int x = (int)(c.w / 2 - hw); x <= (int)(c.w / 2 + hw); x++) {
      const float u = (x - (c.w / 2 - hw)) / (2 * hw);
      int k = u < 0.3f ? 4 : (u < 0.75f ? 3 : 2);
      if ((x * 3 + y * 5) % 13 == 0) k--;
      c.set(x, y, H[k]);
    }
  }
  for (int y = 8; y <= b - 9; y += 3) { c.set(4, y, kLeather[1]); c.set(5, y, kLeather[2]); c.set(c.w - 6, y, kLeather[1]); c.set(c.w - 5, y, kLeather[2]); }
}

void oreCart(Canvas& c) {
  const int b = c.h - 2;
  // the rail stub
  hl(c, 0, c.w - 1, b - 1, kIron[2]);
  hl(c, 0, c.w - 1, b, kWoodDark[1]);
  for (int x = 2; x < c.w; x += 6) vl(c, x, b - 1, b, kWood[1]);
  // the tub: iron-banded planks, wider at the top
  for (int y = 7; y <= b - 5; y++) {
    const int in = (y - 7) / 4;
    for (int x = 3 + in; x <= c.w - 4 - in; x++) {
      int k = x == 3 + in ? 3 : (x >= c.w - 5 - in ? 1 : 2);
      if (y == 9 || y == b - 7) k = 1;
      c.set(x, y, (y == 9 || y == b - 7) ? kIron[k + 1] : kWood[k]);
    }
  }
  // the heap of ore
  const Ramp ore = ramp(rgba(110, 96, 104), 0.8f);
  for (int i = 0; i < 6; i++) ball(c, 6.0f + i * 2.8f, 6.0f - (i % 3 == 1 ? 1.5f : 0.0f), 2.4f, 2.0f, ore, 0.08f);
  c.set(9, 4, rgba(220, 150, 80)); c.set(15, 5, rgba(220, 150, 80)); c.set(12, 6, rgba(196, 210, 222));   // a glint of metal
  // the wheels
  for (int wx : {7, c.w - 8}) { ball(c, wx, b - 3.5, 2.6, 2.6, kIron, 0.05f); c.set(wx, b - 4, kIron[4]); }
}

void orePile(Canvas& c) {
  const Ramp ore = ramp(rgba(112, 98, 106), 0.85f);
  const int b = c.h - 2;
  for (int i = 0; i < 9; i++) {
    const float x = 4.0f + (i % 5) * 3.4f + (i / 5) * 1.6f, y = b - 2.5f - (i / 5) * 3.5f;
    ball(c, x, y, 2.8f, 2.2f, ore, 0.08f);
  }
  c.set(8, b - 6, rgba(222, 150, 80)); c.set(13, b - 3, rgba(222, 150, 80)); c.set(16, b - 5, rgba(196, 210, 222));
}

void mineEntrance(Canvas& c) {
  const int b = c.h - 2, cx = c.w / 2;
  // the outcrop: a knot of weathered boulders (back ones first), each lit from the top-left, moss on their crowns
  struct Lump { float x, y, rx, ry; };
  const Lump lumps[] = {{11, 15, 11, 10}, {31, 13, 12, 11}, {21, 9, 10, 8}, {5, 26, 6, 8}, {39, 26, 6, 8}, {14, 25, 10, 9}, {30, 25, 10, 9}};
  for (const Lump& L : lumps)
    for (int y = (int)(L.y - L.ry - 1); y <= std::min(b, (int)(L.y + L.ry + 1)); y++)
      for (int x = (int)(L.x - L.rx - 1); x <= (int)(L.x + L.rx + 1); x++) {
        const float dx = (x + 0.5f - L.x) / L.rx, dy = (y + 0.5f - L.y) / L.ry;
        const float d = dx * dx + dy * dy;
        if (d > 1.0f + (hashf(x, y, 47) - 0.5f) * 0.12f) continue;
        float l = lightAt(dx * 0.85f, dy * 0.85f) + (vnoise(x * 0.45f, y * 0.45f, 43) - 0.5f) * 0.5f;
        int k = lightIndex(l, x, y, 0.1f);
        if (d > 0.82f && dy > 0.1f) k = std::max(0, k - 1);   // the shaded underside of each boulder
        uint32_t col = kStone[k];
        if (dy < -0.45f && hashf(x / 2, y / 2, 53) < 0.7f) col = kMoss[std::min(4, k + 1)];   // moss and turf on top
        c.set(x, y, col);
      }
  for (int y = 0; y < c.h; y++)   // cracks between the stones
    for (int x = 1; x < c.w - 1; x++)
      if (solid(c, x, y) && hash3(x, y, 59) % 23 == 0 && c.get(x, y) != kMoss[3]) c.set(x, y, kStone[0]);
  // the adit: a dark mouth going back into the rock, framed by two props and a lintel, a lantern by it
  for (int y = 15; y <= b; y++)
    for (int x = cx - 7; x <= cx + 7; x++) {
      const int depth = y - 15;
      c.set(x, y, depth < 2 ? rgba(52, 42, 54) : (depth < 5 && (x < cx - 4 || x > cx + 4) ? rgba(40, 32, 44) : kInk));
    }
  for (int y = 13; y <= b; y++) {
    c.set(cx - 9, y, kWood[3]); c.set(cx - 8, y, kWood[2]); c.set(cx + 8, y, kWood[2]); c.set(cx + 9, y, kWood[0]);
    if (y % 5 == 0) { c.set(cx - 9, y, kWood[2]); c.set(cx + 9, y, kWood[1]); }
  }
  for (int x = cx - 11; x <= cx + 11; x++) { c.set(x, 11, kWood[4]); c.set(x, 12, kWood[3]); c.set(x, 13, kWood[1]); }
  c.set(cx - 11, 12, kWood[2]); c.set(cx + 11, 12, kWood[1]);
  // the rails running in under the frame, sleepers across
  for (int y = b - 7; y <= b; y++) { c.set(cx - 3 - (b - y) / 4, y, kIron[3]); c.set(cx + 3 + (b - y) / 4, y, kIron[2]); }
  for (int y = b - 6; y <= b; y += 2) hl(c, cx - 4 - (b - y) / 4, cx + 4 + (b - y) / 4, y, kWood[1]);
  // the lantern hung from the lintel
  c.set(cx + 5, 14, kIron[2]); c.set(cx + 5, 15, kGlow[4]); c.set(cx + 5, 16, kGlow[3]); c.set(cx + 4, 15, kIron[1]); c.set(cx + 6, 15, kIron[1]);
}

void logPile(Canvas& c) {
  const int b = c.h - 2;
  // trunks stacked four, three, two, lying away from the viewer: their bark running back behind, their sawn ends
  // toward us with the rings showing
  struct L { float x, y; };
  std::vector<L> logs;
  for (int r = 0; r < 3; r++)
    for (int i = 0; i < 4 - r; i++) logs.push_back({6.0f + r * 3.5f + i * 7.0f, b - 3.5f - r * 5.4f});
  for (const L& l : logs)   // the bark behind each end (the log's top running back)
    for (int y = (int)l.y - 6; y <= (int)l.y; y++)
      for (int x = (int)l.x - 3; x <= (int)l.x + 3; x++) c.set(x, y, kBark[x <= (int)l.x - 2 ? 3 : (x >= (int)l.x + 2 ? 1 : 2)]);
  for (const L& l : logs) {
    ball(c, l.x, l.y, 3.3f, 3.1f, ramp(rgba(214, 170, 110), 0.6f), 0.02f);
    for (int a = 0; a < 14; a++) {   // the bark ring round the end, a growth ring inside
      const float ang = a * 0.4488f;
      c.set((int)std::floor(l.x + std::cos(ang) * 3.2f), (int)std::floor(l.y + std::sin(ang) * 3.0f), kBark[ang > 0.5f && ang < 3.6f ? 1 : 2]);
    }
    c.set((int)l.x, (int)l.y, kWood[1]);
    c.set((int)l.x - 1, (int)l.y - 1, kWood[2]);
  }
}

void trough(Canvas& c) {
  const int b = c.h - 2;
  for (int y = 3; y <= b - 2; y++)
    for (int x = 1; x < c.w - 1; x++) {
      int k = y == 3 ? 4 : (x == 1 ? 3 : (x == c.w - 2 ? 1 : 2));
      c.set(x, y, kWood[k]);
    }
  for (int x = 3; x < c.w - 3; x++) { c.set(x, 4, kWater[3]); c.set(x, 5, kWater[2]); }
  c.set(8, 4, kWater[4]); c.set(9, 4, kWater[4]);
  for (int x : {3, c.w - 4}) vl(c, x, b - 1, b, kWood[1]);
}

// The watermill's undershot wheel, seen a little from the side (foreshortened), turning: the rim, eight spokes, the
// paddles standing out of the rim, darker and wet where it dips into the race, white water churning at its foot
void waterWheelProp(Canvas& c, int frame) {
  const float cx = c.w * 0.5f, cy = 17.0f, R = 14.0f, Rx = 8.0f;
  const float rot = frame * 0.19635f;   // a sixteenth of a turn per frame: the paddles step round
  for (int y = 1; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const float fx = (x + 0.5f - cx) / Rx, fy = (y + 0.5f - cy) / R;
      const float d = std::sqrt(fx * fx + fy * fy);
      if (d > 1.12f) continue;
      const float ang = std::atan2(fy, fx) + rot;
      const bool wet = y > cy + 8;
      if (d > 0.84f) {
        const bool paddle = std::fmod(ang + 62.83185f, 0.5236f) < 0.2f;
        if (d > 0.97f && !paddle) continue;
        int k = fx + fy < -0.35f ? 4 : (fx + fy > 0.45f ? 1 : (fx < 0 ? 3 : 2));
        if (d > 0.97f) k = std::max(1, k - 1);
        c.set(x, y, (wet ? kWoodDark : kWood)[wet ? std::max(0, k - 1) : k]);
      } else if (d < 0.2f) {
        c.set(x, y, kIron[d < 0.1f ? 4 : 2]);
      } else {
        const bool spoke = std::fmod(ang + 62.83185f, 0.7854f) < 0.17f;
        if (spoke) c.set(x, y, (wet ? kWoodDark : kWood)[fx < 0 ? 3 : 2]);
      }
    }
  // the race churning white at its foot
  for (int x = 1; x < c.w - 1; x++) {
    const int y = c.h - 4 + (int)((x * 7 + frame * 3) % 3 == 0);
    c.set(x, y, (x + frame) % 3 == 0 ? kWhite : kWater[4]);
    if ((x + frame) % 4 == 0) c.set(x, y - 1, kWater[3]);
    if ((x * 5 + frame) % 6 == 0) c.set(x, y + 1, kWhite);
  }
}

}  // namespace

void setMarketSnow(float z) { g_snowZ = z; }

Canvas marketStall(int trade, int awning) { return marketStallForm(trade, awning, 0, false); }

Canvas marketStallForm(int trade, int awning, int form, bool closed) { return marketStallFacing(trade, awning, form, closed, StallS); }

Canvas marketStallFacing(int trade, int awning, int form, bool closed, int facing) {
  facing &= 3;
  const StallCanvas sc = stallCanvas(facing);
  M3 m(sc.w, sc.h, sc.x0, sc.y0, facing);
  paintStallModel(m, ((trade % kStallTrades) + kStallTrades) % kStallTrades, ((awning % kStallAwnings) + kStallAwnings) % kStallAwnings,
                  ((form % kStallForms) + kStallForms) % kStallForms, closed);
  outline(m.c, 0.95f);
  return m.c;
}

Canvas marketStallStyled(int trade, int awning, int form, bool closed, int facing, const PropStyle& st) {
  if (st.classic() || (st.awning == 0 && !st.awningA && !st.cloth)) return marketStallFacing(trade, awning, form, closed, facing);
  g_awn.on = true;
  g_awn.kind = st.awning;
  g_awn.a = st.awningA ? st.awningA : (st.cloth ? st.cloth : rgba(184, 52, 48));
  g_awn.b = st.awningB ? st.awningB : rgba(236, 226, 200);
  // a tiled lean-to: the timber booth's tiled roof over the same counter (the cloth forms become booths)
  const int f = st.awning == 3 ? 2 : form;
  Canvas c = marketStallFacing(trade, awning, f, closed, facing);
  g_awn = AwnStyle();
  return c;
}

void stallOrigin(int facing, int& dx, int& dy) {
  const StallCanvas sc = stallCanvas(facing & 3);
  dx = sc.x0;
  dy = sc.y0;
}

Canvas marketStallShadow(int form, int facing) { return stallShadowFor(((form % kStallForms) + kStallForms) % kStallForms, facing & 3); }

void stallLantern(int facing, int& lx, int& ly) {
  // hung under the roof's front corner (S), on the near back post outside the back cloth (N), on the near front post
  // at the south end (E, W)
  switch (facing & 3) {
    case StallN: lx = -12; ly = 13; break;
    // (stalls fixer round 3) on the south front post under the low eave, on the customers' side of it
    case StallE: lx = 11; ly = 6; break;
    case StallW: lx = 5; ly = 6; break;
    default: lx = -12; ly = -14; break;
  }
}

Canvas marketTable(int goods, int shade, bool closed) {
  shade = ((shade % kTableShades) + kTableShades) % kTableShades;
  M3 m(TBW, TBH, TBX0, TBY0, StallS);
  tableModel(m, shade, closed);
  if (!closed) tableGoodsM(m, goods);
  else {   // the stock under a tied cover
    const Ramp S = ramp(rgba(196, 180, 146), 0.7f);
    for (float x = 3.0f; x <= 29.0f; x += 0.4f) {
      const float a = (x - 3.0f) / 26.0f;
      for (float y = 4.0f; y <= 13.4f; y += 0.4f) {
        const float hump = std::max(0.5f, 2.0f + std::sin(a * 9.0f) * 1.5f + std::sin((13.4f - y) * 0.4f) * 1.0f);
        const float h = std::min(hump, 0.5f + (13.4f - y) * 1.5f);
        const bool cord = (int)x == 10 || (int)x == 22;
        m.plotW(x, y, TT + h, cord ? kWood[1] : S[y > 12.4f ? 2 : (std::cos(a * 9.0f) > 0.5f ? 2 : 3)]);
      }
      for (float z = TT - 2.0f; z <= TT + 0.5f; z += 0.4f) m.plotW(x, 13.8f, z, S[1], 0.3f);
    }
  }
  outline(m.c, 0.95f);
  return m.c;
}

Canvas groundCloth(int goods, int cloth, bool closed) {
  M3 m(GCW, GCH, GCX0, GCY0, StallS);
  clothModel(m, cloth, goods, closed);
  outline(m.c, 0.9f);
  return m.c;
}

void marketTableOrigin(int& dx, int& dy) { dx = TBX0; dy = TBY0; }
void groundClothOrigin(int& dx, int& dy) { dx = GCX0; dy = GCY0; }

Canvas marketTableShadow(int shade) {
  M3 m(TBW, TBH, TBX0, TBY0, StallS);
  Shade sh(kVendorShadowX, kVendorShadowY, 96, 96);
  for (float x = 2.0f; x <= 30.0f; x += 0.5f)
    for (float y = 3.0f; y <= 14.0f; y += 0.5f) { sh.cast(x, y, TT, 70); sh.cast(x, y, 0, 40); }
  if (((shade % kTableShades) + kTableShades) % kTableShades)
    for (float x = -1.0f; x <= 33.0f; x += 0.5f)
      for (float y = -8.0f; y <= 10.0f; y += 0.5f) sh.cast(x, y, 28.0f + (10.0f - y) * 4.0f / 18.0f, 80);
  (void)m;
  return sh.done();
}

Canvas groundClothShadow() {
  Shade sh(kVendorShadowX, kVendorShadowY, 96, 96);
  for (float x = 2.0f; x <= 30.0f; x += 0.5f)
    for (float y = 4.0f; y <= 14.0f; y += 0.5f) sh.cast(x, y, 3.0f, 34);
  return sh.done();
}

Canvas marketCart(int v) {
  M3 m(CAW, CAH, CAX0, CAY0, StallS);
  cartModel(m, ((v % kCartLooks) + kCartLooks) % kCartLooks);
  outline(m.c, 0.95f);
  return m.c;
}
void marketCartOrigin(int& dx, int& dy) { dx = CAX0; dy = CAY0; }
Canvas marketCartShadow(int v) {
  const bool west = (v & 4) != 0;
  Shade sh(kVendorShadowX, kVendorShadowY, 96, 96);
  for (float x = -12.0f; x <= 15.0f; x += 0.5f)
    for (float y = 2.0f; y <= 14.0f; y += 0.5f) sh.cast(west ? 16.0f - x : x, y, 9.0f, 84);
  for (float x = 15.0f; x <= 31.0f; x += 0.5f)
    for (float y : {4.0f, 12.0f}) sh.cast(west ? 16.0f - x : x, y, 9.0f - (x - 15.0f) * 0.28f, 70);
  return sh.done();
}

Canvas mineHill(int variant, int land) {
  Canvas c(propW(Prop::MineHill), propH(Prop::MineHill));
  paintMineHill(c, variant & 3, land);
  // no ink line round the turf where it meets the ground (the knoll grows out of the land, it is not stuck on it):
  // the skyline and the cliff keep a dark edge, the grassy foot of the shoulders thins out into the grass
  outline(c, 0.9f);
  const int b = c.h - 1;
  for (int y = b - 6; y <= b; y++)
    for (int x = 0; x < c.w; x++) {
      const float u = std::fabs((x + 0.5f - c.w * 0.5f) / (c.w * 0.5f));
      if (u < 0.74f) continue;
      const float fade = (float)(y - (b - 6)) / 6.0f;
      if (bayer(x, y) < fade * 0.85f) c.set(x, y, 0);
    }
  return c;
}

Canvas mineRail(int joins) {
  Canvas c(16, 16);
  railTile(c, joins & 15);
  return c;
}

void paintEconomyProp(Canvas& c, Prop p, int frame) {
  switch (p) {
    case Prop::WaterWheel: waterWheelProp(c, frame); break;
    case Prop::Sacks: sacks(c); break;
    case Prop::Baskets: baskets(c); break;
    case Prop::DryingRack: dryingRack(c); break;
    case Prop::HideRack: hideRack(c); break;
    case Prop::OreCart: oreCart(c); break;
    case Prop::OrePile: orePile(c); break;
    case Prop::MineEntrance: mineEntrance(c); break;
    case Prop::LogPile: logPile(c); break;
    case Prop::Trough: trough(c); break;
    case Prop::MarketTable: blit(c, marketTable(0, 0, false), TBX0 + 16, TBY0 + 28); break;   // (the view draws its own: a fallback)
    case Prop::GroundCloth: blit(c, groundCloth(0, 0, false), GCX0 + 16, GCY0 + 4); break;
    case Prop::MarketCross: marketCross(c); break;
    case Prop::Sheep: sheep(c, frame); break;
    case Prop::Cow: cow(c, frame); break;
    case Prop::MineRail: railTile(c, 5); break;
    case Prop::PenShelter: penShelter(c); break;
    case Prop::MineHill: paintMineHill(c, 0, 0); break;
    default: break;
  }
}

}  // namespace art
