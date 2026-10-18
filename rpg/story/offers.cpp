// (P1 Fresh Roads, lead; VISION_PLAN 15.34) the one picker of what a teller or a place offers (story.h Offer).
// storyTalk, the tale scan, boards, heralds, ruins, the news and rpg_test --quest-mix all ask here, so the quest-mix
// measurement is the game's own choice. STORY lane: change the policy here, never in the callers.
//
// The P1 policy (STORY lane):
//   - a library tale the world has seen ended is never offered again, by anyone, anywhere (Engine::offerFor /
//     offerForPlace skip it); the survey measured 44-58 % of story offers as such replays.
//   - at most one teller per tale per town: a library tale, and a generated story's archetype, is told in a town by the
//     one person who holds its claim (tellerClaim below; Game::marks MK_TALE_TELLER), so a town is not a choir of people
//     with the same trouble.
//   - a person with no library tale (or whose tale another holds) may have a generated story (saga::offerFor: a weekly
//     roll, raised in P1 so variety comes from the archetypes).
//   - a place (board, herald, ruin, the news) offers its library tale only while it is unplayed (and, for the news, only
//     on news the tale has not answered yet: eventTaleFresh); then it falls through to a generated campaign or story
//     (saga::offerForPlace), so the boards' and ruins' campaigns (FAE, BURDEN) and the news' stories can begin.
//   - (P1b TALES lane) a place's unplayed tales come in the place's own order (a hash of the place and the tale, as a
//     teller's do), so two ruins or two boards offer two different library tales (THE CURSED GIFT, THE RUSTED KNIGHT;
//     THE NAME AMONG THE DEAD, THE UNSIGNED NOTICES) until one is played. The P1b tales (tales2: `family tale`) share
//     the tellers with the older library on the same terms: one teller per tale per town, never told twice.
#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/home.h"
#include "rpg/sim/life.h"
#include "rpg/story/saga.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/economy.h"
#include "rpg/world/ids.h"
#include "rpg/world/source.h"

