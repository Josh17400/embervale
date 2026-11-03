// rpg/core/fingerprint.h (P5): the peoples' fingerprints on armour, one row per maker people (docs/FIX_BATCH_P5.md 3,
// STYLE_P2 30). Data only (MARKS tunes the values within FIX_BATCH_P5 3.5); the doll's resolver and the icon painter read
// it, keyed by a piece's maker people (cult::Archetype + 1; 0 none), so the worn piece and its icon agree by construction.
// Core: every layer may include it; it includes only <cstdint>.
//
// The dye is the thread and binding colour on a piece's rims, hems, laces and seams; dye2 the second colour of a two-tone
// band (0: a solid band). The band runs along canvas x + y: `run` px of the dye, then dye2 up to `period`. The motif keys
// the mark tile (`finish.mark`, T1-T4: stitched, then engraved) and the filigree (`finish.filigree`, T5-T6: gold).
// Separation (checked by `python tools/doll/p5_marks.py --table` and rpg_test --fingerprint F1): primaries >= dE 25 apart,
// two-tone signatures >= 25, every primary >= 20 from each step 1-3 of the leather, bronze, iron, steel, gilded and brass
// ramps, every dye2 >= 20 from its own dye.
#pragma once
#include <cstdint>

namespace fp {
constexpr int kPeoples = 13;   // 0 none (culture 0: no fingerprint), 1..12 cult::Archetype + 1
struct Row {
  const char* name;     // "fjordfolk" ... (the doll's people-<name>); "none"
  const char* motif;    // the mark's and the filigree's key prefix: "knot" "plaid" "oak" "laurel" "arab" "horse" ...
  uint32_t dye, dye2;   // 0xRRGGBB; dye2 0: a solid band
  uint8_t run, period;  // the band along canvas x + y: run px of dye, then dye2 up to period
};
// clang-format off
inline constexpr Row kRows[kPeoples] = {
    {"none",      "oak",    0x000000, 0x000000, 0, 0},   // culture 0: the classic pieces, no fingerprint (oak: the classic filigree)
    {"fjordfolk", "knot",   0x3C6EBE, 0xE8E2CE, 3, 4},   // woad blue, bleached wool; knotwork
    {"highland",  "plaid",  0x1E683A, 0xAC2430, 2, 4},   // tartan green, red (a check); the tartan sett (new, D74's hem-plaid sett)
    {"heartland", "oak",    0xC42828, 0xEAE6D8, 4, 5},   // heraldic red, white; oak leaves
    {"imperial",  "laurel", 0x641E6E, 0xE2B840, 2, 3},   // purple, gold thread; laurel
    {"dune",      "arab",   0x243484, 0xECA61E, 2, 4},   // indigo, saffron; arabesque
    {"steppe",    "horse",  0x3CACD6, 0xC43028, 2, 3},   // sky blue, horse-hair red; running horses
    {"marsh",     "reed",   0x687E24, 0x465078, 4, 5},   // moss, faded indigo; reeds
    {"jade",      "clouds", 0xF06E1E, 0x1C1A1E, 2, 3},   // vermilion, black lacquer; cloud scrolls
    {"river",     "wave",   0x12868C, 0xECEAE2, 2, 4},   // river teal, white; waves
    {"suntemple", "ray",    0xB41E64, 0x249688, 2, 4},   // magenta, teal; sun rays
    {"sylvan",    "vein",   0xA0C850, 0xE8B040, 4, 5},   // leaf green, amber; leaf veins
    {"starspire", "star",   0x9678F0, 0xDCE4F8, 3, 4},   // violet, star white; four-point stars
};
// clang-format on
inline const Row& row(int people) { return kRows[people > 0 && people < kPeoples ? people : 0]; }
}  // namespace fp
