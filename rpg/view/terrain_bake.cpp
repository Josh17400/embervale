// TerrainBaker (terrain_bake.h): the chunk cache's keys and snapshots, the job queue, the background bakers, the
// parallel bake of a jump's visible chunks, the web's incremental pump. The bake itself (bakeChunk, bakeRows,
// bakeFinish and the painters they call) stays in terrain.cpp.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>
#include "rpg/view/terrain_bake.h"
#include "rpg/view/view.h"

namespace {
constexpr int CH = TerrainBaker::kChunkTiles;   // tiles per chunk side
}  // namespace

// No bake threads: the web build, or a desktop run with EMB_WEBSIM=1 (the web's bake and arrival path measured on
// desktop: the incremental pump on the main thread, building sprites painted inline)
bool TerrainBaker::singleThread() {
#ifdef __EMSCRIPTEN__
  return true;
#else
  static const bool on = std::getenv("EMB_WEBSIM") != nullptr;
  return on;
#endif
}

// the cache key of local chunk (cx, cy) of map m: the map id, the GLOBAL chunk and whether the chunk is partial (an
// endless window's outer ring: its margin runs past the window, so it is baked again once the window moves on)
// Global chunk coordinates wrap every 2048 chunks (65536 tiles): pixel coordinates stay positive and small enough for
// the float noise to keep its fine detail however far the player walks (one seam line per 65536 tiles).
static int wrapChunk(int c) { return c & 2047; }

uint64_t TerrainBaker::chunkKeyFor(const View& v, const Map& m, uint64_t mapId, int cx, int cy) const {
  const int gcx = wrapChunk(cx + chunkOX_), gcy = wrapChunk(cy + chunkOY_);
  bool partial = false;
  if (chunkEndless_)
    partial = cx * CH - kBakeMargin < 0 || cy * CH - kBakeMargin < 0 || (cx + 1) * CH + kBakeMargin > m.w || (cy + 1) * CH + kBakeMargin > m.h;
  uint64_t k = mapId * 0x9E3779B97F4A7C15ull;
  k ^= ((uint64_t)(uint32_t)gcx * 0xC2B2AE3D27D4EB4Full) ^ ((uint64_t)(uint32_t)gcy * 0x165667B19E3779F9ull);
  k ^= k >> 29;
  return (k << 1) | (partial ? 1u : 0u);
}

void TerrainBaker::rebakeHomeHouses(View& v, const Game& g, const Map& m, uint64_t mapId) {
  if (m.kind != MapKind::Overworld) return;
  std::vector<HomeHouse> now;
  for (const Bldg& b : m.bldgs) {
    if (b.home != 1 && b.home != 3) continue;
    HomeHouse h;
    h.id = b.id; h.home = b.home; h.seed = b.seed;
    h.gx = b.r.x + (chunkEndless_ ? g.world.ox : 0); h.gy = b.r.y + (chunkEndless_ ? g.world.oy : 0);
    h.w = b.r.w; h.h = b.r.h;
    now.push_back(h);
  }
  auto same = [](const HomeHouse& a, const HomeHouse& b) {
    return a.id == b.id && a.home == b.home && a.seed == b.seed && a.gx == b.gx && a.gy == b.gy && a.w == b.w && a.h == b.h;
  };
  if (mapId != homeHousesMap_) {   // another map (or the first frame): its chunks are baked from what it holds now,
    homeHousesMap_ = mapId;        // except those baked before a house was stamped: rebake under every house once
    homeHouses_.clear();
  }
  std::vector<HomeHouse> changed;
  for (const HomeHouse& h : now) {
    bool seen = false;
    for (const HomeHouse& o : homeHouses_) seen = seen || same(h, o);
    if (!seen) changed.push_back(h);
  }
  for (const HomeHouse& o : homeHouses_) {
    bool still = false;
    for (const HomeHouse& h : now) still = still || same(h, o);
    if (!still) changed.push_back(o);
  }
  homeHouses_ = std::move(now);
  if (changed.empty()) return;
  // every chunk the house's footprint or its shadow (two tiles down-right, the sprite's pad) reaches, both its whole
  // and its partial (window-edge) key
  std::vector<uint64_t> keys;
  for (const HomeHouse& h : changed) {
    const int32_t ox = chunkEndless_ ? chunkOX_ * CH : 0, oy = chunkEndless_ ? chunkOY_ * CH : 0;
    const int cx0 = (int)std::floor((h.gx - 2 - ox) / (double)CH), cy0 = (int)std::floor((h.gy - 2 - oy) / (double)CH);
    const int cx1 = (int)std::floor((h.gx + h.w + 3 - ox) / (double)CH), cy1 = (int)std::floor((h.gy + h.h + 3 - oy) / (double)CH);
    for (int cy = cy0; cy <= cy1; cy++)
      for (int cx = cx0; cx <= cx1; cx++) {
        if (cx < 0 || cy < 0 || cx * CH >= m.w || cy * CH >= m.h) continue;
        const uint64_t k = chunkKeyFor(v, m, mapId, cx, cy) & ~1ull;
        keys.push_back(k);
        keys.push_back(k | 1ull);
      }
  }
  auto hit = [&](uint64_t k) { return std::find(keys.begin(), keys.end(), k) != keys.end(); };
  // a baked chunk stays on screen until its new bake is ready (chunkTex swaps it in: no frame waits on it); bakes
  // queued or finished from the old snapshot are dropped (prefetch / chunkTex queue them again)
  for (Chunk& ch : chunks_) if (hit(ch.key)) ch.stale = true;
  std::lock_guard<std::mutex> lk(mu_);
  for (size_t i = 0; i < jobs_.size();) if (hit(jobs_[i].key)) jobs_.erase(jobs_.begin() + (std::ptrdiff_t)i); else i++;
  for (size_t i = 0; i < done_.size();) if (hit(done_[i].key)) done_.erase(done_.begin() + (std::ptrdiff_t)i); else i++;
  pending_.erase(std::remove_if(pending_.begin(), pending_.end(), hit), pending_.end());
  if (incrOn_ && hit(incrJob_.key)) { incrOn_ = false; incrJob_.map.reset(); }
}

