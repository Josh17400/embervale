// M12 REALM lane script commands (rpg/view/script_api.h; `embervale --script-help` lists them). Phase A (the lead):
// they put the deep sim in a state before the tick makes it by itself.
//   rise camp            the nearest loaded bandit camp is noted by the realm (a warlord's seed)
//   rise warlord         the nearest noted camp grows into a warlord realm (it takes the nearest village)
//   rise successor       the kingdom whose land the player stands on falls (its towns go to a neighbour), then its
//                        remnant crowns a successor
//   rise resettle [n]    the n-th nearest settlement is abandoned and resettled by the player's land's realm
//   rise ally            the player's land's realm and its nearest neighbour ally
//   rise vassal          its nearest neighbour bends the knee to it
//   rise bossraid [n]    a world boss (a lair noted at the player's spot) raids the n-th nearest settlement
//   rise stats           print the deep sim's counts to stdout
//   (REALM lane, phase B: the tour of what the tick grows by itself)
//   rise warband         the nearest noted camp grows into a warband and raids its village now (WarbandRaid)
//   rise ruin [n]        the n-th nearest settlement is abandoned and ruined now (its Lost History; resettled later)
//   rise goto warlord|successor|resettled|raided|warband   stand at the place of the newest such event (a warlord's
//                        village, a successor's seat, a resettled ruin, a boss-raided town, a warband's prey)
//   rise hear            the player hears every deep-sim event the realm keeps (the News journal lists them)
//   expect rise <what> <n>    at least n: warlords | successors | vassals | alliances | camps | lairs | player
//                             (player: the player's own realm exists, n 0|1) | raids | warbands | resettled | rebuilt |
//                             news (deep-sim events heard)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/story/story.h"
#include "rpg/view/script_api.h"
#include "rpg/world/source.h"

namespace {

std::vector<int> nearestSites(Game& g, bool settlements, SiteType only = SiteType::COUNT) {
  std::vector<std::pair<float, int>> d;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  for (size_t i = 0; i < g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (settlements ? !s.settlement() : s.type != only) continue;
    const float dx = s.ex - px, dy = s.ey - py;
    d.push_back({dx * dx + dy * dy, (int)i});
  }
  std::sort(d.begin(), d.end());
  std::vector<int> out;
  for (auto& e : d) out.push_back(e.second);
  return out;
}
// the deep sim's own events (M12): the news the tour hears
bool riseEvent(realm::EvType t) { return (int)t >= (int)realm::EvType::WarlordRose && t != realm::EvType::COUNT; }

ew::Gid nearestOther(Game& g, ew::Gid notK, int32_t gx, int32_t gy) {
  ew::Gid best = 0;
  double bd = 1e30;
  for (const realm::KingdomState& k : g.realm.kingdoms()) {
    if (k.id == notK || k.fallen || k.player) continue;
    const realm::SettlementState* cap = g.realm.settlement(k.capital);
    if (!cap) continue;
    const double dx = cap->gx - gx, dy = cap->gy - gy, d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = k.id; }
  }
  return best;
}

