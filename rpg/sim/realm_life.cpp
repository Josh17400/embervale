// M5 "Hearth and Hall": the residents' days reported to the realm (VISION_PLAN 15.12 -> 15.6.3). CITIZENS lane.
// life::Life calls these once per in-game day for each settlement it simulated (its hourly aggregate): the food made,
// brought in and eaten there moves SettlementState::food (and, a little, its kingdom's granaries), its people's mood
// pulls SettlementState::mood, so hunger in the streets feeds the famine -> trade collapse -> war chain. Raids that
// break in cost the place prosperity and damage (10.4 M5). The realm's own daily tick keeps pulling food back toward
// its kingdom's target, so a single bad day only dents it; a famine felt for days drags it down.
#include <algorithm>
#include "rpg/sim/realm.h"

namespace realm {

void Realm::lifeReport(Gid site, int foodDelta, uint8_t mood, int day) {
  (void)day;
  auto it = settlementIx_.find(site);
  if (it == settlementIx_.end()) return;
  SettlementState& s = settlements_[(size_t)it->second];
  foodDelta = std::clamp(foodDelta, -20, 10);
  s.food = (uint8_t)std::clamp((int)s.food + foodDelta, 0, 100);
  s.mood = (uint8_t)((s.mood * 3 + std::min<int>(mood, 100)) / 4);
  // the kingdom's granaries feel a hungry town a little (a city's shortage is the realm's too)
  if (foodDelta < 0 && s.owner)
    if (KingdomState* K = kingdomMut(s.owner)) K->food = std::clamp(K->food + (float)foodDelta * 0.0008f, 0.0f, 1.4f);
}

void Realm::lifeRaid(Gid site, bool failed, int day) {
  (void)day;
  auto it = settlementIx_.find(site);
  if (it == settlementIx_.end()) return;
  SettlementState& s = settlements_[(size_t)it->second];
  if (failed) {
    s.prosperity = (uint8_t)std::max(0, (int)s.prosperity - 10);
    s.damage = (uint8_t)std::min(100, (int)s.damage + 5);
    s.mood = (uint8_t)std::max(0, (int)s.mood - 5);
  } else {
    s.mood = (uint8_t)std::max(0, (int)s.mood - 1);
  }
}

}  // namespace realm
