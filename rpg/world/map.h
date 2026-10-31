// rpg/world/map.h: the tile map (Map) and the building helpers the generator and the sim share (bldgArch,
// bldgRequest, bldgBlueprint, the open fronts, the WORLDGEN_V7 storey and hearth decisions), plus the prop and name rules
// the generator calls. Moved out of rpg/sim/world.h by SOLID 2c so rpg/world includes nothing from rpg/sim; the
// functions declared here are still defined in rpg/sim (world.cpp, prop_rules.cpp: their FP group, never moved).
// Any layer from world/ up may include it. Neighbours: records.h (the plain records), rooms.h, rpg/sim/world.h (World).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "engine/mathx.h"   // Rng
#include "rpg/build/blueprint.h"
#include "rpg/core/art_ids.h"
#include "rpg/core/common.h"
#include "rpg/world/biomes.h"
#include "rpg/world/records.h"
#include "rpg/world/rooms.h"

// the name a building type goes by in the HUD ("INN", "THE KEEP", "THE PALACE"...)
const char* bldgTypeName(art::Building t);

// M0b: the exterior's style (what the view paints) and its art facts, so interiors can match their outside.
inline art::ArchStyle bldgArch(const Bldg& b) {
  if (b.styled) return b.arch;
  return art::withRoofTint(art::urbanize(art::archForBiome((int)b.biome, b.seed), b.urban, b.seed), b.roof);
}
inline art::BuildingFacts bldgFacts(const Bldg& b) {
  art::BuildingFacts f;
  f.storeys = b.storeys;
  f.hearth = b.hearth;
  f.banner = b.banner;
  f.banner2 = b.banner2;
  f.emblem = b.emblem;
  f.variant = b.variant;
  f.charred = b.charred;
  return f;
}
// M3b: the builder's request for a building, and its blueprint (rpg/build/blueprint.h). Every view, interior and test
// that needs a building's look or plan goes through these (no building bypasses the builder).
inline bld::Request bldgRequest(const Bldg& b) {
  bld::Request r = bld::simpleRequest(b.type, b.r.w, b.r.h, bldgArch(b), b.seed, bldgFacts(b));
  r.wealth = b.wealth;
  r.urban = b.urban;
  r.form = (bld::Form)(b.form < (uint8_t)bld::Form::COUNT ? b.form : 0);
  r.civic = b.civic;
  r.seat = b.seat;
  r.home = (uint8_t)(b.home == 1 || b.home == 3 ? 1 : 0);   // (M7) the player's own house (a bought one keeps its look)
  r.row = b.row;                                            // (M7b) a row house's party walls
  return r;
}
inline bld::Blueprint bldgBlueprint(const Bldg& b) { return bld::design(bldgRequest(b)); }
// (owner 2026-10-06) open fronts: the building's entry tiles and pillars (Bldg::open, made on demand from the blueprint
// and cached). bldgEntryAt: is tile (x, y) (the Bldg's map frame) one of its entry tiles (the door, or a walk-in bay of
// an open front). bldgPillarSolid: on an entry tile, does the point (px, py) in map pixels hit a pillar or the wall
// beyond the open span (the only parts of an open front that block). bldgEntryColumns: the entry columns, west to east.
const Bldg::Open& bldgOpenFront(const Bldg& b);
const Bldg::Open& bldgOpenFront(const Bldg& b, const bld::Blueprint& bp);
bool bldgEntryAt(const Bldg& b, int x, int y);
bool bldgPillarSolid(const Bldg& b, float px, float py);
std::vector<int> bldgEntryColumns(const Bldg& b);
// M3b: the seat of power, whatever the society builds it as (a castle keep, a great hall, a tent court, a temple
// complex, a guildhall, a council spire...): game code finds the ruler's and the lord's seats by these, never by type.
// The royal seat holds the king (Role::King) on its ground floor; a city's seat holds its lord (Role::Jarl).
inline bool bldgIsSeat(const Bldg& b) {
  return (b.civic & bld::CIVIC_SEAT) != 0 || b.type == art::Building::Palace || b.type == art::Building::Keep;
}
inline bool bldgIsRoyalSeat(const Bldg& b) {
  return b.type == art::Building::Palace || ((b.civic & bld::CIVIC_SEAT) != 0 && b.urban >= 3);
}
// WORLDGEN_V7 decisions (rpg/sim/world.cpp), deterministic from the type, the footprint and a hash:
int bldgStoreysV7(art::Building t, int wTiles, int hTiles, uint32_t h);
bool bldgHearthV7(art::Building t, int storeys, uint32_t h);

