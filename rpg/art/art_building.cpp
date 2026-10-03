// EMBERVALE art: buildings, roofs, city walls and the gate. See rpg/art.h for the contract and rpg/art/art_internal.h for the shared helpers.
//
// Buildings are built from masses (boxes and cylinders: the main house, wings, cross gables, dormers, chimneys,
// turrets, steeples) and painted as real volumes in the oblique 3/4 view: each mass's front wall is a facade canvas
// (material, windows, doors) and all roofs form one height field, rendered column by column from back to front with
// per-pixel normals lit from the top-left, material texels laid along the courses, cast shadows from chimneys and
// turrets, fascia boards on every eave, and the eave's shadow on the wall below. The building type adds only its
// function (sign, forge, steeple, crenellations, awning); the ArchStyle decides how it is built.
#include "rpg/art/art_internal.h"

namespace art {

namespace {

// ---------------------------------------------------------------- palette (all from the shared hue-shifted family)
const Ramp kPlaster = ramp5(rgba(150, 128, 112), rgba(196, 174, 146), rgba(228, 212, 180), rgba(242, 232, 206), rgba(252, 248, 230));
const Ramp kPlasterOchre = ramp5(rgba(136, 96, 70), rgba(186, 142, 96), rgba(220, 184, 128), rgba(236, 210, 158), rgba(248, 234, 196));
const Ramp kPlasterRose = ramp5(rgba(140, 96, 98), rgba(190, 144, 134), rgba(222, 186, 168), rgba(238, 212, 194), rgba(250, 236, 222));
const Ramp kPlasterBlue = ramp5(rgba(98, 108, 132), rgba(150, 166, 182), rgba(200, 212, 216), rgba(226, 234, 232), rgba(246, 250, 244));
const Ramp kBeam = ramp5(rgba(40, 26, 32), rgba(64, 40, 38), rgba(92, 60, 46), rgba(120, 84, 58), rgba(150, 112, 76));
const Ramp kRoofRed = ramp5(rgba(76, 30, 40), rgba(124, 46, 44), rgba(170, 70, 52), rgba(204, 104, 70), rgba(232, 150, 102));
const Ramp kRoofSlate = ramp5(rgba(38, 42, 64), rgba(58, 66, 92), rgba(82, 94, 122), rgba(114, 128, 154), rgba(156, 170, 190));
const Ramp kRoofBrown = ramp5(rgba(54, 34, 38), rgba(88, 56, 46), rgba(124, 82, 58), rgba(158, 112, 74), rgba(192, 148, 100));
const Ramp kRoofGrey = ramp5(rgba(44, 38, 50), rgba(72, 64, 70), rgba(104, 94, 92), rgba(138, 128, 118), rgba(174, 164, 148));
const Ramp kRoofPurple = ramp5(rgba(36, 26, 60), rgba(58, 40, 94), rgba(86, 60, 132), rgba(122, 92, 170), rgba(170, 140, 206));
const Ramp kBarnRed = ramp5(rgba(70, 24, 36), rgba(112, 36, 40), rgba(156, 52, 46), rgba(190, 82, 62), rgba(220, 126, 96));
const Ramp kClay = ramp5(rgba(92, 38, 36), rgba(150, 62, 42), rgba(196, 98, 56), rgba(224, 140, 80), rgba(244, 186, 120));
const Ramp kCopper = ramp5(rgba(30, 70, 72), rgba(52, 112, 104), rgba(82, 152, 130), rgba(126, 190, 158), rgba(184, 226, 192));
const Ramp kAdobe = ramp5(rgba(122, 82, 64), rgba(172, 122, 86), rgba(212, 166, 114), rgba(232, 198, 144), rgba(246, 226, 182));
const Ramp kTurf = ramp5(rgba(32, 58, 50), rgba(52, 92, 56), rgba(82, 128, 58), rgba(122, 162, 68), rgba(172, 198, 96));
const Ramp kSoil = ramp5(rgba(44, 28, 34), rgba(72, 46, 40), rgba(102, 70, 50), rgba(134, 98, 66), rgba(166, 128, 88));
const Ramp kLog = ramp5(rgba(52, 32, 36), rgba(86, 56, 44), rgba(124, 84, 58), rgba(160, 118, 78), rgba(196, 158, 108));
const Ramp kGlass = ramp5(rgba(24, 26, 48), rgba(38, 48, 78), rgba(58, 78, 112), rgba(108, 140, 164), rgba(220, 236, 240));
const Ramp kThatchOld = ramp5(rgba(74, 56, 46), rgba(116, 90, 62), rgba(156, 128, 86), rgba(188, 162, 110), rgba(214, 196, 150));

enum class Surf : uint8_t { None, Roof, Flat, Parapet, CapRim, CapMouth };

// ---------------------------------------------------------------- facade canvas: u right, v up from the ground
struct Facade {
  int w = 0, h = 0;
  std::vector<uint32_t> px;
  std::vector<int> top;   // highest v of the wall at each u (gables rise)
  Facade() = default;
  Facade(int w_, int h_) : w(w_), h(h_), px((size_t)w_ * h_, 0), top((size_t)w_, h_ - 1) {}
  void set(int u, int v, uint32_t c) { if (u >= 0 && v >= 0 && u < w && v < h && v <= top[(size_t)u]) px[(size_t)v * w + u] = c; }
  uint32_t get(int u, int v) const { return (u >= 0 && v >= 0 && u < w && v < h) ? px[(size_t)v * w + u] : 0; }
  bool in(int u, int v) const { return u >= 0 && v >= 0 && u < w && v < h && v <= top[(size_t)u]; }
};

enum class WallKind : uint8_t { Timber, Plaster, Stone, Brick, Log, Adobe, Planks };

struct Mass {
  bool round = false;
  float x0 = 0, x1 = 0, y0 = 0, y1 = 0;   // box footprint, px (y1 = front wall)
  float cx = 0, cy = 0, r = 0;             // round footprint
  int zBase = 0, wallH = 24;
  RoofShape shape = RoofShape::Hip;
  bool alongY = false;                     // ridge runs north-south: the gable faces the street
  bool crenel = false;                     // flat roofs: merlons on the parapet
  bool chimney = false, smoke = false;     // a chimney stack (flat cap with a dark mouth)
  bool gambrel = false;                    // barn roof: steep lower slope, shallow upper
  float slope = 0.8f, roofH = 0, hk = 1.0f;
  int ov = 3, ovF = 2;                     // eave overhang at the sides/back and at the front
  Ramp rR, wR, tR;                         // roof, wall, trim ramps
  RoofMat rmat = RoofMat::Shingle;
  WallKind wall = WallKind::Timber;
  Facade f;
  bool snow = false;
  int moss = 0;
  uint32_t seed = 0;
  float zTop() const { return (float)(zBase + wallH); }
  float frontY(float x) const {
    if (!round) return (x >= x0 && x < x1) ? y1 - 0.5f : -1e9f;
    float dx = x - cx;
    return std::fabs(dx) < r ? cy + std::sqrt(r * r - dx * dx) - 0.5f : -1e9f;
  }
};

// ---------------------------------------------------------------- roof height field
// Height of mass m's top at ground point (x, y), or -1 where it has none. surf/facet say what is there.
float massTop(const Mass& m, float x, float y, Surf& surf) {
  const float zt = m.zTop();
  surf = Surf::Roof;
  if (m.chimney) {
    if (x < m.x0 || x >= m.x1 || y < m.y0 || y >= m.y1) return -1;
    bool rim = x < m.x0 + 1 || x >= m.x1 - 1 || y < m.y0 + 1 || y >= m.y1 - 1;
    surf = rim ? Surf::CapRim : Surf::CapMouth;
    return rim ? zt + 1 : zt;
  }
  if (m.round) {
    float rr = std::hypot(x - m.cx, y - m.cy);
    if (m.shape == RoofShape::Conical || m.shape == RoofShape::Steep || m.shape == RoofShape::Pagoda) {
      float R = m.r + m.ov;
      if (rr > R) return -1;
      float t = 1 - rr / R;
      if (m.shape == RoofShape::Pagoda) t = t * t * 0.55f + t * 0.45f;   // a swept, concave cone
      return zt - 1 + m.roofH * t;
    }
    if (m.shape == RoofShape::Dome) {
      float R = m.r + 1;
      if (rr > R) return -1;
      float t = rr / R;
      return zt + m.roofH * std::sqrt(std::max(0.0f, 1 - t * t));
    }
    if (rr > m.r) return -1;   // flat top behind a crenellated rim
    if (rr > m.r - 2.2f) {
      surf = Surf::Parapet;
      float a = std::atan2(y - m.cy, x - m.cx);
      int seg = (int)std::floor((a + PI) / TAU * std::max(8.0f, m.r * 1.2f));
      return zt + ((seg & 1) || !m.crenel ? 2 : 5);
    }
    surf = Surf::Flat;
    return zt;
  }
  if (m.shape == RoofShape::FlatParapet || m.shape == RoofShape::Dome) {
    if (x < m.x0 || x >= m.x1 || y < m.y0 || y >= m.y1) return -1;
    if (m.shape == RoofShape::Dome) {   // a dome on a drum in the middle of a flat roof
      float R = std::min(m.x1 - m.x0, m.y1 - m.y0) * 0.5f - 7;
      float dcx = (m.x0 + m.x1) * 0.5f, dcy = (m.y0 + m.y1) * 0.5f - 1;
      float rr = std::hypot(x - dcx, (y - dcy) * 1.0f);
      if (rr < R) { float t = rr / R; return zt + 3 + m.roofH * std::sqrt(std::max(0.0f, 1 - t * t)); }
      if (rr < R + 1.5f) { surf = Surf::Parapet; return zt + 3; }   // the drum's rim
    }
    float e = std::min(std::min(x - m.x0, m.x1 - 1 - x), std::min(y - m.y0, m.y1 - 1 - y));
    if (e < 2) {
      surf = Surf::Parapet;
      if (m.crenel) {
        float along = (y - m.y0 < 2 || m.y1 - 1 - y < 2) ? x : y;
        return zt + ((((int)along % 8) + 8) % 8 < 4 ? 6 : 3);
      }
      return zt + 3;
    }
    surf = Surf::Flat;
    return zt;
  }
  const float rx0 = m.x0 - m.ov, rx1 = m.x1 + m.ov, ry0 = m.y0 - m.ov, ry1 = m.y1 + m.ovF;
  if (x < rx0 || x >= rx1 || y < ry0 || y >= ry1) return -1;
  const float ze = zt - 1;
  float dy = std::min(y - ry0, ry1 - y), dx = std::min(x - rx0, rx1 - x);
  float d, dmax;
  switch (m.shape) {
    case RoofShape::Hip: case RoofShape::Pagoda:
      d = std::min(dy, dx * m.hk);
      if (m.alongY) d = std::min(dx, dy * m.hk);
      dmax = m.alongY ? (rx1 - rx0) * 0.5f : (ry1 - ry0) * 0.5f;
      break;
    default:   // gable, steep, turf: two planes
      d = m.alongY ? dx : dy;
      dmax = m.alongY ? (rx1 - rx0) * 0.5f : (ry1 - ry0) * 0.5f;
      break;
  }
  float t = std::clamp(d / std::max(1.0f, dmax), 0.0f, 1.0f);
  float prof = t;
  if (m.shape == RoofShape::Turf) prof = 1 - (1 - t) * (1 - t) * 0.85f - 0.15f * (1 - t);   // rounded sod hump
  if (m.shape == RoofShape::Pagoda) {
    prof = t * t * 0.55f + t * 0.45f;
    float cxd = std::min(x - rx0, rx1 - x), cyd = std::min(y - ry0, ry1 - y);
    float c = std::max(0.0f, 5 - std::max(cxd, cyd));   // corners sweep up
    return ze + m.roofH * prof + c * 0.9f;
  }
  if (m.gambrel) prof = t < 0.45f ? t * 1.55f : 0.70f + (t - 0.45f) * 0.55f;
  return ze + m.roofH * prof;
}

struct Scene {
  std::vector<Mass> ms;
  int W = 0, D = 0, top = 0;   // footprint size, rows above the footprint's back edge
  float topAt(float x, float y, int& mi, Surf& s) const {
    float best = -1;
    mi = -1;
    s = Surf::None;
    for (int i = 0; i < (int)ms.size(); i++) {
      Surf ss;
      float z = massTop(ms[i], x, y, ss);
      if (z > best) { best = z; mi = i; s = ss; }
    }
    return best;
  }
};

// ---------------------------------------------------------------- roof material texels
// u runs along the courses, v up the slope from the eave (both in px); returns a ramp offset
int roofTexel(RoofMat mat, int u, int v, uint32_t seed, bool& special) {
  special = false;
  switch (mat) {
    case RoofMat::Thatch: {
      // soft courses of straw: a ragged lit butt edge, broken shade under the next course, fine straw strokes
      int wave = (int)std::lround(std::sin(u * 0.37f + (float)(seed % 7)) * 1.2f + std::sin(u * 1.7f + 1.3f) * 0.5f);
      // (integration: kept mostly at the plane's own value so hip and gable planes read as separate lit volumes;
      // the old +-1 speckle on every row drowned the plane lighting and made thatch roofs look flat)
      int vv = v + wave + 64;
      int p = vv % 5, row = vv / 5;
      uint32_t h = hash3(u, row, seed);
      if (p == 4) return (h % 4 == 0) ? 0 : -1;   // shade under the next course's butt edge
      if (p == 0) return (h % 3 == 0) ? 1 : 0;    // a few lit straw ends on the butt edge
      if (hash3(u, vv, seed + 5) % 11 == 0) return -1;   // sparse dark straw strokes
      return 0;
    }
    case RoofMat::Shingle: {
      // courses of wooden shingles: the butt of each course is lit, the course below sits in its shadow
      int row = (v + 63) / 4, p = (v + 63) % 4;
      int off = (row & 1) * 3 + (int)(hash3(row, 7, seed) % 2);
      int w = 6, col = (u + off + 64) / w, q = (u + off + 64) % w;
      uint32_t h = hash3(col, row, seed);
      if (p == 3) return -1;
      if (q == 0 && p < 2) return -1;
      if (p == 0) return (h % 5 == 0) ? 0 : 1;
      return (h % 7 == 0) ? -1 : ((h % 11 == 1) ? 1 : 0);
    }
    case RoofMat::Slate: {
      int row = (v + 63) / 3, p = (v + 63) % 3;
      int off = (row & 1) * 2;
      int col = (u + off + 64) / 4, q = (u + off + 64) % 4;
      uint32_t h = hash3(col, row, seed);
      if (p == 2) return -1;
      if (q == 0 && p == 1) return -1;
      if (p == 0) return h % 4 == 0 ? 0 : 1;
      return (h % 9 == 0) ? 1 : 0;
    }
    case RoofMat::ClayTile: {
      int q = ((u % 4) + 4) % 4, p = ((v + 64) % 5);
      int k = q == 0 ? 1 : (q == 3 ? -1 : 0);   // barrel tiles: lit crest, shaded trough
      if (p == 0) k = q == 3 ? -1 : 1;           // the lip of each course
      if (p == 1 && q != 0) k = -1;
      return k;
    }
    case RoofMat::Turf: {
      uint32_t h = hash3(u, v, seed);
      int k = (h % 5 == 0) ? 1 : ((h % 7 == 0) ? -1 : 0);
      if (hash3(u / 2, v / 2, seed + 3) % 3 == 0) k += (hash3(u / 3, v / 3, seed + 9) & 1) ? 1 : -1;
      if (hash3(u, v, seed + 77) % 97 == 0) special = true;   // a flower
      return std::clamp(k, -1, 1);
    }
    case RoofMat::Copper: {
      int q = ((u % 6) + 6) % 6;
      int k = q == 0 ? 1 : (q == 1 ? -1 : 0);   // standing seams
      if (hash3(u / 2, v / 3, seed) % 9 == 0) k += 1;   // patina blooms
      return std::clamp(k, -1, 1);
    }
    default: {   // adobe: smooth, faintly trowelled
      uint32_t h = hash3(u / 2, v / 2, seed);
      return h % 9 == 0 ? -1 : (h % 13 == 0 ? 1 : 0);
    }
  }
}

// ---------------------------------------------------------------- facade materials
// paints the whole facade (u, v) in the wall material; quoins and plinth included
void paintWallMat(Mass& m, int plinth) {
  Facade& F = m.f;
  const Ramp& R = m.wR;
  const int w = F.w;
  for (int u = 0; u < w; u++)
    for (int v = 0; v <= F.top[(size_t)u] && v < F.h; v++) {
      int k = 2;
      uint32_t col = 0;
      switch (m.wall) {
        case WallKind::Timber: case WallKind::Plaster: {
          uint32_t h = hash3(u / 2, v / 2, m.seed);
          k = 2;
          if (h % 11 == 0) k = 1;
          if (h % 13 == 1) k = 3;
          if (v > F.top[(size_t)u] - 2) k = 3;
          col = R[k];
          break;
        }
        case WallKind::Stone: {
          int row = v / 4, hh = v % 4;
          int off = (int)(hash3(row, 3, m.seed) % 6);
          int bw = 5 + (int)(hash3(row, 5, m.seed) % 3);
          int bx = (u + off), q = bx % bw;
          int brick = bx / bw;
          uint32_t h = hash3(brick, row, m.seed + 1);
          k = 2;
          if (h % 4 == 0) k = 3;
          if (h % 5 == 1) k = 1;
          if (hh == 3 && k >= 2) k = 3;           // lit top of each course
          if (hh == 0 || q == 0) k = 0;           // mortar joints
          if (hh == 0 && q != 0) k = 1;
          col = R[k];
          break;
        }
        case WallKind::Brick: {
          int row = v / 3, hh = v % 3;
          int bx = u + (row & 1) * 3, q = bx % 6;
          uint32_t h = hash3(bx / 6, row, m.seed);
          k = 2;
          if (h % 5 == 0) k = 3;
          if (h % 6 == 1) k = 1;
          if (hh == 0 || q == 0) col = kStoneWarm[1];   // pale mortar
          else col = R[hh == 2 && k == 2 ? 3 : k];
          break;
        }
        case WallKind::Log: {
          int p = v % 4;
          k = p == 3 ? 3 : (p == 0 ? 0 : 2);
          if (p == 1) k = 1;
          if (k == 2 && hash3(u / 3, v / 4, m.seed) % 5 == 0) k = 1;   // knots and checks
          col = R[k];
          break;
        }
        case WallKind::Adobe: {
          float n = vnoise(u / 5.0f, v / 4.0f, m.seed) + (bayer(u, v) - 0.5f) * 0.18f;
          k = n < 0.20f ? 1 : 2;
          if (hash3(u, v, m.seed) % 61 == 0) k = 1;   // pits in the render
          if (v >= F.top[(size_t)u] - 1) k = 3;     // rounded, sun-caught top
          if (v < 3) k = 1;                         // splash zone
          col = R[k];
          break;
        }
        case WallKind::Planks: {
          int q = u % 4;
          k = q == 0 ? 1 : (q == 3 ? 1 : 2);
          if (q == 1) k = 3;
          if (hash3(u / 4, v / 6, m.seed) % 6 == 0 && k == 2) k = 1;
          col = R[k];
          break;
        }
      }
      F.set(u, v, col);
    }
  // corner quoins / posts: the lit west corner and the shaded east corner give the box its turn
  for (int v = 0; v < F.h; v++) {
    int tl = F.top[0], tr = F.top[(size_t)(w - 1)];
    if (m.wall == WallKind::Stone || m.wall == WallKind::Plaster || m.wall == WallKind::Brick) {
      int blk = (v / 4) & 1;
      const Ramp& Q = m.wall == WallKind::Plaster ? kStoneWarm : m.tR;
      if (v <= tl) for (int i = 0; i < (blk ? 3 : 5) && i < w; i++) F.set(i, v, Q[(v % 4 == 0) ? 1 : (i == 0 ? 4 : 3)]);
      if (v <= tr) for (int i = 0; i < (blk ? 5 : 3) && i < w; i++) F.set(w - 1 - i, v, Q[(v % 4 == 0) ? 0 : (i == 0 ? 1 : 2)]);
    } else if (m.wall == WallKind::Timber) {
      if (v <= tl) { F.set(0, v, kBeam[3]); F.set(1, v, kBeam[2]); }
      if (v <= tr) { F.set(w - 2, v, kBeam[1]); F.set(w - 1, v, kBeam[0]); }
    } else if (m.wall == WallKind::Log) {
      // log ends stacked at the corners
      int p = v % 4;
      if (v <= tl) { F.set(0, v, kLog[p == 0 ? 1 : (p == 3 ? 4 : 3)]); F.set(1, v, kLog[p == 0 ? 0 : (p == 2 ? 2 : 3)]); }
      if (v <= tr) { F.set(w - 2, v, kLog[p == 0 ? 0 : 2]); F.set(w - 1, v, kLog[p == 0 ? 0 : 1]); }
    } else if (m.wall == WallKind::Adobe) {
      if (v <= tl) F.set(0, v, R[3]);
      if (v <= tr) F.set(w - 1, v, R[1]);
    }
  }
  // timber frame: sill beam, posts, braces, head beam
  if (m.wall == WallKind::Timber) {
    int wh = m.wallH;
    auto beamH = [&](int v) { for (int u = 0; u < w; u++) { if (v <= F.top[(size_t)u]) F.set(u, v, kBeam[2]); if (v - 1 >= 0) F.set(u, v - 1, kBeam[1]); } };
    beamH(plinth + 1);
    beamH(wh - 1);
    if (wh > 30) beamH(wh / 2 + 2);   // the floor beam of a second storey
    for (int u = 15; u < w - 6; u += 16) {
      for (int v = plinth; v < wh; v++) { F.set(u, v, kBeam[3]); F.set(u + 1, v, kBeam[1]); }
    }
    // gable: vertical boards above the head beam
    for (int u = 0; u < w; u++)
      for (int v = wh; v <= F.top[(size_t)u]; v++) {
        int q = u % 3;
        F.set(u, v, kWood[q == 0 ? 1 : (q == 1 ? 3 : 2)]);
      }
  }
  if (m.wall == WallKind::Planks) {   // barn: white trim boards at the corners and the eave line
    for (int v = 0; v < F.h; v++) {
      if (v <= F.top[0]) { F.set(0, v, kCloth[4]); F.set(1, v, kCloth[3]); }
      if (v <= F.top[(size_t)(w - 1)]) { F.set(w - 2, v, kCloth[2]); F.set(w - 1, v, kCloth[1]); }
    }
  }
  // the plinth: a stone footing below wooden and plastered walls
  if (m.wall == WallKind::Timber || m.wall == WallKind::Plaster || m.wall == WallKind::Log || m.wall == WallKind::Planks) {
    for (int u = 0; u < w; u++)
      for (int v = 0; v < plinth; v++) {
        int bx = u + (v & 1) * 3;
        int k = (bx % 6 == 0) ? 0 : (v == plinth - 1 ? 3 : (hash3(bx / 6, v, m.seed) % 3 == 0 ? 1 : 2));
        F.set(u, v, kStone[k]);
      }
  }
  // weathering: damp stains rising from the footing, moss on old walls
  if (m.moss >= 2)
    for (int u = 0; u < w; u++) {
      int hgt = (int)(hash3(u / 3, 1, m.seed + 5) % 4) + (m.moss - 1);
      if (hash3(u / 5, 2, m.seed + 6) % 3 != 0) continue;
      for (int v = plinth; v < plinth + hgt; v++) {
        uint32_t c = F.get(u, v);
        if (c) F.set(u, v, darken(c, 0.22f + 0.05f * (plinth + hgt - v)));
      }
    }
}

// a window on a facade: lintel, frame, panes with a glint, sill with its shadow, shutters and a flower box
void facadeWindow(Mass& m, int u0, int v0, int ww, int wh, uint32_t seed, bool shutters, bool flowers, bool arched,
                  uint32_t shutterCol) {
  Facade& F = m.f;
  const Ramp& T = m.tR;
  // recess shadow + frame
  for (int j = -1; j <= wh; j++)
    for (int i = -1; i <= ww; i++) {
      int u = u0 + i, v = v0 + j;
      bool frame = i == -1 || i == ww || j == -1 || j == wh;
      if (arched && j == wh && (i <= 0 || i >= ww - 1)) continue;
      if (arched && j == wh - 1 && (i == -1 || i == ww)) continue;
      if (frame) { F.set(u, v, T[(i == -1 || j == wh) ? 3 : 1]); continue; }
      int k = 2;
      if (j >= wh - 2 && i <= 1) k = 3;            // sky reflected in the upper panes
      if (i == ww - 1 || j == wh - 1) k = 1;       // inner shadow of the reveal (top, right)
      if (j == 0) k = 1;
      uint32_t c = kGlass[k];
      if (j < wh / 2 && (i + j) % 3 == 0 && k <= 2) c = mix(kGlass[2], kGlow[1], 0.35f);   // warm room beyond
      F.set(u, v, c);
    }
  // mullion and transom
  for (int j = 0; j < wh; j++) F.set(u0 + ww / 2, v0 + j, T[2]);
  for (int i = 0; i < ww; i++) F.set(u0 + i, v0 + wh / 2, T[2]);
  F.set(u0 + 1, v0 + wh - 2, kGlass[4]);   // glint
  F.set(u0 + 2, v0 + wh - 3, kGlass[3]);
  // lintel above, sill below (lit top, dark underside)
  if (!arched) for (int i = -2; i <= ww + 1; i++) { F.set(u0 + i, v0 + wh + 1, T[3]); }
  for (int i = -2; i <= ww + 1; i++) { F.set(u0 + i, v0 - 1, T[4]); F.set(u0 + i, v0 - 2, T[1]); }
  for (int i = -1; i <= ww; i++) { uint32_t c = F.get(u0 + i, v0 - 3); if (c) F.set(u0 + i, v0 - 3, darken(c, 0.35f)); }
  if (m.snow) for (int i = -2; i <= ww + 1; i++) F.set(u0 + i, v0 - 1, kSnow[4]);
  if (shutters) {
    Ramp S = ramp(shutterCol);
    for (int j = 0; j < wh; j++) {
      int a = (j % 3 == 0) ? 1 : 3, b = (j % 3 == 0) ? 0 : 2;
      F.set(u0 - 3, v0 + j, S[a]); F.set(u0 - 2, v0 + j, S[b]);
      F.set(u0 + ww + 1, v0 + j, S[b]); F.set(u0 + ww + 2, v0 + j, S[(j % 3 == 0) ? 0 : 1]);
    }
  }
  if (flowers) {
    for (int i = -1; i <= ww; i++) { F.set(u0 + i, v0 - 2, kWood[2]); F.set(u0 + i, v0 - 3, kWood[0]); }
    for (int i = -1; i <= ww; i++) {
      uint32_t h = hash3(i, 1, seed);
      uint32_t fc = h % 3 == 0 ? rgba(232, 72, 84) : (h % 3 == 1 ? rgba(250, 206, 86) : rgba(240, 240, 250));
      F.set(u0 + i, v0 - 1, (i & 1) ? kLeaf[2] : fc);
      if (h % 2) F.set(u0 + i, v0, kLeaf[3]);
    }
  }
}

// a recessed plank door: deep reveal, lintel or arch, planks, straps, a handle
void facadeDoor(Mass& m, int uc, int dw, int dh, bool arched, const Ramp& wood) {
  Facade& F = m.f;
  const Ramp& T = m.tR;
  int u0 = uc - dw / 2;
  auto inArch = [&](int i, int j) {   // j counts down from the top of the door
    if (!arched) return true;
    float cx = (dw - 1) * 0.5f, rr = dw * 0.5f;
    if (j >= (int)rr) return true;
    float dy = rr - j - 0.5f, dx = i - cx;
    return dx * dx + dy * dy <= rr * rr;
  };
  for (int j = 0; j < dh; j++)
    for (int i = -2; i <= dw + 1; i++) {
      int v = dh - 1 - j;
      bool inside = i >= 0 && i < dw && inArch(i, j);
      bool frame = !inside && i >= -2 && i <= dw + 1 && (inArch(std::clamp(i, 0, dw - 1), std::max(0, j - 1)) || j >= dw / 2);
      if (inside) {
        int q = i % 3;
        int k = q == 0 ? 1 : 2;
        if (i == 0 || j == 0 || (arched && !inArch(i, j - 1))) k = 0;   // deep reveal shadow (top, left)
        if (i == dw - 1) k = 1;
        uint32_t c = wood[k];
        if ((j == 3 || j == dh - 4) && i > 0) c = kIron[1];         // straps
        F.set(u0 + i, v, c);
      } else if (frame && (i == -1 || i == dw || i == -2 || i == dw + 1)) {
        F.set(u0 + i, v, T[(i < 0) ? (i == -2 ? 2 : 3) : (i == dw + 1 ? 0 : 1)]);
      }
    }
  // lintel / arch ring
  if (!arched) for (int i = -2; i <= dw + 1; i++) { F.set(u0 + i, dh, T[3]); F.set(u0 + i, dh + 1, T[2]); }
  else
    for (int i = -2; i <= dw + 1; i++)
      for (int j = 0; j < 2; j++) {
        float cx = (dw - 1) * 0.5f, rr = dw * 0.5f + 1.5f;
        float dx = i - cx;
        float yy = std::sqrt(std::max(0.0f, rr * rr - dx * dx));
        int v = dh - (int)(dw * 0.5f) + (int)yy + j;
        F.set(u0 + i, v, T[j == 0 ? 3 : 4]);
      }
  F.set(u0 + dw - 3, dh / 2, kGold[4]);
  F.set(u0 + dw - 3, dh / 2 - 1, kGold[1]);
}

// ---------------------------------------------------------------- the plan: masses from type, size, style, seed
struct Plan {
  Scene sc;
  Building b = Building::House;
  ArchStyle st;
  uint32_t seed = 0;
  int W = 0, D = 0;
  int doorX = 0;             // door centre, px
  int zBase = 0;
  int doorMass = 0;
  bool stilts = false;
};

float slopeFor(int pitch) {
  static const float s[5] = {0.45f, 0.62f, 0.80f, 1.05f, 1.40f};
  return s[std::clamp(pitch, 0, 4)];
}

Ramp roofRampFor(RoofMat m, uint32_t tint, uint32_t seed, int weather) {
  Ramp r;
  switch (m) {
    case RoofMat::Thatch: r = weather >= 2 ? kThatchOld : kThatch; break;
    case RoofMat::Shingle: {
      static const Ramp* opts[4] = {&kRoofBrown, &kRoofRed, &kRoofGrey, &kRoofBrown};
      r = tint ? ramp(tint) : *opts[seed % 4];
      break;
    }
    case RoofMat::Slate: r = tint ? ramp(mix(tint, rgba(84, 94, 122), 0.55f)) : kRoofSlate; break;
    case RoofMat::ClayTile: r = tint ? ramp(mix(tint, rgba(196, 98, 56), 0.6f)) : kClay; break;
    case RoofMat::Turf: r = kTurf; break;
    case RoofMat::Adobe: r = kAdobe; break;
    case RoofMat::Copper: r = kCopper; break;
    default: r = kRoofBrown; break;
  }
  return r;
}

WallKind wallKindFor(WallMat w) {
  switch (w) {
    case WallMat::Timber: return WallKind::Timber;
    case WallMat::Plaster: return WallKind::Plaster;
    case WallMat::Stone: return WallKind::Stone;
    case WallMat::Brick: return WallKind::Brick;
    case WallMat::Log: return WallKind::Log;
    case WallMat::Adobe: return WallKind::Adobe;
    default: return WallKind::Timber;
  }
}

Ramp wallRampFor(WallKind w, uint32_t tint, uint32_t seed) {
  switch (w) {
    case WallKind::Timber: { static const Ramp* o[4] = {&kPlaster, &kPlaster, &kPlasterOchre, &kPlasterRose}; return tint ? ramp(tint) : *o[seed % 4]; }
    case WallKind::Plaster: { static const Ramp* o[4] = {&kPlaster, &kPlasterOchre, &kPlasterRose, &kPlasterBlue}; return tint ? ramp(tint) : *o[seed % 4]; }
    case WallKind::Stone: return (seed & 1) ? kStoneWarm : kStone;
    case WallKind::Brick: return kBrick;
    case WallKind::Log: return kLog;
    case WallKind::Adobe: return tint ? ramp(tint, 0.8f) : kAdobe;
    case WallKind::Planks: return kBarnRed;
  }
  return kPlaster;
}

Mass baseMass(const Plan& p, float x0, float x1, float y0, float y1, int wallH) {
  Mass m;
  m.x0 = x0; m.x1 = x1; m.y0 = y0; m.y1 = y1;
  m.zBase = p.zBase;
  m.wallH = wallH;
  m.shape = p.st.roof;
  m.rmat = p.st.roofMat;
  m.slope = slopeFor(p.st.pitch);
  if (m.shape == RoofShape::Steep) m.slope = std::max(m.slope, slopeFor(4));
  if (m.shape == RoofShape::Turf) m.slope = std::min(m.slope, slopeFor(1));
  m.wall = wallKindFor(p.st.wall);
  m.rR = roofRampFor(p.st.roofMat, p.st.roofTint, p.seed, p.st.weather);
  m.wR = wallRampFor(m.wall, p.st.wallTint, p.seed >> 3);
  m.tR = (m.wall == WallKind::Stone || m.wall == WallKind::Adobe) ? kStoneWarm : kBeam;
  if (m.wall == WallKind::Plaster || m.wall == WallKind::Brick) m.tR = kStoneWarm;
  if (m.wall == WallKind::Log) m.tR = kWoodDark;
  m.snow = p.st.snow;
  m.moss = p.st.weather;
  m.seed = p.seed;
  if (m.shape == RoofShape::Turf) m.ov = 2;
  if (m.shape == RoofShape::Pagoda) { m.ov = 5; m.ovF = 4; }
  if (m.shape == RoofShape::FlatParapet || m.shape == RoofShape::Dome) { m.ov = 0; m.ovF = 0; }
  if (m.rmat == RoofMat::Thatch) { m.ov = 4; m.ovF = 3; }   // thick thatch hangs low
  return m;
}

void finishRoofHeight(Mass& m) {
  if (m.round) {
    if (m.shape == RoofShape::Dome) m.roofH = m.r * 0.75f;
    else if (m.shape == RoofShape::Conical || m.shape == RoofShape::Steep || m.shape == RoofShape::Pagoda) m.roofH = (m.r + m.ov) * std::max(1.3f, m.slope * 1.4f);
    return;
  }
  float span = m.alongY ? (m.x1 - m.x0 + 2 * m.ov) : (m.y1 - m.y0 + m.ov + m.ovF);
  if (m.shape == RoofShape::FlatParapet) m.roofH = 0;
  else if (m.shape == RoofShape::Dome) m.roofH = (std::min(m.x1 - m.x0, m.y1 - m.y0) * 0.5f - 7) * 0.85f;
  else m.roofH = span * 0.5f * m.slope;
}

Plan makePlan(Building b, int wT, int hT, const ArchStyle& st0, uint32_t seed) {
  Plan p;
  p.b = b;
  p.seed = seed;
  p.st = st0;
  p.W = std::max(2, wT) * 16;
  p.D = std::max(1, hT) * 16;
  p.doorX = (std::max(2, wT) / 2) * 16 + 8;
  ArchStyle& st = p.st;
  const int W = p.W, D = p.D;
  // the building type only adds function; a few types also pick sturdier materials
  switch (b) {
    case Building::StoneHouse:
      if (st.wall == WallMat::Timber || st.wall == WallMat::Log || st.wall == WallMat::Plaster) st.wall = (seed & 8) ? WallMat::Brick : WallMat::Stone;
      break;
    case Building::Temple: case Building::Keep: case Building::Tower:
      if (st.wall != WallMat::Adobe) st.wall = WallMat::Stone;
      if (st.roofMat == RoofMat::Thatch || st.roofMat == RoofMat::Turf) st.roofMat = RoofMat::Slate;
      break;
    case Building::Smithy:
      if (st.wall == WallMat::Timber || st.wall == WallMat::Plaster) st.wall = WallMat::Stone;
      break;
    case Building::Hut:
      if (st.wall == WallMat::Stone || st.wall == WallMat::Brick || st.wall == WallMat::Plaster) st.wall = WallMat::Log;
      if (st.roofMat == RoofMat::Slate || st.roofMat == RoofMat::ClayTile || st.roofMat == RoofMat::Copper) st.roofMat = RoofMat::Thatch;
      if (st.roof == RoofShape::Dome || st.roof == RoofShape::Pagoda) st.roof = RoofShape::Gable;
      break;
    default: break;
  }
  if (st.roofMat == RoofMat::Adobe && st.roof != RoofShape::Dome) st.roof = RoofShape::FlatParapet;
  p.stilts = st.stilts && b != Building::Keep && b != Building::Temple && b != Building::Tower;
  p.zBase = p.stilts ? 7 : 0;
  auto H = [&](uint32_t k, int n) { return (int)(hash3((int)k, 17, seed) % (uint32_t)n); };
  int wallH = 27;
  switch (b) {
    case Building::Inn: wallH = 40; break;
    case Building::Hut: wallH = 20; break;
    case Building::Farmhouse: wallH = 28; break;
    case Building::Temple: wallH = 30; break;
    case Building::Shop: wallH = 28; break;
    default: break;
  }
  if (st.roof == RoofShape::FlatParapet || st.roof == RoofShape::Dome) wallH += 4;   // the parapet rises above the roof deck
  std::vector<Mass>& ms = p.sc.ms;

  if (b == Building::Keep) {
    Mass m = baseMass(p, 0, (float)W, 0, (float)D, 36);
    m.shape = RoofShape::FlatParapet;
    m.crenel = true;
    m.wall = WallKind::Stone;
    m.wR = kStone; m.tR = kStoneWarm;
    ms.push_back(m);
    // the donjon: a taller block at the back with its own roof in the local style
    float dw = std::min((float)W - 40, 64.0f), dd = std::min((float)D - 20, 34.0f);
    Mass dj = baseMass(p, W * 0.5f - dw * 0.5f, W * 0.5f + dw * 0.5f, 4, 4 + dd, 54);
    dj.wall = WallKind::Stone; dj.wR = kStone; dj.tR = kStoneWarm;
    if (dj.shape == RoofShape::FlatParapet) dj.crenel = true;
    else { dj.shape = RoofShape::Hip; dj.slope = std::max(dj.slope, 1.0f); }
    ms.push_back(dj);
    // corner turrets
    for (int k = 0; k < 4; k++) {
      Mass t = baseMass(p, 0, 0, 0, 0, k < 2 ? 46 : 50);
      t.round = true;
      t.r = 10;
      t.cx = (k & 1) ? W - 10.0f : 10.0f;
      t.cy = k < 2 ? D - 10.0f : 10.0f;
      t.wall = WallKind::Stone; t.wR = kStone; t.tR = kStoneWarm;
      bool cone = st.roof != RoofShape::FlatParapet && st.roof != RoofShape::Dome;
      t.shape = cone ? RoofShape::Conical : RoofShape::FlatParapet;
      t.crenel = !cone;
      t.ov = 2;
      t.slope = 1.2f;
      ms.push_back(t);
    }
    p.doorMass = 0;
  } else if (b == Building::Tower) {
    float r = std::min(W, D) * 0.5f - 1;
    if (W - 2 * r > 20) {   // low annexes either side
      Mass a = baseMass(p, 0, (float)W, D - 26.0f, (float)D, 22);
      a.shape = (st.roof == RoofShape::FlatParapet || st.roof == RoofShape::Dome) ? RoofShape::FlatParapet : RoofShape::Hip;
      ms.push_back(a);
    }
    Mass t = baseMass(p, 0, 0, 0, 0, 62 + H(1, 10));
    t.round = true;
    t.r = std::min(r, 20.0f);
    t.cx = W * 0.5f;
    t.cy = D - t.r - 1;
    t.shape = (st.roof == RoofShape::FlatParapet || st.roof == RoofShape::Dome) ? RoofShape::Dome : (st.roof == RoofShape::Pagoda ? RoofShape::Pagoda : RoofShape::Conical);
    if (t.shape == RoofShape::Dome && st.roofMat == RoofMat::Adobe) t.rmat = RoofMat::Copper, t.rR = kCopper;
    else if (st.roofMat == RoofMat::Slate || st.roofMat == RoofMat::Shingle) t.rR = kRoofPurple, t.rmat = RoofMat::Slate;
    t.ov = 3;
    t.slope = 1.5f;
    ms.push_back(t);
    p.doorMass = (int)ms.size() - 1;
  } else if (b == Building::Temple) {
    bool desert = st.roof == RoofShape::FlatParapet || st.roof == RoofShape::Dome;
    if (desert) {
      Mass h = baseMass(p, 0, (float)W, 0, (float)D, 32);
      h.shape = RoofShape::Dome;
      h.rmat = RoofMat::Copper; h.rR = kCopper;
      ms.push_back(h);
      p.doorMass = 0;
    } else {
      // a nave with a bell tower over the door: side-gabled on wide temples so the steeple stands clear
      Mass n = baseMass(p, 4, (float)W - 4, 0, (float)D - 6, 28);
      n.shape = RoofShape::Gable;
      n.alongY = W < 80;
      n.slope = n.alongY ? 0.72f : 0.9f;
      n.wall = WallKind::Stone; n.wR = kStoneWarm; n.tR = kStone;
      ms.push_back(n);
      float tw = 22;
      Mass t = baseMass(p, p.doorX - tw * 0.5f, p.doorX + tw * 0.5f, D - 24.0f, (float)D, 58);
      t.shape = RoofShape::Hip;
      t.hk = 1.0f;
      t.slope = 2.3f;   // a tall spire
      t.ov = 2; t.ovF = 2;
      t.wall = WallKind::Stone; t.wR = kStoneWarm; t.tR = kStone;
      ms.push_back(t);
      p.doorMass = 1;
    }
  } else {
    // houses, inns, shops, smithies, barns, huts: a main block plus seeded wings, cross gables, dormers
    float mx0 = 0, mx1 = (float)W;
    int wing = 0;   // -1 west, +1 east
    bool pitched = st.roof != RoofShape::FlatParapet && st.roof != RoofShape::Dome;
    if (b == Building::Smithy) wing = 1;
    else if ((b == Building::House || b == Building::StoneHouse || b == Building::Farmhouse || b == Building::Inn) && W >= 64 && H(2, 5) < 2)
      wing = H(3, 2) ? 1 : -1;
    int ww = b == Building::Smithy ? std::min(28, W / 2 - 4) : (W >= 80 ? 24 : 18);
    if (wing == 1 && W - ww < p.doorX + 9) wing = 0;
    if (wing == -1 && ww > p.doorX - 9) wing = 0;
    if (wing == 1) mx1 = (float)(W - ww);
    if (wing == -1) mx0 = (float)ww;
    Mass m = baseMass(p, mx0, mx1, 0, (float)D, wallH);
    if (st.roof == RoofShape::Conical && b != Building::Farmhouse) {   // a round house under a cone (rondavel)
      m.round = true;
      m.r = std::min(mx1 - mx0, (float)D + 6) * 0.5f - 1;
      m.cx = (mx0 + mx1) * 0.5f;
      m.cy = D - m.r - 0.5f;
      m.slope = std::max(m.slope, 0.9f);
      if (std::fabs(m.cx - p.doorX) > m.r - 7) m.cx = (float)p.doorX;
    }
    if (b == Building::Farmhouse) {
      m.wall = WallKind::Planks; m.wR = kBarnRed; m.tR = kCloth;
      if (pitched) { m.shape = RoofShape::Gable; m.alongY = true; m.gambrel = true; m.slope = std::max(m.slope, 1.0f); }
    }
    // narrow-and-deep or seeded: turn the ridge so the gable faces the street
    if (pitched && b != Building::Farmhouse && m.shape != RoofShape::Turf && !m.round && (mx1 - mx0) <= D + 24 && H(4, 3) != 0) m.alongY = true;
    if (b == Building::Hut && pitched) m.alongY = (mx1 - mx0) < 52;
    ms.push_back(m);
    p.doorMass = 0;
    if (wing) {
      float wx0 = wing == 1 ? mx1 : 0, wx1 = wing == 1 ? (float)W : mx0;
      int wh = b == Building::Smithy ? wallH - 2 : wallH - 6 - (b == Building::Inn ? 10 : 0);
      Mass wm = baseMass(p, wx0, wx1, b == Building::Smithy ? 0.0f : 6.0f, (float)D, wh);
      wm.alongY = false;
      if (wm.shape == RoofShape::Gable || wm.shape == RoofShape::Steep) wm.shape = RoofShape::Hip;
      if (b == Building::Smithy) { wm.shape = pitched ? RoofShape::Gable : RoofShape::FlatParapet; wm.wall = WallKind::Timber; wm.wR = kBeam; wm.tR = kBeam; }
      ms.push_back(wm);
    }
    // a cross gable on wide houses with pitched roofs: a front-facing gable over one bay
    if (pitched && !ms[0].alongY && (b == Building::House || b == Building::StoneHouse || b == Building::Inn) && (mx1 - mx0) >= 80 &&
        H(5, 2) == 0 && ms[0].shape != RoofShape::Turf) {
      float cw = 26;
      bool left = H(6, 2) == 0;
      float cx0 = left ? mx0 + 6 : mx1 - 6 - cw;
      if (std::fabs(cx0 + cw * 0.5f - p.doorX) < 6) cx0 = left ? cx0 + 16 : cx0 - 16;
      Mass cg = baseMass(p, cx0, cx0 + cw, 2, (float)D, wallH);
      cg.alongY = true;
      cg.shape = RoofShape::Gable;
      ms.push_back(cg);
    }
  }
  // a flat-roofed house often carries a little roof room (the stair head) at the back
  if (!ms.empty() && ms[0].shape == RoofShape::FlatParapet && !ms[0].round && b != Building::Keep && b != Building::Temple &&
      b != Building::Tower && ms[0].x1 - ms[0].x0 >= 40 && H(8, 5) < 3) {
    const Mass base = ms[0];
    float rw = 18 + (float)H(9, 3) * 3;
    bool left = H(10, 2) == 0;
    float rx0 = left ? base.x0 + 3 : base.x1 - 3 - rw;
    Mass rr = baseMass(p, rx0, rx0 + rw, base.y0 + 3, base.y0 + 3 + std::min(16.0f, (base.y1 - base.y0) * 0.4f), 11);
    rr.zBase = base.zBase + base.wallH;
    rr.shape = RoofShape::FlatParapet;
    ms.push_back(rr);
  }
  // roof heights
  for (Mass& m : ms) finishRoofHeight(m);
  // the cross gable's ridge stops at the main ridge
  if (ms.size() >= 2 && ms.back().alongY && !ms[0].alongY && !ms[0].round && b != Building::Temple && b != Building::Keep) {
    Mass& cg = ms.back();
    float mainRidge = ms[0].zTop() - 1 + ms[0].roofH;
    float cgRidge = cg.zTop() - 1 + cg.roofH;
    if (cgRidge > mainRidge - 1) { cg.roofH = mainRidge - 1 - (cg.zTop() - 1); }
  }
  // chimneys: on the main block, by the ridge, placed by seed; the smithy's forge stack over the wing junction
  {
    int n = st.chimneys;
    if (b == Building::Smithy) n = 1;
    if (b == Building::Keep || b == Building::Tower || b == Building::Temple || b == Building::Farmhouse) n = 0;
    if (b == Building::Hut) n = (st.smoke || (seed & 4)) ? 1 : 0;
    if (b == Building::Inn) n = std::max(n, 1);
    if (st.roof == RoofShape::FlatParapet || st.roof == RoofShape::Dome) n = std::min(n, 1);
    const Mass base = ms[0];
    for (int i = 0; i < n; i++) {
      float cw = b == Building::Smithy ? 8.0f : 6.0f, cd = 4.0f;
      float cx;
      if (b == Building::Smithy && ms.size() >= 2) cx = ms[1].x0 + 2;
      else if (base.alongY) {   // by the ridge, on the lit or the shaded side
        float mid = (base.x0 + base.x1) * 0.5f;
        cx = (hash3(i, 31, seed) & 1) ? mid + 2 : mid - 2 - cw;
        if (n == 2) cx = i == 0 ? mid - 2 - cw : mid + 2;
      } else {
        float span = base.x1 - base.x0;
        float t = (n == 2) ? (i == 0 ? 0.16f : 0.80f) : 0.18f + (hash3(i, 31, seed) % 64) / 100.0f;
        cx = base.x0 + span * t;
        if (std::fabs(cx + cw * 0.5f - p.doorX) < 5) cx += 10;
      }
      bool flatTop = base.shape == RoofShape::FlatParapet || base.shape == RoofShape::Dome;
      if (flatTop) cx = (hash3(i, 34, seed) & 1) ? base.x0 + 4 : base.x1 - cw - 4;   // in a corner of the deck
      cx = std::clamp(cx, base.x0 + 2, base.x1 - cw - 2);
      float cy = base.alongY ? base.y0 + (base.y1 - base.y0) * 0.30f + (hash3(i, 33, seed) % 6) : (base.y0 + base.y1) * 0.5f + 1 + (hash3(i, 32, seed) % 3);
      if (b == Building::Smithy) cy = 6;
      if (flatTop) cy = base.y0 + 3;
      Surf s;
      int mi;
      float zr = p.sc.topAt(cx + cw * 0.5f, cy + cd, mi, s);
      float ridge = 0;
      for (int k = 0; k <= 8; k++) { int mj; Surf s2; ridge = std::max(ridge, p.sc.topAt(cx + cw * 0.5f, cy - 6 + k * 2.0f, mj, s2)); }
      Mass c;
      c.chimney = true;
      c.x0 = cx; c.x1 = cx + cw; c.y0 = cy; c.y1 = cy + cd;
      c.zBase = (int)std::floor(std::max(0.0f, zr - 2));
      c.wallH = (int)std::ceil(ridge + (b == Building::Smithy ? 9 : 6) - c.zBase);
      c.wall = (b == Building::Smithy || st.wall == WallMat::Stone) ? WallKind::Stone : (st.wall == WallMat::Adobe ? WallKind::Adobe : WallKind::Brick);
      c.wR = c.wall == WallKind::Stone ? kStone : (c.wall == WallKind::Adobe ? kAdobe : kBrick);
      c.tR = kStone;
      c.smoke = st.smoke || b == Building::Smithy || (b == Building::Inn && i == 0);
      c.snow = st.snow;
      c.seed = seed + 99u + (uint32_t)i;
      c.shape = RoofShape::FlatParapet;
      ms.push_back(c);
    }
  }
  // dormers: small front gables on the front plane of deep, pitched roofs
  if (!ms[0].alongY && !ms[0].round && (ms[0].shape == RoofShape::Gable || ms[0].shape == RoofShape::Hip || ms[0].shape == RoofShape::Steep) &&
      D >= 48 && (b == Building::House || b == Building::StoneHouse || b == Building::Inn || b == Building::Shop)) {
    int nd = (int)(hash3(7, 7, seed) % 3);
    if (b == Building::Inn) nd = std::max(nd, 1);
    const Mass base = ms[0];
    for (int i = 0; i < nd; i++) {
      float dw = 13;
      float span = base.x1 - base.x0 - 24;
      if (span < dw) break;
      float t = nd == 1 ? 0.5f + ((int)(hash3(i, 9, seed) % 30) - 15) / 100.0f : (i == 0 ? 0.22f : 0.78f);
      float dx0 = base.x0 + 12 + span * t - dw * 0.5f;
      bool clash = false;
      for (const Mass& o : ms) if (o.chimney && dx0 < o.x1 + 2 && dx0 + dw > o.x0 - 2) clash = true;
      if (clash) continue;
      float dfy = base.y1 - 6;   // front of the dormer, a little behind the eave
      Surf s;
      int mi;
      float zr = p.sc.topAt(dx0 + dw * 0.5f, dfy + 1, mi, s);
      Mass d = baseMass(p, dx0, dx0 + dw, dfy - 10, dfy, 11);
      d.zBase = (int)std::floor(zr - 3);
      d.wallH = 11;
      d.alongY = true;
      d.shape = (base.shape == RoofShape::Hip) ? RoofShape::Hip : RoofShape::Gable;
      d.ov = 1; d.ovF = 1;
      d.slope = std::max(0.9f, base.slope);
      if (d.wall == WallKind::Stone || d.wall == WallKind::Brick || d.wall == WallKind::Log) { d.wall = WallKind::Plaster; d.wR = kPlaster; }
      finishRoofHeight(d);
      // keep the dormer's ridge under the main ridge
      float mainRidge = base.zTop() - 1 + base.roofH;
      if (d.zTop() - 1 + d.roofH > mainRidge - 2) d.roofH = std::max(2.0f, mainRidge - 2 - (d.zTop() - 1));
      ms.push_back(d);
    }
  }
  // how far up the picture goes
  float hmax = 0;
  for (const Mass& m : ms) {
    float t = m.zTop() + m.roofH + 6;
    if (m.chimney) t = m.zTop() + 3;
    hmax = std::max(hmax, t);
  }
  p.sc.W = W;
  p.sc.D = D;
  p.sc.top = (int)std::ceil(hmax) + 4;
  return p;
}

// ---------------------------------------------------------------- facades
void buildFacades(Plan& p) {
  const ArchStyle& st = p.st;
  const Building b = p.b;
  const int D = p.D;
  static const uint32_t shutterCols[5] = {rgba(64, 104, 146), rgba(72, 116, 74), rgba(150, 60, 52), rgba(96, 74, 120), rgba(70, 62, 56)};
  uint32_t shutterCol = shutterCols[hash3(1, 2, p.seed) % 5];
  for (int mi = 0; mi < (int)p.sc.ms.size(); mi++) {
    Mass& m = p.sc.ms[mi];
    int w = m.round ? (int)std::ceil(m.r * 2) : (int)std::lround(m.x1 - m.x0);
    if (w <= 0) continue;
    // facade height: walls, and for front-facing gables the triangle up to the roof
    int hTop = m.wallH - 1;
    std::vector<int> tops((size_t)w, hTop);
    if (!m.round && m.alongY && !m.chimney && m.shape != RoofShape::FlatParapet) {
      for (int u = 0; u < w; u++) {
        Surf s;
        float z = massTop(m, m.x0 + u + 0.5f, m.y1 - 0.5f, s);
        tops[(size_t)u] = std::max(hTop, (int)std::floor(z - m.zBase) - 1);
      }
    }
    if ((m.shape == RoofShape::FlatParapet || m.shape == RoofShape::Dome) && !m.chimney) {
      for (int u = 0; u < w; u++) {
        if (m.round) { tops[(size_t)u] = m.wallH + 1; continue; }
        int along = (int)std::floor(m.x0 + u);
        bool merlon = m.crenel && (((along % 8) + 8) % 8) < 4;
        tops[(size_t)u] = m.wallH - 1 + (merlon ? 6 : 3);
      }
    }
    int fh = 1;
    for (int t : tops) fh = std::max(fh, t + 1);
    m.f = Facade(w, fh + 1);
    m.f.top = tops;
    if (m.chimney) {
      for (int u = 0; u < w; u++)
        for (int v = 0; v <= tops[(size_t)u]; v++) {
          int k;
          if (m.wall == WallKind::Stone) k = ((v % 3 == 0) || ((u + (v / 3) * 3) % 5 == 0)) ? 1 : 2;
          else k = ((v % 3 == 0) || ((u + ((v / 3) & 1) * 2) % 4 == 0)) ? 1 : 2;
          if (u == 0) k = std::min(4, k + 1);
          if (u == w - 1) k = std::max(0, k - 1);
          uint32_t c = m.wR[k];
          if (v >= tops[(size_t)u] - 2) c = darken(c, 0.35f);   // soot
          if (v == tops[(size_t)u]) c = m.tR[3];                // cap band
          m.f.set(u, v, c);
        }
      continue;
    }
    const int plinth = m.zBase > 0 ? 2 : 3;
    paintWallMat(m, plinth);
    if (m.round && (b == Building::Tower || b == Building::Keep || mi != p.doorMass)) continue;   // towers and turrets: details are overlays
    // dormers: one small window
    if (m.wallH <= 11) {
      if (m.shape == RoofShape::FlatParapet) {   // roof room: a dark doorway onto the roof
        int u0 = w / 2 - 3;
        for (int v = 0; v < 8; v++)
          for (int i = 0; i < 6; i++) {
            if (v == 7 && (i == 0 || i == 5)) continue;
            m.f.set(u0 + i, v, (i == 0 || v == 7) ? rgba(20, 14, 26) : rgba(40, 28, 36));
          }
        for (int i = -1; i <= 6; i++) m.f.set(u0 + i, 8, m.wR[3]);
        continue;
      }
      facadeWindow(m, w / 2 - 3, 3, 6, 6, p.seed + mi, false, false, false, shutterCol);
      continue;
    }
    bool front = m.y1 >= D - 0.5f;
    // gable vent / attic window
    if (m.alongY && fh > m.wallH + 8) facadeWindow(m, w / 2 - 2, m.wallH + 2, 5, 5, p.seed + 3, false, false, b == Building::Temple, shutterCol);
    // the door
    int du = p.doorX - (int)std::lround(m.x0);
    bool hasDoor = front && mi == p.doorMass && du >= 4 && du < w - 4;
    bool arched = m.wall == WallKind::Stone || m.wall == WallKind::Adobe || b == Building::Temple || b == Building::Keep;
    if (hasDoor) {
      if (b == Building::Farmhouse) {
        // big barn doors with X braces
        int u0 = du - 8, dh = 19;
        for (int v = 0; v < dh; v++)
          for (int i = 0; i < 16; i++) {
            int k = (i % 4 == 0) ? 1 : 2;
            uint32_t c = kBarnRed[k];
            if (i == 0 || i == 15 || i == 7 || i == 8 || v == dh - 1 || v == 0 || v == dh / 2) c = kCloth[(i == 0 || v == dh - 1) ? 4 : 2];
            int a = (i < 8 ? i : i - 8), diag1 = a * (dh - 1) / 7, diag2 = (7 - a) * (dh - 1) / 7;
            if (std::abs(v - diag1) <= 0 || std::abs(v - diag2) <= 0) c = kCloth[3];
            m.f.set(u0 + i, v, c);
          }
        for (int i = -1; i <= 16; i++) m.f.set(u0 + i, dh, kCloth[1]);
        // hay loft door in the gable
        if (fh > m.wallH + 10)
          for (int v = m.wallH + 1; v < m.wallH + 9; v++)
            for (int i = 0; i < 9; i++) {
              uint32_t c = (i == 0 || i == 8 || v == m.wallH + 8) ? kCloth[3] : kWoodDark[1];
              if (v < m.wallH + 3 && i > 0 && i < 8) c = kThatch[3 + ((i + v) & 1)];
              m.f.set(du - 4 + i, v, c);
            }
      } else {
        int dw = b == Building::Hut ? 8 : (b == Building::Keep || b == Building::Temple ? 12 : 10);
        int dh = b == Building::Hut ? 13 : (b == Building::Keep ? 22 : (b == Building::Temple ? 20 : 16));
        facadeDoor(m, du, dw, dh, arched, b == Building::Temple ? kWood : kWoodDark);
        if (b == Building::Temple) {   // rose window over the door
          int rv = dh + 8, ru = du;
          for (int j = -5; j <= 5; j++)
            for (int i = -5; i <= 5; i++) {
              float d = std::hypot((float)i, (float)j);
              if (d > 5.2f) continue;
              uint32_t c = d > 4.2f ? m.tR[3] : (d < 1.2f ? kGold[4] : ((int)std::floor(std::atan2((float)j, (float)i) * 1.9f + 6) % 2 ? rgba(84, 132, 230) : rgba(222, 74, 84)));
              if (d <= 4.2f && d > 3.4f) c = m.tR[1];
              m.f.set(ru + i, rv + j, c);
            }
        }
      }
    }
    // windows on every free bay
    if (b == Building::Keep && mi == 0) {
      for (int u = 18; u < w - 18; u += 16) {
        if (std::abs(u - du) < 14) continue;
        for (int v = 14; v < 22; v++) { m.f.set(u, v, kInk); m.f.set(u + 1, v, m.wR[0]); }
      }
      continue;
    }
    if (b == Building::Smithy && mi == 1) {
      // the open forge: dark bay, glowing hearth, anvil, bellows
      int u0 = 2, u1 = w - 3, vh = m.wallH - 4;
      for (int v = 0; v < vh; v++)
        for (int u = u0; u <= u1; u++) m.f.set(u, v, v > vh - 3 ? kBeam[1] : rgba(34, 24, 32));
      for (int u = u0; u <= u1; u++) m.f.set(u, vh, kBeam[3]);
      for (int u = u0 + 1; u <= u0 + 8; u++)
        for (int v = 0; v < 7; v++) m.f.set(u, v, kStone[u == u0 + 1 ? 3 : (v == 6 ? 3 : 1)]);
      for (int u = u0 + 2; u <= u0 + 7; u++) { m.f.set(u, 7, kFire[(u & 1) ? 3 : 4]); m.f.set(u, 8, kFire[2]); }
      for (int v = 9; v < vh - 2; v++)
        for (int u = u0 + 2; u <= u0 + 7; u++) m.f.set(u, v, mix(rgba(34, 24, 32), kFire[1], std::max(0.0f, 0.45f - (v - 9) * 0.06f)));
      int au = u1 - 7;
      for (int u = au; u <= au + 5; u++) m.f.set(u, 6, kIron[3]);
      for (int u = au + 1; u <= au + 4; u++) m.f.set(u, 5, kIron[2]);
      for (int v = 0; v < 5; v++) { m.f.set(au + 2, v, kIron[1]); m.f.set(au + 3, v, kIron[0]); }
      continue;
    }
    int winV = 8 + (m.zBase > 0 ? 0 : 1), winW = 6, winH = 8;
    if (b == Building::Hut) { winV = 6; winW = 5; winH = 6; }
    if (m.wall == WallKind::Adobe) { winW = 5; winH = 7; }
    int storeys = m.wallH >= 34 ? 2 : 1;
    bool donjon = b == Building::Keep && mi == 1;
    for (int s = 0; s < storeys; s++) {
      int v0 = s == 0 ? winV : m.wallH / 2 + 7;
      if (donjon) { if (s == 0) continue; v0 = m.wallH - 13; }
      for (int u = 8; u < w - 6; u += 16) {
        int uc = u + ((w % 16) / 2);
        if (uc < 6 || uc > w - 7) continue;
        if (hasDoor && s == 0 && std::abs(uc - du) < 11) continue;
        if (b == Building::Shop && s == 0 && std::abs(uc - du) < 20) {   // a wide shop window beside the door
          facadeWindow(m, uc - 5, v0 - 1, 10, 7, p.seed + u, false, false, false, shutterCol);
          continue;
        }
        bool sh = st.shutters && hash3(u, s, p.seed) % 3 != 0 && m.wall != WallKind::Adobe;
        bool fl = !st.snow && m.wall != WallKind::Adobe && hash3(u, s + 5, p.seed) % 4 == 0;
        bool ar = (m.wall == WallKind::Stone && b != Building::Smithy) || b == Building::Temple;
        int wh = b == Building::Temple ? 12 : winH;
        facadeWindow(m, uc - winW / 2, v0 - (b == Building::Temple ? 2 : 0), winW, wh, p.seed + u * 7 + s, sh, fl, ar, shutterCol);
        if (b == Building::Temple)
          for (int j = 1; j < wh - 1; j++)
            for (int i = 0; i < winW; i++) {
              if (i == winW / 2) continue;
              static const uint32_t sg[4] = {rgba(222, 74, 84), rgba(84, 132, 230), rgba(250, 204, 86), rgba(92, 192, 122)};
              m.f.set(uc - winW / 2 + i, v0 - 2 + j, sg[(i + j / 3) % 4]);
            }
      }
    }
    // adobe: wooden beam ends (vigas) poking out under the roofline, and a rain spout
    if (m.wall == WallKind::Adobe) {
      for (int u = 5; u < w - 3; u += 9) { m.f.set(u, m.wallH - 6, kWood[3]); m.f.set(u + 1, m.wallH - 6, kWood[1]); m.f.set(u, m.wallH - 7, kWood[0]); m.f.set(u + 1, m.wallH - 7, kWood[0]); }
    }
  }
}

// ---------------------------------------------------------------- render
struct Painter {
  Plan& p;
  Canvas c;
  std::vector<uint8_t> layer;   // 0 empty, 1 wall, 2 roof, 3 overlay
  int ox = BLDG_PAD_X;
  explicit Painter(Plan& pl) : p(pl) {
    c = Canvas(p.W + 2 * BLDG_PAD_X, p.sc.top + p.D + BLDG_PAD_B);
    layer.assign((size_t)c.w * c.h, 0);
  }
  int rowOf(float y, float z) const { return (int)std::floor(p.sc.top + y - z); }
  void put(int x, int y, uint32_t col, uint8_t l) {
    if (x < 0 || y < 0 || x >= c.w || y >= c.h || !chA(col)) return;
    c.set(x, y, col);
    layer[(size_t)y * c.w + x] = l;
  }
};

// is a roof point shaded by something taller standing up-left of it (chimneys, dormers, towers, higher wings)?
bool roofShadowed(const Scene& sc, float x, float y, float z, int self) {
  for (int k = 2; k <= 10; k += 2) {
    float sx = x - k * 0.75f, sy = y - k * 0.45f, need = z + k * 0.9f;
    for (int i = 0; i < (int)sc.ms.size(); i++) {
      if (i == self) continue;
      const Mass& m = sc.ms[i];
      Surf s;
      float zz = massTop(m, sx, sy, s);
      if (zz > need) return true;
    }
  }
  return false;
}

uint32_t roofColor(const Plan& p, const Mass& m, int mi, float x, float y, float z, Surf s, bool shade) {
  const Ramp& R = m.rR;
  if (m.chimney) {
    if (s == Surf::CapMouth) return kInk;
    return m.wR[(x - m.x0 < 1.5f || y - m.y0 < 1.0f) ? 4 : 3];
  }
  if (s == Surf::Parapet) {
    const Ramp& W = m.wR;
    return W[4];
  }
  if (s == Surf::Flat) {
    // a roof deck: packed earth/plaster for adobe, flagstones for stone
    int k = 2;
    // the parapet (2 px thick, 3+ px tall) casts its shadow down-right onto the deck: a band inside the west and
    // north rims (integration: without it flat roofs read as a flat slab with a painted border)
    bool pshadow = false;
    if (!m.round && (m.shape == RoofShape::FlatParapet || m.shape == RoofShape::Dome)) {
      pshadow = x - m.x0 < 5.0f || y - m.y0 < 4.0f;
    } else if (m.round && !m.chimney) {
      float dx = x - m.cx, dy = y - m.cy, rr = std::hypot(dx, dy);
      pshadow = rr > m.r - 5.0f && dx * -0.72f + dy * -0.45f > rr * 0.35f;
    }
    if (m.wall == WallKind::Adobe || m.rmat == RoofMat::Adobe) {
      int ix = (int)std::floor(x), iy = (int)std::floor(y);
      float n = vnoise(x / 7.0f, y / 5.0f, m.seed + 3) + (bayer(ix, iy) - 0.5f) * 0.16f;
      // the deck faces the sky, so it sits a step above the sunlit-from-the-side front wall (else the box reads flat)
      k = n < 0.16f ? 2 : (n > 0.84f ? 4 : 3);
      if (hash3(ix, iy, m.seed) % 97 == 0) k = 2;
      const Ramp& A = m.wall == WallKind::Adobe ? m.wR : kAdobe;
      if (shade) k = std::max(0, k - 1);
      if (pshadow) return mix(A[std::max(0, k - 2)], A[0], 0.25f);
      return A[k];
    }
    int gx = (int)std::floor(x), gy = (int)std::floor(y);
    k = (gy % 5 == 0 || (gx + (gy / 5) * 3) % 7 == 0) ? 1 : 2;
    if (shade) k = std::max(0, k - 1);
    if (pshadow) return mix(kStone[k], kStone[0], 0.4f);
    return kStone[k + 1 > 4 ? 4 : k + 1];
  }
  // pitched surfaces: the normal from the height field, lit from the top-left
  Surf s2;
  float h = 0.5f;
  float zx0 = massTop(m, x - h, y, s2), zx1 = massTop(m, x + h, y, s2);
  float zy0 = massTop(m, x, y - h, s2), zy1 = massTop(m, x, y + h, s2);
  zx0 = zx0 < 0 ? z : zx0;
  zx1 = zx1 < 0 ? z : zx1;
  zy0 = zy0 < 0 ? z : zy0;
  zy1 = zy1 < 0 ? z : zy1;
  float gx = (zx1 - zx0) / (2 * h), gy = (zy1 - zy0) / (2 * h);
  float nx = -gx, ny = -gy, nz = 1;
  float L = std::sqrt(nx * nx + ny * ny + nz * nz);
  nx /= L; ny /= L;
  float nzz = std::sqrt(std::max(0.0f, 1 - nx * nx - ny * ny));
  float l = nx * -0.72f + ny * -0.36f + nzz * 0.60f;
  int k;
  if (l > 0.97f) k = 4;
  else if (l > 0.56f) k = 3;
  else if (l > 0.16f) k = 2;
  else if (l > -0.16f) k = 1;
  else k = 0;
  // courses: v up the slope from the nearest eave, u along it
  bool facesY = std::fabs(gy) >= std::fabs(gx);
  int u = (int)std::floor(facesY ? x : y);
  int v = (int)std::floor(z - (m.zTop() - 1));
  if (m.round) { u = (int)std::floor(std::atan2(y - m.cy, x - m.cx) * m.r); v = (int)std::floor(z); }
  bool special = false;
  int t = roofTexel(m.rmat, u, v, m.seed, special);
  // keep the strongest highlight for the ridge itself
  if (k == 4 && t > 0) t = 0;
  k = std::clamp(k + t, 0, 4);
  if (special && m.rmat == RoofMat::Turf) { uint32_t fh = hash3(u, v, 5) % 3; return fh == 0 ? rgba(240, 214, 96) : (fh == 1 ? rgba(224, 228, 244) : rgba(196, 132, 210)); }
  // ridge cap: a lit capping along the ridge; thatch gets a woven ridge band
  float zt = m.zTop() - 1 + m.roofH;
  if (!m.round && m.shape != RoofShape::Turf && m.shape != RoofShape::Pagoda) {
    if (m.rmat == RoofMat::Thatch && z > zt - 2.6f) k = ((u + (int)(z * 2)) % 4 < 2) ? 1 : 3;
    else if (z > zt - 1.0f) k = 4;
    else if (z > zt - 1.6f && ny > 0.2f) k = std::min(k, 2);
  }
  // front planes: a soft fall-off toward the eave gives the slope its curve
  if (!m.round && ny > 0.2f && m.roofH > 4) {
    float tt = (z - (m.zTop() - 1)) / m.roofH;
    if (tt < 0.16f) k = std::max(0, k - 1);
  }
  // barge boards on the open gable ends of side-gabled roofs: lit on the west, shaded on the east
  if (!m.round && !m.alongY && (m.shape == RoofShape::Gable || m.shape == RoofShape::Steep) && m.rmat != RoofMat::Thatch) {
    float ex0 = x - (m.x0 - m.ov), ex1 = (m.x1 + m.ov) - x;
    if (ex0 < 1.0f) return m.rmat == RoofMat::Turf ? kWood[3] : m.tR[std::min(4, 3 + (m.wall == WallKind::Timber ? 1 : 0))];
    if (ex0 < 2.0f) return m.rmat == RoofMat::Turf ? kWood[2] : R[std::min(4, k + 1)];
    if (ex1 < 1.0f) return m.rmat == RoofMat::Turf ? kWood[0] : R[0];
    if (ex1 < 2.0f) return R[std::max(0, k - 1)];
  }
  if (!m.round && !m.alongY && m.rmat == RoofMat::Thatch && (m.shape == RoofShape::Gable || m.shape == RoofShape::Steep)) {
    float ex0 = x - (m.x0 - m.ov), ex1 = (m.x1 + m.ov) - x;
    if (ex0 < 1.5f) k = std::min(4, k + 1);
    if (ex1 < 2.0f) k = std::max(0, k - 1);
  }
  // moss on the north (back) plane and the damp lower courses of old roofs
  if (m.moss >= 2 && (m.rmat == RoofMat::Thatch || m.rmat == RoofMat::Shingle || m.rmat == RoofMat::Slate)) {
    bool north = ny < -0.2f;
    if ((north && hash3(u / 3, v / 2, m.seed + 41) % 4 == 0) || (v < 3 && hash3(u / 4, 0, m.seed + 43) % 5 == 0))
      return mix(R[k], kMoss[std::clamp(k, 1, 3)], 0.55f);
  }
  if (m.snow && m.rmat != RoofMat::Turf && v > 1 + (int)(hash3(u / 3, 0, m.seed + 7) % 3)) {
    // snow lies on the roof, thin at the eaves and on steep planes facing the sun
    int ks = std::clamp(k + 1, 1, 4);
    if (t < 0 && hash3(u, v, m.seed) % 3 == 0) ks = std::max(1, ks - 1);
    return kSnow[ks];
  }
  // the lowest courses sit in the gloom under the next roof edge; the eave line darkens a little
  if (!m.round && v <= 1 && k > 1) k--;
  if (shade) k = std::max(0, k - 1);
  (void)p; (void)mi;
  return R[k];
}

void renderBuilding(Painter& P) {
  Plan& p = P.p;
  const Scene& sc = p.sc;
  // y range covered by the roofs and walls
  float yMin = -8, yMax = (float)p.D + 6;
  const float step = 0.25f;
  for (int x = -BLDG_PAD_X; x < p.W + BLDG_PAD_X; x++) {
    const float fx = x + 0.5f;
    // wall events in this column, by depth
    struct Ev { float y; int mi; };
    std::vector<Ev> evs;
    for (int mi = 0; mi < (int)sc.ms.size(); mi++) {
      float fy = sc.ms[mi].frontY(fx);
      if (fy > -1e8f) evs.push_back({fy, mi});
    }
    std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) { return a.y < b.y; });
    size_t ei = 0;
    int prevRow = INT32_MAX, prevMi = -1;
    Surf prevS = Surf::None;
    uint32_t prevCol = 0;
    float prevZ = 0;
    auto fascia = [&](int fromRow, int mi, Surf s) {
      // the eave board under a roof edge: two pixels, dark, with a lighter lower lip on lit roofs
      if (mi < 0) return;
      const Mass& m = sc.ms[mi];
      if (m.chimney || s == Surf::Parapet || s == Surf::Flat) return;
      const Ramp& R = m.rR;
      int th = m.rmat == RoofMat::Thatch ? 3 : (m.rmat == RoofMat::Turf ? 3 : 2);
      for (int k = 1; k <= th; k++) {
        uint32_t col = k == th ? R[0] : R[1];
        if (m.rmat == RoofMat::Turf) col = k == th ? kSoil[0] : kSoil[2];
        if (m.rmat == RoofMat::Thatch && k < th) col = R[1 + ((x + k) & 1)];
        if (m.snow && k == th && hash3(x, 3, m.seed) % 5 == 0) col = kSnow[3];   // icicles
        P.put(P.ox + x, fromRow + k, col, 2);
      }
    };
    for (float y = yMin; y <= yMax; y += step) {
      while (ei < evs.size() && evs[ei].y <= y) {
        const Ev& e = evs[ei++];
        const Mass& m = sc.ms[e.mi];
        if (prevMi >= 0) { fascia(prevRow, prevMi, prevS); prevRow = INT32_MAX; prevMi = -1; }
        int u = m.round ? (int)std::floor(fx - (m.cx - m.r)) : (int)std::floor(fx - m.x0);
        if (u < 0 || u >= m.f.w) continue;
        for (int v = 0; v <= m.f.top[(size_t)u] && v < m.f.h; v++) {
          uint32_t col = m.f.get(u, v);
          if (!chA(col)) continue;
          if (m.round && !m.chimney) {
            // cylinder: light falls off across the curve
            float t = (fx - m.cx) / m.r;
            float l = lightAt(std::clamp(t, -0.96f, 0.96f) * 0.95f, 0.18f);
            if (l > 0.55f) col = lighten(col, 0.18f);
            else if (l < 0.0f) col = darken(col, 0.38f);
            else if (l < 0.25f) col = darken(col, 0.16f);
          }
          P.put(P.ox + x, P.rowOf(e.y, (float)m.zBase + v), col, m.chimney ? 2 : 1);
        }
      }
      int mi;
      Surf s;
      float z = sc.topAt(fx, y, mi, s);
      if (mi < 0 || z < 0) {
        if (prevMi >= 0) fascia(prevRow, prevMi, prevS);
        prevRow = INT32_MAX; prevMi = -1; prevS = Surf::None;
        continue;
      }
      const Mass& m = sc.ms[mi];
      int row = P.rowOf(y, z);
      bool flatTop = s == Surf::Flat || s == Surf::Parapet;
      bool shade = (m.chimney ? false : roofShadowed(sc, fx, y, z, flatTop ? -1 : mi));
      uint32_t col = roofColor(p, m, mi, fx, y, z, s, shade);
      if (prevMi == mi && prevRow != INT32_MAX && row > prevRow + 1) {
        bool solid = s != Surf::Roof || prevS != Surf::Roof || prevZ > z + 2.5f;
        for (int r = prevRow + 1; r < row; r++) {
          uint32_t fc = col;
          if (solid) fc = (prevS == Surf::Parapet || prevS == Surf::CapRim) ? m.wR[std::max(0, (m.wall == WallKind::Adobe ? 2 : 1))] : darken(prevCol, 0.3f);
          P.put(P.ox + x, r, fc, 2);
        }
      } else if (prevMi >= 0 && prevMi != mi && row > prevRow + 1) {
        fascia(prevRow, prevMi, prevS);
      }
      P.put(P.ox + x, row, col, 2);
      prevRow = row; prevMi = mi; prevS = s; prevCol = col; prevZ = z;
    }
    if (prevMi >= 0) fascia(prevRow, prevMi, prevS);
  }
  // eave shadow: the wall just under a roof edge is in the roof's shade (offset right with the light)
  Canvas src = P.c;
  for (int y = 0; y < P.c.h; y++)
    for (int x = 0; x < P.c.w; x++) {
      if (P.layer[(size_t)y * P.c.w + x] != 1) continue;
      int d = 0;
      for (int k = 1; k <= 3 && !d; k++) {
        int ax = x - (k > 1 ? 1 : 0);
        if (y - k >= 0 && ax >= 0 && P.layer[(size_t)(y - k) * P.c.w + ax] == 2) d = k;
      }
      if (d) P.c.set(x, y, darken(src.get(x, y), d == 1 ? 0.55f : (d == 2 ? 0.40f : 0.2f)));
    }
}

