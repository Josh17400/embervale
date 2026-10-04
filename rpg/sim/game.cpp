// EMBERVALE core simulation: player control, movement/collision, combat, AI, spawning, maps.
#include "rpg/sim/game.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/stream.h"

using art::Monster;
using art::Prop;

// The key killedSlots[0] holds for an overworld spawn of a site (M1: 4096 slots per site; saves store site id + slot)
static int owKillKey(int site, int slot) { return site * 4096 + (slot & 4095); }

const char* spellName(Spell s) {
  static const char* n[] = {"FIREBOLT", "MEND", "FROST LANCE"};
  return n[(int)s];
}
int spellCost(Spell s) {
  static const int c[] = {16, 28, 24};
  return c[(int)s];
}


Game::Game(uint64_t s) : seed(s), rng_(s) {}

int Game::findActor(int id) const {
  for (int i = 0; i < (int)actors.size(); i++) if (actors[i].id == id) return i;
  return -1;
}

void Game::sfx(int s, Vec2 p, float pitch, float vol) { emit(Ev::Sfx, p, s, pitch, ""); events.back().vol = vol; }
void Game::say(const std::string& s) { notice = s; noticeT = 3.5f; }

float Game::daylight() const {
  // smooth curve: dark 21..5, dawn 5..8, day, dusk 18..21
  float h = hour;
  if (h < 5 || h >= 21) return 0;
  if (h < 8) return smooth01((h - 5) / 3);
  if (h < 18) return 1;
  return 1 - smooth01((h - 18) / 3);
}

// ------------------------------------------------------------------ setup
void Game::newGame(uint64_t s, int genVer) {
  seed = s;
  rng_ = Rng(s ^ 0xABCDEF);
  world.streamer.reset();   // (an endless world's prefetcher and look-ups do not apply to the island)
  world.nearSites.clear(); world.nearDens.clear(); world.siteSpawns.clear(); world.sstats = World::StreamStats();
  world.generate(s, genVer);
  beginWorld();
}

void Game::newEndlessGame(uint64_t s) {
  seed = s;
  rng_ = Rng(s ^ 0xABCDEF);
  world.generateEndless(s);
  beginWorld();
}

// every per-game state back to a new game's: the player alone, no quests, nothing looted (beginWorld, loading a save)
void Game::resetSession() {
  inside = false; subSite = -1; subBldg = -1; subFloor = 0;
  lodging = Lodging();
  time = 0; hour = 8.5f; day = 1;
  quests.clear(); nextQuestId = 1; trackedQuest = -1;
  npcQuestsDone.clear(); looted.clear(); killedSlots.clear(); shopCache_.clear(); explored.clear();
  projs.clear(); pickups.clear(); events.clear();
  kills = 0; dungeonsCleared = 0; blessT = 0;
  app = Appearance(); background = Background::None; storyFlags = 0;
  actors.clear();
  Actor p;
  p.id = nextId_++;
  p.player = true; p.human = true; p.name = "YOU"; p.faction = Faction::Player;
  actors.push_back(p);
  resetPlayer();
  clearNonPlayer();
  perf = PerfCounters();
  prefetchT_ = 0; siteScanT_ = 0; prefetchFrom_ = Vec2();
  curSite = -1;
  if (!world.endless) world.rebuildSiteSpawns();   // the classic island's spawns, by site (endless worlds keep it as they stream)
}

// everything a new game sets up once its world exists
void Game::beginWorld() {
  resetSession();
  lastTown = world.startSite;
  placePlayerAt(world.sites[world.startSite].r.cx(), world.sites[world.startSite].r.cy() + 1);
  const Site home = world.sites[world.startSite];   // a copy: the endless window may load more sites
  // main quest
  Quest q;
  q.id = nextQuestId++;
  q.type = QType::Main;
  q.title = "THE DRAGON'S SHADOW";
  const Site cap = world.sites[world.capital];
  q.giverSite = world.capital;
  q.target = world.capital;
  q.desc = "A DRAGON HAS BEEN SEEN OVER THE PEAKS. JARL OF " + cap.name + " SEEKS ANYONE BRAVE ENOUGH TO HELP. TRAVEL TO " + cap.name + " AND SPEAK WITH THE JARL IN THE KEEP.";
  q.stage = 0;
  quests.push_back(q);
  // the opening (VISION_PLAN 15.1): shirt only, so the first tracked quest leads to a weapon within a few minutes.
  // The start village's innkeeper keeps an old blade over the hearth (game_rpg.cpp giveFirstWeapon).
  {
    Quest o;
    o.id = nextQuestId++;
    o.type = QType::Retrieve;
    o.title = "A BLADE OF YOUR OWN";
    o.giverSite = world.startSite;
    o.giverName = "THE INNKEEPER";
    for (int b = home.bldgFirst; b < home.bldgFirst + home.bldgCount; b++)
      if (world.over.bldgs[b].type == art::Building::Inn) { o.giverBldg = b; break; }
    o.giverSlot = -1;   // whoever keeps the inn
    o.target = world.startSite;
    o.desc = "YOU CAME TO " + home.name + " WITH NOTHING BUT THE SHIRT ON YOUR BACK. THE INNKEEPER IS SAID TO KEEP AN OLD BLADE OVER THE HEARTH, AND TO GIVE IT TO ANYONE WILLING TO WORK.";
    o.gold = 0; o.xp = 20;
    quests.push_back(o);
    trackedQuest = o.giverBldg >= 0 ? o.id : q.id;
  }
  loadMapActors();
  // nobody stands on the newcomer's toes: a villager placed right on the spawn steps aside, so the first key press or
  // tap doesn't open a dead-end conversation and no name label sits on the hero's head
  for (size_t k = 1; k < actors.size(); k++) {
    Actor& a = actors[k];
    if (!a.npc || len2(a.p - pl().p) >= 26.0f * 26.0f) continue;
    static const Vec2 offs[] = {{-32, 0}, {32, 0}, {0, -32}, {-32, -32}, {32, -32}, {0, 32}, {-32, 32}, {32, 32}};
    for (Vec2 o : offs) {
      Vec2 q = pl().p + o;
      if (world.over.blocked((int)std::floor(q.x / TILE), (int)std::floor((q.y - 2) / TILE))) continue;
      bool crowded = false;
      for (size_t j = 1; j < actors.size(); j++) if (j != k && len2(actors[j].p - q) < 12.0f * 12.0f) crowded = true;
      if (crowded) continue;
      a.p = q;
      break;
    }
  }
  updateLocation();
}

void Game::resetPlayer() {
  plLevel = 1; plXp = 0; gold = 30; perkPts = 0;
  baseHp = 100; maxMp = 60; maxSt = 80;
  inv.clear();
  eqWeapon = eqBow = eqStaff = eqArmor = eqHelmet = eqShield = eqRing = eqAmulet = -1;
  eqGloves = eqBoots = eqCloak = -1;
  // everyone starts with just the shirt on their back (VISION_PLAN 15.1): no weapon, armour, bow or arrows.
  // A heel of bread and a few coins; the first weapon comes from the start village (the opening quest).
  Item bread = makeFood(0); bread.count = 2;
  inv.push_back(bread);
  spellsKnown = 0; spell = Spell::Flames;   // no magic at the start: spells come from a background, tomes or teachers
  recalcPlayer();
  Actor& p = pl();
  p.hp = p.maxHp; mp = maxMp; stamina = maxSt;
  p.st = AState::Idle;
}

void Game::debugKit() {
  Rng r(seed);
  spellsKnown |= (uint8_t)(1 << (int)Spell::Flames);
  Item sw = makeWeapon(r, 1, (int)WeaponType::Sword, false);
  sw.name = "IRON SWORD"; sw.power = 8;
  addItem(sw, false);
  eqWeapon = (int)inv.size() - 1;
  Item bow; bow.kind = ItemKind::Bow; bow.power = 9; bow.name = "HUNTING BOW"; bow.icon = art::Icon::Bow; bow.tint = tierTint(0); bow.value = 40;
  addItem(bow, false);
  eqBow = (int)inv.size() - 1;
  addItem(makeArrows(20), false);
  Item pot = makePotion(PotionType::Health, 0); pot.count = 3;
  addItem(pot, false);
  Item bread = makeFood(0); bread.count = 2;
  addItem(bread, false);
  recalcPlayer();
}

// The character creator is done (app and background are set): apply the background's trait and start playing.
// M0 town-defence/opening lane: the background effects (VISION_PLAN 15.1) are applied here.
void Game::finishCreator() {
  app.created = true;
  storyFlags |= SF_CREATED;
  // the background's one trait (VISION_PLAN 15.1). Never gear. The rest act where they matter:
  //   blacksmith's child  priceFactor(Smith)                      hunter      aggro range (ai.cpp), pelts (dropLoot)
  //   farmhand            stamina regen (updatePlayer), food      urchin      priceFactor(Merchant)
  //   temple novice       MEND known, blessingSecs()              noble       hasOffer: guards and jarls offer more
  //   sailor / marked     small hooks: sea legs (+stamina) / a faint echo of lost magic (+magicka)
  switch (background) {
    case Background::Novice: spellsKnown |= (uint8_t)(1 << (int)Spell::Heal); spell = Spell::Heal; break;
    case Background::Sailor: maxSt += 15; break;
    case Background::Marked: maxMp += 15; spellsKnown |= (uint8_t)(1 << (int)Spell::Flames); spell = Spell::Flames; break;
    default: break;
  }
  recalcPlayer();
  pl().hp = pl().maxHp; mp = maxMp; stamina = maxSt;
  mode = Mode::Play;
  if (int oq = openingQuest(); oq >= 0) {
    trackedQuest = oq;
    emit(Ev::QuestUpdate, pl().p, oq, 0, "QUEST STARTED: A BLADE OF YOUR OWN");
  }
}

Vec2 Game::freeSpot(int tx, int ty) const {
  const Map& m = map();
  for (int r = 0; r < 12; r++)
    for (int oy = -r; oy <= r; oy++)
      for (int ox = -r; ox <= r; ox++) {
        if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
        int x = tx + ox, y = ty + oy;
        if (!m.blocked(x, y)) return Vec2(x * TILE + 8.0f, y * TILE + 10.0f);
      }
  return Vec2(tx * TILE + 8.0f, ty * TILE + 10.0f);
}
void Game::placePlayerAt(int tx, int ty) {
  if (world.endless && !inside) {
    // a teleport off the window's middle (fast travel, respawn, a script's goto): move the window there first
    const int lo = World::WIN_SHIFT, hi = World::WIN - World::WIN_SHIFT;
    if (tx < lo || ty < lo || tx >= hi || ty >= hi) {
      int32_t gx = world.ox + tx, gy = world.oy + ty;
      int32_t ox0 = world.ox, oy0 = world.oy;
      world.recentreOn(gx, gy);
      windowMoved(world.ox - ox0, world.oy - oy0);
      tx = gx - world.ox; ty = gy - world.oy;
      prefetchT_ = 0;
      prefetchFrom_ = Vec2();
    }
  }
  pl().p = freeSpot(tx, ty);
  pl().vel = Vec2();
  pl().knock = Vec2();
}

// ------------------------------------------------------------------ the endless window (M1, VISION_PLAN 2.9)
void Game::maybeRecentre() {
  const Actor& p = pl();
  int tx = (int)std::floor(p.p.x / TILE), ty = (int)std::floor(p.p.y / TILE);
  const int lo = World::WIN_SHIFT, hi = World::WIN - World::WIN_SHIFT;
  int sx = tx < lo ? -World::WIN_SHIFT : (tx >= hi ? World::WIN_SHIFT : 0);
  int sy = ty < lo ? -World::WIN_SHIFT : (ty >= hi ? World::WIN_SHIFT : 0);
  if (!sx && !sy) return;
  world.shiftWindow(sx, sy);
  windowMoved(sx, sy);
  prefetchT_ = 0;   // the ring moved with the window: refresh the wish list this step
}

void Game::windowMoved(int dx, int dy) {
  if (!dx && !dy) return;
  const Vec2 d(-dx * (float)TILE, -dy * (float)TILE);
  auto move = [&](Actor& a) {
    a.p += d; a.home += d; a.goal += d;
    a.navGoal = -1; a.navNext = -1; a.navT = 0;
  };
  for (Actor& a : actors) move(a);
  for (Actor& a : sheltered_) move(a);
  for (Projectile& pr : projs) pr.p += d;
  for (Pickup& pk : pickups) pk.p += d;
  reapplyLooted();
  emit(Ev::WindowShift, d);
}

