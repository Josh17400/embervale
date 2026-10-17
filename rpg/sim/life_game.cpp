// M5 "Hearth and Hall": the townsfolk as actors (VISION_PLAN 10.2-10.4, 15.2, 15.12). TOWNSFOLK lane.
//
// The census (rpg/sim/life.h, CITIZENS lane) says who lives where and what each resident does this hour; this file
// puts the residents near the player on the streets and in the rooms as actors, and makes them live it:
//   - spawning by schedule: the generator's street folk give way to the census (lifeSpawnAllowed); every resident whose
//     plan puts them outdoors this hour and who is within the stream radius comes out WHERE THEIR PLAN HAS THEM (a town
//     walked into is already mid-day: someone whose block began a few minutes ago is part-way along the way there, out
//     of the door they left by), as an actor with a stable identity (slot LIFE_SLOT0 + census index: quests and stories
//     key on it). Residents whose plan is indoors are not on the street; a walker whose plan turns indoors walks to the
//     door and goes in there (never vanishing in the open street; far off screen they are simply put away);
//   - door-to-door routes: A* over the window's walkable tiles (streets cheapest, buildings never cut through), cached
//     per (from, to) pair, at most 2 path requests per step and 40 walkers thinking per step (the rest coast on their
//     route); the stuck watchdog re-plans and, failing that, picks another spot;
//   - interiors by schedule (lifeInterior): the building's key person at their post in working hours (the innkeeper
//     always: the opening's innkeeper at 8:30), the household at home, asleep in their beds at night (Posture::Sleep),
//     at the table at mealtimes, workers at their stations, patrons at the gathering place's tables in the evening with
//     the bard performing; guests upstairs only; nobody in the room the player rented;
//   - furniture: seats (chairs, stools, benches, cushions) and beds are reserved one actor each (Actor::useX / useY),
//     stations (the anvil, the oven, the altar, the lectern, the loom, the bar) give their posture;
//   - tavern life: service (the server walks to a patron, who gets a mug), chat pairs facing each other with bubbles,
//     the bard's instrument from the culture's lead (MusicStyle), toasts and dancing on festival nights, closing time;
//   - social life: greetings by name between friends, gossip near the player, grief, beggars, harmless brawls, emigrants,
//     children at tag, the lamplighter's round;
//   - village animals (dogs, cats, hens, a rooster, goats, pigs, ducks): a handful per settlement near the player;
//   - the player's hooks in dialogue (feed, coin, employ, supply, befriend, the inn's hot meal) and the 15.2 buffs.
// P1 "Fresh Roads" (LIFE lane, VISION_PLAN 15.34): nobody in the street is a statue.
//   - the street's day: a resident's outdoor block is cut into segments of 22-57 game minutes (segOf: a length and a
//     phase of their own), each a task (TaskK): a plain spot of the plan's kind somewhere new (3+ tiles off), minding a
//     stall with trips for stock (back with a crate: Carry), a carry route between real doors (the bakery / smithy /
//     market / seat / inn to customers' homes and back), sweeping along a lane, reading or preaching in the square, a
//     visit to a friend's door, a round of the stalls. The trades with no workplace (WORK@STREET) live mostly on routes;
//     the leisure hours stroll, shop and visit; field hands move along their own fields. Everyone keeps a pace of their
//     own; a stop out of reach gives the next one, twice running a spot in plain sight, a wedge a short step out;
//   - staggered arrival: a fresh street (walked into, or the clock jumped) places 42 % of the outdoor folk part-way to
//     their stop (from inside the town, in plain sight of it: walking at once), the rest at it part-way through its
//     wait; spots a body cannot reach from the town's heart (a fenced yard, a gap between houses) are never used;
//   - no giver pins: an open quest's giver, a story's person, an interior keeper with an open job live their day; the
//     tracked quest's pin follows them (Game::lifeGiverSpot: their spot by the plan, else the door they are behind);
//   - stumble-on hooks: overheard gossip naming a real need (a journal LEAD, once per giver a week, two a town a day),
//     street incidents (a thief, a broken cart, a brawl: one a town an hour at most, by the site and the hour) that
//     open an Errand when the player steps in and leave an Incident lead when ignored (life.h forceIncident etc.).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "engine/audio.h"
#include "engine/music_style.h"
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/war.h"
#include "rpg/world/economy.h"

using art::Prop;
using art::Posture;
using art::Bubble;

namespace {

// ---------------------------------------------------------------- identities and bits
constexpr int LIFE_SLOT0 = 3000;     // a resident actor's slot: LIFE_SLOT0 + its census index (stable: quests, stories)
constexpr int LIFE_SLOTN = 900;      // (3000 .. 3899: below the watch's runtime posts at 3900)
constexpr int CRITTER_SLOT0 = 3960;  // village animals (3960 .. 3991)
constexpr int CRITTER_MAX = 8;       // per settlement near the player

// Actor::lifeBits (TOWNSFOLK lane; scratch, never saved)
constexpr uint32_t LB_LIFE = 1u;       // a resident actor this lane spawned (moves by its plan)
constexpr uint32_t LB_DROP = 2u;       // a generator interior spawn lifeInterior takes away (its resident is elsewhere)
constexpr uint32_t LB_GONE = 4u;       // went in at a door / out of the map: lifeStep removes it
constexpr uint32_t LB_KEY = 8u;        // a building's key person (interior slot 0) kept at their post
constexpr uint32_t LB_GUEST = 16u;     // an inn's traveller (a generator spawn upstairs, not a resident)
constexpr uint32_t LB_SERVED = 32u;    // a patron who has had a mug brought
constexpr uint32_t LB_FRESH = 64u;     // spawned this step (its first think places it)
constexpr uint32_t LB_CRITTER = 128u;  // a village animal
constexpr uint32_t LB_SERVER = 256u;   // the inn's server on their rounds (no chat pair)
constexpr uint32_t LB_THIEF = 512u;    // (P1) a street incident's cutpurse (not a resident; incidentFolk drives them)
constexpr int THIEF_SLOT = 3996;       // (P1) the cutpurse's slot (3992 .. 3999: free of the residents, the watch, the animals)

// what the actor is doing with its target (ARt::kind)
enum TKind : uint8_t { T_NONE, T_SPOT, T_DOOR, T_EXIT, T_AWAY };

// (P1 Fresh Roads) what a resident out in the street is doing within their plan's block (ARt::task): the block is cut
// into segments of 22-57 game minutes (per person; a phase of their own, so nobody turns in lockstep), and each segment
// is one of these: a plain spot (chat, stroll, the field), minding a stall (with trips for stock), a carry route between
// real doors (source to customers and back), sweeping along a lane, reading or preaching in the square, a visit to a
// friend's door, or a round of the stalls
enum TaskK : uint8_t { TK_NONE, TK_STALL, TK_DELIVER, TK_SWEEP, TK_READ, TK_VISIT, TK_SHOP, TK_COUNT };
// ARt::mood values of the P1 street incidents (stageMood's own are 1 begging, 2 brawling)
constexpr uint8_t MOOD_INCIDENT = 3;   // held by a street incident (a victim, a cart's owner, a brawler)
constexpr uint8_t MOOD_PARTED = 4;     // (P1 fixer r2) a brawler the player just parted: backs off a step, glares, goes

const float kWalk = 0.62f;   // walking pace (share of Actor::speed)

double nowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
uint32_t mixu(uint64_t a, uint64_t b) { return (uint32_t)(ew::mix64(a * 0x9E3779B97F4A7C15ull ^ ew::mix64(b + 0x70F0Cull)) >> 16); }
int tileX(Vec2 p) { return (int)std::floor(p.x / TILE); }
int tileY(Vec2 p) { return (int)std::floor((p.y - 2) / TILE); }
Vec2 tileCentre(int x, int y) { return Vec2(x * TILE + 8.0f, y * TILE + 10.0f); }
int faceTo(int dx, int dy) { return std::abs(dx) > std::abs(dy) ? (dx > 0 ? 2 : 3) : (dy > 0 ? 0 : 1); }

bool seatProp(Prop p) { return p == Prop::Chair || p == Prop::Stool || p == Prop::Bench || p == Prop::Cushion; }
bool tableProp(Prop p) {
  return p == Prop::Table || p == Prop::TableSmall || p == Prop::TableMeal || p == Prop::TableL || p == Prop::TableM || p == Prop::TableR ||
         p == Prop::LowTable || p == Prop::TableWork || p == Prop::Desk;
}
bool bedProp(Prop p) { return p == Prop::Bed || p == Prop::BunkBed || p == Prop::Hammock || p == Prop::SleepingMat; }
bool counterProp(Prop p) { return p == Prop::Counter || p == Prop::CounterL || p == Prop::CounterM || p == Prop::CounterR; }
bool roomPublicHall(RoomKind k) {
  return k == RoomKind::Common || k == RoomKind::Feast || k == RoomKind::TeaRoom || k == RoomKind::Bath || k == RoomKind::Court ||
         k == RoomKind::Parlour;
}
bool roomHomeBed(RoomKind k) { return k == RoomKind::Bedroom || k == RoomKind::Cottage || k == RoomKind::OwnerRoom; }

const char* timeGreeting(float h) {
  if (h >= 4.5f && h < 12.0f) return "MORNING";
  if (h >= 12.0f && h < 17.5f) return "GOOD DAY";
  if (h >= 17.5f && h < 22.0f) return "EVENING";
  return "NIGHT";
}
std::string firstName(const std::string& n) {
  const size_t sp = n.find(' ');
  return sp == std::string::npos ? n : n.substr(0, sp);
}
// a night's sleep lasts until morning (game_rpg.cpp's rule: from the afternoon on you wake around 6:00-7:00)
int hoursToMorningLife(float hour) {
  if (hour >= 15.0f || hour < 6.0f) {
    float until = 6.5f - hour;
    if (until < 0) until += 24.0f;
    return std::max(8, (int)std::lround(until));
  }
  return 8;
}

// the instrument a bard plays: the culture's lead voice (the ART lane's mapping; the view picks its variant)
// (rpg_art is not linked into the simulation: these mirror art::bardPosture, art::seatedPosture, art::seatFit and
//  art::bedFit of rpg/art/art_life.h, whose numbers they must follow)
Posture bardPosture(const cult::Culture* cu) {
  if (!cu) return Posture::Lute;
  switch (cu->music.lead) {
    case LeadInst::Flute: case LeadInst::Pipes: case LeadInst::Reed: case LeadInst::Horn: case LeadInst::Brass: case LeadInst::Bells:
    case LeadInst::Voice:
      return Posture::Flute;
    case LeadInst::Marimba: return Posture::Drum;
    default: return Posture::Lute;
  }
}
Posture seatedPost(Prop seat, Posture p) {
  const bool floor = seat == Prop::Cushion || seat == Prop::SleepingMat || seat == Prop::Bedroll || seat == Prop::Rug || seat == Prop::LowTable ||
                     seat == Prop::COUNT;
  switch (p) {
    case Posture::Sit: case Posture::SitFloor: return floor ? Posture::SitFloor : Posture::Sit;
    case Posture::SitEat: case Posture::SitFloorEat: return floor ? Posture::SitFloorEat : Posture::SitEat;
    case Posture::SitDrink: case Posture::SitFloorDrink: return floor ? Posture::SitFloorDrink : Posture::SitDrink;
    default: return p;
  }
}
// where a sitter's p goes from the seat tile's bottom-centre (false: the seat is not sat on facing that way: a chair's
// back is to the north)
bool seatAt(Prop seat, int facing, int& ax, int& ay) {
  const int fc = facing == 3 ? 2 : facing;
  ax = 0;
  switch (seat) {
    case Prop::Chair: if (fc == 1) return false; ay = fc == 0 ? -2 : -3; return true;
    case Prop::Stool: case Prop::Bench: ay = fc == 0 ? -2 : (fc == 1 ? -3 : -4); return true;
    case Prop::Throne: if (fc != 0) return false; ay = -7; return true;
    // (M5 fixer r2) round a low table: the north sitter sits back from it and the south one forward on its cushion,
    // so the tabletop (teapot, cups) shows between them; keep in step with art::seatFit
    case Prop::Cushion: ay = fc == 0 ? -6 : (fc == 1 ? 1 : -4); return true;
    case Prop::SleepingMat: case Prop::Bedroll: case Prop::Rug: case Prop::COUNT: ay = -3; return true;   // (COUNT: the bare floor)
    default: ay = -2; return false;
  }
}
// a sleeper's p (the generic Sleep cell) from the bed tile's bottom-centre: the head on the pillow
int sleepAy(Prop bed, bool longBed, int kit) {
  auto vertical = [](int h, int headTop) { return headTop - 2 + 22 - h - 16; };
  auto kitBed = [](int a) { return a == 4 || a == 5 || a == 6 || a == 7 || a == 9 || a == 10 || a == 11; };
  auto kitHead = [](int a, int H) {
    const int headKind = a == 7 ? 1 : a == 10 ? 3 : a == 4 ? 4 : a == 11 ? 2 : 0;
    const int head = H >= 48 ? 6 : 2;
    return head + (headKind == 0 ? 4 : 10) + 1 - 2;   // the pillow's top, less the face's offset
  };
  switch (bed) {
    case Prop::BunkBed: return vertical(40, 18);
    case Prop::Hammock: case Prop::SleepingMat: return -6;   // (across berths: the view lays the sleeper on it)
    default: break;
  }
  const int H = longBed ? 48 : 32;
  if (kitBed(kit)) return vertical(H, kitHead(kit, H));
  return vertical(H, longBed ? 17 : 10);
}

}  // namespace

// ================================================================= runtime state
struct Game::LifeRuntime {
  const Game* owner = nullptr;   // copy on write: a copied Game (tests) gets its own runtime on first use
  Game::LifeFolkStats stats;

  // ---- outdoor spots of a settlement (global tiles), scanned when it comes near
  struct Spot { int32_t x = 0, y = 0; int8_t face = 0; };
  struct SiteRt {
    ew::Gid id = 0;
    int scannedShift = -1;
    std::vector<Spot> farm, water, wood, mine, pasture, plaza, market, street, lamps, yard, edge;
    int32_t planStamp = INT32_MIN, planHour = INT32_MIN;
    std::vector<life::Plan> plans;
    float reconcileT = 0;
    bool active = false;
    bool crittersOut = false;
    float gossipT = 0, brawlT = 0;
    int emigrants = 0;
    int emigrantDay = -1;
    int shiftDone = -1;   // the watch's last shift change shown (day * 2 + evening)
    int moodStage = -1;   // (M5 fixer r2) the in-game hour the street's mood vignettes were last staged (stageMood)
  };
  std::unordered_map<ew::Gid, SiteRt> sites;

  // ---- per actor (by Actor::id)
  struct ARt {
    uint8_t kind = T_NONE;
    int bldg = -1;                    // T_DOOR: the building (handle) it is going into
    int32_t gx = 0, gy = 0;           // the target tile (global outdoors, map-local indoors)
    std::vector<int32_t> path;        // outdoors: global tile pairs; indoors: unused (navStep)
    size_t pi = 0;
    bool want = false;                // waiting for a path request
    float stuckT = 0, lookT = 0;
    float wpBest = 1e9f;              // the nearest it has come to its current waypoint (no progress: wedged)
    size_t wpIdx = (size_t)-1;
    Vec2 lastP;
    int fails = 0;
    life::Act act = life::Act::Sleep; // the plan the target was made for
    life::Place place = life::Place::Home;
    int16_t pbldg = -1;
    bool arrived = false;
    float t = 0, t2 = 0;              // activity timers
    int partner = -1;                 // a chat partner / the patron a server walks to (actor id)
    int lampIdx = -1;
    bool dropIn = false;              // (fixer M5 r3) an evening's drop-in at the inn (populate): stays till late
    float watchT = 0;                 // the stuck watchdog (30 s windows while walking)
    Vec2 watchP;
    bool stuck = false;
    int seatX = -1, seatY = -1;
    float greetT = 0;                 // the next time it may greet a friend (Game::time)
    int serving = -1, servePhase = 0, barX = -1, barY = -1;   // a server's round (serve)
    // indoors: the place decided for them (walked to, then taken: its exact position, posture and facing); then:
    // 0 take it, 1 vanish there (up the stairs to bed)
    Vec2 dest;
    Posture post = Posture::None;
    int destFace = 0, then = 0;
    bool claimed = false;             // holds the outdoor spot (cx, cy) (global tile)
    bool pairStep = false;            // (P1 fixer r2) a chat pair's step into place beside the partner (dest)
    float pairT = 0;
    int32_t cx = 0, cy = 0;
    uint8_t mood = 0;                 // (M5 fixer r2) a staged mood vignette at its spot: 0 none, 1 begging, 2 brawling
    // ---- P1 Fresh Roads: the street's day (TaskK)
    int32_t seg = INT32_MIN;          // the segment the current task belongs to (segOf)
    uint8_t task = TK_NONE, leg = 0;  // the task and the leg of its route (a stop at a door, a stall, a spot)
    float dwell = 0;                  // seconds left at this leg's stop
    Posture walkPost = Posture::None; // the posture on the way (Carry: a sack, a basket, a crate)
    int srcB = -1;                    // TK_DELIVER: the building (handle) the goods come from; TK_STALL / TK_SWEEP: -1
    int32_t ax = 0, ay = 0;           // the task's anchor (global tile): the stall, the lane swept, the friend's door
    float pace = 1.0f;                // this person's own pace (0.86 .. 1.14 of the town's walking pace)
    float hawkT = 0;                  // TK_STALL: the next cry of the wares
    uint8_t retry = 0;                // spots out of reach in a row this segment (2: one in plain sight instead)
    bool slip = false;                // wedged between walls: a short step out to slipTo, through the snag (never a jump)
    bool slipped = false;             // the step out was tried this block (a second wedge: they keep to their spot: retry 255)
    float slipT = 0;
    Vec2 slipTo;
  };
  std::unordered_map<int, ARt> act;
  ARt& rt(int id) { return act[id]; }

  // ---- furniture reservations (mapKey << 32 | tile)
  std::unordered_set<uint64_t> used;
  std::unordered_set<uint64_t> greeted;   // friends who have greeted each other today
  std::unordered_map<uint64_t, int> spotOwner;   // outdoor spots taken (global tile -> actor id)

  // ---- interior of the current map
  struct Furn { int x, y; Prop p; int room; int8_t face; };
  struct Interior {
    int key = -1;
    std::vector<Furn> seats, beds, stations;
    std::vector<int> floorTiles;      // free floor tiles (y * w + x)
    std::vector<int> poolTiles;       // (M5 fixer r2) a bathhouse's pool water (y * w + x): bathers stand in it
    float tickT = 0;
    int32_t stamp = INT32_MIN;
  } in;
  // beds per (building id, floor): how many residents each floor of a home sleeps (cached; consistent assignment)
  std::unordered_map<uint64_t, int> bedsOnFloor;

  // ---- paths
  std::unordered_map<uint64_t, std::vector<int32_t>> cache;
  std::vector<uint64_t> cacheOrder;
  int pathBudget = 2;
  int thinkBudget = 40;
  std::vector<int> gScore, came;
  std::vector<uint8_t> closed;

  // ---- clock
  double lastAbsH = -1;
  uint8_t lastBuffs = 0;
  float sleepQ = 10.0f;               // hours of Rested the next night gives (the bed's quality)
  double stepT0 = 0;
  double folkMs = 0;                  // lifeFolk time this step
  float barkT = 0;

  // ---- P1 Fresh Roads: stumble-on hooks (never saved: an errand left running is failed when its moment has passed)
  struct Incident {
    int kind = 0;                     // 0 none, 1 a thief, 2 a broken cart, 3 a brawl (life::INC_*)
    int si = -1;
    ew::Gid site = 0;
    int victim = -1, other = -1;      // actor ids: who asks (the robbed, the cart's owner, a brawler) / the thief, the second brawler
    int quest = 0;                    // the errand's id once the player took it on
    float t = 0, holdT = 0, sayT = 0; // seconds since it began / the player's help at the cart / the next word
    int32_t gx = 0, gy = 0;           // where (global tile): the cart, the brawl, the thief now
    int32_t ex = 0, ey = 0;           // the thief's way out (global tile)
    bool engaged = false;
    // the victim's identity (an errand can be opened, a lead written, after their body is put away)
    int vSite = -1, vBldg = -1, vSlot = -1;
    uint64_t vKey = 0;
    std::string vName;
  } inc;
  // (P1 fixer r2) a line said a beat after another (the robbed's thanks after the thief gives up): one plate at a time
  struct LaterSay { int actor = -1; float t = 0; std::string s; uint32_t col = 0; };
  std::vector<LaterSay> sayLater;
  std::unordered_set<uint64_t> incidentSlots;   // (site, hour) slots already decided (one incident an hour at most)
  float incidentCheckT = 0, orphanT = 0;
  life::StreetStats street;
};

// ================================================================= the lane's internals (friend of Game)
struct LifeOps {
  using RT = Game::LifeRuntime;

  static RT& rt(Game& g) {
    if (!g.lifeRt_) g.lifeRt_ = std::make_shared<RT>();
    if (g.lifeRt_->owner != &g) {   // a copy of a Game: its own runtime from here on
      if (g.lifeRt_->owner) g.lifeRt_ = std::make_shared<RT>(*g.lifeRt_);
      g.lifeRt_->owner = &g;
    }
    return *g.lifeRt_;
  }

  static life::Census* censusOfSite(Game& g, int si) {
    if (si < 0 || si >= (int)g.world.sites.size() || !g.world.sites[(size_t)si].settlement()) return nullptr;
    return g.life.census(g.world, si);
  }
  static int siteOfActor(const Game& g, const Actor& a) {
    if (a.site >= 0) return a.site;
    if (a.bldg >= 0 && a.bldg < (int)g.world.over.bldgs.size()) return g.world.over.bldgs[(size_t)a.bldg].site;
    return -1;
  }
  static uint64_t useKey(const Game& g, int x, int y) { return ((uint64_t)(uint32_t)g.mapKey() << 32) | (uint32_t)(y * 4096 + x); }
  static void release(Game& g, Actor& a) {
    if (a.useX >= 0) rt(g).used.erase(useKey(g, a.useX, a.useY));
    a.useX = a.useY = -1;
  }
  static bool reserve(Game& g, Actor& a, int x, int y) {
    RT& R = rt(g);
    const uint64_t k = useKey(g, x, y);
    if (R.used.count(k)) return false;
    release(g, a);
    R.used.insert(k);
    a.useX = x; a.useY = y;
    return true;
  }
  static bool isUsed(Game& g, int x, int y) { return rt(g).used.count(useKey(g, x, y)) > 0; }

  static void bubble(Actor& a, Bubble b, float secs) { a.bubble = b; a.bubbleT = secs; }
  static void say(Game& g, const Actor& a, const std::string& s, uint32_t col = 0) {
    // (P1 fixer) the plate's foot clears the speaker's head (a 24 px body): the words never cover the face that says them
    g.emit(Ev::Text, a.p + Vec2(0, -27), (int)(col ? col : rgba(240, 232, 200)), 0, s);
    // (P1 fixer r2) the speaker rides with the line (Event::id = actor id + 1): the view keeps the plate over their head
    // as they walk (a brawler squaring up, a thief caught on the run) and a new line from them replaces it. A plate left
    // where the words were first said covered the next person there and outlived the moment
    g.events.back().id = (uint32_t)a.id + 1u;
  }
  // (P1 fixer r2) the speaker's plate, if one is up, taken down (an empty line with its speaker: render.cpp Ev::Text)
  static void hush(Game& g, const Actor& a) {
    g.emit(Ev::Text, a.p, 0, 0, "");
    g.events.back().id = (uint32_t)a.id + 1u;
  }

  // ---------------------------------------------------------------- plans
  static double absHour(const Game& g) { return g.day * 24.0 + g.hour; }
  static life::Plan planAt(Game& g, const life::Census& c, const life::Resident& r, double abs) {
    const int d = (int)std::floor(abs / 24.0);
    float h = (float)(abs - d * 24.0);
    return g.life.plan(g.world, c, r, d, h);
  }
  static bool samePlace(const life::Plan& a, const life::Plan& b) { return a.place == b.place && a.bldg == b.bldg && a.act == b.act; }
  // the plans of every resident of a site, refreshed every 3 in-game minutes
  static void refreshPlans(Game& g, RT::SiteRt& S, const life::Census& c) {
    const int32_t stamp = (int32_t)std::floor(absHour(g) * 20.0);
    // (the needs the plans read move once an hour: a census's first aggregate hour changes them too)
    if (S.planStamp == stamp && S.plans.size() == c.res.size() && S.planHour == c.lastHour) return;
    S.planStamp = stamp;
    S.planHour = c.lastHour;
    S.plans.resize(c.res.size());
    for (size_t i = 0; i < c.res.size(); i++) S.plans[i] = g.life.plan(g.world, c, c.res[i], g.day, g.hour);
  }
  // when the resident's current block began (hours ago, up to 1) and where they were before it
  static float blockAge(Game& g, const life::Census& c, const life::Resident& r, const life::Plan& now, life::Plan& before) {
    const double abs = absHour(g);
    for (int k = 1; k <= 12; k++) {
      const double t = abs - k * (1.0 / 12.0);
      before = planAt(g, c, r, t);
      if (!samePlace(before, now)) return (float)(k - 0.5) / 12.0f;
    }
    before = now;
    return 1.0f;
  }

