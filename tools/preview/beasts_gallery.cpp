// Beasts gallery (M6 Steel, BEASTS lane owns): the variant looks of every monster family (rpg/art/art_monsters.cpp
// monsterSheetLook) and the elite auras (rpg/art/art_fx.cpp auraSprite).
//   beasts_gallery <outDir>   writes, each at 1x and 4x:
//     beasts_variants: every family (rows) x {classic, each overlay alone, two combos, two tints} (frame 0)
//     beasts_frames:   every family's 8 frames wearing horns, a bone mask, glowing eyes, plates and spikes (the
//                      anchors follow the pose), then the golem's four materials, 8 frames each
//     beasts_auras:    the aura strip (6 frames) for every affix colour at two widths, over grass and over dark ground,
//                      an aura under a monster as the game draws it, and the glow sheets by night (body darkened, the
//                      glow added back)
#include "tools/preview/preview_util.h"
#include "rpg/sim/foes.h"

namespace {
using art::Monster;
using art::MonsterLook;
const char* kNames[] = {"WOLF", "BOAR", "BEAR", "SLIME", "SPIDER", "BAT", "SKELETON", "DRAUGR", "GOBLIN", "TROLL", "WRAITH",
                        "MUDCRAB", "ICE WOLF", "RIME SPIDER", "SANDWORM", "DRAGON", "SCORPION", "HYENA", "LURKER", "YETI",
                        "WISP", "EMBER HOUND", "BLIGHTSPAWN", "HARPY", "GOLEM"};
static_assert(sizeof(kNames) / sizeof(kNames[0]) == (size_t)Monster::COUNT, "a name for every monster");

Canvas cellOf(const Canvas& sheet, int cw, int chh, int f) {
  Canvas c(cw, chh);
  for (int y = 0; y < chh; y++)
    for (int x = 0; x < cw; x++) c.set(x, y, sheet.get(f * cw + x, y));
  return c;
}
void dark(Canvas& c, int x0, int y0, int w, int h, uint32_t a, uint32_t b) {
  for (int y = y0; y < y0 + h; y++)
    for (int x = x0; x < x0 + w; x++) c.set(x, y, hash2(x / 2, y / 2) % 5 == 0 ? b : a);
}
// alpha composite at a strength
void overOn(Canvas& dst, const Canvas& s, int x, int y, float k) {
  for (int j = 0; j < s.h; j++)
    for (int i = 0; i < s.w; i++) {
      const uint32_t p = s.get(i, j);
      const float a = (p >> 24) / 255.0f * k;
      if (a > 0) dst.set(x + i, y + j, art::mix(dst.get(x + i, y + j), p | 0xFF000000u, a));
    }
}
// additive composite (as the game's blend mode 1)
void addOn(Canvas& dst, const Canvas& s, int x, int y, float k) {
  for (int j = 0; j < s.h; j++)
    for (int i = 0; i < s.w; i++) {
      const uint32_t p = s.get(i, j);
      const float a = (p >> 24) / 255.0f * k;
      if (a <= 0) continue;
      const uint32_t d = dst.get(x + i, y + j);
      auto ch = [&](int sh) { return std::min(255, (int)(((d >> sh) & 255) + ((p >> sh) & 255) * a)); };
      dst.set(x + i, y + j, rgba(ch(0), ch(8), ch(16)));
    }
}
// the night: every pixel of a sprite darkened toward the night's blue (as drawLighting's multiply)
Canvas night(const Canvas& s) {
  Canvas c = s;
  for (uint32_t& p : c.px)
    if (p >> 24) p = rgba((int)((p & 255) * 0.16f), (int)(((p >> 8) & 255) * 0.19f), (int)(((p >> 16) & 255) * 0.34f), (int)(p >> 24));
  return c;
}
}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  const int N = (int)Monster::COUNT;
  // ---- variants
  struct Col { const char* label; uint16_t o; uint32_t tint; };
  const Col cols[] = {{"CLASSIC", 0, 0},
                      {"HORNS", art::MO_HORNS, 0},
                      {"SPIKES", art::MO_SPIKES, 0},
                      {"CRYSTAL", art::MO_CRYSTAL, 0},
                      {"MOSS", art::MO_MOSS, 0},
                      {"RIME", art::MO_RIME, 0},
                      {"EMBER", art::MO_EMBER, 0},
                      {"PLATES", art::MO_PLATES, 0},
                      {"EYES", art::MO_EYES, 0},
                      {"MASK", art::MO_BONEMASK, 0},
                      {"WARLORD", art::MO_HORNS | art::MO_PLATES | art::MO_EYES | art::MO_SPIKES, 0},
                      {"FROSTBONE", art::MO_RIME | art::MO_BONEMASK | art::MO_EYES | art::MO_CRYSTAL, rgba(150, 190, 230)},
                      {"ASHEN", art::MO_EMBER, rgba(84, 76, 82)},
                      {"CRYSTAL T", 0, art::GOLEM_CRYSTAL}};
  const int NC = (int)(sizeof(cols) / sizeof(cols[0]));
  int colW = 0;
  for (int m = 0; m < N; m++) colW = std::max(colW, art::monsterCellW((Monster)m));
  colW += 6;
  int H = 16;
  for (int m = 0; m < N; m++) H += art::monsterCellH((Monster)m) + 12;
  {
    Board b(84 + NC * colW, H);
    for (int k = 0; k < NC; k++) b.text(86 + k * colW, 4, cols[k].label);
    int y = 16;
    for (int m = 0; m < N; m++) {
      const Monster mm = (Monster)m;
      const int cw = art::monsterCellW(mm), chh = art::monsterCellH(mm);
      b.text(2, y + chh / 2, kNames[m]);
      for (int k = 0; k < NC; k++) {
        MonsterLook L;
        L.base = mm; L.overlays = cols[k].o; L.tint = cols[k].tint;
        const Canvas s = art::monsterSheetLook(L);
        b.put(cellOf(s, cw, chh, 0), 86 + k * colW + (colW - 6 - cw) / 2, y + 4);
      }
      y += chh + 12;
    }
    savePng(b.c, dir + "/beasts_variants_1x.png", 1);
    savePng(b.c, dir + "/beasts_variants_4x.png", 4);
  }
  // ---- frames (anchors through the poses) and the golem's materials
  {
    int W = 0, HH = 8;
    for (int m = 0; m < N; m++) {
      W = std::max(W, 84 + (art::monsterCellW((Monster)m) + 3) * art::MONSTER_FRAMES);
      HH += art::monsterCellH((Monster)m) + 6;
    }
    HH += 4 * (art::monsterCellH(Monster::Golem) + 6) + 10;
    Board b(W, HH);
    int y = 6;
    auto row = [&](const MonsterLook& L, const std::string& label) {
      const int cw = art::monsterCellW(L.base), chh = art::monsterCellH(L.base);
      b.text(2, y + chh / 2, label);
      const Canvas s = art::monsterSheetLook(L);
      for (int f = 0; f < art::MONSTER_FRAMES; f++) b.put(cellOf(s, cw, chh, f), 84 + f * (cw + 3), y);
      y += chh + 6;
    };
    for (int m = 0; m < N; m++) {
      MonsterLook L;
      L.base = (Monster)m;
      L.overlays = art::MO_HORNS | art::MO_BONEMASK | art::MO_EYES | art::MO_PLATES | art::MO_SPIKES;
      row(L, kNames[m]);
    }
    y += 10;
    const uint32_t mats[4] = {0, art::GOLEM_CRYSTAL, art::GOLEM_CLAY, art::GOLEM_IRON};
    const char* mn[4] = {"STONE", "CRYSTAL", "CLAY", "IRON"};
    for (int k = 0; k < 4; k++) {
      MonsterLook L;
      L.base = Monster::Golem; L.tint = mats[k];
      row(L, std::string("GOLEM ") + mn[k]);
    }
    savePng(b.c, dir + "/beasts_frames_1x.png", 1);
    savePng(b.c, dir + "/beasts_frames_4x.png", 4);
  }
  // ---- auras and the night's glow
  {
    const int NA = (int)foes::Affix::COUNT;
    const int W = 84 + art::AURA_FRAMES * 52 + 8 + 260;
    Board b(W, 30 + NA * 34 + 150);
    dark(b.c, 84 + art::AURA_FRAMES * 52 + 8, 0, 260, 30 + NA * 34, rgba(40, 38, 48), rgba(34, 32, 42));
    for (int i = 0; i < NA; i++) {
      const foes::AffixInfo& A = foes::affixInfo((foes::Affix)i);
      const int y = 20 + i * 34;
      b.text(2, y + 8, A.name);
      for (int w : {24, 48}) {
        const Canvas s = art::auraSprite(A.aura, w);
        const int ah = art::auraH(w);
        for (int f = 0; f < art::AURA_FRAMES; f++) {
          Canvas cell(w, ah);
          for (int yy = 0; yy < ah; yy++)
            for (int xx = 0; xx < w; xx++) cell.set(xx, yy, s.get(f * w + xx, yy));
          const int ax = w == 24 ? 84 + f * 30 : 84 + art::AURA_FRAMES * 52 + 12 + f * 62;
          if (w == 24 || f < 4) { overOn(b.c, cell, ax, y + 32 - ah, 0.45f); addOn(b.c, cell, ax, y + 32 - ah, 0.7f); }
        }
      }
      // under a wolf, as the game draws it
      const Canvas s = art::auraSprite(A.aura, 24);
      Canvas cell(24, art::auraH(24));
      for (int yy = 0; yy < cell.h; yy++)
        for (int xx = 0; xx < 24; xx++) cell.set(xx, yy, s.get(xx, yy));
      const int gx = 84 + 6 * 30 + 8, gy = y + 32;
      overOn(b.c, cell, gx, gy - cell.h + 6, 0.45f);
      addOn(b.c, cell, gx, gy - cell.h + 6, 0.7f);
      const Canvas wolf = art::monsterSheet(Monster::Wolf);
      b.put(cellOf(wolf, 32, 22, 0), gx - 4, gy - 22 + 2);
    }
    // by night: the body darkened, the glow sheet added back
    const int y0 = 30 + NA * 34;
    b.text(2, y0 + 4, "BY NIGHT");
    dark(b.c, 0, y0 + 14, W, 136, rgba(22, 24, 40), rgba(18, 20, 34));
    struct NL { Monster m; uint16_t o; uint32_t t; };
    const NL nl[] = {{Monster::Wolf, art::MO_EYES | art::MO_EMBER, rgba(84, 76, 82)}, {Monster::Troll, art::MO_CRYSTAL | art::MO_EYES, 0},
                     {Monster::Golem, 0, 0}, {Monster::Golem, 0, art::GOLEM_CRYSTAL}, {Monster::Golem, 0, art::GOLEM_IRON},
                     {Monster::Bear, art::MO_BONEMASK | art::MO_EYES, 0}, {Monster::Harpy, art::MO_EYES, 0}, {Monster::Yeti, art::MO_RIME | art::MO_EYES, 0}};
    int x = 6;
    for (const NL& n : nl) {
      MonsterLook L;
      L.base = n.m; L.overlays = n.o; L.tint = n.t;
      const int cw = art::monsterCellW(n.m), chh = art::monsterCellH(n.m);
      const Canvas s = art::monsterSheetLook(L);
      b.put(night(cellOf(s, cw, chh, 0)), x, y0 + 20);
      if (art::monsterLookGlows(L)) addOn(b.c, cellOf(art::monsterGlowLook(L), cw, chh, 0), x, y0 + 20, 1.0f);
      b.put(cellOf(s, cw, chh, 0), x, y0 + 80);
      x += cw + 8;
    }
    savePng(b.c, dir + "/beasts_auras_1x.png", 1);
    savePng(b.c, dir + "/beasts_auras_4x.png", 4);
  }
  return 0;
}