// details that stand proud of the walls, painted last: stilts, steps, signs, awnings, banners, lanterns
void overlays(Painter& P, BuildingInfo* info) {
  Plan& p = P.p;
  const Building b = p.b;
  const int ox = P.ox;
  const int D = p.D;
  auto at = [&](float x, float y, float z) { return std::pair<int, int>(ox + (int)std::floor(x), P.rowOf(y, z)); };
  // stilts and the shade beneath a raised floor
  if (p.stilts) {
    for (int x = 0; x < p.W; x++)
      for (int z = 0; z < p.zBase; z++) {
        auto q = at(x + 0.5f, D - 0.5f, (float)z);
        if (!chA(P.c.get(q.first, q.second))) P.put(q.first, q.second, rgba(30, 26, 40, 150), 3);
      }
    for (int x = 1; x < p.W - 1; x += 13) {
      for (int z = -2; z < p.zBase; z++) {
        auto q = at(x + 0.5f, D - 0.5f, (float)z);
        P.put(q.first, q.second, kLog[3], 3);
        P.put(q.first + 1, q.second, kLog[1], 3);
      }
    }
    // a ladder down from the door
    for (int z = -2; z < p.zBase; z++) {
      auto q = at(p.doorX - 3.5f, D + 1.5f, (float)z);
      P.put(q.first, q.second, kWood[3], 3);
      P.put(q.first + 6, q.second, kWood[1], 3);
      if ((z & 1) == 0) for (int i = 1; i < 6; i++) P.put(q.first + i, q.second, kWood[2], 3);
    }
  } else if (b != Building::Farmhouse) {
    // a stone step before the door
    for (int i = -6; i <= 5; i++) {
      auto q = at(p.doorX + i + 0.5f, D + 0.5f, 0);
      P.put(q.first, q.second, kStone[i == -6 ? 4 : 3], 3);
      P.put(q.first, q.second + 1, kStone[1], 3);
    }
  }
  const float frontZ0 = (float)p.zBase;
  // signs
  auto sign = [&](float x, float z, int icon) {
    auto q = at(x, D + 1.5f, z);
    int sx = q.first, sy = q.second;
    for (int i = 0; i <= 10; i++) P.put(sx + i, sy, kIron[2], 3);
    P.put(sx, sy + 1, kIron[1], 3);
    P.put(sx + 2, sy + 1, kIron[3], 3); P.put(sx + 9, sy + 1, kIron[3], 3);
    for (int j = 0; j < 8; j++)
      for (int i = 0; i < 11; i++) {
        int k = (i == 0 || j == 0) ? 4 : ((i == 10 || j == 7) ? 1 : 3);
        P.put(sx + 1 + i, sy + 2 + j, kWood[k], 3);
      }
    int ix = sx + 5, iy = sy + 4;
    switch (icon) {
      case 0:   // mug of ale
        for (int j = 0; j < 4; j++) for (int i = 0; i < 3; i++) P.put(ix + i, iy + j, j == 0 ? kWhite : kGold[3 - (i == 2)], 3);
        P.put(ix + 3, iy + 1, kGold[2], 3); P.put(ix + 3, iy + 2, kGold[2], 3);
        break;
      case 1:   // anvil
        for (int i = -1; i < 5; i++) P.put(ix + i, iy + 1, kIron[3], 3);
        for (int i = 0; i < 4; i++) P.put(ix + i, iy + 2, kIron[2], 3);
        P.put(ix + 1, iy + 3, kIron[1], 3); P.put(ix + 2, iy + 3, kIron[1], 3);
        break;
      default:  // coin pouch
        for (int j = 1; j < 4; j++) for (int i = 0; i < 4; i++) P.put(ix + i, iy + j, kLeather[3 - (i == 3)], 3);
        P.put(ix + 1, iy, kLeather[2], 3); P.put(ix + 2, iy, kLeather[2], 3); P.put(ix + 1, iy + 2, kGold[4], 3);
        break;
    }
  };
  if (b == Building::Inn) sign((float)p.doorX + 9, frontZ0 + 26, 0);
  if (b == Building::Shop) sign((float)p.doorX + 9, frontZ0 + 24, 2);
  if (b == Building::Smithy) sign((float)p.doorX - 21, frontZ0 + 22, 1);
  // lanterns by the door
  if (b == Building::Inn || b == Building::Keep || b == Building::Temple) {
    for (int s = -1; s <= 1; s += 2) {
      auto q = at((float)p.doorX + s * (b == Building::Keep ? 11 : 9), D + 0.5f, frontZ0 + 15);
      P.put(q.first, q.second - 1, kIron[2], 3);
      P.put(q.first, q.second, kGlow[4], 3);
      P.put(q.first, q.second + 1, kGlow[2], 3);
      P.put(q.first - 1, q.second, kIron[1], 3);
      P.put(q.first + 1, q.second, kIron[1], 3);
    }
  }
  // awnings: a striped canopy over the shop front, plain cloth over desert doors and windows
  if (b == Building::Shop || (p.st.awnings && b != Building::Keep && b != Building::Tower)) {
    const Mass& m = p.sc.ms[p.doorMass];
    bool stripes = b == Building::Shop;
    static const Ramp* cloths[3] = {&kRed, &kCloth, &kPurple};
    const Ramp& A = *cloths[hash3(4, 4, p.seed) % 3];
    float ax0 = b == Building::Shop ? m.x0 + 3 : p.doorX - 9.0f, ax1 = b == Building::Shop ? m.x1 - 3 : p.doorX + 9.0f;
    float az = frontZ0 + (b == Building::Shop ? 21.0f : 19.0f);
    for (int x = (int)ax0; x < (int)ax1; x++)
      for (int k = 0; k < 6; k++) {
        auto q = at(x + 0.5f, D + 0.5f + k * 0.5f, az - k);
        bool st = ((x / 4) & 1) == 0;
        const Ramp& R = stripes ? (st ? kRed : kCloth) : A;
        int kk = k == 0 ? 4 : (k < 4 ? 3 : 2);
        P.put(q.first, q.second, R[kk], 3);
      }
    for (int x = (int)ax0; x < (int)ax1; x++) {
      auto q = at(x + 0.5f, D + 3.5f, az - 6);
      if ((x & 3) == 1 || (x & 3) == 2) P.put(q.first, q.second, (stripes && ((x / 4) & 1) == 0) ? kRed[1] : (stripes ? kCloth[1] : A[1]), 3);
    }
    // posts at the awning's ends
    for (int s = 0; s < 2; s++) {
      float x = s == 0 ? ax0 : ax1 - 1;
      for (int z = 0; z < (int)az - 5; z++) {
        auto q = at(x + 0.5f, D + 3.5f, (float)z);
        P.put(q.first, q.second, kWood[s == 0 ? 3 : 1], 3);
      }
    }
  }
  // banners on the keep's front, and a flag on its donjon
  if (b == Building::Keep) {
    for (int x = 22; x < p.W - 22; x += 24) {
      if (std::abs(x + 3 - p.doorX) < 14) continue;
      for (int j = 0; j < 16; j++)
        for (int i = 0; i < 6; i++) {
          if (j >= 14 && (i == 2 || i == 3)) continue;
          if (j == 15 && (i == 1 || i == 4)) continue;
          auto q = at(x + i + 0.5f, D + 0.5f, 31.0f - j);
          int k = i == 0 ? 3 : (i == 5 ? 1 : 2);
          P.put(q.first, q.second, kRed[k], 3);
        }
      auto q = at(x + 0.5f, D + 0.5f, 31.0f);
      for (int i = -1; i <= 6; i++) P.put(q.first + i, q.second - 1, kGold[3], 3);
      P.put(q.first + 2, q.second + 5, kGold[4], 3); P.put(q.first + 3, q.second + 5, kGold[3], 3);
      P.put(q.first + 2, q.second + 6, kGold[3], 3); P.put(q.first + 3, q.second + 6, kGold[2], 3);
    }
    const Mass& dj = p.sc.ms[1];
    float fx = (dj.x0 + dj.x1) * 0.5f, fy = (dj.y0 + dj.y1) * 0.5f;
    float ztop = dj.zTop() + dj.roofH;
    for (int z = (int)ztop - 2; z < (int)ztop + 12; z++) { auto q = at(fx, fy, (float)z); P.put(q.first, q.second, kWood[2], 3); }
    for (int j = 0; j < 5; j++)
      for (int i = 1; i < 9 - j / 2; i++) {
        auto q = at(fx + i, fy, ztop + 11 - j);
        P.put(q.first, q.second + (int)(std::sin(i * 0.9f) * 0.8f), kRed[(i + j) % 3 == 0 ? 3 : 2], 3);
      }
  }
  // the temple's bell and finial; the tower's arcane windows and finial
  if (b == Building::Temple && p.sc.ms.size() >= 2 && !p.sc.ms[1].round && p.sc.ms[1].alongY == false && p.sc.ms[1].wallH >= 40) {
    const Mass& t = p.sc.ms[1];
    float cx = (t.x0 + t.x1) * 0.5f;
    // belfry opening with the bell
    for (int z = t.wallH - 13; z < t.wallH - 4; z++)
      for (int i = -4; i <= 4; i++) {
        auto q = at(cx + i, t.y1 - 0.5f, (float)z);
        bool arch = z > t.wallH - 7 && std::abs(i) > 4 - (t.wallH - 4 - z);
        if (arch) continue;
        P.put(q.first, q.second, kInk, 3);
      }
    for (int j = 0; j < 5; j++)
      for (int i = -2; i <= 2; i++) {
        if (j < 1 && std::abs(i) > 1) continue;
        auto q = at(cx + i, t.y1 - 0.5f, t.wallH - 6.0f - j);
        P.put(q.first, q.second, kGold[i < 0 ? 4 : (i > 0 ? 2 : 3)], 3);
      }
    float ztop = t.zTop() - 1 + t.roofH;
    auto q = at(cx, (t.y0 + t.y1) * 0.5f, ztop);
    for (int k = 1; k < 6; k++) P.put(q.first, q.second - k, kGold[k == 3 ? 4 : 2], 3);
    P.put(q.first - 1, q.second - 4, kGold[3], 3); P.put(q.first + 1, q.second - 4, kGold[1], 3);
  }
  if (b == Building::Tower) {
    const Mass& t = p.sc.ms.back().round ? p.sc.ms.back() : p.sc.ms[p.doorMass];
    if (t.round) {
      for (int i = 0; i < 3; i++) {
        float z = 26.0f + i * 14;
        if (z > t.wallH - 10) break;
        for (int j = 0; j < 7; j++)
          for (int k = 0; k < 4; k++) {
            auto q = at(t.cx - 2 + k + 0.5f, t.cy + t.r - 0.5f, z + 6 - j);
            if (j == 0 && (k == 0 || k == 3)) continue;
            P.put(q.first, q.second, j < 3 ? rgba(176, 156, 255) : rgba(112, 92, 224), 3);
          }
        auto q = at(t.cx - 2 + 0.5f, t.cy + t.r - 0.5f, z - 1);
        for (int k = -1; k <= 4; k++) P.put(q.first + k, q.second, kStone[4], 3);
      }
      float ztop = t.zTop() - 1 + t.roofH;
      auto q = at(t.cx, t.cy, ztop);
      for (int k = 0; k < 5; k++) P.put(q.first, q.second - k, kGold[k == 4 ? 4 : 2], 3);
      // the door at the foot of the tower
      int dw = 10, dh = 15;
      for (int z = 0; z < dh; z++)
        for (int i = -dw / 2 - 1; i <= dw / 2; i++) {
          auto q2 = at((float)p.doorX + i + 0.5f, t.cy + t.r - 0.5f, (float)z);
          bool frame = i == -dw / 2 - 1 || i == dw / 2 || z == dh - 1;
          float ax = (i + 0.5f) / (dw * 0.5f), ay = (z - (dh - 6)) / 6.0f;
          if (z > dh - 6 && ax * ax + ay * ay > 1.0f) continue;
          P.put(q2.first, q2.second, frame ? kStoneWarm[3] : kWoodDark[(i % 3 == 0) ? 1 : 2], 3);
        }
    }
  }
  // chimney mouths that smoke
  if (info) {
    info->smokeN = 0;
    for (const Mass& m : p.sc.ms) {
      if (!m.chimney || !m.smoke || info->smokeN >= 3) continue;
      auto q = at((m.x0 + m.x1) * 0.5f, (m.y0 + m.y1) * 0.5f, m.zTop() + 1);
      info->smokeX[info->smokeN] = q.first;
      info->smokeY[info->smokeN] = q.second - 1;
      info->smokeN++;
    }
  }
}

}  // namespace

