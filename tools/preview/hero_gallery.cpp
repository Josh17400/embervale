// Hero gallery (M0 hero lane): the visible-equipment looks and the character-creator options on the human rig.
//   hero_gallery <outDir>     writes bands.png (every armour band, full kit, all 3 facings and frames), slots.png
//                             (each slot alone per band, cloaks, shields, back items, weapons), creator.png (skins x
//                             hairs, builds, beards, eyes), icons.png (every icon x tier tints), plus 1x copies
//                             (*_1x.png) for judging at game scale.
#include <initializer_list>

#include "tools/preview/preview_util.h"

namespace {

using art::HumanLook;

// the player's tier tints (rpg/sim/items.cpp tierTint) by band: 1 leather and 2 iron share the iron tint
uint32_t bandTint(int b) {
  static const uint32_t c[8] = {0, rgba(150, 150, 158), rgba(150, 150, 158), rgba(200, 208, 220), rgba(222, 186, 92),
                                rgba(120, 214, 140), rgba(92, 70, 120), rgba(200, 52, 44)};
  return c[std::clamp(b, 0, 7)];
}
const char* bandName(int b) {
  static const char* n[8] = {"SHIRT", "LEATHER", "IRON", "STEEL", "GILDED", "JADE", "OBSIDIAN", "EMBER"};
  return n[std::clamp(b, 0, 7)];
}

HumanLook shirtOnly() {
  HumanLook L;
  L.skin = rgba(236, 188, 146);
  L.hairColor = rgba(120, 70, 36);
  L.topColor = rgba(70, 110, 150);
  L.bottomColor = rgba(78, 60, 44);
  L.tabardColor = rgba(170, 50, 40);
  L.weapon = 0;
  return L;
}

HumanLook fullKit(int b) {
  HumanLook L = shirtOnly();
  if (b <= 0) return L;
  L.helmStyle = (uint8_t)b; L.armorStyle = (uint8_t)b; L.gloves = (uint8_t)b; L.boots = (uint8_t)b;
  L.cloak = (uint8_t)(b % 3 + 1);
  static const uint32_t cloaks[8] = {0, rgba(60, 96, 62), rgba(128, 40, 40), rgba(52, 70, 120), rgba(48, 64, 140),
                                     rgba(58, 56, 64), rgba(110, 30, 40), rgba(40, 34, 40)};
  L.cloakColor = cloaks[b];
  L.shieldStyle = (uint8_t)((b - 1) % 4 + 1);
  L.trimColor = bandTint(b);
  L.weapon = 1;
  L.weaponColor = bandTint(b);
  L.backItem = 1;
  L.amulet = b >= 4;
  L.ring = b >= 2;
  return L;
}

Canvas cellOf(const Canvas& sheet, int row, int frame) {
  Canvas c(art::HUMAN_W, art::HUMAN_H);
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) c.set(x, y, sheet.get(frame * art::HUMAN_W + x, row * art::HUMAN_H + y));
  return c;
}
Canvas flipped(const Canvas& s) {
  Canvas d(s.w, s.h);
  for (int y = 0; y < s.h; y++)
    for (int x = 0; x < s.w; x++) d.set(s.w - 1 - x, y, s.get(x, y));
  return d;
}

// a soft ground shadow like the game's, then the figure
void putHero(Board& b, const Canvas& cell, int x, int y) {
  for (int j = -1; j <= 1; j++)
    for (int i = -5; i <= 5; i++) {
      if (i * i / 25.0f + j * j / 2.0f > 1.0f) continue;
      uint32_t p = b.c.get(x + 8 + i, y + 22 + j);
      b.c.set(x + 8 + i, y + 22 + j, art::shade(p, 0.72f));
    }
  b.put(cell, x, y);
}

struct Shot { int row, frame; bool flip = false; };
const Shot kAll[] = {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6}, {0, 7}, {1, 0}, {1, 1}, {1, 3}, {1, 5}, {1, 6},
                     {2, 0}, {2, 1}, {2, 2}, {2, 3}, {2, 4}, {2, 5}, {2, 6}, {2, 7}, {2, 0, true}};

void saveBoth(const Board& b, const std::string& dir, const std::string& name) {
  savePng(b.c, dir + "/" + name + ".png", 4);
  savePng(b.c, dir + "/" + name + "_1x.png", 1);
}

