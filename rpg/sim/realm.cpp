// M4 "Banners": the living world simulation (rpg/sim/realm.h). REALM lane.
// This file: names, look-ups, the player's influence, the forcing calls every lane tests with, and the save block.
// The rest of the realm lives in:
//   realm_genesis.cpp  focus: instantiation of the kingdoms round the player (time-sliced), tiers, dormant catch-up
//   realm_history.cpp  the history pre-roll (chronicles, extinct realms) and the ruin records (15.3)
//   realm_tick.cpp     the daily tick: economy, food and harvests, mood, the slow road to war (15.6.3), wars, sieges,
//                      captures, raids, peace, rulers and succession, civil wars, falls, ruins and resettlement
#include "rpg/sim/realm.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
#include "rpg/sim/common.h"
#include "rpg/world/source.h"

namespace realm {
namespace {

// the realm block's own version (bump with any change to its layout). 2: REALM lane phase B (rulers' houses, the
// granaries and harvests, the road to war on relations, war captures, siege kinds, settlement economies, sim ruins)
constexpr uint8_t REALM_BLOCK_VER = 2;
constexpr size_t MAX_EVENTS = 400;       // the event log keeps the newest (rumours fade after 30 / 60 days anyway)

uint16_t dayU16(int day) { return (uint16_t)std::clamp(day, 0, 65535); }

}  // namespace

namespace detail {
double nowMs() {
  using namespace std::chrono;
  return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}
float sitePop(uint8_t type) {   // thousands
  switch ((SiteType)type) {
    case SiteType::City: return 5.0f;
    case SiteType::Town: return 1.0f;
    default: return 0.2f;
  }
}
uint8_t baseGarrison(uint8_t type, bool kingdom) {
  if (!kingdom) return 0;
  return (uint8_t)(type == (uint8_t)SiteType::City ? 60 : type == (uint8_t)SiteType::Town ? 30 : 10);
}
// a settlement's food against its need (percent) from its specialisation (15.11) and size
uint8_t foodYield(uint8_t type, uint8_t special) {
  if (type == (uint8_t)SiteType::City) return special == (uint8_t)ew::Specialty::Farming ? 96 : 88;
  if (type == (uint8_t)SiteType::Town) return special == (uint8_t)ew::Specialty::Farming ? 104 : 94;
  switch ((ew::Specialty)special) {
    case ew::Specialty::Farming: return 138;
    case ew::Specialty::Herding: return 118;
    case ew::Specialty::Fishing: return 114;
    case ew::Specialty::Lumber: return 92;
    case ew::Specialty::Mining: return 86;
    default: return 106;
  }
}
// the lattice does not know a settlement's trade: estimate it from its seed (the true one replaces it on load)
uint8_t estimateSpecial(uint8_t type, uint64_t h) {
  if (type != (uint8_t)SiteType::Village) return (uint8_t)((h % 3) == 0 ? ew::Specialty::Farming : ew::Specialty::None);
  const int r = (int)(h % 100);
  if (r < 40) return (uint8_t)ew::Specialty::Farming;
  if (r < 55) return (uint8_t)ew::Specialty::Herding;
  if (r < 70) return (uint8_t)ew::Specialty::Fishing;
  if (r < 85) return (uint8_t)ew::Specialty::Lumber;
  return (uint8_t)ew::Specialty::Mining;
}
}  // namespace detail

const char* evTypeName(EvType t) {
  static const char* n[] = {"FIRST CONTACT", "TRADE DEAL", "TRADE BROKEN", "HARVEST FAILED", "FAMINE", "BORDER INCIDENT",
                            "SKIRMISH", "WAR DECLARED", "SIEGE BEGUN", "SIEGE BROKEN", "TOWN TAKEN", "TOWN BURNED",
                            "REFUGEES", "PEACE", "RULER DIED", "SUCCESSION", "CIVIL WAR", "KINGDOM FELL", "KINGDOM ROSE",
                            "RESETTLED", "RUINED", "FESTIVAL", "TROOPS MARCHING", "PRICES RISING"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)EvType::COUNT, "evTypeName table");
  return (size_t)t < (size_t)EvType::COUNT ? n[(size_t)t] : "?";
}
const char* relName(Rel r) {
  static const char* n[] = {"UNKNOWN", "CONTACT", "PEACE", "TRADE", "ALLIANCE", "RIVALRY", "WAR", "VASSAL", "OVERLORD"};
  return (size_t)r < (size_t)Rel::COUNT ? n[(size_t)r] : "?";
}
const char* tensionName(Tension t) {
  static const char* n[] = {"CALM", "STRAINED", "TRADE BROKEN", "BORDER TENSION", "SKIRMISHES"};
  return (size_t)t < (size_t)Tension::COUNT ? n[(size_t)t] : "?";
}
const char* warCauseName(WarCause c) {
  static const char* n[] = {"FAMINE", "BROKEN TRADE", "BORDER DISPUTE", "SUCCESSION", "REVENGE", "CONQUEST", "RAIDERS"};
  return (size_t)c < (size_t)WarCause::COUNT ? n[(size_t)c] : "?";
}
const char* fallCauseName(FallCause c) {
  static const char* n[] = {"WAR", "PLAGUE", "FLOOD", "FAMINE", "COLLAPSE", "DRAGON", "CURSE"};
  return (size_t)c < (size_t)FallCause::COUNT ? n[(size_t)c] : "?";
}

