// M7 "Home": the build / decorate / shell / storage / cooking screens (Mode::Build on Game::home.ui; rpg/sim/home.h), and
// the HUD's home pills (YARD / DECORATE, DISMOUNT). VIEW lane.
//
// Phone first (the 639 x 294 logical canvas of an iPhone in landscape, its safe insets respected): every target is at
// least 24 px (33 pt) on touch, the text is the HUD's 1x font, the world stays in view while the yard is laid out.
//   - YARD (outdoors) and DECORATE (inside the player's own house): a panel down the right side (the world on the left is
//     the ghost's canvas): the catalogue by category (tabs), a grid of the pieces with their picture and price, the
//     selected piece's name, footprint and price (and why it does not fit, in red), ROTATE, SELL (50 %), PLACE, DONE.
//     A tap on the world puts the ghost on that tile; a drag moves it with the finger (relative, so the finger never
//     covers it); the camera keeps the ghost in the free part of the screen. Keys: arrows / WASD move the ghost, Q / E
//     the piece, 1-5 (PageUp / PageDown) the category, R rotates, Enter / Space places, X / Delete sells, Esc / Tab done.
//   - SHELL (a bare lot, opened by the builder's talk): the five shells as cards (footprint, storeys, price, days; dimmed
//     when the lot is too small), the house drawn live by the builder in the chosen culture style (the styles the player
//     has discovered, < >), BUILD (home::startBuild) and CANCEL. Keys: up / down the shell, left / right the style.
//   - STORAGE (the player's chest, shelf, crate...): the pack and the store side by side; a tap on an item moves it
//     across (storePut / storeTake); drag a column to scroll; < > between the plot's stores.
//   - COOK (a hearth, a cooking pot, a campfire, an inn's kitchen): the meals known, each ingredient held / needed (from
//     home::ingredientOf over the pack), the meal's quality at this station and its Well Fed hours, COOK (home::cook).
// The view never edits Homes: it sets g.home.ui and g.mode and calls the home:: actions.
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/home.h"
#include "rpg/view/view.h"
#include "rpg/world/source.h"

namespace {
const Color kGoldC(0.98f, 0.82f, 0.42f), kTextC(0.93f, 0.9f, 0.82f), kDimC(0.62f, 0.58f, 0.52f), kBadC(0.98f, 0.50f, 0.42f),
    kGoodC(0.62f, 0.92f, 0.56f), kInkC(0.10f, 0.08f, 0.10f);
using home::Obj;
using home::UiMode;

// ---------------------------------------------------------------- the catalogue
struct Cat { const char* tab; const char* name; std::vector<Obj> objs; };
const std::vector<Cat>& cats(bool inside) {
  static std::vector<Cat> out, in;
  if (out.empty()) {
    out = {{"FENCE", "FENCES AND PATHS", {Obj::Fence, Obj::Gate, Obj::Path}},
           {"FARM", "FARM", {Obj::Farmland, Obj::Well, Obj::Scarecrow, Obj::Beehive, Obj::Sapling}},
           {"BEAST", "ANIMAL HOMES", {Obj::Coop, Obj::Pen, Obj::Stable, Obj::Trough, Obj::HayRack, Obj::Doghouse}},
           {"WORK", "WORK", {Obj::Woodpile, Obj::Workbench, Obj::Forge, Obj::Campfire, Obj::ShippingCrate}},
           {"DECOR", "DECOR", {Obj::FlowerBed, Obj::Bench, Obj::Lantern, Obj::Statue, Obj::Banner}}};
    in = {{"REST", "FURNITURE", {Obj::Bed, Obj::Table, Obj::Chair, Obj::Stool}},
          {"STORE", "STORAGE", {Obj::Chest, Obj::Shelf, Obj::Wardrobe, Obj::Cupboard, Obj::Bookshelf}},
          {"COOK", "KITCHEN", {Obj::Hearth, Obj::CookPot}},
          {"SHOW", "DISPLAY", {Obj::Mannequin, Obj::WeaponRack, Obj::Trophy, Obj::Painting}},
          {"DECOR", "DECOR", {Obj::Rug, Obj::PlantPot, Obj::Candelabra}}};
    // anything the catalogue gains later lands in DECOR rather than nowhere
    for (int o = 0; o < (int)Obj::COUNT; o++) {
      const uint16_t f = home::objInfo((Obj)o).flags;
      for (int side = 0; side < 2; side++) {
        std::vector<Cat>& cs = side ? in : out;
        if (!(f & (side ? home::OBJ_INSIDE : home::OBJ_OUTSIDE))) continue;
        bool found = false;
        for (const Cat& c : cs) for (Obj x : c.objs) if ((int)x == o) found = true;
        if (!found) cs.back().objs.push_back((Obj)o);
      }
    }
  }
  return inside ? in : out;
}
int catOf(bool inside, int sel) {
  const std::vector<Cat>& cs = cats(inside);
  for (int c = 0; c < (int)cs.size(); c++) for (Obj o : cs[(size_t)c].objs) if ((int)o == sel) return c;
  return -1;
}

std::string fitText(const std::string& s, float w) {
  const int n = std::max(1, (int)((w + 1) / 6));
  if ((int)s.size() <= n) return s;
  return s.substr(0, (size_t)std::max(1, n - 1)) + ".";
}
std::string plotName(const Game& g, const home::Plot& p) {
  if (!p.name.empty()) return p.name;
  const int si = p.site ? g.world.siteHandle(p.site) : -1;
  if (si >= 0) return g.world.sites[(size_t)si].name + " HOMESTEAD";
  return "YOUR HOMESTEAD";
}
const char* stationName(home::Station s) {
  switch (s) {
    case home::Station::Campfire: return "CAMPFIRE";
    case home::Station::Hearth: return "HEARTH";
    case home::Station::CookPot: return "COOKING POT";
    case home::Station::InnKitchen: return "INN KITCHEN";
    default: return "FIRE";
  }
}
const char* qualityWord(int q) { return q <= 0 ? "PLAIN" : q == 1 ? "GOOD" : q == 2 ? "FINE" : "SUPERB"; }
art::Icon ingrIcon(home::Ingr i, uint32_t& tint) {
  tint = 0;
  switch (i) {
    case home::Ingr::Grain: return art::Icon::Sheaf;
    case home::Ingr::Flour: return art::Icon::Flour;
    case home::Ingr::Bread: return art::Icon::Bread;
    case home::Ingr::Veg: return art::Icon::Veg;
    case home::Ingr::Fruit: return art::Icon::Fruit;
    case home::Ingr::Meat: return art::Icon::Meat;
    case home::Ingr::Fish: return art::Icon::Fish;
    case home::Ingr::Egg: return art::Icon::Egg;
    case home::Ingr::Milk: return art::Icon::Milk;
    case home::Ingr::Cheese: return art::Icon::Cheese;
    case home::Ingr::Herb: return art::Icon::Herb;
    case home::Ingr::Honey: return art::Icon::Honey;
    default: return art::Icon::Bone;
  }
}
const char* ingrName(home::Ingr i) {
  static const char* n[] = {"", "GRAIN", "FLOUR", "BREAD", "VEG", "FRUIT", "MEAT", "FISH", "EGG", "MILK", "CHEESE", "HERB", "HONEY"};
  return (int)i < (int)(sizeof n / sizeof n[0]) ? n[(int)i] : "";
}
int heldOf(const Game& g, home::Ingr in) {
  int n = 0;
  for (const Item& it : g.inv) if (home::ingredientOf(it) == in) n += it.stackable() ? it.count : 1;
  return n;
}
// (M7 fix) a catalogue card's own label (6 letters at most: the card is 40 px at the phone's 1x font)
std::string cardLabel(Obj o) {
  switch (o) {
    case Obj::Farmland: return "SOIL";
    case Obj::Woodpile: return "LOGS";
    case Obj::Beehive: return "HIVE";
    case Obj::Scarecrow: return "SCARER";
    case Obj::Coop: return "COOP";
    case Obj::Pen: return "PEN";
    case Obj::Workbench: return "BENCH";
    case Obj::Bench: return "SEAT";
    case Obj::FlowerBed: return "BLOOMS";
    case Obj::Campfire: return "FIRE";
    case Obj::Sapling: return "TREE";
    case Obj::Lantern: return "LAMP";
    case Obj::Doghouse: return "KENNEL";
    case Obj::HayRack: return "HAY";
    case Obj::ShippingCrate: return "CRATE";
    case Obj::CookPot: return "POT";
    case Obj::Mannequin: return "ARMOUR";
    case Obj::WeaponRack: return "ARMS";
    case Obj::Painting: return "ART";
    case Obj::Wardrobe: return "CLOSET";
    case Obj::Bookshelf: return "BOOKS";
    case Obj::PlantPot: return "PLANT";
    case Obj::Candelabra: return "CANDLE";
    case Obj::Cupboard: return "HUTCH";
    default: return home::objInfo(o).name;
  }
}
bool lotFits(const home::Plot& p, home::Shell s) {
  return home::shellFits(p, s);
}

// ---------------------------------------------------------------- layouts (screen coordinates)
struct Rect { float x = 0, y = 0, w = 0, h = 0; bool in(Vec2 p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; } };
// YARD / DECORATE: the side panel
struct SideLay {
  Rect panel, tabs[5], grid, info, rot, sell, place, done;
  int cols = 4, rows = 2;
  float cellW = 40, cellH = 34, tabH = 24, btnH = 26;
};
SideLay sideLay(bool touch) {
  SideLay L;
  const float R = (float)(Pix::W - Pix::SR), T = (float)Pix::ST, B = (float)(Pix::H - Pix::SB);
  const float pw = touch ? 190.0f : 176.0f;
  L.panel = {R - pw - 4, T + 4, pw, B - T - 8};
  L.tabH = touch ? 24.0f : 17.0f;
  L.btnH = touch ? 26.0f : 17.0f;
  const float x0 = L.panel.x + 6, w = pw - 12;
  const float tw = (w - 4 * 2) / 5.0f;
  for (int i = 0; i < 5; i++) L.tabs[i] = {x0 + i * (tw + 2), L.panel.y + 18, tw, L.tabH};
  const float by2 = L.panel.y + L.panel.h - 6 - L.btnH, by1 = by2 - 4 - L.btnH;
  const float bw = (w - 4) / 2;
  L.rot = {x0, by1, bw, L.btnH};
  L.sell = {x0 + bw + 4, by1, bw, L.btnH};
  L.place = {x0, by2, bw, L.btnH};
  L.done = {x0 + bw + 4, by2, bw, L.btnH};
  L.info = {x0, by1 - 36, w, 32};
  const float gy = L.tabs[0].y + L.tabH + 5;
  L.cols = 4;
  L.cellW = std::floor((w - (L.cols - 1) * 3) / L.cols);
  // two rows hold every category (at most 8 pieces): the cells take the height there is, so the pictures show at 1x
  L.rows = 2;
  L.cellH = std::clamp(std::floor((L.info.y - 4 - gy - 3) / 2), touch ? 30.0f : 26.0f, 58.0f);
  L.grid = {x0, gy, w, L.rows * (L.cellH + 3) - 3};
  return L;
}
Rect cellRect(const SideLay& L, int i) {
  const int c = i % L.cols, r = i / L.cols;
  return {L.grid.x + c * (L.cellW + 3), L.grid.y + r * (L.cellH + 3), L.cellW, L.cellH};
}
// the modal screens (SHELL / STORAGE / COOK / BUY): the safe area less a margin
Rect modalRect() {
  const float L = (float)Pix::SL, T = (float)Pix::ST, R = (float)(Pix::W - Pix::SR), B = (float)(Pix::H - Pix::SB);
  return {L + 6, T + 6, R - L - 12, B - T - 12};
}
struct ShellLay { Rect card[5], preview, prev, next, styleBar, build, cancel; };
ShellLay shellLay(bool touch) {
  ShellLay S;
  const Rect M = modalRect();
  const float ch = touch ? 33.0f : 29.0f, cw = touch ? 162.0f : 150.0f, bh = touch ? 26.0f : 17.0f;
  for (int i = 0; i < 5; i++) S.card[i] = {M.x + 8, M.y + 20 + i * (ch + 3), cw, ch};
  const float rx = M.x + 8 + cw + 8, rw = M.x + M.w - 8 - rx;
  S.build = {M.x + M.w - 8 - 2 * 96 - 6, M.y + M.h - 8 - bh, 96, bh};
  S.cancel = {M.x + M.w - 8 - 96, M.y + M.h - 8 - bh, 96, bh};
  S.styleBar = {rx, S.build.y - 8 - bh, rw, bh};
  S.prev = {rx, S.styleBar.y, touch ? 30.0f : 20.0f, bh};
  S.next = {rx + rw - (touch ? 30.0f : 20.0f), S.styleBar.y, touch ? 30.0f : 20.0f, bh};
  S.preview = {rx, M.y + 20, rw, S.styleBar.y - 6 - (M.y + 20)};
  return S;
}
struct ListLay { Rect col[2], up[2], down[2], prev, next, done; float rowH = 20; int rows = 6; };
ListLay listLay(bool touch) {
  ListLay S;
  const Rect M = modalRect();
  const float bh = touch ? 26.0f : 17.0f;
  S.rowH = touch ? 22.0f : 18.0f;
  S.done = {M.x + M.w - 8 - 90, M.y + M.h - 8 - bh, 90, bh};
  const float cw = (M.w - 24) / 2, top = M.y + 34, bottom = S.done.y - 6;
  for (int c = 0; c < 2; c++) {
    const float x = M.x + 8 + c * (cw + 8);
    S.col[c] = {x, top, cw, bottom - top};
    // (M7 fix) touch: the hit 32 tall (upward: arrowHit). (M7 fixer r2, review: "the arrows are too small, side by side")
    // drawn 40 x 21 (53 x 28 pt on an iPhone, the hit 53 x 43 pt), 12 px apart so a tap cannot take the wrong one
    S.up[c] = {x + cw - 2 * (touch ? 40.0f : 18.0f) - (touch ? 12.0f : 2.0f), touch ? M.y + 12 : M.y + 18, touch ? 40.0f : 18.0f, touch ? 21.0f : 13.0f};
    S.down[c] = {x + cw - (touch ? 40.0f : 18.0f), S.up[c].y, S.up[c].w, S.up[c].h};
  }
  S.rows = std::max(1, (int)(S.col[0].h / S.rowH));
  // (M7 fixer r2) the stores' pager: < STORE 1/2 > (its label between the buttons, clear of DONE)
  S.prev = {S.col[1].x + 20, M.y + M.h - 8 - bh, touch ? 30.0f : 20.0f, bh};
  S.next = {S.prev.x + S.prev.w + 60, S.prev.y, S.prev.w, bh};
  return S;
}
struct CookLay { Rect list, detail, cook, done, up, down; float rowH = 26; int rows = 6; };
CookLay cookLay(bool touch) {
  CookLay S;
  const Rect M = modalRect();
  const float bh = touch ? 26.0f : 17.0f;
  S.rowH = touch ? 25.0f : 20.0f;
  const float lw = std::min(250.0f, M.w * 0.52f);
  S.list = {M.x + 8, M.y + 20, lw, M.h - 28};
  S.rows = std::max(1, (int)(S.list.h / S.rowH));
  S.list.h = S.rows * S.rowH;
  S.detail = {S.list.x + lw + 10, M.y + 20, M.x + M.w - 8 - (S.list.x + lw + 10), M.h - 28};
  S.done = {S.detail.x + S.detail.w - 90, M.y + M.h - 8 - bh, 90, bh};
  S.cook = {S.done.x - 6 - 100, S.done.y, 100, bh};
  S.up = {S.list.x + lw - 2 * (touch ? 40.0f : 18.0f) - (touch ? 12.0f : 2.0f), touch ? M.y + 1 : M.y + 4, touch ? 40.0f : 18.0f, touch ? 18.0f : 13.0f};
  S.down = {S.list.x + lw - (touch ? 40.0f : 18.0f), S.up.y, S.up.w, S.up.h};
  return S;
}

// a scroll arrow's hit area: at least 32 tall, grown upward (below it is the list, whose rows take taps)
Rect arrowHit(const Rect& r) { return r.h >= 32 ? r : Rect{r.x, r.y + r.h - 32, r.w, 32}; }

// the placed object covering a tile (outside: plot tiles; inside: the floor's tiles), -1 none
int objAtTile(const Game& g, const home::Plot& p, int x, int y, bool inside) {
  const std::vector<home::PlacedObj>& v = inside ? p.inside : p.outside;
  for (int i = (int)v.size() - 1; i >= 0; i--) {
    const home::PlacedObj& o = v[(size_t)i];
    if (inside && o.floor() != g.subFloor) continue;
    const home::ObjInfo& oi = home::objInfo((Obj)o.kind);
    int w = oi.w, h = oi.h;
    if (o.turned() && (oi.flags & home::OBJ_ROTATES)) std::swap(w, h);
    if (x >= o.x && y >= o.y && x < o.x + w && y < o.y + h) return i;
  }
  return -1;
}
}  // namespace

