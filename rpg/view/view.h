// EMBERVALE presentation: sprite bank, terrain baking, world rendering, lighting, particles, HUD, menus, touch controls.
#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "engine/audio.h"
#include "engine/pix.h"
#include "rpg/art.h"
#include "rpg/build/parts.h"
#include "rpg/sim/game.h"
#include "rpg/view/loot_paint.h"

union SDL_Event;

struct Particle {
  Vec2 p, v;
  float life = 1, max = 1, size = 1, grav = 0;
  Color c;
  int kind = 0;      // 0 square, 1 fx sprite
  int fx = 0, layer = 0;
  bool world = true;
};
// a floating line over the world: a combat number rising and fading (0.9 s), or (M5) a spoken line (an overheard
// remark, a greeting by name: Ev::Text of words) held over the speaker on a dark plate, wrapped, for `life` seconds
struct FloatText { Vec2 p; std::string s; Color c; float t = 0; bool speech = false; float life = 0.9f; };
struct Toast { std::string s; Color c; float t = 0; int rarity = -1; };   // (M8b) rarity: an item notice (its name in the ladder colour)

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
  // (M7b CITYART, tests) the kerbs within r tiles of the hero: every walk with a carriageway south of it shows a face
  // (its rows below the lit arris darker than the walk's top); returns the walks without one, faces = the walks checked
  int urbKerbCheck(const Game& g, int r, int& faces);
  Vec2 camera() const { return cam_; }   // M2: top-left of the view in world pixels (scripts tap world things)
  void openMenu(Game& g, int tab) { g.mode = Mode::Menu; menuTab_ = tab; menuSel_ = 0; }
  bool wantsQuit = false;
  bool wantNewGame = false, wantContinue = false, wantSave = false;
  bool touchUI = false;                  // show on-screen controls
  std::string titleNote;                 // a line under the title menu (e.g. an old save that no longer loads)
  std::string loadingCard;               // M1: a full-screen card ("FORGING THE WORLD") shown while a world is made
  void creatorPrepare(Game& g);          // (M3 fixer round 2) the creator's homeland choices, made behind the card
  // test scripts (--script): hold a key as if it were physically down; one-shot presses go through event()
  void scriptHold(int scancode, bool down) { if (scancode >= 0 && scancode < 512) scriptKeys_[scancode] = down; }
  // (M3c) test scripts: force a weather kind (rpg/world/biomes.h Sky value, at full strength; -1 back to the land's own)
  void scriptSky(int sky) { skyOverride_ = sky; wxInit_ = false; wxSampleT_ = 0; }
  int scriptSkyNow() const { return skyOverride_; }

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
  const Tex& humanTex(const art::HumanLook& L, bool child = false);   // (M5) child: the body shortened (childBody)
  const Tex& iconTex(art::Icon i, uint32_t tint);
  // M6 Steel: an item's icon in its maker culture's style and metal (rpg/sim/gear_look.h gearIcon -> art::itemIconLook);
  // use it for every Item drawn (the pack, the shop, the paper doll, pickups). iconTex stays for fixed UI icons.
  std::unordered_map<uint64_t, Tex> itemIcons_;
  const Tex& itemTex(const Game& g, const Item& it);
  // M6 Steel: a monster's sheet in its variant look (Actor::overlays / bodyTint: art::monsterSheetLook); the classic
  // sheet (monsters_) when it has none
  std::unordered_map<uint64_t, Tex> monsterLooks_;
  const Tex& monsterTex(const Actor& a);
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
    uint8_t blendAt(int x, int y) const { return m->blendAt(x - ox, y - oy); }   // M2 ecotones (Map::blend)
    uint8_t urbAt(int x, int y) const { return m->urbAt(x - ox, y - oy); }       // M7b the urban fabric (rpg/world/urban.h)
    bool ecoDerive = false;   // M2: the map carries no blend bytes: the view derives ecotones from the biomes
    bool wallAt(int x, int y) const {   // a city wall piece stands on the tile
      const int lx = x - ox, ly = y - oy;
      return !m->wall.empty() && lx >= 0 && ly >= 0 && lx < m->w && ly < m->h && m->wall[(size_t)ly * m->w + lx] != 0;
    }
  };
  static constexpr int kBakeMargin = 8;
  struct Chunk { Tex tex; uint64_t key = 0; float used = 0; bool stale = false; };   // stale: (M7) to be baked again
  std::vector<Chunk> chunks_;
  int lastMapKey_ = -999;
  // background baking: the worker paints chunk Canvases from per-chunk snapshots
  struct BakeJob {
    std::shared_ptr<const Map> map;   // the chunk and its margin
    int ox = 0, oy = 0;               // the snapshot's origin in global tiles
    int gcx = 0, gcy = 0;             // the chunk in global chunk coordinates
    uint64_t key = 0, mapId = 0;
    bool ecoDerive = false;           // M2: no blend bytes in this map (TMap::ecoDerive)
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
  // (M7 fixer r2) the player's houses appear in World::over.bldgs at runtime (home::stampWindow), after the ground
  // around them may already be baked: the chunks under a house that appeared, changed or went are baked again, so it
  // throws the same cast shadow as every generated building (homeHouses_: id, home, seed, global rect last seen)
  struct HomeHouse { uint64_t id = 0; uint8_t home = 0; uint32_t seed = 0; int32_t gx = 0, gy = 0; int w = 0, h = 0; };
  std::vector<HomeHouse> homeHouses_;
  uint64_t homeHousesMap_ = 0;
  void rebakeHomeHouses(const Game& g, const Map& m, uint64_t mapId);
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
  uint32_t reliefPixel(const TMap& m, int px, int py, uint32_t c, Ground g, bool flat = false);   // cliff faces, lips, ramps, grade (flat: the grade alone, smoothed: a city's paving over a walkable step)
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
  // (M5 fixer) the location / quest column's box as the HUD last drew it (screen): spoken lines keep out of it
  float hudColLeft_ = 1e9f, hudColBottom_ = 0;
  float bossBarX0_ = 0, bossBarX1_ = 0;
  float bossBarBottom_ = 0;   // (M6 fixer) the bottom of a world boss's top bar this frame (0: none): banners keep below it
  float quickBarR_ = 0;       // (M9 fixer) the desktop quick-cast bar's right end this frame (0: none drawn)
  // (M5) last frame's NPC name tag / interact prompt (drawHud): a spoken line's plate stacks above it, never over it
  bool tagOn_ = false;
  float tagX0_ = 0, tagY0_ = 0, tagX1_ = 0, tagY1_ = 0;
  std::vector<Toast> toasts_;
  std::string banner_, bannerSub_;
  float bannerT_ = 0;
  float fade_ = 0;
  Music music_ = Music::Silence;
  uint64_t musicStyle_ = 0, musicStyleOn_ = 0;   // M3: the culture's MusicStyle (packed) where the player is / playing
  float musicStyleT_ = 0;
  float combatT_ = 0;
  // (M3c LIFE) the weather of the land round the player (render.cpp drawWeather: what the eco's Sky makes of the day
  // and hour, averaged over a few tiles round the player and eased, so it fades across a biome border), the land's
  // ambient bed and music mood (update), and the first-visit biome banner (hud.cpp)
  struct Wx {
    float rain = 0, pour = 0, drizzle = 0, fog = 0, snow = 0, blizz = 0, sand = 0, shimmer = 0, ash = 0, eerie = 0;
    float er = 0.7f, eg = 0.9f, eb = 1.0f;   // the eerie motes' and haze's colour
  };
  Wx wx_;
  bool wxInit_ = false;
  int skyOverride_ = -1;
  float wxSampleT_ = 0;
  Wx wxWant_;
  float ambLevel_ = 0;
  int hereEco_ = -1;            // the eco under the player (outdoors), -1 unknown
  float ecoT_ = 0;
  std::string biomeBanner_;     // "THE SAVANNA": shown once per biome per adventure (Game::marks, Mk::Biome)
  float biomeBannerT_ = 0;
  int biomeBannerEco_ = -1;     // (M3c fixer) the eco the banner names (-1: none)

  // ---- input state
  bool scriptKeys_[512] = {};   // indexed by SDL_Scancode (SDL_SCANCODE_COUNT is 512)
  bool kAttack_ = false, kBow_ = false, kSpell_ = false, kRoll_ = false, kUse_ = false, kPotion_ = false, kSwap_ = false;
  bool kTapAttack_ = false;   // this attack came from a tap on the open world (never turned into a talk)
  struct Finger { uint64_t id = 0; bool on = false; Vec2 start, cur; int button = -1; uint64_t t0 = 0; bool use = false; };   // t0: SDL_GetTicks at the touch; use: began as a use (TALK, RIDE...)
  Finger stick_;
  std::vector<Finger> fingers_;
  bool mouseDown_ = false;
  Vec2 mouse_;
  // ---- menus
  int menuTab_ = 0, menuSel_ = 0, menuScroll_ = 0, dlgSel_ = 0, dlgFirst_ = 0, shopSide_ = 0, shopSel_ = 0, shopArm_ = -1, titleSel_ = 0, levelSel_ = 0;
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
  std::unordered_map<uint64_t, Tex> wallTiles_;   // (M3b: keyed by wallTexKey: the key and its fort look)
  Tex gateTex_;
  std::unordered_map<uint64_t, std::vector<Vec2>> bldgSmoke_;
  std::unordered_map<uint64_t, int> bldgTopRow_;   // first opaque row of each building sprite (fade test)
  std::unordered_map<uint64_t, Tex> bldgNight_;    // the same sprite with its windows lit (night, people awake)
  // (fixer M5 r3) the panes as a half-size white mask, added into the light map at night: the glass alone is lit (a
  // round pool per window lit the roofs in front of an upper facade, a palace portico at daylight brightness)
  std::unordered_map<uint64_t, Tex> bldgGlow_;
  std::unordered_map<uint64_t, std::vector<Vec2>> bldgWin_;   // window centres in sprite pixels (night light pools)
  bool windowsLit(const Game& g, const Bldg& b) const;       // night, and this household is awake
  float smokeT_ = 0;
  const Map* bldgMap_ = nullptr;   // the map whose buildings drawWorld is drawing (bldgTex reads its biome)
  double fadePaintMs_ = 0;         // (M1 round 3) building paint time spent this frame behind a fade (drawWorld)
  int bldgPrefetch_ = 0;           // round-robin cursor: buildings near the player are painted ahead, one per frame
  std::vector<uint32_t> wallTodo_; // wall-tile keys of the current map not painted yet (painted ahead, one per frame)
  std::unordered_map<uint64_t, std::vector<uint32_t>> wallKeyCache_;   // per map id, so leaving a house is free
  const Tex& wallTileTex(uint32_t key);
  // ---- M3b forts (render.cpp, FORTIFICATIONS lane): each settlement's walls, towers and gate in its culture's parts
  //      (bld::fortParts). A wall key carries a look slot (art::WALL_LOOK_*) into fortLooks_ (slot 0: the style's
  //      defaults), rebuilt with the wall keys per window and cached with them.
  std::vector<bld::FortParts> fortLooks_;
  std::unordered_map<uint64_t, std::vector<bld::FortParts>> fortLookCache_;
  uint64_t wallTexKey(uint32_t key) const;
  // the parts of a settlement's fortifications (false: no culture known, the M3 look)
  bool fortPartsOfSite(const Game& g, int site, bld::FortParts& out) const;
  const Tex& fortGateTex(const cult::Heraldry& arms, const bld::FortParts& f);
  // the culture of the land at a tile (its settlement's, else its region's: World::cultureAtTile), for the wayside's
  // built props (signposts, toll posts, graves, standing stones), cached per 8 x 8-tile block
  const art::PropStyle* landStyleAt(const Game& g, int tx, int ty);
  std::unordered_map<uint64_t, art::PropStyle> landStyle_;
  uint64_t landStyleWorld_ = 0;
  uint64_t bldgKey(const Map& m, const Bldg& b, int index) const;
  // M2: a building sprite painted off the main thread (an arrival's buildings, desktop), then stored as textures
  struct BldgPaint { uint64_t key = 0; Canvas c, night, glow; bool anyGlass = false; std::vector<Vec2> smoke, wins; int topRow = 0; };
  static BldgPaint paintBldg(const Bldg& b, uint64_t key);
  // the facts the view reads off a painted sprite (smoke, top row, the lit-window variant and its window centres)
  static BldgPaint paintBldgPost(Canvas c, art::BuildingInfo& info, uint64_t key, uint32_t seed);
  const Tex& storeBldg(BldgPaint& p);
  std::vector<std::future<BldgPaint>> bldgAsync_;
  std::vector<uint64_t> bldgAsyncKeys_;
  // M3 (owner carry-over 1: the unsplittable ~41 ms palace paint): building sprites painted on the main thread go
  // through art::BuildingJob in steps within a budget (the walk-in prefetch, paints behind a fade, the web's arrival),
  // so a palace is spread over frames. bldgPaintStep: true once the sprite is stored.
  struct BldgJob { uint64_t key = 0; std::shared_ptr<art::BuildingJob> job; uint32_t seed = 0; float used = 0; };
  std::vector<BldgJob> bldgJobs_;
  bool bldgPaintStep(const Bldg& b, int index, double budgetMs);
  Tex blankTex_;                   // drawn for a building still being painted behind the web arrival's fade
  double worstPaintStepMs_ = 0;    // EMB_TIMING: the longest single paint step on the main thread
  // M3 culture-styled props: the PropStyle of the settlement a tile lies in (cached per site), and the styled
  // textures keyed on (prop, style key, variant)
  const art::PropStyle* propStyleAt(const Game& g, int tx, int ty);
  // (M3 fixer) keyed by the settlement's stable id (Site::id), not its index: loading a save in the same session
  // rebuilds World::sites in another order, and an index-keyed cache drew one town's props in another's style
  std::unordered_map<uint64_t, art::PropStyle> siteProps_;   // Site::id -> its culture's props (classic: culture 0)
  std::unordered_map<uint64_t, uint64_t> propSiteBlock_;     // 8 x 8-tile block (global) -> Site::id there (0: none)
  uint64_t sitePropsWorld_ = 0;
  std::unordered_map<uint64_t, Tex> styledProps_;
  const Tex& styledPropTex(art::Prop p, const art::PropStyle& st);
  // M3 heraldry: banners and gatehouses in a kingdom's arms (cult::Heraldry), cached per arms
  const Tex& heraldryTex(int kind, const cult::Heraldry& h, uint32_t seed, int wallStyle);

  void drawWorld(Game& g);
  void drawLighting(Game& g);
  void drawWeather(Game& g, float dt);
  void drawHud(Game& g);
  float biomeBannerY(float half) const;   // (M9 fixer) the region banner's text line: below a boss bar, clear of the quick bar
  void drawTouch(Game& g);
  void drawToasts();
  float dlgRowH() const { return touchUI ? 25.0f : 13.0f; }   // dialogue option pitch (touch: finger-sized rows, 25 px = 33 pt)
  void drawMenu(Game& g);
  void drawDialogue(Game& g);
  void drawShop(Game& g);
  // ---- M6 Steel: the forge screen (Mode::Forge; rpg/view/craft_ui.cpp, NUMBERS lane). forgeSel_ / forgeScroll_ /
  //      forgeTab_ / forgeArm_ are its UI state (free for the lane to use as it likes)
  int forgeSel_ = 0, forgeScroll_ = 0, forgeTab_ = 0, forgeArm_ = -1;
  void drawForge(Game& g);
  void forgeKey(Game& g, int key);
  void forgeTap(Game& g, Vec2 p);
  // ---- M7 Home (rpg/view/home_view.cpp and buildmode.cpp, VIEW lane; the VIEW lane is the only editor of these and
  //      may ADD members here). The world: drawWorld asks homeCollect for the property's draws (yard objects, crops,
  //      ground edits, the building site, the build ghost) each frame and y-sorts them with the scene (Drawable kind 8:
  //      drawHomeDraw blits one); the player on horseback is drawn by drawMounted (true: drawn, the standing figure is
  //      skipped). The screens: Mode::Build (Game::home.ui: the shell, yard, decorate, storage, cooking and buy screens)
  //      by drawBuild / buildKey / buildTap (screen coordinates; the world under the panel is the ghost's canvas).
  struct HomeDraw {
    float sortY = 0;                 // the y it sorts by (world px: the footprint's bottom edge)
    const Tex* tex = nullptr;
    int sx = 0, sy = 0, sw = 0, sh = 0;   // source rect in tex
    float x = 0, y = 0;              // top-left on screen... in WORLD px (drawHomeDraw subtracts the camera)
    bool flip = false;
    Color tint = Color(1, 1, 1, 1);
    uint8_t shadow = 0;              // 0 none, 1 a small round ground shadow under it, 2 a footprint-wide one
    bool flat = false;               // a ground edit (drawn flat: sorts under everything standing on its row)
    // (M7 VIEW lane) added: the ground shadow's ellipse in world px (shadow != 0; drawn in the shadow pass, under
    // everything standing), a crop's sway (px of lean at the top in the wind, 0 none; phase from x), and UI overlays
    // drawn as tinted rects instead of a texture (ui 1: a footprint cell of the build ghost, 2: the plot's border)
    float shx = 0, shy = 0, shw = 0, shh = 0, shA = 0.7f;
    float sway = 0;
    uint8_t ui = 0;
  };
  std::vector<HomeDraw> homeDraws_;
  std::unordered_map<uint64_t, Tex> homeTex_;   // the lane's sprite cache (crops, objects, horses...), keyed as it likes
  void homeCollect(Game& g, const Map& m, Vec2 cam);
  void drawHomeDraw(const HomeDraw& d, Vec2 cam);
  bool drawMounted(Game& g, const Actor& a, Vec2 cam);
  int buildSel_ = 0, buildScroll_ = 0, buildTab_ = 0;   // the Mode::Build screens' UI state (free for the lane)
  void drawBuild(Game& g);
  void buildKey(Game& g, int key);
  void buildTap(Game& g, Vec2 p);
  // ---- M6 Steel: ranked foes in the world (render.cpp, BEASTS lane): the aura sprite cache (art::auraSprite by colour
  //      and width) and the name plates / boss bars it draws
  std::unordered_map<uint64_t, Tex> auraTex_;
  const Tex& auraTex(uint32_t color, int w);
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
  // M2 (worldmap.cpp owns the whole MAP tab: the map, its side column, legend and travel; top = the content's y)
  void drawMapTab(Game& g, float top);
  void mapTabTap(Game& g, Vec2 p, float top);  // a tap in box coordinates (drags and pinches go to worldMapPointer)
  bool mapTabKey(Game& g, int key);            // false: not the map's key (the menu handles it)
  // M2 travel behind the fade (game.h Travel): called by update() every frame while g.travel.phase is Arrive. It snaps
  // the camera to the arrival, bakes the terrain chunks the arrival shows within a per-frame budget (the screen is
  // black meanwhile) and calls g.finishTravel() once they are ready (terrain.cpp).
  void travelArrive(Game& g);
  // M2 (VIEW lane): the terrain chunk frame of the map drawWorld shows (sets chunkOX_ / chunkOY_ / chunkEndless_) and
  // its map id; drawWorld and travelArrive share it. While arriving_ is set, chunkTex never bakes a chunk inline (it
  // returns an empty texture: the screen is black) and drawWorld skips bakeVisibleNow, so no frame stalls.
  uint64_t terrainFrame(Game& g, const Map*& m);
  uint64_t mapIdFor(const Game& g, int key) const;   // the map id terrainFrame gives the map whose Game::mapKey is key
  // (M3c integration, the seat-of-power entry hitch) while the player walks up to a door, Game::preparedInterior holds
  // the interior entering will show: its terrain chunks are queued for the bakers under the interior's map id (prefetch
  // keeps those jobs) and its deco pieces and interior prop pieces are painted into cachedTex a little each frame
  // (render_deco.cpp), so the entry frame finds them ready.
  void prepareInterior(Game& g);
  uint64_t prepMapId_ = 0;   // the interior map id being prepared (0: none)
  const Map* prepMap_ = nullptr;
  size_t prepCursor_ = 0;    // the next tile whose pieces are painted
  bool arriving_ = false;
  struct Arrival { bool on = false; float t = 0; int frames = 0; double ms = 0, wall0 = 0; };
  Arrival arrival_;
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
  UiBox modalBox(const Game& g) const;       // the menu / shop / dialogue box (the MAP tab: wider on wide screens)
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

  // ---- M4 "Banners" (VIEW lane; realm_hud.cpp, realm_render.cpp): the border herald, the news, the arrival banner's
  //      owner and state, the journal's NEWS and LOST HISTORY sections, war props in their owner's colours, the fires
  //      of burned places and the siege camps' light
 public:
  int journalSection() const { return journalSec_; }
  void setJournalSection(int s) { journalSec_ = s < 0 ? 0 : s > 2 ? 2 : s; menuSel_ = 0; menuScroll_ = 0; }
  bool heraldShowing() const { return herald_.t > 0; }
  const std::string& heraldTitle() const { return herald_.title; }
  // (scripts) the last frame's counts of drawn M4 overlays: war props in an owner's colours, burned buildings smoking,
  // siege-camp fires lit
  struct M4Stats { int warProps = 0, warPropsOwned = 0, burned = 0, campFires = 0, mapMarkers = 0, borderPx = 0; };
  M4Stats m4Stats() const { return m4Stats_; }
 private:
  struct Herald { float t = 0; std::string top, title; uint32_t color = 0; ew::Gid kingdom = 0; bool wild = false; };
  Herald herald_;
  M4Stats m4Stats_, m4Count_;
  int journalSec_ = 0;                        // the QUESTS tab: 0 quests, 1 news, 2 lost history
  ew::Gid arriveOwner_ = 0;                   // the owner and state flags the arrival banner last announced
  uint16_t arriveFlags_ = 0;
  std::string bannerState_;                   // a third line under the arrival banner ("OCCUPIED BY QIBA")
  uint32_t bannerStateCol_ = 0;
  std::unordered_map<uint64_t, Tex> armsTex_;
  const Tex& armsTex(const cult::Heraldry& h, int size);
  void heraldEvent(Game& g, const Event& e);
  void newsEvent(Game& g, const Event& e);
  void drawHerald(Game& g);
  // the arrival banner's text for the settlement the player stands in (its trade, its current owner in its society's
  // word, its state); refreshed when the owner or the state changes while the player is there
  void arrivalText(Game& g, int site, std::string& line, std::string& state, uint32_t& stateCol);
  // the QUESTS tab's section strip (QUESTS | NEWS | HISTORY) and the news / lore lists; journalTap: true when used
  float journalStripH() const { return touchUI ? 26.0f : 16.0f; }
  void drawJournalStrip(float lx, float lw, float top);
  bool journalStripTap(Vec2 p, float lx, float lw, float top);
  void drawJournalSection(Game& g, float top);
  bool journalSectionTap(Game& g, Vec2 p, float top);
  // the realm in the world (realm_render.cpp): a war prop's sprite in its camp's colours (nullptr: the plain one), the
  // fire and smoke over burned buildings, the camp fires' light pools at night
  const Tex* warPropTex(Game& g, art::Prop p, int tx, int ty);
  std::unordered_map<uint64_t, Tex> warPropTex_;
  void burnedFx(Game& g, const Map& m, const Bldg& b, int index, float dt);
  float burnT_ = 0;
  struct LightPool { Vec2 p; float r; Color c; float k; };
  void m4Lights(Game& g, const Map& m, Vec2 cam, float dark, std::vector<LightPool>& out);
  void m4Emissive(Game& g, const Map& m, Vec2 cam, float dark);   // (fixer M4 r3) burned shells glowing over the night
  bool nearSiege(Game& g) const;              // the player is in or near a besieged settlement (the Siege music)

  // ---- M5 "Hearth and Hall" (VIEW / AMBIENCE lane: render.cpp, render_markers.cpp, hud.cpp, worldmap.cpp): the
  //      townsfolk's postures, the village animals, speech bubbles, lamps by the lamplighter's round, shuttered stalls
  //      and festival dressing by the settlement's mood, windows lit by occupancy, the mood on the HUD and the map, the
  //      player's buffs, and the life of the place in sound (the bard's tavern piece, the crowd, animals, the bell)
 public:
  // (scripts) the last frame's counts of what M5 drew
  struct M5Stats {
    int posed = 0, poseFallback = 0, critters = 0, bubbles = 0, lampsLit = 0, lampsDark = 0, stallsShut = 0, bunting = 0,
        festLanterns = 0, winLit = 0, winDark = 0, poseBakes = 0, speech = 0;
    int music = 0;            // the Music playing (its enum value)
    float tavernLevel = 0, crowd = 0;
  };
  M5Stats m5Stats() const { return m5Stats_; }
  // (scripts) force a settlement's festival dressing / shutters on or off whatever its census says (-1: its own)
  void scriptLifeLook(int festival, int shuttered) { forceFest_ = festival; forceShut_ = shuttered; }
  // the word and colour of a settlement's mood (life::Life::moodFlags / mood): "CONTENT", "FESTIVAL", "HUNGRY",
  // "FAMINE", "WAR-TORN", "MOURNING"...; empty when its census is not known
  static std::string moodWord(const Game& g, ew::Gid site, uint32_t* color);
 private:
  M5Stats m5Stats_, m5Count_;
  int forceFest_ = -1, forceShut_ = -1;
  // pose sheets (art::humanPostureSheet) by (look key x posture), baked on demand within a per-frame budget (a pose not
  // baked yet is drawn from the standing sheet meanwhile); least-recently-used trimming with the character sheets
  std::unordered_map<uint64_t, Tex> poseTex_;
  std::unordered_map<uint64_t, float> poseUsed_;
  float poseFrameT_ = -1;
  double poseFrameMs_ = 0;
  // variant: the bard's instrument in his people's style (art::humanPostureSheet(look, p, variant); 0 the default)
  const Tex* poseTex(const art::HumanLook& L, art::Posture p, uint8_t variant = 0, bool child = false);   // nullptr: not baked yet
  // a sleeper fitted to a berth (art::sleeperSprite), cached with the pose sheets and baked within the same budget
  const Tex* sleeperTex(const art::HumanLook& L, art::Berth b, int kit, int frame);
  bool poseBudget();         // true: another pose may be baked this frame (and starts its timing)
  void poseSpent(std::chrono::steady_clock::time_point t0);
  std::unordered_map<uint64_t, Tex> critterTex_;
  const Tex& critterTex(int kind, uint32_t variant);
  std::unordered_map<uint64_t, Tex> lampDark_;   // a lamppost by day / before the lamplighter comes (its glass dark)
  const Tex& lampDarkTex(const art::PropStyle* ps);
  std::unordered_map<uint64_t, Tex> festTex_;    // bunting by (width, colours), lanterns by colour
  ew::Gid siteIdAt(const Game& g, int tx, int ty);   // the settlement a tile lies in (cached per 8 x 8-tile block; 0 none)
  uint16_t lifeFlagsAt(const Game& g, int tx, int ty);   // its mood flags (life::MF_*; forced by scriptLifeLook)
  bool festivalHere(const Game& g, ew::Gid site);
  // speech bubbles: when each actor's current bubble appeared (the pop-in) by Actor::id
  struct BubbleSeen { uint8_t kind = 0; float t0 = 0, seen = 0, maxT = 0; };
  std::unordered_map<int, BubbleSeen> bubbleSeen_;
  void drawBubbles(Game& g, Vec2 cam);
  bool buffTap(Game& g, Vec2 p);   // (fixer M5 r3) a tap on the HUD's buff icons explains them (true: taken)
  // (fixer M5 r3) where each human figure was actually drawn this frame (map px: x centre, y the cell's top; t_ when):
  // the bubble rides on the drawn head, whatever posture or seat fit moved it
  struct DrawnHead { float x = 0, y = 0, t = -1; };
  std::unordered_map<int, DrawnHead> drawnHead_;
  // festival: bunting strung between eaves across the streets of a festive settlement (drawn over the scene: it hangs
  // overhead), paper lanterns at doors and stalls lit at night (festLights_: their glow for the light pass)
  std::vector<Vec2> festLights_;
  void drawFestival(Game& g, const Map& m, Vec2 cam);
  // the festival's strings across the plaza: a run of open paving per string, a pole at each end (y-sorted with the
  // scene: Drawable kind 7), planned once per festive settlement and window
  struct FestSpan { int x0 = 0, x1 = 0, y = 0; uint32_t ca = 0, cb = 0, seed = 0; };
  std::vector<FestSpan> festSpans_;
  uint64_t festPlanKey_ = 0;
  void festivalPlan(Game& g, const Map& m, Vec2 cam);
  void drawFestPole(const FestSpan& s, bool right, Vec2 cam);
  // sound: the bard's tavern piece inside the gathering place and faintly outside its door at night, festival music on
  // the plaza, the crowd's murmur, animal voices, the alarm bell tolling through a night raid (update())
  Music lifeMusic(Game& g, Music want, float dt, bool& festive);
  float tollT_ = 0, critterVoiceT_ = 0, cheerT_ = 0;
  std::unordered_map<int, uint8_t> lastPosture_;   // Actor::id -> the posture it had (a toast's cheer, a bard's last chord)
  int bardSite_ = -1, bardBldg_ = -1;               // the gathering place a bard performs in tonight (-1 none)
  float bardScanT_ = 0;
  bool musicFestive_ = false;                       // the tavern piece playing is the festival's dance
  float levelHoldT_ = 0, heldGain_ = 1, heldMuffle_ = 0;   // the street bleed's level held through a crossfade away
  int alarmWas_ = -1;
  // HUD: the player's buff icons (Well Fed, Rested, Hungry, Weary)
  void drawBuffs(Game& g, float x, float y);

  // ---- M7 Home, continued (VIEW lane: home_view.cpp, buildmode.cpp, render.cpp, hud.cpp). The yard's lights for
  //      drawLighting (lanterns, campfires: collected with the home draws), the shadow and flat passes of the home draws
  //      (drawWorld draws the flats before the shadows, so every shadow lies on the farmland and the paths), the rider's
  //      silhouette for the hidden-hero ghost, the build screens' camera and pointer (drags), the HUD's home pills
 public:
  // (scripts, script_buildmode.cpp) drive the build screens: `build <words...>` / `expect build <words...>`; false with
  // err set when refused
  bool buildCommand(Game& g, const std::vector<std::string>& a, std::string& err);
  bool buildExpect(Game& g, const std::vector<std::string>& a, std::string& err);
 private:
  std::vector<LightPool> homeLights_;
  void drawHomeFlats(Vec2 cam);
  void drawHomeShadow(const HomeDraw& d, Vec2 cam);
  struct RideGhost { const Tex* t = nullptr; int fr = 0, row = 0; bool flip = false; float x = 0, y = 0; };
  RideGhost rideGhost_;
  void drawMountShadow(Game& g, const Actor& a, Vec2 cam);
  bool buildCamWant(Game& g, Vec2& want);       // Mode::Build: where the camera wants to be (the ghost in view)
  void buildPointer(Game& g, int phase, Vec2 p); // 1 move, 2 up (screen coords; the down is buildTap)
  // the HUD's home pills (YARD / DECORATE where the player owns the ground or the house, DISMOUNT on horseback)
  int homeHere(const Game& g, bool& indoors) const;   // the owned plot the player stands in or whose house he is in (-1)
  void drawHomeHud(Game& g);
  bool homeHudTap(Game& g, Vec2 p);              // true: a pill took the tap
  bool homeHudKey(Game& g, int key);             // B: yard / decorate, F: dismount
  void openBuild(Game& g, home::UiMode mode, int plot);
  struct BuildDrag { bool on = false, moved = false; Vec2 start; int gx = 0, gy = 0; int list = -1; float scroll0 = 0; int zone = 0; };
  BuildDrag buildDrag_;
  int buildCat_ = 0, storeSel_ = 0, storeSide_ = 0, packScroll_ = 0, storeScroll_ = 0, cookScroll_ = 0;
  std::string cookNote_;   // (M7 fix r3) the Cook screen's own "COOKED: X (HAVE n)" line (for the meal row cookNoteSel_)
  int cookNoteSel_ = -1;
  float cookNoteT_ = -100.0f;   // (t_ when it was set: it shows for a few seconds)
  float toastX1_ = 0, toastY0_ = 0, toastY1_ = 0;   // (M7 fix) the toast column's extent last frame (world prompts keep clear)
  bool buildHintDone_ = false;                       // (M7) the yard / decorate move hint was seen and used
  int buildHintGx_ = INT32_MIN, buildHintGy_ = INT32_MIN;   // the ghost tile the hint was first shown at

  // ---- M8 Fellowship (VIEW lane). The party column under the vitals (a framed portrait per companion travelling with
  //      the player: health, the low-health pulse, the bleeding-out fuse and the binding, the stance; waiting / at home
  //      ones small and dimmed), the command radial (hold a portrait on touch, C on the keyboard), the alert ribbons
  //      (Ev::PartyAlert) and the banter plates (Ev::Banter): rpg/view/party_hud.cpp. The companions in the world (the
  //      hound's sheet, the downed body and its wound, the binder kneeling, a mounted companion, the graves of the
  //      fallen, the hound's night howl): rpg/view/party_view.cpp. The menu's PARTY page (the companion's paper doll and
  //      the REMEMBERED page): rpg/view/paperdoll.cpp. Script commands: rpg/view/script_partyview.cpp.
 public:
  // (scripts) `partyview <words...>` / `expect partyview <words...>` (script_partyview.cpp); false with err when refused
  bool partyViewCommand(Game& g, const std::vector<std::string>& a, std::string& err);
  bool partyViewExpect(Game& g, const std::vector<std::string>& a, std::string& err);
  static constexpr int kPartyTab = 6;          // the menu's PARTY page (menuTab_ id; shown while there is a party or a grave)
 private:
  struct PartyCard { int member = -1; float x = 0, y = 0, w = 0, h = 0; bool small = false; };
  std::vector<PartyCard> partyCards_;          // laid out by drawPartyHud this frame (the taps' hit tests)
  std::vector<int> partyActor_;                // members index -> g.actors index this frame (-1 not in play)
  float partyColR_ = 0, partyColB_ = 0;        // the column's right edge and foot this frame (0 none: toasts keep clear)
  int partySel_ = 0;                           // the selected member (the radial's and the PARTY page's)
  float partyTagT_ = 0;                        // the selected card's name tag, shown for a moment after a tap
  struct PartyPress { bool on = false; uint64_t id = 0; int member = -1; Vec2 start, cur; float t = 0; bool mouse = false; };
  PartyPress partyPress_;                      // a finger (or the mouse) held on a card: a hold opens the radial
  struct Radial {
    bool open = false, drag = false, armed = false;   // drag: opened by a hold (release picks); armed: the finger moved
    int member = -1, hot = -1;
    Vec2 c, from;
    float t = 0;
    uint64_t finger = 0;
    float denyT = 0;                           // a refused order: the wedge flashes and the reason shows
    int denyWedge = -1;
    std::string deny;
  };
  Radial radial_;
  struct PartyNote { uint32_t id = 0; int kind = 0; std::string s; float t = 0, life = 3;   // id: Member::id
                     float held = 0; bool shown = false; };   // (fixer M8 r3) banter: seconds kept waiting; drawn this frame
  std::vector<PartyNote> banterShow_, alertShow_;
  // (fixer M8 r1, review: "speech plates and ribbons draw on top of each other and over the town banner") the boxes a
  // spoken line keeps clear of this frame (screen px): the HUD's bands (hudBands: the arrival banner, the herald, the
  // biome name, the party ribbon) and every plate already placed (the M5 speech plates, then the banter)
  struct PlateBox { float x0 = 0, y0 = 0, x1 = 0, y1 = 0; };
  std::vector<PlateBox> plateBoxes_;
  void hudBands(const Game& g, std::vector<PlateBox>& out);
  bool partyRibbonBox(const Game& g, PlateBox& b, float& textW);   // where the party ribbon stands this frame (false none)
  // moves a plate (top-left x0, y0; w x h) to the nearest place inside L..R x T..B that overlaps none of `avoid`
  static void clearPlate(float& x0, float& y0, float w, float h, const std::vector<PlateBox>& avoid, float L, float T, float R, float B);
  int partyMapSeen_ = -1;                      // Party::mapChanges last seen: a new place clears the old scene's plates
  std::string partyDeny_;                      // the last refused order's reason by its card (scripts read it)
  float partyDenyT_ = 0;
  uint32_t partyDenyId_ = 0;
  std::unordered_map<uint64_t, Tex> partyTex_; // portraits, the hound's sheets, graves, the radial's wedges and icons
  float howlT_ = 30;
  int partyPage_ = 0, partySlot_ = 0, partyPack_ = 0, partyScroll_ = 0, partyFocus_ = 0;   // the PARTY page's state
  int partyDollTurn_ = 0;                      // (fixer M8 r2) the companion doll's facing: 0 front, 1 right, 2 back, 3 left (< TURN >)
  int memberOf(const Game& g, uint32_t id) const;          // members index of a Member::id (-1 gone)
  int companionIndex(const Game& g, const Actor& a) const; // members index an actor embodies (-1 none)
  void partyEvent(Game& g, const Event& e);    // Ev::Banter, Ev::PartyAlert (update's event pass)
  void partyUpdate(Game& g, float dt);         // timers, the hold, the howl
  void drawPartyHud(Game& g);                  // the column, the alerts (drawHud)
  void drawPartyRadial(Game& g);               // the radial and the refusals (over the whole HUD)
  void drawBanter(Game& g, Vec2 cam);          // the banter plates over the speakers (after the lighting)
  bool partyPointer(Game& g, int phase, uint64_t id, Vec2 p, bool mouse);   // 0 down 1 move 2 up; true: taken
  bool partyKey(Game& g, int key);             // C: the radial; while it is open every key is its
  void radialOpen(Game& g, int member, bool drag, uint64_t finger);
  int radialWedgeAt(Vec2 p) const;             // -1 the hub, -2 outside, else the Cmd
  void radialChoose(Game& g, int cmd);
  std::string radialWhy(const Game& g, int member, int cmd) const;   // why an order cannot be given now ("" it can)
  int bestSkill(const Game& g, int member, float& cd, bool& locked) const;   // the Skill wedge's skill (index 0..2, -1 none)
  const Tex& portraitTex(const Game& g, const party::Member& m, int mood);
  const Tex& houndTex(const party::Member& m, const Game& g);
  art::HoundLook houndLookOf(const party::Member& m, const Game& g) const;
  void drawPortraitCard(Game& g, int mi, float x, float y, bool small);
  // the world (party_view.cpp)
  bool drawCompanionBody(Game& g, const Actor& a, Vec2 cam, float alpha, float flash);   // true: drawn (hound, rider)
  void drawCompanionShadow(Game& g, const Actor& a, Vec2 cam, bool& done);
  void companionOverlays(Game& g, Vec2 cam);   // the wound mark and the binding ring over the downed (after lighting)
  art::Posture partyPosture(const Game& g, const Actor& a) const;    // Downed / Kneel the sim leaves as None
  void graveDraws(Game& g, const Map& m, Vec2 cam, std::vector<int>& out);   // the fallen whose graves are in view
  void drawGrave(Game& g, int fallen, Vec2 cam, bool shadow);
  void drawPartyMinimap(Game& g, float x, float y, int size, int ox, int oy);
  void drawPartyWorldMap(Game& g, Vec2 (*toScr)(void*, double, double), void* ctx, float w, float h);
  // the PARTY page (paperdoll.cpp)
  bool partyTabShown(const Game& g) const { return !g.party.members.empty() || !g.party.fallen.empty(); }
  void drawPartyPage(Game& g);
  void partyPageKey(Game& g, int key);
  void partyPageTap(Game& g, Vec2 p);

  // ---- M8b Treasure (VIEW lane; rpg/view/loot_ui.cpp, VISION_PLAN 15.27): every rarity-coloured surface draws through
  //      these. rarityText: a name in its ladder colour (Mythic: letter by letter in the animated flame, rarityColorAt);
  //      rarityFrame: a slot / icon frame (Common a quiet frame, Uncommon..Legendary a lit bevel in the colour,
  //      Legendary corner gems, Mythic a moving flame gradient with ember licks); itemCard: the tooltip (name, rarity and
  //      item level, the base stat, every affix in plain words, the power, lore, value) with the compare against the
  //      worn piece (green / red pixel arrows per line and an overall verdict from gear::powerScore); the loot on the
  //      ground (beams by rarity, their lights, the chime) and the Mythic wearer's embers.
 public:
  struct LootStats {
    int beams[6] = {};        // pickups drawn last frame with their rarity's ground presence (index: rarity)
    int cards = 0, compares = 0, mythicText = 0, frames = 0;   // item cards, compare panels, Mythic names, rarity frames
    int chimes = 0;           // LootChime plays so far
    int embers = 0;           // Mythic wearers drawn with their flame (embers spawned so far)
  };
  LootStats lootStats() const { return lootShown_; }
  // (scripts, script_loot.cpp) `lootsel menu <i|last>`, `lootsel shop <buy|sell> <i|last>`, `lootsel doll <slot> <cand>`:
  // put the cursor on a row the way the keys do (the doll's pick opens its compare card); false with err when refused
  bool lootSelect(Game& g, const std::vector<std::string>& a, std::string& err);
  void paperdollPeek(Game& g, int slot, int cand);   // paperdoll.cpp: the slot, the pack row, and the compare card shown
  // (scripts) what the last item card drew (`expect lootcard ...`, script_loot.cpp)
  using CardInfo = lootp::CardInfo;
  CardInfo lastCard() const { return lastCard_; }
 private:
  LootStats lootShown_, lootCount_;
  CardInfo lastCard_;
  float lootChimeT_ = -100;                        // t_ of the last chime (a pickup right after its sighting stays quiet)
  float emberT_ = 0;
  Tex beamTex_;                                    // a soft vertical gradient (white; tinted additively per rarity)
  const Tex& beamTex();                            // (baked on first use: loot_paint.h beamProfile)
  // (M8b fixer r3) a Mythic wearer's flame aura sheet (loot_ui.cpp mythicAuraTex), by look; render.cpp draws a cell of it
  // behind the standing body and again additively after the lighting
 public:
  static constexpr int AURA_PAD = 4, AURA_CW = art::HUMAN_W + 2 * AURA_PAD, AURA_CH = art::HUMAN_H + 2 * AURA_PAD, AURA_V = 4;
  const Tex* mythicAuraTex(const art::HumanLook& L);
  // the aura cell for a Mythic look (nothing for any other) at the figure's top-left (x, y), sc screen px per sprite px
  void mythicAuraAt(const art::HumanLook& L, int frame, int row, bool flip, float x, float y, float sc);
 private:
  std::unordered_map<uint64_t, Tex> mythicAura_;
  struct LootPainter;                              // the Pix painter loot_paint.h's templates draw with (loot_ui.cpp)
  void rarityText(float x, float y, const std::string& s, Rarity r, float alpha = 1, int align = 0, bool shadow = false);
  void rarityFrame(float x, float y, float w, float h, Rarity r, bool hot = false, bool small = false);
  void rarityIcon(const Game& g, const Item& it, float x, float y, float size);   // the icon in its rarity cell
  void cmpArrow(float x, float y, int dir);        // 1 up (green), -1 down (red), 0 even (a grey dash)
  // the item card at (x, y) w wide down to maxY; cmp: compare against `vs` (nullptr: the slot is empty); worn: the
  // piece is the one worn (a WORN tag instead of the compare). Returns the y under it.
  float itemCard(const Game& g, const Item& it, float x, float y, float w, float maxY, bool cmp, const Item* vs, bool worn, bool bigIcon = true);
  const Item* wornFor(const Game& g, const Item& it, int selfIndex, bool& isWorn) const;   // the slot's worn piece
  int cmpVerdict(const Game& g, const Item& it, const Item* vs) const;                     // 1 better, -1 worse, 0 even
  int attuneWhy(const Game& g, const Item& it, const Item* vs, std::string* name = nullptr) const;   // 0 / 1 lv10 / 2 full
  void packUse(Game& g, int i);   // the pack's EQUIP / USE (a great piece that cannot be worn: the reason blinks)
  float lootBlockFlash_ = 0;   // (M8b fixer r2) a tap on a disabled EQUIP: the card's reason line blinks a moment
  void drawLootGround(Game& g, const Pickup& k, int idx, Vec2 cam, float bob);   // the sorted pass (case 4)
  void lootLights(Game& g, const std::function<void(Vec2, float, Color, float)>& light);
  void lootUpdate(Game& g, float dt);              // chimes for Epic+ coming into view, the Mythic wearers' embers
  void lootNotice(Game& g, const Event& e, Toast& t);   // an item pickup notice: its rarity, the chime

  // ---- M8b Treasure (POWERS lane; rpg/view/render_powers.cpp). The worn powers' effects in the world: a lightning arc,
  //      a fire patch, a burn, the thorns aura, the time-slow tint, a lifesteal mote, the phoenix's rebirth, a falling
  //      star, a spectral wolf's shimmer (Ev::Power: a = powers::Fx, p / q / f / id). The POWERS lane owns these members
  //      (it may add more state inside PowerFx); every other M8b view change belongs to the VIEW lane.
  struct PowerFx { int kind = 0; Vec2 p, q; float t = 0, life = 0.4f, mag = 1; uint32_t follow = 0; uint32_t seed = 0; };
  std::vector<PowerFx> powerFx_;
  void powerEvent(Game& g, const Event& e);    // Ev::Power (update's event pass)
  void powerUpdate(Game& g, float dt);         // age the effects (update, after partyUpdate)
  void drawPowerFx(Game& g, Vec2 cam);         // world space, after the lighting (draw, with companionOverlays)

  // ---- M9 Arcana & Mastery (VISION_PLAN 15.29, 15.30). Two blocks, each owned by one lane; view.h itself is frozen:
  //      each lane keeps its state in its own opaque struct (defined in its .cpp) and reaches View's internals through
  //      its friend Ops struct (static functions in its own files), so neither needs to edit this header.
  // SPELLFX lane (rpg/view/render_magic.cpp + its own new files): every spell's life on screen (wind-up, launch,
  //   travel, impact, aftermath: Ev::Spell phases from the sim; ProjKind::Spell missiles), the surfaces' animated pixel
  //   art (magic::Field), the statuses' overlays, the spells' light (lit trails, impact flashes, lightning's frame
  //   flash), decals, baked effect sheets and pooled particles inside the phone frame budget, the spell sounds.
 public:
  struct MagicFxStats {      // last frame's counts (scripts `expect magicfx`, --perf, the gallery)
    int effects = 0, particles = 0, lights = 0, decals = 0, surfaceTiles = 0, missiles = 0, statusOverlays = 0;
    double ms = 0;           // the magic draw's own time last frame (ms)
  };
  MagicFxStats magicFxStats() const;
  struct MagicFxState;       // (render_magic.cpp)
 private:
  friend struct MagicFxOps;  // (render_magic.cpp and the SPELLFX lane's other files)
  std::shared_ptr<MagicFxState> magicFx_;
  void magicEvent(Game& g, const Event& e);    // Ev::Spell (the update's event pass, spawnParticles)
  void magicUpdate(Game& g, float dt);         // age the effects (update, after powerUpdate)
  void magicShift(Vec2 d);                     // Ev::WindowShift: effects and decals move with the window
  void drawMagicGround(Game& g, Vec2 cam);     // world space under the bodies: surfaces, decals, ground glyphs (drawWorld)
  void drawSpellProj(Game& g, const Projectile& pr, int idx, Vec2 cam);   // a ProjKind::Spell missile in the sorted pass
  void magicLights(Game& g, const std::function<void(Vec2, float, Color, float)>& light);   // drawLighting
  void drawMagicFx(Game& g, Vec2 cam);         // world space after the lighting: bursts, trails, flashes (View::draw)
  // UI lane (rpg/view/skills_ui.cpp, rpg/view/spellbook_ui.cpp + hud.cpp, which it owns): the skills page (every skill's
  //   level and progress; the perk trees as tappable constellations / culture glyph trees; the weapon families' vs-foe
  //   lines; respec), the spellbook page (known spells, the 3-4 quick slots, tomes), the quick-cast buttons on the phone
  //   (tap to cast, hold to upcast, swipe to dual-cast), skill-up and perk toasts (Ev::Skill), trainers' screen.
 public:
  static constexpr int kSkillsTab = 7;         // the menu's SKILLS page (menuTab_ id)
  static constexpr int kSpellsTab = 8;         // the menu's SPELLS page (menuTab_ id)
  struct SkillsUiState;      // (skills_ui.cpp)
 private:
  friend struct SkillsUiOps; // (skills_ui.cpp, spellbook_ui.cpp and the UI lane's other files)
  std::shared_ptr<SkillsUiState> skillsUi_;
  void skillEvent(Game& g, const Event& e);    // Ev::Skill (the update's event pass): toasts, the level-up glow
  void drawSkillsPage(Game& g);                // menuTab_ == kSkillsTab (hud.cpp's drawMenu calls it)
  void skillsPageKey(Game& g, int key);
  void skillsPageTap(Game& g, Vec2 p);
  void drawSpellsPage(Game& g);                // menuTab_ == kSpellsTab
  void spellsPageKey(Game& g, int key);
  void spellsPageTap(Game& g, Vec2 p);
};
