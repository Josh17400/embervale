// M12 "Rise and Fall": the realm's new records and forcing calls (rpg/sim/realm.h, VISION_PLAN 4.3 "Rise", 15.33).
// REALM lane (phase A by the lead: the records, the forcing calls and the diplomacy rolls the SERVICE lane's quests
// drive). Phase B: the forcing calls share their bodies with the tick (rpg/sim/realm_deep.cpp grows these by itself):
//   raiseWarlord    a camp's warband takes a village: a KO_WARLORD realm (horde-like: raids, tribute demands)
//   crownSuccessor  a fallen realm's remnant crowns a scion of the old house in one of its old towns: KO_SUCCESSOR,
//                   the old field colour with a new charge, a claim on the fallen realm's lands
//   resettle        settlers go back to a ruin (SS_REBUILDING | SS_RESETTLED; refugees of the place go home)
//   makeVassal      a beaten or frightened realm bends the knee (Rel::Vassal, a yearly tribute)
//   lairRaid        a lair's beast falls on a settlement (one schedule: the lair's; it rests after a raid or sleeps)
#include <algorithm>
#include <cmath>
#include "rpg/culture/culture.h"
#include "rpg/sim/realm.h"
#include "rpg/world/source.h"

namespace realm {
namespace {
uint16_t dayU16(int day) { return (uint16_t)std::clamp(day, 0, 65535); }
int64_t dist2(int32_t ax, int32_t ay, int32_t bx, int32_t by) { const int64_t dx = ax - bx, dy = ay - by; return dx * dx + dy * dy; }
uint32_t colour(int r, int g, int b) { return (uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16 | 0xFF000000u; }
const uint32_t PAL[] = {colour(150, 30, 30), colour(30, 60, 140), colour(30, 110, 50), colour(120, 40, 120), colour(200, 150, 30),
                        colour(40, 40, 40), colour(200, 200, 190), colour(170, 80, 20), colour(20, 120, 130), colour(90, 60, 30)};
constexpr int LAIR_REACH = 220;   // tiles: a world boss's raids reach about 3/4 of its territory (foes::WorldBoss::range 220-320)
constexpr int LAIR_FIRST = 60, LAIR_SPREAD = 14000;   // days: a lair the realm first hears of wakes within this window
constexpr int LAIR_SLEEP = 8000, LAIR_SLEEP_SPREAD = 12000;   // days: after its season a beast sleeps this long
}  // namespace

// ---------------------------------------------------------------- camps and lairs
void Realm::noteCamp(Gid site, int32_t gx, int32_t gy, Gid land, int day) {
  if (!site) return;
  auto it = std::lower_bound(camps_.begin(), camps_.end(), site, [](const Camp& c, Gid s) { return c.site < s; });
  if (it != camps_.end() && it->site == site) return;
  Camp c;
  c.site = site; c.gx = gx; c.gy = gy; c.land = land;
  c.flags = land ? 0 : CF_WILD;
  c.strength = (uint8_t)(10 + ew::mix64(site ^ seed_) % 20);
  c.notedDay = dayU16(day);
  camps_.insert(it, c);
  if (camps_.size() > 64) {
    // bounded: forget a broken camp first, else the one farthest from where the player is (never a warlord's)
    size_t drop = camps_.size();
    int64_t far = -1;
    for (size_t i = 0; i < camps_.size(); i++) {
      const Camp& x = camps_[i];
      if (x.site == site || (x.flags & CF_WARLORD)) continue;
      const int64_t d = (x.flags & CF_CLEARED) ? INT64_MAX : dist2(x.gx, x.gy, gx, gy);
      if (d > far) { far = d; drop = i; }
    }
    if (drop < camps_.size()) camps_.erase(camps_.begin() + (std::ptrdiff_t)drop);
  }
}
void Realm::campCleared(Gid site, int day) {
  for (Camp& c : camps_)
    if (c.site == site && !(c.flags & CF_WARLORD)) { c.flags |= CF_CLEARED; c.flags &= (uint8_t)~CF_WARBAND; c.strength = 0; c.clearedDay = dayU16(day); }
}
const Camp* Realm::camp(Gid site) const {
  auto it = std::lower_bound(camps_.begin(), camps_.end(), site, [](const Camp& c, Gid s) { return c.site < s; });
  return it != camps_.end() && it->site == site ? &*it : nullptr;
}
// A lair sleeps (wokeDay 0; nextRaid: the day it wakes) or is awake (wokeDay: the day it woke; nextRaid: its next raid).
// A lair the realm first hears of is already in its own rhythm: about one in three awake, the rest asleep for a while.
void Realm::noteLair(Gid site, int32_t gx, int32_t gy, uint8_t mon, uint8_t level, int day) {
  if (!site) return;
  auto it = std::lower_bound(lairs_.begin(), lairs_.end(), site, [](const Lair& c, Gid s) { return c.site < s; });
  if (it != lairs_.end() && it->site == site) return;
  Lair L;
  L.site = site; L.gx = gx; L.gy = gy; L.mon = mon; L.level = level;
  // the great beasts sleep for years: a lair wakes about once in forty years, raids a season and sleeps again (the
  // land hears of two or three raids a year in all: VISION_PLAN 15.6.3, slow and realistic)
  auto sleepUntil = [&](Gid s, int from) { return dayU16(from + LAIR_FIRST + (int)((ew::mix64(s ^ seed_ ^ 0x4C414952ull) >> 8) % LAIR_SPREAD)); };
  L.wokeDay = 0;
  L.nextRaid = sleepUntil(site, day);
  lairs_.insert(it, L);
  if (lairs_.size() > 32) {
    // bounded: a slain beast's lair first, else the farthest
    size_t drop = 0;
    int64_t far = -1;
    for (size_t i = 0; i < lairs_.size(); i++) {
      if (lairs_[i].site == site) continue;
      const int64_t d = lairs_[i].slain ? INT64_MAX : dist2(lairs_[i].gx, lairs_[i].gy, gx, gy);
      if (d > far) { far = d; drop = i; }
    }
    lairs_.erase(lairs_.begin() + (std::ptrdiff_t)drop);
  }
  // when the hero arrives one beast is abroad: of the lairs the realm hears of on the first days, the one nearest the
  // start with a settlement in its reach is awake, its first raid a week or a few away (the first news of the beasts)
  if (day <= 2 && src_ && src_->seed() == seed_) {
    const ew::GTile sp = src_->start().spawn;
    Lair* me = nullptr;
    Lair* cur = nullptr;
    for (Lair& x : lairs_) {
      if (x.site == site) me = &x;
      else if (x.wokeDay && x.wokeDay <= 2 && x.raids == 0) cur = &x;
    }
    if (me && lairTarget(*me) && (!cur || dist2(me->gx, me->gy, sp.x, sp.y) < dist2(cur->gx, cur->gy, sp.x, sp.y))) {
      if (cur) { cur->wokeDay = 0; cur->nextRaid = sleepUntil(cur->site, day); }
      me->wokeDay = dayU16(std::max(1, day));
      me->nextRaid = dayU16(day + 6 + (int)((ew::mix64(site ^ seed_ ^ 0x4C414952ull) >> 8) % 30));
    }
  }
}
void Realm::lairSlain(Gid site, int day) {
  (void)day;
  for (Lair& L : lairs_) if (L.site == site) L.slain = true;
}
const Lair* Realm::lair(Gid site) const {
  auto it = std::lower_bound(lairs_.begin(), lairs_.end(), site, [](const Lair& c, Gid s) { return c.site < s; });
  return it != lairs_.end() && it->site == site ? &*it : nullptr;
}
Gid Realm::lairTarget(const Lair& L) const {
  const Gid startVillage = src_ && src_->seed() == seed_ ? src_->start().village : 0;   // (as M6: the start village is spared)
  const SettlementState* best = nullptr;
  int64_t bd = (int64_t)LAIR_REACH * LAIR_REACH;
  for (const SettlementState& s : settlements_) {
    if (s.site == startVillage || (s.flags & (SS_ABANDONED | SS_RUINED))) continue;
    const int64_t d = dist2(s.gx, s.gy, L.gx, L.gy);
    if (d > bd) continue;
    if (d == bd && best && best->site < s.site) continue;
    bd = d; best = &s;
  }
  return best ? best->site : 0;
}
void Realm::lairRaid(Gid lair, Gid site, int damage, int day) {
  uint8_t mon = 0;
  for (Lair& L : lairs_) {
    if (L.site != lair) continue;
    mon = L.mon;
    L.raids = (uint16_t)std::min(65535, L.raids + 1);
    L.lastTarget = site;
    if (!L.wokeDay) L.wokeDay = dayU16(std::max(1, day));
    // after a raid the beast rests a few weeks, or (two times in five) goes back to sleep for many years
    if (roll() % 5 < 2) { L.wokeDay = 0; L.nextRaid = dayU16(day + LAIR_SLEEP + (int)(roll() % LAIR_SLEEP_SPREAD)); }
    else L.nextRaid = dayU16(day + 18 + (int)(roll() % 22));
  }
  beastRaid(site, mon, damage, day);
}
Gid Realm::lairRaidedNear(int day, int32_t gx, int32_t gy, int nearTiles, Gid& site) const {
  site = 0;
  for (size_t e = events_.size(); e-- > 0;) {
    const WorldEvent& w = events_[e];
    if ((int)w.day != day) { if ((int)w.day < day) break; continue; }
    if (w.type != EvType::BeastRaid || dist2(w.gx, w.gy, gx, gy) > (int64_t)nearTiles * nearTiles) continue;
    for (const Lair& L : lairs_)
      if (!L.slain && L.lastTarget == w.site && L.mon == (uint8_t)w.mag) { site = w.site; return L.site; }
  }
  return 0;
}

// ---------------------------------------------------------------- new realms
Gid Realm::newRealmId(Gid near) {
  Gid rid = 0;
  for (int tries = 0; tries < 0x800 && (!rid || kingdom(rid)); tries++)
    rid = ew::makeId(ew::idRx(near), ew::idRy(near), ew::IdKind::Kingdom, 0x800u + (nextRebel_++ % 0x800u));
  return kingdom(rid) ? 0 : rid;
}

Gid Realm::foundRealm(uint8_t origin, Gid parent, const std::vector<Gid>& sites, const std::string& name, uint32_t color,
                      uint32_t color2, uint8_t emblem, int day) {
  std::vector<Gid> have;
  for (Gid g : sites) if (settlement(g)) have.push_back(g);
  if (have.empty()) return 0;
  const SettlementState* s0 = settlement(have[0]);
  const Gid nearId = parent ? parent : ew::makeId(ew::EndlessSource::kcellOf(s0->gx), ew::EndlessSource::kcellOf(s0->gy), ew::IdKind::Kingdom, 0);
  const Gid rid = newRealmId(nearId);
  if (!rid) return 0;
  const KingdomState* P = kingdom(parent);
  const SettlementState* home0 = settlement(have[0]);
  const KingdomState* H = P ? P : kingdom(home0->home);
  if (!H) H = kingdom(home0->owner);
  KingdomState R;
  R.id = rid;
  R.rebel = true;
  R.origin = origin;
  R.parent = parent;
  R.player = origin == KO_PLAYER;
  if (H) {
    R.culture = H->culture; R.gov = H->gov; R.inherit = H->inherit;
    for (int v = 0; v < V_COUNT; v++) R.values[v] = H->values[v];
    R.ruler.title = H->ruler.title;
    R.food = H->food; R.harvestDoy = H->harvestDoy; R.harvestYear = H->harvestYear; R.lastHarvest = H->lastHarvest;
    R.isolated = H->isolated; R.hiddenLand = H->hiddenLand; R.contactDay = H->contactDay;
  } else if (src_ && src_->seed() == seed_) {
    R.culture = src_->cultureAt(home0->gx, home0->gy);   // (a frontier village: the land's own people)
  }
  const uint64_t h = ew::mix64(rid ^ seed_ ^ 0x52495345ull);
  const bool named = src_ && src_->seed() == seed_ && R.culture;
  R.name = name;
  if (R.name.empty()) {
    if (named) {
      R.name = cult::kingdomName(src_->culture(R.culture), (uint32_t)h);
      for (int t = 1; t < 4 && (H && R.name == H->name); t++) R.name = cult::kingdomName(src_->culture(R.culture), (uint32_t)(h >> (8 * t)) ^ 0x51u);
    } else {
      R.name = origin == KO_WARLORD ? "THE WARLORD'S MARCH" : "THE FREE MARCH";
    }
  }
  if (named && !R.player) {
    const cult::Culture& C = src_->culture(R.culture);
    R.ruler.female = (h & 3) == 0;
    R.ruler.name = cult::personName(C, (uint32_t)(h >> 8), R.ruler.female);
    R.ruler.house = "HOUSE " + cult::personName(C, (uint32_t)(h >> 28) ^ 0x484F5553u, false);
  } else if (!R.player) {
    R.ruler.name = origin == KO_WARLORD ? "THE WARLORD" : "THE PRETENDER";
    R.ruler.house = origin == KO_WARLORD ? "THE WARBAND" : "HOUSE OF THE REVOLT";
  }
  if (R.ruler.title.empty()) R.ruler.title = "LORD";
  if (origin == KO_WARLORD) R.ruler.title = "WARLORD";
  R.color = color ? color : PAL[h % 10];
  R.color2 = color2 ? color2 : PAL[(h >> 8) % 10];
  if (R.color2 == R.color) R.color2 = PAL[(h + 5) % 10];
  R.emblem = emblem ? emblem : (uint8_t)((h >> 16) % 8);
  R.ruler.age = (uint8_t)(25 + (h >> 36) % 25);
  R.ruler.traits = (uint8_t)(detail::traitsFor(R.values, h) | (origin == KO_WARLORD ? RT_AGGRESSIVE : 0));
  R.ruler.sinceDay = dayU16(std::max(1, day));
  R.ruler.dynasty = ew::mix64(h ^ 0x44594E41ull);
  R.foundedDay = day;
  R.tier = Tier::Active;
  R.lastTick = dayU16(day);
  R.stability = 0.5f;
  R.military = origin == KO_WARLORD ? 3.0f : 2.0f;
  R.wealth = 2.0f;
  kingdoms_.push_back(std::move(R));
  rebuildIndex();
  std::vector<Gid> losers;
  for (Gid g : have) {
    SettlementState& s = state(g);
    if (s.owner && s.owner != rid) losers.push_back(s.owner);
    giveSite(s, rid, day);
    s.flags &= (uint16_t)~(SS_OCCUPIED | SS_ABANDONED | SS_RUINED);
  }
  if (KingdomState* K = kingdomMut(rid)) recount(*K);
  std::sort(losers.begin(), losers.end());
  losers.erase(std::unique(losers.begin(), losers.end()), losers.end());
  for (Gid l : losers) {
    if (KingdomState* L = kingdomMut(l)) {
      recount(*L);
      Relation* r = relMut(l, rid, true);
      if (r) { r->state = Rel::Rivalry; r->base = -40; r->opinion = -50; r->aggrieved = l; r->cause = WarCause::Succession; r->stepDay = dayU16(day); }
      L = kingdomMut(l);
      if (L && L->settlements.empty() && !L->fallen) fallKingdom(*L, day, rid);
    }
  }
  stats.instantiated = (int)kingdoms_.size();
  return rid;
}

Gid Realm::raiseWarlord(Camp& c, Gid site, int day) {
  const SettlementState* t = settlement(site);
  if (!t) return 0;
  const Gid was = t->owner;
  const Gid campSite = c.site;
  const int strength = c.strength;
  const Gid rid = foundRealm(KO_WARLORD, 0, {site}, "", 0, 0, 0, day);
  if (!rid) return 0;
  for (Camp& x : camps_) if (x.site == campSite) { x.flags |= CF_WARLORD; x.flags &= (uint8_t)~CF_WARBAND; x.realm = rid; x.grewDay = dayU16(day); }
  KingdomState* K = kingdomMut(rid);
  K->camp = campSite;
  K->military = 2.0f + strength / 30.0f;
  K->wealth = 1.0f;
  K->stability = 0.45f;
  // the realm it took the village from wants it back, the bandits being bandits (the road to war's first rung)
  if (Relation* r = relMut(was, rid, true)) {
    r->base = -55;
    r->opinion = -60;
    r->aggrieved = was;
    r->cause = WarCause::Raiders;
    r->truceUntil = 0;
    setTension(*r, Tension::Strained, day);
  }
  // its neighbours meet it warily
  const SettlementState* s = settlement(site);
  for (size_t i = 0; i < kingdoms_.size(); i++) {
    const KingdomState& k = kingdoms_[i];
    if (k.id == rid || k.id == was || k.fallen || k.tier != Tier::Active || k.player) continue;
    if (dist2(capX(k), capY(k), s->gx, s->gy) > 1400ll * 1400) continue;
    const KingdomState* W = kingdom(rid);
    if (!W || apart(k, *W)) continue;
    Relation* r = relMut(k.id, rid, true);
    if (!r || r->state != Rel::Unknown) continue;
    r->state = Rel::Contact;
    r->base = -30;
    r->opinion = -30;
    r->stepDay = dayU16(day);
  }
  s = settlement(site);
  WorldEvent& e = addEvent(EvType::WarlordRose, rid, was, site, s->gx, s->gy, day);
  e.mag = (int16_t)strength;
  return rid;
}

Gid Realm::crownSuccessor(Gid fallen, Gid site, int day) {
  const KingdomState* F = kingdom(fallen);
  const SettlementState* t = settlement(site);
  if (!F || !t) return 0;
  const uint32_t fcol = F->color, fcol2 = F->color2;
  const uint8_t femb = F->emblem;
  const std::string fhouse = F->ruler.house, ftitle = F->ruler.title;
  const uint64_t h = ew::mix64(fallen ^ site ^ seed_ ^ 0x53554343ull);
  // the old field colour, a new charge and emblem
  uint32_t c2 = PAL[h % 10];
  for (int t2 = 1; t2 < 6 && (c2 == fcol || c2 == fcol2); t2++) c2 = PAL[(h + (uint64_t)t2 * 3) % 10];
  const uint8_t emb = (uint8_t)((femb + 1 + (h >> 8) % 6) % 8);
  const uint8_t type = t->type;
  const Gid rid = foundRealm(KO_SUCCESSOR, fallen, {site}, "", fcol, c2, emb ? emb : 1, day);
  if (!rid) return 0;
  KingdomState* K = kingdomMut(rid);
  K->claim = fallen;
  if (!fhouse.empty()) K->ruler.house = fhouse;   // a scion of the fallen house takes the old title
  if (!ftitle.empty()) K->ruler.title = ftitle;
  K->military = type == (uint8_t)SiteType::City ? 5.0f : type == (uint8_t)SiteType::Town ? 2.6f : 1.6f;
  K->wealth = 1.5f;
  K->stability = 0.55f;
  // whoever holds the rest of the old realm's land faces a claimant (the road to a war of reunification)
  std::vector<Gid> all;
  for (const SettlementState& s : settlements_)
    if (s.home == fallen && s.owner && s.owner != rid && !(s.flags & (SS_ABANDONED | SS_RUINED))) all.push_back(s.owner);
  std::sort(all.begin(), all.end());
  std::vector<Gid> holders = all;
  holders.erase(std::unique(holders.begin(), holders.end()), holders.end());
  // the realm that holds most of the old land is the claimant's quarrel; the others are only eyed coldly
  Gid main = 0;
  size_t most = 0;
  for (Gid o : holders) {
    const size_t n = (size_t)std::count(all.begin(), all.end(), o);
    if (n > most) { most = n; main = o; }
  }
  for (Gid o : holders) {
    const KingdomState* O = kingdom(o);
    const KingdomState* S = kingdom(rid);
    if (!O || !S || O->fallen || O->player || apart(*O, *S)) continue;
    Relation* r = relMut(o, rid, true);
    if (!r) continue;
    if (r->state == Rel::Unknown || r->state == Rel::Contact) r->state = Rel::Rivalry;
    r->base = (int8_t)std::min<int>(r->base, -35);
    r->opinion = (int8_t)std::min<int>(r->opinion, -45);
    if (o == main && r->tension == Tension::Calm) {
      r->aggrieved = rid;
      r->cause = WarCause::Succession;
      r->truceUntil = 0;
      setTension(*r, Tension::Strained, day);
    }
  }
  const SettlementState* s = settlement(site);
  addEvent(EvType::SuccessorRose, rid, fallen, site, s->gx, s->gy, day).mag = (int16_t)type;
  return rid;
}

void Realm::resettle(SettlementState& s0, Gid by, int day) {
  const Gid site = s0.site;
  const Gid was = s0.owner;
  {
    SettlementState& s = state(site);
    s.flags &= (uint16_t)~(SS_ABANDONED | SS_RUINED | SS_BURNED | SS_FAMINE | SS_OCCUPIED | SS_UNREST | SS_REFUGEES | SS_BESIEGED);
    s.flags |= SS_REBUILDING | SS_RESETTLED;
    s.popPct = 30; s.damage = std::max<uint8_t>(s.damage, 40); s.food = 40; s.mood = 50;
    if (was != by) giveSite(s, by, day);
    s.changedDay = dayU16(day);
  }
  // its people who sheltered elsewhere come home
  int back = 0;
  for (SettlementState& o : settlements_)
    if ((o.flags & SS_REFUGEES) && o.refugeesFrom == site) { o.flags &= (uint16_t)~SS_REFUGEES; o.refugeesFrom = 0; o.changedDay = dayU16(day); back++; }
  SettlementState& s = state(site);
  if (back) s.popPct = (uint8_t)std::min(60, s.popPct + 12 * back);
  ruins_.erase(site);   // (the memo only: the SimRuin stays, the place's Lost History)
  addEvent(EvType::Resettled, by, was, site, s.gx, s.gy, day).mag = (int16_t)back;
  if (KingdomState* K = kingdomMut(by)) recount(*K);
  if (was && was != by)
    if (KingdomState* O = kingdomMut(was)) { recount(*O); if (O->settlements.empty() && !O->fallen) fallKingdom(*O, day, by); }
}

void Realm::makeVassal(Gid vassal, Gid overlord, int day) {
  KingdomState* V = kingdomMut(vassal);
  const KingdomState* O = kingdom(overlord);
  if (!V || !O || vassal == overlord || apart(*V, *O) || V->fallen || O->fallen || V->overlord == overlord) return;
  V->overlord = overlord;
  V->tribute = std::max(0.5f, V->pop * 0.5f);
  Relation* r = relMut(vassal, overlord, true);
  if (r) {
    r->state = Rel::Vassal;   // (a, b order: the state names the bond; KingdomState::overlord says who serves whom)
    r->tension = Tension::Calm;
    r->aggrieved = 0;
    r->marchDay = 0;
    r->sinceDay = dayU16(day);
    r->stepDay = dayU16(day);
  }
  V = kingdomMut(vassal);
  addEvent(EvType::Vassalised, vassal, overlord, V->capital, capX(*V), capY(*V), day);
}

// ---------------------------------------------------------------- forcing
Gid Realm::forceWarlord(Gid campSite, int day) {
  Camp* c = nullptr;
  for (Camp& x : camps_) if (x.site == campSite) c = &x;
  if (!c || (c->flags & (CF_WARLORD | CF_CLEARED))) return 0;
  // the nearest living village (frontier ones first) within 1200 tiles
  const SettlementState* best = nullptr;
  int64_t bd = 1200ll * 1200;
  for (const SettlementState& s : settlements_) {
    if (s.flags & (SS_ABANDONED | SS_RUINED)) continue;
    if (s.type != (uint8_t)SiteType::Village) continue;
    if (const KingdomState* O = kingdom(s.owner)) if (O->player) continue;
    const int64_t d = dist2(s.gx, s.gy, c->gx, c->gy) / ((s.flags & SS_FRONTIER) ? 4 : 1);
    if (d < bd) { bd = d; best = &s; }
  }
  if (!best) return 0;
  c->strength = std::max<uint8_t>(c->strength, 60);
  return raiseWarlord(*c, best->site, day);
}

Gid Realm::forceSuccessor(Gid fallen, int day) {
  const KingdomState* F = kingdom(fallen);
  if (!F) return 0;
  // its old lands that someone else holds now, or nobody: the remnant rallies round the largest of them
  const SettlementState* best = nullptr;
  int bestRank = -1;
  for (const SettlementState& s : settlements_) {
    if (s.home != fallen || (s.flags & (SS_ABANDONED | SS_RUINED))) continue;
    if (s.owner == fallen) continue;
    if (const KingdomState* O = kingdom(s.owner)) if (O->player) continue;
    const int rank = (2 - std::min<int>(2, s.type)) * 100 + s.popPct;
    if (rank > bestRank) { bestRank = rank; best = &s; }
  }
  if (!best) return 0;
  return crownSuccessor(fallen, best->site, day);
}

void Realm::forceResettle(Gid site, Gid by, int day) {
  if (!settlement(site)) return;
  resettle(state(site), by, day);
}

void Realm::forceAlliance(Gid a, Gid b, int day) {
  const KingdomState* A = kingdom(a);
  const KingdomState* B = kingdom(b);
  if (!A || !B || a == b || apart(*A, *B)) return;
  forcePeace(a, b, day);
  Relation* r = relMut(a, b, true);
  if (!r) return;
  r->state = Rel::Alliance;
  r->opinion = (int8_t)std::max<int>(r->opinion, 40);
  r->tension = Tension::Calm;
  r->aggrieved = 0;
  r->marchDay = 0;
  r->sinceDay = dayU16(day);
  r->stepDay = dayU16(day);
  const KingdomState* K = kingdom(a);
  addEvent(EvType::AllianceMade, a, b, K->capital, capX(*K), capY(*K), day);
}

void Realm::forceVassal(Gid vassal, Gid overlord, int day) {
  const KingdomState* V = kingdom(vassal);
  const KingdomState* O = kingdom(overlord);
  if (!V || !O || vassal == overlord || apart(*V, *O)) return;
  forcePeace(vassal, overlord, day);
  makeVassal(vassal, overlord, day);
}

void Realm::forceWarband(Gid campSite, int day) {
  Camp* c = nullptr;
  for (Camp& x : camps_) if (x.site == campSite) c = &x;
  if (!c || (c->flags & (CF_WARLORD | CF_CLEARED))) return;
  c->strength = std::max<uint8_t>(c->strength, 60);
  c->flags |= CF_WARBAND;
  c->grewDay = dayU16(day);
  const SettlementState* best = nullptr;
  int64_t bd = 1200ll * 1200;
  for (const SettlementState& s : settlements_) {
    if (s.type != (uint8_t)SiteType::Village || (s.flags & (SS_ABANDONED | SS_RUINED))) continue;
    const int64_t d = dist2(s.gx, s.gy, c->gx, c->gy) / ((s.flags & SS_FRONTIER) ? 4 : 1);
    if (d < bd) { bd = d; best = &s; }
  }
  if (!best) return;
  SettlementState& s = state(best->site);
  s.damage = (uint8_t)std::min(100, s.damage + 10);
  s.prosperity = (uint8_t)std::max(0, s.prosperity - 8);
  s.mood = (uint8_t)std::max(0, s.mood - 10);
  s.changedDay = dayU16(day);
  addEvent(EvType::WarbandRaid, 0, s.owner, s.site, s.gx, s.gy, day).mag = c->strength;
}

void Realm::forceRuin(Gid site, int day) {
  if (!settlement(site)) return;
  SettlementState& s = state(site);
  s.flags |= SS_ABANDONED | SS_BURNED;
  s.flags &= (uint16_t)~(SS_BESIEGED | SS_REBUILDING | SS_RESETTLED);
  s.markDay = dayU16(day - 30);
  s.burnDay = dayU16(day);
  s.damage = std::max<uint8_t>(s.damage, 80);
  s.popPct = 0;
  s.changedDay = dayU16(day);
  ruinSite(s, FallCause::War, 0, day);
}

void Realm::forceBossRaid(Gid lair, Gid site, int day) {
  if (!this->lair(lair)) { beastRaid(site, 0, 25, day); return; }
  lairRaid(lair, site, 25, day);
}

// ---------------------------------------------------------------- diplomacy rolls (the SERVICE lane's quests)
bool Realm::peaceRoll(Gid a, Gid b, float bonus, int day) {
  War* w = nullptr;
  for (War& x : wars_)
    if (!x.endDay && ((x.attacker == a && x.defender == b) || (x.attacker == b && x.defender == a))) w = &x;
  const KingdomState* A = kingdom(a);
  const KingdomState* B = kingdom(b);
  if (!w || !A || !B) return false;
  const float tired = std::max(A->exhaustion, B->exhaustion);
  const float p = std::clamp(0.25f + 0.6f * tired + 0.3f * std::fabs(w->score) + 0.4f * bonus, 0.05f, 0.95f);
  const bool ok = chance(p);
  if (ok) endWar(*w, day);
  A = kingdom(a);
  addEvent(EvType::TermsCarried, a, b, A->capital, capX(*A), capY(*A), day).mag = ok ? 1 : 0;
  return ok;
}

bool Realm::allianceRoll(Gid a, Gid b, float bonus, int day) {
  const KingdomState* A = kingdom(a);
  const KingdomState* B = kingdom(b);
  if (!A || !B || a == b || apart(*A, *B) || atWar(a, b)) return false;
  const Relation* r = relation(a, b);
  const float op = r ? r->opinion / 100.0f : 0.0f;
  const bool ok = chance(std::clamp(0.15f + 0.5f * op + 0.4f * bonus, 0.02f, 0.9f));
  if (ok) forceAlliance(a, b, day);
  A = kingdom(a);
  addEvent(EvType::TermsCarried, a, b, A->capital, capX(*A), capY(*A), day).mag = ok ? 2 : 0;
  return ok;
}

bool Realm::tributeRoll(Gid weaker, Gid stronger, float bonus, int day) {
  const KingdomState* W = kingdom(weaker);
  const KingdomState* S = kingdom(stronger);
  if (!W || !S || weaker == stronger || apart(*W, *S) || W->player || W->overlord) return false;
  const float ratio = S->military / std::max(0.5f, W->military);
  const bool ok = chance(std::clamp(0.1f * ratio + 0.3f * bonus - 0.1f, 0.02f, 0.85f));
  if (ok) forceVassal(weaker, stronger, day);
  W = kingdom(weaker);
  addEvent(EvType::TermsCarried, stronger, weaker, W->capital, capX(*W), capY(*W), day).mag = ok ? 3 : 0;
  return ok;
}

}  // namespace realm
