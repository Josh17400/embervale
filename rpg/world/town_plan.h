// M7c "Order of Streets" (VISION_PLAN 15.M7c, owner 2026-10-11): ONE settlement planner for villages, towns and cities,
// driven by an ORDER DIAL. This header is the planner's shared vocabulary (LEAD-frozen in phase A: lanes may read it,
// never edit it; anything new goes in their own files):
//   - the order dial (orderFor): low for a village (one spine road, houses scattered round it and loosely facing it,
//     paths wandering to the well, mill and fields: the organic charm the owner loves), mid for a town (a real main
//     street and square, a few structured side streets with frontage, loosening into scattered houses and yards), high
//     for a city (inside the walls a full street network: avenues from the gates to the plaza and seat, streets off
//     them forming real blocks, row houses fronting every street, blocks with roles; outside the walls faubourgs along
//     the approach roads, sporadic houses away from them, then organic farmland);
//   - the road hierarchy every settlement shares (Rank: main road > street > lane > footpath, plus squares), one byte per
//     buffer tile in SettlementOut::rank;
//   - block roles (BlockRole), one byte per buffer tile in SettlementOut::role (a city's whole blocks: seat or temple
//     precinct, market block, park, orchard, kitchen gardens, craft yards...; outside the walls faubourg plots, farms);
//   - the plan's traits (PlanTraits: what makes this city this city), SettlementOut::traits, for dumps and reviews;
//   - the checks (rpg/world/town_metrics.cpp): the structure score (city >> town >> village, every city above a floor),
//     the plan signature and similarity (no two cities near-duplicates), the village character (low structure,
//     organic placement, one spine).
// The rank and role layers are written by the generator (town::Gen::rank / role, copied into SettlementOut at the end
// of finish()); they are never saved and never reach the chunks (the view reads Map::urb as before).
// Determinism: everything here is integer maths or + - * / sqrt on floats; no libm transcendentals.
#pragma once
#include <cstdint>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/world.h"

