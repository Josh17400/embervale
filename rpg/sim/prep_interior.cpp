// (M3c carry, the seat-of-power entry hitch) the interior of the door ahead, made before it is reached. Moved out of
// game.cpp in M4 phase A so the VIEW lane can time-slice it on the web (owner carry-over: the no-thread build still
// spiked 30-120 ms entering a seat of power, because genInterior ran whole in one step there). VIEW lane (with
// rpg/sim/interior_v4.cpp).
//
// M4 (VIEW lane): on the web (and on a desktop run with EMB_WEBSIM=1, which takes the same path so it can be measured)
// the interior is an InteriorJob (interior_v4.h) stepped a little every update, within kWebBudgetMs, from the moment
// the player comes within kPrepTiles of the door. The Map it makes is bit-identical to genInterior's. Natively a worker
// thread makes it from a copy of the building, as before.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/interior_v4.h"
#include "rpg/sim/stream.h"

namespace {
constexpr int kPrepTiles = 9;
constexpr double kWebBudgetMs = 2.5;      // per update, while the streamer is idle
constexpr double kWebBusyBudgetMs = 1.0;  // ... while it is streaming chunks
// the web path (no threads): the web build, or EMB_WEBSIM=1 on a desktop
bool webPath() {
#ifdef __EMSCRIPTEN__
  return true;
#else
  static const bool on = std::getenv("EMB_WEBSIM") != nullptr;
  return on;
#endif
}
// the web path's job in progress, per Game (Game's own members are frozen in game.h; one Game plays at a time)
struct WebPrep {
  std::unique_ptr<InteriorJob> job;
  uint64_t key = 0;
  int bldg = -1;
  uint64_t world = 0;
  double worstUnitMs = 0;
  double totalMs = 0, worstSliceMs = 0;   // (EMB_TIMING) the whole job and its longest slice in one update
  int slices = 0;
};
std::unordered_map<const Game*, WebPrep>& webPreps() {
  static std::unordered_map<const Game*, WebPrep> m;
  return m;
}
uint64_t worldTag(const Game& g) { return g.world.seed * 0x9E3779B97F4A7C15ull ^ (uint64_t)g.world.genVersion; }
}  // namespace

bool Game::takePreparedInterior(const Bldg& b, int floor, Map& out) {
  const uint64_t key = interiorKey(b, floor);
#ifndef __EMSCRIPTEN__
  if (prepJob_ && prepJob_->valid() && prepJobKey_ == key) {   // still being made: wait for it (shorter than starting over)
    prep_.map = prepJob_->get();
    prep_.key = key;
    prep_.ready = true;
    prepJob_.reset();
  }
#endif
  if (webPath()) {
    // the job for this door, part made: finish it now (the rest of its units: shorter than starting over)
    auto it = webPreps().find(this);
    if (it != webPreps().end() && it->second.job && it->second.key == key && it->second.world == worldTag(*this)) {
      it->second.job->step(1e9);
      prep_.map = std::move(it->second.job->map());
      prep_.key = key;
      prep_.ready = true;
      it->second.job.reset();
      it->second.key = 0;
    }
  }
  if (!prep_.ready || prep_.key != key) return false;
  out = std::move(prep_.map);
  prep_ = PrepInterior();
  prepBldg_ = -1;
  return true;
}

const Map* Game::preparedInterior(int& bldg) const {
  bldg = prepBldg_;
  return prep_.ready && prepBldg_ >= 0 && prepBldg_ < (int)world.over.bldgs.size() ? &prep_.map : nullptr;
}

