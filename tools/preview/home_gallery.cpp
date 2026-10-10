// Home gallery (M7 "Home", ART lane): the farm's art (rpg/art/art_home.h) on boards, each written at 1x (*_1x.png)
// and zoomed (3x):
//   home_gallery <outDir>
//     crops.png        every crop x 4 stages, then wilted, then a patch of ripe field (variants) on joined farmland
//     ground.png       farmland and paths: every join mask, dry and watered; assembled beds and paths in every paving
//     objects_<n>.png  every yard object (and its turned form) in the classic look and the cultures, on grass
//     fences.png       fence and gate join patterns: every mask, rings with gates, T and cross joins, staircases
//     scaffold.png     the building site at its 4 stages for every shell footprint
//     horses.png       horse sheets (breed coats and variants) and the rider astride in 3 facings
//     beasts.png       the farm critters (cow, sheep, horse) among people
//     decor.png        wall trophies per monster, paintings (6 kinds), the FOR SALE sign per culture
//     icons.png        the 16 farm icons, plain and tinted
//   home_gallery --check   sizes, anchors, determinism, the rider on the horse; non-zero on failure
#include <initializer_list>

#include "rpg/culture/culture.h"
#include "tools/preview/preview_util.h"

namespace {

int g_fail = 0;
void fail(const std::string& s) { std::printf("FAIL %s\n", s.c_str()); g_fail++; }

const char* const kCropName[] = {"WHEAT", "BARLEY", "OATS", "RYE", "POTATO", "TURNIP", "CABBAGE", "CARROT", "ONION", "FLAX",
                                 "BEANS", "GRAPES", "DATES", "MAIZE", "TEA", "RICE", "HERBS"};
static_assert(sizeof(kCropName) / sizeof(kCropName[0]) == (size_t)art::Crop::COUNT, "crop names");
const char* const kObjName[] = {"FENCE", "GATE", "WELL", "WOODPILE", "BEEHIVE", "SCARECROW", "COOP", "PEN", "STABLE", "TROUGH",
                                "WORKBENCH", "FORGE", "FLOWERBED", "SAPLING", "BENCH", "LANTERN", "STATUE", "BANNER",
                                "CAMPFIRE", "DOGHOUSE", "HAYRACK", "CRATE"};
static_assert(sizeof(kObjName) / sizeof(kObjName[0]) == (size_t)art::FarmObj::COUNT, "object names");
// the footprint of each object in tiles (home::objInfo; the gallery has no sim)
void footprint(art::FarmObj k, bool turned, int& w, int& h) {
  w = h = 1;
  switch (k) {
    case art::FarmObj::Well: case art::FarmObj::Coop: case art::FarmObj::Forge: w = h = 2; break;
    case art::FarmObj::Woodpile: case art::FarmObj::Trough: case art::FarmObj::Workbench: case art::FarmObj::Bench:
    case art::FarmObj::HayRack: w = 2; break;
    case art::FarmObj::Pen: w = 3; h = 2; break;
    case art::FarmObj::Stable: w = h = 3; break;
    default: break;
  }
  if (turned) std::swap(w, h);
}
bool rotates(art::FarmObj k) {
  return k == art::FarmObj::Gate || k == art::FarmObj::Woodpile || k == art::FarmObj::Trough || k == art::FarmObj::Workbench ||
         k == art::FarmObj::Bench || k == art::FarmObj::HayRack;
}

Canvas cellOf(const Canvas& sheet, int col, int row, int w, int h) {
  Canvas c(w, h);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) c.set(x, y, sheet.get(col * w + x, row * h + y));
  return c;
}
void blitOver(Canvas& dst, const Canvas& src, int x0, int y0, bool flip = false) {
  for (int y = 0; y < src.h; y++)
    for (int x = 0; x < src.w; x++) {
      const uint32_t p = src.get(flip ? src.w - 1 - x : x, y);
      const int a = (int)(p >> 24);
      if (!a) continue;
      if (a == 255) { dst.set(x0 + x, y0 + y, p); continue; }
      const uint32_t d = dst.get(x0 + x, y0 + y);
      dst.set(x0 + x, y0 + y, art::mix(d | 0xFF000000u, p | 0xFF000000u, a / 255.0f));
    }
}
void shadowAt(Board& b, int cx, int cy, int rx, int ry) {
  for (int y = cy - ry; y <= cy + ry; y++)
    for (int x = cx - rx; x <= cx + rx; x++) {
      const float d = ((x - cx) * (x - cx)) / (float)(rx * rx) + ((y - cy) * (y - cy)) / (float)(ry * ry);
      if (d <= 1.0f) b.c.set(x, y, art::shade(b.c.get(x, y), 0.72f));
    }
}
void save(const Board& b, const std::string& dir, const std::string& name) {
  savePng(b.c, dir + "/" + name + ".png", 3);
  savePng(b.c, dir + "/" + name + "_1x.png", 1);
}