void Realm::reset(uint64_t worldSeed) {
  *this = Realm();
  seed_ = worldSeed;
  rng_ = ew::mix64(worldSeed ^ ew::tag("realm"));
}

void Realm::noteSite(Gid site, Gid home, uint8_t type, int32_t gx, int32_t gy) {
  if (!site || settlementIx_.count(site)) return;
  SettlementState s;
  s.site = site; s.owner = home; s.home = home; s.type = type; s.gx = gx; s.gy = gy;
  if (!home) s.flags |= SS_FRONTIER;
  s.garrison = detail::baseGarrison(type, home != 0);
  s.special = detail::estimateSpecial(type, ew::mix64(site ^ seed_ ^ 0x5350454Cull));
  s.yield = detail::foodYield(type, s.special);
  settlementIx_[site] = (int)settlements_.size();
  settlements_.push_back(s);
}

void Realm::noteEconomy(Gid site, uint8_t special) {
  auto it = settlementIx_.find(site);
  if (it == settlementIx_.end() || special == 0 || special >= (uint8_t)ew::Specialty::COUNT) return;
  SettlementState& s = settlements_[(size_t)it->second];
  if (s.special == special) return;
  s.special = special;
  s.yield = detail::foodYield(s.type, special);
}

// ---------------------------------------------------------------- look-ups
const KingdomState* Realm::kingdom(Gid id) const {
  auto it = kingdomIx_.find(id);
  return it == kingdomIx_.end() ? nullptr : &kingdoms_[(size_t)it->second];
}
KingdomState* Realm::kingdomMut(Gid id) {
  auto it = kingdomIx_.find(id);
  return it == kingdomIx_.end() ? nullptr : &kingdoms_[(size_t)it->second];
}
const SettlementState* Realm::settlement(Gid site) const {
  auto it = settlementIx_.find(site);
  return it == settlementIx_.end() ? nullptr : &settlements_[(size_t)it->second];
}
SettlementState& Realm::state(Gid site) {
  auto it = settlementIx_.find(site);
  if (it == settlementIx_.end()) { noteSite(site, 0, (uint8_t)SiteType::Village, 0, 0); it = settlementIx_.find(site); }
  return settlements_[(size_t)it->second];
}
void Realm::rebuildIndex() {
  std::sort(kingdoms_.begin(), kingdoms_.end(), [](const KingdomState& a, const KingdomState& b) { return a.id < b.id; });
  kingdomIx_.clear();
  for (size_t i = 0; i < kingdoms_.size(); i++) kingdomIx_[kingdoms_[i].id] = (int)i;
}
Gid Realm::ownerOf(Gid site, Gid genesis) const {
  const SettlementState* s = settlement(site);
  return s ? s->owner : genesis;
}
Gid Realm::landOwner(Gid genesis, int32_t gx, int32_t gy) const {
  if (!genesis) return 0;
  bool moved = false;
  for (const SettlementState& s : settlements_) if (s.home == genesis && s.owner != genesis) { moved = true; break; }
  if (!moved) return genesis;
  const SettlementState* best = nullptr;
  int64_t bd = INT64_MAX;
  for (const SettlementState& s : settlements_) {
    if (s.home != genesis) continue;
    const int64_t dx = s.gx - gx, dy = s.gy - gy, d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = &s; }
  }
  return best ? best->owner : genesis;
}
const Relation* Realm::relation(Gid a, Gid b) const {
  if (a > b) std::swap(a, b);
  for (const Relation& r : relations_) if (r.a == a && r.b == b) return &r;
  return nullptr;
}
Relation* Realm::relMut(Gid a, Gid b, bool create) {
  if (a == b || !a || !b) return nullptr;
  if (a > b) std::swap(a, b);
  for (Relation& r : relations_) if (r.a == a && r.b == b) return &r;
  if (!create) return nullptr;
  Relation r;
  r.a = a; r.b = b;
  r.sinceDay = dayU16(day_);
  auto at = std::lower_bound(relations_.begin(), relations_.end(), r,
                             [](const Relation& x, const Relation& y) { return x.a != y.a ? x.a < y.a : x.b < y.b; });
  return &*relations_.insert(at, r);
}
const Siege* Realm::siegeAt(Gid site) const {
  for (const Siege& g : sieges_) if (g.site == site && !g.over) return &g;
  return nullptr;
}
const War* Realm::warBetween(Gid a, Gid b) const {
  for (const War& w : wars_)
    if (!w.endDay && ((w.attacker == a && w.defender == b) || (w.attacker == b && w.defender == a))) return &w;
  return nullptr;
}
int32_t Realm::capX(const KingdomState& k) const {
  if (const SettlementState* s = settlement(k.capital)) return s->gx;
  for (Gid g : k.settlements) if (const SettlementState* s = settlement(g)) return s->gx;
  return ew::idRx(k.id) * ew::KCELL;
}
int32_t Realm::capY(const KingdomState& k) const {
  if (const SettlementState* s = settlement(k.capital)) return s->gy;
  for (Gid g : k.settlements) if (const SettlementState* s = settlement(g)) return s->gy;
  return ew::idRy(k.id) * ew::KCELL;
}
int32_t Realm::seatX(const KingdomState& k) const { return capX(k); }
int32_t Realm::seatY(const KingdomState& k) const { return capY(k); }

