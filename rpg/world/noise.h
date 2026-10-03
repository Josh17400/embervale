// Fixed-point value noise (VISION_PLAN 2.4): exact on every platform. Values are Q16 in [0, 65536).
// M0 prep: not used by gameplay yet; tools/tests/golden.cpp pins its output (rpg_test --golden).
#pragma once
#include <cstdint>
#include "rpg/world/dmath.h"
#include "rpg/world/ids.h"

namespace ew {

inline int32_t h16(int32_t x, int32_t y, uint64_t s) { return (int32_t)(mix64(s ^ ((uint64_t)(uint32_t)x << 32 | (uint32_t)y)) >> 48); }

// value noise; lattice spacing 2^shift tiles (shift <= 12)
inline int32_t vnoiseQ(int32_t x, int32_t y, int shift, uint64_t s) {
  int32_t m = (1 << shift) - 1, cx = x >> shift, cy = y >> shift;
  int32_t fx = smoothQ((int32_t)(((int64_t)(x & m) << 16) >> shift)), fy = smoothQ((int32_t)(((int64_t)(y & m) << 16) >> shift));
  int32_t a = h16(cx, cy, s), b = h16(cx + 1, cy, s), c = h16(cx, cy + 1, s), d = h16(cx + 1, cy + 1, s);
  int32_t ab = a + (int32_t)(((int64_t)(b - a) * fx) >> 16), cd = c + (int32_t)(((int64_t)(d - c) * fx) >> 16);
  return ab + (int32_t)(((int64_t)(cd - ab) * fy) >> 16);
}

// fractal sum: octaves from lattice 2^shift down, halving amplitude, normalised back to [0, 65536)
inline int32_t fbmQ(int32_t x, int32_t y, int shift, int octaves, uint64_t s) {
  int64_t sum = 0, norm = 0;
  int32_t amp = 1 << 14;
  for (int o = 0; o < octaves && shift - o >= 0; o++) {
    sum += (int64_t)vnoiseQ(x, y, shift - o, mix64(s + (uint64_t)o)) * amp;
    norm += amp;
    amp >>= 1;
  }
  return norm ? (int32_t)(sum / norm) : 0;
}

// domain warp: offsets (x, y) by up to +-amp tiles using two independent noise fields
inline void warpQ(int32_t& x, int32_t& y, int shift, int32_t amp, uint64_t s) {
  int32_t wx = vnoiseQ(x, y, shift, mix64(s ^ 0x5157u)) - 32768, wy = vnoiseQ(x, y, shift, mix64(s ^ 0x9A3Du)) - 32768;
  x += (int32_t)(((int64_t)wx * amp * 2) >> 16);
  y += (int32_t)(((int64_t)wy * amp * 2) >> 16);
}

}  // namespace ew
