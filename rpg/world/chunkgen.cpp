// L2b, the chunk pipeline (VISION_PLAN 2.5), in a fixed order: base land, rivers and lakes, settlement buffers (cached),
// site and den stamps, roads and tracks (bridges, fords), relief (levels, cliff faces, ramps), vegetation. WORLD lane.
//
// Every step reads only plans and pure functions, so chunks come out the same in any order. The base land is computed
// for the chunk plus a 2-tile border, roads and relief bits for a 1-tile border: decisions that look at neighbours
// (cliffs, ramps, keeping trees off ramps and roads) see the same picture on both sides of a chunk edge, so there are
// no seams.
//
// Relief contract with the VIEW lane (VISION_PLAN 11): every tile on the LOWER side of a level step (4-neighbour) gets
// Map::HEIGHT_CLIFF (not walkable) unless it is a ramp (Map::HEIGHT_RAMP, walkable). Ramps sit on a global lattice:
// along a face running east-west (the higher tile north or south) a ramp is where (x + offset(y / 16)) mod 16 < 3;
// along a face running north-south the same with x and y swapped. Every run of 14 or more tiles gets a 3-tile ramp,
// stacked cliffs line up into stairways, and roads and tracks turn every cliff they touch into a ramp. Settlements
// and sites stand on one level: the land is flattened under them and terraced back to its own level around them.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include "rpg/world/gen.h"

namespace ew {
using namespace gen;
using art::Monster;
using art::Prop;

namespace {
constexpr int BR = 2;                         // base border
constexpr int BW = CHUNK + 2 * BR;            // 36
constexpr int RW = CHUNK + 2;                 // relief / road grid (1-tile border): 34
inline bool waterG(uint8_t g) { return g == (uint8_t)Ground::Water || g == (uint8_t)Ground::DeepWater; }
}  // namespace

// ------------------------------------------------------------------ steps 1-2: base land, rivers, lakes
void EndlessSource::Impl::baseRect(int32_t x0, int32_t y0, int w, int h, BaseRect& B) {
  B.x0 = x0; B.y0 = y0; B.w = w; B.h = h;
  const size_t n = (size_t)w * h;
  B.ground.assign(n, 0); B.biome.assign(n, 0); B.level.assign(n, 0); B.riverW.assign(n, 0); B.lake.assign(n, 0); B.bridge.assign(n, 0);
  std::vector<uint8_t> deep(n, 0);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      TileF f = tile(x0 + x, y0 + y);
      size_t i = (size_t)y * w + x;
      B.biome[i] = (uint8_t)f.biome;
      B.ground[i] = (uint8_t)groundFor(f.biome, f.e, f.t, x0 + x, y0 + y);
      B.level[i] = f.h;
    }
  const int32_t x1 = x0 + w, y1 = y0 + h;
  for (int32_t ry = regionOf(y0 - 4); ry <= regionOf(y1 + 3); ry++)
    for (int32_t rx = regionOf(x0 - 4); rx <= regionOf(x1 + 3); rx++) {
      std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
      for (const RiverSeg& s : H->segs) {
        if (std::max(s.x0, s.x1) + 3 < x0 || std::min(s.x0, s.x1) - 3 >= x1 || std::max(s.y0, s.y1) + 3 < y0 || std::min(s.y0, s.y1) - 3 >= y1) continue;
        const int lo = s.w >= 3 ? -1 : 0, hi = s.w == 1 ? 0 : s.w == 4 ? 2 : 1;
        walk4(s.x0, s.y0, s.x1, s.y1, [&](int32_t x, int32_t y) {
          for (int oy = lo; oy <= hi; oy++)
            for (int ox = lo; ox <= hi; ox++) {
              if (!B.in(x + ox, y + oy)) continue;
              size_t i = B.at(x + ox, y + oy);
              if (B.biome[i] == (uint8_t)Biome::Ocean) continue;
              B.riverW[i] = std::max(B.riverW[i], s.w);
              if (s.w >= 3 && ox == 0 && oy == 0) deep[i] = 1;
            }
        });
      }
      for (const Lake& L : H->lakes) {
        const int32_t R = 2 * L.r + 2;
        if (L.x + R < x0 || L.x - R >= x1 || L.y + R < y0 || L.y - R >= y1) continue;
        const uint8_t lakeLv = (uint8_t)natLevel(L.x, L.y);
        for (int32_t y = std::max(y0, L.y - R); y < std::min(y1, L.y + R + 1); y++)
          for (int32_t x = std::max(x0, L.x - R); x < std::min(x1, L.x + R + 1); x++) {
            int32_t rr = lakeRadius(L, x, y);
            int64_t d2 = dist2(x, y, L.x, L.y);
            if (d2 >= (int64_t)rr * rr) continue;
            size_t i = B.at(x, y);
            if (B.biome[i] == (uint8_t)Biome::Ocean) continue;
            if (!B.lake[i] || lakeLv < B.level[i]) B.level[i] = lakeLv;
            B.lake[i] = 1;
            if (d2 < (int64_t)(rr / 2) * (rr / 2)) deep[i] = 1;
          }
      }
    }
  // footbridges: rivers cannot be swum, so every few pieces of a river's course a plank bridge crosses it (about one
  // every 40-50 tiles; roads and tracks add their own). Chosen by a hash of the piece, so every rect agrees.
  for (int32_t ry = regionOf(y0 - 10); ry <= regionOf(y1 + 9); ry++)
    for (int32_t rx = regionOf(x0 - 10); rx <= regionOf(x1 + 9); rx++) {
      std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
      for (const RiverSeg& s : H->segs) {
        if (std::max(s.x0, s.x1) + 10 < x0 || std::min(s.x0, s.x1) - 10 >= x1 || std::max(s.y0, s.y1) + 10 < y0 || std::min(s.y0, s.y1) - 10 >= y1) continue;
        if (mix64(key2(s.x0, s.y0) ^ mix64(key2(s.x1, s.y1)) ^ seed ^ tag("b.bridge")) % 6 != 0) continue;
        int32_t len = idist(s.x0, s.y0, s.x1, s.y1);
        if (len < 5) continue;
        int32_t mx = floorDiv(s.x0 + s.x1, 2), my = floorDiv(s.y0 + s.y1, 2);
        int32_t px = -(s.y1 - s.y0), py = s.x1 - s.x0, reach = s.w + 2;
        walk4(mx - px * reach / len, my - py * reach / len, mx + px * reach / len, my + py * reach / len, [&](int32_t x, int32_t y) {
          if (!B.in(x, y)) return;
          size_t i = B.at(x, y);
          if (B.riverW[i] && !B.lake[i] && B.biome[i] != (uint8_t)Biome::Ocean) B.bridge[i] = 1;
        });
      }
    }
  for (size_t i = 0; i < n; i++) {
    if (B.bridge[i]) B.ground[i] = (uint8_t)Ground::Bridge;
    else if (B.lake[i] || B.riverW[i]) B.ground[i] = (uint8_t)(deep[i] ? Ground::DeepWater : Ground::Water);
  }
  // lake shores: sandy patches on open land (only where the whole neighbourhood is inside the rect)
  for (int y = 1; y < h - 1; y++)
    for (int x = 1; x < w - 1; x++) {
      size_t i = (size_t)y * w + x;
      if (B.lake[i] || B.riverW[i] || waterG(B.ground[i])) continue;
      bool shore = false;
      for (int dy = -1; dy <= 1 && !shore; dy++)
        for (int dx = -1; dx <= 1; dx++) if (B.lake[(size_t)(y + dy) * w + x + dx]) { shore = true; break; }
      if (!shore) continue;
      Biome b = (Biome)B.biome[i];
      if (b != Biome::Plains && b != Biome::Desert && b != Biome::Beach && b != Biome::Forest && b != Biome::Autumn) continue;
      if (hq(tileHash(seed ^ tag("b.shore"), x0 + x, y0 + y)) < Q(0.62)) B.ground[i] = (uint8_t)Ground::Sand;
    }
}