// the culture styles: the classic look (nullptr) and every archetype's house style
struct Style { std::string name; art::ArchStyle st; bool classic; };
std::vector<Style> styles() {
  std::vector<Style> v;
  v.push_back({"CLASSIC", art::ArchStyle{}, true});
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++) {
    const cult::Culture C = cult::Atlas::make((cult::Archetype)a, 1234u + (uint32_t)a * 7919u);
    art::ArchStyle st = cult::buildingArch(C, 2, 0, 1, 99u + (uint32_t)a);
    v.push_back({std::string(cult::archetypeName((cult::Archetype)a)).substr(0, 8), st, false});
  }
  return v;
}

// ---------------------------------------------------------------- crops
void crops(const std::string& dir) {
  const int n = (int)art::Crop::COUNT;
  const int rowH = 40;
  Board b(70 + 8 * 18 + 12 + 5 * 16 + 12, 10 + n * rowH);
  b.text(70, 2, "STAGE 0-3");
  b.text(70 + 4 * 18, 2, "WILTED");
  b.text(70 + 8 * 18 + 12, 2, "RIPE FIELD");
  for (int k = 0; k < n; k++) {
    const art::Crop c = (art::Crop)k;
    const int y = 10 + k * rowH;
    b.text(2, y + 18, kCropName[k]);
    for (int st = 0; st < 4; st++) {
      b.put(art::farmlandTile(15, false, 0), 70 + st * 18, y + 16 + 4);
      b.put(art::cropSprite(c, st, (uint32_t)k, false), 70 + st * 18, y + 4);
      b.put(art::farmlandTile(15, false, 1), 70 + (4 + st) * 18, y + 16 + 4);
      b.put(art::cropSprite(c, st, (uint32_t)k, true), 70 + (4 + st) * 18, y + 4);
    }
    // a 5 x 1 strip of ripe field, joined farmland, variants 0..4 (rows overlap upward like the game draws them)
    const int fx = 70 + 8 * 18 + 12;
    for (int i = 0; i < 5; i++) {
      const uint8_t j = (uint8_t)((i > 0 ? 8 : 0) | (i < 4 ? 2 : 0));
      b.put(art::farmlandTile(j, (i & 1) != 0, (uint32_t)i), fx + i * 16, y + 20);
    }
    for (int i = 0; i < 5; i++) b.put(art::cropSprite(c, 3, (uint32_t)(i * 3 + k), false), fx + i * 16, y + 4);
  }
  save(b, dir, "crops");
  // a mixed field in the game's draw order: 8 x 6 tiles, a crop per row, the stages across, rows drawn north to south
  {
    Board f(16 * 10 + 20, 16 * 9 + 40);
    const int W = 8, H = 7, ox = 10, oy = 30;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        uint8_t j = 0;
        if (y > 0) j |= 1;
        if (x < W - 1) j |= 2;
        if (y < H - 1) j |= 4;
        if (x > 0) j |= 8;
        f.put(art::farmlandTile(j, x >= 6, (uint32_t)((x * 7 + y * 13) & 3)), ox + x * 16, oy + y * 16);
      }
    static const art::Crop rows[7] = {art::Crop::Maize, art::Crop::Wheat, art::Crop::Barley, art::Crop::Cabbage, art::Crop::Carrot, art::Crop::Grapes, art::Crop::Rice};
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++)
        f.put(art::cropSprite(rows[y], std::min(3, x / 2), (uint32_t)((x * 31 + y * 17) & 7), x == 7 && y == 2), ox + x * 16, oy + y * 16 + 16 - art::CROP_H);
    save(f, dir, "field");
  }
}

