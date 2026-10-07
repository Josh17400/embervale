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
  // (M3b: every seat of power flies the colours, whatever its society built it as; and the lodges of sworn warriors)
  if (b.type != Building::Keep && b.type != Building::Palace && b.type != Building::Barracks && b.type != Building::Inn &&
      b.type != Building::Lodge && !(b.civic & bld::CIVIC_SEAT)) return;
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
  // M3: the culture's architecture for this building (VISION_PLAN 5.7), as rich as its quarter (wealthFor); M3b: the
  // builder reads the same wealth (Bldg::wealth: size and finery)
  b.wealth = (uint8_t)wealthFor(type, r);
  if (C.culture) {
    b.arch = cult::buildingArch(*C.culture, (int)bio, b.urban, b.wealth, b.seed, b.roof);
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
  // M3b: what the society asked the builder for (the form, the civic role, the seat), then the storeys the builder
  // resolves for that form (a tent or a yurt keeps one storey, a spire rises): the interior gets as many floors as the
  // exterior shows.
  b.form = curForm;
  b.civic = curCivic;
  b.seat = curSeat;
  kingdomColours(b);
  {
    const int st = std::clamp((int)bldgBlueprint(b).facts.storeys, 1, 4);
    if (st != (int)b.storeys) {
      b.storeys = (uint8_t)st;
      if (type != Building::Palace && type != Building::Barracks) b.hearth = bldgHearthV7(type, st, hash32(hs + 77u));
    }
  }
  storeys = b.storeys;
  M.bldgs.push_back(b);
  const int bi = (int)M.bldgs.size() - 1;
  // (fixer) the way in stands over the marsh on boards: the door's tile (and an open front's bays) is never open water
  // (a stilt house whose doorstep was water could not be entered)
  for (int x : bldgEntryColumns(M.bldgs[(size_t)bi]))
    if (in(x, r.y + r.h - 1) && groundWater(M.at(x, r.y + r.h - 1))) M.setG(x, r.y + r.h - 1, Ground::Bridge);
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

// landmarks and services, nearest the heart first. M3b: a culture's settlement builds what its society requires for
// its tier (societyServices); without a culture the classic lists below.
void Gen::services() {
  if (hasSoc) societyServices();
  else classicServices();
}

// (classic) VISION_PLAN 15.8: every village has an inn or tavern, a well or green and a shop or smith; towns an inn,
// shop, smith and temple; cities several inns and shops, a temple, a mage tower and the jarl's keep
void Gen::classicServices() {
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

// M3b (VISION_PLAN 15.14): the society's required buildings for the tier (cult::requiredBuildings), mapped onto wants:
// the keeper to the owner's Role, the quarter to a district and how near the heart, the gate quarter's buildings by
// the gates the roads come in at; the capital's seat (and its royal guard's barracks) are the seat compound's
// (buildCompound), except a merchant republic's guildhall, which faces the market square. The M1 economy (mill,
// specialisation, trades) is placed beside them as before.
void Gen::societyServices() {
  std::vector<Want> want;
  const std::vector<cult::BuildingReq> reqs = cult::requiredBuildings(soc, *C.culture, tier, (int)P.archetype, P.seed);
  auto roleOf = [](cult::Keeper k) {
    switch (k) {
      case cult::Keeper::Ruler: return Role::King;
      case cult::Keeper::Lord: return Role::Jarl;
      case cult::Keeper::Innkeeper: return Role::Innkeeper;
      case cult::Keeper::Priest: return Role::Priest;
      case cult::Keeper::Merchant: return Role::Merchant;
      case cult::Keeper::Smith: return Role::Smith;
      case cult::Keeper::Guard: return Role::Guard;
      case cult::Keeper::Mage: return Role::Mage;
      default: return Role::Villager;
    }
  };
  // the gates the roads come in at (cities): the gate quarter's buildings stand a little inside them, one per gate
  std::vector<std::pair<int, int>> gates;
  for (size_t k = 0; k < O.wallGaps.size(); k++) {
    if (k >= gateBearing.size() || gateBearing[k] < 0) continue;
    const IRect& g = O.wallGaps[k];
    gates.push_back({g.x + g.w / 2, g.y + g.h / 2});
  }
  int gateUse = 0;
  // the main square's radius (the merchant republic's seat and exchange face it from the north)
  const float sqR = !squares.empty() ? squares[0].r : (capital ? 8.2f : (city ? 7.0f : (town ? 5.4f : 3.2f)));
  const int northEdge = mktSide == 0 && mktZone.w > 0 ? mktZone.y - 1 : cy - (int)(sqR * 1.15f) - 1;
  for (const cult::BuildingReq& q : reqs) {
    const bool seat = (q.civic & bld::CIVIC_SEAT) != 0;
    // the seat compound holds the capital's ruler and its royal guard
    if (compound.w > 0 && seat && q.keeper == cult::Keeper::Ruler) continue;
    if (compound.w > 0 && barracksIdx >= 0 && q.purpose == Building::Barracks) continue;
    for (int n = 0; n < std::max<int>(1, q.count); n++) {
      Want w{q.purpose, roleOf(q.keeper), 4, 3, 0.6f, q.required, false, District::COUNT, -1, -1};
      w.form = q.form;
      w.civic = q.civic;
      w.seat = seat ? (uint8_t)((int)soc.seat + 1) : 0;
      w.w = q.wTiles ? q.wTiles : 4;
      w.h = q.hTiles ? q.hTiles : 3;
      // village footprints stay village-sized (the plot between the green and the fields)
      if (village) { w.w = std::min(w.w, 6); w.h = std::min(w.h, 3); }
      else if (town) { w.w = std::min(w.w, 8); w.h = std::min(w.h, 4); }
      switch (q.quarter) {
        case cult::Quarter::Heart: w.near = village ? 0.5f : (town ? 0.45f : 0.35f); w.d = District::Centre; break;
        case cult::Quarter::Market: w.near = village ? 0.75f : (town ? 0.5f : 0.4f); w.d = District::Centre; break;
        case cult::Quarter::Noble: w.near = 0.55f; w.north = city; w.d = District::Noble; break;
        case cult::Quarter::Sacred: w.near = village ? 0.75f : 0.65f; w.d = District::Temple; break;
        case cult::Quarter::Crafts: w.near = village ? 0.65f : (town ? 0.7f : 0.75f); w.d = District::Crafts; break;
        case cult::Quarter::Gate: w.near = town ? 0.8f : 1.0f; break;
        case cult::Quarter::Edge: w.near = 1.0f; break;
        default: w.near = 0.6f; break;
      }
      if (!city) w.d = District::COUNT;
      // a city's gate quarter: by the gates, a little inside (the barracks further in)
      if (city && q.quarter == cult::Quarter::Gate && gateUse < (int)gates.size()) {
        const auto& g = gates[(size_t)gateUse++];
        const float a = datan2((float)(g.second - cy), (float)(g.first - cx));
        const int in = q.purpose == Building::Barracks ? 12 : 9;
        w.px = g.first - (int)(dcos(a) * in); w.py = g.second - (int)(dsin(a) * in);
        w.d = District::COUNT;
      }
      // the merchant republic's seat faces its market square from the north, the exchange beside it
      if (soc.seat == cult::Seat::GuildExchange && city && (seat || q.purpose == Building::Exchange)) {
        w.px = seat ? cx : cx + (int)(sqR * 1.2f) + 4;
        w.py = northEdge;
        w.north = false; w.d = District::COUNT;
        w.face = true;
      }
      want.push_back(w);
    }
  }
  // M3 radial (the steppe camp, the sun temple, the spires): the seat, the temple or the gathering place faces the
  // plaza from its north side, the rings round them both
  if (style == cult::Layout::Radial && !want.empty()) {
    const bool templeFirst = cArch == (int)cult::Archetype::SunTemple || cArch == (int)cult::Archetype::Starspire;
    const float sq = capital ? 8.2f : (city ? 7.0f : (town ? 5.4f : 3.2f));
    for (Want& w : want) {
      const bool lead = city ? (w.civic & bld::CIVIC_SEAT) != 0 : (templeFirst && town ? w.b == Building::Temple : w.b == Building::Inn);
      if (!lead || w.px >= 0) continue;
      w.px = cx; w.py = mktSide == 0 && mktZone.w > 0 ? mktZone.y - 1 : cy - (int)(sq * 1.15f) - 1;
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
  // the required first (in the society's order), then the ones placed when there is room
  for (int pass = 0; pass < 2; pass++)
    for (const Want& w : want) {
      if (w.req != (pass == 0)) continue;
      const int before = (int)M.bldgs.size();
      placeWant(w);
      if ((int)M.bldgs.size() == before) continue;
      if (w.b == Building::Keep && keepIdx < 0) keepIdx = before;
      if ((w.civic & bld::CIVIC_SEAT) && seatIdx < 0) seatIdx = before;
    }
}

// a landmark on the main square (the doge's guildhall, the exchange): its doorstep on a square tile, its body on the
// open land behind the square's edge, the nearest such spot to (px, py) along the edge (a few rows up or down)
bool Gen::faceSquare(const Want& w) {
  if (w.px < 0) return false;
  for (int r = 0; r <= 14; r++)
    for (int s = 0; s < 2; s++) {
      const int ax = w.px + (s ? -r : r);
      if (s && !r) continue;
      for (int ay = w.py - 3; ay <= w.py + 4; ay++) {
        if (get(ax, ay) != K_SQUARE || get(ax, ay - 1) == K_SQUARE) continue;   // (the square's top edge in this column)
        if (tryPlace(w.b, w.r, w.w, w.h, ax, ay, true)) return true;
      }
    }
  return false;
}

bool Gen::placeWant(const Want& w0) {
  Want w = w0;
  bool ok = false;
  curForm = w.form; curCivic = w.civic; curSeat = w.seat;
  struct Clear { Gen& g; ~Clear() { g.curForm = g.curCivic = g.curSeat = 0; } } clear{*this};
  if (w.face && faceSquare(w)) return true;
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

// ------------------------------------------------------------------------------------------------ the seat compound
// Capitals (VISION_PLAN 15.8, M3b 15.14): the ruler's seat in its own grounds, laid out by the society's seat of power
// (cult::Seat), facing south over its court to an entrance in the middle of the south side:
//   Castle         the classic walled palace compound: palace, royal guard's barracks, fountain court, gardens
//   CourtPalace    walled courts and formal parterres: the courtyard palace, fountains on its axis, rows of trees
//   GreatHall      the jarl's long hall and the huscarls' longhouses in a stockade, a fire pit in the yard
//   TentCourt      no wall: the khan's great tent in a ring of court tents and banners, horse lines and corrals
//   TempleComplex  a precinct wall round the temple court, the stepped temple, the priests' houses, braziers
//   CouncilSpire   the council spire in its hedged gardens, fountains and statues on white walks
//   TreePalace     the round palace behind the elder tree of its grove, a ring of old trees
//   StiltHall      the elders' long hall on stilts over its pools, boardwalks out to the town
// The place is chosen before the streets (they wind round it), in the noble sector, as near the heart as it fits. A
// merchant republic has no compound: its doge's guildhall faces the market square (societyServices).
namespace {
constexpr int BAR_W = 7, BAR_H = 4;
}

bool Gen::compoundWalled() const {
  if (!hasSoc) return true;
  switch ((cult::Seat)seatKind) {
    case cult::Seat::TentCourt: case cult::Seat::CouncilSpire: case cult::Seat::TreePalace: case cult::Seat::StiltHall: return false;
    default: return true;
  }
}

void Gen::placeCompound() {
  if (hasSoc && (cult::Seat)seatKind == cult::Seat::GuildExchange) return;
  const int k2 = rng.irange(2);
  int CW = 0, CH = 0;
  switch (hasSoc ? (cult::Seat)seatKind : cult::Seat::Castle) {
    // (a walled compound keeps within the seat's palace + 6 tiles each side and 22 rows from its top + 3: the city
    // wall checks (rpg_test, test_arch) tell the compound's own wall from the city's ring by that box)
    case cult::Seat::CourtPalace: palW = 15 + 2 * k2; palH = 8; CW = palW + 12; CH = 22; break;
    case cult::Seat::GreatHall: palW = 15 + 2 * k2; palH = 5; CW = palW + 12; CH = 22; break;
    case cult::Seat::TentCourt: palW = 9 + 2 * k2; palH = 6; CW = 31; CH = 25; break;
    case cult::Seat::TempleComplex: palW = 13 + 2 * k2; palH = 7; CW = palW + 12; CH = 22; break;
    case cult::Seat::CouncilSpire: palW = 9; palH = 6 + k2; CW = 27; CH = 27; break;
    case cult::Seat::TreePalace: palW = 11 + 2 * k2; palH = 7; CW = 27; CH = 24; break;
    case cult::Seat::StiltHall: palW = 15 + 2 * k2; palH = 5; CW = palW + 10; CH = 21; break;
    default: palW = 15 + 2 * k2; palH = 7; CW = palW + 10; CH = palH + 14; break;
  }
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

// the gardens of a castle or court palace: flower beds, clipped bushes along the walk, trees in the corners
void Gen::compoundGrounds(int X0, int Y0, int CW, int CH, int gx, int gy, int terrace) {
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
  // another mixed through it and a gap here and there. In the snow they are clumps of evergreen shrubs on the white.
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

void Gen::buildCompound() {
  if (compound.w == 0) return;
  const int X0 = compound.x, Y0 = compound.y, CW = compound.w, CH = compound.h;
  const cult::Seat seat = hasSoc ? (cult::Seat)seatKind : cult::Seat::Castle;
  const bool walledC = compoundWalled();
  const int PW = palW, PH = palH;
  const int gx = X0 + CW / 2 - 1, gy = Y0 + CH - 1;   // the entrance: the middle three tiles of the south side
  const Biome gb = bio;
  const bool dry = gb == Biome::Desert || cArch == (int)cult::Archetype::Dune;
  const bool cold = gb == Biome::Snow || gb == Biome::Taiga;
  const Prop tree = dry ? Prop::PalmTree : (cold ? Prop::PineTree : Prop::OakTree);
  // the grounds: lawns (sand in the desert, snow in the north); a tent court's trampled steppe; a precinct's paving;
  // the elders' pools
  {
    const Ground lawn = base == Ground::Grass ? Ground::Meadow : base;
    Ground g = lawn;
    if (seat == cult::Seat::TentCourt || seat == cult::Seat::GreatHall) g = base == Ground::Grass ? Ground::Grass : base;
    if (seat == cult::Seat::StiltHall) g = base == Ground::Grass ? Ground::Grass : base;
    for (int y = Y0; y < Y0 + CH; y++)
      for (int x = X0; x < X0 + CW; x++) { M.setG(x, y, g); M.setP(x, y, 0); }
  }
  // the enclosure: a wall (the culture's: a castle curtain, a stockade, a precinct wall) with its gate in the middle of
  // the south run; a hedge (the culture's fence) round gardens; open ground for a tent court, a grove, the pools
  if (walledC) {
    for (int x = X0; x < X0 + CW; x++) { M.wall[I(x, Y0)] = wallByte; M.wall[I(x, Y0 + CH - 1)] = wallByte; }
    for (int y = Y0; y < Y0 + CH; y++) { M.wall[I(X0, y)] = wallByte; M.wall[I(X0 + CW - 1, y)] = wallByte; }
    for (int k = 0; k < 3; k++) { M.wall[I(gx + k, gy)] = 0; M.setG(gx + k, gy, Ground::Plaza); set(gx + k, gy, K_SQUARE); }
    O.gates.push_back({gx, gy});
    O.wallGaps.push_back(IRect{gx, gy, 3, 1});
    gateBearing.push_back(-2);
  } else {
    for (int k = 0; k < 3; k++) { M.setG(gx + k, gy, Ground::Plaza); set(gx + k, gy, K_SQUARE); }
  }
  // helpers
  auto lawnFree = [&](int x, int y) {
    return in(x, y) && x > X0 && x < X0 + CW - 1 && y > Y0 && y < Y0 + CH - 1 && get(x, y) == K_COMPOUND && M.bldgAt[I(x, y)] < 0 &&
           !M.prop[I(x, y)] && !front[I(x, y)] && !groundWater(M.at(x, y));
  };
  // (M3b fixer) the forecourts' own stream (each capital its own layout; the town's other draws unchanged)
  Rng cr(hash32(P.seed ^ 0xF0C0E7u));
  // open water laid in the grounds (a pool, a canal, a rill): lawn only, never under a sprite
  auto pond = [&](int x, int y) {
    if (!lawnFree(x, y) || cover[I(x, y)]) return false;
    M.setG(x, y, Ground::Water);
    M.setP(x, y, 0);
    O.pools.push_back((int)I(x, y));
    return true;
  };
  auto walk = [&](int x, int y, Ground g) { if (in(x, y) && !M.wall[I(x, y)] && M.bldgAt[I(x, y)] < 0) { set(x, y, K_SQUARE); M.setG(x, y, g); M.setP(x, y, 0); } };
  auto put = [&](Building t, IRect r, Role o, uint8_t form, uint8_t civic, uint8_t seatB) {
    curForm = form; curCivic = civic; curSeat = seatB;
    const uint32_t hs = hash32(P.seed ^ ((uint32_t)M.bldgs.size() * 0x9E3779B1u) ^ 0x5707EE5u);
    const int bi = putBldg(t, r, o, storeysFor(t, r.w, r.h, hs));
    curForm = curCivic = curSeat = 0;
    return bi;
  };
  // the path from a door to the walkway, `along` the row before the door
  auto doorPath = [&](int bi, Ground g) {
    const Bldg& b = M.bldgs[(size_t)bi];
    const int dx = b.doorX(), dy = b.r.y + b.r.h;
    for (int x = std::min(dx, gx + 1); x <= std::max(dx, gx + 1); x++) if (get(x, dy) != K_SQUARE) { set(x, dy, K_YARD); M.setG(x, dy, g); M.setP(x, dy, 0); }
  };
  // what the society garrisons in the grounds (the royal guard's barracks for a standing army; the khan's riders in a
  // tent; huscarls, temple guards and wardens live in the seat itself)
  bool garrison = !hasSoc;
  uint8_t garrisonForm = 0;
  if (hasSoc)
    switch (soc.military) {
      case cult::Military::Levy: case cult::Military::Knights: case cult::Military::Legions: case cult::Military::Mercenaries: garrison = true; break;
      case cult::Military::HorseArchers: garrison = true; garrisonForm = (uint8_t)bld::Form::Tent; break;
      default: break;
    }
  const uint8_t seatId = hasSoc ? (uint8_t)(seatKind + 1) : 0;
  const uint8_t seatForm = hasSoc ? cult::seatForm(soc.seat, false) : 0;
  const Building seatPurpose = Building::Palace;
  // the seat building: at the head of the grounds (a tent court's and a grove's further in, their court round them)
  int px = X0 + (CW - PW) / 2, py = Y0 + 3;
  if (seat == cult::Seat::TentCourt) py = Y0 + 7;
  if (seat == cult::Seat::TreePalace) py = Y0 + 2;
  if (seat == cult::Seat::StiltHall) py = Y0 + 3;
  palaceIdx = put(seatPurpose, IRect{px, py, PW, PH}, Role::King, seatForm, hasSoc ? (uint8_t)bld::CIVIC_SEAT : 0, seatId);
  seatIdx = palaceIdx;
  {
    Bldg& pb = M.bldgs[(size_t)palaceIdx];
    if (seat == cult::Seat::StiltHall && pb.styled) { pb.arch.foundation = art::Foundation::Stilts; pb.arch.stilts = true; }
  }
  const int doorX = M.bldgs[(size_t)palaceIdx].doorX();
  const int terrace = py + PH;   // the row before the seat's front
  const Ground walkG = seat == cult::Seat::TentCourt || seat == cult::Seat::GreatHall ? Ground::Dirt : Ground::Plaza;
  // the terrace before the front and the walk to the entrance
  for (int y = terrace; y <= terrace + 1; y++)
    for (int x = px + 1; x < px + PW - 1; x++) walk(x, y, walkG);
  for (int y = terrace + 2; y < gy; y++)
    for (int x = gx; x < gx + 3; x++) walk(x, y, walkG);
  int guardSpots[6][2] = {{gx - 1, gy + 1}, {gx + 3, gy + 1}, {gx - 1, gy - 2}, {gx + 3, gy - 2}, {doorX - 2, terrace + 1}, {doorX + 2, terrace + 1}};
  auto banner = [&](int x, int y) { if (lawnFree(x, y) && !cover[I(x, y)]) { M.setP(x, y, 0); M.setProp(x, y, Prop::Banner); return true; } return false; };
  auto propAt = [&](int x, int y, Prop p) { if (lawnFree(x, y) && !cover[I(x, y)]) { M.setProp(x, y, p); return true; } return false; };
  auto fenceRing = [&](int x0, int y0, int w, int h, int gapX, int gapW) {
    // a closed ring of the culture's fence (FenceH along the rows, FenceV down the sides), a gap in its south run
    auto ok = [&](int x, int y) { return in(x, y) && get(x, y) == K_COMPOUND && M.bldgAt[I(x, y)] < 0 && !M.prop[I(x, y)] && !front[I(x, y)]; };
    for (int x = x0; x < x0 + w; x++) {
      if (ok(x, y0)) M.setProp(x, y0, Prop::FenceH);
      if ((x < gapX || x >= gapX + gapW) && ok(x, y0 + h - 1)) M.setProp(x, y0 + h - 1, Prop::FenceH);
    }
    for (int y = y0 + 1; y < y0 + h - 1; y++) {
      if (ok(x0, y)) M.setProp(x0, y, Prop::FenceV);
      if (ok(x0 + w - 1, y)) M.setProp(x0 + w - 1, y, Prop::FenceV);
    }
  };
  // a building of the grounds fits here: its footprint and a ring round it free, its sprite over nobody's front, its
  // own front and doorstep under nobody's sprite, inside the grounds
  auto canPut = [&](const IRect& r, Building t) {
    const int rise = townRiseTiles(t, 2);   // (the builder may give it two storeys)
    if (r.x < X0 + 1 || r.y < Y0 + 1 || r.x + r.w > X0 + CW - 1 || r.y + r.h + 2 > gy) return false;
    for (int y = r.y - 1; y <= r.y + r.h + 1; y++)
      for (int x = r.x - 1; x <= r.x + r.w; x++) if (!in(x, y) || M.bldgAt[I(x, y)] >= 0 || get(x, y) == K_SQUARE) return false;
    for (int y = r.y - rise; y <= r.y + r.h; y++)
      for (int x = r.x - 1; x <= r.x + r.w; x++) if (in(x, y) && front[I(x, y)]) return false;
    for (int x = r.x; x < r.x + r.w; x++) if (cover[I(x, r.y + r.h - 1)]) return false;
    for (int y = r.y + r.h; y <= r.y + r.h + 1; y++)
      for (int x = r.x + r.w / 2 - 1; x <= r.x + r.w / 2 + 1; x++) if (cover[I(x, y)]) return false;
    return true;
  };
  // a formal parterre in [x0, x1] x [y0, y1]: a clipped hedge round its edge, beds of flowers inside in bands of two
  // colours (in the snow: evergreen shrubs on the white); tiles the walks, a building's front or its sprite hold are
  // left as lawn
  auto parterre = [&](int x0, int y0, int x1, int y1, uint32_t k) {
    if (x1 - x0 < 3 || y1 - y0 < 2) return;
    static const Prop fl[3] = {Prop::Flowers1, Prop::Flowers2, Prop::Flowers3};
    const int a = (int)(hashAt(x0, y0, k) % 3u), b = (a + 1 + (int)(hashAt(x0, y0, k + 1) % 2u)) % 3;
    const bool winter = bio == Biome::Snow;
    // (a bed a building, its path or its sprite cuts into is left as lawn: no broken hedges)
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) if (!lawnFree(x, y) || cover[I(x, y)]) return;
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        if (get(x - 1, y) == K_SQUARE || get(x + 1, y) == K_SQUARE || get(x, y + 1) == K_SQUARE) continue;   // (a step off the walks)
        const bool edge = x == x0 || x == x1 || y == y0 || y == y1;
        if (edge) { M.setProp(x, y, winter ? Prop::SnowBush : Prop::Bush); continue; }
        if (winter) { if (((x + y) & 1) == 0) M.setProp(x, y, Prop::SnowBush); continue; }
        M.setG(x, y, Ground::Dirt);
        M.setProp(x, y, fl[((y - y0) & 1) ? a : b]);
      }
  };
  switch (seat) {
    // ---------------------------------------------------------------- the castle and the court palace
    default:
    case cult::Seat::Castle:
    case cult::Seat::CourtPalace: {
      const bool court = seat == cult::Seat::CourtPalace;
      // (M3b fixer) each people's forecourt its own, and every capital its own variant of it (the shared template of
      // banners, a tiered fountain, parterres and paired trees stood before every palace): the jade court paved from
      // wall to wall with the golden-water stream under the walk, censers and stone lions; the dune palace's chahar
      // bagh of four green quarters under palms divided by rills; the imperial court's twin canals lined with statues
      // and the legions' parade ground; the highland chief's trodden bailey; the heartland castle's fountain court with
      // its gardens, an orchard or a tiltyard
      using CA = cult::Archetype;
      const bool jade = court && cArch == (int)CA::Jade, dune = court && cArch == (int)CA::Dune, imperial = court && cArch == (int)CA::Imperial;
      const bool highland = !court && cArch == (int)CA::Highland;
      const int fx = gx + 1;
      int midY = (terrace + 3 + gy - 2) / 2;
      // the royal guard's barracks on one side of the courtyard, its door on a path to the walkway
      const bool west = rng.f() < 0.5f;
      if (garrison) {
        const int bx = west ? X0 + 2 : X0 + CW - 2 - BAR_W, by = terrace + 4;
        barracksIdx = put(Building::Barracks, IRect{bx, by, BAR_W, BAR_H}, Role::Guard, garrisonForm, 0, 0);
        doorPath(barracksIdx, Ground::Road);
      }
      if (jade) {
        for (int y = terrace + 2; y < gy; y++)
          for (int x = X0 + 1; x < X0 + CW - 1; x++)
            if (get(x, y) == K_COMPOUND && M.bldgAt[I(x, y)] < 0 && !front[I(x, y)]) M.setG(x, y, Ground::Plaza);
        // the stream across the court, the walk bridging it
        const int wy = terrace + 4 + cr.irange(std::max(1, gy - terrace - 9));
        for (int x = X0 + 2; x < X0 + CW - 2; x++) {
          if (get(x, wy) == K_SQUARE) { M.setG(x, wy, Ground::Bridge); continue; }
          pond(x, wy);
        }
        // pines in planted squares, set where the seed puts them (never mirrored pairs)
        const int pines = 2 + cr.irange(3);
        for (int k = 0, tries = 0; k < pines && tries < 40; tries++) {
          const int x = X0 + 3 + cr.irange(std::max(1, CW - 6)), y = terrace + 3 + cr.irange(std::max(1, gy - terrace - 5));
          if (std::abs(x - fx) <= 3 || std::abs(y - wy) <= 1 || !lawnFree(x, y) || !lawnFree(x, y - 1) || cover[I(x, y)]) continue;
          for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if (lawnFree(x + ox, y + oy)) M.setG(x + ox, y + oy, Ground::Grass);
          M.setProp(x, y, Prop::PineTree);
          k++;
        }
        // bronze censers in pairs up the way, stone lanterns further out, lions at the gate and before the hall
        const int step = 3 + cr.irange(2);
        for (int y = terrace + 3 + cr.irange(2); y < gy - 2; y += step)
          if (std::abs(y - wy) > 1) for (int s : {-1, 3}) propAt(gx + s, y, Prop::Brazier);
        for (int y = terrace + 4 + cr.irange(3); y < gy - 2; y += 5)
          if (std::abs(y - wy) > 1) for (int s : {-5, 7}) propAt(gx + s, y, Prop::Lamppost);
        for (int s : {-2, 4}) propAt(gx + s, gy - 2, Prop::Statue);
        for (int s : {-3, 3}) propAt(doorX + s, terrace + 2, Prop::Statue);
        midY = wy + 2;
      } else if (dune) {
        // the cross walk, the long rills either side of it, the four green quarters under palms
        const int cy2 = midY + cr.irange(3) - 1;
        for (int x = X0 + 2; x < X0 + CW - 2; x++) walk(x, cy2, Ground::Plaza);
        // (each rill a run of four tiles or more: a tile or two of water cut short by a building read as a puddle)
        for (int s : {-1, 1})
          for (int side = 0; side < 2; side++) {
            const int a0 = side ? gx + 5 : X0 + 3, a1 = side ? X0 + CW - 4 : gx - 3;
            int x = a0;
            while (x <= a1) {
              int e = x;
              while (e <= a1 && lawnFree(e, cy2 + s) && !cover[I(e, cy2 + s)]) e++;
              if (e - x >= 4) for (int k = x; k < e; k++) pond(k, cy2 + s);
              x = e + 1;
            }
          }
        for (int y = terrace + 2; y < gy - 1; y++)
          for (int x = X0 + 2; x < X0 + CW - 2; x++)
            if (lawnFree(x, y) && !cover[I(x, y)] && (base == Ground::Sand || base == Ground::Dirt)) M.setG(x, y, Ground::Grass);   // the irrigated quarters
        // palms in each quarter, where the seed puts them (one or two, never a mirrored pair)
        for (int q = 0; q < 4; q++) {
          const int qx0 = q & 1 ? gx + 5 : X0 + 2, qx1 = q & 1 ? X0 + CW - 3 : gx - 3;
          const int qy0 = q & 2 ? cy2 + 2 : terrace + 3, qy1 = q & 2 ? gy - 2 : cy2 - 2;
          if (qx1 < qx0 || qy1 < qy0) continue;
          const int want = 1 + cr.irange(2);
          for (int k = 0, t = 0; k < want && t < 16; t++) {
            const int x = qx0 + cr.irange(qx1 - qx0 + 1), y = qy0 + cr.irange(qy1 - qy0 + 1);
            if (!lawnFree(x, y) || cover[I(x, y)] || M.bldgAt[I(x, y - 1)] >= 0 || front[I(x, y - 1)]) continue;
            bool near = false;
            for (int oy = -2; oy <= 2; oy++) for (int ox = -2; ox <= 2; ox++) if (M.propAt(x + ox, y + oy) == (int)Prop::PalmTree + 1) near = true;
            if (near) continue;
            M.setProp(x, y, Prop::PalmTree);
            k++;
          }
        }
        // lanterns where the rills meet the walks
        for (int s : {-2, 4}) { propAt(gx + s, cy2 - 2, Prop::Brazier); propAt(gx + s, cy2 + 2, Prop::Brazier); }
        midY = cy2;
      } else if (imperial) {
        // twin canals either side of the way, the emperors' statues along them, cypresses by the walls; the legions'
        // parade ground (trodden earth, the standards in a row) before the barracks
        const int y0 = terrace + 3 + cr.irange(2), y1 = gy - 3 - cr.irange(2);
        const int cw = 1 + cr.irange(2);
        for (int y = y0; y <= y1; y++)
          for (int k = 0; k < cw; k++) { pond(gx - 2 - k, y); pond(gx + 4 + k, y); }
        const int st0 = y0 + cr.irange(2);
        for (int y = st0; y <= y1; y += 3) { propAt(gx - 3 - cw, y, Prop::Statue); propAt(gx + 5 + cw, y, Prop::Statue); }
        for (int y = terrace + 3; y < gy - 1; y += 2 + cr.irange(2)) {
          if (lawnFree(X0 + 2, y) && lawnFree(X0 + 2, y - 1) && !cover[I(X0 + 2, y)]) M.setProp(X0 + 2, y, Prop::PineTree);
          if (lawnFree(X0 + CW - 3, y) && lawnFree(X0 + CW - 3, y - 1) && !cover[I(X0 + CW - 3, y)]) M.setProp(X0 + CW - 3, y, Prop::PineTree);
        }
        if (garrison && barracksIdx >= 0) {
          const Bldg& bb = M.bldgs[(size_t)barracksIdx];
          const int gy0 = bb.r.y + bb.r.h + 2, gx0 = bb.r.x, gx1 = bb.r.x + bb.r.w - 1;
          for (int y = gy0; y <= std::min(gy - 2, gy0 + 3); y++)
            for (int x = gx0; x <= gx1; x++) if (lawnFree(x, y) && !cover[I(x, y)]) M.setG(x, y, Ground::Dirt);
          for (int x = gx0; x <= gx1; x += 2) banner(x, gy0);
        }
      } else if (highland) {
        // the chief's bailey: trodden earth round the well, the woodpiles and the peat stack, a cattle pen, a standing
        // stone where the clan swears its oaths
        for (int y = terrace + 2; y < gy; y++)
          for (int x = X0 + 1; x < X0 + CW - 1; x++)
            if (get(x, y) == K_COMPOUND && blob(x, y, fx, midY, CW * 0.36f, (gy - terrace) * 0.5f, P.seed ^ 0xBA11u) < 1.0f) M.setG(x, y, Ground::Dirt);
        const int ws = cr.irange(2) ? -4 : 6;
        propAt(gx + ws, midY, Prop::Well);
        propAt(X0 + 2 + cr.irange(2), terrace + 2, Prop::Woodpile);
        propAt(X0 + CW - 3 - cr.irange(2), terrace + 2, Prop::Woodpile);
        const int pw = 6, ph = 4, pxx = west ? X0 + CW - 2 - pw : X0 + 2, pyy = gy - 2 - ph;
        bool free = pyy > terrace + 2;
        for (int y = pyy - 1; y <= pyy + ph && free; y++) for (int x = pxx - 1; x <= pxx + pw && free; x++) if (!lawnFree(x, y) || cover[I(x, y)]) free = false;
        if (free) {
          fenceRing(pxx, pyy, pw, ph, pxx + pw / 2, 1);
          propAt(pxx + 1, pyy + 1, Prop::Haystack);
          propAt(pxx + 3, pyy + 2, Prop::Cow);
        }
        propAt(gx + (ws < 0 ? 6 : -4), terrace + 3 + cr.irange(3), Prop::StandingStone);
        for (int s : {-3, 3}) { int x = doorX + s; if (get(x, terrace) == K_SQUARE && !M.prop[I(x, terrace)]) M.setProp(x, terrace, Prop::Banner); }
      } else {
        // the fountain court: a round widening of the walkway (a court palace of another people: two, on the axis)
        const int fy0 = terrace + 4 + cr.irange(3);
        const int fys[2] = {fy0, court && gy - 5 - (fy0) >= 6 ? gy - 5 : -1};
        for (int fy : fys) {
          if (fy < 0) continue;
          for (int y = fy - 2; y <= fy + 2; y++)
            for (int x = fx - 3; x <= fx + 3; x++) {
              int ddx = x - fx, ddy = y - fy;
              if (ddx * ddx * 4 + ddy * ddy * 9 > 40) continue;
              walk(x, y, Ground::Plaza);
            }
          M.setProp(fx, fy, Prop::Fountain);
        }
        // the gardens: parterres, an orchard in rows, or a tiltyard on one side
        const int garden = court ? 0 : cr.irange(3);
        if (garden == 0) {
          if (court) {
            for (int side = 0; side < 2; side++) {
              const int x0 = side == 0 ? X0 + 2 : gx + 7, x1 = side == 0 ? gx - 5 : X0 + CW - 3;
              if (gy - 2 - (terrace + 3) < 7) { parterre(x0, terrace + 3, x1, gy - 2, 0xBED0u + (uint32_t)side); continue; }
              parterre(x0, terrace + 3, x1, midY - 1, 0xBED0u + (uint32_t)side);
              parterre(x0, midY + 1, x1, gy - 2, 0xBED2u + (uint32_t)side);
            }
            for (int y = terrace + 3; y < gy - 1; y += 3)
              for (int s : {-3, 5}) { const int x = gx + s; if (lawnFree(x, y) && !cover[I(x, y)] && lawnFree(x, y - 1)) M.setProp(x, y, tree); }
          } else compoundGrounds(X0, Y0, CW, CH, gx, gy, terrace);
        } else if (garden == 1) {
          // an orchard: fruit trees in staggered rows on the lawns, a bench under them
          const Prop ft = cold ? Prop::PineTree : (dry ? Prop::PalmTree : (cr.irange(2) ? Prop::OakTree : Prop::AutumnTree));
          const int sp = 3 + cr.irange(2);
          for (int y = terrace + 3; y < gy - 1; y += sp)
            for (int x = X0 + 2 + ((y / sp) & 1); x < X0 + CW - 2; x += sp)
              if (std::abs(x - fx) >= 4 && lawnFree(x, y) && lawnFree(x, y - 1) && !cover[I(x, y)]) M.setProp(x, y, ft);
          propAt(gx - 2, midY, Prop::Bench);
        } else {
          // a tiltyard on the side away from the barracks: a trodden list behind a rail, a stand of banners; gardens
          // on the barracks' side
          const int lx0 = west ? gx + 5 : X0 + 3, lx1 = west ? X0 + CW - 4 : gx - 3;
          for (int y = terrace + 4; y < gy - 2; y++)
            for (int x = lx0; x <= lx1; x++) if (lawnFree(x, y) && !cover[I(x, y)]) M.setG(x, y, Ground::Dirt);
          for (int x = lx0; x <= lx1; x++) { if (lawnFree(x, terrace + 3)) M.setProp(x, terrace + 3, Prop::FenceH); }
          for (int x = lx0; x <= lx1; x += 3) banner(x, terrace + 2);
          compoundGrounds(X0, Y0, CW, CH, gx, gy, terrace);
        }
        int corners[4][2] = {{X0 + 2, terrace + 2}, {X0 + CW - 3, terrace + 2}, {X0 + 2, gy - 2}, {X0 + CW - 3, gy - 2}};
        for (auto& c : corners) {
          int x = c[0], y = c[1];
          bool ok = lawnFree(x, y) && !cover[I(x, y)] && lawnFree(x, y - 1);
          for (int oy = -1; oy <= 1 && ok; oy++) for (int ox = -1; ox <= 1; ox++) if (M.bldgAt[I(x + ox, y + oy)] >= 0 || front[I(x + ox, y + oy)]) ok = false;
          if (ok && cr.irange(4) != 0) { M.setP(x, y, 0); M.setProp(x, y, tree); }
        }
        for (int s : {-2, 4}) if (lawnFree(gx + s, gy - 2) && !cover[I(gx + s, gy - 2)]) M.setProp(gx + s, gy - 2, Prop::Statue);
        for (int s : {-3, 3}) { int x = doorX + s; if (get(x, terrace) == K_SQUARE && !M.prop[I(x, terrace)]) M.setProp(x, terrace, Prop::Banner); }
        if (cr.irange(2))
          for (int y = terrace + 3; y < gy - 1; y += 3 + cr.irange(2))
            for (int s : {-1, 3}) if (lawnFree(gx + s, y)) { M.setP(gx + s, y, 0); M.setProp(gx + s, y, Prop::Lamppost); }
        midY = fy0;
      }
      guardSpots[2][0] = gx; guardSpots[2][1] = midY + 1;
      guardSpots[3][0] = gx + 2; guardSpots[3][1] = midY + 1;
      break;
    }
    // ---------------------------------------------------------------- the jarl's great hall in its stockade
    case cult::Seat::GreatHall: {
      // the huscarls' longhouses down both sides of the yard, their doors on it; the hall's yard of trodden earth
      // with the fire pit and the woodpiles
      // (the yard trodden bare in a ragged oval round the fire and down to the gate, grass at its edges)
      for (int y = terrace + 2; y < gy; y++)
        for (int x = X0 + 1; x < X0 + CW - 1; x++)
          if (get(x, y) == K_COMPOUND && blob(x, y, gx + 1, (terrace + gy) / 2, 6.5f, (gy - terrace) * 0.55f, P.seed ^ 0x6A11u) < 1.0f) M.setG(x, y, Ground::Dirt);
      const int lh = 3;
      for (int side = 0; side < 2; side++) {
        for (int k = 0; k < 2; k++) {
          // (every longhouse its own length and set back its own way: four alike in one yard read as a barracks)
          const int lw = 5 + (int)((hashAt(X0 + side, Y0 + k, 0x10E6u)) % 3u);
          const int lx = side == 0 ? X0 + 2 + (int)(k & 1) : X0 + CW - 2 - lw - (int)((k + 1) & 1);
          const int ly = k == 0 ? terrace + 4 : gy - 6;
          if (!canPut(IRect{lx, ly, lw, lh}, Building::House)) continue;
          const int bi = put(Building::House, IRect{lx, ly, lw, lh}, k == 0 ? Role::Guard : Role::Villager, (uint8_t)bld::Form::Long, 0, 0);
          doorPath(bi, Ground::Dirt);
        }
      }
      const int fx = gx + 1, fy = terrace + 5;
      for (int y = fy - 1; y <= fy + 1; y++) for (int x = fx - 2; x <= fx + 2; x++) walk(x, y, Ground::Dirt);
      M.setProp(fx, fy, Prop::Campfire);
      for (int s : {-2, 2}) if (get(fx + s, fy) == K_SQUARE) M.setProp(fx + s, fy, Prop::Bench);
      for (int s : {-3, 3}) { int x = doorX + s; if (get(x, terrace) == K_SQUARE && !M.prop[I(x, terrace)]) M.setProp(x, terrace, Prop::Banner); }
      propAt(X0 + 2, Y0 + 2, Prop::Woodpile);
      propAt(X0 + CW - 3, Y0 + 2, Prop::Woodpile);
      propAt(X0 + 2, gy - 2, Prop::Well) || propAt(X0 + 3, gy - 2, Prop::Well);
      propAt(X0 + CW - 3, gy - 2, Prop::Barrel);
      for (int s : {-2, 4}) banner(gx + s, gy - 2);
      if (garrison) {   // (a jarl's realm with a standing levy too: their barracks by the gate)
        const IRect br{X0 + CW / 2 + 3, gy - 1 - BAR_H - 2, BAR_W, BAR_H};
        if (br.y > terrace + 2 && canPut(br, Building::Barracks)) { barracksIdx = put(Building::Barracks, br, Role::Guard, garrisonForm, 0, 0); doorPath(barracksIdx, Ground::Dirt); }
      }
      break;
    }
    // ---------------------------------------------------------------- the khan's tent court
    case cult::Seat::TentCourt: {
      // the open court before the great tent, trodden bare; the court tents in a ring round it, doors inward and
      // south; banners on their poles round the ring; the horse lines (the culture's rope corrals) in the back corners
      const int ccx = gx + 1 + cr.irange(3) - 1, ccy = terrace + 4 + cr.irange(2);
      for (int y = Y0 + 1; y < gy; y++)
        for (int x = X0 + 1; x < X0 + CW - 1; x++) {
          const int dx = x - ccx, dy = (y - ccy) * 3 / 2;
          if (get(x, y) == K_COMPOUND && dx * dx + dy * dy <= 7 * 7 + (int)(hashAt(x, y, 0x7E17u) % 9u)) M.setG(x, y, Ground::Dirt);
        }
      for (int y = ccy - 2; y <= ccy + 2; y++) for (int x = ccx - 4; x <= ccx + 4; x++) if ((x - ccx) * (x - ccx) + (y - ccy) * (y - ccy) * 4 <= 18) walk(x, y, Ground::Dirt);
      M.setProp(ccx, ccy, Prop::Campfire);
      // the court tents: in a ring round the court, staggered so no tent's felt hides another's door (west: two, the
      // nearer one further in; east: one and the horse lines), and the court's back corners behind the great tent
      const int tw = 4, th = 3;
      const int spots[5][2] = {{ccx - 12 - tw / 2, ccy - 6}, {ccx - 7 - tw / 2, ccy + 1}, {ccx + 12 - tw / 2, ccy - 6},
                               {X0 + 2, Y0 + 1}, {X0 + CW - 2 - tw, Y0 + 1}};
      for (auto& s : spots) {
        IRect r{s[0], s[1], tw, th};
        if (!canPut(r, Building::House)) continue;
        const int bi = put(Building::House, r, Role::Villager, (uint8_t)bld::Form::Tent, 0, 0);
        const Bldg& tb = M.bldgs[(size_t)bi];
        for (int y = tb.r.y + tb.r.h; y <= tb.r.y + tb.r.h + 1; y++) if (get(tb.doorX(), y) == K_COMPOUND) M.setG(tb.doorX(), y, Ground::Dirt);
        // a tent's household: a barrel or a crate by it, a cooking fire's woodpile
        propAt(tb.r.x - 1, tb.r.y + tb.r.h - 1, (hashAt(tb.r.x, tb.r.y, 3u) & 1) ? Prop::Barrel : Prop::Crate);
        propAt(tb.r.x + tb.r.w, tb.r.y + tb.r.h - 1, (hashAt(tb.r.x, tb.r.y, 5u) % 3u) ? Prop::Woodpile : Prop::Haystack);
      }
      // (the khan's riders keep their tents and horse lines at the edge of the town: societyServices)
      // the horse lines: a rope corral in the south-east corner, beasts, a trough and hay
      {
        const int cw = 7, chh = 5, cxx = X0 + CW - 2 - cw, cyy = gy - 1 - chh;
        bool free = cyy > terrace + 2;
        for (int y = cyy - 1; y <= cyy + chh && free; y++) for (int x = cxx - 1; x <= cxx + cw && free; x++) if (!lawnFree(x, y) || cover[I(x, y)]) free = false;
        if (free) {
          fenceRing(cxx, cyy, cw, chh, cxx + cw / 2, 1);
          propAt(cxx + 1, cyy + 1, Prop::Trough);
          propAt(cxx + cw - 2, cyy + 1, Prop::Haystack);
          propAt(cxx + 2, cyy + 3, Prop::Cow);
          propAt(cxx + 4, cyy + 2, Prop::Cow);
          if (!M.prop[I(cxx + cw / 2, cyy + chh - 1)]) set(cxx + cw / 2, cyy + chh - 1, K_YARD);
        }
      }
      // the banner ring: tall standards round the court (how many, and where the ring starts, the khan's own)
      {
        const int nb = 8 + cr.irange(6);
        const float a0 = cr.f() * D_TAU, rr = 8.0f + cr.f() * 1.5f;
        for (int k = 0; k < nb; k++) {
          const float a = a0 + D_TAU * k / (float)nb;
          banner(ccx + (int)std::floor(dcos(a) * rr + 0.5f), ccy + (int)std::floor(dsin(a) * rr * 0.65f + 0.5f));
        }
      }
      // the ovoo: the cairn of stones where the court makes its offerings, at the court's edge
      for (int t = 0; t < 8; t++) {
        const float a = cr.f() * D_TAU;
        if (propAt(ccx + (int)std::floor(dcos(a) * 10.0f + 0.5f), ccy + (int)std::floor(dsin(a) * 6.0f + 0.5f), Prop::GraveCairn)) break;
      }
      for (int s : {-3, 3}) { int x = doorX + s; if (get(x, terrace) == K_SQUARE && !M.prop[I(x, terrace)]) M.setProp(x, terrace, Prop::Banner); }
      // the court round the fire: logs and benches to sit on at the khan's own places round it, the servants' small
      // tents and the court's stores at its edge
      {
        static const int ring[8][2] = {{-2, 0}, {-2, -1}, {-1, -2}, {1, -2}, {2, -1}, {2, 0}, {2, 1}, {-2, 1}};
        const int k0 = cr.irange(8), n = 3 + cr.irange(3);
        for (int k = 0; k < n; k++) {
          const int lx = ccx + ring[(k0 + k * 3) % 8][0], ly = ccy + ring[(k0 + k * 3) % 8][1];
          if (std::abs(lx - doorX) <= 0 && ly < ccy) continue;   // (round 3: the way from the fire to the khan's gate stays open)
          if (get(lx, ly) == K_COMPOUND || get(lx, ly) == K_SQUARE) { if (!M.prop[I(lx, ly)] && !cover[I(lx, ly)]) M.setProp(lx, ly, (k + k0) % 3 == 0 ? Prop::Bench : Prop::Log); }
        }
      }
      for (int k = 0; k < 6; k++) {
        const int tx = X0 + 3 + (int)(hashAt(X0, Y0, 0x7E57u + (uint32_t)k) % (uint32_t)(CW - 6)), ty = terrace + 2 + (int)(hashAt(X0, Y0, 0x7E58u + (uint32_t)k) % (uint32_t)std::max(1, gy - terrace - 4));
        const int dx = tx - ccx, dy = (ty - ccy) * 3 / 2;
        if (dx * dx + dy * dy < 11 * 11 || std::abs(tx - (gx + 1)) <= 2) continue;
        propAt(tx, ty, k < 3 ? Prop::Tent : (k == 3 ? Prop::Haystack : (k == 4 ? Prop::Sacks : Prop::Barrel)));
      }
      propAt(X0 + 2, Y0 + 2, Prop::Haystack);
      propAt(X0 + CW - 3, Y0 + 2, Prop::Crate);
      break;
    }
    // ---------------------------------------------------------------- the temple precinct
    case cult::Seat::TempleComplex: {
      // the temple court: the precinct paved from wall to wall before the temple; the priests' houses along the sides,
      // braziers flanking the processional way, statues of the god at the gate, a fountain of holy water
      for (int y = terrace; y < gy; y++)
        for (int x = X0 + 1; x < X0 + CW - 1; x++) if (get(x, y) == K_COMPOUND) { M.setG(x, y, Ground::Plaza); }
      const int hh = 3;
      for (int side = 0; side < 2; side++)
        for (int k = 0; k < 2; k++) {
          // (the high priest's house and the lesser ones: each its own size and kind, never a mirrored pair)
          const int hw = 4 + (int)((side + k) & 1);
          const int hx = side == 0 ? X0 + 2 : X0 + CW - 2 - hw, hy = terrace + 4 + k * (hh + 3) + side;
          if (!canPut(IRect{hx, hy, hw, hh}, Building::House)) continue;
          const int bi = put(((side + k) & 1) ? Building::StoneHouse : Building::House, IRect{hx, hy, hw, hh}, Role::Priest, 0, 0, 0);
          doorPath(bi, Ground::Plaza);
        }
      const int bstep = 3 + cr.irange(2);
      for (int y = terrace + 3 + cr.irange(2); y < gy - 1; y += bstep)
        for (int s : {-1, 3}) propAt(gx + s, y, Prop::Brazier);
      // (M3b fixer) no tiered fountain: the sacred well's pool sunk in the paving to one side of the way, an altar of
      // offerings on the way before the steps
      {
        const int side = cr.irange(2) ? -1 : 1;
        const int pcx = side < 0 ? gx - 4 - cr.irange(2) : gx + 6 + cr.irange(2), pcy = gy - 5 - cr.irange(3);
        for (int y = pcy - 1; y <= pcy + 1; y++)
          for (int x = pcx - 1; x <= pcx + 1; x++) pond(x, y);
        const int ay = terrace + 3;
        if (get(gx + 1, ay) == K_SQUARE && !M.prop[I(gx + 1, ay)]) M.setProp(gx + 1, ay, Prop::Altar);
      }
      for (int s : {-3, 5}) propAt(gx + s, gy - 2, Prop::Statue);
      for (int s : {-3, 3}) { int x = doorX + s; if (get(x, terrace) == K_SQUARE && !M.prop[I(x, terrace)]) M.setProp(x, terrace, Prop::Banner); }
      // palms (or the land's trees) in the corners of the court
      for (int c = 0; c < 4; c++) {
        const int x = c & 1 ? X0 + CW - 3 : X0 + 2, y = c & 2 ? gy - 2 : Y0 + 2;
        if (lawnFree(x, y) && lawnFree(x, y - 1) && !cover[I(x, y)]) { M.setG(x, y, base == Ground::Grass ? Ground::Grass : base); M.setProp(x, y, tree); }
      }
      break;
    }
    // ---------------------------------------------------------------- the council spire in its gardens
    case cult::Seat::CouncilSpire: {
      fenceRing(X0, Y0, CW, CH, gx, 3);
      // white walks crossing the gardens: the axis and a cross walk with a fountain where they meet
      const int my = (terrace + gy) / 2;
      for (int x = X0 + 2; x < X0 + CW - 2; x++) walk(x, my, Ground::Plaza);
      for (int y = my - 1; y <= my + 1; y++) for (int x = gx - 1; x <= gx + 3; x++) walk(x, y, Ground::Plaza);
      M.setProp(gx + 1, my, Prop::Fountain);
      for (int s : {-6, 8}) if (get(gx + s, my) == K_SQUARE) M.setProp(gx + s, my, Prop::Statue);
      // (M3b fixer) the white gardens: a long moon pool on one side of the cross walk, birch groves (where the seed puts
      // them) on the lawns, a single bed of white flowers; never the two mirrored parterres of every palace
      {
        const int ps = cr.irange(2);
        const int x0 = ps == 0 ? X0 + 4 : gx + 6, x1 = ps == 0 ? gx - 4 : X0 + CW - 5;
        const int y0 = cr.irange(2) ? terrace + 4 : my + 3, y1 = y0 + 1 + cr.irange(2);
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) pond(x, y);
        const int groves = 2 + cr.irange(3);
        for (int k = 0, tries = 0; k < groves && tries < 30; tries++) {
          const int x = X0 + 3 + cr.irange(std::max(1, CW - 6)), y = terrace + 3 + cr.irange(std::max(1, gy - terrace - 5));
          if (std::abs(x - (gx + 1)) <= 3 || std::abs(y - my) <= 1 || !lawnFree(x, y) || !lawnFree(x, y - 1) || cover[I(x, y)]) continue;
          M.setProp(x, y, cold ? Prop::PineTree : Prop::BirchTree);
          if (lawnFree(x + 2, y + 1) && lawnFree(x + 2, y) && !cover[I(x + 2, y + 1)]) M.setProp(x + 2, y + 1, Prop::BirchTree);
          k++;
        }
        const int fs = 1 - ps;
        const int fx0 = fs == 0 ? X0 + 4 : gx + 6, fx1 = fs == 0 ? gx - 4 : X0 + CW - 5;
        parterre(fx0, my + 2, fx1, std::min(gy - 3, my + 5), 0x5B2Eu + (uint32_t)fs);
      }
      for (int c = 0; c < 4; c++) {
        const int x = c & 1 ? X0 + CW - 3 : X0 + 2, y = c & 2 ? gy - 2 : terrace + 2;
        if (lawnFree(x, y) && lawnFree(x, y - 1) && !cover[I(x, y)]) M.setProp(x, y, cold ? Prop::PineTree : Prop::BirchTree);
      }
      for (int s : {-3, 3}) { int x = doorX + s; if (get(x, terrace) == K_SQUARE && !M.prop[I(x, terrace)]) M.setProp(x, terrace, Prop::Banner); }
      break;
    }
    // ---------------------------------------------------------------- the tree palace and its grove
    case cult::Seat::TreePalace: {
      // the elder tree in the court before the palace (its roots spread over five tiles), (fix) beside the processional
      // walk and never on it: on the axis its trunk and roots stood on the paving and its crown hid the approach; it
      // stands to one side on the court's lawn, a ring of paving round its roots joined to the walk
      const int ey = terrace + 9;
      int ex = -1;
      const int pref = (hashAt(gx, terrace, 0x6A0Fu) & 1u) ? 1 : -1;
      for (int sd : {pref, -pref}) {
        const int cx0 = gx + 1 + sd * 7;
        bool ok = cx0 - 5 >= X0 + 1 && cx0 + 5 <= X0 + CW - 2;
        for (int y = ey - 4; y <= ey + 2 && ok; y++)
          for (int x = cx0 - 5; x <= cx0 + 5; x++) if (!in(x, y) || M.bldgAt[I(x, y)] >= 0 || front[I(x, y)] || M.wall[I(x, y)]) { ok = false; break; }
        if (ok) { ex = cx0; break; }
      }
      if (ex >= 0) {
        for (int y = ey - 4; y <= ey + 2; y++)
          for (int x = ex - 5; x <= ex + 5; x++) {
            const float dx = (x - ex) / 4.2f, dy = (y - (ey - 1)) / 2.7f, d = dx * dx + dy * dy;
            if (d >= 0.78f && d <= 1.45f) walk(x, y, Ground::Plaza);
          }
        // the ring joined to the walk by a short path from its near side
        const int dir = ex > gx + 1 ? -1 : 1;
        for (int x = ex + dir * 4; x != (dir < 0 ? gx + 2 : gx); x += dir) walk(x, ey - 1, Ground::Plaza);
        for (int y = ey - 2; y <= ey; y++)
          for (int x = ex - 2; x <= ex + 2; x++) { set(x, y, K_COMPOUND); M.setG(x, y, Ground::Meadow); M.setP(x, y, (x == ex && y == ey) ? (int)Prop::ElderTree + 1 : (int)Prop::Filler + 1); }
      }
      // the ring of old trees: a wild grove's edge, not an avenue (spacing, depth and kind vary tree by tree; a gap
      // here and there, a second tree stepped in behind)
      {
        static const Prop kinds[5] = {Prop::OakTree, Prop::OakTree2, Prop::BirchTree, Prop::OakTree, Prop::WillowTree};
        auto grove = [&](int x, int y, uint32_t k) {
          const uint32_t h = hashAt(x, y, 0x6A0Eu + k);
          if (h % 7u == 0) return;   // a gap
          propAt(x, y, cold ? ((h >> 4) & 1 ? Prop::PineTree : Prop::PineTree2) : kinds[(h >> 4) % 5u]);
        };
        for (int x = X0 + 1; x < X0 + CW - 1;) {
          const uint32_t h = hashAt(x, Y0, 0x6A11u);
          grove(x, Y0 + 1 + (int)(h & 1u), 1);
          if (std::abs(x - (gx + 1)) > 3) grove(x, gy - 1 - (int)((h >> 1) & 1u), 2);
          x += 2 + (int)((h >> 2) % 2u);
        }
        for (int y = Y0 + 3; y < gy - 1;) {
          const uint32_t h = hashAt(X0, y, 0x6A12u);
          grove(X0 + 1 + (int)(h & 1u), y, 3);
          grove(X0 + CW - 2 - (int)((h >> 1) & 1u), y, 4);
          y += 2 + (int)((h >> 2) % 2u);
        }
      }
      for (int y = terrace + 2; y < gy - 1; y++)
        for (int x = X0 + 3; x < X0 + CW - 3; x++) {
          if (!lawnFree(x, y) || cover[I(x, y)] || get(x - 1, y) == K_SQUARE || get(x + 1, y) == K_SQUARE) continue;
          const uint32_t h = hashAt(x, y, 0x6E0Fu);
          if (h % 9u == 0) M.setProp(x, y, Prop::Fern);
          else if (h % 9u == 1) M.setProp(x, y, (h >> 8) & 1 ? Prop::Flowers2 : Prop::Flowers3);
          else if (h % 23u == 2) M.setProp(x, y, Prop::Mushrooms);
        }
      for (int s : {-3, 3}) { int x = doorX + s; if (get(x, terrace) == K_SQUARE && !M.prop[I(x, terrace)]) M.setProp(x, terrace, Prop::Banner); }
      guardSpots[2][0] = ex - 4; guardSpots[2][1] = ey + 1;
      guardSpots[3][0] = ex + 4; guardSpots[3][1] = ey + 1;
      break;
    }
    // ---------------------------------------------------------------- the stilt hall over its pools
    case cult::Seat::StiltHall: {
      // the pools: open water under and round the hall, reed beds at their edges; plank walks (the culture's decking)
      // from the entrance to the hall and round its front
      // (a wobbly oval of open water fading through a reed fringe into the marsh grass: no square tank)
      {
        const float pcx = X0 + CW * 0.5f, pcy = Y0 + CH * 0.42f, prx = CW * 0.58f, pry = CH * 0.56f;
        for (int y = Y0 + 1; y < gy; y++)
          for (int x = X0 + 1; x < X0 + CW - 1; x++) {
            if (get(x, y) != K_COMPOUND) continue;
            const float d = blob(x, y, (int)pcx, (int)pcy, prx, pry, P.seed ^ 0x5717F00Du);
            if (d < 0.86f) M.setG(x, y, Ground::Water);
            else if (d < 1.0f) M.setG(x, y, Ground::Swamp);
            else M.setG(x, y, base == Ground::Grass ? Ground::Grass : base);
          }
      }
      for (int y = py; y < py + PH; y++) for (int x = px; x < px + PW; x++) M.setG(x, y, Ground::Water);
      // (M3b round 3) the hall's door and its landing stay planked: the pools run under the hall, never through its
      // doorway (water there left every marsh ruler's hall unenterable on foot)
      M.setG(doorX, py + PH - 1, Ground::Bridge);
      for (int x = doorX - 1; x <= doorX + 1; x++) walk(x, terrace, Ground::Plaza);
      for (int y = Y0 + 1; y < gy; y++)
        for (int x = X0 + 1; x < X0 + CW - 1; x++) {
          if (get(x, y) != K_COMPOUND || M.bldgAt[I(x, y)] >= 0) continue;
          const uint32_t h = hashAt(x, y, 0x5717u);
          if (M.at(x, y) == Ground::Swamp && h % 3u == 0) M.setProp(x, y, Prop::Reeds);
          else if (M.at(x, y) == Ground::Water && h % 11u == 0 && !cover[I(x, y)]) M.setProp(x, y, Prop::LilyPad);
        }
      // a side walk to two elders' houses on the pools
      const int sy = terrace + 8;
      for (int x = X0 + 2; x < X0 + CW - 2; x++) walk(x, sy, Ground::Plaza);
      for (int side = 0; side < 2; side++) {
        const int hx = side == 0 ? X0 + 3 : X0 + CW - 3 - 5, hy = sy - 3;
        // (its doorstep is the side walk itself)
        bool free = hy > terrace + 1 && hx + 5 < gx - 1 + (side ? 100 : 0) && (side == 0 || hx > gx + 3);
        for (int y = hy - 3; y < hy + 3 && free; y++) for (int x = hx - 1; x <= hx + 5; x++) if (!in(x, y) || M.bldgAt[I(x, y)] >= 0 || front[I(x, y)]) { free = false; break; }
        for (int y = hy - 1; y < hy + 3 && free; y++) for (int x = hx - 1; x <= hx + 5; x++) if (get(x, y) == K_SQUARE) { free = false; break; }
        if (!free) continue;
        const int bi = put(Building::House, IRect{hx, hy, 5, 3}, Role::Villager, (uint8_t)bld::Form::Long, 0, 0);
        Bldg& hb = M.bldgs[(size_t)bi];
        if (hb.styled) { hb.arch.foundation = art::Foundation::Stilts; hb.arch.stilts = true; }
      }
      for (int x = X0 + 3; x < X0 + CW - 3; x += 4) if (get(x, sy) == K_SQUARE && std::abs(x - (gx + 1)) > 1 && !front[I(x, sy)]) M.setProp(x, sy, Prop::Torch);
      for (int s : {-3, 3}) { int x = doorX + s; if (get(x, terrace) == K_SQUARE && !M.prop[I(x, terrace)]) M.setProp(x, terrace, Prop::Banner); }
      guardSpots[2][0] = gx - 1; guardSpots[2][1] = sy;
      guardSpots[3][0] = gx + 3; guardSpots[3][1] = sy;
      break;
    }
  }
  // the royal guard: two before the entrance, two in the court, two at the seat's door
  for (auto& g : guardSpots) addSpawn(Role::Guard, g[0], g[1]);
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
