// Overworld generation: continent, biomes, rivers, settlements, roads, dungeons, vegetation.
//
// VERSIONING RULE (saves store the seed and World::genVersion, and regenerate the world from both, so quest givers,
// interiors, looted chests and killed spawns keep pointing at the same things):
//  1. Never change what an existing version generates. Any change to the output (new dens, settlement archetypes,
//     curved roads, different building mix...) goes behind `if (ver >= N)`, where N = WORLDGEN_LATEST + 1, and then
//     WORLDGEN_LATEST becomes N (world.h). Old saves keep their version and their exact world.
//  2. New features draw randomness from their own stream, `Rng r = stream("dens")` (or genSubSeed(seed, "dens")
//     for hashed choices), never from the shared `rng`: one extra rng draw shifts every later feature.
//  3. Indices are identity: a version's site and building order and counts never change. New sites or buildings
//     exist only in the versions that gate them (rule 1); place them late in run() so earlier indices stay put.
//  4. Sub-levels follow the same rule through Site::genVer / Bldg::genVer (genCave, genRuin, genInterior).
//  5. save_test checks that the checked-in v1 save still regenerates the world it was made in.
#include "rpg/sim/world.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <queue>

using art::Prop;

const char* biomeName(Biome b) {
  static const char* n[] = {"OCEAN", "COAST", "PLAINS", "FOREST", "AUTUMN WOODS", "TAIGA", "FROSTLANDS", "MARSH", "DUNES", "MOUNTAINS"};
  return n[(int)b];
}
const char* bldgTypeName(art::Building t) {
  static const char* n[] = {"HOUSE", "HOUSE", "INN", "SMITHY", "GENERAL GOODS", "TEMPLE", "THE KEEP", "MAGE TOWER", "FARMHOUSE", "HUT",
                            "THE PALACE", "BARRACKS"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)art::Building::COUNT, "a name for every building type");
  int i = (int)t;
  return i >= 0 && i < (int)art::Building::COUNT ? n[i] : "HALL";
}
const char* siteTypeName(SiteType t) {
  static const char* n[] = {"CITY", "TOWN", "VILLAGE", "CAVE", "ANCIENT RUIN", "BANDIT CAMP", "SHRINE", "DRAGON LAIR"};
  return n[(int)t];
}

// ------------------------------------------------------------------ names
namespace {
const char* kPre[] = {"RIVER", "WHITE", "FROST", "STONE", "IRON", "WIND", "RAVEN", "OAK", "ELK", "WOLF", "DAWN", "MOSS",
                      "AMBER", "SALT", "HOLLOW", "MIST", "ASH", "SILVER", "THORN", "BRIGHT", "HAWK", "ELDER", "COLD", "RED"};
const char* kSuf[] = {"WOOD", "RUN", "HOLD", "FELL", "MOOR", "STEAD", "HAVEN", "FORD", "VALE", "MERE", "REACH", "WATCH",
                      "BROOK", "GATE", "HELM", "SHORE", "CREST", "DALE", "HEIM", "BURG"};
const char* kSyl1[] = {"BJ", "ER", "UL", "SV", "HR", "TH", "AL", "IN", "GU", "SI", "VI", "KA", "LY", "MA", "DA", "FRI", "YR", "HE", "OL", "AR"};
const char* kSyl2[] = {"OR", "AN", "UN", "IL", "AL", "IR", "EN", "OL", "ID", "UR", "EG", "ON", "ART", "ULF", "GRIM", "VAR", "MUND", "BERT", "STEN", "RIK"};
const char* kFem[] = {"A", "IA", "RA", "DIS", "HILD", "WYN", "LA", "GRID", "UN", "VEIG", "ETTE", "INA"};
const char* kMale[] = {"", "", "OR", "IN", "AR", "ULF", "ER", "ON", "MAR", "LEIF"};
}  // namespace

std::string makeTownName(Rng& r) {
  std::string s = kPre[r.irange(24)];
  s += kSuf[r.irange(20)];
  return s;
}
std::string makePersonName(Rng& r, bool female) {
  std::string s = kSyl1[r.irange(20)];
  s += kSyl2[r.irange(20)];
  s += female ? kFem[r.irange(12)] : kMale[r.irange(10)];
  std::string out;
  out += s[0];
  for (size_t i = 1; i < s.size(); i++) if (!(s[i] == s[i - 1] && (s[i] == 'A' || s[i] == 'I'))) out += s[i];
  return out;
}
std::string makeDungeonName(Rng& r, SiteType t, Biome b) {
  static const char* adj[] = {"BLEAK", "HOWLING", "SUNDERED", "DRAUGR", "BLACK", "FROZEN", "WEEPING", "BROKEN", "SILENT", "BLOOD", "SHADOW", "BONE", "GLOOM", "EMBER", "WRAITH", "HOLLOW"};
  static const char* caveN[] = {"CAVE", "GROTTO", "DEN", "HOLLOW", "CAVERN", "WARREN", "MINE", "DEEP"};
  static const char* ruinN[] = {"BARROW", "CRYPT", "SANCTUM", "TOMB", "VAULT", "HALLS", "SEPULCHER", "SPIRE"};
  static const char* campN[] = {"CAMP", "HIDEOUT", "REDOUBT", "LOOKOUT", "STOCKADE", "OUTPOST"};
  std::string a = adj[r.irange(16)];
  // "FROZEN" belongs in the cold north: elsewhere pick again
  bool cold = b == Biome::Snow || b == Biome::Taiga;
  for (int k = 0; k < 6 && a == "FROZEN" && !cold; k++) a = adj[r.irange(16)];
  if (a == "FROZEN" && !cold) a = "SILENT";
  if (t == SiteType::Cave) return a + " " + caveN[r.irange(8)];
  if (t == SiteType::Ruin) return a + " " + ruinN[r.irange(8)];
  if (t == SiteType::BanditCamp) return a + " " + campN[r.irange(6)];
  if (t == SiteType::Shrine) { static const char* g[] = {"SHRINE OF SOLMIR", "SHRINE OF VEYNA", "SHRINE OF HALDRUN", "SHRINE OF ORISSA", "SHRINE OF KEVRAN", "SHRINE OF ILMATH", "SHRINE OF BRANNOCK", "SHRINE OF ESKARA"}; return g[r.irange(8)]; }
  // the dragon's peak: a name of its own in every world
  static const char* peak[] = {"SKYFANG", "ASHCROWN", "CINDERHORN", "WYRMSPIRE", "STORMTOOTH", "EMBERCREST", "GREYFANG", "DRAKEHOLM"};
  static const char* kind[] = {"PEAK", "SPIRE", "CRAG", "PEAK"};
  return std::string(peak[r.irange(8)]) + " " + kind[r.irange(4)];
}

// ------------------------------------------------------------------ world generation
namespace {
constexpr int WW = 448, WH = 448;

struct Gen {
  World& W;
  Map& M;
  Rng rng;          // the shared WORLDGEN_V1 stream: its draw order is frozen (see the rule above)
  uint32_t s;
  uint64_t seed64;
  int ver;          // generator version being built (World::genVersion)
  std::vector<float> elev, temp, moist;
  std::vector<uint8_t> reserved;   // 1 = settlement/dungeon area (no vegetation/roads cutting through freely)
  std::vector<uint8_t> river;
  Gen(World& w, uint64_t seed) : W(w), M(w.over), rng(seed), s((uint32_t)(seed ^ (seed >> 32))), seed64(seed), ver(w.genVersion) {}
  // an independent stream for a feature added after v1 (rule 2)
  Rng stream(const char* feature) const { return Rng(genSubSeed(seed64, feature)); }

  size_t I(int x, int y) const { return (size_t)y * WW + x; }
  bool land(int x, int y) const { Ground g = M.at(x, y); return !groundSolid(g) && g != Ground::Void; }

  void terrain() {
    M.kind = MapKind::Overworld;
    M.alloc(WW, WH, Ground::Grass);
    M.biome.assign((size_t)WW * WH, 0);
    M.seed = s;
    elev.resize((size_t)WW * WH); temp.resize(elev.size()); moist.resize(elev.size());
    reserved.assign(elev.size(), 0);
    river.assign(elev.size(), 0);
    for (int y = 0; y < WH; y++)
      for (int x = 0; x < WW; x++) {
        float nx = x / (float)WW * 2 - 1, ny = y / (float)WH * 2 - 1;
        float d = std::sqrt(nx * nx * 0.9f + ny * ny);
        float e = fbm(x / 120.0f, y / 120.0f, s, 6);
        float ridge = 1.0f - std::fabs(fbm(x / 70.0f, y / 70.0f, s + 77, 4) * 2 - 1);
        float rmask = smooth01(clampf((fbm(x / 110.0f, y / 110.0f, s + 99, 3) - 0.42f) * 4.0f, 0, 1));
        e = e * 1.0f + 0.22f - d * d * 0.55f + std::pow(ridge, 3.0f) * 0.42f * rmask;
        e -= smooth01(clampf((d - 0.82f) * 4.0f, 0, 1)) * 0.5f;   // ocean ring
        elev[I(x, y)] = e;
        float t = (y / (float)WH) * 0.95f + (fbm(x / 90.0f, y / 90.0f, s + 5, 3) - 0.5f) * 0.4f - std::max(0.0f, e - 0.58f) * 0.9f;
        temp[I(x, y)] = t;
        moist[I(x, y)] = fbm(x / 75.0f, y / 75.0f, s + 11, 4);
      }
    for (int y = 0; y < WH; y++)
      for (int x = 0; x < WW; x++) classify(x, y);
  }

  void classify(int x, int y) {
    size_t i = I(x, y);
    float e = elev[i], t = temp[i], m = moist[i];
    Biome b; Ground g;
    if (e < 0.30f) { b = Biome::Ocean; g = e < 0.25f ? Ground::DeepWater : Ground::Water; }
    else if (e < 0.325f) { b = Biome::Beach; g = t < 0.25f ? Ground::Snow : Ground::Sand; }
    else if (e > 0.72f) { b = Biome::Mountain; g = Ground::Rock; }
    else if (t < 0.19f) { b = Biome::Snow; g = Ground::Snow; }
    else if (t < 0.31f) { b = Biome::Taiga; g = Ground::Tundra; }
    else if (t > 0.80f && m < 0.50f) { b = Biome::Desert; g = Ground::Sand; }
    else if (m > 0.62f && e < 0.47f) {
      b = Biome::Swamp;
      g = vnoise(x / 3.5f, y / 3.5f, s + 31) > 0.72f ? Ground::Water : Ground::Swamp;
    }
    else if (fbm(x / 55.0f, y / 55.0f, s + 41, 3) > 0.62f && m > 0.42f) { b = Biome::Autumn; g = Ground::Autumn; }
    else if (m > 0.52f) { b = Biome::Forest; g = Ground::ForestFloor; }
    else { b = Biome::Plains; g = fbm(x / 14.0f, y / 14.0f, s + 51, 3) > 0.6f ? Ground::Meadow : Ground::Grass; }
    M.biome[i] = (uint8_t)b;
    M.ground[i] = (uint8_t)g;
  }

  void rivers() {
    int made = 0;
    for (int tries = 0; tries < 4000 && made < 14; tries++) {
      int x = 20 + rng.irange(WW - 40), y = 20 + rng.irange(WH - 40);
      float e = elev[I(x, y)];
      if (e < 0.6f || e > 0.71f) continue;
      std::vector<std::pair<int, int>> path;
      std::vector<uint8_t> seen;
      int cx = x, cy = y;
      bool ok = false;
      for (int step = 0; step < 900; step++) {
        path.push_back({cx, cy});
        if (groundWater(M.at(cx, cy)) && !river[I(cx, cy)] && step > 3) { ok = true; break; }
        river[I(cx, cy)] = 2;   // mark as visiting
        int bx = cx, by = cy; float be = 1e9f;
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
          int nx = cx + dx[k], ny = cy + dy[k];
          if (!M.in(nx, ny) || river[I(nx, ny)] == 2) continue;
          float ne = elev[I(nx, ny)] + hashf(nx, ny, s + 3) * 0.012f;
          if (ne < be) { be = ne; bx = nx; by = ny; }
        }
        if (bx == cx && by == cy) break;
        cx = bx; cy = by;
      }
      for (auto& p : path) if (river[I(p.first, p.second)] == 2) river[I(p.first, p.second)] = 0;
      if (!ok || path.size() < 40) continue;
      made++;
      for (size_t k = 0; k < path.size(); k++) {
        int px = path[k].first, py = path[k].second;
        int wdt = k > 60 ? 1 : 0;
        for (int oy = 0; oy <= wdt; oy++)
          for (int ox = 0; ox <= wdt; ox++) {
            int qx = px + ox, qy = py + oy;
            if (!M.in(qx, qy)) continue;
            if (M.at(qx, qy) == Ground::Rock && k < 3) continue;
            if (!groundWater(M.at(qx, qy))) { M.setG(qx, qy, Ground::Water); river[I(qx, qy)] = 1; }
          }
      }
    }
    // sandy/muddy banks
    for (int y = 1; y < WH - 1; y++)
      for (int x = 1; x < WW - 1; x++) {
        if (!river[I(x, y)]) continue;
        for (int oy = -1; oy <= 1; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            Ground g = M.at(x + ox, y + oy);
            if (g == Ground::Grass || g == Ground::Meadow) if (hashf(x + ox, y + oy, s + 9) < 0.35f) M.setG(x + ox, y + oy, Ground::Dirt);
          }
      }
  }

  // fraction of tiles in a rect that are buildable land (not water / rock / swamp)
  float flatness(IRect r) {
    int ok = 0, n = 0;
    for (int y = r.y; y < r.y + r.h; y += 2)
      for (int x = r.x; x < r.x + r.w; x += 2) {
        n++;
        if (!M.in(x, y) || reserved[I(x, y)] || river[I(x, y)]) continue;
        Ground g = M.at(x, y);
        if (!groundSolid(g) && g != Ground::Swamp) ok++;
      }
    return n ? ok / (float)n : 0;
  }

  int addSite(SiteType t, IRect r, int ex, int ey) {
    Site st;
    st.type = t; st.r = r; st.ex = ex; st.ey = ey;
    st.seed = rng.next();
    st.genVer = ver;
    Rng nr(st.seed);
    if (t == SiteType::City || t == SiteType::Town || t == SiteType::Village) {
      for (int tries = 0; tries < 20; tries++) {
        st.name = makeTownName(nr);
        bool dup = st.name == "WHITERUN" || st.name == "RIVERWOOD" || st.name == "WINDHELM";   // too close to a famous game
        for (auto& o : W.sites) if (o.name == st.name) dup = true;
        if (!dup) break;
      }
    } else st.name = makeDungeonName(nr, t, M.biomeAt(ex, ey + (t == SiteType::Cave ? 2 : 0)));
    W.sites.push_back(st);
    for (int y = r.y - 1; y < r.y + r.h + 1; y++)
      for (int x = r.x - 1; x < r.x + r.w + 1; x++)
        if (M.in(x, y)) reserved[I(x, y)] = 1;
    return (int)W.sites.size() - 1;
  }

  bool farFrom(int x, int y, int minD, bool settlementsOnly) {
    for (auto& o : W.sites) {
      bool isSettle = o.type == SiteType::City || o.type == SiteType::Town || o.type == SiteType::Village;
      if (settlementsOnly && !isSettle) continue;
      int dx = o.r.cx() - x, dy = o.r.cy() - y;
      if (dx * dx + dy * dy < minD * minD) return false;
    }
    return true;
  }