// ---------------------------------------------------------------- ground
void ground(const std::string& dir) {
  Board b(16 * 20 + 40, 400);
  b.text(2, 2, "FARMLAND JOINS DRY / WET");
  for (int m = 0; m < 16; m++) {
    b.put(art::farmlandTile((uint8_t)m, false, (uint32_t)m), 4 + m * 20, 12);
    b.put(art::farmlandTile((uint8_t)m, true, (uint32_t)m), 4 + m * 20, 32);
  }
  // assembled shapes from a small map: '#' = the run
  auto shape = [&](const std::vector<std::string>& M, int ox, int oy, int kind, int style) {
    for (int y = 0; y < (int)M.size(); y++)
      for (int x = 0; x < (int)M[y].size(); x++) {
        if (M[y][x] != '#' && M[y][x] != 'w') continue;
        auto at = [&](int xx, int yy) { return yy >= 0 && yy < (int)M.size() && xx >= 0 && xx < (int)M[yy].size() && (M[yy][xx] == '#' || M[yy][xx] == 'w'); };
        uint8_t j = 0;
        if (at(x, y - 1)) j |= 1;
        if (at(x + 1, y)) j |= 2;
        if (at(x, y + 1)) j |= 4;
        if (at(x - 1, y)) j |= 8;
        const uint32_t v = (uint32_t)((x * 7 + y * 13) & 3);
        b.put(kind == 0 ? art::farmlandTile(j, M[y][x] == 'w', v) : art::pathTile(j, (uint8_t)style, v), ox + x * 16, oy + y * 16);
      }
  };
  const std::vector<std::string> bed = {"#####.", "##ww#.", "##ww##", "....##"};
  shape(bed, 4, 60, 0, 0);
  const std::vector<std::string> path = {"..#..", "#####", "..#..", "..###", "....#"};
  for (int s = 0; s < 16; s++) {
    const int col = s % 4, row = s / 4;
    shape(path, 120 + col * 84 - (col ? 0 : 0), 52 + row * 86, 1, s);
    b.text(120 + col * 84, 52 + row * 86 + 80 - 2, "P" + std::to_string(s));
  }
  save(b, dir, "ground");
}

