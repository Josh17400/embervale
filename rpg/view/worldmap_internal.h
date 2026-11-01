// worldmap_internal.h - PRIVATE to rpg/view/worldmap.cpp and rpg/view/worldmap_markers.cpp (SOLID 3.4d): the world
// map's types, palette and helpers both files use, and one frame of View::drawWorldMap (MapFrame). No other file
// includes it. Everything here is defined in worldmap.cpp (namespace wmap) or is a constant / inline.
#pragma once
#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "rpg/view/view.h"
#include "rpg/world/settlement.h"
#include "rpg/world/source.h"

namespace wmap {
constexpr int TS = 64;   // map pixels per cached tile side
constexpr int MG = 8;    // sample margin round a tile (coast rings, glyphs that cross a tile edge)
const float kZoom[] = {0.25f, 0.5f, 1, 2, 4, 8, 16, 32};
constexpr int NZOOM = (int)(sizeof kZoom / sizeof kZoom[0]);
const Color kGold(0.98f, 0.82f, 0.42f), kText(0.93f, 0.9f, 0.82f), kDim(0.62f, 0.58f, 0.52f), kInk(0.18f, 0.12f, 0.08f);

inline uint32_t C(int r, int g, int b, int a = 255) { return rgba(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255), a); }
inline uint32_t mixc(uint32_t a, uint32_t b, float t) {
  auto ch = [&](int s) { return (int)(((a >> s) & 255) + ((int)((b >> s) & 255) - (int)((a >> s) & 255)) * t); };
  return C(ch(0), ch(8), ch(16));
}
inline uint32_t shadec(uint32_t a, float k) { return C((int)((a & 255) * k), (int)(((a >> 8) & 255) * k), (int)(((a >> 16) & 255) * k)); }

// ---- the parchment palette
const uint32_t kParch = C(218, 198, 156), kParchDark = C(190, 166, 124), kInkC = C(66, 48, 38), kInkSoft = C(112, 86, 64);
const uint32_t kSea = C(150, 178, 186), kSeaDeep = C(120, 152, 170), kRiver = C(70, 112, 156), kRipple = C(196, 210, 204);

enum Icon {
  IC_CITY, IC_TOWN, IC_VILLAGE, IC_CAVE, IC_RUIN, IC_SHARD, IC_CAMP, IC_SHRINE, IC_LAIR,
  IC_V0,                                  // + VignetteKind (9)
  IC_W0 = IC_V0 + (int)ew::VignetteKind::COUNT,   // + WonderKind (4)
  IC_CAPITAL = IC_W0 + (int)ew::WonderKind::COUNT,
  IC_SUMMIT,                              // (M11 CLIMB) a summit shrine (SPF_SUMMIT): a snowy peak crowned by its shrine
  IC_COUNT
};
// Pixel icons: '.' clear, k ink, w light, s stone, d dark, r red, R roof, g gold, G green, L leaf light, b brown,
// B blue, c cyan, p purple, o orange, y yellow, W white
struct IconArt { const char* name; std::vector<const char*> rows; };

// (M4) the war and news markers on the map (VISION_PLAN 2.11, 4.5): crossed swords at a siege, flames at a burned
// place, a tent at a refugee camp, a bowl at a hungry town, a scroll at a heard event; an occupied town flies a little
// banner in its new owner's colours (drawn live, beside these)
enum WarIcon { WI_SIEGE, WI_BURNED, WI_REFUGEES, WI_NEWS, WI_HUNGRY, WI_COUNT };

// ---- (M12) the rise's markers: drawn live (a few rects each), about 9 x 9 at (x, y)
enum RiseMark { RM_WARBAND, RM_WARLORD, RM_LAIR, RM_REBUILT, RM_HOLDING, RM_SEAT, RM_COUNT };
const char* const kRiseMarkName[RM_COUNT] = {"WARBAND CAMP", "WARLORD'S CAMP", "BEAST'S LAIR", "RESETTLED", "YOUR HOLDING", "YOUR SEAT"};

