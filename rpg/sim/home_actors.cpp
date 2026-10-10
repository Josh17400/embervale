// M7 "Home": the actors of the player's property (HOMESTEAD lane, phase B). Never saved: they come and go with the
// player's distance (40 tiles), and stand where they make sense:
//   - farm animals as critter actors (art::critterSheet; the ART lane paints the cow, sheep and horse): hens and ducks
//     peck about the yard, pen beasts graze before their pen, horses at home stand by the stable, the dog by the door
//     (it barks at wolves); all keep inside the plot. The player pets them (happiness, once a day), grooms a horse with
//     the brush, rides a horse at home (RIDE);
//   - the horse out of its stable waits where the rider got off (Homes::horse / horseGx / horseGy), never indoors, in a
//     dungeon or in water; left farther than HORSE_LEFT tiles (or on a journey without it) it walks home;
//   - builders (two, by day) hammer at a building site during its 2..5 days;
//   - the hired farmhand walks the rows by day with a hoe (the watering and the harvest are catchUp's);
//   - wolves come for an exposed plot's pen at night when the player is near (VISION_PLAN 8.5): kill them or lose one
//     animal; far away the same night's roll takes one offline (catchUp).
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/home.h"

namespace {
constexpr float NEAR_TILES = 40.0f;
uint32_t hmix(uint64_t a, uint64_t b) { return (uint32_t)(ew::mix64(a * 0x9E3779B97F4A7C15ull ^ ew::mix64(b + 0x40E5Eull)) >> 16); }
Vec2 tileMid(const Game& g, int32_t gx, int32_t gy) { return Vec2((float)(gx - g.world.ox) * TILE + 8.0f, (float)(gy - g.world.oy) * TILE + 10.0f); }
bool isHomeActor(const Actor& a) { return (a.lifeBits & LB_HOMESTEAD) != 0; }
int playerTileX(const Game& g) { return g.world.ox + (int)std::floor(g.pl().p.x / TILE); }
int playerTileY(const Game& g) { return g.world.oy + (int)std::floor(g.pl().p.y / TILE); }
// a free tile inside the plot near (x, y) (plot tiles; the yard grid's free tiles that the world lets a body stand on)
bool freeYard(const Game& g, const home::Plot& p, int x, int y) {
  if (x < 0 || y < 0 || x >= p.w || y >= p.h) return false;
  for (const home::CropRec& c : p.crops) if (c.x == x && c.y == y) return false;   // nobody stands on the crops
  return HomeOps::bodyFree(g, tileMid(g, p.gx + x, p.gy + y), 4.0f);
}
bool nearestFree(const Game& g, const home::Plot& p, int x0, int y0, int& ox, int& oy) {
  for (int r = 0; r <= 6; r++)
    for (int dy = -r; dy <= r; dy++)
      for (int dx = -r; dx <= r; dx++) {
        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
        if (freeYard(g, p, x0 + dx, y0 + dy)) { ox = x0 + dx; oy = y0 + dy; return true; }
      }
  return false;
}
// where an animal of the plot lives (plot tiles): before its coop / pen / stable, the dog by the door; hens anywhere
void homeSpot(const Game& g, const home::Plot& p, int k, int& x, int& y) {
  const home::AnimalRec& a = p.animals[(size_t)k];
  const home::AnimalInfo& ai = home::animalInfo((home::Animal)a.kind);
  const uint32_t h = hmix(a.seed, (uint64_t)k);
  x = p.w / 2; y = p.h / 2;
  if (ai.home != home::Obj::COUNT) {
    int n = 0, seen = 0;
    for (const home::PlacedObj& o : p.outside) n += (home::Obj)o.kind == ai.home;
    const int pick = n ? (int)(h % (uint32_t)n) : 0;
    for (const home::PlacedObj& o : p.outside) {
      if ((home::Obj)o.kind != ai.home) continue;
      if (seen++ != pick) continue;
      const home::ObjInfo& oi = home::objInfo(ai.home);
      x = o.x + (int)((h >> 8) % (uint32_t)std::max<int>(1, oi.w));
      y = o.y + oi.h;   // the row in front of it
    }
    if ((home::Animal)a.kind == home::Animal::Chicken || (home::Animal)a.kind == home::Animal::Duck) {   // about the yard
      x = 1 + (int)((h >> 12) % (uint32_t)std::max(1, p.w - 2));
      y = 1 + (int)((h >> 20) % (uint32_t)std::max(1, p.h - 2));
    }
  } else {
    int dx, dy;
    home::doorOf(p, dx, dy);
    for (const home::PlacedObj& o : p.outside) if ((home::Obj)o.kind == home::Obj::Doghouse) { dx = o.x; dy = o.y; }
    if (dx >= 0) { x = dx + 1; y = dy + 1; }
  }
}
Actor critterActor(Game& g, art::Critter k, uint32_t var, Vec2 at, int slot, const std::string& name) {
  Actor a;
  a.id = HomeOps::nextId(g);
  a.npc = true; a.human = false; a.hostile = false; a.faction = Faction::Town;
  a.critter = (uint8_t)((int)k + 1);
  a.critterVar = var;
  a.p = at; a.home = at; a.goal = at;
  a.site = -1; a.slot = slot; a.fromMap = false;
  a.maxHp = a.hp = 30;
  const bool big = k == art::Critter::Cow || k == art::Critter::Horse || k == art::Critter::Sheep || k == art::Critter::Goat ||
                   k == art::Critter::Pig || k == art::Critter::Dog;
  a.radius = big ? 4.0f : 2.5f;
  a.speed = k == art::Critter::Dog ? 60.0f : k == art::Critter::Horse ? 40.0f : 26.0f;
  a.lifeBits = LB_HOMESTEAD;
  a.name = name;
  a.level = 1;
  return a;
}
Actor personActor(Game& g, Vec2 at, int slot, uint32_t lookSeed, const std::string& name, Role role) {
  Actor a;
  a.id = HomeOps::nextId(g);
  a.npc = true; a.human = true; a.hostile = false; a.faction = Faction::Town;
  a.p = at; a.home = at; a.goal = at;
  a.site = -1; a.slot = slot; a.fromMap = false;
  a.maxHp = a.hp = 40; a.radius = 5; a.speed = 40;
  a.role = role;
  a.name = name;
  a.lifeBits = LB_HOMESTEAD;
  Rng r(lookSeed ? lookSeed : 1u);
  HomeOps::makeLook(g, a, role, r);
  return a;
}
int findSlot(const Game& g, int slot) {
  for (size_t i = 1; i < g.actors.size(); i++) if (isHomeActor(g.actors[i]) && g.actors[i].slot == slot) return (int)i;
  return -1;
}
void stepToward(Game& g, Actor& a, Vec2 to, float speed, float dt) {
  const Vec2 d = to - a.p;
  const float l = len(d);
  if (l < 1.0f) { a.st = AState::Idle; return; }
  HomeOps::moveActor(g, a, d * (std::min(speed * dt, l) / l));
  a.face = faceOf(d);
  a.st = AState::Walk;
}
std::string critterName(const home::AnimalRec& r) { return home::animalName(r); }
// the plot's centre is within the player's reach (tiles)
bool plotNear(const Game& g, const home::Plot& p, float tiles) {
  return std::abs(p.gx + p.w / 2 - playerTileX(g)) < tiles && std::abs(p.gy + p.h / 2 - playerTileY(g)) < tiles;
}
}  // namespace