// ---------------------------------------------------------------- yard objects
void objects(const std::string& dir) {
  const std::vector<Style> S = styles();
  // one board per group of styles, so each reads at 3x
  const int perBoard = 4;
  for (size_t b0 = 0; b0 < S.size(); b0 += perBoard) {
    // columns: every object (and its turned form when it rotates)
    struct Col { art::FarmObj k; bool turned; uint8_t stage; };
    std::vector<Col> cols;
    for (int k = 0; k < (int)art::FarmObj::COUNT; k++) {
      const art::FarmObj fk = (art::FarmObj)k;
      if (fk == art::FarmObj::Fence) continue;   // (fences.png)
      cols.push_back({fk, false, (uint8_t)(fk == art::FarmObj::Sapling ? 3 : 2)});
      if (rotates(fk) && fk != art::FarmObj::Gate) cols.push_back({fk, true, 2});
    }
    // two rows of columns per style
    const int half = ((int)cols.size() + 1) / 2;
    int colW = 56;
    Board b(10 + half * colW, 10 + perBoard * 2 * 76);
    for (size_t si = b0; si < std::min(S.size(), b0 + perBoard); si++) {
      for (int r = 0; r < 2; r++) {
        const int y0 = 10 + (int)(si - b0) * 152 + r * 76;
        b.text(4, y0 - 8 + 2, S[si].name);
        for (int ci = r * half; ci < std::min((int)cols.size(), (r + 1) * half); ci++) {
          const Col& c = cols[(size_t)ci];
          art::FarmObjLook L;
          L.kind = c.k; L.turned = c.turned; L.stage = c.stage; L.variant = (uint32_t)ci;
          L.style = S[si].classic ? nullptr : &S[si].st;
          L.banner = rgba(40, 70, 150); L.banner2 = rgba(230, 200, 90); L.emblem = 3;
          if (c.k == art::FarmObj::Gate) L.joins = 10;
          int fw, fh;
          footprint(c.k, c.turned, fw, fh);
          const Canvas s = art::farmObjSprite(L);
          const int x0 = 6 + (ci - r * half) * colW, bottom = y0 + 70;
          // the footprint on the ground (a faint darker patch), the sprite bottom-centre on it
          const int fx = x0 + colW / 2 - fw * 8;
          for (int yy = bottom - fh * 16; yy < bottom; yy++)
            for (int xx = fx; xx < fx + fw * 16; xx++) if (((xx + yy) & 3) == 0) b.c.set(xx, yy, art::shade(b.c.get(xx, yy), 0.9f));
          b.put(s, fx + fw * 8 - s.w / 2, bottom - s.h);
          if (si == b0 && r == 0) {}
        }
      }
    }
    save(b, dir, "objects_" + std::to_string(b0 / perBoard));
  }
  // stage rows: sapling 0..3, coop / pen / stable 0..3 animals
  {
    Board b(10 + 4 * 4 * 54, 90);
    int x = 6;
    for (art::FarmObj k : {art::FarmObj::Sapling, art::FarmObj::Coop, art::FarmObj::Pen, art::FarmObj::Stable})
      for (int st = 0; st < 4; st++) {
        art::FarmObjLook L;
        L.kind = k; L.stage = (uint8_t)st; L.variant = (uint32_t)st;
        const Canvas s = art::farmObjSprite(L);
        b.put(s, x + 27 - s.w / 2, 84 - s.h);
        x += 54;
      }
    save(b, dir, "objects_stages");
  }
}