// M1 streaming (VISION_PLAN 2.8): keep the prefetcher's wish list current, a few times a second and right after a
// window move. The streamer is made on first use: a worker thread natively, the per-frame pump on the web.
void Game::prefetchTick(float dt) {
  prefetchT_ -= dt;
  if (prefetchT_ > 0) return;
  prefetchT_ = 0.2f;
  if (!world.streamer) {
#ifdef __EMSCRIPTEN__
    world.streamer = std::make_shared<ChunkStreamer>(world.seed, world.src, false);
#else
    world.streamer = std::make_shared<ChunkStreamer>(world.seed, world.src, true);
#endif
  }
  const Actor& p = pl();
  Vec2 d = p.p - prefetchFrom_;
  if (len2(prefetchFrom_) < 1e-6f || len2(d) > (64.0f * TILE) * (64.0f * TILE)) d = Vec2();   // a teleport: no heading
  prefetchFrom_ = p.p;
  int dirx = d.x > 2.0f ? 1 : (d.x < -2.0f ? -1 : 0), diry = d.y > 2.0f ? 1 : (d.y < -2.0f ? -1 : 0);
  world.prefetch((int)std::floor(p.p.x / TILE), (int)std::floor(p.p.y / TILE), dirx, diry);
}

void Game::frameWork(double budgetMs) {
  if (world.streamer) world.streamer->pump(budgetMs);
}

// NPC LOD: a townsperson well off screen (beyond sleepHalfW tiles across or sleepHalfH up and down: half the view plus
// four tiles) with nothing to react to: no alarm at their town, not talking, not fighting or fleeing
bool Game::sleepy(const Actor& a) const {
  if (inside || !a.npc || a.hostile || a.st == AState::Dead) return false;
  const Vec2 d = a.p - pl().p;
  if (std::fabs(d.x) < sleepHalfW * TILE && std::fabs(d.y) < sleepHalfH * TILE) return false;   // on screen or nearly
  if (a.target >= 0 || a.aggro || a.fleeT > 0 || a.fleeing || a.indoors || len2(a.knock) > 1.0f) return false;
  if (mode == Mode::Dialogue && dlg.actor == a.id) return false;
  if (a.site >= 0) {
    auto it = alarms_.find(a.site);
    if (it != alarms_.end() && (it->second.ringing || time - it->second.lastThreatT < 30.0f)) return false;
  }
  return true;
}

// the actors that may threaten townsfolk this step (monsters, bandits, anything not a friendly NPC or the player):
// town folk scan these for threats instead of every actor (a big city's crowd made that quadratic)
void Game::collectHostiles() {
  hostiles_.clear();
  int n = 0;
  for (size_t i = 1; i < actors.size(); i++) {
    const Actor& e = actors[i];
    if (e.npc && !e.hostile) continue;
    hostiles_.push_back((int)i);
    if (e.hostile && e.st != AState::Dead) n++;
  }
  perf.hostiles = n;
}

void Game::reapplyLooted() {
  if (!world.endless) return;
  Map& m = world.over;
  bool any = false;
  for (uint64_t k : looted) {
    if ((k >> 60) != 0xE) continue;
    int32_t gx = (int32_t)((int64_t)(k << 8) >> 36), gy = (int32_t)((int64_t)(k << 36) >> 36);   // 28-bit signed fields
    int lx = gx - world.ox, ly = gy - world.oy;
    if (!m.in(lx, ly)) continue;
    uint8_t& pr = m.prop[(size_t)ly * m.w + lx];
    if (pr == (int)Prop::Chest + 1) { pr = (uint8_t)((int)Prop::ChestOpen + 1); any = true; }
  }
  if (any) m.rebuildSolid();
}

uint64_t Game::lootKey(int tx, int ty) const {
  if (!inside && world.endless) {
    // 0xE in the top nibble, then the global tile as two 28-bit fields (|coordinate| < 2^27, far beyond the World's End)
    uint64_t gx = (uint64_t)(uint32_t)(world.ox + tx) & 0xFFFFFFFull, gy = (uint64_t)(uint32_t)(world.oy + ty) & 0xFFFFFFFull;
    return (0xEull << 60) | (gx << 28) | gy;
  }
  const Map& m = map();
  return ((uint64_t)mapKey() << 32) | (uint32_t)((size_t)ty * m.w + tx);
}

// ------------------------------------------------------------------ collision
bool Game::solidAt(float x, float y, bool flying) const {
  const Map& m = map();
  int tx = (int)std::floor(x / TILE), ty = (int)std::floor(y / TILE);
  if (!m.in(tx, ty)) return true;
  if (flying) {
    Ground g = m.at(tx, ty);
    return g == Ground::CaveWall || g == Ground::InteriorWall || g == Ground::Void || (g == Ground::Rock && m.kind != MapKind::Overworld);
  }
  return m.blocked(tx, ty);
}

bool Game::bodyFree(Vec2 p, float r, bool flying) const {
  float ry = r * 0.6f;
  return !solidAt(p.x - r, p.y - ry, flying) && !solidAt(p.x + r, p.y - ry, flying) && !solidAt(p.x - r, p.y + ry, flying) && !solidAt(p.x + r, p.y + ry, flying);
}

void Game::moveActor(Actor& a, Vec2 d) {
  float steps = std::ceil(std::max(std::fabs(d.x), std::fabs(d.y)) / 3.0f);
  if (steps < 1) steps = 1;
  Vec2 s = d * (1.0f / steps);
  float rx = a.radius, ry = a.radius * 0.6f;
  bool fl = a.flying || a.fly;
  if (a.fly) { a.p += d; return; }
  auto hit = [&](float x, float y) {
    return solidAt(x - rx, y - ry, fl) || solidAt(x + rx, y - ry, fl) || solidAt(x - rx, y + ry, fl) || solidAt(x + rx, y + ry, fl);
  };
  for (int i = 0; i < (int)steps; i++) {
    if (!hit(a.p.x + s.x, a.p.y)) a.p.x += s.x;
    else if (s.y == 0) {   // corner slide: nudge around tile corners
      if (!hit(a.p.x + s.x, a.p.y - 2)) a.p.y -= 0.6f; else if (!hit(a.p.x + s.x, a.p.y + 2)) a.p.y += 0.6f;
    }
    if (!hit(a.p.x, a.p.y + s.y)) a.p.y += s.y;
    else if (s.x == 0) {
      if (!hit(a.p.x - 2, a.p.y + s.y)) a.p.x -= 0.6f; else if (!hit(a.p.x + 2, a.p.y + s.y)) a.p.x += 0.6f;
    }
  }
}

// ------------------------------------------------------------------ main update
void Game::update(float dt, const Input& in) {
  if (mode != Mode::Play) return;
  if (stFlash > 0) stFlash -= dt;
  if (hitStop > 0) { hitStop -= dt; return; }
  if (slowMo > 0) { slowMo -= dt; dt *= 0.3f; }   // perfect roll / level-up: a brief slow-motion blip
  time += dt;
  float prevHour = hour;
  hour += dt * (24.0f / 840.0f);   // a day lasts 14 minutes
  if (hour >= 24) { hour -= 24; day++; }
  (void)prevHour;
  if (noticeT > 0) noticeT -= dt;
  if (blessT > 0) { blessT -= dt; if (blessT <= 0) recalcPlayer(); }
  if (sleepFade > 0) sleepFade = std::max(0.0f, sleepFade - dt);

  updatePlayer(dt, in);
  if (world.endless && !inside) { maybeRecentre(); prefetchTick(dt); }
  if (!inside && (exploreT_ -= dt) <= 0) {   // fog of war: what the screen shows around the player (global tiles)
    exploreT_ = 0.25f;
    explored.markAround(world.ox + (int)std::floor(pl().p.x / TILE), world.oy + (int)std::floor(pl().p.y / TILE), 16);
  }
  collectHostiles();
  perf.npcAwake = perf.npcAsleep = 0;
  for (size_t i = 1; i < actors.size(); i++) {
    Actor& a = actors[i];
    // NPC level of detail (VISION_PLAN 4.4): townsfolk far off screen with nothing to react to think a few times a
    // second with the gathered time; everything near, fighting or in a ringing town runs every step
    if (a.npc && sleepy(a)) {
      a.asleep = true;
      perf.npcAsleep++;
      a.lodAcc += dt;
      if (a.lodAcc < 0.2f) continue;
      float acc = std::min(a.lodAcc, 0.25f);
      a.lodAcc = 0;
      updateActor(a, acc);
      continue;
    }
    if (a.npc) perf.npcAwake++;
    if (a.asleep) { a.asleep = false; a.lodAcc = 0; }
    updateActor(a, dt);
  }
  // separation between bodies
  for (size_t i = 0; i < actors.size(); i++)
    for (size_t j = i + 1; j < actors.size(); j++) {
      Actor& a = actors[i]; Actor& b = actors[j];
      if (a.st == AState::Dead || b.st == AState::Dead || a.fly || b.fly) continue;
      if (a.asleep && b.asleep) continue;
      Vec2 d = b.p - a.p;
      float r = a.radius + b.radius;
      float l2 = len2(d);
      if (l2 >= r * r || l2 < 1e-4f) continue;
      float l = std::sqrt(l2);
      Vec2 push = d * ((r - l) / l * 0.5f);
      if (!a.player) moveActor(a, push * -1.0f);
      if (!b.player) moveActor(b, push);
      if (a.player && b.npc) moveActor(b, push);
    }
  updateTownDefence(dt);
  updateProjectiles(dt);
  updatePickups(dt);
  if (!inside) updateSpawning(dt);
  updateLocation();
  // cleanup corpses
  for (size_t i = 1; i < actors.size();) {
    Actor& a = actors[i];
    bool far = a.wild && len2(a.p - pl().p) > (48.0f * TILE) * (48.0f * TILE);
    if ((a.st == AState::Dead && a.stT > 6.0f) || far) {
      if (a.id == dragonId_) dragonId_ = -1;
      actors.erase(actors.begin() + i);
    } else i++;
  }
}