void HomeOps::clearHomeActors(Game& g) {
  for (size_t k = g.actors.size(); k-- > 1;)
    if (isHomeActor(g.actors[k])) g.actors.erase(g.actors.begin() + (std::ptrdiff_t)k);
}

// ---------------------------------------------------------------- the step
void HomeOps::actorsStep(Game& g, float dt) {
  Game::HomeRaid& R = g.homeRaid_;
  // the live raid
  if (R.plot >= 0) {
    R.t += dt;
    int alive = 0, gone = 0;
    bool atPen = false;
    if (R.plot < (int)g.home.plots.size()) {
      const home::Plot& p = g.home.plots[(size_t)R.plot];
      for (int id : R.wolves) {
        const int k = g.findActor(id);
        const bool seenDead = std::find(R.dead.begin(), R.dead.end(), id) != R.dead.end();
        if (k >= 0 && g.actors[(size_t)k].st == AState::Dead) { if (!seenDead) R.dead.push_back(id); continue; }
        if (k < 0) { if (!seenDead) gone++; continue; }
        alive++;
        if (len2(g.actors[(size_t)k].p - g.actors[(size_t)k].home) < (2.5f * TILE) * (2.5f * TILE)) atPen = true;
      }
      if (atPen) R.atPen += dt;
      // (M7 fix r3, review: "going indoors during a live raid ends it at once as driven off") a wolf that vanished
      // without being killed went with the player's leaving the overworld (a door, a journey, a respawn clears the
      // actors): the night is settled by the offline catch-up roll (one animal taken), never as a free win
      const bool away = !alive && gone > 0;
      if (!alive && !away) {
        R.outcome = 1;
        news(g, "YOU DROVE THE WOLVES FROM " + home::propertyName(g, p));
        R.plot = -1;
      } else if (away || R.atPen > 12.0f) {
        home::Plot& q = g.home.plots[(size_t)R.plot];
        std::vector<size_t> prey;
        for (size_t k = 0; k < q.animals.size(); k++) {
          const home::Animal ak = (home::Animal)q.animals[k].kind;
          if (!(q.animals[k].flags & home::AF_LOST) && ak != home::Animal::Dog && ak != home::Animal::Horse) prey.push_back(k);
        }
        if (!prey.empty()) {
          const size_t v = prey[(size_t)(hmix(q.id, (uint64_t)g.day) % (uint32_t)prey.size())];
          q.animals[v].flags |= home::AF_LOST;
          q.raidDay = (uint16_t)g.day;
          q.flags &= (uint8_t)~home::PF_RAIDED;
        }
        R.outcome = 2;
        // the pack slinks off with its prize
        for (int id : R.wolves) {
          const int k = g.findActor(id);
          if (k > 0) g.actors.erase(g.actors.begin() + k);
        }
        R.plot = -1;
      } else if (R.t > 150.0f) {
        R.outcome = 1;
        R.plot = -1;
      }
    } else R.plot = -1;
  }
  g.homeActT_ -= dt;
  if (g.homeActT_ > 0) return;
  g.homeActT_ = 0.5f;
  if (g.inside || g.travelling()) return;
  const int32_t px = playerTileX(g), py = playerTileY(g);
  std::vector<int> want;   // slots wanted in play
  const bool day = g.hour >= 7.0f && g.hour < 18.0f;
  for (size_t pi = 0; pi < g.home.plots.size(); pi++) {
    home::Plot& p = g.home.plots[pi];
    if (!plotNear(g, p, NEAR_TILES)) continue;
    const int lx = p.gx - g.world.ox, ly = p.gy - g.world.oy;
    if (!g.world.over.in(lx, ly) || !g.world.over.in(lx + p.w - 1, ly + p.h - 1)) continue;
    // animals
    for (size_t k = 0; k < p.animals.size(); k++) {
      const home::AnimalRec& r = p.animals[k];
      if (r.flags & home::AF_LOST) continue;
      if (g.home.horse == (int)pi * 64 + (int)k) continue;   // out with the player
      const int slot = HOME_SLOT_ANIMAL + (int)pi * 64 + (int)k;
      want.push_back(slot);
      const int have = findSlot(g, slot);
      if (have > 0 && g.actors[(size_t)have].critterVar != r.seed) g.actors.erase(g.actors.begin() + have);   // an index shift
      else if (have > 0) continue;
      int x, y, fx, fy;
      homeSpot(g, p, (int)k, x, y);
      if (!nearestFree(g, p, x, y, fx, fy)) continue;
      const home::AnimalInfo& ai = home::animalInfo((home::Animal)r.kind);
      Actor a = critterActor(g, ai.critter, r.seed, tileMid(g, p.gx + fx, p.gy + fy), slot, critterName(r));
      a.lifeA = (int)pi; a.lifeB = (int)k;
      g.actors.push_back(a);
    }
    // builders by day at a building site
    if (p.state == home::PlotState::Building && day) {
      for (int k = 0; k < 2; k++) {
        const int slot = HOME_SLOT_BUILDER + (int)pi * 4 + k;
        want.push_back(slot);
        if (findSlot(g, slot) > 0) continue;
        // at the site's front corners, facing it
        const int bx = k == 0 ? p.hx - 1 : p.hx + p.hw, by = p.hy + p.hh - 1 - k;
        int fx, fy;
        if (!nearestFree(g, p, bx, by, fx, fy)) continue;
        static const char* nm[] = {"BUILDER", "BUILDER'S MATE"};
        Actor a = personActor(g, tileMid(g, p.gx + fx, p.gy + fy), slot, hmix(p.id, (uint64_t)k + 77u), nm[k], Role::Villager);
        a.lifeA = (int)pi;
        a.goal = tileMid(g, p.gx + p.hx + p.hw / 2, p.gy + p.hy + p.hh / 2);
        a.posture = art::Posture::Hammer;
        a.face = faceOf(a.goal - a.p);
        g.actors.push_back(a);
      }
    }
    // the farmhand by day
    if (p.farmhand && (int)p.farmhandUntil >= g.day && day && !p.crops.empty()) {
      const int slot = HOME_SLOT_HAND + (int)pi;
      want.push_back(slot);
      if (findSlot(g, slot) <= 0) {
        int gx0, gy0, fx, fy;
        home::gateOf(p, gx0, gy0);
        if (nearestFree(g, p, gx0, gy0 - 1, fx, fy)) {
          std::string name = "FARMHAND";
          uint32_t look = hmix(p.id, 0xF4A3u);
          const int si = g.world.siteHandle(p.site);
          if (si >= 0 && p.farmhandRes != 0xFFFF)
            if (const life::Census* c = g.life.find(p.site))
              if (p.farmhandRes < c->res.size()) { name = c->res[p.farmhandRes].name; look = c->res[p.farmhandRes].lookSeed; }
          Actor a = personActor(g, tileMid(g, p.gx + fx, p.gy + fy), slot, look, name, Role::Farmer);
          a.lifeA = (int)pi; a.lifeB = -1;
          g.actors.push_back(a);
        }
      }
    }
  }
  // the horse waiting where it was left
  if (g.home.horse >= 0 && g.home.riding < 0 && !g.home.horseInn) {
    const int pi = g.home.horse / 64, k = g.home.horse % 64;
    const bool valid = pi < (int)g.home.plots.size() && k < (int)g.home.plots[(size_t)pi].animals.size();
    if (!valid) g.home.horse = -1;
    else if (std::abs(g.home.horseGx - px) > home::HORSE_LEFT || std::abs(g.home.horseGy - py) > home::HORSE_LEFT) {
      notice(g, home::animalName(g.home.plots[(size_t)pi].animals[(size_t)k]) + " HAS WANDERED HOME TO HER STABLE", rgba(200, 190, 170));
      g.home.horse = -1;
    } else if (std::abs(g.home.horseGx - px) < NEAR_TILES && std::abs(g.home.horseGy - py) < NEAR_TILES) {
      want.push_back(HOME_SLOT_HORSE);
      const home::AnimalRec& r = g.home.plots[(size_t)pi].animals[(size_t)k];
      const int have = findSlot(g, HOME_SLOT_HORSE);
      if (have <= 0) {
        Vec2 at = tileMid(g, g.home.horseGx, g.home.horseGy);
        if (!bodyFree(g, at, 4.0f)) at = HomeOps::freeSpot(g, g.home.horseGx - g.world.ox, g.home.horseGy - g.world.oy);
        Actor a = critterActor(g, art::Critter::Horse, r.seed, at, HOME_SLOT_HORSE, home::animalName(r));
        a.lifeA = pi; a.lifeB = k;
        g.actors.push_back(a);
      } else {
        // where it stands now is where it waits (it grazes a little)
        const Actor& a = g.actors[(size_t)have];
        g.home.horseGx = g.world.ox + (int)std::floor(a.p.x / TILE);
        g.home.horseGy = g.world.oy + (int)std::floor(a.p.y / TILE);
      }
    }
  }
  // the home actors no longer wanted go
  for (size_t k = g.actors.size(); k-- > 1;) {
    const Actor& a = g.actors[k];
    if (!isHomeActor(a)) continue;
    if (std::find(want.begin(), want.end(), a.slot) == want.end()) g.actors.erase(g.actors.begin() + (std::ptrdiff_t)k);
  }
}