size_t Realm::memoryBytes() const {
  size_t b = sizeof(Realm);
  for (const KingdomState& k : kingdoms_)
    b += sizeof(KingdomState) + k.name.capacity() + k.ruler.name.capacity() + k.ruler.title.capacity() + k.ruler.house.capacity() +
         k.settlements.capacity() * sizeof(Gid) + 32;
  b += kingdomIx_.size() * 32;
  b += relations_.capacity() * sizeof(Relation);
  for (const War& w : wars_) b += sizeof(War) + w.name.capacity() + (w.taken.capacity() + w.takenFrom.capacity()) * sizeof(Gid);
  b += sieges_.capacity() * sizeof(Siege);
  b += settlements_.capacity() * sizeof(SettlementState) + settlementIx_.size() * 32;
  b += events_.capacity() * sizeof(WorldEvent);
  b += simRuins_.capacity() * sizeof(SimRuin);
  b += pending_.capacity() * sizeof(Gid) + scanned_.size() * 24;
  for (const auto& h : hist_) { b += 48; for (const HistoryEntry& e : h.second) b += sizeof(HistoryEntry) + e.text.capacity(); }
  for (const auto& r : ruins_) {
    b += sizeof(RuinRecord) + 48 + r.second.oldName.capacity() + r.second.builtBy.capacity() + r.second.lastLord.capacity();
    for (const std::string& c : r.second.clues) b += sizeof(std::string) + c.capacity();
  }
  return b;
}

// ---------------------------------------------------------------- the player
int Realm::rep(Gid kingdom) const {
  const KingdomState* k = this->kingdom(kingdom);
  return k ? k->playerRep : 0;
}
void Realm::addRep(Gid kingdom, int delta) {
  if (KingdomState* k = kingdomMut(kingdom)) {
    const int was = k->playerRep;
    k->playerRep = (int16_t)std::clamp(k->playerRep + delta, -100, 100);
    // 15.6.4: deeds people talk about make the player known (good or ill: fame counts both)
    const int moved = std::abs(k->playerRep - was);
    if (moved > 0) renown_.fame = std::min(1000000, renown_.fame + std::max(1, moved / 2));
  }
}
void Realm::contribute(uint32_t siege, int side, float amount) {
  for (Siege& g : sieges_)
    if (g.id == siege && !g.over) {
      if (side == 1) g.atk += amount; else if (side == 2) g.def += amount;
      g.playerJoined = true;
      g.playerSide = (uint8_t)side;
      if (amount > 0) renown_.fame = std::min(1000000, renown_.fame + std::max(1, (int)std::lround(amount * 2)));
    }
}
void Realm::markHeard(uint32_t eventId) {
  for (WorldEvent& e : events_) if (e.id == eventId) e.heard = true;
}

