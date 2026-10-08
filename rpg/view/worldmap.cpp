// The world map (M1, VISION_PLAN 2.11; M2 Wayfinder: cartography at every zoom). An endless world needs a map that pans
// and zooms over global tiles, from the street around the hero out to whole continents. The minimap stays on the Active
// Window (hud.cpp). This file owns the whole MAP tab: the map, its side column (the selected place and the journey),
// the legend and the travel buttons.
//   - zoom: 8 levels in four groups (0.25 .. 32 tiles per map pixel); pinch / wheel / Q E / the + - buttons
//       Z0 STREET     (0.25, 0.5)  the real tiles of the window: streets, roofs, walls, trees
//       Z1 TOWN       (1, 2)       explored chunk data inside the window, macro samples and the region plans' roads
//                                  and rivers outside it
//       Z2 LAND       (4, 8)       macro samples, the region plans' roads and rivers, mountain and forest glyphs
//       Z3 CONTINENT  (16, 32)     the far macro field, mountain glyphs, the great landmarks
//   - drawn as a map, not a picture: hillshade from the macro elevation (north-west light), faint contours every two
//     relief levels, hand-inked coasts with ripple lines offshore, rivers and roads as cartographic lines, little
//     snow-tipped peaks on the ridge crests, tree marks in the woods, kingdom borders, labels from the region plans'
//     landmarks (sized by zoom and landmark size, never overlapping), parchment where the hero has not been
//   - markers: the hero's arrow, the tracked quest (Quest::tgx/tgy, else questTarget), an icon for every discovered
//     place by its type (and every vignette and wonder kind), rumoured places as a dashed "?" circle near (not on) where
//     they lie; the legend lists only what is on screen
//   - the side column: the selected place (name, kind, kingdom, level) and the journey (Game::travelQuote on foot and
//     by carriage: hours, fare, or why not) with FAST TRAVEL and CARRIAGE (Game::beginTravel)
//   - a geology debug view (key G, script `mapgeo 1`): provinces coloured by rock, the primary ore's initial, borders
// The map is painted in 64 x 64-pixel tiles cached per zoom level (a quadtree-like cache keyed by zoom and tile), a
// few per frame within a time budget, so the phone never stalls; tiles not painted yet show plain parchment.
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "rpg/world/biomes.h"
#include <vector>
#include "rpg/story/story.h"
#include "rpg/view/realm_ui.h"
#include "rpg/view/view.h"
#include "rpg/world/source.h"

namespace {
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
Color colOf(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }

// ---- the parchment palette
const uint32_t kParch = C(218, 198, 156), kParchDark = C(190, 166, 124), kInkC = C(66, 48, 38), kInkSoft = C(112, 86, 64);
const uint32_t kSea = C(150, 178, 186), kSeaDeep = C(120, 152, 170), kRiver = C(70, 112, 156), kRipple = C(196, 210, 204);

enum Icon {
  IC_CITY, IC_TOWN, IC_VILLAGE, IC_CAVE, IC_RUIN, IC_SHARD, IC_CAMP, IC_SHRINE, IC_LAIR,
  IC_V0,                                  // + VignetteKind (9)
  IC_W0 = IC_V0 + (int)ew::VignetteKind::COUNT,   // + WonderKind (4)
  IC_CAPITAL = IC_W0 + (int)ew::WonderKind::COUNT,
  IC_COUNT
};
// Pixel icons: '.' clear, k ink, w light, s stone, d dark, r red, R roof, g gold, G green, L leaf light, b brown,
// B blue, c cyan, p purple, o orange, y yellow, W white
struct IconArt { const char* name; std::vector<const char*> rows; };
const IconArt kIcons[IC_COUNT] = {
    {"CITY", {"k.k...k.k", "kwk.k.kwk", "kwkkwkkwk", "kwwwwwwwk", "kwwkkkwwk", "kwwkdkwwk", "kkkkkkkkk"}},
    {"TOWN", {"...k.....", "..kRk.k..", ".kRRRkRk.", "kRRRRkRRk", "kwkwkwwwk", "kwkwkwkwk", "kkkkkkkkk"}},
    {"VILLAGE", {"..k..", ".kRk.", "kRRRk", "kwkwk", "kwkwk", "kkkkk"}},
    {"CAVE", {"..kkkkk..", ".ksssssk.", "kssdddssk", "ksdkkkdsk", "kskkkkksk", "kkkkkkkkk"}},
    {"RUIN", {"k.......k", "wk..k..kw", "wk.kw..kw", "wk.kw.kkw", "wk.kw.kwk", "kkkkkkkkk"}},
    {"EMBER SHARD", {"...k...", "..kok..", ".koyok.", "koyWyok", ".koyok.", "..kok..", "...k..."}},
    {"BANDIT CAMP", {"....kr..", "....krr.", "....k...", "...kwk..", "..kwwwk.", ".kwwkwwk", "kkkkkkkk"}},
    {"SHRINE", {"...k...", "..kck..", ".kcWck.", "..kck..", "..kBk..", ".kkkkk.", "kssssk."}},
    {"DRAGON LAIR", {"k.....k", "kr...rk", ".krrrk.", "krWrWrk", "krrrrrk", ".krkrk.", "..k.k.."}},
    // vignettes (ew::VignetteKind order)
    {"OVERTURNED CARAVAN", {"..kkk..", ".kbkbk.", "kbkkkbk", "kkkwkkk", "kbkkkbk", ".kbkbk.", "..kkk.."}},
    {"HUNTER'S CAMP", {"...k....", "..kbk...", ".kbbbk.k", "kbbkbbkb", "kbkdkbk.", "kkkkkkk."}},
    {"STANDING STONES", {".k...k.", "ksk.ksk", "ksk.ksk", "kskkksk", "ksksksk", "kkkkkkk"}},
    {"RUINED WATCHTOWER", {"k.k.k", "kskkk", "kssk.", "ksdsk", "ksssk", "kkkkk"}},
    {"FISHING HUT", {"..k...", ".kRk..", "kRRRk.", "kwdwk.", "kkkkk.", "BBBBBB"}},
    {"TOLL BRIDGE", {"kkkkkkkkk", "ksssssssk", "ksk...ksk", "kk.BBB.kk", "BBBBBBBBB"}},
    {"HERB GARDEN", {".GkGkG.", "kLkLkLk", "kGkGkGk", "bbbbbbb", "kkkkkkk"}},
    {"LONE GRAVE", {"..k..", ".kwk.", "kwwwk", ".kwk.", ".kwk.", "kkkkk"}},
    {"WAYFARERS' REST", {"...o...", "..oyo..", ".koyok.", "kbbbbbk", ".k.k.k."}},
    // wonders (ew::WonderKind order): gold-rimmed
    {"ELDER TREE", {"..kkkkk..", ".kGLLGGk.", "kGLGGGGGk", "kGGGGGGGk", ".kkGbGkk.", "...kbk...", "..kbbbk.."}},
    {"COLOSSUS", {"..kgk..", ".kwwwk.", "..kwk..", ".kwwwk.", "kwkwkwk", ".kwkwk.", "kkkkkkk"}},
    {"STARFALL", {"....W....", "...kck...", "k.kcWck.k", ".kcWWWck.", "kcWWyWWck", ".kcWWWck.", "k.kcWck.k", "...kck...", "....W...."}},
    {"DRAGON BONES", {"kk.......", "kwk.k.k.k", "kwwkwkwkw", ".kwwwwwww", "..kkkkkkk"}},
    {"CAPITAL", {"g.g.g", "ggggg"}},
};
uint32_t iconCol(char ch) {
  switch (ch) {
    case 'k': return C(52, 36, 30);
    case 'w': return C(238, 228, 204);
    case 's': return C(150, 142, 132);
    case 'd': return C(70, 60, 58);
    case 'r': return C(196, 52, 40);
    case 'R': return C(170, 74, 52);
    case 'g': return C(236, 186, 64);
    case 'G': return C(64, 120, 58);
    case 'L': return C(140, 186, 84);
    case 'b': return C(140, 96, 58);
    case 'B': return C(70, 120, 176);
    case 'c': return C(120, 200, 236);
    case 'p': return C(140, 90, 170);
    case 'o': return C(236, 120, 40);
    case 'y': return C(250, 220, 96);
    case 'W': return C(255, 252, 240);
    default: return 0;
  }
}
Canvas iconCanvas(int ic) {
  const IconArt& a = kIcons[ic];
  int w = 0;
  for (const char* r : a.rows) w = std::max(w, (int)std::char_traits<char>::length(r));
  const int h = (int)a.rows.size();
  const bool wonder = ic >= IC_W0 && ic < IC_W0 + (int)ew::WonderKind::COUNT;
  const int pad = wonder ? 2 : 1;
  Canvas c(w + pad * 2, h + pad * 2);
  // a parchment halo so the icon reads over any land (a gold one round a wonder)
  for (int y = 0; y < h; y++)
    for (int x = 0; x < (int)std::char_traits<char>::length(a.rows[(size_t)y]); x++) {
      if (a.rows[(size_t)y][x] == '.') continue;
      for (int oy = -pad; oy <= pad; oy++)
        for (int ox = -pad; ox <= pad; ox++)
          if (std::abs(ox) + std::abs(oy) <= pad && !(c.get(x + pad + ox, y + pad + oy) >> 24))
            c.set(x + pad + ox, y + pad + oy, wonder ? (std::abs(ox) + std::abs(oy) == pad ? C(150, 96, 30, 230) : C(246, 214, 120, 235)) : C(236, 222, 186, 200));
    }
  for (int y = 0; y < h; y++)
    for (int x = 0; x < (int)std::char_traits<char>::length(a.rows[(size_t)y]); x++)
      if (uint32_t col = iconCol(a.rows[(size_t)y][x])) c.set(x + pad, y + pad, col);
  return c;
}
int siteIcon(const Site& s) {
  switch (s.type) {
    case SiteType::City: return IC_CITY;
    case SiteType::Town: return IC_TOWN;
    case SiteType::Village: return IC_VILLAGE;
    case SiteType::Cave: return IC_CAVE;
    case SiteType::Ruin: return s.mainQuest && !s.cleared ? IC_SHARD : IC_RUIN;
    case SiteType::BanditCamp: return IC_CAMP;
    case SiteType::Shrine: return IC_SHRINE;
    case SiteType::DragonLair: return IC_LAIR;
    case SiteType::Vignette: return IC_V0 + std::clamp((int)s.kind, 0, (int)ew::VignetteKind::COUNT - 1);
    case SiteType::Wonder: return IC_W0 + std::clamp((int)s.kind, 0, (int)ew::WonderKind::COUNT - 1);
    default: return IC_RUIN;
  }
}

// (M4) the war and news markers on the map (VISION_PLAN 2.11, 4.5): crossed swords at a siege, flames at a burned
// place, a tent at a refugee camp, a bowl at a hungry town, a scroll at a heard event; an occupied town flies a little
// banner in its new owner's colours (drawn live, beside these)
enum WarIcon { WI_SIEGE, WI_BURNED, WI_REFUGEES, WI_NEWS, WI_HUNGRY, WI_COUNT };
const IconArt kWarIcons[WI_COUNT] = {
    {"SIEGE", {"kk.....kk", "kWk...kWk", ".kWk.kWk.", "..kWkWk..", "...kWk...", "..kWkWk..", ".kbkkkbk.", "kbbk.kbbk", "kkk...kkk"}},
    {"BURNED", {"...k....", "..kyk...", "..koyk..", ".kroyok.", ".koyWyk.", "kroyWyok", "kroyyork", ".krrrrk.", "..kkkk.."}},
    {"REFUGEE CAMP", {"....k....", "...kwk...", "..kwbwk..", ".kwwbwwk.", "kwwkdkwwk", "kwwkdkwwk", "kkkkkkkkk"}},
    {"NEWS HEARD", {".kkkkk.", "kwwwwwk", "kwkkkwk", "kwwwwwk", "kwkkwwk", "kwwwwwk", ".kkkkk."}},
    {"HUNGRY", {"k.......k", "kbwwwwwbk", ".kbwwwbk.", "..kbbbk..", "...kkk..."}},
};
Canvas warIconCanvas(int wi) {
  const IconArt& a = kWarIcons[wi];
  int w = 0;
  for (const char* r : a.rows) w = std::max(w, (int)std::char_traits<char>::length(r));
  const int h = (int)a.rows.size(), pad = 1;
  Canvas c(w + 2, h + 2);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < (int)std::char_traits<char>::length(a.rows[(size_t)y]); x++) {
      if (a.rows[(size_t)y][x] == '.') continue;
      for (int oy = -pad; oy <= pad; oy++)
        for (int ox = -pad; ox <= pad; ox++)
          if (std::abs(ox) + std::abs(oy) <= pad && !(c.get(x + pad + ox, y + pad + oy) >> 24)) c.set(x + pad + ox, y + pad + oy, C(240, 226, 190, 215));
    }
  for (int y = 0; y < h; y++)
    for (int x = 0; x < (int)std::char_traits<char>::length(a.rows[(size_t)y]); x++)
      if (uint32_t col = iconCol(a.rows[(size_t)y][x])) c.set(x + pad, y + pad, col);
  return c;
}

// ---- the colours of the land
uint32_t groundInk(Ground gr) {
  switch (gr) {
    case Ground::DeepWater: return kSeaDeep;
    case Ground::Water: return kSea;
    case Ground::Sand: return C(224, 204, 152);
    case Ground::Snow: return C(238, 238, 232);
    case Ground::Ice: return C(214, 230, 236);
    case Ground::Rock: return C(156, 146, 130);
    case Ground::Road: case Ground::Bridge: return C(150, 118, 82);
    case Ground::Plaza: return C(184, 168, 140);
    case Ground::Swamp: return C(134, 140, 100);
    case Ground::ForestFloor: return C(124, 150, 98);
    case Ground::Autumn: return C(190, 152, 98);
    case Ground::Tundra: return C(160, 166, 136);
    case Ground::Farmland: return C(178, 158, 108);
    case Ground::Dirt: return C(176, 152, 112);
    case Ground::Meadow: return C(176, 190, 122);
    default: return C(168, 182, 120);
  }
}
uint32_t biomeInk(Biome b) {
  switch (b) {
    case Biome::Ocean: return kSeaDeep;
    case Biome::Beach: return C(226, 208, 160);
    case Biome::Forest: return C(126, 150, 98);
    case Biome::Autumn: return C(192, 154, 100);
    case Biome::Taiga: return C(134, 150, 122);
    case Biome::Snow: return C(238, 238, 234);
    case Biome::Swamp: return C(134, 140, 100);
    case Biome::Desert: return C(228, 204, 150);
    case Biome::Mountain: return C(176, 162, 140);
    default: return C(170, 184, 122);
  }
}
// (M3c LIFE) a biome's ink on the parchment: its map colour (biomes.h ecoInfo rgb) toned halfway to its family's ink,
// so the savanna, the heath and the badlands read apart without the map turning garish
uint32_t ecoInk(Eco e) {
  const EcoInfo& I = ecoInfo(e);
  if (e == Eco::Ocean) return kSeaDeep;
  // (M3c fixer, review: the wooded biomes' swatches were one green) mixed less toward the family, so the biomes of one
  // family (forest, bamboo, jungle) stay apart on the map and in its legend
  return mixc(mixc(biomeInk(I.family), C(I.r, I.g, I.b), ecoHas(e, EF_RARE) ? 0.88f : 0.80f), kParch, 0.12f);
}
// the geology view's rock colours (rpg/world/geology.h Rock order)
uint32_t rockInk(ew::Rock r) {
  static const uint32_t k[] = {C(196, 150, 146), C(92, 96, 112), C(226, 218, 182), C(222, 160, 96), C(126, 140, 168), C(240, 238, 232)};
  return k[std::clamp((int)r, 0, 5)];
}

