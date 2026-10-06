// 5.7 variety within a culture (M3, CULTURE lane): one building's architecture. The family's base style (Culture::arch)
// varied per building so a street of one culture reads as one place without copy-pasted houses:
//   - 1 in 5 buildings use the culture's alternate roof form (with a material that suits it);
//   - wealth 0..3: the poor build in wattle and thatch substitutes (the culture's own: felt, palm, plank, bark...), the
//     rich in stone or ashlar with more ornament and gilding;
//   - urbanity 0 village .. 3 capital: taller walls, fired roofs and kept-up fronts in towns, weathered cottages in
//     villages;
//   - the climate it stands in (biome): snow on the roofs, smoking chimneys in the cold, none in the desert;
//   - a +-6 % palette jitter on the tints, small pitch / eave / wall-height wobble, and ArchStyle::variant: free facade
//     bits (window rhythm, porch, dormers, shutters, chimney side) the painter reads.
// Pure integer maths, deterministic in (culture, seed).
#include <cstdint>
#include <utility>
#include "rpg/culture/culture.h"

namespace cult {
namespace detail {
uint64_t smix(uint64_t z);                                  // priors.cpp
uint32_t roofColourFor(art::RoofMat m, uint32_t hue);       // priors.cpp
uint32_t wallColourFor(art::WallMat m, uint32_t hue);       // priors.cpp
}  // namespace detail

namespace {
using namespace art;
struct Pk {
  uint64_t s;
  explicit Pk(uint64_t seed) : s(seed ? seed : 0xA5ull) {}
  uint32_t next() { s = detail::smix(s); return (uint32_t)(s >> 32); }
  int pick(int n) { return n <= 1 ? 0 : (int)(next() % (uint32_t)n); }
  bool chance(int p256) { return (int)(next() & 255) < p256; }
  int range(int lo, int hi) { return hi <= lo ? lo : lo + pick(hi - lo + 1); }
};
int clamp8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
uint32_t jitter(uint32_t c, Pk& p, int pct) {
  if (!c) return 0;
  const int k = 1000 + p.range(-pct * 10, pct * 10);
  const int r = c & 255, g = (c >> 8) & 255, b = (c >> 16) & 255;
  return (uint32_t)clamp8(r * k / 1000 + p.range(-2, 2)) | (uint32_t)clamp8(g * k / 1000 + p.range(-2, 2)) << 8 |
         (uint32_t)clamp8(b * k / 1000 + p.range(-2, 2)) << 16 | 0xFF000000u;
}

// a roof material that suits the roof form (keeping the culture's own where it fits)
RoofMat matFor(RoofShape r, RoofMat m, Archetype a) {
  switch (r) {
    case RoofShape::Turf: return RoofMat::Turf;
    case RoofShape::Dome:
      if (m == RoofMat::Adobe || m == RoofMat::Copper || m == RoofMat::GlazedTile || m == RoofMat::Felt || m == RoofMat::Slate) return m;
      return a == Archetype::Steppe ? RoofMat::Felt : a == Archetype::Imperial ? RoofMat::Copper : RoofMat::Adobe;
    case RoofShape::Onion:
      if (m == RoofMat::Copper || m == RoofMat::GlazedTile || m == RoofMat::Slate) return m;
      return RoofMat::Copper;
    case RoofShape::FlatParapet:
    case RoofShape::Stepped:
      return m == RoofMat::ClayTile && r == RoofShape::FlatParapet ? m : RoofMat::Adobe;
    case RoofShape::Pagoda:
      if (m == RoofMat::GlazedTile || m == RoofMat::ClayTile || m == RoofMat::Slate) return m;
      return RoofMat::ClayTile;
    case RoofShape::Mansard:
      if (m == RoofMat::Slate || m == RoofMat::ClayTile || m == RoofMat::Shingle || m == RoofMat::Copper) return m;
      return RoofMat::Slate;
    case RoofShape::Sweep:
      if (m == RoofMat::Leaf || m == RoofMat::Bark || m == RoofMat::Shingle || m == RoofMat::Thatch) return m;
      return RoofMat::Shingle;
    case RoofShape::Conical:
      if (m == RoofMat::Adobe) return RoofMat::Thatch;
      if (m == RoofMat::Turf) return RoofMat::Thatch;
      return m;
    default:   // Hip, Gable, Steep
      if (m == RoofMat::Adobe || m == RoofMat::Felt) return a == Archetype::Dune || a == Archetype::SunTemple ? RoofMat::Palm : RoofMat::Thatch;
      if (m == RoofMat::Turf && r == RoofShape::Hip) return RoofMat::Shingle;
      return m;
  }
}

struct Sub { WallMat wall; RoofMat roof; uint16_t addOrn; };
// the poor: what a family without means builds from in each culture
const Sub kPoor[(int)Archetype::COUNT] = {
    {WallMat::Plank, RoofMat::Turf, 0},                 // Fjordfolk
    {WallMat::Wattle, RoofMat::Thatch, 0},              // Highland
    {WallMat::Wattle, RoofMat::Thatch, 0},              // Heartland
    {WallMat::Plaster, RoofMat::ClayTile, 0},           // Imperial
    {WallMat::Adobe, RoofMat::Palm, 0},                 // Dune
    {WallMat::Felt, RoofMat::Felt, 0},                  // Steppe
    {WallMat::Wattle, RoofMat::Thatch, 0},              // Marsh
    {WallMat::Timber, RoofMat::Thatch, 0},              // Jade
    {WallMat::Timber, RoofMat::ClayTile, 0},            // River
    {WallMat::Wattle, RoofMat::Palm, 0},                // SunTemple
    {WallMat::Plank, RoofMat::Bark, 0},                 // Sylvan
    {WallMat::Plaster, RoofMat::Slate, 0},              // Starspire
};
// the rich: dressed stone and the culture's grandest ornament
const Sub kRich[(int)Archetype::COUNT] = {
    {WallMat::Log, RoofMat::Shingle, ORN_DRAGON_HEADS | ORN_CARVED_RIDGE | ORN_PAINTED_BANDS},
    {WallMat::Stone, RoofMat::Slate, ORN_CHIMNEY | ORN_CRENELS},
    {WallMat::Stone, RoofMat::Slate, ORN_PORCH_COLUMNS | ORN_FINIALS},
    {WallMat::Ashlar, RoofMat::ClayTile, ORN_GILDING | ORN_FINIALS | ORN_PORCH_COLUMNS},
    {WallMat::Plaster, RoofMat::GlazedTile, ORN_GILDING | ORN_PAINTED_BANDS},
    {WallMat::Felt, RoofMat::Felt, ORN_GILDING | ORN_FINIALS | ORN_PAINTED_BANDS},
    {WallMat::Plank, RoofMat::Thatch, ORN_CARVED_RIDGE | ORN_LANTERNS},
    {WallMat::Brick, RoofMat::GlazedTile, ORN_GILDING | ORN_DRAGON_HEADS | ORN_PORCH_COLUMNS},
    {WallMat::Brick, RoofMat::Slate, ORN_FINIALS | ORN_PAINTED_BANDS},
    {WallMat::Ashlar, RoofMat::Adobe, ORN_GILDING | ORN_PAINTED_BANDS},
    {WallMat::Living, RoofMat::Leaf, ORN_FINIALS | ORN_LANTERNS},
    {WallMat::Ashlar, RoofMat::Copper, ORN_GILDING | ORN_FINIALS},
};
constexpr uint16_t kFancy = ORN_GILDING | ORN_FINIALS | ORN_PORCH_COLUMNS | ORN_PAINTED_BANDS | ORN_DRAGON_HEADS | ORN_CRENELS;
}  // namespace

art::ArchStyle buildingArch(const Culture& c, int biome, int urban, int wealth, uint32_t seed, uint32_t roofTint) {
  (void)roofTint;   // the culture's palette (and its per-building jitter) replaces the M2 generator's roof tints
  ArchStyle s = c.arch;
  if (!s.culture) s.culture = (uint8_t)((int)c.archetype + 1);
  Pk p(detail::smix(((uint64_t)c.seed << 32) ^ seed ^ 0xB11D1A6ull));
  const Archetype a = c.archetype;
  const int ai = (int)a < (int)Archetype::COUNT ? (int)a : 2;
  urban = urban < 0 ? 0 : urban > 3 ? 3 : urban;
  wealth = wealth < 0 ? 0 : wealth > 3 ? 3 : wealth;
  const uint32_t roofHue = c.arch.roofTint, wallHue = c.arch.wallTint;

  // 1 in 5: the alternate roof form
  if (p.pick(5) == 0 && c.altRoof != s.roof) s.roof = c.altRoof;

  // wealth: villages lean poor, capitals lean rich (a building's own wealth still decides)
  int w = wealth;
  if (urban == 0 && w > 0 && p.chance(70)) w--;
  if (urban >= 3 && w < 3 && p.chance(90)) w++;
  if (w == 0) {
    const Sub& S = kPoor[ai];
    if (p.chance(200)) s.wall = S.wall;
    if (s.roof != RoofShape::FlatParapet && s.roof != RoofShape::Stepped && s.roof != RoofShape::Dome && p.chance(190)) s.roofMat = S.roof;
    s.ornament = (uint16_t)(s.ornament & ~kFancy);
    s.weather = (uint8_t)(s.weather + 1 > 3 ? 3 : s.weather + 1);
    if (s.roof == RoofShape::Onion || s.roof == RoofShape::Mansard) s.roof = c.altRoof == RoofShape::Onion || c.altRoof == RoofShape::Mansard ? RoofShape::Gable : c.altRoof;
    if (a == Archetype::SunTemple && s.roof == RoofShape::Stepped && p.chance(170)) { s.roof = RoofShape::Steep; s.roofMat = RoofMat::Palm; }
    if (s.roofMat == RoofMat::Turf && s.roof != RoofShape::Turf && s.roof != RoofShape::Steep) s.roof = RoofShape::Turf;
  } else if (w >= 3) {
    const Sub& S = kRich[ai];
    if (p.chance(170)) s.wall = S.wall;
    if (p.chance(150)) s.roofMat = S.roof;
    s.ornament = (uint16_t)(s.ornament | S.addOrn);
    s.weather = 0;
  } else if (w == 2 && p.chance(60)) {
    s.ornament = (uint16_t)(s.ornament | (kRich[ai].addOrn & (uint16_t)p.next()));
  }
  // towns: fired roofs over the commonest thatch in the crowded lowland cultures (fire risk), kept-up fronts
  if (urban >= 2 && s.roofMat == RoofMat::Thatch && (a == Archetype::Heartland || a == Archetype::Highland || a == Archetype::River) &&
      p.chance(160))
    s.roofMat = a == Archetype::Highland ? RoofMat::Slate : RoofMat::Shingle;
  if (urban >= 2 && s.weather > 1) s.weather = 1;
  s.roofMat = matFor(s.roof, s.roofMat, a);
  if (s.roof == RoofShape::Turf && s.wall == WallMat::Ashlar) s.wall = WallMat::Stone;

  // proportions: wall height grows with town and wealth; pitch and eave wobble a little
  {
    int wh = s.wallH ? s.wallH : 102;
    wh += (urban - 1) * 6 + (w - 1) * 5 + p.range(-8, 8);
    s.wallH = (uint8_t)(wh < 1 ? 1 : wh > 255 ? 255 : wh);
  }
  if (s.roof != RoofShape::FlatParapet && s.roof != RoofShape::Dome && s.roof != RoofShape::Stepped && p.chance(80)) {
    const int pt = (int)s.pitch + (p.chance(128) ? 1 : -1);
    s.pitch = (uint8_t)(pt < 1 ? 1 : pt > 4 ? 4 : pt);
  }
  if (s.roof == RoofShape::Steep && s.pitch < 3) s.pitch = 3;
  if (p.chance(60)) s.eave = (uint8_t)(s.eave >= 3 ? 2 : s.eave + 1);

  // climate
  const bool cold = biome == 5 || biome == 6 || biome == 9;
  s.snow = biome == 6;
  if (biome == 8) { s.snow = false; s.smoke = false; }
  if (cold && (s.chimneys > 0 || (s.ornament & ORN_CHIMNEY))) s.smoke = true;
  if (cold && s.chimneys == 0 && a != Archetype::Dune && a != Archetype::SunTemple && p.chance(160)) { s.chimneys = 1; s.smoke = true; }
  if (biome == 7 && a != Archetype::Dune && a != Archetype::Steppe && p.chance(140)) { s.stilts = true; s.foundation = Foundation::Stilts; }
  if (s.foundation == Foundation::Stilts) s.stilts = true;
  if (urban >= 2 && s.stilts && a != Archetype::Marsh) { s.stilts = false; s.foundation = Foundation::Plinth; }

  // colour: the roof and wall follow their (maybe new) material, then a +-6 % jitter on every tint
  if (s.roofMat != c.arch.roofMat) s.roofTint = detail::roofColourFor(s.roofMat, roofHue);
  if (s.wall != c.arch.wall) s.wallTint = detail::wallColourFor(s.wall, wallHue);
  s.roofTint = jitter(s.roofTint, p, 6);
  s.wallTint = jitter(s.wallTint, p, 6);
  s.trimTint = jitter(s.trimTint, p, 6);
  s.accentTint = jitter(s.accentTint, p, 6);
  s.altTint = jitter(s.altTint, p, 6);
  // one door / band colour in four is the second accent: a street of one culture is not one paint pot
  if (p.pick(4) == 0 && s.altTint) std::swap(s.accentTint, s.altTint);

  // shutters and chimneys as facade variety; rich houses sometimes get the grander window of their culture
  if (s.shutters && p.chance(50)) s.shutters = false;
  if (s.chimneys == 1 && w >= 2 && p.chance(50) && (s.ornament & ORN_CHIMNEY)) s.chimneys = 2;
  if (w >= 3 && s.window == WindowShape::Square && p.chance(120)) s.window = a == Archetype::Heartland ? WindowShape::Lattice : WindowShape::Arched;
  s.furniture = c.customs.furniture;
  s.variant = (uint8_t)(p.next() >> 24);
  // (M3 fixer, owner carry-over 4) a street of one people is not a row of copies: the render or limewash of each house
  // leans to its own shade within the culture's family (an ochre, a rose, a cool grey, or the culture's own), the roof
  // tiles a little darker or lighter, and one pitched house in three turns its roof (hip <-> gable). Drawn after every
  // other choice, so the culture's look and each building's other draws stay as they were.
  {
    auto lerp = [](uint32_t a, uint32_t b, int t) {   // t / 100 of the way from a to b
      const int ar = a & 255, ag = (a >> 8) & 255, ab = (a >> 16) & 255, br = b & 255, bg = (b >> 8) & 255, bb = (b >> 16) & 255;
      return (uint32_t)clamp8(ar + (br - ar) * t / 100) | (uint32_t)clamp8(ag + (bg - ag) * t / 100) << 8 |
             (uint32_t)clamp8(ab + (bb - ab) * t / 100) << 16 | 0xFF000000u;
    };
    const int shade = p.pick(4);
    const bool rendered = s.wall == WallMat::Plaster || s.wall == WallMat::Adobe || s.wall == WallMat::Timber;
    if (rendered && s.wallTint) {
      static const uint32_t hues[3] = {0xFF6EAAD6u, 0xFF8C96CEu, 0xFFB0ACAAu};   // ochre, rose, cool grey (0xAABBGGRR)
      if (shade > 0) s.wallTint = lerp(s.wallTint, hues[shade - 1], 22);
    }
    const int rt = p.pick(3);
    if (s.roofTint && rt) s.roofTint = lerp(s.roofTint, rt == 1 ? 0xFF2A2A2Au : 0xFFE0E0E0u, 12);
    if (p.pick(3) == 0 && w > 0) {
      if (s.roof == RoofShape::Hip) s.roof = RoofShape::Gable;
      else if (s.roof == RoofShape::Gable) s.roof = RoofShape::Hip;
    }
  }
  // (M3 fixer round 3, review: "Jade thatch roofs are ragged blobs with no ridge") the jade peoples' straw roofs are
  // steep gables (the painter caps their ridge with tile), never a low hip that reads as a haystack
  if (a == Archetype::Jade && (s.roofMat == RoofMat::Thatch || s.roofMat == RoofMat::Palm) && s.roof != RoofShape::Gable) {
    s.roof = RoofShape::Gable;
    if (s.pitch < 3) s.pitch = 3;
  }
  return s;
}

}  // namespace cult