// ---------------------------------------------------------------- behaviour
bool HomeOps::folk(Game& g, Actor& a, float dt) {
  a.postureT += dt;
  if (a.bubbleT > 0 && (a.bubbleT -= dt) <= 0) a.bubble = art::Bubble::None;
  const int pi = a.lifeA;
  const home::Plot* p = pi >= 0 && pi < (int)g.home.plots.size() ? &g.home.plots[(size_t)pi] : nullptr;
  // builders: hammering at the site
  if (a.slot >= HOME_SLOT_BUILDER && a.slot < HOME_SLOT_HAND) {
    a.face = faceOf(a.goal - a.p);
    a.st = AState::Idle;
    a.posture = art::Posture::Hammer;
    return true;
  }
  // the farmhand: along the rows, a few strokes of the hoe at each crop
  if (a.slot >= HOME_SLOT_HAND && a.slot < HOME_SLOT_HAND + 64) {
    if (!p || p->crops.empty()) { a.st = AState::Idle; a.posture = art::Posture::None; return true; }
    a.lifeT -= dt;
    if (a.lifeB < 0 || a.lifeB >= (int)p->crops.size() || a.lifeT < -14.0f) {
      a.lifeB = (int)(hmix((uint64_t)a.id, (uint64_t)(g.time * 3.0f)) % (uint32_t)p->crops.size());
      a.lifeT = 0;
      a.posture = art::Posture::None;
    }
    const home::CropRec& c = p->crops[(size_t)a.lifeB];
    // stand on the free tile beside the crop (south of it, else north)
    const Vec2 to = tileMid(g, p->gx + c.x, p->gy + c.y) + Vec2(0, (c.y + 1 < p->h) ? 9.0f : -9.0f);
    if (len2(to - a.p) > 3.0f * 3.0f && a.lifeT > -6.0f) {
      a.posture = art::Posture::None;
      const Vec2 before = a.p;
      stepToward(g, a, to, a.speed * 0.6f, dt);
      if (len2(a.p - before) < 0.0001f) a.lifeT -= dt * 4.0f;   // blocked: give up on this one sooner
      return true;
    }
    if (a.posture != art::Posture::Hoe) { a.posture = art::Posture::Hoe; a.postureT = 0; a.lifeT = -6.0f; }
    a.face = faceOf(tileMid(g, p->gx + c.x, p->gy + c.y) - a.p);
    a.st = AState::Idle;
    return true;
  }
  // animals and the waiting horse
  const art::Critter k = a.critter ? (art::Critter)(a.critter - 1) : art::Critter::Dog;
  const Actor* threat = nullptr;
  float td = (6.0f * TILE) * (6.0f * TILE);
  for (int hi : hostiles(g)) {
    if (hi < 0 || hi >= (int)g.actors.size()) continue;
    const Actor& e = g.actors[(size_t)hi];
    if (e.st == AState::Dead || !e.hostile || e.player) continue;
    const float d = len2(e.p - a.p);
    if (d < td) { td = d; threat = &e; }
  }
  if (threat) {
    a.posture = art::Posture::None;
    if (k == art::Critter::Dog) {
      a.face = faceOf(threat->p - a.p);
      a.st = AState::Idle;
      a.lifeT -= dt;
      if (a.lifeT <= 0) { a.lifeT = 1.3f; sfx(g, (int)Sfx::Bark, a.p, 1.0f, 0.8f); a.bubble = art::Bubble::Exclaim; a.bubbleT = 0.9f; }
      return true;
    }
    const Vec2 away = norm(a.p - threat->p);
    moveActor(g, a, away * (a.speed * 1.2f * dt));
    a.face = faceOf(away); a.st = AState::Walk;
    return true;
  }
  const bool night = g.hour >= 21.0f || g.hour < 5.5f;
  if (night && (k == art::Critter::Dog || k == art::Critter::Chicken || k == art::Critter::Duck)) {
    a.posture = art::Posture::Sleep; a.st = AState::Idle;
    return true;
  }
  // talked to: face the player a moment
  if (len2(g.pl().p - a.p) < (2.0f * TILE) * (2.0f * TILE) && a.bubbleT > 0) { a.face = faceOf(g.pl().p - a.p); a.st = AState::Idle; return true; }
  a.lifeT -= dt;
  if (a.lifeT <= 0) {
    const uint32_t hh = hmix((uint64_t)a.id, (uint64_t)(g.time * 7.0f));
    a.lifeT = 2.0f + (float)(hh % 400u) * 0.01f;
    const bool roam = k == art::Critter::Chicken || k == art::Critter::Duck;
    const float rx = roam ? 40.0f : 18.0f, ry = roam ? 28.0f : 10.0f;
    if (a.slot != HOME_SLOT_HORSE && hh % 3u == 0) {
      Vec2 goal = a.home + Vec2((float)((int)((hh >> 8) % 1000u) - 500) * rx / 500.0f, (float)((int)((hh >> 18) % 1000u) - 500) * ry / 500.0f);
      if (p) {   // never out of the plot
        const float x0 = (float)(p->gx - g.world.ox) * TILE + 6.0f, y0 = (float)(p->gy - g.world.oy) * TILE + 8.0f;
        goal.x = std::clamp(goal.x, x0, x0 + (float)p->w * TILE - 12.0f);
        goal.y = std::clamp(goal.y, y0, y0 + (float)p->h * TILE - 12.0f);
      }
      a.goal = bodyFree(g, goal, a.radius) ? goal : a.p;
      a.posture = art::Posture::None;
    } else {
      a.goal = a.p;
      a.posture = (hh % 3u == 1 || a.slot == HOME_SLOT_HORSE) ? art::Posture::Eat : art::Posture::Sit;   // peck, graze / rest
      if (k == art::Critter::Horse || k == art::Critter::Cow) a.posture = art::Posture::Eat;
      a.postureT = 0;
    }
  }
  if (len2(a.goal - a.p) > 2.0f * 2.0f) stepToward(g, a, a.goal, a.speed * 0.5f, dt);
  else a.st = AState::Idle;
  return true;
}

