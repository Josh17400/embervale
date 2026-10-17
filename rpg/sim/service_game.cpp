// M12 "Rise and Fall": the service in the game (rpg/sim/service_ops.h). SERVICE lane: swearing to a kingdom's army as a
// Recruit at a captain (a town's captain of the watch, a siege camp's captain; a village guard where no captain is near,
// the others point the way), the oath and the pay in the kingdom's own voice, merit from the enemy slain, sieges won and
// orders done, ranks with a weekly wage, the sworn kingdom's men greeting the player by rank, an honourable discharge or
// desertion (barred, hunted for a while), the army disbanded when its realm falls, orders and missions mirrored as
// quests, diplomacy missions rolled by the realm at the receiving court. The orders' play is rpg/sim/service_orders.cpp,
// the courts' missions rpg/sim/service_missions.cpp.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "engine/audio.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/service_ops.h"
#include "rpg/sim/talk.h"
#include "rpg/world/source.h"

namespace {
using service::Rank;
constexpr int K_ENLIST = 1, K_REPORT = 2, K_LEAVE = 3, K_DESERT = 4, K_DELIVER = 5, K_STAY = 6, K_OATH = 7, K_PRESS = 8,
              K_MISSION = 20;   // K_MISSION.. : the courts' missions (service_missions.cpp)
constexpr int HONOUR_DAYS = 60;      // days served before a Recruit or Sergeant may leave with honour
constexpr int HUNT_DAYS = 20;        // a deserter's old comrades stay hostile this long

uint16_t dayU16(int day) { return (uint16_t)std::clamp(day, 0, 65535); }

// the army's words in each culture's voice (cult::Archetype order). %K the kingdom, %W the week's pay, %R the rank
struct Voice { const char* pitch; const char* oath; const char* welcome; const char* greet; };
const Voice VOICES[(int)cult::Archetype::COUNT] = {
    {"%K NEEDS STRONG ARMS FOR THE SHIELD WALL. SWEAR TO THE RAVEN BANNER AND YOU EAT AT OUR FIRE: %W GOLD EVERY SEVEN DAYS.",
     "THEN SAY IT AFTER ME: MY AXE TO %K, MY SHIELD TO MY BROTHERS, MY BLOOD BEFORE I BREAK.", "SKAL, RECRUIT! YOUR SHIELD STANDS IN OUR WALL NOW.",
     "HAIL, %R! THE RAVENS OF %K FLY WITH YOU."},
    {"THE CLANS OF %K ARE CALLING THE SPEARS IN. TAKE THE PLAID AND THE OATH, AND THE CHIEF PAYS %W GOLD EACH WEEK.",
     "HAND ON THE STONE: YOUR SPEAR TO %K'S CLANS, YOUR WORD UNBROKEN, YOUR BACK NEVER TURNED.", "WELCOME TO THE CLAN, RECRUIT. THE GLENS ARE YOURS TO KEEP NOW.",
     "WELL MET, %R. THE GLENS STAND FIRM WHILE YOU DO."},
    {"THE KING'S ARMY OF %K TAKES RECRUITS. %W GOLD A WEEK, A TABARD, BREAD AND A BUNK. ALL IT ASKS IS YOUR OATH.",
     "RAISE YOUR HAND. SWEAR TO SERVE %K AND ITS CROWN, TO KEEP THE KING'S PEACE AND TO FOLLOW YOUR CAPTAINS.",
     "WELCOME TO THE KING'S MEN, RECRUIT. YOUR PAY COMES EVERY SEVEN DAYS. DON'T MAKE ME REGRET IT.", "GOOD DAY, %R. ALL QUIET ON YOUR ROUND?"},
    {"THE LEGIONS OF %K ARE ENROLLING. THE PAY IS %W GOLD EVERY SEVEN DAYS, THE DISCIPLINE IS IRON, THE GLORY ETERNAL.",
     "REPEAT THE SACRAMENTUM: I SWEAR TO OBEY, TO MARCH, TO HOLD THE LINE FOR %K UNTO DEATH.", "YOU ARE ENROLLED. YOUR NAME IS ON THE ROLL OF THE LEGION, RECRUIT.",
     "AVE, %R. THE LEGION SALUTES YOU."},
    {"THE RIDERS OF %K WANT BLADES WHO KNOW THE SANDS. SWEAR TO THE BANNER AND DRAW %W GOLD EACH WEEK, AND WATER WITHOUT ASKING.",
     "SWEAR BY THE WELLS AND THE STARS: YOUR BLADE FOR %K, YOUR WATER SHARED, YOUR WORD AS FIXED AS THE NORTH STAR.",
     "THE SANDS ACCEPT YOU, RECRUIT. RIDE WELL.", "PEACE UPON YOU, %R. THE WELLS OF %K ARE SAFE WHILE YOU RIDE."},
    {"THE KHAN OF %K COUNTS NEW RIDERS. THE HORDE PAYS %W GOLD EVERY SEVEN DAYS, AND A SHARE OF ALL WE TAKE.",
     "DRINK FROM THE CUP AND SWEAR: YOUR BOW TO THE KHAN, YOUR HORSE TO THE HORDE OF %K, YOUR LIFE TO THE SKY.",
     "HAI! A NEW RIDER FOR THE HORDE. DON'T FALL OFF, RECRUIT.", "HAI, %R! THE HORSE TAIL OF %K FLIES FOR YOU."},
    {"THE WARDENS OF %K NEED THOSE WHO DON'T FEAR THE BOG. %W GOLD A WEEK, AND DRY BOOTS IF YOU'RE LUCKY.",
     "SWEAR ON THE OLD WATERS: YOUR STEEL TO %K'S CAUSEWAYS, YOUR EYES TO THE MIST, YOUR TONGUE TO SILENCE.",
     "THE MIST KNOWS YOUR NAME NOW, RECRUIT. STAY ON THE BOARDS.", "QUIET NOW, %R. %K'S WARDENS SEE YOU."},
    {"THE MANDATE OF %K SEEKS WORTHY SOLDIERS. THE BANNER PAYS %W GOLD EACH WEEK TO THOSE WHO SERVE WITH HONOUR.",
     "BOW AND SPEAK THE VOW: LOYALTY TO THE MANDATE OF %K, OBEDIENCE TO YOUR OFFICERS, HONOUR ABOVE LIFE.",
     "THE MANDATE RECEIVES YOU, RECRUIT. SERVE IN HARMONY.", "HONOURED %R, THE BANNER OF %K GREETS YOU."},
    {"THE RIVER GUARD OF %K TAKES ON HANDS. %W GOLD EVERY SEVEN DAYS, PAID AT THE FORD, NO TOLL FOR OUR OWN.",
     "SWEAR BY THE RIVER: YOUR SWORD TO %K'S BANKS, YOUR WORD AS SURE AS THE CURRENT.", "WELCOME ABOARD, RECRUIT. THE RIVER CARRIES US ALL.",
     "FAIR WATER, %R. THE BANKS OF %K ARE HELD."},
    {"THE SUN'S SPEARS OF %K ARE CALLED TO ARMS. SERVE IN THE LIGHT FOR %W GOLD EACH WEEK AND THE TEMPLE'S BLESSING.",
     "FACE THE SUN AND SWEAR: YOUR SPEAR TO %K, YOUR HEART TO THE LIGHT, YOUR SHADOW NEVER TURNED FROM YOUR BROTHERS.",
     "THE SUN SEES YOU, RECRUIT. WALK IN ITS LIGHT.", "THE SUN ON YOU, %R. %K'S SPEARS STAND BESIDE YOU."},
    {"THE WARDENS OF %K TAKE FEW, BUT THE GROVES NEED KEEPERS. %W GOLD A WEEK, AND THE TREES' FAVOUR.",
     "LAY YOUR HAND ON THE BARK AND SWEAR: YOUR BOW TO %K'S GROVES, YOUR STEP SOFT, YOUR WORD AS OLD AS THE ROOTS.",
     "THE GROVES WELCOME YOU, RECRUIT. TREAD SOFTLY.", "THE LEAVES WHISPER YOUR NAME, %R. %K'S WARDENS WALK WITH YOU."},
    {"THE SENTINELS OF %K ENLIST THOSE THE STARS FAVOUR. THE SPIRE PAYS %W GOLD EVERY SEVEN DAYS.",
     "SPEAK THE VOW UNDER THE STARS: YOUR BLADE TO %K'S SPIRES, YOUR WATCH UNBROKEN, YOUR FATE BOUND TO OURS.",
     "THE STARS HAVE WRITTEN YOU IN, RECRUIT.", "STARLIGHT ON YOU, %R. THE SPIRES OF %K STAND."},
};

std::string fill(const char* t, const std::string& K, int wage, Rank r) {
  std::string out;
  for (const char* p = t; *p; p++) {
    if (p[0] == '%' && p[1] == 'K') { out += K; p++; }
    else if (p[0] == '%' && p[1] == 'W') { out += std::to_string(wage); p++; }
    else if (p[0] == '%' && p[1] == 'R') { out += service::rankName(r); p++; }
    else out += *p;
  }
  return out;
}
const Voice& voiceOf(const Game& g, ew::Gid k) { return VOICES[std::clamp(ServiceOps::archetypeOf(g, k), 0, (int)cult::Archetype::COUNT - 1)]; }
int32_t plGx(const Game& g) { return g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE); }
int32_t plGy(const Game& g) { return g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE); }

