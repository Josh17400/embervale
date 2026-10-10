// M7 "Home": the player's own house inside (HOMESTEAD lane, phase B).
//   - homeMapLoaded: nobody lives in a vacant house for sale or in the player's own (the generator's people are sent
//     away); a bought house's generated furniture is taken into Plot::inside on the first entry of each floor (so it can
//     be moved or sold back); a house built on a lot gets its starter bed and chest; then every piece of Plot::inside on
//     this floor is stamped into the map (its prop on the footprint's bottom-middle tile, Filler on the rest, the M2 /
//     M4 footprint rule), replacing whatever catalogue furniture the generator put there.
//   - homePropUsable / homeUseProp: the player's stores open the Storage screen, beds are theirs (Rested, the respawn
//     point), cooking places (hearths, cauldrons, ovens, stoves, fire pits; campfires outdoors) open the Cook screen
//     (an inn's are its kitchen), the FOR SALE sign points at the seller.
#include <algorithm>
#include <string>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/home.h"

namespace {
using home::Obj;
using home::ObjInfo;
using home::PlacedObj;

void footprintOf(const PlacedObj& o, int& w, int& h) {
  const ObjInfo& oi = home::objInfo((Obj)o.kind);
  w = oi.w; h = oi.h;
  if (o.turned() && (oi.flags & home::OBJ_ROTATES)) std::swap(w, h);
}
// the catalogue piece a generated prop is (Obj::COUNT: none)
Obj objOfProp(art::Prop p) {
  if (p == art::Prop::ChestOpen) return Obj::Chest;
  for (int k = 0; k < (int)Obj::COUNT; k++) {
    const ObjInfo& oi = home::objInfo((Obj)k);
    if ((oi.flags & home::OBJ_INSIDE) && oi.prop == p) return (Obj)k;
  }
  return Obj::COUNT;
}
// the top-left of a generated piece whose prop stands at (x, y) (the footprint rule: the prop on the bottom row's middle)
void anchorOf(Obj o, int x, int y, int& ox, int& oy) {
  const ObjInfo& oi = home::objInfo(o);
  if (oi.flags & home::OBJ_WALLDECOR) { ox = x; oy = y; return; }
  ox = x - oi.w / 2;
  oy = y - (oi.h - 1);
}
}  // namespace

void HomeOps::stampObj(Map& m, const PlacedObj& o, bool clear) {
  const ObjInfo& oi = home::objInfo((Obj)o.kind);
  if (oi.prop == art::Prop::COUNT) return;
  if (oi.flags & home::OBJ_WALLDECOR) { m.setP(o.x, o.y, clear ? 0 : (int)oi.prop + 1); return; }
  int w, h;
  footprintOf(o, w, h);
  const int ax = o.x + w / 2, ay = o.y + h - 1;
  for (int y = o.y; y < o.y + h; y++)
    for (int x = o.x; x < o.x + w; x++) {
      if (!m.in(x, y)) continue;
      const bool anchor = x == ax && y == ay;
      if (clear) {
        if (anchor || m.propAt(x, y) == (int)art::Prop::Filler + 1) m.setP(x, y, 0);
        continue;
      }
      if (anchor) m.setProp(x, y, oi.prop);
      else if ((oi.flags & home::OBJ_SOLID) && !groundSolid(m.at(x, y))) m.setProp(x, y, art::Prop::Filler);   // (a short bed's head on the wall row: no Filler)
    }
}

int HomeOps::insideObjAt(const Game& g, int plot, int tx, int ty) {
  if (plot < 0 || plot >= (int)g.home.plots.size()) return -1;
  const home::Plot& p = g.home.plots[(size_t)plot];
  for (size_t i = 0; i < p.inside.size(); i++) {
    const PlacedObj& o = p.inside[i];
    if (o.floor() != g.subFloor) continue;
    int w, h;
    footprintOf(o, w, h);
    if (tx >= o.x && ty >= o.y && tx < o.x + w && ty < o.y + h) return (int)i;
  }
  return -1;
}

