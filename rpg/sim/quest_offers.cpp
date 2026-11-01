// M2 radiant quest OFFERS (SOLID 3.5, split out of quests.cpp): offerFor, the offer a townsperson makes when asked for
// work. offerFor sets up one OfferCtx (the giver, the dice, the local names, the grammar's variables), turns the
// giver's menu of offer kinds, and asks offerBuild (the switch below it) for each kind in turn: offerClear,
// offerBounty, offerHunt, offerDeliver, offerHeirloom, offerStolen, offerSupply, offerMissing, offerProtect. The first
// that fits is priced and pitched (offerPitch). The dice are drawn in the same order as when this was one function.
// Neighbours: quests.cpp (accept, progress, rewards, pickRadiant / pickDestination, the shared quest-text helpers),
// life_quests.cpp (Supply), rpg/sim/jobs_ops.h (the weekly pools).
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

using art::Monster;
using art::Prop;

// ---- from quests.cpp (defined there, namespace qtext): the quest-text helpers both files use
namespace qtext {
Rng personRng(const Quest& q, uint64_t worldSeed);
bool personFemale(const Quest& q, uint64_t worldSeed);
std::string spokenPlace(const std::string& n);
const char* roleWord(Role r);
art::Building roleBuilding(Role r);
int protectZone(const Quest& q, Biome b);
const char* zonePlural(int z, Biome b);
}  // namespace qtext
using namespace qtext;

// (P1, life_quests.cpp) a Supply quest for a trade's good from the nearest loaded neighbour that sells it
bool jobsSupplyQuest(const Game& g, const Actor& npc, ew::Good good, life::Job job, Quest& q);

namespace {
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

// the menu of offer kinds a person gives (by trade), or the one kind a pool job asks for (empty: none fits)
std::vector<OfferKind> offerMenu(const Game& g, const Actor& a, QType want, uint8_t need) {
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
      if (JobOps::censusJob(g, a) == life::Job::Scholar) menu = {K_HEIR, K_RUIN, K_MISSING, K_DELIVER};
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
  }
  return menu;
}
}  // namespace

// One offer being built: what offerFor's lambdas shared when it was one function
struct Game::OfferCtx {
  Game& game;
  const Actor& a;
  Quest& q;
  Rng& r;
  const uint64_t key;
  const int done;
  const int week;
  const uint8_t need;
  const Site home;   // a copy: picking targets loads more sites
  int32_t hx = 0, hy = 0;
  const Kingdom* K = nullptr;
  std::string localName;
  Vars V;
  bool danger = false;
  int targetLevel = 0;
  std::string title, reason, ask, desc;

