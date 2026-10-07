// Architecture styles (VISION_PLAN 5.5): the parameters the building painter takes, so roofs, walls and details
// can follow biome now (M0) and culture later (M3) without reworking the art API.
// M0: owned by the architecture lane (style.h, style.cpp, rpg/art/art_building.*).
//
// Everything here is header-only (inline) on purpose: rpg_sim and the headless tests (which do not link rpg_art)
// also need archForBiome, e.g. to report which style a settlement gets.
#pragma once
#include <cstdint>

namespace art {

// M3 (culture engine) appends: roofs Mansard, Onion (bulb domes), Stepped (sun-temple terraces), Sweep (elven: a long
// curved ridge with upswept ends); materials Palm (fronds), Bark, Felt (yurts), GlazedTile (jade/blue glaze), Leaf
// (elven living thatch); walls Rubble, Plank, Wattle, Felt (yurt lattice under felt), Ashlar (dressed, pale, fine
// joints; Stone stays today's rough grey), Living (elven: grown timber, root buttresses).
// Never saved (Bldg::arch is regenerated with the world): append anyway.
// M3b (BUILDER lane) appends Tent (a tent of canvas or felt: a cone on a round body, a sagging ridge on a box, both with
// a scalloped valance) and Spire (a needle: a steep pyramid on a box, a steep cone on a round body; Volume::pitch 5..8).
enum class RoofShape : uint8_t { Hip, Gable, Steep, FlatParapet, Dome, Conical, Turf, Pagoda, Mansard, Onion, Stepped, Sweep, Tent, Spire, COUNT };
enum class RoofMat : uint8_t { Thatch, Shingle, Slate, ClayTile, Turf, Adobe, Copper, Palm, Bark, Felt, GlazedTile, Leaf, COUNT };
enum class WallMat : uint8_t { Timber, Plaster, Stone, Brick, Log, Adobe, Rubble, Plank, Wattle, Felt, Ashlar, Living, COUNT };
// M3: window and door shapes, foundations (ArchStyle::window / door / foundation). Screen: a carved grille (dune
// kingdoms); Flap: a hide door (yurts); Moon: a round moon gate (jade terraces); Platform: a raised stone dais.
enum class WindowShape : uint8_t { Square, Arched, Slit, Round, Lattice, Pointed, Tall, Screen, COUNT };
enum class DoorShape : uint8_t { Plank, Arched, Curtain, Double, Round, Flap, Moon, COUNT };
enum class Foundation : uint8_t { None, Plinth, Stilts, Terrace, Platform, COUNT };
// M3: ornament bits (ArchStyle::ornament); the painter adds what fits the roof and the building type
enum : uint16_t {
  ORN_CHIMNEY = 1, ORN_FINIALS = 2, ORN_CARVED_RIDGE = 4, ORN_AWNINGS = 8, ORN_CRENELS = 16, ORN_SHUTTERS = 32,
  ORN_FLOWERBOX = 64, ORN_WINDCATCHER = 128, ORN_PRAYER_FLAGS = 256, ORN_PAINTED_BANDS = 512, ORN_ROOF_STONES = 1024,
  ORN_PORCH_COLUMNS = 2048, ORN_DRAGON_HEADS = 4096, ORN_LANTERNS = 8192, ORN_VINES = 16384, ORN_GILDING = 32768
};
// M3: interior furnishing (ArchStyle::furniture): what people sit and sleep on
enum class Furniture : uint8_t { Chairs, Benches, Cushions, Hammocks, Stools, COUNT };
// M3: a culture's town wall and fences. A settlement writes Map::wall = 1 + CityWall on its wall tiles; the view puts
// the style into the wallTile key at art::WALL_STYLE_SHIFT. WhiteStone: elven.
// Talud (M3 fixer round 3): the sun temples' lime-plastered walls, their gates between square stepped pylons under a
// corbelled arch, never the drum-towered portcullis gate of the north.
enum class CityWall : uint8_t { Stone, Palisade, Rampart, Thorn, Adobe, WhiteStone, Jade, Talud, COUNT };   // (Jade: M3 fixer round 2)
enum class Fence : uint8_t { Wattle, Picket, StoneDyke, Bamboo, Rope, Hedge, COUNT };

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
  // ---- M3 culture engine (VISION_PLAN 5.2, 5.5). Every default reproduces the M2 look exactly, so a style made by
  //      archForBiome alone paints as before. The culture engine (cult::buildingArch) sets them.
  WindowShape window = WindowShape::Square;
  DoorShape door = DoorShape::Plank;
  Foundation foundation = Foundation::None;   // (`stilts` above stays the M0 switch; Foundation::Stilts means the same)
  uint16_t ornament = 0;     // ORN_* bits
  uint8_t eave = 0;          // extra eave overhang px 0..3 (0: today's)
  uint8_t wallH = 0;         // wall height: 0 today's; else 1..255 -> 0.8x .. 1.3x
  uint8_t culture = 0;       // cult::Archetype + 1 (0: none, the biome stand-in): culture-specific details no other
                             // field names (an elven root buttress, a yurt's crown ring, a sun-temple's stepped crest)
  uint8_t variant = 0;       // free per-building variety bits for the painter (window rhythm, door placement, porch,
                             // dormers): the generator draws them so a street of one culture never repeats a facade
  Furniture furniture = Furniture::Chairs;   // interiors (genInterior reads it from Bldg::arch)
  uint32_t accentTint = 0;   // 0 = none: painted bands, doors, shutters, awnings, flags
  uint32_t altTint = 0;      // 0 = none: a second accent (a glaze stripe, carved trim)
  // identity for sprite caches: equal keys must paint identical pixels
  uint64_t key() const {
    uint64_t k = 1469598103934665603ull;
    auto mx = [&](uint64_t v) { k ^= v; k *= 1099511628211ull; };
    mx((uint64_t)roof | (uint64_t)roofMat << 8 | (uint64_t)wall << 16 | (uint64_t)pitch << 24 | (uint64_t)chimneys << 32 |
       (uint64_t)smoke << 40 | (uint64_t)awnings << 41 | (uint64_t)stilts << 42 | (uint64_t)shutters << 43 | (uint64_t)snow << 44 |
       (uint64_t)weather << 48);
    mx(roofTint); mx(wallTint); mx(trimTint);
    mx((uint64_t)window | (uint64_t)door << 8 | (uint64_t)foundation << 16 | (uint64_t)ornament << 24 | (uint64_t)eave << 40 |
       (uint64_t)wallH << 48 | (uint64_t)culture << 56);
    mx((uint64_t)furniture | (uint64_t)variant << 8 | (uint64_t)accentTint << 16);
    mx(altTint);
    return k;
  }
};

