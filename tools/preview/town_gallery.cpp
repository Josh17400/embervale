// Town gallery (TOWNS lane, M1): a settlement buffer from rpg/world/settlement.cpp drawn top-down with the real sprites
// (buildings with their facts, wall tiles, gatehouses in the kingdom's colours, props, kingdom banners), the ground
// painted per tile with baked building and wall shadows, so layouts, walls, gates, districts and palaces can be judged
// at 1x and zoomed without starting the game.
//   town_gallery <outDir> [--seed N] [--type village|town|city|capital|all] [--arch NAME] [--land plains|river|hill|coast]
//                [--night]
//       writes <type>_<arch>_<land>_s<seed>.png (1x, the whole buffer) and ..._heart / _gate / _palace / _quarter crops
//       (3x), plus heraldry.png (the eight charges on banners in four kingdoms' colours, gatehouses, palaces, barracks)
//   town_gallery <outDir> heraldry      only the heraldry panel
#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <string>
#include "rpg/sim/world.h"
#include "rpg/world/dmath.h"
#include "rpg/world/settlement.h"
#include "rpg/world/town_rules.h"
#include "tools/preview/preview_util.h"

namespace {

using art::Building;
using art::Prop;

uint32_t ghash(int x, int y) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

uint32_t shadeGround(uint32_t c, int level) {
  if (!level) return c;
  uint32_t s = art::mix(c, rgba(48, 34, 92), level == 2 ? 0.42f : 0.30f);
  return art::shade(s, level == 2 ? 0.70f : 0.80f);
}

// a quick ground painter (the game's terrain renderer blends edges; this one only has to read clearly)
uint32_t groundPx(Ground g, int px, int py) {
  uint32_t n = ghash(px / 2, py / 2) % 100, f = ghash(px, py * 3 + 1) % 100;
  int tx = px / 16, ty = py / 16, lx = px % 16, ly = py % 16;
  switch (g) {
    case Ground::Grass: case Ground::Meadow: {
      uint32_t c = g == Ground::Meadow ? rgba(108, 160, 70) : rgba(86, 140, 62);
      if (n < 18) c = art::shade(c, 0.92f); else if (n > 92) c = art::shade(c, 1.1f);
      if (g == Ground::Meadow && f == 0) c = rgba(236, 220, 120);
      return c;
    }
    case Ground::ForestFloor: return n < 30 ? rgba(58, 98, 52) : rgba(66, 108, 56);
    case Ground::Autumn: return n < 30 ? rgba(150, 110, 54) : rgba(162, 122, 60);
    case Ground::Tundra: return n < 30 ? rgba(120, 132, 104) : rgba(132, 144, 112);
    case Ground::Snow: return n < 30 ? rgba(214, 224, 236) : rgba(232, 238, 246);
    case Ground::Sand: return n < 30 ? rgba(210, 182, 128) : rgba(222, 196, 140);
    case Ground::Dirt: return n < 25 ? rgba(138, 104, 70) : (n > 90 ? rgba(164, 128, 88) : rgba(150, 114, 78));
    case Ground::Farmland: return (ly % 4 < 2) ? rgba(110, 76, 50) : rgba(132, 94, 60);
    case Ground::Road: {   // cobbles: rounded stones with dark joints
      int bx = (px + ((py / 5) & 1) * 3) / 6, by = py / 5;
      bool joint = (px + ((py / 5) & 1) * 3) % 6 == 0 || py % 5 == 0;
      uint32_t c = ghash(bx, by) % 3 == 0 ? rgba(150, 140, 130) : rgba(166, 156, 142);
      return joint ? rgba(104, 96, 96) : c;
    }
    case Ground::Plaza: {   // flagstones
      bool joint = lx == 0 || ly == 0 || (ly == 8 && (lx < 8) == ((tx + ty) & 1));
      uint32_t c = ghash(tx * 2 + (lx >= 8), ty * 2 + (ly >= 8)) % 4 == 0 ? rgba(186, 178, 164) : rgba(198, 190, 176);
      return joint ? rgba(140, 132, 124) : c;
    }
    case Ground::Bridge: return (ly % 4 == 0) ? rgba(96, 64, 44) : rgba(150, 106, 66);
    case Ground::Water: case Ground::DeepWater: return f < 3 ? rgba(150, 200, 230) : (g == Ground::DeepWater ? rgba(40, 80, 140) : rgba(62, 120, 176));
    case Ground::Rock: return n < 40 ? rgba(110, 104, 110) : rgba(124, 118, 122);
    case Ground::Swamp: return n < 40 ? rgba(76, 96, 64) : rgba(86, 104, 70);
    default: return rgba(86, 140, 62);
  }
}

struct Spec { SiteType type; bool capital; ew::Archetype arch; int land; uint32_t seed; };
const char* archNames[] = {"plain", "farming", "fishing", "port", "mining", "rivercrossing", "hillfort", "market"};
const char* landNames[] = {"plains", "river", "hill", "coast"};

void build(const Spec& sp, ew::SitePlan& p, ew::KingdomPlan& k, ew::SettlementOut& so) {
  p = ew::SitePlan();
  p.type = sp.type;
  p.archetype = sp.arch;
  p.seed = sp.seed;
  ew::settlementFootprint(sp.type, sp.arch, p.seed, p.w, p.h);
  p.gx = 1000 - p.w / 2; p.gy = -2000 - p.h / 2; p.ex = 1000; p.ey = -2000;
  p.bldgCap = 400;
  p.flags = sp.capital ? ew::SPF_CAPITAL : 0;
  p.name = "GALLERY";
  static const uint32_t fields[4] = {rgba(40, 70, 160), rgba(150, 30, 40), rgba(40, 110, 60), rgba(90, 40, 120)};
  static const uint32_t trims[4] = {rgba(230, 200, 80), rgba(240, 236, 220), rgba(230, 200, 80), rgba(220, 180, 90)};
  k = ew::KingdomPlan();
  k.id = 7; k.name = "GALLERY";
  k.color = fields[sp.seed % 4]; k.color2 = trims[sp.seed % 4]; k.emblem = (uint8_t)(sp.seed % 8);
  ew::SettlementCtx ctx;
  ctx.plan = &p;
  ctx.kingdom = &k;
  ctx.rx = 3; ctx.ry = -8;
  Rng r(sp.seed * 31 + 7);
  int nr = sp.type == SiteType::Village ? 2 : 3;
  float a0 = r.f() * ew::D_TAU;
  for (int i = 0; i < nr; i++) ctx.roadBearings.push_back(ew::dwrap(a0 + i * ew::D_TAU / nr + r.range(-0.3f, 0.3f)));
  const int cxg = 1000, cyg = -2000, hw = p.w / 2, land = sp.land;
  ctx.base = [land, cxg, cyg, hw](int32_t gx, int32_t gy, Ground& g, Biome& bi, uint8_t& h) {
    g = Ground::Grass; bi = Biome::Plains; h = 1;
    int dx = gx - cxg, dy = gy - cyg;
    if (land == 1) {
      int rxv = -hw / 3 + (int)(ew::dsin(dy * 0.07f) * 4.0f) + dy / 6;
      if (std::abs(dx - rxv) <= 1) g = Ground::Water;
      else if (std::abs(dx - rxv) == 2) g = Ground::Dirt;
    } else if (land == 2) {
      h = (uint8_t)std::clamp(3 - (dy + dx / 2) / 13, 0, 6);
      bi = Biome::Forest;
    } else if (land == 3) {
      if (dx > hw * 2 / 3 + (int)(ew::dsin(dy * 0.11f) * 3.0f)) { g = Ground::Water; bi = Biome::Ocean; }
      else if (dx > hw * 2 / 3 - 3) { g = Ground::Sand; bi = Biome::Beach; }
    }
    // the wild land round the town
    if (g == Ground::Grass && (uint32_t)(gx * 73856093 ^ gy * 19349663) % 7 == 0 && std::abs(dx) > hw) g = Ground::ForestFloor;
  };
  auto t0 = std::chrono::steady_clock::now();
  ew::buildSettlement(ctx, so);
  std::printf("built %s%s %s on %s seed %u: %dx%d, %zu buildings, %d homes, %zu gates, %.1f ms\n", sp.capital ? "capital " : "", siteTypeName(sp.type),
              archNames[(int)sp.arch], landNames[sp.land], sp.seed, p.w, p.h, so.buf.bldgs.size(), so.homes, so.gates.size(),
              std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
}

// draw the buffer like the game does: ground, baked shadows, then rows top to bottom (walls, towers, gatehouses,
// buildings and props by their base row)
Canvas render(const ew::SettlementOut& so, const ew::KingdomPlan& k, bool night) {
  const Map& m = so.buf;
  Canvas c(m.w * 16, m.h * 16);
  for (int py = 0; py < c.h; py++)
    for (int px = 0; px < c.w; px++) c.set(px, py, groundPx(m.at(px / 16, py / 16), px, py));
  // building shadows (down-right, as the terrain bakes them) and the wall's shade
  std::vector<Canvas> sprites(m.bldgs.size());
  std::vector<art::BuildingInfo> infos(m.bldgs.size());
  for (size_t i = 0; i < m.bldgs.size(); i++) {
    const Bldg& b = m.bldgs[i];
    sprites[i] = art::buildingSprite(b.type, b.r.w, b.r.h, bldgArch(b), b.seed, &infos[i], bldgFacts(b));
    int fx = b.r.x * 16, fy = b.r.y * 16, fw = b.r.w * 16, fh = b.r.h * 16;
    int L = std::clamp(infos[i].height / 4, 6, 14), Ly = std::max(3, L * 3 / 5);
    for (int y = fy; y < fy + fh + Ly + 2; y++)
      for (int x = fx; x < fx + fw + L + 2; x++) {
        if (x >= fx && x < fx + fw && y >= fy && y < fy + fh) continue;
        int lvl = 0;
        if (y >= fy + fh && y < fy + fh + 2 && x >= fx && x < fx + fw + 1) lvl = 2;
        else
          for (int q = 1; q <= 8 && !lvl; q++) {
            int sx = x - L * q / 8, sy = y - Ly * q / 8;
            if (sx >= fx && sx < fx + fw && sy >= fy && sy < fy + fh) lvl = 1;
          }
        if (lvl && x >= 0 && y >= 0 && x < c.w && y < c.h) c.set(x, y, shadeGround(c.get(x, y), lvl));
      }
  }
  for (int py = 0; py < c.h; py++)
    for (int px = 0; px < c.w; px++) {
      int lvl = art::wallShadeAt(m.wall.data(), m.w, m.h, px, py);
      if (lvl) c.set(px, py, shadeGround(c.get(px, py), lvl));
    }
  for (auto& gt : so.gates)
    for (int y = 0; y < 16; y++)
      for (int x = 3; x < 45; x++) {
        int px = gt.first * 16 + x, py = gt.second * 16 + y;
        if (px < c.w && py < c.h) c.set(px, py, shadeGround(c.get(px, py), y < 12 ? 2 : 1));
      }
  std::vector<uint32_t> keys;
  art::wallKeys(m.wall.data(), m.w, m.h, so.gates.data(), (int)so.gates.size(), keys);
  std::map<uint32_t, Canvas> wallCache;
  std::map<int, Canvas> propCache;
  const Canvas banner = art::kingdomBanner(k.color, k.color2, k.emblem);
  const Canvas gate = art::gateHouse(7, k.color, k.color2, k.emblem);
  Board B(c.w, c.h);
  B.c = c;
  auto putProp = [&](int x, int y, int p) {
    Prop pp = (Prop)(p - 1);
    if (pp == Prop::Filler) return;
    if (!propCache.count(p)) propCache[p] = pp == Prop::Banner ? banner : art::propSprite(pp);
    const Canvas& s = propCache[p];
    const int w = art::propW(pp), h = art::propH(pp);
    Canvas f(w, h);
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) f.set(i, j, s.get(i, j));
    B.put(f, x * 16 + 8 - w / 2, y * 16 + 16 - h);
  };
  for (int y = 0; y < m.h; y++) {
    for (int pass = 0; pass < 2; pass++)
      for (int x = 0; x < m.w; x++) {
        uint32_t kk = keys[(size_t)y * m.w + x];
        if (!kk || ((kk & art::WALL_BIT_TOWER) != 0) != (pass == 1)) continue;
        if (groundWater(m.at(x, y))) kk |= art::WALL_BIT_CULVERT;
        if (!wallCache.count(kk)) wallCache[kk] = art::wallTile(kk);
        B.put(wallCache[kk], x * 16 - art::WALL_OX, y * 16 - art::WALL_OY);
      }
    for (auto& gt : so.gates)
      if (gt.second == y) B.put(gate, gt.first * 16 - art::GATE_OX, gt.second * 16 - art::GATE_OY);
    struct Item { int x; int kind; int idx; };
    std::vector<Item> items;
    for (size_t i = 0; i < m.bldgs.size(); i++)
      if (m.bldgs[i].r.y + m.bldgs[i].r.h - 1 == y) items.push_back({m.bldgs[i].r.x, 0, (int)i});
    for (int x = 0; x < m.w; x++)
      if (m.propAt(x, y)) items.push_back({x, 1, x});
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.x < b.x; });
    for (const Item& it : items) {
      if (it.kind == 1) { putProp(it.idx, y, m.propAt(it.idx, y)); continue; }
      const Bldg& b = m.bldgs[(size_t)it.idx];
      Canvas s = sprites[(size_t)it.idx];
      if (night) s = art::buildingNight(s, infos[(size_t)it.idx].glass, b.seed);
      B.put(s, b.r.x * 16 - art::BLDG_PAD_X, (b.r.y + b.r.h) * 16 + art::BLDG_PAD_B - s.h);
    }
  }
  if (night)
    for (uint32_t& px : B.c.px) {
      uint32_t lum = (px & 255) + ((px >> 8) & 255) + ((px >> 16) & 255);
      if (lum > 560 && (px & 255) > 200 && ((px >> 8) & 255) > 150) continue;   // lit windows and lamps stay bright
      px = (px & 0xFF000000u) | (art::mix(art::shade(px, 0.42f), rgba(22, 26, 70), 0.38f) & 0xFFFFFFu);
    }
  return B.c;
}