void Game::updatePlayer(float dt, const Input& in) {
  Actor& p = pl();
  p.stT += dt;
  p.animT += dt;
  if (p.flash > 0) p.flash -= dt;
  if (p.iframes > 0) p.iframes -= dt;
  if (p.atkCd > 0) p.atkCd -= dt;
  if (p.shootCd > 0) p.shootCd -= dt;
  if (p.comboT > 0) p.comboT -= dt; else p.combo = 0;
  if (p.st == AState::Dead) return;
  // regen
  bool busy = p.st == AState::Windup || p.st == AState::Strike || p.st == AState::Roll;
  const float stRegen = background == Background::Farmhand ? 1.3f : 1.0f;   // farmhand: a field worker's wind
  if (!busy) stamina = stamina + dt * 22.0f * stRegen;
  float mpMax = maxMp;
  for (int idx : worn()) if (idx >= 0 && inv[idx].ench == Ench::Magicka) mpMax += inv[idx].enchPow;
  float stMax = maxSt;
  for (int idx : worn()) if (idx >= 0 && inv[idx].ench == Ench::Stamina) stMax += inv[idx].enchPow;
  stamina = std::min(stamina, stMax);
  if (!busy) stamina = std::min(stMax, stamina + dt * 4.0f * stRegen);
  mp = std::min(mpMax, mp + dt * 3.2f);
  // health comes back between fights, barely during one (the potion is the in-fight heal)
  bool calm = time - lastHurtT > 6.0f;
  p.hp = std::min(p.maxHp, p.hp + dt * (blessT > 0 ? 1.6f : calm ? 1.1f : 0.12f));
  if (p.burnT > 0) {
    p.burnT -= dt;
    if (!godMode) p.hp -= dt * 3;
    if (p.hp <= 0) { kill(p, -1); return; }   // burning can finish you off
  }
  if (p.slowT > 0) p.slowT -= dt;

  // M0b: rooms are regenerated from the building's seed; a save made on a different layout (a generator still in
  // development) can put the player inside a wall: step out onto the nearest free tile
  if (inside && subBldg >= 0) {
    int ux = (int)std::floor(p.p.x / TILE), uy = (int)std::floor((p.p.y - 2) / TILE);
    if (sub.blocked(ux, uy) && groundSolid(sub.at(ux, uy))) placePlayerAt(ux, uy);
  }
  Vec2 mv = in.move;
  float ml = len(mv);
  if (ml > 1) { mv = mv * (1.0f / ml); ml = 1; }
  // M0b doorway assist (indoors): walking straight at a one-tile doorway (an interior door, the front door) slides you
  // onto its centre line instead of catching you on its frame
  if (inside && subBldg >= 0 && ml > 0.3f) {
    int ax = (int)std::floor(p.p.x / TILE), ay = (int)std::floor((p.p.y - 2) / TILE);
    if (std::fabs(mv.y) > 0.5f && std::fabs(mv.x) < 0.5f) {
      int ny = ay + (mv.y > 0 ? 1 : -1);
      if (!sub.blocked(ax, ny) && groundSolid(sub.at(ax - 1, ny)) && groundSolid(sub.at(ax + 1, ny))) {
        float dx = ax * TILE + 8.0f - p.p.x;
        if (std::fabs(dx) > 1.0f) mv.x = std::clamp(dx * 0.2f, -0.7f, 0.7f);
      }
    } else if (std::fabs(mv.x) > 0.5f && std::fabs(mv.y) < 0.5f) {
      int nx = ax + (mv.x > 0 ? 1 : -1);
      if (!sub.blocked(nx, ay) && groundSolid(sub.at(nx, ay - 1)) && groundSolid(sub.at(nx, ay + 1))) {
        float dy = ay * TILE + 10.0f - p.p.y;
        if (std::fabs(dy) > 1.0f) mv.y = std::clamp(dy * 0.2f, -0.7f, 0.7f);
      }
    }
  }
  if (ml > 0.15f) p.aim = norm(mv);

  // knockback decays
  if (len2(p.knock) > 0.01f) { moveActor(p, p.knock * dt); p.knock *= std::pow(0.001f, dt); }

  switch (p.st) {
    case AState::Roll: {
      float k = 1.0f - p.stT / 0.34f;
      moveActor(p, p.vel * (dt * (0.45f + k)));
      if (p.stT >= 0.34f) { p.st = AState::Idle; p.stT = 0; }
      if (((int)(p.stT * 30)) % 3 == 0) emit(Ev::Dust, p.p);
      return;
    }
    case AState::Windup:
      if (p.stT >= (eqWeapon < 0 ? 0.05f : 0.07f)) {
        p.st = AState::Strike; p.stT = 0; p.hitDone = false;
        sfx((int)Sfx::Swing, p.p, (eqWeapon < 0 ? 1.45f : 1.0f) + p.combo * 0.12f, eqWeapon < 0 ? 0.6f : 1.0f);
      }
      break;
    case AState::Strike:
      if (!p.hitDone) { meleeHit(p); p.hitDone = true; }
      moveActor(p, p.aim * (dt * (p.combo == 2 ? 70.0f : 30.0f)));
      if (p.stT >= 0.11f) { p.st = AState::Recover; p.stT = 0; }
      break;
    case AState::Recover:
      if (p.stT >= (p.combo == 2 ? 0.28f : eqWeapon < 0 ? 0.08f : 0.13f)) { p.st = AState::Idle; p.stT = 0; }
      break;
    case AState::Cast:
      if (p.stT >= 0.22f) { p.st = AState::Idle; p.stT = 0; }
      break;
    case AState::Hurt:
      if (p.stT >= 0.18f) { p.st = AState::Idle; p.stT = 0; }
      break;
    default: break;
  }
  bool canAct = p.st == AState::Idle || p.st == AState::Walk || (p.st == AState::Recover && p.stT > 0.06f);
  float slow = p.slowT > 0 ? 0.6f : 1.0f;
  if (p.st == AState::Idle || p.st == AState::Walk) {
    if (ml > 0.15f) {
      moveActor(p, mv * (p.speed * slow * dt));
      p.face = faceOf(mv);
      if (p.st != AState::Walk) { p.st = AState::Walk; }
      float stepPeriod = 0.29f;
      if ((int)(p.animT / stepPeriod) != (int)((p.animT - dt) / stepPeriod)) sfx((int)Sfx::Step, p.p, 0.85f + rng_.f() * 0.3f, 0.5f);
    } else if (p.st == AState::Walk) p.st = AState::Idle;
  } else if (p.st == AState::Windup || p.st == AState::Cast) {
    if (ml > 0.15f) moveActor(p, mv * (p.speed * 0.35f * dt));
  }

  if (canAct && in.roll && stamina < 20) { if (stFlash <= 0) sfx((int)Sfx::MenuBack, p.p, 0.6f); stFlash = 0.6f; }
  if (canAct && in.roll && stamina >= 20) {
    stamina -= 20;
    perfectRoll_ = false;
    Vec2 d = ml > 0.15f ? norm(mv) : p.aim;
    p.vel = d * 165.0f;
    p.st = AState::Roll; p.stT = 0; p.iframes = 0.3f;
    p.face = faceOf(d);
    sfx((int)Sfx::Roll, p.p);
    return;
  }
  // a use (talk, sleep, open, pray) wins over a swing given in the same step: on touch the use tap and the held
  // finger can land in one step when a render frame runs no sim step
  const bool swing = in.attack && !in.interact;
  if (canAct && swing) {
    // aim assist: snap toward the nearest hostile close by
    int best = -1; float bd = 34.0f * 34.0f;
    for (size_t i = 1; i < actors.size(); i++) {
      const Actor& e = actors[i];
      if (!e.hostile || e.st == AState::Dead || e.fly) continue;
      float d2 = len2(e.p - p.p);
      if (d2 < bd) { bd = d2; best = (int)i; }
    }
    if (best >= 0) { p.aim = norm(actors[best].p - p.p); p.face = faceOf(p.aim); }
    p.combo = (p.comboT > 0) ? (p.combo + 1) % 3 : 0;
    p.comboT = 0.55f;
    float cost = p.combo == 2 ? (eqWeapon < 0 ? 10.0f : 14.0f) : (eqWeapon < 0 ? 3.0f : 4.0f);
    if (stamina < cost) { if (p.combo == 2 || stamina < 4) stFlash = 0.6f; p.combo = 0; }
    stamina = std::max(0.0f, stamina - cost);
    p.st = AState::Windup; p.stT = 0;
    return;
  }
  if (canAct && in.bow) { shootArrow(); return; }
  if (canAct && in.spell) { castSpell(); return; }
  if (in.swapSpell) {
    for (int k = 1; k <= (int)Spell::COUNT; k++) {
      int s = ((int)spell + k) % (int)Spell::COUNT;
      if (spellsKnown & (1 << s)) { spell = (Spell)s; break; }
    }
    say(std::string("SPELL: ") + spellName(spell));
    sfx((int)Sfx::MenuMove, p.p);
  }
  if (in.potion) {
    int best = -1;
    for (int i = 0; i < (int)inv.size(); i++)
      if (inv[i].kind == ItemKind::Potion && inv[i].sub == (uint8_t)PotionType::Health && (best < 0 || inv[i].power < inv[best].power)) best = i;
    if (best < 0) for (int i = 0; i < (int)inv.size(); i++) if (inv[i].kind == ItemKind::Food) { best = i; break; }
    if (best >= 0) useItem(best); else say("NO HEALING ITEMS");
  }
  if (in.interact) interact();

  // doors, cave mouths and exits
  const Map& m = map();
  int tx = (int)std::floor(p.p.x / TILE), ty = (int)std::floor((p.p.y - 2) / TILE);
  if (!inside) {
    if (m.in(tx, ty)) {
      int bi = m.bldgAt[(size_t)ty * m.w + tx];
      if (bi >= 0 && m.bldgs[bi].doorX() == tx && m.bldgs[bi].doorY() == ty) { enterBuilding(bi); return; }
      int pr = m.propAt(tx, ty);
      if (pr == (int)Prop::CaveEntrance + 1 || pr == (int)Prop::IronDoor + 1) {
        int si = world.siteAt(tx, ty);
        if (si >= 0 && (world.sites[si].type == SiteType::Cave || world.sites[si].type == SiteType::Ruin)) { enterSite(si); return; }
      }
    }
  } else {
    // M0b: stairs between the floors of a building. Walking onto the steps takes you to the other floor; you arrive
    // beside the stairs there, and must step off them before they take you back. A flight up climbs north into the
    // wall, so it takes a step north (walking past along the wall does not carry you upstairs); a stairwell down is
    // taken from any side.
    if (subBldg >= 0 && (sub.up.valid() || sub.down.valid())) {
      // a flight is two tiles wide (both carry the stairs prop; Map::up/down record the first)
      int spr = m.propAt(tx, ty);
      bool onUp = sub.up.valid() && spr == (int)Prop::StairsUp + 1;
      bool onDown = sub.down.valid() && spr == (int)Prop::StairsDown + 1;
      bool moving = len2(mv) > 0.09f && p.st != AState::Hurt && len2(p.knock) < 30.0f * 30.0f;
      if (onUp && mv.y > -0.3f) moving = false;
      // arriving with the stick still held (a phone thumb stays on it) must not walk you straight back: the stairs
      // wake once the stick is released or you have moved away from where you arrived
      if (stairsLatch_ && len2(mv) < 0.01f) stairsLatch_ = false;
      if ((onUp || onDown) && len2(mv) < 0.01f) stairsNorth_ = true;   // let go on the steps: a fresh push north takes them
      if (stairsLatch_ && len2(p.p - stairsFrom_) > (1.5f * TILE) * (1.5f * TILE)) stairsLatch_ = false;
      // fix round 3: after climbing, letting go on the arrival tile is not enough to wake the stairwell down against a
      // push north: a phone player lifts the thumb during the fade and pushes on the way they climbed. Until the player
      // has stood on another tile off the steps, only a push across (from beside the opening) or south takes it.
      bool onArrival = stairsArrive_ >= 0 && ty * m.w + tx == stairsArrive_;
      if (!onUp && !onDown) { stairsNorth_ = false; if (!stairsLatch_) stairsArmed_ = true; if (!onArrival) stairsArrive_ = -1; stairsOff_ = p.p; }
      else if (onDown && stairsArrive_ >= 0 && mv.y < -0.3f && !stairsNorth_) {
        // pushing on north into the opening straight after the climb: its railing turns you back (no walking over it)
        p.p = stairsOff_;
        p.vel = Vec2();
      }
      else if (moving && ((stairsArmed_ && !(onDown && stairsArrive_ >= 0 && mv.y < -0.3f)) || (stairsNorth_ && mv.y < -0.3f))) {
        changeFloor(subFloor + (onUp ? 1 : -1));
        return;
      }
    }
    bool onExit = tx == sub.exitX && (ty == sub.exitY || (int)std::floor(p.p.y / TILE) == sub.exitY);
    // leaving takes intent: walking down onto the ladder/doorway. Being knocked or staggered onto it mid-fight
    // must not throw you out of the dungeon.
    bool intent = mv.y > 0.3f && p.st != AState::Hurt && len2(p.knock) < 30.0f * 30.0f;
    if (!onExit) exitArmed_ = true;
    else if (exitArmed_ && intent) { leaveSub(); return; }
  }
}