// ---------------------------------------------------------------- fences
void fences(const std::string& dir) {
  const std::vector<Style> S = styles();
  Board b(16 * 34 + 20, 10 + (int)S.size() * 0 + 1000);
  // every join mask for the fence and the gate, in a few cultures
  int y = 12;
  for (size_t si = 0; si < S.size(); si += 3) {
    b.text(4, y - 10, S[si].name);
    for (int m = 0; m < 16; m++) {
      art::FarmObjLook L;
      L.kind = art::FarmObj::Fence; L.joins = (uint8_t)m; L.style = S[si].classic ? nullptr : &S[si].st;
      const Canvas s = art::farmObjSprite(L);
      b.put(s, 6 + m * 22 + 8 - s.w / 2, y + 30 - s.h);
    }
    y += 40;
  }
  // assembled layouts from maps: 'F' fence, 'G' gate (a run decides its axis), '.' grass
  auto layout = [&](const std::vector<std::string>& M, int ox, int oy, const Style& st) {
    auto at = [&](int x, int yy) { return yy >= 0 && yy < (int)M.size() && x >= 0 && x < (int)M[yy].size() && (M[yy][x] == 'F' || M[yy][x] == 'G'); };
    for (int yy = 0; yy < (int)M.size(); yy++)
      for (int x = 0; x < (int)M[yy].size(); x++) {
        if (!at(x, yy)) continue;
        art::FarmObjLook L;
        L.kind = M[yy][x] == 'G' ? art::FarmObj::Gate : art::FarmObj::Fence;
        if (at(x, yy - 1)) L.joins |= 1;
        if (at(x + 1, yy)) L.joins |= 2;
        if (at(x, yy + 1)) L.joins |= 4;
        if (at(x - 1, yy)) L.joins |= 8;
        L.turned = L.kind == art::FarmObj::Gate && (L.joins & 5) && !(L.joins & 10);
        L.variant = (uint32_t)(x * 7 + yy * 13) & 3u;
        L.style = st.classic ? nullptr : &st.st;
        const Canvas s = art::farmObjSprite(L);
        b.put(s, ox + x * 16 + 8 - s.w / 2, oy + (yy + 1) * 16 - s.h);
      }
  };
  const std::vector<std::string> ring = {"FFFFFF.", "F....F.", "F....FF", "F.....F", "FFGFFFF"};
  const std::vector<std::string> cross = {"..F..", "..F..", "FFFFF", "..G..", "..F.."};
  const std::vector<std::string> stair = {"FF...", ".FF..", "..FF.", "...FF", "....F"};
  const std::vector<std::string> tee = {"FFGFF", "..F..", "..F..", "FFF..", "..G.."};
  for (size_t si = 0; si < S.size(); si++) {
    const int col = (int)(si % 4), row = (int)(si / 4);
    const int ox = 8 + col * 136, oy = y + row * 100;
    if (si >= 12) break;
    b.text(ox, oy, S[si].name);
    layout(si % 3 == 0 ? ring : (si % 3 == 1 ? tee : cross), ox, oy + 10, S[si]);
    layout(stair, ox + 70, oy + 10, S[si]);
  }
  save(b, dir, "fences");
}

// ---------------------------------------------------------------- scaffold
void scaffold(const std::string& dir) {
  struct Shell { const char* n; int w, h, storeys; };
  static const Shell shells[] = {{"HUT", 4, 3, 1}, {"COTTAGE", 5, 4, 1}, {"LONGHOUSE", 8, 4, 1}, {"TOWNHOUSE", 5, 4, 2}, {"HALL", 7, 5, 2}};
  int H = 10;
  for (const Shell& s : shells) H += s.h * 16 + 16 * s.storeys + 30;
  Board b(10 + 4 * (8 * 16 + 12), H);
  int y = 10;
  for (const Shell& s : shells) {
    b.text(4, y, s.n);
    int maxH = 0;
    for (int p = 0; p < 4; p++) {
      const Canvas c = art::scaffoldSprite(s.w, s.h, s.storeys, p, 77u);
      maxH = std::max(maxH, c.h);
    }
    for (int p = 0; p < 4; p++) {
      const Canvas c = art::scaffoldSprite(s.w, s.h, s.storeys, p, 77u);
      const int x0 = 6 + p * (8 * 16 + 12), bottom = y + 10 + maxH;
      for (int yy = bottom - s.h * 16; yy < bottom; yy++)
        for (int xx = x0; xx < x0 + s.w * 16; xx++) if (((xx + yy) & 3) == 0) b.c.set(xx, yy, art::shade(b.c.get(xx, yy), 0.9f));
      b.put(c, x0, bottom - c.h);
    }
    y += maxH + 22;
  }
  save(b, dir, "scaffold");
}

