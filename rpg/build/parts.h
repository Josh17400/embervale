// The BUILDER generator's parts for everything that is not a building (VISION_PLAN 15.14): fortifications (walls,
// gatehouses, towers, palisades, earthworks), roads (paving type, width, kerbs, bridges), fences and monuments
// (statues, obelisks, standing stones, shrines, wells, lamps, benches, market stalls' dress). Chosen from culture x
// purpose x wealth like the buildings, so a culture's wall, its gate, its bridges and its houses are one family, and
// the painters (rpg/art/art_walls.cpp, art_culture_props.cpp, art_wild.cpp, art_market.cpp, rpg/view/terrain.cpp)
// draw the parts, never a hard-wired European kit.
//
// Deterministic, integer maths only; never saved.
//
// Ownership (M3b): written by the lead in phase A (the defaults reproduce the M3 look exactly). The FORTIFICATIONS &
// GROUND lane owns this header and rpg/build/parts*.cpp and may APPEND fields / enum values.
#pragma once
#include <cstdint>
#include "rpg/art/art_building.h"
#include "rpg/culture/culture.h"

namespace bld {

// ---------------------------------------------------------------- fortifications
enum class GateForm : uint8_t {
  DrumTowers,   // the northern gatehouse: two round drum towers, a portcullis under a pointed arch (M2 / CityWall::Stone)
  SquareTowers, // square flanking towers under pyramid roofs, a round arch (highland, heartland variety)
  Pylons,       // two battered pylons under a corbelled arch (sun temples: CityWall::Talud)
  TimberGate,   // a timber gate tower over a log palisade, a walkway and a shingle hood (fjord, steppe forts)
  Iwan,         // a tall pointed iwan arch in a rectangular frame with stepped crenels and tile bands (dune)
  Paifang,      // a gate pavilion under a sweeping tiled roof, red columns (jade)
  MoonGate,     // a round opening in a white wall under a tiled coping (jade gardens, river towns)
  ElvenArch,    // a slender white pointed arch between two spired towers (starspire)
  LivingArch,   // two great trees trained into an arch, root buttresses (sylvan)
  Earthwork,    // a cut through a bank with a timber bridge-gate (ramparts)
  HedgeGap,     // a gap through a thorn hedge, a wicker gate (thorn walls)
  COUNT
};
enum class TowerForm : uint8_t {
  RoundDrum,    // a round tower, crenellated (M2)
  ConeDrum,     // a round tower under a cone (white stone, palisades)
  Square,       // a square tower, crenels or a pyramid roof
  Pagoda,       // a square tower with two tiled roofs (jade)
  Minaret,      // a slender round tower with a balcony and a small dome (dune)
  Bastion,      // a low round bastion (earthworks, adobe)
  Platform,     // a timber watch platform on posts (palisades, steppe)
  COUNT
};
enum class Coping : uint8_t { Merlons, SteppedMerlons, Rounded, TiledHood, Points, Hedge, COUNT };
struct FortParts {
  art::CityWall wall = art::CityWall::Stone;   // the wall material / cross-section (Map::wall = 1 + wall)
  GateForm gate = GateForm::DrumTowers;
  TowerForm tower = TowerForm::RoundDrum;
  Coping coping = Coping::Merlons;
  uint8_t height = 0;          // 0 the standard WALL_H; 1..3 taller (capitals)
  uint8_t finery = 0;          // 0..3: banners, painted bands, gilding (wealth / capital)
  uint32_t stone = 0, trim = 0, roof = 0, accent = 0;   // tints (0: the wall style's ramps)
  uint8_t culture = 0;         // (M3b forts) the culture (archetype + 1, 0 none) the parts were chosen for: the
                               // painters add its own finery (dragon heads, horse-tail standards, lanterns)
  uint8_t variant = 0;         // (M3b forts) per settlement: banner and finial details (never the joins)
};
// the walls of a settlement of this culture (urban 0..3; seed: the settlement's)
FortParts fortParts(const cult::Culture& c, int urban, uint32_t seed);

// ---------------------------------------------------------------- roads, paving and bridges
enum class PaveBond : uint8_t { Cobbles, Flags, Running, Herringbone, Basket, Crazy, Hex, Earth, Boards, COUNT };
enum class BridgeForm : uint8_t { Planks, StoneArch, Covered, MoonArch, Causeway, Living, COUNT };
struct RoadParts {
  uint8_t paving = 0;          // cult::TownStyle::paving (the material: 0 cobbles .. 9 red brick) for squares and streets
  PaveBond bond = PaveBond::Cobbles;   // how the units are laid (never a visible regular grid at 1x: owner)
  uint8_t width = 2;           // main street width in tiles (2..4)
  bool kerbs = false;          // kerb stones along paved streets
  BridgeForm bridge = BridgeForm::Planks;
  uint32_t tint = 0;           // paving tint (0: the material's ramp)
  PaveBond streetBond = PaveBond::Cobbles;   // (M3b forts) the streets' bond (bond: the squares')
};
RoadParts roadParts(const cult::Culture& c, int urban, uint32_t seed);

// ---------------------------------------------------------------- fences, monuments and street furniture
enum class MonumentForm : uint8_t { Statue, Obelisk, Totem, StandingStone, Stele, Cairn, Stupa, SacredTree, FireBowl,
                                    Fountain, COUNT };
struct MonumentParts {
  MonumentForm form = MonumentForm::Statue;
  uint8_t variant = 0;         // shape variant (a standing stone's height, lean, notch, carving; a statue's pose)
  uint8_t finery = 0;          // 0..3
  uint32_t stone = 0, metal = 0, accent = 0;
};
// a monument in this culture (kind: the purpose, e.g. the square's centrepiece (art::PropStyle::centre) or a
// wayside stone; seed: per placement, so a ring of standing stones never repeats one stone)
MonumentParts monumentParts(const cult::Culture& c, int kind, uint32_t seed);

// ---------------------------------------------------------------- (M3b forts lane) appended
// RoadParts::bond is the squares' bond; streets are laid in streetBond. The view's ground painter sees only the
// material (Map::PAVE_MARK carries TownStyle::paving), so the bonds are a pure function of the material: paveBond.
PaveBond paveBond(int material, bool street);
// the bridge a settlement of this paving builds over its streams (Ground::Bridge tiles carrying its PAVE_MARK)
BridgeForm bridgeOfPaving(int material);
// the gate forms and tower forms a wall material is built with (the painters join every form to every material;
// these are the ones a culture may choose: a moon gate in a jade wall, an iwan in mud brick, a hedge gap in thorn...).
// rpg_test (test_arch.cpp) holds every culture's choice to them; culture_gallery --check paints every form on every
// material and checks the joins pixel by pixel.
bool gateFitsWall(GateForm g, art::CityWall w);
bool towerFitsWall(TowerForm t, art::CityWall w);
// the identity of a FortParts (texture caches key on it; equal keys paint identical walls and gates)
uint64_t fortKey(const FortParts& f);
// the M3 parts of a bare wall material (no culture known: the classic island, tools): exactly the M3 look
FortParts fortDefaults(art::CityWall wall);
// a monument of a form for a culture (archetype + 1; 0 classic), varied by seed (per placement): stone, metal and
// accent tints from the PropStyle when given (0: the culture's defaults)
MonumentParts monumentOf(int culture, MonumentForm form, uint32_t seed, uint32_t stone = 0, uint32_t metal = 0);
// a standing stone in a culture's land (culture: archetype + 1, 0 none): weathered field stone, or the culture's own
// (runestones in the fjords, carved stelae in the sun temples' lands, white star-stones by the elves)
inline MonumentParts standingStoneParts(int culture, uint32_t seed) { return monumentOf(culture, MonumentForm::StandingStone, seed); }

}  // namespace bld
