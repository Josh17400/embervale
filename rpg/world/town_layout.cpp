// Settlement layout (TOWNS lane, VISION_PLAN 15.8): the land under the town, its heart, the roads it grows along,
// squares, main streets, ring roads and lanes, the city wall with its gatehouses and side gates, the lanes that join every
// opening to the streets and the approach roads out to the country. See rpg/world/town_gen.h.
#include <cstdio>
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
  // M3b: the society of the culture that builds it (its services, seat of power and spaces)
  tier = capital ? cult::SettleTier::Capital : (city ? cult::SettleTier::City : (town ? cult::SettleTier::Town : cult::SettleTier::Village));
  if (c.culture) {
    soc = cult::societyOf(*c.culture);
    hasSoc = true;
    seatKind = (int)soc.seat;
  }
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
  O.pools.clear();
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
  {
    // (M2 fixer) the biome most of the footprint lies in (the heart's own tile could be a mountain foot or an ecotone
    // sliver: a capital on a snowfield was built as a plains town, thatch and bare slate on white ground)
    int cnt[(int)Biome::COUNT] = {};
    for (int y = 0; y < H; y += 2)
      for (int x = 0; x < W; x += 2) {
        const float dx = (x - cx) / std::max(1.0f, rx), dy = (y - cy) / std::max(1.0f, ry);
        if (dx * dx + dy * dy > 1.0f) continue;
        const int b = M.biome[I(x, y)];
        if (b > 0 && b < (int)Biome::COUNT && b != (int)Biome::Mountain && b != (int)Biome::Beach) cnt[b]++;
      }
    int best = -1;
    for (int b = 1; b < (int)Biome::COUNT; b++)
      if (cnt[b] > 0 && (best < 0 || cnt[b] > cnt[best])) best = b;
    if (best >= 0) bio = (Biome)best;
    // a town on snow-covered ground (the view's snowline: taiga and snowfields from level 4, mountains from 5, or snow
    // itself) is a snow town: snow on every roof, its greenery wintered (finish, case 12 of Gen::step)
    int snowy = 0, all = 0;
    for (int y = 0; y < H; y += 2)
      for (int x = 0; x < W; x += 2) {
        const float dx = (x - cx) / std::max(1.0f, rx), dy = (y - cy) / std::max(1.0f, ry);
        if (dx * dx + dy * dy > 1.0f) continue;
        const Biome b = (Biome)M.biome[I(x, y)];
        const int lv = lvl[I(x, y)];
        all++;
        if (M.at(x, y) == Ground::Snow || ((b == Biome::Snow || b == Biome::Taiga) && lv >= 4) || (b == Biome::Mountain && lv >= 5)) snowy++;
      }
    if (all > 0 && snowy * 2 > all) bio = Biome::Snow;
  }
  if (bio == Biome::Ocean || bio == Biome::Mountain) bio = Biome::Plains;
  eco = ecoFamily(C.eco) == bio ? C.eco : ecoOfFamily(bio);   // (M3c)
  pickStyle();
  // (M3) a culture's village takes its street form from its colour (town_rules.h: its neighbours differ)
  if (village && C.culture) layout = townForm(townColour(C.culture, P.type, P.ex, P.ey, P.kind));
  pickSpecialty();
  // (M3 fixer) the dune folk build on sand: wherever their settlement stands, its ground is the desert's (a mud-brick
  // capital in a bright meadow among oaks did not read as theirs). The meadow gives way to sand inside the outline and
  // raggedly a little beyond it, so the town sits in its own patch of desert
  if (cArch == (int)cult::Archetype::Dune && bio != Biome::Snow) {
    bio = Biome::Desert;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        // (M3c fixer round 2) over the whole buffer: the land round the town is the dune folk's desert apron now
        // (chunkgen.cpp lays it about 12 tiles out), so the town's own outer ground (its gate approaches, its fields'
        // verges) is sand too, not islands of the old meadow by the gates
        const Ground g = M.at(x, y);
        if (g != Ground::Grass && g != Ground::Meadow && g != Ground::ForestFloor && g != Ground::Autumn) continue;
        M.setG(x, y, Ground::Sand);
        M.biome[I(x, y)] = (uint8_t)Biome::Desert;
      }
    // (M3c fixer round 2) and the trees the town plants are the desert's too (the eco was taken from the green land
    // before the sand was laid, so the streets and gardens of a dune capital grew oaks and autumn maples)
    eco = ecoFamily(C.eco) == Biome::Desert ? C.eco : ecoOfFamily(Biome::Desert);
  }
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
      if (g == Ground::Rock || (g == Ground::Swamp && !stilt)) M.setG(x, y, base);
      else if ((g == Ground::ForestFloor || g == Ground::Autumn || g == Ground::Tundra) && b < 0.85f) M.setG(x, y, base);
    }
  // the wall ring's inside (cities and hill forts): the town's outline at wallR (M3: the ring itself is rasterised in
  // wallRing once the roads are known, so its gates sit on straight runs)
  if (walled) {
    wallR = city ? 0.93f : 0.90f;
    for (int y = 1; y < H - 1; y++)
      for (int x = 1; x < W - 1; x++) inside[I(x, y)] = blob(x, y, cx, cy, rx, ry, wseed) < wallR ? 1 : 0;
  }
  if (terraced) cutTerraces();
  if (stilt) marshWater();
}

// ------------------------------------------------------------------------------------------------ M3: culture styles
// The culture's settlement (VISION_PLAN 5.6, 5.7): its layout (TownStyle::layout, or altLayout in about 1 in 4 of its
// settlements), wall, packing and greenery. Hashed from the plan's seed (no Rng draw), so a settlement without a
// culture is built exactly as before.
void Gen::pickStyle() {
  if (!C.culture) return;
  const cult::Culture& K = *C.culture;
  cArch = (int)K.archetype;
  style = townLayoutFor(&K, P.type, P.seed, P.ex, P.ey, P.kind);   // (town_rules.h: the repetition audit asks the same)
  townWealth = townWealthFor(&K, P.type, capital, P.seed, P.ex, P.ey, P.kind);
  wallByte = (uint8_t)(1 + std::min((int)K.town.wall, (int)art::CityWall::COUNT - 1));
  densityF = std::clamp(K.town.density / 128.0f, 0.35f, 1.9f);
  treesF = std::clamp(K.town.trees / 128.0f, 0.1f, 1.9f);
  centreKind = K.town.centre;
  const uint32_t h = hash32(P.seed ^ 0x6A1D5EEDu);
  switch (style) {
    case cult::Layout::Grid:
      wander = 0.3f;
      gridSX = (city ? 12 : 13) + (int)(h % 3u);
      gridSY = (city ? 8 : 9) + (int)((h >> 4) % 2u);
      gridTheta = ((int)((h >> 8) % 7u) - 3) * 0.025f;
      break;
    case cult::Layout::Compound:
      wander = 0.55f;
      gridSX = 17 + (int)(h % 3u);   // (a block holds a compound: 13 x 10 walls and an alley round it)
      gridSY = 12 + (int)((h >> 4) % 2u);
      gridTheta = ((int)((h >> 8) % 5u) - 2) * 0.03f;
      break;
    case cult::Layout::Linear: wander = 0.5f; break;
    case cult::Layout::Radial: wander = 0.7f; break;
    default: break;
  }
  terraced = style == cult::Layout::Terraced;
  // a marsh people's town on its causeway keeps the marsh round it too (stilt houses at its edges)
  stilt = style == cult::Layout::Stilt || (style == cult::Layout::Linear && cArch == (int)cult::Archetype::Marsh);
}