// ------------------------------------------------------------------ settlements (cached buffers)
std::shared_ptr<SettlementOut> EndlessSource::Impl::town(const SitePlan& p, const RegionData& D) {
  auto it = towns.find(p.id);
  if (it != towns.end()) { it->second.used = ++townClock; return it->second.out; }
  Nest nest(*this);
  auto t0 = Clock::now();
  auto out = std::make_shared<SettlementOut>();
  SettlementCtx ctx;
  ctx.plan = &p;
  ctx.kingdom = kingdom(p.kingdom);
  ctx.rx = D.plan.rx; ctx.ry = D.plan.ry;
  auto bi = D.bearings.find(p.id);
  if (bi != D.bearings.end()) ctx.roadBearings = bi->second;
  int flat = 0;
  for (size_t k = 0; k < D.plan.sites.size(); k++) if (D.plan.sites[k].id == p.id) flat = D.flatLevel[k].first;
  // the land the town is built on: base terrain with its rivers and lakes, at the town's flattened level
  auto B = std::make_shared<BaseRect>();
  const int pad = 48;
  baseRect(p.gx - pad, p.gy - pad, p.w + 2 * pad, p.h + 2 * pad, *B);
  ctx.base = [this, B, flat, &p](int32_t gx, int32_t gy, Ground& g, Biome& b, uint8_t& h) {
    if (gx > B->x0 && gy > B->y0 && gx < B->x0 + B->w - 1 && gy < B->y0 + B->h - 1) {
      size_t i = B->at(gx, gy);
      g = (Ground)B->ground[i];
      b = (Biome)B->biome[i];
    } else {
      TileF f = tile(gx, gy);
      g = groundFor(f.biome, f.e, f.t, gx, gy);
      b = f.biome;
    }
    bool inside = gx >= p.gx - 8 && gy >= p.gy - 8 && gx < p.gx + p.w + 8 && gy < p.gy + p.h + 8;
    h = (uint8_t)(inside ? flat : natLevel(gx, gy));
  };
  buildSettlement(ctx, *out);
  double ms = msSince(t0);
  stats.settlements++;
  stats.settlementMs += ms;
  stats.maxSettlementMs = std::max(stats.maxSettlementMs, ms);
  if (towns.size() >= 24) {
    auto oldest = towns.begin();
    for (auto i = towns.begin(); i != towns.end(); ++i) if (i->second.used < oldest->second.used) oldest = i;
    towns.erase(oldest);
  }
  towns[p.id] = TownEntry{out, ++townClock};
  return out;
}

