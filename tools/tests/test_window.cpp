// rpg_test --window [--seeds A..B] [--walk TILES] [--speed TILES_PER_S] [--fast] [--web] [--budget MS]: the endless Active
// Window in a real Game (VISION_PLAN 2.8, 2.9). SIM lane.
//   - a new endless game: start village with an inn (the opening quest's giver), the story city, the lair, 3 shard ruins
//   - a long walk east, then south, then back west, at a brisk pace (default 16 tiles/s, twice a mounted rider) with the
//     main thread paced at 0.5 ms per 60 Hz step (30x faster than real time, so the prefetcher has far less time than
//     in play). Every window shift keeps the player inside the window's middle, the window's tiles always equal what
//     the generator makes for those global chunks (no stale strips, no seams from the shift), and a walking shift
//     finds its chunks ready (the prefetcher kept up). --fast: the lead's original stress walk (2 tiles a step, no
//     pacing: the prefetcher cannot keep up, so it measures the synchronous fallback).
//   - the worst step (ms) of the walk is reported; --budget MS makes a worse step a failure (the M1 target is 4 ms)
//   - sites keep their global place (Site::ex + ox is constant across shifts), buildings their footprints
//   - the spatial look-ups (nearSites, siteAt, settlementAt) agree with a full scan of every record
//   - fast travel to the story city recentres the window
//   - the save round trip is byte-identical (SAVE_VER 5 with stable ids), also after walking far and reloading
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "rpg/sim/stream.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

struct WinOpts {
  int walk = 600;
  float speed = 16.0f;      // tiles per second
  bool fast = false;
  double budget = 0;        // > 0: a worse step fails
  bool verbose = false;     // --verbose: report every step over 2 ms and what it did
  bool web = false;         // --web: stream as the web build does (no worker thread; each step pumps 3 ms of work)
};

int checkWindowTiles(Game& g, const char* when) {
  int bad = 0;
  World& w = g.world;
  ew::EndlessSource fresh(w.seed);
  ew::ChunkData c;
  const int nc = World::WIN / ew::CHUNK;
  // corner chunks and the middle (the strips a shift streams in are at the edges)
  const int picks[][2] = {{0, 0}, {nc - 1, 0}, {0, nc - 1}, {nc - 1, nc - 1}, {nc / 2, nc / 2}, {nc - 2, nc / 2}, {nc / 2, nc - 2}};
  for (auto& pk : picks) {
    int32_t gcx = (w.ox >> ew::CHUNK_SHIFT) + pk[0], gcy = (w.oy >> ew::CHUNK_SHIFT) + pk[1];
    fresh.chunk(gcx, gcy, c);
    int mism = 0;
    for (int y = 0; y < ew::CHUNK; y++)
      for (int x = 0; x < ew::CHUNK; x++) {
        int lx = pk[0] * ew::CHUNK + x, ly = pk[1] * ew::CHUNK + y;
        size_t i = (size_t)ly * w.over.w + lx;
        if (w.over.ground[i] != c.ground[y * ew::CHUNK + x]) mism++;
        if (w.over.biome[i] != c.biome[y * ew::CHUNK + x]) mism++;
        if (w.over.height[i] != c.height[y * ew::CHUNK + x]) mism++;
        if (w.over.wall[i] != c.wall[y * ew::CHUNK + x]) mism++;
      }
    if (mism) { out("FAIL: %s: window chunk (%d,%d) differs from the generator in %d tiles\n", when, pk[0], pk[1], mism); bad++; }
  }
  return bad;
}

