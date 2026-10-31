// LifeOps: the TOWNSFOLK lane's internals (a friend of Game), shared by life_game.cpp (the Game::life* hooks) and the
// life_*.cpp files that define its members by cluster (spots, paths, spawn, street, indoors, critters, incidents,
// lines). Private to the lane: only rpg/sim/life_*.cpp include it. Small hot helpers stay defined in the struct.
#pragma once
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
#include "engine/sound_ids.h"
#include "engine/music_style.h"
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/war.h"
#include "rpg/world/economy.h"
#include "rpg/art/art_rig.h"   // (P2b fix) art::rigVersion

using art::Prop;
using art::Posture;
using art::Bubble;

namespace census {   // (P2b FOLK-SIM, rpg/sim/looks.cpp) the census age on a look: child, youth, adult, elder
void ageLook(art::HumanLook& L, int years, bool childRole, bool elder, uint64_t seed);
}  // namespace census

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

inline double nowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
inline uint32_t mixu(uint64_t a, uint64_t b) { return (uint32_t)(ew::mix64(a * 0x9E3779B97F4A7C15ull ^ ew::mix64(b + 0x70F0Cull)) >> 16); }
inline int tileX(Vec2 p) { return (int)std::floor(p.x / TILE); }
inline int tileY(Vec2 p) { return (int)std::floor((p.y - 2) / TILE); }
inline Vec2 tileCentre(int x, int y) { return Vec2(x * TILE + 8.0f, y * TILE + 10.0f); }
inline int faceTo(int dx, int dy) { return std::abs(dx) > std::abs(dy) ? (dx > 0 ? 2 : 3) : (dy > 0 ? 0 : 1); }

inline bool seatProp(Prop p) { return p == Prop::Chair || p == Prop::Stool || p == Prop::Bench || p == Prop::Cushion; }
inline bool tableProp(Prop p) {
  return p == Prop::Table || p == Prop::TableSmall || p == Prop::TableMeal || p == Prop::TableL || p == Prop::TableM || p == Prop::TableR ||
         p == Prop::LowTable || p == Prop::TableWork || p == Prop::Desk;
}
inline bool bedProp(Prop p) { return p == Prop::Bed || p == Prop::BunkBed || p == Prop::Hammock || p == Prop::SleepingMat; }
inline bool counterProp(Prop p) { return p == Prop::Counter || p == Prop::CounterL || p == Prop::CounterM || p == Prop::CounterR; }
inline bool roomPublicHall(RoomKind k) {
  return k == RoomKind::Common || k == RoomKind::Feast || k == RoomKind::TeaRoom || k == RoomKind::Bath || k == RoomKind::Court ||
         k == RoomKind::Parlour;
}
inline bool roomHomeBed(RoomKind k) { return k == RoomKind::Bedroom || k == RoomKind::Cottage || k == RoomKind::OwnerRoom; }