void Game::meleeHit(Actor& a) {
  bool isPl = a.player;
  float reach = isPl ? (a.combo == 2 ? 24.0f : 20.0f) : a.range;
  float cone = isPl ? (a.combo == 2 ? -0.1f : 0.25f) : 0.2f;
  float dmg = isPl ? weaponDamage() * (a.combo == 2 ? 1.7f : 1.0f) : a.dmg;
  if (isPl && eqWeapon < 0) {   // bare fists: short reach, quick jabs, a finisher that staggers
    reach -= 4;
    dmg *= a.combo == 2 ? 1.0f : 1.2f;
  }
  Ench ench = Ench::None; float ep = 0;
  if (isPl && eqWeapon >= 0) { ench = inv[eqWeapon].ench; ep = inv[eqWeapon].enchPow; }
  if (isPl && inv.size() && eqWeapon >= 0 && inv[eqWeapon].sub == (uint8_t)WeaponType::Dagger) reach -= 3;
  Vec2 origin = a.p + Vec2(0, -4);
  bool any = false;
  for (size_t i = 0; i < actors.size(); i++) {
    Actor& v = actors[i];
    if (v.id == a.id || v.st == AState::Dead) continue;
    if (v.fly) continue;   // can't reach a dragon in the air
    bool enemy = isPl ? v.hostile : factionsHostile(a.faction, v.faction);   // monsters hit townsfolk too (M0)
    if (!enemy) continue;
    Vec2 d = (v.p + Vec2(0, -4)) - origin;
    float l = len(d);
    if (l > reach + v.radius) continue;
    if (l > 4 && dot(d * (1.0f / l), a.aim) < cone) continue;
    bool crit = isPl && rng_.f() < 0.12f;
    damage(v, dmg * (0.9f + rng_.f() * 0.2f) * (crit ? 1.8f : 1.0f), a.p, a.id, ench, ep, crit);
    any = true;
  }
  if (isPl && any) {
    emit(Ev::Shake, a.p, 0, a.combo == 2 ? 2.5f : 1.2f);
    hitStop = a.combo == 2 ? 0.07f : 0.035f;
  }
  // smash pots/props? (not yet)
}

void Game::damage(Actor& v, float dmg, Vec2 from, int attacker, Ench ench, float ep, bool crit) {
  if (v.st == AState::Dead) return;
  if (v.player && v.st == AState::Roll && v.iframes > 0 && attacker >= 0 && !perfectRoll_) {
    // rolled through a blow that would have landed: a slow-motion blip and a little stamina back
    perfectRoll_ = true;
    slowMo = 0.4f;
    stamina = std::min(stamina + 12.0f, maxSt);
    emit(Ev::Sparkle, v.p + Vec2(0, -8));
    emit(Ev::Text, v.p + Vec2(0, -22), (int)rgba(150, 220, 255), 0, "DODGE");
    sfx((int)Sfx::Roll, v.p, 1.6f);
  }
  if (v.player && (v.iframes > 0 || godMode)) return;
  if (v.st == AState::Down) {   // smashing the bones before they rise
    v.reassembled = true;
    v.hp = 0;
    emit(Ev::Hit, v.p + Vec2(0, -4), 0, dmg);
    sfx((int)Sfx::HitHeavy, v.p, 1.3f);
    kill(v, attacker);
    return;
  }
  if (v.npc && !v.hostile) {   // townsfolk and guards are only hurt by their enemies (never by the player's swings)
    int ai = attacker >= 0 ? findActor(attacker) : -1;
    if (ai < 0 || !factionsHostile(actors[ai].faction, v.faction)) return;
  }
  if (v.player) dmg = dmg * 100.0f / (100.0f + armorRating() * 1.6f);
  else dmg = dmg * 60.0f / (60.0f + v.armor);
  if (ench == Ench::Fire) { v.burnT = 3.0f; v.regen = 0; dmg += ep * 0.4f; }
  if (ench == Ench::Frost) { v.slowT = 3.0f; dmg += ep * 0.5f; emit(Ev::Frost, v.p); }
  if (ench == Ench::Drain && attacker == pl().id) { pl().hp = std::min(pl().maxHp, pl().hp + ep * 0.5f); dmg += ep * 0.3f; }
  v.hp -= dmg;
  v.flash = 0.12f;
  v.lastHitT = time;
  if (v.player) lastHurtT = time;
  Vec2 dir = norm(v.p - from);
  float kb = v.boss ? 30.0f : (v.player ? 110.0f : 150.0f);
  if (v.mon == Monster::Dragon) kb = 0;
  v.knock = dir * kb;
  emit(Ev::Text, v.p + Vec2(0, -18), v.player ? (int)rgba(255, 90, 80) : crit ? (int)rgba(255, 210, 60) : (int)rgba(255, 255, 255), dmg,
       std::to_string((int)std::ceil(dmg)) + (crit ? "!" : ""));
  if (crit) emit(Ev::Shake, v.p, 0, 3.0f);
  emit(Ev::Hit, v.p + Vec2(0, -6), 0, dmg);
  if (v.human || v.mon == Monster::Wolf || v.mon == Monster::Bear || v.mon == Monster::Boar || v.mon == Monster::Troll || v.mon == Monster::IceWolf || v.mon == Monster::Dragon)
    emit(Ev::Blood, v.p + Vec2(0, -6));
  if (v.player) { sfx((int)Sfx::PlayerHurt, v.p); emit(Ev::Shake, v.p, 0, 3); v.iframes = 0.35f; }
  else sfx((int)(dmg > 18 ? Sfx::HitHeavy : Sfx::Hit), v.p, 0.9f + rng_.f() * 0.2f);
  if (!v.player) {
    v.aggro = true;
    v.target = attacker;
    // a committed lunge or heavy slam can't be interrupted by a light hit: roll, don't trade
    bool committed = v.st == AState::Strike || (v.st == AState::Windup && (v.heavy || v.lunge) && dmg < v.maxHp * 0.3f);
    if ((v.mon == Monster::Wolf || v.mon == Monster::IceWolf) && !v.human && dmg < v.maxHp * 0.4f) committed = true;   // wolves shrug off light cuts
    if (!v.boss && dmg > v.maxHp * 0.12f && !committed) { v.st = AState::Hurt; v.stT = 0; v.heavy = false; v.lunge = false; }
    // goblins lose their nerve when badly hurt; so does the last wolf of a pack
    bool lastWolf = false;
    // (inside a settlement nothing flees: whatever got in fights until the guards end it, as the leash rule says)
    const bool inTownV = !inside && settlementAt(v.p) >= 0;
    if ((v.mon == Monster::Wolf || v.mon == Monster::IceWolf) && !v.human && v.hp > 0 && v.hp < v.maxHp * 0.4f && !inTownV) {
      lastWolf = true;
      for (const Actor& o : actors) if (o.id != v.id && o.mon == v.mon && !o.human && o.hostile && o.st != AState::Dead && len2(o.p - v.p) < 160 * 160) lastWolf = false;
    }
    if (((v.mon == Monster::Goblin && v.hp < v.maxHp * 0.3f && !inTownV) || lastWolf) && !v.human && !v.fleeing && v.hp > 0) {
      v.fleeing = true;
      emit(Ev::Text, v.p + Vec2(0, -20), (int)rgba(255, 230, 120), 0, "!");
    }
  } else if (v.st != AState::Roll && dmg > 8) { v.st = AState::Hurt; v.stT = 0; }
  if (v.hp <= 0) kill(v, attacker);
}

void Game::kill(Actor& a, int killer) {
  if (a.mon == Monster::Skeleton && !a.human && !a.boss && !a.reassembled && a.st != AState::Down && rng_.f() < 0.4f) {
    // the bones collapse... and may stand back up (see updateActor). Another blow while it is down finishes it.
    a.reassembled = true;
    a.hp = 1;
    a.st = AState::Down; a.stT = 0;
    a.heavy = a.lunge = false;
    sfx((int)Sfx::EnemyDie, a.p, 1.4f);
    emit(Ev::Dust, a.p);
    return;
  }
  a.hp = 0;
  a.st = AState::Dead;
  a.stT = 0;
  a.fly = false;
  if (a.player) {
    mode = Mode::Dead;
    sfx((int)Sfx::Death, a.p);
    return;
  }
  sfx((int)Sfx::EnemyDie, a.p, a.boss ? 0.7f : 1.0f);
  // only the player's own blows, arrows and fire earn XP, the kill count and hunt progress: a wolf the town guard
  // cuts down is not the player's kill
  const bool byPlayer = killer == pl().id;
  if (byPlayer) {
    if (a.xp > 0) gainXp(a.xp);
    kills++;
  }
  // townsfolk felled by monsters are back on their feet when the town next loads (only enemies stay dead)
  if (a.fromMap && !a.npc) killedSlots[mapKey()].insert(!inside && a.site >= 0 ? owKillKey(a.site, a.slot) : a.slot);
  if (a.npc && !inside) emit(Ev::Text, a.p + Vec2(0, -20), (int)rgba(255, 120, 100), 0, "DOWN");
  if (a.den >= 0 && !inside && a.den < (int)world.dens.size()) {
    bool left = false;
    for (const Actor& o : actors) if (o.id != a.id && o.den == a.den && o.st != AState::Dead) left = true;
    if (!left && denClearedDay(a.den) < 0) {
      // the den is cleared: a small reward, and it stays empty for a few days (killedSlots[-1] = den * 4096 + day)
      killedSlots[-1].insert(a.den * 4096 + std::min(day, 4095));
      int bonus = 12 + world.zoneLevel(world.dens[a.den].x, world.dens[a.den].y) * 4;
      if (byPlayer) {
        emit(Ev::Notice, a.p, (int)rgba(255, 210, 90), 0, "DEN CLEARED  +" + std::to_string(bonus) + " XP");
        sfx((int)Sfx::QuestDone, a.p, 1.15f);
        gainXp(bonus);
      }
    }
  }
  dropLoot(a);
  if (byPlayer) questKill(a);
  if (!inside && a.site >= 0 && a.site < (int)world.sites.size() && world.sites[a.site].type == SiteType::BanditCamp) {
    // a camp is broken when its chief falls (what the bounty asks: "KILL THEIR CHIEF"), or when its last fighter
    // does (a camp whose chief is already gone). Either way the player hears about it at once: CLEARED and BOUNTY
    // READY, or NOT YOUR TARGET, and how many stragglers still fight on.
    int left = 0;
    for (const Actor& o : actors)
      if (o.id != a.id && o.site == a.site && o.hostile && o.st != AState::Dead && !o.npc) left++;
    if (a.boss || left == 0) {
      const bool was = world.sites[a.site].cleared;
      checkDungeonCleared(a.site);
      if (!was && a.boss && left > 0)
        emit(Ev::Text, a.p + Vec2(0, -34), (int)rgba(255, 200, 120), 0,
             "CHIEF SLAIN - " + std::to_string(left) + (left == 1 ? " BANDIT FIGHTS ON" : " BANDITS FIGHT ON"));
    }
  } else if (a.boss) {
    if (inside && subSite >= 0) checkDungeonCleared(subSite);
    else if (!inside && a.site >= 0) checkDungeonCleared(a.site);
  }
}

void Game::gainXp(int xp) {
  plXp += xp;
  while (plXp >= xpForNext()) {
    plXp -= xpForNext();
    plLevel++;
    perkPts++;
    emit(Ev::LevelUp, pl().p, plLevel);
    sfx((int)Sfx::LevelUp, pl().p);
    emit(Ev::Shake, pl().p, 0, 3.0f);
    slowMo = std::max(slowMo, 0.5f);
    say("LEVEL UP! YOU ARE NOW LEVEL " + std::to_string(plLevel));
    recalcPlayer();
  }
}

void Game::chooseLevelUp(int stat) {
  if (perkPts <= 0) return;
  perkPts--;
  if (stat == 0) baseHp += 12;
  else if (stat == 1) maxMp += 12;
  else maxSt += 12;
  recalcPlayer();
  pl().hp = pl().maxHp; mp = maxMp; stamina = maxSt;
  sfx((int)Sfx::MenuSelect, pl().p);
  if (perkPts <= 0 && mode == Mode::LevelUp) mode = Mode::Play;
}