// one map tile being painted: its samples are taken a few rows per frame (a tile can take 10 - 20 ms), then finished
struct MapSmp {
  uint32_t c = 0;
  float e = 0;                 // elevation (macro, 0..1)
  uint8_t h = 0;               // relief level
  bool water = false, deep = false, known = false, bldg = false, wall = false, tree = false, edgeB = false, road = false;
  uint64_t kingdom = 0;
  Biome bio = Biome::Plains;
  uint32_t prov = 0;
  uint8_t wd = 255;            // distance (samples) to the nearest land, over water (coast ripples)
  uint32_t roofC = 0;          // (street zoom) the building's roof colour
  uint8_t eco = 0;             // (M4) the biome proper (flower meadows are speckled with blooms)
  uint8_t mist = 0;            // (M10) in a hidden land's Mist: 1 uncharted (thick fog), 2 charted (a thin ring)
  uint8_t mistD = 0;           // (M10) ... its density there (0 at the ring's edges .. 255 in its thick)
  // (M11 CLIMB) the window's real relief (street and town zooms): the step bits of the sample's tiles (a sample over
  // several tiles ORs them, so a face one tile thick never breaks into dashes), the side its higher ground lies on (1
  // north, 2 east, 3 west, 4 south; 0 none) and whether a face there climbs two levels or more (a stacked face)
  uint8_t rb = 0, up = 0;
  bool stack = false;
  // (M7c SURVEY) a settlement's plan: the building's identity (row houses read as separate roofs), its height (the
  // length of its shade), a wall tower, a hedge or garden wall, and that the sample comes from a plan (the region
  // plans' road and river lines and the wood glyphs keep off it)
  int32_t bi = -1;
  uint8_t tall = 1;
  bool tower = false, hedge = false, plan = false;
  uint8_t hedgeKind = 0;       // (hedgeProp: 1 hedge, 2 fence, 3 garden wall)
};
struct MapGRect { int32_t x0, y0, x1, y1; };

// ---- (M7c SURVEY) a settlement's real plan, compact (about 6 bytes a tile: a capital's ~300 x 300 buffer is ~0.5 MB)
enum : uint8_t { PF_WALL = 1, PF_TOWER = 2, PF_GATE = 4, PF_TREE = 8, PF_HEDGE = 16, PF_USED = 32, PF_FENCE = 64, PF_STONE = 128 };
struct PlanRaster {
  ew::Gid id = 0;
  int32_t gx = 0, gy = 0;          // the global tile of (0, 0)
  int w = 0, h = 0;
  std::vector<uint8_t> g, u, f, r; // per tile: Ground, urb class, PF_* flags, Rank | BlockRole << 3
  std::vector<int16_t> b;          // per tile: the building (index into roof / tall) or -1
  std::vector<uint32_t> roof;      // per building: its roof colour
  std::vector<uint8_t> tall;       // per building: its storeys
  float used = 0;                  // (LRU)
  bool in(int32_t x, int32_t y) const { return x >= gx && y >= gy && x < gx + w && y < gy + h; }
  size_t I(int32_t x, int32_t y) const { return (size_t)(y - gy) * (size_t)w + (size_t)(x - gx); }
};

struct TileJob {
  bool on = false;
  int zi = 0, ix = 0, iy = 0;
  bool geo = false;
  std::vector<MapSmp> grid;
  std::vector<MapGRect> towns, cores;
  std::vector<std::shared_ptr<const PlanRaster>> plans;   // (M7c) the settlement plans ready over this tile
  std::vector<uint8_t> corePlanned;                        // ... per core: its plan is ready (no stand-in lattice)
  int row = 0;
  std::unordered_set<uint64_t> moved;      // (M4) genesis kingdoms some of whose settlements another realm holds now
  std::unordered_map<uint64_t, uint32_t> kcol;   // (M4) owner -> its border colour
  // (M12) owner -> how its border is drawn: 1 a warlord's (rough), 2 a vassal's (its liege's colour hatched inside),
  // 3 the player's own (a gilded double line), 0 plain; ksub: the liege's colour for a vassal
  std::unordered_map<uint64_t, uint8_t> kstyle;
  std::unordered_map<uint64_t, uint32_t> ksub;
  // (M10) the lands round this tile (hidden ones' Mist rings) and which land a coarse cell lies on
  std::vector<ew::LandPlan> lands;
  bool landsDone = false;
  std::unordered_map<uint64_t, uint32_t> landCell;
};

