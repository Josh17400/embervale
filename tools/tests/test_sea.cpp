// rpg_test --sea / --sail: M10 "Beyond the Mist" (VISION_PLAN 2.12, 13 M10, 15.4, 15.31). SEA lane.
// --sea [--seeds A..B] (CI "Sea test", 1..3):
//   - tables: the hulls (2.12 prices, waters), the yard's book, the fish and their odds (rarity order, the legendary
//     caught once), the points of sail (in irons < close-hauled < beam < running);
//   - the world's sea contract round the start; the sea block (v2: ferries, a voyage) round trips, a v1 block loads;
//   - play at a port: a sloop given, boarded, sailed out with eased turns, saved and loaded at sea, brought back and
//     MOORED with a glide into the berth (no step jumps more than a stride), left moored (its dock the port);
//   - the Mist: turned back without the chart, parted with it;
//   - isolation: a hidden land's kingdoms stay isolated through 60 days of the realm (no relation, no war with the
//     mainland), first contact opens them (wary -20; three quests for their people make the hero a friend at 25), the
//     word carried home opens a relation;
//   - storms (push, rough water, darkness, lightning; a rowboat battered), sinking (the hero washes ashore, the boat
//     sunk), swimming (pulled back to the shore when spent; the Sailor swims longer), fishing (a cast, a bite, a reel:
//     landed into the pack as Ingr::Fish), the chart chain (the old sailor, the wreck, the cave: the chart), first
//     contact by landing, ferries (a passage on deck, its event, journey's end at the destination's pier; a save
//     mid-voyage loads mid-voyage);
// --sail [--seeds A..B] (13 M10 verification): a headless captain takes a sloop out of the nearest harbour and steers
//   round the coasts to continent 2 (the nearest other continent: landAt there equals its id), printing the time and
//   the route length; then streaming throughput at sea at sloop and galleon speed (the worst step, chunks made on the
//   main thread, chunks ready ahead of the bow).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/home.h"
#include "rpg/sim/sea.h"
#include "rpg/sim/sea_ops.h"
#include "rpg/sim/stream.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {
int m_bad = 0;
#define SEA_FAIL(...) do { out("FAIL: sea: " __VA_ARGS__); m_bad++; } while (0)
constexpr float kPi = 3.14159265f;

void run(Game& g, float secs, const Input& in, std::vector<Event>* keep = nullptr) {
  const int n = (int)std::lround(secs / SIM_DT);
  for (int f = 0; f < n; f++) {
    g.update(SIM_DT, in);
    if (keep) keep->insert(keep->end(), g.events.begin(), g.events.end());
    g.events.clear();
    if (g.mode == Mode::LevelUp || g.mode == Mode::Dialogue) g.mode = Mode::Play;
  }
}
// run until the journey behind the fade (a sinking, a ferry's end) has arrived
void runTravel(Game& g, std::vector<Event>* keep = nullptr) {
  // (the gather waits on the streamer's worker in wall-clock time: step until the arrival, at most 40 s of real time)
  const auto t0 = std::chrono::steady_clock::now();
  while (g.travelling() && std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < 40.0) run(g, 0.1f, Input(), keep);
}
int seaEvents(const std::vector<Event>& ev, sea::SeaEv k) {
  int n = 0;
  for (const Event& e : ev) if (e.type == Ev::Sea && e.a == (int)k) n++;
  return n;
}
Game makeGame(uint64_t seed) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.godMode = true;
  g.hour = 12;
  return g;
}
const char* S(uint64_t s) { static char b[32]; std::snprintf(b, sizeof b, "%llu", (unsigned long long)s); return b; }

void checkTables() {
  const int prices[3] = {300, 4000, 20000};
  for (int h = 0; h < (int)sea::Hull::COUNT; h++) {
    const sea::HullInfo& H = sea::hullInfo((sea::Hull)h);
    if (H.price != prices[h]) SEA_FAIL("%s costs %d (2.12: %d)\n", H.name, H.price, prices[h]);
    if (H.speed <= 0 || H.hull <= 0 || H.radius <= 0 || !H.waters) SEA_FAIL("%s has a zero number\n", H.name);
    if (H.sailorLevel > H.minLevel) SEA_FAIL("%s: the Sailor's level %d is above the others' %d (15.1: early)\n", H.name, H.sailorLevel, H.minLevel);
    if (sea::hullPrice((sea::Hull)h, true) >= H.price) SEA_FAIL("%s: the Sailor pays no less\n", H.name);
  }
  if (!(sea::hullInfo(sea::Hull::Rowboat).waters & sea::W_SHALLOW) || (sea::hullInfo(sea::Hull::Rowboat).waters & sea::W_DEEP))
    SEA_FAIL("the rowboat must keep to the shallows\n");
  sea::Boat sl; sl.hull = sea::Hull::Sloop;
  if (sea::sailsStorm(sl)) SEA_FAIL("a sloop without a keel sails the storm sea\n");
  sl.upgrades = sea::UP_KEEL;
  if (!sea::sailsStorm(sl)) SEA_FAIL("a sloop with a keel cannot sail the storm sea\n");
  if (sea::species().size() < 20) SEA_FAIL("only %zu fish species\n", sea::species().size());
  for (const sea::Species& s : sea::species())
    if (!s.name || std::strlen(s.name) > 18 || s.minCm == 0 || s.maxCm < s.minCm) SEA_FAIL("fish '%s' is malformed\n", s.name ? s.name : "?");
  const uint8_t waters[5] = {sea::FW_LAKE, sea::FW_RIVER, sea::FW_COAST, sea::FW_DEEP, sea::FW_MIST};
  for (uint8_t w : waters) if (sea::legendaryOf(w) < 0 || !*sea::fishStory(sea::legendaryOf(w))) SEA_FAIL("no legendary fish with a tale in water %d\n", w);
  if (sea::fareOf(0) != 20 || sea::fareOf(1000000) != 400) SEA_FAIL("ferry fares must run 20..400 (2.12)\n");
  if (sea::mapPrice(0) != 200 || sea::mapPrice(1000) != 1500) SEA_FAIL("a cartographer's map must cost 200..1500 (2.12)\n");
  // the odds: commons outnumber uncommons outnumber rares; the legendary bites rarely, and never once caught
  {
    int byRarity[4] = {}, legend = sea::legendaryOf(sea::FW_COAST);
    std::vector<sea::Catch> none;
    uint32_t r = 12345;
    for (int i = 0; i < 40000; i++) {
      r = r * 1664525u + 1013904223u;
      const int sp = sea::pickSpecies(sea::FW_COAST, sea::FC_TEMPERATE, 1, 20.0f, (float)(r >> 8) / 16777216.0f, none);
      if (sp >= 0) byRarity[sea::species()[(size_t)sp].rarity]++;
    }
    out("sea: coast fishing odds (40000 bites at dusk, summer, temperate): common %d, uncommon %d, rare %d, legendary %d\n", byRarity[0], byRarity[1], byRarity[2], byRarity[3]);
    if (!(byRarity[0] > byRarity[1] && byRarity[1] > byRarity[2] && byRarity[2] > byRarity[3] && byRarity[3] > 0)) SEA_FAIL("the fishing odds are out of order\n");
    std::vector<sea::Catch> had = {{(uint16_t)legend, 1, 100}};
    bool again = false;
    for (int i = 0; i < 4000 && !again; i++) {
      r = r * 1664525u + 1013904223u;
      again = sea::pickSpecies(sea::FW_COAST, sea::FC_TEMPERATE, 1, 20.0f, (float)(r >> 8) / 16777216.0f, had) == legend;
    }
    if (again) SEA_FAIL("the coast's legendary bit again after it was caught\n");
  }
  // the points of sail: in irons < close-hauled < beam <= running (a sloop in a fresh wind)
  {
    sea::Sea::Wind w; w.dir = 0; w.speed = 1;   // blowing east
    const float irons = sea::windFactor(sea::Hull::Sloop, kPi, w), close = sea::windFactor(sea::Hull::Sloop, kPi - 0.95f, w),
                beam = sea::windFactor(sea::Hull::Sloop, kPi * 0.5f, w), run = sea::windFactor(sea::Hull::Sloop, 0, w);
    if (!(irons < 0.3f && irons < close && close < beam && beam <= run && run > 1.2f))
      SEA_FAIL("points of sail: irons %.2f close %.2f beam %.2f running %.2f\n", irons, close, beam, run);
    if (sea::windFactor(sea::Hull::Rowboat, kPi, w) != 1.0f) SEA_FAIL("oars should not care for the wind\n");
  }
}

void checkWorld(uint64_t seed) {
  ew::EndlessSource src(seed);
  const uint32_t origin = src.landAt(0, 0);
  if (!origin) SEA_FAIL("seed %s: no land at the origin\n", S(seed));
  const ew::LandPlan O = src.land(origin);
  if (O.id != origin || !O.origin || O.hidden) SEA_FAIL("seed %s: the origin land is not the mainland (id %u)\n", S(seed), O.id);
  if (src.seaAt(0, 0) != ew::SeaKind::Land && src.seaAt(src.start().spawn.x, src.start().spawn.y) != ew::SeaKind::Land)
    SEA_FAIL("seed %s: the start is not on land\n", S(seed));
  const std::vector<ew::LandPlan> lands = src.landsIn(-20000, -20000, 20000, 20000);
  int hidden = 0, others = 0;
  for (const ew::LandPlan& L : lands) {
    if (src.land(L.id).id != L.id) SEA_FAIL("seed %s: land %u does not round-trip\n", S(seed), L.id);
    if (L.origin) continue;
    others++;
    if (!L.hidden) continue;
    hidden++;
    if (L.mistOut - L.mistIn != ew::MIST_RING) SEA_FAIL("seed %s: land %u's Mist ring is %d wide\n", S(seed), L.id, L.mistOut - L.mistIn);
    const int32_t mid = (L.mistIn + L.mistOut) / 2;
    if (src.mistAt(L.cx, L.cy) != 0) SEA_FAIL("seed %s: Mist over land %u's heart\n", S(seed), L.id);
    if (src.mistAt(L.cx + mid, L.cy) < 60000) SEA_FAIL("seed %s: land %u's Mist is thin mid-ring (%d)\n", S(seed), L.id, src.mistAt(L.cx + mid, L.cy));
    if (src.mistAt(L.cx + L.mistOut + 40, L.cy) != 0 && src.hiddenLandAt(L.cx + L.mistOut + 40, L.cy) == L.id)
      SEA_FAIL("seed %s: Mist beyond land %u's ring\n", S(seed), L.id);
    if (src.hiddenLandAt(L.cx + mid, L.cy) != L.id) SEA_FAIL("seed %s: hiddenLandAt misses land %u's ring\n", S(seed), L.id);
  }
  out("sea: seed %s: %zu lands within 20000 tiles (%d beyond the mainland, %d hidden)\n", S(seed), lands.size(), others, hidden);
  if (!others) SEA_FAIL("seed %s: no second land within 20000 tiles\n", S(seed));
}

