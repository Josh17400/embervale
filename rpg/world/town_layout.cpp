// Settlement layout (TOWNS lane, VISION_PLAN 15.8): the land under the town, its heart, the roads it grows along,
// squares, main streets, ring roads and lanes, the city wall with its gatehouses and side gates, the lanes that join every
// opening to the streets and the approach roads out to the country. See rpg/world/town_gen.h.
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <queue>
#include "rpg/world/town_gen.h"

namespace ew {
namespace town {

namespace {
const int D4X[4] = {0, 1, -1, 0}, D4Y[4] = {1, 0, 0, -1};
int ifloor(float v) { return (int)std::floor(v); }
int iround(float v) { return (int)std::floor(v + 0.5f); }
}  // namespace

Gen::Gen(const SettlementCtx& c, SettlementOut& o)
    : C(c), P(*c.plan), O(o), M(o.buf), rng(mix64((uint64_t)c.plan->seed * 0x9E3779B97F4A7C15ull ^ 0x70A15E77C1D5ull)) {
  city = P.type == SiteType::City;
  town = P.type == SiteType::Town;
  village = !city && !town;
  capital = city && (P.flags & SPF_CAPITAL) != 0;
  arch = P.archetype;
  walled = city || (town && arch == Archetype::HillFort);
  palisade = village && arch == Archetype::HillFort;
  W = P.w + 2 * MARGIN;
  H = P.h + 2 * MARGIN;
  bseed = hash32(P.seed ^ 0xB10B5EEDu);
  wseed = hash32(P.seed ^ 0x3A11CA5Eu);
}

float Gen::dist(int x, int y) const {
  float dx = (x - cx) / rx, dy = (y - cy) / ry;
  return std::sqrt(dx * dx + dy * dy);
}
float Gen::angleOf(int x, int y) const { return datan2((float)(y - cy), (float)(x - cx)); }
float Gen::blob(int x, int y, int bx, int by, float brx, float bry, uint32_t s) const {
  float dx = (x - bx) / brx, dy = (y - by) / bry;
  float a = datan2(dy, dx);
  float wob = (vnoise(dcos(a) * 1.7f + 5, dsin(a) * 1.7f + 5, s) - 0.5f) * 0.32f;
  return std::sqrt(dx * dx + dy * dy) - wob;
}
District Gen::districtAt(int x, int y) const {
  if (!city || dist(x, y) < 0.30f) return District::Centre;
  float rel = dwrap(angleOf(x, y) - sector0) + D_PI / 4;   // sector 0 spans sector0 -+ pi/4
  if (rel < 0) rel += D_TAU;
  int k = (int)(rel / (D_PI / 2));
  return sectorKind[std::clamp(k, 0, 3)];
}

// ------------------------------------------------------------------------------------------------ the land
void Gen::land() {
  const size_t n = (size_t)W * H;
  M = Map();
  M.kind = MapKind::Overworld;
  M.alloc(W, H, Ground::Grass);
  M.biome.assign(n, (uint8_t)Biome::Plains);
  M.height.assign(n, 0);
  M.seed = P.seed;
  mask.assign(n, 0); water.assign(n, 0); lvl.assign(n, 0); inside.assign(n, 0);
  noBuild.assign(n, 0); cover.assign(n, 0); front.assign(n, 0);
  O.used.assign(n, 0);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      Ground g = Ground::Grass;
      Biome b = Biome::Plains;
      uint8_t h = 0;
      if (C.base) C.base(O.gx + x, O.gy + y, g, b, h);
      M.setG(x, y, g);
      M.biome[I(x, y)] = (uint8_t)b;
      lvl[I(x, y)] = (uint8_t)(h & Map::HEIGHT_LEVEL);
      water[I(x, y)] = groundWater(g) ? 1 : 0;
    }
  layout = village ? rng.irange(3) : 0;
  rx = P.w * 0.5f;
  ry = P.h * 0.5f;
  // the heart: near the middle, on dry level ground (a river may run through the footprint, never through the square)
  cx = W / 2 + rng.irange(3) - 1;
  cy = H / 2 + rng.irange(3) - 1;
  auto dryAround = [&](int x, int y, int r) {
    if (!in(x - r - 1, y - r) || !in(x + r + 1, y + r)) return false;
    for (int oy = -r; oy <= r; oy++)
      for (int ox = -r - 1; ox <= r + 1; ox++) {
        Ground g = M.at(x + ox, y + oy);
        if (groundWater(g) || g == Ground::Rock || lvl[I(x + ox, y + oy)] != lvl[I(x, y)]) return false;
      }
    return true;
  };
  const int need = city ? 5 : (town ? 4 : 3);
  if (!dryAround(cx, cy, need)) {
    bool found = false;
    for (int r = 1; r < std::min(W, H) / 3 && !found; r++)
      for (int oy = -r; oy <= r && !found; oy++)
        for (int ox = -r; ox <= r && !found; ox++) {
          if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
          if (dryAround(cx + ox, cy + oy, need)) { cx += ox; cy += oy; found = true; }
        }
  }
  // the town's biome and its ground
  bio = M.biomeAt(cx, cy);
  if (bio == Biome::Ocean || bio == Biome::Mountain) bio = Biome::Plains;
  switch (bio) {
    case Biome::Snow: base = Ground::Snow; break;
    case Biome::Taiga: base = Ground::Tundra; break;
    case Biome::Desert: case Biome::Beach: base = Ground::Sand; break;
    default: base = Ground::Grass; break;
  }
  mainG = village ? Ground::Dirt : Ground::Road;
  laneG = city ? Ground::Road : Ground::Dirt;
  // clear the outline: rock, swamp and the forest floor give way to open ground (rivers and lakes stay)
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      float b = blob(x, y, cx, cy, rx + 1.5f, ry + 1.5f, bseed);
      if (b > 1.0f) continue;
      Ground g = M.at(x, y);
      if (g == Ground::Rock || g == Ground::Swamp) M.setG(x, y, base);
      else if ((g == Ground::ForestFloor || g == Ground::Autumn || g == Ground::Tundra) && b < 0.85f) M.setG(x, y, base);
    }
  // the wall ring's inside (cities and hill forts): the town's outline at wallR
  if (walled) {
    wallR = city ? 0.93f : 0.90f;
    for (int y = 1; y < H - 1; y++)
      for (int x = 1; x < W - 1; x++) inside[I(x, y)] = blob(x, y, cx, cy, rx, ry, wseed) < wallR ? 1 : 0;
  }
}

