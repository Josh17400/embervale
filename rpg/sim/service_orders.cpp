// M12 "Rise and Fall": campaign orders played out in the world (rpg/sim/service_ops.h; VISION_PLAN 4.5, 4.7, 15.33).
// SERVICE lane. Every few days while the sworn kingdom is at war its army gives the player an order, generated from the
// realm's ACTIVE sieges and wars (never from a script alone):
//   BreakSiege   one of our towns is besieged: slay the besiegers and their captain at the camp (the war's camp people,
//                war_game.cpp; each kill is 0.5 to our side: realm::Realm::contribute)
//   Assault      our siege of their town: slay the defenders at the walls until the gate falls (the camp storms with you)
//   Supply       load supplies at a friendly place and carry them into a besieged town of ours (+5 % def for 3 days)
//   Scout        walk to the enemy's nearest town and report back alive
//   Assassinate  kill the enemy captain at their camp (a siege camp, or a field camp this file raises; -15 % atk: his
//                siege loses the equivalent to our side)
//   Escort       lead the people of a burned place of ours to a friendly town (they follow, and can die)
//   Hold         stand with the garrison of a frontier town through a night raid (two waves; dawn or the last raider)
// Each order fails properly: its siege decided without the player, the due day passed, the people lost, the walls left.
// The squad: from Sergeant, squadOf(rank) soldiers of the sworn realm march with the player on an order (follow,
// fight, can die, go home after). They are NOT companions (party::Party never sees them; Actor::comp 0).
// The captains of the watch: a kingdom's towns and cities keep one by the barracks; he takes recruits.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "engine/audio.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/service_ops.h"
#include "rpg/sim/war.h"
#include "rpg/world/source.h"

namespace {
using service::OrderKind;
using service::Rank;
constexpr int SLOT_CAPTAIN0 = 32000, SLOT_CAPTAINN = 2000;   // a town's captain of the watch (stable per site)
constexpr int SLOT_SQUAD0 = 34000;                           // squad men, the order's enemies, an escort's people (cycling)
constexpr int SLOT_SQUADN = 3000;

uint16_t dayU16(int day) { return (uint16_t)std::clamp(day, 0, 65535); }
int32_t plGx(const Game& g) { return g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE); }
int32_t plGy(const Game& g) { return g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE); }
float tilesTo(const Game& g, int32_t gx, int32_t gy) { return std::hypot((float)(gx - plGx(g)), (float)(gy - plGy(g))); }
Vec2 localPx(const Game& g, int32_t gx, int32_t gy) { return Vec2((gx - g.world.ox) * TILE + 8.0f, (gy - g.world.oy) * TILE + 10.0f); }
const realm::Siege* siegeById(const Game& g, uint32_t id) {
  if (!id) return nullptr;
  for (const realm::Siege& s : g.realm.sieges()) if (s.id == id) return &s;
  return nullptr;
}
Quest* questOf(Game& g, const service::Order& o) {
  for (Quest& q : g.quests) if (q.type == QType::Campaign && q.stage == (int)o.id) return &q;
  return nullptr;
}
bool atWarWith(const Game& g, ew::Gid a, ew::Gid b) { return a && b && g.realm.atWar(a, b); }
bool alive(const Game& g, int id) {
  const int i = ServiceOps::findActor(g, id);
  return i > 0 && g.actors[(size_t)i].st != AState::Dead;
}
// the sworn kingdom's settlement nearest a global tile (0 none); skip: one to leave out
ew::Gid nearestOwned(const Game& g, ew::Gid k, int32_t gx, int32_t gy, ew::Gid skip, bool notBesieged, float& dist) {
  const realm::KingdomState* K = g.realm.kingdom(k);
  ew::Gid best = 0;
  dist = 1e30f;
  if (!K) return 0;
  for (ew::Gid s : K->settlements) {
    if (s == skip) continue;
    const realm::SettlementState* st = g.realm.settlement(s);
    if (!st) continue;
    if (notBesieged && (st->flags & (realm::SS_BESIEGED | realm::SS_BURNED | realm::SS_ABANDONED | realm::SS_RUINED))) continue;
    const float d = std::hypot((float)(st->gx - gx), (float)(st->gy - gy));
    if (d < dist) { dist = d; best = s; }
  }
  return best;
}
}  // namespace

// ---------------------------------------------------------------- shared helpers
bool ServiceOps::siteHeart(const Game& g, ew::Gid site, int32_t& gx, int32_t& gy) {
  const int si = g.world.siteHandle(site);
  if (si >= 0) { gx = g.world.ox + g.world.sites[(size_t)si].ex; gy = g.world.oy + g.world.sites[(size_t)si].ey; return true; }
  if (const realm::SettlementState* st = g.realm.settlement(site)) { gx = st->gx; gy = st->gy; return true; }
  return false;
}

std::string ServiceOps::siteName(Game& g, ew::Gid site) {
  if (!site) return "";
  const int si = g.world.siteHandle(site);
  if (si >= 0) return g.world.sites[(size_t)si].name;
  for (const auto& n : g.service.rt.names) if (n.first == site) return n.second;
  std::string nm;
  int32_t gx, gy;
  if (g.world.src && siteHeart(g, site, gx, gy))
    for (const ew::SettlementNode& n : g.world.src->settlementsIn(gx - 1, gy - 1, gx + 2, gy + 2, true))
      if (n.id == site) nm = n.name;
  g.service.rt.names.push_back({site, nm});
  if (g.service.rt.names.size() > 64) g.service.rt.names.erase(g.service.rt.names.begin());
  return nm;
}

