// M4 "Banners": the daily tick (VISION_PLAN 4.3 as the owner bound it in 15.6.3; rpg/sim/realm.h). REALM lane.
// One tick per in-game day for the active realms (VISION_PLAN 4.4), kingdoms in Gid order, relations in (a, b) order,
// drawing only from the realm's own saved stream. Abstract numbers:
//   economy      income by government and trade partners, upkeep, recruiting toward a martial target, desertion
//                when the treasury is empty, stability drifting toward its mark (honour, exhaustion, famine, a lost
//                capital, occupied towns, unrest)
//   food (15.11, 15.12)  each settlement yields against its need (its specialisation); a realm's granary drains
//                a year's worth per year and fills at its harvest day by yield x quality. Harvest quality is a pure
//                hash (regional droughts over 2 x 2 kingdom cells, single failures, poor and bumper years). Food deals
//                move grain from a surplus realm to a hungry one. An empty granary is a famine.
//   the road to war (15.6.3: slow, realistic, every war has a cause and is foreshadowed)
//                Calm -> Strained (a famine eyes a fed neighbour; old grudges; revenge for lost towns)
//                  -> TradeBroken (a failed harvest breaks a food deal) -> BorderTension (border incidents)
//                  -> Skirmishes (skirmishes, prices rising) -> troops marching -> war declared some days later.
//                Every rung cools again when its cause goes away (a harvest comes in, tempers fade).
//   wars         sieges of border settlements (strength committed, resolution after 3 + garrison / 20 + 4 x walls
//                days, a roll weighted atk / (atk + def x walls), the player's contribution counts), captures
//                (Occupied 30 days, damage +30, popPct -15, a garrison tower when cultures differ) or raids (burned,
//                refugees), exhaustion and score, peace after 20-90 days with a treaty (ceded towns, tribute)
//   rulers       age, death, succession by the society's inheritance (a new house on an elective win)
//   civil war    stability below 0.2 splits the realm: the far half becomes a rebel kingdom
//   settlements  food, mood, unrest, famine, healing (rebuilding), refugees, abandonment -> ruin (30 days) ->
//                resettlement by an expansionist neighbour (60 days)
#include <algorithm>
#include <cmath>
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
#include "rpg/sim/realm.h"
#include "rpg/world/source.h"

