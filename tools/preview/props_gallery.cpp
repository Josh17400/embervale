// Props gallery (M0 homes lane): every interior prop (old and new), the floor clutter, rugs and room surfaces, the
// town props, and whole generated v3 rooms composited the way the game draws them (decoLayers passes, then the props
// y-sorted), so the furniture, walls and clutter can be judged together.
//   props_gallery <outDir>      writes props.png, clutter.png, town.png (3x) and rooms_<type>.png (3x) + rooms_1x.png
#include "tools/preview/preview_util.h"

#include "rpg/sim/deco.h"
#include "rpg/sim/world.h"

namespace {

using art::Prop;

void blitA(Canvas& dst, const Canvas& s, int x, int y, int sx = 0, int sw = -1) {
  if (sw < 0) sw = s.w;
  for (int j = 0; j < s.h; j++)
    for (int i = 0; i < sw; i++) {
      uint32_t p = s.get(sx + i, j);
      int a = (int)(p >> 24);
      if (!a) continue;
      if (a == 255) { dst.set(x + i, y + j, p); continue; }
      dst.set(x + i, y + j, art::mix(dst.get(x + i, y + j), p | 0xFF000000u, a / 255.0f));
    }
}

// a plank-floor board for interior props
Canvas floorBoard(int w, int h) {
  Canvas c(w, h);
  for (int ty = 0; ty * 16 < h; ty++)
    for (int tx = 0; tx * 16 < w; tx++) {
      Canvas t = art::interiorPiece(art::pieceKey(art::Piece::Floor, (int)art::FloorStyle::Planks, tx, ty));
      blitA(c, t, tx * 16, ty * 16);
    }
  return c;
}

void label(Canvas& c, int x, int y, const std::string& s) {
  Board tmp(1, 1);
  (void)tmp;
  for (char ch : s) {
    int gi = pv::font5x7::glyphIndex(ch);
    if (gi >= 0)
      for (int r = 0; r < 7; r++)
        for (int k = 0; k < 5; k++)
          if (pv::font5x7::kFont[gi].rows[r] & (0x10 >> k)) { c.set(x + k + 1, y + r + 1, rgba(20, 16, 24)); c.set(x + k, y + r, rgba(255, 250, 230)); }
    x += 6;
  }
}

void propsSheet(const std::string& dir) {
  std::vector<Prop> list;
  for (int i = (int)Prop::Bed; i < (int)Prop::COUNT; i++) list.push_back((Prop)i);
  list.push_back(Prop::Chest); list.push_back(Prop::Barrel); list.push_back(Prop::Crate); list.push_back(Prop::Altar); list.push_back(Prop::Brazier);
  list.push_back(Prop::Statue); list.push_back(Prop::Banner); list.push_back(Prop::Woodpile); list.push_back(Prop::Haystack); list.push_back(Prop::Anvil);
  const int cols = 10, cellW = 56, cellH = 66;
  int rows = ((int)list.size() + cols - 1) / cols;
  Canvas c = floorBoard(cols * cellW, rows * cellH);
  for (size_t i = 0; i < list.size(); i++) {
    Prop p = list[i];
    Canvas s = art::propSprite(p);
    int fw = art::propW(p), fh = art::propH(p);
    int cx = (int)(i % cols) * cellW + cellW / 2, by = (int)(i / cols) * cellH + cellH - 12;
    blitA(c, s, cx - fw / 2, by - fh, 0, fw);
    label(c, (int)(i % cols) * cellW + 2, (int)(i / cols) * cellH + cellH - 10, std::to_string((int)p));
  }
  savePng(c, dir + "/props.png", kScale);
}

void clutterSheet(const std::string& dir) {
  Canvas c = floorBoard(16 * 24, 16 * 12);
  for (int i = 0; i < art::kClutterKinds; i++) blitA(c, art::interiorPiece(art::pieceKey(art::Piece::Clutter, i)), 8 + (i % 9) * 24, 8 + (i / 9) * 24);
  // rugs: a 3x2, a 1-wide runner and a 2x2 of every colour
  for (int col = 0; col < 4; col++) {
    int ox = 8 + col * 88, oy = 64;
    for (int y = 0; y < 2; y++)
      for (int x = 0; x < 3; x++) {
        int nm = (y > 0 ? 1 : 0) | (x < 2 ? 2 : 0) | (y < 1 ? 4 : 0) | (x > 0 ? 8 : 0);
        blitA(c, art::interiorPiece(art::pieceKey(art::Piece::Rug, col, nm)), ox + x * 16, oy + y * 16);
      }
    for (int y = 0; y < 4; y++) {
      int nm = (y > 0 ? 1 : 0) | (y < 3 ? 4 : 0);
      blitA(c, art::interiorPiece(art::pieceKey(art::Piece::Rug, col, nm)), ox + 56, oy + y * 16);
    }
  }
  savePng(c, dir + "/clutter.png", kScale);
}

void townSheet(const std::string& dir) {
  const Prop list[] = {Prop::FenceH, Prop::FenceV, Prop::Well, Prop::MarketStall, Prop::Crate, Prop::Barrel, Prop::Signpost, Prop::Cart,
                       Prop::Lamppost, Prop::Haystack, Prop::Woodpile, Prop::Tent, Prop::Chest, Prop::ChestOpen, Prop::Anvil, Prop::Fountain};
  const int n = (int)(sizeof list / sizeof list[0]), cols = 8, cellW = 56, cellH = 60;
  Board b(cols * cellW, ((n + cols - 1) / cols) * cellH);
  for (int i = 0; i < n; i++) {
    Prop p = list[i];
    Canvas s = art::propSprite(p);
    int fw = art::propW(p), fh = art::propH(p);
    int cx = (i % cols) * cellW + cellW / 2, by = (i / cols) * cellH + cellH - 10;
    blitA(b.c, s, cx - fw / 2, by - fh, 0, fw);
  }
  // a fence run, as the game lays them
  savePng(b.c, dir + "/town.png", kScale);
}

// a whole room, drawn like render.cpp + render_deco.cpp do
Canvas composeRoom(const Map& m) {
  const int pad = 8;
  Canvas c(m.w * 16 + pad * 2, m.h * 16 + pad * 2 + 32);
  for (auto& p : c.px) p = rgba(10, 9, 13);
  // terrain stand-in: the game paints terrain first; every interior tile is covered by a pass-0 piece anyway
  std::vector<DecoLayer> layers;
  std::vector<int> tileOf;
  for (int ty = 0; ty < m.h; ty++)
    for (int tx = 0; tx < m.w; tx++) {
      size_t n0 = layers.size();
      decoLayers(m, tx, ty, layers);
      for (size_t i = n0; i < layers.size(); i++) tileOf.push_back(ty * m.w + tx);
    }
  for (int pass = 0; pass < kDecoPasses; pass++)
    for (size_t i = 0; i < layers.size(); i++) {
      if (layers[i].pass != pass) continue;
      Canvas s = art::interiorPiece(layers[i].key);
      int tx = tileOf[i] % m.w, ty = tileOf[i] / m.w;
      blitA(c, s, pad + tx * 16 + layers[i].dx, pad + 32 + ty * 16 + layers[i].dy);
    }
  // props, row by row (the game y-sorts them with the actors)
  static std::vector<Canvas> sprites;
  if (sprites.empty()) for (int i = 0; i < (int)Prop::COUNT; i++) sprites.push_back(art::propSprite((Prop)i));
  for (int ty = 0; ty < m.h; ty++)
    for (int tx = 0; tx < m.w; tx++) {
      int pr = m.propAt(tx, ty);
      if (!pr) continue;
      Prop p = (Prop)(pr - 1);
      int fw = art::propW(p), fh = art::propH(p);
      blitA(c, sprites[(size_t)p], pad + tx * 16 + 8 - fw / 2, pad + 32 + ty * 16 + 16 - fh, 0, fw);
    }
  // spawns: a figure where each NPC stands
  for (const Spawn& s : m.spawns) {
    art::HumanLook look;
    static Canvas fig = art::humanSheet(look);
    Canvas one(art::HUMAN_W, art::HUMAN_H);
    for (int y = 0; y < art::HUMAN_H; y++) for (int x = 0; x < art::HUMAN_W; x++) one.set(x, y, fig.get(x, y));
    blitA(c, one, pad + s.x * 16, pad + 32 + s.y * 16 + 16 - art::HUMAN_H + 2);
  }
  return c;
}

void rooms(const std::string& dir) {
  struct T { art::Building b; Role owner; int w, h; const char* name; };
  const T types[] = {{art::Building::House, Role::Villager, 3, 2, "house"}, {art::Building::House, Role::Villager, 5, 3, "house_big"},
                     {art::Building::StoneHouse, Role::Villager, 4, 3, "stonehouse"}, {art::Building::Hut, Role::Villager, 3, 2, "hut"},
                     {art::Building::Farmhouse, Role::Farmer, 5, 3, "farmhouse"}, {art::Building::Inn, Role::Innkeeper, 6, 3, "inn"},
                     {art::Building::Shop, Role::Merchant, 4, 3, "shop"}, {art::Building::Smithy, Role::Smith, 5, 3, "smithy"},
                     {art::Building::Temple, Role::Priest, 6, 4, "temple"}, {art::Building::Keep, Role::Jarl, 9, 4, "keep"},
                     {art::Building::Tower, Role::Mage, 3, 3, "tower"}};
  Canvas all(1, 1);
  std::vector<Canvas> firsts;
  for (const T& t : types) {
    std::vector<Canvas> cs;
    for (int k = 0; k < 3; k++) {
      Bldg b;
      b.type = t.b; b.owner = t.owner; b.r = IRect{10, 10, t.w, t.h}; b.seed = 7771u + (uint32_t)k * 104729u + (uint32_t)t.b * 31u; b.genVer = WORLDGEN_V3;
      Map m;
      genInterior(m, b, b.seed);
      cs.push_back(composeRoom(m));
    }
    int W = 0, H = 0;
    for (auto& c : cs) { W += c.w + 8; H = std::max(H, c.h); }
    Canvas row(W, H);
    for (auto& p : row.px) p = rgba(10, 9, 13);
    int x = 0;
    for (auto& c : cs) { blitA(row, c, x, 0); x += c.w + 8; }
    savePng(row, dir + "/rooms_" + t.name + ".png", kScale);
    firsts.push_back(cs[0]);
  }
  int W = 0, H = 0;
  for (size_t i = 0; i < firsts.size(); i++) { if (i < 6) W += firsts[i].w + 8; }
  int W2 = 0;
  for (size_t i = 6; i < firsts.size(); i++) W2 += firsts[i].w + 8;
  W = std::max(W, W2);
  int H1 = 0, H2 = 0;
  for (size_t i = 0; i < firsts.size(); i++) (i < 6 ? H1 : H2) = std::max(i < 6 ? H1 : H2, firsts[i].h);
  H = H1 + H2 + 8;
  Canvas sheet(W, H);
  for (auto& p : sheet.px) p = rgba(10, 9, 13);
  int x = 0;
  for (size_t i = 0; i < firsts.size(); i++) {
    if (i == 6) x = 0;
    blitA(sheet, firsts[i], x, i < 6 ? 0 : H1 + 8);
    x += firsts[i].w + 8;
  }
  savePng(sheet, dir + "/rooms_1x.png", 1);
}
}  // namespace

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : ".";
  propsSheet(dir);
  clutterSheet(dir);
  townSheet(dir);
  rooms(dir);
  return 0;
}