// ---------------------------------------------------------------- opening the screens
void View::openBuild(Game& g, home::UiMode mode, int plot) {
  home::Ui& U = g.home.ui;
  const home::Ui keep = U;
  U = home::Ui();
  U.mode = mode;
  U.plot = plot;
  buildDrag_ = BuildDrag();
  buildHintGx_ = buildHintGy_ = INT32_MIN;
  if (plot < 0 || plot >= (int)g.home.plots.size()) return;
  const home::Plot& p = g.home.plots[(size_t)plot];
  const Actor& pl = g.pl();
  if (mode == UiMode::Yard || mode == UiMode::Decorate) {
    const bool inside = mode == UiMode::Decorate;
    buildCat_ = std::clamp(buildCat_, 0, (int)cats(inside).size() - 1);
    U.sel = (int)cats(inside)[(size_t)buildCat_].objs.front();
    if (keep.plot == plot && keep.mode == mode && catOf(inside, keep.sel) >= 0) { U.sel = keep.sel; buildCat_ = catOf(inside, keep.sel); }
    const Vec2 probe = pl.p + pl.aim * 16.0f + Vec2(0, -4);
    int tx = (int)std::floor(probe.x / TILE), ty = (int)std::floor(probe.y / TILE);
    if (inside) {
      const Map& m = g.map();
      U.gx = std::clamp(tx, 0, std::max(0, m.w - 1));
      U.gy = std::clamp(ty, 0, std::max(0, m.h - 1));
      // (M7 fix) the ghost starts on the nearest tile the piece fits (not half in a round room's wall by the door)
      std::string why;
      if (!home::canPlaceInside(p, m, (Obj)U.sel, U.gx, U.gy, false, why)) {
        bool found = false;
        for (int r = 1; r <= 6 && !found; r++)
          for (int dy = -r; dy <= r && !found; dy++)
            for (int dx = -r; dx <= r && !found; dx++) {
              if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
              if (home::canPlaceInside(p, m, (Obj)U.sel, U.gx + dx, U.gy + dy, false, why)) { U.gx += dx; U.gy += dy; found = true; }
            }
      }
    } else {
      U.gx = std::clamp(tx + g.world.ox - p.gx, 0, std::max(0, (int)p.w - 1));
      U.gy = std::clamp(ty + g.world.oy - p.gy, 0, std::max(0, (int)p.h - 1));
    }
  } else if (mode == UiMode::Shell) {
    U.sel = (int)home::Shell::Cottage;
    if (!lotFits(p, home::Shell::Cottage)) U.sel = (int)home::Shell::Hut;
    // the style of the land the lot stands in first (when known), else the first discovered
    if (!g.home.styles.empty()) {
      U.style = g.home.styles.front();
      if (g.world.src) {
        const uint64_t here = g.world.src->cultureAt(p.gx + p.w / 2, p.gy + p.h / 2);
        if (g.home.knowsStyle(here)) U.style = here;
      }
    }
  } else if (mode == UiMode::Storage) {
    U.store = p.stores.empty() ? -1 : 0;
    storeSide_ = 0; packScroll_ = 0; storeScroll_ = 0;
  } else if (mode == UiMode::Cook) {
    cookNote_.clear(); cookNoteSel_ = -1;
    U.station = keep.mode == UiMode::Cook ? keep.station : home::Station::Campfire;
    cookScroll_ = 0;
  }
  g.mode = Mode::Build;
}

int View::homeHere(const Game& g, bool& indoors) const {
  indoors = false;
  if (g.home.plots.empty()) return -1;
  if (g.inside) {
    if (g.subBldg < 0 || g.subBldg >= (int)g.world.over.bldgs.size()) return -1;
    const Bldg& b = g.world.over.bldgs[(size_t)g.subBldg];
    for (int i = 0; i < (int)g.home.plots.size(); i++) {
      const home::Plot& p = g.home.plots[(size_t)i];
      ew::Gid id = p.id;
      if (p.kind == home::PlotKind::Lot) {
        if (!p.hw || p.state != home::PlotState::Built) continue;
        id = ew::makeId(ew::idRx(p.id), ew::idRy(p.id), ew::IdKind::Plot, 0xC00u | (ew::idLocal(p.id) & 0x3FFu));
      }
      if (b.id == id) { indoors = true; return i; }
    }
    return -1;
  }
  const Actor& a = g.pl();
  return g.home.plotAt(g.world.ox + (int)std::floor(a.p.x / TILE), g.world.oy + (int)std::floor((a.p.y - 2) / TILE));
}

// ---------------------------------------------------------------- the HUD's pills
namespace {
struct Pills { Rect yard, dismount; bool showYard = false, showRide = false; };
Pills pillLay(bool touch, bool yard, bool ride) {
  Pills P;
  P.showYard = yard; P.showRide = ride;
  const float B = (float)(Pix::H - Pix::SB), R = (float)(Pix::W - Pix::SR);
  const float h = touch ? 26.0f : 15.0f, w = touch ? 76.0f : 74.0f;
  // touch: right of the stick's half, left of the bow button (the right thumb's reach); desktop: above the notice line
  const float bowX = R - (480 - 374) - 14;
  float x = touch ? std::max(Pix::W * 0.55f + 4, bowX - 10 - w) : (float)Pix::SL + 6;
  float y = touch ? B - 8 - h : B - 6 - h;
  if (yard) { P.yard = {x, y, w, h}; y -= h + 5; }
  if (ride) P.dismount = {x, y, w, h};
  return P;
}
}  // namespace

