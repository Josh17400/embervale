// EMBERVALE art, M12 "Rise and Fall" (VISION_PLAN 13 M12, 15.33): the rise props (art::Prop::LordBanner, Watchtower).
// VIEW lane (its only editor after phase A; it may ADD here, never rename or change the meaning of what is listed).
// Included by rpg/art/art_props.cpp (drawn through propSprite like every other prop; the view tints the banner's cloth
// in its owner's arms the way realm_render.cpp tints the war props).
//
// Painted in the game's high 3/4 view with the light from the top-left (lit tops and west faces, mid-tone south faces,
// shaded east sides), a selective 1 px outline, cast shadows to the lower right, and the cohesive palette.
#pragma once
#include "engine/pix.h"
#include "rpg/art/art_props.h"

namespace cult { struct Heraldry; }

namespace art {

// ---- (M12 VIEW lane) the rise's marks on a person (art::HumanLook, painted by art_human.cpp). They ride the high bits
//      of HumanLook::armsOrnament (cult::ARM_* use the low ten), so every earlier look keeps its key and its pixels:
//      the sworn realm's tabard over the gear (in tabardColor, trimmed in patternColor when the look has no cloth pattern,
//      else a lighter field; gold from captain up), the rank's mark on it
//      (recruit: none; sergeant: two chevrons on the chest; captain: a sash; marshal: a gilded sash and collar, the
//      cloak is the look's own), a steward's keys, a warlord's trophies at the belt. And two held things (weapon ids).
constexpr uint16_t RISE_TABARD = 0x0400, RISE_KEYS = 0x4000, RISE_TROPHIES = 0x8000;
constexpr int RISE_RANK_SHIFT = 11;              // 3 bits: 0 none, 1 recruit .. 4 marshal
constexpr uint16_t RISE_MASK = 0xFC00;           // every rise bit (the culture's ornament bits are the rest)
inline uint16_t riseRankBits(int rank) { return (uint16_t)((rank < 0 ? 0 : rank > 4 ? 4 : rank) << RISE_RANK_SHIFT); }
inline int riseRankOf(uint16_t ornament) { return (ornament >> RISE_RANK_SHIFT) & 7; }
constexpr uint8_t WEAPON_LEDGER = 10, WEAPON_SCROLL = 11;   // HumanLook::weapon: a steward's ledger, an envoy's sealed scroll case

int risePropW(Prop p);
int risePropH(Prop p);
int risePropFrames(Prop p);
void paintRiseProp(Canvas& c, Prop p, int frame);

// ---- VIEW lane (phase B) additions: the pieces the view draws round the rise (rpg/view/render_rise.cpp, rise_*.cpp)
// The lord's standard (Prop::LordBanner) in full arms: a sheet of risePropFrames(LordBanner) frames, each
// risePropW x risePropH, the same anchor as the plain prop (division, charge or glyph, trim, gold fringe).
Canvas lordBannerSprite(const cult::Heraldry& h);
// A rank's badge (service::Rank 1..4: recruit one chevron, sergeant three, captain a sash and a star, marshal crossed
// batons under a crown) on a small heater shield in the army's field with its trim, w x h about size x size * 1.2
// (size 9..32). rank 0: an empty shield.
Canvas rankBadge(int rank, uint32_t field, uint32_t trim, int size);
// A warlord's standard (drawn by the view at a warlord's camp and seat): a crooked pole crowned with a horned skull, a
// ragged banner torn and holed in the warlord's colours, trophies hung from the crossbar, a cairn at its foot.
// 4 frames side by side, kWarlordStdW x kWarlordStdH each, bottom-centred on its tile like a prop.
constexpr int kWarlordStdW = 26, kWarlordStdH = 54;
Canvas warlordStandard(uint32_t field, uint32_t trim);
// A trophy pike (variant 0..3: a skull and a dented helm, skulls, a shield and a horn, a beast's skull): 14 x 40,
// bottom-centred on its tile; its cast shadow baked in.
constexpr int kTrophyW = 14, kTrophyH = 40;
Canvas trophyPike(int variant, uint32_t trim);
// Resettlers' stores (variant 0: fresh-cut timber stacked on trestles, 1: golden thatch bundles, 2: a heap of new
// stone and a mortar trough): 28 x 20, bottom-centred on its tile, shadow baked in.
constexpr int kStoresW = 28, kStoresH = 20;
Canvas resettlerStores(int variant);
// A world boss's raid scar on the ground (variant 0..3): three or four scorched claw gashes torn through the earth
// in a patch of scorched earth, embers in their troughs, flat (drawn under everything), kClawW x kClawH, centred.
constexpr int kClawW = 52, kClawH = 26;
Canvas clawScar(int variant);

}  // namespace art
