// city_gallery (M7b "Cities & Voice", VISION_PLAN 15.23, 15.24): the CITYART lane's gallery and checks.
//
//   city_gallery --village-art-write [file]   record the village-art fixture (default tests/fixtures/golden_village_art.txt)
//   city_gallery --village-art-check [file]   every line of the fixture must hash as recorded (villages never change)
//   city_gallery <outdir>                     boards: terraces of 3..6 row houses per culture (city_terraces_<culture>.png,
//                                             3x and _1x), the city props per culture (city_props.png)
//   city_gallery --check                      0 failures wanted:
//     1. the village-art fixture (as --village-art-check);
//     2. terraces: no transparent hole at a terrace's joints (the column at every party wall is painted from the
//        foundation up to the lower roof's eave, both sides);
//     3. every prop drawn with its top-left light (the lit side of its body is brighter than the shaded side);
//     4. the stand-ins are gone (every city prop differs from its phase A stand-in canvas size or pixels in every culture).
//
// The village-art fixture hashes, with Request::row 0: every art::Building for every culture (and the classic style) x
// wealth 0..3 x urban 0..3 x two footprints x two seeds, and every prop sprite that existed before M7b (propSprite(p) and
// propSprite(p, culture style), the joined hedge / dyke / fence pieces). One line per group, so a change shows where.
// Kerbs, sidewalks and the rest of the city ground are the view's (rpg/view/terrain.cpp, drawn only where Map::urb != 0):
// they are proved unchanged in villages by screenshots, not here.
#include "tools/preview/preview_util.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "rpg/art/art_city.h"
#include "rpg/build/blueprint.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/world.h"

using art::Building;
using art::Prop;

namespace {

const char* kCult[] = {"fjordfolk", "highland", "heartland", "imperial", "dune", "steppe", "marsh", "jade", "river", "suntemple", "sylvan", "starspire"};
constexpr int kCults = 12;

struct Fnv {
  uint64_t h = 1469598103934665603ull;
  void u32(uint32_t v) { for (int i = 0; i < 4; i++) { h ^= (uint8_t)(v >> (i * 8)); h *= 1099511628211ull; } }
  void canvas(const Canvas& c) { u32((uint32_t)c.w); u32((uint32_t)c.h); for (uint32_t p : c.px) u32(p); }
};

cult::Culture cultureOf(int c) { return cult::Atlas::make((cult::Archetype)c, 4242u + (uint32_t)c * 7u, 2); }

// ---------------------------------------------------------------- the village-art fixture
std::vector<std::pair<std::string, uint64_t>> villageArt() {
  std::vector<std::pair<std::string, uint64_t>> out;
  static const int sizes[2][2] = {{4, 3}, {6, 4}};
  for (int c = -1; c < kCults; c++) {
    const cult::Culture K = cultureOf(c < 0 ? 0 : c);
    for (int u = 0; u <= 3; u++) {
      Fnv f;
      for (int t = 0; t < (int)Building::COUNT; t++)
        for (int w = 0; w <= 3; w++)
          for (int s = 0; s < 2; s++)
            for (int sd = 0; sd < 2; sd++) {
              const uint32_t seed = 500u + (uint32_t)t * 37u + (uint32_t)s * 11u + (uint32_t)sd * 1009u + (uint32_t)w * 3u;
              const art::ArchStyle st = c < 0 ? art::urbanize(art::archForBiome(2, seed), u, seed) : cult::buildingArch(K, 2, u, w, seed);
              bld::Request r = bld::simpleRequest((Building)t, sizes[s][0], sizes[s][1], st, seed);
              r.wealth = (uint8_t)w;
              r.urban = (uint8_t)u;
              r.row = 0;
              f.canvas(art::buildingSprite(bld::design(r)));
            }
      out.push_back({std::string("bldg ") + (c < 0 ? "classic" : kCult[c]) + " urban" + std::to_string(u), f.h});
    }
  }
  {
    Fnv f;
    for (int p = 0; p < (int)Prop::Bollard; p++) f.canvas(art::propSprite((Prop)p));
    out.push_back({"props classic", f.h});
  }
  for (int c = 0; c < kCults; c++) {
    const cult::Culture K = cultureOf(c);
    Fnv f;
    for (int p = 0; p < (int)Prop::Bollard; p++)
      if (art::propStyled((Prop)p)) f.canvas(art::propSprite((Prop)p, K.props));
    for (int m = 0; m < 16; m++) {
      f.canvas(art::dykePiece(m, K.props));
      f.canvas(art::hedgePiece(m, K.props, false));
      f.canvas(art::hedgePiece(m, K.props, true));
      f.canvas(art::fenceJoinPiece(m | ((m & 5) ? 0 : 0x100), K.props));
    }
    out.push_back({std::string("props ") + kCult[c], f.h});
  }
  return out;
}

std::string fixturePath(int argc, char** argv, int i) {
  if (i + 1 < argc && argv[i + 1][0] != '-') return argv[i + 1];
#ifdef EMB_SOURCE_DIR
  return std::string(EMB_SOURCE_DIR) + "/tests/fixtures/golden_village_art.txt";
#else
  return "tests/fixtures/golden_village_art.txt";
#endif
}

int villageWrite(const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) { std::printf("FAIL: cannot write %s\n", path.c_str()); return 1; }
  std::fprintf(f, "# M7b village-art fixture (tools/preview/city_gallery.cpp --village-art-write): villages never change\n");
  const auto v = villageArt();
  for (const auto& e : v) std::fprintf(f, "%s %016llx\n", e.first.c_str(), (unsigned long long)e.second);
  std::fclose(f);
  std::printf("village-art: %zu lines written to %s\n", v.size(), path.c_str());
  return 0;
}

