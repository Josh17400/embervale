// The culture engine (rpg/culture/culture.h; VISION_PLAN 5.1-5.4, owner 15.6 / 15.11; M3, CULTURE lane): the atlas.
//
//   - A FAMILY per culture cell (ew::CCELL tiles). Its archetype is weighted by the homeland climate (the callback);
//     its every field is drawn inside the archetype's priors (priors.cpp). Rare ISOLATED families cross two archetypes.
//   - Distinctness: the four-phase checkerboard maximin (5.4). Cell (i, j) has phase (i & 1) + 2 (j & 1); all 8
//     neighbours of a cell have other phases, so every adjacent pair is checked by the later phase. Phase 0 takes
//     candidate 0; phase k draws K = 12 candidates and keeps the best against its lower-phase neighbours (memoised,
//     depth <= 3), so the answer is IDENTICAL whatever order the cells are asked in. When no candidate of the first
//     12 meets the thresholds (d >= 0.45 and >= 3 headline differences) up to 12 more are drawn; still deterministic.
//   - A DIALECT per kingdom: the family of the cell holding its seat, mutated by the kingdom's seed: +-15 % on the
//     continuous dials, the accent palette, a roof ornament, the place-suffix set, the music's motifs, and TWO
//     discrete swaps chosen from 10 descriptors by a 3 x 3 tiling of the kingdom lattice (any two kingdoms within one
//     cell of each other have different descriptors, so two dialects of one family that border each other always
//     differ in at least two components: d >= 0.10).
//   - HERALDRY by the same maximin on the kingdom lattice: neighbouring kingdoms differ in >= 2 of {tinctures,
//     division, charge}.
// Structural maths is integer / Q16 only (distance.cpp); this library builds with precise floats and no libm.
#include "rpg/culture/culture.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include "rpg/world/biomes.h"
#include "rpg/world/coords.h"
#include "rpg/world/ids.h"

