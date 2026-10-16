// M10 "Beyond the Mist" (VISION_PLAN 2.12 the sea, 13 M10, 15.4 harbours / boats / fishing, 15.31 the phase A
// contracts): boats and sailing, the wind, storms, ferries, the sea-chart chain, the Mist, first contact, fishing.
//
// SEA lane is the only editor after phase A: it may ADD (types, fields at the end of a struct, functions), never
// rename, remove or change the meaning of what is listed here. The saved block (Sea::serialize) has its own version
// byte: a layout change bumps it (and SAVE_VER with it: old saves are refused, never misread).
//
// The model in one paragraph. A Boat is owned (bought from a harbourmaster / shipwright, or the Sailor's own skiff),
// moored at a port's berth (ew::PortPlan) or left at a tile, and saved. The hero boards it at a pier (interact); while
// aboard (Sea::aboard >= 0) the player actor IS the boat: Actor::swim = 1 (water is ground, land a wall), its speed the
// hull's plus the wind's, the joystick steering (SeaOps::input owns updatePlayer's movement). Shallow water takes a
// rowboat, deep water a sloop, the storm sea a galleon (or a sloop with a keel). Storms are weather fronts that push
// the boat and darken the screen. A hidden land's Mist (ew::EndlessSource::mistAt) turns a boat back after
// MIST_TURN_SECS unless the player holds that land's sea-chart, the end of a 3-step rumour chain (an old sailor, a
// wreck, a sunken chart in a sea cave). Landing on a hidden land is FIRST CONTACT: its kingdoms stop being isolated
// (realm::KingdomState::isolated). Ferries: a route between two known ports is a paid passage that plays out on deck.
// Fishing: cast from a shore, a pier or a boat; a bite; a timing / tension reel (Input::reel held).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/sim/common.h"
#include "rpg/world/ids.h"

class Game;

