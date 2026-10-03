// EMBERVALE core simulation: player control, movement/collision, combat, AI, spawning, maps.
#include "rpg/sim/game.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "engine/audio.h"

using art::Monster;
using art::Prop;

const char* spellName(Spell s) {
  static const char* n[] = {"FIREBOLT", "MEND", "FROST LANCE"};
  return n[(int)s];
}
int spellCost(Spell s) {
  static const int c[] = {16, 28, 24};
  return c[(int)s];
}

namespace {
struct MStat { float hp, dmg, speed, range, aggro, radius, windup; int xp; bool ranged, flying; };
const MStat& mstat(Monster m) {
  static const MStat t[] = {
      {34, 13, 66, 13, 120, 5, 0.32f, 12, false, false},   // Wolf
      {36, 8, 56, 13, 90, 6, 0.40f, 14, false, false},     // Boar
      {95, 15, 46, 17, 100, 8, 0.50f, 38, false, false},   // Bear
      {22, 4, 30, 11, 80, 5, 0.45f, 6, false, false},      // Slime
      {34, 7, 58, 13, 110, 6, 0.34f, 15, true, false},     // Spider
      {14, 3, 76, 10, 120, 4, 0.25f, 5, false, true},      // Bat
      {35, 8, 40, 15, 120, 5, 0.42f, 18, false, false},    // Skeleton
      {70, 12, 38, 17, 120, 6, 0.48f, 30, false, false},   // Draugr
      {26, 6, 60, 12, 120, 5, 0.30f, 10, false, false},    // Goblin
      {105, 17, 44, 19, 110, 9, 0.55f, 60, false, false},  // Troll
      {60, 10, 48, 15, 140, 6, 0.40f, 36, true, true},     // Wraith
      {22, 5, 30, 11, 60, 6, 0.40f, 6, false, false},      // Mudcrab
      {46, 9, 66, 13, 130, 5, 0.30f, 20, false, false},    // IceWolf
      {62, 10, 52, 15, 120, 7, 0.36f, 30, true, false},    // FrostSpider
      {85, 14, 40, 18, 100, 8, 0.50f, 42, false, false},   // Sandworm
      {1500, 30, 72, 34, 320, 18, 0.60f, 1500, true, true},// Dragon
  };
  return t[(int)m];
}
const char* monsterName(Monster m) {
  static const char* n[] = {"WOLF", "BOAR", "CAVE BEAR", "SLIME", "GIANT SPIDER", "BAT", "SKELETON", "DRAUGR", "GOBLIN", "TROLL",
                            "WRAITH", "MUDCRAB", "ICE WOLF", "RIME SPIDER", "SANDWORM", "ASHFANG THE DRAGON"};
  return n[(int)m];
}
Vec2 faceVec(int f) {
  static const Vec2 v[4] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
  return v[f & 3];
}
int faceOf(Vec2 d) {
  if (std::fabs(d.x) > std::fabs(d.y) * 1.05f) return d.x > 0 ? 2 : 3;
  return d.y > 0 ? 0 : 1;
}
}  // namespace

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
  world.generate(s, genVer);
  inside = false; subSite = -1; subBldg = -1;
  time = 0; hour = 8.5f; day = 1;
  quests.clear(); nextQuestId = 1; trackedQuest = -1;
  npcQuestsDone.clear(); looted.clear(); killedSlots.clear(); shopCache_.clear();
  projs.clear(); pickups.clear(); events.clear();
  kills = 0; dungeonsCleared = 0; blessT = 0;
  actors.clear();
  Actor p;
  p.id = nextId_++;
  p.player = true; p.human = true; p.name = "YOU";
  actors.push_back(p);
  resetPlayer();
  const Site& home = world.sites[world.startSite];
  lastTown = world.startSite;
  placePlayerAt(home.r.cx(), home.r.cy() + 1);
  // main quest
  Quest q;
  q.id = nextQuestId++;
  q.type = QType::Main;
  q.title = "THE DRAGON'S SHADOW";
  const Site& cap = world.sites[world.capital];
  q.giverSite = world.capital;
  q.target = world.capital;
  q.desc = "A DRAGON HAS BEEN SEEN OVER THE PEAKS. JARL OF " + cap.name + " SEEKS ANYONE BRAVE ENOUGH TO HELP. TRAVEL TO " + cap.name + " AND SPEAK WITH THE JARL IN THE KEEP.";
  q.stage = 0;
  quests.push_back(q);
  trackedQuest = q.id;
  loadMapActors();
  updateLocation();
}

