// The endless world's Active Window (VISION_PLAN 2.9): World::over is a WIN x WIN tile window onto the endless land, its
// local (0,0) at global tile (ox, oy). M1 Phase A by the lead; owned by the SIM lane.
//
// Everything World holds (sites, buildings, dens, gates, wall gaps, spawns) is in window-local tiles and is translated
// whenever the window moves. Site, building and den records are append-only for the session (handles stay valid: quests,
// lodging and saves hold them); their ids (Gid) are what saves and caches key on. A site's buildings arrive together, in
// order, the first time any chunk it touches streams in, so Site::bldgFirst / bldgCount stay a contiguous range.
//
// SIM lane (M1):
//   - streaming without hitches: a ChunkStreamer (stream.h) prefetches the ring of chunks around the window (and the
//     region plans the next moves need) on a worker thread natively, or with a per-frame budget on the web; a walking
//     shift only copies ready chunks. Teleports and loads generate what is not ready, synchronously, behind the fade.
//   - bounded records: spawns, gates and wall gaps are regenerable bookkeeping, so far ones are dropped (and re-streamed
//     when they come near again) once the session has collected many; buildings, sites and dens stay (handles), and
//     they are small. Map::bldgAt is 32-bit.
//   - spatial look-ups: nearSites / nearDens (the records within the window) and siteSpawns (a site's spawns) keep the
//     per-frame loops independent of how far the player has walked.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdlib>
#include <cstring>
#include "rpg/sim/stream.h"
#include "rpg/sim/world.h"
#include "rpg/world/source.h"

namespace {
inline int32_t floorTo(int32_t v, int32_t step) { return ew::floorDiv(v, step) * step; }
inline uint64_t posKey(int32_t gx, int32_t gy, uint32_t tagv) {
  return ew::mix64((((uint64_t)(uint32_t)gx << 32) | (uint32_t)gy) ^ ((uint64_t)tagv << 56));
}
inline uint64_t spawnKeyOf(ew::Gid siteId, int slot) { return ew::mix64(siteId ^ ((uint64_t)(uint32_t)slot * 0x9E3779B97F4A7C15ull)); }
inline uint64_t gapKeyOf(int32_t gx, int32_t gy, int w, int h) { return posKey(gx, gy, 2 + (uint32_t)w * 4 + (uint32_t)h * 16); }

// the records (and with `tiles`, the ground...) of one generated chunk into the window
void applyChunk(World& W, const ew::ChunkData& c, bool tiles) {
  Map& over = W.over;
  const int WIN = World::WIN;
  const int32_t lx0 = c.cx * ew::CHUNK - W.ox, ly0 = c.cy * ew::CHUNK - W.oy;
  if (tiles && lx0 >= 0 && ly0 >= 0 && lx0 + ew::CHUNK <= WIN && ly0 + ew::CHUNK <= WIN) {
    for (int y = 0; y < ew::CHUNK; y++) {
      size_t di = (size_t)(ly0 + y) * WIN + lx0;
      int si = y * ew::CHUNK;
      std::memcpy(&over.ground[di], &c.ground[si], ew::CHUNK);
      std::memcpy(&over.prop[di], &c.prop[si], ew::CHUNK);
      std::memcpy(&over.wall[di], &c.wall[si], ew::CHUNK);
      std::memcpy(&over.biome[di], &c.biome[si], ew::CHUNK);
      std::memcpy(&over.height[di], &c.height[si], ew::CHUNK);
      std::memset(&over.deco[di], 0, ew::CHUNK);
    }
  }
  // buildings: a site's whole list arrives with each chunk it touches; append it once, contiguously
  for (size_t i = 0; i < c.bldgs.size(); i++) {
    const Bldg& b0 = c.bldgs[i];
    if (W.bldgById.count(b0.id)) continue;
    int sh = W.ensureSite(c.bldgSite[i]);
    Bldg b = b0;
    b.r.x -= W.ox; b.r.y -= W.oy;
    b.site = sh;
    int bi = (int)over.bldgs.size();
    if (sh >= 0) {
      Site& s = W.sites[(size_t)sh];
      if (s.bldgCount == 0) s.bldgFirst = bi;
      s.bldgCount = bi + 1 - s.bldgFirst;
    }
    W.bldgById[b.id] = bi;
    over.bldgs.push_back(b);
  }
  for (const ew::SpawnPlan& sp : c.spawns) {
    if (!W.spawnKeys.insert(spawnKeyOf(sp.siteId, sp.sp.slot)).second) continue;
    Spawn s = sp.sp;
    s.x -= W.ox; s.y -= W.oy;
    s.site = W.ensureSite(sp.siteId);
    if (s.site >= 0) W.siteSpawns[s.site].push_back((int)over.spawns.size());
    over.spawns.push_back(s);
  }
  for (const ew::GTile& g : c.gates)
    if (W.gateKeys.insert(posKey(g.x, g.y, 1)).second) W.gates.push_back({g.x - W.ox, g.y - W.oy});
  for (const IRect& r : c.wallGaps)
    if (W.gateKeys.insert(gapKeyOf(r.x, r.y, r.w, r.h)).second) W.wallGaps.push_back(IRect{r.x - W.ox, r.y - W.oy, r.w, r.h});
}
}  // namespace