// Terraced (jade terraces): the town climbs a stepped hill. Its level rises in terraces from the outer band (the
// plan's flat level, so the town meets the land round it without a step) toward the back of the town, each terrace a
// band wide enough for a row of houses facing the valley; the heart's shelf is levelled for the square and the market.
// Contours wobble gently (two octaves, no 1-wide slivers). finish() writes the cliff faces and the stairs.
void Gen::cutTerraces() {
  const int L0 = lvl[I(cx, cy)];
  const int top = std::min(7, L0 + (city ? 3 : 2));
  if (top <= L0) { terraced = false; return; }
  const uint32_t ts = bseed ^ 0x7E44ACE5u;
  const float hx = cx + (hfAt(0, 0, 901u) - 0.5f) * rx * 0.4f, hy = cy - ry * (0.42f + hfAt(0, 1, 902u) * 0.12f);
  const float bands = city ? 4.2f : 3.4f;
  std::vector<uint8_t> nl = lvl;
  const float sqR = capital ? 8.2f : (city ? 7.0f : (town ? 5.4f : 3.2f));
  int heartK = -1;
  auto kAt = [&](int x, int y) {
    const float dx = (x - hx) / (rx * 1.05f), dy = (y - hy) / (ry * 1.1f);
    float t = 1.0f - std::sqrt(dx * dx + dy * dy);
    t += (vnoise(x * 0.045f, y * 0.045f, ts) - 0.5f) * 0.18f + (vnoise(x * 0.11f, y * 0.11f, ts + 1u) - 0.5f) * 0.05f;
    const float b = blob(x, y, cx, cy, rx, ry, bseed);
    t = std::min(t, (1.0f - b) * 2.0f);   // the outer band stays on the plan's level
    return std::clamp((int)std::floor(t * bands), 0, top - L0);
  };
  heartK = kAt(cx, cy);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (water[I(x, y)] || lvl[I(x, y)] != L0) continue;
      int k = kAt(x, y);
      // the heart's shelf: the square, its market and a margin are one level
      const float ddx = (x - cx) / (sqR * 1.25f + 9.0f), ddy = (y - cy) / (sqR + 8.0f);
      if (ddx * ddx + ddy * ddy < 1.0f) k = heartK;
      nl[I(x, y)] = (uint8_t)(L0 + k);
    }
  // no 1-wide slivers: a tile whose neighbours on both sides of an axis differ from it joins the higher side, a notch
  // or a finger joins its surroundings (as chunkgen's relief passes do)
  for (int pass = 0; pass < 4; pass++) {
    std::vector<uint8_t> t2 = nl;
    for (int y = 1; y < H - 1; y++)
      for (int x = 1; x < W - 1; x++) {
        const int l = nl[I(x, y)], n = nl[I(x, y - 1)], s = nl[I(x, y + 1)], w = nl[I(x - 1, y)], e = nl[I(x + 1, y)];
        int hi = 0, lo = 0, minHi = 99, maxLo = -1;
        for (int v : {n, s, w, e}) {
          if (v > l) { hi++; minHi = std::min(minHi, v); }
          if (v < l) { lo++; maxLo = std::max(maxLo, v); }
        }
        int to = l;
        if (hi >= 3) to = minHi;
        else if (lo >= 3) to = maxLo;
        else if ((n > l && s < l) || (n < l && s > l)) to = std::max(n, s);
        else if ((w > l && e < l) || (w < l && e > l)) to = std::max(w, e);
        else if (n > l && s > l) to = std::min(n, s);
        else if (w > l && e > l) to = std::min(w, e);
        else if (n < l && s < l) to = std::max(n, s);
        else if (w < l && e < l) to = std::max(w, e);
        t2[I(x, y)] = (uint8_t)to;
      }
    nl.swap(t2);
  }
  // a terrace never steps by two at once (one face, then the next terrace)
  for (int pass = 0; pass < 3; pass++)
    for (int y = 1; y < H - 1; y++)
      for (int x = 1; x < W - 1; x++) {
        int mx = 0;
        for (int d = 0; d < 4; d++) mx = std::max(mx, (int)nl[I(x + D4X[d], y + D4Y[d])]);
        if (mx > nl[I(x, y)] + 1) nl[I(x, y)] = (uint8_t)(mx - 1);
      }
  lvl.swap(nl);
}

// Stilt (marsh folk): the town keeps its marsh. Pools of shallow water and reed-swamp lie between the houses (the heart,
// its square and a margin stay dry ground; so does the land by a wall); houses stand on stilts over them and
// boardwalks (Ground::Bridge) carry the lanes across.
void Gen::marshWater() {
  const uint32_t ms = bseed ^ 0x3A25B0A7u;
  const float sqR = city ? 7.0f : (town ? 5.4f : 3.2f);
  const float lim = walled ? wallR - 0.16f : 0.95f;
  const float wetCut = style == cult::Layout::Stilt ? 0.56f : 0.62f;
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      const size_t i = I(x, y);
      if (water[i] || lvl[i] != lvl[I(cx, cy)]) continue;
      const float b = blob(x, y, cx, cy, rx, ry, walled ? wseed : bseed);
      if (b > lim) continue;
      const float ddx = (x - cx) / (sqR * 1.25f + 7.0f), ddy = (y - cy) / (sqR + 6.0f);
      if (ddx * ddx + ddy * ddy < 1.0f) continue;
      // pools and winding channels among reed beds (two octaves: no single lake)
      const float v = vnoise(x * 0.12f, y * 0.12f, ms) * 0.62f + vnoise(x * 0.29f, y * 0.29f, ms + 7u) * 0.38f;
      if (v > wetCut) M.setG(x, y, Ground::Water);
      else if (v > wetCut - 0.12f) M.setG(x, y, Ground::Swamp);
    }
}

// Linear: the spine's line. Along the water when there is some (the shore, the river): of the bearings in steps of 22.5
// degrees and the lines beside the heart (up to a third of the town away, so the spine runs along the bank, not through
// the river), the one that stays dry with the bank a few tiles off one side, nearest the heart. Without water, along
// the strongest road through the heart (east-west preferred: the doors face south onto it).
void Gen::pickSpine() {
  float best = -1e9f;
  spineA = 0;
  spineX = cx + 0.5f; spineY = cy + 0.5f;
  const float ext = std::max(rx, ry);
  bool anyWater = false;
  for (size_t i = 0; i < water.size() && !anyWater; i++) anyWater = water[i] != 0;
  const int maxOff = anyWater ? (int)(std::min(rx, ry) * 0.6f) : 0;
  for (int k = 0; k < 8; k++) {
    const float a = k * (D_PI / 8);
    const float ca = dcos(a), sa = dsin(a), nx = -sa, ny = ca;
    for (int o = -maxOff; o <= maxOff; o += 3) {
      const float ox = cx + 0.5f + nx * o, oy = cy + 0.5f + ny * o;
      float sc = 0;
      if (anyWater) {
        int wl = 0, wr = 0, dry = 0, bad = 0;
        for (float t = -ext; t <= ext; t += 2.0f) {
          const int px = ifloor(ox + ca * t), py = ifloor(oy + sa * t);
          if (!in(px, py) || dist(px, py) > 0.95f) continue;
          if (water[I(px, py)]) { bad++; continue; }
          dry++;
          // the bank: water 3 to 9 tiles off one side
          for (int q = 3; q <= 9; q += 2) {
            const int lx = ifloor(ox + ca * t + nx * q), ly = ifloor(oy + sa * t + ny * q);
            const int rx2 = ifloor(ox + ca * t - nx * q), ry2 = ifloor(oy + sa * t - ny * q);
            if (in(lx, ly) && water[I(lx, ly)]) wl++;
            if (in(rx2, ry2) && water[I(rx2, ry2)]) wr++;
          }
        }
        sc = (float)std::max(wl, wr) - 8.0f * bad + 0.3f * dry - 0.15f * std::abs(o);
      } else {
        sc = 0.3f * std::fabs(ca);
        if (!bearings.empty()) {
          float d = std::fabs(dwrap(a - bearings[0]));
          d = std::min(d, std::fabs(dwrap(a + D_PI - bearings[0])));
          sc -= d;
        }
      }
      if (sc > best) { best = sc; spineA = a; spineX = ox; spineY = oy; }
    }
  }
}

