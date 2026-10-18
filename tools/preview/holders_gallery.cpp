// holders_gallery (P2a HOLDERS; skeleton by the phase A lead, handed to HOLDERS): everything a body uses, at rig 1 and at
// rig 2 side by side (STYLE_P2 1, 16), each beside the figure of its rig (the v1 hero at rig 1, the v2 slice hero at rig
// 2) standing on the same floor row. Every board at 1x and 4x (<name>_1x.png, <name>.png).
//   holders_gallery <outDir> [board...]   boards: cottage furniture beds outdoor mounts carts boats gates graves (default: all)
//     holders_gates.png      every gate form (and holders_gates_capital.png at the capitals' height), the figure in
//                            its mouth, red ticks at the 40 px clear height; holders_spans.png a wall's span arch
//     holders_boats_moored.png  the hulls at four headings, sails furled, the figure aboard between the back and front
//                            layers (gold tick: the rowboat's thwart seat line); holders_boats_layers.png the split
//     holders_cottage.png    the start village's cottage kinds at rig 1 and rig 2
//     holders_furniture.png  chairs, stools, benches, the throne, tables and counters
//     holders_beds.png       the classic, long and bunk beds
//     holders_outdoor.png    fences, garden walls, hedges, carts, the ladder, graves
//   holders_metrics.txt      holderPx for every holder at rig 1 and rig 2, and the ratio to the 32 px figure (H2)
#include <cstring>
#include <functional>
#include "tools/preview/rig_preview.h"
#include "rpg/build/blueprint.h"
#include "rpg/art/art_parts.h"
#include "rpg/art/art_holders2.h"

namespace {
const uint32_t kFloorA = rgba(114, 86, 68), kFloorB = rgba(104, 78, 62), kFloorSeam = rgba(82, 60, 54);

// the figure of the current rig, standing (down idle), with the row its soles stand on
Canvas figureOf(int rv, int& soleRow) {
  if (rv >= 2) {
    const art::RigSheet s = art::humanRig(rigpv::sliceHero(false), art::RigPart::Loco);
    int px, py;
    Canvas f = rigpv::frameCanvas(s, 0, art::Clip::Idle, 0, 0, px, py);
    soleRow = py;
    Canvas out(f.w + 2, f.h + 2);
    for (int y = 0; y < f.h; y++) for (int x = 0; x < f.w; x++) out.set(x + 1, y + 1, f.get(x, y));
    soleRow = py + 1;
    return out;
  }
  const Canvas hero = art::humanSheet(rigpv::sliceHero(false));
  Canvas cell(art::HUMAN_W, art::HUMAN_H);
  for (int yy = 0; yy < art::HUMAN_H; yy++) for (int xx = 0; xx < art::HUMAN_W; xx++) cell.set(xx, yy, hero.get(xx, yy));
  int last = 0;
  for (int yy = 0; yy < cell.h; yy++) for (int xx = 0; xx < cell.w; xx++) if (cell.get(xx, yy) >> 24) last = yy;
  soleRow = last;
  return cell;
}

void floorStrip(Board& b, int x0, int y0, int x1, int y1) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) b.c.set(x, y, (y % 4 == 3) ? kFloorSeam : (((x / 12 + y / 4) & 1) ? kFloorA : kFloorB));
}

struct Item { const char* label; std::function<Canvas()> make; };

// one row of items, each at rig 1 then rig 2 with the rig's figure at its right; the floor row is floorY
void itemRow(Board& b, int x, int floorY, const std::vector<Item>& items, bool floor) {
  if (floor) floorStrip(b, 0, floorY - 70, b.c.w - 1, floorY + 6);
  for (const Item& it : items) {
    const int x0 = x;
    for (int rv = 1; rv <= 2; rv++) {
      art::setRigVersion(rv);
      const Canvas c = it.make();
      b.put(c, x, floorY - c.h + 1);
      int sole = 0;
      const Canvas fig = figureOf(rv, sole);
      b.put(fig, x + c.w + 1, floorY - sole);
      x += c.w + fig.w + 6;
    }
    art::setRigVersion(1);
    b.text(x0, floorY + 8, it.label);
    x += 10;
  }
}

