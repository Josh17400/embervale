// World and Map at run time (the parts every system queries): tiles, solidity, site look-ups, zone levels. Split from
// world.cpp (the generators) in M1 so the SIM lane owns the runtime and the WORLD lane the generation.
#include <algorithm>
#include <cmath>
#include "rpg/sim/world.h"

using art::Prop;

void Map::alloc(int w_, int h_, Ground fill) {
  w = w_; h = h_;
  size_t n = (size_t)w * h;
  ground.assign(n, (uint8_t)fill);
  prop.assign(n, 0);
  solid.assign(n, 0);
  wall.assign(n, 0);
  deco.assign(n, 0);
  bldgAt.assign(n, -1);
  height.clear();
  bldgs.clear();
  spawns.clear();
}

// ------------------------------------------------------------------ open fronts (owner 2026-10-06)
namespace {
uint64_t bldgOpenKey(const Bldg& b) {
  uint64_t k = 0x0BE9F0u;
  auto mx = [&](uint64_t v) { k ^= v + 0x9E3779B97F4A7C15ull + (k << 6) + (k >> 2); k *= 0xBF58476D1CE4E5B9ull; k ^= k >> 31; };
  mx((uint64_t)b.type | (uint64_t)(uint16_t)b.r.w << 8 | (uint64_t)(uint16_t)b.r.h << 24 | (uint64_t)b.seed << 32);
  mx((uint64_t)b.wealth | (uint64_t)b.form << 8 | (uint64_t)b.civic << 16 | (uint64_t)b.seat << 24 | (uint64_t)b.urban << 32 |
     (uint64_t)b.storeys << 40 | (uint64_t)b.styled << 48 | (uint64_t)b.biome << 56);
  mx((uint64_t)b.arch.culture | (uint64_t)b.arch.variant << 8 | (uint64_t)b.arch.wall << 16 | (uint64_t)b.arch.roof << 24 | (uint64_t)b.roof << 32);
  return k | 1u;
}
void fillOpen(const Bldg& b, const bld::Blueprint& bp, uint64_t key) {
  Bldg::Open& o = b.open;
  o = Bldg::Open();
  o.key = key;
  const bld::OpenFront of = bld::openFront(bp);
  const int dcol = b.r.w / 2;
  o.open = of.open();
  o.raised = of.raised;
  o.mask = of.gaps | (1u << std::min(31, dcol));
  if (b.r.w < 32) o.mask &= (1u << b.r.w) - 1u;
  o.solid = of.solid;
  if (!o.open) o.solid[(size_t)std::min(31, dcol)] = 0;   // a door: the whole tile
}
}  // namespace

const Bldg::Open& bldgOpenFront(const Bldg& b) {
  const uint64_t key = bldgOpenKey(b);
  if (b.open.key != key) fillOpen(b, bldgBlueprint(b), key);
  return b.open;
}
const Bldg::Open& bldgOpenFront(const Bldg& b, const bld::Blueprint& bp) {
  const uint64_t key = bldgOpenKey(b);
  if (b.open.key != key) fillOpen(b, bp, key);
  return b.open;
}
bool bldgEntryAt(const Bldg& b, int x, int y) {
  if (y != b.doorY() || x < b.r.x || x >= b.r.x + b.r.w) return false;
  if (x == b.doorX()) return true;
  const int c = x - b.r.x;
  return c < 32 && ((bldgOpenFront(b).mask >> c) & 1u);
}
bool bldgPillarSolid(const Bldg& b, float px, float py) {
  if ((int)std::floor(py / TILE) != b.doorY()) return false;
  const Bldg::Open& o = bldgOpenFront(b);
  if (o.raised) return false;
  const int lx = (int)std::floor(px) - b.r.x * TILE;
  if (lx < 0 || lx >= std::min(32, b.r.w) * TILE) return false;
  return ((o.solid[(size_t)(lx / TILE)] >> (lx % TILE)) & 1u) != 0;
}
std::vector<int> bldgEntryColumns(const Bldg& b) {
  std::vector<int> v;
  const Bldg::Open& o = bldgOpenFront(b);
  for (int c = 0; c < std::min(32, b.r.w); c++) if (((o.mask >> c) & 1u) || c == b.r.w / 2) v.push_back(b.r.x + c);
  return v;
}