// the spatial look-ups against a full scan of every record
int checkLookups(Game& g, const char* when) {
  int bad = 0;
  const World& w = g.world;
  auto fullSiteAt = [&](int tx, int ty, int pad) {
    for (int i = 0; i < (int)w.sites.size(); i++) {
      const IRect& r = w.sites[(size_t)i].r;
      if (tx >= r.x - pad && ty >= r.y - pad && tx < r.x + r.w + pad && ty < r.y + r.h + pad) return i;
    }
    return -1;
  };
  int mism = 0;
  for (int y = 0; y < World::WIN; y += 3)
    for (int x = 0; x < World::WIN; x += 3)
      for (int pad : {0, 2, 3}) if (w.siteAt(x, y, pad) != fullSiteAt(x, y, pad)) mism++;
  if (mism) { out("FAIL: %s: siteAt (near records) disagrees with a full scan at %d probes\n", when, mism); bad++; }
  // every site whose area meets the window is in nearSites
  int missing = 0;
  for (int i = 0; i < (int)w.sites.size(); i++) {
    const IRect& r = w.sites[(size_t)i].r;
    bool meets = r.x + r.w > 0 && r.y + r.h > 0 && r.x < World::WIN && r.y < World::WIN;
    if (meets && std::find(w.nearSites.begin(), w.nearSites.end(), i) == w.nearSites.end()) missing++;
  }
  if (missing) { out("FAIL: %s: %d sites in the window are missing from nearSites\n", when, missing); bad++; }
  // every spawn is indexed under its site
  size_t indexed = 0;
  for (auto& kv : w.siteSpawns) indexed += kv.second.size();
  size_t withSite = 0;
  for (const Spawn& s : w.over.spawns) if (s.site >= 0) withSite++;
  if (indexed != withSite) { out("FAIL: %s: siteSpawns indexes %zu spawns of %zu\n", when, indexed, withSite); bad++; }
  return bad;
}

