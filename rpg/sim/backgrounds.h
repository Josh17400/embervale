// Character backgrounds (VISION_PLAN 15.1): chosen in the character creator. Each grants one special trait,
// never gear; everyone starts with just the shirt on their back. Stored in the save (SAVE_VER 3).
#pragma once
#include <cstdint>

enum class Background : uint8_t {
  None,          // pre-M0 saves and test runs that skip the creator
  Blacksmith,    // blacksmith's child: cheaper smithing, can repair gear
  Hunter,        // animals noticed sooner, better pelts, wolves slower to aggro
  Novice,        // temple novice: knows a weak heal, longer shrine blessings
  Urchin,        // street urchin: better shady prices, simple lockpicking
  Farmhand,      // stamina regen, food heals more
  Noble,         // exiled noble: more quest offers from guards and jarls
  Sailor,        // swims farther; later buys and sails boats early (M10)
  Marked,        // marked one: a faint affinity with a lost magic tradition (hook into M9/M10)
  COUNT
};

struct BackgroundInfo {
  const char* name;    // upper case, fits the 5x7 font
  const char* perk;    // one line: the special trait
  const char* hook;    // one line: the story hook
};
const BackgroundInfo& backgroundInfo(Background b);