void checkBlock() {
  sea::Sea s;
  sea::Boat b;
  b.id = 7; b.hull = sea::Hull::Sloop; b.name = "THE TEST"; b.culture = 0xABCDull; b.dock = 0x1234ull; b.gx = -55; b.gy = 901;
  b.heading = 1.25f; b.hp = 99.5f; b.upgrades = sea::UP_SAILS | sea::UP_KEEL;
  s.boats.push_back(b);
  sea::Boat f = b; f.id = 8; f.ferry = true; f.name = "THE FERRY";
  s.boats.push_back(f);
  s.aboard = 1;
  s.portsKnown = {3, 9, 12};
  sea::LandRecord& L = s.landMut(0x10203u);
  L.chain = sea::Chain::Wreck; L.flags = sea::LF_RUMOURED; L.contactDay = 4;
  s.catches.push_back({2, 5, 41});
  s.nextBoatId = 9; s.fishCaught = 5; s.voyages = 2;
  sea::Sea::Voyage& v = s.live.voyage;
  v.on = true; v.from = 3; v.to = 9; v.t = 12.5f; v.dur = 60; v.gold = 77; v.event = 1; v.toX = -400; v.toY = 812; v.eventAt = 20; v.phase = 0;
  v.hours = 6.5f; v.course = -0.7f;
  std::vector<uint8_t> a, c;
  s.serialize(a);
  sea::Sea t;
  if (!t.deserialize(a)) { SEA_FAIL("the sea block did not load\n"); return; }
  t.serialize(c);
  if (a != c) SEA_FAIL("the sea block's round trip is not byte-identical\n");
  if (!t.atSea() || t.boats[0].name != "THE TEST" || !t.boats[1].ferry || t.land(0x10203u) == nullptr || t.land(0x10203u)->chain != sea::Chain::Wreck)
    SEA_FAIL("the sea block lost its facts\n");
  if (!t.live.voyage.on || t.live.voyage.to != 9 || t.live.voyage.toY != 812 || std::fabs(t.live.voyage.hours - 6.5f) > 1e-4f) SEA_FAIL("the voyage did not survive the block\n");
  std::vector<uint8_t> bad = a;
  bad[0] = 99;
  if (t.deserialize(bad)) SEA_FAIL("a sea block of another version loaded\n");
  // a v1 block (phase A's layout: no ferry flag, no voyage) still loads
  std::vector<uint8_t> v1;
  {
    BinW w(v1);
    w.u8(1); w.u32(1);
    w.u32(5); w.u8(0); w.str("OLD"); w.u64(0); w.u64(0); w.i32(1); w.i32(2); w.f32(0); w.f32(60); w.u16(0); w.u8(0);
    w.i32(-1); w.u32(0); w.u32(0); w.u32(0); w.u32(6); w.u32(0); w.u16(0);
  }
  sea::Sea o;
  if (!o.deserialize(v1) || o.boats.size() != 1 || o.boats[0].name != "OLD" || o.boats[0].ferry || o.live.voyage.on) SEA_FAIL("a v1 sea block does not load\n");
}

// a port near the start whose berth lies on deep water reachable from the shore
bool toPort(Game& g, ew::PortPlan& P, bool harbourOnly = false, int32_t R = 6000) {
  ew::EndlessSource& src = *g.world.src;
  const int32_t hx = SeaOps::heroGx(g), hy = SeaOps::heroGy(g);
  std::vector<ew::PortPlan> ports = src.portsIn(hx - R, hy - R, hx + R, hy + R);
  std::sort(ports.begin(), ports.end(), [&](const ew::PortPlan& a, const ew::PortPlan& b) {
    return std::hypot((double)a.x - hx, (double)a.y - hy) < std::hypot((double)b.x - hx, (double)b.y - hy);
  });
  for (const ew::PortPlan& p : ports) {
    if (harbourOnly && !p.harbour) continue;
    int32_t tx = p.x, ty = p.y;
    const double L = std::max(1.0, std::hypot((double)p.berthX - p.x, (double)p.berthY - p.y));
    for (int s = 0; s <= (int)L; s++) {
      const int32_t x = p.x + (int32_t)std::lround((p.berthX - p.x) * s / L), y = p.y + (int32_t)std::lround((p.berthY - p.y) * s / L);
      if (src.seaAt(x, y) != ew::SeaKind::Land) break;
      tx = x; ty = y;
    }
    g.teleportGlobal(tx, ty);
    run(g, 0.6f, Input());   // (the port's scan: Live::near*)
    P = p;
    return true;
  }
  return false;
}

void checkPlay(uint64_t seed) {
  Game g = makeGame(seed);
  ew::PortPlan P;
  if (!toPort(g, P)) {
    const int32_t hx = SeaOps::heroGx(g), hy = SeaOps::heroGy(g);
    const size_t ns = g.world.src->settlementsIn(hx - 6000, hy - 6000, hx + 6000, hy + 6000).size();
    out("sea: seed %s: start %d,%d: %zu settlements within 6000 tiles, land %u, sea at start %s\n", S(seed), hx, hy, ns,
        g.world.src->landAt(hx, hy), ew::seaKindName(g.world.src->seaAt(hx, hy)));
    // (phase A: not a failure until the WORLD lane's guarantee lands; then this becomes SEA_FAIL again)
    out("sea: seed %s: SKIP play (no port within 6000 tiles of the start)\n", S(seed));
    return;
  }
  int bi = SeaOps::buyBoat(g, sea::Hull::Sloop, P.site);   // (at the port's berth, as a purchase lies)
  if (bi < 0) bi = SeaOps::giveBoat(g, sea::Hull::Sloop, 16);
  if (bi < 0) { SEA_FAIL("seed %s: no water for a sloop by %s\n", S(seed), P.name.c_str()); return; }
  g.sea.boats[(size_t)bi].upgrades |= sea::UP_KEEL;   // (whatever the sea off this port is)
  if (SeaOps::boatNear(g, 16.0f * TILE * 1.5f) != bi) SEA_FAIL("seed %s: the boat given is not near the hero\n", S(seed));
  if (!SeaOps::board(g, bi)) { SEA_FAIL("seed %s: could not board the sloop by %s\n", S(seed), P.name.c_str()); return; }
  if (!g.sea.atSea() || g.pl().swim != 1) SEA_FAIL("seed %s: aboard, the hero is not the boat (swim %d)\n", S(seed), g.pl().swim);
  // sail out: away from the port's heart toward its berth and beyond, 12 s; the heading never jumps (eased rudder)
  const Vec2 out0 = g.pl().p;
  Input in;
  const double dx = (double)P.berthX - P.x, dy = (double)P.berthY - P.y, dl = std::max(1.0, std::hypot(dx, dy));
  in.move = Vec2((float)(dx / dl), (float)(dy / dl));
  std::vector<Event> ev;
  float worstTurn = 0, prevH = g.sea.live.heading;
  // (the helm held out of the harbour: beating to windward if it must, bearing away from a pier it fouls)
  const float outH = std::atan2(in.move.y, in.move.x);
  g.sea.live.autopilot = outH;
  for (int f = 0; f < 720; f++) {
    if (g.sea.live.stuckT > 0.8f) { g.sea.live.autopilot += 0.9f; g.sea.live.stuckT = 0; }
    run(g, SIM_DT, Input(), &ev);
    float d = std::fabs(g.sea.live.heading - prevH);
    if (d > kPi) d = 2 * kPi - d;
    worstTurn = std::max(worstTurn, d);
    prevH = g.sea.live.heading;
  }
  const float moved = len(g.pl().p - out0);
  if (!g.sea.atSea()) SEA_FAIL("seed %s: fell out of the boat while sailing\n", S(seed));
  {
    const Map& m = g.map();
    const int tx = (int)std::floor(g.pl().p.x / TILE), ty = (int)std::floor(g.pl().p.y / TILE);
    if (!groundWater(m.at(tx, ty))) SEA_FAIL("seed %s: the boat stands on %d (not water)\n", S(seed), (int)m.at(tx, ty));
  }
  out("sea: seed %s: sailed %.0f px out of %s in 12 s (wind %.2f, worst turn %.3f rad a step, %d wake puffs)\n", S(seed), moved, P.name.c_str(),
      g.sea.live.wind.speed, worstTurn, seaEvents(ev, sea::SeaEv::Wake));
  if (moved < 3.0f * TILE) SEA_FAIL("seed %s: the sloop made only %.0f px in 12 s out of %s\n", S(seed), moved, P.name.c_str());
  if (worstTurn > sea::hullInfo(sea::Hull::Sloop).turn * SIM_DT * 1.05f) SEA_FAIL("seed %s: the heading jumped %.3f rad in a step\n", S(seed), worstTurn);
  if (moved > 3.0f * TILE && seaEvents(ev, sea::SeaEv::Wake) < 5) SEA_FAIL("seed %s: no wake behind a boat under way\n", S(seed));
  // saved and loaded at sea
  std::vector<uint8_t> a, b2;
  g.serialize(a);
  Game h(seed);
  if (!h.deserialize(a)) SEA_FAIL("seed %s: a save at sea did not load\n", S(seed));
  else {
    if (!h.sea.atSea() || h.pl().swim != 1 || len(h.pl().p - g.pl().p) > 0.5f) SEA_FAIL("seed %s: a save at sea loaded ashore or elsewhere\n", S(seed));
    h.serialize(b2);
    if (a != b2) SEA_FAIL("seed %s: a save at sea is not byte-identical after a load\n", S(seed));
  }
  // back toward the berth and moor: a glide into it (no snap), then ashore on the pier, the boat moored at the port
  bool began = false;
  for (int k = 0; k < 240 && !began; k++) {   // (up to a minute: the helm held on the berth, beating up to it if need be)
    const Vec2 berth((float)(P.berthX - g.world.ox) * TILE + 8.0f, (float)(P.berthY - g.world.oy) * TILE + 8.0f);
    g.sea.live.autopilot = std::atan2(berth.y - g.pl().p.y, berth.x - g.pl().p.x);
    if (g.sea.live.stuckT > 1.0f) g.sea.live.autopilot += 1.3f;
    run(g, 0.25f, Input());
    if (g.sea.live.nearPort == P.site && g.sea.live.nearDist < 6.0f * TILE) began = SeaOps::moorStart(g);
  }
  g.sea.live.autopilot = -100;
  in.move = Vec2(-in.move.x, -in.move.y);
  if (!began) {
    out("sea: seed %s: (no mooring at %s's berth: berth %.0f px off; left at a landing instead)\n", S(seed), P.name.c_str(), g.sea.live.nearDist);
    bool left = false;
    for (int k = 0; k < 40 && !left; k++) { run(g, 0.5f, in); left = SeaOps::disembark(g); }
    if (!left) SEA_FAIL("seed %s: could not come ashore at %s again\n", S(seed), P.name.c_str());
    return;
  }
  float worstStep = 0;
  Vec2 prev = g.pl().p;
  for (int f = 0; f < 60 * 12 && g.sea.atSea(); f++) {
    run(g, SIM_DT, Input());
    if (g.sea.atSea()) worstStep = std::max(worstStep, len(g.pl().p - prev));
    prev = g.pl().p;
  }
  const sea::Boat& B = g.sea.boats[(size_t)bi];
  out("sea: seed %s: moored at %s (worst glide step %.2f px; dock %s)\n", S(seed), P.name.c_str(), worstStep, B.dock == P.site ? "the port" : "none");
  if (g.sea.atSea()) SEA_FAIL("seed %s: the mooring never brought the hero ashore\n", S(seed));
  else if (g.pl().swim != 0 || groundWater(g.map().at((int)std::floor(g.pl().p.x / TILE), (int)std::floor(g.pl().p.y / TILE))))
    SEA_FAIL("seed %s: ashore after mooring, the hero is still afloat\n", S(seed));
  if (worstStep > 3.0f) SEA_FAIL("seed %s: the glide into the berth jumped %.1f px in a step\n", S(seed), worstStep);
  if (B.dock != P.site) SEA_FAIL("seed %s: the boat left at the berth is not moored at %s\n", S(seed), P.name.c_str());
}

