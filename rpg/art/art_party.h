// M8 Fellowship art (VISION_PLAN 9, 15.6.2): the party's portraits, the bonded animal, the graves of the fallen.
// Shared contract, frozen after M8 phase A (lead, 2026-10-11): the ART lane is its only editor (it may ADD declarations
// and fields, never rename, remove or change the meaning of what is here) and paints it in rpg/art/art_party.cpp.
// Phase A: stand-ins (a crop of the standing sheet; a plain shaded beast; the carved NamedGrave prop).
// Same conventions as every sprite (rpg/art.h): 3/4 top-down, top-left light, a 1 px darkened outline, the cohesive
// palette, deterministic, transparent = 0, anchored bottom-centre unless said otherwise.
#pragma once
#include <cstdint>
#include "engine/pix.h"
#include "rpg/art/art_human.h"

namespace art {

// ---------------------------------------------------------------- party portraits (the HUD's party column, the radial)
// A head-and-shoulders bust of a look, facing the viewer a quarter turn, lit from the top left, on a transparent ground
// (the view frames it). mood: 0 calm, 1 hurt (pale, a cut), 2 downed (eyes shut, grey), 3 glad. The culture's headwear,
// hair, ears (the M3 peoples) and the worn helmet / armour collar show (the paper doll's pieces read at this size).
constexpr int PORTRAIT_W = 24, PORTRAIT_H = 24;
Canvas portraitSprite(const HumanLook& look, int mood);

// ---------------------------------------------------------------- the bonded animal (a hound or a wolf raised from a pup)
struct HoundLook {
  uint8_t species = 0;         // 0 a hound (a wolfhound's build), 1 a wolf
  uint8_t age = 2;             // 0 a pup, 1 half grown, 2 grown (the sheet's size within the same cell)
  uint32_t coat = 0;           // the main coat colour (0: the species' own)
  uint32_t coat2 = 0;          // the saddle / mask colour (0: the species' own)
  uint8_t collar = 0;          // 0 none, 1 leather, 2 studded (the player's mark)
  uint32_t seed = 0;           // markings
  uint64_t key() const;        // the view's texture cache key
};
// HOUND_FRAMES columns x 3 rows (0 facing down, 1 up, 2 right; flip for left) of HOUND_W x HOUND_H cells, anchored at the
// cell's bottom centre on the animal's feet. Columns: HF_IDLE (2), HF_RUN (4), HF_BITE (2), HF_DOWN (1: lying hurt),
// HF_SIT (1: sitting by its master).
constexpr int HOUND_W = 28, HOUND_H = 22;
constexpr int HF_IDLE = 0, HF_RUN = 2, HF_BITE = 6, HF_DOWN = 8, HF_SIT = 9, HOUND_FRAMES = 10;
Canvas houndSheet(const HoundLook& look);
Canvas houndPortrait(const HoundLook& look, int mood);   // PORTRAIT_W x PORTRAIT_H, as portraitSprite

// ---------------------------------------------------------------- graves of the fallen (and the belongings laid there)
// A companion's grave in their people's rite (a carved headstone, a cairn, a carved post, a standing stone, a pyre's
// marker stone...), with their weapon laid on it, fresh earth for the first days and flowers once some are laid.
struct GraveLook {
  uint8_t rite = 0;            // 0 headstone, 1 cairn, 2 carved post, 3 standing stone, 4 urn niche (GRAVE_RITES)
  uint32_t stone = 0;          // the stone / wood colour (0: the rite's own)
  uint8_t weapon = 0;          // HumanLook::weapon laid on it (0 none)
  bool fresh = false;          // fresh-turned earth (the first days)
  bool flowers = false;        // flowers laid
  uint32_t seed = 0;           // the carving, the stones' order
  uint64_t key() const;
};
constexpr int GRAVE_RITES = 5;
constexpr int GRAVE_W = 24, GRAVE_H = 32;   // anchored bottom-centre on the grave's tile (solid: one tile)
Canvas graveSprite(const GraveLook& look);

// ================================================================ ART lane, phase B (appended)
}  // namespace art
namespace cult { struct Culture; }
namespace art {
// The grave a culture gives its dead (VIEW: a companion's Fallen record -> its culture -> this): the rite by the faith's
// burial custom and the people's archetype (a carved headstone in the heartlands, a cairn in the highlands and the
// fjords, a carved post on the steppe and in the marshes, a standing stone by the sea, an urn niche where the dead are
// burned), the stone or wood by the land's building stone; seed (the Fallen's epitaph seed) picks the carving. weapon,
// fresh and flowers stay 0 / false: the view sets them from the grave record (belongings, days since, flowers laid).
// c == nullptr: a plain headstone.
GraveLook graveLookFor(const cult::Culture* c, uint32_t seed);
// Kneel has two variants (art_life.h): humanPostureSheet(look, Posture::Kneel, 1) is the mourner at a grave.
constexpr uint8_t KNEEL_BIND = 0, KNEEL_MOURN = 1;
// tests and galleries: the same sprites before their 1 px outline (party_gallery --check)
Canvas portraitRaw(const HumanLook& look, int mood);
Canvas houndSheetRaw(const HoundLook& look);
Canvas graveRaw(const GraveLook& look);

}  // namespace art