int villageCheck(const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) { std::printf("FAIL: village-art: no fixture %s\n", path.c_str()); return 1; }
  std::map<std::string, std::string> want;
  char line[512];
  while (std::fgets(line, sizeof line, f)) {
    if (line[0] == '#') continue;
    std::string s(line);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    const size_t sp = s.rfind(' ');
    if (sp == std::string::npos) continue;
    want[s.substr(0, sp)] = s.substr(sp + 1);
  }
  std::fclose(f);
  int bad = 0, n = 0;
  for (const auto& e : villageArt()) {
    char hx[32];
    std::snprintf(hx, sizeof hx, "%016llx", (unsigned long long)e.second);
    n++;
    auto it = want.find(e.first);
    if (it == want.end()) { std::printf("FAIL: village-art: %s not in the fixture\n", e.first.c_str()); bad++; }
    else if (it->second != hx) { std::printf("FAIL: village-art: %s %s, fixture %s\n", e.first.c_str(), hx, it->second.c_str()); bad++; }
  }
  if ((int)want.size() != n) { std::printf("FAIL: village-art: fixture has %zu lines, %d computed\n", want.size(), n); bad++; }
  std::printf("village-art: %d groups, %d changed\n", n, bad);
  return bad;
}


// ---------------------------------------------------------------- boards
art::PropStyle styleOf(int c) { return c < 0 ? art::PropStyle{} : cultureOf(c).props; }
const char* cultName(int c) { return c < 0 ? "classic" : kCult[c]; }

