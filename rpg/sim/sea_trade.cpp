// M10 "Beyond the Mist": harbours and the boat trade (VISION_PLAN 2.12, 15.4, 15.1; rpg/sim/sea_ops.h). SEA lane.
//   - the nearest port round the hero (Live::near*) and the ports known (visited, or learned from a harbourmaster):
//     ferry routes join two known ports;
//   - the dock folk: a port's harbourmaster, a harbour's shipwright and an old sailor on the pier (the world's own
//     people take the part once the WORLD lane's harbours stand them there; until then these keep the pier);
//   - talk (DLG_SEA 7000..7500): boats for sale at 2.12's prices and levels (the Sailor earlier and cheaper), the yard's
//     repairs, upgrades and a sunk boat raised again, ferry passages and the routes, a land's chart, the old sailor's
//     tale (the chart chain's first step: sea_chain.cpp), a fishmonger's order, the word carried to a king;
//   - ferries that play out on deck: the ferry's hull boarded at the berth, the captain's autopilot out to sea with the
//     clock running fast, a VoyageEvent (pirates, a storm, a sea serpent) fought for real, then journey's end behind the
//     fade at the destination's pier;
//   - the Sailor's hook (15.1): a skiff of their own at the start, or the first boat at half price.
#include "rpg/sim/sea_ops.h"

#include "engine/audio.h"

#include <algorithm>
#include <cmath>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/talk.h"
#include "rpg/world/source.h"

using namespace sea;

namespace {
constexpr float kPi = 3.14159265f;
enum : int {
  K_BUY = 10,        // arg: hull
  K_ROUTES = 20,     // learn the routes (ports known)
  K_PASSAGE = 30,    // arg: the destination's index among passageList
  K_REPAIR = 40,     // arg: boats index
  K_UPGRADE = 50,    // arg: boats index * 16 + upgrades() index
  K_RAISE = 60,      // arg: boats index
  K_TALE = 70,       // arg: unused (the old sailor's tale of the nearest hidden land)
  K_RUMOUR = 75,     // a captain's rumour of unmapped shores (25 gold)
  K_MAP = 80,        // arg: index among mapList (a land's chart: LF_MAPPED)
  K_FISHJOB = 90,    // a fishmonger's order
  K_WORD = 95,       // arg: quest id: the word of the land beyond the Mist told to a king
};
constexpr int FOLK_SLOT = 9100;   // Actor::slot of the dock folk this file stands on a pier (role order below)
constexpr Mk MK_SEA_EARLYSALE = (Mk)200;   // (key 0) the Sailor's first boat is half price (1: still owed)

float distTiles(int32_t ax, int32_t ay, int32_t bx, int32_t by) { return (float)std::hypot((double)ax - bx, (double)ay - by); }
Vec2 localPx(const Game& g, int32_t gx, int32_t gy) { return Vec2((float)(gx - g.world.ox) * TILE + 8.0f, (float)(gy - g.world.oy) * TILE + 8.0f); }
std::string fit(std::string s, size_t n) { if (s.size() > n) s.resize(n); return s; }

// the port the hero stands at (its plan), when Live::nearPort is within reach
bool portHere(Game& g, ew::PortPlan& out, float reachTiles = 60.0f) {
  const Sea::Live& L = g.sea.live;
  if (!L.nearPort || !g.world.src) return false;
  const int32_t hx = SeaOps::heroGx(g), hy = SeaOps::heroGy(g);
  for (const ew::PortPlan& p : g.world.src->portsIn(hx - 120, hy - 120, hx + 120, hy + 120))
    if (p.site == L.nearPort) { out = p; return distTiles(hx, hy, p.pierX, p.pierY) < reachTiles || distTiles(hx, hy, p.x, p.y) < reachTiles; }
  return false;
}
// known harbours a ferry from here may sail to (nearest first; at most 4; never a hidden land not yet reached)
std::vector<ew::PortPlan> passageList(Game& g, const ew::PortPlan& here) {
  std::vector<ew::PortPlan> out;
  if (!here.harbour || !g.world.src) return out;
  constexpr int32_t R = 9000;
  for (const ew::PortPlan& p : g.world.src->portsIn(here.x - R, here.y - R, here.x + R, here.y + R)) {
    if (p.site == here.site || !p.harbour || !g.sea.knowsPort(p.site)) continue;
    const ew::LandPlan L = g.world.src->land(p.land);
    if (L.hidden) { const LandRecord* r = g.sea.land(L.id); if (!r || !(r->flags & LF_LANDED)) continue; }
    out.push_back(p);
  }
  std::sort(out.begin(), out.end(), [&](const ew::PortPlan& a, const ew::PortPlan& b) {
    const float da = distTiles(a.x, a.y, here.x, here.y), db = distTiles(b.x, b.y, here.x, here.y);
    return da < db || (da == db && a.site < b.site);
  });
  if (out.size() > 4) out.resize(4);
  return out;
}
// lands a harbourmaster has charts of: every land within reach that is not hidden, not the one we stand on, not mapped
std::vector<ew::LandPlan> mapList(Game& g, const ew::PortPlan& here) {
  std::vector<ew::LandPlan> out;
  if (!g.world.src) return out;
  constexpr int32_t R = 16000;
  for (const ew::LandPlan& L : g.world.src->landsIn(here.x - R, here.y - R, here.x + R, here.y + R)) {
    if (L.hidden || L.id == here.land) continue;
    const LandRecord* r = g.sea.land(L.id);
    if (r && (r->flags & LF_MAPPED)) continue;
    out.push_back(L);
  }
  std::sort(out.begin(), out.end(), [&](const ew::LandPlan& a, const ew::LandPlan& b) {
    return distTiles(a.cx, a.cy, here.x, here.y) < distTiles(b.cx, b.cy, here.x, here.y);
  });
  if (out.size() > 2) out.resize(2);
  return out;
}
int landSites(Game& g, const ew::LandPlan& L) {
  const int32_t r = std::max<int32_t>(400, L.r);
  int n = 0;
  for (const ew::SettlementNode& s : g.world.src->settlementsIn(L.cx - r, L.cy - r, L.cx + r, L.cy + r))
    if (g.world.src->landAt(s.x, s.y) == L.id) n++;
  return n;
}
std::string landName(const ew::LandPlan& L) { return L.name.empty() ? std::string("THE FAR SHORE") : L.name; }
bool isFishmonger(const Game& g, const Actor& a) {
  if (a.role == Role::Fisher) return true;
  if (a.role != Role::Merchant) return false;
  if (g.inside && g.subBldg >= 0 && g.subBldg < (int)g.world.over.bldgs.size()) return g.world.over.bldgs[(size_t)g.subBldg].type == art::Building::Fishmonger;
  if (a.bldg >= 0 && a.bldg < (int)g.world.over.bldgs.size()) return g.world.over.bldgs[(size_t)a.bldg].type == art::Building::Fishmonger;
  return false;
}
// a boat of ours the yard can work on here: moored at this port, or lying close by
std::vector<int> yardBoats(const Game& g, ew::Gid port) {
  std::vector<int> out;
  for (int i = 0; i < (int)g.sea.boats.size(); i++) {
    const Boat& b = g.sea.boats[(size_t)i];
    if (b.ferry || b.sunk || i == g.sea.aboard) continue;
    const Vec2 bp = localPx(g, b.gx, b.gy);
    if (b.dock == port || len(bp - g.pl().p) < 14.0f * TILE) out.push_back(i);
  }
  return out;
}
}  // namespace

