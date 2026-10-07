// Culture gallery (M3 "Many Peoples", architecture lane): every archetype's settlement art side by side, so the twelve
// civilisations can be judged at 1x and zoomed: houses in three facade variants (and a fourth, two storeys), the inn,
// shop, smithy and temple, the keep and the palace, a run of city wall with a gate and a 1:1 diagonal in the culture's
// wall style, and the small things of its streets (fence, well, lamp, bench, statue, shrine, fire bowl, banner, a
// market stall in its awning style).
//   culture_gallery <outDir>               culture_<archetype>.png (3x) + _1x.png, sheet.png (12 random cultures, 1x),
//                                           night.png (a street of each culture at night, 1x)
//   culture_gallery <outDir> <archetype>   one archetype only (index 0..11 or its name, e.g. JADE)
//   culture_gallery --hand ...             use the gallery's own hand-built styles even if the culture engine has
//                                           real ones (the default uses the engine's when it sets ArchStyle::culture)
#include <chrono>
#include "rpg/build/blueprint.h"
#include "rpg/build/parts.h"
#include "rpg/art/art_parts.h"
#include <cstring>
#include "rpg/culture/culture.h"
#include "rpg/sim/world.h"
#include "tools/preview/preview_util.h"

namespace {

using art::ArchStyle;
using art::Building;
using cult::Archetype;

bool g_hand = false;

uint32_t C3(int r, int g, int b) { return rgba(r, g, b); }

// ---------------------------------------------------------------- hand-built styles (until the culture lane lands)
ArchStyle handArch(Archetype a, uint32_t seed) {
  using namespace art;
  ArchStyle s;
  s.culture = (uint8_t)((int)a + 1);
  s.weather = (uint8_t)(seed % 3);
  s.variant = (uint8_t)(seed >> 3);
  const uint32_t h = seed * 2654435761u ^ (seed >> 7);
  auto pick = [&](int n, int salt) { return (int)(((h >> (salt * 3)) ^ (h * (uint32_t)(salt + 7))) % (uint32_t)n); };
  switch (a) {
    case Archetype::Fjordfolk:
      s.roof = pick(3, 1) == 0 ? RoofShape::Steep : RoofShape::Turf;
      s.roofMat = s.roof == RoofShape::Turf ? RoofMat::Turf : RoofMat::Shingle;
      s.wall = pick(3, 2) == 0 ? WallMat::Plank : WallMat::Log;
      s.pitch = 3; s.smoke = true; s.chimneys = 1;
      s.ornament = ORN_CARVED_RIDGE | ORN_DRAGON_HEADS;
      s.accentTint = C3(170, 56, 40); s.altTint = C3(220, 180, 80);
      s.door = DoorShape::Plank; s.window = WindowShape::Square; s.shutters = false;
      s.foundation = Foundation::Plinth; s.eave = 1;
      break;
    case Archetype::Highland:
      s.roof = RoofShape::Gable; s.roofMat = RoofMat::Thatch; s.wall = WallMat::Rubble;
      s.pitch = 3; s.chimneys = 2; s.smoke = true; s.wallH = 40;
      s.ornament = ORN_ROOF_STONES | ORN_CHIMNEY;
      s.accentTint = C3(46, 92, 70); s.altTint = C3(140, 40, 40);
      s.window = WindowShape::Square; s.door = DoorShape::Plank; s.shutters = false;
      break;
    case Archetype::Heartland:
      s.roof = pick(2, 1) ? RoofShape::Gable : RoofShape::Hip;
      s.roofMat = pick(3, 2) == 0 ? RoofMat::Thatch : RoofMat::Shingle;
      s.wall = pick(4, 3) == 0 ? WallMat::Plaster : WallMat::Timber;
      s.pitch = (uint8_t)(2 + pick(2, 4)); s.chimneys = 1;
      s.ornament = ORN_SHUTTERS | ORN_FLOWERBOX | ORN_CHIMNEY;
      s.accentTint = C3(64, 104, 146); s.altTint = C3(150, 60, 52);
      break;
    case Archetype::Imperial: {
      s.roof = RoofShape::Hip; s.roofMat = RoofMat::ClayTile; s.wall = pick(2, 1) ? WallMat::Ashlar : WallMat::Plaster;
      static const uint32_t ochre[3] = {C3(236, 214, 170), C3(228, 196, 150), C3(240, 226, 196)};
      if (s.wall == WallMat::Plaster) s.wallTint = ochre[pick(3, 2)];
      s.pitch = 1; s.chimneys = 0;
      s.window = pick(2, 3) ? WindowShape::Arched : WindowShape::Tall; s.door = DoorShape::Double;
      s.foundation = Foundation::Plinth;
      s.ornament = ORN_PORCH_COLUMNS | ORN_PAINTED_BANDS;
      s.accentTint = C3(150, 44, 38); s.altTint = C3(226, 190, 104);
      s.wallH = 170;
      break;
    }
    case Archetype::Dune: {
      const int r = pick(6, 1);
      s.roof = r == 0 ? RoofShape::Dome : (r == 1 ? RoofShape::Onion : RoofShape::FlatParapet);
      s.roofMat = RoofMat::Adobe; s.wall = WallMat::Adobe;
      static const uint32_t sand[4] = {C3(222, 186, 132), C3(214, 168, 120), C3(232, 206, 160), C3(240, 228, 206)};
      s.wallTint = sand[pick(4, 2)];
      s.pitch = 0; s.chimneys = 0; s.shutters = false;
      s.window = WindowShape::Screen; s.door = DoorShape::Arched;
      s.ornament = ORN_WINDCATCHER | ORN_CRENELS | ORN_AWNINGS;
      s.accentTint = C3(40, 110, 150); s.altTint = C3(214, 160, 60);
      s.roofTint = C3(52, 140, 168);
      break;
    }
    case Archetype::Steppe:
      s.roof = RoofShape::Conical; s.roofMat = RoofMat::Felt; s.wall = WallMat::Felt;
      s.pitch = 1; s.chimneys = 0; s.shutters = false; s.wallH = 30;
      s.door = DoorShape::Flap; s.window = WindowShape::Round;
      s.ornament = ORN_PAINTED_BANDS | ORN_PRAYER_FLAGS;
      s.accentTint = pick(2, 1) ? C3(196, 70, 40) : C3(44, 84, 168); s.altTint = C3(232, 196, 90);
      break;
    case Archetype::Marsh:
      s.roof = pick(3, 1) == 0 ? RoofShape::Hip : RoofShape::Steep;
      s.roofMat = pick(2, 2) ? RoofMat::Palm : RoofMat::Thatch; s.wall = WallMat::Plank;
      s.pitch = 4; s.stilts = true; s.foundation = Foundation::Stilts; s.shutters = false; s.chimneys = 0;
      s.door = DoorShape::Curtain; s.window = WindowShape::Square;
      s.ornament = ORN_LANTERNS;
      s.accentTint = C3(206, 128, 46); s.altTint = C3(70, 120, 90);
      s.weather = (uint8_t)(2 + pick(2, 3));
      break;
    case Archetype::Jade:
      s.roof = RoofShape::Pagoda; s.roofMat = RoofMat::GlazedTile; s.wall = pick(3, 1) ? WallMat::Plaster : WallMat::Brick;
      s.wallTint = s.wall == WallMat::Plaster ? C3(240, 236, 222) : 0;
      s.roofTint = pick(3, 2) == 0 ? C3(52, 92, 160) : C3(46, 138, 106);
      s.pitch = 1; s.eave = 2; s.chimneys = 0; s.shutters = false;
      s.window = WindowShape::Lattice; s.door = pick(2, 3) ? DoorShape::Moon : DoorShape::Double;
      s.foundation = Foundation::Platform;
      s.ornament = ORN_DRAGON_HEADS | ORN_LANTERNS | ORN_PORCH_COLUMNS;
      s.accentTint = C3(176, 40, 40); s.altTint = C3(232, 190, 80);
      break;
    case Archetype::River:
      s.roof = RoofShape::Mansard; s.roofMat = pick(3, 1) == 0 ? RoofMat::ClayTile : RoofMat::Slate; s.wall = WallMat::Brick;
      s.pitch = 3; s.chimneys = 2; s.wallH = 210;
      s.window = WindowShape::Tall; s.door = DoorShape::Plank;
      s.ornament = ORN_FLOWERBOX | ORN_SHUTTERS | ORN_CHIMNEY;
      s.accentTint = C3(40, 96, 72); s.altTint = C3(236, 232, 220);
      break;
    case Archetype::SunTemple: {
      const bool thatch = pick(3, 1) == 0;
      s.roof = thatch ? RoofShape::Hip : RoofShape::FlatParapet;
      s.roofMat = thatch ? RoofMat::Palm : RoofMat::Adobe; s.wall = WallMat::Ashlar;
      s.wallTint = C3(228, 210, 172);
      s.pitch = 3; s.chimneys = 0; s.shutters = false;
      s.window = WindowShape::Square; s.door = DoorShape::Curtain;
      s.foundation = Foundation::Terrace;
      s.ornament = ORN_PAINTED_BANDS | ORN_CRENELS;
      s.accentTint = C3(30, 150, 140); s.altTint = C3(214, 64, 46);
      break;
    }
    case Archetype::Sylvan:
      s.roof = RoofShape::Sweep; s.roofMat = pick(3, 1) == 0 ? RoofMat::Bark : RoofMat::Leaf; s.wall = WallMat::Living;
      s.roofTint = pick(4, 2) == 0 ? C3(178, 130, 60) : 0;
      s.pitch = 2; s.eave = 2; s.chimneys = 0; s.shutters = false;
      s.window = WindowShape::Round; s.door = DoorShape::Round;
      s.ornament = ORN_VINES | ORN_LANTERNS;
      s.accentTint = C3(90, 160, 120); s.altTint = C3(230, 210, 140);
      break;
    case Archetype::Starspire:
      s.roof = pick(3, 1) == 0 ? RoofShape::Hip : RoofShape::Steep; s.roofMat = RoofMat::Slate; s.wall = WallMat::Ashlar;
      s.wallTint = C3(236, 238, 244); s.roofTint = C3(70, 104, 176);
      s.pitch = 4; s.chimneys = 0; s.wallH = 230;
      s.window = WindowShape::Pointed; s.door = DoorShape::Arched;
      s.foundation = Foundation::Plinth;
      s.ornament = ORN_FINIALS | ORN_GILDING;
      s.accentTint = C3(60, 90, 170); s.altTint = C3(232, 206, 120);
      break;
    default: break;
  }
  return s;
}

art::PropStyle handProps(Archetype a) {
  using namespace art;
  PropStyle p;
  p.culture = (uint8_t)((int)a + 1);
  switch (a) {
    case Archetype::Fjordfolk: p.fence = Fence::Wattle; p.well = 1; p.lamp = 5; p.bench = 3; p.centre = 6; p.awning = 5; p.cloth = C3(170, 56, 40); break;
    case Archetype::Highland: p.fence = Fence::StoneDyke; p.well = 1; p.lamp = 5; p.bench = 1; p.centre = 6; p.awning = 1; p.cloth = C3(46, 92, 70); break;
    case Archetype::Heartland: p.fence = Fence::Picket; p.well = 0; p.lamp = 0; p.bench = 0; p.centre = 0; p.awning = 0; break;
    case Archetype::Imperial: p.fence = Fence::StoneDyke; p.well = 3; p.lamp = 0; p.bench = 1; p.centre = 1; p.awning = 1; p.cloth = C3(150, 44, 38); p.stone = C3(226, 214, 190); break;
    case Archetype::Dune: p.fence = Fence::Hedge; p.well = 4; p.lamp = 2; p.bench = 2; p.centre = 0; p.awning = 4; p.cloth = C3(40, 110, 150); p.awningA = C3(40, 110, 150); p.awningB = C3(232, 206, 150); break;
    case Archetype::Steppe: p.fence = Fence::Rope; p.well = 2; p.lamp = 2; p.bench = 2; p.centre = 4; p.awning = 5; p.cloth = C3(196, 70, 40); break;
    case Archetype::Marsh: p.fence = Fence::Bamboo; p.well = 1; p.lamp = 1; p.bench = 3; p.centre = 3; p.awning = 2; p.cloth = C3(206, 128, 46); break;
    case Archetype::Jade: p.fence = Fence::Bamboo; p.well = 4; p.lamp = 3; p.bench = 1; p.centre = 1; p.awning = 3; p.cloth = C3(176, 40, 40); p.awningA = C3(176, 40, 40); break;
    case Archetype::River: p.fence = Fence::Picket; p.well = 0; p.lamp = 0; p.bench = 0; p.centre = 0; p.awning = 0; p.awningA = C3(40, 96, 72); p.awningB = C3(236, 232, 220); break;
    case Archetype::SunTemple: p.fence = Fence::StoneDyke; p.well = 3; p.lamp = 2; p.bench = 2; p.centre = 5; p.awning = 2; p.cloth = C3(30, 150, 140); break;
    case Archetype::Sylvan: p.fence = Fence::Hedge; p.well = 3; p.lamp = 4; p.bench = 3; p.centre = 3; p.awning = 4; p.cloth = C3(90, 160, 120); break;
    case Archetype::Starspire: p.fence = Fence::Picket; p.well = 3; p.lamp = 4; p.bench = 1; p.centre = 5; p.awning = 4; p.cloth = C3(60, 90, 170); p.stone = C3(236, 238, 244); break;
    default: break;
  }
  return p;
}

art::CityWall handWall(Archetype a) {
  using art::CityWall;
  switch (a) {
    case Archetype::Fjordfolk: case Archetype::Marsh: return CityWall::Palisade;
    case Archetype::Steppe: return CityWall::Rampart;
    case Archetype::Sylvan: return CityWall::Thorn;
    case Archetype::Dune: return CityWall::Adobe;
    case Archetype::SunTemple: return CityWall::Talud;
    case Archetype::Starspire: return CityWall::WhiteStone;
    case Archetype::Jade: return CityWall::Jade;
    default: return CityWall::Stone;
  }
}

// one building's style: the culture engine's (cult::buildingArch) when it fills ArchStyle::culture, else the hand-built
ArchStyle styleFor(const cult::Culture& c, Archetype a, int urban, int wealth, uint32_t seed) {
  if (!g_hand) {
    ArchStyle s = cult::buildingArch(c, c.homeBiome, urban, wealth, seed);
    if (s.culture) return s;
  }
  return handArch(a, seed);
}
art::PropStyle propsFor(const cult::Culture& c, Archetype a) {
  if (!g_hand && c.props.culture) return c.props;
  return handProps(a);
}
art::CityWall wallFor(const cult::Culture& c, Archetype a) {
  if (!g_hand && c.arch.culture) return c.town.wall;
  return handWall(a);
}

// ---------------------------------------------------------------- placing things on a board
uint32_t shadeGround(uint32_t c, int level) {
  if (!level) return c;
  uint32_t s = art::mix(c, rgba(48, 34, 92), level == 2 ? 0.42f : 0.30f);
  return art::shade(s, level == 2 ? 0.70f : 0.80f);
}
void bldgShadow(Board& b, int fx, int fy, int fw, int fh, int height) {
  int L = std::clamp(height / 4, 6, 14), Ly = std::max(3, L * 3 / 5);
  for (int y = fy; y < fy + fh + Ly + 2; y++)
    for (int x = fx; x < fx + fw + L + 2; x++) {
      bool inFoot = x >= fx && x < fx + fw && y >= fy && y < fy + fh;
      if (inFoot) continue;
      int lvl = 0;
      if (y >= fy + fh && y < fy + fh + 2 && x >= fx && x < fx + fw + 1) lvl = 2;
      else
        for (int k = 1; k <= 8 && !lvl; k++) {
          int sx = x - L * k / 8, sy = y - Ly * k / 8;
          if (sx >= fx && sx < fx + fw && sy >= fy && sy < fy + fh) lvl = 1;
        }
      if (lvl) b.c.set(x, y, shadeGround(b.c.get(x, y), lvl));
    }
}
double g_paintMs = 0, g_worstPaint = 0;
// paints a building with its footprint's bottom-left at (fx, groundY); returns its width in px
int putBuilding(Board& b, Building t, int wT, int hT, const ArchStyle& st, uint32_t seed, int fx, int groundY, int storeys = 0,
                bool night = false, uint32_t banner = 0, uint32_t banner2 = 0, int emblem = 0) {
  art::BuildingInfo info;
  art::BuildingFacts f;
  f.storeys = storeys;
  f.banner = banner; f.banner2 = banner2; f.emblem = emblem;
  if (t == Building::Temple || t == Building::Tower) f.hearth = false;
  const auto t0 = std::chrono::steady_clock::now();
  Canvas c = art::buildingSprite(bld::design(bld::simpleRequest(t, wT, hT, st, seed, f)), &info);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  g_paintMs += ms;
  g_worstPaint = std::max(g_worstPaint, ms);
  if (night) c = art::buildingNight(c, info.glass, seed);
  const int fy = groundY - hT * 16;
  bldgShadow(b, fx, fy, wT * 16, hT * 16, info.height);
  b.put(c, fx - art::BLDG_PAD_X, fy + hT * 16 + art::BLDG_PAD_B - c.h);
  return wT * 16;
}
void putProp(Board& b, const Canvas& c, int fw, int cx, int groundY) {
  // a prop's canvas: bottom-centre on the tile's bottom-centre; draw the first frame only
  Canvas f(fw, c.h);
  for (int y = 0; y < c.h; y++) for (int x = 0; x < fw; x++) f.set(x, y, c.get(x, y));
  b.put(f, cx - fw / 2, groundY - c.h);
}

// ---------------------------------------------------------------- walls
struct WallGrid {
  int W, H;
  std::vector<uint8_t> wall;
  std::vector<std::pair<int, int>> gates;
  WallGrid(int w, int h) : W(w), H(h), wall((size_t)w * h, 0) {}
  void set(int x, int y, int v) { if (x >= 0 && y >= 0 && x < W && y < H) wall[(size_t)y * W + x] = (uint8_t)v; }
};
void drawWalls(Board& b, const WallGrid& g, int ox, int oy, art::CityWall style, const cult::Heraldry& her) {
  for (int y = 0; y < g.H * 16; y++)
    for (int x = 0; x < g.W * 16; x++) {
      int lvl = art::wallShadeAt(g.wall.data(), g.W, g.H, x, y);
      if (lvl) b.c.set(ox + x, oy + y, shadeGround(b.c.get(ox + x, oy + y), lvl));
    }
  for (auto& gt : g.gates)
    for (int y = 0; y < 16; y++)
      for (int x = 3; x < 45; x++) b.c.set(ox + gt.first * 16 + x, oy + gt.second * 16 + y, shadeGround(b.c.get(ox + gt.first * 16 + x, oy + gt.second * 16 + y), y < 12 ? 2 : 1));
  std::vector<uint32_t> keys;
  art::wallKeys(g.wall.data(), g.W, g.H, g.gates.data(), (int)g.gates.size(), keys);
  for (uint32_t& k : keys) if (k) k |= ((uint32_t)style << art::WALL_STYLE_SHIFT) & art::WALL_STYLE_MASK;
  for (int y = 0; y < g.H; y++) {
    for (int pass = 0; pass < 2; pass++)
      for (int x = 0; x < g.W; x++) {
        uint32_t k = keys[(size_t)y * g.W + x];
        if (!k || ((k & art::WALL_BIT_TOWER) != 0) != (pass == 1)) continue;
        b.put(art::wallTile(k), ox + x * 16 - art::WALL_OX, oy + y * 16 - art::WALL_OY);
      }
    for (auto& gt : g.gates)
      if (gt.second == y) b.put(art::gateHouse(7, her.field, her.charge, her.emblem, style), ox + gt.first * 16 - art::GATE_OX, oy + gt.second * 16 - art::GATE_OY);
  }
}
// (M3b forts) the walls from the builder's parts: every tile and the gate in the parts' forms, the cast shadows
void drawWallsParts(Board& b, const WallGrid& g, int ox, int oy, const bld::FortParts& f, const cult::Heraldry& her) {
  for (int y = 0; y < g.H * 16; y++)
    for (int x = 0; x < g.W * 16; x++) {
      int lvl = art::wallShadeAt(g.wall.data(), g.W, g.H, x, y);
      if (lvl) b.c.set(ox + x, oy + y, shadeGround(b.c.get(ox + x, oy + y), lvl));
    }
  std::vector<uint32_t> keys;
  art::wallKeys(g.wall.data(), g.W, g.H, g.gates.data(), (int)g.gates.size(), keys);
  for (int y = 0; y < g.H; y++) {
    for (int pass = 0; pass < 2; pass++)
      for (int x = 0; x < g.W; x++) {
        uint32_t k = keys[(size_t)y * g.W + x];
        if (!k || ((k & art::WALL_BIT_TOWER) != 0) != (pass == 1)) continue;
        b.put(art::wallTile(k, f), ox + x * 16 - art::WALL_OX, oy + y * 16 - art::WALL_OY);
      }
    for (auto& gt : g.gates)
      if (gt.second == y) b.put(art::gateHouse(7, her, f), ox + gt.first * 16 - art::GATE_OX, oy + gt.second * 16 - art::GATE_OY);
  }
}
// a stretch of town wall: a run along the bottom with a gate, a 1:1 diagonal (4-connected staircase) climbing to the
// north-east, a run along the top and an end tower; and an 8-connected diagonal on the left
WallGrid wallRun() {
  WallGrid g(14, 9);
  for (int x = 1; x < 8; x++) g.set(x, 7, 1);
  for (int k = 0; k < 3; k++) g.set(2 + k, 7, 0);
  g.gates.push_back({2, 7});
  int x = 7, y = 7;
  for (int k = 0; k < 4; k++) { g.set(x + 1, y, 1); g.set(x + 1, y - 1, 1); x++; y--; }   // 1:1 staircase up to the right
  for (int xx = x; xx < 13; xx++) g.set(xx, y, 1);
  g.set(0, 6, 1); g.set(1, 7, 1);   // a diagonal link down to the run on the left
  return g;
}

// ---------------------------------------------------------------- one archetype's board
const char* nameOf(Archetype a) { return cult::archetypeName(a); }

Canvas archetypeBoard(Archetype a, bool night) {
  const uint32_t seed = 1000u + (uint32_t)a * 7919u;
  const cult::Culture c = cult::Atlas::make(a, seed, 2);
  const art::PropStyle ps = propsFor(c, a);
  Board b(1180, 560);
  char title[96];
  std::snprintf(title, sizeof title, "%s  (%s, %s)", nameOf(a), (!g_hand && c.arch.culture) ? "ENGINE STYLES" : "HAND STYLES", c.name.c_str());
  b.text(8, 6, title);
  // row 1: four houses (three facade variants of one size and a two-storey one), the inn, shop, smithy, temple
  int x = 16;
  const int g1 = 230;
  for (int k = 0; k < 4; k++) {
    const uint32_t s = seed + 101u * (uint32_t)(k + 1);
    const int wT = k == 3 ? 5 : 4;
    x += putBuilding(b, Building::House, wT, 3, styleFor(c, a, 1, 1, s), s, x, g1, k == 3 ? 2 : 1, night) + 18;
  }
  x += putBuilding(b, Building::Inn, 6, 3, styleFor(c, a, 1, 2, seed + 11), seed + 11, x, g1, 2, night) + 18;
  x += putBuilding(b, Building::Shop, 4, 3, styleFor(c, a, 1, 2, seed + 13), seed + 13, x, g1, 1, night) + 18;
  x += putBuilding(b, Building::Smithy, 5, 3, styleFor(c, a, 1, 1, seed + 17), seed + 17, x, g1, 1, night) + 18;
  x += putBuilding(b, Building::Temple, 5, 4, styleFor(c, a, 2, 2, seed + 19), seed + 19, x, g1, 1, night) + 18;
  if (!night) x += putBuilding(b, Building::Barracks, 7, 4, styleFor(c, a, 3, 2, seed + 31), seed + 31, x, g1, 2, false, c.heraldry.field, c.heraldry.charge, c.heraldry.emblem) + 18;   // (M3 fixer r3)
  // row 2: the palace, the keep, a stretch of town wall, the street props
  const int g2 = 520;
  const cult::Heraldry& her = c.heraldry;
  x = 24;
  x += putBuilding(b, Building::Palace, 15, 7, styleFor(c, a, 3, 3, seed + 23), seed + 23, x, g2, 2, night, her.field, her.charge, her.emblem) + 26;
  x += putBuilding(b, Building::Keep, 7, 4, styleFor(c, a, 2, 3, seed + 29), seed + 29, x, g2 - 24, 2, night, her.field, her.charge, her.emblem) + 20;
  drawWalls(b, wallRun(), x, g2 - 9 * 16 - 8, wallFor(c, a), her);
  x += 14 * 16 + 18;
  // props: fence run, well, lamp, bench, centrepiece, shrine, fire bowl, banner, a stall
  const int pg = g2 - 70;
  for (int k = 0; k < 3; k++) putProp(b, art::propSprite(art::Prop::FenceH, ps), art::propW(art::Prop::FenceH), x + 8 + k * 16, pg);
  putProp(b, art::propSprite(art::Prop::FenceV, ps), art::propW(art::Prop::FenceV), x + 56, pg);
  putProp(b, art::propSprite(art::Prop::FenceV, ps), art::propW(art::Prop::FenceV), x + 56, pg - 16);
  putProp(b, art::propSprite(art::Prop::Well, ps), art::propW(art::Prop::Well), x + 100, pg);
  putProp(b, art::propSprite(art::Prop::Lamppost, ps), art::propW(art::Prop::Lamppost), x + 136, pg);
  putProp(b, art::propSprite(art::Prop::Bench, ps), art::propW(art::Prop::Bench), x + 160, pg);
  const art::Prop centre = ps.centre == 0 ? art::Prop::Fountain : ps.centre == 2 ? art::Prop::Well : ps.centre == 3 ? art::Prop::OakTree
                         : ps.centre == 4 ? art::Prop::Brazier : ps.centre == 6 ? art::Prop::StandingStone : art::Prop::Statue;
  putProp(b, art::propSprite(centre, ps), art::propW(centre), x + 16, pg + 64);
  putProp(b, art::propSprite(art::Prop::Statue, ps), art::propW(art::Prop::Statue), x + 60, pg + 64);
  putProp(b, art::propSprite(art::Prop::Shrine, ps), art::propW(art::Prop::Shrine), x + 92, pg + 64);
  putProp(b, art::propSprite(art::Prop::Brazier, ps), art::propW(art::Prop::Brazier), x + 122, pg + 64);
  putProp(b, art::bannerSprite(her), art::propW(art::Prop::Banner), x + 146, pg + 64);
  putProp(b, art::propSprite(art::Prop::Cushion, ps), art::propW(art::Prop::Cushion), x + 168, pg + 64);
  {
    Canvas st = art::propSprite(art::Prop::StallCloth, ps);
    b.put(st, x + 4, g2 - 230 - st.h + 40);
  }
  {
    Canvas sh = art::shieldArms(her, 24);
    b.put(sh, x + 70, g2 - 230);
    b.put(art::glyphSprite(c.faith.glyphSeed ? c.faith.glyphSeed : seed, (int)a % 4, rgba(236, 210, 120), 2), x + 100, g2 - 228);
  }
  return b.c;
}

// ---------------------------------------------------------------- (M3b forts) the cultures' fortifications
const char* gateFormName(bld::GateForm g) {
  static const char* n[] = {"DRUM TOWERS", "SQUARE TOWERS", "PYLONS", "TIMBER GATE", "IWAN", "PAIFANG", "MOON GATE", "ELVEN ARCH", "LIVING ARCH", "EARTHWORK", "HEDGE GAP"};
  return (int)g < (int)bld::GateForm::COUNT ? n[(int)g] : "?";
}
const char* towerFormName(bld::TowerForm t) {
  static const char* n[] = {"ROUND DRUM", "CONE DRUM", "SQUARE", "PAGODA", "MINARET", "BASTION", "PLATFORM"};
  return (int)t < (int)bld::TowerForm::COUNT ? n[(int)t] : "?";
}
const char* wallName(art::CityWall w) {
  static const char* n[] = {"STONE", "PALISADE", "RAMPART", "THORN", "ADOBE", "WHITESTONE", "JADE", "TALUD"};
  return (int)w < 8 ? n[(int)w] : "?";
}
// a town's wall: a long run along the bottom with a gate, a corner and a run north on the left with a diagonal step,
// a 1:1 staircase climbing north-east on the right into a run along the top
WallGrid fortRun() {
  WallGrid g(26, 12);
  for (int x = 2; x < 15; x++) g.set(x, 10, 1);
  for (int k = 0; k < 3; k++) g.set(6 + k, 10, 0);
  g.gates.push_back({6, 10});
  for (int y = 4; y <= 10; y++) g.set(2, y, 1);
  g.set(3, 3, 1); g.set(4, 2, 1); g.set(5, 2, 1);   // an 8-connected diagonal step and a stub
  int x = 14, y = 10;
  for (int k = 0; k < 4; k++) { g.set(x + 1, y, 1); g.set(x + 1, y - 1, 1); x++; y--; }
  for (int xx = x; xx < 24; xx++) g.set(xx, y, 1);
  for (int yy = 1; yy <= y; yy++) g.set(23, yy, 1);
  return g;
}
Canvas fortBoard(Archetype a) {
  const uint32_t seed = 1000u + (uint32_t)a * 7919u;
  const cult::Culture c = cult::Atlas::make(a, seed, 2);
  const WallGrid g = fortRun();
  const int cellW = g.W * 16 + 24, cellH = g.H * 16 + 70;
  Board b(24 + cellW * 3, 30 + cellH);
  b.text(8, 6, std::string(nameOf(a)) + "  FORTIFICATIONS (TOWN, CITY, CAPITAL)");
  for (int u = 1; u <= 3; u++) {
    const bld::FortParts f = bld::fortParts(c, u, seed + (uint32_t)u);
    const int ox = 12 + (u - 1) * cellW, oy = 30 + 56;
    b.text(ox, 22, std::string(u == 1 ? "TOWN: " : (u == 2 ? "CITY: " : "CAPITAL: ")) + wallName(f.wall));
    b.text(ox, 32, std::string(gateFormName(f.gate)) + " / " + towerFormName(f.tower));
    drawWallsParts(b, g, ox, oy, f, u == 3 ? c.heraldry : cult::Heraldry{});
  }
  return b.c;
}
// every gate form and every tower form on the walls they are built in
Canvas formsBoard() {
  struct G { bld::GateForm g; art::CityWall w; int cul; };
  const G gates[] = {{bld::GateForm::DrumTowers, art::CityWall::Stone, 3}, {bld::GateForm::SquareTowers, art::CityWall::Stone, 4},
                     {bld::GateForm::Pylons, art::CityWall::Talud, 10}, {bld::GateForm::TimberGate, art::CityWall::Palisade, 1},
                     {bld::GateForm::Iwan, art::CityWall::Adobe, 5}, {bld::GateForm::Paifang, art::CityWall::Jade, 8},
                     {bld::GateForm::MoonGate, art::CityWall::Jade, 8}, {bld::GateForm::ElvenArch, art::CityWall::WhiteStone, 12},
                     {bld::GateForm::LivingArch, art::CityWall::Thorn, 11}, {bld::GateForm::Earthwork, art::CityWall::Rampart, 2},
                     {bld::GateForm::HedgeGap, art::CityWall::Thorn, 6}, {bld::GateForm::TimberGate, art::CityWall::Palisade, 6},
                     {bld::GateForm::SquareTowers, art::CityWall::Stone, 9}, {bld::GateForm::SquareTowers, art::CityWall::Stone, 2}};
  const int nG = (int)(sizeof(gates) / sizeof(gates[0]));
  const int cw = 11 * 16 + 20, ch = 120;
  Board b(20 + 4 * cw, 20 + ((nG + 3) / 4) * ch + 8 * 110);
  for (int i = 0; i < nG; i++) {
    WallGrid g(11, 3);
    for (int x = 0; x < 11; x++) g.set(x, 1, 1);
    for (int k = 0; k < 3; k++) g.set(4 + k, 1, 0);
    g.gates.push_back({4, 1});
    const cult::Culture c = cult::Atlas::make((Archetype)(gates[i].cul - 1), 77u + (uint32_t)i, 2);
    bld::FortParts f = bld::fortParts(c, 2, 5u);
    f.wall = gates[i].w; f.gate = gates[i].g;
    const int ox = 10 + (i % 4) * cw, oy = 10 + (i / 4) * ch + 60;
    b.text(ox, oy - 58, std::string(gateFormName(f.gate)) + " " + wallName(f.wall));
    drawWallsParts(b, g, ox, oy, f, cult::Heraldry{});
  }
  // the tower forms: a run with an end tower, a corner tower and a tower mid-run, on each material it is built in
  struct T { bld::TowerForm t; art::CityWall w; int cul; };
  const T towers[] = {{bld::TowerForm::RoundDrum, art::CityWall::Stone, 3}, {bld::TowerForm::RoundDrum, art::CityWall::Adobe, 5},
                      {bld::TowerForm::RoundDrum, art::CityWall::Thorn, 11}, {bld::TowerForm::ConeDrum, art::CityWall::Palisade, 1},
                      {bld::TowerForm::ConeDrum, art::CityWall::WhiteStone, 12}, {bld::TowerForm::ConeDrum, art::CityWall::Stone, 3},
                      {bld::TowerForm::Square, art::CityWall::Stone, 4}, {bld::TowerForm::Square, art::CityWall::Stone, 9},
                      {bld::TowerForm::Square, art::CityWall::Stone, 2}, {bld::TowerForm::Square, art::CityWall::Talud, 10},
                      {bld::TowerForm::Pagoda, art::CityWall::Jade, 8}, {bld::TowerForm::Minaret, art::CityWall::Adobe, 5},
                      {bld::TowerForm::Bastion, art::CityWall::Rampart, 2}, {bld::TowerForm::Bastion, art::CityWall::Stone, 2},
                      {bld::TowerForm::Platform, art::CityWall::Palisade, 1}, {bld::TowerForm::Platform, art::CityWall::Thorn, 6}};
  const int nT = (int)(sizeof(towers) / sizeof(towers[0]));
  const int y0 = 20 + ((nG + 3) / 4) * ch;
  for (int i = 0; i < nT; i++) {
    WallGrid g(11, 5);
    for (int x = 0; x < 11; x++) g.set(x, 3, 1);
    for (int y = 1; y <= 3; y++) g.set(10, y, 1);
    const cult::Culture c = cult::Atlas::make((Archetype)(towers[i].cul - 1), 99u + (uint32_t)i, 2);
    bld::FortParts f = bld::fortParts(c, 2, 5u);
    f.wall = towers[i].w; f.tower = towers[i].t;
    const int ox = 10 + (i % 4) * cw, oy = y0 + (i / 4) * 110 + 30;
    b.text(ox, oy - 26, std::string(towerFormName(f.tower)) + " " + wallName(f.wall));
    drawWallsParts(b, g, ox, oy, f, cult::Heraldry{});
  }
  return b.c;
}

// every built street prop (signs, tents, banners, graves, crosses, work yards, tables, monuments) in the twelve
// cultures: a row per prop, a column per archetype (classic first)
Canvas streetPropsBoard() {
  static const art::Prop props[] = {art::Prop::Signpost, art::Prop::TollPost, art::Prop::Tent, art::Prop::MarketStall, art::Prop::Banner,
                                    art::Prop::Gravestone, art::Prop::GraveCairn, art::Prop::MarketCross, art::Prop::StandingStone,
                                    art::Prop::Well, art::Prop::Lamppost, art::Prop::Bench, art::Prop::Statue, art::Prop::Fountain,
                                    art::Prop::Shrine, art::Prop::Brazier, art::Prop::FenceH, art::Prop::DryingRack, art::Prop::HideRack,
                                    art::Prop::Trough, art::Prop::WaterWheel, art::Prop::PenShelter, art::Prop::FishingShack, art::Prop::HerbBed,
                                    art::Prop::MineEntrance, art::Prop::MarketTable, art::Prop::GroundCloth};
  const int nP = (int)(sizeof(props) / sizeof(props[0]));
  int rowH[64] = {}, y = 16;
  for (int i = 0; i < nP; i++) { rowH[i] = std::max(24, art::propH(props[i]) + 6); y += rowH[i]; }
  const int colW = 60;
  Board b(110 + 13 * colW, y + 10);
  for (int a = -1; a < 12; a++) b.text(110 + (a + 1) * colW, 4, a < 0 ? "CLASSIC" : std::string(nameOf((Archetype)a)).substr(0, 9));
  y = 16;
  for (int i = 0; i < nP; i++) {
    const art::Prop p = props[i];
    b.text(4, y + rowH[i] / 2 - 4, std::to_string((int)p));
    for (int a = -1; a < 12; a++) {
      art::PropStyle st;
      if (a >= 0) st = cult::Atlas::make((Archetype)a, 300u + (uint32_t)a, 2).props;
      const Canvas c = a < 0 ? art::propSprite(p) : art::propSprite(p, st);
      const int fw = art::propW(p);
      putProp(b, c, fw, 110 + (a + 1) * colW + colW / 2, y + rowH[i] - 3);
    }
    y += rowH[i];
  }
  return b.c;
}

// --check: every gate form joins its wall run on every material it can stand in, and every tower form caps its runs.
// A run of wall with the gate (or the towers) is painted and every column of the run outside the passage must be
// covered by the wall's own pixels from its walk down to its foot (no gap, no uncovered tile, no orphan piece left
// floating), and the gate's flanking tiles must be covered where the runs meet them.
int fortCheck() {
  int bad = 0, runs = 0;
  auto opaque = [](uint32_t p) { return (p >> 24) != 0; };
  for (int gi = 0; gi < (int)bld::GateForm::COUNT; gi++)
    for (int wi = 0; wi < (int)art::CityWall::COUNT; wi++) {
      bld::FortParts f = bld::fortDefaults((art::CityWall)wi);
      f.gate = (bld::GateForm)gi;
      for (int cul = 0; cul <= 12; cul += 6) {
        f.culture = (uint8_t)cul;
        WallGrid g(13, 3);
        for (int x = 0; x < 13; x++) g.set(x, 1, 1);
        for (int k = 0; k < 3; k++) g.set(5 + k, 1, 0);
        g.gates.push_back({5, 1});
        Canvas cv(13 * 16 + 64, 3 * 16 + 96);
        const int ox = 32, oy = 80;
        std::vector<uint32_t> keys;
        art::wallKeys(g.wall.data(), g.W, g.H, g.gates.data(), 1, keys);
        auto put = [&](const Canvas& s, int x, int y) {
          for (int j = 0; j < s.h; j++)
            for (int i = 0; i < s.w; i++) if (opaque(s.get(i, j))) cv.set(x + i, y + j, s.get(i, j));
        };
        for (int x = 0; x < 13; x++) if (keys[(size_t)13 + x]) put(art::wallTile(keys[(size_t)13 + x], f), ox + x * 16 - art::WALL_OX, oy + 16 - art::WALL_OY);
        put(art::gateHouse(7, cult::Heraldry{}, f), ox + 5 * 16 - art::GATE_OX, oy + 16 - art::GATE_OY);
        runs++;
        // every column of the wall outside the passage: covered from the walk's top edge to the foot
        const int walk = art::wallWalkHeight(f);
        int gaps = 0, firstGap = -1;
        for (int px = 2 * 16; px < 11 * 16; px++) {
          if (px >= 5 * 16 && px < 8 * 16) continue;
          int n = 0;
          for (int y = oy + 32 - walk - 2; y < oy + 32; y++) if (opaque(cv.get(ox + px, y))) n++;
          if (n < walk - 2) { gaps++; if (firstGap < 0) firstGap = px; }
        }
        if (gaps) {
          std::printf("FAIL: gate %s on %s (culture %d): %d wall columns uncovered (first at x %d)\n", gateFormName((bld::GateForm)gi), wallName((art::CityWall)wi), cul, gaps, firstGap);
          bad++;
        }
      }
    }
  for (int ti = 0; ti < (int)bld::TowerForm::COUNT; ti++)
    for (int wi = 0; wi < (int)art::CityWall::COUNT; wi++) {
      bld::FortParts f = bld::fortDefaults((art::CityWall)wi);
      f.tower = (bld::TowerForm)ti;
      f.roof = (ti == (int)bld::TowerForm::Square) ? rgba(178, 82, 54) : 0;
      // an L: a run east-west with its end free (an end tower), a corner tower, a run north
      WallGrid g(14, 8);
      for (int x = 1; x < 13; x++) g.set(x, 6, 1);
      for (int y = 1; y <= 6; y++) g.set(12, y, 1);
      std::vector<uint32_t> keys;
      art::wallKeys(g.wall.data(), g.W, g.H, nullptr, 0, keys);
      Canvas cv(14 * 16 + 64, 8 * 16 + 96);
      const int ox = 32, oy = 80;
      auto put = [&](const Canvas& s, int x, int y) {
        for (int j = 0; j < s.h; j++)
          for (int i = 0; i < s.w; i++) if (opaque(s.get(i, j))) cv.set(x + i, y + j, s.get(i, j));
      };
      int towersN = 0;
      for (int y = 0; y < g.H; y++)
        for (int pass = 0; pass < 2; pass++)
          for (int x = 0; x < g.W; x++) {
            const uint32_t k = keys[(size_t)y * g.W + x];
            if (!k || ((k & art::WALL_BIT_TOWER) != 0) != (pass == 1)) continue;
            if (k & art::WALL_BIT_TOWER) towersN++;
            put(art::wallTile(k, f), ox + x * 16 - art::WALL_OX, oy + y * 16 - art::WALL_OY);
          }
      runs++;
      // every wall tile's footprint is covered (a tower caps its tile whole) and the east-west run shows no column gap
      int unc = 0;
      for (int y = 0; y < g.H; y++)
        for (int x = 0; x < g.W; x++) {
          if (!keys[(size_t)y * g.W + x]) continue;
          // the tile's top row on screen sits between its top (z = walk) and its foot: count covered pixels in its box
          int n = 0;
          for (int j = 0; j < 16; j++)
            for (int i = 0; i < 16; i++) if (opaque(cv.get(ox + x * 16 + i, oy + y * 16 + j - 8))) n++;
          if (n < 16 * 16 * 3 / 4) unc++;
        }
      const int walk = art::wallWalkHeight(f);
      int gaps = 0, firstGap = -1;
      for (int px = 2 * 16; px < 11 * 16; px++) {   // (the corner tile is bevelled by design)
        int n = 0;
        for (int y = oy + 7 * 16 - walk - 2; y < oy + 7 * 16; y++) if (opaque(cv.get(ox + px, y))) n++;
        if (n < walk - 2) { gaps++; if (firstGap < 0) firstGap = px; }
      }
      if (unc || gaps || towersN < 2) {
        std::printf("FAIL: tower %s on %s: %d uncovered tiles, %d gap columns (first x %d), %d towers\n", towerFormName((bld::TowerForm)ti), wallName((art::CityWall)wi), unc, gaps, firstGap, towersN);
        bad++;
      }
    }
  // and the cultures' own choices: every archetype x town / city / capital, its gate and towers on its walls
  for (int a = 0; a < (int)Archetype::COUNT; a++)
    for (int u = 0; u <= 3; u++) {
      const cult::Culture c = cult::Atlas::make((Archetype)a, 4000u + (uint32_t)a, 2);
      const bld::FortParts f = bld::fortParts(c, u, 11u);
      const Canvas gh = art::gateHouse(7, c.heraldry, f);
      bool any = false;
      for (uint32_t p : gh.px) if (p >> 24) { any = true; break; }
      if (!any) { std::printf("FAIL: %s urban %d: empty gate\n", nameOf((Archetype)a), u); bad++; }
    }
  std::printf("culture_gallery --check: %d wall runs (every gate form x wall x 3 cultures, every tower form x wall), %d failure(s)\n", runs, bad);
  return bad ? 1 : 0;
}

// a sheet of 12 random cultures (engine archetype and seed), one street each
Canvas randomSheet() {
  Board b(16 + 7 * 100, 16 + 12 * 110);
  for (int r = 0; r < 12; r++) {
    const uint32_t seed = 50021u * (uint32_t)(r + 3) ^ 0x5EEDu;
    const Archetype a = (Archetype)(seed % (uint32_t)Archetype::COUNT);
    const cult::Culture c = cult::Atlas::make(a, seed, 2);
    b.text(8, 4 + r * 110, nameOf(a));
    int x = 16;
    for (int k = 0; k < 6; k++) {
      const uint32_t s = seed + 31u * (uint32_t)k;
      const Building t = k == 2 ? Building::Inn : (k == 4 ? Building::Temple : (k == 5 ? Building::Shop : Building::House));
      const int wT = t == Building::Inn ? 5 : (t == Building::Temple ? 5 : 4), hT = t == Building::Temple ? 4 : 3;
      x += putBuilding(b, t, wT, hT, styleFor(c, a, 1, k % 3, s), s, x, 16 + r * 110 + 100, t == Building::Inn ? 2 : 1) + 14;
      if (x > b.c.w - 90) break;
    }
  }
  return b.c;
}

}  // namespace