  // ---------------------------------------------------------------- outdoor spots
  static RT::SiteRt& siteRt(Game& g, int si) {
    RT& R = rt(g);
    RT::SiteRt& S = R.sites[g.world.sites[(size_t)si].id];
    S.id = g.world.sites[(size_t)si].id;
    if (S.scannedShift != g.world.windowShifts) scanSpots(g, si, S);
    return S;
  }
  static void scanSpots(Game& g, int si, RT::SiteRt& S) {
    S.scannedShift = g.world.windowShifts;
    for (auto* v : {&S.farm, &S.water, &S.wood, &S.mine, &S.pasture, &S.plaza, &S.market, &S.street, &S.lamps, &S.yard, &S.edge}) v->clear();
    const Site& st = g.world.sites[(size_t)si];
    const Map& m = g.world.over;
    const int pad = 14;
    const int x0 = std::max(1, st.r.x - pad), y0 = std::max(1, st.r.y - pad);
    const int x1 = std::min(m.w - 2, st.r.x + st.r.w + pad), y1 = std::min(m.h - 2, st.r.y + st.r.h + pad);
    auto walk = [&](int x, int y) { return m.in(x, y) && !m.blocked(x, y) && m.bldgAt[(size_t)y * m.w + x] < 0 && !m.wall[(size_t)y * m.w + x]; };
    auto propIs = [&](int x, int y, std::initializer_list<Prop> ps) {
      const int p = m.propAt(x, y);
      if (!p) return false;
      for (Prop q : ps) if ((int)q + 1 == p) return true;
      return false;
    };
    static const int dx[4] = {0, 0, 1, -1}, dy[4] = {1, -1, 0, 0};
    auto put = [&](std::vector<RT::Spot>& v, int x, int y, int face) { v.push_back({g.world.ox + x, g.world.oy + y, (int8_t)face}); };
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        if (!walk(x, y)) continue;
        const Ground gr = m.at(x, y);
        const bool inSite = st.r.contains(x, y);
        if (gr == Ground::Farmland) put(S.farm, x, y, 0);
        if (inSite && gr == Ground::Plaza) put(S.plaza, x, y, 0);
        if (inSite && gr == Ground::Road && ((x + y) % 2 == 0)) put(S.street, x, y, 0);
        for (int k = 0; k < 4; k++) {
          const int nx = x + dx[k], ny = y + dy[k];
          if (!m.in(nx, ny)) continue;
          const Ground ng = m.at(nx, ny);
          const int face = k == 0 ? 0 : k == 1 ? 1 : k == 2 ? 2 : 3;
          if (groundWater(ng) && gr != Ground::Bridge && m.propAt(nx, ny) == 0) put(S.water, x, y, face);
          if (propIs(nx, ny, {Prop::Woodpile, Prop::LogPile, Prop::Stump})) put(S.wood, x, y, face);
          if (propIs(nx, ny, {Prop::OrePile, Prop::OreCart, Prop::MineEntrance, Prop::MineHill})) put(S.mine, x, y, face);
          if (propIs(nx, ny, {Prop::Sheep, Prop::Cow, Prop::PenShelter, Prop::Trough, Prop::Haystack})) put(S.pasture, x, y, face);
          if (k == 1 && m.propAt(nx, ny) && art::isVendorProp((Prop)(m.propAt(nx, ny) - 1))) put(S.market, x, y, 1);
          if (k == 1 && propIs(nx, ny, {Prop::Lamppost})) put(S.lamps, x, y, 1);
        }
        if (!inSite && (x == st.r.x - 2 || x == st.r.x + st.r.w + 1 || y == st.r.y - 2 || y == st.r.y + st.r.h + 1) && (gr == Ground::Road || gr == Ground::Bridge))
          put(S.edge, x, y, 0);
      }
    // yards: open ground beside farmhouses, houses and huts (hens, goats, a dog by the door)
    for (int b = st.bldgFirst; b < st.bldgFirst + st.bldgCount && b < (int)m.bldgs.size(); b++) {
      const Bldg& B = m.bldgs[(size_t)b];
      if (B.site != si) continue;
      const bool farmy = B.type == art::Building::Farmhouse || B.type == art::Building::House || B.type == art::Building::Hut || B.type == art::Building::Granary;
      if (!farmy) continue;
      for (int y = B.r.y - 1; y <= B.r.y + B.r.h + 1; y++)
        for (int x = B.r.x - 2; x <= B.r.x + B.r.w + 1; x++) {
          if (!walk(x, y)) continue;
          const Ground gr = m.at(x, y);
          if (gr == Ground::Road || gr == Ground::Plaza) continue;
          if (x == B.doorX() && y == B.doorY() + 1) continue;
          put(S.yard, x, y, 0);
        }
    }
    // no plaza (a village green): the open tiles round the heart
    if (S.plaza.size() < 6)
      for (int y = st.ey - 4; y <= st.ey + 4; y++)
        for (int x = st.ex - 6; x <= st.ex + 6; x++)
          if (walk(x, y)) put(S.plaza, x, y, 0);
    if (S.street.size() < 6)
      for (int y = st.r.y; y < st.r.y + st.r.h; y++)
        for (int x = st.r.x; x < st.r.x + st.r.w; x++)
          if (walk(x, y) && (m.at(x, y) == Ground::Road || m.at(x, y) == Ground::Dirt) && ((x * 7 + y * 3) % 5 == 0)) put(S.street, x, y, 0);
    // (P1) only spots a body can walk to from the town: a woodpile in a yard fenced all round, a gap shut in by houses,
    // are left out (someone placed there could never leave it). A flood over the scanned area from the heart (or, when
    // the heart stands on something solid, from the nearest street spot)
    {
      const int W = x1 - x0 + 1, H = y1 - y0 + 1;
      if (W > 0 && H > 0) {
        std::vector<uint8_t> seen((size_t)W * H, 0);
        std::vector<int> q;
        q.reserve(4096);
        auto push = [&](int x, int y) {
          if (x < x0 || y < y0 || x > x1 || y > y1 || !walk(x, y)) return;
          uint8_t& s = seen[(size_t)(y - y0) * W + (x - x0)];
          if (s) return;
          s = 1;
          q.push_back((y - y0) * W + (x - x0));
        };
        push(st.ex, st.ey);
        if (q.empty())
          for (const RT::Spot& s : S.street) { push(s.x - g.world.ox, s.y - g.world.oy); if (!q.empty()) break; }
        for (size_t i = 0; i < q.size(); i++) {
          const int x = q[i] % W + x0, y = q[i] / W + y0;
          push(x + 1, y); push(x - 1, y); push(x, y + 1); push(x, y - 1);
        }
        // (a town whose heart reaches only a sliver of it is left as scanned: better a stuck few than an empty street)
        if (q.size() * 4 >= (size_t)W * H / 8)
          for (auto* v : {&S.farm, &S.water, &S.wood, &S.mine, &S.pasture, &S.plaza, &S.market, &S.street, &S.lamps, &S.yard, &S.edge})
            v->erase(std::remove_if(v->begin(), v->end(), [&](const RT::Spot& s) {
                       const int x = s.x - g.world.ox, y = s.y - g.world.oy;
                       return x < x0 || y < y0 || x > x1 || y > y1 || !seen[(size_t)(y - y0) * W + (x - x0)];
                     }),
                     v->end());
      }
    }
  }
  static bool spotOk(const Game& g, const RT::Spot& s) {
    const int x = s.x - g.world.ox, y = s.y - g.world.oy;
    const Map& m = g.world.over;
    if (!m.in(x, y) || m.blocked(x, y) || m.bldgAt[(size_t)y * m.w + x] >= 0) return false;
    // (M7c integration) never on the doorstep of a burned-out shell: someone standing there reads as having come out
    // of it (its door is barred, nobody comes or goes)
    if (m.in(x, y - 1)) {
      const int b = m.bldgAt[(size_t)(y - 1) * m.w + x];
      if (b >= 0 && b < (int)m.bldgs.size() && m.bldgs[(size_t)b].charred == 2 && m.bldgs[(size_t)b].doorX() == x) return false;
    }
    return true;
  }
  // a spot of the list by hash (free of anyone else's claim: two people never stand on one spot)
  static const RT::Spot* pick(Game& g, const std::vector<RT::Spot>& v, uint32_t h, int self = -1) {
    if (v.empty()) return nullptr;
    for (int k = 0; k < 10; k++) {
      const RT::Spot& s = v[(h + (uint32_t)k * 2654435761u) % v.size()];
      if (spotOk(g, s) && !spotTaken(g, s.x, s.y, self)) return &s;
    }
    return nullptr;
  }
  static uint64_t spotKey(int32_t x, int32_t y) { return ((uint64_t)(uint32_t)x << 32) | (uint32_t)y; }
  static bool spotTaken(Game& g, int32_t x, int32_t y, int self) {
    RT& R = rt(g);
    auto it = R.spotOwner.find(spotKey(x, y));
    return it != R.spotOwner.end() && it->second != self;
  }
  static void releaseSpot(Game& g, RT::ARt& A, int self) {
    if (!A.claimed) return;
    RT& R = rt(g);
    auto it = R.spotOwner.find(spotKey(A.cx, A.cy));
    if (it != R.spotOwner.end() && it->second == self) R.spotOwner.erase(it);
    A.claimed = false;
  }
  static void claimSpot(Game& g, RT::ARt& A, int self, int32_t x, int32_t y) {
    releaseSpot(g, A, self);
    rt(g).spotOwner[spotKey(x, y)] = self;
    A.claimed = true; A.cx = x; A.cy = y;
  }
  // the door step (overworld tile in front of a building's door), local tiles
  static bool doorStep(const Game& g, int b, int& x, int& y) {
    if (b < 0 || b >= (int)g.world.over.bldgs.size()) return false;
    const Bldg& B = g.world.over.bldgs[(size_t)b];
    if (B.charred == 2) return false;   // (M4) a burned-out shell: its door is barred, nobody comes or goes
    x = B.doorX(); y = B.doorY() + 1;
    return g.world.over.in(x, y) && !g.world.over.blocked(x, y);
  }

  // where on the street a resident's plan puts them now (false: indoors / away / nowhere outdoors). out: global tile
  // and facing; post: the posture once there
  // (P1 Fresh Roads: no more plaza pin for an open quest's giver or a story's person: they live their day like
  //  everyone else, and the tracked quest's pin follows them, Game::lifeGiverSpot)
  static bool targetSpot(Game& g, int si, RT::SiteRt& S, const life::Census& c, const life::Resident& r, const life::Plan& p,
                         int32_t& gx, int32_t& gy, int& face, int self = -1) {
    return outdoorSpot(g, si, S, c, r, p, gx, gy, face, self);
  }
  static bool fieldJob(life::Job j) {
    return j == life::Job::Farmer || j == life::Job::Herder || j == life::Job::Woodcutter || j == life::Job::Fisher || j == life::Job::Miner;
  }
  static bool fieldLunch(const Game& g, const life::Resident& r, const life::Plan& p) {
    return p.act == life::Act::Eat && p.place == life::Place::Home && fieldJob(r.job) && g.hour >= 11.0f && g.hour < 14.0f;
  }
  // salt: (P1) the segment of the block (segOf + 1; 0 the block's first spot): a fresh spot of the same kind each segment
  static bool outdoorSpot(Game& g, int si, RT::SiteRt& S, const life::Census& c, const life::Resident& r, const life::Plan& p0,
                          int32_t& gx, int32_t& gy, int& face, int self = -1, uint32_t salt = 0) {
    using life::Act;
    using life::Place;
    using life::Job;
    if (p0.place == Place::Away || (r.flags & (life::RF_DEAD | life::RF_AWAY))) return false;
    // a field hand's midday meal is eaten out at the work (bread and cheese at the field's edge), not walked home for
    life::Plan p = p0;
    if (fieldLunch(g, r, p0)) { p.act = Act::Work; p.place = Place::Field; p.bldg = -1; }
    if (p.bldg >= 0 && p.act != Act::Emigrate) return false;
    const uint32_t h = mixu(c.seed ^ (uint64_t)r.idx * 7919u, (uint64_t)g.day * 31u + (uint64_t)p.act * 977u + (uint64_t)p.place + (uint64_t)salt * 0x9E3779B1ull);
    const std::vector<RT::Spot>* v = &S.street;
    switch (p.place) {
      case Place::Field:
        switch (r.job) {
          case Job::Fisher: v = &S.water; break;
          case Job::Woodcutter: v = &S.wood; break;
          case Job::Miner: v = &S.mine; break;
          case Job::Herder: v = &S.pasture; break;
          default: v = &S.farm; break;
        }
        if (v->empty()) v = !S.farm.empty() ? &S.farm : !S.pasture.empty() ? &S.pasture : &S.yard;
        if (r.job == Job::Hunter) v = !S.edge.empty() ? &S.edge : &S.street;
        break;
      case Place::Plaza: case Place::Gathering: case Place::Gathering2: case Place::Temple: v = &S.plaza; break;
      case Place::Market: v = !S.market.empty() ? &S.market : &S.plaza; break;
      case Place::Gate: v = !S.edge.empty() ? &S.edge : &S.street; break;
      case Place::Home: case Place::Work: case Place::Street: default: v = &S.street; break;
    }
    if (p.act == Act::Emigrate) v = !S.edge.empty() ? &S.edge : &S.street;
    if (p.act == Act::LightLamps && !S.lamps.empty()) v = &S.lamps;
    if (v->empty()) v = &S.plaza;
    // a home's own street: near the home door when the place is the street (labourers, homeless sleepers)
    const RT::Spot* s = nullptr;
    if ((p.place == Place::Street || p.place == Place::Home) && r.home >= 0 && !v->empty()) {
      int hx, hy;
      const life::Census& cc = c;
      const int hb = cc.bldgHandle(g.world.sites[(size_t)si], r.home);
      if (doorStep(g, hb, hx, hy)) {
        // the nearest of a few hashed candidates
        int best = 1 << 30;
        for (int k = 0; k < 8; k++) {
          const RT::Spot& q = (*v)[(h + (uint32_t)k * 40503u) % v->size()];
          if (!spotOk(g, q) || spotTaken(g, q.x, q.y, self)) continue;
          const int d = std::abs(q.x - (g.world.ox + hx)) + std::abs(q.y - (g.world.oy + hy));
          if (d < best) { best = d; s = &q; }
        }
      }
    }
    // out at the fields' and the woods' edge: never shoulder to shoulder with the next worker (a row of them on
    // neighbouring spots reads as a drilled line): the first hashed spot with nobody on the tiles round it
    // (the hunters, whose edge of town may be one short stretch of road, try the woods and the pastures next)
    const std::vector<RT::Spot>* lists[4] = {v, nullptr, nullptr, nullptr};
    if (p.place == Place::Field && r.job == Job::Hunter) { lists[1] = &S.wood; lists[2] = &S.pasture; lists[3] = &S.farm; }
    const bool spaced = p.place != Place::Market && p.act != Act::LightLamps && p.act != Act::Perform;
    for (const std::vector<RT::Spot>* vv : lists) {
      if (s || !spaced) break;
      if (!vv || vv->size() <= 2) continue;
      for (int k = 0; k < 12 && !s; k++) {
        const RT::Spot& q = (*vv)[(h + (uint32_t)k * 2654435761u) % vv->size()];
        if (!spotOk(g, q) || spotTaken(g, q.x, q.y, self)) continue;
        bool crowd = false;
        for (int oy = -1; oy <= 1 && !crowd; oy++)
          for (int ox = -2; ox <= 2 && !crowd; ox++)
            if ((ox || oy) && spotTaken(g, q.x + ox, q.y + oy, self)) crowd = true;
        if (!crowd) s = &q;
      }
    }
    if (!s) s = pick(g, *v, h, self);
    if (!s) s = pick(g, S.plaza, h, self);
    if (!s) return false;
    gx = s->x; gy = s->y; face = s->face;
    return true;
  }

  // ---------------------------------------------------------------- paths (outdoors)
  static int stepCost(const Map& m, int x, int y) {
    switch (m.at(x, y)) {
      case Ground::Road: case Ground::Plaza: case Ground::Bridge: return 10;
      case Ground::Dirt: return 12;
      case Ground::Farmland: return 17;
      default: return 14;
    }
  }
  // (P1 fixer r2) a tile out in the open, where a body is seen whole: not behind a stall's counter or under its awning
  // (a vendor's prop, a cart or their filler within a tile across, one above or two below), nor in the strip a roof's
  // eave hides north of a building (its footprint one or two tiles below)
  static bool openTile(const Map& m, int x, int y) {
    for (int dy = -1; dy <= 2; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        const int tx = x + dx, ty = y + dy;
        if (!m.in(tx, ty)) continue;
        const int q = m.propAt(tx, ty);
        if (q && (art::isVendorProp((Prop)(q - 1)) || q == (int)Prop::Cart + 1 || q == (int)Prop::Filler + 1)) return false;
        if (dx == 0 && dy >= 1 && m.bldgAt[(size_t)ty * m.w + tx] >= 0) return false;
      }
    return true;
  }
  // inView: (P1 fixer r2: the cutpurse's run) the open street costs as usual and a hidden tile (openTile) five times
  // over, so the chase stays where it is seen and the catch never happens behind a stall
  static bool findPath(Game& g, int sx, int sy, int tx, int ty, std::vector<int32_t>& out, bool inView = false) {
    RT& R = rt(g);
    const Map& m = g.world.over;
    out.clear();
    if (!m.in(sx, sy) || !m.in(tx, ty)) return false;
    const uint64_t key = ew::mix64(((uint64_t)(uint32_t)(g.world.ox + sx) << 32 | (uint32_t)(g.world.oy + sy)) * 31ull ^
                                   ((uint64_t)(uint32_t)(g.world.ox + tx) << 32 | (uint32_t)(g.world.oy + ty))) ^ (inView ? 0x0BE11ull : 0ull);
    auto it = R.cache.find(key);
    if (it != R.cache.end()) { out = it->second; R.stats.pathCacheHits++; return !out.empty(); }
    R.stats.pathRequests++;
    const int pad = 18;
    const int x0 = std::max(0, std::min(sx, tx) - pad), y0 = std::max(0, std::min(sy, ty) - pad);
    const int x1 = std::min(m.w - 1, std::max(sx, tx) + pad), y1 = std::min(m.h - 1, std::max(sy, ty) + pad);
    const int W = x1 - x0 + 1, H = y1 - y0 + 1;
    if (W > 200 || H > 200) { R.stats.pathFails++; return false; }
    const size_t N = (size_t)W * H;
    R.gScore.assign(N, INT32_MAX);
    R.came.assign(N, -1);
    R.closed.assign(N, 0);
    auto L = [&](int x, int y) { return (y - y0) * W + (x - x0); };
    auto pass = [&](int x, int y) {
      if (x == tx && y == ty) return true;
      const size_t i = (size_t)y * m.w + x;
      return !m.blocked(x, y) && m.bldgAt[i] < 0;
    };
    struct Node { int f, i; };
    std::vector<Node> heap;
    heap.reserve(512);
    auto push = [&](int f, int i) { heap.push_back({f, i}); std::push_heap(heap.begin(), heap.end(), [](const Node& a, const Node& b) { return a.f > b.f; }); };
    const int s = L(sx, sy), goal = L(tx, ty);
    R.gScore[(size_t)s] = 0;
    push(0, s);
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    bool found = false;
    int expanded = 0;
    while (!heap.empty()) {
      std::pop_heap(heap.begin(), heap.end(), [](const Node& a, const Node& b) { return a.f > b.f; });
      const Node n = heap.back();
      heap.pop_back();
      if (R.closed[(size_t)n.i]) continue;
      R.closed[(size_t)n.i] = 1;
      if (n.i == goal) { found = true; break; }
      if (++expanded > 24000) break;
      const int cx = n.i % W + x0, cy = n.i / W + y0;
      for (int k = 0; k < 4; k++) {
        const int nx = cx + dx[k], ny = cy + dy[k];
        if (nx < x0 || ny < y0 || nx > x1 || ny > y1 || !pass(nx, ny)) continue;
        const int li = L(nx, ny);
        if (R.closed[(size_t)li]) continue;
        const int gs = R.gScore[(size_t)n.i] + stepCost(m, nx, ny) * (inView && !openTile(m, nx, ny) ? 5 : 1);
        if (gs >= R.gScore[(size_t)li]) continue;
        R.gScore[(size_t)li] = gs;
        R.came[(size_t)li] = n.i;
        push(gs + 10 * (std::abs(nx - tx) + std::abs(ny - ty)), li);
      }
    }
    if (!found) { R.stats.pathFails++; remember(R, key, out); return false; }
    std::vector<int> rev;
    for (int i = goal; i != s && i >= 0; i = R.came[(size_t)i]) rev.push_back(i);
    for (size_t k = rev.size(); k-- > 0;) {
      out.push_back(g.world.ox + rev[k] % W + x0);
      out.push_back(g.world.oy + rev[k] / W + y0);
    }
    remember(R, key, out);
    return true;
  }
  static void remember(RT& R, uint64_t key, const std::vector<int32_t>& p) {
    if (R.cache.size() >= 192) {
      for (int k = 0; k < 32 && !R.cacheOrder.empty(); k++) { R.cache.erase(R.cacheOrder.front()); R.cacheOrder.erase(R.cacheOrder.begin()); }
    }
    R.cache[key] = p;
    R.cacheOrder.push_back(key);
  }

  // is the straight way between two points clear for a body (the walker cuts corners on open ground)
  static bool clearLine(Game& g, Vec2 a, Vec2 b, float r) {
    const Vec2 d = b - a;
    const float l = len(d);
    for (float t = 5.0f; t < l; t += 5.0f)
      if (!g.bodyFree(a + d * (t / l), r, false)) return false;
    return true;
  }

  // walk the actor's route outdoors (requesting it when needed). true: arrived at the target tile
  static bool walkOut(Game& g, Actor& a, RT::ARt& A, float speed, float dt) {
    RT& R = rt(g);
    const Map& m = g.world.over;
    const int lx = A.gx - g.world.ox, ly = A.gy - g.world.oy;
    const Vec2 dest = tileCentre(lx, ly);
    if (len2(dest - a.p) < 3.0f * 3.0f) return true;
    if (A.path.empty() || A.want) {
      // in plain sight: walk straight
      if (len2(dest - a.p) < (8.0f * TILE) * (8.0f * TILE) && clearLine(g, a.p, dest, a.radius)) {
        const Vec2 before = a.p;
        stepToward(g, a, dest, speed, dt);
        A.want = false;
        // (a spot a body cannot quite reach, a pillar's corner, a fence post: close enough counts as there)
        if (len2(a.p - before) < 0.0004f) {
          A.stuckT += dt;
          if (A.stuckT > 1.2f) { A.stuckT = 0; return true; }
        } else A.stuckT = std::max(0.0f, A.stuckT - dt);
        return false;
      }
      if (R.pathBudget <= 0) { a.st = AState::Idle; return false; }
      R.pathBudget--;
      A.want = false;
      std::vector<int32_t> p;
      if (!findPath(g, tileX(a.p), tileY(a.p), lx, ly, p)) {
        A.fails++;
        R.street.legFails[std::min<int>(A.task, 7)]++;
        a.st = AState::Idle;
        return false;
      }
      A.path = std::move(p);
      A.pi = 0;
    }
    // look ahead along the route: cut to the farthest of the next few waypoints in plain sight
    A.lookT -= dt;
    if (A.lookT <= 0) {
      A.lookT = 0.4f;
      for (size_t k = std::min(A.path.size() / 2, A.pi + 5); k > A.pi + 1; k--) {
        const Vec2 w = tileCentre(A.path[(k - 1) * 2] - g.world.ox, A.path[(k - 1) * 2 + 1] - g.world.oy);
        if (clearLine(g, a.p, w, a.radius)) { A.pi = k - 1; break; }
      }
    }
    while (A.pi < A.path.size() / 2) {
      const Vec2 w = tileCentre(A.path[A.pi * 2] - g.world.ox, A.path[A.pi * 2 + 1] - g.world.oy);
      if (len2(w - a.p) < 2.5f * 2.5f) { A.pi++; continue; }
      stepToward(g, a, w, speed, dt);
      // progress toward the waypoint (a body sliding to and fro on a corner moves every frame yet gets nowhere)
      if (A.wpIdx != A.pi) { A.wpIdx = A.pi; A.wpBest = 1e9f; }
      const float dw = len(w - a.p);
      if (dw >= A.wpBest - 0.05f) {
        A.stuckT += dt;
        if (A.stuckT > 1.2f) {   // wedged: plan again (and, wedged again, step back to the middle of a walkable tile)
          A.stuckT = 0; A.path.clear(); A.fails++;
          A.wpIdx = (size_t)-1;
          if (A.fails >= 2) unwedge(g, a);
        }
      } else { A.wpBest = dw; A.stuckT = std::max(0.0f, A.stuckT - dt); }
      (void)m;
      return false;
    }
    const Vec2 before = a.p;
    stepToward(g, a, dest, speed, dt);
    if (len2(a.p - before) < 0.0004f) {   // (the last stretch blocked by a corner: close enough)
      A.stuckT += dt;
      if (A.stuckT > 1.2f) { A.stuckT = 0; return true; }
    }
    return len2(dest - a.p) < 3.0f * 3.0f;
  }
  // a body caught on a corner: back to the middle of its own tile when that is free, else of the nearest walkable one
  static void unwedge(Game& g, Actor& a) {
    Vec2 c = tileCentre(tileX(a.p), tileY(a.p));
    if (!g.bodyFree(c, a.radius, false)) c = snapWalk(g, a.p);
    if (g.bodyFree(c, a.radius, false)) a.p = c;
  }
  static void stepToward(Game& g, Actor& a, Vec2 to, float speed, float dt) {
    const Vec2 d = to - a.p;
    const float l = len(d);
    if (l < 0.01f) return;
    g.moveActor(a, d * (std::min(speed * dt, l) / l));
    a.face = faceOf(d);
    a.st = AState::Walk;
  }

  // ---------------------------------------------------------------- spawning residents on the street
  static void dressResident(Game& g, Actor& a, const life::Resident& r) {
    // the look from the resident's own seed (its sex as the census has it), its census name
    for (uint32_t k = 0; k < 12; k++) {
      Rng rr(r.lookSeed + k * 7919u);
      Rng probe = rr;
      const bool fem = probe.f() < 0.45f;
      if (fem != r.female && k + 1 < 12) continue;
      const Role role = a.role;
      g.makeLook(a, role, rr);
      break;
    }
    a.name = r.name;
    // the role they are talked to as: farmers (the fields' jobs), children, else plain townsfolk (the shop keepers'
    // roles stay with the buildings' key persons; a baker at the tavern sells nothing)
    a.role = r.job == life::Job::Child ? Role::Child : (r.job == life::Job::Farmer ? Role::Farmer : Role::Villager);
    if (r.job == life::Job::Beggar) { a.look.outfit = art::Outfit::Rags; a.look.topColor = rgba(120, 104, 84); }
    if (r.job == life::Job::Child) { a.radius = 3.5f; a.look.outfit = art::Outfit::Tunic; a.look.beard = false; }
    if (r.job == life::Job::Elder) a.speed *= 0.8f;
    // (M5 fixer, review: "a row of identical clones outside the gate") the working folk of one trade wear their own
    // homespun: a tunic and breeches of their own dye (earth tones, never a uniform), and hat or none by their own seed
    if (r.job == life::Job::Hunter || r.job == life::Job::Farmer || r.job == life::Job::Herder || r.job == life::Job::Woodcutter ||
        r.job == life::Job::Miner || r.job == life::Job::Labourer || r.job == life::Job::Fisher)
      homespun(a, r);
    else if (r.job != life::Job::Guard && r.job != life::Job::Noble && r.job != life::Job::Beggar && r.job != life::Job::Priest) {
      // everyone else keeps the culture's dress, in their own dye lot: a shade lighter or darker, warmer or cooler
      const uint32_t h = (uint32_t)mixu(r.lookSeed, 0x7EA1ull);
      static const uint32_t tints[6] = {rgba(40, 30, 30), rgba(230, 220, 200), rgba(150, 70, 40), rgba(70, 100, 60), rgba(60, 70, 120), rgba(120, 90, 50)};
      a.look.topColor = tint(a.look.topColor, tints[h % 6u], 0.12f + (float)((h >> 8) % 20u) * 0.01f);
    }
    if (r.job == life::Job::Lamplighter) a.look.weapon = 4;   // the pole
    if (r.job == life::Job::Smith) a.look.weapon = 6;
  }
  static uint32_t tint(uint32_t c, uint32_t to, float t) {
    auto ch = [&](int sh) {
      const float a = (float)((c >> sh) & 255u), b = (float)((to >> sh) & 255u);
      return (uint32_t)std::clamp((int)std::lround(a + (b - a) * t), 0, 255) << sh;
    };
    return (c & 0xFF000000u) | ch(16) | ch(8) | ch(0);
  }
  static void homespun(Actor& a, const life::Resident& r) {
    static const uint32_t tops[8] = {rgba(118, 92, 62), rgba(92, 104, 64), rgba(132, 84, 58), rgba(84, 92, 104),
                                     rgba(146, 120, 80), rgba(104, 70, 60), rgba(70, 88, 70), rgba(150, 132, 104)};
    static const uint32_t legs[4] = {rgba(80, 64, 50), rgba(64, 56, 48), rgba(96, 82, 60), rgba(58, 60, 66)};
    const uint32_t h = (uint32_t)mixu(r.lookSeed, 0xD7E5ull);
    a.look.topColor = tops[h % 8u];
    a.look.bottomColor = legs[(h >> 4) % 4u];
  }
  static int spawnResident(Game& g, int si, life::Census& c, int idx, Vec2 at, bool interior) {
    life::Resident& r = c.res[(size_t)idx];
    Spawn sp;
    sp.npc = true;
    sp.role = life::jobRole(r.job);
    if (sp.role == Role::Guard) sp.role = Role::Villager;
    sp.site = si;
    sp.slot = LIFE_SLOT0 + idx;
    sp.x = tileX(at); sp.y = tileY(at);
    const int id = g.spawnHuman(sp, at);
    Actor& a = g.actors.back();
    a.bldg = -1;   // (one identity indoors and out: npcKey(site, -1, slot))
    // a keyed resident (a workplace's keeper, a home's head) is also its building's interior slot-0 spawn: a quest given
    // by either body is turned in at either (isGiver)
    if (r.keyBldg >= 0 && r.keySlot >= 0) { a.altBldg = g.world.sites[(size_t)si].bldgFirst + r.keyBldg; a.altSlot = r.keySlot; }
    a.stallKeeper = false;   // (spawnHuman makes a merchant beside a stall its keeper: residents keep their own ways)
    a.p = at;
    a.site = si;
    a.resident = idx;
    a.lifeBits = LB_LIFE | LB_FRESH;
    a.fromMap = true;
    dressResident(g, a, r);
    // home is where they live (the militia defend their own street; when trouble comes they run for its door, or for
    // the nearest door when home is far: ai.cpp homeDoor)
    const int hb = r.home >= 0 ? c.bldgHandle(g.world.sites[(size_t)si], r.home) : -1;
    a.homeBldg = -1;
    int hx, hy;
    a.home = !interior && doorStep(g, hb, hx, hy) ? tileCentre(hx, hy) : at;
    a.goal = at;
    // the militia by the resident's own trade (a farmer's pitchfork; others by courage: war_game.cpp's rule, keyed
    // like spawnHuman's)
    if (!interior) {
      // (a farmer with a pitchfork is no braver than his neighbours: courage decides, as for any villager)
      const Role keep = a.role;
      a.role = Role::Villager;
      warArmMilitia(g, a, ((uint64_t)(si + 1) << 24) ^ (uint64_t)(LIFE_SLOT0 + idx));
      a.role = keep;
    } else a.militia = false;
    r.actor = id;
    RT& R = rt(g);
    R.act.erase(id);
    R.stats.spawned++;
    g.perf.spawnedNpcs++;
    return id;
  }
  static void removeActorAt(Game& g, size_t k) {
    Actor& a = g.actors[k];
    release(g, a);
    RT& R = rt(g);
    { auto it = R.act.find(a.id); if (it != R.act.end()) releaseSpot(g, it->second, a.id); }
    R.act.erase(a.id);
    if (a.resident >= 0) {
      const int si = siteOfActor(g, a);
      if (si >= 0)
        if (life::Census* c = g.life.findMut(g.world.sites[(size_t)si].id))
          if (a.resident < (int)c->res.size() && c->res[(size_t)a.resident].actor == a.id) c->res[(size_t)a.resident].actor = -1;
    }
    R.stats.despawned++;
    g.actors.erase(g.actors.begin() + (std::ptrdiff_t)k);
  }

  static bool onScreen(const Game& g, Vec2 p, float margin = 2.0f) {
    const Vec2 d = p - g.pl().p;
    return std::fabs(d.x) < (g.sleepHalfW + margin) * TILE && std::fabs(d.y) < (g.sleepHalfH + margin) * TILE;
  }

  // the street folk of an active settlement by the census (fresh: the site just came in, or the clock jumped: everyone
  // is placed where their plan has them, part-way along their way when their block began only minutes ago)
  static void reconcile(Game& g, int si, bool fresh) {
    RT& R = rt(g);
    life::Census* c = censusOfSite(g, si);
    if (!c || c->res.empty()) return;
    RT::SiteRt& S = siteRt(g, si);
    const Site& st = g.world.sites[(size_t)si];
    refreshPlans(g, S, *c);
    const bool empty = warEmpty(g, si);
    // who is out now (in play or sheltering indoors from a threat)
    std::vector<int> present(c->res.size(), -1);
    int folk = 0;
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& a = g.actors[k];
      if (a.npc && a.fromMap && a.site == si && a.role != Role::Guard && !(a.lifeBits & LB_CRITTER)) folk++;
      if (!(a.lifeBits & LB_LIFE) || a.site != si || a.resident < 0 || a.resident >= (int)c->res.size()) continue;
      present[(size_t)a.resident] = a.id;
    }
    for (const Actor& a : g.sheltered_) {
      if (a.site == si && a.role != Role::Guard) folk++;
      if ((a.lifeBits & LB_LIFE) && a.site == si && a.resident >= 0 && a.resident < (int)c->res.size()) present[(size_t)a.resident] = a.id;
    }
    for (size_t i = 0; i < c->res.size(); i++) c->res[i].actor = present[i];
    // (M4) a home burned out from under them: its people leave town (refugees on the road out)
    for (size_t k = 1; k < g.actors.size(); k++) {
      Actor& a = g.actors[k];
      if (!(a.lifeBits & LB_LIFE) || a.site != si || a.resident < 0 || a.resident >= (int)c->res.size()) continue;
      if (!homeBurned(g, st, *c, c->res[(size_t)a.resident])) continue;
      RT::ARt& A = R.rt(a.id);
      if (A.kind == T_AWAY) continue;
      const RT::Spot* e = pick(g, S.edge.empty() ? S.street : S.edge, mixu(c->seed, (uint64_t)a.resident), a.id);
      if (!e || !onScreen(g, a.p)) { a.lifeBits |= LB_GONE; continue; }
      A.kind = T_AWAY; A.gx = e->x; A.gy = e->y; A.path.clear(); A.arrived = false;
      a.home = tileCentre(e->x - g.world.ox, e->y - g.world.oy);
    }
    if (empty) {   // an abandoned or ruined place: nobody (its key quest givers are the war lane's)
      for (size_t k = g.actors.size(); k-- > 1;)
        if ((g.actors[k].lifeBits & LB_LIFE) && g.actors[k].site == si) removeActorAt(g, k);
      return;
    }
    // fresh: everyone out of place goes (a jump of the clock: the 2:00 street must not keep the noon crowd)
    if (fresh) {
      for (size_t k = g.actors.size(); k-- > 1;) {
        Actor& a = g.actors[k];
        if (!(a.lifeBits & LB_LIFE) || a.site != si || a.resident < 0 || a.resident >= (int)c->res.size()) continue;
        if (g.mode == Mode::Dialogue && g.dlg.actor == a.id) continue;
        int32_t gx, gy;
        int face;
        const life::Resident& r = c->res[(size_t)a.resident];
        // (P1) everyone in the street is placed afresh, as a street walked into is (staggered: some on their way, some at
        // their stop part-way through it): kept as they were, the whole street would turn to its new block at once
        auto ai = R.act.find(a.id);
        const bool held = ai != R.act.end() && ai->second.mood == MOOD_INCIDENT;
        if (!held || !targetSpot(g, si, S, *c, r, S.plans[(size_t)a.resident], gx, gy, face, a.id) || !keepOut(g, si, r)) {
          c->res[(size_t)a.resident].actor = -1;
          present[(size_t)a.resident] = -1;
          removeActorAt(g, k);
          folk--;
        }
      }
    }
    const float ptx = g.pl().p.x / TILE, pty = g.pl().p.y / TILE;
    const bool whole = c->res.size() <= 60 || g.lifeFolkCap > 0;
    int cap = g.lifeFolkCap > 0 ? g.lifeFolkCap : std::min(Game::FOLK_CAP, whole ? 60 : Game::FOLK_CAP);
    // (M5 fixer r2) a hungry, war-torn or emptying town has thinner streets: people stay in, or have gone
    if (g.lifeFolkCap <= 0 && (c->moodFlags & (life::MF_FAMINE | life::MF_WARTORN | life::MF_EMIGRATING))) cap = cap * 3 / 5;
    else if (g.lifeFolkCap <= 0 && (c->moodFlags & life::MF_HUNGRY)) cap = cap * 4 / 5;
    struct Cand { float d; int idx; int32_t gx, gy; int face; };
    std::vector<Cand> want;
    for (size_t i = 0; i < c->res.size(); i++) {
      if (present[i] >= 0 || i >= (size_t)LIFE_SLOTN) continue;
      const life::Resident& r = c->res[i];
      if (r.job == life::Job::Guard || (r.flags & (life::RF_DEAD | life::RF_AWAY))) continue;
      if (homeBurned(g, st, *c, r)) continue;   // (M4) the people of a burned-out home are gone with it
      if (keyAtPost(g, *c, r)) continue;   // at their post indoors (lifeInterior)
      int32_t gx, gy;
      int face;
      if (!targetSpot(g, si, S, *c, r, S.plans[i], gx, gy, face)) continue;
      if (!keepOut(g, si, r)) continue;
      const int lx = gx - g.world.ox, ly = gy - g.world.oy;
      if (!g.world.over.in(lx, ly)) continue;
      const float dx = lx + 0.5f - ptx, dy = ly + 0.5f - pty;
      const float d2 = dx * dx + dy * dy;
      if (!whole && d2 > (float)(Game::FOLK_IN * Game::FOLK_IN)) continue;
      if (!warSpawnAllowed(g, si, lx, ly, LIFE_SLOT0 + (int)i)) continue;
      if (g.felled_.count(si) && g.felled_[si].count(LIFE_SLOT0 + (int)i)) continue;
      want.push_back({d2, (int)i, gx, gy, face});
    }
    std::sort(want.begin(), want.end(), [](const Cand& a, const Cand& b) { return a.d < b.d || (a.d == b.d && a.idx < b.idx); });
    int budget = fresh ? 1 << 30 : 6;   // (a few a step: a schedule window's turn must not land in one frame)
    // children at play take at most an eighth of the street (the square must show the town's working day, not a
    // schoolyard): the ones already out count
    const int playCap = std::max(3, cap / 8);
    int playing = 0;
    for (size_t i = 0; i < c->res.size() && i < S.plans.size(); i++)
      if (present[i] >= 0 && S.plans[i].act == life::Act::Play) playing++;
    // the watch changes at 6:00 and 18:00: the night's (or the day's) men come off their posts and walk home to the
    // barracks, a word to the relief at the post (the posts themselves stay manned: the war lane's watch)
    {
      const bool morning = g.hour >= 5.8f && g.hour < 6.4f, evening = g.hour >= 17.8f && g.hour < 18.4f;
      const int key = g.day * 2 + (evening ? 1 : 0);
      if ((morning || evening) && S.shiftDone != key && !warFrontier(g, si)) {
        S.shiftDone = key;
        int shown = 0;
        for (size_t k = 1; k < g.actors.size() && shown < 2; k++) {
          const Actor post = g.actors[k];   // (a copy: spawning moves the actors)
          if (!post.npc || post.role != Role::Guard || post.site != si || post.resident >= 0 || post.st == AState::Dead) continue;
          if (len2(post.p - g.pl().p) > (float)(Game::FOLK_IN * TILE) * (float)(Game::FOLK_IN * TILE)) continue;
          // a guard of the outgoing shift who lives in the barracks or the lodge
          int gi = -1;
          for (size_t i = 0; i < c->res.size() && i < (size_t)LIFE_SLOTN; i++) {
            const life::Resident& r = c->res[i];
            if (r.job != life::Job::Guard || present[i] >= 0 || (r.flags & (life::RF_DEAD | life::RF_AWAY))) continue;
            if ((int)r.shift != (morning ? 1 : 0)) continue;   // the night watch goes off at dawn, the day watch at dusk
            if ((mixu(c->seed ^ i, (uint64_t)key) % 2u) != 0) continue;
            gi = (int)i;
            break;
          }
          if (gi < 0) break;
          const Vec2 at = snapWalk(g, post.p + Vec2(14.0f, 0.0f));
          const int id = spawnResident(g, si, *c, gi, at, false);
          present[(size_t)gi] = id;
          Actor& a = g.actors.back();
          // off duty: the watch's own look, no weapon drawn, a plain townsman in the AI's eyes for the walk home
          Rng rr(c->res[(size_t)gi].lookSeed);
          g.makeLook(a, Role::Guard, rr);
          a.name = c->res[(size_t)gi].name;
          a.militia = false;
          a.face = faceOf(post.p - a.p);
          say(g, a, "YOUR WATCH, FRIEND.", rgba(220, 226, 240));
          bubble(a, Bubble::Exclaim, 1.2f);
          RT::ARt& A = R.rt(id);
          const life::Plan& p = S.plans[(size_t)gi];
          A.act = p.act; A.place = p.place; A.pbldg = p.bldg;
          int hb = c->res[(size_t)gi].home >= 0 ? c->bldgHandle(st, c->res[(size_t)gi].home) : -1;
          int dx, dy;
          if (!doorStep(g, hb, dx, dy)) { a.lifeBits |= LB_GONE; continue; }
          A.kind = T_DOOR; A.bldg = hb; A.gx = g.world.ox + dx; A.gy = g.world.oy + dy;
          shown++;
        }
      }
    }
    // (15.12) a town people are leaving: those who packed up these last days are seen on the road out, packs on their
    // backs (two a day at most, by day)
    if ((c->moodFlags & life::MF_EMIGRATING) && g.hour >= 8.0f && g.hour < 17.0f && S.emigrantDay != g.day) {
      S.emigrantDay = g.day;
      int shown = 0;
      for (size_t i = 0; i < c->res.size() && i < (size_t)LIFE_SLOTN && shown < 2; i++) {
        const life::Resident& r = c->res[i];
        if (!(r.flags & life::RF_AWAY) || (r.flags & (life::RF_DEAD | life::RF_REFUGEE)) || present[i] >= 0 || r.job == life::Job::Child) continue;
        if ((mixu(c->seed ^ i, (uint64_t)g.day) % 3u) != 0) continue;
        int hx, hy;
        const int hb = r.home >= 0 ? c->bldgHandle(st, r.home) : -1;
        if (!doorStep(g, hb, hx, hy) || S.edge.empty()) continue;
        const int id = spawnResident(g, si, *c, (int)i, tileCentre(hx, hy), false);
        Actor& a = g.actors.back();
        RT::ARt& A = R.rt(id);
        const RT::Spot* e = pick(g, S.edge, mixu(c->seed, i), id);
        if (!e) { a.lifeBits |= LB_GONE; continue; }
        A.kind = T_AWAY; A.gx = e->x; A.gy = e->y; A.act = life::Act::Emigrate; A.place = life::Place::Away; A.pbldg = -1;
        shown++;
      }
      // (M5 fixer r2) nobody has gone yet (the town has only now begun to empty): a household in the street packs up and
      // takes the road out before the player's eyes (they come back with the next plan, as the census has them)
      if (shown == 0 && !S.edge.empty()) {
        for (size_t k = 1; k < g.actors.size() && shown < 2; k++) {
          Actor& a = g.actors[k];
          if (!(a.lifeBits & LB_LIFE) || a.site != si || a.resident < 0 || a.resident >= (int)c->res.size() || (a.lifeBits & LB_KEY)) continue;
          const life::Resident& r = c->res[(size_t)a.resident];
          if (r.job == life::Job::Guard || r.job == life::Job::Noble || r.job == life::Job::Child || (r.flags & life::RF_KEY) || !onScreen(g, a.p, 8.0f)) continue;
          RT::ARt& A = R.rt(a.id);
          if (A.kind != T_SPOT) continue;
          const RT::Spot* e = pick(g, S.edge, mixu(c->seed, (uint64_t)a.resident), a.id);
          if (!e) continue;
          releaseSpot(g, A, a.id);
          A.kind = T_AWAY; A.gx = e->x; A.gy = e->y; A.path.clear(); A.arrived = false; A.mood = 0;
          A.act = life::Act::Emigrate; A.place = life::Place::Away; A.pbldg = -1;
          a.home = tileCentre(e->x - g.world.ox, e->y - g.world.oy);
          if (onScreen(g, a.p)) say(g, a, "THERE'S NOTHING LEFT FOR US HERE.", rgba(220, 210, 190));
          shown++;
          // the spouse and the children go with them
          for (size_t j = 1; j < g.actors.size(); j++) {
            Actor& o = g.actors[j];
            if (o.id == a.id || !(o.lifeBits & LB_LIFE) || o.site != si || o.resident < 0 || o.resident >= (int)c->res.size()) continue;
            const life::Resident& q = c->res[(size_t)o.resident];
            if ((int)q.idx != r.spouse && q.parent != (int16_t)r.idx) continue;
            RT::ARt& B = R.rt(o.id);
            if (B.kind != T_SPOT) continue;
            releaseSpot(g, B, o.id);
            B.kind = T_AWAY; B.gx = e->x; B.gy = e->y; B.path.clear(); B.arrived = false; B.mood = 0;
            B.act = life::Act::Emigrate; B.place = life::Place::Away; B.pbldg = -1;
          }
        }
      }
    }
    for (Cand w : want) {
      if (folk >= cap || budget <= 0) break;
      const life::Resident& r = c->res[(size_t)w.idx];
      const life::Plan& p = S.plans[(size_t)w.idx];
      if (p.act == life::Act::Play && playing >= playCap) continue;
      // the spot again, now that the ones spawned before have claimed theirs
      if (!targetSpot(g, si, S, *c, r, p, w.gx, w.gy, w.face)) continue;
      Vec2 at = tileCentre(w.gx - g.world.ox, w.gy - g.world.oy);
      // where from: the door they came out of, part-way along the way (VISION_PLAN 10.2)
      life::Plan before;
      const float age = blockAge(g, *c, r, p, before);
      int fromB = -1;
      if (before.bldg >= 0) fromB = c->bldgHandle(st, before.bldg);
      int fx, fy;
      const bool haveDoor = doorStep(g, fromB, fx, fy);
      if (fresh) {
        if (haveDoor && age < 0.3f) {
          const Vec2 door = tileCentre(fx, fy);
          const float tripH = len(at - door) / (g.actors[0].speed * kWalk) * (14.5f / 1200.0f);   // in-game hours
          const float t = tripH > 0 ? std::clamp(age / tripH, 0.0f, 1.0f) : 1.0f;
          at = snapWalk(g, door + (at - door) * t);
        }
      } else {
        // never out of thin air on screen: out of the door they left, else their home door, else (off screen) there
        int hx, hy;
        const int hb = r.home >= 0 ? c->bldgHandle(st, r.home) : -1;
        if (haveDoor && len2(tileCentre(fx, fy) - at) < (40.0f * TILE) * (40.0f * TILE)) at = tileCentre(fx, fy);
        else if (onScreen(g, at) && doorStep(g, hb, hx, hy)) at = tileCentre(hx, hy);
        else if (onScreen(g, at)) {
          // the nearest door of the settlement to the spot
          int best = -1, bd = 1 << 30;
          for (int b = st.bldgFirst; b < st.bldgFirst + st.bldgCount && b < (int)g.world.over.bldgs.size(); b++) {
            int x, y;
            if (!doorStep(g, b, x, y)) continue;
            const int d = std::abs(x - tileX(at)) + std::abs(y - tileY(at));
            if (d < bd) { bd = d; best = b; }
          }
          int x, y;
          if (best < 0 || !doorStep(g, best, x, y)) continue;
          at = tileCentre(x, y);
        }
      }
      const int id = spawnResident(g, si, *c, w.idx, at, false);
      Actor& a = g.actors.back();
      RT::ARt& A = R.rt(id);
      A.kind = T_SPOT; A.gx = w.gx; A.gy = w.gy; A.act = p.act; A.place = p.place; A.pbldg = p.bldg;
      claimSpot(g, A, id, w.gx, w.gy);
      A.pace = personPace(*c, r);
      // (P1) the segment's task: its first stop is where they are bound (a stall, a door on a round, a new spot)
      const bool tasked = rhythmic(r, p, A);
      if (tasked) pickTask(g, a, A, *c, r, p, S, segOf(g, *c, r));
      if (fresh && tasked) {
        // a street walked into is mid-day (P1 staggered arrival): those whose block began minutes ago are part-way from
        // the door they left by; of the rest, some are on their way somewhere (a few tiles off, walking), the others at
        // their stop, part-way through its wait
        const Vec2 tgt = tileCentre(A.gx - g.world.ox, A.gy - g.world.oy);
        const uint32_t hh = mixu(c->seed ^ ((uint64_t)w.idx << 4), (uint64_t)(absHour(g) * 12.0));
        Vec2 from;
        if (haveDoor && age < 0.3f) {
          const Vec2 door = tileCentre(fx, fy);
          const float tripH = len(tgt - door) / (g.actors[0].speed * kWalk) * (14.5f / 1200.0f);
          const float t = tripH > 0 ? std::clamp(age / tripH, 0.0f, 1.0f) : 1.0f;
          at = snapWalk(g, door + (tgt - door) * t);
        } else if (hh % 100u < 42u && midRoute(g, a, tgt, hh, from)) at = from;
        else at = tgt;
        a.p = at; a.goal = at;
      }
      if (p.act == life::Act::Play) playing++;
      A.arrived = len2(a.p - tileCentre(A.gx - g.world.ox, A.gy - g.world.oy)) < 4.0f;
      A.watchP = a.p;
      a.face = w.face;
      if (fresh && A.arrived) {
        settleOutdoor(g, a, A, *c, c->res[(size_t)w.idx]);
        if (A.task != TK_NONE) {
          taskArrive(g, a, A, r);
          if (A.dwell < 1e5f) A.dwell *= 0.15f + (float)(mixu(c->seed ^ (uint64_t)w.idx, (uint64_t)g.day) % 85u) * 0.01f;
        }
      }
      folk++;
      budget--;
    }
    stageMood(g, si, *c, S);
  }
  // (M5 fixer r2, 15.12 visible mood) a hungry town's street shows it: a few of the people idling near the player sit
  // down to beg by the stalls and the square (a bowl held out, a bread bubble); a restless one's has a scuffle (two at
  // it with their fists, angry bubbles). Staged once an in-game hour among the people at leisure near the player.
  static void stageMood(Game& g, int si, life::Census& c, RT::SiteRt& S) {
    RT& R = rt(g);
    const uint16_t mf = c.moodFlags;
    const int key = g.day * 24 + (int)g.hour;
    // (topped up at every reconcile: people just spawned are still walking to their spots; the choice is keyed by the
    // hour, so the same few are picked while it lasts)
    S.moodStage = key;
    int begN = 0, brawlN = 0;
    const bool hungry = (mf & (life::MF_HUNGRY | life::MF_FAMINE)) != 0;
    const bool brawls = (mf & life::MF_BRAWLS) != 0 && g.hour >= 8.0f && g.hour < 23.0f;
    struct C { float d; int k; };
    std::vector<C> cand;
    for (size_t k = 1; k < g.actors.size(); k++) {
      Actor& a = g.actors[k];
      if (!(a.lifeBits & LB_LIFE) || a.site != si || a.resident < 0 || a.resident >= (int)c.res.size() || a.st == AState::Dead) continue;
      auto it = R.act.find(a.id);
      if (it == R.act.end()) continue;
      RT::ARt& A = it->second;
      // the last hour's vignettes end when the town is better (a beggar gets up, a brawl breaks off)
      if (A.mood == 1 && !hungry) { A.mood = 0; a.posture = Posture::None; }
      if (A.mood == 2 && !brawls) { A.mood = 0; A.partner = -1; a.posture = Posture::None; }
      if (A.mood == 1) begN++;
      if (A.mood == 2) brawlN++;
      if (A.mood || A.kind != T_SPOT || !A.arrived || A.partner >= 0) continue;
      const life::Resident& r = c.res[(size_t)a.resident];
      if (r.job == life::Job::Child || r.job == life::Job::Guard || r.job == life::Job::Noble || (r.flags & life::RF_KEY) || (a.lifeBits & LB_KEY)) continue;
      const life::Act pa = A.act;
      if (!(pa == life::Act::Wander || pa == life::Act::Socialise || pa == life::Act::Shop || pa == life::Act::Eat || pa == life::Act::Tavern)) continue;
      const float d = len2(a.p - g.pl().p);
      if (d > (14.0f * TILE) * (14.0f * TILE)) continue;
      cand.push_back({d + (float)(mixu(c.seed ^ (uint64_t)a.resident, (uint64_t)key) % 4096u), (int)k});
    }
    std::sort(cand.begin(), cand.end(), [](const C& a, const C& b) { return a.d < b.d || (a.d == b.d && a.k < b.k); });
    size_t ci = 0;
    if (hungry) {
      const int want = ((mf & life::MF_FAMINE) ? 3 : 2) - begN;
      for (int n = 0; n < want && ci < cand.size(); ci++, n++) {
        Actor& a = g.actors[(size_t)cand[ci].k];
        RT::ARt& A = R.rt(a.id);
        A.mood = 1;
        a.posture = Posture::Beg;
        a.postureT = 0;
        a.face = 0;
        bubble(a, Bubble::Bread, 3.0f);
        begN++;
      }
      // too few out at leisure (a famine noon: the rest are in the fields): the hungriest kept at home come out to beg
      // at the square or the market, the nearest free spots to the player
      const int wantAll = (mf & life::MF_FAMINE) ? 3 : 2;
      while (begN < wantAll) {
        const int i = homeBody(g, si, c, S, key, 60, begN);
        if (i < 0) break;
        const RT::Spot* sp = nearSpot(g, S, -1, -1);
        if (!sp || spawnMood(g, si, c, S, i, sp->x, sp->y, 1) < 0) break;
        begN++;
      }
    }
    if (brawls && brawlN == 0) {
      bool staged = false;
      for (; ci + 1 < cand.size(); ci++) {
        Actor& a = g.actors[(size_t)cand[ci].k];
        // the nearest other candidate within a few tiles is the one they fall out with
        size_t bj = 0;
        float bd = (6.0f * TILE) * (6.0f * TILE);
        for (size_t j = ci + 1; j < cand.size(); j++) {
          const float d = len2(g.actors[(size_t)cand[j].k].p - a.p);
          if (d < bd) { bd = d; bj = j; }
        }
        if (!bj) continue;
        Actor& o = g.actors[(size_t)cand[bj].k];
        RT::ARt& A = R.rt(a.id);
        RT::ARt& B = R.rt(o.id);
        A.mood = B.mood = 2;
        A.partner = o.id; B.partner = a.id;
        A.t2 = 0; B.t2 = 0.7f;
        a.posture = o.posture = Posture::None;
        const Vec2 mid = (a.p + o.p) * 0.5f;
        const Vec2 dir = len2(o.p - a.p) > 1.0f ? norm(o.p - a.p) : Vec2(1, 0);
        const Vec2 pa = mid - dir * 8.0f, pb = mid + dir * 8.0f;
        if (g.bodyFree(pa, a.radius, false)) a.p = pa;
        if (g.bodyFree(pb, o.radius, false)) o.p = pb;
        a.face = faceOf(o.p - a.p); o.face = faceOf(a.p - o.p);
        bubble(a, Bubble::Anger, 1.5f);
        if (onScreen(g, a.p)) say(g, a, "THAT WAS MY BREAD, YOU THIEF!", rgba(240, 150, 120));
        staged = true;
        break;
      }
      // nobody idle near enough: two neighbours come out and fall to it on the square (a spot and the next one by it)
      if (!staged) {
        const int i = homeBody(g, si, c, S, key, 101, 7);
        const int j = i >= 0 ? homeBody(g, si, c, S, key, 101, 8, i) : -1;
        const RT::Spot* s1 = j >= 0 ? nearSpot(g, S, -1, -1) : nullptr;
        const RT::Spot* s2 = s1 ? nearSpot(g, S, s1->x, s1->y) : nullptr;
        if (s1 && s2) {
          const int ia = spawnMood(g, si, c, S, i, s1->x, s1->y, 2);
          const int ib = ia >= 0 ? spawnMood(g, si, c, S, j, s2->x, s2->y, 2) : -1;
          if (ia >= 0 && ib >= 0) {
            RT::ARt& A = R.rt(ia);
            RT::ARt& B = R.rt(ib);
            A.partner = ib; B.partner = ia; A.t2 = 0; B.t2 = 0.7f;
          }
        }
      }
    }
  }
  // (stageMood) a resident kept at home this hour who may come out for a vignette: not in play, a grown-up of the
  // street (no keeper, guard or noble), hungrier than `hungerBelow`, picked by the hour (`salt` varies the pick)
  static int homeBody(Game& g, int si, life::Census& c, RT::SiteRt& S, int key, int hungerBelow, int salt, int not1 = -1) {
    if (S.plans.size() != c.res.size() || g.hour < 8.0f || g.hour >= 20.0f) return -1;
    int best = -1;
    uint32_t bh = 0xFFFFFFFFu;
    for (size_t i = 0; i < c.res.size() && i < (size_t)LIFE_SLOTN; i++) {
      const life::Resident& r = c.res[i];
      if ((int)i == not1 || r.actor >= 0 || (r.flags & (life::RF_DEAD | life::RF_AWAY | life::RF_KEY)) || r.job == life::Job::Child ||
          r.job == life::Job::Guard || r.job == life::Job::Noble || r.job == life::Job::Beggar)
        continue;
      bool out = false;
      for (size_t k = 1; k < g.actors.size() && !out; k++) {
        const Actor& a = g.actors[k];
        if ((a.lifeBits & LB_LIFE) && a.site == si && a.resident == (int)i) out = true;
      }
      if (out) continue;
      const life::Act pa = S.plans[i].act;
      if (!(pa == life::Act::Home || pa == life::Act::Eat || pa == life::Act::Wander)) continue;
      if (r.need[(int)life::Need::Hunger] >= hungerBelow) continue;
      const uint32_t h = mixu(c.seed ^ (i * 977u), (uint64_t)key * 31u + (uint64_t)salt);
      if (h < bh) { bh = h; best = (int)i; }
    }
    return best;
  }
  // (stageMood) the free square or market spot nearest the player (beside (nx, ny) when given: within two tiles of it)
  static const RT::Spot* nearSpot(Game& g, RT::SiteRt& S, int32_t nx, int32_t ny) {
    const RT::Spot* best = nullptr;
    float bd = (16.0f * TILE) * (16.0f * TILE);
    for (const std::vector<RT::Spot>* v : {&S.market, &S.plaza, &S.street})
      for (const RT::Spot& s : *v) {
        if (!spotOk(g, s) || spotTaken(g, s.x, s.y, -1)) continue;
        if (nx != -1 || ny != -1) {
          if ((s.x == nx && s.y == ny) || std::abs(s.x - nx) > 2 || std::abs(s.y - ny) > 1) continue;
          const float d = (float)(std::abs(s.x - nx) + std::abs(s.y - ny));
          if (d < bd) { bd = d; best = &s; }
          continue;
        }
        const Vec2 at = tileCentre(s.x - g.world.ox, s.y - g.world.oy);
        const float d = len2(at - g.pl().p);
        if (d < bd && d > (2.0f * TILE) * (2.0f * TILE)) { bd = d; best = &s; }
      }
    return best;
  }
  // (stageMood) bring resident i out to spot (gx, gy) for a vignette (mood 1 begging, 2 brawling): out of their own door
  // when the spot is on screen (never out of thin air), already there when it is not. Returns the actor id or -1.
  static int spawnMood(Game& g, int si, life::Census& c, RT::SiteRt& S, int i, int32_t gx, int32_t gy, uint8_t mood) {
    RT& R = rt(g);
    const life::Resident& r = c.res[(size_t)i];
    const Vec2 at = tileCentre(gx - g.world.ox, gy - g.world.oy);
    Vec2 from = at;
    if (onScreen(g, at, -1.0f)) {
      int hx, hy;
      const int hb = r.home >= 0 ? c.bldgHandle(g.world.sites[(size_t)si], r.home) : -1;
      if (doorStep(g, hb, hx, hy) && len2(tileCentre(hx, hy) - at) <= (16.0f * TILE) * (16.0f * TILE)) from = tileCentre(hx, hy);
      else {   // home is across town: out of the nearest door to the spot (a neighbour's, a shop's)
        const Site& st = g.world.sites[(size_t)si];
        int bd = 1 << 30;
        for (int b = st.bldgFirst; b < st.bldgFirst + st.bldgCount && b < (int)g.world.over.bldgs.size(); b++) {
          int x, y;
          if (!doorStep(g, b, x, y)) continue;
          const int d = std::abs(x - tileX(at)) + std::abs(y - tileY(at));
          if (d < bd) { bd = d; from = tileCentre(x, y); }
        }
        if (bd > 16) return -1;
      }
    }
    const int id = spawnResident(g, si, c, i, from, false);
    Actor& a = g.actors.back();
    RT::ARt& A = R.rt(id);
    const life::Plan& p = S.plans[(size_t)i];
    A.kind = T_SPOT; A.gx = gx; A.gy = gy; A.act = p.act; A.place = p.place; A.pbldg = p.bldg;
    A.arrived = from.x == at.x && from.y == at.y; A.mood = mood;
    claimSpot(g, A, id, gx, gy);
    if (A.arrived && mood == 1) { a.posture = Posture::Beg; a.face = 0; }
    return id;
  }
  static bool homeBurned(const Game& g, const Site& st, const life::Census& c, const life::Resident& r) {
    const int hb = r.home >= 0 ? c.bldgHandle(st, r.home) : -1;
    return hb >= 0 && hb < (int)g.world.over.bldgs.size() && g.world.over.bldgs[(size_t)hb].charred == 2;
  }
  // the nearest walkable open tile to a point (its centre)
  static Vec2 snapWalk(Game& g, Vec2 p) {
    const Map& m = g.world.over;
    const int tx = tileX(p), ty = tileY(p);
    for (int r = 0; r < 6; r++)
      for (int oy = -r; oy <= r; oy++)
        for (int ox = -r; ox <= r; ox++) {
          if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
          const int x = tx + ox, y = ty + oy;
          if (m.in(x, y) && !m.blocked(x, y) && m.bldgAt[(size_t)y * m.w + x] < 0) return tileCentre(x, y);
        }
    return p;
  }
  // a resident who should stay out whatever the hour: the giver of an open quest or a story's person (daytime)
  static bool keepOut(Game& g, int si, const life::Resident& r) {
    if ((r.flags & life::RF_GRIEVING) && r.job != life::Job::Child && r.job != life::Job::Beggar) {
      // the grieving stay home (out only for work)
      (void)si;
    }
    return true;
  }
  // a key person (a building's interior slot 0: the innkeeper, the smith, the merchant, the priest, the lord) is at
  // their post now: their plan has them in the building, or it is business hours (quest givers stay reachable)
  // a workplace's keeper (the census keys its master to the building's interior slot 0); a home's head is keyed to
  // their home's slot 0 but lives by the clock like everyone else
  static bool workKey(const life::Resident& r) { return r.keySlot == 0 && r.keyBldg >= 0 && r.keyBldg == r.work; }
  static bool keyAtPost(Game& g, const life::Census& c, const life::Resident& r) {
    if (!workKey(r)) return false;
    using life::Job;
    if (r.job == Job::Innkeeper || r.job == Job::Noble) return true;   // the inn never closes; the court sits
    const life::Plan p = g.life.plan(g.world, c, r, g.day, g.hour);
    if (p.bldg == r.keyBldg) return true;
    // (every workplace's keeper keeps business hours there: a mill's or a granary's master is a farmer by the census's
    //  reckoning, yet the mill's door is where people come to find them)
    return g.hour >= 8.0f && g.hour < 18.0f;
  }

  // a resident who just got to their outdoor spot: the posture of what they do there
  static void settleOutdoor(Game& g, Actor& a, RT::ARt& A, const life::Census& c, const life::Resident& r) {
    using life::Act;
    using life::Job;
    a.st = AState::Idle;
    a.posture = Posture::None;
    a.postureT = 0;
    A.t = 4.0f + (float)(mixu(c.seed, (uint64_t)a.id * 31u + (uint64_t)g.day) % 1000u) * 0.02f;
    switch (A.act) {
      case Act::Work:
        if (A.place == life::Place::Field) {
          switch (r.job) {
            case Job::Fisher: a.posture = Posture::Fish; break;
            case Job::Woodcutter: a.posture = Posture::Chop; break;
            case Job::Miner: a.posture = Posture::Hammer; break;
            case Job::Herder: case Job::Hunter: a.posture = Posture::None; break;
            default: a.posture = Posture::Hoe; break;
          }
          // face the work: the water, the woodpile, the ore (the spot's facing)
          if (RT::SiteRt* S = siteRtById(g, c.site))
            for (const auto* v : {&S->water, &S->wood, &S->mine})
              for (const RT::Spot& s : *v)
                if (s.x == A.gx && s.y == A.gy) { a.face = s.face; break; }
        } else if (r.job == Job::Labourer) a.posture = Posture::Carry;
        else if (r.job == Job::Merchant || r.job == Job::Tailor) a.posture = Posture::None;
        else a.posture = Posture::Sweep;
        break;
      case Act::Beg: a.posture = Posture::Beg; a.face = 0; break;
      case Act::Sleep:   // a beggar dozing in a doorway (anyone else out at this hour simply waits)
        if (r.job == Job::Beggar) { a.posture = Posture::Beg; bubble(a, Bubble::Zzz, 3); }
        break;
      case Act::Perform: a.posture = bardPosture(g.world.cultureOf(siteOfActor(g, a))); a.face = 0; break;
      case Act::Pray: a.posture = Posture::Pray; break;
      case Act::LightLamps: a.posture = Posture::Lamp; a.face = 1; A.t = 2.5f; break;
      case Act::Play: a.posture = Posture::None; break;   // (playTag picks the game)
      case Act::Eat: a.posture = Posture::Eat; break;
      default: break;
    }
  }
  static RT::SiteRt* siteRtById(Game& g, ew::Gid id) {
    auto it = rt(g).sites.find(id);
    return it == rt(g).sites.end() ? nullptr : &it->second;
  }

  // ================================================================ P1 Fresh Roads: the street's day (TaskK)
  // A resident out in the street lives their block in segments (segMinutes: 22-57 game minutes, a phase of their own);
  // each segment is a task with a route of stops (startLeg / taskArrive / taskTick). Deterministic per resident, day and
  // segment; the runtime only decides who is free to claim a spot or a door step.
  static float personPace(const life::Census& c, const life::Resident& r) {
    return 0.86f + (float)(mixu(c.seed ^ 0x9ACEull, (uint64_t)r.idx) % 29u) * 0.01f;
  }
  static int segMinutes(const life::Census& c, const life::Resident& r) { return 22 + (int)(mixu(c.seed ^ 0x5E6Dull, (uint64_t)r.idx * 131u) % 36u); }
  static int32_t segOf(const Game& g, const life::Census& c, const life::Resident& r) {
    const int L = segMinutes(c, r);
    const double mins = absHour(g) * 60.0 + (double)(mixu(c.seed ^ 0xF4A5ull, (uint64_t)r.idx) % (uint32_t)L);
    return (int32_t)std::floor(mins / (double)L);
  }
  // does the street's rhythm drive this person now (not a beggar's pitch, the bard's corner, the lamplighter's round, a
  // brawl, a staged vignette, the night)
  static bool rhythmic(const life::Resident& r, const life::Plan& p, const RT::ARt& A) {
    using life::Act;
    if (A.kind != T_SPOT || A.mood) return false;
    switch (p.act) {
      case Act::Perform: case Act::LightLamps: case Act::Brawl: case Act::Emigrate: case Act::Sleep: case Act::Patrol: return false;
      default: break;
    }
    // (a beggar moves along now and then: another pitch by the stalls, the alms meal at the temple's door)
    return r.job != life::Job::Guard;
  }
  // a trade with no workplace at work in the street (WORK@STREET: merchants, tailors, scholars, stablehands, smiths and
  // bakers without a shop of their own, servants, priests without a temple)
  static bool streetTrade(const life::Resident& r, const life::Plan& p) {
    using life::Job;
    if (p.act != life::Act::Work || p.place != life::Place::Street) return false;
    return r.job != Job::Labourer && r.job != Job::Lamplighter && r.job != Job::Beggar && r.job != Job::Guard && r.job != Job::Child;
  }
  static bool leisureAct(const life::Plan& p) {
    using life::Act;
    return p.bldg < 0 && (p.act == Act::Wander || p.act == Act::Socialise || p.act == Act::Shop || p.act == Act::Home || p.act == Act::Eat ||
                          p.act == Act::Pray || p.act == Act::Tavern || p.act == Act::Bathe);
  }
  static void breakPair(Game& g, RT::ARt& A, int self) {
    if (A.partner < 0) return;
    RT& R = rt(g);
    auto it = R.act.find(A.partner);
    if (it != R.act.end() && it->second.partner == self) it->second.partner = -1;
    A.partner = -1;
  }
  static int resBldg(const Game& g, int si, const life::Census& c, int off) { return off >= 0 ? c.bldgHandle(g.world.sites[(size_t)si], off) : -1; }
  // a free place at a building's door (global tile): the step itself, else a tile beside or below it (two people never
  // stand on one tile; a busy shop's door has a queue round it)
  static bool doorNear(Game& g, int b, int32_t& gx, int32_t& gy, int self) {
    int x, y;
    if (!doorStep(g, b, x, y)) return false;
    static const int ox[6] = {0, -1, 1, 0, -1, 1}, oy[6] = {0, 0, 0, 1, 1, 1};
    const Map& m = g.world.over;
    for (int k = 0; k < 6; k++) {
      const int tx = x + ox[k], ty = y + oy[k];
      if (!m.in(tx, ty) || m.blocked(tx, ty) || m.bldgAt[(size_t)ty * m.w + tx] >= 0 || m.wall[(size_t)ty * m.w + tx]) continue;
      if (spotTaken(g, g.world.ox + tx, g.world.oy + ty, self)) continue;
      gx = g.world.ox + tx; gy = g.world.oy + ty;
      return true;
    }
    return false;
  }
  // where a trade's goods come from: the bakery (or the mill), the smithy, the market hall, the inn's yard, the lord's
  // seat, the temple; a quarter of the rounds start from their own house
  static int sourceOf(Game& g, int si, const life::Census& c, const life::Resident& r) {
    using life::Job;
    int off = -1;
    switch (r.job) {
      case Job::Baker: off = c.bakery >= 0 ? c.bakery : c.mill; break;
      case Job::Smith: off = c.smithy; break;
      case Job::Merchant: case Job::Tailor: off = c.market >= 0 ? c.market : c.inn; break;
      case Job::Stablehand: off = c.inn; break;
      case Job::Servant: off = c.seat >= 0 ? c.seat : c.inn; break;
      case Job::Scholar: off = c.temple >= 0 ? c.temple : c.seat; break;
      case Job::Priest: off = c.temple; break;
      default: break;
    }
    if (off < 0 || mixu(c.seed ^ r.idx, 0x50Cull) % 4u == 0) off = r.home >= 0 ? r.home : off;
    int x, y;
    int b = resBldg(g, si, c, off);
    if (doorStep(g, b, x, y)) return b;
    b = resBldg(g, si, c, c.inn);
    return doorStep(g, b, x, y) ? b : -1;
  }
  // a real door of the census within [4, maxD] tiles of (gx, gy): a resident's home, the inn, the market (not notB);
  // the first hashed candidate, or a nearer one half the time (short rounds and long ones)
  static int customerDoor(Game& g, int si, const life::Census& c, int32_t gx, int32_t gy, uint32_t h, int notB, int maxD, int self) {
    const int n = (int)c.res.size();
    if (n <= 0) return -1;
    int best = -1, bd = 1 << 30;
    for (int k = 0; k < 14; k++) {
      const uint32_t hk = mixu(h, (uint64_t)k * 7u + 1u);
      const uint32_t kind = hk % 10u;
      const int off = kind == 0 && c.inn >= 0 ? c.inn : kind == 1 && c.market >= 0 ? c.market : c.res[(size_t)((hk >> 8) % (uint32_t)n)].home;
      const int b = resBldg(g, si, c, off);
      if (b < 0 || b == notB) continue;
      int32_t dx, dy;
      if (!doorNear(g, b, dx, dy, self)) continue;
      const int d = std::abs(dx - gx) + std::abs(dy - gy);
      if (d < 4 || d > maxD) continue;
      if (best < 0 || (((hk >> 4) & 1u) && d < bd)) { best = b; bd = d; }
      if (k >= 5) break;
    }
    return best;
  }
  // a free spot of a list within [minD, maxD] tiles (Manhattan) of (gx, gy), by hash
  static const RT::Spot* pickNear(Game& g, const std::vector<RT::Spot>& v, uint32_t h, int32_t gx, int32_t gy, int minD, int maxD, int self) {
    if (v.empty()) return nullptr;
    for (int k = 0; k < 16; k++) {
      const RT::Spot& s = v[(h + (uint32_t)k * 2654435761u) % v.size()];
      const int d = std::abs(s.x - gx) + std::abs(s.y - gy);
      if (d < minD || d > maxD) continue;
      if (spotOk(g, s) && !spotTaken(g, s.x, s.y, self)) return &s;
    }
    // (P1 fixer r2) a city's lists run to thousands of spots: sixteen picks from the whole list rarely land within a few
    // tiles, and a stall keeper's every trip for stock then failed (stood at the stall for hours: town-motion seed 5).
    // One pass from a hashed start finds the first fitting spot (once a leg: cheap)
    const size_t n = v.size(), s0 = (size_t)(h % (uint32_t)n);
    for (size_t k = 0; k < n; k++) {
      const RT::Spot& s = v[(s0 + k) % n];
      const int d = std::abs(s.x - gx) + std::abs(s.y - gy);
      if (d < minD || d > maxD) continue;
      if (spotOk(g, s) && !spotTaken(g, s.x, s.y, self)) return &s;
    }
    return nullptr;
  }
  static void headTo(Game& g, Actor& a, RT::ARt& A, int32_t gx, int32_t gy) {
    A.gx = gx; A.gy = gy; A.arrived = false; A.path.clear(); A.pi = 0; A.fails = 0; A.want = false; A.stuckT = 0;
    A.watchT = 0; A.watchP = a.p;
    claimSpot(g, A, a.id, gx, gy);
  }
  // a new segment: what this person does with it (weights by trade; the leisure hours stroll, look round the stalls,
  // call on a friend)
  static void pickTask(Game& g, Actor& a, RT::ARt& A, life::Census& c, const life::Resident& r, const life::Plan& p, RT::SiteRt& S, int32_t seg) {
    using life::Job;
    RT& R = rt(g);
    A.seg = seg; A.leg = 0; A.dwell = 0; A.srcB = -1; A.walkPost = Posture::None; A.retry = 0; A.slip = false;
    breakPair(g, A, a.id);
    const uint32_t h = mixu(c.seed ^ ((uint64_t)r.idx << 12), (uint64_t)(uint32_t)seg * 2654435761u + 0x7A5Cull);
    int w[TK_COUNT] = {};
    if (streetTrade(r, p)) {
      switch (r.job) {
        case Job::Merchant: w[TK_STALL] = 42; w[TK_DELIVER] = 30; w[TK_SHOP] = 6; w[TK_VISIT] = 10; w[TK_NONE] = 12; break;
        case Job::Tailor: w[TK_STALL] = 36; w[TK_DELIVER] = 28; w[TK_VISIT] = 14; w[TK_SHOP] = 10; w[TK_NONE] = 12; break;
        case Job::Baker: w[TK_DELIVER] = 46; w[TK_STALL] = 32; w[TK_SWEEP] = 10; w[TK_NONE] = 12; break;
        case Job::Smith: w[TK_DELIVER] = 46; w[TK_SWEEP] = 22; w[TK_STALL] = 12; w[TK_NONE] = 20; break;
        case Job::Stablehand: w[TK_DELIVER] = 46; w[TK_SWEEP] = 34; w[TK_NONE] = 20; break;
        case Job::Scholar: w[TK_READ] = 38; w[TK_VISIT] = 24; w[TK_SHOP] = 14; w[TK_DELIVER] = 12; w[TK_NONE] = 12; break;
        case Job::Priest: w[TK_READ] = 34; w[TK_VISIT] = 34; w[TK_SWEEP] = 14; w[TK_NONE] = 18; break;
        case Job::Servant: w[TK_DELIVER] = 46; w[TK_SHOP] = 24; w[TK_SWEEP] = 20; w[TK_NONE] = 10; break;
        default: w[TK_DELIVER] = 36; w[TK_SWEEP] = 24; w[TK_VISIT] = 14; w[TK_NONE] = 26; break;
      }
    } else if (leisureAct(p) && r.job != Job::Child && r.job != Job::Beggar) {
      w[TK_NONE] = 52;
      w[TK_SHOP] = p.act == life::Act::Shop ? 56 : 18;
      w[TK_VISIT] = r.job == Job::Elder ? 26 : 18;
    } else w[TK_NONE] = 1;
    if (S.market.empty()) { w[TK_DELIVER] += w[TK_STALL]; w[TK_STALL] = 0; w[TK_NONE] += w[TK_SHOP]; w[TK_SHOP] = 0; }
    int sum = 0;
    for (int k = 0; k < TK_COUNT; k++) sum += w[k];
    int roll = (int)(h % (uint32_t)std::max(1, sum)), task = TK_NONE;
    for (int k = 0; k < TK_COUNT; k++) {
      if (roll < w[k]) { task = k; break; }
      roll -= w[k];
    }
    A.task = (uint8_t)task;
    if (!startLeg(g, a, A, c, r, p, S)) { A.task = TK_NONE; A.leg = 0; startLeg(g, a, A, c, r, p, S); }
    R.street.tasks[std::min<int>(A.task, 7)]++;
  }
  // the next stop of the task's route (false: no way to do it here: the caller falls back to a plain spot)
  static bool startLeg(Game& g, Actor& a, RT::ARt& A, life::Census& c, const life::Resident& r, const life::Plan& p, RT::SiteRt& S) {
    const int si = a.site;
    const int32_t px = g.world.ox + tileX(a.p), py = g.world.oy + tileY(a.p);
    const uint32_t h = mixu(c.seed ^ ((uint64_t)r.idx << 12) ^ 0x1E6ull, (uint64_t)(uint32_t)A.seg * 977u + A.leg);
    int32_t gx = 0, gy = 0;
    switch (A.task) {
      case TK_STALL: {
        if (A.leg == 0) {   // a stall to mind (a market row's front, facing the passers-by)
          const RT::Spot* s = pickNear(g, S.market, h, px, py, 0, 40, a.id);
          if (!s) s = pick(g, S.market, h, a.id);
          if (!s) return false;
          A.ax = s->x; A.ay = s->y;
          A.walkPost = Posture::None;
          headTo(g, a, A, s->x, s->y);
          return true;
        }
        bool fetched = A.leg >= 2;
        if (A.leg & 1) {   // a trip for stock: a store door near the stall (none near: stay on)
          const int b = customerDoor(g, si, c, A.ax, A.ay, h, -1, 18, a.id);
          if (b >= 0 && doorNear(g, b, gx, gy, a.id)) { A.walkPost = Posture::None; headTo(g, a, A, gx, gy); return true; }
          if (const RT::Spot* s = pickNear(g, S.street, h, A.ax, A.ay, 3, 10, a.id)) { A.walkPost = Posture::None; headTo(g, a, A, s->x, s->y); return true; }   // (a handcart down the lane)
          A.leg++;
          fetched = false;
        }
        A.walkPost = fetched ? Posture::Carry : Posture::None;   // back with the stock
        if (spotTaken(g, A.ax, A.ay, a.id)) return false;
        headTo(g, a, A, A.ax, A.ay);
        return true;
      }
      case TK_DELIVER: {
        if (A.leg == 0) A.srcB = sourceOf(g, si, c, r);
        if (A.srcB < 0) return false;
        if ((A.leg & 1) == 0) {   // to the source (the goods to fetch)
          if (!doorNear(g, A.srcB, gx, gy, a.id)) return false;
          A.walkPost = Posture::None;
          headTo(g, a, A, gx, gy);
          return true;
        }
        int sx, sy;
        if (!doorStep(g, A.srcB, sx, sy)) return false;
        const int b = customerDoor(g, si, c, g.world.ox + sx, g.world.oy + sy, h, A.srcB, 30, a.id);
        if (b < 0 || !doorNear(g, b, gx, gy, a.id)) return false;
        A.walkPost = Posture::Carry;   // a sack, a basket, a crate on the shoulder
        headTo(g, a, A, gx, gy);
        return true;
      }
      case TK_SWEEP: {
        if (A.leg == 0) {   // their own lane (the house's, or the trade's door)
          int ox = px, oy = py, x, y;
          const int b = r.home >= 0 ? resBldg(g, si, c, r.home) : sourceOf(g, si, c, r);
          if (doorStep(g, b, x, y)) { ox = g.world.ox + x; oy = g.world.oy + y; }
          A.ax = ox; A.ay = oy;
        }
        const RT::Spot* s = pickNear(g, S.street, h, A.leg == 0 ? A.ax : px, A.leg == 0 ? A.ay : py, A.leg == 0 ? 0 : 1, A.leg == 0 ? 7 : 4, a.id);
        if (!s) s = pickNear(g, S.street, h, A.ax, A.ay, 0, 10, a.id);
        if (!s) return false;
        A.walkPost = Posture::None;
        headTo(g, a, A, s->x, s->y);
        return true;
      }
      case TK_READ: {
        const RT::Spot* s = pickNear(g, S.plaza, h, px, py, 3, 24, a.id);
        if (!s) s = pick(g, S.plaza, h, a.id);
        if (!s) return false;
        A.walkPost = Posture::None;
        headTo(g, a, A, s->x, s->y);
        return true;
      }
      case TK_VISIT: {   // a friend's door (else a neighbour's)
        int b = -1;
        const std::vector<int> fr = g.life.friendsOf(c, r.idx);
        for (size_t k = 0; k < fr.size() && k < 3 && b < 0; k++) {
          const int f = fr[(k + h) % std::min<size_t>(fr.size(), 3)];
          if (f < 0 || f >= (int)c.res.size() || c.res[(size_t)f].home < 0 || c.res[(size_t)f].home == r.home) continue;
          const int fb = resBldg(g, si, c, c.res[(size_t)f].home);
          int32_t dx, dy;
          if (fb >= 0 && doorNear(g, fb, dx, dy, a.id) && std::abs(dx - px) + std::abs(dy - py) <= 40) b = fb;
        }
        if (b < 0) b = customerDoor(g, si, c, px, py, h, r.home >= 0 ? resBldg(g, si, c, r.home) : -1, 26, a.id);
        if (b < 0 || !doorNear(g, b, gx, gy, a.id)) return false;
        A.walkPost = Posture::None;
        headTo(g, a, A, gx, gy);
        return true;
      }
      case TK_SHOP: {   // a round of the stalls
        const RT::Spot* s = pickNear(g, S.market, h, px, py, 2, 16, a.id);
        if (!s && A.leg == 0) s = pick(g, S.market, h, a.id);
        if (!s) return false;
        A.walkPost = Posture::None;
        headTo(g, a, A, s->x, s->y);
        return true;
      }
      default: {   // a plain spot of the plan's kind, somewhere new (never the spot they stand on: 3 tiles off at least)
        int face = 0;
        const uint32_t salt = (uint32_t)A.seg * 2u + 1u + (uint32_t)A.leg * 7919u;
        // (the fields: on along their own furrows, the next woodpile, the next stretch of bank: never across the land)
        const bool lunch = fieldLunch(g, r, p);   // (the midday bread and cheese is eaten out at the field: its spots)
        if ((p.place == life::Place::Field || lunch) && A.arrived) {
          life::Plan fp = p;
          if (lunch) { fp.act = life::Act::Work; fp.place = life::Place::Field; fp.bldg = -1; }
          if (const RT::Spot* s = pickNear(g, spotsFor(S, r, fp), h, px, py, 3, 12, a.id)) {
            A.walkPost = Posture::None;
            headTo(g, a, A, s->x, s->y);
            return true;
          }
        }
        if (!outdoorSpot(g, si, S, c, r, p, gx, gy, face, a.id, salt)) return false;
        for (uint32_t k = 1; k < 4 && std::abs(gx - px) + std::abs(gy - py) < 3; k++) {
          int32_t x2, y2;
          int f2 = 0;
          if (outdoorSpot(g, si, S, c, r, p, x2, y2, f2, a.id, salt + k * 0x5151u) && std::abs(x2 - px) + std::abs(y2 - py) >= 3) { gx = x2; gy = y2; }
        }
        const int far = std::abs(gx - px) + std::abs(gy - py);
        if (far > 36) {   // (a capital's square, a city's woods, are a long way across: somewhere of the kind nearer)
          const std::vector<RT::Spot>& v = spotsFor(S, r, fieldAware(g, r, p));
          if (const RT::Spot* s = pickNear(g, v, h ^ 0x4EA7u, px, py, 3, 30, a.id)) { gx = s->x; gy = s->y; }
        }
        if (std::abs(gx - px) + std::abs(gy - py) < 3 && p.place != life::Place::Field) {
          // (their own lane's spot is the nearest to the door whatever the hash: a few steps along it instead)
          const bool lane = p.place == life::Place::Street || p.place == life::Place::Home || p.place == life::Place::Work;
          const RT::Spot* s = pickNear(g, lane ? S.street : S.plaza, h, px, py, 3, 10, a.id);
          if (!s) s = pickNear(g, lane ? S.plaza : S.street, h, px, py, 3, 12, a.id);
          if (s) { gx = s->x; gy = s->y; }
        }
        A.walkPost = Posture::None;
        headTo(g, a, A, gx, gy);
        return true;
      }
    }
  }
  // the cry of a stall's wares
  static std::string wares(const life::Resident& r, uint32_t h) {
    using life::Job;
    static const char* merchant[] = {"FAIR PRICES, FRIEND!", "COME AND SEE, COME AND SEE!", "THE BEST IN TOWN!", "FRESH IN TODAY!"};
    static const char* baker[] = {"HOT BREAD! STILL WARM!", "FRESH LOAVES! HONEY CAKES!", "BREAD! GOOD BREAD!"};
    static const char* tailor[] = {"FINE CLOTH! GOOD WOOL!", "CLOAKS FOR THE COLD!", "MENDING DONE, CHEAP!"};
    static const char* smith[] = {"NAILS, HINGES, KNIVES!", "GOOD IRON, FAIR PRICE!"};
    switch (r.job) {
      case Job::Baker: return baker[h % 3u];
      case Job::Tailor: return tailor[h % 3u];
      case Job::Smith: return smith[h % 2u];
      default: return merchant[h % 4u];
    }
  }
  // got to the task's stop: the posture and the wait there
  static void taskArrive(Game& g, Actor& a, RT::ARt& A, const life::Resident& r) {
    (void)g;
    const uint32_t h = mixu((uint64_t)a.id * 31u + A.leg, (uint64_t)(uint32_t)A.seg);
    auto secs = [&](float lo, float hi) { return lo + (hi - lo) * (float)(h % 1000u) * 0.001f; };
    a.st = AState::Idle;
    a.postureT = 0;
    switch (A.task) {
      case TK_STALL:
        a.posture = Posture::None;
        if (A.leg & 1) { a.face = 1; A.dwell = secs(2.0f, 3.5f); }   // at the store's door: the stock taken up
        else { a.face = 0; A.dwell = secs(16.0f, 38.0f); A.hawkT = secs(1.5f, 6.0f); }
        break;
      case TK_DELIVER:
        a.posture = Posture::None;
        a.face = 1;
        if (A.leg & 1) { A.dwell = secs(1.8f, 3.2f); bubble(a, (h >> 7) % 3u == 0 ? Bubble::Coin : Bubble::Talk, 1.4f); }   // handed over
        else A.dwell = secs(2.4f, 4.5f);
        break;
      case TK_SWEEP: a.posture = Posture::Sweep; a.face = (int)((h >> 10) % 4u); A.dwell = secs(14.0f, 28.0f); break;
      case TK_READ: a.posture = r.job == life::Job::Priest ? Posture::Pray : Posture::Read; a.face = 0; A.dwell = 1e6f; break;
      case TK_VISIT:
        a.posture = Posture::None; a.face = 1; A.dwell = secs(6.0f, 11.0f);
        bubble(a, (h >> 3) & 1u ? Bubble::Talk : Bubble::Question, 1.6f);
        break;
      case TK_SHOP:
        a.posture = Posture::None; a.face = 1; A.dwell = secs(5.0f, 12.0f);
        if ((h >> 5) & 1u) bubble(a, Bubble::Coin, 1.5f);
        break;
      default: break;
    }
  }
  // at the stop: the wait (a stall's cries), then the next leg
  static void taskTick(Game& g, Actor& a, RT::ARt& A, life::Census& c, const life::Resident& r, const life::Plan& p, RT::SiteRt& S, float dt) {
    RT& R = rt(g);
    a.st = AState::Idle;
    A.dwell -= dt;
    if (A.task == TK_STALL && !(A.leg & 1)) {
      A.hawkT -= dt;
      if (A.hawkT <= 0) {
        const uint32_t hh = mixu((uint64_t)a.id, (uint64_t)(g.time * 3.0f));
        A.hawkT = 8.0f + (float)(hh % 1000u) * 0.01f;
        a.posture = Posture::Wave; a.postureT = 0;
        if ((hh >> 12) & 1u) bubble(a, Bubble::Talk, 1.2f);
        if (R.barkT <= 0 && len2(g.pl().p - a.p) < (5.0f * TILE) * (5.0f * TILE) && onScreen(g, a.p)) {
          R.barkT = 7.0f;
          say(g, a, wares(r, hh));
        }
      } else if (a.posture == Posture::Wave && a.postureT > 1.1f) a.posture = Posture::None;
    }
    if (A.dwell > 0) return;
    A.leg++;
    if ((A.task == TK_SHOP && A.leg >= 3) || (A.task == TK_VISIT && A.leg >= 1)) { A.task = TK_NONE; A.leg = 0; }
    if (!startLeg(g, a, A, c, r, p, S)) {
      A.task = TK_NONE; A.leg = 0;
      if (!startLeg(g, a, A, c, r, p, S)) A.dwell = 4.0f;
    }
  }
  // the kind of spots a plan's place uses (outdoorSpot's lists, roughly): for the plain-sight fallback
  // a field hand's midday meal is out at the field (outdoorSpot's rule): the plan as the street's spots see it
  static life::Plan fieldAware(const Game& g, const life::Resident& r, const life::Plan& p) {
    life::Plan q = p;
    if (fieldLunch(g, r, p)) { q.act = life::Act::Work; q.place = life::Place::Field; q.bldg = -1; }
    return q;
  }
  static const std::vector<RT::Spot>& spotsFor(RT::SiteRt& S, const life::Resident& r, const life::Plan& p) {
    using life::Place;
    using life::Job;
    switch (p.place) {
      case Place::Field:
        switch (r.job) {
          case Job::Fisher: if (!S.water.empty()) return S.water; break;
          case Job::Woodcutter: if (!S.wood.empty()) return S.wood; break;
          case Job::Miner: if (!S.mine.empty()) return S.mine; break;
          case Job::Herder: if (!S.pasture.empty()) return S.pasture; break;
          default: break;
        }
        return !S.farm.empty() ? S.farm : S.street;
      case Place::Market: return !S.market.empty() ? S.market : S.plaza;
      case Place::Plaza: case Place::Gathering: case Place::Gathering2: case Place::Temple: return S.plaza;
      default: return S.street;
    }
  }
  // a spot of the plan's kind (else any street or square spot) 2..8 tiles off in plain sight: walked straight to
  static bool plainSight(Game& g, Actor& a, RT::ARt& A, RT::SiteRt& S, const life::Resident& r, const life::Plan& p) {
    const int32_t px = g.world.ox + tileX(a.p), py = g.world.oy + tileY(a.p);
    const uint32_t h = mixu((uint64_t)a.id * 7u + A.leg, (uint64_t)(uint32_t)A.seg);
    const std::vector<RT::Spot>* lists[3] = {&spotsFor(S, r, fieldAware(g, r, p)), &S.street, &S.plaza};
    for (const std::vector<RT::Spot>* v : lists) {
      for (int k = 0; k < 24; k++) {
        if (v->empty()) break;
        const RT::Spot& s = (*v)[(h + (uint32_t)k * 2654435761u) % v->size()];
        const int d = std::abs(s.x - px) + std::abs(s.y - py);
        if (d < 2 || d > 8 || !spotOk(g, s) || spotTaken(g, s.x, s.y, a.id)) continue;
        if (!clearLine(g, a.p, tileCentre(s.x - g.world.ox, s.y - g.world.oy), a.radius)) continue;
        A.task = TK_NONE;
        headTo(g, a, A, s.x, s.y);
        return true;
      }
    }
    return false;
  }
  // the nearest open tile (its centre free for a body) other than the one they are stuck in, within 3 tiles
  static bool slipOut(Game& g, const Actor& a, Vec2& out) {
    const Map& m = g.world.over;
    const int tx = tileX(a.p), ty = tileY(a.p);
    for (int rr = 1; rr <= 3; rr++)
      for (int oy = -rr; oy <= rr; oy++)
        for (int ox = -rr; ox <= rr; ox++) {
          if (std::max(std::abs(ox), std::abs(oy)) != rr) continue;
          const int x = tx + ox, y = ty + oy;
          if (!m.in(x, y) || m.blocked(x, y) || m.bldgAt[(size_t)y * m.w + x] >= 0 || m.wall[(size_t)y * m.w + x]) continue;
          const Vec2 c = tileCentre(x, y);
          if (!g.bodyFree(c, a.radius + 1.0f, false)) continue;
          out = c;
          return true;
        }
    return false;
  }
  // a fresh street's walker part-way along: a walkable tile 3..9 tiles from the target in plain sight of it
  // (from inside the town's own streets: someone bound for the fields is met coming out of town, never out of the wild)
  static bool midRoute(Game& g, const Actor& a, Vec2 tgt, uint32_t h, Vec2& out) {
    const Map& m = g.world.over;
    const IRect town = a.site >= 0 && a.site < (int)g.world.sites.size() ? g.world.sites[(size_t)a.site].r : IRect();
    const int tx = tileX(tgt), ty = tileY(tgt);
    for (int k = 0; k < 12; k++) {
      const uint32_t hk = mixu(h, (uint64_t)k + 0x3D1ull);
      const int dx = (int)(hk % 15u) - 7, dy = (int)((hk >> 5) % 11u) - 5;
      const int d = std::abs(dx) + std::abs(dy);
      if (d < 3 || d > 9) continue;
      const int x = tx + dx, y = ty + dy;
      if (!m.in(x, y) || !town.contains(x, y) || m.blocked(x, y) || m.bldgAt[(size_t)y * m.w + x] >= 0 || m.wall[(size_t)y * m.w + x]) continue;
      const Vec2 c = tileCentre(x, y);
      if (!g.bodyFree(c, a.radius, false) || !clearLine(g, c, tgt, a.radius)) continue;
      out = c;
      return true;
    }
    return false;
  }

  // ---------------------------------------------------------------- the street life of one resident actor
  static bool streetFolk(Game& g, Actor& a, float dt) {
    RT& R = rt(g);
    const int si = a.site;
    life::Census* c = si >= 0 ? g.life.findMut(g.world.sites[(size_t)si].id) : nullptr;
    if (!c || a.resident < 0 || a.resident >= (int)c->res.size()) { a.lifeBits |= LB_GONE; return true; }
    life::Resident& r = c->res[(size_t)a.resident];
    // (M7) something solid was put up where they stand (a notice board, a FOR SALE sign, a yard object): step off it
    if (a.posture == Posture::None && g.world.over.blocked(tileX(a.p), tileY(a.p))) { a.p = snapWalk(g, a.p); a.home = a.goal = a.p; }
    RT::ARt& A = R.rt(a.id);
    RT::SiteRt* S = siteRtById(g, c->site);
    if (!S || S->plans.size() != c->res.size()) return true;
    const life::Plan& p = S->plans[(size_t)a.resident];
    if (a.postureT >= 0) a.postureT += dt;
    if (a.bubbleT > 0 && (a.bubbleT -= dt) <= 0) a.bubble = Bubble::None;
    // (P1) held by a street incident (the robbed, a cart's owner, a brawler): incidentTick drives them
    if (A.mood == MOOD_INCIDENT) return incidentHeld(g, a, A, dt);
    if (A.mood == MOOD_PARTED) return partedHeld(g, a, A, dt);
    // the plan changed: a new target (time-sliced: 40 thinkers per step)
    if (A.kind != T_AWAY && (A.kind == T_NONE || A.act != p.act || A.place != p.place || A.pbldg != p.bldg) && R.thinkBudget > 0) {
      R.thinkBudget--;
      if (A.mood == 2 && A.partner >= 0) { auto it = R.act.find(A.partner); if (it != R.act.end()) it->second.partner = -1; }
      if (A.mood) A.partner = -1;
      A.mood = 0;
      retarget(g, a, A, *c, r, p);
    }
    switch (A.kind) {
      case T_DOOR: {
        a.posture = Posture::None;
        int dx, dy;
        if (!doorStep(g, A.bldg, dx, dy)) { a.lifeBits |= LB_GONE; return true; }
        // far off screen: they are simply indoors already
        if (!onScreen(g, a.p, 6.0f) && len2(a.p - g.pl().p) > (30.0f * TILE) * (30.0f * TILE)) { a.lifeBits |= LB_GONE; return true; }
        A.gx = g.world.ox + dx; A.gy = g.world.oy + dy;
        // husband and wife going home together: the one walks beside the other (a heart now and then)
        if (r.spouse >= 0 && r.spouse < (int)c->res.size() && c->res[(size_t)r.spouse].actor >= 0 && c->res[(size_t)r.spouse].actor < a.id) {
          const int k = g.findActor(c->res[(size_t)r.spouse].actor);
          auto it = k > 0 ? R.act.find(g.actors[(size_t)k].id) : R.act.end();
          if (k > 0 && it != R.act.end() && it->second.kind == T_DOOR && it->second.bldg == A.bldg &&
              len2(g.actors[(size_t)k].p - a.p) < (4.0f * TILE) * (4.0f * TILE) && len2(g.actors[(size_t)k].p - tileCentre(dx, dy)) > 12.0f * 12.0f) {
            const Actor& o = g.actors[(size_t)k];
            const Vec2 side = Vec2(-o.aim.y, o.aim.x) * 11.0f;
            const Vec2 to = o.p + side - o.aim * 3.0f;
            if (len2(to - a.p) > 4.0f) stepToward(g, a, to, a.speed * kWalk * 1.15f, dt);
            else { a.face = o.face; a.st = o.st; }
            if (a.bubbleT <= 0 && mixu(a.id, (uint64_t)(g.time / 8.0f)) % 5u == 0) bubble(a, Bubble::Heart, 1.2f);
            return true;
          }
        }
        if (walkOut(g, a, A, a.speed * kWalk, dt) || len2(a.p - tileCentre(dx, dy)) < 7.0f * 7.0f) {
          a.lifeBits |= LB_GONE;
          if (onScreen(g, a.p)) g.sfx((int)Sfx::Door, a.p, 1.1f, 0.25f);
          return true;
        }
        watch(g, a, A, dt);
        return true;
      }
      case T_AWAY: {   // an emigrant on the road out, a pack on the back
        a.posture = Posture::Carry;
        if (walkOut(g, a, A, a.speed * 0.55f, dt) || (!onScreen(g, a.p, 4.0f) && len2(a.p - g.pl().p) > (24.0f * TILE) * (24.0f * TILE))) a.lifeBits |= LB_GONE;
        watch(g, a, A, dt);
        return true;
      }
      case T_SPOT: break;
      default: a.st = AState::Idle; return true;
    }
    // (P1) wedged: the short step out (a walk, never a jump), then the next stop of the day
    if (A.slip) {
      const Vec2 d = A.slipTo - a.p;
      const float l = len(d);
      A.slipT += dt;
      if (l > 0.6f && A.slipT < 3.0f) {
        a.p += d * (std::min(l, a.speed * kWalk * 0.8f * dt) / l);
        a.face = faceOf(d); a.st = AState::Walk; a.posture = Posture::None;
        return true;
      }
      A.slip = false; A.path.clear(); A.fails = 0;
      A.leg++;
      if (!startLeg(g, a, A, *c, r, p, *S)) { A.arrived = true; settleOutdoor(g, a, A, *c, r); }
      return true;
    }
    // (P1) the street's rhythm: a new segment of the block, a new task (time-sliced with the plan changes)
    if (rhythmic(r, p, A) && A.act == p.act && A.place == p.place && A.retry != 255) {
      const int32_t seg = segOf(g, *c, r);
      if (A.seg != seg && R.thinkBudget > 0) {
        R.thinkBudget--;
        pickTask(g, a, A, *c, r, p, *S, seg);
      }
    }
    if (!A.arrived) {
      // (P1) everyone at their own pace (a little slower under a load)
      const float pace = ((p.act == life::Act::Play) ? 0.9f : kWalk) * A.pace * (A.task != TK_NONE && A.walkPost == Posture::Carry ? 0.93f : 1.0f);
      a.posture = p.act == life::Act::Play ? Posture::Play : (r.job == life::Job::Labourer && p.act == life::Act::Work ? Posture::Carry : Posture::None);
      if (A.task != TK_NONE) a.posture = A.walkPost;
      if (walkOut(g, a, A, a.speed * pace, dt)) {
        A.arrived = true;
        A.path.clear();
        A.watchT = 0;
        settleOutdoor(g, a, A, *c, r);
        if (A.task != TK_NONE) taskArrive(g, a, A, r);
      } else {
        watch(g, a, A, dt);
        if (A.fails >= 3) {   // no way there: another spot of the same kind (or stay put)
          A.fails = 0;
          if (A.task != TK_NONE) {   // (P1) a stop of the route out of reach: the next one
            A.arrived = true;
            A.dwell = 0;
            settleOutdoor(g, a, A, *c, r);
          } else if (rhythmic(r, p, A)) {   // (P1) a spot out of reach: another one (never the same unreachable spot again)
            // (twice in a row: one in plain sight, walked straight to without a search; none: they stay where they are)
            A.leg++;
            if (++A.retry >= 2 ? !plainSight(g, a, A, *S, r, p) : !startLeg(g, a, A, *c, r, p, *S)) {
              // nowhere to go from here: wedged on a corner or in a gap too tight for a body: a step out to the nearest
              // open tile (through the snag), then on with the day
              // (once a block; trapped for good, a yard fenced all round: they keep to their spot until the block ends)
              Vec2 to;
              if (A.retry >= 2 && !A.slipped && slipOut(g, a, to)) { A.slip = true; A.slipped = true; A.slipT = 0; A.slipTo = to; A.retry = 0; }
              else { A.retry = 255; A.task = TK_NONE; A.arrived = true; settleOutdoor(g, a, A, *c, r); }
            }
          } else {
            A.kind = T_NONE;
            A.arrived = true;
            settleOutdoor(g, a, A, *c, r);
          }
        }
      }
      if (!A.arrived) { greetings(g, a, *c, r); return true; }
    }
    // (P1) at a stop of the segment's task
    if (A.task != TK_NONE && A.mood == 0) {
      taskTick(g, a, A, *c, r, p, *S, dt);
      greetings(g, a, *c, r);
      return true;
    }
    // ---- at the spot: what they do there
    a.st = AState::Idle;
    A.t -= dt;
    if (A.mood == 1) {   // (stageMood) begging by the stalls
      a.posture = Posture::Beg;
      if (len2(g.pl().p - a.p) < (2.5f * TILE) * (2.5f * TILE) && R.barkT <= 0) {
        R.barkT = 9.0f;
        bubble(a, Bubble::Bread, 2.5f);
        say(g, a, "A CRUST, FRIEND? THE CHILDREN HAVEN'T EATEN.");
      }
      return true;
    }
    if (A.mood == 2) {   // (stageMood) a scuffle in the street
      if (A.partner < 0 || g.findActor(A.partner) <= 0) { A.mood = 0; A.partner = -1; }
      else { brawl(g, a, A, *c, dt); return true; }
    }
    using life::Act;
    switch (p.act) {
      case Act::Work:
        if (p.place == life::Place::Field && (r.job == life::Job::Farmer || r.job == life::Job::Herder) && A.t <= 0) {
          // along the furrow: a few tiles on to the next patch
          const std::vector<RT::Spot>& v = r.job == life::Job::Herder && !S->pasture.empty() ? S->pasture : S->farm;
          const RT::Spot* s = pick(g, v, mixu(c->seed ^ (uint64_t)r.idx, (uint64_t)(g.time * 0.1f)), a.id);
          if (s && std::abs(s->x - A.gx) + std::abs(s->y - A.gy) < 10) { A.gx = s->x; A.gy = s->y; A.arrived = false; A.path.clear(); claimSpot(g, A, a.id, s->x, s->y); }
          A.t = 14.0f + (float)(mixu(r.idx, (uint64_t)g.time) % 1600u) * 0.01f;
        } else if (r.job == life::Job::Labourer && A.t <= 0) {
          // (P1: a load carried a few lanes on, not across the whole city: a route that is walked, never a search that fails)
          const uint32_t lh = mixu(c->seed ^ (uint64_t)r.idx, (uint64_t)(g.time * 0.1f));
          const RT::Spot* s = pickNear(g, S->street, lh, g.world.ox + tileX(a.p), g.world.oy + tileY(a.p), 3, 14, a.id);
          if (!s) s = pick(g, S->street, lh, a.id);
          if (s) { A.gx = s->x; A.gy = s->y; A.arrived = false; A.path.clear(); claimSpot(g, A, a.id, s->x, s->y); }
          A.t = 6.0f;
        }
        break;
      case Act::Play:
        // a child sometimes tags along after a parent passing by (the rest play tag on the green)
        if (r.parent >= 0 && r.parent < (int)c->res.size() && c->res[(size_t)r.parent].actor >= 0 && (mixu(c->seed ^ r.idx, (uint64_t)g.day) % 3u) == 0) {
          const int k = g.findActor(c->res[(size_t)r.parent].actor);
          if (k > 0 && g.actors[(size_t)k].st == AState::Walk && len2(g.actors[(size_t)k].p - a.p) < (6.0f * TILE) * (6.0f * TILE)) {
            const Actor& o = g.actors[(size_t)k];
            const Vec2 to = o.p - o.aim * 13.0f + Vec2(-o.aim.y, o.aim.x) * 6.0f;
            a.posture = Posture::None;
            if (len2(to - a.p) > 9.0f) stepToward(g, a, to, a.speed * 0.95f, dt);
            break;
          }
        }
        playTag(g, a, A, *S, p.place);
        break;
      case Act::LightLamps: lampRound(g, a, A, *S, dt); break;
      case Act::Eat:
        if (fieldLunch(g, r, p)) { a.posture = Posture::Eat; break; }
        chatOrStroll(g, a, A, *c, r, *S, dt);
        break;
      case Act::Socialise: case Act::Tavern: case Act::Wander: case Act::Shop: case Act::Home: case Act::Pray: case Act::Bathe:
        chatOrStroll(g, a, A, *c, r, *S, dt);
        break;
      case Act::Beg:
        if (len2(g.pl().p - a.p) < (2.5f * TILE) * (2.5f * TILE) && R.barkT <= 0) {
          R.barkT = 9.0f;
          bubble(a, Bubble::Coin, 2.5f);
          say(g, a, "SPARE A COIN, FRIEND?");
        }
        break;
      case Act::Perform:
        if (a.bubbleT <= 0 && mixu(a.id, (uint64_t)(g.time * 2)) % 7 == 0) bubble(a, Bubble::Note, 1.4f);
        break;
      case Act::Brawl: brawl(g, a, A, *c, dt); break;
      default: break;
    }
    if (p.act != Act::Play) greetings(g, a, *c, r);
    // (P1) the street's talk as the player passes close: a grown-up at leisure (alone or in a pair) lets a line slip now
    // and then (the site's gossip clock: one line in 24 s at most), sometimes naming someone's need (overheard: a lead)
    if (r.job != life::Job::Child && r.job != life::Job::Beggar && A.mood == 0 && S->gossipT <= g.time &&
        (A.partner >= 0 || p.act == Act::Socialise || p.act == Act::Wander || p.act == Act::Shop || p.act == Act::Tavern || p.act == Act::Eat) &&
        len2(g.pl().p - a.p) < (3.0f * TILE) * (3.0f * TILE)) {
      gossip(g, a, *c, *S);
      bubble(a, Bubble::Talk, 1.6f);
    }
    // a word of need as the player passes (15.12: "HAVEN'T EATEN SINCE YESTERDAY"), now and then
    if (R.barkT <= 0 && r.job != life::Job::Child && r.job != life::Job::Beggar && len2(g.pl().p - a.p) < (2.2f * TILE) * (2.2f * TILE)) {
      R.barkT = 12.0f;
      const std::string b = shortBark(*c, r);
      if (!b.empty()) {
        a.face = faceOf(g.pl().p - a.p);
        bubble(a, r.need[(int)life::Need::Hunger] < 25 ? Bubble::Bread : (r.flags & life::RF_GRIEVING) ? Bubble::Tear : Bubble::Talk, 2.0f);
        say(g, a, b, rgba(232, 222, 196));
      }
    }
    return true;
  }
  // the stuck watchdog: a walker that moved less than a tile in 30 s
  static void watch(Game& g, Actor& a, RT::ARt& A, float dt) {
    A.watchT += dt;
    if (A.watchT >= 30.0f) {
      A.stuck = len2(a.p - A.watchP) < (float)(TILE * TILE);
      if (A.stuck) {
        // re-route; failing that, off screen they are where they were going
        A.path.clear();
        A.fails++;
        if (!onScreen(g, a.p)) {
          if (A.kind == T_DOOR || A.kind == T_AWAY) a.lifeBits |= LB_GONE;
          else { a.p = snapWalk(g, tileCentre(A.gx - g.world.ox, A.gy - g.world.oy)); A.stuck = false; }
        } else {
          // in sight: off the corner it is caught on (the next route starts from a clean tile; the spot walkers
          // give up after three failed routes)
          unwedge(g, a);
        }
      }
      A.watchT = 0;
      A.watchP = a.p;
    }
  }
  static void retarget(Game& g, Actor& a, RT::ARt& A, life::Census& c, const life::Resident& r, const life::Plan& p) {
    const int si = a.site;
    RT::SiteRt& S = siteRt(g, si);
    A.act = p.act; A.place = p.place; A.pbldg = p.bldg;
    A.path.clear(); A.pi = 0; A.fails = 0; A.arrived = false; A.want = false;
    breakPair(g, A, a.id);
    A.task = TK_NONE; A.seg = INT32_MIN; A.leg = 0; A.retry = 0; A.slipped = false; A.slip = false;   // (P1: the new block's first segment picks its task at once)
    a.posture = Posture::None;
    int32_t gx, gy;
    int face;
    if (p.act == life::Act::Emigrate && !S.edge.empty()) {
      const RT::Spot* s = pick(g, S.edge, mixu(c.seed, (uint64_t)r.idx));
      if (s) { A.kind = T_AWAY; A.gx = s->x; A.gy = s->y; return; }
    }
    if (targetSpot(g, si, S, c, r, p, gx, gy, face, a.id)) {
      A.kind = T_SPOT; A.gx = gx; A.gy = gy;
      claimSpot(g, A, a.id, gx, gy);
      return;
    }
    releaseSpot(g, A, a.id);
    // indoors: walk to the door and go in
    const Site& st = g.world.sites[(size_t)si];
    int b = p.bldg >= 0 ? c.bldgHandle(st, p.bldg) : -1;
    if (b < 0 && r.home >= 0) b = c.bldgHandle(st, r.home);
    int dx, dy;
    if (b >= 0 && doorStep(g, b, dx, dy)) { A.kind = T_DOOR; A.bldg = b; A.gx = g.world.ox + dx; A.gy = g.world.oy + dy; return; }
    // no door to go to (a burned house): off along the street
    const RT::Spot* s = pick(g, S.edge.empty() ? S.street : S.edge, mixu(c.seed, (uint64_t)r.idx + 5u));
    if (s) { A.kind = T_AWAY; A.gx = s->x; A.gy = s->y; return; }
    A.kind = T_NONE;
    a.lifeBits |= LB_GONE;
  }

  // children at tag on the green: run from spot to spot, the one who is "it" after the nearest
  // children at play: each child's game changes every half minute or so: tag (short dashes between nearby spots, the
  // run frames while moving, a breath stood still between dashes), knucklebones sat on the ground, hopscotch (the
  // hopping step on the spot). Never the whole lot frozen with their arms up.
  static void playTag(Game& g, Actor& a, RT::ARt& A, RT::SiteRt& S, life::Place place) {
    const std::vector<RT::Spot>& v = place == life::Place::Plaza || S.street.empty() ? S.plaza : S.street;
    const uint32_t game = mixu((uint64_t)a.id * 7u + 3u, (uint64_t)(g.time / 28.0f)) % 5u;
    if (game == 3) { a.posture = Posture::SitFloor; return; }   // knucklebones / marbles in the dust
    if (game == 4) { a.posture = Posture::Dance; return; }      // hopscotch
    a.posture = Posture::None;                                  // tag: catching a breath between dashes
    if (A.t > 0) return;
    for (int k = 0; k < 4; k++) {
      // (P1: a dash to a spot near, not a hashed one anywhere in town that is never near enough to run to)
      const RT::Spot* s = pickNear(g, v, mixu((uint64_t)a.id * 131u + (uint64_t)k, (uint64_t)(g.time * 3.0f)), A.gx, A.gy, 2, 6, a.id);
      if (s && (s->x != A.gx || s->y != A.gy) && std::abs(s->x - A.gx) + std::abs(s->y - A.gy) < 7) {
        A.gx = s->x; A.gy = s->y; A.arrived = false; A.path.clear(); claimSpot(g, A, a.id, s->x, s->y);
        break;
      }
    }
    A.t = 0.8f + (float)(mixu(a.id, (uint64_t)(g.time * 5.0f)) % 220u) * 0.01f;
    // a near miss: a shriek
    for (const Actor& o : g.actors)
      if (o.id != a.id && o.resident >= 0 && o.st == AState::Walk && o.posture == Posture::Play && len2(o.p - a.p) < 14.0f * 14.0f) { bubble(a, Bubble::Exclaim, 0.8f); break; }
  }
  // the lamplighter walks the round: each lamp is lit at its hour (life::lampLightHour), put out at dawn
  static void lampRound(Game& g, Actor& a, RT::ARt& A, RT::SiteRt& S, float dt) {
    (void)dt;
    if (S.lamps.empty()) return;
    if (A.t > 0) { a.posture = Posture::Lamp; return; }
    a.posture = Posture::None;
    // the next lamp due (not lit yet by the clock, or the nearest when it is still too early)
    int best = -1;
    float bh = 1e9f;
    const bool dusk = g.hour >= 12.0f;
    for (size_t i = 0; i < S.lamps.size(); i++) {
      const RT::Spot& s = S.lamps[i];
      const float lh = dusk ? life::lampLightHour(s.x, s.y - 1) : life::lampOutHour(s.x, s.y - 1);
      if ((int)i == A.lampIdx || lh < g.hour - 0.25f) continue;
      if (lh < bh) { bh = lh; best = (int)i; }
    }
    if (best < 0) return;
    A.lampIdx = best;
    A.gx = S.lamps[(size_t)best].x; A.gy = S.lamps[(size_t)best].y;
    claimSpot(g, A, a.id, A.gx, A.gy);
    A.arrived = false; A.path.clear();
    A.t = 2.5f;   // the pole raised once there (settleOutdoor sets the posture)
  }
  // chatting in pairs (facing each other, bubbles), else a slow stroll round the spot
  static void chatOrStroll(Game& g, Actor& a, RT::ARt& A, life::Census& c, const life::Resident& r, RT::SiteRt& S, float dt) {
    RT& R = rt(g);
    (void)dt;
    if (A.partner >= 0) {
      const int k = g.findActor(A.partner);
      if (k <= 0 || len2(g.actors[(size_t)k].p - a.p) > (2.2f * TILE) * (2.2f * TILE)) A.partner = -1;
      else {
        Actor& o = g.actors[(size_t)k];
        if (A.pairStep) {   // (P1 fixer r2) a step or two into place beside them (never a hop)
          A.pairT += dt;
          if (len2(A.dest - a.p) > 1.0f && A.pairT < 2.5f) { stepToward(g, a, A.dest, a.speed * kWalk * 0.8f, dt); return; }
          A.pairStep = false;
        }
        a.face = faceOf(o.p - a.p);
        a.st = AState::Idle;
        if (A.t <= 0) {
          A.t = 2.4f + (float)(mixu(a.id, (uint64_t)(g.time * 4)) % 200u) * 0.01f;
          static const Bubble chat[] = {Bubble::Talk, Bubble::Talk, Bubble::Exclaim, Bubble::Question, Bubble::Talk, Bubble::Heart};
          Bubble b = chat[mixu(a.id ^ o.id, (uint64_t)(g.time * 2)) % 6u];
          if ((r.flags & life::RF_GRIEVING)) b = Bubble::Tear;
          if (b == Bubble::Heart && (r.spouse < 0 || o.resident != r.spouse)) b = Bubble::Talk;
          if (r.need[(int)life::Need::Hunger] < 25 && (A.t > 3.3f)) b = Bubble::Bread;
          bubble(a, b, 1.6f);
          gossip(g, a, c, S);
        }
        return;
      }
    }
    if (A.t > 0) return;
    A.t = 5.0f + (float)(mixu(a.id, (uint64_t)(g.time * 3)) % 600u) * 0.01f;
    // a partner: another resident actor at leisure nearby (a friend first)
    int best = -1;
    float bd = (4.0f * TILE) * (4.0f * TILE);
    for (size_t k = 1; k < g.actors.size(); k++) {
      Actor& o = g.actors[k];
      if (o.id == a.id || !(o.lifeBits & LB_LIFE) || o.site != a.site || o.st == AState::Dead || o.st == AState::Walk) continue;
      auto it = R.act.find(o.id);
      if (it == R.act.end() || !it->second.arrived || it->second.partner >= 0) continue;
      const life::Act oa = it->second.act;
      if (!(oa == life::Act::Socialise || oa == life::Act::Tavern || oa == life::Act::Wander || oa == life::Act::Shop || oa == life::Act::Eat)) continue;
      float d = len2(o.p - a.p);
      for (int f : g.life.friendsOf(c, r.idx)) if (f == o.resident) d *= 0.5f;
      if (d < bd) { bd = d; best = (int)k; }
    }
    if (best > 0) {
      Actor& o = g.actors[(size_t)best];
      RT::ARt& B = R.rt(o.id);
      // (P1 fixer r2) side by side, facing: west and east of the middle, 9 px each way, walked to. (They were put there
      // in one jump, along the line between them: a pair met north-south stood in one column, a head painted over the
      // other's body, and with a wall at one side one of them stayed where they were, the two in one heap)
      Vec2 mid = (a.p + o.p) * 0.5f;
      const Vec2 dir(o.p.x >= a.p.x ? 1.0f : -1.0f, 0.0f);
      bool room = false;
      for (float sx : {0.0f, 6.0f, -6.0f, 12.0f, -12.0f}) {
        const Vec2 m2 = mid + Vec2(sx, 0);
        if (g.bodyFree(m2 - dir * 9.0f, a.radius, false) && g.bodyFree(m2 + dir * 9.0f, o.radius, false)) { mid = m2; room = true; break; }
      }
      if (!room) return;   // (no room for two here: no chat this time)
      A.partner = o.id; B.partner = a.id;
      A.dest = mid - dir * 9.0f; B.dest = mid + dir * 9.0f;
      A.pairStep = B.pairStep = true; A.pairT = B.pairT = 0;
      A.t = 0.2f; B.t = 1.4f;
      return;
    }
    // a stroll round the spot
    const std::vector<RT::Spot>& v = (A.place == life::Place::Market && !S.market.empty()) ? S.market : (S.plaza.empty() ? S.street : S.plaza);
    const RT::Spot* s = pick(g, v, mixu((uint64_t)a.id * 977u, (uint64_t)(g.time * 0.5f)), a.id);
    if (s && std::abs(s->x - A.gx) + std::abs(s->y - A.gy) < 12) { A.gx = s->x; A.gy = s->y; A.arrived = false; A.path.clear(); claimSpot(g, A, a.id, s->x, s->y); }
  }
  // two neighbours at it in the street (MF_BRAWLS): fists, an angry bubble, until the watch comes or they tire
  static void brawl(Game& g, Actor& a, RT::ARt& A, life::Census& c, float dt) {
    (void)c;
    if (A.partner < 0) return;
    const int k = g.findActor(A.partner);
    if (k <= 0) { A.partner = -1; return; }
    Actor& o = g.actors[(size_t)k];
    a.face = faceOf(o.p - a.p);
    A.t2 += dt;
    if (a.st == AState::Idle && fmodf(A.t2, 1.4f) < dt) { a.st = AState::Windup; a.stT = 0; }
    if (a.st == AState::Windup && a.stT > 0.3f) {
      a.st = AState::Strike; a.stT = 0; g.sfx((int)Sfx::Hit, a.p, 1.3f, 0.3f);
      // (P1 fixer) a scuffle reads at a glance: a kick of dust between them on every blow
      if (len2(o.p - a.p) < (1.6f * TILE) * (1.6f * TILE)) g.emit(Ev::Dust, (a.p + o.p) * 0.5f + Vec2(0, 1), 0, 0, "");
    }
    if (a.st == AState::Strike && a.stT > 0.25f) a.st = AState::Idle;
    if (a.bubbleT <= 0) bubble(a, Bubble::Anger, 1.2f);
  }

  // gossip near the player: a pair chatting within a few tiles lets a line slip (a real rumour now and then)
  static void gossip(Game& g, Actor& a, life::Census& c, RT::SiteRt& S) {
    if (len2(g.pl().p - a.p) > (4.0f * TILE) * (4.0f * TILE) || S.gossipT > g.time) return;
    S.gossipT = g.time + 24.0f;
    const uint32_t h = mixu(c.seed ^ (uint64_t)a.id, (uint64_t)g.day * 24u + (uint64_t)g.hour);
    rt(g).street.gossipLines++;
    // (P1) overheard: now and then the talk is of someone in town who needs help (a journal lead); close by only
    if (h % 3 == 1 && len2(g.pl().p - a.p) < (3.5f * TILE) * (3.5f * TILE) && overheard(g, a, rt(g).rt(a.id).partner, false)) return;
    std::string line;
    if (h % 3 == 0) {
      // (the places the session already holds near the window: nothing is loaded for a line of gossip)
      const int tx = tileX(a.p), ty = tileY(a.p);
      int rs = -1;
      float bd = 160.0f * 160.0f;
      for (int si : g.world.nearSites) {
        if (si < 0 || si >= (int)g.world.sites.size()) continue;
        const Site& s = g.world.sites[(size_t)si];
        if (s.discovered || s.rumoured || !(s.type == SiteType::Cave || s.type == SiteType::Ruin || s.type == SiteType::BanditCamp)) continue;
        const float d = (float)((s.ex - tx) * (s.ex - tx) + (s.ey - ty) * (s.ey - ty));
        if (d > 14.0f * 14.0f && d < bd) { bd = d; rs = si; }
      }
      if (rs >= 0) {
        const Site& s = g.world.sites[(size_t)rs];
        const int32_t gx = g.world.ox + tx, gy = g.world.oy + ty;
        line = "...THEY SAY THERE'S " + std::string(s.type == SiteType::Cave ? "A CAVE" : s.type == SiteType::Ruin ? "AN OLD RUIN" : s.type == SiteType::BanditCamp ? "A BANDIT CAMP" : "SOMETHING ODD") +
               " OFF TO THE " + dirWord(g.world.ox + s.ex - gx, g.world.oy + s.ey - gy, false) + "...";
      }
    }
    if (line.empty()) {
      const uint16_t mf = c.moodFlags;
      static const char* content[] = {"...BEST HARVEST IN YEARS...", "...AND THEN HE FELL IN THE TROUGH...", "...THE BARD'S NEW SONG...",
                                      "...MARRIED BY SPRING, YOU'LL SEE...", "...WOLVES OUT BY THE OLD MILL..."};
      static const char* hungry[] = {"...BREAD'S DEAR AS SILVER NOW...", "...THE STORES ARE NEARLY EMPTY...", "...MY BOYS GO TO BED HUNGRY..."};
      static const char* war[] = {"...THE SOLDIERS TOOK OUR CART...", "...WILL THEY COME BACK, YOU THINK?...", "...MY BROTHER WENT TO THE WAR..."};
      if (mf & (life::MF_FAMINE | life::MF_HUNGRY)) line = hungry[h % 3];
      else if (mf & life::MF_WARTORN) line = war[h % 3];
      else line = content[h % 5];
    }
    say(g, a, line, rgba(210, 214, 230));
  }

  // friends greet each other by name when they pass ("EVENING, HALLA")
  static void greetings(Game& g, Actor& a, life::Census& c, const life::Resident& r) {
    RT& R = rt(g);
    RT::ARt& A = R.rt(a.id);
    if (A.greetT > g.time || r.job == life::Job::Child) return;
    A.greetT = g.time + 1.5f;
    for (int f : g.life.friendsOf(c, r.idx)) {
      if (f < 0 || f >= (int)c.res.size()) continue;
      const int oid = c.res[(size_t)f].actor;
      if (oid < 0) continue;
      const int k = g.findActor(oid);
      if (k <= 0) continue;
      Actor& o = g.actors[(size_t)k];
      if (len2(o.p - a.p) > (2.5f * TILE) * (2.5f * TILE)) continue;
      // once a day per pair (marks: no; the runtime keeps it)
      const uint64_t key = ((uint64_t)(uint32_t)std::min(r.idx, (uint16_t)f) << 32 | (uint32_t)std::max((int)r.idx, f)) ^ ((uint64_t)c.site * 31u) ^ ((uint64_t)g.day << 48);
      if (R.greeted.count(key)) continue;
      RT::ARt& B = R.rt(o.id);
      if (B.greetT > g.time + 6.0f) continue;   // just greeted by someone else: one greeting at a time
      if (R.greeted.size() > 4096) R.greeted.clear();
      R.greeted.insert(key);
      B.greetT = g.time + 8.0f;
      a.face = faceOf(o.p - a.p);
      bubble(a, Bubble::Exclaim, 1.2f);
      a.posture = a.posture == Posture::None ? Posture::Wave : a.posture;
      a.postureT = 0;
      if (onScreen(g, a.p)) say(g, a, std::string(timeGreeting(g.hour)) + ", " + firstName(c.res[(size_t)f].name));
      return;
    }
  }

  // ---------------------------------------------------------------- interiors
  static void scanInterior(Game& g) {
    RT& R = rt(g);
    RT::Interior& I = R.in;
    I = RT::Interior();
    I.key = g.mapKey();
    const Map& m = g.sub;
    auto room = [&](int x, int y) { return m.roomIndexAt(x, y); };
    static const int dx[4] = {0, 0, 1, -1}, dy[4] = {1, -1, 0, 0};
    // ((M7c fixer) the floor the player can walk to from the way in (the door, the stairs): a worker's station tile
    // that the furniture walls off (seed 7's start smithy: the anvil's side between the quench tub, a barrel, the
    // bench and the wall) is never a post, or the smith stands where nobody can reach her to talk or trade)
    std::vector<uint8_t> reach((size_t)m.w * m.h, 0);
    {
      std::vector<int> q;
      auto seedAt = [&](int x, int y) {
        if (!m.in(x, y) || reach[(size_t)y * m.w + x]) return;
        reach[(size_t)y * m.w + x] = 1;
        q.push_back(y * m.w + x);
      };
      if (m.exitY >= 0) {
        seedAt(m.exitX, m.exitY);
        for (int16_t e : m.exits) seedAt(e, m.exitY);
      }
      for (const Stairs* st : {&m.up, &m.down})
        if (st->valid())
          for (int k = 0; k < 4; k++)
            if (m.in(st->x + dx[k], st->y + dy[k]) && !m.blocked(st->x + dx[k], st->y + dy[k])) seedAt(st->x + dx[k], st->y + dy[k]);
      if (q.empty()) std::fill(reach.begin(), reach.end(), (uint8_t)1);   // (no way in known: no filter)
      for (size_t h = 0; h < q.size(); h++) {
        const int x = q[h] % m.w, y = q[h] / m.w;
        for (int k = 0; k < 4; k++) {
          const int nx = x + dx[k], ny = y + dy[k];
          if (m.in(nx, ny) && !m.blocked(nx, ny)) seedAt(nx, ny);
        }
      }
    }
    for (int y = 1; y < m.h - 1; y++)
      for (int x = 1; x < m.w - 1; x++) {
        const int pr = m.propAt(x, y);
        if (!pr) {
          // a bath's pool (the water of a ground floor inside): a bather may stand in it (never at its rim row: the
          // coping is drawn there), nobody else
          if (groundWater(m.at(x, y))) {
            if (m.floor == 0 && groundWater(m.at(x, y - 1))) I.poolTiles.push_back(y * m.w + x);
            continue;
          }
          if (!m.blocked(x, y) && !m.isExit(x, y)) I.floorTiles.push_back(y * m.w + x);
          continue;
        }
        const Prop p = (Prop)(pr - 1);
        if (seatProp(p)) {
          // facing the table beside it (a pew faces the altar: north)
          int face = -1;
          for (int k = 0; k < 4; k++) {
            const int q = m.propAt(x + dx[k], y + dy[k]);
            if (q && tableProp((Prop)(q - 1))) { face = k == 0 ? 0 : k == 1 ? 1 : k == 2 ? 2 : 3; break; }
          }
          // (M5 fixer r2) no table: a pew faces the altar (north); a bench by a bath faces its pool; any other seat faces
          // the middle of its room (a bench against the north wall faces down into the room, never into a bare wall)
          if (face < 0) {
            const int ri = room(x, y);
            const RoomKind rk = roomKind(m, ri);
            if (rk == RoomKind::Nave) face = 1;
            else {
              float tx = -1, ty = -1, bd = 1e9f;
              for (int yy = std::max(0, y - 6); yy <= std::min(m.h - 1, y + 6); yy++)
                for (int xx = std::max(0, x - 6); xx <= std::min(m.w - 1, x + 6); xx++)
                  if (groundWater(m.at(xx, yy))) {
                    const float d = (float)((xx - x) * (xx - x) + (yy - y) * (yy - y));
                    if (d < bd) { bd = d; tx = (float)xx; ty = (float)yy; }
                  }
              if (tx < 0 && ri >= 0 && ri < (int)m.rooms.size()) {
                const IRect& rr = m.rooms[(size_t)ri].r;
                tx = rr.x + (rr.w - 1) * 0.5f; ty = rr.y + (rr.h - 1) * 0.5f;
              }
              if (tx < 0) face = 0;
              else {
                const float ddx = tx - x, ddy = ty - y;
                if (std::fabs(ddx) > std::fabs(ddy) + 0.5f) face = ddx > 0 ? 2 : 3;
                else face = ddy < -0.5f ? 1 : 0;
              }
            }
          }
          I.seats.push_back({x, y, p, room(x, y), (int8_t)face});
        } else if (bedProp(p)) {
          I.beds.push_back({x, y, p, room(x, y), 0});
        } else if (p == Prop::Anvil || p == Prop::Forge || p == Prop::Oven || p == Prop::Cauldron || p == Prop::Loom || p == Prop::SpinningWheel ||
                   p == Prop::Lectern || p == Prop::Desk || p == Prop::Altar || p == Prop::Workbench || p == Prop::Grindstone || p == Prop::PrepTable ||
                   p == Prop::Bookshelf || p == Prop::Hearth || counterProp(p) || p == Prop::Stove || p == Prop::FirePitM) {
          // the standing tile: free floor beside it, the worker facing it (below it first: the 3/4 view shows the work)
          // (a free-standing piece, the anvil or a wheel, is worked from its side so the piece and the work both show)
          static const int pref[4][3] = {{0, 1, 1}, {1, 0, 3}, {-1, 0, 2}, {0, -1, 0}};
          static const int side[4][3] = {{-1, 0, 2}, {1, 0, 3}, {0, 1, 1}, {0, -1, 0}};
          const bool freeStanding = p == Prop::Anvil || p == Prop::Grindstone || p == Prop::SpinningWheel || p == Prop::Cauldron;
          for (int k = 0; k < 4; k++) {
            const int (&pk)[3] = freeStanding ? side[k] : pref[k];
            const int sx = x + pk[0], sy = y + pk[1];
            if (!m.in(sx, sy) || m.blocked(sx, sy) || m.propAt(sx, sy) || m.isExit(sx, sy)) continue;
            if (!reach[(size_t)sy * m.w + sx]) continue;   // (walled off by the furniture: no post there)
            // a bar is worked from behind (its north side), never from the customers' side
            if (counterProp(p) && pk[1] != -1) continue;
            I.stations.push_back({sx, sy, p, room(sx, sy), (int8_t)pk[2]});
            // (a free-standing piece keeps every free side, best first: when one is taken the worker still stands at
            //  the piece, never at the next prop in the room)
            if (!freeStanding) break;
          }
        }
      }
    // floor seats: the open floor round a low table, a long fire trench or a yurt's stove is sat on cross-legged (a
    // feast tent, a tea room without cushions)
    for (int y = 1; y < m.h - 1; y++)
      for (int x = 1; x < m.w - 1; x++) {
        const int pr = m.propAt(x, y);
        if (!pr) continue;
        const Prop p = (Prop)(pr - 1);
        if (!(p == Prop::LowTable || p == Prop::FirePitL || p == Prop::FirePitM || p == Prop::FirePitR || p == Prop::Stove)) continue;
        for (int k = 0; k < 4; k++) {
          const int sx = x + dx[k], sy = y + dy[k];
          if (!m.in(sx, sy) || m.blocked(sx, sy) || m.propAt(sx, sy) || m.isExit(sx, sy)) continue;
          // (never behind a fire or a stove: in the 3/4 view the flames and the flue would stand in front of the sitter)
          // (a long hearth's fires burn at its segments' joins, between the places: its far side is sat at too, the
          //  patrons facing the room across the embers)
          if (k == 1 && p != Prop::LowTable && p != Prop::FirePitL && p != Prop::FirePitM && p != Prop::FirePitR) continue;
          bool dup = false;
          for (const RT::Furn& s : I.seats) if (s.x == sx && s.y == sy) dup = true;
          if (dup) continue;
          // facing the table / the fire (k: the seat lies below, above, right, left of it)
          const int face = k == 0 ? 1 : k == 1 ? 0 : k == 2 ? 3 : 2;
          I.seats.push_back({sx, sy, Prop::COUNT, room(sx, sy), (int8_t)face});
        }
      }
  }
  static RoomKind roomKind(const Map& m, int ri) { return ri >= 0 && ri < (int)m.rooms.size() ? m.rooms[(size_t)ri].kind : RoomKind::Hall; }
  static bool inRentedRoom(const Game& g, int x, int y) {
    return g.lodgingActive() && g.lodging.bldg == g.subBldg && g.lodging.floor == g.subFloor && g.sub.roomIndexAt(x, y) == g.lodging.room;
  }
  static bool tileFree(Game& g, int x, int y) {
    const Map& m = g.sub;
    if (!m.in(x, y) || m.blocked(x, y) || m.isExit(x, y) || inRentedRoom(g, x, y)) return false;
    const int pr = m.propAt(x, y);
    if (pr && ((Prop)(pr - 1) == Prop::StairsUp || (Prop)(pr - 1) == Prop::StairsDown || (Prop)(pr - 1) == Prop::DoorH || (Prop)(pr - 1) == Prop::DoorV)) return false;
    for (size_t k = 1; k < g.actors.size(); k++)
      if (tileX(g.actors[k].p) == x && tileY(g.actors[k].p) == y && g.actors[k].st != AState::Dead) return false;
    return true;
  }
  // a standing body here would be drawn through someone seated: the seat's own tile (a sitter's body is drawn off its
  // tile centre, so the actor scan alone misses it) or the tile straight above or below a taken seat
  static bool bySitter(Game& g, int x, int y) {
    for (const RT::Furn& s : rt(g).in.seats)
      if (s.x == x && std::abs(s.y - y) <= 1 && isUsed(g, s.x, s.y)) return true;
    return false;
  }
  // (M5 fixer r2) the bard keeps a clear ring: nobody sits or stands on a tile next to a performer (a body a step behind
  // the bard hid the lute and most of the player at 1x)
  static bool nearPerformer(const Game& g, const Actor& a, int x, int y) {
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& o = g.actors[k];
      if (o.id == a.id || o.st == AState::Dead) continue;
      if (o.posture != Posture::Lute && o.posture != Posture::Flute && o.posture != Posture::Drum) continue;
      if (std::abs(tileX(o.p) - x) <= 1 && std::abs(tileY(o.p) - y) <= 1) return true;
    }
    return false;
  }
  static void placeAt(Actor& a, int x, int y, int face) {
    a.p = tileCentre(x, y);
    a.home = a.p; a.goal = a.p;
    a.face = face;
    a.st = AState::Idle;
  }
  // sit an actor on a free seat (in rooms of the given kinds, near (nx, ny) when given); false: none free
  static bool sitDown(Game& g, Actor& a, std::initializer_list<RoomKind> kinds, Posture post, int nx = -1, int ny = -1) {
    RT& R = rt(g);
    int best = -1, bd = 1 << 30;
    for (size_t i = 0; i < R.in.seats.size(); i++) {
      const RT::Furn& s = R.in.seats[i];
      bool ok = kinds.size() == 0;
      for (RoomKind k : kinds) if (roomKind(g.sub, s.room) == k) ok = true;
      if (!ok || isUsed(g, s.x, s.y) || !tileFree(g, s.x, s.y) || nearPerformer(g, a, s.x, s.y)) continue;
      int d = nx >= 0 ? std::abs(s.x - nx) + std::abs(s.y - ny) : (int)(mixu((uint64_t)a.id, i) % 97u);
      // a chair is sat on facing its table (never into its back: art::seatFit); one that would face away goes last
      int ax, ay;
      if (!seatAt(s.p, s.face, ax, ay)) d += 1000;
      // a real seat first (a chair, a bench, a cushion), the bare floor by a fire last
      if (s.p == Prop::COUNT) d += 300;
      if (d < bd) { bd = d; best = (int)i; }
    }
    if (best < 0) return false;
    const RT::Furn& s = R.in.seats[(size_t)best];
    reserve(g, a, s.x, s.y);
    int face = s.face;
    int fax = 0, fay = 0;
    if (!seatAt(s.p, face, fax, fay)) { face = 0; seatAt(s.p, face, fax, fay); }
    // (M5 fixer r2) on the bench before a long hearth the patrons sit on its front edge, leaning in to the warmth: their
    // backs no longer hide the whole trench (the embers and the logs show over their shoulders, not just the flames)
    if (s.p == Prop::Bench && face == 1 && g.sub.in(s.x, s.y - 1)) {
      const int up = g.sub.propAt(s.x, s.y - 1);
      if (up == (int)Prop::FirePitL + 1 || up == (int)Prop::FirePitM + 1 || up == (int)Prop::FirePitR + 1) fay += 5;
    }
    placeAt(a, s.x, s.y, face);
    // the seat line on the seat's surface (art::seatFit: p from the seat tile's bottom-centre)
    a.p = Vec2(s.x * TILE + 8.0f + fax, s.y * TILE + 16.0f + fay);
    a.home = a.p; a.goal = a.p;
    a.posture = seatedPost(s.p, post);
    a.postureT = (float)(mixu(a.id, 7) % 100u) * 0.01f;
    return true;
  }
  static int kitOf(const Game& g) { return g.inside && g.sub.kit ? (int)g.sub.kit - 1 : -1; }
  static bool isSeated(Posture p) {
    return p == Posture::Sit || p == Posture::SitEat || p == Posture::SitDrink || p == Posture::SitFloor || p == Posture::SitFloorEat ||
           p == Posture::SitFloorDrink;
  }
  static bool standAt(Game& g, Actor& a, Prop want, Posture post, std::initializer_list<Prop> alt = {}) {
    RT& R = rt(g);
    for (int pass = 0; pass < 2; pass++)
      for (const RT::Furn& s : R.in.stations) {
        bool ok = pass == 0 ? s.p == want : false;
        if (pass == 1) for (Prop q : alt) if (s.p == q) ok = true;
        if (!ok || isUsed(g, s.x, s.y) || !tileFree(g, s.x, s.y)) continue;
        reserve(g, a, s.x, s.y);
        placeAt(a, s.x, s.y, s.face);
        a.posture = post;
        return true;
      }
    return false;
  }
  static bool lieDown(Game& g, Actor& a, int bedIdx) {
    RT& R = rt(g);
    if (bedIdx < 0 || bedIdx >= (int)R.in.beds.size()) return false;
    const RT::Furn& b = R.in.beds[(size_t)bedIdx];
    if (isUsed(g, b.x, b.y) || inRentedRoom(g, b.x, b.y)) return false;
    reserve(g, a, b.x, b.y);
    // the head on the pillow (art::bedFit: the generic Sleep cell's p from the berth's anchor tile bottom-centre; a
    // two-tile bed has its head piece (Filler) on the tile above)
    const bool longBed = g.sub.propAt(b.x, b.y - 1) == (int)Prop::Filler + 1;
    a.p = Vec2(b.x * TILE + 8.0f, b.y * TILE + 16.0f + (float)sleepAy(b.p, longBed, kitOf(g)));
    a.home = a.p; a.goal = a.p;
    a.face = 0;
    a.st = AState::Idle;
    a.posture = Posture::Sleep;
    a.postureT = 0;
    bubble(a, Bubble::Zzz, 2.0f + (float)(mixu(a.id, 3) % 300u) * 0.01f);
    return true;
  }
  static bool standFree(Game& g, Actor& a, std::initializer_list<RoomKind> kinds, int nx, int ny, int maxD = 99) {
    RT& R = rt(g);
    int best = -1, bd = 1 << 30;
    for (int t : R.in.floorTiles) {
      const int x = t % g.sub.w, y = t / g.sub.w;
      const int ri = g.sub.roomIndexAt(x, y);
      bool ok = kinds.size() == 0;
      for (RoomKind k : kinds) if (roomKind(g.sub, ri) == k) ok = true;
      if (!ok || !tileFree(g, x, y) || isUsed(g, x, y) || bySitter(g, x, y) || nearPerformer(g, a, x, y)) continue;
      // a body clear of the walls and furniture beside it
      if (!g.bodyFree(tileCentre(x, y), a.radius + 2.0f, false)) continue;
      if (nx >= 0 && std::abs(x - nx) + std::abs(y - ny) > maxD) continue;
      // near the anchor when there is one, else anywhere (by hash); never in a huddle (a body a step from another)
      int d = nx >= 0 ? (std::abs(x - nx) + std::abs(y - ny)) * 1000 + (int)(mixu((uint64_t)a.id, (uint64_t)t) % 1000u)
                      : (int)(mixu((uint64_t)a.id, (uint64_t)t) % 100000u);
      for (size_t k = 1; k < g.actors.size(); k++) {
        const Actor& o = g.actors[k];
        if (o.id == a.id || o.st == AState::Dead) continue;
        if (std::abs(tileX(o.p) - x) <= 1 && std::abs(tileY(o.p) - y) <= 1) d += 200000;
        // the keeper's counter stays clear for customers (the player talks to them from in front of it)
        if ((o.lifeBits & LB_KEY) && std::abs(tileX(o.p) - x) <= 2 && std::abs(tileY(o.p) - y) <= 3) d += 400000;
      }
      if (d < bd) { bd = d; best = t; }
    }
    if (best < 0) return false;
    placeAt(a, best % g.sub.w, best / g.sub.w, 0);
    return true;
  }

  // (M5 fixer r2) a bather: standing chest-deep in the pool (the view draws the body under the waterline), one to a
  // tile and never a step from another bather, facing the nearest bather or the room
  static bool bathe(Game& g, Actor& a) {
    RT& R = rt(g);
    int best = -1, bd = 1 << 30;
    for (int t : R.in.poolTiles) {
      const int x = t % g.sub.w, y = t / g.sub.w;
      if (isUsed(g, x, y)) continue;
      bool clear = true;
      for (size_t k = 1; k < g.actors.size() && clear; k++) {
        const Actor& o = g.actors[k];
        if (o.id == a.id || o.st == AState::Dead) continue;
        if (std::abs(tileX(o.p) - x) <= 1 && std::abs(tileY(o.p) - y) <= 1) clear = false;
      }
      if (!clear) continue;
      const int d = (int)(mixu((uint64_t)a.id, (uint64_t)t) % 1000u);
      if (d < bd) { bd = d; best = t; }
    }
    if (best < 0) return false;
    const int x = best % g.sub.w, y = best / g.sub.w;
    reserve(g, a, x, y);
    a.useX = -1; a.useY = -1;   // (not furniture: the view finds the water under the feet)
    placeAt(a, x, y, 0);
    float bdd = 1e9f;
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& o = g.actors[k];
      if (o.id == a.id || !o.npc || o.st == AState::Dead) continue;
      const float d = len2(o.p - a.p);
      if (d < bdd && d < (5.0f * TILE) * (5.0f * TILE)) { bdd = d; a.face = faceOf(o.p - a.p); }
    }
    a.posture = Posture::None;
    return true;
  }

  // stand a step beside another standing patron who has no one to talk to (the two face each other)
  static bool standBeside(Game& g, Actor& a) {
    static const int ox[4] = {1, -1, 0, 0}, oy[4] = {0, 0, 1, -1};
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& o = g.actors[k];
      if (o.id == a.id || !(o.lifeBits & LB_LIFE) || o.posture != Posture::Drink) continue;
      int mates = 0;
      for (size_t j = 1; j < g.actors.size(); j++)
        if (j != k && g.actors[j].id != a.id && len2(g.actors[j].p - o.p) < (1.6f * TILE) * (1.6f * TILE)) mates++;
      if (mates) continue;   // a pair already
      const int x0 = tileX(o.p), y0 = tileY(o.p);
      for (int d = 0; d < 4; d++) {
        const int x = x0 + ox[d], y = y0 + oy[d];
        if (!tileFree(g, x, y) || g.sub.propAt(x, y) || isUsed(g, x, y) || bySitter(g, x, y) || nearPerformer(g, a, x, y) ||
            !g.bodyFree(tileCentre(x, y), a.radius + 1.0f, false)) continue;
        placeAt(a, x, y, faceTo(x0 - x, y0 - y));
        return true;
      }
    }
    return false;
  }

  // beds of a home's floor, counted once per (building, floor) (the household is shared out over the floors in order)
  static int bedsOnFloor(Game& g, int bi, int floor) {
    RT& R = rt(g);
    const Bldg& B = g.world.over.bldgs[(size_t)bi];
    const uint64_t key = (B.id ? B.id : (uint64_t)B.seed) * 8u + (uint64_t)floor;
    auto it = R.bedsOnFloor.find(key);
    if (it != R.bedsOnFloor.end()) return it->second;
    int n = 0;
    if (floor == g.subFloor && bi == g.subBldg) {
      for (const RT::Furn& b : R.in.beds) n += roomHomeBed(roomKind(g.sub, b.room)) ? 1 : 0;
    } else {
      Map m;
      genInterior(m, B, B.seed, floor);
      for (int y = 0; y < m.h; y++)
        for (int x = 0; x < m.w; x++) {
          const int pr = m.propAt(x, y);
          if (pr && bedProp((Prop)(pr - 1)) && roomHomeBed(roomKind(m, m.roomIndexAt(x, y)))) n++;
        }
    }
    R.bedsOnFloor[key] = n;
    return n;
  }

  // the people of the building the player is in, by the clock (loadMapActors has made the generator's spawns; their
  // key person was kept or dropped by lifeSpawned)
  static void populate(Game& g) {
    RT& R = rt(g);
    // the generator spawns whose residents are elsewhere go
    for (size_t k = g.actors.size(); k-- > 1;)
      if (g.actors[k].lifeBits & LB_DROP) { g.actors.erase(g.actors.begin() + (std::ptrdiff_t)k); }
    // reservations of this map start over
    R.used.clear();
    scanInterior(g);
    const int bi = g.subBldg;
    if (bi < 0) return;
    const Bldg& B = g.world.over.bldgs[(size_t)bi];
    const int si = B.site;
    life::Census* c = censusOfSite(g, si);
    // a census just built has not had its first aggregate hour yet (its needs, and so its plans, move then): run it now
    if (c && c->lastHour != g.day * 24 + (int)g.hour) g.life.tick(g, 0.0f);
    // the key person and the travellers first (they hold their places)
    for (size_t k = 1; k < g.actors.size(); k++) {
      Actor& a = g.actors[k];
      if (a.lifeBits & LB_KEY) keyPost(g, a, c, B);
      else if (a.lifeBits & LB_GUEST) guestPlace(g, a);
    }
    if (!c || !g.world.sites[(size_t)si].settlement()) return;
    const Site& st = g.world.sites[(size_t)si];
    const int off = bi - st.bldgFirst;
    if (off < 0 || off >= st.bldgCount) return;
    if (warEmpty(g, si) || B.charred == 2) return;
    RT::SiteRt& S = siteRt(g, si);
    refreshPlans(g, S, *c);
    R.in.stamp = S.planStamp;
    const bool gatherHere = off == c->gathering || off == c->gathering2;
    // who is here this hour, in census order
    std::vector<int> here;
    for (size_t i = 0; i < c->res.size() && i < (size_t)LIFE_SLOTN; i++) {
      const life::Resident& r = c->res[i];
      if (r.flags & (life::RF_DEAD | life::RF_AWAY)) continue;
      if (r.job == life::Job::Guard) continue;
      if (workKey(r) && r.keyBldg == off && r.actor >= 0 && g.findActor(r.actor) > 0) continue;   // the keeper at their post (kept above)
      if (S.plans[i].bldg != off) continue;
      here.push_back((int)i);
    }
    // the staff and the bard take their places first (a full hall never leaves the bard outside)
    std::stable_partition(here.begin(), here.end(), [&](int i) {
      const life::Act a = S.plans[(size_t)i].act;
      return a == life::Act::Work || a == life::Act::Perform;
    });
    // sleepers are shared out over the floors' beds in census order (the household's own beds; the inn's staff in
    // the keeper's room, never a guest room)
    int sleepersBefore = 0;
    for (int f = 0; f < g.subFloor; f++) sleepersBefore += bedsOnFloor(g, bi, f);
    std::vector<int> myBeds;
    for (size_t b = 0; b < R.in.beds.size(); b++) if (roomHomeBed(roomKind(g.sub, R.in.beds[b].room))) myBeds.push_back((int)b);
    int sleeperN = 0, placed = 0;
    for (int idx : here) {
      const life::Resident& r = c->res[(size_t)idx];
      const life::Plan& p = S.plans[(size_t)idx];
      if (p.act == life::Act::Sleep) {
        const int n = sleeperN++;
        if (n < sleepersBefore || n - sleepersBefore >= (int)myBeds.size()) continue;   // asleep on another floor
        Actor& a = g.actors[(size_t)g.findActor(spawnResident(g, si, *c, idx, tileCentre(g.sub.exitX, g.sub.exitY - 1), true))];
        if (!lieDown(g, a, myBeds[(size_t)(n - sleepersBefore)])) { a.lifeBits |= LB_GONE; continue; }
        placed++;
        continue;
      }
      if (g.subFloor != 0) continue;   // the day's life is on the ground floor
      if (placed >= 18) break;
      const int id = spawnResident(g, si, *c, idx, tileCentre(g.sub.exitX, g.sub.exitY - 1), true);
      Actor& a = g.actors[(size_t)g.findActor(id)];
      if (!placeIndoors(g, a, *c, r, p, gatherHere)) { a.lifeBits |= LB_GONE; continue; }
      placed++;
    }
    // (fixer M5 r3, review: "village inns are near-empty in the evening while the mood is CONTENT") a content town's
    // gathering place fills of an evening even where few have it in their plans (a village's handful of regulars): its
    // free neighbours drop in for a drink (those at home or about, never the sleeping, the working or anyone out in the
    // street), and somebody takes up the culture's instrument when no bard is playing
    const bool evening = g.hour >= 18.0f && g.hour < 23.5f;
    const bool innHere = B.type == art::Building::Inn;   // (the inn is a gathering place too, whatever the culture's own)
    if ((gatherHere || innHere) && g.subFloor == 0 && evening && placed < 18 && (c->moodFlags & life::MF_CONTENT) &&
        !(c->moodFlags & (life::MF_HUNGRY | life::MF_FAMINE | life::MF_WARTORN | life::MF_EMIGRATING))) {
      int patrons = 0;
      bool bard = false;
      for (size_t k = 1; k < g.actors.size(); k++) {
        const Actor& o = g.actors[k];
        if (!(o.lifeBits & LB_LIFE) || (o.lifeBits & LB_GONE)) continue;
        auto it = R.act.find(o.id);
        if (it == R.act.end()) continue;
        const life::Act oa = it->second.act;
        if (oa == life::Act::Perform) bard = true;
        if (oa == life::Act::Tavern || oa == life::Act::Socialise || oa == life::Act::Eat || oa == life::Act::Brawl) patrons++;
      }
      const int target = std::clamp((int)R.in.seats.size() * 2 / 3, 6, 12);
      const int n = (int)c->res.size();
      const int start = n ? (int)(mixu(c->seed ^ 0xA1E5ull, (uint64_t)g.day) % (uint32_t)n) : 0;
      // (a quiet town abed early still has its night owls: when the free ones run short, the sociable who were
      //  for bed stay up a while longer, until 23:00)
      bool owls = false;
      auto freeTonight = [&](int i) {
        const life::Resident& r = c->res[(size_t)i];
        if (i >= LIFE_SLOTN || (r.flags & (life::RF_DEAD | life::RF_AWAY | life::RF_GRIEVING))) return false;
        if (r.job == life::Job::Guard || r.job == life::Job::Child || r.job == life::Job::Noble || r.job == life::Job::Beggar) return false;
        if (r.actor >= 0 && g.findActor(r.actor) > 0) return false;   // out in the street, or already here
        const life::Act pa = S.plans[(size_t)i].act;
        if (S.plans[(size_t)i].bldg == off) return false;             // already placed above
        if (owls && pa == life::Act::Sleep && g.hour < 23.0f && (r.traits & life::TR_SOCIABLE) && r.job != life::Job::Elder) return true;
        return pa == life::Act::Home || pa == life::Act::Wander || pa == life::Act::Socialise || pa == life::Act::Eat;
      };
      auto drop = [&](int i, life::Act act) {
        life::Plan p2;
        p2.act = act; p2.place = life::Place::Gathering; p2.bldg = (int16_t)off;
        const int id = spawnResident(g, si, *c, i, tileCentre(g.sub.exitX, g.sub.exitY - 1), true);
        const int k = g.findActor(id);
        if (k <= 0) return false;
        Actor& a = g.actors[(size_t)k];
        if (!placeIndoors(g, a, *c, c->res[(size_t)i], p2, true)) { a.lifeBits |= LB_GONE; return false; }
        R.rt(a.id).dropIn = true;
        placed++;
        return true;
      };
      for (int round = 0; round < 2; round++, owls = true) {
      if (!bard)   // the musician: the town's bard if free, else a sociable soul with an instrument
        for (int pass = 0; pass < 2 && !bard; pass++)
          for (int j = 0; j < n && !bard; j++) {
            const int i = (start + j) % n;
            if (!freeTonight(i)) continue;
            const life::Resident& r = c->res[(size_t)i];
            if (pass == 0 ? r.job != life::Job::Bard : !(r.traits & life::TR_SOCIABLE)) continue;
            bard = drop(i, life::Act::Perform);
          }
      for (int j = 0; j < n && patrons < target && placed < 18; j++) {
        const int i = (start + j) % n;
        if (!freeTonight(i)) continue;
        if (drop(i, life::Act::Tavern)) patrons++;
      }
      }
    }
    // drop the ones that found no place
    for (size_t k = g.actors.size(); k-- > 1;)
      if (g.actors[k].lifeBits & LB_GONE) removeActorAt(g, k);
    pairChats(g);
  }
  // where a resident is in the room by what they do (sit, work at a station, perform, stand about)
  static bool placeIndoors(Game& g, Actor& a, life::Census& c, const life::Resident& r, const life::Plan& p, bool gatherHere) {
    using life::Act;
    using life::Job;
    RT& R = rt(g);
    RT::ARt& A = R.rt(a.id);
    A.act = p.act; A.place = p.place; A.pbldg = p.bldg; A.kind = T_SPOT; A.arrived = true;
    a.posture = Posture::None;
    switch (p.act) {
      case Act::Perform: {
        // the bard: a free spot in the hall near its fire, facing the room, the culture's instrument
        int hx = -1, hy = -1;
        for (const RT::Furn& s : R.in.stations) if (s.p == Prop::Hearth || s.p == Prop::FirePitM || s.p == Prop::Stove) { hx = s.x; hy = s.y; break; }
        if (hx < 0) { hx = g.sub.w / 2; hy = g.sub.h / 2; }
        if (!standFree(g, a, {RoomKind::Common, RoomKind::Feast, RoomKind::TeaRoom, RoomKind::Court, RoomKind::Bath, RoomKind::Hall}, hx, hy + 1) &&
            !standFree(g, a, {}, hx, hy + 1))
          return false;
        a.face = 0;
        a.posture = bardPosture(g.world.cultureOf(a.site));
        return true;
      }
      case Act::Tavern: case Act::Socialise: case Act::Bathe: {
        // (M5 fixer r2) a bathhouse's evening is spent in the water: bathers first, then the benches round the pool
        if (!R.in.poolTiles.empty() && (p.act == Act::Bathe || (mixu((uint64_t)a.id, (uint64_t)g.day + 0xBA7Eull) % 5u) < 3) && bathe(g, a))
          return true;
        const Posture post = p.act == Act::Bathe ? Posture::Sit : ((mixu((uint64_t)a.id, (uint64_t)g.day) % 4u) == 0 ? Posture::SitEat : Posture::SitDrink);
        if (sitDown(g, a, {RoomKind::Common, RoomKind::Feast, RoomKind::TeaRoom, RoomKind::Bath, RoomKind::Court, RoomKind::Parlour, RoomKind::Hall, RoomKind::ThroneHall, RoomKind::Changing}, post))
          return true;
        // standing about the hall with a drink: beside someone else standing (a pair, facing), else anywhere in it
        if (!standBeside(g, a) &&
            !standFree(g, a, {RoomKind::Common, RoomKind::Feast, RoomKind::TeaRoom, RoomKind::Bath, RoomKind::Court, RoomKind::Hall, RoomKind::Changing}, -1, -1))
          return false;
        a.posture = Posture::Drink;
        return true;
      }
      case Act::Eat: {
        const Posture post = Posture::SitEat;
        if (sitDown(g, a, gatherHere ? std::initializer_list<RoomKind>{RoomKind::Common, RoomKind::Feast, RoomKind::TeaRoom, RoomKind::Hall}
                                     : std::initializer_list<RoomKind>{RoomKind::Hall, RoomKind::Cottage, RoomKind::Kitchen, RoomKind::Common, RoomKind::Feast},
                    post))
          return true;
        if (!standFree(g, a, {}, -1, -1)) return false;
        a.posture = Posture::Eat;
        return true;
      }
      case Act::Pray: {
        if (sitDown(g, a, {RoomKind::Nave, RoomKind::Court}, Posture::Pray)) { a.face = 1; return true; }
        if (!standFree(g, a, {RoomKind::Nave, RoomKind::Court}, -1, -1) && !standFree(g, a, {}, -1, -1)) return false;
        a.face = 1;
        a.posture = Posture::Pray;
        return true;
      }
      case Act::Work: {
        bool ok = false;
        switch (r.job) {
          case Job::Smith: ok = standAt(g, a, Prop::Anvil, Posture::Hammer, {Prop::Grindstone, Prop::Forge}); break;
          case Job::Baker: ok = standAt(g, a, Prop::Oven, Posture::Stir, {Prop::PrepTable, Prop::Cauldron, Prop::Hearth}); break;
          case Job::Innkeeper: ok = standAt(g, a, Prop::CounterM, Posture::None, {Prop::CounterL, Prop::CounterR, Prop::Counter}); break;
          case Job::Server: {
            // the server keeps the tables (sweeping, wiping) and brings the mugs round (serve); the bar is the keeper's
            a.lifeBits |= LB_SERVER;
            RT& RR = rt(g);
            if (!RR.in.seats.empty()) {
              const RT::Furn& s = RR.in.seats[mixu((uint64_t)a.id, (uint64_t)g.day) % RR.in.seats.size()];
              ok = standFree(g, a, {RoomKind::Common, RoomKind::Feast, RoomKind::TeaRoom, RoomKind::Hall, RoomKind::Bath, RoomKind::Court}, s.x, s.y, 3);
              if (ok) a.posture = Posture::Sweep;
            }
            if (!ok) ok = standAt(g, a, Prop::CounterM, Posture::None, {Prop::CounterL, Prop::CounterR, Prop::Counter, Prop::Cauldron, Prop::Oven});
            break;
          }
          case Job::Tailor: ok = standAt(g, a, Prop::Loom, Posture::None, {Prop::SpinningWheel, Prop::Workbench, Prop::Counter}); break;
          case Job::Scholar: ok = standAt(g, a, Prop::Lectern, Posture::Read, {Prop::Desk, Prop::Bookshelf}); break;
          case Job::Priest: ok = standAt(g, a, Prop::Altar, Posture::Pray, {Prop::Lectern}); break;
          case Job::Merchant: ok = standAt(g, a, Prop::CounterM, Posture::None, {Prop::CounterL, Prop::CounterR, Prop::Counter, Prop::Desk}); break;
          case Job::Miner: case Job::Woodcutter: ok = standAt(g, a, Prop::Workbench, Posture::Chop, {Prop::Grindstone}); break;
          case Job::Noble: ok = sitDown(g, a, {RoomKind::ThroneHall, RoomKind::Council, RoomKind::Assembly, RoomKind::Hall}, Posture::Sit); break;
          default: break;
        }
        if (ok) return true;
        // sweeping: servants, stablehands, apprentices without a station
        if (!standFree(g, a, {}, -1, -1)) return false;
        a.posture = (r.job == Job::Servant || r.job == Job::Server || r.job == Job::Stablehand || r.job == Job::Innkeeper || r.job == Job::Merchant)
                        ? Posture::Sweep : Posture::None;
        return true;
      }
      case Act::Home: case Act::Wander: case Act::Play: default: {
        // at home in the day: by the hearth or at the table; children about the room
        if (r.job == Job::Child) {
          if (!standFree(g, a, {}, -1, -1)) return false;
          a.posture = (mixu((uint64_t)a.id, (uint64_t)g.day) % 2u) ? Posture::SitFloor : Posture::None;   // a game on the floor, or about
          return true;
        }
        if ((r.job == Job::Elder || (mixu((uint64_t)a.id, (uint64_t)g.day * 3u) % 2u) == 0) &&
            sitDown(g, a, {RoomKind::Hall, RoomKind::Cottage, RoomKind::Parlour, RoomKind::Common, RoomKind::Feast}, Posture::Sit))
          return true;
        if (!standFree(g, a, {}, -1, -1)) return false;
        if (r.flags & life::RF_GRIEVING) bubble(a, Bubble::Tear, 4.0f);
        a.posture = (mixu((uint64_t)a.id, 11u) % 3u) == 0 ? Posture::Sweep : Posture::None;
        return true;
      }
    }
  }
  // a building's key person at their post: the smith at the anvil, the priest at the altar, the baker at the oven...
  static void keyPost(Game& g, Actor& a, life::Census* c, const Bldg& B) {
    using art::Building;
    (void)c;
    switch (B.type) {
      case Building::Smithy: case Building::Smelter: standAt(g, a, Prop::Anvil, Posture::Hammer, {Prop::Forge, Prop::Grindstone}); break;
      case Building::Bakery: standAt(g, a, Prop::Oven, Posture::Stir, {Prop::PrepTable, Prop::Cauldron}); break;
      case Building::Temple: if (standAt(g, a, Prop::Altar, Posture::Pray, {Prop::Lectern})) a.face = 1; break;
      case Building::Tower: break;   // the mage keeps the generator's place (and goes up to bed at night: loadMapActors)
      case Building::Weaver: standAt(g, a, Prop::Loom, Posture::None, {Prop::SpinningWheel}); break;
      default: {
        // a keeper behind their counter where the generator did not already put them there
        const int x = tileX(a.p), y = tileY(a.p);
        bool behind = false;
        for (const RT::Furn& s : rt(g).in.stations) if (counterProp(s.p) && std::abs(s.x - x) + std::abs(s.y - y) <= 1) behind = true;
        if (!behind && (B.type == Building::Inn || B.type == Building::Shop || B.type == Building::Butcher || B.type == Building::Fishmonger ||
                        B.type == Building::MeadHall || B.type == Building::TeaHouse))
          standAt(g, a, Prop::CounterM, Posture::None, {Prop::CounterL, Prop::CounterR, Prop::Counter});
        if (a.useX < 0) reserve(g, a, tileX(a.p), tileY(a.p));
        break;
      }
    }
    if (a.useX < 0) reserve(g, a, tileX(a.p), tileY(a.p));
    a.lifeBits |= LB_KEY;
  }
  // an inn's traveller: in bed at night (their own guest room), about their room by day
  static void guestPlace(Game& g, Actor& a) {
    if (!(g.hour >= 21.0f || g.hour < 7.0f)) return;
    RT& R = rt(g);
    const int ri = g.sub.roomIndexAt(tileX(a.p), tileY(a.p));
    for (size_t b = 0; b < R.in.beds.size(); b++)
      if (R.in.beds[b].room == ri && lieDown(g, a, (int)b)) return;
  }
  // seated neighbours chat in pairs
  static void pairChats(Game& g) {
    RT& R = rt(g);
    for (size_t i = 1; i < g.actors.size(); i++) {
      Actor& a = g.actors[i];
      if (!(a.lifeBits & LB_LIFE) || (a.lifeBits & LB_SERVER) || a.posture == Posture::Sleep) continue;
      RT::ARt& A = R.rt(a.id);
      if (A.partner >= 0) continue;
      for (size_t j = i + 1; j < g.actors.size(); j++) {
        Actor& o = g.actors[j];
        if (!(o.lifeBits & LB_LIFE) || (o.lifeBits & LB_SERVER) || o.posture == Posture::Sleep) continue;
        RT::ARt& B = R.rt(o.id);
        if (B.partner >= 0 || len2(o.p - a.p) > (2.2f * TILE) * (2.2f * TILE)) continue;
        A.partner = o.id; B.partner = a.id;
        A.t = 0.5f; B.t = 1.7f;
        break;
      }
    }
  }

  // the interior's life over time: bubbles, service, the bard, the festival, comings and goings
  static bool interiorFolk(Game& g, Actor& a, float dt) {
    RT& R = rt(g);
    RT::ARt& A = R.rt(a.id);
    a.postureT += dt;
    if (a.bubbleT > 0 && (a.bubbleT -= dt) <= 0) a.bubble = Bubble::None;
    A.t -= dt;
    // leaving: walk to the way out, then gone
    if (A.kind == T_EXIT) {
      a.posture = Posture::None;
      const Vec2 ex = tileCentre(g.sub.exitX, g.sub.exitY - 1);
      if (len2(ex - a.p) < 6.0f * 6.0f) { a.lifeBits |= LB_GONE; return true; }
      if (!g.navStep(a, ex, a.speed * kWalk, dt)) a.lifeBits |= LB_GONE;
      a.st = AState::Walk;
      return true;
    }
    if (A.kind == T_SPOT && !A.arrived) {   // walking to the place decided for them (a seat, a station, the stairs)
      const Vec2 to = tileCentre(A.gx, A.gy);
      a.posture = Posture::None;
      if (len2(to - a.p) < 3.0f * 3.0f || !g.navStep(a, to, a.speed * kWalk, dt)) {
        A.arrived = true;
        if (A.then == 1) { a.lifeBits |= LB_GONE; return true; }
        a.p = A.dest;
        // (M7) a spot taken since it was chosen (a notice board put up, a FOR SALE sign): stand on the nearest clear tile
        if (!g.inside && A.post == Posture::None && !g.bodyFree(a.p, a.radius, false)) a.p = snapWalk(g, a.p);
        a.home = a.p; a.goal = a.p;
        a.posture = A.post;
        a.face = A.destFace;
        a.st = AState::Idle;
        a.postureT = 0;
      } else a.st = AState::Walk;
      return true;
    }
    a.st = AState::Idle;
    switch (a.posture) {
      case Posture::Sleep:
        if (a.bubbleT <= 0 && A.t <= 0) { bubble(a, Bubble::Zzz, 2.0f); A.t = 4.0f + (float)(mixu(a.id, (uint64_t)g.time) % 300u) * 0.01f; }
        return true;
      case Posture::Lute: case Posture::Drum: case Posture::Flute:
        if (A.t <= 0) {
          A.t = 2.0f;
          if ((mixu(a.id, (uint64_t)(g.time * 2)) % 3u) == 0) bubble(a, Bubble::Note, 1.4f);
          // the last chord of a tune: the room cheers
          if ((mixu(a.id, (uint64_t)(g.time / 30.0f)) % 9u) == 0 && fmodf(g.time, 30.0f) < 2.0f) g.sfx((int)Sfx::Cheer, a.p, 1.0f, 0.5f);
        }
        return true;
      default: break;
    }
    // service: the server takes a mug to a seated patron who has none, then back to the bar
    if (a.resident >= 0 && a.site >= 0) {
      life::Census* c = g.life.findMut(g.world.sites[(size_t)a.site].id);
      if (c && a.resident < (int)c->res.size() && c->res[(size_t)a.resident].job == life::Job::Server && serve(g, a, A, dt)) return true;
    }
    // a brawl (an unhappy town's tavern, MF_BRAWLS): two at it with their fists, the keeper shouting, then one storms out
    if (A.act == life::Act::Brawl) {
      if (A.partner < 0) {
        for (size_t k = 1; k < g.actors.size(); k++) {
          Actor& o = g.actors[k];
          if (o.id == a.id || !(o.lifeBits & LB_LIFE)) continue;
          auto it = R.act.find(o.id);
          if (it == R.act.end() || it->second.act != life::Act::Brawl || it->second.partner >= 0 || len2(o.p - a.p) > (7.0f * TILE) * (7.0f * TILE)) continue;
          A.partner = o.id; it->second.partner = a.id;
          A.t2 = 0; it->second.t2 = 0;
          break;
        }
      }
      const int k = A.partner >= 0 ? g.findActor(A.partner) : -1;
      if (k > 0) {
        Actor& o = g.actors[(size_t)k];
        release(g, a);
        a.posture = Posture::None;
        if (len2(o.p - a.p) > 15.0f * 15.0f) { if (!g.navStep(a, o.p, a.speed * 0.8f, dt)) A.partner = -1; a.st = AState::Walk; return true; }
        a.face = faceOf(o.p - a.p);
        A.t2 += dt;
        if (a.st != AState::Windup && a.st != AState::Strike && fmodf(A.t2 + (float)(a.id % 7) * 0.2f, 1.5f) < dt) { a.st = AState::Windup; a.stT = 0; }
        if (a.st == AState::Windup && a.stT > 0.3f) { a.st = AState::Strike; a.stT = 0; g.sfx((int)Sfx::Hit, a.p, 1.3f, 0.25f); }
        if (a.st == AState::Strike && a.stT > 0.25f) a.st = AState::Idle;
        if (a.bubbleT <= 0) bubble(a, Bubble::Anger, 1.0f);
        if (A.t2 > 3.0f && A.t2 - dt <= 3.0f)
          for (size_t j = 1; j < g.actors.size(); j++)
            if ((g.actors[j].lifeBits & LB_KEY) && onScreen(g, g.actors[j].p)) { say(g, g.actors[j], "TAKE IT OUTSIDE, YOU TWO!", rgba(255, 190, 150)); break; }
        if (A.t2 > 12.0f && a.id < o.id) {   // over: the one storms off home, the other back to their drink
          A.kind = T_EXIT; A.partner = -1; a.st = AState::Idle;
          auto it = R.act.find(o.id);
          if (it != R.act.end()) { it->second.partner = -1; it->second.act = life::Act::Tavern; }
          o.st = AState::Idle;
          o.posture = Posture::Drink;
        }
        return true;
      }
    }
    // chat pairs
    if (A.partner >= 0) {
      const int k = g.findActor(A.partner);
      if (k > 0 && A.t <= 0) {
        Actor& o = g.actors[(size_t)k];
        if (a.posture == Posture::None || a.posture == Posture::Drink || a.posture == Posture::Sweep) a.face = faceOf(o.p - a.p);
        A.t = 2.6f + (float)(mixu(a.id, (uint64_t)(g.time * 3)) % 240u) * 0.01f;
        static const Bubble chat[] = {Bubble::Talk, Bubble::Talk, Bubble::Mug, Bubble::Exclaim, Bubble::Talk, Bubble::Question};
        Bubble b = chat[mixu(a.id ^ o.id, (uint64_t)(g.time * 2)) % 6u];
        if (a.posture == Posture::Sit || a.posture == Posture::SitEat || a.posture == Posture::SitFloor || a.posture == Posture::SitFloorEat) b = b == Bubble::Mug ? Bubble::Talk : b;
        bubble(a, b, 1.5f);
      }
    }
    // a festival night: toasts and dancing
    if (a.site >= 0 && g.life.festival(g.world.sites[(size_t)a.site].id, g.day) && g.hour >= 18.0f) {
      if (a.posture == Posture::SitDrink && A.t2 <= 0 && (mixu(a.id, (uint64_t)(g.time / 6.0f)) % 5u) == 0) {
        a.posture = Posture::Cheer; a.postureT = 0; A.t2 = 2.0f;
        if (mixu(a.id, (uint64_t)g.time) % 3u == 0) g.sfx((int)Sfx::Cheer, a.p, 1.0f, 0.4f);
      }
      if (a.posture == Posture::Drink || a.posture == Posture::None) a.posture = Posture::Dance;
    }
    if (A.t2 > 0 && (A.t2 -= dt) <= 0 && a.posture == Posture::Cheer) a.posture = Posture::SitDrink;
    return true;
  }
  static bool serve(Game& g, Actor& a, RT::ARt& A, float dt) {
    // 0 at the bar, 1 taking a mug to a patron, 2 handing it over, 3 back to the bar
    switch (A.servePhase) {
      case 0: {
        if (A.t > 0) return false;
        A.t = 3.0f;
        for (size_t k = 1; k < g.actors.size(); k++) {
          Actor& o = g.actors[k];
          if (o.id == a.id || !(o.lifeBits & LB_LIFE) || (o.lifeBits & LB_SERVED) || !isSeated(o.posture)) continue;
          A.serving = o.id;
          A.barX = tileX(a.p); A.barY = tileY(a.p);   // the way back
          release(g, a);
          A.servePhase = 1;
          return true;
        }
        return false;
      }
      case 1: {
        const int k = g.findActor(A.serving);
        if (k <= 0) { A.servePhase = 3; return true; }
        Actor& o = g.actors[(size_t)k];
        a.posture = Posture::Carry;
        if (len2(o.p - a.p) > 17.0f * 17.0f) {
          if (!g.navStep(a, o.p, a.speed * 0.7f, dt)) { A.servePhase = 3; return true; }
          a.st = AState::Walk;
          return true;
        }
        A.t2 = 1.6f;
        A.servePhase = 2;
        a.face = faceOf(o.p - a.p);
        a.st = AState::Idle;
        a.posture = Posture::None;
        o.lifeBits |= LB_SERVED;
        o.posture = (o.posture == Posture::SitFloor || o.posture == Posture::SitFloorEat || o.posture == Posture::SitFloorDrink) ? Posture::SitFloorDrink
                                                                                                                               : Posture::SitDrink;
        bubble(o, Bubble::Mug, 1.8f);
        g.sfx((int)Sfx::Coin, o.p, 1.4f, 0.25f);
        return true;
      }
      case 2:
        a.st = AState::Idle;
        if ((A.t2 -= dt) <= 0) A.servePhase = 3;
        return true;
      default: {
        if (A.barX < 0) { A.servePhase = 0; return false; }
        const Vec2 bar = tileCentre(A.barX, A.barY);
        if (len2(bar - a.p) > 3.0f * 3.0f) {
          if (!g.navStep(a, bar, a.speed * 0.7f, dt)) a.p = bar;
          a.st = AState::Walk;
          a.posture = Posture::None;
          return true;
        }
        a.st = AState::Idle;
        reserve(g, a, A.barX, A.barY);
        a.face = 0;
        A.servePhase = 0;
        A.t = 6.0f + (float)(mixu(a.id, (uint64_t)g.time) % 400u) * 0.01f;
        return true;
      }
    }
  }

  // decide where a resident goes in this room (placeIndoors) and walk there from where they stand (false: no place)
  static bool walkToPlace(Game& g, Actor& a, life::Census& c, const life::Resident& r, const life::Plan& p, bool gatherHere) {
    RT& R = rt(g);
    const Vec2 from = a.p;
    const int face0 = a.face;
    if (!placeIndoors(g, a, c, r, p, gatherHere)) { a.p = from; return false; }
    RT::ARt& A = R.rt(a.id);
    A.dest = a.p; A.post = a.posture; A.destFace = a.face; A.then = 0;
    A.gx = tileX(a.p); A.gy = tileY(a.p);
    A.kind = T_SPOT; A.arrived = false;
    a.p = from; a.home = from; a.goal = from;
    a.face = face0;
    a.posture = Posture::None;
    return true;
  }
  // to bed: a free bed of the household on this floor, else up the stairs (gone), else out of the door
  static void goToBed(Game& g, Actor& a, RT::ARt& A) {
    RT& R = rt(g);
    release(g, a);
    a.posture = Posture::None;
    for (size_t b = 0; b < R.in.beds.size(); b++) {
      const RT::Furn& f = R.in.beds[b];
      if (!roomHomeBed(roomKind(g.sub, f.room)) || isUsed(g, f.x, f.y) || inRentedRoom(g, f.x, f.y)) continue;
      // the tile beside the bed to walk to, then lie down
      const Vec2 from = a.p;
      if (!lieDown(g, a, (int)b)) continue;
      A.dest = a.p; A.post = a.posture; A.destFace = a.face; A.then = 0;
      a.p = from; a.posture = Posture::None;
      // a free tile next to the bed (the bed itself is solid)
      static const int dx[4] = {0, 1, -1, 0}, dy[4] = {1, 0, 0, -1};
      A.gx = f.x; A.gy = f.y + 1;
      for (int k = 0; k < 4; k++)
        if (g.sub.in(f.x + dx[k], f.y + dy[k]) && !g.sub.blocked(f.x + dx[k], f.y + dy[k])) { A.gx = f.x + dx[k]; A.gy = f.y + dy[k]; break; }
      A.kind = T_SPOT; A.arrived = false;
      return;
    }
    if (g.sub.up.valid()) {
      A.gx = g.sub.up.x; A.gy = g.sub.up.y + 0;
      A.dest = tileCentre(A.gx, A.gy); A.then = 1;
      A.kind = T_SPOT; A.arrived = false;
      return;
    }
    A.kind = T_EXIT;
  }

  // the interior over time: residents whose plan takes them elsewhere walk out; arrivals walk in (ground floor)
  static void interiorTick(Game& g, float dt) {
    RT& R = rt(g);
    if (g.subBldg < 0 || R.in.key != g.mapKey()) return;
    R.in.tickT -= dt;
    if (R.in.tickT > 0) return;
    R.in.tickT = 1.5f;
    const Bldg& B = g.world.over.bldgs[(size_t)g.subBldg];
    const int si = B.site;
    life::Census* c = censusOfSite(g, si);
    if (!c) return;
    RT::SiteRt& S = siteRt(g, si);
    refreshPlans(g, S, *c);
    if (S.planStamp == R.in.stamp) return;
    R.in.stamp = S.planStamp;
    const Site& st = g.world.sites[(size_t)si];
    const int off = g.subBldg - st.bldgFirst;
    std::vector<uint8_t> here(c->res.size(), 0);
    int count = 0;
    for (size_t k = 1; k < g.actors.size(); k++) {
      Actor& a = g.actors[k];
      if (!(a.lifeBits & LB_LIFE) || a.resident < 0 || a.resident >= (int)c->res.size()) continue;
      here[(size_t)a.resident] = 1;
      count++;
      const life::Plan& p = S.plans[(size_t)a.resident];
      RT::ARt& A = R.rt(a.id);
      if (A.kind == T_EXIT) continue;
      // an evening's drop-in keeps their seat until late (one leaves a tick after 23:00, the rest by midnight)
      if (A.dropIn && p.bldg != off && (g.hour >= 18.0f && g.hour < 23.0f + (float)(mixu((uint64_t)a.id, 5u) % 10u) * 0.1f)) continue;
      if (p.bldg != off) {
        if (a.posture == Posture::Sleep) {   // the sleeper gets up and goes (or, upstairs, is simply gone)
          if (g.subFloor != 0) { a.lifeBits |= LB_GONE; continue; }
          release(g, a);
          a.posture = Posture::None;
          if (!g.bodyFree(a.p, a.radius, false)) { const Vec2 f = g.freeSpot(tileX(a.p), tileY(a.p) + 1); a.p = f; }
        }
        release(g, a);
        A.kind = T_EXIT;
        A.partner = -1;
        a.posture = Posture::None;
      } else if (p.act != A.act) {
        // a change of activity in the same building (a meal after work, bed after the evening)
        using life::Act;
        const bool sitAct = p.act == Act::Tavern || p.act == Act::Socialise || p.act == Act::Eat || p.act == Act::Bathe;
        if (p.act == Act::Sleep) { A.act = p.act; goToBed(g, a, A); continue; }
        if (sitAct && isSeated(a.posture)) {   // a meal ordered, a drink after supper: they stay in their seat
          const bool fl = a.posture == Posture::SitFloor || a.posture == Posture::SitFloorEat || a.posture == Posture::SitFloorDrink;
          a.posture = p.act == Act::Eat ? (fl ? Posture::SitFloorEat : Posture::SitEat) : (fl ? Posture::SitFloorDrink : Posture::SitDrink);
          A.act = p.act;
          continue;
        }
        release(g, a);
        A.act = p.act; A.place = p.place; A.pbldg = p.bldg;
        if (!walkToPlace(g, a, *c, c->res[(size_t)a.resident], p, off == c->gathering || off == c->gathering2)) A.kind = T_EXIT;
      }
    }
    if (g.subFloor != 0 || count >= 18) return;
    // arrivals by the door
    int arrivals = 0;
    for (size_t i = 0; i < c->res.size() && i < (size_t)LIFE_SLOTN && arrivals < 3; i++) {
      if (here[i]) continue;
      const life::Resident& r = c->res[i];
      if (r.flags & (life::RF_DEAD | life::RF_AWAY) || r.job == life::Job::Guard || (workKey(r) && r.keyBldg == off && r.actor >= 0 && g.findActor(r.actor) > 0)) continue;
      const life::Plan& p = S.plans[i];
      if (p.bldg != off || p.act == life::Act::Sleep) continue;
      const int id = spawnResident(g, si, *c, (int)i, tileCentre(g.sub.exitX, g.sub.exitY - 1), true);
      Actor& a = g.actors[(size_t)g.findActor(id)];
      // where they are going: decided now (a seat, a station), walked to from the door
      if (!walkToPlace(g, a, *c, r, p, off == c->gathering || off == c->gathering2)) { a.lifeBits |= LB_GONE; continue; }
      a.st = AState::Walk;
      g.sfx((int)Sfx::Door, a.p, 1.1f, 0.3f);
      arrivals++;
    }
  }

  // ---------------------------------------------------------------- village animals
  static void critters(Game& g, int si) {
    RT::SiteRt& S = siteRt(g, si);
    if (S.crittersOut) return;
    life::Census* c = censusOfSite(g, si);
    if (!c) return;
    S.crittersOut = true;
    const Site& st = g.world.sites[(size_t)si];
    const uint32_t h0 = mixu(c->seed, 0xA417A15ull);
    const bool farm = (ew::Specialty)st.special == ew::Specialty::Farming || st.type == SiteType::Village;
    const bool herd = (ew::Specialty)st.special == ew::Specialty::Herding;
    struct Want { art::Critter k; const std::vector<RT::Spot>* v; };
    std::vector<Want> w;
    const int dogs = st.type == SiteType::Village ? 2 : 3, cats = 2;
    for (int k = 0; k < dogs; k++) w.push_back({art::Critter::Dog, &S.yard});
    for (int k = 0; k < cats; k++) w.push_back({art::Critter::Cat, &S.street});
    if (farm) { w.push_back({art::Critter::Rooster, &S.yard}); for (int k = 0; k < 3; k++) w.push_back({art::Critter::Chicken, &S.yard}); }
    if (herd || farm) { w.push_back({herd ? art::Critter::Goat : art::Critter::Pig, !S.pasture.empty() ? &S.pasture : &S.yard}); }
    if (!S.water.empty()) for (int k = 0; k < 2; k++) w.push_back({art::Critter::Duck, &S.water});
    int n = 0;
    for (size_t i = 0; i < w.size() && n < CRITTER_MAX; i++) {
      const std::vector<RT::Spot>& v = w[i].v->empty() ? S.street : *w[i].v;
      const RT::Spot* s = pick(g, v, h0 + (uint32_t)i * 2654435761u);
      if (!s) continue;
      const int lx = s->x - g.world.ox, ly = s->y - g.world.oy;
      if (len2(tileCentre(lx, ly) - g.pl().p) > (float)(Game::FOLK_OUT * TILE) * (float)(Game::FOLK_OUT * TILE)) continue;
      // hens of one yard stay together: near the rooster
      Actor a;
      a.id = g.nextId_++;
      a.npc = true; a.human = false; a.hostile = false; a.faction = Faction::Town;
      a.critter = (uint8_t)((int)w[i].k + 1);
      a.critterVar = mixu(c->seed, i * 977u + 13u);
      a.p = tileCentre(lx, ly); a.home = a.p; a.goal = a.p;
      a.site = si; a.slot = CRITTER_SLOT0 + (int)i; a.fromMap = true;
      a.maxHp = a.hp = 12; a.radius = w[i].k == art::Critter::Dog || w[i].k == art::Critter::Goat || w[i].k == art::Critter::Pig ? 4.0f : 2.5f;
      a.speed = w[i].k == art::Critter::Dog ? 60.0f : w[i].k == art::Critter::Cat ? 50.0f : 28.0f;
      a.lifeBits = LB_CRITTER;
      static const char* names[] = {"DOG", "CAT", "HEN", "ROOSTER", "GOAT", "PIG", "DUCK"};
      a.name = names[(int)w[i].k];
      a.level = 1;
      if (g.felled_.count(si) && g.felled_[si].count(a.slot)) continue;
      g.actors.push_back(a);
      n++;
    }
  }
  static bool critterFolk(Game& g, Actor& a, float dt) {
    RT& R = rt(g);
    RT::ARt& A = R.rt(a.id);
    const art::Critter k = (art::Critter)(a.critter - 1);
    if (a.bubbleT > 0 && (a.bubbleT -= dt) <= 0) a.bubble = Bubble::None;
    // (M7) a yard spot that lies under a building or a solid prop now (a lot built on, a house's footprint): off it
    if (!g.inside && g.world.over.blocked(tileX(a.p), tileY(a.p))) { a.p = snapWalk(g, a.p); a.home = a.goal = a.p; }
    a.postureT += dt;
    A.t -= dt;
    // danger: a beast near (dogs bark at it, everyone else scatters)
    const Actor* threat = nullptr;
    float td = (6.0f * TILE) * (6.0f * TILE);
    for (int hi : g.hostiles_) {
      if (hi < 0 || hi >= (int)g.actors.size()) continue;
      const Actor& e = g.actors[(size_t)hi];
      if (e.st == AState::Dead || !e.hostile || e.player) continue;
      const float d = len2(e.p - a.p);
      if (d < td) { td = d; threat = &e; }
    }
    if (threat) {
      a.posture = Posture::None;
      if (k == art::Critter::Dog) {
        a.face = faceOf(threat->p - a.p);
        a.st = AState::Idle;
        if (A.t2 <= 0) { A.t2 = 1.3f; g.sfx((int)Sfx::Bark, a.p, 1.0f, 0.8f); bubble(a, Bubble::Exclaim, 0.9f); }
        A.t2 -= dt;
        return true;
      }
      const Vec2 away = norm(a.p - threat->p);
      g.moveActor(a, away * (a.speed * 1.2f * dt));
      a.face = faceOf(away); a.st = AState::Walk;
      return true;
    }
    const bool night = g.hour >= 21.0f || g.hour < 5.5f;
    // dogs keep to their owner (the nearest resident in the street) by day, lie by a door at night
    if (k == art::Critter::Dog && !night) {
      int best = -1;
      float bd = (7.0f * TILE) * (7.0f * TILE);
      for (size_t j = 1; j < g.actors.size(); j++) {
        const Actor& o = g.actors[j];
        if (!(o.lifeBits & LB_LIFE) || o.site != a.site || o.st != AState::Walk) continue;
        const float d = len2(o.p - a.p);
        if (d < bd) { bd = d; best = (int)j; }
      }
      if (best > 0 && (mixu(a.id, (uint64_t)(g.time / 20.0f)) % 3u) != 0) {
        const Actor& o = g.actors[(size_t)best];
        const Vec2 to = o.p - o.aim * 12.0f + Vec2(8.0f, 2.0f);
        if (len2(to - a.p) > 10.0f * 10.0f) {
          a.posture = Posture::None;
          stepToward(g, a, to, a.speed * 0.8f, dt);
          return true;
        }
      }
    }
    if (night && (k == art::Critter::Dog || k == art::Critter::Cat || k == art::Critter::Chicken || k == art::Critter::Rooster)) {
      a.posture = Posture::Sleep; a.st = AState::Idle;
      return true;
    }
    // idle and potter about the home spot: peck, graze, groom
    if (A.t <= 0) {
      A.t = 2.0f + (float)(mixu(a.id, (uint64_t)(g.time * 2)) % 500u) * 0.01f;
      const uint32_t hh = mixu(a.id, (uint64_t)(g.time * 7));
      if (hh % 3u == 0) {
        a.goal = a.home + Vec2((float)((int)(hh >> 8) % 41 - 20), (float)((int)(hh >> 16) % 25 - 12));
        if (!g.bodyFree(a.goal, a.radius, false)) a.goal = a.p;
        a.posture = Posture::None;
      } else {
        a.goal = a.p;
        a.posture = (hh % 3u == 1) ? Posture::Eat : Posture::Sit;   // the action frames (peck, graze, groom / sit and wag)
        a.postureT = 0;
        // (the idle voices are the view's: render.cpp picks one animal near the player every few seconds; the sim only
        // sounds event barks, e.g. a dog at wolves, so nothing is heard twice)
      }
    }
    const Vec2 d = a.goal - a.p;
    if (len2(d) > 4.0f) {
      const Vec2 before = a.p;
      stepToward(g, a, a.goal, a.speed * 0.35f, dt);
      if (len2(a.p - before) < 0.0004f) a.goal = a.p;
    } else a.st = AState::Idle;
    return true;
  }

  // ================================================================ P1 Fresh Roads: stumble-on hooks
  // the LIFE lane's Game::marks tags (Mk 144..159): 144 an overheard giver (npcKey): 1 + the week their need was last
  // overheard; 145 a settlement (site id): day * 4 + the overheard leads that day
  static Mk mkLife(int n) { return (Mk)(144 + n); }
  static Actor* actorById(Game& g, int id) {
    const int k = id >= 0 ? g.findActor(id) : -1;
    return k > 0 ? &g.actors[(size_t)k] : nullptr;
  }
  static std::string callName(const life::Resident* r, const std::string& name, uint32_t h) {
    const std::string fn = firstName(name);
    if (!r) return fn;
    if (r->age >= 58 && h % 2u == 0) return "OLD " + fn;
    if (h % 3u == 1 && r->job != life::Job::Child && r->job != life::Job::Elder && r->job != life::Job::None) {
      std::string j = life::jobName(r->job);
      return fn + " THE " + j;
    }
    return fn;
  }
  static const life::Resident* residentOf(Game& g, const Actor& a) {
    if (a.resident < 0 || a.site < 0 || a.site >= (int)g.world.sites.size()) return nullptr;
    const life::Census* c = g.life.find(g.world.sites[(size_t)a.site].id);
    return c && a.resident < (int)c->res.size() ? &c->res[(size_t)a.resident] : nullptr;
  }
  // the lead's place: the person's own door (a resident), else where they stand (global tile)
  static void leadPlace(Game& g, const Actor& a, jobs::Lead& L) {
    L.hasPos = true;
    L.gx = g.world.ox + tileX(a.p); L.gy = g.world.oy + tileY(a.p);
    const life::Resident* r = residentOf(g, a);
    if (r && r->home >= 0) {
      const life::Census* c = g.life.find(g.world.sites[(size_t)a.site].id);
      int x, y;
      if (c && doorStep(g, c->bldgHandle(g.world.sites[(size_t)a.site], r->home), x, y)) { L.gx = g.world.ox + x; L.gy = g.world.oy + y; }
    }
  }
  // (5a) overheard: a chatting pair near the player lets slip a real need of someone in town ("...OLD BRANDR'S BEEN
  // ASKING FOR SOMEONE TO CULL THE WOLVES..."): a person in play in this settlement with work to give, not overheard this
  // week; the line becomes a journal lead (LeadFrom::Gossip) pointing at their door. At most two a town a day.
  static uint32_t overheard(Game& g, const Actor& speaker, int partnerId, bool force) {
    RT& R = rt(g);
    const int si = speaker.site;
    if (si < 0 || si >= (int)g.world.sites.size() || !g.world.sites[(size_t)si].settlement()) return 0;
    const ew::Gid sid = g.world.sites[(size_t)si].id;
    const int week = g.day / 7;
    int32_t today = 0;
    {
      auto it = g.marks.find(markKey(sid, mkLife(1)));
      if (it != g.marks.end() && it->second / 4 == g.day) today = it->second % 4;
    }
    if (!force && today >= 2) return 0;
    std::vector<std::pair<uint32_t, int>> cand;
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& o = g.actors[k];
      if (!o.npc || !o.human || o.hostile || o.comp || o.critter || o.st == AState::Dead || o.site != si || o.id == speaker.id || o.id == partnerId) continue;
      if (o.lifeBits & LB_THIEF) continue;
      if (!g.offersWork(o)) continue;
      const uint64_t key = g.npcKey(o);
      auto it = g.marks.find(markKey(key, mkLife(0)));
      if (it != g.marks.end() && it->second == week + 1) continue;
      cand.push_back({mixu(key, (uint64_t)g.day * 24u + (uint64_t)g.hour), (int)k});
    }
    if (cand.empty()) return 0;
    std::sort(cand.begin(), cand.end());
    const Actor giver = g.actors[(size_t)cand[0].second];   // (a copy: offerFor may stream places in)
    // (P1 fixer) the job named is the one this person offers when asked (makeOffer: their pool job this week), so the
    // lead's words, its qtype and the lead closing on taking the job all agree
    bool ok = false;
    const Quest q = g.makeOffer(giver, &ok);
    if (!ok) return 0;
    const life::Resident* gr = residentOf(g, giver);
    const uint32_t h = mixu(g.npcKey(giver), (uint64_t)g.day);
    const std::string who = callName(gr, giver.name, h);
    const std::string fn = firstName(giver.name);
    const std::string town = g.world.sites[(size_t)si].name;
    const std::string subj = q.subject.empty() ? std::string() : q.subject;
    const std::string mons = monsterPlural(q.mon);
    const std::string there = q.target >= 0 && q.target < (int)g.world.sites.size() ? g.world.sites[(size_t)q.target].name : std::string("THE NEXT TOWN");
    // the line said (short: the street's floating words; the first that fits 30 letters) and the journal's lead
    std::vector<std::string> lines;
    std::string lead;
    const std::string sj = subj.empty() ? std::string() : subj;
    switch (q.type) {
      case QType::Hunt:
        lines = {who + " WANTS THE " + mons + " CULLED", fn + " WANTS " + mons + " CULLED", fn + " NEEDS A HUNTER"};
        lead = fn + " OF " + town + " WANTS " + mons + " CULLED";
        break;
      case QType::Clear: lines = {fn + " WANTS " + there + " CLEARED", fn + " WANTS A DEN CLEARED"}; lead = fn + " OF " + town + " WANTS " + there + " CLEARED"; break;
      case QType::Deliver: lines = {who + " NEEDS A COURIER", fn + " NEEDS A COURIER"}; lead = fn + " OF " + town + " NEEDS A COURIER"; break;
      case QType::Missing:
        lines = {sj.empty() ? std::string() : sj + " WENT MISSING, POOR " + fn, fn + " HAS SOMEONE MISSING"};
        lead = fn + " OF " + town + ": SOMEONE IS MISSING";
        break;
      case QType::Retrieve: case QType::Heirloom:
        lines = {sj.empty() ? std::string() : fn + " LOST " + sj, fn + " LOST SOMETHING DEAR"};
        lead = fn + " OF " + town + " LOST " + (sj.empty() ? std::string("AN HEIRLOOM") : sj);
        break;
      case QType::Bounty: case QType::NamedBandit:
        lines = {sj.empty() ? std::string() : fn + " WANTS " + sj + " DEAD", fn + " WANTS BANDITS DEAD", fn + " OFFERS A BOUNTY"};
        lead = fn + " OF " + town + " OFFERS A BOUNTY";
        break;
      case QType::Protect: lines = {who + " FEARS FOR THE FARM", fn + " FEARS FOR THE FARM"}; lead = fn + " OF " + town + " FEARS FOR THE FARM"; break;
      case QType::Supply: lines = {who + " IS SHORT OF GOODS", fn + " IS SHORT OF GOODS"}; lead = fn + " OF " + town + " IS SHORT OF GOODS"; break;
      default: lines = {who + " COULD USE A HAND", fn + " NEEDS A HAND"}; lead = fn + " OF " + town + " NEEDS A HAND"; break;
    }
    std::string line;
    for (const std::string& c2 : lines) if (!c2.empty() && c2.size() <= 30) { line = c2; break; }
    if (line.empty()) line = (fn.size() > 18 ? fn.substr(0, 18) : fn) + " NEEDS HELP";
    jobs::Lead L;
    L.from = jobs::LeadFrom::Gossip;
    L.qtype = (uint8_t)q.type;
    L.site = sid;
    L.who = g.npcKey(giver);
    leadPlace(g, giver, L);
    L.text = lead;
    const uint32_t id = g.addLead(L);
    if (!id) return 0;
    g.marks[markKey(L.who, mkLife(0))] = week + 1;
    g.marks[markKey(sid, mkLife(1))] = g.day * 4 + std::min(3, today + 1);
    say(g, speaker, "..." + line + "...", rgba(232, 214, 150));
    R.street.gossipLeads++;
    return id;
  }

  // ---- (5b) street incidents: a thief running with a purse, a broken cart in the road, a brawl. Decided by the site and
  // the hour (one an hour at most, by day, a fair way from rare in a city), staged near the player with the people in the
  // street; the player steps in (talks to the victim, catches the thief, walks between the brawlers) and an Errand opens
  // (Game::openErrand) and ends on the spot (finishErrand); ignored, the incident leaves a lead (LeadFrom::Incident).
  static bool incidentFit(Game& g, const Actor& a, int si) {
    if (!(a.lifeBits & LB_LIFE) || (a.lifeBits & (LB_GONE | LB_KEY)) || a.site != si || a.st == AState::Dead || a.resident < 0) return false;
    const life::Resident* r = residentOf(g, a);
    if (!r || r->job == life::Job::Child || r->job == life::Job::Guard || r->job == life::Job::Beggar || r->job == life::Job::Noble) return false;
    auto it = rt(g).act.find(a.id);
    if (it == rt(g).act.end() || it->second.kind != T_SPOT || it->second.mood) return false;
    if (g.mode == Mode::Dialogue && g.dlg.actor == a.id) return false;
    return onScreen(g, a.p, -2.0f);
  }
  static void holdForIncident(Game& g, Actor& a) {
    RT::ARt& A = rt(g).rt(a.id);
    breakPair(g, A, a.id);
    A.mood = MOOD_INCIDENT;
    A.task = TK_NONE;
    A.path.clear();
    A.arrived = true;
    A.t = 0; A.fails = 0;
    a.posture = Posture::None;
    a.st = AState::Idle;
  }
  static bool startIncident(Game& g, int si, int kind) {
    RT& R = rt(g);
    RT::Incident& I = R.inc;
    if (I.kind || si < 0 || si >= (int)g.world.sites.size()) return false;
    const Site& st = g.world.sites[(size_t)si];
    RT::SiteRt& S = siteRt(g, si);
    const Vec2 pp = g.pl().p;
    const uint32_t h = mixu(st.id ^ 0x1AC2ull, (uint64_t)g.day * 24u + (uint64_t)g.hour);
    // the people in the street fit for it, nearest the player first (a little hash so it is not always the nearest)
    std::vector<std::pair<float, int>> cand;
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& a = g.actors[k];
      if (!incidentFit(g, a, si)) continue;
      const float d = len(a.p - pp) / TILE;
      if (d < 2.5f || d > 13.0f) continue;
      cand.push_back({d + (float)(mixu(h, (uint64_t)a.id) % 300u) * 0.01f, a.id});
    }
    std::sort(cand.begin(), cand.end());
    if (cand.empty()) return false;
    if (kind == life::INC_THIEF) {
      Actor* v = actorById(g, cand[0].second);
      if (!v || S.edge.empty()) return false;
      Vec2 side = Vec2(v->p.x < pp.x ? -12.0f : 12.0f, 3.0f);
      {   // (P1 fixer r2) off from the victim on the open side when the far side is behind a stall or under an eave
        const Vec2 s2 = v->p + side;
        if (!openTile(g.world.over, tileX(s2), tileY(s2)) && openTile(g.world.over, tileX(v->p - Vec2(side.x, -side.y)), tileY(v->p - Vec2(side.x, -side.y))))
          side.x = -side.x;
      }
      const Vec2 at = snapWalk(g, v->p + side);
      // the way out: an edge of town on the thief's side, away from the player (never past them), of a few by hash.
      // (P1 integration fixer) only an edge the thief can actually reach from where they stand: thiefRun counts a failed
      // route as a fail and gives up after three, and a failed route is cached, so a way out with no road to it ended
      // the run within three frames of the snatch (the thief "escaped" standing still, and a catch found no thief)
      std::vector<int32_t> route;
      auto reachable = [&](const RT::Spot& s) { return findPath(g, tileX(at), tileY(at), s.x - g.world.ox, s.y - g.world.oy, route, true); };
      std::vector<std::pair<float, const RT::Spot*>> ways;
      for (int k = 0; k < 32; k++) {
        const RT::Spot& s = S.edge[(h + (uint32_t)k * 2654435761u) % S.edge.size()];
        const Vec2 e = tileCentre(s.x - g.world.ox, s.y - g.world.oy);
        const float dp = len(e - pp), dv = len(e - v->p);
        if (dp < 18.0f * TILE || dv > dp) continue;
        ways.push_back({-(dp - 1.3f * dv), &s});
      }
      std::stable_sort(ways.begin(), ways.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
      const RT::Spot* esc = nullptr;
      int tries = 0;
      for (const auto& w : ways)
        if (++tries <= 6 && reachable(*w.second)) { esc = w.second; break; }
      if (!esc) {   // (no way out on their side: the edges farthest from the player)
        ways.clear();
        for (int k = 0; k < 32; k++) {
          const RT::Spot& s = S.edge[(h + (uint32_t)k * 2654435761u) % S.edge.size()];
          const float dp = len(tileCentre(s.x - g.world.ox, s.y - g.world.oy) - pp);
          if (dp >= 18.0f * TILE) ways.push_back({-dp, &s});
        }
        std::stable_sort(ways.begin(), ways.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        tries = 0;
        for (const auto& w : ways)
          if (++tries <= 6 && reachable(*w.second)) { esc = w.second; break; }
      }
      if (!esc) return false;
      Spawn sp;
      sp.npc = true; sp.role = Role::Villager; sp.site = si; sp.slot = THIEF_SLOT; sp.x = tileX(at); sp.y = tileY(at);
      const int tid = g.spawnHuman(sp, at);
      Actor& t = g.actors.back();
      t.lifeBits = LB_THIEF;
      t.name = "CUTPURSE";
      t.militia = false;
      t.fromMap = true;
      t.resident = -1;
      t.bldg = -1;
      t.p = at; t.home = at; t.goal = at;
      t.speed = 56.0f;
      t.look.topColor = rgba(58, 52, 50);
      t.look.bottomColor = rgba(44, 40, 38);
      // (P1 fixer) told from the crowd at a glance: a hooded charcoal travel cloak (nobody in town walks hooded by day)
      t.look.cloak = 1;
      t.look.cloakColor = rgba(40, 36, 42);
      t.look.hood = true;
      t.posture = Posture::Carry;   // the purse clutched to the chest
      RT::ARt& T = R.rt(tid);
      T = RT::ARt();
      T.kind = T_AWAY; T.gx = esc->x; T.gy = esc->y; T.pace = 1.0f;
      v = actorById(g, cand[0].second);   // (spawning moved the actors)
      if (!v) { t.lifeBits |= LB_GONE; return false; }
      holdForIncident(g, *v);
      v->face = faceOf(t.p - v->p);
      bubble(*v, Bubble::Exclaim, 2.5f);
      say(g, *v, "STOP, THIEF! MY PURSE!", rgba(250, 170, 130));
      g.sfx((int)Sfx::Hit, v->p, 1.6f, 0.25f);
      I = RT::Incident();
      I.kind = life::INC_THIEF; I.si = si; I.site = st.id; I.victim = v->id; I.other = tid;
      I.gx = g.world.ox + tileX(t.p); I.gy = g.world.oy + tileY(t.p); I.ex = esc->x; I.ey = esc->y;
    } else if (kind == life::INC_CART) {
      // a cart standing in the street near the player (the world's own cart props: nothing new is put down)
      const Map& m = g.world.over;
      const int px = tileX(pp), py = tileY(pp);
      int cx = -1, cy = -1, sx = -1, sy = -1;
      float best = 1e9f;
      for (int y = py - 12; y <= py + 12; y++)
        for (int x = px - 14; x <= px + 14; x++) {
          if (!m.in(x, y) || m.propAt(x, y) != (int)Prop::Cart + 1 || !st.r.contains(x, y)) continue;
          static const int ox[3] = {0, 1, -1}, oy[3] = {1, 0, 0};   // (P1 fixer: in front first, beside the loose wheel)
          for (int k = 0; k < 3; k++) {
            const int tx = x + ox[k], ty = y + oy[k];
            if (!m.in(tx, ty) || m.blocked(tx, ty) || m.bldgAt[(size_t)ty * m.w + tx] >= 0 || spotTaken(g, g.world.ox + tx, g.world.oy + ty, -1)) continue;
            const float d = (float)(std::abs(x - px) + std::abs(y - py)) + (float)(mixu(h, (uint64_t)(y * 4096 + x)) % 400u) * 0.01f;
            if (d >= 3.0f && d < best) { best = d; cx = x; cy = y; sx = tx; sy = ty; }
            break;
          }
        }
      if (cx < 0) return false;
      // its owner: the nearest fit person to the cart
      Actor* o = nullptr;
      float od = (16.0f * TILE) * (16.0f * TILE);
      for (auto& cd : cand) {
        Actor* a = actorById(g, cd.second);
        if (!a) continue;
        const float d = len2(a->p - tileCentre(cx, cy));
        if (d < od) { od = d; o = a; }
      }
      if (!o) return false;
      holdForIncident(g, *o);
      RT::ARt& A = R.rt(o->id);
      A.arrived = false;
      A.gx = g.world.ox + sx; A.gy = g.world.oy + sy;
      claimSpot(g, A, o->id, A.gx, A.gy);
      bubble(*o, Bubble::Exclaim, 2.0f);
      g.emit(Ev::Dust, tileCentre(cx, cy) + Vec2(-11, 11), 0, 0, "");   // (P1 fixer) the wheel just came off
      g.sfx((int)Sfx::Hit, tileCentre(cx, cy), 0.7f, 0.3f);
      I = RT::Incident();
      I.kind = life::INC_CART; I.si = si; I.site = st.id; I.victim = o->id;
      I.gx = g.world.ox + cx; I.gy = g.world.oy + cy;
    } else if (kind == life::INC_BRAWL) {
      // two who fall out: the first fit person and the nearest other within a few tiles of them
      for (size_t i = 0; i < cand.size(); i++) {
        Actor* a = actorById(g, cand[i].second);
        if (!a) continue;
        Actor* b = nullptr;
        float bd2 = (3.5f * TILE) * (3.5f * TILE);   // (neighbours already close: a few steps to square up, not a march)
        for (size_t j = 0; j < cand.size(); j++) {
          if (j == i) continue;
          Actor* o = actorById(g, cand[j].second);
          if (!o) continue;
          const float d = len2(o->p - a->p);
          if (d < bd2) { bd2 = d; b = o; }
        }
        if (!b) continue;
        holdForIncident(g, *a);
        holdForIncident(g, *b);
        RT::ARt& A = R.rt(a->id);
        RT::ARt& B = R.rt(b->id);
        A.partner = b->id; B.partner = a->id; A.t2 = 0; B.t2 = 0.7f;
        // a step apart, squared up
        // (P1 fixer) always side by side, face to face (west / east of the middle, 11 px each way): two people squared
        // up north-south were drawn in one column, one head painted over the other's body
        Vec2 mid = (a->p + b->p) * 0.5f;
        const Vec2 dir(b->p.x >= a->p.x ? 1.0f : -1.0f, 0.0f);
        {   // (a wall or a stall at one side: the pair shift along the street until both feet are free)
          static const float sh[5] = {0.0f, 6.0f, -6.0f, 12.0f, -12.0f};
          for (float sx : sh) {
            const Vec2 m2 = mid + Vec2(sx, 0);
            if (g.bodyFree(m2 - dir * 11.0f, a->radius, false) && g.bodyFree(m2 + dir * 11.0f, b->radius, false)) { mid = m2; break; }
          }
        }
        A.dest = mid - dir * 11.0f; B.dest = mid + dir * 11.0f;
        A.arrived = false; B.arrived = false;
        bubble(*a, Bubble::Anger, 2.0f);
        static const char* opener[] = {"SAY THAT AGAIN, I DARE YOU!", "YOU CHEATED ME, YOU DOG!", "THAT'S MY WIFE YOU'RE TALKING ABOUT!", "GIVE IT BACK!"};
        say(g, *a, opener[h % 4u], rgba(240, 150, 120));
        I = RT::Incident();
        I.kind = life::INC_BRAWL; I.si = si; I.site = st.id; I.victim = a->id; I.other = b->id;
        I.gx = g.world.ox + tileX(mid); I.gy = g.world.oy + tileY(mid);
        // the street stops to watch
        for (size_t k = 1; k < g.actors.size(); k++) {
          Actor& w = g.actors[k];
          if (w.id == a->id || w.id == b->id || !(w.lifeBits & LB_LIFE) || w.site != si || len2(w.p - mid) > (7.0f * TILE) * (7.0f * TILE)) continue;
          if (w.st == AState::Walk) continue;
          w.face = faceOf(mid - w.p);
          bubble(w, Bubble::Exclaim, 1.4f);
        }
        break;
      }
      if (I.kind != life::INC_BRAWL) return false;
    } else return false;
    if (Actor* v = actorById(g, I.victim)) { I.vSite = v->site; I.vBldg = v->bldg; I.vSlot = v->slot; I.vKey = g.npcKey(*v); I.vName = v->name; }
    R.street.incidents++;
    return true;
  }
  // an incident's resident (the robbed, the cart's owner, a brawler): what they do while it lasts
  static bool incidentHeld(Game& g, Actor& a, RT::ARt& A, float dt) {
    RT& R = rt(g);
    RT::Incident& I = R.inc;
    if (!I.kind || (a.id != I.victim && a.id != I.other)) {   // it is over: back to their day
      A.mood = 0; A.partner = -1; A.kind = T_NONE; A.task = TK_NONE; A.seg = INT32_MIN;
      a.posture = Posture::None; a.st = AState::Idle;
      return true;
    }
    life::Census* c = g.life.findMut(I.site);
    switch (I.kind) {
      case life::INC_THIEF: {
        Actor* t = actorById(g, I.other);
        a.st = AState::Idle;
        a.face = faceOf((t ? t->p : g.pl().p) - a.p);
        if (!I.engaged && len2(g.pl().p - a.p) < (4.0f * TILE) * (4.0f * TILE) && I.sayT <= 0) {
          I.sayT = 6.0f;
          bubble(a, Bubble::Exclaim, 1.5f);
          a.face = faceOf(g.pl().p - a.p);
          say(g, a, "HE'S GOT MY PURSE! STOP HIM!", rgba(250, 190, 150));
        }
        return true;
      }
      case life::INC_CART: {
        if (!A.arrived) {
          a.posture = Posture::None;
          if (walkOut(g, a, A, a.speed * kWalk * 1.1f, dt) || A.fails >= 3) { A.arrived = true; A.path.clear(); }
          else { watch(g, a, A, dt); return true; }
        }
        a.st = AState::Idle;
        a.face = faceOf(tileCentre(I.gx - g.world.ox, I.gy - g.world.oy) - a.p);
        a.posture = I.engaged && I.holdT > 0 ? Posture::Hammer : Posture::Kneel;
        if (!I.engaged && len2(g.pl().p - a.p) < (5.0f * TILE) * (5.0f * TILE) && I.sayT <= 0) {
          I.sayT = 8.0f;
          bubble(a, Bubble::Question, 2.0f);
          say(g, a, "MY WHEEL'S OFF! A HAND, PLEASE?", rgba(232, 222, 196));
        }
        return true;
      }
      case life::INC_BRAWL: {
        if (!A.arrived) {   // square up (a step, never a jump)
          if (len2(A.dest - a.p) > 4.0f && g.bodyFree(A.dest, a.radius, false)) { stepToward(g, a, A.dest, a.speed * 0.9f, dt); A.t += dt; if (A.t < 4.0f) return true; }
          A.arrived = true; A.t = 0;
        }
        if (c) brawl(g, a, A, *c, dt);
        return true;
      }
      default: break;
    }
    return true;
  }
  // (P1 fixer r2) parted: the two brawlers back off from whoever stepped between them to a body and a half on their own
  // side (the player stands where the fight was: left there, all three were drawn in one heap), glare a moment each (not
  // in step), then go back to their day
  static void partFrom(Game& g, Actor& a, Vec2 from, float side, float vy) {
    RT::ARt& A = rt(g).rt(a.id);
    breakPair(g, A, a.id);
    A.mood = MOOD_PARTED; A.partner = -1; A.kind = T_NONE; A.task = TK_NONE; A.path.clear();
    A.t = 0; A.arrived = false; A.fails = 0;
    // their own side first; a wall, a post or a hedge there: off at a slant, then up (or down) the street, the two
    // always on opposite sides of the one who parted them
    const Vec2 dirs[5] = {Vec2(side, 0), Vec2(side * 0.7f, vy * 0.7f), Vec2(side * 0.7f, -vy * 0.7f), Vec2(0, vy), Vec2(-side * 0.45f, vy * 0.9f)};
    A.dest = a.p;
    bool found = false;
    for (const Vec2& dv : dirs) {
      for (float d : {28.0f, 22.0f, 17.0f}) {
        const Vec2 c = from + dv * d;
        if (len2(c - from) <= len2(a.p - from) + 4.0f) continue;   // (never closer than they already are)
        if (g.bodyFree(c, a.radius, false)) { A.dest = c; found = true; break; }
      }
      if (found) break;
    }
    A.t2 = 2.1f + (float)(mixu((uint64_t)a.id, 0xB4A1ull) % 9u) * 0.15f;
    a.posture = Posture::None; a.st = AState::Idle;
  }
  static void partBrawlers(Game& g, int ida, int idb, Vec2 from) {
    Actor* a = actorById(g, ida);
    Actor* b = actorById(g, idb);
    float sa = -1.0f, sb = 1.0f;
    if (a && b) { sa = a->p.x <= b->p.x ? -1.0f : 1.0f; sb = -sa; }
    else if (a) sa = a->p.x < from.x ? -1.0f : 1.0f;
    else if (b) sb = b->p.x < from.x ? -1.0f : 1.0f;
    if (a) partFrom(g, *a, from, sa, -1.0f);
    if ((b = actorById(g, idb))) partFrom(g, *b, from, sb, 1.0f);
  }
  static bool partedHeld(Game& g, Actor& a, RT::ARt& A, float dt) {
    A.t += dt;
    const Vec2 pp = g.pl().p;
    if (!A.arrived) {
      if (len2(A.dest - a.p) > 1.5f && A.t < 1.8f) {
        // backing off with their eyes on the one who parted them: a startled quick step that slows (ease-out)
        const float ease = 1.6f - 1.1f * std::min(1.0f, A.t / 0.7f);
        stepToward(g, a, A.dest, a.speed * ease, dt);
        a.face = faceOf(pp - a.p);
        return true;
      }
      A.arrived = true;
    }
    a.st = AState::Idle;
    a.face = faceOf(pp - a.p);
    if (A.t >= A.t2) {   // over: back to their day
      A.mood = 0; A.kind = T_NONE; A.task = TK_NONE; A.seg = INT32_MIN; A.arrived = false;
      a.posture = Posture::None;
    }
    return true;
  }
  // the cutpurse: runs for the edge of town, a look back over the shoulder now and then (catchable on foot); caught or
  // away, off out of town
  static bool thiefFolk(Game& g, Actor& a, float dt) {
    RT& R = rt(g);
    RT::ARt& T = R.rt(a.id);
    if (a.bubbleT > 0 && (a.bubbleT -= dt) <= 0) a.bubble = Bubble::None;
    const bool running = R.inc.kind == life::INC_THIEF && R.inc.other == a.id;
    if (running) {
      T.t += dt;
      a.posture = Posture::Carry;
      if (std::fmod(T.t, 5.5f) > 4.9f) { a.st = AState::Idle; a.face = faceOf(g.pl().p - a.p); return true; }
      if (thiefRun(g, a, T, a.speed, dt) || T.fails >= 3) T.arrived = true;
      watch(g, a, T, dt);
      // (P1 fixer) running, not walking: a puff of dust at the heels every few strides
      if (a.st == AState::Walk && std::fmod(T.t, 0.32f) < dt) g.emit(Ev::Dust, a.p + Vec2(0, 1), 0, 0, "");
      return true;
    }
    // (P1 fixer r2) caught: down on one knee, the purse given up, eyes on the one who caught them, a moment before
    // slinking off (they ran on at once, and the catch never read as one)
    if (T.t2 > 0) {
      T.t2 -= dt;
      if (T.t2 > 1.45f && len2(T.dest - a.p) > 1.0f) {   // the stumble (fast, slowing)
        stepToward(g, a, T.dest, a.speed * (0.6f + 2.0f * (T.t2 - 1.45f)), dt);
        a.face = faceOf(g.pl().p - a.p);
        return true;
      }
      a.st = AState::Idle;
      a.posture = T.t2 > 0.35f ? Posture::Kneel : Posture::None;
      a.face = faceOf(g.pl().p - a.p);
      return true;
    }
    a.posture = Posture::None;
    if (walkOut(g, a, T, a.speed * 0.85f, dt) || T.fails >= 3 || !onScreen(g, a.p, 1.0f)) a.lifeBits |= LB_GONE;
    return true;
  }
  // (P1 fixer r2) the cutpurse's run: a route that keeps to the open street (findPath inView), followed waypoint by
  // waypoint, a corner cut only where the straight way is open too. (walkOut's own look-ahead cut through the gap behind
  // a stall's counter, so the thief ran, and was caught, under the awning: a ghost in the stall)
  static bool openLine(Game& g, Vec2 a, Vec2 b, float r) {
    if (!clearLine(g, a, b, r)) return false;
    const Vec2 d = b - a;
    const float l = len(d);
    for (float t = 4.0f; t < l; t += 6.0f) {
      const Vec2 q = a + d * (t / l);
      if (!openTile(g.world.over, tileX(q), tileY(q))) return false;
    }
    return true;
  }
  static bool thiefRun(Game& g, Actor& a, RT::ARt& T, float speed, float dt) {
    RT& R = rt(g);
    const int lx = T.gx - g.world.ox, ly = T.gy - g.world.oy;
    const Vec2 dest = tileCentre(lx, ly);
    if (len2(dest - a.p) < 3.0f * 3.0f) return true;
    if (T.path.empty()) {
      if (R.pathBudget > 0) R.pathBudget--;   // (one route a run: never held up by the street's budget)
      std::vector<int32_t> p;
      if (!findPath(g, tileX(a.p), tileY(a.p), lx, ly, p, true)) { T.fails++; a.st = AState::Idle; return false; }
      T.path = std::move(p);
      T.pi = 0; T.lookT = 0; T.stuckT = 0;
    }
    T.lookT -= dt;
    if (T.lookT <= 0) {
      T.lookT = 0.3f;
      for (size_t k = std::min(T.path.size() / 2, T.pi + 4); k > T.pi + 1; k--) {
        const Vec2 w = tileCentre(T.path[(k - 1) * 2] - g.world.ox, T.path[(k - 1) * 2 + 1] - g.world.oy);
        if (openLine(g, a.p, w, a.radius)) { T.pi = k - 1; break; }
      }
    }
    while (T.pi < T.path.size() / 2) {
      const Vec2 w = tileCentre(T.path[T.pi * 2] - g.world.ox, T.path[T.pi * 2 + 1] - g.world.oy);
      if (len2(w - a.p) < 2.5f * 2.5f) { T.pi++; continue; }
      const Vec2 before = a.p;
      stepToward(g, a, w, speed, dt);
      if (len2(a.p - before) < 0.0004f) {   // wedged on a corner: back to the tile's middle and plan again
        T.stuckT += dt;
        if (T.stuckT > 1.0f) { T.stuckT = 0; T.path.clear(); T.fails++; unwedge(g, a); }
      } else T.stuckT = std::max(0.0f, T.stuckT - dt);
      return false;
    }
    stepToward(g, a, dest, speed, dt);
    return len2(dest - a.p) < 3.0f * 3.0f;
  }
  static int incidentTier(const Game& g, int si) {
    const SiteType t = g.world.sites[(size_t)si].type;
    return t == SiteType::Village ? 0 : t == SiteType::Town ? 1 : 2;
  }
  // the errand the player took on (the victim is the giver: their identity, their thanks)
  static int openIncidentErrand(Game& g, const Actor& giver) {
    RT& R = rt(g);
    RT::Incident& I = R.inc;
    if (I.quest) return I.quest;
    Quest q;
    q.type = QType::Errand;
    q.stage = (int)(I.kind == life::INC_THIEF ? jobs::ErrandKind::Thief : I.kind == life::INC_CART ? jobs::ErrandKind::Cart : jobs::ErrandKind::Brawl);
    q.giverSite = giver.site; q.giverBldg = giver.bldg; q.giverSlot = giver.slot; q.giverName = giver.name;
    q.giverId = I.site;
    const std::string fn = firstName(giver.name);
    const std::string town = g.world.sites[(size_t)I.si].name;
    const int tier = incidentTier(g, I.si);
    const uint32_t h = mixu(I.site, (uint64_t)g.day * 24u + (uint64_t)g.hour);
    q.gold = 8 + tier * 6 + (int)(h % 7u);
    q.xp = 15 + tier * 6;
    switch (I.kind) {
      case life::INC_THIEF:
        q.title = "STOP, THIEF!"; q.subject = "CATCH THE THIEF";
        q.desc = "A CUTPURSE SNATCHED " + fn + "'S PURSE IN " + town + ". CATCH THEM BEFORE THEY'RE OUT OF TOWN.";
        break;
      case life::INC_CART:
        q.title = "A BROKEN CART"; q.subject = "HELP " + fn + " WITH THE CART";
        q.desc = fn + "'S CART LOST A WHEEL IN THE STREETS OF " + town + ". HOLD IT UP WHILE THE WHEEL GOES BACK ON.";
        break;
      default:
        q.title = "A BRAWL IN THE STREET"; q.subject = "BREAK UP THE BRAWL";
        q.desc = "TWO OF " + town + "'S FOLK ARE AT EACH OTHER'S THROATS. STEP IN BEFORE SOMEONE GETS HURT.";
        break;
    }
    q.hasPos = true; q.tgx = I.gx; q.tgy = I.gy;
    I.quest = g.openErrand(q);
    I.engaged = true;
    R.street.errandsOpened++;
    return I.quest;
  }
  static void finishIncident(Game& g, bool success) {
    RT& R = rt(g);
    if (R.inc.quest) {
      g.finishErrand(R.inc.quest, success);
      if (success) R.street.errandsWon++; else R.street.errandsLost++;
    }
  }
  // ignored: the incident leaves a lead (the person to see; their job when they have one to give)
  static void incidentLead(Game& g, Actor* victim) {
    RT& R = rt(g);
    const RT::Incident& I = R.inc;
    if (I.engaged || I.si < 0) return;
    jobs::Lead L;
    L.from = jobs::LeadFrom::Incident;
    L.site = I.site;
    L.qtype = (uint8_t)QType::COUNT;
    L.hasPos = true; L.gx = I.gx; L.gy = I.gy;
    const std::string town = g.world.sites[(size_t)I.si].name;
    std::string fn = I.vName.empty() ? std::string("SOMEONE") : firstName(I.vName);
    L.who = I.vKey;
    if (victim) {
      L.who = g.npcKey(*victim);
      leadPlace(g, *victim, L);
      fn = firstName(victim->name);
      if (g.offersWork(*victim)) {
        bool ok = false;
        const Quest q = g.makeOffer(*victim, &ok);   // (P1 fixer: the job they really offer when asked)
        if (ok) L.qtype = (uint8_t)q.type;
      }
    }
    L.text = I.kind == life::INC_THIEF ? "SEEN: A THIEF ROBBED " + fn + " IN " + town
             : I.kind == life::INC_CART ? "SEEN: " + fn + "'S CART BROKE DOWN IN " + town
                                        : "SEEN: " + fn + " IN A BRAWL IN " + town;
    if (g.addLead(L)) R.street.incidentLeads++;
  }
  static void endIncident(Game& g) {
    RT& R = rt(g);
    for (int id : {R.inc.victim, R.inc.other}) {
      Actor* a = actorById(g, id);
      if (!a || (a->lifeBits & LB_THIEF)) continue;
      auto it = R.act.find(id);
      if (it == R.act.end()) continue;
      RT::ARt& A = it->second;
      if (A.mood != MOOD_INCIDENT) continue;
      A.mood = 0; A.partner = -1; A.kind = T_NONE; A.task = TK_NONE; A.seg = INT32_MIN;
      a->posture = Posture::None; a->st = AState::Idle;
    }
    R.inc = RT::Incident();
  }
  // the player stepped in (dialogue's I'LL HELP, or caught up with it)
  static void engageIncident(Game& g, Actor& by) {
    RT& R = rt(g);
    RT::Incident& I = R.inc;
    if (!I.kind) return;
    Actor* v = actorById(g, I.victim);
    if (I.kind == life::INC_BRAWL) {
      // the giver: someone watching (the brawlers do not pay to be parted), else the calmer of the two
      const Actor* giver = nullptr;
      float bd = (8.0f * TILE) * (8.0f * TILE);
      for (size_t k = 1; k < g.actors.size(); k++) {
        const Actor& w = g.actors[k];
        if (w.id == I.victim || w.id == I.other || !(w.lifeBits & LB_LIFE) || w.site != I.si || w.resident < 0 || w.st == AState::Dead) continue;
        const life::Resident* r = residentOf(g, w);
        if (!r || r->job == life::Job::Child) continue;
        const float d = len2(w.p - by.p);
        if (d < bd) { bd = d; giver = &w; }
      }
      Actor* o = actorById(g, I.other);
      const Actor gv = giver ? *giver : (o ? *o : by);
      openIncidentErrand(g, gv);
      if (v) { bubble(*v, Bubble::Question, 1.6f); say(g, *v, "ALL RIGHT, ALL RIGHT! HE STARTED IT.", rgba(232, 222, 196)); }
      if ((o = actorById(g, I.other))) { o->face = faceOf(g.pl().p - o->p); bubble(*o, Bubble::Tear, 1.6f); }
      finishIncident(g, true);
      const int ia = I.victim, ib = I.other;
      endIncident(g);
      partBrawlers(g, ia, ib, by.p);
      return;
    }
    if (v) openIncidentErrand(g, *v);
  }
  // once a step outdoors: the hour's incident decided and staged; the live one followed to its end
  static void incidentTick(Game& g, float dt) {
    RT& R = rt(g);
    RT::Incident& I = R.inc;
    if (I.sayT > 0) I.sayT -= dt;
    for (size_t k = 0; k < R.sayLater.size();) {   // (P1 fixer r2) the lines said a beat later
      RT::LaterSay& ls = R.sayLater[k];
      if ((ls.t -= dt) > 0) { k++; continue; }
      if (Actor* sp = actorById(g, ls.actor)) say(g, *sp, ls.s, ls.col);
      R.sayLater.erase(R.sayLater.begin() + (std::ptrdiff_t)k);
    }
    // an errand left running with no incident behind it (a load, the clock jumped): it passes once the player is well
    // away, or after a little while
    if (!I.kind) {
      bool orphan = false;
      for (Quest& q : g.quests) {
        if (q.type != QType::Errand || q.state == QState::Done) continue;
        orphan = true;
        R.orphanT += dt;
        const float d = (float)(std::abs(q.tgx - (g.world.ox + tileX(g.pl().p))) + std::abs(q.tgy - (g.world.oy + tileY(g.pl().p))));
        if (R.orphanT > 25.0f || d > 48.0f) { g.finishErrand(q.id, false); R.orphanT = 0; break; }
      }
      if (!orphan) R.orphanT = 0;
    }
    if (!I.kind) {
      if ((R.incidentCheckT -= dt) > 0) return;
      R.incidentCheckT = 1.0f;
      if (g.mode != Mode::Play || g.inside || g.hour < 8.0f || g.hour >= 20.0f) return;
      const int px = tileX(g.pl().p), py = tileY(g.pl().p);
      int si = -1;
      for (int s : g.activeSites_)
        if (s >= 0 && s < (int)g.world.sites.size() && g.world.sites[(size_t)s].settlement() && g.world.sites[(size_t)s].r.contains(px, py)) si = s;
      if (si < 0 || warEmpty(g, si)) return;
      for (int hi : g.hostiles_)
        if (hi >= 0 && hi < (int)g.actors.size() && g.actors[(size_t)hi].aggro && g.actors[(size_t)hi].st != AState::Dead) return;   // (not in a fight)
      const Site& st = g.world.sites[(size_t)si];
      const int32_t slot = g.day * 24 + (int)g.hour;
      const uint64_t key = st.id * 31ull ^ (uint64_t)(uint32_t)slot;
      if (R.incidentSlots.count(key)) return;
      const uint32_t h = mixu(st.id ^ 0x1AC1Dull, (uint64_t)(uint32_t)slot);
      const int minute = 6 + (int)((h >> 8) % 40u);
      if ((g.hour - std::floor(g.hour)) * 60.0f < (float)minute) return;
      if (R.incidentSlots.size() > 2048) R.incidentSlots.clear();
      R.incidentSlots.insert(key);
      const int tier = incidentTier(g, si);
      if ((int)(h % 100u) >= (tier == 0 ? 10 : tier == 1 ? 24 : 32)) return;
      int kind = (int)((h >> 16) % 3u) + 1;
      if (tier == 0 && kind == life::INC_THIEF) kind = life::INC_CART;
      for (int k = 0; k < 3; k++, kind = kind % 3 + 1)
        if (startIncident(g, si, kind)) return;
      return;
    }
    I.t += dt;
    Actor* v = actorById(g, I.victim);
    const Vec2 pp = g.pl().p;
    if (I.kind == life::INC_THIEF) {
      Actor* t = actorById(g, I.other);
      if (t) {
        I.gx = g.world.ox + tileX(t->p); I.gy = g.world.oy + tileY(t->p);
        if (I.quest)
          for (Quest& q : g.quests) if (q.id == I.quest) { q.tgx = I.gx; q.tgy = I.gy; }
      }
      if (t && len2(t->p - pp) < (1.4f * TILE) * (1.4f * TILE) && g.mode == Mode::Play) {   // caught
        if (v) openIncidentErrand(g, *v);
        else if (I.vSlot >= 0) {   // (the robbed is out of sight: their identity still pays, through the errand)
          Actor who;
          who.npc = true; who.site = I.vSite; who.bldg = I.vBldg; who.slot = I.vSlot; who.name = I.vName;
          openIncidentErrand(g, who);
        }
        t->face = faceOf(pp - t->p);
        bubble(*t, Bubble::Exclaim, 1.5f);
        say(g, *t, "ALL RIGHT! ALL RIGHT! TAKE IT!", rgba(220, 200, 170));
        // (P1 fixer r2) the robbed calls their thanks only when near enough to be heard (left behind down the street:
        // a heart, no second plate over the catch)
        if (v) {
          hush(g, *v);   // (their "STOP, THIEF!" still up from the snatch: done with)
          bubble(*v, Bubble::Heart, 2.0f);
          if (len2(v->p - pp) < (6.0f * TILE) * (6.0f * TILE)) { RT::LaterSay ls; ls.actor = v->id; ls.t = 1.2f; ls.s = "MY PURSE! BLESS YOU, FRIEND!"; ls.col = rgba(240, 220, 170); R.sayLater.push_back(ls); }
        }
        finishIncident(g, I.quest != 0);
        RT::ARt& T = R.rt(t->id);
        T.path.clear(); T.fails = 0;
        T.t2 = 1.8f;   // (P1 fixer r2) the caught moment (thiefFolk): a stumble a step on from the hand that caught them
        {   // (to the side of the one who caught them, both seen whole: one behind the other in 3/4 hides the thief)
          const float sx = t->p.x >= pp.x ? 1.0f : -1.0f;
          const Vec2 d = t->p - pp;
          const float l = std::max(len(d), 0.5f);
          const Vec2 cands[5] = {pp + Vec2(sx * 19.0f, 2.0f), pp + Vec2(-sx * 19.0f, 2.0f), pp + Vec2(sx * 15.0f, -6.0f), pp + Vec2(-sx * 15.0f, -6.0f), pp + d * (18.0f / l)};
          T.dest = t->p;
          bool got = false;
          for (int pass = 0; pass < 2 && !got; pass++)   // (out in the open first: never under a stall's awning)
            for (const Vec2& c : cands)
              if ((pass || openTile(g.world.over, tileX(c), tileY(c))) && g.bodyFree(c, t->radius, false) && clearLine(g, t->p, c, t->radius)) { T.dest = c; got = true; break; }
        }
        endIncident(g);
        return;
      }
      const bool away = !t || R.rt(t->id).arrived || len2(t->p - pp) > (28.0f * TILE) * (28.0f * TILE) || I.t > 80.0f;
      if (away) {
        if (I.quest) {
          if (v && onScreen(g, v->p)) { bubble(*v, Bubble::Tear, 2.0f); say(g, *v, "GONE... EVERY COIN I HAD.", rgba(220, 200, 190)); }
          finishIncident(g, false);
        } else incidentLead(g, v);
        if (t) t->lifeBits |= LB_GONE;
        endIncident(g);
      }
      return;
    }
    if (!v) { if (!I.engaged) incidentLead(g, nullptr); else finishIncident(g, false); endIncident(g); return; }
    if (I.kind == life::INC_CART) {
      const Vec2 cart = tileCentre(I.gx - g.world.ox, I.gy - g.world.oy);
      const float d = len(pp - cart) / TILE;
      if (I.engaged) {
        if (d < 2.3f) {
          if (I.holdT <= 0) { say(g, *v, "HOLD IT STEADY... THAT'S IT...", rgba(232, 222, 196)); g.sfx((int)Sfx::Hit, cart, 0.8f, 0.3f); }
          I.holdT += dt;
          if (I.holdT >= 6.0f) {
            bubble(*v, Bubble::Heart, 2.0f);
            say(g, *v, "GOOD AS NEW! THANK YOU, FRIEND.", rgba(240, 220, 170));
            finishIncident(g, true);
            endIncident(g);
          }
        } else if (d > 14.0f) {
          say(g, *v, "...I'LL MANAGE ON MY OWN, THEN.", rgba(220, 200, 190));
          finishIncident(g, false);
          endIncident(g);
        }
        return;
      }
      if (I.t > 150.0f || d > 30.0f) { incidentLead(g, v); endIncident(g); }
      return;
    }
    if (I.kind == life::INC_BRAWL) {
      Actor* o = actorById(g, I.other);
      if (!o) { if (!I.engaged) incidentLead(g, v); endIncident(g); return; }
      if (len2(v->p - pp) < (1.8f * TILE) * (1.8f * TILE) || len2(o->p - pp) < (1.8f * TILE) * (1.8f * TILE)) {   // stepped between them
        engageIncident(g, g.pl());
        return;
      }
      if (I.t > 42.0f) {   // they tire of it
        say(g, *v, "HMPH. THIS ISN'T OVER.", rgba(220, 200, 190));
        incidentLead(g, v);
        endIncident(g);
      }
    }
  }

  // ---- the public hooks' internals (life.h: scripts and tests)
  static int taskOfActor(const Game& g, int id) {
    if (!g.lifeRt_) return -1;
    auto it = g.lifeRt_->act.find(id);
    const int k = g.findActor(id);
    if (it == g.lifeRt_->act.end() || k <= 0 || g.actors[(size_t)k].resident < 0) return -1;
    return it->second.task;
  }
  static const life::StreetStats& streetOf(const Game& g) {
    static const life::StreetStats none;
    return g.lifeRt_ ? g.lifeRt_->street : none;
  }
  static life::IncidentView incidentView(const Game& g) {
    life::IncidentView v;
    if (!g.lifeRt_) return v;
    const RT::Incident& I = g.lifeRt_->inc;
    v.kind = I.kind; v.victim = I.victim; v.other = I.other; v.quest = I.quest; v.engaged = I.engaged; v.gx = I.gx; v.gy = I.gy; v.ex = I.ex; v.ey = I.ey; v.holdT = I.holdT;
    return v;
  }
  static uint32_t forceGossip(Game& g) {
    if (g.inside) return 0;
    // the nearest resident in the street near the player is the one who lets it slip
    const Actor* best = nullptr;
    float bd = (10.0f * TILE) * (10.0f * TILE);
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& a = g.actors[k];
      if (!(a.lifeBits & LB_LIFE) || a.resident < 0 || a.st == AState::Dead) continue;
      const float d = len2(a.p - g.pl().p);
      if (d < bd) { bd = d; best = &a; }
    }
    if (!best) return 0;
    const Actor speaker = *best;
    return overheard(g, speaker, rt(g).rt(speaker.id).partner, true);
  }
  static std::string debugFolk(const Game& g, int id) {
    if (!g.lifeRt_) return "";
    auto it = g.lifeRt_->act.find(id);
    if (it == g.lifeRt_->act.end()) return "";
    const RT::ARt& A = it->second;
    char b[200];
    snprintf(b, sizeof b, "kind %d task %d leg %d arrived %d fails %d retry %d target %d,%d (local %d,%d) dwell %.1f mood %d partner %d path %d",
             (int)A.kind, (int)A.task, (int)A.leg, (int)A.arrived, A.fails, (int)A.retry, A.gx, A.gy, A.gx - g.world.ox, A.gy - g.world.oy, A.dwell, (int)A.mood,
             A.partner, (int)(A.path.size() / 2)); if (A.slip) snprintf(b + strlen(b), sizeof b - strlen(b), " SLIP to %.0f,%.0f", A.slipTo.x / TILE, A.slipTo.y / TILE);
    return b;
  }
  static bool debugTalk(Game& g, int id) {
    const int k = g.findActor(id);
    if (k <= 0) return false;
    g.talkTo(g.actors[(size_t)k]);
    return g.mode == Mode::Dialogue;
  }

  // ---------------------------------------------------------------- dialogue (15.12 hooks)
  enum DlgL { L_FEED = DLG_LIFE + 1, L_COIN, L_EMPLOY, L_SUPPLY, L_CHAT, L_MEAL, L_PET, L_HELP };
  static uint64_t markOf(const Game& g, int si, int idx, int tag) {
    return markKey(life::npcId(g.world.sites[(size_t)si].id, idx), (Mk)(80 + tag));
  }
  static int foodIndex(const Game& g) {
    for (int i = 0; i < (int)g.inv.size(); i++) if (g.inv[(size_t)i].kind == ItemKind::Food) return i;
    return -1;
  }
  // (fixer M5 r3) the town's food stock (what the inn's pot is filled from) and one serving taken from it
  static int townFood(const life::Census& c) {
    static const ew::Good foods[5] = {ew::Good::Bread, ew::Good::Meat, ew::Good::Fish, ew::Good::Produce, ew::Good::Grain};
    int n = 0;
    for (ew::Good f : foods) n += c.stock[(size_t)f];
    return n;
  }
  static void takeFood(life::Census& c) {
    static const ew::Good foods[5] = {ew::Good::Bread, ew::Good::Meat, ew::Good::Fish, ew::Good::Produce, ew::Good::Grain};
    size_t best = (size_t)foods[0];
    for (ew::Good f : foods) if (c.stock[(size_t)f] > c.stock[best]) best = (size_t)f;
    if (c.stock[best] > 0) c.stock[best]--;
  }
  // the inn's dish in the people's own kitchen (cult::Archetype order), thinner in a hungry town
  static std::string mealLine(int arch, uint16_t mf) {
    static const char* dish[] = {
        "FISH STEW, RYE BREAD AND A HORN OF MEAD.",                  // fjordfolk
        "MUTTON BROTH, OATCAKES AND A DRAM AGAINST THE COLD.",        // highland
        "STEW, BLACK BREAD AND A MUG OF THE GOOD STUFF.",             // heartland
        "LENTILS, A CUT OF ROAST AND A CUP OF WATERED WINE.",         // imperial
        "SPICED LAMB, FLATBREAD, DATES AND MINT TEA.",                // dune
        "BOILED MUTTON, NOODLES IN BROTH AND A BOWL OF KUMIS.",       // steppe
        "EEL STEW, A RICE CAKE AND A CUP OF REED BEER.",              // marsh
        "RICE, PICKLED GREENS, A LITTLE PORK AND A POT OF TEA.",      // jade
        "RIVER FISH, BARLEY BREAD AND A JUG OF BEER.",                // river
        "MAIZE CAKES, BEANS, PEPPERS AND A CUP OF CHOCOLATE.",        // sun temple
        "NUT BREAD, MUSHROOMS, BERRIES AND A CUP OF FLOWER WINE.",    // sylvan
        "WHITE BREAD, HONEYED FRUIT AND A GLASS OF STARWINE."};       // starspire
    const int n = (int)(sizeof(dish) / sizeof(dish[0]));
    std::string t = arch >= 0 && arch < n ? dish[arch] : dish[2];
    if (mf & (life::MF_FAMINE | life::MF_HUNGRY)) return "A SMALL BOWL, I'M AFRAID. " + t + " WHAT THERE IS OF IT. FOOD'S SHORT IN TOWN.";
    return t + " THAT'LL KEEP YOU GOING TILL TONIGHT.";
  }
  static std::string needBark(const life::Census& c, const life::Resident& r) {
    const uint32_t h = mixu(c.seed ^ r.idx, 0xBA4Cull);
    if (r.flags & life::RF_GRIEVING) return "WE BURIED ONE OF OUR OWN THIS WEEK. FORGIVE ME IF I'M POOR COMPANY.";
    if (r.need[(int)life::Need::Hunger] < 25) return h % 2 ? "I HAVEN'T EATEN SINCE YESTERDAY." : "MY BELLY THINKS MY THROAT'S BEEN CUT.";
    if (r.need[(int)life::Need::Money] < 20) return "NOT TWO COINS TO RUB TOGETHER, THESE DAYS.";
    if (r.need[(int)life::Need::Rest] < 20) return "I COULD SLEEP FOR A WEEK.";
    if (r.need[(int)life::Need::Social] < 20) return "NOBODY'S STOPPED TO TALK WITH ME ALL DAY. THANK YOU.";
    if (r.need[(int)life::Need::Faith] < 20) return "I OUGHT TO GET MYSELF TO THE TEMPLE.";
    return "";
  }
  // the short street bark of a pressing need ("" when content)
  static std::string shortBark(const life::Census& c, const life::Resident& r) {
    if (r.flags & life::RF_GRIEVING) return "...";
    if (r.need[(int)life::Need::Hunger] < 25) return "HAVEN'T EATEN SINCE YESTERDAY...";
    if (r.need[(int)life::Need::Money] < 15) return "NOT A COIN TO MY NAME...";
    if (r.need[(int)life::Need::Rest] < 15) return "SO TIRED...";
    if (c.moodFlags & life::MF_FAMINE) return "NO BREAD AGAIN TODAY...";
    if (c.moodFlags & life::MF_FESTIVAL) return "FEAST TONIGHT!";
    return "";
  }
  static std::string moodLine(const life::Census& c, const std::string& town) {
    const uint16_t f = c.moodFlags;
    if (f & life::MF_FAMINE) return "THERE'S NO BREAD IN " + town + ". PEOPLE ARE LEAVING.";
    if (f & life::MF_HUNGRY) return "THE STORES ARE NEARLY EMPTY AND BREAD COSTS DOUBLE.";
    if (f & life::MF_WARTORN) return "THE WAR HAS TAKEN SO MUCH FROM US.";
    if (f & life::MF_FESTIVAL) return "IT'S FEAST DAY! COME TO THE HALL TONIGHT.";
    if (f & life::MF_GRIEF) return "THE WHOLE TOWN IS IN MOURNING.";
    if (c.mood >= 70) return "GOOD TIMES IN " + town + ". FULL BELLIES, FULL HALL.";
    return "";
  }
};