int ServiceOps::spawnMan(Game& g, Role r, ew::Gid kingdom, Vec2 at, int slot, int level) {
  Actor a;
  a.id = g.nextId_++;
  a.human = true; a.npc = true; a.role = r;
  a.p = at; a.home = at; a.goal = at;
  a.faction = r == Role::Refugee ? Faction::Town : Faction::Army;
  a.realm = r == Role::Refugee ? 0 : kingdom;
  a.site = -1; a.slot = slot; a.fromMap = false; a.bldg = -1;
  Rng rr(hash32((uint32_t)slot * 2654435761u ^ (uint32_t)kingdom ^ (uint32_t)(kingdom >> 32)) ^ (uint32_t)g.seed);
  g.makeLook(a, r, rr);
  a.maxHp = 140; a.dmg = 13; a.speed = 50; a.range = 16; a.radius = 4.5f; a.aggroR = 120;
  if (r == Role::Captain) { a.maxHp = 320; a.dmg = 20; a.speed = 52; a.radius = 5.0f; }
  if (r == Role::Refugee) { a.maxHp = 60; a.dmg = 0; a.speed = 42; }
  const int L = std::max(1, level);
  a.maxHp *= 1.0f + 0.10f * (L - 1);
  a.dmg *= 1.0f + 0.13f * (L - 1);
  a.armor = r == Role::Refugee ? 0 : L * 1.5f;
  a.hp = a.maxHp;
  a.level = 10;
  a.special = 0.5f;
  a.xp = r == Role::Captain ? 60 : (r == Role::Soldier ? 22 : 0);
  // (people come out of the crowd at slightly different paces: no lockstep)
  a.speed *= 0.94f + 0.12f * (float)(hash32((uint32_t)slot * 40503u) % 100) / 100.0f;
  g.actors.push_back(a);
  return a.id;
}

bool ServiceOps::isServiceCaptain(const Actor& a) { return a.npc && a.role == Role::Captain && a.slot >= SLOT_CAPTAIN0 && a.slot < SLOT_CAPTAIN0 + SLOT_CAPTAINN; }
bool ServiceOps::squadMember(const Game& g, int id) { return std::find(g.service.rt.squad.begin(), g.service.rt.squad.end(), id) != g.service.rt.squad.end(); }
int ServiceOps::squadAlive(const Game& g) {
  int n = 0;
  for (int id : g.service.rt.squad) n += alive(g, id) ? 1 : 0;
  return n;
}

void ServiceOps::sendSquadHome(Game& g) {
  service::Service& S = g.service;
  for (int id : S.rt.squad) if (alive(g, id)) S.rt.going.push_back({id, 24.0f});
  for (int id : S.rt.walkers) if (alive(g, id)) S.rt.going.push_back({id, 24.0f});
  S.rt.squad.clear();
  S.rt.walkers.clear();
  S.rt.squadOrder = 0;
  for (service::Order& o : S.orders) if (o.state != service::OS_OPEN) o.flags |= service::OF_HOME;
  if (!S.rt.going.empty()) {
    // they turn for home: a few steps on, then out of sight (never a vanishing in plain view)
    const Vec2 pp = g.pl().p;
    for (auto& gw : S.rt.going) {
      const int i = g.findActor(gw.first);
      if (i <= 0) continue;
      Actor& a = g.actors[(size_t)i];
      Vec2 away = a.p - pp;
      const float l = len(away);
      away = l > 1 ? away * (1.0f / l) : Vec2(0, 1);
      a.goal = a.p + away * (40.0f * TILE);
      a.special = 0.55f + 0.1f * (float)(a.id % 3);
      a.target = -1;
    }
  }
}

std::string ServiceOps::orderLine(const Game& gc, const service::Order& o) {
  Game& g = const_cast<Game&>(gc);   // (the name cache only)
  const std::string site = [&] { std::string n = siteName(g, o.site); return n.empty() ? std::string("THE FRONT") : n; }();
  const std::string from = [&] { std::string n = siteName(g, o.from); return n.empty() ? std::string("OUR LINES") : n; }();
  int have = 0, need = 1;
  for (const Quest& q : g.quests) if (q.type == QType::Campaign && q.stage == (int)o.id) { have = q.have; need = q.need; }
  char b[48];
  std::snprintf(b, sizeof b, " %d/%d", have, need);
  switch (o.kind) {
    case OrderKind::BreakSiege:
      return "SLAY THE BESIEGERS OF " + site + b + ((o.flags & service::OF_CAPDEAD) ? ", THEIR CAPTAIN DEAD" : ", AND THEIR CAPTAIN");
    case OrderKind::Assault: return "BREAK THE DEFENDERS OF " + site + b;
    case OrderKind::Supply: return (o.from && !(o.flags & service::OF_REACHED)) ? "LOAD THE SUPPLIES AT " + from : "CARRY THE SUPPLIES INTO " + site;
    case OrderKind::Scout: return (o.flags & service::OF_REACHED) ? "REPORT BACK AT " + from : "SCOUT THE ENEMY AT " + site;
    case OrderKind::Assassinate: return "KILL THE ENEMY CAPTAIN AT THE CAMP BY " + site;
    case OrderKind::Escort:
      if (!(o.flags & service::OF_REACHED)) return "FIND THE PEOPLE OF " + from;
      return "LEAD THEM TO " + site + " (" + std::to_string(o.aux) + " ALIVE)";
    case OrderKind::Hold:
      if (!(o.flags & service::OF_REACHED)) return "AT NIGHTFALL: THE WALLS OF " + site;
      return "HOLD " + site + " UNTIL DAWN (" + std::to_string(o.aux) + " RAIDERS SLAIN)";
    default: return site;
  }
}

// ---------------------------------------------------------------- the generator
std::vector<uint32_t> ServiceOps::councilSieges(const Game& g) {
  std::vector<std::pair<float, uint32_t>> v;
  const ew::Gid k = g.service.sworn;
  for (const realm::Siege& sg : g.realm.sieges()) {
    if (sg.over || (sg.attacker != k && sg.defender != k)) continue;
    int32_t gx, gy;
    if (!siteHeart(g, sg.site, gx, gy)) continue;
    v.push_back({tilesTo(g, gx, gy), sg.id});
  }
  std::sort(v.begin(), v.end());
  std::vector<uint32_t> out;
  for (const auto& p : v) out.push_back(p.second);
  return out;
}

uint32_t ServiceOps::pressSiege(Game& g, uint32_t siege) {
  const realm::Siege* sg = siegeById(g, siege);
  if (!sg || sg->over || !g.service.enlisted() || openOrder(g)) return 0;
  const bool att = sg->attacker == g.service.sworn;
  int32_t gx = sg->campX, gy = sg->campY;
  if (att || (!gx && !gy)) siteHeart(g, sg->site, gx, gy);
  return giveOrder(g, att ? OrderKind::Assault : OrderKind::BreakSiege, sg->site, gx, gy, sg->war, sg->id);
}

