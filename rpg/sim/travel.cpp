// M2 Wayfinder, SIM lane: journeys across the endless world behind the fade (game.h Travel; owner carry-over: the
// ~180 ms fast-travel / respawn hitch, worse on iPhone web).
//
//   beginTravel / respawn   the screen fades to black (kTravelFadeOut s) while the streamer gathers the destination
//                           window: its 8 x 8 chunks, the ring around it and the region plans placeWindow reads. Natively
//                           the worker makes them; on the web the per-frame pump does, with a larger budget once the
//                           screen is black (frameWork). Gather is capped at 2 s of real time (then the arrival makes
//                           what is missing itself).
//   Arrive                  one cheap step moves the window (ready chunks only: 0 generated on the main thread), places
//                           the player by fast travel's arrival rules and moves the clock by the journey's hours (day
//                           rollover, shop restock, lodging expiry follow from the clock). The screen stays black: the
//                           townsfolk stream in, the view bakes the terrain the arrival shows (View::travelArrive) and
//                           calls finishTravel(), which fades back in. Headless runs end Arrive by themselves after
//                           kTravelArriveHold s of updates.
//   fastTravel              stays immediate (scripts and tests).
// Rules (VISION_PLAN 2.11): discovered places on the same landmass (EndlessSource::landmass), 30 tiles an hour on
// foot; a carriage only from a town or city the player stands in, to a discovered settlement, 60 tiles an hour for
// 0.1 gold a tile (at least 5).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include "engine/audio.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/stream.h"
#include "rpg/world/source.h"

