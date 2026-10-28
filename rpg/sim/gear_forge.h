// P2c forge styles (docs/P2C_CONTRACTS.md 3c; docs/PLAN.md "Forge styles"; docs/STYLE_P2.md 28): every material and alloy
// its own armour. A piece's FORGE STYLE (how it was made: shoulder, crest, cut, edge, collar, limb, fittings) is DERIVED
// from what the item already carries (its material, its alloy's name, recipe, sheen and rank, its maker culture): no item
// field, no save change, no RNG stream, no rule change; rig 1 never sees it (HumanLook::forge enters key() at rig 2 only).
// Pure integer functions (FORGE-DOLL). The value lists and the hero styles' table: rpg/art/doll/doll_forge.h.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/items.h"

namespace gear {

struct ForgeStyle { uint8_t base = 0, shoulder = 0, crest = 0, cut = 0, edge = 0, collar = 0, limb = 0, fit = 0; };
uint64_t packForge(const ForgeStyle& s);
ForgeStyle unpackForge(uint64_t p);
// a material's hero style (STYLE_P2 28.4): metalKind (HumanLook codes: 1 leather 2 bronze 3 iron 4 steel 5 gilded 6 jade
// 7 obsidian 8 ember 9 starmetal 11 mythic; 0 cloth) on a form (its cut mapped onto the form's family); base 0 and all axes
// 0 for a metalKind with no hero style (10 alloy: alloyStyle)
ForgeStyle baseStyle(uint8_t metalKind, cult::BodyArm form);
// a culture alloy's style by the pick rule (contracts 3c): the people's vocabulary as the menu; scores from the metal word,
// the sheen, the rank, the recipe's reagents and the people's ornament; a seed from the name and the recipe; the forge
// school; distances (7 inside the culture with two silhouette axes apart, 4 to every hero style, 5 to the metal word's),
// a walk of at most 64 steps. alloyIndex 0-based. Its cut is on the alloy's own armour family (familyOf below).
ForgeStyle alloyStyle(const cult::Culture& c, int alloyIndex);
// what alloyStyle measured (the gates: C16b and C16d report it)
struct ForgePick {
  ForgeStyle style;
  int steps = 0;                 // walk steps taken
  bool ok = true;                // every constraint held
  int minCulture = 99, minHero = 99, wordHero = 99;   // the distances found (99: none to compare)
  std::string why;               // the first constraint that failed ("" ok)
};
ForgePick alloyPick(const cult::Culture& c, int alloyIndex);
// a worn piece's style: a Mythic metal piece the MYTHIC; an alloy its maker's alloyStyle; else its material's baseStyle
// (cloth padded: HOMESPUN). maker may be null (the heartland look). base 0 + no axes: no style (a piece with no armour look)
ForgeStyle forgeOf(const Item& it, const cult::Culture* maker);
int styleDistance(const ForgeStyle& a, const ForgeStyle& b);     // shoulder 3, cut 3, crest 2, edge 2, collar 1, limb 1, fit 1
int silhouetteDiffs(const ForgeStyle& a, const ForgeStyle& b);   // shoulder, crest, edge, collar that differ (0..4)
// the forge school: the family's culture cell on a 3 x 3 tiling (posmod(ci, 3) + 3 posmod(cj, 3)); a test culture
// (Atlas::make: no cell) seed % 9
int forgeSchool(const cult::Culture& c);
// the cell's checkerboard parity ((ci + cj) & 1; a test culture (seed / 9) & 1): the school's pick on an axis with two
// candidates (every neighbour flips it)
int forgeParity(const cult::Culture& c);
// (forge i2) the cell's column parity (ci & 1; a test culture (seed / 18) & 1): with the parity, every neighbour (orthogonal
// or diagonal) differs in one of the two, so the crest (parity) or the shoulder (column) leads apart
int forgeColumn(const cult::Culture& c);
const char* forgeValueName(int axis, int v);   // axis 0 base (0-based), 1 shoulder .. 7 fittings (1-based; "" none)
// the armour family (art::doll::forge::Family) a culture's alloy armour takes (gear_look bodyFormOf for a Mat::Alloy body)
int alloyFamilyOf(const cult::Culture& c);
// "foundry: cop comb muscled strips torc scaled bosses" (the boards and --expand-looks)
std::string describeForge(const ForgeStyle& s);
// the hero style of an alloy's metal word (0 BRONZE 1 BRASS 2 IRON 3 STEEL 4 SILVER 5 GLASS -> foundry foundry wrought
// armourer parade knapped)
int wordHeroBase(int metalWord);
// (forge i3) a culture alloy's guise (art::doll::forge::Guise: helm, skirt, cloak, shoulder size), packed for
// HumanLook::forgeGuise (0: none). The helm by the crest's affinity, rotated by the alloy's name, the everyday alloy on the
// people's own helm where it is drawn; a culture's alloys never share a helm; the skirt by the edge pick, the shoulders and
// the cloak by the rank (the masterwork cloaked, its shoulders large)
uint16_t alloyGuise(const cult::Culture& c, int alloyIndex);

// ======================================================================== (forge round 2) the silhouette (contracts 3c.1)
// Six axes read at 1x with the colour off (art::doll::forge SX_*; 1-based values, 0 unset): the helm family, the skirt, the
// shoulder mass, the over-layer, the arm bulk and the chest motif. A culture alloy's KEY (helm, skirt, mass, layer) comes
// from the solved shape table (rpg/sim/gear_forge_shapes.inc, written by rpg_test --forge-solve), its arm and motif (and
// ForgeStyle's shoulder, crest and collar) from the alloy's own name and data (the details). Pure, integer, no RNG stream.
struct Shape { uint8_t helm = 0, skirt = 0, mass = 0, layer = 0, arm = 0, motif = 0;
               uint8_t variant = 0; };   // (P2c envelope, D51) the helm family's variant (0 the base drawing; forgeShape bits 20-22)
uint32_t packShape(const Shape& s);   // HumanLook::forgeShape's packing (0 for an all-zero shape)
Shape unpackShape(uint32_t p);
uint32_t shapeKey(const Shape& s);    // helm | skirt << 8 | mass << 16 | layer << 24: the uniqueness key (arm, motif aside)
Shape shapeOfKey(uint32_t key);       // the key's four axes (arm, motif 0)
int keyAxesApart(const Shape& a, const Shape& b);   // of helm, skirt, mass, layer (0..4)
// the forge school on the 6 x 6 tiling: posmod(ci, 6) + 6 posmod(cj, 6) for a lattice culture (a dialect its family's
// cell); a kit culture (Atlas::make, gear::kitCulture) ((seed - 1) * 12 + archetype + 6 * ((seed - 1) / 3)) mod 36: the 12
// peoples at seeds 1..3 take 36 different schools (contracts 3c.1), and a later seed's people another people's school
int forgeSchool6(const cult::Culture& c);
int shapeRank(int alloyIndex);        // 0 everyday, 1 warriors', 2 masterwork (index 2 and up)
// a material's silhouette (STYLE_P2 28.4's table; base art::doll::forge::B_*); people (may be null: the Heartland kit):
// outside the Heartland TANNER'S..ARMOURER'S take the people's own helm and its body form's hang, and every hero helm over a
// people's signature helm form (horned, conical, masked, winged) keeps the people's
Shape heroShape(int base, const cult::Culture* people);
// the solved table's key for a people (cult::Archetype), school (0..35) and rank (0..2); 0 when the table is empty
uint32_t tableKey(int people, int school, int rank);
// a culture alloy's silhouette: the table's key for its people (an isolated or hidden family its first archetype's row),
// school and rank, plus the details' arm and motif
Shape alloyShape(const cult::Culture& c, int alloyIndex);
// a worn piece's silhouette: a Mythic metal piece the MYTHIC's, an alloy its maker's alloyShape, else its material's
// heroShape (cloth padded HOMESPUN); all 0 for a piece with no armour look
Shape shapeOf(const Item& it, const cult::Culture* maker);
// "kabuto split drop sash | wide bib" ("none" for an unset shape)
std::string describeShape(const Shape& s);
// the details (contracts 3c.1): the name's root word (kRootDetails), the hidden word, then round 1's scores; the shoulder's
// shape inside the key's mass, the crest on a socketed family, the collar; cut, edge, limb and fittings 0
struct AlloyDetails { uint8_t arm = 0, motif = 0, shoulder = 0, crest = 0, collar = 0; std::string root;
                      uint8_t variant = 0; };   // (P2c envelope) the helm's variant (the owner's 37: kBoardHeads)
// (P2c envelope, contracts 3d, D51) the head key: helm | variant << 5 | crest << 8 (a crest only on a socketed family)
uint32_t headKey(const Shape& s, int crest);
// the owner's 37 board alloys' heads: the helm family as shipped at df2a155 (pinned in the solve), its variant and crest
struct BoardHead { const char* name; uint8_t helm, variant, crest; uint32_t key; };   // key: the whole shape key at df2a155 (pinned)
const BoardHead* boardHeads(int* n);
const BoardHead* boardHead(const std::string& alloyName);   // null: not on the owner's board
// (P2c envelope) an alloy's head key as the doll draws it (table helm + details' variant and crest); the variant rule can be
// swapped for the stability selftest (legacy: the fix round's mass-keyed variant, which C16k's stability test must FAIL)
uint32_t alloyHeadKey(const cult::Culture& c, int alloyIndex);
void setLegacyVariantRule(bool on);
// (P2c envelope, contracts 3d; A1-A3) the shape table's metal rule: a bronze or brass everyday alloy (rank 0) never pairs
// mass NONE with layer TABARD, nor a fur MANTLE with layer TABARD or HALFCAPE
bool metalRuleBreaks(const Shape& s);
bool warmEverydayCell(int people, int school);   // some culture (seeds 1..64, the board set) has a bronze or brass rank-0 alloy here
AlloyDetails alloyDetails(const cult::Culture& c, int alloyIndex, const Shape& key);
// the root word an alloy's name carries ("" none): its prefix before the metal word, when it is one of STYLE_P2 28.5's roots
std::string alloyRoot(const std::string& name);

// the board set (contracts 3c.1; STYLE_P2 28.10): the 12 peoples' kit cultures at seeds 1..3 (owner: the owner's round-1
// board, seed 1 and the Marsh and Jade seed 2) and the owner's Jade seed 5; and the owner's round-1 twin groups (names)
struct BoardCulture { int people = 0; uint32_t seed = 1; bool owner = false; };
std::vector<BoardCulture> forgeBoardSet();
const char* const* forgeTwinGroups(int* n);

// ---------------------------------------------------------------- the solver (rpg_test --forge-solve, --forge-shapes)
// The outline model the rules read: a body OD between two keys on a people's T4 man kit (skirt, mass, layer: the arm and
// motif aside), the same across two peoples' kits (the board set's twins), a head OD between two families, and a body OD
// between a key and one of the people's material styles. The design estimates (art::doll::forge estBodyOD / estHeadOD) when
// rpg_test has no pool to measure.
struct ShapeModel {
  std::function<double(int people, const Shape& a, const Shape& b)> body;
  std::function<double(int peopleA, const Shape& a, int peopleB, const Shape& b)> bodyX;
  std::function<double(int helmA, int helmB)> head;
  std::function<double(int people, const Shape& a, int base)> material;
  // (optional) a board-set pair on the real figures: cell (people, school, rank) wearing key a against cell (q, t, rr) wearing b
  std::function<double(int p, int s, int r, const Shape& a, int q, int t, int rr, const Shape& b)> boardBody;
  // (optional, INTEGRATE silhouettes i1) pairs measured as twins on the real figures (C18a) after an earlier solve: true holds
  std::function<bool(int peopleA, const Shape& a, int peopleB, const Shape& b)> knownTwin;
  bool measured = false;
  // (optional, silhouettes i2) a table to start from (the solver's passes then move only what the rules and the known twins
  // ask: the measured rounds converge instead of starting over); nullptr starts empty
  const uint32_t (*start)[36][3] = nullptr;
};
ShapeModel estimateModel();
// a people's material keys (heroShape of every base on that people's kit cultures, seeds 1..8: its helm forms and body forms)
std::vector<std::pair<int, Shape>> materialKeys(int people);
struct ShapeSolve {
  uint32_t table[12][36][3] = {};
  int hardFails = 0, twins = 0, twinsBoard = 0, twinsAdjacent = 0;
  std::vector<std::string> log;   // the hard failures (people and school) and the soft summary
};
ShapeSolve solveShapes(const ShapeModel& m);
// rules 1-4 (contracts 3c.1) on a table: one line per failure
std::vector<std::string> checkShapeTable(const uint32_t (*t)[36][3], const ShapeModel& m);
const uint32_t (*shapeTable())[36][3];   // the compiled table (gear_forge_shapes.inc)
// (INTEGRATE silhouettes i1, test only) tableKey and shapeTable read this table instead while set (nullptr: the compiled one):
// rpg_test --forge-solve measures a candidate table's real figures (C18a) before writing it
void setShapeTableOverride(const uint32_t (*t)[36][3]);
uint64_t shapeTablePoolHash();           // the pool hash the compiled table was solved on (0: design estimates)
// the .inc text for a table (deterministic)
std::string shapeTableInc(const ShapeSolve& s, uint64_t poolHash, const std::string& how);

}  // namespace gear