// ---------------------------------------------------------------- the nearest port and the ports known
void SeaOps::portScan(Game& g) {
  Sea::Live& L = g.sea.live;
  L.nearPort = 0;
  L.nearDist = 1e9f;
  if (g.inside || !g.world.src || !g.world.endless) return;
  const int32_t hx = heroGx(g), hy = heroGy(g);
  const Vec2 hero = g.pl().p;
  for (const ew::PortPlan& p : g.world.src->portsIn(hx - 160, hy - 160, hx + 160, hy + 160)) {
    const Vec2 berth = localPx(g, p.berthX, p.berthY);
    const float d = len(berth - hero);
    if (d < L.nearDist) {
      L.nearDist = d; L.nearPort = p.site; L.nearBerth = berth; L.nearPier = localPx(g, p.pierX, p.pierY);
      L.nearHarbour = p.harbour; L.nearName = p.name;
    }
    // standing in a port (or sailing into its harbour) makes it known: a ferry route's end
    if (!g.sea.knowsPort(p.site) && (distTiles(hx, hy, p.x, p.y) < 40.0f || distTiles(hx, hy, p.berthX, p.berthY) < 14.0f)) {
      g.sea.portsKnown.insert(std::lower_bound(g.sea.portsKnown.begin(), g.sea.portsKnown.end(), p.site), p.site);
    }
  }
}

// ---------------------------------------------------------------- the dock folk
void SeaOps::tradeStep(Game& g, float dt) {
  Sea::Live& L = g.sea.live;
  if ((L.portScanT -= dt) <= 0) { L.portScanT = 0.5f; portScan(g); }
  if ((L.folkT -= dt) > 0) return;
  L.folkT = 1.5f;
  // put away dock folk the hero has left behind (beyond 70 tiles), and see who the near port lacks
  const Vec2 hero = g.pl().p;
  for (int i = (int)g.actors.size() - 1; i >= 1; i--) {
    const Actor& a = g.actors[(size_t)i];
    if (a.slot < FOLK_SLOT || a.slot > FOLK_SLOT + 3 || a.fromMap || !a.npc) continue;
    if (len(a.p - hero) > 70.0f * TILE) g.actors.erase(g.actors.begin() + i);
  }
  if (!L.nearPort || L.nearDist > 45.0f * TILE) return;
  const int si = g.world.siteHandle(L.nearPort);
  // who stands at this pier already (the world's own harbour folk, or ours)
  bool have[3] = {false, false, false};
  static const Role kRoles[3] = {Role::Harbourmaster, Role::Shipwright, Role::Sailor};
  for (const Actor& a : g.actors) {
    if (!a.npc || a.st == AState::Dead) continue;
    for (int k = 0; k < 3; k++)
      if (a.role == kRoles[k] && len(a.p - L.nearPier) < 30.0f * TILE) have[k] = true;
  }
  for (int k = 0; k < 3; k++) {
    if (have[k] || (k == 1 && !L.nearHarbour)) continue;
    // a spot on the shore by the pier: the harbourmaster at its foot, the shipwright by the slipway, the old sailor
    // on a bollard a little way along
    const int px = (int)std::floor(L.nearPier.x / TILE), py = (int)std::floor(L.nearPier.y / TILE);
    const int dx = k == 0 ? 0 : k == 1 ? -3 : 3, dy = k == 0 ? -1 : 1;
    Vec2 at;
    const int tx = px + dx, ty = py + dy;
    if (!g.world.over.in(tx, ty)) continue;
    Vec2 spot = g.freeSpot(tx, ty);
    if (!coastNear(g, spot, 6, at)) at = spot;
    if (groundWater(g.world.over.at((int)std::floor(at.x / TILE), (int)std::floor(at.y / TILE)))) continue;
    Actor a;
    a.id = g.nextId_++;
    a.npc = true; a.human = true; a.role = kRoles[k];
    a.faction = Faction::Town;
    a.p = at; a.home = at; a.goal = at;
    a.site = si; a.slot = FOLK_SLOT + k; a.bldg = -1;
    a.speed = 28; a.radius = 4.5f; a.maxHp = a.hp = 60;
    Rng rr(hash32((uint32_t)(L.nearPort ^ (L.nearPort >> 32)) * 31u + (uint32_t)k * 977u) ^ (uint32_t)g.seed);
    const bool female = k != 2 && rr.f() < 0.4f;
    g.makeLook(a, a.role, rr);
    a.name = makePersonName(rr, female);
    if (k == 2) a.look.hairColor = rgba(200, 200, 196);   // (an OLD sailor: grey)
    g.actors.push_back(a);
  }
}

