// Settlement buildings (TOWNS lane, VISION_PLAN 15.8): landmarks and services first, then homes by district, as frontage
// plots along the streets with uneven setbacks and short footpaths to the doors; and the capital's palace compound.
// Every sprite keeps clear of its neighbours' fronts and doorsteps (the V5 rule, here on occupancy grids so a city of
// two hundred buildings stays cheap). See rpg/world/town_gen.h.
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <queue>
#include "rpg/world/town_gen.h"

namespace ew {
namespace town {

using art::Building;
using art::Prop;

namespace {
const int D4X[4] = {0, 1, -1, 0}, D4Y[4] = {1, 0, 0, -1};
// the royal colours of a capital whose kingdom is unknown (never in a real endless world: capitals have kingdoms)
constexpr uint32_t kRoyalField = 0xFF8A2A2Au, kRoyalTrim = 0xFF3CC8E8u;
}  // namespace

// M3 (VISION_PLAN 5.7): how rich a building is, 0 poor .. 3 rich, for cult::buildingArch (poor: wattle and thatch
// substitutes; rich: stone, glazing, more ornament). A city by its quarter (noble 3, the old centre 2, the crafts and
// temple quarters 1 or 2, the poor quarter 0; a royal seat lifts its poorer quarters a step); a town by how near the
// heart it stands; a village 0 or 1. Landmarks keep their rank (palace and keep 3, a temple 2), stone houses a step up.
int Gen::wealthFor(Building t, const IRect& r) const {
  switch (t) {
    case Building::Palace: case Building::Keep: return 3;
    case Building::Temple: return capital ? 3 : 2;
    case Building::Barracks: case Building::Tower: return 2;
    case Building::Hut: return 0;
    default: break;
  }
  const uint32_t h = hash2(r.x, r.y, P.seed ^ 0x3EA17Bu);
  int w = 1;
  if (city) {
    switch (districtAt(r.cx(), r.cy())) {
      case District::Noble: w = 3; break;
      case District::Centre: w = 2; break;
      case District::Crafts: w = 1 + (int)(h & 1u); break;
      case District::Temple: w = 1 + (int)((h >> 1) & 1u); break;
      case District::Poor: w = 0; break;
      default: break;
    }
    if (capital && w < 2) w++;
  } else if (town) {
    const float d = dist(r.cx(), r.cy());
    w = d < 0.35f ? 2 : (d < 0.7f ? 1 : ((h % 3u) == 0 ? 0 : 1));
  } else {
    w = dist(r.cx(), r.cy()) < 0.45f ? 1 : (int)(h & 1u);
  }
  // the settlement's own wealth skews its houses a step either way (a rich market town's poor lane is still kept up)
  if (townWealth >= 0) w += std::clamp(townWealth - (capital ? 3 : (city ? 2 : (town ? 1 : 0))), -1, 1);
  if (t == Building::StoneHouse) w++;
  if (t == Building::Inn || t == Building::Shop) w = std::max(w, 1);
  if (t == Building::Farmhouse) w = std::min(w, 1);
  return std::clamp(w, 0, 3);
}

void Gen::kingdomColours(Bldg& b) const {
  if (b.type != Building::Keep && b.type != Building::Palace && b.type != Building::Barracks && b.type != Building::Inn) return;
  if (C.kingdom && C.kingdom->color) {
    b.banner = C.kingdom->color;
    b.banner2 = C.kingdom->color2 ? C.kingdom->color2 : 0xFF40C0E0u;
    b.emblem = (uint8_t)(C.kingdom->emblem % KINGDOM_EMBLEMS);
  } else if (capital && (b.type == Building::Palace || b.type == Building::Barracks)) {
    b.banner = kRoyalField;
    b.banner2 = kRoyalTrim;
    b.emblem = 0;
  }
}

// storeys and hearths are hashed per building (no rng draw), as the classic generator decides them; the palace and the
// barracks always rise two storeys (the throne hall under the royal apartments; the guardroom under the dormitory)
static int storeysFor(Building t, int w, int h, uint32_t hs) {
  if (t == Building::Palace || t == Building::Barracks) return 2;
  return bldgStoreysV7(t, w, h, hs);
}

int Gen::putBldg(Building type, IRect r, Role owner, int storeys) {
  Bldg b;
  b.type = type;
  b.r = r;
  b.site = -1;
  b.owner = owner;
  b.storeys = (uint8_t)storeys;
  const uint32_t hs = hash32(P.seed ^ ((uint32_t)M.bldgs.size() * 0x9E3779B1u) ^ 0x5707EE5u);
  b.hearth = (type == Building::Palace || type == Building::Barracks) ? true : bldgHearthV7(type, storeys, hash32(hs + 77u));
  // (M2 fixer) the settlement's own biome, not the tile's: a town that straddles an ecotone built half its houses in
  // another family (bare slate, thatch and copper beside snowed roofs in a snowbound capital)
  b.biome = bio;
  b.seed = rng.next();
  b.genVer = WORLDGEN_LATEST;
  b.urban = (uint8_t)(capital ? 3 : city ? 2 : town ? 1 : 0);
  static const uint32_t roofs[] = {rgba(150, 62, 48), rgba(84, 92, 120), rgba(110, 78, 52), rgba(70, 100, 80), rgba(130, 100, 60), rgba(96, 60, 90)};
  b.roof = (type == Building::House || type == Building::StoneHouse) ? roofs[rng.irange(6)] : 0;
  // M3: the culture's architecture for this building (VISION_PLAN 5.7), as rich as its quarter (wealthFor)
  if (C.culture) {
    b.arch = cult::buildingArch(*C.culture, (int)bio, b.urban, wealthFor(type, r), b.seed, b.roof);
    b.styled = true;
    // a house over the marsh stands on stilts whatever its people's usual foundation
    if (stilt) {
      bool wetFoot = false;
      for (int y = r.y; y < r.y + r.h && !wetFoot; y++)
        for (int x = r.x; x < r.x + r.w; x++)
          if (in(x, y) && (M.at(x, y) == Ground::Water || M.at(x, y) == Ground::Swamp)) { wetFoot = true; break; }
      if (wetFoot) { b.arch.foundation = art::Foundation::Stilts; b.arch.stilts = true; }
    }
  }
  kingdomColours(b);
  M.bldgs.push_back(b);
  const int bi = (int)M.bldgs.size() - 1;
  for (int y = r.y; y < r.y + r.h; y++)
    for (int x = r.x; x < r.x + r.w; x++) if (in(x, y)) M.bldgAt[I(x, y)] = (int16_t)bi;
  // the clearance grids: what its sprite covers, and its front (foundation row, doorstep and the apron before it)
  const int rise = townRiseTiles(type, storeys);
  for (int y = r.y - rise; y <= r.y + r.h; y++)
    for (int x = r.x - 1; x <= r.x + r.w; x++) if (in(x, y)) cover[I(x, y)] = 1;
  for (int x = r.x; x < r.x + r.w; x++) if (in(x, r.y + r.h - 1)) front[I(x, r.y + r.h - 1)] = 1;
  const int dx = b.doorX();
  for (int y = r.y + r.h; y <= r.y + r.h + 1; y++)
    for (int x = dx - 1; x <= dx + 1; x++) if (in(x, y)) front[I(x, y)] = 1;
  return bi;
}

bool Gen::fits(const IRect& r, Building type, int storeys) const {
  if (!in(r.x - 1, r.y - 1) || !in(r.x + r.w, r.y + r.h + 1)) return false;
  const int rise = townRiseTiles(type, storeys);
  const int need = lvl[I(r.x, r.y + r.h - 1)];
  // the footprint and a ring of one tile round it
  for (int y = r.y - 1; y <= r.y + r.h; y++)
    for (int x = r.x - 1; x <= r.x + r.w; x++) {
      const size_t i = I(x, y);
      if (M.bldgAt[i] >= 0 || M.wall[i] || noBuild[i]) return false;
      const bool foot = x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
      if (!foot) continue;
      if (mask[i] != K_NONE) return false;
      Ground g = M.at(x, y);
      // (M1) never on the paving a street spreads over (its frayed edges), and a house's side stands back a step
      // from a main street instead of closing half of it (only its front may face the road directly)
      if (g == Ground::Road || g == Ground::Plaza) return false;
      if (y < r.y + r.h - 1 && (mask[I(x - 1, y)] == K_MAIN || mask[I(x + 1, y)] == K_MAIN || mask[I(x, y - 1)] == K_MAIN)) return false;
      // (M3) a stilt town builds over its marsh pools and reeds (never over a river)
      const bool marsh = stilt && !water[i] && (g == Ground::Water || g == Ground::Swamp);
      if (!marsh && (groundSolid(g) || g == Ground::Bridge || g == Ground::Swamp || water[i])) return false;
      if (lvl[i] != need) return false;
      if (walled && !inside[i]) return false;
      // (M3) on terraces nothing stands on a terrace's face (the row under a higher neighbour)
      if (terraced)
        for (int d = 0; d < 4; d++) if (in(x + D4X[d], y + D4Y[d]) && lvl[I(x + D4X[d], y + D4Y[d])] > lvl[i]) return false;
    }
  // the doorstep and the apron before it: dry land on the same level, nobody's wall or house
  const int dx = r.x + r.w / 2;
  for (int y = r.y + r.h; y <= r.y + r.h + 1; y++)
    for (int x = dx - 1; x <= dx + 1; x++) {
      const size_t i = I(x, y);
      if (M.bldgAt[i] >= 0 || M.wall[i]) return false;
      if (x == dx && y == r.y + r.h) {
        const bool marsh = stilt && !water[i] && M.at(x, y) == Ground::Water;   // (a boardwalk will cross it)
        if ((groundSolid(M.at(x, y)) && !marsh) || lvl[i] != need || mask[i] == K_FIELD) return false;
        if (terraced)
          for (int d = 0; d < 4; d++) if (in(x + D4X[d], y + D4Y[d]) && lvl[I(x + D4X[d], y + D4Y[d])] > lvl[i]) return false;
      }
    }
  // the sprite (roof, eaves, steps) covers nobody's front, and stays out of the gate approaches and the compound
  for (int y = r.y - rise; y <= r.y + r.h; y++)
    for (int x = r.x - 1; x <= r.x + r.w; x++) {
      if (!in(x, y)) continue;
      const size_t i = I(x, y);
      if (front[i] || (noBuild[i] & 6)) return false;   // (4: a street market's lot, kept open for its stalls)
    }
  // our front lies under nobody's sprite
  for (int x = r.x; x < r.x + r.w; x++) if (cover[I(x, r.y + r.h - 1)]) return false;
  for (int y = r.y + r.h; y <= r.y + r.h + 1; y++)
    for (int x = dx - 1; x <= dx + 1; x++) if (cover[I(x, y)]) return false;
  if (walled || compound.w > 0) {
    // a front door opens onto the street, never onto a wall a step away; and the roof keeps off the ring
    for (int y = r.y + r.h; y <= r.y + r.h + 2; y++)
      for (int x = r.x - 1; x <= r.x + r.w; x++) if (wallAt(x, y)) return false;
    for (int y = r.y - 2; y <= r.y - 1; y++)
      for (int x = r.x - 1; x <= r.x + r.w; x++) if (wallAt(x, y)) return false;
  }
  return true;
}

// shortest footpath from a door's apron to any street (BFS near the door)
bool Gen::footpath(int ax, int ay, std::vector<std::pair<int, int>>& path, const IRect* own) const {
  path.clear();
  if (isStreet(ax, ay)) return true;
  const int R = 14, S = 2 * R + 1;
  std::vector<int> prev((size_t)S * S, -2);
  auto id = [&](int x, int y) { return (y - ay + R) * S + (x - ax + R); };
  std::queue<std::pair<int, int>> q;
  prev[(size_t)id(ax, ay)] = -1;
  q.push({ax, ay});
  while (!q.empty()) {
    auto [x, y] = q.front();
    q.pop();
    if (isStreet(x, y)) {
      for (int p = prev[(size_t)id(x, y)]; p >= 0; p = prev[(size_t)p]) path.push_back({ax - R + p % S, ay - R + p / S});
      if (!path.empty()) path.pop_back();   // the apron itself is not part of the path
      return (int)path.size() <= pathMax;
    }
    for (int d = 0; d < 4; d++) {
      int nx = x + D4X[d], ny = y + D4Y[d];
      if (std::abs(nx - ax) > R || std::abs(ny - ay) > R || !in(nx, ny) || prev[(size_t)id(nx, ny)] != -2) continue;
      const size_t i = I(nx, ny);
      // (M3) never through the house being placed (its footprint is not in bldgAt yet: a way over the marsh round to
      // its back looked shorter than one from its door)
      if (own && nx >= own->x && ny >= own->y && nx < own->x + own->w && ny < own->y + own->h) continue;
      // (M3) nor along a wall's foot (a path running into the masonry reads as a road that missed the gate)
      if ((noBuild[i] & 1) && !isStreet(nx, ny)) continue;
      const bool marsh = stilt && !water[i] && M.at(nx, ny) == Ground::Water;   // (M3) a boardwalk will cross it
      if (M.bldgAt[i] >= 0 || M.wall[i] || (groundSolid(M.at(nx, ny)) && !marsh) || mask[i] == K_FIELD || mask[i] == K_COMPOUND) continue;
      if (M.prop[i] && propSolid((Prop)(M.prop[i] - 1))) continue;
      prev[(size_t)id(nx, ny)] = id(x, y);
      q.push({nx, ny});
    }
  }
  return false;
}

bool Gen::tryPlace(Building type, Role owner, int bw, int bh, int ax, int ay, bool required) {
  (void)required;
  IRect r{ax - bw / 2, ay - bh, bw, bh};
  const uint32_t hs = hash32(P.seed ^ ((uint32_t)M.bldgs.size() * 0x9E3779B1u) ^ 0x5707EE5u);
  const int st = storeysFor(type, bw, bh, hs);
  if (!fits(r, type, st)) return false;
  std::vector<std::pair<int, int>> path;
  if (!footpath(ax, ay, path, &r)) return false;
  putBldg(type, r, owner, st);
  const Ground fp = city ? Ground::Road : Ground::Dirt;
  // (M3) in a stilt town the way over the marsh is a boardwalk
  auto pave = [&](int x, int y) {
    const Ground g = M.at(x, y);
    if (stilt && !water[I(x, y)] && (g == Ground::Water || g == Ground::Swamp)) M.setG(x, y, Ground::Bridge);
    else if (!groundWater(g) && g != Ground::Bridge) M.setG(x, y, fp);
  };
  if (get(ax, ay) == K_NONE) { set(ax, ay, K_YARD); if (stilt) pave(ax, ay); else M.setG(ax, ay, fp); M.setP(ax, ay, 0); }
  for (auto& p : path) {
    if (get(p.first, p.second) != K_NONE && get(p.first, p.second) != K_YARD) continue;
    set(p.first, p.second, K_YARD);
    pave(p.first, p.second);
    M.setP(p.first, p.second, 0);
  }
  return true;
}

bool Gen::placeBuilding(const Want& w) {
  const int tries = w.req ? 3000 : 600;
  for (int t = 0; t < tries; t++) {
    if (allStreet.empty()) return false;
    auto s = allStreet[(size_t)rng.irange((int)allStreet.size())];
    if (w.px >= 0) {
      if (std::abs(s.first - w.px) + std::abs(s.second - w.py) > 8 + t / 40) continue;
    } else if (dist(s.first, s.second) > w.near + t / 500.0f) continue;
    // (M1) a farm stands out among its fields, never on the market square
    if (w.b == Building::Farmhouse && dist(s.first, s.second) < 0.62f - t / 2500.0f) continue;
    // (M1 economy) the windmill stands out by the fields in the wind; the noisy, smelly and dusty trades (the smelter,
    // the sawmill, the tannery, the granary) keep to the edge
    // (M1 fixer round 2: a village's mill stands just past its houses, in sight of the green, not out at the far edge)
    if (w.b == Building::Windmill && dist(s.first, s.second) < (village ? 0.55f : 0.78f) - t / 2000.0f) continue;
    // (M3) ... but in sight of the green: a village strung out along its spine keeps its mill within a short walk
    if (w.b == Building::Windmill && village && std::max(std::abs(s.first - cx), std::abs(s.second - cy)) > 18 && t < tries * 9 / 10) continue;
    if ((w.b == Building::Smelter || w.b == Building::Sawmill || w.b == Building::Tanner || w.b == Building::Granary) && !city &&
        dist(s.first, s.second) < 0.5f - t / 2500.0f) continue;
    if (city && w.d != District::COUNT && districtAt(s.first, s.second) != w.d && t < tries * 6 / 10) continue;
    if (w.north && s.second > cy - 2 && t < tries / 2) continue;
    if (M.at(s.first, s.second) == Ground::Bridge) continue;
    int bw = w.w, bh = w.h;
    int ax, ay;
    int side = rng.irange(100);
    if (side < 50) { ax = s.first + rng.irange(3) - 1; ay = s.second - 1 - (rng.irange(3) == 0 ? 1 : 0); }
    else if (side < 68) { ax = s.first + 1 + bw / 2 + rng.irange(2); ay = s.second + rng.irange(3) - 1; }
    else if (side < 86) { ax = s.first - (bw - bw / 2) - rng.irange(2); ay = s.second + rng.irange(3) - 1; }
    else { ax = s.first + rng.irange(3) - 1; ay = s.second + 1 + bh + rng.irange(2); }
    if (!w.req && !city && rng.f() > 1.3f - dist(ax, ay) * 0.75f) continue;   // denser at the heart, sparse at the edge
    if (tryPlace(w.b, w.r, bw, bh, ax, ay, w.req)) return true;
  }
  return false;
}

// what a home looks like in each part of town
void Gen::homeShape(District d, Building& t, int& bw, int& bh) {
  if (village) {
    float hutP = arch == Archetype::Fishing ? 0.55f : 0.4f;
    if (rng.f() < hutP) { t = Building::Hut; bw = 3; bh = 2; return; }
    t = (arch == Archetype::Mining && rng.f() < 0.35f) ? Building::StoneHouse : Building::House;
    bw = 3 + rng.irange(2); bh = 2 + rng.irange(2);
    return;
  }
  if (town) {
    float stoneP = arch == Archetype::Mining || arch == Archetype::HillFort ? 0.5f : 0.3f;
    t = rng.f() < stoneP ? Building::StoneHouse : Building::House;
    bw = 3 + rng.irange(3); bh = 2 + rng.irange(2);
    if (rng.f() < 0.08f) { t = Building::Hut; bw = 3; bh = 2; }
    return;
  }
  switch (d) {
    case District::Noble:
      t = rng.f() < 0.8f ? Building::StoneHouse : Building::House;
      bw = 4 + rng.irange(3); bh = 3 + rng.irange(2);
      break;
    case District::Poor:
      if (rng.f() < (capital ? 0.08f : 0.35f)) { t = Building::Hut; bw = 3; bh = 2; }   // (M1: a royal seat's poor quarter is still a town)
      else { t = Building::House; bw = 3 + rng.irange(2); bh = 2; }
      break;
    case District::Crafts:
      t = rng.f() < 0.3f ? Building::StoneHouse : Building::House;
      bw = 3 + rng.irange(3); bh = rng.f() < 0.75f ? 2 : 3;
      break;
    case District::Temple:
      t = rng.f() < 0.55f ? Building::StoneHouse : Building::House;
      bw = 3 + rng.irange(3); bh = rng.f() < 0.7f ? 2 : 3;
      break;
    default:   // the old centre: tall narrow town houses (in a capital, mostly stone)
      t = rng.f() < (capital ? 0.8f : 0.55f) ? Building::StoneHouse : Building::House;
      bw = 3 + rng.irange(2); bh = rng.f() < 0.75f ? 2 : 3;
      break;
  }
}

// landmarks and services, nearest the heart first (VISION_PLAN 15.8: every village has an inn or tavern, a well or green
// and a shop or smith; towns an inn, shop, smith and temple; cities several inns and shops, a temple, a mage tower and
// the jarl's keep)
void Gen::services() {
  std::vector<Want> want;
  auto add = [&](Building b, Role r, int w, int h, float near, bool req, bool north = false, District d = District::COUNT, int px = -1, int py = -1) {
    want.push_back(Want{b, r, w, h, near, req, north, d, px, py});
  };
  if (village) {
    // (owner 2026-10-05) every village keeps its inn, its smith, its well and a small market; a general store in some
    add(Building::Inn, Role::Innkeeper, 5 + rng.irange(2), 3, 0.5f, true);
    add(Building::Smithy, Role::Smith, 5, 3, 0.65f, true);
    if (rng.f() < 0.45f || arch == Archetype::Market) add(Building::Shop, Role::Merchant, 4, 3, 0.75f, false);
  } else if (town) {
    add(Building::Inn, Role::Innkeeper, 6, 3, 0.45f, true);
    add(Building::Shop, Role::Merchant, 4, 3, 0.45f, true);
    add(Building::Smithy, Role::Smith, 5, 3, 0.7f, true);
    add(Building::Temple, Role::Priest, 6, 4, 0.6f, true);
    if (rng.f() < 0.5f || arch == Archetype::Market) add(Building::Shop, Role::Merchant, 4, 3, 0.6f, false);
    if (arch == Archetype::Market || arch == Archetype::Port || arch == Archetype::RiverCrossing) add(Building::Inn, Role::Innkeeper, 5, 3, 0.7f, false);
    if (arch == Archetype::Mining) add(Building::Smithy, Role::Smith, 5, 3, 0.8f, false);
    if (arch == Archetype::Market) add(Building::Shop, Role::Merchant, 4, 3, 0.5f, false);
  } else {
    add(Building::Keep, Role::Jarl, 9, 4, 0.55f, true, true, District::Noble);
    add(Building::Temple, Role::Priest, 7, 5, 0.65f, true, false, District::Temple);
    add(Building::Inn, Role::Innkeeper, 6, 3, 0.35f, true, false, District::Centre);
    add(Building::Shop, Role::Merchant, 4, 3, 0.35f, true, false, District::Centre);
    add(Building::Smithy, Role::Smith, 5, 3, 0.75f, true, false, District::Crafts);
    add(Building::Tower, Role::Mage, 3, 3, 0.8f, true, false, rng.f() < 0.5f ? District::Temple : District::Noble);
    // the gate inns, one by each of the two strongest roads' gates
    int inns = 0;
    for (size_t k = 0; k < O.wallGaps.size() && inns < 2; k++) {
      if (k >= gateBearing.size() || gateBearing[k] < 0) continue;
      const IRect& g = O.wallGaps[k];
      int gx = g.x + g.w / 2, gy = g.y + g.h / 2;
      float a = datan2((float)(gy - cy), (float)(gx - cx));
      add(Building::Inn, Role::Innkeeper, 5 + rng.irange(2), 3, 1.0f, inns == 0, false, District::COUNT, gx - (int)(dcos(a) * 9), gy - (int)(dsin(a) * 9));
      inns++;
    }
    add(Building::Shop, Role::Merchant, 4, 3, 0.4f, true, false, District::Centre);
    add(Building::Shop, Role::Merchant, 5, 3, 0.5f, false, false, District::Centre);
    add(Building::Shop, Role::Merchant, 4, 3, 0.6f, false, false, District::Crafts);
    add(Building::Smithy, Role::Smith, 5, 3, 0.8f, false, false, District::Crafts);
    // the garrison: a barracks by the strongest road's gate (capitals keep the royal guard's in the palace compound)
    if (!capital) {
      for (size_t k = 0; k < O.wallGaps.size(); k++) {
        if (k >= gateBearing.size() || gateBearing[k] != 0) continue;
        const IRect& g = O.wallGaps[k];
        int gx = g.x + g.w / 2, gy = g.y + g.h / 2;
        float a = datan2((float)(gy - cy), (float)(gx - cx));
        add(Building::Barracks, Role::Guard, 7, 4, 1.0f, false, false, District::COUNT, gx - (int)(dcos(a) * 12), gy - (int)(dsin(a) * 12));
        break;
      }
    }
    if (arch == Archetype::Market) { add(Building::Shop, Role::Merchant, 4, 3, 0.5f, false); add(Building::Shop, Role::Merchant, 5, 3, 0.5f, false); add(Building::Inn, Role::Innkeeper, 6, 3, 0.5f, false); }
    if (arch == Archetype::Mining) add(Building::Smithy, Role::Smith, 5, 3, 0.8f, false, false, District::Crafts);
    if (arch == Archetype::Port || arch == Archetype::RiverCrossing) add(Building::Inn, Role::Innkeeper, 6, 3, 0.8f, false);
  }
  // M3 radial (the steppe camp, the sun temple, the spires): the chief's hall, the temple or the keep faces the plaza
  // from its north side, the rings round them both
  if (style == cult::Layout::Radial && !want.empty()) {
    const bool templeFirst = cArch == (int)cult::Archetype::SunTemple || cArch == (int)cult::Archetype::Starspire;
    const Building lead = city ? Building::Keep : (templeFirst && town ? Building::Temple : Building::Inn);
    const float sqR = capital ? 8.2f : (city ? 7.0f : (town ? 5.4f : 3.2f));
    for (Want& w : want)
      if (w.b == lead) {
        // (beyond the market when the market fills the plaza's north side)
        w.px = cx; w.py = mktSide == 0 && mktZone.w > 0 ? mktZone.y - 1 : cy - (int)(sqR * 1.15f) - 1;
        w.north = false; w.d = District::COUNT;
        break;
      }
  }
  // (M1 economy) the mill, the specialisation's workshop, the towns' trades; the optional ones after the homes
  {
    std::vector<Want> econ;
    economyServices(econ);
    for (const Want& w : econ) (w.req ? want : lateWants).push_back(w);
  }
  for (const Want& w : want) {
    int before = (int)M.bldgs.size();
    placeWant(w);
    if (w.b == Building::Keep && (int)M.bldgs.size() > before) keepIdx = before;
  }
}

bool Gen::placeWant(const Want& w0) {
  Want w = w0;
  bool ok = false;
  if (w.water) ok = placeByWater(w.b, w.r, w.w, w.h, w.water == 1);
  if (!ok && w.water == 1) { w.b = Building::Windmill; w.w = 4; w.h = 3; w.near = 1.2f; }   // no bank for a wheel: wind
  if (!ok) ok = placeBuilding(w);
  if (!ok && w.req) {
    Want s = w;
    s.w = std::max(3, w.w - 1); s.h = std::max(w.b == Building::Windmill ? 3 : 2, w.h - 1); s.near = 2.0f; s.north = false; s.d = District::COUNT; s.px = s.py = -1;
    ok = placeBuilding(s);
  }
  return ok;
}

// (M3) homes() is built over several generator phases (homesBegin, homesSweep until it is done, homesFinish), so a
// city's two hundred homes never cost one web frame their whole build: the sweep places the homes of a few hundred
// street tiles per call. The sequence is the same however it is cut (run() and the web's phase-at-a-time build agree).
void Gen::homes() {
  homesBegin();
  while (homesSweep()) {}
  homesFinish();
}

void Gen::homesBegin() {
  const TownScale sc = townScale(P.type);
  homesWant = village ? 11 + rng.irange(4) : (town ? 44 + rng.irange(12) : sc.homesMin + 6 + rng.irange(24));
  hHave = 0;
  for (const Bldg& b : M.bldgs) if (townIsHome(b.type)) hHave++;
  // farmhouses at the edge, among their fields (villages and towns)
  if (!city) {
    int farms = village ? (arch == Archetype::Farming ? 3 : 1 + rng.irange(2)) : (arch == Archetype::Farming ? 4 : 2);
    for (int k = 0; k < farms && hHave < homesWant; k++)
      if (placeBuilding(Want{Building::Farmhouse, Role::Farmer, 5, 3, 1.2f, false, false, District::COUNT, -1, -1})) hHave++;
  }
  // M3 compound (dune courtyards, highland clans): most homes stand in walled family compounds along the lanes
  if (style == cult::Layout::Compound) hHave += compoundHomes(homesWant - hHave);
  pathMax = city ? 5 : (town ? 7 : 9);   // homes front the streets: short footpaths, not trails across the gardens
  // the frontage sweep: every street tile once, from the heart outward (a little noise so the edge of the built-up
  // area is ragged, not a circle), a plot on its north side first (the door right on the street), then behind it,
  // then beside it; a second pass with smaller plots fills what is left
  std::vector<uint8_t> seen((size_t)W * H, 0);
  hOrder.clear();
  for (auto& s : allStreet) {
    const size_t i = I(s.first, s.second);
    if (seen[i] || M.at(s.first, s.second) == Ground::Bridge || M.wall[i]) continue;
    seen[i] = 1;
    float d = dist(s.first, s.second);
    if (!walled && d > 1.0f) continue;
    hOrder.push_back({d * (city ? 0.7f : 1.0f) + hfAt(s.first, s.second, 19u) * (city ? 0.25f : 0.35f), (int)i});
  }
  std::stable_sort(hOrder.begin(), hOrder.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first < b.first; });
  hPass = 0;
  hIdx = 0;
}

// a slice of the frontage sweep; false once it is done
bool Gen::homesSweep() {
  const size_t slice = 160;
  for (size_t n = 0; n < slice; n++) {
    if (hPass >= 4 || hHave >= homesWant) return false;
    if (hIdx >= hOrder.size()) { hIdx = 0; hPass++; continue; }
    const auto& o = hOrder[hIdx++];
    const int pass = hPass;
    const int sx = o.second % W, sy = o.second / W;
    if (!isStreet(sx, sy)) continue;
    const float d = dist(sx, sy);
    // villages and towns: denser at the heart, gardens and orchards between the houses toward the edge
    if (!city && pass < 3 && rng.f() > (village ? 1.35f : 1.3f) - d * 0.7f / densityF) continue;   // (M3: a dense people packs closer)
    Building t;
    int bw, bh;
    homeShape(districtAt(sx, sy), t, bw, bh);
    if (pass >= 2) { bw = std::max(3, bw - 1); bh = 2; if (t == Building::StoneHouse && rng.f() < 0.5f) t = Building::House; }
    if (pass == 3) bw = 3;   // the last pass squeezes cottages into what is left
    int setback = (!city && rng.irange(3) == 0) || (city && rng.irange(5) == 0) ? 1 : 0;
    // (M3) a dense people builds right on the street; a loose one stands back more often
    if (setback && densityF > 1.25f && hfAt(sx, sy, 29u) < densityF - 1.25f) setback = 0;
    if (!setback && densityF < 0.75f && hfAt(sx, sy, 31u) < 0.75f - densityF) setback = 1;
    const Role owner = t == Building::Farmhouse ? Role::Farmer : Role::Villager;
    const int j0 = rng.irange(3) - 1;
    bool done = false;
    // north of the street (the door on it), at an uneven setback and a little to either side; then a narrower plot
    for (int k = 0; k < 9 && !done; k++) {
      int wv = k < 3 ? bw : std::max(3, bw - 1);
      int hv = k < 6 ? bh : 2;
      int jx = j0 + (k % 3 == 1 ? 1 : (k % 3 == 2 ? -1 : 0));
      done = tryPlace(t, owner, wv, hv, sx + jx, sy - 1 - setback, false) || tryPlace(t, owner, wv, hv, sx + jx, sy - 2 + setback, false);
    }
    // behind it (the back to the street, a path round), beside it
    if (!done && pass >= 1) done = tryPlace(t, owner, bw, bh, sx + j0, sy + 1 + bh, false) || tryPlace(t, owner, bw, bh, sx + j0, sy + 2 + bh, false);
    for (int k = 0; k < 3 && !done; k++)
      done = tryPlace(t, owner, bw, bh, sx + 1 + bw / 2, sy + j0 + k - 1, false) || tryPlace(t, owner, bw, bh, sx - (bw - bw / 2), sy + j0 + k - 1, false);
    if (done) hHave++;
  }
  return hPass < 4 && hHave < homesWant;
}

void Gen::homesFinish() {
  int have = hHave;
  hOrder.clear();
  hOrder.shrink_to_fit();
  // still short of the scale (a river or the sea took the land, a hillside's terraces): every open tile near a street
  // becomes a candidate doorstep for a cottage, the footpaths may run a little longer
  const TownScale need = townScale(P.type);
  // (M1 economy: a second, wider round when the first leaves a hillside city short: the footpaths may run to 15)
  for (int round = 0; round < 2 && have < need.homesMin + 2; round++) {
    pathMax = round == 0 ? 12 : 15;
    const int reach = round == 0 ? 5 : 8;
    // (M3) the tiles near a street, from a distance map (it was a square search round every tile: a city paid
    // several ms for it in one web frame)
    std::vector<uint8_t> near((size_t)W * H, 0);
    {
      // a street within the square of `reach` round the tile: the street mask dilated along the rows, then the columns
      // (running counts over a sliding window)
      std::vector<uint8_t> row((size_t)W * H, 0);
      for (int y = 0; y < H; y++) {
        int cnt = 0;
        for (int x = -reach; x < W; x++) {
          if (x + reach < W && isStreet(x + reach, y)) cnt++;
          if (x - reach - 1 >= 0 && isStreet(x - reach - 1, y)) cnt--;
          if (x >= 0) row[I(x, y)] = cnt > 0 ? 1 : 0;
        }
      }
      for (int x = 0; x < W; x++) {
        int cnt = 0;
        for (int y = -reach; y < H; y++) {
          if (y + reach < H && row[I(x, y + reach)]) cnt++;
          if (y - reach - 1 >= 0 && row[I(x, y - reach - 1)]) cnt--;
          if (y >= 0) near[I(x, y)] = cnt > 0 ? 1 : 0;
        }
      }
    }
    std::vector<int> spots;
    for (int y = 2; y < H - 2; y++)
      for (int x = 2; x < W - 2; x++) {
        uint8_t k = get(x, y);
        if ((k != K_NONE && k != K_YARD) || M.bldgAt[I(x, y)] >= 0 || water[I(x, y)]) continue;
        if (!walled && dist(x, y) > 1.15f) continue;
        if (near[I(x, y)]) spots.push_back(y * W + x);
      }
    for (size_t i = spots.size(); i > 1; i--) std::swap(spots[i - 1], spots[(size_t)rng.irange((int)i)]);
    for (int s : spots) {
      if (have >= need.homesMin + 2) break;
      const Building t = village && rng.f() < 0.5f ? Building::Hut : Building::House;
      if (tryPlace(t, Role::Villager, 3, 2, s % W, s / W, false)) have++;
    }
  }
  pathMax = 12;
  // (M1 economy) the optional trades where the homes left room
  for (const Want& w : lateWants) placeWant(w);
  lateWants.clear();
}

// ------------------------------------------------------------------------------------------------ M3 compounds
// Compound (dune courtyards, highland clan steadings): a family's houses stand along the north side of a walled court
// (the fence art takes the culture's style: dry-stone dykes, mud-brick walls), their doors on the court, one gate in
// the south wall onto the lane; a well or a trough and a tree in the court. Compounds sit side by side along the lanes
// with narrow alleys between them. A compound that gets no house is taken down again. Returns the homes made.
int Gen::compoundHomes(int want) {
  if (want <= 0) return 0;
  int made = 0;
  const int target = std::max(1, want * 3 / 4);
  const bool dune = cArch == (int)cult::Archetype::Dune;
  const int savedMax = pathMax;
  pathMax = 14;
  std::vector<uint8_t> seen((size_t)W * H, 0);
  std::vector<std::pair<float, int>> order;
  for (auto& s : allStreet) {
    const size_t i = I(s.first, s.second);
    if (seen[i] || M.at(s.first, s.second) == Ground::Bridge || M.wall[i]) continue;
    seen[i] = 1;
    const float d = dist(s.first, s.second);
    if (!walled && d > 0.95f) continue;
    order.push_back({d + hfAt(s.first, s.second, 23u) * 0.3f, (int)i});
  }
  std::stable_sort(order.begin(), order.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first < b.first; });
  // the compound's tiles: open land of the town on one level; round them a margin of one that may be a lane (a
  // compound's back and sides may stand on the lanes), never anybody's house, wall or field
  auto fitsC = [&](int x0, int y0, int cw, int ch, int lv) {
    for (int y = y0 - 1; y <= y0 + ch - 1; y++)
      for (int x = x0 - 1; x <= x0 + cw; x++) {
        if (!in(x, y) || x < 1 || y < 1 || x >= W - 1 || y >= H - 1) return false;
        const size_t i = I(x, y);
        const bool margin = x < x0 || x >= x0 + cw || y < y0;
        if (margin) {
          if (M.bldgAt[i] >= 0 || M.wall[i] || mask[i] == K_FIELD || mask[i] == K_COMPOUND || mask[i] == K_SQUARE || front[i]) return false;
          continue;
        }
        if (mask[i] != K_NONE || M.bldgAt[i] >= 0 || M.wall[i] || noBuild[i] || water[i] || M.prop[i] || front[i]) return false;
        const Ground g = M.at(x, y);
        if (groundSolid(g) || g == Ground::Bridge || g == Ground::Swamp || lvl[i] != lv) return false;
        if (walled ? !inside[i] : dist(x, y) > 1.0f) return false;
        if (!reserved.empty() && reserved[i]) return false;
      }
    return true;
  };
  struct Was { size_t i; uint8_t k, g, p; };
  for (const auto& o : order) {
    if (made >= target) break;
    const int sx = o.second % W, sy = o.second / W;
    if (!isStreet(sx, sy) || get(sx, sy) == K_SQUARE || get(sx, sy - 1) != K_NONE) continue;
    const int y1 = sy - 1;   // the south wall, its gate right on the street
    const uint32_t h = hashAt(sx, sy, 0xC0B1u);
    bool built = false;
    for (int cw : {13, 12, 11, 10}) {
      if (built) break;
      const int ch = (cw >= 12 ? 9 : 8) + (int)(h % 2u);
      // the gate anywhere along the south wall but its corners (middle first, then out to either side)
      for (int gk = 0; gk < cw - 4 && !built; gk++) {
      const int gOff = cw / 2 + ((gk & 1) ? -(gk + 1) / 2 : gk / 2) + (int)((h >> 3) % 3u) - 1;
      if (gOff < 2 || gOff > cw - 3) continue;
      const int x0 = sx - gOff, y0 = y1 - ch + 1;
      if (!fitsC(x0, y0, cw, ch, lvl[I(sx, sy)])) continue;
      std::vector<Was> was;
      auto mark = [&](int x, int y, uint8_t k, Ground g, int prop) {
        const size_t i = I(x, y);
        was.push_back(Was{i, mask[i], M.ground[i], M.prop[i]});
        mask[i] = k;
        M.setG(x, y, g);
        M.prop[i] = (uint8_t)prop;
      };
      const int gx = x0 + gOff;
      const Ground court = dune && wealthFor(Building::House, IRect{x0, y0, cw, ch}) >= 2 ? Ground::Plaza : Ground::Dirt;
      for (int x = x0; x < x0 + cw; x++) {
        mark(x, y0, K_YARD, M.at(x, y0), (int)Prop::FenceH + 1);
        if (x == gx) mark(x, y1, K_YARD, Ground::Dirt, 0);
        else mark(x, y1, K_YARD, M.at(x, y1), (int)Prop::FenceH + 1);
      }
      for (int y = y0 + 1; y < y1; y++) {
        mark(x0, y, K_YARD, M.at(x0, y), (int)Prop::FenceV + 1);
        mark(x0 + cw - 1, y, K_YARD, M.at(x0 + cw - 1, y), (int)Prop::FenceV + 1);
      }
      // the family's houses along the north wall, doors on the court
      int n = 0;
      const int bh = ch >= 9 ? 3 : 2;
      for (int x = x0 + 1; x + 3 <= x0 + cw - 1;) {
        const int room = x0 + cw - 1 - x;
        const int bw = std::min(room, 3 + (int)(hashAt(x, y0, 0xC0B2u) % 2u));
        const Building t = (wealthFor(Building::House, IRect{x, y0, bw, bh}) >= 2 || (cArch == (int)cult::Archetype::Highland && hashAt(x, y0, 5u) % 3u == 0))
                               ? Building::StoneHouse : Building::House;
        if (bw >= 3 && tryPlace(t, Role::Villager, bw, bh, x + bw / 2, y0 + 1 + bh, false)) { n++; x += bw + 1; }
        else x++;
      }
      if (n == 0) {
        for (auto it = was.rbegin(); it != was.rend(); ++it) { mask[it->i] = it->k; M.ground[it->i] = it->g; M.prop[it->i] = it->p; }
        continue;
      }
      made += n;
      for (int y = y0 + 1; y < y1; y++)
        for (int x = x0 + 1; x < x0 + cw - 1; x++) if (M.bldgAt[I(x, y)] < 0) M.setG(x, y, court);
      // the court: a well (the dune's cistern) or a trough, and a tree in a corner; the rest of it is the family's yard
      const int wy = y1 - 1;
      for (int x = gx + 2; x < x0 + cw - 1; x++) {
        if (get(x, wy) != K_NONE || front[I(x, wy)] || M.prop[I(x, wy)]) continue;
        if (putSolid(x, wy, dune || hashAt(x0, y0, 7u) % 2u ? Prop::Well : Prop::Trough)) break;
      }
      for (int cxn : {x0 + 1, x0 + cw - 2}) {
        if (get(cxn, wy) != K_NONE || front[I(cxn, wy)] || cover[I(cxn, wy)] || M.prop[I(cxn, wy)]) continue;
        const Prop tree = bio == Biome::Desert || dune ? Prop::PalmTree : (bio == Biome::Snow || bio == Biome::Taiga ? Prop::PineTree : Prop::OakTree);
        if (treesF > 0.3f && putSolid(cxn, wy, tree)) break;
      }
      for (int y = y0 + 1; y < y1; y++)
        for (int x = x0 + 1; x < x0 + cw - 1; x++) if (get(x, y) == K_NONE) set(x, y, K_YARD);
      built = true;
      }
    }
  }
  pathMax = savedMax;
  return made;
}