// ---------------------------------------------------------------- horses and riders
art::HumanLook riderLook(int i) {
  art::HumanLook L;
  L.skin = rgba(226, 176, 132); L.hairColor = rgba(96, 60, 34); L.topColor = rgba(70, 110, 160); L.bottomColor = rgba(84, 66, 50);
  if (i == 1) { L.outfit = art::Outfit::Robe; L.topColor = rgba(150, 40, 60); L.hair = art::Hair::Long; }
  if (i == 2) { L.outfit = art::Outfit::Guard; L.helmet = true; L.tabardColor = rgba(40, 70, 150); L.cloak = 1; L.cloakColor = rgba(120, 40, 40); }
  if (i == 3) { L.outfit = art::Outfit::Dress; L.hair = art::Hair::Long; L.topColor = rgba(60, 120, 80); }
  return L;
}
// the view's drawMounted composition (rpg/view/home_view.cpp): the horse cell's bottom-centre at (fx, fy + 2), the
// rider's Ride cell at riderSeat; the rider before the horse facing up, after it otherwise
void mounted(Canvas& dst, const Canvas& horse, const Canvas& rider, int fx, int fy, int row, int frame, bool flip) {
  const int hx = fx - art::HORSE_W / 2, hy = fy - art::HORSE_H + 2;
  int dx = 0, dy = 0;
  art::riderSeat(row, frame, dx, dy);
  if (flip) dx = -dx;
  const int rx = fx + dx - art::HUMAN_W / 2, ry = fy + dy - art::HUMAN_H + 2;
  const Canvas hc = cellOf(horse, frame, row, art::HORSE_W, art::HORSE_H);
  const Canvas rc = cellOf(rider, 0, row, art::HUMAN_W, art::HUMAN_H);
  if (row == 1) { blitOver(dst, rc, rx, ry, flip); blitOver(dst, hc, hx, hy, flip); }
  else { blitOver(dst, hc, hx, hy, flip); blitOver(dst, rc, rx, ry, flip); }
}
void horses(const std::string& dir) {
  static const uint32_t coats[6] = {rgba(140, 90, 48), rgba(180, 134, 78), rgba(90, 60, 36), rgba(180, 180, 176), rgba(216, 196, 140), rgba(56, 48, 40)};
  static const char* names[6] = {"BAY", "DUN", "CHESTNUT", "GREY", "FJORD", "BLACK"};
  Board b(70 + 8 * 38, 10 + 6 * 2 * (art::HORSE_H * 3 + 6));
  int y = 6;
  for (int i = 0; i < 6; i++)
    for (int sad = 1; sad >= 0; sad--) {
      b.text(2, y + 20, names[i]);
      if (!sad) b.text(2, y + 30, "BARE");
      const Canvas sh = art::horseSheet(coats[i], (uint32_t)(i * 37 + 5), sad != 0);
      for (int row = 0; row < 3; row++)
        for (int f = 0; f < art::MOUNT_FRAMES; f++) b.put(cellOf(sh, f, row, art::HORSE_W, art::HORSE_H), 70 + f * 38, y + row * art::HORSE_H);
      y += art::HORSE_H * 3 + 6;
    }
  save(b, dir, "horses");
  // riders astride: 4 looks x 3 facings (and the left flip) x the walk frames
  Board r(20 + 4 * 5 * 40, 20 + 4 * 4 * 46);
  for (int li = 0; li < 4; li++) {
    const art::HumanLook L = riderLook(li);
    const Canvas rider = art::humanPostureSheet(L, art::Posture::Ride);
    const Canvas horse = art::horseSheet(coats[li], (uint32_t)(li * 11), true);
    for (int face = 0; face < 4; face++) {
      const int row = face == 0 ? 0 : (face == 1 ? 1 : 2);
      const bool flip = face == 3;
      for (int f = 0; f < 5; f++) mounted(r.c, horse, rider, 30 + f * 40, 50 + (li * 4 + face) * 46, row, f, flip);
    }
  }
  save(r, dir, "riders");
}

