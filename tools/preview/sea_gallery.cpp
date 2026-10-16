// Sea gallery (M10 ART lane): the ships of every culture at every heading, the hero aboard (the layer split), the sea
// props and the sea creatures, on a sea-coloured board, scaled up for review.
//   sea_gallery <outDir> [ships|culture <n>|fine|aboard|props|monsters|all]   (default all)
// writes sea_ships_<hull>.png, sea_culture_<n>.png, sea_fine.png, sea_aboard.png, sea_props.png, sea_monsters.png
#include "tools/preview/preview_util.h"

#include "rpg/art/art_sea.h"
#include "rpg/culture/culture.h"

namespace cult { namespace detail { void applyPriors(Culture& c, Archetype a, uint32_t seed, int favouredOre, int cross); } }

namespace {

const char* kCulName[] = {"CLASSIC", "FJORDFOLK", "HIGHLAND", "HEARTLAND", "IMPERIAL", "DUNE", "STEPPE",
                          "MARSH", "JADE", "RIVER", "SUNTEMPLE", "SYLVAN", "STARSPIRE", "PIRATE"};

// a sea board: deep water with soft swells and glints (roughly the game's open sea)
Canvas seaBoard(int w, int h) {
  Canvas c(w, h);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const float n = std::sin(x * 0.21f + std::sin(y * 0.13f) * 2.0f) * 0.5f + std::sin(y * 0.37f + x * 0.05f) * 0.5f;
      uint32_t col = rgba(34, 74, 128);
      if (n > 0.72f) col = rgba(52, 100, 156);
      else if (n < -0.75f) col = rgba(28, 62, 112);
      if ((hash2(x, y) % 997) == 0) col = rgba(150, 200, 230);
      c.set(x, y, col);
    }
  return c;
}
void put(Canvas& dst, const Canvas& s, int x, int y) {
  for (int j = 0; j < s.h; j++)
    for (int i = 0; i < s.w; i++) {
      const uint32_t p = s.get(i, j);
      const int a = (int)(p >> 24);
      if (!a) continue;
      if (a == 255) { dst.set(x + i, y + j, p); continue; }
      dst.set(x + i, y + j, art::mix(dst.get(x + i, y + j), p | 0xFF000000u, a / 255.0f));
    }
}
void label(Canvas& c, int x, int y, const std::string& s, uint32_t col = rgba(255, 250, 230)) {
  for (char ch : s) {
    const int gi = pv::font5x7::glyphIndex(ch);
    if (gi >= 0)
      for (int r = 0; r < 7; r++)
        for (int k = 0; k < 5; k++)
          if (pv::font5x7::kFont[gi].rows[r] & (0x10 >> k)) { c.set(x + k + 1, y + r + 1, rgba(16, 20, 34)); c.set(x + k, y + r, col); }
    x += 6;
  }
}

art::ShipStyle styleFor(int cul, bool furled, int fill = 2, int trim = 0) {
  art::ShipStyle s;
  if (cul >= 13) { s.culture = 0; s.extras = art::SX_PIRATE; }
  else s.culture = (uint8_t)cul;
  s.sailCut = art::shipSailCutFor(s.culture);
  s.furled = furled;
  s.fill = (uint8_t)fill;
  s.brace = (int8_t)trim;
  s.pennant = 1;
  return s;
}

void shipsSheet(const std::string& dir, int hull) {
  const int cw = art::shipCellW(hull), ch = art::shipCellH(hull);
  const int rows = 14, lw = 70;
  Canvas c = seaBoard(lw + cw * 8, rows * ch);
  for (int r = 0; r < rows; r++) {
    label(c, 2, r * ch + ch / 2, kCulName[r]);
    for (int d = 0; d < 8; d++) put(c, art::shipCell(hull, styleFor(r, false), d * 4, 0, art::SHIP_LAYER_ALL), lw + d * cw, r * ch);
  }
  savePng(c, dir + "/sea_ships_" + std::to_string(hull) + ".png", hull >= 2 ? 1 : 2);
}

