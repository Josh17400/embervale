// The player's chosen appearance (character creator). Stored in the save from SAVE_VER 3 as a length-prefixed
// block, so later builds can append fields: readers skip what they don't know, older saves get the defaults.
#pragma once
#include <cstdint>
#include <string>
#include "engine/pix.h"
#include "rpg/culture/culture.h"

struct Appearance {
  std::string name = "YOU";                 // shown in dialogue and the journal; upper case
  bool female = false;
  uint8_t build = 0;                        // 0 average, 1 slim, 2 broad (art::HumanLook::build)
  uint8_t skinTone = 2;                     // index into the creator's 8 skin tones
  uint32_t skin = rgba(236, 188, 146);      // the resolved colour (what the painter uses)
  uint8_t hair = 1;                         // art::Hair
  uint32_t hairColor = rgba(120, 70, 36);
  bool beard = false;
  uint32_t eyeColor = 0;                    // 0 = palette default
  uint32_t topColor = rgba(70, 110, 150);   // shirt
  uint32_t bottomColor = rgba(78, 60, 44);  // trousers
  bool created = false;                     // went through the character creator (false for pre-M0 saves)
  // ---- M3 (SAVE_VER 7): the player's people, homeland and personal arms (VISION_PLAN 13 "homeland culture and
  //      personal heraldry added in M3", 15.6 peoples)
  uint8_t people = 0;                       // cult::People: 0 human, 1 half-breed, 2 elf (art::HumanLook::people)
  uint64_t homeland = 0;                    // cult::CultureId the player grew up in (0: none chosen)
  cult::Heraldry heraldry;                  // personal arms (shield, banner, the paper doll); empty = none yet
};