// ------------------------------------------------------------------------------------------------ roads and districts
void Gen::pickBearings() {
  std::vector<float> b;
  for (float a0 : C.roadBearings) {
    float a = dwrap(a0);
    bool dup = false;
    for (float o : b) if (std::fabs(dwrap(a - o)) < 0.45f) dup = true;
    if (!dup) b.push_back(a);
  }
  const size_t maxN = village ? 3 : (town ? 4 : 6);
  if (b.size() > maxN) b.resize(maxN);
  if (b.empty()) {
    float a0 = rng.f() * D_TAU - D_PI;
    int n = village ? (layout == 1 ? 2 : (layout == 0 ? 4 : 3)) : (town ? 3 + rng.irange(2) : 4);
    for (int k = 0; k < n; k++) b.push_back(dwrap(a0 + k * D_TAU / n + rng.range(-0.3f, 0.3f)));
  } else if (b.size() == 1) {
    b.push_back(dwrap(b[0] + D_PI + rng.range(-0.35f, 0.35f)));   // a through road: the town grows along it
  }
  // cities open to at least three roads: more gates in the widest gaps
  while (city && b.size() < 3) {
    std::vector<float> s = b;
    std::sort(s.begin(), s.end());
    float best = -1, mid = 0;
    for (size_t i = 0; i < s.size(); i++) {
      float a = s[i], c = i + 1 < s.size() ? s[i + 1] : s[0] + D_TAU;
      if (c - a > best) { best = c - a; mid = a + (c - a) * 0.5f; }
    }
    b.push_back(dwrap(mid));
  }
  bearings = b;
}

void Gen::pickDistricts() {
  // the noble sector: the middle of the widest gap between the roads, leaning north (the palace and the keep face south
  // over the town)
  std::vector<float> s = bearings;
  std::sort(s.begin(), s.end());
  float best = -1e9f;
  sector0 = -D_PI / 2;
  for (size_t i = 0; i < s.size(); i++) {
    float a = s[i], c = i + 1 < s.size() ? s[i + 1] : s[0] + D_TAU;
    float mid = dwrap(a + (c - a) * 0.5f);
    float score = (c - a) - 0.9f * std::fabs(dwrap(mid + D_PI / 2));
    if (score > best) { best = score; sector0 = mid; }
  }
  sectorKind[0] = District::Noble;
  sectorKind[2] = District::Crafts;
  bool swap = rng.f() < 0.5f;
  sectorKind[1] = swap ? District::Poor : District::Temple;
  sectorKind[3] = swap ? District::Temple : District::Poor;
}

// ------------------------------------------------------------------------------------------------ squares and streets
void Gen::paintSquare(int x0, int y0, float r, District d) {
  const uint32_t s = bseed + 5u + (uint32_t)squares.size() * 31u;
  int n = 0;
  for (int y = y0 - (int)r - 2; y <= y0 + (int)r + 2; y++)
    for (int x = x0 - (int)(r * 1.25f) - 2; x <= x0 + (int)(r * 1.25f) + 2; x++) {
      if (!in(x, y) || inCompound(x, y, 2) || wet(x, y)) continue;
      if (blob(x, y, x0, y0, r * 1.25f, r, s) > 1.0f) continue;
      if (walled && !ins(x, y)) continue;
      set(x, y, K_SQUARE);
      if (village) M.setG(x, y, layout == 1 ? Ground::Dirt : (base == Ground::Grass ? Ground::Meadow : base));
      else M.setG(x, y, Ground::Plaza);
      M.setP(x, y, 0);
      n++;
    }
  if (n) squares.push_back(Square{x0, y0, r, d});
}

void Gen::squaresPass() {
  float sqR = city ? 6.0f : (town ? 4.2f : 2.8f);
  if (arch == Archetype::Market) sqR *= 1.3f;
  paintSquare(cx, cy, sqR, District::Centre);
  if (city) {
    for (int k = 0; k < 4; k++) {
      float a = sector0 + k * (D_PI / 2) + rng.range(-0.25f, 0.25f);
      float d = 0.56f + rng.range(-0.06f, 0.06f);
      int x = cx + iround(dcos(a) * rx * d), y = cy + iround(dsin(a) * ry * d);
      if (inCompound(x, y, 6) || wet(x, y)) continue;
      paintSquare(x, y, 3.0f + rng.f(), sectorKind[k]);
    }
  } else if (town && rng.f() < 0.65f) {
    float a = rng.f() * D_TAU;
    int x = cx + iround(dcos(a) * rx * 0.5f), y = cy + iround(dsin(a) * ry * 0.5f);
    if (!wet(x, y)) paintSquare(x, y, 2.6f, District::Centre);
  }
}

void Gen::paintStreet(int x, int y, uint8_t kind, Ground g) {
  if (!in(x, y) || inCompound(x, y)) return;
  uint8_t cur = get(x, y);
  if (cur == K_SQUARE || cur == K_FIELD) return;
  // a lane crossing a main street leaves its paving alone (no dirt patches across the road)
  const bool upgrade = cur == K_NONE || cur == K_YARD || (cur == K_LANE && kind == K_MAIN);
  if (upgrade) set(x, y, kind);
  Ground was = M.at(x, y);
  if (groundWater(was)) M.setG(x, y, Ground::Bridge);
  else if (upgrade && was != Ground::Plaza && was != Ground::Bridge) M.setG(x, y, g);
  M.setP(x, y, 0);
  allStreet.push_back({x, y});
  if (kind == K_MAIN) mainTiles.push_back({x, y});
}