// ---------------------------------------------------------------- the player and the beasts
std::string HomeOps::critterLabel(const Game& g, const Actor& a) {
  if (a.slot == HOME_SLOT_HORSE) return g.home.riding < 0 ? "RIDE " + a.name : "";
  if (a.lifeA < 0 || a.lifeA >= (int)g.home.plots.size()) return "";
  const home::Plot& p = g.home.plots[(size_t)a.lifeA];
  if (a.lifeB < 0 || a.lifeB >= (int)p.animals.size()) return "";
  const home::AnimalRec& r = p.animals[(size_t)a.lifeB];
  if ((home::Animal)r.kind == home::Animal::Horse) {
    if (hasTool(g, home::Tool::Brush) && !(r.flags & home::AF_GROOMED)) return "GROOM " + a.name;
    return g.home.riding < 0 ? "RIDE " + a.name : "";
  }
  return "PET " + a.name;
}

bool HomeOps::critterUse(Game& g, Actor& a) {
  if (!isHomeActor(a) || !a.critter) return false;
  if (a.slot == HOME_SLOT_HORSE) {
    std::string why;
    if (!home::mountWaiting(g, why)) say(g, why);
    return true;
  }
  if (a.lifeA < 0 || a.lifeA >= (int)g.home.plots.size()) return true;
  home::Plot& p = g.home.plots[(size_t)a.lifeA];
  if (a.lifeB < 0 || a.lifeB >= (int)p.animals.size()) return true;
  home::AnimalRec& r = p.animals[(size_t)a.lifeB];
  a.face = faceOf(g.pl().p - a.p);
  a.bubble = art::Bubble::Heart; a.bubbleT = 1.6f;
  if ((home::Animal)r.kind == home::Animal::Horse) {
    if (hasTool(g, home::Tool::Brush) && !(r.flags & home::AF_GROOMED)) {
      r.flags |= home::AF_GROOMED;
      say(g, "YOU BRUSH " + a.name + "'S COAT TILL IT SHINES");
      return true;
    }
    if (g.home.riding < 0) {
      std::string why;
      if (!home::mount(g, a.lifeA, a.lifeB, why)) say(g, why);
    }
    return true;
  }
  // a pat once a day cheers a beast (happiness: better eggs, milk and wool)
  const uint64_t key = markKey(p.id ^ ((uint64_t)r.seed << 16), (Mk)(96 + 1));
  auto it = g.marks.find(key);
  if (it == g.marks.end() || it->second != g.day) {
    g.marks[key] = g.day;
    r.happiness = (uint8_t)std::min(100, r.happiness + 5);
  }
  const art::Critter k = (art::Critter)(a.critter - 1);
  sfx(g, (int)(k == art::Critter::Dog ? Sfx::Bark : Sfx::Cluck), a.p, 1.15f, 0.6f);
  return true;
}