// open water just outside a hidden land's Mist, a sloop there aboard facing the land (returns the land id, 0 none)
uint32_t toMistEdge(Game& g, int32_t extra = 30) {
  ew::EndlessSource& src = *g.world.src;
  const int32_t hx = SeaOps::heroGx(g), hy = SeaOps::heroGy(g);
  std::vector<ew::LandPlan> lands = src.landsIn(hx - 30000, hy - 30000, hx + 30000, hy + 30000);
  std::sort(lands.begin(), lands.end(), [&](const ew::LandPlan& a, const ew::LandPlan& b) {
    return std::hypot((double)a.cx - hx, (double)a.cy - hy) < std::hypot((double)b.cx - hx, (double)b.cy - hy);
  });
  for (const ew::LandPlan& L : lands) {
    if (!L.hidden) continue;
    for (int k = 0; k < 24; k++) {
      const double a = k * 0.2617994;
      bool open = true;
      for (int32_t r = L.mistOut + extra; r >= L.mistIn + 40 && open; r -= 20) {
        const ew::SeaKind sk = src.seaAt(L.cx + (int32_t)std::lround(std::cos(a) * r), L.cy + (int32_t)std::lround(std::sin(a) * r));
        open = sk == ew::SeaKind::Deep || sk == ew::SeaKind::Storm;
      }
      if (!open) continue;
      const int32_t x = L.cx + (int32_t)std::lround(std::cos(a) * (L.mistOut + extra)), y = L.cy + (int32_t)std::lround(std::sin(a) * (L.mistOut + extra));
      g.teleportGlobal(x, y);
      const ew::GTile at{x, y};
      const int bi = SeaOps::giveBoat(g, sea::Hull::Sloop, 30, "", &at);
      if (bi < 0) continue;
      g.sea.boats[(size_t)bi].upgrades |= sea::UP_KEEL;
      if (!SeaOps::board(g, bi)) continue;
      g.sea.live.heading = (float)(a + 3.14159265);
      return L.id;
    }
  }
  return 0;
}

void checkMist(uint64_t seed) {
  for (int withChart = 0; withChart < 2; withChart++) {
    Game g = makeGame(seed);
    const uint32_t landId = toMistEdge(g);
    if (!landId) { out("sea: seed %s: SKIP the Mist (no open water at a hidden land's Mist within 30000 tiles)\n", S(seed)); return; }
    if (withChart) { sea::LandRecord& R = g.sea.landMut(landId); R.chain = sea::Chain::Chart; R.flags |= sea::LF_CHART; }
    Input in;
    in.move = Vec2(std::cos(g.sea.live.heading), std::sin(g.sea.live.heading));
    std::vector<Event> ev;
    float inFog = 0, turnedAt = -1;
    for (int step = 0; step < 120 * 4 && turnedAt < 0; step++) {   // up to 120 s
      run(g, 0.25f, g.sea.live.turningBack ? Input() : in, &ev);
      if (g.sea.live.mist > 0.05f) inFog += 0.25f;
      if (g.sea.live.turningBack) turnedAt = inFog;
    }
    if (!withChart) {
      if (turnedAt < 0) SEA_FAIL("seed %s: the Mist never turned a chartless sloop back (%.1f s in the fog)\n", S(seed), inFog);
      else if (turnedAt > sea::MIST_TURN_SECS + 3.0f) SEA_FAIL("seed %s: the Mist turned the sloop back only after %.1f s\n", S(seed), turnedAt);
      if (!seaEvents(ev, sea::SeaEv::MistEnter) || !seaEvents(ev, sea::SeaEv::MistTurn)) SEA_FAIL("seed %s: no MistEnter / MistTurn event\n", S(seed));
      run(g, 40.0f, Input(), &ev);
      if (g.sea.live.mist > 0.05f) SEA_FAIL("seed %s: turned back, the sloop is still in the Mist after 40 s (%.2f)\n", S(seed), g.sea.live.mist);
      out("sea: seed %s: the Mist turned the sloop back after %.1f s in the fog\n", S(seed), turnedAt);
    } else {
      if (turnedAt >= 0) SEA_FAIL("seed %s: the Mist turned back a sloop holding the chart\n", S(seed));
      if (!seaEvents(ev, sea::SeaEv::MistPart)) SEA_FAIL("seed %s: with the chart, no MistPart event\n", S(seed));
      out("sea: seed %s: with the chart the Mist parted (%.1f s in the fog, never turned)\n", S(seed), inFog);
    }
  }
}

// ---- isolation: a hidden kingdom stays apart until contact, then opens
void checkIsolation(uint64_t seed) {
  Game g = makeGame(seed);
  ew::EndlessSource& src = *g.world.src;
  const int32_t hx = SeaOps::heroGx(g), hy = SeaOps::heroGy(g);
  std::vector<ew::LandPlan> lands = src.landsIn(hx - 30000, hy - 30000, hx + 30000, hy + 30000);
  std::sort(lands.begin(), lands.end(), [&](const ew::LandPlan& a, const ew::LandPlan& b) {
    return std::hypot((double)a.cx - hx, (double)a.cy - hy) < std::hypot((double)b.cx - hx, (double)b.cy - hy);
  });
  uint32_t land = 0;
  for (const ew::LandPlan& L : lands) {
    if (!L.hidden) continue;
    g.realm.focusNow(src, L.cx, L.cy, g.day);
    for (const realm::KingdomState& k : g.realm.kingdoms()) if (k.hiddenLand == L.id) { land = L.id; break; }
    if (land) break;
  }
  if (!land) { out("sea: seed %s: SKIP isolation (no hidden kingdom within 30000 tiles)\n", S(seed)); return; }
  auto crossing = [&](bool wars) {
    int n = 0;
    for (const realm::Relation& r : g.realm.relations()) {
      const realm::KingdomState* A = g.realm.kingdom(r.a);
      const realm::KingdomState* B = g.realm.kingdom(r.b);
      if (!A || !B) continue;
      if ((A->hiddenLand == land) != (B->hiddenLand == land)) n += wars ? (g.realm.atWar(r.a, r.b) ? 1 : 0) : 1;
    }
    return n;
  };
  int hiddenK = 0, isolated = 0;
  for (const realm::KingdomState& k : g.realm.kingdoms()) if (k.hiddenLand == land) { hiddenK++; if (k.isolated) isolated++; }
  if (isolated != hiddenK) SEA_FAIL("seed %s: %d of land %u's %d kingdoms start isolated\n", S(seed), isolated, land, hiddenK);
  if (crossing(false)) SEA_FAIL("seed %s: %d relations cross the Mist before contact\n", S(seed), crossing(false));
  // a forced war across the Mist is refused (declareWar keeps the rule)
  realm::Gid hid = 0, mid = 0;
  for (const realm::KingdomState& k : g.realm.kingdoms()) { if (k.hiddenLand == land && !hid) hid = k.id; if (!k.hiddenLand && !mid) mid = k.id; }
  if (hid && mid && g.realm.forceWar(mid, hid, g.day) != 0) SEA_FAIL("seed %s: a war was declared across the Mist before contact\n", S(seed));
  g.realm.advanceTo(src, g.day + 60);
  if (crossing(false)) SEA_FAIL("seed %s: after 60 days, %d relations cross the Mist before contact\n", S(seed), crossing(false));
  // first contact: the land's kingdoms open, wary of the stranger
  const int opened = g.realm.contactLand(land, g.realm.day(), true);
  int stillIso = 0, wary = 0;
  for (const realm::KingdomState& k : g.realm.kingdoms()) if (k.hiddenLand == land) { stillIso += k.isolated; wary += k.playerRep <= -20 ? 1 : 0; }
  bool evt = false;
  for (const realm::WorldEvent& e : g.realm.events()) if (e.type == realm::EvType::FirstContact && e.b == 0) evt = true;
  if (opened != hiddenK || stillIso) SEA_FAIL("seed %s: first contact opened %d of %d kingdoms (%d still isolated)\n", S(seed), opened, hiddenK, stillIso);
  if (!evt) SEA_FAIL("seed %s: first contact made no FirstContact event\n", S(seed));
  if (wary != hiddenK) SEA_FAIL("seed %s: the hidden people are not wary at first (%d of %d)\n", S(seed), wary, hiddenK);
  // three jobs done for them make the hero a friend (Friendly 25)
  if (hid) {
    Site s;
    s.type = SiteType::Village;
    s.name = "TEST HAMLET";
    s.kingdom = g.world.addKingdom(hid);
    g.world.sites.push_back(s);
    Quest q;
    q.giverSite = (int)g.world.sites.size() - 1;
    for (int k = 0; k < 3; k++) SeaOps::questDone(g, q);
    if (g.realm.rep(hid) < 25) SEA_FAIL("seed %s: three quests left the hidden people at rep %d (need Friendly 25)\n", S(seed), g.realm.rep(hid));
    out("sea: seed %s: land %u: %d kingdoms isolated 60 days, opened on contact; rep -20 -> %d after three jobs\n", S(seed), land, hiddenK, g.realm.rep(hid));
  }
  // the word carried home: a relation across the water
  if (hid && mid) {
    g.realm.openContact(land, mid, g.realm.day());
    if (!g.realm.relation(hid, mid)) SEA_FAIL("seed %s: the word carried home opened no relation\n", S(seed));
  }
}