void Gen::line(float x0, float y0, float x1, float y1, int width, uint8_t kind, Ground g) {
  float dx = x1 - x0, dy = y1 - y0;
  int steps = (int)std::ceil(std::max(std::fabs(dx), std::fabs(dy)) * 2) + 1;
  int px = ifloor(x0), py = ifloor(y0);
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / steps;
    int ix = ifloor(x0 + dx * t), iy = ifloor(y0 + dy * t);
    if (ix != px && iy != py) for (int o = 0; o < width; o++) paintStreet(ix + o, py, kind, g);
    for (int oy = 0; oy < width; oy++) for (int ox = 0; ox < width; ox++) paintStreet(ix + ox, iy + oy, kind, g);
    px = ix; py = iy;
  }
}

// A street that wanders: its heading drifts smoothly and is pulled back toward `target` (the road it follows), diagonal
// steps are filled so it stays 4-connected. stop: 0 the buffer's edge, 1 the town's edge, 2 a lane (also ends where it
// meets another street), 3 an approach road (ends at the buffer's edge, never runs back inside the wall).
bool Gen::walkStreet(float x, float y, float ang, float target, int maxLen, int width, uint8_t kind, Ground g, int stop) {
  float turn = 0;
  int px = ifloor(x), py = ifloor(y);
  const size_t own0 = allStreet.size(), main0 = mainTiles.size();
  struct Was { int x, y; uint8_t k; uint8_t g; };
  std::vector<Was> painted;   // what a lane changed, so a dead end can be taken back
  bool joined = false;
  auto mine = [&](int tx, int ty) {
    for (size_t k = own0; k < allStreet.size(); k++) if (allStreet[k].first == tx && allStreet[k].second == ty) return true;
    return false;
  };
  for (int i = 0; i < maxLen; i++) {
    float err = dwrap(target - ang);
    turn = turn * 0.82f + rng.range(-0.07f, 0.07f) + err * 0.05f;
    turn = std::clamp(turn, -0.3f, 0.3f);
    float na = ang + turn;
    int ix = ifloor(x + dcos(na)), iy = ifloor(y + dsin(na));
    if (inCompound(ix, iy, 2) || inCompound(ix + width - 1, iy + width - 1, 2)) {   // round the palace compound
      bool ok = false;
      for (float d : {0.6f, -0.6f, 1.2f, -1.2f, 1.6f, -1.6f}) {
        float a2 = ang + d;
        int jx = ifloor(x + dcos(a2)), jy = ifloor(y + dsin(a2));
        if (!inCompound(jx, jy, 2) && !inCompound(jx + width - 1, jy + width - 1, 2)) { na = a2; ix = jx; iy = jy; ok = true; break; }
      }
      if (!ok) break;
      turn = 0;
    }
    ang = na;
    x += dcos(ang);
    y += dsin(ang);
    if (!in(ix, iy) || !in(ix + width - 1, iy + width - 1)) break;
    if (stop == 1 && (walled ? !ins(ix, iy) : dist(ix, iy) > 1.0f)) break;
    if (stop == 2) {
      if (walled && (!ins(ix - 2, iy) || !ins(ix + 2, iy) || !ins(ix, iy - 2) || !ins(ix, iy + 2))) break;
      if (!walled && dist(ix, iy) > 0.95f) break;
    }
    if (stop == 3 && (wallAt(ix, iy) || ins(ix, iy))) break;
    auto paint = [&](int tx, int ty) {
      if (stop == 2 && in(tx, ty) && get(tx, ty) == K_NONE) painted.push_back(Was{tx, ty, K_NONE, M.ground[I(tx, ty)]});
      paintStreet(tx, ty, kind, g);
    };
    if (ix != px && iy != py) for (int o = 0; o < width; o++) paint(ix + o, py);
    for (int oy = 0; oy < width; oy++) for (int ox = 0; ox < width; ox++) paint(ix + ox, iy + oy);
    // a lane ends where it runs into another street
    if (stop == 2 && i > 2) {
      int ax = ix + iround(dcos(ang) * 2), ay = iy + iround(dsin(ang) * 2);
      if (isStreet(ax, ay) && !mine(ax, ay)) {
        paint(ix + iround(dcos(ang)), iy + iround(dsin(ang)));
        joined = true;
        break;
      }
    }
    px = ix; py = iy;
  }
  // most lanes that lead nowhere are taken back (a few cul-de-sacs stay: courts and closes)
  if (stop == 2 && !joined && rng.f() < 0.7f) {
    for (auto it = painted.rbegin(); it != painted.rend(); ++it) { set(it->x, it->y, it->k); M.ground[I(it->x, it->y)] = it->g; }
    allStreet.resize(own0);
    mainTiles.resize(main0);
  }
  return joined;
}

void Gen::mainStreets() {
  const int width = village ? 1 : 2;
  for (float a : bearings)
    walkStreet(cx + 0.5f, cy + 0.5f, a + rng.range(-0.2f, 0.2f), a, W + H, width, K_MAIN, mainG, 0);
}