void crop(const Canvas& src, int cx, int cy, int w, int h, const std::string& path) {
  int x0 = std::clamp(cx - w / 2, 0, std::max(0, src.w - w)), y0 = std::clamp(cy - h / 2, 0, std::max(0, src.h - h));
  Canvas c(std::min(w, src.w), std::min(h, src.h));
  for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) c.set(x, y, src.get(x0 + x, y0 + y));
  savePng(c, path, 3);
}

void town(const std::string& dir, const Spec& sp, bool night) {
  ew::SitePlan p;
  ew::KingdomPlan k;
  ew::SettlementOut so;
  build(sp, p, k, so);
  Canvas c = render(so, k, night);
  char base[200];
  std::snprintf(base, sizeof base, "%s/%s_%s_%s_s%u%s", dir.c_str(), sp.capital ? "capital" : siteTypeName(sp.type), archNames[(int)sp.arch],
                landNames[sp.land], sp.seed, night ? "_night" : "");
  savePng(c, std::string(base) + ".png", 1);
  const int hx = (so.ex - so.gx) * 16, hy = (so.ey - so.gy) * 16;
  crop(c, hx, hy, 400, 240, std::string(base) + "_heart.png");
  if (!so.gates.empty()) crop(c, so.gates[0].first * 16 + 24, so.gates[0].second * 16, 400, 240, std::string(base) + "_gate.png");
  for (const Bldg& b : so.buf.bldgs)
    if (b.type == Building::Palace) crop(c, b.r.cx() * 16, (b.r.y + b.r.h + 4) * 16, 520, 400, std::string(base) + "_palace.png");
  if (sp.type == SiteType::City) {
    int qx = hx + (int)(p.w * 16 * 0.25f), qy = hy + (int)(p.h * 16 * 0.2f);
    crop(c, qx, qy, 400, 240, std::string(base) + "_quarter.png");
  }
}

