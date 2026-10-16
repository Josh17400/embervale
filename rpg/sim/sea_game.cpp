// M10 "Beyond the Mist": the sea's Game hooks (rpg/sim/sea_ops.h). SEA lane.
//
// The core: boats given / boarded / sailed / moored / left, the hull's waters (a rowboat keeps to the shallows, a sloop
// without a keel to the near seas, a galleon goes anywhere deep), the wind (points of sail: tacking upwind, running
// before it), momentum (a galleon gathers way slowly and carries it), a rudder that eases over (no snapping heading),
// a wake behind a boat under way, fighting from the deck (the bow, spells, a blade over the rail), the Mist's turn-back
// after MIST_TURN_SECS without the land's chart (and its parting with it), the boat aboard saved at the hero's tile.
// The rest lives in the SEA lane's other files: sea_sail.cpp (storms, sinking, swimming), sea_fish.cpp (fishing),
// sea_trade.cpp (harbours, the yard, ferries, the Sailor), sea_chain.cpp (the chart chain, first contact, isolation),
// sea_foes.cpp (the serpent, the kraken, merrow, pirates).
#include "rpg/sim/sea_ops.h"

#include "engine/audio.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/world/source.h"

using namespace sea;
using gsim::faceOf;

namespace {
constexpr float kPi = 3.14159265f;
Vec2 boatPx(const Game& g, const Boat& b) { return Vec2((float)(b.gx - g.world.ox) * TILE + 8.0f, (float)(b.gy - g.world.oy) * TILE + 8.0f); }
float wrapPi(float a) {
  while (a > kPi) a -= 2 * kPi;
  while (a < -kPi) a += 2 * kPi;
  return a;
}
// the room a hull needs round its tile (tiles each way that must be sailable too)
int roomOf(Hull h) { return h == Hull::Galleon ? 2 : h == Hull::Sloop ? 1 : 0; }
// a hull's waters on a window tile, with or without a storm keel (giving a boat ignores the storm rule: scripts, tests)
bool sailableAs(const Game& g, Hull h, bool storm, int tx, int ty) {
  if (g.inside) return false;
  const Map& m = g.world.over;
  if (!m.in(tx, ty)) return false;
  const Ground gr = m.at(tx, ty);
  if (!groundWater(gr) || m.solid[(size_t)ty * m.w + tx]) return false;
  const HullInfo& H = hullInfo(h);
  if (gr == Ground::Water) return (H.waters & W_SHALLOW) != 0;
  if (!(H.waters & (W_DEEP | W_STORM))) return false;
  if (!storm && !(H.waters & W_STORM) && SeaOps::stormSea(g, tx, ty)) return false;
  return true;
}
}  // namespace

bool SeaOps::landing(const Game& g, Vec2& out) {
  const Map& m = g.map();
  const Actor& p = g.pl();
  const int tx0 = (int)std::floor(p.p.x / TILE), ty0 = (int)std::floor(p.p.y / TILE);
  // a big hull reaches a little farther (its rail is wider than a rowboat's)
  const int reach = g.sea.atSea() && g.sea.boats[(size_t)g.sea.aboard].hull == Hull::Galleon ? 3 : 2;
  float best = 1e9f;
  bool found = false;
  for (int dy = -reach; dy <= reach; dy++)
    for (int dx = -reach; dx <= reach; dx++) {
      const int tx = tx0 + dx, ty = ty0 + dy;
      if (!m.in(tx, ty) || groundWater(m.at(tx, ty)) || m.at(tx, ty) == Ground::Void) continue;
      const Vec2 c((float)tx * TILE + 8.0f, (float)ty * TILE + 10.0f);
      if (!g.bodyFree(c, 4.5f, false)) continue;
      const float d = len2(c - p.p);
      if (d < best) { best = d; out = c; found = true; }
    }
  return found;
}

int32_t SeaOps::heroGx(const Game& g) { return g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE); }
int32_t SeaOps::heroGy(const Game& g) { return g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE); }

bool SeaOps::sailable(const Game& g, Hull h, int tx, int ty) { return sailableAs(g, h, false, tx, ty); }

const Boat* SeaOps::aboardBoat(const Game& g) { return g.sea.atSea() ? &g.sea.boats[(size_t)g.sea.aboard] : nullptr; }