// every city prop per culture (a row each), the variants, and a garden wall and a hedge laid out as an L with a T and
// a gateway (the joins as the view picks them), a canopy run of three tiles over a strip of street
void propsBoard(const std::string& dir) {
  const int rowH = 84, W = 760;
  Board B(W, 14 + rowH * (kCults + 1));
  for (int c = -1; c < kCults; c++) {
    const art::PropStyle st = styleOf(c);
    const int y0 = 12 + (c + 1) * rowH;
    B.text(4, y0, cultName(c));
    int x = 4;
    auto put = [&](Prop p, int v, int j) {
      const Canvas s = art::cityPropStyled(p, st, v, j, false);
      B.put(s, x, y0 + rowH - 6 - s.h);
      x += s.w + 4;
    };
    put(Prop::Bollard, 0, 0);
    for (int v = 0; v < 4; v++) put(Prop::Planter, v, 0);
    for (int v = 0; v < 2; v++) put(Prop::FlowerBed, v, 0);
    for (int v = 0; v < 4; v++) put(Prop::FruitTree, v, 0);
    put(Prop::Monument, 0, 0);
    put(Prop::Pavilion, 0, 0);
    // a canopy run over three tiles (W end, middle, E end) and a second row south of it
    {
      const int cx = x, cy = y0 + rowH - 6 - 40;
      for (int r = 0; r < 2; r++)
        for (int k = 0; k < 3; k++) {
          const int j = (k > 0 ? 8 : 0) | (k < 2 ? 2 : 0) | (r == 0 ? 4 : 1);
          const Canvas s = art::cityPropStyled(Prop::Canopy, st, (k + r) % 4, j, false);
          B.put(s, cx + k * 16, cy - 16 + r * 16);
        }
      x += 52;
    }
    // walls: an L with a T, a pier at the run's end, a gateway gap
    auto runAt = [&](Prop p, int ox) {
      // tiles (col, row) of the little layout: a row of 5 east-west, a turn south at col 0, a T south at col 3
      static const int T[][2] = {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {5, 0}, {0, 1}, {3, 1}};
      auto has = [&](int cx2, int cy2) { for (auto& t : T) if (t[0] == cx2 && t[1] == cy2) return true; return false; };
      for (int r = 0; r < 2; r++)
        for (const auto& t : T) {
          if (t[1] != r) continue;
          int m = (has(t[0], t[1] - 1) ? 1 : 0) | (has(t[0] + 1, t[1]) ? 2 : 0) | (has(t[0], t[1] + 1) ? 4 : 0) | (has(t[0] - 1, t[1]) ? 8 : 0);
          if (t[0] == 3 && t[1] == 0) m |= 2 << 4;    // a one-tile gateway east of col 3
          if (t[0] == 5) m |= (8 << 4) | 2 << 8;       // its other jamb; the run ends east of it
          if (t[1] == 1) m |= 4 << 8;
          const Canvas s = art::cityPropStyled(p, st, 0, m, false);
          B.put(s, ox + t[0] * 16, y0 + 18 + t[1] * 16 + 16 - s.h + 16);
        }
    };
    runAt(Prop::GardenWall, x); x += 6 * 16 + 6;
    runAt(Prop::Hedge, x);
  }
  savePng(B.c, dir + "/city_props.png", 3);
  savePng(B.c, dir + "/city_props_1x.png", 1);
  std::printf("city_gallery: %s/city_props.png\n", dir.c_str());
}

// ---------------------------------------------------------------- terraces (synthetic row-house requests)
struct Terrace { std::vector<Canvas> spr; std::vector<int> wt; int H = 3; bool round = false; };   // round: a body that is not a box (yurts, pods): never a row
// a terrace of n houses of culture c on one front row: footprints 2..3 tiles wide, 3 deep; the storeys, purposes and
// seeds vary along the row; every inner joint shares its wall (ROW_E on the west house, ROW_W on the east one)
Terrace makeTerrace(int c, int n, int wealth, uint32_t seed, bool yard) {
  Terrace T;
  const cult::Culture K = cultureOf(c < 0 ? 2 : c);
  for (int i = 0; i < n; i++) {
    const uint32_t s = seed * 977u + (uint32_t)i * 131u + 7u;
    const int w = 2 + (int)((s >> 3) % 2u);
    Building t = Building::House;
    if (i == 1 && n >= 4) t = Building::Shop;
    if (i == n - 2 && n >= 5) t = Building::Inn;
    if (i == 2 && n >= 6) t = Building::Bakery;
    const art::ArchStyle st = cult::buildingArch(K, 2, 2, wealth, s);
    art::BuildingFacts f;
    f.storeys = 1 + (int)((s >> 7) % 3u);
    bld::Request r = bld::simpleRequest(t, w, T.H, st, s, f);
    r.wealth = (uint8_t)wealth;
    r.urban = 2;
    r.row = (uint8_t)((i > 0 ? bld::ROW_W : 0) | (i < n - 1 ? bld::ROW_E : 0) | (yard ? bld::ROW_YARD : 0));
    const bld::Blueprint bp = bld::design(r);
    if (!bp.vols.empty() && bp.vols[0].shape != bld::VolShape::Box) T.round = true;
    T.spr.push_back(art::buildingSprite(bp));
    T.wt.push_back(w);
  }
  return T;
}
// lays the terrace on a board with its front row's bottom edge at (x, base); returns its width in px
int putTerrace(Board& B, const Terrace& T, int x, int base) {
  int cx = x;
  for (size_t i = 0; i < T.spr.size(); i++) {
    const Canvas& s = T.spr[i];
    B.put(s, cx - art::BLDG_PAD_X, base + art::BLDG_PAD_B - s.h);
    cx += T.wt[i] * 16;
  }
  return cx - x;
}
// holes at the joints: in the two columns either side of each shared wall, every pixel from the foot of the facade up
// to the lower of the two houses' tops must be painted (no ground seen through the joint)
int terraceHoles(const Terrace& T, std::string* why) {
  int bad = 0;
  int cx = 0;
  std::vector<int> jx;
  for (size_t i = 0; i + 1 < T.spr.size(); i++) { cx += T.wt[i] * 16; jx.push_back(cx); }
  // a combined canvas of the row
  int W = 0, Hh = 0;
  for (size_t i = 0; i < T.spr.size(); i++) { W += T.wt[i] * 16; Hh = std::max(Hh, T.spr[i].h); }
  Canvas row(W + 2 * art::BLDG_PAD_X, Hh);
  cx = 0;
  for (size_t i = 0; i < T.spr.size(); i++) {
    const Canvas& s = T.spr[i];
    for (int y = 0; y < s.h; y++)
      for (int x = 0; x < s.w; x++) {
        const uint32_t p = s.get(x, y);
        if (p >> 24) row.set(cx + x, Hh - s.h + y, p);
      }
    cx += T.wt[i] * 16;
  }
  const int foot = Hh - art::BLDG_PAD_B - 2;   // a little above the footprint's front edge (the plinth's foot)
  for (int j : jx) {
    const int x0 = art::BLDG_PAD_X + j - 1, x1 = x0 + 1;
    auto topOf = [&](int x) { for (int y = 0; y < Hh; y++) if (row.get(x, y) >> 24) return y; return Hh; };
    const int top = std::max(topOf(x0), topOf(x1)) + 1;
    for (int x : {x0, x1})
      for (int y = top; y <= foot; y++)
        if (!(row.get(x, y) >> 24)) {
          bad++;
          if (why && why->empty()) *why = "hole at joint x " + std::to_string(j) + " row " + std::to_string(y) + (x == x0 ? " (west unit" : " (east unit") + ", " + std::to_string(Hh - y) + " px over the canvas foot)";
          break;
        }
  }
  return bad;
}

