// EMBERVALE art: item icons. See rpg/art.h for the contract and rpg/art/art_internal.h for the shared helpers.
#include "rpg/art/art_internal.h"

namespace art {

// =====================================================================================================
// 7. item icons (16x16, outlined, top-left light). Weapons point up-right like a classic inventory.
// =====================================================================================================
namespace {

// diagonal blade from (x,y) going up-right for len pixels; width 1..3
void diagBlade(Canvas& c, int x, int y, int len, int width, const Ramp& m) {
  for (int i = 0; i < len; i++) {
    int px2 = x + i, py = y - i;
    bool tip = i == len - 1;
    c.set(px2, py, tip ? m[4] : m[3]);
    if (!tip) c.set(px2 + 1, py, m[1]);
    if (width >= 2 && !tip) c.set(px2, py - 1, m[4]);
    if (width >= 3 && i < len - 2) { c.set(px2 + 1, py + 1, m[0]); c.set(px2 + 2, py, m[1]); }
  }
}
void diagHandle(Canvas& c, int x, int y, int len, const Ramp& r) {
  for (int i = 0; i < len; i++) { c.set(x + i, y - i, r[(i & 1) ? 2 : 3]); c.set(x + i + 1, y - i, r[1]); }
}

void potion(Canvas& c, const Ramp& liquid) {
  // round flask: glass rim, coloured liquid, cork, glints
  ball(c, 8, 10.5f, 5.0f, 4.8f, liquid);
  for (int y = 5; y <= 7; y++) { c.set(7, y, kCrystal[4]); c.set(8, y, kCrystal[3]); }
  for (int y = 6; y <= 9; y++)
    for (int x = 3; x <= 13; x++)
      if (solid(c, x, y) && y < 8) c.set(x, y, withA(mix(kCrystal[3], liquid[2], 0.2f), 255));
  box(c, 6, 2, 9, 4, kWood[3]); hline(c, 6, 9, 2, kWood[4]); c.set(9, 3, kWood[1]); c.set(9, 4, kWood[1]);
  hline(c, 6, 9, 5, kCrystal[2]);
  c.set(5, 10, kWhite); c.set(5, 9, kWhite); c.set(6, 8, liquid[4]);
}

void paintIcon(Canvas& c, Icon ic, uint32_t tint) {
  const bool tinted = tint != 0;
  const Ramp M = tinted ? ramp(tint, 1.1f) : kIron;     // metal / main material
  switch (ic) {
    case Icon::Sword:
      diagBlade(c, 6, 9, 8, 2, M);
      line(c, 3, 8, 7, 12, kBrass[3]); c.set(4, 9, kBrass[4]); c.set(6, 11, kBrass[1]);   // guard
      diagHandle(c, 3, 12, 2, kLeather);
      c.set(2, 13, kBrass[3]); c.set(1, 14, kBrass[2]);
      break;
    case Icon::Greatsword:
      diagBlade(c, 5, 10, 10, 3, M);
      line(c, 1, 8, 7, 14, kBrass[3]); line(c, 2, 8, 7, 13, kBrass[4]); c.set(7, 14, kBrass[1]);
      diagHandle(c, 2, 13, 2, kLeather);
      c.set(1, 14, kBrass[3]); c.set(0, 15, kBrass[2]);
      break;
    case Icon::Dagger:
      diagBlade(c, 7, 8, 5, 2, M);
      line(c, 5, 8, 7, 10, kBrass[3]);
      diagHandle(c, 4, 11, 3, kLeather);
      c.set(3, 12, kBrass[3]);
      break;
    case Icon::Axe:
      diagHandle(c, 2, 14, 10, kWood);
      // crescent head on the upper end
      for (int y = 1; y <= 8; y++)
        for (int x = 6; x <= 14; x++) {
          float dx = x - 13.5f, dy = y - 1.0f;
          float d = std::sqrt(dx * dx + dy * dy);
          float d2 = std::sqrt((x - 15.5f) * (x - 15.5f) + (y + 1.5f) * (y + 1.5f));
          if (d < 7.2f && d2 > 4.2f && x + y > 12) c.set(x, y, M[d > 6.0f ? 4 : (d > 4.6f ? 3 : 2)]);
        }
      c.set(12, 4, M[1]); c.set(11, 4, M[1]);
      break;
    case Icon::Mace:
      diagHandle(c, 2, 14, 8, kWood);
      c.set(2, 14, kLeather[2]); c.set(3, 13, kLeather[3]);
      ball(c, 11.5f, 4.5f, 3.6f, 3.6f, M);
      for (int a = 0; a < 6; a++) {
        float ang = a * TAU / 6 + 0.3f;
        c.set((int)std::lround(11.5f + std::cos(ang) * 4.6f - 0.5f), (int)std::lround(4.5f + std::sin(ang) * 4.6f - 0.5f), M[4]);
      }
      break;
    case Icon::Bow: {
      for (int i = 0; i <= 12; i++) {
        float t = i / 12.0f;
        int x = (int)std::lround(2 + t * 11 - std::sin(t * PI) * 3.0f), y = (int)std::lround(13 - t * 11 - std::sin(t * PI) * 3.0f);
        c.set(x, y, kWood[t < 0.5f ? 3 : 2]); c.set(x + 1, y, kWood[1]);
      }
      line(c, 2, 13, 13, 2, kCloth[4]);
      c.set(7, 8, kLeather[3]); c.set(8, 7, kLeather[2]); c.set(8, 8, kLeather[1]);
      if (tinted) { c.set(2, 12, M[3]); c.set(12, 2, M[3]); }
      break;
    }
    case Icon::Staff:
      diagHandle(c, 1, 14, 10, kWood);
      ball(c, 12.5f, 3.5f, 2.8f, 2.8f, tinted ? M : kCrystal);
      c.set(12, 2, kWhite);
      c.set(10, 6, kGold[3]); c.set(11, 6, kGold[2]); c.set(10, 5, kGold[4]);
      break;
    case Icon::Arrows:
      for (int k = 0; k < 3; k++) {
        int ox = k * 3 - 3, oy = k * 1 - 1;
        line(c, 3 + ox + 2, 13 + oy, 12 + ox + 2, 4 + oy, kWood[3]);
        c.set(13 + ox + 2, 3 + oy, M[4]); c.set(12 + ox + 2, 3 + oy, M[2]); c.set(13 + ox + 2, 4 + oy, M[2]);
        c.set(3 + ox + 2, 12 + oy, kRed[3]); c.set(4 + ox + 2, 13 + oy, kRed[2]); c.set(3 + ox + 2, 14 + oy, kCloth[4]);
      }
      break;
    case Icon::Shield: {
      // heater shield: metal rim, painted field with a chevron
      for (int y = 1; y <= 14; y++)
        for (int x = 2; x <= 13; x++) {
          float dx = (x + 0.5f - 8) / 6.0f;
          float bottom = y > 8 ? (y - 8) / 6.5f : 0;
          if (std::fabs(dx) > 1.0f - bottom * bottom) continue;
          bool rim = std::fabs(dx) > 0.82f - bottom * bottom || y == 1 || (y > 12);
          int k = x < 8 ? 3 : 2;
          if (x > 11 || y > 11) k--;
          c.set(x, y, rim ? M[k + (x < 8 ? 1 : 0)] : kRed[k]);
        }
      for (int i = 0; i < 4; i++) { c.set(5 + i, 5 + i, kGold[3]); c.set(10 - i, 5 + i, kGold[2]); }
      c.set(4, 2, kWhite);
      break;
    }
    case Icon::Helmet:
      for (int y = 2; y <= 13; y++)
        for (int x = 2; x <= 13; x++) {
          float dx = (x + 0.5f - 8) / 5.6f, dy = (y + 0.5f - 8) / 6.0f;
          if (y < 8 ? (dx * dx + dy * dy > 1) : std::fabs(dx) > 1) continue;
          c.set(x, y, M[lightIndex(lightAt(dx * 0.85f, std::min(0.6f, dy) * 0.85f), x, y, 0.1f)]);
        }
      hline(c, 4, 11, 8, kInk); hline(c, 4, 11, 9, M[0]);   // visor slit
      vline(c, 7, 9, 13, M[3]); vline(c, 8, 9, 13, M[1]);   // nasal guard
      vline(c, 8, 1, 3, kRed[3]); c.set(9, 1, kRed[2]); c.set(9, 2, kRed[1]);
      break;
    case Icon::Armor:
      for (int y = 2; y <= 14; y++)
        for (int x = 1; x <= 14; x++) {
          bool shoulders = y <= 5 && (x <= 4 || x >= 11) && y >= 2;
          bool torso = x >= 4 && x <= 11 && y >= 3;
          bool neck = y < 5 && x >= 6 && x <= 9;
          if (!(shoulders || torso) || neck) continue;
          int k = x < 6 ? 3 : (x > 10 ? 1 : 2);
          if (shoulders && y == 2) k = 4;
          c.set(x, y, M[k]);
        }
      vline(c, 7, 6, 13, M[3]); vline(c, 8, 6, 13, M[1]);
      hline(c, 4, 11, 11, kLeather[1]); c.set(7, 11, kGold[3]); c.set(8, 11, kGold[2]);
      c.set(5, 6, M[4]);
      break;
    case Icon::Boots: {
      // a pair in profile, toes to the right: the far boot sits up-left in shadow; folded cuff, heel, dark sole
      const Ramp& B = tinted ? M : kLeather;
      for (int b = 0; b < 2; b++) {
        int ox = b ? 0 : -3, oy = b ? 0 : -2, bias = b ? 0 : -1;
        std::vector<Vec2> shape = {{5.0f + ox, 2.5f + oy}, {10.0f + ox, 2.5f + oy}, {10.0f + ox, 8.5f + oy}, {12.5f + ox, 9.5f + oy},
                                   {14.5f + ox, 11.0f + oy}, {14.5f + ox, 14.0f + oy}, {4.5f + ox, 14.0f + oy}, {4.5f + ox, 9.0f + oy}};
        Canvas part(ICON, ICON);
        poly(part, shape, B[2]);
        for (int y = 0; y < ICON; y++)
          for (int x = 0; x < ICON; x++) {
            if (!solid(part, x, y)) continue;
            int lx = x - ox, ly = y - oy, k = 2;
            if (lx <= 5) k = 3;
            if (lx >= 9 && ly < 9) k = 1;
            if (ly <= 3) k = lx <= 6 ? 4 : 3;              // the folded cuff catches the light
            if (ly == 4) k = 1;                             // ...and shades the shaft below it
            if (ly >= 10 && lx >= 11 && ly <= 11) k = 3;    // toe cap
            if (ly == 13) k = 0;                            // sole
            if (ly == 12 && lx >= 5 && lx <= 7) k = 1;      // heel
            if (tinted && (ly == 6 || ly == 8) && lx >= 5 && lx <= 9) k = std::max(0, k - 2);   // greave plates
            c.set(x, y, B[std::clamp(k + bias, 0, 4)]);
          }
      }
      break;
    }
    case Icon::Cloak: {
      // a hooded cloak hanging from its clasp: a shadowed opening and two folds; tint = the cloth
      const Ramp K = tinted ? ramp(tint) : ramp(rgba(120, 44, 40));
      poly(c, {{5.5f, 2.0f}, {10.5f, 2.0f}, {11.5f, 5.0f}, {14.5f, 14.5f}, {11.0f, 13.5f}, {8.0f, 15.0f}, {5.0f, 13.5f}, {1.5f, 14.5f}, {4.5f, 5.0f}}, K[2]);
      for (int y = 0; y < ICON; y++)
        for (int x = 0; x < ICON; x++) {
          if (!solid(c, x, y)) continue;
          int k = 2;
          if (x <= 4 || (x == 5 && y < 9)) k = 3;
          if (x >= 11) k = 1;
          if (y > 6 && (x == 6 || x == 10)) k = 1;   // folds
          if (y > 6 && (x == 5 || x == 9)) k = std::min(4, k + 1);
          if (y <= 3) k = x <= 7 ? 4 : 3;           // the hood's crown
          c.set(x, y, K[k]);
        }
      poly(c, {{7.0f, 8.0f}, {9.0f, 8.0f}, {10.0f, 14.0f}, {8.0f, 15.0f}, {6.0f, 14.0f}}, K[0]);   // the opening
      hline(c, 6, 10, 5, K[1]);                                                                   // hood rim
      c.set(7, 6, kGold[4]); c.set(8, 6, kGold[2]); c.set(7, 7, kGold[1]);                       // clasp
      break;
    }
    case Icon::Gloves: {
      // gauntlet: four fingers, thumb, knuckle plate, leather cuff
      const Ramp& G = M;
      for (int f = 0; f < 4; f++) {
        int x = 5 + f * 2, top = f == 0 || f == 3 ? 4 : 2;
        for (int y = top; y <= 7; y++) c.set(x, y, G[y == top ? 4 : (f < 2 ? 3 : 2)]);
        c.set(x, 5, G[1]);
      }
      for (int y = 7; y <= 11; y++) for (int x = 4; x <= 11; x++) c.set(x, y, G[x < 6 ? 3 : (x > 9 ? 1 : 2)]);
      hline(c, 4, 11, 7, G[4]);
      for (int k = 0; k < 3; k++) { c.set(3 - k / 2, 8 + k, G[3]); c.set(2, 7 + k, G[2]); }
      for (int y = 12; y <= 14; y++) for (int x = 3; x <= 12; x++) c.set(x, y, kLeather[y == 12 ? 4 : (x < 6 ? 3 : 2)]);
      break;
    }
    case Icon::Ring: {
      const Ramp& Rg = tinted ? M : kGold;
      for (int a = 0; a < 48; a++) {
        float ang = a / 48.0f * TAU;
        int x = (int)std::lround(8 + std::cos(ang) * 4.5f - 0.5f), y = (int)std::lround(9.5f + std::sin(ang) * 3.6f - 0.5f);
        int k = lightIndex(lightAt(std::cos(ang) * 0.8f, std::sin(ang) * 0.8f), x, y, 0);
        c.set(x, y, Rg[k]);
        c.set(x, y + 1, Rg[std::max(0, k - 1)]);
      }
      ball(c, 8, 4.5f, 2.4f, 2.2f, ramp(rgba(220, 50, 80)));
      c.set(7, 3, kWhite);
      break;
    }
    case Icon::Amulet: {
      const Ramp& Rg = tinted ? M : kGold;
      for (int i = 0; i <= 10; i++) {
        float t = i / 10.0f;
        int x = (int)std::lround(3 + t * 10), y = (int)std::lround(2 + std::sin(t * PI) * 5);
        c.set(x, y, Rg[(i & 1) ? 2 : 4]);
      }
      ball(c, 8, 11, 3.4f, 3.6f, Rg);
      ball(c, 8, 11, 2.0f, 2.2f, kCrystal);
      c.set(7, 10, kWhite);
      break;
    }
    case Icon::PotionRed: potion(c, tinted ? M : ramp5(rgba(96, 18, 40), rgba(156, 28, 44), rgba(214, 50, 52), rgba(244, 96, 80), rgba(255, 170, 140))); break;
    case Icon::PotionBlue: potion(c, tinted ? M : ramp5(rgba(24, 30, 96), rgba(36, 60, 160), rgba(56, 104, 214), rgba(100, 156, 244), rgba(180, 216, 255))); break;
    case Icon::PotionGreen: potion(c, tinted ? M : ramp5(rgba(20, 70, 48), rgba(34, 120, 56), rgba(64, 176, 70), rgba(120, 220, 96), rgba(200, 250, 170))); break;
    case Icon::Bread: {
      const Ramp Br = ramp5(rgba(110, 54, 34), rgba(166, 92, 44), rgba(212, 140, 64), rgba(236, 186, 104), rgba(250, 226, 160));
      ball(c, 8, 9, 6.5f, 4.2f, Br);
      for (int k = 0; k < 3; k++) { c.set(5 + k * 3, 7, Br[4]); c.set(6 + k * 3, 8, Br[1]); c.set(6 + k * 3, 6, Br[4]); }
      break;
    }
    case Icon::Meat: {
      const Ramp Mt = ramp5(rgba(90, 24, 34), rgba(146, 46, 42), rgba(192, 82, 60), rgba(222, 124, 88), rgba(244, 172, 136));
      ball(c, 6.5f, 7.5f, 5.0f, 4.6f, Mt);
      capsule(c, V(9, 10), V(13, 13), 1.2f, 1.0f, kBone);
      ball(c, 13.5f, 13, 1.6f, 1.4f, kBone); ball(c, 12.8f, 14.2f, 1.3f, 1.2f, kBone);
      c.set(4, 5, Mt[4]); c.set(5, 5, Mt[4]); c.set(7, 9, Mt[1]);
      break;
    }
    case Icon::Apple: {
      const Ramp Ap = tinted ? M : kRed;
      ball(c, 8, 9.5f, 5.2f, 4.9f, Ap);
      c.set(8, 5, Ap[0]); vline(c, 8, 2, 4, kWood[1]);
      c.set(9, 3, kLeaf[3]); c.set(10, 3, kLeaf[3]); c.set(11, 2, kLeaf[2]); c.set(10, 2, kLeaf[4]);
      c.set(5, 7, kWhite); c.set(5, 8, Ap[4]);
      break;
    }
    case Icon::Cheese: {
      const Ramp Ch = ramp5(rgba(150, 100, 30), rgba(206, 150, 40), rgba(240, 196, 70), rgba(252, 226, 116), rgba(255, 246, 180));
      poly(c, {{1.5f, 11.5f}, {13.5f, 5.0f}, {14.5f, 11.5f}, {14.5f, 14.0f}, {1.5f, 14.0f}}, Ch[2]);
      poly(c, {{1.5f, 11.5f}, {13.5f, 5.0f}, {14.5f, 11.5f}}, Ch[4]);
      hline(c, 2, 14, 12, Ch[3]);
      c.set(5, 13, Ch[0]); c.set(10, 13, Ch[0]); c.set(11, 13, Ch[1]); c.set(8, 10, Ch[1]); c.set(12, 9, Ch[1]);
      break;
    }
    case Icon::Gold: {
      const Ramp& G = tinted ? M : kGold;
      const float cs[6][2] = {{5, 12}, {10, 12}, {7.5f, 9.5f}, {12, 9}, {4, 9}, {8, 6.5f}};
      for (auto& p : cs) {
        ellipse(c, p[0], p[1], 3.2f, 1.8f, G[1]);
        ellipse(c, p[0], p[1] - 0.6f, 3.0f, 1.5f, G[3]);
        c.set((int)p[0] - 1, (int)p[1] - 1, G[4]);
      }
      break;
    }
    case Icon::Gem: {
      const Ramp& Gm = tinted ? M : ramp5(rgba(30, 50, 110), rgba(36, 100, 180), rgba(60, 160, 230), rgba(130, 214, 250), rgba(230, 252, 255));
      poly(c, {{8, 2}, {14, 7}, {8, 15}, {2, 7}}, Gm[2]);
      poly(c, {{8, 2}, {8, 7}, {2, 7}}, Gm[4]);
      poly(c, {{8, 2}, {14, 7}, {8, 7}}, Gm[3]);
      poly(c, {{2, 7}, {8, 7}, {8, 15}}, Gm[2]);
      poly(c, {{8, 7}, {14, 7}, {8, 15}}, Gm[1]);
      c.set(5, 5, kWhite);
      break;
    }
    case Icon::Key: {
      const Ramp& K = tinted ? M : kBrass;
      for (int a = 0; a < 24; a++) {
        float ang = a / 24.0f * TAU;
        c.set((int)std::lround(4.5f + std::cos(ang) * 2.6f), (int)std::lround(4.5f + std::sin(ang) * 2.6f), K[std::sin(ang) < 0 ? 4 : 2]);
      }
      line(c, 6, 6, 13, 13, K[3]); line(c, 7, 6, 13, 12, K[1]);
      c.set(11, 13, K[2]); c.set(10, 14, K[2]); c.set(13, 11, K[2]); c.set(14, 10, K[1]);
      break;
    }
    case Icon::Scroll:
      for (int y = 3; y <= 12; y++) for (int x = 3; x <= 12; x++) c.set(x, y, kCloth[x < 5 ? 4 : (x > 10 ? 2 : 3)]);
      for (int x = 2; x <= 13; x++) { c.set(x, 2, kCloth[4]); c.set(x, 3, kCloth[2]); c.set(x, 13, kCloth[2]); c.set(x, 12, kCloth[3]); }
      for (int y = 5; y <= 10; y += 2) hline(c, 5, 10 - (y % 4 == 1 ? 2 : 0), y, kCloth[0]);
      vline(c, 8, 1, 14, kRed[2]); c.set(9, 13, kRed[1]); c.set(7, 14, kRed[3]);
      break;
    case Icon::Book: {
      const Ramp Bk = tinted ? M : ramp5(rgba(56, 22, 34), rgba(90, 30, 40), rgba(130, 44, 48), rgba(170, 70, 60), rgba(206, 110, 86));
      for (int y = 2; y <= 13; y++) for (int x = 3; x <= 12; x++) c.set(x, y, Bk[x == 3 ? 1 : (x < 6 ? 3 : 2)]);
      for (int y = 3; y <= 13; y++) c.set(13, y, kCloth[(y & 1) ? 3 : 2]);
      hline(c, 4, 13, 14, kCloth[2]);
      vline(c, 4, 2, 13, Bk[0]);
      box(c, 7, 6, 10, 9, kGold[2]); c.set(7, 6, kGold[4]); c.set(8, 7, kGold[1]);
      break;
    }
    case Icon::Map:
      poly(c, {{1, 3}, {5, 2}, {10, 3}, {15, 2}, {15, 13}, {10, 14}, {5, 13}, {1, 14}}, kCloth[3]);
      poly(c, {{5, 2}, {10, 3}, {10, 14}, {5, 13}}, kCloth[2]);
      line(c, 3, 11, 6, 8, kRed[2]); c.set(8, 7, kRed[2]); c.set(9, 6, kRed[2]); c.set(10, 7, kRed[2]);
      c.set(12, 5, kRed[1]); c.set(13, 6, kRed[1]); c.set(12, 6, kRed[3]); c.set(13, 5, kRed[3]);
      c.set(3, 5, kLeaf[2]); c.set(4, 5, kLeaf[1]); c.set(3, 4, kLeaf[3]);
      break;
    case Icon::Pelt: {
      const Ramp Pl = ramp5(rgba(64, 40, 38), rgba(104, 68, 50), rgba(146, 102, 70), rgba(182, 140, 96), rgba(214, 180, 130));
      furBall(c, 8, 8, 5.5f, 5.0f, Pl, 0.5f, 3);
      for (int k : {-1, 1}) { capsule(c, V(8 + k * 4, 5), V(8 + k * 7, 2), 1.4f, 0.8f, Pl); capsule(c, V(8 + k * 4, 11), V(8 + k * 7, 14), 1.4f, 0.8f, Pl); }
      capsule(c, V(8, 12), V(9, 15), 1.0f, 0.5f, Pl, -1);
      break;
    }
    case Icon::Bone:
      capsule(c, V(4, 12), V(12, 4), 1.3f, 1.3f, kBone);
      ball(c, 3, 11.5f, 1.8f, 1.8f, kBone); ball(c, 4.5f, 13, 1.8f, 1.8f, kBone);
      ball(c, 11.5f, 3, 1.8f, 1.8f, kBone); ball(c, 13, 4.5f, 1.8f, 1.8f, kBone);
      break;
    case Icon::Ore:
      rock(c, 8, 9, 6.5f, 5.5f, kStone, 41, 5);
      for (int i = 0; i < 6; i++) {
        int x = 4 + (int)(hash3(i, 0, 9) % 8), y = 6 + (int)(hash3(i, 1, 9) % 6);
        if (solid(c, x, y)) { c.set(x, y, (tinted ? M : kGold)[4]); c.set(x + 1, y, (tinted ? M : kGold)[2]); }
      }
      break;
    case Icon::Herb:
      vline(c, 8, 6, 14, kLeaf[1]);
      for (int k = 0; k < 3; k++) {
        int y = 4 + k * 3;
        ellipse(c, 5.5f, y + 1.0f, 2.5f, 1.4f, (tinted ? M : kLeaf)[3 - (k & 1)]);
        ellipse(c, 10.5f, y + 2.0f, 2.5f, 1.4f, (tinted ? M : kLeaf)[2]);
      }
      c.set(8, 2, rgba(240, 220, 250)); c.set(7, 3, rgba(200, 150, 230)); c.set(9, 3, rgba(200, 150, 230));
      break;
    case Icon::Sigil: {
      const Ramp& Sg = tinted ? M : ramp5(rgba(40, 30, 90), rgba(70, 50, 150), rgba(110, 90, 210), rgba(160, 150, 240), rgba(230, 230, 255));
      ball(c, 8, 8, 6.5f, 6.5f, kStone);
      for (int a = 0; a < 5; a++) {   // glowing pentagram-like rune
        float a0 = -PI / 2 + a * TAU / 5, a1 = -PI / 2 + (a + 2) * TAU / 5;
        line(c, (int)std::lround(8 + std::cos(a0) * 4.5f - 0.5f), (int)std::lround(8 + std::sin(a0) * 4.5f - 0.5f),
             (int)std::lround(8 + std::cos(a1) * 4.5f - 0.5f), (int)std::lround(8 + std::sin(a1) * 4.5f - 0.5f), Sg[3]);
      }
      c.set(7, 7, Sg[4]); c.set(8, 8, Sg[4]);
      break;
    }
    case Icon::Letter:
      for (int y = 4; y <= 12; y++) for (int x = 1; x <= 14; x++) c.set(x, y, kCloth[x < 3 ? 4 : 3]);
      line(c, 1, 4, 8, 9, kCloth[1]); line(c, 14, 4, 8, 9, kCloth[1]);
      line(c, 1, 12, 6, 8, kCloth[2]); line(c, 14, 12, 10, 8, kCloth[2]);
      ball(c, 8, 9.5f, 1.9f, 1.8f, kRed); c.set(7, 9, kRed[4]);
      break;
    case Icon::Crown: {
      const Ramp& G = tinted ? M : kGold;
      for (int y = 8; y <= 13; y++) for (int x = 2; x <= 13; x++) c.set(x, y, G[x < 5 ? 4 : (x > 11 ? 2 : 3)]);
      for (int p = 0; p < 3; p++) {
        int px2 = 3 + p * 5;
        for (int y = 3; y < 8; y++) { int half = (y - 3) / 2; for (int x = px2 - half; x <= px2 + half; x++) c.set(x, y, G[x <= px2 ? 3 : 2]); }
        c.set(px2, 2, G[4]);
      }
      hline(c, 2, 13, 12, G[1]); hline(c, 2, 13, 13, G[0]);
      c.set(5, 10, rgba(220, 50, 70)); c.set(8, 10, rgba(80, 160, 240)); c.set(11, 10, rgba(80, 200, 110));
      c.set(4, 9, kWhite);
      break;
    }
    default: break;
  }
}

}  // namespace

Canvas itemIcon(Icon i, uint32_t tint) {
  Canvas c(ICON, ICON);
  paintIcon(c, i, tint);
  outline(c);
  return c;
}

}  // namespace art