namespace cult {

namespace detail {
uint64_t smix(uint64_t z);
void applyPriors(Culture& c, Archetype a, uint32_t seed, int favouredOre, int cross);
void finishCulture(Culture& c);
uint32_t roofColourFor(art::RoofMat m, uint32_t hue);
uint32_t wallColourFor(art::WallMat m, uint32_t hue);
struct NameHist {
  std::vector<std::pair<uint16_t, uint32_t>> bins;
  uint32_t total = 0;
};
NameHist nameHist(const Culture& c);
struct PalKey { int32_t L[5], a[5], b[5]; };
PalKey palKey(const Culture& c);
void compareCheap(const Culture& a, const PalKey& ka, const Culture& b, const PalKey& kb, int32_t& dOut, int& headOut);
void compareFull(const Culture& a, const PalKey& ka, const NameHist& ha, const Culture& b, const PalKey& kb, const NameHist& hb,
                 int32_t& dOut, int& headOut);
}  // namespace detail

namespace {
uint32_t h32(uint64_t v) { return (uint32_t)(ew::mix64(v) >> 32); }
uint32_t rgbaC(int r, int g, int b) { return (uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16 | 0xFF000000u; }
int posmod(int a, int m) { const int r = a % m; return r < 0 ? r + m : r; }

struct Picker {
  uint64_t s;
  explicit Picker(uint64_t seed) : s(seed ? seed : 1) {}
  uint32_t next() { s = ew::mix64(s + 0x9E3779B97F4A7C15ull); return (uint32_t)(s >> 32); }
  int pick(int n) { return n <= 0 ? 0 : (int)(next() % (uint32_t)n); }
  bool chance(int p256) { return (int)(next() & 255) < p256; }
  int range(int lo, int hi) { return hi <= lo ? lo : lo + pick(hi - lo + 1); }
};

const char* const kArchName[(int)Archetype::COUNT] = {"FJORDFOLK", "HIGHLAND", "HEARTLAND", "IMPERIAL", "DUNE", "STEPPE", "MARSH",
                                                      "JADE", "RIVER", "SUN-TEMPLE", "SYLVAN", "STARSPIRE"};

// ------------------------------------------------------------------ climate affinity (5.3)
// fit 0..255 of each archetype per biome (Ocean, Beach, Plains, Forest, Autumn, Taiga, Snow, Swamp, Desert, Mountain)
const uint8_t kFit[(int)Archetype::COUNT][10] = {
    {120, 150, 30, 60, 20, 200, 230, 0, 0, 80},     // Fjordfolk: cold coasts
    {20, 40, 60, 120, 120, 120, 100, 10, 0, 230},   // Highland: cool hills
    {40, 60, 230, 160, 200, 40, 10, 40, 10, 60},    // Heartland: temperate
    {80, 160, 180, 60, 80, 0, 0, 20, 100, 60},      // Imperial: warm temperate
    {10, 60, 40, 0, 0, 0, 0, 0, 250, 50},           // Dune: hot and dry
    {0, 10, 180, 20, 40, 60, 30, 0, 160, 60},       // Steppe: dry grass
    {60, 80, 20, 60, 40, 20, 0, 250, 0, 0},         // Marsh: wet and warm
    {30, 40, 60, 120, 120, 20, 10, 100, 0, 180},    // Jade: warm hills
    {160, 120, 150, 80, 120, 30, 0, 80, 20, 0},     // River: temperate river lands
    {40, 80, 40, 120, 20, 0, 0, 160, 80, 40},       // Sun-temple: hot and wet
    {0, 0, 30, 240, 200, 120, 30, 60, 0, 40},       // Sylvan: deep forest
    {20, 40, 20, 40, 40, 80, 160, 0, 20, 220},      // Starspire: high and cold
};
int fitOf(Archetype a, const ClimateSample& cs) {
  const int b = cs.biome >= 0 && cs.biome < 10 ? cs.biome : 2;
  int f = kFit[(int)a][b];
  // temperature leans (temp Q16, 0 cold .. 65536 hot)
  const int t = (int)((cs.temp & 0xFFFFFF) >> 8);   // 0..256
  switch (a) {
    case Archetype::Dune: case Archetype::SunTemple: case Archetype::Marsh: case Archetype::Jade: f += (t - 128) / 3; break;
    case Archetype::Fjordfolk: case Archetype::Starspire: case Archetype::Highland: f += (128 - t) / 3; break;
    default: break;
  }
  if (cs.coast && (a == Archetype::Fjordfolk || a == Archetype::River || a == Archetype::Imperial)) f += 40;
  // (M3c) the biome proper at the cell's centre (culture_map.cpp packs eco + 1 into moist's top byte): each people
  // leans to its own lands
  const int ecoV = (int)(((uint32_t)cs.moist >> 24) & 255) - 1;
  if (ecoV >= 0 && ecoV < (int)Eco::COUNT) {
    const Eco e = (Eco)ecoV;
    auto in = [&](std::initializer_list<Eco> l) { for (Eco q : l) if (q == e) return true; return false; };
    switch (a) {
      case Archetype::Dune: if (in({Eco::Dunes, Eco::StonyDesert, Eco::SaltFlats, Eco::Oasis, Eco::Scrubland, Eco::Badlands})) f += 60; break;
      case Archetype::Steppe: if (in({Eco::Steppe, Eco::Savanna, Eco::Prairie})) f += 60; break;
      case Archetype::Marsh: if (in({Eco::ReedMarsh, Eco::PeatBog, Eco::Mangrove, Eco::FloodedForest, Eco::LakeDistrict})) f += 60; break;
      case Archetype::Fjordfolk: if (cs.coast && in({Eco::Shingle, Eco::SeaCliffs, Eco::Tundra, Eco::Taiga, Eco::SnowField, Eco::FrozenLakes})) f += 50; break;
      case Archetype::Sylvan:
        if (in({Eco::Silverwood, Eco::GiantForest})) f += 80;
        else if (in({Eco::MixedForest, Eco::BirchWood, Eco::AutumnWood})) f += 25;
        break;
      case Archetype::Jade: if (in({Eco::BambooForest, Eco::BlossomGrove})) f += 70; break;
      case Archetype::SunTemple: if (in({Eco::Jungle, Eco::Mangrove})) f += 50; break;
      case Archetype::Highland: if (in({Eco::Heath, Eco::AlpineMeadow, Eco::ChalkDowns})) f += 40; break;
      case Archetype::Starspire: if (in({Eco::AlpineMeadow, Eco::Glacier, Eco::CrystalBarrens})) f += 40; break;
      case Archetype::Heartland: if (in({Eco::Meadow, Eco::FlowerMeadow})) f += 30; break;
      case Archetype::River: if (in({Eco::LakeDistrict, Eco::Meadow})) f += 25; break;
      default: break;
    }
  }
  return f < 0 ? 0 : f > 255 ? 255 : f;
}
// candidate k's archetype: the first 8 weighted by fit^2 (climate-true), the last 4 by fit + 40 (any people can settle
// anywhere at a pinch: the maximin may need one)
Archetype drawArchetype(const ClimateSample& cs, uint32_t h, int k) {
  int64_t w[(int)Archetype::COUNT], sum = 0;
  for (int a = 0; a < (int)Archetype::COUNT; a++) {
    const int f = fitOf((Archetype)a, cs);
    w[a] = k < 8 ? (int64_t)f * f : (int64_t)f + 40;
    sum += w[a];
  }
  if (sum <= 0) return Archetype::Heartland;
  int64_t r = (int64_t)(ew::mix64(h) % (uint64_t)sum);
  for (int a = 0; a < (int)Archetype::COUNT; a++) {
    if (r < w[a]) return (Archetype)a;
    r -= w[a];
  }
  return Archetype::Heartland;
}

// ------------------------------------------------------------------ heraldry
const uint32_t kTinct[8] = {
    rgbaC(212, 172, 52),    // or
    rgbaC(232, 232, 224),   // argent
    rgbaC(168, 36, 40),     // gules
    rgbaC(40, 72, 150),     // azure
    rgbaC(32, 110, 60),     // vert
    rgbaC(110, 48, 120),    // purpure
    rgbaC(40, 36, 40),      // sable
    rgbaC(184, 96, 40),     // tenne
};
bool isMetal(int t) { return t < 2; }
Heraldry heraldryCandidate(uint64_t h) {
  Picker p(h);
  Heraldry H;
  // the rule of tincture: a colour on a metal or a metal on a colour
  const bool metalField = p.chance(80);
  const int f = metalField ? p.pick(2) : 2 + p.pick(6);
  int f2 = metalField ? 2 + p.pick(6) : p.pick(2);
  if (p.chance(90)) { f2 = metalField ? p.pick(2) : 2 + p.pick(6); if (f2 == f) f2 = metalField ? 1 - f : 2 + (f - 2 + 1 + p.pick(5)) % 6; }
  const int ch = metalField ? 2 + p.pick(6) : p.pick(2);
  static const uint8_t divW[8] = {3, 2, 2, 2, 1, 2, 1, 1};
  int r = p.pick(14), div = 0;
  while (div < 7 && r >= divW[div]) { r -= divW[div]; div++; }
  H.field = kTinct[f];
  H.division = (uint8_t)div;
  H.field2 = div ? kTinct[f2] : 0;
  H.charge = kTinct[ch == f ? (isMetal(ch) ? 1 - ch : 2 + (ch - 1) % 6) : ch];
  H.chargeKind = 0;
  H.emblem = (uint8_t)p.pick(8);
  H.glyphSeed = p.next() | 1u;
  H.shape = 0;
  return H;
}
int heraldryDiffs(const Heraldry& a, const Heraldry& b) {
  const bool tinct = a.field != b.field || a.field2 != b.field2 || a.charge != b.charge;
  const bool divi = a.division != b.division;
  const bool chg = a.chargeKind != b.chargeKind || a.emblem != b.emblem || (a.chargeKind == 1 && a.glyphSeed != b.glyphSeed);
  return (int)tinct + (int)divi + (int)chg;
}

// ------------------------------------------------------------------ dialect swaps
// the 10 descriptors: unordered pairs of {wall, layout, roof material, clothing cut, helm}
const uint8_t kSwapPairs[10][2] = {{0, 1}, {0, 2}, {0, 3}, {0, 4}, {1, 2}, {1, 3}, {1, 4}, {2, 3}, {2, 4}, {3, 4}};
const art::WallMat kAltWall[(int)Archetype::COUNT][3] = {
    {art::WallMat::Plank, art::WallMat::Log, art::WallMat::Timber},       {art::WallMat::Stone, art::WallMat::Rubble, art::WallMat::Wattle},
    {art::WallMat::Plaster, art::WallMat::Timber, art::WallMat::Brick},   {art::WallMat::Plaster, art::WallMat::Ashlar, art::WallMat::Brick},
    {art::WallMat::Plaster, art::WallMat::Adobe, art::WallMat::Brick},    {art::WallMat::Wattle, art::WallMat::Felt, art::WallMat::Plank},
    {art::WallMat::Wattle, art::WallMat::Plank, art::WallMat::Log},       {art::WallMat::Brick, art::WallMat::Plaster, art::WallMat::Stone},
    {art::WallMat::Timber, art::WallMat::Brick, art::WallMat::Plaster},   {art::WallMat::Plaster, art::WallMat::Ashlar, art::WallMat::Stone},
    {art::WallMat::Plank, art::WallMat::Living, art::WallMat::Timber},    {art::WallMat::Plaster, art::WallMat::Ashlar, art::WallMat::Stone},
};
const Cut kAltCut[(int)Archetype::COUNT][3] = {
    {Cut::Coat, Cut::Tunic, Cut::Kilt}, {Cut::Tunic, Cut::Kilt, Cut::Coat},  {Cut::Coat, Cut::Tunic, Cut::Robe},
    {Cut::Tunic, Cut::Robe, Cut::Wrap}, {Cut::Robe, Cut::Kaftan, Cut::Wrap}, {Cut::Kaftan, Cut::Coat, Cut::Tunic},
    {Cut::Tunic, Cut::Wrap, Cut::Poncho}, {Cut::Kaftan, Cut::Robe, Cut::Wrap}, {Cut::Tunic, Cut::Coat, Cut::Robe},
    {Cut::Poncho, Cut::Wrap, Cut::Tunic}, {Cut::Tunic, Cut::Coat, Cut::Robe}, {Cut::Coat, Cut::Robe, Cut::Gown},
};
const HelmForm kAltHelm[(int)Archetype::COUNT][2] = {
    {HelmForm::Horned, HelmForm::Nasal},   {HelmForm::GreatHelm, HelmForm::Nasal}, {HelmForm::Kettle, HelmForm::Crested},
    {HelmForm::Plumed, HelmForm::GreatHelm}, {HelmForm::Masked, HelmForm::Plumed}, {HelmForm::Spangen, HelmForm::Horned},
    {HelmForm::Nasal, HelmForm::Plumed},   {HelmForm::Crested, HelmForm::Winged},   {HelmForm::Crested, HelmForm::GreatHelm},
    {HelmForm::Masked, HelmForm::Crested}, {HelmForm::Winged, HelmForm::Masked},    {HelmForm::Crested, HelmForm::Plumed},
};
art::RoofMat roofMatSwap(Culture& c, Picker& p) {
  using art::RoofMat;
  using art::RoofShape;
  const RoofShape r = c.arch.roof;
  RoofMat opts[4];
  int n = 0;
  switch (r) {
    case RoofShape::Hip: case RoofShape::Gable: case RoofShape::Steep:
      opts[0] = RoofMat::Shingle; opts[1] = RoofMat::Thatch; opts[2] = RoofMat::Slate; opts[3] = RoofMat::ClayTile; n = 4;
      if (c.archetype == Archetype::Marsh || c.archetype == Archetype::SunTemple) { opts[2] = RoofMat::Palm; opts[3] = RoofMat::Bark; }
      if (c.archetype == Archetype::Fjordfolk) { opts[1] = RoofMat::Bark; opts[3] = RoofMat::Turf; }
      break;
    case RoofShape::Pagoda: opts[0] = RoofMat::GlazedTile; opts[1] = RoofMat::ClayTile; opts[2] = RoofMat::Slate; n = 3; break;
    case RoofShape::Mansard: opts[0] = RoofMat::Slate; opts[1] = RoofMat::ClayTile; opts[2] = RoofMat::Copper; n = 3; break;
    case RoofShape::Dome: opts[0] = RoofMat::Adobe; opts[1] = RoofMat::GlazedTile; opts[2] = RoofMat::Copper; n = 3;
      if (c.archetype == Archetype::Steppe) { opts[0] = RoofMat::Felt; }
      break;
    case RoofShape::Onion: opts[0] = RoofMat::Copper; opts[1] = RoofMat::GlazedTile; opts[2] = RoofMat::Slate; n = 3; break;
    case RoofShape::Conical: opts[0] = RoofMat::Felt; opts[1] = RoofMat::Bark; opts[2] = RoofMat::Thatch; opts[3] = RoofMat::Slate; n = 4;
      if (c.archetype == Archetype::Starspire) { opts[0] = RoofMat::Copper; opts[2] = RoofMat::GlazedTile; }
      break;
    case RoofShape::Sweep: opts[0] = RoofMat::Leaf; opts[1] = RoofMat::Bark; opts[2] = RoofMat::Shingle; n = 3; break;
    case RoofShape::FlatParapet: opts[0] = RoofMat::Adobe; opts[1] = RoofMat::ClayTile; n = 2; break;
    default: n = 0; break;   // Turf, Stepped: the material is the form
  }
  int k = p.pick(n > 0 ? n : 1);
  for (int t = 0; t < n; t++, k = (k + 1) % n)
    if (opts[k] != c.arch.roofMat) return opts[k];
  // the form fixes the material: a sister roof form instead (turf -> steep shingle, stepped -> flat adobe terrace)
  if (r == RoofShape::Turf) { c.arch.roof = RoofShape::Steep; c.arch.pitch = 4; return RoofMat::Shingle; }
  if (r == RoofShape::Stepped) { c.arch.roof = RoofShape::FlatParapet; return RoofMat::ClayTile; }
  return c.arch.roofMat == RoofMat::Shingle ? RoofMat::Thatch : RoofMat::Shingle;
}
void applySwap(Culture& c, int comp, Picker& p) {
  const int a = (int)c.archetype < (int)Archetype::COUNT ? (int)c.archetype : 2;
  switch (comp) {
    case 0: {   // wall material
      int k = p.pick(3);
      for (int t = 0; t < 3; t++, k = (k + 1) % 3)
        if (kAltWall[a][k] != c.arch.wall) { c.arch.wall = kAltWall[a][k]; break; }
      c.arch.wallTint = detail::wallColourFor(c.arch.wall, c.arch.wallTint);
      break;
    }
    case 1:   // the town layout variant
      std::swap(c.town.layout, c.town.altLayout);
      if (c.town.layout == c.town.altLayout) c.town.layout = c.town.layout == Layout::Organic ? Layout::Linear : Layout::Organic;
      break;
    case 2: {   // roof material
      const art::RoofMat m = roofMatSwap(c, p);
      c.arch.roofMat = m;
      c.arch.roofTint = detail::roofColourFor(m, c.arch.roofTint);
      break;
    }
    case 3: {   // clothing cut (men's; women's follows when it was the same)
      int k = p.pick(3);
      for (int t = 0; t < 3; t++, k = (k + 1) % 3)
        if (kAltCut[a][k] != c.dress.cutM) {
          if (c.dress.cutF == c.dress.cutM && kAltCut[a][k] != Cut::Kilt) c.dress.cutF = kAltCut[a][k];
          c.dress.cutM = kAltCut[a][k];
          break;
        }
      break;
    }
    default: {   // helm form
      const HelmForm cur = c.arms.helm[0];
      HelmForm nx = c.arms.helm[1] != cur ? c.arms.helm[1] : kAltHelm[a][p.pick(2)];
      if (nx == cur) nx = kAltHelm[a][0] != cur ? kAltHelm[a][0] : kAltHelm[a][1];
      c.arms.helm[1] = cur;
      c.arms.helm[0] = nx;
      break;
    }
  }
}
uint8_t dial(uint8_t v, Picker& p, int lo = 0, int hi = 255) {   // +-15 %
  const int x = (int)v * (85 + p.pick(31)) / 100;
  return (uint8_t)(x < lo ? lo : x > hi ? hi : x);
}
uint32_t shiftColour(uint32_t c, Picker& p) {   // an accent of the same family, moved round the wheel a little
  const int r = c & 255, g = (c >> 8) & 255, b = (c >> 16) & 255;
  int nr = r, ng = g, nb = b;
  switch (p.pick(3)) {
    case 0: nr = (r * 2 + g) / 3; ng = (g * 2 + b) / 3; nb = (b * 2 + r) / 3; break;
    case 1: nr = (r * 2 + b) / 3; ng = (g * 2 + r) / 3; nb = (b * 2 + g) / 3; break;
    default: nr = r * 3 / 4; ng = g * 3 / 4; nb = b * 3 / 4; break;
  }
  return rgbaC(nr, ng, nb);
}
}  // namespace

namespace {
// banner shapes by archetype: hanging square (heartland, imperial, river), swallow-tail (fjord, dune), pennant
// (highland, marsh, sylvan), gonfalon with tails (steppe, jade, sun-temple, starspire)
uint8_t bannerShapeFor(Archetype a) {
  static const uint8_t k[(int)Archetype::COUNT] = {1, 2, 0, 0, 1, 3, 2, 3, 0, 3, 2, 3};
  return (int)a < (int)Archetype::COUNT ? k[(int)a] : 0;
}
}  // namespace

const char* peopleName(People p) {
  static const char* const n[] = {"HUMAN", "HALF-BREED", "ELF"};
  return (int)p < 3 ? n[(int)p] : "HUMAN";
}
const char* archetypeName(Archetype a) { return (int)a < (int)Archetype::COUNT ? kArchName[(int)a] : "HEARTLAND"; }
const char* tierName(Tier t) {
  static const char* const n[] = {"LEATHER", "BRONZE", "IRON", "STEEL"};
  return (int)t < 4 ? n[(int)t] : "STEEL";
}
uint32_t skinRamp(int i) {
  static const uint32_t k[kSkinSteps] = {rgbaC(250, 222, 200), rgbaC(244, 204, 168), rgbaC(236, 188, 146), rgbaC(232, 180, 140),
                                         rgbaC(218, 164, 120), rgbaC(200, 146, 104), rgbaC(180, 124, 86),  rgbaC(160, 108, 74),
                                         rgbaC(150, 100, 70),  rgbaC(128, 84, 58),   rgbaC(110, 72, 50),   rgbaC(88, 58, 42)};
  return k[i < 0 ? 0 : i >= kSkinSteps ? kSkinSteps - 1 : i];
}

// ------------------------------------------------------------------ making a culture
Culture Atlas::make(Archetype a, uint32_t seed, int homeBiome) {
  Culture c;
  c.seed = seed;
  c.homeBiome = (uint8_t)homeBiome;
  detail::applyPriors(c, (int)a < (int)Archetype::COUNT ? a : Archetype::Heartland, seed, -1, -1);
  detail::finishCulture(c);
  c.heraldry = heraldryCandidate(((uint64_t)seed << 16) ^ 0x4E4Aull);
  c.heraldry.shape = bannerShapeFor(c.archetype);
  return c;
}

// ------------------------------------------------------------------ the atlas
struct Atlas::Impl {
  uint64_t seed = 0;
  std::function<ClimateSample(int32_t, int32_t)> climate;
  std::unordered_map<CultureId, std::unique_ptr<Culture>> memo;
  struct Aux { detail::NameHist h; detail::PalKey k; };
  std::unordered_map<CultureId, Aux> aux;                  // families' name histograms and palette keys (the maximin)
  std::unordered_map<uint64_t, Heraldry> heraldry;         // the kingdom lattice's arms
  uint8_t swapPerm[10] = {};                               // this world's order of the 10 dialect descriptors
  Culture fallback;
  Stats stats;
};

Atlas::Atlas(uint64_t worldSeed, std::function<ClimateSample(int32_t, int32_t)> climate) : d_(new Impl) {
  d_->seed = worldSeed;
  d_->climate = std::move(climate);
  d_->fallback = make(Archetype::Heartland, h32(worldSeed ^ 0xFA11BAC4ull), 2);
  Picker p(worldSeed ^ 0x5A5Bull);
  for (int i = 0; i < 10; i++) d_->swapPerm[i] = (uint8_t)i;
  for (int i = 9; i > 0; i--) std::swap(d_->swapPerm[i], d_->swapPerm[p.pick(i + 1)]);
}
Atlas::~Atlas() = default;
uint64_t Atlas::seed() const { return d_->seed; }
const Atlas::Stats& Atlas::stats() const { return d_->stats; }

namespace {
// the culture of a family candidate (candidate k of cell (ci, cj))
Culture familyCandidate(uint64_t cellH, int k, const ClimateSample& cs, int ore, int forceArch = -1) {
  const uint32_t sk = h32(cellH + (uint64_t)k * 0xD1B54A32D192ED03ull);
  const Archetype a = forceArch >= 0 ? (Archetype)forceArch : drawArchetype(cs, sk ^ 0xA7C4u, k);
  // an isolated family (about 1 cell in 48) crosses its archetype with another
  int cross = -1;
  if ((cellH >> 40) % 48 == 0) cross = (int)((a == Archetype::COUNT ? 0 : (int)a) + 1 + (int)((cellH >> 20) % 11)) % (int)Archetype::COUNT;
  Culture c;
  c.seed = sk;
  c.homeBiome = (uint8_t)(cs.biome >= 0 && cs.biome < 10 ? cs.biome : 2);
  detail::applyPriors(c, a, sk, ore, cross);
  c.heraldry = heraldryCandidate(cellH ^ 0x4E4A11ull);
  c.heraldry.shape = bannerShapeFor(a);
  return c;
}
}  // namespace

const Culture& Atlas::family(int32_t ci, int32_t cj) {
  const CultureId id = familyId(ci, cj);
  auto it = d_->memo.find(id);
  if (it != d_->memo.end()) return *it->second;
  // the lower-phase neighbours first (memoised recursion, depth <= 3)
  const int phase = (ci & 1) + 2 * (cj & 1);
  std::vector<std::pair<const Culture*, const Impl::Aux*>> nb;
  if (phase > 0)
    for (int dj = -1; dj <= 1; dj++)
      for (int di = -1; di <= 1; di++) {
        if (!di && !dj) continue;
        const int32_t ni = ci + di, nj = cj + dj;
        if ((ni & 1) + 2 * (nj & 1) >= phase) continue;
        const Culture& n = family(ni, nj);
        nb.push_back({&n, &d_->aux[n.id]});
      }
  auto t0 = std::chrono::steady_clock::now();
  const int32_t cx = ci * ew::CCELL + ew::CCELL / 2, cy = cj * ew::CCELL + ew::CCELL / 2;
  ClimateSample cs;
  if (d_->climate) cs = d_->climate(cx, cy);
  // culture_map.cpp packs the province's richest ore into temp's top byte (ore + 1; 0 = unknown)
  const int ore = (int)((uint32_t)cs.temp >> 24) - 1;
  cs.temp &= 0xFFFFFF;
  const uint64_t cellH = ew::cellSeed(d_->seed, ew::tag("culture"), ci, cj);

  std::unique_ptr<Culture> best;
  Impl::Aux bestA;
  if (nb.empty()) {
    best = std::make_unique<Culture>(familyCandidate(cellH, 0, cs, ore));
    bestA.h = detail::nameHist(*best);
    bestA.k = detail::palKey(*best);
    d_->stats.candidates++;
  } else {
    // Branch and bound over the candidates, exact: the cheap components (everything but the name bigrams) bound each
    // candidate's score from above (names add at most 0.10 to a distance and one headline difference), so only the
    // candidates that could still win have their 200 sample names drawn.
    //   score = [valid] * 2^32 + minD + 0.15 climate fit - 0.1 per headline equal to a neighbour's
    //   valid = minD >= 0.46 (a hair over the 0.45 bar) and >= 3 headline differences with every neighbour
    struct Cand { Culture c; detail::PalKey key; int64_t ub; int k; };
    std::vector<Cand> cands;
    auto addCands = [&](int from, int to, const std::vector<int>* force = nullptr) {
      for (int k = from; k < to; k++) {
        const int fa = force && !force->empty() ? (*force)[(size_t)(k - from) % force->size()] : -1;
        Cand x{familyCandidate(cellH, k, cs, ore, fa), {}, 0, k};
        x.key = detail::palKey(x.c);
        d_->stats.candidates++;
        int32_t ubMin = 65536;
        int hubMin = 5;
        for (const auto& n : nb) {
          int32_t d;
          int hd;
          detail::compareCheap(x.c, x.key, *n.first, n.second->k, d, hd);
          ubMin = std::min(ubMin, d + 6554);
          hubMin = std::min(hubMin, hd + 1);
        }
        ubMin = std::min(ubMin, 65536);
        hubMin = std::min(hubMin, 5);
        const bool validUb = ubMin >= 30147 && hubMin >= 3;
        x.ub = (validUb ? (int64_t)1 << 32 : 0) + ubMin + (int64_t)fitOf(x.c.archetype, cs) * 9830 / 255 - (int64_t)(5 - hubMin) * 6554;
        cands.push_back(std::move(x));
      }
    };
    int64_t bestScore = INT64_MIN;
    int bestK = 1 << 30;
    auto evaluate = [&]() {
      std::vector<size_t> order(cands.size());
      for (size_t i = 0; i < order.size(); i++) order[i] = i;
      std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return cands[a].ub > cands[b].ub; });
      for (size_t oi : order) {
        Cand& x = cands[oi];
        if (x.ub < bestScore || (x.ub == bestScore && x.k > bestK)) break;
        detail::NameHist h = detail::nameHist(x.c);
        int32_t dmin = 65536;
        int hmin = 5;
        for (const auto& n : nb) {
          int32_t d;
          int hd;
          detail::compareFull(x.c, x.key, h, *n.first, n.second->k, n.second->h, d, hd);
          dmin = std::min(dmin, d);
          hmin = std::min(hmin, hd);
        }
        const bool valid = dmin >= 30147 && hmin >= 3;
        const int64_t score = (valid ? (int64_t)1 << 32 : 0) + dmin + (int64_t)fitOf(x.c.archetype, cs) * 9830 / 255 - (int64_t)(5 - hmin) * 6554;
        if (score > bestScore || (score == bestScore && x.k < bestK)) {
          bestScore = score;
          bestK = x.k;
          best = std::make_unique<Culture>(x.c);
          bestA.h = std::move(h);
          bestA.k = x.key;
        }
      }
    };
    addCands(0, 12);
    evaluate();
    if (bestScore < ((int64_t)1 << 32)) {   // none of the 12 meets the bar: draw 12 more (still deterministic)
      bestScore = INT64_MIN;
      bestK = 1 << 30;
      addCands(12, 24);
      evaluate();
    }
    if (bestScore < ((int64_t)1 << 32)) {
      // the last resort (about 1 cell in 2000): the archetypes none of the neighbours has, three draws each
      std::vector<int> absent;
      for (int a = 0; a < (int)Archetype::COUNT; a++) {
        bool used = false;
        for (const auto& n : nb) used |= (int)n.first->archetype == a;
        if (!used) absent.push_back(a);
      }
      bestScore = INT64_MIN;
      bestK = 1 << 30;
      addCands(24, 24 + 3 * (int)absent.size(), &absent);
      evaluate();
    }
  }
  detail::finishCulture(*best);
  best->id = id;
  d_->aux[id] = std::move(bestA);
  d_->stats.families++;
  d_->stats.ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  return *d_->memo.emplace(id, std::move(best)).first->second;
}