// the heraldry panel: banners with every charge in four kingdoms' colours, gatehouses, palaces and barracks by biome
void heraldry(const std::string& dir) {
  static const uint32_t fields[4] = {rgba(40, 70, 160), rgba(150, 30, 40), rgba(40, 110, 60), rgba(90, 40, 120)};
  static const uint32_t trims[4] = {rgba(230, 200, 80), rgba(240, 236, 220), rgba(230, 200, 80), rgba(220, 180, 90)};
  Board b(16 + 8 * 40 + 20 + 3 * 104, 24 + 4 * 44 + 16 + 3 * 220);
  for (int k = 0; k < 4; k++)
    for (int e = 0; e < 8; e++) {
      Canvas s = art::kingdomBanner(fields[k], trims[k], e);
      Canvas f(art::propW(Prop::Banner), art::propH(Prop::Banner));
      for (int j = 0; j < f.h; j++) for (int i = 0; i < f.w; i++) f.set(i, j, s.get(i + (k % 4) * f.w, j));
      b.put(f, 16 + e * 40, 24 + k * 44);
    }
  b.text(16, 6, "BANNERS: CROWN SWORD TOWER STAR TREE SUN MOON EAGLE");
  for (int k = 0; k < 3; k++) {
    Canvas g = art::gateHouse(7 + k, fields[k], trims[k], k * 3);
    b.put(g, 16 + 8 * 40 + 20 + k * 104, 24 + k * 30);
  }
  static const int biomes[3] = {2, 6, 8};
  for (int r = 0; r < 3; r++) {
    art::BuildingFacts f;
    f.storeys = 2;
    f.banner = fields[r]; f.banner2 = trims[r]; f.emblem = r * 2 + 1;
    uint32_t seed = 900u + (uint32_t)r * 77u;
    art::ArchStyle st = art::archForBiome(biomes[r], seed);
    art::BuildingInfo info;
    Canvas pal = art::buildingSprite(Building::Palace, 15, 7, st, seed, &info, f);
    std::printf("palace biome %d: %d storeys painted, %d chimneys, height %d px\n", biomes[r], info.storeys, info.chimneys, info.height);
    int y0 = 24 + 4 * 44 + 16 + r * 220;
    b.put(pal, 16, y0 + 210 - pal.h);
    Canvas bar = art::buildingSprite(Building::Barracks, 7, 4, st, seed + 5, &info, f);
    std::printf("barracks biome %d: %d storeys painted, %d chimneys\n", biomes[r], info.storeys, info.chimneys);
    b.put(bar, 16 + pal.w + 20, y0 + 210 - bar.h);
    Canvas keep = art::buildingSprite(Building::Keep, 9, 4, st, seed + 9, &info, f);
    b.put(keep, 16 + pal.w + bar.w + 40, y0 + 210 - keep.h);
    Canvas inn = art::buildingSprite(Building::Inn, 6, 3, st, seed + 13, &info, f);
    b.put(inn, 16 + pal.w + bar.w + keep.w + 60, y0 + 210 - inn.h);
  }
  savePng(b.c, dir + "/heraldry.png", 2);
  savePng(b.c, dir + "/heraldry_1x.png", 1);
}

}  // namespace

