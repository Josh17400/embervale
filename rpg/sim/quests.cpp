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
#include "engine/audio.h"
#include "rpg/sim/game_internal.h"
#include "rpg/world/source.h"

using art::Monster;
using art::Prop;

namespace {
constexpr int kRadiantRange = 768;   // tiles: a long day's walk (VISION_PLAN 2.11)

// the person behind a quest (a Deliver recipient, a missing person, a named chief): look and name come from the
// quest's 16-bit person seed (Quest::flags bits 16-31), so they are the same in every session
Rng personRng(const Quest& q, uint64_t worldSeed) {
  return Rng(((uint64_t)hash32(((q.flags >> 16) & 0xFFFFu) * 2654435761u ^ (uint32_t)worldSeed ^ 0x9E57u) << 8) ^ ((uint64_t)q.type << 40) ^ 0x51u);
}
bool personFemale(const Quest& q, uint64_t worldSeed) { Rng r = personRng(q, worldSeed); return r.f() < 0.45f; }   // makeLook's first draw

void replaceAll(std::string& s, const std::string& from, const std::string& to) {
  if (from.empty()) return;
  for (size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size())) s.replace(at, from.size(), to);
}
struct Vars {
  std::vector<std::pair<std::string, std::string>> kv;
  void set(const char* k, const std::string& v) { kv.push_back({std::string("{") + k + "}", v}); }
  std::string fill(std::string s) const {
    for (const auto& p : kv) replaceAll(s, p.first, p.second);
    return s;
  }
};
template <size_t N> const char* pick(Rng& r, const char* const (&a)[N]) { return a[r.irange((int)N)]; }

const char* walkWords(float tiles) {
  const float h = tiles / 30.0f;
  if (h < 0.75f) return "LESS THAN AN HOUR'S WALK";
  if (h < 1.5f) return "AN HOUR'S WALK";
  if (h < 4.5f) return "A FEW HOURS' WALK";
  if (h < 9.0f) return "HALF A DAY'S WALK";
  if (h < 18.0f) return "A DAY'S WALK";
  return "DAYS ON THE ROAD";
}

// ---- the grammar
const char* const kMoodAny[] = {
    "I HATE TO ASK A STRANGER, BUT I'M OUT OF FRIENDS TO ASK.", "YOU HAVE THE LOOK OF SOMEONE WHO CAN HANDLE A BLADE.",
    "KEEP YOUR VOICE DOWN AND LISTEN.", "BY THE OLD GODS, I'M AT MY WIT'S END.", "I'LL BE PLAIN WITH YOU.",
    "YOU'RE NOT FROM {H}. GOOD: NOBODY HERE WILL DO IT.", "SIT A MOMENT. I NEED SOMEONE I DON'T OWE FAVOURS TO.",
    "THE GODS MUST HAVE SENT YOU."};
const char* const kMoodGuard[] = {"THE WATCH IS STRETCHED THIN, SO I'LL TAKE ANY BLADE I CAN GET.", "OFF THE RECORD, TRAVELLER:",
                                  "THE CAPTAIN WON'T SPARE THE MEN. SO I'M ASKING YOU."};
const char* const kMoodJarl[] = {"THE HOLD HAS NEED OF YOU.", "MY HOUSECARLS ARE NEEDED HERE. YOU ARE NOT.", "SERVE THE HOLD AND THE HOLD REMEMBERS."};
const char* const kMoodInn[] = {"YOU HEAR THINGS, KEEPING AN INN.", "POUR YOURSELF A MUG. IT'S ON THE HOUSE IF YOU HEAR ME OUT.",
                                "EVERY TRAVELLER WHO SITS AT THAT TABLE HAS THE SAME STORY LATELY."};
const char* const kMoodPriest[] = {"THE GODS ASK MUCH OF US. TODAY THEY ASK SOMETHING OF YOU.", "I PRAYED FOR HELP, AND HERE YOU STAND.",
                                   "THERE IS A WEIGHT ON THIS TEMPLE, CHILD."};
const char* const kMoodFarmer[] = {"IT'S BEEN A HARD SEASON, AND IT'S ABOUT TO GET HARDER.", "I'M A FARMER, NOT A FIGHTER.",
                                   "THE HARVEST WON'T WAIT, AND NEITHER WILL THIS."};
const char* const kMoodHunter[] = {"YOU WALK QUIETER THAN MOST TOWNSFOLK.", "THE WOODS ARE TALKING, IF YOU KNOW HOW TO LISTEN."};
const char* const kMoodTrade[] = {"BUSINESS IS BUSINESS, AND THIS IS BUSINESS.", "TIME IS COIN, SO I'LL BE QUICK.", "I'VE A JOB FOR STEADY HANDS."};

const char* const kReward[] = {"THERE'S {G} GOLD IN IT FOR YOU.", "I CAN PAY {G} GOLD. IT'S ALL I HAVE.", "{G} GOLD, AND MY THANKS.",
                               "{G} GOLD WHEN IT'S DONE. HALF OF {H} CHIPPED IN.", "DO THIS AND {G} GOLD IS YOURS.",
                               "{G} GOLD. DON'T HAGGLE, I'M NOT A MERCHANT."};
const char* const kRewardCrown[] = {"THE CROWN OF {K} PAYS {G} GOLD FOR WORK LIKE THIS.", "{G} GOLD FROM THE HOLD'S PURSE."};
const char* const kDanger[] = {"BE WARNED: IT IS DANGEROUS COUNTRY. I'LL PAY HALF AGAIN FOR THE RISK.",
                               "IT IS DANGEROUS COUNTRY OUT THERE, SO THE PRICE IS HALF AGAIN."};

const char* const kRelK[] = {"GRANDMOTHER", "GRANDFATHER", "FATHER", "MOTHER", "UNCLE", "AUNT", "BROTHER", "SISTER", "HUSBAND", "WIFE"};
const bool kRelFemale[] = {true, false, false, true, false, true, false, true, false, true};
const char* const kKeepsake[] = {"SILVER RING", "GOLD LOCKET", "IVORY COMB", "OLD SWORD", "WEDDING BAND", "SIGNET RING", "CARVED HORN",
                                 "AMBER PENDANT", "BRASS COMPASS", "PRAYER BEADS", "WAR MEDAL", "HUNTING KNIFE"};
const char* const kEpithet[] = {"THE RED", "THE GRIM", "THE CRUEL", "ONE-EYE", "BLACKHAND", "THE BUTCHER", "IRONJAW", "THE WOLF",
                                "SILVERTONGUE", "THE BURNED", "HALFHAND", "THE CROW"};
const char* const kLookingFor[] = {"MUSHROOMS", "A LOST GOAT", "SILVER", "FIREWOOD", "THE OLD DOG", "BLUE MOUNTAIN FLOWERS"};

art::Icon keepsakeIcon(const std::string& s) {
  if (s.find("RING") != std::string::npos || s.find("BAND") != std::string::npos) return art::Icon::Ring;
  if (s.find("LOCKET") != std::string::npos || s.find("PENDANT") != std::string::npos || s.find("BEADS") != std::string::npos ||
      s.find("MEDAL") != std::string::npos) return art::Icon::Amulet;
  if (s.find("SWORD") != std::string::npos) return art::Icon::Sword;
  if (s.find("KNIFE") != std::string::npos) return art::Icon::Dagger;
  if (s.find("HORN") != std::string::npos) return art::Icon::Bone;
  return art::Icon::Gem;
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
}  // namespace

