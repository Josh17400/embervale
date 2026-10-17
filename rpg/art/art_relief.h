// EMBERVALE art, M11 "High Ground" (VISION_PLAN 11.3, 13 M11, 15.32): the relief props (art::Prop::SummitShrine ..
// PrayerFlags). FALLS lane (its only editor after phase A; it may ADD here, never rename or change the meaning of what
// is listed). Included by rpg/art/art_props.cpp (relief props are drawn through propSprite like every other prop).
//
// Painted in the game's high 3/4 view with the light from the top-left (lit tops and west faces, mid-tone south faces,
// shaded east sides), a selective 1 px outline, cast shadows to the lower right, and the cohesive palette.
#pragma once
#include <cstdint>
#include "engine/pix.h"
#include "rpg/art/art_props.h"

namespace art {

int reliefPropW(Prop p);
int reliefPropH(Prop p);
int reliefPropFrames(Prop p);
void paintReliefProp(Canvas& c, Prop p, int frame);

// ---- the waterfalls' frames (rpg/view/render_relief.cpp bakes one strip per fall tile; tools/preview/relief_gallery
// previews them). A fall tile's columns (a fall from the north or toward the north) or rows (a side cascade) are given
// for -3..18 of the tile (22 spans): where the step's contour crosses each (window px: the first face pixel below the
// lip; for a rim the first pixel of the higher ground; for a side cascade the contour's x) and whether water runs over
// it there. Each painter writes frame f into its cell of c (FALL_FRAMES cells side by side, one for a frozen fall),
// the cell's top-left at window px (ox, oy); gox, goy the window's origin in global tiles (the streaks follow the
// GLOBAL pixel, so a wide fall is one curtain across tiles). Faces are 12 px a level.
constexpr int FALL_FRAMES = 6;
struct FallSpan { int16_t at = 0; bool wet = false; };
void paintFallNorth(Canvas& c, int f, int ox, int oy, int x0, const FallSpan* col, int fh, int drop, bool frozen, int gox, int goy);
void paintFallSide(Canvas& c, int f, int ox, int oy, int y0, const FallSpan* row, int dir, int drop, bool frozen, int gox, int goy);   // dir 1 from the west, 2 from the east
void paintFallRim(Canvas& c, int f, int ox, int oy, int x0, const FallSpan* col, bool frozen, int gox, int goy);

}  // namespace art