// ================================================================= the hooks
void Game::lifeStep(float dt) {
  LifeOps::RT& R = LifeOps::rt(*this);
  const double t0 = nowMs();
  R.pathBudget = 2;
  R.thinkBudget = 40;
  if (R.barkT > 0) R.barkT -= dt;
  // the clock: a jump (a script's `hour`, a night's sleep, a load) places everyone afresh
  const double abs = LifeOps::absHour(*this);
  const bool jump = R.lastAbsH >= 0 && (abs < R.lastAbsH - 1e-4 || abs - R.lastAbsH > 0.2);
  R.lastAbsH = abs;
  if (jump) R.sayLater.clear();
  if (jump && R.inc.kind) LifeOps::endIncident(*this);   // (P1: the street's moment passed with the hours)
  // the 15.2 buffs: tell the player when one comes or goes
  {
    const uint8_t b = life.player.buffs();
    const uint8_t gained = (uint8_t)(b & ~R.lastBuffs), lost = (uint8_t)(R.lastBuffs & ~b);
    if (R.stats.steps > 0) {
      if (gained & life::BUFF_WELLFED) emit(Ev::Notice, pl().p, (int)rgba(240, 200, 110), 0, "WELL FED: STAMINA AND HEALTH COME BACK FASTER");
      if (gained & life::BUFF_RESTED) emit(Ev::Notice, pl().p, (int)rgba(150, 200, 255), 0, "RESTED: +10% EXPERIENCE");
      if (gained & life::BUFF_HUNGRY) emit(Ev::Notice, pl().p, (int)rgba(220, 160, 110), 0, "HUNGRY: STAMINA COMES BACK SLOWER. EAT FOOD (ITEMS) OR A MEAL AT AN INN");
      if (gained & life::BUFF_WEARY) emit(Ev::Notice, pl().p, (int)rgba(170, 160, 200), 0, "WEARY: FIND A BED AND SLEEP");
      if ((lost & life::BUFF_WELLFED) && !(b & life::BUFF_HUNGRY)) emit(Ev::Notice, pl().p, (int)rgba(200, 190, 170), 0, "NO LONGER WELL FED");
    }
    R.lastBuffs = b;
  }
  // gone: walked in at a door, out of the room, off the map
  for (size_t k = actors.size(); k-- > 1;)
    if (actors[k].lifeBits & LB_GONE) LifeOps::removeActorAt(*this, k);
  if (inside) {
    // (out again, the street is placed afresh: as it is at that hour)
    for (auto& kv : R.sites) { kv.second.active = false; kv.second.crittersOut = false; }
    LifeOps::interiorTick(*this, dt);
    if (jump && subBldg >= 0) {
      // a jump of the clock indoors: the room as it is at the new hour
      for (size_t k = actors.size(); k-- > 1;)
        if (actors[k].lifeBits & 1u) LifeOps::removeActorAt(*this, k);
      LifeOps::populate(*this);
    }
  } else if (world.endless) {
    // the active settlements' people, each reconciled twice a second (staggered), afresh when it just came in
    for (int si : activeSites_) {
      if (si < 0 || si >= (int)world.sites.size() || !world.sites[(size_t)si].settlement()) continue;
      if (!LifeOps::censusOfSite(*this, si)) continue;
      auto& S = LifeOps::siteRt(*this, si);
      const bool fresh = !S.active || jump;
      S.active = true;
      S.reconcileT -= dt;
      if (fresh || S.reconcileT <= 0) {
        S.reconcileT = 0.5f + (float)(si % 5) * 0.03f;
        LifeOps::reconcile(*this, si, fresh);
        LifeOps::critters(*this, si);
      }
    }
    for (auto& kv : R.sites) {
      if (!kv.second.active) continue;
      const int h = world.siteHandle(kv.first);
      if (h < 0 || !activeSites_.count(h)) { kv.second.active = false; kv.second.crittersOut = false; }
    }
    // (P1) the street's incidents: the hour's decided and staged near the player, the live one followed to its end
    LifeOps::incidentTick(*this, dt);
  }
  if (inside && R.inc.kind) {   // (in at a door: the street's moment passes without them: a lead, or the errand lost)
    if (R.inc.engaged) LifeOps::finishIncident(*this, false);
    else LifeOps::incidentLead(*this, nullptr);
    LifeOps::endIncident(*this);
  }
  // the per-actor state of actors no longer in play
  if ((R.stats.steps & 63) == 0) {
    std::unordered_set<int> ids;
    for (const Actor& a : actors) ids.insert(a.id);
    for (const Actor& a : sheltered_) ids.insert(a.id);
    for (auto it = R.act.begin(); it != R.act.end();) {
      if (ids.count(it->first)) { ++it; continue; }
      LifeOps::releaseSpot(*this, it->second, it->first);
      it = R.act.erase(it);
    }
  }
  // stats
  LifeFolkStats& s = R.stats;
  s.residents = s.walking = s.seated = s.working = s.sleeping = s.critters = 0;
  int stuck = 0;
  for (size_t k = 1; k < actors.size(); k++) {
    const Actor& a = actors[k];
    if (a.critter) { s.critters++; continue; }
    if (a.resident < 0) continue;
    s.residents++;
    if (a.st == AState::Walk) s.walking++;
    if (LifeOps::isSeated(a.posture)) s.seated++;
    if (a.posture == Posture::Sleep) s.sleeping++;
    if (a.posture == Posture::Hammer || a.posture == Posture::Hoe || a.posture == Posture::Chop || a.posture == Posture::Fish || a.posture == Posture::Stir ||
        a.posture == Posture::Sweep || a.posture == Posture::Read)
      s.working++;
    auto it = R.act.find(a.id);
    if (it != R.act.end() && it->second.stuck) stuck++;
  }
  s.stuck = stuck;
  const double ms = nowMs() - t0 + R.folkMs;
  R.folkMs = 0;
  s.steps++;
  s.lastMs = ms;
  s.worstMs = std::max(s.worstMs, ms);
  s.sumMs += ms;
}

