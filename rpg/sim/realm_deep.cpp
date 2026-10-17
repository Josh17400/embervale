// M12 "Rise and Fall": the deep sim the daily tick grows by itself (rpg/sim/realm.h; VISION_PLAN 4.3 "Rise", 7.6,
// 15.3, 15.6.3, 15.33). REALM lane, phase B. Called once per tick after the civil wars (Realm::tickRise), drawing only
// from the realm's own saved stream, kingdoms in Gid order, camps and lairs in site order. Slow and realistic (15.6.3:
// no "lively" default): every new realm grows from a cause the player can hear of long before.
//   camps      a bandit camp nobody clears grows (faster in the wildlands and beside a weak or warring realm, slower
//              beside a strong one; the land's soldiers sweep it now and then); from strength 55 and 30 days on its
//              warband raids the village it preys on (WarbandRaid: the foreshadowing); from 80, forty days a warband
//              and the village soft, it may take the village: a KO_WARLORD realm (WarlordRose). A camp the player
//              cleared (campCleared) never grows again.
//   warlords   behave like a horde: their wars are raids, and every few weeks they demand tribute of a weaker
//              neighbour (TributePaid buys a little peace; TributeRefused climbs the road to war a rung)
//   lairs      a world boss's lair sleeps for months, wakes (BossWoke) and raids the settlement nearest it every few
//              weeks while awake (beastRaid: damage, prosperity, refugees), going back to sleep two raids in five; a
//              slain beast never raids. One schedule (Lair::nextRaid): the FOES lane plays a raid out in front of the
//              player on its day (Realm::lairRaid); the tick raids the day after when nobody did.
//   claims     a fallen realm whose old towns others hold may crown a successor there (crownSuccessor: a scion of the
//              old house, the old colour, a claim); successors and rebels climb the road to war on the holders
//              (WarCause::Succession) and a claim is fulfilled when the claimant holds every living town of the fallen
//              realm (Reunified)
//   alliances  friends who share a threat ally (AllianceMade); an alliance lapses when the warmth goes. A war declared
//              on a realm calls its allies, its overlord and its vassals to arms (AllyJoined, troops marching; the
//              road to war's Skirmishes rung declares their war some days later, unless they back down)
//   vassals    a crushing defeat may make a vassal (endWar); each year on the bond's day a vassal pays its tribute
//              (TributePaid) or refuses it (TributeRefused) and is free, its overlord aggrieved
// None of it acts for the player's realm (KO_PLAYER): the LORD and SERVICE lanes do, through the Realm's calls.
#include <algorithm>
#include <cmath>
#include "rpg/sim/realm.h"
#include "rpg/world/source.h"

