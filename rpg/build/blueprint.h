// The BUILDER generator (VISION_PLAN 15.14, M3b "Builders & Societies"): a construction grammar that assembles every
// building from parts chosen by culture x purpose x wealth. design() turns a Request (what the settlement asked for:
// purpose, footprint, the culture's style, wealth, form) into a Blueprint: the resolved form, the massing (volumes with
// storeys, roof kind, materials, doors, windows, ornament), the signage, the yard and the INTERIOR shape. The painter
// (art::buildingSprite(const bld::Blueprint&)) draws exactly the blueprint; the interior generator
// (rpg/sim/interior_v4.cpp) derives its floor plan from the same blueprint (a round yurt has a round interior), so the
// outside and the inside always agree (15.7).
//
// Rules:
//  - deterministic, integer maths only (it is generation: the blueprint decides interiors and footprints);
//  - the door contract stays: the front door is in the footprint's bottom row at tile column wTiles / 2
//    (Bldg::doorX), on the front wall of the body volume;
//  - a Blueprint is never saved: it is regenerated from the Bldg (bldgBlueprint, rpg/sim/world.h);
//  - every building in the world is designed here: the art API has no other building entry point (the "no bypass"
//    test, rpg_test --builder, enumerates every purpose x culture x wealth x form).
//
// Ownership (M3b): this header was written by the lead in phase A. The BUILDER lane owns it and rpg/build/builder*.cpp
// and may APPEND fields / enum values; the fields marked FROZEN are read by other lanes (interiors, towns) and keep
// their meaning.
#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "rpg/art/art_building.h"
#include "rpg/culture/style.h"

