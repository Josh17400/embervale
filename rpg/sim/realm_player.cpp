// M12 "Rise and Fall": the player's lordship and realm as the realm sees them (rpg/sim/realm.h; VISION_PLAN 4.7
// "Kingdom-making", 15.6.4, 15.33). LORD lane (phase A by the lead). The lordship block (rpg/sim/lordship.h) keeps the
// hall's coffers, taxes, the steward and the fief's works; the realm keeps the political facts: who holds the village,
// whom the player answers to (Renown::liege), the player's own realm (KO_PLAYER, Renown::realmOf), tribute between
// realms. The REALM lane's tick treats a KO_PLAYER realm as the player's: it never declares its wars, kills its ruler,
// makes its peace or splits it.
#include <algorithm>
#include "rpg/culture/society.h"
#include "rpg/sim/realm.h"
#include "rpg/world/source.h"

namespace realm {
namespace {
uint16_t dayU16(int day) { return (uint16_t)std::clamp(day, 0, 65535); }
}  // namespace

bool Realm::grantFief(Gid site, Gid liege, uint8_t how, int day) {
  const SettlementState* s = settlement(site);
  if (!s || (s->flags & SS_BESIEGED)) return false;
  // a liege grants only what it holds; a ruin resettled (3) or a conquest (4) is the player's to hold under them
  if (how <= 2 && s->owner != liege) return false;
  if (renown_.realmOf) {
    // already independent: the place joins the player's realm
    playerTakes(site, day);
  } else {
    if (liege && s->owner != liege) {
      SettlementState& m = state(site);
      giveSite(m, liege, day);
      if (KingdomState* L = kingdomMut(liege)) recount(*L);
    }
    renown_.liege = liege;
  }
  if (!renown_.lordOf) renown_.lordOf = site;
  const SettlementState* t = settlement(site);
  addEvent(EvType::FiefGranted, liege, 0, site, t->gx, t->gy, day).mag = how;
  return true;
}

void Realm::revokeFief(Gid site, int day) {
  (void)day;
  if (renown_.lordOf == site) renown_.lordOf = 0;
  if (!renown_.lordOf && !renown_.realmOf) renown_.liege = 0;
}

Gid Realm::declareIndependence(const std::string& name, const std::string& rulerName, bool female, uint32_t color,
                               uint32_t color2, uint8_t emblem, int day) {
  if (renown_.realmOf) return renown_.realmOf;
  if (!renown_.lordOf || !settlement(renown_.lordOf)) return 0;
  const Gid liege = renown_.liege;
  const Gid seat = renown_.lordOf;
  const Gid rid = foundRealm(KO_PLAYER, 0, {seat}, name.empty() ? std::string("THE FREE MARCH") : name, color, color2, emblem, day);
  if (!rid) return 0;
  KingdomState* K = kingdomMut(rid);
  K->ruler.name = rulerName;
  K->ruler.female = female;
  K->ruler.title = female ? "LADY" : "LORD";
  K->ruler.house = "HOUSE " + rulerName;
  K->ruler.traits = RT_HONOURABLE;
  // (LORD lane) a new lordship rules as a crown of its own, not with the old liege's government (a liege's
  // MerchantRepublic read "REPUBLIC OF ..."): the player's lords and later kings
  K->gov = (uint8_t)cult::Government::Monarchy;
  K->capital = seat;
  K->parent = liege;
  renown_.realmOf = rid;
  renown_.liege = 0;
  if (Relation* r = relMut(liege, rid, true)) {
    r->state = Rel::Rivalry;
    r->aggrieved = liege;
    r->cause = WarCause::Succession;
    r->opinion = -50;
    r->base = -30;
    setTension(*r, Tension::TradeBroken, day);
  }
  const SettlementState* s = settlement(seat);
  addEvent(EvType::Independence, rid, liege, seat, s->gx, s->gy, day);
  return rid;
}

void Realm::payTribute(Gid from, Gid to, float wealth, int day) {
  KingdomState* T = kingdomMut(to);
  if (!T) return;
  T->wealth += std::max(0.0f, wealth);
  if (KingdomState* F = kingdomMut(from)) F->wealth -= std::max(0.0f, wealth);
  T = kingdomMut(to);
  // (M12 fixer) the player's tribute (from 0) is news of the player's own village, not of a far realm's capital
  const SettlementState* seat = !from && renown_.lordOf ? settlement(renown_.lordOf) : nullptr;
  if (seat) addEvent(EvType::TributePaid, 0, to, seat->site, seat->gx, seat->gy, day).mag = (int16_t)std::clamp((int)(wealth * 100), 0, 32767);
  else addEvent(EvType::TributePaid, from, to, T->capital, capX(*T), capY(*T), day).mag = (int16_t)std::clamp((int)(wealth * 100), 0, 32767);
}

void Realm::refuseTribute(Gid from, Gid to, int day, bool defiant) {
  const KingdomState* T = kingdom(to);
  if (!T) return;
  if (from) {
    if (Relation* r = relMut(from, to, true)) {
      if (r->state == Rel::Unknown) r->state = Rel::Contact;
      r->aggrieved = to;
      r->cause = WarCause::Revenge;
      if (r->tension < Tension::Skirmishes) setTension(*r, (Tension)((int)r->tension + 1), day);
    }
  }
  const SettlementState* seat = !from && renown_.lordOf ? settlement(renown_.lordOf) : nullptr;
  if (seat) addEvent(EvType::TributeRefused, 0, to, seat->site, seat->gx, seat->gy, day).mag = defiant ? 1 : 0;
  else addEvent(EvType::TributeRefused, from, to, T->capital, capX(*T), capY(*T), day);
}

void Realm::playerTakes(Gid site, int day) {
  const Gid rid = renown_.realmOf;
  if (!rid || !settlement(site)) return;
  if (state(site).owner != rid) {
    War* w = nullptr;
    const Gid was = state(site).owner;
    for (War& x : wars_)
      if (!x.endDay && ((x.attacker == rid && x.defender == was) || (x.attacker == was && x.defender == rid))) w = &x;
    capture(site, rid, day, w);
  }
  // (LORD lane) the crown follows the land: LORD / LADY of a lordship, KING / QUEEN once it holds 3 settlements (a
  // call on a site the realm already holds only refreshes the title; the LORD lane makes it after every conquest)
  if (KingdomState* K = kingdomMut(rid)) {
    recount(*K);
    const bool king = K->settlements.size() >= 3;
    K->ruler.title = king ? (K->ruler.female ? "QUEEN" : "KING") : (K->ruler.female ? "LADY" : "LORD");
  }
}

void Realm::playerRealmEnds(int day) {
  const Gid rid = renown_.realmOf;
  KingdomState* K = kingdomMut(rid);
  if (!K || K->fallen) return;
  const Gid by = K->parent;
  // fallKingdom keeps a player's realm standing (the LORD lane decides): this is that decision. The flag stays set so
  // the fallen realm is never revived as a rebel cause; its wars end with it.
  K->player = false;
  fallKingdom(*K, day, by);
  if (KingdomState* K2 = kingdomMut(rid)) K2->player = true;
  renown_.realmOf = 0;
  if (!renown_.lordOf) renown_.liege = 0;
}

}  // namespace realm