// lab: one building (or a row of one type in eight seeds) big, for close work
//   culture_gallery <dir> lab <archetype 0..11> <type 0..> <wTiles> <hTiles> [seed] [storeys] [urban] [wealth]
int lab(const std::string& dir, int argc, char** argv, int ai) {
  const Archetype a = (Archetype)std::atoi(argv[ai]);
  const Building t = (Building)std::atoi(argv[ai + 1]);
  const int wT = std::atoi(argv[ai + 2]), hT = std::atoi(argv[ai + 3]);
  const uint32_t seed = argc > ai + 4 ? (uint32_t)std::strtoul(argv[ai + 4], nullptr, 10) : 1234u;
  const int storeys = argc > ai + 5 ? std::atoi(argv[ai + 5]) : 0;
  const int urban = argc > ai + 6 ? std::atoi(argv[ai + 6]) : 1, wealth = argc > ai + 7 ? std::atoi(argv[ai + 7]) : 1;
  const cult::Culture c = cult::Atlas::make(a, 1000u + (uint32_t)a * 7919u, 2);
  const int n = 6;
  Board b(24 + n * (wT * 16 + 30), 40 + 2 * (hT * 16 + 150));
  for (int night = 0; night < 2; night++) {
    int x = 16;
    for (int k = 0; k < n; k++) {
      const uint32_t s = seed + 977u * (uint32_t)k;
      const ArchStyle st = styleFor(c, a, urban, wealth, s);
      if (k == 0 && night == 0)
        std::printf("style: roof %d mat %d wall %d win %d door %d fnd %d orn %04x eave %d wallH %d pitch %d accent %08x alt %08x roofTint %08x wallTint %08x\n",
                    (int)st.roof, (int)st.roofMat, (int)st.wall, (int)st.window, (int)st.door, (int)st.foundation, st.ornament, st.eave, st.wallH, st.pitch,
                    st.accentTint, st.altTint, st.roofTint, st.wallTint);
      x += putBuilding(b, t, wT, hT, st, s, x, 30 + (night + 1) * (hT * 16 + 150) - 20, storeys, night == 1) + 30;
    }
  }
  savePng(b.c, dir + "/lab.png", 3);
  return 0;
}