// ------------------------------------------------------------------ ranged
void Game::shootArrow() {
  Actor& p = pl();
  if (eqBow < 0) { say("NO BOW EQUIPPED"); return; }
  int ai = -1;
  for (int i = 0; i < (int)inv.size(); i++) if (inv[i].kind == ItemKind::Arrows && inv[i].count > 0) { ai = i; break; }
  if (ai < 0) { say("OUT OF ARROWS"); return; }
  if (p.shootCd > 0) return;
  // aim at the nearest hostile roughly in front
  Vec2 aim = p.aim;
  float bd = 1e9f;
  for (size_t i = 1; i < actors.size(); i++) {
    const Actor& e = actors[i];
    if (!e.hostile || e.st == AState::Dead) continue;
    Vec2 d = e.p - p.p;
    float l = len(d);
    if (l > 190 || l < 1) continue;
    float score = l * (1.6f - dot(d * (1.0f / l), p.aim));
    if (dot(d * (1.0f / l), p.aim) > 0.2f && score < bd) { bd = score; aim = norm(e.p + Vec2(0, -6) - (p.p + Vec2(0, -8))); }
  }
  inv[ai].count--;
  if (inv[ai].count <= 0) {
    inv.erase(inv.begin() + ai);
    auto fix = [&](int& e) { if (e == ai) e = -1; else if (e > ai) e--; };
    fix(eqWeapon); fix(eqBow); fix(eqStaff); fix(eqArmor); fix(eqHelmet); fix(eqShield); fix(eqRing); fix(eqAmulet);
    fix(eqGloves); fix(eqBoots); fix(eqCloak);
  }
  Projectile pr;
  pr.p = p.p + Vec2(0, -8) + aim * 6;
  pr.v = aim * 290;
  pr.dmg = inv[eqBow].power * (1.0f + (plLevel - 1) * 0.04f);
  pr.ench = inv[eqBow].ench; pr.enchPow = inv[eqBow].enchPow;
  pr.fromPlayer = true; pr.owner = p.id; pr.kind = ProjKind::Arrow; pr.life = 0.9f;
  projs.push_back(pr);
  p.aim = aim; p.face = faceOf(aim);
  p.st = AState::Cast; p.stT = 0; p.shootCd = 0.42f;
  sfx((int)Sfx::Arrow, p.p);
}

void Game::castSpell() {
  Actor& p = pl();
  if (!(spellsKnown & (1 << (int)spell))) {
    say(spellsKnown ? "SWAP TO A SPELL YOU KNOW" : "YOU KNOW NO SPELLS YET");
    sfx((int)Sfx::MenuBack, p.p);
    return;
  }
  int cost = spellCost(spell);
  if (mp < cost) { say("NOT ENOUGH MAGICKA"); sfx((int)Sfx::MenuBack, p.p); return; }
  if (p.shootCd > 0) return;
  mp -= cost;
  float power = 1.0f + (plLevel - 1) * 0.05f + (eqStaff >= 0 ? inv[eqStaff].power * 0.02f : 0);
  if (spell == Spell::Heal) {
    p.hp = std::min(p.maxHp, p.hp + 45 * power);
    emit(Ev::Heal, p.p);
    sfx((int)Sfx::Heal, p.p);
  } else {
    Vec2 aim = p.aim; float bd = 1e9f;
    for (size_t i = 1; i < actors.size(); i++) {
      const Actor& e = actors[i];
      if (!e.hostile || e.st == AState::Dead) continue;
      Vec2 d = e.p - p.p; float l = len(d);
      if (l > 170 || l < 1) continue;
      if (dot(d * (1.0f / l), p.aim) > 0.1f && l < bd) { bd = l; aim = norm(e.p + Vec2(0, -6) - (p.p + Vec2(0, -8))); }
    }
    Projectile pr;
    pr.p = p.p + Vec2(0, -8) + aim * 6;
    pr.fromPlayer = true; pr.owner = p.id;
    if (spell == Spell::Flames) { pr.kind = ProjKind::Fireball; pr.v = aim * 200; pr.dmg = 18 * power; pr.ench = Ench::Fire; pr.enchPow = 8 * power; pr.radius = 4; }
    else { pr.kind = ProjKind::IceSpike; pr.v = aim * 260; pr.dmg = 24 * power; pr.ench = Ench::Frost; pr.enchPow = 6 * power; }
    pr.life = 1.0f;
    projs.push_back(pr);
    p.aim = aim; p.face = faceOf(aim);
    sfx((int)(spell == Spell::Flames ? Sfx::Fireball : Sfx::Frost), p.p);
  }
  p.st = AState::Cast; p.stT = 0; p.shootCd = 0.35f;
}

void Game::updateProjectiles(float dt) {
  for (size_t i = 0; i < projs.size();) {
    Projectile& pr = projs[i];
    pr.life -= dt;
    Vec2 prev = pr.p;
    pr.p += pr.v * dt;
    bool dead = pr.life <= 0;
    // walls stop projectiles (water and low props don't)
    const Map& m = map();
    int tx = (int)std::floor(pr.p.x / TILE), ty = (int)std::floor((pr.p.y + 8) / TILE);
    if (!dead && pr.kind != ProjKind::DragonFire) {
      Ground g = m.at(tx, ty);
      bool wall = g == Ground::Rock || g == Ground::CaveWall || g == Ground::InteriorWall || g == Ground::Void;
      if (m.in(tx, ty) && (m.bldgAt[(size_t)ty * m.w + tx] >= 0 || m.wall[(size_t)ty * m.w + tx])) wall = true;
      int pp = m.propAt(tx, ty);
      if (pp) {
        Prop p = (Prop)(pp - 1);
        if (p == Prop::OakTree || p == Prop::OakTree2 || p == Prop::PineTree || p == Prop::PineTree2 || p == Prop::SnowPine || p == Prop::Boulder ||
            p == Prop::BirchTree || p == Prop::AutumnTree || p == Prop::WillowTree || p == Prop::Stalagmite) wall = true;
      }
      if (wall) dead = true;
    }
    if (!dead) {
      for (size_t k = 0; k < actors.size(); k++) {
        Actor& a = actors[k];
        if (a.st == AState::Dead || a.id == pr.owner) continue;
        bool enemy = pr.fromPlayer ? a.hostile : (a.player || a.npc) && factionsHostile(pr.fac, a.faction);
        if (!enemy) continue;
        Vec2 c = a.p + Vec2(0, a.fly ? -28.0f : -7.0f);
        float r = a.radius + pr.radius + (a.mon == Monster::Dragon ? 10 : 2);
        if (len2(c - pr.p) < r * r) {
          damage(a, pr.dmg, prev, pr.owner, pr.ench, pr.enchPow);
          dead = true;
          if (pr.kind == ProjKind::Arrow) sfx((int)Sfx::ArrowHit, pr.p);
          break;
        }
      }
    }
    if (dead) {
      if (pr.kind == ProjKind::Fireball || pr.kind == ProjKind::DragonFire) {
        emit(Ev::Explode, pr.p, 0, pr.kind == ProjKind::DragonFire ? 1.5f : 1.0f);
        if (pr.kind == ProjKind::Fireball) {
          sfx((int)Sfx::Explode, pr.p, 1.0f, 0.7f);
          for (auto& a : actors)
            if (a.hostile && a.st != AState::Dead && len2(a.p - pr.p) < 22 * 22 && len2(a.p - pr.p) > 4) damage(a, pr.dmg * 0.4f, pr.p, pr.owner, Ench::Fire, pr.enchPow * 0.5f);
        } else {
          Actor& p = pl();
          if (len2(p.p - pr.p) < 20 * 20) damage(p, pr.dmg, pr.p, pr.owner, Ench::Fire, 4);
        }
      }
      if (pr.kind == ProjKind::IceSpike) emit(Ev::Frost, pr.p);
      projs.erase(projs.begin() + i);
    } else i++;
  }
}

void Game::updatePickups(float dt) {
  Actor& p = pl();
  for (size_t i = 0; i < pickups.size();) {
    Pickup& k = pickups[i];
    k.t += dt;
    Vec2 d = p.p - k.p;
    float l = len(d);
    if (k.t > 0.5f && l < 40) {
      if (!k.magnet) { k.magnet = true; sfx((int)(k.gold > 0 ? Sfx::Coin : Sfx::Pickup), k.p, 1.5f + rng_.f() * 0.2f, 0.5f); }
      k.p += d * (std::min(1.0f, dt * 7.0f));
    }
    if (k.t > 0.5f && l < 9) {
      if (k.gold > 0) { giveGold(k.gold); }
      else addItem(k.item);
      pickups.erase(pickups.begin() + i);
      continue;
    }
    if (k.t > 240) { pickups.erase(pickups.begin() + i); continue; }
    i++;
  }
}

// ------------------------------------------------------------------ NPC / monster update
void Game::updateActor(Actor& a, float dt) {
  a.stT += dt;
  a.animT += dt;
  if (a.flash > 0) a.flash -= dt;
  if (a.atkCd > 0) a.atkCd -= dt;
  if (a.shootCd > 0) a.shootCd -= dt;
  if (a.st == AState::Dead) return;
  if (a.burnT > 0) {
    a.burnT -= dt;
    a.hp -= dt * (3 + a.level * 0.6f);
    if (a.hp <= 0) { kill(a, pl().id); return; }
  }
  if (a.slowT > 0) a.slowT -= dt;
  // troll regeneration pauses for a moment after every wound (and fire stops it, see damage())
  if (a.regen > 0 && a.hp < a.maxHp && time - a.lastHitT > 3.0f) a.hp = std::min(a.maxHp, a.hp + a.regen * dt);
  if (len2(a.knock) > 0.01f) { moveActor(a, a.knock * dt); a.knock *= std::pow(0.0005f, dt); }
  if (a.st == AState::Down) {
    // a collapsed skeleton rattles back together unless it is struck again
    if (a.stT > 2.8f) {
      a.st = AState::Idle; a.stT = 0;
      a.hp = a.maxHp * 0.5f;
      a.atkCd = 0.6f;
      sfx((int)Sfx::Block, a.p, 1.4f);
      emit(Ev::Dust, a.p);
      emit(Ev::Text, a.p + Vec2(0, -20), (int)rgba(220, 220, 200), 0, "RISES");
    }
    return;
  }
  updateAI(a, dt);
}

// ------------------------------------------------------------------ spawning
void Game::applyLevel(Actor& a, int level) {
  a.level = std::max(1, level);
  // tuned against the player's growth (perks, gear): a same-level wolf stays a 3-4 hit kill from level 1 to 5
  float hm = 1.0f + 0.10f * (a.level - 1), dm = 1.0f + 0.13f * (a.level - 1);
  a.maxHp *= hm; a.hp = a.maxHp; a.dmg *= dm;
  a.xp = (int)(a.xp * (1.0f + 0.12f * (a.level - 1)));
  a.armor = a.level * 1.5f;
}

int Game::spawnMonster(Monster m, Vec2 p, int level, bool boss) {
  const MStat& s = mstat(m);
  Actor a;
  a.id = nextId_++;
  a.hostile = true; a.mon = m; a.p = p; a.home = p; a.goal = p;
  a.faction = monsterFaction(m);
  a.maxHp = s.hp; a.dmg = s.dmg; a.speed = s.speed; a.range = s.range; a.aggroR = s.aggro; a.radius = s.radius;
  a.windup = s.windup; a.xp = s.xp; a.ranged = s.ranged; a.flying = s.flying;
  a.name = monsterName(m);
  a.orbitDir = (hash32((uint32_t)a.id * 2654435761u) & 1) ? 1 : -1;
  if (m == Monster::Troll) a.regen = 2.5f;
  applyLevel(a, level);
  if (boss && m != Monster::Dragon) {
    a.boss = true; a.maxHp *= 3.2f; a.hp = a.maxHp; a.dmg *= 1.35f; a.xp *= 4; a.radius += 1.5f;
    static const char* titles[] = {"ANCIENT ", "ELDER ", "DREAD ", "GREAT "};
    a.name = std::string(titles[hash32((uint32_t)a.id) % 4]) + a.name;
  }
  if (m == Monster::Dragon) {
    a.boss = true; a.fly = true; a.special = 10;
    // generic level scaling would push Ashfang past 4000 HP: a slog, not a fight. Keep it a few minutes long.
    a.maxHp = 700.0f + a.level * 60.0f; a.hp = a.maxHp;
  }
  actors.push_back(a);
  return a.id;
}

