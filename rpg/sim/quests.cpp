// M2 Wayfinder, SIM lane: radiant quests over the region plans (VISION_PLAN 2.11) and the PLAN.md task 6 quest types,
// with a text grammar (giver mood x reason x reward phrasing x local names) so no two offers read alike.
//
//   Clear / Bounty / Hunt   the M0 jobs, now aimed at real places from the region plans within 768 tiles
//   Deliver      a parcel (ItemKind::Quest) for a named innkeeper, smith or merchant in another settlement 150-700
//                tiles away; destBldg + subject name the recipient, whose dialogue offers HAND OVER THE PARCEL; they pay
//   Heirloom     a named keepsake in an old chest in a named cave or ruin: the chest is placed in that map when the player
//                enters (its tile kept in marks); bring it back to the giver (or, read on a lone grave, lay it there)
//   Missing      a named person alive in a cave: placed in the map on entry, follows the player out as an escort and
//                keeps away from fights; the quest completes once they are outside
//   NamedBandit  a bandit camp whose chief has a name and a title ("TORVALD THE RED"), somewhat stronger
//   Protect      accepted by day; on the night of deadlineDay waves by zone (wolves, goblins or bandits) attack the
//                settlement's fields; hold until dawn; fail if the farmer who asked dies
// Targets beyond the player (danger > level + 3) are offered with "IT IS DANGEROUS COUNTRY" and pay half again.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include "engine/sound_ids.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/jobs_ops.h"
#include "rpg/world/source.h"
#include "rpg/world/wildlife.h"
#include "rpg/sim/life_hooks.h"
#include "rpg/sim/travel_ops.h"
#include "rpg/sim/quest_ops.h"
#include "rpg/sim/game.h"

using art::Monster;
using art::Prop;

namespace {
constexpr int kRadiantRange = 768;   // tiles: a long day's walk (VISION_PLAN 2.11)
}  // namespace

// (SOLID 3.5) the quest-text helpers quest_offers.cpp shares: it declares them in its "from quests.cpp" block
namespace qtext {
// the person behind a quest (a Deliver recipient, a missing person, a named chief): look and name come from the
// quest's 16-bit person seed (Quest::flags bits 16-31), so they are the same in every session
Rng personRng(const Quest& q, uint64_t worldSeed) {
  return Rng(((uint64_t)hash32(((q.flags >> 16) & 0xFFFFu) * 2654435761u ^ (uint32_t)worldSeed ^ 0x9E57u) << 8) ^ ((uint64_t)q.type << 40) ^ 0x51u);
}
bool personFemale(const Quest& q, uint64_t worldSeed) { Rng r = personRng(q, worldSeed); return r.f() < 0.45f; }   // makeLook's first draw
// (M7b integration) a place named for what it is ("TOMB OF SHENSHA", "CAVES OF IRON") is spoken with its article:
// "WENT INTO THE TOMB OF SHENSHA", and once named it is "THE PLACE"; a proper name ("SESAI CAVERN") is not
std::string spokenPlace(const std::string& n) {
  if (n.empty() || n.rfind("THE ", 0) == 0 || n.find(" OF ") == std::string::npos) return n;
  return "THE " + n;
}
const char* roleWord(Role r) {
  switch (r) {
    case Role::Innkeeper: return "INNKEEPER";
    case Role::Smith: return "SMITH";
    case Role::Merchant: return "MERCHANT";
    default: return "GOOD FOLK";
  }
}
art::Building roleBuilding(Role r) {
  switch (r) {
    case Role::Smith: return art::Building::Smithy;
    case Role::Merchant: return art::Building::Shop;
    default: return art::Building::Inn;
  }
}
// Protect: who comes for the fields (0 wolves, 1 goblins, 2 bandits), from the quest's person seed and the land
int protectZone(const Quest& q, Biome b) {
  int z = (int)(((q.flags >> 16) & 0xFFFFu) % 3u);
  if (b == Biome::Desert && z == 0) z = 1;
  return z;
}
Monster zoneMonster(int z, Biome b) {
  if (z == 1) return Monster::Goblin;
  return (b == Biome::Snow || b == Biome::Taiga) ? Monster::IceWolf : Monster::Wolf;
}
const char* zonePlural(int z, Biome b) { return z == 2 ? "BANDITS" : monsterPlural(zoneMonster(z, b)); }
}  // namespace qtext
using namespace qtext;

namespace {
art::Icon keepsakeIcon(const std::string& s) {
  if (s.find("RING") != std::string::npos || s.find("BAND") != std::string::npos) return art::Icon::Ring;
  if (s.find("LOCKET") != std::string::npos || s.find("PENDANT") != std::string::npos || s.find("BEADS") != std::string::npos ||
      s.find("MEDAL") != std::string::npos) return art::Icon::Amulet;
  if (s.find("SWORD") != std::string::npos) return art::Icon::Sword;
  if (s.find("KNIFE") != std::string::npos) return art::Icon::Dagger;
  if (s.find("HORN") != std::string::npos) return art::Icon::Bone;
  return art::Icon::Gem;
}
}  // namespace

// the nearest-ish unfinished place of a type within kRadiantRange tiles of a global tile, from the region plans (no
// tiles are generated); places beyond the player (danger > level + 3) only when nothing else fits (danger = true)
int QuestOps::pickRadiant(Game& g, SiteType t, int32_t gx, int32_t gy, Rng& r, bool& danger, int exclude) {
  danger = false;
  if (!g.world.endless || !g.world.src) return -1;
  const ew::Gid exId = exclude >= 0 && exclude < (int)g.world.sites.size() ? g.world.sites[(size_t)exclude].id : 0;
  const int rings = (kRadiantRange + ew::REGION - 1) / ew::REGION;
  const int32_t rx0 = ew::regionOf(gx), ry0 = ew::regionOf(gy);
  struct Cand { ew::Gid id; float score; bool fit; };
  std::vector<Cand> c;
  for (int32_t ry = ry0 - rings; ry <= ry0 + rings; ry++)
    for (int32_t rx = rx0 - rings; rx <= rx0 + rings; rx++) {
      const ew::RegionPlan R = g.world.regionPlan(rx, ry);
      for (const ew::SitePlan& p : R.sites) {
        if (p.type != t || (p.flags & ew::SPF_MAINQUEST) || p.id == exId) continue;
        const float d = std::hypot((float)(p.ex - gx), (float)(p.ey - gy));
        if (d > (float)kRadiantRange) continue;
        const int h = g.world.siteHandle(p.id);
        if (h >= 0 && g.world.sites[(size_t)h].cleared) continue;
        // not a place another open job points at, nor one a finished job already sent the player to
        bool taken = false;
        for (const Quest& o : g.quests) {
          if (o.type == QType::Main) continue;
          const ew::Gid oid = o.targetId ? o.targetId : (o.target >= 0 && o.target < (int)g.world.sites.size() ? g.world.sites[(size_t)o.target].id : 0);
          if (oid == p.id) taken = true;
        }
        if (taken) continue;
        c.push_back({p.id, d * (0.8f + r.f() * 0.5f), p.level <= g.plLevel + 3});
      }
    }
  const Cand* best = nullptr;
  for (const Cand& k : c) if (k.fit && (!best || k.score < best->score)) best = &k;
  if (!best) {
    for (const Cand& k : c) if (!best || k.score < best->score) best = &k;
    danger = best != nullptr;
  }
  return best ? g.world.ensureSite(best->id) : -1;
}

