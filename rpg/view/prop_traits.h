// How the renderer treats each prop: flat (drawn under everything), nature (jittered), tree, ground shadow, light.
// One owner per milestone (props/interiors lane); add a case here when a new art::Prop needs one.
#pragma once
#include "engine/color.h"
#include "rpg/art.h"

bool flatProp(art::Prop p);      // lies on the ground: drawn before the y-sorted scene
bool natureProp(art::Prop p);    // gets a small per-tile position jitter on the overworld
bool treeProp(art::Prop p);      // fades when the player walks behind it
int propShadow(art::Prop p);     // ground shadow: 0 none, 1 tree-sized, 2 large object, 3 a market stall's awning,
                                 // 4 a two-tile table, 5 a beast, 6 / 7 (M2) a wild prop's footprint-wide shadow
bool propLight(art::Prop p, float& radius, Color& c);   // light source: radius (px) and colour