// ---- storms, sinking
void checkStormSink(uint64_t seed) {
  Game g = makeGame(seed);
  if (!toMistEdge(g, 200)) { out("sea: seed %s: SKIP storms (no open sea found)\n", S(seed)); return; }
  std::vector<Event> ev;
  SeaOps::beginStorm(g, g.pl().p, 0.95f, 80.0f);
  g.sea.live.storm.radius = 50.0f * TILE;
  const float hp0 = g.sea.boats[(size_t)g.sea.aboard].hp;
  run(g, 24.0f, Input(), &ev);
  const sea::Sea::Live& L = g.sea.live;
  out("sea: seed %s: storm: dark %.2f rough %.2f drift %.2f, %d lightning, sloop hull %.0f -> %.0f\n", S(seed), L.stormDark, L.rough, L.drift,
      seaEvents(ev, sea::SeaEv::Lightning), hp0, g.sea.atSea() ? g.sea.boats[(size_t)g.sea.aboard].hp : -1.0f);
  if (L.stormDark < 0.3f || L.rough < 0.3f || L.drift < 0.3f) SEA_FAIL("seed %s: a storm on the boat: dark %.2f rough %.2f drift %.2f\n", S(seed), L.stormDark, L.rough, L.drift);
  if (!seaEvents(ev, sea::SeaEv::Lightning)) SEA_FAIL("seed %s: no lightning in 24 s of storm\n", S(seed));
  if (!seaEvents(ev, sea::SeaEv::StormBegin)) SEA_FAIL("seed %s: no StormBegin event\n", S(seed));
  // sinking: the hull breached, the boat goes down, the hero washes ashore behind the fade
  if (!g.sea.atSea()) { SEA_FAIL("seed %s: not aboard for the sinking\n", S(seed)); return; }
  const int bi = g.sea.aboard;
  SeaOps::hurtBoat(g, 99999.0f, g.pl().p);
  ev.clear();
  run(g, 2.5f, Input(), &ev);
  runTravel(g, &ev);
  run(g, 0.5f, Input(), &ev);
  const Map& m = g.map();
  const int tx = (int)std::floor(g.pl().p.x / TILE), ty = (int)std::floor(g.pl().p.y / TILE);
  if (!g.sea.boats[(size_t)bi].sunk) SEA_FAIL("seed %s: the breached boat is not sunk\n", S(seed));
  if (g.sea.atSea() || g.pl().swim) SEA_FAIL("seed %s: after the sinking the hero is still aboard\n", S(seed));
  if (m.in(tx, ty) && groundWater(m.at(tx, ty)))
    SEA_FAIL("seed %s: after the sinking the hero stands in the water at %d,%d (travelling %d, lastTown %d, ground %d, wash %d)\n", S(seed), SeaOps::heroGx(g),
             SeaOps::heroGy(g), g.travelling() ? 1 : 0, g.lastTown, (int)m.at(tx, ty), seaEvents(ev, sea::SeaEv::WashAshore));
  if (!seaEvents(ev, sea::SeaEv::Sink) && !seaEvents(ev, sea::SeaEv::WashAshore)) SEA_FAIL("seed %s: no Sink / WashAshore events\n", S(seed));
  if (SeaOps::boatNear(g, 1e6f) == bi) SEA_FAIL("seed %s: a sunk boat can still be boarded\n", S(seed));
}

// a land tile next to the shallows near the hero (global), facing the water: false none
bool shoreNear(Game& g, int32_t& gx, int32_t& gy, Vec2& face) {
  const Map& m = g.world.over;
  const int cx = (int)std::floor(g.pl().p.x / TILE), cy = (int)std::floor(g.pl().p.y / TILE);
  for (int r = 1; r < 120; r++)
    for (int dy = -r; dy <= r; dy++)
      for (int dx = -r; dx <= r; dx++) {
        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
        const int tx = cx + dx, ty = cy + dy;
        if (!m.in(tx, ty) || m.blocked(tx, ty) || groundWater(m.at(tx, ty)) || m.at(tx, ty) == Ground::Void || m.at(tx, ty) == Ground::Bridge) continue;
        static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& d : D) {
          // three tiles of shallow water straight out (room for a swim and a cast)
          bool ok = true;
          for (int s = 1; s <= 5 && ok; s++) {
            const int wx = tx + d[0] * s, wy = ty + d[1] * s;
            ok = m.in(wx, wy) && m.at(wx, wy) == Ground::Water && !m.solid[(size_t)wy * m.w + wx] && m.propAt(wx, wy) == 0;
          }
          if (!ok) continue;
          gx = g.world.ox + tx; gy = g.world.oy + ty;
          face = Vec2((float)d[0], (float)d[1]);
          return true;
        }
      }
  return false;
}

void checkSwimFish(uint64_t seed) {
  for (int sailor = 0; sailor < 2; sailor++) {
    Game g = makeGame(seed);
    g.godMode = false;
    g.pl().hp = g.pl().maxHp = 99999;
    if (sailor) g.background = Background::Sailor;
    int32_t gx = 0, gy = 0;
    Vec2 face;
    ew::PortPlan P0;
    if (!shoreNear(g, gx, gy, face) && !(toPort(g, P0) && shoreNear(g, gx, gy, face))) {
      out("sea: seed %s: SKIP swimming (no shallows near the start or its port)\n", S(seed));
      return;
    }
    g.teleportGlobal(gx, gy);
    run(g, 0.3f, Input());
    g.stamina = 200;
    Input in;
    in.move = face;
    float swum = 0, pulledAt = -1, backAt = -1;
    for (int f = 0; f < 60 * 40; f++) {
      run(g, SIM_DT, (pulledAt < 0 && swum < 1.0f) ? in : Input());   // (in, then treading water)
      const sea::Sea::Live& L = g.sea.live;
      if (L.swimming && pulledAt < 0) swum += SIM_DT;
      if (L.pulled && pulledAt < 0) pulledAt = swum;
      if (pulledAt >= 0 && !L.swimming) { backAt = (float)f * SIM_DT; break; }
    }
    const float want = sailor ? sea::SAILOR_SWIM_SECS : sea::SWIM_SECS;
    out("sea: seed %s: %s swam %.1f s before tiring (limit %.0f), back on dry land: %s\n", S(seed), sailor ? "the Sailor" : "a swimmer", swum, want, backAt >= 0 ? "yes" : "no");
    if (swum < 0.5f) SEA_FAIL("seed %s: the hero never got into the shallows\n", S(seed));
    else if (pulledAt < 0) SEA_FAIL("seed %s: a swimmer never tired (swam %.1f s)\n", S(seed), swum);
    else if (std::fabs(pulledAt - want) > 1.0f && g.stamina > 1.0f) SEA_FAIL("seed %s: tired after %.1f s, not %.0f\n", S(seed), pulledAt, want);
    if (pulledAt >= 0 && backAt < 0) SEA_FAIL("seed %s: a tired swimmer was never brought back to the shore\n", S(seed));
    if (sailor) continue;
    // ---- fishing from that shore: cast, bite, strike, reel (the bot lets go when the fish runs or the line strains)
    g.teleportGlobal(gx, gy);
    run(g, 0.3f, Input());
    g.pl().aim = face;
    int landed = 0, lost = 0, casts = 0;
    for (int c = 0; c < 6; c++) {
      g.sea.live.fishCd = 0;
      g.pl().aim = face;
      if (!SeaOps::castLine(g)) { SEA_FAIL("seed %s: could not cast from the shore facing water\n", S(seed)); break; }
      casts++;
      for (int f = 0; f < 60 * 60; f++) {
        const sea::Sea::Line& ln = g.sea.live.line;
        Input fi;
        if (ln.phase == sea::Sea::Cast::Bite) fi.interact = true;
        if (ln.phase == sea::Sea::Cast::Reeling) fi.reel = ln.surge <= 0 && ln.tension < 0.62f;
        const sea::Sea::Cast before = ln.phase;
        run(g, SIM_DT, fi);
        const sea::Sea::Cast now = g.sea.live.line.phase;
        if (now == sea::Sea::Cast::Landed && before != now) landed++;
        if (now == sea::Sea::Cast::Lost && before != now) lost++;
        if (now == sea::Sea::Cast::None) break;
      }
    }
    int fishItems = 0;
    for (const Item& it : g.inv) if (it.kind == ItemKind::Food && home::ingredientOf(it) == home::Ingr::Fish && it.icon == art::Icon::Fish) fishItems += it.count;
    out("sea: seed %s: fishing: %d casts, %d landed, %d lost; %d fish in the pack; %u in the book\n", S(seed), casts, landed, lost, fishItems, g.sea.fishCaught);
    if (landed < 3) SEA_FAIL("seed %s: the reeling bot landed only %d of %d fish\n", S(seed), landed, casts);
    if (fishItems != landed || (int)g.sea.fishCaught != landed) SEA_FAIL("seed %s: %d landed but %d in the pack, %u in the book\n", S(seed), landed, fishItems, g.sea.fishCaught);
  }
}

