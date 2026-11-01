// EMBERVALE presentation: sprite bank, terrain baking, world rendering, lighting, particles, HUD, menus, touch controls.
#pragma once
#include <cstdlib>
#include <unordered_set>
#include <chrono>
#include <cmath>
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
#include "rpg/view/loot_card.h"
#include "rpg/view/camera.h"
#include "rpg/view/fx_state.h"
#include "rpg/view/hud_extents.h"
#include "rpg/view/lens_state.h"
#include "rpg/view/power_view.h"
#include "rpg/view/weather_view.h"
#include "rpg/view/home_view.h"
#include "rpg/view/life_view.h"
#include "rpg/view/loot_view.h"
#include "rpg/view/party_ui.h"
#include "rpg/view/realm_hud.h"
#include "rpg/view/sprite_cache.h"
#include "rpg/view/terrain_bake.h"

union SDL_Event;

// (Witness, View::witnessRelease) presentation state a file keeps outside View (its function statics, a memo by actor
// id) registers its reset next to that state with EMB_VIEW_RESET(fn); witnessRelease runs every registered reset (in
// no set order: each touches only its own file's state), so none can be forgotten. witness_state.cpp keeps the list.
struct ViewReset {
  explicit ViewReset(void (*fn)());
  static void runAll();
};
#define EMB_VIEW_RESET(fn) static const ViewReset embViewReset_##fn(fn)

