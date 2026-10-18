// P1b "New Roads Content" (VISION_PLAN 15.35): when a settlement raises one of the new requests. REQUESTS lane.
// Engine::offerFor / offerForPlace (offers.cpp) ask here first and never offer a `family request` themselves, so this file
// alone decides how often and by whom a request is raised.
//
// THE RAISE POLICY (req_village.md / req_town.md / req_city.md triggers, judged against the world as it is):
//   - the header words gate first (offerPrecheck: settle tier and coast, needs season / stock..., once site, cooldown: a
//     village raises one new request in 6 days, a city one in 30 to 60 after the last ended there)
//   - then each request's RAISE RULE below reads the settlement's real state: its census (stores, prices, mood flags,
//     festival days, refugees), the realm's fresh news that has reached it (a festival, rising prices, a failed harvest, a
//     broken trade deal, a famine, refugees, war, troops marching), the beasts that den near it (wolves for the bitten
//     flock and the late lantern, bears for the drunken bear), its buildings (a temple, barracks), a harbour (the three
//     harbour-board requests). STRONG: the state the spec names holds (raised on 45 % of the weeks, less where that state
//     is common: the sour well's empty plank store); WEAK: a fallback the story still fits (a few weeks a year, so every
//     request can come up in normal play: the yearly muster for the herald's banner-boy when there is no war); NONE: never.
//   - variety: a request the world has already seen played comes up half as often again, per play (in real play `once
//     site` already keeps it from the settlement it was played in)
//   - ONE request a settlement a week: of the rules that hold and win their week's roll, the settlement raises the one with
//     the highest roll, and only if someone of its trade is there to raise it (and a try-cast of its roles works). The
//     raise is stable through the week, so the person says the same thing every time they are asked.
//   - one teller per request per town (tellerClaim), so a town is not a choir with one trouble.
//   - place hooks (the harbour board's three requests, the capital herald's) follow the same rules on their own roll.
// Measured: rpg_test --quest-mix --seeds 1..20 --p1b (the requests row of --gates-p1b; the report lists offers per request).
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/home.h"
#include "rpg/sim/life.h"
#include "rpg/sim/realm.h"
#include "rpg/story/dsl.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/coords.h"
#include "rpg/world/economy.h"
#include "rpg/world/ids.h"
#include "rpg/world/source.h"

