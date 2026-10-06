// EMBERVALE art API: humans (painted in art_human.cpp). Part of rpg/art.h (include that).
#pragma once
#include <cstdint>
#include "engine/pix.h"

namespace art {

// ---------------------------------------------------------------- humans (player, NPCs, bandits)
// Sheet layout: cells of HUMAN_W x HUMAN_H, HUMAN_FRAMES columns x 3 rows.
//   rows:    0 = facing down (toward camera), 1 = facing up (away), 2 = facing right (flip for left)
//   columns: 0 idle, 1-4 walk cycle, 5 attack wind-up (arm raised back), 6 attack strike (arm forward), 7 hurt
// The character is ~11-13 px wide and ~20-22 px tall, standing on the bottom row of the cell.
constexpr int HUMAN_W = 16, HUMAN_H = 24, HUMAN_FRAMES = 8;

// Values are saved (Appearance::hair): append only. Bun and Curls arrived with the M0 character creator.
enum class Hair : uint8_t { Bald, Short, Long, Ponytail, Mohawk, Braids, Bun, Curls, COUNT };
enum class Outfit : uint8_t {
  Tunic,      // shirt + trousers (villager / player default)
  Dress,      // long skirt
  Robe,       // mage / priest, hooded optional
  Leather,    // leather armour (brown, studs)
  Chain,      // iron chainmail (grey)
  Plate,      // steel plate (bright steel, pauldrons)
  Elven,      // gold-green ornate armour
  Ebony,      // black/purple dark armour
  Guard,      // city guard: chain + tabard in tabardColor
  Rags,       // beggar / prisoner
  COUNT
};

struct HumanLook {
  uint32_t skin = rgba(232, 184, 140);
  uint32_t hairColor = rgba(90, 56, 30);
  uint32_t topColor = rgba(60, 100, 160);     // tunic/dress/robe/tabard main colour
  uint32_t bottomColor = rgba(80, 64, 50);    // trousers
  uint32_t tabardColor = rgba(150, 40, 40);   // Guard outfit trim, robe trim
  Hair hair = Hair::Short;
  Outfit outfit = Outfit::Tunic;
  bool beard = false;
  bool helmet = false;   // drawn over hair, style follows outfit (leather cap, iron helm, steel great-helm, elven, ebony horns)
  bool hood = false;     // cloth hood (robes, bandits)
  bool cape = false;     // cape behind (visible esp. from behind), colour = tabardColor
  bool shield = false;   // round shield on off-hand
  uint8_t weapon = 0;    // 0 none, 1 sword, 2 axe, 3 bow, 4 staff, 5 dagger, 6 hammer/pick — drawn in hand, swings in frames 5/6
  uint32_t weaponColor = rgba(200, 205, 215);   // blade/metal colour (tier tint)

