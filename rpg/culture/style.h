// Architecture styles (VISION_PLAN 5.5): the parameters the building painter takes, so roofs, walls and details
// can follow biome now (M0) and culture later (M3) without reworking the art API.
// M0: owned by the architecture lane (style.h, style.cpp, rpg/art/art_building.*).
//
// Everything here is header-only (inline) on purpose: rpg_sim and the headless tests (which do not link rpg_art)
// also need archForBiome, e.g. to report which style a settlement gets.
#pragma once
#include <cstdint>

namespace art {

enum class RoofShape : uint8_t { Hip, Gable, Steep, FlatParapet, Dome, Conical, Turf, Pagoda, COUNT };
enum class RoofMat : uint8_t { Thatch, Shingle, Slate, ClayTile, Turf, Adobe, Copper, COUNT };
enum class WallMat : uint8_t { Timber, Plaster, Stone, Brick, Log, Adobe, COUNT };

struct ArchStyle {
  RoofShape roof = RoofShape::Hip;
  RoofMat roofMat = RoofMat::Shingle;
  WallMat wall = WallMat::Timber;
  uint32_t roofTint = 0, wallTint = 0, trimTint = 0;   // 0 = the material's default ramp
  uint8_t pitch = 2;         // roof steepness: 0 flat .. 4 very steep
  uint8_t chimneys = 1;      // 0..2
  bool smoke = false;        // chimneys smoke (cold climates)
  bool awnings = false;      // cloth awnings over doors/windows (desert, markets)
  bool stilts = false;       // raised on stilts (swamp)
  bool shutters = true;
  bool snow = false;         // snow lies on the roofs and sills
  uint8_t weather = 0;       // 0 new .. 3 old: stains, moss on the north (back) roof plane, patched shingles
  // identity for sprite caches: equal keys must paint identical pixels
  uint64_t key() const {
    uint64_t k = 1469598103934665603ull;
    auto mx = [&](uint64_t v) { k ^= v; k *= 1099511628211ull; };
    mx((uint64_t)roof | (uint64_t)roofMat << 8 | (uint64_t)wall << 16 | (uint64_t)pitch << 24 | (uint64_t)chimneys << 32 |
       (uint64_t)smoke << 40 | (uint64_t)awnings << 41 | (uint64_t)stilts << 42 | (uint64_t)shutters << 43 | (uint64_t)snow << 44 |
       (uint64_t)weather << 48);
    mx(roofTint); mx(wallTint); mx(trimTint);
    return k;
  }
};

namespace style_detail {
inline uint32_t mixSeed(uint32_t s, uint32_t k) {
  uint32_t h = s * 2654435761u ^ (k + 0x9E3779B9u + (s << 6) + (s >> 2));
  h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
  return h;
}
inline uint32_t rgbaS(int r, int g, int b) { return (uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16 | 0xFF000000u; }
}  // namespace style_detail

// The M0 stand-in for the culture engine: a style from the biome (world.h Biome as int) and a per-building seed.
// snow: steep slate + smoking chimneys; desert: flat adobe with a parapet and awnings; swamp: steep thatch on stilts;
// autumn: timber and thatch; plains: mixed hip and gable. Neighbours differ by seed (shape, material, pitch, weathering)
// inside the biome's family so a street reads as one place without copy-pasted houses.
// Biome order (world.h): Ocean, Beach, Plains, Forest, Autumn, Taiga, Snow, Swamp, Desert, Mountain.
inline ArchStyle archForBiome(int biome, uint32_t seed) {
  using namespace style_detail;
  ArchStyle s;
  uint32_t h = mixSeed(seed, 0xA5C1u + (uint32_t)biome * 977u);
  auto pick = [&](int n) { int v = (int)(h % (uint32_t)n); h = mixSeed(h, 0x51u); return v; };
  s.weather = (uint8_t)pick(4);
  s.chimneys = (uint8_t)(pick(5) == 0 ? 2 : 1);
  switch (biome) {
    case 6: {  // Snow: steep slate (some shingle), stone or heavy timber, smoking chimneys, snow on the roofs
      s.roof = pick(3) == 0 ? RoofShape::Hip : RoofShape::Steep;
      s.roofMat = pick(4) == 0 ? RoofMat::Shingle : RoofMat::Slate;
      s.wall = pick(3) == 0 ? WallMat::Timber : (pick(2) ? WallMat::Stone : WallMat::Log);
      s.pitch = 4; s.smoke = true; s.snow = true; s.shutters = true;
      break;
    }
    case 8: {  // Desert: flat adobe roofs behind parapets, the odd dome, awnings
      s.roof = pick(5) == 0 ? RoofShape::Dome : RoofShape::FlatParapet;
      s.roofMat = s.roof == RoofShape::Dome ? (pick(3) == 0 ? RoofMat::Copper : RoofMat::Adobe) : RoofMat::Adobe;
      s.wall = WallMat::Adobe;
      static const uint32_t sand[4] = {rgbaS(222, 186, 132), rgbaS(214, 168, 120), rgbaS(230, 200, 150), rgbaS(206, 160, 116)};
      s.wallTint = sand[pick(4)];
      s.pitch = 0; s.awnings = true; s.shutters = false; s.chimneys = 0;
      break;
    }
    case 7: {  // Swamp: steep thatch on stilts, log or timber walls
      s.roof = pick(3) == 0 ? RoofShape::Hip : RoofShape::Steep;
      s.roofMat = pick(5) == 0 ? RoofMat::Turf : RoofMat::Thatch;
      s.wall = pick(2) ? WallMat::Log : WallMat::Timber;
      s.pitch = 4; s.stilts = true; s.shutters = pick(2) == 0;
      s.weather = (uint8_t)(2 + pick(2));
      break;
    }
    case 4: {  // Autumn: timber frame under thatch, some shingle; gable and hip
      s.roof = pick(2) ? RoofShape::Gable : RoofShape::Hip;
      s.roofMat = pick(3) == 0 ? RoofMat::Shingle : RoofMat::Thatch;
      s.wall = pick(4) == 0 ? WallMat::Plaster : WallMat::Timber;
      s.pitch = (uint8_t)(2 + pick(2));
      break;
    }
    case 5: {  // Taiga: log halls with turf or shingle roofs, steep gables, smoke
      s.roof = pick(3) == 0 ? RoofShape::Turf : RoofShape::Steep;
      s.roofMat = s.roof == RoofShape::Turf ? RoofMat::Turf : (pick(2) ? RoofMat::Shingle : RoofMat::Turf);
      s.wall = pick(4) == 0 ? WallMat::Timber : WallMat::Log;
      s.pitch = 3; s.smoke = true;
      break;
    }
    case 9: {  // Mountain: stone under slate, low and solid, smoke
      s.roof = pick(2) ? RoofShape::Gable : RoofShape::Hip;
      s.roofMat = pick(4) == 0 ? RoofMat::Shingle : RoofMat::Slate;
      s.wall = pick(4) == 0 ? WallMat::Brick : WallMat::Stone;
      s.pitch = 2; s.smoke = true;
      break;
    }
    case 1: {  // Beach: whitewashed plaster under clay tile
      s.roof = pick(2) ? RoofShape::Hip : RoofShape::Gable;
      s.roofMat = pick(4) == 0 ? RoofMat::Thatch : RoofMat::ClayTile;
      s.wall = pick(3) == 0 ? WallMat::Timber : WallMat::Plaster;
      s.pitch = 1;
      break;
    }
    case 3: {  // Forest: shingle and log, gables
      s.roof = pick(3) == 0 ? RoofShape::Hip : RoofShape::Gable;
      s.roofMat = pick(3) == 0 ? RoofMat::Thatch : RoofMat::Shingle;
      s.wall = pick(3) == 0 ? WallMat::Log : (pick(2) ? WallMat::Timber : WallMat::Plaster);
      s.pitch = (uint8_t)(2 + pick(2));
      break;
    }
    default: {  // Plains (and anything else): a mix of hip and gable, thatch, shingle and clay over timber, plaster, stone
      s.roof = pick(2) ? RoofShape::Gable : RoofShape::Hip;
      int m = pick(6);
      s.roofMat = m < 2 ? RoofMat::Thatch : (m < 5 ? RoofMat::Shingle : RoofMat::ClayTile);
      int w = pick(6);
      s.wall = w < 3 ? WallMat::Timber : (w < 4 ? WallMat::Plaster : (w < 5 ? WallMat::Stone : WallMat::Brick));
      s.pitch = (uint8_t)(1 + pick(3));
      break;
    }
  }
  if (s.roofMat == RoofMat::Turf && s.roof != RoofShape::Turf && s.roof != RoofShape::Steep) s.roof = RoofShape::Turf;
  return s;
}

// Bldg::roof (a world-generator tint) applies to the materials that come in colours; thatch, turf and adobe keep theirs.
inline ArchStyle withRoofTint(ArchStyle s, uint32_t tint) {
  if (tint && (s.roofMat == RoofMat::Shingle || s.roofMat == RoofMat::Slate || s.roofMat == RoofMat::ClayTile)) s.roofTint = tint;
  return s;
}

}  // namespace art
