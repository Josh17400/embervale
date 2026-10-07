// EMBERVALE art API: monsters (painted in art_monsters.cpp). Part of rpg/art.h (include that).
#pragma once
#include <cstdint>
#include "engine/pix.h"

namespace art {

// ---------------------------------------------------------------- monsters
// Sheet layout: one row, MONSTER_FRAMES cells of monsterCellW x monsterCellH, facing RIGHT (renderer flips for left).
//   columns: 0-3 move cycle, 4 attack wind-up, 5 attack strike, 6 hurt, 7 dead (lying on the ground)
constexpr int MONSTER_FRAMES = 8;
enum class Monster : uint8_t {
  Wolf, Boar, Bear, Slime, Spider, Bat, Skeleton, Draugr, Goblin, Troll, Wraith, Mudcrab,
  IceWolf, FrostSpider, Sandworm,   // biome variants
  Dragon,                           // boss: big (~64x48), wings
  // M3c Wildlands wildlife (appended; phase A stand-ins recolour the classic painters, the LIFE lane paints them):
  Scorpion,                         // giant desert scorpion: pincers, a raised sting (dunes, badlands, scrub, stony desert)
  Hyena,                            // a spotted hyena, sloping back (savanna, steppe, scrubland); hunts in packs
  Lurker,                           // a marsh crocodile, low and long, jaws (mangrove, jungle, reed marsh, flooded forest)
  Yeti,                             // a shaggy white ape-giant (glacier, snowfields, tundra)
  Wisp,                             // a floating glow of light, flickering (mushroom forest, silverwood, crystal barrens, bogs at night)
  EmberHound,                       // a black hound with glowing cracks and a smoking mane (ash fields)
  Blightspawn,                      // a hunched, thorn-grown husk (blighted land, dark forest)
  COUNT
};
int monsterCellW(Monster m);
int monsterCellH(Monster m);
Canvas monsterSheet(Monster m);

}  // namespace art