namespace sea {

// ---- hulls (2.12 table; saved by index: append only)
enum class Hull : uint8_t { Rowboat, Sloop, Galleon, COUNT };
// the waters a hull may sail (ew::SeaKind as bits)
enum : uint8_t { W_SHALLOW = 1, W_DEEP = 2, W_STORM = 4 };
struct HullInfo {
  const char* name;      // "ROWBOAT", "SLOOP", "GALLEON"
  float speed;           // tiles a second at oars / full sail in a beam wind
  float windGain;        // tiles a second the wind adds astern (and takes away head-on); 0: oars
  float turn;            // radians a second at full helm
  int price;             // gold at a shipwright (300, 4000, 20000)
  float hull;            // hit points of the hull (the sea, reefs, monsters, pirates)
  uint8_t waters;        // W_* it may sail
  uint8_t crew;          // companions it carries
  uint8_t cargo;         // cargo slots (the galleon's hold)
  uint8_t minLevel;      // the player level a harbourmaster sells it at...
  uint8_t sailorLevel;   // ...or this level for the Sailor background (15.1: "can buy and sail boats early")
  float radius;          // the body's radius aboard (px): Actor::radius while aboard
};
const HullInfo& hullInfo(Hull h);
const char* hullName(Hull h);

// a boat's upgrades (bits; the shipwright's work)
enum : uint16_t {
  UP_PLATING = 1 << 0,   // +50 % hull
  UP_SAILS = 1 << 1,     // +15 % speed
  UP_HOLD = 1 << 2,      // +2 cargo slots
  UP_KEEL = 1 << 3,      // a sloop may sail the storm sea
  UP_LANTERN = 1 << 4,   // a lantern at the bow: sees farther in storms and at night
  UP_FIGUREHEAD = 1 << 5 // the culture's figurehead (looks; a little luck against monsters)
};

// ---- a boat the player owns (saved)
struct Boat {
  uint32_t id = 0;               // Sea::nextBoatId at purchase (stable)
  Hull hull = Hull::Rowboat;
  std::string name;              // "THE GULL'S LUCK" (the player may keep the shipwright's name)
  uint64_t culture = 0;          // cult::CultureId of the yard that built it: its look (art::shipSprite style)
  ew::Gid dock = 0;              // the port site it is moored at (0: left at a tile)
  int32_t gx = 0, gy = 0;        // where it lies (global tile; the berth when docked)
  float heading = 0;             // radians (0 east, pi/2 south: screen space)
  float hp = 0;                  // hull now (hullInfo().hull, more with UP_PLATING)
  uint16_t upgrades = 0;         // UP_*
  bool sunk = false;             // lost at sea (a shipwright raises a new one at a price)
  // ---- SEA lane (block v2)
  bool ferry = false;            // a ferry's hull the hero rides as a passenger (never the player's own: gone on arrival)
};

// ---- the sea-chart chain of a hidden land (2.12): the steps, in order
enum class Chain : uint8_t {
  None,       // nothing heard
  Sailor,     // 1: an old sailor's tale in a sailors' tavern (the land and its wreck rumoured)
  Wreck,      // 2: the wreck found (SPF_CHART Shipwreck): the drowned captain's log names the sea cave
  Cave,       // 3: the sea cave reached (SPF_CHART | SPF_SEACAVE Cave): the sunken chart at its end
  Chart,      // the chart held: the Mist lets the boat through
  COUNT
};
// what the player knows of a land (saved; one record per land ever touched)
enum : uint8_t {
  LF_RUMOURED = 1 << 0,   // shows on the Z3 map (tavern talk, a cartographer, the chain's first step)
  LF_CHART = 1 << 1,      // its sea-chart held (== chain Chart)
  LF_LANDED = 1 << 2,     // the player set foot there (first contact for a hidden land)
  LF_WORD = 1 << 3,       // word carried home ("Tell King X of the land beyond the Mist": done)
  LF_MAPPED = 1 << 4      // its macro and major sites shown (a cartographer's map, 200-1500 gold)
};
struct LandRecord {
  uint32_t land = 0;             // ew::LandPlan::id
  Chain chain = Chain::None;
  uint8_t flags = 0;             // LF_*
  int32_t contactDay = -1;       // the day of first contact (-1: none)
};

// ---- fishing (15.4): species and the player's catches
enum : uint8_t { FW_LAKE = 1, FW_RIVER = 2, FW_COAST = 4, FW_DEEP = 8, FW_MIST = 16 };   // the waters a fish lives in
enum : uint8_t { FC_COLD = 1, FC_TEMPERATE = 2, FC_WARM = 4 };                          // its climates
enum : uint8_t { FH_DAWN = 1, FH_DAY = 2, FH_DUSK = 4, FH_NIGHT = 8 };                  // when it bites
struct Species {
  const char* name;      // "BROWN TROUT"
  uint8_t waters;        // FW_*
  uint8_t climate;       // FC_*
  uint8_t seasons;       // bit per season 0 spring .. 3 winter
  uint8_t hours;         // FH_*
  uint8_t rarity;        // 0 common, 1 uncommon, 2 rare, 3 legendary (one of a kind per water: a name and a story)
  uint16_t value;        // gold at a fishmonger, a fish of average size
  float fight;           // 0..1: how hard it pulls (the reel's tension)
  uint16_t minCm, maxCm; // its size range
  uint32_t color;        // rgba (the view's catch card; the art lane paints the fish)
};
const std::vector<Species>& species();   // phase A: a starter list; the SEA lane fills it (rare and legendary fish)
struct Catch { uint16_t species = 0; uint16_t count = 0; uint16_t bestCm = 0; };

// ---- the saved state
struct Sea {
  std::vector<Boat> boats;
  int aboard = -1;                       // boats index the hero is aboard (-1: ashore). A save at sea loads at sea.
  std::vector<ew::Gid> portsKnown;       // ports visited or learned of (sorted): ferry routes join two known ports
  std::vector<LandRecord> lands;         // sorted by land id
  std::vector<Catch> catches;            // sorted by species
  uint32_t nextBoatId = 1;
  uint32_t fishCaught = 0;               // all time
  uint16_t voyages = 0;                  // ferry passages taken