// Deliver: a settlement 150-700 tiles away on the same landmass (one of the nearer few, by the offer's dice)
int QuestOps::pickDestination(Game& g, int32_t gx, int32_t gy, Rng& r) {
  if (!g.world.endless || !g.world.src) return -1;
  const int rings = 3;
  const int32_t rx0 = ew::regionOf(gx), ry0 = ew::regionOf(gy);
  const int32_t land = TravelOps::landAt(g, gx, gy);
  std::vector<std::pair<float, ew::Gid>> c;
  for (int32_t ry = ry0 - rings; ry <= ry0 + rings; ry++)
    for (int32_t rx = rx0 - rings; rx <= rx0 + rings; rx++) {
      const ew::RegionPlan R = g.world.regionPlan(rx, ry);
      for (const ew::SitePlan& p : R.sites) {
        if (p.type != SiteType::Village && p.type != SiteType::Town && p.type != SiteType::City) continue;
        const float d = std::hypot((float)(p.ex - gx), (float)(p.ey - gy));
        if (d < 150.0f || d > 700.0f) continue;
        bool taken = false;
        for (const Quest& o : g.quests) if (o.type == QType::Deliver && o.state != QState::Done && o.targetId == p.id) taken = true;
        if (taken) continue;
        if (land && TravelOps::landAt(g, p.ex, p.ey) != land) continue;
        c.push_back({d, p.id});
      }
    }
  if (c.empty()) return -1;
  std::sort(c.begin(), c.end());
  const int n = std::min<int>(5, (int)c.size());
  return g.world.ensureSite(c[(size_t)r.irange(n)].second);
}


bool Game::debugOfferTalk(int actorId, QType t) {
  const int k = findActor(actorId);
  if (k < 0) return false;
  bool ok = false;
  Quest q = offerFor(actors[(size_t)k], t, ok);
  if (!ok) return false;
  pendingOffer_ = q;
  dlg = Dialogue();
  dlg.actor = actorId;
  dlg.speaker = actors[(size_t)k].name;
  dlg.role = actors[(size_t)k].role;
  dlg.text = pendingPitch_.empty() ? q.desc : pendingPitch_;
  {   // (fixer M7b r3) a giver met for the first time says who they are before the pitch, so the panel's title is
      // "TERAALAI, HERDER" from the first page, not "THE HERDER" and then a name on accepting
    const Actor& ga = actors[(size_t)k];
    const bool named = ga.npc && ga.human && ga.role != Role::Guard && ga.role != Role::Soldier && ga.role != Role::Herald && ga.role != Role::Bandit &&
                       ga.role != Role::King && !ga.name.empty() && ga.name.find("GUARD") == std::string::npos;
    const uint64_t key = named ? npcKey(ga) : 0;
    if (key && !talkMem.introduced(key)) {
      const std::string t = speech::joinFit("I'M " + speech::givenName(ga.name) + ".", dlg.text);
      if (t != dlg.text) { dlg.text = t; talkMem.flag(key, speech::TM_INTRODUCED, day); }
    }
  }
  dlg.opts = {{"I'LL DO IT.", A_ACCEPT, 0}, {"NOT RIGHT NOW.", A_DECLINE, 0}};
  mode = Mode::Dialogue;
  return true;
}

// M5 (CITIZENS lane): the giver's own words for an offer from life_quests.cpp
std::string lifePitch(const Game& g, const Quest& q);

// (P1 Fresh Roads, JOBS lane) does this NPC have work to offer? An unmet need first (life_quests.cpp), then the
// settlement's weekly pool (rpg/sim/jobs_pool.cpp: worked out from the land's real needs the first time anyone there is
// asked this week, frozen, each job named to a resident of a fitting trade). No lifetime caps, no 1-in-3 hash: a
// fresh week is a fresh pool. Outside the settlements (a wayside hunter, a hermit) one job a week. Runs every frame
// (the talk prompt, the markers): the pool is read through an O(1) index (jobs::Book::live).
bool Game::hasOffer(const Actor& a) const {
  const bool noble = background == Background::Noble;   // exiled noble: guards and jarls offer more work
  if (!a.npc || (a.role == Role::Guard && !noble) || a.role == Role::Child || a.role == Role::King || a.site < 0) return false;
  if (!a.human || a.quest > 0 || a.role == Role::Traveller) return false;   // M2: a troll, a missing person, a traveller
  if (a.site >= (int)world.sites.size()) return false;
  for (auto& q : quests) if (q.state != QState::Done && q.type != QType::Main && isGiver(q, a)) return false;
  { Quest lq; if (LifeHooks::offer(*this, a, lq)) return true; }   // M5: an unmet need asks for help (life_quests.cpp)
  const Site& S = world.sites[(size_t)a.site];
  if (S.settlement() && world.endless) {
    if (!life.find(S.id)) return false;   // (its census is a moment away: the pool is worked out from it)
    const jobs::PoolJob* j = JobOps::openJob(*this, a);
    if (!j) return false;
    // a night on the fields is asked for by day, out of doors (offerFor's Protect rule)
    if (j->qtype == (uint8_t)QType::Protect && (hour < 6.0f || hour >= 17.0f || inside)) return false;
    return true;
  }
  // no census (the wilds, a wayside place): one job a week; the wayside trades always have one, others every other week
  const uint64_t key = npcKey(a);
  if (!JobOps::waysideFree(*this, key)) return false;
  if (a.role == Role::Hunter || a.role == Role::Fisher || a.role == Role::Herbalist || a.role == Role::Innkeeper || a.role == Role::Jarl) return true;
  return hash32((uint32_t)key ^ (uint32_t)seed ^ (uint32_t)(day / 7) * 2654435761u) % 2 == 0;
}

Quest QuestOps::makeOffer(Game& g, const Actor& npc, bool* okOut) {
  if (okOut) *okOut = true;
  { Quest lq; if (LifeHooks::offer(g, npc, lq)) { g.pendingPitch_ = lifePitch(g, lq); return lq; } }   // M5: an unmet need asks for help first (life_quests.cpp)
  bool ok = false;
  // (P1) the person's job this week: its kind and its need (offerFor reads jobBook.offerNeed while it builds)
  if (const jobs::PoolJob* j = JobOps::openJob(g, npc)) {
    g.jobBook.offerNeed = j->need;
    const QType want = (QType)j->qtype;
    // (P1b) a radiant job (QType::Story) is begun by "ANY WORK GOING?" itself (radiant::offerFor); here only when it
    // could not be composed or cast in this world: the fallback hunt below
    Quest q = want == QType::Story ? Quest() : g.offerFor(npc, want, ok);
    if (!ok && JobOps::censusJob(g, npc) != life::Job::Scholar) {   // the place it named is gone (cleared since Monday, taken by another job): a hunt at a den always fits (not a scholar's)
      g.jobBook.offerNeed = jobs::JN_DEN;
      q = g.offerFor(npc, QType::Hunt, ok);
    }
    g.jobBook.offerNeed = jobs::JN_NONE;
    if (ok) return q;
  }
  Quest q = g.offerFor(npc, QType::COUNT, ok);
  if (okOut) *okOut = ok;
  return q;
}

