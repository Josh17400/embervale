// TerrainBaker (SOLID 4.2, View part 1): the terrain chunk cache and its bakers, a value member of View (View::bake_).
// It owns the baked chunks, the job queue, the worker threads, the web's incremental pump, the chunk frame drawWorld set,
// the player's houses whose ground was baked again, the prepared interior and the arrival behind the fade. The painters
// (View::groundPixel, reliefPixel, rockLevel) stay in terrain.cpp; the bake entry points that reach them take the View.
// Included by rpg/view/view.h; defined in terrain_bake.cpp (queue, threads, cache) and terrain.cpp (the bake itself).
#pragma once
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "engine/pix.h"
#include "rpg/world/map.h"

class View;
class Game;

struct TerrainBaker {
  // M1: terrain chunks (32 x 32 tiles, 512 px) are keyed by GLOBAL chunk coordinates (an endless window's origin
  // ox >> 5 plus the local chunk), so a window shift keeps every baked chunk. Each bake reads a snapshot of its chunk
  // plus a margin (kBakeMargin tiles) instead of a copy of the whole map, and paints with GLOBAL pixel coordinates, so
  // the noise is the same whichever window a chunk was baked in (no seams between old and new chunks).
  // TMap: a map seen through an origin, looked up in global tiles (classic islands and interiors: origin 0).
  struct TMap {
    const Map* m = nullptr;
    int ox = 0, oy = 0;   // the global tile of m's (0, 0)
    MapKind kind = MapKind::Overworld;
    Ground at(int x, int y) const { return m->at(x - ox, y - oy); }
    Biome biomeAt(int x, int y) const { return m->biomeAt(x - ox, y - oy); }
    uint8_t heightBits(int x, int y) const { return m->heightBits(x - ox, y - oy); }
    int heightAt(int x, int y) const { return m->heightAt(x - ox, y - oy); }
    bool relief() const { return !m->height.empty(); }
    uint8_t blendAt(int x, int y) const { return m->blendAt(x - ox, y - oy); }   // M2 ecotones (Map::blend)
    uint8_t urbAt(int x, int y) const { return m->urbAt(x - ox, y - oy); }       // M7b the urban fabric (rpg/world/urban.h)
    bool ecoDerive = false;   // M2: the map carries no blend bytes: the view derives ecotones from the biomes
    bool wallAt(int x, int y) const {   // a city wall piece stands on the tile
      const int lx = x - ox, ly = y - oy;
      return !m->wall.empty() && lx >= 0 && ly >= 0 && lx < m->w && ly < m->h && m->wall[(size_t)ly * m->w + lx] != 0;
    }
  };
  static constexpr int kBakeMargin = 8;
  static constexpr int kChunkTiles = 32;   // tiles per chunk side (terrain.cpp CH)
  struct Chunk { Tex tex; uint64_t key = 0; float used = 0; bool stale = false; };   // stale: (M7) to be baked again
  std::vector<Chunk> chunks_;
  int lastMapKey_ = -999;
  // background baking: the worker paints chunk Canvases from per-chunk snapshots
  struct BakeJob {
    std::shared_ptr<const Map> map;   // the chunk and its margin
    int ox = 0, oy = 0;               // the snapshot's origin in global tiles
    int gcx = 0, gcy = 0;             // the chunk in global chunk coordinates
    uint64_t key = 0, mapId = 0;
    bool ecoDerive = false;           // M2: no blend bytes in this map (TMap::ecoDerive)
  };
  struct BakeDone { uint64_t key = 0, mapId = 0; Canvas c; double ms = 0; };
  std::thread worker_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<BakeJob> jobs_;
  std::vector<BakeDone> done_;
  std::vector<uint64_t> pending_;
  bool quit_ = false;
  // the chunk frame drawWorld set for this map: global chunk = local chunk + (chunkOX_, chunkOY_); partial chunks
  // (an endless window's outer ring, whose margin runs off the window) get their own key and are baked again once
  // the window has moved over them
  int chunkOX_ = 0, chunkOY_ = 0;
  bool chunkEndless_ = false;
  void workerLoop(View& v);
  void clearChunks(View& v);
  uint64_t chunkKeyFor(const View& v, const Map& m, uint64_t mapId, int cx, int cy) const;
  // (M7 fixer r2) the player's houses appear in World::over.bldgs at runtime (home::stampWindow), after the ground
  // around them may already be baked: the chunks under a house that appeared, changed or went are baked again, so it
  // throws the same cast shadow as every generated building (homeHouses_: id, home, seed, global rect last seen)
  struct HomeHouse { uint64_t id = 0; uint8_t home = 0; uint32_t seed = 0; int32_t gx = 0, gy = 0; int w = 0, h = 0; };
  std::vector<HomeHouse> homeHouses_;
  uint64_t homeHousesMap_ = 0;
  void rebakeHomeHouses(View& v, const Game& g, const Map& m, uint64_t mapId);
  BakeJob makeJob(const View& v, const Map& m, uint64_t mapId, int cx, int cy) const;
  void prefetch(View& v, const Map& m, uint64_t mapId, Vec2 cam);
  Tex chunkTex(View& v, const Map& m, uint64_t mapId, int cx, int cy);
  // the visible chunks not baked yet (a teleport, a new game): painted in parallel right now (desktop), so a jump
  // costs one chunk's bake time instead of one per chunk; chunkTex then finds them ready
  void bakeVisibleNow(View& v, const Map& m, uint64_t mapId, int c0x, int c0y, int c1x, int c1y);
  std::vector<std::thread> workers_;   // more background bakers (desktop): worker_ plus these
  void bakeChunk(View& v, const BakeJob& j, Canvas& c);
  void bakeRows(View& v, const BakeJob& j, Canvas& c, int r0, int r1);
  void bakeFinish(const BakeJob& j, Canvas& c);
  void pumpBake(View& v, double budgetMs);
  BakeJob incrJob_{};
  Canvas incrCanvas_;
  int incrRow_ = 0;
  bool incrOn_ = false;
  // (Witness) wait (up to 5 s) until the bakers are idle (View::settleBakes)
  void settleBakes();
  static bool singleThread();   // no bake threads: the web build, or EMB_WEBSIM=1 on desktop
  // (M3c integration, the seat-of-power entry hitch: View::prepareInterior) the interior map id being prepared (0:
  // none), its map and the next tile whose pieces are painted
  uint64_t prepMapId_ = 0;
  const Map* prepMap_ = nullptr;
  size_t prepCursor_ = 0;
  // (M2, View::terrainFrame / travelArrive) while arriving_ is set, chunkTex never bakes a chunk inline (it returns an
  // empty texture: the screen is black) and drawWorld skips bakeVisibleNow, so no frame stalls
  bool arriving_ = false;
  struct Arrival { bool on = false; float t = 0; int frames = 0; double ms = 0, wall0 = 0; };
  Arrival arrival_;
};