bool Game::lifeFolk(Actor& a, float dt) {
  if (a.lifeBits & LB_HOMESTEAD) return HomeOps::folk(*this, a, dt);   // (M7) the player's farm animals, horse, builders, farmhand
  if (!(a.lifeBits & (LB_LIFE | LB_CRITTER | LB_KEY | LB_GUEST | LB_THIEF))) return false;
  LifeOps::RT& R = LifeOps::rt(*this);
  const double t0 = nowMs();
  bool done;
  if (a.lifeBits & LB_THIEF) done = inside ? false : LifeOps::thiefFolk(*this, a, dt);
  else if (a.critter) done = LifeOps::critterFolk(*this, a, dt);
  else if (inside) done = LifeOps::interiorFolk(*this, a, dt);
  else if (a.lifeBits & LB_LIFE) done = LifeOps::streetFolk(*this, a, dt);
  else done = false;
  R.folkMs += nowMs() - t0;
  return done;
}

bool Game::lifeSpawnAllowed(int site, const Spawn& sp) {
  // the street folk of a settlement come from its census now (lifeStep): the generator's villagers, farmers and
  // children stay in; a stall's keeper keeps the stall's hours (home for the night); the watch is the war lane's
  if (!sp.npc || sp.bandit) return true;
  if (sp.role == Role::Guard || sp.role == Role::Herald || sp.role == Role::Soldier || sp.role == Role::Captain || sp.role == Role::King) return true;
  if (!LifeOps::censusOfSite(*this, site)) return true;
  // (P1 Fresh Roads: an open quest's giver or a story's person no longer keeps the generator's place: the census's
  //  resident the spawn is lives their day, and the quest's pin follows them)
  // (M2 owner note 6) a capital's square folk (idlers, a crier, children round the centrepiece) keep the square by day
  // and go home at night (ai.cpp's square-goers): they are the square's own, besides the census's passers-by
  if ((sp.role == Role::Villager || sp.role == Role::Child) && world.sites[(size_t)site].type == SiteType::City && world.over.at(sp.x, sp.y) == Ground::Plaza)
    return true;
  if (sp.role == Role::Merchant) return hour >= 6.5f && hour < 21.5f;
  return false;
}