  // ---- runtime (never saved): what the sim and the view read every frame
  struct Wind { float dir = 0; float speed = 0; };               // radians (where it blows TO), 0..1 strength
  struct Storm {
    bool on = false;
    Vec2 c;                              // its eye (window px; shifted with the window)
    float radius = 0;                    // px
    float t = 0, life = 0;               // seconds
    float power = 0;                     // 0..1 at its height
  };
  enum class Cast : uint8_t { None, Casting, Waiting, Bite, Reeling, Landed, Lost };
  struct Line {
    Cast phase = Cast::None;
    float t = 0;                         // seconds in this phase
    Vec2 bob;                            // the float (window px)
    float tension = 0;                   // 0..1 (the line snaps at 1)
    float dist = 1;                      // 1 at the bob .. 0 landed
    float bite = 0;                      // seconds left to strike on a bite
    uint16_t species = 0;                // the fish on the line
    uint16_t cm = 0;
    bool fromBoat = false;
    // ---- SEA lane: the fight's runs (Reeling): surge > 0 the fish is running (reeling then strains the line hard),
    //      next: seconds to its next run
    float surge = 0, next = 0;
  };
  struct Voyage {                        // a ferry passage under way (on deck)
    bool on = false;
    ew::Gid from = 0, to = 0;
    float t = 0, dur = 0;                // seconds on deck (the clock runs faster)
    int gold = 0;
    uint8_t event = 0;                   // 0 calm, 1 pirates, 2 a storm, 3 a sea serpent
    // ---- SEA lane (saved with the block, v2: a save mid-voyage loads mid-voyage)
    int32_t toX = 0, toY = 0;            // the destination's berth / pier (global tile): where the hero steps ashore
    float eventAt = 0;                   // seconds into the passage the event comes (0: none)
    uint8_t phase = 0;                   // 0 sailing, 1 the event under way, 2 the event over, 3 arriving (the fade)
    float hours = 0;                     // in-game hours the whole passage takes (the clock runs ahead by them)
    float course = 0;                    // the captain's heading out of the harbour (radians)
  };
  // ---- SEA lane: a ship that is not ours (never saved): a pirate sloop closing in, a ferry's escort. The view draws
  //      it like a boat (art::shipSheet; pirates: a black sail, ShipStyle from the culture key 0)
  struct Ship {
    uint32_t id = 0;                     // runtime serial
    uint8_t kind = 0;                    // 1 pirates
    Hull hull = Hull::Sloop;
    Vec2 p;                              // window px
    float heading = 0, speed = 0;
    float hp = 0;
    float t = 0;                         // seconds alive
    bool boarded = false;                // its crew has jumped aboard the hero's boat
    bool fleeing = false;                // beaten off: it turns away and is gone
  };
  // ---- SEA lane: a creature of the sea in play (never saved; keyed by its actor id): the swimmers' AI state
  struct Swimmer {
    int id = 0;                          // Actor::id
    uint8_t kind = 0;                    // SW_* (sea_foes.cpp)
    float t = 0, cd = 0;                 // seconds alive, the next move's cooldown
    float dive = 0;                      // 0 on the surface .. 1 under the swell (the view fades it; untouchable)
    uint8_t move = 0;                    // the move under way (sea_foes.cpp)
    Vec2 mark;                           // where the move lands (window px): the view's telegraph
    int parent = 0;                      // a kraken's tentacle: the kraken's actor id
    int ship = 0;                        // a pirate: its Ship::id until it boards
    bool aboard = false;                 // a boarder on the hero's deck: rides it at `off` from the hero (window px)
    Vec2 off;
  };
  struct Live {
    float speed = 0;                     // the boat aboard: tiles a second now
    float heading = 0;                   // radians
    float helm = 0;                      // -1..1 (the view leans the sail)
    Wind wind;
    Storm storm;
    float stormDark = 0;                 // 0..1 how dark the sky is (the view)
    float mist = 0;                      // 0..1 the Mist's density at the boat / the hero (the view's fog)
    uint32_t mistLand = 0;               // the hidden land whose Mist that is
    float mistT = 0;                     // seconds in that Mist without its chart (MIST_TURN_SECS: turned back)
    bool turningBack = false;            // the Mist is turning the boat round
    Line line;
    float swimT = 0;                     // seconds in the water (swimming: the Sailor swims farther)
    float heroRadius = 0;                // the hero's body radius ashore (put back on disembarking)
    float mistProbeT = 0;                // seconds to the next Mist sample
    bool mistTold = false;               // the Mist's first line said for this crossing
    float autopilot = -100;              // a heading (radians) the helm holds at full sail while the stick is idle
                                         // (scripts, tests, a ferry's captain); <= -50: off. The Mist's turn-back clears it
    Voyage voyage;
    uint32_t serial = 0;                 // bumps whenever boats change (the view's caches)
    // ---- SEA lane additions (all runtime)
    float wakeT = 0;                     // seconds to the next wake puff
    float tack = 0;                      // the autopilot's tack: -1 / +1 the side it beats to windward on, 0 none
    float tackT = 0;                     // seconds left on this tack before it goes about
    bool mooring = false;                // gliding into a berth (no stick; it ends ashore on the pier)
    Vec2 moorTo;                         // the berth (window px)
    ew::Gid moorDock = 0;                // the port whose berth that is
    float moorT = 0;
    float drift = 0;                     // tiles a second the wind's push adds (a storm's more): the view leans the hull
    float heel = 0;                      // -1..1 the hull heeled by the wind on the beam (the view tilts the sail)
    float rough = 0;                     // 0..1 how rough the water is under the boat (storm, the storm sea)
    float lightningT = 0;                // seconds to the next lightning stroke in a storm
    float stormRollT = 0;                // seconds to the next look for a weather front
    float sinkT = 0;                     // > 0: the boat is going down (seconds since; the fade, then ashore)
    float swimMax = 0;                   // the hero's swimming time (SWIM_SECS or SAILOR_SWIM_SECS) this swim
    bool swimming = false;               // the hero is in the water (ashore rules; not aboard)
    bool pulled = false;                 // tired out: the hero is being pulled back to the shore
    Vec2 lastDry;                        // the hero's last dry footing (window px): where a tired swimmer is pulled to
    Vec2 swimVel;                        // the swimmer's way (px a second; eased)
    std::vector<Ship> ships;             // pirates afloat
    std::vector<Swimmer> swimmers;       // the sea's creatures in play
    uint32_t nextShip = 1;
    float encounterT = 0;                // seconds to the next encounter roll at sea
    float portScanT = 0;                 // seconds to the next look for a port the hero stands in (ports known)
    float folkT = 0;                     // seconds to the next look for the dock folk (harbourmaster, shipwright...)
    float chainT = 0;                    // seconds to the next look at the chart chain's places / a landing
    int32_t landedLand = 0;              // the land the hero last stood on (first contact once per landing)
    float fishCd = 0;                    // a moment after a catch before the next cast
    float lineDrift = 0;                 // the bob's slow drift phase (the view)
    float swell = 0;                     // the sea's swell phase (the boat's bob; the swimmers rise with it)
    // the nearest port round the hero (refreshed twice a second): its berth and pier (window px), a harbour or a jetty
    ew::Gid nearPort = 0;
    Vec2 nearBerth, nearPier;
    bool nearHarbour = false;
    float nearDist = 1e9f;               // px from the hero (or the boat aboard) to that berth
    std::string nearName;
    int sinkBoat = -1;                   // the boat going down (sinkT)
    float stuckT = 0;                    // seconds the boat aboard has made no way against the stick (the view's bump)
    float clearT = 0;                    // (fixer M10 r1) pivoting off a pier: seconds she keeps the clear heading, rudder amidships
    float rollT = 0;                     // seconds since the last roll for the sea's commoner perils
    bool moorPier = false;               // the glide goes on from the berth to the pier's foot (no step ashore at the berth)
    bool autoReel = false;               // scripts and tests: the line strikes and reels by itself (lets go on a run)
    double stepMsWorst = 0;              // the slowest SeaOps::step so far (--perf, the --sail bot; tests reset it)
    uint32_t wordLand = 0;               // first contact: the land whose word-to-the-king quest comes in wordT seconds
    float wordT = 0;
  } live;
  // ---- SEA lane additions (pure helpers on the saved state)
  int ownedAt(ew::Gid dock) const;       // a boat of ours moored at that port (-1 none; ferries and the sunk aside)
  int owned(Hull h) const;               // a boat of ours of that hull, afloat (-1 none)