void HomeOps::mapLoaded(Game& g) {
  if (!g.inside || g.subBldg < 0) return;
  const int pi = home::plotOfInterior(g);
  if (pi < 0 && !home::houseEmpty(g.world, &g, g.subBldg)) return;
  // nobody lives here: the generator's people go (a vacant house for sale, or the player's own)
  for (size_t k = g.actors.size(); k-- > 1;)
    if (g.actors[k].npc && !g.actors[k].hostile && !(g.actors[k].lifeBits & LB_HOMESTEAD)) g.actors.erase(g.actors.begin() + (std::ptrdiff_t)k);
  if (pi < 0) return;
  home::Plot& p = g.home.plots[(size_t)pi];
  Map& m = g.sub;
  const int fl = g.subFloor;
  const uint8_t bit = (uint8_t)(1u << std::min(fl, 7));
  // a bought house's own furniture becomes the player's (first entry of this floor)
  // (integration) a house built on a lot keeps the fire its chimney promises: the builder's hearth / cook pot is
  // adopted the same way (the empty-shell interior has no other catalogue pieces)
  if ((p.kind == home::PlotKind::House || p.kind == home::PlotKind::Lot) && !(p.furnished & bit)) {
    const bool cookOnly = p.kind == home::PlotKind::Lot;
    for (int y = 0; y < m.h; y++)
      for (int x = 0; x < m.w; x++) {
        const int pr = m.propAt(x, y);
        if (!pr) continue;
        const Obj o = objOfProp((art::Prop)(pr - 1));
        if (o == Obj::COUNT || (int)p.inside.size() >= home::MAX_OBJECTS) continue;
        if (cookOnly && !(home::objInfo(o).flags & home::OBJ_COOK)) continue;
        int ox, oy;
        anchorOf(o, x, y, ox, oy);
        if (ox < 0 || oy < 0) continue;
        PlacedObj po;
        po.kind = (uint8_t)o; po.x = (uint8_t)ox; po.y = (uint8_t)oy; po.flags = (uint8_t)(fl << 4);
        if (home::objInfo(o).flags & home::OBJ_STORAGE) {
          const int k = home::allocStore(p);
          if (k < 0) continue;
          po.data = (uint32_t)k;
        }
        p.inside.push_back(po);
      }
    p.furnished |= bit;
  }
  // the generator's catalogue furniture goes (Plot::inside is the furniture now); other pieces (barrels, shelves of
  // bottles, the hearth's chimney breast...) stay
  for (int y = 0; y < m.h; y++)
    for (int x = 0; x < m.w; x++) {
      const int pr = m.propAt(x, y);
      if (!pr) continue;
      const Obj o = objOfProp((art::Prop)(pr - 1));
      if (o == Obj::COUNT) continue;
      int ox, oy;
      anchorOf(o, x, y, ox, oy);
      PlacedObj po;
      po.kind = (uint8_t)o; po.x = (uint8_t)std::max(0, ox); po.y = (uint8_t)std::max(0, oy);
      if (ox >= 0 && oy >= 0) stampObj(m, po, true);
      m.setP(x, y, 0);
    }
  m.rebuildSolid();
  // a house built on a lot: the starter bed against the back wall and a chest beside it (once)
  if (p.kind == home::PlotKind::Lot && !p.starter && fl == 0) {
    std::string why;
    bool bed = false;
    int bx = -1, by = -1;
    for (int y = 1; y < m.h - 1 && !bed; y++)
      for (int x = 1; x < m.w - 1 && !bed; x++)
        if (home::canPlaceInside(p, m, Obj::Bed, x, y, false, why)) {
          PlacedObj po;
          po.kind = (uint8_t)Obj::Bed; po.x = (uint8_t)x; po.y = (uint8_t)y; po.flags = (uint8_t)(fl << 4);
          p.inside.push_back(po);
          stampObj(m, po, false);
          m.rebuildSolid();
          bed = true; bx = x; by = y;
        }
    bool chest = false;
    const int cx[] = {bx + 1, bx - 1, bx + 2, bx + 1, bx - 1};
    const int cy[] = {by, by, by, by + 1, by + 1};
    for (int k = 0; k < 5 && bed && !chest; k++)
      if (home::canPlaceInside(p, m, Obj::Chest, cx[k], cy[k], false, why)) {
        PlacedObj po;
        po.kind = (uint8_t)Obj::Chest; po.x = (uint8_t)cx[k]; po.y = (uint8_t)cy[k]; po.flags = (uint8_t)(fl << 4);
        po.data = 0;   // the house's first store (startBuild made it)
        if (p.stores.empty()) p.stores.push_back(home::Store());
        p.inside.push_back(po);
        stampObj(m, po, false);
        m.rebuildSolid();
        chest = true;
      }
    for (int y = 1; y < m.h - 1 && bed && !chest; y++)
      for (int x = 1; x < m.w - 1 && !chest; x++)
        if (home::canPlaceInside(p, m, Obj::Chest, x, y, false, why)) {
          PlacedObj po;
          po.kind = (uint8_t)Obj::Chest; po.x = (uint8_t)x; po.y = (uint8_t)y; po.flags = (uint8_t)(fl << 4);
          if (p.stores.empty()) p.stores.push_back(home::Store());
          p.inside.push_back(po);
          stampObj(m, po, false);
          m.rebuildSolid();
          chest = true;
        }
    p.starter = 1;
  }
  for (const PlacedObj& o : p.inside)
    if (o.floor() == fl) stampObj(m, o, false);
  m.rebuildSolid();
}