void cultureSheet(const std::string& dir, int cul) {
  int H = 0;
  for (int h = 0; h < 3; h++) H += art::shipCellH(h) * 2;
  const int W = art::shipCellW(2) * 8;
  Canvas c = seaBoard(W, H + 10);
  int y = 0;
  label(c, 2, 2, kCulName[std::min(cul, 13)]);
  for (int h = 0; h < 3; h++) {
    const int cw = art::shipCellW(h), ch = art::shipCellH(h);
    for (int furled = 0; furled < 2; furled++) {
      for (int d = 0; d < 8; d++) put(c, art::shipCell(h, styleFor(cul, furled != 0), d * 4, 0, art::SHIP_LAYER_ALL), d * (W / 8) + (W / 8 - cw) / 2, y);
      y += ch;
    }
  }
  savePng(c, dir + "/sea_culture_" + std::to_string(cul) + ".png", 2);
}

void fineSheet(const std::string& dir) {
  const int cw = art::shipCellW(1), ch = art::shipCellH(1);
  Canvas c = seaBoard(cw * 16, ch * 2 + art::shipCellH(0) * 2);
  for (int d = 0; d < 32; d++) put(c, art::shipCell(1, styleFor(3, false, 2, (d % 3) - 1), d, d & 1, art::SHIP_LAYER_ALL), (d % 16) * cw, (d / 16) * ch);
  const int rw = art::shipCellW(0), rh = art::shipCellH(0);
  for (int d = 0; d < 32; d++) put(c, art::shipCell(0, styleFor(1, false), d, d & 1, art::SHIP_LAYER_ALL), (d % 16) * cw + (cw - rw) / 2, ch * 2 + (d / 16) * rh);
  savePng(c, dir + "/sea_fine.png", 2);
}

// the hero aboard: back layer, the hero on the deck, the front layer (as the view composes them)
void aboardSheet(const std::string& dir) {
  art::HumanLook look;
  const Canvas hero = art::humanSheet(look);
  int W = 0;
  for (int h = 0; h < 3; h++) W += art::shipCellW(h);
  const int ch = art::shipCellH(2);
  Canvas c = seaBoard(W * 2, ch * 4);
  for (int row = 0; row < 4; row++) {
    const int d = row * 8 + 4;   // SE, SW, NW, NE ... a spread of headings
    int x = 0;
    for (int pass = 0; pass < 2; pass++)
      for (int h = 0; h < 3; h++) {
        const int cw = art::shipCellW(h), chh = art::shipCellH(h);
        const art::ShipStyle st = styleFor(pass ? 4 : 3, false, 2, row % 2 ? 1 : -1);
        const int dd = pass ? (d + 2) % 32 : d;
        const int oy = row * ch + (ch - chh);
        put(c, art::shipCell(h, st, dd, 0, art::SHIP_LAYER_BACK), x, oy);
        // the hero's feet at the anchor, lifted onto the deck (row: 0 down, 1 up, 2 right)
        const int hr = (dd >= 4 && dd < 12) ? 0 : (dd >= 20 && dd < 28) ? 1 : 2;
        const bool flip = dd >= 12 && dd < 20;
        Canvas cell(art::HUMAN_W, art::HUMAN_H);
        for (int j = 0; j < art::HUMAN_H; j++)
          for (int i = 0; i < art::HUMAN_W; i++) cell.set(flip ? art::HUMAN_W - 1 - i : i, j, hero.get(i, hr * art::HUMAN_H + j));
        put(c, cell, x + art::shipAnchorX(h) - art::HUMAN_W / 2, oy + art::shipAnchorY(h) - art::shipDeckY(h) - art::HUMAN_H + 2);
        put(c, art::shipCell(h, st, dd, 0, art::SHIP_LAYER_FRONT), x, oy);
        x += cw;
      }
  }
  savePng(c, dir + "/sea_aboard.png", 2);
}

void propsSheet(const std::string& dir) {
  using art::Prop;
  const Prop list[] = {Prop::MooringPost, Prop::NetRack, Prop::FishBaskets, Prop::Buoy, Prop::Anchor,
                       Prop::CargoPile, Prop::Slipway, Prop::HarbourDesk, Prop::Lighthouse, Prop::Wreck};
  const int CW = 72, CH = 104;
  Canvas c(5 * CW, 2 * CH);
  // sand, a strip of shallow water under the buoy and the wreck (what they stand in)
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const int i = (y / CH) * 5 + x / CW;
      const bool wet = (i == 3 || i == 9) && (y % CH) > CH - 30;
      c.set(x, y, wet ? rgba(52, 110, 160) : ((hash2(x, y) % 9) ? rgba(214, 186, 128) : rgba(196, 166, 110)));
    }
  for (int i = 0; i < 10; i++) {
    const Canvas s = art::propSprite(list[i]);
    const int fw = art::propW(list[i]);
    Canvas f0(fw, s.h);
    for (int y = 0; y < s.h; y++) for (int x = 0; x < fw; x++) f0.set(x, y, s.get(x, y));
    put(c, f0, (i % 5) * CW + (CW - fw) / 2, (i / 5) * CH + CH - 8 - s.h);
  }
  savePng(c, dir + "/sea_props.png", 4);
}

