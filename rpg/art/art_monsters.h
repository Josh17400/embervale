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
  // M6 Steel (VISION_PLAN 7.6: two new families; phase A stand-ins recolour the bat and the troll, the BEASTS lane
  // paints them):
  Harpy,                            // a winged woman-bird of the cliffs and crags: feathered wings, talons, a shriek;
                                    // flies, swoops from above (sea cliffs, mountains, badlands, alpine meadows)
  Golem,                            // a hulking walker of stacked stone (or crystal, or clay) with a glowing core rune:
                                    // slow, armoured, a ground-pounding slam (ruins, mountains, crystal barrens)
  COUNT
};
int monsterCellW(Monster m);
int monsterCellH(Monster m);
Canvas monsterSheet(Monster m);

// ---------------------------------------------------------------- M6 variants (VISION_PLAN 7.6 "Variant looks")
// Overlays drawn on top of the existing painters (a Crystal Troll or an Ashen Wolf costs no new rig), in the sheet's own
// frames (they follow the move cycle, the wind-up, the hurt and the dead frame).
enum : uint16_t {
  MO_HORNS = 1 << 0,      // curling horns on the head
  MO_SPIKES = 1 << 1,     // a ridge of spikes down the back
  MO_CRYSTAL = 1 << 2,    // crystal growths (glints)
  MO_MOSS = 1 << 3,       // moss and lichen on the back
  MO_RIME = 1 << 4,       // frost rime, icicles under the belly
  MO_EMBER = 1 << 5,      // glowing ember cracks
  MO_PLATES = 1 << 6,     // armour plates (bandit-strapped iron, or bony scutes)
  MO_EYES = 1 << 7,       // glowing eyes
  MO_BONEMASK = 1 << 8,   // a skull mask over the face
  MO_ALL = 0x1FF,
  // (M6 fixer round 2) not a drawn overlay but a body size: the GREAT form, painted at native resolution in a bigger
  // cell (lookCellW / lookCellH). Only the lurker has one (the world-boss WYRM); other species ignore the bit.
  MO_GREAT = 1 << 9
};
struct MonsterLook {
  Monster base = Monster::Wolf;
  uint32_t tint = 0;        // body recolour (an "Ashen" or "Crystal" palette; 0: the species' own colours)
  uint16_t overlays = 0;    // MO_* bits
  uint8_t scale = 0;        // draw scale in percent for the renderer (0 = 100; elites ~115, named ~130); the sheet
                            // itself is painted at the species' cell size
  bool classic() const { return tint == 0 && overlays == 0; }
  uint64_t key() const { return (uint64_t)base | (uint64_t)overlays << 8 | (uint64_t)tint << 24; }   // scale is draw-time
};
// the species' sheet with the look's recolour and overlays (same layout and cell size as monsterSheet(base));
// classic() looks are monsterSheet(base) pixel for pixel
Canvas monsterSheetLook(const MonsterLook& look);
// the cell size of a look's sheet: the species' own, or the great form's (MO_GREAT) bigger cell
int lookCellW(const MonsterLook& look);
// (M6 fixer round 2, review: "elite overlays are stamped on families whose anatomy can't carry them") the overlays a
// species' body can carry: a wisp (a ball of light) takes only frost or embers, a slime no horns, plates, spikes or
// mask, a flyer no plates or horns, the crawlers in their own shells no horns or plates. The painter applies it, and
// foes::variantFor stores only what fits (so the look's key and its glow agree).
// (inline: the simulation, which does not link the art, uses it too)
inline uint16_t overlaysFit(Monster m, uint16_t o) {
  switch (m) {
    case Monster::Wisp: return (uint16_t)(o & (MO_EMBER | MO_RIME | MO_GREAT));
    case Monster::Slime: return (uint16_t)(o & ~(MO_HORNS | MO_PLATES | MO_SPIKES | MO_BONEMASK));
    case Monster::Bat: case Monster::Harpy: return (uint16_t)(o & ~(MO_PLATES | MO_HORNS));
    case Monster::Mudcrab: case Monster::Spider: case Monster::FrostSpider: case Monster::Scorpion: case Monster::Sandworm:
      return (uint16_t)(o & ~(MO_HORNS | MO_PLATES));
    default: return o;
  }
}
int lookCellH(const MonsterLook& look);

// (M6 BEASTS) the tint is a palette-ramp recolour: every hide pixel takes the tint ramp's hue at its own ramp step and
// luminance (eyes, teeth, claws and fire keep theirs). The overlays sit on per-frame anchors the painters report (the
// head, the eye, the torso), so horns stay on the head through the wind-up, the hurt frame and the body lying dead.
//
// Golem materials: a golem's tint picks its material, painted as such (not washed): 0 = stone (lichen, cracks, a cyan
// rune), and the tints below (or any tint golemMaterial maps to one; another tint recolours the stone).
constexpr uint32_t GOLEM_CRYSTAL = 0xFFE6B46Eu;   // rgba(110, 180, 230): faceted crystal, shards on the back, a magenta rune
constexpr uint32_t GOLEM_CLAY = 0xFF4668B0u;      // rgba(176, 104, 70): smoothed clay, incised glyphs, an amber rune
constexpr uint32_t GOLEM_IRON = 0xFF706460u;      // rgba(96, 100, 112): riveted iron plates, a furnace heart
int golemMaterial(uint32_t tint);                 // 0 stone, 1 crystal, 2 clay, 3 iron
// The emissive pixels of a look (glowing eyes, ember cracks, crystal glints, a golem's rune and eye slits) on their own
// sheet (same layout, transparent elsewhere): the view adds it after the night's darkness so they read in the dark.
bool monsterLookGlows(const MonsterLook& look);   // false: its glow sheet would be empty (skip it)
Canvas monsterGlowLook(const MonsterLook& look);

}  // namespace art