bool HomeOps::propUsable(const Game& g, art::Prop p, int tx, int ty) {
  home::Station st;
  if (home::stationOf(g, p, st)) return true;
  if (p == art::Prop::ForSaleSign && !g.inside) return true;
  const int pi = home::plotOfInterior(g);
  if (pi < 0) return false;
  const int k = insideObjAt(g, pi, tx, ty);
  return k >= 0 && (home::objInfo((Obj)g.home.plots[(size_t)pi].inside[(size_t)k].kind).flags & (home::OBJ_STORAGE | home::OBJ_COOK | home::OBJ_BED));
}

bool HomeOps::bed(const Game& g, int tx, int ty) {
  const int pi = home::plotOfInterior(g);
  if (pi < 0) return false;
  const int k = insideObjAt(g, pi, tx, ty);
  return k >= 0 && (Obj)g.home.plots[(size_t)pi].inside[(size_t)k].kind == Obj::Bed;
}

bool HomeOps::useProp(Game& g, art::Prop p, int tx, int ty) {
  const int pi = home::plotOfInterior(g);
  if (pi >= 0) {
    const int k = insideObjAt(g, pi, tx, ty);
    if (k >= 0) {
      home::Plot& P = g.home.plots[(size_t)pi];
      const PlacedObj& o = P.inside[(size_t)k];
      const uint16_t f = home::objInfo((Obj)o.kind).flags;
      if (f & home::OBJ_STORAGE) {
        if (o.data >= P.stores.size()) P.stores.resize((size_t)o.data + 1);
        g.home.ui = home::Ui();
        g.home.ui.mode = home::UiMode::Storage; g.home.ui.plot = pi; g.home.ui.store = (int)o.data;
        g.mode = Mode::Build;
        return true;
      }
      if (f & home::OBJ_BED) {
        // the player's own bed: home is where they wake (then lifeUseProp sleeps: Rested, the night passes)
        if (!(P.flags & home::PF_RESPAWN)) say(g, "YOUR OWN BED: YOU WILL WAKE HERE FROM NOW ON");
        for (home::Plot& q : g.home.plots) q.flags &= (uint8_t)~home::PF_RESPAWN;
        P.flags |= home::PF_RESPAWN;
        return false;
      }
      if (f & home::OBJ_COOK) {
        g.home.ui = home::Ui();
        g.home.ui.mode = home::UiMode::Cook; g.home.ui.plot = pi;
        g.home.ui.station = (Obj)o.kind == Obj::CookPot ? home::Station::CookPot : home::Station::Hearth;
        g.mode = Mode::Build;
        return true;
      }
    }
  }
  home::Station st;
  if (home::stationOf(g, p, st)) {
    g.home.ui = home::Ui();
    g.home.ui.mode = home::UiMode::Cook; g.home.ui.plot = pi; g.home.ui.station = st;
    g.mode = Mode::Build;
    return true;
  }
  if (p == art::Prop::ForSaleSign && !g.inside) {
    const int si = g.world.siteAt(tx, ty, 6);
    const bool village = si >= 0 && g.world.sites[(size_t)si].type == SiteType::Village;
    say(g, std::string("FOR SALE. ASK ") + (village ? "THE INNKEEPER" : "AT THE " + lordTitleAt(g.world, si) + "'S HALL") + " ABOUT IT.");
    return true;
  }
  return false;
}