void Gen::ringRoads() {
  if (village) return;
  auto ring = [&](float rr, float a0, float span, int width, uint8_t kind, Ground g, uint32_t s) {
    const float step = 0.6f / (std::max(rx, ry) * rr);
    float px = 0, py = 0;
    bool have = false;
    for (float t = 0; t <= span; t += step) {
      float a = a0 + t;
      float wob = (vnoise(dcos(a) * 1.4f + 7, dsin(a) * 1.4f + 7, s) - 0.5f) * 0.16f;
      float r = rr + wob;
      float x = cx + 0.5f + dcos(a) * rx * r, y = cy + 0.5f + dsin(a) * ry * r;
      if (have) line(px, py, x, y, width, kind, g);
      px = x; py = y; have = true;
    }
  };
  if (town) {
    ring(0.55f, rng.f() * D_TAU, 3.4f + rng.f() * 2.2f, 1, K_LANE, Ground::Road, bseed + 21);
  } else {
    ring(0.36f, rng.f() * D_TAU, D_TAU + 0.05f, 2, K_MAIN, Ground::Road, bseed + 22);
    ring(0.64f, rng.f() * D_TAU, D_TAU + 0.05f, 1, K_LANE, Ground::Road, bseed + 23);
    // the wall street: a lane following the inside of the city wall a few tiles in (where the wall's own outline is)
    const float inset = 4.5f / std::min(rx, ry);
    float px = 0, py = 0;
    bool have = false;
    const float step = 0.5f / std::max(rx, ry);
    for (float a = 0; a <= D_TAU + step; a += step) {
      float ca = dcos(a), sa = dsin(a);
      float r0 = 0.5f;
      for (float rr = 0.5f; rr < 1.2f; rr += 0.01f) {
        int x = cx + (int)std::floor(ca * rx * rr + 0.5f), y = cy + (int)std::floor(sa * ry * rr + 0.5f);
        if (blob(x, y, cx, cy, rx, ry, wseed) >= wallR - inset) break;
        r0 = rr;
      }
      float x = cx + 0.5f + ca * rx * r0, y = cy + 0.5f + sa * ry * r0;
      if (have) line(px, py, x, y, 1, K_LANE, Ground::Road);
      px = x; py = y; have = true;
    }
  }
  // radial streets between the rings: the spokes of the city's web (towns: from the square out past the ring)
  const int spokes = city ? 9 + rng.irange(3) : 4 + rng.irange(3);
  const float a0 = rng.f() * D_TAU;
  for (int k = 0; k < spokes; k++) {
    float a = a0 + k * D_TAU / spokes + rng.range(-0.15f, 0.15f);
    bool nearMain = false;
    for (float b : bearings) if (std::fabs(dwrap(a - b)) < 0.22f) nearMain = true;
    if (nearMain) continue;
    const float starts[2] = {city ? 0.36f : 0.12f, 0.64f};
    for (int j = 0; j < (city ? 2 : 1); j++) {
      // from the street on this ring (searched along the spoke) outward until it meets the next one
      float st = starts[j];
      int sx = -1, sy = -1;
      for (float rr = st - 0.06f; rr < st + 0.08f && sx < 0; rr += 0.01f) {
        int x = cx + (int)std::floor(dcos(a) * rx * rr + 0.5f), y = cy + (int)std::floor(dsin(a) * ry * rr + 0.5f);
        if (isStreet(x, y)) { sx = x; sy = y; }
      }
      if (sx < 0) { if (!city) { sx = cx; sy = cy; } else continue; }
      float ta = datan2(dsin(a) * ry, dcos(a) * rx);   // the spoke's direction in tiles
      walkStreet(sx + 0.5f + dcos(ta) * 2, sy + 0.5f + dsin(ta) * 2, ta, ta, (int)(std::max(rx, ry) * 0.6f), 1, K_LANE, city ? Ground::Road : laneG, 2);
    }
  }
}

// The street fabric: gently winding east-west lanes a house-row apart across the town (in the 3/4 view every door faces
// south, so a lane running east-west fronts a door every few tiles). The rings, the spokes and the main streets cross
// them, so the blocks come out irregular; the noble quarter keeps every other lane out (big plots and gardens), the
// poor quarter packs them closer. A lane never runs alongside another street, never over water (it stops at the bank),
// and in a walled city it ends at the wall street.
void Gen::fabric() {
  if (village) {
    // a village: a back lane or two east-west, north and south of the heart, each joined to the street it runs off
    const int n = 3;
    for (int k = 0; k < n; k++) {
      int y = cy + (k == 0 ? -(7 + rng.irange(3)) : (k == 1 ? 7 + rng.irange(3) : -(15 + rng.irange(3))));
      const float phase = rng.f() * D_TAU, amp = 0.6f + rng.f() * 1.2f;
      const float half = rx * (0.45f + rng.f() * 0.3f);
      const int x0 = cx - (int)(half * (0.6f + rng.f() * 0.4f)), x1 = cx + (int)(half * (0.6f + rng.f() * 0.4f));
      int px = -10, py = 0, mid = -1, midY = 0;
      for (int x = x0; x <= x1; x++) {
        int yy = y + iround(dsin(x * 0.11f + phase) * amp);
        bool ok = in(x, yy) && !water[I(x, yy)] && dist(x, yy) < 0.85f;
        if (ok && !isStreet(x, yy) && (isStreet(x, yy - 1) || isStreet(x, yy + 1))) ok = false;
        if (!ok) { px = -10; continue; }
        if (px == x - 1 && py != yy) for (int t = std::min(py, yy); t <= std::max(py, yy); t++) paintStreet(x, t, K_LANE, laneG);
        paintStreet(x, yy, K_LANE, laneG);
        if (std::abs(x - cx) < std::abs(mid - cx)) { mid = x; midY = yy; }
        px = x; py = yy;
      }
      // the link to the village street: straight toward the heart until it meets a street
      if (mid >= 0) {
        int dy = midY < cy ? 1 : -1;
        bool met = false;
        for (int yy = midY + dy; yy != cy && in(mid, yy) && !met; yy += dy) {
          if (isStreet(mid, yy) || isStreet(mid - 1, yy) || isStreet(mid + 1, yy)) met = true;
          if (water[I(mid, yy)]) break;
          paintStreet(mid, yy, K_LANE, laneG);
        }
      }
    }
    return;
  }
  const float inset = walled ? 4.5f / std::min(rx, ry) : 0.0f;
  const float edge = city ? 0.0f : 0.78f;
  int y = cy - (int)(ry * (walled ? wallR : edge)) + 3 + rng.irange(4);
  const int yEnd = cy + (int)(ry * (walled ? wallR : edge)) - 3;
  for (int band = 0; y < yEnd; band++) {
    const float phase = rng.f() * D_TAU, freq = 0.05f + rng.f() * 0.05f, amp = 0.8f + rng.f() * 1.6f;
    const uint32_t gs = bseed + 300u + (uint32_t)band * 17u;
    int px = -10, py = 0;
    for (int x = 1; x < W - 1; x++) {
      int yy = y + iround(dsin(x * freq + phase) * amp);
      bool ok = in(x, yy) && !inCompound(x, yy, 2) && !water[I(x, yy)];
      if (ok && walled) ok = blob(x, yy, cx, cy, rx, ry, wseed) < wallR - inset;
      if (ok && !walled) ok = blob(x, yy, cx, cy, rx, ry, bseed + 3) < edge;
      if (ok && city && districtAt(x, yy) == District::Noble && (band & 1)) ok = false;
      if (ok && town && hashf(x / 14, band, gs) < 0.15f) ok = false;   // towns: broken lines, closes and yards
      // never alongside another street (a double street), only across one
      if (ok && !isStreet(x, yy) && (isStreet(x, yy - 1) || isStreet(x, yy + 1))) ok = false;
      if (!ok) { px = -10; continue; }
      if (px == x - 1 && py != yy) for (int t = std::min(py, yy); t <= std::max(py, yy); t++) paintStreet(x, t, K_LANE, laneG);
      paintStreet(x, yy, K_LANE, laneG);
      px = x; py = yy;
    }
    // a row of houses and their roofs between two lanes: the street, the doorstep, the house, the roof rising over the
    // lane behind it (VISION_PLAN 15.7 / V5: no roof hides another front door), so 8 to 10 rows in a city
    y += city ? 8 + rng.irange(2) : 9 + rng.irange(3);
  }
}