void Game::prepInteriorTick(float dt) {
#ifndef __EMSCRIPTEN__
  // a finished job is collected whenever it lands
  if (prepJob_ && prepJob_->valid() && prepJob_->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    prep_.map = prepJob_->get();
    prep_.key = prepJobKey_;
    prep_.ready = true;
    prepBldg_ = prepJobBldg_;
    prepJob_.reset();
  }
#endif
  // the web path: the job in progress takes its slice of this update
  if (webPath()) {
    WebPrep& W = webPreps()[this];
    if (W.job && W.world != worldTag(*this)) { W.job.reset(); W.key = 0; }
    if (W.job && !inside) {
      const bool busy = world.streamer && !world.streamer->idle();
      const auto s0 = std::chrono::steady_clock::now();
      const bool fin = W.job->step(busy ? kWebBusyBudgetMs : kWebBudgetMs, &W.worstUnitMs);
      const double sl = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s0).count();
      W.totalMs += sl; W.worstSliceMs = std::max(W.worstSliceMs, sl); W.slices++;
      if (fin) {
        prep_ = PrepInterior();
        prep_.map = std::move(W.job->map());
        prep_.key = W.key;
        prep_.ready = true;
        prepBldg_ = W.bldg;
        W.job.reset();
        W.key = 0;
        if (std::getenv("EMB_TIMING"))
          std::printf("prep interior (web path): %.2f ms in %d slices, worst slice %.2f ms, worst unit %.2f ms\n", W.totalMs, W.slices, W.worstSliceMs, W.worstUnitMs);
      }
    }
  }
  prepT_ -= dt;
  if (prepT_ > 0 || inside) return;
  prepT_ = 0.15f;
  const Map& m = world.over;
  if (m.bldgAt.empty()) return;
  const int px = (int)std::floor(pl().p.x / TILE), py = (int)std::floor(pl().p.y / TILE);
  int best = -1, bestD = 1 << 30;
  for (int y = py - kPrepTiles; y <= py + kPrepTiles; y++)
    for (int x = px - kPrepTiles; x <= px + kPrepTiles; x++) {
      if (!m.in(x, y)) continue;
      const int bi = m.bldgAt[(size_t)y * m.w + x];
      if (bi < 0 || bi == best) continue;
      const Bldg& b = m.bldgs[(size_t)bi];
      // the door, or the nearest way in of an open front (its whole front row is in reach)
      const int dx = std::max(0, std::max(b.r.x - px, px - (b.r.x + b.r.w - 1))), dy = b.doorY() + 1 - py;
      const int d = dx * dx + dy * dy * 2;
      if (d < bestD) { bestD = d; best = bi; }
    }
  if (best < 0 || bestD > kPrepTiles * kPrepTiles * 2) return;
  const Bldg& b = m.bldgs[(size_t)best];
  const uint64_t key = interiorKey(b, 0);
  if (prep_.key == key && prep_.ready) { prepBldg_ = best; return; }
  if (webPath()) {
    WebPrep& W = webPreps()[this];
    if (W.job && W.key == key) return;   // being made
    // a new door: the old job (another door the player turned from) gives way
    W.job.reset(new InteriorJob(b, b.seed, 0));
    W.key = key;
    W.bldg = best;
    W.world = worldTag(*this);
    W.worstUnitMs = 0; W.totalMs = 0; W.worstSliceMs = 0; W.slices = 0;
    // EMB_PREP_ONESHOT=1: the M3c behaviour (the whole interior in one step), for before / after measurements
    static const bool oneShot = std::getenv("EMB_PREP_ONESHOT") != nullptr;
    if (oneShot) {
      const auto s0 = std::chrono::steady_clock::now();
      W.job->step(1e9, &W.worstUnitMs);
      W.totalMs = W.worstSliceMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s0).count();
      W.slices = 1;
    }
    return;
  }
#ifndef __EMSCRIPTEN__
  if (prepJob_ && prepJob_->valid()) return;   // one at a time (the one being made lands first)
  prepJobKey_ = key;
  prepJobBldg_ = best;
  const Bldg copy = b;
  prepJob_ = std::make_shared<std::future<Map>>(std::async(std::launch::async, [copy] {
    Map out;
    genInterior(out, copy, copy.seed, 0);
    return out;
  }));
#endif
}