namespace story {

namespace {

enum Strength : int { NONE = 0, WEAK = 1, STRONG = 2 };
// Game::marks tag 176 (the REQUESTS lane's 176..191, story_internal.h): a raise forced for a week, key (site id ^ the
// request id's hash) -> week + 1 (rpg/view/script_life_p1.cpp `raise`, the screenshot tours)
constexpr uint64_t MK_REQ_RAISE = 176;
constexpr int STRONG_PCT = 45;

// ---------------------------------------------------------------- the settlement's state
const life::Census* censusAt(const Game& g, int site) {
  const Site& S = g.world.sites[(size_t)site];
  return S.settlement() ? g.life.find(S.id) : nullptr;
}
int stockOf(const life::Census* c, ew::Good gd) { return c ? (int)c->stock[(size_t)gd] : 0; }
int priceOf(const life::Census* c, ew::Good gd) { return c ? (int)c->price[(size_t)gd] : 100; }
bool flag(const life::Census* c, uint16_t mf) { return c && (c->moodFlags & mf) != 0; }

// news of this kind has reached the settlement and is no older than `days`
bool fresh(const Game& g, int site, realm::EvType t, int days) {
  const Site& S = g.world.sites[(size_t)site];
  const int32_t hx = g.world.ox + S.ex, hy = g.world.oy + S.ey;
  for (const realm::WorldEvent& e : g.realm.events())
    if (e.type == t && g.day - (int)e.day <= days && rumourReaches(e, g.day, hx, hy)) return true;
  return false;
}
bool atWar(const Game& g, int site) {
  const Site& S = g.world.sites[(size_t)site];
  const ew::Gid land = landAt(g, g.world.ox + S.ex, g.world.oy + S.ey);
  if (!land) return false;
  for (const realm::War& w : g.realm.wars()) if (!w.endDay && (w.attacker == land || w.defender == land)) return true;
  return false;
}
// a festival under way or due within `within` days
bool festivalNear(const Game& g, const life::Census* c, int within) {
  if (!c) return false;
  if (c->moodFlags & life::MF_FESTIVAL) return true;
  return c->festivalDay >= g.day && c->festivalDay - g.day <= within;
}
int buildings(const Game& g, int site, art::Building t) {
  const Site& S = g.world.sites[(size_t)site];
  int n = 0;
  for (int b = S.bldgFirst; b < S.bldgFirst + S.bldgCount && b < (int)g.world.over.bldgs.size(); b++)
    if (b >= 0 && (int)g.world.over.bldgs[(size_t)b].type == (int)t) n++;
  return n;
}
// a harbour (the three harbour-board requests: their headers say `coast`, the board and the quay need a port)
bool harbour(const Game& g, int site) { return g.world.sites[(size_t)site].archetype == (uint8_t)ew::Archetype::Port; }
// beasts of these kinds den within ~`tiles` of the settlement (a wilderness den, or a cave they live in): the regions'
// plans, so it reads the land as generated, loaded or not
bool beastsNear(Game& g, int site, std::initializer_list<art::Monster> kinds, int tiles) {
  const Site& S = g.world.sites[(size_t)site];
  const int32_t hx = g.world.ox + S.ex, hy = g.world.oy + S.ey;
  const int64_t r2 = (int64_t)tiles * tiles;
  auto fits = [&](art::Monster m) { for (art::Monster k : kinds) if (k == m) return true; return false; };
  for (int32_t ry = ew::regionOf(hy - tiles); ry <= ew::regionOf(hy + tiles); ry++)
    for (int32_t rx = ew::regionOf(hx - tiles); rx <= ew::regionOf(hx + tiles); rx++) {
      const ew::RegionPlan R = g.world.regionPlan(rx, ry);
      for (const ew::DenPlan& d : R.dens)
        if (fits(d.mon) && (int64_t)(d.x - hx) * (d.x - hx) + (int64_t)(d.y - hy) * (d.y - hy) <= r2) return true;
      for (const ew::SitePlan& p : R.sites)
        if (p.type == SiteType::Cave && fits(p.theme) && (int64_t)(p.ex - hx) * (p.ex - hx) + (int64_t)(p.ey - hy) * (p.ey - hy) <= r2) return true;
    }
  return false;
}
bool wolvesNear(Game& g, int site) { return beastsNear(g, site, {art::Monster::Wolf, art::Monster::IceWolf}, 600); }
bool bearsNear(Game& g, int site) { return beastsNear(g, site, {art::Monster::Bear}, 600); }
// war or a raid has touched the district: a war of the land, the news of a burned or raided place, or a settlement the
// session knows nearby whose census is war-torn or raided
bool warTouched(Game& g, int site) {
  if (atWar(g, site)) return true;
  if (fresh(g, site, realm::EvType::TownBurned, 40) || fresh(g, site, realm::EvType::WarbandRaid, 40) || fresh(g, site, realm::EvType::TownTaken, 40) ||
      fresh(g, site, realm::EvType::Refugees, 40))
    return true;
  const Site& S = g.world.sites[(size_t)site];
  for (const Site& T : g.world.sites) {
    if (!T.settlement() || T.id == S.id) continue;
    const int64_t dx = T.ex - S.ex, dy = T.ey - S.ey;
    if (dx * dx + dy * dy > 400 * 400) continue;
    if (const life::Census* c = g.life.find(T.id)) if (c->moodFlags & (life::MF_WARTORN | life::MF_RAIDED)) return true;
  }
  return false;
}

// ---------------------------------------------------------------- the raise rules
struct Rule {
  const char* id;
  int strongPct;                          // the weekly chance on a STRONG week (0: STRONG_PCT)
  int weakPct;                            // the weekly chance on a WEAK week
  bool noCapital;                         // sends the player to the jarl: never where a king sits
  int (*state)(Game& g, int site, const life::Census* c);
};

// villages (req_village.md triggers)
int stSourwell(Game&, int, const life::Census* c) { return stockOf(c, ew::Good::Planks) <= 4 ? STRONG : WEAK; }
int stRabidfold(Game& g, int s, const life::Census* c) {
  if (!wolvesNear(g, s)) return NONE;
  return stockOf(c, ew::Good::Livestock) >= 6 ? STRONG : WEAK;
}
int stWolfcub(Game& g, int s, const life::Census*) { return wolvesNear(g, s) ? STRONG : NONE; }
int stAshgranary(Game& g, int, const life::Census* c) {
  if (!c) return NONE;
  if (c->lastFestival >= 0 && g.day - c->lastFestival <= 6) return STRONG;   // the feast night the lamp went over
  return WEAK;                                                               // (a saint's night, a wedding: any feast)
}
int stEatenseed(Game&, int, const life::Census* c) {
  if (flag(c, life::MF_HUNGRY | life::MF_FAMINE)) return STRONG;
  return stockOf(c, ew::Good::Grain) <= 6 ? WEAK : NONE;
}
int stDrunkenbear(Game& g, int s, const life::Census* c) {
  if (!bearsNear(g, s)) return NONE;
  if (festivalNear(g, c, 6)) return STRONG;
  return WEAK;
}
int stFarsister(Game& g, int s, const life::Census* c) {
  if (flag(c, life::MF_WARTORN | life::MF_RAIDED) || fresh(g, s, realm::EvType::WarbandRaid, 10)) return NONE;   // a natural death only
  return flag(c, life::MF_GRIEF) ? STRONG : WEAK;
}
int stMovedstone(Game&, int, const life::Census* c) { return flag(c, life::MF_BRAWLS | life::MF_HUNGRY) ? STRONG : WEAK; }
int stLongdrove(Game&, int, const life::Census* c) {
  if (stockOf(c, ew::Good::Livestock) >= 10 || priceOf(c, ew::Good::Livestock) <= 85) return STRONG;
  return stockOf(c, ew::Good::Livestock) >= 4 ? WEAK : NONE;
}
int stTurnipthief(Game& g, int s, const life::Census* c) {
  if (warTouched(g, s)) return STRONG;
  // (villages burn for many reasons: a raid, a dry summer's fire; the lean rows make a few turnips worth the risk)
  return (stockOf(c, ew::Good::Produce) <= 6 || priceOf(c, ew::Good::Produce) > 120) ? WEAK : NONE;
}

// market towns (req_town.md triggers: the event each answers, or the census state behind it)
int stCandles(Game& g, int s, const life::Census* c) { return festivalNear(g, c, 8) || fresh(g, s, realm::EvType::Festival, 20) ? STRONG : NONE; }
int stLedger(Game& g, int s, const life::Census* c) {
  if (fresh(g, s, realm::EvType::PricesRising, 30)) return STRONG;
  const int avg = (priceOf(c, ew::Good::Bread) + priceOf(c, ew::Good::Tools) + priceOf(c, ew::Good::Cloth)) / 3;
  return avg >= 115 ? STRONG : WEAK;
}
int stMeasure(Game& g, int s, const life::Census* c) {
  if (fresh(g, s, realm::EvType::HarvestFailed, 40) || (c && c->harvestPct < 80) || stockOf(c, ew::Good::Grain) <= 4) return STRONG;
  return WEAK;
}
int stPlayers(Game& g, int s, const life::Census*) { return fresh(g, s, realm::EvType::TradeBroken, 40) ? STRONG : WEAK; }
int stWagon(Game& g, int s, const life::Census* c) {
  return fresh(g, s, realm::EvType::Famine, 40) || fresh(g, s, realm::EvType::HarvestFailed, 40) || flag(c, life::MF_HUNGRY | life::MF_FAMINE) ? STRONG : WEAK;
}
int stSaltring(Game& g, int s, const life::Census*) { return fresh(g, s, realm::EvType::TradeDeal, 40) || fresh(g, s, realm::EvType::FirstContact, 40) ? STRONG : WEAK; }
int stPitches(Game& g, int s, const life::Census* c) { return fresh(g, s, realm::EvType::Refugees, 40) || (c && c->refugees > 0) ? STRONG : WEAK; }
int stPig(Game& g, int s, const life::Census* c) { return festivalNear(g, c, 3) || fresh(g, s, realm::EvType::Festival, 14) ? STRONG : NONE; }
int stBell(Game& g, int s, const life::Census*) { return buildings(g, s, art::Building::Temple) > 0 ? WEAK : NONE; }
int stCourier(Game&, int, const life::Census*) { return WEAK; }   // (a ruin near: the try-cast)

// cities and capitals (req_city.md triggers)
int stRowwell(Game& g, int s, const life::Census* c) { return fresh(g, s, realm::EvType::Refugees, 30) || (c && c->refugees > 0) ? STRONG : WEAK; }
int stSilverskin(Game& g, int s, const life::Census*) {
  return fresh(g, s, realm::EvType::PricesRising, 20) || fresh(g, s, realm::EvType::TradeDeal, 20) || fresh(g, s, realm::EvType::Festival, 20) ? STRONG : WEAK;
}
int stBarrels(Game& g, int s, const life::Census*) {
  if (!harbour(g, s)) return NONE;
  return fresh(g, s, realm::EvType::Famine, 20) || fresh(g, s, realm::EvType::HarvestFailed, 20) || fresh(g, s, realm::EvType::BeastRaid, 20) ? STRONG : WEAK;
}
int stLongtable(Game& g, int s, const life::Census*) {
  // a war, or troops marching: STRONG. Otherwise the crown's yearly muster against the border raiders still takes its
  // banner-boys (WEAK): the story speaks of the host marching, never of whose war it is
  return atWar(g, s) || fresh(g, s, realm::EvType::WarDeclared, 30) || fresh(g, s, realm::EvType::TroopsMarching, 20) ? STRONG : WEAK;
}
int stRatguild(Game& g, int s, const life::Census*) {
  return fresh(g, s, realm::EvType::Famine, 25) || fresh(g, s, realm::EvType::HarvestFailed, 25) || fresh(g, s, realm::EvType::TradeDeal, 25) ? STRONG : WEAK;
}
int stCistern(Game& g, int s, const life::Census*) {
  return fresh(g, s, realm::EvType::Famine, 30) || fresh(g, s, realm::EvType::HarvestFailed, 30) || fresh(g, s, realm::EvType::PricesRising, 30) ||
                 home::seasonOf(g.day) == home::Season::Summer
             ? STRONG : WEAK;
}
int stDarklanes(Game& g, int s, const life::Census*) {
  return fresh(g, s, realm::EvType::TroopsMarching, 10) || fresh(g, s, realm::EvType::PricesRising, 10) || home::seasonOf(g.day) == home::Season::Winter ||
                 home::seasonOf(g.day) == home::Season::Autumn
             ? STRONG : WEAK;
}
int stYellowflag(Game& g, int s, const life::Census*) {
  if (!harbour(g, s)) return NONE;
  return fresh(g, s, realm::EvType::PricesRising, 20) || fresh(g, s, realm::EvType::HarvestFailed, 20) || fresh(g, s, realm::EvType::Famine, 20) ? STRONG : WEAK;
}
int stArticles(Game& g, int s, const life::Census*) {
  if (!harbour(g, s)) return NONE;
  return fresh(g, s, realm::EvType::Enlisted, 15) || fresh(g, s, realm::EvType::TroopsMarching, 15) || fresh(g, s, realm::EvType::FirstContact, 15) ? STRONG : WEAK;
}
int stBlueglove(Game& g, int s, const life::Census*) { return buildings(g, s, art::Building::Barracks) > 0 ? WEAK : NONE; }

const Rule kRules[] = {
    // villages
    {"rq_sourwell", 20, 8, false, stSourwell},
    {"rq_rabidfold", 0, 10, false, stRabidfold},
    {"rq_wolfcub", 30, 0, false, stWolfcub},
    {"rq_ashgranary", 0, 8, false, stAshgranary},
    {"rq_eatenseed", 0, 12, false, stEatenseed},
    {"rq_drunkenbear", 0, 10, false, stDrunkenbear},
    {"rq_farsister", 0, 10, false, stFarsister},
    {"rq_movedstone", 0, 10, false, stMovedstone},
    {"rq_longdrove", 25, 10, false, stLongdrove},
    {"rq_turnipthief", 0, 10, false, stTurnipthief},
    // market towns
    {"rq_candles", 0, 0, false, stCandles},
    {"rq_ledger", 0, 10, false, stLedger},
    {"rq_measure", 0, 10, false, stMeasure},
    {"rq_players", 0, 10, false, stPlayers},
    {"rq_wagon", 0, 10, false, stWagon},
    {"rq_saltring", 0, 10, false, stSaltring},
    {"rq_pitches", 0, 10, false, stPitches},
    {"rq_pig", 0, 0, false, stPig},
    {"rq_bell", 0, 12, false, stBell},
    {"rq_courier", 0, 12, false, stCourier},
    // cities and capitals
    {"rq_rowwell", 0, 10, false, stRowwell},
    {"rq_silverskin", 0, 10, false, stSilverskin},
    {"rq_barrels", 0, 14, false, stBarrels},
    {"rq_longtable", 0, 12, false, stLongtable},
    {"rq_ratguild", 0, 12, false, stRatguild},
    {"rq_cisternbook", 0, 10, false, stCistern},
    {"rq_darklanes", 0, 10, false, stDarklanes},
    {"rq_yellowflag", 0, 12, false, stYellowflag},
    {"rq_articles", 0, 14, false, stArticles},
    {"rq_blueglove", 0, 12, false, stBlueglove},
};

const Rule* ruleOf(const std::string& id) {
  for (const Rule& r : kRules) if (id == r.id) return &r;
  return nullptr;
}

// a rule's state at a settlement this week (memoised per world, settlement, week and rule: the beasts and the district's
// war are read off the region plans, and the raise is stable through the week anyway)
int ruleState(Game& g, const Rule& R, int site) {
  thread_local std::map<uint64_t, int8_t> memo;
  const ew::Gid sid = g.world.sites[(size_t)site].id;
  const uint64_t key = ew::mix64(g.world.seed ^ ew::mix64(sid ^ ((uint64_t)(g.day / 7) << 48)) ^ (uint64_t)(&R - kRules) * 0x51ED27ull);
  auto it = memo.find(key);
  if (it != memo.end()) return it->second;
  if (memo.size() > 8192) memo.clear();
  const int st = R.state(g, site, censusAt(g, site));
  memo[key] = (int8_t)st;
  return st;
}

bool running(const Game& g, const std::string& id) {
  for (const Instance& in : g.story.running()) if (in.script == id && !in.done) return true;
  return false;
}

// the week's roll of request `id` at settlement `siteId` (0..99): stable through the week
uint32_t weekRoll(const Game& g, ew::Gid siteId, const std::string& id) {
  const uint64_t h = ew::mix64(siteId * 0x9E3779B97F4A7C15ull ^ strHash64s(id) ^ ((uint64_t)(g.day / 7) << 40));
  return (uint32_t)(h >> 33);
}

// someone of this trade is in the settlement now (an npc hook's raiser)
bool tradeHere(const Game& g, int site, int trade) {
  for (const Actor& a : g.actors) {
    if (!a.npc || !a.human || a.st == AState::Dead || isStoryPerson(a) || a.hostile) continue;
    if ((int)a.role != trade || homeSiteOf(g, a) != site) continue;
    return true;
  }
  return false;
}

// does request `s` pass its gates and win its week at settlement `site`? (its roll, 0 when it does not)
uint32_t raised(Game& g, const dsl::Script& s, int site) {
  if (s.family != "request") return 0;
  if (running(g, s.id)) return 0;
  if (g.story.timesPlayed(s.id) > 0 && !s.onceSite) return 0;
  const Rule* R = ruleOf(s.id);
  if (!R) return 0;
  const Site& S = g.world.sites[(size_t)site];
  if (R->noCapital && S.capital) return 0;
  if (s.hook == dsl::HookKind::Event && !eventTaleFresh(g, s, site)) return 0;
  if (!offerPrecheck(g, s, site)) return 0;
  const int st = ruleState(g, *R, site);
  int pct = st == STRONG ? (R->strongPct ? R->strongPct : STRONG_PCT) : st == WEAK ? R->weakPct : 0;
  // (tours and tests: rpg/view/script_life_p1.cpp `raise <id>`) a raise forced at this settlement for this week
  {
    auto fm = g.marks.find(storyMarkKey(S.id ^ strHash64s(s.id), MK_REQ_RAISE));
    if (fm != g.marks.end() && fm->second == g.day / 7 + 1) return 0xFFFFFFFFu;
  }
  if (pct <= 0) return 0;
  // a new game's first two days belong to its opening (the start village's innkeeper hands out the first job)
  if (g.day < 3) return 0;
  // variety: a trouble the world has already seen played somewhere comes up less often elsewhere (halved per play)
  pct = std::max(1, pct >> std::min(4, g.story.timesPlayed(s.id)));
  const uint32_t roll = weekRoll(g, S.id, s.id);
  if ((int)(roll % 100u) >= pct) return 0;
  return roll | 1u;
}

// the npc-hook request settlement `site` raises this week (library index; -1 none)
int siteRequest(Game& g, int site) {
  const dsl::Library& L = dsl::library();
  int best = -1;
  uint32_t bestRoll = 0;
  for (size_t i = 0; i < L.scripts.size(); i++) {
    const dsl::Script& s = L.scripts[i];
    if (s.family != "request" || s.hook != dsl::HookKind::Npc) continue;
    if (s.hookTrade >= 0 && !tradeHere(g, site, s.hookTrade)) continue;
    const uint32_t r = raised(g, s, site);
    if (r > bestRoll) { bestRoll = r; best = (int)i; }
  }
  return best;
}

}  // namespace

int requestOfferFor(Game& g, const Actor& a) {
  if (!a.npc || !a.human || isStoryPerson(a)) return -1;
  const int hs = homeSiteOf(g, a);
  if (hs < 0 || hs >= (int)g.world.sites.size() || !g.world.sites[(size_t)hs].settlement()) return -1;
  const int idx = siteRequest(g, hs);
  if (idx < 0) return -1;
  const dsl::Script& s = dsl::library().scripts[(size_t)idx];
  if (s.hookTrade == -2 ? !(a.role == Role::Guard || a.role == Role::Jarl) : s.hookTrade != (int)a.role) return -1;
  if (!tellerClaim(g, a, hs, strHash64s(s.id))) return -1;
  // the roles can be cast here (a ruin near, a camp, a town to buy from): an offer the player could not begin is no offer
  std::vector<Binding> cast;
  std::string why;
  if (!castScript(g, s, a.id, hs, offerCastKey(g, s, &a, hs), cast, why)) return -1;
  return idx;
}

int requestOfferForPlace(Game& g, int hookKind, int site) {
  if (site < 0 || site >= (int)g.world.sites.size()) return -1;
  const dsl::Library& L = dsl::library();
  int best = -1;
  uint32_t bestRoll = 0;
  for (size_t i = 0; i < L.scripts.size(); i++) {
    const dsl::Script& s = L.scripts[i];
    if (s.family != "request" || (int)s.hook != hookKind) continue;
    const uint32_t r = raised(g, s, site);
    if (r > bestRoll) { bestRoll = r; best = (int)i; }
  }
  if (best < 0) return -1;
  std::vector<Binding> cast;
  std::string why;
  if (!castScript(g, L.scripts[(size_t)best], -1, site, offerCastKey(g, L.scripts[(size_t)best], nullptr, site), cast, why)) return -1;
  return best;
}

}  // namespace story