void Game::lifeSpawned(Actor& a, const Spawn& sp) {
  if (!inside) return;   // (the street: stall keepers keep the generator's ways)
  if (subBldg < 0) return;
  const Bldg& B = world.over.bldgs[(size_t)subBldg];
  if (B.site < 0 || B.site >= (int)world.sites.size() || !world.sites[(size_t)B.site].settlement()) return;
  life::Census* c = life.census(world, B.site);
  if (!c) return;
  const int off = subBldg - world.sites[(size_t)B.site].bldgFirst;
  if (sp.role == Role::Guard || sp.role == Role::King || sp.role == Role::Jarl) {   // the watch and the court stay
    if (sp.slot == 0) {
      const int ri = life::spawnResident(*c, sp, off);
      if (ri >= 0) { a.resident = ri; c->res[(size_t)ri].actor = a.id; }
      a.lifeBits |= LB_KEY;
    }
    return;
  }
  if (sp.slot == 0) {
    // the building's key person: at their post in working hours (the innkeeper always), else elsewhere
    const int ri = life::spawnResident(*c, sp, off);
    if (ri < 0) { a.lifeBits |= LB_KEY; return; }
    life::Resident& r = c->res[(size_t)ri];
    const bool mage = B.type == art::Building::Tower;
    // (P1 Fresh Roads: an open quest no longer keeps its giver at the post out of hours: they live their day, and the
    //  tracked quest's pin follows them, lifeGiverSpot)
    if (!mage && !LifeOps::keyAtPost(*this, *c, r)) {
      // a home's head at home this hour is placed by the schedule instead (in bed at night, at the table...)
      a.lifeBits |= LB_DROP;
      return;
    }
    a.resident = ri;
    r.actor = a.id;
    a.altBldg = -1; a.altSlot = LIFE_SLOT0 + ri;   // (its street identity: spawnResident's)
    if (sp.role != Role::Priest && sp.role != Role::Mage) a.name = r.name;
    if (sp.role == Role::Priest) a.name = "PRIEST " + r.name;
    if (r.job == life::Job::Beggar) a.look.outfit = art::Outfit::Rags;
    a.lifeBits |= LB_KEY;
    return;
  }
  // every other generator spawn: inn travellers upstairs stay (not residents); the rest are the census's people
  if (B.type == art::Building::Inn && subFloor > 0 && sp.role == Role::Villager) {
    if (hour >= 9.0f && hour < 19.0f) a.lifeBits |= LB_DROP;   // out on the road by day
    else a.lifeBits |= LB_GUEST;
    return;
  }
  a.lifeBits |= LB_DROP;
}