namespace ew {

struct SettlementOut;
struct SitePlan;

// ---------------------------------------------------------------- the road hierarchy (SettlementOut::rank)
enum class Rank : uint8_t {
  None = 0,
  Main,       // a main road: a village's spine, a town's main street, a city's avenue (and the approach roads)
  Street,     // a street: a town's structured side street, a city's block street (fronted both sides)
  Lane,       // a lane or alley (narrow, a back way between plots or through a block)
  Footpath,   // a footpath: to a door, the well, the mill, across a park, out to the fields
  Square,     // a square, close, court or well-square (open paving at a junction)
  COUNT
};
inline bool rankStreet(Rank r) { return r == Rank::Main || r == Rank::Street || r == Rank::Lane; }   // a carriageway
inline bool rankWay(Rank r) { return rankStreet(r) || r == Rank::Square; }                             // street or square
const char* rankName(Rank r);   // rpg/world/town_metrics.cpp

// ---------------------------------------------------------------- block roles (SettlementOut::role)
enum class BlockRole : uint8_t {
  None = 0,
  Homes,          // an ordinary block of homes (row houses round shared back yards and gardens)
  Seat,           // the seat of power's precinct (palace, keep, khan's court, council spire...)
  Temple,         // the temple or cathedral close / precinct
  Market,         // the market block / market square and its halls
  Park,           // a park or green
  Orchard,        // an orchard block inside the walls
  KitchenGarden,  // allotments and kitchen gardens
  CraftYard,      // craft yards and workshops (the trades' yards)
  Compound,       // walled family compounds (jade, dune)
  Barracks,       // the garrison's block
  Harbour,        // quays, harbour sheds, the waterfront
  Faubourg,       // outside the walls: the ribbon of inns, smithies, stables and houses along an approach road
  Farm,           // outside the walls: fields, orchards and pasture (organic, like the countryside)
  COUNT
};
const char* blockRoleName(BlockRole r);   // rpg/world/town_metrics.cpp

// ---------------------------------------------------------------- the order dial
// 0 (a scatter of huts) .. 100 (an imperial grid). Villages 10..25, towns 40..60, cities 70..100: the culture bends it
// (an ordered people's jade or imperial city reaches the top, the steppe khan's rings and the elves' city among the trees
// keep lower), a capital and an old city add a little. Deterministic from the inputs; the planners read it as Gen::order.
// cArch: cult::Archetype as int (-1: no culture); age: cityAge (0 young .. 3 ancient).
inline int orderFor(SiteType t, int cArch, bool capital, int age) {
  int base = t == SiteType::City ? 82 : (t == SiteType::Town ? 50 : 16);
  int bend = 0;
  using A = cult::Archetype;
  switch (cArch) {
    case (int)A::Imperial: case (int)A::Jade: bend = 8; break;                         // imperial grid, jade axial plan
    case (int)A::SunTemple: case (int)A::Starspire: bend = 4; break;                   // processional ways, the spire's geometry
    case (int)A::Fjordfolk: case (int)A::Highland: case (int)A::Dune: bend = -4; break; // harbour terraces, clans, courtyards
    case (int)A::Steppe: case (int)A::Marsh: case (int)A::Sylvan: bend = -8; break;    // rings, boardwalks, the city among trees
    default: break;                                                                     // heartland market grid, river towns
  }
  if (t == SiteType::Village) bend /= 4;
  else if (t == SiteType::Town) bend /= 2;
  int v = base + bend + (t == SiteType::City ? (capital ? 4 : 0) + age : 0);
  const int lo = t == SiteType::City ? 70 : (t == SiteType::Town ? 40 : 10), hi = t == SiteType::City ? 100 : (t == SiteType::Town ? 60 : 25);
  return v < lo ? lo : (v > hi ? hi : v);
}
// a city's age 0 (a young frontier city) .. 3 (an ancient capital, denser and more layered); hashed from its seed, a
// capital at least 2
inline int cityAge(uint32_t seed, bool capital) {
  const uint32_t h = hash32(seed ^ 0xA6E5EEDu);
  const int a = (int)(h % 4u);
  return capital && a < 2 ? 2 + (int)((h >> 8) & 1u) : a;
}

// ---------------------------------------------------------------- what makes a city this city (SettlementOut::traits)
// Filled by the planners as they decide (0 / empty where a choice was not made); printed by rpg_test --plan and the
// script command "plan"; the reviews read it next to the map. The checks never trust it: the structure score and the
// signature are measured on the built layout.
enum class WallShape : uint8_t { None = 0, Round, Oval, Polygon, River, Coast, Land, COUNT };   // the wall's outline
enum class WaterKind : uint8_t { None = 0, River, Lake, Coast, Marsh, COUNT };
struct PlanTraits {
  uint8_t order = 0;            // the dial (orderFor)
  uint8_t age = 0;              // cityAge
  uint8_t layout = 0;           // the culture layout built (cult::Layout)
  uint8_t planKind = 0;         // the planner's own plan type for this city (a lane's enum; 0 = the M7b plan)
  WallShape wall = WallShape::None;
  WaterKind water = WaterKind::None;
  uint8_t gates = 0, avenues = 0, bridges = 0;
  int16_t avenueDeg[6] = {};    // the avenues' bearings from the heart, degrees (0 east, 90 south)
  uint8_t blockW = 0, blockH = 0;   // the typical block (tiles, street to street)
  uint8_t gridSections = 0;     // grid sections laid at their own tilt
  int16_t gridDeg[4] = {};      // their tilts, degrees
  uint8_t plazas = 0;           // squares on the chain from the market to the seat
  int16_t plazaTiles = 0;       // the main plaza's paved area
  int8_t seatDX = 0, seatDY = 0;// the seat's offset from the heart, in tenths of the radii
  uint8_t blocks[(int)BlockRole::COUNT] = {};   // blocks given each role
  uint8_t faubourgs = 0;        // approach roads with a faubourg
  int16_t faubourgBldgs = 0;    // buildings outside the walls along the roads
};

// ---------------------------------------------------------------- the checks (rpg/world/town_metrics.cpp)
// The structure score (0..100) of a built settlement, measured on its rank layer and buildings over its core (the wall
// ring's box, else the buildings'):
//   frontage     share of buildings whose front (or a side, along a north-south street) faces a straight street (a run of
//                9+ street tiles through the tile it faces) a tile or less away, a building standing alone counting half
//                (a continuous frontage: a neighbour on the same front row a tile or less away counts it whole)
//   straightness share of the carriageway tiles lying on a straight run of 9+ street tiles (any of the four directions:
//                winding lanes score low, real streets high)
//   regularity   the blocks (land of 16..700 tiles cut off by ways, walls and water, half its rim or more street) weighted
//                by how rectangular they are (area / the bounding box's, the better of axis-aligned and diagonal), times
//                the share of the buildings' tiles standing in such blocks
//   deadEnds     1 - the coarse street graph's dead ends (4-tile cells) over (blocks + dead ends)
//   connectivity the blocks against what a full street network of the core's land would hold (one per 260 tiles)
// total = 30 frontage + 20 straightness + 20 regularity + 10 deadEnds + 20 connectivity. Cities clearly above towns,
// towns clearly above villages, every city above a floor (the gates below, rpg_test --structure).
struct StructureScore {
  float frontage = 0, straightness = 0, regularity = 0, deadEnds = 0, connectivity = 0, total = 0;
  int bldgs = 0, fronting = 0, blocks = 0, streetCells = 0, ends = 0, loops = 0, streetTiles = 0;
};
StructureScore structureScore(const SettlementOut& o);

// the targets M7c must meet at the end of phase B (rpg_test --structure --strict: CI after integration)
constexpr float STRUCT_CITY_FLOOR = 68.0f;    // every city (M7b baseline: min 44.5, the steppe rings)
constexpr float STRUCT_CITY_MEAN = 78.0f;     // cities' mean (M7b baseline 70.7)
constexpr float STRUCT_TOWN_MIN = 35.0f;      // towns' mean at least (M7b baseline 42.2)
constexpr float STRUCT_TOWN_MAX = 62.0f;      // towns' mean at most (a town is not a city)
constexpr float STRUCT_VILLAGE_MAX = 40.0f;   // every village at most (M7b: median 27, 85% under 40; over it the radial / grid villages)
constexpr float STRUCT_VILLAGE_MEAN = 28.0f;  // the villages' mean at most (M7b baseline 28.7)
constexpr float STRUCT_GAP = 15.0f;           // city mean - town mean and town mean - village mean, at least

// The plan signature of a walled city: its core (the wall ring's box) resampled to SIG_N x SIG_N cells (each cell's
// street share, building share and open-ground share) and its wall's radial profile. planSimilarity is the best match
// over the 8 rotations and mirror images of: the Jaccard overlap of the cells that are mostly street (share >= 0.35:
// where the streets run), the same for buildings, the radial profiles' agreement and the aspects', weighted 0.5 / 0.2 /
// 0.2 / 0.1: 0 (nothing alike) .. 1 (identical). Two cities at or above CITY_DUP_SIM are near-duplicates
// (rpg_test --city-unique).
constexpr int SIG_N = 32, SIG_RAYS = 32;
struct PlanSig {
  bool valid = false;
  float street[SIG_N * SIG_N] = {}, bldg[SIG_N * SIG_N] = {}, open[SIG_N * SIG_N] = {};
  float ray[SIG_RAYS] = {};     // the wall's distance from its centroid per bearing, over the mean (1 = round)
  float aspect = 1;             // the wall box's width over its height
};
PlanSig planSignature(const SettlementOut& o);
float planSimilarity(const PlanSig& a, const PlanSig& b);
constexpr float CITY_DUP_SIM = 0.70f;         // no two cities' plans this alike (or more) (M7b baseline: most alike 0.52)
constexpr float CITY_MEAN_SIM_MAX = 0.48f;    // and the cities' mean pairwise similarity at most this (M7b baseline 0.39)

// The village character (the order dial's low end; the byte-identical --villages lock is replaced by it):
//   structure   its structure score (at most STRUCT_VILLAGE_MAX)
//   spines      main-road arms through the heart (a crossroads hamlet two, a road village one, a green its ring)
//   setbackVar  the spread of the houses' setbacks from their street (tiles; organic >= VILLAGE_SETBACK_VAR)
//   aligned     share of houses standing in a row with a neighbour (same front row, a tile or less apart; organic <=)
//   blocks      enclosed street blocks (a village has at most VILLAGE_BLOCKS)
//   cityFabric  Map::urb tiles or Bldg::row party walls (always 0 in a village)
struct VillageCharacter {
  float structure = 0, setbackVar = 0, aligned = 0;
  int spines = 0, blocks = 0, cityFabric = 0, homes = 0;
  bool ok = false;
  const char* why = "";
};
VillageCharacter villageCharacter(const SettlementOut& o);
constexpr float VILLAGE_SETBACK_VAR = 0.40f;
constexpr float VILLAGE_ALIGNED_MAX = 0.45f;
constexpr int VILLAGE_BLOCKS = 4;
constexpr int VILLAGE_SPINES_MAX = 3;

}  // namespace ew