  // ---- M0 equipment and appearance looks. Every default reproduces the pre-M0 pixels exactly (art_hash checks it),
  //      so NPC looks built before these fields existed are unchanged. 0 always means "none / legacy behaviour".
  //      Armour pieces share one material BAND id: 1 leather, 2 iron, 3 steel, 4 gilded, 5 jade, 6 obsidian,
  //      7 emberforged (see kBand* below; the player's item tiers map onto it in rpg/sim/player.cpp).
  uint8_t build = 0;        // body build: 0 average, 1 slim (narrow waist), 2 broad (heavy shoulders, deep chest)
  uint32_t eyeColor = 0;    // 0 = the palette's default eye colour
  uint8_t helmStyle = 0;    // 0 = legacy (style follows outfit when helmet is set), else a helmet BAND (always drawn):
                            //   leather cap, iron nasal helm, steel great-helm, gilded winged, jade crested, obsidian horns,
                            //   emberforged horned crown
  uint8_t armorStyle = 0;   // 0 = legacy (outfit decides), else a body-armour BAND (overrides the outfit's torso/legs)
  uint8_t gloves = 0;       // 0 none, else a glove BAND (leather gloves .. gauntlets with a flared cuff)
  uint8_t boots = 0;        // 0 none (legacy shoes), else a boot BAND (taller boots; metal greaves from steel up)
  uint8_t cloak = 0;        // 0 none, else a cloak style: 1 hooded travel cloak, 2 fur mantle, 3 long noble cloak with
                            //   gilt trim; colour = cloakColor. Replaces `cape` when set.
  uint32_t cloakColor = rgba(110, 40, 40);
  uint8_t shieldStyle = 0;  // 0 = legacy round shield (when shield is set), else 1 round, 2 heater, 3 kite, 4 tower
                            //   (always drawn); the field is tabardColor, the rim trimColor (or weaponColor)
  uint8_t backItem = 0;     // carried on the back, bits: 1 bow (+ quiver), 2 staff
  bool amulet = false;      // a pendant at the collar that glints on the idle frame
  uint32_t trimColor = 0;   // 0 = none; metal trim tint for armour pieces (tier tint): the shield rim
  bool ring = false;        // a ring glint on the weapon hand (idle frame)
  static constexpr int kBands = 7, kCloaks = 3, kShields = 4;

  // ---- M3 peoples and cultures (VISION_PLAN 5.5, 15.6). 0 everywhere = the M2 look, pixel for pixel (art_hash).
  //      Enum values are the culture engine's (rpg/culture/culture.h) + 1, so 0 can mean "legacy".
  uint8_t people = 0;       // 0 human, 1 half-breed (short pointed ears, a touch slimmer), 2 elf (long swept ears,
                            //   slender, a pixel taller, finer features)
  uint8_t cut = 0;          // garment cut: 0 = the outfit decides, else cult::Cut + 1 (tunic, robe, kaftan, wrap,
                            //   coat, kilt, poncho, gown) over the outfit's torso and legs
  uint8_t headwear = 0;     // 0 none, else cult::Headwear (+0: Headwear::None is 0) -- hood, cap, turban, fur hat,
                            //   veil, circlet, conical hat, headscarf; colour = headColor
  uint32_t headColor = 0;   // 0 = topColor
  uint8_t pattern = 0;      // cloth pattern on the garment: cult::Pattern (0 plain); colour = patternColor
  uint32_t patternColor = 0;
  uint8_t facePaint = 0;    // 0 none, 1..4 (stripes, dots, mask band, sun mark)
  uint8_t jewellery = 0;    // 0 none, 1 earrings, 2 torc / necklace, 3 both
  // culture arms: silhouettes over the material band (helmStyle / armorStyle / shieldStyle pick the band; these the
  // form). 0 = the band's own form.
  uint8_t helmForm = 0;     // cult::HelmForm + 1
  uint8_t bodyForm = 0;     // cult::BodyArm + 1
  uint8_t shieldForm = 0;   // cult::ShieldForm + 1 (when set it replaces shieldStyle's shape)
  uint8_t bladeForm = 0;    // cult::Blade + 1 (the sword's silhouette: leaf, falchion, scimitar, khopesh...)
  uint16_t armsOrnament = 0;// cult::ARM_* bits (plumes, fur trim, gilding, horse-tail, studs...)
  uint8_t pauldron = 0, skirt = 0, crest = 0;   // cult::ArmsStyle dials + 1 (0: the band's own)
  uint32_t plumeColor = 0;  // 0 = tabardColor

  // Identity of the painted sheet: equal keys must mean identical sheets. The renderer caches baked sheets by it,
  // so every field that changes pixels must be mixed in here (art_human.cpp).
  uint64_t key() const;
};
Canvas humanSheet(const HumanLook& look);
// one cell of the sheet (row = facing, frame = column) before the 1px outline: tests check what is painted where
Canvas humanCellRaw(const HumanLook& look, int row, int frame);

}  // namespace art