  bool atSea() const { return aboard >= 0 && aboard < (int)boats.size(); }
  const LandRecord* land(uint32_t id) const;
  LandRecord& landMut(uint32_t id);      // made on first touch
  bool hasChart(uint32_t landId) const;
  bool knowsPort(ew::Gid site) const;
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);
};

// ---- the numbers (2.12)
constexpr float MIST_TURN_SECS = 20.0f;   // seconds of fog before a boat without the chart is turned round
constexpr float SWIM_SECS = 6.0f, SAILOR_SWIM_SECS = 15.0f;   // how long the hero swims in shallow water
int fareOf(int tiles);                    // a ferry passage: 20 .. 400 gold by distance
// the wind over the sea at a global tile and an hour (deterministic for a seed; a slow field)
Sea::Wind windAt(uint64_t seed, float gx, float gy, double hours);
// a hull's speed factor sailing on `heading` in that wind (0.4 head-on .. 1 beam .. 1 + windGain / speed astern)
float windFactor(Hull h, float heading, const Sea::Wind& w);

// ---- Ev::Sea: a = SeaEv, p = where, q = a second point, f = magnitude, id = boat id or 0, s = the line to show
enum class SeaEv : uint8_t {
  Board, Disembark,               // aboard / ashore (p = the pier)
  Wake,                           // a bow wave / wake puff (f = speed)
  HullHit,                        // the hull struck (f = damage): rocks, a reef, a monster, a cannon
  Sink,                           // the boat lost
  MistEnter, MistTurn, MistPart,  // the Mist: entering it, turning the boat back, parting for the chart
  StormBegin, StormEnd, Lightning,
  Cast, Bite, Strike, Reel, Landed, Lost,   // fishing (f: tension on Reel)
  VoyageStart, VoyageEvent, VoyageEnd,      // a ferry passage (VoyageEvent: f = the event kind)
  ChartStep,                      // a sea-chart chain step taken (f = the step)
  FirstContact,                   // a hidden land reached (s: "THE MIST PARTS ON ...")
  Splash,                         // something broke the water (a serpent, a merrow, a dive; f = size)
  // ---- SEA lane additions (append only)
  Moor,                           // the boat glides into a berth (p = the berth; ends with Disembark)
  Telegraph,                      // a sea creature's blow is coming (p = where it lands, f = seconds to impact, q = from,
                                  // id = the actor id): a ring on the water, a tentacle's shadow, the serpent's rise
  Tentacle,                       // a kraken's tentacle rises (p) or sinks (f = 0)
  Boarded,                        // pirates come over the rail (p = where)
  Buy, Repair, Upgrade, Raise,    // the shipwright's work (id = the boat id, s = the line)
  WashAshore,                     // the hero comes to on a beach after a sinking (p)
  SwimTired,                      // the swimmer is spent: pulled back to the shore (p)
  COUNT
};