int windowSeed(uint64_t seed, const WinOpts& o) {
  int bad = 0;
  auto fail = [&](const std::string& s) { out("FAIL: %s\n", s.c_str()); bad++; };
  auto t0 = std::chrono::steady_clock::now();
  Game g(seed);
  g.newEndlessGame(seed);
  double newMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  g.mode = Mode::Play;
  g.godMode = true;
  g.noWildSpawns = true;
  g.streamThreads = !o.web;
  World& w = g.world;
  if (!w.endless) { fail("not an endless world"); return bad; }
  const Site& home = w.sites[(size_t)w.startSite];
  if (!home.settlement() || !home.start) fail("start site is not the start settlement");
  bool inn = false;
  for (int b = home.bldgFirst; b < home.bldgFirst + home.bldgCount; b++) if (w.over.bldgs[(size_t)b].type == art::Building::Inn) inn = true;
  if (!inn) fail("the start village has no inn");
  if (w.capital < 0 || w.sites[(size_t)w.capital].type != SiteType::City) fail("no story city");
  if (w.lair < 0 || w.sites[(size_t)w.lair].type != SiteType::DragonLair) fail("no lair");
  int shards = 0;
  for (const Site& s : w.sites) if (s.mainQuest) shards++;
  if (shards != 3) fail("main-quest ruins: " + std::to_string(shards));
  bad += checkWindowTiles(g, "start");
  bad += checkLookups(g, "start");
  // interiors and dungeons come from their record's seed: the start inn's ground floor and the nearest cave, now and
  // again after the walk (the records moved with every shift; their maps must not)
  auto mapHash = [](const Map& m) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < m.ground.size(); i++) { h = (h ^ m.ground[i]) * 1099511628211ull; h = (h ^ m.prop[i]) * 1099511628211ull; }
    return h ^ ((uint64_t)m.w << 32) ^ (uint64_t)m.h;
  };
  ew::Gid innId = 0, caveId = 0;
  uint64_t innHash = 0, caveHash = 0;
  for (int b = home.bldgFirst; b < home.bldgFirst + home.bldgCount; b++)
    if (w.over.bldgs[(size_t)b].type == art::Building::Inn) {
      Map im;
      genInterior(im, w.over.bldgs[(size_t)b], w.over.bldgs[(size_t)b].seed, 0);
      innId = w.over.bldgs[(size_t)b].id; innHash = mapHash(im);
      break;
    }
  {
    int cave = w.nearestSite(home.ex, home.ey, SiteType::Cave);
    if (cave >= 0) {
      Map cm;
      genCave(cm, w.sites[(size_t)cave], w.sites[(size_t)cave].seed);
      caveId = w.sites[(size_t)cave].id; caveHash = mapHash(cm);
    }
  }
  // remember global places
  struct Gp { ew::Gid id; int32_t gx, gy; };
  std::vector<Gp> places;
  for (const Site& s : w.sites) places.push_back({s.id, w.ox + s.ex, w.oy + s.ey});
  // the walk
  const int legs[3][2] = {{1, 0}, {0, 1}, {-1, 0}};
  int shifts0 = w.sstats.walkShifts;
  double worstStep = 0, sumStep = 0;
  std::vector<float> stepMs;   // every step (percentiles: a busy machine preempts a single step now and then)
  long steps = 0;
  const float perStep = o.fast ? 2.0f * TILE : o.speed * TILE / 60.0f;   // pixels per 60 Hz step
  const double pace = o.fast || o.web ? 0.0 : 0.5;                                // ms of wall time per step at least
  for (auto& L : legs) {
    float walked = 0;
    while (walked < (float)o.walk * TILE) {
      auto s0 = std::chrono::steady_clock::now();
      g.pl().p += Vec2(L[0] * perStep, L[1] * perStep);
      walked += perStep;
      // the walk passes over everything; stepping onto a door would walk in, so a door tile is stepped over
      for (int k = 0; k < 8; k++) {
        int fx = (int)std::floor(g.pl().p.x / TILE), fy = (int)std::floor((g.pl().p.y - 2) / TILE);
        const Map& m = g.world.over;
        int bi = m.in(fx, fy) ? m.bldgAt[(size_t)fy * m.w + fx] : -1;
        int pr = m.propAt(fx, fy);
        bool door = bi >= 0 && m.bldgs[(size_t)bi].doorX() == fx && m.bldgs[(size_t)bi].doorY() == fy;
        bool mouth = pr == (int)art::Prop::CaveEntrance + 1 || pr == (int)art::Prop::IronDoor + 1;
        if (!door && !mouth) break;
        g.pl().p += Vec2(L[0] * (float)TILE, L[1] * (float)TILE + (L[1] == 0 ? (float)TILE : 0.0f));
      }
      const int sh0 = w.sstats.shifts, mc0 = w.src->stats().chunks, mr0 = w.src->stats().regions;
      const size_t a0 = g.actors.size(), sites0 = w.sites.size(), k0 = w.kingdoms.size(), b0 = w.over.bldgs.size();
      const double rms0 = w.src->stats().regionMs, cms0 = w.src->stats().chunkMs;
      g.update(SIM_DT, Input());
      if (o.web) g.frameWork(3.0);   // the web's per-frame generation budget (rpg/main.cpp)
      g.events.clear();
      if (g.mode != Mode::Play) g.mode = Mode::Play;
      double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s0).count();
      if (o.verbose && ms > 2.0)
        out("  slow step %.2f ms: shift %d, main chunks +%d, main regions +%d, actors %zu -> %zu, sites %zu -> %zu, kingdoms %zu -> %zu, buildings %zu -> %zu, main region ms %.1f, main chunk ms %.1f\n", ms,
            w.sstats.shifts - sh0, w.src->stats().chunks - mc0, w.src->stats().regions - mr0, a0, g.actors.size(), sites0, w.sites.size(), k0, w.kingdoms.size(), b0, w.over.bldgs.size(),
            w.src->stats().regionMs - rms0, w.src->stats().chunkMs - cms0);
      worstStep = std::max(worstStep, ms);
      sumStep += ms;
      stepMs.push_back((float)ms);
      steps++;
      while (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s0).count() < pace) {}
      int tx = (int)(g.pl().p.x / TILE), ty = (int)(g.pl().p.y / TILE);
      if (tx < World::WIN_SHIFT - 3 || ty < World::WIN_SHIFT - 3 || tx >= World::WIN - World::WIN_SHIFT + 3 || ty >= World::WIN - World::WIN_SHIFT + 3) {
        fail("player left the window's middle at local tile " + std::to_string(tx) + "," + std::to_string(ty));
        break;
      }
    }
    bad += checkWindowTiles(g, "after a walk leg");
    bad += checkLookups(g, "after a walk leg");
  }
  int shifts = w.sstats.walkShifts - shifts0;
  if (shifts < 2 && o.walk >= 200) fail("the walk shifted the window only " + std::to_string(shifts) + " times");
  if (!o.fast && w.sstats.syncInShifts > 0)
    out("WARN: %d chunks were generated on the main thread during walking shifts (the prefetcher fell behind)\n", w.sstats.syncInShifts);
  if (o.budget > 0 && worstStep > o.budget) fail("worst step " + std::to_string(worstStep) + " ms is over the budget of " + std::to_string(o.budget) + " ms");
  int moved = 0;
  for (const Gp& p : places) {
    int h = w.siteHandle(p.id);
    if (h < 0 || w.ox + w.sites[(size_t)h].ex != p.gx || w.oy + w.sites[(size_t)h].ey != p.gy) moved++;
  }
  if (moved) fail(std::to_string(moved) + " sites moved in global terms");
  for (const Bldg& b : w.over.bldgs)
    if (w.over.in(b.r.x, b.r.y) && w.over.in(b.r.x + b.r.w - 1, b.r.y + b.r.h - 1) && w.over.bldgAt[(size_t)b.r.y * w.over.w + b.r.x] < 0) { fail("a building in the window has no footprint"); break; }
  // the interiors again, far from home: the same maps; going into the inn from afar and out again brings the window
  // back to it
  if (innId) {
    int bi = w.bldgHandle(innId);
    if (bi < 0) fail("the start inn's record is gone after the walk");
    else {
      Map im;
      genInterior(im, w.over.bldgs[(size_t)bi], w.over.bldgs[(size_t)bi].seed, 0);
      if (mapHash(im) != innHash) fail("the start inn's interior changed after the walk");
    }
  }
  if (caveId) {
    int ci = w.siteHandle(caveId);
    Map cm;
    if (ci >= 0) genCave(cm, w.sites[(size_t)ci], w.sites[(size_t)ci].seed);
    if (ci < 0 || mapHash(cm) != caveHash) fail("the nearest cave's map changed after the walk");
  }
  std::vector<uint8_t> s1, s2;
  {
    Game far = g;   // (a copy: the walk's game goes on below)
    int bi = innId ? far.world.bldgHandle(innId) : -1;
    if (bi >= 0 && far.debugEnterBuilding(bi, 0)) {
      Map im;
      genInterior(im, far.world.over.bldgs[(size_t)bi], far.world.over.bldgs[(size_t)bi].seed, 0);
      // (the sub-map is the generated one plus its looted chests: none here)
      if (far.sub.w != im.w || far.sub.h != im.h) fail("entering the start inn from afar gave another interior");
      far.pl().p = Vec2(far.sub.exitX * 16 + 8.0f, (far.sub.exitY - 2) * 16 + 8.0f);
      Input down; down.move = Vec2(0, 1);
      for (int f = 0; f < 120 && far.inside; f++) far.update(SIM_DT, down);
      int tx = (int)(far.pl().p.x / TILE), ty = (int)(far.pl().p.y / TILE);
      const Bldg& B = far.world.over.bldgs[(size_t)bi];
      if (far.inside) fail("could not walk out of the start inn entered from afar");
      else if (!far.world.over.in(tx, ty) || std::abs(tx - B.doorX()) > 2 || std::abs(ty - B.doorY() - 1) > 2)
        fail("walking out of the start inn did not bring the player (and the window) to its door");
      else bad += checkWindowTiles(far, "after leaving the start inn from afar");
    } else if (innId) fail("could not enter the start inn from afar");
  }
  // the save round trip, far from home
  g.serialize(s1);
  Game h(seed);
  if (!h.deserialize(s1)) fail("the endless save did not load");
  else {
    h.serialize(s2);
    if (s1 != s2) fail("endless save round trip is not byte-identical (" + std::to_string(s1.size()) + " vs " + std::to_string(s2.size()) + " bytes)");
    if (std::fabs(h.pl().p.x - g.pl().p.x) > 0.01f || h.world.ox != w.ox || h.world.oy != w.oy) fail("reload put the player somewhere else");
    bad += checkWindowTiles(h, "after reload");
  }
  // fast travel home
  w.sites[(size_t)w.capital].discovered = true;
  if (!g.fastTravel(w.capital)) fail("fast travel to the story city refused");
  else {
    int tx = (int)(g.pl().p.x / TILE), ty = (int)(g.pl().p.y / TILE);
    if (!w.over.in(tx, ty)) fail("fast travel left the player outside the window");
    bad += checkWindowTiles(g, "after fast travel");
    bad += checkLookups(g, "after fast travel");
  }
  const ew::EndlessSource::Stats& st = w.src->stats();
  ChunkStreamer::Stats ps;
  if (w.streamer) ps = w.streamer->stats();
  std::sort(stepMs.begin(), stepMs.end());
  const double p999 = stepMs.empty() ? 0.0 : stepMs[(size_t)((stepMs.size() - 1) * 0.999)];
  int over4 = 0;
  for (float v : stepMs) if (v > 4.0f) over4++;
  out("seed %llu: %zu steps, p99.9 %.2f ms, %d over 4 ms\n", (unsigned long long)seed, stepMs.size(), p999, over4);
  out("seed %llu: new game %.0f ms, %d walking shifts, worst step %.2f ms (avg %.3f), worst shift %.1f ms, worst recentre %.1f ms | "
      "chunks prefetched %d, made in a walking shift %d, in recentres %d | records: sites %zu, buildings %zu, spawns %zu, dens %zu, "
      "kingdoms %zu | save %zu B | worker chunks %d (avg %.2f ms, max %.1f) regions %d, work %.0f ms | main chunks %d (%.0f ms) "
      "regions %d (%.0f ms) settlements %d (avg %.1f ms)\n",
      (unsigned long long)seed, newMs, shifts, worstStep, steps ? sumStep / steps : 0.0, w.sstats.worstShiftMs, w.sstats.worstRecentreMs,
      w.sstats.prefetched, w.sstats.syncInShifts, w.sstats.syncChunks - w.sstats.syncInShifts, w.sites.size(), w.over.bldgs.size(),
      w.over.spawns.size(), w.dens.size(), w.kingdoms.size(), s1.size(), ps.chunksMade, ps.chunksMade ? ps.workMs / ps.chunksMade : 0.0,
      ps.maxChunkMs, ps.regionsMade, ps.workMs, st.chunks, st.chunkMs, st.regions, st.regionMs, st.settlements,
      st.settlements ? st.settlementMs / st.settlements : 0.0);
  return bad;
}