void terraceBoards(const std::string& dir) {
  for (int c = 0; c < kCults; c++) {
    const int rows = 4, rowH = 150, W = 900;
    Board B(W, 16 + rows * rowH);
    B.text(4, 4, std::string("TERRACES: ") + kCult[c] + "  (3..6 HOUSES, WEALTH 0..3)");
    for (int r = 0; r < rows; r++) {
      const int base = 16 + (r + 1) * rowH - 12;
      int x = 16;
      for (int n = 3; n <= 6 && x < W - 200; n++) {
        const Terrace T = makeTerrace(c, n, r, 4100u + (uint32_t)c * 37u + (uint32_t)n * 11u + (uint32_t)r, r == 3);
        // the street before it: a strip of pale paving (the board's ground is grass)
        int tw = 0;
        for (int w : T.wt) tw += w * 16;
        for (int yy = base + 1; yy < base + 10; yy++) for (int xx = x - 4; xx < x + tw + 4; xx++) B.c.set(xx, yy, rgba(170, 164, 152));
        x += putTerrace(B, T, x, base) + 24;
      }
    }
    savePng(B.c, dir + "/city_terraces_" + kCult[c] + ".png", 2);
    savePng(B.c, dir + "/city_terraces_" + kCult[c] + "_1x.png", 1);
  }
  std::printf("city_gallery: %s/city_terraces_<culture>.png\n", dir.c_str());
}

