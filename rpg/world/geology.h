// M2: a deterministic GEOLOGY per geological province (owner 2026-10-05, VISION_PLAN 15.11): what the land is made of
// and which ores it favours, recorded now so the M6 ores, smelters and culture alloys land in the right places. Data
// only in M2 (plus the world map's geology debug view). Shared contract, frozen after M2 phase A: the WORLD lane
// computes it (rpg/world/geology.cpp, EndlessSource::geology), the VIEW lane shows it, M6 reads it.
//
// Rules the generator follows (VISION_PLAN 15.11: ores are regional, no region has everything):
//   - a pure function of (seed, global tile), integer maths only, constant over a province (about a region or two
//     across, edges following the macro fields, not the region grid), so a mining village and the mine beside it agree;
//   - rock follows the macro fields: granite / slate in old ranges, basalt along rifts and volcanic ground, limestone /
//     marble in wet lowlands and hills, sandstone in dry country;
//   - ore affinities follow the rock and the land: copper and tin are common but rarely together (bronze needs trade),
//     iron in granite and slate highlands, coal in lowland basins and limestone, silver in old granite and marble ranges,
//     "rare" (the metals of the culture alloys, M3/M6) only in a few remote provinces;
//   - every province has a primary ore with affinity >= 160, at most two others above 96, and the rest low.
#pragma once
#include <cstddef>
#include <cstdint>

namespace ew {

enum class Rock : uint8_t { Granite, Basalt, Limestone, Sandstone, Slate, Marble, COUNT };
enum class Ore : uint8_t { Copper, Tin, Iron, Coal, Silver, Rare, COUNT };

struct Geology {
  Rock rock = Rock::Granite;
  uint8_t ore[(int)Ore::COUNT] = {};   // affinity 0..255 per ore: how likely a vein or mine here yields it
  uint32_t province = 0;               // which province (a hash: equal for every tile of one province)
  Ore primary() const {
    int best = 0;
    for (int i = 1; i < (int)Ore::COUNT; i++) if (ore[i] > ore[best]) best = i;
    return (Ore)best;
  }
};

inline const char* rockName(Rock r) {
  static const char* n[] = {"GRANITE", "BASALT", "LIMESTONE", "SANDSTONE", "SLATE", "MARBLE"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Rock::COUNT, "a name for every rock");
  return (int)r < (int)Rock::COUNT ? n[(int)r] : "STONE";
}
inline const char* oreName(Ore o) {
  static const char* n[] = {"COPPER", "TIN", "IRON", "COAL", "SILVER", "RARE METAL"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Ore::COUNT, "a name for every ore");
  return (int)o < (int)Ore::COUNT ? n[(int)o] : "ORE";
}

}  // namespace ew
