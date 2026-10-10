// The TOWNS lane's settlement generator (VISION_PLAN 2.5 L2a, 15.8), internal to rpg/world/town_*.cpp and
// rpg/world/settlement.cpp. One Gen builds one settlement on its local buffer (SettlementOut::buf):
//   town_layout.cpp  the land, the heart, the road bearings, squares, main streets, ring roads, lanes, city walls,
//                    gatehouses and side gates, the lanes that join every opening to the streets, approach roads
//   town_build.cpp   buildings: landmarks and services first, then homes by district (frontage plots with uneven
//                    setbacks and footpaths to the doors), the V5 sprite clearance on occupancy grids, the capital's
//                    palace compound (walled courtyard, gardens, fountain, palace, barracks, royal guard)
//   town_dress.cpp   centrepieces, lamps, banners, signposts, yards and gardens, fields and pastures, archetype
//                    dressing (quays, palisades), greenery, townsfolk and guards, the used mask
//   town_market.cpp  (M1 economy) the settlement's specialisation and its production buildings, the market: stalls in
//                    tidy rows facing the walking space with their stock and keepers; the other squares' benches and
//                    trees; the trades' yards (mine, ore carts, log piles, drying racks, hide frames, troughs)
// Determinism: only the ctx inputs; one Rng seeded from the plan's seed; + - * / sqrt and ew::dsin/dcos/datan2 (no libm
// transcendentals), so MSVC, clang and Emscripten build the same town.
#pragma once
#include <cstdint>
#include <utility>
#include <vector>
#include "rpg/culture/society.h"
#include "rpg/world/dmath.h"
#include "rpg/world/settlement.h"
#include "rpg/world/town_rules.h"

namespace ew {
namespace town {

constexpr int MARGIN = 8;   // buffer tiles around the footprint (fields, approach roads, signposts)

// what a tile is to the town (Gen::mask)
enum Kind : uint8_t {
  K_NONE = 0,
  K_MAIN = 1,      // main street (the roads through town)
  K_LANE = 2,      // lane / side street
  K_SQUARE = 3,    // square, green, the palace walkway
  K_YARD = 4,      // footpath, doorstep, yard, garden, lamp spot
  K_FIELD = 5,     // field or pasture
  K_COMPOUND = 6,  // the palace compound's grounds (nothing else is built there)
  K_LOT = 7        // (M7) a lot for sale (rpg/world/plots.h): its fence ring and the clear ground inside (town_lots.cpp)
};

// city districts (VISION_PLAN 15.8): the old centre with the market, and four sectors around it
enum class District : uint8_t { Centre, Crafts, Temple, Noble, Poor, COUNT };

struct Gen {
  Gen(const SettlementCtx& c, SettlementOut& o);
  void run();
  bool step();                     // the next phase of run() (false: finished); the web builds a town over frames
  int phase = 0;