bool Map::blocked(int x, int y) const {
  if (!in(x, y)) return true;
  size_t i = (size_t)y * w + x;
  return groundSolid((Ground)ground[i]) || solid[i] || (!height.empty() && (height[i] & HEIGHT_CLIFF));
}

void Map::rebuildSolid() {
  std::fill(solid.begin(), solid.end(), 0);
  std::fill(bldgAt.begin(), bldgAt.end(), -1);
  for (size_t i = 0; i < prop.size(); i++)
    if (prop[i] && propSolid((Prop)(prop[i] - 1))) {
      solid[i] = 1;
      // (M1) a fountain's basin is wider than its tile (art: about 38 px across): its rim blocks the tiles either side
      if ((Prop)(prop[i] - 1) == Prop::Fountain) {
        const int x = (int)(i % (size_t)w);
        if (x > 0 && !groundSolid((Ground)ground[i - 1])) solid[i - 1] = 1;
        if (x + 1 < w && !groundSolid((Ground)ground[i + 1])) solid[i + 1] = 1;
        // ... and its spout and back rim rise into the tile above: walking up from the north stops short of the basin
        if (i >= (size_t)w && !groundSolid((Ground)ground[i - (size_t)w])) solid[i - (size_t)w] = 1;
      }
    }
  for (size_t i = 0; i < wall.size(); i++) if (wall[i]) solid[i] = 1;
  for (int bi = 0; bi < (int)bldgs.size(); bi++) {
    const Bldg& b = bldgs[bi];
    // (an endless session keeps every building it has met: most lie far outside the window, so clip first)
    const int x0 = std::max(0, b.r.x), y0 = std::max(0, b.r.y), x1 = std::min(w, b.r.x + b.r.w), y1 = std::min(h, b.r.y + b.r.h);
    if (x0 >= x1 || y0 >= y1) continue;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) { solid[(size_t)y * w + x] = 1; bldgAt[(size_t)y * w + x] = bi; }
    if (in(b.doorX(), b.doorY())) solid[(size_t)b.doorY() * w + b.doorX()] = 0;   // the door is walkable: stepping in enters
    // (owner) an open front: every walk-in bay of its front row too (its pillars block: Game::solidAt)
    if (kind == MapKind::Overworld && b.doorY() >= 0 && b.doorY() < h) {
      const Bldg::Open& o = bldgOpenFront(b);
      if (o.mask)
        for (int c = 0; c < std::min(32, b.r.w); c++)
          if (((o.mask >> c) & 1u) && in(b.r.x + c, b.doorY())) solid[(size_t)b.doorY() * w + b.r.x + c] = 0;
    }
  }
}

int World::siteAt(int tx, int ty, int pad) const {
  if (endless && pad <= NEAR_MARGIN / 2 && nearWindow(tx, ty) && tx >= -NEAR_MARGIN / 2 && ty >= -NEAR_MARGIN / 2 &&
      tx < WIN + NEAR_MARGIN / 2 && ty < WIN + NEAR_MARGIN / 2) {
    // the records near the window (ascending handles, so the first match is the same one the full scan finds)
    for (int i : nearSites) {
      const IRect& r = sites[(size_t)i].r;
      if (tx >= r.x - pad && ty >= r.y - pad && tx < r.x + r.w + pad && ty < r.y + r.h + pad) return i;
    }
    return -1;
  }
  for (int i = 0; i < (int)sites.size(); i++) {
    const IRect& r = sites[i].r;
    if (tx >= r.x - pad && ty >= r.y - pad && tx < r.x + r.w + pad && ty < r.y + r.h + pad) return i;
  }
  return -1;
}

int World::nearestSite(int tx, int ty, SiteType t, int exclude) const {
  int best = -1; int bd = 1 << 30;
  for (int i = 0; i < (int)sites.size(); i++) {
    if (sites[i].type != t || i == exclude) continue;
    int dx = sites[i].ex - tx, dy = sites[i].ey - ty;
    int d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

int World::zoneLevel(int tx, int ty) const {
  if (endless) return endlessDanger(tx, ty);
  if (sites.empty()) return 1;
  const Site& h = sites[startSite];
  float d = std::hypot((float)(tx - h.r.cx()), (float)(ty - h.r.cy()));
  int lv = 1 + (int)(d / 22.0f);
  if (over.biomeAt(tx, ty) == Biome::Snow || over.biomeAt(tx, ty) == Biome::Mountain) lv += 2;
  return std::min(lv, 30);
}
