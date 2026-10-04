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
  std::string titleNote;                 // a line under the title menu (e.g. an old save that no longer loads)
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

  // ---- terrain (terrain.cpp)
  // M1: terrain chunks (32 x 32 tiles, 512 px) are keyed by GLOBAL chunk coordinates (an endless window's origin
  // ox >> 5 plus the local chunk), so a window shift keeps every baked chunk. Each bake reads a snapshot of its chunk
  // plus a margin (kBakeMargin tiles) instead of a copy of the whole map, and paints with GLOBAL pixel coordinates, so
  // the noise is the same whichever window a chunk was baked in (no seams between old and new chunks).
  // TMap: a map seen through an origin, looked up in global tiles (classic islands and interiors: origin 0).
  struct TMap {
    const Map* m = nullptr;
    int ox = 0, oy = 0;   // the global tile of m's (0, 0)
    MapKind kind = MapKind::Overworld;
    Ground at(int x, int y) const { return m->at(x - ox, y - oy); }
    Biome biomeAt(int x, int y) const { return m->biomeAt(x - ox, y - oy); }
    uint8_t heightBits(int x, int y) const { return m->heightBits(x - ox, y - oy); }
    int heightAt(int x, int y) const { return m->heightAt(x - ox, y - oy); }
    bool relief() const { return !m->height.empty(); }
  };
  static constexpr int kBakeMargin = 8;
  struct Chunk { Tex tex; uint64_t key = 0; float used = 0; };
  std::vector<Chunk> chunks_;
  int lastMapKey_ = -999;
  // background baking: the worker paints chunk Canvases from per-chunk snapshots
  struct BakeJob {
    std::shared_ptr<const Map> map;   // the chunk and its margin
    int ox = 0, oy = 0;               // the snapshot's origin in global tiles
    int gcx = 0, gcy = 0;             // the chunk in global chunk coordinates
    uint64_t key = 0, mapId = 0;
  };
  struct BakeDone { uint64_t key = 0, mapId = 0; Canvas c; double ms = 0; };
  std::thread worker_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<BakeJob> jobs_;
  std::vector<BakeDone> done_;
  std::vector<uint64_t> pending_;
  bool quit_ = false;
  // the chunk frame drawWorld set for this map: global chunk = local chunk + (chunkOX_, chunkOY_); partial chunks
  // (an endless window's outer ring, whose margin runs off the window) get their own key and are baked again once
  // the window has moved over them
  int chunkOX_ = 0, chunkOY_ = 0;
  bool chunkEndless_ = false;
  void workerLoop();
  void clearChunks();
  uint64_t chunkKeyFor(const Map& m, uint64_t mapId, int cx, int cy) const;
  BakeJob makeJob(const Map& m, uint64_t mapId, int cx, int cy) const;
  void prefetch(const Map& m, uint64_t mapId, Vec2 cam);
  Tex chunkTex(const Map& m, uint64_t mapId, int cx, int cy);
  // the visible chunks not baked yet (a teleport, a new game): painted in parallel right now (desktop), so a jump
  // costs one chunk's bake time instead of one per chunk; chunkTex then finds them ready
  void bakeVisibleNow(const Map& m, uint64_t mapId, int c0x, int c0y, int c1x, int c1y);
  std::vector<std::thread> workers_;   // more background bakers (desktop): worker_ plus these
  void bakeChunk(const BakeJob& j, Canvas& c);
  void bakeRows(const BakeJob& j, Canvas& c, int r0, int r1);
  void bakeFinish(const BakeJob& j, Canvas& c);
  void pumpBake(double budgetMs);
  BakeJob incrJob_{};
  Canvas incrCanvas_;
  int incrRow_ = 0;
  bool incrOn_ = false;
  uint32_t groundPixel(const TMap& m, int px, int py);   // px, py: global pixels
  int rockLevel(const TMap& m, int tx, int ty);
  uint32_t reliefPixel(const TMap& m, int px, int py, uint32_t c, Ground g);   // cliff faces, lips, ramps, grade
  // EMB_PERF: bake time and texture counts, printed once a second
  struct Perf { double bakeMs = 0, worstBakeMs = 0, worstFrameMs = 0, frameMs = 0; int bakes = 0, inlineBakes = 0, frames = 0; float t = 0; };
  Perf perf_;
  void perfTick(float dt);
  // M1 caches for huge cities: building sprites and character sheets are dropped least-recently-used once their
  // count passes a cap (a 220-building city, then the next one); buildings are found through a per-32-tile-cell
  // index of the current map (records are append-only for the session, so never every building per frame)
  std::unordered_map<uint64_t, float> bldgUsed_, humanUsed_;
  float lruT_ = 0;
  void trimCaches();
  std::vector<std::vector<int>> bgrid_;
  int bgridW_ = 0, bgridH_ = 0, bgridKey_ = -999, bgridOX_ = 0, bgridOY_ = 0;
  size_t bgridN_ = 0;
  const Map* bgridMap_ = nullptr;
  std::vector<uint32_t> bstamp_;
  uint32_t bstampN_ = 0;
  // indices of m's buildings whose sprite may show in the rectangle (x0, y0)-(x1, y1) (map pixels)
  void bldgsIn(const Game& g, const Map& m, float x0, float y0, float x1, float y1, std::vector<int>& out);

  // ---- lighting
  Tex lightMap_;
  // ---- minimap
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
  bool kTapAttack_ = false;   // this attack came from a tap on the open world (never turned into a talk)
  struct Finger { uint64_t id = 0; bool on = false; Vec2 start, cur; int button = -1; uint64_t t0 = 0; };   // t0: SDL_GetTicks at the touch
  Finger stick_;
  std::vector<Finger> fingers_;
  bool mouseDown_ = false;
  Vec2 mouse_;
  // ---- menus
  int menuTab_ = 0, menuSel_ = 0, menuScroll_ = 0, dlgSel_ = 0, shopSide_ = 0, shopSel_ = 0, shopArm_ = -1, titleSel_ = 0, levelSel_ = 0;
  float dlgChars_ = 0;
  int mapSel_ = -1;
  float menuOpenT_ = 0;
  Mode lastMode_ = Mode::Title;
  float modeT_ = 0;          // seconds since the game mode last changed (death-screen grace, etc.)

  // ---- M0 lane entry points (declared here once so no lane has to edit this header; each lives in its own file
  //      and keeps its private state as file statics there - there is only one View)
  // creator.cpp (hero lane): the character creator shown in Mode::Creator
  void drawCreator(Game& g);
  void creatorKey(Game& g, int key);       // SDL keycode
  void creatorTap(Game& g, Vec2 p);        // logical coordinates
  // paperdoll.cpp (hero lane): the equipment screen (a menu tab)
  void drawPaperdoll(Game& g, float x, float y, float w, float h);
  void paperdollKey(Game& g, int key);
  void paperdollTap(Game& g, Vec2 p);
  // render_deco.cpp (homes lane): Map::deco floor decorations for the visible tiles, drawn after the terrain
  void drawDeco(const Map& m, Vec2 cam, int tx0, int ty0, int tx1, int ty1);
  // render_markers.cpp (town-defence lane): world-space markers over actors ("!" reward waiting, alarm...)
  void drawMarkers(Game& g, Vec2 cam);
  // a texture cache for lane-painted sprites: key spaces are tagged by the top byte (0x01 deco, 0x02 markers,
  // 0x03 creator/paperdoll, 0x04 architecture); paint(key) is called once per key
  const Tex& cachedTex(uint64_t key, Canvas (*paint)(uint64_t key));
  // M1 kingdom identity: banners (kind 0) and gatehouses (kind 1) in a kingdom's colours, cached per look
  const Tex& kingdomTex(int kind, const Kingdom& k, uint32_t seed);
  std::unordered_map<uint64_t, Tex> kingdomTex_;
  int arriveSite_ = -2;                      // the settlement the arrival banner last announced
  std::unordered_map<uint64_t, Tex> laneTex_;

  // ---- architecture (render.cpp, M0 architecture lane): wall-tile keys for the current map (art::wallKeys), the
  //      wall-tile / gatehouse sprites, and the chimney mouths of each building sprite (for smoke)
  std::vector<uint32_t> wallKeys_;
  uint64_t wallKeysId_ = 0;
  std::unordered_map<uint32_t, Tex> wallTiles_;
  Tex gateTex_;
  std::unordered_map<uint64_t, std::vector<Vec2>> bldgSmoke_;
  std::unordered_map<uint64_t, int> bldgTopRow_;   // first opaque row of each building sprite (fade test)
  std::unordered_map<uint64_t, Tex> bldgNight_;    // the same sprite with its windows lit (night, people awake)
  std::unordered_map<uint64_t, std::vector<Vec2>> bldgWin_;   // window centres in sprite pixels (night light pools)
  bool windowsLit(const Game& g, const Bldg& b) const;       // night, and this household is awake
  float smokeT_ = 0;
  const Map* bldgMap_ = nullptr;   // the map whose buildings drawWorld is drawing (bldgTex reads its biome)
  int bldgPrefetch_ = 0;           // round-robin cursor: buildings near the player are painted ahead, one per frame
  std::vector<uint32_t> wallTodo_; // wall-tile keys of the current map not painted yet (painted ahead, one per frame)
  std::unordered_map<uint64_t, std::vector<uint32_t>> wallKeyCache_;   // per map id, so leaving a house is free
  const Tex& wallTileTex(uint32_t key);
  uint64_t bldgKey(const Map& m, const Bldg& b, int index) const;

  void drawWorld(Game& g);
  void drawLighting(Game& g);
  void drawWeather(Game& g, float dt);
  void drawHud(Game& g);
  void drawTouch(Game& g);
  void drawToasts();
  float dlgRowH() const { return touchUI ? 20.0f : 13.0f; }   // dialogue option pitch (touch: finger-sized rows)
  void drawMenu(Game& g);
  void drawDialogue(Game& g);
  void drawShop(Game& g);
  void shopDeal(Game& g);   // buy or sell the selected row (a worn item asks twice)
  void drawLevelUp(Game& g);
  void drawTitle(Game& g, bool hasSave);
  void drawDead(Game& g);
  void drawMinimap(Game& g, float x, float y, int size);
  // worldmap.cpp (M1): the pannable, zoomable world map of the MAP tab
  void drawWorldMap(Game& g, float x, float y, float w, float h);
  void worldMapZoom(int dz, Vec2 at);          // dz < 0 closer; `at` keeps that box point still (-1: the centre)
  void worldMapCentre(Game& g, int site);      // on a place (-1: the hero)
  bool worldMapKey(Game& g, int key);          // Q E - + zoom, arrows pan, C centre; false: not a map key
  bool worldMapPointer(int phase, uint64_t id, Vec2 p);   // 0 down 1 move 2 up (box coords); true: a tap
  int worldMapPick(Game& g, Vec2 p);           // the discovered place under a box point, or -1
  int worldMapZoomLevel() const;
  void panel(float x, float y, float w, float h, float alpha = 0.92f);
  void button(float x, float y, float w, float h, const std::string& label, bool hot);
  void wrapText(float x, float y, float w, const std::string& s, Color c, int maxChars = -1, int lineH = 9);
  void spawnParticles(const Event& e, Game& g);
  void tap(Game& g, Vec2 p);             // UI tap / click in logical coords
  int buttonAt(Vec2 p) const;            // touch control hit test
  void menuKey(Game& g, int key);
  void titleTap(Vec2 p);
  bool hasSave_ = false;

  // ---- M1 screen fit (screen.h): menus, dialogues and the title are laid out for 480 px of width; on a wider (or
  //      taller) screen they sit in a centred box inside the safe area. drawX pushes the box (Pix::pushBox), and
  //      tap() maps a screen point into the same box.
  struct UiBox { int x = 0, y = 0, w = 480, h = 270; };
  UiBox uiBox(int maxH) const;               // a 480-wide box, up to maxH tall, centred in the safe area
  static Vec2 inBox(const UiBox& b, Vec2 p) { return Vec2(p.x - b.x, p.y - b.y); }
  int titleItems() const;                    // CONTINUE (with a save), NEW ADVENTURE, SETTINGS
  // ---- settings.cpp: the SETTINGS screen (SCREEN mode, BORDER, HUD MARGIN, touch controls), reached from the
  //      title and from the menu's SYSTEM tab; it takes every key and tap while open
  bool settingsOpen_ = false;
  int setSel_ = 0;
  void openSettings() { settingsOpen_ = true; setSel_ = 0; }
  void drawSettings();                       // full screen (draws its own box)
  void settingsKey(int key);
  void settingsTap(Vec2 p);                  // screen coordinates
  void drawSafeGuides();                     // corner marks at the HUD margin (shown while the settings are open)
};