// ---------------------------------------------------------------- buying, the yard
int SeaOps::buyBoat(Game& g, Hull h, ew::Gid port) {
  if (!g.world.src) return -1;
  const int32_t hx = heroGx(g), hy = heroGy(g);
  for (const ew::PortPlan& p : g.world.src->portsIn(hx - 160, hy - 160, hx + 160, hy + 160)) {
    if (p.site != port) continue;
    const ew::GTile berth{p.berthX, p.berthY};
    const int i = giveBoat(g, h, 14, "", &berth);
    if (i < 0) return -1;
    Boat& b = g.sea.boats[(size_t)i];
    b.dock = port;
    if (const int si = g.world.siteHandle(port); si >= 0) b.culture = g.world.sites[(size_t)si].culture ? g.world.sites[(size_t)si].culture : b.culture;
    ev(g, SeaEv::Buy, localPx(g, b.gx, b.gy), 1, b.id, b.name);
    return i;
  }
  return -1;
}

void SeaOps::talk(Game& g, Actor& a) {
  if (g.inside && a.role != Role::King && a.role != Role::Jarl && !isFishmonger(g, a)) return;
  Dialogue& d = g.dlg;
  const bool sailor = g.background == Background::Sailor;
  // ---- the word of the land beyond the Mist, told to a king (sea_chain.cpp's quest)
  if (a.role == Role::King || a.role == Role::Jarl) {
    for (const Quest& q : g.quests) {
      if (q.type != QType::Sea || q.state != QState::Active || q.stage != 2) continue;
      const int si = g.world.siteHandle(q.targetId);
      const bool inBldg = g.inside && g.subBldg >= 0 && g.subBldg < (int)g.world.over.bldgs.size() && g.world.over.bldgs[(size_t)g.subBldg].site == si;
      const bool here = si >= 0 && (a.site == si || g.curSite == si || inBldg);
      if (!here) continue;
      d.opts.push_back({"I BRING WORD OF A LAND BEYOND THE MIST", DLG_SEA + K_WORD, q.id});
      speech::propose(d, speech::P_STORY, speech::Topic::Story, "YOU HAVE THE LOOK OF SOMEONE WHO HAS SEEN FAR SHORES. SPEAK.");
    }
    return;
  }
  // ---- a fishmonger: an order for a fish of these waters
  if (isFishmonger(g, a)) {
    bool open = false;
    for (const Quest& q : g.quests) if (q.type == QType::Sea && q.stage == 3 && q.state != QState::Done) open = true;
    if (!open) d.opts.push_back({"NEED ANY FISH?", DLG_SEA + K_FISHJOB, 0});
    return;
  }
  ew::PortPlan P;
  const bool atPort = portHere(g, P);
  if (a.role == Role::Harbourmaster) {
    if (!atPort) { speech::propose(d, speech::P_SERVICE, speech::Topic::Service, "NO BERTHS TO LET OUT HERE, FRIEND."); return; }
    std::string line = P.harbour ? "WELCOME TO " + P.name + ". BERTHS, BOATS AND PASSAGE: ASK AWAY."
                                 : "IT'S ONLY A JETTY, BUT A ROWBOAT'S A ROWBOAT. YOURS FOR THE GOLD.";
    // boats for sale: a jetty sells rowboats, a harbour sloops, a city's harbour galleons too (2.12)
    for (int h = 0; h < (int)Hull::COUNT; h++) {
      const Hull hull = (Hull)h;
      if (hull != Hull::Rowboat && !P.harbour) continue;
      if (hull == Hull::Galleon && P.type != SiteType::City) continue;
      const HullInfo& H = hullInfo(hull);
      int price = hullPrice(hull, sailor);
      if (g.marks.count(markKey(0, MK_SEA_EARLYSALE)) && g.marks[markKey(0, MK_SEA_EARLYSALE)] == 1) price /= 2;
      const int lvl = sailor ? H.sailorLevel : H.minLevel;
      d.opts.push_back({std::string("BUY A ") + H.name + " (" + std::to_string(price) + " GOLD" + (g.plLevel < lvl ? ", LEVEL " + std::to_string(lvl) : "") + ")",
                        DLG_SEA + K_BUY, h});
    }
    if (P.harbour) {
      d.opts.push_back({"WHERE DO YOUR SHIPS SAIL?", DLG_SEA + K_ROUTES, 0});
      const std::vector<ew::PortPlan> to = passageList(g, P);
      for (int i = 0; i < (int)to.size(); i++) {
        const int tiles = (int)distTiles(P.x, P.y, to[(size_t)i].x, to[(size_t)i].y);
        d.opts.push_back({"PASSAGE TO " + fit(to[(size_t)i].name, 16) + " (" + std::to_string(fareOf(tiles)) + " GOLD)", DLG_SEA + K_PASSAGE, i});
      }
      const std::vector<ew::LandPlan> maps = mapList(g, P);
      for (int i = 0; i < (int)maps.size(); i++)
        d.opts.push_back({"A CHART OF " + fit(landName(maps[(size_t)i]), 16) + " (" + std::to_string(mapPrice(landSites(g, maps[(size_t)i]))) + " GOLD)", DLG_SEA + K_MAP, i});
    }
    speech::propose(d, speech::P_SERVICE, speech::Topic::Service, line);
    return;
  }
  if (a.role == Role::Shipwright) {
    if (!atPort) return;
    std::string line = "SHE'LL FLOAT IF I BUILT HER. WHAT DO YOU NEED?";
    for (int i : yardBoats(g, P.site)) {
      const Boat& b = g.sea.boats[(size_t)i];
      if (b.hp < maxHp(b) - 0.5f) d.opts.push_back({"REPAIR " + fit(b.name, 18) + " (" + std::to_string(repairPrice(b)) + " GOLD)", DLG_SEA + K_REPAIR, i});
      int shown = 0;
      std::vector<int> ups;
      for (int u = 0; u < (int)upgrades().size(); u++) {
        const UpgradeInfo& U = upgrades()[(size_t)u];
        if ((b.upgrades & U.bit) || !(U.hulls & (1 << (int)b.hull))) continue;
        ups.push_back(u);
      }
      std::sort(ups.begin(), ups.end(), [](int x, int y) { return upgrades()[(size_t)x].price < upgrades()[(size_t)y].price; });
      for (int u : ups) {
        if (shown++ >= 3) break;
        d.opts.push_back({std::string("FIT ") + upgrades()[(size_t)u].name + " (" + std::to_string(upgrades()[(size_t)u].price) + " GOLD)", DLG_SEA + K_UPGRADE, i * 16 + u});
      }
    }
    for (int i = 0; i < (int)g.sea.boats.size(); i++) {
      const Boat& b = g.sea.boats[(size_t)i];
      if (!b.sunk || b.ferry) continue;
      if (b.hull != Hull::Rowboat && !P.harbour) continue;
      d.opts.push_back({std::string("RAISE A NEW ") + hullName(b.hull) + " (" + std::to_string(raisePrice(b.hull, sailor)) + " GOLD)", DLG_SEA + K_RAISE, i});
      line = "LOST HER, DID YOU? THE SEA TAKES ITS SHARE. I CAN BUILD YOU ANOTHER.";
    }
    speech::propose(d, speech::P_SERVICE, speech::Topic::Service, line);
    return;
  }
  if (a.role == Role::Sailor) {
    // the old sailor of a pier: the tale of a land beyond the Mist (the chart chain's first step), else a rumour
    d.opts.push_back({"TELL ME OF THE MIST", DLG_SEA + K_TALE, 0});
    d.opts.push_back({"ANY WORD OF UNMAPPED SHORES? (25 GOLD)", DLG_SEA + K_RUMOUR, 0});
    speech::propose(d, speech::P_NEWS, speech::Topic::News,
                    sailor ? "ANOTHER OLD SALT! SIT, SIT. THE SEA'S BEEN STRANGE THIS YEAR." : "FORTY YEARS AT SEA, AND I'VE SEEN THINGS THAT'D TURN YOUR HAIR WHITE.");
    return;
  }
}

