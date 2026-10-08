// M5 "Hearth and Hall": the needs-driven citizens in aggregate (VISION_PLAN 10.2, 15.2, 15.12). CITIZENS lane.
//   - plans: the job templates (10.2) are the backbone; inside each template's free windows (and at the edges of its
//     fixed ones) a resident picks what best serves its most pressing need (15.12 utility scoring, integers and hashes
//     only: deterministic per resident, day and hour): the hungry eat (at home, a stall, the inn; a meal at the post in
//     a long shift), the lonely go to the gathering place, the broke work longer or beg, the devout pray, the weary go
//     home early. Children, elders, nobles, beggars and refugees keep their own rules; festivals, famine, war and
//     grief bend them.
//   - the hourly aggregate (HOUR LOD): needs decay by trait and refill by what was done; meals consume the settlement's
//     real stock (bread, produce, fish, meat, grain) and cost coin; work earns wages and produces by the 15.11 chains
//     (ew::recipes at the workplace; raw goods in the fields, the docks, the mine); ties strengthen and form among the
//     people sharing a table; occupancy per building for the view.
//   - the daily update: prices from stock against use, imports (the countryside and the caravans, by the realm's food;
//     none in a famine), light trade between loaded neighbours along the roads (never across a war), the realm's flags
//     read back (famine, sieges, occupation, burning, unrest, refugees), the residents' day reported to the realm
//     (Realm::lifeReport: the 15.6.3 chain), festivals, emigration and return, refugees.
//   - LOD and budget: censuses are built in slices (census.cpp) and the hour's aggregate runs settlement by settlement
//     under a per-step budget, so no Game::update step pays for a whole capital at once.
#include <algorithm>
#include <chrono>
#include <cmath>
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/life.h"