namespace bld {

// ---------------------------------------------------------------- forms (footprint and massing archetypes) FROZEN
// Auto: the builder picks from the purpose and the culture's parts (a steppe home is Round, a fjord hall Long...).
// The settlement may ask for a form when its layout depends on it (the seat's compound, a courtyard house plot).
enum class Form : uint8_t {
  Auto,
  Rect,        // one rectangular body (wings, porches and annexes may be added)
  Round,       // a round body: yurt, roundhouse, tower mill, mage tower, elven pod
  L,           // an L: the body and a wing at the front left or right (the missing corner is a yard)
  Courtyard,   // rooms round an open court (dune houses, imperial domus, caravanserai, jade siheyuan)
  Compound,    // several separate buildings inside one enclosure (khan's tent court, temple precinct, farm steading)
  Tower,       // a tall narrow mass of 3+ storeys (tower houses, spires, pagodas)
  Long,        // a long hall, door in the long side (longhouse, mead hall, stilt hall)
  Stepped,     // a stepped platform / pyramid with a shrine on top (sun temples, ziggurats)
  Tent,        // a tent of canvas or felt (nomads, camps, market pavilions)
  COUNT
};
const char* formName(Form f);

// Bldg::civic / Request::civic bits: the role a building plays in its settlement's society
enum : uint8_t {
  CIVIC_SEAT = 1,       // the seat of power (the ruler's in a capital, the lord's in a city)
  CIVIC_GATHERING = 2,  // the settlement's gathering place (cult::Society::gathering)
  CIVIC_INSTITUTION = 4,// an institution's house (guild, lodge, academy, exchange)
  CIVIC_SACRED = 8      // the chief temple
};

// ---------------------------------------------------------------- the request
struct Request {
  art::Building purpose = art::Building::House;
  int wTiles = 4, hTiles = 3;            // footprint (collision box) in tiles
  art::ArchStyle style;                  // the culture's style for this building (Bldg::arch / bldgArch): palette,
                                         // materials, roof, windows, doors, ornament, ArchStyle::culture
  art::BuildingFacts facts;              // storeys, hearth, banner, emblem, variant (Bldg facts)
  uint32_t seed = 0;                     // Bldg::seed
  uint8_t wealth = 1;                    // 0 poor .. 3 rich (size and finery within one culture)
  uint8_t urban = 0;                     // 0 village .. 3 capital
  Form form = Form::Auto;
  uint8_t civic = 0;                     // CIVIC_* bits
  uint8_t seat = 0;                      // cult::Seat + 1 for a CIVIC_SEAT building (0: none)
  uint64_t key() const;
};

// ---------------------------------------------------------------- the massing
enum class VolShape : uint8_t { Box, Round, Octagon, COUNT };
enum class VolRole : uint8_t {
  Body,        // the main body (storeys are counted on it; the front door is in it)
  Wing,        // a side or rear wing
  Tower,       // a tower / turret / minaret / spire base
  Porch,       // a porch, portico or colonnade before a door
  Annex,       // a lean-to, outshot, oven, forge bay
  Tier,        // an upper tier (pagoda storey, stepped platform level)
  Drum,        // a drum under a dome
  Chimney,     // a chimney stack (only with facts.hearth)
  Gate,        // a compound's gate / gatehouse
  Enclosure,   // a compound's wall, stockade, fence or hedge segment (low, no roof)
  Tent,        // a tent of a compound
  // ---- M3b BUILDER lane (appended)
  Platform,    // a flat-topped block with no roof: a plinth, terrace, stepped dais, boardwalk or a paved court
  Tree,        // a colossal living tree (the sylvan tree palace, a shrine's sacred tree): its trunk; the crown is
               // painted above it
  COUNT
};
// M3b BUILDER lane: what a volume's front wall shows (Volume::face)
enum class Face : uint8_t {
  Windows,     // a door (doorHere) and rows of windows, one per storey
  Shopfront,   // wide shop windows either side of the door on the ground floor, windows above
  ForgeBay,    // an open forge: a dark bay, the hearth's glow, an anvil
  BarnDoor,    // great braced barn doors, a loft door in the gable
  Slits,       // arrow slits under string courses (keeps, towers, kasbahs)
  Sacred,      // tall arched or pointed windows of coloured glass, a rose window over the door
  Colonnade,   // open: columns before a shaded recess (porticos, stoas, verandas of posts)
  Arcade,      // open: round or pointed arches on piers (exchanges, cloisters, caravanserais)
  Veranda,     // open: posts and a railing before a shaded recess (marsh, jade, sylvan galleries)
  Blank,       // no openings (enclosure walls, platforms, drums under domes)
  Gate,        // a great gateway: an arch with leaves or a dark passage (a compound's gate)
  Iwan,        // a tall pointed arch recessed in a rectangular frame (dune portals)
  Vents,       // slatted vents high up (wind-catchers, belfries without a bell)
  Arcane,      // a mage tower: pointed windows of violet glass between string courses
  Lattice,     // a lattice fence or screen of wood or felt rope (steppe court fences)
  COUNT
};
// M3b BUILDER lane: Volume::feat bits (details the painter adds to a volume)
enum : uint32_t {
  VF_CRENELS = 1u,       // merlons on a flat roof's parapet (stepped merlons in the sun temples' idiom)
  VF_JETTY = 2u,         // the upper floor jetties out over the ground floor
  VF_GAMBREL = 4u,       // a barn's broken roof: steep below, shallow above
  VF_STILTS = 8u,        // the volume stands on stilts (z0 is the deck height): posts, the shade beneath, a ladder at a door
  VF_BARNBOARDS = 16u,   // board-and-batten barn walls with white trim
  VF_POINTS = 32u,       // an enclosure of upright logs with sharpened tops (a stockade)
  VF_BANNERS = 64u,      // the ruler's banners hang on the front wall
  VF_FLAG = 128u,        // a flag flies from the top
  VF_CROWN = 256u,       // a felt roof with a crown ring (toono) at its top
  VF_ROPES = 512u,       // a tent's guy ropes and pegs
  VF_STEAM = 1024u,      // steam vents and their plumes (baths)
  VF_BELL = 2048u,       // a bell hangs in the belfry openings at the top of the wall
  VF_BALCONY = 4096u,    // a ring balcony / gallery round a tower near its top (minarets)
  VF_STEPS = 8192u,      // steps cut into a platform's front up to the door
  VF_STAIR = 16384u,     // the great stair up a stepped pyramid's front (x range: the door column +-7)
  VF_OCULI = 32768u,     // a dome studded with glass oculi (hammams)
  VF_DOOR = 65536u,      // a door painted at the middle of this volume's front although the entrance is elsewhere
  VF_BALCONYDOOR = 1u << 17,   // a timber balcony on the upper floor over the door
  VF_HOOD = 1u << 18,    // a little hood roof over the door
  VF_LAMPS = 1u << 19,   // lanterns either side of the door
  VF_CARVED = 1u << 20,  // carved bargeboards / a carved porch (fjords, marsh)
  VF_GARDEN = 1u << 21,  // a court laid out as a garden (trees, a pool) rather than paved
  VF_PENNANTS = 1u << 22,// strings of pennants / horse-tail standards (steppe court)
  VF_LANTERNS = 1u << 23 // paper lanterns hung under the eaves (jade, marsh)
};
struct Volume {
  VolShape shape = VolShape::Box;
  VolRole role = VolRole::Body;
  // footprint in pixels in the building's frame: x right, y down from the footprint's top-left tile corner (16 px per
  // tile). May overhang the footprint by up to art::BLDG_PAD_X at the sides and art::BLDG_PAD_B at the front (eaves,
  // porches); y1 is the front face. Round / Octagon: the shape inscribed in the box.
  int16_t x0 = 0, y0 = 0, x1 = 64, y1 = 48;
  int16_t z0 = 0;                        // base height px (a tower on a wall, an upper tier, a dome on its drum)
  int16_t wallH = 24;                    // wall height px
  uint8_t storeys = 1;
  art::RoofShape roof = art::RoofShape::Hip;
  art::RoofMat roofMat = art::RoofMat::Shingle;
  art::WallMat wall = art::WallMat::Timber;
  uint8_t pitch = 2;                     // 0 flat .. 4 very steep
  bool ridgeNS = false;                  // the ridge runs north-south (the gable faces the street)
  int8_t eave = 0;                       // extra eave overhang px
  art::WindowShape window = art::WindowShape::Square;
  art::DoorShape door = art::DoorShape::Plank;
  bool doorHere = false;                 // the front door is in this volume's front face (exactly one volume)
  uint16_t ornament = 0;                 // art::ORN_* bits for this volume
  uint8_t finery = 0;                    // 0..3: carving, gilding, glazing (from wealth)
  uint32_t wallTint = 0, roofTint = 0, trimTint = 0;   // 0: the blueprint style's
  // ---- M3b BUILDER lane (appended)
  Face face = Face::Windows;             // what the front wall shows
  uint32_t feat = 0;                     // VF_* bits
  uint8_t dormers = 0;                   // dormers on the front roof plane (0..2; pitched roofs with the ridge across)
  uint8_t gable = 0;                     // the street gable's outline (ridgeNS): 0 plain, 1 crow-stepped, 2 bell (curved),
                                         // 3 clipped (jerkinhead)
};

// ---------------------------------------------------------------- signs and yards
enum class Sign : uint8_t { None, Hanging, Board, Banner, Awning, Lantern, Carved, Totem, COUNT };
// M3b BUILDER lane: Signage::icon values (the painter draws each)
enum : uint8_t {
  ICON_NONE = 0, ICON_MUG, ICON_ANVIL, ICON_PURSE, ICON_LOAF, ICON_HAM, ICON_FISH, ICON_YARN, ICON_HIDE, ICON_INGOT, ICON_SHEAF,
  ICON_SAW, ICON_SACK, ICON_SCALES, ICON_KEY, ICON_STEAM, ICON_CUP, ICON_SHIELD, ICON_HORN, ICON_CIRCLE, ICON_STAR, ICON_COUNT
};
struct Signage {
  Sign kind = Sign::None;
  uint8_t icon = 0;                      // the trade's icon (ICON_*; 0: none)
};
enum class Yard : uint8_t { None, Fence, Wall, Hedge, Stockade, COUNT };

// ---------------------------------------------------------------- the interior shape (FROZEN: the INTERIORS lane reads it)
enum class Floorplan : uint8_t { Rect, Round, L, Courtyard, Long, Cross, COUNT };
struct InteriorShape {
  Floorplan plan = Floorplan::Rect;
  uint8_t lCorner = 0;                   // L: the missing corner (0 NW, 1 NE, 2 SW, 3 SE)
  uint8_t courtW = 0, courtH = 0;        // Courtyard: the open court as a share of the floor in 1/16ths (8 = half)
  uint8_t floors = 1;                    // interior floors (== the body's storeys; 15.7: stairs iff 2+)
  art::Furniture furniture = art::Furniture::Chairs;
  uint8_t culture = 0;                   // ArchStyle::culture (cult::Archetype + 1): the culture's own room plans
                                         // (inns are culture specific: a caravanserai, a mead hall, a tea house...)
};

// ---------------------------------------------------------------- the blueprint
struct Blueprint {
  Request req;                           // what was asked (FROZEN)
  Form form = Form::Rect;                // the resolved form (never Auto) (FROZEN)
  art::ArchStyle style;                  // the resolved style (req.style with the builder's choices applied)
  art::BuildingFacts facts;              // the resolved facts (storeys = the body's storeys)
  std::vector<Volume> vols;              // body first; back to front order is the painter's business
  Signage sign;
  Yard yard = Yard::None;
  InteriorShape interior;                // (FROZEN)
  int heightPx = 0;                      // estimated px from the footprint's bottom edge to the top (ground shadows)
  uint64_t key = 0;                      // identity: equal keys paint identical pixels and give identical interiors
  const Volume* body() const { return vols.empty() ? nullptr : &vols[0]; }
  // ---- M3b BUILDER lane (appended)
  uint8_t seat = 0;                      // the seat of power this building is (cult::Seat + 1; 0: none), resolved from the
                                         // request or, for a palace, the culture's own seat
  uint8_t culture = 0;                   // ArchStyle::culture (cult::Archetype + 1; 0: the biome stand-in)
  int doorVol() const { for (size_t i = 0; i < vols.size(); i++) if (vols[i].doorHere) return (int)i; return 0; }
};

// Design one building. Cheap (microseconds, no allocation beyond the volumes): callers may design on demand.
Blueprint design(const Request& r);
// A request from the classic inputs (tools, galleries, wayside huts): wealth 1, urban 0, Form::Auto
Request simpleRequest(art::Building purpose, int wTiles, int hTiles, const art::ArchStyle& style, uint32_t seed,
                      const art::BuildingFacts& facts = art::BuildingFacts{});
// tools: the classic plains-style building of the M0 tools (archForBiome(plains) tinted roofColor, 0 = none)
inline Request classicRequest(art::Building purpose, int wTiles, int hTiles, uint32_t roofColor, uint32_t seed) {
  return simpleRequest(purpose, wTiles, hTiles, art::withRoofTint(art::archForBiome(2, seed), roofColor), seed);
}
// checks a blueprint against the contract (non-empty, one body first, exactly one doorHere volume whose front holds
// the door column, volumes inside the sprite's pads, storeys 1..4, interior floors == body storeys); returns "" when
// valid, else what is wrong (rpg_test --builder, arch_gallery --check)
const char* validate(const Blueprint& b);

// ---------------------------------------------------------------- open fronts (owner, 2026-10-06)
// "There shouldn't be a door on an open building. If a building has pillars it's an open front, so you just walk on
// in." A volume whose face is open (a colonnade, an arcade, a veranda or porch of posts, an iwan's recess) never shows
// a door: the open front IS the entrance. Its pillars stand at the face's two ends and on the 16 px tile boundaries
// between them (a boundary too close to an end pillar is left out), so the bays between them line up with the
// footprint's tiles. When the entrance volume (doorHere) is open and stands on the ground, every tile of the
// footprint's front row that a walk-in bay (>= OPEN_MIN_BAY px clear) reaches is an entry: the world opens those tiles
// (Map::rebuildSolid), stepping into any of them enters the building (bldgEntryAt), and on them only the pillars and
// the wall beyond the open span block (bldgPillarSolid). A raised front (stilts, a high plinth) is entered by its
// ladder or its steps at the door column alone. The painter lays the pillars from openPillars (art_building.cpp
// openFace), so the art and the walking agree.
constexpr int OPEN_MIN_BAY = 12;   // px clear between two pillars for a person (9 px wide) to walk through with room to spare
struct Pillar { int16_t x0 = 0, x1 = 0; };   // px [x0, x1) in the building's frame
bool openFaceKind(Face f);                   // Colonnade, Arcade, Veranda, Iwan
// the pillars (piers, posts; an iwan's two jambs) of an open volume, left to right; empty for a closed face
void openPillars(const Volume& v, int doorX, std::vector<Pillar>& out);
struct OpenFront {
  int vol = -1;                    // the open entrance (the door volume, or a portico before the door column): no door
                                   // anywhere; -1: a closed front with a door (side galleries may still be walked into)
  Face face = Face::Windows;
  bool raised = false;             // the entrance on stilts or a plinth: entered by the ladder / steps at the door column
  std::vector<Pillar> pillars;     // the pillars of every open face walked through (left to right)
  uint32_t gaps = 0;               // bit i: footprint column i of the front row is a walk-in bay (an open entrance's
                                   // door column always is; a closed front's door column is its door, not a bit here)
  std::array<uint16_t, 32> solid{};   // per entry column: its px columns that block (bit i: px 16 col + i), the pillars
                                      // and the wall beyond the open face
  int bays = 0;                    // walk-in bays (each >= OPEN_MIN_BAY px clear)
  bool open() const { return vol >= 0; }
};
OpenFront openFront(const Blueprint& b);

}  // namespace bld