// a snapshot of local chunk (cx, cy) and its margin
TerrainBaker::BakeJob TerrainBaker::makeJob(const View& v, const Map& m, uint64_t mapId, int cx, int cy) const {
  BakeJob j;
  j.mapId = mapId;
  j.key = chunkKeyFor(v, m, mapId, cx, cy);
  j.gcx = wrapChunk(cx + chunkOX_); j.gcy = wrapChunk(cy + chunkOY_);
  const int S = CH + 2 * kBakeMargin;
  const int lx0 = cx * CH - kBakeMargin, ly0 = cy * CH - kBakeMargin;   // the snapshot's origin in m
  j.ox = j.gcx * CH - kBakeMargin; j.oy = j.gcy * CH - kBakeMargin;
  auto sub = std::make_shared<Map>();
  sub->kind = m.kind;
  sub->seed = m.seed;
  sub->w = S; sub->h = S;
  const size_t n = (size_t)S * S;
  sub->ground.assign(n, (uint8_t)Ground::Void);
  if (!m.wall.empty()) sub->wall.assign(n, 0);
  if (!m.biome.empty()) sub->biome.assign(n, 0);
  if (!m.height.empty()) sub->height.assign(n, 0);
  if (!m.blend.empty()) sub->blend.assign(n, 0);
  if (!m.eco.empty()) sub->eco.assign(n, 0);       // (M3c) the biome proper and the eco it blends toward
  if (!m.ecoNb.empty()) sub->ecoNb.assign(n, 0);
  if (!m.urb.empty()) sub->urb.assign(n, 0);       // (M7b) the urban fabric (kerbs, sidewalks, squares, parks)
  // M2 ecotones: a map whose generator writes no blend bytes gets them derived from its biomes (decided per map, so
  // every chunk of it agrees)
  j.ecoDerive = m.kind == MapKind::Overworld && !m.biome.empty() && std::none_of(m.blend.begin(), m.blend.end(), [](uint8_t v) { return v != 0; });
  for (int y = 0; y < S; y++) {
    const int sy = ly0 + y;
    if (sy < 0 || sy >= m.h) continue;
    const int xa = std::max(0, -lx0), xb = std::min(S, m.w - lx0);
    if (xa >= xb) continue;
    const size_t si = (size_t)sy * m.w + (size_t)(lx0 + xa), di = (size_t)y * S + (size_t)xa;
    const size_t len = (size_t)(xb - xa);
    std::copy_n(m.ground.begin() + si, len, sub->ground.begin() + di);
    if (!m.wall.empty()) std::copy_n(m.wall.begin() + si, len, sub->wall.begin() + di);
    if (!m.biome.empty()) std::copy_n(m.biome.begin() + si, len, sub->biome.begin() + di);
    if (!m.height.empty()) std::copy_n(m.height.begin() + si, len, sub->height.begin() + di);
    if (!m.blend.empty()) std::copy_n(m.blend.begin() + si, len, sub->blend.begin() + di);
    if (!m.eco.empty()) std::copy_n(m.eco.begin() + si, len, sub->eco.begin() + di);
    if (!m.ecoNb.empty()) std::copy_n(m.ecoNb.begin() + si, len, sub->ecoNb.begin() + di);
    if (!m.urb.empty()) std::copy_n(m.urb.begin() + si, len, sub->urb.begin() + di);
  }
  // the buildings whose footprint (or shadow, up to a tile and a half down-right) reaches the snapshot
  for (const Bldg& b : m.bldgs) {
    if (b.r.x + b.r.w + 2 < lx0 || b.r.y + b.r.h + 2 < ly0 || b.r.x > lx0 + S || b.r.y > ly0 + S) continue;
    Bldg c = b;
    c.r.x -= lx0; c.r.y -= ly0;
    sub->bldgs.push_back(c);
  }
  j.map = std::move(sub);
  return j;
}