namespace realm {
namespace {

// ---- tuning (the owner's targets, 15.6.3; rpg_test --history 2000 --seeds 1..20 checks them)
struct Tune {
  float calmFamine = 0.05f;      // per day: a starving realm turns on a fed neighbour (Strained)
  float calmGrudge = 0.0006f;    // per day x aggression: an old grudge flares (opinion < -45)
  float calmRevenge = 0.0012f;   // per day: a realm strong again eyes the towns it lost
  float strainedHungry = 0.05f;  // Strained -> BorderTension while the aggrieved side is hungry
  float strainedGrudge = 0.006f; // ... for the other causes (x aggression)
  float tbHungry = 0.05f;        // TradeBroken -> BorderTension while hungry
  float btSkirmish = 0.03f;      // BorderTension -> Skirmishes (x aggression)
  float skirmishEvent = 0.03f;   // a skirmish while at Skirmishes
  float warDecide = 0.03f;       // Skirmishes -> troops marching (x aggression x strength)
  float cool = 0.02f;            // a rung cools once its cause is gone
  float death = 0.00004f;        // a ruler's death per day at 40 (x e^((age - 40) / 9))
  float civil = 0.025f;          // civil war per day below stability 0.2
};
const Tune T;

uint16_t dayU16(int day) { return (uint16_t)std::clamp(day, 0, 65535); }
uint32_t colour(int r, int g, int b) { return (uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16 | 0xFF000000u; }
int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
int64_t dist2(int32_t ax, int32_t ay, int32_t bx, int32_t by) {
  const int64_t dx = ax - bx, dy = ay - by;
  return dx * dx + dy * dy;
}

const char* const ADJ[] = {"SALT", "IRON", "BROKEN", "SILVER", "RED", "BITTER", "LONG", "GREY", "BURNING", "HOLLOW",
                           "WINTER", "GOLDEN", "BLACK", "WEEPING", "THREE", "WHITE"};
const char* const NOUN[] = {"CROWN", "FORD", "HARVEST", "BRIDGE", "BANNER", "OATH", "FIELDS", "SPEAR", "GRANARY", "RIVER",
                            "STAG", "TOWER", "WELL", "ROAD", "HILLS", "MILL"};

}  // namespace

namespace detail {
uint8_t traitsFor(const uint8_t* v, uint64_t h) {
  const int w[6] = {20 + v[V_MARTIAL] / 3 + v[V_EXPANSIONIST] / 4, 30 + v[V_ISOLATIONIST] / 4, 20 + v[V_MERCANTILE] / 4,
                    15 + v[V_PIOUS] / 3, 15 + v[V_SCHOLARLY] / 3, 20 + v[V_HONOUR] / 3};
  int tot = 0;
  for (int x : w) tot += x;
  uint8_t t = 0;
  for (int pick = 0; pick < 1 + (int)(h % 3 == 0); pick++) {
    int r = (int)((h >> (8 + pick * 16)) % (uint64_t)tot);
    for (int i = 0; i < 6; i++) { if (r < w[i]) { t |= (uint8_t)(1u << i); break; } r -= w[i]; }
  }
  if ((t & RT_AGGRESSIVE) && (t & RT_CAUTIOUS)) t &= (uint8_t)~RT_CAUTIOUS;
  return t;
}
}  // namespace detail

uint64_t Realm::roll() { rng_ = ew::mix64(rng_ + 0x9E3779B97F4A7C15ull); return rng_; }

// ---------------------------------------------------------------- the clock
void Realm::advanceTo(ew::EndlessSource& src, int day) {
  src_ = &src;
  if (day <= day_) { if (day_ == 0) day_ = std::max(0, day); return; }
  const double t0 = detail::nowMs();
  int n = 0;
  const int gap = day - day_;
  if (gap > 60) {
    // VISION_PLAN 4.8: at most 60 daily ticks per call, weekly ticks (7x rates) before them, the rest skipped
    const int weeks = std::min((gap - 60) / 7, 50);
    for (int i = 0; i < weeks; i++) { day_ += 7; tickDay(src, 7); stats.weeklyTicks++; n++; }
    if (day - day_ > 60) day_ = day - 60;
  }
  while (day_ < day) { day_++; tickDay(src, 1); n++; }
  const double ms = detail::nowMs() - t0;
  stats.ticks += n;
  stats.tickMsSum += ms;
  stats.lastTickMs = ms / std::max(1, n);
  stats.worstTickMs = std::max(stats.worstTickMs, stats.lastTickMs);
}

void Realm::tickDay(ew::EndlessSource& src, int span) {
  // trade partners and wars per kingdom (index-parallel scratch)
  std::vector<int> trades(kingdoms_.size(), 0);
  warNow_.assign(kingdoms_.size(), 0);
  for (const Relation& r : relations_) {
    if (r.state != Rel::Trade && !r.foodDeal) continue;
    auto ia = kingdomIx_.find(r.a), ib = kingdomIx_.find(r.b);
    if (ia != kingdomIx_.end()) trades[(size_t)ia->second]++;
    if (ib != kingdomIx_.end()) trades[(size_t)ib->second]++;
  }
  for (const War& w : wars_) {
    if (w.endDay) continue;
    auto ia = kingdomIx_.find(w.attacker), ib = kingdomIx_.find(w.defender);
    if (ia != kingdomIx_.end()) warNow_[(size_t)ia->second] = 1;
    if (ib != kingdomIx_.end()) warNow_[(size_t)ib->second] = 1;
  }
  for (size_t i = 0; i < kingdoms_.size(); i++) {
    KingdomState& k = kingdoms_[i];
    if (k.fallen || k.tier != Tier::Active) continue;
    tickEconomy(k, span, trades[i]);
    tickFood(k, span);
    k.lastTick = dayU16(day_);
  }
  tickSettlements(span);
  tickSieges();
  tickWars(span);
  tickDiplomacy(span);
  tickRulers(src, span);
  tickCivil(src, span);
  if (day_ % 30 == 0 || span >= 7) prune();
}

// ---------------------------------------------------------------- economy and food
void Realm::tickEconomy(KingdomState& k, int span, int trades) {
  if (day_ % 7 == 0 || span >= 7) recount(k);
  static const float TAX[] = {1.0f, 0.9f, 0.8f, 0.85f, 1.0f, 1.2f, 0.95f};
  const float tax = TAX[std::min<int>(k.gov, 6)];
  const auto ix = kingdomIx_.find(k.id);
  const bool war = ix != kingdomIx_.end() && (size_t)ix->second < warNow_.size() && warNow_[(size_t)ix->second];
  const float income = k.pop * tax * (0.6f + 0.1f * (float)std::min(trades, 4)) * 0.012f * span;
  const float upkeep = k.military * 0.006f * (war ? 1.4f : 1.0f) * span;
  k.wealth += income - upkeep;
  // a famine: the treasury buys grain abroad while it lasts (dear grain: prices rise)
  if (k.famine && k.wealth > 0.2f) {
    const float buy = std::min(k.wealth * 0.03f, 0.05f) * span;
    k.wealth -= buy;
    k.food += buy * 0.03f;
  }
  // in ordinary years a realm short of grain buys it on the markets while its treasury allows (a deficit realm is
  // not a starving one: famine comes when a harvest fails, a deal breaks, or the money runs out)
  if (!k.famine && k.food < 0.45f && k.wealth > k.pop * 0.2f) {
    const float amt = std::min(0.0015f * span, 0.45f - k.food);
    k.food += amt;
    k.wealth -= amt * k.pop * 10.0f;
  }
  const float mart = k.values[V_MARTIAL] / 255.0f;
  const float target = k.pop * (0.6f + 1.2f * mart) * ((k.ruler.traits & RT_AGGRESSIVE) ? 1.2f : 1.0f);
  if (k.military < target && k.wealth > k.pop * 0.3f) {
    const float add = std::min(0.004f * k.pop * (0.5f + mart) * span, target - k.military);
    k.military += add;
    k.wealth -= add * 0.6f;
  }
  if (k.wealth < 0) { k.military *= 1.0f - 0.01f * span; k.wealth = std::max(k.wealth, -k.pop); }
  // stability drifts toward its mark
  int occ = 0, unrest = 0, n = 0;
  for (Gid g : k.settlements)
    if (const SettlementState* s = settlement(g)) {
      n++;
      if (s->flags & SS_OCCUPIED) occ++;
      if (s->flags & SS_UNREST) unrest++;
    }
  float mark = 0.62f + 0.18f * k.values[V_HONOUR] / 255.0f + ((k.ruler.traits & RT_HONOURABLE) ? 0.05f : 0.0f) - 0.35f * k.exhaustion -
               (k.capitalLostDay && day_ - (int)k.capitalLostDay < 180 ? 0.25f : 0.0f) - (k.famine ? 0.25f : 0.0f) -
               (n ? 0.15f * occ / n + 0.15f * unrest / n : 0.0f) - (k.wealth < 0 ? 0.1f : 0.0f);
  const float rate = 0.015f * span;
  k.stability = std::clamp(k.stability + std::clamp(mark - k.stability, -rate, rate), 0.0f, 1.0f);
  if (!war) k.exhaustion = std::max(0.0f, k.exhaustion - 0.003f * span);
}

float Realm::harvestQuality(const KingdomState& k, int year) const {
  // a regional drought over 2 x 2 kingdom cells (neighbours suffer together), else the realm's own luck
  const int32_t rx = floorDiv(ew::idRx(k.id), 2), ry = floorDiv(ew::idRy(k.id), 2);
  const uint64_t hr = ew::mix64(ew::cellSeed(seed_, ew::tag("drought"), rx, ry) + (uint64_t)(year + 1) * 0x9E3779B97F4A7C15ull);
  const uint64_t h = ew::mix64(k.id ^ seed_ ^ ((uint64_t)(year + 7) * 0xD6E8FEB86659FD93ull));
  const float jitter = ((int)(h % 13) - 6) / 100.0f;
  if (hr % 100 < 5) return 0.4f + (float)((hr >> 8) % 22) / 100.0f + jitter;
  const int r = (int)((h >> 8) % 1000);
  if (r < 20) return 0.42f + (float)((h >> 20) % 25) / 100.0f;
  if (r < 170) return 0.8f + (float)((h >> 20) % 12) / 100.0f;
  if (r > 930) return 1.07f + (float)((h >> 20) % 8) / 100.0f;
  return 0.93f + (float)((h >> 20) % 14) / 100.0f;
}

void Realm::tickFood(KingdomState& k, int span) {
  k.food -= (float)span / (float)YEAR;
  const int year = day_ / YEAR, doy = day_ % YEAR;
  if (k.harvestYear < year && doy >= (int)k.harvestDoy) {
    const float q = harvestQuality(k, year);
    k.harvestYear = (int16_t)year;
    k.lastHarvest = q;
    k.food = std::min(1.4f, std::max(0.0f, k.food) + k.yieldRatio * q);
    if (q < 0.7f) {
      Gid at = k.capital;
      for (Gid g : k.settlements)
        if (const SettlementState* s = settlement(g)) if (s->special == (uint8_t)ew::Specialty::Farming) { at = g; break; }
      const SettlementState* s = settlement(at);
      addEvent(EvType::HarvestFailed, k.id, 0, at, s ? s->gx : capX(k), s ? s->gy : capY(k), day_).mag = (int16_t)std::lround(q * 100);
    } else if (q > 1.06f && !k.famine && chance(0.5f)) {
      const auto ix = kingdomIx_.find(k.id);
      if (!(ix != kingdomIx_.end() && (size_t)ix->second < warNow_.size() && warNow_[(size_t)ix->second]))
        addEvent(EvType::Festival, k.id, 0, k.capital, capX(k), capY(k), day_);
    }
  }
  k.food = std::clamp(k.food, 0.0f, 1.4f);
  if (!k.famine && k.food <= 0.04f) {
    k.famine = true;
    k.famineDay = dayU16(day_);
    // the hungriest settlement: the one that grows least (a city, usually the capital)
    Gid at = k.capital;
    int lo = 1000;
    for (Gid g : k.settlements)
      if (const SettlementState* s = settlement(g)) if ((int)s->yield < lo) { lo = s->yield; at = g; }
    const SettlementState* s = settlement(at);
    addEvent(EvType::Famine, k.id, 0, at, s ? s->gx : capX(k), s ? s->gy : capY(k), day_).mag = 100;
    addEvent(EvType::PricesRising, k.id, 0, k.capital, capX(k), capY(k), day_);
  } else if (k.famine && k.food > 0.2f) {
    k.famine = false;
  }
}

// ---------------------------------------------------------------- settlements (15.12 in aggregate)
void Realm::tickSettlements(int span) {
  for (size_t i = 0; i < settlements_.size(); i++) {
    SettlementState& s = settlements_[i];
    KingdomState* K = s.owner ? kingdomMut(s.owner) : nullptr;
    if (s.owner && (!K || K->tier != Tier::Active)) continue;   // a dormant or latent realm's towns are frozen
    tickSettlement(s, K, span);
  }
}

void Realm::tickSettlement(SettlementState& s, KingdomState* K, int span) {
  // ---- abandoned: a ruin after 30 days, resettled after 60 by an expansionist neighbour (or its own realm after 180)
  if (s.flags & SS_ABANDONED) {
    const int since = day_ - (int)s.markDay;
    if (!(s.flags & SS_RUINED) && since >= 30) {
      Gid by = 0;
      for (size_t e = events_.size(); e-- > 0;)
        if (events_[e].site == s.site && events_[e].type == EvType::TownBurned) { by = events_[e].a; break; }
      ruinSite(s, (s.flags & SS_BURNED) || by ? FallCause::War : FallCause::Famine, by, day_);
    }
    if (since >= 60 && chance(0.04f * span)) {
      KingdomState* best = nullptr;
      int64_t bd = 1800ll * 1800;
      for (KingdomState& k : kingdoms_) {
        if (k.fallen || k.tier != Tier::Active) continue;
        const bool ok = k.values[V_EXPANSIONIST] > 160 || (k.id == s.owner && since >= 180);
        if (!ok) continue;
        const int64_t d = dist2(capX(k), capY(k), s.gx, s.gy);
        if (d < bd) { bd = d; best = &k; }
      }
      if (best) {
        const Gid was = s.owner, to = best->id;
        s.flags &= (uint16_t)~(SS_ABANDONED | SS_RUINED | SS_BURNED | SS_FAMINE | SS_OCCUPIED | SS_UNREST | SS_REFUGEES | SS_BESIEGED);
        s.flags |= SS_REBUILDING;
        s.popPct = 30; s.damage = std::max<uint8_t>(s.damage, 40); s.food = 40; s.mood = 50;
        if (was != to) giveSite(s, to, day_);
        s.changedDay = dayU16(day_);
        ruins_.erase(s.site);
        addEvent(EvType::Resettled, to, was, s.site, s.gx, s.gy, day_);
        if (KingdomState* O = kingdomMut(was)) if (was != to && O->settlements.empty() && !O->fallen) fallKingdom(*O, day_, to);
      }
    }
    return;
  }
  // ---- food
  int target = K ? (int)std::lround(K->food * 70.0f + (s.yield - 100) * 0.6f + 12.0f) : 50 + (s.yield - 100) / 2;
  if (s.flags & SS_BESIEGED) target -= 30;
  target = std::clamp(target, 0, 100);
  const int step = 5 * span;
  if (s.food < target) s.food = (uint8_t)std::min(target, s.food + step);
  else if (s.food > target) s.food = (uint8_t)std::max(target, s.food - step);
  // ---- famine
  if (K && K->famine && (s.yield < 115 || s.food < 15)) {
    if (!(s.flags & SS_FAMINE)) { s.flags |= SS_FAMINE; s.changedDay = dayU16(day_); }
  } else if ((s.flags & SS_FAMINE) && (!K || !K->famine) && s.food > 30) {
    s.flags &= (uint16_t)~SS_FAMINE;
    s.changedDay = dayU16(day_);
  }
  if ((s.flags & SS_FAMINE) && chance(span / 12.0f) && s.popPct > 0) s.popPct--;
  if ((s.flags & SS_FAMINE) && s.type == (uint8_t)SiteType::Village && s.popPct <= 30 && chance(0.02f * span)) {
    s.flags |= SS_ABANDONED;
    s.markDay = dayU16(day_);
    s.changedDay = dayU16(day_);
    return;
  }
  // ---- war damage heals; burned places start rebuilding after 30 days
  if (s.flags & SS_BURNED) {
    if (day_ - (int)s.burnDay >= 30) { s.flags &= (uint16_t)~SS_BURNED; s.flags |= SS_REBUILDING; s.changedDay = dayU16(day_); }
  } else if (s.damage > 0 && !(s.flags & SS_BESIEGED)) {
    s.damage = (uint8_t)std::max(0, s.damage - span);
  }
  if ((s.flags & SS_REBUILDING) && s.damage == 0) { s.flags &= (uint16_t)~SS_REBUILDING; s.changedDay = dayU16(day_); }
  if ((s.flags & SS_OCCUPIED) && day_ - (int)s.takenDay >= 30) { s.flags &= (uint16_t)~SS_OCCUPIED; s.changedDay = dayU16(day_); }
  if ((s.flags & SS_REFUGEES) && day_ - (int)s.refugeesDay >= 45) {
    s.flags &= (uint16_t)~SS_REFUGEES;
    s.refugeesFrom = 0;
    s.changedDay = dayU16(day_);
  }
  // ---- people come back, the garrison is made up again
  if (!(s.flags & SS_FAMINE) && s.food > 45 && s.damage < 50 && s.popPct < 100 && chance(0.12f * span)) s.popPct++;
  const int base = detail::baseGarrison(s.type, s.owner != 0);
  if (!(s.flags & SS_BESIEGED) && chance(0.35f * span)) {
    if (s.garrison < base) s.garrison++;
    else if (s.garrison > base) s.garrison--;
  }
  // ---- prosperity and mood
  const bool war = K && [&] {
    const auto ix = kingdomIx_.find(K->id);
    return ix != kingdomIx_.end() && (size_t)ix->second < warNow_.size() && warNow_[(size_t)ix->second] != 0;
  }();
  const float purse = K ? std::clamp(K->wealth / std::max(0.1f, K->pop), 0.0f, 2.0f) : 0.5f;
  const int ptarget = std::clamp((int)std::lround(25 + s.food * 0.35f + purse * 10 - s.damage * 0.4f - ((s.flags & SS_OCCUPIED) ? 15 : 0)), 0, 100);
  if (s.prosperity < ptarget) s.prosperity = (uint8_t)std::min(ptarget, s.prosperity + span);
  else if (s.prosperity > ptarget) s.prosperity = (uint8_t)std::max(ptarget, s.prosperity - span);
  int mood = (int)std::lround(0.45f * s.food + 0.3f * s.prosperity + 25 - s.damage * 0.2f);
  if (s.flags & SS_OCCUPIED) mood -= 15;
  if (s.flags & SS_FAMINE) mood -= 20;
  if (s.flags & SS_BESIEGED) mood -= 25;
  if (s.flags & SS_REFUGEES) mood -= 5;
  if (war) mood -= 8;
  mood = std::clamp(mood, 0, 100);
  const int ms = 3 * span;
  if (s.mood < mood) s.mood = (uint8_t)std::min(mood, s.mood + ms);
  else if (s.mood > mood) s.mood = (uint8_t)std::max(mood, s.mood - ms);
  if (s.mood < 25 && !(s.flags & SS_UNREST)) { s.flags |= SS_UNREST; s.changedDay = dayU16(day_); }
  else if (s.mood > 35 && (s.flags & SS_UNREST)) { s.flags &= (uint16_t)~SS_UNREST; s.changedDay = dayU16(day_); }
}

// ---------------------------------------------------------------- owners, captures, raids, ruins
void Realm::giveSite(SettlementState& s, Gid to, int day) {
  const Gid was = s.owner;
  if (KingdomState* k = kingdomMut(was)) {
    k->settlements.erase(std::remove(k->settlements.begin(), k->settlements.end(), s.site), k->settlements.end());
    if (k->capital == s.site) k->capital = 0;
  }
  s.owner = to;
  if (KingdomState* k = kingdomMut(to))
    if (std::find(k->settlements.begin(), k->settlements.end(), s.site) == k->settlements.end()) k->settlements.push_back(s.site);
  s.flags &= (uint16_t)~SS_FRONTIER;
  if (!to) s.flags |= SS_FRONTIER;
  if ((s.flags & SS_GARRISON) && s.garrisonOf != to) { s.flags &= (uint16_t)~SS_GARRISON; s.garrisonOf = 0; }
  s.changedDay = dayU16(day);
}

void Realm::capture(Gid site, Gid owner, int day, War* w) {
  SettlementState& s = state(site);
  const Gid was = s.owner;
  if (was == owner) return;
  const bool wasCapital = [&] { const KingdomState* O = kingdom(was); return O && O->capital == site; }();
  giveSite(s, owner, day);
  // (M4 integration) a town that changes hands is no longer besieged: a siege still open there ends with it (the
  // HUD used to read "THEOCRACY OF X / BESIEGED BY X" after a forced take, and the camp stayed)
  // (fixer M4 r3) a third kingdom's siege there is resolved as broken, the way tickSieges breaks one: its losses, its war's
  // score, the news line, and resolveDay today (so the 60-day prune counts from now)
  for (Siege& g : sieges_) {
    if (g.over || g.site != site) continue;
    g.over = true;
    g.attackerWon = g.attacker == owner;
    g.resolveDay = dayU16(day);
    if (g.attackerWon) continue;
    if (KingdomState* A = kingdomMut(g.attacker)) {
      A->military = std::max(0.5f, A->military - g.atk * 0.35f);
      A->exhaustion = std::min(1.0f, A->exhaustion + 0.1f);
    }
    for (War& x : wars_)
      if (x.id == g.war && !x.endDay) {
        const float sign = g.attacker == x.attacker ? 1.0f : -1.0f;
        x.score = std::clamp(x.score - sign * 0.12f, -1.0f, 1.0f);
      }
    addEvent(EvType::SiegeBroken, g.defender, g.attacker, site, s.gx, s.gy, day);
  }
  s.flags &= (uint16_t)~SS_BESIEGED;
  s.flags |= SS_OCCUPIED;
  s.takenDay = dayU16(day);
  s.damage = (uint8_t)std::min(100, s.damage + 30);
  s.popPct = (uint8_t)std::max(0, s.popPct - 15);
  s.garrison = (uint8_t)(owner ? 10 : 0);
  const KingdomState* N = kingdom(owner);
  const KingdomState* O = kingdom(was);
  if (N && O && N->culture != O->culture) { s.flags |= SS_GARRISON; s.garrisonOf = owner; }
  if (w) { w->taken.push_back(site); w->takenFrom.push_back(was); }
  addEvent(EvType::TownTaken, owner, was, site, s.gx, s.gy, day);
  if (KingdomState* k = kingdomMut(owner)) recount(*k);
  if (KingdomState* k = kingdomMut(was)) {
    if (wasCapital) {
      k->capitalLostDay = dayU16(std::max(1, day));
      k->stability = std::max(0.0f, k->stability - 0.4f);
    }
    recount(*k);
    if (k->settlements.empty() && !k->fallen) fallKingdom(*k, day, owner);
  }
}

void Realm::burn(Gid site, Gid by, int day) {
  SettlementState& s = state(site);
  s.flags |= SS_BURNED;
  s.damage = 80;
  s.popPct = (uint8_t)std::max(0, s.popPct - 50);
  s.changedDay = dayU16(day);
  s.burnDay = dayU16(day);
  const Gid site0 = s.site, owner0 = s.owner;
  const int32_t gx = s.gx, gy = s.gy;
  addEvent(EvType::TownBurned, by, owner0, site0, gx, gy, day);
  // refugees go to the nearest settlement of the same owner (or any, in the wildlands)
  SettlementState* best = nullptr;
  int64_t bd = INT64_MAX;
  for (SettlementState& o : settlements_) {
    if (o.site == site0 || (o.flags & (SS_BURNED | SS_ABANDONED))) continue;
    if (owner0 && o.owner != owner0) continue;
    const int64_t d = dist2(o.gx, o.gy, gx, gy);
    if (d < bd) { bd = d; best = &o; }
  }
  if (best) {
    best->flags |= SS_REFUGEES;
    best->refugeesFrom = site0;
    best->refugeesDay = dayU16(day);
    best->changedDay = dayU16(day);
    addEvent(EvType::Refugees, best->owner, owner0, best->site, best->gx, best->gy, day);
  }
  // a raid by a realm (not a forced burn) may empty the place for good
  if (by) {
    SettlementState& t = state(site0);
    const bool village = t.type == (uint8_t)SiteType::Village;
    if ((village && t.popPct <= 50 && chance(0.4f)) || (!village && t.popPct <= 25 && chance(0.5f))) {
      t.flags |= SS_ABANDONED;
      t.markDay = dayU16(day);
    }
  }
}

void Realm::ruinSite(SettlementState& s, FallCause cause, Gid destroyer, int day) {
  s.flags |= SS_RUINED;
  s.changedDay = dayU16(day);
  SimRuin r;
  r.site = s.site; r.day = dayU16(day); r.cause = cause; r.destroyer = destroyer; r.owner = s.owner; r.home = s.home;
  simRuins_.erase(std::remove_if(simRuins_.begin(), simRuins_.end(), [&](const SimRuin& x) { return x.site == s.site; }), simRuins_.end());
  simRuins_.push_back(r);
  if (simRuins_.size() > 64) simRuins_.erase(simRuins_.begin());
  ruins_.erase(s.site);
  addEvent(EvType::Ruined, s.owner, destroyer, s.site, s.gx, s.gy, day);
}

void Realm::fallKingdom(KingdomState& k, int day, Gid by) {
  if (k.fallen) return;
  k.fallen = true;
  k.famine = false;
  const Gid id = k.id;
  const int32_t x = capX(k), y = capY(k);
  addEvent(EvType::KingdomFell, id, by, 0, x, y, day);
  for (War& w : wars_)
    if (!w.endDay && (w.attacker == id || w.defender == id)) endWar(w, day);
  if (KingdomState* K = kingdomMut(id)) { K->tier = Tier::Latent; K->military = 0; }
}

// ---------------------------------------------------------------- wars and sieges
uint32_t Realm::declareWar(Gid attacker, Gid defender, WarCause cause, int day, bool forced) {
  War w;
  w.id = nextWar_++;
  w.attacker = attacker; w.defender = defender;
  w.startDay = dayU16(std::max(1, day));
  w.cause = cause;
  w.forced = forced;
  const KingdomState* A = kingdom(attacker);
  const KingdomState* D = kingdom(defender);
  const uint64_t h = ew::mix64(seed_ ^ ((uint64_t)w.id * 0x9E3779B97F4A7C15ull) ^ attacker);
  w.goal = WarGoal::Conquer;
  if (A && A->gov == (uint8_t)cult::Government::Khanate) w.goal = WarGoal::Raid;
  else if (cause == WarCause::Famine && (h & 1)) w.goal = WarGoal::Raid;   // hungry raiders come for the granaries
  std::string base;
  const std::string an = A ? A->name : std::string("THE NORTH"), dn = D ? D->name : std::string("THE SOUTH");
  switch (cause) {
    case WarCause::Famine: { const char* const n[] = {"THE HUNGER WAR", "THE WAR OF THE EMPTY GRANARIES", "THE BREAD WAR", "THE WAR OF THE LEAN YEAR"}; base = n[(h >> 4) % 4]; break; }
    case WarCause::BrokenTrade: { const char* const n[] = {"THE GRAIN WAR", "THE WAR OF THE BROKEN BARGAIN", "THE WAR OF THE SPOILED OATH", "THE MERCHANTS' WAR"}; base = n[(h >> 4) % 4]; break; }
    case WarCause::Succession: base = "THE WAR OF THE " + an + " SUCCESSION"; break;
    case WarCause::Revenge: { const char* const n[] = {"THE WAR OF RECKONING", "THE WAR OF THE LOST TOWNS", "THE WAR OF OLD WOUNDS"}; base = n[(h >> 4) % 3]; break; }
    case WarCause::Conquest: base = "THE " + an + " CONQUEST"; break;
    case WarCause::Raiders: base = "THE RAIDING SEASON"; break;
    default: base = std::string("THE WAR OF THE ") + ADJ[(h >> 8) % 16] + " " + NOUN[(h >> 16) % 16]; break;
  }
  if (forced && cause == WarCause::BorderDispute) base = "THE WAR OF " + an + " AND " + dn;
  int same = 0;
  for (const War& o : wars_) if (o.name == base || o.name.find(base) != std::string::npos) same++;
  static const char* const ORD[] = {"SECOND", "THIRD", "FOURTH", "FIFTH"};
  w.name = same == 0 ? base : (base.compare(0, 4, "THE ") == 0 ? "THE " + std::string(ORD[std::min(same - 1, 3)]) + " " + base.substr(4) : base);
  wars_.push_back(w);
  if (Relation* r = relMut(attacker, defender, true)) {
    r->state = Rel::War;
    r->tension = Tension::Skirmishes;
    r->opinion = (int8_t)std::min<int>(r->opinion, -60);
    r->sinceDay = dayU16(day);
    r->stepDay = dayU16(day);
    r->marchDay = 0;
    r->foodDeal = false;
    r->seller = 0;
    if (!r->aggrieved) { r->aggrieved = attacker; r->cause = cause; }
  }
  int32_t ex = 0, ey = 0;
  if (D) { ex = capX(*D); ey = capY(*D); }
  addEvent(EvType::WarDeclared, attacker, defender, 0, ex, ey, day).mag = (int16_t)(w.id & 0x7FFF);
  return w.id;
}

void Realm::endWar(War& w, int day) {
  if (w.endDay) return;
  w.endDay = dayU16(std::max(1, day));
  if (w.endDay <= w.startDay) w.endDay = (uint16_t)(w.startDay + 1);
  KingdomState* A = kingdomMut(w.attacker);
  KingdomState* D = kingdomMut(w.defender);
  // the treaty: a defender who won takes back what it lost; otherwise the captured towns are ceded
  if (w.score <= -0.15f)
    for (size_t i = 0; i < w.taken.size(); i++) {
      const Gid from = i < w.takenFrom.size() ? w.takenFrom[i] : 0;
      auto it = settlementIx_.find(w.taken[i]);
      if (it == settlementIx_.end() || from != w.defender || !D || D->fallen) continue;
      SettlementState& s = settlements_[(size_t)it->second];
      if (s.owner != w.attacker) continue;
      giveSite(s, w.defender, day);
      s.flags &= (uint16_t)~SS_OCCUPIED;
      addEvent(EvType::TownTaken, w.defender, w.attacker, s.site, s.gx, s.gy, day).mag = 1;   // mag 1: by treaty
      if (KingdomState* a = kingdomMut(w.attacker)) recount(*a);
      if (KingdomState* d = kingdomMut(w.defender)) recount(*d);
    }
  // tribute from the loser
  A = kingdomMut(w.attacker);
  D = kingdomMut(w.defender);
  if (A && D) {
    KingdomState* win = w.score >= 0.15f ? A : w.score <= -0.15f ? D : nullptr;
    KingdomState* lose = win == A ? D : win == D ? A : nullptr;
    if (win && lose && lose->wealth > 0) { const float t = lose->wealth * 0.25f; lose->wealth -= t; win->wealth += t; }
  }
  for (Siege& g : sieges_)
    if (g.war == w.id && !g.over) {
      g.over = true;
      auto it = settlementIx_.find(g.site);
      if (it != settlementIx_.end()) settlements_[(size_t)it->second].flags &= (uint16_t)~SS_BESIEGED;
    }
  int32_t x = 0, y = 0;
  if (D) { x = capX(*D); y = capY(*D); }
  addEvent(EvType::Peace, w.attacker, w.defender, 0, x, y, day).mag = (int16_t)std::lround(w.score * 100);
  if (Relation* r = relMut(w.attacker, w.defender, false)) {
    r->opinion = (int8_t)std::min<int>(r->opinion, -30);
    r->state = r->opinion < -40 ? Rel::Rivalry : Rel::Peace;
    r->tension = Tension::Strained;
    r->stepDay = w.endDay;
    r->marchDay = 0;
    r->truceUntil = dayU16(day + YEAR);
    r->foodDeal = false;
    r->seller = 0;
    r->aggrieved = w.score >= 0 ? w.defender : w.attacker;
    r->cause = WarCause::Revenge;
  }
}

uint32_t Realm::startSiege(War* w, Gid site, Gid attacker, int day, bool forced) {
  SettlementState& s = state(site);
  Siege g;
  g.id = nextSiege_++;
  g.war = w ? w->id : 0;
  g.site = site; g.attacker = attacker; g.defender = s.owner;
  g.startDay = dayU16(day);
  const KingdomState* A = kingdom(attacker);
  const KingdomState* D = kingdom(s.owner);
  const float units = s.garrison / 10.0f + 1.0f;
  g.atk = A ? std::min(0.5f * A->military, 4.0f * units) : 4.0f;
  g.def = units + (!forced && D ? 0.15f * D->military : 0.0f);
  const bool walls = s.type != (uint8_t)SiteType::Village;
  g.resolveDay = (uint16_t)(g.startDay + 3 + s.garrison / 20 + (walls ? 4 : 0));
  g.raid = w && w->goal == WarGoal::Raid && !forced;
  g.forced = forced;
  // the camp: south of the heart, beyond a town's reach (the WARDS lane picks the real ground: 14-30 tiles outside)
  const int32_t off = s.type == (uint8_t)SiteType::City ? 120 : s.type == (uint8_t)SiteType::Town ? 80 : 56;
  g.campX = s.gx; g.campY = s.gy + off;
  s.flags |= SS_BESIEGED;
  s.changedDay = dayU16(day);
  const Gid def = s.owner;
  const int32_t gx = s.gx, gy = s.gy;
  sieges_.push_back(g);
  addEvent(EvType::SiegeBegun, attacker, def, site, gx, gy, day);
  return g.id;
}

void Realm::tickSieges() {
  for (size_t i = 0; i < sieges_.size(); i++) {
    if (sieges_[i].over) continue;
    War* w = nullptr;
    for (War& x : wars_) if (x.id == sieges_[i].war) w = &x;
    auto it = settlementIx_.find(sieges_[i].site);
    if (it == settlementIx_.end()) { sieges_[i].over = true; continue; }
    if (w && w->endDay) { sieges_[i].over = true; settlements_[(size_t)it->second].flags &= (uint16_t)~SS_BESIEGED; continue; }
    if (day_ < (int)sieges_[i].resolveDay) continue;
    KingdomState* A = kingdomMut(sieges_[i].attacker);
    KingdomState* D = kingdomMut(sieges_[i].defender);
    // a dormant side pauses the siege (VISION_PLAN 4.4); forced sieges resolve regardless
    if (!sieges_[i].forced && ((A && A->tier != Tier::Active) || (D && D->tier != Tier::Active))) continue;
    Siege& g = sieges_[i];
    SettlementState& s = settlements_[(size_t)it->second];
    const float wf = s.type != (uint8_t)SiteType::Village ? 1.5f : 1.0f;
    bool won;
    if (g.forced) won = g.atk > g.def;
    else if (g.playerJoined) won = g.atk > g.def * wf;   // the player's part decides, readably
    else won = frand() < g.atk / std::max(0.01f, g.atk + g.def * wf);
    g.over = true;
    g.attackerWon = won;
    s.flags &= (uint16_t)~SS_BESIEGED;
    s.changedDay = dayU16(day_);
    if (A) { A->military = std::max(0.5f, A->military - g.atk * (won ? 0.15f : 0.35f)); A->exhaustion = std::min(1.0f, A->exhaustion + (won ? 0.05f : 0.1f)); }
    if (D) { D->military = std::max(0.5f, D->military - g.def * (won ? 0.3f : 0.12f)); D->exhaustion = std::min(1.0f, D->exhaustion + (won ? 0.1f : 0.05f)); }
    if (w) {
      const float sign = g.attacker == w->attacker ? 1.0f : -1.0f;
      const float d = won ? (s.type == (uint8_t)SiteType::City ? 0.35f : s.type == (uint8_t)SiteType::Town ? 0.25f : 0.18f) : -0.12f;
      w->score = std::clamp(w->score + sign * d, -1.0f, 1.0f);
    }
    if (g.playerJoined && ((g.playerSide == 1) == won)) renown_.fame = std::min(1000000, renown_.fame + 10);
    const Gid site = g.site, atk = g.attacker, def = g.defender;
    const bool raid = g.raid;
    if (won) {
      if (raid) {
        burn(site, atk, day_);
        if (KingdomState* a = kingdomMut(atk)) a->food = std::min(1.4f, a->food + 0.06f);
        if (KingdomState* d = kingdomMut(def)) d->food = std::max(0.0f, d->food - 0.04f);
      } else {
        capture(site, atk, day_, w);
      }
    } else {
      const SettlementState* t = settlement(site);
      addEvent(EvType::SiegeBroken, def, atk, site, t ? t->gx : 0, t ? t->gy : 0, day_);
    }
  }
}

void Realm::tickWars(int span) {
  for (size_t i = 0; i < wars_.size(); i++) {
    if (wars_[i].endDay) continue;
    KingdomState* A = kingdomMut(wars_[i].attacker);
    KingdomState* D = kingdomMut(wars_[i].defender);
    if (!A || !D || A->fallen || D->fallen) { endWar(wars_[i], day_); continue; }
    const int len = day_ - (int)wars_[i].startDay;
    if (A->tier != Tier::Active || D->tier != Tier::Active) {
      wars_[i].pausedDays = (uint16_t)std::min(65535, wars_[i].pausedDays + span);
      if (len >= 90) endWar(wars_[i], day_);
      continue;
    }
    A->exhaustion = std::min(1.0f, A->exhaustion + (0.0035f + (A->famine ? 0.002f : 0.0f)) * span);
    D->exhaustion = std::min(1.0f, D->exhaustion + (0.0035f + (D->famine ? 0.002f : 0.0f)) * span);
    bool openA = false, openD = false;
    for (const Siege& g : sieges_)
      if (!g.over && g.war == wars_[i].id) { if (g.attacker == A->id) openA = true; else openD = true; }
    const float ratio = A->military / std::max(0.5f, D->military);
    const Gid aid = A->id, did = D->id;
    // the attacker besieges the defender's border settlement nearest its own land
    if (!openA && len >= 2 && chance(0.10f * std::clamp(ratio, 0.5f, 2.0f) * span)) {
      Gid best = 0;
      int64_t bd = INT64_MAX;
      const bool raid = wars_[i].goal == WarGoal::Raid;
      for (Gid g : D->settlements) {
        const SettlementState* s = settlement(g);
        if (!s || (s->flags & (SS_BESIEGED | SS_ABANDONED | SS_BURNED))) continue;
        int64_t d = INT64_MAX;
        for (Gid m : A->settlements) if (const SettlementState* t = settlement(m)) d = std::min(d, dist2(s->gx, s->gy, t->gx, t->gy));
        if (raid && s->type != (uint8_t)SiteType::Village) d *= 3;
        if (d < bd) { bd = d; best = g; }
      }
      if (best) startSiege(&wars_[i], best, aid, day_, false);
    }
    // the defender tries to take back what it lost
    if (!openD) {
      Gid best = 0;
      for (size_t t = 0; t < wars_[i].taken.size(); t++) {
        const SettlementState* s = settlement(wars_[i].taken[t]);
        if (s && s->owner == aid && t < wars_[i].takenFrom.size() && wars_[i].takenFrom[t] == did && !(s->flags & SS_BESIEGED)) { best = s->site; break; }
      }
      if (best && chance(0.06f * std::clamp(1.0f / std::max(0.1f, ratio), 0.5f, 2.0f) * span)) startSiege(&wars_[i], best, did, day_, false);
    }
    War& w = wars_[i];
    A = kingdomMut(aid);
    D = kingdomMut(did);
    if (!A || !D) continue;
    const bool tired = A->exhaustion > 0.7f || D->exhaustion > 0.7f || std::fabs(w.score) > 0.6f;
    if (len >= 90 || (len >= 20 && tired) || (len >= 45 && !openA && !openD && chance(0.02f * span))) endWar(w, day_);
  }
}

// ---------------------------------------------------------------- diplomacy: the slow road to war (15.6.3)
void Realm::setTension(Relation& r, Tension t, int day) {
  r.tension = t;
  r.stepDay = dayU16(day);
  if (t < Tension::Skirmishes) r.marchDay = 0;
  if (t == Tension::Calm) r.aggrieved = 0;
  if (r.state != Rel::War) r.state = r.opinion < -50 ? Rel::Rivalry : (r.state == Rel::Rivalry ? Rel::Peace : r.state);
}

Gid Realm::borderSite(const KingdomState& from, const KingdomState& to) const {
  // the settlement of `to` nearest `from`'s capital (where incidents happen)
  Gid best = to.capital;
  int64_t bd = INT64_MAX;
  const int32_t x = capX(from), y = capY(from);
  for (Gid g : to.settlements)
    if (const SettlementState* s = settlement(g)) {
      const int64_t d = dist2(s->gx, s->gy, x, y);
      if (d < bd) { bd = d; best = g; }
    }
  return best;
}

void Realm::tickDiplomacy(int span) {
  const int year = day_ / YEAR;
  auto aggr = [](const KingdomState* k) {
    if (!k) return 0.5f;
    float f = 0.55f + 0.6f * k->values[V_MARTIAL] / 255.0f + 0.3f * k->values[V_EXPANSIONIST] / 255.0f;
    if (k->ruler.traits & RT_AGGRESSIVE) f += 0.5f;
    if (k->ruler.traits & RT_CAUTIOUS) f -= 0.3f;
    if (k->ruler.traits & RT_HONOURABLE) f -= 0.1f;
    f -= k->exhaustion;
    if (k->stability < 0.35f) f -= 0.2f;
    return std::clamp(f, 0.15f, 2.0f);
  };
  auto hungry = [](const KingdomState* k) { return k && (k->famine || k->food < 0.12f); };
  auto at = [&](EvType t, KingdomState* a, KingdomState* b, Gid site) {
    const SettlementState* s = settlement(site);
    addEvent(t, a ? a->id : 0, b ? b->id : 0, site, s ? s->gx : (a ? capX(*a) : 0), s ? s->gy : (a ? capY(*a) : 0), day_);
  };
  for (size_t i = 0; i < relations_.size(); i++) {
    Relation& r = relations_[i];
    KingdomState* A = kingdomMut(r.a);
    KingdomState* B = kingdomMut(r.b);
    if (!A || !B || A->fallen || B->fallen) continue;
    if (A->tier != Tier::Active || B->tier != Tier::Active) continue;
    const bool war = warBetween(r.a, r.b) != nullptr;
    // opinion drifts toward its base (trade warms, tension and war chill)
    int target = r.base + (r.state == Rel::Trade ? 10 : 0) + (r.foodDeal ? 5 : 0) - 10 * (int)r.tension;
    if (war) target = std::min(target, -60);
    if (chance(0.25f * span) && target != r.opinion) r.opinion = (int8_t)std::clamp(r.opinion + (target > r.opinion ? 1 : -1), -100, 100);
    if (war) continue;
    if (r.state == Rel::War) r.state = Rel::Peace;
    // discovery: realms two cells apart meet through merchants and sailors
    if (r.state == Rel::Unknown) {
      const float p = 0.0005f + 0.003f * (A->values[V_MERCANTILE] + B->values[V_SEAFARING]) / 510.0f;
      if (chance(p * span)) {
        r.state = Rel::Contact;
        addEvent(EvType::FirstContact, r.a, r.b, 0, (capX(*A) + capX(*B)) / 2, (capY(*A) + capY(*B)) / 2, day_);
      }
      continue;
    }
    // trade
    if ((r.state == Rel::Contact || r.state == Rel::Peace) && r.opinion > 0 && r.tension == Tension::Calm &&
        chance(0.004f * span * (A->values[V_MERCANTILE] + B->values[V_MERCANTILE]) / 510.0f)) {
      r.state = Rel::Trade;
      at(EvType::TradeDeal, A, B, A->capital);
    }
    // food deals: grain flows; a failed harvest or a famine on the seller's side breaks the deal
    if (r.foodDeal) {
      KingdomState* S = r.seller == r.a ? A : B;
      KingdomState* Bu = S == A ? B : A;
      const bool failed = S->food < 0.3f || S->famine || (S->harvestYear == year && S->lastHarvest < 0.75f);
      if (failed) {
        r.foodDeal = false;
        r.seller = 0;
        r.opinion = (int8_t)std::max(-100, r.opinion - 20);
        WorldEvent& e = addEvent(EvType::TradeBroken, S->id, Bu->id, Bu->capital, capX(*Bu), capY(*Bu), day_);
        e.mag = 1;   // a food deal
        addEvent(EvType::PricesRising, Bu->id, S->id, Bu->capital, capX(*Bu), capY(*Bu), day_);
        if (r.tension < Tension::TradeBroken && day_ >= (int)r.truceUntil) {
          r.aggrieved = Bu->id;
          r.cause = WarCause::BrokenTrade;
          setTension(r, Tension::TradeBroken, day_);
          r.lastIncident = dayU16(day_);
        }
      } else {
        const float amt = std::clamp(S->yieldRatio - 1.0f, 0.06f, 0.25f) * span / (float)YEAR;
        S->food -= amt; Bu->food += amt;
        S->wealth += amt * 3; Bu->wealth -= amt * 3;
      }
    } else if (r.tension == Tension::Calm && r.state != Rel::Rivalry && day_ >= (int)r.truceUntil) {
      const bool aSells = A->yieldRatio > 1.03f && A->food > 0.55f && !A->famine && (B->yieldRatio < 1.0f || B->food < 0.35f);
      const bool bSells = B->yieldRatio > 1.03f && B->food > 0.55f && !B->famine && (A->yieldRatio < 1.0f || A->food < 0.35f);
      if ((aSells || bSells) && chance(0.01f * span)) {
        r.foodDeal = true;
        r.seller = aSells ? r.a : r.b;
        r.state = Rel::Trade;
        at(EvType::TradeDeal, A, B, (aSells ? B : A)->capital);
        events_.back().mag = 1;
      }
    }
    // ---- the ladder
    if (day_ < (int)r.truceUntil) continue;
    KingdomState* G = r.aggrieved == r.a ? A : r.aggrieved == r.b ? B : nullptr;
    KingdomState* O = G == A ? B : A;
    const int since = day_ - (int)r.stepDay;
    bool gone = true;   // the cause is gone: tempers cool
    if (G) switch (r.cause) {
        case WarCause::Famine: case WarCause::BrokenTrade: gone = !G->famine && G->food > 0.4f; break;
        case WarCause::Revenge: gone = G->military < 0.8f * O->military; break;
        case WarCause::Succession: gone = since > 240; break;
        default: gone = r.opinion > -25; break;
      }
    switch (r.tension) {
      case Tension::Calm: {
        KingdomState* H = A->famine && !B->famine && B->food > 0.45f ? A : (B->famine && !A->famine && A->food > 0.45f ? B : nullptr);
        if (H && chance(T.calmFamine * span)) {
          r.aggrieved = H->id;
          r.cause = WarCause::Famine;
          setTension(r, Tension::Strained, day_);
          KingdomState* other = H == A ? B : A;
          at(EvType::PricesRising, H, other, H->capital);
          break;
        }
        if (r.opinion < -45) {
          KingdomState* X = aggr(A) >= aggr(B) ? A : B;
          if (chance(T.calmGrudge * span * aggr(X))) {
            r.aggrieved = X->id;
            r.cause = X->values[V_EXPANSIONIST] > 170 ? WarCause::Conquest : WarCause::BorderDispute;
            setTension(r, Tension::Strained, day_);
            break;
          }
        }
        // revenge: a realm strong again eyes the towns the other took from it
        for (int side = 0; side < 2; side++) {
          KingdomState* X = side ? B : A;
          KingdomState* Y = side ? A : B;
          if (X->stability < 0.5f || X->military < 1.1f * Y->military) continue;
          bool lost = false;
          for (Gid g : Y->settlements) if (const SettlementState* s = settlement(g)) if (s->home == X->id) { lost = true; break; }
          if (lost && chance(T.calmRevenge * span)) {
            r.aggrieved = X->id;
            r.cause = WarCause::Revenge;
            setTension(r, Tension::Strained, day_);
            break;
          }
        }
        break;
      }
      case Tension::Strained: {
        if (gone && chance(T.cool * span)) { setTension(r, Tension::Calm, day_); break; }
        if (!G || since < 8) break;
        const bool food = r.cause == WarCause::Famine || r.cause == WarCause::BrokenTrade;
        const float p = food ? (hungry(G) ? T.strainedHungry : 0.0f) : T.strainedGrudge * aggr(G);
        if (chance(p * span)) {
          setTension(r, Tension::BorderTension, day_);
          at(EvType::BorderIncident, G, O, borderSite(*G, *O));
          r.lastIncident = dayU16(day_);
        }
        break;
      }
      case Tension::TradeBroken:
        if (gone && chance(T.cool * span)) { setTension(r, Tension::Strained, day_); break; }
        if (G && since >= 10 && hungry(G) && chance(T.tbHungry * span)) {
          setTension(r, Tension::BorderTension, day_);
          at(EvType::BorderIncident, G, O, borderSite(*G, *O));
          r.lastIncident = dayU16(day_);
        }
        break;
      case Tension::BorderTension:
        if (gone && chance(T.cool * span)) { setTension(r, Tension::Strained, day_); break; }
        if (G && since >= 10 && chance(T.btSkirmish * span * aggr(G) * (hungry(G) ? 1.5f : 1.0f))) {
          setTension(r, Tension::Skirmishes, day_);
          at(EvType::Skirmish, G, O, borderSite(*G, *O));
          at(EvType::PricesRising, O, G, O->capital);
          r.lastIncident = dayU16(day_);
        }
        break;
      case Tension::Skirmishes: {
        if (!G) { setTension(r, Tension::BorderTension, day_); break; }
        if (r.marchDay) {
          if (day_ >= (int)r.marchDay) {
            const bool stale = day_ - (int)r.marchDay > 10;   // a side lay dormant meanwhile: the host went home
            r.marchDay = 0;
            if (stale) { setTension(r, Tension::BorderTension, day_); break; }
            if (G->military < 0.55f * O->military) { setTension(r, Tension::BorderTension, day_); break; }   // it backs down
            declareWar(G->id, O->id, r.cause, day_, false);
          }
          break;
        }
        if (gone && chance(T.cool * span)) { setTension(r, Tension::BorderTension, day_); break; }
        if (chance(T.skirmishEvent * span)) { at(EvType::Skirmish, G, O, borderSite(*G, *O)); r.lastIncident = dayU16(day_); }
        if (since >= 7) {
          const float ratio = G->military / std::max(0.5f, O->military);
          const float p = T.warDecide * aggr(G) * std::clamp(ratio, 0.4f, 1.8f) * (hungry(G) ? 1.4f : 1.0f);
          if (chance(p * span)) {
            if (day_ - (int)r.lastIncident > 60) { at(EvType::Skirmish, G, O, borderSite(*G, *O)); r.lastIncident = dayU16(day_); }
            at(EvType::TroopsMarching, G, O, borderSite(*O, *G));
            r.marchDay = dayU16(day_ + 3 + (int)(roll() % 8));
          }
        }
        break;
      }
      default: break;
    }
  }
}

// ---------------------------------------------------------------- rulers and civil wars
void Realm::succession(KingdomState& k, int day, bool died) {
  if (died) addEvent(EvType::RulerDied, k.id, 0, k.capital, capX(k), capY(k), day).mag = k.ruler.age;
  bool newHouse = false;
  float hit = 0.05f;
  switch ((cult::Inheritance)k.inherit) {
    case cult::Inheritance::Gavelkind: hit = 0.15f; break;
    case cult::Inheritance::Elective: newHouse = chance(0.5f); hit = 0.1f; break;
    case cult::Inheritance::Merit: newHouse = chance(0.7f); break;
    case cult::Inheritance::Tanistry: newHouse = chance(0.4f); hit = 0.2f; break;
    default: break;
  }
  const bool female = (cult::Inheritance)k.inherit == cult::Inheritance::Matrilineal ? chance(0.85f) : chance(0.3f);
  const uint64_t h = roll();
  if (src_ && src_->seed() == seed_) {
    const cult::Culture& C = src_->culture(k.culture);
    k.ruler.name = cult::personName(C, (uint32_t)(h >> 8), female);
    if (newHouse) k.ruler.house = "HOUSE " + cult::personName(C, (uint32_t)(h >> 32) ^ 0x484F5553u, false);
  } else {
    k.ruler.name = k.ruler.name + (k.ruler.name.size() < 12 ? " II" : "");
  }
  k.ruler.female = female;
  k.ruler.age = (uint8_t)(newHouse ? 30 + (h >> 40) % 30 : 16 + (h >> 40) % 30);
  k.ruler.traits = detail::traitsFor(k.values, ew::mix64(h));
  k.ruler.sinceDay = dayU16(std::max(1, day));
  if (newHouse) k.ruler.dynasty = ew::mix64(h ^ 0x44594E41ull);
  k.stability = std::max(0.0f, k.stability - hit);
  addEvent(EvType::Succession, k.id, 0, k.capital, capX(k), capY(k), day).mag = newHouse ? 1 : 0;
}

void Realm::tickRulers(ew::EndlessSource& src, int span) {
  (void)src;
  const bool newYear = day_ / YEAR != (day_ - span) / YEAR;
  for (size_t i = 0; i < kingdoms_.size(); i++) {
    KingdomState& k = kingdoms_[i];
    if (k.fallen || k.tier != Tier::Active) continue;
    if (newYear && k.ruler.age < 250) k.ruler.age++;
    const bool war = i < warNow_.size() && warNow_[i];
    const float p = T.death * std::exp((k.ruler.age - 40) / 9.0f) + (war ? 0.0002f : 0.0f);
    if (chance(p * span)) succession(k, day_, true);
  }
}

void Realm::tickCivil(ew::EndlessSource& src, int span) {
  std::vector<Gid> split;
  for (const KingdomState& k : kingdoms_) {
    if (k.fallen || k.tier != Tier::Active || k.stability >= 0.2f) continue;
    int living = 0;
    for (Gid g : k.settlements) if (const SettlementState* s = settlement(g)) if (!(s->flags & SS_ABANDONED)) living++;
    if (living >= 3 && chance(T.civil * span)) split.push_back(k.id);
  }
  for (Gid id : split) if (KingdomState* k = kingdomMut(id)) splitRealm(&src, *k, day_);
}

Gid Realm::splitRealm(ew::EndlessSource* src, KingdomState& kref, int day) {
  const Gid pid = kref.id;
  std::vector<Gid> living;
  for (Gid g : kref.settlements) if (const SettlementState* s = settlement(g)) if (!(s->flags & SS_ABANDONED)) living.push_back(g);
  if (living.size() < 3) return 0;
  // the rebels hold the far half (never the capital)
  const int32_t cx = capX(kref), cy = capY(kref);
  std::vector<std::pair<int64_t, Gid>> far;
  for (Gid g : living) {
    if (g == kref.capital) continue;
    const SettlementState* s = settlement(g);
    far.push_back({-dist2(s->gx, s->gy, cx, cy), g});
  }
  std::sort(far.begin(), far.end());
  const size_t n = std::min(far.size(), std::max<size_t>(1, living.size() / 2));
  if (n == 0) return 0;
  Gid rid = 0;
  for (int tries = 0; tries < 0x800 && (!rid || kingdom(rid)); tries++)
    rid = ew::makeId(ew::idRx(pid), ew::idRy(pid), ew::IdKind::Kingdom, 0x800u + (nextRebel_++ % 0x800u));
  if (kingdom(rid)) return 0;
  KingdomState R;
  R.id = rid;
  R.rebel = true;
  R.parent = pid;
  R.culture = kref.culture;
  R.gov = kref.gov;
  R.inherit = kref.inherit;
  for (int v = 0; v < V_COUNT; v++) R.values[v] = kref.values[v];
  const uint64_t h = ew::mix64(rid ^ seed_ ^ 0x5245424Cull);
  if (src && src->seed() == seed_) {
    const cult::Culture& C = src->culture(kref.culture);
    R.name = cult::kingdomName(C, (uint32_t)h);
    if (R.name == kref.name) R.name = cult::kingdomName(C, (uint32_t)(h >> 20) ^ 0x51u);
    R.ruler.name = cult::personName(C, (uint32_t)(h >> 8), (h & 3) == 0);
    R.ruler.house = "HOUSE " + cult::personName(C, (uint32_t)(h >> 28) ^ 0x484F5553u, false);
  } else {
    R.name = "FREE " + kref.name;
    R.ruler.name = "THE PRETENDER";
    R.ruler.house = "HOUSE OF THE REVOLT";
  }
  static const uint32_t PAL[] = {colour(150, 30, 30), colour(30, 60, 140), colour(30, 110, 50), colour(120, 40, 120),
                                 colour(200, 150, 30), colour(40, 40, 40), colour(200, 200, 190), colour(170, 80, 20),
                                 colour(20, 120, 130), colour(90, 60, 30)};
  R.color = PAL[h % 10];
  if (R.color == kref.color) R.color = PAL[(h + 3) % 10];
  R.color2 = PAL[(h >> 8) % 10];
  if (R.color2 == R.color) R.color2 = PAL[(h + 5) % 10];
  R.emblem = (uint8_t)((kref.emblem + 1 + (h >> 16) % 6) % 8);
  R.ruler.female = (h & 3) == 0;
  R.ruler.title = kref.ruler.title;
  R.ruler.age = (uint8_t)(25 + (h >> 36) % 25);
  R.ruler.traits = (uint8_t)(detail::traitsFor(R.values, h) | RT_AGGRESSIVE);
  R.ruler.sinceDay = dayU16(std::max(1, day));
  R.ruler.dynasty = ew::mix64(h ^ 0x44594E41ull);
  R.foundedDay = day;
  R.tier = Tier::Active;
  R.lastTick = dayU16(day);
  R.stability = 0.5f;
  R.food = kref.food;
  R.harvestDoy = kref.harvestDoy;
  R.harvestYear = kref.harvestYear;
  R.lastHarvest = kref.lastHarvest;
  R.famine = kref.famine;
  R.famineDay = kref.famineDay;
  // split the realm's strength by population
  float popR = 0, popAll = 0;
  for (Gid g : living) {
    const SettlementState* s = settlement(g);
    const float p = detail::sitePop(s->type) * s->popPct / 100.0f;
    popAll += p;
    for (size_t i = 0; i < n; i++) if (far[i].second == g) popR += p;
  }
  const float f = popAll > 0 ? popR / popAll : 0.5f;
  R.wealth = std::max(0.0f, kref.wealth) * f;
  R.military = kref.military * f;
  kref.wealth -= R.wealth;
  kref.military -= R.military;
  kref.stability = 0.45f;   // the malcontents have gone
  const std::string parentName = kref.name;
  kingdoms_.push_back(std::move(R));
  rebuildIndex();
  KingdomState* P = kingdomMut(pid);
  KingdomState* K = kingdomMut(rid);
  for (size_t i = 0; i < n; i++) {
    auto it = settlementIx_.find(far[i].second);
    if (it == settlementIx_.end()) continue;
    SettlementState& s = settlements_[(size_t)it->second];
    giveSite(s, rid, day);
    s.flags &= (uint16_t)~SS_OCCUPIED;
  }
  P = kingdomMut(pid);
  K = kingdomMut(rid);
  recount(*P);
  recount(*K);
  // relations: the old realm wants its land back; the rebels inherit the neighbours' acquaintance
  std::vector<Relation> inherit;
  for (const Relation& r : relations_)
    if ((r.a == pid || r.b == pid) && r.state != Rel::Unknown) inherit.push_back(r);
  for (const Relation& r : inherit) {
    const Gid other = r.a == pid ? r.b : r.a;
    if (other == rid) continue;
    Relation* x = relMut(rid, other, true);
    x->state = Rel::Contact;
    x->base = r.base;
    x->opinion = r.base;
    x->border = r.border;
    x->stepDay = dayU16(day);
  }
  Relation* pr = relMut(pid, rid, true);
  pr->state = Rel::Rivalry;
  pr->base = -40;
  pr->opinion = -60;
  pr->border = (uint8_t)n;
  pr->aggrieved = pid;
  pr->cause = WarCause::Succession;
  pr->tension = Tension::BorderTension;
  pr->stepDay = dayU16(day);
  pr->lastIncident = 0;   // only foreshadowing events (incidents, skirmishes, troops) count as incidents
  K = kingdomMut(rid);
  addEvent(EvType::CivilWar, pid, rid, K->capital, capX(*K), capY(*K), day);
  addEvent(EvType::KingdomRose, rid, pid, K->capital, capX(*K), capY(*K), day);
  (void)parentName;
  stats.instantiated = (int)kingdoms_.size();
  return rid;
}

// ---------------------------------------------------------------- bounds
void Realm::prune() {
  // ended wars: kept two years (the chronicle and the news), at most 24
  wars_.erase(std::remove_if(wars_.begin(), wars_.end(), [&](const War& w) { return w.endDay && day_ - (int)w.endDay > 2 * YEAR; }), wars_.end());
  int ended = 0;
  for (const War& w : wars_) if (w.endDay) ended++;
  for (size_t i = 0; i < wars_.size() && ended > 24;)
    if (wars_[i].endDay) { wars_.erase(wars_.begin() + (std::ptrdiff_t)i); ended--; } else i++;
  // sieges: gone 60 days after they ended
  sieges_.erase(std::remove_if(sieges_.begin(), sieges_.end(), [&](const Siege& g) { return g.over && day_ - (int)g.resolveDay > 60; }), sieges_.end());
  // relations of realms that are gone
  relations_.erase(std::remove_if(relations_.begin(), relations_.end(), [&](const Relation& r) {
                     const KingdomState* a = kingdom(r.a);
                     const KingdomState* b = kingdom(r.b);
                     return !a || !b || a->fallen || b->fallen;
                   }), relations_.end());
  // a traveller leaves a trail of dormant realms: forget the far, untouched ones (they are rebuilt identically later)
  if (kingdoms_.size() > 80) {
    const int32_t px = ew::EndlessSource::kcellOf(focusGx_), py = ew::EndlessSource::kcellOf(focusGy_);
    std::vector<Gid> drop;
    for (const KingdomState& k : kingdoms_) {
      if (k.tier == Tier::Active || k.rebel || k.fallen) continue;
      if (std::max(std::abs(ew::idRx(k.id) - px), std::abs(ew::idRy(k.id) - py)) <= 6) continue;
      bool touched = k.playerRep != 0;
      for (const SettlementState& s : settlements_)
        if ((s.home == k.id || s.owner == k.id) && (s.home != s.owner || s.flags & ~(uint16_t)SS_FRONTIER)) { touched = true; break; }
      for (const War& w : wars_) if (w.attacker == k.id || w.defender == k.id) touched = true;
      if (!touched) drop.push_back(k.id);
      if (kingdoms_.size() - drop.size() <= 64) break;
    }
    if (!drop.empty()) {
      auto gone = [&](Gid g) { return std::find(drop.begin(), drop.end(), g) != drop.end(); };
      kingdoms_.erase(std::remove_if(kingdoms_.begin(), kingdoms_.end(), [&](const KingdomState& k) { return gone(k.id); }), kingdoms_.end());
      rebuildIndex();
      relations_.erase(std::remove_if(relations_.begin(), relations_.end(), [&](const Relation& r) { return gone(r.a) || gone(r.b); }), relations_.end());
      settlements_.erase(std::remove_if(settlements_.begin(), settlements_.end(), [&](const SettlementState& s) { return gone(s.home); }), settlements_.end());
      settlementIx_.clear();
      for (size_t i = 0; i < settlements_.size(); i++) settlementIx_[settlements_[i].site] = (int)i;
      for (Gid g : drop) {
        hist_.erase(g);
        for (int dy = -1; dy <= 1; dy++)
          for (int dx = -1; dx <= 1; dx++)
            scanned_.erase(((uint64_t)(uint32_t)(ew::idRx(g) + dx) << 32) | (uint32_t)(ew::idRy(g) + dy));
      }
      stats.instantiated = (int)kingdoms_.size();
    }
  }
}

// ---------------------------------------------------------------- dormant catch-up (VISION_PLAN 4.4)
void Realm::catchUp(KingdomState& k, int weeks) {
  const int now = day_;
  const Gid id = k.id;
  for (int i = 1; i <= weeks; i++) {
    day_ = std::min(now, (int)k.lastTick + 7 * i);
    KingdomState* K = kingdomMut(id);
    if (!K || K->fallen) break;
    tickEconomy(*K, 7, 1);
    tickFood(*K, 7);
    for (Gid g : std::vector<Gid>(K->settlements)) {
      auto it = settlementIx_.find(g);
      if (it != settlementIx_.end()) tickSettlement(settlements_[(size_t)it->second], kingdomMut(id), 7);
    }
    if (day_ / YEAR != (day_ - 7) / YEAR) if (KingdomState* k2 = kingdomMut(id)) if (k2->ruler.age < 250) k2->ruler.age++;
  }
  day_ = now;
  stats.weeklyTicks += weeks;
  stats.catchUps++;
}

}  // namespace realm