// ---- the chart chain and first contact by landing
void checkChain(uint64_t seed) {
  Game g = makeGame(seed);
  ew::EndlessSource& src = *g.world.src;
  const int32_t hx = SeaOps::heroGx(g), hy = SeaOps::heroGy(g);
  uint32_t land = 0;
  double bd = 1e18;
  for (const ew::LandPlan& L : src.landsIn(hx - 30000, hy - 30000, hx + 30000, hy + 30000))
    if (L.hidden) { const double d = std::hypot((double)L.cx - hx, (double)L.cy - hy); if (d < bd) { bd = d; land = L.id; } }
  if (!land) { out("sea: seed %s: SKIP the chain (no hidden land)\n", S(seed)); return; }
  ew::SitePlan wreck, cave;
  if (!SeaOps::chainPlaces(g, land, wreck, cave)) { SEA_FAIL("seed %s: no chart chain places for land %u\n", S(seed), land); return; }
  SeaOps::tellOfLand(g, land, "OLD HROLF");
  const sea::LandRecord* R = g.sea.land(land);
  const Quest* q = nullptr;
  for (const Quest& qq : g.quests) if (qq.type == QType::Sea && qq.stage == 1 && (uint32_t)qq.giverId == land) q = &qq;
  if (!R || R->chain != sea::Chain::Sailor || !(R->flags & sea::LF_RUMOURED)) SEA_FAIL("seed %s: the old sailor's tale did not start the chain\n", S(seed));
  if (!q || !q->hasPos || q->tgx != wreck.ex || q->tgy != wreck.ey) SEA_FAIL("seed %s: the chain's quest does not mark the wreck\n", S(seed));
  auto sailTo = [&](int32_t x, int32_t y) {
    g.teleportGlobal(x, y);
    const ew::GTile at{x, y};
    const int bi = SeaOps::giveBoat(g, sea::Hull::Sloop, 12, "", &at);
    if (bi >= 0) { g.sea.boats[(size_t)bi].upgrades |= sea::UP_KEEL; SeaOps::board(g, bi); }
    run(g, 1.2f, Input());
  };
  sailTo(wreck.ex, wreck.ey);
  R = g.sea.land(land);
  if (!R || R->chain != sea::Chain::Wreck) SEA_FAIL("seed %s: at the wreck the chain stands at %d\n", S(seed), R ? (int)R->chain : -1);
  for (const Quest& qq : g.quests) if (qq.type == QType::Sea && qq.stage == 1 && (uint32_t)qq.giverId == land) q = &qq;
  if (q && (q->tgx != cave.ex || q->tgy != cave.ey)) SEA_FAIL("seed %s: after the wreck the quest does not mark the sea cave\n", S(seed));
  sailTo(cave.ex, cave.ey);
  if (const int ci = g.world.siteHandle(cave.id); ci >= 0 && g.world.sites[(size_t)ci].type == SiteType::Cave) {
    // the world's own sea cave: the sunken chart lies at its end (entered, its depths cleared)
    if (g.sea.atSea()) { g.sea.aboard = -1; g.pl().swim = 0; g.pl().radius = 4.5f; }
    g.debugEnterSite(ci);
    g.world.sites[(size_t)ci].cleared = true;
    run(g, 1.2f, Input());
    g.debugLeave();
  }
  R = g.sea.land(land);
  if (!R || R->chain != sea::Chain::Chart || !g.sea.hasChart(land))
    SEA_FAIL("seed %s: at the sea cave no chart (chain %d, %.0f tiles off, aboard %d)\n", S(seed), R ? (int)R->chain : -1,
             std::hypot((double)SeaOps::heroGx(g) - cave.ex, (double)SeaOps::heroGy(g) - cave.ey), g.sea.atSea() ? 1 : 0);
  // first contact: the hero's feet on the hidden land
  if (g.sea.atSea()) { g.sea.aboard = -1; g.pl().swim = 0; g.pl().radius = 4.5f; }
  const ew::LandPlan L = src.land(land);
  std::vector<Event> ev;
  g.teleportGlobal(L.cx, L.cy);
  g.realm.focusNow(src, L.cx, L.cy, g.day);
  run(g, 5.5f, Input(), &ev);
  R = g.sea.land(land);
  const bool onLand = src.landAt(SeaOps::heroGx(g), SeaOps::heroGy(g)) == land;
  if (!onLand) { out("sea: seed %s: SKIP first contact (the hidden land's heart %d,%d is not its land)\n", S(seed), L.cx, L.cy); return; }
  if (!R || !(R->flags & sea::LF_LANDED) || R->contactDay < 0) SEA_FAIL("seed %s: landing on the hidden land made no first contact\n", S(seed));
  if (!seaEvents(ev, sea::SeaEv::FirstContact)) SEA_FAIL("seed %s: no FirstContact sea event\n", S(seed));
  bool word = false;
  for (const Quest& qq : g.quests) if (qq.type == QType::Sea && qq.stage == 2 && qq.state == QState::Active) word = true;
  int iso = 0;
  for (const realm::KingdomState& k : g.realm.kingdoms()) if (k.hiddenLand == land && k.isolated) iso++;
  if (iso) SEA_FAIL("seed %s: after landing %d of the land's kingdoms are still isolated\n", S(seed), iso);
  out("sea: seed %s: chain: tale -> wreck -> cave -> chart; landed on %s: first contact, word-to-the-king quest %s\n", S(seed),
      L.name.c_str(), word ? "given" : "NOT given");
  if (!word) SEA_FAIL("seed %s: no quest to tell a mainland king of the land\n", S(seed));
}

// ---- a ferry passage on deck
void checkFerry(uint64_t seed) {
  Game g = makeGame(seed);
  g.godMode = true;   // (the passage is under test, not the hero: boarders can't end it early)
  g.pl().hp = g.pl().maxHp = 99999;
  ew::PortPlan P;
  // (ferries sail between harbours; until the WORLD lane builds them near every start, the passage's machinery is
  // tested between the nearest two ports of any kind)
  const bool harbours = toPort(g, P, true, 12000);
  if (!harbours && !toPort(g, P, false)) { out("sea: seed %s: SKIP the ferry (no port near the start)\n", S(seed)); return; }
  ew::EndlessSource& src = *g.world.src;
  ew::PortPlan T;
  bool have = false;
  double bd = 1e18;
  for (int pass = 0; pass < 2 && !have; pass++)   // (harbours first; then any port: the passage's machinery is the same)
    for (const ew::PortPlan& p : src.portsIn(P.x - 9000, P.y - 9000, P.x + 9000, P.y + 9000)) {
      if (p.site == P.site || (harbours && pass == 0 && !p.harbour) || src.land(p.land).hidden) continue;
      const double d = std::hypot((double)p.x - P.x, (double)p.y - P.y);
      if (d < bd) { bd = d; T = p; have = true; }
    }
  if (!have) { out("sea: seed %s: SKIP the ferry (no second port within 9000 tiles of %s)\n", S(seed), P.name.c_str()); return; }
  g.gold = 1000;
  const int day0 = g.day;
  const float hour0 = g.hour;
  if (!SeaOps::beginVoyage(g, T.site, T.pierX, T.pierY, 50, (int)bd)) { SEA_FAIL("seed %s: no ferry put out from %s\n", S(seed), P.name.c_str()); return; }
  g.sea.live.voyage.event = 1;   // (pirates: the fight on deck)
  g.sea.live.voyage.eventAt = 6.0f;
  std::vector<Event> ev;
  run(g, 4.0f, Input(), &ev);
  // a save mid-voyage loads mid-voyage
  {
    std::vector<uint8_t> a, b;
    g.serialize(a);
    Game h(seed);
    if (!h.deserialize(a)) SEA_FAIL("seed %s: a save mid-voyage did not load\n", S(seed));
    else {
      if (!h.sea.live.voyage.on || !h.sea.atSea() || !h.sea.boats[(size_t)h.sea.aboard].ferry) SEA_FAIL("seed %s: a save mid-voyage loaded off the ferry\n", S(seed));
      h.serialize(b);
      if (a != b) SEA_FAIL("seed %s: a save mid-voyage is not byte-identical after a load\n", S(seed));
    }
  }
  // the event: pirates close in and board; the crew (the hero) fights them off
  int crewSeen = 0;
  bool boarded = false;
  for (int k = 0; k < 120 && g.sea.live.voyage.on && g.sea.live.voyage.phase <= 1; k++) {
    run(g, 0.5f, Input(), &ev);
    for (const sea::Sea::Ship& s : g.sea.live.ships) boarded |= s.boarded;
    int crew = 0;
    for (size_t i = 1; i < g.actors.size(); i++) if (g.actors[i].role == Role::Pirate && g.actors[i].st != AState::Dead) crew++;
    crewSeen = std::max(crewSeen, crew);
    if (boarded)   // (the hero's blades: the boarders fall one by one)
      for (size_t i = 1; i < g.actors.size(); i++)
        if (g.actors[i].role == Role::Pirate && g.actors[i].st != AState::Dead && (k % 4) == 0) { g.debugFell(g.actors[i].id, true); break; }
  }
  const float heroHp = g.pl().hp;
  run(g, 90.0f, Input(), &ev);
  runTravel(g, &ev);
  run(g, 0.5f, Input(), &ev);
  const float dPier = std::hypot((float)(SeaOps::heroGx(g) - T.pierX), (float)(SeaOps::heroGy(g) - T.pierY));
  const float hours = (float)(g.day - day0) * 24.0f + g.hour - hour0;
  out("sea: seed %s: ferry %s -> %s (%.0f tiles): %d pirates seen%s, arrived %.0f tiles from the pier, %.1f h passed, %d voyage events\n", S(seed),
      P.name.c_str(), T.name.c_str(), bd, crewSeen, boarded ? " (boarded)" : "", dPier, hours, seaEvents(ev, sea::SeaEv::VoyageEvent));
  if (!crewSeen) SEA_FAIL("seed %s: the pirates' event brought no pirates\n", S(seed));
  if (!boarded) SEA_FAIL("seed %s: the pirates never came alongside to board\n", S(seed));
  if (g.sea.live.voyage.on || g.sea.atSea()) SEA_FAIL("seed %s: the passage never ended\n", S(seed));
  if (dPier > 25.0f) SEA_FAIL("seed %s: journey's end %.0f tiles from %s's pier\n", S(seed), dPier, T.name.c_str());
  for (const sea::Boat& b : g.sea.boats) if (b.ferry) SEA_FAIL("seed %s: the ferry's hull stayed among the player's boats\n", S(seed));
  if (hours < 1.0f) SEA_FAIL("seed %s: the clock did not run fast on deck (%.2f h)\n", S(seed), hours);
  if (!g.sea.knowsPort(T.site)) SEA_FAIL("seed %s: the destination is not a known port after the passage\n", S(seed));
  (void)heroHp;
}