// a new endless world; the window at `origin` (global tile, rounded to chunks) or, without one, around the start
static void initEndless(World& W, uint64_t sd, const int32_t* origin) {
  const int WIN = World::WIN;
  W.seed = sd;
  W.genVersion = WORLDGEN_LATEST;
  W.endless = true;
  W.sites.clear(); W.gates.clear(); W.dens.clear(); W.wallGaps.clear(); W.kingdoms.clear();
  W.siteById.clear(); W.bldgById.clear(); W.denById.clear(); W.kingdomById.clear(); W.spawnKeys.clear(); W.gateKeys.clear();
  W.nearSites.clear(); W.nearDens.clear(); W.siteSpawns.clear();
  W.startSite = 0; W.capital = 0; W.lair = -1;
  W.streamer.reset();   // a new seed: the old prefetcher's chunks belong to another world
  W.over = Map();
  W.over.kind = MapKind::Overworld;
  W.over.alloc(WIN, WIN, Ground::Grass);
  W.over.biome.assign((size_t)WIN * WIN, (uint8_t)Biome::Plains);
  W.over.height.assign((size_t)WIN * WIN, 0);
  W.over.seed = (uint32_t)(sd ^ (sd >> 32));
  W.src = std::make_shared<ew::EndlessSource>(sd);
  const ew::StartPlan sp = W.src->start();
  const int32_t gx = origin ? floorTo(origin[0], ew::CHUNK) : floorTo(sp.spawn.x - WIN / 2, ew::CHUNK);
  const int32_t gy = origin ? floorTo(origin[1], ew::CHUNK) : floorTo(sp.spawn.y - WIN / 2, ew::CHUNK);
  // stream everything: a fresh window (no old tiles to keep)
  W.ox = gx + 100000 * ew::CHUNK;   // far away, so placeWindow keeps nothing
  W.oy = gy;
  W.placeWindow(gx, gy);
  W.windowShifts = 0;
  W.sstats = World::StreamStats();
  W.startSite = W.ensureSite(sp.village);
  W.capital = W.ensureSite(sp.capital);
  W.lair = W.ensureSite(sp.lair);
  for (ew::Gid g : sp.shards) { int h = W.ensureSite(g); if (h >= 0) W.sites[(size_t)h].mainQuest = true; }
  if (W.startSite < 0) W.startSite = 0;
  if (W.capital < 0) W.capital = W.startSite;
  if (!W.sites.empty()) W.sites[(size_t)W.startSite].discovered = true;
  W.rebuildNear();
}

void World::generateEndless(uint64_t sd) { initEndless(*this, sd, nullptr); }

void World::generateEndlessAt(uint64_t sd, int32_t originX, int32_t originY) {
  const int32_t o[2] = {originX, originY};
  initEndless(*this, sd, o);
}

void World::recentreOn(int32_t gx, int32_t gy) {
  placeWindow(floorTo(gx - WIN / 2, ew::CHUNK), floorTo(gy - WIN / 2, ew::CHUNK));
}

void World::shiftWindow(int sx, int sy) { placeWindow(ox + sx, oy + sy); }