  // (M2 fixer round 2) the grammar's words turn with the jobs this person has given: two offers of one kind a few jobs
  // apart never come out word for word the same (a giver repeated HOLD THE FIELDS)
  // (by the person, not the job's dice: the title cycles by 3 and the description by 4, so two offers of one kind
  // within a dozen jobs never share both)
  template <class A> const char* pickN(Rng& rr, const A& arr) const {
    const int n = (int)(sizeof(arr) / sizeof(arr[0]));
    (void)rr.irange(n);   // (the dice still roll: the rest of the offer keeps its draws)
    // (P1) and with the week: a giver asked again next week (the notice board above all) says it another way
    return arr[(int)(((key >> 13) % (uint64_t)n + (uint64_t)done + (uint64_t)week) % (uint64_t)n)];
  }
  void atSite(int t) {
    const World& world = game.world;
    const Site& T = world.sites[(size_t)t];
    q.target = t; q.targetId = T.id;
    q.hasPos = true; q.tgx = world.ox + T.ex; q.tgy = world.oy + T.ey;
    targetLevel = std::max(targetLevel, T.level);
    V.set("T", spokenPlace(T.name));
    V.set("D", dirWord(q.tgx - hx, q.tgy - hy, false));
    V.set("W", walkWords(std::hypot((float)(q.tgx - hx), (float)(q.tgy - hy))));
    V.set("M", monsterPlural(T.theme));
  }
  void pron(bool female) {
    V.set("P_SUBJ", female ? "SHE" : "HE");
    V.set("P_OBJ", female ? "HER" : "HIM");
    V.set("P_POSS", female ? "HER" : "HIS");
  }
};

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
  q.giverSite = a.site; q.giverBldg = a.bldg; q.giverSlot = a.slot; q.giverName = a.name;
  OfferCtx oc{*this, a, q, r, key, done, week, need, world.sites[(size_t)a.site]};
  const Site& home = oc.home;   // (the copy the offer works from: picking targets loads more sites)
  q.giverId = home.id;
  const int32_t hx = world.ox + home.ex, hy = world.oy + home.ey;
  oc.hx = hx; oc.hy = hy;
  q.flags = (r.next() & 0xFFFFu) << 16;   // the person seed (personRng)
  // the local names the grammar may use: the kingdom, a named landmark or the nearest dark place
  const Kingdom* K = world.kingdomOf(a.site);
  oc.K = K;
  std::string& localName = oc.localName;
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
  Vars& V = oc.V;
  V.set("H", home.name);
  V.set("GIVER", a.name);
  V.set("K", K ? K->name : home.name);
  V.set("N", localName);
  // what this person offers (by trade), tried from a rotating start so successive jobs differ; the kind of the last
  // job they gave goes to the back of the line
  std::vector<OfferKind> menu = offerMenu(*this, a, want, need);
  if (menu.empty()) return q;
  {
    const int n = (int)menu.size();
    const int start = (r.irange(n) + done) % n;
    std::rotate(menu.begin(), menu.begin() + start, menu.end());
    auto lo = marks.find(markKey(key, Mk::LastOffer));
    if (lo != marks.end() && n > 1)
      std::stable_partition(menu.begin(), menu.end(), [&](OfferKind k) { return (int)kindType(k) + 1 != lo->second; });
  }
  oc.targetLevel = home.level;
  OfferKind made = K_HUNT;
  for (OfferKind k : menu) {
    const Quest keep = q;
    if (offerBuild(oc, k)) { ok = true; made = k; break; }
    q = keep;
    oc.danger = false;
  }
  if (!ok) {
    if (want != QType::COUNT) return q;
    ok = offerBuild(oc, K_HUNT);   // a hunt always fits: the beasts around home
    made = K_HUNT;
  }
  offerPitch(oc, made);
  return q;
}

// the switch: one builder per kind of offer (false: it does not fit this giver now)
bool Game::offerBuild(OfferCtx& oc, int kind) {
  switch ((OfferKind)kind) {
    case K_CAVE: case K_RUIN: return offerClear(oc, kind);
    case K_BOUNTY: case K_NAMED: return offerBounty(oc, kind);
    case K_HUNT: return offerHunt(oc);
    case K_DELIVER: return offerDeliver(oc);
    case K_HEIR: return offerHeirloom(oc);
    case K_STOLEN: return offerStolen(oc);
    case K_SUPPLY: return offerSupply(oc);
    case K_MISSING: return offerMissing(oc);
    case K_PROTECT: return offerProtect(oc);
    case K_COUNT: break;
  }
  return false;
}