// --soak TILES: a very long walk (a 2-hour session on foot is about 25 000 tiles), zig-zagging so it covers ground,
// unpaced. The session's records must stay bounded: spawns, gates and wall gaps are recycled once far away, and
// what stays (sites, buildings, dens: handles that quests and saves hold) stays small.
int soakSeed(uint64_t seed, int tiles) {
  int bad = 0;
  auto fail = [&](const std::string& s) { out("FAIL: soak: %s\n", s.c_str()); bad++; };
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play; g.godMode = true; g.noWildSpawns = true;
  World& w = g.world;
  const int legs[4][2] = {{1, 0}, {0, 1}, {1, 0}, {0, -1}};
  int walked = 0, leg = 0;
  size_t maxSpawns = 0, maxGates = 0;
  auto t0 = std::chrono::steady_clock::now();
  while (walked < tiles) {
    for (int t = 0; t < 900 && walked < tiles; t += 2, walked += 2) {
      g.pl().p += Vec2(legs[leg][0] * 2.0f * TILE, legs[leg][1] * 2.0f * TILE);
      g.update(SIM_DT, Input());
      g.events.clear();
      if (g.mode != Mode::Play) g.mode = Mode::Play;
      if (g.inside) { g.inside = false; g.sub = Map(); g.subBldg = -1; g.subSite = -1; }
      maxSpawns = std::max(maxSpawns, w.over.spawns.size());
      maxGates = std::max(maxGates, w.gates.size() + w.wallGaps.size());
    }
    leg = (leg + 1) % 4;
  }
  double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  size_t bytes = w.sites.size() * sizeof(Site) + w.over.bldgs.size() * sizeof(Bldg) + w.over.spawns.size() * sizeof(Spawn) +
                 w.dens.size() * sizeof(Den) + (w.gates.size() + w.wallGaps.size()) * sizeof(IRect) +
                 (w.siteById.size() + w.bldgById.size() + w.denById.size() + w.spawnKeys.size() + w.gateKeys.size()) * 32;
  for (const Site& st : w.sites) bytes += st.name.capacity();
  if (maxSpawns > World::SPAWN_SOFT_CAP + 2000) fail("spawn records grew to " + std::to_string(maxSpawns));
  if (bytes > 16u * 1024 * 1024) fail("the session's records take " + std::to_string(bytes / 1024) + " KB");
  if (w.over.bldgs.size() >= 0x7FFFFFFF / 2) fail("building handles near the limit");
  // recycling, exercised: with a small cap the far records go, and coming home brings the start village's people back
  {
    Game h(seed);
    h.newEndlessGame(seed);
    h.mode = Mode::Play; h.godMode = true; h.noWildSpawns = true;
    World& v = h.world;
    v.spawnCap = 150;
    const int home = v.startSite;
    const size_t homeSpawns = v.siteSpawns.count(home) ? v.siteSpawns[home].size() : 0;
    const Vec2 start = h.pl().p;
    const int32_t gx0 = v.ox + (int)(start.x / TILE), gy0 = v.oy + (int)(start.y / TILE);
    for (int t = 0; t < 2400; t += 2) { h.pl().p += Vec2(2.0f * TILE, 0); h.update(SIM_DT, Input()); h.events.clear(); h.mode = Mode::Play;
      if (h.inside) { h.inside = false; h.sub = Map(); h.subBldg = -1; h.subSite = -1; } }
    const bool gone = !v.siteSpawns.count(home) || v.siteSpawns[home].empty();
    h.teleportGlobal(gx0, gy0);
    const size_t back = v.siteSpawns.count(home) ? v.siteSpawns[home].size() : 0;
    if (v.sstats.recycled == 0) fail("nothing was recycled with a cap of 150 spawns");
    if (!gone) fail("the start village's spawns were not recycled 2400 tiles away");
    if (back != homeSpawns) fail("coming home brought back " + std::to_string(back) + " of the start village's " + std::to_string(homeSpawns) + " spawns");
    out("soak seed %llu: recycling with a cap of 150: %d records recycled, start village spawns %zu -> %s -> %zu\n", (unsigned long long)seed,
        v.sstats.recycled, homeSpawns, gone ? "gone" : "kept", back);
  }
  out("soak seed %llu: %d tiles in %.1f s, %d window moves | records: sites %zu, buildings %zu, dens %zu, spawns %zu (max %zu), gates+gaps %zu "
      "(max %zu), recycled %d | about %zu KB | fog regions %zu\n",
      (unsigned long long)seed, walked, s, w.sstats.shifts, w.sites.size(), w.over.bldgs.size(), w.dens.size(), w.over.spawns.size(), maxSpawns,
      w.gates.size() + w.wallGaps.size(), maxGates, w.sstats.recycled, bytes / 1024, g.explored.regions.size());
  return bad;
}