namespace {
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

enum OfferKind { K_CAVE, K_RUIN, K_BOUNTY, K_HUNT, K_DELIVER, K_HEIR, K_MISSING, K_NAMED, K_PROTECT };
QType kindType(OfferKind k) {
  switch (k) {
    case K_CAVE: case K_RUIN: return QType::Clear;
    case K_BOUNTY: return QType::Bounty;
    case K_HUNT: return QType::Hunt;
    case K_DELIVER: return QType::Deliver;
    case K_HEIR: return QType::Heirloom;
    case K_MISSING: return QType::Missing;
    case K_NAMED: return QType::NamedBandit;
    default: return QType::Protect;
  }
}
}  // namespace

// the nearest-ish unfinished place of a type within kRadiantRange tiles of a global tile, from the region plans (no
// tiles are generated); places beyond the player (danger > level + 3) only when nothing else fits (danger = true)
int Game::pickRadiant(SiteType t, int32_t gx, int32_t gy, Rng& r, bool& danger, int exclude) {
  danger = false;
  if (!world.endless || !world.src) return -1;
  const ew::Gid exId = exclude >= 0 && exclude < (int)world.sites.size() ? world.sites[(size_t)exclude].id : 0;
  const int rings = (kRadiantRange + ew::REGION - 1) / ew::REGION;
  const int32_t rx0 = ew::regionOf(gx), ry0 = ew::regionOf(gy);
  struct Cand { ew::Gid id; float score; bool fit; };
  std::vector<Cand> c;
  for (int32_t ry = ry0 - rings; ry <= ry0 + rings; ry++)
    for (int32_t rx = rx0 - rings; rx <= rx0 + rings; rx++) {
      const ew::RegionPlan R = world.regionPlan(rx, ry);
      for (const ew::SitePlan& p : R.sites) {
        if (p.type != t || (p.flags & ew::SPF_MAINQUEST) || p.id == exId) continue;
        const float d = std::hypot((float)(p.ex - gx), (float)(p.ey - gy));
        if (d > (float)kRadiantRange) continue;
        const int h = world.siteHandle(p.id);
        if (h >= 0 && world.sites[(size_t)h].cleared) continue;
        // not a place another open job points at, nor one a finished job already sent the player to
        bool taken = false;
        for (const Quest& o : quests) {
          if (o.type == QType::Main) continue;
          const ew::Gid oid = o.targetId ? o.targetId : (o.target >= 0 && o.target < (int)world.sites.size() ? world.sites[(size_t)o.target].id : 0);
          if (oid == p.id) taken = true;
        }
        if (taken) continue;
        c.push_back({p.id, d * (0.8f + r.f() * 0.5f), p.level <= plLevel + 3});
      }
    }
  const Cand* best = nullptr;
  for (const Cand& k : c) if (k.fit && (!best || k.score < best->score)) best = &k;
  if (!best) {
    for (const Cand& k : c) if (!best || k.score < best->score) best = &k;
    danger = best != nullptr;
  }
  return best ? world.ensureSite(best->id) : -1;
}

// Deliver: a settlement 150-700 tiles away on the same landmass (one of the nearer few, by the offer's dice)
int Game::pickDestination(int32_t gx, int32_t gy, Rng& r) {
  if (!world.endless || !world.src) return -1;
  const int rings = 3;
  const int32_t rx0 = ew::regionOf(gx), ry0 = ew::regionOf(gy);
  const int32_t land = landAt(gx, gy);
  std::vector<std::pair<float, ew::Gid>> c;
  for (int32_t ry = ry0 - rings; ry <= ry0 + rings; ry++)
    for (int32_t rx = rx0 - rings; rx <= rx0 + rings; rx++) {
      const ew::RegionPlan R = world.regionPlan(rx, ry);
      for (const ew::SitePlan& p : R.sites) {
        if (p.type != SiteType::Village && p.type != SiteType::Town && p.type != SiteType::City) continue;
        const float d = std::hypot((float)(p.ex - gx), (float)(p.ey - gy));
        if (d < 150.0f || d > 700.0f) continue;
        bool taken = false;
        for (const Quest& o : quests) if (o.type == QType::Deliver && o.state != QState::Done && o.targetId == p.id) taken = true;
        if (taken) continue;
        if (land && landAt(p.ex, p.ey) != land) continue;
        c.push_back({d, p.id});
      }
    }
  if (c.empty()) return -1;
  std::sort(c.begin(), c.end());
  const int n = std::min<int>(5, (int)c.size());
  return world.ensureSite(c[(size_t)r.irange(n)].second);
}