// Clear: the thing in a cave, or the restless dead (or a defiled shrine) in a ruin
bool Game::offerClear(OfferCtx& oc, int kind) {
  Quest& q = oc.q;
  Rng& r = oc.r;
  const int32_t hx = oc.hx;
  const int32_t hy = oc.hy;
  const uint8_t need = oc.need;
  bool& danger = oc.danger;
  std::string& title = oc.title;
  std::string& reason = oc.reason;
  std::string& ask = oc.ask;
  std::string& desc = oc.desc;
  const OfferKind k = (OfferKind)kind;
  const int t = pickRadiant(k == K_CAVE ? SiteType::Cave : SiteType::Ruin, hx, hy, r, danger);
  if (t < 0) return false;
  q.type = QType::Clear;
  oc.atSite(t);
  if (k == K_CAVE) {
    static const char* const ti[] = {"CLEAR {T}", "THE THING IN {T}", "TROUBLE AT {T}"};
    static const char* const re[] = {"SOMETHING FOUL HAS MADE ITS NEST IN {T}, {W} {D} OF HERE.",
                                     "THE HUNTERS WON'T GO NEAR {T} ANY MORE. THE LAST ONE CAME BACK WITHOUT HIS DOG.",
                                     "{M} CRAWL OUT OF {T} AT NIGHT AND TAKE OUR GOATS.",
                                     "MY BROTHER SWEARS HE HEARD SOMETHING HUGE BREATHING IN {T}, PAST {N}."};
    static const char* const as[] = {"KILL WHATEVER LEADS THEM.", "GO IN AND KILL THE BIGGEST THING YOU FIND.", "END IT, WHATEVER IT IS."};
    static const char* const de[] = {"SOMETHING FOUL NESTS IN {T}, {D} OF {H}. KILL WHATEVER LEADS IT, THEN RETURN TO {GIVER} IN {H}.",
                                     "{M} HAUNT {T} ({D} OF {H}). KILL THEIR LEADER AND TELL {GIVER} IN {H}."};
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as); desc = oc.pickN(r, de);
  } else if (need == jobs::JN_DESECRATED) {   // (P1) a priest's: holy ground gone foul
    static const char* const ti[] = {"CLEANSE {T}", "HOLY GROUND DEFILED", "THE DEFILERS OF {T}", "A SHRINE GONE DARK"};
    static const char* const re[] = {"{T} WAS HOLY GROUND WHEN MY GRANDMOTHER WAS A GIRL. NOW SOMETHING UNCLEAN DWELLS THERE.",
                                     "THE OLD SHRINE IN {T} HAS BEEN DEFILED. THE CANDLES WE LEAVE THERE ARE FOUND SNUFFED AND BLACK.",
                                     "PILGRIMS WON'T WALK TO {T} ANY MORE. THEY SAY THE STONES WEEP AT NIGHT.",
                                     "WHATEVER SQUATS IN {T}, {W} {D} OF HERE, HAS MADE A MOCKERY OF THE GODS' HOUSE."};
    static const char* const as[] = {"DRIVE IT OUT AND THE GODS WILL KNOW YOUR NAME.", "SLAY WHAT DEFILES IT, SO WE CAN BLESS IT AGAIN."};
    static const char* const de[] = {"SOMETHING UNCLEAN DEFILES {T} ({D} OF {H}). SLAY ITS MASTER AND TELL {GIVER} THE GROUND IS CLEAN.",
                                     "{GIVER} OF {H} WANTS {T} CLEANSED ({D}). DESTROY WHAT DWELLS THERE AND RETURN."};
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as); desc = oc.pickN(r, de);
  } else {
    static const char* const ti[] = {"THE RESTLESS DEAD", "LIGHTS IN {T}", "LAY {T} TO REST"};
    static const char* const re[] = {"THE DEAD STIR IN {T}.", "LIGHTS BURN IN {T} WHERE NO LIVING HAND LIT THEM.",
                                     "THE OLD KINGS OF {K} BURIED THEIR DEAD IN {T}, AND THE DEAD HAVE NOT STAYED BURIED.",
                                     "GRAVE-ROBBERS WENT INTO {T} AND WOKE SOMETHING. NOW IT WALKS THE ROAD {D} OF HERE."};
    static const char* const as[] = {"PUT THEIR MASTER BACK IN THE GROUND.", "FIND WHAT WAKES THEM AND DESTROY IT."};
    static const char* const de[] = {"THE DEAD STIR IN {T}, {D} OF {H}. DESTROY THEIR MASTER, THEN REPORT TO {GIVER}.",
                                     "SOMETHING WAKES THE DEAD OF {T} ({D} OF {H}). END IT AND RETURN TO {GIVER}."};
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as); desc = oc.pickN(r, de);
  }
  return true;
}

