// EMBERVALE art preview: composes contact sheets of every procedural sprite and saves them as PNGs.
// Usage: art_preview [outDir]   (default: %TEMP%\claude\embervale_art)
// Sprites are shown on a grass-coloured ground, scaled 3x with nearest neighbour.
#include <SDL3/SDL.h>
#include "rpg/build/blueprint.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "engine/pix.h"
#include "rpg/art.h"

namespace pv {
#include "engine/font5x7.h"
}

namespace {

constexpr int kScale = 3;

uint32_t hash2(int x, int y) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

// A board is a canvas with a grass background onto which sprites and labels are placed.
struct Board {
  Canvas c;
  Board(int w, int h) : c(w, h) {
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        uint32_t n = hash2(x / 2, y / 2) % 100;
        uint32_t col = rgba(86, 140, 62);
        if (n < 18) col = rgba(78, 130, 58);
        else if (n > 92) col = rgba(100, 154, 68);
        uint32_t t = hash2(x, y * 7 + 3) % 400;
        if (t == 0) { c.set(x, y, rgba(118, 170, 76)); c.set(x, y - 1, rgba(118, 170, 76)); continue; }
        c.set(x, y, col);
      }
  }
  void put(const Canvas& s, int x, int y) {
    for (int j = 0; j < s.h; j++)
      for (int i = 0; i < s.w; i++) {
        uint32_t p = s.get(i, j);
        int a = (int)(p >> 24);
        if (!a) continue;
        if (a == 255) { c.set(x + i, y + j, p); continue; }
        c.set(x + i, y + j, art::mix(c.get(x + i, y + j), p | 0xFF000000u, a / 255.0f));
      }
  }
  // a darker strip behind a sprite so its canvas bounds are visible
  void frame(int x, int y, int w, int h) {
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++)
        if (i == 0 || j == 0 || i == w - 1 || j == h - 1) {
          uint32_t p = c.get(x + i, y + j);
          c.set(x + i, y + j, art::shade(p, 0.82f));
        }
  }
  void text(int x, int y, const std::string& s, uint32_t col = rgba(255, 250, 230)) {
    for (char ch : s) {
      int gi = pv::font5x7::glyphIndex(ch);
      if (gi >= 0)
        for (int r = 0; r < 7; r++)
          for (int k = 0; k < 5; k++)
            if (pv::font5x7::kFont[gi].rows[r] & (0x10 >> k)) {
              c.set(x + k + 1, y + r + 1, rgba(30, 40, 30));
              c.set(x + k, y + r, col);
            }
      x += 6;
    }
  }
};

bool savePng(const Canvas& src, const std::string& path, int scale) {
  Canvas big(src.w * scale, src.h * scale);
  for (int y = 0; y < big.h; y++)
    for (int x = 0; x < big.w; x++) big.px[(size_t)y * big.w + x] = src.get(x / scale, y / scale) | 0xFF000000u;
  SDL_Surface* s = SDL_CreateSurfaceFrom(big.w, big.h, SDL_PIXELFORMAT_RGBA32, big.px.data(), big.w * 4);
  if (!s) return false;
  bool ok = SDL_SavePNG(s, path.c_str());
  SDL_DestroySurface(s);
  std::printf("%s %s (%dx%d)\n", ok ? "wrote" : "FAILED", path.c_str(), big.w, big.h);
  return ok;
}

// (M2 fixer round 2) a name table's entry, or the number past its end: the art enums grew past these lists and the
// contact sheets read past them (a crash before props / buildings / icons were written)
template <size_t N> std::string nameAt(const char* const (&a)[N], int i) { return i >= 0 && i < (int)N ? std::string(a[i]) : "#" + std::to_string(i); }
const char* kOutfitNames[] = {"TUNIC", "DRESS", "ROBE", "LEATHER", "CHAIN", "PLATE", "ELVEN", "EBONY", "GUARD", "RAGS"};
const char* kHairNames[] = {"BALD", "SHORT", "LONG", "PONY", "MOHAWK", "BRAIDS"};