uint32_t ServiceOps::generateOrder(Game& g) {
  service::Service& S = g.service;
  if (!S.enlisted() || openOrder(g)) return 0;
  const ew::Gid k = S.sworn;
  const int32_t px = plGx(g), py = plGy(g);
  struct Cand { OrderKind kind; ew::Gid site, from, foe; int32_t gx, gy; uint32_t war, siege; float score; };
  std::vector<Cand> cs;
  OrderKind last = OrderKind::None;
  for (const service::Order& o : S.orders) last = o.kind;
  constexpr float FAR = 600.0f;
  // ---- the sieges
  for (const realm::Siege& sg : g.realm.sieges()) {
    if (sg.over || (sg.attacker != k && sg.defender != k)) continue;
    int32_t hx, hy;
    if (!siteHeart(g, sg.site, hx, hy)) continue;
    const float d = std::hypot((float)(hx - px), (float)(hy - py));
    if (d > FAR) continue;
    if (sg.defender == k) {
      const int32_t cx = (sg.campX || sg.campY) ? sg.campX : hx, cy = (sg.campX || sg.campY) ? sg.campY : hy;
      cs.push_back({OrderKind::BreakSiege, sg.site, 0, sg.attacker, cx, cy, sg.war, sg.id, 100.0f - d / 10.0f});
      float fd;
      const ew::Gid depot = nearestOwned(g, k, hx, hy, sg.site, true, fd);
      cs.push_back({OrderKind::Supply, sg.site, fd < 220 ? depot : 0, sg.attacker, hx, hy, sg.war, sg.id, 70.0f - d / 10.0f});
    } else {
      cs.push_back({OrderKind::Assault, sg.site, 0, sg.defender, hx, hy, sg.war, sg.id, 90.0f - d / 10.0f});
    }
  }
  // ---- the wars
  for (const realm::War& w : g.realm.wars()) {
    if (w.endDay || (w.attacker != k && w.defender != k)) continue;
    const ew::Gid foe = w.attacker == k ? w.defender : w.attacker;
    const realm::KingdomState* F = g.realm.kingdom(foe);
    if (!F || F->fallen) continue;
    // their settlement nearest the player, and ours nearest it (the front)
    float fd;
    const ew::Gid theirs = nearestOwned(g, foe, px, py, 0, false, fd);
    int32_t tx = 0, ty = 0;
    if (theirs && siteHeart(g, theirs, tx, ty) && fd < FAR) {
      float od;
      const ew::Gid ours = nearestOwned(g, k, tx, ty, 0, true, od);
      float hd;
      const ew::Gid home = nearestOwned(g, k, px, py, 0, true, hd);
      cs.push_back({OrderKind::Scout, theirs, home, foe, tx, ty, w.id, 0, 40.0f - fd / 20.0f});
      if (S.rank >= Rank::Sergeant) {
        // the enemy captain: at their siege camp before one of our towns, else at a field camp on their side of the front
        const realm::Siege* their = nullptr;
        for (const realm::Siege& sg : g.realm.sieges()) if (!sg.over && sg.attacker == foe && sg.defender == k && (sg.campX || sg.campY)) their = &sg;
        if (their) cs.push_back({OrderKind::Assassinate, their->site, 0, foe, their->campX, their->campY, w.id, their->id, 55.0f});
        else if (ours) {
          int32_t ox, oy;
          if (siteHeart(g, ours, ox, oy)) {
            const int32_t cx = ox + (int32_t)std::lround((tx - ox) * 0.62f), cy = oy + (int32_t)std::lround((ty - oy) * 0.62f);
            cs.push_back({OrderKind::Assassinate, theirs, 0, foe, cx, cy, w.id, 0, 45.0f - fd / 25.0f});
          }
        }
      }
      if (ours && od < 400) {
        int32_t ox, oy;
        if (siteHeart(g, ours, ox, oy)) cs.push_back({OrderKind::Hold, ours, 0, foe, ox, oy, w.id, 0, 44.0f - std::hypot((float)(ox - px), (float)(oy - py)) / 20.0f});
      }
    }
    // a burned place of ours: its people to a friendly town
    if (const realm::KingdomState* K = g.realm.kingdom(k))
      for (ew::Gid s : K->settlements) {
        const realm::SettlementState* st = g.realm.settlement(s);
        if (!st || !(st->flags & realm::SS_BURNED)) continue;
        float dd;
        const ew::Gid dest = nearestOwned(g, k, st->gx, st->gy, s, true, dd);
        const float d = std::hypot((float)(st->gx - px), (float)(st->gy - py));
        if (!dest || dd > 260 || d > FAR) continue;
        int32_t dx, dy;
        if (!siteHeart(g, dest, dx, dy)) continue;
        cs.push_back({OrderKind::Escort, dest, s, foe, dx, dy, w.id, 0, 52.0f - d / 20.0f});
      }
  }
  if (cs.empty()) return 0;
  Rng& r = rng(g);
  for (Cand& c : cs) {
    c.score += r.f() * 18.0f;
    if (c.kind == last) c.score -= 25.0f;
  }
  const Cand* best = &cs[0];
  for (const Cand& c : cs) if (c.score > best->score) best = &c;
  const Cand c = *best;
  const uint32_t id = giveOrder(g, c.kind, c.site, c.gx, c.gy, c.war, c.siege);
  if (service::Order* o = S.order(id)) {
    o->foe = c.foe;
    o->from = c.from;
    if (c.kind == OrderKind::Supply && !c.from) o->flags |= service::OF_REACHED;   // (no depot near: the carts come with the order)
    if (Quest* q = questOf(g, *o)) { q->subject = orderLine(g, *o); q->desc = q->subject; }
    // the captain's words for it
    static const char* say[] = {"", "OUR PEOPLE ARE STARVING BEHIND THE WALLS. BREAK THEIR CAMP.", "THE WALLS ARE WEAK. WE GO IN, AND YOU GO FIRST.",
                                "THE GARRISON NEEDS BREAD AND ARROWS. GET THE CARTS THROUGH.", "WE NEED EYES ON THE ENEMY. LOOK, COUNT, AND COME BACK ALIVE.",
                                "THEIR CAPTAIN IS THE HEAD OF THE SNAKE. CUT IT OFF.", "THE BURNED-OUT FAMILIES NEED AN ESCORT. KEEP THEM ALIVE.",
                                "THE FRONTIER EXPECTS A RAID AT NIGHT. STAND WITH THE GARRISON."};
    g.say(std::string("ORDERS FROM THE CAPTAINS: ") + say[std::clamp((int)c.kind, 0, 7)]);
  }
  return id;
}