Canvas buildingSprite(Building b, int wTiles, int hTiles, const ArchStyle& style, uint32_t seed, BuildingInfo* info) {
  Plan p = makePlan(b, wTiles, hTiles, style, seed);
  buildFacades(p);
  Painter P(p);
  renderBuilding(P);
  overlays(P, info);
  outline(P.c, 0.95f);
  if (info) info->height = p.sc.top;
  return P.c;
}

Canvas buildingSprite(Building b, int wTiles, int hTiles, uint32_t roofColor, uint32_t seed) {
  ArchStyle st = withRoofTint(archForBiome(2, seed), roofColor);
  return buildingSprite(b, wTiles, hTiles, st, seed, nullptr);
}

int buildingHeight(Building b, int wTiles, int hTiles, const ArchStyle& style) {
  Plan p = makePlan(b, wTiles, hTiles, style, 1u);
  return p.sc.top;
}

// ---- city wall -------------------------------------------------------------------------------------
// Walls and towers are height fields over the ground plane, rendered in the oblique view column by column, back to
// front: each ground pixel paints its top at (x, y - z), then the south-facing face below it down to where the next
// pixel's top begins. Each wall tile owns the ground pixels of its cell (plus bevel fills in the empty cells beside
// it and its tower), but renders its whole 3x3 neighbourhood so outlines and faces are decided against the real
// neighbours, then keeps only what it owns: adjacent tiles meet without seams, gaps or double-drawn pixels.
namespace {

constexpr int kBevel = 8;    // legs of the corner bevel / fill triangles, px
constexpr int kTowerR = 11;  // wall tower radius, px
constexpr int kTowerZ = WALL_H + 9;

// a column-by-column oblique renderer for height fields. z(gx, gy) > 0 is solid; top() and face() pick colours.
// owner(gx, gy) marks which footprint pixels belong to this sprite (the last painter of a screen pixel owns it).
template <class ZF, class TOP, class FACE, class OWN>
void renderField(Canvas& c, std::vector<uint8_t>& own, int fx0, int fx1, int fy0, int fy1, int ox, int oy, ZF&& zf, TOP&& top,
                 FACE&& face, OWN&& owner) {
  for (int fx = fx0; fx <= fx1; fx++)
    for (int fy = fy0; fy <= fy1; fy++) {
      int z = zf(fx, fy);
      if (z <= 0) continue;
      uint8_t o = owner(fx, fy) ? 1 : 2;
      int x = ox + fx, row = oy + fy - z;
      auto put = [&](int yy, uint32_t col) {
        if (x < 0 || yy < 0 || x >= c.w || yy >= c.h) return;
        c.set(x, yy, col);
        own[(size_t)yy * c.w + x] = o;
      };
      put(row, top(fx, fy, z));
      int zn = std::max(0, zf(fx, fy + 1));
      int rowN = oy + fy + 1 - zn;
      for (int r = row + 1, v = 0; r < rowN; r++, v++) {
        int h = z - 1 - v;   // height of this face pixel above the ground
        if (h < 0) break;
        put(r, face(fx, fy, z, h, v, zn));
      }
    }
}

// keep only this sprite's pixels, plus outline pixels that touch them
Canvas cropOwned(const Canvas& big, const std::vector<uint8_t>& own, int x0, int y0, int w, int h) {
  Canvas lined = big;
  outline(lined, 0.9f);
  Canvas out(w, h);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      int bx = x0 + x, by = y0 + y;
      if (bx < 0 || by < 0 || bx >= big.w || by >= big.h) continue;
      size_t i = (size_t)by * big.w + bx;
      if (own[i] == 1) { out.set(x, y, lined.px[i]); continue; }
      if (own[i] == 0 && chA(lined.px[i])) {
        bool touch = false;
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
          int nx = bx + dx[k], ny = by + dy[k];
          if (nx >= 0 && ny >= 0 && nx < big.w && ny < big.h && own[(size_t)ny * big.w + nx] == 1) touch = true;
        }
        if (touch) out.set(x, y, lined.px[i]);
      }
    }
  return out;
}