void TerrainBaker::workerLoop(View& v) {
  for (;;) {
    BakeJob job;
    {
      std::unique_lock<std::mutex> lk(mu_);
      cv_.wait(lk, [&] { return quit_ || !jobs_.empty(); });
      if (quit_) return;
      job = jobs_.front();
      jobs_.pop_front();
    }
    BakeDone d;
    d.key = job.key; d.mapId = job.mapId;
    auto t0 = std::chrono::steady_clock::now();
    bakeChunk(v, job, d.c);
    d.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::lock_guard<std::mutex> lk(mu_);
    done_.push_back(std::move(d));
  }
}

void TerrainBaker::settleBakes() {
  if (singleThread()) return;
  for (int i = 0; i < 5000; i++) {
    {
      std::lock_guard<std::mutex> lk(mu_);
      bool busy = false;
      for (uint64_t k : pending_) {   // queued, or being painted (pending, not finished)
        bool finished = false;
        for (const BakeDone& d : done_) if (d.key == k) { finished = true; break; }
        if (!finished) { busy = true; break; }
      }
      if (!busy) return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void TerrainBaker::clearChunks(View& v) {
  for (auto& ch : chunks_) v.pix_->destroy(ch.tex);
  chunks_.clear();
  std::lock_guard<std::mutex> lk(mu_);
  jobs_.clear();
  pending_.clear();
}

void TerrainBaker::prefetch(View& v, const Map& m, uint64_t mapId, Vec2 cam) {
#ifndef __EMSCRIPTEN__
  if (!worker_.joinable() && !singleThread()) {
    worker_ = std::thread([this, &v] { workerLoop(v); });
    const unsigned hw = std::thread::hardware_concurrency();
    for (unsigned i = 1; i < std::min(3u, hw > 2 ? hw - 2 : 1u); i++) workers_.emplace_back([this, &v] { workerLoop(v); });
  }
#endif
  int c0x = (int)std::floor((cam.x - 256) / 512), c0y = (int)std::floor((cam.y - 256) / 512);
  int c1x = (int)std::floor((cam.x + v.viewW() + 256) / 512), c1y = (int)std::floor((cam.y + v.viewH() + 256) / 512);   // (P2a: the world view)
  std::lock_guard<std::mutex> lk(mu_);
  // keep only jobs for the current map (the global chunk key survives window shifts, so those jobs stay)
  for (size_t i = 0; i < jobs_.size();) {
    if (jobs_[i].mapId == mapId || (prepMapId_ && jobs_[i].mapId == prepMapId_)) { i++; continue; }   // (M3c: the door ahead's interior too)
    // forget it was pending too, or coming back to that map would wait forever and bake inline (a hitch)
    uint64_t k = jobs_[i].key;
    pending_.erase(std::remove(pending_.begin(), pending_.end(), k), pending_.end());
    jobs_.erase(jobs_.begin() + i);
  }
  // finished bakes nobody will collect (another map, or a chunk already baked inline) are ~1 MB each: drop them
  for (size_t i = 0; i < done_.size();) {
    bool stale = done_[i].mapId != mapId && !(prepMapId_ && done_[i].mapId == prepMapId_);
    for (auto& ch : chunks_) if (ch.key == done_[i].key && !ch.stale) stale = true;   // (a stale chunk awaits it)
    if (stale) {
      uint64_t k = done_[i].key;
      pending_.erase(std::remove(pending_.begin(), pending_.end(), k), pending_.end());
      done_.erase(done_.begin() + i);
    } else i++;
  }
  for (int cy = c0y; cy <= c1y; cy++)
    for (int cx = c0x; cx <= c1x; cx++) {
      if (cx < 0 || cy < 0 || cx * 32 >= m.w || cy * 32 >= m.h) continue;
      const uint64_t k = chunkKeyFor(v, m, mapId, cx, cy);
      bool have = false;
      for (auto& ch : chunks_) if (ch.key == k) { have = true; break; }
      if (have) continue;
      if (std::find(pending_.begin(), pending_.end(), k) != pending_.end()) continue;
      pending_.push_back(k);
      jobs_.push_back(makeJob(v, m, mapId, cx, cy));
    }
  cv_.notify_all();
}

void TerrainBaker::bakeVisibleNow(View& v, const Map& m, uint64_t mapId, int c0x, int c0y, int c1x, int c1y) {
#ifdef __EMSCRIPTEN__
  (void)m; (void)mapId; (void)c0x; (void)c0y; (void)c1x; (void)c1y;   // no threads: chunkTex bakes them one by one
#else
  if (singleThread()) return;
  std::vector<BakeJob> need;
  {
    std::lock_guard<std::mutex> lk(mu_);
    for (int cy = c0y; cy <= c1y; cy++)
      for (int cx = c0x; cx <= c1x; cx++) {
        if (cx < 0 || cy < 0 || cx * 32 >= m.w || cy * 32 >= m.h) continue;
        const uint64_t k = chunkKeyFor(v, m, mapId, cx, cy);
        bool have = false;
        for (auto& ch : chunks_) if (ch.key == k) { have = true; break; }
        for (auto& d : done_) if (d.key == k) { have = true; break; }
        // a job the worker already started is still pending without a queue entry: let chunkTex wait it out inline
        if (!have) need.push_back(makeJob(v, m, mapId, cx, cy));
      }
  }
  if (need.size() < 2) return;
  std::vector<BakeDone> out(need.size());
  std::vector<std::thread> th;
  auto t0 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < need.size(); i++)
    th.emplace_back([this, &v, &need, &out, i] { out[i].key = need[i].key; out[i].mapId = need[i].mapId; bakeChunk(v, need[i], out[i].c); });
  for (auto& t : th) t.join();
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  v.perf_.inlineBakes += (int)need.size();
  v.perf_.bakeMs += ms;
  v.perf_.worstBakeMs = std::max(v.perf_.worstBakeMs, ms);
  if (getenv("EMB_TIMING")) printf("%zu chunks baked in parallel in %.1f ms\n", need.size(), ms);
  std::lock_guard<std::mutex> lk(mu_);
  for (auto& d : out) {
    for (size_t i = 0; i < jobs_.size();) if (jobs_[i].key == d.key) jobs_.erase(jobs_.begin() + i); else i++;
    pending_.push_back(d.key);   // chunkTex takes it from done_ and clears this
    d.ms = 0;
    done_.push_back(std::move(d));
  }
#endif
}

Tex TerrainBaker::chunkTex(View& v, const Map& m, uint64_t mapId, int cx, int cy) {
  const uint64_t key = chunkKeyFor(v, m, mapId, cx, cy);
  for (auto& ch : chunks_)
    if (ch.key == key) {
      ch.used = v.t_;
      if (ch.stale && v.witness) {   // (Witness) the new bake now, not whenever a baker gets to it
        {
          std::lock_guard<std::mutex> lk(mu_);
          for (size_t i = 0; i < jobs_.size();) if (jobs_[i].key == key) jobs_.erase(jobs_.begin() + (std::ptrdiff_t)i); else i++;
          for (size_t i = 0; i < done_.size();) if (done_[i].key == key) done_.erase(done_.begin() + (std::ptrdiff_t)i); else i++;
          pending_.erase(std::remove(pending_.begin(), pending_.end(), key), pending_.end());
        }
        Canvas c;
        bakeChunk(v, makeJob(v, m, mapId, cx, cy), c);
        v.pix_->destroy(ch.tex);
        ch.tex = v.pix_->bake(c);
        ch.stale = false;
        return ch.tex;
      }
      if (ch.stale) {   // (M7) to be baked again (a player's house came or changed): swap the new bake in when ready
        std::unique_lock<std::mutex> lk(mu_);
        for (size_t i = 0; i < done_.size(); i++)
          if (done_[i].key == key) {
            BakeDone d = std::move(done_[i]);
            done_.erase(done_.begin() + (std::ptrdiff_t)i);
            pending_.erase(std::remove(pending_.begin(), pending_.end(), key), pending_.end());
            lk.unlock();
            v.pix_->destroy(ch.tex);
            ch.tex = v.pix_->bake(d.c);
            ch.stale = false;
            return ch.tex;
          }
        if (std::find(pending_.begin(), pending_.end(), key) == pending_.end()) {
          pending_.push_back(key);
          jobs_.push_back(makeJob(v, m, mapId, cx, cy));
          lk.unlock();
          cv_.notify_all();
        }
      }
      return ch.tex;
    }
  auto store = [&](Canvas& c) -> Tex {
    Chunk* slot = nullptr;
    // enough for the visible chunks of a wide phone screen and the ring prefetched around them
    const size_t cap = (size_t)std::max(24, 2 * (v.viewW() / 512 + 3) * (v.viewH() / 512 + 3));
    if (chunks_.size() < cap) { chunks_.push_back(Chunk()); slot = &chunks_.back(); }
    else {
      slot = &chunks_[0];
      for (auto& ch : chunks_) if (ch.used < slot->used) slot = &ch;
      v.pix_->destroy(slot->tex);
    }
    slot->tex = v.pix_->bake(c);
    slot->key = key; slot->used = v.t_; slot->stale = false;
    return slot->tex;
  };
  // finished in the background?
  {
    std::unique_lock<std::mutex> lk(mu_);
    for (size_t i = 0; i < done_.size(); i++)
      if (done_[i].key == key) {
        BakeDone d = std::move(done_[i]);
        done_.erase(done_.begin() + i);
        pending_.erase(std::remove(pending_.begin(), pending_.end(), key), pending_.end());
        lk.unlock();
        if (d.ms > 0) v.perf_.bakes++;   // (0: one of bakeVisibleNow's, counted there)
        v.perf_.bakeMs += d.ms;
        v.perf_.worstBakeMs = std::max(v.perf_.worstBakeMs, d.ms);
        return store(d.c);
      }
  }
  // needed right now (teleport / first frame): bake synchronously. (M2) Not while an arrival bakes behind the fade:
  // travelArrive waits for the bakers instead, so no frame stalls
  if (arriving_) return Tex{};
  Canvas c;
  auto t0 = std::chrono::steady_clock::now();
  bakeChunk(v, makeJob(v, m, mapId, cx, cy), c);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  v.perf_.inlineBakes++;
  v.perf_.bakeMs += ms;
  v.perf_.worstBakeMs = std::max(v.perf_.worstBakeMs, ms);
  if (getenv("EMB_TIMING")) printf("chunk %d,%d baked inline in %.1f ms\n", cx + chunkOX_, cy + chunkOY_, ms);
  {
    std::lock_guard<std::mutex> lk(mu_);
    for (size_t i = 0; i < jobs_.size();) if (jobs_[i].key == key) jobs_.erase(jobs_.begin() + i); else i++;
    pending_.erase(std::remove(pending_.begin(), pending_.end(), key), pending_.end());
  }
  return store(c);
}

// Without threads (the web build) the prefetch queue is baked a few rows at a time on the main thread,
// within a per-frame time budget, so walking into new terrain never stalls a frame.
void TerrainBaker::pumpBake(View& v, double budgetMs) {
  if (!singleThread()) return;
  auto t0 = std::chrono::steady_clock::now();
  for (;;) {
    if (!incrOn_) {
      std::lock_guard<std::mutex> lk(mu_);
      if (jobs_.empty()) return;
      incrJob_ = jobs_.front();
      jobs_.pop_front();
      incrCanvas_ = Canvas(CH * 16, CH * 16);
      incrRow_ = 0;
      incrOn_ = true;
    }
    // (M2 fixer round 3) four rows a step: a step of 16 rows of a rocky or built chunk ran 3-6 ms past the budget
    int r1 = std::min(incrCanvas_.h, incrRow_ + 4);
    bakeRows(v, incrJob_, incrCanvas_, incrRow_, r1);
    incrRow_ = r1;
    if (incrRow_ >= incrCanvas_.h) {
      bakeFinish(incrJob_, incrCanvas_);
      BakeDone d;
      d.key = incrJob_.key; d.mapId = incrJob_.mapId; d.c = std::move(incrCanvas_);
      std::lock_guard<std::mutex> lk(mu_);
      done_.push_back(std::move(d));
      incrOn_ = false;
      incrJob_.map.reset();
    }
    if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() > budgetMs) return;
  }
}
