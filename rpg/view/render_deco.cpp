// Map::deco and the interior surfaces (rpg/sim/deco.h): walls with thickness, floors, rugs, contact shadows and floor
// clutter, drawn after the terrain and before the y-sorted scene. M0 homes lane.
// decoLayers decides the pieces per tile; art::interiorPiece paints them; cachedTex keeps one texture per piece key
// (the keys depend on the tile position only within a room, so the cache stays bounded).
#include <vector>

#include "rpg/sim/deco.h"
#include "rpg/view/view.h"

namespace {
Canvas paintPiece(uint64_t key) { return art::interiorPiece((uint32_t)key); }
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