// the wall's footprint shape from a tile's 8-neighbourhood (coordinates relative to the tile's top-left, any cell of
// the 3x3 block). Cells further out are unknown and treated as empty; they never reach the pixels a tile keeps.
struct WallShape {
  bool c[3][3] = {};   // [cy+1][cx+1]: the cell and its 8 neighbours are wall
  WallShape() = default;
  explicit WallShape(uint32_t key) {   // a wall tile with these neighbour bits
    static const int bit[3][3] = {{7, 0, 1}, {6, -1, 2}, {5, 4, 3}};
    for (int j = 0; j < 3; j++)
      for (int i = 0; i < 3; i++) c[j][i] = bit[j][i] < 0 ? true : ((key >> bit[j][i]) & 1u) != 0;
  }
  bool cell(int cx, int cy) const {
    if (cx < -1 || cx > 1 || cy < -1 || cy > 1) return false;
    return c[cy + 1][cx + 1];
  }
  int degree(int cx, int cy) const {
    int n = 0;
    for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && cell(cx + ox, cy + oy)) n++;
    return n;
  }
  bool in(int gx, int gy) const {
    int cx = (gx >= 0 ? gx / 16 : (gx - 15) / 16), cy = (gy >= 0 ? gy / 16 : (gy - 15) / 16);
    if (cx < -1 || cx > 1 || cy < -1 || cy > 1) return false;
    int lx = gx - cx * 16, ly = gy - cy * 16;
    int kx = lx >= 8, ky = ly >= 8;
    int sx = kx ? 1 : -1, sy = ky ? 1 : -1;
    bool H = cell(cx + sx, cy), V = cell(cx, cy + sy), D = cell(cx + sx, cy + sy);
    int dx = kx ? 15 - lx : lx, dy = ky ? 15 - ly : ly;
    bool tri = dx + dy < kBevel;
    if (cell(cx, cy)) return !(tri && !H && !V && !D && degree(cx, cy) >= 2);   // bevelled outer corner
    return tri && H && V;                                                         // filled inner corner / diagonal link
  }
  // who draws a footprint pixel: the cell itself, or for fills in an empty cell the wall cell beside it in the same row
  bool mine(int gx, int gy) const {
    if (gy < 0 || gy > 15) return false;
    if (gx >= 0 && gx <= 15) return true;
    if (gx < -16 || gx > 31) return false;
    int lx = gx < 0 ? gx + 16 : gx - 16;
    return gx < 0 ? lx >= 8 : lx < 8;   // the half of the side cell that touches this tile
  }
};

