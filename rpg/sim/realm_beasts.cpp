// M6 Steel: world bosses and the realm (rpg/sim/realm.h beastRaid / beastSlain). FOES lane.
// A raid by a world boss (rolled once a game day by Game::foeStep, rpg/sim/foes_game.cpp) burns part of the place
// (damage: its buildings draw charred), scatters its people's trade (prosperity) and frightens them (mood). The
// event names the beast through its species (mag): rpg/story/rumours.cpp finds its name (foes::beastNameNear).
#include <algorithm>
#include "rpg/sim/realm.h"

namespace realm {

void Realm::beastRaid(Gid site, uint8_t mon, int damage, int day) {
  const SettlementState* known = settlement(site);
  if (!known) return;
  SettlementState& s = state(site);
  const int d = std::clamp(damage, 0, 100);
  s.damage = (uint8_t)std::clamp((int)s.damage + d, 0, 100);
  s.prosperity = (uint8_t)std::clamp((int)s.prosperity - d / 2, 0, 100);
  s.mood = (uint8_t)std::clamp((int)s.mood - (d > 0 ? 8 : 4), 0, 100);
  s.changedDay = (uint16_t)std::max(0, day);
  WorldEvent& e = addEvent(EvType::BeastRaid, s.owner, 0, site, s.gx, s.gy, day);
  e.mag = (int16_t)mon;
}

void Realm::beastSlain(uint8_t mon, int32_t gx, int32_t gy, int day, uint8_t rank, bool byPlayer) {
  WorldEvent& e = addEvent(EvType::BeastSlain, 0, 0, 0, gx, gy, day);
  e.mag = (int16_t)((int)mon | ((int)(rank & 7) << 8) | (byPlayer ? 0x800 : 0));
}

}  // namespace realm