Quest Game::offerFor(const Actor& a, QType want, bool& ok) {
  ok = false;
  Quest q;
  pendingPitch_.clear();
  if (!a.npc || a.site < 0 || a.site >= (int)world.sites.size()) return q;
  const uint64_t key = npcKey(a);
  auto it = npcQuestsDone.find(key);
  const int done = it == npcQuestsDone.end() ? 0 : it->second;
  Rng r(((uint64_t)hash32((uint32_t)key * 31u + (uint32_t)done) << 24) ^ (key >> 9) ^ seed ^ 0x0FFE5ull);
  // (M2 fixer round 2) the grammar's words turn with the jobs this person has given: two offers of one kind a few jobs
  // apart never come out word for word the same (a giver repeated HOLD THE FIELDS)
  // (by the person, not the job's dice: the title cycles by 3 and the description by 4, so two offers of one kind
  // within a dozen jobs never share both)
  auto pickN = [&](Rng& rr, const auto& arr) {
    const int n = (int)(sizeof(arr) / sizeof(arr[0]));
    (void)rr.irange(n);   // (the dice still roll: the rest of the offer keeps its draws)
    return arr[(int)(((key >> 13) % (uint64_t)n + (uint64_t)done) % (uint64_t)n)];
  };
  q.giverSite = a.site; q.giverBldg = a.bldg; q.giverSlot = a.slot; q.giverName = a.name;
  const Site home = world.sites[(size_t)a.site];   // a copy: picking targets loads more sites
  q.giverId = home.id;
  const int32_t hx = world.ox + home.ex, hy = world.oy + home.ey;
  q.flags = (r.next() & 0xFFFFu) << 16;   // the person seed (personRng)
  // the local names the grammar may use: the kingdom, a named landmark or the nearest dark place
  const Kingdom* K = world.kingdomOf(a.site);
  std::string localName;
  {
    const ew::RegionPlan R = world.regionPlan(ew::regionOf(hx), ew::regionOf(hy));
    float bd = 1e30f;
    for (const ew::LandmarkPlan& L : R.landmarks) {
      const float d = std::hypot((float)(L.x - hx), (float)(L.y - hy));
      if (d < bd) { bd = d; localName = L.name; }
    }
    if (localName.empty()) {
      for (int i : world.nearSites) {
        const Site& s = world.sites[(size_t)i];
        if (s.type != SiteType::Cave && s.type != SiteType::Ruin) continue;
        const float d = std::hypot((float)(s.ex - home.ex), (float)(s.ey - home.ey));
        if (d < bd) { bd = d; localName = s.name; }
      }
    }
    if (localName.empty()) localName = home.name;
  }
  Vars V;
  V.set("H", home.name);
  V.set("GIVER", a.name);
  V.set("K", K ? K->name : home.name);
  V.set("N", localName);
  // what this person offers (by trade), tried from a rotating start so successive jobs differ; the kind of the last
  // job they gave goes to the back of the line
  std::vector<OfferKind> menu;
  switch (a.role) {
    case Role::Jarl: menu = {K_CAVE, K_RUIN, K_NAMED, K_MISSING}; break;
    case Role::Guard: menu = {K_BOUNTY, K_NAMED, K_CAVE}; break;
    case Role::Innkeeper: menu = {K_BOUNTY, K_HUNT, K_DELIVER, K_NAMED, K_HEIR}; break;
    case Role::Priest: menu = {K_RUIN, K_MISSING, K_HEIR}; break;
    case Role::Smith: menu = {K_HUNT, K_DELIVER, K_CAVE}; break;
    case Role::Mage: menu = {K_RUIN, K_HEIR}; break;
    case Role::Merchant: menu = {K_DELIVER, K_HEIR, K_BOUNTY}; break;
    case Role::Farmer: menu = {K_PROTECT, K_HUNT, K_MISSING}; break;
    case Role::Hunter: menu = {K_HUNT}; break;
    case Role::Fisher: menu = {K_HUNT, K_DELIVER}; break;
    case Role::Herbalist: menu = {K_HEIR, K_MISSING}; break;
    default: menu = {K_HUNT, K_HEIR, K_MISSING, K_DELIVER, K_PROTECT}; break;
  }
  if (want != QType::COUNT) {
    menu.clear();
    if (want == QType::Clear) menu = {K_CAVE, K_RUIN};
    else
      for (int k = K_BOUNTY; k <= K_PROTECT; k++)
        if (kindType((OfferKind)k) == want) menu.push_back((OfferKind)k);
    if (menu.empty()) return q;
  }
  {
    const int n = (int)menu.size();
    const int start = (r.irange(n) + done) % n;
    std::rotate(menu.begin(), menu.begin() + start, menu.end());
    auto lo = marks.find(markKey(key, Mk::LastOffer));
    if (lo != marks.end() && n > 1)
      std::stable_partition(menu.begin(), menu.end(), [&](OfferKind k) { return (int)kindType(k) + 1 != lo->second; });
  }
  bool danger = false;
  int targetLevel = home.level;
  std::string title, reason, ask, desc;
  auto atSite = [&](int t) {
    const Site& T = world.sites[(size_t)t];
    q.target = t; q.targetId = T.id;
    q.hasPos = true; q.tgx = world.ox + T.ex; q.tgy = world.oy + T.ey;
    targetLevel = std::max(targetLevel, T.level);
    V.set("T", T.name);
    V.set("D", dirWord(q.tgx - hx, q.tgy - hy, false));
    V.set("W", walkWords(std::hypot((float)(q.tgx - hx), (float)(q.tgy - hy))));
    V.set("M", monsterPlural(T.theme));
  };
  auto pron = [&](bool female) {
    V.set("P_SUBJ", female ? "SHE" : "HE");
    V.set("P_OBJ", female ? "HER" : "HIM");
    V.set("P_POSS", female ? "HER" : "HIS");
  };
  auto build = [&](OfferKind k) -> bool {
    switch (k) {
      case K_CAVE: case K_RUIN: {
        const int t = pickRadiant(k == K_CAVE ? SiteType::Cave : SiteType::Ruin, hx, hy, r, danger);
        if (t < 0) return false;
        q.type = QType::Clear;
        atSite(t);
        if (k == K_CAVE) {
          static const char* const ti[] = {"CLEAR {T}", "THE THING IN {T}", "TROUBLE AT {T}"};
          static const char* const re[] = {"SOMETHING FOUL HAS MADE ITS NEST IN {T}, {W} {D} OF HERE.",
                                           "THE HUNTERS WON'T GO NEAR {T} ANY MORE. THE LAST ONE CAME BACK WITHOUT HIS DOG.",
                                           "{M} CRAWL OUT OF {T} AT NIGHT AND TAKE OUR GOATS.",
                                           "MY BROTHER SWEARS HE HEARD SOMETHING HUGE BREATHING IN {T}, PAST {N}."};
          static const char* const as[] = {"KILL WHATEVER LEADS THEM.", "GO IN AND KILL THE BIGGEST THING YOU FIND.", "END IT, WHATEVER IT IS."};
          static const char* const de[] = {"SOMETHING FOUL NESTS IN {T}, {D} OF {H}. KILL WHATEVER LEADS IT, THEN RETURN TO {GIVER} IN {H}.",
                                           "{M} HAUNT {T} ({D} OF {H}). KILL THEIR LEADER AND TELL {GIVER} IN {H}."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as); desc = pickN(r, de);
        } else {
          static const char* const ti[] = {"THE RESTLESS DEAD", "LIGHTS IN {T}", "LAY {T} TO REST"};
          static const char* const re[] = {"THE DEAD STIR IN {T}.", "LIGHTS BURN IN {T} WHERE NO LIVING HAND LIT THEM.",
                                           "THE OLD KINGS OF {K} BURIED THEIR DEAD IN {T}, AND THE DEAD HAVE NOT STAYED BURIED.",
                                           "GRAVE-ROBBERS WENT INTO {T} AND WOKE SOMETHING. NOW IT WALKS THE ROAD {D} OF HERE."};
          static const char* const as[] = {"PUT THEIR MASTER BACK IN THE GROUND.", "FIND WHAT WAKES THEM AND DESTROY IT."};
          static const char* const de[] = {"THE DEAD STIR IN {T}, {D} OF {H}. DESTROY THEIR MASTER, THEN REPORT TO {GIVER}.",
                                           "SOMETHING WAKES THE DEAD OF {T} ({D} OF {H}). END IT AND RETURN TO {GIVER}."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as); desc = pickN(r, de);
        }
        return true;
      }
      case K_BOUNTY: case K_NAMED: {
        const int t = pickRadiant(SiteType::BanditCamp, hx, hy, r, danger);
        if (t < 0) return false;
        atSite(t);
        if (k == K_BOUNTY) {
          q.type = QType::Bounty;
          static const char* const ti[] = {"BOUNTY: {T}", "A PRICE ON {T}", "THE OUTLAWS OF {T}"};
          static const char* const re[] = {"BANDITS AT {T} HAVE BEEN RAIDING THE ROADS {D} OF {H}.",
                                           "THE OUTLAWS OF {T} ROBBED A WAGON BOUND FOR {H} LAST WEEK.",
                                           "THEY BURNED A FARM NEAR {N}. THE TRACKS LED TO {T}, {W} {D}."};
          static const char* const as[] = {"KILL THEIR CHIEF AND THE REST WILL SCATTER.", "CUT THE HEAD OFF THAT SNAKE: KILL THEIR CHIEF."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as);
          desc = "BANDITS AT {T} HAVE BEEN RAIDING TRAVELLERS. KILL THEIR CHIEF AND COLLECT THE BOUNTY FROM {GIVER}.";
        } else {
          q.type = QType::NamedBandit;
          Actor d;
          d.site = t;
          Rng pr = personRng(q, seed);
          makeLook(d, Role::Bandit, pr);
          q.subject = d.name + " " + pick(r, kEpithet);
          pron(personFemale(q, seed));
          V.set("S", q.subject);
          static const char* const ti[] = {"{S}", "THE END OF {S}", "A PRICE ON {S}"};
          static const char* const re[] = {"{S} RULES {T} NOW. {P_SUBJ} BURNED THE MILL AT {H} LAST SPRING.",
                                           "THEY CALL {P_OBJ} {S}. {P_SUBJ} AND {P_POSS} CUTTHROATS HOLD {T}, {W} {D} OF HERE.",
                                           "EVERY CART ON THE ROAD PAST {N} PAYS {S} A TOLL OR BLEEDS FOR IT."};
          static const char* const as[] = {"END {S} AND THE PRICE ON {P_POSS} HEAD IS YOURS.", "BRING {S} DOWN. NOBODY ELSE WILL."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as);
          desc = "{S} HOLDS {T} ({D} OF {H}). KILL {P_OBJ} AND COLLECT THE BOUNTY FROM {GIVER}.";
        }
        return true;
      }
      case K_HUNT: {
        q.type = QType::Hunt;
        const Biome b = world.over.biomeAt(home.ex, home.ey);
        Monster opts[3] = {Monster::Wolf, Monster::Boar, Monster::Spider};
        if (b == Biome::Snow) { opts[0] = Monster::IceWolf; opts[1] = Monster::FrostSpider; opts[2] = Monster::Troll; }
        else if (b == Biome::Taiga) { opts[1] = Monster::Bear; opts[2] = Monster::Troll; }
        else if (b == Biome::Swamp) { opts[0] = Monster::Slime; opts[1] = Monster::Mudcrab; }
        else if (b == Biome::Desert) { opts[0] = Monster::Sandworm; opts[1] = Monster::Goblin; opts[2] = Monster::Skeleton; }
        else if (b == Biome::Plains) { opts[2] = Monster::Goblin; }
        q.mon = opts[r.irange(3)];
        q.need = (q.mon == Monster::Troll || q.mon == Monster::Bear || q.mon == Monster::Sandworm) ? 2 : 4 + r.irange(3);
        q.target = -1;
        V.set("M", monsterPlural(q.mon));
        V.set("n", std::to_string(q.need));
        static const char* const ti[] = {"CULL THE {M}", "TOO MANY {M}", "THE {M} OF {H}"};
        static const char* const re[] = {"THE {M} AROUND {H} ARE GETTING BOLD.", "{M} HAVE BEEN AT THE SHEEP AGAIN.",
                                         "THE {M} CAME DOWN PAST {N} WITH THE COLD, AND THEY ARE HUNGRY.", "I LOST A GOOD DOG TO THE {M} LAST NIGHT."};
        static const char* const as[] = {"KILL {n} OF THEM.", "THIN THEM OUT: {n} SHOULD DO IT."};
        static const char* const de[] = {"THE {M} AROUND {H} ARE GETTING BOLD. KILL {n} OF THEM AND {GIVER} WILL PAY YOU.",
                                         "{GIVER} WANTS THE {M} NEAR {H} THINNED OUT: {n} OF THEM, FOR A FAIR PRICE.",
                                         "HUNT {n} {M} IN THE WILDS ROUND {H}. {GIVER} PAYS ON YOUR RETURN.",
                                         "{M} PROWL CLOSE TO {H} AGAIN. BRING DOWN {n} AND {GIVER} WILL MAKE IT WORTH YOUR WHILE."};
        title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as); desc = pickN(r, de);
        return true;
      }
      case K_DELIVER: {
        const int t = pickDestination(hx, hy, r);
        if (t < 0) return false;
        atSite(t);
        const Site T = world.sites[(size_t)t];
        q.type = QType::Deliver;
        const float rr = r.f();
        const Role role = rr < 0.5f ? Role::Innkeeper : (rr < 0.8f || T.type == SiteType::Village ? Role::Smith : Role::Merchant);
        q.flags |= (uint32_t)role << 8;
        Actor d;
        d.site = t;
        Rng pr = personRng(q, seed);
        makeLook(d, role, pr);
        q.subject = d.name;
        pron(personFemale(q, seed));
        V.set("S", q.subject);
        V.set("R", roleWord(role));
        // the recipient's door when the place's records are in (else it is found on arrival: questTick)
        for (int b = T.bldgFirst; b < T.bldgFirst + T.bldgCount && b < (int)world.over.bldgs.size(); b++)
          if (world.over.bldgs[(size_t)b].type == roleBuilding(role)) { q.destBldg = b; break; }
        static const char* const ti[] = {"A PARCEL FOR {S}", "THE ROAD TO {T}", "LETTERS FOR {T}"};
        static const char* const re[] = {"THIS PARCEL MUST REACH {S}, THE {R} OF {T}. THE ROAD RUNS {D}, {W}.",
                                         "I PROMISED {S} IN {T} THIS PACKAGE BEFORE THE NEW MOON, AND MY LEGS ARE NOT WHAT THEY WERE.",
                                         "{S}, THE {R} OF {T}, IS WAITING ON THESE LETTERS. IT IS {W} {D} OF HERE."};
        static const char* const as[] = {"TAKE IT TO {P_OBJ}.", "PUT IT IN THE HANDS OF {S} AND NOBODY ELSE."};
        title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as);
        desc = "CARRY A PARCEL FROM {GIVER} TO {S}, THE {R} OF {T} ({D} OF {H}). {S} PAYS ON DELIVERY.";
        return true;
      }
      case K_HEIR: {
        const int t = pickRadiant(r.f() < 0.5f ? SiteType::Ruin : SiteType::Cave, hx, hy, r, danger);
        if (t < 0) return false;
        q.type = QType::Heirloom;
        atSite(t);
        const int ri = r.irange(10), ki = r.irange(12);
        q.subject = std::string(kRelK[ri]) + "'S " + kKeepsake[ki];
        V.set("S", q.subject);
        V.set("REL", kRelK[ri]);
        V.set("REL_P", kRelFemale[ri] ? "HER" : "HIM");
        V.set("OBJ", std::string(kRelFemale[ri] ? "HER " : "HIS ") + kKeepsake[ki]);
        static const char* const ti[] = {"{S}", "AN HEIRLOOM IN {T}", "WHAT {T} TOOK"};
        static const char* const re[] = {"MY {REL} DIED IN {T} WITH {OBJ} ON {REL_P}.",
                                         "WHEN MY {REL} WENT INTO {T} AND NEVER CAME OUT, {OBJ} WENT WITH {REL_P}. THEY SAY IT LIES IN AN OLD CHEST DOWN THERE.",
                                         "{OBJ} IS ALL THAT IS LEFT OF MY {REL}, AND IT SITS IN A CHEST IN {T}, {W} {D}."};
        static const char* const as[] = {"BRING IT BACK TO ME.", "FIND THE CHEST AND BRING IT HOME."};
        title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as);
        desc = "{GIVER}'S {REL} WAS LOST IN {T} ({D} OF {H}). FIND {OBJ} IN AN OLD CHEST THERE AND BRING IT BACK.";
        return true;
      }
      case K_MISSING: {
        const int t = pickRadiant(SiteType::Cave, hx, hy, r, danger);
        if (t < 0) return false;
        q.type = QType::Missing;
        atSite(t);
        Actor d;
        Rng pr = personRng(q, seed);
        makeLook(d, Role::Villager, pr);
        q.subject = d.name;
        const bool fem = personFemale(q, seed);
        pron(fem);
        static const char* const relF[] = {"DAUGHTER", "SISTER", "WIFE", "NIECE"};
        static const char* const relM[] = {"SON", "BROTHER", "HUSBAND", "NEPHEW"};
        V.set("S", q.subject);
        V.set("REL2", fem ? pick(r, relF) : pick(r, relM));
        V.set("X", std::to_string(2 + r.irange(4)));
        V.set("LOOK", pick(r, kLookingFor));
        static const char* const ti[] = {"MISSING: {S}", "FIND {S}", "{S} HAS NOT COME HOME"};
        static const char* const re[] = {"MY {REL2} {S} WENT INTO {T} {X} DAYS AGO AND HAS NOT COME BACK.",
                                         "{S} WENT LOOKING FOR {LOOK} NEAR {T} AND NEVER CAME BACK. SOMEONE SAW A LIGHT IN THE CAVE.",
                                         "{S} WAS TAKEN. THE TRACKS LEAD TO {T}, {W} {D}."};
        static const char* const as[] = {"BRING {P_OBJ} HOME ALIVE.", "FIND {P_OBJ}. PLEASE."};
        title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as);
        desc = "{S}, {GIVER}'S {REL2}, WENT MISSING IN {T} ({D} OF {H}). FIND {P_OBJ} ALIVE AND LEAD {P_OBJ} OUT.";
        return true;
      }
      case K_PROTECT: {
        if (!(a.role == Role::Farmer || a.role == Role::Villager) || !(home.type == SiteType::Village || home.type == SiteType::Town)) return false;
        if (hour < 6.0f || hour >= 17.0f) return false;   // asked by day, for tonight
        if (inside) return false;
        // the fields: the farmland nearest the giver
        const int ax = (int)std::floor(a.home.x / TILE), ay = (int)std::floor(a.home.y / TILE);
        int best = -1, bd = 1 << 30;
        for (int y = ay - 40; y <= ay + 40; y++)
          for (int x = ax - 40; x <= ax + 40; x++) {
            if (world.over.at(x, y) != Ground::Farmland) continue;
            const int d = (x - ax) * (x - ax) + (y - ay) * (y - ay);
            if (d < bd) { bd = d; best = y * World::WIN + x; }
          }
        if (best < 0) return false;
        q.type = QType::Protect;
        q.target = a.site; q.targetId = home.id;
        q.hasPos = true; q.tgx = world.ox + best % World::WIN; q.tgy = world.oy + best / World::WIN;
        q.deadlineDay = day;
        const Biome b = world.over.biomeAt(best % World::WIN, best / World::WIN);
        V.set("M", zonePlural(protectZone(q, b), b));
        static const char* const ti[] = {"HOLD THE FIELDS", "A LONG NIGHT IN {H}", "THE {M} AT HARVEST"};
        static const char* const re[] = {"{M} HAVE BEEN AT OUR FIELDS EVERY NIGHT THIS WEEK, AND TONIGHT THEY WILL COME IN FORCE.",
                                         "THE {M} SMELL THE HARVEST. THEY WILL COME FOR THE FIELDS TONIGHT.",
                                         "LAST NIGHT {M} KILLED TWO OF OUR DOGS. TONIGHT THEY WILL COME FOR THE REST OF US."};
        static const char* const as[] = {"STAND WITH ME AT THE FIELDS FROM DUSK TILL DAWN.", "GUARD THE FIELDS TONIGHT, AND KEEP ME ALIVE TILL DAWN."};
        static const char* const de[] = {"{M} WILL RAID THE FIELDS OF {H} TONIGHT. GUARD THEM FROM NIGHTFALL UNTIL DAWN, AND KEEP {GIVER} ALIVE.",
                                         "TONIGHT {M} COME FOR THE FIELDS OF {H}. STAND WITH {GIVER} THROUGH THE DARK AND SEE THE SUN COME UP.",
                                         "{GIVER} FEARS FOR THE HARVEST OF {H}: {M} WILL STRIKE AFTER DUSK. HOLD THE FIELDS TILL MORNING.",
                                         "THE FIELDS OF {H} WILL NOT SEE ANOTHER DAWN IF {M} HAVE THEIR WAY. KEEP WATCH WITH {GIVER} ALL NIGHT."};
        title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as); desc = pickN(r, de);
        return true;
      }
    }
    return false;
  };
  OfferKind made = K_HUNT;
  for (OfferKind k : menu) {
    const Quest keep = q;
    if (build(k)) { ok = true; made = k; break; }
    q = keep;
    danger = false;
  }
  if (!ok) {
    if (want != QType::COUNT) return q;
    ok = build(K_HUNT);   // a hunt always fits: the beasts around home
    made = K_HUNT;
  }
  // the reward: by the place and its danger (VISION_PLAN 15.9: not by the player's level)
  const int lvl = std::max(home.level, targetLevel);
  int g = 60 + lvl * 22 + r.irange(40), xp = 40 + lvl * 14;
  switch (made) {
    case K_CAVE: case K_RUIN: case K_BOUNTY: g += 80; xp += 50; break;
    case K_DELIVER: g = 30 + lvl * 8 + (int)(std::hypot((float)(q.tgx - hx), (float)(q.tgy - hy)) * 0.15f) + r.irange(20); xp = 30 + lvl * 8; break;
    case K_HEIR: g += 70; xp += 50; break;
    case K_MISSING: g += 100; xp += 70; break;
    case K_NAMED: g += 130; xp += 80; break;
    case K_PROTECT: g += 110; xp += 80; break;
    default: break;
  }
  if (danger) { q.flags |= QF_DANGER; g = g * 3 / 2; xp = xp * 3 / 2; }
  q.gold = g; q.xp = xp;
  V.set("G", std::to_string(g));
  // the pitch: mood x reason x ask x (danger) x reward
  const char* mood = nullptr;
  switch (a.role) {
    case Role::Guard: mood = pick(r, kMoodGuard); break;
    case Role::Jarl: mood = pick(r, kMoodJarl); break;
    case Role::Innkeeper: mood = pick(r, kMoodInn); break;
    case Role::Priest: case Role::Mage: mood = pick(r, kMoodPriest); break;
    case Role::Farmer: mood = pick(r, kMoodFarmer); break;
    case Role::Hunter: case Role::Fisher: case Role::Herbalist: mood = pick(r, kMoodHunter); break;
    case Role::Merchant: case Role::Smith: mood = pick(r, kMoodTrade); break;
    default: mood = pick(r, kMoodAny); break;
  }
  const char* reward = (K && (a.role == Role::Jarl || a.role == Role::Guard) && r.f() < 0.5f) ? pick(r, kRewardCrown) : pick(r, kReward);
  if (made == K_DELIVER) reward = "{S} PAYS {G} GOLD WHEN IT ARRIVES.";
  std::string pitch = std::string(mood) + " " + reason + " " + ask;
  if (danger) pitch += std::string(" ") + pick(r, kDanger);
  pitch += std::string(" ") + reward;
  pendingPitch_ = V.fill(pitch);
  q.title = V.fill(title);
  q.desc = V.fill(desc) + (danger ? " IT IS DANGEROUS COUNTRY." : "");
  return q;
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
  dlg.opts = {{"I'LL DO IT.", A_ACCEPT, 0}, {"NOT RIGHT NOW.", A_DECLINE, 0}};
  mode = Mode::Dialogue;
  return true;
}