// ---------------------------------------------------------------- (M7c ART) the street faces of the M7c blocks
// A row of n houses with its street sides as given (ROW_SIDE_W on the first, ROW_SIDE_E on the last, ROW_BACK on all
// when back), depth h tiles, every house the same storeys (a terrace of one build), the purposes varied.
Terrace makeRow(int c, int n, int wealth, uint32_t seed, int h, bool sideW, bool sideE, bool back, int storeys) {
  Terrace T;
  T.H = h;
  const cult::Culture K = cultureOf(c < 0 ? 2 : c);
  for (int i = 0; i < n; i++) {
    const uint32_t s = seed * 977u + (uint32_t)i * 131u + 7u;
    const int w = 2 + (int)((s >> 3) % 2u);
    Building t = Building::House;
    if (i == 1 && n >= 3) t = Building::Shop;
    const art::ArchStyle st = cult::buildingArch(K, 2, 2, wealth, s);
    art::BuildingFacts f;
    f.storeys = storeys;
    f.variant = (uint8_t)(1 + (i * 3 + seed) % 7);
    bld::Request r = bld::simpleRequest(t, w, h, st, s, f);
    r.wealth = (uint8_t)wealth;
    r.urban = 2;
    uint8_t row = (uint8_t)((i > 0 ? bld::ROW_W : 0) | (i < n - 1 ? bld::ROW_E : 0));
    if (i == 0 && sideW) row |= bld::ROW_SIDE_W;
    if (i == n - 1 && sideE) row |= bld::ROW_SIDE_E;
    if (back) row |= bld::ROW_BACK;
    r.row = row;
    const bld::Blueprint bp = bld::design(r);
    if (!bp.vols.empty() && bp.vols[0].shape != bld::VolShape::Box) T.round = true;
    T.spr.push_back(art::buildingSprite(bp));
    T.wt.push_back(w);
  }
  return T;
}

// city_rows_<culture>.png: (1) a block's south front with corner houses at both ends and the side streets beside it,
// (2) the same for the rich (corner turrets), (3) a deep double-pile row backing onto a street (ROW_BACK: its back
// eave along the street north of it), (4) a north-south street lined by the street sides of six houses stacked one
// behind the other (ROW_SIDE_E: the side street's view of a block), with a single free-standing corner house
void rowBoards(const std::string& dir) {
  const uint32_t pave = rgba(170, 164, 152), walk = rgba(196, 190, 178);
  for (int c = 0; c < kCults; c++) {
    const int W = 980, Hh = 760;
    Board B(W, Hh);
    B.text(4, 4, std::string("M7C STREET FACES: ") + kCult[c] + "  (CORNERS, RICH CORNERS, DOUBLE PILE + BACK ON A STREET, A SIDE STREET)");
    auto street = [&](int x0, int y0, int x1, int y1) {
      for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) B.c.set(x, y, ((x / 16 + y / 16) & 1) ? pave : rgba(164, 158, 146));
    };
    auto walkStrip = [&](int x0, int y0, int x1, int y1) { for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) B.c.set(x, y, walk); };
    int x = 40;
    const int base1 = 200;
    for (int k = 0; k < 2; k++) {   // (1) and (2): fronts with corners
      const Terrace T = makeRow(c, 4, k == 0 ? 1 : 3, 8100u + (uint32_t)c * 37u + (uint32_t)k, 3, true, true, false, 2 + k);
      int tw = 0;
      for (int w : T.wt) tw += w * 16;
      street(x - 36, base1 - 3 * 16 - 40, x - 4, base1 + 24); walkStrip(x - 4, base1 - 3 * 16 - 40, x, base1 + 24);
      street(x + tw + 4, base1 - 3 * 16 - 40, x + tw + 36, base1 + 24); walkStrip(x + tw, base1 - 3 * 16 - 40, x + tw + 4, base1 + 24);
      walkStrip(x - 4, base1, x + tw + 4, base1 + 6); street(x - 36, base1 + 6, x + tw + 36, base1 + 24);
      putTerrace(B, T, x, base1);
      x += tw + 90;
    }
    {   // (3) a deep double pile backing onto a street
      const int base3 = 450;
      const Terrace T = makeRow(c, 5, 2, 8300u + (uint32_t)c * 41u, 5, true, false, true, 3);
      int tw = 0;
      for (int w : T.wt) tw += w * 16;
      const int top = base3 - 5 * 16;
      street(30, top - 40, 40 + tw + 16, top - 6); walkStrip(30, top - 6, 40 + tw + 16, top);
      street(4, top - 40, 36, base3 + 24); walkStrip(36, top, 40, base3 + 24);
      walkStrip(36, base3, 40 + tw + 16, base3 + 6); street(36, base3 + 6, 40 + tw + 16, base3 + 24);
      putTerrace(B, T, 40, base3);
    }
    {   // (4) a north-south street lined by the street sides of a block (six houses stacked), and a lone corner house
      const int sx = 560, sy0 = 250;
      street(sx + 3 * 16 + 4, sy0 - 60, sx + 3 * 16 + 40, Hh - 10);
      walkStrip(sx + 3 * 16, sy0 - 60, sx + 3 * 16 + 4, Hh - 10);
      for (int r = 0; r < 6; r++) {
        const Terrace T = makeRow(c, 1, r % 3, 8500u + (uint32_t)c * 53u + (uint32_t)r * 7u, 3, false, true, false, 2 + (r & 1));
        const int by = sy0 + r * 3 * 16 + 48 - 48;
        // (each house of the column is the east end of an east-west row: its west neighbour off the board)
        putTerrace(B, T, sx + 3 * 16 - T.wt[0] * 16, by + 48);
      }
      const Terrace L = makeRow(c, 1, 2, 8700u + (uint32_t)c * 59u, 3, true, true, false, 3);
      street(sx + 160, sy0 + 40, sx + 160 + 36, sy0 + 140); street(sx + 160 + 36 + L.wt[0] * 16, sy0 + 40, sx + 232 + L.wt[0] * 16, sy0 + 140);
      street(sx + 160, sy0 + 140, sx + 232 + L.wt[0] * 16, sy0 + 160);
      putTerrace(B, L, sx + 196, sy0 + 140);
    }
    savePng(B.c, dir + "/city_rows_" + kCult[c] + ".png", 2);
    savePng(B.c, dir + "/city_rows_" + kCult[c] + "_1x.png", 1);
  }
  std::printf("city_gallery: %s/city_rows_<culture>.png\n", dir.c_str());
}

}  // namespace