// the geology view's ore symbols (two letters, one colour each: copper and coal share an initial)
const char* const kOreSym[] = {"CU", "SN", "FE", "CO", "AG", "RA"};
const Color kOreCol[] = {Color(0.95f, 0.55f, 0.30f), Color(0.80f, 0.84f, 0.88f), Color(0.86f, 0.38f, 0.30f), Color(0.62f, 0.62f, 0.66f),
                         Color(0.92f, 0.94f, 1.0f), Color(1.0f, 0.45f, 1.0f)};
static_assert(sizeof(kOreSym) / sizeof(kOreSym[0]) == (size_t)ew::Ore::COUNT, "a symbol for every ore");

uint64_t tileKey(int zi, int ix, int iy) { return ((uint64_t)(uint32_t)zi << 56) ^ ((uint64_t)(uint32_t)(ix & 0xFFFFFFF) << 28) ^ (uint32_t)(iy & 0xFFFFFFF); }

uint64_t exploredSig(const Game& g) {
  uint64_t h = g.explored.regions.size() * 0x9E3779B97F4A7C15ull;
  for (auto& kv : g.explored.regions) {
    uint64_t s = kv.first;
    for (uint8_t b : kv.second) s = s * 1099511628211ull + b;
    h ^= ew::mix64(s);
  }
  return h;
}

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
};
struct MapGRect { int32_t x0, y0, x1, y1; };
struct TileJob {
  bool on = false;
  int zi = 0, ix = 0, iy = 0;
  bool geo = false;
  std::vector<MapSmp> grid;
  std::vector<MapGRect> towns, cores;
  int row = 0;
  std::unordered_set<uint64_t> moved;      // (M4) genesis kingdoms some of whose settlements another realm holds now
  std::unordered_map<uint64_t, uint32_t> kcol;   // (M4) owner -> its border colour
};

struct MapState {
  bool init = false;
  uint64_t world = 0;            // the world the view belongs to (a new game re-centres and drops the tiles)
  double cx = 0, cy = 0;         // the centre in global tiles
  int zi = 4;
  uint64_t sig = 0;              // the explored mask's signature the tiles were painted with
  bool geo = false;              // the geology debug view
  struct T { Tex tex; float used = 0, born = -10; };
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
};
MapState S;

// a tiny polygon fill for the arrow (the view has no art helpers)
void fillTri(Canvas& c, float ax, float ay, float bx, float by, float cx, float cy, uint32_t col) {
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const float px = x + 0.5f, py = y + 0.5f;
      const float d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
      const float d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
      const float d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
      const bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
      if (!(neg && pos)) c.set(x, y, col);
    }
}
Canvas arrowCanvas(int dir) {
  Canvas c(13, 13);
  const float a = dir * 0.785398f;   // 0 = north, clockwise
  auto rot = [&](float x, float y, float& ox, float& oy) { ox = 6.5f + x * std::cos(a) - y * std::sin(a); oy = 6.5f + x * std::sin(a) + y * std::cos(a); };
  float x0, y0, x1, y1, x2, y2, x3, y3;
  rot(0, -6, x0, y0); rot(-4.5f, 4.5f, x1, y1); rot(0, 2, x2, y2); rot(4.5f, 4.5f, x3, y3);
  Canvas body(13, 13);
  fillTri(body, x0, y0, x1, y1, x2, y2, C(250, 246, 236));
  fillTri(body, x0, y0, x2, y2, x3, y3, C(214, 52, 40));
  // ink outline
  for (int y = 0; y < 13; y++)
    for (int x = 0; x < 13; x++) {
      if (body.get(x, y) >> 24) { c.set(x, y, body.get(x, y)); continue; }
      bool edge = false;
      for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if (body.get(x + ox, y + oy) >> 24) edge = true;
      if (edge) c.set(x, y, C(40, 26, 22));
    }
  return c;
}

// the explored edge wanders (a warped look-up of the 8-tile fog cells: no square blocks)
bool seenSoft(const Game& g, int32_t gx, int32_t gy) {
  const float jx = (vnoise(gx / 13.0f, gy / 13.0f, 701) - 0.5f) * 14.0f + (vnoise(gx / 5.0f, gy / 5.0f, 703) - 0.5f) * 4.0f;
  const float jy = (vnoise(gx / 13.0f, gy / 13.0f, 709) - 0.5f) * 14.0f + (vnoise(gx / 5.0f, gy / 5.0f, 711) - 0.5f) * 4.0f;
  return g.explored.seen(gx + (int32_t)std::lround(jx), gy + (int32_t)std::lround(jy));
}
bool isTreeProp(int pr) {
  if (!pr) return false;
  const art::Prop p = (art::Prop)(pr - 1);
  return art::isTreeProp(p) || p == art::Prop::ElderTree;   // (M3c: the Wildlands trees too)
}
}  // namespace