Quest Game::makeOffer(const Actor& npc) {
  bool ok = false;
  return offerFor(npc, QType::COUNT, ok);
}

// ---------------------------------------------------------------- markers and journal lines
bool Game::questTargetM2(const Quest& q, int& tx, int& ty) const {
  if (q.state != QState::Active) return false;
  auto global = [&](int32_t gx, int32_t gy) { tx = gx - world.ox; ty = gy - world.oy; return true; };
  auto atSite = [&]() {
    if (q.target >= 0 && q.target < (int)world.sites.size()) { tx = world.sites[(size_t)q.target].ex; ty = world.sites[(size_t)q.target].ey; return true; }
    return q.hasPos ? global(q.tgx, q.tgy) : false;
  };
  switch (q.type) {
    case QType::Deliver:
      if (q.destBldg >= 0 && q.destBldg < (int)world.over.bldgs.size()) {
        tx = world.over.bldgs[(size_t)q.destBldg].doorX(); ty = world.over.bldgs[(size_t)q.destBldg].doorY();
        return true;
      }
      return atSite();
    case QType::Heirloom: case QType::NamedBandit: return atSite();
    case QType::Missing: return atSite();
    case QType::Protect: return q.hasPos ? global(q.tgx, q.tgy) : atSite();
    default: return false;
  }
}

