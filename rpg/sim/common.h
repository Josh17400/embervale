// Shared helpers for the EMBERVALE simulation: hashing, value noise, small geometry. No SDL.
#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include "engine/mathx.h"

constexpr int TILE = 16;
constexpr float SIM_DT = 1.0f / 60.0f;

inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}
inline uint32_t hash2(int x, int y, uint32_t seed = 0) {
  return hash32((uint32_t)x * 0x9E3779B1u ^ hash32((uint32_t)y * 0x85EBCA77u ^ seed * 0xC2B2AE3Du));
}
inline float hashf(int x, int y, uint32_t seed = 0) { return (hash2(x, y, seed) >> 8) * (1.0f / 16777216.0f); }

inline float smooth01(float t) { return t * t * (3 - 2 * t); }
// 2D value noise in [0,1]
inline float vnoise(float x, float y, uint32_t seed) {
  int xi = (int)std::floor(x), yi = (int)std::floor(y);
  float fx = smooth01(x - xi), fy = smooth01(y - yi);
  float a = hashf(xi, yi, seed), b = hashf(xi + 1, yi, seed), c = hashf(xi, yi + 1, seed), d = hashf(xi + 1, yi + 1, seed);
  return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fy);
}
inline float fbm(float x, float y, uint32_t seed, int oct = 5) {
  float s = 0, amp = 0.5f, norm = 0;
  for (int i = 0; i < oct; i++) {
    s += vnoise(x, y, seed + i * 1013) * amp;
    norm += amp; amp *= 0.5f; x *= 2.03f; y *= 2.03f;
  }
  return s / norm;
}

struct IRect {
  int x = 0, y = 0, w = 0, h = 0;
  bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
  bool overlaps(const IRect& o, int pad = 0) const {
    return x - pad < o.x + o.w && o.x - pad < x + w && y - pad < o.y + o.h && o.y - pad < y + h;
  }
  int cx() const { return x + w / 2; }
  int cy() const { return y + h / 2; }
};

inline std::string upper(std::string s) {
  for (char& c : s) if (c >= 'a' && c <= 'z') c = (char)(c - 32);
  return s;
}

// little-endian binary writer/reader for saves
struct BinW {
  std::vector<uint8_t>& b;
  explicit BinW(std::vector<uint8_t>& buf) : b(buf) {}
  void u8(uint8_t v) { b.push_back(v); }
  void u16(uint16_t v) { u8((uint8_t)v); u8((uint8_t)(v >> 8)); }
  void u32(uint32_t v) { u16((uint16_t)v); u16((uint16_t)(v >> 16)); }
  void u64(uint64_t v) { u32((uint32_t)v); u32((uint32_t)(v >> 32)); }
  void i32(int32_t v) { u32((uint32_t)v); }
  void f32(float v) { uint32_t u; std::memcpy(&u, &v, 4); u32(u); }
  void str(const std::string& s) { u16((uint16_t)s.size()); for (char c : s) u8((uint8_t)c); }
};
struct BinR {
  const std::vector<uint8_t>& b;
  size_t p = 0;
  bool bad = false;
  explicit BinR(const std::vector<uint8_t>& buf) : b(buf) {}
  uint8_t u8() { if (p >= b.size()) { bad = true; return 0; } return b[p++]; }
  uint16_t u16() { uint16_t lo = u8(); return (uint16_t)(lo | (u8() << 8)); }
  uint32_t u32() { uint32_t lo = u16(); return lo | ((uint32_t)u16() << 16); }
  uint64_t u64() { uint64_t lo = u32(); return lo | ((uint64_t)u32() << 32); }
  int32_t i32() { return (int32_t)u32(); }
  float f32() { uint32_t u = u32(); float v; std::memcpy(&v, &u, 4); return v; }
  std::string str() { int n = u16(); std::string s; for (int i = 0; i < n && !bad; i++) s.push_back((char)u8()); return s; }
};