// ---------------------------------------------------------------- markers and journal lines
bool QuestOps::questTargetM2(const Game& g, const Quest& q, int& tx, int& ty) {
  if (q.state != QState::Active) return false;
  auto global = [&](int32_t gx, int32_t gy) { tx = gx - g.world.ox; ty = gy - g.world.oy; return true; };
  auto atSite = [&]() {
    if (q.target >= 0 && q.target < (int)g.world.sites.size()) { tx = g.world.sites[(size_t)q.target].ex; ty = g.world.sites[(size_t)q.target].ey; return true; }
    return q.hasPos ? global(q.tgx, q.tgy) : false;
  };
  switch (q.type) {
    case QType::Deliver:
      if (q.destBldg >= 0 && q.destBldg < (int)g.world.over.bldgs.size()) {
        tx = g.world.over.bldgs[(size_t)q.destBldg].doorX(); ty = g.world.over.bldgs[(size_t)q.destBldg].doorY();
        return true;
      }
      return atSite();
    case QType::Heirloom: case QType::NamedBandit: return atSite();
    case QType::Missing: return atSite();
    case QType::Protect: return q.hasPos ? global(q.tgx, q.tgy) : atSite();
    case QType::Story: return q.hasPos ? global(q.tgx, q.tgy) : false;   // M4: the story engine's marker (rpg/story)
    case QType::War: return q.hasPos ? global(q.tgx, q.tgy) : atSite();   // M4: the enemy camp (relief) or the town (assault)
    case QType::Supply: return (q.flags & life::QF_SUP_GOODS) ? false : atSite();   // M5: where the goods are to be had
    case QType::Sea: return q.hasPos ? global(q.tgx, q.tgy) : false;   // M10: the chart chain's place, the king's capital
    case QType::Campaign: case QType::Envoy: case QType::Fief:   // M12: the order's / court's / holding's place
      return q.hasPos ? global(q.tgx, q.tgy) : atSite();
    case QType::Errand: return q.hasPos ? global(q.tgx, q.tgy) : false;   // P1: the street incident's place (LIFE lane)
    case QType::Hunt: {   // P1: the den or the hunting ground; a quarry in sight first (out of doors, within 20 tiles)
      if (!q.hasPos) return false;   // (a hunt from before P1: questTarget looks for the quarry and the dens itself)
      if (!g.inside) {
        float bd = (20.0f * TILE) * (20.0f * TILE);
        bool seen = false;
        for (size_t i = 1; i < g.actors.size(); i++) {
          const Actor& m = g.actors[i];
          if (m.st == AState::Dead || m.human || m.npc || m.mon != q.mon) continue;
          // (P1 fixer) never the hero's own summon, a charmed or raised beast (a calmed or held wild one is still quarry)
          if (m.faction == Faction::Player || m.magicTag == magic::MT_SUMMON || m.magicTag == magic::MT_CHARM || m.magicTag == magic::MT_RAISE || m.magicTag == magic::MT_BOUND) continue;
          const float d = len2(m.p - g.pl().p);
          if (d < bd) { bd = d; tx = (int)std::floor(m.p.x / TILE); ty = (int)std::floor(m.p.y / TILE); seen = true; }
        }
        if (seen) return true;
      }
      return global(q.tgx, q.tgy);
    }
    default: return false;
  }
}

std::string QuestOps::questStatusM2(const Game& g, const Quest& q) {
  if (q.state == QState::Done) return "";
  int px, py;
  overworldTile(g, px, py);
  int tx = 0, ty = 0;
  // (fixer M6b r3) a story's step names the direction of the live person it waits on (Game::questTarget)
  const bool has = q.type == QType::Story ? g.questTarget(q.id, tx, ty) : QuestOps::questTargetM2(g, q, tx, ty);
  const std::string dl = has ? " (" + dirWord(tx - px, ty - py, false) + ")" : std::string(), ds = has ? " (" + dirWord(tx - px, ty - py, true) + ")" : std::string();
  const std::string tname = q.target >= 0 && q.target < (int)g.world.sites.size() ? g.world.sites[(size_t)q.target].name : std::string();
  if (q.state == QState::Complete && (q.flags & QF_GRAVE)) {
    int gx = 0, gy = 0;
    if (q.giverSite >= 0 && q.giverSite < (int)g.world.sites.size()) { gx = g.world.sites[(size_t)q.giverSite].ex; gy = g.world.sites[(size_t)q.giverSite].ey; }
    return fitLine({"LAY IT ON THE LONE GRAVE (" + dirWord(gx - px, gy - py, false) + ")", "LAY IT ON THE GRAVE (" + dirWord(gx - px, gy - py, true) + ")"});
  }
  if (q.state != QState::Active) return "";
  switch (q.type) {
    case QType::Deliver:
      return fitLine({"BRING THE PARCEL TO " + q.subject + " IN " + tname + dl, "PARCEL FOR " + q.subject + " IN " + tname + ds,
                      "PARCEL: " + q.subject + ds, "PARCEL TO " + tname + ds});
    case QType::Heirloom:
      { const size_t ap = q.subject.find("'S "); const std::string item = ap == std::string::npos ? q.subject : q.subject.substr(ap + 3);
        return fitLine({"FIND THE " + item + " IN " + tname + dl, "FIND THE " + item + " IN " + tname + ds, "FIND IT IN " + tname + ds, tname + ds}); }
    case QType::Missing:
      if (q.flags & QF_FOUND) return fitLine({"LEAD " + q.subject + " OUT OF " + tname, "LEAD " + q.subject + " OUT", "LEAD THEM OUT"});
      return fitLine({"FIND " + q.subject + " IN " + tname + dl, "FIND " + q.subject + " IN " + tname + ds, "FIND " + q.subject + ds, tname + ds});
    case QType::NamedBandit:
      return fitLine({"KILL " + q.subject + " AT " + tname + dl, "KILL " + q.subject + ds, "KILL " + q.subject, tname + ds});
    case QType::Story:   // M4: the story engine writes what to do next in subject ("SPEAK WITH ASTRID", "2/4 WOLVES")
      return q.subject.empty() ? std::string() : fitLine({q.subject + dl, q.subject + ds, q.subject});
    case QType::Supply:   // M5: fetch the goods (then the classic RETURN TO line)
      if (q.flags & life::QF_SUP_GOODS) return "";
      return fitLine({"FETCH " + std::to_string(q.need) + " " + q.subject + " FROM " + tname + dl, "FETCH " + q.subject + " FROM " + tname + ds,
                      q.subject + ": " + tname + ds, tname + ds});
    case QType::Sea:   // M10 (sea_chain.cpp): the chain's step / the word to the king in subject; a fishmonger's order
      if (q.stage == 3) return fitLine({"CATCH " + std::to_string(q.need) + " " + q.subject + " (" + std::to_string(q.have) + "/" + std::to_string(q.need) + ")",
                                        q.subject + " " + std::to_string(q.have) + "/" + std::to_string(q.need)});
      return q.subject.empty() ? std::string() : fitLine({q.subject + dl, q.subject + ds, q.subject});
    case QType::Campaign: case QType::Envoy: case QType::Fief:   // M12 (service_game.cpp / lord_game.cpp write subject)
      return q.subject.empty() ? std::string() : fitLine({q.subject + dl, q.subject + ds, q.subject});
    case QType::Errand: {   // P1 (LIFE lane writes subject: "CATCH THE THIEF"); without one, the kind says it
      static const char* const byKind[] = {"CATCH THE THIEF", "MEND THE CART", "BREAK UP THE BRAWL", "BRING THEM BACK", "LEND A HAND"};
      const std::string what = !q.subject.empty() ? q.subject : byKind[std::clamp(q.stage, 0, (int)jobs::ErrandKind::COUNT - 1)];
      return fitLine({what + dl, what + ds, what});
    }
    case QType::Hunt: {   // P1: "THIN THE WOLVES BY THE GREY TEETH 2/5 (NORTH-EAST)"
      if (!q.hasPos) return "";
      const std::string m = monsterPlural(q.mon), prog = std::to_string(q.have) + "/" + std::to_string(q.need);
      const std::string area = !q.subject.empty() ? " BY " + spokenPlace(q.subject) : q.targetId ? std::string(" AT THEIR DEN") : std::string(" IN THE WILDS");
      return fitLine({"THIN THE " + m + area + " " + prog + dl, "THIN THE " + m + area + " " + prog + ds, "THIN THE " + m + " " + prog + ds, m + " " + prog + ds,
                      m + " " + prog});
    }
    case QType::Protect: {
      const bool night = (g.day == q.deadlineDay && g.hour >= 20.5f) || (g.day == q.deadlineDay + 1 && g.hour < 5.5f);
      const std::string home = giverTown(g.world, q);
      if (night) return fitLine({"HOLD THE FIELDS UNTIL DAWN" + ds, "HOLD THE FIELDS UNTIL DAWN"});
      return fitLine({"AT NIGHTFALL: THE FIELDS OF " + home + ds, "AT NIGHTFALL: THE FIELDS" + ds, "GUARD THE FIELDS AT NIGHT"});
    }
    default: return "";
  }
}