int main(int argc, char** argv) {
  std::string dir = argc >= 2 ? argv[1] : ".";
  SDL_CreateDirectory(dir.c_str());
  if (argc >= 3 && !std::strcmp(argv[2], "heraldry")) { heraldry(dir); return 0; }
  uint32_t seed = 7;
  std::string type = "all", archS = "plain", landS = "plains";
  bool night = false;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint32_t)std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--type") && i + 1 < argc) type = argv[++i];
    else if (!std::strcmp(argv[i], "--arch") && i + 1 < argc) archS = argv[++i];
    else if (!std::strcmp(argv[i], "--land") && i + 1 < argc) landS = argv[++i];
    else if (!std::strcmp(argv[i], "--night")) night = true;
  }
  ew::Archetype arch = ew::Archetype::Plain;
  for (int a = 0; a < 8; a++) if (archS == archNames[a]) arch = (ew::Archetype)a;
  int land = 0;
  for (int l = 0; l < 4; l++) if (landS == landNames[l]) land = l;
  if (type == "village" || type == "all") town(dir, Spec{SiteType::Village, false, arch, land, seed}, night);
  if (type == "town" || type == "all") town(dir, Spec{SiteType::Town, false, arch, land, seed}, night);
  if (type == "city" || type == "all") town(dir, Spec{SiteType::City, false, arch, land, seed}, night);
  if (type == "capital" || type == "all") town(dir, Spec{SiteType::City, true, arch, land, seed}, night);
  if (type == "all") heraldry(dir);
  return 0;
}