// ---------------------------------------------------------------- the day
void ServiceOps::ordersDaily(Game& g) {
  service::Service& S = g.service;
  if (!S.enlisted()) return;
  // a siege won that the player fought in: merit (VISION_PLAN 4.7: "merit from ... sieges won")
  for (const realm::Siege& sg : g.realm.sieges()) {
    if (!sg.over || sg.resolveDay <= S.siegeDay || sg.resolveDay > g.day || !sg.playerJoined) continue;
    const bool ours = (sg.playerSide == 1 && sg.attacker == S.sworn && sg.attackerWon) || (sg.playerSide == 2 && sg.defender == S.sworn && !sg.attackerWon);
    if (!ours) continue;
    S.battles++;
    const std::string nm = siteName(g, sg.site);
    rise(g, RiseEv::OrderDone, 15, "THE SIEGE OF " + (nm.empty() ? std::string("THE TOWN") : nm) + " IS WON. +15 MERIT.", S.sworn);
    addMerit(g, 15);
    if (!S.enlisted()) return;
  }
  S.siegeDay = dayU16(g.day);
  // the supplies' boost runs out after 3 days
  for (service::Order& o : S.orders)
    if ((o.flags & service::OF_BOOST) && g.day >= (int)o.untilDay) {
      if (const realm::Siege* sg = siegeById(g, o.siege)) if (!sg->over) g.realm.contribute(o.siege, 2, -(float)o.aux / 100.0f);
      o.flags &= (uint8_t)~service::OF_BOOST;
    }
  // the open order: past its day, or its war over
  if (service::Order* o = openOrder(g)) {
    const uint32_t id = o->id;
    if (g.day > (int)o->dueDay) finishOrder(g, id, false);
    else if (o->foe && !atWarWith(g, S.sworn, o->foe) && o->kind != OrderKind::Supply && !o->siege) {
      g.say("PEACE WITH " + kingdomName(g, o->foe) + ": THE ORDER IS WITHDRAWN.");
      finishOrder(g, id, false);
    }
  }
  if (!S.enlisted() || openOrder(g)) return;
  // the next order: every few days at war; at once (a day on) when one of our towns is besieged
  bool urgent = false;
  for (const realm::Siege& sg : g.realm.sieges()) urgent |= !sg.over && (sg.defender == S.sworn || sg.attacker == S.sworn) && sg.startDay >= S.lastOrderDay;
  const int since = g.day - (int)S.lastOrderDay;
  const int gap = S.rank >= Rank::Marshal ? 4 : 3;   // (a Marshal chooses at the council first: the army waits a day longer)
  if (since >= gap || (urgent && since >= 1)) generateOrder(g);
}

// ---------------------------------------------------------------- the kills
void ServiceOps::orderKill(Game& g, const Actor& victim, int killer) {
  service::Service& S = g.service;
  service::Order* o = openOrder(g);
  if (!o) return;
  // an order's own enemies slain (by anyone: the garrison's kills count for the Hold)
  const bool ownFoe = std::find(S.rt.foes.begin(), S.rt.foes.end(), victim.id) != S.rt.foes.end();
  if (ownFoe && o->kind == OrderKind::Hold) o->aux = (uint16_t)std::min(9999, (int)o->aux + 1);
  // the field camp's captain (the first of its men; a bandit chief when no realm is named)
  if (ownFoe && o->kind == OrderKind::Assassinate && S.rt.foes.front() == victim.id && (killer == g.pl().id || squadMember(g, killer))) o->flags |= service::OF_CAPDEAD;
  const bool ours = killer == g.pl().id || squadMember(g, killer);
  if (!ours || !victim.npc || !victim.human || !victim.realm || victim.realm != o->foe) {
    if (Quest* q = questOf(g, *o)) { q->subject = orderLine(g, *o); q->desc = q->subject; }
    return;
  }
  Quest* q = questOf(g, *o);
  if (o->kind == OrderKind::BreakSiege || o->kind == OrderKind::Assault) {
    const realm::Siege* sg = siegeById(g, o->siege);
    if (sg && !sg->over) {
      const bool assault = o->kind == OrderKind::Assault;
      if (assault && victim.role != Role::Guard && victim.role != Role::Captain && victim.role != Role::Soldier) return;
      g.realm.contribute(sg->id, assault ? 1 : 2, 0.5f);
      g.realm.addRep(S.sworn, 1);
      g.realm.addRep(o->foe, -2);
      if (victim.role == Role::Captain) o->flags |= service::OF_CAPDEAD;
      if (q && (assault || victim.role != Role::Captain)) q->have = std::min(q->need, q->have + 1);
      if (q) {
        char line[48];
        std::snprintf(line, sizeof line, "%d/%d%s", q->have, q->need, (!assault && (o->flags & service::OF_CAPDEAD)) ? " + CAPTAIN" : "");
        g.emit(Ev::Text, victim.p + Vec2(0, -26), (int)rgba(255, 220, 120), 0, line);
      }
    }
  } else if (o->kind == OrderKind::Assassinate && victim.role == Role::Captain && tilesTo(g, o->gx, o->gy) < 40) {
    o->flags |= service::OF_CAPDEAD;
  }
  if (q) { q->subject = orderLine(g, *o); q->desc = q->subject; }
}

