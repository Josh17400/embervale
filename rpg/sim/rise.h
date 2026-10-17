// M12 "Rise and Fall" (VISION_PLAN 13 M12, 4.7, 15.6.4, 15.33): what the view is told of the player's place in the
// world's history. Frozen in phase A (the lead's): the SERVICE lane (rpg/sim/service*.cpp) and the LORD lane
// (rpg/sim/lord*.cpp) emit Ev::Rise; the VIEW lane (rpg/view/render_rise.cpp, View::riseEvent) shows it: a herald's
// banner, a fanfare, the rank's insignia, the fief's toast. Event::a = RiseEv, Event::f = the kind's number (the new
// rank, gold paid or taken, ...), Event::s = the line to show, Event::id = the low 32 bits of the kingdom it concerns.
#pragma once
#include <cstdint>

enum class RiseEv : uint8_t {
  None,
  Enlisted,        // sworn to a kingdom's army (f = the rank)
  Promoted,        // a new rank (f = the rank)
  Wage,            // the week's pay (f = gold)
  Order,           // a campaign order given (f = service::OrderKind)
  OrderDone,       // an order carried out (f = merit gained)
  OrderFailed,     // an order failed or its siege ended without the player
  Discharged,      // left the army honourably (f = the rank held)
  Deserted,        // broke the oath (barred from that army; f = the rank held)
  Mission,         // a diplomacy mission given (f = service::MissionKind)
  MissionDone,     // the terms carried (f: 1 peace, 2 alliance, 3 tribute, 0 refused)
  FiefGranted,     // a lordship (f: 1 granted, 2 charter bought, 3 a ruin resettled, 4 conquered)
  Tax,             // the fief's taxes came in (f = gold)
  Tribute,         // tribute paid to the liege (f = gold)
  TributeRefused,  // tribute withheld (the liege's anger)
  Independence,    // the player's banner raised over a realm of their own
  Crowned,         // the player's realm is a kingdom (f = the number of settlements)
  WarlordNear,     // a warlord rose near the player (f = 1 rose, 2 its warband raids)
  BossRaid,        // a world boss raided a settlement near the player
  COUNT
};
