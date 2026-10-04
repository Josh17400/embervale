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

bool Map::blocked(int x, int y) const {
  if (!in(x, y)) return true;
  size_t i = (size_t)y * w + x;
  return groundSolid((Ground)ground[i]) || solid[i] || (!height.empty() && (height[i] & HEIGHT_CLIFF));
}

void Map::rebuildSolid() {
  std::fill(solid.begin(), solid.end(), 0);
  std::fill(bldgAt.begin(), bldgAt.end(), -1);
  for (size_t i = 0; i < prop.size(); i++)
    if (prop[i] && propSolid((Prop)(prop[i] - 1))) solid[i] = 1;
  for (size_t i = 0; i < wall.size(); i++) if (wall[i]) solid[i] = 1;
  for (int bi = 0; bi < (int)bldgs.size(); bi++) {
    const Bldg& b = bldgs[bi];
    // (an endless session keeps every building it has met: most lie far outside the window, so clip first)
    const int x0 = std::max(0, b.r.x), y0 = std::max(0, b.r.y), x1 = std::min(w, b.r.x + b.r.w), y1 = std::min(h, b.r.y + b.r.h);
    if (x0 >= x1 || y0 >= y1) continue;
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) { solid[(size_t)y * w + x] = 1; bldgAt[(size_t)y * w + x] = bi; }
    if (in(b.doorX(), b.doorY())) solid[(size_t)b.doorY() * w + b.doorX()] = 0;   // the door is walkable: stepping in enters
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