void Game::lifeInterior() {
  if (!inside || subBldg < 0) return;
  if (home::houseEmpty(world, this, subBldg)) return;   // (M7) nobody lives in a vacant house or the player's own
  LifeOps::populate(*this);
}

void Game::lifeTalk(Actor& a) {
  const int si = a.site >= 0 ? a.site : (a.bldg >= 0 && a.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)a.bldg].site : -1);
  life::Census* c = si >= 0 && si < (int)world.sites.size() && world.sites[(size_t)si].settlement() ? life.findMut(world.sites[(size_t)si].id) : nullptr;
  // the innkeeper (or a hall's keeper) sells a hot meal: the strongest Well Fed (15.2). (fixer M5 r3) The pot is filled
  // from the town's own food stock (15.12): dearer in a hungry town, dearer still in a famine, and nothing at all when
  // the stock is gone
  if (a.role == Role::Innkeeper) {
    const int food = c ? LifeOps::townFood(*c) : 99;
    const uint16_t mf = c ? c->moodFlags : (uint16_t)life::MF_CONTENT;
    if (food <= 0) dlg.opts.push_back({"ANYTHING TO EAT?", LifeOps::L_MEAL, 0});
    else {
      const int price = (mf & life::MF_FAMINE) ? 20 : (mf & life::MF_HUNGRY) ? 12 : 6;
      dlg.opts.push_back({"A HOT MEAL (" + std::to_string(price) + " GOLD)", LifeOps::L_MEAL, price});
    }
  }
  if (!c || a.resident < 0 || a.resident >= (int)c->res.size()) return;
  life::Resident& r = c->res[(size_t)a.resident];
  const std::string town = world.sites[(size_t)si].name;
  // (P1) a street incident's person: the plea, and the offer to step in (the errand opens with it)
  {
    const LifeOps::RT::Incident& I = LifeOps::rt(*this).inc;
    if (I.kind && !I.engaged && (a.id == I.victim || a.id == I.other)) {
      const char* plea = I.kind == life::INC_THIEF ? "HE'S GOT MY PURSE! THAT WAY, QUICK!"
                         : I.kind == life::INC_CART ? "THE WHEEL'S COME OFF, AND I CAN'T LIFT THE CART ON MY OWN."
                                                    : "STAY OUT OF THIS, STRANGER! THIS IS BETWEEN HIM AND ME.";
      speech::propose(dlg, speech::P_URGENT, speech::Topic::Need, plea);   // (it is happening now: over a tale or a job)
      dlg.opts.insert(dlg.opts.begin(), {I.kind == life::INC_THIEF ? "I'LL CATCH THEM!" : I.kind == life::INC_CART ? "LEND A HAND" : "BREAK IT UP",
                                         LifeOps::L_HELP, 0});
    }
  }
  // a plain townsperson's own words (M7b: proposed, so a reward, an offer or a parcel still speaks over them; talkTo's
  // frame puts a stranger's introduction or an acquaintance's hello in front)
  bool asks = false;
  for (const DlgOpt& o : dlg.opts) if (o.action == A_ASK) asks = true;
  const bool friendOf = (r.flags & life::RF_BEFRIENDED) != 0;
  const std::string hello = friendOf ? std::string(timeGreeting(hour)) + ", FRIEND! " : std::string();
  const bool hard = (c->moodFlags & (life::MF_FAMINE | life::MF_HUNGRY | life::MF_WARTORN | life::MF_GRIEF)) != 0;
  const bool pressing = r.need[(int)life::Need::Hunger] < 25 || (r.flags & life::RF_GRIEVING);
  // (M5) a standing offer of work (A_ASK: an innkeeper, a farmer) still voices the town's need or mood when there is
  // one: the most-visited people in a hungry town must show the famine; with nothing pressing their own line stays
  if ((a.lifeBits & (LB_LIFE | LB_KEY)) && asks && r.job != life::Job::Beggar) {
    const std::string mood = hard ? LifeOps::moodLine(*c, town) : std::string();
    const std::string need = (pressing || mood.empty()) ? LifeOps::needBark(*c, r) : std::string();
    if (!need.empty()) speech::propose(dlg, pressing ? speech::P_NEED : speech::P_BARK, speech::Topic::Need, hello + need);
    else if (!mood.empty()) speech::propose(dlg, speech::P_BARK, speech::Topic::Bark, hello + mood);
  }
  if ((a.lifeBits & LB_LIFE) && !asks) {
    const std::string need = LifeOps::needBark(*c, r);
    const std::string mood = LifeOps::moodLine(*c, town);
    if (r.job == life::Job::Beggar) speech::propose(dlg, speech::P_NEED, speech::Topic::Need, "ALMS FOR THE POOR? A COIN, A CRUST, ANYTHING.");
    else if (!need.empty()) speech::propose(dlg, pressing ? speech::P_NEED : speech::P_BARK, speech::Topic::Need, hello + need);
    else if (!mood.empty() && (hard || !talkMem.saidRecently(speech::lineHash(mood), day, 0)))
      // a good mood is said once a day in a town, not by every person in the street
      speech::propose(dlg, hard ? speech::P_BARK : speech::P_SMALLTALK, speech::Topic::Bark, hello + mood);
    else if (r.job == life::Job::Bard)
      speech::propose(dlg, speech::P_SMALLTALK, speech::Topic::Smalltalk, hello + "COME TO THE HALL TONIGHT. I'VE A NEW SONG, AND IT NEEDS EARS.");
    else if (!hello.empty())
      speech::propose(dlg, speech::P_SMALLTALK, speech::Topic::Smalltalk, hello + "IT'S ALWAYS GOOD TO SEE YOU IN " + town + ".");
  }
  // the player's hooks (15.12)
  const int food = LifeOps::foodIndex(*this);
  const bool adult = r.job != life::Job::Child;
  if (food >= 0 && (r.need[(int)life::Need::Hunger] < 70 || r.job == life::Job::Beggar))
    dlg.opts.push_back({"SHARE YOUR " + inv[(size_t)food].name, LifeOps::L_FEED, food});
  if (r.job == life::Job::Beggar && gold >= 2) dlg.opts.push_back({"SPARE A COIN (2 GOLD)", LifeOps::L_COIN, 2});
  if (adult && r.keySlot != 0 && r.job != life::Job::Beggar && r.job != life::Job::Noble && r.need[(int)life::Need::Money] < 60) {
    auto it = marks.find(LifeOps::markOf(*this, si, r.idx, 2));
    if (it == marks.end() || it->second != day) dlg.opts.push_back({"HIRE A DAY'S HELP (12 GOLD)", LifeOps::L_EMPLOY, 12});
  }
  if ((a.role == Role::Merchant || a.role == Role::Innkeeper) && food >= 0 && (c->moodFlags & (life::MF_HUNGRY | life::MF_FAMINE)))
    dlg.opts.push_back({"SELL YOUR FOOD TO THE STORES", LifeOps::L_SUPPLY, food});
  if (adult && !(r.flags & life::RF_BEFRIENDED)) {
    auto it = marks.find(LifeOps::markOf(*this, si, r.idx, 1));
    if (it == marks.end() || it->second != day) dlg.opts.push_back({"HOW ARE YOU KEEPING?", LifeOps::L_CHAT, 0});
  }
}