bool SeaOps::choose(Game& g, const DlgOpt& o) {
  const int kind = o.action - DLG_SEA;
  Dialogue& d = g.dlg;
  const bool sailor = g.background == Background::Sailor;
  auto drop = [&]() {
    for (size_t i = 0; i < d.opts.size(); i++) if (d.opts[i].action == o.action && d.opts[i].arg == o.arg) { d.opts.erase(d.opts.begin() + (std::ptrdiff_t)i); break; }
  };
  ew::PortPlan P;
  const bool atPort = portHere(g, P);
  switch (kind) {
    case K_BUY: {
      if (!atPort) { d.text = "THERE'S NO WATER HERE FOR A BOAT."; return true; }
      const Hull h = (Hull)std::clamp(o.arg, 0, (int)Hull::COUNT - 1);
      const HullInfo& H = hullInfo(h);
      const int lvl = sailor ? H.sailorLevel : H.minLevel;
      if (g.plLevel < lvl) { d.text = "COME BACK WHEN YOU'VE MORE SEA IN YOU. LEVEL " + std::to_string(lvl) + ", SAY."; return true; }
      int price = hullPrice(h, sailor);
      const bool early = g.marks.count(markKey(0, MK_SEA_EARLYSALE)) && g.marks[markKey(0, MK_SEA_EARLYSALE)] == 1;
      if (early) price /= 2;
      if (g.gold < price) { d.text = "A " + std::string(H.name) + " IS " + std::to_string(price) + " GOLD. NOT A COPPER LESS."; return true; }
      const int i = buyBoat(g, h, P.site);
      if (i < 0) { d.text = "THERE'S NO BERTH DEEP ENOUGH FOR HER HERE."; return true; }
      g.gold -= price;
      if (early) g.marks[markKey(0, MK_SEA_EARLYSALE)] = 2;
      snd(g, (int)Sfx::Buy, g.pl().p);
      d.text = "SHE'S YOURS: " + g.sea.boats[(size_t)i].name + ". SHE LIES AT THE BERTH. TREAT HER KINDLY.";
      return true;
    }
    case K_ROUTES: {
      if (!atPort) return true;
      std::vector<ew::PortPlan> fresh;
      constexpr int32_t R = 9000;
      for (const ew::PortPlan& p : g.world.src->portsIn(P.x - R, P.y - R, P.x + R, P.y + R)) {
        if (p.site == P.site || !p.harbour || g.sea.knowsPort(p.site)) continue;
        if (g.world.src->land(p.land).hidden) continue;   // (nobody sails there: the Mist)
        fresh.push_back(p);
      }
      std::sort(fresh.begin(), fresh.end(), [&](const ew::PortPlan& a, const ew::PortPlan& b) {
        return distTiles(a.x, a.y, P.x, P.y) < distTiles(b.x, b.y, P.x, P.y);
      });
      if (fresh.empty()) { d.text = "YOU KNOW EVERY PORT OUR SHIPS CALL AT."; drop(); return true; }
      std::string names;
      for (size_t i = 0; i < fresh.size() && i < 3; i++) {
        g.sea.portsKnown.insert(std::lower_bound(g.sea.portsKnown.begin(), g.sea.portsKnown.end(), fresh[i].site), fresh[i].site);
        names += (i ? (i + 1 == std::min<size_t>(fresh.size(), 3) ? " AND " : ", ") : "") + fresh[i].name;
      }
      d.text = "OUR SHIPS CALL AT " + names + ". ASK AGAIN AND I'LL BOOK YOU PASSAGE.";
      drop();
      return true;
    }
    case K_PASSAGE: {
      if (!atPort) return true;
      const std::vector<ew::PortPlan> to = passageList(g, P);
      if (o.arg < 0 || o.arg >= (int)to.size()) return true;
      const ew::PortPlan& T = to[(size_t)o.arg];
      const int tiles = (int)distTiles(P.x, P.y, T.x, T.y);
      const int fare = fareOf(tiles);
      if (g.gold < fare) { d.text = "PASSAGE TO " + T.name + " IS " + std::to_string(fare) + " GOLD."; return true; }
      // the passenger steps aboard at the pier: the hero walks out to the ferry's hull (the dialogue closes)
      g.gold -= fare;
      if (!beginVoyage(g, T.site, T.pierX, T.pierY, fare, tiles)) { g.gold += fare; d.text = "NO SHIP CAN PUT OUT FROM HERE TODAY."; return true; }
      g.closeDialogue();
      return true;
    }
    case K_REPAIR: {
      if (o.arg < 0 || o.arg >= (int)g.sea.boats.size()) return true;
      Boat& b = g.sea.boats[(size_t)o.arg];
      const int price = repairPrice(b);
      if (g.gold < price) { d.text = "PLANKS AND PITCH COST MONEY. " + std::to_string(price) + " GOLD."; return true; }
      g.gold -= price;
      b.hp = maxHp(b);
      ev(g, SeaEv::Repair, localPx(g, b.gx, b.gy), 1, b.id, b.name);
      snd(g, (int)Sfx::Block, g.pl().p, 0.8f);
      d.text = "SOUND AS THE DAY SHE WAS LAUNCHED.";
      drop();
      return true;
    }
    case K_UPGRADE: {
      const int bi = o.arg / 16, ui = o.arg % 16;
      if (bi < 0 || bi >= (int)g.sea.boats.size() || ui < 0 || ui >= (int)upgrades().size()) return true;
      Boat& b = g.sea.boats[(size_t)bi];
      const UpgradeInfo& U = upgrades()[(size_t)ui];
      if (b.upgrades & U.bit) { drop(); return true; }
      if (g.gold < U.price) { d.text = std::string(U.name) + " IS " + std::to_string(U.price) + " GOLD."; return true; }
      g.gold -= U.price;
      const float frac = b.hp / std::max(1.0f, maxHp(b));
      b.upgrades |= U.bit;
      b.hp = maxHp(b) * frac;
      if (U.bit == UP_PLATING) b.hp = maxHp(b);
      g.sea.live.serial++;
      ev(g, SeaEv::Upgrade, localPx(g, b.gx, b.gy), (float)U.bit, b.id, U.name);
      snd(g, (int)Sfx::Buy, g.pl().p);
      d.text = U.bit == UP_KEEL ? "A STORM KEEL. NOW SHE'LL TAKE THE OPEN OCEAN, IF YOU'RE MAD ENOUGH." : std::string("DONE. ") + U.name + ", FITTED AND TRUE.";
      drop();
      return true;
    }
    case K_RAISE: {
      if (!atPort || o.arg < 0 || o.arg >= (int)g.sea.boats.size()) return true;
      Boat& b = g.sea.boats[(size_t)o.arg];
      if (!b.sunk) { drop(); return true; }
      const int price = raisePrice(b.hull, sailor);
      if (g.gold < price) { d.text = "A NEW HULL IS " + std::to_string(price) + " GOLD."; return true; }
      const Hull h = b.hull;
      const uint32_t id = b.id;
      const int ni = buyBoat(g, h, P.site);   // (finds her a berth here; the new hull takes the lost one's place)
      if (ni < 0) { d.text = "THERE'S NO BERTH FOR HER HERE."; return true; }
      g.gold -= price;
      Boat nb = g.sea.boats[(size_t)ni];
      g.sea.boats.erase(g.sea.boats.begin() + ni);
      for (Boat& o2 : g.sea.boats)
        if (o2.id == id) { o2.sunk = false; o2.gx = nb.gx; o2.gy = nb.gy; o2.dock = P.site; o2.hp = maxHp(o2); o2.heading = nb.heading; }
      g.sea.nextBoatId = std::max(g.sea.nextBoatId, nb.id);   // (the temporary id is not kept)
      g.sea.live.serial++;
      ev(g, SeaEv::Raise, localPx(g, nb.gx, nb.gy), 1, id, "A NEW HULL");
      snd(g, (int)Sfx::Buy, g.pl().p);
      d.text = "SHE'S AT THE BERTH. SAME NAME, NEW TIMBER. DON'T SINK THIS ONE.";
      drop();
      return true;
    }
    case K_TALE: {
      const int32_t hx = heroGx(g), hy = heroGy(g);
      uint32_t best = 0;
      double bd = 1e18;
      for (const ew::LandPlan& L : g.world.src->landsIn(hx - 30000, hy - 30000, hx + 30000, hy + 30000)) {
        if (!L.hidden) continue;
        const LandRecord* r = g.sea.land(L.id);
        if (r && (r->flags & LF_LANDED)) continue;
        const double dd = std::hypot((double)L.cx - hx, (double)L.cy - hy);
        if (dd < bd) { bd = dd; best = L.id; }
      }
      if (!best) { d.text = "THE MIST? AYE, I'VE HEARD OF IT. BUT NOT IN THESE WATERS."; drop(); return true; }
      const Actor* teller = d.actor >= 0 && g.findActor(d.actor) >= 0 ? &g.actors[(size_t)g.findActor(d.actor)] : nullptr;
      const LandRecord* had = g.sea.land(best);
      if (had && had->chain >= Chain::Sailor) {
        d.text = had->chain == Chain::Chart ? "YOU HAVE THE CHART? THEN GO. AND TELL ME WHAT YOU FIND THERE." : "THE WRECK, LAD. FIND THE WRECK. THE CAPTAIN'S LOG WILL TELL YOU THE REST.";
        drop();
        return true;
      }
      tellOfLand(g, best, teller ? teller->name : std::string("AN OLD SAILOR"));
      ew::SitePlan wreck, cave;
      chainPlaces(g, best, wreck, cave);
      const std::string dir = dirWord(wreck.ex - hx, wreck.ey - hy, false);
      d.text = "THERE'S LAND OUT PAST THE MIST, " + dir + " OF HERE. MY CAPTAIN TRIED FOR IT. HIS SHIP LIES ON THE ROCKS STILL. FIND THE WRECK AND YOU'LL FIND HIS LOG.";
      drop();
      return true;
    }
    case K_RUMOUR: {
      if (g.gold < 25) { d.text = "TALK'S CHEAP, BUT NOT THAT CHEAP. 25 GOLD."; return true; }
      const int32_t hx = heroGx(g), hy = heroGy(g);
      std::vector<ew::LandPlan> lands = g.world.src->landsIn(hx - 20000, hy - 20000, hx + 20000, hy + 20000);
      std::sort(lands.begin(), lands.end(), [&](const ew::LandPlan& a, const ew::LandPlan& b) {
        return std::hypot((double)a.cx - hx, (double)a.cy - hy) < std::hypot((double)b.cx - hx, (double)b.cy - hy);
      });
      for (const ew::LandPlan& L : lands) {
        if (L.origin || L.hidden) continue;
        LandRecord& r = g.sea.landMut(L.id);
        if (r.flags & LF_RUMOURED) continue;
        r.flags |= LF_RUMOURED;
        g.gold -= 25;
        d.text = "THEY SAY THERE'S A WHOLE COUNTRY " + dirWord(L.cx - hx, L.cy - hy, false) + " ACROSS THE WATER. " + landName(L) + ", THE TRADERS CALL IT.";
        drop();
        return true;
      }
      d.text = "I'VE TOLD YOU ALL I KNOW. EVERY SHORE I'VE HEARD OF, YOU HAVE TOO.";
      drop();
      return true;
    }
    case K_MAP: {
      if (!atPort) return true;
      const std::vector<ew::LandPlan> maps = mapList(g, P);
      if (o.arg < 0 || o.arg >= (int)maps.size()) return true;
      const ew::LandPlan& L = maps[(size_t)o.arg];
      const int price = mapPrice(landSites(g, L));
      if (g.gold < price) { d.text = "A GOOD CHART IS " + std::to_string(price) + " GOLD. THE SEA FORGIVES NO GUESSWORK."; return true; }
      g.gold -= price;
      LandRecord& r = g.sea.landMut(L.id);
      r.flags |= LF_MAPPED | LF_RUMOURED;
      snd(g, (int)Sfx::Buy, g.pl().p);
      d.text = "HERE: " + landName(L) + ", ITS COASTS AND ITS GREAT TOWNS. MIND THE REEFS.";
      drop();
      return true;
    }
    case K_FISHJOB: {
      // an order for a fish of these waters (a species that lives in the water nearest the shop)
      int water = FW_COAST;
      if (!g.inside) {
        const int hx = (int)std::floor(g.pl().p.x / TILE), hy = (int)std::floor(g.pl().p.y / TILE);
        for (int r = 1; r < 30 && water == FW_COAST; r++)
          for (int k = 0; k < 8; k++) {
            const int tx = hx + (int)std::lround(std::cos(k * 0.785398) * r), ty = hy + (int)std::lround(std::sin(k * 0.785398) * r);
            if (g.world.over.in(tx, ty) && groundWater(g.world.over.at(tx, ty))) { water = watersAt(g, tx, ty) & ~FW_MIST; break; }
          }
      }
      const std::vector<Species>& S = species();
      std::vector<int> pool;
      for (int i = 0; i < (int)S.size(); i++) if ((S[(size_t)i].waters & water) && S[(size_t)i].rarity <= 1) pool.push_back(i);
      if (pool.empty()) { d.text = "NOT TODAY. THE BOATS BROUGHT IN PLENTY."; drop(); return true; }
      const int sp = pool[(size_t)rng(g).irange((int)pool.size())];
      const int n = S[(size_t)sp].rarity ? 2 : 3;
      const int q = seaQuest(g, 3, 0, "FISH FOR THE FISHMONGER", 0, 0, 0);
      if (q >= 0) {
        for (Quest& qq : g.quests)
          if (qq.id == q) {
            qq.subject = S[(size_t)sp].name; qq.need = n; qq.have = 0;
            qq.gold = n * S[(size_t)sp].value * 2 + 20; qq.xp = 30 + 10 * n;
            qq.giverName = g.speakerTitle();
            const Actor* me = d.actor >= 0 && g.findActor(d.actor) >= 0 ? &g.actors[(size_t)g.findActor(d.actor)] : nullptr;
            if (me) { qq.giverSite = me->site; qq.giverBldg = me->bldg; qq.giverSlot = me->slot; }
            qq.desc = "BRING " + std::to_string(n) + " " + qq.subject + " TO THE FISHMONGER.";
          }
      }
      d.text = "BRING ME " + std::to_string(n) + " " + S[(size_t)sp].name + ", FRESH. I'LL PAY WELL.";
      drop();
      return true;
    }
    case K_WORD: {
      for (Quest& q : g.quests) {
        if (q.id != o.arg || q.state != QState::Active) continue;
        LandRecord& r = g.sea.landMut((uint32_t)q.giverId);
        r.flags |= LF_WORD;
        // the two realms meet: the hidden kingdom and the king who heard the word (their relation opens)
        ew::Gid mainK = 0;
        const int si = g.world.siteHandle(q.targetId);
        if (si >= 0) if (const Kingdom* K = g.world.kingdomOf(si)) mainK = K->id;
        g.realm.openContact((uint32_t)q.giverId, mainK, g.day);
        q.state = QState::Complete;
        g.completeQuest(q);
        d.text = "A KINGDOM NOBODY KNEW? ... THEN WE SHALL SEND ENVOYS. YOU HAVE DONE THIS REALM A GREAT SERVICE.";
        g.realm.renown().fame += 30;
        if (mainK) g.realm.addRep(mainK, 10);
        break;
      }
      drop();
      return true;
    }
    default: return false;
  }
}

