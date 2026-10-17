// M6 Steel: world bosses and the realm (rpg/sim/realm.h beastRaid / beastSlain). FOES lane.
// A raid by a world boss (M12: on its lair's schedule, Realm::tickLairs / lairRaid in rpg/sim/realm_rise.cpp; the FOES
// lane plays one out in front of the player, rpg/sim/foes_game.cpp) burns part of the place
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
  if (d > 0) s.popPct = (uint8_t)std::max(0, (int)s.popPct - d / 5);   // (M12) the dead and the fled
  const Gid owner = s.owner;
  const int32_t gx = s.gx, gy = s.gy;
  const bool flee = d > 0 && s.damage >= 55;
  WorldEvent& e = addEvent(EvType::BeastRaid, owner, 0, site, gx, gy, day);
  e.mag = (int16_t)mon;
  // (M12 REALM lane) a place left half burned sends its people to the nearest town of its own realm
  if (flee) {
    SettlementState* best = nullptr;
    int64_t bd = 700ll * 700;
    for (SettlementState& o : settlements_) {
      if (o.site == site || o.owner != owner || (o.flags & (SS_BURNED | SS_ABANDONED | SS_RUINED))) continue;
      const int64_t dx = o.gx - gx, dy = o.gy - gy, dd = dx * dx + dy * dy;
      if (dd < bd) { bd = dd; best = &o; }
    }
    if (best && !(best->flags & SS_REFUGEES)) {
      best->flags |= SS_REFUGEES;
      best->refugeesFrom = site;
      best->refugeesDay = (uint16_t)std::clamp(day, 0, 65535);
      best->changedDay = best->refugeesDay;
      addEvent(EvType::Refugees, best->owner, owner, best->site, best->gx, best->gy, day);
    }
  }
}

void Realm::beastSlain(uint8_t mon, int32_t gx, int32_t gy, int day, uint8_t rank, bool byPlayer) {
  WorldEvent& e = addEvent(EvType::BeastSlain, 0, 0, 0, gx, gy, day);
  e.mag = (int16_t)((int)mon | ((int)(rank & 7) << 8) | (byPlayer ? 0x800 : 0));
}

}  // namespace realm
