// Endless-world coordinates (VISION_PLAN 2.1). M0 prep: not used by gameplay yet.
// Global tile coordinates are int32; floor division uses arithmetic shifts (well defined for negatives in C++20).
#pragma once
#include <cstdint>

namespace ew {

constexpr int CHUNK = 32, REGION = 256, KCELL = 1024, CCELL = 2048, LCELL = 6144;
constexpr int CHUNK_SHIFT = 5, REGION_SHIFT = 8;
constexpr int32_t WORLD_EDGE = 1000000;   // the soft "World's End" at |tile| = 1e6

struct GTile { int32_t x = 0, y = 0; };
struct GChunk { int32_t x = 0, y = 0; };
struct GRegion { int32_t x = 0, y = 0; };

inline int32_t chunkOf(int32_t t) { return t >> CHUNK_SHIFT; }
inline int32_t regionOf(int32_t t) { return t >> REGION_SHIFT; }
inline int32_t floorDiv(int32_t a, int32_t b) {
  int32_t q = a / b;
  return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}
inline int32_t floorMod(int32_t a, int32_t b) { return a - floorDiv(a, b) * b; }
inline GChunk chunkOf(GTile t) { return {chunkOf(t.x), chunkOf(t.y)}; }
inline GRegion regionOf(GTile t) { return {regionOf(t.x), regionOf(t.y)}; }
inline GTile chunkOrigin(GChunk c) { return {c.x * CHUNK, c.y * CHUNK}; }
inline GTile regionOrigin(GRegion r) { return {r.x * REGION, r.y * REGION}; }

}  // namespace ew