  void placeSettlements() {
    struct Want { SiteType t; int n, w, h, minD; };
    Want wants[] = {{SiteType::City, 3, 46, 36, 110}, {SiteType::Town, 6, 32, 24, 64}, {SiteType::Village, 9, 24, 18, 44}};
    for (auto& wnt : wants) {
      int placed = 0;
      for (int tries = 0; tries < 6000 && placed < wnt.n; tries++) {
        int x = 30 + rng.irange(WW - 60 - wnt.w), y = 30 + rng.irange(WH - 60 - wnt.h);
        IRect r{x, y, wnt.w, wnt.h};
        int md = wnt.minD - tries / 300;   // relax over time
        if (!farFrom(r.cx(), r.cy(), std::max(md, 26), false)) continue;
        if (wnt.t == SiteType::City && M.biomeAt(r.cx(), r.cy()) == Biome::Snow && tries < 3000) continue;
        if (flatness(IRect{x - 2, y - 2, wnt.w + 4, wnt.h + 4}) < 0.93f) continue;
        addSite(wnt.t, r, r.cx(), r.cy());
        placed++;
      }
    }
  }

  // ---- settlement stamping
  void clearArea(IRect r, Ground g) {
    for (int y = r.y; y < r.y + r.h; y++)
      for (int x = r.x; x < r.x + r.w; x++) {
        if (!M.in(x, y)) continue;
        Ground cur = M.at(x, y);
        if (groundSolid(cur) || cur == Ground::Swamp) M.setG(x, y, g);
        else if (g != Ground::Grass) M.setG(x, y, g);
        M.setP(x, y, 0);
      }
  }
  Ground baseFor(int x, int y) {
    Biome b = M.biomeAt(x, y);
    if (b == Biome::Snow) return Ground::Snow;
    if (b == Biome::Taiga) return Ground::Tundra;
    if (b == Biome::Desert || b == Biome::Beach) return Ground::Sand;
    return Ground::Grass;
  }
  bool canPlaceBldg(IRect r) {
    for (int y = r.y - 1; y < r.y + r.h + 1; y++)
      for (int x = r.x - 1; x < r.x + r.w + 1; x++) {
        if (!M.in(x, y)) return false;
        if (M.bldgAt[(size_t)y * WW + x] >= 0) return false;
        Ground g = M.at(x, y);
        if (y < r.y + r.h && (g == Ground::Road || g == Ground::Plaza || groundSolid(g) || M.wall[I(x, y)])) return false;
        if (M.propAt(x, y) && y < r.y + r.h) return false;
      }
    return true;
  }
  // WORLDGEN_V7: storeys and hearths are hashed per building index on their own stream (no rng draw moves)
  uint32_t storeyHash(int index) const { return hash32((uint32_t)genSubSeed(seed64, "storeys") ^ (uint32_t)index * 0x9E3779B1u); }
  int storeysFor(art::Building type, int w, int h) const {
    return ver >= WORLDGEN_V7 ? bldgStoreysV7(type, w, h, storeyHash((int)M.bldgs.size())) : art::defaultStoreys(type);
  }
  int riseOf(art::Building t, int storeys) const { return ver >= WORLDGEN_V7 ? bldgRiseTiles(t, storeys) : bldgRiseTiles(t); }
  int putBldg(art::Building type, IRect r, int site, Role owner) {
    Bldg b;
    b.type = type; b.r = r; b.site = site; b.owner = owner;
    b.storeys = (uint8_t)storeysFor(type, r.w, r.h);
    b.hearth = ver >= WORLDGEN_V7 ? bldgHearthV7(type, b.storeys, hash32(storeyHash((int)M.bldgs.size()) + 77u)) : true;
    b.biome = M.biomeAt(r.x + r.w / 2, r.y + r.h / 2);
    b.seed = rng.next();
    b.genVer = ver;
    static const uint32_t roofs[] = {rgba(150, 62, 48), rgba(84, 92, 120), rgba(110, 78, 52), rgba(70, 100, 80), rgba(130, 100, 60), rgba(96, 60, 90)};
    b.roof = (type == art::Building::House || type == art::Building::StoneHouse) ? roofs[rng.irange(6)] : 0;
    M.bldgs.push_back(b);
    int bi = (int)M.bldgs.size() - 1;
    for (int y = r.y; y < r.y + r.h; y++)
      for (int x = r.x; x < r.x + r.w; x++) M.bldgAt[(size_t)y * WW + x] = (int16_t)bi;
    return bi;
  }

  // ---- organic settlements
  // Towns grow the way real ones did: winding streets that follow the approach roads, a market square
  // (or a village green), buildings fronting the streets at uneven setbacks with footpaths to their doors,
  // gardens, trees and fences in between, and fields thinning out at the edge. Cities get a curved wall.
  struct Town {
    int si = 0, cx = 0, cy = 0, x0 = 0, y0 = 0, w = 0, h = 0;
    float rx = 1, ry = 1;
    bool city = false, town = false;
    std::vector<uint8_t> mask;               // 1 main street, 2 lane, 3 square/green, 4 footpath/yard, 5 field
    std::vector<std::pair<int, int>> mainTiles, allStreet;
    std::vector<IRect> keepClear;            // WORLDGEN_V4 cities: the wall openings, kept free of buildings and roofs
    bool in(int x, int y) const { return x >= x0 && y >= y0 && x < x0 + w && y < y0 + h; }
    uint8_t get(int x, int y) const { return in(x, y) ? mask[(size_t)(y - y0) * w + (x - x0)] : 0; }
    void set(int x, int y, uint8_t v) { if (in(x, y)) mask[(size_t)(y - y0) * w + (x - x0)] = v; }
    float dist(int x, int y) const { float dx = (x - cx) / rx, dy = (y - cy) / ry; return std::sqrt(dx * dx + dy * dy); }
  };

  // noisy blob test used for the town footprint, squares and city walls
  float blob(int x, int y, int cx, int cy, float rx, float ry, uint32_t seed) {
    float dx = (x - cx) / rx, dy = (y - cy) / ry;
    float a = std::atan2(dy, dx);
    float wob = (vnoise(std::cos(a) * 1.7f + 5, std::sin(a) * 1.7f + 5, seed) - 0.5f) * 0.32f;
    return std::sqrt(dx * dx + dy * dy) - wob;
  }