// a captain who takes recruits within 60 tiles of this guard (his name and where), so the guard points the way
const Actor* captainNear(const Game& g, const Actor& guard, ew::Gid k) {
  const Actor* best = nullptr;
  float bd = (60.0f * TILE) * (60.0f * TILE);
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& c = g.actors[i];
    if (!c.npc || c.role != Role::Captain || c.st == AState::Dead || c.realm != k || c.hostile) continue;
    const float d = len2(c.p - guard.p);
    if (d < bd) { bd = d; best = &c; }
  }
  return best;
}
}  // namespace

// ---------------------------------------------------------------- shared helpers
int ServiceOps::archetypeOf(const Game& g, ew::Gid k) {
  auto it = g.world.kingdomById.find(k);
  if (it != g.world.kingdomById.end())
    if (const cult::Culture* C = g.world.cultureOfKingdom(it->second)) return (int)C->archetype;
  // a realm the generator never planned (rebels, warlords, successors) speaks its parent's tongue
  if (const realm::KingdomState* K = g.realm.kingdom(k)) {
    if (K->culture && g.world.src) return (int)g.world.src->culture(K->culture).archetype;
    if (K->parent && K->parent != k) {
      auto jt = g.world.kingdomById.find(K->parent);
      if (jt != g.world.kingdomById.end())
        if (const cult::Culture* C = g.world.cultureOfKingdom(jt->second)) return (int)C->archetype;
    }
  }
  return (int)cult::Archetype::Heartland;
}

std::string ServiceOps::kingdomName(const Game& g, ew::Gid k) {
  if (const realm::KingdomState* K = g.realm.kingdom(k)) return K->name;
  if (g.world.src) if (const ew::KingdomPlan* kp = g.world.src->kingdom(k)) return kp->name;
  return "THE REALM";
}

// the kingdom a person serves or lives under: soldiers, captains and heralds carry Actor::realm; townsfolk and guards
// answer to their settlement's current owner
ew::Gid ServiceOps::kingdomOf(const Game& g, const Actor& a) {
  if (a.realm) return a.realm;
  int site = a.site;
  if (site < 0 && a.bldg >= 0 && a.bldg < (int)g.world.over.bldgs.size()) site = g.world.over.bldgs[(size_t)a.bldg].site;
  if (site < 0 || site >= (int)g.world.sites.size()) return 0;
  const int k = g.world.sites[(size_t)site].kingdom;
  return k >= 0 && k < (int)g.world.kingdoms.size() ? g.world.kingdoms[(size_t)k].id : 0;
}

