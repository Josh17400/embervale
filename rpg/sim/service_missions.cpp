// M12 "Rise and Fall": the courts' diplomacy missions (rpg/sim/service_ops.h; VISION_PLAN 4.7 "a diplomacy quest
// ('carry the terms') forces a peace roll", 15.31 first contact). SERVICE lane. A ruler (king, jarl, a court's envoy)
// asks the player to carry its word when the world gives a reason:
//   CarryTerms     a war of theirs has worn them out (exhaustion): the terms to the enemy court (Realm::peaceRoll)
//   SeekAlliance   a common threat: a court at war with the same enemy, or hostile to it (Realm::allianceRoll)
//   DemandTribute  a much weaker neighbour (Realm::tributeRoll)
//   FirstEnvoy     a hidden kingdom the player has set foot in (sea::LF_LANDED) and this court has never met: its
//                  greeting carried across the Mist (Realm::openContact)
// On arrival the receiving ruler hears it in dialogue (service_game.cpp K_DELIVER) and the realm rolls with the
// player's weight as the bonus (missionBonus: rank and fame). A mission not carried by its day fails.
#include <algorithm>
#include <cmath>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/service_ops.h"
#include "rpg/sim/talk.h"

namespace {
constexpr int K_MISSION = 20;   // (service_game.cpp's action numbering: DLG_SERVICE + K_MISSION + MissionKind)
int kingdomIndex(const Game& g, ew::Gid k) {
  const auto& ks = g.realm.kingdoms();
  for (size_t i = 0; i < ks.size(); i++) if (ks[i].id == k) return (int)i;
  return -1;
}
float capDist(const Game& g, const realm::KingdomState& a, const realm::KingdomState& b) {
  const realm::SettlementState* x = g.realm.settlement(a.capital);
  const realm::SettlementState* y = g.realm.settlement(b.capital);
  if (!x || !y) return 1e9f;
  return std::hypot((float)(x->gx - y->gx), (float)(x->gy - y->gy));
}
}  // namespace

float ServiceOps::missionBonus(const Game& g) {
  const int rank = g.service.enlisted() ? (int)g.service.rank : 0;
  return std::clamp(0.12f * (float)rank + (float)g.realm.renown().fame / 1500.0f, 0.0f, 1.0f);
}

void ServiceOps::missionOffers(Game& g, Actor& ruler, ew::Gid court) {
  (void)ruler;
  const realm::KingdomState* C = g.realm.kingdom(court);
  if (!C || C->fallen || C->player || g.realm.rep(court) < 0) return;
  for (const service::Mission& m : g.service.missions) if (m.state == service::OS_OPEN && m.from == court) return;   // (one at a time per court)
  Dialogue& d = g.dlg;
  const auto& ks = g.realm.kingdoms();
  std::string pitch;
  const size_t opts0 = d.opts.size();
  auto offers = [&]() { return d.opts.size() - opts0; };
  // ---- terms: a war that has worn them out
  for (const realm::War& w : g.realm.wars()) {
    if (w.endDay || (w.attacker != court && w.defender != court)) continue;
    const ew::Gid foe = w.attacker == court ? w.defender : w.attacker;
    const realm::KingdomState* F = g.realm.kingdom(foe);
    if (!F || F->fallen) continue;
    const float tired = std::max(C->exhaustion, F->exhaustion);
    if (tired < 0.4f && g.day - (int)w.startDay < 90 && !w.forced) continue;
    const int fi = kingdomIndex(g, foe);
    if (fi < 0) continue;
    d.opts.push_back({"I'LL CARRY YOUR TERMS TO " + F->name, DLG_SERVICE + K_MISSION + (int)service::MissionKind::CarryTerms, fi});
    pitch = "THIS WAR WITH " + F->name + " HAS BLED US BOTH WHITE. SOMEONE MUST CARRY OUR TERMS TO THEIR COURT.";
    break;
  }
  // ---- an alliance against a common threat
  if (offers() < 2) {
    for (const realm::War& w : g.realm.wars()) {
      if (w.endDay || (w.attacker != court && w.defender != court)) continue;
      const ew::Gid enemy = w.attacker == court ? w.defender : w.attacker;
      const realm::KingdomState* best = nullptr;
      float bd = 1e9f;
      for (const realm::KingdomState& X : ks) {
        if (X.id == court || X.id == enemy || X.fallen || X.player || realm::Realm::apart(*C, X) || g.realm.atWar(X.id, court)) continue;
        const realm::Relation* rc = g.realm.relation(court, X.id);
        if (rc && rc->state == realm::Rel::Alliance) continue;
        const realm::Relation* re = g.realm.relation(enemy, X.id);
        const bool threat = g.realm.atWar(X.id, enemy) || (re && re->opinion < -30);
        if (!threat) continue;
        const float dd = capDist(g, *C, X);
        if (dd < bd) { bd = dd; best = &X; }
      }
      if (best) {
        const int xi = kingdomIndex(g, best->id);
        d.opts.push_back({"I'LL SEEK AN ALLIANCE WITH " + best->name, DLG_SERVICE + K_MISSION + (int)service::MissionKind::SeekAlliance, xi});
        if (pitch.empty()) pitch = best->name + " HATES " + kingdomName(g, enemy) + " AS MUCH AS WE DO. AN ALLIANCE COULD END THIS. WILL YOU CARRY OUR OFFER?";
        break;
      }
    }
  }
  // ---- tribute from a much weaker neighbour
  if (offers() < 2 && !C->overlord) {
    const realm::KingdomState* best = nullptr;
    float bd = 1e9f;
    for (const realm::KingdomState& X : ks) {
      if (X.id == court || X.fallen || X.player || X.overlord || realm::Realm::apart(*C, X) || g.realm.atWar(X.id, court)) continue;
      if (!g.realm.relation(court, X.id) || C->military < 2.0f * std::max(0.5f, X.military)) continue;
      const float dd = capDist(g, *C, X);
      if (dd < bd && dd < 900) { bd = dd; best = &X; }
    }
    if (best) {
      d.opts.push_back({"I'LL DEMAND TRIBUTE OF " + best->name, DLG_SERVICE + K_MISSION + (int)service::MissionKind::DemandTribute, kingdomIndex(g, best->id)});
      if (pitch.empty()) pitch = best->name + " IS WEAK, AND WE ARE NOT. LET THEM PAY FOR OUR PEACE, OR LEARN WHY THEY SHOULD.";
    }
  }
  // ---- a first envoy across the Mist (the player has set foot in a hidden land this court has never met)
  if (offers() < 3 && !C->hiddenLand) {
    for (const realm::KingdomState& H : ks) {
      if (!H.hiddenLand || H.fallen || H.contactDay < 0 || H.isolated) continue;
      const realm::Relation* r = g.realm.relation(court, H.id);
      if (r && r->state != realm::Rel::Unknown) continue;
      bool landed = false;
      for (const sea::LandRecord& L : g.sea.lands) landed |= L.land == H.hiddenLand && (L.flags & sea::LF_LANDED);
      if (!landed) continue;
      d.opts.push_back({"I'LL CARRY YOUR GREETING TO " + H.name, DLG_SERVICE + K_MISSION + (int)service::MissionKind::FirstEnvoy, kingdomIndex(g, H.id)});
      if (pitch.empty()) pitch = "THEY SAY YOU HAVE WALKED BEYOND THE MIST, AND MET THE COURT OF " + H.name + ". CARRY OUR GREETING TO THEM.";
      break;
    }
  }
  if (!pitch.empty()) speech::propose(d, speech::P_QUEST, speech::Topic::QuestOffer, pitch);
}

