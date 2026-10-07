// EMBERVALE art API, M3b "Builders & Societies" (VISION_PLAN 15.14), FORTIFICATIONS & GROUND lane: everything built
// that is not a building, painted from the builder's parts (rpg/build/parts.h): walls, wall towers and gatehouses from
// bld::FortParts, monuments and standing stones from bld::MonumentParts, the culture's street furniture (signs, tents,
// banners, graves, market crosses, the trades' work yards, market tables and cloths) from its art::PropStyle.
//
// Standing art rules (owner): the game's high 3/4 top-down camera, light from the top-left, cast shadows to the lower
// right, commercial 16-bit quality; walls join seamlessly (runs, corners, diagonals, gate joins).
#pragma once
#include <cstdint>
#include "engine/pix.h"
#include "rpg/art/art_building.h"
#include "rpg/art/art_props.h"
#include "rpg/build/parts.h"
#include "rpg/culture/culture.h"

namespace art {

// ---------------------------------------------------------------- walls, towers and gatehouses from FortParts
// A wall key may carry a fort look slot at bits 29..31 (WALL_LOOK_*): the view's per-window table of the settlements'
// FortParts (0: the wall style's defaults, bld::fortDefaults). The painters ignore the slot bits; the view resolves
// them and keys its texture cache on (key without the slot, bld::fortKey(parts)).
constexpr int WALL_LOOK_SHIFT = 29;
constexpr uint32_t WALL_LOOK_MASK = 7u << WALL_LOOK_SHIFT;
// wallTile(key) in the parts' material, tower form, coping, height and tints (the key's style bits are ignored: the
// parts' wall wins). wallTile(key) == wallTile(key, bld::fortDefaults(style of key)).
Canvas wallTile(uint32_t key, const bld::FortParts& f);
// the walk's height above the ground for these parts (WALL_H for the M3 walls; capitals stand taller)
int wallWalkHeight(const bld::FortParts& f);
// the gatehouse in the parts' gate form (the canvas, anchor and flank contract of gateHouse: GATE_OX / GATE_OY /
// GATE_CW / GATE_CH, the passage on tiles gx..gx+2, the flanking wall tiles gx-1 and gx+3 painted by the gate and
// joined to the wall runs arriving from gx-2 and gx+4). arms: the kingdom's (empty: the culture's own colours).
Canvas gateHouse(uint32_t seed, const cult::Heraldry& arms, const bld::FortParts& f);

// ---------------------------------------------------------------- monuments
// a standing stone in variant v (height, lean, notches, lichen; same canvas and anchor as propSprite(StandingStone)),
// in the parts' stone; the view picks v per global tile (like boulderVariant), so a ring never repeats one stone
Canvas standingStoneVariant(int v, const bld::MonumentParts& m);

// ---------------------------------------------------------------- the culture's street furniture
// the built props this file family styles beyond art_culture_props.cpp's (signs, toll posts, tents, the old market
// stall, banners, graves and grave cairns, the market cross, the trades' work yards, market tables and ground cloths):
// whether p has culture looks, and the look (same canvas, anchor and frames as propSprite(p); false: not styled here)
bool builtPropStyled(Prop p);
bool builtPropSprite(Prop p, const PropStyle& st, Canvas& out);
// the market's open tables, ground cloths and the mine hill in the culture's dress (canvases as marketTable /
// groundCloth / mineHill; the classic style gives exactly those)
Canvas marketTableStyled(int goods, int shade, bool closed, const PropStyle& st);
Canvas groundClothStyled(int goods, int cloth, bool closed, const PropStyle& st);
Canvas mineHillStyled(int variant, int land, const PropStyle& st);
// the culture of a PropStyle as an archetype (-1: the classic look)
inline int propCulture(const PropStyle& st) { return (int)st.culture - 1; }

// the culture's materials for its street furniture and work yards (base colours; each painter builds its ramps):
// timber and its dark, stone, the roofing (sod, thatch, tile, felt, reed, glazed tile, slate, leaves), two cloths, metal
struct PartLook {
  int arch = -1;               // cult::Archetype (-1: the classic look, every colour the classic ramps')
  uint32_t wood = 0, woodDark = 0, stone = 0, roof = 0, cloth = 0, cloth2 = 0, metal = 0;
};
PartLook partLook(const PropStyle& st);
// the trades' yard props and the wayside props in a culture's materials (same canvas, anchor and frames as
// propSprite(p)); the classic look (arch -1) gives exactly propSprite(p)
Canvas economyPropStyled(Prop p, const PartLook& L);   // rpg/art/art_market.cpp (DryingRack .. MineHill)
Canvas wildPropStyled(Prop p, const PartLook& L);      // (FishingShack, TollPost, HerbBed, GraveCairn)
// a lone grave marked by a culture's burial custom (arch: cult::Archetype, -1 classic; stone / accent: 0 the classic)
Canvas graveCairnSprite(int arch, uint32_t stone, uint32_t accent);

}  // namespace art