// ---------------------------------------------------------------- the play
void ServiceOps::ordersStep(Game& g, float dt) {
  service::Service& S = g.service;
  // men sent home walk off and are gone once out of sight
  for (size_t i = 0; i < S.rt.going.size();) {
    auto& gw = S.rt.going[i];
    gw.second -= dt;
    const int ai = g.findActor(gw.first);
    bool drop = ai <= 0;
    if (!drop) {
      Actor& a = g.actors[(size_t)ai];
      const float d = len(a.p - g.pl().p) / TILE;
      if (a.st == AState::Dead) drop = true;
      else if (d > 17.0f || (gw.second <= 0 && d > 11.0f) || gw.second < -40.0f) {
        if (!(g.mode == Mode::Dialogue && g.dlg.actor == a.id)) g.actors.erase(g.actors.begin() + ai);
        drop = true;
      } else a.target = -1;
    }
    if (drop) S.rt.going.erase(S.rt.going.begin() + (std::ptrdiff_t)i);
    else i++;
  }
  S.rt.t -= dt;
  if (S.rt.t > 0) return;
  const float step = 0.25f;
  S.rt.t = step;
  if (g.inside || !g.world.endless || g.world.over.ground.empty()) return;   // (the order waits outside)
  // the order's enemies who wandered far are put away
  S.rt.foes.erase(std::remove_if(S.rt.foes.begin(), S.rt.foes.end(), [&](int id) {
                    const int i = g.findActor(id);
                    if (i <= 0) return true;
                    if (g.actors[(size_t)i].st == AState::Dead) return true;
                    if (len(g.actors[(size_t)i].p - g.pl().p) > 75.0f * TILE) { g.actors.erase(g.actors.begin() + i); return true; }
                    return false;
                  }), S.rt.foes.end());
  service::Order* o = S.enlisted() ? openOrder(g) : nullptr;
  if (!o) {
    if (!S.rt.squad.empty() || !S.rt.walkers.empty()) sendSquadHome(g);
    return;
  }
  const uint32_t id = o->id;
  const int level = std::max(1, g.world.zoneLevel((int)std::floor(g.pl().p.x / TILE), (int)std::floor(g.pl().p.y / TILE)));
  auto nextSlot = [&]() { return SLOT_SQUAD0 + (S.rt.nextSlot++ % SLOT_SQUADN); };
  auto freeNear = [&](Vec2 c, float rTiles, float ang) {
    return g.freeSpot((int)std::floor((c.x + std::cos(ang) * rTiles * TILE) / TILE), (int)std::floor((c.y + std::sin(ang) * rTiles * TILE) / TILE));
  };
  // the order's enemy: the foe realm's soldiers, or (no realm to name: a scripted order in peace) bandits
  auto spawnFoe = [&](Role r, Vec2 at, int lv) {
    const ew::Gid fk = S.order(id) ? S.order(id)->foe : 0;
    if (fk) return spawnMan(g, r, fk, at, nextSlot(), lv);
    const int aid = spawnMan(g, Role::Bandit, 0, at, nextSlot(), lv);
    const int ai = g.findActor(aid);
    if (ai > 0) { Actor& a = g.actors[(size_t)ai]; a.faction = Faction::Bandit; a.hostile = true; a.realm = 0; }
    return aid;
  };
  auto refresh = [&]() {
    service::Order* oo = S.order(id);
    if (!oo) return;
    if (Quest* q = questOf(g, *oo)) {
      const std::string l = orderLine(g, *oo);
      if (q->subject != l) { q->subject = l; q->desc = l; }
    }
  };
  // ---- the squad (from Sergeant): it marches with the player while the order is open
  if (S.rt.squadOrder != id) { sendSquadHome(g); S.rt.squadOrder = id; o = S.order(id); if (!o) return; }
  {
    // the fallen never come back
    for (size_t i = 0; i < S.rt.squad.size();) {
      const int ai = g.findActor(S.rt.squad[i]);
      if (ai > 0 && g.actors[(size_t)ai].st == AState::Dead) {
        if (o->squad > 0) o->squad--;
        g.say(g.actors[(size_t)ai].name + " FALLS. " + (o->squad ? std::to_string(o->squad) + " OF YOUR SQUAD STILL STAND." : std::string("YOUR SQUAD IS GONE.")));
        S.rt.squad.erase(S.rt.squad.begin() + (std::ptrdiff_t)i);
      } else if (ai <= 0) S.rt.squad.erase(S.rt.squad.begin() + (std::ptrdiff_t)i);
      else i++;
    }
    const Actor& P = g.pl();
    Vec2 back = P.aim;
    if (len2(back) < 0.01f) back = Vec2(0, 1);
    back = norm(back) * -1.0f;
    const Vec2 side(-back.y, back.x);
    int n = 0;
    while ((int)S.rt.squad.size() < (int)o->squad && n++ < 8) {
      const int k = (int)S.rt.squad.size();
      const Vec2 at = g.freeSpot((int)std::floor((P.p.x + back.x * 40 + side.x * (k - 1) * 16) / TILE), (int)std::floor((P.p.y + back.y * 40 + side.y * (k - 1) * 16) / TILE));
      const int aid = spawnMan(g, Role::Soldier, S.sworn, at, nextSlot(), level);
      S.rt.squad.push_back(aid);
    }
    const int total = (int)S.rt.squad.size();
    for (int k = 0; k < total; k++) {
      const int ai = g.findActor(S.rt.squad[(size_t)k]);
      if (ai <= 0) continue;
      Actor& a = g.actors[(size_t)ai];
      const Actor& PP = g.pl();
      const float d = len(a.p - PP.p);
      // a fight that drags them far from the player is let go
      if (a.target >= 0) {
        const int ti = g.findActor(a.target);
        if (ti < 0 || len(g.actors[(size_t)ti].p - PP.p) > 14.0f * TILE) a.target = -1;
        else continue;
      }
      // the formation: a loose wedge behind the player, each man his own spot and pace
      const float off = (float)k - (total - 1) * 0.5f;
      const float depth = 30.0f + 6.0f * (float)(k % 2) + 3.0f * std::sin(g.time * 0.7f + (float)k * 1.9f);
      const Vec2 slot = PP.p + back * depth + side * (off * 17.0f);
      a.goal = slot;
      // a walk at his place in the line, a run when he falls behind (soldiers outpace a walking hero: 50 x 2 > 74)
      a.special = std::clamp(len(slot - a.p) / 45.0f, 0.45f, 2.0f);
      // left far behind (a river crossed, a cliff, a ride): he catches up out of sight, never in view (the phone shows
      // about 40 x 24 tiles round the hero)
      const Vec2 rel = (a.p - PP.p) * (1.0f / TILE);
      const bool unseen = std::fabs(rel.x) > 22.0f || std::fabs(rel.y) > 14.0f;
      if (d > 22.0f * TILE && unseen) {
        const float t = std::min(std::fabs(back.x) > 0.01f ? 22.0f / std::fabs(back.x) : 1e9f, std::fabs(back.y) > 0.01f ? 14.0f / std::fabs(back.y) : 1e9f) + 1.5f;
        const Vec2 at = g.freeSpot((int)std::floor((PP.p.x + back.x * t * TILE + side.x * off * 20) / TILE), (int)std::floor((PP.p.y + back.y * t * TILE + side.y * off * 20) / TILE));
        a.p = at; a.navNext = -1; a.vel = Vec2(0, 0);
      }
    }
  }
  // ---- the kind's play
  // an order about a siege given without its id (a script, an old save): it takes the siege at its place when one runs
  if (!o->siege && (o->kind == OrderKind::BreakSiege || o->kind == OrderKind::Assault || o->kind == OrderKind::Supply))
    if (const realm::Siege* at = g.realm.siegeAt(o->site)) {
      o->siege = at->id;
      if (!o->foe) o->foe = at->attacker == S.sworn ? at->defender : at->attacker;
    }
  const realm::Siege* sg = siegeById(g, o->siege);
  Quest* q = questOf(g, *o);
  const bool waiting = !o->siege && (o->kind == OrderKind::BreakSiege || o->kind == OrderKind::Assault || o->kind == OrderKind::Supply);
  if (waiting) return;   // (no siege at its place: it waits for one, or for its due day)
  // the camp the war's overlays laid out is where the besiegers really stand (its spot is chosen once, in global
  // terms, near the realm's campX / campY): the order and its marker follow it
  if (o->siege && (o->kind == OrderKind::BreakSiege || o->kind == OrderKind::Assassinate))
    if (const WarCamp* c = warCampOf(g, o->siege))
      if (c->gx != o->gx || c->gy != o->gy) {
        o->gx = c->gx; o->gy = c->gy;
        if (q) { q->tgx = c->gx; q->tgy = c->gy; q->hasPos = true; }
      }
  const float dT = tilesTo(g, o->gx, o->gy);
  switch (o->kind) {
    case OrderKind::BreakSiege:
    case OrderKind::Assault: {
      const bool assault = o->kind == OrderKind::Assault;
      if (!sg || sg->over) {
        const bool won = sg && assault == sg->attackerWon;
        finishOrder(g, id, won && q && q->have > 0);
        return;
      }
      if (q && q->have >= q->need && (assault || (o->flags & service::OF_CAPDEAD))) {
        if (assault) {
          if (std::find(g.war.gatesOpen.begin(), g.war.gatesOpen.end(), sg->site) == g.war.gatesOpen.end()) g.war.gatesOpen.push_back(sg->site);
          g.war.dirty = true;
        }
        g.realm.addRep(S.sworn, 8);
        g.say(assault ? "THE GATE IS OPEN! THE TOWN IS OURS FOR THE TAKING." : "THE CAMP IS BROKEN! THE SIEGE WILL NOT HOLD.");
        finishOrder(g, id, true);
        return;
      }
      break;
    }
    case OrderKind::Supply: {
      if (!sg || sg->over) { finishOrder(g, id, false); return; }
      if (!(o->flags & service::OF_REACHED)) {
        int32_t fx, fy;
        if (siteHeart(g, o->from, fx, fy) && tilesTo(g, fx, fy) < 9.0f) {
          o->flags |= service::OF_REACHED;
          g.say("THE QUARTERMASTER LOADS THE CARTS: GRAIN, ARROWS AND BANDAGES. NOW GET THEM THROUGH.");
          g.sfx((int)Sfx::Pickup, g.pl().p);
          if (q) { int32_t hx, hy; if (siteHeart(g, o->site, hx, hy)) { q->tgx = hx; q->tgy = hy; q->hasPos = true; } }
          refresh();
        } else if (q && siteHeart(g, o->from, fx, fy) && (q->tgx != fx || q->tgy != fy)) { q->tgx = fx; q->tgy = fy; q->hasPos = true; }
      } else {
        int32_t hx, hy;
        if (siteHeart(g, o->site, hx, hy) && tilesTo(g, hx, hy) < 9.0f) {
          const float add = std::max(0.05f, sg->def * 0.05f);
          g.realm.contribute(sg->id, 2, add);
          o->flags |= service::OF_BOOST;
          o->aux = (uint16_t)std::clamp((int)std::lround(add * 100.0f), 0, 65535);
          o->untilDay = dayU16(g.day + 3);
          g.say("THE SUPPLIES ARE IN. THE GARRISON WILL HOLD STRONGER FOR DAYS.");
          finishOrder(g, id, true);
          return;
        }
      }
      break;
    }
    case OrderKind::Scout: {
      if (!(o->flags & service::OF_REACHED)) {
        if (dT < 10.0f) {
          o->flags |= service::OF_REACHED;
          const realm::KingdomState* F = g.realm.kingdom(o->foe);
          char b[160];
          std::snprintf(b, sizeof b, "YOU COUNT THEIR STRENGTH: ABOUT %d MEN UNDER ARMS. NOW GET BACK ALIVE.", F ? std::max(25, (int)std::lround(F->military * 25)) : 100);
          g.say(b);
          if (q) { int32_t fx, fy; if (siteHeart(g, o->from, fx, fy)) { q->tgx = fx; q->tgy = fy; q->hasPos = true; } }
          refresh();
        }
      } else {
        // back at the place the order named, or any town of ours
        bool home = false;
        int32_t fx, fy;
        if (o->from && siteHeart(g, o->from, fx, fy) && tilesTo(g, fx, fy) < 11.0f) home = true;
        if (!home) {
          float dd;
          const ew::Gid near = nearestOwned(g, S.sworn, plGx(g), plGy(g), 0, false, dd);
          home = near && dd < 9.0f;
        }
        if (home) {
          // a scouting report tips the war's numbers a little: our next siege knows where to strike
          for (const realm::Siege& x : g.realm.sieges())
            if (!x.over && ((x.attacker == S.sworn && x.defender == o->foe) || (x.defender == S.sworn && x.attacker == o->foe)))
              g.realm.contribute(x.id, x.attacker == S.sworn ? 1 : 2, 0.25f);
          g.say("YOUR REPORT IS IN THE CAPTAINS' HANDS.");
          finishOrder(g, id, true);
          return;
        }
      }
      break;
    }
    case OrderKind::Assassinate: {
      if (o->flags & service::OF_CAPDEAD) {
        // -15 % atk: the siege he led (or any of theirs against us) loses the equivalent to our side
        for (const realm::Siege& x : g.realm.sieges())
          if (!x.over && x.attacker == o->foe && x.defender == S.sworn) { g.realm.contribute(x.id, 2, x.atk * 0.15f); break; }
        g.say("THE ENEMY CAPTAIN IS DEAD. THEIR MEN WILL FIGHT WITHOUT A HEAD.");
        finishOrder(g, id, true);
        return;
      }
      if (o->siege && (!sg || sg->over)) { g.say("THE ENEMY CAMP HAS BROKEN UP. THEIR CAPTAIN GOT AWAY."); finishOrder(g, id, false); return; }
      // no siege camp of theirs: a field camp on their side of the front, its captain and his guard
      if (!o->siege && S.rt.foes.empty() && dT < 45.0f) {
        const Vec2 c = localPx(g, o->gx, o->gy);
        S.rt.foes.push_back(spawnFoe(Role::Captain, g.freeSpot(o->gx - g.world.ox, o->gy - g.world.oy), level + 1));
        for (int k = 0; k < 3; k++) S.rt.foes.push_back(spawnFoe(Role::Soldier, freeNear(c, 3.0f, 0.7f + k * 2.1f), level));
        for (int fid : S.rt.foes) {
          const int ai = g.findActor(fid);
          if (ai > 0) g.actors[(size_t)ai].special = 0.35f;
        }
      }
      break;
    }
    case OrderKind::Escort: {
      int32_t hx, hy;
      const bool dest = siteHeart(g, o->site, hx, hy);
      if (!(o->flags & service::OF_REACHED)) {
        int32_t fx, fy;
        if (siteHeart(g, o->from, fx, fy)) {
          if (q && (q->tgx != fx || q->tgy != fy)) { q->tgx = fx; q->tgy = fy; q->hasPos = true; }
          if (tilesTo(g, fx, fy) < 11.0f) {
            o->flags |= service::OF_REACHED;
            o->aux = 3;
            g.say("THE BURNED-OUT FAMILIES GATHER THEIR BUNDLES. THEY'LL FOLLOW YOU.");
            if (q && dest) { q->tgx = hx; q->tgy = hy; }
            refresh();
          }
        }
        break;
      }
      // the people follow (they come back into the window with the player after a door or a long road)
      for (size_t i = 0; i < S.rt.walkers.size();) {
        const int ai = g.findActor(S.rt.walkers[i]);
        if (ai > 0 && g.actors[(size_t)ai].st == AState::Dead) {
          if (o->aux > 0) o->aux--;
          g.say(o->aux ? "ONE OF THE PEOPLE YOU LEAD IS DEAD." : "THE LAST OF THEM IS DEAD.");
          S.rt.walkers.erase(S.rt.walkers.begin() + (std::ptrdiff_t)i);
          refresh();
        } else if (ai <= 0) S.rt.walkers.erase(S.rt.walkers.begin() + (std::ptrdiff_t)i);
        else i++;
      }
      if (o->aux == 0) { finishOrder(g, id, false); return; }
      while ((int)S.rt.walkers.size() < (int)o->aux) {
        const float ang = 1.3f + 0.8f * (float)S.rt.walkers.size();
        S.rt.walkers.push_back(spawnMan(g, Role::Refugee, 0, freeNear(g.pl().p, 2.5f, ang), nextSlot(), 1));
      }
      const Actor& P = g.pl();
      for (size_t k = 0; k < S.rt.walkers.size(); k++) {
        const int ai = g.findActor(S.rt.walkers[k]);
        if (ai <= 0) continue;
        Actor& a = g.actors[(size_t)ai];
        const Vec2 slot = P.p + Vec2(std::cos(2.4f + 1.1f * (float)k) * 26.0f, std::sin(2.4f + 1.1f * (float)k) * 20.0f + 14.0f);
        a.goal = slot;
        a.special = std::clamp(len(slot - a.p) / 50.0f, 0.4f, 1.5f);
        if (len(a.p - P.p) > 26.0f * TILE) { a.p = g.freeSpot((int)std::floor(P.p.x / TILE) + (int)k - 1, (int)std::floor(P.p.y / TILE) + 9); a.navNext = -1; }
      }
      // the road's ambush, about halfway
      if (!(o->flags & service::OF_AMBUSH) && dest) {
        int32_t fx, fy;
        const float total = siteHeart(g, o->from, fx, fy) ? std::hypot((float)(hx - fx), (float)(hy - fy)) : 0.0f;
        const float left = tilesTo(g, hx, hy);
        if (total > 20.0f && left < total * 0.6f && left > 12.0f) {
          o->flags |= service::OF_AMBUSH;
          const float ang = std::atan2((float)(hy - plGy(g)), (float)(hx - plGx(g)));
          for (int k = 0; k < 2; k++) S.rt.foes.push_back(spawnFoe(Role::Soldier, freeNear(P.p, 10.0f, ang + (k ? 0.5f : -0.5f)), level));
          g.say("RAIDERS ON THE ROAD! PROTECT THE PEOPLE!");
        }
      }
      // arrived: any of them at the town's heart
      if (dest)
        for (int wid : S.rt.walkers) {
          const int ai = g.findActor(wid);
          if (ai <= 0) continue;
          const Actor& a = g.actors[(size_t)ai];
          const float d = std::hypot(g.world.ox + a.p.x / TILE - hx, g.world.oy + a.p.y / TILE - hy);
          if (d < 10.0f) {
            g.realm.addRep(S.sworn, 2 * (int)o->aux);
            g.say(std::to_string(o->aux) + (o->aux == 1 ? " SOUL" : " SOULS") + " BROUGHT TO SAFETY.");
            // they walk on into the town
            for (int w2 : S.rt.walkers) S.rt.going.push_back({w2, 20.0f});
            for (int w2 : S.rt.walkers) { const int bi = g.findActor(w2); if (bi > 0) { g.actors[(size_t)bi].goal = localPx(g, hx, hy); g.actors[(size_t)bi].special = 0.45f; } }
            S.rt.walkers.clear();
            finishOrder(g, id, true);
            return;
          }
        }
      break;
    }
    case OrderKind::Hold: {
      const bool night = g.hour >= 21.0f || g.hour < 4.0f;
      if (!(o->flags & service::OF_REACHED)) {
        if (night && dT < 14.0f) {
          o->flags |= service::OF_REACHED;
          o->aux = 0;
          // the raiders come from the enemy's side of the town
          float fd;
          const ew::Gid theirs = nearestOwned(g, o->foe, o->gx, o->gy, 0, false, fd);
          int32_t tx = o->gx + 30, ty = o->gy;
          if (theirs) siteHeart(g, theirs, tx, ty);
          const float ang = std::atan2((float)(ty - o->gy), (float)(tx - o->gx));
          const Vec2 c = localPx(g, o->gx, o->gy);
          for (int k = 0; k < 4; k++) S.rt.foes.push_back(spawnFoe(Role::Soldier, freeNear(c, 15.0f + (float)(k % 2), ang + (k - 1.5f) * 0.25f), level));
          S.rt.raidT = 40.0f;
          g.say("TORCHES IN THE DARK! THE RAID IS COMING. HOLD THE LINE!");
          g.sfx((int)Sfx::Bell, g.pl().p);
          refresh();
        }
        break;
      }
      // the raiders make for the town's heart
      const Vec2 heart = localPx(g, o->gx, o->gy);
      for (int fid : S.rt.foes) {
        const int ai = g.findActor(fid);
        if (ai > 0 && g.actors[(size_t)ai].target < 0) { g.actors[(size_t)ai].goal = heart; g.actors[(size_t)ai].special = 0.8f; }
      }
      if (!(o->flags & service::OF_AMBUSH)) {
        S.rt.raidT -= step;
        if (S.rt.raidT <= 0) {
          o->flags |= service::OF_AMBUSH;
          const float ang = (float)(hash32(id) % 628) / 100.0f;
          for (int k = 0; k < 3; k++) S.rt.foes.push_back(spawnFoe(Role::Soldier, freeNear(heart, 15.0f, ang + k * 0.3f), level));
          g.say("A SECOND WAVE!");
        }
      }
      refresh();
      const bool dawn = g.hour >= 5.0f && g.hour < 12.0f;
      if (dT > 40.0f) { g.say("YOU LEFT THE WALLS. THE GARRISON HELD THE NIGHT WITHOUT YOU."); finishOrder(g, id, false); return; }
      if ((o->flags & service::OF_AMBUSH) && S.rt.foes.empty() && o->aux > 0) { g.say("THE LAST RAIDER FALLS. THE FRONTIER HOLDS."); finishOrder(g, id, true); return; }
      if (dawn) { g.say("DAWN. THE RAIDERS MELT AWAY AND THE FRONTIER STILL STANDS."); finishOrder(g, id, true); return; }
      break;
    }
    default: break;
  }
}