// ---- the harbour's people: buying a boat, the yard (repair, an upgrade, a sunk boat raised), the routes, a
//      fishmonger's order, the Sailor's skiff
int talkPick(Game& g, int actorId, int action) {   // the option of that action (DLG_SEA + k) after the talk hook: index
  const int i = SeaOps::findActor(g, actorId);
  if (i < 0) return -1;
  g.dlg = Dialogue();
  g.dlg.actor = actorId;
  SeaOps::talk(g, g.actors[(size_t)i]);
  for (int k = 0; k < (int)g.dlg.opts.size(); k++) if (g.dlg.opts[(size_t)k].action == action) return k;
  return -1;
}
void checkHarbour(uint64_t seed) {
  Game g = makeGame(seed);
  ew::PortPlan P;
  if (!toPort(g, P)) { out("sea: seed %s: SKIP the harbour (no port)\n", S(seed)); return; }
  run(g, 2.0f, Input());   // (the dock folk come to the pier)
  int hm = -1, sw = -1, old = -1;
  for (const Actor& a : g.actors) {
    if (a.role == Role::Harbourmaster && hm < 0) hm = a.id;
    if (a.role == Role::Shipwright && sw < 0) sw = a.id;
    if (a.role == Role::Sailor && old < 0) old = a.id;
  }
  if (hm < 0) { SEA_FAIL("seed %s: no harbourmaster at %s's pier\n", S(seed), P.name.c_str()); return; }
  if (P.harbour && sw < 0) SEA_FAIL("seed %s: no shipwright at %s's harbour\n", S(seed), P.name.c_str());
  if (old < 0) SEA_FAIL("seed %s: no old sailor on %s's pier\n", S(seed), P.name.c_str());
  // a rowboat bought at the 2.12 price, lying at the berth, moored at the port
  g.gold = 30000;
  g.plLevel = 20;
  const int k = talkPick(g, hm, DLG_SEA + 10);
  if (k < 0) { SEA_FAIL("seed %s: the harbourmaster sells no boat\n", S(seed)); return; }
  const size_t boats0 = g.sea.boats.size();
  const DlgOpt o = g.dlg.opts[(size_t)k];
  SeaOps::choose(g, o);
  if (g.sea.boats.size() != boats0 + 1) { SEA_FAIL("seed %s: buying a %s gave no boat ('%s')\n", S(seed), sea::hullName((sea::Hull)o.arg), g.dlg.text.c_str()); return; }
  const sea::Boat& B = g.sea.boats.back();
  if (B.dock != P.site || g.gold != 30000 - sea::hullPrice(B.hull, false)) SEA_FAIL("seed %s: the bought boat: dock %s, paid %d\n", S(seed), B.dock == P.site ? "ok" : "no", 30000 - g.gold);
  // the yard: a damaged hull repaired, an upgrade fitted, a sunk boat raised
  if (sw >= 0) {
    g.sea.boats.back().hp = 10;
    int r = talkPick(g, sw, DLG_SEA + 40);
    if (r < 0) SEA_FAIL("seed %s: the shipwright offers no repair for a battered hull\n", S(seed));
    else { SeaOps::choose(g, g.dlg.opts[(size_t)r]); if (g.sea.boats.back().hp < sea::maxHp(g.sea.boats.back()) - 0.5f) SEA_FAIL("seed %s: the repair left the hull at %.0f\n", S(seed), g.sea.boats.back().hp); }
    r = talkPick(g, sw, DLG_SEA + 50);
    if (r < 0) SEA_FAIL("seed %s: the shipwright offers no upgrade\n", S(seed));
    else { const uint16_t up0 = g.sea.boats.back().upgrades; SeaOps::choose(g, g.dlg.opts[(size_t)r]); if (g.sea.boats.back().upgrades == up0) SEA_FAIL("seed %s: the upgrade was not fitted\n", S(seed)); }
    g.sea.boats.back().sunk = true;
    r = talkPick(g, sw, DLG_SEA + 60);
    if (r < 0) SEA_FAIL("seed %s: the shipwright offers no new hull for a sunk boat\n", S(seed));
    else { SeaOps::choose(g, g.dlg.opts[(size_t)r]); if (g.sea.boats.back().sunk || g.sea.boats.back().dock != P.site) SEA_FAIL("seed %s: the sunk boat was not raised at the port\n", S(seed)); }
  }
  // the old sailor's tale starts the chart chain
  const int t = talkPick(g, old, DLG_SEA + 70);
  if (t < 0) SEA_FAIL("seed %s: the old sailor tells no tale\n", S(seed));
  else {
    SeaOps::choose(g, g.dlg.opts[(size_t)t]);
    bool chain = false;
    for (const sea::LandRecord& r : g.sea.lands) chain |= r.chain == sea::Chain::Sailor;
    if (!chain) SEA_FAIL("seed %s: the old sailor's tale did not start a chain ('%s')\n", S(seed), g.dlg.text.c_str());
  }
  // a fishmonger's order, filled with the catch and handed in
  {
    Actor f;
    f.id = 900001; f.npc = true; f.human = true; f.role = Role::Fisher; f.p = g.pl().p; f.site = -1;
    g.actors.push_back(f);
    const int fk = talkPick(g, f.id, DLG_SEA + 90);
    if (fk < 0) SEA_FAIL("seed %s: a fisher offers no order\n", S(seed));
    else {
      SeaOps::choose(g, g.dlg.opts[(size_t)fk]);
      Quest* q = nullptr;
      for (Quest& qq : g.quests) if (qq.type == QType::Sea && qq.stage == 3) q = &qq;
      if (!q) SEA_FAIL("seed %s: the fishmonger's order made no quest\n", S(seed));
      else {
        int sp = -1;
        for (int i = 0; i < (int)sea::species().size(); i++) if (q->subject == sea::species()[(size_t)i].name) sp = i;
        for (int i = 0; i < q->need; i++) { g.inv.push_back(SeaOps::fishItem(sp, 40)); SeaOps::seaQuestStep(g, 3, 0, 0, 0, q->subject, false); }
        if (q->state != QState::Complete) SEA_FAIL("seed %s: the fish order is not complete after the catch\n", S(seed));
        const int gold0 = g.gold;
        q->state = QState::Complete;
        size_t fish0 = 0;
        for (const Item& it : g.inv) if (it.name == q->subject) fish0 += (size_t)it.count;
        SeaOps::questDone(g, *q);
        size_t fish1 = 0;
        for (const Item& it : g.inv) if (it.name == q->subject) fish1 += (size_t)it.count;
        if (fish0 - fish1 != (size_t)q->need) SEA_FAIL("seed %s: handing in the order took %zu fish, not %d\n", S(seed), fish0 - fish1, q->need);
        (void)gold0;
      }
    }
  }
  // the Sailor (15.1): a skiff of their own by the start, else the first boat at half price
  {
    Game s2 = makeGame(seed);
    s2.background = Background::Sailor;
    const size_t n0 = s2.sea.boats.size();
    SeaOps::background(s2);
    bool sale = false;
    for (const auto& m : s2.marks) sale |= m.second == 1;
    if (s2.sea.boats.size() == n0 && !sale) SEA_FAIL("seed %s: the Sailor got neither a skiff nor an early sale\n", S(seed));
  }
  out("sea: seed %s: harbour %s: a boat bought, the yard's work done, the old sailor's tale, a fish order filled\n", S(seed), P.name.c_str());
}