// ---------------------------------------------------------------- events and forcing
WorldEvent& Realm::addEvent(EvType t, Gid a, Gid b, Gid site, int32_t gx, int32_t gy, int day) {
  WorldEvent e;
  e.id = nextEvent_++;
  e.day = dayU16(day);
  e.type = t; e.a = a; e.b = b; e.site = site; e.gx = gx; e.gy = gy;
  events_.push_back(e);
  if (events_.size() > MAX_EVENTS) events_.erase(events_.begin(), events_.begin() + (std::ptrdiff_t)(events_.size() - MAX_EVENTS));
  return events_.back();
}
void Realm::forceEvent(EvType t, Gid a, Gid b, Gid site, int32_t gx, int32_t gy, int day) { addEvent(t, a, b, site, gx, gy, day); }

uint32_t Realm::forceWar(Gid attacker, Gid defender, int day) {
  if (const War* w = warBetween(attacker, defender)) return w->id;
  return declareWar(attacker, defender, WarCause::BorderDispute, day, true);
}

uint32_t Realm::forceSiege(Gid site, Gid attacker, int day) {
  if (const Siege* g = siegeAt(site)) return g->id;
  const Gid defender = state(site).owner;
  const uint32_t war = defender && defender != attacker ? forceWar(attacker, defender, day) : 0;
  War* w = nullptr;
  for (War& x : wars_) if (x.id == war) w = &x;
  return startSiege(w, site, attacker, day, true);
}

void Realm::forceOwner(Gid site, Gid owner, int day) {
  const Gid was = state(site).owner;
  if (was == owner) return;
  War* w = nullptr;
  for (War& x : wars_)
    if (!x.endDay && ((x.attacker == owner && x.defender == was) || (x.attacker == was && x.defender == owner))) w = &x;
  capture(site, owner, day, w);
}

void Realm::forceBurn(Gid site, int day) { burn(site, 0, day); }

void Realm::forceFamine(Gid site, int day) {
  SettlementState& s = state(site);
  s.food = 0;
  s.mood = (uint8_t)std::min<int>(s.mood, 25);
  s.flags |= SS_FAMINE;
  s.changedDay = dayU16(day);
  // the chain of 15.6.3 starts from here: its realm's granaries are empty too
  if (KingdomState* k = kingdomMut(s.owner)) {
    k->food = 0;
    if (!k->famine) { k->famine = true; k->famineDay = dayU16(day); }
  }
  addEvent(EvType::Famine, s.owner, 0, site, s.gx, s.gy, day);
}

void Realm::forceRulerDeath(Gid kingdom, int day) {
  if (KingdomState* k = kingdomMut(kingdom)) if (!k->fallen) succession(*k, day, true);
}
Gid Realm::forceCivilWar(ew::EndlessSource& src, Gid kingdom, int day) {
  src_ = &src;
  KingdomState* k = kingdomMut(kingdom);
  if (!k || k->fallen) return 0;
  return splitRealm(&src, *k, day);
}
void Realm::forcePeace(Gid a, Gid b, int day) {
  for (War& w : wars_)
    if (!w.endDay && ((w.attacker == a && w.defender == b) || (w.attacker == b && w.defender == a))) endWar(w, day);
}
void Realm::forceHarvestFail(Gid kingdom, int day) {
  KingdomState* k = kingdomMut(kingdom);
  if (!k) return;
  k->lastHarvest = 0.4f;
  k->harvestYear = (int16_t)(std::max(0, day) / YEAR);
  k->food = std::min(k->food, 0.25f);
  Gid at = k->capital;
  for (Gid g : k->settlements)
    if (const SettlementState* s = settlement(g)) if (s->special == (uint8_t)ew::Specialty::Farming) { at = g; break; }
  const SettlementState* s = settlement(at);
  addEvent(EvType::HarvestFailed, k->id, 0, at, s ? s->gx : capX(*k), s ? s->gy : capY(*k), day).mag = 40;
}
void Realm::forceTension(Gid a, Gid b, Tension t, WarCause cause, int day) {
  Relation* r = relMut(a, b, true);
  if (!r) return;
  if (r->state == Rel::Unknown) r->state = Rel::Contact;
  r->aggrieved = a;
  r->cause = cause;
  r->truceUntil = 0;
  setTension(*r, t, day);
}