  // ---------------------------------------------------------------- inputs and shape
  const SettlementCtx& C;
  const SitePlan& P;
  SettlementOut& O;
  Map& M;
  Rng rng;
  int W = 0, H = 0;
  bool city = false, town = false, village = false, capital = false;
  bool walled = false;           // a curved stone wall (cities, hill-fort towns)
  bool palisade = false;         // a fence ring (hill-fort villages)
  Archetype arch = Archetype::Plain;
  Specialty spec = Specialty::None;   // (M1 economy) what the settlement lives from (SitePlan::special, else derived)
  int cx = 0, cy = 0;            // the heart (local)
  float rx = 1, ry = 1;          // the settlement's radii (1.0 = the footprint's edge)
  float wallR = 0.93f;           // the wall ring's normalised radius
  uint32_t bseed = 0, wseed = 0; // blob seeds (town outline, wall)
  Ground base = Ground::Grass, mainG = Ground::Road, laneG = Ground::Dirt;
  Biome bio = Biome::Plains;
  Eco eco = Eco::Meadow;         // (M3c) the town's biome proper (its family is bio): the trees it plants (townTree)
  int layout = 0;                // villages: 0 crossroads hamlet, 1 road village, 2 village green
  int homesWant = 0;
  // ---------------------------------------------------------------- M3: the culture's settlement (VISION_PLAN 5.6, 5.7)
  // style: the culture's TownStyle::layout (altLayout in about 1 in 4 of its settlements, by the plan's seed);
  // Organic without a culture. cArch: the culture's cult::Archetype (-1: none).
  cult::Layout style = cult::Layout::Organic;
  int cArch = -1;
  uint8_t wallByte = 1;          // Map::wall on the town's wall tiles: 1 + the culture's art::CityWall
  float densityF = 1.0f;         // TownStyle::density / 128: setbacks and gardens shrink as it grows
  float treesF = 1.0f;           // TownStyle::trees / 128: street trees, orchards and gardens
  float wander = 1.0f;           // how much streets wander (a grid's streets run straighter)
  bool terraced = false;         // Terraced: the town cut its own terraces into lvl (cliff faces and stairs in finish)
  bool stilt = false;            // Stilt: houses stand on stilts over the marsh, boardwalks join them
  float spineA = 0;              // Linear: the bearing of the spine (along the shore, the river, the causeway)
  float spineX = 0, spineY = 0;  // Linear: a point the spine runs through (the heart, or the bank beside it)
  int gridSX = 13, gridSY = 9;   // Grid / Compound: the lattice's spacing (tiles)
  float gridTheta = 0;           // Grid: the lattice's small tilt
  int centreKind = -1;           // the main square's centrepiece (art::PropStyle::centre), -1: the classic choice
  int townWealth = -1;           // the settlement's wealth 0..3 (townWealthFor), -1: no culture (no skew)
  bool cultureIs(int a) const { return cArch == a; }
  // ---------------------------------------------------------------- M3b: the society (rpg/culture/society.h)
  // hasSoc: a culture builds this settlement, so its society decides the services, the seat of power and the spaces
  // (services() keeps the classic lists without one); tier: the settlement's cult::SettleTier
  cult::Society soc;
  bool hasSoc = false;
  cult::SettleTier tier = cult::SettleTier::Village;
  // the builder fields of the next building putBldg makes (bld::Form, CIVIC_* bits, cult::Seat + 1); placeWant sets
  // them for the want it places and clears them after
  uint8_t curForm = 0, curCivic = 0, curSeat = 0;
  int seatIdx = -1;              // the seat of power (the capital's ruler's, a city's lord's), -1: none
  int wealthFor(art::Building t, const IRect& r) const;   // 0 poor .. 3 rich (district, size, capital)

  // ---------------------------------------------------------------- per tile
  std::vector<uint8_t> mask;     // Kind
  std::vector<uint8_t> water;    // the base land was water (river, lake, sea): bridges, culverts, quays
  std::vector<uint8_t> lvl;      // relief level 0..7 of the base land
  std::vector<uint8_t> inside;   // walled: inside the ring
  std::vector<uint8_t> noBuild;  // no building or roof here (gate approaches, the compound, beside the wall)
  std::vector<uint8_t> cover;    // a building's sprite covers this tile (V5 clearance)
  std::vector<uint8_t> front;    // a building's front (foundation row, doorstep, apron): no sprite may cover it
  std::vector<std::pair<int, int>> mainTiles, allStreet;
  std::vector<float> bearings;   // where the roads leave, radians (0 east, +pi/2 south)
  struct Square { int x, y; float r; District d; };
  std::vector<Square> squares;
  float sector0 = 0;             // city districts: the noble sector's centre angle
  District sectorKind[4] = {District::Noble, District::Temple, District::Crafts, District::Poor};
  IRect compound;                // the palace compound, walls included (w == 0: none)
  int palaceIdx = -1, barracksIdx = -1, keepIdx = -1;
  std::vector<int> gateBearing;  // per wall gap: index of the bearing it serves (-1: a side gate, -2: the palace compound)
  int slot = 0;                  // spawn slots
  // (M1 economy) the market: every stall (its middle tile and trade) and the tiles kept clear for its customers and
  // keepers (no banner, bench or tree may stand there)
  struct Stall { int x, y, trade; };
  std::vector<Stall> stalls;
  std::vector<uint8_t> reserved;
  int cpX = -1, cpY = -1;        // the main square's centrepiece (towns and cities: across the square from the market)
  // (M1 fixer) the market place: the side of the heart the main square opens out to for the stalls (0 north, 1 south,
  // 2 east, 3 west; by the town's seed, so no two towns hold their market the same way) and that part's bounds
  int mktSide = 0;
  IRect mktZone;
  int pathMax = 12;              // the longest footpath from a door to the street

