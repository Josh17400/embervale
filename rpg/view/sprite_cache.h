// SpriteCache (SOLID 4.2, View part 2): every texture cache of the view, a value member of View (View::sprites_). The
// people (standing sheets, pose sheets, sleepers, the v2 atlases and their bake queue, the night lift's masks, the menu
// boxes and portrait glow), the buildings (sprites, their night and glass variants, smoke and windows, the async and
// stepped paints), the walls and forts, heraldry and kingdom banners, the culture-styled props, icons, monsters, and the
// lanes' sprites (deco, markers, home, party, loot, critters, lamps, festival). Its LRU trims (trimCaches,
// trimRigCaches) and bake budgets live here too. A getter that paints takes the View (its Pix, clock and flags).
// Included by rpg/view/view.h; defined where each cache was (render.cpp, render_anim.cpp, realm_render.cpp, party_hud.cpp,
// loot_ui.cpp, render_deco.cpp ...): the move kept every body in its file.
#pragma once
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "engine/pix.h"
#include "rpg/art.h"
#include "rpg/build/parts.h"
#include "rpg/sim/game.h"

class View;

struct SpriteCache {
  // (item 4) a look's v2 atlas for a part, baked and cached (LENS owns the caches, their MB caps and the bake queue;
  // render_anim.cpp draws from it). Not ready (tex null) until the painters cover the look: callers draw v1 meanwhile.
  struct RigTex {
    const Tex* tex = nullptr;
    const art::RigSheet* sheet = nullptr;   // the atlas's frames (the canvas itself is dropped once baked)
    const Tex* glowTex = nullptr;           // (P2c ladder) the glow plane's texture (null: the look has none)
    const std::vector<art::finish::Mask>* masks = nullptr;   // (polish i1) T6: each frame's silhouette (the aura, the sweep)
    bool ready() const { return tex && tex->t && sheet && !sheet->empty(); }
  };
  RigTex rigTex(View& v, const art::HumanLook& L, art::RigPart part, bool child = false);
  // (script_p2b.cpp) the `dollpart` frame for a look: baked and cached with the v2 atlases (not ready: drawn as usual)
  RigTex dollPartTex(View& v, const art::HumanLook& L);
  double peopleMB() const;            // the people textures alive (standing sheets, poses, sleepers, v2 atlases), MB
  std::unordered_map<uint64_t, Tex> humans_;
  std::vector<Tex> monsters_;
  std::vector<Tex> props_;
  std::unordered_map<uint64_t, Tex> bldgTex_;
  std::vector<Tex> walls_;
  Tex gate_;
  std::unordered_map<uint64_t, Tex> icons_;
  std::vector<Tex> fx_;
  Tex shadow_, shadowBig_, light_, white_, water_, vignette_;
  const Tex& humanTex(View& v, const art::HumanLook& L, bool child = false);   // (M5) child: the body shortened (childBody)
  const Tex& iconTex(View& v, art::Icon i, uint32_t tint);
  // M6 Steel: an item's icon in its maker culture's style and metal (rpg/sim/gear_look.h gearIcon -> art::itemIconLook);
  // use it for every Item drawn (the pack, the shop, the paper doll, pickups). iconTex stays for fixed UI icons.
  std::unordered_map<uint64_t, Tex> itemIcons_;
  const Tex& itemTex(View& v, const Game& g, const Item& it);
  // M6 Steel: a monster's sheet in its variant look (Actor::overlays / bodyTint: art::monsterSheetLook); the classic
  // sheet (monsters_) when it has none
  std::unordered_map<uint64_t, Tex> monsterLooks_;
  const Tex& monsterTex(View& v, const Actor& a);
  const Tex& bldgTex(View& v, const Bldg& b, int index);
  // M1 caches for huge cities: building sprites and character sheets are dropped least-recently-used once their
  // count passes a cap (a 220-building city, then the next one); buildings are found through a per-32-tile-cell
  // index of the current map (records are append-only for the session, so never every building per frame)
  std::unordered_map<uint64_t, float> bldgUsed_, humanUsed_;
  float lruT_ = 0;
  void trimCaches(View& v);
  // a texture cache for lane-painted sprites: key spaces are tagged by the top byte (0x01 deco, 0x02 markers,
  // 0x03 creator/paperdoll, 0x04 architecture); paint(key) is called once per key
  const Tex& cachedTex(View& v, uint64_t key, Canvas (*paint)(uint64_t key));
  // M1 kingdom identity: banners (kind 0) and gatehouses (kind 1) in a kingdom's colours, cached per look
  const Tex& kingdomTex(View& v, int kind, const Kingdom& k, uint32_t seed);
  std::unordered_map<uint64_t, Tex> kingdomTex_;
  std::unordered_map<uint64_t, Tex> laneTex_;
  std::unordered_map<uint64_t, Tex> wallTiles_;   // (M3b: keyed by wallTexKey: the key and its fort look)
  Tex gateTex_;
  std::unordered_map<uint64_t, std::vector<Vec2>> bldgSmoke_;
  std::unordered_map<uint64_t, int> bldgTopRow_;   // first opaque row of each building sprite (fade test)
  std::unordered_map<uint64_t, Tex> bldgNight_;    // the same sprite with its windows lit (night, people awake)
  // (fixer M5 r3) the panes as a half-size white mask, added into the light map at night: the glass alone is lit (a
  // round pool per window lit the roofs in front of an upper facade, a palace portico at daylight brightness)
  std::unordered_map<uint64_t, Tex> bldgGlow_;
  std::unordered_map<uint64_t, std::vector<Vec2>> bldgWin_;   // window centres in sprite pixels (night light pools)
  std::unordered_map<uint64_t, std::vector<uint32_t>> wallKeyCache_;   // per map id, so leaving a house is free
  const Tex& wallTileTex(View& v, uint32_t key);
  // M3b forts (render.cpp, FORTIFICATIONS lane): each settlement's walls, towers and gate in its culture's parts
  //      (bld::fortParts). A wall key carries a look slot (art::WALL_LOOK_*) into fortLooks_ (slot 0: the style's
  //      defaults), rebuilt with the wall keys per window and cached with them.
  std::vector<bld::FortParts> fortLooks_;
  std::unordered_map<uint64_t, std::vector<bld::FortParts>> fortLookCache_;
  uint64_t wallTexKey(uint32_t key) const;
  // the parts of a settlement's fortifications (false: no culture known, the M3 look)
  bool fortPartsOfSite(const Game& g, int site, bld::FortParts& out) const;
  const Tex& fortGateTex(View& v, const cult::Heraldry& arms, const bld::FortParts& f);
  // the culture of the land at a tile (its settlement's, else its region's: World::cultureAtTile), for the wayside's
  // built props (signposts, toll posts, graves, standing stones), cached per 8 x 8-tile block
  const art::PropStyle* landStyleAt(View& v, const Game& g, int tx, int ty);
  std::unordered_map<uint64_t, art::PropStyle> landStyle_;
  uint64_t landStyleWorld_ = 0;
  uint64_t bldgKey(const Map& m, const Bldg& b, int index) const;
  // M2: a building sprite painted off the main thread (an arrival's buildings, desktop), then stored as textures
  struct BldgPaint { uint64_t key = 0; Canvas c, night, glow; bool anyGlass = false; std::vector<Vec2> smoke, wins; int topRow = 0; };
  static BldgPaint paintBldg(const Bldg& b, uint64_t key);
  // the facts the view reads off a painted sprite (smoke, top row, the lit-window variant and its window centres)
  static BldgPaint paintBldgPost(Canvas c, art::BuildingInfo& info, uint64_t key, uint32_t seed);
  const Tex& storeBldg(View& v, BldgPaint& p);
  std::vector<std::future<BldgPaint>> bldgAsync_;
  std::vector<uint64_t> bldgAsyncKeys_;
  // M3 (owner carry-over 1: the unsplittable ~41 ms palace paint): building sprites painted on the main thread go
  // through art::BuildingJob in steps within a budget (the walk-in prefetch, paints behind a fade, the web's arrival),
  // so a palace is spread over frames. bldgPaintStep: true once the sprite is stored.
  struct BldgJob { uint64_t key = 0; std::shared_ptr<art::BuildingJob> job; uint32_t seed = 0; float used = 0; };
  std::vector<BldgJob> bldgJobs_;
  bool bldgPaintStep(View& v, const Bldg& b, int index, double budgetMs);
  Tex blankTex_;                   // drawn for a building still being painted behind the web arrival's fade
  double worstPaintStepMs_ = 0;    // EMB_TIMING: the longest single paint step on the main thread
  // M3 culture-styled props: the PropStyle of the settlement a tile lies in (cached per site), and the styled
  // textures keyed on (prop, style key, variant)
  const art::PropStyle* propStyleAt(View& v, const Game& g, int tx, int ty);
  // (M3 fixer) keyed by the settlement's stable id (Site::id), not its index: loading a save in the same session
  // rebuilds World::sites in another order, and an index-keyed cache drew one town's props in another's style
  std::unordered_map<uint64_t, art::PropStyle> siteProps_;   // Site::id -> its culture's props (classic: culture 0)
  std::unordered_map<uint64_t, uint64_t> propSiteBlock_;     // 8 x 8-tile block (global) -> Site::id there (0: none)
  uint64_t sitePropsWorld_ = 0;
  std::unordered_map<uint64_t, Tex> styledProps_;
  const Tex& styledPropTex(View& v, art::Prop p, const art::PropStyle& st);
  // M3 heraldry: banners and gatehouses in a kingdom's arms (cult::Heraldry), cached per arms
  const Tex& heraldryTex(View& v, int kind, const cult::Heraldry& h, uint32_t seed, int wallStyle);
  std::unordered_map<uint64_t, Tex> homeTex_;   // the lane's sprite cache (crops, objects, horses...), keyed as it likes
  // M6 Steel: ranked foes in the world (render.cpp, BEASTS lane): the aura sprite cache (art::auraSprite by colour
  //      and width)
  std::unordered_map<uint64_t, Tex> auraTex_;
  const Tex& auraTex(View& v, uint32_t color, int w);
  std::unordered_map<uint64_t, Tex> armsTex_;
  const Tex& armsTex(View& v, const cult::Heraldry& h, int size);
  // the realm in the world (realm_render.cpp): a war prop's sprite in its camp's colours (nullptr: the plain one), the
  // fire and smoke over burned buildings, the camp fires' light pools at night
  const Tex* warPropTex(View& v, Game& g, art::Prop p, int tx, int ty);
  std::unordered_map<uint64_t, Tex> warPropTex_;
  // pose sheets (art::humanPostureSheet) by (look key x posture), baked on demand within a per-frame budget (a pose not
  // baked yet is drawn from the standing sheet meanwhile); least-recently-used trimming with the character sheets
  std::unordered_map<uint64_t, Tex> poseTex_;
  std::unordered_map<uint64_t, float> poseUsed_;
  float poseFrameT_ = -1;
  double poseFrameMs_ = 0;
  // variant: the bard's instrument in his people's style (art::humanPostureSheet(look, p, variant); 0 the default)
  const Tex* poseTex(View& v, const art::HumanLook& L, art::Posture p, uint8_t variant = 0, bool child = false);   // nullptr: not baked yet
  // a sleeper fitted to a berth (art::sleeperSprite), cached with the pose sheets and baked within the same budget
  const Tex* sleeperTex(View& v, const art::HumanLook& L, art::Berth b, int kit, int frame);
  bool poseBudget(View& v);         // true: another pose may be baked this frame (and starts its timing)
  void poseSpent(View& v, std::chrono::steady_clock::time_point t0);
  bool rigBakeNow_ = false;
  std::unordered_set<uint64_t> rigEvicted_;   // (P2c SWITCH) atlases the cap evicted (a later bake of one counts as a rebake)   // (P2c SWITCH) a menu figure's atlas bakes in the frame it is asked for (no v1 frame first)
  // (P2c SWITCH) the box of a look's menu clips round the pivot (px: left, up, right, down), by look key x gait set
  struct MenuBox { int l = 0, u = 0, r = 0, d = 0; };
  std::unordered_map<uint64_t, MenuBox> menuBoxes_;
  // (P2c SWITCH) a doll portrait's glow pixels (bust px) by look key, for portraitFinish
  std::unordered_map<uint64_t, std::vector<art::finish::GlowPx>> portraitGlow_;
  std::unordered_map<uint32_t, std::pair<uint64_t, int>> portraitLook_;   // member id -> (bust key, the look's top tier)
  // v2 atlases by (look key x part x child): the canvas is baked and dropped, its frames kept (LENS: MB caps, bake queue)
  struct RigEntry { Tex tex; art::RigSheet sheet; float used = 0; Tex glowTex; std::vector<art::finish::Mask> masks; };   // (ladder) glowTex: the glow plane
  std::unordered_map<uint64_t, RigEntry> rigTex_;
  // (Witness) each v1 standing sheet's opaque box per cell (state.player.sprite.box), by humans_ key; filled only for a
  // harness run
  struct CellBox { int8_t x0 = 0, y0 = 0, x1 = -1, y1 = -1; };
  std::unordered_map<uint64_t, std::vector<CellBox>> humanBoxes_;
  // the night lift's interior masks: per people texture (by its SDL texture), white where the figure is opaque inside its
  // ink outline (the outline stays dark: the edge contrast holds while the body's median rises)
  // (kept as bytes when the sheet is painted; baked into a texture the first night it is needed)
  struct LiftMask { int w = 0, h = 0; std::vector<uint8_t> m; Tex tex; };
  std::unordered_map<SDL_Texture*, LiftMask> liftMask_;
  void liftMaskFor(View& v, const Canvas& sheet, const Tex& t);
  void dropLiftMask(View& v, const Tex& t);
  struct RigJob { art::HumanLook look; art::RigPart part = art::RigPart::Loco; bool child = false; float d2 = 0; };
  std::vector<RigJob> rigQueue_;
  float rigQueueT_ = 0;   // when rigBakeQueue last rebuilt the queue (a member: a Witness reset's new View starts it over)
  float rigBakeFrameT_ = -1;
  double rigBakeFrameMs_ = 0;
  bool rigBakeBudget(View& v);
  RigTex rigPoseTex(View& v, const art::HumanLook& L, art::Posture p, uint8_t variant);
  void rigBakeQueue(View& v, const Game& g);
  void trimRigCaches(View& v);
  std::unordered_map<uint64_t, Tex> critterTex_;
  const Tex& critterTex(View& v, int kind, uint32_t variant);
  std::unordered_map<uint64_t, Tex> lampDark_;   // a lamppost by day / before the lamplighter comes (its glass dark)
  const Tex& lampDarkTex(View& v, const art::PropStyle* ps);
  std::unordered_map<uint64_t, Tex> festTex_;    // bunting by (width, colours), lanterns by colour
  std::unordered_map<uint64_t, Tex> partyTex_; // portraits, the hound's sheets, graves, the radial's wedges and icons
  const Tex& portraitTex(View& v, const Game& g, const party::Member& m, int mood);
  const Tex& houndTex(View& v, const party::Member& m, const Game& g);
  Tex beamTex_;                                    // a soft vertical gradient (white; tinted additively per rarity)
  const Tex& beamTex(View& v);                            // (baked on first use: loot_paint.h beamProfile)
  const Tex* mythicAuraTex(View& v, const art::HumanLook& L);
  std::unordered_map<uint64_t, Tex> mythicAura_;
};