// ------------------------------------------------------------------ site stamps
void EndlessSource::Impl::stampSite(const SitePlan& p, Stamp& S) {
  const int32_t x = p.ex, y = p.ey;
  for (int32_t yy = p.gy - 2; yy < p.gy + p.h + 2; yy++)
    for (int32_t xx = p.gx - 2; xx < p.gx + p.w + 2; xx++) S.reserve(xx, yy);
  switch (p.type) {
    case SiteType::Cave: {
      // the mouth in a real cliff face (relief): the entrance tile stays walkable, its approach clear. Where the land
      // has no face here (a start-plan cave on the flat), the mouth opens in a rocky knoll.
      if (natLevel(x, y - 1) <= natLevel(x, y))
        for (int32_t yy = y - 5; yy <= y; yy++)
          for (int32_t xx = x - 5; xx <= x + 5; xx++) {
            int32_t dx = xx - x, dy = yy - (y - 2);
            if (dx * dx * 4 + dy * dy * 10 > 92 + (int32_t)(tileHash(p.seed, xx, yy) & 15)) continue;
            S.g(xx, yy, Ground::Rock); S.clear(xx, yy);
          }
      S.g(x, y, Ground::Dirt);
      S.p(x, y, Prop::CaveEntrance);
      for (int k = -1; k <= 1; k++)
        for (int j = 1; j <= 2; j++) {
          S.clear(x + k, y + j);
          Ground gg = S.at(x + k, y + j);
          if (gg == Ground::Rock || gg == Ground::Swamp) S.g(x + k, y + j, Ground::Dirt);
        }
      if (S.in(x, y + 1) && (tileHash(p.seed, x, y) & 1)) S.g(x, y + 1, Ground::Dirt);
      S.p(x + 2, y + 1, (p.seed >> 3) & 1 ? Prop::Bones : Prop::Boulder);
      break;
    }
    case SiteType::Ruin: {
      const int32_t rx = p.gx, ry = p.gy;
      for (int32_t yy = ry; yy < ry + 7; yy++) for (int32_t xx = rx; xx < rx + 9; xx++) { S.clear(xx, yy); if (groundSolid(S.at(xx, yy))) S.g(xx, yy, Ground::Dirt); }
      for (int32_t yy = ry + 1; yy < ry + 7; yy++) for (int32_t xx = rx + 1; xx < rx + 8; xx++) S.g(xx, yy, Ground::StoneFloor);
      S.p(rx + 4, ry + 2, Prop::IronDoor);
      S.p(rx + 1, ry + 2, Prop::Statue); S.p(rx + 7, ry + 2, Prop::Statue);
      S.p(rx + 2, ry + 5, Prop::Brazier); S.p(rx + 6, ry + 5, Prop::Brazier);
      S.p(rx + 1, ry + 6, Prop::Rock); S.p(rx + 7, ry + 4, Prop::Gravestone);
      for (int32_t xx = rx + 2; xx <= rx + 6; xx++) if (xx != rx + 4) S.p(xx, ry + 1, Prop::Boulder);
      break;
    }
    case SiteType::BanditCamp: {
      const int32_t bx = p.gx, by = p.gy;
      for (int32_t yy = by + 1; yy < by + 8; yy++)
        for (int32_t xx = bx + 1; xx < bx + 10; xx++) {
          S.clear(xx, yy);
          int32_t dx = 2 * (xx - (bx + 5)), dy = 2 * (yy - (by + 4));
          if (dx * dx + (dy * dy * 169) / 100 < 58) S.g(xx, yy, Ground::Dirt);
          else if (groundSolid(S.at(xx, yy))) S.g(xx, yy, Ground::Dirt);
        }
      S.p(bx + 5, by + 4, Prop::Campfire);
      S.p(bx + 2, by + 2, Prop::Tent); S.p(bx + 8, by + 2, Prop::Tent);
      S.p(bx + 2, by + 6, Prop::Crate); S.p(bx + 3, by + 7, Prop::Barrel);
      S.p(bx + 8, by + 6, Prop::Chest); S.p(bx + 9, by + 7, Prop::Woodpile);
      S.p(bx + 1, by + 4, Prop::Torch); S.p(bx + 9, by + 4, Prop::Torch);
      // the camp's bandits (the whole site's, so they arrive with any chunk the camp touches)
      Rng r(p.seed ^ 0xBA4Du);
      int n = 4 + r.irange(3);
      for (int k = 0; k < n; k++) {
        SpawnPlan s; s.siteId = p.id;
        s.sp.bandit = true; s.sp.slot = k;
        s.sp.x = bx + 3 + r.irange(5); s.sp.y = by + 3 + r.irange(3);
        S.c.spawns.push_back(s);
      }
      SpawnPlan chief; chief.siteId = p.id;
      chief.sp.bandit = true; chief.sp.boss = true; chief.sp.slot = 9; chief.sp.x = bx + 6; chief.sp.y = by + 3;
      S.c.spawns.push_back(chief);
      break;
    }
    case SiteType::Shrine: {
      const int32_t sx = p.gx, sy = p.gy;
      for (int32_t yy = sy; yy < sy + 5; yy++) for (int32_t xx = sx; xx < sx + 5; xx++) { S.clear(xx, yy); if (groundSolid(S.at(xx, yy))) S.g(xx, yy, Ground::Grass); }
      for (int32_t yy = sy + 1; yy < sy + 4; yy++) for (int32_t xx = sx + 1; xx < sx + 4; xx++) S.g(xx, yy, Ground::Plaza);
      S.p(sx + 2, sy + 1, Prop::Shrine);
      S.p(sx + 1, sy + 3, Prop::Flowers2); S.p(sx + 3, sy + 3, Prop::Flowers3);
      break;
    }
    case SiteType::DragonLair: {
      // a scorched eyrie on the ridge: a ring of rubble and bones around a paved fire-scarred floor
      for (int32_t yy = y - 7; yy <= y + 7; yy++)
        for (int32_t xx = x - 8; xx <= x + 8; xx++) {
          int32_t dx = xx - x, dy = yy - y;
          int32_t d2 = dx * dx * 100 + dy * dy * 132;
          int32_t lim = 76 + (int32_t)(tileHash(p.seed ^ 61u, xx, yy) % 15);
          if (d2 >= lim * lim) continue;
          S.clear(xx, yy);
          S.g(xx, yy, d2 < 34 * 34 ? Ground::StoneFloor : Ground::Dirt);
          if (d2 > 52 * 52 && (tileHash(p.seed ^ 62u, xx, yy) & 7) == 0) S.p(xx, yy, (tileHash(p.seed ^ 63u, xx, yy) & 1) ? Prop::Boulder : Prop::Bones);
        }
      S.p(x - 3, y - 2, Prop::SkullPile); S.p(x + 4, y + 1, Prop::Bones); S.p(x - 5, y + 3, Prop::Bones);
      S.p(x + 2, y - 4, Prop::Brazier); S.p(x - 2, y - 4, Prop::Brazier); S.p(x, y - 5, Prop::Altar);
      break;
    }
    default: break;
  }
}

