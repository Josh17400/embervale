// Architecture gallery (M0 architecture lane): buildings in every style, the roof library (shape x material), and city
// walls laid out the way the game draws them (wallKeys + wallTile + gateHouse, y-sorted, ground shadows baked), so wall
// joins, towers, gates, roofs and lighting can be judged at 1x and zoomed.
//   arch_gallery <outDir>      writes buildings.png, roofs.png, styles.png, walls.png (3x) and *_1x.png (1x)
#include <algorithm>
#include <chrono>
#include "tools/preview/preview_util.h"

namespace {

uint32_t shadeGround(uint32_t c, int level) {
  if (!level) return c;
  uint32_t s = art::mix(c, rgba(48, 34, 92), level == 2 ? 0.42f : 0.30f);
  return art::shade(s, level == 2 ? 0.70f : 0.80f);
}

void save2(const Canvas& c, const std::string& dir, const std::string& name) {
  savePng(c, dir + "/" + name + ".png", kScale);
  savePng(c, dir + "/" + name + "_1x.png", 1);
}

// ground shadow of a building footprint, as the terrain bakes it
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

void placeBuilding(Board& b, const Canvas& c, int fx, int fy, int wT, int hT, int height) {
  bldgShadow(b, fx, fy, wT * 16, hT * 16, height);
  b.put(c, fx - art::BLDG_PAD_X, fy + hT * 16 + art::BLDG_PAD_B - c.h);
}

// every building type in two sizes, in the plains style and in the style of every main biome
void buildings(const std::string& dir) {
  const int n = (int)art::Building::COUNT;
  static const int biomes[6] = {2, 6, 8, 7, 4, 5};   // plains, snow, desert, swamp, autumn, taiga
  static const char* names[6] = {"PLAINS", "SNOW", "DESERT", "SWAMP", "AUTUMN", "TAIGA"};
  const int colW = 7 * 16 + 40;
  Board b(16 + 6 * colW, 24 + n * 150);
  double ms = 0;
  for (int bi = 0; bi < 6; bi++) b.text(16 + bi * colW, 4, names[bi]);
  for (int i = 0; i < n; i++) {
    int wT = 5, hT = 3;
    art::Building t = (art::Building)i;
    if (t == art::Building::Keep) { wT = 7; hT = 4; }
    if (t == art::Building::Hut) { wT = 3; hT = 2; }
    if (t == art::Building::Tower) { wT = 3; hT = 3; }
    if (t == art::Building::Inn) { wT = 6; hT = 3; }
    for (int bi = 0; bi < 6; bi++) {
      uint32_t seed = 1000u + (uint32_t)i * 31u + (uint32_t)bi * 7u;
      art::ArchStyle st = art::archForBiome(biomes[bi], seed);
      art::BuildingInfo info;
      auto t0 = std::chrono::steady_clock::now();
      Canvas c = art::buildingSprite(t, wT, hT, st, seed, &info);
      ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      int fx = 16 + bi * colW + 8, fy = 24 + i * 150 + 142 - hT * 16;
      placeBuilding(b, c, fx, fy, wT, hT, info.height);
    }
  }
  std::printf("buildings: %d sprites painted in %.1f ms\n", n * 6, ms);
  save2(b.c, dir, "buildings");
}

// the roof library: every shape x material on one house
void roofs(const std::string& dir) {
  const int nS = (int)art::RoofShape::COUNT, nM = (int)art::RoofMat::COUNT;
  const int cw = 4 * 16 + 28, ch = 116;
  Board b(16 + nM * cw, 24 + nS * ch);
  static const char* mats[7] = {"THATCH", "SHINGLE", "SLATE", "CLAY", "TURF", "ADOBE", "COPPER"};
  for (int m = 0; m < nM; m++) b.text(16 + m * cw, 4, mats[m]);
  for (int s = 0; s < nS; s++)
    for (int m = 0; m < nM; m++) {
      art::ArchStyle st;
      st.roof = (art::RoofShape)s;
      st.roofMat = (art::RoofMat)m;
      st.wall = m == 5 ? art::WallMat::Adobe : (s % 3 == 0 ? art::WallMat::Timber : (s % 3 == 1 ? art::WallMat::Stone : art::WallMat::Plaster));
      st.pitch = 2;
      art::BuildingInfo info;
      Canvas c = art::buildingSprite(art::Building::House, 4, 3, st, 77u + (uint32_t)s * 13u + (uint32_t)m, &info);
      placeBuilding(b, c, 16 + m * cw + 8, 24 + s * ch + ch - 8 - 48, 4, 3, info.height);
    }
  save2(b.c, dir, "roofs");
}

// one street per biome: neighbours in one style must not look copy-pasted
void styles(const std::string& dir) {
  static const int biomes[8] = {2, 3, 4, 5, 6, 7, 8, 9};
  static const char* names[8] = {"PLAINS", "FOREST", "AUTUMN", "TAIGA", "SNOW", "SWAMP", "DESERT", "MOUNTAIN"};
  const int rowH = 120;
  Board b(16 + 5 * 92, 16 + 8 * rowH);
  for (int r = 0; r < 8; r++) {
    b.text(8, 6 + r * rowH, names[r]);
    int x = 12;
    for (int k = 0; k < 5; k++) {
      uint32_t seed = 9000u + (uint32_t)r * 101u + (uint32_t)k * 17u;
      int wT = 3 + (int)(seed % 3), hT = 2 + (int)((seed / 3) % 2);
      art::Building t = k == 2 ? art::Building::Inn : (k == 4 ? art::Building::StoneHouse : art::Building::House);
      if (k == 3 && r % 2) t = art::Building::Hut, wT = 3, hT = 2;
      art::ArchStyle st = art::archForBiome(biomes[r], seed);
      art::BuildingInfo info;
      Canvas c = art::buildingSprite(t, wT, hT, st, seed, &info);
      placeBuilding(b, c, x + 8, 16 + r * rowH + rowH - 12 - hT * 16, wT, hT, info.height);
      x += wT * 16 + 18;
    }
  }
  save2(b.c, dir, "styles");
}

// ---------------------------------------------------------------- walls
struct WallGrid {
  int W, H;
  std::vector<uint8_t> wall;
  std::vector<std::pair<int, int>> gates;
  WallGrid(int w, int h) : W(w), H(h), wall((size_t)w * h, 0) {}
  bool at(int x, int y) const { return x >= 0 && y >= 0 && x < W && y < H && wall[(size_t)y * W + x]; }
  void set(int x, int y, int v) { if (x >= 0 && y >= 0 && x < W && y < H) wall[(size_t)y * W + x] = (uint8_t)v; }
};

// draw a wall grid like the game: shadows into the ground, then rows top to bottom: plain walls, towers, gates
void drawWalls(Board& b, const WallGrid& g, int ox, int oy) {
  for (int y = 0; y < g.H * 16; y++)
    for (int x = 0; x < g.W * 16; x++) {
      int lvl = art::wallShadeAt(g.wall.data(), g.W, g.H, x, y);
      if (lvl) b.c.set(ox + x, oy + y, shadeGround(b.c.get(ox + x, oy + y), lvl));
    }
  // passage floors in the shade of the gatehouse
  for (auto& gt : g.gates)
    for (int y = 0; y < 16; y++)
      for (int x = 3; x < 45; x++) b.c.set(ox + gt.first * 16 + x, oy + gt.second * 16 + y, shadeGround(b.c.get(ox + gt.first * 16 + x, oy + gt.second * 16 + y), y < 12 ? 2 : 1));
  std::vector<uint32_t> keys;
  art::wallKeys(g.wall.data(), g.W, g.H, g.gates.data(), (int)g.gates.size(), keys);
  {   // bake cost: every distinct wall tile once, as the view caches them
    std::vector<uint32_t> uniq;
    for (uint32_t k : keys) if (k && std::find(uniq.begin(), uniq.end(), k) == uniq.end()) uniq.push_back(k);
    auto t0 = std::chrono::steady_clock::now();
    for (uint32_t k : uniq) (void)art::wallTile(k);
    std::printf("walls: %zu distinct wall tiles painted in %.1f ms\n", uniq.size(), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
  }
  for (int y = 0; y < g.H; y++) {
    for (int pass = 0; pass < 2; pass++)
      for (int x = 0; x < g.W; x++) {
        uint32_t k = keys[(size_t)y * g.W + x];
        if (!k || ((k & art::WALL_BIT_TOWER) != 0) != (pass == 1)) continue;
        Canvas c = art::wallTile(k);
        b.put(c, ox + x * 16 - art::WALL_OX, oy + y * 16 - art::WALL_OY);
      }
    for (auto& gt : g.gates)
      if (gt.second == y) {
        Canvas c = art::gateHouse(7);
        b.put(c, ox + gt.first * 16 - art::GATE_OX, oy + gt.second * 16 - art::GATE_OY);
      }
  }
}

// the world generator's ring: inside tiles of a wobbly blob with an outside 8-neighbour, a gate on the bottom run
// and a side opening on the left run
WallGrid blobRing(int W, int H, float wobA, float wobB, bool gate, bool side) {
  WallGrid g(W, H);
  std::vector<uint8_t> inside((size_t)W * H, 0);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float dx = (x + 0.5f - W / 2.0f) / (W / 2.0f - 1.5f), dy = (y + 0.5f - H / 2.0f) / (H / 2.0f - 1.5f);
      float a = std::atan2(dy, dx);
      float wob = 1.0f + wobA * std::sin(a * 3 + 0.7f) + wobB * std::cos(a * 5 + 1.3f);
      inside[(size_t)y * W + x] = std::sqrt(dx * dx + dy * dy) < wob * 0.95f;
    }
  auto ins = [&](int x, int y) { return x >= 0 && y >= 0 && x < W && y < H && inside[(size_t)y * W + x]; };
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!ins(x, y)) continue;
      bool edge = false;
      for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if (!ins(x + ox, y + oy)) edge = true;
      if (edge) g.set(x, y, 1);
    }
  if (gate) {   // middle of the longest run in the lower half
    int best = 0, bx = -1, by = -1;
    for (int y = H / 2; y < H; y++)
      for (int x = 0; x < W; x++) {
        int n = 0;
        while (g.at(x + n, y) && !g.at(x + n, y - 1) && !g.at(x + n, y + 1)) n++;
        if (n > best) { best = n; bx = x; by = y; }
      }
    if (best >= 5) {
      int gx = bx + best / 2 - 1;
      for (int k = 0; k < 3; k++) g.set(gx + k, by, 0);
      g.gates.push_back({gx, by});
    }
  }
  if (side) {   // a plain 3-tile opening in the left run (towers at the jambs)
    int best = 0, bx = -1, by = -1;
    for (int x = 0; x < W / 2; x++)
      for (int y = 0; y < H; y++) {
        int n = 0;
        while (g.at(x, y + n) && !g.at(x - 1, y + n) && !g.at(x + 1, y + n)) n++;
        if (n > best) { best = n; bx = x; by = y; }
      }
    if (best >= 5) for (int k = 0; k < 3; k++) g.set(bx, by + best / 2 - 1 + k, 0);
  }
  return g;
}