int SeaOps::giveBoat(Game& g, Hull h, int radius, const std::string& name, const ew::GTile* at) {
  if (g.inside) return -1;
  const Actor& p = g.pl();
  const int tx0 = at ? at->x - g.world.ox : (int)std::floor(p.p.x / TILE), ty0 = at ? at->y - g.world.oy : (int)std::floor(p.p.y / TILE);
  const int room = roomOf(h);
  int bx = 0, by = 0;
  long best = -1;
  for (int dy = -radius; dy <= radius; dy++)
    for (int dx = -radius; dx <= radius; dx++) {
      const int tx = tx0 + dx, ty = ty0 + dy;
      bool ok = true;
      for (int ry = -room; ry <= room && ok; ry++)
        for (int rx = -room; rx <= room && ok; rx++) ok = sailableAs(g, h, true, tx + rx, ty + ry);
      if (!ok) continue;
      const long d = (long)dx * dx + (long)dy * dy;
      if (best < 0 || d < best) { best = d; bx = tx; by = ty; }
    }
  if (best < 0) return -1;
  Boat b;
  b.id = g.sea.nextBoatId++;
  b.hull = h;
  static const char* kNames[] = {"THE GULL'S LUCK", "THE SALT WREN", "THE GREY HERON", "THE BRAVE OTTER", "THE KELPIE", "THE DAWN TIDE",
                                 "THE STORM PETREL", "THE LANTERN FISH", "THE OLD ANCHOR", "THE NORTH STAR"};
  b.name = name.empty() ? std::string(kNames[hash32(b.id * 2654435761u ^ (uint32_t)g.seed) % 10]) : name;
  b.gx = g.world.ox + bx; b.gy = g.world.oy + by;
  b.hp = hullInfo(h).hull;
  if (g.world.src && g.world.endless) b.culture = g.world.src->cultureAt(b.gx, b.gy);
  g.sea.boats.push_back(b);
  g.sea.live.serial++;
  return (int)g.sea.boats.size() - 1;
}

