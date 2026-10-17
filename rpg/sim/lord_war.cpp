// M12 "Rise and Fall": the liege's anger and the war for independence (rpg/sim/lord_ops.h). LORD lane.
//   - a tribute withheld (or the coffers too thin) is a TributeRefused; the second in a row brings the liege's ultimatum
//     (10 days to pay). Unpaid on its day: a loyal village (40+) stands with its lord, who is named rebel: the banner is
//     raised (any oath to the liege's army ends first), the liege's tension goes to Skirmishes and its army besieges the
//     seat; a village that has lost faith opens its gates and the lordship is taken back.
//   - once independent the old liege's anger climbs a rung of the realm's ladder every ~2 weeks (Strained .. Skirmishes)
//     until its army besieges the seat (Realm::forceSiege: the M4 siege, BREAK THE SIEGE with the militia, who take the
//     walls at once). A broken siege shakes the liege (its exhaustion): its envoy offers peace (the steward's ACCEPT,
//     Realm::forcePeace); a lost seat is a lost holding (lord_game.cpp ends the lordship gracefully with the last one).
//   - the player's own march (a conquest) is watched here too: its end told, the conquest adopted by lord_game.cpp.
#include <algorithm>
#include <cmath>
#include "engine/audio.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/lord_ops.h"
#include "rpg/sim/service.h"
#include "rpg/sim/service_ops.h"
#include "rpg/world/source.h"

namespace {
uint16_t dayU16(int day) { return (uint16_t)std::clamp(day, 0, 65535); }
std::string realmName(const Game& g, ew::Gid k) {
  if (const realm::KingdomState* K = g.realm.kingdom(k)) return K->name;
  if (g.world.src) if (const ew::KingdomPlan* kp = g.world.src->kingdom(k)) return kp->name;
  return "THE CROWN";
}
std::string siteName(const Game& g, ew::Gid site) {
  const int si = g.world.siteHandle(site);
  return si >= 0 ? g.world.sites[(size_t)si].name : std::string("THE VILLAGE");
}
const realm::Siege* siegeById(const Game& g, uint32_t id) {
  for (const realm::Siege& s : g.realm.sieges()) if (s.id == id) return &s;
  return nullptr;
}
ew::Gid oldLiege(const Game& g) {
  const realm::KingdomState* P = g.realm.kingdom(g.lordship.realm);
  return P ? P->parent : 0;
}
// the liege's army besieges the seat now (the militia take the walls)
uint32_t besiegeSeat(Game& g, ew::Gid liege) {
  lord::Lordship& L = g.lordship;
  if (L.holdings.empty() || !liege) return 0;
  const ew::Gid seat = L.holdings[0].site;
  const realm::KingdomState* A = g.realm.kingdom(liege);
  if (!A || A->fallen) return 0;
  const uint32_t id = g.realm.forceSiege(seat, liege, g.day);
  if (!id) return 0;
  L.siege = id;
  if (L.holdings[0].militia) g.realm.contribute(id, 2, 0.5f * L.holdings[0].militia);
  const std::string line = realmName(g, liege) + "'S ARMY BESIEGES " + siteName(g, seat) + ". BREAK THE SIEGE!";
  LordOps::tell(g, line);
  LordOps::rise(g, RiseEv::TributeRefused, 2, line, liege);
  g.realmSync();
  return id;
}
}  // namespace

std::string LordOps::freeCost(const Game& g) {
  const lord::Lordship& L = g.lordship;
  std::string s = realmName(g, L.liege) + " WILL CALL YOU REBEL, AND ITS ARMY WILL COME FOR " + siteName(g, L.holdings.empty() ? 0 : L.holdings[0].site) + ".";
  if (g.service.enlisted() && g.service.sworn == L.liege) {
    const bool honour = g.service.rank >= service::Rank::Captain || g.day - (int)g.service.enlistDay >= 60;
    s += honour ? " YOUR OATH TO ITS ARMY ENDS WITH HONOUR." : " YOU WILL BREAK YOUR OATH TO ITS ARMY: A DESERTER.";
  }
  s += " NO MORE TRIBUTE.";
  return s;
}

void LordOps::releaseService(Game& g) {
  const lord::Lordship& L = g.lordship;
  if (!g.service.enlisted() || !L.liege || g.service.sworn != L.liege) return;
  ServiceOps::leave(g, false);   // honourable for a captain or after 60 days served, else desertion (service_game.cpp)
}

void LordOps::refused(Game& g) {
  lord::Lordship& L = g.lordship;
  if (L.realm || !L.liege) return;
  if (L.missed >= 2 && L.wrath == 0) {
    L.wrath = 1;
    L.wrathDay = dayU16(g.day + 10);
    const std::string line = realmName(g, L.liege) + " DEMANDS ITS TRIBUTE: PAY WITHIN 10 DAYS, OR ANSWER FOR IT.";
    g.say(line);
    rise(g, RiseEv::TributeRefused, 1, line, L.liege);
  }
}