void Gen::lanes() {
  const int n = village ? 5 + rng.irange(3) : (town ? 14 : 40);
  for (int k = 0; k < n && !allStreet.empty(); k++) {
    auto s = allStreet[(size_t)rng.irange((int)allStreet.size())];
    if (M.at(s.first, s.second) == Ground::Bridge) continue;
    if (!walled && dist(s.first, s.second) > 0.85f) continue;
    if (walled && !ins(s.first, s.second)) continue;
    // lanes run east-west more often than not: houses face south onto them, so they front the most doors
    float a;
    if (rng.f() < 0.6f) a = rng.f() < 0.5f ? 0.0f : D_PI;
    else {
      float radial = datan2((float)(s.second - cy), (float)(s.first - cx));
      a = rng.f() < 0.5f ? radial + (rng.f() < 0.5f ? D_PI / 2 : -D_PI / 2) : (rng.f() < 0.7f ? radial : radial + D_PI);
    }
    a += rng.range(-0.25f, 0.25f);
    int len = 8 + rng.irange(city ? 18 : 12);
    walkStreet(s.first + 0.5f, s.second + 0.5f, a, a, len, 1, K_LANE, laneG, 2);
  }
}

// ------------------------------------------------------------------------------------------------ city wall
// The ring of inside tiles with an outside 8-neighbour (a closed band the wall art bevels into curves), then one proper
// opening where each main street crosses it: a gatehouse on a horizontal run (preferred), a plain 3-tile opening flanked
// by the wall's end towers on a vertical run, a breach where the run is stepped. Then one or two side gates on long
// runs far from the main gates (a lane joins them to the streets, a track leads out to the fields). Rivers keep
// flowing under the wall (the view draws a culvert arch there).
void Gen::cityWall() {
  const size_t gaps0 = O.wallGaps.size();
  auto setWall = [&](int x, int y) {
    if (!in(x, y)) return;
    M.wall[I(x, y)] = 1;
    M.setP(x, y, 0);
    if (water[I(x, y)] && groundWater(M.at(x, y))) return;   // a culvert: the river runs on under the wall
    if (groundWater(M.at(x, y)) || groundSolid(M.at(x, y)) || M.at(x, y) == Ground::Bridge) M.setG(x, y, base);
  };
  auto clearWall = [&](int x, int y) {
    if (!in(x, y)) return;
    if (M.wall[I(x, y)]) { M.wall[I(x, y)] = 0; M.setG(x, y, mainG); }
    M.setP(x, y, 0);
    if (groundSolid(M.at(x, y))) M.setG(x, y, mainG);
  };
  std::vector<std::pair<int, int>> ring;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!ins(x, y)) continue;
      bool edge = false;
      for (int oy = -1; oy <= 1 && !edge; oy++) for (int ox = -1; ox <= 1; ox++) if (!ins(x + ox, y + oy)) { edge = true; break; }
      if (!edge) continue;
      setWall(x, y);
      ring.push_back({x, y});
    }
  // main-street crossings, clustered
  std::vector<uint8_t> seen((size_t)W * H, 0);
  struct Cross { float mx, my; int n; };
  std::vector<Cross> crosses;
  for (auto& rt : ring) {
    int x = rt.first, y = rt.second;
    if (get(x, y) != K_MAIN || seen[I(x, y)]) continue;
    Cross c{0, 0, 0};
    std::vector<std::pair<int, int>> q{{x, y}};
    seen[I(x, y)] = 1;
    for (size_t qi = 0; qi < q.size(); qi++) {
      c.mx += q[qi].first; c.my += q[qi].second; c.n++;
      for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) {
          int nx = q[qi].first + ox, ny = q[qi].second + oy;
          if (!in(nx, ny) || !wallAt(nx, ny) || get(nx, ny) != K_MAIN || seen[I(nx, ny)]) continue;
          seen[I(nx, ny)] = 1;
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
  auto wetNear = [&](int x, int y) {
    for (int oy = -3; oy <= 3; oy++)
      for (int ox = -3; ox <= 3; ox++)
        if (in(x + ox, y + oy) && (water[I(x + ox, y + oy)] || M.at(x + ox, y + oy) == Ground::Bridge)) return true;
    return false;
  };
  // a straight run of five ring tiles centred on (x, y), nothing beside its middle three
  auto straightH = [&](int x, int y) {
    if (wetNear(x, y)) return false;
    for (int k = -2; k <= 2; k++) if (!wallAt(x + k, y)) return false;
    for (int k = -1; k <= 1; k++) if (wallAt(x + k, y - 1) || wallAt(x + k, y + 1)) return false;
    return true;
  };
  auto straightV = [&](int x, int y) {
    if (wetNear(x, y)) return false;
    for (int k = -2; k <= 2; k++) if (!wallAt(x, y + k)) return false;
    for (int k = -1; k <= 1; k++) if (wallAt(x - 1, y + k) || wallAt(x + 1, y + k)) return false;
    return true;
  };
  auto gap = [&](IRect g, int bearing) {
    for (int y = g.y - 1; y <= g.y + g.h; y++)
      for (int x = g.x - 1; x <= g.x + g.w; x++) {
        bool inG = x >= g.x && x < g.x + g.w && y >= g.y && y < g.y + g.h;
        bool approach = (g.h == 1 && x >= g.x && x < g.x + g.w) || (g.w == 1 && y >= g.y && y < g.y + g.h) || (g.w == 3 && g.h == 3);
        if (!inG && !approach) continue;
        if (inG) clearWall(x, y);
        if (!in(x, y) || M.wall[I(x, y)]) continue;
        M.setP(x, y, 0);
        if (groundSolid(M.at(x, y))) M.setG(x, y, mainG);
        else if (M.at(x, y) != Ground::Bridge && M.at(x, y) != Ground::Plaza) M.setG(x, y, mainG);
        if (get(x, y) != K_SQUARE) set(x, y, bearing >= 0 ? K_MAIN : K_LANE);
      }
    O.wallGaps.push_back(g);
    gateBearing.push_back(bearing);
    made.push_back(g);
  };
  auto bearingOf = [&](float mx, float my) {
    float a = datan2(my - cy, mx - cx);
    int best = -1;
    float bd = 1e9f;
    for (int k = 0; k < (int)bearings.size(); k++) {
      float d = std::fabs(dwrap(a - bearings[(size_t)k]));
      if (d < bd) { bd = d; best = k; }
    }
    return best;
  };
  bool gated = false;
  for (const Cross& c : crosses) {
    int cxr = iround(c.mx), cyr = iround(c.my);
    if (nearMade(cxr, cyr, 4)) continue;
    int bx = 0, by = 0, bd = 1 << 30;
    bool bH = true;
    const int R = 11;
    for (int oy = -R; oy <= R; oy++)
      for (int ox = -R; ox <= R; ox++)
        for (int kind = 0; kind < 2; kind++) {
          const bool h = kind == 0;
          int x = cxr + ox, y = cyr + oy;
          if (!(h ? straightH(x, y) : straightV(x, y))) continue;
          if (nearMade(x, y, 4)) continue;
          int d = ox * ox + oy * oy + (h ? 0 : 30);
          if (d < bd) { bd = d; bx = x; by = y; bH = h; }
        }
    const int b = bearingOf(c.mx, c.my);
    if (bd < (1 << 30)) {
      if (bH) { gap(IRect{bx - 1, by, 3, 1}, b); O.gates.push_back({bx - 1, by}); gated = true; }
      else gap(IRect{bx, by - 1, 1, 3}, b);
    } else {
      gap(IRect{cxr - 1, cyr - 1, 3, 3}, b);   // a breach through a stepped run; its ends become towers
    }
  }
  if (!gated) {   // every city gets a gatehouse: on the straight top or bottom run nearest a street crossing
    int bx = 0, by = 0, bd = 1 << 30;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        if (!straightH(x, y) || nearMade(x, y, 2)) continue;
        int d = 1 << 29;
        for (const Cross& c : crosses) d = std::min(d, (int)((x - c.mx) * (x - c.mx) + (y - c.my) * (y - c.my)));
        if (crosses.empty()) d = std::abs(x - cx) + (H - y);
        if (d < bd) { bd = d; bx = x; by = y; }
      }
    if (bd < (1 << 30)) { gap(IRect{bx - 1, by, 3, 1}, bearingOf((float)bx, (float)by)); O.gates.push_back({bx - 1, by}); }
  }
  // every road gets its gate: a road whose street was pushed aside (by a river, by the palace compound) and crossed the
  // ring somewhere else gets an opening of its own on the ring within about 23 degrees of its bearing
  for (int b = 0; b < (int)bearings.size(); b++) {
    bool have = false;
    for (const IRect& g : made)
      if (std::fabs(dwrap(datan2(g.y + g.h * 0.5f - cy, g.x + g.w * 0.5f - cx) - bearings[(size_t)b])) < 0.40f) have = true;
    if (have) continue;
    int bx = 0, by = 0, bd = 1 << 30;
    bool bH = true;
    for (auto& rt : ring) {
      int x = rt.first, y = rt.second;
      float da = std::fabs(dwrap(datan2((float)(y - cy), (float)(x - cx)) - bearings[(size_t)b]));
      if (da > 0.40f || nearMade(x, y, 4)) continue;
      bool h = straightH(x, y), v = !h && straightV(x, y);
      if (!h && !v) continue;
      int d = (int)(da * 100.0f) + (h ? 0 : 15);
      if (d < bd) { bd = d; bx = x; by = y; bH = h; }
    }
    if (bd < (1 << 30)) {
      if (bH) { gap(IRect{bx - 1, by, 3, 1}, b); O.gates.push_back({bx - 1, by}); }
      else gap(IRect{bx, by - 1, 1, 3}, b);
      continue;
    }
    // no clean straight run there (a river at the foot of the wall, a stepped curve): a breach on the dry ring tile
    // nearest the road, its ends become towers
    float best = 1e9f;
    for (auto& rt : ring) {
      int x = rt.first, y = rt.second;
      float da = std::fabs(dwrap(datan2((float)(y - cy), (float)(x - cx)) - bearings[(size_t)b]));
      if (da > 0.45f || nearMade(x, y, 3) || water[I(x, y)]) continue;
      bool dry = true;
      for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if (in(x + ox, y + oy) && water[I(x + ox, y + oy)]) dry = false;
      if (!dry) continue;
      if (da < best) { best = da; bx = x; by = y; }
    }
    if (best < 1e8f) gap(IRect{bx - 1, by - 1, 3, 3}, b);
  }
  // side gates: on straight runs far from every opening, where a street runs near the inner side
  {
    int want = city ? (capital ? 2 : 1 + rng.irange(2)) : 1;
    for (int k = 0; k < want; k++) {
      int bx = 0, by = 0, bd = 1 << 30;
      bool bH = true;
      for (auto& rt : ring) {
        int x = rt.first, y = rt.second;
        bool h = straightH(x, y), v = !h && straightV(x, y);
        if (!h && !v) continue;
        if (nearMade(x, y, 4)) continue;
        float a = datan2((float)(y - cy), (float)(x - cx));
        bool far = true;
        for (const IRect& g : made) {
          float ga = datan2(g.y + g.h * 0.5f - cy, g.x + g.w * 0.5f - cx);
          if (std::fabs(dwrap(a - ga)) < 0.75f) far = false;
        }
        if (!far) continue;
        int d = 1 << 20;   // the nearest street on the inner side
        for (int oy = -7; oy <= 7; oy++)
          for (int ox = -7; ox <= 7; ox++)
            if (ins(x + ox, y + oy) && !wallAt(x + ox, y + oy) && isStreet(x + ox, y + oy)) d = std::min(d, ox * ox + oy * oy);
        d += (int)(hashAt(x, y, 61u) % 9u);
        if (d < bd) { bd = d; bx = x; by = y; bH = h; }
      }
      if (bd >= (1 << 20)) break;
      if (bH) gap(IRect{bx - 1, by, 3, 1}, -1);
      else gap(IRect{bx, by - 1, 1, 3}, -1);
    }
  }
  // tidy: drop orphans and spurs that are not jambs of an opening
  for (int pass = 0; pass < 12; pass++) {
    bool changed = false;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        if (!wallAt(x, y)) continue;
        int n = 0;
        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && wallAt(x + ox, y + oy)) n++;
        bool jamb = false;
        for (const IRect& r : made) {
          auto inR = [&](int px, int py) { return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h; };
          if (inR(x - 1, y) || inR(x + 1, y) || inR(x, y - 1) || inR(x, y + 1)) jamb = true;
        }
        if (n < 2 && !jamb) { M.wall[I(x, y)] = 0; changed = true; }
      }
    if (!changed) break;
  }
  linkGates(gaps0);
  approachRoads();
}