int SeaOps::boatNear(const Game& g, float px) {
  if (g.inside) return -1;
  int best = -1;
  float bd = px * px;
  for (int i = 0; i < (int)g.sea.boats.size(); i++) {
    const Boat& b = g.sea.boats[(size_t)i];
    if (b.sunk || b.ferry || i == g.sea.aboard) continue;
    const float d = len2(boatPx(g, b) - g.pl().p);
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

bool SeaOps::board(Game& g, int i) {
  if (i < 0 || i >= (int)g.sea.boats.size() || i == g.sea.aboard || g.inside) return false;
  if (g.sea.boats[(size_t)i].sunk) return false;
  if (g.homeRiding()) g.homeDismount(home::Dismount::Water);
  if (g.sea.atSea()) {   // (from one boat to another: the first stays where it lies)
    Boat& o = g.sea.boats[(size_t)g.sea.aboard];
    o.gx = heroGx(g); o.gy = heroGy(g); o.heading = g.sea.live.heading;
    g.sea.aboard = -1;
  }
  Boat& b = g.sea.boats[(size_t)i];
  const HullInfo& H = hullInfo(b.hull);
  const Vec2 at = boatPx(g, b);
  if (!g.bodyFree(at, H.radius, false, 1)) return false;
  Actor& p = g.pl();
  Sea::Live& L = g.sea.live;
  if (!L.swimming) L.heroRadius = p.radius;
  L.line = Sea::Line();
  L.swimming = false; L.pulled = false; L.swimT = 0;
  g.sea.aboard = i;
  p.p = at; p.vel = Vec2(); p.knock = Vec2();
  p.swim = 1;
  p.radius = H.radius;
  p.st = AState::Idle; p.stT = 0;
  b.dock = 0;
  L.heading = b.heading;
  L.speed = 0;
  L.helm = 0;
  L.mooring = false;
  L.tack = 0;
  L.stuckT = 0;
  L.serial++;
  ev(g, SeaEv::Board, at, 1, b.id, "ABOARD " + b.name);
  g.sfx((int)Sfx::Splash, at, 1.3f, 0.5f);
  if (!b.ferry) g.say("ABOARD " + b.name);
  return true;
}

bool SeaOps::disembark(Game& g) {
  if (!g.sea.atSea()) return false;
  Vec2 to;
  if (!landing(g, to)) return false;
  Boat& b = g.sea.boats[(size_t)g.sea.aboard];
  Actor& p = g.pl();
  Sea::Live& L = g.sea.live;
  b.gx = heroGx(g); b.gy = heroGy(g);
  b.heading = L.heading;
  // left by a port's berth: moored at that port (2.12: "boats are moored at the dock they were left at")
  b.dock = (L.nearPort && L.nearDist < 3.5f * TILE && !b.ferry) ? L.nearPort : 0;
  g.sea.aboard = -1;
  p.swim = 0;
  p.radius = 4.5f;
  ev(g, SeaEv::Disembark, to, 1, b.id);
  p.p = to; p.vel = Vec2();
  L.speed = 0;
  L.mooring = false;
  L.line = Sea::Line();
  L.serial++;
  g.recalcPlayer();
  if (b.dock && !b.ferry) g.say(b.name + " IS MOORED AT " + L.nearName);
  return true;
}

// ---------------------------------------------------------------- steering
float SeaOps::autopilotHeading(Game& g, float want) {
  Sea::Live& L = g.sea.live;
  const Boat* b = aboardBoat(g);
  if (!b || hullInfo(b->hull).windGain <= 0 || L.wind.speed < 0.12f) { L.tack = 0; return want; }
  const float from = L.wind.dir + kPi;   // where the wind blows FROM
  const float off = wrapPi(want - from);
  constexpr float kBeat = 0.80f;          // ~46 degrees: inside it a sail luffs, so the course is beaten to windward
  if (std::fabs(off) >= kBeat) { L.tack = 0; return want; }
  if (L.tack == 0) { L.tack = off >= 0 ? 1.0f : -1.0f; L.tackT = 9.0f + 6.0f * g.rng_.f(); }
  return wrapPi(from + L.tack * 0.90f);   // ~52 degrees off: close-hauled, the best way to windward
}

bool SeaOps::moorStart(Game& g) {
  Sea::Live& L = g.sea.live;
  const Boat* b = aboardBoat(g);
  if (!b || b->ferry || !L.nearPort || L.nearDist > 6.0f * TILE) return false;
  const int tx = (int)std::floor(L.nearBerth.x / TILE), ty = (int)std::floor(L.nearBerth.y / TILE);
  if (!sailableAs(g, b->hull, true, tx, ty) && len(L.nearBerth - g.pl().p) > 1.5f * TILE) return false;
  L.mooring = true;
  L.moorPier = false;
  L.stuckT = 0;
  L.moorTo = L.nearBerth;
  L.moorDock = L.nearPort;
  L.moorT = 0;
  L.autopilot = -100;
  ev(g, SeaEv::Moor, L.moorTo, 1, b->id);
  g.say("BRINGING HER INTO THE BERTH");
  return true;
}

void SeaOps::sailStep(Game& g, const Input& in, float dt) {
  Boat& b = g.sea.boats[(size_t)g.sea.aboard];
  const HullInfo& H = hullInfo(b.hull);
  Sea::Live& L = g.sea.live;
  Actor& p = g.pl();
  const float topSpeed = boatSpeed(b);
  const Vec2 windV(std::cos(L.wind.dir), std::sin(L.wind.dir));
  // ---- the glide into a berth: the way comes off as she nears it, the bow swings to the berth, no snap at the end
  if (L.mooring) {
    const Vec2 d = L.moorTo - p.p;
    const float dist = len(d);
    L.moorT += dt;
    const float want = std::min(topSpeed * 0.4f, dist / TILE * 0.9f + 0.15f);
    L.speed += std::clamp(want - L.speed, -2.5f * dt, 1.4f * dt);
    if (dist > 1.0f) {
      const float diff = wrapPi(std::atan2(d.y, d.x) - L.heading);
      L.helm += std::clamp(std::clamp(diff * 1.5f, -1.0f, 1.0f) - L.helm, -3.0f * dt, 3.0f * dt);
      L.heading = wrapPi(L.heading + std::clamp(diff, -H.turn * 1.2f * dt, H.turn * 1.2f * dt));
      const Vec2 before = p.p;
      g.moveActor(p, norm(d) * std::min(dist, L.speed * TILE * dt));
      if (len2(p.p - before) < 0.0001f) L.moorT += dt * 2;   // (fouled on something: give up sooner)
      else L.stuckT = 0;
      if (len2(p.p - before) < 0.0001f && L.speed > 0.3f) L.stuckT += dt;
    }
    b.heading = L.heading;
    p.face = faceOf(Vec2(std::cos(L.heading), std::sin(L.heading)));
    p.st = L.speed > 0.3f ? AState::Walk : AState::Idle;
    // at the berth with no step ashore in reach (a berth out in the harbour): she glides on in to the pier's foot
    if ((dist < 2.5f || L.stuckT > 0.6f) && !L.moorPier && L.moorT < 9.0f) {
      Vec2 to;
      if (!landing(g, to) && len(L.nearPier - L.moorTo) > TILE) { L.moorPier = true; L.moorTo = L.nearPier; L.stuckT = 0; return; }
    }
    if (dist < 2.5f || L.moorT > 9.0f || L.stuckT > 0.6f) {
      L.moorPier = false;
      L.stuckT = 0;
      L.mooring = false;
      L.speed = 0;
      const ew::Gid dock = L.moorDock;
      const std::string name = L.nearName;
      if (!disembark(g)) { g.say("MOORED, BUT THERE IS NO WAY ASHORE HERE"); return; }
      if (dock && g.sea.aboard < 0) {   // (disembark judged the dock by the hero's distance: the glide's berth wins)
        for (Boat& o : g.sea.boats) if (o.id == b.id) o.dock = dock;
      }
      (void)name;
    }
    return;
  }
  const bool line = L.line.phase != Sea::Cast::None;
  Vec2 mv = (line || b.ferry) ? Vec2() : in.move;   // (a ferry's passenger has no say at the helm)
  float ml = len(mv);
  if (ml > 1) { mv = mv * (1.0f / ml); ml = 1; }
  // a held course (scripts, tests, a ferry's captain, the --sail bot) while the stick is idle: beats to windward
  if (ml <= 0.15f && L.autopilot > -50 && !line) {
    if (L.tack != 0 && (L.tackT -= dt) <= 0) { L.tack = -L.tack; L.tackT = 10.0f + 8.0f * g.rng_.f(); }
    const float h = autopilotHeading(g, L.autopilot);
    mv = Vec2(std::cos(h), std::sin(h));
    ml = 1;
  } else if (ml > 0.15f) L.tack = 0;
  // the Mist turning the boat round: the helm is the sea's until the fog thins
  if (L.turningBack) { mv = Vec2(std::cos(L.heading), std::sin(L.heading)); ml = 1; }
  float throttle = 0;
  float diff = 0;
  if (ml > 0.15f) {
    diff = wrapPi(std::atan2(mv.y, mv.x) - L.heading);
    throttle = ml * std::max(0.2f, std::cos(diff));   // (a hard turn sheds way)
  }
  // the rudder eases over and back (never a snap): the heading turns by the rudder, faster with way on (oars pivot)
  if (L.clearT > 0) { L.clearT -= dt; if (ml > 0.15f) { diff = 0; throttle = ml; } }   // (fixer M10 r1) standing off a pier first
  const float rudderWant = ml > 0.15f ? std::clamp(diff * 1.6f, -1.0f, 1.0f) : 0.0f;
  L.helm += std::clamp(rudderWant - L.helm, -3.2f * dt, 3.2f * dt);
  const float way = std::clamp(L.speed / std::max(0.5f, topSpeed * 0.5f), 0.0f, 1.0f);
  const float bite = H.windGain <= 0 ? 0.7f + 0.3f * way : 0.3f + 0.7f * way;
  float turn = L.helm * H.turn * bite * dt;
  if (ml > 0.15f && std::fabs(turn) > std::fabs(diff)) turn = diff;   // (never past the wished course)
  L.heading = wrapPi(L.heading + turn);
  const float target = topSpeed * windFactor(b.hull, L.heading, L.wind) * throttle * (1.0f - 0.35f * L.rough);
  const float accel = (b.hull == Hull::Galleon ? 1.0f : b.hull == Hull::Sloop ? 1.8f : 3.2f) * dt;
  const float decel = (b.hull == Hull::Galleon ? 0.55f : b.hull == Hull::Sloop ? 0.9f : 2.4f) * dt;   // she carries her way
  L.speed += std::clamp(target - L.speed, -decel, accel);
  if (L.speed < 0) L.speed = 0;
  const Vec2 dir(std::cos(L.heading), std::sin(L.heading));
  const bool keel = sailsStorm(b);
  // the bow's water: a hull keeps to the waters it can sail (a rowboat runs aground at the deep water's edge)
  const Vec2 bow = p.p + dir * (H.radius + 3.0f);
  const bool bowFoul = !sailableAs(g, b.hull, keel, (int)std::floor(bow.x / TILE), (int)std::floor(bow.y / TILE));
  if (L.speed > 0.05f && bowFoul) {
    if (L.speed > 2.5f) {
      ev(g, SeaEv::HullHit, bow, L.speed * 0.5f, b.id);
      g.sfx((int)Sfx::HitHeavy, bow, 0.6f, 0.5f);
      if (L.speed > 4.0f) hurtBoat(g, (L.speed - 4.0f) * 2.0f, bow);   // (running hard aground hurts the hull)
    }
    L.speed = 0;
    if (throttle > 0.3f) L.stuckT += dt;
  } else L.stuckT = std::max(0.0f, L.stuckT - dt);
  const Vec2 before = p.p;
  if (L.speed > 0.01f) g.moveActor(p, dir * (L.speed * TILE * dt));
  // (fixer M10 r1) pinned (the bow on a pier or a bank, or the hull pressed against one) with the helm asking for way:
  // she pivots off toward the open water, a little faster than the rudder pulls her back, so she slides clear along
  // the obstacle instead of sitting at 0.0 knots (seeds 3 and 7: a sloop given at the berth, the helm held out)
  const bool pinned = throttle > 0.3f && (bowFoul || (L.speed > 0.5f && len2(p.p - before) < 0.0001f));
  if (pinned) {
    int off = 0;   // the side (1 starboard, -1 port) whose water opens at the smallest swing
    for (int k = 1; k <= 4 && !off; k++)
      for (int side = 1; side >= -1 && !off; side -= 2) {
        const float hs = L.heading + side * 0.5f * (float)k;
        const Vec2 pr = p.p + Vec2(std::cos(hs), std::sin(hs)) * (H.radius + 3.0f);
        if (sailableAs(g, b.hull, keel, (int)std::floor(pr.x / TILE), (int)std::floor(pr.y / TILE))) off = side;
      }
    if (off) {
      L.heading = wrapPi(L.heading + (float)off * H.turn * 0.9f * dt);
      L.speed = std::max(L.speed, 0.6f);   // (steerage way: the oars, a push off the pier)
      L.clearT = 2.5f;                      // then a few seconds on the clear heading before the helm takes her round
      L.helm = 0;
    }
  }
  // the wind's push (and a storm's): a sail drifts downwind, more with the sails idle; only onto water she can sail
  if (H.windGain > 0 || L.drift > 0) {
    const float push = (H.windGain > 0 ? L.wind.speed * 0.3f * (1.0f - 0.7f * throttle) : 0.0f) + L.drift;
    if (push > 0.01f) {
      const Vec2 dp = windV * (push * TILE * dt);
      const Vec2 to = p.p + dp + windV * H.radius;
      if (sailableAs(g, b.hull, keel, (int)std::floor(to.x / TILE), (int)std::floor(to.y / TILE))) g.moveActor(p, dp);
    }
  }
  // the hull heels to a wind on the beam (the view tilts the sail), eased
  const float heelWant = H.windGain > 0 ? std::sin(L.heading - L.wind.dir) * L.wind.speed * std::min(1.0f, L.speed / std::max(1.0f, topSpeed * 0.6f)) : 0.0f;
  L.heel += (heelWant - L.heel) * std::min(1.0f, dt * 2.0f);
  // a wake behind a boat under way: puffs at the stern, quicker the faster she goes, never in lockstep
  const float moved = len(p.p - before) / std::max(dt, 1e-4f) / TILE;
  if ((L.wakeT -= dt) <= 0 && moved > 0.6f) {
    const Vec2 stern = p.p - dir * (H.radius + 2.0f);
    ev(g, SeaEv::Wake, stern, moved, b.id, "", bow);
    L.wakeT = std::clamp(0.34f - moved * 0.022f, 0.08f, 0.34f) * (0.75f + 0.5f * g.rng_.f());
  }
  p.face = faceOf(dir);
  p.aim = dir;
  p.st = L.speed > 0.3f ? AState::Walk : AState::Idle;
  b.heading = L.heading;
}

// ---------------------------------------------------------------- the hooks
bool SeaOps::input(Game& g, const Input& in, float dt) {
  if (g.inside) return false;
  Sea::Live& L = g.sea.live;
  if (L.sinkT > 0) return true;   // going down: no helm, no swim (sinkStep runs the fade)
  if (!g.sea.atSea()) {
    if (fishInput(g, in, dt)) return true;
    return swimInput(g, in, dt);
  }
  Actor& p = g.pl();
  if (in.interact && !L.mooring && L.line.phase == Sea::Cast::None) { g.interact(); if (!g.sea.atSea()) return true; }
  const bool lineOut = fishInput(g, in, dt);
  sailStep(g, in, dt);
  if (lineOut) return true;
  // fighting from the deck: arrows at whatever is near, a spell, a blade over the rail at whatever comes alongside
  const Boat& b = g.sea.boats[(size_t)g.sea.aboard];
  const float reach = hullInfo(b.hull).radius + 24.0f;
  int near = -1;
  float nd = 190.0f * 190.0f;
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& e = g.actors[i];
    if (!e.hostile || e.st == AState::Dead || e.fly) continue;
    const float d2 = len2(e.p - p.p);
    if (d2 < nd) { nd = d2; near = (int)i; }
  }
  if (in.bow) {
    const Vec2 keep = p.aim;
    if (near >= 0) p.aim = norm(g.actors[(size_t)near].p - p.p);
    g.shootArrow();
    p.aim = keep;
    return true;
  }
  if (in.attack && !in.interact && p.atkCd <= 0) {
    if (near >= 0 && nd < reach * reach) p.aim = norm(g.actors[(size_t)near].p - p.p);
    p.combo = 0;
    g.sfx((int)Sfx::Swing, p.p, 1.0f, 0.8f);
    g.meleeHit(p);
    p.atkCd = 0.45f;
    g.stamina = std::max(0.0f, g.stamina - 4.0f);
    return true;
  }
  MagicOps::input(g, in, true);
  return true;
}

bool SeaOps::passable(const Game& g, const Actor& a, int tx, int ty) {
  // the hero wades and swims in the shallows (Ground::Water; deep water is a boat's), never under a prop or a wall
  if (!a.player || g.inside || g.sea.atSea() || g.sea.live.sinkT > 0) return false;
  const Map& m = g.world.over;
  if (!m.in(tx, ty) || m.at(tx, ty) != Ground::Water) return false;
  if (m.propAt(tx, ty) != 0 || m.bldgAt[(size_t)ty * m.w + tx] >= 0 || m.wall[(size_t)ty * m.w + tx]) return false;
  return true;
}

void SeaOps::step(Game& g, float dt) {
  Sea::Live& L = g.sea.live;
  struct Clock {   // (the step's cost: Live::stepMsWorst)
    Sea::Live& l;
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    ~Clock() { l.stepMsWorst = std::max(l.stepMsWorst, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
  } clock{L};
  L.swell += dt;
  if (L.fishCd > 0) L.fishCd -= dt;
  if (g.inside) { chainStep(g, dt); return; }   // (a sea cave's depths: the sunken chart)
  if (!g.world.endless || !g.world.src) return;
  const int32_t gx = heroGx(g), gy = heroGy(g);
  L.wind = windAt(g.seed, (float)gx, (float)gy, (double)g.day * 24.0 + g.hour);
  if ((L.mistProbeT -= dt) <= 0) {
    L.mistProbeT = 0.25f;
    L.mist = (float)g.world.src->mistAt(gx, gy) / 65536.0f;
    L.mistLand = L.mist > 0 ? g.world.src->hiddenLandAt(gx, gy) : 0;
  }
  // a hero aboard who is no longer on the water (a respawn, a fast travel, a teleport moved the body ashore): the boat
  // stays where it was last afloat and the hero walks again
  if (g.sea.atSea() && !g.travelling() && L.sinkT <= 0) {
    const Map& m = g.world.over;
    const int tx = (int)std::floor(g.pl().p.x / TILE), ty = (int)std::floor(g.pl().p.y / TILE);
    if (m.in(tx, ty) && !groundWater(m.at(tx, ty)) && m.at(tx, ty) != Ground::Void) {   // (Void: not streamed yet)
      Boat& b = g.sea.boats[(size_t)g.sea.aboard];
      const bool ferry = b.ferry;
      g.sea.aboard = -1;
      g.pl().swim = 0;
      g.pl().radius = 4.5f;
      L.speed = 0;
      L.mooring = false;
      if (ferry) { g.sea.boats.erase(g.sea.boats.begin() + (std::ptrdiff_t)(&b - &g.sea.boats[0])); L.voyage = Sea::Voyage(); }
      L.serial++;
    }
  }
  sinkStep(g, dt);
  stormStep(g, dt);
  tradeStep(g, dt);
  voyageStep(g, dt);
  chainStep(g, dt);
  encounterStep(g, dt);
  if (!g.sea.atSea()) { L.mistT = 0; L.turningBack = false; L.mistTold = false; return; }
  const Vec2 at = g.pl().p;
  if (L.mist > 0.05f && L.mistLand) {
    const bool chart = g.sea.hasChart(L.mistLand);
    if (!L.mistTold) {
      L.mistTold = true;
      if (chart) { ev(g, SeaEv::MistPart, at, 1, L.mistLand, "THE CHART SHOWS THE WAY: THE MIST PARTS"); g.say("THE CHART SHOWS THE WAY: THE MIST PARTS"); }
      else { ev(g, SeaEv::MistEnter, at, 1, L.mistLand, "A COLD FOG CLOSES ROUND THE BOAT"); g.say("A COLD FOG CLOSES ROUND THE BOAT"); }
    }
    if (!chart && !L.turningBack) {
      L.mistT += dt;
      if (L.mistT >= MIST_TURN_SECS) {
        // turned round: away from the hidden land's heart, out of the Mist
        const ew::LandPlan land = g.world.src->land(L.mistLand);
        const float ax = (float)(gx - land.cx), ay = (float)(gy - land.cy);
        L.heading = std::atan2(ay, ax);
        L.turningBack = true;
        L.autopilot = -100;
        L.mooring = false;
        ev(g, SeaEv::MistTurn, at, 1, L.mistLand, "THE MIST TURNS YOU BACK. YOU NEED A CHART OF THESE WATERS.");
        g.say("THE MIST TURNS YOU BACK. YOU NEED A CHART OF THESE WATERS.");
      }
    }
  } else {
    L.mistT = 0;
    L.turningBack = false;
    L.mistTold = false;
  }
}

bool SeaOps::interact(Game& g) {
  if (g.inside) return false;
  Sea::Live& L = g.sea.live;
  if (L.line.phase != Sea::Cast::None || L.sinkT > 0) return true;
  if (g.sea.atSea()) {
    const Boat& b = g.sea.boats[(size_t)g.sea.aboard];
    if (b.ferry) { g.say(L.voyage.on ? "THE CAPTAIN WON'T PUT YOU ASHORE MID-PASSAGE" : "WAIT FOR THE GANGPLANK"); return true; }
    if (L.mooring) return true;
    if (moorStart(g)) return true;
    if (disembark(g)) return true;
    if (castLine(g)) return true;
    g.say("NO LANDING HERE");
    return true;
  }
  if (L.swimming) return false;
  const int b = boatNear(g);
  if (b >= 0 && board(g, b)) return true;
  int tx = 0, ty = 0;
  if (g.interactProp(tx, ty)) return false;   // (a chest, a bed, a shrine by the water: the prop's use wins)
  return castLine(g);
}

std::string SeaOps::interactLabel(const Game& g) {
  if (g.inside) return "";
  const Sea::Live& L = g.sea.live;
  if (L.sinkT > 0) return "";
  switch (L.line.phase) {
    case Sea::Cast::Casting: case Sea::Cast::Waiting: return "REEL IN";
    case Sea::Cast::Bite: return "STRIKE!";
    case Sea::Cast::Reeling: return "HOLD: REEL";   // (fixer M10 r1) the reel works while held (the HUD reads HOLD as its own verb)
    case Sea::Cast::Landed: case Sea::Cast::Lost: return "";
    default: break;
  }
  Vec2 bob;
  uint8_t w = 0;
  if (g.sea.atSea()) {
    const Boat& b = g.sea.boats[(size_t)g.sea.aboard];
    if (b.ferry || L.mooring) return "";
    // (fixer M10 r1) foes close by: the attack key/button fights (an empty verb keeps the HUD from turning it into use)
    const bool fight = foesNear(g, 6.0f * TILE) > 0;
    if (L.nearPort && L.nearDist < 6.0f * TILE) return fight ? "" : "MOOR";
    Vec2 to;
    if (landing(g, to)) return fight ? "" : "DISEMBARK";
    return L.fishCd <= 0 && L.speed < 1.0f && foesNear(g, 10.0f * TILE) == 0 && fishSpot(g, bob, w) ? "FISH" : "";
  }
  if (L.swimming) return "";
  if (boatNear(g) >= 0) return foesNear(g, 6.0f * TILE) > 0 ? "" : "BOARD";
  int tx = 0, ty = 0;
  if (g.interactProp(tx, ty)) return "";
  return L.fishCd <= 0 && foesNear(g, 10.0f * TILE) == 0 && fishSpot(g, bob, w) ? "FISH" : "";
}

void SeaOps::shift(Game& g, Vec2 d) {
  Sea::Live& L = g.sea.live;
  L.storm.c += d;
  L.line.bob += d;
  L.moorTo += d;
  L.lastDry += d;
  L.nearBerth += d;
  L.nearPier += d;
  for (Sea::Ship& s : L.ships) s.p += d;
  for (Sea::Swimmer& s : L.swimmers) s.mark += d;
}

void SeaOps::clearMap(Game& g) {
  g.sea.live.line = Sea::Line();
  dropSwimmers(g);
}

void SeaOps::reset(Game& g) { g.sea = Sea(); g.pl().swim = 0; }

void SeaOps::loaded(Game& g) {
  Actor& p = g.pl();
  p.swim = 0;
  Sea::Live& L = g.sea.live;
  if (g.inside) g.sea.aboard = -1;   // (never saved indoors aboard; a damaged save stands ashore)
  if (!g.sea.atSea()) {
    // a ferry's hull with nobody aboard is no boat of ours; a voyage without its deck is over
    for (int i = (int)g.sea.boats.size() - 1; i >= 0; i--) if (g.sea.boats[(size_t)i].ferry) g.sea.boats.erase(g.sea.boats.begin() + i);
    L.voyage = Sea::Voyage();
    return;
  }
  const Boat& b = g.sea.boats[(size_t)g.sea.aboard];
  if (L.voyage.on && !b.ferry) L.voyage = Sea::Voyage();
  p.swim = 1;
  p.radius = hullInfo(b.hull).radius;
  L.heading = b.heading;
  L.serial++;
  // (fixer M10 r1) saved in the sinking fade (the hull at 0, not yet marked sunk): she goes down again, and the hero
  // washes ashore as the save left off
  if (!b.ferry && b.hp <= 0) { L.sinkT = 0.0001f; L.sinkBoat = g.sea.aboard; }
}

void SeaOps::saving(const Game& g, Sea& out) {
  if (!out.atSea()) return;
  Boat& b = out.boats[(size_t)out.aboard];
  b.gx = heroGx(g); b.gy = heroGy(g);
  b.heading = g.sea.live.heading;
  b.dock = 0;
}

void SeaOps::ev(Game& g, SeaEv k, Vec2 p, float f, uint32_t id, const std::string& s, Vec2 q) {
  Event e;
  e.type = Ev::Sea;
  e.p = p; e.a = (int)k; e.f = f; e.s = s; e.id = id; e.q = q;
  g.events.push_back(e);
}