// M3: the culture's look for the small things of a settlement (VISION_PLAN 5.5 "style-variant props"): fences, wells,
// lamps, benches, the centrepiece, market awnings (the market LAYOUT stays as the owner approved it; only its palette
// and awning style follow the culture), banners and shrines. The view picks the style of the site a prop stands in
// (World::cultureOf(site)->props); wild props outside settlements use the default. Default = today's look.
struct PropStyle {
  uint8_t culture = 0;       // cult::Archetype + 1 (0: the classic look)
  Fence fence = Fence::Picket;
  uint8_t well = 0;          // 0 stone well with a roof (today), 1 open stone ring, 2 sweep (shadoof), 3 carved spring
                             // basin, 4 tiled cistern
  uint8_t lamp = 0;          // 0 iron lamppost (today), 1 paper lantern on a pole, 2 brazier, 3 stone lantern,
                             // 4 glow-orb (elven), 5 torch on a post
  uint8_t bench = 0;         // 0 plank bench (today), 1 stone bench, 2 cushions on a rug, 3 log seat
  uint8_t centre = 0;        // centrepiece: 0 fountain (today), 1 statue, 2 well, 3 sacred tree, 4 fire bowl, 5 obelisk,
                             // 6 standing stones
  uint8_t awning = 0;        // market awnings: 0 striped canvas (today), 1 plain dyed cloth, 2 reed mat, 3 tiled lean-to,
                             // 4 silk with tassels, 5 hide
  uint32_t awningA = 0, awningB = 0;                    // awning colours (0: the trade's own, as today)
  uint32_t wood = 0, stone = 0, metal = 0, cloth = 0;   // material tints (0: today's ramps)
  bool classic() const { return culture == 0; }
  uint64_t key() const {   // 0 for the classic look, so caches keyed on (prop, style key) keep their pre-M3 entries
    if (classic()) return 0;
    uint64_t k = 0xCBF29CE484222325ull;
    auto mx = [&](uint64_t v) { k ^= v; k *= 0x100000001B3ull; };
    mx((uint64_t)culture | (uint64_t)fence << 8 | (uint64_t)well << 16 | (uint64_t)lamp << 24 | (uint64_t)bench << 32 |
       (uint64_t)centre << 40 | (uint64_t)awning << 48);
    mx((uint64_t)awningA << 32 | awningB);
    mx((uint64_t)wood << 32 | stone);
    mx((uint64_t)metal << 32 | cloth);
    return k | 1;
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

// (M1) Town and city building: the biome's family, made urban. urban: 0 the countryside (unchanged), 1 a town (no
// stilts or turf roofs; log walls mostly give way to timber framing), 2 a city, 3 a capital (stone, brick and
// plaster under slate, tile or shingle; no thatch, turf, logs or stilts: town houses that read as a royal seat).
// Desert adobe and the snow family's slate stay as they are (they are already masonry).
inline ArchStyle urbanize(ArchStyle s, int urban, uint32_t seed) {
  using namespace style_detail;
  if (urban <= 0) return s;
  uint32_t h = mixSeed(seed, 0x0B4Au + (uint32_t)urban * 131u);
  auto pick = [&](int n) { int v = (int)(h % (uint32_t)n); h = mixSeed(h, 0x77u); return v; };
  s.stilts = false;
  if (s.roof == RoofShape::Turf) s.roof = pick(2) ? RoofShape::Gable : RoofShape::Steep;
  if (s.roofMat == RoofMat::Turf) s.roofMat = RoofMat::Shingle;
  if (s.wall == WallMat::Adobe) return s;
  if (urban == 1) {
    if (s.wall == WallMat::Log && pick(3) != 0) s.wall = pick(2) ? WallMat::Timber : WallMat::Plaster;
    if (s.roofMat == RoofMat::Thatch && pick(2) == 0) s.roofMat = RoofMat::Shingle;
    return s;
  }
  // cities and capitals: masonry and fired roofs; a capital's core is grander still
  static const WallMat walls[6] = {WallMat::Stone, WallMat::Plaster, WallMat::Brick, WallMat::Stone, WallMat::Timber, WallMat::Plaster};
  if (s.wall == WallMat::Log || s.wall == WallMat::Timber || urban >= 3) s.wall = walls[pick(urban >= 3 ? 4 : 6)];
  if (s.roofMat == RoofMat::Thatch) { const int r = pick(3); s.roofMat = r == 0 ? RoofMat::Slate : r == 1 ? RoofMat::ClayTile : RoofMat::Shingle; }
  if (urban >= 3 && s.roofMat == RoofMat::Shingle && pick(2) == 0) s.roofMat = RoofMat::Slate;
  // (M2 fixer round 3, review: "rows of nearly identical gable-front houses in capital streets") a city street mixes
  // its roofs: more eave-fronted hips than gables, the odd steep gable, so a row of same-width houses rarely shows the
  // same gable end three times running
  if (s.roof == RoofShape::Gable || s.roof == RoofShape::Hip) {
    const int r = pick(20);
    s.roof = r < 10 ? RoofShape::Hip : (r < 17 ? RoofShape::Gable : RoofShape::Steep);
    if (s.roof == RoofShape::Steep && s.pitch < 3) s.pitch = 3;
  }
  if (s.pitch < 2) s.pitch = 2;
  s.weather = (uint8_t)(s.weather > 1 ? 1 : s.weather);   // kept up
  s.shutters = true;
  return s;
}

// Bldg::roof (a world-generator tint) applies to the materials that come in colours; thatch, turf and adobe keep theirs.
inline ArchStyle withRoofTint(ArchStyle s, uint32_t tint) {
  if (tint && (s.roofMat == RoofMat::Shingle || s.roofMat == RoofMat::Slate || s.roofMat == RoofMat::ClayTile)) s.roofTint = tint;
  return s;
}

}  // namespace art