void View::drawHomeHud(Game& g) {
  if (g.mode != Mode::Play || g.travelling()) return;
  Pix& P = *pix_;
  bool indoors = false;
  const int here = homeHere(g, indoors);
  const bool ride = g.homeRiding();
  if (here < 0 && !ride) return;
  const Pills L = pillLay(touchUI, here >= 0 && !ride, ride);
  auto pill = [&](const Rect& r, const std::string& key, const std::string& label) {
    P.rect(r.x + 1, r.y + 1, r.w, r.h, Color(0, 0, 0, 0.35f));
    P.rect(r.x, r.y, r.w, r.h, Color(0.12f, 0.09f, 0.07f, touchUI ? 0.82f : 0.7f));
    P.frame(r.x, r.y, r.w, r.h, Color(0.78f, 0.62f, 0.32f, 0.95f));
    P.rect(r.x + 1, r.y + 1, r.w - 2, 1, Color(1, 0.9f, 0.6f, 0.25f));
    if (touchUI) P.textS(r.x + r.w / 2, r.y + (r.h - 7) / 2, label, 1, kGoldC, 1);
    else {
      P.textS(r.x + 4, r.y + (r.h - 7) / 2, key, 1, kDimC);
      P.textS(r.x + 4 + P.textW(key, 1) + 4, r.y + (r.h - 7) / 2, label, 1, kGoldC);
    }
  };
  if (L.showYard) pill(L.yard, "B", indoors ? "DECORATE" : "YARD");
  if (L.showRide) pill(L.dismount, "F", "DISMOUNT");
}

bool View::homeHudTap(Game& g, Vec2 p) {
  if (g.mode != Mode::Play || g.travelling()) return false;
  bool indoors = false;
  const int here = homeHere(g, indoors);
  const bool ride = g.homeRiding();
  if (here < 0 && !ride) return false;
  const Pills L = pillLay(touchUI, here >= 0 && !ride, ride);
  auto hit = [&](const Rect& r) { return Rect{r.x - 3, r.y - 3, r.w + 6, r.h + 6}.in(p); };
  if (L.showRide && hit(L.dismount)) { g.homeDismount(home::Dismount::Player); audio_->play(Sfx::MenuBack); return true; }
  if (L.showYard && hit(L.yard)) { openBuild(g, indoors ? UiMode::Decorate : UiMode::Yard, here); audio_->play(Sfx::MenuSelect); return true; }
  return false;
}

bool View::homeHudKey(Game& g, int key) {
  if (g.mode != Mode::Play || g.travelling()) return false;
  if (key == SDLK_F && g.homeRiding()) { g.homeDismount(home::Dismount::Player); audio_->play(Sfx::MenuBack); return true; }
  if (key == SDLK_B && !g.homeRiding()) {
    bool indoors = false;
    const int here = homeHere(g, indoors);
    if (here < 0) return false;
    openBuild(g, indoors ? UiMode::Decorate : UiMode::Yard, here);
    audio_->play(Sfx::MenuSelect);
    return true;
  }
  return false;
}

// ---------------------------------------------------------------- the camera while building
bool View::buildCamWant(Game& g, Vec2& want) {
  const home::Ui& U = g.home.ui;
  if (U.mode != UiMode::Yard && U.mode != UiMode::Decorate) return false;
  if (U.plot < 0 || U.plot >= (int)g.home.plots.size()) return false;
  const home::Plot& p = g.home.plots[(size_t)U.plot];
  const SideLay L = sideLay(touchUI);
  const float freeX0 = (float)Pix::SL, freeX1 = L.panel.x - 4;
  Vec2 c;
  if (U.mode == UiMode::Yard) c = Vec2((p.gx - g.world.ox + U.gx) * 16.0f + 8, (p.gy - g.world.oy + U.gy) * 16.0f + 8);
  else c = Vec2(U.gx * 16.0f + 8, U.gy * 16.0f + 8);
  // keep the ghost in the middle of the free part of the screen, nudged toward the plot's middle so its edges show
  if (U.mode == UiMode::Yard) {
    const Vec2 mid((p.gx - g.world.ox + p.w * 0.5f) * 16.0f, (p.gy - g.world.oy + p.h * 0.5f) * 16.0f);
    c = c * 0.65f + mid * 0.35f;
  }
  want = Vec2(c.x - (freeX0 + freeX1) * 0.5f, c.y - (float)(Pix::ST + Pix::H - Pix::SB) * 0.5f);
  return true;
}

// ---------------------------------------------------------------- the open plot
// The plot the screen works on. COOK needs none (an inn's kitchen, a campfire on the road: ui.plot == -1), so it
// gets an empty stand-in plot; every other screen without a plot shows NO PROPERTY HERE / closes.
namespace {
const home::Plot* uiPlot(const Game& g) {
  static const home::Plot kNoPlot{};
  const home::Ui& U = g.home.ui;
  if (U.plot >= 0 && U.plot < (int)g.home.plots.size()) return &g.home.plots[(size_t)U.plot];
  return U.mode == UiMode::Cook ? &kNoPlot : nullptr;
}
}  // namespace