static const Heraldry& kingdomArms(std::unordered_map<uint64_t, Heraldry>& memo, uint64_t worldSeed, int32_t kx, int32_t ky, int& cands) {
  const uint64_t key = ((uint64_t)(uint32_t)kx << 32) | (uint32_t)ky;
  auto it = memo.find(key);
  if (it != memo.end()) return it->second;
  const int phase = (kx & 1) + 2 * (ky & 1);
  std::vector<Heraldry> nb;
  if (phase > 0)
    for (int dj = -1; dj <= 1; dj++)
      for (int di = -1; di <= 1; di++) {
        if (!di && !dj) continue;
        if (((kx + di) & 1) + 2 * ((ky + dj) & 1) >= phase) continue;
        nb.push_back(kingdomArms(memo, worldSeed, kx + di, ky + dj, cands));
      }
  const uint64_t h = ew::cellSeed(worldSeed, ew::tag("heraldry"), kx, ky);
  Heraldry best = heraldryCandidate(h);
  cands++;
  if (!nb.empty()) {
    int bestScore = -1;
    for (int k = 0; k < 12; k++) {
      Heraldry c = heraldryCandidate(h + (uint64_t)k * 0x9E3779B97F4A7C15ull);
      cands++;
      int mn = 3, sum = 0;
      for (const Heraldry& o : nb) { const int d = heraldryDiffs(c, o); mn = std::min(mn, d); sum += d; }
      const int score = mn * 64 + sum;
      if (score > bestScore) { bestScore = score; best = c; }
      if (mn == 3 && k >= 3) break;
    }
  }
  return memo.emplace(key, best).first->second;
}