// Every opening joins the streets (a lane from its inner side to the nearest street), and the street stubs left outside
// the ring (where a street ran on past the wall with no opening) go back to the land, so no road runs up to a blank
// wall and every road out of town leaves through a gate.
void Gen::linkGates(size_t gaps0) {
  auto nearGap = [&](int x, int y, int d) {
    for (size_t k = gaps0; k < O.wallGaps.size(); k++) {
      const IRect& g = O.wallGaps[k];
      if (x >= g.x - d && x < g.x + g.w + d && y >= g.y - d && y < g.y + g.h + d) return true;
    }
    return false;
  };
  auto restore = [&](int x, int y) { M.setG(x, y, water[I(x, y)] ? Ground::Water : base); };
  bool cut = false;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint8_t k = get(x, y);
      if ((k != K_MAIN && k != K_LANE) || ins(x, y) || wallAt(x, y) || nearGap(x, y, 2)) continue;
      set(x, y, K_NONE);
      restore(x, y);
      cut = true;
    }
  // inside, a street that ran out through the ring away from an opening would dead-end against blank masonry
  std::vector<std::pair<int, int>> blocked;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      if ((get(x, y) == K_MAIN || get(x, y) == K_LANE) && wallAt(x, y) && !nearGap(x, y, 2)) blocked.push_back({x, y});
  for (auto& b : blocked) {
    set(b.first, b.second, K_NONE);
    for (int oy = -2; oy <= 2; oy++)
      for (int ox = -2; ox <= 2; ox++) {
        int x = b.first + ox, y = b.second + oy;
        uint8_t k = get(x, y);
        if ((k != K_MAIN && k != K_LANE) || !in(x, y) || wallAt(x, y) || nearGap(x, y, 2)) continue;
        set(x, y, K_NONE);
        restore(x, y);
        cut = true;
      }
  }
  // and a lane that wanders up to the wall and stops there: trim its dead end back from the masonry
  auto wallNear = [&](int x, int y) {
    for (int oy = -2; oy <= 2; oy++) for (int ox = -2; ox <= 2; ox++) if (wallAt(x + ox, y + oy)) return true;
    return false;
  };
  for (int pass = 0; pass < 5; pass++) {
    std::vector<std::pair<int, int>> ends;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        uint8_t k = get(x, y);
        if ((k != K_MAIN && k != K_LANE) || wallAt(x, y) || nearGap(x, y, 2) || !wallNear(x, y)) continue;
        int nb = 0;
        for (int d = 0; d < 4; d++) { uint8_t o = get(x + D4X[d], y + D4Y[d]); if (o == K_MAIN || o == K_LANE || o == K_SQUARE) nb++; }
        if (nb <= 1) ends.push_back({x, y});
      }
    if (ends.empty()) break;
    for (auto& t : ends) { set(t.first, t.second, K_NONE); restore(t.first, t.second); }
    cut = true;
  }
  if (cut) {
    auto gone = [&](const std::pair<int, int>& t) { uint8_t k = get(t.first, t.second); return k != K_MAIN && k != K_LANE && k != K_SQUARE; };
    mainTiles.erase(std::remove_if(mainTiles.begin(), mainTiles.end(), gone), mainTiles.end());
    allStreet.erase(std::remove_if(allStreet.begin(), allStreet.end(), gone), allStreet.end());
  }
  // a lane from each opening's inner side to the nearest street that is left
  for (size_t k = gaps0; k < O.wallGaps.size(); k++) {
    const IRect g = O.wallGaps[k];
    std::vector<int> prev((size_t)W * H, -2);
    std::queue<int> q;
    for (int y = g.y - 1; y <= g.y + g.h; y++)
      for (int x = g.x - 1; x <= g.x + g.w; x++)
        if (ins(x, y) && !wallAt(x, y) && prev[I(x, y)] == -2) { prev[I(x, y)] = -1; q.push((int)I(x, y)); }
    int found = -1;
    while (!q.empty() && found < 0) {
      int c = q.front(); q.pop();
      int x = c % W, y = c / W;
      if (isStreet(x, y) && !nearGap(x, y, 1)) { found = c; break; }
      for (int d = 0; d < 4; d++) {
        int nx = x + D4X[d], ny = y + D4Y[d];
        if (!ins(nx, ny) || prev[I(nx, ny)] != -2 || wallAt(nx, ny) || groundSolid(M.at(nx, ny)) || inCompound(nx, ny)) continue;
        prev[I(nx, ny)] = c;
        q.push((int)I(nx, ny));
      }
    }
    const bool mainGate = k < gateBearing.size() && gateBearing[k] >= 0;
    for (int c = found >= 0 ? prev[(size_t)found] : -1; c >= 0; c = prev[(size_t)c])
      paintStreet(c % W, c / W, mainGate ? K_MAIN : K_LANE, mainGate ? mainG : laneG);
  }
}