namespace {
// (M7b) where a quest's place lies as its giver says it: "SESAI CAVERN, NORTH-WEST OF HERE" ("" without a place)
std::string placeFromGiver(const Game& g, const Quest& q) {
  if (q.target < 0 || q.target >= (int)g.world.sites.size()) return "";
  const Site& T = g.world.sites[(size_t)q.target];
  if (q.giverSite < 0 || q.giverSite >= (int)g.world.sites.size() || q.giverSite == q.target) return spokenPlace(T.name);
  const Site& H = g.world.sites[(size_t)q.giverSite];
  return spokenPlace(T.name) + ", " + dirWord(T.ex - H.ex, T.ey - H.ey, false) + " OF HERE";
}
// the keepsake of a heirloom ("GRANDMOTHER'S SILVER RING" -> "SILVER RING")
std::string keepsakeOf(const std::string& subject) {
  const size_t ap = subject.find("'S ");
  return ap == std::string::npos ? subject : subject.substr(ap + 3);
}
}  // namespace

std::string QuestOps::turnInLine(const Game& gm, const Quest& q) {
  const std::string g = std::to_string(q.gold);
  const std::string tname = q.target >= 0 && q.target < (int)gm.world.sites.size() ? spokenPlace(gm.world.sites[(size_t)q.target].name) : std::string();
  const bool fem = personFemale(q, gm.seed);
  switch (q.type) {
    case QType::Heirloom:
      if (q.subject.rfind("THE TEMPLE'S ", 0) == 0)   // (P1) a temple's relic
        return "THE " + keepsakeOf(q.subject) + ", BACK WHERE IT BELONGS! THE GODS WILL REMEMBER THIS. " + g + " GOLD FROM THE OFFERINGS, AND OUR BLESSING.";
      if (q.subject.rfind("THE CARAVAN'S ", 0) == 0)   // (P1) a merchant's stolen goods
        return "MY " + keepsakeOf(q.subject) + ", AND NOT A SEAL BROKEN! THOSE THIEVES WILL GET NOTHING FROM ME. " + g + " GOLD, AS PROMISED.";
      return "MY " + q.subject + "! I NEVER THOUGHT I WOULD HOLD IT AGAIN. I PROMISED YOU " + g + " GOLD, AND YOU HAVE MY BLESSING TOO.";
    case QType::Missing:
      return q.subject + " IS HOME, AND SAFE! I DON'T KNOW HOW TO THANK YOU. " + std::string(fem ? "SHE" : "HE") + " TOLD US WHAT YOU DID. THE " + g +
             " GOLD IS YOURS.";
    case QType::NamedBandit: return "SO " + q.subject + " IS DEAD. THE ROADS WILL BREATHE EASIER NOW. THE BOUNTY IS " + g + " GOLD, AND YOU'VE EARNED IT.";
    case QType::Protect: return "WE SAW THE FIELDS AT DAWN, STILL STANDING. YOU HELD THE NIGHT FOR US. " + g + " GOLD, AS I PROMISED.";
    case QType::Hunt:
      return std::string("YOU'RE BACK, AND THE ") + monsterPlural(q.mon) + " ARE THINNED OUT. WE CAN SLEEP AGAIN. I OWE YOU " + g + " GOLD.";
    case QType::Supply: return goodUnits(q.need, q.subject) + "! NOW THE WORK CAN GO ON. THE WHOLE TOWN WILL THANK YOU. I OWE YOU " + g + " GOLD.";
    case QType::Bounty:
      return tname.empty() ? "YOU'RE BACK, AND THE BANDITS' CHIEF IS DEAD. THE BOUNTY IS " + g + " GOLD."
                           : "THEY SAY THE BANDITS OF " + tname + " HAVE SCATTERED, AND THEIR CHIEF IS DEAD. THE BOUNTY IS " + g + " GOLD.";
    case QType::War: return "YOU FOUGHT WELL OUT THERE. THE CAPTAINS ALL SAY IT. YOUR PAY IS " + g + " GOLD.";
    case QType::Sea: return q.stage == 3 ? "FINE FISH, AND FRESH! " + g + " GOLD, AS AGREED." : "YOU'VE DONE A GREAT THING. " + g + " GOLD, AND MY THANKS.";
    case QType::Campaign: {   // M12 (SERVICE lane): the army's thanks for the order's kind
      const service::Order* o = nullptr;
      for (const service::Order& x : gm.service.orders) if ((int)x.id == q.stage) o = &x;
      static const char* by[] = {"ORDERS CARRIED OUT. THE CAPTAINS WILL HEAR OF IT.", "THE SIEGE IS BROKEN AND OUR PEOPLE EAT AGAIN.",
                                 "THE GATE FELL BECAUSE YOU WENT IN FIRST.", "THE CARTS GOT THROUGH. THE GARRISON WILL HOLD.",
                                 "A GOOD REPORT, AND YOU CAME BACK ALIVE TO GIVE IT.", "THEIR CAPTAIN IS DEAD, AND HIS MEN ALREADY WAVER.",
                                 "THOSE FAMILIES ARE ALIVE BECAUSE OF YOU.", "THE FRONTIER HELD THE NIGHT, AND YOU HELD IT WITH THEM."};
      return std::string(by[o ? std::clamp((int)o->kind, 0, 7) : 0]) + " " + g + " GOLD FROM THE WAR CHEST.";
    }
    case QType::Envoy: {   // M12 (SERVICE lane): the court's thanks, by what its word won
      const service::Mission* m = nullptr;
      for (const service::Mission& x : gm.service.missions) if ((int)x.id == q.stage) m = &x;
      static const char* by[] = {"THEY REFUSED, BUT YOU CARRIED OUR WORD WITH HONOUR.", "PEACE. YOU CARRIED IT, AND IT HELD.",
                                 "AN ALLIANCE! WE WILL NOT STAND ALONE NOW.", "THEY WILL PAY. YOU SPOKE FOR US WELL."};
      if (m && m->kind == service::MissionKind::FirstEnvoy) return "THE COURT BEYOND THE MIST HAS OUR GREETING. A NEW WORLD OPENS. " + g + " GOLD, AND OUR THANKS.";
      return std::string(by[m ? std::clamp((int)m->outcome, 0, 3) : 0]) + " " + g + " GOLD, AND THE COURT'S THANKS.";
    }
    case QType::Fief: return "THE VILLAGE WON'T FORGET THIS, MY LORD. " + g + " GOLD FROM THE COFFERS.";
    case QType::Errand: {   // P1: a street incident's thanks, by its kind (jobs.h ErrandKind)
      static const char* const byKind[] = {"MY PURSE! I THOUGHT I'D NEVER SEE IT AGAIN.", "THE WHEEL HOLDS! I'D HAVE BEEN STUCK HERE TILL DARK.",
                                           "THANK THE GODS SOMEBODY STEPPED IN BEFORE THE WATCH CAME.", "HOME SAFE! I DON'T KNOW WHAT I'D HAVE DONE.",
                                           "THAT WAS KIND OF YOU, STRANGER."};
      return std::string(byKind[std::clamp(q.stage, 0, (int)jobs::ErrandKind::COUNT - 1)]) + " TAKE THESE " + g + " GOLD, AND MY THANKS.";
    }
    default:
      return (tname.empty() ? std::string("YOU'RE BACK, AND IT'S DONE.") : "YOU'RE BACK! THEY SAY " + tname + " HAS GONE QUIET AT LAST.") + " I OWE YOU " + g +
             " GOLD.";
  }
}