// ---------------------------------------------------------------- drawing
void View::drawBuild(Game& g) {
  Pix& P = *pix_;
  home::Ui& U = g.home.ui;
  const bool touch = touchUI;
  if (!uiPlot(g)) {
    const Rect M = modalRect();
    panel(M.x + M.w / 2 - 110, M.y + M.h / 2 - 24, 220, 48);
    P.text(M.x + M.w / 2, M.y + M.h / 2 - 12, "NO PROPERTY HERE", 1, kDimC, 1);
    P.text(M.x + M.w / 2, M.y + M.h / 2 + 2, touch ? "TAP TO CLOSE" : "ESC TO CLOSE", 1, kDimC, 1);
    return;
  }
  const home::Plot& p = *uiPlot(g);
  auto gold = [&](float rx, float y) {   // the purse at the right end of a header row
    const std::string s = std::to_string(g.gold);
    const float w = (float)P.textW(s, 1);
    P.textS(rx - w, y, s, 1, kGoldC);
    P.blit(iconTex(art::Icon::Gold, 0), rx - w - 17, y - 5);
  };
  auto btn = [&](const Rect& r, const std::string& label, bool hot, bool enabled = true) {
    if (!enabled) {
      P.rect(r.x, r.y, r.w, r.h, Color(0.10f, 0.09f, 0.09f, 0.9f));
      P.frame(r.x, r.y, r.w, r.h, Color(0.30f, 0.26f, 0.20f));
      P.text(r.x + r.w / 2, r.y + (r.h - 7) / 2, label, 1, Color(0.42f, 0.40f, 0.36f), 1);
      return;
    }
    button(r.x, r.y, r.w, r.h, label, hot);
    P.rect(r.x + 1, r.y + 1, r.w - 2, 1, Color(1, 0.92f, 0.7f, hot ? 0.35f : 0.12f));
  };
  // a scroll button: a small solid triangle (the font has no caret)
  auto arrowBtn = [&](const Rect& r, bool up, bool enabled) {
    btn(r, "", false, enabled);
    const Color c = enabled ? kTextC : Color(0.42f, 0.40f, 0.36f);
    const float cx = std::floor(r.x + r.w / 2), cy = std::floor(r.y + r.h / 2);
    for (int k = 0; k < 4; k++) {
      const float yy = up ? cy - 2 + k : cy + 1 - k;
      P.rect(cx - k, yy, (float)(2 * k + 1), 1, c);
    }
  };
  // ================================================================ YARD / DECORATE
  if (U.mode == UiMode::Yard || U.mode == UiMode::Decorate) {
    const bool inside = U.mode == UiMode::Decorate;
    const std::vector<Cat>& cs = cats(inside);
    if (catOf(inside, U.sel) >= 0) buildCat_ = catOf(inside, U.sel);
    buildCat_ = std::clamp(buildCat_, 0, (int)cs.size() - 1);
    const SideLay L = sideLay(touch);
    panel(L.panel.x, L.panel.y, L.panel.w, L.panel.h, 0.94f);
    P.textS(L.panel.x + 7, L.panel.y + 6, inside ? "DECORATE" : "YARD", 1, kGoldC);
    gold(L.panel.x + L.panel.w - 7, L.panel.y + 6);
    for (int i = 0; i < (int)cs.size() && i < 5; i++) {
      const Rect& r = L.tabs[i];
      const bool on = i == buildCat_;
      P.rect(r.x, r.y, r.w, r.h, on ? Color(0.34f, 0.25f, 0.13f, 0.98f) : Color(0.13f, 0.11f, 0.10f, 0.95f));
      P.frame(r.x, r.y, r.w, r.h, on ? kGoldC : Color(0.40f, 0.33f, 0.22f));
      if (on) P.rect(r.x + 1, r.y + r.h - 2, r.w - 2, 2, kGoldC);
      P.text(r.x + r.w / 2, r.y + (r.h - 7) / 2, cs[(size_t)i].tab, 1, on ? Color(1, 0.95f, 0.8f) : kDimC, 1);
    }
    // the grid of pieces
    const Cat& cat = cs[(size_t)buildCat_];
    const cult::Culture* C = nullptr;
    if (g.world.src) {
      uint64_t cid = p.style;
      if (!cid) {
        const int si = p.site ? g.world.siteHandle(p.site) : -1;
        if (si >= 0) if (const cult::Culture* sc = g.world.cultureOf(si)) cid = sc->id;
      }
      if (cid) C = &g.world.src->culture(cid);
    }
    for (int i = 0; i < (int)cat.objs.size() && i < L.cols * L.rows; i++) {
      const Rect r = cellRect(L, i);
      const Obj o = cat.objs[(size_t)i];
      const home::ObjInfo& oi = home::objInfo(o);
      const bool sel = (int)o == U.sel;
      const bool afford = g.gold >= oi.price;
      P.rect(r.x, r.y, r.w, r.h, sel ? Color(0.30f, 0.23f, 0.13f, 0.98f) : Color(0.15f, 0.13f, 0.12f, 0.95f));
      P.rect(r.x + 1, r.y + 1, r.w - 2, r.h - 20, sel ? Color(0.40f, 0.36f, 0.26f, 0.6f) : Color(0.26f, 0.30f, 0.20f, 0.45f));   // a grassy plate
      P.frame(r.x, r.y, r.w, r.h, sel ? kGoldC : Color(0.36f, 0.30f, 0.21f));
      // its picture: the yard object in the plot's style (or the furniture's prop), at 1x when it fits, else halved
      const Tex* t = nullptr;
      int sw = 0, sh = 0;
      const art::FarmObj fa = home::farmObjArt(o);
      if (fa != art::FarmObj::COUNT) {
        const uint64_t key = (0xB1ull << 56) | (uint64_t)fa << 40 | (C ? (C->id & 0xFFFFFFFFull) : 0);
        auto it = homeTex_.find(key);
        if (it == homeTex_.end()) {
          art::FarmObjLook lk;
          lk.kind = fa;
          lk.style = C ? &C->arch : nullptr;
          if (fa == art::FarmObj::Fence || fa == art::FarmObj::Gate) lk.joins = 2 | 8;
          if (oi.flags & home::OBJ_ANIMALS) lk.stage = 2;
          it = homeTex_.emplace(key, P.bake(art::farmObjSprite(lk))).first;
        }
        t = &it->second; sw = t->w; sh = t->h;
      } else if (o == Obj::Path || o == Obj::Farmland) {
        const uint64_t key = (0xB2ull << 56) | (uint64_t)o;
        auto it = homeTex_.find(key);
        if (it == homeTex_.end()) {
          Canvas c(32, 16);
          const Canvas a = o == Obj::Path ? art::pathTile(2, 0, 1) : art::farmlandTile(2, false, 1);
          const Canvas b = o == Obj::Path ? art::pathTile(8, 0, 2) : art::farmlandTile(8, false, 2);
          for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) { c.set(x, y, a.px[(size_t)y * a.w + x]); c.set(16 + x, y, b.px[(size_t)y * b.w + x]); }
          it = homeTex_.emplace(key, P.bake(c)).first;
        }
        t = &it->second; sw = t->w; sh = t->h;
      } else if (o == Obj::Trophy) {
        const uint64_t key = (0xB3ull << 56) | 1;
        auto it = homeTex_.find(key);
        if (it == homeTex_.end()) it = homeTex_.emplace(key, P.bake(art::trophySprite(art::Monster::Wolf, 0))).first;
        t = &it->second; sw = t->w; sh = t->h;
      } else if (oi.prop != art::Prop::COUNT && (size_t)oi.prop < props_.size()) {
        t = &props_[(size_t)oi.prop]; sw = art::propW(oi.prop); sh = t->h;
      }
      if (t) {
        const float fitW = r.w - 4, fitH = r.h - 22;
        float sc = 1.0f;
        while ((sw * sc > fitW || sh * sc > fitH) && sc > 0.3f) sc *= 0.5f;
        const float dw = sw * sc, dh = sh * sc;
        const float dx = std::floor(r.x + (r.w - dw) / 2), dy = std::floor(r.y + 2 + fitH - dh);
        P.blitEx(*t, 0, 0, sw, sh, dx, dy, dw, dh, false, Color(1, 1, 1, afford ? 1.0f : 0.5f));
      }
      const std::string pr = oi.price ? std::to_string(oi.price) : "FREE";
      P.rect(r.x + 1, r.y + r.h - 19, r.w - 2, 18, Color(0.05f, 0.04f, 0.05f, 0.75f));
      P.text(r.x + r.w / 2, r.y + r.h - 18, fitText(cardLabel(o), r.w - 2), 1, sel ? Color(1, 0.95f, 0.8f) : kDimC, 1);   // its name
      P.text(r.x + r.w / 2, r.y + r.h - 9, pr, 1, afford ? (sel ? kGoldC : kTextC) : kBadC, 1);
    }
    // the selected piece
    const Obj so = (Obj)std::clamp(U.sel, 0, (int)Obj::COUNT - 1);
    const home::ObjInfo& si = home::objInfo(so);
    int fw = si.w, fh = si.h;
    if (U.turned && (si.flags & home::OBJ_ROTATES)) std::swap(fw, fh);
    std::string why;
    bool fits = false;
    if (inside) fits = home::canPlaceInside(p, g.map(), so, U.gx, U.gy, U.turned, why);
    else fits = home::canPlaceOutside(p, so, U.gx, U.gy, U.turned, why);
    if (fits && g.gold < si.price) { fits = false; why = "IT COSTS " + std::to_string(si.price) + " GOLD"; }
    const float iy = L.info.y;
    P.textS(L.info.x, iy, fitText(si.name, L.info.w - 40), 1, kGoldC);
    P.text(L.info.x + L.info.w, iy, std::to_string(fw) + "X" + std::to_string(fh), 1, kDimC, 2);
    const std::string shown = !U.why.empty() ? U.why : why;
    const bool placed = U.justPlaced() && U.why.empty();
    if (placed) {
      P.text(L.info.x, iy + 11, "PLACED", 1, kGoodC);
      P.text(L.info.x, iy + 20, touch ? "TAP THE GROUND FOR ANOTHER" : "MOVE FOR ANOTHER", 1, kDimC);
    } else if (fits && U.why.empty()) P.text(L.info.x, iy + 11, "FITS HERE", 1, kGoodC);
    else if (!shown.empty()) {
      // the refusal in red, wrapped over two lines
      const int per = std::max(6, (int)(L.info.w / 6));
      std::string a = shown, b;
      if ((int)a.size() > per) {
        size_t cut = a.rfind(' ', (size_t)per);
        if (cut == std::string::npos || cut == 0) cut = (size_t)per;
        b = a.substr(cut + (a[cut] == ' ' ? 1 : 0));
        a = a.substr(0, cut);
      }
      P.text(L.info.x, iy + 11, a, 1, kBadC);
      if (!b.empty()) P.text(L.info.x, iy + 20, fitText(b, L.info.w), 1, kBadC);
    }
    const int under = objAtTile(g, p, U.gx, U.gy, inside);
    const bool groundHere = !inside && home::groundAt(p, U.gx, U.gy) != 0;
    std::string sellLabel = "SELL";
    if (under >= 0) {
      const int refund = home::objInfo((Obj)(inside ? p.inside : p.outside)[(size_t)under].kind).price / 2;
      sellLabel = refund > 0 ? "SELL +" + std::to_string(refund) : "REMOVE";
    } else if (groundHere) sellLabel = "CLEAR";
    btn(L.rot, "ROTATE", false, (si.flags & home::OBJ_ROTATES) != 0);
    btn(L.sell, sellLabel, false, under >= 0 || groundHere);
    btn(L.place, "PLACE " + std::to_string(si.price), fits, fits);
    btn(L.done, "DONE", false);
    // a hint over the world, once: how to move the ghost (gone for good after the first move or placement)
    if (buildHintGx_ == INT32_MIN) { buildHintGx_ = U.gx; buildHintGy_ = U.gy; }
    else if (U.gx != buildHintGx_ || U.gy != buildHintGy_) buildHintDone_ = true;
    const float hx = ((float)Pix::SL + L.panel.x) / 2;   // (at the top: the bottom is where the yard's gate usually is)
    const std::string hint = touch ? "TAP OR DRAG ON THE GROUND TO MOVE" : "ARROWS MOVE   Q E PICK   R TURN   ENTER PLACE";
    const float hw = (float)P.textW(hint, 1) + 10;
    if (!buildHintDone_ && U.placedKey < 0 && hw < L.panel.x - Pix::SL - 8) {
      const float hy = (float)Pix::ST + 7;
      P.rect(hx - hw / 2, hy - 3, hw, 13, Color(0.04f, 0.03f, 0.05f, 0.6f));
      P.text(hx, hy, hint, 1, Color(0.95f, 0.9f, 0.8f, 0.9f), 1);
    }
    return;
  }
  // ================================================================ the modal screens
  const Rect M = modalRect();
  P.rect(0, 0, (float)Pix::W, (float)Pix::H, Color(0, 0, 0, 0.35f));
  panel(M.x, M.y, M.w, M.h, 0.95f);
  // ---------------------------------------------------------------- SHELL
  if (U.mode == UiMode::Shell) {
    const ShellLay S = shellLay(touch);
    P.textS(M.x + 8, M.y + 6, fitText("BUILD A HOUSE: " + plotName(g, p), M.w - 120), 1, kGoldC);
    gold(M.x + M.w - 8, M.y + 6);
    U.sel = std::clamp(U.sel, 0, (int)home::Shell::COUNT - 1);
    for (int i = 0; i < (int)home::Shell::COUNT && i < 5; i++) {
      const home::ShellInfo& si = home::shellInfo((home::Shell)i);
      const Rect& r = S.card[i];
      const bool on = i == U.sel, fitsLot = lotFits(p, (home::Shell)i);
      P.rect(r.x, r.y, r.w, r.h, on ? Color(0.32f, 0.24f, 0.13f, 0.98f) : Color(0.14f, 0.12f, 0.11f, 0.95f));
      P.frame(r.x, r.y, r.w, r.h, on ? kGoldC : Color(0.38f, 0.31f, 0.21f));
      if (on) P.rect(r.x + 1, r.y + 1, 2, r.h - 2, kGoldC);
      // a little plan of the footprint at the card's right: one cell per tile, a second tone per extra storey
      const float cs = 3.0f, px0 = r.x + r.w - 6 - si.w * cs, py0 = r.y + (r.h - si.h * cs) / 2;
      for (int y = 0; y < si.h; y++)
        for (int x = 0; x < si.w; x++)
          P.rect(px0 + x * cs, py0 + y * cs, cs - 1, cs - 1, fitsLot ? (si.storeys > 1 ? Color(0.86f, 0.62f, 0.36f) : Color(0.74f, 0.58f, 0.40f)) : Color(0.4f, 0.36f, 0.32f));
      const Color nameC = fitsLot ? (on ? Color(1, 0.95f, 0.8f) : kTextC) : kDimC;
      P.text(r.x + 7, r.y + 4, si.name, 1, nameC);
      const std::string l2 = std::to_string(si.w) + "X" + std::to_string(si.h) + "  " + std::to_string(si.storeys) + (si.storeys > 1 ? " STOREYS" : " STOREY");
      P.text(r.x + 7, r.y + 4 + (r.h > 30 ? 10.0f : 9.0f), l2, 1, kDimC);
      // (integration) the price the builders really ask: the settlement's priceFactor, less the materials brought
      const int price = home::buildPrice(g, U.plot, (home::Shell)i, U.timber, U.iron);
      const std::string l3 = fitsLot ? std::to_string(price) + " G  " + std::to_string(si.days) + " DAYS" : "NEEDS A BIGGER LOT";
      P.text(r.x + 7, r.y + 4 + (r.h > 30 ? 20.0f : 18.0f), l3, 1, fitsLot ? (g.gold >= price ? kGoldC : kBadC) : kBadC);
    }
    // the preview: the builder's design of this shell in this style, drawn on a patch of the lot's ground
    const Rect& pv = S.preview;
    P.rect(pv.x, pv.y, pv.w, pv.h, Color(0.20f, 0.27f, 0.16f, 1));
    for (int k = 0; k < (int)(pv.h / 4); k++)   // a soft grassy dither, darker at the foot
      P.rect(pv.x, pv.y + k * 4, pv.w, 2, Color(0.24f, 0.32f, 0.19f, 0.55f + 0.02f * (k % 3)));
    P.frame(pv.x, pv.y, pv.w, pv.h, Color(0.45f, 0.37f, 0.24f));
    uint64_t style = U.style;
    if (!style && !g.home.styles.empty()) style = g.home.styles.front();
    if (g.home.styles.empty()) P.text(pv.x + pv.w / 2, pv.y + pv.h / 2 - 3, "NO STYLES KNOWN", 1, kDimC, 1);
    else {
      home::Plot q = p;
      q.shell = (uint8_t)U.sel;
      q.style = style;
      const home::ShellInfo& si = home::shellInfo((home::Shell)U.sel);
      q.hw = si.w; q.hh = si.h; q.hx = 0; q.hy = 0;
      q.state = home::PlotState::Built;
      if (!q.houseSeed) q.houseSeed = (uint32_t)ew::mix64(p.id ^ 0x5348454C4Cull);
      const uint64_t key = (0xB4ull << 56) ^ ((uint64_t)U.sel << 48) ^ (style & 0xFFFFFFFFFFFull) ^ ((uint64_t)q.houseSeed << 20);
      auto it = homeTex_.find(key);
      if (it == homeTex_.end()) {
        const Bldg b = home::houseBldg(q, g);
        it = homeTex_.emplace(key, P.bake(art::buildingSprite(bld::design(bldgRequest(b))))).first;
      }
      const Tex& t = it->second;
      float sc = 1.0f;
      if (t.w * 2 <= pv.w - 16 && t.h * 2 <= pv.h - 12) sc = 2.0f;
      while ((t.w * sc > pv.w - 8 || t.h * sc > pv.h - 6) && sc > 0.3f) sc *= 0.75f;
      const float dw = std::floor(t.w * sc), dh = std::floor(t.h * sc);
      const float dx = std::floor(pv.x + (pv.w - dw) / 2), dy = std::floor(pv.y + pv.h - 10 - dh);
      // its ground shadow, thrown down-right
      const float sw = dw * 0.9f, shh = std::max(6.0f, dh * 0.12f);
      P.blitEx(shadowBig_, 0, 0, 40, 12, dx + (dw - sw) / 2 + 4 * sc, dy + dh - shh * 0.75f - art::BLDG_PAD_B * sc, sw, shh, false, Color(1, 1, 1, 0.75f));
      P.blitEx(t, 0, 0, t.w, t.h, dx, dy, dw, dh, false, Color(1, 1, 1, 1));
    }
    // the style picker
    const Rect& sb = S.styleBar;
    P.rect(sb.x, sb.y, sb.w, sb.h, Color(0.12f, 0.10f, 0.09f, 0.95f));
    P.frame(sb.x, sb.y, sb.w, sb.h, Color(0.42f, 0.34f, 0.22f));
    btn(S.prev, "<", false, g.home.styles.size() > 1);
    btn(S.next, ">", false, g.home.styles.size() > 1);
    if (!g.home.styles.empty()) {
      int idx = 0;
      for (int i = 0; i < (int)g.home.styles.size(); i++) if (g.home.styles[(size_t)i] == style) idx = i;
      std::string nm = "HEARTLAND";
      if (g.world.src) {
        const cult::Culture& Cc = g.world.src->culture(style);
        nm = Cc.adjective.empty() ? Cc.name : Cc.adjective;
      }
      const std::string s = nm + " STYLE  " + std::to_string(idx + 1) + "/" + std::to_string(g.home.styles.size());
      P.text(sb.x + sb.w / 2, sb.y + (sb.h - 7) / 2, fitText(s, sb.w - S.prev.w * 2 - 8), 1, kTextC, 1);
    }
    const home::ShellInfo& si = home::shellInfo((home::Shell)U.sel);
    const int price = home::buildPrice(g, U.plot, (home::Shell)U.sel, U.timber, U.iron);
    const bool can = lotFits(p, (home::Shell)U.sel) && g.gold >= price && !g.home.styles.empty() && p.kind == home::PlotKind::Lot &&
                     p.state == home::PlotState::Owned;
    btn(S.build, "BUILD " + std::to_string(price), can, can);
    btn(S.cancel, "CANCEL", false);
    std::string why = U.why;
    if (why.empty() && p.kind == home::PlotKind::Lot && p.state != home::PlotState::Owned) why = "A HOUSE STANDS THERE ALREADY";
    if (why.empty() && !lotFits(p, (home::Shell)U.sel)) why = std::string("A ") + si.name + " NEEDS A BIGGER LOT";
    if (why.empty() && g.gold < price) why = "THE BUILDERS ASK " + std::to_string(price) + " GOLD";
    if (!why.empty()) P.text(S.card[0].x, S.build.y + (S.build.h - 7) / 2, fitText(why, S.build.x - S.card[0].x - 6), 1, kBadC);
    return;
  }
  // ---------------------------------------------------------------- STORAGE
  if (U.mode == UiMode::Storage) {
    const ListLay S = listLay(touch);
    // (M7 fix r3, review: "the gold is covered by the store's scroll-down button in touch mode") the purse sits left
    // of the store column's arrows when they are the tall touch ones (they reach up into the header row)
    const float purseR = touch ? S.up[1].x - 8 : M.x + M.w - 8;
    P.textS(M.x + 8, M.y + 6, fitText("STORAGE: " + plotName(g, p), purseR - 80 - (M.x + 8)), 1, kGoldC);
    gold(purseR, M.y + 6);
    const bool hasStore = U.store >= 0 && U.store < (int)p.stores.size();
    const std::vector<Item> none;
    const std::vector<Item>& st = hasStore ? p.stores[(size_t)U.store].items : none;
    // the store's name: the object holding it
    std::string sname = "HOUSE STORE";
    for (const home::PlacedObj& o : p.inside) if ((home::objInfo((Obj)o.kind).flags & home::OBJ_STORAGE) && (int)o.data == U.store) sname = home::objInfo((Obj)o.kind).name;
    for (const home::PlacedObj& o : p.outside) if ((home::objInfo((Obj)o.kind).flags & home::OBJ_STORAGE) && (int)o.data == U.store) sname = home::objInfo((Obj)o.kind).name;
    const std::string heads[2] = {"PACK " + std::to_string(g.inv.size()), sname + " " + std::to_string(st.size()) + "/" + std::to_string(home::STORE_STACKS)};
    int* scrolls[2] = {&packScroll_, &storeScroll_};
    const int counts[2] = {(int)g.inv.size(), (int)st.size()};
    for (int c = 0; c < 2; c++) {
      const Rect& r = S.col[c];
      *scrolls[c] = std::clamp(*scrolls[c], 0, std::max(0, counts[c] - S.rows));
      P.textS(r.x + 2, M.y + 21, heads[c], 1, c == storeSide_ ? kGoldC : kTextC);
      arrowBtn(S.up[c], true, *scrolls[c] > 0);
      arrowBtn(S.down[c], false, *scrolls[c] + S.rows < counts[c]);
      P.rect(r.x, r.y, r.w, r.h, Color(0.10f, 0.09f, 0.08f, 0.9f));
      P.frame(r.x, r.y, r.w, r.h, Color(0.36f, 0.30f, 0.21f));
      const std::vector<Item>& items = c == 0 ? g.inv : st;
      for (int k = 0; k < S.rows; k++) {
        const int i = *scrolls[c] + k;
        if (i >= (int)items.size()) break;
        const Item& it = items[(size_t)i];
        const float ry = r.y + k * S.rowH;
        const bool sel = c == storeSide_ && i == storeSel_;
        if (sel) { P.rect(r.x + 1, ry + 1, r.w - 2, S.rowH - 1, Color(0.32f, 0.24f, 0.13f, 0.95f)); P.frame(r.x + 1, ry + 1, r.w - 2, S.rowH - 1, kGoldC); }
        else if (k & 1) P.rect(r.x + 1, ry + 1, r.w - 2, S.rowH - 1, Color(1, 1, 1, 0.03f));
        P.blit(itemTex(g, it), r.x + 3, ry + (S.rowH - 16) / 2);
        bool worn = false;
        if (c == 0) for (int w : g.worn()) if (w == i) worn = true;
        const std::string cnt = it.stackable() && it.count > 1 ? " X" + std::to_string(it.count) : "";
        P.text(r.x + 22, ry + (S.rowH - 7) / 2, fitText(it.name + cnt, r.w - 26 - (worn ? 14 : 0)), 1, it.kind == ItemKind::Quest ? kDimC : kTextC);
        if (worn) P.text(r.x + r.w - 4, ry + (S.rowH - 7) / 2, "E", 1, kGoldC, 2);
      }
      if (items.empty()) P.text(r.x + r.w / 2, r.y + r.h / 2 - 3, c == 0 ? "YOUR PACK IS EMPTY" : hasStore ? "EMPTY" : "NO STORE HERE", 1, kDimC, 1);
    }
    // the plot's other stores
    if (p.stores.size() > 1) {
      btn(S.prev, "<", false);
      btn(S.next, ">", false);
      // (M7 fixer r2) the pager says what it pages through: the plot's stores (chests, shelves, the crate)
      // (M7 fix r3, review: "'STORE 2/24' over '< STORE 1/2 >'") the header names the container and its fill; the
      // pager only which of them this is
      P.text(S.prev.x + S.prev.w + 30, S.prev.y + (S.prev.h - 7) / 2, std::to_string(U.store + 1) + " OF " + std::to_string(p.stores.size()), 1, kDimC, 1);
    }
    P.text(M.x + 8, S.done.y + (S.done.h - 7) / 2, !U.why.empty() ? fitText(U.why, S.col[1].x + 12 - M.x - 8) : touch ? "TAP AN ITEM TO MOVE IT" : "ENTER MOVES IT",
           1, !U.why.empty() ? kBadC : kDimC);
    btn(S.done, "DONE", false);
    return;
  }
  // ---------------------------------------------------------------- COOK
  if (U.mode == UiMode::Cook) {
    const CookLay S = cookLay(touch);
    const std::vector<home::Meal> meals = home::knownMeals(g);
    P.textS(M.x + 8, M.y + 6, std::string("COOK AT THE ") + stationName(U.station), 1, kGoldC);
    gold(M.x + M.w - 8, M.y + 6);
    cookScroll_ = std::clamp(cookScroll_, 0, std::max(0, (int)meals.size() - S.rows));
    U.sel = std::clamp(U.sel, 0, std::max(0, (int)meals.size() - 1));
    arrowBtn(S.up, true, cookScroll_ > 0);
    arrowBtn(S.down, false, cookScroll_ + S.rows < (int)meals.size());
    P.rect(S.list.x, S.list.y, S.list.w, S.list.h, Color(0.10f, 0.09f, 0.08f, 0.9f));
    P.frame(S.list.x, S.list.y, S.list.w, S.list.h, Color(0.36f, 0.30f, 0.21f));
    auto canCook = [&](const home::Meal& m) {
      int need[(int)home::Ingr::COUNT] = {};
      for (home::Ingr i : m.in) if (i != home::Ingr::None) need[(int)i]++;
      for (int k = 1; k < (int)home::Ingr::COUNT; k++) if (need[k] && heldOf(g, (home::Ingr)k) < need[k]) return false;
      return true;
    };
    for (int k = 0; k < S.rows; k++) {
      const int i = cookScroll_ + k;
      if (i >= (int)meals.size()) break;
      const home::Meal& m = meals[(size_t)i];
      const float ry = S.list.y + k * S.rowH;
      const bool sel = i == U.sel, ok = canCook(m);
      if (sel) { P.rect(S.list.x + 1, ry + 1, S.list.w - 2, S.rowH - 1, Color(0.32f, 0.24f, 0.13f, 0.95f)); P.frame(S.list.x + 1, ry + 1, S.list.w - 2, S.rowH - 1, kGoldC); }
      else if (k & 1) P.rect(S.list.x + 1, ry + 1, S.list.w - 2, S.rowH - 1, Color(1, 1, 1, 0.03f));
      const float ty = ry + (S.rowH - 7) / 2;
      int nIn = 0;
      for (home::Ingr in : m.in) if (in != home::Ingr::None) nIn++;
      const float iconsW = nIn * 17.0f;
      P.text(S.list.x + 5, ty, fitText(m.name, S.list.w - 12 - iconsW), 1, ok ? (sel ? Color(1, 0.95f, 0.8f) : kTextC) : kDimC);
      if (m.culture) P.rect(S.list.x + 2, ry + 3, 1, S.rowH - 5, kGoldC);   // a culture's signature dish
      float ix = S.list.x + S.list.w - 3 - iconsW;
      for (home::Ingr in : m.in) {
        if (in == home::Ingr::None) continue;
        uint32_t tint = 0;
        const art::Icon ic = ingrIcon(in, tint);
        const bool have = heldOf(g, in) > 0;
        P.blitEx(iconTex(ic, tint), 0, 0, 16, 16, ix, ry + (S.rowH - 16) / 2, 16, 16, false, Color(1, 1, 1, have ? 1.0f : 0.35f));
        if (!have) P.rect(ix + 3, ry + S.rowH / 2, 10, 1, kBadC);
        ix += 17;
      }
    }
    if (meals.empty()) P.text(S.list.x + S.list.w / 2, S.list.y + S.list.h / 2, "NO RECIPES KNOWN", 1, kDimC, 1);
    // the selected meal
    const Rect& d = S.detail;
    P.rect(d.x, d.y, d.w, d.h - 34, Color(0.12f, 0.10f, 0.09f, 0.9f));
    P.frame(d.x, d.y, d.w, d.h - 34, Color(0.36f, 0.30f, 0.21f));
    if (!meals.empty()) {
      const home::Meal& m = meals[(size_t)U.sel];
      const Item out = home::makeMeal(m, U.station);
      float fed = 0;
      uint8_t q = 0;
      home::mealBuff(out, fed, q);
      P.blitEx(itemTex(g, out), 0, 0, 16, 16, d.x + 8, d.y + 8, 32, 32, false, Color(1, 1, 1, 1));
      P.textS(d.x + 48, d.y + 9, fitText(m.name, d.w - 54), 1, kGoldC);
      for (int s = 0; s < 3; s++) {   // the quality as three pips
        const float sx = d.x + 48 + s * 9, sy = d.y + 21;
        P.rect(sx, sy, 7, 7, Color(0.05f, 0.04f, 0.04f));
        P.rect(sx + 1, sy + 1, 5, 5, s < q ? kGoldC : Color(0.25f, 0.22f, 0.20f));
        if (s < q) P.rect(sx + 1, sy + 1, 2, 1, Color(1, 1, 0.85f));
      }
      P.text(d.x + 78, d.y + 21, qualityWord(q), 1, q >= 2 ? kGoldC : kTextC);
      P.text(d.x + 48, d.y + 32, "HEALS " + std::to_string(out.power) + "  WELL FED " + std::to_string((int)std::lround(fed)) + "H", 1, kDimC);
      float y = d.y + 46;
      P.text(d.x + 8, y, "NEEDS", 1, kDimC);
      y += 16;
      int need[(int)home::Ingr::COUNT] = {};
      for (home::Ingr in : m.in) if (in != home::Ingr::None) need[(int)in]++;
      for (int k = 1; k < (int)home::Ingr::COUNT; k++) {
        if (!need[k]) continue;
        uint32_t tint = 0;
        const art::Icon ic = ingrIcon((home::Ingr)k, tint);
        const int have = heldOf(g, (home::Ingr)k);
        P.blit(iconTex(ic, tint), d.x + 8, y - 4);
        P.text(d.x + 28, y, ingrName((home::Ingr)k), 1, kTextC);
        // (M7 fixer r2, review: "'FLOUR 3/1' reads backwards") what it takes, then what the pack holds
        P.text(d.x + d.w - 8, y, "NEED " + std::to_string(need[k]) + "  HAVE " + std::to_string(std::min(have, 99)), 1, have >= need[k] ? kGoodC : kBadC, 2);
        y += 17;
      }
      // (M7 fixer r2, review: "'INN KITCHEN: THE BEST' beside a FINE meal") the station's own bonus, as makeMeal gives it
      const std::string st = std::string(stationName(U.station)) + (U.station == home::Station::Campfire ? ": PLAIN FARE, NO BONUS" : U.station == home::Station::InnKitchen ? ": +2 QUALITY" : ": +1 QUALITY");
      P.text(d.x + 8, d.y + d.h - 34 - 11, fitText(st, d.w - 16), 1, kDimC);
      const bool ok = canCook(m);
      btn(S.cook, "COOK", ok, ok);
    } else btn(S.cook, "COOK", false, false);
    if (!U.why.empty()) P.text(d.x, S.done.y - 10, fitText(U.why, d.w), 1, kBadC);
    else if (!cookNote_.empty() && cookNoteSel_ == U.sel && t_ - cookNoteT_ < 4.0f) P.text(d.x + 8, d.y + d.h - 34 - 23, fitText(cookNote_, d.w - 16), 1, kGoodC);   // (in the panel, over the station's line)
    btn(S.done, "DONE", false);
    return;
  }
  // ---------------------------------------------------------------- BUY (and anything else): what the sim says, DONE
  P.textS(M.x + 8, M.y + 6, fitText(plotName(g, p), M.w - 120), 1, kGoldC);
  gold(M.x + M.w - 8, M.y + 6);
  if (!U.why.empty()) wrapText(M.x + 12, M.y + 30, M.w - 24, U.why, kTextC);
  const ListLay S = listLay(touch);
  btn(S.done, "DONE", false);
}