namespace {
// ---------------------------------------------------------------- --check
// the light: the mean brightness of a prop's opaque body on its lit side against its shaded side (left / right halves;
// the canopy's cloth: its north half, facing the light, against its south half)
float lumaOf(uint32_t p) { return ((p & 255) * 0.30f + ((p >> 8) & 255) * 0.55f + ((p >> 16) & 255) * 0.15f) / 255.0f; }
bool litTopLeft(const Canvas& c, bool vertical, float& a, float& b) {
  double s0 = 0, s1 = 0;
  int n0 = 0, n1 = 0, x0 = c.w, x1 = -1, y0 = c.h, y1 = -1;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++)
      if ((c.get(x, y) >> 24) == 255) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
  if (x1 < 0) { a = b = 0; return false; }
  const float mx = (x0 + x1) * 0.5f, my = (y0 + y1) * 0.5f;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const uint32_t p = c.get(x, y);
      if ((p >> 24) != 255) continue;
      const bool first = vertical ? (y + 0.5f < my) : (x + 0.5f < mx);
      if (first) { s0 += lumaOf(p); n0++; } else { s1 += lumaOf(p); n1++; }
    }
  a = n0 ? (float)(s0 / n0) : 0; b = n1 ? (float)(s1 / n1) : 0;
  return a >= b - 0.004f;
}