// a varied but deterministic look for each outfit/hair cell
art::HumanLook sampleLook(int outfit, int hair) {
  static const uint32_t skins[] = {rgba(240, 196, 152), rgba(214, 160, 116), rgba(168, 112, 76), rgba(112, 72, 52), rgba(246, 210, 176)};
  static const uint32_t hairs[] = {rgba(90, 56, 30), rgba(40, 30, 28), rgba(196, 140, 64), rgba(150, 64, 36), rgba(200, 196, 188), rgba(232, 200, 120)};
  static const uint32_t tops[] = {rgba(60, 100, 160), rgba(150, 52, 60), rgba(70, 120, 70), rgba(120, 80, 150), rgba(190, 150, 70), rgba(60, 130, 140)};
  art::HumanLook l;
  l.outfit = (art::Outfit)outfit;
  l.hair = (art::Hair)hair;
  int k = outfit * 7 + hair * 3;
  l.skin = skins[k % 5];
  l.hairColor = hairs[(k + hair) % 6];
  l.topColor = tops[(outfit + hair) % 6];
  l.bottomColor = (k & 1) ? rgba(80, 64, 50) : rgba(70, 70, 92);
  l.tabardColor = outfit == 8 ? rgba(150, 40, 40) : tops[(outfit + hair + 3) % 6];
  l.beard = (outfit + hair) % 4 == 1 && outfit != 1;
  return l;
}

void humanCombos(const std::string& dir) {
  const int cols = 6, rows = 10, cw = 3 * art::HUMAN_W + 6, ch = art::HUMAN_H + 4;
  Board b(48 + cols * cw, 12 + rows * ch);
  for (int h = 0; h < cols; h++) b.text(48 + h * cw + 4, 2, nameAt(kHairNames, h));
  for (int o = 0; o < rows; o++) {
    b.text(2, 12 + o * ch + 9, nameAt(kOutfitNames, o));
    for (int h = 0; h < cols; h++) {
      Canvas sheet = art::humanSheet(sampleLook(o, h));
      for (int r = 0; r < 3; r++) {
        Canvas cell(art::HUMAN_W, art::HUMAN_H);
        int row = r == 0 ? 0 : (r == 1 ? 2 : 1);
        for (int y = 0; y < cell.h; y++)
          for (int x = 0; x < cell.w; x++) cell.set(x, y, sheet.get(x, row * art::HUMAN_H + y));
        b.put(cell, 48 + h * cw + r * art::HUMAN_W, 12 + o * ch + 2);
      }
    }
  }
  savePng(b.c, dir + "/humans_combos.png", kScale);
}