void bands(const std::string& dir) {
  const int n = (int)(sizeof kAll / sizeof kAll[0]);
  Board b(64 + n * 18, 12 + 8 * 28);
  for (int band = 0; band < 8; band++) {
    Canvas sheet = art::humanSheet(fullKit(band));
    int y = 10 + band * 28;
    b.text(2, y + 8, bandName(band));
    for (int i = 0; i < n; i++) {
      Canvas c = cellOf(sheet, kAll[i].row, kAll[i].frame);
      if (kAll[i].flip) c = flipped(c);
      putHero(b, c, 60 + i * 18, y);
    }
  }
  saveBoth(b, dir, "bands");
}

void slots(const std::string& dir) {
  Board b(64 + 22 * 18, 12 + 7 * 28 + 4 * 28 + 4 * 28 + 6 * 28 + 20);
  int y = 6;
  b.text(60, y, "HELM DUS   BODY DUS   GLOVES D S  BOOTS D S W");
  y += 10;
  for (int band = 1; band <= 7; band++, y += 28) {
    b.text(2, y + 8, bandName(band));
    int x = 60;
    auto row = [&](HumanLook L, std::initializer_list<Shot> shots) {
      Canvas sheet = art::humanSheet(L);
      for (const Shot& s : shots) { putHero(b, cellOf(sheet, s.row, s.frame), x, y); x += 18; }
      x += 6;
    };
    HumanLook h = shirtOnly(); h.helmStyle = (uint8_t)band;
    row(h, {{0, 0}, {1, 0}, {2, 0}});
    HumanLook a = shirtOnly(); a.armorStyle = (uint8_t)band;
    row(a, {{0, 0}, {1, 0}, {2, 0}});
    HumanLook g = shirtOnly(); g.gloves = (uint8_t)band;
    row(g, {{0, 0}, {2, 0}});
    HumanLook t = shirtOnly(); t.boots = (uint8_t)band;
    row(t, {{0, 0}, {2, 0}, {2, 1}});
  }
  // cloaks: 3 styles, idle and walking on every facing
  b.text(60, y, "CLOAKS");
  y += 10;
  static const uint32_t cc[4] = {rgba(110, 80, 56), rgba(60, 96, 62), rgba(52, 70, 120), rgba(150, 40, 36)};
  for (int st = 1; st <= 4; st++, y += 28) {
    HumanLook L = shirtOnly();
    L.cloak = (uint8_t)(st > 3 ? 1 : st);
    L.cloakColor = cc[st - 1];
    if (st == 4) { L.armorStyle = 3; L.helmStyle = 3; L.weapon = 1; }
    Canvas sheet = art::humanSheet(L);
    b.text(2, y + 8, st == 1 ? "TRAVEL" : st == 2 ? "MANTLE" : st == 3 ? "NOBLE" : "ON PLATE");
    int x = 60;
    for (const Shot& s : kAll) {
      Canvas c = cellOf(sheet, s.row, s.frame);
      if (s.flip) c = flipped(c);
      putHero(b, c, x, y);
      x += 18;
    }
  }
  // shields: 4 shapes with a sword
  b.text(60, y, "SHIELDS");
  y += 10;
  for (int st = 1; st <= 4; st++, y += 28) {
    HumanLook L = shirtOnly();
    L.armorStyle = 2; L.shieldStyle = (uint8_t)st; L.weapon = 1; L.trimColor = bandTint(st + 2);
    Canvas sheet = art::humanSheet(L);
    static const char* nm[4] = {"ROUND", "HEATER", "KITE", "TOWER"};
    b.text(2, y + 8, nm[st - 1]);
    int x = 60;
    for (const Shot& s : kAll) {
      Canvas c = cellOf(sheet, s.row, s.frame);
      if (s.flip) c = flipped(c);
      putHero(b, c, x, y);
      x += 18;
    }
  }
  // back items and weapons in hand
  b.text(60, y, "BACK ITEMS / WEAPONS");
  y += 10;
  for (int k = 0; k < 6; k++, y += 28) {
    HumanLook L = shirtOnly();
    L.armorStyle = 1; L.boots = 1;
    const char* nm = "";
    switch (k) {
      case 0: L.weapon = 1; L.backItem = 1; nm = "SWORD+BOW"; break;
      case 1: L.weapon = 2; L.backItem = 2; L.weaponColor = rgba(200, 208, 220); nm = "AXE+STAFF"; break;
      case 2: L.weapon = 6; L.backItem = 3; nm = "MACE+BOTH"; break;
      case 3: L.weapon = 3; nm = "BOW"; break;
      case 4: L.weapon = 4; L.weaponColor = rgba(255, 120, 40); L.backItem = 1; nm = "STAFF+BOW"; break;
      case 5: L.weapon = 5; L.cloak = 1; L.backItem = 1; L.cloakColor = rgba(60, 96, 62); nm = "DAGGER+CLOAK"; break;
    }
    Canvas sheet = art::humanSheet(L);
    b.text(2, y + 8, nm);
    int x = 60;
    for (const Shot& s : kAll) {
      Canvas c = cellOf(sheet, s.row, s.frame);
      if (s.flip) c = flipped(c);
      putHero(b, c, x, y);
      x += 18;
    }
  }
  saveBoth(b, dir, "slots");
}

