// P2c forge styles (docs/P2C_CONTRACTS.md 3c; docs/PLAN.md "Forge styles"; docs/STYLE_P2.md 28): every material and alloy
// its own armour. A piece's FORGE STYLE (how it was made: shoulder, crest, cut, edge, collar, limb, fittings) is DERIVED
// from what the item already carries (its material, its alloy's name, recipe, sheen and rank, its maker culture): no item
// field, no save change, no RNG stream, no rule change; rig 1 never sees it (HumanLook::forge enters key() at rig 2 only).
// Pure integer functions (FORGE-DOLL). The value lists and the hero styles' table: rpg/art/doll/doll_forge.h.
#pragma once
#include <cstdint>
#include <string>
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

}  // namespace gear