void humanSheets(const std::string& dir) {
  std::vector<std::pair<std::string, art::HumanLook>> looks;
  art::HumanLook a;  // player default + sword + shield
  a.weapon = 1; a.shield = true; a.cape = true;
  looks.push_back({"HERO SWORD+SHIELD+CAPE", a});
  art::HumanLook g; g.outfit = art::Outfit::Guard; g.helmet = true; g.weapon = 1; g.shield = true; g.beard = true;
  looks.push_back({"GUARD HELMET", g});
  art::HumanLook m; m.outfit = art::Outfit::Robe; m.hood = true; m.weapon = 4; m.topColor = rgba(70, 60, 130); m.tabardColor = rgba(210, 170, 70); m.hair = art::Hair::Long; m.hairColor = rgba(220, 220, 210); m.beard = true;
  looks.push_back({"MAGE HOOD STAFF", m});
  art::HumanLook bnd; bnd.outfit = art::Outfit::Leather; bnd.hood = true; bnd.weapon = 2; bnd.bottomColor = rgba(70, 60, 50); bnd.hair = art::Hair::Mohawk;
  looks.push_back({"BANDIT AXE", bnd});
  art::HumanLook p; p.outfit = art::Outfit::Plate; p.helmet = true; p.weapon = 6; p.cape = true; p.tabardColor = rgba(40, 70, 140);
  looks.push_back({"PLATE HAMMER", p});
  art::HumanLook e; e.outfit = art::Outfit::Elven; e.helmet = true; e.weapon = 3; e.hair = art::Hair::Long; e.hairColor = rgba(232, 200, 120); e.weaponColor = rgba(220, 190, 90);
  looks.push_back({"ELVEN BOW", e});
  art::HumanLook eb; eb.outfit = art::Outfit::Ebony; eb.helmet = true; eb.weapon = 1; eb.weaponColor = rgba(110, 70, 140); eb.shield = true; eb.cape = true; eb.tabardColor = rgba(90, 20, 40);
  looks.push_back({"EBONY", eb});
  art::HumanLook d; d.outfit = art::Outfit::Dress; d.hair = art::Hair::Braids; d.topColor = rgba(170, 70, 90); d.tabardColor = rgba(240, 220, 170); d.hairColor = rgba(170, 70, 30); d.weapon = 5;
  looks.push_back({"DRESS DAGGER", d});
  art::HumanLook r; r.outfit = art::Outfit::Rags; r.hair = art::Hair::Ponytail; r.weapon = 6; r.skin = rgba(168, 112, 76); r.hairColor = rgba(40, 30, 28);
  looks.push_back({"RAGS PICK", r});
  art::HumanLook c; c.outfit = art::Outfit::Chain; c.helmet = true; c.weapon = 1; c.weaponColor = rgba(120, 220, 140); c.hair = art::Hair::Braids;
  looks.push_back({"CHAIN GLASS SWORD", c});
  const int colW = art::HUMAN_W * art::HUMAN_FRAMES + 8, rowH = art::HUMAN_H * 3 + 14;
  Board b(4 + 2 * colW, 4 + 5 * rowH);
  for (size_t i = 0; i < looks.size(); i++) {
    int x = 4 + (int)(i % 2) * colW, y = 4 + (int)(i / 2) * rowH;
    b.text(x, y, looks[i].first);
    Canvas s = art::humanSheet(looks[i].second);
    b.put(s, x, y + 10);
    for (int f = 0; f < art::HUMAN_FRAMES; f++)
      for (int rr = 0; rr < 3; rr++) b.frame(x + f * art::HUMAN_W, y + 10 + rr * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H);
  }
  savePng(b.c, dir + "/humans_sheets.png", kScale);
}

const char* kMonsterNames[] = {"WOLF", "BOAR", "BEAR", "SLIME", "SPIDER", "BAT", "SKELETON", "DRAUGR", "GOBLIN",
                               "TROLL", "WRAITH", "MUDCRAB", "ICE WOLF", "FROST SPIDER", "SANDWORM", "DRAGON"};

void monsters(const std::string& dir) {
  int W = 0, H = 4;
  for (int m = 0; m < (int)art::Monster::COUNT; m++) {
    W = std::max(W, art::monsterCellW((art::Monster)m) * art::MONSTER_FRAMES + 8);
    H += art::monsterCellH((art::Monster)m) + 12;
  }
  Board b(W, H);
  int y = 4;
  for (int m = 0; m < (int)art::Monster::COUNT; m++) {
    auto mm = (art::Monster)m;
    int cw = art::monsterCellW(mm), chh = art::monsterCellH(mm);
    b.text(4, y, nameAt(kMonsterNames, m) + " " + std::to_string(cw) + "x" + std::to_string(chh));
    Canvas s = art::monsterSheet(mm);
    b.put(s, 4, y + 10);
    for (int f = 0; f < art::MONSTER_FRAMES; f++) b.frame(4 + f * cw, y + 10, cw, chh);
    y += chh + 12;
  }
  savePng(b.c, dir + "/monsters.png", kScale);
}

const char* kPropNames[] = {
  "OAK", "OAK2", "PINE", "PINE2", "SNOWPINE", "BIRCH", "DEAD", "WILLOW", "PALM", "AUTUMN",
  "BUSH", "BERRY", "SNOWBUSH", "BOULDER", "ROCK", "MOSSROCK", "SNOWROCK", "STUMP", "LOG",
  "FLOWER1", "FLOWER2", "FLOWER3", "GRASS", "REEDS", "CACTUS", "SHROOMS", "LILY", "FERN",
  "CHEST", "CHESTOPEN", "BARREL", "CRATE", "TORCH", "CAMPFIRE", "SIGN", "WELL", "FENCEH", "FENCEV", "LAMP", "TENT",
  "STALL", "HAY", "ANVIL", "CART", "FOUNTAIN", "STATUE", "BANNER", "WOODPILE", "GRAVE", "SHRINE",
  "CAVE", "LADDER", "STALAGMITE", "CRYSTAL", "BONES", "SKULLS", "COBWEB", "BRAZIER", "COFFIN", "URN", "IRONDOOR", "ALTAR",
  "BED", "TABLE", "CHAIR", "SHELF", "FIREPLACE", "RUG", "COUNTER", "BARREL2", "PLANT", "CAULDRON", "BOOKS", "THRONE"};