void Game::makeLook(Actor& a, Role r, Rng& rr) {
  art::HumanLook& L = a.look;
  static const uint32_t skins[] = {rgba(244, 204, 168), rgba(232, 180, 140), rgba(200, 146, 104), rgba(150, 100, 70), rgba(110, 72, 50)};
  static const uint32_t hairs[] = {rgba(40, 30, 24), rgba(90, 56, 30), rgba(150, 96, 50), rgba(210, 170, 90), rgba(180, 70, 40), rgba(200, 200, 200), rgba(240, 220, 150)};
  static const uint32_t cloth[] = {rgba(70, 110, 150), rgba(150, 60, 50), rgba(80, 120, 70), rgba(140, 110, 60), rgba(110, 80, 130), rgba(160, 140, 110), rgba(60, 70, 90), rgba(170, 120, 60)};
  bool female = rr.f() < 0.45f;
  L.skin = skins[rr.irange(5)];
  L.hairColor = hairs[rr.irange(7)];
  L.topColor = cloth[rr.irange(8)];
  L.bottomColor = cloth[rr.irange(8)];
  L.bottomColor = rgba((int)((L.bottomColor & 255) * 0.7f), (int)(((L.bottomColor >> 8) & 255) * 0.7f), (int)(((L.bottomColor >> 16) & 255) * 0.7f));
  L.hair = female ? (rr.f() < 0.5f ? art::Hair::Long : (rr.f() < 0.5f ? art::Hair::Braids : art::Hair::Ponytail))
                  : (rr.f() < 0.15f ? art::Hair::Bald : (rr.f() < 0.15f ? art::Hair::Mohawk : art::Hair::Short));
  L.beard = !female && rr.f() < 0.45f;
  L.outfit = female && rr.f() < 0.6f ? art::Outfit::Dress : art::Outfit::Tunic;
  L.weapon = 0;
  a.name = makePersonName(rr, female);
  switch (r) {
    case Role::Guard: L.outfit = art::Outfit::Guard; L.helmet = true; L.shield = true; L.weapon = 1; L.tabardColor = rgba(150, 40, 40); L.beard = false; break;
    case Role::Jarl: L.outfit = art::Outfit::Plate; L.cape = true; L.tabardColor = rgba(130, 30, 40); a.name = "JARL " + a.name; break;
    case Role::Priest: L.outfit = art::Outfit::Robe; L.topColor = rgba(232, 234, 244); L.tabardColor = rgba(150, 34, 48); a.name = "PRIEST " + a.name; break;   // white + crimson: never skin-toned
    case Role::Mage: L.outfit = art::Outfit::Robe; L.topColor = rgba(70, 60, 140); L.tabardColor = rgba(200, 180, 80); L.hood = rr.f() < 0.5f; L.weapon = 4; break;
    case Role::Smith: L.outfit = art::Outfit::Leather; L.weapon = 6; L.beard = !female; break;
    case Role::Innkeeper: L.outfit = female ? art::Outfit::Dress : art::Outfit::Tunic; L.topColor = rgba(170, 140, 100); break;
    case Role::Merchant: L.topColor = rgba(150, 70, 110); break;
    case Role::Farmer: L.topColor = rgba(140, 120, 70); L.weapon = 0; break;
    case Role::Bandit:
      L.outfit = rr.f() < 0.6f ? art::Outfit::Leather : art::Outfit::Rags; L.hood = rr.f() < 0.5f; L.tabardColor = rgba(80, 60, 50);
      L.weapon = rr.f() < 0.5f ? 1 : 2;
      break;
    case Role::King:
      // gilded plate under a long noble cloak in the kingdom's colours (below), a winged gilded helm, a sword
      L.outfit = art::Outfit::Plate; L.armorStyle = 4; L.helmStyle = 4; L.helmet = true; L.cloak = 3; L.beard = !female;
      L.weapon = 1; L.weaponColor = rgba(236, 206, 120); L.trimColor = rgba(236, 196, 92); L.amulet = true; L.ring = true;
      L.tabardColor = rgba(130, 30, 40); L.cloakColor = rgba(130, 30, 40);
      a.name = "KING " + a.name;
      break;
    default: break;
  }
  // M1 kingdom identity (VISION_PLAN 15.8): guards wear their kingdom's colours; the king's cloak and the royal guard
  // (the palace and its barracks) carry the royal banner. (Colours only: the look's random draws above are unchanged.)
  const int home = a.site >= 0 ? a.site : (a.bldg >= 0 && a.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)a.bldg].site : -1);
  if (const Kingdom* K = world.kingdomOf(home)) {
    const art::Building in = a.bldg >= 0 && a.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)a.bldg].type : art::Building::House;
    const bool royal = in == art::Building::Palace || in == art::Building::Barracks;
    if (r == Role::Guard) {
      L.tabardColor = K->color;
      if (royal) {   // the royal guard: steel plate and great-helm, a heater shield with the royal field and trim
        L.outfit = art::Outfit::Plate; L.armorStyle = 3; L.helmStyle = 3; L.shieldStyle = 2; L.trimColor = K->color2;
        L.cloak = 3; L.cloakColor = K->color;
        a.name = "ROYAL GUARD";
      } else if (world.sites[(size_t)home].capital) {
        L.shieldStyle = 2; L.trimColor = K->color2;   // a capital's watch carries the royal shield
      }
    }
    if (r == Role::King) { L.cloakColor = K->color; L.tabardColor = K->color; }
    if (r == Role::Jarl) L.tabardColor = K->color;
  }
}

int Game::spawnHuman(const Spawn& sp, Vec2 p) {
  Actor a;
  a.id = nextId_++;
  a.human = true; a.p = p; a.home = p; a.goal = p;
  a.site = sp.site; a.slot = sp.slot; a.fromMap = true; a.bldg = inside ? subBldg : -1;
  uint64_t key = ((uint64_t)(sp.site + 1) << 24) ^ ((uint64_t)(a.bldg + 1) << 12) ^ (uint64_t)sp.slot;
  Rng rr(hash32((uint32_t)key) ^ (uint32_t)seed);
  if (sp.bandit) {
    a.hostile = true; a.role = Role::Bandit; a.faction = Faction::Bandit;
    makeLook(a, Role::Bandit, rr);
    a.ranged = rr.f() < 0.35f && !sp.boss;
    if (a.ranged) a.look.weapon = 3;
    a.maxHp = 46; a.dmg = 9; a.speed = 50; a.range = 15; a.aggroR = 130; a.windup = 0.36f; a.xp = 18; a.radius = 4.5f;
    a.name = a.ranged ? "BANDIT ARCHER" : "BANDIT";
    const Site& st = world.sites[std::max(0, sp.site)];
    applyLevel(a, st.level);
    if (sp.boss) {
      a.boss = true; a.maxHp *= 3; a.hp = a.maxHp; a.dmg *= 1.4f; a.xp *= 4; a.name = "BANDIT CHIEF";
      a.look.outfit = art::Outfit::Plate; a.look.helmet = true; a.look.weapon = 2; a.look.cape = true; a.look.tabardColor = rgba(60, 40, 40);
    }
  } else {
    a.npc = true; a.role = sp.role;
    makeLook(a, sp.role, rr);
    a.maxHp = 80; a.hp = 80; a.dmg = 14; a.speed = 46; a.range = 16; a.radius = 4.5f;
    if (sp.role == Role::Guard) {
      a.maxHp = 300; a.hp = 300; a.dmg = 22; a.speed = 54;
      if (a.name != "ROYAL GUARD") a.name = "GUARD";
      else { a.maxHp = 420; a.hp = 420; a.dmg = 28; }
    }
    if (sp.role == Role::King) { a.maxHp = 500; a.hp = 500; a.dmg = 30; }
    if (sp.role == Role::Child) { a.radius = 3.5f; a.look.outfit = art::Outfit::Tunic; a.look.beard = false; }
    a.level = 10;
    // militia (M0 town defence): brave adults with a tool pick it up when monsters come. Weak, and they run when hurt.
    // (Decided by hash so the look's random stream is untouched.)
    if (!inside && (sp.role == Role::Smith || sp.role == Role::Farmer || (sp.role == Role::Villager && hash32((uint32_t)key * 2246822519u ^ (uint32_t)seed) % 100 < 30))) {
      a.militia = true;
      a.dmg = 7;
    }
  }
  actors.push_back(a);
  return a.id;
}

void Game::clearNonPlayer() {
  actors.resize(1);
  projs.clear();
  pickups.clear();
  activeSites_.clear();
  activeDens_.clear();
  sheltered_.clear();
  alarms_.clear();
  alarmSite = -1;
  dragonId_ = -1;
}

void Game::loadMapActors() {
  clearNonPlayer();
  if (inside) {
    auto& killed = killedSlots[mapKey()];
    int lvl = subSite >= 0 ? world.sites[subSite].level : 1;
    bool cleared = subSite >= 0 && world.sites[subSite].cleared;
    // M0b: a tower's mage sleeps on the top floor at night; nobody else stands in the room you rented
    const Bldg* B = subBldg >= 0 ? &world.over.bldgs[subBldg] : nullptr;
    const bool night = hour >= 22.0f || hour < 6.0f;
    const bool mageUp = B && B->type == art::Building::Tower && B->floors() >= 2 && night;
    // the mage is one person on two floors: killed on either, gone from both
    auto slainOn = [&](int key) { auto it = killedSlots.find(key); return it != killedSlots.end() && it->second.count(0) > 0; };
    const bool mageDead = B && B->type == art::Building::Tower && (slainOn(100000 + subBldg) || slainOn(10000000 + subBldg * 16 + B->floors() - 1));
    for (const Spawn& sp : sub.spawns) {
      if (killed.count(sp.slot)) continue;
      if ((mageUp || mageDead) && subFloor == 0 && sp.slot == 0) continue;
      if (sp.npc && B && lodgingActive() && lodging.bldg == subBldg && lodging.floor == subFloor && sub.roomIndexAt(sp.x, sp.y) == lodging.room) continue;
      Vec2 p(sp.x * TILE + 8.0f, sp.y * TILE + 10.0f);
      if (sp.npc) { spawnHuman(sp, p); continue; }
      if (cleared && !sp.boss && hashf(sp.x, sp.y, (uint32_t)day) < 0.6f) continue;   // cleared dungeons are mostly empty
      if (cleared && sp.boss) continue;
      int id = spawnMonster(sp.mon, p, lvl + (sp.boss ? 2 : 0), sp.boss);
      Actor& a = actors[findActor(id)];
      a.fromMap = true; a.slot = sp.slot; a.site = -1;
      if (sp.boss && subSite >= 0 && world.sites[subSite].mainQuest) a.name = "DRAUGR WARLORD";
    }
    // M1: a capital's palace always has its king on the throne-hall floor (the ground floor), flanked by the royal
    // guard. The interior generator places them (TOWNS lane); until it does, or if it ever leaves them out, the king
    // holds court at the head of the hall.
    if (B && B->type == art::Building::Palace && subFloor == 0) {
      bool king = false;
      int guards = 0;
      for (size_t k = 1; k < actors.size(); k++) {
        if (actors[k].role == Role::King) king = true;
        if (actors[k].role == Role::Guard) guards++;
      }
      if (!king) {
        // the head of the hall: the free tile nearest the top middle (away from the door at the bottom)
        int bx = sub.w / 2, by = 2, best = -1, bd = 1 << 30;
        for (int y = 1; y < sub.h - 1; y++)
          for (int x = 1; x < sub.w - 1; x++) {
            if (sub.blocked(x, y) || sub.blocked(x - 1, y) || sub.blocked(x + 1, y)) continue;
            int d = std::abs(x - bx) * 2 + std::abs(y - by) * 3;
            if (d < bd) { bd = d; best = y * sub.w + x; }
          }
        if (best >= 0) {
          int kx = best % sub.w, ky = best / sub.w;
          Spawn ks; ks.npc = true; ks.role = Role::King; ks.slot = 900; ks.site = B->site; ks.x = kx; ks.y = ky;
          spawnHuman(ks, Vec2(kx * TILE + 8.0f, ky * TILE + 10.0f));
          for (int g = 0; g < 2 && guards < 2; g++) {
            int gx = kx + (g == 0 ? -2 : 2), gy = ky + 1;
            if (!sub.in(gx, gy) || sub.blocked(gx, gy)) { gx = kx + (g == 0 ? -1 : 1); gy = ky + 1; }
            if (!sub.in(gx, gy) || sub.blocked(gx, gy)) continue;
            Spawn gs; gs.npc = true; gs.role = Role::Guard; gs.slot = 901 + g; gs.site = B->site; gs.x = gx; gs.y = gy;
            spawnHuman(gs, Vec2(gx * TILE + 8.0f, gy * TILE + 10.0f));
            guards++;
          }
        }
      }
    }
    if (mageUp && !mageDead && subFloor == B->floors() - 1) {
      // the mage's own spawn (slot 0 of the ground floor: same name and face), beside the bed
      Map g0;
      genInterior(g0, *B, B->seed, 0);
      for (const Spawn& sp : g0.spawns) {
        if (sp.slot != 0 || !sp.npc) continue;
        int bx = sub.w / 2, by = sub.h / 2;
        for (const RoomInfo& R : sub.rooms) if (R.bedX >= 0) { bx = R.bedX; by = R.bedY; break; }
        static const int dx[5] = {0, 1, -1, 0, 2}, dy[5] = {1, 0, 0, 2, 1};
        int px = sub.down.valid() ? sub.down.ax : sub.w / 2, py = sub.down.valid() ? sub.down.ay : sub.h / 2;
        for (int k = 0; k < 5; k++)
          if (sub.in(bx + dx[k], by + dy[k]) && !sub.blocked(bx + dx[k], by + dy[k])) { px = bx + dx[k]; py = by + dy[k]; break; }
        spawnHuman(sp, Vec2(px * TILE + 8.0f, py * TILE + 10.0f));
      }
    }
  }
}