// ---------------------------------------------------------------- input
namespace {
void closeBuild(Game& g) { g.mode = Mode::Play; g.home.ui.mode = UiMode::None; g.home.ui.why.clear(); }
}  // namespace

void View::buildKey(Game& g, int key) {
  home::Ui& U = g.home.ui;
  if (key == SDLK_ESCAPE || key == SDLK_TAB) { closeBuild(g); audio_->play(Sfx::MenuBack); return; }
  if (!uiPlot(g)) return;
  const home::Plot& p = *uiPlot(g);
  if (U.mode == UiMode::Yard || U.mode == UiMode::Decorate) {
    const bool inside = U.mode == UiMode::Decorate;
    const int mw = inside ? g.map().w : p.w, mh = inside ? g.map().h : p.h;
    const std::vector<Cat>& cs = cats(inside);
    const int cat = std::max(0, catOf(inside, U.sel));
    auto pickCat = [&](int c) {
      buildCat_ = (c + (int)cs.size()) % (int)cs.size();
      U.sel = (int)cs[(size_t)buildCat_].objs.front();
      U.why.clear();
      audio_->play(Sfx::MenuMove);
    };
    if (key == SDLK_LEFT || key == SDLK_A) { U.gx = std::max(0, U.gx - 1); U.why.clear(); }
    if (key == SDLK_RIGHT || key == SDLK_D) { U.gx = std::min(mw - 1, U.gx + 1); U.why.clear(); }
    if (key == SDLK_UP || key == SDLK_W) { U.gy = std::max(0, U.gy - 1); U.why.clear(); }
    if (key == SDLK_DOWN || key == SDLK_S) { U.gy = std::min(mh - 1, U.gy + 1); U.why.clear(); }
    if (key >= SDLK_1 && key <= SDLK_5) pickCat(key - SDLK_1);
    if (key == SDLK_PAGEUP) pickCat(cat - 1);
    if (key == SDLK_PAGEDOWN) pickCat(cat + 1);
    if (key == SDLK_Q || key == SDLK_E) {
      const std::vector<Obj>& v = cs[(size_t)cat].objs;
      int i = 0;
      for (int k = 0; k < (int)v.size(); k++) if ((int)v[(size_t)k] == U.sel) i = k;
      i = (i + (key == SDLK_E ? 1 : -1) + (int)v.size()) % (int)v.size();
      U.sel = (int)v[(size_t)i];
      U.why.clear();
      audio_->play(Sfx::MenuMove);
    }
    if (key == SDLK_R && (home::objInfo((Obj)U.sel).flags & home::OBJ_ROTATES)) { U.turned = !U.turned; U.why.clear(); audio_->play(Sfx::MenuMove); }
    if (key == SDLK_RETURN || key == SDLK_SPACE) {
      U.why.clear();
      if (home::placeObj(g, U.plot, (Obj)U.sel, U.gx, U.gy, U.turned, inside, U.why)) { U.placedKey = U.ghostKey(); audio_->play(Sfx::MenuSelect); }
      else audio_->play(Sfx::MenuBack);
    }
    if (key == SDLK_X || key == SDLK_DELETE || key == SDLK_BACKSPACE) {
      U.why.clear();
      if (home::removeObj(g, U.plot, U.gx, U.gy, inside, U.why)) { U.placedKey = -1; audio_->play(Sfx::MenuSelect); }
      else audio_->play(Sfx::MenuBack);
    }
    return;
  }
  if (U.mode == UiMode::Shell) {
    const int n = (int)home::Shell::COUNT;
    if (key == SDLK_UP || key == SDLK_W) { U.sel = (U.sel + n - 1) % n; U.why.clear(); audio_->play(Sfx::MenuMove); }
    if (key == SDLK_DOWN || key == SDLK_S) { U.sel = (U.sel + 1) % n; U.why.clear(); audio_->play(Sfx::MenuMove); }
    if ((key == SDLK_LEFT || key == SDLK_A || key == SDLK_RIGHT || key == SDLK_D || key == SDLK_Q || key == SDLK_E || key == SDLK_R) && !g.home.styles.empty()) {
      const int dir = (key == SDLK_LEFT || key == SDLK_A || key == SDLK_Q) ? -1 : 1;
      const int ns = (int)g.home.styles.size();
      int i = 0;
      for (int k = 0; k < ns; k++) if (g.home.styles[(size_t)k] == U.style) i = k;
      U.style = g.home.styles[(size_t)((i + dir + ns) % ns)];
      U.why.clear();
      audio_->play(Sfx::MenuMove);
    }
    if (key == SDLK_RETURN || key == SDLK_SPACE) {
      U.why.clear();
      uint64_t style = U.style;
      if (!style && !g.home.styles.empty()) style = g.home.styles.front();
      if (home::startBuild(g, U.plot, (home::Shell)U.sel, style, U.timber, U.iron, U.why)) { closeBuild(g); audio_->play(Sfx::MenuSelect); }
      else audio_->play(Sfx::MenuBack);
    }
    return;
  }
  if (U.mode == UiMode::Storage) {
    const ListLay S = listLay(touchUI);
    const int n = storeSide_ == 0 ? (int)g.inv.size() : (U.store >= 0 && U.store < (int)p.stores.size() ? (int)p.stores[(size_t)U.store].items.size() : 0);
    int& scroll = storeSide_ == 0 ? packScroll_ : storeScroll_;
    if (key == SDLK_UP || key == SDLK_W) storeSel_ = std::max(0, storeSel_ - 1);
    if (key == SDLK_DOWN || key == SDLK_S) storeSel_ = std::min(std::max(0, n - 1), storeSel_ + 1);
    if (key == SDLK_LEFT || key == SDLK_A || key == SDLK_RIGHT || key == SDLK_D) { storeSide_ ^= 1; storeSel_ = 0; }
    if (key == SDLK_Q && p.stores.size() > 1) U.store = (U.store + (int)p.stores.size() - 1) % (int)p.stores.size();
    if (key == SDLK_E && p.stores.size() > 1) U.store = (U.store + 1) % (int)p.stores.size();
    if (storeSel_ < scroll) scroll = storeSel_;
    if (storeSel_ >= scroll + S.rows) scroll = storeSel_ - S.rows + 1;
    if (key == SDLK_RETURN || key == SDLK_SPACE) {
      U.why.clear();
      const bool ok = storeSide_ == 0 ? home::storePut(g, U.plot, U.store, storeSel_, U.why) : home::storeTake(g, U.plot, U.store, storeSel_, U.why);
      audio_->play(ok ? Sfx::MenuSelect : Sfx::MenuBack);
    }
    return;
  }
  if (U.mode == UiMode::Cook) {
    const CookLay S = cookLay(touchUI);
    const std::vector<home::Meal> meals = home::knownMeals(g);
    if (key == SDLK_UP || key == SDLK_W) U.sel = std::max(0, U.sel - 1);
    if (key == SDLK_DOWN || key == SDLK_S) U.sel = std::min(std::max(0, (int)meals.size() - 1), U.sel + 1);
    if (U.sel < cookScroll_) cookScroll_ = U.sel;
    if (U.sel >= cookScroll_ + S.rows) cookScroll_ = U.sel - S.rows + 1;
    if ((key == SDLK_RETURN || key == SDLK_SPACE) && U.sel < (int)meals.size()) {
      U.why.clear();
      if (home::cook(g, meals[(size_t)U.sel], U.station, U.why)) {
        audio_->play(Sfx::MenuSelect);
        // (M7 fix r3, review: "no feedback while the Cook screen is open, then two messages per meal") the panel says
        // it (the HUD's toasts are behind the modal); the pack's own "gained" notice is the one left for later
        const Item made = home::makeMeal(meals[(size_t)U.sel], U.station);
        int have = 0;
        for (const Item& it : g.inv) if (it.same(made)) have += it.stackable() ? std::max(1, (int)it.count) : 1;
        cookNote_ = "COOKED: " + meals[(size_t)U.sel].name + " (HAVE " + std::to_string(have) + ")";
        cookNoteSel_ = U.sel; cookNoteT_ = t_;
      } else { audio_->play(Sfx::MenuBack); cookNote_.clear(); }
    }
    return;
  }
  if (key == SDLK_RETURN || key == SDLK_SPACE) closeBuild(g);
}

