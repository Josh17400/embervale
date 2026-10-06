// EMBERVALE art API, M3 culture engine (VISION_PLAN 5.5): the culture-parameterised entry points the view and the
// galleries call. Part of rpg/art.h (include that). FROZEN after M3 phase A; the ARCHITECTURE lane implements them in
// rpg/art/art_culture.cpp (and whatever new rpg/art/*.cpp files it adds).
//
// Standing art rules (owner): every sprite matches the game's high 3/4 top-down camera (about 45-55 degrees: top
// surfaces visible, fronts foreshortened; never an eye-level elevation) with light from the top-left; props that can
// face a direction get facings; commercial 16-bit quality, cohesive palette, no boxy repetition.
#pragma once
#include <cstdint>
#include <memory>
#include "engine/pix.h"
#include "rpg/art/art_building.h"
#include "rpg/art/art_props.h"
#include "rpg/culture/culture.h"

namespace art {

// ---------------------------------------------------------------- heraldry
// A standing banner on a pole with full arms (division, two tinctures, charge or glyph, banner shape): the same canvas
// size and anchor as propSprite(Prop::Banner) (frames included). h.empty(): the plain banner prop.
Canvas bannerSprite(const cult::Heraldry& h);
// The arms on a heater shield, size x size px (8..32), for the creator, the paper doll, signs and the map
Canvas shieldArms(const cult::Heraldry& h, int size);
// A glyph: strokes on a 5 x 5 grid with symmetry rules (holy symbols, runes, magic sigils, shop signs), drawn at
// cell px per grid step in colour col (with a darker outline), on a transparent canvas of (5 * cell + 2)^2 px.
// style: 0 runic (straight strokes), 1 flowing (curves), 2 geometric (circles and dots), 3 knotwork
Canvas glyphSprite(uint32_t seed, int style, uint32_t col, int cell = 2);

// ---------------------------------------------------------------- city walls
// The gatehouse in a culture's wall style (Stone = gateHouse(seed, field, trim, emblem) exactly): same canvas, anchor
// and flanking towers as gateHouse. The view reads the style from Map::wall on the gate's flanking tile (1 + CityWall).
Canvas gateHouse(uint32_t seed, uint32_t field, uint32_t trim, int emblem, CityWall style);

// ---------------------------------------------------------------- culture-styled props
// A prop in a culture's style (fences, wells, lamps, benches, centrepieces, market stalls' awnings, shrines, statues,
// signs): same canvas size, anchor and frames as propSprite(p) / propFrame(p, f), so the view can swap one for the
// other. A classic style (st.classic()) or a prop with no styled variants returns exactly propSprite(p).
Canvas propSprite(Prop p, const PropStyle& st);
// does this prop have culture variants (the view only keys its texture cache on the style for these)
bool propStyled(Prop p);
// (M3 fixer round 2) a dry-stone dyke piece that joins its neighbours: mask bits 1 north, 2 east, 4 south, 8 west (the
// fence tiles beside it). One 3/4 wall of one height for every run: a lit coping on top, coursed stones on every face
// the camera sees, a cast shadow to the south-east, L / T / X corners closed with no gap or overshoot. 16x24, anchored
// like FenceV (bottom on the tile's bottom edge, centred).
Canvas dykePiece(int mask, const PropStyle& st);

// ---------------------------------------------------------------- incremental building paints (M2 carry-over)
// The web has no threads, and a palace painted in one go cost ~41 ms on desktop (~100 ms on an iPhone): one frame.
// A BuildingJob paints the same pixels as buildingSprite(b, w, h, style, seed, &info, facts), in steps of about
// budgetMs each, so the view can spread a big building over frames (desktop: still on its paint workers).
// PHASE A STUB: the first step paints everything.
struct BuildingJob;
std::shared_ptr<BuildingJob> beginBuilding(Building b, int wTiles, int hTiles, const ArchStyle& style, uint32_t seed,
                                           const BuildingFacts& facts);
bool stepBuilding(BuildingJob& job, double budgetMs);   // true once finished
Canvas finishBuilding(BuildingJob& job, BuildingInfo* info);   // after stepBuilding returned true

}  // namespace art