Canvas prop(art::Prop p) { return art::propSprite(p); }
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::printf("usage: holders_gallery <outDir> [cottage|furniture|beds|outdoor ...]\n"); return 2; }
  const std::string dir = argv[1];
  auto want = [&](const char* n) {
    if (argc <= 2) return true;
    for (int i = 2; i < argc; i++) if (!std::strcmp(argv[i], n)) return true;
    return false;
  };
  using art::Prop;
  if (want("cottage")) {
    Board b(1400, 300);
    int x = 10;
    const art::Building kinds[3] = {art::Building::House, art::Building::House, art::Building::Inn};
    for (int k = 0; k < 3; k++)
      for (int rv = 1; rv <= 2; rv++) {
        art::setRigVersion(rv);
        const Canvas c = art::buildingSprite(bld::design(bld::classicRequest(kinds[k], 5, 4, k == 1 ? rgba(150, 60, 50) : 0, 1234u + k * 77u)));
        b.put(c, x, 280 - c.h);
        int sole = 0;
        const Canvas fig = figureOf(rv, sole);
        b.put(fig, x + c.w + 2, 279 - sole);
        art::setRigVersion(1);
        b.text(x, 10, rv == 1 ? "RIG 1" : "RIG 2");
        x += c.w + 30;
      }
    art::setRigVersion(1);
    rigpv::saveBoth(b, dir, "holders_cottage");
  }
  if (want("furniture")) {
    Board b(900, 380);
    itemRow(b, 8, 100, {{"CHAIR", [] { return prop(Prop::Chair); }}, {"STOOL", [] { return prop(Prop::Stool); }},
                        {"BENCH", [] { return prop(Prop::Bench); }}, {"THRONE", [] { return prop(Prop::Throne); }},
                        {"TABLE", [] { return prop(Prop::Table); }}}, true);
    itemRow(b, 8, 220, {{"TABLE MEAL", [] { return prop(Prop::TableMeal); }}, {"TABLE WORK", [] { return prop(Prop::TableWork); }},
                        {"TABLE L M R", [] {
                           Canvas l = prop(Prop::TableL), m = prop(Prop::TableM), r = prop(Prop::TableR);
                           Canvas c(l.w * 3, l.h);
                           for (int y = 0; y < l.h; y++) for (int xx = 0; xx < l.w; xx++) { c.set(xx, y, l.get(xx, y)); c.set(xx + l.w, y, m.get(xx, y)); c.set(xx + 2 * l.w, y, r.get(xx, y)); }
                           return c;
                         }},
                        {"COUNTER", [] { return prop(Prop::Counter); }},
                        {"SHOP COUNTER", [] {
                           Canvas l = art::interiorPiece(art::pieceKey(art::Piece::Styled, 0, 5, 0)), m = art::interiorPiece(art::pieceKey(art::Piece::Styled, 0, 5, 1 | 4)),
                                  r = art::interiorPiece(art::pieceKey(art::Piece::Styled, 0, 5, 2));
                           Canvas c(l.w * 3, l.h);
                           for (int y = 0; y < l.h; y++) for (int xx = 0; xx < l.w; xx++) { c.set(xx, y, l.get(xx, y)); c.set(xx + l.w, y, m.get(xx, y)); c.set(xx + 2 * l.w, y, r.get(xx, y)); }
                           return c;
                         }}}, true);
    itemRow(b, 8, 340, {{"DESK", [] { return prop(Prop::Desk); }}, {"WORKBENCH", [] { return prop(Prop::Workbench); }},
                        {"PREP TABLE", [] { return prop(Prop::PrepTable); }}, {"DISPLAY TABLE", [] { return prop(Prop::DisplayTable); }},
                        {"THRONE STYLED", [] { return art::interiorPiece(art::pieceKey(art::Piece::Styled, 0, art::kStyledThrone, 3)); }}}, true);
    rigpv::saveBoth(b, dir, "holders_furniture");
  }
  if (want("beds")) {
    Board b(700, 150);
    itemRow(b, 8, 110, {{"BED", [] { return prop(Prop::Bed); }}, {"LONG BED", [] { return art::interiorPiece(art::pieceKey(art::Piece::Styled, 0, 4, 1)); }},
                        {"BUNK", [] { return prop(Prop::BunkBed); }}, {"HAMMOCK", [] { return prop(Prop::Hammock); }},
                        {"MAT", [] { return prop(Prop::SleepingMat); }}, {"CUSHION", [] { return prop(Prop::Cushion); }},
                        {"LOW TABLE", [] { return prop(Prop::LowTable); }}}, true);
    rigpv::saveBoth(b, dir, "holders_beds");
  }
  if (want("outdoor")) {
    Board b(900, 150);
    itemRow(b, 8, 100, {{"FENCE", [] {
                           Canvas a = prop(Prop::FenceH);
                           Canvas c(a.w * 3, a.h);
                           for (int k = 0; k < 3; k++) for (int y = 0; y < a.h; y++) for (int xx = 0; xx < a.w; xx++) c.set(xx + k * a.w, y, a.get(xx, y));
                           return c;
                         }},
                        {"FENCE V", [] { return prop(Prop::FenceV); }}, {"GARDEN WALL", [] { return prop(Prop::GardenWall); }},
                        {"HEDGE", [] { return prop(Prop::Hedge); }}, {"CART", [] { return prop(Prop::Cart); }},
                        {"LADDER", [] { return prop(Prop::Ladder); }}, {"GRAVE", [] { return prop(Prop::Gravestone); }}}, false);
    rigpv::saveBoth(b, dir, "holders_outdoor");
  }
  if (want("mounts")) {   // the ridden horse (the rig's horse: horseSheetRig) side, back, front; the saddle seat ticked
    Board b(1100, 260);
    int y = 110;
    for (int rv = 1; rv <= 2; rv++) {
      art::setRigVersion(rv);
      const int cw = art::horseCellW(), ch = art::horseCellH();
      const Canvas sh = art::horseSheetRig(rgba(140, 90, 48), 77u, true);
      int x = 10;
      for (int row : {2, 1, 0})
        for (int f : {0, 1, 2, 5}) {
          Canvas cell(cw, ch);
          for (int yy = 0; yy < ch; yy++) for (int xx = 0; xx < cw; xx++) cell.set(xx, yy, sh.get(f * cw + xx, row * ch + yy));
          b.put(cell, x, y - ch + 1);
          int dx, dy;
          art::saddleSeat(row, f, dx, dy);
          const int hoof = y - 1;   // the cell's row ch - 2
          for (int k = -2; k <= 2; k++) b.c.set(x + cw / 2 + dx + k, hoof + dy, rgba(242, 193, 78));
          x += cw + 4;
        }
      int sole = 0;
      const Canvas fig = figureOf(rv, sole);
      b.put(fig, x + 4, y - sole);
      b.text(x + 30, y - 20, rv == 1 ? "RIG 1" : "RIG 2");
      y += 120;
    }
    art::setRigVersion(1);
    rigpv::saveBoth(b, dir, "holders_mounts");
  }
  if (want("carts")) {   // the market stall (3/4 model), the trestle table, the market cart, the hand cart
    Board b(1200, 300);
    int x = 10;
    for (int rv = 1; rv <= 2; rv++) {
      art::setRigVersion(rv);
      const int ground = 220;   // the prop tile's bottom: the stall's tile top-left at (x + 40, ground - 16)
      int ox, oy;
      art::stallOrigin(art::StallS, ox, oy);
      const Canvas st = art::marketStallFacing(2, 1, 0, false, art::StallS);
      b.put(st, x + 40 + ox, ground - 16 + oy);
      const Canvas st2 = art::marketStallFacing(4, 3, 2, false, art::StallS);
      b.put(st2, x + 110 + ox, ground - 16 + oy);
      art::marketTableOrigin(ox, oy);
      b.put(art::marketTable(1, 1, false), x + 200 + ox, ground - 16 + oy);
      art::marketCartOrigin(ox, oy);
      b.put(art::marketCart(0), x + 270 + ox, ground - 16 + oy);
      for (int d = 0; d < 4; d++) {
        const Canvas hc = art::handcartSprite((uint64_t)d | (uint64_t)(d & 3) << 2);
        b.put(hc, x + 330 + d * 60 - art::HANDCART_OX, ground - art::HANDCART_OY + (d >= 2 ? 0 : 0));
      }
      int sole = 0;
      const Canvas fig = figureOf(rv, sole);
      b.put(fig, x + 20, ground - sole);
      b.put(fig, x + 300, ground - sole);
      b.text(x, 10, rv == 1 ? "RIG 1" : "RIG 2");
      x += 590;
    }
    art::setRigVersion(1);
    rigpv::saveBoth(b, dir, "holders_carts");
  }
  if (want("boats")) {   // the rowboat, the sloop, the galleon (heading south-east), the figure on the deck
    Board b(1100, 520);
    int x = 10;
    for (int rv = 1; rv <= 2; rv++) {
      art::setRigVersion(rv);
      int y = 250 * rv;
      int xx = 10;
      for (int hull = 0; hull < 3; hull++) {
        const Canvas c = art::shipCell(hull, art::ShipStyle{}, 4, 0, art::SHIP_LAYER_BACK);
        const int ax = art::shipAnchorX(hull), ay = art::shipAnchorY(hull);
        b.put(c, xx, y - c.h);
        int sole = 0;
        const Canvas fig = figureOf(rv, sole);
        b.put(fig, xx + ax - fig.w / 2, y - c.h + ay - art::shipDeckY(hull) - sole);
        b.put(art::shipCell(hull, art::ShipStyle{}, 4, 0, art::SHIP_LAYER_FRONT), xx, y - c.h);
        xx += c.w + 10;
      }
      (void)x;
      b.text(10, y - 240, rv == 1 ? "RIG 1" : "RIG 2");
    }
    art::setRigVersion(1);
    rigpv::saveBoth(b, dir, "holders_boats");
    // (fix r1) the hulls at four headings with their sails furled (east, south-east, south, north-east), moored on a
    // water strip, the figure aboard between the back and front layers as the view draws it; rig 1 over rig 2
    const uint32_t water = rgba(52, 96, 140), water2 = rgba(60, 108, 152);
    Board d(1500, 560);
    for (int rv = 1; rv <= 2; rv++) {
      art::setRigVersion(rv);
      const int y0 = 10 + (rv - 1) * 275;
      for (int yy = y0; yy < y0 + 265; yy++) for (int xx = 0; xx < d.c.w; xx++) d.c.set(xx, yy, ((xx / 7 + yy / 3) % 5) ? water : water2);
      int xx = 10;
      for (int hull = 0; hull < 3; hull++)
        for (int fd : {0, 4, 8, 28}) {
          art::ShipStyle st;
          st.furled = true;
          const Canvas bk = art::shipCell(hull, st, fd, 0, art::SHIP_LAYER_BACK);
          const int ax = art::shipAnchorX(hull), ay = art::shipAnchorY(hull);
          const int top = y0 + 255 - bk.h;
          d.put(bk, xx, top);
          int sole = 0;
          const Canvas fig = figureOf(rv, sole);
          d.put(fig, xx + ax - fig.w / 2, top + ay - art::shipDeckY(hull) - sole);
          d.put(art::shipCell(hull, st, fd, 0, art::SHIP_LAYER_FRONT), xx, top);
          // a gold tick at the thwart's seat line (rowboat): where a seated rower's hips go
          if (hull == 0) for (int k = -3; k <= 3; k++) d.c.set(xx + ax + k + 14, top + ay - art::shipThwartY(hull), rgba(242, 193, 78));
          xx += std::max(bk.w * 3 / 4, 40) + 6;
        }
      d.text(10, y0 + 4, rv == 1 ? "RIG 1" : "RIG 2");
    }
    art::setRigVersion(1);
    rigpv::saveBoth(d, dir, "holders_boats_moored");
    // the layer split of the rowboat and the sloop at rig 2: the back layer, the front layer, at four headings
    Board e(1100, 330);
    art::setRigVersion(2);
    for (int hull = 0; hull < 2; hull++)
      for (int k = 0; k < 4; k++) {
        art::ShipStyle st;
        st.furled = true;
        const int fd = k * 8, x = 10 + k * 270, y = 10 + hull * 160;
        const Canvas bk = art::shipCell(hull, st, fd, 0, art::SHIP_LAYER_BACK), fr = art::shipCell(hull, st, fd, 0, art::SHIP_LAYER_FRONT);
        e.put(bk, x, y);
        e.put(fr, x + bk.w + 4, y);
      }
    art::setRigVersion(1);
    rigpv::saveBoth(e, dir, "holders_boats_layers");
  }
  if (want("gates")) {   // every gate form at rig 1 and rig 2, the rig's figure standing in its passage's mouth (STYLE_P2 1: walks under)
    struct G { const char* name; art::CityWall wall; bld::GateForm gate; bld::TowerForm tower; int culture; uint32_t roof; };
    const G gs[] = {{"DRUM TOWERS", art::CityWall::Stone, bld::GateForm::DrumTowers, bld::TowerForm::RoundDrum, 0, 0},
                    {"SQUARE TOWERS", art::CityWall::Stone, bld::GateForm::SquareTowers, bld::TowerForm::Square, 2, rgba(150, 70, 52)},
                    {"PYLONS", art::CityWall::Talud, bld::GateForm::Pylons, bld::TowerForm::Square, 10, 0},
                    {"PALISADE DRUMS", art::CityWall::Palisade, bld::GateForm::DrumTowers, bld::TowerForm::ConeDrum, 0, 0},
                    {"THORN DRUMS", art::CityWall::Thorn, bld::GateForm::DrumTowers, bld::TowerForm::RoundDrum, 0, 0},
                    {"WHITE DRUMS", art::CityWall::WhiteStone, bld::GateForm::DrumTowers, bld::TowerForm::ConeDrum, 0, 0},
                    {"JADE DRUMS", art::CityWall::Jade, bld::GateForm::DrumTowers, bld::TowerForm::ConeDrum, 0, 0},
                    {"ADOBE DRUMS", art::CityWall::Adobe, bld::GateForm::DrumTowers, bld::TowerForm::RoundDrum, 0, 0},
                    {"TIMBER GATE", art::CityWall::Palisade, bld::GateForm::TimberGate, bld::TowerForm::ConeDrum, 1, 0},
                    {"STEPPE TIMBER", art::CityWall::Palisade, bld::GateForm::TimberGate, bld::TowerForm::Platform, 6, 0},
                    {"EARTHWORK", art::CityWall::Rampart, bld::GateForm::Earthwork, bld::TowerForm::Bastion, 2, 0},
                    {"HEDGE GAP", art::CityWall::Thorn, bld::GateForm::HedgeGap, bld::TowerForm::RoundDrum, 6, 0},
                    {"IWAN", art::CityWall::Adobe, bld::GateForm::Iwan, bld::TowerForm::Minaret, 5, 0},
                    {"PAIFANG", art::CityWall::Jade, bld::GateForm::Paifang, bld::TowerForm::Pagoda, 8, 0},
                    {"MOON GATE", art::CityWall::Jade, bld::GateForm::MoonGate, bld::TowerForm::Pagoda, 8, 0},
                    {"ELVEN ARCH", art::CityWall::WhiteStone, bld::GateForm::ElvenArch, bld::TowerForm::ConeDrum, 12, 0},
                    {"LIVING ARCH", art::CityWall::Thorn, bld::GateForm::LivingArch, bld::TowerForm::RoundDrum, 11, 0}};
    const int n = (int)(sizeof(gs) / sizeof(gs[0])), perRow = 4;
    for (int height = 0; height <= 3; height += 3) {
      Board b(perRow * 2 * (art::GATE_CW + 8) + 16, ((n + perRow - 1) / perRow) * (art::GATE_CH + 22) + 12);
      for (int i = 0; i < n; i++) {
        bld::FortParts f = bld::fortDefaults(gs[i].wall);
        f.gate = gs[i].gate; f.tower = gs[i].tower; f.culture = (uint8_t)gs[i].culture; f.roof = gs[i].roof; f.height = (uint8_t)height;
        f.finery = 2;
        cult::Heraldry arms;
        arms.field = rgba(150, 40, 44); arms.charge = rgba(232, 200, 90); arms.emblem = 2;
        const int row = i / perRow, col = i % perRow;
        const int y0 = 8 + row * (art::GATE_CH + 22);
        for (int rv = 1; rv <= 2; rv++) {
          art::setRigVersion(rv);
          const Canvas g = art::gateHouse(77u + i, arms, f);
          const int x0 = 8 + (col * 2 + rv - 1) * (art::GATE_CW + 8);
          for (int y = 0; y < g.h; y++) for (int x = 0; x < g.w; x++) if (y > art::GATE_OY + 15) b.c.set(x0 + x, y0 + y, rgba(150, 128, 96));   // the road
          b.put(g, x0, y0);
          b.frame(x0, y0, g.w, g.h);
          int sole = 0;
          const Canvas fig = figureOf(rv, sole);
          b.put(fig, x0 + art::GATE_OX + 24 - fig.w / 2, y0 + art::GATE_OY + 16 - sole);
          // the clear-height ticks: 40 px (1.25 of the v2 figure) over the passage's front ground row
          const int tick = y0 + art::GATE_OY + 15 - 40;
          for (int k = 0; k < 3; k++) { b.c.set(x0 + art::GATE_OX - 4 + k, tick, rgba(240, 60, 60)); b.c.set(x0 + art::GATE_OX + 49 + k, tick, rgba(240, 60, 60)); }
        }
        art::setRigVersion(1);
        b.text(8 + col * 2 * (art::GATE_CW + 8), y0 + art::GATE_CH + 3, gs[i].name);
      }
      rigpv::saveBoth(b, dir, height ? "holders_gates_capital" : "holders_gates");
    }
    // a wall run with a span over an opening (no gatehouse): jamb, three span tiles, jamb, east-west then north-south
    Board b(2 * 9 * 16 + 60, 2 * 100 + 20);
    const art::CityWall walls[4] = {art::CityWall::Stone, art::CityWall::Palisade, art::CityWall::Adobe, art::CityWall::WhiteStone};
    for (int w = 0; w < 4; w++)
      for (int rv = 1; rv <= 2; rv++) {
        art::setRigVersion(rv);
        const bld::FortParts f = bld::fortDefaults(walls[w]);
        const int x0 = 10 + (rv - 1) * (9 * 16 + 30), y0 = 10 + (w & 1) * 100 + 60;
        if (w >= 2) continue;   // (two materials keep the board small)
        for (int t = 0; t < 7; t++) {
          uint32_t k = art::WALL_BIT_E | art::WALL_BIT_W;
          if (t >= 2 && t <= 4) k |= art::WALL_BIT_SPAN | ((uint32_t)(t - 1) << art::WALL_SPAN_POS_SHIFT);
          const Canvas c = art::wallTile(k, f);
          for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) b.c.set(x0 + t * 16 + x, y0 + 16 + y, rgba(150, 128, 96));
          b.put(c, x0 + t * 16 - art::WALL_OX, y0 - art::WALL_OY);
        }
        int sole = 0;
        const Canvas fig = figureOf(rv, sole);
        b.put(fig, x0 + 3 * 16 + 8 - fig.w / 2, y0 + 16 - sole);
      }
    art::setRigVersion(1);
    rigpv::saveBoth(b, dir, "holders_spans");
  }
  if (want("graves")) {
    Board b(700, 130);
    std::vector<Item> items;
    static const char* names[5] = {"HEADSTONE", "CAIRN", "POST", "STONE", "URN"};
    for (int r = 0; r < 5; r++)
      items.push_back({names[r], [r] { art::GraveLook g; g.rite = (uint8_t)r; g.weapon = 1; g.seed = 5u + r; return art::graveSprite(g); }});
    itemRow(b, 8, 100, items, false);
    rigpv::saveBoth(b, dir, "holders_graves");
  }
  {
    FILE* f = std::fopen((dir + "/holders_metrics.txt").c_str(), "wb");
    for (int h = 0; h < (int)art::Holder::COUNT; h++) {
      art::setRigVersion(1);
      const int a = art::holderPx((art::Holder)h);
      art::setRigVersion(2);
      const int b2 = art::holderPx((art::Holder)h);
      const char* line = "%-18s rig1 %3d px  rig2 %3d px  %.2f of the 32 px figure\n";
      std::printf(line, art::holderName((art::Holder)h), a, b2, b2 / 32.0);
      if (f) std::fprintf(f, line, art::holderName((art::Holder)h), a, b2, b2 / 32.0);
    }
    art::setRigVersion(1);
    if (f) std::fclose(f);
  }
  return 0;
}
