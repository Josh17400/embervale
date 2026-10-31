// EMBERVALE RPG private header (SOLID 1.1): what the game_rpg / game_items / game_quests / game_dialogue / game_shop /
// game_save.cpp split shares: their common includes, the art enum names, and the lodging helpers that interaction
// (game_rpg.cpp) and the inn keeper's dialogue (game_dialogue.cpp) both use. Include it from those files only.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/sim/game.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/sim/jobs_ops.h"
#include "rpg/sim/lord_ops.h"
#include "rpg/sim/sea_ops.h"
#include "rpg/sim/service_ops.h"
#include "rpg/sim/skill_ops.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/home.h"
#include "rpg/sim/loot_ops.h"
#include "rpg/culture/culture.h"
#include "rpg/world/source.h"
#include "rpg/culture/society.h"
#include "rpg/story/radiant.h"
#include "rpg/story/story.h"

using art::Monster;
using art::Prop;

// (dialogue actions: DlgAct in rpg/sim/game_internal.h)

// M0b: a night in a bed lasts until morning: from the afternoon on you wake around 06:00-07:00 (never less than 8
// hours); a nap in the day is 8 hours
inline int hoursToMorning(float hour) {
  if (hour >= 15.0f || hour < 6.0f) {
    float until = 6.5f - hour;
    if (until < 0) until += 24.0f;
    return std::max(8, (int)std::lround(until));
  }
  return 8;
}

// M0b fix round 2: hours of sleep in your rented room. On the morning it is due back you wake by 11:00 (the
// innkeeper would knock); from 11:00 on that day there is no time left to sleep (0)
inline int lodgingSleepHours(int day, float hour, int untilDay) {
  int h = hoursToMorning(hour);
  if (day == untilDay) {
    if (hour >= 11.0f) return 0;
    h = std::max(1, std::min(h, (int)std::floor(11.0f - hour)));
  } else if (day + 1 == untilDay && hour >= 15.0f) {
    // a night's sleep never runs past 11:00 on the due morning either
    h = std::min(h, (int)std::floor(24.0f - hour + 11.0f));
  }
  return h;
}
static const char* kRoomDue = "IT IS NEARLY NOON: YOUR ROOM IS DUE BACK.";