// ---- one cached tile -------------------------------------------------------------------------------------------
// The tile's samples cover it plus a margin of MG map pixels (coasts and glyphs are decided the same way on both sides
// of a tile edge, so tiles meet seamlessly).
static bool paintMapTile(Game& g, TileJob& J, Canvas& out, std::chrono::steady_clock::time_point deadline) {
  using Smp = MapSmp;
  using GRect = MapGRect;
  const int zi = J.zi, ix = J.ix, iy = J.iy;
  const bool geo = J.geo;
  const float z = kZoom[zi];
  const int N = TS + 2 * MG;
  std::vector<Smp>& grid = J.grid;
  const bool endless = g.world.endless && g.world.src != nullptr;
  const Map& m = g.world.over;
  std::vector<GRect>& towns = J.towns;   // discovered settlements: with a margin (shown whole)
  std::vector<GRect>& cores = J.cores;   // ... and their built area
  const int32_t tx0 = (int32_t)std::floor((ix * TS - MG) * z) - 8, ty0 = (int32_t)std::floor((iy * TS - MG) * z) - 8;
  const int32_t tx1 = (int32_t)std::floor(((ix + 1) * TS + MG) * z) + 8, ty1 = (int32_t)std::floor(((iy + 1) * TS + MG) * z) + 8;
  if (J.row == 0 && grid.empty()) {
    grid.assign((size_t)N * N, Smp());
    // (M4) the realms whose land moved, and every owner's border colour
    for (const realm::KingdomState& k : g.realm.kingdoms())
      for (ew::Gid sid : k.settlements)
        if (const realm::SettlementState* st = g.realm.settlement(sid))
          if (st->home && st->home != k.id) J.moved.insert(st->home);
    if (endless)
      for (const Site& st : g.world.sites) {
        if (!st.discovered || !st.settlement()) continue;
        GRect r{g.world.ox + st.r.x - 6, g.world.oy + st.r.y - 6, g.world.ox + st.r.x + st.r.w + 6, g.world.oy + st.r.y + st.r.h + 6};
        if (r.x1 < tx0 || r.y1 < ty0 || r.x0 > tx1 || r.y0 > ty1) continue;
        towns.push_back(r);
        cores.push_back(GRect{r.x0 + 6, r.y0 + 6, r.x1 - 6, r.y1 - 6});
      }
  }
  for (int j = J.row; j < N; j++) {
    if (j > J.row && std::chrono::steady_clock::now() > deadline) { J.row = j; return false; }
    for (int i = 0; i < N; i++) {
      const int mpx = ix * TS + i - MG, mpy = iy * TS + j - MG;
      const int32_t gx = (int32_t)std::floor((mpx + 0.5f) * z), gy = (int32_t)std::floor((mpy + 0.5f) * z);
      Smp s;
      const int lx = gx - g.world.ox, ly = gy - g.world.oy;
      if (!endless) {
        if (m.in(gx, gy)) {
          const Ground gr = m.at(gx, gy);
          s.c = groundInk(gr); s.water = groundWater(gr) || gr == Ground::Void; s.known = true;
          s.bldg = m.bldgAt[(size_t)gy * m.w + gx] >= 0;
        } else { s.water = true; s.deep = true; s.known = true; s.c = kSeaDeep; }
        grid[(size_t)j * N + i] = s;
        continue;
      }
      if (geo) {
        const ew::MacroSample ms = g.world.src->macroFar(gx, gy);
        s.known = true; s.water = ms.water; s.e = ms.elev / 65536.0f; s.h = ms.height;
        const ew::Geology gl = g.world.src->geology(gx, gy);
        s.prov = gl.province;
        uint32_t col = rockInk(gl.rock);
        col = shadec(col, 0.86f + ((gl.province >> 8) & 15) / 15.0f * 0.22f);   // provinces of one rock still differ
        s.c = ms.water ? kSea : col;
        grid[(size_t)j * N + i] = s;
        continue;
      }
      s.known = seenSoft(g, gx, gy);
      // zoomed out a map pixel spans many fog cells: it is known if any part of it was seen
      if (!s.known && z >= 4) {
        const int32_t hz = (int32_t)(z * 0.5f);
        s.known = seenSoft(g, gx - hz, gy - hz) || seenSoft(g, gx + hz, gy - hz) || seenSoft(g, gx - hz, gy + hz) || seenSoft(g, gx + hz, gy + hz);
      }
      for (const GRect& r : towns)
        if (!s.known && gx >= r.x0 && gy >= r.y0 && gx < r.x1 && gy < r.y1) s.known = true;
      // (the unexplored land is only sketched: the far field, which builds no caches, so far lands cost no generation)
      const ew::MacroSample ms = z >= 16 || !s.known ? g.world.src->macroFar(gx, gy) : g.world.src->macro(gx, gy);
      s.e = ms.elev / 65536.0f;
      s.h = ms.height;
      s.water = ms.water;
      s.deep = ms.water && ms.elev < ew::ELEV_SEA - 4000;
      // (M4) whose land it is NOW: the realm's owner where it moved the genesis kingdom's settlements (Realm::landOwner,
      // a Voronoi of the owned settlements inside the old realm), else the generator's kingdom
      s.kingdom = ms.kingdom && J.moved.count(ms.kingdom) ? g.realm.landOwner(ms.kingdom, gx, gy) : ms.kingdom;
      s.bio = ms.biome;
      s.eco = (uint8_t)ms.eco;
      if (!s.known) { grid[(size_t)j * N + i] = s; continue; }
      if (z <= 2 && m.in(lx, ly)) {
        // the real tiles of the window
        const Ground gr = m.at(lx, ly);
        s.c = groundInk(gr);
        // natural ground takes its biome's ink (tile by tile the ecotone grounds made a checkerboard at the street zoom)
        // (M3c fixer) the sand, marsh and snow grounds too: the badlands, salt flats, crystal barrens and ash fields lie on
        // Sand, and drew as anonymous sand matching no legend entry
        if (!m.biome.empty() && (gr == Ground::Grass || gr == Ground::Meadow || gr == Ground::ForestFloor || gr == Ground::Autumn || gr == Ground::Tundra ||
                                 gr == Ground::Sand || gr == Ground::Swamp || gr == Ground::Snow))
          s.c = mixc(ecoInk(m.ecoAt(lx, ly)), groundInk(gr), z >= 1 ? 0.12f : 0.25f);
        s.water = groundWater(gr);
        s.deep = gr == Ground::DeepWater;
        s.road = gr == Ground::Road || gr == Ground::Bridge || gr == Ground::Plaza;
        s.h = (uint8_t)m.heightAt(lx, ly);
        s.wall = !m.wall.empty() && m.wall[(size_t)ly * m.w + lx] != 0;
        s.tree = isTreeProp(m.prop[(size_t)ly * m.w + lx]);
        // (M2 fixer round 3, review: "the town zoom is muddy, blurry green noise") a sample spanning two tiles takes
        // the trees and buildings anywhere in its 2 x 2 tiles, so woods read as canopy and houses as roofs, not as a
        // speckle of single tiles
        // At the town zooms a lone tree is left out (its single dark pixel was the noise); woods (four or more trees
        // in the 4 x 4 tiles round the sample) show as one canopy
        int bAt = m.bldgAt[(size_t)ly * m.w + lx];
        if (z >= 1) {
          const int fp = z >= 2 ? 1 : 0;
          bool any = false;
          for (int oy = -fp; oy <= 0; oy++)
            for (int ox = -fp; ox <= 0; ox++) {
              const int qx = lx + ox, qy = ly + oy;
              if (!m.in(qx, qy)) continue;
              if (isTreeProp(m.prop[(size_t)qy * m.w + qx])) any = true;
              if (bAt < 0 && m.bldgAt[(size_t)qy * m.w + qx] >= 0) bAt = m.bldgAt[(size_t)qy * m.w + qx];
            }
          int near = 0;
          (void)any;
          for (int oy = -2; oy <= 1; oy++)
              for (int ox = -2; ox <= 1; ox++)
                if (m.in(lx + ox, ly + oy) && isTreeProp(m.prop[(size_t)(ly + oy) * m.w + lx + ox])) near++;
          s.tree = near >= 4;   // (spread over the clump, so a wood is one canopy, not a scatter of dark pixels)
        }
        s.bldg = bAt >= 0;
        if (z < 1 && !s.tree) {
          // (M2 fixer) a tree's crown spreads over about a tile and a half: round blobs, not single squares
          const float fx = (mpx + 0.5f) * z, fy = (mpy + 0.5f) * z;
          for (int oy = -1; oy <= 1 && !s.tree; oy++)
            for (int ox = -1; ox <= 1; ox++) {
              const int qx = lx + ox, qy = ly + oy;
              if (!m.in(qx, qy) || !isTreeProp(m.prop[(size_t)qy * m.w + qx])) continue;
              const float dx = fx - (gx + ox + 0.5f), dy = fy - (gy + oy + 0.15f);
              if (dx * dx + dy * dy < 0.62f) { s.tree = true; break; }
            }
        }
        if (s.bldg) {
          const Bldg& bd = m.bldgs[(size_t)bAt];
          s.roofC = bd.biome == Biome::Snow ? C(222, 228, 238) : bd.roof ? bd.roof : C(166, 90, 66);
        }
        if (!m.biome.empty()) s.bio = m.biomeAt(lx, ly);
        if (!m.eco.empty()) s.eco = (uint8_t)m.ecoAt(lx, ly);
      } else {
        s.c = ms.water ? (s.deep ? kSeaDeep : kSea) : ecoInk(ms.eco);
        if (!ms.water)
          for (const GRect& r : cores)
            if (gx >= r.x0 && gy >= r.y0 && gx < r.x1 && gy < r.y1) {
              // a discovered settlement's built area: blocks of roofs between its streets
              const bool street = ((gx - r.x0) % 9) < 2 || ((gy - r.y0) % 8) < 2;
              s.c = street ? C(188, 164, 124) : C(166, 92, 70);
              s.bldg = !street;
              s.road = street;
              break;
            }
      }
      // (M2 fixer) the view's snowline: taiga and snowfields from level 4 lie under snow (a snowbound
      // capital was drawn on green-grey land)
      if (!s.water && !s.road && !s.bldg && !s.wall && !s.tree &&
          (s.bio == Biome::Snow || s.bio == Biome::Taiga) && s.h >= 4)
        s.c = mixc(s.c, biomeInk(Biome::Snow), 0.85f);
      grid[(size_t)j * N + i] = s;
    }
  }
  J.row = N;
  auto at = [&](int i, int j) -> Smp& { return grid[(size_t)std::clamp(j, 0, N - 1) * N + std::clamp(i, 0, N - 1)]; };
  // water distance to the land (4-neighbour, a few steps): the ripple lines that follow a coast offshore
  {
    std::vector<int> q;
    for (int j = 0; j < N; j++)
      for (int i = 0; i < N; i++) {
        Smp& s = at(i, j);
        if (!s.water) continue;
        bool coast = false;
        for (int k = 0; k < 4 && !coast; k++) {
          static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
          const int ni = i + dx[k], nj = j + dy[k];
          if (ni >= 0 && nj >= 0 && ni < N && nj < N && !at(ni, nj).water) coast = true;
        }
        if (coast) { s.wd = 1; q.push_back(j * N + i); }
      }
    for (size_t h = 0; h < q.size(); h++) {
      const int i = q[h] % N, j = q[h] / N;
      const uint8_t d = at(i, j).wd;
      if (d >= 9) continue;
      static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; k++) {
        const int ni = i + dx[k], nj = j + dy[k];
        if (ni < 0 || nj < 0 || ni >= N || nj >= N) continue;
        Smp& n = at(ni, nj);
        if (!n.water || n.wd <= d + 1) continue;
        n.wd = (uint8_t)(d + 1);
        q.push_back(nj * N + ni);
      }
    }
  }
  // the hillshade's strength: the same mountains read at every zoom (a sample spans z tiles)
  const float hsA = 17.0f * std::pow(4.0f / z, 0.8f);
  Canvas c(TS, TS);
  for (int y = 0; y < TS; y++)
    for (int x = 0; x < TS; x++) {
      const int i = x + MG, j = y + MG;
      const Smp& s = at(i, j);
      const Smp& n = at(i, j - 1);
      const Smp& w = at(i - 1, j);
      const int px = ix * TS + x, py = iy * TS + y;
      const float grain = hashf(px, py, 77);
      uint32_t k;
      if (geo) {
        k = s.c;
        if (!s.water) {
          const float hs = std::clamp(0.5f + (at(i - 1, j - 1).e - at(i + 1, j + 1).e) * hsA, 0.15f, 0.85f);
          k = shadec(k, 0.80f + hs * 0.4f);
          if (n.prov != s.prov || w.prov != s.prov) k = kInkC;   // province borders
        } else if (s.wd <= 1) k = kInkC;
        c.set(x, y, k);
        continue;
      }
      if (!s.known) {
        // parchment: paper grain, a faint hatching, a little darker along the edge of the known land
        k = mixc(kParch, kParchDark, grain * 0.32f + (((px + py) & 7) == 0 ? 0.22f : 0.0f));
        if (endless && z >= 2) {   // the sketched outline of the unexplored coasts and seas
          if (s.water) k = mixc(k, C(150, 174, 182), ((px * 3 + py * 5) & 7) == 0 ? 0.38f : 0.22f);
          if ((!n.known && s.water != n.water) || (!w.known && s.water != w.water)) k = mixc(k, kInkC, 0.42f);
        }
        if (n.known || w.known || at(i - 1, j - 1).known) k = shadec(k, 0.9f);
        c.set(x, y, k);
        continue;
      }
      k = s.c;
      if (!s.water) {
        // hillshade (north-west light) from the macro elevation, and the relief levels' steps at close zoom
        // (M2 fixer) at the street zoom a sample is a quarter tile: the slope is taken a tile or more apart and at the
        // town zoom's strength (sample to sample the per-tile elevation steps were amplified into a plaid), and a level
        // step is one thin shaded line on its low side instead of a shaded band
        const int ko = z < 1 ? (int)std::lround(1.0f / z) : 1;
        const float slope = at(i - ko, j - ko).e - at(i + ko, j + ko).e;
        float hs = std::clamp(0.5f + slope * (z < 1 ? 17.0f * std::pow(4.0f, 0.8f) : hsA), 0.0f, 1.0f);
        if (z < 1) {
          if ((int)n.h > (int)s.h) hs = std::max(0.0f, hs - 0.30f);
          else if ((int)w.h > (int)s.h) hs = std::max(0.0f, hs - 0.16f);
          else if ((int)at(i, j + 1).h < (int)s.h || (int)at(i + 1, j).h < (int)s.h) hs = std::min(1.0f, hs + 0.12f);
        } else if (z <= 2) hs = std::clamp(hs + ((int)at(i - 1, j - 1).h - (int)s.h) * 0.16f - ((int)at(i + 1, j + 1).h - (int)s.h) * 0.10f, 0.0f, 1.0f);
        k = shadec(k, 0.74f + hs * 0.52f);
        // faint contours every two relief levels (Z1, Z2)
        if (z >= 1 && z <= 8 && (n.h / 2 != s.h / 2 || w.h / 2 != s.h / 2) && !n.water && !w.water) k = mixc(k, kInkSoft, 0.28f);
        // woods stippled at the town zoom outside the window (the land zoom gets tree marks)
        if (z < 1 && !s.road && !s.bldg && (s.bio == Biome::Forest || s.bio == Biome::Taiga || s.bio == Biome::Autumn) && !m.in((int)std::floor((px + 0.5f) * z) - g.world.ox, (int)std::floor((py + 0.5f) * z) - g.world.oy)) {
          const int jx = (int)(hash2(px / 3, py / 3, 731) % 2);
          if ((px + jx) % 3 == 0 && py % 3 == 0) k = shadec(k, 0.7f);
        }
        if (s.bio == Biome::Swamp && py % 3 == 0 && (px % 5) < 3) k = shadec(k, 0.86f);
        // (M4, owner carry-over: the two meadows read alike) flower meadows are flecked with blooms: rose, gold, white
        if (s.eco == (uint8_t)Eco::FlowerMeadow && !s.road && !s.bldg && !s.tree && !s.wall && z <= 16) {
          const uint32_t fh = hash2(px, py, 919);
          if (fh % 6u == 0) {
            static const uint32_t bloom[3] = {C(226, 140, 164), C(242, 210, 92), C(248, 242, 228)};
            k = mixc(k, bloom[(fh >> 8) % 3u], 0.72f);
          }
        }
        if (s.tree) {
          // woods seen from above: a canopy shaded in soft clumps, its edge lit on the north-west and in shade on the
          // south-east (tile by tile blobs made a plaid at the street zoom)
          const float v = vnoise(px / 2.6f, py / 2.6f, 741) + (hashf(px, py, 743) - 0.5f) * 0.25f;
          k = v > 0.62f ? C(92, 136, 70) : v > 0.36f ? C(62, 104, 58) : C(44, 80, 50);
          if (!n.tree || !w.tree) k = C(104, 148, 78);
          else if (!at(i + 1, j + 1).tree) k = C(34, 62, 44);
        }
        if (s.bldg) {
          // a roof seen from above: its south edge (the wall) darker, the north slope lit, the west edge lit, an
          // ink line round it; the house throws a short shade on the ground to its south-east
          const bool southEdge = !at(i, j + 1).bldg, northEdge = !n.bldg, westEdge = !w.bldg, eastEdge = !at(i + 1, j).bldg;
          const uint32_t rc = s.roofC ? s.roofC : C(166, 90, 66);
          if (z < 1) {
            // (M2 fixer) the street zoom: the roof in its own colour, the north slope lit and the south one in shade
            // either side of a ridge line, the wall a dark band along the south edge, an ink outline
            int up = 0, down = 0;
            for (int q = 1; q < 16 && at(i, j - q).bldg; q++) up = q;
            for (int q = 1; q < 16 && at(i, j + q).bldg; q++) down = q;
            const int tot = up + down + 1, wallR = std::max(1, tot / 4), roofR = tot - wallR;
            if (down < wallR) k = southEdge ? shadec(rc, 0.40f) : shadec(rc, 0.55f);   // the wall under the eaves
            else if (up == roofR / 2) k = shadec(rc, 1.28f);                            // the ridge
            else k = up < roofR / 2 ? shadec(rc, 1.10f) : shadec(rc, 0.80f);            // lit north slope, shaded south
            if (westEdge && !southEdge) k = shadec(k, 1.08f);
            if (eastEdge && !southEdge) k = shadec(k, 0.8f);
            if (northEdge || westEdge || eastEdge) k = mixc(k, kInkC, 0.45f);
          } else if (s.roofC) {
            // (M2 fixer round 3) the town zoom: each house a block of its own roof colour, lit on its north and west
            // edges, its wall a dark line on the south, an ink line round it
            k = southEdge ? shadec(rc, 0.45f) : northEdge ? mixc(shadec(rc, 1.2f), kInkC, 0.25f) : westEdge ? shadec(rc, 1.12f) : eastEdge ? shadec(rc, 0.72f) : rc;
          } else {
            k = southEdge ? C(96, 56, 48) : northEdge ? C(204, 126, 92) : westEdge ? C(186, 108, 80) : eastEdge ? C(132, 72, 58) : C(166, 90, 66);
            if (z <= 0.5f && !southEdge && !northEdge && at(i, j + 2).bldg && !at(i, j - 2).bldg) k = C(214, 140, 102);   // the ridge
          }
        } else if (at(i - 1, j - 1).bldg && z <= 1) k = shadec(k, 0.78f);
        if (s.wall) k = (!n.wall || !w.wall) ? C(176, 168, 152) : C(126, 118, 108);
      } else {
        // water: darker offshore, a ripple line following the coast, an inked shore
        k = s.deep ? kSeaDeep : kSea;
        if (s.wd >= 2 && s.wd <= 9) {
          if (s.wd == 3 || (s.wd == 6 && ((px + py) & 3) != 0)) k = mixc(k, kRipple, 0.65f);
          else k = mixc(k, C(176, 200, 200), 0.18f * (10 - s.wd) / 8.0f);
        }
        if (((px * 3 + py * 5) & 31) == 0 && s.wd > 6) k = mixc(k, kRipple, 0.4f);   // a few wave strokes far out
        if (s.wd == 1) k = mixc(kRiver, kInkC, 0.45f);
      }
      // (M4) kingdom borders at every zoom: where the owner changes (Realm::landOwner: conquests move them), a dashed line
      // in each owner's colour on its own side (two-tone between two realms, a double-width line facing the wildlands),
      // and a faint wash of the owner's colour a few pixels inside, so a realm reads as a shape on a phone
      if (!s.water && s.known && s.kingdom) {
        int bd = 9;
        bool wildSide = false;
        for (int r = 1; r <= 4 && bd == 9; r++)
          for (int oy = -r; oy <= r && bd == 9; oy++)
            for (int ox = -r; ox <= r; ox++) {
              if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
              const Smp& o = at(i + ox, j + oy);
              if (o.water || o.kingdom == s.kingdom) continue;
              bd = r; wildSide = o.kingdom == 0;
              break;
            }
        if (bd <= 4) {
          auto kc = J.kcol.find(s.kingdom);
          if (kc == J.kcol.end()) {
            const rui::Look L = rui::look(g, s.kingdom);
            uint32_t c0 = L.color ? L.color : C(160, 40, 44);
            // a pale field (white, gold) reads badly as a line on parchment: its charge, else it is darkened
            const int lum = ((int)(c0 & 255) * 3 + (int)((c0 >> 8) & 255) * 6 + (int)((c0 >> 16) & 255)) / 10;
            if (lum > 170 && L.color2) c0 = L.color2;
            const int lum2 = ((int)(c0 & 255) * 3 + (int)((c0 >> 8) & 255) * 6 + (int)((c0 >> 16) & 255)) / 10;
            kc = J.kcol.emplace(s.kingdom, lum2 > 150 ? shadec(c0, 0.62f) : c0).first;
          }
          const uint32_t ink = kc->second;
          const int width = wildSide ? 2 : 1;
          if (bd <= width) {
            const bool gap = ((px + py) / 2) % 3 == 2;   // dashes along the line, whichever way it runs
            k = gap ? mixc(k, ink, 0.06f) : mixc(ink, kInkC, 0.18f);   // (fixer M4 r3) the gaps read as gaps
            S.borderPx++;
          } else k = mixc(k, ink, 0.22f - 0.04f * (float)(bd - width - 1));
        }
      }
      // the paper shows through the paint, and the known land fades into the parchment at its edge (dithered)
      float fade = 0.14f + grain * 0.06f;
      if (z >= 8) {}
      else if (!n.known || !w.known || !at(i - 1, j - 1).known) fade = 0.55f;
      else if (!at(i + 1, j).known || !at(i, j + 1).known) fade = 0.4f;
      if (fade > 0.3f && grain < 0.35f) k = mixc(k, kParch, 0.85f);
      k = mixc(k, kParch, fade);
      c.set(x, y, k);
    }
  if (geo || !endless) { out = std::move(c); return true; }
  // ---- cartographic lines: rivers and roads from the explored regions' plans (Z1 outside the window, Z2)
  auto knownAt = [&](int x, int y) { return x >= -MG && y >= -MG && x < TS + MG && y < TS + MG && at(x + MG, y + MG).known; };
  auto plotLine = [&](float x0, float y0, float x1, float y1, uint32_t col, int dash, bool skipWindow) {
    const float L = std::max(std::fabs(x1 - x0), std::fabs(y1 - y0));
    const int n = std::max(1, (int)std::ceil(L));
    for (int k2 = 0; k2 <= n; k2++) {
      const float t = (float)k2 / n;
      const float fx = x0 + (x1 - x0) * t, fy = y0 + (y1 - y0) * t;
      const int x = (int)std::floor(fx), y = (int)std::floor(fy);
      if (x < 0 || y < 0 || x >= TS || y >= TS || !knownAt(x, y)) continue;
      if (dash && ((ix * TS + x + iy * TS + y) % (dash + 1)) == dash) continue;
      if (skipWindow) {   // inside the window the real tiles already show it
        const int lx = (int)std::floor((ix * TS + x + 0.5f) * z) - g.world.ox, ly = (int)std::floor((iy * TS + y + 0.5f) * z) - g.world.oy;
        if (m.in(lx, ly)) continue;
      }
      if (at(x + MG, y + MG).water && col != kRiver) { c.set(x, y, mixc(c.get(x, y), col, 0.5f)); continue; }
      c.set(x, y, mixc(col, kParch, 0.12f));
    }
  };
  if (z >= 1 && z <= 8) {
    const int32_t rx0 = ew::regionOf(tx0), rx1 = ew::regionOf(tx1), ry0 = ew::regionOf(ty0), ry1 = ew::regionOf(ty1);
    const double ox = ix * TS, oy = iy * TS;
    for (int32_t ry = ry0; ry <= ry1; ry++)
      for (int32_t rx = rx0; rx <= rx1; rx++) {
        if (!g.explored.regions.count(ExploredMask::key(rx, ry))) continue;
        const ew::RegionPlan& rp = g.world.src->region(rx, ry);
        const bool skipW = z <= 2;
        for (const ew::RiverPlan& rv : rp.rivers)
          plotLine((float)(rv.a.x / z - ox), (float)(rv.a.y / z - oy), (float)(rv.b.x / z - ox), (float)(rv.b.y / z - oy), kRiver, 0, skipW);
        for (const ew::RoadPlan& rd : rp.roads) {
          if (rd.cls == 2 && z > 2) continue;   // tracks only at the town zoom
          const uint32_t col = rd.cls == 0 ? C(136, 50, 40) : rd.cls == 1 ? C(122, 80, 54) : C(150, 120, 88);
          for (size_t k2 = 1; k2 < rd.pts.size(); k2++)
            plotLine((float)(rd.pts[k2 - 1].x / z - ox), (float)(rd.pts[k2 - 1].y / z - oy), (float)(rd.pts[k2].x / z - ox), (float)(rd.pts[k2].y / z - oy), col,
                     rd.cls == 0 ? 0 : rd.cls == 1 ? 3 : 1, skipW);
        }
      }
  }
  // ---- glyphs: tree marks in the woods (Z1 outside the window, Z2) and snow-tipped peaks on the ridge crests (Z2, Z3).
  //      A jittered global lattice of candidates; each tile draws every glyph that reaches it (sorted north to south).
  struct Gl { int x, y, kind, size; bool snow; int lean = 0; };
  std::vector<Gl> gls;
  auto knownLand = [&](int x, int y) { if (x < -MG || y < -MG || x >= TS + MG || y >= TS + MG) return false; const Smp& s = at(x + MG, y + MG); return s.known && !s.water && !s.bldg && !s.road; };
  if (z >= 4) {
    // (M3c fixer, review: "a heap of identical white triangles") a sparser lattice at the land zooms, a quarter of
    // the lower crests left out, and every peak its own shape (it leans one way or the other, some broader, some taller)
    // (fixer M4 r1, review: "a carpet of identical mountain glyphs buries labels" on the phone map) sparser still at the
    // land zooms: a wider lattice, half the lower crests and a quarter of the high ones left out, so the ranges read as
    // ranges and the labels and the realm's markers stand clear of them
    const int G = z >= 16 ? 8 : 16;
    for (int cy = (int)std::floor((iy * TS - 7.0) / G); cy <= (int)std::floor((iy * TS + TS + 7.0) / G); cy++)
      for (int cx = (int)std::floor((ix * TS - 7.0) / G); cx <= (int)std::floor((ix * TS + TS + 7.0) / G); cx++) {
        const uint32_t h = hash2(cx, cy, 977);
        const int gx = cx * G + 1 + (int)(h % (uint32_t)(G - 2)) - ix * TS, gy = cy * G + 1 + (int)((h >> 8) % (uint32_t)(G - 2)) - iy * TS;
        if (!knownLand(gx, gy)) continue;
        const Smp& s = at(gx + MG, gy + MG);
        if (s.h < 5 && s.bio != Biome::Mountain) continue;
        // a crest: as high as the land a few pixels either way along one axis at least
        const float e = s.e;
        const bool crestX = e >= at(gx + MG - 3, gy + MG).e && e >= at(gx + MG + 3, gy + MG).e;
        const bool crestY = e >= at(gx + MG, gy + MG - 3).e && e >= at(gx + MG, gy + MG + 3).e;
        if (!crestX && !crestY && s.h < 6) continue;
        if ((int)((h >> 16) & 3) <= (s.h < 6 ? 1 : 0)) continue;
        const bool snow = s.h >= 6 || s.bio == Biome::Snow || s.bio == Biome::Taiga;
        Gl pk{gx, gy, 0, std::clamp((int)s.h - 3, 2, 4) + (z >= 16 ? 0 : 1) + (int)((h >> 24) % 3u) - 1, snow};
        pk.size = std::max(2, pk.size);
        pk.lean = (int)((h >> 20) % 3u) - 1;
        gls.push_back(pk);
      }
  }
  if (z >= 1 && z <= 8) {   // (M2 fixer round 3: the town zooms too, outside the window: the stipple read as halftone)
    const int G = 6;
    for (int cy = (int)std::floor((iy * TS - 4.0) / G); cy <= (int)std::floor((iy * TS + TS + 4.0) / G); cy++)
      for (int cx = (int)std::floor((ix * TS - 4.0) / G); cx <= (int)std::floor((ix * TS + TS + 4.0) / G); cx++) {
        const uint32_t h = hash2(cx, cy, 983);
        // woods in clumps with clearings between them (an even stipple read as a pattern, not as a forest)
        if ((h & 7) < 3 || vnoise((cx * G) / 14.0f, (cy * G) / 14.0f, 985) < 0.42f) continue;
        const int gx = cx * G + (int)(h % (uint32_t)G) - ix * TS, gy = cy * G + (int)((h >> 8) % (uint32_t)G) - iy * TS;
        if (!knownLand(gx, gy) || !knownLand(gx, gy - 3)) continue;
        const Smp& s = at(gx + MG, gy + MG);
        if (s.h >= 5) continue;
        if (z <= 2 && m.in((int)std::floor((ix * TS + gx + 0.5f) * z) - g.world.ox, (int)std::floor((iy * TS + gy + 0.5f) * z) - g.world.oy)) continue;
        int kind = 0;
        if (s.bio == Biome::Forest) kind = 1;
        else if (s.bio == Biome::Autumn) kind = 2;
        else if (s.bio == Biome::Taiga) kind = 3;
        if (!kind) continue;
        gls.push_back({gx, gy, kind, 0, false});
      }
  }
  std::sort(gls.begin(), gls.end(), [](const Gl& a, const Gl& b) { return a.y < b.y; });
  for (const Gl& q : gls) {
    if (q.kind == 0) {
      // a little peak: lit west flank, hatched east flank, inked edges, a snow tip
      const int s = q.size, hgt = s + 2 + (s >= 4 ? 1 : 0) + (q.lean == 0 ? 1 : 0);
      const float kl = 1.0f + 0.4f * (float)q.lean, kr = 1.0f - 0.4f * (float)q.lean;   // the flanks' spread (a lean)
      for (int r = 0; r <= hgt; r++) {
        const int halfL = (int)std::lround(r * (float)s / hgt * kl), halfR = (int)std::lround(r * (float)s / hgt * kr);
        const int y = q.y - hgt + r;
        for (int dx = -halfL; dx <= halfR; dx++) {
          const int x = q.x + dx;
          if (x < 0 || y < 0 || x >= TS || y >= TS) continue;
          uint32_t col = dx < 0 ? C(236, 226, 196) : (dx == 0 ? C(200, 186, 160) : (((x + y) & 1) ? C(150, 128, 104) : C(176, 156, 128)));
          if (q.snow && r < hgt * 0.45f) col = dx <= 0 ? C(252, 252, 248) : C(204, 212, 222);
          if (dx == -halfL || dx == halfR) col = kInkC;
          c.set(x, y, col);
        }
      }
      // its shade on the land to the south-east
      for (int dx = 1; dx <= s + 1; dx++) { const int x = q.x + dx, y = q.y + 1; if (x >= 0 && y >= 0 && x < TS && y < TS) c.set(x, y, shadec(c.get(x, y), 0.82f)); }
    } else {
      // a tree mark: a crown with a lit top-left and an ink trunk (a little conifer in the taiga)
      const uint32_t dark = q.kind == 2 ? C(150, 82, 40) : q.kind == 3 ? C(54, 88, 70) : C(58, 98, 52);
      const uint32_t lite = q.kind == 2 ? C(214, 150, 74) : q.kind == 3 ? C(98, 136, 104) : C(110, 150, 82);
      auto put = [&](int x, int y, uint32_t col) { if (x >= 0 && y >= 0 && x < TS && y < TS) c.set(x, y, col); };
      if (q.kind == 3) {
        put(q.x, q.y - 3, lite); put(q.x - 1, q.y - 2, lite); put(q.x, q.y - 2, dark); put(q.x + 1, q.y - 2, dark);
        put(q.x - 1, q.y - 1, dark); put(q.x, q.y - 1, dark); put(q.x + 1, q.y - 1, dark); put(q.x, q.y, kInkSoft);
      } else {
        put(q.x - 1, q.y - 2, lite); put(q.x, q.y - 2, dark); put(q.x - 1, q.y - 1, dark); put(q.x, q.y - 1, dark); put(q.x + 1, q.y - 1, dark);
        put(q.x, q.y, kInkSoft); put(q.x + 1, q.y, shadec(c.get(q.x + 1, q.y), 0.85f));
      }
    }
  }
  out = std::move(c);
  return true;
}