// a lattice or ring lane may run on (x, y): the town's ground (inside the wall street), dry, clear of the compound, and
// never alongside another street (a double street), only across one. (px, py): the lane's own last tile, which may
// touch it
bool Gen::laneOk(int x, int y, bool ns, int px, int py) const {
  if (!in(x, y) || x < 1 || y < 1 || x >= W - 1 || y >= H - 1) return false;
  if (inCompound(x, y, 2) || water[I(x, y)]) return false;
  if (walled) {
    if (blob(x, y, cx, cy, rx, ry, wseed) >= wallR - 4.5f / std::min(rx, ry)) return false;
    if (nearRing(x, y, 2)) return false;   // (the ring as rasterised: no lane along the wall's foot)
  } else if (blob(x, y, cx, cy, rx, ry, bseed + 3) >= (village ? 0.95f : 0.88f)) return false;
  if (terraced && groundSolid(M.at(x, y))) return false;
  if (isStreet(x, y)) return true;
  auto other = [&](int qx, int qy) { return !(qx == px && qy == py) && isStreet(qx, qy); };
  if (ns) { if (other(x - 1, y) || other(x + 1, y)) return false; }
  else if (other(x, y - 1) || other(x, y + 1)) return false;
  return true;
}

// Grid (imperial): a lattice of lanes, slightly tilted and warped, east-west a house-row apart and north-south a
// block apart; one stretch in seven or so is left out so the blocks come irregular (a bigger plot, a close, a garden),
// the noble quarter keeps every other east-west lane out. Compound (dune, highland): the same with big blocks for the
// courtyard compounds and narrow winding lanes.
void Gen::gridLanes(bool compoundStyle) {
  const uint32_t gs = bseed ^ (compoundStyle ? 0xC0A9B0u : 0x6A1D00u);
  const float amp = compoundStyle ? 0.9f : 0.45f;
  const float drop = compoundStyle ? 0.2f : 0.13f;
  const int SX = gridSX, SY = gridSY;
  // east-west lanes
  const int ky0 = -(int)(ry / SY) - 1, ky1 = (int)(ry / SY) + 1;
  for (int k = ky0; k <= ky1; k++) {
    const int y0 = cy + k * SY + (int)(hash2(k, 1, gs) % 3u) - 1 + (k == 0 ? (int)(ry > 30 ? 0 : 0) : 0);
    const float ph = hashf(k, 2, gs) * D_TAU;
    int px = -10, py = 0;
    for (int x = 1; x < W - 1; x++) {
      const int yy = y0 + iround(gridTheta * (x - cx) + dsin(x * 0.045f + ph) * amp);
      bool ok = laneOk(x, yy, false);
      const int seg = (x - cx + 4000) / SX;
      if (ok && hashf(seg, k + 77, gs) < drop && !isStreet(x, yy)) ok = false;
      if (ok && city && districtAt(x, yy) == District::Noble && (k & 1) && !isStreet(x, yy)) ok = false;
      if (!ok) { px = -10; continue; }
      if (px == x - 1 && py != yy) for (int t = std::min(py, yy); t <= std::max(py, yy); t++) paintStreet(x, t, K_LANE, laneG);
      paintStreet(x, yy, K_LANE, laneG);
      px = x; py = yy;
    }
  }
  // north-south lanes
  const int kx0 = -(int)(rx / SX) - 1, kx1 = (int)(rx / SX) + 1;
  for (int k = kx0; k <= kx1; k++) {
    const int x0 = cx + k * SX + SX / 2 + (int)(hash2(k, 3, gs) % 3u) - 1;
    const float ph = hashf(k, 4, gs) * D_TAU;
    int py = -10, pxx = 0;
    for (int y = 1; y < H - 1; y++) {
      const int xx = x0 + iround(-gridTheta * (y - cy) + dsin(y * 0.05f + ph) * amp);
      bool ok = laneOk(xx, y, true);
      const int seg = (y - cy + 4000) / SY;
      if (ok && hashf(k + 55, seg, gs) < drop && !isStreet(xx, y)) ok = false;
      if (!ok) { py = -10; continue; }
      if (py == y - 1 && pxx != xx) for (int t = std::min(pxx, xx); t <= std::max(pxx, xx); t++) paintStreet(t, y, K_LANE, laneG);
      paintStreet(xx, y, K_LANE, laneG);
      py = y; pxx = xx;
    }
  }
}

// Radial (steppe camps, sun temples, the spires): rings of lanes round the plaza, spokes straight out to the edge. The
// houses fill the rings; the chief's hall (or the temple) faces the plaza from the north (services).
void Gen::radialRings() {
  std::vector<float> radii;
  if (city) radii = {0.27f, 0.45f, 0.63f, 0.80f};
  else if (town) radii = {0.34f, 0.57f, 0.80f};
  else radii = {0.48f, 0.80f};
  for (size_t i = 0; i < radii.size(); i++) {
    const float rr = radii[i] + (hashf((int)i, 9, bseed) - 0.5f) * 0.04f;
    const uint32_t s = bseed + 40u + (uint32_t)i;
    const bool mainRing = city && i == 0;
    const float step = 0.6f / (std::max(rx, ry) * rr);
    float px = 0, py = 0;
    bool have = false;
    for (float a = 0; a <= D_TAU + step; a += step) {
      const float wob = (vnoise(dcos(a) * 1.3f + 7, dsin(a) * 1.3f + 7, s) - 0.5f) * 0.07f;
      const float x = cx + 0.5f + dcos(a) * rx * (rr + wob), y = cy + 0.5f + dsin(a) * ry * (rr + wob);
      const int ix = ifloor(x), iy = ifloor(y);
      const bool ok = in(ix, iy) && !water[I(ix, iy)] && (!walled || (blob(ix, iy, cx, cy, rx, ry, wseed) < wallR - 4.5f / std::min(rx, ry) && !nearRing(ix, iy, 2)));
      if (have && ok) line(px, py, x, y, mainRing ? 2 : 1, mainRing ? K_MAIN : K_LANE, mainRing ? Ground::Road : laneG);
      px = x; py = y; have = ok;
    }
  }
  // the spokes, straight from the plaza's edge to past the outer ring (none hard by a main street)
  const int spokes = city ? 12 : (town ? 8 : 6);
  const float a0 = hashf(1, 1, bseed + 61u) * D_TAU;
  const float r0 = (capital ? 8.2f : (city ? 7.0f : (town ? 5.4f : 3.2f))) * 1.2f + 1.0f;
  for (int k = 0; k < spokes; k++) {
    const float a = a0 + k * D_TAU / spokes;
    bool nearMain = false;
    for (float b : bearings) if (std::fabs(dwrap(a - b)) < 0.24f) nearMain = true;
    if (nearMain) continue;
    const float ca = dcos(a), sa = dsin(a);
    const float rEnd = radii.back() + 0.07f;
    const float x0 = cx + 0.5f + ca * r0, y0 = cy + 0.5f + sa * r0;
    float x1 = cx + 0.5f + ca * rx * rEnd, y1 = cy + 0.5f + sa * ry * rEnd;
    // stop short of water and the wall street
    const int n = (int)std::max(std::fabs(x1 - x0), std::fabs(y1 - y0)) + 1;
    float lx = x0, ly = y0;
    for (int t = 0; t <= n; t++) {
      const float fx = x0 + (x1 - x0) * t / n, fy = y0 + (y1 - y0) * t / n;
      const int ix = ifloor(fx), iy = ifloor(fy);
      if (!in(ix, iy) || water[I(ix, iy)] || inCompound(ix, iy, 2)) break;
      if (walled && (blob(ix, iy, cx, cy, rx, ry, wseed) >= wallR - 4.5f / std::min(rx, ry) || nearRing(ix, iy, 2))) break;
      lx = fx; ly = fy;
    }
    x1 = lx; y1 = ly;
    if (std::fabs(x1 - x0) + std::fabs(y1 - y0) < 3) continue;
    line(x0, y0, x1, y1, 1, K_LANE, laneG);
  }
}