// M7b: what a giver says while their job is under way: what is still to do and where, in the first person (never the
// journal's q.desc read aloud, which names the giver in the third person)
std::string QuestOps::questReminder(const Game& g, const Quest& q) {
  const std::string place = placeFromGiver(g, q);
  const std::string home = giverTown(g.world, q);
  const bool fem = personFemale(q, g.seed);
  const std::string obj = fem ? "HER" : "HIM", subj = fem ? "SHE" : "HE";
  const uint32_t h = hash32((uint32_t)q.id * 2654435761u ^ (uint32_t)g.day);
  switch (q.type) {
    case QType::Hunt: {
      const int left = std::max(0, q.need - q.have);
      const std::string m = monsterPlural(q.mon);
      const std::string by = q.hasPos && !q.subject.empty() ? "OUT BY " + spokenPlace(q.subject) : "THE WILDS ROUND " + home;   // (P1) the hunt's ground
      if (q.have <= 0)
        return h % 2 ? "NO LUCK WITH THE " + m + " YET? THEY ROAM " + by + ". I NEED " + speech::countWord(q.need) + " OF THEM DEAD."
                     : "THE " + m + " ARE STILL OUT THERE, BOLDER EVERY NIGHT. KILL " + speech::countWord(q.need) + " OF THEM AND I'LL PAY WHAT I PROMISED.";
      return "HOW GOES THE HUNT? THAT'S " + speech::countWord(q.have) + " DOWN, SO " + speech::countWord(left) + " MORE " + m + " TO GO.";
    }
    case QType::Clear:
      return place.empty() ? "IT'S STILL OUT THERE. KILL WHATEVER LEADS THEM, THEN COME BACK TO ME."
                           : "NOTHING'S CHANGED AT " + place + ". KILL WHATEVER LEADS THEM, THEN COME BACK TO ME.";
    case QType::Bounty:
      return place.empty() ? "THOSE BANDITS ARE STILL ON THE ROADS. KILL THEIR CHIEF AND THE BOUNTY IS YOURS."
                           : "THE BANDITS ARE STILL RAIDING FROM " + place + ". KILL THEIR CHIEF AND THE BOUNTY IS YOURS.";
    case QType::NamedBandit: {
      const std::string p = fem ? "HER" : "HIS";
      return q.subject + " STILL HOLDS " + (place.empty() ? std::string("THE CAMP") : place) + ". BRING ME WORD OF " + p + " DEATH AND THE BOUNTY IS YOURS.";
    }
    case QType::Deliver: {
      const std::string r = std::string(roleWord(questRecipientRole(q)));
      return "YOU STILL HAVE MY PARCEL? " + q.subject + ", THE " + r + (place.empty() ? std::string(", IS WAITING ON IT.") : " IN " + place + ", IS WAITING ON IT.");
    }
    case QType::Heirloom: {
      const std::string k = keepsakeOf(q.subject);
      const bool many = k.size() > 2 && k.back() == 'S' && k[k.size() - 2] != 'S';   // PRAYER BEADS
      return "ANY SIGN OF THE " + k + "? " + (many ? "THEY" : "IT") + " SHOULD LIE IN AN OLD CHEST IN " + (place.empty() ? std::string("THAT PLACE") : place) + ".";
    }
    case QType::Missing:
      if (q.flags & QF_FOUND) return "YOU FOUND " + q.subject + "? THEN BRING " + obj + " HOME, PLEASE. I CAN'T BEAR ANOTHER NIGHT OF THIS.";
      return "PLEASE, HAVE YOU FOUND " + q.subject + "? " + subj + " IS SOMEWHERE IN " + (place.empty() ? std::string("THAT PLACE") : place) + ".";
    case QType::Protect: {
      static const char* const v[] = {"DON'T FORGET: THE FIELDS AT NIGHTFALL. THEY COME AFTER DARK, AND I CAN'T HOLD THEM ALONE.",
                                       "I'LL BE AT THE FIELDS WHEN THE SUN GOES DOWN. BE THERE, OR THERE'LL BE NO HARVEST.",
                                       "TONIGHT, THEN. I'VE SHARPENED THE PITCHFORK, FOR WHAT GOOD IT WILL DO. MEET ME AT THE FIELDS AT DUSK."};
      return v[(hash32((uint32_t)q.giverSlot * 2654435761u ^ (uint32_t)q.giverId) + h) % 3];
    }
    case QType::Supply: {
      if (q.flags & life::QF_SUP_GOODS) return "YOU HAVE THE " + q.subject + "? THEN BRING THEM IN. PEOPLE ARE WAITING.";
      const std::string t = place.empty() ? std::string("THE NEXT TOWN") : place.find(", ") != std::string::npos ? place + "," : place;
      return "STILL NO " + q.subject + "? " + t + " HAS PLENTY. I NEED " + goodUnits(q.need, q.subject) + ".";
    }
    case QType::War: return "YOU'RE WITH US, THEN. DO WHAT WE AGREED, AND COME BACK ALIVE.";
    case QType::Campaign: return q.subject.empty() ? "YOU HAVE YOUR ORDERS, SOLDIER. CARRY THEM OUT." : "YOUR ORDERS STAND: " + q.subject + ". MOVE, SOLDIER.";   // M12
    case QType::Envoy: return q.subject.empty() ? "OUR WORD IS STILL IN YOUR SATCHEL? THEY ARE WAITING FOR IT." : "OUR WORD IS STILL IN YOUR SATCHEL? " + q.subject + ", AND QUICKLY.";
    default: return "HAVE YOU DONE WHAT I ASKED? I'M STILL WAITING.";
  }
}

// M7b: what a giver says as the job is taken (where to go, who pays, when to come back)
std::string QuestOps::acceptLine(const Game& g, const Quest& q) {
  // (the pitch said the way there: the place by its name now)
  const std::string place = q.target >= 0 && q.target < (int)g.world.sites.size() && q.target != q.giverSite ? spokenPlace(g.world.sites[(size_t)q.target].name) : std::string();
  // (a place spoken with its article was named in the pitch already: "THE TOMB OF SHENSHA" once is enough)
  const std::string marked = place.empty() || place.rfind("THE ", 0) == 0 ? std::string("THE PLACE") : place;
  switch (q.type) {
    case QType::Deliver:
      return "HERE'S THE PARCEL. " + q.subject + (place.empty() ? std::string(" PAYS ON DELIVERY.") : " IS IN " + place + ", AND PAYS ON DELIVERY.") +
             " KEEP IT DRY, AND KEEP IT SHUT.";
    case QType::Protect: return "THEN MEET ME AT THE FIELDS BY NIGHTFALL. THEY COME AFTER DARK, AND THEY DON'T GIVE UP EASILY.";
    case QType::Hunt:   // (P1) the den or the ground is on the map
      return q.hasPos ? std::string("GOOD. I'VE MARKED ") + (q.targetId ? "THEIR DEN" : "WHERE THEY ROAM") + " ON YOUR MAP. COME BACK WHEN " + speech::countWord(q.need) +
                            " OF THE " + monsterPlural(q.mon) + " ARE DEAD."
                      : "GOOD. COME BACK TO ME WHEN " + speech::countWord(q.need) + " OF THE " + monsterPlural(q.mon) + " ARE DEAD.";
    case QType::Missing: return "THANK YOU. GODS, THANK YOU. BRING " + q.subject + " HOME ALIVE. I'VE MARKED " + marked + " ON YOUR MAP.";
    case QType::Heirloom: return "THANK YOU. I'VE MARKED " + marked + " ON YOUR MAP. I'LL BE WAITING FOR IT.";
    case QType::Supply: return "GOOD. " + (place.empty() ? std::string("THE NEXT TOWN") : place) + " HAS WHAT I NEED. COME STRAIGHT BACK WITH IT.";
    case QType::Clear: case QType::Bounty: case QType::NamedBandit:
      return place.empty() ? "GOOD. COME BACK TO ME WHEN IT'S DONE." : "GOOD. I'VE MARKED " + marked + " ON YOUR MAP. COME BACK TO ME WHEN IT'S DONE.";
    default: return "THANK YOU. COME BACK WHEN IT'S DONE.";
  }
}

// ---------------------------------------------------------------- the people of quests
bool QuestOps::isRecipient(const Game& g, const Quest& q, const Actor& a) {
  return q.type == QType::Deliver && q.state == QState::Active && g.inside && g.subBldg >= 0 && g.subBldg == q.destBldg && a.npc &&
         a.st != AState::Dead && a.role == questRecipientRole(q);
}

int QuestOps::escortFor(const Game& g, const Quest& q) {
  for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].quest == q.id && g.actors[k].npc && g.actors[k].st != AState::Dead) return (int)k;
  return -1;
}

void QuestOps::spawnEscort(Game& g, Quest& q, Vec2 at) {
  Spawn sp;
  sp.npc = true; sp.role = Role::Villager; sp.site = -1; sp.slot = -1;
  g.spawnHuman(sp, at);
  Actor& a = g.actors.back();
  Rng pr = personRng(q, g.seed);
  a.site = q.giverSite;   // dressed as the giver's people (the same look the quest named them with), then homeless again
  g.makeLook(a, Role::Villager, pr);
  a.site = -1;
  a.name = q.subject;
  a.quest = q.id;
  a.fromMap = false; a.militia = false;
  a.speed = 60;
}