// ---- free functions for the script commands (rpg/view/script_view.cpp) -----------------------------------------
void worldMapSetGeo(bool on) {
  if (S.geo == on) return;
  S.geo = on;   // (the explored signature carries the view: the next draw repaints and frees the old tiles)
}
bool worldMapGeo() { return S.geo; }
void worldMapSetZoomLevel(int zi) { S.zi = std::clamp(zi, 0, NZOOM - 1); }
void worldMapCentreTile(double gx, double gy) { S.cx = gx; S.cy = gy; }
void worldMapRequestSelect(int site) { S.reqSel = site; }
int worldMapLegendCount() { return (int)S.legend.size(); }
double worldMapWorstPaintMs() { return S.worstPaintMs; }

// ---- drawing -----------------------------------------------------------------------------------------------------
void View::drawWorldMap(Game& g, float x, float y, float w, float h) {
  Pix& P = *pix_;
  const auto t0 = std::chrono::steady_clock::now();
  const uint64_t wid = g.world.seed ^ ((uint64_t)g.world.genVersion << 56) ^ (g.world.endless ? 1ull << 55 : 0);
  auto playerG = [&](double& gx, double& gy) {
    Vec2 pp = g.pl().p;
    if (g.inside) {
      if (g.subSite >= 0) pp = Vec2(g.world.sites[g.subSite].ex * 16.0f, g.world.sites[g.subSite].ey * 16.0f);
      else if (g.subBldg >= 0) pp = Vec2(g.world.over.bldgs[g.subBldg].doorX() * 16.0f, g.world.over.bldgs[g.subBldg].doorY() * 16.0f);
    }
    gx = pp.x / 16.0 + g.world.ox; gy = pp.y / 16.0 + g.world.oy;
  };
  if (!S.init || S.world != wid) {
    for (auto& kv : S.tiles) pix_->destroy(kv.second.tex);
    S.tiles.clear();
    S.lms.clear();
    S.job = TileJob();
    S.init = true; S.world = wid;
    playerG(S.cx, S.cy);
    S.zi = g.world.endless ? 3 : 2;   // (M2 fixer: the town zoom first, where the known country round home fills the map)
  }
  if (S.icons.empty()) {
    for (int i = 0; i < IC_COUNT; i++) S.icons.push_back(pix_->bake(iconCanvas(i)));
    for (int d = 0; d < 8; d++) S.arrow[d] = pix_->bake(arrowCanvas(d));
  }
  if (S.warIcons.empty()) for (int i = 0; i < WI_COUNT; i++) S.warIcons.push_back(pix_->bake(warIconCanvas(i)));
  static float lastDraw = -10;
  if (t_ - lastDraw > 0.5f && mapSel_ < 0 && S.reqSel == -2) playerG(S.cx, S.cy);   // the map was closed: open it on the hero
  lastDraw = t_;
  // (M4) a conquest moves the borders: the realm's owners are part of what the tiles were painted with
  const uint64_t sig = exploredSig(g) ^ (S.geo ? 0x5A5A5A5A5A5A5A5Aull : 0) ^ (g.world.endless ? rui::signature(g) * 31 : 0);
  if (sig != S.sig) {   // the hero has seen more (or the view changed): repaint (the tiles fill back in over a few frames)
    for (auto& kv : S.tiles) pix_->destroy(kv.second.tex);
    S.tiles.clear();
    S.job = TileJob();
    S.sig = sig;
  }
  S.rx = x; S.ry = y; S.rw = w; S.rh = h;
  const float z = kZoom[S.zi];
  P.rect(x, y, w, h, colOf(kParch));
  // the tiles in view, painted within a per-frame budget (nearest the centre first)
  const double left = S.cx / z - w / 2, top = S.cy / z - h / 2;   // map pixel at the rectangle's top-left
  const int ix0 = (int)std::floor(left / TS), iy0 = (int)std::floor(top / TS);
  const int ix1 = (int)std::floor((left + w) / TS), iy1 = (int)std::floor((top + h) / TS);
  std::vector<std::pair<int, int>> todo;
  P.pushBox((int)x, (int)y, (int)w, (int)h);   // clip to the map rectangle (box coordinates from here)
  const uint64_t geoBit = S.geo ? (1ull << 63) : 0;
  // (M2 fixer) a tile not painted yet shows the same paper grain and hatching as the unexplored parchment (it showed
  // plain flat blocks with crisp seams), and a freshly painted tile fades in over it instead of popping
  if (!S.blankOn) {
    Canvas bc(TS, TS);
    for (int yy = 0; yy < TS; yy++)
      for (int xx = 0; xx < TS; xx++) bc.set(xx, yy, mixc(kParch, kParchDark, hashf(xx, yy, 77) * 0.32f + (((xx + yy) & 7) == 0 ? 0.22f : 0.0f)));
    S.blank = pix_->bake(bc);
    S.blankOn = true;
  }
  for (int iy = iy0; iy <= iy1; iy++)
    for (int ix = ix0; ix <= ix1; ix++) {
      auto it = S.tiles.find(tileKey(S.zi, ix, iy) ^ geoBit);
      const float sx = (float)std::floor(ix * TS - left), sy = (float)std::floor(iy * TS - top);
      if (it == S.tiles.end()) { P.blit(S.blank, sx, sy); todo.push_back({ix, iy}); continue; }
      it->second.used = t_;
      const float a = std::clamp((t_ - it->second.born) / 0.18f, 0.0f, 1.0f);
      if (a < 1.0f) {
        P.blit(S.blank, sx, sy);
        P.blit(it->second.tex, sx, sy, false, Color(1, 1, 1, a));
      } else P.blit(it->second.tex, sx, sy);
    }
  std::sort(todo.begin(), todo.end(), [&](const std::pair<int, int>& a, const std::pair<int, int>& b) {
    auto d = [&](const std::pair<int, int>& q) { double dx = (q.first + 0.5) * TS - (left + w / 2), dy = (q.second + 0.5) * TS - (top + h / 2); return dx * dx + dy * dy; };
    return d(a) < d(b);
  });
  // paint the missing tiles nearest the centre first, a few sample rows at a time within the frame's budget (a tile
  // half done is resumed next frame)
  S.lastPaintMs = 0;
  if (S.job.on && (S.job.zi != S.zi || S.job.geo != S.geo)) S.job = TileJob();
  const auto deadline = t0 + std::chrono::microseconds(5000);
  for (size_t qi = 0;;) {
    if (!S.job.on) {
      while (qi < todo.size() && S.tiles.count(tileKey(S.zi, todo[qi].first, todo[qi].second) ^ geoBit)) qi++;
      if (qi >= todo.size()) break;
      S.job = TileJob();
      S.job.on = true; S.job.zi = S.zi; S.job.ix = todo[qi].first; S.job.iy = todo[qi].second; S.job.geo = S.geo;
      qi++;
    }
    const auto p0 = std::chrono::steady_clock::now();
    Canvas cv;
    const bool done = paintMapTile(g, S.job, cv, deadline);
    S.lastPaintMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - p0).count();
    if (!done) break;
    MapState::T t;
    t.tex = pix_->bake(cv);
    t.used = t_;
    t.born = t_;   // (drawn from the next frame, fading in over the blank parchment already drawn here)
    S.tiles[tileKey(S.zi, S.job.ix, S.job.iy) ^ geoBit] = t;
    S.tilesPainted++;
    S.job = TileJob();
    if (std::chrono::steady_clock::now() > deadline) break;
  }
  S.worstPaintMs = std::max(S.worstPaintMs, S.lastPaintMs);
  // bound the cache (the oldest tiles go)
  if (S.tiles.size() > 192) {
    std::vector<std::pair<float, uint64_t>> age;
    for (auto& kv : S.tiles) age.push_back({kv.second.used, kv.first});
    std::sort(age.begin(), age.end());
    for (size_t i = 0; i + 128 < age.size(); i++) { pix_->destroy(S.tiles[age[i].second].tex); S.tiles.erase(age[i].second); }
  }
  auto toScr = [&](double gx, double gy) { return Vec2((float)(gx / z - left), (float)(gy / z - top)); };
  S.legend.clear();
  auto legendAdd = [&](int ic) { if (std::find(S.legend.begin(), S.legend.end(), ic) == S.legend.end()) S.legend.push_back(ic); };
  // (M3c LIFE) the biomes in view: a coarse census of the known land (12 x 8 far samples, re-counted when the view
  // moves by a tenth of its width or the zoom or the explored land changes)
  if (g.world.endless && g.world.src && !S.geo &&
      (S.ecoZi != S.zi || S.ecoSig != sig || std::fabs(S.ecoCx - S.cx) > w * z * 0.1 || std::fabs(S.ecoCy - S.cy) > h * z * 0.1)) {
    S.ecoZi = S.zi; S.ecoSig = sig; S.ecoCx = S.cx; S.ecoCy = S.cy;
    int cnt[(int)Eco::COUNT] = {};
    // (M3c fixer round 3: 20 x 12 samples, so a biome a sixth of the view wide is not missed between them)
    for (int j = 0; j < 12; j++)
      for (int i = 0; i < 20; i++) {
        const int32_t gx = (int32_t)std::floor((left + w * (i + 0.5) / 20.0) * z), gy = (int32_t)std::floor((top + h * (j + 0.5) / 12.0) * z);
        if (!seenSoft(g, gx, gy)) continue;
        const ew::MacroSample ms = g.world.src->macroFar(gx, gy);
        if (ms.water) continue;
        cnt[(int)ms.eco]++;
      }
    S.ecoLeg.clear();
    for (int e = 0; e < (int)Eco::COUNT; e++) if (cnt[e] > 0 && e != (int)Eco::Ocean) S.ecoLeg.push_back(e);
    std::sort(S.ecoLeg.begin(), S.ecoLeg.end(), [&](int a, int b) { return cnt[a] != cnt[b] ? cnt[a] > cnt[b] : a < b; });
    // (M3c fixer round 3, review: "the legend stops at 5 entries and leaves the most visible colours unexplained")
    // every biome that covers a visible share of the known land (at least 1 %), up to 14; the side column cuts the list
    // to the rows it has (the places keep theirs)
    {
      long tot = 0;
      for (int e : S.ecoLeg) tot += cnt[e];
      size_t keep = 0;
      while (keep < S.ecoLeg.size() && keep < 14 && (keep < 3 || cnt[S.ecoLeg[keep]] * 100 >= tot)) keep++;
      S.ecoLeg.resize(keep);
    }
  }
  // ---- labels are placed after the markers, most important first; one that would overlap a placed label (or a
  //      marker) tries its other position and is otherwise left out
  struct Lab { int prio; float x, y, alt; std::string s; Color ink, halo; bool spaced; };
  std::vector<Lab> labs;
  struct Box { float x0, y0, x1, y1; };
  std::vector<Box> taken;
  // the hero
  double pgx0, pgy0;
  playerG(pgx0, pgy0);
  const Vec2 hp = toScr(pgx0, pgy0);
  taken.push_back({hp.x - 9, hp.y - 10, hp.x + 9, hp.y + 8});   // (M3c fixer round 3: the hero's whole marker)
  taken.push_back({0, h - 14, 110, h});            // the scale bar
  taken.push_back({w - 26, 0, w, 30});             // the compass rose
  if (g.world.endless && !S.geo) {
    // kingdom names (M4: in the society's word, where the realm holds its land now: the middle of its settlements; a
    // fallen realm has none)
    if (z >= 4)
      for (const Kingdom& k : g.world.kingdoms) {
        double kx = k.gx + 0.5, ky = k.gy + 0.5;
        if (const realm::KingdomState* K = g.realm.kingdom(k.id)) {
          if (K->fallen) continue;
          double sx = 0, sy = 0;
          int n = 0;
          for (ew::Gid sid : K->settlements)
            if (const realm::SettlementState* st = g.realm.settlement(sid)) { sx += st->gx; sy += st->gy; n++; }
          // (its seat when it still holds it, else the middle of what it holds)
          bool seat = false;
          for (ew::Gid sid : K->settlements) if (sid == k.capitalId) seat = true;
          if (n > 0 && !seat) { kx = sx / n + 0.5; ky = sy / n + 0.5; }
        }
        if (!g.explored.seen((int32_t)kx, (int32_t)ky)) continue;
        Vec2 s = toScr(kx, ky);
        if (s.x < -80 || s.y < -20 || s.x > w + 80 || s.y > h + 20) continue;
        const rui::Look L = rui::look(g, k.id);
        const uint32_t kcol = L.color ? L.color : k.color;
        labs.push_back({3, s.x, s.y - 16, s.y + 8, rui::realmTitle(L, false), colOf(kcol ? mixc(kcol, C(60, 30, 30), 0.3f) : rgba(150, 40, 44)), Color(0.95f, 0.9f, 0.8f, 0.7f), z >= 16});
      }
    // landmarks of the explored regions near the view (the region plans are fetched a few per frame)
    {
      const int32_t gx0 = (int32_t)std::floor(left * z) - 512, gy0 = (int32_t)std::floor(top * z) - 512;
      const int32_t gx1 = (int32_t)std::floor((left + w) * z) + 512, gy1 = (int32_t)std::floor((top + h) * z) + 512;
      int fetched = 0;
      for (auto& kv : g.explored.regions) {
        const int32_t rx = ExploredMask::keyRx(kv.first), ry = ExploredMask::keyRy(kv.first);
        if ((rx + 1) * ew::REGION < gx0 || rx * ew::REGION > gx1 || (ry + 1) * ew::REGION < gy0 || ry * ew::REGION > gy1) continue;
        auto it = S.lms.find(kv.first);
        if (it == S.lms.end()) {
          // (M2 fixer round 3) one region plan a frame, and none once the frame's map work passed 6 ms: on the web a
          // plan is made on the main thread, and opening the map stacked two of them on the tile painting
          if (fetched >= 1 || std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() > 6.0) continue;
          fetched++;
          it = S.lms.emplace(kv.first, g.world.src->region(rx, ry).landmarks).first;
        }
        for (const ew::LandmarkPlan& lm : it->second) {
          // big enough at this zoom (a feature of `size` tiles spans size / z map pixels), not absurdly large
          const float spanPx = lm.size / z;
          if (spanPx < 14.0f || spanPx > 2400.0f) continue;
          bool seenNear = g.explored.seen(lm.x, lm.y);
          const int32_t r = std::min<int32_t>(lm.size / 2, 96);
          for (int k = 0; k < 8 && !seenNear; k++) {
            const float a = k * 0.785398f;
            seenNear = g.explored.seen(lm.x + (int32_t)(std::cos(a) * r), lm.y + (int32_t)(std::sin(a) * r));
          }
          if (!seenNear) continue;
          const Vec2 s = toScr(lm.x + 0.5, lm.y + 0.5);
          if (s.x < -100 || s.y < -20 || s.x > w + 100 || s.y > h + 20) continue;
          Color ink(0.36f, 0.24f, 0.14f);
          switch (lm.kind) {
            case ew::LandmarkKind::Lake: case ew::LandmarkKind::River: case ew::LandmarkKind::Sea: ink = Color(0.16f, 0.30f, 0.52f); break;
            case ew::LandmarkKind::Forest: ink = Color(0.14f, 0.34f, 0.16f); break;
            case ew::LandmarkKind::Marsh: ink = Color(0.30f, 0.34f, 0.18f); break;
            case ew::LandmarkKind::Desert: ink = Color(0.52f, 0.34f, 0.12f); break;
            case ew::LandmarkKind::Pass: ink = Color(0.50f, 0.16f, 0.12f); break;
            default: break;
          }
          const int prio = lm.kind == ew::LandmarkKind::Range || lm.kind == ew::LandmarkKind::Sea ? 6 : 7;
          const bool spaced = (lm.kind == ew::LandmarkKind::Range || lm.kind == ew::LandmarkKind::Sea || lm.kind == ew::LandmarkKind::Desert) && spanPx > 60;
          labs.push_back({prio, s.x, s.y - 4, s.y + 6, lm.name, ink, Color(0.96f, 0.92f, 0.82f, 0.65f), spaced});
        }
      }
    }
  }
  // ---- markers: rumoured places, discovered places (icons), the tracked quest, the hero
  struct Mk { float x, y; int ic; int site; };
  std::vector<Mk> marks;
  for (int i = 0; i < (int)g.world.sites.size() && !S.geo; i++) {
    const Site& st = g.world.sites[(size_t)i];
    if (st.rumoured && !st.discovered) {
      // near, not on: the "?" sits a stable distance off where the place lies (directions, not a pin)
      const uint64_t hsh = ew::mix64(st.id ^ 0x52554D4F5552ull);
      const float ang = (float)(hsh & 0xFFFF) / 65536.0f * 6.2831853f, dist = 8.0f + (float)((hsh >> 16) % 17);
      const Vec2 s = toScr(st.ex + g.world.ox + 0.5 + std::cos(ang) * dist, st.ey + g.world.oy + 0.5 + std::sin(ang) * dist);
      if (s.x < -12 || s.y < -12 || s.x > w + 12 || s.y > h + 12) continue;
      const float R = 7.5f + (st.type == SiteType::Wonder ? 2.0f : 0.0f);
      for (int k = 0; k < 24; k++) {
        if ((k & 3) == 3) continue;   // dashed
        const float a = k / 24.0f * 6.2831853f + t_ * 0.3f;
        P.rect(std::floor(s.x + std::cos(a) * R), std::floor(s.y + std::sin(a) * R), 1, 1, st.type == SiteType::Wonder ? Color(0.62f, 0.40f, 0.10f) : kInk);
      }
      P.text(s.x + 1, s.y - 2, "?", 1, Color(0.95f, 0.9f, 0.8f, 0.8f), 1);
      P.text(s.x, s.y - 3, "?", 1, st.type == SiteType::Wonder ? Color(0.62f, 0.40f, 0.10f) : kInk, 1);
      taken.push_back({s.x - R, s.y - R, s.x + R, s.y + R});
      if (i == mapSel_) P.frame(s.x - R - 2, s.y - R - 2, R * 2 + 4, R * 2 + 4, Color(1, 1, 1));
      legendAdd(-1);
      continue;
    }
    if (!st.discovered) continue;
    const Vec2 s = toScr(st.ex + g.world.ox + 0.5, st.ey + g.world.oy + 0.5);
    if (s.x < -10 || s.y < -10 || s.x > w + 10 || s.y > h + 10) continue;
    // far out only the places that matter are drawn (settlements, wonders, the shard ruins, the lair)
    const bool minor = !st.settlement() && st.type != SiteType::Wonder && !st.mainQuest && st.type != SiteType::DragonLair;
    if (minor && z >= 16) continue;
    if (st.type == SiteType::Village && z >= 32) continue;
    marks.push_back({s.x, s.y, siteIcon(st), i});
  }
  // (fixer M4 r3, review: "about 8 icons sit in one knot round the start on the phone") declutter: the places are
  // taken most important first (the selected one, capitals, cities, towns, wonders, villages, the rest) and an icon
  // that would sit mostly on one already kept, or on the hero's arrow, is left out at this zoom (zoom in to see it)
  {
    auto rank = [&](const Mk& m) {
      const Site& st = g.world.sites[(size_t)m.site];
      if (m.site == mapSel_) return 0;
      if (st.capital) return 1;
      if (st.type == SiteType::City) return 2;
      if (st.type == SiteType::Town || st.type == SiteType::Wonder || st.mainQuest) return 3;
      if (st.type == SiteType::Village) return 4;
      return 5;
    };
    std::stable_sort(marks.begin(), marks.end(), [&](const Mk& a, const Mk& b) { return rank(a) < rank(b); });
    std::vector<Mk> kept;
    for (const Mk& m : marks) {
      const Tex& t = S.icons[(size_t)m.ic];
      const float hwI = t.w / 2.0f, hhI = t.h / 2.0f;
      bool crowd = false;
      auto over = [&](float x0, float y0, float x1, float y1) {
        const float ox = std::min(m.x + hwI, x1) - std::max(m.x - hwI, x0), oy = std::min(m.y + hhI, y1) - std::max(m.y - hhI, y0);
        return ox > 0 && oy > 0 && ox * oy > 0.20f * (2 * hwI) * (2 * hhI);
      };
      if (m.site != mapSel_) {
        if (over(hp.x - 9, hp.y - 9, hp.x + 9, hp.y + 9)) crowd = true;
        for (const Mk& k : kept) {   // (with a little air round each kept icon: touching icons read as one knot)
          const Tex& kt = S.icons[(size_t)k.ic];
          if (over(k.x - kt.w / 2.0f - 3, k.y - kt.h / 2.0f - 3, k.x + kt.w / 2.0f + 3, k.y + kt.h / 2.0f + 3)) { crowd = true; break; }
        }
      }
      if (!crowd) kept.push_back(m);
    }
    marks.swap(kept);
  }
  // draw the icons south last (they overlap like the land)
  std::sort(marks.begin(), marks.end(), [](const Mk& a, const Mk& b) { return a.y < b.y; });
  for (const Mk& mk : marks) {
    const Tex& t = S.icons[(size_t)mk.ic];
    const float ix = std::floor(mk.x - t.w / 2.0f), iy = std::floor(mk.y - t.h / 2.0f);
    P.blit(t, ix, iy);
    const Site& st = g.world.sites[(size_t)mk.site];
    if (st.capital) { const Tex& cr = S.icons[IC_CAPITAL]; P.blit(cr, std::floor(mk.x - cr.w / 2.0f), iy - cr.h + 1); }
    if (st.cleared && !st.settlement()) P.rect(ix + t.w - 3, iy + t.h - 3, 3, 3, Color(0.3f, 0.75f, 0.3f));   // a green tick: cleared
    if (mk.site == mapSel_) {
      const float pr = 2 + 1.0f * (0.5f + 0.5f * std::sin(t_ * 6));
      P.frame(ix - pr, iy - pr, t.w + pr * 2, t.h + pr * 2, Color(1, 1, 1));
    }
    taken.push_back({ix, iy, ix + t.w, iy + t.h});
    legendAdd(mk.ic);
    // labels: every settlement at the land zoom and closer, towns and cities further out, wonders always
    const float z2 = z;
    const bool label = st.type == SiteType::City || st.type == SiteType::Wonder || (st.type == SiteType::Town && z2 <= 16) || (st.settlement() && z2 <= 8) ||
                       (!st.settlement() && z2 <= 2) || mk.site == mapSel_;
    if (label) {
      const int prio = mk.site == mapSel_ ? 0 : st.capital ? 1 : st.type == SiteType::City ? 2 : st.type == SiteType::Wonder ? 2 : st.type == SiteType::Town ? 4 : 5;
      std::string nm = st.name;
      if (mk.site == mapSel_ && st.settlement() && st.special) {
        if (st.archetype == (uint8_t)ew::Archetype::Market && st.type != SiteType::Village)
          nm += std::string(" - MARKET ") + siteTypeName(st.type);
        else nm += std::string(" - ") + ew::specialtyName((ew::Specialty)st.special) + " " + siteTypeName(st.type);
      }
      const Color ink = st.type == SiteType::Wonder ? Color(0.50f, 0.30f, 0.06f) : kInk;
      labs.push_back({prio, mk.x, iy + t.h + 1, iy - (st.capital ? 15 : 10), nm, ink, Color(0.96f, 0.92f, 0.82f, 0.7f), false});
    }
  }
  // (M4) the war on the map: a marker at each troubled settlement the hero has seen or heard of (beside its icon, up and
  // right), a scroll at every other heard event; the side column names the state of the selected place
  S.markers = 0;
  std::vector<Box> warBoxes;   // (fixer M4 r3) the war's markers and the news scrolls: labels keep off them
  if (g.world.endless && !S.geo) {
    std::unordered_set<ew::Gid> marked;
    auto inView = [&](const Vec2& q) { return q.x >= -12 && q.y >= -12 && q.x <= w + 12 && q.y <= h + 12; };
    std::unordered_set<ew::Gid> heardAt;
    for (const realm::WorldEvent& e : g.realm.events()) if (e.heard && e.site) heardAt.insert(e.site);
    for (const realm::SettlementState* st : rui::knownStates(g)) {
      const uint16_t f = st->flags;
      int wi = -1;
      bool occupied = false;
      if (f & realm::SS_BESIEGED) wi = WI_SIEGE;
      else if (f & (realm::SS_BURNED | realm::SS_RUINED)) wi = WI_BURNED;
      else if (f & realm::SS_OCCUPIED) occupied = true;
      else if (f & realm::SS_REFUGEES) wi = WI_REFUGEES;
      else if (f & realm::SS_FAMINE) wi = WI_HUNGRY;
      if (wi < 0 && !occupied) continue;
      if (!g.explored.seen(st->gx, st->gy) && !heardAt.count(st->site)) continue;
      const Vec2 q = toScr(st->gx + 0.5, st->gy + 0.5);
      if (!inView(q)) continue;
      // beside the settlement's icon (up and to the right) so both read
      const float mx = std::floor(q.x + 6), my = std::floor(q.y - 9);
      if (occupied) {
        // a little banner on a pole in the occupier's colours
        const rui::Look L = rui::look(g, st->owner);
        const Color fc = colOf(L.color ? L.color : C(160, 40, 44)), tc = colOf(L.color2 ? L.color2 : C(236, 210, 120));
        P.rect(mx - 1, my - 1, 9, 11, Color(0.94f, 0.88f, 0.74f, 0.8f));
        P.rect(mx, my, 1, 10, kInk);
        P.rect(mx + 1, my, 6, 6, kInk);
        P.rect(mx + 1, my + 1, 5, 4, fc);
        P.rect(mx + 3, my + 2, 1, 2, tc);
        P.rect(mx + 1, my + 5, 2, 1, fc);
        P.rect(mx + 4, my + 5, 2, 1, fc);
        legendAdd(-5);
      } else {
        const Tex& t = S.warIcons[(size_t)wi];
        P.blit(t, mx, my);
        if (wi == WI_SIEGE) {   // a slow pulse round a siege still being fought
          const float pr = 7 + 1.5f * (0.5f + 0.5f * std::sin(t_ * 3));
          for (int k = 0; k < 18; k++) {
            const float a = k / 18.0f * 6.2831853f;
            P.rect(std::floor(mx + t.w / 2.0f + std::cos(a) * pr), std::floor(my + t.h / 2.0f + std::sin(a) * pr), 1, 1, Color(0.75f, 0.12f, 0.1f, 0.7f));
          }
        }
        legendAdd(wi == WI_SIEGE ? -3 : wi == WI_BURNED ? -4 : wi == WI_REFUGEES ? -6 : -9);
      }
      taken.push_back({mx - 1, my - 1, mx + 11, my + 11});
      warBoxes.push_back({mx - 1, my - 1, mx + 11, my + 11});
      marked.insert(st->site);
      S.markers++;
    }
    // (M5, 15.12) the settlements whose people the hero has seen (a census is known): a hungry one shows the bowl, a
    // festival a little string of pennants, beside its icon like the realm's markers
    for (int si = 0; si < (int)g.world.sites.size(); si++) {
      const Site& s = g.world.sites[(size_t)si];
      if (!s.settlement() || !s.discovered || marked.count(s.id) || !g.life.find(s.id)) continue;
      const uint16_t f = g.life.moodFlags(s.id);
      const bool fest = g.life.festival(s.id, g.day) || (f & life::MF_FESTIVAL);
      const bool hungry = (f & (life::MF_HUNGRY | life::MF_FAMINE)) != 0;
      if (!fest && !hungry) continue;
      const Vec2 q = toScr(s.ex + g.world.ox + 0.5, s.ey + g.world.oy + 0.5);
      if (!inView(q)) continue;
      const float mx = std::floor(q.x + 6), my = std::floor(q.y - 9);
      if (hungry) {
        P.blit(S.warIcons[WI_HUNGRY], mx, my);
        legendAdd(-9);
      } else {   // the pennants: a sagging cord with three flags in red and gold
        P.rect(mx - 1, my + 1, 11, 7, Color(0.94f, 0.88f, 0.74f, 0.8f));
        const int sag[9] = {0, 1, 1, 2, 2, 2, 1, 1, 0};
        for (int k = 0; k < 9; k++) P.rect(mx + k, my + 2 + sag[k], 1, 1, kInk);
        for (int k = 0; k < 3; k++) {
          const Color fc = (k & 1) ? Color(0.95f, 0.78f, 0.25f) : Color(0.80f, 0.20f, 0.17f);
          const float fx = mx + 1 + k * 3, fy = my + 3 + sag[1 + k * 3];
          P.rect(fx, fy, 2, 2, fc);
          P.rect(fx, fy + 2, 1, 1, fc);
        }
      }
      taken.push_back({mx - 1, my - 1, mx + 11, my + 11});
      marked.insert(s.id);
      S.markers++;
    }
    // the other heard events (a war declared, a famine, prices rising...): a scroll where it happened
    // (M4 integration) newest first, only the last 90 days' news, at most 12 scrolls and never on top of another
    // marker: after a long game the whole continent used to be papered with scrolls (unreadable on a phone)
    {
      const auto& evs = g.realm.events();
      int scrolls = 0;
      S.news.clear();
      std::vector<Box> drawn;   // the scrolls keep off the markers and each other; labels still win over them
      for (size_t ei = evs.size(); ei-- > 0 && scrolls < 12;) {
        const realm::WorldEvent& e = evs[ei];
        if (!e.heard || (e.site && marked.count(e.site))) continue;
        if (!e.gx && !e.gy) continue;
        if ((uint16_t)((uint16_t)g.day - e.day) > 90) continue;
        // (fixer M4 r2) only where the map knows the land: a scroll on blank fogged parchment said nothing
        if (!g.explored.seen(e.gx, e.gy)) continue;
        const Vec2 q = toScr(e.gx + 0.5, e.gy + 0.5);
        if (!inView(q)) continue;
        const Tex& t = S.warIcons[WI_NEWS];
        const float mx = std::floor(q.x - t.w / 2.0f), my = std::floor(q.y - t.h / 2.0f);
        bool clash = false;
        for (const std::vector<Box>* L : {&taken, &drawn})
          for (const Box& b : *L)
            if (mx < b.x1 && mx + t.w > b.x0 && my < b.y1 && my + t.h > b.y0) { clash = true; break; }
        if (clash) continue;
        P.blit(t, mx, my);
        if (e.id == S.newsSel) P.frame(mx - 2, my - 2, t.w + 4, t.h + 4, Color(1, 1, 1));
        S.news.push_back({e.id, Vec2(q.x + x, q.y + y)});
        drawn.push_back({mx - 1, my - 1, mx + t.w + 1, my + t.h + 1});
        warBoxes.push_back({mx - 1, my - 1, mx + t.w + 1, my + t.h + 1});
        legendAdd(-7);
        S.markers++;
        scrolls++;
        if (e.site) marked.insert(e.site);
      }
    }
    // a border is on screen: say what the dashed lines are
    legendAdd(-8);
  }
  // (the war's markers and the border lead the legend: the places' icons follow and are cut first)
  std::stable_partition(S.legend.begin(), S.legend.end(), [](int ic) { return ic <= -3 && ic >= -9; });
  m4Stats_.mapMarkers = S.markers;
  m4Stats_.borderPx = (int)std::min<long>(S.borderPx, 1 << 30);
  // the tracked quest's destination: the objective's global tile (Quest::tgx/tgy), else the classic rule
  bool hasQ = false;
  double qgx = 0, qgy = 0;
  if (g.trackedQuest >= 0 && !S.geo) {
    for (const Quest& q : g.quests)
      if (q.id == g.trackedQuest && q.hasPos && q.state != QState::Done) { hasQ = true; qgx = q.tgx + 0.5; qgy = q.tgy + 0.5; }
    int qtx = 0, qty = 0;
    if (!hasQ && g.questTarget(g.trackedQuest, qtx, qty)) { hasQ = true; qgx = qtx + g.world.ox + 0.5; qgy = qty + g.world.oy + 0.5; }
  }
  Vec2 qs;
  if (hasQ) {
    qs = toScr(qgx, qgy);
    const bool onMap = qs.x >= 0 && qs.y >= 0 && qs.x < w && qs.y < h;
    qs.x = std::clamp(qs.x, 6.0f, w - 7); qs.y = std::clamp(qs.y, 12.0f, h - 6);
    std::string nm;
    double bd = 12.0 * 12.0;
    for (const Site& st : g.world.sites) {
      const double dx = st.ex + g.world.ox + 0.5 - qgx, dy = st.ey + g.world.oy + 0.5 - qgy, d = dx * dx + dy * dy;
      if (d <= bd && (st.discovered || st.rumoured || true)) { bd = d; nm = st.name; }
    }
    nm = nm.empty() ? "QUEST" : nm + " (QUEST)";
    if (!onMap) nm += " >";
    // (fixer M4 r3, review: "the QUEST label is drawn over the icons round the start") a quest at the hero's own spot
    // keeps its diamond but no label: the label only covered the places round him
    const bool atHero = onMap && len2(qs - hp) < 14.0f * 14.0f;
    if (!atHero) labs.push_back({0, qs.x, qs.y + 7, qs.y - 18, nm, Color(0.45f, 0.24f, 0.04f), Color(1.0f, 0.95f, 0.8f, 0.8f), false});
    taken.push_back({qs.x - 6, qs.y - 6, qs.x + 6, qs.y + 6});
    legendAdd(-2);
  }
  {
    std::stable_sort(labs.begin(), labs.end(), [](const Lab& a, const Lab& b) { return a.prio < b.prio; });
    const size_t labelBase = taken.size();   // the boxes from here on are labels (before: markers, the hero, the furniture)
    auto clear = [&](const Box& b) {
      for (const Box& o : taken) if (b.x0 < o.x1 && b.x1 > o.x0 && b.y0 < o.y1 && b.y1 > o.y0) return false;
      for (const Box& o : warBoxes) if (b.x0 < o.x1 && b.x1 > o.x0 && b.y0 < o.y1 && b.y1 > o.y0) return false;
      return true;
    };
    // (M3c fixer round 3, review: "about 16 region names are drawn at once, several collide ... they hide the biome
    // colours") the feature names (ranges, seas, forests, lakes: prio 6 and 7) are capped by the map's area, the
    // greatest first, and keep a wider berth from every other label
    const int featCap = std::max(4, (int)(w * h / 15000.0f));
    int feats = 0;
    for (Lab l : labs) {
      if (l.prio >= 6 && feats >= featCap) continue;
      const int gap = l.spaced ? 3 : 0;
      const float tw = (float)P.textW(l.s, 1) + gap * (float)std::max<size_t>(0, l.s.size() - 1);
      l.x = std::clamp(l.x, tw / 2 + 2, std::max(tw / 2 + 2, w - tw / 2 - 2));   // whole on the map, not cut at its edge
      // (M2 fixer round 2) a forced label (the selected place, the quest) stays beside its marker: below, above, right
      // or left of it, first where nothing lies, then where only icons lie (the label is drawn over them with its
      // halo); it only stacks further off to keep clear of the other forced label. (On a phone the old stacking put
      // labels 40-50 px off their markers, over other places.)
      std::vector<Box> labelOnly;
      bool forcedHere = false;
      if (l.prio == 0) {
        for (size_t k = 0; k < taken.size(); k++) if (k >= labelBase) labelOnly.push_back(taken[k]);
        std::vector<Box> labelWar = labelOnly;   // (fixer M4 r3) round 1: over the places' icons, never over war markers
        labelWar.insert(labelWar.end(), warBoxes.begin(), warBoxes.end());
        const float cy = (l.y + l.alt) * 0.5f;
        const float cxs[4] = {l.x, l.x, l.x + tw / 2 + 10, l.x - tw / 2 - 10};
        const float cys[4] = {l.y, l.alt, cy, cy};
        bool placed = false;
        for (int round = 0; round < 3 && !placed; round++)
          for (int k = 0; k < 4 && !placed; k++) {
            const float x = std::clamp(cxs[k], tw / 2 + 2, std::max(tw / 2 + 2, w - tw / 2 - 2)), y = cys[k];
            if (y < 0 || y + 9 > h) continue;
            const Box b{x - tw / 2 - 1, y - 1, x + tw / 2 + 1, y + 9};
            bool ok = true;
            for (const Box& o : (round == 0 ? taken : round == 1 ? labelWar : labelOnly)) if (b.x0 < o.x1 && b.x1 > o.x0 && b.y0 < o.y1 && b.y1 > o.y0) { ok = false; break; }
            if (ok) { l.x = x; l.y = y; l.alt = y; placed = true; forcedHere = true; }
          }
      }
      for (int pass = 0; pass < (l.prio == 0 ? 3 : 2); pass++) {
        float yy = pass == 1 ? l.alt : l.y;   // (a forced label: below, above, else below anyway)
        if (pass == 2) {   // (M2 integration) two forced labels (the selected place, the quest) stack, never overprint
          for (int k = 1; k <= 4; k++) {
            const float cand[2] = {l.y + 10.0f * (float)k, l.alt - 10.0f * (float)k};
            bool found = false;
            for (float cy : cand) {
              if (cy < 0 || cy + 9 > h) continue;
              if (clear(Box{l.x - tw / 2 - 1, cy - 1, l.x + tw / 2 + 1, cy + 9})) { yy = cy; found = true; break; }
            }
            if (found) break;
          }
        }
        Box b{l.x - tw / 2 - 1, yy - 1, l.x + tw / 2 + 1, yy + 9};
        // (M2 fixer round 2) the lesser labels keep a little air round them: names stacked touching read as one
        const Box bp = l.prio >= 6 ? Box{b.x0 - 8, b.y0 - 4, b.x1 + 8, b.y1 + 4} : l.prio > 0 ? Box{b.x0 - 3, b.y0 - 2, b.x1 + 3, b.y1 + 2} : b;
        if (pass < 2 && !clear(bp) && !(forcedHere && pass == 0)) continue;
        if (l.prio > 0 && (b.y0 < 0 || b.y1 > h)) continue;
        taken.push_back(b);
        if (l.prio >= 6) feats++;
        // a parchment halo all round, then the ink (spaced capitals for the great features)
        if (!l.spaced) {
          for (int k = 0; k < 4; k++) P.text(l.x + (k == 0 ? 1.0f : k == 1 ? -1.0f : 0.0f), yy + (k == 2 ? 1.0f : k == 3 ? -1.0f : 0.0f), l.s, 1, l.halo, 1);
          P.text(l.x, yy, l.s, 1, l.ink, 1);
        } else {
          float cx = l.x - tw / 2;
          for (char ch : l.s) {
            const std::string one(1, ch);
            for (int k = 0; k < 4; k++) P.text(cx + (k == 0 ? 1.0f : k == 1 ? -1.0f : 0.0f), yy + (k == 2 ? 1.0f : k == 3 ? -1.0f : 0.0f), one, 1, l.halo, 0);
            P.text(cx, yy, one, 1, l.ink, 0);
            cx += 6 + gap;
          }
        }
        break;
      }
    }
  }
  if (hasQ) {
    // a gold diamond in a dark outline with a pulsing ring round it
    const float pr = 7 + 2.5f * (0.5f + 0.5f * std::sin(t_ * 5));
    for (int k = 0; k < 28; k++) {
      const float a = k / 28.0f * 6.2831853f;
      P.rect(std::floor(qs.x + std::cos(a) * pr), std::floor(qs.y + std::sin(a) * pr), 1, 1, Color(0.98f, 0.82f, 0.42f, 0.85f));
    }
    for (int pass = 0; pass < 2; pass++) {
      const int rr = pass == 0 ? 5 : 4;
      for (int yy = -rr; yy <= rr; yy++) {
        const int hw = rr - std::abs(yy);
        P.rect(qs.x - hw, qs.y + yy, hw * 2 + 1, 1, pass == 0 ? Color(0.18f, 0.1f, 0.04f) : kGold);
      }
    }
    P.rect(qs.x, qs.y - 2, 1, 3, Color(0.45f, 0.24f, 0.04f));
    P.rect(qs.x, qs.y + 2, 1, 1, Color(0.45f, 0.24f, 0.04f));
  }
  // the hero: an arrow the way he faces (a soft pulse round it)
  {
    const Vec2 aim = g.pl().aim;
    int dir = (int)std::lround(std::atan2(aim.x, -aim.y) / 0.785398f);
    dir = ((dir % 8) + 8) % 8;
    const float pulse = 0.5f + 0.5f * std::sin(t_ * 4);
    for (int k = 0; k < 20; k++) {
      const float a = k / 20.0f * 6.2831853f, r = 8 + pulse * 2;
      P.rect(std::floor(hp.x + std::cos(a) * r), std::floor(hp.y + std::sin(a) * r), 1, 1, Color(0.85f, 0.15f, 0.12f, 0.35f + 0.3f * (1 - pulse)));
    }
    const Tex& ar = S.arrow[dir];
    P.blit(ar, std::floor(hp.x - ar.w / 2.0f), std::floor(hp.y - ar.h / 2.0f));
  }
  // the geology view: one ore initial per province on screen, at the middle of what shows of it
  if (S.geo && g.world.src) {
    struct Pv { double sx = 0, sy = 0; int n = 0; ew::Ore ore = ew::Ore::Copper; };
    std::unordered_map<uint32_t, Pv> pv;
    const int step = 18;
    for (float sy = 4; sy < h; sy += step)
      for (float sx = 4; sx < w; sx += step) {
        const int32_t gx = (int32_t)std::floor((left + sx) * z), gy = (int32_t)std::floor((top + sy) * z);
        if (g.world.src->macroFar(gx, gy).water) continue;
        const ew::Geology gl = g.world.src->geology(gx, gy);
        Pv& p = pv[gl.province];
        p.sx += sx; p.sy += sy; p.n++; p.ore = gl.primary();
      }
    for (auto& kv : pv) {
      if (kv.second.n < 2) continue;
      const float cx = (float)(kv.second.sx / kv.second.n), cy = (float)(kv.second.sy / kv.second.n);
      const int o = (int)kv.second.ore;
      P.rect(cx - 7, cy - 6, 15, 11, Color(0.12f, 0.08f, 0.06f, 0.78f));
      P.text(cx + 1, cy - 3, kOreSym[o], 1, kOreCol[o], 1);
    }
  }
  // the scale bar and a small compass rose
  {
    const float tilesPer40 = 40 * z;
    std::string sc = tilesPer40 >= 1000 ? std::to_string((int)(tilesPer40 / 100) / 10.0).substr(0, 3) + "K TILES" : std::to_string((int)tilesPer40) + " TILES";
    // (fixer M4 r1) on a parchment plate of its own: the land's glyphs never run through the scale
    P.rect(3, h - 15, 50.0f + (float)P.textW(sc, 1) + 4, 13, Color(0.92f, 0.86f, 0.72f, 0.9f));
    P.rect(6, h - 9, 40, 2, kInk);
    P.rect(6, h - 11, 1, 4, kInk); P.rect(45, h - 11, 1, 4, kInk); P.rect(25, h - 10, 1, 2, kInk);
    P.text(50, h - 12, sc, 1, kInk);
    const float cx = w - 14, cy = 16;
    for (int k = -6; k <= 6; k++) { P.rect(cx + k, cy, 1, 1, Color(0.3f, 0.22f, 0.16f, 0.6f)); P.rect(cx, cy + k, 1, 1, Color(0.3f, 0.22f, 0.16f, 0.6f)); }
    for (int k = 1; k <= 5; k++) P.rect(cx - (5 - k) / 2.0f, cy - 6 - k + 5, (float)(5 - k), 1, Color(0.62f, 0.16f, 0.12f));
    P.text(cx, cy - 15, "N", 1, kInk, 1);
  }
  P.popBox();
  if (std::getenv("EMB_TIMING")) {
    const double all = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (all > 8.0) std::printf("world map frame: %.1f ms (tiles %.1f ms)\n", all, S.lastPaintMs);
  }
  // an inked double frame
  P.frame(x - 1, y - 1, w + 2, h + 2, Color(0.30f, 0.21f, 0.14f));
  P.frame(x - 3, y - 3, w + 6, h + 6, Color(0.52f, 0.40f, 0.26f));
}