void SeaOps::shopStock(Game& g, const Actor& a, std::vector<Item>& stock) {
  if (!isFishmonger(g, a)) return;
  // a fishmonger's slab: today's catch of the waters round the town (by the day: the same fish all day)
  Rng r(hash32((uint32_t)g.npcKey(a) ^ (uint32_t)g.day * 2654435761u) ^ 0xF15Bu);
  const std::vector<Species>& S = species();
  for (int k = 0; k < 3; k++) {
    int sp = r.irange((int)S.size());
    for (int t = 0; t < 8 && S[(size_t)sp].rarity >= 2; t++) sp = r.irange((int)S.size());
    if (S[(size_t)sp].rarity >= 2) continue;
    const int cm = S[(size_t)sp].minCm + r.irange(std::max(1, (int)(S[(size_t)sp].maxCm - S[(size_t)sp].minCm) / 2));
    Item it = fishItem(sp, cm);
    it.count = 1 + r.irange(3);
    stock.push_back(it);
  }
}

void SeaOps::background(Game& g) {
  if (g.background != Background::Sailor) return;
  // 15.1 the Sailor: a skiff of their own at the start village's water, else the first boat at half price
  const int i = giveBoat(g, Hull::Rowboat, 40, "THE OLD SKIFF");
  if (i >= 0) { tell(g, "YOUR OLD SKIFF IS TIED UP AT THE WATER NEARBY"); return; }
  g.marks[markKey(0, MK_SEA_EARLYSALE)] = 1;
}