// Bounty (raiders, deserters, outlaws) or NamedBandit (a chief with a name and a title) at a bandit camp
bool Game::offerBounty(OfferCtx& oc, int kind) {
  Quest& q = oc.q;
  Rng& r = oc.r;
  const int32_t hx = oc.hx;
  const int32_t hy = oc.hy;
  Vars& V = oc.V;
  const uint8_t need = oc.need;
  bool& danger = oc.danger;
  std::string& title = oc.title;
  std::string& reason = oc.reason;
  std::string& ask = oc.ask;
  std::string& desc = oc.desc;
  const OfferKind k = (OfferKind)kind;
  const int t = pickRadiant(SiteType::BanditCamp, hx, hy, r, danger);
  if (t < 0) return false;
  oc.atSite(t);
  if (k == K_BOUNTY && need == jobs::JN_RAIDERS) {   // (P1) a merchant's: the caravan robbers
    q.type = QType::Bounty;
    static const char* const ti[] = {"RAIDERS ON THE ROAD", "THE CARAVAN ROBBERS", "WHAT {T} TOOK", "A PRICE ON THE RAIDERS"};
    static const char* const re[] = {"MY LAST CARAVAN NEVER REACHED {H}. THE RAIDERS WHO TOOK IT HOLE UP AT {T}, {W} {D}.",
                                     "THREE WAGONS OF MINE ROBBED THIS SEASON, ALL BY THE SAME BAND OUT OF {T}.",
                                     "NO CARTER WILL TAKE MY GOODS PAST {N} WHILE THE RAIDERS OF {T} WATCH THE ROAD."};
    static const char* const as[] = {"KILL THEIR CHIEF AND THE ROAD IS MINE AGAIN.", "PUT THEIR CHIEF IN THE GROUND."};
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as);
    desc = "RAIDERS FROM {T} ({D} OF {H}) ROB {GIVER}'S CARAVANS. KILL THEIR CHIEF AND COLLECT THE BOUNTY FROM {GIVER}.";
  } else if (k == K_BOUNTY && need == jobs::JN_WARBAND) {   // (P1) the realm at war: deserters turned bandit
    q.type = QType::Bounty;
    static const char* const ti[] = {"DESERTERS AT {T}", "THE WAR'S LEFTOVERS", "TURNCOATS ON THE ROAD"};
    static const char* const re[] = {"THE WAR HAS FILLED THE WOODS WITH DESERTERS. A BAND OF THEM HOLDS {T}, AND THEY TAKE WHAT THEY LIKE.",
                                     "SOLDIERS WHO RAN FROM THE WAR CAMP AT {T} NOW, {W} {D}. THEY ROB THEIR OWN PEOPLE.",
                                     "WITH THE ARMY AWAY AT THE WAR, DESERTERS FROM {T} RAID OUR ROADS UNCHALLENGED."};
    static const char* const as[] = {"KILL THE ONE WHO LEADS THEM.", "END THEIR CAPTAIN AND THE REST WILL SCATTER."};
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as);
    desc = "DESERTERS HOLD {T} ({D} OF {H}) WHILE THE REALM IS AT WAR. KILL THEIR LEADER AND COLLECT THE BOUNTY FROM {GIVER}.";
  } else if (k == K_BOUNTY) {
    q.type = QType::Bounty;
    static const char* const ti[] = {"BOUNTY: {T}", "A PRICE ON {T}", "THE OUTLAWS OF {T}", "CUTTHROATS AT {T}"};
    static const char* const re[] = {"BANDITS AT {T} HAVE BEEN RAIDING THE ROADS {D} OF HERE.",
                                     "THE OUTLAWS OF {T} ROBBED A WAGON ON ITS WAY HERE LAST WEEK.",
                                     "THEY BURNED A FARM NEAR {N}. THE TRACKS LED TO {T}, {W} {D}."};
    static const char* const as[] = {"KILL THEIR CHIEF AND THE REST WILL SCATTER.", "CUT THE HEAD OFF THAT SNAKE: KILL THEIR CHIEF."};
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as);
    desc = "BANDITS AT {T} HAVE BEEN RAIDING TRAVELLERS. KILL THEIR CHIEF AND COLLECT THE BOUNTY FROM {GIVER}.";
  } else {
    q.type = QType::NamedBandit;
    Actor d;
    d.site = t;
    Rng pr = personRng(q, seed);
    makeLook(d, Role::Bandit, pr);
    q.subject = d.name + " " + pick(r, kEpithet);
    oc.pron(personFemale(q, seed));
    V.set("S", q.subject);
    static const char* const ti[] = {"{S}", "THE END OF {S}", "A PRICE ON {S}"};
    static const char* const re[] = {"{S} RULES {T} NOW. {P_SUBJ} BURNED OUR MILL LAST SPRING.",
                                     "THEY CALL {P_OBJ} {S}. {P_SUBJ} AND {P_POSS} CUTTHROATS HOLD {T}, {W} {D} OF HERE.",
                                     "EVERY CART ON THE ROAD PAST {N} PAYS {S} A TOLL OR BLEEDS FOR IT."};
    static const char* const as[] = {"END {S} AND THE PRICE ON {P_POSS} HEAD IS YOURS.", "BRING {S} DOWN. NOBODY ELSE WILL."};
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as);
    desc = "{S} HOLDS {T} ({D} OF {H}). KILL {P_OBJ} AND COLLECT THE BOUNTY FROM {GIVER}.";
  }
  return true;
}