// irregular flagstones for tower floors and walkways: offset slabs with jittered joints, a few lighter and darker
int flagK(int gx, int gy, uint32_t seed) {
  int row = (gy + 64) / 4, yy = (gy + 64) % 4;
  int off = (int)(hash3(row, 0, seed) % 5);
  int w = 4 + (int)(hash3(row, 1, seed) % 3);
  int col = (gx + 64 + off) / w, xx = (gx + 64 + off) % w;
  if (yy == 3 || xx == 0) return 1;
  uint32_t h = hash3(col, row, seed + 9u);
  int k = 2;
  if (h % 4 == 0) k = 3;
  if (yy == 0 && k == 2 && h % 3 == 0) k = 3;
  return k;
}

inline float towerDist(int gx, int gy, int cx, int cy) { return std::hypot(gx + 0.5f - cx, gy + 0.5f - cy); }

// height of a round tower (centre cx, cy) at a pixel, 0 outside: crenellated rim around a stone floor
int towerZ(int gx, int gy, int cx, int cy, int r, int zTop) {
  float d = towerDist(gx, gy, cx, cy);
  if (d > r) return 0;
  if (d > r - 2.2f) {
    float a = std::atan2(gy + 0.5f - cy, gx + 0.5f - cx);
    int seg = (int)std::floor((a + PI) / TAU * 14.0f + 0.25f);
    return zTop + ((seg & 1) ? 1 : 4);
  }
  return zTop;
}