service::Order* ServiceOps::openOrder(Game& g) {
  for (auto it = g.service.orders.rbegin(); it != g.service.orders.rend(); ++it) if (it->state == service::OS_OPEN) return &*it;
  return nullptr;
}
service::Mission* ServiceOps::openMission(Game& g) {
  for (auto it = g.service.missions.rbegin(); it != g.service.missions.rend(); ++it) if (it->state == service::OS_OPEN) return &*it;
  return nullptr;
}

void ServiceOps::rise(Game& g, RiseEv k, float f, const std::string& s, ew::Gid kingdom) {
  g.events.push_back({Ev::Rise, g.pl().p, (int)k, f, s});
  g.events.back().id = (uint32_t)kingdom;
}

void ServiceOps::sync(Game& g) {
  g.realm.renown().sworn = g.service.enlisted() ? g.service.sworn : 0;
  g.realm.renown().rank = (uint8_t)(g.service.enlisted() ? g.service.rank : Rank::None);
}

void ServiceOps::reset(Game& g) { g.service = service::Service(); }
void ServiceOps::loaded(Game& g) {
  g.service.rt = service::Service::Runtime();
  sync(g);
}

// ---------------------------------------------------------------- the core
bool ServiceOps::enlist(Game& g, ew::Gid kingdom, std::string& why) {
  if (!kingdom) { why = "THERE IS NO ARMY HERE TO JOIN."; return false; }
  if (g.service.enlisted()) { why = g.service.sworn == kingdom ? "YOU ALREADY SERVE." : "YOU ARE SWORN TO ANOTHER BANNER. ONE OATH AT A TIME."; return false; }
  if (g.service.deserterOf(kingdom)) { why = "WE DON'T TAKE BACK DESERTERS. YOUR NAME IS STRUCK FROM OUR ROLLS."; return false; }
  if (g.realm.rep(kingdom) < -10) { why = "NOT YOU. NOT AFTER WHAT YOU'VE DONE TO US."; return false; }
  if (const ew::Gid mine = g.realm.playerRealm()) {
    if (mine == kingdom) { why = "YOU RULE HERE. A LORD DOESN'T TAKE A RECRUIT'S PAY."; return false; }
    if (g.realm.atWar(mine, kingdom)) { why = "YOUR OWN BANNER IS AT WAR WITH US."; return false; }
    // the player's realm's enemies: a realm at war with the player's own will not take its ruler's oath either
  }
  const realm::KingdomState* K = g.realm.kingdom(kingdom);
  if (K && K->fallen) { why = "THERE IS NO ARMY LEFT TO JOIN. THE REALM IS GONE."; return false; }
  service::Service& S = g.service;
  S.sworn = kingdom;
  S.rank = Rank::Recruit;
  S.merit = 0;
  S.enlistDay = S.paidDay = S.rankDay = dayU16(g.day);
  S.lastOrderDay = dayU16(std::max(0, g.day - 2));   // the first order comes within a day or so of the oath when at war
  sync(g);
  g.realm.forceEvent(realm::EvType::Enlisted, kingdom, 0, K ? K->capital : 0, plGx(g), plGy(g), g.day);
  g.realm.addRep(kingdom, 5);
  const std::string line = "YOU SWEAR TO THE ARMY OF " + kingdomName(g, kingdom) + ". RANK: RECRUIT. PAY: " + std::to_string(service::wageOf(Rank::Recruit)) + " GOLD A WEEK.";
  g.say(line);
  rise(g, RiseEv::Enlisted, (float)Rank::Recruit, line, kingdom);
  g.sfx((int)Sfx::QuestStart, g.pl().p);
  return true;
}

bool ServiceOps::promote(Game& g) {
  service::Service& S = g.service;
  if (!S.enlisted() || S.rank >= Rank::Marshal) return false;
  S.rank = (Rank)((int)S.rank + 1);
  S.merit = 0;
  S.rankDay = dayU16(g.day);
  sync(g);
  const realm::KingdomState* K = g.realm.kingdom(S.sworn);
  g.realm.forceEvent(realm::EvType::Promoted, S.sworn, 0, K ? K->capital : 0, plGx(g), plGy(g), g.day);
  g.realm.renown().fame += 10 * (int)S.rank;
  static const char* more[] = {"", "", " TWO OF OUR MEN MARCH WITH YOU NOW.", " YOU LEAD A COMPANY NOW, AND THE CAPTAINS' TABLE HAS A SEAT FOR YOU.",
                               " THE WHOLE ARMY IS YOURS TO COMMAND. CHOOSE WHERE WE STRIKE."};
  const std::string line = std::string("YOU RISE TO ") + service::rankName(S.rank) + " IN THE ARMY OF " + kingdomName(g, S.sworn) + ". PAY: " +
                           std::to_string(service::wageOf(S.rank)) + " GOLD A WEEK." + more[std::clamp((int)S.rank, 0, 4)];
  g.say(line);
  rise(g, RiseEv::Promoted, (float)S.rank, line, S.sworn);
  g.sfx((int)Sfx::LevelUp, g.pl().p);
  return true;
}

void ServiceOps::addMerit(Game& g, int merit) {
  service::Service& S = g.service;
  if (!S.enlisted() || merit <= 0) return;
  S.merit += merit;
  // (a big deed can carry a soldier over more than one rank's need: the rest counts toward the next)
  for (int guard = 0; guard < 4; guard++) {
    const int need = service::meritToRise(S.rank);
    if (need <= 0 || S.merit < need) break;
    const int rest = S.merit - need;
    if (!promote(g)) break;
    S.merit = rest;
  }
}