void View::buildTap(Game& g, Vec2 p) {
  home::Ui& U = g.home.ui;
  buildDrag_ = BuildDrag();
  if (!uiPlot(g)) { closeBuild(g); return; }
  const home::Plot& pl = *uiPlot(g);
  // ---- YARD / DECORATE
  if (U.mode == UiMode::Yard || U.mode == UiMode::Decorate) {
    const bool inside = U.mode == UiMode::Decorate;
    const SideLay L = sideLay(touchUI);
    if (L.panel.in(p)) {
      const std::vector<Cat>& cs = cats(inside);
      for (int i = 0; i < (int)cs.size() && i < 5; i++)
        if (L.tabs[i].in(p)) {
          buildCat_ = i; U.sel = (int)cs[(size_t)i].objs.front(); U.why.clear();
          audio_->play(Sfx::MenuMove);
          return;
        }
      const Cat& cat = cs[(size_t)std::clamp(buildCat_, 0, (int)cs.size() - 1)];
      for (int i = 0; i < (int)cat.objs.size() && i < L.cols * L.rows; i++)
        if (cellRect(L, i).in(p)) {
          if (U.sel != (int)cat.objs[(size_t)i]) U.turned = false;
          U.sel = (int)cat.objs[(size_t)i]; U.why.clear();
          audio_->play(Sfx::MenuMove);
          return;
        }
      if (L.rot.in(p)) { buildKey(g, SDLK_R); return; }
      if (L.sell.in(p)) { buildKey(g, SDLK_X); return; }
      if (L.place.in(p)) { buildKey(g, SDLK_RETURN); return; }
      if (L.done.in(p)) { buildKey(g, SDLK_ESCAPE); return; }
      return;
    }
    // the world: the ghost goes to the tile under the finger, and a drag from here moves it
    const Vec2 w(p.x + std::floor(cam_.x), p.y + std::floor(cam_.y));
    int tx = (int)std::floor(w.x / TILE), ty = (int)std::floor(w.y / TILE);
    int mw = pl.w, mh = pl.h;
    if (inside) { mw = g.map().w; mh = g.map().h; }
    else { tx += g.world.ox - pl.gx; ty += g.world.oy - pl.gy; }
    U.gx = std::clamp(tx, 0, std::max(0, mw - 1));
    U.gy = std::clamp(ty, 0, std::max(0, mh - 1));
    U.why.clear();
    buildDrag_.on = true; buildDrag_.start = p; buildDrag_.gx = U.gx; buildDrag_.gy = U.gy; buildDrag_.zone = 1;
    return;
  }
  // ---- SHELL
  if (U.mode == UiMode::Shell) {
    const ShellLay S = shellLay(touchUI);
    for (int i = 0; i < (int)home::Shell::COUNT && i < 5; i++)
      if (S.card[i].in(p)) { U.sel = i; U.why.clear(); audio_->play(Sfx::MenuMove); return; }
    if (S.prev.in(p)) { buildKey(g, SDLK_LEFT); return; }
    if (S.next.in(p)) { buildKey(g, SDLK_RIGHT); return; }
    if (S.build.in(p)) { buildKey(g, SDLK_RETURN); return; }
    if (S.cancel.in(p) || !modalRect().in(p)) { buildKey(g, SDLK_ESCAPE); return; }
    return;
  }
  // ---- STORAGE: the rows act on release (a drag scrolls instead)
  if (U.mode == UiMode::Storage) {
    const ListLay S = listLay(touchUI);
    if (S.done.in(p)) { buildKey(g, SDLK_ESCAPE); return; }
    if (pl.stores.size() > 1 && S.prev.in(p)) { buildKey(g, SDLK_Q); return; }
    if (pl.stores.size() > 1 && S.next.in(p)) { buildKey(g, SDLK_E); return; }
    int* scrolls[2] = {&packScroll_, &storeScroll_};
    for (int c = 0; c < 2; c++) {
      if (arrowHit(S.up[c]).in(p)) { *scrolls[c] = std::max(0, *scrolls[c] - std::max(1, S.rows - 1)); return; }
      if (arrowHit(S.down[c]).in(p)) { *scrolls[c] += std::max(1, S.rows - 1); return; }
      if (S.col[c].in(p)) {
        buildDrag_.on = true; buildDrag_.start = p; buildDrag_.list = c; buildDrag_.scroll0 = (float)*scrolls[c]; buildDrag_.zone = 2;
        return;
      }
    }
    return;
  }
  // ---- COOK
  if (U.mode == UiMode::Cook) {
    const CookLay S = cookLay(touchUI);
    if (S.done.in(p)) { buildKey(g, SDLK_ESCAPE); return; }
    if (S.cook.in(p)) { buildKey(g, SDLK_RETURN); return; }
    if (arrowHit(S.up).in(p)) { cookScroll_ = std::max(0, cookScroll_ - std::max(1, S.rows - 1)); return; }
    if (arrowHit(S.down).in(p)) { cookScroll_ += std::max(1, S.rows - 1); return; }
    if (S.list.in(p)) { buildDrag_.on = true; buildDrag_.start = p; buildDrag_.list = 0; buildDrag_.scroll0 = (float)cookScroll_; buildDrag_.zone = 3; }
    return;
  }
  // ---- BUY and the rest
  const ListLay S = listLay(touchUI);
  if (S.done.in(p) || !modalRect().in(p)) buildKey(g, SDLK_ESCAPE);
}