void HomeOps::dismounted(Game& g, home::Dismount why) {
  const int id = g.home.riding;
  if (id < 0) return;
  int32_t x = g.homeDryX_, y = g.homeDryY_;
  if (!g.inside && why != home::Dismount::Water) {
    x = playerTileX(g);
    y = playerTileY(g) + (why == home::Dismount::Enter ? 1 : 0);   // not in the doorway
  }
  if (!x && !y) { x = playerTileX(g); y = playerTileY(g); }
  // (M7 fix) got off to go indoors: she waits by that building's door (the last dry tile can be far off when the
  // rider was set down at the door: a journey's arrival, a script)
  if (g.inside && g.subBldg >= 0 && g.subBldg < (int)g.world.over.bldgs.size() && why == home::Dismount::Enter) {
    const Bldg& B = g.world.over.bldgs[(size_t)g.subBldg];
    x = g.world.ox + B.doorX(); y = g.world.oy + B.doorY() + 1;
  }
  // she stands beside the rider, not under them (a free, dry tile: east, west, south, north)
  if (!g.inside) {
    static const int ox[] = {1, -1, 0, 0, 1, -1}, oy[] = {0, 0, 1, -1, 1, 1};
    for (int k = 0; k < 6; k++) {
      const int lx = x + ox[k] - g.world.ox, ly = y + oy[k] - g.world.oy;
      if (!g.world.over.in(lx, ly) || groundWater(g.world.over.at(lx, ly)) || g.world.over.at(lx, ly) == Ground::Swamp) continue;
      if (!bodyFree(g, tileMid(g, x + ox[k], y + oy[k]), 4.0f)) continue;
      x += ox[k]; y += oy[k];
      break;
    }
  }
  // in the yard of a plot with a stable: she goes into it
  const int pi = g.home.plotAt(x, y);
  if (pi >= 0 && pi == id / 64)
    for (const home::PlacedObj& o : g.home.plots[(size_t)pi].outside)
      if ((home::Obj)o.kind == home::Obj::Stable) { g.home.horse = -1; g.home.horseInn = 0; return; }
  g.home.horse = id;
  g.home.horseInn = 0;
  g.home.horseGx = x; g.home.horseGy = y;
  HomeOps::wakeActors(g);   // the actor appears now
}

