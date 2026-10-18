// EMBERVALE art: the story's hand cart (art_handcart.h). Moved verbatim out of rpg/view/render.cpp in P2a phase A
// (pixel-identical); P2a HOLDERS owns it from here and scales it with the rig (art::bodyScale()).
#include "rpg/art/art_handcart.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "engine/mathx.h"
#include "rpg/art/art_rig.h"

namespace art {

namespace {

// rpg/sim/common.h's hash (the art library does not include the simulation's headers)
inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}

// ---------------------------------------------------------------- (P1b ENGINE lane) a story's hand cart
// The wagon an escort's driver pulls (goal escort ... wagon): a two-wheeled hand cart on raised shafts, modelled in
// 3/4 like the market's carts (a small voxel model: the bed, its boards, the load, the wheels and the shafts, shaded by
// the top-left light and outlined in ink) and painted for the four headings (the wheels face on when it rolls east or
// west, edge on when it rolls north or south), its spokes in four turns. Its ground shadow is cast from the same model.
// key: dir (0 east, 1 west, 2 north, 3 south) | load << 2 (0 sacks, 1 crates, 2 barrels, 3 hay) | turn << 4 | shadow << 6
constexpr int HC_W = 64, HC_H = 50, HC_OX = 32, HC_OY = 38;   // the cart's ground point (its axle's foot) in the canvas
// (P2a HOLDERS) at rig 2 the whole cart is modelled k = 32 / 22 bigger (its wheel kHandcartWheel across, the driver's
// shafts at the v2 hands) on a taller canvas; the ground point (HC_OX, HC_OY) stays, so the view places it as before
constexpr int HC_H2 = 58;
inline float hcK() { return rigVersion() >= 2 ? 32.0f / 22.0f : 1.0f; }
inline int hcH() { return rigVersion() >= 2 ? HC_H2 : HC_H; }
struct HcModel {
  float k = hcK();
  Canvas c{HC_W, hcH()};
  std::vector<float> depth = std::vector<float>((size_t)HC_W * hcH(), -1e9f);
  Canvas sh{HC_W, hcH()};
  int dir = 0;
  // model space: u forward (toward the driver), v to the cart's right, z up; world: x east, y south (the ground's depth)
  void toWorld(float u, float v, float& x, float& y) const {
    switch (dir) {
      case 0: x = u; y = v; break;          // east
      case 1: x = -u; y = -v; break;        // west
      case 2: x = v; y = -u; break;         // north
      default: x = -v; y = u; break;        // south
    }
  }
  void nWorld(float nu, float nv, float& nx, float& ny) const { toWorld(nu, nv, nx, ny); }
  // the top-left light (from the north-west, high): 0..4 on a ramp
  static int lightK(float nx, float ny, float nz) {
    const float l = -0.48f * nx - 0.52f * ny + 0.71f * nz;
    return l > 0.62f ? 4 : l > 0.32f ? 3 : l > 0.02f ? 2 : l > -0.3f ? 1 : 0;
  }
  void plot(float u, float v, float z, uint32_t col) {
    u *= k; v *= k; z *= k;
    float x, y;
    toWorld(u, v, x, y);
    const int sx = (int)std::floor(HC_OX + x + 0.5f), sy = (int)std::floor(HC_OY + y - z + 0.5f);
    if (sx < 0 || sy < 0 || sx >= c.w || sy >= c.h) return;
    float& d = depth[(size_t)sy * c.w + sx];
    if (y + z * 0.001f < d) return;
    d = y + z * 0.001f;
    c.set(sx, sy, col);
    // its shadow on the ground, cast to the lower right (the light's way)
    const int hx = (int)std::floor(HC_OX + x + z * 0.55f + 0.5f), hy = (int)std::floor(HC_OY + y + z * 0.38f + 0.5f);
    sh.set(hx, hy, 0xFF000000u);
  }
};
uint32_t hcRgb(int r, int g, int b) { return rgba((uint8_t)r, (uint8_t)g, (uint8_t)b); }
// ramps, darkest first (the game's warm wood, iron, sacking, hay)
const uint32_t* hcWood() { static const uint32_t r[5] = {hcRgb(62, 38, 24), hcRgb(96, 60, 34), hcRgb(134, 88, 50), hcRgb(170, 118, 70), hcRgb(204, 156, 98)}; return r; }
const uint32_t* hcWoodDark() { static const uint32_t r[5] = {hcRgb(44, 28, 20), hcRgb(70, 44, 28), hcRgb(98, 64, 38), hcRgb(128, 86, 52), hcRgb(156, 110, 68)}; return r; }
const uint32_t* hcIron() { static const uint32_t r[5] = {hcRgb(40, 40, 48), hcRgb(66, 66, 76), hcRgb(98, 98, 108), hcRgb(140, 140, 150), hcRgb(186, 186, 192)}; return r; }
const uint32_t* hcSack() { static const uint32_t r[5] = {hcRgb(112, 92, 64), hcRgb(146, 122, 86), hcRgb(178, 154, 112), hcRgb(206, 184, 140), hcRgb(230, 214, 176)}; return r; }
const uint32_t* hcHay() { static const uint32_t r[5] = {hcRgb(122, 92, 40), hcRgb(160, 124, 52), hcRgb(196, 160, 72), hcRgb(222, 192, 104), hcRgb(240, 220, 150)}; return r; }

// a box in model space, each face lit by its normal (the far / north faces are hidden by the depth test)
void hcBox(HcModel& m, float u0, float u1, float v0, float v1, float z0, float z1, const uint32_t* R, float grain = 0) {
  auto face = [&](float nu, float nv, float nz) {
    float nx, ny;
    m.nWorld(nu, nv, nx, ny);
    return HcModel::lightK(nx, ny, nz);
  };
  const float st = 0.45f;
  const int kTop = face(0, 0, 1);
  for (float u = u0; u <= u1; u += st)
    for (float v = v0; v <= v1; v += st) {
      int k = kTop;
      if (grain > 0 && (int)std::floor((u - u0) / grain) % 2 == 1) k = std::max(0, k - 1);
      m.plot(u, v, z1, R[k]);
    }
  const int kU0 = face(-1, 0, 0), kU1 = face(1, 0, 0), kV0 = face(0, -1, 0), kV1 = face(0, 1, 0);
  for (float z = z0; z <= z1; z += st) {
    for (float v = v0; v <= v1; v += st) { m.plot(u0, v, z, R[kU0]); m.plot(u1, v, z, R[kU1]); }
    for (float u = u0; u <= u1; u += st) {
      const bool seam = grain > 0 && std::fmod(u - u0 + 0.01f, grain) < st;
      m.plot(u, v0, z, R[seam ? std::max(0, kV0 - 1) : kV0]);
      m.plot(u, v1, z, R[seam ? std::max(0, kV1 - 1) : kV1]);
    }
  }
}
// an ellipsoid's upper part (a sack, a heap of hay): lit by its own normal
void hcBlob(HcModel& m, float cu, float cv, float cz, float ru, float rv, float rz, const uint32_t* R, uint32_t seed, bool rough) {
  for (float a = 0; a < 6.2832f; a += 0.09f / m.k)
    for (float b = -0.2f; b < 1.5708f; b += 0.08f / m.k) {
      const float cb = std::cos(b), sb = std::sin(b);
      const float nu = std::cos(a) * cb, nv = std::sin(a) * cb, nz = sb;
      float nx, ny;
      m.nWorld(nu, nv, nx, ny);
      int k = HcModel::lightK(nx, ny, nz);
      if (rough) k = std::clamp(k + (int)(hash32(seed + (uint32_t)(a * 40) * 131u + (uint32_t)(b * 50) * 7u) % 3u) - 1, 0, 4);
      m.plot(cu + nu * ru, cv + nv * rv, cz + nz * rz, R[k]);
    }
}
// a wheel: rim, eight spokes turned by `turn` (a quarter of a spoke's step each), the iron hub; plane at v = vw
void hcWheel(HcModel& m, float uw, float vw, float zc, float r, int turn) {
  const uint32_t* W = hcWood();
  const uint32_t* D = hcWoodDark();
  const uint32_t* I = hcIron();
  const float ph = turn * (0.7854f / 4.0f);
  for (float t = -0.7f; t <= 0.7f; t += 0.35f) {
    for (float a = 0; a < 6.2832f; a += 0.025f) {
      const float ca = std::cos(a), sa = std::sin(a);
      float nx, ny;
      m.nWorld(ca, 0, nx, ny);
      const int k = HcModel::lightK(nx, ny, sa);
      for (float rr = r - 1.3f; rr <= r; rr += 0.35f) m.plot(uw + ca * rr, vw + t, zc + sa * rr, rr > r - 0.4f ? I[std::clamp(k - 1, 1, 3)] : W[std::max(1, k)]);
    }
    for (int s = 0; s < 8; s++) {
      const float a = ph + s * 0.7854f;
      for (float rr = 1.0f; rr < r - 1.0f; rr += 0.3f) m.plot(uw + std::cos(a) * rr, vw + t * 0.5f, zc + std::sin(a) * rr, D[2 + (s & 1)]);
    }
  }
  for (float rr = 0; rr <= 1.4f; rr += 0.3f)
    for (float a = 0; a < 6.2832f; a += 0.3f) m.plot(uw + std::cos(a) * rr, vw + (vw > 0 ? 0.9f : -0.9f), zc + std::sin(a) * rr, I[rr < 0.7f ? 4 : 2]);
}
// ink round the opaque pixels (the game's outline weight), and the shadow softened to a dither
void hcFinish(Canvas& c, float weight) {
  Canvas o = c;
  const uint32_t ink = rgba(28, 20, 18);
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      if (c.get(x, y) >> 24) continue;
      const bool edge = (c.get(x - 1, y) >> 24) || (c.get(x + 1, y) >> 24) || (c.get(x, y - 1) >> 24) || (c.get(x, y + 1) >> 24);
      if (edge) o.set(x, y, (ink & 0x00FFFFFFu) | ((uint32_t)(weight * 255.0f) << 24));
    }
  c = o;
}
Canvas paintHandcart(uint64_t key) {
  HcModel m;
  m.dir = (int)(key & 3);
  const int load = (int)((key >> 2) & 3), turn = (int)((key >> 4) & 3);
  const bool shadow = ((key >> 6) & 1) != 0;
  const uint32_t* W = hcWood();
  const uint32_t* D = hcWoodDark();
  // the far wheel first (the depth test sorts the rest), the bed, the boards
  hcWheel(m, -1.0f, -6.8f, 5.2f, 5.2f, turn);
  hcBox(m, -11.0f, 11.0f, -6.0f, 6.0f, 6.0f, 7.0f, D);                 // the floor
  hcBox(m, -11.0f, 11.0f, -6.0f, -5.0f, 7.0f, 11.0f, W, 3.6f);         // the side boards
  hcBox(m, -11.0f, 11.0f, 5.0f, 6.0f, 7.0f, 11.0f, W, 3.6f);
  hcBox(m, -11.0f, -10.0f, -5.0f, 5.0f, 7.0f, 11.5f, D, 3.0f);         // the tailboard, a little higher
  hcBox(m, 10.0f, 11.0f, -5.0f, 5.0f, 7.0f, 10.0f, W, 3.0f);           // the front board
  for (float u : {-6.0f, 0.5f, 6.5f}) hcBox(m, u, u + 0.9f, -6.3f, -5.8f, 7.0f, 11.3f, D);   // the stakes
  for (float u : {-6.0f, 0.5f, 6.5f}) hcBox(m, u, u + 0.9f, 5.8f, 6.3f, 7.0f, 11.3f, D);
  // the load
  const uint32_t seed = (uint32_t)(key * 2654435761u);
  if (load == 0) {
    hcBlob(m, -6.0f, -2.6f, 9.0f, 3.6f, 2.8f, 3.8f, hcSack(), seed, false);
    hcBlob(m, 0.4f, 2.4f, 9.0f, 3.6f, 2.8f, 3.6f, hcSack(), seed + 1, false);
    hcBlob(m, 5.6f, -2.0f, 9.0f, 3.2f, 2.8f, 3.4f, hcSack(), seed + 2, false);
    hcBlob(m, -2.2f, 0.4f, 12.0f, 3.2f, 2.6f, 3.0f, hcSack(), seed + 3, false);
    for (float u : {-6.0f, 0.4f, 5.6f}) for (float t = -0.6f; t <= 0.6f; t += 0.3f) m.plot(u, (u > 0 && u < 2) ? 2.4f : -2.0f, 12.4f + t, hcSack()[0]);   // the ties
  } else if (load == 1) {
    hcBox(m, -9.5f, -2.0f, -4.6f, 0.4f, 7.0f, 13.0f, W, 2.5f);
    hcBox(m, -1.0f, 6.5f, -0.6f, 4.6f, 7.0f, 12.0f, D, 2.5f);
    hcBox(m, -7.5f, -1.0f, -3.4f, 1.6f, 13.0f, 17.0f, W, 2.2f);
  } else if (load == 2) {
    for (int i = 0; i < 3; i++) {
      const float cu = -6.5f + i * 6.0f, cv = (i & 1) ? 1.8f : -1.6f;
      for (float z = 7.0f; z <= 14.0f; z += 0.4f) {
        const float bulge = 2.6f + std::sin((z - 7.0f) / 7.0f * 3.1416f) * 0.6f;
        for (float a = 0; a < 6.2832f; a += 0.1f) {
          float nx, ny;
          m.nWorld(std::cos(a), std::sin(a), nx, ny);
          const bool hoop = std::fabs(z - 8.5f) < 0.4f || std::fabs(z - 12.5f) < 0.4f;
          m.plot(cu + std::cos(a) * bulge, cv + std::sin(a) * bulge, z, hoop ? hcIron()[HcModel::lightK(nx, ny, 0.1f)] : W[HcModel::lightK(nx, ny, 0.1f)]);
        }
      }
      for (float r = 0; r <= 2.6f; r += 0.4f)
        for (float a = 0; a < 6.2832f; a += 0.2f) m.plot(cu + std::cos(a) * r, cv + std::sin(a) * r, 14.0f, r > 2.2f ? D[3] : W[4]);
    }
  } else {
    hcBlob(m, 0.0f, 0.0f, 9.5f, 10.5f, 5.6f, 6.5f, hcHay(), seed, true);
    for (int i = 0; i < 14; i++) {   // straws standing out of the heap
      const uint32_t h = hash32(seed + (uint32_t)i * 977u);
      const float u = -8.0f + (float)(h % 160u) / 10.0f, v = -4.0f + (float)((h >> 8) % 80u) / 10.0f;
      for (float t = 0; t < 1.8f; t += 0.4f) m.plot(u + t * 0.6f, v, 13.5f + t, hcHay()[(h >> 16) % 2u ? 4 : 3]);
    }
  }
  // the shafts, raised to the driver's hands, and the cross bar at their tips (rig 2: a little shorter, so the bigger
  // cart stays inside its canvas either way round)
  const float shaftEnd = m.k != 1.0f ? 21.3f : 23.5f, barU = m.k != 1.0f ? 20.8f : 22.5f;
  for (float vs : {-4.4f, 4.4f})
    for (float u = 11.0f; u <= shaftEnd; u += 0.3f) {
      const float z = 9.5f + (u - 11.0f) * 0.26f;
      m.plot(u, vs, z + 0.6f, W[3]);
      m.plot(u, vs, z, W[1]);
      m.plot(u, vs + 0.5f, z, W[2]);
    }
  for (float v = -4.4f; v <= 4.4f; v += 0.3f) { m.plot(barU, v, 13.4f, W[3]); m.plot(barU, v, 12.8f, W[1]); }
  // the near wheel over the bed's side
  hcWheel(m, -1.0f, 6.8f, 5.2f, 5.2f, turn);
  if (shadow) {
    // the cast shadow: a soft-edged dither of the model's footprint (drawn under the cart at reduced alpha)
    Canvas s(HC_W, m.c.h);
    for (int y = 1; y < m.c.h - 1; y++)
      for (int x = 1; x < HC_W - 1; x++) {
        int n = 0;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) n += (m.sh.get(x + dx, y + dy) >> 24) ? 1 : 0;
        if (n >= 5) s.set(x, y, rgba(0, 0, 0, 255));
        else if (n >= 2 && ((x + y) & 1)) s.set(x, y, rgba(0, 0, 0, 255));
      }
    return s;
  }
  hcFinish(m.c, 0.95f);
  return m.c;
}
}  // namespace

Canvas handcartSprite(uint64_t key) { return paintHandcart(key); }

// which of the four paintings a heading (radians, toward the driver) takes
int handcartDir(float ang) {
  const float cx = std::cos(ang), cy = std::sin(ang);
  if (std::fabs(cx) >= std::fabs(cy) * 0.9f) return cx >= 0 ? 0 : 1;
  return cy < 0 ? 2 : 3;
}

}  // namespace art