void ServiceOps::leave(Game& g, bool desert) {
  service::Service& S = g.service;
  if (!S.enlisted()) return;
  const bool honourable = !desert && (S.rank >= Rank::Captain || g.day - (int)S.enlistDay >= HONOUR_DAYS);
  service::Past p;
  p.kingdom = S.sworn; p.rank = S.rank; p.day = dayU16(g.day);
  p.how = honourable ? service::PAST_DISCHARGED : service::PAST_DESERTED;
  S.past.push_back(p);
  if (S.past.size() > 16) S.past.erase(S.past.begin());
  if (!honourable) {
    g.realm.addRep(S.sworn, -25);
    S.huntedBy = S.sworn;
    S.huntedUntil = dayU16(g.day + HUNT_DAYS);
  } else {
    g.realm.addRep(S.sworn, 5);
    g.realm.renown().fame += 5 * (int)S.rank;
  }
  const std::string K = kingdomName(g, S.sworn);
  const std::string line = honourable ? "YOU LEAVE THE ARMY OF " + K + " WITH HONOUR, A " + service::rankName(S.rank) + " TO THE LAST."
                                      : "YOU BREAK YOUR OATH TO " + K + ". THEIR SOLDIERS WILL HUNT A DESERTER FOR A WHILE.";
  rise(g, honourable ? RiseEv::Discharged : RiseEv::Deserted, (float)S.rank, line, S.sworn);
  sendSquadHome(g);
  for (service::Order& o : S.orders) if (o.state == service::OS_OPEN) o.state = service::OS_FAILED;
  for (Quest& q : g.quests)
    if (q.type == QType::Campaign && q.state != QState::Done) { q.state = QState::Done; q.flags |= QF_FAILED; }
  S.sworn = 0;
  S.rank = Rank::None;
  S.merit = 0;
  sync(g);
  g.say(line);
}

uint32_t ServiceOps::giveOrder(Game& g, service::OrderKind k, ew::Gid site, int32_t gx, int32_t gy, uint32_t war, uint32_t siege) {
  service::Service& S = g.service;
  if (!S.enlisted()) return 0;
  service::Order o;
  o.id = S.nextId++;
  o.kind = k; o.site = site; o.gx = gx; o.gy = gy; o.war = war; o.siege = siege;
  o.givenDay = dayU16(g.day);
  o.dueDay = dayU16(g.day + (k == service::OrderKind::Scout ? 8 : k == service::OrderKind::Hold ? 6 : 10));
  o.merit = (uint8_t)(k == service::OrderKind::Assassinate || k == service::OrderKind::Assault ? 20 : 12);
  if (S.rank >= Rank::Captain) { o.flags |= service::OF_LEAD; o.merit = (uint8_t)(o.merit + 8); }
  o.squad = (uint8_t)service::squadOf(S.rank);
  // the enemy: the siege's other side, else the sworn kingdom's foe in that war, else whoever holds the place
  for (const realm::Siege& sg : g.realm.sieges())
    if (siege && sg.id == siege) o.foe = sg.attacker == S.sworn ? sg.defender : sg.attacker;
  if (!o.foe && war)
    for (const realm::War& w : g.realm.wars())
      if (w.id == war) o.foe = w.attacker == S.sworn ? w.defender : w.attacker;
  if (!o.foe) if (const realm::SettlementState* st = g.realm.settlement(site)) if (st->owner != S.sworn) o.foe = st->owner;
  if (!o.foe)   // (a scripted order: the sworn kingdom's enemy in its first war, if it has one)
    for (const realm::War& w : g.realm.wars())
      if (!w.endDay && (w.attacker == S.sworn || w.defender == S.sworn)) { o.foe = w.attacker == S.sworn ? w.defender : w.attacker; break; }
  std::string where = "THE FRONT";
  const int si = g.world.siteHandle(site);
  if (si >= 0) where = g.world.sites[(size_t)si].name;
  Quest q;
  q.type = QType::Campaign;
  q.stage = (int)o.id;
  const bool lead = (o.flags & service::OF_LEAD) != 0;
  q.title = (lead && k == service::OrderKind::Assault ? std::string("LEAD THE ASSAULT ON") : lead && k == service::OrderKind::BreakSiege ? std::string("LEAD THE RELIEF OF")
                                                                                                                                     : std::string(service::orderName(k))) + " " + where;
  q.giverName = std::string("THE ARMY OF ") + kingdomName(g, S.sworn);
  q.giverId = S.sworn;
  q.targetId = site;
  q.hasPos = gx != 0 || gy != 0;
  q.tgx = gx; q.tgy = gy;
  q.gold = 40 + 30 * (int)S.rank;
  q.xp = 120 + 60 * (int)S.rank;
  q.need = k == service::OrderKind::BreakSiege ? (lead ? 10 : 8) : k == service::OrderKind::Assault ? (lead ? 8 : 6) : 1;
  q.subject = q.title;
  g.acceptQuest(q);
  o.quest = g.quests.empty() ? 0 : g.quests.back().id;
  S.orders.push_back(o);
  S.lastOrderDay = dayU16(g.day);
  while (S.orders.size() > 24) {
    auto it = std::find_if(S.orders.begin(), S.orders.end(), [](const service::Order& x) { return x.state != service::OS_OPEN; });
    if (it == S.orders.end()) break;
    S.orders.erase(it);
  }
  service::Order* mine = S.order(o.id);
  if (mine && !g.quests.empty()) {
    g.quests.back().subject = orderLine(g, *mine);
    g.quests.back().desc = g.quests.back().subject;
    g.trackedQuest = g.quests.back().id;   // the army's order leads the HUD and the compass
  }
  rise(g, RiseEv::Order, (float)k, "ORDERS: " + q.title, S.sworn);
  return o.id;
}

