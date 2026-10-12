// Loot gallery (M8b "Treasure", VIEW lane; VISION_PLAN 15.27): the rarity ladder as the screen draws it, through the
// same painter templates the game uses (rpg/view/loot_paint.h) on a software canvas.
//   loot_gallery <outDir>   writes loot_sheet.png (4x) and loot_sheet_1x.png:
//     - the six rarities x the eleven kinds: the slot frame (the doll's 28 px box, the icon at 24) and the list cell
//       (the pack's 12 px icon cell) with the rarity's name, at three moments of the Mythic flame
//     - the item card: a Mythic sword against a worn Rare (the compare), a Common iron sword against it (a downgrade), a
//       Legendary amulet in an empty slot, at the 480 width and the phone shop's
//     - the loot on the ground: one of each rarity by day and at night (the light map), at three moments
//   loot_gallery --check    every rarity's frame drawn, the ground presence growing with the rarity (Uncommon < Rare <
//                           Epic < Legendary < Mythic, by the light it adds), Mythic's frame and beam animated (two
//                           moments differ), the others' frames still, deterministic between two runs; non-zero on failure
#include <cstring>
#include <initializer_list>

#include "rpg/culture/culture.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/gear_look.h"
#include "rpg/view/loot_paint.h"
#include "tools/preview/preview_util.h"

namespace {

uint32_t toU(Color c) {
  auto q = [](float v) { return (int)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f); };
  return rgba(q(c.r), q(c.g), q(c.b), 255);
}
Color fromU(uint32_t u) { return Color((u & 255) / 255.0f, ((u >> 8) & 255) / 255.0f, ((u >> 16) & 255) / 255.0f, 1); }

// the software painter for loot_paint.h: alpha blending and additive fills on a Canvas, the same beam / pool profiles
struct CanvasPainter {
  Canvas& c;
  void blend(int x, int y, Color k, bool add) {
    if (x < 0 || y < 0 || x >= c.w || y >= c.h || k.a <= 0) return;
    const Color d = fromU(c.get(x, y));
    const float a = std::min(1.0f, k.a);
    Color o = add ? Color(d.r + k.r * a, d.g + k.g * a, d.b + k.b * a) : Color(d.r + (k.r - d.r) * a, d.g + (k.g - d.g) * a, d.b + (k.b - d.b) * a);
    c.set(x, y, toU(o));
  }
  void fill(float x, float y, float w, float h, Color k, bool add) {
    const int x0 = (int)std::floor(x), y0 = (int)std::floor(y), x1 = (int)std::floor(x + w), y1 = (int)std::floor(y + h);
    for (int yy = y0; yy < y1; yy++)
      for (int xx = x0; xx < x1; xx++) blend(xx, yy, k, add);
  }
  void rect(float x, float y, float w, float h, Color k) { fill(x, y, w, h, k, false); }
  void add(float x, float y, float w, float h, Color k) { fill(x, y, w, h, k, true); }
  void frame(float x, float y, float w, float h, Color k) {
    rect(x, y, w, 1, k); rect(x, y + h - 1, w, 1, k); rect(x, y + 1, 1, h - 2, k); rect(x + w - 1, y + 1, 1, h - 2, k);
  }
  int textW(const std::string& s) const { return (int)s.size() * 6 - 1; }
  void text(float x, float y, const std::string& s, Color k, int align) {
    if (align == 1) x -= std::floor(textW(s) * 0.5f); else if (align == 2) x -= (float)textW(s);
    int px = (int)std::floor(x);
    const int py = (int)std::floor(y);
    for (char ch : s) {
      const int gi = pv::font5x7::glyphIndex(ch);
      if (gi >= 0)
        for (int r = 0; r < 7; r++)
          for (int q = 0; q < 5; q++)
            if (pv::font5x7::kFont[gi].rows[r] & (0x10 >> q)) blend(px + q, py + r, k, false);
      px += 6;
    }
  }
  // the beam column (loot_paint.h beamProfile) stretched over w x h from its rows sy..sy+sh
  void beam(float x, float y, float w, float h, Color k, int sy, int sh, bool addMode) {
    const int x0 = (int)std::floor(x), y0 = (int)std::floor(y), x1 = (int)std::floor(x + w), y1 = (int)std::floor(y + h);
    for (int yy = y0; yy < y1; yy++)
      for (int xx = x0; xx < x1; xx++) {
        const float sx = std::floor(((float)xx + 0.5f - x) / w * 16.0f), syy = std::floor((float)sy + ((float)yy + 0.5f - y) / h * (float)sh);
        const float a = (float)std::lround(255 * lootp::beamProfile(sx, syy)) / 255.0f;
        blend(xx, yy, Color(k.r, k.g, k.b, k.a * a), addMode);
      }
  }
  void pool(float x, float y, float w, float h, Color k) {
    const int x0 = (int)std::floor(x), y0 = (int)std::floor(y), x1 = (int)std::floor(x + w), y1 = (int)std::floor(y + h);
    for (int yy = y0; yy < y1; yy++)
      for (int xx = x0; xx < x1; xx++) {
        const float u = (((float)xx + 0.5f - x) / w) * 2 - 1, v = (((float)yy + 0.5f - y) / h) * 2 - 1;
        blend(xx, yy, Color(k.r, k.g, k.b, k.a * lootp::poolProfile(u, v)), true);
      }
  }
  void shadow(float x, float y, float w, float h, float a) {
    for (int yy = 0; yy < (int)h; yy++)
      for (int xx = 0; xx < (int)w; xx++) {
        const float dx = ((float)xx + 0.5f - w / 2) / (w / 2), dy = ((float)yy + 0.5f - h / 2) / (h / 2);
        const float d = dx * dx + dy * dy;
        if (d < 1) blend((int)x + xx, (int)y + yy, Color(20 / 255.0f, 16 / 255.0f, 30 / 255.0f, a * (110 / 255.0f) * (1 - d * 0.5f)), false);
      }
  }
  void icon(const Item& it, float x, float y, float size) {
    const Canvas ic = art::itemIconLook(gearIcon(it, nullptr));
    const int s = (int)size;
    for (int yy = 0; yy < s; yy++)
      for (int xx = 0; xx < s; xx++) {
        const uint32_t p = ic.get(xx * 16 / s, yy * 16 / s);
        if (!(p >> 24)) continue;
        blend((int)x + xx, (int)y + yy, Color((p & 255) / 255.0f, ((p >> 8) & 255) / 255.0f, ((p >> 16) & 255) / 255.0f, (p >> 24) / 255.0f), false);
      }
  }
  void noted(Rarity) {}
};