  // ---------------------------------------------------------------- helpers
  size_t I(int x, int y) const { return (size_t)y * W + x; }
  bool in(int x, int y) const { return x >= 0 && y >= 0 && x < W && y < H; }
  uint8_t get(int x, int y) const { return in(x, y) ? mask[I(x, y)] : 0; }
  void set(int x, int y, uint8_t v) { if (in(x, y)) mask[I(x, y)] = v; }
  bool isStreet(int x, int y) const { uint8_t k = get(x, y); return k == K_MAIN || k == K_LANE || k == K_SQUARE; }
  bool wet(int x, int y) const { return in(x, y) && groundWater(M.at(x, y)); }
  bool ins(int x, int y) const { return in(x, y) && !inside.empty() && inside[I(x, y)]; }
  bool wallAt(int x, int y) const { return in(x, y) && M.wall[I(x, y)] != 0; }
  bool bldgAt(int x, int y) const { return !in(x, y) || M.bldgAt[I(x, y)] >= 0; }
  bool inCompound(int x, int y, int pad = 0) const {
    return compound.w > 0 && x >= compound.x - pad && y >= compound.y - pad && x < compound.x + compound.w + pad && y < compound.y + compound.h + pad;
  }
  float dist(int x, int y) const;    // normalised distance from the heart (1 = the footprint's edge, no wobble)
  float blob(int x, int y, int bx, int by, float brx, float bry, uint32_t s) const;   // wobbly ellipse distance
  float angleOf(int x, int y) const; // bearing of a tile from the heart
  District districtAt(int x, int y) const;
  uint32_t hashAt(int x, int y, uint32_t k) const { return hash2(x, y, P.seed * 0x9E3779B1u + k); }
  float hfAt(int x, int y, uint32_t k) const { return (hashAt(x, y, k) >> 8) * (1.0f / 16777216.0f); }

  // ---------------------------------------------------------------- town_layout.cpp
  void land();                       // buffer from the base land, the heart, the outline cleared
  void pickBearings();
  void pickDistricts();
  void paintSquare(int x, int y, float r, District d);
  void squaresPass();
  void paintStreet(int x, int y, uint8_t kind, Ground g);
  void line(float x0, float y0, float x1, float y1, int width, uint8_t kind, Ground g);
  bool walkStreet(float x, float y, float ang, float target, int maxLen, int width, uint8_t kind, Ground g, int stop);
  void mainStreets();
  void ringRoads();
  void fabric();                     // east-west lanes a house-row apart (towns and cities)
  void lanes();
  void cityWall();                   // ring, gatehouses, side gates (port of the classic cityWallV3, plus side gates)
  void linkGates(size_t gaps0);      // lanes from each opening to the streets, stubs outside the ring removed
  void approachRoads();              // from every opening out to the buffer's edge along its bearing
  void palisadeRing();               // hill-fort villages: a fence ring with gaps where the streets leave
  void pruneStreets();               // streets the heart cannot reach go back to the land
  // M3 layout styles (town_layout.cpp)
  void pickStyle();                  // the culture's layout, wall, density, trees (in land, before anything is drawn)
  void wallRing();                   // walled: the ring as a thin octilinear polygon (ringT) and its inside
  void wallStreet();                 // cities: the lane along the inside of the wall
  std::vector<uint8_t> ringT;        // the wall ring's tiles (empty: the classic ring, inside tiles touching the outside)
  bool face(int x, int y) const {   // terraced: the row under a higher neighbour (a terrace's retaining face)
    if (!terraced || !in(x, y)) return false;
    static const int fx[4] = {0, 1, -1, 0}, fy[4] = {1, 0, 0, -1};
    for (int d = 0; d < 4; d++) if (in(x + fx[d], y + fy[d]) && lvl[I(x + fx[d], y + fy[d])] > lvl[I(x, y)]) return true;
    return false;
  }
  std::vector<uint8_t> ringNear;     // Chebyshev distance to the nearest ring tile, capped at 7 (wallRing)
  bool nearRing(int x, int y, int r) const {   // a ring tile within r (Chebyshev)
    if (ringNear.empty() || !in(x, y)) return false;
    return ringNear[I(x, y)] <= r;
  }
  void cutTerraces();                // Terraced: relief terraces rising to the back of the town (land)
  void marshWater();                 // Stilt: the marsh left standing in pools and channels (land)
  void pickSpine();                  // Linear: the spine's bearing (along the water, else the strongest road)
  void spineStreets();               // Linear: the spine through the heart, parallel back lanes, cross lanes
  void gridLanes(bool compound);     // Grid / Compound: a warped lattice of lanes (the forum, courtyard blocks)
  void radialRings();                // Radial: rings round the plaza and spokes out to the edge
  void contourLanes();               // Terraced: a lane along each terrace's front edge, stairs between them
  bool laneOk(int x, int y, bool ns, int px = -1, int py = -1) const;   // a lattice / ring lane may run here (not beside another street)
  int compoundHomes(int want);       // Compound: walled family courtyards along the lanes (town_build.cpp); homes made
  void quays();                      // Linear by the water: a paved quay along the bank (town_dress.cpp)