// ---- input (box coordinates)
void View::worldMapZoom(int dz, Vec2 at) {
  const int nz = std::clamp(S.zi + dz, 0, NZOOM - 1);
  if (nz == S.zi) return;
  // keep the point under the cursor / pinch in place
  const float z0 = kZoom[S.zi], z1 = kZoom[nz];
  const double ax = at.x - (S.rx + S.rw / 2), ay = at.y - (S.ry + S.rh / 2);
  const bool inside = at.x >= S.rx && at.y >= S.ry && at.x < S.rx + S.rw && at.y < S.ry + S.rh;
  if (inside) { S.cx += ax * (z0 - z1); S.cy += ay * (z0 - z1); }
  S.zi = nz;
  audio_->play(Sfx::MenuMove);
}

void View::worldMapCentre(Game& g, int site) {
  if (site < 0) {
    Vec2 pp = g.pl().p;
    if (g.inside && g.subSite >= 0) pp = Vec2(g.world.sites[g.subSite].ex * 16.0f, g.world.sites[g.subSite].ey * 16.0f);
    else if (g.inside && g.subBldg >= 0) pp = Vec2(g.world.over.bldgs[g.subBldg].doorX() * 16.0f, g.world.over.bldgs[g.subBldg].doorY() * 16.0f);
    S.cx = pp.x / 16.0 + g.world.ox; S.cy = pp.y / 16.0 + g.world.oy;
    return;
  }
  if (site < (int)g.world.sites.size()) { S.cx = g.world.sites[site].ex + g.world.ox + 0.5; S.cy = g.world.sites[site].ey + g.world.oy + 0.5; }
}