int gsim::lifeOptKind(int action) {
  switch (action) {
    case LifeOps::L_MEAL: case LifeOps::L_SUPPLY: return 1;
    case LifeOps::L_FEED: case LifeOps::L_COIN: case LifeOps::L_EMPLOY: return 2;
    case LifeOps::L_CHAT: return 3;
    default: return 0;
  }
}

bool Game::lifeChoose(const DlgOpt& o) {
  const int ai = findActor(dlg.actor);
  if (o.action == LifeOps::L_MEAL) {
    // the keeper's town and people (the pot's stock, the dish's words)
    int msi = -1;
    if (ai >= 0) {
      const Actor& k = actors[(size_t)ai];
      msi = k.site >= 0 ? k.site : (k.bldg >= 0 && k.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)k.bldg].site : -1);
    }
    if (msi < 0 && inside && subBldg >= 0 && subBldg < (int)world.over.bldgs.size()) msi = world.over.bldgs[(size_t)subBldg].site;
    life::Census* mc = msi >= 0 && msi < (int)world.sites.size() && world.sites[(size_t)msi].settlement() ? life.findMut(world.sites[(size_t)msi].id) : nullptr;
    if (o.arg <= 0 || (mc && LifeOps::townFood(*mc) <= 0)) {
      dlg.text = "NOTHING IN THE POT, FRIEND. NOT A CRUST IN THE TOWN TO PUT IN IT. IF YOU CAN BRING FOOD IN, PEOPLE WILL BLESS YOU FOR IT.";
      dlg.opts = {{"I UNDERSTAND.", A_BYE, 0}};
      return true;
    }
    if (gold < o.arg) { dlg.text = "THAT'S " + std::to_string(o.arg) + " GOLD FOR THE MEAL, FRIEND."; return true; }
    gold -= o.arg;
    sfx((int)Sfx::Coin, pl().p);
    if (mc) LifeOps::takeFood(*mc);
    deed(party::Deed::Tavern, mc ? mc->site : 0);   // M8: a meal at the inn (the companions eat too)
    life::PlayerNeeds& n = life.player;
    n.sinceMealH = 0;
    n.mealQuality = 3;
    n.fedH = std::max(n.fedH, 9.0f);
    pl().hp = std::min(pl().maxHp, pl().hp + 30.0f);
    const cult::Culture* cu = msi >= 0 ? world.cultureOf(msi) : nullptr;
    dlg.text = LifeOps::mealLine(cu ? (int)cu->archetype : -1, mc ? mc->moodFlags : (uint16_t)life::MF_CONTENT);
    dlg.opts = {{"THANK YOU.", A_BYE, 0}};
    return true;
  }
  if (ai < 0) return false;
  Actor& a = actors[(size_t)ai];
  if (o.action == LifeOps::L_HELP) {   // (P1) stepping in to a street incident: the errand opens on the spot
    LifeOps::RT& R = LifeOps::rt(*this);
    if (!R.inc.kind || R.inc.engaged || (a.id != R.inc.victim && a.id != R.inc.other)) {
      dlg.text = "NEVER MIND. IT'S OVER NOW.";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return true;
    }
    if (R.inc.kind == life::INC_BRAWL) {
      LifeOps::engageIncident(*this, a);
      dlg.text = "ALL RIGHT, ALL RIGHT... WE'RE DONE. NO NEED FOR THAT.";
    } else {
      LifeOps::openIncidentErrand(*this, a);
      dlg.text = R.inc.kind == life::INC_THIEF ? "THANK YOU! HE WENT THAT WAY, RUNNING LIKE A HARE!"
                                               : "BLESS YOU. GET YOUR SHOULDER UNDER THE SIDE THERE, AND HOLD IT STEADY WHILE I KNOCK THE WHEEL ON.";
    }
    dlg.opts = {{"GO.", A_BYE, 0}};
    return true;
  }
  const int si = a.site >= 0 ? a.site : (a.bldg >= 0 && a.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)a.bldg].site : -1);
  if (si < 0) return false;
  life::Census* c = life.findMut(world.sites[(size_t)si].id);
  if (!c || a.resident < 0 || a.resident >= (int)c->res.size()) return false;
  life::Resident& r = c->res[(size_t)a.resident];
  auto kindness = [&](int amount) {
    int& k = marks[LifeOps::markOf(*this, si, r.idx, 0)];
    k += amount;
    if (k >= 3 && !(r.flags & life::RF_BEFRIENDED) && life.befriend(c->site, r.idx)) {
      emit(Ev::Notice, pl().p, (int)rgba(240, 150, 170), 0, "YOU HAVE A FRIEND IN " + world.sites[(size_t)si].name + ": " + r.name);
      deed(party::Deed::Befriended, life::npcId(c->site, r.idx));   // M8
      LifeOps::bubble(a, Bubble::Heart, 2.5f);
    }
  };
  switch (o.action) {
    case LifeOps::L_FEED: {
      const int fi = o.arg >= 0 && o.arg < (int)inv.size() && inv[(size_t)o.arg].kind == ItemKind::Food ? o.arg : LifeOps::foodIndex(*this);
      if (fi < 0) { dlg.text = "YOU HAVE NOTHING TO SHARE."; return true; }
      const int q = inv[(size_t)fi].power >= 25 ? 2 : 1;
      const std::string what = inv[(size_t)fi].name;
      if (--inv[(size_t)fi].count <= 0) {
        inv.erase(inv.begin() + fi);
        for (int* s : {&eqWeapon, &eqBow, &eqStaff, &eqArmor, &eqHelmet, &eqShield, &eqRing, &eqAmulet, &eqGloves, &eqBoots, &eqCloak})
          if (*s > fi) (*s)--;
      }
      life.feed(c->site, r.idx, q);
      if (r.flags & life::RF_REFUGEE) deed(party::Deed::HelpedRefugees, c->site, 2);   // M8 (FELLOWS): the war's homeless fed
      else deed(party::Deed::Charity, c->site, r.job == life::Job::Beggar ? 2 : 1);   // M8
      LifeOps::bubble(a, Bubble::Heart, 2.0f);
      kindness(r.job == life::Job::Beggar ? 2 : 1);
      dlg.text = r.job == life::Job::Beggar ? "BLESS YOU. BLESS YOU! THE FIRST THING I'VE EATEN IN DAYS." : what + " TO SHARE? THAT'S KIND OF YOU. I WON'T FORGET IT.";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return true;
    }
    case LifeOps::L_COIN: {
      if (gold < o.arg) { dlg.text = "YOU HAVE NOTHING TO GIVE."; return true; }
      gold -= o.arg;
      r.coin = (uint16_t)std::min(60000, r.coin + o.arg);
      deed((r.flags & life::RF_REFUGEE) ? party::Deed::HelpedRefugees : party::Deed::Charity, c->site);   // M8
      r.need[(int)life::Need::Money] = (uint8_t)std::min(100, r.need[(int)life::Need::Money] + 15);
      sfx((int)Sfx::Coin, a.p, 1.2f);
      LifeOps::bubble(a, Bubble::Coin, 2.0f);
      kindness(1);
      dlg.text = "GODS KEEP YOU, TRAVELLER.";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return true;
    }
    case LifeOps::L_EMPLOY: {
      if (gold < o.arg) { dlg.text = "YOU CAN'T PAY ME THAT."; return true; }
      gold -= o.arg;
      life.employ(c->site, r.idx, day);
      marks[LifeOps::markOf(*this, si, r.idx, 2)] = day;
      sfx((int)Sfx::Coin, a.p);
      kindness(1);
      dlg.text = "A DAY'S PAY FOR A DAY'S WORK? GLADLY. I'LL MEND FENCES AND HAUL WATER FOR THE WHOLE STREET IN YOUR NAME.";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return true;
    }
    case LifeOps::L_SUPPLY: {
      int units = 0, pay = 0;
      for (int i = (int)inv.size(); i-- > 0;) {
        if (inv[(size_t)i].kind != ItemKind::Food) continue;
        units += inv[(size_t)i].count;
        pay += inv[(size_t)i].count * std::max(1, inv[(size_t)i].value);
        inv.erase(inv.begin() + i);
        for (int* s : {&eqWeapon, &eqBow, &eqStaff, &eqArmor, &eqHelmet, &eqShield, &eqRing, &eqAmulet, &eqGloves, &eqBoots, &eqCloak})
          if (*s > i) (*s)--;
      }
      if (!units) { dlg.text = "YOU HAVE NO FOOD TO SELL."; return true; }
      life.supply(c->site, ew::Good::Bread, units);
      giveGold(pay);
      kindness(1);
      dlg.text = "FOOD! " + std::to_string(units) + " PORTIONS FOR THE STORES. THAT'LL FEED A FEW HOUSES THIS WEEK. HERE, " + std::to_string(pay) + " GOLD.";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return true;
    }
    case LifeOps::L_CHAT: {
      marks[LifeOps::markOf(*this, si, r.idx, 1)] = day;
      r.need[(int)life::Need::Social] = (uint8_t)std::min(100, r.need[(int)life::Need::Social] + 20);
      kindness(1);
      // (M7b) the answer is how they really are (their needs, the town's mood), and the ways the player could help come
      // up with it (the kindnesses the first page left out: talkToInner groups them under this question)
      const uint16_t mf = c->moodFlags;
      std::string t;
      if (r.flags & life::RF_GRIEVING) t = "NOT WELL. WE BURIED ONE OF OUR OWN THIS WEEK, AND THE HOUSE IS TOO QUIET.";
      else if (r.need[(int)life::Need::Hunger] < 40) t = "HUNGRY, IF I'M HONEST. THERE'S LITTLE ENOUGH IN THE POT THESE DAYS.";
      else if (mf & life::MF_FAMINE) t = "BADLY. WE WENT TO BED HUNGRY AGAIN, AND HALF THE STREET IS PACKING UP TO LEAVE.";   // (the opening said the famine)
      else if (r.need[(int)life::Need::Money] < 30) t = "SHORT OF COIN, LIKE EVERYONE HERE. BUT I GET BY.";
      else if (r.need[(int)life::Need::Rest] < 30) t = "TIRED. I'VE BEEN ON MY FEET SINCE BEFORE DAWN.";
      else if (mf & life::MF_WARTORN) t = "AS WELL AS ANYONE CAN, WITH THE WAR SO CLOSE.";
      else {
        std::vector<int> fr = life.friendsOf(*c, r.idx);
        t = "OH, GETTING BY. ";
        if (!fr.empty() && fr[0] < (int)c->res.size()) {
          // (M7b integration) a name that reads as a word ("MY FRIEND AND AND I") is not said
          const std::string fn = firstName(c->res[(size_t)fr[0]].name);
          static const char* const kWords[] = {"AND", "OR", "THE", "CAN", "OLD", "I", "A", "AN", "IS", "IT", "TO", "OF", "IN", "ON", "AT", "BUT", "SO", "NO", "YES", "WILL", "MAY", "MAN"};
          bool word = fn.empty();
          for (const char* w : kWords) if (fn == w) word = true;
          t += (word ? std::string("A FRIEND OF MINE") : "MY FRIEND " + fn) + " AND I ARE OFF TO THE " + std::string(c->gathering >= 0 ? "HALL" : "SQUARE") +
               " LATER, IF YOU'RE ABOUT.";
        }
        else t += "IT'S GOOD OF YOU TO ASK. MOST FOLK DON'T.";
      }
      dlg.text = t;
      std::vector<DlgOpt> keep;
      for (const DlgOpt& k : dlg.opts) if (k.grp == 2 || lifeOptKind(k.action) == 2) { DlgOpt k2 = k; k2.grp = 0; keep.push_back(k2); }
      keep.push_back({"FAREWELL.", A_BYE, 0});
      dlg.opts = keep;
      dlg.group = 0;
      return true;
    }
    default: return false;
  }
}