// M1 (SIM lane): a site's people stream in and out by distance from the player instead of all at once (a city of
// 160-220 homes has hundreds of spawns): within FOLK_IN tiles they appear (nearest first, at most FOLK_CAP townsfolk
// at a time), beyond FOLK_OUT the calm ones are put away. Hostile camps come whole (their fight is one encounter).
void Game::streamSitePeople(int si) {
  const Site& st = world.sites[(size_t)si];
  auto it = world.siteSpawns.find(si);
  if (it == world.siteSpawns.end()) return;
  const Actor& p = pl();
  const float ptx = p.p.x / TILE, pty = p.p.y / TILE;
  // who of this site is out already (in play or sheltering indoors)
  std::vector<uint8_t> present;
  int folk = 0;
  auto mark = [&](const Actor& a) {
    if (!a.fromMap || a.wild || a.site != si || a.slot < 0) return;
    if ((size_t)a.slot >= present.size()) present.resize((size_t)a.slot + 1, 0);
    present[(size_t)a.slot] = 1;
  };
  for (size_t k = 1; k < actors.size(); k++) { mark(actors[k]); if (actors[k].npc && actors[k].fromMap && actors[k].role != Role::Guard) folk++; }
  for (const Actor& a : sheltered_) { mark(a); if (a.role != Role::Guard) folk++; }
  auto& killed = killedSlots[0];
  const bool camp = st.type == SiteType::BanditCamp;
  // a small place (a village, a town of the classic size, a camp) comes out whole while it is active; a big one
  // streams. The watch always turns out across a wider ring (they answer the bell from across town).
  const bool whole = camp || it->second.size() <= 48;
  const int guardIn = 2 * FOLK_IN, guardOut = 2 * FOLK_OUT;
  // put the far calm ones away (never someone fighting, fleeing, talking or hostile and roused)
  for (size_t k = 1; k < actors.size();) {
    Actor& a = actors[k];
    bool mine = a.fromMap && !a.wild && a.site == si && a.st != AState::Dead;
    if (mine && !whole) {
      float dx = a.p.x / TILE - ptx, dy = a.p.y / TILE - pty;
      const int out = a.role == Role::Guard ? guardOut : FOLK_OUT;
      bool far = dx * dx + dy * dy > (float)(out * out);
      bool busy = a.aggro || a.target >= 0 || a.fleeT > 0 || a.fleeing || (mode == Mode::Dialogue && dlg.actor == a.id) ||
                  (a.hostile && a.aggro);
      if (far && !busy) {
        if (a.npc && a.role != Role::Guard) folk--;
        perf.despawnedNpcs++;
        actors.erase(actors.begin() + (std::ptrdiff_t)k);
        continue;
      }
    }
    k++;
  }
  // bring in the near ones, nearest first
  std::vector<std::pair<float, int>> want;
  for (int idx : it->second) {
    if (idx < 0 || idx >= (int)world.over.spawns.size()) continue;
    const Spawn& sp = world.over.spawns[(size_t)idx];
    if (sp.site != si) continue;
    if (sp.slot >= 0 && (size_t)sp.slot < present.size() && present[(size_t)sp.slot]) continue;
    if (killed.count(owKillKey(si, sp.slot))) continue;
    if (camp && st.cleared) continue;
    if (!world.over.in(sp.x, sp.y)) continue;   // endless: the far side of a big town lies outside the window
    float dx = sp.x + 0.5f - ptx, dy = sp.y + 0.5f - pty;
    float d2 = dx * dx + dy * dy;
    const int in = sp.role == Role::Guard ? guardIn : FOLK_IN;
    if (!whole && d2 > (float)(in * in)) continue;
    want.push_back({d2, idx});
  }
  std::sort(want.begin(), want.end());
  for (auto& w : want) {
    const Spawn& sp = world.over.spawns[(size_t)w.second];
    const bool counts = sp.npc && sp.role != Role::Guard;
    if (counts && !whole && folk >= FOLK_CAP) continue;
    spawnHuman(sp, Vec2(sp.x * TILE + 8.0f, sp.y * TILE + 10.0f));
    if (sp.npc) perf.spawnedNpcs++;
    if (counts) folk++;
  }
}

void Game::updateSpawning(float dt) {
  Actor& p = pl();
  int ptx = (int)std::floor(p.p.x / TILE), pty = (int)std::floor(p.p.y / TILE);
  // activate settlement and camp populations near the player (distance to the site's area); an active site streams
  // its people in and out around the player (streamSitePeople)
  siteScanT_ -= dt;
  const bool scan = siteScanT_ <= 0;
  if (scan) siteScanT_ = 0.25f;
  auto visit = [&](int si) {
    const Site& st = world.sites[(size_t)si];
    if (!st.settlement() && st.type != SiteType::BanditCamp && world.siteSpawns.find(si) == world.siteSpawns.end()) return;
    int ddx = std::max({st.r.x - ptx, ptx - (st.r.x + st.r.w - 1), 0});
    int ddy = std::max({st.r.y - pty, pty - (st.r.y + st.r.h - 1), 0});
    int d2 = ddx * ddx + ddy * ddy;
    bool active = activeSites_.count(si) > 0;
    if (!active && d2 < SITE_IN * SITE_IN) {
      activeSites_.insert(si);
      streamSitePeople(si);
    } else if (active && d2 > SITE_OUT * SITE_OUT) {
      activeSites_.erase(si);
      for (size_t i = 1; i < actors.size();)
        if (actors[i].site == si && !actors[i].wild && actors[i].fromMap) actors.erase(actors.begin() + (std::ptrdiff_t)i); else i++;
      for (size_t i = 0; i < sheltered_.size();)
        if (sheltered_[i].site == si) sheltered_.erase(sheltered_.begin() + (std::ptrdiff_t)i); else i++;
      alarms_.erase(si);
    } else if (active && scan) streamSitePeople(si);
  };
  if (world.endless) {
    const std::vector<int> near = world.nearSites;   // a copy: streaming people never adds sites, but stay safe
    for (int si : near) visit(si);
    // an active site that left the near list (a teleport) is put away whole
    for (auto itA = activeSites_.begin(); itA != activeSites_.end();) {
      int si = *itA;
      if (std::find(near.begin(), near.end(), si) != near.end()) { ++itA; continue; }
      itA = activeSites_.erase(itA);
      for (size_t i = 1; i < actors.size();)
        if (actors[i].site == si && !actors[i].wild && actors[i].fromMap) actors.erase(actors.begin() + (std::ptrdiff_t)i); else i++;
      for (size_t i = 0; i < sheltered_.size();)
        if (sheltered_[i].site == si) sheltered_.erase(sheltered_.begin() + (std::ptrdiff_t)i); else i++;
      alarms_.erase(si);
    }
  } else {
    for (int si = 0; si < (int)world.sites.size(); si++) visit(si);
  }
  perf.activeSites = (int)activeSites_.size();
  // the dragon waits at its peak once the hunt is on
  const Quest* mq = nullptr;
  for (auto& q : quests) if (q.type == QType::Main) mq = &q;
  if (mq && mq->stage == 3 && dragonId_ < 0 && world.lair >= 0) {
    const Site& L = world.sites[world.lair];
    int dx = L.ex - ptx, dy = L.ey - pty;
    if (dx * dx + dy * dy < 30 * 30) {
      dragonId_ = spawnMonster(Monster::Dragon, Vec2(L.ex * TILE + 8.0f, (L.ey - 6) * TILE + 0.0f), std::max(plLevel + 2, 12), true);
      // the dragon belongs to its lair: killing it clears the lair, which ends the main quest
      actors[findActor(dragonId_)].site = world.lair;
      sfx((int)Sfx::Roar, pl().p);
      emit(Ev::Shake, pl().p, 0, 6);
      say("ASHFANG DESCENDS FROM THE PEAK!");
    }
  }
  if (noWildSpawns) return;
  // the opening (shirt only, VISION_PLAN 15.1): no roaming beasts around the start village until the first weapon
  if (eqWeapon < 0 && !hasFlag(SF_FIRST_WEAPON) && openingQuest() >= 0) {
    const Site& home = world.sites[world.startSite];
    int dx = home.r.cx() - ptx, dy = home.r.cy() - pty;
    if (dx * dx + dy * dy < 40 * 40) return;
  }
  // packs live in dens (WORLDGEN_V2+): they appear at their den when you come near and go home when they lose you
  bool haveDens = !world.dens.empty();
  if (haveDens) {
    denT_ -= dt;
    if (denT_ <= 0) { denT_ = 0.5f; updateDens(ptx, pty); }
  }
  // wildlife and roaming monsters: with dens these are lone wanderers, fewer of them
  spawnT_ -= dt;
  if (spawnT_ > 0) return;
  spawnT_ = haveDens ? 3.5f : 1.5f;
  int nearby = 0;
  for (const Actor& a : actors) if (a.wild && a.st != AState::Dead) nearby++;
  int cap = haveDens ? (isNight() ? 4 : 2) : (isNight() ? 9 : 6);
  if (nearby >= cap) return;
  for (int tries = 0; tries < 12; tries++) {
    float ang = rng_.f() * TAU;
    float d = rng_.range(17.0f, 24.0f) * TILE;
    Vec2 sp = p.p + Vec2(std::cos(ang) * d, std::sin(ang) * d * 0.7f);
    int tx = (int)(sp.x / TILE), ty = (int)(sp.y / TILE);
    if (world.over.blocked(tx, ty) || world.siteAt(tx, ty, 8) >= 0) continue;
    Ground g = world.over.at(tx, ty);
    if (g == Ground::Road || g == Ground::Bridge) continue;
    Biome b = world.over.biomeAt(tx, ty);
    bool night = isNight();
    Monster m;
    float q = rng_.f();
    switch (b) {
      case Biome::Plains: m = night && q < 0.35f ? Monster::Skeleton : (q < 0.4f ? Monster::Wolf : q < 0.65f ? Monster::Boar : q < 0.85f ? Monster::Slime : Monster::Goblin); break;
      case Biome::Forest: m = q < 0.4f ? Monster::Wolf : q < 0.6f ? Monster::Spider : q < 0.8f ? Monster::Boar : Monster::Bear; break;
      case Biome::Autumn: m = q < 0.35f ? Monster::Boar : q < 0.6f ? Monster::Spider : q < 0.8f ? Monster::Goblin : Monster::Bear; break;
      case Biome::Taiga: m = q < 0.5f ? Monster::Wolf : q < 0.8f ? Monster::Bear : Monster::Troll; break;
      case Biome::Snow: m = q < 0.5f ? Monster::IceWolf : q < 0.8f ? Monster::FrostSpider : Monster::Troll; break;
      case Biome::Swamp: m = night && q < 0.3f ? Monster::Wraith : (q < 0.4f ? Monster::Slime : q < 0.7f ? Monster::Mudcrab : Monster::Spider); break;
      case Biome::Desert: m = night && q < 0.4f ? Monster::Skeleton : (q < 0.5f ? Monster::Sandworm : Monster::Goblin); break;
      case Biome::Beach: m = Monster::Mudcrab; break;
      default: continue;
    }
    int lvl = world.zoneLevel(tx, ty);
    int pack = (m == Monster::Wolf || m == Monster::IceWolf || m == Monster::Goblin || m == Monster::Slime) ? 1 + rng_.irange(3) : 1;
    if (lvl <= 2 && pack > 2) pack = 2;
    if (haveDens) {
      pack = m == Monster::Slime ? 1 + rng_.irange(2) : 1;
      if ((m == Monster::Troll || m == Monster::Bear) && lvl <= 3) continue;   // big brutes live in dens near home
    }
    for (int k = 0; k < pack; k++) {
      Vec2 at = sp + Vec2(rng_.range(-12, 12), rng_.range(-12, 12));
      if (solidAt(at.x, at.y, false)) at = sp;
      int id = spawnMonster(m, at, lvl, false);
      actors[findActor(id)].wild = true;
    }
    return;
  }
}