bool View::worldMapKey(Game& g, int key) {
  const float step = 24 * kZoom[S.zi];
  switch (key) {
    case SDLK_Q: case SDLK_MINUS: case SDLK_KP_MINUS: worldMapZoom(1, Vec2(-1, -1)); return true;
    case SDLK_E: case SDLK_EQUALS: case SDLK_KP_PLUS: worldMapZoom(-1, Vec2(-1, -1)); return true;
    case SDLK_LEFT: S.cx -= step; return true;
    case SDLK_RIGHT: S.cx += step; return true;
    case SDLK_UP: S.cy -= step; return true;
    case SDLK_DOWN: S.cy += step; return true;
    case SDLK_C: case SDLK_HOME: worldMapCentre(g, -1); return true;
    case SDLK_G: worldMapSetGeo(!S.geo); audio_->play(Sfx::MenuMove); return true;   // the geology debug view
    default: return false;
  }
}

// pointer phases: 0 down, 1 move, 2 up. Returns true for a tap (an up without a drag) the caller treats as a pick.
bool View::worldMapPointer(int phase, uint64_t id, Vec2 p) {
  auto find = [&]() -> MapState::Ptr* { for (auto& q : S.ptrs) if (q.id == id) return &q; return nullptr; };
  const float z = kZoom[S.zi];
  if (phase == 0) {
    if (p.x < S.rx || p.y < S.ry || p.x >= S.rx + S.rw || p.y >= S.ry + S.rh) return false;
    if (!find()) S.ptrs.push_back({id, p, p});
    if (S.ptrs.size() == 1) S.moved = false;
    if (S.ptrs.size() == 2) { S.pinchD = std::max(8.0f, len(S.ptrs[0].p - S.ptrs[1].p)); S.pinchZ = S.zi; S.moved = true; }
    return false;
  }
  MapState::Ptr* q = find();
  if (!q) return false;
  if (phase == 1) {
    const Vec2 d = p - q->p;
    if (S.ptrs.size() == 1) {
      if (len2(p - q->start) > 64) S.moved = true;   // (M2 fixer) 8 px (about 10 pt on a phone) before a finger drags
      if (S.moved) { S.cx -= d.x * z; S.cy -= d.y * z; }
    } else if (S.ptrs.size() >= 2) {
      q->p = p;
      const float dd = std::max(8.0f, len(S.ptrs[0].p - S.ptrs[1].p));
      // every doubling of the finger spread is one zoom level closer
      const int want = std::clamp(S.pinchZ - (int)std::lround(std::log2(dd / S.pinchD)), 0, NZOOM - 1);
      if (want != S.zi) worldMapZoom(want - S.zi, (S.ptrs[0].p + S.ptrs[1].p) * 0.5f);
      return false;
    }
    q->p = p;
    return false;
  }
  // up
  const bool tapped = !S.moved && S.ptrs.size() == 1;
  S.ptrs.erase(std::remove_if(S.ptrs.begin(), S.ptrs.end(), [&](const MapState::Ptr& r) { return r.id == id; }), S.ptrs.end());
  return tapped;
}

