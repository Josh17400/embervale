// Simulation rules for props: which ones block movement. Also decoLayers (rpg/sim/deco.h): which interior pieces a tile
// shows, shared by the renderer and the props gallery.
// WARNING: world generation calls propSolid (vegetation clearing, den placement), so changing the answer for an
// existing prop changes the generated world of every version. Only add cases for props appended after M0 began
// (Prop::Bed... are old; check art_props.h) unless the change is gated on a world-gen version.
#include <algorithm>
#include <cmath>

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
    // M3c: the Wildlands' ground cover is walked through (art_props.h: Heather .. Petals)
    case Prop::Heather: case Prop::Wildflowers: case Prop::PrairieGrass: case Prop::CottonGrass: case Prop::Agave:
    case Prop::DryBrush: case Prop::Saltbush: case Prop::Lichen: case Prop::Wrack: case Prop::Shells: case Prop::GlowCaps:
    case Prop::Blightweed: case Prop::SilverFern: case Prop::JungleFern: case Prop::Petals:
      return false;
    // M4: ash is a scorch on the ground, a fallen journal lies on the floor, a mural hangs on a wall tile
    case Prop::Ash: case Prop::LostJournal: case Prop::Mural:
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
// M3b shaped floors (a round room, an L's yard corner, a cross's arms: Ground::Void beyond the shell, Deco::Shell on the
// shell's tiles, Deco::Shell at (0, 0) marks the map): the shell's tiles with the room right below them are tall faces
// like the back wall's, wherever the outline puts them (a round room's back arc steps down round the circle); the rest
// of the shell is a strip of wall top that bends with the outline (rounded where it turns); the void is black.
namespace {
bool isWallT(const Map& m, int x, int y) { return !m.in(x, y) || m.at(x, y) == Ground::InteriorWall || m.at(x, y) == Ground::Void; }
bool isVoidT(const Map& m, int x, int y) { return !m.in(x, y) || m.at(x, y) == Ground::Void; }
bool shapedMap(const Map& m) { return m.decoAt(0, 0) == (int)Deco::Shell; }
bool shellT(const Map& m, int x, int y) { return m.in(x, y) && m.at(x, y) == Ground::InteriorWall && m.decoAt(x, y) == (int)Deco::Shell; }
// a column of the back wall: rows 0..1 are wall over an inner column (the tall face is drawn there)
bool backCol(const Map& m, int x) { return x >= 1 && x <= m.w - 2 && isWallT(m, x, 0) && isWallT(m, x, 1) && m.h > 2; }
bool backFace(const Map& m, int x, int y) { return y >= 0 && y <= 1 && backCol(m, x); }
// an inner wall tile (partition) not on the shell
bool innerWall(const Map& m, int x, int y) {
  return m.in(x, y) && x >= 1 && x <= m.w - 2 && y >= 2 && y <= m.h - 2 && m.at(x, y) == Ground::InteriorWall && m.decoAt(x, y) != (int)Deco::Shell;
}
// (shaped floors) the foot of a tall shell face: a shell tile with the room (or a partition hanging from it) right below
// and no floor right above
bool footT(const Map& m, int x, int y) {
  if (!shellT(m, x, y) || !m.in(x, y + 1)) return false;
  const bool below = !isWallT(m, x, y + 1) || innerWall(m, x, y + 1);
  return below && isWallT(m, x, y - 1);
}
// a partition face: an inner wall tile with floor right below it
// (M5 fixer r2) a north-south partition runs on through its doorway: the wall tile over a DoorV is the run's cap, never a
// face (a face there read as a stack of separate blocks with a door stuck to the wall's side)
bool doorVBelow(const Map& m, int x, int y) { return m.in(x, y + 1) && m.propAt(x, y + 1) == (int)Prop::DoorV + 1; }
bool partFace(const Map& m, int x, int y) { return innerWall(m, x, y) && !isWallT(m, x, y + 1) && !doorVBelow(m, x, y); }
bool partCap(const Map& m, int x, int y) { return innerWall(m, x, y) && (isWallT(m, x, y + 1) || doorVBelow(m, x, y)); }
// a visible face of any wall (it counts as room for the wall tops around it); on shaped floors a tall shell face covers
// its foot's tile, the one above and (its top band and the top of the face) the one above that
bool shellFaceAt(const Map& m, int x, int y) { return footT(m, x, y) || footT(m, x, y + 1) || footT(m, x, y + 2); }
bool anyFace(const Map& m, int x, int y) { return backFace(m, x, y) || partFace(m, x, y) || (shapedMap(m) && shellFaceAt(m, x, y)); }
// the room as it is seen from a shell wall: floor and the visible faces
bool roomArea(const Map& m, int x, int y) { return m.in(x, y) && (!isWallT(m, x, y) || anyFace(m, x, y)); }
// kept for the old "is this a back-wall face" question (row 1 with floor below)
bool isFace(const Map& m, int x, int y) { return m.in(x, y) && m.at(x, y) == Ground::InteriorWall && !isWallT(m, x, y + 1); }

art::RoomStyle roomStyleOf(const Map& m) {
  // (M3b) row 0 carries it in every interior made since; row 1 in the older ones
  for (int row = 0; row <= 1; row++)
    for (int x = 0; x < m.w; x++) {
      int d = m.decoAt(x, row);
      if (d >= (int)Deco::WallTimber && d <= (int)Deco::WallTile) return (art::RoomStyle)(d - (int)Deco::WallTimber);
    }
  // interiors made before WORLDGEN_V3 carry no style: derive it from the floor
  for (int y = 0; y < m.h; y++)
    for (int x = 0; x < m.w; x++)
      if (m.at(x, y) == Ground::StoneFloor) return art::RoomStyle::Stone;
  return art::RoomStyle::Timber;
}
// (M3b) the room styles need four bits: the fourth rides in the key's extra bits (art::pieceKeyX)
uint32_t rsKey(art::Piece k, art::RoomStyle rs, int styleRest, int a = 0, int b = 0, int x = 0) {
  return art::pieceKeyX(k, ((int)rs >> 3) | (x << 1), ((int)rs & 7) | styleRest, a, b);
}
// the floor a tile shows: the room's own (RoomInfo::floorStyle), an open court's paving, else by the room style
art::FloorStyle floorStyleOf(const Map& m, int tx, int ty, art::RoomStyle rs) {
  const Ground g = m.at(tx, ty);
  if (g == Ground::Plaza) return art::FloorStyle::Court;
  const int ri = m.roomIndexAt(tx, ty);
  if (ri >= 0 && ri < (int)m.rooms.size() && m.rooms[(size_t)ri].floorStyle) return (art::FloorStyle)(m.rooms[(size_t)ri].floorStyle - 1);
  switch (rs) {
    case art::RoomStyle::Felt: return art::FloorStyle::Felt;
    case art::RoomStyle::Living: return art::FloorStyle::Roots;
    case art::RoomStyle::Marble: return g == Ground::StoneFloor ? art::FloorStyle::Marble : art::FloorStyle::Planks;
    case art::RoomStyle::Tile: return g == Ground::StoneFloor ? art::FloorStyle::Mosaic : art::FloorStyle::Terracotta;
    case art::RoomStyle::Paper: return g == Ground::StoneFloor ? art::FloorStyle::Slab : art::FloorStyle::Planks;
    default: break;
  }
  if (g == Ground::StoneFloor) return rs == art::RoomStyle::Hall ? art::FloorStyle::Slab : (rs == art::RoomStyle::Adobe ? art::FloorStyle::Terracotta : art::FloorStyle::Flagstone);
  if (rs == art::RoomStyle::Log) return art::FloorStyle::Rushes;
  if (rs == art::RoomStyle::Soot) return art::FloorStyle::OldPlanks;
  return art::FloorStyle::Planks;
}
// a floor key: the floor style's fourth bit in the style byte's top bit (the wall-shadow mask is bits 3..6)
uint32_t floorKey(art::FloorStyle fs, int mask, int tx, int ty) {
  return art::pieceKey(art::Piece::Floor, ((int)fs & 7) | mask << 3 | (((int)fs >> 3) & 1) << 7, tx, ty);
}

// (M3b round 3) a round room (Plan::inShape's disc, marked by Deco::Shell at the end of row 0): the signed distance in
// px from a pixel to its outline (> 0 outside) and the outward normal there. Its front arc and sides are drawn along
// this smooth outline (Piece::RoundEdge) instead of tile steps; the back arc keeps its tall faces.
bool roundMap(const Map& m) { return shapedMap(m) && m.w > 2 && m.decoAt(m.w - 1, 0) == (int)Deco::Shell; }
float roundDist(const Map& m, float px, float py, float& nx, float& ny) {
  const float k = 1.0246951f;   // sqrt(21 / 20): inShape's threshold
  const float ax = 8.0f * (float)(m.w - 2) * k, ay = 8.0f * (float)(m.h - 3) * k;
  const float X = px - 8.0f * (float)m.w, Y = py - 8.0f * (float)m.h - 8.0f;
  const float q = std::sqrt((X * X) / (ax * ax) + (Y * Y) / (ay * ay));
  float gx = X / (ax * ax), gy = Y / (ay * ay);
  const float gl = std::sqrt(gx * gx + gy * gy);
  if (q < 1e-4f || gl < 1e-9f) { nx = 0; ny = 1; return -1e9f; }
  nx = gx / gl; ny = gy / gl;
  return (q - 1.0f) * q / gl;
}
// (M3b fixer) a round room's back arc on the true ellipse: the foot of the far wall at map px column x (the back half
// of roundDist's outline), and whether tile column tx is part of the back arc (the outline's normal there points back,
// as RoundEdge decides for its tiles). anchor: the row of the tile holding the column's lowest foot; fl / fr: the foot's
// row at the column's left / right edge, px from the anchor tile's top.
float roundBackFoot(const Map& m, float x) {
  const float k = 1.0246951f;
  const float ax = 8.0f * (float)(m.w - 2) * k, ay = 8.0f * (float)(m.h - 3) * k;
  const float t = std::clamp((x - 8.0f * (float)m.w) / ax, -1.0f, 1.0f);
  return 8.0f * (float)m.h + 8.0f - ay * std::sqrt(std::max(0.0f, 1.0f - t * t));
}
bool roundBackCol(const Map& m, int tx, int& anchor, int& fl, int& fr) {
  if (!roundMap(m) || tx < 1 || tx > m.w - 2) return false;
  const float cx = tx * 16.0f + 8.0f, cy = roundBackFoot(m, cx);
  float nx, ny;
  roundDist(m, cx, cy, nx, ny);
  if (ny >= -0.72f) return false;   // (the steep ends are seen edge-on: the side strip carries on there)
  const float yl = roundBackFoot(m, tx * 16.0f), yr = roundBackFoot(m, tx * 16.0f + 16.0f);
  anchor = (int)std::floor((std::max(yl, yr) - 0.01f) / 16.0f);
  if (anchor < 1 || anchor >= m.h - 1) return false;
  fl = (int)std::lround(yl - anchor * 16.0f);
  fr = (int)std::lround(yr - anchor * 16.0f);
  return true;
}

// the RoundEdge piece for tile (tx, ty), or 0 where the plain pieces draw it (the back arc's tall faces, the doorway)
uint32_t roundEdgeKey(const Map& m, int tx, int ty, art::RoomStyle rs) {
  if (!roundMap(m)) return 0;
  {
    int an, a1, a2;
    if (roundBackCol(m, tx, an, a1, a2) && ty <= an) return 0;   // the back arc's face covers it
  }
  if (std::abs(tx - m.w / 2) <= 1 && ty >= m.h - 3) return 0;   // the doorway and its jambs
  float nx, ny;
  const float dc = roundDist(m, tx * 16.0f + 8.0f, ty * 16.0f + 8.0f, nx, ny);
  if (dc <= -12.0f) return 0;
  // (toward the back the face's columns are the face's, above their feet; beyond them the strip runs on round the curve)
  int ang = (int)std::lround(std::atan2(ny, nx) / 6.2831853f * 64.0f);
  ang = ((ang % 64) + 64) % 64;
  const int off = std::clamp((int)std::lround(dc), -60, 60) + 64;
  return art::pieceKeyX(art::Piece::RoundEdge, ((int)rs >> 3) & 1, (int)rs & 7, ang, off);
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
    // M3b furniture
    case Prop::Stove: w = 14; h = 5; break;
    case Prop::TrainingDummy: w = 12; h = 4; break;
    case Prop::FoldScreen: w = 22; h = 4; break;
    case Prop::Well: w = 22; h = 6; break;
    case Prop::Fountain: w = 40; h = 7; break;
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
  const bool shaped = shapedMap(m);
  // (M3b fixer) a round room's back arc: one face per column on the true ellipse (Piece::RoundBack), anchored on the tile
  // holding its lowest foot; the wall tiles above it are black behind the face
  int rbAnchor = -1, rbL = 0, rbR = 0;
  uint32_t rbKey = 0;
  if (shaped && roundBackCol(m, tx, rbAnchor, rbL, rbR)) {
    if (ty < rbAnchor) {
      // the face (anchored below) paints black above its top band; under its foot the floor runs on to the curve
      if (isWallT(m, tx, ty)) {
        if (ty >= rbAnchor - 2) add(floorKey(floorStyleOf(m, tx, rbAnchor + 1, rs), 0, tx, ty), 0, 0, 0);
        else add(rsKey(art::Piece::Cap, rs, 0, 0, 0), 0, 0, 0);
        return;
      }
    } else if (ty > rbAnchor && shellT(m, tx, ty) && !isWallT(m, tx, ty + 1)) {   // the true curve runs above: floor
      add(floorKey(floorStyleOf(m, tx, ty + 1, rs), 0, tx, ty), 0, 0, 0);
      return;
    } else if (ty == rbAnchor) {
      int an2, l2, r2;
      const int ends = (roundBackCol(m, tx - 1, an2, l2, r2) ? 0 : 8) | (roundBackCol(m, tx + 1, an2, l2, r2) ? 0 : 16);
      rbKey = art::pieceKeyX(art::Piece::RoundBack, (((int)rs >> 3) & 1) | ((tx & 3) << 1), ((int)rs & 7) | ends, std::clamp(rbL, -60, 16) + 64,
                             std::clamp(rbR, -60, 16) + 64);
      if (isWallT(m, tx, ty)) {   // the foot's tile is wall: the floor runs on to the curve, the face over it
        add(floorKey(floorStyleOf(m, tx, ty + 1, rs), 0, tx, ty), 0, 0, 0);
        add(rbKey, 0, -64, 0);
        if (innerWall(m, tx, ty + 1)) {   // a partition hangs from the face: its top rises over the face's foot
          add(rsKey(art::Piece::PartCap, rs, (int)((uint32_t)(tx * 7 + ty * 13) % 31u) << 3, art::CapN | art::CapS, art::CapN), 0, 0, 0);
        }
        return;
      }
    }
  }
  if (shaped && isVoidT(m, tx, ty)) {   // beyond the shell: black (a round room's strip may reach over it)
    const uint32_t re = roundEdgeKey(m, tx, ty, rs);
    add(re ? re : rsKey(art::Piece::Cap, rs, 0, 0, 0), 0, 0, 0);
    return;
  }
  if (!isWallT(m, tx, ty)) {
    Ground g = m.at(tx, ty);
    art::FloorStyle fs = floorStyleOf(m, tx, ty, rs);
    int mask = (isWallT(m, tx, ty - 1) ? 1 : 0) | (isWallT(m, tx - 1, ty) ? 2 : 0) | (isWallT(m, tx + 1, ty) ? 4 : 0) | (isWallT(m, tx, ty + 1) ? 8 : 0);
    // (M3b fixer) a round room's outer wall is drawn on the true curve with its own contact shadow: the tile grid's
    // shadow edges along the shell would show as steps round the circle (partitions keep theirs)
    if (roundMap(m)) {
      auto shellish = [&](int x, int y) { return isWallT(m, x, y) && !innerWall(m, x, y); };
      if (shellish(tx, ty - 1)) mask &= ~1;
      if (shellish(tx - 1, ty)) mask &= ~2;
      if (shellish(tx + 1, ty)) mask &= ~4;
      if (shellish(tx, ty + 1)) mask &= ~8;
    }
    if (groundWater(g)) {
      // (M3b) a bath pool: its water, the coping round it, the far side's wall below the coping; the floor round it
      static const int ndx[8] = {1, -1, 0, 0, 1, -1, 1, -1}, ndy[8] = {0, 0, -1, 1, -1, -1, 1, 1};
      static const int bit[8] = {art::CapE, art::CapW, art::CapN, art::CapS, art::CapNE, art::CapNW, art::CapSE, art::CapSW};
      int wm = 0;
      for (int k = 0; k < 8; k++) if (groundWater(m.at(tx + ndx[k], ty + ndy[k]))) wm |= bit[k];
      const art::FloorStyle around = floorStyleOf(m, tx, ty - 1 >= 0 && !groundWater(m.at(tx, ty - 1)) ? ty - 1 : ty, rs);
      add(art::pieceKeyX(art::Piece::Pool, 0, ((int)around & 15) | ((int)rs & 15) << 4, wm, (tx * 7 + ty * 13) & 255), 0, 0, 0);
      return;
    }
    add(floorKey(fs, mask, tx, ty), 0, 0, 0);
    if (const uint32_t re = roundEdgeKey(m, tx, ty, rs)) add(re, 0, 0, 0);   // (a round room's outline over its edge)
    if (rbKey) add(rbKey, 0, -64, 0);   // (a round room's back arc, its foot in this floor tile)
    // the way out: a door's threshold, or (owner 2026-10-06) an open front's bay: daylight and the pillars' sections
    if (m.isExit(tx, ty)) add(art::pieceKey(art::Piece::Door, (int)rs, m.exits.empty() || (tx == m.exitX && !m.exitOpen) ? 0 : 1), 0, 0, 0);
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
  if (shaped && shellT(m, tx, ty)) {
    if (roundMap(m) && rbAnchor < 0) {   // (M3b fixer) a round room off its back arc: the strip along the true outline
      if (const uint32_t re = roundEdgeKey(m, tx, ty, rs)) {
        int fx = tx, fy = ty;
        static const int ddx[4] = {0, 1, -1, 0}, ddy[4] = {-1, 0, 0, 1};
        for (int k = 0; k < 4; k++)
          if (!isWallT(m, tx + ddx[k], ty + ddy[k])) { fx = tx + ddx[k]; fy = ty + ddy[k]; break; }
        add(floorKey(floorStyleOf(m, fx, fy, rs), 0, tx, ty), 0, 0, 0);
        add(re, 0, 0, 0);
        return;
      }
    }
    if (footT(m, tx, ty)) {
      // the tall face of the outer wall: the back wall's own piece, on whatever row the outline puts its foot. Its ends:
      // a side wall beside it (shade), or an open end where the outline steps down beside it (the floor runs on there)
      int ends = 0;
      if (!footT(m, tx - 1, ty)) ends |= isWallT(m, tx - 1, ty + 1) && !innerWall(m, tx - 1, ty + 1) ? 16 : 0;
      if (!footT(m, tx + 1, ty)) ends |= isWallT(m, tx + 1, ty + 1) && !innerWall(m, tx + 1, ty + 1) ? 32 : 0;
      if (innerWall(m, tx - 1, ty + 1)) ends |= 64;
      if (innerWall(m, tx + 1, ty + 1)) ends |= 128;
      // the steps of a curved wall: where the neighbour's face stands a row higher (it is further back) or lower, this
      // face's top meets it (ShellFace draws the top's rise toward each neighbour)
      auto rise = [&](int x) {
        for (int k = 1; k <= 3; k++) { if (footT(m, x, ty - k)) return k; if (footT(m, x, ty + k)) return -k; }
        return 0;
      };
      const int rl = rise(tx - 1), rr = rise(tx + 1);
      if (rl == 0 && rr == 0) add(art::pieceKey(art::Piece::BackWall, (int)rs | ends, tx), 0, -32, 0);
      else add(art::pieceKeyX(art::Piece::ShellFace, 0, (int)rs | ((ends >> 4) & 15) << 4, tx, (std::clamp(rl, -3, 3) + 4) | (std::clamp(rr, -3, 3) + 4) << 4), 0, -48, 0);
      if (innerWall(m, tx, ty + 1)) {   // a partition hangs from the face: its top rises over the face's foot
        int open, faces;
        openMasks(open, faces);
        open |= art::CapN; faces |= art::CapN;
        add(rsKey(art::Piece::PartCap, rs, seedOf() << 3, open, faces), 0, 0, 0);
      }
      return;
    }
    if (shellFaceAt(m, tx, ty)) return;   // drawn by the tall face below it
    if (const uint32_t re = roundEdgeKey(m, tx, ty, rs)) {
      // (M3b round 3) a round room's front arc and sides: the floor runs out to the true outline, the strip along it
      int fx = tx, fy = ty;
      static const int ddx[4] = {0, 1, -1, 0}, ddy[4] = {-1, 0, 0, 1};
      for (int k = 0; k < 4; k++)
        if (!isWallT(m, tx + ddx[k], ty + ddy[k])) { fx = tx + ddx[k]; fy = ty + ddy[k]; break; }
      add(floorKey(floorStyleOf(m, fx, fy, rs), 0, tx, ty), 0, 0, 0);
      add(re, 0, 0, 0);
      return;
    }
    // a strip of wall top along the room, bending round the outline
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
    // a tall face's top band beside this tile: the strip joins it
    int fl = (footT(m, tx + 1, ty + 2) ? 1 : 0) | (footT(m, tx - 1, ty + 2) ? 2 : 0);
    int part = (innerWall(m, tx + 1, ty) ? 64 : 0) | (innerWall(m, tx - 1, ty) ? 128 : 0);
    int partN = innerWall(m, tx, ty - 1) ? 128 : 0;
    if (mask) add(rsKey(art::Piece::Cap, rs, fl << 3 | part, mask, ((tx + ty) & 127) | partN, 1), 0, 0, 0);
    else add(rsKey(art::Piece::Cap, rs, 0, 0, 0), 0, 0, 0);   // (a shell tile out of the room's sight: black)
    return;
  }
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
      add(rsKey(art::Piece::PartCap, rs, seedOf() << 3, open, faces), 0, 0, 0);
    }
    return;
  }
  if (partFace(m, tx, ty)) {
    auto end = [&](int x) {
      if (partFace(m, x, ty)) return (int)art::PartEndRun;
      if (!isWallT(m, x, ty)) return (int)art::PartEndFree;
      return (int)art::PartEndWall;
    };
    add(rsKey(art::Piece::PartFace, rs, end(tx - 1) << 3 | end(tx + 1) << 5, tx, ty), 0, 0, 0);
    return;
  }
  if (partCap(m, tx, ty)) {
    int open, faces;
    openMasks(open, faces);
    // fix round 2: an N-S partition's first tile below the back wall: the partition's top already runs on up over the
    // foot of the face (the PartCap on the back-wall tile), so that is no edge here (it drew a seam band)
    if ((backFace(m, tx, ty - 1) || (shaped && footT(m, tx, ty - 1))) && innerWall(m, tx, ty)) { open &= ~(int)art::CapN; faces &= ~(int)art::CapN; }
    add(rsKey(art::Piece::PartCap, rs, seedOf() << 3, open, faces), 0, 0, 0);
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
  if (mask) add(rsKey(art::Piece::Cap, rs, part, mask, ((tx + ty) & 127) | partN), 0, 0, 0);
  // the side walls rise with the back wall above the map's top row: one more strip that joins the wall's top
  if (ty == 0 && (mask & (art::CapE | art::CapW))) {
    int em = 0, fl = 4;
    if ((mask & art::CapE) && backCol(m, tx + 1)) { em |= art::CapE | art::CapSE; fl |= 1; }
    if ((mask & art::CapW) && backCol(m, tx - 1)) { em |= art::CapW | art::CapSW; fl |= 2; }
    if (em) add(rsKey(art::Piece::Cap, rs, fl << 3, em, (tx + 31) & 127), 0, -16, 0);
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
    const RoomKind rk = ri >= 0 && ri < (int)m.rooms.size() ? m.rooms[(size_t)ri].kind : RoomKind::Common;
    if (rk == RoomKind::Shopfloor || rk == RoomKind::Trading || rk == RoomKind::TeaRoom) {   // (M3b: the exchange's, the tea house's)
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
  // (M3b) a seat of power's high seat in its people's idiom (the khan's cushioned dais, the jarl's carved high seat,
  // the elven councils' thrones...); a column in the room's own material (a living trunk, white marble, red lacquer, the
  // yurt's painted roof poles)
  if (p == Prop::Throne && m.kit) return art::pieceKey(art::Piece::Styled, (int)roomStyleOf(m), art::kStyledThrone, m.kit - 1);
  // (fix) a log or timber hall's posts are carved timber, never the stone classical column
  if (p == Prop::Pillar && ((int)roomStyleOf(m) >= (int)art::RoomStyle::Felt || roomStyleOf(m) == art::RoomStyle::Log || roomStyleOf(m) == art::RoomStyle::Timber)) return art::pieceKey(art::Piece::Styled, (int)roomStyleOf(m), 9, (int)(hash2(tx, ty, 83) & 3));
  // wall decor hung on a partition's short face
  if ((int)p >= (int)Prop::Tapestry && (int)p <= (int)Prop::HolySymbol && partFace(m, tx, ty))
    return art::pieceKey(art::Piece::LowDecor, (int)p - (int)Prop::Tapestry);
  return 0;
}
