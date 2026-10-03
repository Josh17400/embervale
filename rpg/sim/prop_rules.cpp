// Simulation rules for props: which ones block movement. Also decoLayers (rpg/sim/deco.h): which interior pieces a tile
// shows, shared by the renderer and the props gallery.
// WARNING: world generation calls propSolid (vegetation clearing, den placement), so changing the answer for an
// existing prop changes the generated world of every version. Only add cases for props appended after M0 began
// (Prop::Bed... are old; check art_props.h) unless the change is gated on a world-gen version.
#include "rpg/sim/deco.h"
#include "rpg/sim/world.h"

using art::Prop;

bool propSolid(Prop p) {
  switch (p) {
    case Prop::Flowers1: case Prop::Flowers2: case Prop::Flowers3: case Prop::TallGrass: case Prop::Reeds:
    case Prop::Mushrooms: case Prop::LilyPad: case Prop::Fern: case Prop::Bones: case Prop::SkullPile:
    case Prop::Cobweb: case Prop::Rug: case Prop::Chair: case Prop::ChestOpen: case Prop::Torch: case Prop::Ladder:
    case Prop::CaveEntrance: case Prop::Banner: case Prop::IronDoor:
      return false;
    // M0 (appended props): wall decor hangs on wall tiles; stools and benches are walk-through seats like chairs
    case Prop::Tapestry: case Prop::WallShelf: case Prop::HerbBundle: case Prop::Antlers: case Prop::Painting:
    case Prop::Sconce: case Prop::Window: case Prop::WallShield: case Prop::ToolRack: case Prop::PanRack:
    case Prop::Wreath: case Prop::HolySymbol: case Prop::Stool: case Prop::Bench:
      return false;
    default: return true;
  }
}

// ---------------------------------------------------------------- interior pieces
namespace {
bool isWallT(const Map& m, int x, int y) { return !m.in(x, y) || m.at(x, y) == Ground::InteriorWall; }
// a back-wall face: a wall tile with floor right below it (its face is visible in 3/4)
bool isFace(const Map& m, int x, int y) { return m.in(x, y) && m.at(x, y) == Ground::InteriorWall && !isWallT(m, x, y + 1); }
// the room as it is seen: floor, the visible back-wall faces and the wall tiles above each face (the face is drawn 3
// tiles tall: map rows 0 and 1 plus one row above the map)
bool roomArea(const Map& m, int x, int y) {
  if (!m.in(x, y)) return false;
  return !isWallT(m, x, y) || isFace(m, x, y) || isFace(m, x, y + 1) || isFace(m, x, y + 2);
}

art::RoomStyle roomStyleOf(const Map& m) {
  for (int x = 1; x < m.w - 1; x++) {
    int d = m.decoAt(x, 1);
    if (d >= (int)Deco::WallTimber && d <= (int)Deco::WallArcane) return (art::RoomStyle)(d - (int)Deco::WallTimber);
  }
  // interiors made before WORLDGEN_V3 carry no style: derive it from the floor
  for (int y = 0; y < m.h; y++)
    for (int x = 0; x < m.w; x++)
      if (m.at(x, y) == Ground::StoneFloor) return art::RoomStyle::Stone;
  return art::RoomStyle::Timber;
}

// contact shadow under a piece of furniture: size and offset (from the tile's top-left); false = none
bool shadowOf(Prop p, int& w, int& h, int& ox, int& oy) {
  w = 0; h = 5; ox = 0; oy = 12;
  switch (p) {
    case Prop::Bed: w = 20; h = 6; break;
    case Prop::Table: w = 30; h = 6; break;
    case Prop::Counter: w = 30; h = 6; break;
    case Prop::Chair: case Prop::Stool: w = 11; h = 4; break;
    case Prop::Shelf: case Prop::Bookshelf: w = 24; h = 5; break;
    case Prop::Fireplace: w = 30; h = 5; break;
    case Prop::Barrel: case Prop::Barrel2: case Prop::Crate: case Prop::Chest: case Prop::ChestOpen: w = 15; h = 5; break;
    case Prop::PlantPot: case Prop::Urn: w = 10; h = 4; break;
    case Prop::Cauldron: w = 18; h = 5; break;
    case Prop::Throne: w = 24; h = 6; break;
    case Prop::Anvil: case Prop::Haystack: case Prop::Woodpile: w = 20; h = 5; break;
    case Prop::Altar: w = 30; h = 6; break;
    case Prop::Brazier: w = 12; h = 4; break;
    case Prop::Cupboard: case Prop::Wardrobe: case Prop::Desk: case Prop::Workbench: case Prop::Loom: w = 16; h = 5; break;
    case Prop::Cradle: case Prop::SpinningWheel: w = 14; h = 5; break;
    case Prop::Bench: w = 16; h = 4; break;
    case Prop::Hearth: case Prop::Forge: w = 46; h = 6; break;
    case Prop::TableSmall: case Prop::TableMeal: case Prop::TableWork: w = 15; h = 5; break;
    case Prop::TableL: case Prop::CounterL: w = 14; h = 5; ox = 2; break;
    case Prop::TableM: case Prop::CounterM: w = 16; h = 5; ox = 0; break;
    case Prop::TableR: case Prop::CounterR: w = 17; h = 5; ox = 0; return true;
    default: return false;
  }
  if (p != Prop::TableL && p != Prop::CounterL && p != Prop::TableM && p != Prop::CounterM) ox = 8 - w / 2 + 2;
  return true;
}
}  // namespace