void ServiceOps::finishOrder(Game& g, uint32_t id, bool done) {
  service::Order* o = g.service.order(id);
  if (!o || o->state != service::OS_OPEN) return;
  o->state = done ? service::OS_DONE : service::OS_FAILED;
  const service::OrderKind kind = o->kind;
  for (Quest& q : g.quests) {
    if (q.type != QType::Campaign || q.stage != (int)id || q.state == QState::Done) continue;
    if (done) { q.state = QState::Complete; g.completeQuest(q); }
    else { q.state = QState::Done; q.flags |= QF_FAILED; g.emit(Ev::QuestUpdate, g.pl().p, q.id, 1, "QUEST FAILED: " + q.title); }
  }
  if (done) g.service.ordersDone++;   // (completeQuest -> questDone gave the merit)
  else {
    g.service.ordersFailed++;
    static const char* why[] = {"THE ORDER FAILED.", "THE SIEGE WAS DECIDED WITHOUT YOU.", "THE SIEGE ENDED BEFORE YOU STRUCK.", "THE SUPPLIES NEVER CAME.",
                                "NO REPORT CAME BACK IN TIME.", "THE CAPTAIN LIVES. THE CHANCE IS GONE.", "THE PEOPLE YOU LED ARE LOST.", "THE GARRISON STOOD WITHOUT YOU."};
    rise(g, RiseEv::OrderFailed, 0, why[std::clamp((int)kind, 0, 7)], g.service.sworn);
  }
  sendSquadHome(g);
}

uint32_t ServiceOps::giveMission(Game& g, service::MissionKind k, ew::Gid from, ew::Gid to) {
  service::Service& S = g.service;
  const realm::KingdomState* T = g.realm.kingdom(to);
  if (!T || from == to) return 0;
  service::Mission m;
  m.id = S.nextId++;
  m.kind = k; m.from = from; m.to = to; m.toSite = T->capital;
  if (const realm::SettlementState* st = g.realm.settlement(T->capital)) { m.gx = st->gx; m.gy = st->gy; }
  m.givenDay = dayU16(g.day); m.dueDay = dayU16(g.day + (k == service::MissionKind::FirstEnvoy ? 60 : 30));
  Quest q;
  q.type = QType::Envoy;
  q.stage = (int)m.id;
  q.title = std::string(service::missionName(k)) + " " + kingdomName(g, to);
  q.subject = "SPEAK WITH THE RULER OF " + kingdomName(g, to);
  q.desc = q.subject;
  q.giverName = std::string("THE COURT OF ") + kingdomName(g, from);
  q.giverId = from;
  q.targetId = m.toSite;
  q.hasPos = m.gx != 0 || m.gy != 0;
  q.tgx = m.gx; q.tgy = m.gy;
  q.gold = k == service::MissionKind::FirstEnvoy ? 300 : 150; q.xp = 250; q.need = 1;
  g.acceptQuest(q);
  m.quest = g.quests.empty() ? 0 : g.quests.back().id;
  S.missions.push_back(m);
  if (S.missions.size() > 16) {
    auto it = std::find_if(S.missions.begin(), S.missions.end(), [](const service::Mission& x) { return x.state != service::OS_OPEN; });
    S.missions.erase(it == S.missions.end() ? S.missions.begin() : it);
  }
  rise(g, RiseEv::Mission, (float)k, q.title, from);
  return m.id;
}

bool ServiceOps::arrive(Game& g, uint32_t id) {
  service::Mission* m = g.service.mission(id);
  if (!m || m->state != service::OS_OPEN) return false;
  const float bonus = missionBonus(g);
  bool ok = false;
  uint8_t outcome = 0;
  const service::MissionKind kind = m->kind;
  const ew::Gid from = m->from, to = m->to;
  switch (kind) {
    case service::MissionKind::CarryTerms: ok = g.realm.peaceRoll(from, to, bonus, g.day); outcome = ok ? 1 : 0; break;
    case service::MissionKind::SeekAlliance: ok = g.realm.allianceRoll(from, to, bonus, g.day); outcome = ok ? 2 : 0; break;
    case service::MissionKind::DemandTribute: ok = g.realm.tributeRoll(to, from, bonus, g.day); outcome = ok ? 3 : 0; break;
    case service::MissionKind::FirstEnvoy: {
      // the mainland court's greeting reaches the hidden kingdom: the two courts meet (Rel::Contact) and the hidden
      // court warms to the one who carried it
      ok = true; outcome = 1;
      if (const realm::KingdomState* T = g.realm.kingdom(to)) if (T->hiddenLand) g.realm.openContact(T->hiddenLand, from, g.day);
      g.realm.addRep(to, 10);
      g.realm.forceEvent(realm::EvType::TermsCarried, from, to, g.realm.kingdom(to) ? g.realm.kingdom(to)->capital : 0, m->gx, m->gy, g.day);
      break;
    }
    default: break;
  }
  m = g.service.mission(id);
  if (!m) return ok;
  m->state = service::OS_DONE;
  m->outcome = outcome;
  g.service.missionsDone++;
  g.realm.addRep(from, ok ? 6 : 2);
  g.realm.renown().fame += ok ? 15 : 3;
  for (Quest& q : g.quests)
    if (q.type == QType::Envoy && q.stage == (int)id && q.state == QState::Active) { q.state = QState::Complete; g.completeQuest(q); }
  const std::string A = kingdomName(g, from), B = kingdomName(g, to);
  std::string line;
  switch (kind) {
    case service::MissionKind::CarryTerms: line = ok ? "PEACE BETWEEN " + A + " AND " + B + ": THE TERMS ARE SIGNED." : B + " REFUSES THE TERMS OF " + A + "."; break;
    case service::MissionKind::SeekAlliance: line = ok ? A + " AND " + B + " SWEAR AN ALLIANCE." : B + " WILL NOT ALLY WITH " + A + ", NOT YET."; break;
    case service::MissionKind::DemandTribute: line = ok ? B + " BENDS THE KNEE AND PAYS TRIBUTE TO " + A + "." : B + " REFUSES TO PAY TRIBUTE TO " + A + "."; break;
    default: line = A + " AND " + B + " EXCHANGE THEIR FIRST WORDS ACROSS THE MIST."; break;
  }
  rise(g, RiseEv::MissionDone, (float)outcome, line, to);
  g.say(line);
  return ok;
}