// from each opening out to the buffer's edge, turning onto the bearing of the road it serves (side gates: a short
// track out to the fields)
void Gen::approachRoads() {
  for (size_t k = 0; k < O.wallGaps.size(); k++) {
    const IRect g = O.wallGaps[k];
    const int b = k < gateBearing.size() ? gateBearing[k] : -1;
    if (b == -2) continue;   // the palace compound's gate
    int gx = g.x + g.w / 2, gy = g.y + g.h / 2;
    int dx = 0, dy = 0;
    if (g.h == 1) dy = ins(gx, gy - 1) ? 1 : -1;
    else if (g.w == 1) dx = ins(gx - 1, gy) ? 1 : -1;
    else {
      float a = datan2((float)(gy - cy), (float)(gx - cx));
      if (std::fabs(dcos(a)) > std::fabs(dsin(a))) dx = dcos(a) > 0 ? 1 : -1; else dy = dsin(a) > 0 ? 1 : -1;
    }
    const bool main = b >= 0;
    const int width = main ? 2 : 1;
    const Ground gr = main ? mainG : Ground::Dirt;
    // the passage through the opening and the first steps out, straight and as wide as the opening (main gates)
    int sx = gx, sy = gy;
    for (int s = 0; s <= 2; s++) {
      sx = gx + dx * s; sy = gy + dy * s;
      for (int t = main ? -1 : 0; t <= (main ? 1 : 0); t++) {
        int tx = sx + (dx ? 0 : t), ty = sy + (dy ? 0 : t);
        if (!wallAt(tx, ty)) paintStreet(tx, ty, main ? K_MAIN : K_LANE, gr);
      }
    }
    float a0 = datan2((float)dy, (float)dx);
    float target = main ? bearings[(size_t)b] : a0;
    walkStreet(sx + 0.5f, sy + 0.5f, a0, target, main ? W + H : 7 + rng.irange(6), width, main ? K_MAIN : K_LANE, gr, 3);
  }
}