namespace realm {
namespace {

uint16_t dayU16(int day) { return (uint16_t)std::clamp(day, 0, 65535); }
int64_t dist2(int32_t ax, int32_t ay, int32_t bx, int32_t by) { const int64_t dx = ax - bx, dy = ay - by; return dx * dx + dy * dy; }

// ---- tuning (the M12 drama targets; rpg_test --history 5000 --seeds 1..20 checks them)
struct RiseTune {
  float campUp = 0.175f;         // per week: a camp's strength grows a point (more in hard times and weak lands)
  float campDown = 0.22f;        // per week: it loses one (more beside a strong, settled realm)
  float campSweep = 0.0016f;     // per day x the land's strength: its soldiers break the camp's strength
  float warbandRaid = 1.0f / 45; // per day: a warband raids its village
  float warlordRise = 0.015f;    // per day: a ripe warband takes its soft village
  float warlordDemand = 1.0f / 55;   // per day: a warlord demands tribute of a neighbour
  float successor = 0.0035f;     // per day: a fallen realm's remnant crowns a successor (x2 where its old towns seethe)
  float ally = 0.0022f;          // per day: two friends ally (x3 under a common threat)
};
const RiseTune RT;

}  // namespace

void Realm::tickRise(int span) {
  tickCamps(span);
  tickWarlords(span);
  tickLairs(span);
  tickClaims(span);
  tickAlliances(span);
  tickVassals(span);
  // the wars declared today call the defenders' friends to arms
  std::vector<uint32_t> calls;
  calls.swap(allyCalls_);
  for (uint32_t w : calls) callAllies(w, day_);
}

// ---------------------------------------------------------------- camps and warbands
void Realm::tickCamps(int span) {
  for (size_t i = 0; i < camps_.size(); i++) {
    if (camps_[i].flags & (CF_CLEARED | CF_WARLORD)) continue;
    // a camp is looked at once a week (its own weekday): the rates below are per day x E
    if (span < 7 && (day_ + (int)(camps_[i].site % 7)) % 7 != 0) continue;
    const float E = 7.0f;
    Camp& c = camps_[i];
    // the village it preys on: the nearest living one within 700 tiles (a frontier village counts as half as far)
    const SettlementState* T = nullptr;
    int64_t bd = 700ll * 700;
    for (const SettlementState& s : settlements_) {
      if (s.type != (uint8_t)SiteType::Village || (s.flags & (SS_ABANDONED | SS_RUINED))) continue;
      int64_t d = dist2(s.gx, s.gy, c.gx, c.gy);
      if (s.flags & SS_FRONTIER) d /= 4;
      if (d < bd || (d == bd && T && s.site < T->site)) { bd = d; T = &s; }
    }
    if (!T) continue;
    const Gid tsite = T->site, towner = T->owner;
    KingdomState* K = towner ? kingdomMut(towner) : nullptr;
    if (towner && (!K || K->fallen || K->tier != Tier::Active)) continue;   // frozen with a far realm (VISION_PLAN 4.4)
    bool war = false;
    if (K) for (const War& w : wars_) if (!w.endDay && (w.attacker == towner || w.defender == towner)) { war = true; break; }
    const bool weak = K && (K->military < 2.5f || K->stability < 0.45f || K->famine || war);
    const bool strong = K && !war && K->stability > 0.65f && K->military > 4.0f;
    const bool hungry = (T->flags & (SS_FAMINE | SS_UNREST)) != 0 || (K && K->famine);
    // a camp's men come and go each week: hard times and a weak or warring lord bring more of them (deserters, the
    // hungry, the dispossessed); a strong, settled realm drains them away. Most camps stay small all their lives.
    float up = RT.campUp, down = RT.campDown;
    if (!K) { up += 0.04f; down -= 0.02f; }             // the wildlands: nobody's soldiers, nobody's law
    if (weak) { up += 0.08f; down -= 0.05f; }
    if (hungry) { up += 0.05f; down -= 0.03f; }
    if (strong) { up -= 0.05f; down += 0.08f; }
    (void)E;
    if (chance(std::max(0.0f, up)) && c.strength < 100) c.strength++;
    if (chance(std::max(0.0f, down)) && c.strength > 5) c.strength--;
    // the land's soldiers sweep the hills
    if (K && c.strength >= 30 && chance(RT.campSweep * E * std::clamp(K->military / 4.0f, 0.3f, 1.5f) * (war ? 0.3f : 1.0f))) {
      c.strength = (uint8_t)std::max(5, c.strength - 35);
      c.flags &= (uint8_t)~CF_WARBAND;
      c.grewDay = dayU16(day_);
    }
    if (!(c.flags & CF_WARBAND) && c.strength >= 55 && day_ - (int)c.notedDay >= 30) { c.flags |= CF_WARBAND; c.grewDay = dayU16(day_); }
    if (!(c.flags & CF_WARBAND)) continue;
    // the warband raids its village (the foreshadowing of a warlord: the news names the camp's prey)
    if (chance(RT.warbandRaid * E)) {
      SettlementState& s = state(tsite);
      s.damage = (uint8_t)std::min(100, s.damage + 6 + (int)(roll() % 5));
      s.prosperity = (uint8_t)std::max(0, s.prosperity - 8);
      s.mood = (uint8_t)std::max(0, s.mood - 10);
      s.food = (uint8_t)std::max(0, s.food - 8);
      s.popPct = (uint8_t)std::max(0, s.popPct - 2);
      s.changedDay = dayU16(day_);
      addEvent(EvType::WarbandRaid, 0, towner, tsite, s.gx, s.gy, day_).mag = c.strength;
      Camp& cc = camps_[i];
      cc.strength = (uint8_t)std::min(100, cc.strength + 2);
      if (K && K->military > 1.5f && chance(0.4f)) cc.strength = (uint8_t)std::max(5, cc.strength - 12);   // the riders give chase
    }
    Camp& cc = camps_[i];
    if (cc.strength < 80 || day_ - (int)cc.grewDay < 40 || day_ - (int)cc.notedDay < 30 || day_ < 30) continue;
    if (K && K->player) continue;   // (the player's fief is the LORD lane's to lose)
    const bool soft = !K || K->military < 3.5f || K->stability < 0.5f || war || K->famine;
    if (!soft) continue;
    // one warlord at a time in a land: none alive within 1500 tiles
    bool near = false;
    for (const KingdomState& k : kingdoms_)
      if (k.origin == KO_WARLORD && !k.fallen && dist2(capX(k), capY(k), cc.gx, cc.gy) < 1500ll * 1500) { near = true; break; }
    if (near || !chance(RT.warlordRise * E)) continue;
    raiseWarlord(cc, tsite, day_);
  }
}

// a warlord's host lives off its neighbours: tribute, or a reason for war
void Realm::tickWarlords(int span) {
  for (size_t i = 0; i < kingdoms_.size(); i++) {
    const KingdomState& W0 = kingdoms_[i];
    if (W0.origin != KO_WARLORD || W0.fallen || W0.tier != Tier::Active || W0.player) continue;
    if (span < 7 && (day_ + (int)(W0.id % 7)) % 7 != 0) continue;
    if (!chance(RT.warlordDemand * 7.0f)) continue;
    const Gid wid = W0.id;
    const int32_t wx = capX(W0), wy = capY(W0);
    // the nearest neighbour it can frighten
    Gid pick = 0;
    int64_t bd = 1500ll * 1500;
    for (const KingdomState& n : kingdoms_) {
      if (n.id == wid || n.fallen || n.tier != Tier::Active || n.player || n.settlements.empty() || apart(n, W0)) continue;
      if (n.military > 1.5f * W0.military || atWar(n.id, wid)) continue;
      const Relation* r = relation(n.id, wid);
      if (!r || r->state == Rel::Unknown || r->state == Rel::Alliance || day_ < (int)r->truceUntil) continue;
      const int64_t d = dist2(capX(n), capY(n), wx, wy);
      if (d < bd) { bd = d; pick = n.id; }
    }
    if (!pick) continue;
    KingdomState* N = kingdomMut(pick);
    KingdomState* W = kingdomMut(wid);
    Relation* r = relMut(pick, wid, true);
    if (!N || !W || !r) continue;
    float p = std::clamp(0.25f + 0.35f * (W->military / std::max(0.5f, N->military) - 0.6f), 0.1f, 0.8f);
    if (N->ruler.traits & RT_HONOURABLE) p -= 0.15f;
    if (N->ruler.traits & RT_AGGRESSIVE) p -= 0.2f;
    if (N->wealth > 0.3f && chance(p)) {
      const float amt = std::clamp(N->wealth * 0.08f, 0.2f, 1.0f);
      N->wealth -= amt;
      W->wealth += amt;
      r->opinion = (int8_t)std::max(-100, r->opinion - 5);
      if (r->aggrieved == wid && r->tension > Tension::Calm && r->tension < Tension::Skirmishes) setTension(*r, (Tension)((int)r->tension - 1), day_);
      addEvent(EvType::TributePaid, pick, wid, N->capital, capX(*N), capY(*N), day_).mag = (int16_t)std::lround(amt * 100);
    } else {
      r->opinion = (int8_t)std::max(-100, r->opinion - 10);
      if (r->tension < Tension::Skirmishes && (r->aggrieved == wid || r->tension == Tension::Calm)) {
        r->aggrieved = wid;
        r->cause = WarCause::Raiders;
        setTension(*r, (Tension)((int)r->tension + 1), day_);
      }
      addEvent(EvType::TributeRefused, pick, wid, N->capital, capX(*N), capY(*N), day_);
    }
  }
}

// ---------------------------------------------------------------- lairs
void Realm::tickLairs(int span) {
  (void)span;
  for (size_t i = 0; i < lairs_.size(); i++) {
    if (lairs_[i].slain || day_ <= (int)lairs_[i].nextRaid) continue;   // (its own day belongs to the FOES lane's live raid)
    const Gid t = lairTarget(lairs_[i]);
    const SettlementState* T = settlement(t);
    const KingdomState* K = T && T->owner ? kingdom(T->owner) : nullptr;
    Lair& L = lairs_[i];
    if (!T || (T->owner && (!K || K->tier != Tier::Active))) { L.nextRaid = dayU16(day_ + 30); continue; }   // nobody in reach
    if (!L.wokeDay) {
      // one great beast abroad at a time in a land: another awake within 2500 tiles sends this one back to sleep a while
      bool other = false;
      for (const Lair& o : lairs_)
        if (o.site != L.site && !o.slain && o.wokeDay && dist2(o.gx, o.gy, L.gx, L.gy) < 2500ll * 2500) { other = true; break; }
      if (other) { L.nextRaid = dayU16(day_ + 90 + (int)(roll() % 300)); continue; }
      L.wokeDay = dayU16(std::max(1, day_));
      L.nextRaid = dayU16(day_ + 8 + (int)(roll() % 18));
      addEvent(EvType::BossWoke, T->owner, 0, L.site, L.gx, L.gy, day_).mag = L.mon;
      continue;
    }
    lairRaid(L.site, t, 10 + (int)(roll() % 16), day_);
  }
}

// ---------------------------------------------------------------- successors and claims
void Realm::tickClaims(int span) {
  if (span < 7 && day_ % 7 != 3) return;   // once a week
  const float E = 7.0f;
  // a fallen realm's remnant crowns a successor in one of its old towns others hold
  std::vector<Gid> fallen;
  for (const KingdomState& k : kingdoms_) {
    if (!k.fallen || k.rebel) continue;   // (only a planned realm has old towns of its own: SettlementState::home)
    const int since = day_ - (int)k.lastTick;
    if (since < 20 || since > 2 * YEAR) continue;
    fallen.push_back(k.id);
  }
  for (Gid f : fallen) {
    bool claimed = false;
    for (const KingdomState& k : kingdoms_) if (!k.fallen && k.claim == f) { claimed = true; break; }
    if (claimed) continue;
    const SettlementState* best = nullptr;
    int bestRank = -1;
    bool seethe = false;
    for (const SettlementState& s : settlements_) {
      if (s.home != f || !s.owner || s.owner == f || (s.flags & (SS_ABANDONED | SS_RUINED | SS_BESIEGED))) continue;
      const KingdomState* O = kingdom(s.owner);
      if (!O || O->fallen || O->player || O->tier != Tier::Active) continue;
      if (s.flags & SS_UNREST) seethe = true;
      const int rank = (2 - std::min<int>(2, s.type)) * 100 + ((s.flags & SS_UNREST) ? 60 : 0) + (100 - s.mood) / 2;
      if (rank > bestRank || (rank == bestRank && best && s.site < best->site)) { bestRank = rank; best = &s; }
    }
    if (!best || !chance(RT.successor * E * (seethe ? 2.0f : 1.0f))) continue;
    crownSuccessor(f, best->site, day_);
  }
  // claims fulfilled: the claimant holds every living town of the fallen realm it claims
  for (size_t i = 0; i < kingdoms_.size(); i++) {
    const KingdomState& R = kingdoms_[i];
    if (R.fallen || !R.claim || R.player) continue;
    const KingdomState* C = kingdom(R.claim);
    if (C && !C->fallen) continue;
    int all = 0, held = 0;
    for (const SettlementState& s : settlements_) {
      if (s.home != R.claim || (s.flags & (SS_ABANDONED | SS_RUINED))) continue;
      all++;
      if (s.owner == R.id) held++;
    }
    if (all < 2 || held < all) continue;
    KingdomState& K = kingdoms_[i];
    const Gid claim = K.claim;
    K.claim = 0;
    K.stability = std::min(1.0f, K.stability + 0.2f);
    addEvent(EvType::Reunified, K.id, claim, K.capital, capX(K), capY(K), day_).mag = (int16_t)all;
  }
}

// ---------------------------------------------------------------- alliances and the call to arms
void Realm::tickAlliances(int span) {
  if (span < 7 && day_ % 7 != 5) return;   // once a week
  const float E = 7.0f;
  // who feels threatened: at war, on the road to war's upper rungs, or a warlord at the door
  std::vector<uint8_t> threat(kingdoms_.size(), 0);
  auto mark = [&](Gid g) { auto it = kingdomIx_.find(g); if (it != kingdomIx_.end()) threat[(size_t)it->second] = 1; };
  for (const War& w : wars_) if (!w.endDay) { mark(w.attacker); mark(w.defender); }
  for (const Relation& r : relations_) {
    if (r.tension < Tension::BorderTension) continue;
    if (r.aggrieved == r.a) mark(r.b); else if (r.aggrieved == r.b) mark(r.a); else { mark(r.a); mark(r.b); }
  }
  for (size_t i = 0; i < relations_.size(); i++) {
    Relation& r = relations_[i];
    const auto ia = kingdomIx_.find(r.a), ib = kingdomIx_.find(r.b);
    if (ia == kingdomIx_.end() || ib == kingdomIx_.end()) continue;
    const KingdomState& A = kingdoms_[(size_t)ia->second];
    const KingdomState& B = kingdoms_[(size_t)ib->second];
    if (A.fallen || B.fallen || A.tier != Tier::Active || B.tier != Tier::Active || A.player || B.player || apart(A, B)) continue;
    if (r.state == Rel::Alliance) {
      if (r.opinion < 10 || r.tension >= Tension::BorderTension) { r.state = r.opinion < -50 ? Rel::Rivalry : Rel::Peace; r.stepDay = dayU16(day_); }
      continue;
    }
    if (r.state != Rel::Peace && r.state != Rel::Trade && r.state != Rel::Contact) continue;
    if (r.tension != Tension::Calm || r.opinion < 30 || A.overlord == B.id || B.overlord == A.id) continue;
    const bool menaced = threat[(size_t)ia->second] || threat[(size_t)ib->second];
    const float warmth = std::clamp((r.opinion - 20) / 40.0f, 0.0f, 1.5f);
    if (!chance(RT.ally * E * (menaced ? 3.0f : 1.0f) * warmth)) continue;
    r.state = Rel::Alliance;
    r.sinceDay = dayU16(day_);
    r.stepDay = dayU16(day_);
    const Gid a = A.id, b = B.id;
    const int32_t x = capX(A), y = capY(A);
    const Gid cap = A.capital;
    addEvent(EvType::AllianceMade, a, b, cap, x, y, day_);
  }
}

void Realm::callAllies(uint32_t warId, int day) {
  const War* w = nullptr;
  for (const War& x : wars_) if (x.id == warId) w = &x;
  if (!w || w->endDay) return;
  const Gid aid = w->attacker, did = w->defender;
  const WarCause cause = w->cause;
  const KingdomState* A = kingdom(aid);
  const KingdomState* D = kingdom(did);
  if (!A || !D || A->fallen || D->fallen) return;
  // a war already part of a wider one calls nobody (no chains of calls: one war, its friends, no world war)
  for (const War& x : wars_)
    if (!x.endDay && x.id != warId && (x.attacker == aid || x.defender == aid || x.attacker == did || x.defender == did)) return;
  std::vector<Gid> called;
  for (size_t i = 0; i < kingdoms_.size() && called.size() < 2; i++) {
    const KingdomState& X = kingdoms_[i];
    if (X.id == aid || X.id == did || X.fallen || X.player || X.tier != Tier::Active || apart(X, *A) || apart(X, *D)) continue;
    const Relation* xd = relation(X.id, did);
    const bool bound = (xd && xd->state == Rel::Alliance) || D->overlord == X.id || X.overlord == did;
    if (!bound || atWar(X.id, aid)) continue;
    if (A->overlord == X.id || X.overlord == aid) continue;   // sworn to both sides: it sits the war out
    const Relation* xa = relation(X.id, aid);
    if (xa && (xa->state == Rel::Alliance || day < (int)xa->truceUntil)) continue;
    called.push_back(X.id);
  }
  for (Gid x : called) {
    Relation* r = relMut(x, aid, true);
    const KingdomState* X = kingdom(x);
    A = kingdom(aid);
    if (!r || !X || !A) continue;
    if (r->state == Rel::Unknown) r->state = Rel::Contact;
    r->aggrieved = x;
    r->cause = cause;
    r->opinion = (int8_t)std::min<int>(r->opinion, -40);
    setTension(*r, Tension::Skirmishes, day);
    r->marchDay = dayU16(day + 4 + (int)(roll() % 8));
    r->lastIncident = dayU16(day);
    addEvent(EvType::AllyJoined, x, did, X->capital, capX(*X), capY(*X), day).mag = (int16_t)(warId & 0x7FFF);
    const Gid at = borderSite(*A, *X);
    const SettlementState* s = settlement(at);
    addEvent(EvType::TroopsMarching, x, aid, at, s ? s->gx : capX(*X), s ? s->gy : capY(*X), day);
  }
}

// ---------------------------------------------------------------- vassals and tribute
void Realm::tickVassals(int span) {
  for (size_t i = 0; i < kingdoms_.size(); i++) {
    if (!kingdoms_[i].overlord) continue;
    KingdomState& V = kingdoms_[i];
    const Gid vid = V.id, oid = V.overlord;
    KingdomState* O = kingdomMut(oid);
    if (V.fallen || !O || O->fallen) {
      V.overlord = 0; V.tribute = 0;
      if (Relation* r = relMut(vid, oid, false)) if (r->state == Rel::Vassal) r->state = Rel::Peace;
      continue;
    }
    if (V.player || O->player || V.tier != Tier::Active || O->tier != Tier::Active) continue;
    Relation* r = relMut(vid, oid, true);
    if (!r) continue;
    const int since = day_ - (int)r->sinceDay;
    if (since < YEAR || since % YEAR >= span) continue;   // the bond's day, once a year
    bool war = false;
    for (const War& w : wars_) if (!w.endDay && (w.attacker == oid || w.defender == oid)) { war = true; break; }
    float p = 0.12f + 0.6f * std::clamp(V.military / std::max(0.5f, O->military) - 0.6f, 0.0f, 1.0f) + (r->opinion < -40 ? 0.15f : 0.0f) +
              (war ? 0.15f : 0.0f) - ((V.ruler.traits & RT_HONOURABLE) ? 0.1f : 0.0f);
    if (V.wealth < 0.1f) p += 0.2f;
    if (chance(p)) {
      // refused: the bond is broken and the overlord means to have it back (the road to war's first rung)
      V.overlord = 0;
      V.tribute = 0;
      r->state = r->opinion < -50 ? Rel::Rivalry : Rel::Peace;
      r->opinion = (int8_t)std::max(-100, r->opinion - 15);
      r->aggrieved = oid;
      r->cause = WarCause::Revenge;
      r->truceUntil = 0;
      setTension(*r, Tension::Strained, day_);
      addEvent(EvType::TributeRefused, vid, oid, V.capital, capX(V), capY(V), day_);
    } else {
      const float amt = std::min(V.tribute, std::max(0.0f, V.wealth));
      V.wealth -= amt;
      O->wealth += amt;
      addEvent(EvType::TributePaid, vid, oid, O->capital, capX(*O), capY(*O), day_).mag = (int16_t)std::clamp((int)std::lround(amt * 100), 0, 32767);
    }
  }
}

// ---------------------------------------------------------------- bounds
void Realm::pruneRealms() {
  // the fallen realms the generator never planned (rebels, warlords, successors): the newest 12 are kept (the
  // chronicle, the news, the ruins' records); older ones go unless a living realm still claims or descends from them
  std::vector<std::pair<int, Gid>> old;
  for (const KingdomState& k : kingdoms_) if (k.fallen && k.rebel && !k.player) old.push_back({(int)k.lastTick, k.id});
  if (old.size() <= 12) return;
  std::sort(old.begin(), old.end());
  std::vector<Gid> drop;
  for (size_t i = 0; i + 12 < old.size(); i++) {
    bool needed = false;
    for (const KingdomState& k : kingdoms_) if (!k.fallen && (k.claim == old[i].second || k.parent == old[i].second)) { needed = true; break; }
    if (!needed) drop.push_back(old[i].second);
  }
  if (drop.empty()) return;
  kingdoms_.erase(std::remove_if(kingdoms_.begin(), kingdoms_.end(),
                                 [&](const KingdomState& k) { return std::find(drop.begin(), drop.end(), k.id) != drop.end(); }),
                  kingdoms_.end());
  rebuildIndex();
  stats.instantiated = (int)kingdoms_.size();
}

}  // namespace realm