std::string Game::questStatusM2(const Quest& q) const {
  if (q.state == QState::Done) return "";
  int px, py;
  overworldTile(*this, px, py);
  int tx = 0, ty = 0;
  const bool has = questTargetM2(q, tx, ty);
  const std::string dl = has ? " (" + dirWord(tx - px, ty - py, false) + ")" : std::string(), ds = has ? " (" + dirWord(tx - px, ty - py, true) + ")" : std::string();
  const std::string tname = q.target >= 0 && q.target < (int)world.sites.size() ? world.sites[(size_t)q.target].name : std::string();
  if (q.state == QState::Complete && (q.flags & QF_GRAVE)) {
    int gx = 0, gy = 0;
    if (q.giverSite >= 0 && q.giverSite < (int)world.sites.size()) { gx = world.sites[(size_t)q.giverSite].ex; gy = world.sites[(size_t)q.giverSite].ey; }
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
    case QType::Protect: {
      const bool night = (day == q.deadlineDay && hour >= 20.5f) || (day == q.deadlineDay + 1 && hour < 5.5f);
      const std::string home = giverTown(world, q);
      if (night) return fitLine({"HOLD THE FIELDS UNTIL DAWN" + ds, "HOLD THE FIELDS UNTIL DAWN"});
      return fitLine({"AT NIGHTFALL: THE FIELDS OF " + home + ds, "AT NIGHTFALL: THE FIELDS" + ds, "GUARD THE FIELDS AT NIGHT"});
    }
    default: return "";
  }
}