void Game::resetPlayer() {
  plLevel = 1; plXp = 0; gold = 30; perkPts = 0;
  baseHp = 100; maxMp = 60; maxSt = 80;
  inv.clear();
  eqWeapon = eqBow = eqStaff = eqArmor = eqHelmet = eqShield = eqRing = eqAmulet = -1;
  Rng r(seed);
  Item sw = makeWeapon(r, 1, (int)WeaponType::Sword, false);
  sw.name = "IRON SWORD"; sw.power = 8;
  inv.push_back(sw); eqWeapon = 0;
  Item bow; bow.kind = ItemKind::Bow; bow.power = 9; bow.name = "HUNTING BOW"; bow.icon = art::Icon::Bow; bow.tint = tierTint(0); bow.value = 40;
  inv.push_back(bow); eqBow = 1;
  inv.push_back(makeArrows(20));
  Item pot = makePotion(PotionType::Health, 0); pot.count = 3;
  inv.push_back(pot);
  Item bread = makeFood(0); bread.count = 2;
  inv.push_back(bread);
  spellsKnown = 1; spell = Spell::Flames;
  recalcPlayer();
  Actor& p = pl();
  p.hp = p.maxHp; mp = maxMp; stamina = maxSt;
  p.st = AState::Idle;
}

void Game::recalcPlayer() {
  Actor& p = pl();
  float bonusHp = 0, bonusMp = 0, bonusSt = 0;
  for (int idx : {eqArmor, eqHelmet, eqShield, eqRing, eqAmulet}) {
    if (idx < 0 || idx >= (int)inv.size()) continue;
    const Item& it = inv[idx];
    if (it.ench == Ench::Health) bonusHp += it.enchPow;
    if (it.ench == Ench::Magicka) bonusMp += it.enchPow;
    if (it.ench == Ench::Stamina) bonusSt += it.enchPow;
  }
  float oldMax = p.maxHp;
  p.maxHp = baseHp + bonusHp;
  if (oldMax > 0 && p.hp > p.maxHp) p.hp = p.maxHp;
  (void)bonusMp; (void)bonusSt;
  p.armor = armorRating();
  p.radius = 4.5f;
  p.speed = 74;
  // appearance follows equipment
  art::HumanLook& L = p.look;
  L.skin = rgba(236, 188, 146);
  L.hairColor = rgba(120, 70, 36);
  L.hair = art::Hair::Short;
  L.topColor = rgba(70, 110, 150);
  L.bottomColor = rgba(78, 60, 44);
  L.tabardColor = rgba(170, 50, 40);
  L.outfit = art::Outfit::Tunic;
  if (eqArmor >= 0) {
    static const art::Outfit byTier[] = {art::Outfit::Leather, art::Outfit::Plate, art::Outfit::Elven, art::Outfit::Elven, art::Outfit::Ebony, art::Outfit::Ebony};
    const Item& a = inv[eqArmor];
    L.outfit = a.name.rfind("IRON", 0) == 0 ? art::Outfit::Chain : byTier[a.tier];
  }
  L.helmet = eqHelmet >= 0;
  L.shield = eqShield >= 0;
  L.cape = plLevel >= 5;
  L.weapon = 1;
  if (eqWeapon >= 0) {
    static const uint8_t wmap[] = {1, 2, 6, 5, 1};
    L.weapon = wmap[inv[eqWeapon].sub % 5];
    L.weaponColor = inv[eqWeapon].tint ? inv[eqWeapon].tint : rgba(200, 205, 215);
  }
}