// ---------------------------------------------------------------- the captains of the watch
void ServiceOps::captainsStep(Game& g, float dt) {
  service::Service& S = g.service;
  S.rt.capT -= dt;
  if (S.rt.capT > 0) return;
  S.rt.capT = 0.5f;
  if (g.inside || !g.world.endless || g.world.over.ground.empty()) return;
  const Vec2 pp = g.pl().p;
  // put away the far, the dead stay down until the player leaves; a town that changed hands gets a new captain
  for (size_t i = 0; i < S.rt.captains.size();) {
    const ew::Gid site = S.rt.captains[i].first;
    const int si = g.world.siteHandle(site);
    const int ai = S.rt.captains[i].second >= 0 ? g.findActor(S.rt.captains[i].second) : -1;
    bool drop = si < 0;
    if (!drop) {
      const Site& s = g.world.sites[(size_t)si];
      const float d = std::hypot(s.ex - pp.x / TILE, s.ey - pp.y / TILE);
      const ew::Gid owner = s.kingdom >= 0 ? g.world.kingdoms[(size_t)s.kingdom].id : 0;
      if (d > 62.0f) drop = true;
      else if (ai > 0 && g.actors[(size_t)ai].realm != owner && g.actors[(size_t)ai].st != AState::Dead) drop = true;
      else if (S.rt.captains[i].second >= 0 && ai <= 0) drop = true;   // (cleared by a door or a teleport: he comes back)
    }
    if (drop) {
      if (ai > 0 && !(g.mode == Mode::Dialogue && g.dlg.actor == g.actors[(size_t)ai].id) && g.actors[(size_t)ai].st != AState::Dead) g.actors.erase(g.actors.begin() + ai);
      S.rt.captains.erase(S.rt.captains.begin() + (std::ptrdiff_t)i);
    } else {
      if (ai > 0 && g.actors[(size_t)ai].st == AState::Dead) S.rt.captains[i].second = -1;
      i++;
    }
  }
  for (int si : g.activeSites_) {
    if (si < 0 || si >= (int)g.world.sites.size()) continue;
    const Site& s = g.world.sites[(size_t)si];
    if (s.type != SiteType::Town && s.type != SiteType::City) continue;
    if (s.kingdom < 0 || s.kingdom >= (int)g.world.kingdoms.size() || warFrontier(g, si) || warEmpty(g, si)) continue;
    if (std::hypot(s.ex - pp.x / TILE, s.ey - pp.y / TILE) > 46.0f) continue;
    bool have = false;
    for (const auto& c : S.rt.captains) have |= c.first == s.id;
    if (have) continue;
    const ew::Gid owner = g.world.kingdoms[(size_t)s.kingdom].id;
    // his post: before the barracks, else the keep or the palace, else by the heart
    int px = s.ex + 2, py = s.ey + 2;
    int pri = 0;
    for (int b = s.bldgFirst; b < s.bldgFirst + s.bldgCount && b < (int)g.world.over.bldgs.size(); b++) {
      const Bldg& B = g.world.over.bldgs[(size_t)b];
      const int p = B.type == art::Building::Barracks ? 3 : (B.type == art::Building::Keep ? 2 : (B.type == art::Building::Palace ? 1 : 0));
      if (p > pri) { pri = p; px = B.doorX() + 1; py = B.doorY() + 2; }
    }
    const int slot = SLOT_CAPTAIN0 + (int)(hash32((uint32_t)s.id ^ (uint32_t)(s.id >> 32)) % (uint32_t)SLOT_CAPTAINN);
    const int aid = spawnMan(g, Role::Captain, owner, g.freeSpot(px, py), slot, std::max(1, s.level) + 1);
    const int ai = g.findActor(aid);
    if (ai > 0) { g.actors[(size_t)ai].site = si; g.actors[(size_t)ai].special = 0.32f; }
    S.rt.captains.push_back({s.id, aid});
  }
  // the captains keep their post: a slow pace now and then, back and forth before the door (never frozen to a tile)
  for (const auto& c : S.rt.captains) {
    if (c.second < 0) continue;
    const int ai = g.findActor(c.second);
    if (ai <= 0) continue;
    Actor& a = g.actors[(size_t)ai];
    if (a.target >= 0 || a.st == AState::Dead) continue;
    if (len2(a.goal - a.p) < 5.0f * 5.0f && (hash32((uint32_t)a.id * 31u + (uint32_t)(g.time * 2.0f)) % 23) == 0) {
      const float ang = (float)(hash32((uint32_t)a.id ^ (uint32_t)(g.time * 7.0f)) % 628) / 100.0f;
      a.goal = a.home + Vec2(std::cos(ang) * 14.0f, std::sin(ang) * 8.0f);
      a.special = 0.25f + 0.1f * (float)(a.id % 3);
    }
  }
}