// ---------------------------------------------------------------- the farm critters among people
void beasts(const std::string& dir) {
  Board b(10 + 8 * 40, 10 + 3 * 4 * 38 + 40);
  int y = 6;
  for (art::Critter cr : {art::Critter::Cow, art::Critter::Sheep, art::Critter::Horse}) {
    const int cw = art::critterCellW(cr), ch = art::critterCellH(cr);
    for (int v = 0; v < 4; v++) {
      const Canvas sh = art::critterSheet(cr, cr == art::Critter::Sheep && v == 3 ? 128u + 1u : (uint32_t)v);
      for (int f = 0; f < 8; f++) b.put(cellOf(sh, f, 2, cw, ch), 6 + f * 40, y);
      y += ch + 2;
    }
    y += 6;
  }
  save(b, dir, "beasts");
  Board s(400, 70);
  int x = 6;
  const Canvas hs = art::humanSheet(riderLook(0));
  for (art::Critter cr : {art::Critter::Chicken, art::Critter::Goat, art::Critter::Pig, art::Critter::Sheep, art::Critter::Cow, art::Critter::Horse, art::Critter::Dog}) {
    const int cw = art::critterCellW(cr), ch = art::critterCellH(cr);
    for (int row = 0; row < 3; row++) {
      const Canvas sh = art::critterSheet(cr, 1);
      shadowAt(s, x + cw / 2, 60 - 1 - row * 0, cw / 3, 1);
      s.put(cellOf(sh, 0, row, cw, ch), x, 62 - ch);
      x += cw;
      if (row == 2) x += 4;
    }
    if (cr == art::Critter::Pig) { s.put(cellOf(hs, 0, 0, 16, 24), x, 62 - 24); x += 18; }
  }
  save(s, dir, "beasts_scale");
}

// ---------------------------------------------------------------- trophies, paintings, the sign, icons
void decor(const std::string& dir) {
  const int nm = (int)art::Monster::COUNT;
  Board b(10 + 14 * 24, 30 + ((nm + 13) / 14) * 30 + 6 * 30 + 12 * 34 + 40);
  for (int m = 0; m < nm; m++) b.put(art::trophySprite((art::Monster)m, (uint32_t)m), 6 + (m % 14) * 24, 6 + (m / 14) * 30);
  int y = 6 + ((nm + 13) / 14) * 30 + 6;
  static const uint32_t skies[4] = {rgba(120, 160, 210), rgba(230, 170, 120), rgba(90, 110, 150), rgba(200, 210, 230)};
  static const uint32_t lands[4] = {rgba(80, 130, 70), rgba(196, 160, 100), rgba(70, 90, 80), rgba(150, 160, 120)};
  for (int k = 0; k < 6; k++) {
    for (int i = 0; i < 4; i++) {
      art::PaintingSpec ps;
      ps.kind = (uint8_t)k; ps.sky = skies[i]; ps.land = lands[i]; ps.accent = i == 1 ? rgba(200, 60, 50) : 0; ps.seed = (uint32_t)(k * 31 + i * 7 + 1);
      b.put(art::paintingSprite(ps), 6 + i * 36, y);
    }
    y += 28;
  }
  // the FOR SALE sign: classic and every culture (paintHomeProp has no style: the classic sign) and icons
  y += 4;
  b.put(art::propSprite(art::Prop::ForSaleSign), 160, 6 + ((nm + 13) / 14) * 30 + 6);
  static const art::Icon icons[16] = {art::Icon::Seeds, art::Icon::Sheaf, art::Icon::Veg, art::Icon::Fruit, art::Icon::Egg, art::Icon::Milk,
                                      art::Icon::Wool, art::Icon::Hay, art::Icon::Flour, art::Icon::Honey, art::Icon::Meal, art::Icon::Hoe,
                                      art::Icon::WateringCan, art::Icon::Sickle, art::Icon::Brush, art::Icon::Deed};
  static const uint32_t tints[4] = {0, rgba(214, 160, 60), rgba(150, 50, 150), rgba(230, 110, 40)};
  for (int t = 0; t < 4; t++)
    for (int i = 0; i < 16; i++) b.put(art::itemIcon(icons[i], tints[t]), 6 + i * 20, y + t * 20);
  save(b, dir, "decor");
}