void fillRect(Canvas& c, int x, int y, int w, int h, uint32_t col) {
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) c.set(x + i, y + j, col);
}
void panel(Canvas& c, int x, int y, int w, int h) {
  fillRect(c, x, y, w, h, rgba(18, 15, 20));
  CanvasPainter P{c};
  P.frame((float)x, (float)y, (float)w, (float)h, Color(0.55f, 0.45f, 0.28f));
}

const ItemKind kKinds[11] = {ItemKind::Weapon, ItemKind::Bow, ItemKind::Staff, ItemKind::Armor, ItemKind::Helmet, ItemKind::Shield,
                             ItemKind::Gloves, ItemKind::Boots, ItemKind::Cloak, ItemKind::Ring, ItemKind::Amulet};
Item piece(ItemKind k, int sub, int ilvl, Rarity r, uint32_t seed) {
  Rng rr(0x10A7u ^ seed * 977u ^ (uint32_t)k * 131u ^ (uint32_t)r * 7u);
  return gear::makeGearC(rr, k, sub, ilvl, r, nullptr);
}

// the whole sheet at moment t (seconds)
Canvas sheet(float t0) {
  const int W = 720, H = 760;
  Canvas c(W, H);
  fillRect(c, 0, 0, W, H, rgba(12, 10, 14));
  CanvasPainter P{c};
  // ---- A: frames x kinds at three moments (the Mythic flame moves)
  panel(c, 6, 6, W - 12, 238);
  P.text(14, 12, "SLOT FRAMES (THE DOLL'S 28 PX BOX) AND LIST CELLS, THREE MOMENTS", lootp::kGold, 0);
  for (int r = 0; r < RARITIES; r++) {
    const float y = 28.0f + (float)r * 35;
    lootp::rarityText(P, t0, 14, y + 10, rarityName((Rarity)r), (Rarity)r);
    for (int k = 0; k < 11; k++) {
      const Item it = piece(kKinds[k], 0, 30, (Rarity)r, (uint32_t)k);
      const float x = 88.0f + (float)k * 33;
      P.rect(x, y, 28, 28, Color(0.1f, 0.085f, 0.08f, 0.95f));
      lootp::rarityFrame(P, t0 + (float)k * 0.13f, x, y, 28, 28, (Rarity)r, false, false);
      P.icon(it, x + 2, y + 2, 24);
    }
    // the list cells (12 px) and the Mythic frame at two later moments
    for (int m = 0; m < 3; m++) {
      const Item it = piece(ItemKind::Weapon, 0, 30, (Rarity)r, 3);
      const float x = 460.0f + (float)m * 82;
      lootp::rarityIcon(P, t0 + (float)m * 0.37f, it, x, y + 8, 12);
      lootp::rarityText(P, t0 + (float)m * 0.37f, x + 16, y + 10, lootp::fit(it.name, 60), (Rarity)r);
    }
  }
  // ---- B: the item cards (480 width: 188; phone shop: 178)
  panel(c, 6, 250, W - 12, 262);
  P.text(14, 256, "THE ITEM CARD AND THE COMPARE (188 PX: THE ITEMS TAB AT 480; 178: THE PHONE SHOP)", lootp::kGold, 0);
  const Item worn = piece(ItemKind::Weapon, 0, 28, Rarity::Rare, 11);
  const Item myth = piece(ItemKind::Weapon, 0, 31, Rarity::Mythic, 12);
  const Item iron = [] { Rng rr(77); return gear::makeGearC(rr, ItemKind::Weapon, 0, 28, Rarity::Common, nullptr, Mat::Iron); }();
  const Item amul = piece(ItemKind::Amulet, 0, 30, Rarity::Legendary, 13);
  lootp::CardCtx X;
  X.level = 30;
  X.attuned = 1;
  lootp::itemCard(P, t0, X, myth, 16, 272, 188, 506, true, &worn, false, true, nullptr);
  lootp::itemCard(P, t0, X, iron, 252, 272, 188, 506, true, &worn, false, true, nullptr);
  lootp::itemCard(P, t0, X, amul, 488, 272, 178, 400, true, nullptr, false, true, nullptr);
  lootp::itemCard(P, t0, X, worn, 488, 410, 178, 506, true, nullptr, true, true, nullptr);
  // ---- C: the ground, by day and at night
  panel(c, 6, 518, W - 12, 236);
  P.text(14, 524, "THE LOOT ON THE GROUND: DAY (LEFT) AND NIGHT (RIGHT), COMMON .. MYTHIC", lootp::kGold, 0);
  for (int night = 0; night < 2; night++) {
    const int gx0 = 12 + night * 352, gy0 = 538, gw = 344, gh = 210;
    for (int y = 0; y < gh; y++)
      for (int x = 0; x < gw; x++) {
        const uint32_t n = hash2((gx0 + x) / 2, (gy0 + y) / 2) % 100;
        uint32_t col = rgba(86, 140, 62);
        if (n < 18) col = rgba(78, 130, 58);
        else if (n > 92) col = rgba(100, 154, 68);
        c.set(gx0 + x, gy0 + y, col);
      }
    for (int row = 0; row < 2; row++)
      for (int r = 0; r < RARITIES; r++) {
        const Item it = piece(row ? ItemKind::Armor : ItemKind::Weapon, 0, 32, (Rarity)r, 21);
        const float bx = (float)gx0 + 30.0f + (float)r * 56, by = (float)gy0 + 100.0f + (float)row * 92;
        lootp::lootGround(P, t0, &it, r, bx, by, std::sin(t0 * 4 + (float)r) * 1.5f, 1.0f, r + row * 6);
      }
    if (night) {   // the light map: the night ambient plus the loot's lights (loot_ui.cpp lootLights), multiplied in
      std::vector<Color> L((size_t)gw * gh, Color(0.26f, 0.28f, 0.46f));
      auto light = [&](float wx, float wy, float rad, Color k, float a) {
        for (int y = 0; y < gh; y++)
          for (int x = 0; x < gw; x++) {
            const float d = std::hypot((float)(gx0 + x) - wx, (float)(gy0 + y) - wy) / rad;
            if (d >= 1) continue;
            const float f = (1 - d) * (1 - d) * a;
            Color& q = L[(size_t)y * gw + x];
            q = Color(std::min(1.0f, q.r + k.r * f), std::min(1.0f, q.g + k.g * f), std::min(1.0f, q.b + k.b * f));
          }
      };
      for (int row = 0; row < 2; row++)
        for (int r = 1; r < RARITIES; r++) {
          const float bx = (float)gx0 + 30.0f + (float)r * 56, by = (float)gy0 + 100.0f + (float)row * 92;
          const Color rc = lootp::colU(rarityColor((Rarity)r));
          switch (r) {
            case 1: light(bx, by - 4, 16, rc, 0.30f); break;
            case 2: light(bx, by - 4, 26, rc, 0.45f); light(bx, by - 22, 22, rc, 0.36f); break;
            case 3: light(bx, by - 4, 34, rc, 0.52f); light(bx, by - 28, 26, rc, 0.42f); break;
            case 4: light(bx, by - 6, 50, rc, 0.62f); light(bx, by - 36, 32, rc, 0.46f); break;
            default: light(bx, by - 8, 58, Color(1.0f, 0.50f, 0.20f), 0.70f); light(bx, by - 40, 36, Color(1.0f, 0.36f, 0.12f), 0.52f); break;
          }
        }
      for (int y = 0; y < gh; y++)
        for (int x = 0; x < gw; x++) {
          const Color d = fromU(c.get(gx0 + x, gy0 + y)), q = L[(size_t)y * gw + x];
          c.set(gx0 + x, gy0 + y, toU(Color(d.r * q.r, d.g * q.g, d.b * q.b)));
        }
    }
  }
  return c;
}