  // WORLDGEN_V3 city wall: the whole ring of inside tiles with an outside 8-neighbour (a closed, 4-connected band),
  // then one proper opening where each main street crosses it: a gatehouse (World::gates) on the top and bottom
  // runs, a plain 3-tile opening flanked by the wall's end towers on the side runs (or a breach where the run is
  // stepped). Streets no longer punch stray gaps; lanes that reach the ring end at it. It runs after everything else
  // is generated and makes no random draws, so the rest of a v3 world is exactly the v2 world.
  struct V3Wall { Town T; std::vector<uint8_t> inside; Ground mainG, base; size_t gates0, gates1, gaps0, gaps1; };
  std::vector<V3Wall> v3Walls;
  void rebuildCityWallsV3() {
    // drop the v2 walls, gates and gaps of every city (latest first so the recorded ranges stay valid)
    for (int i = (int)v3Walls.size() - 1; i >= 0; i--) {
      V3Wall& v = v3Walls[(size_t)i];
      W.gates.erase(W.gates.begin() + (std::ptrdiff_t)v.gates0, W.gates.begin() + (std::ptrdiff_t)v.gates1);
      W.wallGaps.erase(W.wallGaps.begin() + (std::ptrdiff_t)v.gaps0, W.wallGaps.begin() + (std::ptrdiff_t)v.gaps1);
      for (int y = v.T.y0 - 1; y <= v.T.y0 + v.T.h; y++)
        for (int x = v.T.x0 - 1; x <= v.T.x0 + v.T.w; x++) if (M.in(x, y)) M.wall[I(x, y)] = 0;
    }
    for (V3Wall& v : v3Walls) cityWallV3(v.T, v.inside, v.mainG, v.base);
    M.rebuildSolid();
  }
  void cityWallV3(Town& T, const std::vector<uint8_t>& inside, Ground mainG, Ground base) {
    auto ins = [&](int x, int y) { return T.in(x, y) && inside[(size_t)(y - T.y0) * T.w + (x - T.x0)]; };
    auto wallAt = [&](int x, int y) { return M.in(x, y) && M.wall[I(x, y)] != 0; };
    auto setWall = [&](int x, int y) {
      if (!M.in(x, y)) return;
      M.wall[I(x, y)] = 1;
      M.setP(x, y, 0);
      // v5: a river keeps flowing under the wall (the view draws a culvert arch there instead of a dam)
      if (ver >= WORLDGEN_V5 && river[I(x, y)] && groundWater(M.at(x, y))) return;
      if (groundWater(M.at(x, y)) || groundSolid(M.at(x, y))) M.setG(x, y, base);
    };
    auto clearWall = [&](int x, int y) {
      if (!M.in(x, y)) return;
      if (M.wall[I(x, y)]) { M.wall[I(x, y)] = 0; M.setG(x, y, mainG); }
      M.setP(x, y, 0);
      if (groundSolid(M.at(x, y))) M.setG(x, y, mainG);
    };
    std::vector<std::pair<int, int>> ring;
    for (int y = T.y0; y < T.y0 + T.h; y++)
      for (int x = T.x0; x < T.x0 + T.w; x++) {
        if (!ins(x, y)) continue;
        bool edge = false;
        for (int oy = -1; oy <= 1 && !edge; oy++) for (int ox = -1; ox <= 1; ox++) if (!ins(x + ox, y + oy)) { edge = true; break; }
        if (!edge) continue;
        setWall(x, y);
        ring.push_back({x, y});
      }
    // main-street crossings, clustered
    std::vector<uint8_t> seen((size_t)T.w * T.h, 0);
    struct Cross { float mx, my; int n; };
    std::vector<Cross> crosses;
    for (auto& rt : ring) {
      int x = rt.first, y = rt.second;
      if (T.get(x, y) != 1 || seen[(size_t)(y - T.y0) * T.w + (x - T.x0)]) continue;
      Cross c{0, 0, 0};
      std::vector<std::pair<int, int>> q{{x, y}};
      seen[(size_t)(y - T.y0) * T.w + (x - T.x0)] = 1;
      for (size_t qi = 0; qi < q.size(); qi++) {
        c.mx += q[qi].first; c.my += q[qi].second; c.n++;
        for (int oy = -1; oy <= 1; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            int nx = q[qi].first + ox, ny = q[qi].second + oy;
            if (!T.in(nx, ny) || !wallAt(nx, ny) || T.get(nx, ny) != 1 || seen[(size_t)(ny - T.y0) * T.w + (nx - T.x0)]) continue;
            seen[(size_t)(ny - T.y0) * T.w + (nx - T.x0)] = 1;
            q.push_back({nx, ny});
          }
      }
      c.mx /= c.n; c.my /= c.n;
      crosses.push_back(c);
    }
    std::vector<IRect> made;
    auto nearMade = [&](int x, int y, int d) {
      for (const IRect& r : made)
        if (x >= r.x - d && x < r.x + r.w + d && y >= r.y - d && y < r.y + r.h + d) return true;
      return false;
    };
    // a straight run of five ring tiles centred on (x, y), with nothing beside its middle three
    auto bldg = [&](int x, int y) { return !M.in(x, y) || M.bldgAt[I(x, y)] >= 0; };
    // v5: no opening on or beside a river (no gatehouse tower standing in the water, no stub of bridge at its foot)
    auto wetNear = [&](int x, int y) {
      if (ver < WORLDGEN_V5) return false;
      for (int oy = -3; oy <= 3; oy++)
        for (int ox = -3; ox <= 3; ox++)
          if (M.in(x + ox, y + oy) && (groundWater(M.at(x + ox, y + oy)) || M.at(x + ox, y + oy) == Ground::Bridge)) return true;
      return false;
    };
    auto straightH = [&](int x, int y) {
      if (wetNear(x, y)) return false;
      for (int k = -2; k <= 2; k++) if (!wallAt(x + k, y)) return false;
      for (int k = -1; k <= 1; k++) if (wallAt(x + k, y - 1) || wallAt(x + k, y + 1) || bldg(x + k, y - 1) || bldg(x + k, y + 1)) return false;
      return true;
    };
    auto straightV = [&](int x, int y) {
      if (wetNear(x, y)) return false;
      for (int k = -2; k <= 2; k++) if (!wallAt(x, y + k)) return false;
      for (int k = -1; k <= 1; k++) if (wallAt(x - 1, y + k) || wallAt(x + 1, y + k) || bldg(x - 1, y + k) || bldg(x + 1, y + k)) return false;
      return true;
    };
    auto gap = [&](IRect g) {
      // the opening and the tiles before and behind it become street, so nothing is built or planted in the way
      for (int y = g.y - 1; y <= g.y + g.h; y++)
        for (int x = g.x - 1; x <= g.x + g.w; x++) {
          bool in = x >= g.x && x < g.x + g.w && y >= g.y && y < g.y + g.h;
          bool approach = (g.h == 1 && x >= g.x && x < g.x + g.w) || (g.w == 1 && y >= g.y && y < g.y + g.h) || (g.w == 3 && g.h == 3);
          if (!in && !approach) continue;
          if (in) clearWall(x, y);
          if (!M.in(x, y) || M.wall[I(x, y)]) continue;
          M.setP(x, y, 0);
          if (groundSolid(M.at(x, y))) M.setG(x, y, mainG);
          // v4: the passage and its approaches are paved, so every opening reads as a gateway and the road runs through
          else if (ver >= WORLDGEN_V4 && M.at(x, y) != Ground::Bridge && M.at(x, y) != Ground::Plaza) M.setG(x, y, mainG);
          if (T.in(x, y) && T.get(x, y) != 3) T.set(x, y, 1);
        }
      W.wallGaps.push_back(g);
      made.push_back(g);
    };
    bool gated = false;
    for (const Cross& c : crosses) {
      int cxr = (int)std::lround(c.mx), cyr = (int)std::lround(c.my);
      if (nearMade(cxr, cyr, 4)) continue;
      float a = std::atan2((c.my - T.cy) / T.ry, (c.mx - T.cx) / T.rx);
      bool horiz = std::fabs(std::sin(a)) > 0.70f;
      // the nearest straight run in the street's direction of travel: a gatehouse there, or a side opening.
      // v5 looks wider and takes either kind (a gatehouse preferred), so a road never ends in a bare breach
      int bx = 0, by = 0, bd = 1 << 30;
      const int R = ver >= WORLDGEN_V5 ? 11 : 6;
      bool bH = horiz;
      for (int oy = -R; oy <= R; oy++)
        for (int ox = -R; ox <= R; ox++)
          for (int kind = 0; kind < (ver >= WORLDGEN_V5 ? 2 : 1); kind++) {
          const bool h = ver >= WORLDGEN_V5 ? kind == 0 : horiz;
          int x = cxr + ox, y = cyr + oy;
          if (ver < WORLDGEN_V5 && (std::abs(ox) > 6 || std::abs(oy) > 6)) continue;
          if (!(h ? straightH(x, y) : straightV(x, y))) continue;
          if (ver >= WORLDGEN_V5 && nearMade(x, y, 4)) continue;
          int d = ox * ox + oy * oy + (ver >= WORLDGEN_V5 && !h ? 30 : 0);
          // prefer an opening with room around it: no house pressed against the gatehouse
          for (int ky = -2; ky <= 2; ky++)
            for (int kx = -2; kx <= 2; kx++)
              if (bldg(x + kx, y + ky)) { d += 40; ky = 3; break; }
          if (d < bd) { bd = d; bx = x; by = y; bH = h; }
        }
      if (ver >= WORLDGEN_V5) horiz = bH;
      if (bd < (1 << 30)) {
        if (horiz) { gap(IRect{bx - 1, by, 3, 1}); W.gates.push_back({bx - 1, by}); gated = true; }
        else gap(IRect{bx, by - 1, 1, 3});
      } else {
        gap(IRect{cxr - 1, cyr - 1, 3, 3});   // a breach through a stepped run, its ends become towers
      }
    }
    if (!gated) {   // every city gets a gatehouse: on the straight top or bottom run nearest a street crossing
      int bx = 0, by = 0, bd = 1 << 30;
      for (int y = T.y0; y < T.y0 + T.h; y++)
        for (int x = T.x0; x < T.x0 + T.w; x++) {
          if (!straightH(x, y) || nearMade(x, y, 2)) continue;
          int d = 1 << 29;
          for (const Cross& c : crosses) d = std::min(d, (int)((x - c.mx) * (x - c.mx) + (y - c.my) * (y - c.my)));
          if (crosses.empty()) d = std::abs(x - T.cx) + (T.y0 + T.h - y);
          if (d < bd) { bd = d; bx = x; by = y; }
        }
      if (bd < (1 << 30)) { gap(IRect{bx - 1, by, 3, 1}); W.gates.push_back({bx - 1, by}); }
    }
    // tidy: drop orphans and spurs that are not jambs of an opening
    for (int pass = 0; pass < 12; pass++) {
      bool changed = false;
      for (int y = T.y0 - 1; y <= T.y0 + T.h; y++)
        for (int x = T.x0 - 1; x <= T.x0 + T.w; x++) {
          if (!wallAt(x, y)) continue;
          int n = 0;
          for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && wallAt(x + ox, y + oy)) n++;
          bool jamb = false;   // beside an opening (4-neighbour), as the tests define it
          for (const IRect& r : made) {
            auto inR = [&](int px, int py) { return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h; };
            if (inR(x - 1, y) || inR(x + 1, y) || inR(x, y - 1) || inR(x, y + 1)) jamb = true;
          }
          if (n < 2 && !jamb) { M.wall[I(x, y)] = 0; changed = true; }
        }
      if (!changed) break;
    }
  }

  // WORLDGEN_V5: every opening joins the streets (a lane from its inner side to the nearest street), and the street
  // stubs left outside the ring (where a street ran on past the wall with no opening) go back to grass or river, so
  // no road runs up to a blank wall and every road out of town leaves through a gate.
  void linkGatesV5(Town& T, const std::vector<uint8_t>& inside, size_t gaps0, Ground laneG, Ground base) {
    auto ins = [&](int x, int y) { return T.in(x, y) && M.in(x, y) && inside[(size_t)(y - T.y0) * T.w + (x - T.x0)]; };
    auto nearGap = [&](int x, int y, int d) {
      for (size_t k = gaps0; k < W.wallGaps.size(); k++) {
        const IRect& g = W.wallGaps[k];
        if (x >= g.x - d && x < g.x + g.w + d && y >= g.y - d && y < g.y + g.h + d) return true;
      }
      return false;
    };
    // street stubs outside the ring, away from every opening
    bool cut = false;
    for (int y = T.y0; y < T.y0 + T.h; y++)
      for (int x = T.x0; x < T.x0 + T.w; x++) {
        uint8_t kk = T.get(x, y);
        if ((kk != 1 && kk != 2) || !M.in(x, y) || ins(x, y) || M.wall[I(x, y)] || nearGap(x, y, 2)) continue;
        T.set(x, y, 0);
        M.setG(x, y, river[I(x, y)] ? Ground::Water : base);
        cut = true;
      }
    // inside, a street that used to run out through the ring (its crossing tiles are wall now, away from any opening)
    // would dead-end against blank masonry, or on a bridge into the river: pull its end back two tiles from the wall
    std::vector<std::pair<int, int>> blocked;
    for (int y = T.y0; y < T.y0 + T.h; y++)
      for (int x = T.x0; x < T.x0 + T.w; x++)
        if ((T.get(x, y) == 1 || T.get(x, y) == 2) && M.in(x, y) && M.wall[I(x, y)] && !nearGap(x, y, 2)) blocked.push_back({x, y});
    for (auto& b : blocked) {
      T.set(b.first, b.second, 0);
      for (int oy = -2; oy <= 2; oy++)
        for (int ox = -2; ox <= 2; ox++) {
          int x = b.first + ox, y = b.second + oy;
          uint8_t kk = T.get(x, y);
          if ((kk != 1 && kk != 2) || !M.in(x, y) || M.wall[I(x, y)] || nearGap(x, y, 2)) continue;
          T.set(x, y, 0);
          M.setG(x, y, river[I(x, y)] ? Ground::Water : base);
          cut = true;
        }
    }
    // and a lane that wanders up to the wall and stops there: trim its dead end back from the masonry
    auto wallNear = [&](int x, int y) {
      for (int oy = -2; oy <= 2; oy++) for (int ox = -2; ox <= 2; ox++) if (M.in(x + ox, y + oy) && M.wall[I(x + ox, y + oy)]) return true;
      return false;
    };
    for (int pass = 0; pass < 5; pass++) {
      std::vector<std::pair<int, int>> ends;
      for (int y = T.y0; y < T.y0 + T.h; y++)
        for (int x = T.x0; x < T.x0 + T.w; x++) {
          uint8_t kk = T.get(x, y);
          if ((kk != 1 && kk != 2) || !M.in(x, y) || M.wall[I(x, y)] || nearGap(x, y, 2) || !wallNear(x, y)) continue;
          int nb = 0;
          static const int ddx[4] = {0, 1, -1, 0}, ddy[4] = {1, 0, 0, -1};
          for (int d = 0; d < 4; d++) { uint8_t o = T.get(x + ddx[d], y + ddy[d]); if (o == 1 || o == 2 || o == 3) nb++; }
          if (nb <= 1) ends.push_back({x, y});
        }
      if (ends.empty()) break;
      for (auto& t : ends) {
        T.set(t.first, t.second, 0);
        M.setG(t.first, t.second, river[I(t.first, t.second)] ? Ground::Water : base);
      }
      cut = true;
    }
    if (cut) {
      auto gone = [&](const std::pair<int, int>& t) { uint8_t kk = T.get(t.first, t.second); return kk != 1 && kk != 2 && kk != 3; };
      T.mainTiles.erase(std::remove_if(T.mainTiles.begin(), T.mainTiles.end(), gone), T.mainTiles.end());
      T.allStreet.erase(std::remove_if(T.allStreet.begin(), T.allStreet.end(), gone), T.allStreet.end());
    }
    // then a lane from each opening's inner side to the nearest street that is left
    for (size_t k = gaps0; k < W.wallGaps.size(); k++) {
      const IRect g = W.wallGaps[k];
      // BFS over the inside of the ring from the tiles just inside the opening to the nearest street not part of it
      std::vector<int> prev((size_t)T.w * T.h, -2);
      std::queue<int> q;
      auto id = [&](int x, int y) { return (y - T.y0) * T.w + (x - T.x0); };
      for (int y = g.y - 1; y <= g.y + g.h; y++)
        for (int x = g.x - 1; x <= g.x + g.w; x++)
          if (ins(x, y) && !M.wall[I(x, y)] && prev[(size_t)id(x, y)] == -2) { prev[(size_t)id(x, y)] = -1; q.push(id(x, y)); }
      int found = -1;
      static const int dx[4] = {0, 1, -1, 0}, dy[4] = {1, 0, 0, -1};
      while (!q.empty() && found < 0) {
        int c = q.front(); q.pop();
        int x = T.x0 + c % T.w, y = T.y0 + c / T.w;
        uint8_t kk = T.get(x, y);
        if ((kk == 1 || kk == 2 || kk == 3) && !nearGap(x, y, 1)) { found = c; break; }
        for (int d = 0; d < 4; d++) {
          int nx = x + dx[d], ny = y + dy[d];
          if (!ins(nx, ny) || prev[(size_t)id(nx, ny)] != -2 || M.wall[I(nx, ny)] || groundSolid(M.at(nx, ny))) continue;
          prev[(size_t)id(nx, ny)] = c;
          q.push(id(nx, ny));
        }
      }
      for (int c = found >= 0 ? prev[(size_t)found] : -1; c >= 0; c = prev[(size_t)c])
        paintStreet(T, T.x0 + c % T.w, T.y0 + c / T.w, 2, laneG);
    }
  }

  void paintStreet(Town& T, int x, int y, uint8_t kind, Ground g) {
    if (!T.in(x, y) || !M.in(x, y)) return;
    uint8_t cur = T.get(x, y);
    if (cur == 3) return;
    if (cur == 0 || cur == 4 || (cur == 2 && kind == 1)) T.set(x, y, kind);
    Ground was = M.at(x, y);
    if (groundWater(was)) M.setG(x, y, Ground::Bridge);
    else if (was != Ground::Plaza && was != Ground::Bridge) M.setG(x, y, g);
    M.setP(x, y, 0);
    T.allStreet.push_back({x, y});
    if (kind == 1) T.mainTiles.push_back({x, y});
  }

  // a street that wanders: heading drifts smoothly; diagonal steps are filled so it stays 4-connected
  void walkStreet(Town& T, float x, float y, float ang, int maxLen, int width, uint8_t kind, Ground g, bool stopAtEdge) {
    float turn = 0;
    int px = (int)std::floor(x), py = (int)std::floor(y);
    for (int i = 0; i < maxLen; i++) {
      turn = turn * 0.82f + rng.range(-0.07f, 0.07f);
      ang += turn;
      x += std::cos(ang); y += std::sin(ang);
      int ix = (int)std::floor(x), iy = (int)std::floor(y);
      if (!T.in(ix, iy)) break;
      if (stopAtEdge && T.dist(ix, iy) > 1.25f) break;
      if (ix != px && iy != py) for (int o = 0; o < width; o++) paintStreet(T, ix + o, py, kind, g);
      for (int oy = 0; oy < width; oy++) for (int ox = 0; ox < width; ox++) paintStreet(T, ix + ox, iy + oy, kind, g);
      // lanes end when they run into an existing street
      if (kind == 2 && i > 3 && T.get(ix + (int)std::round(std::cos(ang) * 2), iy + (int)std::round(std::sin(ang) * 2)) == 1) {
        int ex = ix + (int)std::round(std::cos(ang)), ey = iy + (int)std::round(std::sin(ang));
        paintStreet(T, ex, ey, kind, g);
        break;
      }
      px = ix; py = iy;
    }
  }

  bool footprintFree(Town& T, IRect r, bool city, float wallR) {
    // roofs rise two tiles above the footprint: keep clear of whatever stands just north, and of whatever is
    // just south (its roof would hide this door)
    for (int y = r.y - 3; y <= r.y + r.h + 2; y++)
      for (int x = r.x - 1; x <= r.x + r.w; x++)
        if (M.in(x, y) && M.bldgAt[I(x, y)] >= 0) return false;
    for (int y = r.y - 1; y <= r.y + r.h; y++)
      for (int x = r.x - 1; x <= r.x + r.w; x++) {
        if (!T.in(x, y) || !M.in(x, y)) return false;
        if (M.bldgAt[I(x, y)] >= 0 || M.wall[I(x, y)]) return false;
        bool inside = y >= r.y && y < r.y + r.h && x >= r.x && x < r.x + r.w;
        if (!inside) continue;
        uint8_t k = T.get(x, y);
        if (k != 0) return false;
        Ground g = M.at(x, y);
        if (groundSolid(g) || g == Ground::Bridge || g == Ground::Swamp) return false;
        if (city && blob(x, y, T.cx, T.cy, T.rx, T.ry, M.seed + 900 + T.si) > wallR - 0.07f) return false;
      }
    if (ver >= WORLDGEN_V4 && city) {
      // keep off the ring: a tile beside the footprint and below it, two above it (the roof rises there)
      for (int y = r.y - 2; y <= r.y + r.h; y++)
        for (int x = r.x - 1; x <= r.x + r.w; x++)
          if (M.in(x, y) && M.wall[I(x, y)]) return false;
      // and out of every gate's approach: the opening grown by two tiles across and three or four along the road
      for (const IRect& g : T.keepClear) {
        IRect z{g.x - 2, g.y - 3, g.w + 4, g.h + 7};
        IRect f{r.x - 1, r.y - 3, r.w + 2, r.h + 4};   // footprint + its roof
        if (f.x < z.x + z.w && z.x < f.x + f.w && f.y < z.y + z.h && z.y < f.y + f.h) return false;
      }
    }
    return true;
  }

  // shortest footpath from a door to any street (BFS inside the town area)
  bool footpath(Town& T, int ax, int ay, std::vector<std::pair<int, int>>& path) {
    path.clear();
    if (T.get(ax, ay) == 1 || T.get(ax, ay) == 2 || T.get(ax, ay) == 3) return true;
    std::vector<int> prev((size_t)T.w * T.h, -2);
    std::queue<int> q;
    auto id = [&](int x, int y) { return (y - T.y0) * T.w + (x - T.x0); };
    prev[(size_t)id(ax, ay)] = -1;
    q.push(id(ax, ay));
    static const int dx[4] = {0, 1, -1, 0}, dy[4] = {1, 0, 0, -1};
    while (!q.empty()) {
      int c = q.front(); q.pop();
      int x = T.x0 + c % T.w, y = T.y0 + c / T.w;
      uint8_t k = T.get(x, y);
      if (k == 1 || k == 2 || k == 3) {
        for (int p = prev[(size_t)c]; p >= 0; p = prev[(size_t)p]) path.push_back({T.x0 + p % T.w, T.y0 + p / T.w});
        return path.size() <= 12;
      }
      for (int d = 0; d < 4; d++) {
        int nx = x + dx[d], ny = y + dy[d];
        if (!T.in(nx, ny) || prev[(size_t)id(nx, ny)] != -2) continue;
        if (M.bldgAt[I(nx, ny)] >= 0 || M.wall[I(nx, ny)] || groundSolid(M.at(nx, ny)) || T.get(nx, ny) == 5) continue;
        if (std::abs(nx - ax) + std::abs(ny - ay) > 14) continue;
        prev[(size_t)id(nx, ny)] = c;
        q.push(id(nx, ny));
      }
    }
    return false;
  }

  // WORLDGEN_V5: how many tiles a building's sprite rises above its footprint's top row (roof, steeple, cone),
  // generous on purpose. A generator constant, not measured from the art, so art changes never move buildings.
  // (WORLDGEN_V7: and its storeys, riseOf)
  // WORLDGEN_V5: no sprite covers another building's front (its foundation row and doorstep) or the apron in front
  // of its door, in either direction; and a sprite's sides (eaves, wings) keep a tile from the neighbour.
  bool clearOfNeighboursV5(const Town& T, IRect r, art::Building type, int storeys) {
    auto spriteOf = [&](IRect f, art::Building t, int s) { int up = riseOf(t, s); return IRect{f.x - 1, f.y - up, f.w + 2, f.h + up + 1}; };
    auto frontOf = [&](IRect f, int doorX) {
      IRect a{f.x, f.y + f.h - 1, f.w, 1};            // the foundation row (front wall foot)
      IRect b{doorX - 1, f.y + f.h, 3, 2};              // the doorstep and the apron before it
      return std::make_pair(a, b);
    };
    const IRect sN = spriteOf(r, type, storeys);
    const auto fN = frontOf(r, r.x + r.w / 2);
    for (int i = W.sites[T.si].bldgFirst; i < (int)M.bldgs.size(); i++) {
      const Bldg& E = M.bldgs[i];
      const IRect sE = spriteOf(E.r, E.type, E.storeys);
      const auto fE = frontOf(E.r, E.doorX());
      if (sN.overlaps(fE.first) || sN.overlaps(fE.second) || sE.overlaps(fN.first) || sE.overlaps(fN.second)) return false;
    }
    return true;
  }

  bool placeBuilding(Town& T, art::Building type, Role owner, int bw, int bh, float nearC, bool required, float wallR, bool north) {
    int tries = required ? 900 : 70;
    if (ver >= WORLDGEN_V4 && T.city && !required) tries = 160;   // v4 keeps houses off the ring and the gates: look harder
    if (ver >= WORLDGEN_V5 && !required) tries = T.city ? 320 : 160;   // v5 also keeps every front clear: harder still
    for (int t = 0; t < tries; t++) {
      if (T.allStreet.empty()) return false;
      auto s = T.allStreet[rng.irange((int)T.allStreet.size())];
      if (T.dist(s.first, s.second) > nearC + t / 300.0f) continue;
      if (north && s.second > T.cy - 1 && t < 600) continue;
      // the door's approach tile sits a little way off the street, on either side
      int ax = s.first + rng.irange(5) - 2, ay = s.second + rng.irange(7) - 3;
      if (T.get(ax, ay) == 1 && rng.f() < 0.5f) ay -= 1;
      IRect r{ax - bw / 2, ay - bh, bw, bh};
      if (!footprintFree(T, r, T.city, wallR)) continue;
      if (ver >= WORLDGEN_V5 && !clearOfNeighboursV5(T, r, type, storeysFor(type, bw, bh))) continue;
      if (ver >= WORLDGEN_V5 && T.city) {   // a front door opens onto the street, never onto the city wall a step away
        bool wallInFront = false;
        for (int y = r.y + r.h; y <= r.y + r.h + 2; y++)
          for (int x = r.x - 1; x <= r.x + r.w; x++)
            if (M.in(x, y) && M.wall[I(x, y)]) wallInFront = true;
        if (wallInFront) continue;
      }
      if (groundSolid(M.at(ax, ay)) || M.bldgAt[I(ax, ay)] >= 0) continue;
      float d = T.dist(ax, ay);
      if (!required && rng.f() > 1.25f - d * 0.8f) continue;   // denser in the middle, sparse at the edge
      std::vector<std::pair<int, int>> path;
      if (!footpath(T, ax, ay, path)) continue;
      putBldg(type, r, T.si, owner);
      Ground fp = T.city ? Ground::Road : Ground::Dirt;
      if (T.get(ax, ay) == 0) { T.set(ax, ay, 4); M.setG(ax, ay, Ground::Dirt); M.setP(ax, ay, 0); }
      for (auto& p : path) {
        if (T.get(p.first, p.second) != 0 && T.get(p.first, p.second) != 4) continue;
        T.set(p.first, p.second, 4);
        if (!groundWater(M.at(p.first, p.second))) M.setG(p.first, p.second, T.city ? fp : Ground::Dirt);
        M.setP(p.first, p.second, 0);
      }
      return true;
    }
    return false;
  }

  void stampSettlement(int si) {
    Site& st = W.sites[si];
    Town T;
    T.si = si;
    T.city = st.type == SiteType::City;
    T.town = st.type == SiteType::Town;
    bool village = !T.city && !T.town;
    T.cx = st.r.cx() + rng.irange(3) - 1; T.cy = st.r.cy() + rng.irange(3) - 1;
    T.rx = st.r.w * 0.5f; T.ry = st.r.h * 0.5f;
    T.x0 = st.r.x - 3; T.y0 = st.r.y - 3; T.w = st.r.w + 6; T.h = st.r.h + 6;
    T.mask.assign((size_t)T.w * T.h, 0);
    Ground base = baseFor(T.cx, T.cy);
    uint32_t bseed = M.seed + 700 + si * 13;
    // re-shape the reserved area into an organic blob so the wilderness hugs the town instead of a rectangle
    for (int y = T.y0 - 1; y < T.y0 + T.h + 1; y++)
      for (int x = T.x0 - 1; x < T.x0 + T.w + 1; x++)
        if (M.in(x, y)) reserved[I(x, y)] = 0;
    for (int y = T.y0; y < T.y0 + T.h; y++)
      for (int x = T.x0; x < T.x0 + T.w; x++) {
        if (!M.in(x, y)) continue;
        float b = blob(x, y, T.cx, T.cy, T.rx + 1.5f, T.ry + 1.5f, bseed);
        if (b > 1.0f) continue;
        reserved[I(x, y)] = 1;
        Ground g = M.at(x, y);
        if (g == Ground::Rock || g == Ground::Swamp || (g == Ground::Water && !river[I(x, y)])) M.setG(x, y, base);
        else if (g == Ground::ForestFloor || g == Ground::Autumn || g == Ground::Tundra) { if (b < 0.85f) M.setG(x, y, base == Ground::Grass ? Ground::Grass : base); }
        M.setP(x, y, 0);
      }
    st.bldgFirst = (int)M.bldgs.size();
    std::vector<std::pair<int, int>> removeLater;
    Ground mainG = village ? Ground::Dirt : Ground::Road;
    Ground laneG = T.city ? Ground::Road : Ground::Dirt;
    int layout = village ? rng.irange(3) : 0;   // villages: 0 crossroads hamlet, 1 road village, 2 village green
    // ---- the heart: market square or village green
    float sqR = T.city ? 4.2f : T.town ? 3.4f : 2.6f;
    int gx = T.cx, gy = T.cy;
    for (int y = gy - 6; y <= gy + 6; y++)
      for (int x = gx - 7; x <= gx + 7; x++) {
        if (blob(x, y, gx, gy, sqR * 1.25f, sqR, bseed + 5) > 1.0f) continue;
        T.set(x, y, 3);
        if (village) { if (layout == 2 || layout == 0) M.setG(x, y, M.at(x, y) == Ground::Water ? base : base); else M.setG(x, y, Ground::Dirt); }
        else M.setG(x, y, Ground::Plaza);
        M.setP(x, y, 0);
      }
    // ---- main streets follow the compass toward the neighbours; they wander a little
    float a0 = rng.f() * TAU;
    int widthMain = village ? 1 : 2;
    int nMain = layout == 1 ? 1 : 2;
    for (int k = 0; k < nMain; k++) {
      float a = a0 + k * (PI * 0.5f + rng.range(-0.35f, 0.35f));
      walkStreet(T, (float)gx, (float)gy, a, 80, widthMain, 1, mainG, false);
      walkStreet(T, (float)gx, (float)gy, a + PI + rng.range(-0.3f, 0.3f), 80, widthMain, 1, mainG, false);
    }
    // ---- lanes branch off the main streets
    int nLanes = T.city ? 11 : T.town ? 6 : 2 + rng.irange(2);
    for (int k = 0; k < nLanes && !T.mainTiles.empty(); k++) {
      auto s = T.mainTiles[rng.irange((int)T.mainTiles.size())];
      if (T.dist(s.first, s.second) > 0.8f) continue;
      float a = rng.f() * TAU;
      walkStreet(T, (float)s.first, (float)s.second, a, 6 + rng.irange(T.city ? 12 : 8), 1, 2, laneG, true);
    }
    // ---- city wall: a wobbly ring inside the site rectangle, with gates where streets leave
    float wallR = 0.93f;
    if (T.city) {
      std::vector<uint8_t> inside((size_t)T.w * T.h, 0);
      for (int y = T.y0; y < T.y0 + T.h; y++)
        for (int x = T.x0; x < T.x0 + T.w; x++)
          inside[(size_t)(y - T.y0) * T.w + (x - T.x0)] = blob(x, y, T.cx, T.cy, T.rx, T.ry, M.seed + 900 + si) < wallR;
      auto ins = [&](int x, int y) { return T.in(x, y) && inside[(size_t)(y - T.y0) * T.w + (x - T.x0)]; };
      // WORLDGEN_V4: the final wall (ring, gatehouses, side gates) goes up now, before any building, so the town is
      // laid out around it: nothing is built in a gate's approach or against the ring, and overland roads (routed
      // later) pass through the real openings.
      if (ver >= WORLDGEN_V4) {
        const size_t gaps0 = W.wallGaps.size();
        cityWallV3(T, inside, mainG, base);
        if (ver >= WORLDGEN_V5) linkGatesV5(T, inside, gaps0, laneG, base);
        for (size_t k = gaps0; k < W.wallGaps.size(); k++) T.keepClear.push_back(W.wallGaps[k]);
      }
      // WORLDGEN_V3 keeps this wall while the town is laid out (so every later random draw matches v2) and
      // rebuilds it at the very end of generation (cityWallV3, from run())
      const size_t gates0 = W.gates.size(), gaps0 = W.wallGaps.size();
      if (ver < WORLDGEN_V4) {
      for (int y = T.y0; y < T.y0 + T.h; y++)
        for (int x = T.x0; x < T.x0 + T.w; x++) {
          if (!ins(x, y)) continue;
          bool edge = false;
          for (int oy = -1; oy <= 1 && !edge; oy++) for (int ox = -1; ox <= 1; ox++) if (!ins(x + ox, y + oy)) { edge = true; break; }
          if (!edge) continue;
          if (T.get(x, y) == 1 || T.get(x, y) == 2) continue;
          M.wall[I(x, y)] = 1;
          M.setP(x, y, 0);
          if (groundWater(M.at(x, y))) M.setG(x, y, base);
        }
      // widen every street crossing into a 3-tile gateway
      for (auto& s : T.allStreet) {
        int x = s.first, y = s.second;
        bool nearWall = M.wall[I(x - 1, y)] || M.wall[I(x + 1, y)] || M.wall[I(x, y - 1)] || M.wall[I(x, y + 1)];
        if (!nearWall) continue;
        bool horizRun = M.wall[I(x - 1, y)] || M.wall[I(x + 1, y)] || M.wall[I(x - 2, y)] || M.wall[I(x + 2, y)];
        if (horizRun && (M.wall[I(x - 1, y)] || M.wall[I(x + 1, y)])) {
          for (int k = -1; k <= 1; k++) if (M.wall[I(x + k, y)]) { M.wall[I(x + k, y)] = 0; M.setG(x + k, y, mainG); }
          W.wallGaps.push_back(IRect{x - 1, y, 3, 1});
          bool dup = false;
          for (auto& gt : W.gates) if (std::abs(gt.first - (x - 1)) <= 2 && std::abs(gt.second - y) <= 2) dup = true;
          if (!dup && std::abs(y - T.cy) > T.ry * 0.5f) W.gates.push_back({x - 1, y});
        } else {
          for (int k = -1; k <= 1; k++) if (M.wall[I(x, y + k)]) { M.wall[I(x, y + k)] = 0; M.setG(x, y + k, mainG); }
          W.wallGaps.push_back(IRect{x, y - 1, 1, 3});
        }
      }
      }   // ver < WORLDGEN_V4
      if (ver == WORLDGEN_V3) v3Walls.push_back({T, inside, mainG, base, gates0, W.gates.size(), gaps0, W.wallGaps.size()});
    }
    // ---- buildings: landmarks first, near the heart
    struct Want { art::Building b; Role r; int w, h; float near; bool req; bool north; };
    std::vector<Want> want;
    if (T.city) {
      want = {{art::Building::Keep, Role::Jarl, 9, 4, 0.55f, true, true}, {art::Building::Temple, Role::Priest, 6, 4, 0.5f, true, false},
              {art::Building::Inn, Role::Innkeeper, 6, 3, 0.45f, true, false}, {art::Building::Shop, Role::Merchant, 4, 3, 0.4f, true, false},
              {art::Building::Smithy, Role::Smith, 5, 3, 0.7f, true, false}, {art::Building::Tower, Role::Mage, 3, 3, 0.8f, true, false},
              {art::Building::Shop, Role::Merchant, 4, 3, 0.55f, false, false}};
      for (int i = 0; i < 26; i++) want.push_back({rng.f() < 0.55f ? art::Building::StoneHouse : art::Building::House, Role::Villager, 3 + rng.irange(3), 2 + rng.irange(2), 1.0f, false, false});
    } else if (T.town) {
      want = {{art::Building::Inn, Role::Innkeeper, 6, 3, 0.45f, true, false}, {art::Building::Shop, Role::Merchant, 4, 3, 0.45f, true, false},
              {art::Building::Smithy, Role::Smith, 5, 3, 0.7f, true, false}, {art::Building::Temple, Role::Priest, 6, 4, 0.6f, true, false}};
      for (int i = 0; i < 16; i++) want.push_back({rng.f() < 0.3f ? art::Building::StoneHouse : art::Building::House, Role::Villager, 3 + rng.irange(2), 2 + rng.irange(2), 1.1f, false, false});
      want.push_back({art::Building::Farmhouse, Role::Farmer, 5, 3, 1.2f, false, false});
    } else {
      want = {{art::Building::Inn, Role::Innkeeper, 5 + rng.irange(2), 3, 0.5f, true, false}};
      if (rng.f() < 0.6f) want.push_back({art::Building::Smithy, Role::Smith, 5, 3, 0.8f, false, false});
      want.push_back({art::Building::Farmhouse, Role::Farmer, 5, 3, 1.2f, false, false});
      if (rng.f() < 0.5f) want.push_back({art::Building::Farmhouse, Role::Farmer, 5, 3, 1.3f, false, false});
      for (int i = 0; i < 8; i++) want.push_back({rng.f() < 0.45f ? art::Building::Hut : art::Building::House, Role::Villager, 3 + rng.irange(2), 2 + rng.irange(2), 1.25f, false, false});
    }
    for (auto& wnt : want) {
      int bw = wnt.w, bh = wnt.h;
      if (wnt.b == art::Building::Hut) { bw = 3; bh = 2; }
      if (!placeBuilding(T, wnt.b, wnt.r, bw, bh, wnt.near, wnt.req, wallR, wnt.north) && wnt.req)
        placeBuilding(T, wnt.b, wnt.r, std::max(3, bw - 1), std::max(2, bh - 1), 2.0f, true, wallR, false);
    }
    st.bldgCount = (int)M.bldgs.size() - st.bldgFirst;
    // ---- the heart's centrepiece
    // the heart's centrepiece varies so squares don't all read the same: fountain, statue, well or an old tree
    float cq = hashf(si, 3, M.seed);   // hashed, not rng: keeps every existing seed's layout (and saves) intact
    Prop centre = T.city ? (cq < 0.55f ? Prop::Fountain : Prop::Statue)
                : T.town ? (cq < 0.5f ? Prop::Well : cq < 0.75f ? Prop::Fountain : Prop::OakTree)
                : (village && layout == 2 ? (rng.f() < 0.5f ? Prop::OakTree : Prop::Well) : Prop::Well);   // same draw as before
    if (village && layout == 2 && rng.f() < 0.4f && M.at(gx + 2, gy) != Ground::Bridge) {
      // a duck pond on the green
      for (int y = gy - 1; y <= gy + 1; y++) for (int x = gx; x <= gx + 2; x++) if (T.get(x, y) == 3 && !(x == gx + 2 && y != gy)) M.setG(x, y, Ground::Water);
      M.setProp(gx - 2, gy, Prop::Well);
    } else M.setProp(gx, gy, centre);
    // market stalls and benches around a town square
    if (!village) {
      int stalls = T.city ? 3 : 2, placed = 0;
      for (int t = 0; t < 200 && placed < stalls; t++) {
        int x = gx + rng.irange(11) - 5, y = gy + rng.irange(9) - 4;
        if (T.get(x, y) != 3 || M.propAt(x, y) || std::abs(x - gx) + std::abs(y - gy) < 3) continue;
        if (T.get(x, y + 1) != 3 || T.get(x - 1, y) != 3 || T.get(x + 1, y) != 3) continue;
        bool crowded = false;
        for (int oy = -2; oy <= 2; oy++) for (int ox = -2; ox <= 2; ox++) if (M.propAt(x + ox, y + oy)) crowded = true;
        if (crowded) continue;
        M.setProp(x, y, Prop::MarketStall);
        // roofs rise ~3 tiles above a footprint: a stall under one gets swallowed by the roof sprite
        bool underRoof = false;
        for (int oy = 1; oy <= 3; oy++) for (int ox = -1; ox <= 1; ox++) if (M.bldgAt[I(x + ox, y + oy)] >= 0) underRoof = true;
        // v5: nor against a facade (the stall's awning rises over the row behind it): a free row between them
        if (ver >= WORLDGEN_V5)
          for (int oy = -2; oy <= 0; oy++) for (int ox = -1; ox <= 1; ox++) if (M.bldgAt[I(x + ox, y + oy)] >= 0) underRoof = true;
        if (underRoof) removeLater.push_back({x, y});
        placed++;
      }
      if (T.city && rng.f() < 0.7f) {
        int x = gx + (rng.f() < 0.5f ? -3 : 3), y = gy - 2;
        if (T.get(x, y) == 3 && !M.propAt(x, y)) {
          M.setProp(x, y, Prop::Statue);
          bool clear = true;   // statues jammed against stalls, houses or under a roof come out again at the end
          for (int oy = -1; oy <= 1 && clear; oy++) for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && (M.propAt(x + ox, y + oy) || M.bldgAt[I(x + ox, y + oy)] >= 0)) clear = false;
          for (int oy = 1; oy <= 3 && clear; oy++) if (M.bldgAt[I(x, y + oy)] >= 0) clear = false;
          if (!clear) removeLater.push_back({x, y});
        }
      }
    }
    // ---- lampposts along the main streets (towns and cities)
    if (!village) {
      int step = 0;
      for (auto& s : T.mainTiles) {
        if (++step % 9) continue;
        for (int k = 0; k < 4; k++) {
          static const int dx[4] = {2, -1, 0, 0}, dy[4] = {0, 0, 2, -1};
          int x = s.first + dx[k], y = s.second + dy[k];
          if (T.get(x, y) != 0 || M.bldgAt[I(x, y)] >= 0 || M.wall[I(x, y)] || M.propAt(x, y) || groundSolid(M.at(x, y))) continue;
          bool lampNear = false;
          for (int oy = -5; oy <= 5; oy++) for (int ox = -5; ox <= 5; ox++) if (M.propAt(x + ox, y + oy) == (int)Prop::Lamppost + 1) lampNear = true;
          if (lampNear) break;
          M.setProp(x, y, Prop::Lamppost);
          T.set(x, y, 4);
          break;
        }
      }
    }
    // ---- yards: gardens, fences, barrels, woodpiles, trees
    for (int i = 0; i < st.bldgCount; i++) {
      const Bldg& b = M.bldgs[st.bldgFirst + i];
      auto freeAt = [&](int x, int y) { return T.in(x, y) && T.get(x, y) == 0 && M.bldgAt[I(x, y)] < 0 && !M.wall[I(x, y)] && !M.propAt(x, y) && !groundSolid(M.at(x, y)); };
      int side = rng.f() < 0.5f ? b.r.x - 1 : b.r.x + b.r.w;
      int fy = b.r.y + b.r.h - 1;
      if (b.type == art::Building::Smithy) { if (freeAt(side, fy)) { M.setProp(side, fy, Prop::Anvil); T.set(side, fy, 4); } continue; }
      if (b.type == art::Building::Inn) { if (freeAt(side, fy)) { M.setProp(side, fy, Prop::Woodpile); T.set(side, fy, 4); } if (freeAt(b.doorX() + 2, b.r.y + b.r.h)) M.setProp(b.doorX() + 2, b.r.y + b.r.h, Prop::Barrel); continue; }
      float q = rng.f();
      if (q < 0.35f && !T.city) {
        // a fenced garden beside the house: a short fence run with flowers or crops behind it
        int gx0 = side == b.r.x - 1 ? b.r.x - 3 : b.r.x + b.r.w;
        bool ok = true;
        for (int y = b.r.y; y < b.r.y + b.r.h && ok; y++) for (int x = gx0; x < gx0 + 3 && ok; x++) if (!freeAt(x, y)) ok = false;
        if (ok) {
          for (int y = b.r.y; y < b.r.y + b.r.h; y++)
            for (int x = gx0; x < gx0 + 3; x++) {
              T.set(x, y, 4);
              bool edgeY = y == b.r.y + b.r.h - 1;
              if (edgeY) M.setProp(x, y, Prop::FenceH);
              else if (hashf(x, y, bseed) < 0.6f) M.setProp(x, y, hashf(x, y, bseed + 1) < 0.5f ? Prop::Flowers2 : (village ? Prop::Mushrooms : Prop::Flowers3));
              if (village && !edgeY && hashf(x, y, bseed + 2) < 0.5f) M.setG(x, y, Ground::Farmland);
            }
        }
      } else if (q < 0.6f) {
        if (freeAt(side, fy)) { M.setProp(side, fy, rng.f() < 0.5f ? Prop::Barrel : (rng.f() < 0.5f ? Prop::Crate : Prop::Woodpile)); T.set(side, fy, 4); }
      } else if (q < 0.8f && !T.city) {
        int tx = side + (side < b.r.x ? -1 : 1), ty = b.r.y + rng.irange(b.r.h);
        if (freeAt(tx, ty) && freeAt(tx, ty + 1)) M.setProp(tx, ty, M.biomeAt(tx, ty) == Biome::Taiga || M.biomeAt(tx, ty) == Biome::Snow ? Prop::PineTree : (M.biomeAt(tx, ty) == Biome::Autumn ? Prop::AutumnTree : Prop::OakTree));
      }
    }
    // ---- fields and pasture on the outskirts (not in cities' walls)
    int fields = T.city ? 3 : T.town ? 3 : 3 + rng.irange(3);
    for (int k = 0, t = 0; k < fields && t < 300; t++) {
      int fw = 4 + rng.irange(4), fh = 3 + rng.irange(2);
      float ang = rng.f() * TAU;
      float rr = T.city ? 1.05f : rng.range(0.6f, 1.0f);
      int fx = T.cx + (int)(std::cos(ang) * T.rx * rr) - fw / 2, fy = T.cy + (int)(std::sin(ang) * T.ry * rr) - fh / 2;
      bool ok = true;
      for (int y = fy - 1; y <= fy + fh && ok; y++)
        for (int x = fx - 1; x <= fx + fw && ok; x++) {
          if (!T.in(x, y) || !M.in(x, y) || T.get(x, y) != 0 || M.bldgAt[I(x, y)] >= 0 || M.wall[I(x, y)] || groundSolid(M.at(x, y)) || M.at(x, y) == Ground::Bridge) ok = false;
          else if (T.city && blob(x, y, T.cx, T.cy, T.rx, T.ry, M.seed + 900 + si) < wallR + 0.08f) ok = false;
        }
      if (!ok) continue;
      bool pasture = rng.f() < 0.3f;
      for (int y = fy; y < fy + fh; y++)
        for (int x = fx; x < fx + fw; x++) {
          T.set(x, y, 5);
          reserved[I(x, y)] = 1;
          M.setP(x, y, 0);
          if (!pasture) M.setG(x, y, Ground::Farmland);
          else if (y == fy || y == fy + fh - 1) M.setProp(x, y, Prop::FenceH);
          else if (x == fx || x == fx + fw - 1) M.setProp(x, y, Prop::FenceV);
        }
      if (!pasture) {
        // a fence along the top and one side, gaps left for the farmer
        for (int x = fx - 1; x <= fx + fw; x++) if (hashf(x, fy, bseed + 9) < 0.85f && T.get(x, fy - 1) == 0) M.setProp(x, fy - 1, Prop::FenceH);
        if (T.get(fx + fw, fy + 1) == 0) M.setProp(fx + fw, fy + 1, Prop::Haystack);
        if (rng.f() < 0.5f && T.get(fx - 1, fy + fh - 1) == 0) M.setProp(fx - 1, fy + fh - 1, Prop::Cart);
      } else M.setProp(fx + fw / 2, fy + fh / 2, Prop::Haystack);
      k++;
    }
    // ---- greenery left standing inside the town (old trees, bushes, flowers between houses)
    float treeP = T.city ? 0.012f : T.town ? 0.03f : 0.06f;
    Biome bio = M.biomeAt(T.cx, T.cy);
    for (int y = T.y0; y < T.y0 + T.h; y++)
      for (int x = T.x0; x < T.x0 + T.w; x++) {
        if (!M.in(x, y) || !reserved[I(x, y)] || T.get(x, y) != 0 || M.bldgAt[I(x, y)] >= 0 || M.wall[I(x, y)] || M.propAt(x, y)) continue;
        if (groundSolid(M.at(x, y)) || M.at(x, y) == Ground::Bridge) continue;
        if (M.bldgAt[I(x, y - 1)] >= 0 || M.wall[I(x, y + 1)]) continue;   // keep doors' surroundings and wall feet clear
        bool besideStreet = T.get(x + 1, y) == 1 || T.get(x - 1, y) == 1 || T.get(x, y + 1) == 1 || T.get(x, y - 1) == 1;
        float r = hashf(x, y, bseed + 77);
        if (r < treeP && !besideStreet) {
          Prop tree = bio == Biome::Taiga || bio == Biome::Snow ? Prop::PineTree : bio == Biome::Autumn ? Prop::AutumnTree : bio == Biome::Desert ? Prop::PalmTree
                    : (hashf(x, y, bseed + 78) < 0.3f ? Prop::BirchTree : Prop::OakTree);
          if (bio == Biome::Snow) tree = Prop::SnowPine;
          M.setProp(x, y, tree);
        } else if (r < treeP + 0.03f) M.setProp(x, y, Prop::Bush);
        else if (r < treeP + 0.10f && bio != Biome::Snow && bio != Biome::Desert) M.setProp(x, y, hashf(x, y, bseed + 79) < 0.5f ? Prop::TallGrass : Prop::Flowers1);
      }
    // ---- townsfolk walk the streets and the square; guards patrol
    int slot = 0;
    int folk = T.city ? 16 : T.town ? 10 : 5;
    std::vector<std::pair<int, int>> spots = T.allStreet;
    for (int y = gy - 4; y <= gy + 4; y++) for (int x = gx - 5; x <= gx + 5; x++) if (T.get(x, y) == 3 && !M.propAt(x, y)) spots.push_back({x, y});
    for (int i = 0; i < folk && !spots.empty(); i++) {
      auto p = spots[rng.irange((int)spots.size())];
      Spawn sp; sp.npc = true; sp.site = si; sp.slot = slot++; sp.x = p.first; sp.y = p.second;
      sp.role = rng.f() < 0.15f ? Role::Child : (rng.f() < 0.2f && village ? Role::Farmer : Role::Villager);
      M.spawns.push_back(sp);
    }
    int guards = T.city ? 7 : T.town ? 3 : 0;
    for (int i = 0; i < guards && !T.mainTiles.empty(); i++) {
      auto p = T.mainTiles[rng.irange((int)T.mainTiles.size())];
      Spawn sp; sp.npc = true; sp.site = si; sp.slot = slot++; sp.role = Role::Guard; sp.x = p.first; sp.y = p.second;
      M.spawns.push_back(sp);
    }
    // a signpost where the main street leaves town
    for (auto& s : T.mainTiles)
      if (T.dist(s.first, s.second) > 1.05f && T.dist(s.first, s.second) < 1.2f) {
        int x = s.first + 1, y = s.second + 1;
        if (T.get(x, y) == 0 && !M.propAt(x, y) && M.bldgAt[I(x, y)] < 0 && !M.wall[I(x, y)] && !groundSolid(M.at(x, y))) { M.setProp(x, y, Prop::Signpost); break; }
      }
    // everything the town touched is off-limits to the wilderness pass
    for (int y = T.y0; y < T.y0 + T.h; y++)
      for (int x = T.x0; x < T.x0 + T.w; x++) {
        if (!M.in(x, y)) continue;
        bool used = T.get(x, y) != 0 || M.bldgAt[I(x, y)] >= 0 || M.wall[I(x, y)];
        if (!used) continue;
        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if (M.in(x + ox, y + oy)) reserved[I(x + ox, y + oy)] = 1;
      }
    // props that ended up overlapping are only removed now, after every rng draw: removing them earlier would
    // change later placement decisions and so every seed's layout (and the owner's saves)
    for (auto& rp : removeLater) M.setP(rp.first, rp.second, 0);
    st.ex = gx; st.ey = gy;
  }

  // ---- dungeons and camps
  void placeCaves(int want) {
    int placed = 0;
    for (int tries = 0; tries < 60000 && placed < want; tries++) {
      int x = 6 + rng.irange(WW - 12), y = 6 + rng.irange(WH - 12);
      if (M.at(x, y) != Ground::Rock || M.at(x - 1, y) != Ground::Rock || M.at(x + 1, y) != Ground::Rock || M.at(x, y - 1) != Ground::Rock) continue;
      bool ok = true;
      for (int k = -1; k <= 1; k++) for (int j = 1; j <= 3; j++) if (!land(x + k, y + j) || reserved[I(x + k, y + j)] || M.at(x + k, y + j) == Ground::Swamp) ok = false;
      if (!ok) continue;
      if (!farFrom(x, y, 26 - tries / 6000, false)) continue;
      int si = addSite(SiteType::Cave, IRect{x - 2, y - 1, 5, 4}, x, y);
      Site& st = W.sites[si];
      Biome b = M.biomeAt(x, y + 2);
      static const art::Monster themes[] = {art::Monster::Spider, art::Monster::Troll, art::Monster::Goblin, art::Monster::Bat, art::Monster::Skeleton};
      st.theme = b == Biome::Snow ? (rng.f() < 0.5f ? art::Monster::FrostSpider : art::Monster::IceWolf)
               : b == Biome::Desert ? art::Monster::Sandworm : themes[rng.irange(5)];
      M.setG(x, y, Ground::Dirt);
      M.setProp(x, y, Prop::CaveEntrance);
      for (int k = -1; k <= 1; k++) { M.setP(x + k, y + 1, 0); M.setP(x + k, y + 2, 0); }
      placed++;
    }
    // WORLDGEN_V6: worlds with few mountain edges (about 1 in 10 seeds) ended with as few as 3 caves. When fewer than 8
    // were placed, a top-up pass (its own stream, so the shared stream is untouched) scans every rock edge in a
    // shuffled order with a looser rule (rock above and on one side) and a shorter spacing, up to 10 caves. Worlds that
    // already had 8 or more are unchanged.
    if (ver >= WORLDGEN_V6 && placed < 8) {
      Rng r = stream("caves_v6");
      std::vector<std::pair<int, int>> cand;
      for (int y = 6; y < WH - 6; y++)
        for (int x = 6; x < WW - 6; x++) {
          if (M.at(x, y) != Ground::Rock || M.at(x, y - 1) != Ground::Rock) continue;
          if (M.at(x - 1, y) != Ground::Rock && M.at(x + 1, y) != Ground::Rock) continue;
          bool ok = true;
          for (int k = -1; k <= 1 && ok; k++)
            for (int j = 1; j <= 3; j++)
              if (!land(x + k, y + j) || reserved[I(x + k, y + j)] || M.at(x + k, y + j) == Ground::Swamp) { ok = false; break; }
          if (ok) cand.push_back({x, y});
        }
      for (int i = (int)cand.size() - 1; i > 0; i--) std::swap(cand[(size_t)i], cand[(size_t)r.irange(i + 1)]);
      for (int minD : {14, 10})
        for (const auto& c : cand) {
          if (placed >= 10) break;
          const int x = c.first, y = c.second;
          if (M.at(x, y) != Ground::Rock || M.propAt(x, y) || !farFrom(x, y, minD, false)) continue;
          int si = addSite(SiteType::Cave, IRect{x - 2, y - 1, 5, 4}, x, y);
          Site& st = W.sites[si];
          Biome b = M.biomeAt(x, y + 2);
          static const art::Monster themes[] = {art::Monster::Spider, art::Monster::Troll, art::Monster::Goblin, art::Monster::Bat, art::Monster::Skeleton};
          st.theme = b == Biome::Snow ? (r.f() < 0.5f ? art::Monster::FrostSpider : art::Monster::IceWolf)
                   : b == Biome::Desert ? art::Monster::Sandworm : themes[r.irange(5)];
          M.setG(x, y, Ground::Dirt);
          M.setProp(x, y, Prop::CaveEntrance);
          for (int k = -1; k <= 1; k++) { M.setP(x + k, y + 1, 0); M.setP(x + k, y + 2, 0); }
          placed++;
        }
    }
  }

  bool findOpen(int w, int h, int& ox, int& oy, int minD, std::initializer_list<Biome> avoid = {}) {
    for (int tries = 0; tries < 8000; tries++) {
      int x = 20 + rng.irange(WW - 40 - w), y = 20 + rng.irange(WH - 40 - h);
      IRect r{x, y, w, h};
      if (flatness(IRect{x - 1, y - 1, w + 2, h + 2}) < 0.97f) continue;
      bool bad = false;
      for (Biome b : avoid) if (M.biomeAt(r.cx(), r.cy()) == b) bad = true;
      if (bad) continue;
      if (!farFrom(r.cx(), r.cy(), minD - tries / 400, false)) continue;
      ox = x; oy = y;
      return true;
    }
    return false;
  }

  void placeRuins(int want) {
    for (int i = 0; i < want; i++) {
      int x, y;
      if (!findOpen(9, 7, x, y, 40)) continue;
      int si = addSite(SiteType::Ruin, IRect{x, y, 9, 7}, x + 4, y + 2);
      Site& st = W.sites[si];
      static const art::Monster themes[] = {art::Monster::Draugr, art::Monster::Skeleton, art::Monster::Wraith};
      st.theme = themes[rng.irange(3)];
      for (int yy = y + 1; yy < y + 7; yy++)
        for (int xx = x + 1; xx < x + 8; xx++) { M.setG(xx, yy, Ground::StoneFloor); M.setP(xx, yy, 0); }
      M.setProp(x + 4, y + 2, Prop::IronDoor);
      M.setProp(x + 1, y + 2, Prop::Statue);
      M.setProp(x + 7, y + 2, Prop::Statue);
      M.setProp(x + 2, y + 5, Prop::Brazier);
      M.setProp(x + 6, y + 5, Prop::Brazier);
      M.setProp(x + 1, y + 6, Prop::Rock);
      M.setProp(x + 7, y + 4, Prop::Gravestone);
      // the mouth: walls of rubble around the door
      for (int xx = x + 2; xx <= x + 6; xx++) if (xx != x + 4) M.setProp(xx, y + 1, Prop::Boulder);
    }
  }

  void placeCamps(int want) {
    for (int i = 0; i < want; i++) {
      int x, y;
      if (!findOpen(11, 9, x, y, 36, {Biome::Snow, Biome::Desert})) continue;
      int si = addSite(SiteType::BanditCamp, IRect{x, y, 11, 9}, x + 5, y + 4);
      for (int yy = y + 1; yy < y + 8; yy++)
        for (int xx = x + 1; xx < x + 10; xx++) {
          M.setP(xx, yy, 0);
          float d = std::hypot(xx - (x + 5.f), (yy - (y + 4.f)) * 1.3f);
          if (d < 3.8f) M.setG(xx, yy, Ground::Dirt);
        }
      M.setProp(x + 5, y + 4, Prop::Campfire);
      M.setProp(x + 2, y + 2, Prop::Tent);
      M.setProp(x + 8, y + 2, Prop::Tent);
      M.setProp(x + 2, y + 6, Prop::Crate);
      M.setProp(x + 3, y + 7, Prop::Barrel);
      M.setProp(x + 8, y + 6, Prop::Chest);
      M.setProp(x + 9, y + 7, Prop::Woodpile);
      M.setProp(x + 1, y + 4, Prop::Torch);
      M.setProp(x + 9, y + 4, Prop::Torch);
      int lvl = 0;
      (void)lvl;
      for (int k = 0; k < 4 + rng.irange(3); k++) {
        Spawn sp; sp.bandit = true; sp.site = si; sp.slot = k;
        sp.x = x + 3 + rng.irange(5); sp.y = y + 3 + rng.irange(3);
        M.spawns.push_back(sp);
      }
      Spawn chief; chief.bandit = true; chief.boss = true; chief.site = si; chief.slot = 9; chief.x = x + 6; chief.y = y + 3;
      M.spawns.push_back(chief);
    }
  }

  void placeShrines(int want) {
    for (int i = 0; i < want; i++) {
      int x, y;
      if (!findOpen(5, 5, x, y, 30)) continue;
      int si = addSite(SiteType::Shrine, IRect{x, y, 5, 5}, x + 2, y + 2);
      (void)si;
      for (int yy = y; yy < y + 5; yy++) for (int xx = x; xx < x + 5; xx++) { M.setP(xx, yy, 0); }
      for (int yy = y + 1; yy < y + 4; yy++) for (int xx = x + 1; xx < x + 4; xx++) M.setG(xx, yy, Ground::Plaza);
      M.setProp(x + 2, y + 1, Prop::Shrine);
      M.setProp(x + 1, y + 3, Prop::Flowers2);
      M.setProp(x + 3, y + 3, Prop::Flowers3);
    }
  }

  void placeLair() {
    // the largest open chunk of mountain far from the start
    int best = -1, bx = 0, by = 0;
    const Site& home = W.sites[W.startSite];
    for (int tries = 0; tries < 30000; tries++) {
      int x = 15 + rng.irange(WW - 30), y = 15 + rng.irange(WH - 30);
      if (M.at(x, y) != Ground::Rock) continue;
      int rockN = 0;
      for (int oy = -8; oy <= 8; oy += 2) for (int ox = -8; ox <= 8; ox += 2) if (M.at(x + ox, y + oy) == Ground::Rock) rockN++;
      int dx = x - home.r.cx(), dy = y - home.r.cy();
      int score = rockN * 40 + (int)std::sqrt((float)(dx * dx + dy * dy));
      if (score > best) { best = score; bx = x; by = y; }
    }
    if (best < 0) { bx = WW / 2; by = 40; }
    int si = addSite(SiteType::DragonLair, IRect{bx - 8, by - 7, 17, 15}, bx, by);
    W.lair = si;
    for (int y = by - 7; y <= by + 7; y++)
      for (int x = bx - 8; x <= bx + 8; x++) {
        float d = std::hypot((float)(x - bx), (y - by) * 1.15f);
        if (d < 7.6f + vnoise(x * 0.5f, y * 0.5f, s + 61) * 1.5f) {
          M.setG(x, y, d < 3.2f ? Ground::StoneFloor : Ground::Snow);
          M.setP(x, y, 0);
          M.biome[I(x, y)] = (uint8_t)Biome::Mountain;
        }
      }
    M.setProp(bx - 3, by - 2, Prop::SkullPile);
    M.setProp(bx + 4, by + 1, Prop::Bones);
    M.setProp(bx - 5, by + 3, Prop::Bones);
    M.setProp(bx + 2, by - 4, Prop::Brazier);
    M.setProp(bx - 2, by - 4, Prop::Brazier);
    M.setProp(bx, by - 5, Prop::Altar);
  }

  // ---- roads (A* over a cost grid)
  bool road(int ax, int ay, int bx, int by, bool allowRock) {
    const int N = WW * WH;
    static std::vector<float> g;
    static std::vector<int> from;
    static std::vector<uint8_t> closed;
    g.assign(N, 1e30f); from.assign(N, -1); closed.assign(N, 0);
    auto cost = [&](int x, int y) -> float {
      Ground gr = M.at(x, y);
      size_t i = I(x, y);
      if (M.wall[i] || M.bldgAt[i] >= 0) return -1;
      int p = M.prop[i];
      if (p && ((Prop)(p - 1) == Prop::CaveEntrance || (Prop)(p - 1) == Prop::Well || (Prop)(p - 1) == Prop::Fountain ||
                (Prop)(p - 1) == Prop::Tent || (Prop)(p - 1) == Prop::Campfire || (Prop)(p - 1) == Prop::Shrine || (Prop)(p - 1) == Prop::IronDoor ||
                (Prop)(p - 1) == Prop::Statue || (Prop)(p - 1) == Prop::MarketStall || (Prop)(p - 1) == Prop::Lamppost)) return -1;
      if (gr == Ground::Road || gr == Ground::Bridge || gr == Ground::Plaza) return 0.35f;
      if (gr == Ground::DeepWater) return -1;
      if (gr == Ground::Water) return river[i] ? 9.0f : 40.0f;
      if (gr == Ground::Rock) return allowRock ? 5.0f : -1;
      if (gr == Ground::Farmland) return 8;
      if (reserved[i] && gr != Ground::Dirt) return 6;
      if (gr == Ground::Swamp) return 3.5f;
      if (gr == Ground::ForestFloor || gr == Ground::Autumn) return 1.6f;
      return 1.0f;
    };
    using QN = std::pair<float, int>;
    std::priority_queue<QN, std::vector<QN>, std::greater<QN>> open;
    int start = ay * WW + ax, goal = by * WW + bx;
    g[start] = 0;
    open.push({0, start});
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    bool found = false;
    while (!open.empty()) {
      int cur = open.top().second;
      open.pop();
      if (closed[cur]) continue;
      closed[cur] = 1;
      if (cur == goal) { found = true; break; }
      int cx = cur % WW, cy = cur / WW;
      for (int k = 0; k < 4; k++) {
        int nx = cx + dx[k], ny = cy + dy[k];
        if (nx < 1 || ny < 1 || nx >= WW - 1 || ny >= WH - 1) continue;
        int ni = ny * WW + nx;
        if (closed[ni]) continue;
        float c = cost(nx, ny);
        if (c < 0 && ni != goal) continue;
        if (c < 0) c = 1;
        // prefer straight segments a little
        float turn = (from[cur] >= 0 && (from[cur] % WW != nx && from[cur] / WW != ny)) ? 0.25f : 0;
        float ng = g[cur] + c + turn;
        if (ng < g[ni]) {
          g[ni] = ng; from[ni] = cur;
          float h = (float)(std::abs(nx - bx) + std::abs(ny - by)) * 0.35f;
          open.push({ng + h, ni});
        }
      }
    }
    if (!found) return false;
    for (int cur = goal; cur >= 0; cur = from[cur]) {
      int x = cur % WW, y = cur / WW;
      Ground gr = M.at(x, y);
      if (reserved[I(x, y)] && W.siteAt(x, y, 3) >= 0 && (W.sites[W.siteAt(x, y, 3)].type <= SiteType::Village)) {
        // inside a settlement: join its streets; cut a modest footpath only where needed
        if (gr != Ground::Road && gr != Ground::Plaza && gr != Ground::Bridge && gr != Ground::Dirt && !groundSolid(gr) && gr != Ground::Farmland && !M.prop[I(x, y)]) M.setG(x, y, Ground::Dirt);
        if (gr == Ground::Water) M.setG(x, y, Ground::Bridge);
        // a pass out of a town hemmed in by cliffs or hedges must actually be walkable
        if (allowRock && gr == Ground::Rock) M.setG(x, y, Ground::Dirt);
        if (allowRock && M.prop[I(x, y)] && propSolid((Prop)(M.prop[I(x, y)] - 1))) M.setP(x, y, 0);
        continue;
      }
      if (gr == Ground::Water) M.setG(x, y, Ground::Bridge);
      else if (gr == Ground::Rock) M.setG(x, y, Ground::Dirt);
      else if (gr != Ground::Plaza && gr != Ground::Bridge && gr != Ground::StoneFloor) M.setG(x, y, gr == Ground::Dirt && reserved[I(x, y)] ? Ground::Dirt : Ground::Road);
      if (M.prop[I(x, y)]) {
        Prop p = (Prop)(M.prop[I(x, y)] - 1);
        if (p != Prop::CaveEntrance && p != Prop::IronDoor && p != Prop::Signpost) M.setP(x, y, 0);
      }
    }
    return true;
  }

  void roads() {
    std::vector<int> settle;
    for (int i = 0; i < (int)W.sites.size(); i++) {
      SiteType t = W.sites[i].type;
      if (t == SiteType::City || t == SiteType::Town || t == SiteType::Village) settle.push_back(i);
    }
    // MST (Prim) + a few extra short links
    std::vector<int> inTree{settle.empty() ? 0 : settle[0]};
    std::vector<std::pair<int, int>> edges;
    std::vector<uint8_t> used(W.sites.size(), 0);
    if (!settle.empty()) used[settle[0]] = 1;
    while (inTree.size() < settle.size()) {
      int ba = -1, bb = -1; float bd = 1e30f;
      for (int a : inTree)
        for (int b : settle) {
          if (used[b]) continue;
          float d = std::hypot((float)(W.sites[a].r.cx() - W.sites[b].r.cx()), (float)(W.sites[a].r.cy() - W.sites[b].r.cy()));
          if (d < bd) { bd = d; ba = a; bb = b; }
        }
      if (bb < 0) break;
      used[bb] = 1; inTree.push_back(bb); edges.push_back({ba, bb});
    }
    for (int a : settle) {
      int best = -1; float bd = 1e30f;
      for (int b : settle) {
        if (a == b) continue;
        bool have = false;
        for (auto& e : edges) if ((e.first == a && e.second == b) || (e.first == b && e.second == a)) have = true;
        if (have) continue;
        float d = std::hypot((float)(W.sites[a].r.cx() - W.sites[b].r.cx()), (float)(W.sites[a].r.cy() - W.sites[b].r.cy()));
        if (d < bd) { bd = d; best = b; }
      }
      if (best >= 0 && bd < 120 && rng.f() < 0.5f) edges.push_back({a, best});
    }
    for (auto& e : edges) {
      const Site& A = W.sites[e.first]; const Site& B = W.sites[e.second];
      road(A.ex, A.ey, B.ex, B.ey, false);
    }
    // dirt tracks to dungeons that are close to a road network
    for (int i = 0; i < (int)W.sites.size(); i++) {
      const Site& d = W.sites[i];
      if (d.type != SiteType::Ruin && d.type != SiteType::BanditCamp && d.type != SiteType::Shrine && d.type != SiteType::DragonLair) continue;
      int n = W.nearestSite(d.ex, d.ey, SiteType::Town);
      int v = W.nearestSite(d.ex, d.ey, SiteType::Village);
      int c = W.nearestSite(d.ex, d.ey, SiteType::City);
      int pick = n;
      auto dist = [&](int k) { return k < 0 ? 1e9f : std::hypot((float)(W.sites[k].r.cx() - d.ex), (float)(W.sites[k].r.cy() - d.ey)); };
      if (dist(v) < dist(pick)) pick = v;
      if (dist(c) < dist(pick)) pick = c;
      if (pick < 0) continue;
      if (d.type == SiteType::DragonLair || dist(pick) < 70) {
        int sx = d.ex, sy = d.ey + (d.type == SiteType::Ruin ? 3 : 0);
        if (d.type == SiteType::DragonLair) sy = d.ey + 3;
        road(sx, sy, W.sites[pick].ex, W.sites[pick].ey, d.type == SiteType::DragonLair);
      }
    }
    // tracks to caves (short)
    for (int i = 0; i < (int)W.sites.size(); i++) {
      const Site& d = W.sites[i];
      if (d.type != SiteType::Cave) continue;
      // nearest road tile within 30
      int bx = -1, by = -1; int bd = 1 << 30;
      for (int y = std::max(1, d.ey - 30); y < std::min(WH - 1, d.ey + 30); y++)
        for (int x = std::max(1, d.ex - 30); x < std::min(WW - 1, d.ex + 30); x++)
          if (M.at(x, y) == Ground::Road) { int dd = (x - d.ex) * (x - d.ex) + (y - d.ey) * (y - d.ey); if (dd < bd) { bd = dd; bx = x; by = y; } }
      if (bx >= 0) road(d.ex, d.ey + 1, bx, by, false);
    }
  }

  void vegetation() {
    for (int y = 1; y < WH - 1; y++)
      for (int x = 1; x < WW - 1; x++) {
        size_t i = I(x, y);
        if (M.prop[i] || reserved[i] || M.wall[i] || M.bldgAt[i] >= 0) continue;
        Ground g = M.at(x, y);
        if (g == Ground::Road || g == Ground::Bridge || g == Ground::Plaza || g == Ground::Farmland || g == Ground::StoneFloor) continue;
        // keep a clear margin along roads
        bool nearRoad = false;
        for (int k = 0; k < 4 && !nearRoad; k++) {
          static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
          Ground ng = M.at(x + dx[k], y + dy[k]);
          if (ng == Ground::Road || ng == Ground::Bridge) nearRoad = true;
        }
        float r = hashf(x, y, s + 100);
        float dens = fbm(x / 9.0f, y / 9.0f, s + 200, 3);
        Biome b = M.biomeAt(x, y);
        Prop p = Prop::COUNT;
        auto pick = [&](std::initializer_list<Prop> l) { int n = (int)l.size(); return *(l.begin() + (hash2(x, y, s + 300) % n)); };
        if (g == Ground::Water) {
          if (b == Biome::Swamp && r < 0.12f) p = Prop::LilyPad;
        } else if (groundSolid(g)) {
          continue;
        } else switch (b) {
          case Biome::Plains:
            if (!nearRoad && r < 0.012f + std::max(0.0f, dens - 0.62f) * 0.5f) p = pick({Prop::OakTree, Prop::OakTree2, Prop::BirchTree});
            else if (r < 0.07f) p = pick({Prop::Flowers1, Prop::Flowers2, Prop::Flowers3});
            else if (r < 0.15f) p = Prop::TallGrass;
            else if (r < 0.162f && !nearRoad) p = pick({Prop::Bush, Prop::BerryBush});
            else if (r < 0.167f && !nearRoad) p = pick({Prop::Rock, Prop::Boulder});
            break;
          case Biome::Forest:
            if (!nearRoad && r < 0.10f + dens * 0.32f) p = pick({Prop::OakTree, Prop::OakTree2, Prop::OakTree, Prop::BirchTree});
            else if (r < 0.50f) { float q = hashf(x, y, s + 400); if (q < 0.12f) p = Prop::Fern; else if (q < 0.15f) p = pick({Prop::Bush, Prop::Mushrooms}); else if (q < 0.17f && !nearRoad) p = pick({Prop::Stump, Prop::Log, Prop::MossRock}); else if (q < 0.25f) p = Prop::TallGrass; }
            break;
          case Biome::Autumn:
            if (!nearRoad && r < 0.08f + dens * 0.3f) p = pick({Prop::AutumnTree, Prop::AutumnTree, Prop::BirchTree});
            else if (r < 0.5f) { float q = hashf(x, y, s + 400); if (q < 0.08f) p = Prop::Fern; else if (q < 0.12f) p = pick({Prop::Bush, Prop::Mushrooms, Prop::Flowers3}); else if (q < 0.13f && !nearRoad) p = Prop::Stump; }
            break;
          case Biome::Taiga:
            if (!nearRoad && r < 0.06f + dens * 0.28f) p = pick({Prop::PineTree, Prop::PineTree2});
            else if (r < 0.45f) { float q = hashf(x, y, s + 400); if (q < 0.04f) p = Prop::Fern; else if (q < 0.06f && !nearRoad) p = pick({Prop::MossRock, Prop::Boulder, Prop::Stump}); else if (q < 0.1f) p = Prop::TallGrass; }
            break;
          case Biome::Snow:
            if (!nearRoad && r < 0.03f + dens * 0.18f) p = Prop::SnowPine;
            else if (r < 0.4f) { float q = hashf(x, y, s + 400); if (q < 0.03f && !nearRoad) p = Prop::SnowRock; else if (q < 0.05f) p = Prop::SnowBush; }
            break;
          case Biome::Swamp:
            if (!nearRoad && r < 0.03f + dens * 0.1f) p = pick({Prop::WillowTree, Prop::WillowTree, Prop::DeadTree});
            else if (r < 0.5f) { float q = hashf(x, y, s + 400); if (q < 0.2f) p = Prop::Reeds; else if (q < 0.24f) p = Prop::Mushrooms; else if (q < 0.28f) p = Prop::TallGrass; }
            break;
          case Biome::Desert:
            if (!nearRoad && r < 0.012f) p = Prop::Cactus;
            else if (!nearRoad && r < 0.018f) p = pick({Prop::Rock, Prop::DeadTree});
            break;
          case Biome::Beach: {
            bool warm = temp[i] > 0.6f;
            if (!nearRoad && warm && r < 0.03f) p = Prop::PalmTree;
            else if (!nearRoad && r < 0.04f) p = Prop::Rock;
            else if (r < 0.07f && !warm) p = Prop::Reeds;
            break;
          }
          default: break;
        }
        if (p != Prop::COUNT) M.setProp(x, y, p);
      }
    // palms and reeds by warm rivers/lakes
    for (int y = 1; y < WH - 1; y++)
      for (int x = 1; x < WW - 1; x++) {
        size_t i = I(x, y);
        if (M.prop[i] || reserved[i] || groundSolid(M.at(x, y)) || M.at(x, y) == Ground::Road || M.at(x, y) == Ground::Bridge) continue;
        bool shore = groundWater(M.at(x + 1, y)) || groundWater(M.at(x - 1, y)) || groundWater(M.at(x, y + 1)) || groundWater(M.at(x, y - 1));
        if (!shore) continue;
        float r = hashf(x, y, s + 700);
        if (M.biomeAt(x, y) == Biome::Desert && r < 0.25f) M.setProp(x, y, Prop::PalmTree);
        else if (r < 0.10f && M.biomeAt(x, y) != Biome::Snow) M.setProp(x, y, Prop::Reeds);
      }
  }

  // Guarantee every site can be walked to from the start village. Mountains and forests can wall a basin off
  // (seed 7 shut the start village away from the capital and every shard ruin); where that happens, cut a
  // mountain-pass trail from the stranded site to the nearest walkable road. Runs after vegetation.
  void connectAll() {
    std::vector<uint8_t> giveUp(W.sites.size(), 0), tries(W.sites.size(), 0);
    std::vector<uint8_t> seen((size_t)WW * WH);
    std::vector<int> q;
    q.reserve((size_t)WW * WH);
    auto approach = [&](const Site& st, int& ax, int& ay) {
      ax = st.ex;
      ay = st.ey + (st.type == SiteType::Cave ? 1 : st.type == SiteType::Ruin ? 3 : st.type == SiteType::DragonLair ? 3 : 0);
    };
    for (int pass = 0; pass < 80; pass++) {
      M.rebuildSolid();
      std::fill(seen.begin(), seen.end(), 0);
      q.clear();
      const Site& home = W.sites[W.startSite];
      int sx = home.ex, sy = home.ey;
      for (int r = 0; r < 8 && M.blocked(sx, sy); r++)
        for (int oy = -r; oy <= r && M.blocked(sx, sy); oy++)
          for (int ox = -r; ox <= r; ox++) if (!M.blocked(home.ex + ox, home.ey + oy)) { sx = home.ex + ox; sy = home.ey + oy; break; }
      seen[I(sx, sy)] = 1; q.push_back((int)I(sx, sy));
      for (size_t h = 0; h < q.size(); h++) {
        int x = q[h] % WW, y = q[h] / WW;
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
          int nx = x + dx[k], ny = y + dy[k];
          if (!M.in(nx, ny) || seen[I(nx, ny)] || M.blocked(nx, ny)) continue;
          seen[I(nx, ny)] = 1;
          q.push_back((int)I(nx, ny));
        }
      }
      auto reached = [&](int x, int y) {
        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if (M.in(x + ox, y + oy) && seen[I(x + ox, y + oy)]) return true;
        return false;
      };
      // settlements first, then the story sites, then everything else
      int pick = -1, bestRank = 99;
      for (int i = 0; i < (int)W.sites.size(); i++) {
        if (giveUp[i]) continue;
        const Site& st = W.sites[i];
        int ax, ay;
        approach(st, ax, ay);
        if (reached(ax, ay)) continue;
        int rank = st.type <= SiteType::Village ? 0 : (st.mainQuest || i == W.lair) ? 1 : 2;
        if (rank < bestRank) { bestRank = rank; pick = i; }
      }
      if (pick < 0) return;
      const Site& st = W.sites[pick];
      int ax, ay;
      approach(st, ax, ay);
      // join the nearest reachable road if there is one not much further than the nearest reachable ground
      int bx = -1, by = -1, rx = -1, ry = -1;
      long long bd = 1LL << 40, rd = 1LL << 40;
      for (int y = 1; y < WH - 1; y++)
        for (int x = 1; x < WW - 1; x++) {
          if (!seen[I(x, y)]) continue;
          long long d = (long long)(x - ax) * (x - ax) + (long long)(y - ay) * (y - ay);
          if (d < bd) { bd = d; bx = x; by = y; }
          if (M.at(x, y) == Ground::Road && d < rd) { rd = d; rx = x; ry = y; }
        }
      if (rx >= 0 && rd < bd * 2 + 400) { bx = rx; by = ry; }
      bool okR = bx >= 0 && road(ax, ay, bx, by, true);
      if (!okR || ++tries[pick] >= 3) giveUp[pick] = 1;
    }
  }

  void wildernessCamps() {
    // lone wildlife dens are dynamic; here only static points of interest: abandoned campfires, graves, carts
    for (int k = 0; k < 40; k++) {
      int x = 10 + rng.irange(WW - 20), y = 10 + rng.irange(WH - 20);
      if (reserved[I(x, y)] || !land(x, y) || M.prop[I(x, y)] || M.at(x, y) == Ground::Road) continue;
      static const Prop deco[] = {Prop::Gravestone, Prop::Log, Prop::Woodpile, Prop::Cart, Prop::Bones, Prop::Chest};
      Prop p = deco[rng.irange(6)];
      if (p == Prop::Chest && rng.f() < 0.5f) p = Prop::Barrel;
      M.setProp(x, y, p);
    }
  }

  // WORLDGEN_V2: wilderness dens. A small clearing with a few readable props (bones and a skull pile for wolves,
  // a campfire and crates for goblins, webs for spiders, graves for a barrow...) and a chest tucked at the back.
  // Every random choice comes from the "dens" stream, so nothing else in the world moves. Solid props are only
  // placed on tiles whose 8 neighbours are all open, so a den can never cut a path (rpg_test checks reachability).
  void dens() {
    Rng r = stream("dens");
    const Site& home = W.sites[W.startSite];
    auto openAround = [&](int x, int y) {
      for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) if (M.blocked(x + ox, y + oy) || M.bldgAt[I(x + ox, y + oy)] >= 0) return false;
      return true;
    };
    auto natural = [&](Prop p) { return (int)p <= (int)Prop::Fern; };
    for (int tries = 0; tries < 4000 && W.dens.size() < 64; tries++) {
      int x = 16 + r.irange(WW - 32), y = 16 + r.irange(WH - 32);
      Biome b = M.biomeAt(x, y);
      if (b == Biome::Ocean || b == Biome::Beach || b == Biome::Mountain) continue;
      if (W.siteAt(x, y, 9) >= 0) continue;
      float dh = std::hypot((float)(x - home.r.cx()), (float)(y - home.r.cy()));
      if (dh < 20) continue;   // not on the doorstep of the start village
      bool farEnough = true;
      for (const Den& d : W.dens) if (std::abs(d.x - x) < 24 && std::abs(d.y - y) < 24) { farEnough = false; break; }
      if (!farEnough) continue;
      // the clearing: open land, no roads, no water, no settlement ground within it
      bool ok = true;
      for (int oy = -3; oy <= 3 && ok; oy++)
        for (int ox = -3; ox <= 3 && ok; ox++) {
          int tx = x + ox, ty = y + oy;
          if (!M.in(tx, ty) || reserved[I(tx, ty)] || M.wall[I(tx, ty)] || M.bldgAt[I(tx, ty)] >= 0) { ok = false; break; }
          Ground g = M.at(tx, ty);
          if (g == Ground::Road || g == Ground::Bridge || g == Ground::Plaza || g == Ground::Farmland) ok = false;
          if (std::abs(ox) <= 2 && std::abs(oy) <= 2 && groundSolid(g)) ok = false;
          int pp = M.prop[I(tx, ty)];
          if (pp && !natural((Prop)(pp - 1))) ok = false;   // leave camps, graves, chests alone
        }
      if (!ok) continue;
      Den d;
      d.x = x; d.y = y;
      float q = r.f();
      using art::Monster;
      switch (b) {
        case Biome::Plains: d.mon = q < 0.5f ? Monster::Wolf : q < 0.85f ? Monster::Goblin : Monster::Skeleton; break;
        case Biome::Forest: d.mon = q < 0.45f ? Monster::Wolf : q < 0.75f ? Monster::Spider : Monster::Bear; break;
        case Biome::Autumn: d.mon = q < 0.4f ? Monster::Goblin : q < 0.7f ? Monster::Spider : Monster::Bear; break;
        case Biome::Taiga: d.mon = q < 0.5f ? Monster::Wolf : q < 0.8f ? Monster::Bear : Monster::Troll; break;
        case Biome::Snow: d.mon = q < 0.55f ? Monster::IceWolf : q < 0.8f ? Monster::FrostSpider : Monster::Troll; break;
        case Biome::Swamp: d.mon = q < 0.6f ? Monster::Spider : Monster::Skeleton; break;
        case Biome::Desert: d.mon = q < 0.6f ? Monster::Goblin : Monster::Skeleton; break;
        default: continue;
      }
      switch (d.mon) {
        case Monster::Wolf: case Monster::IceWolf: d.pack = (uint8_t)(3 + (r.f() < 0.35f)); break;
        case Monster::Goblin: d.pack = (uint8_t)(3 + r.irange(2)); break;
        case Monster::Skeleton: d.pack = 3; break;
        case Monster::Spider: case Monster::FrostSpider: d.pack = 2; break;
        default: d.pack = 1; break;   // bears, trolls
      }
      // clear an irregular glade (trees and bushes go; the ground is left alone: hard-edged dirt tiles read as boxes)
      bool snowy = b == Biome::Snow;
      for (int oy = -3; oy <= 3; oy++)
        for (int ox = -3; ox <= 3; ox++) {
          int tx = x + ox, ty = y + oy;
          float rr = std::sqrt((float)(ox * ox) + oy * oy * 1.3f) + hashf(tx, ty, s + 7101) * 1.2f;
          if (rr > 3.2f) continue;
          int pp = M.prop[I(tx, ty)];
          if (pp && natural((Prop)(pp - 1))) M.prop[I(tx, ty)] = 0;
        }
      M.rebuildSolid();
      // scatter: (dx, dy, prop) candidates by kind; the back (north) half gets the big pieces
      auto put = [&](int ox, int oy, Prop p) {
        int tx = x + ox, ty = y + oy;
        if (!M.in(tx, ty) || M.prop[I(tx, ty)] || groundSolid(M.at(tx, ty))) return false;
        if (propSolid(p)) {
          if (!openAround(tx, ty)) return false;
          M.setProp(tx, ty, p);
          M.solid[I(tx, ty)] = 1;
        } else M.setProp(tx, ty, p);
        return true;
      };
      Prop rock = snowy ? Prop::SnowRock : (b == Biome::Forest || b == Biome::Swamp || b == Biome::Taiga) ? Prop::MossRock : Prop::Boulder;
      std::vector<Prop> big, small;
      switch (d.mon) {
        case Monster::Wolf: case Monster::IceWolf: big = {rock, Prop::Boulder, Prop::DeadTree}; small = {Prop::Bones, Prop::SkullPile, Prop::Bones}; break;
        case Monster::Goblin: big = {Prop::Tent, Prop::Crate, Prop::Woodpile}; small = {Prop::Bones, Prop::Barrel}; break;
        case Monster::Skeleton: big = {Prop::Gravestone, Prop::Gravestone, Prop::DeadTree}; small = {Prop::SkullPile, Prop::Bones, Prop::Bones}; break;
        case Monster::Spider: case Monster::FrostSpider: big = {Prop::DeadTree, rock}; small = {Prop::Cobweb, Prop::Cobweb, Prop::Bones}; break;
        case Monster::Troll: big = {rock, Prop::Boulder, rock}; small = {Prop::SkullPile, Prop::Bones, Prop::Bones}; break;
        default: big = {rock, Prop::Boulder, Prop::Log}; small = {Prop::Bones, Prop::Bones}; break;   // bear
      }
      if (d.mon == Monster::Goblin) put(0, 0, Prop::Campfire);
      // the reward: a chest at the back of the den (placed first so the bigger pieces arrange around it)
      {
        static const int co[][2] = {{0, -2}, {1, -2}, {-1, -2}, {0, -1}, {2, -1}, {-2, -1}, {1, -1}, {-1, -1}, {2, -2}, {-2, -2}};
        int start = r.irange(3);
        for (int k = 0; k < 10; k++) { const int* c = co[(start + k) % 10]; if (put(c[0], c[1], Prop::Chest)) break; }
      }
      // big pieces in an arc behind the centre
      int placedBig = 0;
      for (int k = 0; k < 10 && placedBig < (int)big.size(); k++) {
        float a = 3.14159f + 0.35f + r.f() * 2.45f;   // upper half (north)
        float rad = 2.0f + r.f() * 1.2f;
        if (put((int)std::lround(std::cos(a) * rad), (int)std::lround(std::sin(a) * rad * 0.8f), big[placedBig])) placedBig++;
      }
      for (int k = 0; k < 12; k++) {
        if (!put(r.irange(5) - 2, r.irange(5) - 2, small[r.irange((int)small.size())])) continue;
        if (k > 4 && r.f() < 0.5f) break;
      }
      W.dens.push_back(d);
    }
    M.rebuildSolid();
  }

  void run() {
    terrain();
    rivers();
    placeSettlements();
    // the start village: the settlement closest to the warm south-centre lowlands
    int bestV = -1; float bd = 1e30f;
    for (int i = 0; i < (int)W.sites.size(); i++) {
      const Site& st = W.sites[i];
      if (st.type != SiteType::Village) continue;
      Biome b = M.biomeAt(st.r.cx(), st.r.cy());
      float d = std::hypot((float)(st.r.cx() - WW / 2), (float)(st.r.cy() - WH * 0.68f));
      if (b == Biome::Snow || b == Biome::Desert) d += 150;
      if (d < bd) { bd = d; bestV = i; }
    }
    W.startSite = bestV >= 0 ? bestV : 0;
    float cd = 1e30f; W.capital = W.startSite;
    for (int i = 0; i < (int)W.sites.size(); i++) {
      if (W.sites[i].type != SiteType::City) continue;
      float d = std::hypot((float)(W.sites[i].r.cx() - W.sites[W.startSite].r.cx()), (float)(W.sites[i].r.cy() - W.sites[W.startSite].r.cy()));
      if (d < cd) { cd = d; W.capital = i; }
    }
    for (int i = 0; i < (int)W.sites.size(); i++) stampSettlement(i);
    // the main quest starts with the jarl in the capital's keep: make sure the capital actually got one
    auto hasKeep = [&](int si) {
      const Site& st = W.sites[si];
      for (int b = st.bldgFirst; b < st.bldgFirst + st.bldgCount; b++) if (M.bldgs[b].type == art::Building::Keep) return true;
      return false;
    };
    if (W.sites[W.capital].type != SiteType::City || !hasKeep(W.capital)) {
      float kd = 1e30f;
      for (int i = 0; i < (int)W.sites.size(); i++) {
        if (W.sites[i].type != SiteType::City || !hasKeep(i)) continue;
        float d = std::hypot((float)(W.sites[i].r.cx() - W.sites[W.startSite].r.cx()), (float)(W.sites[i].r.cy() - W.sites[W.startSite].r.cy()));
        if (d < kd) { kd = d; W.capital = i; }
      }
    }
    placeCaves(18);
    placeRuins(8);
    placeCamps(10);
    placeShrines(8);
    placeLair();
    roads();
    vegetation();
    wildernessCamps();
    connectAll();
    M.rebuildSolid();
    if (ver >= WORLDGEN_V2) dens();
    if (ver >= WORLDGEN_V3) rebuildCityWallsV3();
    // difficulty per site
    for (auto& st : W.sites) st.level = W.zoneLevel(st.ex, st.ey);
    // main quest: the three ruins furthest apart-ish from home, mid difficulty
    std::vector<int> ruins;
    for (int i = 0; i < (int)W.sites.size(); i++) if (W.sites[i].type == SiteType::Ruin) ruins.push_back(i);
    if (ver >= WORLDGEN_V4) {
      // v4: the story only uses ruins that can be walked to from the start (no shard on an island across deep water)
      std::vector<uint8_t> seen((size_t)WW * WH, 0);
      std::vector<int> q;
      const Site& home = W.sites[W.startSite];
      int sx = home.ex, sy = home.ey;
      for (int r = 0; r < 8 && M.blocked(sx, sy); r++)
        for (int oy = -r; oy <= r && M.blocked(sx, sy); oy++)
          for (int ox = -r; ox <= r; ox++) if (!M.blocked(home.ex + ox, home.ey + oy)) { sx = home.ex + ox; sy = home.ey + oy; break; }
      seen[I(sx, sy)] = 1; q.push_back((int)I(sx, sy));
      for (size_t h = 0; h < q.size(); h++) {
        int x = q[h] % WW, y = q[h] / WW;
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
          int nx = x + dx[k], ny = y + dy[k];
          if (!M.in(nx, ny) || seen[I(nx, ny)] || M.blocked(nx, ny)) continue;
          seen[I(nx, ny)] = 1;
          q.push_back((int)I(nx, ny));
        }
      }
      std::vector<int> ok;
      for (int i : ruins) {
        const Site& st = W.sites[i];
        bool r = false;
        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if (M.in(st.ex + ox, st.ey + 3 + oy) && seen[I(st.ex + ox, st.ey + 3 + oy)]) r = true;
        if (r) ok.push_back(i);
      }
      if (ok.size() >= 3) ruins = ok;
    }
    std::sort(ruins.begin(), ruins.end(), [&](int a, int b) { return W.sites[a].level < W.sites[b].level; });
    // every other ruin by level, so the three shards climb in difficulty (v4: three in a row when the reachable
    // ruins are too few for that, so the story always has its three)
    const size_t stride = (ver >= WORLDGEN_V4 && ruins.size() < 5) ? 1 : 2;
    for (size_t k = 0, n = 0; k < ruins.size() && n < 3; k += stride, n++) W.sites[ruins[k]].mainQuest = true;
    W.sites[W.startSite].discovered = true;
  }
};
}  // namespace