// 5. (M7c ART) the painted street faces of a row house, for every people x wealth x {house, shop} x {2, 3 storeys} on a
// 3x3 plot: the street side's end of the front (the 20 px of the footprint by that side, from the plinth up two storeys
// and the roof's foot) differs from the party-wall unit's by 40 px or more (never a blank party wall seen from a side
// street: windows in the end bay, the quoins, the bracket, the hipped end), and a lone corner unit (both sides on
// streets) is lit from the top left: its west corner's three facade columns at least as bright as its east corner's.
int streetFaceCheck() {
  int units = 0, blank = 0, dark = 0;
  for (int c = -1; c < kCults; c++)
    for (int w = 0; w < 4; w++)
      for (int t = 0; t < 2; t++)
        for (int n = 2; n <= 3; n++) {
          const uint32_t seed = 7000u + (uint32_t)(c + 1) * 131u + (uint32_t)w * 17u + (uint32_t)t * 5u + (uint32_t)n;
          const art::ArchStyle st = c < 0 ? art::urbanize(art::archForBiome(2, seed), 2, seed) : cult::buildingArch(cultureOf(c), 2, 2, w, seed);
          art::BuildingFacts f;
          f.storeys = n;
          f.variant = (uint8_t)(1 + (seed % 7u));
          bld::Request r = bld::simpleRequest(t ? Building::Shop : Building::House, 3, 3, st, seed, f);
          r.wealth = (uint8_t)w;
          r.urban = 2;
          static const uint8_t rows[4] = {bld::ROW_W | bld::ROW_E, bld::ROW_SIDE_W | bld::ROW_E, bld::ROW_W | bld::ROW_SIDE_E, bld::ROW_SIDE_W | bld::ROW_SIDE_E};
          Canvas sp[4];
          bool box = true;
          int wallH = 24, z0 = 0;
          for (int k = 0; k < 4; k++) {
            r.row = rows[k];
            const bld::Blueprint bp = bld::design(r);
            if (bp.vols.empty() || bp.vols[0].shape != bld::VolShape::Box) { box = false; continue; }
            if (k == 3) { wallH = bp.vols[0].wallH; z0 = bp.vols[0].z0; }
            sp[k] = art::buildingSprite(bp);
          }
          if (!box) continue;   // a round house is never a row
          units++;
          auto endDiff = [&](const Canvas& a, const Canvas& b, bool east) {
            if (a.w != b.w || a.h != b.h) return 9999;
            int d = 0;
            const int fx0 = art::BLDG_PAD_X, fx1 = art::BLDG_PAD_X + 48;
            const int y1 = a.h - art::BLDG_PAD_B, y0 = std::max(0, y1 - 48 - 3 * 16);
            for (int y = y0; y < y1; y++)
              for (int x = east ? fx1 - 20 : fx0; x < (east ? fx1 : fx0 + 20); x++)
                if (a.get(x, y) != b.get(x, y)) d++;
            return d;
          };
          // or the end bay already an elevation: glass (the painter's blue-black panes) on the street side's 20 px
          auto glassIn = [&](const Canvas& a, bool east) {
            int g = 0;
            const int fx0 = art::BLDG_PAD_X, fx1 = art::BLDG_PAD_X + 48, y1 = a.h - art::BLDG_PAD_B;
            for (int y = std::max(0, y1 - 48 - 3 * 16); y < y1; y++)
              for (int x = east ? fx1 - 20 : fx0; x < (east ? fx1 : fx0 + 20); x++) {
                const uint32_t p = a.get(x, y);
                const int rr = (int)(p & 255u), gg = (int)((p >> 8) & 255u), bb = (int)((p >> 16) & 255u);
                if ((p >> 24) == 255 && bb > rr + 25 && bb >= gg && bb < 200) g++;
              }
            return g;
          };
          const int dw = std::max(endDiff(sp[1], sp[0], false), glassIn(sp[1], false) >= 8 * n ? 999 : 0);
          const int de = std::max(endDiff(sp[2], sp[0], true), glassIn(sp[2], true) >= 8 * n ? 999 : 0);
          if (dw < 40 || de < 40) {
            if (blank < 8) std::printf("FAIL: street face: %s wealth %d %s %d storeys: a blank street side (west %d px, east %d px changed)\n", cultName(c), w, t ? "shop" : "house", n, dw, de);
            blank++;
          }
          const Canvas& s = sp[3];
          // the upper storey's band of the front (under the eave, over the ground floor's awnings, boards and doors)
          const int foot = s.h - art::BLDG_PAD_B - z0;
          const int y0 = std::max(0, foot - wallH + 5), y1 = std::max(y0 + 1, foot - wallH / 2 - 2);
          double lw = 0, le = 0;
          int nw = 0, ne = 0;
          for (int y = y0; y < y1; y++)
            for (int i = 0; i < 3; i++) {
              const uint32_t pw = s.get(art::BLDG_PAD_X + i, y), pe = s.get(art::BLDG_PAD_X + 47 - i, y);
              if ((pw >> 24) == 255) { lw += lumaOf(pw); nw++; }
              if ((pe >> 24) == 255) { le += lumaOf(pe); ne++; }
            }
          const bool isDark = nw && ne && lw / nw + 0.025 < le / ne;
          if ((isDark || dw < 40 || de < 40) && std::getenv("EMB_FACE_DIR")) {   // the four units, 4x
            Board B(4 * (s.w + 8) + 8, s.h + 16);
            for (int k = 0; k < 4; k++) B.put(sp[k], 8 + k * (s.w + 8), 8);
            savePng(B.c, std::string(std::getenv("EMB_FACE_DIR")) + "/face_" + cultName(c) + "_w" + std::to_string(w) + (t ? "_shop" : "_house") + std::to_string(n) + ".png", 4);
          }
          if (isDark) {
            if (dark < 8) std::printf("FAIL: street face: %s wealth %d %s %d storeys: the corner unit's west corner %.3f darker than its east %.3f\n", cultName(c), w, t ? "shop" : "house", n, lw / nw, le / ne);
            dark++;
          }
        }
  std::printf("street faces: %d row units, %d with a blank street side, %d corner units against the top-left light\n", units, blank, dark);
  return blank + dark;
}