void View::buildPointer(Game& g, int phase, Vec2 p) {
  home::Ui& U = g.home.ui;
  if (g.mode != Mode::Build || !buildDrag_.on) return;
  if (!uiPlot(g)) { buildDrag_ = BuildDrag(); return; }
  const home::Plot& pl = *uiPlot(g);
  const Vec2 d = p - buildDrag_.start;
  if (len2(d) > 6.0f * 6.0f) buildDrag_.moved = true;
  if (buildDrag_.zone == 1) {   // the ghost, dragged by the finger's motion (relative: it never hides under the finger)
    const bool inside = U.mode == UiMode::Decorate;
    const int mw = inside ? g.map().w : pl.w, mh = inside ? g.map().h : pl.h;
    U.gx = std::clamp(buildDrag_.gx + (int)std::lround(d.x / TILE), 0, std::max(0, mw - 1));
    U.gy = std::clamp(buildDrag_.gy + (int)std::lround(d.y / TILE), 0, std::max(0, mh - 1));
    if (phase == 2) buildDrag_ = BuildDrag();
    return;
  }
  if (buildDrag_.zone == 2) {   // STORAGE: drag scrolls a column, a still tap moves the item
    const ListLay S = listLay(touchUI);
    int* scrolls[2] = {&packScroll_, &storeScroll_};
    const int c = std::clamp(buildDrag_.list, 0, 1);
    if (buildDrag_.moved) *scrolls[c] = std::max(0, (int)std::lround(buildDrag_.scroll0 - d.y / S.rowH));
    if (phase == 2) {
      if (!buildDrag_.moved) {
        const int row = (int)((buildDrag_.start.y - S.col[c].y) / S.rowH);
        const int i = *scrolls[c] + row;
        storeSide_ = c; storeSel_ = i;
        const int n = c == 0 ? (int)g.inv.size() : (U.store >= 0 && U.store < (int)pl.stores.size() ? (int)pl.stores[(size_t)U.store].items.size() : 0);
        if (row >= 0 && row < S.rows && i < n) {
          U.why.clear();
          const bool ok = c == 0 ? home::storePut(g, U.plot, U.store, i, U.why) : home::storeTake(g, U.plot, U.store, i, U.why);
          audio_->play(ok ? Sfx::MenuSelect : Sfx::MenuBack);
        }
      }
      buildDrag_ = BuildDrag();
    }
    return;
  }
  if (buildDrag_.zone == 3) {   // COOK: drag scrolls, a still tap picks the meal (a second tap on it cooks)
    const CookLay S = cookLay(touchUI);
    if (buildDrag_.moved) cookScroll_ = std::max(0, (int)std::lround(buildDrag_.scroll0 - d.y / S.rowH));
    if (phase == 2) {
      if (!buildDrag_.moved) {
        const int row = (int)((buildDrag_.start.y - S.list.y) / S.rowH);
        const int i = cookScroll_ + row;
        const int n = (int)home::knownMeals(g).size();
        if (row >= 0 && row < S.rows && i < n) {
          if (U.sel == i) buildKey(g, SDLK_RETURN);
          else { U.sel = i; U.why.clear(); audio_->play(Sfx::MenuMove); }
        }
      }
      buildDrag_ = BuildDrag();
    }
  }
}