// Streets cut off from the heart (a lane on the far bank of a river with no bridge, a stub behind a wall) go back to
// the land: everything the town builds must be reachable.
void Gen::pruneStreets() {
  std::vector<uint8_t> seen((size_t)W * H, 0);
  std::vector<int> q;
  for (int oy = -3; oy <= 3; oy++)
    for (int ox = -3; ox <= 3; ox++)
      if (isStreet(cx + ox, cy + oy) && !seen[I(cx + ox, cy + oy)]) { seen[I(cx + ox, cy + oy)] = 1; q.push_back((int)I(cx + ox, cy + oy)); }
  for (size_t h = 0; h < q.size(); h++) {
    int x = q[h] % W, y = q[h] / W;
    for (int d = 0; d < 4; d++) {
      int nx = x + D4X[d], ny = y + D4Y[d];
      if (!in(nx, ny) || seen[I(nx, ny)] || !isStreet(nx, ny) || wallAt(nx, ny)) continue;
      seen[I(nx, ny)] = 1;
      q.push_back((int)I(nx, ny));
    }
  }
  bool cut = false;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint8_t k = get(x, y);
      if ((k != K_MAIN && k != K_LANE) || seen[I(x, y)]) continue;
      // the stretch of a main road outside the town that runs on to the buffer's edge stays (the country road)
      if (k == K_MAIN && !walled) continue;
      set(x, y, K_NONE);
      if (!wallAt(x, y)) M.setG(x, y, water[I(x, y)] ? Ground::Water : base);
      cut = true;
    }
  if (cut) {
    auto gone = [&](const std::pair<int, int>& t) { uint8_t k = get(t.first, t.second); return k != K_MAIN && k != K_LANE && k != K_SQUARE; };
    mainTiles.erase(std::remove_if(mainTiles.begin(), mainTiles.end(), gone), mainTiles.end());
    allStreet.erase(std::remove_if(allStreet.begin(), allStreet.end(), gone), allStreet.end());
  }
}

// hill-fort villages: a fence ring round the houses, open where the streets leave
void Gen::palisadeRing() {
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      float b = blob(x, y, cx, cy, rx * 0.95f, ry * 0.95f, bseed + 3);
      if (b >= 1.0f) continue;
      bool edge = false;
      for (int oy = -1; oy <= 1 && !edge; oy++)
        for (int ox = -1; ox <= 1; ox++)
          if (blob(x + ox, y + oy, cx, cy, rx * 0.95f, ry * 0.95f, bseed + 3) >= 1.0f) { edge = true; break; }
      if (!edge || !freeTile(x, y) || cover[I(x, y)]) continue;
      bool nearStreet = false;
      for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if (isStreet(x + ox, y + oy) || get(x + ox, y + oy) == K_YARD) nearStreet = true;
      if (nearStreet) continue;
      float a = datan2((float)(y - cy) / ry, (float)(x - cx) / rx);
      M.setProp(x, y, std::fabs(dsin(a)) > 0.7f ? art::Prop::FenceH : art::Prop::FenceV);
      set(x, y, K_YARD);
    }
}

}  // namespace town
}  // namespace ew