// masonry courses on a vertical face: h = height above the ground, gx = ground x. Returns a ramp index.
int masonryK(int gx, int h, uint32_t var, int base) {
  int row = h / 4, hh = h % 4;
  int off = (row & 1) * 4;
  int bx = gx + off;
  bool mortarH = hh == 3, mortarV = ((bx % 8) + 8) % 8 == 0;
  if (mortarH || mortarV) return base - 1;
  int brick = (((bx >= 0 ? bx : bx - 7) / 8) % 2 + 2) % 2;   // brick parity: identical on both sides of a tile seam
  uint32_t hsh = hash3(brick, row, 41u + var * 7u);
  int k = base;
  if (hsh % 5 == 0) k = base + 1;
  else if (hsh % 7 == 1) k = base - 1;
  if (hh == 2 && k == base) k = base + 1;   // lit upper edge of each course
  return k;
}

}  // namespace

Canvas wallTile(uint32_t key) {
  WallShape S(key & 255u);
  const bool tower = key & WALL_BIT_TOWER, towerN = key & WALL_BIT_TOWER_N;
  const uint32_t var = (key >> WALL_VAR_SHIFT) & 3u;
  const Ramp& R = kStone;
  // the 3x3 block plus room for heights: footprint x -16..31, y -16..31
  const int BX = 16, BY = 56, BW = 48, BH = 56 + 32 + 4;
  Canvas big(BW, BH);
  std::vector<uint8_t> own((size_t)BW * BH, 0);
  auto inTower = [&](int gx, int gy, int& z) {
    if (tower) { int t = towerZ(gx, gy, 8, 8, kTowerR, kTowerZ); if (t) { z = t; return 1; } }
    if (towerN) { int t = towerZ(gx, gy, 8, -8, kTowerR, kTowerZ); if (t) { z = t; return 2; } }
    return 0;
  };
  auto Z = [&](int gx, int gy) -> int {
    int tz = 0;
    if (inTower(gx, gy, tz)) return tz;
    if (!S.in(gx, gy)) return 0;
    // parapet along every exposed edge: a 2px band, merlons 4 on / 4 off along the edge
    bool eN = !S.in(gx, gy - 1), eS = !S.in(gx, gy + 1), eW = !S.in(gx - 1, gy), eE = !S.in(gx + 1, gy);
    bool e2 = !S.in(gx, gy - 2) || !S.in(gx, gy + 2) || !S.in(gx - 2, gy) || !S.in(gx + 2, gy);
    if (eN || eS || eW || eE || e2) {
      int along = (eN || eS || (!eW && !eE && (!S.in(gx, gy - 2) || !S.in(gx, gy + 2)))) ? gx : gy;
      bool merlon = (((along % 8) + 8) % 8) < 4;
      return WALL_H + (merlon ? 5 : 2);
    }
    return WALL_H;
  };
  auto top = [&](int gx, int gy, int z) -> uint32_t {
    int tz = 0;
    int t = inTower(gx, gy, tz);
    int k;
    if (t) {
      const int cy = t == 1 ? 8 : -8;
      float d = towerDist(gx, gy, 8, cy);
      if (z > kTowerZ) k = (gx + gy) % 3 == 0 ? 3 : 4;   // merlon caps
      else {
        k = flagK(gx, gy, 61u) + 1;                        // flagstone floor, lit
        if (d > kTowerR - 3.4f) k = std::max(1, k - 1);    // shade under the rim
        if (std::abs(gx - 9) <= 1 && std::abs(gy - cy - 1) <= 1) return kWoodDark[(gx == 8) ? 3 : 1];   // hatch
      }
    } else if (z > WALL_H + 2) k = 4;                      // merlon tops
    else if (z > WALL_H) k = 3;                            // crenel sills
    else {
      // walkway flagstones
      int row = ((gy % 4) + 4) % 4, col = (((gx + ((gy >> 2) & 1) * 2) % 5) + 5) % 5;
      k = (row == 3 || col == 0) ? 2 : 3;
      if (hash3((gx + 64) / 5, (gy + 64) / 4, 23u) % 7 == 0 && k == 3) k = 4;
    }
    // shade cast by anything taller just up-left
    int zul = Z(gx - 1, gy - 1), zu = Z(gx, gy - 1);
    if (zul > z + 1 || zu > z + 2) k = std::max(0, k - 1);
    if (zul > z + 3 && Z(gx - 2, gy - 2) > z + 3) k = std::max(0, k - 1);
    return R[k];
  };
  auto face = [&](int gx, int gy, int z, int h, int v, int zNext) -> uint32_t {
    int tz = 0;
    int t = inTower(gx, gy, tz);
    if (t) {
      // round tower: cylinder light across, masonry courses, an arrow slit facing out
      const int cy = t == 1 ? 8 : -8;
      (void)cy;
      float u = (gx + 0.5f - 8) / kTowerR;
      int base = lightIndex(lightAt(std::clamp(u, -0.95f, 0.95f) * 0.95f, 0.15f), gx, h, 0.12f);
      base = std::clamp(base, 1, 3);
      if (v == 0) return R[std::min(4, base + 1)];
      if (z > kTowerZ && h >= kTowerZ - 1) return R[std::min(4, base + 1)];   // merlon fronts
      if (h == kTowerZ - 2) return R[std::max(0, base - 1)];                  // shadow under the rim
      if (std::abs(gx - 8) <= 0 && h >= 9 && h <= 15 && zNext == 0) return kInk;            // arrow slit
      if (gx == 9 && h >= 9 && h <= 15 && zNext == 0) return R[std::max(0, base - 1)];
      if (h < 4) return R[std::max(0, base - 1 - (h == 3 ? -1 : 0))];          // plinth
      return R[std::clamp(masonryK(gx, h, var, base), 0, 4)];
    }
    if (zNext > 0) {
      // inner face of the parapet above the walkway: in its own shade
      return R[v == 0 ? 3 : 1];
    }
    // outer face of the wall
    if (v == 0) return R[4];                                  // lit coping edge
    if (h >= WALL_H + 2) return R[2];                         // merlon fronts
    if (h == WALL_H + 1) return R[3];                         // cornice
    if (h == WALL_H) return R[1];                             // shadow line under the cornice
    if (h < 3) {                                              // foundation course, darker, bigger blocks
      bool joint = ((gx + (h == 1 ? 3 : 0)) % 6 + 6) % 6 == 0;
      return R[joint ? 0 : 1];
    }
    if (h == 3) return R[3];                                  // lit lip of the plinth
    int k = masonryK(gx, h, var, 2);
    // grime and moss near the ground, rain streaks from the crenels
    if (h <= 6 && hash3(gx + 40, h, 91u + var) % 4 == 0) k = std::max(0, k - 1);
    if (var >= 2) {   // moss creeping up from the foot in soft patches
      int mh = 3 + (int)(hash3((gx + 40) / 2, 3, 17u + var) % 4) - (int)(hash3((gx + 41) / 3, 4, 19u) % 3);
      if (h <= mh && hash3((gx + 40) / 5, 6, 29u + var) % 3 == 0) return kMoss[h == mh ? 2 : 1];
    }
    if (hash3(gx + 40, 1, 33u + var) % 9 == 0 && h > 8 && h < WALL_H - 1) k = std::max(1, k - 1);
    return R[std::clamp(k, 0, 4)];
  };
  auto owner = [&](int gx, int gy) -> bool {
    int tz = 0;
    int t = inTower(gx, gy, tz);
    if (t == 1) return true;
    if (t == 2) return false;
    return S.mine(gx, gy);
  };
  renderField(big, own, -16, 31, -16, 31, BX, BY, Z, top, face, owner);
  // the canvas keeps x -WALL_OX..15+WALL_OX and rows from WALL_OY above the tile down to 20 below its top
  return cropOwned(big, own, BX - WALL_OX, BY - WALL_OY, WALL_CW, WALL_CH);
}