void walls(const std::string& dir) {
  Board b(16 * 64, 16 * 44);
  // 1. a city ring as the generator makes it, with a gatehouse and a side opening
  WallGrid r1 = blobRing(30, 22, 0.10f, 0.06f, true, true);
  drawWalls(b, r1, 16, 48);
  b.text(16, 8, "CITY RING (GENERATOR) + GATE + SIDE OPENING");
  // 2. a ring with only diagonal links (8-connected), as older layouts can have
  WallGrid r2(16, 12);
  for (int y = 0; y < 12; y++)
    for (int x = 0; x < 16; x++) {
      float dx = (x + 0.5f - 8) / 6.5f, dy = (y + 0.5f - 6) / 4.5f;
      float d = dx * dx + dy * dy;
      bool in = d < 1.0f;
      bool edge = in && ((x > 0 && (((x - 1.5f - 8) / 6.5f) * ((x - 1.5f - 8) / 6.5f) + dy * dy) >= 1.0f) ||
                         (((x + 1.5f - 8) / 6.5f) * ((x + 1.5f - 8) / 6.5f) + dy * dy) >= 1.0f ||
                         (dx * dx + ((y - 1.5f - 6) / 4.5f) * ((y - 1.5f - 6) / 4.5f)) >= 1.0f ||
                         (dx * dx + ((y + 1.5f - 6) / 4.5f) * ((y + 1.5f - 6) / 4.5f)) >= 1.0f);
      if (edge) r2.set(x, y, 1);
    }
  drawWalls(b, r2, 16 * 33, 48);
  b.text(16 * 33, 8, "DIAGONAL LINKS");
  // 3. joins: T, X, ends, a clump, a lone pier, an L and a long straight run
  WallGrid r3(30, 14);
  for (int x = 1; x < 12; x++) r3.set(x, 3, 1);
  for (int y = 3; y < 9; y++) r3.set(6, y, 1);            // T
  for (int x = 14; x < 23; x++) r3.set(x, 6, 1);
  for (int y = 2; y < 11; y++) r3.set(18, y, 1);          // X
  r3.set(2, 8, 1); r3.set(3, 8, 1); r3.set(2, 9, 1); r3.set(3, 9, 1);   // 2x2 clump
  r3.set(9, 10, 1);                                       // lone pier
  for (int x = 24; x < 29; x++) r3.set(x, 2, 1);
  for (int y = 2; y < 9; y++) r3.set(28, y, 1);           // L
  for (int x = 1; x < 16; x++) r3.set(x, 12, 1);          // long run with a gate
  for (int k = 0; k < 3; k++) r3.set(6 + k, 12, 0);
  r3.gates.push_back({6, 12});
  drawWalls(b, r3, 16 * 33, 16 * 16);
  b.text(16 * 33, 16 * 15, "T, X, ENDS, CLUMP, PIER, L, GATE");
  save2(b.c, dir, "walls");
}

}  // namespace

int main(int argc, char** argv) {
  std::string dir = argc >= 2 ? argv[1] : ".";
  SDL_CreateDirectory(dir.c_str());
  std::string only = argc >= 3 ? argv[2] : "";
  if (only.empty() || only == "walls") walls(dir);
  if (only.empty() || only == "buildings") buildings(dir);
  if (only.empty() || only == "roofs") roofs(dir);
  if (only.empty() || only == "styles") styles(dir);
  return 0;
}
