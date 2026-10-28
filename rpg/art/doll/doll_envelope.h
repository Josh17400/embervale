// EMBERVALE art: the tier envelope (P2c envelope, FORGE-DOLL; docs/P2C_CONTRACTS.md 3d, docs/PLAN.md "The envelope",
// docs/STYLE_P2.md 29). The owner's 32 px lock (2026-10-27): the body never grows; better gear takes more room around it,
// tier by tier, inside these caps. Pure constexpr, integer, no RNG; read at rig 2 only. Every gate that uses a number reads
// it from here (C12f, C3b, W8, B10 by tier, C13's drawn motion, C8's pool), and `rpg_test --envelope` prints the table.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

namespace art {
namespace doll {

struct TierEnvelope {
  uint8_t shoulders;             // px across facing D: 16 + the pauldrons' reach past the shoulder line, both sides (0: no cap)
  uint8_t rows, ornRows;         // head: every helm-layer pixel / a drawn crest or plume (plume slot), rows above the crown
  uint8_t side, brim, flare;     // columns past the 16 px head box: the shell / a drawn brim / horns, wings, flares, brims, tails
  uint8_t pauldron;              // the resolver's size clamp: 0 none, 2 small, 3 medium, 4 large, 5 great, 6 legend (0 row 0: the dial)
  uint8_t cloak;                 // 0 none, 1 the people's cape, 2 a cape or the travel cloak, 3 the long cloak
  uint8_t cloakSideD, cloakBack; // px past each leg facing D (cap); columns past the body's box behind facing R / L (W8)
  uint8_t skirtFlare, stir;      // extra px past the hips; the cloak's breeze swing in px
  uint8_t movedMin, movedMax;    // drawn motion px a figure-frame (C13; looks with a cloak or a plume)
  uint16_t area;                 // x100 of the line's T1 (C12f); 0: no ladder
  uint16_t loco, run, drawn, action, death, posture;   // B10 KB; 0 = measured on e6 (T5-T6 action, death, posture)
};
constexpr TierEnvelope kEnvelope[7] = {
  // shld rows orn side brim flare paul cloak sideD back sflare stir mvMin mvMax area loco run drawn action death posture
  {  0,  4,  6,  1,  1,  3,  0,  3,  0,  2,  0,  0,  0,  0,   0, 128,  64, 256, 512, 52, 64 },  // 0 untiered: today's rules
  { 16,  2,  2,  0,  1,  3,  0,  0,  0,  2,  0,  0,  0,  0, 100, 128,  64, 256, 512, 52, 64 },  // T1
  { 18,  3,  6,  0,  1,  3,  2,  0,  0,  2,  0,  0,  0,  0, 110, 128,  64, 256, 512, 52, 64 },  // T2
  { 20,  4,  6,  1,  1,  3,  3,  1,  0,  2,  0,  0,  0,  0, 120, 128,  64, 256, 512, 52, 64 },  // T3
  { 22,  6,  6,  1,  1,  3,  4,  2,  0,  2,  0,  0,  0,  0, 135, 160,  80, 320, 512, 52, 64 },  // T4
  { 26,  8,  8,  2,  2,  3,  5,  3,  2,  8,  1,  1,  8, 32, 155, 200, 100, 400,   0,  0,  0 },  // T5
  { 28, 10, 10,  2,  2,  3,  6,  3,  2,  8,  1,  2, 16, 48, 170, 200, 100, 400,   0,  0,  0 },  // T6
};
constexpr int kAreaStep = 5;                       // each tier's area >= the tier below's + 5 (x100 of T1)
constexpr uint32_t kPoolBudget = 2621440;          // 2.5 MiB (D46); pxc.mjs prints it
constexpr int kMeasuredCap[3] = {716, 72, 89};     // T5-T6 ACTION, DEATH, POSTURE KB: e6 measured x 1.1, never above these
constexpr double kHeadOdStep = 0.10;               // consecutive heads of a line (C12f) and the 37's heads (C16k): planned
constexpr int kHeadAreaStepPct = 5;                // T3..T6: each head's area >= the tier below's + 5 %
// (envelope fix round, D57) the T3 guard (eg3, the fair test's T3d in gear): its area x100 of the hero's T1 (the fair test's
// own caption, "guard(d) 1.20-1.21") and of the watch at his own tier (eg0: the envelope's T2-to-T3 step); its shoulders;
// its cape's rows beside the arms facing D and its columns behind the body facing R
constexpr int kGuardOverHeroT1 = 120;
constexpr int kGuardOverOwnTier = 110;
constexpr int kGuardShoulders = 20;
constexpr int kGuardCapeRowsD = 6;
constexpr int kGuardCapeBackR = 1;

inline const TierEnvelope& envelopeOf(int tier) { return kEnvelope[tier >= 0 && tier <= 6 ? tier : 0]; }
// B10's KB for a figure whose highest piece is `tier` (atlas: 0 LOCO 1 RUN 2 DRAWN 3 ACTION 4 DEATH 5 POSTURE); a 0 left in
// the T5-T6 rows reads the cap (kMeasuredCap)
inline int envelopeAtlasKB(int tier, int atlas) {
  const TierEnvelope& e = envelopeOf(tier);
  const uint16_t v[6] = {e.loco, e.run, e.drawn, e.action, e.death, e.posture};
  if (atlas < 0 || atlas > 5) return 0;
  if (v[atlas]) return v[atlas];
  return atlas >= 3 ? kMeasuredCap[atlas - 3] : 0;
}
// the head's rows cap for a helm of `tier`: every helm-layer pixel, and a drawn crest or plume (ornament: the plume slot)
inline int envelopeHeadRows(int tier, bool ornament) { const TierEnvelope& e = envelopeOf(tier); return ornament ? e.ornRows : e.rows; }

// rpg_test --envelope: the table as PLAN "The envelope" prints it (the judge compares the two)
inline std::string describeEnvelope() {
  static const char* const kCloak[4] = {"none", "the people's cape", "a cape or the travel cloak", "the long cloak"};
  std::string s;
  char b[400];
  s += "the tier envelope (art::doll::kEnvelope; docs/PLAN.md \"The envelope\"; a man facing D, the woman one row lower)\n";
  s += "tier | shoulders | head rows (orn) | head cols shell/brim/flare | pauldron | cloak (sideD, back) | flare stir | moved | area | LOCO RUN DRAWN ACTION DEATH POSTURE KB\n";
  for (int t = 0; t <= 6; t++) {
    const TierEnvelope& e = kEnvelope[t];
    char tn[8];
    std::snprintf(tn, sizeof tn, t ? "T%d" : "--", t);
    std::snprintf(b, sizeof b, "%-4s | %9d | %4d (%d) F-%d | %d / %d / %d | %8d | %d %s (%d, %d) | %d %d | %d-%d | %4.2f | %d %d %d %d %d %d%s\n", tn,
                  e.shoulders, e.rows, e.ornRows, 31 + e.rows, e.side, e.brim, e.flare, e.pauldron, e.cloak, kCloak[e.cloak & 3],
                  e.cloakSideD, e.cloakBack, e.skirtFlare, e.stir, e.movedMin, e.movedMax, e.area / 100.0,
                  envelopeAtlasKB(t, 0), envelopeAtlasKB(t, 1), envelopeAtlasKB(t, 2), envelopeAtlasKB(t, 3), envelopeAtlasKB(t, 4),
                  envelopeAtlasKB(t, 5), (!e.action && t) ? " (ACTION DEATH POSTURE: the cap until e6 is measured)" : "");
    s += b;
  }
  std::snprintf(b, sizeof b, "area step %d (x100 of T1); head OD step %.2f; head area step %d %%; pool %u B (2.5 MiB)\n", kAreaStep,
                kHeadOdStep, kHeadAreaStepPct, (unsigned)kPoolBudget);
  s += b;
  return s;
}

}  // namespace doll
}  // namespace art