// ---------------------------------------------------------------- scripts (script_buildmode.cpp forwards here)
namespace {
const char* const kObjWords[] = {"fence", "gate", "path", "farmland", "well", "woodpile", "beehive", "scarecrow", "coop", "pen", "stable",
                                 "trough", "workbench", "forge", "flowerbed", "sapling", "bench", "lantern", "statue", "banner", "campfire",
                                 "doghouse", "hayrack", "crate", "bed", "table", "chair", "shelf", "chest", "rug", "hearth", "cookpot",
                                 "mannequin", "weaponrack", "trophy", "painting", "wardrobe", "bookshelf", "plantpot", "candelabra",
                                 "stool", "cupboard"};
static_assert(sizeof(kObjWords) / sizeof(kObjWords[0]) == (size_t)Obj::COUNT, "a word for every object");
const char* const kModeWords[] = {"none", "shell", "yard", "decorate", "storage", "cook", "buy"};
const char* const kShellWords[] = {"hut", "cottage", "longhouse", "townhouse", "hall"};
const char* const kStationWords[] = {"campfire", "hearth", "cookpot", "inn"};
std::string lower(std::string s) {
  for (char& ch : s) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
  return s;
}
int wordIn(const char* const* t, int n, const std::string& w) {
  for (int i = 0; i < n; i++) if (w == t[i]) return i;
  for (int i = 0; i < n; i++) if (std::string(t[i]).rfind(w, 0) == 0) return i;
  return -1;
}
}  // namespace

bool View::buildCommand(Game& g, const std::vector<std::string>& a, std::string& err) {
  // a[0] == "build"
  const std::string op = a.size() > 1 ? lower(a[1]) : std::string();
  auto arg = [&](size_t i) { return i < a.size() ? lower(a[i]) : std::string(); };
  home::Ui& U = g.home.ui;
  if (op == "tab" || op == "open") {
    const int m = wordIn(kModeWords, 7, arg(2));
    if (m <= 0) { err = "build tab: yard|decorate|shell|storage|cook|buy"; return false; }
    bool indoors = false;
    int plot = homeHere(g, indoors);
    if (plot < 0) plot = g.home.plots.empty() ? -1 : (int)g.home.plots.size() - 1;
    if (plot < 0) { err = "build tab: no property (home lot first)"; return false; }
    openBuild(g, (UiMode)m, plot);
    if ((UiMode)m == UiMode::Storage) {   // a store: the one named by index (default: the first), made if the plot has none
      const int si = arg(3).empty() ? 0 : std::atoi(arg(3).c_str());
      U.store = g.home.plots[(size_t)plot].stores.empty() ? -1 : std::clamp(si, 0, (int)g.home.plots[(size_t)plot].stores.size() - 1);
    }
    if ((UiMode)m == UiMode::Cook && !arg(3).empty()) {
      const int st = wordIn(kStationWords, 4, arg(3));
      if (st >= 0) U.station = (home::Station)st;
    }
    return true;
  }
  if (op == "stand" || op == "enterhome") {   // (staging) the player onto a tile of the last plot / into its house
    if (g.home.plots.empty()) { err = "build " + op + ": no property (home lot first)"; return false; }
    const home::Plot& p = g.home.plots.back();
    if (op == "stand") {
      if (g.inside) { err = "build stand: outdoors only"; return false; }
      const int x = std::atoi(arg(2).c_str()), y = std::atoi(arg(3).c_str());
      g.pl().p = Vec2((p.gx - g.world.ox + x) * 16.0f + 8.0f, (p.gy - g.world.oy + y) * 16.0f + 12.0f);
      snap(g);
      return true;
    }
    ew::Gid id = p.id;
    if (p.kind == home::PlotKind::Lot) id = ew::makeId(ew::idRx(p.id), ew::idRy(p.id), ew::IdKind::Plot, 0xC00u | (ew::idLocal(p.id) & 0x3FFu));
    const int bi = g.world.bldgHandle(id);
    if (bi < 0) { err = "build enterhome: the house is not in the window (built? stamped?)"; return false; }
    if (!g.debugEnterBuilding(bi, std::atoi(arg(2).c_str()))) { err = "build enterhome: could not enter"; return false; }
    snap(g);
    return true;
  }
  if (g.mode != Mode::Build) { err = "build " + op + ": no build screen open (build tab ...)"; return false; }
  if (op == "sel") {
    if (U.mode == UiMode::Shell) {
      const int s = wordIn(kShellWords, 5, arg(2));
      if (s < 0) { err = "build sel: hut|cottage|longhouse|townhouse|hall"; return false; }
      U.sel = s;
      return true;
    }
    if (U.mode == UiMode::Cook || U.mode == UiMode::Storage) { U.sel = std::atoi(arg(2).c_str()); storeSel_ = U.sel; return true; }
    const int o = wordIn(kObjWords, (int)Obj::COUNT, arg(2));
    if (o < 0) { err = "build sel: unknown object '" + arg(2) + "'"; return false; }
    U.sel = o; U.why.clear();
    const int c = catOf(U.mode == UiMode::Decorate, o);
    if (c >= 0) buildCat_ = c;
    return true;
  }
  if (op == "cat") { buildCat_ = std::atoi(arg(2).c_str()); U.sel = (int)cats(U.mode == UiMode::Decorate)[(size_t)std::clamp(buildCat_, 0, 4)].objs.front(); return true; }
  if (op == "ghost") { U.gx = std::atoi(arg(2).c_str()); U.gy = std::atoi(arg(3).c_str()); U.why.clear(); return true; }
  if (op == "rotate") { buildKey(g, SDLK_R); return true; }
  if (op == "place") { buildKey(g, SDLK_RETURN); return true; }
  if (op == "remove" || op == "sell") { buildKey(g, SDLK_X); return true; }
  if (op == "style") {   // next | prev | <n>
    if (g.home.styles.empty()) { err = "build style: no styles known"; return false; }
    if (arg(2) == "prev") buildKey(g, SDLK_LEFT);
    else if (arg(2).empty() || arg(2) == "next") buildKey(g, SDLK_RIGHT);
    else U.style = g.home.styles[(size_t)std::clamp(std::atoi(arg(2).c_str()), 0, (int)g.home.styles.size() - 1)];
    return true;
  }
  if (op == "side") { storeSide_ = std::clamp(std::atoi(arg(2).c_str()), 0, 1); storeSel_ = 0; return true; }
  if (op == "move") { buildKey(g, SDLK_RETURN); return true; }
  if (op == "cook") { buildKey(g, SDLK_RETURN); return true; }
  if (op == "done" || op == "close") { buildKey(g, SDLK_ESCAPE); return true; }
  err = "build: tab|sel|cat|ghost|rotate|place|remove|style|side|move|cook|done";
  return false;
}

bool View::buildExpect(Game& g, const std::vector<std::string>& a, std::string& err) {
  // a: "expect", "build", <what>, ...
  const std::string what = a.size() > 2 ? lower(a[2]) : std::string();
  auto arg = [&](size_t i) { return i < a.size() ? lower(a[i]) : std::string(); };
  const home::Ui& U = g.home.ui;
  if (what == "mode") {
    const int m = wordIn(kModeWords, 7, arg(3));
    const int now = g.mode == Mode::Build ? (int)U.mode : 0;
    if (m != now) err = "expect build mode " + arg(3) + ": it is " + kModeWords[now];
    return err.empty();
  }
  if (U.plot < 0 || U.plot >= (int)g.home.plots.size()) { err = "expect build " + what + ": no plot open"; return false; }
  const home::Plot& p = g.home.plots[(size_t)U.plot];
  if (what == "fits") {
    std::string why;
    const bool inside = U.mode == UiMode::Decorate;
    const bool fits = inside ? home::canPlaceInside(p, g.map(), (Obj)U.sel, U.gx, U.gy, U.turned, why)
                             : home::canPlaceOutside(p, (Obj)U.sel, U.gx, U.gy, U.turned, why);
    if (fits != (std::atoi(arg(3).c_str()) != 0)) err = std::string("expect build fits ") + arg(3) + ": " + (fits ? "it fits" : why);
    return err.empty();
  }
  if (what == "objects") {   // outside (yard) or inside (decorate) objects on the plot
    const int n = (int)(U.mode == UiMode::Decorate ? p.inside.size() : p.outside.size());
    if (n != std::atoi(arg(3).c_str())) err = "expect build objects " + arg(3) + ": " + std::to_string(n);
    return err.empty();
  }
  if (what == "at") {   // expect build at <x> <y> <object>: the object covering that tile
    const int i = objAtTile(g, p, std::atoi(arg(3).c_str()), std::atoi(arg(4).c_str()), U.mode == UiMode::Decorate);
    const std::vector<home::PlacedObj>& v = U.mode == UiMode::Decorate ? p.inside : p.outside;
    const std::string have = i >= 0 ? kObjWords[v[(size_t)i].kind] : "nothing";
    if (have != arg(5)) err = "expect build at: " + have;
    return err.empty();
  }
  if (what == "why") {   // the last refusal contains the words (upper case in the game)
    std::string want;
    for (size_t k = 3; k < a.size(); k++) { if (k > 3) want += ' '; want += a[k]; }
    if (U.why.find(want) == std::string::npos) err = "expect build why '" + want + "': '" + U.why + "'";
    return err.empty();
  }
  if (what == "store") {   // expect build store <n>: stacks in the open store
    const int n = U.store >= 0 && U.store < (int)p.stores.size() ? (int)p.stores[(size_t)U.store].items.size() : -1;
    if (n != std::atoi(arg(3).c_str())) err = "expect build store " + arg(3) + ": " + std::to_string(n);
    return err.empty();
  }
  if (what == "state") {   // expect build state owned|building|built
    static const char* st[] = {"owned", "building", "built"};
    if (arg(3) != st[std::min(2, (int)p.state)]) err = std::string("expect build state ") + arg(3) + ": " + st[std::min(2, (int)p.state)];
    return err.empty();
  }
  err = "expect build: mode|fits|objects|at|why|store|state";
  return false;
}