// ---- the sea's creatures: every blow telegraphed, the world bosses
void checkCreatures(uint64_t seed) {
  Game g = makeGame(seed);
  if (!toMistEdge(g, 600)) { out("sea: seed %s: SKIP the creatures (no open sea)\n", S(seed)); return; }
  g.godMode = true;   // (the hull takes the blows: hurtBoat ignores godMode; the hero must outlive the level-ups)
  g.pl().hp = g.pl().maxHp = 99999;
  sea::Boat& b = g.sea.boats[(size_t)g.sea.aboard];
  b.hull = sea::Hull::Galleon;
  b.hp = 99999;
  struct Case { art::Monster m; uint8_t kind; const char* name; };
  const Case cases[3] = {{art::Monster::SeaSerpent, 1, "serpent"}, {art::Monster::Kraken, 2, "kraken"}, {art::Monster::Merrow, 4, "merrow"}};
  for (const Case& c : cases) {
    std::vector<Event> ev;
    Vec2 at;
    bool ok = false;
    for (int t = 0; t < 40 && !ok; t++) {
      at = g.pl().p + Vec2(std::cos(t * 0.7f), std::sin(t * 0.7f)) * (7.0f * TILE);
      ok = groundWater(g.map().at((int)std::floor(at.x / TILE), (int)std::floor(at.y / TILE)));
    }
    if (!ok) continue;
    const int id = SeaOps::spawnSwimmer(g, c.m, at, 10, c.kind);
    const float hp0 = g.pl().hp, hull0 = g.sea.boats[(size_t)g.sea.aboard].hp;
    run(g, 3.0f, Input(), &ev);
    if (const int i = SeaOps::findActor(g, id); i >= 0) {
      const Actor& a = g.actors[(size_t)i];
      int rk = -1, mv = -1;
      float st = -1;
      for (const sea::Sea::Swimmer& w : g.sea.live.swimmers) if (w.id == id) { rk = w.kind; mv = w.move; st = w.t; }
      out("sea: seed %s: (%s after 3 s: state %d aggro %d dist %.0f hp %.0f swim %d aggroR %.0f; record kind %d move %d t %.1f)\n", S(seed), c.name, (int)a.st,
          a.aggro ? 1 : 0, len(a.p - g.pl().p), a.hp, a.swim, a.aggroR, rk, mv, st);
    } else out("sea: seed %s: (%s gone after 3 s)\n", S(seed), c.name);
    run(g, 22.0f, Input(), &ev);
    const int tele = seaEvents(ev, sea::SeaEv::Telegraph), tent = seaEvents(ev, sea::SeaEv::Tentacle);
    const bool hurt = g.pl().hp < hp0 || g.sea.boats[(size_t)g.sea.aboard].hp < hull0;
    out("sea: seed %s: a %s round the boat for 25 s: %d telegraphs, %d tentacles, the boat %s\n", S(seed), c.name, tele, tent, hurt ? "struck" : "untouched");
    if (tele < 2) SEA_FAIL("seed %s: the %s telegraphed only %d blows in 25 s\n", S(seed), c.name, tele);
    if (!hurt) SEA_FAIL("seed %s: the %s never struck the boat\n", S(seed), c.name);
    if (c.m == art::Monster::Kraken && tent < 1) SEA_FAIL("seed %s: no kraken tentacle rose\n", S(seed));
    // every blow came after its telegraph: no damage lands without a Telegraph at least 0.3 s before it
    if (const int i = SeaOps::findActor(g, id); i >= 0) g.debugFell(id, true);
    for (size_t i = 1; i < g.actors.size(); i++) if (g.actors[i].swim && g.actors[i].st != AState::Dead) g.debugFell(g.actors[i].id, true);
    run(g, 1.0f, Input());
  }
  // a world boss of the sea rises near its lair, and stays slain
  ew::EndlessSource& src = *g.world.src;
  const int32_t hx = SeaOps::heroGx(g), hy = SeaOps::heroGy(g);
  foes::SeaBoss B;
  bool found = false;
  for (int r = 0; r <= 3 && !found; r++)
    for (int dy = -r; dy <= r && !found; dy++)
      for (int dx = -r; dx <= r && !found; dx++) {
        bool ok = false;
        const foes::SeaBoss c = foes::seaBossOf(src, foes::seaCellOf(hx) + dx, foes::seaCellOf(hy) + dy, ok);
        if (ok) { B = c; found = true; }
      }
  if (!found) { out("sea: seed %s: SKIP the sea's world boss (no lair within 3 sea cells)\n", S(seed)); return; }
  g.noWildSpawns = false;
  g.teleportGlobal(B.lairX, B.lairY);
  const ew::GTile at{B.lairX, B.lairY};
  const int bi = SeaOps::giveBoat(g, sea::Hull::Galleon, 20, "", &at);
  if (bi < 0 || !SeaOps::board(g, bi)) { out("sea: seed %s: SKIP the sea's world boss (no water at its lair)\n", S(seed)); return; }
  g.sea.boats[(size_t)bi].hp = 99999;
  run(g, 3.0f, Input());
  int boss = -1, swimmers = 0;
  for (const Actor& a : g.actors) { if (a.unique == B.id && a.st != AState::Dead) boss = a.id; if (a.swim && !a.player) swimmers++; }
  if (boss < 0)
    out("sea: seed %s: (boss check: hero %d,%d, %.0f tiles from the lair, aboard %d, swimmers %d, records %zu, encT %.2f rollT %.1f, nowild %d, sink %.1f moor %d ferry %d)\n", S(seed), SeaOps::heroGx(g), SeaOps::heroGy(g),
        std::hypot((double)SeaOps::heroGx(g) - B.lairX, (double)SeaOps::heroGy(g) - B.lairY), g.sea.atSea() ? 1 : 0, swimmers, g.sea.live.swimmers.size(),
        g.sea.live.encounterT, g.sea.live.rollT, g.noWildSpawns ? 1 : 0, g.sea.live.sinkT, g.sea.live.mooring ? 1 : 0, g.sea.atSea() && g.sea.boats[(size_t)g.sea.aboard].ferry ? 1 : 0);
  out("sea: seed %s: the sea's world boss %s (%s) at %d,%d: %s\n", S(seed), B.name.c_str(), B.kind.c_str(), B.lairX, B.lairY, boss >= 0 ? "rose" : "did NOT rise");
  if (boss < 0) { SEA_FAIL("seed %s: %s did not rise at its lair\n", S(seed), B.name.c_str()); return; }
  g.debugFell(boss, true);
  run(g, 4.0f, Input());
  int again = 0;
  for (const Actor& a : g.actors) if (a.unique == B.id && a.st != AState::Dead) again++;
  if (again) SEA_FAIL("seed %s: %s rose again after it was slain\n", S(seed), B.name.c_str());
}

int seaCmd(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) {
      const char* s = argv[++i];
      a = std::strtoull(s, nullptr, 10);
      const char* dots = std::strstr(s, "..");
      b = dots ? std::strtoull(dots + 2, nullptr, 10) : a;
    }
  }
  m_bad = 0;
  checkTables();
  checkBlock();
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    checkWorld(s);
    checkPlay(s);
    checkMist(s);
    checkIsolation(s);
    checkStormSink(s);
    checkSwimFish(s);
    checkChain(s);
    checkFerry(s);
    checkHarbour(s);
    checkCreatures(s);
  }
  out("sea: %s (%d failures)\n", m_bad ? "FAIL" : "OK", m_bad);
  return m_bad ? 1 : 0;
}