int Game::denClearedDay(int den) const {
  auto it = killedSlots.find(-1);
  if (it == killedSlots.end()) return -1;
  for (int v : it->second) if (v / 4096 == den) return v % 4096;
  return -1;
}

void Game::updateDens(int ptx, int pty) {
  // endless worlds: only the dens near the window (an active den that left it is put away below)
  std::vector<int> all;
  const std::vector<int>* list = &world.nearDens;
  if (!world.endless) { all.resize(world.dens.size()); for (size_t i = 0; i < all.size(); i++) all[i] = (int)i; list = &all; }
  else
    for (int i : std::vector<int>(activeDens_.begin(), activeDens_.end()))
      if (std::find(world.nearDens.begin(), world.nearDens.end(), i) == world.nearDens.end()) {
        activeDens_.erase(i);
        for (size_t k = 1; k < actors.size();)
          if (actors[k].den == i) actors.erase(actors.begin() + (std::ptrdiff_t)k); else k++;
      }
  for (int i : *list) {
    const Den& dn = world.dens[i];
    int dx = dn.x - ptx, dy = dn.y - pty;
    int d2 = dx * dx + dy * dy;
    bool active = activeDens_.count(i) > 0;
    if (!active && d2 < 27 * 27) {
      int cd = denClearedDay(i);
      if (cd >= 0) {
        if (day - cd < 3) continue;   // cleared dens stay empty for three days, then something moves back in
        for (int v : std::vector<int>(killedSlots[-1].begin(), killedSlots[-1].end())) if (v / 4096 == i) killedSlots[-1].erase(v);
      }
      activeDens_.insert(i);
      int lvl = world.zoneLevel(dn.x, dn.y);
      int n = dn.pack;
      bool packKind = dn.mon == Monster::Wolf || dn.mon == Monster::IceWolf || dn.mon == Monster::Goblin || dn.mon == Monster::Skeleton;
      if (packKind && lvl <= 3) n = std::min(n, lvl <= 1 ? 2 : 3);       // gentler packs close to home
      if (packKind && lvl > 3 && isNight() && dn.mon != Monster::Goblin) n++;   // and bigger ones out in the dark
      Rng r(hash2(dn.x, dn.y, (uint32_t)seed ^ (uint32_t)day));
      for (int k = 0; k < n; k++) {
        float a = k * TAU / n + r.f();
        int tx = dn.x + (int)std::lround(std::cos(a) * 1.5f), ty = dn.y + (int)std::lround(std::sin(a) * 1.2f);
        Vec2 at(tx * TILE + 8.0f, ty * TILE + 10.0f);
        if (!bodyFree(at, mstat(dn.mon).radius, false)) at = Vec2(dn.x * TILE + 8.0f, dn.y * TILE + 10.0f);
        if (!bodyFree(at, mstat(dn.mon).radius, false)) at = freeSpot(dn.x, dn.y);
        int id = spawnMonster(dn.mon, at, lvl, false);
        Actor& m = actors[findActor(id)];
        m.den = i;
        m.home = Vec2(dn.x * TILE + 8.0f, dn.y * TILE + 10.0f);
        m.goal = m.p;
      }
    } else if (active && d2 > 38 * 38) {
      // out of range: the den resets (a pack you only thinned is whole again next time)
      activeDens_.erase(i);
      for (size_t k = 1; k < actors.size();)
        if (actors[k].den == i) actors.erase(actors.begin() + k); else k++;
    }
  }
}

void Game::updateLocation() {
  Actor& p = pl();
  int tx = (int)(p.p.x / TILE), ty = (int)(p.p.y / TILE);
  if (inside) {
    if (subBldg >= 0) {
      const Bldg& b = world.over.bldgs[subBldg];
      // M0b: upper floors drop the town's name so the label fits the HUD ("MAGE TOWER - TOP FLOOR")
      bool top = subFloor > 0 && subFloor == b.floors() - 1 && b.floors() >= 3;
      if (subFloor > 0) locName = std::string(bldgTypeName(b.type)) + (top ? " - TOP FLOOR" : " - UPSTAIRS");
      else locName = (b.site >= 0 ? world.sites[b.site].name + " - " : std::string()) + bldgTypeName(b.type);
    } else if (subSite >= 0) locName = world.sites[subSite].name;
    return;
  }
  int si = world.siteAt(tx, ty, 2);
  if (si != curSite) {
    curSite = si;
    if (si >= 0) {
      Site& st = world.sites[si];
      if (!st.discovered) {
        st.discovered = true;
        emit(Ev::Discover, p.p, si, 1, st.name);
        sfx((int)Sfx::Discover, p.p);
        gainXp(10);
        if (background == Background::Marked && (st.type == SiteType::Ruin || st.type == SiteType::DragonLair))   // marked one: a hook into M9
          emit(Ev::Notice, p.p, (int)rgba(170, 140, 255), 0, "THE MARK ON YOUR WRIST GROWS WARM");
      }
      if (st.type == SiteType::City || st.type == SiteType::Town || st.type == SiteType::Village) lastTown = si;
    }
  }
  if (curSite >= 0) locName = world.sites[curSite].name;
  else locName = biomeName(world.over.biomeAt(tx, ty));
}

// ------------------------------------------------------------------ maps
void Game::enterSite(int si) {
  Site& st = world.sites[si];
  st.discovered = true;
  inside = true; subSite = si; subBldg = -1; subFloor = 0;
  if (st.type == SiteType::Ruin) genRuin(sub, st, st.seed); else genCave(sub, st, st.seed);
  // apply looted chests
  for (int i = 0; i < sub.w * sub.h; i++)
    if (sub.prop[(size_t)i] == (int)Prop::Chest + 1 && looted.count(((uint64_t)mapKey() << 32) | (uint32_t)i)) sub.prop[(size_t)i] = (int)Prop::ChestOpen + 1;
  sub.rebuildSolid();
  placePlayerAt(sub.exitX, sub.exitY - 1);
  pl().face = 1;
  exitArmed_ = false;
  loadMapActors();
  emit(Ev::MapChange, pl().p);
  sfx((int)Sfx::Door, pl().p, 0.7f);
  say(st.name);
}

void Game::enterBuilding(int bi) {
  const Bldg& b = world.over.bldgs[bi];
  inside = true; subSite = -1; subBldg = bi; subFloor = 0;
  genInterior(sub, b, b.seed, 0);
  for (int i = 0; i < sub.w * sub.h; i++)
    if (sub.prop[(size_t)i] == (int)Prop::Chest + 1 && looted.count(((uint64_t)mapKey() << 32) | (uint32_t)i)) sub.prop[(size_t)i] = (int)Prop::ChestOpen + 1;
  sub.rebuildSolid();
  placePlayerAt(sub.exitX, sub.exitY - 1);
  pl().face = 1;
  exitArmed_ = false;
  stairsArmed_ = true;
  stairsLatch_ = false;
  stairsNorth_ = false;
  stairsArrive_ = -1;
  loadMapActors();
  emit(Ev::MapChange, pl().p);
  sfx((int)Sfx::Door, pl().p);
}

// M0b: another floor of the current building. The new floor's map is generated like any interior (looted chests
// re-applied by its own mapKey); the player arrives beside the stairs that lead back where they came from.
void Game::changeFloor(int f) {
  if (!inside || subBldg < 0) return;
  const Bldg& b = world.over.bldgs[subBldg];
  if (f < 0 || f >= b.floors() || f == subFloor) return;
  bool up = f > subFloor;
  clearNonPlayer();
  subFloor = f;
  genInterior(sub, b, b.seed, f);
  for (int i = 0; i < sub.w * sub.h; i++)
    if (sub.prop[(size_t)i] == (int)Prop::Chest + 1 && looted.count(((uint64_t)mapKey() << 32) | (uint32_t)i)) sub.prop[(size_t)i] = (int)Prop::ChestOpen + 1;
  sub.rebuildSolid();
  const Stairs& via = up ? sub.down : sub.up;   // going up you arrive at the head of the stairs down, and vice versa
  if (via.valid()) placePlayerAt(via.ax, via.ay);
  else placePlayerAt(sub.w / 2, sub.h / 2);
  // face away from the stairs you came by: beside the stairwell, look along the floor away from it; below, south
  if (via.valid() && via.ax != via.x) pl().face = via.ax > via.x ? 2 : 3;
  else pl().face = 0;
  stairsArrive_ = up && via.valid() ? via.ay * sub.w + via.ax : -1;   // only a climb leaves the stairwell asleep
  stairsOff_ = pl().p;
  exitArmed_ = false;
  stairsArmed_ = false;
  stairsLatch_ = true;
  stairsNorth_ = false;
  stairsFrom_ = pl().p;
  loadMapActors();
  updateLocation();
  emit(Ev::MapChange, pl().p);
  sfx((int)Sfx::Door, pl().p, up ? 1.15f : 0.9f);
}

bool Game::debugEnterBuilding(int bi, int f) {
  if (bi < 0 || bi >= (int)world.over.bldgs.size()) return false;
  if (f < 0 || f >= world.over.bldgs[bi].floors()) return false;
  if (inside) leaveSub();
  enterBuilding(bi);
  if (f > 0) changeFloor(f);
  sleepFade = 0;
  return subFloor == f;
}

void Game::leaveSub() {
  int bi = subBldg, si = subSite;
  inside = false; subBldg = -1; subSite = -1; subFloor = 0;
  sub = Map();
  clearNonPlayer();
  if (bi >= 0) {
    const Bldg& b = world.over.bldgs[bi];
    pl().p = Vec2(b.doorX() * TILE + 8.0f, (b.r.y + b.r.h) * TILE + 10.0f);
  } else if (si >= 0) {
    const Site& st = world.sites[si];
    pl().p = Vec2(st.ex * TILE + 8.0f, (st.ey + 1) * TILE + 10.0f);
  }
  // endless: a door far off the window's middle (a building entered from afar by a script, a test or a save made in a
  // distant inn) brings the window along, so the player steps out onto real ground
  if (world.endless && (bi >= 0 || si >= 0)) {
    int tx = (int)std::floor(pl().p.x / TILE), ty = (int)std::floor(pl().p.y / TILE);
    const int lo = World::WIN_SHIFT, hi = World::WIN - World::WIN_SHIFT;
    if (tx < lo || ty < lo || tx >= hi || ty >= hi) placePlayerAt(tx, ty);   // (the door's tile: free, so exact)
  }
  pl().face = 0;
  emit(Ev::MapChange, pl().p);
  sfx((int)Sfx::Door, pl().p, 0.8f);
  updateLocation();
}

void Game::teleportGlobal(int32_t gx, int32_t gy) {
  if (inside) leaveSub();
  clearNonPlayer();
  placePlayerAt(gx - world.ox, gy - world.oy);
  sleepFade = 0;
  updateLocation();
  emit(Ev::MapChange, pl().p);
}

void Game::debugSpawn(Monster m, int n, float dist) {
  for (int i = 0; i < n; i++) {
    float a = i * TAU / n + 0.3f;
    Vec2 at = pl().p + Vec2(std::cos(a) * dist, std::sin(a) * dist * 0.7f);
    // never inside rock or water (the whole body, or it can't move): walk the ring inward until it fits
    const MStat& ms = mstat(m);
    for (float k = 1.0f; k > 0.15f && !bodyFree(at, ms.radius, ms.flying); k -= 0.1f)
      at = pl().p + Vec2(std::cos(a) * dist * k, std::sin(a) * dist * 0.7f * k);
    if (!bodyFree(at, ms.radius, ms.flying)) { int tx = (int)(at.x / TILE), ty = (int)(at.y / TILE); at = freeSpot(tx, ty); }
    int id = spawnMonster(m, at, 1, false);
    actors[findActor(id)].aggro = true;
  }
}

int Game::debugSpawnAt(Monster m, Vec2 at, int level) {
  int id = spawnMonster(m, at, level, false);
  actors[findActor(id)].aggro = true;
  return id;
}