void decoLayers(const Map& m, int tx, int ty, std::vector<DecoLayer>& out) {
  int d = m.decoAt(tx, ty);
  auto add = [&](uint32_t key, int dx, int dy, int pass) { out.push_back(DecoLayer{key, (int16_t)dx, (int16_t)dy, (uint8_t)pass}); };
  auto clutter = [&]() {
    if (d >= (int)Deco::Basket && d <= (int)Deco::Kindling) add(art::pieceKey(art::Piece::Clutter, d - (int)Deco::Basket), 0, 0, 3);
  };
  if (m.kind != MapKind::Interior) { clutter(); return; }
  art::RoomStyle rs = roomStyleOf(m);
  if (!isWallT(m, tx, ty)) {
    Ground g = m.at(tx, ty);
    art::FloorStyle fs = g == Ground::StoneFloor ? (rs == art::RoomStyle::Hall ? art::FloorStyle::Slab : art::FloorStyle::Flagstone)
                                                 : (rs == art::RoomStyle::Log || rs == art::RoomStyle::Soot ? art::FloorStyle::OldPlanks : art::FloorStyle::Planks);
    int mask = (isWallT(m, tx, ty - 1) ? 1 : 0) | (isWallT(m, tx - 1, ty) ? 2 : 0) | (isWallT(m, tx + 1, ty) ? 4 : 0) | (isWallT(m, tx, ty + 1) ? 8 : 0);
    add(art::pieceKey(art::Piece::Floor, (int)fs | mask << 3, tx, ty), 0, 0, 0);
    if (tx == m.exitX && ty == m.exitY) add(art::pieceKey(art::Piece::Door, (int)rs), 0, 0, 0);
    if (d >= (int)Deco::RugRed && d <= (int)Deco::RugGold) {
      int nm = (m.decoAt(tx, ty - 1) == d ? 1 : 0) | (m.decoAt(tx + 1, ty) == d ? 2 : 0) | (m.decoAt(tx, ty + 1) == d ? 4 : 0) | (m.decoAt(tx - 1, ty) == d ? 8 : 0);
      add(art::pieceKey(art::Piece::Rug, d - (int)Deco::RugRed, nm), 0, 0, 1);
    }
    int pr = m.propAt(tx, ty);
    int w, h, ox, oy;
    if (pr && shadowOf((Prop)(pr - 1), w, h, ox, oy)) add(art::pieceKey(art::Piece::Shadow, 0, w, h), ox, oy, 2);
    clutter();
    return;
  }
  if (isFace(m, tx, ty)) {
    int ends = (isWallT(m, tx - 1, ty) && !isFace(m, tx - 1, ty) ? 16 : 0) | (isWallT(m, tx + 1, ty) && !isFace(m, tx + 1, ty) ? 32 : 0);
    add(art::pieceKey(art::Piece::BackWall, (int)rs | ends, tx), 0, -32, 0);
    return;
  }
  if (roomArea(m, tx, ty)) return;   // the upper half of a back wall: painted by the face below
  int mask = 0;
  if (roomArea(m, tx + 1, ty)) mask |= art::CapE;
  if (roomArea(m, tx - 1, ty)) mask |= art::CapW;
  if (roomArea(m, tx, ty - 1)) mask |= art::CapN;
  if (roomArea(m, tx, ty + 1)) mask |= art::CapS;
  if (roomArea(m, tx + 1, ty - 1)) mask |= art::CapNE;
  if (roomArea(m, tx - 1, ty - 1)) mask |= art::CapNW;
  if (roomArea(m, tx + 1, ty + 1)) mask |= art::CapSE;
  if (roomArea(m, tx - 1, ty + 1)) mask |= art::CapSW;
  if (mask) add(art::pieceKey(art::Piece::Cap, (int)rs, mask, tx + ty), 0, 0, 0);
  // the side walls rise with the back wall above the map's top row: one more strip that joins the wall's top
  if (ty == 0 && (mask & (art::CapE | art::CapW))) {
    int em = 0, fl = 4;
    if ((mask & art::CapE) && isFace(m, tx + 1, 1)) { em |= art::CapE | art::CapSE; fl |= 1; }
    if ((mask & art::CapW) && isFace(m, tx - 1, 1)) { em |= art::CapW | art::CapSW; fl |= 2; }
    if (em) add(art::pieceKey(art::Piece::Cap, (int)rs | fl << 3, em, tx + 31), 0, -16, 0);
  }
}
