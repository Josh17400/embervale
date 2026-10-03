// Architecture styles (style.h). M0 architecture lane.
// archForBiome and ArchStyle::key are inline in style.h so rpg_sim and the headless tests (which do not link rpg_art)
// can use them; this file only anchors the header in the rpg_art library and checks it compiles on its own.
#include "rpg/culture/style.h"

namespace art {
static_assert((int)RoofShape::COUNT == 8 && (int)RoofMat::COUNT == 7 && (int)WallMat::COUNT == 6, "style enums changed: update the painters");
}  // namespace art