constexpr int kPropNamesN = (int)(sizeof(kPropNames) / sizeof(kPropNames[0]));
// (M2 fixer round 2) the props appended since this list was written (M0, M0b, M1, M2) are named by their number: the
// list ran out at 74 and the contact sheet read past its end (a crash before props.png was written)
std::string propName(int p) { return p < kPropNamesN ? std::string(kPropNames[p]) : "P" + std::to_string(p); }

void props(const std::string& dir) {
  const int maxW = 400;
  // layout pass then draw pass
  struct Item { int p, x, y; };
  std::vector<Item> items;
  int x = 4, y = 4, rowH = 0;
  for (int p = 0; p < (int)art::Prop::COUNT; p++) {
    auto pp = (art::Prop)p;
    int w = art::propW(pp) * art::propFrames(pp), h = art::propH(pp) + 10;
    int cellW = std::max(w, (int)propName(p).size() * 6) + 6;
    if (x + cellW > maxW) { x = 4; y += rowH + 4; rowH = 0; }
    items.push_back({p, x, y});
    x += cellW;
    rowH = std::max(rowH, h);
  }
  Board b(maxW, y + rowH + 6);
  for (auto& it : items) {
    auto pp = (art::Prop)it.p;
    b.text(it.x, it.y, propName(it.p));
    Canvas s = art::propSprite(pp);
    b.put(s, it.x, it.y + 9);
    for (int f = 0; f < art::propFrames(pp); f++) b.frame(it.x + f * art::propW(pp), it.y + 9, art::propW(pp), art::propH(pp));
  }
  savePng(b.c, dir + "/props.png", kScale);
}

const char* kBuildingNames[] = {"HOUSE", "STONEHOUSE", "INN", "SMITHY", "SHOP", "TEMPLE", "KEEP", "TOWER", "FARMHOUSE", "HUT"};

void buildings(const std::string& dir) {
  const int sizes[4][2] = {{3, 2}, {4, 3}, {5, 3}, {6, 4}};
  int W = 4, H = 4;
  for (auto& s : sizes) W += s[0] * 16 + 8;
  H += (int)art::Building::COUNT * (6 * 16 + 12);
  Board b(W + 70, H);
  int y = 4;
  for (int bi = 0; bi < (int)art::Building::COUNT; bi++) {
    int x = 4, rowH = 0;
    b.text(x, y, nameAt(kBuildingNames, bi));
    for (int si = 0; si < 4; si++) {
      int w = sizes[si][0], h = sizes[si][1];
      Canvas s = art::buildingSprite(bld::design(bld::classicRequest((art::Building)bi, w, h, 0, (uint32_t)(bi * 31 + si * 7 + 1))));
      b.put(s, x, y + 10);
      rowH = std::max(rowH, s.h);
      x += s.w + 8;
    }
    // one tinted variant
    Canvas t = art::buildingSprite(bld::design(bld::classicRequest((art::Building)bi, 3, 2, rgba(70, 110, 160), 99)));
    b.put(t, x, y + 10);
    y += rowH + 14;
  }
  savePng(b.c, dir + "/buildings.png", kScale);
}

const char* kIconNames[] = {"SWORD", "GREATSWORD", "AXE", "DAGGER", "MACE", "BOW", "STAFF", "ARROWS", "SHIELD", "HELMET",
                            "ARMOR", "BOOTS", "GLOVES", "RING", "AMULET", "POT R", "POT B", "POT G", "BREAD", "MEAT",
                            "APPLE", "CHEESE", "GOLD", "GEM", "KEY", "SCROLL", "BOOK", "MAP", "PELT", "BONE",
                            "ORE", "HERB", "SIGIL", "LETTER", "CROWN"};
const char* kFxNames[] = {"SLASH", "SLASHDOWN", "SLASHUP", "ARROW", "ARROWDOWN", "FIREBALL", "EXPLOSION", "SPARKLE", "BLOOD", "DUST", "FROST", "HEAL"};