// ================================================================ --check
void check() {
  // crops: the canvas, something painted, standing on the soil row band, deterministic
  for (int k = 0; k < (int)art::Crop::COUNT; k++)
    for (int st = 0; st < 4; st++)
      for (int w = 0; w < 2; w++)
        for (uint32_t v = 0; v < 8; v++) {
          const Canvas c = art::cropSprite((art::Crop)k, st, v, w != 0);
          if (c.w != art::CROP_W || c.h != art::CROP_H) fail("crop size");
          int n = 0, lowest = -1;
          for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) if (c.get(x, y) >> 24) { n++; lowest = y; }
          if (n < 4) fail(std::string(kCropName[k]) + " stage " + std::to_string(st) + ": empty");
          if (lowest < 24) fail(std::string(kCropName[k]) + " stage " + std::to_string(st) + ": floats above the soil (" + std::to_string(lowest) + ")");
          if (art::cropSprite((art::Crop)k, st, v, w != 0).px != c.px) fail("crop not deterministic");
        }
  // objects: the size functions agree with the sprite, and the sprite reaches its canvas bottom (it stands)
  for (int k = 0; k < (int)art::FarmObj::COUNT; k++)
    for (int t = 0; t < 2; t++)
      for (int j = 0; j < 16; j++) {
        art::FarmObjLook L;
        L.kind = (art::FarmObj)k; L.turned = t != 0; L.joins = (uint8_t)j;
        const Canvas c = art::farmObjSprite(L);
        if (c.w != art::farmObjW(L) || c.h != art::farmObjH(L)) fail(std::string(kObjName[k]) + ": size functions disagree");
        int lowest = -1;
        for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) if (c.get(x, y) >> 24) lowest = y;
        if (lowest < c.h - 9 && j == 0 && t == 0) fail(std::string(kObjName[k]) + ": does not stand in its footprint (" + std::to_string(lowest) + "/" + std::to_string(c.h) + ")");
        if (art::farmObjSprite(L).px != c.px) fail("object not deterministic");
      }
  // the scaffold: the canvas is the footprint's width
  for (int p = 0; p < 4; p++) {
    const Canvas c = art::scaffoldSprite(5, 4, 1, p, 3u);
    if (c.w != 5 * 16) fail("scaffold width");
  }
  // the horse: sheet size; hooves on row HORSE_H - 2 in every standing frame
  const Canvas hs = art::horseSheet(rgba(140, 90, 48), 3u, true);
  if (hs.w != art::HORSE_W * art::MOUNT_FRAMES || hs.h != art::HORSE_H * 4) fail("horse sheet size");
  for (int row = 0; row < 3; row++)
    for (int f = 0; f < art::MOUNT_FRAMES; f++) {
      const Canvas c = cellOf(hs, f, row, art::HORSE_W, art::HORSE_H);
      int lowest = -1, x0 = 99, x1 = -1, y0 = 99;
      for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) if (c.get(x, y) >> 24) { lowest = y; x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); }
      if (lowest < 0) { fail("horse cell empty"); continue; }
      if (x0 < 0 || x1 > art::HORSE_W - 1 || y0 < 0) fail("horse leaves its cell");
      if (f <= 4 && (lowest < art::HORSE_H - 2 || lowest > art::HORSE_H - 1)) fail("horse row " + std::to_string(row) + " f" + std::to_string(f) + ": hooves not on the ground (" + std::to_string(lowest) + ")");
    }
  if (art::horseSheet(rgba(140, 90, 48), 3u, true).px != hs.px) fail("horse not deterministic");
  std::printf("home_gallery --check: %d failures\n", g_fail);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && std::string(argv[1]) == "--check") { check(); return g_fail ? 1 : 0; }
  const std::string dir = argc >= 2 ? argv[1] : ".";
  const std::string only = argc >= 3 ? argv[2] : "";
  auto want = [&](const char* n) { return only.empty() || only == n; };
  if (want("crops")) crops(dir);
  if (want("ground")) ground(dir);
  if (want("objects")) objects(dir);
  if (want("fences")) fences(dir);
  if (want("scaffold")) scaffold(dir);
  if (want("horses")) horses(dir);
  if (want("beasts")) beasts(dir);
  if (want("decor")) decor(dir);
  return 0;
}