void monstersSheet(const std::string& dir) {
  using art::Monster;
  const Monster list[] = {Monster::SeaSerpent, Monster::Kraken, Monster::Merrow};
  int W = 0, H = 0;
  for (Monster m : list) { W = std::max(W, art::monsterCellW(m) * art::MONSTER_FRAMES + 80); H += art::monsterCellH(m) + 12; }
  Canvas c = seaBoard(W, H);
  int y = 4;
  const char* names[] = {"SERPENT", "KRAKEN", "MERROW"};
  for (int i = 0; i < 3; i++) {
    const Canvas s = art::monsterSheet(list[i]);
    label(c, 2, y + 4, names[i]);
    for (int f = 0; f < art::MONSTER_FRAMES; f++) {
      Canvas cell(art::monsterCellW(list[i]), art::monsterCellH(list[i]));
      for (int j = 0; j < cell.h; j++)
        for (int k = 0; k < cell.w; k++) cell.set(k, j, s.get(f * cell.w + k, j));
      put(c, cell, 70 + f * (cell.w + 1), y);
    }
    y += art::monsterCellH(list[i]) + 12;
  }
  {   // the kraken's tentacle
    const Canvas s = art::seaTentacleSheet();
    Canvas c2 = seaBoard(c.w, c.h + art::SEA_TENTACLE_H + 12);
    put(c2, c, 0, 0);
    label(c2, 2, c.h + 4, "TENTACLE");
    for (int f = 0; f < 8; f++) {
      Canvas cell(art::SEA_TENTACLE_W, art::SEA_TENTACLE_H);
      for (int j = 0; j < cell.h; j++)
        for (int k = 0; k < cell.w; k++) cell.set(k, j, s.get(f * cell.w + k, j));
      put(c2, cell, 70 + f * (cell.w + 6), c.h + 4);
    }
    c = c2;
  }
  savePng(c, dir + "/sea_monsters.png", 3);
}