void World::placeWindow(int32_t nox, int32_t noy) {
  if (!endless || !src) return;
  const auto t0 = std::chrono::steady_clock::now();
  const int32_t dx = nox - ox, dy = noy - oy;
  // translate every window-local record
  for (Site& s : sites) { s.r.x -= dx; s.r.y -= dy; s.ex -= dx; s.ey -= dy; }
  for (Bldg& b : over.bldgs) { b.r.x -= dx; b.r.y -= dy; }
  for (Den& d : dens) { d.x -= dx; d.y -= dy; }
  for (auto& g : gates) { g.first -= dx; g.second -= dy; }
  for (IRect& r : wallGaps) { r.x -= dx; r.y -= dy; }
  for (Spawn& sp : over.spawns) { sp.x -= dx; sp.y -= dy; }
  // keep the tiles both windows share; stream the chunks that are new
  const size_t n = (size_t)WIN * WIN;
  const bool keep = std::abs(dx) < WIN && std::abs(dy) < WIN && over.ground.size() == n;
  // an ordinary walking shift (as opposed to a teleport, a load or a new game): it must find its chunks ready
  const bool walking = keep && std::abs(dx) <= WIN_SHIFT && std::abs(dy) <= WIN_SHIFT;
  if (keep) {
    // move the kept rows in place: walk rows (and copy within each row) in the direction that never overwrites a
    // source not yet read (memmove handles the overlap inside a row)
    auto shiftLayer = [&](std::vector<uint8_t>& L) {
      const int x0 = std::max(0, -dx), x1 = std::min(WIN, WIN - dx);
      if (x0 >= x1) return;
      const size_t len = (size_t)(x1 - x0);
      if (dy >= 0) {
        for (int y = 0; y < WIN; y++) {
          int sy = y + dy;
          if (sy >= WIN) break;
          std::memmove(&L[(size_t)y * WIN + x0], &L[(size_t)sy * WIN + x0 + dx], len);
        }
      } else {
        for (int y = WIN - 1; y >= 0; y--) {
          int sy = y + dy;
          if (sy < 0) break;
          std::memmove(&L[(size_t)y * WIN + x0], &L[(size_t)sy * WIN + x0 + dx], len);
        }
      }
    };
    shiftLayer(over.ground); shiftLayer(over.prop); shiftLayer(over.wall); shiftLayer(over.deco); shiftLayer(over.biome);
    shiftLayer(over.height);
  } else {
    over.ground.assign(n, (uint8_t)Ground::Grass); over.prop.assign(n, 0); over.wall.assign(n, 0); over.deco.assign(n, 0);
    over.biome.assign(n, (uint8_t)Biome::Plains); over.height.assign(n, 0);
  }
  over.w = WIN; over.h = WIN;
  over.solid.assign(n, 0);
  over.bldgAt.assign(n, -1);
  const int32_t oldOx = ox, oldOy = oy;
  ox = nox; oy = noy;
  loadRegionsAround();
  const int nc = WIN / ew::CHUNK;
  ew::ChunkData c;
  for (int cy = 0; cy < nc; cy++)
    for (int cx = 0; cx < nc; cx++) {
      int32_t gcx = (ox >> ew::CHUNK_SHIFT) + cx, gcy = (oy >> ew::CHUNK_SHIFT) + cy;
      // was this chunk inside the old window?
      int32_t gx = gcx * ew::CHUNK, gy = gcy * ew::CHUNK;
      bool had = keep && gx >= oldOx && gy >= oldOy && gx < oldOx + WIN && gy < oldOy + WIN;
      if (had) continue;
      if (streamer && streamer->takeChunk(gcx, gcy, c)) sstats.prefetched++;
      else {
        src->chunk(gcx, gcy, c);
        sstats.syncChunks++;
        if (walking) sstats.syncInShifts++;
      }
      applyChunk(*this, c, true);
    }
  rebuildNear();
  // every cave and ruin in the window can be entered: its entrance tile carries its door (a generator that paints a
  // road or a stamp over it would lock a dungeon, a shard ruin even, away for good; counted so tests can report it)
  for (int si : nearSites) {
    const Site& s = sites[(size_t)si];
    if ((s.type != SiteType::Cave && s.type != SiteType::Ruin) || !over.in(s.ex, s.ey)) continue;
    uint8_t& pr = over.prop[(size_t)s.ey * WIN + s.ex];
    const uint8_t want = (uint8_t)((int)(s.type == SiteType::Ruin ? art::Prop::IronDoor : art::Prop::CaveEntrance) + 1);
    if (pr != (uint8_t)((int)art::Prop::IronDoor + 1) && pr != (uint8_t)((int)art::Prop::CaveEntrance + 1)) {
      if (std::getenv("EMB_DEBUG_DOORS"))
        std::printf("door repaired: %s %s at %d,%d (prop %d, ground %d)\n", siteTypeName(s.type), s.name.c_str(), ox + s.ex, oy + s.ey, (int)pr - 1,
                    (int)over.at(s.ex, s.ey));
      pr = want;
      sstats.entrancesRepaired++;
    }
  }
  over.rebuildSolid();
  if (over.spawns.size() > spawnCap || gates.size() + wallGaps.size() > spawnCap) recycleFar();
  windowShifts++;
  sstats.shifts++;
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  sstats.lastMoveMs = ms;
  if (walking) { sstats.walkShifts++; sstats.worstShiftMs = std::max(sstats.worstShiftMs, ms); }
  else sstats.worstRecentreMs = std::max(sstats.worstRecentreMs, ms);
}