float Game::armorRating() const {
  float a = 0;
  for (int idx : {eqArmor, eqHelmet, eqShield}) if (idx >= 0 && idx < (int)inv.size()) a += inv[idx].power;
  for (int idx : {eqArmor, eqHelmet, eqShield, eqRing, eqAmulet})
    if (idx >= 0 && idx < (int)inv.size() && inv[idx].ench == Ench::Fortify) a += inv[idx].enchPow * 0.5f;
  if (blessT > 0) a += 15;
  return a;
}
float Game::weaponDamage() const {
  float d = eqWeapon >= 0 ? inv[eqWeapon].power : 4;
  return d * (1.0f + (plLevel - 1) * 0.04f);
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
  pl().p = freeSpot(tx, ty);
  pl().vel = Vec2();
  pl().knock = Vec2();
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
  for (size_t i = 1; i < actors.size(); i++) updateActor(actors[i], dt);
  // separation between bodies
  for (size_t i = 0; i < actors.size(); i++)
    for (size_t j = i + 1; j < actors.size(); j++) {
      Actor& a = actors[i]; Actor& b = actors[j];
      if (a.st == AState::Dead || b.st == AState::Dead || a.fly || b.fly) continue;
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
  if (!busy) stamina = stamina + dt * 22.0f;
  float mpMax = maxMp;
  for (int idx : {eqArmor, eqHelmet, eqShield, eqRing, eqAmulet}) if (idx >= 0 && inv[idx].ench == Ench::Magicka) mpMax += inv[idx].enchPow;
  float stMax = maxSt;
  for (int idx : {eqArmor, eqHelmet, eqShield, eqRing, eqAmulet}) if (idx >= 0 && inv[idx].ench == Ench::Stamina) stMax += inv[idx].enchPow;
  stamina = std::min(stamina, stMax);
  if (!busy) stamina = std::min(stMax, stamina + dt * 4.0f);
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

  Vec2 mv = in.move;
  float ml = len(mv);
  if (ml > 1) { mv = mv * (1.0f / ml); ml = 1; }
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
      if (p.stT >= 0.07f) { p.st = AState::Strike; p.stT = 0; p.hitDone = false; sfx((int)Sfx::Swing, p.p, 1.0f + p.combo * 0.12f); }
      break;
    case AState::Strike:
      if (!p.hitDone) { meleeHit(p); p.hitDone = true; }
      moveActor(p, p.aim * (dt * (p.combo == 2 ? 70.0f : 30.0f)));
      if (p.stT >= 0.11f) { p.st = AState::Recover; p.stT = 0; }
      break;
    case AState::Recover:
      if (p.stT >= (p.combo == 2 ? 0.28f : 0.13f)) { p.st = AState::Idle; p.stT = 0; }
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
  if (canAct && in.attack) {
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
    float cost = p.combo == 2 ? 14.0f : 4.0f;
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
  Ench ench = Ench::None; float ep = 0;
  if (isPl && eqWeapon >= 0) { ench = inv[eqWeapon].ench; ep = inv[eqWeapon].enchPow; }
  if (isPl && inv.size() && eqWeapon >= 0 && inv[eqWeapon].sub == (uint8_t)WeaponType::Dagger) reach -= 3;
  Vec2 origin = a.p + Vec2(0, -4);
  bool any = false;
  for (size_t i = 0; i < actors.size(); i++) {
    Actor& v = actors[i];
    if (v.id == a.id || v.st == AState::Dead) continue;
    if (v.fly) continue;   // can't reach a dragon in the air
    bool enemy = isPl ? v.hostile : (a.hostile ? (v.player || (v.npc && v.role == Role::Guard)) : v.hostile);
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
  if (v.npc && v.role != Role::Guard && !v.hostile) return;
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
    if ((v.mon == Monster::Wolf || v.mon == Monster::IceWolf) && !v.human && v.hp > 0 && v.hp < v.maxHp * 0.4f) {
      lastWolf = true;
      for (const Actor& o : actors) if (o.id != v.id && o.mon == v.mon && !o.human && o.hostile && o.st != AState::Dead && len2(o.p - v.p) < 160 * 160) lastWolf = false;
    }
    if (((v.mon == Monster::Goblin && v.hp < v.maxHp * 0.3f) || lastWolf) && !v.human && !v.fleeing && v.hp > 0) {
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
  if (killer == pl().id || a.hostile) {
    if (a.xp > 0) gainXp(a.xp);
    kills++;
  }
  if (a.fromMap) killedSlots[mapKey()].insert(a.slot + (a.site >= 0 && !inside ? a.site * 1000 : 0));
  if (a.den >= 0 && !inside && a.den < (int)world.dens.size()) {
    bool left = false;
    for (const Actor& o : actors) if (o.id != a.id && o.den == a.den && o.st != AState::Dead) left = true;
    if (!left && denClearedDay(a.den) < 0) {
      // the den is cleared: a small reward, and it stays empty for a few days (killedSlots[-1] = den * 4096 + day)
      killedSlots[-1].insert(a.den * 4096 + std::min(day, 4095));
      int bonus = 12 + world.zoneLevel(world.dens[a.den].x, world.dens[a.den].y) * 4;
      emit(Ev::Notice, a.p, (int)rgba(255, 210, 90), 0, "DEN CLEARED  +" + std::to_string(bonus) + " XP");
      sfx((int)Sfx::QuestDone, a.p, 1.15f);
      gainXp(bonus);
    }
  }
  dropLoot(a);
  questKill(a);
  if (a.boss) {
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
        bool enemy = pr.fromPlayer ? a.hostile : (a.player || (a.npc && a.role == Role::Guard));
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

void Game::updateAI(Actor& a, float dt) {
  Actor& p = pl();
  float slow = a.slowT > 0 ? 0.55f : 1.0f;
  if (a.st == AState::Hurt) { if (a.stT > 0.25f) { a.st = AState::Idle; a.stT = 0; } return; }

  // ---- friendly NPCs
  if (!a.hostile) {
    bool talking = mode == Mode::Dialogue && dlg.actor == a.id;
    // guards engage hostiles nearby
    if (a.role == Role::Guard) {
      a.thinkT -= dt;
      if (a.thinkT <= 0) {
        a.thinkT = 0.4f;
        a.target = -1;
        float bd = 110 * 110;
        for (auto& e : actors) if (e.hostile && e.st != AState::Dead && !e.fly && len2(e.p - a.p) < bd) { bd = len2(e.p - a.p); a.target = e.id; }
      }
      int ti = a.target >= 0 ? findActor(a.target) : -1;
      if (ti >= 0 && actors[ti].st != AState::Dead) {
        Actor& t = actors[ti];
        Vec2 d = t.p - a.p; float l = len(d);
        a.aim = norm(d); a.face = faceOf(d);
        if (a.st == AState::Windup) { if (a.stT > 0.3f) { a.st = AState::Strike; a.stT = 0; meleeHit(a); sfx((int)Sfx::Swing, a.p); } return; }
        if (a.st == AState::Strike) { if (a.stT > 0.25f) { a.st = AState::Idle; a.stT = 0; a.atkCd = 0.9f; } return; }
        if (l > a.range) { moveActor(a, a.aim * (a.speed * 1.2f * dt)); a.st = AState::Walk; }
        else if (a.atkCd <= 0) { a.st = AState::Windup; a.stT = 0; }
        return;
      }
    }
    if (talking) { a.face = faceOf(p.p - a.p); a.st = AState::Idle; return; }
    // townsfolk run from monsters on the loose instead of strolling past them
    if (a.role != Role::Guard && !inside) {
      const Actor* threat = nullptr; float td = 90.0f * 90.0f;
      for (const Actor& e : actors) if (e.hostile && e.st != AState::Dead && !e.fly && len2(e.p - a.p) < td) { td = len2(e.p - a.p); threat = &e; }
      if (threat) {
        Vec2 away = norm(a.p - threat->p);
        moveActor(a, away * (a.speed * 1.15f * dt));
        a.face = faceOf(away); a.st = AState::Walk;
        a.goal = a.p; a.thinkT = 1.0f;   // when it's over, stay put a moment before wandering again
        return;
      }
    }
    a.thinkT -= dt;
    if (a.thinkT <= 0) {
      a.thinkT = 2.0f + rng_.f() * 4.0f;
      if (rng_.f() < 0.45f) a.goal = a.p;
      else {
        float range = inside ? 3.0f : 6.0f;
        a.goal = a.home + Vec2(rng_.range(-range, range) * TILE, rng_.range(-range * 0.4f, range * 0.4f) * TILE);
      }
    }
    Vec2 d = a.goal - a.p;
    float l = len(d);
    if (l > 3) {
      Vec2 before = a.p;
      moveActor(a, d * (1.0f / l) * (a.speed * 0.45f * dt));
      a.face = faceOf(d);
      a.st = AState::Walk;
      if (len2(a.p - before) < 0.0004f) a.goal = a.p;
    } else a.st = AState::Idle;
    return;
  }

  // ---- hostiles
  Vec2 toP = p.p - a.p;
  float dist = len(toP);
  a.thinkT -= dt;
  if (a.thinkT <= 0) {
    a.thinkT = 0.25f;
    if (p.st != AState::Dead && !a.fleeing && dist < a.aggroR * (isNight() && !inside ? 0.8f : 1.0f)) a.aggro = true;
    // packs are tied to their den: past the leash (or once the player is long gone) they give up and go home
    float leash = a.den >= 0 ? 18.0f * TILE : 28.0f * TILE;
    if (len(a.p - a.home) > leash && dist > 6 * TILE && !a.boss) a.aggro = false;
    if (!a.boss && !inside && dist > std::max(a.aggroR * 2.4f, 15.0f * TILE)) a.aggro = false;
    if (p.st == AState::Dead) a.aggro = false;
    if (!a.aggro) a.fleeing = false;
    if (!a.aggro && a.st != AState::Windup) {
      if (rng_.f() < 0.25f) {
        float r = a.wild ? 5.0f : 3.0f;
        a.goal = a.home + Vec2(rng_.range(-r, r) * TILE, rng_.range(-r, r) * TILE);
      }
    }
  }

  // dragon: its own dance
  if (a.mon == Monster::Dragon) {
    a.special -= dt;
    if (!a.aggro) { if (dist < 260) a.aggro = true; else return; }
    a.aim = norm(toP);
    a.face = toP.x >= 0 ? 2 : 3;
    if (a.fly) {
      // circle the player and rain fire
      float ang = std::atan2(a.p.y - p.p.y, a.p.x - p.p.x) + dt * 0.9f;
      Vec2 want = p.p + Vec2(std::cos(ang), std::sin(ang)) * 90.0f;
      Vec2 d = want - a.p;
      a.p += d * std::min(1.0f, dt * 1.8f);
      a.st = AState::Walk;
      a.shootCd -= 0;
      if (a.shootCd <= 0) {
        a.shootCd = 1.6f;
        for (int k = -1; k <= 1; k++) {
          Projectile pr;
          pr.kind = ProjKind::DragonFire; pr.fromPlayer = false; pr.owner = a.id;
          Vec2 aim = norm(p.p + Vec2(k * 14.0f, 0) - (a.p + Vec2(0, -28)));
          pr.p = a.p + Vec2(0, -28); pr.v = aim * 150; pr.dmg = a.dmg * 0.8f; pr.life = 0.7f + (rng_.f() * 0.3f); pr.radius = 6;
          // fire lands on the ground near the player: let it travel to the target point
          pr.life = std::min(1.4f, len(p.p - pr.p) / 150.0f);
          projs.push_back(pr);
        }
        sfx((int)Sfx::Fireball, a.p, 0.6f);
      }
      if (a.special <= 0) { a.fly = false; a.special = 7.0f; sfx((int)Sfx::Roar, a.p); emit(Ev::Shake, a.p, 0, 5); a.st = AState::Idle; a.stT = 0; }
    } else {
      if (a.special <= 0) { a.fly = true; a.special = 9.0f; sfx((int)Sfx::Roar, a.p, 1.1f); return; }
      if (a.st == AState::Windup) {
        if (a.stT > a.windup) { a.st = AState::Strike; a.stT = 0; meleeHit(a); sfx((int)Sfx::HitHeavy, a.p, 0.6f); emit(Ev::Shake, a.p, 0, 4); }
        return;
      }
      if (a.st == AState::Strike) { if (a.stT > 0.4f) { a.st = AState::Idle; a.stT = 0; a.atkCd = 1.2f; } return; }
      if (dist > a.range + 4) { moveActor(a, a.aim * (a.speed * 0.6f * dt)); a.st = AState::Walk; }
      else if (a.atkCd <= 0) { a.st = AState::Windup; a.stT = 0; }
      // breath cone
      if (a.shootCd <= 0 && dist < 120) {
        a.shootCd = 2.6f;
        for (int k = 0; k < 5; k++) {
          Projectile pr; pr.kind = ProjKind::DragonFire; pr.owner = a.id; pr.radius = 5;
          float ang = std::atan2(a.aim.y, a.aim.x) + (k - 2) * 0.18f;
          pr.p = a.p + Vec2(a.aim.x * 20, -14); pr.v = Vec2(std::cos(ang), std::sin(ang)) * 170; pr.dmg = a.dmg * 0.6f; pr.life = 0.6f;
          projs.push_back(pr);
        }
        sfx((int)Sfx::Fireball, a.p, 0.5f);
      }
    }
    return;
  }

  // behaviour sets: wolves circle and lunge from the flank, goblins swarm and flee when hurt, bandit archers
  // kite, bears and trolls mix in a slow heavy slam you roll through
  const bool wolf = a.mon == Monster::Wolf || a.mon == Monster::IceWolf;
  const bool goblin = a.mon == Monster::Goblin;
  const bool brute = a.mon == Monster::Bear || a.mon == Monster::Troll;
  const bool archer = a.ranged && a.human;
  const float wu = a.heavy ? (a.mon == Monster::Troll ? 0.9f : 0.8f) : a.windup;
  switch (a.st) {
    case AState::Windup:
      a.face = faceOf(toP);
      if (!a.lunge) a.aim = norm(toP);
      if (a.heavy && a.stT < wu - 0.2f) a.aim = norm(toP);   // tracks you, then commits
      if (a.stT >= wu) {
        a.st = AState::Strike; a.stT = 0; a.atkN++; a.special = 0;
        if (a.ranged && dist > a.range + 6) {
          Projectile pr;
          pr.owner = a.id; pr.fromPlayer = false; pr.dmg = a.dmg;
          Vec2 aim = norm(p.p + Vec2(0, -6) - (a.p + Vec2(0, -8)));
          pr.p = a.p + Vec2(0, -8) + aim * 5;
          if (a.human) { pr.kind = ProjKind::Arrow; pr.v = aim * 210; pr.life = 1.0f; sfx((int)Sfx::Arrow, a.p, 0.9f); }
          else if (a.mon == Monster::Wraith) { pr.kind = ProjKind::Magic; pr.v = aim * 130; pr.life = 1.6f; pr.ench = Ench::Frost; pr.enchPow = 4; sfx((int)Sfx::Frost, a.p, 0.8f); }
          else { pr.kind = ProjKind::Spit; pr.v = aim * 150; pr.life = 1.0f; sfx((int)Sfx::Splash, a.p, 1.4f); }
          projs.push_back(pr);
          a.atkCd = 1.6f + rng_.f();
        } else if (a.heavy) {
          heavySlam(a);
          a.vel = a.aim * 30.0f;
          a.atkCd = 1.5f + rng_.f() * 0.6f;
        } else if (a.lunge) {
          a.aim = norm(toP);
          a.vel = a.aim * 235.0f;
          a.hitDone = false;
          a.atkCd = 0.8f + rng_.f() * 0.6f;
          sfx((int)Sfx::Swing, a.p, 0.7f);
        } else {
          meleeHit(a);
          a.atkCd = 1.0f + rng_.f() * 0.7f;
          if (wolf || a.mon == Monster::Boar || goblin) a.vel = a.aim * 120.0f;
          else a.vel = a.aim * 40.0f;
          sfx((int)Sfx::Swing, a.p, 0.8f);
        }
      }
      return;
    case AState::Strike:
      if (a.lunge && !a.hitDone && len2(a.vel) > 1) {   // a lunge bends a little toward a dodging target
        float sp = len(a.vel);
        a.vel = norm(norm(a.vel) + norm(toP) * std::min(1.0f, dt * 4.0f)) * sp;
      }
      moveActor(a, a.vel * dt);
      a.vel *= std::pow(0.01f, dt);
      if (a.lunge && !a.hitDone && len(p.p - a.p) < a.range + p.radius + 2) {   // the bite lands on contact
        a.aim = norm(p.p - a.p);
        damage(p, a.dmg * (0.9f + rng_.f() * 0.2f), a.p, a.id);
        a.hitDone = true;
        a.vel *= 0.25f;
      }
      if (a.stT >= (a.heavy ? 0.4f : 0.28f)) { a.st = AState::Recover; a.stT = 0; }
      return;
    case AState::Recover:
      if (a.stT >= (a.heavy ? 0.75f : 0.35f)) { a.st = AState::Idle; a.stT = 0; a.heavy = false; a.lunge = false; }
      return;
    default: break;
  }

  if (a.aggro && p.st != AState::Dead) {
    a.aim = norm(toP);
    a.face = faceOf(toP);
    Vec2 side(-a.aim.y, a.aim.x);
    float want = a.ranged ? 70.0f : a.range * 0.8f;
    float spd = 1.0f;
    Vec2 mv;
    if (a.fleeing) {
      // run, weaving a little; once well away, give up the fight
      mv = a.aim * -1.0f + side * (std::sin(a.animT * 3 + a.id) * 0.5f);
      spd = 1.1f;
      if (dist > 11 * TILE) { a.aggro = false; a.fleeing = false; a.goal = a.home; }
    } else if (wolf) {
      // circle at a few strides, drifting in and out; the lunge comes from wherever the player isn't looking
      float orbitR = 38.0f + (a.id % 3) * 5.0f;
      float radial = clampf((dist - orbitR) / 14.0f, -1.0f, 1.0f);
      mv = a.aim * radial + side * (0.95f * a.orbitDir);
      spd = 0.85f;
      if (rng_.f() < dt * 0.35f) a.orbitDir = (int8_t)-a.orbitDir;
      a.special += dt;   // time spent circling
    } else if (goblin) {
      // swarm: each goblin closes in from its own side so the pack surrounds you
      float spread = (float)((int)(a.id % 3) - 1) * 0.75f;
      if (dist > want) mv = a.aim + side * (dist > 28 ? spread : 0.0f);
      spd = 1.05f;
    } else if (archer) {
      // keep a bow-shot away: back off when crowded, close in when far, strafe in between
      if (dist < 62) mv = a.aim * -1.0f + side * (0.5f * a.orbitDir);
      else if (dist > 115) mv = a.aim;
      else mv = side * (0.6f * a.orbitDir);
      if (rng_.f() < dt * 0.4f) a.orbitDir = (int8_t)-a.orbitDir;
    } else if (dist > want) mv = a.aim;
    if (a.mon == Monster::Bat || a.mon == Monster::Wraith) {   // erratic flight
      float w = std::sin(a.animT * 5 + a.id) * 0.8f;
      mv = mv + side * w;
    }
    // simple obstacle avoidance: if stuck, sidestep
    Vec2 before = a.p;
    if (len2(mv) > 0.01f) {
      moveActor(a, norm(mv) * (a.speed * spd * slow * dt));
      a.st = AState::Walk;
      if (len2(a.p - before) < 0.02f * a.speed * dt) {
        Vec2 sd = side * ((a.id & 1) ? 1.0f : -1.0f);
        moveActor(a, sd * (a.speed * slow * dt));
        if (wolf) a.orbitDir = (int8_t)-a.orbitDir;
        if (a.fleeing && dist < 34) a.fleeing = false;   // cornered: fight
      }
    } else a.st = AState::Idle;
    if (a.fleeing) return;
    bool inMelee = dist < a.range + p.radius + 2;
    bool inShot = a.ranged && dist < 150 && dist > 30;
    if (a.atkCd > 0) return;
    if (wolf) {
      // the pack takes turns (two at once in a big pack); prefer the flank, but don't circle forever
      int busy = 0, mates = 0;
      for (const Actor& o : actors) {
        if (o.id == a.id || o.st == AState::Dead || o.mon != a.mon || len2(o.p - p.p) > 140 * 140) continue;
        mates++;
        if (o.lunge && o.st == AState::Windup) busy++;
      }
      int slots = mates >= 2 ? 2 : 1;
      bool flank = dot(norm(a.p - p.p), p.aim) < 0.3f;
      if (busy < slots && dist > 16 && dist < 48 && (flank || a.special > 0.7f)) {
        a.st = AState::Windup; a.stT = 0; a.lunge = true;
      } else if (inMelee && dist <= 16 && busy < slots) {
        a.st = AState::Windup; a.stT = 0; a.lunge = false;
      }
      return;
    }
    if (brute) {
      bool heavyNow = (a.atkN % 3 == 2) || rng_.f() < 0.15f;
      if (inMelee || (heavyNow && dist < a.range + p.radius + 12)) { a.st = AState::Windup; a.stT = 0; a.heavy = heavyNow; }
      return;
    }
    if (inMelee || (inShot && rng_.f() < dt * 2.5f)) { a.st = AState::Windup; a.stT = 0; }
  } else {
    Vec2 d = a.goal - a.p;
    float l = len(d);
    // a pack that lost its prey trots back to the den
    float back = (a.den >= 0 && len2(a.p - a.home) > 5.0f * TILE * 5.0f * TILE) ? 0.7f : 0.35f;
    if (back > 0.5f) { d = a.home - a.p; l = len(d); }
    if (l > 4) { moveActor(a, d * (1.0f / l) * (a.speed * back * slow * dt)); a.face = faceOf(d); a.st = AState::Walk; }
    else a.st = AState::Idle;
  }
}

void Game::heavySlam(Actor& a) {
  // a ground slam in front of the brute: big damage and knockback, no cone check. Rolling through it is the answer.
  Vec2 c = a.p + a.aim * 10.0f;
  float r = 26.0f + a.radius;
  for (Actor& v : actors) {
    if (v.id == a.id || v.st == AState::Dead || v.fly) continue;
    bool enemy = v.player || (v.npc && v.role == Role::Guard);
    if (!enemy) continue;
    if (len2(v.p - c) > (r + v.radius) * (r + v.radius)) continue;
    damage(v, a.dmg * 2.1f * (0.9f + rng_.f() * 0.2f), a.p, a.id);
    if (v.player && v.st != AState::Roll && v.iframes <= 0.36f) v.knock = norm(v.p - a.p) * 190.0f;
  }
  emit(Ev::Shake, c, 0, 4.5f);
  for (int k = 0; k < 6; k++) emit(Ev::Dust, c + Vec2(std::cos(k * 1.047f) * r * 0.6f, std::sin(k * 1.047f) * r * 0.4f));
  sfx((int)Sfx::HitHeavy, a.p, 0.55f);
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
    case Role::Priest: L.outfit = art::Outfit::Robe; L.topColor = rgba(220, 200, 150); L.tabardColor = rgba(200, 160, 60); a.name = "PRIEST " + a.name; break;
    case Role::Mage: L.outfit = art::Outfit::Robe; L.topColor = rgba(70, 60, 140); L.tabardColor = rgba(200, 180, 80); L.hood = rr.f() < 0.5f; L.weapon = 4; break;
    case Role::Smith: L.outfit = art::Outfit::Leather; L.weapon = 6; L.beard = !female; break;
    case Role::Innkeeper: L.outfit = female ? art::Outfit::Dress : art::Outfit::Tunic; L.topColor = rgba(170, 140, 100); break;
    case Role::Merchant: L.topColor = rgba(150, 70, 110); break;
    case Role::Farmer: L.topColor = rgba(140, 120, 70); L.weapon = 0; break;
    case Role::Bandit:
      L.outfit = rr.f() < 0.6f ? art::Outfit::Leather : art::Outfit::Rags; L.hood = rr.f() < 0.5f; L.tabardColor = rgba(80, 60, 50);
      L.weapon = rr.f() < 0.5f ? 1 : 2;
      break;
    default: break;
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
    a.hostile = true; a.role = Role::Bandit;
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
    if (sp.role == Role::Guard) { a.maxHp = 300; a.hp = 300; a.dmg = 22; a.name = "GUARD"; a.speed = 54; }
    if (sp.role == Role::Child) { a.radius = 3.5f; a.look.outfit = art::Outfit::Tunic; a.look.beard = false; }
    a.level = 10;
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
  dragonId_ = -1;
}

void Game::loadMapActors() {
  clearNonPlayer();
  if (inside) {
    auto& killed = killedSlots[mapKey()];
    int lvl = subSite >= 0 ? world.sites[subSite].level : 1;
    bool cleared = subSite >= 0 && world.sites[subSite].cleared;
    for (const Spawn& sp : sub.spawns) {
      if (killed.count(sp.slot)) continue;
      Vec2 p(sp.x * TILE + 8.0f, sp.y * TILE + 10.0f);
      if (sp.npc) { spawnHuman(sp, p); continue; }
      if (cleared && !sp.boss && hashf(sp.x, sp.y, (uint32_t)day) < 0.6f) continue;   // cleared dungeons are mostly empty
      if (cleared && sp.boss) continue;
      int id = spawnMonster(sp.mon, p, lvl + (sp.boss ? 2 : 0), sp.boss);
      Actor& a = actors[findActor(id)];
      a.fromMap = true; a.slot = sp.slot; a.site = -1;
      if (sp.boss && subSite >= 0 && world.sites[subSite].mainQuest) a.name = "DRAUGR WARLORD";
    }
  }
}

void Game::updateSpawning(float dt) {
  Actor& p = pl();
  int ptx = (int)(p.p.x / TILE), pty = (int)(p.p.y / TILE);
  // activate settlement and camp populations near the player
  for (int si = 0; si < (int)world.sites.size(); si++) {
    const Site& st = world.sites[si];
    int dx = st.r.cx() - ptx, dy = st.r.cy() - pty;
    bool nearSite = dx * dx + dy * dy < 46 * 46;
    bool active = activeSites_.count(si) > 0;
    if (nearSite && !active) {
      activeSites_.insert(si);
      auto& killed = killedSlots[0];
      for (const Spawn& sp : world.over.spawns) {
        if (sp.site != si) continue;
        if (killed.count(sp.slot + si * 1000)) continue;
        if (st.type == SiteType::BanditCamp && st.cleared) continue;
        spawnHuman(sp, Vec2(sp.x * TILE + 8.0f, sp.y * TILE + 10.0f));
      }
    } else if (!nearSite && active && dx * dx + dy * dy > 60 * 60) {
      activeSites_.erase(si);
      for (size_t i = 1; i < actors.size();)
        if (actors[i].site == si && !actors[i].wild && actors[i].fromMap) actors.erase(actors.begin() + i); else i++;
    }
  }
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
  for (int i = 0; i < (int)world.dens.size(); i++) {
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
      static const char* nm[] = {"HOUSE", "HOUSE", "INN", "SMITHY", "GENERAL GOODS", "TEMPLE", "THE KEEP", "MAGE TOWER", "FARMHOUSE", "HUT"};
      locName = (b.site >= 0 ? world.sites[b.site].name + " - " : std::string()) + nm[(int)b.type];
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
  inside = true; subSite = si; subBldg = -1;
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
  inside = true; subSite = -1; subBldg = bi;
  genInterior(sub, b, b.seed);
  for (int i = 0; i < sub.w * sub.h; i++)
    if (sub.prop[(size_t)i] == (int)Prop::Chest + 1 && looted.count(((uint64_t)mapKey() << 32) | (uint32_t)i)) sub.prop[(size_t)i] = (int)Prop::ChestOpen + 1;
  sub.rebuildSolid();
  placePlayerAt(sub.exitX, sub.exitY - 1);
  pl().face = 1;
  exitArmed_ = false;
  loadMapActors();
  emit(Ev::MapChange, pl().p);
  sfx((int)Sfx::Door, pl().p);
}

void Game::leaveSub() {
  int bi = subBldg, si = subSite;
  inside = false; subBldg = -1; subSite = -1;
  sub = Map();
  clearNonPlayer();
  if (bi >= 0) {
    const Bldg& b = world.over.bldgs[bi];
    pl().p = Vec2(b.doorX() * TILE + 8.0f, (b.r.y + b.r.h) * TILE + 10.0f);
  } else if (si >= 0) {
    const Site& st = world.sites[si];
    pl().p = Vec2(st.ex * TILE + 8.0f, (st.ey + 1) * TILE + 10.0f);
  }
  pl().face = 0;
  emit(Ev::MapChange, pl().p);
  sfx((int)Sfx::Door, pl().p, 0.8f);
  updateLocation();
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