// ---------------------------------------------------------------- hooks
void ServiceOps::step(Game& g, float dt) {
  service::Service& S = g.service;
  captainsStep(g, dt);
  const bool newDay = S.rt.lastDay != g.day;
  if (newDay) {
    S.rt.lastDay = g.day;
    missionsDaily(g);
  }
  if (!S.enlisted()) {
    // the order's leftovers still walk off (a squad sent home, an escort's people) when the player left the army
    ordersStep(g, dt);
    return;
  }
  // the realm the player served fell: the army is disbanded (the record is kept)
  if (const realm::KingdomState* K = g.realm.kingdom(S.sworn)) {
    if (K->fallen) {
      service::Past p;
      p.kingdom = S.sworn; p.rank = S.rank; p.how = service::PAST_DISBANDED; p.day = dayU16(g.day);
      S.past.push_back(p);
      if (S.past.size() > 16) S.past.erase(S.past.begin());
      const std::string line = "THE ARMY OF " + K->name + " IS DISBANDED: ITS REALM HAS FALLEN. YOU KEEP YOUR RANK OF " + service::rankName(S.rank) + " IN MEMORY.";
      rise(g, RiseEv::Discharged, (float)S.rank, line, S.sworn);
      sendSquadHome(g);
      for (service::Order& o : S.orders) if (o.state == service::OS_OPEN) o.state = service::OS_FAILED;
      for (Quest& q : g.quests)
        if (q.type == QType::Campaign && q.state != QState::Done) { q.state = QState::Done; q.flags |= QF_FAILED; }
      S.sworn = 0; S.rank = Rank::None; S.merit = 0;
      sync(g);
      g.say(line);
      return;
    }
  }
  // a ruler of a realm of their own cannot take another's pay: discharged with honour
  if (const ew::Gid mine = g.realm.playerRealm()) {
    if (mine != S.sworn && g.realm.kingdom(mine)) {
      g.say("A RULER CANNOT SERVE UNDER ANOTHER'S BANNER.");
      S.enlistDay = dayU16(g.day - HONOUR_DAYS);   // (the crown releases the oath: never a desertion)
      leave(g, false);
      return;
    }
  }
  // the weekly wage
  if (g.day - (int)S.paidDay >= 7) {
    const int weeks = (g.day - (int)S.paidDay) / 7;
    const int pay = service::wageOf(S.rank) * std::min(weeks, 8);
    S.paidDay = dayU16((int)S.paidDay + 7 * weeks);
    if (pay > 0) {
      g.giveGold(pay);
      const std::string line = "THE PAYMASTER OF " + kingdomName(g, S.sworn) + " COUNTS OUT YOUR PAY: " + std::to_string(pay) + " GOLD.";
      rise(g, RiseEv::Wage, (float)pay, line, S.sworn);
      g.say(line);
      g.sfx((int)Sfx::Coin, g.pl().p);
    }
  }
  if (newDay) ordersDaily(g);
  ordersStep(g, dt);
}