ew::RegionPlan World::regionPlan(int32_t rx, int32_t ry) {
  if (streamer)
    if (const ew::RegionPlan* r = streamer->region(rx, ry)) return *r;
  return src->region(rx, ry);
}

void World::loadRegionsAround() {
  int32_t r0x = ew::regionOf(ox - ew::REGION), r1x = ew::regionOf(ox + WIN + ew::REGION - 1);
  int32_t r0y = ew::regionOf(oy - ew::REGION), r1y = ew::regionOf(oy + WIN + ew::REGION - 1);
  for (int32_t ry = r0y; ry <= r1y; ry++)
    for (int32_t rx = r0x; rx <= r1x; rx++) {
      const ew::RegionPlan R = regionPlan(rx, ry);   // a copy: adding sites may build other regions
      for (const ew::SitePlan& p : R.sites) if (!siteById.count(p.id)) addSitePlan(p);
      for (const ew::DenPlan& dp : R.dens) {
        if (denById.count(dp.id)) continue;
        Den d;
        d.id = dp.id;
        d.x = dp.x - ox; d.y = dp.y - oy;
        d.mon = dp.mon;
        d.pack = dp.pack;
        denById[d.id] = (int)dens.size();
        dens.push_back(d);
      }
    }
}

int World::addKingdom(ew::Gid id) {
  if (!id) return -1;
  auto it = kingdomById.find(id);
  if (it != kingdomById.end()) return it->second;
  const ew::KingdomPlan* kp = src ? src->kingdom(id) : nullptr;
  if (!kp) return -1;
  Kingdom k;
  k.id = kp->id; k.name = kp->name; k.color = kp->color; k.color2 = kp->color2; k.emblem = kp->emblem;
  k.capitalId = kp->capital; k.gx = kp->gx; k.gy = kp->gy;
  kingdomById[id] = (int)kingdoms.size();
  kingdoms.push_back(k);
  return (int)kingdoms.size() - 1;
}

int World::addSitePlan(const ew::SitePlan& p) {
  auto it = siteById.find(p.id);
  if (it != siteById.end()) return it->second;
  Site s;
  s.id = p.id;
  s.type = p.type;
  s.name = p.name;
  s.r = IRect{p.gx - ox, p.gy - oy, p.w, p.h};
  s.ex = p.ex - ox; s.ey = p.ey - oy;
  s.level = p.level;
  s.seed = p.seed;
  s.mainQuest = (p.flags & ew::SPF_MAINQUEST) != 0;
  s.theme = p.theme;
  s.genVer = WORLDGEN_LATEST;
  s.capital = (p.flags & ew::SPF_CAPITAL) != 0;
  s.start = (p.flags & ew::SPF_START) != 0;
  s.archetype = (uint8_t)p.archetype;
  s.bldgFirst = (int)over.bldgs.size();
  s.bldgCount = 0;
  int h = (int)sites.size();
  siteById[p.id] = h;
  sites.push_back(s);
  sites[(size_t)h].kingdom = addKingdom(p.kingdom);
  const IRect& r = sites[(size_t)h].r;
  if (r.x + r.w > -NEAR_MARGIN && r.y + r.h > -NEAR_MARGIN && r.x < WIN + NEAR_MARGIN && r.y < WIN + NEAR_MARGIN) nearSites.push_back(h);
  return h;
}

int World::ensureSite(ew::Gid id) {
  if (!id) return -1;
  int h = siteHandle(id);
  if (h >= 0 || !endless || !src) return h;
  if (ew::idKind(id) != ew::IdKind::Site) return -1;   // not a site id at all (a damaged save): nothing to load
  const ew::RegionPlan R = regionPlan(ew::idRx(id), ew::idRy(id));
  for (const ew::SitePlan& p : R.sites) if (p.id == id) return addSitePlan(p);
  return -1;
}