void wallsIconsFx(const std::string& dir) {
  Board b(560, 360);
  // walls: all 16 masks, then a little composed wall ring with the gate
  b.text(4, 4, "WALL MASKS 0-15");
  for (int m = 0; m < 16; m++) {
    Canvas w = art::wallPiece(m);
    b.put(w, 4 + m * 20, 14);
    b.frame(4 + m * 20, 14, 16, 32);
  }
  b.text(4, 52, "WALL RING + GATE");
  {
    // 9 x 6 tile ring; gate spans tiles 3..5 of the bottom row
    const int ox = 4, oy = 62, tw = 9, th = 6;
    auto isWall = [&](int tx, int ty) {
      if (tx < 0 || ty < 0 || tx >= tw || ty >= th) return false;
      return tx == 0 || ty == 0 || tx == tw - 1 || ty == th - 1;
    };
    for (int ty = 0; ty < th; ty++)
      for (int tx = 0; tx < tw; tx++) {
        if (!isWall(tx, ty)) continue;
        if (ty == th - 1 && tx >= 3 && tx <= 5) continue;
        int mask = (isWall(tx, ty - 1) ? 1 : 0) | (isWall(tx + 1, ty) ? 2 : 0) | (isWall(tx, ty + 1) ? 4 : 0) | (isWall(tx - 1, ty) ? 8 : 0);
        b.put(art::wallPiece(mask), ox + tx * 16, oy + ty * 16 - 16 + 16);
      }
    Canvas g = art::gatePiece();
    b.put(g, ox + 3 * 16, oy + (th - 1) * 16 + 16 - g.h + 16);
  }
  b.text(170, 52, "GATE");
  b.put(art::gatePiece(), 170, 62);
  b.frame(170, 62, 48, 40);
  // icons, untinted then tier tints
  b.text(4, 196, "ICONS (DEFAULT / STEEL / ELVEN / GLASS / EBONY / DAEDRIC)");
  const uint32_t tints[] = {0, rgba(190, 200, 214), rgba(220, 180, 70), rgba(110, 210, 140), rgba(90, 60, 120), rgba(190, 40, 40)};
  for (int i = 0; i < (int)art::Icon::COUNT; i++) {
    int col = i % 18, row = i / 18;
    b.put(art::itemIcon((art::Icon)i), 4 + col * 22, 206 + row * 20);
  }
  for (int t = 1; t < 6; t++)
    for (int i = 0; i < 15; i++) b.put(art::itemIcon((art::Icon)i, tints[t]), 4 + i * 22, 248 + (t - 1) * 18 - 2);
  // fx
  int fx = 230, fy = 62;
  b.text(fx, 52, "FX");
  for (int f = 0; f < (int)art::Fx::COUNT; f++) {
    auto ff = (art::Fx)f;
    Canvas s = art::fxSprite(ff);
    b.put(s, fx, fy);
    fy += art::fxH(ff) + 3;
    if (fy > 150) { fy = 62; fx += 110; }
  }
  savePng(b.c, dir + "/walls_icons_fx.png", kScale);
}