void EndlessSource::Impl::stampDen(const DenPlan& d, Stamp& S) {
  Rng r(mix64(seed ^ d.id));
  for (int oy = -3; oy <= 3; oy++) for (int ox = -3; ox <= 3; ox++) if (ox * ox + oy * oy <= 10) S.reserve(d.x + ox, d.y + oy);
  Biome b = S.in(d.x, d.y) ? (Biome)S.c.biome[S.idx(d.x, d.y)] : tile(d.x, d.y).biome;
  bool snowy = b == Biome::Snow;
  Prop rock = snowy ? Prop::SnowRock : (b == Biome::Forest || b == Biome::Swamp || b == Biome::Taiga) ? Prop::MossRock : Prop::Boulder;
  std::vector<Prop> big, small;
  switch (d.mon) {
    case Monster::Wolf: case Monster::IceWolf: big = {rock, Prop::Boulder, Prop::DeadTree}; small = {Prop::Bones, Prop::SkullPile, Prop::Bones}; break;
    case Monster::Goblin: big = {Prop::Tent, Prop::Crate, Prop::Woodpile}; small = {Prop::Bones, Prop::Barrel}; break;
    case Monster::Skeleton: big = {Prop::Gravestone, Prop::Gravestone, Prop::DeadTree}; small = {Prop::SkullPile, Prop::Bones, Prop::Bones}; break;
    case Monster::Spider: case Monster::FrostSpider: big = {Prop::DeadTree, rock}; small = {Prop::Cobweb, Prop::Cobweb, Prop::Bones}; break;
    case Monster::Troll: big = {rock, Prop::Boulder, rock}; small = {Prop::SkullPile, Prop::Bones, Prop::Bones}; break;
    default: big = {rock, Prop::Boulder, Prop::Log}; small = {Prop::Bones, Prop::Bones}; break;
  }
  if (d.mon == Monster::Goblin) S.p(d.x, d.y, Prop::Campfire);
  static const int chestAt[3][2] = {{0, -2}, {1, -2}, {-1, -2}};
  const int* ch = chestAt[r.irange(3)];
  S.p(d.x + ch[0], d.y + ch[1], Prop::Chest);
  static const int bigAt[3][2] = {{-2, -2}, {2, -1}, {-3, 0}};
  for (size_t k = 0; k < big.size() && k < 3; k++) S.p(d.x + bigAt[k][0], d.y + bigAt[k][1], big[k]);
  static const int smallAt[4][2] = {{1, 1}, {-1, 1}, {2, 2}, {-2, 1}};
  for (int k = 0; k < 4; k++) if (r.next() % 10 < 7) S.p(d.x + smallAt[k][0], d.y + smallAt[k][1], small[(size_t)r.irange((int)small.size())]);
}

