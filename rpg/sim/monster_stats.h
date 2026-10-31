// Monster numbers and names: mstat (the stat table), heavyWindup and monsterName (SOLID 2a.4 SIM-SEAMS; moved verbatim
// from rpg/sim/game_internal.h, which includes this file).
// Who may include it: rpg/sim, rpg/story, rpg/view and tools (it needs only the Monster enum).
#pragma once
#include <cstddef>
#include "rpg/art/art_monsters.h"

namespace gsim {
using art::Monster;

struct MStat { float hp, dmg, speed, range, aggro, radius, windup; int xp; bool ranged, flying; };
// windup: the telegraph before the blow (s). (Witness playtests, 2026-10) the pack lungers (wolf, ice wolf, hyena,
// ember hound) wind up 0.44-0.48 s, readable at a human reaction (0.25-0.35 s) with time left to roll; at 0.28-0.32 s
// a newcomer reacting in 0.35 s was bitten 18 times in 24 (the metrics bot never showed it: it reacts with no delay).
// The wolf bites for 10 (was 13): with the pack's turns and its circling after a lunge (ai.cpp), a level-1 hero who
// never rolls beats three same-level wolves with about half their health left (rpg_test --pack-probe)
inline const MStat& mstat(Monster m) {
  static const MStat t[] = {
      {34, 10, 66, 13, 120, 5, 0.48f, 12, false, false},   // Wolf
      {36, 8, 56, 13, 90, 6, 0.40f, 14, false, false},     // Boar
      {95, 15, 46, 17, 100, 8, 0.50f, 38, false, false},   // Bear
      {22, 4, 30, 11, 80, 5, 0.45f, 6, false, false},      // Slime
      {34, 7, 58, 13, 110, 6, 0.34f, 15, true, false},     // Spider
      {14, 3, 76, 10, 120, 4, 0.25f, 5, false, true},      // Bat
      {35, 8, 40, 15, 120, 5, 0.42f, 18, false, false},    // Skeleton
      {70, 12, 38, 17, 120, 6, 0.48f, 30, false, false},   // Draugr
      {26, 6, 60, 12, 120, 5, 0.30f, 10, false, false},    // Goblin
      {105, 17, 44, 19, 110, 9, 0.55f, 60, false, false},  // Troll
      {60, 10, 48, 15, 140, 6, 0.40f, 36, true, true},     // Wraith
      {22, 5, 30, 11, 60, 6, 0.40f, 6, false, false},      // Mudcrab
      {46, 9, 66, 13, 130, 5, 0.46f, 20, false, false},    // IceWolf
      {62, 10, 52, 15, 120, 7, 0.36f, 30, true, false},    // FrostSpider
      {85, 14, 40, 18, 100, 8, 0.50f, 42, false, false},   // Sandworm
      {1500, 30, 72, 34, 320, 18, 0.60f, 1500, true, true},// Dragon
      // M3c Wildlands wildlife (LIFE lane): fair against the classic beasts of the same depth (ai.cpp gives each its
      // move: the scorpion's every-third heavy sting, the hyena pack's circling, the lurker's lunge from the water, the
      // yeti's frost slam, the wisp's kited bolts, the hound's fire bite, the blightspawn's spore burst)
      {52, 10, 48, 16, 100, 6, 0.42f, 24, false, false},   // Scorpion
      {28, 7, 72, 13, 150, 5, 0.44f, 11, false, false},    // Hyena (packs of 3-4)
      {78, 15, 36, 17, 70, 8, 0.50f, 32, false, false},    // Lurker (slow ashore, a fast lunge)
      {108, 17, 46, 19, 120, 9, 0.55f, 64, false, false},  // Yeti
      {26, 7, 58, 12, 150, 4, 0.50f, 20, true, true},      // Wisp (ranged bolts, floats)
      {48, 11, 74, 13, 150, 5, 0.46f, 32, false, false},   // EmberHound
      {50, 10, 40, 15, 130, 6, 0.45f, 28, false, false},   // Blightspawn
      // M6 Steel (FOES lane; ai.cpp gives each its moves): the harpy circles on the wing, dives (a swoop you roll
      // through: it bites on contact) and shrieks (a short slow); the golem is slow stone (double armour, foes_game.cpp),
      // every second blow its wide telegraphed ground slam, and fire and frost slide off it sooner
      {30, 9, 72, 12, 160, 5, 0.30f, 22, false, true},     // Harpy (packs of 2-3 on the cliffs and crags)
      {160, 19, 28, 19, 90, 9, 0.70f, 76, false, false},   // Golem (alone: crystal barrens, mountains, ruin wardens)
      // M10 Beyond the Mist (phase A numbers; the SEA lane tunes them and gives each its moves): they swim (Actor::swim)
      {240, 20, 62, 22, 170, 10, 0.60f, 120, false, false},  // SeaSerpent (open and storm seas; strikes from the swell)
      {420, 24, 40, 26, 150, 12, 0.70f, 220, false, false},  // Kraken (the storm seas; tentacles round a boat)
      {40, 9, 58, 14, 130, 5, 0.34f, 20, false, false},      // Merrow (reefs, wrecks, sea caves; bands of 2-4; amphibious)
  };
  static_assert(sizeof(t) / sizeof(t[0]) == (size_t)Monster::COUNT, "stats for every monster");
  return t[(int)m];
}
// the wind-up of a heavy, telegraphed blow (the brutes' slam, the scorpion's sting, the blightspawn's spore burst);
// the view's danger ring fills over the same time
inline float heavyWindup(Monster m) {
  switch (m) {
    case Monster::Troll: case Monster::Yeti: return 0.9f;
    case Monster::Blightspawn: return 0.75f;
    case Monster::Golem: return 1.0f;
    default: return 0.8f;
  }
}
inline const char* monsterName(Monster m) {
  static const char* n[] = {"WOLF", "BOAR", "CAVE BEAR", "SLIME", "GIANT SPIDER", "BAT", "SKELETON", "DRAUGR", "GOBLIN", "TROLL",
                            "WRAITH", "MUDCRAB", "ICE WOLF", "RIME SPIDER", "SANDWORM", "ASHFANG THE DRAGON",
                            "GIANT SCORPION", "HYENA", "MIRE LURKER", "YETI", "WILL-O'-WISP", "EMBER HOUND", "BLIGHTSPAWN",
                            "HARPY", "STONE GOLEM", "SEA SERPENT", "KRAKEN", "MERROW"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Monster::COUNT, "a name for every monster");
  return n[(int)m];
}
}  // namespace gsim
