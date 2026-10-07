// Map::deco and the interior surfaces (rpg/sim/deco.h): walls with thickness, floors, rugs, contact shadows and floor
// clutter, drawn after the terrain and before the y-sorted scene. M0 homes lane.
// decoLayers decides the pieces per tile; art::interiorPiece paints them; cachedTex keeps one texture per piece key
// (the keys depend on the tile position only within a room, so the cache stays bounded).
#include <algorithm>
#include <chrono>
#include <mutex>
#include <vector>

#include "rpg/sim/deco.h"
#include "rpg/view/view.h"

namespace {
Canvas paintPiece(uint64_t key) { return art::interiorPiece((uint32_t)key); }
constexpr int kChunkTiles = 32;   // terrain.cpp CH
}  // namespace

void View::drawDeco(const Map& m, Vec2 cam, int tx0, int ty0, int tx1, int ty1) {
  if (m.kind != MapKind::Interior && m.deco.empty()) return;
  static std::vector<DecoLayer> layers;
  static std::vector<int> tileOf;
  layers.clear();
  tileOf.clear();
  for (int ty = ty0; ty < ty1; ty++)
    for (int tx = tx0; tx < tx1; tx++) {
      if (m.kind != MapKind::Interior && !m.deco[(size_t)ty * m.w + tx]) continue;
      size_t n0 = layers.size();
      decoLayers(m, tx, ty, layers);
      for (size_t i = n0; i < layers.size(); i++) tileOf.push_back(ty * m.w + tx);
    }
  if (layers.empty()) return;
  Pix& P = *pix_;
  for (int pass = 0; pass < kDecoPasses; pass++)
    for (size_t i = 0; i < layers.size(); i++) {
      const DecoLayer& L = layers[i];
      if (L.pass != pass) continue;
      const Tex& t = cachedTex(0x01ull << 56 | L.key, paintPiece);
      int tx = tileOf[i] % m.w, ty = tileOf[i] / m.w;
      P.blit(t, tx * 16.0f + L.dx - cam.x, ty * 16.0f + L.dy - cam.y);
    }
}

// (M3c integration, the seat-of-power entry hitch: the entry frame spent 13-28 ms baking the interior's terrain chunk
// inline and 24-53 ms painting its deco pieces the first time they showed) While the sim holds the interior of the
// door ahead (Game::preparedInterior), its chunks are queued for the bakers under the map id it will have once
// entered (prefetch keeps those jobs and their results), and its deco and interior prop pieces are painted into
// cachedTex about 1.5 ms a frame. The cache keys are the ones drawDeco and drawWorld use, so the entry finds them.
void View::prepareInterior(Game& g) {
  int bi = -1;
  const Map* pm = g.preparedInterior(bi);
  if (!pm || bi < 0 || pm->w <= 0 || pm->h <= 0 || pm->kind != MapKind::Interior) { prepMapId_ = 0; prepMap_ = nullptr; return; }
  const Map& m = *pm;
  const uint64_t id = mapIdFor(g, 100000 + bi);   // Game::mapKey once inside, on the ground floor
  if (id != prepMapId_ || prepMap_ != pm) {
    prepMapId_ = id;
    prepMap_ = pm;
    prepCursor_ = 0;
    // its terrain chunks, keyed as chunkTex will key them inside (an interior's chunk frame: local, not endless)
    const int sOX = chunkOX_, sOY = chunkOY_;
    const bool sEnd = chunkEndless_;
    chunkOX_ = chunkOY_ = 0;
    chunkEndless_ = false;
    {
      std::lock_guard<std::mutex> lk(mu_);
      for (int cy = 0; cy * kChunkTiles < m.h; cy++)
        for (int cx = 0; cx * kChunkTiles < m.w; cx++) {
          const uint64_t k = chunkKeyFor(m, id, cx, cy);
          bool have = std::find(pending_.begin(), pending_.end(), k) != pending_.end();
          for (auto& ch : chunks_) if (ch.key == k) have = true;
          for (auto& d : done_) if (d.key == k) have = true;
          if (have) continue;
          pending_.push_back(k);
          jobs_.push_back(makeJob(m, id, cx, cy));
        }
    }
    chunkOX_ = sOX; chunkOY_ = sOY;
    chunkEndless_ = sEnd;
    cv_.notify_all();
  }
  // its pieces, a few tiles a frame within the budget
  const size_t n = (size_t)m.w * m.h;
  if (prepCursor_ >= n) return;
  const auto t0 = std::chrono::steady_clock::now();
  static std::vector<DecoLayer> layers;
  while (prepCursor_ < n) {
    const int tx = (int)(prepCursor_ % (size_t)m.w), ty = (int)(prepCursor_ / (size_t)m.w);
    prepCursor_++;
    layers.clear();
    decoLayers(m, tx, ty, layers);
    for (const DecoLayer& L : layers) cachedTex(0x01ull << 56 | L.key, paintPiece);
    const int pr = m.prop.empty() ? 0 : m.prop[(size_t)ty * m.w + tx];
    if (pr) {
      const art::Prop p = (art::Prop)(pr - 1);
      if (const uint32_t key = interiorPropKey(m, tx, ty, p)) {
        cachedTex(0x01ull << 56 | key, paintPiece);
        if (p == art::Prop::DoorH || p == art::Prop::DoorV) cachedTex(0x01ull << 56 | (key | (2u << 24)), paintPiece);   // shut
        if ((key & 255u) == (uint32_t)art::Piece::Culture && p == art::Prop::Hearth)
          for (uint32_t f = 1; f < 4; f++) cachedTex(0x01ull << 56 | (key | (f << 24)), paintPiece);   // its flicker frames
      }
    }
    if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() > 1.5) break;
  }
}