void HomeOps::dayTurned(Game& g, int plot, int day) {
  (void)day;
  if (g.inside || g.homeRaid_.plot >= 0 || plot < 0 || plot >= (int)g.home.plots.size()) return;
  raidStart(g, plot);
}

int HomeOps::raidStart(Game& g, int plot) {
  using namespace home;
  if (plot < 0 || plot >= (int)g.home.plots.size()) return 0;
  Plot& p = g.home.plots[(size_t)plot];
  bool prey = false;
  for (const AnimalRec& a : p.animals)
    prey |= !(a.flags & AF_LOST) && (Animal)a.kind != Animal::Dog && (Animal)a.kind != Animal::Horse;
  if (!prey) return 0;
  const bool live = !g.inside && plotNear(g, p, 50.0f) && g.home.plots.size() > 0;
  if (!live) {
    // offline: the catch-up rule (one animal)
    std::vector<size_t> v;
    for (size_t k = 0; k < p.animals.size(); k++) {
      const Animal ak = (Animal)p.animals[k].kind;
      if (!(p.animals[k].flags & AF_LOST) && ak != Animal::Dog && ak != Animal::Horse) v.push_back(k);
    }
    p.animals[v[(size_t)(hmix(p.id, (uint64_t)g.day) % (uint32_t)v.size())]].flags |= AF_LOST;
    p.raidDay = (uint16_t)g.day;
    p.flags &= (uint8_t)~PF_RAIDED;
    return 0;
  }
  // wolves come for the pen (its front; else the coop's, else the yard's middle)
  int tx = p.w / 2, ty = p.h / 2;
  for (const PlacedObj& o : p.outside)
    if ((Obj)o.kind == Obj::Pen || (Obj)o.kind == Obj::Coop) { tx = o.x + 1; ty = o.y + objInfo((Obj)o.kind).h; if ((Obj)o.kind == Obj::Pen) break; }
  const Vec2 pen = tileMid(g, p.gx + tx, p.gy + ty);
  Game::HomeRaid& R = g.homeRaid_;
  R = Game::HomeRaid();
  R.plot = plot;
  const int n = 2 + (int)(hmix(p.id, (uint64_t)g.day * 7u) % 2u);
  const int si = g.world.siteHandle(p.site);
  const int lvl = si >= 0 ? std::max(1, g.world.sites[(size_t)si].level) : 2;
  // out of the open country 9 tiles off, from the side away from the player when a clear run leads in from it (the
  // pack trots straight for the pen: trees and walls on the way would hold it up out of sight)
  Vec2 fromP = norm(pen - g.pl().p + Vec2(0.01f, 0.0f));
  {
    const float a0 = std::atan2(fromP.y, fromP.x);
    for (int k = 0; k < 8; k++) {
      const float a = a0 + (float)((k + 1) / 2) * (k % 2 ? 0.785398f : -0.785398f);
      const Vec2 d(std::cos(a), std::sin(a));
      bool clear = true;
      for (int s = 2; s <= 9 && clear; s++) clear = HomeOps::bodyFree(g, pen + d * ((float)s * TILE), 5.0f);
      if (clear) { fromP = d; break; }
    }
  }
  for (int k = 0; k < n; k++) {
    const Vec2 at0 = pen + fromP * (9.0f * TILE) + Vec2((float)(k * 18 - 18), (float)(k % 2) * 14.0f);
    const Vec2 at = HomeOps::freeSpot(g, (int)std::floor(at0.x / TILE), (int)std::floor(at0.y / TILE));
    const int id = HomeOps::spawnMonster(g, art::Monster::Wolf, at, lvl);
    const int ai = g.findActor(id);
    if (ai > 0) {
      g.actors[(size_t)ai].home = pen;
      g.actors[(size_t)ai].goal = pen;
      g.actors[(size_t)ai].aggro = false;
    }
    R.wolves.push_back(id);
  }
  HomeOps::notice(g, "WOLVES ARE AT " + propertyName(g, p) + "!", rgba(240, 120, 90));
  return n;
}