uint64_t genSubSeed(uint64_t seed, const char* feature) {
  uint64_t h = 1469598103934665603ull ^ (seed * 0x9E3779B97F4A7C15ull);
  for (const char* c = feature; *c; c++) { h ^= (uint8_t)*c; h *= 1099511628211ull; }
  h ^= h >> 33; h *= 0xFF51AFD7ED558CCDull; h ^= h >> 33;
  return h;
}

// ---- WORLDGEN_V7 (M0b): storeys and hearths. h is a per-building hash; the answers must never change for V7.
int bldgStoreysV7(art::Building t, int wTiles, int hTiles, uint32_t h) {
  (void)hTiles;
  float f = (h >> 8) * (1.0f / 16777216.0f);
  switch (t) {
    case art::Building::Inn: case art::Building::Keep: return 2;   // the public room below, rented rooms / quarters above
    case art::Building::Tower: return 3;                           // workroom, library, the mage's chamber at the top
    case art::Building::House: return wTiles >= 5 ? (f < 0.65f ? 2 : 1) : (wTiles >= 4 ? (f < 0.4f ? 2 : 1) : 1);
    case art::Building::StoneHouse: return wTiles >= 4 ? (f < 0.6f ? 2 : 1) : (f < 0.3f ? 2 : 1);   // narrow town houses too
    case art::Building::Shop: return wTiles >= 4 && f < 0.7f ? 2 : 1;   // the shopkeeper lives above the shop
    default: return 1;   // smithy, temple, farmhouse, hut
  }
}
bool bldgHearthV7(art::Building t, int storeys, uint32_t h) {
  switch (t) {
    case art::Building::Temple: case art::Building::Tower: return false;   // braziers and a cauldron, no chimney
    case art::Building::Shop: return storeys >= 2 || (h >> 8) % 100 < 40;   // living quarters cook; a lock-up shop may not
    default: return true;   // homes, the inn's kitchen, the smithy's forge, the keep's great hearth
  }
}

