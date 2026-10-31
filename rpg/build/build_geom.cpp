// Builder roof and volume helpers (SOLID 1.6, cut from builder.cpp): the painter's roof slopes and heights that
// MIRROR art_building.cpp, and the pieces every assembler uses (porch, side wing, end stacks, stilts). The pieces
// with default arguments (roofTo, flat, chimneys, towerVol, platformUnder) live inline in build_detail.h.
#include "rpg/build/build_detail.h"

namespace bld {

namespace {
// the painter's roof slopes (x100) by pitch: 0..4 the culture's roofs, 5..8 spires and needles
const int kSlope[9] = {45, 62, 80, 105, 140, 190, 230, 280, 340};

// MIRRORS the painter (art_building.cpp volumeMass): eave overhangs at the sides / back and at the front
void overhang(const Volume& x, int& ov, int& ovF) {
  ov = 3; ovF = 2;
  switch (x.roof) {
    case RoofShape::Turf: ov = 2; break;
    case RoofShape::Pagoda: ov = 5; ovF = 4; break;
    case RoofShape::Sweep: ov = 4; ovF = 3; break;
    case RoofShape::Mansard: ov = 2; ovF = 2; break;
    case RoofShape::Stepped: ov = 1; ovF = 1; break;
    case RoofShape::Tent: case RoofShape::Spire: ov = 1; ovF = 1; break;
    case RoofShape::FlatParapet: case RoofShape::Dome: case RoofShape::Onion: ov = 0; ovF = 0; break;
    default: break;
  }
  if ((x.roofMat == RoofMat::Thatch || x.roofMat == RoofMat::Palm) && pitchedRoof(x.roof) && x.roof != RoofShape::Tent && x.roof != RoofShape::Spire) { ov = 4; ovF = 3; }
  if (ov > 0 || ovF > 0) { ov += x.eave; ovF += x.eave; }
  if (x.role == VolRole::Chimney || x.role == VolRole::Platform || x.role == VolRole::Enclosure || x.role == VolRole::Tree) { ov = 0; ovF = 0; }
}
int slopeOf(const Volume& x) {
  int s = kSlope[std::clamp((int)x.pitch, 0, 8)];
  if (x.roof == RoofShape::Steep) s = std::max(s, 140);
  if (x.roof == RoofShape::Turf) s = std::clamp(s, 80, 105);
  return s;
}
}  // namespace
// MIRRORS the painter's roof heights (px)
int roofHeight(const Volume& x) {
  if (x.role == VolRole::Platform || x.role == VolRole::Enclosure || x.role == VolRole::Chimney || x.role == VolRole::Tree) return 0;
  int ov, ovF;
  overhang(x, ov, ovF);
  const int w = x.x1 - x.x0, d = x.y1 - x.y0;
  if (x.shape != VolShape::Box) {
    const int r = std::min(w, d) / 2;
    switch (x.roof) {
      case RoofShape::Dome: return r * 60 / 100;
      case RoofShape::Onion: return (r * 10 + 15) * 17 / 100;
      case RoofShape::FlatParapet: return 0;
      case RoofShape::Stepped: return r * 60 / 100;
      default:
        if (x.roofMat == RoofMat::Felt && x.roof == RoofShape::Conical) return (r + ov) * (52 + 8 * std::min(3, (int)x.pitch)) / 100;
        if (x.roof == RoofShape::Tent) return (r + ov) * 95 / 100;
        if (x.roof == RoofShape::Sweep) return (r + ov) * 115 / 100;   // (fixer: a slender swept cone)
        return (r + ov) * std::max(130, slopeOf(x) * 14 / 10) / 100;
    }
  }
  const int span = x.ridgeNS ? (w + 2 * ov) : (d + ov + ovF);
  switch (x.roof) {
    case RoofShape::FlatParapet: return 0;
    case RoofShape::Dome: return std::max(0, std::min(w, d) / 2 - 7) * 80 / 100;
    case RoofShape::Onion: return std::max(0, std::min(w, d) / 2 - 6) * 160 / 100;
    case RoofShape::Stepped: return span * std::max(70, slopeOf(x)) / 200;
    case RoofShape::Spire: return std::min(w + 2 * ov, d + ov + ovF) * slopeOf(x) / 200;
    case RoofShape::Tent: return span * 60 / 200;
    default: return span * slopeOf(x) / 200;
  }
}
// px the volume rises above the footprint's top edge (z - y at its highest point), roughly as the painter measures it
int riseOf(const Volume& x) {
  const int zt = x.z0 + x.wallH, rh = roofHeight(x);
  int ov, ovF;
  overhang(x, ov, ovF);
  if (x.role == VolRole::Chimney) return zt + 1 - x.y0;
  if (x.role == VolRole::Tree) return zt + 40 - (x.y0 + x.y1) / 2;   // the crown
  if (x.shape != VolShape::Box) {
    const int r = std::min(x.x1 - x.x0, x.y1 - x.y0) / 2, cy = x.y1 - r;
    if (x.roof == RoofShape::FlatParapet) return zt + 5 - (cy - r);
    return std::max(zt - 1 + rh - cy, zt + 2 - (cy - r));
  }
  switch (x.roof) {
    case RoofShape::FlatParapet: case RoofShape::Stepped: return std::max(zt + 6 - x.y0, zt + rh - (x.y0 + x.y1) / 2);
    case RoofShape::Dome: return std::max(zt + 6 - x.y0, zt + 3 + rh - (x.y0 + x.y1) / 2);
    case RoofShape::Onion: return std::max(zt + 6 - x.y0, zt + 6 + rh - (x.y0 + x.y1) / 2);
    default: break;
  }
  // the upswept eave corners of a pagoda or a sweep roof stand above the back eave
  const int lift = x.roof == RoofShape::Pagoda ? 6 : (x.roof == RoofShape::Sweep ? 8 : 0);
  if (x.ridgeNS) return std::max(zt - 1 + rh - (x.y1 - 8), zt + lift - (x.y0 - ov));   // the painter hips the back of a long ridge
  return std::max(zt - 1 + rh - (x.y0 - ov + x.y1 + ovF) / 2, zt + lift - (x.y0 - ov));
}

// the height of a box volume's pitched roof at depth y on its ridge line's column (an upper stage set on the roof
// stands with its front foot there)
int roofZAt(const Volume& x, int y) {
  int ov, ovF;
  overhang(x, ov, ovF);
  const int rh = roofHeight(x), zt = x.z0 + x.wallH - 1;
  if (!pitchedRoof(x.roof)) return zt + 1;
  if (x.ridgeNS) return zt + rh;
  const int ry0 = x.y0 - ov, ry1 = x.y1 + ovF, half = std::max(1, (ry1 - ry0) / 2);
  const int d = std::min(y - ry0, ry1 - y);
  return zt + rh * std::clamp(d, 0, half) / half;
}

// ---------------------------------------------------------------- pieces
// a door porch before the body's front (the body's front is set back by `back` px): a gabled porch on two posts, a
// columned portico under a pediment, a veranda of posts the width of the house, or an arcade; it carries the door
int porch(Bx& c, int bi, int kind, int halfW, int depth) {
  const Volume& bo = c.v[(size_t)bi];
  const int y1 = std::min(c.H + art::BLDG_PAD_B - 1, bo.y1 + depth);
  int x0 = c.dc - halfW, x1 = c.dc + halfW;
  if (kind == 2) { x0 = bo.x0 + 2; x1 = bo.x1 - 2; }
  Volume p = c.vol(VolRole::Porch, x0, bo.y1 - 1, x1, y1, std::max(14, std::min((int)bo.wallH - 4, 20)));
  p.z0 = bo.z0;
  p.window = bo.window; p.door = bo.door; p.wall = bo.wall; p.wallTint = bo.wallTint; p.trimTint = bo.trimTint;
  p.ornament = bo.ornament;
  p.feat = bo.feat & (VF_STILTS);
  p.roofMat = bo.roofMat; p.roofTint = bo.roofTint;
  switch (kind) {
    case 0: p.face = Face::Colonnade; p.roof = pitchedRoof(bo.roof) && bo.roof != RoofShape::Turf ? RoofShape::Gable : RoofShape::Hip; p.ridgeNS = true; p.pitch = (uint8_t)std::max(2, (int)bo.pitch); break;
    case 1: p.face = Face::Colonnade; p.roof = RoofShape::Gable; p.ridgeNS = true; p.pitch = 0; p.wallH = (int16_t)std::max(18, (int)bo.wallH - 2);
            if (p.roofMat == RoofMat::Thatch || p.roofMat == RoofMat::Palm || p.roofMat == RoofMat::Adobe) p.roofMat = RoofMat::ClayTile; break;
    case 2: p.face = Face::Veranda; p.roof = RoofShape::Hip; p.pitch = 1; p.wallH = (int16_t)std::max(14, std::min((int)bo.wallH - 6, 18)); break;
    default: p.face = Face::Arcade; p.roof = flatRoof(bo.roof) ? RoofShape::FlatParapet : RoofShape::Hip; p.pitch = 1; break;
  }
  if (p.roof == RoofShape::FlatParapet) { p.roofMat = RoofMat::Adobe; p.wallH = (int16_t)(p.wallH + 4); }
  if (p.roof == RoofShape::Turf) p.roof = RoofShape::Gable;
  c.v[(size_t)bi].doorHere = false;
  p.doorHere = true;
  return c.add(p);
}
namespace {
// a lower side wing beside the body (east or west), set back a little
int sideWing(Bx& c, int bi, bool east, int ww, int setBack, int wallH) {
  const Volume bo = c.v[(size_t)bi];
  Volume w = c.vol(VolRole::Wing, east ? bo.x1 : bo.x0 - ww, bo.y0 + setBack, east ? bo.x1 + ww : bo.x0, bo.y1 - (setBack > 8 ? 2 : 0), wallH);
  w.z0 = bo.z0;
  w.wall = bo.wall; w.wallTint = bo.wallTint; w.roofMat = bo.roofMat; w.roofTint = bo.roofTint; w.window = bo.window;
  w.feat = bo.feat & VF_STILTS;
  w.roof = bo.roof == RoofShape::Gable || bo.roof == RoofShape::Steep ? RoofShape::Hip : bo.roof;
  if (bo.shape != VolShape::Box) { w.roof = bo.roof; }
  w.ridgeNS = false;
  return c.add(w);
}
}  // namespace
// a gable-end stack at each end of a side-gabled cottage (highland, heartland)
void endStacks(Bx& c, int bi, int n) {
  if (!c.r.facts.hearth || n <= 0) return;
  const Volume bo = c.v[(size_t)bi];
  for (int i = 0; i < n; i++) {
    const bool east = n == 1 ? c.ch(128) : i == 1;
    const int x = east ? bo.x1 - 9 : bo.x0 + 3;
    Volume ch = c.vol(VolRole::Chimney, x, (bo.y0 + bo.y1) / 2 - 1, x + 6, (bo.y0 + bo.y1) / 2 + 3, 6);
    ch.wall = bo.wall == WallMat::Rubble || bo.wall == WallMat::Stone ? WallMat::Stone : WallMat::Brick;
    ch.roof = RoofShape::FlatParapet;
    ch.z0 = bo.z0;
    c.add(ch);
  }
}
void stilts(Bx& c, int h) {
  for (Volume& x : c.v) {
    if (x.role == VolRole::Enclosure || x.role == VolRole::Platform || x.role == VolRole::Tree) continue;
    x.z0 = (int16_t)(x.z0 + h);
    if (x.role != VolRole::Chimney) x.feat |= VF_STILTS;
  }
  c.grounded = true;
}

}  // namespace bld
