// EMBERVALE presentation: sprite bank, terrain baking, world rendering, lighting, particles, HUD, menus, touch controls.
#pragma once
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "engine/audio.h"
#include "engine/pix.h"
#include "rpg/art.h"
#include "rpg/sim/game.h"

union SDL_Event;

struct Particle {
  Vec2 p, v;
  float life = 1, max = 1, size = 1, grav = 0;
  Color c;
  int kind = 0;      // 0 square, 1 fx sprite
  int fx = 0, layer = 0;
  bool world = true;
};
struct FloatText { Vec2 p; std::string s; Color c; float t = 0; };
struct Toast { std::string s; Color c; float t = 0; };

class View {
 public:
  bool init(Pix& pix, Audio& audio);
  void shutdown();
  // input: feed SDL events; returns true if the event was consumed by UI
  void event(const SDL_Event& e, Game& g);
  Input input(Game& g);                  // per-frame game input built from keyboard/touch state
  void update(Game& g, float dt);        // consumes g.events, animates UI
  void draw(Game& g, bool hasSave);
  void snap(Game& g);                    // snap camera (after load/teleport)
  void openMenu(Game& g, int tab) { g.mode = Mode::Menu; menuTab_ = tab; menuSel_ = 0; }
  bool wantsQuit = false;
  bool wantNewGame = false, wantContinue = false, wantSave = false;
  bool touchUI = false;                  // show on-screen controls
  // test scripts (--script): hold a key as if it were physically down; one-shot presses go through event()
  void scriptHold(int scancode, bool down) { if (scancode >= 0 && scancode < 512) scriptKeys_[scancode] = down; }

 private:
  Pix* pix_ = nullptr;
  Audio* audio_ = nullptr;
  float t_ = 0;
  Vec2 cam_;                 // top-left of the view in world pixels
  float shake_ = 0, titleT_ = 0;
  Vec2 shakeOff_;

  // ---- sprites
  std::unordered_map<uint64_t, Tex> humans_;
  std::vector<Tex> monsters_;
  std::vector<Tex> props_;
  std::unordered_map<uint64_t, Tex> bldgTex_;
  std::vector<Tex> walls_;
  Tex gate_;
  std::unordered_map<uint64_t, Tex> icons_;
  std::vector<Tex> fx_;
  Tex shadow_, shadowBig_, light_, white_, water_, vignette_;
  const Tex& humanTex(const art::HumanLook& L);
  const Tex& iconTex(art::Icon i, uint32_t tint);
  const Tex& bldgTex(const Bldg& b, int index);

  // ---- terrain
  struct Chunk { Tex tex; int cx = -1, cy = -1; uint64_t mapId = 0; float used = 0; };
  std::vector<Chunk> chunks_;
  int lastMapKey_ = -999;
  // background baking: the worker paints chunk Canvases from a snapshot of the current map
  struct BakeJob { std::shared_ptr<const Map> map; uint64_t mapId; int cx, cy; };
  struct BakeDone { uint64_t mapId; int cx, cy; Canvas c; };
  std::shared_ptr<const Map> snap_;
  uint64_t snapId_ = 0;
  std::thread worker_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<BakeJob> jobs_;
  std::vector<BakeDone> done_;
  std::vector<uint64_t> pending_;
  bool quit_ = false;
  void workerLoop();
  void clearChunks();
  void prefetch(const Map& m, uint64_t mapId, Vec2 cam);
  Tex chunkTex(const Map& m, uint64_t mapId, int cx, int cy);
  void bakeChunk(const Map& m, int cx, int cy, Canvas& c);
  void bakeRows(const Map& m, int cx, int cy, Canvas& c, int r0, int r1);
  void bakeFinish(const Map& m, int cx, int cy, Canvas& c);
  void pumpBake(double budgetMs);
  BakeJob incrJob_{};
  Canvas incrCanvas_;
  int incrRow_ = 0;
  bool incrOn_ = false;
  uint32_t groundPixel(const Map& m, int px, int py);
  int rockLevel(const Map& m, int tx, int ty);

  // ---- lighting
  Tex lightMap_;
  // ---- minimap / world map
  Tex worldMap_;
  bool worldMapBaked_ = false;
  std::vector<uint32_t> miniPx_;
  float miniT_ = 0;

  // ---- effects
  std::vector<Particle> parts_;
  std::vector<FloatText> texts_;
  std::vector<Toast> toasts_;
  std::string banner_, bannerSub_;
  float bannerT_ = 0;
  float fade_ = 0;
  Music music_ = Music::Silence;
  float combatT_ = 0;

  // ---- input state
  bool scriptKeys_[512] = {};   // indexed by SDL_Scancode (SDL_SCANCODE_COUNT is 512)
  bool kAttack_ = false, kBow_ = false, kSpell_ = false, kRoll_ = false, kUse_ = false, kPotion_ = false, kSwap_ = false;
  struct Finger { uint64_t id = 0; bool on = false; Vec2 start, cur; int button = -1; };
  Finger stick_;
  std::vector<Finger> fingers_;
  bool mouseDown_ = false;
  Vec2 mouse_;
  // ---- menus
  int menuTab_ = 0, menuSel_ = 0, menuScroll_ = 0, dlgSel_ = 0, shopSide_ = 0, shopSel_ = 0, titleSel_ = 0, levelSel_ = 0;
  float dlgChars_ = 0;
  int mapSel_ = -1;
  float menuOpenT_ = 0;
  Mode lastMode_ = Mode::Title;
  float modeT_ = 0;          // seconds since the game mode last changed (death-screen grace, etc.)

  void drawWorld(Game& g);
  void drawLighting(Game& g);
  void drawWeather(Game& g, float dt);
  void drawHud(Game& g);
  void drawTouch(Game& g);
  void drawMenu(Game& g);
  void drawDialogue(Game& g);
  void drawShop(Game& g);
  void drawLevelUp(Game& g);
  void drawTitle(Game& g, bool hasSave);
  void drawDead(Game& g);
  void drawMinimap(Game& g, float x, float y, int size);
  void drawWorldMap(Game& g, float x, float y, float w, float h);
  void bakeWorldMap(Game& g);
  void panel(float x, float y, float w, float h, float alpha = 0.92f);
  void button(float x, float y, float w, float h, const std::string& label, bool hot);
  void wrapText(float x, float y, float w, const std::string& s, Color c, int maxChars = -1, int lineH = 9);
  void spawnParticles(const Event& e, Game& g);
  void tap(Game& g, Vec2 p);             // UI tap / click in logical coords
  int buttonAt(Vec2 p) const;            // touch control hit test
  void menuKey(Game& g, int key);
  void titleTap(Vec2 p);
  bool hasSave_ = false;
};
