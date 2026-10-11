// EMBERVALE art, M7b "Cities" (VISION_PLAN 15.23, 15.24): the city props (art::Prop::Bollard .. Canopy). CITYART lane
// (its only editor). Included by rpg/art/art_props.cpp; the props are drawn through propSprite like every other prop
// (art_props.cpp dispatches isCityProp props here).
//
// The CITYART lane may ADD here (culture-styled variants for the view to cache, joined runs for GardenWall / Hedge,
// the souk Canopy's cloth by culture...), never rename or change the meaning of what is listed.
//
// Every city prop is painted in the game's high 3/4 view with the light from the top-left (lit tops and west ends,
// mid-tone south faces, shaded east sides), a selective 1 px outline, and its ground shadow (the joined walls and the
// canopy carry their own; the view adds the rest by prop_traits propShadow).
#pragma once
#include <cstdint>
#include "engine/pix.h"
#include "rpg/art/art_props.h"
#include "rpg/culture/style.h"

namespace art {

int cityPropW(Prop p);
int cityPropH(Prop p);
int cityPropFrames(Prop p);
void paintCityProp(Canvas& c, Prop p, int frame);

// ---- (CITYART) the styled and joined looks. Same canvas size and anchor as propSprite(p) (cityPropW x cityPropH,
// standing on the tile's bottom edge, centred on its tile; the Pavilion over its 3x2 footprint), so the view swaps one
// for the other. st.classic(): the heartland look. variant: 0 .. cityPropVariants(p) - 1 (FruitTree: FRUIT_*; Planter:
// the planting; FlowerBed: the colours; Canopy: the cloth's pattern). joins (the joined props):
//   GardenWall, Hedge: fenceJoinPiece's bits (0..3 the joined sides N E S W, 4..7 the sides facing a one-tile gateway, a
//                      pier stands there, 8..11 the sides where the run ends, a pier caps it); 0 a lone pier
//   Canopy:            1 N, 2 E, 4 S, 8 W: a canopy tile beside it (the cloth runs on; a hem, ropes or a pole where not)
// snow: snow lies on its top (walls, hedges, planters, the monument's steps).
Canvas cityPropStyled(Prop p, const PropStyle& st, int variant = 0, int joins = 0, bool snow = false);
int cityPropVariants(Prop p);
inline bool cityPropJoins(Prop p) { return p == Prop::GardenWall || p == Prop::Hedge || p == Prop::Canopy; }
// does the culture's style change this prop's look (the view keys its cache on the style only for these)
inline bool cityPropStyledByCulture(Prop p) {
  return p == Prop::Bollard || p == Prop::Planter || p == Prop::GardenWall || p == Prop::Monument || p == Prop::Pavilion || p == Prop::Canopy;
}
// the fruit trees by the climate: apples and pears in the temperate north, oranges and pomegranates in the warm south
enum : int { FRUIT_APPLE = 0, FRUIT_PEAR = 1, FRUIT_ORANGE = 2, FRUIT_POMEGRANATE = 3 };
// biome: world.h Biome as int (0 ocean .. see rpg/sim/world.h; Desert, Swamp warm; Snow, Taiga cold), h: a per-tile hash
int fruitTreeKind(int biome, uint32_t h);
// a street tree's pit on a city walk or square: a kerbed square of soil under an iron grate (16x10, laid flat on the
// paving, its bottom row on the tile's bottom edge, centred on the tile), the kerb in the culture's stone
Canvas cityTreePit(const PropStyle& st);

}  // namespace art