void QuestOps::questFail(Game& g, Quest& q, const std::string& why) {
  q.state = QState::Done;
  q.flags |= QF_FAILED;
  PartyOps::quest(g, q, false);   // M8: the companions saw it fail (party_game.cpp)
  for (int i = (int)g.inv.size() - 1; i >= 0; i--) if (g.inv[(size_t)i].kind == ItemKind::Quest && g.inv[(size_t)i].questId == q.id) g.dropItem(i);
  g.emit(Ev::QuestUpdate, g.pl().p, q.id, 3, "QUEST FAILED: " + q.title);
  g.say(why);
  g.sfx((int)Sfx::MenuBack, g.pl().p, 0.7f);
  if (g.trackedQuest == q.id) {
    g.trackedQuest = -1;
    for (auto& o : g.quests) if (o.state != QState::Done && o.type != QType::Main) { g.trackedQuest = o.id; break; }
    if (g.trackedQuest < 0) for (auto& o : g.quests) if (o.state != QState::Done) { g.trackedQuest = o.id; break; }
  }
}

// a far floor tile of the current dungeon map, in the open (all 8 neighbours walkable, no prop), skipping `avoid`
// (a tile index) and its surroundings: where a lost keepsake or a lost person ends up
static int farTile(const Map& m, int avoid) {
  const int sx = m.exitX, sy = m.exitY - 1;
  if (!m.in(sx, sy)) return -1;
  std::vector<int> dist((size_t)m.w * m.h, -1);
  std::vector<int> qq;
  qq.push_back(sy * m.w + sx);
  dist[(size_t)qq[0]] = 0;
  // (M3 fixer) the best spot by rank, then by distance: open all round (3x3) and clear of the chest, else a free tile
  // clear of the chest, else any free tile (a tight cave with no 3x3 opening left a Missing quest's person unplaced:
  // seed 43, a quest that could never be finished)
  int best = -1, bd = -1, brank = -1;
  for (size_t h = 0; h < qq.size(); h++) {
    const int cx = qq[h] % m.w, cy = qq[h] / m.w;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      const int nx = cx + dx[k], ny = cy + dy[k];
      if (!m.in(nx, ny) || m.blocked(nx, ny)) continue;
      const int li = ny * m.w + nx;
      if (dist[(size_t)li] >= 0) continue;
      dist[(size_t)li] = dist[(size_t)qq[h]] + 1;
      qq.push_back(li);
    }
    bool free = m.propAt(cx, cy) == 0 && !m.blocked(cx, cy);
    for (const Spawn& sp : m.spawns) if (sp.x == cx && sp.y == cy) free = false;
    if (!free) continue;
    bool open = true;
    for (int oy = -1; oy <= 1 && open; oy++)
      for (int ox = -1; ox <= 1 && open; ox++) if (m.blocked(cx + ox, cy + oy)) open = false;
    const bool clear = !(avoid >= 0 && std::abs(cx - avoid % m.w) + std::abs(cy - avoid / m.w) < 6);
    const int rank = open && clear ? 2 : (clear ? 1 : 0);
    if (rank > brank || (rank == brank && dist[(size_t)qq[h]] > bd)) { brank = rank; bd = dist[(size_t)qq[h]]; best = qq[h]; }
  }
  return best;
}

bool Game::questChestAt(int qid, int& tx, int& ty) const {
  const Quest* q = questById(qid);
  if (!q || !inside || subSite < 0 || q->target != subSite) return false;
  auto it = marks.find(markKey((uint64_t)qid, Mk::HeirChest));
  if (it == marks.end() || it->second < 0 || it->second >= sub.w * sub.h) return false;
  tx = it->second % sub.w; ty = it->second / sub.w;
  return true;
}

Vec2 QuestOps::questChestTile(const Game& g, const Quest& q) {
  auto it = g.marks.find(markKey((uint64_t)q.id, Mk::HeirChest));
  const int idx = it != g.marks.end() ? it->second : farTile(g.sub, -1);
  if (idx < 0) return Vec2(-1, -1);
  return Vec2((float)(idx % g.sub.w), (float)(idx / g.sub.w));
}

// a cave, ruin or building map was just made: put the quests' chests and people in it
void QuestOps::questMapLoaded(Game& g) {
  if (!g.inside) return;
  bool solidDirty = false;
  if (g.subSite >= 0) {
    int chestIdx = -1;
    for (Quest& q : g.quests) {
      if (q.state != QState::Active || q.target != g.subSite) continue;
      if (q.type == QType::Heirloom && !(q.flags & QF_FOUND)) {
        const uint64_t mk = markKey((uint64_t)q.id, Mk::HeirChest);
        auto it = g.marks.find(mk);
        int idx = it != g.marks.end() ? it->second : farTile(g.sub, -1);
        if (idx < 0 || idx >= g.sub.w * g.sub.h) continue;
        g.marks[mk] = idx;
        chestIdx = idx;
        const bool opened = g.looted.count(((uint64_t)g.mapKey() << 32) | (uint32_t)idx) > 0;
        g.sub.prop[(size_t)idx] = (uint8_t)((int)(opened ? Prop::ChestOpen : Prop::Chest) + 1);
        solidDirty = true;
        if (!opened) g.emit(Ev::Notice, g.pl().p, (int)rgba(255, 220, 140), 0, "SOMEWHERE IN HERE: " + q.subject);
      }
    }
    if (solidDirty) g.sub.rebuildSolid();
    for (Quest& q : g.quests) {
      if (q.state != QState::Active || q.target != g.subSite || q.type != QType::Missing || QuestOps::escortFor(g, q) >= 0) continue;
      if (q.flags & QF_FOUND) QuestOps::spawnEscort(g, q, ClimbOps::freeSpot(g, (int)std::floor(g.pl().p.x / TILE), (int)std::floor(g.pl().p.y / TILE) + 1));
      else {
        const int idx = farTile(g.sub, chestIdx);
        if (idx < 0) continue;
        QuestOps::spawnEscort(g, q, Vec2((idx % g.sub.w) * TILE + 8.0f, (idx / g.sub.w) * TILE + 10.0f));
        g.emit(Ev::Notice, g.pl().p, (int)rgba(255, 220, 140), 0, "SOMEWHERE IN HERE: " + q.subject);
      }
    }
  }
  if (g.subBldg >= 0) {
    // a parcel's recipient: whoever keeps this place (the innkeeper, the smith, the merchant) wears the name the giver
    // gave; when nobody of that trade is here, the first person in the building is the one the parcel is for
    for (Quest& q : g.quests) {
      if (q.type != QType::Deliver || q.state != QState::Active || q.destBldg != g.subBldg) continue;
      int k = -1;
      for (size_t j = 1; j < g.actors.size(); j++) if (g.actors[j].npc && g.actors[j].role == questRecipientRole(q)) { k = (int)j; break; }
      if (k < 0)
        for (size_t j = 1; j < g.actors.size(); j++)
          if (g.actors[j].npc && g.actors[j].human) { k = (int)j; q.flags = (q.flags & ~0xFF00u) | ((uint32_t)g.actors[j].role << 8); break; }
      if (k < 0) continue;
      Actor& a = g.actors[(size_t)k];
      Rng pr = personRng(q, g.seed);
      g.makeLook(a, a.role, pr);
      a.name = q.subject;
    }
  }
}