const uint32_t kSkins[8] = {rgba(250, 220, 190), rgba(242, 204, 166), rgba(236, 188, 146), rgba(214, 160, 116),
                            rgba(190, 134, 94),  rgba(160, 104, 70),  rgba(124, 80, 56),   rgba(92, 60, 46)};
const uint32_t kHairC[8] = {rgba(40, 32, 36), rgba(70, 44, 30), rgba(120, 70, 36), rgba(150, 64, 36),
                            rgba(200, 104, 44), rgba(220, 180, 100), rgba(150, 148, 150), rgba(230, 226, 214)};

void creator(const std::string& dir) {
  const int nH = (int)art::Hair::COUNT;
  Board b(64 + 24 * 18, 16 + 8 * 28 + 3 * 28 + 80);
  int y = 6;
  b.text(60, y, "SKIN x HAIR (DOWN, UP, SIDE)");
  y += 10;
  for (int s = 0; s < 8; s++, y += 28) {
    b.text(2, y + 8, "SKIN " + std::to_string(s));
    for (int h = 0; h < nH; h++) {
      HumanLook L = shirtOnly();
      L.skin = kSkins[s]; L.hair = (art::Hair)h; L.hairColor = kHairC[(s + h) % 8];
      L.beard = (s + h) % 3 == 0;
      Canvas sheet = art::humanSheet(L);
      int x = 60 + h * 56;
      putHero(b, cellOf(sheet, 0, 0), x, y);
      putHero(b, cellOf(sheet, 1, 0), x + 17, y);
      putHero(b, cellOf(sheet, 2, 0), x + 34, y);
    }
  }
  b.text(60, y, "BUILD: AVERAGE SLIM BROAD   (D U S WALK)");
  y += 10;
  for (int bl = 0; bl < 3; bl++, y += 28) {
    b.text(2, y + 8, bl == 0 ? "AVERAGE" : bl == 1 ? "SLIM" : "BROAD");
    for (int v = 0; v < 2; v++) {
      HumanLook L = v ? fullKit(3) : shirtOnly();
      L.build = (uint8_t)bl;
      if (v) { L.cloak = 0; L.backItem = 0; L.helmStyle = 0; }
      Canvas sheet = art::humanSheet(L);
      int x = 60 + v * 200;
      for (const Shot& s : {Shot{0, 0}, Shot{0, 2}, Shot{1, 0}, Shot{2, 0}, Shot{2, 1}, Shot{2, 3}, Shot{0, 5}, Shot{0, 6}}) {
        putHero(b, cellOf(sheet, s.row, s.frame), x, y);
        x += 18;
      }
    }
  }
  b.text(60, y, "EYES");
  y += 10;
  static const uint32_t eyes[7] = {0, rgba(90, 54, 30), rgba(60, 110, 190), rgba(60, 140, 80), rgba(120, 130, 140), rgba(200, 140, 40), rgba(130, 80, 170)};
  for (int e = 0; e < 7; e++) {
    HumanLook L = shirtOnly();
    L.eyeColor = eyes[e];
    Canvas sheet = art::humanSheet(L);
    putHero(b, cellOf(sheet, 0, 0), 60 + e * 36, y);
    putHero(b, cellOf(sheet, 2, 0), 60 + e * 36 + 17, y);
  }
  saveBoth(b, dir, "creator");
}

void icons(const std::string& dir) {
  const int n = (int)art::Icon::COUNT;
  static const uint32_t tints[7] = {0, rgba(150, 150, 158), rgba(200, 208, 220), rgba(222, 186, 92), rgba(120, 214, 140), rgba(92, 70, 120), rgba(200, 52, 44)};
  Board b(8 + 7 * 20, 8 + n * 20);
  for (int i = 0; i < n; i++)
    for (int t = 0; t < 7; t++) b.put(art::itemIcon((art::Icon)i, tints[t]), 4 + t * 20, 4 + i * 20);
  saveBoth(b, dir, "icons");
}

}  // namespace

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : ".";
  bands(dir);
  slots(dir);
  creator(dir);
  icons(dir);
  return 0;
}