uint32_t World::fingerprint() const {
  uint32_t h = 2166136261u;
  auto mix = [&](int v) { for (int k = 0; k < 4; k++) { h ^= (uint8_t)(v >> (k * 8)); h *= 16777619u; } };
  mix(genVersion); mix((int)sites.size()); mix((int)over.bldgs.size()); mix(startSite); mix(capital); mix(lair);
  for (const Site& st : sites) {
    mix((int)st.type); for (char c : st.name) mix(c);
    mix(st.r.x); mix(st.r.y); mix(st.r.w); mix(st.r.h); mix(st.ex); mix(st.ey); mix(st.bldgFirst); mix(st.bldgCount); mix((int)st.seed);
  }
  for (const Bldg& b : over.bldgs) { mix((int)b.type); mix(b.r.x); mix(b.r.y); mix(b.r.w); mix(b.r.h); mix(b.site); mix((int)b.owner); mix((int)b.seed); }
  if (genVersion >= WORLDGEN_V7) for (const Bldg& b : over.bldgs) { mix(b.storeys); mix(b.hearth ? 1 : 0); }
  if (genVersion >= WORLDGEN_V4) {
    // from v4 the city walls, gates and openings are part of the fingerprint, so any later wall drift flags old saves
    // (worldChanged). Earlier versions keep their original fingerprint (their saves store it).
    mix((int)gates.size()); for (auto& g : gates) { mix(g.first); mix(g.second); }
    mix((int)wallGaps.size()); for (const IRect& r : wallGaps) { mix(r.x); mix(r.y); mix(r.w); mix(r.h); }
    for (size_t i = 0; i < over.wall.size(); i++) if (over.wall[i]) mix((int)i);
  }
  return h;
}