// Hunt: thin the beasts round a den near home (or hides for the trade)
bool Game::offerHunt(OfferCtx& oc) {
  Quest& q = oc.q;
  Rng& r = oc.r;
  const Site& home = oc.home;
  const int32_t hx = oc.hx;
  const int32_t hy = oc.hy;
  Vars& V = oc.V;
  const uint8_t need = oc.need;
  std::string& title = oc.title;
  std::string& reason = oc.reason;
  std::string& ask = oc.ask;
  std::string& desc = oc.desc;
  const uint64_t key = oc.key;
  const int done = oc.done;
  const int week = oc.week;
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
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as); desc = oc.pickN(r, de);
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
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as); desc = oc.pickN(r, de);
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
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as); desc = oc.pickN(r, de);
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

// Deliver: a parcel for a named innkeeper, smith or merchant in another settlement
bool Game::offerDeliver(OfferCtx& oc) {
  Quest& q = oc.q;
  Rng& r = oc.r;
  const Actor& a = oc.a;
  const int32_t hx = oc.hx;
  const int32_t hy = oc.hy;
  Vars& V = oc.V;
  const uint8_t need = oc.need;
  std::string& title = oc.title;
  std::string& reason = oc.reason;
  std::string& ask = oc.ask;
  std::string& desc = oc.desc;
  const int t = pickDestination(hx, hy, r);
  if (t < 0) return false;
  oc.atSite(t);
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
  oc.pron(personFemale(q, seed));
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
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as);
    desc = "CARRY A CRATE OF TOOLS FROM {GIVER} TO {S}, THE {R} OF {T} ({D} OF {H}). {S} PAYS ON DELIVERY.";
  } else if (need == jobs::JN_BREAD) {
    static const char* const ti[] = {"BREAD FOR {T}", "THE BAKER'S ORDER", "LOAVES FOR {S}", "A FEAST IN {T}"};
    static const char* const re[] = {"{S} AT THE INN OF {T} WANTS MY HONEY LOAVES FOR A WEDDING FEAST. IT'S {W} {D} OF HERE.",
                                     "THE INN AT {T} SWEARS BY MY BLACK BREAD, AND {S} HAS SENT FOR A BASKET.",
                                     "A BASKET OF CAKES FOR {S} IN {T}, BAKED THIS MORNING. THEY WON'T KEEP FOREVER."};
    static const char* const as[] = {"TAKE IT TO {P_OBJ} WHILE IT'S FRESH.", "DON'T EAT ANY ON THE ROAD."};
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as);
    desc = "CARRY {GIVER}'S BREAD TO {S}, THE {R} OF {T} ({D} OF {H}). {S} PAYS ON DELIVERY.";
  } else {
    static const char* const ti[] = {"A PARCEL FOR {S}", "THE ROAD TO {T}", "LETTERS FOR {T}", "A LETTER FOR {S}"};
    static const char* const re[] = {"THIS PARCEL MUST REACH {S}, THE {R} OF {T}. THE ROAD RUNS {D}, {W}.",
                                     "I PROMISED {S}, THE {R} OF {T}, THIS PARCEL BEFORE THE NEW MOON, AND MY LEGS ARE NOT WHAT THEY WERE.",
                                     "{S}, THE {R} OF {T}, IS WAITING ON THIS BUNDLE OF LETTERS. IT IS {W} {D} OF HERE.",
                                     "A SEALED LETTER FOR {S}, THE {R} OF {T}. THE CARRIER WHO SHOULD HAVE TAKEN IT NEVER CAME."};
    static const char* const as[] = {"TAKE IT TO {P_OBJ}, AND TO NOBODY ELSE.", "PUT IT IN {P_POSS} OWN HANDS."};
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as);
    desc = "CARRY A PARCEL FROM {GIVER} TO {S}, THE {R} OF {T} ({D} OF {H}). {S} PAYS ON DELIVERY.";
  }
  return true;
}