int World::ensureDen(ew::Gid id) {
  if (!id) return -1;
  auto it = denById.find(id);
  if (it != denById.end()) return it->second;
  if (!endless || !src || ew::idKind(id) != ew::IdKind::Den) return -1;
  const ew::RegionPlan R = regionPlan(ew::idRx(id), ew::idRy(id));
  for (const ew::DenPlan& dp : R.dens) {
    if (denById.count(dp.id)) continue;
    Den d;
    d.id = dp.id; d.x = dp.x - ox; d.y = dp.y - oy; d.mon = dp.mon; d.pack = dp.pack;
    denById[d.id] = (int)dens.size();
    if (nearWindow(d.x, d.y)) nearDens.push_back((int)dens.size());
    dens.push_back(d);
  }
  it = denById.find(id);
  return it == denById.end() ? -1 : it->second;
}

void World::ensureSiteRecords(int site) {
  if (!endless || !src || site < 0 || site >= (int)sites.size()) return;
  const Site& s = sites[(size_t)site];
  if (s.bldgCount > 0 || !s.settlement()) return;
  // the chunk under the site's heart carries the whole site's records
  streamChunk(ew::chunkOf(ox + s.ex), ew::chunkOf(oy + s.ey), false);
}

void World::streamChunk(int32_t gcx, int32_t gcy, bool tiles) {
  ew::ChunkData c;
  if (!(streamer && streamer->takeChunk(gcx, gcy, c))) src->chunk(gcx, gcy, c);
  applyChunk(*this, c, tiles);
}

void World::rebuildNear() {
  nearSites.clear();
  nearDens.clear();
  if (!endless) return;
  for (int i = 0; i < (int)sites.size(); i++) {
    const IRect& r = sites[(size_t)i].r;
    if (r.x + r.w > -NEAR_MARGIN && r.y + r.h > -NEAR_MARGIN && r.x < WIN + NEAR_MARGIN && r.y < WIN + NEAR_MARGIN) nearSites.push_back(i);
  }
  for (int i = 0; i < (int)dens.size(); i++)
    if (nearWindow(dens[(size_t)i].x, dens[(size_t)i].y)) nearDens.push_back(i);
}

void World::rebuildSiteSpawns() {
  siteSpawns.clear();
  for (int i = 0; i < (int)over.spawns.size(); i++)
    if (over.spawns[(size_t)i].site >= 0) siteSpawns[over.spawns[(size_t)i].site].push_back(i);
}

void World::recycleFar() {
  // far = the site's heart (or the record) more than a window and a half from the window's middle: well outside
  // every activation radius, so nothing in play refers to these records (actors key on site + slot, never on spawns)
  const int R = WIN + WIN / 2, c = WIN / 2;
  auto farTile = [&](int x, int y) { return std::abs(x - c) > R || std::abs(y - c) > R; };
  std::vector<Spawn> keepS;
  keepS.reserve(over.spawns.size());
  for (const Spawn& s : over.spawns) {
    bool far = s.site >= 0 && s.site < (int)sites.size() && farTile(sites[(size_t)s.site].ex, sites[(size_t)s.site].ey);
    if (!far) { keepS.push_back(s); continue; }
    spawnKeys.erase(spawnKeyOf(sites[(size_t)s.site].id, s.slot));
    sstats.recycled++;
  }
  over.spawns.swap(keepS);
  std::vector<std::pair<int, int>> keepG;
  for (auto& g : gates) {
    if (!farTile(g.first, g.second)) keepG.push_back(g);
    else { gateKeys.erase(posKey(ox + g.first, oy + g.second, 1)); sstats.recycled++; }
  }
  gates.swap(keepG);
  std::vector<IRect> keepW;
  for (const IRect& r : wallGaps) {
    if (!farTile(r.x, r.y)) keepW.push_back(r);
    else { gateKeys.erase(gapKeyOf(ox + r.x, oy + r.y, r.w, r.h)); sstats.recycled++; }
  }
  wallGaps.swap(keepW);
  rebuildSiteSpawns();
}