void World::generate(uint64_t sd, int genVer) {
  seed = sd;
  genVersion = genVer < WORLDGEN_V1 ? WORLDGEN_V1 : (genVer > WORLDGEN_LATEST ? WORLDGEN_LATEST : genVer);
  sites.clear();
  gates.clear();
  dens.clear();
  wallGaps.clear();
  lair = -1;
  kingdoms.clear();
  endless = false; ox = 0; oy = 0; src.reset();
  siteById.clear(); bldgById.clear(); denById.clear(); kingdomById.clear(); spawnKeys.clear(); gateKeys.clear();
  Gen g(*this, sd);
  g.run();
  // M1: stable ids (the classic island's are its indices) and the island's one kingdom, seated at the capital
  for (int i = 0; i < (int)sites.size(); i++) { sites[(size_t)i].id = ew::legacySiteId(i); siteById[sites[(size_t)i].id] = i; }
  for (int i = 0; i < (int)over.bldgs.size(); i++) { over.bldgs[(size_t)i].id = ew::legacyBldgId(i); bldgById[over.bldgs[(size_t)i].id] = i; }
  for (int i = 0; i < (int)dens.size(); i++) { dens[(size_t)i].id = ew::makeId(0, 0, ew::IdKind::Den, (uint32_t)i); denById[dens[(size_t)i].id] = i; }
  Kingdom k;
  k.id = ew::makeId(0, 0, ew::IdKind::Kingdom, 0);
  Rng kr(genSubSeed(sd, "kingdom"));
  k.name = makeTownName(kr);
  static const uint32_t fields[] = {rgba(150, 32, 36), rgba(36, 64, 140), rgba(28, 100, 60), rgba(110, 40, 120), rgba(180, 120, 30)};
  k.color = fields[kr.irange(5)];
  k.color2 = rgba(232, 214, 160);
  k.emblem = (uint8_t)kr.irange(8);
  if (capital >= 0 && capital < (int)sites.size()) {
    k.capitalId = sites[(size_t)capital].id;
    k.gx = sites[(size_t)capital].ex; k.gy = sites[(size_t)capital].ey;
    sites[(size_t)capital].capital = true;
  }
  kingdoms.push_back(k);
  kingdomById[k.id] = 0;
  for (Site& st : sites) if (st.settlement()) st.kingdom = 0;
  if (startSite >= 0 && startSite < (int)sites.size()) sites[(size_t)startSite].start = true;
}