// ---------------------------------------------------------------- persistence
// Layout (REALM_BLOCK_VER 2): ver u8, seed u64, rng u64, day i32, nextEvent u32, nextWar u32, nextSiege u32,
// nextRebel u32, focus kx i32, ky i32;
// kingdoms (u32 n, each: id u64, capital u64, culture u64, name str, color u32, color2 u32, emblem u8, gov u8,
//   ruler (name str, title str, house str, age u8, traits u8, female u8, sinceDay u16, dynasty u64), pop, wealth,
//   military, stability, exhaustion, food f32, values 8 x u8, fallen u8, rebel u8, tier u8, foundedDay i32, rep i32,
//   settlements (u32 n, u64 each), parent u64, lastTick u16, famine u8, famineDay u16, yieldRatio f32, harvestDoy u16,
//   harvestYear i32, lastHarvest f32, capitalLostDay u16, inherit u8);
// relations (u32 n: a u16, b u16 (indices into the kingdoms above), state u8, tension u8, opinion i8, sinceDay u16,
//   border u8, bits u8 (foodDeal 1, seller side << 1, aggrieved side << 3; side 0 none, 1 a, 2 b), base i8, cause u8,
//   stepDay u16, lastIncident u16, marchDay u16, truceUntil u16);
// wars (u32 n: id u32, attacker u64, defender u64, start u16, end u16, score f32, goal u8, cause u8, name str,
//   taken (u32 n, site u64, from u64 each), pausedDays u16, forced u8);
// sieges (u32 n: id u32, war u32, site u64, attacker u64, defender u64, atk f32, def f32, start u16, resolve u16, over u8,
//   attackerWon u8, joined u8, side u8, campX i32, campY i32, forced u8, raid u8);
// settlements (u32 n: site u64, owner u64, home u64, type u8, gx i32, gy i32, popPct, prosperity, damage, garrison, food,
//   mood u8, flags u16, changed u16, refugeesFrom u64, garrisonOf u64, special u8, yield u8, refugeesDay u16, markDay u16,
//   takenDay u16, burnDay u16);
// events (u32 n: id u32, day u16, type u8, a u64, b u64, site u64, mag i32, gx i32, gy i32, heard u8);
// renown (fame i32, lordOf u64, rank u8, sworn u64); sim ruins (u32 n: site u64, day u16, cause u8, destroyer u64,
// owner u64, home u64).
void Realm::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  w.u8(REALM_BLOCK_VER);
  w.u64(seed_); w.u64(rng_); w.i32(day_);
  w.u32(nextEvent_); w.u32(nextWar_); w.u32(nextSiege_); w.u32(nextRebel_);
  w.i32(focusKx_); w.i32(focusKy_);
  w.u32((uint32_t)kingdoms_.size());
  for (const KingdomState& k : kingdoms_) {
    w.u64(k.id); w.u64(k.capital); w.u64(k.culture); w.str(k.name); w.u32(k.color); w.u32(k.color2); w.u8(k.emblem); w.u8(k.gov);
    w.str(k.ruler.name); w.str(k.ruler.title); w.str(k.ruler.house); w.u8(k.ruler.age); w.u8(k.ruler.traits);
    w.u8(k.ruler.female ? 1 : 0); w.u16(k.ruler.sinceDay); w.u64(k.ruler.dynasty);
    w.f32(k.pop); w.f32(k.wealth); w.f32(k.military); w.f32(k.stability); w.f32(k.exhaustion); w.f32(k.food);
    for (uint8_t v : k.values) w.u8(v);
    w.u8(k.fallen ? 1 : 0); w.u8(k.rebel ? 1 : 0); w.u8((uint8_t)k.tier); w.i32(k.foundedDay); w.i32(k.playerRep);
    w.u32((uint32_t)k.settlements.size());
    for (Gid s : k.settlements) w.u64(s);
    w.u64(k.parent); w.u16(k.lastTick); w.u8(k.famine ? 1 : 0); w.u16(k.famineDay); w.f32(k.yieldRatio); w.u16(k.harvestDoy);
    w.i32(k.harvestYear); w.f32(k.lastHarvest); w.u16(k.capitalLostDay); w.u8(k.inherit);
  }
  // relations name their realms by index into the kingdom list above (compact: 24 bytes each)
  uint32_t nrel = 0;
  for (const Relation& r : relations_) if (kingdomIx_.count(r.a) && kingdomIx_.count(r.b)) nrel++;
  w.u32(nrel);
  for (const Relation& r : relations_) {
    auto ia = kingdomIx_.find(r.a), ib = kingdomIx_.find(r.b);
    if (ia == kingdomIx_.end() || ib == kingdomIx_.end()) continue;
    auto side = [&](Gid g) -> uint8_t { return g == r.a ? 1 : g == r.b ? 2 : 0; };
    w.u16((uint16_t)ia->second); w.u16((uint16_t)ib->second); w.u8((uint8_t)r.state); w.u8((uint8_t)r.tension);
    w.u8((uint8_t)r.opinion); w.u16(r.sinceDay); w.u8(r.border); w.u8((uint8_t)((r.foodDeal ? 1 : 0) | side(r.seller) << 1 | side(r.aggrieved) << 3));
    w.u8((uint8_t)r.base); w.u8((uint8_t)r.cause); w.u16(r.stepDay); w.u16(r.lastIncident); w.u16(r.marchDay); w.u16(r.truceUntil);
  }
  w.u32((uint32_t)wars_.size());
  for (const War& x : wars_) {
    w.u32(x.id); w.u64(x.attacker); w.u64(x.defender); w.u16(x.startDay); w.u16(x.endDay); w.f32(x.score);
    w.u8((uint8_t)x.goal); w.u8((uint8_t)x.cause); w.str(x.name);
    w.u32((uint32_t)x.taken.size());
    for (size_t i = 0; i < x.taken.size(); i++) { w.u64(x.taken[i]); w.u64(i < x.takenFrom.size() ? x.takenFrom[i] : 0); }
    w.u16(x.pausedDays); w.u8(x.forced ? 1 : 0);
  }
  w.u32((uint32_t)sieges_.size());
  for (const Siege& g : sieges_) {
    w.u32(g.id); w.u32(g.war); w.u64(g.site); w.u64(g.attacker); w.u64(g.defender); w.f32(g.atk); w.f32(g.def);
    w.u16(g.startDay); w.u16(g.resolveDay); w.u8(g.over ? 1 : 0); w.u8(g.attackerWon ? 1 : 0); w.u8(g.playerJoined ? 1 : 0);
    w.u8(g.playerSide); w.i32(g.campX); w.i32(g.campY); w.u8(g.forced ? 1 : 0); w.u8(g.raid ? 1 : 0);
  }
  w.u32((uint32_t)settlements_.size());
  for (const SettlementState& s : settlements_) {
    w.u64(s.site); w.u64(s.owner); w.u64(s.home); w.u8(s.type); w.i32(s.gx); w.i32(s.gy);
    w.u8(s.popPct); w.u8(s.prosperity); w.u8(s.damage); w.u8(s.garrison); w.u8(s.food); w.u8(s.mood);
    w.u16(s.flags); w.u16(s.changedDay); w.u64(s.refugeesFrom); w.u64(s.garrisonOf);
    w.u8(s.special); w.u8(s.yield); w.u16(s.refugeesDay); w.u16(s.markDay); w.u16(s.takenDay); w.u16(s.burnDay);
  }
  w.u32((uint32_t)events_.size());
  for (const WorldEvent& e : events_) {
    w.u32(e.id); w.u16(e.day); w.u8((uint8_t)e.type); w.u64(e.a); w.u64(e.b); w.u64(e.site); w.i32(e.mag);
    w.i32(e.gx); w.i32(e.gy); w.u8(e.heard ? 1 : 0);
  }
  w.i32(renown_.fame); w.u64(renown_.lordOf); w.u8(renown_.rank); w.u64(renown_.sworn);
  w.u32((uint32_t)simRuins_.size());
  for (const SimRuin& r : simRuins_) { w.u64(r.site); w.u16(r.day); w.u8((uint8_t)r.cause); w.u64(r.destroyer); w.u64(r.owner); w.u64(r.home); }
}