// ---- SEA lane: the shipwright's book (2.12, 15.4)
struct UpgradeInfo { uint16_t bit; const char* name; int price; uint8_t hulls; };   // hulls: bit per Hull it fits
const std::vector<UpgradeInfo>& upgrades();
float maxHp(const Boat& b);                          // the hull's hit points with its plating
float boatSpeed(const Boat& b);                      // tiles a second at full sail (with its sails)
bool sailsStorm(const Boat& b);                      // may it sail the storm sea (a galleon, a sloop with UP_KEEL)
// a hull's price at a yard: the Sailor (15.1) pays 85 %; sailorLevel instead of minLevel; a shipwright raising a sunk
// boat asks 40 % of the hull's price
int hullPrice(Hull h, bool sailor);
int raisePrice(Hull h, bool sailor);
int repairPrice(const Boat& b);                      // 2 gold a hit point lost (at least 5)
int mapPrice(int sites);                             // a cartographer's map of a kingdom: 200..1500 gold
// ---- SEA lane: fishing (15.4)
// the waters of a fishing spot: FW_* (lake, river, coast, deep, Mist), from the tile kind and the world
// pick the fish that bites: species index or -1 (nothing bites). roll: 0..1 uniform; luck 0..1 (a better rod, bait)
int pickSpecies(uint8_t water, uint8_t climate, int season, float hour, float roll, const std::vector<Catch>& caught);
uint8_t hourBits(float hour);                        // FH_* of an hour
uint8_t climateOf(float temperature);                // FC_* from a climate's temperature (0 cold .. 1 hot)
// the legendary fish of a water (one each: a name and a story): species index or -1
int legendaryOf(uint8_t water);
const char* fishStory(int species);                  // a legendary fish's tale ("" for the rest)

}  // namespace sea
