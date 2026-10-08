// Lore gallery (M4 STORY lane): the six lore props (rpg/art/art_lore.cpp) where the game puts them: the notice board
// and the toppled statue on a town's grass and paving beside the props they live among (a signpost, a statue, a well,
// a cairn), and the inscription, mural, named grave and lost journal on a ruin's flagstones against its wall, each
// sprite bottom-centred on its tile as the view draws it.
//   lore_gallery <outDir>      writes lore.png (4x) and lore_1x.png (1x)
#include "tools/preview/preview_util.h"

namespace {

using art::Prop;

// a ruin's floor: worn flagstones with dark joints, a little moss in the cracks; the wall's face along the top
void ruinGround(Canvas& c, int x0, int y0, int w, int h, int wallRows) {
  for (int y = y0; y < y0 + h; y++)
    for (int x = x0; x < x0 + w; x++) {
      const int ly = y - y0;
      if (ly < wallRows) {
        // the wall face: courses of dressed stone, lit from the top-left, darker toward the floor
        const int course = ly / 6, cx = (x + (course & 1) * 5) % 10;
        uint32_t col = rgba(92, 86, 98);
        if (ly % 6 == 0 || cx == 0) col = rgba(54, 50, 64);
        else if (ly % 6 == 1) col = rgba(118, 112, 120);
        if (ly > wallRows - 4) col = art::shade(col, 0.8f);
        c.set(x, y, col);
        continue;
      }
      const int fy = ly - wallRows;
      const int row = fy / 8, cx = (x + (row & 1) * 6) % 12;
      uint32_t col = (hash2(x / 12, row) % 3 == 0) ? rgba(84, 80, 92) : rgba(96, 90, 100);
      if (fy % 8 == 0 || cx == 0) col = rgba(50, 46, 58);
      else if (fy % 8 == 1 || cx == 1) col = art::shade(col, 1.12f);
      if (hash2(x, y) % 41 == 0) col = rgba(70, 96, 64);
      if (fy < 3) col = art::shade(col, 0.7f);   // the wall's shadow at its foot
      c.set(x, y, col);
    }
}
// a town's paving under the board
void paving(Canvas& c, int x0, int y0, int w, int h) {
  for (int y = y0; y < y0 + h; y++)
    for (int x = x0; x < x0 + w; x++) {
      const int row = (y - y0) / 6, cx = (x + (row & 1) * 4) % 8;
      uint32_t col = (hash2(x / 8, row) % 3 == 0) ? rgba(176, 164, 140) : rgba(160, 150, 130);
      if ((y - y0) % 6 == 0 || cx == 0) col = rgba(118, 108, 96);
      c.set(x, y, col);
    }
}
// draw a prop as the view does: bottom-centred on its tile (tile = 16 px, the prop's footprint middle tile)
void placeProp(Board& b, Prop p, int tileX, int tileY) {
  Canvas sheet = art::propSprite(p);
  const int fw = art::propW(p);   // (an animated prop's sheet: its first frame)
  Canvas s(fw, sheet.h);
  for (int y = 0; y < sheet.h; y++) for (int x = 0; x < fw; x++) s.set(x, y, sheet.get(x, y));
  b.put(s, tileX * 16 + 8 - s.w / 2, tileY * 16 + 16 - s.h);
}

}  // namespace

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : ".";
  const int W = 16 * 22, H = 16 * 14;
  Board b(W, H);
  // ---- the town: paving round the board, the statue in the grass, neighbours for scale and style
  paving(b.c, 16 * 1, 16 * 1, 16 * 9, 16 * 5);
  b.text(4, 2, "TOWN SQUARE");
  placeProp(b, Prop::Well, 2, 3);
  placeProp(b, Prop::NoticeBoard, 5, 3);
  placeProp(b, Prop::Signpost, 8, 3);
  placeProp(b, Prop::Lamppost, 7, 5);
  b.text(16 * 11 + 4, 2, "BY A RUIN");
  placeProp(b, Prop::ToppledStatue, 13, 3);
  placeProp(b, Prop::Statue, 17, 3);
  placeProp(b, Prop::GraveCairn, 19, 4);
  placeProp(b, Prop::StandingStone, 16, 5);
  // ---- the ruin: flagstones, its wall along the top; the mural hangs on the wall over its tile
  ruinGround(b.c, 0, 16 * 7, W, 16 * 7, 14);
  b.text(4, 16 * 7 + 2, "RUIN");
  placeProp(b, Prop::Mural, 3, 8);
  placeProp(b, Prop::Inscription, 6, 9);
  placeProp(b, Prop::NamedGrave, 9, 10);
  placeProp(b, Prop::LostJournal, 12, 10);
  placeProp(b, Prop::ToppledStatue, 16, 11);
  placeProp(b, Prop::Urn, 19, 9);
  placeProp(b, Prop::Coffin, 20, 11);
  placeProp(b, Prop::Brazier, 1, 9);
  savePng(b.c, dir + "/lore.png", 4);
  savePng(b.c, dir + "/lore_1x.png", 1);
  // each sprite alone at 6x on a neutral ground (the canvas bounds framed), for the pixel detail
  const Prop all[6] = {Prop::NoticeBoard, Prop::Inscription, Prop::ToppledStatue, Prop::Mural, Prop::NamedGrave, Prop::LostJournal};
  int x = 4;
  Board solo(4 + 6 * 60, 50);
  for (Prop p : all) {
    Canvas s = art::propSprite(p);
    solo.frame(x - 1, 46 - s.h - 1, s.w + 2, s.h + 2);
    solo.put(s, x, 46 - s.h);
    x += std::max(s.w, 20) + 8;
  }
  savePng(solo.c, dir + "/lore_solo.png", 6);
  return 0;
}