// the light a rarity's ground presence adds over plain grass, summed (the --check ladder)
double presence(int r, float t) {
  Canvas c(80, 100);
  for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) c.set(x, y, rgba(86, 140, 62));
  Canvas base = c;
  CanvasPainter P{c};
  lootp::lootGround(P, t, nullptr, r, 40, 90, 0, 1.0f, 0);
  double s = 0;
  for (int i = 0; i < c.w * c.h; i++) {
    const Color a = fromU(c.px[(size_t)i]), b = fromU(base.px[(size_t)i]);
    s += std::fabs(a.r - b.r) + std::fabs(a.g - b.g) + std::fabs(a.b - b.b);
  }
  return s;
}
bool sameCanvas(const Canvas& a, const Canvas& b) { return a.w == b.w && a.h == b.h && a.px == b.px; }
Canvas frameAt(int r, float t) {
  Canvas c(40, 40);
  fillRect(c, 0, 0, 40, 40, rgba(18, 15, 20));
  CanvasPainter P{c};
  lootp::rarityFrame(P, t, 6, 6, 28, 28, (Rarity)r, false, false);
  return c;
}

int check() {
  int fails = 0;
  auto bad = [&](const std::string& s) { std::printf("FAIL %s\n", s.c_str()); fails++; };
  for (int r = 0; r < RARITIES; r++) {
    const Canvas f = frameAt(r, 0.3f);
    int lit = 0;
    for (uint32_t p : f.px) lit += p != rgba(18, 15, 20);
    if (lit < 80) bad(std::string("frame ") + rarityName((Rarity)r) + ": " + std::to_string(lit) + " pixels");
    const Canvas f2 = frameAt(r, 1.1f);
    // Legendary's glint travels along its top edge; Mythic's whole ring burns; the others are still
    const bool moves = !sameCanvas(f, f2);
    if (r == (int)Rarity::Mythic && !moves) bad("Mythic frame is not animated");
    if (r < (int)Rarity::Legendary && moves) bad(std::string("frame ") + rarityName((Rarity)r) + " moves (only Legendary's glint and Mythic's flame do)");
  }
  double prev = 0;
  for (int r = 1; r < RARITIES; r++) {
    double s = 0;
    for (int k = 0; k < 8; k++) s += presence(r, 0.11f + 0.37f * (float)k);
    std::printf("presence %-9s %.1f\n", rarityName((Rarity)r), s / 8);
    if (s / 8 <= prev) bad(std::string("ground presence of ") + rarityName((Rarity)r) + " is not above the rarity below it");
    prev = s / 8;
  }
  if (presence(0, 0.5f) > 60) bad("Common has a ground presence");
  if (std::fabs(presence(5, 0.2f) - presence(5, 0.9f)) < 1.0) bad("Mythic's pillar is not animated");
  if (!sameCanvas(sheet(0.4f), sheet(0.4f))) bad("the sheet is not deterministic");
  std::printf("loot_gallery --check: %d failure(s)\n", fails);
  return fails ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && !std::strcmp(argv[1], "--check")) return check();
  const std::string out = argc > 1 ? argv[1] : ".";
  const Canvas c = sheet(0.4f);
  savePng(c, out + "/loot_sheet.png", 4);
  savePng(c, out + "/loot_sheet_1x.png", 1);
  // the Mythic pieces at three moments, zoomed (the flame frame and the pillar)
  for (int m = 0; m < 3; m++) {
    const Canvas s = sheet(0.4f + 0.29f * (float)m);
    Canvas z(240, 130);
    for (int y = 0; y < z.h; y++)
      for (int x = 0; x < z.w; x++) {
        const int sx = x < 120 ? 88 + 10 * 33 - 50 + x : 12 + 30 + 5 * 56 - 60 + (x - 120);
        const int sy = x < 120 ? 28 + 5 * 35 - 20 + y : 538 + y - 10;
        z.set(x, y, s.get(sx, sy));
      }
    savePng(z, out + "/loot_mythic_" + std::to_string(m) + ".png", 6);
  }
  return 0;
}