// A small village vignette: everything placed bottom-centre anchored and y-sorted, as the game will.
void scene(const std::string& dir) {
  const int W = 320, H = 208;
  Board b(W, H);
  // dirt road + a pond for context
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      int road = 132 + (int)(std::sin(x * 0.03f) * 4);
      if (y >= road && y < road + 14) {
        uint32_t n = hash2(x / 2, y / 2) % 7;
        b.c.set(x, y, n == 0 ? rgba(160, 124, 84) : (n == 1 ? rgba(186, 150, 104) : rgba(174, 138, 94)));
      }
      float dx = (x - 262) / 30.0f, dy = (y - 180) / 14.0f;
      float d = dx * dx + dy * dy;
      if (d < 1) b.c.set(x, y, d > 0.8f ? rgba(150, 140, 100) : (d > 0.6f ? rgba(70, 130, 180) : rgba(56, 110, 166)));
    }
  struct Item { Canvas c; int x, y; };   // x = centre, y = bottom
  std::vector<Item> items;
  auto add = [&](Canvas c, int cx, int bottom) { items.push_back({std::move(c), cx, bottom}); };
  auto frameOf = [](const Canvas& sheet, int fw, int fh, int col, int row) {
    Canvas c(fw, fh);
    for (int y = 0; y < fh; y++) for (int x = 0; x < fw; x++) c.set(x, y, sheet.get(col * fw + x, row * fh + y));
    return c;
  };
  add(art::buildingSprite(bld::design(bld::classicRequest(art::Building::House, 4, 3, 0, 3))), 40, 112);
  add(art::buildingSprite(bld::design(bld::classicRequest(art::Building::Inn, 5, 3, 0, 4))), 124, 112);
  add(art::buildingSprite(bld::design(bld::classicRequest(art::Building::Smithy, 4, 3, 0, 5))), 210, 112);
  add(art::buildingSprite(bld::design(bld::classicRequest(art::Building::Temple, 4, 3, 0, 6))), 284, 104);
  add(art::propSprite(art::Prop::OakTree), 6, 136);
  add(art::propSprite(art::Prop::OakTree2), 300, 140);
  add(art::propSprite(art::Prop::PineTree), 248, 196);
  add(art::propSprite(art::Prop::AutumnTree), 20, 204);
  add(art::propSprite(art::Prop::BirchTree), 168, 205);
  add(art::propSprite(art::Prop::Well), 84, 168);
  add(frameOf(art::propSprite(art::Prop::Campfire), 20, 20, 1, 0), 120, 186);
  add(frameOf(art::propSprite(art::Prop::Torch), 8, 20, 0, 0), 96, 128);
  add(art::propSprite(art::Prop::Barrel), 238, 124);
  add(art::propSprite(art::Prop::Crate), 252, 125);
  add(art::propSprite(art::Prop::Bush), 60, 126);
  add(art::propSprite(art::Prop::Flowers1), 150, 124);
  add(art::propSprite(art::Prop::Flowers3), 36, 124);
  add(art::propSprite(art::Prop::Boulder), 210, 196);
  add(art::propSprite(art::Prop::Signpost), 176, 130);
  for (int i = 0; i < 4; i++) add(art::propSprite(art::Prop::FenceH), 40 + i * 16 - 8 + 8, 200);
  add(art::propSprite(art::Prop::LilyPad), 268, 182);
  add(art::propSprite(art::Prop::Reeds), 240, 184);
  // people
  art::HumanLook hero; hero.weapon = 1; hero.shield = true; hero.cape = true;
  add(frameOf(art::humanSheet(hero), 16, 24, 0, 0), 144, 150);
  art::HumanLook g; g.outfit = art::Outfit::Guard; g.helmet = true; g.weapon = 1;
  add(frameOf(art::humanSheet(g), 16, 24, 2, 2), 200, 146);
  art::HumanLook v; v.outfit = art::Outfit::Dress; v.hair = art::Hair::Long; v.topColor = rgba(170, 70, 90); v.hairColor = rgba(232, 200, 120);
  add(frameOf(art::humanSheet(v), 16, 24, 0, 2), 104, 186);
  art::HumanLook m; m.outfit = art::Outfit::Robe; m.hood = true; m.weapon = 4; m.topColor = rgba(70, 60, 130); m.tabardColor = rgba(210, 170, 70);
  add(frameOf(art::humanSheet(m), 16, 24, 0, 0), 136, 190);
  // monsters
  add(frameOf(art::monsterSheet(art::Monster::Wolf), 32, 22, 1, 0), 290, 150);
  add(frameOf(art::monsterSheet(art::Monster::Slime), 20, 16, 0, 0), 196, 168);
  std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.y < b.y; });
  for (auto& it : items) b.put(it.c, it.x - it.c.w / 2, it.y - it.c.h);
  savePng(b.c, dir + "/scene.png", kScale);
}

}  // namespace

int main(int argc, char** argv) {
  std::string dir;
  if (argc > 1) dir = argv[1];
  else {
    const char* t = std::getenv("TEMP");
    dir = std::string(t ? t : ".") + "\\claude\\embervale_art";
  }
  SDL_CreateDirectory(dir.c_str());
  humanCombos(dir);
  humanSheets(dir);
  monsters(dir);
  props(dir);
  buildings(dir);
  wallsIconsFx(dir);
  scene(dir);
  return 0;
}