int main(int argc, char** argv) {
  int ai = 1;
  if (argc > ai && !std::strcmp(argv[ai], "--check")) return fortCheck();
  if (argc > ai && !std::strcmp(argv[ai], "--hand")) { g_hand = true; ai++; }
  const std::string dir = argc > ai ? argv[ai] : ".";
  // (M3b forts) culture_gallery <dir> forts [archetype]: forts_<archetype>.png (3x) + _1x, forms.png (every gate and
  // tower form, 1x and 3x)
  if (argc > ai + 1 && !std::strcmp(argv[ai + 1], "forts")) {
    const std::string only = argc > ai + 2 ? argv[ai + 2] : "";
    for (int i = 0; i < (int)Archetype::COUNT; i++) {
      if (!only.empty() && only != std::to_string(i) && only != nameOf((Archetype)i)) continue;
      std::string name = nameOf((Archetype)i);
      for (char& ch : name) if (ch == '-') ch = '_';
      const Canvas c = fortBoard((Archetype)i);
      savePng(c, dir + "/forts_" + name + "_1x.png", 1);
      savePng(c, dir + "/forts_" + name + ".png", 3);
    }
    if (only.empty() || only == "forms") {
      const Canvas f = formsBoard();
      savePng(f, dir + "/forms_1x.png", 1);
      savePng(f, dir + "/forms.png", 3);
    }
    if (only.empty() || only == "props") {
      const Canvas f = streetPropsBoard();
      savePng(f, dir + "/streetprops_1x.png", 1);
      savePng(f, dir + "/streetprops.png", 3);
    }
    return 0;
  }
  if (argc > ai + 5 && !std::strcmp(argv[ai + 1], "lab")) return lab(dir, argc, argv, ai + 2);
  std::string only = argc > ai + 1 ? argv[ai + 1] : "";
  for (int i = 0; i < (int)Archetype::COUNT; i++) {
    const Archetype a = (Archetype)i;
    if (!only.empty() && only != std::to_string(i) && only != nameOf(a)) continue;
    Canvas c = archetypeBoard(a, false);
    std::string name = nameOf(a);
    for (char& ch : name) if (ch == '-') ch = '_';
    savePng(c, dir + "/culture_" + name + "_1x.png", 1);
    savePng(c, dir + "/culture_" + name + ".png", 3);
    if (!only.empty()) savePng(archetypeBoard(a, true), dir + "/culture_" + name + "_night_1x.png", 1);
  }
  if (only.empty()) savePng(randomSheet(), dir + "/sheet.png", 1);
  std::printf("painted in %.1f ms, worst single building %.1f ms\n", g_paintMs, g_worstPaint);
  return 0;
}