// the discovered (or rumoured) place nearest a box point on the map (within 12 px: a finger's reach), or -1
static const realm::WorldEvent* newsEventById(const Game& g, uint32_t id) {
  if (!id) return nullptr;
  for (const realm::WorldEvent& e : g.realm.events()) if (e.id == id) return &e;
  return nullptr;
}

int View::worldMapPick(Game& g, Vec2 p) {
  const float z = kZoom[S.zi];
  const double left = S.cx / z - S.rw / 2, top = S.cy / z - S.rh / 2;
  int best = -1;
  float bd = (touchUI ? 14.0f : 10.0f) * (touchUI ? 14.0f : 10.0f);
  for (int i = 0; i < (int)g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    double gx = s.ex + g.world.ox + 0.5, gy = s.ey + g.world.oy + 0.5;
    if (!s.discovered) {
      if (!s.rumoured) continue;
      const uint64_t hsh = ew::mix64(s.id ^ 0x52554D4F5552ull);
      const float ang = (float)(hsh & 0xFFFF) / 65536.0f * 6.2831853f, dist = 8.0f + (float)((hsh >> 16) % 17);
      gx += std::cos(ang) * dist; gy += std::sin(ang) * dist;
    }
    Vec2 q((float)(gx / z - left + S.rx), (float)(gy / z - top + S.ry));
    float d = len2(q - p);
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

int View::worldMapZoomLevel() const { return S.zi; }

// ---- the MAP tab: the map on the left, the side column on the right (box coordinates; top: the content's y) ------
namespace {
struct TabLayout {
  float mapX, mapY, mapW, mapH, colX, colW, bh, btnY, infoY, travelY, carriageY, tbH;
};
TabLayout tabLayout(float top, bool touch) {
  TabLayout L;
  L.colW = 144;
  L.mapX = 16; L.mapY = top + 6;
  L.colX = (float)Pix::W - L.colW - 16;
  L.mapW = L.colX - 12 - L.mapX;
  L.mapH = (float)Pix::H - top - 18;
  L.bh = touch ? 33.0f : 16.0f;   // (M2 fixer) 33 logical px = 44 pt on a phone (Apple's touch target)
  L.btnY = top + 20;
  L.infoY = L.btnY + L.bh + 14;
  L.tbH = touch ? 33.0f : 16.0f;
  L.travelY = L.infoY + 66;
  L.carriageY = L.travelY + L.tbH + 4;
  return L;
}
std::string hoursText(float h) {
  if (h < 1.0f) return std::to_string(std::max(1, (int)std::lround(h * 60))) + " MIN";
  if (h < 36.0f) return std::to_string((int)std::lround(h)) + (std::lround(h) == 1 ? " HOUR" : " HOURS");
  const int d = (int)std::lround(h / 24.0f);
  return std::to_string(d) + (d == 1 ? " DAY" : " DAYS");
}
}  // namespace

void View::drawMapTab(Game& g, float top) {
  Pix& P = *pix_;
  if (S.reqSel != -2) { mapSel_ = S.reqSel; S.reqSel = -2; if (mapSel_ >= 0) worldMapCentre(g, mapSel_); }
  const TabLayout L = tabLayout(top, touchUI);
  drawWorldMap(g, L.mapX, L.mapY, L.mapW, L.mapH);
  const float x = L.colX;
  static const char* kZoomName[] = {"STREET", "STREET", "TOWN", "TOWN", "LAND", "LAND", "CONTINENT", "CONTINENT"};
  P.text(x, top + 8, S.geo ? "GEOLOGY" : "WORLD MAP", 1, kGold);
  P.text(x + L.colW, top + 8, kZoomName[std::clamp(worldMapZoomLevel(), 0, 7)], 1, kDim, 2);
  // zoom and centre buttons (finger-sized on touch)
  const float bw = (L.colW - 8) / 3;
  button(x, L.btnY, bw, L.bh, "+", false);
  button(x + bw + 4, L.btnY, bw, L.bh, "-", false);
  button(x + 2 * (bw + 4), L.btnY, bw, L.bh, "ME", false);
  float iy = L.infoY;
  const bool sel = mapSel_ >= 0 && mapSel_ < (int)g.world.sites.size();
  if (S.geo && g.world.src) {
    // the geology legend: the rocks, the ore initials, what is under the centre
    P.text(x, iy, "ROCK", 1, kDim);
    for (int r = 0; r < (int)ew::Rock::COUNT; r++) {
      const float yy = iy + 10 + r * 9;
      P.rect(x, yy, 7, 7, colOf(rockInk((ew::Rock)r)));
      P.frame(x, yy, 7, 7, Color(0.2f, 0.15f, 0.1f));
      P.text(x + 10, yy, ew::rockName((ew::Rock)r), 1, kText);
    }
    float oy = iy + 10 + 6 * 9 + 4;
    P.text(x, oy, "ORE", 1, kDim);
    for (int o = 0; o < (int)ew::Ore::COUNT; o++) {
      // (M2 fixer round 2) two columns inside the panel's column: "RA RARE METAL" ran to its frame
      const float half = std::floor(L.colW / 2.0f);
      const float xx = x + (o % 2) * half, yy = oy + 10 + (o / 2) * 9;
      P.text(xx, yy, kOreSym[o], 1, kOreCol[o]);
      std::string on = ew::oreName((ew::Ore)o);
      const size_t fitN = (size_t)std::max(1, (int)((half - 18 + 1) / 6));
      if (on.size() > fitN) { const size_t sp = on.find(' '); on = sp != std::string::npos && sp <= fitN ? on.substr(0, sp) : on.substr(0, fitN); }
      P.text(xx + 15, yy, on, 1, kText);
    }
    const ew::Geology gl = g.world.src->geology((int32_t)std::floor(S.cx), (int32_t)std::floor(S.cy));
    const float cy = oy + 10 + 3 * 9 + 6;
    P.text(x, cy, std::string("HERE: ") + ew::rockName(gl.rock), 1, kText);
    std::string aff;
    for (int o = 0; o < (int)ew::Ore::COUNT; o++)
      if (gl.ore[o] >= 96) aff += std::string(aff.empty() ? "" : " ") + ew::oreName((ew::Ore)o)[0] + std::to_string(gl.ore[o]);
    P.text(x, cy + 10, aff, 1, kDim);
    return;
  }
  if (sel) {
    const Site& s = g.world.sites[(size_t)mapSel_];
    const bool known = s.discovered;
    wrapText(x, iy, L.colW, known ? s.name : "A RUMOURED PLACE", kText, -1, 9);
    std::string sub = known ? ew::poiKindName(s.type, s.kind) : "SOMEWHERE NEAR HERE";
    if (known && s.capital) sub += " - CAPITAL";
    P.text(x, iy + 20, sub, 1, kDim);
    // (M4) its owner NOW in its society's word, and its state ("BESIEGED BY QIBA") where it has one (the level then
    // moves up beside its kind)
    uint32_t stc = 0;
    const std::string state = known && s.settlement() ? rui::stateLine(g, s.id, &stc) : std::string();
    const size_t fitC = (size_t)std::max(6, (int)(L.colW / 6));
    if (const Kingdom* k = known ? g.world.kingdomOf(mapSel_) : nullptr) {
      const rui::Look KL = rui::look(g, k->id);
      std::string kt = rui::realmTitle(KL, false);
      if (kt.size() > fitC) kt = KL.name;
      P.text(x, iy + 30, kt, 1, colOf(k->color ? mixc(k->color, C(250, 240, 220), 0.25f) : rgba(200, 180, 140)));
    } else if (known) P.text(x, iy + 30, "WILDLANDS", 1, kDim);
    if (!state.empty()) {
      std::string st = state;
      if (st.size() > fitC) st = st.substr(0, fitC);
      P.text(x, iy + 40, st, 1, colOf(stc ? stc : C(236, 120, 90)));
      P.text(x + L.colW, iy + 20, "LV " + std::to_string(s.level), 1, kDim, 2);
    } else if (known && s.settlement() && !moodWord(g, s.id, nullptr).empty()) {
      // (M5, 15.12) a settlement whose people the hero has seen: its mood in its colour ("CONTENT", "HUNGRY", "FESTIVAL")
      uint32_t mc = 0;
      const std::string mw = moodWord(g, s.id, &mc);
      P.text(x, iy + 40, "MOOD: ", 1, kDim);
      P.text(x + P.textW("MOOD: ", 1), iy + 40, mw, 1, colOf(mc));
      P.text(x + L.colW, iy + 20, "LV " + std::to_string(s.level), 1, kDim, 2);
    } else if (known) P.text(x, iy + 40, "LEVEL " + std::to_string(s.level) + (s.cleared ? "  CLEARED" : ""), 1, kDim);
    // the journey
    if (known) {
      const TravelQuote qf = g.travelQuote(mapSel_), qc = g.travelQuote(mapSel_, true);
      if (qf.ok) P.text(x, iy + 52, "ON FOOT: " + hoursText(qf.hours), 1, kText);
      else wrapText(x, iy + 50, L.colW, qf.why.empty() ? "YOU CANNOT TRAVEL THERE" : qf.why, Color(0.95f, 0.55f, 0.4f), -1, 9);
      // (M2 fixer round 3) a refused journey shows no FAST TRAVEL button at all (the reason stands above it), as the
      // CARRIAGE line already did; a tap there does nothing and the map stays open
      if (qf.ok) button(x, L.travelY, L.colW, L.tbH, "FAST TRAVEL", true);
      if (qc.ok) button(x, L.carriageY, L.colW, L.tbH, "CARRIAGE  " + std::to_string(qc.gold) + " GOLD", g.gold >= qc.gold);
      else {
        const std::string why = qc.why.empty() ? "NO CARRIAGE" : qc.why;
        wrapText(x, L.carriageY + 2, L.colW, "CARRIAGE: " + why, kDim, -1, 9);
      }
    }
  } else if (const realm::WorldEvent* ne = newsEventById(g, S.newsSel)) {
    // (fixer M4 r2) the tapped news scroll: what was heard, and when
    P.text(x, iy, "NEWS HEARD", 1, kDim);
    wrapText(x, iy + 12, L.colW, story::newsLine(g, *ne, 0), kText, -1, 9);   // (it says how long ago)
  } else {
    wrapText(x, iy, L.colW, touchUI ? "DRAG TO PAN, PINCH TO ZOOM. TAP A PLACE FOR THE JOURNEY THERE."
                                    : "DRAG OR ARROWS TO PAN, WHEEL OR Q/E TO ZOOM. CLICK A PLACE, OR W/S, THEN ENTER TO TRAVEL. G: GEOLOGY.", kDim, -1, 9);
  }
  // the legend: only what the map shows right now
  {
    float ly = L.mapY + L.mapH - 9;
    std::vector<int> leg;
    float legTop = L.infoY + 60;
    if (mapSel_ >= 0 && mapSel_ < (int)g.world.sites.size()) legTop = L.carriageY + (g.travelQuote(mapSel_, true).ok ? L.tbH + 8 : 34);
    const int rows = std::max(0, (int)((ly + 9 - legTop) / 10));
    // (M3c fixer round 3) the biomes first, then the places; the biomes take the rows the places leave (at least 3)
    const int ecoRows = std::max(std::min(3, rows), rows - (int)S.legend.size());
    for (int e : S.ecoLeg) if ((int)leg.size() < ecoRows) leg.push_back(-100 - e);
    leg.insert(leg.end(), S.legend.begin(), S.legend.end());
    if ((int)leg.size() > rows) leg.resize((size_t)rows);
    for (int i = (int)leg.size() - 1; i >= 0; i--) {
      const int ic = leg[(size_t)i];
      if (ic == -1) {   // a rumour
        for (int k = 0; k < 12; k++) if ((k & 3) != 3) { const float a = k / 12.0f * 6.2831853f; P.rect(std::floor(x + 3 + std::cos(a) * 3.5f), std::floor(ly + 3 + std::sin(a) * 3.5f), 1, 1, kText); }
        P.text(x + 12, ly, "RUMOURED PLACE", 1, kDim);
      } else if (ic == -2) {
        P.rect(x + 1, ly + 1, 5, 5, kGold);
        P.text(x + 12, ly, "TRACKED QUEST", 1, kDim);
      } else if (ic == -8) {   // (M4) the border: a dashed line in a realm's colour
        // (fixer M4 r3, review: "the legend's red dash does not match the purple borders") in the colour the map draws
        // the hero's own realm's border with (the same pale-field rule), dashed the same way
        uint32_t c0 = C(160, 40, 44);
        if (const Kingdom* K = g.lastTown >= 0 && g.lastTown < (int)g.world.sites.size() ? g.world.kingdomOf(g.lastTown) : nullptr) {
          const rui::Look Lk = rui::look(g, K->id);
          c0 = Lk.color ? Lk.color : (K->color ? K->color : c0);
          const int lum = ((int)(c0 & 255) * 3 + (int)((c0 >> 8) & 255) * 6 + (int)((c0 >> 16) & 255)) / 10;
          if (lum > 170 && Lk.color2) c0 = Lk.color2;
          const int lum2 = ((int)(c0 & 255) * 3 + (int)((c0 >> 8) & 255) * 6 + (int)((c0 >> 16) & 255)) / 10;
          if (lum2 > 150) c0 = shadec(c0, 0.62f);
        }
        const Color bc = colOf(mixc(c0, kInkC, 0.18f));
        for (int k = 0; k < 10; k++) if (k % 6 < 4) P.rect(x + k, ly + 3, 1, 2, bc);
        P.text(x + 12, ly, "BORDER", 1, kDim);
      } else if (ic == -5) {   // (M4) an occupied town's banner
        P.rect(x + 1, ly - 1, 1, 9, kText);
        P.rect(x + 2, ly - 1, 5, 5, Color(0.70f, 0.18f, 0.16f));
        P.text(x + 12, ly, "OCCUPIED", 1, kDim);
      } else if (ic <= -3 && ic >= -9) {   // (M4) siege, burned, refugees, news, hungry
        const int wi = ic == -3 ? WI_SIEGE : ic == -4 ? WI_BURNED : ic == -6 ? WI_REFUGEES : ic == -7 ? WI_NEWS : WI_HUNGRY;
        const Tex& t = S.warIcons[(size_t)wi];
        P.blit(t, std::floor(x + 4 - t.w / 2.0f), std::floor(ly + 3 - t.h / 2.0f));
        P.text(x + 12, ly, kWarIcons[wi].name, 1, kDim);
      } else if (ic <= -100) {   // (M3c) a biome: its ink swatch and its name (cut to the column at a word)
        const Eco e = (Eco)(-100 - ic);
        P.rect(x, ly, 8, 7, colOf(ecoInk(e)));
        P.frame(x, ly, 8, 7, Color(0.25f, 0.18f, 0.12f));
        std::string nm = ecoName(e);
        const size_t fit = (size_t)std::max(4, (int)((L.colW - 12) / 6));
        if (nm.size() > fit) { const size_t sp = nm.rfind(' ', fit); nm = sp != std::string::npos && sp > 3 ? nm.substr(0, sp) : nm.substr(0, fit); }
        P.text(x + 12, ly, nm, 1, kDim);
      } else {
        const Tex& t = S.icons[(size_t)ic];
        P.blit(t, std::floor(x + 4 - t.w / 2.0f), std::floor(ly + 3 - t.h / 2.0f));
        P.text(x + 12, ly, kIcons[ic].name, 1, kDim);
      }
      ly -= 10;
    }
  }
}

// start the journey to the selected place (behind the fade, Game::beginTravel)
static void travelTo(View& v, Game& g, int& sel, bool carriage, void (View::*snapFn)(Game&)) {
  // (M2 fixer round 3) a journey the quote refuses keeps the map open: the side column already says why
  if (!g.travelQuote(sel, carriage).ok) return;
  if (g.beginTravel(sel, carriage)) {
    if (!g.travelling()) (v.*snapFn)(g);   // an immediate journey: the camera follows now (a faded one snaps on arrival)
    sel = -1;
  } else g.mode = Mode::Play;   // (refused: back to the world, where the notice says why)
}

void View::mapTabTap(Game& g, Vec2 p, float top) {
  const TabLayout L = tabLayout(top, touchUI);
  auto inR = [&](float rx, float ry, float rw, float rh) { return p.x >= rx && p.x < rx + rw && p.y >= ry && p.y < ry + rh; };
  const float x = L.colX, bw = (L.colW - 8) / 3;
  if (inR(x, L.btnY, bw, L.bh)) { worldMapZoom(-1, Vec2(-1, -1)); return; }
  if (inR(x + bw + 4, L.btnY, bw, L.bh)) { worldMapZoom(1, Vec2(-1, -1)); return; }
  if (inR(x + 2 * (bw + 4), L.btnY, bw, L.bh)) { mapSel_ = -1; S.newsSel = 0; worldMapCentre(g, -1); return; }
  const bool sel = mapSel_ >= 0 && mapSel_ < (int)g.world.sites.size() && g.world.sites[(size_t)mapSel_].discovered && !S.geo;
  if (sel && inR(x, L.travelY, L.colW, L.tbH) && g.travelQuote(mapSel_).ok) { travelTo(*this, g, mapSel_, false, &View::snap); return; }
  if (sel && inR(x, L.carriageY, L.colW, L.tbH) && g.travelQuote(mapSel_, true).ok) { travelTo(*this, g, mapSel_, true, &View::snap); return; }
  // a tap on the map itself picks the place under it (drags pan instead: worldMapPointer, from event())
  // (M2 fixer) a near miss keeps the selection (and its FAST TRAVEL panel); only a tap on another place changes it
  if (p.x >= L.mapX && p.x < L.mapX + L.mapW && p.y >= L.mapY && p.y < L.mapY + L.mapH) {
    const int pick = worldMapPick(g, p);
    if (pick >= 0) { mapSel_ = pick; S.newsSel = 0; }
    else {   // (fixer M4 r2) a news scroll: its news in the side column
      float bd = (touchUI ? 14.0f : 10.0f) * (touchUI ? 14.0f : 10.0f);
      uint32_t best = 0;
      for (const auto& nw : S.news) { const float d = len2(nw.second - p); if (d < bd) { bd = d; best = nw.first; } }
      if (best) { S.newsSel = best; mapSel_ = -1; }
    }
  }
}

bool View::mapTabKey(Game& g, int key) {
  if (key != SDLK_UP && key != SDLK_DOWN && worldMapKey(g, key)) return true;
  if (key == SDLK_UP || key == SDLK_DOWN) { worldMapKey(g, key); return true; }   // the arrows pan
  // keyboard fast travel: W / S cycle the discovered places (nearest first), enter travels, T takes the carriage
  if (key == SDLK_W || key == SDLK_S) {
    std::vector<int> list;
    for (int i = 0; i < (int)g.world.sites.size(); i++) if (g.world.sites[i].discovered) list.push_back(i);
    const Vec2 pp = g.pl().p;
    std::sort(list.begin(), list.end(), [&](int a, int b) {
      const Site& A = g.world.sites[a]; const Site& B = g.world.sites[b];
      return len2(Vec2(A.ex * 16.0f, A.ey * 16.0f) - pp) < len2(Vec2(B.ex * 16.0f, B.ey * 16.0f) - pp);
    });
    if (!list.empty()) {
      int at = (int)(std::find(list.begin(), list.end(), mapSel_) - list.begin());
      int n = (int)list.size();
      if (at >= n) at = key == SDLK_W ? 0 : -1;
      at = key == SDLK_W ? (at + n - 1) % n : (at + 1) % n;
      mapSel_ = list[at];
      worldMapCentre(g, mapSel_);
      audio_->play(Sfx::MenuMove);
    }
    return true;
  }
  const bool sel = mapSel_ >= 0 && mapSel_ < (int)g.world.sites.size() && g.world.sites[(size_t)mapSel_].discovered;
  if ((key == SDLK_RETURN || key == SDLK_SPACE) && sel) { travelTo(*this, g, mapSel_, false, &View::snap); return true; }
  if (key == SDLK_T && sel) { travelTo(*this, g, mapSel_, true, &View::snap); return true; }
  return false;
}