namespace {
double nowMs() {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
int32_t floorTo(int32_t v, int32_t step) { return ew::floorDiv(v, step) * step; }
constexpr float kTravelArriveHold = 0.5f;   // headless: Arrive ends by itself after this much update time
constexpr double kGatherCapMs = 2000.0;     // real time the gather may take before the arrival makes what is missing
constexpr float kFootTilesPerHour = 30.0f, kCarriageTilesPerHour = 60.0f;

// where the player stands on the overworld, as a GLOBAL tile (inside: the door or entrance they went in by)
void playerGlobal(const Game& g, int32_t& gx, int32_t& gy) {
  int x, y;
  if (g.inside && g.subBldg >= 0) { x = g.world.over.bldgs[(size_t)g.subBldg].doorX(); y = g.world.over.bldgs[(size_t)g.subBldg].doorY() + 1; }
  else if (g.inside && g.subSite >= 0) { x = g.world.sites[(size_t)g.subSite].ex; y = g.world.sites[(size_t)g.subSite].ey; }
  else { x = (int)std::floor(g.pl().p.x / TILE); y = (int)std::floor(g.pl().p.y / TILE); }
  gx = g.world.ox + x; gy = g.world.oy + y;
}

// (M2 fixer round 3) the arrival names the hours the way the map quoted them (worldmap.cpp hoursText), so a journey
// the map called "5 HOURS" does not come back as "half a day"
std::string hoursWords(float h) {
  if (h < 1.5f) return "AN HOUR";
  if (h < 36.0f) return std::to_string((int)std::lround(h)) + " HOURS";
  const int d = (int)std::lround(h / 24.0f);
  return d <= 1 ? std::string("A DAY") : std::to_string(d) + " DAYS";
}
}  // namespace

int32_t Game::landAt(int32_t gx, int32_t gy) const {
  if (!world.src) return 1;
  // a bridge, a ford or a quay may stand on water: the nearest land around it decides
  static const int off[][2] = {{0, 0}, {8, 0}, {-8, 0}, {0, 8}, {0, -8}, {16, 16}, {-16, 16}, {16, -16}, {-16, -16}, {32, 0}, {-32, 0}, {0, 32}, {0, -32}};
  for (auto& o : off)
    if (uint32_t m = world.src->landmass(gx + o[0], gy + o[1])) return (int32_t)m;
  return 0;
}

bool Game::carriageHere() const {
  int si = -1;
  if (inside) si = subBldg >= 0 ? world.over.bldgs[(size_t)subBldg].site : -1;
  else si = settlementAt(pl().p);
  if (si < 0 || si >= (int)world.sites.size()) return false;
  const SiteType t = world.sites[(size_t)si].type;
  return t == SiteType::Town || t == SiteType::City;
}

// the arrival tile of a journey to s (fast travel's rules), as a GLOBAL tile
void Game::travelDest(const Site& s, int kind, int32_t& gx, int32_t& gy) const {
  // waking after a death: a settlement's square (as fast travel; arriveInOpen then finds open ground), else the middle
  if (kind == 2 && !s.settlement()) { gx = world.ox + s.r.cx(); gy = world.oy + s.r.cy() + 1; return; }
  // (settlements: a few tiles south of the square's centrepiece, whose fountain blocks the tiles beside and above it,
  //  so the first step after arriving is onto open paving)
  int ty = s.ey + (s.type == SiteType::Cave ? 2 : (s.type == SiteType::Ruin ? 4 : s.settlement() ? 3 : 1));
  // arrive at the edge of hostile places, not in the bandit chief's lap or under the dragon
  if (s.type == SiteType::BanditCamp) ty = s.r.y + s.r.h + 4;
  if (s.type == SiteType::DragonLair) ty = s.ey + 5;
  if (s.type == SiteType::Vignette || s.type == SiteType::Wonder) ty = s.r.y + s.r.h + 1;   // beside the place, not on it
  gx = world.ox + s.ex; gy = world.oy + ty;
}

// (M1 round 3) arrive in the open: a settlement's square is full of stalls, crates and wells, and landing on the tile
// just north of one hid the hero under its awning with the way south blocked. Take the nearest tile with open ground
// around it and two clear tiles south of it (nothing standing in front of the hero, a free first step).
void Game::arriveInOpen(const Site& s) {
  if (!s.settlement()) return;
  const Map& m = map();
  const int px = (int)std::floor(pl().p.x / TILE), py = (int)std::floor(pl().p.y / TILE);
  auto open = [&](int x, int y) {
    for (int dy = -1; dy <= 2; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        if (m.blocked(x + dx, y + dy)) return false;
        if (dy >= 0 && m.propAt(x + dx, y + dy) > 0) return false;   // nothing standing in front (low clutter too)
      }
    // no house just south either: its roof would rise over the hero
    for (int dy = 3; dy <= 5; dy++)
      if (m.in(x, y + dy) && !m.bldgAt.empty() && m.bldgAt[(size_t)(y + dy) * m.w + x] >= 0) return false;
    return true;
  };
  for (int r = 0; r <= 10; r++)
    for (int oy = -r; oy <= r; oy++)
      for (int ox = -r; ox <= r; ox++) {
        if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
        if (open(px + ox, py + oy)) { pl().p = Vec2((px + ox) * TILE + 8.0f, (py + oy) * TILE + 10.0f); return; }
      }
}

TravelQuote Game::travelQuote(int si, bool carriage) const {
  TravelQuote q;
  if (si < 0 || si >= (int)world.sites.size() || !world.sites[(size_t)si].discovered) { q.why = "YOU HAVE NOT BEEN THERE"; return q; }
  const Site& s = world.sites[(size_t)si];
  int32_t px, py;
  playerGlobal(*this, px, py);
  const int32_t dx = world.ox + s.ex, dy = world.oy + s.ey;
  const float dist = std::hypot((float)(dx - px), (float)(dy - py));
  q.hours = dist / (carriage ? kCarriageTilesPerHour : kFootTilesPerHour);
  q.gold = carriage ? std::max(5, (int)std::lround(dist * 0.1f)) : 0;
  if (travelling()) { q.why = "YOU ARE ALREADY ON THE ROAD"; return q; }
  // (M3 integration) standing anywhere inside the place counts too: settlements grew, and a journey to the gate of
  // the village you stand in made no sense
  // (M3 fixer r3) indoors too: the building's own site (or the cave/ruin being explored) is "here"
  bool insideIt = false;
  if (!inside) insideIt = world.siteAt((int)(pl().p.x / TILE), (int)(pl().p.y / TILE)) == si;
  else if (subBldg >= 0 && subBldg < (int)world.over.bldgs.size()) {
    const Bldg& hb = world.over.bldgs[(size_t)subBldg];
    insideIt = hb.site == si || world.siteAt(hb.doorX(), hb.doorY() + 1) == si;
  } else if (subSite >= 0) insideIt = subSite == si;
  if (dist < 6.0f || insideIt) { q.why = "YOU ARE ALREADY THERE"; return q; }
  for (const Actor& a : actors)
    if (a.hostile && a.aggro && a.st != AState::Dead && len2(a.p - pl().p) < 120 * 120) { q.why = "YOU CANNOT TRAVEL WITH ENEMIES NEARBY"; return q; }
  // the landmass rule: over the sea it is ships (M10), not a walk
  const int32_t lp = landAt(px, py), ld = landAt(dx, dy);
  if (lp && ld && lp != ld) { q.why = "ACROSS THE SEA - YOU NEED A SHIP"; return q; }
  if (carriage) {
    if (!carriageHere()) { q.why = "CARRIAGES LEAVE ONLY FROM TOWNS AND CITIES"; return q; }
    if (!s.settlement()) { q.why = "THE CARRIAGE ONLY GOES TO TOWNS AND VILLAGES"; return q; }
    if (gold < q.gold) { q.why = "THE FARE IS " + std::to_string(q.gold) + " GOLD"; return q; }
  }
  q.ok = true;
  return q;
}

bool Game::beginTravel(int si, bool carriage) {
  const TravelQuote q = travelQuote(si, carriage);
  if (!q.ok) {
    say(q.why);
    return false;
  }
  return startJourney(si, carriage ? 1 : 0, q.hours, q.gold);
}

bool Game::startJourney(int si, int kind, float hours, int fare) {
  if (si < 0 || si >= (int)world.sites.size()) return false;
  const Site s = world.sites[(size_t)si];
  travel = Travel();
  travel.phase = TravelPhase::Gather;
  travel.site = si;
  travel.kind = (uint8_t)kind;
  travel.hours = hours;
  travel.gold = fare;
  travelDest(s, kind, travel.gx, travel.gy);
  if (fare > 0) {
    gold -= fare;
    sfx((int)Sfx::Coin, pl().p);
  }
  // the destination window: as placePlayerAt would recentre (a tile off the current window's middle moves it)
  const int lx = travel.gx - world.ox, ly = travel.gy - world.oy;
  const int lo = World::WIN_SHIFT, hi = World::WIN - World::WIN_SHIFT;
  travel.move = world.endless && (lx < lo || ly < lo || lx >= hi || ly >= hi);
  travel.nox = travel.move ? floorTo(travel.gx - World::WIN / 2, ew::CHUNK) : world.ox;
  travel.noy = travel.move ? floorTo(travel.gy - World::WIN / 2, ew::CHUNK) : world.oy;
  travel.wall0 = nowMs();
  travel.lastStepMs = travel.wall0;
  travel.gatherReal = 0;
  mode = Mode::Play;
  if (travel.move && !world.streamer) {
#ifdef __EMSCRIPTEN__
    world.streamer = std::make_shared<ChunkStreamer>(world.seed, world.src, false);
#else
    world.streamer = std::make_shared<ChunkStreamer>(world.seed, world.src, streamThreads);
#endif
  }
  travelWant();
  return true;
}

// the destination window's needs, most urgent first: the region plans placeWindow reads (loadRegionsAround), the 8 x 8
// chunks of the window nearest the arrival first, then the ring the first walking shifts will copy
void Game::travelWant() {
  if (!travel.move || !world.streamer) return;
  std::vector<ChunkStreamer::Key> keys;
  const int32_t nox = travel.nox, noy = travel.noy;
  const int32_t r0x = ew::regionOf(nox - ew::REGION), r1x = ew::regionOf(nox + World::WIN + ew::REGION - 1);
  const int32_t r0y = ew::regionOf(noy - ew::REGION), r1y = ew::regionOf(noy + World::WIN + ew::REGION - 1);
  const int32_t drx = ew::regionOf(travel.gx), dry = ew::regionOf(travel.gy);
  std::vector<std::pair<int, ChunkStreamer::Key>> rk;
  for (int32_t ry = r0y; ry <= r1y; ry++)
    for (int32_t rx = r0x; rx <= r1x; rx++) {
      ChunkStreamer::Key k; k.x = rx; k.y = ry; k.region = true;
      rk.push_back({std::max(std::abs(rx - drx), std::abs(ry - dry)), k});
    }
  std::stable_sort(rk.begin(), rk.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  for (auto& k : rk) keys.push_back(k.second);
  const int nc = World::WIN / ew::CHUNK;
  std::vector<std::pair<int64_t, ChunkStreamer::Key>> ck, ring;
  for (int cy = -1; cy <= nc; cy++)
    for (int cx = -1; cx <= nc; cx++) {
      const int32_t gcx = (nox >> ew::CHUNK_SHIFT) + cx, gcy = (noy >> ew::CHUNK_SHIFT) + cy;
      const int64_t ddx = (int64_t)gcx * ew::CHUNK + ew::CHUNK / 2 - travel.gx, ddy = (int64_t)gcy * ew::CHUNK + ew::CHUNK / 2 - travel.gy;
      ChunkStreamer::Key k; k.x = gcx; k.y = gcy;
      const bool inWin = cx >= 0 && cy >= 0 && cx < nc && cy < nc;
      if (inWin) {
        // a chunk the current window already holds is kept by placeWindow (not wanted)
        const int32_t gx = gcx * ew::CHUNK, gy = gcy * ew::CHUNK;
        const bool keep = std::abs(nox - world.ox) < World::WIN && std::abs(noy - world.oy) < World::WIN;
        if (keep && gx >= world.ox && gy >= world.oy && gx < world.ox + World::WIN && gy < world.oy + World::WIN) continue;
        ck.push_back({ddx * ddx + ddy * ddy, k});
      } else ring.push_back({ddx * ddx + ddy * ddy, k});
    }
  std::stable_sort(ck.begin(), ck.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  std::stable_sort(ring.begin(), ring.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  for (auto& k : ck) keys.push_back(k.second);
  for (auto& k : ring) keys.push_back(k.second);
  world.streamer->want(keys);
}

bool Game::travelReady() {
  if (!travel.move) return true;
  if (!world.streamer) return false;
  const int32_t nox = travel.nox, noy = travel.noy;
  const int32_t r0x = ew::regionOf(nox - ew::REGION), r1x = ew::regionOf(nox + World::WIN + ew::REGION - 1);
  const int32_t r0y = ew::regionOf(noy - ew::REGION), r1y = ew::regionOf(noy + World::WIN + ew::REGION - 1);
  for (int32_t ry = r0y; ry <= r1y; ry++)
    for (int32_t rx = r0x; rx <= r1x; rx++)
      if (!world.streamer->hasRegion(rx, ry)) return false;
  const int nc = World::WIN / ew::CHUNK;
  const bool keep = std::abs(nox - world.ox) < World::WIN && std::abs(noy - world.oy) < World::WIN;
  for (int cy = 0; cy < nc; cy++)
    for (int cx = 0; cx < nc; cx++) {
      const int32_t gcx = (nox >> ew::CHUNK_SHIFT) + cx, gcy = (noy >> ew::CHUNK_SHIFT) + cy;
      const int32_t gx = gcx * ew::CHUNK, gy = gcy * ew::CHUNK;
      if (keep && gx >= world.ox && gy >= world.oy && gx < world.ox + World::WIN && gy < world.oy + World::WIN) continue;
      if (!world.streamer->hasChunk(gcx, gcy)) return false;
    }
  return true;
}

void Game::travelStep(float dt) {
  const double t0 = nowMs();
  travel.steps++;
  travel.t += dt;
  travel.gatherReal += std::clamp(t0 - travel.lastStepMs, 0.0, 250.0);
  travel.lastStepMs = t0;
  if (travel.phase == TravelPhase::Gather) {
    // fade to black, then keep it black
    sleepFade = std::max(sleepFade, std::min(1.0f, travel.t / kTravelFadeOut));
    const bool black = travel.t >= kTravelFadeOut;
    // no frame loop pumping a thread-less streamer (a headless run with streamThreads off): pump it here
    if (world.streamer && !world.streamer->threaded() && frameWorkCalls_ == 0) world.streamer->pump(black ? kTravelBlackBudgetMs : 3.0);
    if (black && (travelReady() || travel.gatherReal > kGatherCapMs)) travelArriveNow();
  } else if (travel.phase == TravelPhase::Arrive) {
    sleepFade = std::max(sleepFade, 1.0f);
    // behind the black: the place's people stream in, the next ring starts streaming
    updateSpawning(dt);
    prefetchTick(dt);
    // headless runs end the arrival by themselves; with a view it is the view's call (its own 6 s safety net), and
    // this is only a backstop for a view that stopped drawing the arrival (a slow phone took longer than 0.5 s)
    if (travel.t >= (travel.viewed ? 10.0f : kTravelArriveHold)) finishTravel();
  }
  travel.worstStepMs = std::max(travel.worstStepMs, nowMs() - t0);
}

void Game::travelArriveNow() {
  const double t0 = nowMs();
  const Site s = world.sites[(size_t)travel.site];   // a copy: moving the window loads more sites
  int leftSite = -1;
  if (inside) {
    // out of the building or dungeon without stepping onto its doorstep first (the screen is black)
    leftSite = subSite;
    inside = false; subBldg = -1; subSite = -1; subFloor = 0;
    sub = Map();
  }
  clearNonPlayer();
  const int sync0 = world.sstats.syncChunks;
  if (travel.move) {
    const int32_t ox0 = world.ox, oy0 = world.oy;
    world.placeWindow(travel.nox, travel.noy);
    windowMoved(world.ox - ox0, world.oy - oy0);
    prefetchT_ = 0;
    prefetchFrom_ = Vec2();
  }
  travel.syncChunks = world.sstats.syncChunks - sync0;
  pl().p = freeSpot(travel.gx - world.ox, travel.gy - world.oy);
  pl().vel = Vec2(); pl().knock = Vec2();
  pl().face = 0; pl().aim = Vec2(0, 1);
  pl().st = AState::Idle; pl().stT = 0;
  arriveInOpen(s);   // (respawns too: waking north of a house hid the hero under its roof)
  // a missing person found in the cave left with the player: led out, they are safe (as leaveSub does). Sites are
  // never removed, but a window move re-indexes nothing, so the index still names the cave.
  if (leftSite >= 0 && leftSite < (int)world.sites.size() && travel.kind != 2) questLeftSite(leftSite);
  // the journey's hours: the day rolls over, shops restock (every 2 days), a rented room runs out
  if (travel.hours > 0) {
    hour += travel.hours;
    while (hour >= 24) { hour -= 24; day++; }
    if (blessT > 0) {   // a blessing wears off on the road (a game hour is 1800/24 = 75 s on average)
      blessT = std::max(0.0f, blessT - travel.hours * 75.0f);
      if (blessT <= 0) recalcPlayer();
    }
  }
  exitArmed_ = false;
  updateLocation();
  emit(Ev::MapChange, pl().p);
  travel.phase = TravelPhase::Arrive;
  travel.t = 0;
  const double now = nowMs();
  travel.arriveMs = now - t0;
  travel.gatherMs = now - travel.wall0;
}

void Game::finishTravel() {
  if (travel.phase != TravelPhase::Arrive) return;   // (the view only calls it once the arrival is placed)
  const Travel done = travel;
  lastTravel = done;
  lastTravel.phase = TravelPhase::None;
  travel = Travel();
  sleepFade = 1.0f;   // fade back in
  const std::string where = done.site >= 0 && done.site < (int)world.sites.size() ? world.sites[(size_t)done.site].name : std::string();
  if (done.kind == 1) say("THE CARRIAGE SETS YOU DOWN IN " + where + " AFTER " + hoursWords(done.hours) + " ON THE ROAD.");
  else if (done.kind == 0 && done.hours >= 1.0f) say("YOU REACH " + where + " AFTER " + hoursWords(done.hours) + " ON THE ROAD.");
}

// immediate fast travel (scripts and tests; the UI journeys behind the fade with beginTravel)
bool Game::fastTravel(int si) {
  if (si < 0 || si >= (int)world.sites.size() || !world.sites[si].discovered) return false;
  for (const Actor& a : actors) if (a.hostile && a.aggro && a.st != AState::Dead && len2(a.p - pl().p) < 120 * 120) { say("YOU CANNOT TRAVEL WITH ENEMIES NEARBY"); return false; }
  if (inside) leaveSub();
  travel = Travel();
  const Site s = world.sites[si];   // a copy: placing the player may move the endless window
  float dist = std::hypot(s.ex * 16.0f - pl().p.x, s.ey * 16.0f - pl().p.y) / 16.0f;
  hour += dist / kFootTilesPerHour;
  while (hour >= 24) { hour -= 24; day++; }
  clearNonPlayer();
  int32_t gx, gy;
  travelDest(s, 0, gx, gy);
  placePlayerAt(gx - world.ox, gy - world.oy);
  arriveInOpen(s);
  sleepFade = 1.2f;
  mode = Mode::Play;
  updateLocation();
  emit(Ev::MapChange, pl().p);
  return true;
}

// waking in the last town after a death: the same journey behind the fade (kind 2), with nothing to quote or pay
void Game::respawn() {
  const int si = lastTown >= 0 && lastTown < (int)world.sites.size() ? lastTown : world.startSite;
  Actor& p = pl();
  p.st = AState::Idle; p.stT = 0; p.hp = p.maxHp; p.burnT = 0; p.slowT = 0;
  p.knock = Vec2(); p.vel = Vec2(); p.iframes = 1.5f;   // a moment of grace after waking
  mp = maxMp; stamina = maxSt;
  int lost = gold / 10;
  gold -= lost;
  const std::string name = world.sites[(size_t)si].name;
  startJourney(si, 2, 0.0f, 0);
  travel.t = kTravelFadeOut;   // the death screen is already dark: no fade out
  sleepFade = 1.0f;
  say(lost > 0 ? "YOU WAKE IN " + name + ". LOST " + std::to_string(lost) + " GOLD." : "YOU WAKE IN " + name + ".");
}
