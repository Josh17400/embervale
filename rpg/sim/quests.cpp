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
#include "rpg/sim/jobs_ops.h"
#include "rpg/world/source.h"
#include "rpg/world/wildlife.h"

using art::Monster;
using art::Prop;

// (P1, life_quests.cpp) a Supply quest for a trade's good from the nearest loaded neighbour that sells it
bool jobsSupplyQuest(const Game& g, const Actor& npc, ew::Good good, life::Job job, Quest& q);

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
// (M7b integration) a place named for what it is ("TOMB OF SHENSHA", "CAVES OF IRON") is spoken with its article:
// "WENT INTO THE TOMB OF SHENSHA", and once named it is "THE PLACE"; a proper name ("SESAI CAVERN") is not
std::string spokenPlace(const std::string& n) {
  if (n.empty() || n.rfind("THE ", 0) == 0 || n.find(" OF ") == std::string::npos) return n;
  return "THE " + n;
}

const char* walkWords(float tiles) {
  const float h = tiles / 30.0f;
  if (h < 0.75f) return "LESS THAN AN HOUR'S WALK";
  if (h < 1.5f) return "AN HOUR'S WALK";
  if (h < 4.5f) return "A FEW HOURS' WALK";
  if (h < 9.0f) return "HALF A DAY'S WALK";
  if (h < 18.0f) return "A DAY'S WALK";
  return "SEVERAL DAYS' WALK";
}

// ---- the grammar (M7b: each opener a whole sentence in the speaker's own trade and tone, so the pitch reads as one
// person talking: why they ask, what happened, where, what they want, what they pay)
const char* const kMoodAny[] = {
    "I HATE TO ASK A STRANGER, BUT I'M OUT OF FRIENDS TO ASK.", "YOU LOOK LIKE SOMEONE WHO CAN HANDLE A BLADE, AND I NEED SOMEONE LIKE THAT.",
    "I'LL BE PLAIN WITH YOU: I NEED HELP.", "YOU'RE NOT FROM {H}, ARE YOU? GOOD. NOBODY HERE WILL DO THIS.",
    "IF IT'S WORK YOU'RE AFTER, I HAVE SOME.", "THE GODS MUST HAVE SENT YOU. I'M AT MY WIT'S END."};
const char* const kMoodGuard[] = {"THE WATCH IS STRETCHED THIN, SO I'LL TAKE ANY BLADE I CAN GET.", "THIS STAYS BETWEEN US, TRAVELLER.",
                                  "THE CAPTAIN WON'T SPARE THE MEN, SO I'M ASKING YOU."};
const char* const kMoodJarl[] = {"{H} HAS NEED OF YOU.", "MY OWN GUARD IS NEEDED HERE, BUT YOU ARE FREE TO GO WHERE THEY CANNOT.",
                                 "SERVE {H} WELL AND {H} WILL REMEMBER IT."};
const char* const kMoodInn[] = {"YOU HEAR THINGS, KEEPING AN INN, AND I DON'T LIKE WHAT I'M HEARING.",
                                "SIT DOWN A MOMENT. THE NEXT MUG IS ON THE HOUSE IF YOU HEAR ME OUT.",
                                "EVERY TRAVELLER AT MY TABLES TELLS THE SAME STORY LATELY."};
const char* const kMoodPriest[] = {"THE GODS ASK MUCH OF US. TODAY THEY ASK SOMETHING OF YOU.", "I PRAYED FOR HELP, AND HERE YOU STAND.",
                                   "A WEIGHT LIES ON THIS TEMPLE, CHILD."};
const char* const kMoodFarmer[] = {"IT'S BEEN A HARD SEASON, AND IT'S ABOUT TO GET HARDER.", "I'M A FARMER, NOT A FIGHTER. YOU LOOK LIKE ONE.",
                                   "THE HARVEST WON'T WAIT, AND NEITHER WILL THIS."};
const char* const kMoodHunter[] = {"YOU WALK QUIETER THAN MOST TOWNSFOLK. GOOD.", "THE WOODS ARE UNEASY, IF YOU KNOW HOW TO LISTEN."};
const char* const kMoodTrade[] = {"I'VE A JOB FOR STEADY HANDS, AND IT PAYS.", "TIME IS COIN, SO I'LL BE QUICK.",
                                  "HEAR ME OUT. I'LL MAKE IT WORTH YOUR WHILE."};

const char* const kReward[] = {"THERE'S {G} GOLD IN IT FOR YOU.", "I CAN PAY {G} GOLD. IT'S ALL I HAVE.", "{G} GOLD, AND MY THANKS.",
                               "{G} GOLD WHEN IT'S DONE. HALF OF US HERE CHIPPED IN.", "DO THIS AND {G} GOLD IS YOURS.",
                               "I'LL PAY {G} GOLD WHEN YOU'RE BACK."};
const char* const kRewardTrade[] = {"{G} GOLD, PAID THE DAY IT'S DONE.", "I'LL PAY {G} GOLD. A FAIR PRICE, AND YOU KNOW IT.",
                                    "{G} GOLD. DON'T HAGGLE: I'M THE MERCHANT HERE."};
const char* const kRewardCrown[] = {"THE CROWN OF {K} PAYS {G} GOLD FOR WORK LIKE THIS.", "{G} GOLD FROM THE TREASURY WHEN IT'S DONE."};
const char* const kDanger[] = {"BE WARNED: IT'S DANGEROUS COUNTRY, SO I'LL PAY HALF AGAIN FOR THE RISK.",
                               "IT'S DANGEROUS COUNTRY OUT THERE, SO THE PRICE IS HALF AGAIN."};

const char* const kRelK[] = {"GRANDMOTHER", "GRANDFATHER", "FATHER", "MOTHER", "UNCLE", "AUNT", "BROTHER", "SISTER", "HUSBAND", "WIFE"};
const bool kRelFemale[] = {true, false, false, true, false, true, false, true, false, true};
const char* const kKeepsake[] = {"SILVER RING", "GOLD LOCKET", "IVORY COMB", "OLD SWORD", "WEDDING BAND", "SIGNET RING", "CARVED HORN",
                                 "AMBER PENDANT", "BRASS COMPASS", "PRAYER BEADS", "WAR MEDAL", "HUNTING KNIFE"};
const char* const kEpithet[] = {"THE RED", "THE GRIM", "THE CRUEL", "ONE-EYE", "BLACKHAND", "THE BUTCHER", "IRONJAW", "THE WOLF",
                                "SILVERTONGUE", "THE BURNED", "HALFHAND", "THE CROW"};