Canvas wallPiece(int mask) {
  bool n = mask & 1, e = mask & 2, s = mask & 4, w = mask & 8;
  uint32_t k = (n ? WALL_BIT_N : 0) | (e ? WALL_BIT_E : 0) | (s ? WALL_BIT_S : 0) | (w ? WALL_BIT_W : 0);
  if (n && e) k |= WALL_BIT_NE;
  if (s && e) k |= WALL_BIT_SE;
  if (s && w) k |= WALL_BIT_SW;
  if (n && w) k |= WALL_BIT_NW;
  if ((int)n + (int)e + (int)s + (int)w <= 1) k |= WALL_BIT_TOWER;
  return wallTile(k);
}

// ---- gatehouse ---------------------------------------------------------------------------------------
Canvas gateHouse(uint32_t seed) {
  // footprint x -16..63 (flank tile, three passage tiles, flank tile), y 0..15; towers centred on the flank tiles
  const Ramp& R = kStone;
  const int BX = GATE_OX, BY = GATE_OY;
  Canvas c(GATE_CW, GATE_CH);
  std::vector<uint8_t> own((size_t)GATE_CW * GATE_CH, 0);
  const int TR = 13, TZ = WALL_H + 16, BZ = WALL_H + 10;   // tower radius / height, gate block height
  const int t0x = -8, t1x = 56, tcy = 8;
  const int ax0 = 3, ax1 = 44, archH = 24;   // the arch opening on the front face (ground x range, height)
  auto Z = [&](int gx, int gy) -> int {
    int z = std::max(towerZ(gx, gy, t0x, tcy, TR, TZ), towerZ(gx, gy, t1x, tcy, TR, TZ));
    if (z) return z;
    if (gx < 0 || gx > 47 || gy < 1 || gy > 15) return 0;
    // gate block: walkway with a parapet front and back
    if (gy <= 2 || gy >= 14) return BZ + (((gx % 8) < 4) ? 5 : 2);
    return BZ;
  };
  auto which = [&](int gx, int gy) { return towerZ(gx, gy, t0x, tcy, TR, TZ) ? 0 : (towerZ(gx, gy, t1x, tcy, TR, TZ) ? 1 : 2); };
  auto archTop = [&](int gx) {   // height of the arch soffit above ground at this x, -1 outside the opening
    if (gx < ax0 || gx > ax1) return -1;
    float u = (gx + 0.5f - (ax0 + ax1 + 1) * 0.5f) / ((ax1 - ax0 + 1) * 0.5f);
    return (int)std::lround(archH - 8 + 8 * std::sqrt(std::max(0.0f, 1 - u * u)));
  };
  auto top = [&](int gx, int gy, int z) -> uint32_t {
    int w = which(gx, gy);
    if (w < 2) {
      int cx = w == 0 ? t0x : t1x;
      float d = towerDist(gx, gy, cx, tcy);
      if (z > TZ) return R[(gx + gy) % 3 == 0 ? 3 : 4];
      int k = flagK(gx, gy, 62u) + 1;
      if (d > TR - 3.4f) k = std::max(1, k - 1);
      int zul = Z(gx - 1, gy - 1);
      if (zul > z + 1) k = std::max(0, k - 1);
      return R[k];
    }
    if (z > BZ + 2) return R[4];
    if (z > BZ) return R[3];
    int k = ((gy % 4) == 3 || ((gx + (gy / 4) * 2) % 5) == 0) ? 2 : 3;
    if (Z(gx - 1, gy - 1) > z + 1 || Z(gx, gy - 1) > z + 2) k--;
    return R[k];
  };
  auto face = [&](int gx, int gy, int z, int h, int v, int zNext) -> uint32_t {
    int w = which(gx, gy);
    if (w < 2) {
      int cx = w == 0 ? t0x : t1x;
      float u = (gx + 0.5f - cx) / TR;
      int base = std::clamp(lightIndex(lightAt(std::clamp(u, -0.95f, 0.95f) * 0.95f, 0.15f), gx, h, 0.12f), 1, 3);
      if (v == 0 || (z > TZ && h >= TZ - 1)) return R[std::min(4, base + 1)];
      if (h == TZ - 2) return R[base - 1];
      if (zNext == 0 && std::abs(gx - cx) <= 0 && ((h >= 22 && h <= 28) || (h >= 10 && h <= 15))) return kInk;   // arrow slits
      if (zNext == 0 && gx - cx == 1 && ((h >= 22 && h <= 28) || (h >= 10 && h <= 15))) return R[base - 1];
      if (h < 3) return R[base - 1];
      if (h == 3) return R[std::min(4, base + 1)];
      return R[std::clamp(masonryK(gx, h, seed & 3u, base), 0, 4)];
    }
    if (zNext > 0) return R[v == 0 ? 3 : 1];
    // front face of the gate block with the arch
    int at = archTop(gx);
    if (at >= 0 && h < at) return 0;   // the opening: see through to the passage
    if (v == 0) return R[4];
    if (h >= BZ + 2) return R[2];
    if (h == BZ + 1) return R[3];
    if (h == BZ) return R[1];
    // voussoirs: a ring of light and dark wedges around the arch
    if (at >= 0 && h >= at && h < at + 3) {
      int wedge = ((gx - ax0) / 3) & 1;
      if (h == at + 2) return R[1];
      return R[wedge ? 4 : 3];
    }
    if (std::abs(gx - 24) <= 1 && at >= 0 && h >= at + 3 && h <= at + 4) return R[4];   // keystone
    if (h < 3) return R[1];
    if (h == 3) return R[3];
    return R[std::clamp(masonryK(gx, h, seed & 3u, 2), 0, 4)];
  };
  auto owner = [&](int, int) { return true; };
  // the field must reach the bottom of the round towers (tcy + TR = 21), or their lower front is never painted
  renderField(c, own, -16 - 8, 63 + 8, -8, tcy + TR + 1, BX, BY, Z, top, face, owner);
  // the passage: dark vault in the upper part of the opening, the raised portcullis teeth, a lit floor beyond
  for (int gx = ax0; gx <= ax1; gx++) {
    int at = archTop(gx);
    int x = BX + gx;
    int frontBase = BY + 15;   // ground row of the front face
    for (int h = at - 1; h >= 0; h--) {
      int y = frontBase - h;
      uint32_t col;
      int depth = at - 1 - h;   // rows below the soffit
      if (depth < 7) col = mix(rgba(30, 24, 40), rgba(44, 36, 54), depth / 7.0f);   // vault in shadow
      else continue;          // lower part: see through to the passage floor (terrain)
      c.set(x, y, col);
    }
    // portcullis teeth hanging just under the soffit
    if ((gx - ax0) % 3 == 1) for (int t = 0; t < 4; t++) c.set(x, frontBase - (at - 1) + t, t == 3 ? kIron[3] : kIron[1]);
    c.set(x, frontBase - (at - 1) + 2, kIron[2]);
  }
  // coat of arms over the arch, banners on the towers, lanterns either side of the opening
  {
    int sx = BX + 21, sy = BY + 15 - (archH + 9);
    for (int j = 0; j < 7; j++)
      for (int i = 0; i < 6; i++) {
        if (j >= 5 && (i == 0 || i == 5)) continue;
        if (j == 6 && (i == 1 || i == 4)) continue;
        c.set(sx + i, sy + j, (i == 0 || j == 0) ? kRed[3] : (i == 5 || j == 6 ? kRed[1] : kRed[2]));
      }
    c.set(sx + 2, sy + 2, kGold[4]); c.set(sx + 3, sy + 2, kGold[3]); c.set(sx + 2, sy + 3, kGold[3]); c.set(sx + 3, sy + 3, kGold[2]);
    c.set(sx + 2, sy + 4, kGold[2]);
  }
  for (int side = 0; side < 2; side++) {
    int cx = side == 0 ? t0x : t1x;
    int bx = BX + cx - 3, by = BY + 15 - (TZ - 6);
    for (int j = 0; j < 16; j++)
      for (int i = 0; i < 6; i++) {
        if (j >= 14 && (i == 2 || i == 3)) continue;
        if (j == 15 && (i == 1 || i == 4)) continue;
        int k = i == 0 ? 3 : (i == 5 ? 1 : 2);
        if (j == 0) k = 1;
        c.set(bx + i, by + j, kRed[k]);
      }
    hline(c, bx - 1, bx + 6, by - 1, kGold[3]);
    c.set(bx + 2, by + 5, kGold[4]); c.set(bx + 3, by + 5, kGold[3]); c.set(bx + 2, by + 6, kGold[3]); c.set(bx + 3, by + 6, kGold[2]);
    c.set(bx + 1, by + 6, kGold[2]); c.set(bx + 4, by + 6, kGold[1]); c.set(bx + 2, by + 7, kGold[2]); c.set(bx + 3, by + 7, kGold[1]);
    // lantern on a bracket by the arch
    int lx = BX + (side == 0 ? ax0 - 3 : ax1 + 3), ly = BY + 15 - 16;
    c.set(lx, ly - 1, kIron[2]); c.set(lx, ly, kGlow[4]); c.set(lx, ly + 1, kGlow[2]); c.set(lx - 1, ly, kIron[1]); c.set(lx + 1, ly, kIron[1]);
  }
  outline(c, 0.9f);
  // the outline must not close the opening at the ground line
  for (int gx = ax0; gx <= ax1; gx++) c.set(BX + gx, BY + 16, 0);
  return c;
}

