// Rooms gallery (M0b interior-art lane): interior walls, doorways, doors and stairs drawn straight from the rooms.h
// geometry contract, composited the way the game draws them (decoLayers passes, then the props y-sorted, stairs and
// doors in the room's material via interiorPropKey).
//   rooms_gallery <outDir>
//     contract_<style>.png   a hand-made test floor per RoomStyle at 4x: E-W and N-S partitions, T-joins with the back
//                            wall, the side walls and each other, a cross, free ends, doorways (DoorH / DoorV, open and
//                            shut), StairsUp under the back wall and under a partition face, StairsDown, wall decor on a
//                            partition face
//     contract_1x.png        all eight at 1x
//     real_<seed>_<n>_<type>.png   every floor of a few WORLDGEN_V7 buildings (genInterior), 3x; real_1x.png at 1x
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

void label(Canvas& c, int x, int y, const std::string& s) {
  for (char ch : s) {
    int gi = pv::font5x7::glyphIndex(ch);
    if (gi >= 0)
      for (int r = 0; r < 7; r++)
        for (int k = 0; k < 5; k++)
          if (pv::font5x7::kFont[gi].rows[r] & (0x10 >> k)) { c.set(x + k + 1, y + r + 1, rgba(20, 16, 24)); c.set(x + k, y + r, rgba(255, 250, 230)); }
    x += 6;
  }
}

// a whole floor, drawn like render.cpp + render_deco.cpp do. shut: doors drawn shut (as when nobody is near a private
// room's door). figures: (tx, ty) tiles where a person stands, for scale.
Canvas composeRoom(const Map& m, bool shut, const std::vector<std::pair<int, int>>& figures) {
  const int pad = 8, top = 32;
  Canvas c(m.w * 16 + pad * 2, m.h * 16 + pad * 2 + top);
  for (auto& p : c.px) p = rgba(10, 9, 13);
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
      blitA(c, s, pad + tx * 16 + layers[i].dx, pad + top + ty * 16 + layers[i].dy);
    }
  static std::vector<Canvas> sprites;
  if (sprites.empty()) for (int i = 0; i < (int)Prop::COUNT; i++) sprites.push_back(art::propSprite((Prop)i));
  static Canvas fig;
  if (fig.w == 0) {
    art::HumanLook look;
    Canvas sheet = art::humanSheet(look);
    fig = Canvas(art::HUMAN_W, art::HUMAN_H);
    for (int y = 0; y < art::HUMAN_H; y++) for (int x = 0; x < art::HUMAN_W; x++) fig.set(x, y, sheet.get(x, y));
  }
  auto drawProp = [&](int tx, int ty) {
    int pr = m.propAt(tx, ty);
    if (!pr) return;
    Prop p = (Prop)(pr - 1);
    uint32_t ik = interiorPropKey(m, tx, ty, p);
    if (ik && shut && (p == Prop::DoorH || p == Prop::DoorV)) ik |= 2u << 24;
    if (ik) {
      Canvas s = art::interiorPiece(ik);
      blitA(c, s, pad + tx * 16 + 8 - s.w / 2, pad + top + ty * 16 + 16 - s.h);
      return;
    }
    int fw = art::propW(p), fh = art::propH(p);
    blitA(c, sprites[(size_t)p], pad + tx * 16 + 8 - fw / 2, pad + top + ty * 16 + 16 - fh, 0, fw);
  };
  // flat props first, then rows top to bottom with the figures sorted in (as the game y-sorts them)
  for (int ty = 0; ty < m.h; ty++)
    for (int tx = 0; tx < m.w; tx++) {
      int pr = m.propAt(tx, ty);
      if (pr && (Prop)(pr - 1) == Prop::StairsDown) drawProp(tx, ty);
    }
  for (int ty = 0; ty < m.h; ty++) {
    for (int tx = 0; tx < m.w; tx++) {
      int pr = m.propAt(tx, ty);
      if (pr && (Prop)(pr - 1) != Prop::StairsDown) drawProp(tx, ty);
    }
    for (auto& f : figures)
      if (f.second == ty) blitA(c, fig, pad + f.first * 16 + 8 - art::HUMAN_W / 2, pad + top + ty * 16 + 16 - art::HUMAN_H + 2);
    for (const Spawn& s : m.spawns)
      if (s.y == ty) blitA(c, fig, pad + s.x * 16 + 8 - art::HUMAN_W / 2, pad + top + s.y * 16 + 16 - art::HUMAN_H + 2);
  }
  return c;
}