// Heirloom: a keepsake (or a temple relic) in an old chest in a cave or ruin
bool Game::offerHeirloom(OfferCtx& oc) {
  Quest& q = oc.q;
  Rng& r = oc.r;
  const int32_t hx = oc.hx;
  const int32_t hy = oc.hy;
  Vars& V = oc.V;
  const uint8_t need = oc.need;
  bool& danger = oc.danger;
  std::string& title = oc.title;
  std::string& reason = oc.reason;
  std::string& ask = oc.ask;
  std::string& desc = oc.desc;
  const int t = pickRadiant(r.f() < 0.5f ? SiteType::Ruin : SiteType::Cave, hx, hy, r, danger);
  if (t < 0) return false;
  q.type = QType::Heirloom;
  oc.atSite(t);
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
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as);
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
  title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as);
  desc = "{GIVER}'S {REL} WAS LOST IN {T} ({D} OF {H}). FIND {OBJ} IN AN OLD CHEST THERE AND BRING IT BACK.";
  return true;
}

// Stolen (an Heirloom, P1): a merchant's strongbox: the robbers hid it in a cave
bool Game::offerStolen(OfferCtx& oc) {
  Quest& q = oc.q;
  Rng& r = oc.r;
  const int32_t hx = oc.hx;
  const int32_t hy = oc.hy;
  Vars& V = oc.V;
  bool& danger = oc.danger;
  std::string& title = oc.title;
  std::string& reason = oc.reason;
  std::string& ask = oc.ask;
  std::string& desc = oc.desc;
  const int t = pickRadiant(SiteType::Cave, hx, hy, r, danger);
  if (t < 0) return false;
  q.type = QType::Heirloom;
  oc.atSite(t);
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
  title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as);
  desc = "ROBBERS HID {GIVER}'S {OBJ} IN {T} ({D} OF {H}). FIND IT IN A CHEST THERE AND BRING IT BACK.";
  return true;
}

// Supply (P1): the goods the giver's trade works, from a neighbour that sells them (life_quests.cpp)
bool Game::offerSupply(OfferCtx& oc) {
  Quest& q = oc.q;
  Rng& r = oc.r;
  const Actor& a = oc.a;
  Vars& V = oc.V;
  const uint8_t need = oc.need;
  std::string& title = oc.title;
  std::string& reason = oc.reason;
  std::string& ask = oc.ask;
  std::string& desc = oc.desc;
  q.flags = 0;   // (no person seed: bits 20-21 are the supply run's own flags)
  const life::Job job = JobOps::censusJob(*this, a);
  const ew::Good gd = JobOps::tradeGood(a.role, job, need);
  if (gd == ew::Good::COUNT) return false;
  Quest sq;
  if (!jobsSupplyQuest(*this, a, gd, job, sq)) return false;
  q.type = QType::Supply;
  q.stage = sq.stage; q.need = sq.need; q.have = 0; q.gold = sq.gold; q.xp = sq.xp; q.subject = sq.subject;
  oc.atSite(sq.target);
  V.set("GOOD", q.subject);
  V.set("UNITS", goodUnits(q.need, q.subject));
  if (need == jobs::JN_FAMINE) {
    static const char* const ti[] = {"FOOD FOR {H}", "THE HUNGRY WEEKS", "{GOOD} FROM {T}", "BEFORE THE STORES RUN DRY"};
    static const char* const re[] = {"THE GRANARIES ARE EMPTY AND THE CHILDREN CRY AT NIGHT. {T} STILL HAS {GOOD}.",
                                     "THIS FAMINE WILL TAKE THE OLD ONES FIRST. {T}, {W} {D}, HAS {GOOD} TO SPARE.",
                                     "WE'VE EATEN THE SEED CORN. IF NOBODY BRINGS {GOOD} FROM {T}, THERE'LL BE NO SPRING."};
    static const char* const as[] = {"BRING US {UNITS}.", "FETCH {UNITS} AND I'LL SEE IT SHARED OUT FAIRLY."};
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as);
  } else {
    static const char* const ti[] = {"{GOOD} FROM {T}", "SHORT OF {GOOD}", "THE {GOOD} RUN", "AN ORDER OF {GOOD}"};
    static const char* const re[] = {"MY WORK STANDS IDLE: NOT A SCRAP OF {GOOD} LEFT, AND THE CARTER IS A WEEK LATE. {T} SELLS IT.",
                                     "I'VE ORDERS I CAN'T FILL WITHOUT {GOOD}. {T}, {W} {D}, HAS PLENTY.",
                                     "THE LAST LOT OF {GOOD} WAS RUBBISH. {T} SELLS THE GOOD STUFF, {W} {D} OF HERE."};
    static const char* const as[] = {"BRING ME {UNITS}.", "FETCH {UNITS}. THEY KNOW ME THERE."};
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as);
  }
  desc = sq.desc;
  return true;
}

