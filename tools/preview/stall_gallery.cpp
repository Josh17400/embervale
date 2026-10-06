// Stall gallery (M2 fixer round 3): every market stall form in all four facings (S, N, E, W), open and closed, on a
// cobbled-looking board with their shadows, the way the view composes them (shadow, then the model at stallOrigin
// from the prop tile). Side profiles (E / W) are the ones to judge here.
//   stall_gallery <outDir>      writes stalls.png (4x)
#include "tools/preview/preview_util.h"

#include "rpg/art/art_props.h"

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  const int cellW = 96, cellH = 96;
  const int trades[] = {0, 1, 2, 4, 6};
  const int cols = 4 * 2, rows = art::kStallForms * 5;
  Board b(cols * cellW + 8, rows * cellH + 8);
  for (int f = 0; f < art::kStallForms; f++)
    for (int ti = 0; ti < 5; ti++) {
      const int t = trades[ti];
      for (int closed = 0; closed < 2; closed++)
        for (int fc = 0; fc < 4; fc++) {
          const int col = closed * 4 + fc, row = f * 5 + ti;
          const int px = 4 + col * cellW + 40, py = 4 + row * cellH + 56;   // the prop tile's top-left
          // the counter tiles in a lighter tone (the stall's footprint)
          for (int i = 0; i < 3; i++) {
            int dx = 0, dy = 0;
            art::stallCounterTile(fc, i, dx, dy);
            for (int y = 0; y < 16; y++)
              for (int x = 0; x < 16; x++)
                if (x == 0 || y == 0) b.c.set(px + dx * 16 + x, py + dy * 16 + y, rgba(120, 160, 90));
          }
          int ox = 0, oy = 0;
          art::stallOrigin(fc, ox, oy);
          Canvas sh = art::marketStallShadow(f, fc);
          b.put(sh, px + art::kVendorShadowX, py + art::kVendorShadowY);
          Canvas s = art::marketStallFacing(t, t + f * 3 + fc, f, closed != 0, fc);
          b.put(s, px + ox, py + oy);
          static const char* nm[] = {"S", "N", "E", "W"};
          b.text(4 + col * cellW + 2, 4 + row * cellH + 2, std::string(nm[fc]) + (closed ? " SHUT" : ""));
        }
    }
  savePng(b.c, dir + "/stalls.png", 2);
  return 0;
}