void ServiceOps::talk(Game& g, Actor& a) {
  Dialogue& d = g.dlg;
  const ew::Gid k = kingdomOf(g, a);
  const service::Service& S = g.service;
  // a court: its ruler hears the terms the player carries, and has missions of its own to give
  if (a.role == Role::King || a.role == Role::Jarl || a.role == Role::Envoy) {
    bool hears = false;
    for (const service::Mission& m : S.missions)
      if (m.state == service::OS_OPEN && m.to == k) {
        d.opts.push_back({"I CARRY WORD FROM " + kingdomName(g, m.from), DLG_SERVICE + K_DELIVER, (int)m.id});
        hears = true;
      }
    if (hears) speech::propose(d, speech::P_QUEST, speech::Topic::Service, "AN ENVOY? SPEAK, THEN. THE COURT OF " + kingdomName(g, k) + " IS LISTENING.");
    else if (k) missionOffers(g, a, k);
  }
  if (a.role != Role::Captain && a.role != Role::Guard && a.role != Role::Soldier) return;
  if (!k || a.hostile) return;
  const Voice& v = voiceOf(g, k);
  const std::string K = kingdomName(g, k);
  if (!S.enlisted()) {
    if (S.huntedBy == k && g.day < (int)S.huntedUntil) {
      speech::propose(d, speech::P_URGENT, speech::Topic::War, "YOU'VE GOT SOME NERVE SHOWING YOUR FACE HERE, DESERTER.");
      return;
    }
    if (a.role == Role::Captain) {
      if (!S.deserterOf(k) && g.realm.rep(k) >= -10) speech::propose(d, speech::P_STORY, speech::Topic::Service, fill(v.pitch, K, service::wageOf(Rank::Recruit), Rank::Recruit));
      d.opts.push_back({"I WANT TO ENLIST", DLG_SERVICE + K_ENLIST, 0});
    } else if (a.role == Role::Guard) {
      // a guard points the way to the captain who takes recruits; with none near he takes the oath himself
      if (const Actor* c = captainNear(g, a, k)) {
        const int dx = (int)std::floor(c->p.x / TILE) - (int)std::floor(a.p.x / TILE), dy = (int)std::floor(c->p.y / TILE) - (int)std::floor(a.p.y / TILE);
        if (hash32((uint32_t)a.id ^ (uint32_t)g.day * 131u) % 3 == 0)
          speech::propose(d, speech::P_SMALLTALK + 1, speech::Topic::Service,
                          "LOOKING TO SERVE " + K + "? " + c->name + " TAKES RECRUITS, " + dirWord(dx, dy, false) + " OF HERE.");
      } else {
        d.opts.push_back({"I WANT TO ENLIST", DLG_SERVICE + K_ENLIST, 0});
      }
    }
    return;
  }
  if (S.sworn != k) return;
  // the sworn kingdom's men greet the player by rank
  {
    // (M12 integrator: the dialogue review flagged one greeting from every guard) the culture's salute from about half
    // of them; the rest speak plainly, each in their own words (by who they are and the day)
    static const char* const PLAIN[] = {"STAND EASY, %R. NOTHING TO REPORT ON MY POST.", "THE WATCH CHANGES AT DUSK, %R, IF YOU'RE ASKING.",
                                        "A GOOD DAY TO YOU, %R. THE ROADS OF %K ARE QUIET.", "%R! I HEARD YOU WERE SWORN. GOOD TO HAVE YOU.",
                                        "EYES OPEN, %R. THERE'S TALK OF TROUBLE BEYOND THE BORDER.", "MORNING OR EVENING, %R, THE PAY IS THE SAME. KEEP WELL.",
                                        "YOU AGAIN, %R. I'D SALUTE, BUT MY ARM'S STILL SORE FROM DRILL."};
    const uint32_t hv = hash32((uint32_t)a.id * 2654435761u ^ (uint32_t)g.day * 977u);
    // (P1 integrator: half of a capital's guard on the salute still gave 4 the same line) the salute is one choice
    // among the plain ones, so a square full of guards spreads over eight lines
    const uint32_t np = (uint32_t)(sizeof(PLAIN) / sizeof(PLAIN[0])), pick = (hv >> 1) % (np + 1);
    const char* line = pick == np ? v.greet : PLAIN[pick];
    speech::propose(d, speech::P_NEWS, speech::Topic::Service, fill(line, K, 0, S.rank));
  }
  d.opts.push_back({"REPORTING FOR DUTY", DLG_SERVICE + K_REPORT, 0});
  if (a.role == Role::Captain) {
    if (S.rank >= Rank::Marshal) {
      bool open = false;
      for (const service::Order& o : S.orders) open |= o.state == service::OS_OPEN;
      if (!open) {
        const std::vector<uint32_t> ss = councilSieges(g);
        for (size_t i = 0; i < ss.size() && i < 3; i++)
          for (const realm::Siege& sg : g.realm.sieges())
            if (sg.id == ss[i]) {
              const int si = g.world.siteHandle(sg.site);
              const std::string nm = si >= 0 ? g.world.sites[(size_t)si].name : std::string("THE TOWN");
              d.opts.push_back({std::string(sg.attacker == S.sworn ? "PRESS THE SIEGE OF " : "RELIEVE ") + nm, DLG_SERVICE + K_PRESS, (int)sg.id});
            }
        if (!ss.empty()) speech::propose(d, speech::P_QUEST, speech::Topic::Service, "MARSHAL. THE CAPTAINS AWAIT YOUR WORD: WHERE DO WE STRIKE?");
      }
    }
    d.opts.push_back({"I WANT TO LEAVE THE ARMY", DLG_SERVICE + K_LEAVE, 0});
  }
}

