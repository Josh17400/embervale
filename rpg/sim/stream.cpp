// ChunkStreamer: prefetching chunks and region plans for the endless Active Window (see stream.h). SIM lane, M1.
#include "rpg/sim/stream.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <unordered_map>
#include <utility>
#include "rpg/world/source.h"
#ifndef __EMSCRIPTEN__
#include <condition_variable>
#include <mutex>
#include <system_error>
#include <thread>
#endif

namespace {
uint64_t packKey(int32_t x, int32_t y) { return ((uint64_t)(uint32_t)x << 32) | (uint32_t)y; }
double msSince(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}
}  // namespace

struct ChunkStreamer::Impl {
  uint64_t seed = 0;
  std::shared_ptr<ew::EndlessSource> mainSrc;
  // main-thread side: what is ready to be taken
  std::unordered_map<uint64_t, std::unique_ptr<ew::ChunkData>> ready;
  std::unordered_map<uint64_t, ew::RegionPlan> readyR;
  std::deque<Key> wishLocal;   // the pump's own list (no worker)
  Stats st;
  bool threaded = false;
#ifndef __EMSCRIPTEN__
  std::thread th;
  std::mutex mu;
  std::condition_variable cv;
  bool stop = false;
  std::deque<Key> wish;        // guarded by mu
  bool busy = false;           // guarded by mu
  Key busyKey;
  struct DoneC { Key k; std::unique_ptr<ew::ChunkData> c; double ms; };
  struct DoneR { Key k; ew::RegionPlan r; double ms; };
  std::vector<DoneC> doneC;    // guarded by mu
  std::vector<DoneR> doneR;    // guarded by mu

  void loop() {
    // the worker's own generator: never shared with the main thread (one instance is not thread-safe)
    ew::EndlessSource src(seed);
    for (;;) {
      Key k;
      {
        std::unique_lock<std::mutex> lk(mu);
        cv.wait(lk, [&] { return stop || !wish.empty(); });
        if (stop) return;
        k = wish.front();
        wish.pop_front();
        busy = true;
        busyKey = k;
      }
      auto t0 = std::chrono::steady_clock::now();
      if (k.region) {
        ew::RegionPlan r = src.region(k.x, k.y);
        double ms = msSince(t0);
        std::lock_guard<std::mutex> lk(mu);
        doneR.push_back(DoneR{k, std::move(r), ms});
        busy = false;
      } else {
        std::unique_ptr<ew::ChunkData> c(new ew::ChunkData());
        src.chunk(k.x, k.y, *c);
        double ms = msSince(t0);
        std::lock_guard<std::mutex> lk(mu);
        doneC.push_back(DoneC{k, std::move(c), ms});
        busy = false;
      }
    }
  }
#endif

  // move finished work into the ready maps (main thread)
  int collect() {
    int n = 0;
#ifndef __EMSCRIPTEN__
    if (!threaded) return 0;
    std::vector<DoneC> c;
    std::vector<DoneR> r;
    {
      std::lock_guard<std::mutex> lk(mu);
      c.swap(doneC);
      r.swap(doneR);
    }
    for (DoneC& d : c) {
      st.chunksMade++; st.workMs += d.ms; st.maxChunkMs = std::max(st.maxChunkMs, d.ms);
      ready[packKey(d.k.x, d.k.y)] = std::move(d.c);
      n++;
    }
    for (DoneR& d : r) {
      st.regionsMade++; st.workMs += d.ms;
      readyR[packKey(d.k.x, d.k.y)] = std::move(d.r);
    }
#endif
    return n;
  }
};

ChunkStreamer::ChunkStreamer(uint64_t seed, std::shared_ptr<ew::EndlessSource> mainSrc, bool threaded) : d_(new Impl()) {
  d_->seed = seed;
  d_->mainSrc = std::move(mainSrc);
#ifndef __EMSCRIPTEN__
  if (threaded) {
    try {
      d_->th = std::thread([this] { d_->loop(); });
      d_->threaded = true;
    } catch (const std::system_error&) {
      d_->threaded = false;   // no threads here: the main loop pumps instead
    }
  }
#else
  (void)threaded;
#endif
}

ChunkStreamer::~ChunkStreamer() {
#ifndef __EMSCRIPTEN__
  if (d_->threaded) {
    {
      std::lock_guard<std::mutex> lk(d_->mu);
      d_->stop = true;
      d_->wish.clear();
    }
    d_->cv.notify_all();
    if (d_->th.joinable()) d_->th.join();
  }
#endif
}

bool ChunkStreamer::threaded() const { return d_->threaded; }