struct MapState {
  bool init = false;
  uint64_t world = 0;            // the world the view belongs to (a new game re-centres and drops the tiles)
  double cx = 0, cy = 0;         // the centre in global tiles
  int zi = 4;
  uint64_t sig = 0;              // the explored mask's signature the tiles were painted with
  bool geo = false;              // the geology debug view
  struct T { Tex tex; float used = 0, born = -10; bool stale = false; };   // stale: shown until its repaint is done
  Tex blank;                     // (M2 fixer) a tile of bare parchment (grain and hatching) under tiles still painting
  bool blankOn = false;
  std::unordered_map<uint64_t, T> tiles;
  std::vector<Tex> icons;        // kIcons, baked once
  Tex arrow[8];                  // the hero's arrow in 8 directions
  // landmarks of the explored regions (copied from the region plans, fetched a few per frame)
  std::unordered_map<uint64_t, std::vector<ew::LandmarkPlan>> lms;
  // pointers: one finger / the mouse drags, two fingers pinch
  struct Ptr { uint64_t id; Vec2 p, start; };
  std::vector<Ptr> ptrs;
  float pinchD = 0;
  int pinchZ = 4;
  bool moved = false;
  float rx = 0, ry = 0, rw = 1, rh = 1;   // the map rectangle drawn last (box coordinates)
  std::vector<int> legend;                 // icons on screen this frame
  std::vector<int> ecoLeg;                 // (M3c) the biomes of the known land in view, most widespread first
  double ecoCx = 1e30, ecoCy = 1e30;       // ... counted for this centre and zoom
  int ecoZi = -1;
  uint64_t ecoSig = 0;
  int reqSel = -2;                         // a selection asked for by a script (mapselect)
  double lastPaintMs = 0, worstPaintMs = 0;
  int tilesPainted = 0;
  TileJob job;                             // the tile being painted (resumed next frame)
  long borderPx = 0;                       // (M4) border pixels painted so far (scripts: expect m4 border)
  int markers = 0;                         // (M4) war and news markers drawn last frame
  std::vector<Tex> warIcons;               // (M4) the war markers (kWarIcon order)
  // (fixer M4 r2) the news scrolls drawn last frame (event ids, screen centres) and the one tapped (0 none): a tap on a
  // scroll shows its news in the side column
  std::vector<std::pair<uint32_t, Vec2>> news;
  uint32_t newsSel = 0;
  // (M7c SURVEY) the settlement plans (LRU), the places that have none, and what making them cost
  std::unordered_map<ew::Gid, std::shared_ptr<PlanRaster>> plans;
  std::unordered_set<ew::Gid> noPlan;
  ew::Gid planBusy = 0;                    // the place being prepared (its build spans frames)
  double planWorstMs = 0, planLastMs = 0;  // the worst frame's plan work, the last one's
  int plansMade = 0;
  bool forcePlan = false;                  // (scripts: mapplan 1) the plan even inside the Active Window
  // (M7c) the region plans the tiles have asked for (a hint: the source caches them), the frame counter, and the frame
  // the last one was made in (one a frame)
  std::unordered_set<uint64_t> regSeen;
  uint64_t frame = 0, regFrame = ~0ull;
  double regWorstMs = 0, finishWorstMs = 0;   // the worst region plan a tile asked for, the worst tile's last pass
};
extern MapState S;
Color colOf(uint32_t c, float a = 1);
void mapPlayerG(Game& g, double& gx, double& gy);
void riseMapGlyph(Pix& P, int kind, float x, float y, uint32_t c1, uint32_t c2);   // (M12) a rise's marker, about 9 x 9 at (x, y)

// ---- one frame of the world map (View::drawWorldMap's shared locals): the rectangle, the zoom and the view's top-left
//      map pixel, the labels queued (placed last, most important first) and the boxes they keep off, the places' icons,
//      the hero's spot, the tracked quest's pin and the other quests' diamonds
// labels are placed after the markers, most important first; one that would overlap a placed label (or a marker) tries
// its other position and is otherwise left out
struct Lab { int prio; float x, y, alt; std::string s; Color ink, halo; bool spaced; std::string sub = std::string(); Color subInk = Color(); bool realm = false; };   // (M12) sub: a second, smaller line under it
struct Box { float x0, y0, x1, y1; };
struct Mk { float x, y; int ic; int site; };
struct MapFrame {
  Pix& P;
  float x, y, w, h;                               // the map rectangle (box coordinates)
  std::chrono::steady_clock::time_point t0;       // when this frame's map draw began
  uint64_t sig;                                   // what the tiles were painted with (exploredSig and the rest)
  float z;                                        // tiles per map pixel (kZoom)
  double left, top;                               // the map pixel at the rectangle's top-left
  std::vector<Lab> labs;
  std::vector<Box> taken;
  std::vector<Box> warBoxes;   // (fixer M4 r3) the war's markers and the news scrolls: labels keep off them
  std::vector<Mk> marks;       // the places' icons kept this frame
  Vec2 hp;                     // the hero on screen
  bool hasQ = false;           // the tracked quest's destination (global tiles) and its pin on screen
  double qgx = 0, qgy = 0;
  Vec2 qs;
  std::vector<Vec2> otherQ;    // the other open quests' diamonds
  Vec2 toScr(double gx, double gy) const { return Vec2((float)(gx / z - left), (float)(gy / z - top)); }
  void legendAdd(int ic) const { if (std::find(S.legend.begin(), S.legend.end(), ic) == S.legend.end()) S.legend.push_back(ic); }
};
}  // namespace wmap
