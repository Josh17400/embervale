// Architecture styles (style.h). M0 architecture lane.
// archForBiome and ArchStyle::key are inline in style.h so rpg_sim and the headless tests (which do not link rpg_art)
// can use them; this file only anchors the header in the rpg_art library and checks it compiles on its own.
#include "rpg/culture/style.h"

namespace art {
static_assert((int)RoofShape::COUNT == 14 && (int)RoofMat::COUNT == 12 && (int)WallMat::COUNT == 12, "style enums changed (M3): update the painters");
}  // namespace art