// ---------------------------------------------------------------- ferries on deck
bool SeaOps::beginVoyage(Game& g, ew::Gid to, int32_t toX, int32_t toY, int gold, int tiles) {
  Sea::Live& L = g.sea.live;
  if (L.voyage.on || g.sea.atSea() || !L.nearPort) return false;
  const Hull h = tiles > 2600 ? Hull::Galleon : Hull::Sloop;
  const int32_t bx = g.world.ox + (int32_t)std::floor(L.nearBerth.x / TILE), by = g.world.oy + (int32_t)std::floor(L.nearBerth.y / TILE);
  const ew::GTile berth{bx, by};
  static const char* kFerries[] = {"THE SEA WIFE", "THE FAIR PASSAGE", "THE MERCHANT'S PRIDE", "THE GOOD PACKET", "THE WANDERER", "THE BLUE MAID"};
  const int i = giveBoat(g, h, 16, kFerries[hash32((uint32_t)to ^ (uint32_t)(to >> 32)) % 6], &berth);
  if (i < 0) return false;
  g.sea.boats[(size_t)i].ferry = true;
  g.sea.boats[(size_t)i].dock = 0;
  const Vec2 heroPx = g.pl().p;
  if (!board(g, i)) { g.sea.boats.erase(g.sea.boats.begin() + i); return false; }
  Sea::Voyage& v = L.voyage;
  v = Sea::Voyage();
  v.on = true;
  v.from = L.nearPort; v.to = to;
  v.toX = toX; v.toY = toY;
  v.gold = gold;
  v.dur = std::clamp(38.0f + (float)tiles / 140.0f, 40.0f, 80.0f);
  v.hours = std::clamp((float)tiles / 260.0f, 1.0f, 40.0f);
  const float r = rng(g).f();
  v.event = r < 0.4f ? 0 : r < 0.6f ? 1 : r < 0.8f ? 2 : 3;
  v.eventAt = v.event ? v.dur * (0.3f + 0.25f * rng(g).f()) : v.dur * 0.5f;
  v.phase = 0;
  // the captain's course: out of the harbour, away from the shore (the berth's side of the heart)
  const Vec2 out = L.nearBerth - L.nearPier;
  v.course = len(out) > 1.0f ? std::atan2(out.y, out.x) : std::atan2(L.nearBerth.y - heroPx.y, L.nearBerth.x - heroPx.x);
  L.heading = v.course;
  L.autopilot = v.course;
  g.sea.voyages++;
  ev(g, SeaEv::VoyageStart, g.pl().p, (float)tiles, g.sea.boats[(size_t)i].id, "ALL ABOARD FOR THE CROSSING");
  tell(g, "ALL ABOARD! THE CREW CASTS OFF");
  return true;
}