bool ServiceOps::choose(Game& g, const DlgOpt& o) {
  const int act = o.action - DLG_SERVICE;
  Dialogue& d = g.dlg;
  const int ai = g.findActor(g.dlg.actor);
  const ew::Gid k = ai >= 0 ? kingdomOf(g, g.actors[(size_t)ai]) : 0;
  if (act >= K_MISSION) return missionChoose(g, act - K_MISSION, o.arg);
  switch (act) {
    case K_ENLIST: {
      // the oath first, in the kingdom's own words (refused at once when it would be refused anyway)
      std::string why;
      if (!k) why = "THERE IS NO ARMY HERE TO JOIN.";
      else if (g.service.enlisted()) why = g.service.sworn == k ? "YOU ALREADY SERVE." : "YOU ARE SWORN TO ANOTHER BANNER. ONE OATH AT A TIME.";
      else if (g.service.deserterOf(k)) why = "WE DON'T TAKE BACK DESERTERS. YOUR NAME IS STRUCK FROM OUR ROLLS.";
      else if (g.realm.rep(k) < -10) why = "NOT YOU. NOT AFTER WHAT YOU'VE DONE TO US.";
      else if (const ew::Gid mine = g.realm.playerRealm()) { if (mine == k || g.realm.atWar(mine, k)) why = mine == k ? "YOU RULE HERE. A LORD DOESN'T TAKE A RECRUIT'S PAY." : "YOUR OWN BANNER IS AT WAR WITH US."; }
      d.opts.clear();
      if (!why.empty()) { d.text = why; d.opts.push_back({"FAREWELL", 0, 0}); return true; }
      d.text = fill(voiceOf(g, k).oath, kingdomName(g, k), service::wageOf(Rank::Recruit), Rank::Recruit);
      d.opts.push_back({"I SWEAR IT", DLG_SERVICE + K_OATH, 0});
      d.opts.push_back({"NOT TODAY", 0, 0});
      return true;
    }
    case K_OATH: {
      std::string why;
      d.opts.clear();
      if (enlist(g, k, why)) d.text = fill(voiceOf(g, k).welcome, kingdomName(g, k), service::wageOf(Rank::Recruit), Rank::Recruit);
      else d.text = why;
      d.opts.push_back({"FAREWELL", 0, 0});
      return true;
    }
    case K_REPORT: {
      const service::Service& S = g.service;
      std::string t;
      const int need = service::meritToRise(S.rank);
      char line[160];
      const int days = std::max(0, g.day - (int)S.enlistDay);
      const std::string served = days == 0 ? std::string("FRESH TO THE BANNER") : days == 1 ? std::string("ONE DAY UNDER THE BANNER") : std::to_string(days) + " DAYS UNDER THE BANNER";
      if (need > 0) std::snprintf(line, sizeof line, "ON DUTY AS %s, %s. %d OF THE %d MERIT A %s NEEDS.", service::rankName(S.rank), served.c_str(), S.merit, need,
                                  service::rankName((Rank)((int)S.rank + 1)));
      else std::snprintf(line, sizeof line, "ON DUTY AS MARSHAL. THE ARMY IS YOURS TO LEAD.");
      t = line;
      if (const service::Order* oo = openOrder(g)) {
        t += " YOUR ORDERS: " + orderLine(g, *oo) + ", BY DAY " + std::to_string(oo->dueDay) + ".";
      } else {
        bool war = false;
        for (const realm::War& w : g.realm.wars()) war |= !w.endDay && (w.attacker == S.sworn || w.defender == S.sworn);
        t += war ? " NO ORDERS YET. THE NEXT WILL COME WITHIN A FEW DAYS." : " THE REALM IS AT PEACE. DRILL, AND WAIT FOR THE CALL.";
      }
      d.text = t;
      return true;
    }
    case K_LEAVE: {
      const service::Service& S = g.service;
      const int served = g.day - (int)S.enlistDay;
      if (S.rank < Rank::Captain && served < HONOUR_DAYS) {
        d.text = "YOU'VE SERVED " + std::to_string(std::max(0, served)) + " DAYS OF THE " + std::to_string(HONOUR_DAYS) +
                 " YOUR OATH ASKS. WALK AWAY NOW AND YOU'RE A DESERTER, AND WE DON'T FORGET DESERTERS.";
        d.opts.clear();
        d.opts.push_back({"THEN I BREAK MY OATH", DLG_SERVICE + K_DESERT, 0});
        d.opts.push_back({"I'LL STAY", DLG_SERVICE + K_STAY, 0});
        return true;
      }
      leave(g, false);
      d.text = "YOU'VE DONE YOUR PART. GO WITH OUR THANKS, AND COME BACK IF THE BANNER CALLS.";
      d.opts.clear();
      d.opts.push_back({"FAREWELL", 0, 0});
      return true;
    }
    case K_DESERT:
      leave(g, true);
      d.text = "THEN GO, AND DON'T COME BACK. IF MY MEN FIND YOU ON THE ROAD, THEY WON'T ASK QUESTIONS.";
      d.opts.clear();
      d.opts.push_back({"FAREWELL", 0, 0});
      return true;
    case K_STAY:
      d.text = "GOOD. BACK TO YOUR POST, SOLDIER.";
      d.opts.clear();
      d.opts.push_back({"FAREWELL", 0, 0});
      return true;
    case K_PRESS: {
      const uint32_t id = pressSiege(g, (uint32_t)o.arg);
      d.text = id ? "AS YOU COMMAND, MARSHAL. THE ARMY MARCHES AT DAWN." : "THAT FIGHT IS ALREADY DECIDED, MARSHAL.";
      d.opts.clear();
      d.opts.push_back({"FAREWELL", 0, 0});
      return true;
    }
    case K_DELIVER: {
      const service::Mission* m = g.service.mission((uint32_t)o.arg);
      const service::MissionKind mk = m ? m->kind : service::MissionKind::None;
      const bool ok = arrive(g, (uint32_t)o.arg);
      static const char* yes[] = {"", "ENOUGH BLOOD. WE ACCEPT THE TERMS. TELL YOUR MASTERS THE WAR IS OVER.", "AGAINST A COMMON FOE, TOGETHER. WE WILL STAND WITH YOU.",
                                  "WE CANNOT FIGHT YOU AND WIN. WE WILL PAY. TELL THEM THAT.", "A GREETING FROM BEYOND THE MIST! TELL YOUR KING WE WELCOME HIS WORD."};
      static const char* no[] = {"", "NO. TAKE THAT ANSWER BACK WITH YOU. WE FIGHT ON.", "AN ALLIANCE? NOT WITH THEM. NOT YET.", "TRIBUTE? COME AND TAKE IT, IF YOU CAN.", ""};
      const int ki = std::clamp((int)mk, 0, 4);
      d.text = ok ? yes[ki] : no[ki];
      d.opts.clear();
      d.opts.push_back({"FAREWELL", 0, 0});
      return true;
    }
    default: return false;
  }
}

void ServiceOps::kill(Game& g, const Actor& victim, int killer) {
  service::Service& S = g.service;
  if (!S.enlisted()) return;
  orderKill(g, victim, killer);
  if (killer != g.pl().id || !victim.npc || !victim.human || !victim.realm) return;
  if (victim.realm == S.sworn || !g.realm.atWar(victim.realm, S.sworn)) return;
  S.battles++;
  addMerit(g, victim.role == Role::Captain ? 8 : 1);
}

void ServiceOps::questDone(Game& g, const Quest& q) {
  if (q.type == QType::Campaign) {
    for (service::Order& o : g.service.orders)
      if ((int)o.id == q.stage && o.state == service::OS_OPEN) { o.state = service::OS_DONE; g.service.ordersDone++; }
    const service::Order* o = g.service.order((uint32_t)q.stage);
    const int merit = o ? o->merit : 10;
    rise(g, RiseEv::OrderDone, (float)merit, "ORDERS CARRIED OUT: " + q.title + ". +" + std::to_string(merit) + " MERIT.", g.service.sworn);
    g.realm.addRep(g.service.sworn, 3);
    g.say(g.turnInLine(q));   // (the army's thanks, by the order's kind: quests.cpp)
    addMerit(g, merit);
  }
  if (q.type == QType::Envoy) g.say(g.turnInLine(q));   // (the court's thanks, by what its word won)
}