// ------------------------------------------------------------------------------------------------ the palace compound
// Capitals (VISION_PLAN 15.8): the king's palace in its own walled compound, facing south over a courtyard of gardens
// with a fountain, the royal guard's barracks on one side, a gatehouse in the south wall flying the royal banners.
// The place is chosen before the streets (they wind round it), in the noble sector, as near the heart as it fits.
namespace {
constexpr int PAL_H = 7, BAR_W = 7, BAR_H = 4;
}

void Gen::placeCompound() {
  const int PW = 15 + 2 * rng.irange(2);
  const int CW = PW + 10, CH = PAL_H + 14;
  float bestD = 1e9f;
  for (int k = 0; k < 15 && bestD > 1e8f; k++) {
    float a = sector0 + (k == 0 ? 0 : ((k + 1) / 2) * 0.35f * (k % 2 ? 1 : -1));
    for (float d = 0.30f; d <= 0.72f; d += 0.02f) {
      int mx = cx + (int)std::floor(dcos(a) * rx * d + 0.5f), my = cy + (int)std::floor(dsin(a) * ry * d + 0.5f);
      IRect r{mx - CW / 2, my - CH / 2, CW, CH};
      bool ok = true;
      int lo = 7, hi = 0;
      for (int y = r.y - 3; y < r.y + r.h + 3 && ok; y++)
        for (int x = r.x - 3; x < r.x + r.w + 3 && ok; x++) {
          if (!ins(x, y) || water[I(x, y)]) { ok = false; break; }   // (M3: a marsh pool is drained for it: its lawns)
          lo = std::min(lo, (int)lvl[I(x, y)]); hi = std::max(hi, (int)lvl[I(x, y)]);
        }
      if (ok && hi - lo > 2) ok = false;   // a palace may level a couple of steps of a hillside, not a mountain
      // clear of the main square
      if (ok && r.x - 3 < cx + 9 && cx - 9 < r.x + r.w + 3 && r.y - 3 < cy + 8 && cy - 8 < r.y + r.h + 3) ok = false;
      if (ok) { compound = r; bestD = d; break; }
    }
  }
  if (compound.w == 0) return;
  // the palace's terrace: the compound and a tile round it levelled to one height (the view shows the step at its edge)
  {
    const int lv = lvl[I(compound.x + compound.w / 2, compound.y + compound.h - 1)];
    for (int y = compound.y - 1; y < compound.y + compound.h + 1; y++)
      for (int x = compound.x - 1; x < compound.x + compound.w + 1; x++)
        if (in(x, y)) lvl[I(x, y)] = (uint8_t)lv;
  }
  for (int y = compound.y - 1; y < compound.y + compound.h + 1; y++)
    for (int x = compound.x - 1; x < compound.x + compound.w + 1; x++)
      if (in(x, y)) noBuild[I(x, y)] |= 2;
  for (int y = compound.y; y < compound.y + compound.h; y++)
    for (int x = compound.x; x < compound.x + compound.w; x++) set(x, y, K_COMPOUND);
}