namespace home {

int forceRaid(Game& g, int plot) { return HomeOps::raidStart(g, plot); }

int horseActor(const Game& g) { return findSlot(g, HOME_SLOT_HORSE); }

bool mountWaiting(Game& g, std::string& why) {
  if (g.home.horse < 0 || g.home.riding >= 0 || g.home.horseInn) { why = "NO HORSE IS WAITING"; return false; }
  const int k = horseActor(g);
  if (k <= 0) { why = "YOUR HORSE IS NOT HERE"; return false; }
  if (len2(g.actors[(size_t)k].p - g.pl().p) > (2.5f * TILE) * (2.5f * TILE)) { why = "GO TO YOUR HORSE"; return false; }
  const int pi = g.home.horse / 64, ai = g.home.horse % 64;
  // up into the saddle where she stands
  g.pl().p = g.actors[(size_t)k].p;
  return mount(g, pi, ai, why);
}

int horseRoomPlot(const Game& g) {
  for (size_t i = 0; i < g.home.plots.size(); i++) {
    const Plot& p = g.home.plots[i];
    int room = 0, used = 0;
    for (const PlacedObj& o : p.outside) if ((Obj)o.kind == Obj::Stable) room += objInfo(Obj::Stable).capacity;
    for (const AnimalRec& r : p.animals) if ((Animal)r.kind == Animal::Horse) used++;
    if (used < room && (int)p.animals.size() < MAX_ANIMALS) return (int)i;
  }
  return -1;
}

bool buyHorse(Game& g, Breed b, int site, std::string& why) {
  const int pi = horseRoomPlot(g);
  if (pi < 0) { why = "YOU NEED A STABLE WITH ROOM AT HOME FIRST"; return false; }
  const int price = breedInfo(b).price;
  if (g.gold < price) { why = "SHE COSTS " + std::to_string(price) + " GOLD"; return false; }
  g.gold -= price;
  Plot& p = g.home.plots[(size_t)pi];
  AnimalRec r;
  r.kind = (uint8_t)Animal::Horse;
  r.breed = (uint8_t)b;
  r.boughtDay = (uint16_t)g.day; r.lastFedDay = (uint16_t)g.day; r.lastProduceDay = (uint16_t)g.day;
  r.happiness = 70;
  const ew::Gid sid = site >= 0 && site < (int)g.world.sites.size() ? g.world.sites[(size_t)site].id : 0;
  r.seed = (uint32_t)ew::mix64(p.id ^ sid ^ ((uint64_t)p.animals.size() << 32) ^ (uint64_t)(uint32_t)g.day);
  p.animals.push_back(r);
  // she waits beside the player (another horse that was out goes home)
  if (g.home.riding >= 0) g.homeDismount(Dismount::Player);
  g.home.horse = pi * 64 + (int)p.animals.size() - 1;
  g.home.horseInn = 0;
  g.home.horseGx = playerTileX(g) + 1;
  g.home.horseGy = playerTileY(g) + 1;
  if (g.inside && g.subBldg >= 0) {
    const Bldg& B = g.world.over.bldgs[(size_t)g.subBldg];
    g.home.horseGx = g.world.ox + B.doorX() + 1; g.home.horseGy = g.world.oy + B.doorY() + 1;
  }
  for (size_t k = g.actors.size(); k-- > 1;)
    if (isHomeActor(g.actors[k]) && g.actors[k].slot == HOME_SLOT_HORSE) g.actors.erase(g.actors.begin() + (std::ptrdiff_t)k);
  HomeOps::wakeActors(g);
  HomeOps::sfx(g, (int)Sfx::Coin, g.pl().p);
  return true;
}

int stableOwed(const Game& g) {
  if (g.home.horse < 0 || !g.home.horseInn) return 0;
  return std::max(0, g.day - (int)g.home.horseGy - 1) * STABLE_INN_FEE;
}

bool stableAtInn(Game& g, int site, std::string& why) {
  if (site < 0 || site >= (int)g.world.sites.size()) { why = "THERE IS NO STABLE HERE"; return false; }
  const ew::Gid sid = g.world.sites[(size_t)site].id;
  if (g.home.horse >= 0 && g.home.horseInn == sid) {   // fetch her: she waits at the inn's door
    const int owed = stableOwed(g);   // the nights after the first (paid when she was stabled)
    if (g.gold < owed) { why = "THE STABLE WANTS " + std::to_string(owed) + " GOLD FOR HER KEEP"; return false; }
    g.gold -= owed;
    g.home.horseInn = 0;
    int tx, ty;
    overworldTile(g, tx, ty);
    g.home.horseGx = g.world.ox + tx + 1; g.home.horseGy = g.world.oy + ty;
    HomeOps::wakeActors(g);
    return true;
  }
  if (g.home.horse < 0 && g.home.riding < 0) { why = "YOU HAVE NO HORSE WITH YOU"; return false; }
  if (g.gold < STABLE_INN_FEE) { why = "THE STABLE IS " + std::to_string(STABLE_INN_FEE) + " GOLD A NIGHT"; return false; }
  g.gold -= STABLE_INN_FEE;
  if (g.home.riding >= 0) { g.home.horse = g.home.riding; g.home.riding = -1; }
  g.home.horseInn = sid;
  g.home.horseGx = 0; g.home.horseGy = g.day;   // (while stabled) the day she came: the nightly fee runs from it
  for (size_t k = g.actors.size(); k-- > 1;)
    if (isHomeActor(g.actors[k]) && g.actors[k].slot == HOME_SLOT_HORSE) g.actors.erase(g.actors.begin() + (std::ptrdiff_t)k);
  return true;
}

}  // namespace home