const Culture& Atlas::dialect(int32_t ci, int32_t cj, int32_t kx, int32_t ky) {
  const CultureId id = dialectId(ci, cj, kx, ky);
  auto it = d_->memo.find(id);
  if (it != d_->memo.end()) return *it->second;
  const Culture& F = family(ci, cj);
  auto t0 = std::chrono::steady_clock::now();
  auto c = std::make_unique<Culture>(F);
  c->id = id;
  c->seed = h32(ew::cellSeed(d_->seed, ew::tag("dialect"), kx, ky));
  Picker p(c->seed);
  // two discrete swaps from this kingdom's descriptor (3 x 3 tiling: neighbours never share one)
  const int label = posmod(kx, 3) + 3 * posmod(ky, 3);
  const uint8_t* sw = kSwapPairs[d_->swapPerm[label]];
  applySwap(*c, sw[0], p);
  applySwap(*c, sw[1], p);
  // the accent palette: one of the family's colours, moved
  const uint32_t acc = c->arch.accentTint;
  c->arch.accentTint = shiftColour(p.chance(128) ? c->arch.altTint : c->dress.cloth[3], p);
  c->arch.altTint = acc;
  c->dress.cloth[2] = shiftColour(c->dress.cloth[2], p);
  c->props.awningA = c->arch.accentTint;
  c->props.awningB = acc;
  // a roof ornament on or off
  static const uint16_t roofOrn[6] = {art::ORN_FINIALS, art::ORN_CARVED_RIDGE, art::ORN_DRAGON_HEADS, art::ORN_ROOF_STONES,
                                      art::ORN_PAINTED_BANDS, art::ORN_LANTERNS};
  c->arch.ornament = (uint16_t)(c->arch.ornament ^ roofOrn[p.pick(6)]);
  // the place-suffix set: one dropped, the order shifted (the family's tongue, a regional accent)
  auto& suf = c->phon.placeSuffix;
  if (suf.size() > 2) {
    suf.erase(suf.begin() + p.pick((int)suf.size()));
    std::rotate(suf.begin(), suf.begin() + 1, suf.end());
  }
  // +-15 % on the continuous dials
  c->arch.wallH = dial(c->arch.wallH ? c->arch.wallH : 102, p, 1, 255);
  if (p.chance(70) && c->arch.pitch > 0) c->arch.pitch = (uint8_t)std::clamp((int)c->arch.pitch + (p.chance(128) ? 1 : -1), 1, 4);
  c->town.density = dial(c->town.density, p);
  c->town.trees = dial(c->town.trees, p);
  c->dress.headP = dial(c->dress.headP, p);
  c->dress.beardP = dial(c->dress.beardP, p);
  c->dress.jewellery = dial(c->dress.jewellery, p);
  for (uint8_t& v : c->values) v = dial(v, p);
  if (c->music.bpm) c->music.bpm = dial(c->music.bpm, p, 50, 180);
  c->music.swing = (uint8_t)std::clamp((int)c->music.swing + p.range(-2, 2), 0, 15);
  c->music.ornament = (uint8_t)std::clamp((int)c->music.ornament + p.range(-2, 2), 0, 15);
  c->music.drone = (uint8_t)std::clamp((int)c->music.drone + p.range(-2, 2), 0, 15);
  c->music.seed = (uint16_t)(c->seed >> 9);
  // the kingdom's arms and its own name
  int cands = 0;
  const uint8_t shape = F.heraldry.shape;
  c->heraldry = kingdomArms(d_->heraldry, d_->seed, kx, ky, cands);
  c->heraldry.shape = shape;
  d_->stats.candidates += cands;
  for (int t = 0; t < 6; t++) {
    std::string n = kingdomName(*c, c->seed + (uint32_t)t * 7u);
    std::string clean;
    for (char ch : n) if (ch >= 'A' && ch <= 'Z') clean += ch;
    if (clean.size() >= 4 && clean.size() <= 9) { c->name = clean; break; }
  }
  const char last = c->name.empty() ? 'A' : c->name.back();
  const bool vowel = last == 'A' || last == 'E' || last == 'I' || last == 'O' || last == 'U' || last == 'Y';
  static const char* const adj[(int)Archetype::COUNT] = {"SK", "ACH", "ISH", "IAN", "I", "I", "AN", "ESE", "ER", "ECA", "ARI", "IAN"};
  c->adjective = c->name + (vowel ? std::string("N") : std::string(adj[(int)c->archetype < (int)Archetype::COUNT ? (int)c->archetype : 2]));
  d_->stats.dialects++;
  d_->stats.ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  return *d_->memo.emplace(id, std::move(c)).first->second;
}

const Culture& Atlas::get(CultureId id) {
  if (cultureKind(id) == 1) return family(cultureCi(id), cultureCj(id));
  if (cultureKind(id) == 2) return dialect(cultureCi(id), cultureCj(id), cultureKx(id), cultureKy(id));
  return d_->fallback;
}

}  // namespace cult