inline const char* timeGreeting(float h) {
  if (h >= 4.5f && h < 12.0f) return "MORNING";
  if (h >= 12.0f && h < 17.5f) return "GOOD DAY";
  if (h >= 17.5f && h < 22.0f) return "EVENING";
  return "NIGHT";
}
inline std::string firstName(const std::string& n) {
  const size_t sp = n.find(' ');
  return sp == std::string::npos ? n : n.substr(0, sp);
}
// a night's sleep lasts until morning (game_rpg.cpp's rule: from the afternoon on you wake around 6:00-7:00)
inline int hoursToMorningLife(float hour) {
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
inline Posture bardPosture(const cult::Culture* cu) {
  if (!cu) return Posture::Lute;
  switch (cu->music.lead) {
    case LeadInst::Flute: case LeadInst::Pipes: case LeadInst::Reed: case LeadInst::Horn: case LeadInst::Brass: case LeadInst::Bells:
    case LeadInst::Voice:
      return Posture::Flute;
    case LeadInst::Marimba: return Posture::Drum;
    default: return Posture::Lute;
  }
}
inline Posture seatedPost(Prop seat, Posture p) {
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
inline bool seatAt(Prop seat, int facing, int& ax, int& ay) {
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
inline int sleepAy(Prop bed, bool longBed, int kit) {
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

  // ---- spots: defined in rpg/sim/life_spots.cpp
  // ---------------------------------------------------------------- plans
  static double absHour(const Game& g) { return g.day * 24.0 + g.hour; }
  static life::Plan planAt(Game& g, const life::Census& c, const life::Resident& r, double abs);
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
  static void scanSpots(Game& g, int si, RT::SiteRt& S);
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
  static bool fieldJob(life::Job j);
  static bool fieldLunch(const Game& g, const life::Resident& r, const life::Plan& p) {
    return p.act == life::Act::Eat && p.place == life::Place::Home && fieldJob(r.job) && g.hour >= 11.0f && g.hour < 14.0f;
  }
  static bool outdoorSpot(Game& g, int si, RT::SiteRt& S, const life::Census& c, const life::Resident& r, const life::Plan& p0,
                          int32_t& gx, int32_t& gy, int& face, int self = -1, uint32_t salt = 0);

  // ---- paths: defined in rpg/sim/life_paths.cpp
  static int stepCost(const Map& m, int x, int y);
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
  static bool findPath(Game& g, int sx, int sy, int tx, int ty, std::vector<int32_t>& out, bool inView = false);
  static void remember(RT& R, uint64_t key, const std::vector<int32_t>& p);

  // is the straight way between two points clear for a body (the walker cuts corners on open ground)
  static bool clearLine(Game& g, Vec2 a, Vec2 b, float r) {
    const Vec2 d = b - a;
    const float l = len(d);
    for (float t = 5.0f; t < l; t += 5.0f)
      if (!g.bodyFree(a + d * (t / l), r, false)) return false;
    return true;
  }

  static bool walkOut(Game& g, Actor& a, RT::ARt& A, float speed, float dt);
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

  // ---- spawn: defined in rpg/sim/life_spawn.cpp
  static void dressResident(Game& g, Actor& a, const life::Resident& r);
  static uint32_t tint(uint32_t c, uint32_t to, float t);
  static void homespun(Actor& a, const life::Resident& r);
  static int spawnResident(Game& g, int si, life::Census& c, int idx, Vec2 at, bool interior);
  static void removeActorAt(Game& g, size_t k);

  static bool onScreen(const Game& g, Vec2 p, float margin = 2.0f) {
    const Vec2 d = p - g.pl().p;
    return std::fabs(d.x) < (g.sleepHalfW + margin) * TILE && std::fabs(d.y) < (g.sleepHalfH + margin) * TILE;
  }

  static void reconcile(Game& g, int si, bool fresh);
  static void stageMood(Game& g, int si, life::Census& c, RT::SiteRt& S);
  static int homeBody(Game& g, int si, life::Census& c, RT::SiteRt& S, int key, int hungerBelow, int salt, int not1 = -1);
  static const RT::Spot* nearSpot(Game& g, RT::SiteRt& S, int32_t nx, int32_t ny);
  static int spawnMood(Game& g, int si, life::Census& c, RT::SiteRt& S, int i, int32_t gx, int32_t gy, uint8_t mood);
  static bool homeBurned(const Game& g, const Site& st, const life::Census& c, const life::Resident& r);
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
  static bool keepOut(Game& g, int si, const life::Resident& r);
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

  static void settleOutdoor(Game& g, Actor& a, RT::ARt& A, const life::Census& c, const life::Resident& r);
  static RT::SiteRt* siteRtById(Game& g, ew::Gid id) {
    auto it = rt(g).sites.find(id);
    return it == rt(g).sites.end() ? nullptr : &it->second;
  }

  // ---- street: defined in rpg/sim/life_street.cpp
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
  static bool streetTrade(const life::Resident& r, const life::Plan& p);
  static bool leisureAct(const life::Plan& p);
  static void breakPair(Game& g, RT::ARt& A, int self) {
    if (A.partner < 0) return;
    RT& R = rt(g);
    auto it = R.act.find(A.partner);
    if (it != R.act.end() && it->second.partner == self) it->second.partner = -1;
    A.partner = -1;
  }
  static int resBldg(const Game& g, int si, const life::Census& c, int off) { return off >= 0 ? c.bldgHandle(g.world.sites[(size_t)si], off) : -1; }
  static bool doorNear(Game& g, int b, int32_t& gx, int32_t& gy, int self);
  static int sourceOf(Game& g, int si, const life::Census& c, const life::Resident& r);
  static int customerDoor(Game& g, int si, const life::Census& c, int32_t gx, int32_t gy, uint32_t h, int notB, int maxD, int self);
  static const RT::Spot* pickNear(Game& g, const std::vector<RT::Spot>& v, uint32_t h, int32_t gx, int32_t gy, int minD, int maxD, int self);
  static void headTo(Game& g, Actor& a, RT::ARt& A, int32_t gx, int32_t gy);
  static void pickTask(Game& g, Actor& a, RT::ARt& A, life::Census& c, const life::Resident& r, const life::Plan& p, RT::SiteRt& S, int32_t seg);
  static bool startLeg(Game& g, Actor& a, RT::ARt& A, life::Census& c, const life::Resident& r, const life::Plan& p, RT::SiteRt& S);
  static std::string wares(const life::Resident& r, uint32_t h);
  static void taskArrive(Game& g, Actor& a, RT::ARt& A, const life::Resident& r);
  static void taskTick(Game& g, Actor& a, RT::ARt& A, life::Census& c, const life::Resident& r, const life::Plan& p, RT::SiteRt& S, float dt);
  static life::Plan fieldAware(const Game& g, const life::Resident& r, const life::Plan& p);
  static const std::vector<RT::Spot>& spotsFor(RT::SiteRt& S, const life::Resident& r, const life::Plan& p);
  static bool plainSight(Game& g, Actor& a, RT::ARt& A, RT::SiteRt& S, const life::Resident& r, const life::Plan& p);
  static bool slipOut(Game& g, const Actor& a, Vec2& out);
  static bool midRoute(Game& g, const Actor& a, Vec2 tgt, uint32_t h, Vec2& out);

  static bool streetFolk(Game& g, Actor& a, float dt);
  static void watch(Game& g, Actor& a, RT::ARt& A, float dt);
  static void retarget(Game& g, Actor& a, RT::ARt& A, life::Census& c, const life::Resident& r, const life::Plan& p);

  static void playTag(Game& g, Actor& a, RT::ARt& A, RT::SiteRt& S, life::Place place);
  static void lampRound(Game& g, Actor& a, RT::ARt& A, RT::SiteRt& S, float dt);
  static void chatOrStroll(Game& g, Actor& a, RT::ARt& A, life::Census& c, const life::Resident& r, RT::SiteRt& S, float dt);
  static void brawl(Game& g, Actor& a, RT::ARt& A, life::Census& c, float dt);

  static void gossip(Game& g, Actor& a, life::Census& c, RT::SiteRt& S);

  static void greetings(Game& g, Actor& a, life::Census& c, const life::Resident& r);

  // ---- indoors: defined in rpg/sim/life_indoors.cpp
  static void scanInterior(Game& g);
  static RoomKind roomKind(const Map& m, int ri) { return ri >= 0 && ri < (int)m.rooms.size() ? m.rooms[(size_t)ri].kind : RoomKind::Hall; }
  static bool inRentedRoom(const Game& g, int x, int y);
  static bool tileFree(Game& g, int x, int y);
  static bool bySitter(Game& g, int x, int y);
  static bool nearPerformer(const Game& g, const Actor& a, int x, int y);
  static void placeAt(Actor& a, int x, int y, int face);
  static bool sitDown(Game& g, Actor& a, std::initializer_list<RoomKind> kinds, Posture post, int nx = -1, int ny = -1);
  static int kitOf(const Game& g) { return g.inside && g.sub.kit ? (int)g.sub.kit - 1 : -1; }
  static bool isSeated(Posture p) {
    return p == Posture::Sit || p == Posture::SitEat || p == Posture::SitDrink || p == Posture::SitFloor || p == Posture::SitFloorEat ||
           p == Posture::SitFloorDrink;
  }
  static bool standAt(Game& g, Actor& a, Prop want, Posture post, std::initializer_list<Prop> alt = {});
  static bool lieDown(Game& g, Actor& a, int bedIdx);
  static bool standFree(Game& g, Actor& a, std::initializer_list<RoomKind> kinds, int nx, int ny, int maxD = 99);

  static bool bathe(Game& g, Actor& a);

  static bool standBeside(Game& g, Actor& a);

  static int bedsOnFloor(Game& g, int bi, int floor);

  static void populate(Game& g);
  static bool placeIndoors(Game& g, Actor& a, life::Census& c, const life::Resident& r, const life::Plan& p, bool gatherHere);
  static void keyPost(Game& g, Actor& a, life::Census* c, const Bldg& B);
  static void guestPlace(Game& g, Actor& a);
  static void pairChats(Game& g);

  static bool interiorFolk(Game& g, Actor& a, float dt);
  static bool serve(Game& g, Actor& a, RT::ARt& A, float dt);

  static bool walkToPlace(Game& g, Actor& a, life::Census& c, const life::Resident& r, const life::Plan& p, bool gatherHere);
  static void goToBed(Game& g, Actor& a, RT::ARt& A);

  static void interiorTick(Game& g, float dt);

  // ---- critters: defined in rpg/sim/life_critters.cpp
  static void critters(Game& g, int si);
  static bool critterFolk(Game& g, Actor& a, float dt);

  // ---- incidents: defined in rpg/sim/life_incidents.cpp
  // ================================================================ P1 Fresh Roads: stumble-on hooks
  // the LIFE lane's Game::marks tags (Mk 144..159): 144 an overheard giver (npcKey): 1 + the week their need was last
  // overheard; 145 a settlement (site id): day * 4 + the overheard leads that day
  static Mk mkLife(int n) { return (Mk)(144 + n); }
  static Actor* actorById(Game& g, int id);
  static std::string callName(const life::Resident* r, const std::string& name, uint32_t h);
  static const life::Resident* residentOf(Game& g, const Actor& a);
  static void leadPlace(Game& g, const Actor& a, jobs::Lead& L);
  static std::string spoken(std::string s);
  static const Mk MK_P1B_GOSSIP_TAG;   // defined in life_game.cpp (story internals stay out of this header)
  static const Mk MK_STORY_GIVER_TAG;   // defined in life_game.cpp (story internals stay out of this header)
  static uint32_t overheardP1b(Game& g, const Actor& speaker, int partnerId, bool force, int32_t today);

  static uint32_t overheard(Game& g, const Actor& speaker, int partnerId, bool force);

  static bool incidentFit(Game& g, const Actor& a, int si);
  static void holdForIncident(Game& g, Actor& a);
  static bool startIncident(Game& g, int si, int kind);
  static bool incidentHeld(Game& g, Actor& a, RT::ARt& A, float dt);
  static void partFrom(Game& g, Actor& a, Vec2 from, float side, float vy);
  static void partBrawlers(Game& g, int ida, int idb, Vec2 from);
  static bool partedHeld(Game& g, Actor& a, RT::ARt& A, float dt);
  static bool thiefFolk(Game& g, Actor& a, float dt);
  static bool openLine(Game& g, Vec2 a, Vec2 b, float r);
  static bool thiefRun(Game& g, Actor& a, RT::ARt& T, float speed, float dt);
  static int incidentTier(const Game& g, int si);
  static int openIncidentErrand(Game& g, const Actor& giver);
  static void finishIncident(Game& g, bool success);
  static void incidentLead(Game& g, Actor* victim);
  static void endIncident(Game& g);
  static void engageIncident(Game& g, Actor& by);
  static void incidentTick(Game& g, float dt);

  // ---- lines: defined in rpg/sim/life_lines.cpp
  static int taskOfActor(const Game& g, int id);
  static const life::StreetStats& streetOf(const Game& g);
  static life::IncidentView incidentView(const Game& g);
  static uint32_t forceGossip(Game& g);
  static std::string debugFolk(const Game& g, int id);
  static bool debugTalk(Game& g, int id);

  // ---------------------------------------------------------------- dialogue (15.12 hooks)
  enum DlgL { L_FEED = DLG_LIFE + 1, L_COIN, L_EMPLOY, L_SUPPLY, L_CHAT, L_MEAL, L_PET, L_HELP };
  static uint64_t markOf(const Game& g, int si, int idx, int tag);
  static int foodIndex(const Game& g);
  static int townFood(const life::Census& c);
  static void takeFood(life::Census& c);
  static std::string mealLine(int arch, uint16_t mf);
  static std::string needBark(const life::Census& c, const life::Resident& r);
  static std::string shortBark(const life::Census& c, const life::Resident& r);
  static std::string moodLine(const life::Census& c, const std::string& town);
};