void ChunkStreamer::want(const std::vector<Key>& keys) {
  d_->collect();
  std::deque<Key> w;
  for (const Key& k : keys) {
    if (k.region ? d_->readyR.count(packKey(k.x, k.y)) > 0 : d_->ready.count(packKey(k.x, k.y)) > 0) continue;
    w.push_back(k);
  }
#ifndef __EMSCRIPTEN__
  if (d_->threaded) {
    {
      std::lock_guard<std::mutex> lk(d_->mu);
      // skip what the worker is making right now or has made but not handed over yet
      auto pending = [&](const Key& k) {
        if (d_->busy && d_->busyKey.x == k.x && d_->busyKey.y == k.y && d_->busyKey.region == k.region) return true;
        if (k.region) { for (auto& r : d_->doneR) if (r.k.x == k.x && r.k.y == k.y) return true; }
        else for (auto& c : d_->doneC) if (c.k.x == k.x && c.k.y == k.y) return true;
        return false;
      };
      d_->wish.clear();
      for (const Key& k : w) if (!pending(k)) d_->wish.push_back(k);
      d_->st.wished = (int)d_->wish.size();
    }
    d_->cv.notify_one();
    return;
  }
#endif
  d_->wishLocal = std::move(w);
  d_->st.wished = (int)d_->wishLocal.size();
}

bool ChunkStreamer::takeChunk(int32_t cx, int32_t cy, ew::ChunkData& out) {
  d_->collect();
  auto it = d_->ready.find(packKey(cx, cy));
  if (it == d_->ready.end() || !it->second) return false;
  out = std::move(*it->second);
  d_->ready.erase(it);
  return true;
}

bool ChunkStreamer::hasChunk(int32_t cx, int32_t cy) {
  d_->collect();
  return d_->ready.count(packKey(cx, cy)) > 0;
}

const ew::RegionPlan* ChunkStreamer::region(int32_t rx, int32_t ry) {
  d_->collect();
  auto it = d_->readyR.find(packKey(rx, ry));
  return it == d_->readyR.end() ? nullptr : &it->second;
}

void ChunkStreamer::trim(int32_t cx0, int32_t cy0, int32_t cx1, int32_t cy1, size_t keep) {
  if (d_->ready.size() > keep) {
    for (auto it = d_->ready.begin(); it != d_->ready.end();) {
      int32_t x = (int32_t)(uint32_t)(it->first >> 32), y = (int32_t)(uint32_t)it->first;
      if (x < cx0 || y < cy0 || x >= cx1 || y >= cy1) it = d_->ready.erase(it);
      else ++it;
    }
  }
  // region plans: keep the ones around the kept chunk box (a region is 8 x 8 chunks)
  const size_t keepR = 48;
  if (d_->readyR.size() > keepR) {
    int32_t rx0 = ew::floorDiv(cx0, 8) - 1, ry0 = ew::floorDiv(cy0, 8) - 1, rx1 = ew::floorDiv(cx1, 8) + 2, ry1 = ew::floorDiv(cy1, 8) + 2;
    for (auto it = d_->readyR.begin(); it != d_->readyR.end();) {
      int32_t x = (int32_t)(uint32_t)(it->first >> 32), y = (int32_t)(uint32_t)it->first;
      if (x < rx0 || y < ry0 || x >= rx1 || y >= ry1) it = d_->readyR.erase(it);
      else ++it;
    }
  }
}

int ChunkStreamer::pump(double budgetMs) {
  if (d_->threaded) return d_->collect();
  if (!d_->mainSrc) return 0;
  auto t0 = std::chrono::steady_clock::now();
  int n = 0;
  while (!d_->wishLocal.empty() && msSince(t0) < budgetMs) {
    Key k = d_->wishLocal.front();
    d_->wishLocal.pop_front();
    auto c0 = std::chrono::steady_clock::now();
    if (k.region) {
      d_->mainSrc->region(k.x, k.y);   // warms the main source's own cache: World reads it from there
      d_->st.regionsMade++;
    } else {
      if (d_->ready.count(packKey(k.x, k.y))) continue;
      // a settlement under the chunk (a city takes tens of ms) is built a phase at a time over several frames, so
      // no single frame pays for a whole town; the chunk itself is made once that work is done
      const double left = budgetMs - msSince(t0);
      if (!d_->mainSrc->prepareChunk(k.x, k.y, left > 0.5 ? left : 0.5)) {
        d_->wishLocal.push_front(k);
        double ms = msSince(c0);
        d_->st.workMs += ms;
        d_->st.maxChunkMs = std::max(d_->st.maxChunkMs, ms);
        break;
      }
      std::unique_ptr<ew::ChunkData> c(new ew::ChunkData());
      d_->mainSrc->chunk(k.x, k.y, *c);
      d_->ready[packKey(k.x, k.y)] = std::move(c);
      d_->st.chunksMade++;
      n++;
    }
    double ms = msSince(c0);
    d_->st.workMs += ms;
    d_->st.maxChunkMs = std::max(d_->st.maxChunkMs, ms);
  }
  return n;
}

ChunkStreamer::Stats ChunkStreamer::stats() {
  d_->collect();
  Stats s = d_->st;
  s.readyChunks = (int)d_->ready.size();
  s.readyRegions = (int)d_->readyR.size();
  return s;
}