void World::prefetch(int ptx, int pty, int dirx, int diry) {
  if (!endless || !streamer) return;
  const int nc = WIN / ew::CHUNK, RING = 2;
  const int32_t c0x = ox >> ew::CHUNK_SHIFT, c0y = oy >> ew::CHUNK_SHIFT;
  // the region plans loadRegionsAround asks for with the window at (wx, wy)
  struct RBox { int32_t x0, y0, x1, y1; };
  auto regionsAt = [&](int32_t wx, int32_t wy) {
    return RBox{ew::regionOf(wx - ew::REGION), ew::regionOf(wy - ew::REGION), ew::regionOf(wx + WIN + ew::REGION - 1), ew::regionOf(wy + WIN + ew::REGION - 1)};
  };
  auto inBox = [](const RBox& b, int32_t x, int32_t y) { return x >= b.x0 && y >= b.y0 && x <= b.x1 && y <= b.y1; };
  const RBox now = regionsAt(ox, oy);
  std::vector<ChunkStreamer::Key> keys;
  auto addRegion = [&](int32_t rx, int32_t ry) {
    for (const auto& k : keys) if (k.region && k.x == rx && k.y == ry) return;
    ChunkStreamer::Key k; k.x = rx; k.y = ry; k.region = true;
    keys.push_back(k);
  };
  // 1. the region plans the next shift the way the player is heading will need (a row or column of a few regions):
  //    without them the shift would plan regions on the main thread
  {
    std::vector<std::pair<int, int>> dirs;
    if (dirx || diry) { if (dirx) dirs.push_back({dirx, 0}); if (diry) dirs.push_back({0, diry}); if (dirx && diry) dirs.push_back({dirx, diry}); }
    else dirs = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (auto& d : dirs) {
      const RBox nx = regionsAt(ox + d.first * WIN_SHIFT, oy + d.second * WIN_SHIFT);
      for (int32_t ry = nx.y0; ry <= nx.y1; ry++)
        for (int32_t rx = nx.x0; rx <= nx.x1; rx++)
          if (!inBox(now, rx, ry)) addRegion(rx, ry);
    }
  }
  // 2. the ring of chunks just outside the window (what the next shifts copy in), nearest the player first, the way
  //    they are heading before the way they came
  std::vector<std::pair<float, ChunkStreamer::Key>> ck;
  for (int cy = -RING; cy < nc + RING; cy++)
    for (int cx = -RING; cx < nc + RING; cx++) {
      if (cx >= 0 && cy >= 0 && cx < nc && cy < nc) continue;
      float ddx = (float)(cx * ew::CHUNK + ew::CHUNK / 2 - ptx), ddy = (float)(cy * ew::CHUNK + ew::CHUNK / 2 - pty);
      float d = std::sqrt(ddx * ddx + ddy * ddy);
      float along = ddx * (float)dirx + ddy * (float)diry;
      ChunkStreamer::Key k; k.x = c0x + cx; k.y = c0y + cy;
      ck.push_back({d - 0.75f * along, k});
    }
  std::stable_sort(ck.begin(), ck.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  for (auto& c : ck) keys.push_back(c.second);
  // 3. every other region plan a shift in any direction could ask for
  {
    const RBox all = regionsAt(ox - WIN_SHIFT, oy - WIN_SHIFT), all2 = regionsAt(ox + WIN_SHIFT, oy + WIN_SHIFT);
    for (int32_t ry = all.y0; ry <= all2.y1; ry++)
      for (int32_t rx = all.x0; rx <= all2.x1; rx++)
        if (!inBox(now, rx, ry)) addRegion(rx, ry);
  }
  streamer->want(keys);
  streamer->trim(c0x - RING - 2, c0y - RING - 2, c0x + nc + RING + 2, c0y + nc + RING + 2, 140);
}

int World::endlessDanger(int tx, int ty) const { return src ? src->danger(ox + tx, oy + ty) : 1; }

int World::findSiteNear(int32_t gx, int32_t gy, SiteType t, int regions, bool capitalOnly) {
  if (!endless || !src) return -1;
  const int32_t rx0 = ew::regionOf(gx), ry0 = ew::regionOf(gy);
  ew::Gid best = 0;
  int64_t bd = INT64_MAX;
  // ring by ring: a site in ring r lies at least (r - 1) regions away, so stop once that is beyond the best so far
  for (int r = 0; r <= regions; r++) {
    if (best && (int64_t)(r - 1) * ew::REGION * (r - 1) * ew::REGION > bd) break;
    for (int32_t ry = ry0 - r; ry <= ry0 + r; ry++)
      for (int32_t rx = rx0 - r; rx <= rx0 + r; rx++) {
        if (std::max(std::abs(rx - rx0), std::abs(ry - ry0)) != r) continue;
        const ew::RegionPlan R = regionPlan(rx, ry);
        for (const ew::SitePlan& p : R.sites) {
          if (p.type != t || (capitalOnly && !(p.flags & ew::SPF_CAPITAL))) continue;
          int64_t dx = p.ex - gx, dy = p.ey - gy, d = dx * dx + dy * dy;
          if (d < bd) { bd = d; best = p.id; }
        }
      }
  }
  return best ? ensureSite(best) : -1;
}
