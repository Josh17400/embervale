// P2c the gear ladder (docs/P2C_CONTRACTS.md 3b; docs/PLAN.md "The gear ladder"; docs/STYLE_P2.md 27): the owner's rags to
// riches. Six look tiers per worn piece, DERIVED from what every item already carries (its item level's band, rarity,
// material, alloy) and, for people, from the census rank. No new item field, no save change, no RNG draw in the loot rolls
// or the census, no rule or balance change; rig 1 never sees a tier (HumanLook's ladder fields enter key() at rig 2 only).
// Pure functions (LADDER-DOLL).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/gear_look.h"
#include "rpg/sim/items.h"

namespace gear {

// A piece's look tier (PLAN's table): rows the item's band (gear::bandOf of its ilvl), columns its rarity. tier 0: not a
// worn piece (rings, amulets, goods). rare: the Rare mark (the next tier's trim). flame: Mythic (T6 with the ember).
// maker: the culture whose alloy the piece is made of (a masterwork alloy, tierStep 3, is T5 at least); may be null.
struct LookTier { uint8_t tier = 0; bool rare = false; bool flame = false; };
LookTier lookTier(const Item& it, const cult::Culture* maker = nullptr);
// the metal a piece is drawn in (HumanLook::metalKind codes): 1 leather 2 bronze 3 iron 4 steel 5 gilded 6 jade 7 obsidian
// 8 ember 9 starmetal 10 alloy 11 mythic; 0 cloth, wood or nothing. A heartland steel piece above band 3 is the band's
// metal (GILDED band 4, OBSIDIAN 5, EMBERFORGED 6, STARMETAL 7: gear.cpp matWord), D20.
uint8_t metalKindOf(const Item& it);

// (alloys i1, the owner: "does this fit with all our ores") a culture alloy's keys as the icons draw them (gearIcon: the
// light and dark key and the sheen, cult::Sheen + 1), and each alloy's metal family (art::doll::alloyFamily*, 0-based) in
// arms.alloys order: (alloys i3) the people's own family of the metal its name says (art::doll::alloyFamilyAt(slot,
// people)), parted from the people's other metals by dE 20 (docs/P2C_CONTRACTS.md 3b, gate C15)
struct AlloyKeys { uint32_t light = 0, dark = 0; uint8_t sheen = 0; };
AlloyKeys alloyKeys(const cult::Culture& c, int alloyIndex);   // alloyIndex 0-based
std::vector<uint8_t> alloyFamilies(const cult::Culture& c);
// (alloys i3) the metal word of an alloy's name: 0 BRONZE 1 BRASS 2 IRON 3 STEEL 4 SILVER 5 GLASS (art::doll::alloyFamilyMetal)
int alloyMetalWord(const cult::Alloy& a, int rank);

// wearGear's ladder half (gear_look.cpp calls it last): each worn slot's tier, the Rare and Mythic marks, the armour's
// metal (the body's, else the helm's, gloves', boots') and the metal in hand. Writes only the ladder fields.
void fillLadder(art::HumanLook& L, const WornGear& g, const CultureLookup& cultureOf);

// census::dress's ladder half: every slot the look wears takes `tier` (PLAN "People"); the metal from the look's band or
// alloy tint. Writes only the ladder fields (no draw).
// person: a census person (sets HumanLook::kitVar from the face: each fighter's own kit); false for the looks macros' tier=
// (P2c forge, D29) realm: the culture whose kingdom the person serves: its T4 officers wear its best alloy below the
// masterwork, its king (T5) the masterwork (alloyStyle); null: the rank's metal by baseStyle
void rankTiers(art::HumanLook& L, int tier, bool person = true, const cult::Culture* realm = nullptr);

// ---- the ladder kit (contracts 3b "The kit"): real Items for one people at one tier, from cult::Atlas::make(people,
// seed); the Heartland's forms are Item::form picks (padded, leather, mail, plate; the leather cap, nasal, great helm; round,
// kite, heater). wpn: the weapon family name (sword axe mace dagger greatsword spear bow staff; "" the people's sword).
// rarity -1: the kit's own, else every piece's (Rarity).
struct LadderKit {
  cult::Culture culture;
  Item weapon, bow, staff, armor, helmet, shield, gloves, boots, cloak;
  bool hasWeapon = false, hasBow = false, hasStaff = false, hasArmor = false, hasHelmet = false, hasShield = false, hasGloves = false,
       hasBoots = false, hasCloak = false;
  // (P2c envelope r3, PLAN "The envelope" T3 row) the people's cape to the waist at T3 (a look dial, not an item: L.cape and
  // the cloak's tier 3)
  bool cape = false;
  WornGear worn() const;
  CultureLookup lookup() const;   // resolves culture.id (only valid while this kit lives)
};
bool peopleOf(const std::string& name, cult::Archetype& out);   // fjordfolk highland heartland imperial dune steppe marsh jade
                                                                 // river suntemple sylvan starspire
LadderKit ladderKit(cult::Archetype people, int tier, int body, const std::string& wpn, int rarity, uint32_t seed);
// the kit's culture (Atlas::make(people, seed) with a test id 0x1ADD...: items carry it, the lookup resolves it); memoised
const cult::Culture& kitCulture(cult::Archetype people, uint32_t seed);

// ---- the looks macros (contracts 3b), installed as art::doll's LookSpecHook so every looks reader (rpg_test --limbs,
// the gates' matrices, doll_gallery --look, script_p2c) takes them: hero=1 heroine=1 kit=<people>:<tier>[:<seed>] wpn=
// rarity= npc=<people>:<role>[:<rank>][:<seed>] tier=<n> tierBody= tierHelm= tierShield= tierWeapon= tierGloves=
// tierBoots= tierCloak= tierBack= rare=<bits>. Order: hero / heroine, then the kit or the npc (with wpn and rarity), then
// the plain fields in the order written, then the tiers.
void applyLadderSpec(art::HumanLook& L, const std::string& spec, bool* lookPal, bool* bare, std::string* bad);
void installLadderMacros();   // idempotent (also done at static init where the linker keeps this file)
// (forge round 2) a culture's alloy (0-based) on that people's kit (a kit culture's own seed, else seed 1's forms), every
// metal piece in the alloy, worn over base (the A3 hero or heroine) at tier (0: T4, the masterwork T5): the boards and C18
art::HumanLook alloyKitLook(const art::HumanLook& base, const cult::Culture& c, int alloyIndex, int tier = 0);
// every field of L that differs from a default HumanLook, as a looks spec (`k=v,...`; rpg_test --expand-looks)
std::string describeLook(const art::HumanLook& L);

}  // namespace gear