namespace story {

namespace {
constexpr uint64_t TELLER_DAY_SALT = 0xDA7E5A17ull;
int32_t tellerTag(uint64_t key) { return (int32_t)(((key ^ (key >> 31)) & 0x7FFFFFFEull) | 1ull); }
}  // namespace

bool tellerClaim(Game& g, const Actor& a, int site, uint64_t tale) {
  if (site < 0 || site >= (int)g.world.sites.size()) return true;
  const uint64_t base = ew::mix64(g.world.sites[(size_t)site].id * 0x9E3779B97F4A7C15ull ^ tale);
  const uint64_t kWho = storyMarkKey(base, MK_TALE_TELLER), kDay = storyMarkKey(base ^ TELLER_DAY_SALT, MK_TALE_TELLER);
  const int32_t me = tellerTag(g.npcKey(a));
  auto w = g.marks.find(kWho);
  if (w != g.marks.end() && w->second != me) {
    auto d = g.marks.find(kDay);
    bool held = d != g.marks.end() && g.day - d->second <= TELLER_DAYS;
    // the holder is here and can no longer tell it (dead, or busy with another story of their own): it passes on
    if (held)
      for (const Actor& o : g.actors) {
        if (!o.npc || !o.human || &o == &a || tellerTag(g.npcKey(o)) != w->second) continue;
        if (o.st == AState::Dead || g.marks.count(storyMarkKey(g.npcKey(o), MK_STORY_GIVER))) held = false;
        break;
      }
    if (held) return false;
  }
  g.marks[kWho] = me;
  g.marks[kDay] = g.day;
  return true;
}

uint64_t eventTaleMarkKey(const dsl::Script& s) { return storyMarkKey(strHash64s(s.id), MK_STORY_MARK); }

uint32_t eventTaleFresh(const Game& g, const dsl::Script& s, int site) {
  if (s.hook != dsl::HookKind::Event || site < 0 || site >= (int)g.world.sites.size()) return 0;
  const Site& home = g.world.sites[(size_t)site];
  const int32_t hx = g.world.ox + home.ex, hy = g.world.oy + home.ey;
  uint32_t fresh = 0;
  for (const realm::WorldEvent& e : g.realm.events())
    if ((int)e.type == s.hookEv && rumourReaches(e, g.day, hx, hy) && e.id > fresh) fresh = e.id;
  if (!fresh) return 0;
  auto it = g.marks.find(eventTaleMarkKey(s));
  if (it != g.marks.end() && (uint32_t)it->second >= fresh) return 0;
  return fresh;
}

bool Offer::campaign() const {
  if (lib >= 0) {
    const dsl::Library& L = dsl::library();
    return (size_t)lib < L.scripts.size() && L.scripts[(size_t)lib].tier == 3;
  }
  if (saga.empty()) return false;
  saga::Spec s;
  return saga::parseSpecId(saga, s) && s.campaign;
}

std::string Offer::id() const {
  if (lib >= 0) {
    const dsl::Library& L = dsl::library();
    return (size_t)lib < L.scripts.size() ? L.scripts[(size_t)lib].id : std::string();
  }
  return saga;
}

Offer tellerOffer(Game& g, const Actor& a) {
  Offer o;
  // the library's unplayed tale this person holds the town's claim to (Engine::offerFor)
  const int idx = g.story.offerFor(g, a);
  if (idx >= 0) { o.lib = idx; return o; }
  // else a generated story, unless someone else in town already tells one of its archetype
  const std::string sid = saga::offerFor(g, a);
  if (sid.empty() || !scriptById(sid)) return o;
  saga::Spec s;
  if (saga::parseSpecId(sid, s) && !tellerClaim(g, a, homeSiteOf(g, a), strHash64s((s.campaign ? "saga:@" : "saga:") + s.arch))) return o;
  o.saga = sid;
  return o;
}

Offer placeOffer(Game& g, int hookKind, int site) {
  Offer o;
  // the place's own unplayed library tale (Engine::offerForPlace: never one already ended; the news' only on fresh news)
  const int st = g.story.offerForPlace(g, hookKind, site);
  // a library campaign (THE EMPTY THRONE) first, then a generated campaign that fits here (rare: chain.cpp), then the
  // place's own tale, so a board's or a ruin's tale never keeps FAE or BURDEN from beginning
  if (st >= 0 && dsl::library().scripts[(size_t)st].tier == 3) { o.lib = st; return o; }
  const std::string camp = saga::campaignForPlace(g, hookKind, site);
  if (!camp.empty() && scriptById(camp)) { o.saga = camp; return o; }
  if (st >= 0) { o.lib = st; return o; }
  // else it falls through to a generated story (or a campaign: saga::offerForPlace asks the chainer too)
  const std::string sid = saga::offerForPlace(g, hookKind, site);
  if (!sid.empty() && scriptById(sid)) o.saga = sid;
  return o;
}

// ---------------------------------------------------------------- the library's offers (moved from story.cpp, P1b lead)
using dsl::RoleKind;
// a cheap look before casting: does the world have what the script's roles need (a war, an event)? (P1b: and its
// offer gates hold)
bool offerPrecheck(const Game& g, const dsl::Script& s, int hookSite) {
  if (hookSite < 0 || hookSite >= (int)g.world.sites.size()) return false;
  const Site& home = g.world.sites[(size_t)hookSite];
  const int32_t hx = g.world.ox + home.ex, hy = g.world.oy + home.ey;
  const ew::Gid land = landAt(g, hx, hy);
  for (const dsl::RoleDef& R : s.roles) {
    if ((R.kind == RoleKind::Kingdom || R.kind == RoleKind::Ruler || R.kind == RoleKind::Capital) && !land) return false;
    if (R.kind == RoleKind::Kingdom && !g.realm.kingdom(land)) return false;
    if (R.kind == RoleKind::War) {
      bool any = false;
      for (const realm::War& w : g.realm.wars()) if (!w.endDay && (w.attacker == land || w.defender == land)) any = true;
      if (!any) return false;
    }
    if (R.kind == RoleKind::Event) {
      bool any = false;
      for (const realm::WorldEvent& e : g.realm.events()) if ((int)e.type == R.ev && rumourReaches(e, g.day, hx, hy)) any = true;
      if (!any) return false;
    }
    if (R.kind == RoleKind::Site && R.a == "home" && !home.settlement()) return false;
  }
  return offerGates(g, s, hookSite);
}

// (P1b) the offer gates of a script at a hook settlement: family test never; settle; needs; once site; cooldown
bool offerGates(const Game& g, const dsl::Script& s, int hookSite, std::string* why) {
  auto no = [&](const std::string& w) { if (why) *why = w; return false; };
  if (s.family == "test") return no("a test script");
  if (hookSite < 0 || hookSite >= (int)g.world.sites.size()) return no("no hook place");
  const Site& home = g.world.sites[(size_t)hookSite];
  const int32_t hx = g.world.ox + home.ex, hy = g.world.oy + home.ey;
  if (!s.settle.empty()) {
    bool tierAsked = false, tierOk = false;
    for (const std::string& w : s.settle) {
      if (w == "village" || w == "town" || w == "city" || w == "capital") {
        tierAsked = true;
        if (w == "village" && home.type == SiteType::Village) tierOk = true;
        if (w == "town" && home.type == SiteType::Town) tierOk = true;
        if (w == "city" && home.type == SiteType::City) tierOk = true;
        if (w == "capital" && home.type == SiteType::City && home.capital) tierOk = true;
      } else if (w == "port") {
        if (home.archetype != (uint8_t)ew::Archetype::Port) return no("not a port");
      } else if (w == "coast") {
        if (!nearCoast(g, hx, hy)) return no("not on a coast");
      }
    }
    if (tierAsked && !tierOk) return no("not the settlement it is raised in");
  }
  const ew::Gid land = landAt(g, hx, hy);
  const life::Census* c = home.settlement() ? g.life.find(home.id) : nullptr;
  for (const dsl::NeedDef& n : s.needs) {
    auto arg = [&](size_t i) { return i < n.args.size() ? n.args[i] : std::string(); };
    auto argN = [&](size_t i) { return atoi(arg(i).c_str()); };
    if (n.word == "nowar" || n.word == "war") {
      bool at = false;
      for (const realm::War& w : g.realm.wars()) if (!w.endDay && (w.attacker == land || w.defender == land)) at = true;
      if ((n.word == "war") != at) return no(n.word == "war" ? "no war" : "a war");
    } else if (n.word == "coast") {
      if (!nearCoast(g, hx, hy)) return no("not on a coast");
    } else if (n.word == "season") {
      bool ok = false;
      for (const std::string& a : n.args) if (dsl::seasonWord(a) == (int)home::seasonOf(g.day)) ok = true;
      if (!ok) return no("the wrong season");
    } else if (n.word == "stock") {
      if (!c) return no("no census");
      const int gd = dsl::goodWord(arg(0));
      const int have = gd >= 0 ? (int)c->stock[(size_t)gd] : 0;
      if (arg(1) == "below" ? have >= argN(2) : have <= argN(2)) return no("the stores do not fit");
    } else if (n.word == "mood") {
      const int m = c ? (int)c->mood : 60;
      if (arg(0) == "below" ? m >= argN(1) : m <= argN(1)) return no("the mood does not fit");
    } else if (n.word == "flag") {
      if (!c || !(c->moodFlags & (uint16_t)dsl::moodFlagWord(arg(0)))) return no("the town is not " + arg(0));
    } else if (n.word == "building") {
      const int bt = dsl::buildingWord(arg(0));
      const int want = n.args.size() > 1 ? std::max(1, argN(1)) : 1;
      int have = 0;
      for (int b = home.bldgFirst; b < home.bldgFirst + home.bldgCount && b < (int)g.world.over.bldgs.size(); b++)
        if (b >= 0 && (int)g.world.over.bldgs[(size_t)b].type == bt) have++;
      if (have < want) return no("not enough " + arg(0));
    } else if (n.word == "level") {
      if (g.plLevel < argN(0)) return no("the player is too green");
    }
  }
  if (s.onceSite && g.marks.count(storyMarkKey(ew::mix64(strHash64s(s.id) ^ home.id), MK_P1B_PLAYED))) return no("played here");
  if (s.cooldown > 0) {
    auto it = g.marks.find(storyMarkKey(home.id, MK_P1B_COOLDOWN));
    if (it != g.marks.end() && g.day - it->second < s.cooldown) return no("too soon after the last");
  }
  return true;
}

// (P1b) the sea within reach of a global tile (the composer's N_COAST look: a coarse ring of macro samples)
bool nearCoast(const Game& g, int32_t gx, int32_t gy) {
  if (!g.world.src) return false;
  for (int32_t dy = -360; dy <= 360; dy += 60)
    for (int32_t dx = -360; dx <= 360; dx += 60) {
      if (dx * dx + dy * dy > 360 * 360) continue;
      if (g.world.src->macroFar(gx + dx, gy + dy).elev < ew::ELEV_SEA) return true;
    }
  return false;
}

int Engine::offerFor(Game& g, const Actor& a) {
  if (!a.npc || !a.human || isStoryPerson(a) || a.role == Role::King || a.role == Role::Child || a.role == Role::Herald) return -1;
  const int hs = homeSiteOf(g, a);
  if (hs < 0 || !g.world.sites[(size_t)hs].settlement()) return -1;
  const uint64_t key = g.npcKey(a);
  if (g.marks.count(storyMarkKey(key, MK_STORY_GIVER))) return -1;
  for (const Instance& in : running_) if (!in.cast.empty() && in.cast[0].id == key) return -1;
  // (P1b) a request this person raises comes first: requests are need-driven, not a tale-teller's roll (REQUESTS lane:
  // rpg/story/req/req_offers.cpp decides when a settlement raises one; the loops below skip family request)
  { const int r = requestOfferFor(g, a); if (r >= 0) return r; }
  // not everyone has a tale: innkeepers and priests often, everyone else now and then
  const uint32_t h = hash32((uint32_t)key ^ (uint32_t)(key >> 32) ^ 0x57A1u);
  const int pct = a.role == Role::Innkeeper ? 60 : a.role == Role::Priest ? 50 : a.role == Role::Guard ? 25 : 30;
  if ((int)(h % 100) >= pct) return -1;
  // (P1 Fresh Roads) a tale the world has seen ended is never told again, and a town has one teller per tale (offers.cpp
  // tellerClaim): the unplayed tales of this trade, the person's own order, the first one that fits and is theirs to tell
  const dsl::Library& L = dsl::library();
  std::vector<std::pair<uint32_t, int>> cand;
  for (size_t i = 0; i < L.scripts.size(); i++) {
    const dsl::Script& s = L.scripts[i];
    if (s.hook != dsl::HookKind::Npc || s.family == "request") continue;
    if (s.hookTrade == -2 ? (a.role == Role::Guard || a.role == Role::Jarl) : s.hookTrade != (int)a.role) continue;
    if (timesPlayed(s.id) > 0 && !s.onceSite) continue;   // (P1b: a `once site` script is gated per settlement in precheck)
    bool running = false;
    for (const Instance& in : running_) if (in.script == s.id) running = true;
    if (running) continue;
    cand.push_back({hash32(h ^ (uint32_t)strHash64s(s.id)), (int)i});
  }
  std::sort(cand.begin(), cand.end(), [](const std::pair<uint32_t, int>& x, const std::pair<uint32_t, int>& y) { return x.first > y.first; });
  for (const auto& c : cand) {
    const dsl::Script& s = L.scripts[(size_t)c.second];
    if (!offerPrecheck(g, s, hs)) continue;
    if (!tellerClaim(g, a, hs, strHash64s(s.id))) continue;
    return c.second;
  }
  return -1;
}

int Engine::offerForPlace(Game& g, int hookKind, int site) {
  // (P1b) a request the place raises (a board's, an event's) first (REQUESTS lane)
  { const int r = requestOfferForPlace(g, hookKind, site); if (r >= 0) return r; }
  const dsl::Library& L = dsl::library();
  // (P1b TALES lane) every place has its own order of the unplayed tales that fit it (a hash of the place and the tale),
  // as every teller has (offerFor): two ruins, two boards, offer two different tales until one is played, instead of
  // every ruin in the land pinning the library's first ruin tale (the survey's top tale: 1,119 offers over 20 seeds).
  // A library campaign still comes first (placeOffer: THE EMPTY THRONE is never kept off a place by a tale).
  const uint64_t sid = site >= 0 && site < (int)g.world.sites.size() ? g.world.sites[(size_t)site].id : (uint64_t)(uint32_t)site;
  int best = -1, bestPlays = 1 << 30, bestTier = 0;
  uint32_t bestH = 0;
  for (size_t i = 0; i < L.scripts.size(); i++) {
    const dsl::Script& s = L.scripts[i];
    if ((int)s.hook != hookKind || s.family == "request") continue;
    bool running = false;
    for (const Instance& in : running_) if (in.script == s.id) running = true;
    if (running) continue;
    // (P1 Fresh Roads) every tale, not only a campaign, is told once per world: a played one falls through to a
    // generated story (offers.cpp placeOffer); a news tale waits for news it has not answered yet
    if (timesPlayed(s.id) > 0 && !s.onceSite) continue;
    if (s.hook == dsl::HookKind::Event && !eventTaleFresh(g, s, site)) continue;
    if (!offerPrecheck(g, s, site)) continue;
    const int plays = timesPlayed(s.id);
    const uint32_t h = hash32((uint32_t)sid ^ (uint32_t)(sid >> 32) ^ (uint32_t)strHash64s(s.id) ^ 0x51ACEu);
    const bool better = s.tier != bestTier ? s.tier > bestTier : plays != bestPlays ? plays < bestPlays : h > bestH;
    if (best < 0 || better) { best = (int)i; bestPlays = plays; bestTier = s.tier; bestH = h; }
  }
  return best;
}

}  // namespace story