// Missing: a named person (or a stray herder) alive in a cave
bool Game::offerMissing(OfferCtx& oc) {
  Quest& q = oc.q;
  Rng& r = oc.r;
  const int32_t hx = oc.hx;
  const int32_t hy = oc.hy;
  Vars& V = oc.V;
  const uint8_t need = oc.need;
  bool& danger = oc.danger;
  std::string& title = oc.title;
  std::string& reason = oc.reason;
  std::string& ask = oc.ask;
  std::string& desc = oc.desc;
  const int t = pickRadiant(SiteType::Cave, hx, hy, r, danger);
  if (t < 0) return false;
  q.type = QType::Missing;
  oc.atSite(t);
  Actor d;
  d.site = q.giverSite;   // M3: the missing relative shares the giver's culture (name, dress); spawnEscort matches
  Rng pr = personRng(q, seed);
  makeLook(d, Role::Villager, pr);
  q.subject = d.name;
  const bool fem = personFemale(q, seed);
  oc.pron(fem);
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
    title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as);
    desc = "{S}, WHO WORKS {GIVER}'S FIELDS, WENT AFTER A STRAYED {BEAST} AND WAS LOST IN {T} ({D} OF {H}). FIND {P_OBJ} AND LEAD {P_OBJ} OUT.";
    return true;
  }
  static const char* const ti[] = {"MISSING: {S}", "FIND {S}", "{S} HAS NOT COME HOME", "WHERE IS {S}?"};
  static const char* const re[] = {"MY {REL2} {S} WENT INTO {T} {X} DAYS AGO AND HAS NOT COME BACK.",
                                   "MY {REL2} {S} WENT LOOKING FOR {LOOK} NEAR {T} AND NEVER CAME BACK. SOMEONE SAW A LIGHT IN THE CAVE.",
                                   "MY {REL2} {S} WAS TAKEN. THE TRACKS LEAD TO {T}, {W} {D}."};
  static const char* const as[] = {"BRING {P_OBJ} HOME ALIVE.", "FIND {P_OBJ}. PLEASE."};
  title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as);
  desc = "{S}, {GIVER}'S {REL2}, WENT MISSING IN {T} ({D} OF {H}). FIND {P_OBJ} ALIVE AND LEAD {P_OBJ} OUT.";
  return true;
}

// Protect: hold the fields tonight
bool Game::offerProtect(OfferCtx& oc) {
  Quest& q = oc.q;
  Rng& r = oc.r;
  const Actor& a = oc.a;
  const Site& home = oc.home;
  Vars& V = oc.V;
  std::string& title = oc.title;
  std::string& reason = oc.reason;
  std::string& ask = oc.ask;
  std::string& desc = oc.desc;
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
  title = oc.pickN(r, ti); reason = oc.pickN(r, re); ask = oc.pickN(r, as); desc = oc.pickN(r, de);
  return true;
}

// the reward and the spoken pitch of the offer that was made
void Game::offerPitch(OfferCtx& oc, int madeKind) {
  Quest& q = oc.q;
  Rng& r = oc.r;
  const Actor& a = oc.a;
  const Site& home = oc.home;
  const int32_t hx = oc.hx;
  const int32_t hy = oc.hy;
  Vars& V = oc.V;
  bool& danger = oc.danger;
  int& targetLevel = oc.targetLevel;
  std::string& title = oc.title;
  std::string& reason = oc.reason;
  std::string& ask = oc.ask;
  std::string& desc = oc.desc;
  const Kingdom* K = oc.K;
  const OfferKind made = (OfferKind)madeKind;
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
}
