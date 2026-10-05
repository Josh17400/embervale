// The TOWNS lane's settlement generator (VISION_PLAN 2.5 L2a, 15.8), internal to rpg/world/town_*.cpp and
// rpg/world/settlement.cpp. One Gen builds one settlement on its local buffer (SettlementOut::buf):
//   town_layout.cpp  the land, the heart, the road bearings, squares, main streets, ring roads, lanes, city walls,
//                    gatehouses and side gates, the lanes that join every opening to the streets, approach roads
//   town_build.cpp   buildings: landmarks and services first, then homes by district (frontage plots with uneven
//                    setbacks and footpaths to the doors), the V5 sprite clearance on occupancy grids, the capital's
//                    palace compound (walled courtyard, gardens, fountain, palace, barracks, royal guard)
//   town_dress.cpp   centrepieces, market stalls, lamps, banners, signposts, yards and gardens, fields and pastures,
//                    archetype dressing (quays, mine carts, palisades), greenery, townsfolk and guards, the used mask
// Determinism: only the ctx inputs; one Rng seeded from the plan's seed; + - * / sqrt and ew::dsin/dcos/datan2 (no libm
// transcendentals), so MSVC, clang and Emscripten build the same town.
#pragma once
#include <cstdint>
#include <utility>
#include <vector>
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
  K_COMPOUND = 6   // the palace compound's grounds (nothing else is built there)
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
  int cx = 0, cy = 0;            // the heart (local)
  float rx = 1, ry = 1;          // the settlement's radii (1.0 = the footprint's edge)
  float wallR = 0.93f;           // the wall ring's normalised radius
  uint32_t bseed = 0, wseed = 0; // blob seeds (town outline, wall)
  Ground base = Ground::Grass, mainG = Ground::Road, laneG = Ground::Dirt;
  Biome bio = Biome::Plains;
  int layout = 0;                // villages: 0 crossroads hamlet, 1 road village, 2 village green
  int homesWant = 0;

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
  void palisadeRing();
  void pruneStreets();               // streets the heart cannot reach go back to the land               // hill-fort villages: a fence ring with gaps where the streets leave

  // ---------------------------------------------------------------- town_build.cpp
  struct Want { art::Building b; Role r; int w, h; float near; bool req; bool north; District d; int px, py; };
  int putBldg(art::Building type, IRect r, Role owner, int storeys);
  bool fits(const IRect& r, art::Building type, int storeys) const;
  bool footpath(int ax, int ay, std::vector<std::pair<int, int>>& path) const;
  bool tryPlace(art::Building type, Role owner, int bw, int bh, int ax, int ay, bool required);
  bool placeBuilding(const Want& w);
  void homeShape(District d, art::Building& t, int& bw, int& bh);
  void services();
  void homes();
  void placeCompound();              // choose the palace compound's place (before the streets)
  void buildCompound();              // its wall, gate, palace, barracks, courtyard and gardens
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
  void finish();
  void addSpawn(Role r, int x, int y);
};

}  // namespace town
}  // namespace ew