bool LordOps::forceWrathSiege(Game& g) {
  lord::Lordship& L = g.lordship;
  if (L.holdings.empty()) return false;
  if (L.siege) if (const realm::Siege* s = siegeById(g, L.siege)) if (!s->over) return true;
  const ew::Gid liege = L.realm ? oldLiege(g) : L.liege;
  if (!L.realm) {
    // a sworn lord's village cannot be besieged by its own liege: defiance first (the village stands with its lord)
    std::string why;
    if (!independence(g, "", why)) return false;
  }
  g.realm.forceTension(liege, L.realm, realm::Tension::Skirmishes, realm::WarCause::Revenge, g.day);
  L.wrath = 4;
  return besiegeSeat(g, liege) != 0;
}

void LordOps::wrathStep(Game& g) {
  lord::Lordship& L = g.lordship;
  if (L.holdings.empty()) return;
  // ---- the liege's siege of the seat
  if (L.siege) {
    const realm::Siege* s = siegeById(g, L.siege);
    if (!s || s->over) {
      const bool broken = s && !s->attackerWon;
      const ew::Gid liege = s ? s->attacker : 0;
      L.siege = 0;
      if (broken) {
        const std::string line = "THE SIEGE OF " + siteName(g, s->site) + " IS BROKEN. " + realmName(g, liege) + "'S ARMY FALLS BACK.";
        g.say(line);
        rise(g, RiseEv::OrderDone, 0, line, L.realm);
        if (lord::Holding* h = L.holding(s->site)) h->loyalty = (uint8_t)std::min(100, h->loyalty + 10);
        // a broken army: the liege's envoy comes with terms
        if (L.realm && liege == oldLiege(g)) L.peaceOffer = 1;
        L.wrathDay = dayU16(g.day + 30);
        L.wrath = 2;
      }
      // (a won siege: the seat changed hands; lord_game.cpp's step loses the holding)
    }
  }
  // ---- the player's own march
  if (L.march) {
    const realm::Siege* s = siegeById(g, L.march);
    if (!s || s->over) {
      if (s && !s->attackerWon) g.say("THE ASSAULT ON " + siteName(g, s->site) + " FAILED. YOUR MEN COME HOME.");
      L.march = 0;
    }
  }
  if (!L.wrathDay || g.day < (int)L.wrathDay) return;
  // ---- a sworn lord's ultimatum ran out
  if (!L.realm) {
    if (!L.liege || L.missed < 2) { L.wrath = 0; L.wrathDay = 0; return; }
    lord::Holding& seat = L.holdings[0];
    const ew::Gid liege = L.liege;
    if (seat.loyalty >= 40) {
      g.say(siteName(g, seat.site) + " STANDS WITH ITS LORD AGAINST " + realmName(g, liege) + ".");
      forceWrathSiege(g);
    } else {
      // the people open the gates to the liege's men: the lordship is taken back
      g.say("THE PEOPLE OF " + siteName(g, seat.site) + " OPEN THEIR GATES TO " + realmName(g, liege) + ". YOUR LORDSHIP IS TAKEN BACK.");
      for (const lord::Holding& h : L.holdings) g.realm.revokeFief(h.site, g.day);
      const uint32_t next = L.nextTask;
      L = lord::Lordship();
      L.nextTask = next;
      sync(g);
      restamp(g);
    }
    return;
  }
  // ---- the old liege's war for its land
  const ew::Gid liege = oldLiege(g);
  const realm::KingdomState* A = g.realm.kingdom(liege);
  if (!A || A->fallen) { L.wrath = 0; L.wrathDay = 0; return; }   // nobody left to be angry
  if (L.peaceDay && (int)L.peaceDay >= (int)L.independenceDay) { L.wrath = 0; L.wrathDay = 0; return; }
  // a liege worn out by its wars sends terms instead of soldiers
  if (A->exhaustion >= 0.5f) {
    if (!L.peaceOffer) { L.peaceOffer = 1; g.say(realmName(g, liege) + "'S ENVOY HAS COME TO " + siteName(g, L.holdings[0].site) + " WITH TERMS OF PEACE."); }
    L.wrathDay = dayU16(g.day + 20);
    return;
  }
  const realm::Relation* r = g.realm.relation(liege, L.realm);
  const realm::Tension t = r ? r->tension : realm::Tension::Calm;
  if (t < realm::Tension::Skirmishes && !(r && r->state == realm::Rel::War)) {
    const realm::Tension next = (realm::Tension)std::min<int>((int)realm::Tension::Skirmishes, (int)t + 1);
    g.realm.forceTension(liege, L.realm, next, realm::WarCause::Succession, g.day);
    static const char* rung[] = {"", "GROWS COLD", "BREAKS ALL TRADE", "MASSES ON THE BORDER", "RAIDS THE BORDER"};
    g.say(realmName(g, liege) + " " + rung[(int)next] + ": IT HAS NOT FORGIVEN " + L.realmName + ".");
    L.wrath = (uint8_t)std::min(250, L.wrath + 1);
    L.wrathDay = dayU16(g.day + 12 + (int)(ew::mix64(L.realm ^ (uint64_t)g.day) % 6));
    return;
  }
  // Skirmishes (or war): the siege of the seat
  if (!L.siege) besiegeSeat(g, liege);
  L.wrathDay = dayU16(g.day + 40);
}
