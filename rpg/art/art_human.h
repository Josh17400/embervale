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
  uint8_t weapon = 0;    // 0 none, 1 sword, 2 axe, 3 bow, 4 staff, 5 dagger, 6 hammer/pick, (M4) 8 spear / polearm, 9 walking
                         // stick — drawn in hand, swings in frames 5/6
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

  // ---- M6 Steel: the rest of the culture arms grammar and the metal a piece is made of (VISION_PLAN 7.2, 15.11).
  //      0 everywhere = the M5 look, pixel for pixel (art_hash). weapon 8 (M4) is the polearm; 9 a walking stick.
  uint8_t polearmForm = 0;  // weapon 8: cult::Polearm + 1 (spear, glaive, halberd); 0 = the M4 spear
  uint8_t bowForm = 0;      // weapon 3 and a bow on the back: cult::BowKind + 1 (self, recurve, composite, crossbow,
                            // longbow); 0 = the classic bow
  uint32_t armourTint = 0;  // 0 = each piece's material band ramp; else the metal's light key (bronze, a culture alloy)
                            // the armour pieces (helm, body, gloves, boots, shield rim) are painted in
  uint32_t armourTint2 = 0; // its dark key (0: derived from armourTint)
  uint8_t sheen = 0;        // cult::Sheen + 1 of that metal (matte, bright, dark, banded, iridescent, glowing, pale,
                            // burnished); 0 plain
  uint8_t glow = 0;         // legendary pieces that glint (bits: 1 weapon, 2 body, 4 helm, 8 shield, 16 cloak);
                            // the colour is glowColor
  uint32_t glowColor = 0;
  uint32_t shieldField = 0;  // (M6 fixer) a maker culture's shield paint: the field (0: tabardColor, the hero's arms)
  uint32_t shieldDevice = 0; // and its device (0: gold)

  // ---- P2a Clear Sight (VISION_PLAN 15.36, rpg/art/art_rig.h): the body base the v2 rig paints. v1 ignores both (no
  //      v1 pixel moves); mixed into key() only when set, so every earlier look keeps its key. Derived, never saved:
  //      looks.cpp sets body from the census `female`, gear_look.cpp from Appearance::female.
  uint8_t body = 0;          // 0 man, 1 woman
  uint8_t age = 0;           // 0 adult, 1 child, 2 youth (P2b: census children aged 10-14), 3 elder (P2b: census age 60+)

  // ---- P2b New People (VISION_PLAN 15.36 "The look fields"; rpg/art/doll/doll.h): what the doll draws a face and a cut
  //      from. Derived, never saved: looks.cpp fills them from its own hash stream at the end of census::dress (after
  //      every existing draw), gear_look.cpp from a hash of the Appearance. v1 ignores them (no v1 pixel moves) and they
  //      enter key() only at rig 2 (rig 1's keys, caches and goldens stay identical until the flip). 0 = A3's.
  uint8_t head = 0;          // head type (doll::headTypeName): man 0 oval 1 square 2 round 3 long 4 craggy 5 heavy, 6 fine
                             //   7 sharp 8 long-elven; woman 0 oval 1 heart 2 round 3 long 4 square 5 full, 6-8 the elven;
                             //   youth the soft four of its sex (oval round long fine); child 0 round 1 oval 2 pointed 3 elfin
  uint8_t nose = 0;          // man 0 button 1 straight 2 long 3 hooked 4 broad; woman 0 button 1 straight 2 upturned 3 aquiline
  uint8_t brow = 0;          // man 0 straight 1 heavy 2 arched 3 thin; woman 0 arched 1 straight 2 thin 3 full
  uint8_t marks = 0;         // bits: 1 freckles, 2 scar, 4 beauty mark, 8 weathered, 16 lined, 32 gaunt (never on kids)
  uint8_t hairVar = 0;       // the style within the hair family (doll::hairStyleOf): Bald (bald, shaved); Short (short,
                             //   cropped, tousled, spiky, receding); Long (long loose, long wavy, shag, bob); Ponytail
                             //   (ponytail, high ponytail, pigtails); Mohawk (mohawk, topknot); Braids (braid, twin braids,
                             //   locs, crown braid); Bun (bun, top bun); Curls (curly, big curls)
  uint8_t beardStyle = 0;    // when beard: 0 full 1 stubble 2 short boxed 3 long 4 braided 5 goatee 6 moustache 7 Van Dyke
                             //   8 mutton chops 9 handlebar
  uint8_t neck = 0;          // neckwear: 0 none 1 scarf 2 kerchief 3 collar (torc and amulet come from jewellery / amulet)
  uint8_t garb = 0;          // a role's work garment over the outfit and cut: 0 none (they decide) 1 work apron (smith,
                             //   shipwright) 2 smock (farmer, labourer) 3 jerkin (hunter); (FOLK-SIM, added) the
                             //   commoners' own wear beside the culture's cut: 4 dress 5 tunic (shirt and trousers) 6 coat
                             //   7 culture tunic 8 kilt 9 gown (the doll draws it; v1 keeps the cut); (P2b production)
                             //   10 rags (the poorest) 11 kaftan 12 wrap 13 poncho (a woman in her people's men's cut)
  uint8_t civil = 0;         // (P2b fix TRADES-DRAWN-ON-V1) 1: a civil trade (smith, shipwright, hunter, scholar,
                             //   lamplighter) that never fights: its v1 leathers are work clothes (the doll draws its
                             //   garb, no armour: doll_resolve.cpp armourOf) and its tool rides at rest (no ACTION:
                             //   art_rig.cpp rig2Covers). Derived, never saved; v1 ignores it; in key() only at rig 2

  // ---- P2c the gear ladder (docs/P2C_CONTRACTS.md 3b; rpg/sim/gear_ladder.h): derived, never saved; v1 ignores them and
  //      key() mixes them only at rig 2 and only when set. Filled by wearGear (each worn piece's tier) and census::dress
  //      (the rank's tier); the looks macros (gear::applyLadderSpec) for tools.
  enum : int { TIER_BODY, TIER_HELM, TIER_SHIELD, TIER_WEAPON, TIER_GLOVES, TIER_BOOTS, TIER_CLOAK, TIER_BACK, TIER_SLOTS };
  uint8_t tier[TIER_SLOTS] = {};  // 0 none, 1-6 (T1 Scrap .. T6 Legendary) per slot: body helm shield weapon (in hand)
                                  // gloves boots cloak back (the bow or staff on the back)
  uint8_t rare = 0;          // bits by slot (1 << TIER_*): the Rare mark (the piece wears the next tier's trim)
  uint8_t metalKind = 0;     // the armour's metal: 0 legacy; 1 leather 2 bronze 3 iron 4 steel 5 gilded 6 jade 7 obsidian
                             // 8 ember 9 starmetal 10 alloy (armourTint) 11 mythic
  uint8_t bladeKind = 0;     // the same codes for the piece in hand (0: metalKind)
  uint8_t mythic = 0;        // bits by slot: a Mythic piece (T6 with the flame)
  uint32_t gemColor = 0;     // 0: the glow colour, else the metal's default glow
  uint8_t motif = 0;         // the filigree's motif by the body's maker people: 0 oak (Heartland, default) 1 clouds (Jade)
                             // 2 laurel (Imperial) 3 knot (Fjordfolk, Highland)
  uint8_t floor = 0;         // (polish i1) bits by slot (1 << TIER_*): an empty slot drawn as the T1 floor (gear::fillLadder:
                             // nothing worn there; the resolver draws T1's padded jerkin / leather boots; rig 2)
  uint8_t kitVar = 0;        // (polish i1) a ranked fighter's own kit (census: rankTiers, from the person's face; no draw):
                             // 0 none, else 1-255; the resolver varies each piece's tier a step, the dials, gloves, the
                             // sergeant's cloak, a bare head now and then (doll_resolve.cpp ladderLook; rig 2)
  int topTier() const { int t = 0; for (uint8_t v : tier) t = v > t ? v : t; return t; }

  // Identity of the painted sheet: equal keys must mean identical sheets. The renderer caches baked sheets by it,
  // so every field that changes pixels must be mixed in here (art_human.cpp).
  uint64_t key() const;
};
Canvas humanSheet(const HumanLook& look);
// M6 Steel: the 5-step metal ramp (0 deepest shadow .. 4 highlight) of an alloy's light / dark keys and its sheen
// (HumanLook::sheen: cult::Sheen + 1; 0 plain, e.g. bronze). The armour painters and the item icons share it so a
// piece looks the same metal on the doll and in the pack.
void metalRampKeys(uint32_t light, uint32_t dark, uint8_t sheen, uint32_t out[5]);
// one cell of the sheet (row = facing, frame = column) before the 1px outline: tests check what is painted where
Canvas humanCellRaw(const HumanLook& look, int row, int frame);

}  // namespace art