// the contract test floor (17 x 16):
//   back wall rows 0..1; side walls; front wall row 15 with the entrance at (4, 15)
//   N-S partition x=5 rows 2..5 (hangs from the back wall, T into the E-W below), and rows 8..9 below it (a cross,
//   free end at row 9); N-S x=11 rows 2..5 with a DoorV at row 3
//   E-W partition rows 6 (cap) / 7 (face) wall to wall, DoorH at x=3 and x=13
//   N-S x=8 rows 8..14 (T from the E-W above down to the front wall) with a DoorV at row 10
//   E-W rows 12 / 13 from x=8 to the east side wall (a T against x=8), DoorH at x=12
//   StairsUp under the back wall (1, 2) and under the partition face (14, 8); StairsDown (3, 12)
Map contractMap(art::RoomStyle rs, bool stoneFloor) {
  const int W = 17, H = 16;
  Map m;
  m.kind = MapKind::Interior;
  m.alloc(W, H, Ground::InteriorWall);
  Ground fl = stoneFloor ? Ground::StoneFloor : Ground::WoodFloor;
  for (int y = 2; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) m.setG(x, y, fl);
  m.exitX = 4; m.exitY = H - 1;
  m.setG(4, H - 1, fl);
  auto wall = [&](int x, int y) { m.setG(x, y, Ground::InteriorWall); };
  for (int y = 2; y <= 5; y++) { wall(5, y); wall(11, y); }
  for (int x = 1; x < W - 1; x++) { wall(x, 6); wall(x, 7); }
  for (int y = 8; y <= 9; y++) wall(5, y);
  for (int y = 8; y <= 14; y++) wall(8, y);
  for (int x = 9; x < W - 1; x++) { wall(x, 12); wall(x, 13); }
  auto doorH = [&](int x, int capY) { m.setG(x, capY, fl); m.setG(x, capY + 1, fl); m.setProp(x, capY + 1, Prop::DoorH); };
  auto doorV = [&](int x, int y) { m.setG(x, y, fl); m.setProp(x, y, Prop::DoorV); };
  doorH(3, 6); doorH(13, 6); doorH(12, 12);
  doorV(11, 3); doorV(8, 10);
  m.setProp(1, 2, Prop::StairsUp);
  m.setProp(14, 8, Prop::StairsUp);
  m.setProp(3, 12, Prop::StairsDown);
  for (int x = 1; x < W - 1; x++) m.deco[(size_t)(1 * W + x)] = (uint8_t)((int)Deco::WallTimber + (int)rs);
  // a little furniture and decor, for scale and the consistency pass
  m.setProp(7, 1, Prop::Window);
  m.setProp(13, 1, Prop::Sconce);
  m.setProp(6, 7, Prop::WallShelf);
  m.setProp(10, 7, Prop::Sconce);
  m.setProp(2, 7, Prop::HerbBundle);
  m.setProp(15, 13, Prop::Painting);
  m.setProp(6, 2, Prop::Bed);
  m.setProp(7, 2, Prop::Nightstand);
  m.setProp(9, 2, Prop::Chest);
  m.setProp(12, 2, Prop::Dresser);
  m.setProp(15, 2, Prop::Wardrobe);
  m.setProp(14, 4, Prop::TableSmall);
  m.setProp(6, 10, Prop::Barrel);
  m.setProp(1, 8, Prop::Cupboard);
  m.setProp(10, 14, Prop::Bench);
  m.rebuildSolid();
  return m;
}

const char* styleName(art::RoomStyle rs) {
  static const char* n[] = {"timber", "log", "stone", "hall", "soot", "arcane", "adobe", "plaster"};
  return n[(int)rs & 7];
}

void contract(const std::string& dir) {
  std::vector<Canvas> all;
  for (int s = 0; s < (int)art::RoomStyle::COUNT; s++) {
    art::RoomStyle rs = (art::RoomStyle)s;
    bool stone = rs == art::RoomStyle::Stone || rs == art::RoomStyle::Hall || rs == art::RoomStyle::Soot || rs == art::RoomStyle::Arcane ||
                 rs == art::RoomStyle::Adobe;
    Map m = contractMap(rs, stone);
    Canvas open = composeRoom(m, false, {{3, 8}, {9, 10}, {12, 14}});
    Canvas shut = composeRoom(m, true, {});
    Canvas both(open.w * 2 + 8, open.h);
    for (auto& p : both.px) p = rgba(10, 9, 13);
    blitA(both, open, 0, 0);
    blitA(both, shut, open.w + 8, 0);
    label(both, 2, 2, std::string(styleName(rs)) + " (open / shut)");
    savePng(both, dir + "/contract_" + styleName(rs) + ".png", 4);
    all.push_back(open);
  }
  int cw = all[0].w, ch = all[0].h;
  Canvas sheet(cw * 4 + 24, ch * 2 + 8);
  for (auto& p : sheet.px) p = rgba(10, 9, 13);
  for (size_t i = 0; i < all.size(); i++) {
    blitA(sheet, all[i], (int)(i % 4) * (cw + 8), (int)(i / 4) * (ch + 8));
    label(sheet, (int)(i % 4) * (cw + 8) + 2, (int)(i / 4) * (ch + 8) + 2, styleName((art::RoomStyle)i));
  }
  savePng(sheet, dir + "/contract_1x.png", 1);
}

