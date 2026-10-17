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
#include <string>
#include "rpg/story/saga.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/ids.h"

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

}  // namespace story
