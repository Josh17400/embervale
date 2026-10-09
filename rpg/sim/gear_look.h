// M6 Steel: how worn gear LOOKS (VISION_PLAN 7.2, 15.11; owner M6 notes: "the paper doll must show every equipped piece
// visibly in the culture's style; the character still starts in a shirt with no gear"). Shared contract, frozen after
// M6 phase A (lead, 2026-10-08): the ARMS lane is its only editor (it may ADD declarations) and implements it in
// rpg/sim/gear_look.cpp. Pure functions: the player (Game::recalcPlayer), the paper doll and the HUD's icons, NPC loot
// previews, the galleries and the tests all call them.
//
// An item carries its maker culture (Item::culture), material (Item::mat / alloy) and silhouette choice (Item::form);
// these functions turn that into the painter's inputs: art::HumanLook fields (helmForm, bodyForm, shieldForm,
// bladeForm, polearmForm, bowForm, armsOrnament, pauldron / skirt / crest, plume colour, armourTint / sheen, glow) and
// art::IconLook. A culture id of 0 means the heartland look (the M5 pixels).
#pragma once
#include <cstdint>
#include <functional>
#include "rpg/art.h"
#include "rpg/sim/appearance.h"
#include "rpg/sim/items.h"

namespace cult { struct Culture; }

// the culture behind an id (nullptr: unknown or 0). Game passes its world's atlas (ew::EndlessSource::culture).
using CultureLookup = std::function<const cult::Culture*(uint64_t)>;

// What the player has on, one pointer per equipment slot (nullptr: empty).
struct WornGear {
  const Item* weapon = nullptr; const Item* bow = nullptr; const Item* staff = nullptr;
  const Item* armor = nullptr; const Item* helmet = nullptr; const Item* shield = nullptr;
  const Item* ring = nullptr; const Item* amulet = nullptr;
  const Item* gloves = nullptr; const Item* boots = nullptr; const Item* cloak = nullptr;
};

// The chosen appearance in the shirt and trousers it starts with (the creator's preview uses it too). M3: the people
// (ears, proportions), the personal arms' field on every shield, and the homeland's dress cut and pattern on the shirt.
// (Moved here from rpg/sim/player.cpp in M6 phase A.)
art::HumanLook appearanceLook(const Appearance& app, const cult::Culture* home);

// Dress a look in worn gear: every slot shows (helmet, body, gloves, boots, cloak, shield, the weapon in hand, a bow or a
// staff on the back, amulet and ring glints), each piece in ITS maker culture's silhouettes and its metal. Fields of L
// that no worn piece sets keep their value (so call it on a fresh appearanceLook).
void wearGear(art::HumanLook& L, const WornGear& g, const CultureLookup& cultureOf);

// One piece's icon look (the pack, the shop, the paper doll's slots, pickups on the ground)
art::IconLook gearIcon(const Item& it, const cult::Culture* maker);