// the peoples beyond the Mist beside a mainland people of the same archetype and seed: their cloth, dress, banners and
// ships (the priors' hidden branch; the mainland one is the classic draw)
void hiddenSheet(const std::string& dir) {
  const int rows = 6, CW = 300, CH = 110;
  Canvas c = seaBoard(CW * 2, rows * CH);
  for (int r = 0; r < rows; r++) {
    for (int hid = 0; hid < 2; hid++) {
      const cult::Archetype a = (cult::Archetype)((r * 5) % (int)cult::Archetype::COUNT);
      cult::Culture C;
      C.seed = 1000u + (uint32_t)r * 77u;
      if (hid) { C.hidden = true; C.hiddenLand = (uint32_t)(r * 2 + 1); }
      cult::detail::applyPriors(C, a, C.seed, -1, hid ? (int)(((int)a + 3) % (int)cult::Archetype::COUNT) : -1);
      const int x0 = hid * CW, y0 = r * CH;
      label(c, x0 + 4, y0 + 4, std::string(hid ? "HIDDEN " : "MAINLAND ") + cult::archetypeName(a));
      for (int k = 0; k < 6; k++)   // the cloth
        for (int y = 0; y < 8; y++) for (int x = 0; x < 10; x++) c.set(x0 + 4 + k * 12 + x, y0 + 16 + y, C.dress.cloth[k] | 0xFF000000u);
      for (int y = 0; y < 8; y++) for (int x = 0; x < 10; x++) {   // trims: roof, accent, alt, metal
        c.set(x0 + 80 + x, y0 + 16 + y, C.arch.roofTint | 0xFF000000u); c.set(x0 + 92 + x, y0 + 16 + y, C.arch.accentTint | 0xFF000000u);
        c.set(x0 + 104 + x, y0 + 16 + y, C.arch.altTint | 0xFF000000u); c.set(x0 + 116 + x, y0 + 16 + y, C.arms.metal | 0xFF000000u);
      }
      for (int k = 0; k < 3; k++) {   // people in its dress
        art::HumanLook L;
        L.topColor = C.dress.cloth[k] | 0xFF000000u;
        L.bottomColor = C.dress.cloth[(k + 2) % 6] | 0xFF000000u;
        L.pattern = (uint8_t)C.dress.pattern;
        L.patternColor = C.dress.trim ? (C.dress.trim | 0xFF000000u) : 0;
        L.headwear = (uint8_t)C.dress.head[k % 3];
        L.cut = (uint8_t)((int)(k == 1 ? C.dress.cutF : C.dress.cutM) + 1);
        L.facePaint = k == 2 ? C.dress.facePaint : 0;
        L.jewellery = C.dress.jewellery > 128 ? 3 : 0;
        L.skin = cult::skinRamp((C.dress.skinLo + C.dress.skinHi) / 2) | 0xFF000000u;
        const Canvas h = art::humanSheet(L);
        Canvas cell(art::HUMAN_W, art::HUMAN_H);
        for (int y = 0; y < art::HUMAN_H; y++) for (int x = 0; x < art::HUMAN_W; x++) cell.set(x, y, h.get(x, y));
        put(c, cell, x0 + 6 + k * 20, y0 + 30);
      }
      art::ShipStyle st;   // its sloop, as the view styles it
      st.culture = (uint8_t)((int)C.archetype + 1);
      st.culture2 = C.hidden ? (uint8_t)((int)C.archetype2 + 1) : 0;
      st.sailCut = art::shipSailCutFor(C.hidden ? st.culture2 : st.culture);
      if (C.hidden && C.dress.cloth[1]) st.sail = art::mix(C.dress.cloth[1] | 0xFF000000u, rgba(236, 228, 212), 0.35f);
      if (C.hidden && C.dress.cloth[2]) st.sail2 = C.dress.cloth[2] | 0xFF000000u;
      if (C.arch.accentTint) st.trim = C.arch.accentTint | 0xFF000000u;
      st.pennantA = C.heraldry.field ? (C.heraldry.field | 0xFF000000u) : 0;
      put(c, art::shipCell(1, st, 4, 0, art::SHIP_LAYER_ALL), x0 + 70, y0 + 14);
      put(c, art::shipCell(0, st, 28, 0, art::SHIP_LAYER_ALL), x0 + 160, y0 + 50);
      char buf[64];
      std::snprintf(buf, sizeof buf, "SCALE %d", (int)C.music.scale);
      label(c, x0 + 200, y0 + 20, buf);
    }
  }
  savePng(c, dir + "/sea_hidden.png", 2);
}

// (fixer M10 r1) the moored (furled) and drowned (wrecked) looks of every hull at 8 headings: harbours and the Mist
void wreckSheet(const std::string& dir) {
  const int W = art::shipCellW(2) * 8;
  int H = 0;
  for (int h = 0; h < 3; h++) H += art::shipCellH(h) * 2;
  Canvas c = seaBoard(W, H);
  int y = 0;
  for (int h = 0; h < 3; h++) {
    const int cw = art::shipCellW(h), ch = art::shipCellH(h);
    for (int k = 0; k < 2; k++) {
      art::ShipStyle st = styleFor(k ? 0 : 3, k == 0);
      if (k) { st = art::ShipStyle(); st.extras = art::SX_WRECKED; st.sailCut = 0; }
      for (int d = 0; d < 8; d++) put(c, art::shipCell(h, st, d * 4, 0, art::SHIP_LAYER_ALL), d * (W / 8) + (W / 8 - cw) / 2, y);
      y += ch;
    }
  }
  savePng(c, dir + "/sea_wreck.png", 2);
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  const std::string what = argc > 2 ? argv[2] : "all";
  if (what == "ships" || what == "all") for (int h = 0; h < 3; h++) shipsSheet(dir, h);
  if (what == "culture") cultureSheet(dir, argc > 3 ? std::atoi(argv[3]) : 0);
  if (what == "fine" || what == "all") fineSheet(dir);
  if (what == "aboard" || what == "all") aboardSheet(dir);
  if (what == "props" || what == "all") propsSheet(dir);
  if (what == "monsters" || what == "all") monstersSheet(dir);
  if (what == "hidden" || what == "all") hiddenSheet(dir);
  if (what == "wreck" || what == "all") wreckSheet(dir);
  return 0;
}