bool cmdRise(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.world.src) { c.fail("rise: no endless world"); return true; }
  const std::string what = c.arg(1);
  const int32_t gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  g.realm.focusNow(*g.world.src, gx, gy, g.day);
  g.realmNoteSites();
  const ew::Gid land = g.realm.landOwner(g.world.src->kingdomAt(gx, gy), gx, gy);
  const int n = std::max(0, std::atoi(c.arg(2).c_str()));
  if (what == "stats") {
    int warlords = 0, succ = 0, vassals = 0;
    for (const realm::KingdomState& k : g.realm.kingdoms()) {
      if (k.fallen) continue;
      warlords += k.origin == realm::KO_WARLORD; succ += k.origin == realm::KO_SUCCESSOR; vassals += k.overlord != 0;
    }
    printf("rise: day %d, camps %zu, lairs %zu, warlords %d, successors %d, vassals %d, player realm %llu\n", g.realm.day(),
           g.realm.camps().size(), g.realm.lairs().size(), warlords, succ, vassals, (unsigned long long)g.realm.playerRealm());
    fflush(stdout);
    return true;
  }
  if (what == "goto") {
    const std::string kind = c.arg(2);
    realm::EvType want = realm::EvType::COUNT, also = realm::EvType::COUNT;
    if (kind == "warlord") want = realm::EvType::WarlordRose;
    else if (kind == "successor") want = realm::EvType::SuccessorRose;
    else if (kind == "resettled") { want = realm::EvType::Resettled; also = realm::EvType::Rebuilt; }
    else if (kind == "raided") want = realm::EvType::BeastRaid;
    else if (kind == "warband") want = realm::EvType::WarbandRaid;
    else { c.fail("rise goto warlord|successor|resettled|raided|warband"); return true; }
    const realm::WorldEvent* e = nullptr;
    for (const realm::WorldEvent& x : g.realm.events()) if ((x.type == want || x.type == also) && x.site) e = &x;
    if (!e) { c.fail("rise goto " + kind + ": no such event"); return true; }
    int32_t tx = e->gx, ty = e->gy;
    if (const realm::SettlementState* st = g.realm.settlement(e->site)) { tx = st->gx; ty = st->gy; }
    g.teleportGlobal(tx, ty + 7);   // a few steps south of the heart: its roofs and banners in view
    g.realmNoteSites();
    g.realmSync();
    return true;
  }
  if (what == "hear") {
    int n = 0;
    std::vector<realm::WorldEvent> evs(g.realm.events().begin(), g.realm.events().end());
    for (size_t i = evs.size(); i-- > 0 && n < 24;)
      if (riseEvent(evs[i].type) || evs[i].type == realm::EvType::BeastRaid || evs[i].type == realm::EvType::Resettled) { story::hearEvent(g, evs[i], 0); n++; }
    printf("rise hear: %d deep-sim events heard\n", n);
    fflush(stdout);
    return true;
  }
  if (what == "warband") {
    const std::vector<int> camps = nearestSites(g, false, SiteType::BanditCamp);
    for (int si : camps) {
      const Site& s = g.world.sites[(size_t)si];
      g.realm.noteCamp(s.id, g.world.ox + s.ex, g.world.oy + s.ey, g.world.src->kingdomAt(g.world.ox + s.ex, g.world.oy + s.ey), g.day);
    }
    if (camps.empty()) { c.fail("rise warband: no bandit camp loaded"); return true; }
    g.realm.forceWarband(g.world.sites[(size_t)camps[0]].id, g.day);
    g.realmSync();
    return true;
  }
  if (what == "camp" || what == "warlord") {
    const std::vector<int> camps = nearestSites(g, false, SiteType::BanditCamp);
    for (int si : camps) {
      const Site& s = g.world.sites[(size_t)si];
      g.realm.noteCamp(s.id, g.world.ox + s.ex, g.world.oy + s.ey, g.world.src->kingdomAt(g.world.ox + s.ex, g.world.oy + s.ey), g.day);
    }
    if (camps.empty()) { c.fail("rise " + what + ": no bandit camp loaded"); return true; }
    if (what == "warlord") {
      const Site& s = g.world.sites[(size_t)camps[0]];
      if (!g.realm.forceWarlord(s.id, g.day)) { c.fail("rise warlord: the camp found no village to take"); return true; }
    }
    g.realmSync();
    return true;
  }
  if (what == "ally" || what == "vassal" || what == "successor") {
    if (!land || !g.realm.kingdom(land)) { c.fail("rise " + what + ": the player stands in the wildlands"); return true; }
    const ew::Gid other = nearestOther(g, land, gx, gy);
    if (!other) { c.fail("rise " + what + ": no neighbouring kingdom"); return true; }
    if (what == "ally") g.realm.forceAlliance(land, other, g.day);
    else if (what == "vassal") g.realm.forceVassal(other, land, g.day);
    else {
      const realm::KingdomState* K = g.realm.kingdom(land);
      for (ew::Gid s : std::vector<ew::Gid>(K->settlements)) g.realm.forceOwner(s, other, g.day);
      if (!g.realm.forceSuccessor(land, g.day)) { c.fail("rise successor: nothing left to hold"); return true; }
    }
    g.realmSync();
    return true;
  }
  const std::vector<int> near = nearestSites(g, true);
  if ((size_t)n >= near.size()) { c.fail("rise " + what + ": no settlement " + std::to_string(n)); return true; }
  const Site& s = g.world.sites[(size_t)near[(size_t)n]];
  if (what == "resettle") {
    if (!land) { c.fail("rise resettle: the player stands in the wildlands"); return true; }
    g.realm.forceResettle(s.id, land, g.day);
  } else if (what == "ruin") {
    g.realm.forceRuin(s.id, g.day);
  } else if (what == "bossraid") {
    const ew::Gid lair = ew::mix64((uint64_t)gx * 31 + (uint64_t)gy) | 1;
    g.realm.noteLair(lair, gx, gy, 0, 30, g.day);
    g.realm.forceBossRaid(lair, s.id, g.day);
  } else {
    c.fail("rise camp|warlord|warband|successor|resettle|ruin|ally|vassal|bossraid|goto|hear|stats");
    return true;
  }
  g.realmSync();
  return true;
}

bool expRise(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = c.arg(2);
  const int want = std::atoi(c.arg(3).c_str());
  int have = 0;
  for (const realm::KingdomState& k : g.realm.kingdoms()) {
    if (k.fallen) continue;
    if (what == "warlords") have += k.origin == realm::KO_WARLORD;
    else if (what == "successors") have += k.origin == realm::KO_SUCCESSOR;
    else if (what == "vassals") have += k.overlord != 0;
  }
  if (what == "alliances") for (const realm::Relation& r : g.realm.relations()) have += r.state == realm::Rel::Alliance;
  if (what == "camps") have = (int)g.realm.camps().size();
  for (const realm::WorldEvent& e : g.realm.events()) {
    if (what == "raids") have += e.type == realm::EvType::BeastRaid;
    else if (what == "warbands") have += e.type == realm::EvType::WarbandRaid;
    else if (what == "rebuilt") have += e.type == realm::EvType::Rebuilt;
    else if (what == "news") have += e.heard && (riseEvent(e.type) || e.type == realm::EvType::BeastRaid || e.type == realm::EvType::Resettled);
  }
  if (what == "resettled")
    for (const Site& s : g.world.sites)
      if (const realm::SettlementState* st = g.realm.settlement(s.id)) have += (st->flags & realm::SS_RESETTLED) != 0;
  if (what == "lairs") have = (int)g.realm.lairs().size();
  if (what == "player") {
    have = g.realm.playerRealm() != 0;
    if (have != want) c.fail("expect rise player " + std::to_string(want) + ": got " + std::to_string(have));
    return true;
  }
  if (have < want) c.fail("expect rise " + what + " " + std::to_string(want) + ": got " + std::to_string(have));
  return true;
}

}  // namespace

EMB_SCRIPT_CMD("rise", "rise camp|warlord|warband|successor|resettle [n]|ruin [n]|ally|vassal|bossraid [n]|goto <what>|hear|stats: force the deep sim (M12)", cmdRise);
EMB_SCRIPT_CMD("expect:rise", "expect rise warlords|successors|vassals|alliances|camps|lairs|player|raids|warbands|resettled|rebuilt|news <n>", expRise);