bool Game::lifeGiverOpen(int si, int idx) const {
  if (si < 0 || si >= (int)world.sites.size() || idx < 0) return false;
  const int slot = LIFE_SLOT0 + idx;
  for (const Quest& q : quests)
    if (q.state != QState::Done && q.type != QType::Main && q.giverSite == si && q.giverBldg == -1 && q.giverSlot == slot) return true;
  // (fixer M5 r3) a running story's giver or cast person stays too: emigration or a raid would stall its stage
  const uint64_t key = npcKeyOf(si, -1, slot);
  // (fixer M6b r1) a saga's census resident is bound by its census id (life::npcId), not its street key
  const uint64_t rkey = life::npcId(world.sites[(size_t)si].id, idx);
  for (const story::Instance& in : story.running()) {
    if (in.done || in.failed) continue;
    for (const story::Binding& b : in.cast) if (b.id == key || b.id == rkey) return true;
  }
  return false;
}

void Game::lifeKill(const Actor& victim, int killer) {
  (void)killer;
  if (victim.critter || victim.resident < 0) return;
  const int si = victim.site >= 0 ? victim.site
                 : (victim.bldg >= 0 && victim.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)victim.bldg].site : -1);
  if (si < 0 || si >= (int)world.sites.size()) return;
  life.residentDied(world.sites[(size_t)si].id, victim.resident, day);
  // (M5) a giver's own open jobs die with them: nobody is left to hand them in to (no reward; the log says why)
  for (Quest& q : quests)
    if (q.state != QState::Done && q.type != QType::Main && isGiver(q, victim)) {
      q.state = QState::Done;
      for (int i = (int)inv.size() - 1; i >= 0; i--) if (inv[(size_t)i].kind == ItemKind::Quest && inv[(size_t)i].questId == q.id) dropItem(i);
      emit(Ev::QuestUpdate, pl().p, q.id, 2, "QUEST FAILED: " + q.title + " (" + victim.name + " IS DEAD)");
      if (trackedQuest == q.id) trackedQuest = -1;
    }
}

bool Game::lifeUseProp(art::Prop p, int tx, int ty) {
  (void)tx; (void)ty;
  LifeOps::RT& R = LifeOps::rt(*this);
  // no sleeping with a fight close by (the same rule as travel: a bedroll by the road, a bed in a raided house)
  if (p == Prop::Bedroll || p == Prop::Bed || p == Prop::Hammock || p == Prop::SleepingMat)
    for (const Actor& a : actors)
      if (a.hostile && a.aggro && a.st != AState::Dead && len2(a.p - pl().p) < 120 * 120) {
        say("YOU CANNOT SLEEP WITH ENEMIES NEARBY");
        return true;
      }
  if (p == Prop::Bedroll) {   // a wayfarer's bedroll by the embers: the night passes, rested (a little less than a bed)
    R.sleepQ = 7.0f;
    rest(hoursToMorningLife(hour));
    say("YOU SLEEP BY THE EMBERS UNDER THE STARS.");
    return true;
  }
  if (p == Prop::Bed) R.sleepQ = inside && subBldg >= 0 && world.over.bldgs[(size_t)subBldg].type == art::Building::Inn ? 12.0f : 10.0f;
  if (p == Prop::Hammock || p == Prop::SleepingMat) R.sleepQ = 9.0f;
  return false;
}

void Game::lifeAte(const Item& food) {
  // a snack (an apple, bread) gives a short Well Fed; a hearty dish longer (15.2: the inn's cooked meal is best)
  life::PlayerNeeds& n = life.player;
  const int q = food.power >= 28 ? 2 : 1;
  n.sinceMealH = 0;
  n.mealQuality = (uint8_t)std::max<int>(q, n.fedH > 0 ? n.mealQuality : 0);
  n.fedH = std::max(n.fedH, q >= 2 ? 6.0f : 3.5f);
}

void Game::lifeSlept(int hours, bool bed) {
  // rest() has already moved the clock: charge the night to the meal clock, then start the day rested
  LifeOps::RT& R = LifeOps::rt(*this);
  life::PlayerNeeds& n = life.player;
  n.fedH = std::max(0.0f, n.fedH - (float)hours);
  n.sinceMealH = std::min(1000.0f, n.sinceMealH + (float)hours);
  life.resync(day, hour);
  // (an inn's rented room upstairs is the best night's sleep there is)
  if (inside && subBldg >= 0 && world.over.bldgs[(size_t)subBldg].type == art::Building::Inn && subFloor > 0) R.sleepQ = std::max(R.sleepQ, 12.0f);
  if (bed && hours >= 4) { n.sinceSleepH = 0; n.restedH = std::max(n.restedH, R.sleepQ); }
  R.sleepQ = 10.0f;
}

float Game::lifeStaminaRegenMul() const {
  const uint8_t b = life.player.buffs();
  float m = 1.0f;
  if (b & life::BUFF_WELLFED) m *= life.player.mealQuality >= 3 ? 1.3f : 1.25f;
  if (b & life::BUFF_HUNGRY) m *= 0.85f;
  if (b & life::BUFF_WEARY) m *= 0.9f;
  return m;
}
float Game::lifeHealthRegenMul() const { return (life.player.buffs() & life::BUFF_WELLFED) ? 1.2f : 1.0f; }
float Game::lifeXpMul() const { return (life.player.buffs() & life::BUFF_RESTED) ? 1.1f : 1.0f; }

const Game::LifeFolkStats& Game::lifeFolkStats() const {
  static const LifeFolkStats none;
  return lifeRt_ ? lifeRt_->stats : none;
}

// (M8 lead) a census resident's street look on any actor (a companion recruited from the census keeps the face, the dress
// and the name the town knew): the TOWNSFOLK rules of spawnResident / dressResident, without spawning anyone
bool Game::dressAsResident(Actor& a, ew::Gid site, int idx) {
  const int si = world.siteHandle(site);
  const life::Census* c = si >= 0 ? life.find(site) : nullptr;
  if (!c || idx < 0 || idx >= (int)c->res.size()) return false;
  const life::Resident& r = c->res[(size_t)idx];
  const int keepSite = a.site, keepSlot = a.slot, keepBldg = a.bldg;
  a.site = si;   // makeLook dresses by the site's culture
  // (M8 integration) the street body's identity (spawnResident: bldg -1, slot LIFE_SLOT0 + idx): the culture dress is
  // seeded by them, so without it a recruit's face, people and hat changed the moment they joined
  a.slot = LIFE_SLOT0 + idx;
  a.bldg = -1;
  a.role = life::jobRole(r.job);
  if (a.role == Role::Guard) a.role = Role::Villager;
  LifeOps::dressResident(*this, a, r);
  a.site = keepSite; a.slot = keepSlot; a.bldg = keepBldg;
  return true;
}

// ================================================================= (M8 FELLOWS) a companion's body on the furniture
// of the player's own house (game_internal.h): the TOWNSFOLK fit rules (the head on the pillow, the seat line on the
// seat), without the townsfolk's seat reservations (nobody of the census lives in the player's house)
bool lifeLayInBed(Game& g, Actor& a, int x, int y) {
  if (!g.inside || !g.sub.in(x, y)) return false;
  const int pr = g.sub.propAt(x, y);
  if (!pr) return false;
  const Prop b = (Prop)(pr - 1);
  if (b != Prop::Bed && b != Prop::BunkBed && b != Prop::SleepingMat && b != Prop::Bedroll && b != Prop::Hammock) return false;
  const bool longBed = g.sub.propAt(x, y - 1) == (int)Prop::Filler + 1;
  a.p = Vec2(x * TILE + 8.0f, y * TILE + 16.0f + (float)sleepAy(b, longBed, LifeOps::kitOf(g)));
  a.home = a.p; a.goal = a.p;
  a.face = 0;
  a.st = AState::Idle;
  a.posture = Posture::Sleep;
  a.postureT = 0;
  a.useX = x; a.useY = y;
  return true;
}

bool lifeSitOn(Game& g, Actor& a, int x, int y, Posture post) {
  if (!g.inside || !g.sub.in(x, y)) return false;
  const int pr = g.sub.propAt(x, y);
  if (!pr) return false;
  const Prop s = (Prop)(pr - 1);
  if (s != Prop::Chair && s != Prop::Stool && s != Prop::Bench && s != Prop::Cushion) return false;
  // facing the table beside it when there is one
  int pref = 0;
  const int tbl = (int)Prop::Table + 1;
  if (g.sub.propAt(x, y - 1) == tbl) pref = 1;
  else if (g.sub.propAt(x + 1, y) == tbl) pref = 2;
  else if (g.sub.propAt(x - 1, y) == tbl) pref = 3;
  const int faces[4] = {pref, 0, 2, 3};
  for (int f : faces) {
    int ax = 0, ay = 0;
    if (!seatAt(s, f, ax, ay)) continue;
    a.p = Vec2(x * TILE + 8.0f + (float)ax, y * TILE + 16.0f + (float)ay);
    a.home = a.p; a.goal = a.p;
    a.face = f;
    a.st = AState::Idle;
    a.posture = seatedPost(s, post);
    a.postureT = (float)(hash32((uint32_t)a.id * 7u) % 100u) * 0.01f;
    a.useX = x; a.useY = y;
    return true;
  }
  return false;
}

// (P1 Fresh Roads; LIFE lane) where a quest giver is now when the map has no actor for them: the census resident's
// spot by their plan this hour (a street, a stall, the field, the square), else the door of the building their plan has
// them in (home, work, the tavern, the temple). Local tiles (questTarget's). False: not a census resident / unknown.
bool Game::lifeGiverSpot(const Quest& q, int& tx, int& ty) const {
  if (q.giverSite < 0 || q.giverSite >= (int)world.sites.size()) return false;
  const Site& st = world.sites[(size_t)q.giverSite];
  if (!st.settlement() || !st.id) return false;
  const life::Census* c = life.find(st.id);
  if (!c) return false;
  int idx = -1;
  if (q.giverBldg < 0 && q.giverSlot >= LIFE_SLOT0 && q.giverSlot < LIFE_SLOT0 + LIFE_SLOTN) idx = q.giverSlot - LIFE_SLOT0;
  else if (q.giverSlot == 0 && q.giverBldg >= st.bldgFirst && q.giverBldg < st.bldgFirst + st.bldgCount) {
    const int off = q.giverBldg - st.bldgFirst;
    for (const life::Resident& r : c->res)
      if (r.keyBldg == off && r.keySlot == 0) { idx = (int)r.idx; break; }
  }
  if (idx < 0 || idx >= (int)c->res.size()) return false;
  const life::Resident& r = c->res[(size_t)idx];
  if (r.flags & (life::RF_DEAD | life::RF_AWAY)) return false;
  const life::Plan p = life.plan(world, *c, r, day, hour);
  if (p.place == life::Place::Away) return false;
  Game& g = const_cast<Game&>(*this);   // (the street's spot lists are the runtime's cache; nothing of the game changes)
  if (p.bldg < 0 && !LifeOps::keyAtPost(g, *c, r)) {
    LifeOps::RT::SiteRt& S = LifeOps::siteRt(g, q.giverSite);
    int32_t gx, gy;
    int face = 0;
    if (LifeOps::outdoorSpot(g, q.giverSite, S, *c, r, p, gx, gy, face, -2)) {
      tx = gx - world.ox; ty = gy - world.oy;
      if (world.over.in(tx, ty)) return true;
    }
  }
  int off = p.bldg >= 0 ? p.bldg : r.home;
  if (LifeOps::keyAtPost(g, *c, r)) off = r.keyBldg;
  const int b = c->bldgHandle(st, off);
  if (b < 0 || b >= (int)world.over.bldgs.size()) return false;
  tx = world.over.bldgs[(size_t)b].doorX(); ty = world.over.bldgs[(size_t)b].doorY();
  return true;
}

// ================================================================= P1 Fresh Roads: the LIFE lane's public hooks (life.h)
namespace life {
const char* taskName(int t) {
  static const char* n[] = {"SPOT", "STALL", "DELIVER", "SWEEP", "READ", "VISIT", "SHOP"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)TK_COUNT, "a name for every street task");
  return t >= 0 && t < (int)TK_COUNT ? n[t] : "-";
}
int taskOf(const Game& g, int actorId) { return LifeOps::taskOfActor(g, actorId); }
const StreetStats& streetStats(const Game& g) { return LifeOps::streetOf(g); }
IncidentView incidentNow(const Game& g) { return LifeOps::incidentView(g); }
bool forceIncident(Game& g, int kind) {
  const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor((g.pl().p.y - 2) / TILE);
  int si = -1;
  float bd = 1e30f;
  for (int s : g.world.nearSites) {
    if (s < 0 || s >= (int)g.world.sites.size() || !g.world.sites[(size_t)s].settlement()) continue;
    const Site& st = g.world.sites[(size_t)s];
    const float d = (float)((st.ex - px) * (st.ex - px) + (st.ey - py) * (st.ey - py));
    if (d < bd) { bd = d; si = s; }
  }
  if (si < 0 || g.inside) return false;
  return LifeOps::startIncident(g, si, kind);
}
uint32_t forceGossip(Game& g) { return LifeOps::forceGossip(g); }
std::string debugFolk(const Game& g, int actorId) { return LifeOps::debugFolk(g, actorId); }
bool debugTalk(Game& g, int actorId) { return LifeOps::debugTalk(g, actorId); }
bool debugChoose(Game& g, const std::string& labelStart) {
  if (g.mode != Mode::Dialogue) return false;
  for (size_t i = 0; i < g.dlg.opts.size(); i++)
    if (g.dlg.opts[i].label.compare(0, labelStart.size(), labelStart) == 0) { g.dialogueChoose((int)i); return true; }
  return false;
}
}  // namespace life
