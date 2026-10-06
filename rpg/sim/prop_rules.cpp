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
    case Prop::Cushion: case Prop::SleepingMat:   // M3 culture furniture you step over
    case Prop::CaveEntrance: case Prop::Banner: case Prop::IronDoor:
      return false;
    // M0 (appended props): wall decor hangs on wall tiles; stools and benches are walk-through seats like chairs
    case Prop::Tapestry: case Prop::WallShelf: case Prop::HerbBundle: case Prop::Antlers: case Prop::Painting:
    case Prop::Sconce: case Prop::Window: case Prop::WallShield: case Prop::ToolRack: case Prop::PanRack:
    case Prop::Wreath: case Prop::HolySymbol: case Prop::Stool: case Prop::Bench:
      return false;
    // M0b: stairs are walked onto (that changes floor); doors stand open in their doorways
    case Prop::StairsUp: case Prop::StairsDown: case Prop::DoorH: case Prop::DoorV:
      return false;
    // M1 fixer round 2: the mine's track is walked over
    case Prop::MineRail:
      return false;
    // M2: a wayfarer's bedroll by the embers is walked over
    case Prop::Bedroll:
      return false;
    default: return true;
  }
}

// ---------------------------------------------------------------- interior pieces
// How an interior's walls are seen (rpg/sim/rooms.h has the geometry contract):
//  - the back wall (rows 0..1 over every inner column) is a tall face drawn over both rows and one row above the map;
//  - partitions (inner wall tiles) are cut-away walls one tile high: a tile with floor below shows a 16 px face, any
//    other inner wall tile shows the wall's top. An N-S run hanging from the back wall puts its top over row 1;
//  - the shell (side walls, front wall) shows a strip of wall top along the room, joining partition tops seamlessly.
namespace {
bool isWallT(const Map& m, int x, int y) { return !m.in(x, y) || m.at(x, y) == Ground::InteriorWall; }
// a column of the back wall: rows 0..1 are wall over an inner column (the tall face is drawn there)
bool backCol(const Map& m, int x) { return x >= 1 && x <= m.w - 2 && isWallT(m, x, 0) && isWallT(m, x, 1) && m.h > 2; }
bool backFace(const Map& m, int x, int y) { return y >= 0 && y <= 1 && backCol(m, x); }
// an inner wall tile (partition) not on the shell
bool innerWall(const Map& m, int x, int y) { return m.in(x, y) && x >= 1 && x <= m.w - 2 && y >= 2 && y <= m.h - 2 && isWallT(m, x, y); }
// a partition face: an inner wall tile with floor right below it
bool partFace(const Map& m, int x, int y) { return innerWall(m, x, y) && !isWallT(m, x, y + 1); }
bool partCap(const Map& m, int x, int y) { return innerWall(m, x, y) && isWallT(m, x, y + 1); }
// a visible face of any wall (it counts as room for the wall tops around it)
bool anyFace(const Map& m, int x, int y) { return backFace(m, x, y) || partFace(m, x, y); }
// the room as it is seen from a shell wall: floor and the visible faces
bool roomArea(const Map& m, int x, int y) { return m.in(x, y) && (!isWallT(m, x, y) || anyFace(m, x, y)); }
// kept for the old "is this a back-wall face" question (row 1 with floor below)
bool isFace(const Map& m, int x, int y) { return m.in(x, y) && m.at(x, y) == Ground::InteriorWall && !isWallT(m, x, y + 1); }

art::RoomStyle roomStyleOf(const Map& m) {
  for (int x = 1; x < m.w - 1; x++) {
    int d = m.decoAt(x, 1);
    if (d >= (int)Deco::WallTimber && d <= (int)Deco::WallPlaster) return (art::RoomStyle)(d - (int)Deco::WallTimber);
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
    // M0b furniture
    case Prop::Nightstand: w = 11; h = 4; break;
    case Prop::Washstand: case Prop::Lectern: w = 12; h = 4; break;
    case Prop::Oven: w = 18; h = 5; break;
    case Prop::PrepTable: case Prop::DisplayTable: w = 15; h = 5; break;
    case Prop::BottleShelf: case Prop::WeaponRack: case Prop::Dresser: w = 16; h = 5; break;
    case Prop::Candelabra: w = 8; h = 3; break;
    case Prop::QuenchTub: case Prop::Grindstone: w = 15; h = 5; break;
    case Prop::Pillar: w = 17; h = 6; break;
    case Prop::BunkBed: w = 20; h = 6; break;
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
    art::FloorStyle fs = art::FloorStyle::Planks;
    if (g == Ground::StoneFloor)
      fs = rs == art::RoomStyle::Hall ? art::FloorStyle::Slab : (rs == art::RoomStyle::Adobe ? art::FloorStyle::Terracotta : art::FloorStyle::Flagstone);
    else if (rs == art::RoomStyle::Log) fs = art::FloorStyle::Rushes;
    else if (rs == art::RoomStyle::Soot) fs = art::FloorStyle::OldPlanks;
    int mask = (isWallT(m, tx, ty - 1) ? 1 : 0) | (isWallT(m, tx - 1, ty) ? 2 : 0) | (isWallT(m, tx + 1, ty) ? 4 : 0) | (isWallT(m, tx, ty + 1) ? 8 : 0);
    add(art::pieceKey(art::Piece::Floor, (int)fs | mask << 3, tx, ty), 0, 0, 0);
    if (tx == m.exitX && ty == m.exitY) add(art::pieceKey(art::Piece::Door, (int)rs), 0, 0, 0);
    int pr = m.propAt(tx, ty);
    // an interior doorway: a threshold across the wall's line (an E-W doorway also has its passage tile behind)
    if (pr == (int)Prop::DoorH + 1) add(art::pieceKey(art::Piece::Sill, (int)rs, 0), 0, 0, 0);
    else if (pr == (int)Prop::DoorV + 1) add(art::pieceKey(art::Piece::Sill, (int)rs, 1), 0, 0, 0);
    if (d >= (int)Deco::RugRed && d <= (int)Deco::RugGold) {
      int nm = (m.decoAt(tx, ty - 1) == d ? 1 : 0) | (m.decoAt(tx + 1, ty) == d ? 2 : 0) | (m.decoAt(tx, ty + 1) == d ? 4 : 0) | (m.decoAt(tx - 1, ty) == d ? 8 : 0);
      add(art::pieceKey(art::Piece::Rug, d - (int)Deco::RugRed, nm), 0, 0, 1);
    }
    int w, h, ox, oy;
    if (pr && shadowOf((Prop)(pr - 1), w, h, ox, oy)) add(art::pieceKey(art::Piece::Shadow, 0, w, h), ox, oy, 2);
    clutter();
    return;
  }
  // the neighbours of a wall top: open (floor or a visible face) and which of those are faces
  auto openMasks = [&](int& open, int& faces) {
    static const int dx[8] = {1, -1, 0, 0, 1, -1, 1, -1}, dy[8] = {0, 0, -1, 1, -1, -1, 1, 1};
    static const int bit[8] = {art::CapE, art::CapW, art::CapN, art::CapS, art::CapNE, art::CapNW, art::CapSE, art::CapSW};
    open = faces = 0;
    for (int k = 0; k < 8; k++) {
      int x = tx + dx[k], y = ty + dy[k];
      if (!roomArea(m, x, y)) continue;
      open |= bit[k];
      if (anyFace(m, x, y)) faces |= bit[k];
    }
  };
  auto seedOf = [&]() { return (int)((uint32_t)(tx * 7 + ty * 13) % 31u); };
  if (backFace(m, tx, ty)) {
    if (ty == 0) return;   // the upper part of the back wall: painted by the face below
    // ends: shade where the side wall meets the face; a low partition casts its shadow onto the face's foot
    int ends = (!backCol(m, tx - 1) ? 16 : 0) | (!backCol(m, tx + 1) ? 32 : 0);
    if (innerWall(m, tx - 1, 2)) ends |= 64;
    if (innerWall(m, tx + 1, 2)) ends |= 128;
    add(art::pieceKey(art::Piece::BackWall, (int)rs | ends, tx), 0, -32, 0);
    // an N-S partition hangs from the back wall here: its top rises over the foot of the face
    if (innerWall(m, tx, 2)) {
      int open, faces;
      openMasks(open, faces);
      open |= art::CapN; faces |= art::CapN;   // the back wall's face behind it
      add(art::pieceKey(art::Piece::PartCap, (int)rs | seedOf() << 3, open, faces), 0, 0, 0);
    }
    return;
  }
  if (partFace(m, tx, ty)) {
    auto end = [&](int x) {
      if (partFace(m, x, ty)) return (int)art::PartEndRun;
      if (!isWallT(m, x, ty)) return (int)art::PartEndFree;
      return (int)art::PartEndWall;
    };
    add(art::pieceKey(art::Piece::PartFace, (int)rs | end(tx - 1) << 3 | end(tx + 1) << 5, tx, ty), 0, 0, 0);
    return;
  }
  if (partCap(m, tx, ty)) {
    int open, faces;
    openMasks(open, faces);
    // fix round 2: an N-S partition's first tile below the back wall: the partition's top already runs on up over the
    // foot of the face (the PartCap on the back-wall tile), so that is no edge here (it drew a seam band)
    if (backFace(m, tx, ty - 1) && innerWall(m, tx, ty)) { open &= ~(int)art::CapN; faces &= ~(int)art::CapN; }
    add(art::pieceKey(art::Piece::PartCap, (int)rs | seedOf() << 3, open, faces), 0, 0, 0);
    return;
  }
  // the shell: a strip of wall top along the room; a partition's top continues into it without an edge
  int mask = 0;
  auto openT = [&](int x, int y) { return roomArea(m, x, y) || innerWall(m, x, y); };
  if (openT(tx + 1, ty)) mask |= art::CapE;
  if (openT(tx - 1, ty)) mask |= art::CapW;
  if (openT(tx, ty - 1)) mask |= art::CapN;
  if (openT(tx, ty + 1)) mask |= art::CapS;
  if (openT(tx + 1, ty - 1)) mask |= art::CapNE;
  if (openT(tx - 1, ty - 1)) mask |= art::CapNW;
  if (openT(tx + 1, ty + 1)) mask |= art::CapSE;
  if (openT(tx - 1, ty + 1)) mask |= art::CapSW;
  int part = (innerWall(m, tx + 1, ty) ? 64 : 0) | (innerWall(m, tx - 1, ty) ? 128 : 0);
  int partN = innerWall(m, tx, ty - 1) ? 128 : 0;
  if (mask) add(art::pieceKey(art::Piece::Cap, (int)rs | part, mask, ((tx + ty) & 127) | partN), 0, 0, 0);
  // the side walls rise with the back wall above the map's top row: one more strip that joins the wall's top
  if (ty == 0 && (mask & (art::CapE | art::CapW))) {
    int em = 0, fl = 4;
    if ((mask & art::CapE) && backCol(m, tx + 1)) { em |= art::CapE | art::CapSE; fl |= 1; }
    if ((mask & art::CapW) && backCol(m, tx - 1)) { em |= art::CapW | art::CapSW; fl |= 2; }
    if (em) add(art::pieceKey(art::Piece::Cap, (int)rs | fl << 3, em, (tx + 31) & 127), 0, -16, 0);
  }
}

art::RoomStyle interiorStyle(const Map& m) { return roomStyleOf(m); }

uint32_t interiorPropKey(const Map& m, int tx, int ty, Prop p) {
  if (m.kind != MapKind::Interior) return 0;
  // (M3 fixer round 2) a hammock slung from the wall or a sleeping mat rolled out, two tiles long like a bed
  if ((p == Prop::Hammock || p == Prop::SleepingMat) && m.propAt(tx, ty - 1) == (int)Prop::Filler + 1)
    return art::pieceKey(art::Piece::Styled, (int)roomStyleOf(m), p == Prop::Hammock ? 6 : 7, (int)(hash2(tx, ty, 79) & 3));
  // (M3 fixer) a people's own furniture (Map::kit): the view adds the hearth's frame
  if (m.kit && art::cultureInteriorHas(m.kit - 1, p) && !((p == Prop::Painting || p == Prop::Wreath) && partFace(m, tx, ty)))
    return art::pieceKey(art::Piece::Culture, m.kit - 1, (int)p, p == Prop::Bed && m.propAt(tx, ty - 1) == (int)Prop::Filler + 1 ? 1 : 0);
  int idx = -1;
  switch (p) {
    case Prop::StairsUp: idx = 0; break;
    case Prop::StairsDown: idx = 1; break;
    case Prop::DoorH: idx = 2; break;
    case Prop::DoorV: idx = 3; break;
    default: break;
  }
  // M0b fix round: a two-tile bed (the head tile holds Filler) is drawn long, in one of four quilts
  if (p == Prop::Bed && m.propAt(tx, ty - 1) == (int)Prop::Filler + 1)
    return art::pieceKey(art::Piece::Styled, (int)roomStyleOf(m), 4, (int)(hash2(tx, ty, 77) & 3));
  // M0b fix round 2: a shop's counter shows the trade's things (ledger, scales, cloth, coins), not the inn's tankards
  if (p == Prop::CounterL || p == Prop::CounterM || p == Prop::CounterR) {
    int ri = m.roomIndexAt(tx, ty);
    if (ri >= 0 && ri < (int)m.rooms.size() && m.rooms[(size_t)ri].kind == RoomKind::Shopfloor) {
      int part = p == Prop::CounterL ? 0 : (p == Prop::CounterM ? 1 : 2);
      int goods = (int)((hash2(tx, ty, 91) + (uint32_t)tx) & 3);
      return art::pieceKey(art::Piece::Styled, (int)roomStyleOf(m), 5, part | goods << 2);
    }
  }
  if (idx >= 0) {
    int variant = (p == Prop::StairsUp && partFace(m, tx, ty - 1)) ? 1 : 0;
    // M0b fix round: a two-tile flight paints its two halves (2: the left tile, 4: the right one)
    if (idx <= 1) {
      int self = (int)p + 1;
      if (m.propAt(tx + 1, ty) == self) variant |= 2;
      else if (m.propAt(tx - 1, ty) == self) variant |= 4;
    }
    return art::pieceKey(art::Piece::Styled, (int)roomStyleOf(m), idx, variant);
  }
  // wall decor hung on a partition's short face
  if ((int)p >= (int)Prop::Tapestry && (int)p <= (int)Prop::HolySymbol && partFace(m, tx, ty))
    return art::pieceKey(art::Piece::LowDecor, (int)p - (int)Prop::Tapestry);
  return 0;
}