int cmdWindow(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  WinOpts o;
  int soak = 0;
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "--soak") && i + 1 < argc) soak = atoi(argv[++i]);
  if (soak > 0) {
    for (int i = 1; i < argc; i++)
      if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    int bad = 0;
    for (uint64_t s = a; s <= b; s++) { g_curSeed = s; bad += soakSeed(s, soak); }
    printf("window soak: %d failure(s)\n", bad);
    return bad ? 1 : 0;
  }
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--walk") && i + 1 < argc) o.walk = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--speed") && i + 1 < argc) o.speed = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--budget") && i + 1 < argc) o.budget = atof(argv[++i]);
    else if (!strcmp(argv[i], "--fast")) o.fast = true;
    else if (!strcmp(argv[i], "--verbose")) o.verbose = true;
    else if (!strcmp(argv[i], "--web")) o.web = true;
  }
  int bad = 0, failedSeeds = 0;
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    int f = windowSeed(s, o);
    bad += f;
    if (f) failedSeeds++;
  }
  printf("window: %llu seeds, %d failed (%d failures)\n", (unsigned long long)(b - a + 1), failedSeeds, bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--window", "endless Active Window: walk, shifts, prefetching, look-ups, fast travel, save round trip "
                         "[--seeds A..B] [--walk N] [--speed T/S] [--fast] [--web] [--budget MS]", cmdWindow);