bool ServiceOps::missionChoose(Game& g, int kind, int arg) {
  Dialogue& d = g.dlg;
  const int ai = g.findActor(g.dlg.actor);
  const ew::Gid court = ai >= 0 ? kingdomOf(g, g.actors[(size_t)ai]) : 0;
  const auto& ks = g.realm.kingdoms();
  d.opts.clear();
  if (kind <= 0 || kind >= (int)service::MissionKind::COUNT || arg < 0 || arg >= (int)ks.size() || !court) {
    d.text = "ANOTHER TIME, THEN.";
    d.opts.push_back({"FAREWELL", 0, 0});
    return true;
  }
  const ew::Gid to = ks[(size_t)arg].id;
  const uint32_t id = giveMission(g, (service::MissionKind)kind, court, to);
  if (!id) d.text = "ON SECOND THOUGHT, NO. THE MOMENT HAS PASSED.";
  else {
    const service::Mission* m = g.service.mission(id);
    d.text = "THEN GO WITH OUR SEAL. THE COURT OF " + kingdomName(g, to) + " MUST HEAR IT BEFORE DAY " + std::to_string(m ? m->dueDay : g.day + 30) + ".";
    g.trackedQuest = g.quests.empty() ? g.trackedQuest : g.quests.back().id;
  }
  d.opts.push_back({"FAREWELL", 0, 0});
  return true;
}

void ServiceOps::missionsDaily(Game& g) {
  for (service::Mission& m : g.service.missions) {
    if (m.state != service::OS_OPEN) continue;
    bool moot = false;
    if (m.kind == service::MissionKind::CarryTerms && !g.realm.atWar(m.from, m.to)) moot = true;
    const realm::KingdomState* T = g.realm.kingdom(m.to);
    const realm::KingdomState* F = g.realm.kingdom(m.from);
    if (!T || T->fallen || !F || F->fallen) moot = true;
    if (!moot && g.day <= (int)m.dueDay) continue;
    m.state = service::OS_FAILED;
    m.outcome = 0;
    const std::string line = moot ? "THE MISSION TO " + kingdomName(g, m.to) + " IS OVERTAKEN BY EVENTS." : "THE WORD FROM " + kingdomName(g, m.from) + " NEVER REACHED " + kingdomName(g, m.to) + ". THE MISSION FAILED.";
    for (Quest& q : g.quests)
      if (q.type == QType::Envoy && q.stage == (int)m.id && q.state != QState::Done) {
        q.state = QState::Done;
        q.flags |= QF_FAILED;
        q.desc = line;
        g.emit(Ev::QuestUpdate, g.pl().p, q.id, 1, "QUEST FAILED: " + q.title);
      }
    if (!moot) g.realm.addRep(m.from, -3);
    rise(g, RiseEv::MissionDone, 0, line, m.from);
    g.say(line);
  }
}