std::string Game::turnInLine(const Quest& q) const {
  const std::string g = std::to_string(q.gold);
  const std::string tname = q.target >= 0 && q.target < (int)world.sites.size() ? world.sites[(size_t)q.target].name : std::string();
  switch (q.type) {
    case QType::Heirloom: return "MY " + q.subject + "! I NEVER THOUGHT I WOULD HOLD IT AGAIN. HERE: " + g + " GOLD, AND MY BLESSING.";
    case QType::Missing: return q.subject + " CAME HOME! I DON'T KNOW HOW TO THANK YOU. TAKE " + g + " GOLD. IT IS NOT ENOUGH.";
    case QType::NamedBandit: return "SO " + q.subject + " IS DEAD. THE ROADS WILL BREATHE EASIER. THE BOUNTY IS " + g + " GOLD.";
    case QType::Protect: return "WE SAW THE FIELDS AT DAWN, STILL STANDING. YOU HELD THE NIGHT. " + g + " GOLD, AS PROMISED.";
    case QType::Hunt: return std::string("YOU'RE BACK, AND THE ") + monsterPlural(q.mon) + " ARE THINNED OUT. I OWE YOU " + g + " GOLD.";
    default:
      return (tname.empty() ? std::string("YOU'RE BACK.") : "YOU'RE BACK! THEY SAY " + tname + " HAS GONE QUIET.") + " I OWE YOU " + g + " GOLD.";
  }
}

// ---------------------------------------------------------------- the people of quests
bool Game::isRecipient(const Quest& q, const Actor& a) const {
  return q.type == QType::Deliver && q.state == QState::Active && inside && subBldg >= 0 && subBldg == q.destBldg && a.npc &&
         a.st != AState::Dead && a.role == questRecipientRole(q);
}

int Game::escortFor(const Quest& q) const {
  for (size_t k = 1; k < actors.size(); k++) if (actors[k].quest == q.id && actors[k].npc && actors[k].st != AState::Dead) return (int)k;
  return -1;
}

void Game::spawnEscort(Quest& q, Vec2 at) {
  Spawn sp;
  sp.npc = true; sp.role = Role::Villager; sp.site = -1; sp.slot = -1;
  spawnHuman(sp, at);
  Actor& a = actors.back();
  Rng pr = personRng(q, seed);
  makeLook(a, Role::Villager, pr);
  a.name = q.subject;
  a.quest = q.id;
  a.fromMap = false; a.militia = false;
  a.speed = 60;
}

