// Stable 64-bit identities and cell seeds (VISION_PLAN 2.2, 2.3). M0 prep: not used by gameplay yet.
// Layout: [63..40 rx:24][39..16 ry:24][15..12 kind:4][11..0 local:12]
#pragma once
#include <cstdint>

namespace ew {

enum class IdKind : uint8_t { None = 0, Site = 1, Bldg = 2, Den = 3, Npc = 4, Poi = 5, Plot = 6, Edge = 7, Kingdom = 8, Legacy = 15 };
using Gid = uint64_t;

inline Gid makeId(int32_t rx, int32_t ry, IdKind k, uint32_t local) {
  return ((uint64_t)(uint32_t)(rx & 0xFFFFFF) << 40) | ((uint64_t)(uint32_t)(ry & 0xFFFFFF) << 16) | ((uint64_t)k << 12) |
         (local & 0xFFF);
}
inline int32_t signExtend24(uint32_t v) { return (v & 0x800000u) ? (int32_t)(v | 0xFF000000u) : (int32_t)v; }
inline int32_t idRx(Gid g) { return signExtend24((uint32_t)(g >> 40) & 0xFFFFFFu); }
inline int32_t idRy(Gid g) { return signExtend24((uint32_t)(g >> 16) & 0xFFFFFFu); }
inline IdKind idKind(Gid g) { return (IdKind)((g >> 12) & 15); }
inline uint32_t idLocal(Gid g) { return (uint32_t)(g & 0xFFF); }
// legacy (classic island) identities: sites by index, buildings by index with bit 11 set
inline Gid legacySiteId(int index) { return makeId(0, 0, IdKind::Legacy, (uint32_t)index & 0x7FF); }
inline Gid legacyBldgId(int index) { return makeId(0, 0, IdKind::Legacy, 0x800u | ((uint32_t)index & 0x7FF)); }

// ---- seeds
inline uint64_t mix64(uint64_t z) {   // splitmix64 finaliser
  z += 0x9E3779B97F4A7C15ull;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}
constexpr uint64_t tag(const char* s) {
  uint64_t h = 1469598103934665603ull;
  while (*s) { h ^= (uint8_t)*s++; h *= 1099511628211ull; }
  return h;
}
// The only randomness a generated thing may use: its own cell's seed (order-independence contract, VISION_PLAN 2.3)
inline uint64_t cellSeed(uint64_t world, uint64_t layerTag, int32_t x, int32_t y) {
  return mix64(world ^ mix64(layerTag ^ mix64(((uint64_t)(uint32_t)x << 32) | (uint32_t)y)));
}

}  // namespace ew