namespace life {

namespace {
double nowMs() {
  using namespace std::chrono;
  return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}
uint32_t h32(uint64_t a, uint64_t b) { return (uint32_t)(ew::mix64(a * 0x9E3779B97F4A7C15ull ^ ew::mix64(b + 0x9A11ull)) >> 16); }

constexpr int COIN_CAP = 250;
constexpr int HOUR_SLICE_RESIDENTS = 1500;   // the hour's aggregate per step (a capital and its neighbours)
constexpr ew::Good FOODS[5] = {ew::Good::Bread, ew::Good::Meat, ew::Good::Fish, ew::Good::Produce, ew::Good::Grain};
bool isFood(int g) {
  for (ew::Good f : FOODS) if ((int)f == g) return true;
  return false;
}
int purseOf(Job j) {
  switch (j) {
    case Job::Noble: return 160;
    case Job::Merchant: case Job::Innkeeper: case Job::Scholar: return 60;
    case Job::Smith: case Job::Guard: case Job::Baker: case Job::Tailor: return 45;
    case Job::Child: return 10;
    case Job::Elder: return 20;
    case Job::Beggar: return 12;
    default: return 30;
  }
}
int wageOf(Job j) {
  switch (j) {
    case Job::Noble: return 5;
    case Job::Merchant: case Job::Innkeeper: case Job::Scholar: case Job::Smith: case Job::Guard: case Job::Baker: case Job::Tailor:
    case Job::Lamplighter: return 2;
    case Job::Child: case Job::Elder: case Job::Beggar: return 0;
    default: return 1;
  }
}
void addCoin(Resident& r, int d) { r.coin = (uint16_t)std::clamp((int)r.coin + d, 0, COIN_CAP); }
inline void addNeed(uint8_t& v, int d) { v = (uint8_t)std::clamp((int)v + d, 0, 100); }
int stockCap(const Census& c) { return 40 + 4 * (int)c.res.size(); }
void put(Census& c, ew::Good g, int n) {
  uint16_t& s = c.stock[(size_t)g];
  s = (uint16_t)std::min(stockCap(c), (int)s + n);
}
int foodStock(const Census& c) { int n = 0; for (ew::Good f : FOODS) n += c.stock[(size_t)f]; return n; }
int alive(const Census& c) { int n = 0; for (const Resident& r : c.res) n += !(r.flags & (RF_DEAD | RF_AWAY)); return n; }
// what a day of meals for its people takes (2.5 a head)
int dailyFood(const Census& c) { return std::max(4, alive(c) * 5 / 2); }

void reprice(Census& c) {
  for (int g = 0; g < (int)ew::Good::COUNT; g++) {
    const int D = std::max<int>(c.use[(size_t)g], isFood(g) ? 8 : 2);
    const int S = c.stock[(size_t)g];
    c.price[(size_t)g] = (uint8_t)std::clamp(100 * (2 * D + 8) / (S + D + 8), 40, 250);
  }
}
}  // namespace

const char* jobName(Job j) {
  static const char* n[] = {"NOBODY", "FARMER", "FISHER", "MINER", "WOODCUTTER", "HUNTER", "HERDER", "SMITH", "MERCHANT", "BAKER",
                            "TAILOR", "STABLEHAND", "INNKEEPER", "SERVER", "BARD", "SCHOLAR", "PRIEST", "GUARD", "NOBLE",
                            "SERVANT", "LABOURER", "LAMPLIGHTER", "CHILD", "ELDER", "BEGGAR"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Job::COUNT, "a name for every job");
  return (int)j < (int)Job::COUNT ? n[(int)j] : "";
}
Role jobRole(Job j) {
  switch (j) {
    case Job::Farmer: case Job::Herder: case Job::Woodcutter: case Job::Miner: return Role::Farmer;
    case Job::Fisher: return Role::Fisher;
    case Job::Hunter: return Role::Hunter;
    case Job::Smith: return Role::Smith;
    case Job::Merchant: case Job::Baker: case Job::Tailor: return Role::Merchant;
    case Job::Innkeeper: return Role::Innkeeper;
    case Job::Priest: return Role::Priest;
    case Job::Guard: return Role::Guard;
    case Job::Noble: return Role::Jarl;
    case Job::Scholar: return Role::Mage;
    case Job::Child: return Role::Child;
    default: return Role::Villager;
  }
}
const char* actName(Act a) {
  static const char* n[] = {"SLEEP", "EAT", "WORK", "WANDER", "SOCIALISE", "TAVERN", "PRAY", "PATROL", "PLAY", "PERFORM", "SHOP",
                            "HOME", "BEG", "BATHE", "LIGHTLAMPS", "BRAWL", "EMIGRATE"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Act::COUNT, "a name for every activity");
  return (int)a < (int)Act::COUNT ? n[(int)a] : "";
}
const char* placeName(Place p) {
  static const char* n[] = {"HOME", "WORK", "GATHERING", "GATHERING2", "PLAZA", "TEMPLE", "FIELD", "GATE", "MARKET", "STREET", "AWAY"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Place::COUNT, "a name for every place");
  return (int)p < (int)Place::COUNT ? n[(int)p] : "";
}
const char* needName(Need n) {
  static const char* t[] = {"HUNGER", "REST", "SOCIAL", "FAITH", "MONEY"};
  return (int)n < NEEDS ? t[(int)n] : "";
}
const char* buffName(uint8_t b) {
  switch (b) {
    case BUFF_WELLFED: return "WELL FED";
    case BUFF_RESTED: return "RESTED";
    case BUFF_HUNGRY: return "HUNGRY";
    case BUFF_WEARY: return "WEARY";
    default: return "";
  }
}
const char* moodFlagName(uint16_t bit) {
  switch (bit) {
    case MF_CONTENT: return "CONTENT";
    case MF_FESTIVAL: return "FESTIVAL";
    case MF_HUNGRY: return "HUNGRY";
    case MF_FAMINE: return "FAMINE";
    case MF_WARTORN: return "WARTORN";
    case MF_GRIEF: return "GRIEF";
    case MF_BRAWLS: return "BRAWLS";
    case MF_EMIGRATING: return "EMIGRATING";
    case MF_SHUTTERED: return "SHUTTERED";
    case MF_RAIDED: return "RAIDED";
    default: return "";
  }
}

ew::Gid npcId(ew::Gid site, int idx) { return ew::mix64(site * 0x9E3779B97F4A7C15ull ^ ((uint64_t)(uint32_t)idx + 0x4E50C1Dull)) | 1ull; }

// ---------------------------------------------------------------- job templates (VISION_PLAN 10.2)
const Block* jobTemplate(Job j, int shift, int& n) {
  using A = Act;
  using P = Place;
#define T(name, ...) static const Block name[] = {__VA_ARGS__}
  T(field, {0, 5, A::Sleep, P::Home, 100}, {5, 6, A::Eat, P::Home, 100}, {6, 12, A::Work, P::Field, 100}, {12, 13, A::Eat, P::Home, 100},
    {13, 18, A::Work, P::Field, 100}, {18, 21, A::Tavern, P::Gathering, 40}, {21, 24, A::Sleep, P::Home, 100});
  T(smith, {0, 7, A::Sleep, P::Home, 100}, {7, 12, A::Work, P::Work, 100}, {12, 13, A::Eat, P::Gathering, 100}, {13, 19, A::Work, P::Work, 100},
    {19, 22, A::Tavern, P::Gathering, 50}, {22, 24, A::Sleep, P::Home, 100});
  T(trade, {0, 7, A::Sleep, P::Home, 100}, {7, 8, A::Eat, P::Home, 100}, {8, 12, A::Work, P::Work, 100}, {12, 13, A::Eat, P::Home, 100},
    {13, 18, A::Work, P::Work, 100}, {18, 21, A::Tavern, P::Gathering, 50}, {21, 24, A::Sleep, P::Home, 100});
  T(baker, {0, 4, A::Sleep, P::Home, 100}, {4, 12, A::Work, P::Work, 100}, {12, 13, A::Eat, P::Home, 100}, {13, 16, A::Work, P::Work, 100},
    {16, 20, A::Home, P::Home, 100}, {20, 24, A::Sleep, P::Home, 100});
  T(inn, {0, 1, A::Work, P::Work, 100}, {1, 9, A::Sleep, P::Home, 100}, {9, 10, A::Eat, P::Work, 100}, {10, 24, A::Work, P::Work, 100});
  T(bard, {0, 1, A::Perform, P::Gathering, 100}, {1, 11, A::Sleep, P::Home, 100}, {11, 13, A::Eat, P::Gathering, 100},
    {13, 17, A::Wander, P::Plaza, 100}, {17, 18, A::Eat, P::Gathering, 100}, {18, 24, A::Perform, P::Gathering, 100});
  T(priest, {0, 6, A::Sleep, P::Home, 100}, {6, 20, A::Work, P::Work, 100}, {20, 24, A::Sleep, P::Home, 100});
  T(guardDay, {0, 6, A::Sleep, P::Home, 100}, {6, 18, A::Patrol, P::Gate, 100}, {18, 22, A::Tavern, P::Gathering, 50}, {22, 24, A::Sleep, P::Home, 100});
  T(guardNight, {0, 6, A::Patrol, P::Gate, 100}, {6, 14, A::Sleep, P::Home, 100}, {14, 17, A::Wander, P::Plaza, 100}, {17, 18, A::Eat, P::Home, 100},
    {18, 24, A::Patrol, P::Gate, 100});
  T(noble, {0, 8, A::Sleep, P::Home, 100}, {8, 9, A::Eat, P::Home, 100}, {9, 12, A::Work, P::Work, 100}, {12, 13, A::Eat, P::Home, 100},
    {13, 17, A::Work, P::Work, 100}, {17, 21, A::Socialise, P::Work, 100}, {21, 24, A::Sleep, P::Home, 100});
  T(servant, {0, 6, A::Sleep, P::Home, 100}, {6, 21, A::Work, P::Work, 100}, {21, 24, A::Sleep, P::Home, 100});
  T(labour, {0, 6, A::Sleep, P::Home, 100}, {6, 7, A::Eat, P::Home, 100}, {7, 12, A::Work, P::Street, 100}, {12, 13, A::Eat, P::Gathering, 60},
    {13, 18, A::Work, P::Street, 100}, {18, 21, A::Tavern, P::Gathering, 45}, {21, 24, A::Sleep, P::Home, 100});
  T(lamp, {0, 5, A::Sleep, P::Home, 100}, {5, 7, A::LightLamps, P::Street, 100}, {7, 14, A::Sleep, P::Home, 100}, {14, 18, A::Wander, P::Plaza, 100},
    {18, 19, A::Eat, P::Home, 100}, {19, 21, A::LightLamps, P::Street, 100}, {21, 24, A::Tavern, P::Gathering, 50});
  T(child, {0, 7, A::Sleep, P::Home, 100}, {7, 12, A::Play, P::Plaza, 100}, {12, 13, A::Eat, P::Home, 100}, {13, 18, A::Play, P::Plaza, 100},
    {18, 20, A::Home, P::Home, 100}, {20, 24, A::Sleep, P::Home, 100});
  T(elder, {0, 7, A::Sleep, P::Home, 100}, {7, 8, A::Eat, P::Home, 100}, {8, 11, A::Socialise, P::Plaza, 100}, {11, 12, A::Pray, P::Temple, 50},
    {12, 13, A::Eat, P::Home, 100}, {13, 17, A::Home, P::Home, 100}, {17, 20, A::Socialise, P::Gathering, 30}, {20, 24, A::Sleep, P::Home, 100});
  T(beggar, {0, 7, A::Sleep, P::Street, 100}, {7, 20, A::Beg, P::Market, 100}, {20, 24, A::Sleep, P::Street, 100});
#undef T
#define R(arr) do { n = (int)(sizeof(arr) / sizeof(arr[0])); return arr; } while (0)
  switch (j) {
    case Job::Farmer: case Job::Fisher: case Job::Miner: case Job::Woodcutter: case Job::Hunter: case Job::Herder: R(field);
    case Job::Smith: R(smith);
    case Job::Merchant: case Job::Tailor: case Job::Stablehand: case Job::Scholar: R(trade);
    case Job::Baker: R(baker);
    case Job::Innkeeper: case Job::Server: R(inn);
    case Job::Bard: R(bard);
    case Job::Priest: R(priest);
    case Job::Guard: if (shift) R(guardNight); R(guardDay);
    case Job::Noble: R(noble);
    case Job::Servant: R(servant);
    case Job::Lamplighter: R(lamp);
    case Job::Child: R(child);
    case Job::Elder: R(elder);
    case Job::Beggar: R(beggar);
    default: R(labour);
  }
#undef R
  n = 0;   // (unreachable: every case returns)
  return nullptr;
}

// ---------------------------------------------------------------- plans (15.12 utility inside the 10.2 windows)
Plan Life::plan(const World& w, const Census& c, const Resident& r, int day, float hour) const {
  (void)w;
  Plan p;
  if (r.flags & (RF_DEAD | RF_AWAY)) { p.act = Act::Emigrate; p.place = Place::Away; return p; }
  float h = hour + (float)r.jitter * (5.0f / 60.0f);
  if (h < 0) h += 24.0f;
  if (h >= 24.0f) h -= 24.0f;
  const int hi = std::clamp((int)h, 0, 23);
  int n = 0;
  const Block* t = jobTemplate(r.job, r.shift, n);
  int bi = 0;
  for (int i = 0; i < n; i++)
    if (h >= (float)t[i].from && h < (float)t[i].to) { bi = i; break; }
  const Block& b = t[bi];
  p.act = b.act;
  p.place = b.place;
  bool free = b.act == Act::Home || b.act == Act::Wander || b.act == Act::Socialise || b.act == Act::Tavern;
  if (b.prob < 100 && h32(c.seed ^ r.idx, (uint64_t)day * 31u + b.from) % 100u >= b.prob) { p.act = Act::Home; p.place = Place::Home; free = true; }
  const uint16_t mf = c.moodFlags;
  const bool festival = c.festivalDay == day;
  const int hunger = r.need[(int)Need::Hunger], rest = r.need[(int)Need::Rest], social = r.need[(int)Need::Social],
            faith = r.need[(int)Need::Faith], money = r.need[(int)Need::Money];
  const bool grieving = (r.flags & RF_GRIEVING) != 0;
  const uint32_t dice = h32(c.seed ^ ((uint64_t)r.idx << 20), (uint64_t)day * 24u + (uint64_t)hi);
  const int len = b.to - b.from, into = hi - b.from;
  // ---- people with their own rules
  if (r.job == Job::Child) {
    if (hi == 7 || hi == 18) { p.act = Act::Eat; p.place = Place::Home; }   // breakfast and supper at home
    else if (p.act == Act::Play && hunger < 35 && (hi == 10 || hi == 16)) { p.act = Act::Eat; p.place = Place::Home; }
    else if (p.act == Act::Play && (mf & (MF_WARTORN | MF_RAIDED | MF_FAMINE)) && dice % 3u == 0) { p.act = Act::Home; p.place = Place::Home; }
    // most children play in their own lane by the door (a few go to the square: it is not a playground for the town's
    // whole brood); the feast brings them all
    if (p.act == Act::Play && p.place == Place::Plaza && h32(c.seed ^ ((uint64_t)r.idx << 8), (uint64_t)day + 0xC41Du) % 3u != 0) p.place = Place::Street;
    if (festival && hi >= 13 && hi < 21 && p.act != Act::Eat) { p.act = Act::Play; p.place = Place::Plaza; }
    free = false;
  } else if (r.job == Job::Beggar) {
    // alms meals at the temple's door (or the gathering place) morning, noon and evening
    if (p.act == Act::Beg && (hi == 8 || hi == 13 || hi == 18) && hunger < 80) { p.act = Act::Eat; p.place = c.temple >= 0 ? Place::Temple : Place::Gathering; }
    if (festival && hi >= 17 && hi < 20) { p.act = Act::Socialise; p.place = Place::Plaza; }
    free = false;
  }
  // ---- the fixed blocks bend a little
  if (!free) {
    const bool shift = p.act == Act::Work || p.act == Act::Patrol || p.act == Act::Perform;
    if (shift && r.job != Job::Child) {
      // a meal at the post in a long shift (priests, servants, guards, the inn's staff): one in the middle, two in a
      // very long one
      // (the bard sups before the evening's playing, never in the middle of it: the hall would fall silent)
      const bool mealHour = r.job != Job::Bard && (len >= 12 ? (into == len / 3 || into == (2 * len) / 3) : (len >= 6 && into == len / 2));
      if (mealHour && hunger < 80) p.act = Act::Eat;
      // the lazy and tired leave an hour early; the broke stay an hour longer (below, in the free window)
      else if (p.act == Act::Work && (r.traits & TR_LAZY) && rest < 35 && into == len - 1) { p.act = Act::Home; p.place = Place::Home; free = true; }
      // a festival: work stops at 14:00 except for those the feast needs
      else if (festival && hi >= 14 && p.act == Act::Work && r.job != Job::Innkeeper && r.job != Job::Server && r.job != Job::Priest &&
               r.job != Job::Bard && r.job != Job::Guard && r.job != Job::Lamplighter) { p.act = Act::Home; p.place = Place::Home; free = true; }
      // shuttered stalls: a merchant with nothing to sell goes home (the shopkeepers with a door stay at it)
      else if ((mf & MF_SHUTTERED) && r.job == Job::Merchant && r.keySlot < 0 && p.act == Act::Work && (dice & 1u)) { p.act = Act::Home; p.place = Place::Home; free = true; }
      // the priest's sermon in the plaza on festival mornings
      if (festival && r.job == Job::Priest && hi == 10) { p.act = Act::Work; p.place = Place::Plaza; }
      if (festival && r.job == Job::Bard && p.act == Act::Perform && hi >= 18) p.place = Place::Plaza;
    } else if (p.act == Act::Eat) {
      // a hungry worker on the way in buys bread at the market instead of a cold breakfast at home
      if (p.place == Place::Home && r.home < 0) p.place = c.temple >= 0 && (r.flags & RF_REFUGEE) ? Place::Temple : Place::Gathering;
      else if (p.place == Place::Home && hunger < 40 && r.coin >= 3 && c.tier >= 1 && hi < 10 && (dice % 3u) == 0) p.place = Place::Market;
    } else if (p.act == Act::Sleep) {
      // night owls stay up an hour past an evening bedtime; on a festival night the late sleepers dance till 23:00
      if (b.from >= 20 && b.from < 24 && into == 0 && (r.traits & TR_NIGHTOWL) && r.job != Job::Baker) free = true;
      if (festival && b.from >= 21 && hi < 23 && r.job != Job::Baker) free = true;
      if (r.home < 0 && (r.flags & RF_REFUGEE)) p.place = c.temple >= 0 ? Place::Temple : Place::Gathering;
    }
  }
  // ---- the free windows: the most pressing need wins (15.12 utility; integer scores plus a little per-hour dice)
  if (free && r.job != Job::Child && r.job != Job::Beggar) {
    struct Cand { Act a; Place pl; int s; };
    Cand best{Act::Wander, Place::Plaza, 15 + (int)(dice % 13u)};
    auto offer = [&](Act a, Place pl, int s, int k) {
      s += (int)((dice >> (3 * k)) % 13u);
      if (s > best.s) best = Cand{a, pl, s};
    };
    const bool evening = hi >= 17 && hi < 23;
    const bool late = hi >= 21 || hi < 5;
    const bool adult = r.job != Job::Elder;
    // the template's own way of spending the window keeps a fair claim (a lord's evening in his hall, an elder's
    // morning on the bench by the well, the bard's afternoon stroll)
    if (b.act == Act::Socialise || b.act == Act::Wander) offer(b.act, b.place, 50 + (r.job == Job::Noble ? 40 : 0), 10);
    // the feast: everyone dances in the square of an evening
    if (festival && evening) offer(Act::Socialise, Place::Plaza, 95 - (rest < 20 ? 40 : 0), 0);
    // the template's evening at the gathering place (its 10.2 odds passed): the tavern is where the evening goes,
    // supper included, unless a need pulls harder
    const bool tavernNight = b.act == Act::Tavern && p.act == Act::Tavern;
    // hunger: supper at home (or the inn, for those with coin and company in mind)
    if (hunger < 70) {
      const bool inn = tavernNight || r.home < 0 || ((r.traits & TR_SOCIABLE) && r.coin >= 4 && (dice % 3u) == 0);
      offer(Act::Eat, inn ? Place::Gathering : Place::Home, (100 - hunger) * 3 / 2 + (evening ? 30 : 0), 1);
    }
    // company: the gathering place (a tavern, the mead hall, the tea house, the baths...)
    {
      int s = (100 - social) * ((r.traits & TR_SOCIABLE) ? 5 : 4) / 4 + (evening ? 15 : -30) + (r.coin > 0 ? 10 : -20);
      if (grieving) s -= 40;
      if (mf & MF_CONTENT) s += 10;
      if (late) s -= 25;
      if (r.job == Job::Noble) s -= 60;   // lords keep their own hall (their template's evening)
      if (tavernNight) s += 60;
      const Act a = c.gatherKind == 2 /* cult::Gathering::Bathhouse */ && !evening ? Act::Bathe : Act::Tavern;
      const Place where = (c.gathering2 >= 0 && (dice % 4u) == 0) ? Place::Gathering2 : Place::Gathering;
      offer(a, where, s, 2);
      offer(Act::Socialise, Place::Plaza, (100 - social) * 3 / 4 + (hi < 20 ? 10 : -20) + (late ? -40 : 0), 3);
    }
    // faith: the temple (or the shrine in the square)
    offer(Act::Pray, Place::Temple, (100 - faith) * ((r.traits & TR_DEVOUT) ? 3 : 2) / 2 - 25 + (grieving ? 35 : 0) + (late ? -40 : 0), 4);
    // money: work an hour or two longer straight after the shift, or beg
    if (adult && r.job != Job::Noble && money < 30 && bi > 0 && t[bi - 1].act == Act::Work && hi - t[bi - 1].to < 2 && hi >= t[bi - 1].to)
      offer(Act::Work, Place::Work, (100 - money) + ((r.traits & TR_THRIFTY) ? 20 : 0), 5);
    if (adult && r.job != Job::Noble && r.coin == 0 && money < 10 && c.tier >= 1 && hi >= 8 && hi < 20)
      offer(Act::Beg, Place::Market, 40 + ((mf & (MF_HUNGRY | MF_FAMINE)) ? 30 : 0), 6);
    // rest: home early, to bed early when worn out
    if (rest < 25 && hi >= 19) offer(Act::Sleep, Place::Home, 140, 7);
    offer(Act::Home, Place::Home, (100 - rest) * 3 / 2 + 10 + (late ? 45 : 0) + (r.job == Job::Elder ? 15 : 0), 8);
    // a purse to spend: the market
    if (money > 70 && hi >= 9 && hi < 19) offer(Act::Shop, Place::Market, 25 + (money - 70), 9);
    p.act = best.a;
    p.place = best.pl;
    // tempers fray: brawls in the tavern of an unhappy town
    if (p.act == Act::Tavern && (mf & MF_BRAWLS) && ((r.traits & TR_GRUMPY) || r.mood < 35) && (dice % 3u) == 0) p.act = Act::Brawl;
  }
  // ---- where
  switch (p.place) {
    case Place::Home: p.bldg = r.home; break;
    case Place::Work: p.bldg = r.work; if (r.work < 0) p.place = r.job == Job::Guard ? Place::Gate : Place::Street; break;
    case Place::Gathering: p.bldg = c.gathering; if (c.gathering < 0) p.place = Place::Plaza; break;
    case Place::Gathering2: p.bldg = c.gathering2 >= 0 ? c.gathering2 : c.gathering; if (p.bldg < 0) p.place = Place::Plaza; break;
    case Place::Temple: p.bldg = c.temple; if (c.temple < 0) p.place = Place::Plaza; break;
    case Place::Market: p.bldg = -1; break;   // the stalls (the view's market rows); shops keep their own doors
    default: p.bldg = -1; break;
  }
  if (p.place == Place::Home && r.home < 0) p.place = Place::Street;
  return p;
}

// ---------------------------------------------------------------- the census store
Census* Life::finishBuild(const World& w) {
  if (build_.site < 0) return nullptr;
  Build b = std::move(build_);
  const ew::Gid id = buildGid_;
  build_ = Build();
  buildGid_ = 0;
  return install(w, b, id);
}

Census* Life::install(const World& w, Build& b, ew::Gid id) {
  const int si = b.site;
  Census& c = sites_[id];
  c = std::move(b.c);
  auto pd = pending_.find(c.site);
  if (pd != pending_.end()) {
    if (pd->second.refugees > c.refugees) addRefugees(w, c, pd->second.refugees - c.refugees);
    applyRecord(c, pd->second);
    pending_.erase(pd);
  }
  c.handle = si;
  stats.censuses++;
  stats.residents += (int)c.res.size();
  stats.worstCensusMs = std::max(stats.worstCensusMs, b.ms);
  return &c;
}

Census* Life::census(const World& w, int site) {
  if (site < 0 || site >= (int)w.sites.size()) return nullptr;
  const Site& s = w.sites[(size_t)site];
  if (!s.settlement() || !s.id) return nullptr;
  auto it = sites_.find(s.id);
  if (it != sites_.end()) { it->second.handle = site; return &it->second; }
  if (s.bldgCount <= 0) return nullptr;
  const double t0 = nowMs();
  if (build_.site == site && buildGid_ == s.id) {   // a slice-built census wanted now: finish it
    buildCensusStep(w, build_, 0);
    build_.ms += nowMs() - t0;
    return finishBuild(w);
  }
  Build b;
  b.site = site;
  buildCensusStep(w, b, 0);
  b.ms = nowMs() - t0;
  return install(w, b, s.id);
}
const Census* Life::find(ew::Gid site) const { auto it = sites_.find(site); return it == sites_.end() ? nullptr : &it->second; }
Census* Life::findMut(ew::Gid site) { auto it = sites_.find(site); return it == sites_.end() ? nullptr : &it->second; }

uint8_t Life::mood(ew::Gid site) const { const Census* c = find(site); return c ? c->mood : 60; }
uint16_t Life::moodFlags(ew::Gid site) const { const Census* c = find(site); return c ? c->moodFlags : MF_CONTENT; }
bool Life::festival(ew::Gid site, int day) const { const Census* c = find(site); return c && c->festivalDay == day; }
int Life::occupants(const World& w, int site, int bldgOff) const {
  if (site < 0 || site >= (int)w.sites.size()) return -1;
  const Census* c = find(w.sites[(size_t)site].id);
  if (!c || c->occHour < 0 || bldgOff < 0 || bldgOff >= (int)c->occ.size()) return -1;
  return c->occ[(size_t)bldgOff];
}

std::vector<int> Life::friendsOf(const Census& c, int idx) const {
  std::vector<std::pair<int, int>> v;
  if (idx >= 0 && idx < (int)c.tiesOf.size()) {
    for (uint32_t ti : c.tiesOf[(size_t)idx]) {
      if (ti >= c.ties.size()) continue;
      const Tie& t = c.ties[ti];
      if (t.b == PLAYER_TIE) continue;
      v.push_back({-(int)t.bond, t.a == idx ? (int)t.b : (int)t.a});
    }
  } else {
    for (const Tie& t : c.ties) {
      if (t.b == PLAYER_TIE) continue;
      if (t.a == idx) v.push_back({-(int)t.bond, (int)t.b});
      else if (t.b == idx) v.push_back({-(int)t.bond, (int)t.a});
    }
  }
  std::sort(v.begin(), v.end());
  std::vector<int> out;
  out.reserve(v.size());
  for (auto& p : v) out.push_back(p.second);
  return out;
}

void Life::residentDied(ew::Gid site, int idx, int day) {
  Census* c = findMut(site);
  if (!c || idx < 0 || idx >= (int)c->res.size()) return;
  Resident& r = c->res[(size_t)idx];
  if (r.flags & RF_DEAD) return;
  r.flags |= RF_DEAD;
  r.actor = -1;
  auto grieve = [&](int k) {
    if (k < 0 || k >= (int)c->res.size() || (c->res[(size_t)k].flags & RF_DEAD)) return;
    c->res[(size_t)k].flags |= RF_GRIEVING;
    c->res[(size_t)k].griefDay = (uint16_t)std::max(0, day);
  };
  for (const Resident& o : c->res)
    if (o.idx != r.idx && o.household == r.household) grieve(o.idx);
  if (r.spouse >= 0) grieve(r.spouse);
  if (r.parent >= 0) grieve(r.parent);
  for (int f : friendsOf(*c, idx)) grieve(f);
  if (c->takenIdx == idx) { c->takenIdx = -1; c->takenQuest = 0; }
}

bool Life::feed(ew::Gid site, int idx, int quality) {
  Census* c = findMut(site);
  if (!c || idx < 0 || idx >= (int)c->res.size()) return false;
  Resident& r = c->res[(size_t)idx];
  if (r.flags & (RF_DEAD | RF_AWAY)) return false;
  r.need[(int)Need::Hunger] = (uint8_t)std::min(100, r.need[(int)Need::Hunger] + 30 + 15 * std::max(0, quality));
  r.flags |= RF_FED;
  r.sinceMeal = 0;
  if (r.meals < 255) r.meals++;
  return true;
}
bool Life::employ(ew::Gid site, int idx, int day) {
  (void)day;
  Census* c = findMut(site);
  if (!c || idx < 0 || idx >= (int)c->res.size()) return false;
  Resident& r = c->res[(size_t)idx];
  if (r.flags & (RF_DEAD | RF_AWAY) || r.job == Job::Child) return false;
  r.flags |= RF_EMPLOYED;
  addCoin(r, 10);
  r.need[(int)Need::Money] = (uint8_t)std::min(100, r.need[(int)Need::Money] + 25);
  r.misery = 0;
  return true;
}
bool Life::supply(ew::Gid site, ew::Good g, int units) {
  Census* c = findMut(site);
  if (!c || (int)g >= (int)ew::Good::COUNT || units < 0) return false;
  if (units == 0) { reprice(*c); return false; }   // (nothing given: the prices are brought up to date)
  c->stock[(size_t)g] = (uint16_t)std::min(60000, c->stock[(size_t)g] + units);
  reprice(*c);
  return true;
}
bool Life::befriend(ew::Gid site, int idx) {
  Census* c = findMut(site);
  if (!c || idx < 0 || idx >= (int)c->res.size() || (c->res[(size_t)idx].flags & RF_DEAD)) return false;
  for (Tie& t : c->ties)
    if (t.a == idx && t.b == PLAYER_TIE) { t.bond = (uint8_t)std::min(100, t.bond + 10); return true; }
  c->ties.push_back(Tie{(uint16_t)idx, PLAYER_TIE, 50});
  if (idx < (int)c->tiesOf.size()) c->tiesOf[(size_t)idx].push_back((uint32_t)c->ties.size() - 1);
  c->res[(size_t)idx].flags |= RF_BEFRIENDED;
  return true;
}

// ---------------------------------------------------------------- forcing and reading (scripts, tests)
bool Life::forceFamine(ew::Gid site, int day, int days) {
  Census* c = findMut(site);
  if (!c) return false;
  c->famineUntil = day + std::max(1, days);
  for (ew::Good f : FOODS) c->stock[(size_t)f] = 0;
  c->stock[(size_t)ew::Good::Flour] = 0;
  c->harvestPct = 10;
  reprice(*c);
  c->moodFlags |= MF_FAMINE | MF_SHUTTERED;
  return true;
}
bool Life::forceFestival(ew::Gid site, int day) {
  Census* c = findMut(site);
  if (!c) return false;
  c->festivalDay = day;
  c->moodFlags |= MF_FESTIVAL;
  return true;
}
std::string Life::festivalTitle(const World& w, const Census& c) const {
  const cult::Culture* cu = c.handle >= 0 && c.handle < (int)w.sites.size() ? w.cultureOf(c.handle) : nullptr;
  if (c.festivalKind == FEST_GOD && cu && !cu->faith.names.empty()) return "THE FEAST OF " + cu->faith.names[0];
  return festivalName(c.festivalKind);
}
int Life::needAverage(const Census& c, Need n) {
  int64_t s = 0;
  int k = 0;
  for (const Resident& r : c.res) {
    if (r.flags & (RF_DEAD | RF_AWAY)) continue;
    s += r.need[(int)n];
    k++;
  }
  return k ? (int)(s / k) : 0;
}

void Life::advanceHours(Game& g, int hours) {
  for (int k = 0; k < hours; k++) {
    g.hour += 1.0f;
    if (g.hour >= 24.0f) { g.hour -= 24.0f; g.day++; }
    for (int i = 0; i < 2; i++) { g.update(SIM_DT, Input()); g.events.clear(); }
  }
}

int Life::raidChancePct(int pressure, float military, int doy) {
  if (pressure <= 0) return 0;
  doy = ((doy % 360) + 360) % 360;
  const int season = doy >= 270 ? 130 : doy >= 180 ? 110 : doy >= 90 ? 85 : 100;   // winter hungry beasts .. summer plenty
  const int p = std::min(pressure, 4) * 25;
  const int mil = (int)std::lround(std::clamp(military, 0.0f, 1.0f) * 60.0f);
  const int v = std::min(100, p * (100 - mil) / 100 * season / 100);
  return 2 + 6 * v / 100;
}
bool Life::raidRoll(ew::Gid site, int night, int pct) {
  if (pct <= 0) return false;
  return (int)(h32(site ^ 0x4A1D5EEDull, (uint64_t)(uint32_t)night * 7919u) % 100u) < pct;
}

// ---------------------------------------------------------------- refugees (M4 SS_REFUGEES)
void Life::addRefugees(const World& w, Census& c, int n) {
  if (n <= 0) return;
  const cult::Culture* cu = c.handle >= 0 && c.handle < (int)w.sites.size() ? w.cultureOf(c.handle) : nullptr;
  uint16_t hh = 0;
  for (const Resident& r : c.res) hh = std::max<uint16_t>(hh, (uint16_t)(r.household + 1));
  int head = -1;
  for (int k = 0; k < n; k++) {
    Resident r;
    r.idx = (uint16_t)c.res.size();
    const uint32_t h = h32(c.seed ^ 0x2EF06EEull, r.idx);
    const uint32_t roll = h % 10u;
    r.job = head < 0 || roll < 6 ? Job::Labourer : roll < 9 ? Job::Child : Job::Elder;
    r.female = (h >> 4) & 1u;
    r.age = (uint8_t)(r.job == Job::Child ? 3 + (h >> 6) % 12u : r.job == Job::Elder ? 62 + (h >> 6) % 18u : 18 + (h >> 6) % 35u);
    r.traits = (uint8_t)(((h >> 9) & (h >> 17)) & 0xFFu);
    r.jitter = (int8_t)((int)((h >> 20) % 13u) - 6);
    r.lookSeed = h32(c.seed, (uint64_t)r.idx * 7919u + 0x100Cull);
    r.household = (uint16_t)(hh + (uint16_t)(k / 3));
    if (k % 3 == 0) head = r.idx;
    r.payer = (int16_t)(r.job == Job::Labourer ? r.idx : head);
    if (r.job != Job::Labourer) r.parent = (int16_t)head;
    r.coin = (uint16_t)((h >> 12) % 5u);
    r.need = {{(uint8_t)(35 + (h >> 4) % 20u), (uint8_t)(40 + (h >> 7) % 20u), (uint8_t)(35 + (h >> 10) % 30u), (uint8_t)(40 + (h >> 13) % 30u), 10}};
    r.flags = RF_REFUGEE | RF_HOMELESS;
    r.origin = ORIGIN_REFUGEE;
    r.name = cu ? cult::personName(*cu, h32(c.seed, (uint64_t)r.idx * 104729u + 0x4A4Eull), r.female) : std::string("REFUGEE");
    c.res.push_back(r);
  }
  c.refugees = (uint16_t)(c.refugees + n);
  c.tiesOf.resize(c.res.size());
}

// ---------------------------------------------------------------- the hourly aggregate
void Life::hourTick(Game& g, Census& c, int32_t hourAbs) {
  // (a clock set back by a script or a test counts as one hour)
  const int hours = c.lastHour < 0 || hourAbs < c.lastHour ? 1 : (int)std::min(hourAbs - c.lastHour, 24);
  const int32_t was = c.lastHour;
  c.lastHour = hourAbs;
  if (hours <= 0) return;
  const World& w = g.world;
  const Site& s = w.sites[(size_t)c.handle];
  const int hr = (int)(hourAbs % 24);
  const int day = hourAbs / 24;
  // ---- a new day: yesterday's books are closed first
  if (c.lastDayRun < 0) {
    if (const realm::SettlementState* s0 = g.realm.settlement(c.site)) c.realmFlags = (uint8_t)(s0->flags & 0xFF);
    c.lastDayRun = day;
  }
  if (day > c.lastDayRun) { dayTick(g, c, day); c.lastDayRun = day; }
  if (c.tallyDay != day) {
    const bool whole = c.tallyDay >= 0 && c.hoursToday >= 24;
    for (Resident& r : c.res) {
      if (whole && !(r.flags & (RF_DEAD | RF_AWAY))) {
        stats.dayResidents++;
        if (r.meals >= 2) stats.dayMealsOk++;
        if (r.slept >= 6) stats.daySleepOk++;
      }
      r.mealsY = r.meals; r.sleptY = r.slept; r.meals = 0; r.slept = 0;
    }
    c.tallyDay = day;
    c.hoursToday = 0;
  }
  c.hoursToday = (uint8_t)std::min(255, c.hoursToday + (was >= 0 && hourAbs - was > 1 && hourAbs / 24 == was / 24 ? hours : 1));
  if ((int)c.occ.size() != s.bldgCount) c.occ.assign((size_t)std::max(0, s.bldgCount), 0);
  else std::fill(c.occ.begin(), c.occ.end(), 0);
  const bool festival = c.festivalDay == day;
  if (festival && c.lastFestival != day) { c.lastFestival = day; stats.festivals++; }
  const uint16_t mf = c.moodFlags;
  const int townPen = ((mf & MF_FAMINE) ? 15 : 0) + ((mf & MF_HUNGRY) ? 4 : 0) + ((mf & MF_WARTORN) ? 10 : 0) + ((mf & MF_RAIDED) ? 5 : 0) -
                      (festival ? 10 : 0);
  int moodSum = 0, nAlive = 0, hungry = 0, grieving = 0;
  scratch_.clear();
  const int capS = stockCap(c);
  auto made = [&](ew::Good gd, int k) {
    uint16_t& st = c.stock[(size_t)gd];
    st = (uint16_t)std::min(capS, (int)st + k);
    if (isFood((int)gd)) c.foodMade += k;
  };
  auto take = [&](ew::Good gd) -> bool {
    uint16_t& st = c.stock[(size_t)gd];
    if (!st) return false;
    st--;
    if (c.usedToday[(size_t)gd] < 60000) c.usedToday[(size_t)gd]++;
    return true;
  };
  auto unitPrice = [&](ew::Good gd) { return std::max(1, ((int)c.price[(size_t)gd] + 50) / 100); };
  // a meal: one unit of food from the stores (the resident's turn in the rotation spreads it over what there is)
  auto meal = [&](Resident& r, bool inn) -> bool {
    const int start = (int)((r.idx + (uint32_t)hourAbs) % 5u);
    for (int k = 0; k < 5; k++) {
      const ew::Good f = FOODS[(start + k) % 5];
      if (!take(f)) continue;
      c.foodEaten++;
      const int cost = unitPrice(f) + (inn ? 1 : 0);
      Resident& P = r.payer >= 0 && r.payer < (int)c.res.size() ? c.res[(size_t)r.payer] : r;
      if (P.coin >= cost) addCoin(P, -cost);   // (else on credit or charity: nobody starves for want of coin alone)
      return true;
    }
    c.foodMissed++;
    return false;
  };
  // a gap of hours not lived here (a journey, a night slept, a census built late): the residents lived them as usual,
  // in bulk (their meals out of the stores, their needs back toward an ordinary day's), and this hour is aggregated
  // as one (a single plan stretched over many hours would starve or exhaust them)
  int steps = hours;
  if (hours > 3) {
    for (Resident& r : c.res) {
      if (r.flags & (RF_DEAD | RF_AWAY)) continue;
      const int meals = std::max(1, hours * 5 / 48);
      int ate = 0;
      for (int k = 0; k < meals; k++) ate += meal(r, false) ? 1 : 0;
      std::array<uint8_t, NEEDS>& nd = r.need;
      nd[(int)Need::Hunger] = (uint8_t)std::clamp(ate == meals ? (nd[(int)Need::Hunger] + 70) / 2 : (int)nd[(int)Need::Hunger] - 3 * hours, 0, 100);
      nd[(int)Need::Rest] = (uint8_t)((nd[(int)Need::Rest] + 75) / 2);
      nd[(int)Need::Social] = (uint8_t)((nd[(int)Need::Social] + 60) / 2);
      if (r.meals < 255 - ate) r.meals = (uint8_t)(r.meals + ate);
      addCoin(r, wageOf(r.job) * hours / 3);
      if (ate) r.sinceMeal = 0;
    }
    steps = 1;
  }
  for (Resident& r : c.res) {
    if (r.flags & (RF_DEAD | RF_AWAY)) continue;
    const Plan p = plan(w, c, r, day, (float)hr + 0.5f);
    r.act = p.act; r.place = p.place; r.at = p.bldg;
    if (p.bldg >= 0 && p.bldg < (int)c.occ.size() && c.occ[(size_t)p.bldg] < 255) c.occ[(size_t)p.bldg]++;
    const bool asleep = p.act == Act::Sleep;
    std::array<uint8_t, NEEDS>& nd = r.need;
    for (int k = 0; k < steps; k++) {
      const int hh = hourAbs - (steps - 1 - k);
      // ---- decay (by trait)
      addNeed(nd[(int)Need::Hunger], asleep ? -2 : ((r.traits & TR_GLUTTON) ? -5 : -4));
      if (!asleep) addNeed(nd[(int)Need::Social], (r.traits & TR_SOCIABLE) ? -3 : -2);
      const bool quarter = ((hh + r.idx) & 3) == 0;
      addNeed(nd[(int)Need::Faith], (r.traits & TR_DEVOUT) ? -1 : (quarter || ((hh + r.idx) & 1) ? 0 : -1));
      if (r.sinceMeal < 255) r.sinceMeal++;
      // ---- what the hour did
      switch (p.act) {
        case Act::Sleep: addNeed(nd[(int)Need::Rest], p.bldg >= 0 ? 7 : 4); if (r.slept < 255) r.slept++; break;
        case Act::Eat:
          if (meal(r, p.place == Place::Gathering || p.place == Place::Gathering2)) {
            addNeed(nd[(int)Need::Hunger], p.place == Place::Gathering ? 45 : 35);
            r.sinceMeal = 0;
            if (r.meals < 255) r.meals++;
          }
          addNeed(nd[(int)Need::Rest], -1);
          if (p.place == Place::Gathering || p.place == Place::Gathering2) addNeed(nd[(int)Need::Social], 5);
          break;
        case Act::Work: case Act::Patrol: case Act::Perform: case Act::LightLamps: {
          addNeed(nd[(int)Need::Rest], (r.traits & TR_LAZY) ? -5 : -4);
          addNeed(nd[(int)Need::Social], p.act == Act::Perform ? 8 : 1);
          if (r.job == Job::Priest) addNeed(nd[(int)Need::Faith], 5);
          addCoin(r, wageOf(r.job) + (p.act == Act::Perform && (hh & 1) ? 1 : 0));
          if (p.act != Act::Work) break;
          // ---- production (15.11 chains): the workplace's recipe, or raw goods out in the fields, docks, mines
          const uint32_t d = h32(c.seed ^ r.idx, (uint64_t)hh);
          const art::Building bt = r.work >= 0 && s.bldgFirst + r.work < (int)w.over.bldgs.size()
                                       ? w.over.bldgs[(size_t)(s.bldgFirst + r.work)].type : art::Building::COUNT;
          using B = art::Building;
          using G = ew::Good;
          bool crafted = true;
          switch (bt) {
            case B::Bakery: if ((d & 1u) && take(G::Flour)) made(G::Bread, 4); break;
            case B::Windmill: case B::Watermill: if ((d & 1u) && take(G::Grain)) made(G::Flour, 2); break;
            case B::Smelter: if ((d & 1u) && take(G::Ore)) made(G::Ingot, 1); break;
            case B::Smithy: if ((d % 3u) == 0 && take(G::Ingot)) made(G::Tools, 1); break;
            case B::Sawmill: if ((d & 1u) && take(G::Logs)) made(G::Planks, 2); break;
            case B::Weaver: if ((d & 1u) && take(G::Wool)) made(G::Cloth, 1); break;
            case B::Tanner: if ((d & 1u) && take(G::Hides)) made(G::Leather, 1); break;
            case B::Butcher: if ((d % 3u) == 0 && take(G::Livestock)) { made(G::Meat, 3); made(G::Hides, 1); } break;
            default: crafted = false; break;
          }
          if (crafted) break;
          switch (r.job) {
            case Job::Farmer: if ((int)(d % 100u) < c.harvestPct) made((hh & 1) ? G::Produce : G::Grain, 1); break;
            case Job::Fisher: if (d % 100u < 85) made(G::Fish, 1); break;
            case Job::Hunter: if ((hh & 1) == 0) made(G::Meat, 1); if ((hh & 3) == 1) made(G::Hides, 1); break;
            case Job::Herder: made((hh % 3) == 0 ? G::Livestock : (hh % 3) == 1 ? G::Wool : G::Produce, 1); break;
            case Job::Miner: made(G::Ore, 1); break;
            case Job::Woodcutter: made(G::Logs, 1); break;
            default: break;
          }
          // tools wear out
          if ((d % 40u) == 7) take(G::Tools);
          break;
        }
        case Act::Tavern: case Act::Brawl: case Act::Bathe: {
          addNeed(nd[(int)Need::Social], p.act == Act::Brawl ? 5 : 14);
          addNeed(nd[(int)Need::Rest], p.act == Act::Bathe ? 4 : p.act == Act::Brawl ? -4 : -2);
          if (p.act == Act::Bathe) addNeed(nd[(int)Need::Faith], 2);
          Resident& P = r.payer >= 0 && r.payer < (int)c.res.size() ? c.res[(size_t)r.payer] : r;
          if (P.coin > 0) addCoin(P, -1);   // a drink (an ale, a cup of tea, the bath's fee)
          if (nd[(int)Need::Hunger] < 60 && meal(r, true)) {   // and a bite with it
            addNeed(nd[(int)Need::Hunger], 40);
            r.sinceMeal = 0;
            if (r.meals < 255) r.meals++;
          }
          if (p.act != Act::Brawl && k == 0) scratch_.push_back(r.idx);
          break;
        }
        case Act::Socialise: case Act::Play:
          addNeed(nd[(int)Need::Social], festival ? 16 : 11);
          addNeed(nd[(int)Need::Rest], -1);
          if (festival) addNeed(nd[(int)Need::Faith], 2);
          if (p.act == Act::Socialise && k == 0) scratch_.push_back(r.idx);
          break;
        case Act::Pray: addNeed(nd[(int)Need::Faith], 30); addNeed(nd[(int)Need::Social], 2); break;
        case Act::Beg: {
          addNeed(nd[(int)Need::Rest], -2);
          const int alms = (int)(h32(c.seed ^ r.idx, (uint64_t)hh + 0xA1D5ull) % 100u) < (c.mood >= 50 ? 45 : 20) ? 1 : 0;
          addCoin(r, alms);
          break;
        }
        case Act::Shop: {
          static const ew::Good wares[4] = {ew::Good::Cloth, ew::Good::Pottery, ew::Good::Tools, ew::Good::Leather};
          const ew::Good gd = wares[(r.idx + (uint32_t)hh) % 4u];
          if (take(gd)) addCoin(r, -std::max(1, (int)c.price[(size_t)gd] * 3 / 100));
          addNeed(nd[(int)Need::Social], 3);
          break;
        }
        case Act::Home: addNeed(nd[(int)Need::Rest], 1); addNeed(nd[(int)Need::Social], 1); break;
        default: addNeed(nd[(int)Need::Rest], -3); break;
      }
    }
    // the Money need follows the purse (dependants feel their household's)
    if (r.job == Job::Child || r.job == Job::Elder) {
      const Resident& P = r.payer >= 0 && r.payer < (int)c.res.size() && r.payer != (int16_t)r.idx ? c.res[(size_t)r.payer] : r;
      nd[(int)Need::Money] = &P == &r ? (uint8_t)60 : P.need[(int)Need::Money];
    } else nd[(int)Need::Money] = (uint8_t)std::min(100, (int)r.coin * 100 / purseOf(r.job));
    if ((r.flags & RF_GRIEVING) && day - (int)r.griefDay > 7) r.flags &= (uint16_t)~RF_GRIEVING;
    int m = 0;
    for (int k = 0; k < NEEDS; k++) m += nd[(size_t)k];
    m /= NEEDS;
    if (r.flags & RF_GRIEVING) { m -= 20; grieving++; }
    m -= townPen;
    r.mood = (uint8_t)std::clamp(m, 0, 100);
    moodSum += r.mood;
    nAlive++;
    if (nd[(int)Need::Hunger] < 30) hungry++;
    for (int k = 0; k < NEEDS; k++) stats.needSum[k] += nd[(size_t)k];
    stats.needSamples++;
  }
  // ---- ties: the people sharing a table this hour grow closer; strangers sometimes become friends (15.12)
  if (scratch_.size() >= 2) {
    std::sort(scratch_.begin(), scratch_.end(), [&](uint16_t a, uint16_t b) {
      const int pa = c.res[a].at, pb = c.res[b].at;
      return pa != pb ? pa < pb : a < b;
    });
    int formed = 0;
    for (size_t k = 0; k + 1 < scratch_.size(); k++) {
      const uint16_t a = scratch_[k], b = scratch_[k + 1];
      if (a == b || c.res[a].at != c.res[b].at || c.res[a].place != c.res[b].place) continue;
      if (Tie* t = findTie(c, a, b)) { if (t->bond < 100) t->bond++; continue; }
      if (formed >= 2 || c.ties.size() >= (size_t)c.baseTies + 400) continue;
      if (a >= c.tiesOf.size() || b >= c.tiesOf.size() || c.tiesOf[a].size() >= 10 || c.tiesOf[b].size() >= 10) continue;
      if (h32(c.seed ^ ((uint64_t)a << 16 | b), (uint64_t)hourAbs) % 25u != 0) continue;
      c.ties.push_back(Tie{a, b, 20});
      c.tiesOf[a].push_back((uint32_t)c.ties.size() - 1);
      c.tiesOf[b].push_back((uint32_t)c.ties.size() - 1);
      formed++;
    }
  }
  // ---- the settlement's mood and its flags (what the view and the townsfolk act out)
  const int avg = nAlive ? moodSum / nAlive : 60;
  const realm::SettlementState* st = g.realm.settlement(c.site);
  int mood = st ? (avg * 4 + (int)st->mood) / 5 : avg;
  c.mood = (uint8_t)std::clamp(mood, 0, 100);
  uint16_t f = 0;
  const int food = foodStock(c);
  if (nAlive && hungry * 4 > nAlive) f |= MF_HUNGRY;
  if ((c.realmFlags & realm::SS_FAMINE) || (c.famineUntil >= 0 && day <= c.famineUntil)) f |= MF_FAMINE;
  if (c.realmFlags & (realm::SS_BESIEGED | realm::SS_OCCUPIED | realm::SS_BURNED)) f |= MF_WARTORN;
  if (nAlive && (grieving * 10 >= nAlive || grieving >= 3)) f |= MF_GRIEF;
  if ((c.realmFlags & realm::SS_UNREST) || (c.mood < 38 && nAlive >= 8)) f |= MF_BRAWLS;
  if (c.emigrateDay >= 0 && day - c.emigrateDay <= 2) f |= MF_EMIGRATING;
  if (food * 3 < nAlive || (f & MF_FAMINE)) f |= MF_SHUTTERED;
  if (c.raidDay >= 0 && day - c.raidDay <= 2) f |= MF_RAIDED;
  if (festival) f |= MF_FESTIVAL;
  if (c.mood >= 55 && !(f & (MF_HUNGRY | MF_FAMINE | MF_WARTORN))) f |= MF_CONTENT;
  c.moodFlags = f;
  c.occHour = hourAbs;
  stats.hourTicks++;
}

// ---------------------------------------------------------------- the daily update (for the day that just ended)
void Life::dayTick(Game& g, Census& c, int day) {
  const World& w = g.world;
  const int nAlive = alive(c);
  // ---- use and prices
  for (int k = 0; k < (int)ew::Good::COUNT; k++) {
    c.use[(size_t)k] = (uint16_t)((c.use[(size_t)k] * 3 + c.usedToday[(size_t)k]) / 4);
    c.usedToday[(size_t)k] = 0;
  }
  // ---- the realm's view of the place, read back (15.6.3 -> 15.12)
  const realm::SettlementState* st = g.realm.settlement(c.site);
  c.realmFlags = st ? (uint8_t)(st->flags & 0xFF) : 0;
  if (c.realmFlags & realm::SS_FAMINE) c.famineUntil = std::max(c.famineUntil, day);
  const bool famine = c.famineUntil >= 0 && day <= c.famineUntil;
  const bool besieged = (c.realmFlags & realm::SS_BESIEGED) != 0;
  // the fields' yield: the kingdom's last harvest; a famine halves it; a siege keeps the farmers inside the walls
  int harvest = 100;
  const Site& s = w.sites[(size_t)c.handle];
  if (s.kingdom >= 0 && s.kingdom < (int)w.kingdoms.size())
    if (const realm::KingdomState* K = g.realm.kingdom(w.kingdoms[(size_t)s.kingdom].id))
      harvest = std::clamp((int)std::lround(K->lastHarvest * 100.0f), 30, 110);
  if (famine) harvest = std::min(harvest, 10);
  if (besieged) harvest = std::min(harvest, 20);
  c.harvestPct = (uint8_t)harvest;
  // ---- imports: the countryside's carts and the caravans fill what the place does not make (none in a famine or a
  //      siege; fewer when the realm's own granaries are low)
  const int need = dailyFood(c);
  const int realmFood = st ? (int)st->food : 60;
  int imported = 0;
  if (!famine && !besieged) {
    const int want = need * 3 / 2 - foodStock(c);
    if (want > 0) {
      // (the countryside's own carts keep coming even when the realm's granaries run low: at least a third of a day)
      imported = std::min(want, need) * std::clamp(realmFood * 100 / 70, 35, 100) / 100;
      const bool town = c.tier >= 1;
      put(c, ew::Good::Bread, imported / (town ? 3 : 2));
      put(c, ew::Good::Produce, imported - imported / (town ? 3 : 2) - (town ? imported / 3 : 0));
      if (town) put(c, ew::Good::Meat, imported / 3);
    }
    // a town's workshops buy what the countryside brings (15.11: grain, ore, logs, wool, hides) a little at a time
    static const ew::Good raw[] = {ew::Good::Grain, ew::Good::Ore, ew::Good::Logs, ew::Good::Wool, ew::Good::Hides, ew::Good::Livestock,
                                   ew::Good::Tools, ew::Good::Cloth, ew::Good::Pottery};
    for (ew::Good gd : raw)
      if (c.stock[(size_t)gd] < 4) put(c, gd, 3 + c.tier * 2);
  }
  // ---- the market: a place that does not feed itself sells what it makes over its own use (ore, logs, cloth, tools...)
  //      to the carters and buys food with it (two units for one; none in a famine, when nobody has bread to sell, or
  //      in a siege). Without it a mining or timber village with few fields starves while its ore piles up.
  if (!famine && !besieged) {
    int want = need * 3 / 2 - foodStock(c);
    const int rate = 2;
    for (int k = 0; k < (int)ew::Good::COUNT && want > 0; k++) {
      if (isFood(k)) continue;
      const int keep = 3 * std::max<int>(c.use[(size_t)k], 2) + 6;
      const int spare = (int)c.stock[(size_t)k] - keep;
      if (spare < rate) continue;
      const int buy = std::min(want, spare / rate);
      if (buy <= 0) continue;
      c.stock[(size_t)k] = (uint16_t)(c.stock[(size_t)k] - buy * rate);
      const int bread = buy / 2;
      put(c, ew::Good::Bread, bread);
      put(c, ew::Good::Produce, buy - bread);
      imported += buy;
      want -= buy;
      stats.trades++;
    }
  }
  c.foodImported += imported;
  reprice(c);
  // ---- the day reported to the realm (Realm::lifeReport): what was made and brought in against what was eaten and
  //      what was wanted and missing, in percent points of the place's food
  if (st && c.tallyDay >= 0) {
    const int bal = c.foodMade + c.foodImported - c.foodEaten - 2 * c.foodMissed;
    const int delta = std::clamp(bal * 10 / std::max(1, need), -15, 5);
    g.realm.lifeReport(c.site, delta, c.mood, day - 1);
    stats.reports++;
  }
  c.foodMade = c.foodEaten = c.foodMissed = c.foodImported = 0;
  // ---- the player's day-long hooks wear off; earners pay their rent and dues (a tenth of the purse and a little)
  for (Resident& r : c.res) {
    r.flags &= (uint16_t)~(RF_FED | RF_EMPLOYED);
    if (!(r.flags & (RF_DEAD | RF_AWAY)) && wageOf(r.job) > 0) addCoin(r, -(2 + r.coin / 8));
  }
  // ---- content days, festivals (a culture-flavoured feast every 10-20 days while the place is content)
  const bool content = (c.moodFlags & MF_CONTENT) != 0;
  c.contentDays = content ? (uint16_t)std::min(60000, c.contentDays + 1) : 0;
  if (c.festivalDay >= 0 && c.festivalDay < day) c.festivalDay = -1;
  if (c.festivalDay >= day && !content && c.festivalDay == day) c.festivalDay = -1;   // nobody feasts in a hungry town
  if (c.festivalDay < 0 && content) {
    const uint32_t hf = h32(c.seed ^ 0xFE57ull, (uint64_t)(uint32_t)(c.lastFestival + 1));
    c.festivalDay = c.lastFestival >= 0 ? std::max(day + 1, c.lastFestival + 10 + (int)(hf % 11u)) : day + 2 + (int)(hf % 9u);
  }
  // ---- misery and emigration: days below 25 in a row; three and they pack up and leave (their children with them).
  //      Key people (quest identities) never leave; a few go a day.
  int left = 0;
  const int maxLeave = std::max(1, nAlive / 40);
  for (Resident& r : c.res) {
    if (r.flags & (RF_DEAD | RF_AWAY)) continue;
    r.misery = r.mood < 25 ? (uint8_t)std::min(30, r.misery + 1) : 0;
    if (r.misery < 3 || (r.flags & RF_KEY) || r.job == Job::Child || r.job == Job::Guard || r.job == Job::Noble || left >= maxLeave) continue;
    if (g.lifeGiverOpen(c.handle, (int)r.idx)) continue;   // a quest giver waits for the player (its job is open)
    if (h32(c.seed ^ r.idx, (uint64_t)day + 0xE1Aull) % 3u != 0) continue;
    r.flags |= RF_AWAY;
    left++;
    for (Resident& k : c.res) if (k.parent == (int16_t)r.idx && k.job == Job::Child) k.flags |= RF_AWAY;
  }
  if (left) { c.emigrateDay = day; stats.emigrated += left; }
  // ---- after a good stretch, those who left come home (one a day; the carried-off wait for rescue)
  if (c.contentDays >= 5)
    for (Resident& r : c.res)
      if ((r.flags & RF_AWAY) && !(r.flags & RF_DEAD) && r.idx != c.takenIdx && !(r.flags & RF_REFUGEE)) {
        r.flags &= (uint16_t)~RF_AWAY;
        r.misery = 0;
        for (Resident& k : c.res) if (k.parent == (int16_t)r.idx) k.flags &= (uint16_t)~RF_AWAY;
        break;
      }
  // ---- refugees (M4): a camp outside the walls brings people to feed; they go home when it empties
  if ((c.realmFlags & realm::SS_REFUGEES) && c.refugees == 0) addRefugees(w, c, 3 + 2 * c.tier + (int)(c.seed % 4u));
  else if (!(c.realmFlags & realm::SS_REFUGEES) && c.refugees > 0)
    for (Resident& r : c.res) if (r.flags & RF_REFUGEE) r.flags |= RF_AWAY;
}

// light trade between loaded neighbours along the roads (not across a war, not into a siege): surpluses flow to need
void Life::tradeDay(Game& g, int day) {
  (void)day;
  const World& w = g.world;
  std::vector<Census*> loaded;
  for (int si : w.nearSites) {
    if (si < 0 || si >= (int)w.sites.size() || !w.sites[(size_t)si].settlement()) continue;
    if (Census* c = findMut(w.sites[(size_t)si].id)) if (!c->res.empty()) loaded.push_back(c);
  }
  for (size_t i = 0; i < loaded.size(); i++)
    for (size_t j = i + 1; j < loaded.size(); j++) {
      Census& A = *loaded[i];
      Census& B = *loaded[j];
      const Site& sa = w.sites[(size_t)A.handle];
      const Site& sb = w.sites[(size_t)B.handle];
      if (std::abs(sa.ex - sb.ex) + std::abs(sa.ey - sb.ey) > 360) continue;
      if ((A.realmFlags | B.realmFlags) & realm::SS_BESIEGED) continue;
      if (sa.kingdom >= 0 && sb.kingdom >= 0 && sa.kingdom != sb.kingdom && sa.kingdom < (int)w.kingdoms.size() &&
          sb.kingdom < (int)w.kingdoms.size() && g.realm.atWar(w.kingdoms[(size_t)sa.kingdom].id, w.kingdoms[(size_t)sb.kingdom].id))
        continue;
      for (int k = 0; k < (int)ew::Good::COUNT; k++) {
        Census* from = &A;
        Census* to = &B;
        auto spare = [&](const Census& c) { return (int)c.stock[(size_t)k] - 3 * std::max<int>(c.use[(size_t)k], 2); };
        auto lack = [&](const Census& c) { return 3 * std::max<int>(c.use[(size_t)k], 2) / 2 - (int)c.stock[(size_t)k]; };
        if (spare(B) > spare(A)) std::swap(from, to);
        const int n = std::min({spare(*from), lack(*to), 20});
        if (n <= 0) continue;
        from->stock[(size_t)k] = (uint16_t)(from->stock[(size_t)k] - n);
        put(*to, (ew::Good)k, n);
        stats.trades++;
      }
    }
  for (Census* c : loaded) reprice(*c);
}

void Life::evictFar(Game& g) {
  // censuses of places the window has left go back to records (memory; the save keeps them as it would)
  const World& w = g.world;
  std::vector<ew::Gid> gone;
  for (auto& kv : sites_) {
    const Census& c = kv.second;
    if (kv.first == buildGid_) continue;
    bool near = false;
    for (int si : w.nearSites) if (si == c.handle) { near = true; break; }
    if (!near) gone.push_back(kv.first);
  }
  for (ew::Gid id : gone) {
    auto it = sites_.find(id);
    if (it == sites_.end()) continue;
    pending_[id] = recordOf(it->second);
    sites_.erase(it);
  }
}

void Life::resync(int day, float hour) { lastAbsH_ = day * 24.0 + hour; }

void Life::tick(Game& g, float dt) {
  (void)dt;
  const double t0 = nowMs();
  const double abs = g.day * 24.0 + g.hour;
  if (lastAbsH_ < 0 || abs < lastAbsH_ || abs - lastAbsH_ > 24.0 * 30) lastAbsH_ = abs;
  const double dh = abs - lastAbsH_;
  lastAbsH_ = abs;
  if (dh > 0) {
    player.fedH = std::max(0.0f, player.fedH - (float)dh);
    player.restedH = std::max(0.0f, player.restedH - (float)dh);
    player.sinceMealH = std::min(1000.0f, player.sinceMealH + (float)dh);
    player.sinceSleepH = std::min(1000.0f, player.sinceSleepH + (float)dh);
  }
  if (g.world.endless && !g.travelling()) {
    World& w = g.world;
    const int32_t hourAbs = g.day * 24 + (int)g.hour;
    // ---- a new day: trade between the loaded places, the far ones put away
    if (lastDay_ != g.day) {
      if (lastDay_ >= 0 && g.day > lastDay_) { tradeDay(g, g.day); evictFar(g); }
      lastDay_ = g.day;
    }
    // ---- censuses for the settlements the window holds, built in slices (one at a time)
    bool builtNow = false;
    {
      const double bt0 = nowMs();
      if (build_.site >= 0 && (build_.site >= (int)w.sites.size() || w.sites[(size_t)build_.site].id != buildGid_)) { build_ = Build(); buildGid_ = 0; }
      if (build_.site < 0) {
        for (int si : w.nearSites) {
          if (si < 0 || si >= (int)w.sites.size()) continue;
          const Site& s = w.sites[(size_t)si];
          if (!s.settlement() || !s.id || s.bldgCount <= 0 || sites_.count(s.id)) continue;
          build_ = Build();
          build_.site = si;
          buildGid_ = s.id;
          break;
        }
      }
      if (build_.site >= 0) {
        builtNow = true;
        const bool done = buildCensusStep(w, build_, CENSUS_SLICE);
        const double ms = nowMs() - bt0;
        build_.ms += ms;
        stats.worstSliceMs = std::max(stats.worstSliceMs, ms);
        if (done) finishBuild(w);
      }
    }
    // ---- this hour's aggregate, place by place under the step's budget
    if (queueHour_ != hourAbs) {
      queue_.clear();
      for (int si : w.nearSites)
        if (si >= 0 && si < (int)w.sites.size() && w.sites[(size_t)si].settlement() && sites_.count(w.sites[(size_t)si].id)) queue_.push_back(si);
      queuePos_ = 0;
      queueHour_ = hourAbs;
    }
    // (one heavy job a step: a census being built this step leaves the hour's aggregate to the next one; 15.12 budget,
    // counted in residents, never on the clock, so the sim does not depend on the machine's speed)
    int residents = 0;
    while (!builtNow && queuePos_ < queue_.size()) {
      const int si = queue_[queuePos_++];
      auto it = sites_.find(w.sites[(size_t)si].id);
      if (it == sites_.end()) continue;
      Census& c = it->second;
      c.handle = si;
      if (c.lastHour != hourAbs) hourTick(g, c, hourAbs);
      residents += (int)c.res.size();
      if (residents >= HOUR_SLICE_RESIDENTS) break;
    }
    // a census finished this hour after the queue was made joins it
    if (queuePos_ >= queue_.size())
      for (int si : w.nearSites) {
        if (si < 0 || si >= (int)w.sites.size() || !w.sites[(size_t)si].settlement()) continue;
        auto it = sites_.find(w.sites[(size_t)si].id);
        if (it != sites_.end() && it->second.lastHour != hourAbs) { queue_.push_back(si); break; }
      }
  }
  const double ms = nowMs() - t0;
  stats.lastTickMs = ms;
  stats.worstTickMs = std::max(stats.worstTickMs, ms);
  stats.tickMsSum += ms;
  stats.ticks++;
}

// ---------------------------------------------------------------- persistence
Life::Record Life::recordOf(const Census& c) {
  Record r;
  r.site = c.site;
  r.mood = c.mood;
  r.moodFlags = c.moodFlags;
  r.lastHour = c.lastHour;
  r.festivalDay = c.festivalDay;
  r.stock = c.stock;
  r.hasNeeds = true;
  r.res.reserve(c.res.size());
  for (const Resident& x : c.res) {
    r.res.push_back(ResState{(uint16_t)(x.flags & RF_SAVED), x.griefDay, x.need, x.coin});
    if ((x.flags & RF_SAVED) || x.griefDay || x.misery) {
      r.sparse.push_back({x.idx, ResState{(uint16_t)(x.flags & RF_SAVED), x.griefDay, x.need, x.coin}});
      r.misery.push_back(x.misery);
    }
  }
  for (size_t t = 0; t < c.ties.size(); t++) {
    const Tie& T = c.ties[t];
    if (T.b == PLAYER_TIE) r.playerTies.push_back(T);
    else if (t >= c.baseTies && r.ties.size() < 400) r.ties.push_back(T);
  }
  r.lastFestival = c.lastFestival; r.famineUntil = c.famineUntil; r.raidDay = c.raidDay; r.raidNight = c.raidNight;
  r.takenIdx = c.takenIdx; r.takenQuest = c.takenQuest; r.emigrateDay = c.emigrateDay; r.refugees = c.refugees;
  r.contentDays = c.contentDays;
  return r;
}
void Life::applyRecord(Census& c, const Record& r) {
  c.mood = r.mood;
  c.moodFlags = r.moodFlags;
  c.lastHour = r.lastHour;
  c.festivalDay = r.festivalDay;
  c.stock = r.stock;
  c.lastFestival = r.lastFestival; c.famineUntil = r.famineUntil; c.raidDay = r.raidDay; c.raidNight = r.raidNight;
  c.takenIdx = r.takenIdx >= 0 && r.takenIdx < (int)c.res.size() ? r.takenIdx : (int16_t)-1;
  c.takenQuest = r.takenQuest; c.emigrateDay = r.emigrateDay;
  c.contentDays = r.contentDays;
  if (r.hasNeeds)
    for (size_t i = 0; i < r.res.size() && i < c.res.size(); i++) { c.res[i].need = r.res[i].need; c.res[i].coin = r.res[i].coin; }
  for (size_t k = 0; k < r.sparse.size(); k++) {
    const uint16_t i = r.sparse[k].first;
    if (i >= c.res.size()) continue;
    Resident& x = c.res[i];
    x.flags = (uint16_t)((x.flags & ~RF_SAVED) | (r.sparse[k].second.flags & RF_SAVED));
    x.griefDay = r.sparse[k].second.griefDay;
    x.misery = k < r.misery.size() ? r.misery[k] : 0;
  }
  c.ties.resize(std::min<size_t>(c.ties.size(), c.baseTies));
  for (const Tie& t : r.ties) if (t.a < c.res.size() && t.b < c.res.size() && t.a != t.b) c.ties.push_back(t);
  for (const Tie& t : r.playerTies) if (t.a < c.res.size()) c.ties.push_back(Tie{t.a, PLAYER_TIE, t.bond});
  tieIndex(c);
  reprice(c);
}

void Life::clear() {
  sites_.clear();
  pending_.clear();
  player = PlayerNeeds();
  lastAbsH_ = -1;
  lastDay_ = -1;
  stats = Stats();
  raid = RaidLive();
  lastRaidOutcome = 0;
  forceRaidSite = 0;
  build_ = Build();
  buildGid_ = 0;
  queue_.clear();
  queuePos_ = 0;
  queueHour_ = -1;
}

static constexpr uint8_t LIFE_BLOCK_VER = 2;
static constexpr size_t MAX_RECORDS = 48, DETAIL_RECORDS = 8;

namespace {
uint8_t q4(uint8_t v) { return (uint8_t)std::min(14, v / 7); }
uint8_t dq4(uint8_t q) { return (uint8_t)std::min(100, q * 7 + 3); }
}  // namespace

void Life::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  w.u8(LIFE_BLOCK_VER);
  w.f32(player.fedH); w.f32(player.restedH); w.f32(player.sinceMealH); w.f32(player.sinceSleepH);
  w.u8(player.mealQuality); w.u8(player.survival ? 1 : 0);
  std::vector<Record> recs;
  recs.reserve(sites_.size() + pending_.size());
  for (const auto& kv : sites_) recs.push_back(recordOf(kv.second));
  for (const auto& kv : pending_) recs.push_back(kv.second);
  // what is kept: the places that matter to the player first (friends, the dead, the carried-off), then the most
  // recently lived in; the first few keep every resident's needs
  auto important = [](const Record& r) {
    if (!r.playerTies.empty() || r.takenIdx >= 0) return 1;
    for (const auto& s : r.sparse) if (s.second.flags & (RF_DEAD | RF_BEFRIENDED)) return 1;
    return 0;
  };
  std::sort(recs.begin(), recs.end(), [&](const Record& a, const Record& b) {
    const int ia = important(a), ib = important(b);
    if (ia != ib) return ia > ib;
    if (a.lastHour != b.lastHour) return a.lastHour > b.lastHour;
    return a.site < b.site;
  });
  if (recs.size() > MAX_RECORDS) recs.resize(MAX_RECORDS);
  std::vector<uint8_t> detail(recs.size(), 0);
  {
    std::vector<size_t> byTime(recs.size());
    for (size_t i = 0; i < recs.size(); i++) byTime[i] = i;
    std::sort(byTime.begin(), byTime.end(), [&](size_t a, size_t b) {
      if (recs[a].lastHour != recs[b].lastHour) return recs[a].lastHour > recs[b].lastHour;
      return recs[a].site < recs[b].site;
    });
    for (size_t k = 0; k < byTime.size() && k < DETAIL_RECORDS; k++) detail[byTime[k]] = recs[byTime[k]].hasNeeds ? 1 : 0;
  }
  w.u32((uint32_t)recs.size());
  for (size_t ri = 0; ri < recs.size(); ri++) {
    const Record& r = recs[ri];
    w.u64(r.site); w.u8(detail[ri]); w.u8(r.mood); w.u16(r.moodFlags); w.i32(r.lastHour); w.i32(r.festivalDay);
    w.i32(r.lastFestival); w.i32(r.famineUntil); w.i32(r.raidDay); w.i32(r.raidNight); w.u16((uint16_t)r.takenIdx); w.i32(r.takenQuest);
    w.i32(r.emigrateDay); w.u16(r.refugees); w.u16(r.contentDays);
    for (uint16_t s : r.stock) w.u16(s);
    w.u16((uint16_t)std::min<size_t>(r.sparse.size(), 65535));
    for (size_t k = 0; k < r.sparse.size() && k < 65535; k++) {
      w.u16(r.sparse[k].first); w.u16(r.sparse[k].second.flags); w.u16(r.sparse[k].second.griefDay);
      w.u8(k < r.misery.size() ? r.misery[k] : 0);
    }
    if (detail[ri]) {
      w.u16((uint16_t)std::min<size_t>(r.res.size(), 65535));
      for (size_t k = 0; k < r.res.size() && k < 65535; k++) {
        const ResState& x = r.res[k];
        w.u8((uint8_t)(q4(x.need[0]) | q4(x.need[1]) << 4));
        w.u8((uint8_t)(q4(x.need[2]) | q4(x.need[3]) << 4));
        w.u8(q4(x.need[4]));
        w.u8((uint8_t)std::min<int>(x.coin, 255));
      }
      w.u16((uint16_t)std::min<size_t>(r.ties.size(), 400));
      for (size_t k = 0; k < r.ties.size() && k < 400; k++) { w.u16(r.ties[k].a); w.u16(r.ties[k].b); w.u8(r.ties[k].bond); }
    }
    w.u16((uint16_t)std::min<size_t>(r.playerTies.size(), 65535));
    for (const Tie& t : r.playerTies) { w.u16(t.a); w.u8(t.bond); }
  }
}