void SeaOps::voyageStep(Game& g, float dt) {
  Sea::Live& L = g.sea.live;
  Sea::Voyage& v = L.voyage;
  if (!v.on) return;
  if (!g.sea.atSea() || !g.sea.boats[(size_t)g.sea.aboard].ferry) { v = Sea::Voyage(); return; }
  if (g.travelling()) return;
  const Boat& b = g.sea.boats[(size_t)g.sea.aboard];
  const HullInfo& H = hullInfo(b.hull);
  // the captain's helm: hold the course; land or the storm sea ahead turns her (a slow arc, never a snap)
  const Vec2 dir(std::cos(L.autopilot), std::sin(L.autopilot));
  bool blocked = false;
  for (int s = 2; s <= 7 && !blocked; s++) {
    const Vec2 c = g.pl().p + dir * (H.radius + (float)s * TILE);
    if (!sailable(g, b.hull, (int)std::floor(c.x / TILE), (int)std::floor(c.y / TILE))) blocked = true;
  }
  if (blocked || L.stuckT > 1.0f) {
    L.autopilot += (v.phase == 1 ? 0.6f : 0.9f) * dt * ((hash32((uint32_t)v.to) & 1) ? 1.0f : -1.0f) + (L.stuckT > 1.0f ? 0.8f : 0.0f);
    if (L.stuckT > 1.0f) L.stuckT = 0;
  }
  if (L.autopilot > kPi) L.autopilot -= 2 * kPi;
  if (L.autopilot < -kPi) L.autopilot += 2 * kPi;
  // the clock runs fast on deck (hours of sailing pass in a minute), except while the crew fights
  if (v.phase != 1) {
    g.hour += dt * v.hours / std::max(1.0f, v.dur);
    while (g.hour >= 24.0f) { g.hour -= 24.0f; g.day++; }
    v.t += dt;
  }
  if (v.phase == 0 && v.t >= v.eventAt) {
    if (v.event) {
      v.phase = 1;
      ev(g, SeaEv::VoyageEvent, g.pl().p, (float)v.event, b.id, v.event == 1 ? "SAIL HO! PIRATES!" : v.event == 2 ? "A STORM BEARS DOWN" : "SOMETHING BIG IN THE WATER!");
      voyageEvent(g, v.event);
      L.voyage.eventAt = 0;   // (reused as the event's clock)
    } else v.phase = 2;
  } else if (v.phase == 1) {
    v.eventAt += dt;
    const bool foes = foesNear(g, 40.0f * TILE) > 0 || !L.ships.empty();
    const bool stormy = v.event == 2 && L.storm.on && v.eventAt < 45.0f;
    if ((!foes && !stormy && v.eventAt > 4.0f) || v.eventAt > 150.0f) {
      v.phase = 2;
      tell(g, v.event == 2 ? "THE STORM PASSES. THE CREW CHEERS" : "THE CREW CHEERS: CLEAR SAILING AGAIN");
    }
  } else if (v.phase == 2 && v.t >= v.dur) {
    v.phase = 3;
    // journey's end: the hero steps off at the destination's pier (behind the fade), the ferry sails on without us
    const int32_t tx = v.toX, ty = v.toY;
    const ew::Gid dest = v.to;
    const int fi = g.sea.aboard;
    g.sea.aboard = -1;
    g.pl().swim = 0;
    g.pl().radius = 4.5f;
    L.speed = 0;
    L.autopilot = -100;
    if (fi >= 0 && fi < (int)g.sea.boats.size()) g.sea.boats.erase(g.sea.boats.begin() + fi);
    L.serial++;
    v = Sea::Voyage();
    if (!g.sea.knowsPort(dest)) g.sea.portsKnown.insert(std::lower_bound(g.sea.portsKnown.begin(), g.sea.portsKnown.end(), dest), dest);
    g.beginTravelTo(tx, ty);
    ev(g, SeaEv::VoyageEnd, g.pl().p, 1, 0, "JOURNEY'S END");
    tell(g, "THE FERRY TIES UP. JOURNEY'S END");
  }
}