const char* const kLookingFor[] = {"MUSHROOMS", "A LOST GOAT", "SILVER", "FIREWOOD", "THE OLD DOG", "BLUE MOUNTAIN FLOWERS"};
const char* const kStrayBeast[] = {"HEIFER", "GOAT", "RAM", "MARE", "OX", "SOW"};

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
// (M3c LIFE) a hunt's reason in the land's own terms ({M} the beasts, {E} the biome's name; (fixer M7b r2) the giver's
// own home said as "HERE", "OUR", never by its name);
// nullptr where the classic lines fit (the temperate meadows and downs)
const char* huntFlavour(Eco e, uint64_t k) {
  static const char* const desert[] = {"THE {M} CAME OUT OF THE {E} AND STUNG A CAMEL DRIVER JUST OUTSIDE OUR WALLS.",
                                       "NOTHING MOVES IN THE {E} BY DAY BUT THE {M}. NOW THEY COME TO OUR WELL BY NIGHT.",
                                       "THE CARAVANS WON'T CROSS THE {E} WHILE THE {M} ARE OUT."};
  static const char* const grass[] = {"THE {M} CIRCLE OUR HERDS EVERY DUSK.", "YOU CAN HEAR THE {M} OUT ON THE {E} ALL NIGHT. THEY HAVE OUR SCENT.",
                                      "WE LOST TWO GOATS TO THE {M} IN THE LONG GRASS."};
  static const char* const cold[] = {"THE {M} CAME DOWN OFF THE {E} WITH THE BLIZZARD, AND THEY ARE STARVING.",
                                     "TRACKS IN THE SNOW BY THE WOODPILE AGAIN. THE {M}.", "THE ICE FISHERS WON'T GO OUT WHILE THE {M} ROAM."};
  static const char* const wet[] = {"THE {M} TOOK A NET-MENDER AT THE WATER'S EDGE, A STONE'S THROW FROM HERE.", "NOBODY WADES THE {E} NOW. THE {M} ARE TOO MANY.",
                                    "SOMETHING DRAGS OUR EEL TRAPS UNDER. THE {M}, THE OLD ONES SAY."};
  static const char* const wood[] = {"THE {M} OF THE {E} ARE TAKING THE CHARCOAL BURNERS' DOGS.",
                                     "THE WOODCUTTERS WON'T GO INTO THE {E} WHILE THE {M} ARE ABOUT.", "THE {M} HAVE THE FORAGERS TOO FRIGHTENED TO LEAVE HOME."};
  static const char* const ash[] = {"THE {M} RUN THE {E} IN BURNING PACKS. ONE SET A BARN ALIGHT.", "THE {M} COME OUT OF THE ASH WITH EMBERS IN THEIR FUR."};
  static const char* const blight[] = {"THE {E} SPREADS, AND THE {M} SPREAD WITH IT.", "THE {M} CAME OUT OF THE ROTTEN GROUND. THE CROPS WITHER WHERE THEY WALK."};
  static const char* const coast[] = {"THE {M} HAVE BEEN AT THE NETS ON THE SHORE BELOW US.", "THE GULLS GO QUIET WHEN THE {M} COME UP THE BEACH."};
  static const char* const eerie[] = {"LIGHTS IN THE {E} AT NIGHT, AND THE {M} AMONG THEM. A CHILD FOLLOWED ONE.",
                                      "THE {M} OF THE {E} LEAD TRAVELLERS ASTRAY. SOME NEVER COME BACK."};
  auto pickA = [&](const char* const* a, int n) { return a[(int)(k % (uint64_t)n)]; };
  if (e == Eco::AshFields) return pickA(ash, 2);
  if (e == Eco::Blight || e == Eco::DarkForest) return pickA(blight, 2);
  if (ecoHas(e, EF_MAGIC)) return pickA(eerie, 2);
  const Biome f = ecoFamily(e);
  if (f == Biome::Beach) return pickA(coast, 2);
  if (f == Biome::Desert) return pickA(desert, 3);
  if (f == Biome::Snow || f == Biome::Taiga) return pickA(cold, 3);
  if (f == Biome::Swamp || e == Eco::LakeDistrict) return pickA(wet, 3);
  if (f == Biome::Forest || f == Biome::Autumn) return pickA(wood, 3);
  if (e == Eco::Savanna || e == Eco::Steppe || e == Eco::Prairie || e == Eco::Heath) return pickA(grass, 3);
  return nullptr;
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

enum OfferKind { K_CAVE, K_RUIN, K_BOUNTY, K_HUNT, K_DELIVER, K_HEIR, K_MISSING, K_NAMED, K_PROTECT, K_STOLEN, K_SUPPLY, K_COUNT };
QType kindType(OfferKind k) {
  switch (k) {
    case K_STOLEN: return QType::Heirloom;
    case K_SUPPLY: return QType::Supply;
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
  // (P1 Fresh Roads) the pool job's need (makeOffer sets it while this runs); the notice board's two posted jobs are
  // the town's pool's board jobs (whatever kind places.cpp asks for: the pool decides; none open: nothing posted)
  uint8_t need = jobBook.offerNeed;
  if (a.bldg == -1 && (a.slot == jobs::BOARD_SLOT0 || a.slot == jobs::BOARD_SLOT0 + 1) && world.sites[(size_t)a.site].settlement() && world.endless) {
    JobOps::ensurePool(*this, a.site);
    const jobs::PoolJob* bj = JobOps::openJobByKey(*this, key);
    if (!bj) return q;
    want = (QType)bj->qtype;
    need = bj->need;
  }
  const int week = day / 7;
  Rng r(((uint64_t)hash32((uint32_t)key * 31u + (uint32_t)done) << 24) ^ (key >> 9) ^ seed ^ 0x0FFE5ull);
  // (M2 fixer round 2) the grammar's words turn with the jobs this person has given: two offers of one kind a few jobs
  // apart never come out word for word the same (a giver repeated HOLD THE FIELDS)
  // (by the person, not the job's dice: the title cycles by 3 and the description by 4, so two offers of one kind
  // within a dozen jobs never share both)
  auto pickN = [&](Rng& rr, const auto& arr) {
    const int n = (int)(sizeof(arr) / sizeof(arr[0]));
    (void)rr.irange(n);   // (the dice still roll: the rest of the offer keeps its draws)
    // (P1) and with the week: a giver asked again next week (the notice board above all) says it another way
    return arr[(int)(((key >> 13) % (uint64_t)n + (uint64_t)done + (uint64_t)week) % (uint64_t)n)];
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
  // (P1) every trade has four kinds or more (VISION_PLAN 15.34): the weekly pool picks one by the land's needs, and
  // a person asked outside the pools (a test, a wayside keeper) turns through them
  switch (a.role) {
    case Role::Jarl: menu = {K_CAVE, K_RUIN, K_NAMED, K_MISSING, K_BOUNTY}; break;
    case Role::Guard: menu = {K_BOUNTY, K_NAMED, K_CAVE, K_HUNT}; break;
    case Role::Innkeeper: menu = {K_BOUNTY, K_HUNT, K_DELIVER, K_NAMED, K_HEIR}; break;
    case Role::Priest: menu = {K_RUIN, K_MISSING, K_HEIR, K_DELIVER}; break;
    case Role::Smith: menu = {K_HUNT, K_DELIVER, K_CAVE, K_SUPPLY}; break;
    case Role::Mage: menu = {K_RUIN, K_HEIR, K_DELIVER, K_CAVE}; break;
    case Role::Merchant: menu = {K_DELIVER, K_HEIR, K_BOUNTY, K_STOLEN}; break;
    case Role::Farmer: menu = {K_PROTECT, K_HUNT, K_MISSING, K_SUPPLY}; break;
    case Role::Hunter: menu = {K_HUNT, K_DELIVER, K_MISSING, K_CAVE}; break;
    case Role::Fisher: menu = {K_HUNT, K_DELIVER, K_MISSING, K_HEIR}; break;
    case Role::Herbalist: menu = {K_HEIR, K_MISSING, K_HUNT, K_CAVE}; break;
    default:
      // (P1 fixer) a scholar's work is books, relics and ruins (a SCHOLAR offered a wolves-at-the-sheep hunt)
      if (JobOps::censusJob(*this, a) == life::Job::Scholar) menu = {K_HEIR, K_RUIN, K_MISSING, K_DELIVER};
      else menu = {K_HUNT, K_HEIR, K_MISSING, K_DELIVER, K_PROTECT};
      break;
  }
  if (want != QType::COUNT) {
    menu.clear();
    if (want == QType::Clear) menu = need == jobs::JN_CAVE ? std::vector<OfferKind>{K_CAVE}
                                     : (need == jobs::JN_RUIN || need == jobs::JN_DESECRATED) ? std::vector<OfferKind>{K_RUIN} : std::vector<OfferKind>{K_CAVE, K_RUIN};
    else if (want == QType::Heirloom) menu = need == jobs::JN_STOLEN ? std::vector<OfferKind>{K_STOLEN} : std::vector<OfferKind>{K_HEIR};
    else
      for (int k = K_BOUNTY; k < K_COUNT; k++)
        if (kindType((OfferKind)k) == want && k != K_STOLEN) menu.push_back((OfferKind)k);
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
    V.set("T", spokenPlace(T.name));
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
        } else if (need == jobs::JN_DESECRATED) {   // (P1) a priest's: holy ground gone foul
          static const char* const ti[] = {"CLEANSE {T}", "HOLY GROUND DEFILED", "THE DEFILERS OF {T}", "A SHRINE GONE DARK"};
          static const char* const re[] = {"{T} WAS HOLY GROUND WHEN MY GRANDMOTHER WAS A GIRL. NOW SOMETHING UNCLEAN DWELLS THERE.",
                                           "THE OLD SHRINE IN {T} HAS BEEN DEFILED. THE CANDLES WE LEAVE THERE ARE FOUND SNUFFED AND BLACK.",
                                           "PILGRIMS WON'T WALK TO {T} ANY MORE. THEY SAY THE STONES WEEP AT NIGHT.",
                                           "WHATEVER SQUATS IN {T}, {W} {D} OF HERE, HAS MADE A MOCKERY OF THE GODS' HOUSE."};
          static const char* const as[] = {"DRIVE IT OUT AND THE GODS WILL KNOW YOUR NAME.", "SLAY WHAT DEFILES IT, SO WE CAN BLESS IT AGAIN."};
          static const char* const de[] = {"SOMETHING UNCLEAN DEFILES {T} ({D} OF {H}). SLAY ITS MASTER AND TELL {GIVER} THE GROUND IS CLEAN.",
                                           "{GIVER} OF {H} WANTS {T} CLEANSED ({D}). DESTROY WHAT DWELLS THERE AND RETURN."};
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
        if (k == K_BOUNTY && need == jobs::JN_RAIDERS) {   // (P1) a merchant's: the caravan robbers
          q.type = QType::Bounty;
          static const char* const ti[] = {"RAIDERS ON THE ROAD", "THE CARAVAN ROBBERS", "WHAT {T} TOOK", "A PRICE ON THE RAIDERS"};
          static const char* const re[] = {"MY LAST CARAVAN NEVER REACHED {H}. THE RAIDERS WHO TOOK IT HOLE UP AT {T}, {W} {D}.",
                                           "THREE WAGONS OF MINE ROBBED THIS SEASON, ALL BY THE SAME BAND OUT OF {T}.",
                                           "NO CARTER WILL TAKE MY GOODS PAST {N} WHILE THE RAIDERS OF {T} WATCH THE ROAD."};
          static const char* const as[] = {"KILL THEIR CHIEF AND THE ROAD IS MINE AGAIN.", "PUT THEIR CHIEF IN THE GROUND."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as);
          desc = "RAIDERS FROM {T} ({D} OF {H}) ROB {GIVER}'S CARAVANS. KILL THEIR CHIEF AND COLLECT THE BOUNTY FROM {GIVER}.";
        } else if (k == K_BOUNTY && need == jobs::JN_WARBAND) {   // (P1) the realm at war: deserters turned bandit
          q.type = QType::Bounty;
          static const char* const ti[] = {"DESERTERS AT {T}", "THE WAR'S LEFTOVERS", "TURNCOATS ON THE ROAD"};
          static const char* const re[] = {"THE WAR HAS FILLED THE WOODS WITH DESERTERS. A BAND OF THEM HOLDS {T}, AND THEY TAKE WHAT THEY LIKE.",
                                           "SOLDIERS WHO RAN FROM THE WAR CAMP AT {T} NOW, {W} {D}. THEY ROB THEIR OWN PEOPLE.",
                                           "WITH THE ARMY AWAY AT THE WAR, DESERTERS FROM {T} RAID OUR ROADS UNCHALLENGED."};
          static const char* const as[] = {"KILL THE ONE WHO LEADS THEM.", "END THEIR CAPTAIN AND THE REST WILL SCATTER."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as);
          desc = "DESERTERS HOLD {T} ({D} OF {H}) WHILE THE REALM IS AT WAR. KILL THEIR LEADER AND COLLECT THE BOUNTY FROM {GIVER}.";
        } else if (k == K_BOUNTY) {
          q.type = QType::Bounty;
          static const char* const ti[] = {"BOUNTY: {T}", "A PRICE ON {T}", "THE OUTLAWS OF {T}", "CUTTHROATS AT {T}"};
          static const char* const re[] = {"BANDITS AT {T} HAVE BEEN RAIDING THE ROADS {D} OF HERE.",
                                           "THE OUTLAWS OF {T} ROBBED A WAGON ON ITS WAY HERE LAST WEEK.",
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
          static const char* const re[] = {"{S} RULES {T} NOW. {P_SUBJ} BURNED OUR MILL LAST SPRING.",
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
        // (P1) a real target area: a den within a morning's walk (the nearer few, by the dice; not one another hunt
        // already points at), else a hunting ground out in the wilds; the quarry is the den's beasts or the land's
        int32_t ax = hx, ay = hy;
        std::string area;   // a landmark near it ("THE GREY TEETH"), else ""
        bool den = false;
        if (world.endless && world.src) {
          struct DenC { int64_t d2; ew::Gid id; int32_t x, y; Monster mon; };
          std::vector<DenC> dc;
          const int32_t rx0 = ew::regionOf(hx), ry0 = ew::regionOf(hy);
          for (int32_t ry = ry0 - 1; ry <= ry0 + 1; ry++)
            for (int32_t rx = rx0 - 1; rx <= rx0 + 1; rx++) {
              const ew::RegionPlan R = world.regionPlan(rx, ry);
              for (const ew::DenPlan& d : R.dens) {
                const int64_t dx = d.x - hx, dy = d.y - hy, d2 = dx * dx + dy * dy;
                if (d2 > 260 * 260 || d2 < 12 * 12) continue;
                bool taken = false;
                for (const Quest& o : quests) if (o.type == QType::Hunt && o.state != QState::Done && o.targetId == d.id) taken = true;
                for (int di = 0; di < (int)world.dens.size() && !taken; di++)
                  if (world.dens[(size_t)di].id == d.id && denClearedDay(di) >= 0) taken = true;
                if (!taken) dc.push_back({d2, d.id, d.x, d.y, d.mon});
              }
            }
          std::sort(dc.begin(), dc.end(), [](const DenC& x, const DenC& y) { return x.d2 != y.d2 ? x.d2 < y.d2 : x.id < y.id; });
          // (Witness playtests, 2026-10) a den that suits the hero first: at most two levels past them (the dice over
          // the nearer three sent a level-1 newcomer to a level-4 pack of four, 20 a bite). The nearer few of those, else (none within
          // the walk) the nearer few of all, as before; one draw either way
          const int fitD = plLevel + 2;
          const auto fitEnd = std::stable_partition(dc.begin(), dc.end(), [&](const DenC& c) { return world.src->danger(c.x, c.y) <= fitD; });
          const int fit = (int)(fitEnd - dc.begin());
          if (!dc.empty() && need != jobs::JN_GROUND) {
            const DenC& d = dc[(size_t)r.irange(std::min<int>(3, fit > 0 ? fit : (int)dc.size()))];
            q.mon = d.mon; q.targetId = d.id; ax = d.x; ay = d.y; den = true;
          }
        }
        if (!den) {
          Monster opts[3];
          ew::huntTargets(world.over.ecoAt(home.ex, home.ey), opts);
          q.mon = opts[r.irange(3)];
          // the ground: 40-60 tiles out, the first of eight ways (from the dice) on the home's own land
          static const int dx8[8] = {1, 1, 0, -1, -1, -1, 0, 1}, dy8[8] = {0, 1, 1, 1, 0, -1, -1, -1};
          const int d0 = r.irange(8), far = 40 + r.irange(21);
          const int32_t land = landAt(hx, hy);
          ax = hx + dx8[d0] * far; ay = hy + dy8[d0] * far;
          for (int k = 0; k < 8; k++) {
            const int d = (d0 + k) % 8, m = (dx8[d] && dy8[d]) ? far * 7 / 10 : far;
            const int32_t tx = hx + dx8[d] * m, ty = hy + dy8[d] * m;
            if (!land || landAt(tx, ty) == land) { ax = tx; ay = ty; break; }
          }
        }
        if (world.endless && world.src) {
          float bd = 140.0f;
          for (int32_t ry = ew::regionOf(ay) - 1; ry <= ew::regionOf(ay) + 1; ry++)
            for (int32_t rx = ew::regionOf(ax) - 1; rx <= ew::regionOf(ax) + 1; rx++) {
              const ew::RegionPlan R = world.regionPlan(rx, ry);
              for (const ew::LandmarkPlan& L : R.landmarks) {
                const float d = std::hypot((float)(L.x - ax), (float)(L.y - ay));
                if (d < bd) { bd = d; area = L.name; }
              }
            }
        }
        q.hasPos = true; q.tgx = ax; q.tgy = ay;
        q.subject = area;
        const bool big = q.mon == Monster::Troll || q.mon == Monster::Bear || q.mon == Monster::Sandworm || q.mon == Monster::Yeti || q.mon == Monster::Lurker;
        q.need = big ? 2 : 3 + r.irange(3);
        q.target = -1;
        V.set("M", monsterPlural(q.mon));
        V.set("n", speech::countWord(q.need));
        V.set("D", dirWord(ax - hx, ay - hy, false));
        V.set("W", walkWords(std::hypot((float)(ax - hx), (float)(ay - hy))));
        V.set("L", spokenPlace(area));
        V.set("PLACE", area.empty() ? std::string(den ? "THEIR DEN" : "THE WILDS") : spokenPlace(area));
        if (need == jobs::JN_HIDES) {   // a smith's or a tailor's: hides for the trade
          static const char* const ti[] = {"HIDES FOR THE TRADE", "{M} PELTS WANTED", "A HUNT FOR HIDES", "LEATHER ON THE HOOF", "PELTS FROM {PLACE}", "HIDES FROM THE {D}"};
          static const char* const re[] = {"I'M OUT OF GOOD HIDE FOR GRIPS AND BELLOWS. THE {M} OUT BY {PLACE} WEAR THE BEST THERE IS.",
                                           "THE TANNER'S RACKS ARE EMPTY AND MY ORDERS WON'T WAIT. THE {M} DEN {W} {D} OF HERE.",
                                           "FRESH {M} HIDES FETCH MORE THAN ANYTHING I MAKE THIS SEASON, AND THEY ROAM {PLACE}."};
          static const char* const as[] = {"BRING DOWN {n} OF THEM. I'LL SEND THE TANNER FOR THE SKINS.", "KILL {n}, CLEANLY IF YOU CAN."};
          static const char* const de[] = {"{GIVER} OF {H} WANTS HIDES: KILL {n} {M} OUT BY {PLACE} ({D} OF {H}).",
                                           "HUNT {n} {M} NEAR {PLACE}, {D} OF {H}. {GIVER} PAYS FOR THE HIDES."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as); desc = pickN(r, de);
        } else if (!area.empty()) {
          static const char* const ti[] = {"THIN THE {M} BY {L}", "THE {M} OF {L}", "{M} AT {L}", "CULL THE {M}", "TOO MANY {M}", "THE {M} OF {H}"};
          static const char* const re[] = {"THE {M} HAVE TAKEN TO {L}, {W} {D} OF HERE, AND EVERY NIGHT THEY COME CLOSER.",
                                           "{M} HAVE BEEN AT THE SHEEP AGAIN. THEY DEN OUT BY {L}.",
                                           "THE SHEPHERDS WON'T TAKE THE FLOCKS PAST {L} WHILE THE {M} ARE ABOUT.",
                                           "I LOST A GOOD DOG TO THE {M} BY {L} LAST NIGHT.",
                                           "SINCE THE THAW THE {M} OF {L} HAVE GROWN BOLD. THEY TOOK A CALF IN DAYLIGHT."};
          static const char* const as[] = {"KILL {n} OF THEM.", "THIN THEM OUT: {n} SHOULD DO IT.", "GO OUT TO {L} AND KILL {n}."};
          static const char* const de[] = {"THE {M} BY {L} ({D} OF {H}) ARE GETTING BOLD. KILL {n} OF THEM AND {GIVER} WILL PAY YOU.",
                                           "{GIVER} WANTS THE {M} NEAR {L} THINNED OUT: {n} OF THEM, FOR A FAIR PRICE.",
                                           "HUNT {n} {M} AROUND {L}, {D} OF {H}. {GIVER} PAYS ON YOUR RETURN."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as); desc = pickN(r, de);
        } else {
          static const char* const ti[] = {"CULL THE {M}", "TOO MANY {M}", "THE {M} OF {H}", "A DEN OF {M}", "{M} ON THE PROWL", "NIGHTS OF {M}"};
          static const char* const re[] = {"THE {M} AROUND HERE ARE GETTING BOLD. THEY DEN {W} {D} OF HERE.", "{M} HAVE BEEN AT THE SHEEP AGAIN.",
                                           "THE {M} CAME DOWN PAST {N} WITH THE COLD, AND THEY ARE HUNGRY.", "I LOST A GOOD DOG TO THE {M} LAST NIGHT.",
                                           "WE HEAR THE {M} EVERY NIGHT NOW, OUT {D} WHERE THE ROAD BENDS."};
          static const char* const as[] = {"KILL {n} OF THEM.", "THIN THEM OUT: {n} SHOULD DO IT."};
          static const char* const de[] = {"THE {M} AROUND {H} ARE GETTING BOLD. KILL {n} OF THEM ({D} OF {H}) AND {GIVER} WILL PAY YOU.",
                                           "{GIVER} WANTS THE {M} NEAR {H} THINNED OUT: {n} OF THEM, FOR A FAIR PRICE.",
                                           "HUNT {n} {M} IN THE WILDS {D} OF {H}. {GIVER} PAYS ON YOUR RETURN.",
                                           "{M} PROWL CLOSE TO {H} AGAIN. BRING DOWN {n} AND {GIVER} WILL MAKE IT WORTH YOUR WHILE."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as); desc = pickN(r, de);
        }
        // (M3c LIFE) the land speaks in the giver's words: a reason and a title of its biome (the person's key chooses)
        if (need != jobs::JN_HIDES) {
          const Eco he = world.over.ecoAt(home.ex, home.ey);
          const char* fl = huntFlavour(he, (key >> 21) + (uint64_t)done + (uint64_t)week);
          if (fl && ((key >> 17) & 3) == 1) {
            std::string en = ecoName(he);
            V.set("E", en);
            reason = fl;
            if (((key >> 19) & 1) != 0 && area.empty()) title = "THE {M} OF THE {E}";
          }
        }
        return true;
      }
      case K_DELIVER: {
        const int t = pickDestination(hx, hy, r);
        if (t < 0) return false;
        atSite(t);
        const Site T = world.sites[(size_t)t];
        q.type = QType::Deliver;
        const float rr = r.f();
        Role role = rr < 0.5f ? Role::Innkeeper : (rr < 0.8f || T.type == SiteType::Village ? Role::Smith : Role::Merchant);
        if (need == jobs::JN_TOOLS) role = T.type == SiteType::Village ? Role::Innkeeper : Role::Merchant;   // (P1) a smith's order
        if (need == jobs::JN_BREAD) role = Role::Innkeeper;                                                  // (P1) a baker's
        q.flags |= (uint32_t)role << 8;
        Actor d;
        d.site = t;
        Rng pr = personRng(q, seed);
        makeLook(d, role, pr);
        // (fixer M7b r2, review: "the recipient has the same name as the giver: it reads as the speaker naming
        // herself") another person seed until the recipient's name is not the giver's (personRng keys on it everywhere)
        for (int k = 0; k < 4 && d.name == a.name; k++) {
          q.flags = (q.flags & 0xFFFFu) | ((((q.flags >> 16) + 1u) & 0xFFFFu) << 16);
          d = Actor();
          d.site = t;
          Rng pr2 = personRng(q, seed);
          makeLook(d, role, pr2);
        }
        q.subject = d.name;
        pron(personFemale(q, seed));
        V.set("S", q.subject);
        V.set("R", roleWord(role));
        // the recipient's door when the place's records are in (else it is found on arrival: questTick)
        for (int b = T.bldgFirst; b < T.bldgFirst + T.bldgCount && b < (int)world.over.bldgs.size(); b++)
          if (world.over.bldgs[(size_t)b].type == roleBuilding(role)) { q.destBldg = b; break; }
        if (need == jobs::JN_TOOLS) {
          static const char* const ti[] = {"TOOLS FOR {T}", "A CRATE FOR {S}", "THE SMITH'S ORDER", "IRON FOR {T}"};
          static const char* const re[] = {"{S}, THE {R} OF {T}, ORDERED A CRATE OF MY TOOLS. THE ROAD RUNS {D}, {W}.",
                                           "SCYTHES, NAILS AND TWO GOOD AXES, ALL PROMISED TO {S} IN {T} BY THE NEW MOON.",
                                           "MY APPRENTICE WAS TO CARRY THIS CRATE TO {S} IN {T}, BUT THE BOY BROKE HIS ARM."};
          static const char* const as[] = {"CARRY IT TO {P_OBJ}. IT'S HEAVY, BUT IT WON'T BREAK.", "PUT IT IN {P_POSS} OWN HANDS."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as);
          desc = "CARRY A CRATE OF TOOLS FROM {GIVER} TO {S}, THE {R} OF {T} ({D} OF {H}). {S} PAYS ON DELIVERY.";
        } else if (need == jobs::JN_BREAD) {
          static const char* const ti[] = {"BREAD FOR {T}", "THE BAKER'S ORDER", "LOAVES FOR {S}", "A FEAST IN {T}"};
          static const char* const re[] = {"{S} AT THE INN OF {T} WANTS MY HONEY LOAVES FOR A WEDDING FEAST. IT'S {W} {D} OF HERE.",
                                           "THE INN AT {T} SWEARS BY MY BLACK BREAD, AND {S} HAS SENT FOR A BASKET.",
                                           "A BASKET OF CAKES FOR {S} IN {T}, BAKED THIS MORNING. THEY WON'T KEEP FOREVER."};
          static const char* const as[] = {"TAKE IT TO {P_OBJ} WHILE IT'S FRESH.", "DON'T EAT ANY ON THE ROAD."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as);
          desc = "CARRY {GIVER}'S BREAD TO {S}, THE {R} OF {T} ({D} OF {H}). {S} PAYS ON DELIVERY.";
        } else {
          static const char* const ti[] = {"A PARCEL FOR {S}", "THE ROAD TO {T}", "LETTERS FOR {T}", "A LETTER FOR {S}"};
          static const char* const re[] = {"THIS PARCEL MUST REACH {S}, THE {R} OF {T}. THE ROAD RUNS {D}, {W}.",
                                           "I PROMISED {S}, THE {R} OF {T}, THIS PARCEL BEFORE THE NEW MOON, AND MY LEGS ARE NOT WHAT THEY WERE.",
                                           "{S}, THE {R} OF {T}, IS WAITING ON THIS BUNDLE OF LETTERS. IT IS {W} {D} OF HERE.",
                                           "A SEALED LETTER FOR {S}, THE {R} OF {T}. THE CARRIER WHO SHOULD HAVE TAKEN IT NEVER CAME."};
          static const char* const as[] = {"TAKE IT TO {P_OBJ}, AND TO NOBODY ELSE.", "PUT IT IN {P_POSS} OWN HANDS."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as);
          desc = "CARRY A PARCEL FROM {GIVER} TO {S}, THE {R} OF {T} ({D} OF {H}). {S} PAYS ON DELIVERY.";
        }
        return true;
      }
      case K_HEIR: {
        const int t = pickRadiant(r.f() < 0.5f ? SiteType::Ruin : SiteType::Cave, hx, hy, r, danger);
        if (t < 0) return false;
        q.type = QType::Heirloom;
        atSite(t);
        if (need == jobs::JN_HOLYRELIC) {   // (P1) a temple's relic, carried off long ago
          static const char* const relic[] = {"RELIQUARY", "SILVER CHALICE", "HOLY BOOK", "SAINT'S BONE", "ALTAR CANDLESTICK", "BLESSED BELL"};
          const std::string item = relic[r.irange(6)];
          q.subject = "THE TEMPLE'S " + item;
          V.set("S", q.subject);
          V.set("OBJ", "THE " + item);
          static const char* const ti[] = {"THE LOST RELIC", "{S}", "A RELIC IN {T}", "WHAT THE THIEVES DROPPED"};
          static const char* const re[] = {"THIEVES TOOK {OBJ} FROM OUR ALTAR AND FLED INTO {T}. THEY NEVER CAME OUT, BUT NEITHER DID IT.",
                                           "{OBJ} WAS CARRIED INTO {T} BY A PRIEST WHO WENT TO CLEANSE IT. SHE NEVER RETURNED.",
                                           "OUR FEAST DAY COMES, AND {OBJ} STILL SITS IN A CHEST IN {T}, {W} {D}."};
          static const char* const as[] = {"BRING IT BACK TO THE TEMPLE.", "FIND THE CHEST AND BRING IT HOME TO US."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as);
          desc = "THE TEMPLE OF {H} LOST {OBJ} IN {T} ({D} OF {H}). FIND IT IN AN OLD CHEST THERE AND BRING IT TO {GIVER}.";
          return true;
        }
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
      case K_STOLEN: {   // (P1) a merchant's strongbox: the robbers hid it in a cave
        const int t = pickRadiant(SiteType::Cave, hx, hy, r, danger);
        if (t < 0) return false;
        q.type = QType::Heirloom;
        atSite(t);
        static const char* const goods[] = {"STRONGBOX", "LEDGER", "SPICE CHEST", "BOLT OF SILK", "SILVER PLATE", "SEAL"};
        const std::string item = goods[r.irange(6)];
        q.subject = "THE CARAVAN'S " + item;
        V.set("S", q.subject);
        V.set("OBJ", "THE " + item);
        static const char* const ti[] = {"STOLEN GOODS", "{S}", "THE ROBBERS' CACHE", "WHAT WAS TAKEN ON THE ROAD"};
        static const char* const re[] = {"ROBBERS TOOK MY CARAVAN ON THE ROAD AND HID {OBJ} IN {T}. I'D KNOW IT ANYWHERE.",
                                         "{OBJ} WENT MISSING WITH MY LAST WAGON. A TRAPPER SAW IT CARRIED INTO {T}, {W} {D}.",
                                         "WITHOUT {OBJ} I CAN'T PROVE A SINGLE DEBT. THE THIEVES LEFT IT IN A CHEST IN {T}."};
        static const char* const as[] = {"BRING IT BACK, UNOPENED.", "FIND THE CHEST AND BRING IT TO ME."};
        title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as);
        desc = "ROBBERS HID {GIVER}'S {OBJ} IN {T} ({D} OF {H}). FIND IT IN A CHEST THERE AND BRING IT BACK.";
        return true;
      }
      case K_SUPPLY: {   // (P1) the goods the giver's trade works, from a neighbour that sells them (life_quests.cpp)
        q.flags = 0;   // (no person seed: bits 20-21 are the supply run's own flags)
        const life::Job job = JobOps::censusJob(*this, a);
        const ew::Good gd = JobOps::tradeGood(a.role, job, need);
        if (gd == ew::Good::COUNT) return false;
        Quest sq;
        if (!jobsSupplyQuest(*this, a, gd, job, sq)) return false;
        q.type = QType::Supply;
        q.stage = sq.stage; q.need = sq.need; q.have = 0; q.gold = sq.gold; q.xp = sq.xp; q.subject = sq.subject;
        atSite(sq.target);
        V.set("GOOD", q.subject);
        V.set("UNITS", goodUnits(q.need, q.subject));
        if (need == jobs::JN_FAMINE) {
          static const char* const ti[] = {"FOOD FOR {H}", "THE HUNGRY WEEKS", "{GOOD} FROM {T}", "BEFORE THE STORES RUN DRY"};
          static const char* const re[] = {"THE GRANARIES ARE EMPTY AND THE CHILDREN CRY AT NIGHT. {T} STILL HAS {GOOD}.",
                                           "THIS FAMINE WILL TAKE THE OLD ONES FIRST. {T}, {W} {D}, HAS {GOOD} TO SPARE.",
                                           "WE'VE EATEN THE SEED CORN. IF NOBODY BRINGS {GOOD} FROM {T}, THERE'LL BE NO SPRING."};
          static const char* const as[] = {"BRING US {UNITS}.", "FETCH {UNITS} AND I'LL SEE IT SHARED OUT FAIRLY."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as);
        } else {
          static const char* const ti[] = {"{GOOD} FROM {T}", "SHORT OF {GOOD}", "THE {GOOD} RUN", "AN ORDER OF {GOOD}"};
          static const char* const re[] = {"MY WORK STANDS IDLE: NOT A SCRAP OF {GOOD} LEFT, AND THE CARTER IS A WEEK LATE. {T} SELLS IT.",
                                           "I'VE ORDERS I CAN'T FILL WITHOUT {GOOD}. {T}, {W} {D}, HAS PLENTY.",
                                           "THE LAST LOT OF {GOOD} WAS RUBBISH. {T} SELLS THE GOOD STUFF, {W} {D} OF HERE."};
          static const char* const as[] = {"BRING ME {UNITS}.", "FETCH {UNITS}. THEY KNOW ME THERE."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as);
        }
        desc = sq.desc;
        return true;
      }
      case K_MISSING: {
        const int t = pickRadiant(SiteType::Cave, hx, hy, r, danger);
        if (t < 0) return false;
        q.type = QType::Missing;
        atSite(t);
        Actor d;
        d.site = q.giverSite;   // M3: the missing relative shares the giver's culture (name, dress); spawnEscort matches
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
        if (need == jobs::JN_STRAYED) {   // (P1) a farm's: the hand who went after the herd
          V.set("BEAST", pick(r, kStrayBeast));
          static const char* const ti[] = {"STRAYED: {S}", "{S} WENT AFTER THE HERD", "LOST WITH THE {BEAST}", "FIND {S}"};
          static const char* const re[] = {"OUR FARMHAND {S} WENT AFTER A STRAYED {BEAST} {X} DAYS AGO. THE TRACKS RUN TO {T}, {W} {D}.",
                                           "{S} TOOK THE DOG TO BRING BACK THE {BEAST}. THE DOG CAME HOME ALONE, FROM THE WAY OF {T}.",
                                           "THE {BEAST} CAME BACK BY ITSELF. {S}, WHO WENT LOOKING FOR IT NEAR {T}, DID NOT."};
          static const char* const as[] = {"BRING {P_OBJ} HOME ALIVE.", "FIND {P_OBJ} BEFORE ANOTHER NIGHT FALLS."};
          title = pickN(r, ti); reason = pickN(r, re); ask = pickN(r, as);
          desc = "{S}, WHO WORKS {GIVER}'S FIELDS, WENT AFTER A STRAYED {BEAST} AND WAS LOST IN {T} ({D} OF {H}). FIND {P_OBJ} AND LEAD {P_OBJ} OUT.";
          return true;
        }
        static const char* const ti[] = {"MISSING: {S}", "FIND {S}", "{S} HAS NOT COME HOME", "WHERE IS {S}?"};
        static const char* const re[] = {"MY {REL2} {S} WENT INTO {T} {X} DAYS AGO AND HAS NOT COME BACK.",
                                         "MY {REL2} {S} WENT LOOKING FOR {LOOK} NEAR {T} AND NEVER CAME BACK. SOMEONE SAW A LIGHT IN THE CAVE.",
                                         "MY {REL2} {S} WAS TAKEN. THE TRACKS LEAD TO {T}, {W} {D}."};
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
      case K_COUNT: break;
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
  if (made == K_SUPPLY) { g = q.gold; xp = q.xp; danger = false; }   // (P1) the going price, doubled (life_quests.cpp)
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
  const bool trade = a.role == Role::Merchant || a.role == Role::Smith || a.role == Role::Mage;
  // (a lord or the watch pays from the purse of the realm, never "all I have")
  const bool crown = a.role == Role::Jarl || a.role == Role::Guard;
  const char* reward = (K && crown && r.f() < 0.5f) ? pick(r, kRewardCrown) : (trade || crown) ? pick(r, kRewardTrade) : pick(r, kReward);
  if (made == K_DELIVER) reward = "{P_SUBJ} PAYS {G} GOLD WHEN IT ARRIVES.";
  // (M7b) where it is, when the reason did not say: the place by name and the way there, before the ask
  std::string where;
  if (q.target >= 0 && made != K_PROTECT && reason.find("{D}") == std::string::npos)
    // (named once is enough: a place the reason already named is "the place"; a delivery's town keeps its name)
    where = reason.find("{T}") != std::string::npos && made != K_DELIVER ? "THE PLACE IS {W} {D} OF HERE." : "{T} IS {W} {D} OF HERE.";
  // (P1) a hunt's ground: where the beasts den, when the reason did not say
  if (made == K_HUNT && q.hasPos && reason.find("{D}") == std::string::npos && reason.find("{L}") == std::string::npos && reason.find("{PLACE}") == std::string::npos)
    where = !q.subject.empty() ? "THEY DEN BY {L}, {W} {D} OF HERE." : q.targetId ? "THEIR DEN IS {W} {D} OF HERE." : "THEY ROAM THE WILDS {W} {D} OF HERE.";
  const char* const dangerLine = danger ? pick(r, kDanger) : "";
  // one person talking, in order: why they ask, what happened, where, what they want, the risk, the pay. A pitch too
  // long for the phone panel loses its opener first, then the way there (the journal and the map have it)
  auto compose = [&](bool withMood, bool withWhere) {
    std::string t;
    auto add = [&](const std::string& x) { if (!x.empty()) t += (t.empty() ? "" : " ") + x; };
    if (withMood) add(mood);
    add(reason);
    if (withWhere) add(where);
    add(ask);
    add(dangerLine);
    add(reward);
    return V.fill(t);
  };
  pendingPitch_ = compose(true, true);
  if (!speech::fitsPanel(pendingPitch_)) pendingPitch_ = compose(false, true);
  if (!speech::fitsPanel(pendingPitch_)) pendingPitch_ = compose(false, false);
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
  { Quest lq; if (lifeOffer(a, lq)) return true; }   // M5: an unmet need asks for help (life_quests.cpp)
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

Quest Game::makeOffer(const Actor& npc, bool* okOut) {
  if (okOut) *okOut = true;
  { Quest lq; if (lifeOffer(npc, lq)) { pendingPitch_ = lifePitch(*this, lq); return lq; } }   // M5: an unmet need asks for help first (life_quests.cpp)
  bool ok = false;
  // (P1) the person's job this week: its kind and its need (offerFor reads jobBook.offerNeed while it builds)
  if (const jobs::PoolJob* j = JobOps::openJob(*this, npc)) {
    jobBook.offerNeed = j->need;
    const QType want = (QType)j->qtype;
    // (P1b) a radiant job (QType::Story) is begun by "ANY WORK GOING?" itself (radiant::offerFor); here only when it
    // could not be composed or cast in this world: the fallback hunt below
    Quest q = want == QType::Story ? Quest() : offerFor(npc, want, ok);
    if (!ok && JobOps::censusJob(*this, npc) != life::Job::Scholar) {   // the place it named is gone (cleared since Monday, taken by another job): a hunt at a den always fits (not a scholar's)
      jobBook.offerNeed = jobs::JN_DEN;
      q = offerFor(npc, QType::Hunt, ok);
    }
    jobBook.offerNeed = jobs::JN_NONE;
    if (ok) return q;
  }
  Quest q = offerFor(npc, QType::COUNT, ok);
  if (okOut) *okOut = ok;
  return q;
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
    case QType::Story: return q.hasPos ? global(q.tgx, q.tgy) : false;   // M4: the story engine's marker (rpg/story)
    case QType::War: return q.hasPos ? global(q.tgx, q.tgy) : atSite();   // M4: the enemy camp (relief) or the town (assault)
    case QType::Supply: return (q.flags & life::QF_SUP_GOODS) ? false : atSite();   // M5: where the goods are to be had
    case QType::Sea: return q.hasPos ? global(q.tgx, q.tgy) : false;   // M10: the chart chain's place, the king's capital
    case QType::Campaign: case QType::Envoy: case QType::Fief:   // M12: the order's / court's / holding's place
      return q.hasPos ? global(q.tgx, q.tgy) : atSite();
    case QType::Errand: return q.hasPos ? global(q.tgx, q.tgy) : false;   // P1: the street incident's place (LIFE lane)
    case QType::Hunt: {   // P1: the den or the hunting ground; a quarry in sight first (out of doors, within 20 tiles)
      if (!q.hasPos) return false;   // (a hunt from before P1: questTarget looks for the quarry and the dens itself)
      if (!inside) {
        float bd = (20.0f * TILE) * (20.0f * TILE);
        bool seen = false;
        for (size_t i = 1; i < actors.size(); i++) {
          const Actor& m = actors[i];
          if (m.st == AState::Dead || m.human || m.npc || m.mon != q.mon) continue;
          // (P1 fixer) never the hero's own summon, a charmed or raised beast (a calmed or held wild one is still quarry)
          if (m.faction == Faction::Player || m.magicTag == magic::MT_SUMMON || m.magicTag == magic::MT_CHARM || m.magicTag == magic::MT_RAISE || m.magicTag == magic::MT_BOUND) continue;
          const float d = len2(m.p - pl().p);
          if (d < bd) { bd = d; tx = (int)std::floor(m.p.x / TILE); ty = (int)std::floor(m.p.y / TILE); seen = true; }
        }
        if (seen) return true;
      }
      return global(q.tgx, q.tgy);
    }
    default: return false;
  }
}

std::string Game::questStatusM2(const Quest& q) const {
  if (q.state == QState::Done) return "";
  int px, py;
  overworldTile(*this, px, py);
  int tx = 0, ty = 0;
  // (fixer M6b r3) a story's step names the direction of the live person it waits on (Game::questTarget)
  const bool has = q.type == QType::Story ? questTarget(q.id, tx, ty) : questTargetM2(q, tx, ty);
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
      const bool night = (day == q.deadlineDay && hour >= 20.5f) || (day == q.deadlineDay + 1 && hour < 5.5f);
      const std::string home = giverTown(world, q);
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

std::string Game::turnInLine(const Quest& q) const {
  const std::string g = std::to_string(q.gold);
  const std::string tname = q.target >= 0 && q.target < (int)world.sites.size() ? spokenPlace(world.sites[(size_t)q.target].name) : std::string();
  const bool fem = personFemale(q, seed);
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
      for (const service::Order& x : service.orders) if ((int)x.id == q.stage) o = &x;
      static const char* by[] = {"ORDERS CARRIED OUT. THE CAPTAINS WILL HEAR OF IT.", "THE SIEGE IS BROKEN AND OUR PEOPLE EAT AGAIN.",
                                 "THE GATE FELL BECAUSE YOU WENT IN FIRST.", "THE CARTS GOT THROUGH. THE GARRISON WILL HOLD.",
                                 "A GOOD REPORT, AND YOU CAME BACK ALIVE TO GIVE IT.", "THEIR CAPTAIN IS DEAD, AND HIS MEN ALREADY WAVER.",
                                 "THOSE FAMILIES ARE ALIVE BECAUSE OF YOU.", "THE FRONTIER HELD THE NIGHT, AND YOU HELD IT WITH THEM."};
      return std::string(by[o ? std::clamp((int)o->kind, 0, 7) : 0]) + " " + g + " GOLD FROM THE WAR CHEST.";
    }
    case QType::Envoy: {   // M12 (SERVICE lane): the court's thanks, by what its word won
      const service::Mission* m = nullptr;
      for (const service::Mission& x : service.missions) if ((int)x.id == q.stage) m = &x;
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
std::string Game::questReminder(const Quest& q) const {
  const std::string place = placeFromGiver(*this, q);
  const std::string home = giverTown(world, q);
  const bool fem = personFemale(q, seed);
  const std::string obj = fem ? "HER" : "HIM", subj = fem ? "SHE" : "HE";
  const uint32_t h = hash32((uint32_t)q.id * 2654435761u ^ (uint32_t)day);
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
std::string Game::acceptLine(const Quest& q) const {
  // (the pitch said the way there: the place by its name now)
  const std::string place = q.target >= 0 && q.target < (int)world.sites.size() && q.target != q.giverSite ? spokenPlace(world.sites[(size_t)q.target].name) : std::string();
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
  a.site = q.giverSite;   // dressed as the giver's people (the same look the quest named them with), then homeless again
  makeLook(a, Role::Villager, pr);
  a.site = -1;
  a.name = q.subject;
  a.quest = q.id;
  a.fromMap = false; a.militia = false;
  a.speed = 60;
}

void Game::questFail(Quest& q, const std::string& why) {
  q.state = QState::Done;
  q.flags |= QF_FAILED;
  partyQuest(q, false);   // M8: the companions saw it fail (party_game.cpp)
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

void Game::questTick(float dt) {
  (void)dt;
  {   // M5 (CITIZENS lane): supply runs and rescues
  for (Quest& q : quests) {
    if (q.type == QType::Supply) {
      if (q.state == QState::Active && !(q.flags & life::QF_SUP_GOODS) && q.target >= 0 && q.target < (int)world.sites.size()) {
        const Site& T = world.sites[(size_t)q.target];
        bool there = false;
        if (inside) there = subSite == q.target || (subBldg >= 0 && subBldg < (int)world.over.bldgs.size() && world.over.bldgs[(size_t)subBldg].site == q.target);
        else {
          const int tx = (int)std::floor(pl().p.x / TILE), ty = (int)std::floor(pl().p.y / TILE);
          there = tx >= T.r.x - 2 && ty >= T.r.y - 2 && tx < T.r.x + T.r.w + 2 && ty < T.r.y + T.r.h + 2;
        }
        if (there) {
          const ew::Good g = (ew::Good)std::clamp(q.stage, 0, (int)ew::Good::COUNT - 1);
          Item it;
          it.kind = ItemKind::Quest; it.name = std::to_string(q.need) + " " + q.subject; it.icon = goodIcon(g);
          it.questId = q.id; it.value = 0; it.tint = rgba(200, 170, 120);
          addItem(it, false);
          if (life::Census* tc = life.findMut(T.id)) tc->stock[(size_t)g] = (uint16_t)std::max(0, (int)tc->stock[(size_t)g] - q.need);
          q.flags |= life::QF_SUP_GOODS;
          q.have = q.need;
          q.state = QState::Complete;
          const std::string m = "LOADED " + std::to_string(q.need) + " " + q.subject + " IN " + T.name + ". " + rewardReadyMsg(world, q);
          emit(Ev::QuestUpdate, pl().p, q.id, 2, m);
          say(m);
          sfx((int)Sfx::QuestStart, pl().p, 1.1f);
        }
      }
      if (q.state == QState::Done && !(q.flags & QF_FAILED) && (q.flags & life::QF_SUP_GOODS) && !(q.flags & life::QF_SUP_DONE)) {
        life.supply(q.giverId, (ew::Good)std::clamp(q.stage, 0, (int)ew::Good::COUNT - 1), q.need);
        q.flags |= life::QF_SUP_DONE;
      }
      continue;
    }
    if (q.type != QType::Missing || !q.giverId) continue;
    life::Census* c = life.findMut(q.giverId);
    if (!c || c->takenIdx < 0 || c->takenIdx >= (int)c->res.size() || c->res[(size_t)c->takenIdx].name != q.subject) continue;
    if (c->takenQuest == 0 && q.state != QState::Done) c->takenQuest = q.id;
    if (c->takenQuest != q.id) continue;
    life::Resident& r = c->res[(size_t)c->takenIdx];
    if (q.state == QState::Complete || (q.state == QState::Done && !(q.flags & QF_FAILED))) {
      r.flags &= (uint16_t)~life::RF_AWAY;
      c->takenIdx = -1;
      c->takenQuest = 0;
    } else if (q.state == QState::Done) {
      life.residentDied(q.giverId, r.idx, day);
    }
  }
  }
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