// Linear: the spine along spineA (a main street, through the heart or along the bank beside it, joined to the heart),
// back lanes running beside it a house-row or two off on the land side (never out over the water), and short cross
// lanes joining them every block or so.
void Gen::spineStreets() {
  const int width = village ? 1 : 2;
  const float ext = std::max(rx, ry) * 1.2f;
  walkStreet(spineX, spineY, spineA, spineA, (int)ext, width, K_MAIN, mainG, 1);
  walkStreet(spineX, spineY, spineA + D_PI, dwrap(spineA + D_PI), (int)ext, width, K_MAIN, mainG, 1);
  // a spine along the bank is joined to the heart straight across
  if (std::fabs(spineX - (cx + 0.5f)) + std::fabs(spineY - (cy + 0.5f)) > 2.0f) line(cx + 0.5f, cy + 0.5f, spineX, spineY, width, K_MAIN, mainG);
  const float ca = dcos(spineA), sa = dsin(spineA), nx = -sa, ny = ca;
  const bool ns = std::fabs(sa) > std::fabs(ca);
  const int rowGap = village ? 7 : 9;
  // as many back lanes as the town is deep across the spine; they shorten away from it (the town thins out)
  const float across = std::fabs(nx) * rx + std::fabs(ny) * ry;
  const int rows = village ? 1 : std::max(2, (int)(across * 0.95f / rowGap));
  const uint32_t ls = bseed ^ 0x51A1E5u;
  for (int side : {-1, 1})
    for (int r = 1; r <= rows; r++) {
      const float off = (float)(side * r * rowGap) + (hashf(r, side + 5, ls) - 0.5f) * 2.0f;
      const float ph = hashf(r, side + 9, ls) * D_TAU;
      const float reach = ext * (0.95f - 0.5f * r / (rows + 1)) * (0.85f + hashf(r, side + 11, ls) * 0.3f);
      int px = -10000, py = 0;
      for (float t = -reach; t <= reach; t += 0.5f) {
        const float wob = dsin(t * 0.06f + ph) * 1.2f;
        const int x = ifloor(spineX + ca * t + nx * (off + wob)), y = ifloor(spineY + sa * t + ny * (off + wob));
        if (x == px && y == py) continue;
        const bool ok = laneOk(x, y, ns, px, py) && dist(x, y) < 0.95f;
        if (!ok) { px = -10000; continue; }
        if (px != -10000 && x != px && y != py) paintStreet(x, py, K_LANE, laneG);
        paintStreet(x, y, K_LANE, laneG);
        px = x; py = y;
      }
    }
  // cross lanes from the spine out through the back lanes, every block or so along it
  const int gap = village ? 11 : 13;
  for (int k = -(int)(ext / gap); k <= (int)(ext / gap); k++) {
    if (k == 0) continue;
    for (int side : {-1, 1}) {
      if (hashf(k, side, ls + 3u) < 0.25f) continue;
      const float t = k * gap + (hashf(k, side + 2, ls) - 0.5f) * 4.0f;
      const float x = spineX + ca * t, y = spineY + sa * t;
      const int sx = ifloor(x), sy = ifloor(y);
      if (!in(sx, sy) || !isStreet(sx, sy)) continue;
      int px = sx, py = sy;
      for (int st = 1; st <= rows * rowGap + 2; st++) {
        const int qx = ifloor(x + nx * side * st), qy = ifloor(y + ny * side * st);
        if (!in(qx, qy) || water[I(qx, qy)] || inCompound(qx, qy, 2) || dist(qx, qy) > 0.95f) break;
        if (walled && (blob(qx, qy, cx, cy, rx, ry, wseed) >= wallR - 4.5f / std::min(rx, ry) || nearRing(qx, qy, 2))) break;
        if (qx != px && qy != py) paintStreet(qx, py, K_LANE, laneG);
        paintStreet(qx, qy, K_LANE, laneG);
        px = qx; py = qy;
      }
    }
  }
}