namespace wmap { struct MapFrame; }   // (SOLID 3.4d) worldmap_internal.h

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
  friend struct ViewProbe;
  // (view_probe.h) the hooks only the script commands and the Witness state use (camera, menu page, forced weather and
  // town look, the presented layers, herald and journal, the M4 / M5 / loot counters, partyview): ViewProbe's statics
  void openMenu(Game& g, int tab) { g.mode = Mode::Menu; menuTab_ = tab; menuSel_ = 0; }
  // (Witness) wait (up to 5 s) until the terrain bakers are idle, so the next draw shows every chunk asked for: a
  // harness screenshot never depends on how fast the worker threads were
  void settleBakes() { bake_.settleBakes(); }
  // (Witness) a harness drives this view (rpg/main.cpp --witness, or a WITNESS_RECORD session): nothing on screen
  // waits on a frame budget or a worker (chunks, buildings and pose sheets are made inline when drawn), and a journey's
  // arrival is the sim's to end (Game::syncStreams: a fixed number of steps), so a run never depends on thread timing
  bool witness = false;
  // (Witness) with `witness` set, every pointer position (mouse, fingers) is snapped to the centre of its 1/16 logical-px
  // cell, here and in the WITNESS_RECORD recorder (rpg/main.cpp). A replay delivers the recorded position through window
  // coordinates at another window size; that round trip is off by an ulp or two, far inside half a cell, so the view
  // sees the very same floats as the recorded session did. Normal play reads positions as they come.
  static float witnessSnap(float v) { return (std::floor(v * 16.0f) + 0.5f) / 16.0f; }
  // (Witness) between runs (rpg/view/witness_state.cpp): the bakers are joined, every texture made since
  // Pix::markTextures() is destroyed (this View's caches, the lanes', the function statics') and the presentation state
  // kept outside View is reset. The caller then rebuilds the View in place and init()s it again, so no picture or
  // sound of an earlier run carries into the next.
  void witnessRelease();
  // (Witness reset param hide: [ids or type names], sent by `witness measure figure`) these actors are not drawn this run:
  // no body, contact shadow, ring, silhouette through a roof, bubble or marker. Drawing only: the sim, the snapshot and
  // every other picture stay as they are, so the shot minus this run's is the actor's exact silhouette. An id matches
  // Actor::id, a name matches the snapshot's entity type (Player, Villager, Guard, Wolf, Hen...; witness_state.cpp).
  struct WitnessHide {
    std::vector<int> ids;
    std::vector<std::string> types;
    bool empty() const { return ids.empty() && types.empty(); }
  };
  WitnessHide wHide;
  bool witnessHides(const Game& g, const Actor& a) const;   // (witness_state.cpp, EMB_WITNESS builds)
  bool wantsQuit = false;
  bool wantNewGame = false, wantContinue = false, wantSave = false;
  bool touchUI = false;                  // show on-screen controls
  std::string titleNote;                 // a line under the title menu (e.g. an old save that no longer loads)
  std::string loadingCard;               // M1: a full-screen card ("FORGING THE WORLD") shown while a world is made
  void creatorPrepare(Game& g);          // (M3 fixer round 2) the creator's homeland choices, made behind the card
  // test scripts (--script): hold a key as if it were physically down; one-shot presses go through event()
  void scriptHold(int scancode, bool down) { if (scancode >= 0 && scancode < 512) scriptKeys_[scancode] = down; }

  // ---- P2a Clear Sight (VISION_PLAN 15.36 phase A; FROZEN: lanes ADD, never rename or change meaning) -----------------
  // (P2a item 3, FROZEN) the projection between the world layer and the UI layer: a world point in map px to UI logical
  // px and back, UI px per world px, the world layer's camera this frame (Camera, camera.h, says how)
  Vec2 toUi(Vec2 world) const { return camera_.toUi(*this, world); }
  Vec2 toWorld(Vec2 ui) const { return camera_.toWorld(*this, ui); }
  float uiPerWorld() const { return camera_.uiPerWorld(*this); }
  Vec2 worldCam() const { return camera_.worldCam(*this); }
  using RigTex = SpriteCache::RigTex;   // (sprite_cache.h) a look's v2 atlas for a part (rigTex)
  // after art::setRigVersion: every cache whose pixels depend on the rig (people, poses, sleepers, monsters, buildings,
  // props, the v2 atlases) is dropped and baked again on demand
  void rigChanged();
  // (item 10) test hooks (script commands and Witness calls: rpg/view/script_p2a.cpp)
  struct P2aHooks {
    bool hidePlayer = false;   // `hideplayer`: the hero's body is not drawn (the contact shadow and the ring stay): the
                               // shot pair p2_measure.mjs masks the figure with
    bool ui = true;            // `ui 0|1`: the UI layer (HUD, text, menus, touch controls) is drawn
    bool poseOn = false;       // `pose <clip> [family] [variant] [frame]`: the hero's frame frozen
    art::Clip poseClip = art::Clip::Idle;
    art::Family poseFamily = art::Family::Unarmed;
    int poseVariant = 0, poseFrame = 0;
  };
  P2aHooks p2a;
  // (P2b phase A) test hooks (script commands and Witness calls: rpg/view/script_p2b.cpp)
  struct P2bHooks {
    bool camHold = false;          // `camhold 1|0`: the camera stays where it is (a body crosses still ground: witness
                                   // measure motion reads the walk's registration); no follow, no shake. Witness's
                                   // reserved reset param camera:"hold" (--hold-camera) sets the same flag (rpg/main.cpp)
    std::string dollPart;          // `dollpart <part> <view> [key]`: the hero drawn as that one doll part alone at rig 2
    int dollFacing = 0;            //   (its view D U R L, its key), for an artist's review in the game ("" off)
    std::string dollKey = "rest";
  };
  P2bHooks p2b;
  // the hero as drawn last frame (world-layer px, relative to the world view's top-left): the cell's top-left, size and
  // frame, the opaque box inside the cell (v1: from the baked sheet), the pivot (the feet point) and the rig it came from
  struct HeroSprite {
    bool on = false;
    int rig = 1;
    float x = 0, y = 0;
    int w = 0, h = 0, fr = 0, row = 0;
    bool flip = false;
    int bx0 = 0, by0 = 0, bx1 = -1, by1 = -1;   // opaque box in the cell (inclusive; bx1 < bx0: unknown)
    float pivX = 0, pivY = 0;
    int part = -1;   // (P2b) the art::RigPart the rig-2 frame came from (VIEW-B sets it on every v2 path; -1: v1 or unknown)
  };
  HeroSprite heroSprite;
  // people frames drawn from v1 sheets (P2b gate B2): v1Draws counts during a draw and holds the last drawn frame's count
  // after it (what Witness reports); v1DrawsLast the frame before
  int v1Draws = 0, v1DrawsLast = 0;
  // (P2c flip, D103: C2 over a whole run) the people frames from v1 sheets summed over the run, by the rig they were drawn
  // at: v1DrawsRun2 while rig 2 (the default) is set, v1DrawsRun1 while rig 1 is opted into. View::draw adds each finished
  // frame; v1DrawsRunAt adds the last drawn frame too (a `--script` run's `v1draws` line, Witness screen.v1DrawsRun)
  long long v1DrawsRun2 = 0, v1DrawsRun1 = 0;
  int v1DrawsRig = 0;   // the rig the frame in v1Draws was drawn at (0: none drawn yet)
  long long v1DrawsRunAt(int rig) const {
    return (rig >= 2 ? v1DrawsRun2 : v1DrawsRun1) + (v1DrawsRig != 0 && (v1DrawsRig >= 2) == (rig >= 2) ? v1Draws : 0);
  }
  // ---- (P2c SWITCH) the menus on the doll (rig 2; rig 1 draws every menu as before). Each menu figure drawn this frame
  // (Witness state.menus): where (MenuWhere), the rig it came from, the whole multiple, its rect on the UI layer, the stage
  // it stands on and whether the rect leaves it (C7: the T6 envelope uncropped), the furthest finish light from the
  // silhouette (C7: the Mythic aura within 3 px; -1 none drawn), the part / clip / frame / facing, the look's top tier
  enum MenuWhere { kMenuPaperdoll = 0, kMenuCreator, kMenuHero, kMenuParty, kMenuDialogue, kMenuPortrait, kMenuLoot, kMenuWhereN };
  enum class MenuGait : uint8_t { Idle, Walk, Run, Swing };
  struct MenuStage { float x = 0, y = 0, w = 0, h = 0; };
  struct MenuFigure {
    int where = 0, rig = 1, scale = 0, w = 0, h = 0, part = -1, clip = 0, frame = 0, facing = 0, tier = 0, auraOut = -1, lights = 0;
    float x = 0, y = 0;
    MenuStage stage;
    bool cropped = false;
  };
  std::vector<MenuFigure> menuFigs;
  // the largest whole multiple (at most want, at least 1) at which the T6 envelope (the fin at F-41, 28 px shoulders, the
  // cloak 8 px behind) and the look's own menu frames stand inside the stage with their feet at feetY; 0 at rig 1 or when
  // the doll does not cover the look (the caller draws v1)
  int menuDollScale(const art::HumanLook& L, const MenuStage& st, float feetX, float feetY, int want, bool swing = false);
  // a doll frame at a whole multiple with its living finish (T5-T6, the Mythic's flame on its own frame), the stir on the
  // idle's breathe frame; facing 0 D 1 U 2 R 3 L, never flipped. topRows > 0: only that many rows from the frame's top
  // (a dialogue's head-and-shoulders). False at rig 1 or when the look is not covered (the caller draws v1, counted)
  bool menuSwingFits(const art::HumanLook& L, const MenuStage& st, float feetX, float feetY, int scale, int facing);
  bool drawMenuDoll(const art::HumanLook& L, int where, int facing, MenuGait gait, float feetX, float feetY, int scale, const MenuStage& st,
                    float alpha = 1.0f, int topRows = 0, int id = 0);
  // a menu figure drawn from a v1 sheet (rig 1, or a look the doll does not cover yet): counted in v1Draws and listed
  void noteMenuV1(int where, float x, float y, int w, int h, const MenuStage& st);
  // (P2c SWITCH) a party portrait's living finish (its glow pixels pulsed, rig 2, T5-T6): after each blit of portraitTex
  // (the bust's source rect sx, sy, sw, sh as blitted: a face crop draws the glow inside it only)
  void portraitFinish(uint32_t memberId, float x, float y, float scale, float alpha = 1.0f, int sx = 0, int sy = 0, int sw = 24, int sh = 24);
  double peopleMB() const { return sprites_.peopleMB(); }   // the people textures alive (SpriteCache::peopleMB), MB
  // ---- (P2a LENS, added) the world view: its size in world px (the world layer's w x h: what the camera frames, what the
  // world draws read as Pix::W x Pix::H inside beginWorld / endWorld); ZOOM one level in (+1) or out (-1) through the
  // levels this screen allows (the - / = keys, the wheel, a pinch; saved to the settings file); the lens's counters for
  // the tests (the pips drawn last frame, the ring's flare, the v2 bake queue)
  int viewW() const;
  int viewH() const;
  // the metrics of the rig a person is DRAWN with (rig 2 only where art::rig2Covers(look): a v1 figure keeps v1's
  // marker, bubble, plate and bar heights while the preview is partial)
  static const art::RigMetrics& rigFor(const art::HumanLook& L) {
    return art::rigVersion() >= 2 && art::rig2Covers(L) ? art::rig(2) : art::rig(1);
  }
  // (P2b VIEW-B, added) px a person's drawn doll frame top lies above (+) or below (-) the 32 px reference crown the rig's
  // markerY / plateY / barY assume (a child, a seated body, a hat); 0 for a v1 figure or one not drawn lately
  float topLift(const Actor& a) const;
  struct LensStats { int pips = 0; float ringA = 0; int liftFigs = 0; int rigQueued = 0, rigBaked = 0; double rigBakeMs = 0, rigMB = 0;
                     int rigEvictions = 0, rigRebakes = 0; double actionMsMax = 0; };   // (P2c SWITCH, C8) evicted / baked again after; the slowest ACTION atlas
  double rigCapMB() const { const char* e = std::getenv("EMB_RIG_CAP_MB"); return e && std::atof(e) > 0 ? std::atof(e) : 16.0; }
  LensStats lensStats;

 private:
  Pix* pix_ = nullptr;
  Audio* audio_ = nullptr;
  float t_ = 0;
  // eased presentation state that used to be function statics (one View, so the same; members so a Witness reset renews
  // them): the snow's cold dusk light (drawLighting), the ridden horse's stride (update), the map tab's last draw
  float coldK_ = -1, strideT_ = 0, mapLastDraw_ = -10;
  // the camera: where the view stands, its zoom and the zoom's hand-over, the pinch, the wheel, the finisher's nudge
  friend struct Camera;
  Camera camera_;   // (camera.h)
  float titleT_ = 0;
  // the effects: particles, floating text, toasts, the banner and the screen shake (FxState, fx_state.h)
  friend struct FxState;
  FxState fx_;   // (fx_state.h)

  // ---- sprites: every texture cache (people, poses, v2 atlases, buildings, walls, forts, heraldry, props, icons,
  //      the lanes' sprites), its LRU trims and bake budgets live in the SpriteCache part (sprite_cache.h)
  friend struct SpriteCache;
  SpriteCache sprites_;

  // ---- terrain (terrain.cpp): the chunk cache and its bakers live in the TerrainBaker part (terrain_bake.h)
  friend struct TerrainBaker;
  TerrainBaker bake_;
  using TMap = TerrainBaker::TMap;
  uint32_t groundPixel(const TMap& m, int px, int py);   // px, py: global pixels
  int rockLevel(const TMap& m, int tx, int ty);
  uint32_t reliefPixel(const TMap& m, int px, int py, uint32_t c, Ground g, bool flat = false);   // cliff faces, lips, ramps, grade (flat: the grade alone, smoothed: a city's paving over a walkable step)
  friend struct TerrainScriptOps;   // (M11 TERRAIN lane) rpg/view/script_terrain.cpp: test stamps re-bake the chunks
  // EMB_PERF: bake time and texture counts, printed once a second
  struct Perf { double bakeMs = 0, worstBakeMs = 0, worstFrameMs = 0, frameMs = 0; int bakes = 0, inlineBakes = 0, frames = 0; float t = 0; };
  Perf perf_;
  void perfTick(float dt);
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

  // ---- the HUD's last extents (HudExtents), the fade, the music and the weather (WeatherView)
  friend struct HudExtents;
  HudExtents hudx_;   // (hud_extents.h)
  float fade_ = 0;
  Music music_ = Music::Silence;
  uint64_t musicStyle_ = 0, musicStyleOn_ = 0;   // M3: the culture's MusicStyle (packed) where the player is / playing
  float musicStyleT_ = 0;
  float combatT_ = 0;
  friend struct WeatherView;
  WeatherView weather_;   // (weather_view.h)

  // ---- input state
  bool scriptKeys_[512] = {};   // indexed by SDL_Scancode (SDL_SCANCODE_COUNT is 512)
  bool kAttack_ = false, kBow_ = false, kSpell_ = false, kRoll_ = false, kUse_ = false, kPotion_ = false, kSwap_ = false;
  bool kTapAttack_ = false;   // this attack came from a tap on the open world (never turned into a talk)
  struct Finger { uint64_t id = 0; bool on = false; Vec2 start, cur; int button = -1; uint64_t t0 = 0; bool use = false; };   // t0: ms at the touch (hud.cpp gestureMs); use: began as a use (TALK, RIDE...)
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
  friend struct RealmHud;
  RealmHud realm_;   // (realm_hud.h)

  // ---- architecture (render.cpp, M0 architecture lane): wall-tile keys for the current map (art::wallKeys), the
  //      wall-tile / gatehouse sprites, and the chimney mouths of each building sprite (for smoke)
  std::vector<uint32_t> wallKeys_;
  uint64_t wallKeysId_ = 0;
  bool windowsLit(const Game& g, const Bldg& b) const;       // night, and this household is awake
  float smokeT_ = 0;
  const Map* bldgMap_ = nullptr;   // the map whose buildings drawWorld is drawing (bldgTex reads its biome)
  double fadePaintMs_ = 0;         // (M1 round 3) building paint time spent this frame behind a fade (drawWorld)
  int bldgPrefetch_ = 0;           // round-robin cursor: buildings near the player are painted ahead, one per frame
  std::vector<uint32_t> wallTodo_; // wall-tile keys of the current map not painted yet (painted ahead, one per frame)

  void drawWorld(Game& g);
  void layoutWallKeys(Game& g, const Map& m, uint64_t mapId, bool endlessOver);   // (SOLID 3.4a) once per map
  void spanWallGaps(Game& g, const Map& m);
  // ---- (SOLID 3.4b) update's pieces, called in this order (render.cpp)
  void updateBanners(Game& g);
  void updateBeastCues(Game& g, float dt);
  void updateMusic(Game& g, float dt);
  void updateDrift(Game& g, float dt, bool outdoors, float VW, float VH);
  // ---- (SOLID 3.4a) drawWorld's sorted pass (rpg/view/render_sorted.cpp): what stands is collected as a Drawable,
  //      sorted by its foot line, its ground shadow drawn, then its body, one member per kind (FrameCtx: the frame's
  //      camera, the roofs in view, the hidden hero)
  struct Drawable;
  struct FrameCtx;
  void drawSortedPass(Game& g, const Map& m, Vec2 cam);
  void collectTiles(Game& g, FrameCtx& F, std::vector<Drawable>& list);
  void collectBuildings(Game& g, FrameCtx& F, std::vector<Drawable>& list);
  void collectActors(Game& g, FrameCtx& F, std::vector<Drawable>& list);
  void collectOthers(Game& g, FrameCtx& F, std::vector<Drawable>& list);
  void drawShadowSorted(Game& g, FrameCtx& F, const Drawable& d);
  void drawActorShadow(Game& g, FrameCtx& F, const Drawable& d);
  void drawPropShadow(Game& g, FrameCtx& F, const Drawable& d);
  void drawGateShadow(Game& g, FrameCtx& F, const Drawable& d);
  void drawPropSorted(Game& g, FrameCtx& F, const Drawable& d);
  void propMarketTex(Game& g, FrameCtx& F, const Drawable& d, art::Prop p, bool vendorOpen, const Tex*& tp);
  bool propCultureStyled(Game& g, FrameCtx& F, const Drawable& d, art::Prop p, const Tex*& tp);
  bool drawCityPropSorted(Game& g, FrameCtx& F, const Drawable& d, art::Prop p, const Tex*& tp);
  void propVariantTex(Game& g, FrameCtx& F, const Drawable& d, art::Prop p, const Tex*& tp, bool& flipVar);
  void drawVendorSorted(Game& g, FrameCtx& F, const Drawable& d, art::Prop p, const Tex* tp, int stallFacing, bool vendorOpen);
  void drawPalisadePiece(Game& g, FrameCtx& F, const Drawable& d);
  void drawBuildingSorted(Game& g, FrameCtx& F, const Drawable& d);
  void drawWallSorted(FrameCtx& F, const Drawable& d);
  void drawGateSorted(Game& g, FrameCtx& F, const Drawable& d);
  void drawActorSorted(Game& g, FrameCtx& F, const Drawable& d);
  void drawHumanSorted(Game& g, FrameCtx& F, const Actor& a, float alpha, float flash);
  void drawCritterSorted(FrameCtx& F, const Actor& a, float alpha, float flash);
  bool drawMonsterSorted(Game& g, FrameCtx& F, const Actor& a, float alpha, float flash);
  void drawTelegraph(FrameCtx& F, const Actor& a);
  void drawProjSorted(Game& g, FrameCtx& F, const Drawable& d);
  void drawHandcartSorted(Game& g, FrameCtx& F, const Drawable& d);
  void drawHiddenHero(Game& g, FrameCtx& F);
  void drawLighting(Game& g);
  void drawHud(Game& g);
  // ---- (SOLID 3.4c) drawHud's pieces, in this order (hud.cpp); HudFrame: the frame's shared values
  struct HudFrame;
  float hudVitals(Game& g, const HudFrame& H);   // returns the quick bar's left edge
  void hudQuickSlots(Game& g, const HudFrame& H, float spellX);
  void hudBurden(Game& g, const HudFrame& H);
  void hudLocationColumn(Game& g, HudFrame& H);
  bool hudColumnFade(Game& g, HudFrame& H);      // true: the minimap shows
  void hudKingLines(Game& g, const std::string& loc, std::string& kingLine, Color& kingCol, std::string& stateLine, Color& stateCol);
  void hudRaidPointer(Game& g, const HudFrame& H);
  void hudMenuBadge(Game& g, const HudFrame& H);
  void hudQuest(Game& g, HudFrame& H);
  void hudBossBar(Game& g, const HudFrame& H);
  void hudPrompt(Game& g, const HudFrame& H);
  bool hudNotices(Game& g, const HudFrame& H);   // true: the companions' order ring is open
  void drawTouch(Game& g);
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
  // ---- M7 Home: the property's draws, the Mode::Build screens and the HUD's home pills live in HomeView (home_view.h)
  friend struct HomeView;
  HomeView home_;   // (home_view.h)
  // (kept View members, as the size gate names them; they read and write HomeView's state) the property's draws for
  // drawWorld's sorted pass (Drawable kind 8); the Mode::Build screens (buildmode.cpp)
  void homeCollect(Game& g, const Map& m, Vec2 cam);
  void drawBuild(Game& g);
  void shopDeal(Game& g);   // buy or sell the selected row (a worn item asks twice)
  void drawLevelUp(Game& g);
  void drawTitle(Game& g, bool hasSave);
  void drawDead(Game& g);
  void drawMinimap(Game& g, float x, float y, int size);
  // worldmap.cpp (M1): the pannable, zoomable world map of the MAP tab
  void drawWorldMap(Game& g, float x, float y, float w, float h);
  // ---- (SOLID 3.4d) drawWorldMap's pieces, in this order (worldmap.cpp; the markers in worldmap_markers.cpp);
  //      wmap::MapFrame (worldmap_internal.h): the frame's rectangle, zoom, labels, boxes and pins
  void mapTiles(Game& g, wmap::MapFrame& M);
  void mapPlans(Game& g, wmap::MapFrame& M);
  void mapBiomeCensus(Game& g, wmap::MapFrame& M);
  void mapRealmLabels(Game& g, wmap::MapFrame& M);
  void mapMarkersPlaces(Game& g, wmap::MapFrame& M);
  void mapMarkersWar(Game& g, wmap::MapFrame& M);
  void mapQuestPins(Game& g, wmap::MapFrame& M);
  void mapMarkersHome(Game& g, wmap::MapFrame& M);
  void mapMarkersParty(Game& g, wmap::MapFrame& M);
  void placeLabels(Game& g, wmap::MapFrame& M);
  void mapQuestMarks(Game& g, wmap::MapFrame& M);
  void mapMarkersSea(Game& g, wmap::MapFrame& M);
  void mapHero(Game& g, wmap::MapFrame& M);
  void mapGeologyAndScale(Game& g, wmap::MapFrame& M);
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
  void panel(float x, float y, float w, float h, float alpha = 0.92f);
  void button(float x, float y, float w, float h, const std::string& label, bool hot);
  void wrapText(float x, float y, float w, const std::string& s, Color c, int maxChars = -1, int lineH = 9);
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

  // ---- M4 "Banners": the herald, the news, the arrival banner, the journal's sections and the realm's fires (RealmHud)
 public:
  // (M12) 3: STANDING (the army, the holdings, the player's realm, the world's history: realm_hud.cpp)
  void setJournalSection(int s) { realm_.journalSec_ = s < 0 ? 0 : s > 3 ? 3 : s; menuSel_ = 0; menuScroll_ = 0; }
  using M4Stats = RealmHud::M4Stats;   // (realm_hud.h)
 private:
  // the QUESTS tab's section strip (QUESTS | NEWS | HISTORY) and the news / lore lists; journalTap: true when used
  float journalStripH() const { return touchUI ? 26.0f : 16.0f; }

  // ---- M5 "Hearth and Hall": bubbles, festival, buffs and the life of the place in sound live in LifeView (life_view.h)
 public:
  using M5Stats = LifeView::M5Stats;   // (life_view.h)
  // the word and colour of a settlement's mood (life::Life::moodFlags / mood): "CONTENT", "FESTIVAL", "HUNGRY",
  // "FAMINE", "WAR-TORN", "MOURNING"...; empty when its census is not known
  static std::string moodWord(const Game& g, ew::Gid site, uint32_t* color);
 private:
  friend struct LifeView;
  LifeView life_;   // (life_view.h)
  // ---- P2a (rpg/view/render_anim.cpp: the human frame pick, body draw, swing effect and the dead's offset, moved out of
  //      render.cpp pixel-identical at rig 1; the ANIM lane owns render_anim.cpp)
  // a human's body this frame (the standing sheet, a posture, a sleeper in its berth, a bather; the Mythic aura, the hit
  // flash and the burn); the hero's draw also fills ghost (the silhouette shown through a roof: set only for the hero)
  // and heroSprite
  struct HumanGhost { const Tex* t = nullptr; int fr = 0, row = 0; bool flip = false; float x = 0, y = 0; };   // (as RideGhost)
  void drawHumanBody(Game& g, const Map& m, const Actor& a, Vec2 cam, float alpha, float flash, HumanGhost& ghost);
  void drawSwingFx(const Game& g, const Actor& a, Vec2 cam);   // a human's melee strike effect (after its body)
  friend struct LensState;
  LensState lens_;   // (lens_state.h)
  friend struct P2aScriptOps;   // (rpg/view/script_p2a.cpp: the P2a test commands)
  friend struct P2bScriptOps;   // (rpg/view/script_p2b.cpp: the P2b test commands)
  // ---- (P2a LENS) the rider on v2, the elites' plates (the ring, the night lift, the pips: LensState, lens_state.h)
  struct RideGhost;
  bool drawRiderV2(const art::HumanLook& L, int facing, float hipX, float hipY, float pivX, float pivY, Color c, int blend, RideGhost* ghost, int horseFrame = 0, int riderId = -1);
  void drawBeastPlates(Game& g);   // the elites' name plates and a world boss's bar: the UI layer (STYLE_P2 18)
  friend struct PartyUi;
  PartyUi party_;   // (party_ui.h)
  void drawPartyPage(Game& g);   // the menu's PARTY page (paperdoll.cpp; a View member as the size gate names it)
  // (fixer M5 r3) where each human figure was actually drawn this frame (map px: x centre, y the cell's top; t_ when):
  // the bubble rides on the drawn head, whatever posture or seat fit moved it
  // (P2b VIEW-B, added) v2: y is the doll frame's top (RigAnchors::topY of the look's rest frame, moved with the head in a
  // posture), so markers, plates and bubbles float over hair, a hat or a crest (STYLE_P2 17)
  struct DrawnHead { float x = 0, y = 0, t = -1; bool v2 = false; };
  std::unordered_map<int, DrawnHead> drawnHead_;

  // ---- M7 Home, continued: the build screens' script commands and the rider's silhouette (the rest: HomeView)
 public:
  // (scripts, script_buildmode.cpp) drive the build screens: `build <words...>` / `expect build <words...>`; false with
  // err set when refused
  bool buildCommand(Game& g, const std::vector<std::string>& a, std::string& err);
  bool buildExpect(Game& g, const std::vector<std::string>& a, std::string& err);
 private:
  // (P2a LENS, added: sx, sy, w, h) a v2 frame's own rect in its atlas (w 0: the v1 cell fr x row)
  struct RideGhost { const Tex* t = nullptr; int fr = 0, row = 0; bool flip = false; float x = 0, y = 0; int sx = 0, sy = 0, w = 0, h = 0; };
  RideGhost rideGhost_;

  // ---- M8 Fellowship: the party column, radial, banter, companions in the world and the PARTY page (PartyUi, party_ui.h)
 public:
  static constexpr int kPartyTab = 6;          // the menu's PARTY page (menuTab_ id; shown while there is a party or a grave)
 private:
  // the PARTY page (paperdoll.cpp)
  bool partyTabShown(const Game& g) const { return !g.party.members.empty() || !g.party.fallen.empty(); }

  // ---- M8b Treasure: every rarity-coloured surface, the item card and the loot on the ground (LootView, loot_view.h)
 public:
  using LootStats = LootView::LootStats;   // (loot_view.h)
  // (scripts, script_loot.cpp) `lootsel menu <i|last>`, `lootsel shop <buy|sell> <i|last>`, `lootsel doll <slot> <cand>`:
  // put the cursor on a row the way the keys do (the doll's pick opens its compare card); false with err when refused
  bool lootSelect(Game& g, const std::vector<std::string>& a, std::string& err);
  void paperdollPeek(Game& g, int slot, int cand);   // paperdoll.cpp: the slot, the pack row, and the compare card shown
  // (scripts) what the last item card drew (`expect lootcard ...`, script_loot.cpp)
  using CardInfo = lootp::CardInfo;
 private:
  friend struct LootView;
  LootView loot_;   // (loot_view.h)
  // (M8b fixer r3) a Mythic wearer's flame aura sheet: its cell geometry (SpriteCache::mythicAuraTex bakes it)
 public:
  static constexpr int AURA_PAD = 4, AURA_CW = art::HUMAN_W + 2 * AURA_PAD, AURA_CH = art::HUMAN_H + 2 * AURA_PAD, AURA_V = 4;
 private:
  struct LootPainter;                              // the Pix painter the rarity templates draw with (loot_ui.cpp)

  // ---- M8b Treasure (POWERS lane): the worn powers' effects in the world (PowerView, power_view.h)
  friend struct PowerView;
  PowerView powers_;   // (power_view.h)
  void drawPowerFx(Game& g, Vec2 cam);   // the worn powers' effects, world space after the lighting (render_powers.cpp)

  // ---- M9 Arcana & Mastery: the SPELLFX lane (MagicLane) and the UI lane (SkillsLane); entry points in lane_entry.h
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
  friend struct MagicLane;
  void drawIncidentGround(Game& g, Vec2 cam);  // (P1 fixer) a street incident's ground props: a broken cart's loose wheel (render_markers.cpp)
 public:
  static constexpr int kSkillsTab = 7;         // the menu's SKILLS page (menuTab_ id)
  static constexpr int kSpellsTab = 8;         // the menu's SPELLS page (menuTab_ id)
  struct SkillsUiState;      // (skills_ui.cpp)
 private:
  friend struct SkillsUiOps; // (skills_ui.cpp, spellbook_ui.cpp and the UI lane's other files)
  std::shared_ptr<SkillsUiState> skillsUi_;
  friend struct SkillsLane;

  // ---- M10 Beyond the Mist: the ART lane's sea (SeaLane, lane_entry.h; state in the opaque SeaViewState)
 public:
  struct SeaViewStats {      // last frame's counts (scripts `expect seaview`, --perf, the sea gallery)
    int boats = 0, waveTiles = 0, foamTiles = 0, wakes = 0, mistPuffs = 0, rainDrops = 0;
    double ms = 0;           // the sea draw's own time last frame (ms)
  };
  SeaViewStats seaViewStats() const;
  struct SeaViewState;       // (render_sea.cpp)
 private:
  friend struct SeaViewOps;  // (render_sea.cpp and the ART lane's other sea files)
  std::shared_ptr<SeaViewState> seaView_;
  friend struct SeaLane;

  // ---- M11 High Ground: the FALLS lane's live relief (ReliefLane, lane_entry.h; state in the opaque ReliefViewState)
 public:
  struct ReliefViewStats {   // last frame's counts (scripts `expect reliefview`, --perf)
    int falls = 0;           // HEIGHT_FALL tiles drawn this frame
    int mistPuffs = 0, sprays = 0;
    float roar = 0;          // the level handed to Audio::setFalls
    double ms = 0;           // the relief draw's own time last frame (ms)
  };
  ReliefViewStats reliefViewStats() const;
  struct ReliefViewState;    // (render_relief.cpp)
 private:
  friend struct ReliefViewOps;   // (render_relief.cpp and the FALLS lane's other relief files)
  std::shared_ptr<ReliefViewState> reliefView_;
  friend struct ReliefLane;

  // ---- M12 Rise and Fall: the VIEW lane's rise (RiseLane, lane_entry.h; state in the opaque RiseViewState)
 public:
  struct RiseViewStats { int banners = 0; int toasts = 0; RiseEv last = RiseEv::None;     // scripts `expect riseview`
    // (phase B, VIEW lane) banners shown so far (queued ones wait their turn), the one showing's box (screen px), the
    // rank's badge drawn on the HUD this frame, the world's rise pieces drawn last frame (rise_world.cpp)
    int shown = 0;
    float bannerX = 0, bannerY = 0, bannerW = 0, bannerH = 0;
    bool insignia = false;
    int lordBanners = 0, works = 0, warlordPieces = 0, rebuildPieces = 0, raidScars = 0, standingRows = 0;
    int mapMarks = 0;          // the world map's rise markers drawn last frame (worldmap.cpp)
  };
  RiseViewStats riseViewStats() const;
  struct RiseViewState;      // (render_rise.cpp)
 private:
  friend struct RiseViewOps;     // (render_rise.cpp and the VIEW lane's other rise files)
  std::shared_ptr<RiseViewState> riseView_;
  friend struct RiseLane;
};

#include "rpg/view/lane_entry.h"   // the opaque-state lanes' entry points (MagicLane, SkillsLane, SeaLane, ReliefLane, RiseLane)