bool Life::deserialize(const std::vector<uint8_t>& in) {
  clear();
  BinR r(in);
  if (r.u8() != LIFE_BLOCK_VER || r.bad) return false;
  player.fedH = r.f32(); player.restedH = r.f32(); player.sinceMealH = r.f32(); player.sinceSleepH = r.f32();
  player.mealQuality = r.u8(); player.survival = r.u8() != 0;
  auto okH = [](float v) { return std::isfinite(v) && v >= 0.0f && v <= 100000.0f; };
  if (!okH(player.fedH) || !okH(player.restedH) || !okH(player.sinceMealH) || !okH(player.sinceSleepH)) return false;
  const uint32_t n = r.u32();
  auto left = [&]() { return in.size() > r.p ? in.size() - r.p : (size_t)0; };
  if (r.bad || n > MAX_RECORDS || n > left() / 60 + 1) return false;
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    Record rec;
    rec.site = r.u64();
    const uint8_t det = r.u8();
    rec.mood = r.u8(); rec.moodFlags = r.u16(); rec.lastHour = r.i32(); rec.festivalDay = r.i32();
    rec.lastFestival = r.i32(); rec.famineUntil = r.i32(); rec.raidDay = r.i32(); rec.raidNight = r.i32();
    rec.takenIdx = (int16_t)r.u16(); rec.takenQuest = r.i32(); rec.emigrateDay = r.i32(); rec.refugees = r.u16(); rec.contentDays = r.u16();
    if (det > 1 || rec.mood > 100 || rec.refugees > 200) return false;
    for (uint16_t& s : rec.stock) s = r.u16();
    const uint32_t ns = r.u16();
    if (r.bad || ns > left() / 7) return false;
    rec.sparse.resize(ns);
    rec.misery.resize(ns);
    for (uint32_t k = 0; k < ns; k++) {
      rec.sparse[k].first = r.u16();
      rec.sparse[k].second.flags = (uint16_t)(r.u16() & RF_SAVED);
      rec.sparse[k].second.griefDay = r.u16();
      rec.misery[k] = r.u8();
    }
    if (det) {
      rec.hasNeeds = true;
      const uint32_t m = r.u16();
      if (r.bad || m > left() / 4) return false;
      rec.res.resize(m);
      for (ResState& x : rec.res) {
        const uint8_t a = r.u8(), b = r.u8(), c = r.u8();
        x.need = {{dq4(a & 15), dq4(a >> 4), dq4(b & 15), dq4(b >> 4), dq4(c & 15)}};
        x.coin = r.u8();
        if ((a & 15) > 14 || (a >> 4) > 14 || (b & 15) > 14 || (b >> 4) > 14 || c > 14) return false;
      }
      const uint32_t t = r.u16();
      if (r.bad || t > 400 || t > left() / 5) return false;
      rec.ties.resize(t);
      for (Tie& T : rec.ties) { T.a = r.u16(); T.b = r.u16(); T.bond = r.u8(); if (T.b == PLAYER_TIE || T.bond > 100) return false; }
    }
    const uint32_t pt = r.u16();
    if (r.bad || pt > left() / 3) return false;
    for (uint32_t k = 0; k < pt; k++) { Tie tie; tie.a = r.u16(); tie.b = PLAYER_TIE; tie.bond = r.u8(); rec.playerTies.push_back(tie); }
    if (!rec.site || r.bad) return false;
    pending_[rec.site] = std::move(rec);
  }
  return !r.bad;
}

}  // namespace life