// A placed object on a tile (props layer). Buildings are separate.
struct Map : MapBits {   // the bit constants (PAVE_MARK, HEIGHT_*...): rpg/world/records.h
  std::vector<Spawn> spawns;
  MapKind kind = MapKind::Overworld;
  int w = 0, h = 0;
  uint32_t seed = 0;
  std::vector<uint8_t> ground;   // Ground
  std::vector<uint8_t> prop;     // art::Prop + 1, 0 = none
  std::vector<uint8_t> solid;    // 1 = blocks movement (props, buildings, walls)
  std::vector<uint8_t> wall;     // city wall pieces (1)
  std::vector<uint8_t> deco;     // per-tile decoration id (rpg/sim/deco.h; 0 = none): non-blocking clutter drawn on
                                 // the floor (stains, straw, scattered papers...). Regenerated with the map, never saved
  std::vector<int32_t> bldgAt;   // building index at tile or -1 (M1: 32-bit, an endless session's records may pass 32767)
  std::vector<uint8_t> biome;    // Biome (overworld only)
  // M2 biome transitions (VISION_PLAN 11.6), endless overworld only (else empty): bits 0-3 the neighbouring biome
  // (Biome) this tile blends toward, bits 4-7 its weight 0..15 (0: pure own biome, 8: half and half; never above 8, a
  // tile past the midpoint belongs to the other biome). The generator writes it (ChunkData::blend); the view dithers
  // the two biomes' ground and the generator ramps transition flora with it.
  // (M3 fixer) on a settlement's own tiles (never an ecotone: town ground keeps its own look) the byte is a paving mark
  // instead: PAVE_MARK | the culture's TownStyle::paving (0..15), or BOARDWALK_MARK | paving on a stilt town's
  // boardwalk (a Bridge over its marsh, not a river bridge). Ecotone readers take weights 1..8 only.
  // (M3b fixer) POOL_MARK | paving on the open water of a seat's grounds laid by hand (pools, canals, rills): the view
  // draws them in cut stone kerbs, never with a pond's organic banks.
  std::vector<uint8_t> blend;
  // M3c Wildlands (rpg/world/biomes.h), endless overworld only (else empty): eco = the biome proper (Eco; its family is
  // the biome byte), ecoNb = the Eco this tile blends toward (with blend's weight; == eco where the weight is 0 or the
  // byte is a PAVE / BOARDWALK / POOL mark). Written by the generator (ChunkData::eco / ecoNb), never saved.
  std::vector<uint8_t> eco, ecoNb;
  std::vector<uint8_t> height;   // M1: relief per tile (endless overworld only, else empty): bits 0..2 the level 0..7
                                 // (VISION_PLAN 11.1), HEIGHT_CLIFF a cliff face (not walkable), HEIGHT_RAMP a ramp or
                                 // stairs between levels (walkable). The view draws faces where the level steps down.
                                 // M11 High Ground (VISION_PLAN 11.2 / 11.3 / 15.32) adds three bits; every step bit
                                 // (CLIFF, RAMP, SLOPE) sits on the LOWER tile of a 4-neighbour level step:
                                 //   HEIGHT_SLOPE a gentle 1-level step (a hillside): walkable at reliefSpeed's 0.8,
                                 //                drawn as a shaded hillside band, never with CLIFF or RAMP
                                 //   HEIGHT_FALL  a water tile a river (or a plateau lake's spill) pours over a step
                                 //                into (always with CLIFF): an animated waterfall; a wall to walkers,
                                 //                swimmers and boats alike
                                 //   HEIGHT_STAIR with RAMP: a built way up (stone stairs: roads, tracks, towns, a summit's
                                 //                trail); a RAMP without it is a worn earth ramp
  // M7b Cities (rpg/world/urban.h): the urban fabric per tile (endless overworld and settlement buffers only, else
  // empty): the street class (avenue, street, lane, alley, sidewalk, gutter, square, close, park, garden, court, quay),
  // the upkeep and the quarter. Written by the settlement generator (cities; a town's main street and square), always
  // 0 in villages; the view draws kerbs, sidewalks, drains and the quarters' paving from it. Regenerated, never saved.
  std::vector<uint8_t> urb;
  std::vector<Bldg> bldgs;
  int exitX = 0, exitY = 0;      // caves/interiors: tile you appear on when entering (and where the exit is); upper
                                 // floors of a building have no exit (-1)
  std::vector<int16_t> exits;    // (owner 2026-10-06) a building walked into between pillars, its ground floor: the
                                 // ways out on row exitY, west to east (exitX among them); empty: the one door at exitX
  bool exitOpen = false;         // exitX is an open bay too (an open entrance); false: exitX is a door
  bool isExit(int x, int y) const {
    if (y != exitY || exitY < 0) return false;
    if (x == exitX) return true;
    for (int16_t e : exits) if (e == x) return true;
    return false;
  }
  // M0b interiors (WORLDGEN_V7+, rooms.h): which floor of its building this map is, its stairs, and its rooms
  int floor = 0;
  uint8_t kit = 0;               // (M3 fixer) interiors: 1 + the cult::Archetype whose own furniture the view draws
                                 // (art::cultureInteriorPiece), 0 the classic kit. Regenerated with the map, never saved
  Stairs up, down;               // the staircase up to floor+1 / down to floor-1 (x < 0: none)
  std::vector<RoomInfo> rooms;
  std::vector<int8_t> roomAt;    // per tile: index into rooms, -1 for walls (empty for older interiors)
  int roomIndexAt(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h || roomAt.empty()) ? -1 : roomAt[(size_t)y * w + x]; }
  Ground at(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h) ? Ground::Void : (Ground)ground[(size_t)y * w + x]; }
  int decoAt(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h || deco.empty()) ? 0 : deco[(size_t)y * w + x]; }
  int propAt(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h) ? 0 : prop[(size_t)y * w + x]; }
  void setG(int x, int y, Ground g) { if (in(x, y)) ground[(size_t)y * w + x] = (uint8_t)g; }
  void setP(int x, int y, int p) { if (in(x, y)) prop[(size_t)y * w + x] = (uint8_t)p; }
  void setProp(int x, int y, art::Prop p) { setP(x, y, (int)p + 1); }
  bool in(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
  bool blocked(int x, int y) const;
  // M11: the walking speed factor of a relief tile (VISION_PLAN 11.2): a hillside 0.8, an earth ramp 0.8, stone stairs
  // 0.9, anything else 1. The SIM lane applies it (the player, NPCs, companions, mounts); this is its one definition.
  static float reliefSpeed(uint8_t bits) {
    if (bits & HEIGHT_SLOPE) return 0.8f;
    if (bits & HEIGHT_RAMP) return (bits & HEIGHT_STAIR) ? 0.9f : 0.8f;
    return 1.0f;
  }
  int heightAt(int x, int y) const { return in(x, y) && !height.empty() ? (height[(size_t)y * w + x] & HEIGHT_LEVEL) : 0; }
  uint8_t heightBits(int x, int y) const { return in(x, y) && !height.empty() ? height[(size_t)y * w + x] : 0; }
  float reliefSpeedAt(int x, int y) const { return reliefSpeed(heightBits(x, y)); }
  bool fallAt(int x, int y) const { return (heightBits(x, y) & HEIGHT_FALL) != 0; }   // M11: a waterfall tile
  Biome biomeAt(int x, int y) const { return in(x, y) && !biome.empty() ? (Biome)biome[(size_t)y * w + x] : Biome::Plains; }
  uint8_t blendAt(int x, int y) const { return in(x, y) && !blend.empty() ? blend[(size_t)y * w + x] : 0; }
  uint8_t urbAt(int x, int y) const { return in(x, y) && !urb.empty() ? urb[(size_t)y * w + x] : 0; }   // M7b (rpg/world/urban.h)
  // M3c: the biome proper (a map without the layer: its family's classic eco) and the eco it blends toward
  Eco ecoAt(int x, int y) const { return in(x, y) && !eco.empty() ? (Eco)eco[(size_t)y * w + x] : ecoOfFamily(biomeAt(x, y)); }
  Eco ecoNbAt(int x, int y) const { return in(x, y) && !ecoNb.empty() ? (Eco)ecoNb[(size_t)y * w + x] : ecoAt(x, y); }
  void alloc(int w_, int h_, Ground fill);
  void rebuildSolid();
};

bool propSolid(art::Prop p);   // rpg/sim/prop_rules.cpp

// names
std::string makeTownName(Rng& r);
std::string makeDungeonName(Rng& r, SiteType t, Biome b = Biome::Plains);
std::string makePersonName(Rng& r, bool female);
