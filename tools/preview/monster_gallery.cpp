// Monster gallery (M3c Wildlands, LIFE lane owns): every monster sheet (rpg/art/art_monsters.cpp), all 8 frames
// (0-3 move, 4 wind-up, 5 strike, 6 hurt, 7 dead), with the frame cells outlined, then a scale line-up of every
// monster's first frame standing on the grounds the Wildlands creatures live on (grass, sand, snow, marsh, ash).
//   monster_gallery <outDir> [first]   writes monsters_3x.png and monsters_1x.png (first: start at this monster index,
//                                      e.g. 16 for the Wildlands seven only)
#include "tools/preview/preview_util.h"

namespace {
using art::Monster;
const char* kNames[] = {"WOLF", "BOAR", "BEAR", "SLIME", "SPIDER", "BAT", "SKELETON", "DRAUGR", "GOBLIN", "TROLL", "WRAITH",
                        "MUDCRAB", "ICE WOLF", "RIME SPIDER", "SANDWORM", "DRAGON", "SCORPION", "HYENA", "LURKER", "YETI",
                        "WISP", "EMBER HOUND", "BLIGHTSPAWN"};
static_assert(sizeof(kNames) / sizeof(kNames[0]) == (size_t)Monster::COUNT, "a name for every monster");

// a ground swatch (hash speckle in three tones)
void ground(Canvas& c, int x0, int y0, int w, int h, uint32_t a, uint32_t b, uint32_t d) {
  for (int y = y0; y < y0 + h; y++)
    for (int x = x0; x < x0 + w; x++) {
      const uint32_t n = hash2(x / 2, y / 2) % 100;
      c.set(x, y, n < 20 ? b : n > 90 ? d : a);
    }
}
// a soft ellipse shadow under a creature (as the game draws it)
void shadowAt(Canvas& c, float cx, float cy, float rx, float ry) {
  for (int y = (int)(cy - ry); y <= (int)(cy + ry); y++)
    for (int x = (int)(cx - rx); x <= (int)(cx + rx); x++) {
      const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry, d = dx * dx + dy * dy;
      if (d < 1) c.set(x, y, art::mix(c.get(x, y), rgba(20, 16, 30), 0.43f * (1 - d * 0.5f)));
    }
}
}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  const int first = argc > 2 ? std::max(0, std::min((int)Monster::COUNT - 1, std::atoi(argv[2]))) : 0;
  int W = 0, H = 8;
  for (int m = first; m < (int)Monster::COUNT; m++) {
    W = std::max(W, art::monsterCellW((Monster)m) * art::MONSTER_FRAMES + 8 + art::MONSTER_FRAMES * 2);
    H += art::monsterCellH((Monster)m) + 14;
  }
  const int lineH = 70;
  W = std::max(W, 760);
  Board b(W, H + lineH * 2 + 8);
  int y = 4;
  for (int m = first; m < (int)Monster::COUNT; m++) {
    const Monster mm = (Monster)m;
    const int cw = art::monsterCellW(mm), chh = art::monsterCellH(mm);
    b.text(4, y, std::string(kNames[m]) + " " + std::to_string(cw) + "X" + std::to_string(chh));
    const Canvas s = art::monsterSheet(mm);
    for (int f = 0; f < art::MONSTER_FRAMES; f++) {
      Canvas cell(cw, chh);
      for (int yy = 0; yy < chh; yy++)
        for (int xx = 0; xx < cw; xx++) cell.set(xx, yy, s.get(f * cw + xx, yy));
      const int x = 4 + f * (cw + 2);
      b.frame(x - 1, y + 9, cw + 2, chh + 2);
      b.put(cell, x, y + 10);
    }
    y += chh + 14;
  }
  // the line-up: frame 0 of every monster on five grounds (two rows), ground line shared
  struct G { uint32_t a, b, d; } gs[5] = {{rgba(86, 140, 62), rgba(78, 130, 58), rgba(100, 154, 68)},
                                          {rgba(214, 180, 118), rgba(200, 164, 104), rgba(232, 204, 146)},
                                          {rgba(224, 232, 242), rgba(204, 216, 232), rgba(244, 248, 252)},
                                          {rgba(88, 104, 70), rgba(74, 90, 62), rgba(104, 120, 80)},
                                          {rgba(78, 72, 76), rgba(64, 58, 64), rgba(96, 88, 90)}};
  for (int row = 0; row < 2; row++) {
    const int gy = y + row * lineH;
    for (int k = 0; k < 5; k++) ground(b.c, k * W / 5, gy, W / 5 + 1, lineH, gs[k].a, gs[k].b, gs[k].d);
    int x = 6;
    for (int m = 0; m < (int)Monster::COUNT; m++) {
      if (m == (int)Monster::Dragon) continue;
      if ((m < 16) != (row == 0)) continue;
      const Monster mm = (Monster)m;
      const int cw = art::monsterCellW(mm), chh = art::monsterCellH(mm);
      const Canvas s = art::monsterSheet(mm);
      Canvas cell(cw, chh);
      for (int yy = 0; yy < chh; yy++)
        for (int xx = 0; xx < cw; xx++) cell.set(xx, yy, s.get(xx, yy));
      const int base = gy + lineH - 12;
      shadowAt(b.c, x + cw * 0.5f, (float)base, cw * 0.32f, 2.5f);
      const bool fly = mm == Monster::Bat || mm == Monster::Wraith || mm == Monster::Wisp;
      b.put(cell, x, base - chh + 2 - (fly ? 6 : 0));
      x += cw + (row == 1 ? 26 : 4);
    }
  }
  savePng(b.c, dir + "/monsters_3x.png", kScale);
  savePng(b.c, dir + "/monsters_1x.png", 1);
  return 0;
}