void Game::questFail(Quest& q, const std::string& why) {
  q.state = QState::Done;
  q.flags |= QF_FAILED;
  for (int i = (int)inv.size() - 1; i >= 0; i--) if (inv[(size_t)i].kind == ItemKind::Quest && inv[(size_t)i].questId == q.id) dropItem(i);
  emit(Ev::QuestUpdate, pl().p, q.id, 3, "QUEST FAILED: " + q.title);
  say(why);
  sfx((int)Sfx::MenuBack, pl().p, 0.7f);
  if (trackedQuest == q.id) {
    trackedQuest = -1;
    for (auto& o : quests) if (o.state != QState::Done && o.type != QType::Main) { trackedQuest = o.id; break; }
    if (trackedQuest < 0) for (auto& o : quests) if (o.state != QState::Done) { trackedQuest = o.id; break; }
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
  int best = -1, bd = -1;
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
    bool open = m.propAt(cx, cy) == 0;
    for (int oy = -1; oy <= 1 && open; oy++)
      for (int ox = -1; ox <= 1 && open; ox++) if (m.blocked(cx + ox, cy + oy)) open = false;
    if (avoid >= 0 && std::abs(cx - avoid % m.w) + std::abs(cy - avoid / m.w) < 6) open = false;
    for (const Spawn& sp : m.spawns) if (sp.x == cx && sp.y == cy) open = false;
    if (open && dist[(size_t)qq[h]] > bd) { bd = dist[(size_t)qq[h]]; best = qq[h]; }
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

Vec2 Game::questChestTile(const Quest& q) const {
  auto it = marks.find(markKey((uint64_t)q.id, Mk::HeirChest));
  const int idx = it != marks.end() ? it->second : farTile(sub, -1);
  if (idx < 0) return Vec2(-1, -1);
  return Vec2((float)(idx % sub.w), (float)(idx / sub.w));
}

// a cave, ruin or building map was just made: put the quests' chests and people in it
void Game::questMapLoaded() {
  if (!inside) return;
  bool solidDirty = false;
  if (subSite >= 0) {
    int chestIdx = -1;
    for (Quest& q : quests) {
      if (q.state != QState::Active || q.target != subSite) continue;
      if (q.type == QType::Heirloom && !(q.flags & QF_FOUND)) {
        const uint64_t mk = markKey((uint64_t)q.id, Mk::HeirChest);
        auto it = marks.find(mk);
        int idx = it != marks.end() ? it->second : farTile(sub, -1);
        if (idx < 0 || idx >= sub.w * sub.h) continue;
        marks[mk] = idx;
        chestIdx = idx;
        const bool opened = looted.count(((uint64_t)mapKey() << 32) | (uint32_t)idx) > 0;
        sub.prop[(size_t)idx] = (uint8_t)((int)(opened ? Prop::ChestOpen : Prop::Chest) + 1);
        solidDirty = true;
        if (!opened) emit(Ev::Notice, pl().p, (int)rgba(255, 220, 140), 0, "SOMEWHERE IN HERE: " + q.subject);
      }
    }
    if (solidDirty) sub.rebuildSolid();
    for (Quest& q : quests) {
      if (q.state != QState::Active || q.target != subSite || q.type != QType::Missing || escortFor(q) >= 0) continue;
      if (q.flags & QF_FOUND) spawnEscort(q, freeSpot((int)std::floor(pl().p.x / TILE), (int)std::floor(pl().p.y / TILE) + 1));
      else {
        const int idx = farTile(sub, chestIdx);
        if (idx < 0) continue;
        spawnEscort(q, Vec2((idx % sub.w) * TILE + 8.0f, (idx / sub.w) * TILE + 10.0f));
        emit(Ev::Notice, pl().p, (int)rgba(255, 220, 140), 0, "SOMEWHERE IN HERE: " + q.subject);
      }
    }
  }
  if (subBldg >= 0) {
    // a parcel's recipient: whoever keeps this place (the innkeeper, the smith, the merchant) wears the name the giver
    // gave; when nobody of that trade is here, the first person in the building is the one the parcel is for
    for (Quest& q : quests) {
      if (q.type != QType::Deliver || q.state != QState::Active || q.destBldg != subBldg) continue;
      int k = -1;
      for (size_t j = 1; j < actors.size(); j++) if (actors[j].npc && actors[j].role == questRecipientRole(q)) { k = (int)j; break; }
      if (k < 0)
        for (size_t j = 1; j < actors.size(); j++)
          if (actors[j].npc && actors[j].human) { k = (int)j; q.flags = (q.flags & ~0xFF00u) | ((uint32_t)actors[j].role << 8); break; }
      if (k < 0) continue;
      Actor& a = actors[(size_t)k];
      Rng pr = personRng(q, seed);
      makeLook(a, a.role, pr);
      a.name = q.subject;
    }
  }
}

// the player came out of a site: a missing person who followed them out is safe
void Game::questLeftSite(int si) {
  for (Quest& q : quests) {
    if (q.type != QType::Missing || q.state != QState::Active || q.target != si || !(q.flags & QF_FOUND)) continue;
    q.state = QState::Complete;
    q.flags |= QF_ESCORTED;
    spawnEscort(q, freeSpot((int)std::floor(pl().p.x / TILE) + 1, (int)std::floor(pl().p.y / TILE) + 1));
    emit(Ev::Text, actors.back().p + Vec2(0, -24), (int)rgba(255, 240, 200), 0, "DAYLIGHT! THANK YOU!");
    const std::string m = q.subject + " IS SAFE. " + rewardReadyMsg(world, q);
    emit(Ev::QuestUpdate, pl().p, q.id, 2, m);
    say(m);
    sfx((int)Sfx::QuestStart, pl().p, 1.2f);
  }
}

bool Game::questChest(int tx, int ty) {
  if (!inside || subSite < 0) return false;
  const int idx = ty * sub.w + tx;
  for (Quest& q : quests) {
    if (q.type != QType::Heirloom || q.state != QState::Active || q.target != subSite) continue;
    auto it = marks.find(markKey((uint64_t)q.id, Mk::HeirChest));
    if (it == marks.end() || it->second != idx) continue;
    sub.prop[(size_t)idx] = (uint8_t)((int)Prop::ChestOpen + 1);
    looted.insert(lootKey(tx, ty));
    sub.rebuildSolid();
    sfx((int)Sfx::Chest, pl().p);
    Item it2;
    it2.kind = ItemKind::Quest; it2.name = q.subject; it2.icon = keepsakeIcon(q.subject); it2.rarity = Rarity::Rare;
    it2.questId = q.id; it2.value = 0; it2.tint = rgba(236, 206, 120);
    addItem(it2);
    Pickup g; g.p = Vec2(tx * TILE + 8.0f, ty * TILE + 18.0f); g.gold = 10 + world.sites[(size_t)subSite].level * 3; pickups.push_back(g);
    q.flags |= QF_FOUND;
    q.state = QState::Complete;
    const std::string m = (q.flags & QF_GRAVE) ? rewardReadyMsg(world, q) : "FOUND THE " + q.subject + ". " + rewardReadyMsg(world, q);
    emit(Ev::QuestUpdate, pl().p, q.id, 2, m);
    say(m);
    return true;
  }
  return false;
}

// someone just streamed in outdoors: a named bandit chief takes the name and the strength the bounty gave him
void Game::questSpawned(Actor& a) {
  if (inside || !a.boss || a.role != Role::Bandit || a.site < 0) return;
  for (const Quest& q : quests) {
    if (q.type != QType::NamedBandit || q.state != QState::Active || q.target != a.site) continue;
    Rng pr = personRng(q, seed);
    const art::HumanLook old = a.look;
    makeLook(a, Role::Bandit, pr);
    a.look.outfit = art::Outfit::Plate; a.look.helmet = old.helmet; a.look.weapon = old.weapon; a.look.cape = true;
    a.look.tabardColor = rgba(120, 28, 30);
    a.name = q.subject;
    a.maxHp *= 1.35f; a.hp = a.maxHp; a.dmg *= 1.2f; a.xp *= 2;
    a.quest = q.id;
    return;
  }
}

void Game::questActorDown(const Actor& a) {
  for (Quest& q : quests) {
    if (q.state != QState::Active) continue;
    if (q.type == QType::Protect && isGiver(q, a)) questFail(q, q.giverName + " IS DEAD. THE FIELDS OF " + giverTown(world, q) + " ARE LOST.");
    else if (q.type == QType::Missing && a.quest == q.id && a.npc) questFail(q, q.subject + " DID NOT SURVIVE.");
    else if (q.type == QType::Protect && a.quest == q.id && a.hostile) q.have++;   // a raider of the current wave down
  }
}

// once per step: Deliver recipients found on arrival, the Protect night, escorts that have gone home
void Game::questTick(float dt) {
  (void)dt;
  for (Quest& q : quests) {
    if (q.state != QState::Active) continue;
    if (q.type == QType::Deliver && q.destBldg < 0 && q.target >= 0 && q.target < (int)world.sites.size()) {
      const Site& T = world.sites[(size_t)q.target];
      if (T.bldgCount <= 0) continue;
      int pickB = -1;
      for (int b = T.bldgFirst; b < T.bldgFirst + T.bldgCount && b < (int)world.over.bldgs.size(); b++)
        if (world.over.bldgs[(size_t)b].type == roleBuilding(questRecipientRole(q))) { pickB = b; break; }
      if (pickB < 0)
        for (int b = T.bldgFirst; b < T.bldgFirst + T.bldgCount && b < (int)world.over.bldgs.size(); b++)
          if (world.over.bldgs[(size_t)b].type == art::Building::Inn) { pickB = b; q.flags = (q.flags & ~0xFF00u) | ((uint32_t)Role::Innkeeper << 8); break; }
      if (pickB >= 0) {
        q.destBldg = pickB;
        q.hasPos = true;
        q.tgx = world.ox + world.over.bldgs[(size_t)pickB].doorX(); q.tgy = world.oy + world.over.bldgs[(size_t)pickB].doorY();
      }
    }
    if (q.type != QType::Protect) continue;
    const bool after = day > q.deadlineDay + 1 || (day == q.deadlineDay + 1 && hour >= 5.5f);
    const bool night = (day == q.deadlineDay && hour >= 20.5f) || (day == q.deadlineDay + 1 && hour < 5.5f);
    if (after) {
      // held only when both waves came and the second was beaten, or the player still stood by the fields at dawn
      // with it (hiding indoors after the first wave, or walking off, does not count)
      bool held = false;
      if (q.flags & QF_WAVE2) {
        const Vec2 fp((q.tgx - world.ox) * TILE + 8.0f, (q.tgy - world.oy) * TILE + 10.0f);
        held = q.have >= q.need || (!inside && len2(pl().p - fp) <= (45.0f * TILE) * (45.0f * TILE));
      }
      if (held) {
        q.state = QState::Complete;
        for (size_t k = 1; k < actors.size(); k++)
          if (actors[k].quest == q.id && actors[k].hostile && actors[k].st != AState::Dead) { actors[k].aggro = false; actors[k].wild = true; actors[k].home = actors[k].p + (actors[k].p - pl().p) * 4.0f; }
        const std::string m = "DAWN BREAKS OVER " + giverTown(world, q) + ". THE FIELDS ARE SAFE. " + rewardReadyMsg(world, q);
        emit(Ev::QuestUpdate, pl().p, q.id, 2, m);
        say(m);
        sfx((int)Sfx::QuestDone, pl().p, 1.1f);
      } else {
        const Biome b = world.over.biomeAt(q.tgx - world.ox, q.tgy - world.oy);
        questFail(q, std::string("THE ") + zonePlural(protectZone(q, b), b) + " RAVAGED THE FIELDS OF " + giverTown(world, q) + " WHILE YOU WERE AWAY.");
      }
      continue;
    }
    if (!night || inside) continue;
    const int fx = q.tgx - world.ox, fy = q.tgy - world.oy;
    const Vec2 fp(fx * TILE + 8.0f, fy * TILE + 10.0f);
    if (len2(pl().p - fp) > (45.0f * TILE) * (45.0f * TILE)) continue;
    // the waves: q.need raiders in the current wave, q.have of them felled. Raiders that vanished (the player went
    // indoors, a reload) are still out there: the missing ones come back; the next wave comes once this one is down.
    int alive = 0;
    for (size_t k = 1; k < actors.size(); k++) if (actors[k].quest == q.id && actors[k].hostile && actors[k].st != AState::Dead) alive++;
    const bool first = !(q.flags & QF_WAVE1);
    if (alive > 0) continue;
    const bool regroup = !first && q.have < q.need;
    if (!first && !regroup && (q.flags & QF_WAVE2)) continue;   // both waves are down: dawn will tell
    // a wave: from the wild side of the fields (away from the heart of the village), aimed at the fields
    const Biome b = world.over.biomeAt(fx, fy);
    const int z = protectZone(q, b);
    Vec2 out(1, 0);
    if (q.target >= 0 && q.target < (int)world.sites.size()) {
      const Site& S = world.sites[(size_t)q.target];
      const Vec2 d((float)(fx - S.ex), (float)(fy - S.ey));
      if (len2(d) > 0.5f) out = norm(d);
    }
    const int lvl = std::max(1, world.zoneLevel(fx, fy));
    const bool second = !first && !regroup;
    const int n = regroup ? q.need - q.have : std::min(7, (first ? 3 : 4) + lvl / 3);
    if (!regroup) { q.need = n; q.have = 0; }
    for (int i = 0; i < n; i++) {
      const float ang = (i - (n - 1) * 0.5f) * 0.28f;
      const Vec2 dir(out.x * std::cos(ang) - out.y * std::sin(ang), out.x * std::sin(ang) + out.y * std::cos(ang));
      const Vec2 at0 = fp + dir * (17.0f * TILE);
      const Vec2 at = freeSpot((int)std::floor(at0.x / TILE), (int)std::floor(at0.y / TILE));
      int id;
      if (z == 2) {
        Spawn sp;
        sp.bandit = true; sp.site = q.target; sp.slot = -1;
        id = spawnHuman(sp, at);
      } else id = spawnMonster(zoneMonster(z, b), at, lvl, false);
      const int k = findActor(id);
      if (k < 0) continue;
      Actor& m = actors[(size_t)k];
      m.quest = q.id; m.aggro = true; m.fromMap = false; m.site = -1; m.home = fp; m.goal = fp;
    }
    if (regroup) continue;
    (void)second;
    q.flags |= first ? QF_WAVE1 : QF_WAVE2;
    const std::string m = first ? std::string("THE ") + zonePlural(z, b) + " ARE COMING! HOLD THE FIELDS UNTIL DAWN!" : "MORE OF THEM! HOLD THE LINE!";
    emit(Ev::Notice, pl().p, (int)rgba(255, 110, 80), 0, m);
    say(m);
    sfx((int)Sfx::Bell, pl().p, 1.0f, 0.8f);
  }
  // a missing person brought home walks off once out of sight
  for (size_t k = 1; k < actors.size();) {
    const Actor& a = actors[k];
    if (a.quest > 0 && a.npc) {
      const Quest* q = questById(a.quest);
      const bool following = q && q->state == QState::Active;
      if (!following && len2(a.p - pl().p) > (14.0f * TILE) * (14.0f * TILE)) { actors.erase(actors.begin() + (std::ptrdiff_t)k); continue; }
    }
    k++;
  }
}

// M2 quest talk: options for the people of the new quest types. Returns a line that replaces the greeting ("" none).
std::string Game::questDialogue(Actor& a) {
  std::string text;
  for (Quest& q : quests) {
    if (isRecipient(q, a)) {
      const std::string from = q.giverName + (giverTown(world, q).empty() ? std::string() : " IN " + giverTown(world, q));
      text = "A PARCEL? FROM " + from + "? I'VE BEEN WAITING ON THIS FOR WEEKS.";
      dlg.opts.push_back({"HAND OVER THE PARCEL (+" + std::to_string(q.gold) + " GOLD)", A_DELIVER, q.id});
    }
  }
  return text;
}
