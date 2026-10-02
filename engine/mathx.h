// Small math + RNG helpers shared by engine and game.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

constexpr float PI = 3.14159265358979f;
constexpr float TAU = 6.28318530717959f;

struct Vec2 {
  float x = 0, y = 0;
  Vec2() = default;
  Vec2(float x_, float y_) : x(x_), y(y_) {}
  Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
  Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
  Vec2 operator*(float s) const { return {x * s, y * s}; }
  Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
  Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
  Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
};
inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
inline float len2(Vec2 a) { return dot(a, a); }
inline float len(Vec2 a) { return std::sqrt(dot(a, a)); }
inline Vec2 norm(Vec2 a) { float l = len(a); return l > 1e-6f ? a * (1.0f / l) : Vec2{1, 0}; }
inline Vec2 fromAngle(float a) { return {std::cos(a), std::sin(a)}; }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float wrapAngle(float a) {
  while (a > PI) a -= TAU;
  while (a < -PI) a += TAU;
  return a;
}

// xorshift64* : seedable, deterministic (daily-seed runs rely on this).
struct Rng {
  uint64_t s = 0x9E3779B97F4A7C15ull;
  explicit Rng(uint64_t seed = 1) { s = seed * 0x9E3779B97F4A7C15ull + 0x1234567ull; if (!s) s = 1; }
  uint32_t next() {
    s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
    return (uint32_t)((s * 0x2545F4914F6CDD1Dull) >> 32);
  }
  float f() { return (next() >> 8) * (1.0f / 16777216.0f); }          // [0,1)
  float range(float a, float b) { return a + (b - a) * f(); }
  int irange(int n) { return (int)(next() % (uint32_t)n); }           // [0,n)
};