  // ---------------------------------------------------------------- town_build.cpp
  struct Want {
    art::Building b; Role r; int w, h; float near; bool req; bool north; District d; int px, py;
    int water = 0;   // (M1 economy) 1: beside a river (the watermill's wheel), 2: by the shore (placeByWater)
    uint8_t form = 0, civic = 0, seat = 0;   // (M3b) the builder's form, CIVIC_* bits, cult::Seat + 1
    bool face = false;   // (M3b) its door right on the main square's edge near (px, py) first (faceSquare)
  };
  std::vector<Want> lateWants;   // (M1 economy) the optional trades, built where the homes leave room
  bool placeWant(const Want& w); // services(): one want, by the water when it asks, with the fallback for required ones
  bool faceSquare(const Want& w);   // (M3b) a building whose door opens right onto the main square's edge, nearest (px, py)
  int putBldg(art::Building type, IRect r, Role owner, int storeys);
  bool fits(const IRect& r, art::Building type, int storeys) const;
  bool footpath(int ax, int ay, std::vector<std::pair<int, int>>& path, const IRect* own = nullptr) const;   // own: the house to be (not walked through)
  bool tryPlace(art::Building type, Role owner, int bw, int bh, int ax, int ay, bool required);
  bool placeBuilding(const Want& w);
  void homeShape(District d, art::Building& t, int& bw, int& bh);
  void services();
  void classicServices();            // the M1..M3 lists (no culture)
  void societyServices();            // (M3b) cult::requiredBuildings for the tier, the M1 economy beside them
  void spaces();                     // (M3b) cult::requiredSpaces: moot rings, corrals, parade grounds, groves, gardens...
  bool spaceRing(int x, int y, int r, art::Prop p, int every, bool keepSouth);   // a ring of props round (x, y)
  bool findOpen(int w, int h, float nearD, float farD, int& ox, int& oy, uint32_t k, District d = District::COUNT) const;   // an open patch
  int spacesMade = 0;                // (rpg_test --towns: spaces placed / asked)
  int spacesAsked = 0;
  void homes();                      // homesBegin, homesSweep until done, homesFinish (the generator's phases)
  void homesBegin();
  bool homesSweep();                 // a slice of the frontage sweep (false: done)
  void homesFinish();
  std::vector<std::pair<float, int>> hOrder;   // the sweep's street tiles, heart outward
  size_t hIdx = 0;
  int hPass = 0, hHave = 0;
  void placeCompound();              // choose the palace compound's place (before the streets)
  void buildCompound();              // its wall, gate, palace, barracks, courtyard and gardens
  // (M3b) the seat compound by the society's seat (cult::Seat): a castle's walled palace and gardens; a court palace's
  // walled courts and parterres; a jarl's great hall and longhouses in a stockade; the khan's great tent in a ring of
  // court tents, banners and horse lines; a temple precinct round its court; a council spire in its gardens; a tree
  // palace round the elder tree of its grove; the marsh elders' long hall over its pools on boardwalks. seatKind:
  // cult::Seat (Castle without a culture); palW / palH: the seat building's footprint
  int seatKind = 0;
  int palW = 15, palH = 7;
  bool compoundWalled() const;
  void compoundGrounds(int X0, int Y0, int CW, int CH, int gx, int gy, int terrace);
  void compoundApproach();           // the paved way from its gate to the town's streets (after pruneStreets)
  void kingdomColours(Bldg& b) const;

