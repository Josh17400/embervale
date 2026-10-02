// TAILSPIN simulation. Pure C++ (no SDL): runs in the app and in the headless tool.
#pragma once
#include <deque>
#include <vector>
#include "engine/mathx.h"

constexpr float ARENA_W = 4000.0f;
constexpr float ARENA_H = 2250.0f;
constexpr float SIM_DT = 1.0f / 120.0f;
constexpr float TAIL_SPACING = 16.0f;
constexpr int   MIN_LOOP_SEGS = 14;      // ignore crossings of the newest N segments
constexpr float MIN_LOOP_AREA = 6000.0f;
constexpr int   MAX_UPGRADE_LEVEL = 5;

enum class Mode : uint8_t { Title, Play, LevelUp, Dead, Paused };
enum class EType : uint8_t { Drifter, Charger, Splitter, Shooter, Boss };

enum class Up : uint8_t {
  LongTail, BigBlast, BurnTail, Magnet, Swift, TightTurn, QuickBoost,
  Vitality, Lightning, LoopHeal, WideTail, EchoLoop, COUNT
};

struct Enemy {
  Vec2 p, v;
  float hp = 1, maxhp = 1, r = 18;
  EType t = EType::Drifter;
  float timer = 0;      // type-specific timer
  uint8_t state = 0;    // charger: 0 chase 1 telegraph 2 dash 3 rest
  float flash = 0;      // hit flash (render)
  float tailCd = 0;     // per-enemy tail hit cooldown
  float burn = 0;       // remaining burn seconds
  Vec2 dir;             // charger dash direction
  float spawnT = 0;     // fade-in (render)
};
struct Gem { Vec2 p, v; float value = 1; bool magnet = false; };
struct Bullet { Vec2 p, v; float life = 4; };

enum class Ev : uint8_t { Pop, Gem, Loop, Level, Hurt, Boost, Zap, Burn, BossSpawn, BossDie, Dead, Select };
struct Event { Ev t; Vec2 p; float a = 0; int n = 0; };

struct LoopFx { std::vector<Vec2> poly; float t = 0; int kills = 0; Vec2 center; float radius = 0; };
struct ZapFx { Vec2 a, b; float t = 0; };

struct Input {
  Vec2 steer;            // desired direction, magnitude 0..1 (0 = keep heading)
  bool boost = false;    // level-triggered; sim edge-detects
};

struct Stats {
  float time = 0; int kills = 0; int loops = 0; int bestLoop = 0; int level = 1; int bosses = 0;
};

class Game {
 public:
  Game() { reset(1); mode = Mode::Title; }
  void reset(uint64_t seed);
  void update(float dt, const Input& in);   // advances one fixed step while Play
  void pickUpgrade(int idx);
  void setPaused(bool p);
  void startRun() { if (mode == Mode::Title || mode == Mode::Dead) { reset(nextSeed++); mode = Mode::Play; } }

  // --- state (read by renderer) ---
  Mode mode = Mode::Title;
  Stats stats;
  uint64_t nextSeed = 1;
  Vec2 head, prevHead;
  float heading = 0, speed = 0;
  std::deque<Vec2> tail;                 // [0] = newest point behind the head
  float distAcc = 0;
  float hp = 100, maxHp = 100, invuln = 0;
  float boostT = 0, boostCd = 0;
  bool boostHeld = false;
  float xp = 0, xpNeed = 8;
  int level = 1;
  int upLevel[(int)Up::COUNT] = {};
  Up choices[3] = {};
  int numChoices = 0;
  float chainShow = 0; int chainCount = 0;   // "CHAIN xN" readout
  int comboLast = 0;

  std::vector<Enemy> enemies;
  std::vector<Gem> gems;
  std::vector<Bullet> bullets;
  std::vector<LoopFx> loopFx;
  std::vector<ZapFx> zaps;
  std::vector<Event> events;                 // drained by the app each frame

  // derived stats
  int   tailMaxPts() const;
  float tailWidth() const;
  float turnRate() const;
  float cruiseSpeed() const;
  float boostCooldown() const;
  float magnetRadius() const;
  float blastMul() const;
  float loopDamage() const;
  float chainRadius() const;
  bool  bossAlive() const;
  const Enemy* boss() const;

  static const char* upName(Up u);
  static const char* upDesc(Up u);

  // exposed for tests
  int  doLoop(const std::vector<Vec2>& poly, Vec2 center);
  float time() const { return stats.time; }

 private:
  Rng rng{1};
  float spawnAcc = 0;
  float nextRing = 40.0f;
  int bossesSpawned = 0;
  float zapT = 0;
  float echoT = 0; std::vector<Vec2> echoPoly; Vec2 echoCenter;
  int pendingLevels = 0;
  std::vector<Vec2> chainQ;
  int loopKillCount = 0;
  std::vector<std::vector<int>> grid;       // enemy grid scratch
  std::vector<std::vector<int>> tailGrid;
  int gridW = 0, gridH = 0;

  void steer(float dt, const Input& in);
  void moveTail(float dt);
  bool checkLoop();
  void updateEnemies(float dt);
  void updateBullets(float dt);
  void updateGems(float dt);
  void spawnDirector(float dt);
  void spawnEnemy(EType t, Vec2 at, bool ring = false);
  Vec2 spawnPoint(float dist);
  void damageEnemy(int i, float dmg, bool viaLoop);
  void killEnemy(int i, bool viaLoop);
  void reapDead();
  void addXp(float v);
  void hurtPlayer(float dmg);
  void buildGrids();
  void emit(Ev t, Vec2 p, float a = 0, int n = 0) { events.push_back({t, p, a, n}); }
  void rollChoices();
};