const char* typeName(art::Building t) {
  switch (t) {
    case art::Building::Inn: return "inn";
    case art::Building::Keep: return "keep";
    case art::Building::Tower: return "tower";
    case art::Building::Temple: return "temple";
    case art::Building::Shop: return "shop";
    case art::Building::Smithy: return "smithy";
    case art::Building::House: return "house";
    case art::Building::StoneHouse: return "stonehouse";
    case art::Building::Farmhouse: return "farmhouse";
    case art::Building::Hut: return "hut";
    case art::Building::Palace: return "palace";
    case art::Building::Barracks: return "barracks";
    default: return "bldg";
  }
}

void real(const std::string& dir, uint64_t seed, std::vector<Canvas>& firsts) {
  World w;
  w.generateEndless(seed);   // (M2: the start window of the endless world)
  // one building of each type (preferring ones with upper floors), every floor side by side
  int n = 0;
  // M1: the palace and the barracks stand only in endless capitals: built here from their own facts
  std::vector<Bldg> extra;
  for (int k = 0; k < 2; k++) {
    Bldg b;
    b.type = k == 0 ? art::Building::Palace : art::Building::Barracks;
    b.r = k == 0 ? IRect{0, 0, 15 + 2 * (int)(seed & 1), 7} : IRect{0, 0, 7, 4};
    b.owner = k == 0 ? Role::King : Role::Guard;
    b.storeys = 2;
    b.biome = Biome::Plains;
    b.seed = (uint32_t)(seed * 2654435761u) + (uint32_t)k * 977u;
    extra.push_back(b);
  }
  for (art::Building t : {art::Building::Inn, art::Building::Keep, art::Building::Tower, art::Building::House, art::Building::StoneHouse,
                          art::Building::Shop, art::Building::Smithy, art::Building::Temple, art::Building::Farmhouse, art::Building::Hut,
                          art::Building::Palace, art::Building::Barracks}) {
    const Bldg* best = nullptr;
    for (const Bldg& b : w.over.bldgs)
      if (b.type == t && (!best || b.floors() > best->floors())) best = &b;
    for (const Bldg& b : extra)
      if (b.type == t) best = &b;
    if (!best) continue;
    std::vector<Canvas> fl;
    for (int f = 0; f < best->floors(); f++) {
      Map m;
      genInterior(m, *best, best->seed, f);
      fl.push_back(composeRoom(m, false, {}));
    }
    int W = 0, H = 0;
    for (auto& c : fl) { W += c.w + 8; H = std::max(H, c.h); }
    Canvas row(W, H);
    for (auto& p : row.px) p = rgba(10, 9, 13);
    int x = 0;
    for (size_t i = 0; i < fl.size(); i++) {
      blitA(row, fl[i], x, 0);
      label(row, x + 2, 2, std::string(typeName(t)) + " floor " + std::to_string(i));
      x += fl[i].w + 8;
    }
    savePng(row, dir + "/real_" + std::to_string(seed) + "_" + std::to_string(n++) + "_" + typeName(t) + ".png", 3);
    firsts.push_back(row);
  }
}
}  // namespace

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : ".";
  bool onlyContract = argc > 2 && std::string(argv[2]) == "contract";
  contract(dir);
  if (onlyContract) return 0;
  std::vector<Canvas> rows;
  for (uint64_t seed : {7ull, 42ull}) real(dir, seed, rows);
  int W = 0, H = 0;
  for (auto& r : rows) { W = std::max(W, r.w); H += r.h + 8; }
  Canvas sheet(W, H);
  for (auto& p : sheet.px) p = rgba(10, 9, 13);
  int y = 0;
  for (auto& r : rows) { blitA(sheet, r, 0, y); y += r.h + 8; }
  savePng(sheet, dir + "/real_1x.png", 1);
  return 0;
}