  // ---------------------------------------------------------------- town_dress.cpp
  bool freeTile(int x, int y) const;
  void centrepieces();
  void stallsAndLamps();
  void banners();
  void signposts();
  void yards();
  void gardens();                    // packed-earth yards, allotments, orchards and flower gardens
  void fields();
  void archetypeDress();
  void greenery();
  void folk();
  // ---------------------------------------------------------------- town_market.cpp (M1 economy)
  void pickSpecialty();              // spec from the plan, or from the archetype and the land
  void economyServices(std::vector<Want>& want);   // the production buildings for services()
  bool placeByWater(art::Building type, Role owner, int bw, int bh, bool sideWater);   // watermill, fishmonger
  bool walkable(int x, int y) const; // nothing solid stands here (buildings, walls, water, solid props)
  bool keepsWay(int x, int y) const; // (M1 fixer) something solid on (x, y) would cut nobody off (a local check: the
                                     // open tiles beside it still reach each other round it)
  bool putSolid(int x, int y, art::Prop p);   // setProp guarded by keepsWay when p is solid (false: nothing placed)
  void marketLots();                 // (M1 fixer) lots kept along the main street for a street market (noBuild bit 4)
  int lotStalls = 0;                 // the stalls those lots hold
  void markets();                    // the market rows on the main square (or along the main street), the keepers
  void squareDress();                // the other squares: benches, trees, a lamp
  void tradeYards();                 // the specialisation's yard props round its buildings and at the edge
  // (M2, owner note 6) no bare paved expanse: features go up in every empty paved block of 8 x 8 or more; a city's
  // main square gets its square-goers (idlers, shoppers, a crier, children)
  int largestEmptyPlaza(int& bx, int& by, const std::vector<uint8_t>& skip) const;
  // (M2 fixer round 3) the largest empty paved rectangle at least minSide each way (by area): long bare bands too
  int largestEmptyBand(int& bx, int& by, int& bw, int& bh, int minSide, const std::vector<uint8_t>& skip) const;
  void plazaFill();
  void squareFolk();
  void finish();
  // (M3 fixer) every door walkable from the heart: a quarter the river (or a marsh pool) cut off gets a way to the rest
  // of the town, over the water on a bridge (seed 41's river town had a square, a mill and a house on the far bank)
  void joinBanks();
  void addSpawn(Role r, int x, int y);
  // ---------------------------------------------------------------- town_lots.cpp (M7 Home, VISION_PLAN 8.2)
  // villages and towns: 1 to 3 fenced empty lots for sale on the outskirts, each beside a street that the heart reaches,
  // its gate on that street and a FOR SALE sign inside by the gate; a lot along a river or lake bank when the land has
  // one (PLOT_RIVERSIDE). Hash-driven (no draw from rng: the rest of the town is unchanged by the lots' choices).
  void lots();
};

}  // namespace town

// (M3c) the tree a settlement plants (street trees, greens, courts, orchards) in its biome: its own woods' kind where
// it has one (acacias on the savanna, palms at an oasis, birches in a birch wood, larches in the taiga, blossom in a
// blossom grove, silver trees in the silverwood), else the family's classic tree. h varies the pick.
inline art::Prop townTree(Biome bio, Eco eco, uint32_t h) {
  using art::Prop;
  if (bio == Biome::Snow) return Prop::PineTree;
  if (ecoFamily(eco) == bio)
    switch (eco) {
      case Eco::Savanna: case Eco::Scrubland: return Prop::AcaciaTree;
      case Eco::Oasis: return h % 4 ? Prop::PalmTree : Prop::AcaciaTree;
      case Eco::BirchWood: case Eco::LakeDistrict: return h % 4 ? Prop::BirchTree : Prop::OakTree;
      case Eco::Taiga: case Eco::TaigaBog: return h % 3 ? Prop::LarchTree : Prop::PineTree;
      case Eco::AlpineMeadow: return Prop::LarchTree;
      case Eco::BlossomGrove: return h % 5 ? Prop::BlossomTree : Prop::OakTree;
      case Eco::BambooForest: return h % 3 ? Prop::BlossomTree : Prop::BambooClump;
      case Eco::Jungle: return h % 2 ? Prop::PalmTree : Prop::JungleTree;
      case Eco::Silverwood: return h % 4 ? Prop::SilverTree : Prop::BirchTree;
      case Eco::Heath: case Eco::ChalkDowns: case Eco::Steppe: return h % 3 ? Prop::JuniperTree : Prop::OakTree;
      case Eco::ReedMarsh: case Eco::FloodedForest: case Eco::PeatBog: return Prop::WillowTree;
      case Eco::Mangrove: return Prop::PalmTree;
      case Eco::AutumnWood: return Prop::AutumnTree;
      default: break;
    }
  switch (bio) {
    case Biome::Desert: return Prop::PalmTree;
    case Biome::Taiga: return Prop::PineTree;
    case Biome::Autumn: return Prop::AutumnTree;
    default: return Prop::OakTree;
  }
}

}  // namespace ew