// the player came out of a site: a missing person who followed them out is safe
void QuestOps::questLeftSite(Game& g, int si) {
  for (Quest& q : g.quests) {
    if (q.type != QType::Missing || q.state != QState::Active || q.target != si || !(q.flags & QF_FOUND)) continue;
    q.state = QState::Complete;
    q.flags |= QF_ESCORTED;
    QuestOps::spawnEscort(g, q, ClimbOps::freeSpot(g, (int)std::floor(g.pl().p.x / TILE) + 1, (int)std::floor(g.pl().p.y / TILE) + 1));
    g.emit(Ev::Text, g.actors.back().p + Vec2(0, -24), (int)rgba(255, 240, 200), 0, "DAYLIGHT! THANK YOU!");
    const std::string m = q.subject + " IS SAFE. " + rewardReadyMsg(g.world, q);
    g.emit(Ev::QuestUpdate, g.pl().p, q.id, 2, m);
    g.say(m);
    g.sfx((int)Sfx::QuestStart, g.pl().p, 1.2f);
  }
}

bool QuestOps::questChest(Game& gm, int tx, int ty) {
  if (!gm.inside || gm.subSite < 0) return false;
  const int idx = ty * gm.sub.w + tx;
  for (Quest& q : gm.quests) {
    if (q.type != QType::Heirloom || q.state != QState::Active || q.target != gm.subSite) continue;
    auto it = gm.marks.find(markKey((uint64_t)q.id, Mk::HeirChest));
    if (it == gm.marks.end() || it->second != idx) continue;
    gm.sub.prop[(size_t)idx] = (uint8_t)((int)Prop::ChestOpen + 1);
    gm.looted.insert(gm.lootKey(tx, ty));
    gm.sub.rebuildSolid();
    gm.sfx((int)Sfx::Chest, gm.pl().p);
    Item it2;
    it2.kind = ItemKind::Quest; it2.name = q.subject; it2.icon = keepsakeIcon(q.subject); it2.rarity = Rarity::Rare;
    it2.questId = q.id; it2.value = 0; it2.tint = rgba(236, 206, 120);
    gm.addItem(it2);
    Pickup g; g.p = Vec2(tx * TILE + 8.0f, ty * TILE + 18.0f); g.gold = 10 + gm.world.sites[(size_t)gm.subSite].level * 3; gm.pickups.push_back(g);
    q.flags |= QF_FOUND;
    q.state = QState::Complete;
    const std::string m = (q.flags & QF_GRAVE) ? rewardReadyMsg(gm.world, q) : "FOUND THE " + q.subject + ". " + rewardReadyMsg(gm.world, q);
    gm.emit(Ev::QuestUpdate, gm.pl().p, q.id, 2, m);
    gm.say(m);
    return true;
  }
  return false;
}

// someone just streamed in outdoors: a named bandit chief takes the name and the strength the bounty gave him
void QuestOps::questSpawned(Game& g, Actor& a) {
  if (g.inside || !a.boss || a.role != Role::Bandit || a.site < 0) return;
  for (const Quest& q : g.quests) {
    if (q.type != QType::NamedBandit || q.state != QState::Active || q.target != a.site) continue;
    Rng pr = personRng(q, g.seed);
    const art::HumanLook old = a.look;
    g.makeLook(a, Role::Bandit, pr);
    a.look.outfit = art::Outfit::Plate; a.look.helmet = old.helmet; a.look.weapon = old.weapon; a.look.cape = true;
    a.look.tabardColor = rgba(120, 28, 30);
    a.name = q.subject;
    a.maxHp *= 1.35f; a.hp = a.maxHp; a.dmg *= 1.2f; a.xp *= 2;
    a.quest = q.id;
    return;
  }
}

void QuestOps::questActorDown(Game& g, const Actor& a) {
  for (Quest& q : g.quests) {
    if (q.state != QState::Active) continue;
    if (q.type == QType::Protect && isGiver(q, a)) QuestOps::questFail(g, q, q.giverName + " IS DEAD. THE FIELDS OF " + giverTown(g.world, q) + " ARE LOST.");
    else if (q.type == QType::Missing && a.quest == q.id && a.npc) QuestOps::questFail(g, q, q.subject + " DID NOT SURVIVE.");
    else if (q.type == QType::Protect && a.quest == q.id && a.hostile) q.have++;   // a raider of the current wave down
  }
}

// once per step: Deliver recipients found on arrival, the Protect night, escorts that have gone home
// M5 (CITIZENS lane): the supply runs (goods loaded at the producer, into the giver's stores at the turn-in) and the
// rescues of villagers carried off by raids (life_quests.cpp offers them; life_raids.cpp takes the villager)
static art::Icon goodIcon(ew::Good g) {
  using G = ew::Good;
  switch (g) {
    case G::Grain: case G::Flour: case G::Bread: return art::Icon::Bread;
    case G::Produce: return art::Icon::Apple;
    case G::Fish: case G::Meat: case G::Livestock: return art::Icon::Meat;
    case G::Ore: case G::Ingot: case G::Tools: return art::Icon::Ore;
    case G::Hides: case G::Leather: case G::Wool: return art::Icon::Pelt;
    case G::Cloth: return art::Icon::Cloak;
    default: return art::Icon::Letter;
  }
}