// ---------------------------------------------------------------- --sail: the captain bot to continent 2
struct Route { std::vector<std::pair<int32_t, int32_t>> pts; bool ok = false; int cells = 0; int32_t landX = 0, landY = 0; };
// a water route on a coarse lattice (C-tile cells judged at their centres) from (sx, sy) to a water cell beside the
// target land; keel: the storm sea allowed
Route planRoute(ew::EndlessSource& src, int32_t sx, int32_t sy, uint32_t target, const ew::LandPlan& T, bool keel, int C) {
  Route R;
  auto key = [](int32_t x, int32_t y) { return ((uint64_t)(uint32_t)x << 32) | (uint32_t)y; };
  std::unordered_map<uint64_t, uint8_t> water;   // 1 water we may sail, 2 not, 3 the target's land
  auto kindOf = [&](int32_t cx, int32_t cy) -> uint8_t {
    const uint64_t k = key(cx, cy);
    auto it = water.find(k);
    if (it != water.end()) return it->second;
    const int32_t x = cx * C + C / 2, y = cy * C + C / 2;
    // open sea a sloop can sail, with room round the cell's heart (rivers and lakes are no road to another continent)
    auto sailable = [&](int32_t px, int32_t py) {
      const ew::SeaKind s = src.seaAt(px, py);
      if (s == ew::SeaKind::Deep || (keel && s == ew::SeaKind::Storm)) return true;
      return s == ew::SeaKind::Shallow && src.macro(px, py).eco == Eco::Ocean;
    };
    uint8_t v = 2;
    const int r = C / 3;
    // (round the harbour the boat lies in, any water will do: a harbour is a tight place)
    const bool nearStart = std::max(std::abs(cx - (int32_t)std::floor((double)sx / C)), std::abs(cy - (int32_t)std::floor((double)sy / C))) <= 3;
    const int n = (int)sailable(x, y) + (int)sailable(x + r, y) + (int)sailable(x - r, y) + (int)sailable(x, y + r) + (int)sailable(x, y - r);
    if (n == 5 || (nearStart && n >= 1)) v = 1;
    else if (src.seaAt(x, y) == ew::SeaKind::Land && src.landAt(x, y) == target) v = 3;
    water[k] = v;
    return v;
  };
  const int32_t c0x = (int32_t)std::floor((double)sx / C), c0y = (int32_t)std::floor((double)sy / C);
  struct N { float f; int32_t x, y; };
  auto cmp = [](const N& a, const N& b) { return a.f > b.f; };
  std::priority_queue<N, std::vector<N>, decltype(cmp)> open(cmp);
  std::unordered_map<uint64_t, uint64_t> from;
  std::unordered_map<uint64_t, float> gs;
  const float tx = (float)T.cx / C, ty = (float)T.cy / C;
  open.push({0, c0x, c0y});
  gs[key(c0x, c0y)] = 0;
  from[key(c0x, c0y)] = key(c0x, c0y);
  uint64_t goal = 0;
  bool found = false;
  int expanded = 0;
  while (!open.empty() && expanded < 400000) {
    const N n = open.top();
    open.pop();
    expanded++;
    const uint64_t nk = key(n.x, n.y);
    // beside the target's shore: done
    for (int d = 0; d < 8 && !found; d++) {
      static const int D[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
      if (kindOf(n.x + D[d][0], n.y + D[d][1]) == 3) { found = true; goal = nk; R.landX = (n.x + D[d][0]) * C + C / 2; R.landY = (n.y + D[d][1]) * C + C / 2; }
    }
    if (found) break;
    const float g0 = gs[nk];
    for (int d = 0; d < 8; d++) {
      static const int D[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
      const int32_t x = n.x + D[d][0], y = n.y + D[d][1];
      if (kindOf(x, y) != 1) continue;
      if (d >= 4 && (kindOf(n.x + D[d][0], n.y) != 1 || kindOf(n.x, n.y + D[d][1]) != 1)) continue;   // (no corner cutting)
      const float ng = g0 + (d >= 4 ? 1.414f : 1.0f);
      const uint64_t k = key(x, y);
      auto it = gs.find(k);
      if (it != gs.end() && it->second <= ng) continue;
      gs[k] = ng;
      from[k] = nk;
      open.push({ng + 1.05f * std::hypot((float)x - tx, (float)y - ty), x, y});
    }
  }
  R.cells = expanded;
  if (!found) return R;
  for (uint64_t k = goal;; k = from[k]) {
    R.pts.push_back({(int32_t)(k >> 32) * C + C / 2, (int32_t)(uint32_t)k * C + C / 2});
    if (from[k] == k) break;
  }
  std::reverse(R.pts.begin(), R.pts.end());
  R.ok = true;
  return R;
}

int sailCmd(int argc, char** argv) {
  uint64_t a = 1, b = 5;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) {
      const char* s = argv[++i];
      a = std::strtoull(s, nullptr, 10);
      const char* dots = std::strstr(s, "..");
      b = dots ? std::strtoull(dots + 2, nullptr, 10) : a;
    }
  }
  m_bad = 0;
  using clk = std::chrono::steady_clock;
  for (uint64_t seed = a; seed <= b; seed++) {
    g_curSeed = seed;
    Game g = makeGame(seed);
    g.streamThreads = false;   // (the phone's path: no worker, the frame pump generates within 3 ms a frame)
    g.world.streamer.reset();
    ew::EndlessSource& src = *g.world.src;
    ew::PortPlan P;
    if (!toPort(g, P, true) && !toPort(g, P, false)) { out("sail: seed %s: SKIP (no port within 6000 tiles of the start)\n", S(seed)); continue; }
    const uint32_t home = src.landAt(P.x, P.y);
    // continent 2: the nearest land that is neither the start's nor hidden (a continent first)
    ew::LandPlan T;
    double bd = 1e18;
    for (int pass = 0; pass < 2 && !T.id; pass++)
      for (const ew::LandPlan& L : src.landsIn(P.x - 24000, P.y - 24000, P.x + 24000, P.y + 24000)) {
        if (L.id == home || L.hidden || L.origin || (pass == 0 && L.kind != ew::LandKind::Continent)) continue;
        const double d = std::hypot((double)L.cx - P.x, (double)L.cy - P.y);
        if (d < bd) { bd = d; T = L; }
      }
    if (!T.id) { SEA_FAIL("seed %s: no second continent within 24000 tiles\n", S(seed)); continue; }
    int bi = SeaOps::buyBoat(g, sea::Hull::Sloop, P.site);
    if (bi < 0) bi = SeaOps::giveBoat(g, sea::Hull::Sloop, 16);
    if (bi < 0 || !SeaOps::board(g, bi)) { SEA_FAIL("seed %s: no sloop at %s\n", S(seed), P.name.c_str()); continue; }
    bool keel = false;
    auto t0 = clk::now();
    Route R = planRoute(src, SeaOps::heroGx(g), SeaOps::heroGy(g), T.id, T, false, 24);
    if (!R.ok) { keel = true; R = planRoute(src, SeaOps::heroGx(g), SeaOps::heroGy(g), T.id, T, true, 24); }
    const double planMs = std::chrono::duration<double, std::milli>(clk::now() - t0).count();
    if (!R.ok) { SEA_FAIL("seed %s: no water route from %s to %s (%d cells searched)\n", S(seed), P.name.c_str(), T.name.c_str(), R.cells); continue; }
    if (keel) g.sea.boats[(size_t)bi].upgrades |= sea::UP_KEEL;
    // follow it: the autopilot holds the heading to the next waypoint (beating to windward when it must)
    const int sync0 = g.world.sstats.syncChunks;
    double worstMs = 0, worstUpd = 0;
    g.sea.live.stepMsWorst = 0;
    float simT = 0, sailed = 0;
    size_t wp = 0;
    Vec2 prevG((float)SeaOps::heroGx(g), (float)SeaOps::heroGy(g));
    int stuckFor = 0, escapes = 0;
    bool arrived = false;
    int aheadReady = 0, aheadAll = 0, aheadMin = 1 << 30;
    while (simT < 3600.0f && !arrived) {
      const int32_t hx = SeaOps::heroGx(g), hy = SeaOps::heroGy(g);
      while (wp + 1 < R.pts.size() && std::hypot((double)R.pts[wp].first - hx, (double)R.pts[wp].second - hy) < 30.0) wp++;
      // the last leg: from the last waypoint straight for the target's shore (she runs up to the beach)
      const bool last = wp + 1 >= R.pts.size() && std::hypot((double)R.pts[wp].first - hx, (double)R.pts[wp].second - hy) < 30.0;
      const double ax = (last ? R.landX : R.pts[wp].first) - hx, ay = (last ? R.landY : R.pts[wp].second) - hy;
      if (!g.sea.atSea()) {
        const int tx = (int)std::floor(g.pl().p.x / TILE), ty = (int)std::floor(g.pl().p.y / TILE);
        out("sail: seed %s: (off the boat at %d,%d on ground %d after %.0f s)\n", S(seed), hx, hy, g.world.over.in(tx, ty) ? (int)g.world.over.at(tx, ty) : -1, simT);
        break;
      }
      g.sea.live.autopilot = (float)std::atan2(ay, ax);
      if (stuckFor > 0) { g.sea.live.autopilot += (escapes & 1) ? 1.2f : -1.2f; stuckFor--; }
      const auto s0 = clk::now();
      g.update(SIM_DT, Input());
      const auto s1 = clk::now();
      g.frameWork(3.0);
      worstMs = std::max(worstMs, std::chrono::duration<double, std::milli>(clk::now() - s0).count());
      worstUpd = std::max(worstUpd, std::chrono::duration<double, std::milli>(s1 - s0).count());
      g.events.clear();
      if (g.mode != Mode::Play) g.mode = Mode::Play;
      simT += SIM_DT;
      if (g.sea.atSea()) g.sea.boats[(size_t)g.sea.aboard].hp = sea::maxHp(g.sea.boats[(size_t)g.sea.aboard]);   // (the bot's crew keeps her afloat)
      const Vec2 nowG((float)SeaOps::heroGx(g), (float)SeaOps::heroGy(g));
      sailed += len(nowG - prevG);
      prevG = nowG;
      if (g.sea.live.stuckT > 1.5f && stuckFor <= 0) { stuckFor = 90; escapes++; g.sea.live.stuckT = 0; }
      // the chunks ready ahead of the bow (the strip the next window shift brings in)
      if (((int)(simT / SIM_DT)) % 120 == 0 && g.world.streamer) {
        const float hc = std::cos(g.sea.live.heading), hs = std::sin(g.sea.live.heading);
        const int32_t ox = g.world.ox, oy = g.world.oy, W = World::WIN, SH = World::WIN_SHIFT, C = ew::CHUNK;
        int ready = 0, all = 0;
        if (std::fabs(hc) >= std::fabs(hs)) {
          const int32_t x0 = hc > 0 ? ox + W : ox - SH;
          for (int32_t x = x0; x < x0 + SH; x += C) for (int32_t y = oy; y < oy + W; y += C) { all++; ready += g.world.streamer->hasChunk(x / C - (x < 0 && x % C ? 1 : 0), y / C - (y < 0 && y % C ? 1 : 0)) ? 1 : 0; }
        } else {
          const int32_t y0 = hs > 0 ? oy + W : oy - SH;
          for (int32_t y = y0; y < y0 + SH; y += C) for (int32_t x = ox; x < ox + W; x += C) { all++; ready += g.world.streamer->hasChunk(x / C - (x < 0 && x % C ? 1 : 0), y / C - (y < 0 && y % C ? 1 : 0)) ? 1 : 0; }
        }
        aheadReady += ready; aheadAll += all; aheadMin = std::min(aheadMin, ready);
      }
      // ashore on continent 2: a landing whose land is the target's
      if (wp + 1 >= R.pts.size() || ((int)(simT / SIM_DT)) % 30 == 0) {
        Vec2 to;
        if (SeaOps::landing(g, to)) {
          const int32_t lx = g.world.ox + (int32_t)std::floor(to.x / TILE), ly = g.world.oy + (int32_t)std::floor(to.y / TILE);
          if (src.landAt(lx, ly) == T.id && SeaOps::disembark(g)) arrived = src.landAt(SeaOps::heroGx(g), SeaOps::heroGy(g)) == T.id;
        }
      }
    }
    const int sync = g.world.sstats.syncChunks - sync0;
    out("sail: seed %s: %s -> %s (land %u%s): %s in %.0f s, route %.0f tiles (%zu waypoints, plan %.0f ms); worst step %.1f ms (update %.1f, the sea's own %.1f), %d sync chunks, ahead of the bow %d/%d ready (worst %d)\n",
        S(seed), P.name.c_str(), T.name.c_str(), T.id, keel ? ", storm keel" : "", arrived ? "ARRIVED" : "NOT ARRIVED", simT, sailed, R.pts.size(), planMs, worstMs, worstUpd, g.sea.live.stepMsWorst, sync,
        aheadReady, aheadAll, aheadMin == (1 << 30) ? 0 : aheadMin);
    {   // where the worst update steps came from (the world's systems, by their worst ms)
      static const char* kSys[] = {"quest", "realm", "war", "story", "lifestep", "raid", "foe", "lifetick", "spawn", "home", "party"};
      std::string worst;
      for (int k = 0; k < Game::PerfCounters::S_COUNT && k < 11; k++)
        if (g.perf.sysWorstMs[k] > 8.0) worst += std::string(worst.empty() ? "" : ", ") + kSys[k] + " " + std::to_string((int)std::lround(g.perf.sysWorstMs[k])) + " ms";
      if (!worst.empty()) out("sail: seed %s: (systems over 8 ms on the voyage: %s)\n", S(seed), worst.c_str());
    }
    if (!arrived) SEA_FAIL("seed %s: the captain bot did not reach %s within an hour (%.0f tiles sailed, waypoint %zu of %zu)\n", S(seed), T.name.c_str(), sailed, wp, R.pts.size());
    // ---- streaming at sea: two minutes in a straight line across open water at sloop and galleon speed
    for (int h = 1; h <= 2; h++) {
      Game s2 = makeGame(seed);
      s2.streamThreads = false;
      s2.world.streamer.reset();
      if (!toMistEdge(s2, 900)) { out("sail: seed %s: SKIP the streaming leg (no open sea)\n", S(seed)); break; }
      sea::Boat& B = s2.sea.boats[(size_t)s2.sea.aboard];
      B.hull = (sea::Hull)h;
      B.upgrades |= sea::UP_KEEL | sea::UP_SAILS;
      s2.sea.live.autopilot = s2.sea.live.heading + kPi;   // (away from the land: the open ocean)
      const int s0 = s2.world.sstats.syncChunks, sh0 = s2.world.sstats.shifts;
      double worst = 0;
      float dist = 0;
      Vec2 pg((float)SeaOps::heroGx(s2), (float)SeaOps::heroGy(s2));
      for (int f = 0; f < 60 * 120 && s2.sea.atSea(); f++) {
        const auto c0 = clk::now();
        s2.update(SIM_DT, Input());
        s2.frameWork(3.0);
        worst = std::max(worst, std::chrono::duration<double, std::milli>(clk::now() - c0).count());
        s2.events.clear();
        s2.sea.boats[(size_t)s2.sea.aboard].hp = 9999;
        if (s2.sea.live.stuckT > 1.0f) { s2.sea.live.autopilot += 0.8f; s2.sea.live.stuckT = 0; }
        const Vec2 ng((float)SeaOps::heroGx(s2), (float)SeaOps::heroGy(s2));
        dist += len(ng - pg);
        pg = ng;
      }
      out("sail: seed %s: streaming at %s speed: %.0f tiles in 120 s, %d window shifts, %d sync chunks, worst step %.1f ms\n", S(seed),
          sea::hullName((sea::Hull)h), dist, s2.world.sstats.shifts - sh0, s2.world.sstats.syncChunks - s0, worst);
      if (worst > 100.0) SEA_FAIL("seed %s: a %.0f ms step streaming at %s speed\n", S(seed), worst, sea::hullName((sea::Hull)h));
    }
  }
  out("sail: %s (%d failures)\n", m_bad ? "FAIL" : "OK", m_bad);
  return m_bad ? 1 : 0;
}
}  // namespace

RPG_TEST_CMD("--sea", "M10: the sea: hulls, fish odds, the sea block, sailing / mooring / saving at sea, the Mist, isolation and first contact, storms, sinking, swimming, fishing, the chart chain, ferries [--seeds A..B]", seaCmd);
RPG_TEST_CMD("--sail", "M10: a captain bot sails a sloop from the nearest harbour to continent 2; streaming at sea at sloop and galleon speed [--seeds A..B]", sailCmd);
