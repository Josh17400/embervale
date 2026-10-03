// Deterministic math for structural generation (VISION_PLAN 2.4): integer / Q16 fixed point and fixed polynomial
// approximations of sin, cos and atan2 that give the same bits on MSVC, clang (iOS) and Emscripten. Only + - * /
// on floats (IEEE exact), no libm transcendental calls. M0 prep: not used by gameplay yet.
// Compile users with /fp:precise (MSVC) or -ffp-contract=off (clang) once rpg/world/*.cpp exists (M1).
#pragma once
#include <cstdint>

namespace ew {

using q16 = int32_t;   // 16.16 fixed point
constexpr q16 Q_ONE = 65536;
inline q16 qmul(q16 a, q16 b) { return (q16)(((int64_t)a * b) >> 16); }
inline q16 qdiv(q16 a, q16 b) { return b ? (q16)(((int64_t)a * 65536) / b) : 0; }
inline q16 qlerp(q16 a, q16 b, q16 t) { return a + qmul(b - a, t); }
inline q16 smoothQ(q16 t) { return (q16)((((int64_t)t * t >> 16) * (3 * 65536 - 2 * (int64_t)t)) >> 16); }   // 3t^2-2t^3

// integer square root (floor)
inline uint32_t isqrt(uint64_t v) {
  uint64_t r = 0, bit = 1ull << 62;
  while (bit > v) bit >>= 2;
  while (bit) {
    if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
    else r >>= 1;
    bit >>= 2;
  }
  return (uint32_t)r;
}

// ---- float approximations
constexpr float D_PI = 3.14159265358979f, D_TAU = 6.28318530717959f;
// wrap to [-pi, pi]
inline float dwrap(float a) {
  float k = a / D_TAU;
  int64_t n = (int64_t)(k >= 0 ? k + 0.5f : k - 0.5f);
  return a - (float)n * D_TAU;
}
// sin: 9th-order odd polynomial on [-pi/2, pi/2] (max error about 4e-6)
inline float dsin(float a) {
  a = dwrap(a);
  if (a > D_PI / 2) a = D_PI - a;
  else if (a < -D_PI / 2) a = -D_PI - a;
  float x2 = a * a;
  return a * (1.0f + x2 * (-0.16666667f + x2 * (0.0083333310f + x2 * (-0.00019840874f + x2 * 2.7525562e-6f))));
}
inline float dcos(float a) { return dsin(a + D_PI / 2); }
// atan on [-1, 1] (max error about 1e-5 rad), then the octant fix-up
inline float datanUnit(float z) {
  float z2 = z * z;
  return z * (0.99997726f + z2 * (-0.33262347f + z2 * (0.19354346f + z2 * (-0.11643287f + z2 * (0.05265332f - z2 * 0.01172120f)))));
}
inline float datan2(float y, float x) {
  if (x == 0.0f && y == 0.0f) return 0.0f;
  float ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
  float r = ay <= ax ? datanUnit(ay / ax) : D_PI / 2 - datanUnit(ax / ay);
  if (x < 0) r = D_PI - r;
  return y < 0 ? -r : r;
}

}  // namespace ew