Canvas gatePiece() { return gateHouse(0); }

int wallShadeAt(const uint8_t* wall, int W, int H, int px, int py) {
  auto at = [&](int x, int y) { return x >= 0 && y >= 0 && x < W && y < H && wall[(size_t)y * W + x] != 0; };
  // the wall's ground shape at a pixel: build the 3x3 neighbourhood of the pixel's cell
  auto shapeAt = [&](int gx, int gy) {
    int cx = gx >= 0 ? gx / 16 : (gx - 15) / 16, cy = gy >= 0 ? gy / 16 : (gy - 15) / 16;
    WallShape S;
    bool any = false;
    for (int j = -1; j <= 1; j++)
      for (int i = -1; i <= 1; i++) { S.c[j + 1][i + 1] = at(cx + i, cy + j); any = any || S.c[j + 1][i + 1]; }
    return any && S.in(gx - cx * 16, gy - cy * 16);
  };
  if (shapeAt(px, py)) return 0;
  // contact shade right at the foot of the wall face, then the cast shadow, which falls down-right
  if (shapeAt(px, py - 1) || shapeAt(px - 1, py - 1)) return 2;
  static const int sx[6] = {1, 2, 3, 4, 5, 6}, sy[6] = {1, 1, 2, 3, 3, 4};
  for (int k = 0; k < 6; k++)
    if (shapeAt(px - sx[k], py - sy[k])) return 1;
  return 0;
}

void wallKeys(const uint8_t* wall, int W, int H, const std::pair<int, int>* gates, int nGates, std::vector<uint32_t>& keys) {
  keys.assign((size_t)W * H, 0);
  auto at = [&](int x, int y) { return x >= 0 && y >= 0 && x < W && y < H && wall[(size_t)y * W + x] != 0; };
  std::vector<uint8_t> flank((size_t)W * H, 0), tower((size_t)W * H, 0);
  std::vector<std::pair<int, int>> marks;   // towers and gate centres placed so far (spacing)
  for (int i = 0; i < nGates; i++) {
    int gx = gates[i].first, gy = gates[i].second;
    // 1 = hidden under the gatehouse tower; 2 = still drawn: a flank joined to the ring only diagonally keeps its
    // wall piece (and the diagonal link it paints), or a sliver of ground shows between the link and the tower
    if (at(gx - 1, gy)) flank[(size_t)gy * W + gx - 1] = at(gx - 2, gy) ? 1 : 2;
    if (at(gx + 3, gy)) flank[(size_t)gy * W + gx + 3] = at(gx + 4, gy) ? 1 : 2;
    marks.push_back({gx + 1, gy});
  }
  const int nGateMarks = (int)marks.size();
  auto degree = [&](int x, int y) {
    int n = 0;
    for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && at(x + ox, y + oy)) n++;
    return n;
  };
  auto clear = [&](int x, int y, int towerGap, int gateGap) {
    for (int i = 0; i < (int)marks.size(); i++) {
      int d = std::max(std::abs(marks[i].first - x), std::abs(marks[i].second - y));
      if (d < (i < nGateMarks ? gateGap : towerGap)) return false;
    }
    return true;
  };
  auto place = [&](int x, int y) { tower[(size_t)y * W + x] = 1; marks.push_back({x, y}); };
  // 1. every wall end (and lone pier) gets a tower
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      if (at(x, y) && !flank[(size_t)y * W + x] && degree(x, y) <= 1) place(x, y);
  // 2. strong corners: an L whose two arms run straight for 3+ tiles
  struct Cand { int score, x, y; };
  std::vector<Cand> cands;
  auto run = [&](int x, int y, int dx, int dy) {
    int n = 0;
    for (int k = 1; k <= 6; k++) {
      int px = x + dx * k, py = y + dy * k;
      if (!at(px, py)) break;
      bool straight = dx ? (!at(px, py - 1) && !at(px, py + 1)) : (!at(px - 1, py) && !at(px + 1, py));
      if (!straight) break;
      n++;
    }
    return n;
  };
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!at(x, y) || flank[(size_t)y * W + x] || tower[(size_t)y * W + x]) continue;
      bool e = at(x + 1, y), w = at(x - 1, y), n = at(x, y - 1), s = at(x, y + 1);
      if ((e == w) || (n == s)) continue;
      int a = run(x, y, e ? 1 : -1, 0), b = run(x, y, 0, s ? 1 : -1);
      if (a >= 3 && b >= 3) cands.push_back({a + b, x, y});
    }
  std::stable_sort(cands.begin(), cands.end(), [](const Cand& p, const Cand& q) { return p.score > q.score; });
  for (const Cand& c : cands)
    if (clear(c.x, c.y, 6, 4)) place(c.x, c.y);
  // 3. long runs: a tower wherever nothing stands within 9 tiles
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!at(x, y) || flank[(size_t)y * W + x] || tower[(size_t)y * W + x]) continue;
      bool e = at(x + 1, y), w = at(x - 1, y), n = at(x, y - 1), s = at(x, y + 1);
      bool straight = (e && w && !n && !s) || (n && s && !e && !w);
      if (straight && degree(x, y) == 2 && clear(x, y, 10, 5)) place(x, y);
    }
  // keys
  static const int ndx[8] = {0, 1, 1, 1, 0, -1, -1, -1}, ndy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!at(x, y) || flank[(size_t)y * W + x] == 1) continue;
      uint32_t k = 0;
      for (int b = 0; b < 8; b++) if (at(x + ndx[b], y + ndy[b])) k |= 1u << b;
      if (tower[(size_t)y * W + x]) k |= WALL_BIT_TOWER;
      if (y > 0 && tower[(size_t)(y - 1) * W + x]) k |= WALL_BIT_TOWER_N;
      k |= (hash3(x, y, 777u) & 3u) << WALL_VAR_SHIFT;
      keys[(size_t)y * W + x] = k;
    }
}

}  // namespace art