// ------------------------------------------------------------------ the chunk
void EndlessSource::Impl::chunk(int32_t cx, int32_t cy, ChunkData& c) {
  makeStart();
  auto t0 = Clock::now();
  const double nested0 = nestedMs;
  c = ChunkData();
  c.cx = cx; c.cy = cy;
  const int32_t x0 = cx * CHUNK, y0 = cy * CHUNK;
  // 1-2: base land, rivers, lakes (with a 2-tile border)
  BaseRect B;
  baseRect(x0 - BR, y0 - BR, BW, BW, B);
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) {
      size_t bi = (size_t)(ly + BR) * BW + (lx + BR);
      c.ground[c.at(lx, ly)] = B.ground[bi];
      c.biome[c.at(lx, ly)] = B.biome[bi];
    }
  std::vector<uint8_t> lev = B.level;   // BW x BW, flattened below
  std::vector<uint8_t> reserved((size_t)ChunkData::N, 0);
  std::vector<uint8_t> used(RW * RW, 0), road(RW * RW, 0), entrance(RW * RW, 0);
  auto rin = [&](int32_t gx, int32_t gy) { return gx >= x0 - 1 && gy >= y0 - 1 && gx < x0 + CHUNK + 1 && gy < y0 + CHUNK + 1; };
  auto ri = [&](int32_t gx, int32_t gy) { return (size_t)(gy - (y0 - 1)) * RW + (gx - (x0 - 1)); };
  Stamp S{c, x0, y0, reserved};
  const int32_t rx0 = regionOf(x0), ry0 = regionOf(y0);
  std::shared_ptr<const RegionData> RD[9];
  for (int k = 0; k < 9; k++) RD[k] = regionData(rx0 - 1 + k % 3, ry0 - 1 + k / 3);
  const RegionData& own = *RD[4];
  // 3: settlement buffers
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites) {
      if (!isSettlement(p.type)) continue;
      const int margin = 32;
      if (p.gx - margin >= x0 + CHUNK + 1 || p.gy - margin >= y0 + CHUNK + 1 || p.gx + p.w + margin <= x0 - 1 || p.gy + p.h + margin <= y0 - 1) continue;
      std::shared_ptr<SettlementOut> T = town(p, *RD[k]);
      const Map& b = T->buf;
      for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
        for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++) {
          int32_t bx = gx - T->gx, by = gy - T->gy;
          if (!b.in(bx, by)) continue;
          size_t bi = (size_t)by * b.w + bx;
          if (!T->used.empty() && !T->used[bi]) continue;   // outside the town: the wild land stays
          used[ri(gx, gy)] = 1;
          if (!S.in(gx, gy)) continue;
          int i = S.idx(gx, gy);
          c.ground[i] = b.ground[bi];
          c.prop[i] = b.prop[bi];
          c.wall[i] = b.wall[bi];
          if (!b.biome.empty()) c.biome[i] = b.biome[bi];
          reserved[(size_t)i] = 1;
        }
      if (p.gx - 24 >= x0 + CHUNK || p.gy - 24 >= y0 + CHUNK || p.gx + p.w + 24 <= x0 || p.gy + p.h + 24 <= y0) continue;
      // the whole site's records travel with every chunk it touches, so a site's buildings arrive together
      for (const Bldg& bl : b.bldgs) {
        Bldg g = bl;
        g.r.x += T->gx; g.r.y += T->gy;
        g.site = -1;
        c.bldgs.push_back(g);
        c.bldgSite.push_back(p.id);
        int bidx = (int)c.bldgs.size();
        for (int yy = g.r.y; yy < g.r.y + g.r.h; yy++)
          for (int xx = g.r.x; xx < g.r.x + g.r.w; xx++)
            if (S.in(xx, yy)) c.bldg[S.idx(xx, yy)] = (uint16_t)bidx;
      }
      for (const Spawn& sp0 : b.spawns) {
        SpawnPlan sp;
        sp.sp = sp0;
        sp.sp.x += T->gx; sp.sp.y += T->gy;
        sp.siteId = p.id;
        c.spawns.push_back(sp);
      }
      for (auto& gt : T->gates) c.gates.push_back(GTile{gt.first + T->gx, gt.second + T->gy});
      for (IRect r : T->wallGaps) { r.x += T->gx; r.y += T->gy; c.wallGaps.push_back(r); }
    }
  // 4: site and den stamps
  for (int k = 0; k < 9; k++) {
    for (const SitePlan& p : RD[k]->plan.sites) {
      if (isSettlement(p.type)) continue;
      if (p.type == SiteType::Cave && rin(p.ex, p.ey)) entrance[ri(p.ex, p.ey)] = 1;
      if (p.gx - 6 >= x0 + CHUNK || p.gy - 6 >= y0 + CHUNK || p.gx + p.w + 6 <= x0 || p.gy + p.h + 6 <= y0) continue;
      stampSite(p, S);
    }
    for (const DenPlan& d : RD[k]->plan.dens) {
      if (d.x + 5 < x0 || d.y + 5 < y0 || d.x - 5 >= x0 + CHUNK || d.y - 5 >= y0 + CHUNK) continue;
      stampDen(d, S);
    }
  }
  // 5: roads (this region's graph roads) and tracks (every nearby site's spur), clipped at settlement footprints
  struct Clip { Gid id; int32_t x0, y0, x1, y1; };
  std::vector<Clip> clips;
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites) {
      int pad = isSettlement(p.type) ? 0 : 1;
      clips.push_back(Clip{p.id, p.gx - pad, p.gy - pad, p.gx + p.w + pad, p.gy + p.h + pad});
    }
  auto clipOf = [&](Gid id) -> const Clip* {
    for (const Clip& cl : clips) if (cl.id == id) return &cl;
    return nullptr;
  };
  auto drawRoad = [&](const RoadPlan& rp) {
    const Clip* ca = rp.a ? clipOf(rp.a) : nullptr;
    const Clip* cb = rp.b ? clipOf(rp.b) : nullptr;
    const uint8_t code = rp.cls == 0 ? 3 : rp.cls == 1 ? 2 : 1;
    const int hi = rp.cls == 0 ? 1 : 0;
    for (size_t n = 0; n + 1 < rp.pts.size(); n++) {
      const GTile &a = rp.pts[n], &b = rp.pts[n + 1];
      if (std::max(a.x, b.x) + 2 < x0 - 1 || std::min(a.x, b.x) - 2 > x0 + CHUNK || std::max(a.y, b.y) + 2 < y0 - 1 || std::min(a.y, b.y) - 2 > y0 + CHUNK) continue;
      walk4(a.x, a.y, b.x, b.y, [&](int32_t x, int32_t y) {
        for (int oy = 0; oy <= hi; oy++)
          for (int ox = 0; ox <= hi; ox++) {
            int32_t gx = x + ox, gy = y + oy;
            if (!rin(gx, gy)) continue;
            if (ca && gx >= ca->x0 && gy >= ca->y0 && gx < ca->x1 && gy < ca->y1) continue;
            if (cb && gx >= cb->x0 && gy >= cb->y0 && gx < cb->x1 && gy < cb->y1) continue;
            size_t i = ri(gx, gy);
            if (used[i]) continue;
            road[i] = std::max(road[i], code);
          }
      });
    }
  };
  for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
    for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++)
      if (B.bridge[B.at(gx, gy)] && !used[ri(gx, gy)]) road[ri(gx, gy)] = 1;
  for (const RoadPlan& rp : own.plan.roads) if (rp.cls != 2) drawRoad(rp);
  for (int k = 0; k < 9; k++)
    for (const RoadPlan& rp : RD[k]->plan.roads) if (rp.cls == 2) drawRoad(rp);
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) {
      uint8_t code = road[ri(x0 + lx, y0 + ly)];
      if (!code) continue;
      int i = c.at(lx, ly);
      size_t bi = (size_t)(ly + BR) * BW + (lx + BR);
      Ground g = (Ground)c.ground[i];
      if (g == Ground::Water || g == Ground::DeepWater) {
        bool ford = B.riverW[bi] == 1 && !B.lake[bi];
        c.ground[i] = (uint8_t)(ford ? Ground::Dirt : Ground::Bridge);
      } else if (g == Ground::Bridge || g == Ground::Road) {
        // keep
      } else {
        c.ground[i] = (uint8_t)(code >= 2 ? Ground::Road : Ground::Dirt);
      }
      c.prop[i] = 0;
      reserved[(size_t)i] = 1;
    }
  // 6: relief. Settlements and sites stand on one level, terraced back to the land's own level around them.
  // the flat zone is a rounded rectangle around the footprint with a wobbling edge, so the terraces around a town
  // follow the land's grain instead of drawing a box
  struct Flat { int32_t cx2, cy2, hw2, hh2, rc; int lv, step; uint64_t wob; };   // centre and half sizes doubled
  std::vector<Flat> flats;
  for (int k = 0; k < 9; k++) {
    const RegionData& D = *RD[k];
    for (size_t s = 0; s < D.plan.sites.size(); s++) {
      int lv = D.flatLevel[s].first;
      if (lv < 0) continue;
      const SitePlan& p = D.plan.sites[s];
      const bool st = isSettlement(p.type);
      const int pad = st ? 10 : 3;
      Flat f{2 * p.gx + p.w, 2 * p.gy + p.h, p.w + 2 * pad, p.h + 2 * pad, 0, lv, st ? 14 : 6, mix64(p.id ^ 0xF1A7u)};
      f.rc = std::min(f.hw2, f.hh2) * 7 / 10;   // corner radius (doubled units)
      const int32_t reach = 4 * f.step + 8;
      if ((f.cx2 - f.hw2) / 2 - reach > x0 + CHUNK + BR || (f.cy2 - f.hh2) / 2 - reach > y0 + CHUNK + BR ||
          (f.cx2 + f.hw2) / 2 + reach < x0 - BR || (f.cy2 + f.hh2) / 2 + reach < y0 - BR)
        continue;
      flats.push_back(f);
    }
  }
  if (!flats.empty())
    for (int y = 0; y < BW; y++)
      for (int x = 0; x < BW; x++) {
        const int32_t gx = x0 - BR + x, gy = y0 - BR + y;
        int lo = 0, hi = 7, bestD = INT32_MAX, bestLv = -1;
        bool any = false;
        for (const Flat& f : flats) {
          // distance (tiles) outside the rounded rectangle, plus a slow wobble of +-5 tiles
          int32_t qx = std::max(0, std::abs(2 * gx + 1 - f.cx2) - (f.hw2 - f.rc)), qy = std::max(0, std::abs(2 * gy + 1 - f.cy2) - (f.hh2 - f.rc));
          int32_t out2 = (int32_t)isqrt((uint64_t)qx * qx + (uint64_t)qy * qy) - f.rc;   // doubled units
          int32_t wob = (int32_t)(((int64_t)(vnoiseQ(gx, gy, 4, f.wob) - 32768) * 10) >> 16);
          int32_t d = std::max(0, out2 / 2 + wob);
          int dev = d / f.step;
          if (dev >= 4) continue;
          any = true;
          lo = std::max(lo, f.lv - dev);
          hi = std::min(hi, f.lv + dev);
          if (d < bestD) { bestD = d; bestLv = f.lv; }
        }
        if (!any) continue;
        uint8_t& L = lev[(size_t)y * BW + x];
        if (lo <= hi) L = (uint8_t)std::clamp((int)L, lo, hi);
        else L = (uint8_t)bestLv;
      }
  // cliff and ramp bits on the 1-tile border grid
  std::vector<uint8_t> bits(RW * RW, 0);
  auto L = [&](int32_t gx, int32_t gy) { return (int)lev[(size_t)(gy - (y0 - BR)) * BW + (gx - (x0 - BR))]; };
  const uint64_t rampSeed = seed ^ tag("relief.ramp");
  for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
    for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++) {
      const int l = L(gx, gy);
      const int ln = L(gx, gy - 1), ls = L(gx, gy + 1), lw = L(gx - 1, gy), le = L(gx + 1, gy);
      uint8_t b = (uint8_t)l;
      size_t bi = (size_t)(gy - (y0 - BR)) * BW + (gx - (x0 - BR));
      const bool wet = !B.bridge[bi] && (B.lake[bi] || B.riverW[bi] || waterG(B.ground[bi]));
      if (ln > l || ls > l || lw > l || le > l) {
        bool ramp = false;
        if (!wet && !entrance[ri(gx, gy)]) {
          if (ln > l || ls > l) {
            int32_t off = (int32_t)(tileHash(rampSeed, 0, gy >> 4) & 15);
            ramp = ((gx + off) & 15) < 3;
          } else {
            int32_t off = (int32_t)(tileHash(rampSeed, 1, gx >> 4) & 15);
            ramp = ((gy + off) & 15) < 3;
          }
          // roads and tracks climb by stairs
          if (road[ri(gx, gy)]) ramp = true;
          else
            for (int k = 0; k < 4 && !ramp; k++) {
              static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
              int32_t nx = gx + dx[k], ny = gy + dy[k];
              if (rin(nx, ny) && road[ri(nx, ny)]) ramp = true;
            }
        }
        if (entrance[ri(gx, gy)]) b = (uint8_t)l;
        else b |= ramp ? Map::HEIGHT_RAMP : Map::HEIGHT_CLIFF;
      }
      bits[ri(gx, gy)] = b;
    }
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) c.height[c.at(lx, ly)] = bits[ri(x0 + lx, y0 + ly)];
  // 7: vegetation by per-tile hash (off reserved tiles, roads, cliffs, ramps and their approaches)
  const uint64_t vs = mix64(seed ^ tag("veg"));
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) {
      const int i = c.at(lx, ly);
      if (reserved[(size_t)i] || c.prop[i] || c.wall[i] || c.bldg[i]) continue;
      const int32_t x = x0 + lx, y = y0 + ly;
      if (bits[ri(x, y)] & (Map::HEIGHT_CLIFF | Map::HEIGHT_RAMP)) continue;
      bool nearRamp = false, nearRoad = false, nearWater = false, nearCliff = false;
      for (int k = 0; k < 8; k++) {
        static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1}, dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        int32_t nx = x + dx[k], ny = y + dy[k];
        uint8_t nb = bits[ri(nx, ny)];
        if (nb & Map::HEIGHT_RAMP) nearRamp = true;
        if ((nb & Map::HEIGHT_CLIFF) && k < 4) nearCliff = true;
        if (road[ri(nx, ny)]) nearRoad = true;
        size_t bi = (size_t)(ny - (y0 - BR)) * BW + (nx - (x0 - BR));
        if (k < 4 && (B.lake[bi] || B.riverW[bi])) nearWater = true;
      }
      if (nearRamp) continue;
      Ground g = (Ground)c.ground[i];
      if (g == Ground::Road || g == Ground::Bridge || g == Ground::Plaza || g == Ground::Farmland || g == Ground::StoneFloor || g == Ground::Dirt) continue;
      int32_t r = hq(tileHash(vs, x, y));
      int32_t dens = fbmQ(x, y, 3, 3, vs ^ 0xD3u);
      int32_t q = hq(tileHash(vs ^ 0x400u, x, y));
      uint64_t pk = tileHash(vs ^ 0x300u, x, y);
      auto pick = [&](std::initializer_list<Prop> l) { return *(l.begin() + (size_t)(pk % l.size())); };
      const Biome b = (Biome)c.biome[i];
      const int lv = bits[ri(x, y)] & Map::HEIGHT_LEVEL;
      // highland thins the woods
      const int32_t thin = lv >= 5 ? Q(0.55) : lv >= 4 ? Q(0.8) : Q_ONE;
      Prop p = Prop::COUNT;
      if (g == Ground::Water) {
        if (b == Biome::Swamp && r < Q(0.12)) p = Prop::LilyPad;
        else if (r < Q(0.025) && (Biome)c.biome[i] != Biome::Ocean && !B.riverW[(size_t)(ly + BR) * BW + lx + BR]) p = Prop::LilyPad;
      } else if (groundSolid(g)) {
        continue;
      } else if (nearWater && b != Biome::Desert && b != Biome::Snow && r < Q(0.30)) {
        p = q < Q(0.6) ? Prop::Reeds : Prop::TallGrass;
      } else switch (b) {
        case Biome::Plains:
          if (r < qm(Q(0.010) + std::max(0, (dens - Q(0.60)) / 2), thin)) p = pick({Prop::OakTree, Prop::OakTree2, Prop::BirchTree});
          else if (r < Q(0.07)) p = pick({Prop::Flowers1, Prop::Flowers2, Prop::Flowers3});
          else if (r < Q(0.15)) p = Prop::TallGrass;
          else if (r < Q(0.162)) p = pick({Prop::Bush, Prop::BerryBush});
          else if (r < Q(0.168) + (lv >= 4 ? Q(0.01) : 0)) p = pick({Prop::Rock, Prop::Boulder});
          break;
        case Biome::Forest:
          if (r < qm(Q(0.10) + qm(dens, Q(0.32)), thin)) p = pick({Prop::OakTree, Prop::OakTree2, Prop::OakTree, Prop::BirchTree});
          else if (r < Q(0.50)) { if (q < Q(0.12)) p = Prop::Fern; else if (q < Q(0.15)) p = pick({Prop::Bush, Prop::Mushrooms}); else if (q < Q(0.17)) p = pick({Prop::Stump, Prop::Log, Prop::MossRock}); else if (q < Q(0.25)) p = Prop::TallGrass; }
          break;
        case Biome::Autumn:
          if (r < qm(Q(0.08) + qm(dens, Q(0.30)), thin)) p = pick({Prop::AutumnTree, Prop::AutumnTree, Prop::BirchTree});
          else if (r < Q(0.5)) { if (q < Q(0.08)) p = Prop::Fern; else if (q < Q(0.12)) p = pick({Prop::Bush, Prop::Mushrooms, Prop::Flowers3}); else if (q < Q(0.13)) p = Prop::Stump; }
          break;
        case Biome::Taiga:
          if (r < qm(Q(0.06) + qm(dens, Q(0.28)), thin)) p = pick({Prop::PineTree, Prop::PineTree2});
          else if (r < Q(0.45)) { if (q < Q(0.04)) p = Prop::Fern; else if (q < Q(0.06)) p = pick({Prop::MossRock, Prop::Boulder, Prop::Stump}); else if (q < Q(0.1)) p = Prop::TallGrass; }
          break;
        case Biome::Snow:
          if (r < qm(Q(0.03) + qm(dens, Q(0.18)), thin)) p = Prop::SnowPine;
          else if (r < Q(0.4)) { if (q < Q(0.03)) p = Prop::SnowRock; else if (q < Q(0.05)) p = Prop::SnowBush; }
          break;
        case Biome::Swamp:
          if (r < Q(0.03) + qm(dens, Q(0.10))) p = pick({Prop::WillowTree, Prop::WillowTree, Prop::DeadTree});
          else if (r < Q(0.5)) { if (q < Q(0.2)) p = Prop::Reeds; else if (q < Q(0.24)) p = Prop::Mushrooms; else if (q < Q(0.28)) p = Prop::TallGrass; }
          break;
        case Biome::Desert:
          if (r < Q(0.012)) p = Prop::Cactus;
          else if (r < Q(0.018)) p = pick({Prop::Rock, Prop::DeadTree});
          break;
        case Biome::Beach:
          if (r < Q(0.025)) p = Prop::Rock;
          else if (r < Q(0.05)) p = Prop::Reeds;
          break;
        default: break;
      }
      if (p == Prop::COUNT) continue;
      // keep the road verges and cliff feet free of anything solid
      if ((nearRoad || nearCliff) && propSolid(p)) continue;
      c.prop[i] = (uint8_t)((int)p + 1);
    }
  double ms = msSince(t0) - (nestedMs - nested0);
  stats.chunks++;
  stats.chunkMs += ms;
  stats.maxChunkMs = std::max(stats.maxChunkMs, ms);
}

}  // namespace ew
