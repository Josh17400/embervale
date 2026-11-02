// What an item card showed (lootp::CardInfo): the scripts' `expect lootcard ...` checks read it from the View
// (View::lastCard_), so view.h needs only this, not the whole rarity painter (loot_paint.h, which includes it).
#pragma once
#include <string>

namespace lootp {
// What the card shows, for the scripts' checks (`expect lootcard ...`)
struct CardInfo {
  int rarity = -1, ilvl = 0, affixLines = 0, lostLines = 0, verdict = -2;   // verdict: 1 / 0 / -1, -2 none (WORN or no compare)
  bool power = false, compare = false, worn = false;
  std::string name;
  std::string maker;   // (P3) the maker row as drawn (gear::makerLine: the alloy's rank, an old maker's metal); "" none
};
}  // namespace lootp