void Gen::buildCompound() {
  if (compound.w == 0) return;
  const int X0 = compound.x, Y0 = compound.y, CW = compound.w, CH = compound.h;
  const int PW = CW - 10;
  // the grounds: lawns (sand in the desert, snow in the north), cleared
  const Ground lawn = base == Ground::Grass ? Ground::Meadow : base;
  for (int y = Y0; y < Y0 + CH; y++)
    for (int x = X0; x < X0 + CW; x++) { M.setG(x, y, lawn); M.setP(x, y, 0); }
  // the wall: a rectangle (the wall art puts towers on its corners); the gatehouse in the middle of the south run
  for (int x = X0; x < X0 + CW; x++) { M.wall[I(x, Y0)] = wallByte; M.wall[I(x, Y0 + CH - 1)] = wallByte; }
  for (int y = Y0; y < Y0 + CH; y++) { M.wall[I(X0, y)] = wallByte; M.wall[I(X0 + CW - 1, y)] = wallByte; }
  const int gx = X0 + CW / 2 - 1, gy = Y0 + CH - 1;
  for (int k = 0; k < 3; k++) { M.wall[I(gx + k, gy)] = 0; M.setG(gx + k, gy, Ground::Plaza); set(gx + k, gy, K_SQUARE); }
  O.gates.push_back({gx, gy});
  O.wallGaps.push_back(IRect{gx, gy, 3, 1});
  gateBearing.push_back(-2);
  // the palace, its terrace and the walkway from the gate
  const int px = X0 + (CW - PW) / 2, py = Y0 + 3;
  palaceIdx = putBldg(Building::Palace, IRect{px, py, PW, PAL_H}, Role::King, 2);
  const int doorX = M.bldgs[(size_t)palaceIdx].doorX();
  const int terrace = py + PAL_H;   // the row before the palace front
  for (int y = terrace; y <= terrace + 1; y++)
    for (int x = px + 1; x < px + PW - 1; x++) { set(x, y, K_SQUARE); M.setG(x, y, Ground::Plaza); }
  for (int y = terrace + 2; y < gy; y++)
    for (int x = gx; x < gx + 3; x++) { set(x, y, K_SQUARE); M.setG(x, y, Ground::Plaza); }
  (void)doorX;
  // the fountain court: a round widening of the walkway halfway down, the fountain in its middle
  const int fy = terrace + 5, fx = gx + 1;   // the court halfway down, clear of the gatehouse
  for (int y = fy - 2; y <= fy + 2; y++)
    for (int x = fx - 3; x <= fx + 3; x++) {
      int ddx = x - fx, ddy = y - fy;
      if (ddx * ddx * 4 + ddy * ddy * 9 > 40) continue;
      set(x, y, K_SQUARE);
      M.setG(x, y, Ground::Plaza);
    }
  M.setProp(fx, fy, Prop::Fountain);
  // the royal guard's barracks on one side of the courtyard, its door on a path to the walkway
  const bool west = rng.f() < 0.5f;
  const int bx = west ? X0 + 2 : X0 + CW - 2 - BAR_W, by = terrace + 4;
  barracksIdx = putBldg(Building::Barracks, IRect{bx, by, BAR_W, BAR_H}, Role::Guard, 2);
  {
    const int bdx = M.bldgs[(size_t)barracksIdx].doorX(), bdy = by + BAR_H;
    for (int x = std::min(bdx, gx + 1); x <= std::max(bdx, gx + 1); x++) { if (get(x, bdy) != K_SQUARE) { set(x, bdy, K_YARD); M.setG(x, bdy, Ground::Road); } }
  }
  // gardens: clipped bushes along the walkway, flower beds on the lawns, trees in the corners, statues by the gate
  const Biome gb = bio;
  const Prop tree = gb == Biome::Desert ? Prop::PalmTree : (gb == Biome::Snow || gb == Biome::Taiga ? Prop::PineTree : Prop::OakTree);
  auto lawnFree = [&](int x, int y) {
    return in(x, y) && x > X0 && x < X0 + CW - 1 && y > Y0 && y < Y0 + CH - 1 && get(x, y) == K_COMPOUND && M.bldgAt[I(x, y)] < 0 &&
           !M.prop[I(x, y)] && !front[I(x, y)];
  };
  for (int y = terrace + 2; y < gy - 1; y++)
    for (int x = X0 + 2; x < X0 + CW - 2; x++) {
      if (!lawnFree(x, y) || cover[I(x, y)]) continue;
      bool byWalk = get(x - 1, y) == K_SQUARE || get(x + 1, y) == K_SQUARE;
      if (byWalk && (y & 1) == 0) M.setProp(x, y, Prop::Bush);
    }
  // (M2 fixer) the flower beds: a few ragged ovals of dark soil on each lawn, each planted mostly in one colour with
  // another mixed through it and a gap here and there (they were a rigid grid of identical tufts on bare lawn). In
  // the snow they are clumps of evergreen shrubs on the white.
  {
    const bool winter = bio == Biome::Snow;
    static const Prop fl[3] = {Prop::Flowers1, Prop::Flowers2, Prop::Flowers3};
    Rng br(hash32(P.seed ^ 0xBED5u));   // (its own stream: the rest of the town keeps its draws)
    for (int side = 0; side < 2; side++) {
      const int lx0 = side == 0 ? X0 + 3 : gx + 5, lx1 = side == 0 ? gx - 3 : X0 + CW - 4;
      const int ly0 = terrace + 3, ly1 = gy - 3;
      if (lx1 - lx0 < 2 || ly1 - ly0 < 2) continue;
      const int nb = 2 + br.irange(2);
      for (int k = 0; k < nb; k++) {
        const int bx = lx0 + br.irange(lx1 - lx0 + 1), by = ly0 + br.irange(ly1 - ly0 + 1);
        const float rxb = 1.3f + br.f() * 1.3f, ryb = 0.9f + br.f() * 0.8f;
        const int main = br.irange(3), accent = (main + 1 + br.irange(2)) % 3;
        const uint32_t bs = br.next();
        for (int y = by - 3; y <= by + 3; y++)
          for (int x = bx - 4; x <= bx + 4; x++) {
            if (!lawnFree(x, y) || cover[I(x, y)]) continue;
            if (get(x - 1, y) == K_SQUARE || get(x + 1, y) == K_SQUARE || get(x - 2, y) == K_SQUARE || get(x + 2, y) == K_SQUARE) continue;
            const uint32_t h = hash32(bs ^ ((uint32_t)x * 73856093u) ^ ((uint32_t)y * 19349663u));
            const float ex = (x - bx) / rxb, ey = (y - by) / ryb;
            if (ex * ex + ey * ey + (h & 255) / 255.0f * 0.45f > 1.15f) continue;
            if (winter) {
              if (h % 3 != 0) M.setProp(x, y, Prop::SnowBush);
              continue;
            }
            M.setG(x, y, Ground::Dirt);
            if ((h >> 8) % 6 == 0) continue;   // a gap in the planting
            M.setProp(x, y, fl[(h >> 12) % 4 == 0 ? accent : main]);
          }
      }
    }
  }
  int corners[4][2] = {{X0 + 2, terrace + 2}, {X0 + CW - 3, terrace + 2}, {X0 + 2, gy - 2}, {X0 + CW - 3, gy - 2}};
  for (auto& c : corners) {
    int x = c[0], y = c[1];
    bool ok = lawnFree(x, y) && !cover[I(x, y)] && lawnFree(x, y - 1);
    for (int oy = -1; oy <= 1 && ok; oy++) for (int ox = -1; ox <= 1; ox++) if (M.bldgAt[I(x + ox, y + oy)] >= 0 || front[I(x + ox, y + oy)]) ok = false;
    if (ok) { M.setP(x, y, 0); M.setProp(x, y, tree); }
  }
  for (int s : {-2, 4}) if (lawnFree(gx + s, gy - 2) && !cover[I(gx + s, gy - 2)]) M.setProp(gx + s, gy - 2, Prop::Statue);
  // the royal banners: either side of the palace door, along the walkway, and before the gate
  for (int s : {-3, 3}) { int x = doorX + s; if (get(x, terrace) == K_SQUARE && !M.prop[I(x, terrace)]) M.setProp(x, terrace, Prop::Banner); }
  for (int y = terrace + 3; y < gy - 1; y += 3)
    for (int s : {-1, 3}) if (lawnFree(gx + s, y) || (get(gx + s, y) == K_COMPOUND && !M.prop[I(gx + s, y)] && M.bldgAt[I(gx + s, y)] < 0)) { M.setP(gx + s, y, 0); M.setProp(gx + s, y, Prop::Banner); }
  // the royal guard: two before the gate, two in the court, two at the palace door
  addSpawn(Role::Guard, gx - 1, gy + 1);
  addSpawn(Role::Guard, gx + 3, gy + 1);
  addSpawn(Role::Guard, fx - 3, fy);
  addSpawn(Role::Guard, fx + 3, fy);
  addSpawn(Role::Guard, doorX - 2, terrace + 1);
  addSpawn(Role::Guard, doorX + 2, terrace + 1);
}


