// Flora gallery (M3c Wildlands, LAND lane owns): every Wildlands flora prop (rpg/art/art_flora.cpp, AcaciaTree ..
// Petals) in up to six of its variants as a temperate land grows it, then as a cold land (snow) and a hot one (dry)
// grows it, the classic trees beside them for scale and style, and a grove of each tree planted with a free tile
// between trunks (the forest rule) to judge how a wood of them reads.
//   flora_gallery <outDir> [eco]   writes flora_3x.png and flora_1x.png (eco: a biomes.h key to grow the variant rows
//                                  in, default meadow; the cold row uses snowfield, the hot row savanna)
#include <chrono>
#include "rpg/world/biomes.h"
#include "tools/preview/preview_util.h"

namespace {
using art::Prop;
// a sprite's frame 0
Canvas frame0(Prop p, int v, int eco) {
  const Canvas sheet = art::floraVariant(p, v, eco);
  Canvas k(art::propW(p), art::propH(p));
  for (int y = 0; y < k.h; y++)
    for (int x = 0; x < k.w; x++) k.set(x, y, sheet.get(x, y));
  return k;
}
// a soft ground shadow (as the renderer's shadowBig_ under a tree, a little down-right)
void shadow(Board& b, int cx, int by, int w) {
  for (int y = by - 3; y <= by + 3; y++)
    for (int x = cx - w / 2; x <= cx + w / 2 + 4; x++) {
      const float dx = (x - (cx + 2.0f)) / (w * 0.5f + 2), dy = (y - (float)by) / 3.0f;
      if (dx * dx + dy * dy > 1) continue;
      b.c.set(x, y, art::shade(b.c.get(x, y), 0.74f));
    }
}
}  // namespace

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : ".";
  const Eco warm = argc > 2 && ecoFromName(argv[2]) != Eco::COUNT ? ecoFromName(argv[2]) : Eco::Meadow;
  const int first = (int)Prop::AcaciaTree, last = (int)Prop::Petals;
  int colW = 0, rowH = 0;
  for (int p = first; p <= last; p++) { colW = std::max(colW, art::propW((Prop)p)); rowH = std::max(rowH, art::propH((Prop)p)); }
  const int perRow = 14;
  const int nProps = last - first + 1, blocks = (nProps + perRow - 1) / perRow;
  const int vrows = 6, rows = vrows + 2;   // variants, then cold, then hot
  const int nTrees = (int)Prop::JuniperTree - first + 1;
  const int groveW = 6 * 16 + 60, groveH = 5 * 16 + 80;
  const int W = std::max(perRow * (colW + 6) + 8, 4 * groveW + 8), gridH = blocks * (rows * (rowH + 4) + 14);
  const int H = gridH + 70 + ((nTrees + 3) / 4) * groveH + 10;
  Board b(W, H);
  for (int i = 0; i < nProps; i++) {
    const Prop p = (Prop)(first + i);
    const int bx = 4 + (i % perRow) * (colW + 6), by = 4 + (i / perRow) * (rows * (rowH + 4) + 14);
    const int nv = std::max(1, art::floraVariants(p));
    for (int r = 0; r < rows; r++) {
      const bool cold = r == vrows, hot = r == vrows + 1;
      if (!cold && !hot && r >= nv) continue;
      const Canvas k = frame0(p, cold || hot ? 1 : r, (int)(cold ? Eco::SnowField : hot ? Eco::Savanna : warm));
      const int x = bx + (colW - k.w) / 2, y = by + 10 + r * (rowH + 4) + (rowH - k.h);
      if (art::isWildlandsSolid(p)) shadow(b, x + k.w / 2, y + k.h - 3, std::min(24, k.w - 6));
      b.put(k, x, y);
    }
    char lab[8];
    std::snprintf(lab, sizeof lab, "%d", first + i);
    b.text(bx, by, lab);
  }
  // the classic trees for scale
  const Prop classic[] = {Prop::OakTree, Prop::PineTree, Prop::BirchTree, Prop::AutumnTree, Prop::WillowTree, Prop::PalmTree, Prop::Bush, Prop::Boulder, Prop::TallGrass, Prop::Fern};
  int x = 4;
  for (Prop p : classic) {
    const Canvas k = art::propSprite(p);
    if (art::isTreeProp(p)) shadow(b, x + k.w / 2, gridH + 60 - 3, 24);
    b.put(k, x, gridH + 60 - k.h);
    x += k.w + 6;
  }
  // groves: each tree planted on the tile lattice with a free tile between trunks (two rows offset), drawn in y order
  for (int t = 0; t < nTrees; t++) {
    const Prop p = (Prop)(first + t);
    const int gx = 4 + (t % 4) * groveW, gy = gridH + 70 + (t / 4) * groveH;
    const int nv = std::max(1, art::floraVariants(p));
    int vi = 0;
    for (int row = 0; row < 3; row++)
      for (int col = 0; col < 3; col++) {
        const int tx = col * 2 + (row & 1), ty = row * 2;   // trunks two tiles apart, rows offset
        const Canvas k = frame0(p, (vi++ * 5 + t) % nv, (int)warm);
        const int fx = gx + 30 + tx * 16 + 8, fy = gy + 70 + ty * 16 + 16;   // the tile's bottom-centre (16 px tiles)
        shadow(b, fx, fy - 3, 22);
        b.put(k, fx - k.w / 2, fy - k.h);
      }
  }
  // the paint cost (the view paints each prop, variant and land the first time it shows one, on the main thread)
  {
    double total = 0, worst = 0;
    int n = 0, worstP = first;
    for (int p = first; p <= last; p++)
      for (int v = 0; v < art::floraVariants((Prop)p); v++) {
        const auto t0 = std::chrono::steady_clock::now();
        const Canvas k = art::floraVariant((Prop)p, v, (int)warm);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        total += ms; n++;
        if (ms > worst) { worst = ms; worstP = p; }
        (void)k;
      }
    std::printf("flora paint: %d sprites, %.2f ms total, %.3f ms mean, worst %.3f ms (prop %d)\n", n, total, total / n, worst, worstP);
  }
  savePng(b.c, dir + "/flora_3x.png", kScale);
  savePng(b.c, dir + "/flora_1x.png", 1);
  return 0;
}