void QuestOps::questTick(Game& gm, float dt) {
  (void)dt;
  {   // M5 (CITIZENS lane): supply runs and rescues
  for (Quest& q : gm.quests) {
    if (q.type == QType::Supply) {
      if (q.state == QState::Active && !(q.flags & life::QF_SUP_GOODS) && q.target >= 0 && q.target < (int)gm.world.sites.size()) {
        const Site& T = gm.world.sites[(size_t)q.target];
        bool there = false;
        if (gm.inside) there = gm.subSite == q.target || (gm.subBldg >= 0 && gm.subBldg < (int)gm.world.over.bldgs.size() && gm.world.over.bldgs[(size_t)gm.subBldg].site == q.target);
        else {
          const int tx = (int)std::floor(gm.pl().p.x / TILE), ty = (int)std::floor(gm.pl().p.y / TILE);
          there = tx >= T.r.x - 2 && ty >= T.r.y - 2 && tx < T.r.x + T.r.w + 2 && ty < T.r.y + T.r.h + 2;
        }
        if (there) {
          const ew::Good g = (ew::Good)std::clamp(q.stage, 0, (int)ew::Good::COUNT - 1);
          Item it;
          it.kind = ItemKind::Quest; it.name = std::to_string(q.need) + " " + q.subject; it.icon = goodIcon(g);
          it.questId = q.id; it.value = 0; it.tint = rgba(200, 170, 120);
          gm.addItem(it, false);
          if (life::Census* tc = gm.life.findMut(T.id)) tc->stock[(size_t)g] = (uint16_t)std::max(0, (int)tc->stock[(size_t)g] - q.need);
          q.flags |= life::QF_SUP_GOODS;
          q.have = q.need;
          q.state = QState::Complete;
          const std::string m = "LOADED " + std::to_string(q.need) + " " + q.subject + " IN " + T.name + ". " + rewardReadyMsg(gm.world, q);
          gm.emit(Ev::QuestUpdate, gm.pl().p, q.id, 2, m);
          gm.say(m);
          gm.sfx((int)Sfx::QuestStart, gm.pl().p, 1.1f);
        }
      }
      if (q.state == QState::Done && !(q.flags & QF_FAILED) && (q.flags & life::QF_SUP_GOODS) && !(q.flags & life::QF_SUP_DONE)) {
        gm.life.supply(q.giverId, (ew::Good)std::clamp(q.stage, 0, (int)ew::Good::COUNT - 1), q.need);
        q.flags |= life::QF_SUP_DONE;
      }
      continue;
    }
    if (q.type != QType::Missing || !q.giverId) continue;
    life::Census* c = gm.life.findMut(q.giverId);
    if (!c || c->takenIdx < 0 || c->takenIdx >= (int)c->res.size() || c->res[(size_t)c->takenIdx].name != q.subject) continue;
    if (c->takenQuest == 0 && q.state != QState::Done) c->takenQuest = q.id;
    if (c->takenQuest != q.id) continue;
    life::Resident& r = c->res[(size_t)c->takenIdx];
    if (q.state == QState::Complete || (q.state == QState::Done && !(q.flags & QF_FAILED))) {
      r.flags &= (uint16_t)~life::RF_AWAY;
      c->takenIdx = -1;
      c->takenQuest = 0;
    } else if (q.state == QState::Done) {
      gm.life.residentDied(q.giverId, r.idx, gm.day);
    }
  }
  }
  for (Quest& q : gm.quests) {
    if (q.state != QState::Active) continue;
    if (q.type == QType::Deliver && q.destBldg < 0 && q.target >= 0 && q.target < (int)gm.world.sites.size()) {
      const Site& T = gm.world.sites[(size_t)q.target];
      if (T.bldgCount <= 0) continue;
      int pickB = -1;
      for (int b = T.bldgFirst; b < T.bldgFirst + T.bldgCount && b < (int)gm.world.over.bldgs.size(); b++)
        if (gm.world.over.bldgs[(size_t)b].type == roleBuilding(questRecipientRole(q))) { pickB = b; break; }
      if (pickB < 0)
        for (int b = T.bldgFirst; b < T.bldgFirst + T.bldgCount && b < (int)gm.world.over.bldgs.size(); b++)
          if (gm.world.over.bldgs[(size_t)b].type == art::Building::Inn) { pickB = b; q.flags = (q.flags & ~0xFF00u) | ((uint32_t)Role::Innkeeper << 8); break; }
      if (pickB >= 0) {
        q.destBldg = pickB;
        q.hasPos = true;
        q.tgx = gm.world.ox + gm.world.over.bldgs[(size_t)pickB].doorX(); q.tgy = gm.world.oy + gm.world.over.bldgs[(size_t)pickB].doorY();
      }
    }
    if (q.type != QType::Protect) continue;
    const bool after = gm.day > q.deadlineDay + 1 || (gm.day == q.deadlineDay + 1 && gm.hour >= 5.5f);
    const bool night = (gm.day == q.deadlineDay && gm.hour >= 20.5f) || (gm.day == q.deadlineDay + 1 && gm.hour < 5.5f);
    if (after) {
      // held only when both waves came and the second was beaten, or the player still stood by the fields at dawn
      // with it (hiding indoors after the first wave, or walking off, does not count)
      bool held = false;
      if (q.flags & QF_WAVE2) {
        const Vec2 fp((q.tgx - gm.world.ox) * TILE + 8.0f, (q.tgy - gm.world.oy) * TILE + 10.0f);
        held = q.have >= q.need || (!gm.inside && len2(gm.pl().p - fp) <= (45.0f * TILE) * (45.0f * TILE));
      }
      if (held) {
        q.state = QState::Complete;
        for (size_t k = 1; k < gm.actors.size(); k++)
          if (gm.actors[k].quest == q.id && gm.actors[k].hostile && gm.actors[k].st != AState::Dead) { gm.actors[k].aggro = false; gm.actors[k].wild = true; gm.actors[k].home = gm.actors[k].p + (gm.actors[k].p - gm.pl().p) * 4.0f; }
        const std::string m = "DAWN BREAKS OVER " + giverTown(gm.world, q) + ". THE FIELDS ARE SAFE. " + rewardReadyMsg(gm.world, q);
        gm.emit(Ev::QuestUpdate, gm.pl().p, q.id, 2, m);
        gm.say(m);
        gm.sfx((int)Sfx::QuestDone, gm.pl().p, 1.1f);
      } else {
        const Biome b = gm.world.over.biomeAt(q.tgx - gm.world.ox, q.tgy - gm.world.oy);
        QuestOps::questFail(gm, q, std::string("THE ") + zonePlural(protectZone(q, b), b) + " RAVAGED THE FIELDS OF " + giverTown(gm.world, q) + " WHILE YOU WERE AWAY.");
      }
      continue;
    }
    if (!night || gm.inside) continue;
    const int fx = q.tgx - gm.world.ox, fy = q.tgy - gm.world.oy;
    const Vec2 fp(fx * TILE + 8.0f, fy * TILE + 10.0f);
    if (len2(gm.pl().p - fp) > (45.0f * TILE) * (45.0f * TILE)) continue;
    // the waves: q.need raiders in the current wave, q.have of them felled. Raiders that vanished (the player went
    // indoors, a reload) are still out there: the missing ones come back; the next wave comes once this one is down.
    int alive = 0;
    for (size_t k = 1; k < gm.actors.size(); k++) if (gm.actors[k].quest == q.id && gm.actors[k].hostile && gm.actors[k].st != AState::Dead) alive++;
    const bool first = !(q.flags & QF_WAVE1);
    if (alive > 0) continue;
    const bool regroup = !first && q.have < q.need;
    if (!first && !regroup && (q.flags & QF_WAVE2)) continue;   // both waves are down: dawn will tell
    // a wave: from the wild side of the fields (away from the heart of the village), aimed at the fields
    const Biome b = gm.world.over.biomeAt(fx, fy);
    const int z = protectZone(q, b);
    Vec2 out(1, 0);
    if (q.target >= 0 && q.target < (int)gm.world.sites.size()) {
      const Site& S = gm.world.sites[(size_t)q.target];
      const Vec2 d((float)(fx - S.ex), (float)(fy - S.ey));
      if (len2(d) > 0.5f) out = norm(d);
    }
    const int lvl = std::max(1, gm.world.zoneLevel(fx, fy));
    const bool second = !first && !regroup;
    const int n = regroup ? q.need - q.have : std::min(7, (first ? 3 : 4) + lvl / 3);
    if (!regroup) { q.need = n; q.have = 0; }
    for (int i = 0; i < n; i++) {
      const float ang = (i - (n - 1) * 0.5f) * 0.28f;
      const Vec2 dir(out.x * std::cos(ang) - out.y * std::sin(ang), out.x * std::sin(ang) + out.y * std::cos(ang));
      const Vec2 at0 = fp + dir * (17.0f * TILE);
      const Vec2 at = ClimbOps::freeSpot(gm, (int)std::floor(at0.x / TILE), (int)std::floor(at0.y / TILE));
      int id;
      if (z == 2) {
        Spawn sp;
        sp.bandit = true; sp.site = q.target; sp.slot = -1;
        id = gm.spawnHuman(sp, at);
      } else id = gm.spawnMonster(zoneMonster(z, b), at, lvl, false);
      const int k = gm.findActor(id);
      if (k < 0) continue;
      Actor& m = gm.actors[(size_t)k];
      m.quest = q.id; m.aggro = true; m.fromMap = false; m.site = -1; m.home = fp; m.goal = fp;
    }
    if (regroup) continue;
    (void)second;
    q.flags |= first ? QF_WAVE1 : QF_WAVE2;
    const std::string m = first ? std::string("THE ") + zonePlural(z, b) + " ARE COMING! HOLD THE FIELDS UNTIL DAWN!" : "MORE OF THEM! HOLD THE LINE!";
    gm.emit(Ev::Notice, gm.pl().p, (int)rgba(255, 110, 80), 0, m);
    gm.say(m);
    gm.sfx((int)Sfx::Bell, gm.pl().p, 1.0f, 0.8f);
  }
  // a missing person brought home walks off once out of sight
  for (size_t k = 1; k < gm.actors.size();) {
    const Actor& a = gm.actors[k];
    if (a.quest > 0 && a.npc) {
      const Quest* q = gm.questById(a.quest);
      const bool following = q && q->state == QState::Active;
      if (!following && len2(a.p - gm.pl().p) > (14.0f * TILE) * (14.0f * TILE)) { gm.actors.erase(gm.actors.begin() + (std::ptrdiff_t)k); continue; }
    }
    k++;
  }
}

// M2 quest talk: options for the people of the new quest types. Returns a line that replaces the greeting ("" none).
std::string QuestOps::questDialogue(Game& g, Actor& a) {
  std::string text;
  for (Quest& q : g.quests) {
    if (QuestOps::isRecipient(g, q, a)) {
      const std::string from = q.giverName + (giverTown(g.world, q).empty() ? std::string() : " IN " + giverTown(g.world, q));
      text = "A PARCEL? FROM " + from + "? I'VE BEEN WAITING ON THIS FOR WEEKS.";
      g.dlg.opts.push_back({"HAND OVER THE PARCEL (+" + std::to_string(q.gold) + " GOLD)", A_DELIVER, q.id});
    }
  }
  return text;
}