// the way out of the palace compound: a paved approach from its gate to the nearest street of the town's network.
// (M1: run after pruneStreets, so it always meets a street that leads somewhere; before, it could join a stub that
// the pruning then removed, leaving the gate opening onto bare grass)
void Gen::compoundApproach() {
  if (compound.w == 0) return;
  const int gx = compound.x + compound.w / 2 - 1, gy = compound.y + compound.h - 1;
  {
    std::vector<int> prev((size_t)W * H, -2);
    std::queue<int> q;
    for (int k = 0; k < 3; k++) { int x = gx + k, y = gy + 1; if (in(x, y)) { prev[I(x, y)] = -1; q.push((int)I(x, y)); } }
    int found = -1;
    while (!q.empty() && found < 0) {
      int c = q.front(); q.pop();
      int x = c % W, y = c / W;
      if (isStreet(x, y) && !inCompound(x, y)) { found = c; break; }
      for (int d = 0; d < 4; d++) {
        int nx = x + D4X[d], ny = y + D4Y[d];
        const bool marsh = stilt && !water[I(nx, ny)] && M.at(nx, ny) == Ground::Water;   // (M3: a boardwalk will cross it)
        if (!ins(nx, ny) || prev[I(nx, ny)] != -2 || wallAt(nx, ny) || inCompound(nx, ny) || (groundSolid(M.at(nx, ny)) && !marsh)) continue;
        prev[I(nx, ny)] = c;
        q.push((int)I(nx, ny));
      }
    }
    std::vector<int> path;
    for (int c = found >= 0 ? prev[(size_t)found] : -1; c >= 0; c = prev[(size_t)c]) path.push_back(c);
    for (int c : path) paintStreet(c % W, c / W, K_MAIN, Ground::Road);
    for (int k = 0; k < 3; k++) for (int y = gy + 1; y <= gy + 2; y++) paintStreet(gx + k, y, K_MAIN, Ground::Plaza);
  }
}

}  // namespace town
}  // namespace ew