int check(const std::string& fixture, bool quick) {
  int bad = 0;
  // 1. villages never change (--check-quick: the terraces and the props only)
  if (!quick) bad += villageCheck(fixture);
  // 2. terraces: no hole at a joint, for every culture, 3..6 houses, every wealth, with and without a yard
  int terraces = 0, joints = 0, holes = 0;
  for (int c = 0; c < kCults; c++)
    for (int n = 3; n <= 6; n++)
      for (int w = 0; w <= 3; w++)
        for (int yard = 0; yard < 2; yard++) {
          const Terrace T = makeTerrace(c, n, w, 9000u + (uint32_t)c * 101u + (uint32_t)n * 13u + (uint32_t)w * 7u + (uint32_t)yard, yard != 0);
          std::string why;
          if (T.round) continue;   // (a people of round houses builds no rows: the steppe's tents, the elves' pods)
          const int h = terraceHoles(T, &why);
          terraces++; joints += n - 1;
          if (h && std::getenv("EMB_HOLE_DIR") && holes < 6) {   // the offending row, 4x
            Board B(40 + 16 * 3 * n, 160);
            putTerrace(B, T, 20, 140);
            savePng(B.c, std::string(std::getenv("EMB_HOLE_DIR")) + "/hole_" + kCult[c] + "_" + std::to_string(n) + "_" + std::to_string(w) + (yard ? "y" : "") + ".png", 4);
          }
          if (h) { if (holes < 8) std::printf("FAIL: terrace %s %d houses wealth %d%s: %s\n", kCult[c], n, w, yard ? " yard" : "", why.c_str()); holes++; }
        }
  std::printf("terraces: %d rows, %d joints, %d with a hole\n", terraces, joints, holes);
  bad += holes;
  // 3. the light on every city prop in every culture and variant
  int props = 0, dark = 0;
  for (int c = -1; c < kCults; c++) {
    const art::PropStyle st = styleOf(c);
    for (int p = (int)Prop::Bollard; p <= (int)Prop::Canopy; p++) {
      const Prop pp = (Prop)p;
      if (pp == Prop::Hedge) continue;   // (the land's clipped hedge, art::hedgePiece: the culture lane's painter)
      for (int v = 0; v < art::cityPropVariants(pp); v++) {
        const Canvas s = art::cityPropStyled(pp, st, v, art::cityPropJoins(pp) ? (pp == Prop::Canopy ? 10 : 10) : 0, false);
        float a = 0, b = 0;
        props++;
        if (!litTopLeft(s, pp == Prop::Canopy, a, b)) {
          if (dark < 8) std::printf("FAIL: light: prop %d variant %d (%s): lit side %.3f, shaded side %.3f\n", p, v, cultName(c), a, b);
          dark++;
        }
      }
    }
  }
  std::printf("props: %d city prop looks, %d against the top-left light\n", props, dark);
  bad += dark;
  bad += streetFaceCheck();
  std::printf("city_gallery --check: %d failure(s)\n", bad);
  return bad;
}

}  // namespace

int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--check")) return check(fixturePath(argc, argv, i), false) ? 1 : 0;
    if (!std::strcmp(argv[i], "--check-quick")) return check(fixturePath(argc, argv, i), true) ? 1 : 0;
    if (!std::strcmp(argv[i], "--village-art-write")) return villageWrite(fixturePath(argc, argv, i));
    if (!std::strcmp(argv[i], "--village-art-check")) return villageCheck(fixturePath(argc, argv, i)) ? 1 : 0;
  }
  if (argc >= 3 && argv[1][0] != '-' && !std::strcmp(argv[2], "rows")) { rowBoards(argv[1]); return 0; }
  if (argc >= 2 && argv[1][0] != '-') { propsBoard(argv[1]); terraceBoards(argv[1]); rowBoards(argv[1]); return 0; }
  std::printf("usage: city_gallery --village-art-write|--village-art-check [file] | <outdir> | --check\n");
  return 2;
}