bool Realm::deserialize(const std::vector<uint8_t>& in) {
  BinR r(in);
  if (r.u8() != REALM_BLOCK_VER || r.bad) return false;
  // a count can never claim more records than the bytes left could hold (a damaged save must fail fast)
  auto fits = [&](uint32_t n, size_t minBytes) { return !r.bad && (uint64_t)n * minBytes <= (uint64_t)(in.size() - std::min(in.size(), r.p)); };
  Realm R;
  R.seed_ = r.u64(); R.rng_ = r.u64(); R.day_ = r.i32();
  R.nextEvent_ = r.u32(); R.nextWar_ = r.u32(); R.nextSiege_ = r.u32(); R.nextRebel_ = r.u32();
  R.focusKx_ = r.i32(); R.focusKy_ = r.i32();
  if (R.day_ < 0 || R.day_ > 65535) return false;
  const uint32_t nk = r.u32();
  if (!fits(nk, 120)) return false;
  for (uint32_t i = 0; i < nk && !r.bad; i++) {
    KingdomState k;
    k.id = r.u64(); k.capital = r.u64(); k.culture = r.u64(); k.name = r.str(); k.color = r.u32(); k.color2 = r.u32();
    k.emblem = r.u8(); k.gov = r.u8();
    k.ruler.name = r.str(); k.ruler.title = r.str(); k.ruler.house = r.str(); k.ruler.age = r.u8(); k.ruler.traits = r.u8();
    k.ruler.female = r.u8() != 0; k.ruler.sinceDay = r.u16(); k.ruler.dynasty = r.u64();
    k.pop = r.f32(); k.wealth = r.f32(); k.military = r.f32(); k.stability = r.f32(); k.exhaustion = r.f32(); k.food = r.f32();
    for (uint8_t& v : k.values) v = r.u8();
    k.fallen = r.u8() != 0; k.rebel = r.u8() != 0; k.tier = (Tier)std::min<uint8_t>(r.u8(), (uint8_t)Tier::Present);
    k.foundedDay = r.i32(); k.playerRep = (int16_t)std::clamp(r.i32(), -100, 100);
    const uint32_t ns = r.u32();
    if (!fits(ns, 8)) return false;
    for (uint32_t j = 0; j < ns && !r.bad; j++) k.settlements.push_back(r.u64());
    k.parent = r.u64(); k.lastTick = r.u16(); k.famine = r.u8() != 0; k.famineDay = r.u16(); k.yieldRatio = r.f32();
    k.harvestDoy = (uint16_t)(r.u16() % YEAR); k.harvestYear = (int16_t)r.i32(); k.lastHarvest = r.f32();
    k.capitalLostDay = r.u16(); k.inherit = r.u8();
    // floats from a damaged save must stay finite and in range (the tick multiplies them)
    auto sane = [](float v, float lo, float hi) { return std::isfinite(v) ? std::clamp(v, lo, hi) : lo; };
    k.pop = sane(k.pop, 0, 1000); k.wealth = sane(k.wealth, -1000, 100000); k.military = sane(k.military, 0, 10000);
    k.stability = sane(k.stability, 0, 1); k.exhaustion = sane(k.exhaustion, 0, 1); k.food = sane(k.food, 0, 2);
    k.yieldRatio = sane(k.yieldRatio, 0.3f, 2); k.lastHarvest = sane(k.lastHarvest, 0, 2);
    if (R.kingdomIx_.count(k.id)) return false;
    R.kingdomIx_[k.id] = (int)R.kingdoms_.size();
    R.kingdoms_.push_back(std::move(k));
  }
  for (size_t i = 1; i < R.kingdoms_.size(); i++) if (R.kingdoms_[i - 1].id >= R.kingdoms_[i].id) return false;   // Gid order
  const uint32_t nr = r.u32();
  if (!fits(nr, 24)) return false;
  for (uint32_t i = 0; i < nr && !r.bad; i++) {
    Relation x;
    const uint16_t ia = r.u16(), ib = r.u16();
    if (ia >= R.kingdoms_.size() || ib >= R.kingdoms_.size() || ia == ib) return false;
    x.a = R.kingdoms_[ia].id; x.b = R.kingdoms_[ib].id;
    x.state = (Rel)std::min<uint8_t>(r.u8(), (uint8_t)Rel::COUNT - 1);
    x.tension = (Tension)std::min<uint8_t>(r.u8(), (uint8_t)Tension::COUNT - 1);
    x.opinion = (int8_t)std::clamp<int>((int8_t)r.u8(), -100, 100);
    x.sinceDay = r.u16(); x.border = r.u8();
    const uint8_t bits = r.u8();
    auto side = [&](int v) -> Gid { return v == 1 ? x.a : v == 2 ? x.b : 0; };
    x.foodDeal = (bits & 1) != 0; x.seller = side((bits >> 1) & 3); x.aggrieved = side((bits >> 3) & 3);
    if (x.foodDeal && !x.seller) x.foodDeal = false;
    x.base = (int8_t)std::clamp<int>((int8_t)r.u8(), -100, 100);
    x.cause = (WarCause)std::min<uint8_t>(r.u8(), (uint8_t)WarCause::COUNT - 1);
    x.stepDay = r.u16(); x.lastIncident = r.u16(); x.marchDay = r.u16(); x.truceUntil = r.u16();
    if (x.a > x.b) return false;   // a < b, in (a, b) order
    if (!R.relations_.empty() && (R.relations_.back().a > x.a || (R.relations_.back().a == x.a && R.relations_.back().b >= x.b))) return false;
    R.relations_.push_back(x);
  }
  const uint32_t nw = r.u32();
  if (!fits(nw, 40)) return false;
  for (uint32_t i = 0; i < nw && !r.bad; i++) {
    War x;
    x.id = r.u32(); x.attacker = r.u64(); x.defender = r.u64(); x.startDay = r.u16(); x.endDay = r.u16(); x.score = r.f32();
    if (!std::isfinite(x.score)) x.score = 0;
    x.goal = (WarGoal)std::min<uint8_t>(r.u8(), (uint8_t)WarGoal::COUNT - 1);
    x.cause = (WarCause)std::min<uint8_t>(r.u8(), (uint8_t)WarCause::COUNT - 1); x.name = r.str();
    const uint32_t nt = r.u32();
    if (!fits(nt, 16)) return false;
    for (uint32_t j = 0; j < nt && !r.bad; j++) { x.taken.push_back(r.u64()); x.takenFrom.push_back(r.u64()); }
    x.pausedDays = r.u16(); x.forced = r.u8() != 0;
    R.wars_.push_back(std::move(x));
  }
  const uint32_t ng = r.u32();
  if (!fits(ng, 60)) return false;
  for (uint32_t i = 0; i < ng && !r.bad; i++) {
    Siege g;
    g.id = r.u32(); g.war = r.u32(); g.site = r.u64(); g.attacker = r.u64(); g.defender = r.u64(); g.atk = r.f32(); g.def = r.f32();
    if (!std::isfinite(g.atk)) g.atk = 0;
    if (!std::isfinite(g.def)) g.def = 0;
    g.startDay = r.u16(); g.resolveDay = r.u16(); g.over = r.u8() != 0; g.attackerWon = r.u8() != 0; g.playerJoined = r.u8() != 0;
    g.playerSide = r.u8(); g.campX = r.i32(); g.campY = r.i32(); g.forced = r.u8() != 0; g.raid = r.u8() != 0;
    R.sieges_.push_back(g);
  }
  const uint32_t nsS = r.u32();
  if (!fits(nsS, 60)) return false;
  for (uint32_t i = 0; i < nsS && !r.bad; i++) {
    SettlementState s;
    s.site = r.u64(); s.owner = r.u64(); s.home = r.u64(); s.type = r.u8(); s.gx = r.i32(); s.gy = r.i32();
    s.popPct = r.u8(); s.prosperity = r.u8(); s.damage = r.u8(); s.garrison = r.u8(); s.food = r.u8(); s.mood = r.u8();
    s.flags = r.u16(); s.changedDay = r.u16(); s.refugeesFrom = r.u64(); s.garrisonOf = r.u64();
    s.special = r.u8(); s.yield = r.u8(); s.refugeesDay = r.u16(); s.markDay = r.u16(); s.takenDay = r.u16(); s.burnDay = r.u16();
    if (R.settlementIx_.count(s.site)) return false;
    R.settlementIx_[s.site] = (int)R.settlements_.size();
    R.settlements_.push_back(s);
  }
  const uint32_t ne = r.u32();
  if (!fits(ne, 43)) return false;
  for (uint32_t i = 0; i < ne && !r.bad; i++) {
    WorldEvent e;
    e.id = r.u32(); e.day = r.u16(); e.type = (EvType)std::min<uint8_t>(r.u8(), (uint8_t)EvType::COUNT - 1);
    e.a = r.u64(); e.b = r.u64(); e.site = r.u64(); e.mag = (int16_t)r.i32();
    e.gx = r.i32(); e.gy = r.i32(); e.heard = r.u8() != 0;
    R.events_.push_back(e);
  }
  R.renown_.fame = r.i32(); R.renown_.lordOf = r.u64(); R.renown_.rank = r.u8(); R.renown_.sworn = r.u64();
  const uint32_t nru = r.u32();
  if (!fits(nru, 35)) return false;
  for (uint32_t i = 0; i < nru && !r.bad; i++) {
    SimRuin x;
    x.site = r.u64(); x.day = r.u16(); x.cause = (FallCause)std::min<uint8_t>(r.u8(), (uint8_t)FallCause::COUNT - 1);
    x.destroyer = r.u64(); x.owner = r.u64(); x.home = r.u64();
    R.simRuins_.push_back(x);
  }
  if (r.bad || r.p != in.size()) return false;
  R.stats = Stats();
  R.refocus_ = true;   // the next focus rebuilds the tiers, the pending list and the history memo
  *this = std::move(R);
  return true;
}

}  // namespace realm