// Terraced: a lane along each terrace's front edge (houses on the terrace face the valley over it), and stairs
// (short lanes straight down the face) joining each edge lane to the street below every block or so.
void Gen::contourLanes() {
  const int L0 = lvl[I(cx, cy)];
  int top = L0;
  for (uint8_t v : lvl) top = std::max(top, (int)v);
  const uint32_t cs = bseed ^ 0xC0A70Eu;
  for (int L = 0; L <= top; L++) {
    std::vector<int> bot((size_t)W, -1);
    for (int x = 1; x < W - 1; x++)
      for (int y = H - 3; y >= 1; y--) {
        if (lvl[I(x, y)] == L && lvl[I(x, y + 1)] < L) { bot[(size_t)x] = y; break; }
      }
    int px = -10, py = 0;
    for (int x = 1; x < W - 1; x++) {
      const int y = bot[(size_t)x];
      bool ok = y >= 0 && laneOk(x, y, false) && lvl[I(x, y)] == L;
      // the vertical join runs down the column whose terrace reaches that far, and stays on this terrace
      const int col = y > py ? x : px;
      if (ok && px == x - 1)
        for (int t = std::min(py, y); t <= std::max(py, y); t++) if (lvl[I(col, t)] != L || (inCompound(col, t, 2))) ok = false;
      if (!ok) { px = -10; continue; }
      if (px == x - 1 && py != y)
        for (int t = std::min(py, y); t <= std::max(py, y); t++) paintStreet(col, t, K_LANE, laneG);
      paintStreet(x, y, K_LANE, laneG);
      px = x; py = y;
    }
    // the stairs down from this edge
    const int gap = city ? 16 : 13;
    for (int x = 2 + (int)(hash2(L, 0, cs) % (uint32_t)gap); x < W - 2; x += gap + (int)(hash2(x, L, cs) % 4u)) {
      const int y = bot[(size_t)x];
      if (y < 0 || !isStreet(x, y) || lvl[I(x, y)] != L) continue;
      for (int s = 1; s <= 24; s++) {
        const int qy = y + s;
        if (!in(x, qy) || water[I(x, qy)] || inCompound(x, qy, 2) || dist(x, qy) > 0.95f) break;
        if (walled && (blob(x, qy, cx, cy, rx, ry, wseed) >= wallR - 4.5f / std::min(rx, ry) || nearRing(x, qy, 2))) break;
        const bool met = isStreet(x, qy);
        paintStreet(x, qy, K_LANE, laneG);
        if (met && s > 1) break;
      }
    }
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

// ------------------------------------------------------------------------------------------------ M3: the wall ring
// (M3, owner note 3: "diagonal city wall runs read as a jagged staircase") The ring is no longer the inside tiles that
// touch the outside (two tiles thick on a diagonal, 2-1-2 jitter on a slope): it is an octilinear polygon round the
// wall's outline, one tile thick: straight horizontal and vertical runs and clean 45-degree diagonals (one tile per
// row, every step 1:1), each part at least three tiles long, so the painter draws long straight walls and slanted
// ones. Where a road leaves north or south, the polygon holds a straight horizontal run of nine tiles for its
// gatehouse. inside = the tiles the ring encloses, and the ring. If the polygon came out wrong (a self-crossing on a
// strange outline), the old ring is kept.
void Gen::wallRing() {
  ringT.clear();
  // the outline at normalised angle a (blob(): the radius where the wobbly ellipse reaches wallR)
  auto rho = [&](float a) { return wallR + (vnoise(dcos(a) * 1.7f + 5, dsin(a) * 1.7f + 5, wseed) - 0.5f) * 0.32f; };
  auto ptAt = [&](float a) { const float r = rho(a); return std::make_pair(cx + dcos(a) * rx * r, cy + dsin(a) * ry * r); };
  struct V { float a; int x, y; bool fixed; };
  std::vector<V> vs;
  const int N = city ? 22 : 16;
  const float a0 = hashf(3, 3, wseed) * D_TAU;
  // the gate runs: a road leaving north or south crosses a straight horizontal run of 9 tiles
  std::vector<std::pair<float, float>> runs;   // (normalised angle, half width in radians)
  for (float b : bearings) {
    if (std::fabs(dsin(b)) < 0.55f) continue;
    const float an = datan2(dsin(b) / ry, dcos(b) / rx);
    const auto p = ptAt(an);
    const int px = iround(p.first), py = iround(p.second);
    const float hw = 5.5f / std::max(1.0f, std::fabs(dsin(an)) * rx * rho(an));
    bool clash = false;
    for (auto& r : runs) if (std::fabs(dwrap(r.first - an)) < r.second + hw + 0.15f) clash = true;
    if (clash) continue;
    runs.push_back({an, hw});
    const float aL = datan2((float)(py - cy) / ry, (float)(px - 4 - cx) / rx), aR = datan2((float)(py - cy) / ry, (float)(px + 4 - cx) / rx);
    vs.push_back(V{aL, px - 4, py, true});
    vs.push_back(V{aR, px + 4, py, true});
  }
  for (int k = 0; k < N; k++) {
    const float a = dwrap(a0 + k * D_TAU / N);
    bool nearRun = false;
    for (auto& r : runs) if (std::fabs(dwrap(a - r.first)) < r.second + 0.2f) nearRun = true;
    if (nearRun) continue;
    // each corner pushed in or out a little (a real curtain wall bends where its builders met the ground), so the
    // ring is an irregular polygon of straight and slanted runs, not a regular octagon
    const float j = 1.0f + (hashf(k, 7, wseed) - 0.5f) * 0.10f;
    const float r = rho(a) * j;
    vs.push_back(V{a, iround(cx + dcos(a) * rx * r), iround(cy + dsin(a) * ry * r), false});
  }
  if (vs.size() < 6) return;
  for (V& v : vs) {   // (the outline may reach past the buffer: the ring keeps three tiles in from its edge)
    v.a = dwrap(v.a);
    v.x = std::clamp(v.x, 3, W - 4);
    v.y = std::clamp(v.y, 3, H - 4);
  }
  std::stable_sort(vs.begin(), vs.end(), [](const V& p, const V& q) { return p.a < q.a; });
  // start at a fixed vertex when there is one (the closing edge, the only one allowed short parts, then ends there)
  size_t st = 0;
  for (size_t i = 0; i < vs.size(); i++) if (vs[i].fixed) { st = (i + 1) % vs.size(); break; }
  std::rotate(vs.begin(), vs.begin() + (long)st, vs.end());
  std::vector<std::pair<int, int>> path;
  int curX = vs[0].x, curY = vs[0].y;
  path.push_back({curX, curY});
  int lastDx = 0, lastDy = 0;
  auto stepN = [&](int sx, int sy, int n) {
    for (int k = 0; k < n; k++) { curX += sx; curY += sy; path.push_back({curX, curY}); }
    if (n > 0) { lastDx = sx; lastDy = sy; }
  };
  for (size_t i = 1; i <= vs.size(); i++) {
    const V& T = vs[i % vs.size()];
    const bool exact = T.fixed || i == vs.size();
    const int dx = T.x - curX, dy = T.y - curY;
    const int ax = std::abs(dx), ay = std::abs(dy), sx = dx > 0 ? 1 : (dx < 0 ? -1 : 0), sy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
    const int d = std::min(ax, ay), s = std::max(ax, ay) - d;
    const int mx = ax >= ay ? sx : 0, my = ax >= ay ? 0 : sy;   // the straight part's direction
    if (!exact && std::max(ax, ay) < 4) continue;                                  // a tiny edge: carried on whole
    if (!exact && d > 0 && s > 0 && s < 3) { stepN(sx, sy, d); continue; }        // short straight: carried on
    if (!exact && d > 0 && s > 0 && d < 3) { stepN(mx, my, s + d); continue; }    // short diagonal: carried on
    // straight then diagonal, or diagonal then straight (whichever continues the last part)
    const bool diagFirst = lastDx == sx && lastDy == sy && d > 0;
    if (diagFirst) { stepN(sx, sy, d); stepN(mx, my, s); }
    else { stepN(mx, my, s); stepN(sx, sy, d); }
  }
  auto bail = [&](const char* why) { if (std::getenv("EMB_DEBUG_WALLS")) std::printf("wallRing: fallback (%s)\n", why); };
  if (path.size() < 12 || path.back() != path.front()) { bail("open"); return; }
  path.pop_back();
  // a corner tile whose neighbours along the ring touch each other is cut (an L corner or a one-tile jog becomes a
  // clean diagonal step: no tile is buried in the ring, no double-thick corner)
  auto cutCorners = [&]() {
    for (bool cut = true; cut && path.size() > 12;) {
      cut = false;
      for (size_t i = 0; i < path.size() && path.size() > 12; i++) {
        const auto& p = path[(i + path.size() - 1) % path.size()];
        const auto& n = path[(i + 1) % path.size()];
        if (std::abs(p.first - n.first) <= 1 && std::abs(p.second - n.second) <= 1) {
          path.erase(path.begin() + (long)i);
          cut = true;
        }
      }
    }
  };
  cutCorners();
  // a straight piece of one or two tiles between slanted ones (the 2-1-2 jitter a slope leaves) moves along the ring
  // into the nearest longer straight run of the same direction: the steps are only reordered, so the ring still
  // closes; the stretch between shifts by a tile or two
  {
    const size_t n = path.size();
    std::vector<std::pair<int, int>> st(n);
    for (size_t i = 0; i < n; i++) st[i] = {path[(i + 1) % n].first - path[i].first, path[(i + 1) % n].second - path[i].second};
    auto straight = [](const std::pair<int, int>& d) { return (d.first == 0) != (d.second == 0); };
    for (int round = 0; round < 3; round++) {
      bool moved = false;
      for (size_t i = 0; i < n; i++) {
        if (!straight(st[i]) || st[(i + n - 1) % n] == st[i]) continue;   // the start of a straight run
        size_t L = 1;
        while (L < n && st[(i + L) % n] == st[i]) L++;
        if (L > 2) continue;
        // (a short run at a corner too: moved away, the corner meets its slant directly, and cutCorners bevels it)
        // the nearest run of the same direction (3+ long), either way round the ring. In the sequence rotated to start
        // at the short run, a run ahead starts `off` steps after it; a run behind ends `off` steps before it
        const std::pair<int, int> S = st[i];
        size_t fOff = 0, bOff = 0;
        bool fwd = false, bwd = false;
        for (size_t off = 0; off + L < n; off++) {
          if (st[(i + L + off) % n] != S) continue;
          size_t len = 0;
          while (len < n && st[(i + L + off + len) % n] == S) len++;
          if (len >= 3) { fOff = off; fwd = true; break; }
          off += len - 1;   // (another short run: look on past it)
        }
        for (size_t off = 0; off + L < n; off++) {
          if (st[(i + n - 1 - off) % n] != S) continue;
          size_t len = 0;
          while (len < n && st[(i + 2 * n - 1 - off - len) % n] == S) len++;
          if (len >= 3) { bOff = off; bwd = true; break; }
          off += len - 1;
        }
        if (!fwd && !bwd) continue;
        const bool ahead = fwd && (!bwd || fOff <= bOff);
        // take the L steps out at i and put them into the far run
        std::vector<std::pair<int, int>> seq(st.begin(), st.end());
        std::rotate(seq.begin(), seq.begin() + (long)i, seq.end());   // the short run is at 0..L-1
        std::vector<std::pair<int, int>> rest(seq.begin() + (long)L, seq.end());
        const size_t at = ahead ? fOff : rest.size() - bOff;
        rest.insert(rest.begin() + (long)at, seq.begin(), seq.begin() + (long)L);
        // rebuild the tiles from the original start tile of step i
        std::vector<std::pair<int, int>> np;
        int x = path[i].first, y = path[i].second;
        for (const auto& d : rest) { np.push_back({x, y}); x += d.first; y += d.second; }
        if (x != path[i].first || y != path[i].second) continue;
        path.swap(np);
        for (size_t q = 0; q < n; q++) st[q] = {path[(q + 1) % n].first - path[q].first, path[(q + 1) % n].second - path[q].second};
        moved = true;
      }
      if (!moved) break;
    }
  }
  cutCorners();
  // a simple closed curve inside the buffer
  std::vector<uint8_t> on((size_t)W * H, 0);
  for (auto& t : path) {
    if (t.first < 1 || t.second < 1 || t.first >= W - 1 || t.second >= H - 1) { bail("edge"); return; }
    if (on[I(t.first, t.second)]) { bail("crossing"); return; }
    on[I(t.first, t.second)] = 1;
  }
  // what it encloses: everything a 4-connected flood from the buffer's edge cannot reach
  std::vector<uint8_t> outside((size_t)W * H, 0);
  std::vector<int> q;
  for (int x = 0; x < W; x++) for (int y : {0, H - 1}) if (!outside[I(x, y)] && !on[I(x, y)]) { outside[I(x, y)] = 1; q.push_back((int)I(x, y)); }
  for (int y = 0; y < H; y++) for (int x : {0, W - 1}) if (!outside[I(x, y)] && !on[I(x, y)]) { outside[I(x, y)] = 1; q.push_back((int)I(x, y)); }
  for (size_t h = 0; h < q.size(); h++) {
    const int x = q[h] % W, y = q[h] / W;
    for (int k = 0; k < 4; k++) {
      const int nx = x + D4X[k], ny = y + D4Y[k];
      if (!in(nx, ny) || outside[I(nx, ny)] || on[I(nx, ny)]) continue;
      outside[I(nx, ny)] = 1;
      q.push_back((int)I(nx, ny));
    }
  }
  int enclosed = 0, before = 0;
  for (size_t i = 0; i < outside.size(); i++) { enclosed += !outside[i] && !on[i]; before += inside[i]; }
  if (outside[I(cx, cy)] || on[I(cx, cy)] || enclosed * 10 < before * 8) { bail("small"); return; }
  // every ring tile touches the outside along an edge (a thin ring, no tile buried in it)
  for (auto& t : path) {
    bool edge = false;
    for (int k = 0; k < 4; k++) if (in(t.first + D4X[k], t.second + D4Y[k]) && outside[I(t.first + D4X[k], t.second + D4Y[k])]) edge = true;
    if (!edge) { bail("buried"); return; }
  }
  ringT = on;
  for (size_t i = 0; i < inside.size(); i++) inside[i] = !outside[i] ? 1 : 0;
  // how far every tile is from the ring (Chebyshev, capped at 7): the lanes keep off its foot cheaply
  ringNear.assign((size_t)W * H, 7);
  for (auto& t : path)
    for (int oy = -6; oy <= 6; oy++)
      for (int ox = -6; ox <= 6; ox++) {
        const int x = t.first + ox, y = t.second + oy;
        if (!in(x, y)) continue;
        const uint8_t d = (uint8_t)std::max(std::abs(ox), std::abs(oy));
        if (d < ringNear[I(x, y)]) ringNear[I(x, y)] = d;
      }
}

// ------------------------------------------------------------------------------------------------ squares and streets
void Gen::paintSquare(int x0, int y0, float r, District d) {
  const uint32_t s = bseed + 5u + (uint32_t)squares.size() * 31u;
  int n = 0;
  // M3: a grid town's squares are rectangles (the forum, ragged at its edge where the houses crowd in); a radial
  // town's are round
  const float rxs = style == cult::Layout::Radial ? r * 1.15f : r * 1.25f, rys = style == cult::Layout::Radial ? r * 1.15f : r;
  for (int y = y0 - (int)rys - 2; y <= y0 + (int)rys + 2; y++)
    for (int x = x0 - (int)rxs - 2; x <= x0 + (int)rxs + 2; x++) {
      if (!in(x, y) || inCompound(x, y, 2) || wet(x, y)) continue;
      if (style == cult::Layout::Grid) {
        const float ex = std::fabs((float)(x - x0)) / (rxs + 0.5f), ey = std::fabs((float)(y - y0)) / (rys + 0.5f);
        if (std::max(ex, ey) > 1.0f || (ex > 0.86f && ey > 0.8f)) continue;
        if (std::max(ex, ey) > 0.88f && hashf(x, y, s) < 0.3f) continue;
      } else if (blob(x, y, x0, y0, rxs, rys, s) > 1.0f) continue;
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
  // (M1 economy) the main square holds the market: room for rows of stalls with their aisles beside the centrepiece
  float sqR = capital ? 8.2f : (city ? 7.0f : (town ? 5.4f : 3.2f));
  if (arch == Archetype::Market) sqR *= 1.25f;
  paintSquare(cx, cy, sqR, District::Centre);
  // (M1 economy) the market place: one side of the square opened out to fit the rows of stalls (their aisles and
  // keepers), its edge left a little ragged where the houses crowd in; the centrepiece stands across from it.
  // (M1 fixer) which side, and how deep and wide, by the town's seed: a market to the north of the heart in one town,
  // to the south, east or west in the next (the stalls always face south, so the rows sit differently in each)
  {
    const bool mk = arch == Archetype::Market;
    const uint32_t hs = hash2(cx, cy, bseed + 823u);
    const int hw = (village ? 4 : (town ? (mk ? 11 : 8) : (capital ? 10 : (mk ? 13 : 9)))) + (int)((hs >> 4) % 3) - 1;
    const int top = (village ? 4 : (town ? (mk ? 9 : 6) : (capital ? 11 : (mk ? 12 : 9)))) + (int)((hs >> 8) % 3) - 1;
    const int bot = village ? 1 : 2;
    int x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    auto zoneOf = [&](int side) {
      switch (side) {
        // (the houses on a square's south side rise over its southern rows, so a market reaching south takes a few
        // more rows to keep the same open ground)
        case 1: x0 = cx - hw; x1 = cx + hw; y0 = cy - bot; y1 = cy + top + 4; break;                   // south
        case 2: x0 = cx - bot - 1; x1 = cx + hw + top / 2 + 1; y0 = cy - (top + bot) / 2 - 2; y1 = cy + (top + bot) / 2 + 4; break;   // east
        case 3: x0 = cx - hw - top / 2 - 1; x1 = cx + bot + 1; y0 = cy - (top + bot) / 2 - 2; y1 = cy + (top + bot) / 2 + 4; break;   // west
        default: x0 = cx - hw; x1 = cx + hw; y0 = cy - top; y1 = cy + bot; break;                    // north
      }
    };
    // the drawn side, unless the water, the slope or the edge of the buffer spoils it: then the next side round that
    // does not (or spoils it least)
    int bestSide = (int)(hs % 4u), bestBad = 1 << 30;
    for (int k = 0; k < 4; k++) {
      const int side = (int)((hs + (uint32_t)k) % 4u);
      zoneOf(side);
      int bad = 0;
      for (int y = y0 - 1; y <= y1 + 1; y++)
        for (int x = x0 - 1; x <= x1 + 1; x++) {
          if (!in(x, y)) { bad += 4; continue; }
          if (wet(x, y) || water[I(x, y)] || inCompound(x, y, 2)) bad += 4;
          else if (lvl[I(x, y)] != lvl[I(cx, cy)]) bad++;
        }
      if (bad < bestBad) { bestBad = bad; bestSide = side; }
      if (bad == 0) break;
    }
    mktSide = bestSide;
    zoneOf(mktSide);
    mktZone = IRect{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
    // (M3) the market place is levelled (on a hillside its builders cut a step or two, as for a palace's terrace), so
    // its rows and tables get the whole of it; a mountain stays a mountain
    {
      const int L = lvl[I(cx, cy)];
      for (int y = y0 - 1; y <= y1 + 1; y++)
        for (int x = x0 - 1; x <= x1 + 1; x++)
          if (in(x, y) && !water[I(x, y)] && !inCompound(x, y, 2) && std::abs((int)lvl[I(x, y)] - L) <= 2) lvl[I(x, y)] = (uint8_t)L;
    }
    const uint32_t ms = bseed + 811u;
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        if (!in(x, y) || inCompound(x, y, 2) || wet(x, y) || get(x, y) == K_SQUARE) continue;
        if (walled && !ins(x, y)) continue;
        if (lvl[I(x, y)] != lvl[I(cx, cy)]) continue;
        const bool edgeX = x == x0 || x == x1, edgeY = y == y0 || y == y1;
        if (edgeX && edgeY) continue;                                   // rounded corners
        if ((edgeX || edgeY) && hashf(x, y, ms) < 0.35f) continue;       // a ragged edge
        set(x, y, K_SQUARE);
        if (village) M.setG(x, y, layout == 1 ? Ground::Dirt : (base == Ground::Grass ? Ground::Meadow : base));
        else M.setG(x, y, Ground::Plaza);
        M.setP(x, y, 0);
      }
  }
  if (city) {
    for (int k = 0; k < 4; k++) {
      float a = sector0 + k * (D_PI / 2) + rng.range(-0.25f, 0.25f);
      float d = 0.56f + rng.range(-0.06f, 0.06f);
      int x = cx + iround(dcos(a) * rx * d), y = cy + iround(dsin(a) * ry * d);
      if (inCompound(x, y, 6) || wet(x, y)) continue;
      paintSquare(x, y, 3.0f + rng.f(), sectorKind[k]);
    }
  } else if (town) {
    // (M1 fixer) a market town always has its second square: the second market (the produce and beast market)
    // (M3) a culture's town takes it from its colour (none, a small one, a larger one: town_rules.h townForm)
    const float q = rng.f();
    const int form = C.culture ? townForm(townColour(C.culture, P.type, P.ex, P.ey, P.kind)) : -1;
    if (form < 0 ? (q < 0.65f || arch == Archetype::Market) : (form > 0 || arch == Archetype::Market)) {
      float a = rng.f() * D_TAU;
      int x = cx + iround(dcos(a) * rx * 0.5f), y = cy + iround(dsin(a) * ry * 0.5f);
      if (!wet(x, y)) paintSquare(x, y, arch == Archetype::Market ? 3.6f : (form == 2 ? 3.3f : 2.6f), District::Centre);
    }
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
  else if (stilt && was == Ground::Swamp) M.setG(x, y, Ground::Bridge);   // (M3) a boardwalk over the reeds
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
    turn = turn * 0.82f + rng.range(-0.07f, 0.07f) * wander + err * 0.05f;
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
    // (M3) an approach road keeps off the wall's foot outside the ring (it would read as a road running into it):
    // past its first steps out of the gate it turns away from the masonry
    if (stop == 3 && i > 1) {
      auto foot = [&](int tx, int ty) {
        for (int oy = 0; oy < width; oy++)
          for (int ox = 0; ox < width; ox++)
            for (int d = 0; d < 4; d++) if (wallAt(tx + ox + D4X[d], ty + oy + D4Y[d])) return true;
        return false;
      };
      if (foot(ix, iy)) {
        bool ok = false;
        for (float d : {0.5f, -0.5f, 1.0f, -1.0f, 1.5f, -1.5f}) {
          const float a2 = ang + d;
          const int jx = ifloor(x + dcos(a2)), jy = ifloor(y + dsin(a2));
          if (!foot(jx, jy) && !wallAt(jx, jy) && !ins(jx, jy)) { na = a2; ix = jx; iy = jy; ok = true; break; }
        }
        if (!ok) break;
        turn = 0;
      }
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
    if (stop == 2 && terraced && (!in(px, py) || lvl[I(ix, iy)] != lvl[I(px, py)])) break;   // (M3) a lane keeps to its terrace
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
  // M3: the culture's layout replaces the organic rings and spokes
  if (style != cult::Layout::Organic && style != cult::Layout::Stilt) {
    switch (style) {
      case cult::Layout::Grid: gridLanes(false); break;
      case cult::Layout::Compound: gridLanes(true); break;
      case cult::Layout::Radial: radialRings(); break;
      case cult::Layout::Linear: pickSpine(); spineStreets(); break;
      default: break;   // Terraced: contour lanes (fabric)
    }
    if (city) wallStreet();
    return;
  }
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
    const float span = 3.4f + rng.f() * 2.2f;   // (draws sequenced: the right argument's first, as MSVC/GCC)
    const float a0 = rng.f() * D_TAU;
    ring(0.55f, a0, span, 1, K_LANE, Ground::Road, bseed + 21);
  } else {
    ring(0.36f, rng.f() * D_TAU, D_TAU + 0.05f, 2, K_MAIN, Ground::Road, bseed + 22);
    ring(0.64f, rng.f() * D_TAU, D_TAU + 0.05f, 1, K_LANE, Ground::Road, bseed + 23);
    wallStreet();
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

// the wall street: a lane following the inside of the city wall a few tiles in (where the wall's own outline is)
void Gen::wallStreet() {
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
      if (nearRing(x, y, 3)) break;   // (M3: the ring as rasterised may stand a little inside the outline)
      r0 = rr;
    }
    float x = cx + 0.5f + ca * rx * r0, y = cy + 0.5f + sa * ry * r0;
    if (have) line(px, py, x, y, 1, K_LANE, Ground::Road);
    px = x; py = y; have = true;
  }
}

// The street fabric: gently winding east-west lanes a house-row apart across the town (in the 3/4 view every door faces
// south, so a lane running east-west fronts a door every few tiles). The rings, the spokes and the main streets cross
// them, so the blocks come out irregular; the noble quarter keeps every other lane out (big plots and gardens), the
// poor quarter packs them closer. A lane never runs alongside another street, never over water (it stops at the bank),
// and in a walled city it ends at the wall street.
void Gen::fabric() {
  // M3: the lattice, the rings and the spine are the fabric of their styles; terraces get their contour lanes
  if (style == cult::Layout::Terraced) { contourLanes(); return; }
  if (style != cult::Layout::Organic && style != cult::Layout::Stilt) return;
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
      if (ok && walled) ok = blob(x, yy, cx, cy, rx, ry, wseed) < wallR - inset && !nearRing(x, yy, 2);
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
  int n = village ? 5 + rng.irange(3) : (town ? 14 : 40);
  // M3: a planned town has few stray lanes (a grid or compound town none: its lattice is complete)
  if (style == cult::Layout::Grid || style == cult::Layout::Compound) n = 0;
  else if (style == cult::Layout::Radial) n /= 3;
  else if (style == cult::Layout::Linear || style == cult::Layout::Terraced) n /= 2;
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
    M.wall[I(x, y)] = wallByte;   // (M3: 1 + the culture's CityWall)
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
      if (!ringT.empty()) edge = ringT[I(x, y)] != 0;   // (M3) the octilinear ring (wallRing)
      else
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
  // (M2 fixer round 3) the wall ring may reach into the buffer's edge, so a gate's surroundings can lie past the
  // buffer: there the land the chunk will have (C.base: a marsh pool, a lake shore) is asked directly, so no gatehouse
  // stands against a pool just outside the town's own land (seeds 36 and 37)
  auto wetNear = [&](int x, int y) {
    for (int oy = -3; oy <= 3; oy++)
      for (int ox = -3; ox <= 3; ox++) {
        if (in(x + ox, y + oy)) {
          if (water[I(x + ox, y + oy)] || M.at(x + ox, y + oy) == Ground::Bridge) return true;
        } else if (C.base) {
          Ground g = Ground::Grass;
          Biome b = Biome::Plains;
          uint8_t h = 0;
          C.base(O.gx + x + ox, O.gy + y + oy, g, b, h);
          if (groundWater(g) || g == Ground::Bridge) return true;
        }
      }
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
          // (M3: diagonally too: a thin 45-degree run meets a breach's corner diagonally, and it must not unravel)
          for (int oy = -1; oy <= 1 && !jamb; oy++)
            for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && inR(x + ox, y + oy)) { jamb = true; break; }
        }
        if (n < 2 && !jamb) { M.wall[I(x, y)] = 0; changed = true; }
      }
    if (!changed) break;
  }
  // (M3) a thin 45-degree run cut by a breach ends diagonally against the opening's corner: one more tile beside its
  // end makes it a proper jamb (the tower at the end of the run stands on it)
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      if (!wallAt(x, y)) continue;
      int n = 0;
      for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && wallAt(x + ox, y + oy)) n++;
      if (n >= 2) continue;
      auto inGap = [&](int px, int py) {
        for (const IRect& r : made) if (px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h) return true;
        return false;
      };
      if (inGap(x - 1, y) || inGap(x + 1, y) || inGap(x, y - 1) || inGap(x, y + 1)) continue;
      for (int oy : {-1, 1})
        for (int ox : {-1, 1}) {
          if (!inGap(x + ox, y + oy)) continue;
          // the tile beside the end, toward the opening, that is not in it
          if (!inGap(x + ox, y) && in(x + ox, y) && !M.wall[I(x + ox, y)]) { setWall(x + ox, y); oy = 2; break; }
          if (!inGap(x, y + oy) && in(x, y + oy) && !M.wall[I(x, y + oy)]) { setWall(x, y + oy); oy = 2; break; }
        }
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
  // (M3) a street right against the wall away from every opening is taken back first, however wide (a two-wide main
  // street that met the thin ring where its gate went elsewhere ended square against the masonry)
  for (int pass = 0; pass < 2; pass++) {
    std::vector<std::pair<int, int>> touch;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        const uint8_t k = get(x, y);
        if ((k != K_MAIN && k != K_LANE) || wallAt(x, y) || nearGap(x, y, 2)) continue;
        bool t = false;
        for (int d = 0; d < 4; d++) if (wallAt(x + D4X[d], y + D4Y[d])) t = true;
        if (t) touch.push_back({x, y});
      }
    if (touch.empty()) break;
    for (auto& t : touch) { set(t.first, t.second, K_NONE); restore(t.first, t.second); }
    cut = true;
  }
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
        // (M3) not along the wall's foot away from its own opening (it would read as a road running into the wall)
        if (!nearGap(nx, ny, 3)) {
          bool foot = false;
          for (int dd = 0; dd < 4; dd++) if (wallAt(nx + D4X[dd], ny + D4Y[dd])) foot = true;
          if (foot) continue;
        }
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
  auto flood = [&](size_t from) {
    for (size_t h = from; h < q.size(); h++) {
      int x = q[h] % W, y = q[h] / W;
      for (int d = 0; d < 4; d++) {
        int nx = x + D4X[d], ny = y + D4Y[d];
        if (!in(nx, ny) || seen[I(nx, ny)] || !isStreet(nx, ny) || wallAt(nx, ny)) continue;
        seen[I(nx, ny)] = 1;
        q.push_back((int)I(nx, ny));
      }
    }
  };
  for (int oy = -3; oy <= 3; oy++)
    for (int ox = -3; ox <= 3; ox++)
      if (isStreet(cx + ox, cy + oy) && !seen[I(cx + ox, cy + oy)]) { seen[I(cx + ox, cy + oy)] = 1; q.push_back((int)I(cx + ox, cy + oy)); }
  flood(0);
  // (M3) a planned town's lattice, rings or terraces can leave whole lanes cut off by a dropped stretch: each piece of
  // street the heart cannot reach is joined back by the shortest lane (up to 14 tiles) over open land before the
  // pruning would take it away
  if (C.culture) {
    std::vector<uint8_t> tried((size_t)W * H, 0);
    for (int y0 = 0; y0 < H; y0++)
      for (int x0 = 0; x0 < W; x0++) {
        const size_t i0 = I(x0, y0);
        const uint8_t k0 = mask[i0];
        if ((k0 != K_LANE && k0 != K_MAIN) || seen[i0] || tried[i0] || wallAt(x0, y0)) continue;
        // the piece
        std::vector<int> piece{(int)i0};
        tried[i0] = 1;
        for (size_t h = 0; h < piece.size(); h++) {
          const int x = piece[h] % W, y = piece[h] / W;
          for (int d = 0; d < 4; d++) {
            const int nx = x + D4X[d], ny = y + D4Y[d];
            if (!in(nx, ny) || tried[I(nx, ny)] || seen[I(nx, ny)] || !isStreet(nx, ny) || wallAt(nx, ny)) continue;
            tried[I(nx, ny)] = 1;
            piece.push_back((int)I(nx, ny));
          }
        }
        if (piece.size() < 4) continue;   // (a stub: pruned)
        // the shortest way from it over open land to a street the heart reaches
        std::vector<int> prev((size_t)W * H, -2), bq;
        for (int c : piece) { prev[(size_t)c] = -1; bq.push_back(c); }
        int found = -1;
        for (size_t h = 0; h < bq.size() && found < 0; h++) {
          const int c = bq[h], x = c % W, y = c / W;
          int depth = 0;
          for (int p = c; prev[(size_t)p] >= 0; p = prev[(size_t)p]) depth++;
          if (depth > 14) continue;
          for (int d = 0; d < 4; d++) {
            const int nx = x + D4X[d], ny = y + D4Y[d];
            if (!in(nx, ny) || prev[I(nx, ny)] != -2) continue;
            const size_t j = I(nx, ny);
            if (seen[j] && isStreet(nx, ny)) { prev[j] = c; found = (int)j; break; }
            if (mask[j] != K_NONE || water[j] || wallAt(nx, ny) || inCompound(nx, ny, 1) || groundSolid(M.at(nx, ny))) continue;
            if (walled && (!ins(nx, ny) || nearRing(nx, ny, 2))) continue;
            prev[j] = c;
            bq.push_back((int)j);
          }
        }
        if (found < 0) continue;
        for (int c = prev[(size_t)found]; c >= 0 && prev[(size_t)c] != -1; c = prev[(size_t)c]) paintStreet(c % W, c / W, K_LANE, laneG);
        // the piece and its new lane now reach the heart
        const size_t q0 = q.size();
        for (int c : piece) if (!seen[(size_t)c]) { seen[(size_t)c] = 1; q.push_back(c); }
        flood(q0);
        for (int y = 0; y < H; y++)   // (the lane's own tiles)
          for (int x = 0; x < W; x++)
            if (isStreet(x, y) && !seen[I(x, y)] && !wallAt(x, y)) {
              bool nb = false;
              for (int d = 0; d < 4; d++) if (in(x + D4X[d], y + D4Y[d]) && seen[I(x + D4X[d], y + D4Y[d])]) nb = true;
              if (nb) { const size_t q1 = q.size(); seen[I(x, y)] = 1; q.push_back((int)I(x, y)); flood(q1); }
            }
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
  // (M1 fixer round 2) the ring's tiles are chosen first and fenced after: marking each piece a yard as it went made
  // its neighbour skip itself (a yard beside it), so the ring came out as a dotted line of lone posts
  std::vector<int> ring;
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
      ring.push_back(y * W + x);
    }
  for (int k : ring) {
    const int x = k % W, y = k / W;
    float a = datan2((float)(y - cy) / ry, (float)(x - cx) / rx);
    M.setProp(x, y, std::fabs(dsin(a)) > 0.7f ? art::Prop::FenceH : art::Prop::FenceV);
    set(x, y, K_YARD);
  }
  // a lone post between two gaps encloses nothing: it goes
  auto fenceAt = [&](int x, int y) { const int q = M.propAt(x, y); return q == (int)art::Prop::FenceH + 1 || q == (int)art::Prop::FenceV + 1; };
  for (int k : ring) {
    const int x = k % W, y = k / W;
    bool nb = false;
    for (int oy = -1; oy <= 1 && !nb; oy++)
      for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && fenceAt(x + ox, y + oy)) { nb = true; break; }
    if (!nb) M.setP(x, y, 0);
  }
}

}  // namespace town
}  // namespace ew
