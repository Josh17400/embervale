// EMBERVALE art: buildings, roofs, city walls and the gate. See rpg/art.h for the contract and rpg/art/art_internal.h for the shared helpers.
//
// Buildings are built from masses (boxes and cylinders: the main house, wings, cross gables, dormers, chimneys,
// turrets, steeples) and painted as real volumes in the oblique 3/4 view: each mass's front wall is a facade canvas
// (material, windows, doors) and all roofs form one height field, rendered column by column from back to front with
// per-pixel normals lit from the top-left, material texels laid along the courses, cast shadows from chimneys and
// turrets, fascia boards on every eave, and the eave's shadow on the wall below. The building type adds only its
// function (sign, forge, steeple, crenellations, awning); the ArchStyle decides how it is built.
#include "rpg/art/art_internal.h"
#include "rpg/art/art_heraldry.h"

#include <chrono>
#include <cstdio>
#include <memory>
#include <cstdlib>

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
// M3 culture materials
const Ramp kPalm = ramp5(rgba(56, 54, 40), rgba(94, 88, 52), rgba(138, 128, 68), rgba(178, 166, 92), rgba(212, 200, 132));
const Ramp kBarkRoof = ramp5(rgba(40, 30, 38), rgba(64, 50, 48), rgba(94, 74, 60), rgba(126, 104, 80), rgba(160, 140, 110));
const Ramp kFelt = ramp5(rgba(118, 102, 96), rgba(168, 150, 132), rgba(212, 198, 172), rgba(232, 222, 200), rgba(248, 242, 226));
const Ramp kJadeTile = ramp5(rgba(14, 50, 56), rgba(24, 88, 80), rgba(40, 132, 104), rgba(88, 180, 132), rgba(178, 230, 182));
const Ramp kLeafRoof = ramp5(rgba(24, 50, 46), rgba(38, 84, 52), rgba(66, 122, 56), rgba(112, 162, 66), rgba(182, 206, 104));
const Ramp kRubble = ramp5(rgba(58, 50, 58), rgba(96, 86, 86), rgba(136, 124, 112), rgba(174, 162, 142), rgba(208, 198, 176));
const Ramp kAshlar = ramp5(rgba(122, 108, 108), rgba(178, 164, 150), rgba(222, 210, 186), rgba(240, 232, 212), rgba(252, 250, 238));
const Ramp kDaub = ramp5(rgba(108, 84, 64), rgba(158, 126, 90), rgba(198, 168, 120), rgba(220, 196, 150), rgba(240, 224, 186));
const Ramp kLiving = ramp5(rgba(42, 34, 40), rgba(70, 54, 48), rgba(104, 82, 62), rgba(140, 114, 82), rgba(176, 152, 108));
const Ramp kLacquer = ramp5(rgba(70, 18, 30), rgba(120, 28, 34), rgba(170, 44, 40), rgba(208, 80, 56), rgba(238, 136, 96));
const Ramp kGlowElf = ramp5(rgba(40, 110, 120), rgba(70, 170, 170), rgba(130, 220, 200), rgba(196, 248, 226), rgba(240, 255, 246));

// M3: the archetype a style was made for (ArchStyle::culture = cult::Archetype + 1); CU_NONE: the biome stand-in
enum Cul : uint8_t { CU_NONE, CU_FJORD, CU_HIGHLAND, CU_HEART, CU_IMPERIAL, CU_DUNE, CU_STEPPE, CU_MARSH, CU_JADE, CU_RIVER, CU_SUN,
                     CU_SYLVAN, CU_STAR };

enum class Surf : uint8_t { None, Roof, Flat, Parapet, CapRim, CapMouth, Crown };

// ---------------------------------------------------------------- facade canvas: u right, v up from the ground
struct Facade {
  int w = 0, h = 0;
  std::vector<uint32_t> px;
  std::vector<int> top;   // highest v of the wall at each u (gables rise)
  std::vector<uint8_t> glass;   // 1 = a window pane (lit from inside at night, see BuildingInfo::glass)
  Facade() = default;
  Facade(int w_, int h_) : w(w_), h(h_), px((size_t)w_ * h_, 0), top((size_t)w_, h_ - 1), glass((size_t)w_ * h_, 0) {}
  void set(int u, int v, uint32_t c) {
    if (u >= 0 && v >= 0 && u < w && v < h && v <= top[(size_t)u]) { px[(size_t)v * w + u] = c; glass[(size_t)v * w + u] = 0; }
  }
  void markGlass(int u, int v) { if (u >= 0 && v >= 0 && u < w && v < h && v <= top[(size_t)u] && px[(size_t)v * w + u]) glass[(size_t)v * w + u] = 1; }
  bool isGlass(int u, int v) const { return u >= 0 && v >= 0 && u < w && v < h && glass[(size_t)v * w + u]; }
  uint32_t get(int u, int v) const { return (u >= 0 && v >= 0 && u < w && v < h) ? px[(size_t)v * w + u] : 0; }
  bool in(int u, int v) const { return u >= 0 && v >= 0 && u < w && v < h && v <= top[(size_t)u]; }
};

enum class WallKind : uint8_t { Timber, Plaster, Stone, Brick, Log, Adobe, Planks, Rubble, Wattle, Felt, Ashlar, Living };

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
  // M0b
  float backHip = 0;        // > 0: a ridge running north-south (alongY) is hipped down to the back eave at this rate,
                            // so a front-gabled roof keeps its street gable but stays inside the generator's clearance
  int jetty = 0, jettyV = 0;  // jettied upper floor: the wall below facade row jettyV stands jetty px back (and in at the
                              // sides), under the overhang of the floor above
  bool body = false;        // the building's main body (storeys are counted on it)
  int floorV = 0;           // facade row of the upper floor line (0 = one storey)
  // M3
  bool barn = false;        // board-and-batten barn walls (white trim boards); culture plank walls are plain boards
  bool found = false;       // a foundation (plinth, terrace, platform): a flat-topped stone box under the walls
  bool tier = false;        // an upper tier of a pagoda / a stacked roof: no doors, a band of windows
  uint8_t cul = 0;          // Cul of the style
  uint16_t orn = 0;         // ORN_* bits of the style
  uint8_t win = 0, door = 0;   // WindowShape / DoorShape
  uint32_t accent = 0, alt = 0;   // accent and alt tints (0 none)
  float stairX0 = 0, stairX1 = 0; // a stepped pyramid's great stair up its front (x range): a smooth incline there
  float zTop() const { return (float)(zBase + wallH); }
  float frontY(float x) const {
    if (!round) return (x >= x0 && x < x1) ? y1 - 0.5f : -1e9f;
    float dx = x - cx;
    return std::fabs(dx) < r ? cy + std::sqrt(r * r - dx * dx) - 0.5f : -1e9f;
  }
};

// M3: roofs that are a flat deck behind a parapet (a dome or an onion bulb may stand on it)
inline bool flatShape(RoofShape r) { return r == RoofShape::FlatParapet || r == RoofShape::Dome || r == RoofShape::Onion; }

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
  if (m.found) {   // M3: a foundation's flat top
    if (x < m.x0 || x >= m.x1 || y < m.y0 || y >= m.y1) return -1;
    surf = Surf::Flat;
    return zt;
  }
  if (m.round) {
    float rr = std::hypot(x - m.cx, y - m.cy);
    if (m.shape == RoofShape::Conical || m.shape == RoofShape::Steep || m.shape == RoofShape::Pagoda || m.shape == RoofShape::Sweep) {
      float R = m.r + m.ov;
      if (rr > R) return -1;
      float t = 1 - rr / R;
      if (m.shape == RoofShape::Pagoda || m.shape == RoofShape::Sweep) t = t * t * 0.55f + t * 0.45f;   // a swept, concave cone
      if (m.rmat == RoofMat::Felt) {
        // M3 a yurt: a low felt cone that bellies out (rafters bent over the lattice), the crown ring (toono) at its top
        if (rr < 3.4f) { surf = Surf::Crown; return zt - 1 + m.roofH * (1 - 3.4f / R) + 1.0f; }
        t = 1 - (1 - t) * (1 - t) * 0.35f - (1 - t) * 0.65f;   // a slight dome to the slope
        return zt - 1 + m.roofH * std::min(t, 1 - 3.4f / R);
      }
      return zt - 1 + m.roofH * t;
    }
    if (m.shape == RoofShape::Onion) {   // M3 a bulb dome wider than its drum, drawn up into a point
      float R = m.r + 1.5f;
      if (rr > R) return -1;
      float t = rr / R;
      float f = std::sqrt(std::max(0.0f, 1 - t * t)) * 0.58f + 0.42f * std::pow(1 - t, 2.4f);
      if (t < 0.12f) f += (0.12f - t) * 2.2f;   // the spike
      return zt + 2 + m.roofH * f;
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
  if (m.shape == RoofShape::FlatParapet || m.shape == RoofShape::Dome || m.shape == RoofShape::Onion) {
    if (x < m.x0 || x >= m.x1 || y < m.y0 || y >= m.y1) return -1;
    if (m.shape == RoofShape::Dome) {   // a dome on a drum in the middle of a flat roof
      float R = std::min(m.x1 - m.x0, m.y1 - m.y0) * 0.5f - 7;
      float dcx = (m.x0 + m.x1) * 0.5f, dcy = (m.y0 + m.y1) * 0.5f - 1;
      // (M3 fixer round 3) its ground circle drawn as an ellipse, foreshortened in depth like the view: a hemisphere,
      // not an egg standing on the deck
      float rr = std::hypot(x - dcx, (y - dcy) / 0.74f);
      if (rr < R) { float t = rr / R; return zt + 3 + m.roofH * std::sqrt(std::max(0.0f, 1 - t * t)); }
      if (rr < R + 1.5f) { surf = Surf::Parapet; return zt + 3; }   // the drum's rim
    }
    if (m.shape == RoofShape::Onion) {   // M3 an onion bulb on a drum in the middle of a flat roof
      float R = std::min(m.x1 - m.x0, m.y1 - m.y0) * 0.5f - 6;
      float dcx = (m.x0 + m.x1) * 0.5f, dcy = (m.y0 + m.y1) * 0.5f - 1;
      float rr = std::hypot(x - dcx, y - dcy);
      if (rr < R) {
        float t = rr / R;
        float f = std::sqrt(std::max(0.0f, 1 - t * t)) * 0.58f + 0.42f * std::pow(1 - t, 2.4f);
        if (t < 0.12f) f += (0.12f - t) * 2.2f;
        return zt + 6 + m.roofH * f;
      }
      if (rr < R + 1.5f) { surf = Surf::Parapet; return zt + 6; }   // the drum
    }
    float e = std::min(std::min(x - m.x0, m.x1 - 1 - x), std::min(y - m.y0, m.y1 - 1 - y));
    if (e < 2) {
      surf = Surf::Parapet;
      if (m.crenel) {
        float along = (y - m.y0 < 2 || m.y1 - 1 - y < 2) ? x : y;
        if (m.cul == CU_SUN) {   // (M3 fixer round 3) the sun temples' stepped merlons (almenas), not a castle's crenels
          static const int alm[8] = {4, 6, 8, 8, 6, 4, 2, 2};
          return zt + alm[(((int)along % 8) + 8) % 8];
        }
        return zt + ((((int)along % 8) + 8) % 8 < 4 ? 6 : 3);
      }
      return zt + 3;
    }
    surf = Surf::Flat;
    return zt;
  }
  const float rx0 = m.x0 - m.ov, rx1 = m.x1 + m.ov, ry0 = m.y0 - m.ov, ry1 = m.y1 + m.ovF;
  if (x < rx0 || x >= rx1 || y < ry0 || y >= ry1) return -1;
  // thatch is laid round the corners: a rounded eave outline instead of a ruler-cut rectangle (a hipped thatch roof
  // otherwise reads as one flat slab from above; the rounded corners give it the soft, bulky cottage silhouette)
  if ((m.rmat == RoofMat::Thatch || m.rmat == RoofMat::Palm) && m.shape != RoofShape::Turf && !m.gambrel) {
    const float R = 6.0f;
    float qx = std::clamp(x, rx0 + R, rx1 - R), qy = std::clamp(y, ry0 + R, ry1 - R);
    if ((x - qx) * (x - qx) + (y - qy) * (y - qy) > R * R) return -1;
  }
  const float ze = zt - 1;
  float dy = std::min(y - ry0, ry1 - y), dx = std::min(x - rx0, rx1 - x);
  float d, dmax;
  switch (m.shape) {
    case RoofShape::Hip: case RoofShape::Pagoda: case RoofShape::Mansard: case RoofShape::Stepped:
      d = std::min(dy, dx * m.hk);
      if (m.alongY) d = std::min(dx, dy * m.hk);
      dmax = m.alongY ? (rx1 - rx0) * 0.5f : (ry1 - ry0) * 0.5f;
      if (m.alongY && m.backHip > 0) d = std::min(d, (y - ry0) * m.backHip);
      break;
    case RoofShape::Sweep:   // M3 elven: two planes, the ridge running the whole length (its ends sweep up below)
      d = m.alongY ? dx : dy;
      dmax = m.alongY ? (rx1 - rx0) * 0.5f : (ry1 - ry0) * 0.5f;
      if (m.alongY && m.backHip > 0) d = std::min(d, (y - ry0) * m.backHip);
      break;
    default:   // gable, steep, turf: two planes
      d = m.alongY ? dx : dy;
      dmax = m.alongY ? (rx1 - rx0) * 0.5f : (ry1 - ry0) * 0.5f;
      // side-gabled roofs (ridge across the view) are clipped at both ends into a half-hip: in the 3/4 view their
      // gable triangles face east and west and are never seen, so without the clipped ends the roof is one flat
      // front plane (a featureless slab). The small end facets catch the light (west) and fall into shade (east).
      if (!m.alongY && !m.gambrel) d = std::min(d, dx * 1.1f + dmax * 0.32f);
      if (m.alongY && m.backHip > 0) d = std::min(d, (y - ry0) * m.backHip);
      break;
  }
  float t = std::clamp(d / std::max(1.0f, dmax), 0.0f, 1.0f);
  float prof = t;
  if (m.shape == RoofShape::Turf) prof = 1 - (1 - t) * (1 - t) * 0.85f - 0.15f * (1 - t);   // rounded sod hump
  if (m.shape == RoofShape::Pagoda) {
    prof = t * t * 0.55f + t * 0.45f;
    float cxd = std::min(x - rx0, rx1 - x), cyd = std::min(y - ry0, ry1 - y);
    if (m.cul == CU_NONE) {
      float c = std::max(0.0f, 5 - std::max(cxd, cyd));   // corners sweep up
      return ze + m.roofH * prof + c * 0.9f;
    }
    // M3: the eaves curve up along their length toward each corner (a smooth sweep, not a horn)
    const float e = std::max(cxd, cyd) / 12.0f, nearEave = std::max(0.0f, 1 - t * 2.2f);
    const float lift = 6.0f * nearEave * nearEave * std::max(0.0f, 1 - e) * std::max(0.0f, 1 - e);
    return ze + m.roofH * prof + lift;
  }
  if (m.gambrel) prof = t < 0.45f ? t * 1.55f : 0.70f + (t - 0.45f) * 0.55f;
  if (m.shape == RoofShape::Mansard) prof = t < 0.42f ? t * 1.8f : 0.756f + (t - 0.42f) * 0.42f;   // steep sides, a low top
  if (m.shape == RoofShape::Stepped) {   // terraces: flat treads, upright risers (the renderer fills them as faces)
    const float n = 4.0f;
    prof = std::min(1.0f, std::floor(t * n + 0.25f) / n);
    if (m.stairX1 > m.stairX0 && x >= m.stairX0 && x < m.stairX1 && y > (m.y0 + m.y1) * 0.5f) prof = std::min(1.0f, t * 1.06f);   // the great stair
  }
  if (m.shape == RoofShape::Sweep) {
    // a concave sweep, the ridge sagging a little in the middle and both ends (and the eave corners) swept up
    prof = t * t * 0.45f + t * 0.55f;
    const float span = m.alongY ? (ry1 - ry0) : (rx1 - rx0);
    const float e = m.alongY ? std::min(y - ry0, ry1 - y) : std::min(x - rx0, rx1 - x);   // to the nearer end
    const float c = (m.alongY ? (y - (ry0 + ry1) * 0.5f) : (x - (rx0 + rx1) * 0.5f)) / std::max(1.0f, span * 0.5f);
    const float lift = std::max(0.0f, 9.0f - e) * (0.45f + 0.45f * t);
    return ze + m.roofH * prof - 2.0f * t * (1 - c * c) + lift;
  }
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
      int wave = (int)std::lround(std::sin(u * 0.29f + (float)(seed % 7)) * 1.1f);   // gently rolling course lines
      // (integration: kept mostly at the plane's own value so hip and gable planes read as separate lit volumes;
      // the old +-1 speckle on every row drowned the plane lighting and made thatch roofs look flat)
      // (M0 round 3: the old texture was short horizontal dashes in rows, which read as bricks or stone blocks. Straw
      // now runs DOWN the slope: each course is a combed bundle with vertical strands, a ragged lit butt edge at its
      // foot and a continuous wavy shadow line under it, so the roof reads as layered thatch.)
      int vv = v + wave + 70;
      const int CH = 7;
      int p = vv % CH, row = vv / CH;
      uint32_t h = hash3(u, row, seed);
      if (p == CH - 1) return -1;                       // the soft shadow under the next course's butt: a full line,
      if (p == CH - 2 && (h & 1)) return -1;            // ragged upward where straw ends hang over it
      if (p == 0) return (h % 3) ? 1 : 0;               // lit straw ends along the butt edge
      // combed strands: 1 px vertical strokes of varying length down the course, dense and staggered per column, so
      // the course reads as bundled straw (never as brick joints)
      uint32_t s = hash3(u, row, seed + 11);
      const int a = 1 + (int)((s >> 4) % 2), len = 2 + (int)((s >> 6) % 3);
      if (s % 3 == 0 && p >= a && p < a + len) return -1;
      if (s % 5 == 1 && p >= 1 && p <= 3) return 1;
      return 0;
    }
    case RoofMat::Shingle: {
      // courses of wooden shingles: the butt of each course is lit, the course below sits in its shadow
      int row = (v + 63) / 4, p = (v + 63) % 4;
      int off = (row & 1) * 3 + (int)(hash3(row, 7, seed) % 2);
      int w = 6, col = (u + off + 64) / w, q = (u + off + 64) % w;
      uint32_t h = hash3(col, row, seed);
      // (quieter than before: lit butts on every course plus joints and speckle drowned the two planes' light and
      // shade, and a shingle roof read as a noisy check pattern at 1x)
      if (p == 3) return -1;
      if (q == 0 && p == 1) return -1;
      if (p == 0) return (h % 3 == 0) ? 1 : 0;
      return (h % 13 == 0) ? -1 : 0;
    }
    case RoofMat::Slate: {
      int row = (v + 63) / 3, p = (v + 63) % 3;
      int off = (row & 1) * 2;
      int col = (u + off + 64) / 4, q = (u + off + 64) % 4;
      uint32_t h = hash3(col, row, seed);
      // (M0 round 3: no more isolated light slates (h % 9 -> +1): on a flat front plane they read as random light
      // rectangles. The courses carry a lit lower lip and a shadow line; the plane light does the rest.)
      if (p == 2) return -1;
      if (q == 0 && p == 1) return -1;
      if (p == 0) return q == 0 ? 0 : 1;   // a continuous lit lip per course, broken only at the joints
      (void)h;
      return 0;
    }
    case RoofMat::ClayTile: {
      int q = ((u % 4) + 4) % 4, p = ((v + 64) % 5);
      int k = q == 0 ? 1 : (q == 3 ? -1 : 0);   // barrel tiles: lit crest, shaded trough
      if (p == 0) k = q == 3 ? -1 : 1;           // the lip of each course
      if (p == 1 && q != 0) k = -1;
      return k;
    }
    case RoofMat::Turf: {
      // sod: short grass in loose tufts (a lit blade over a dark root), quiet enough that the two planes, the ridge
      // and the eave read; blotchy +-1 noise made sod roofs a flat lawn
      uint32_t h = hash3(u, v, seed);
      int k = 0;
      if (h % 13 == 0) k = 1;
      else if (hash3(u, v + 1, seed) % 13 == 0) k = -1;   // the shade under the tuft above
      if (hash3(u, v, seed + 77) % 97 == 0) special = true;   // a flower
      return k;
    }
    case RoofMat::Copper: {
      int q = ((u % 6) + 6) % 6;
      int k = q == 0 ? 1 : (q == 1 ? -1 : 0);   // standing seams
      if (hash3(u / 2, v / 3, seed) % 9 == 0) k += 1;   // patina blooms
      return std::clamp(k, -1, 1);
    }
    // ---- M3 culture roofs
    case RoofMat::Palm: {
      // layered palm fronds (screen-aligned like thatch): each course ends in a row of V-shaped frond tips with a dark
      // gap under them, the leaflets running down the course as fine diagonal ribs
      int vv = v + 70;
      const int CH = 5;
      int p = vv % CH, row = vv / CH;
      int sh = (int)(hash3(row, 5, seed) % 4);
      int tip = ((u + sh) % 4 + 4) % 4;
      if (p == CH - 1) return (tip == 1 || tip == 2) ? 0 : -1;   // the tips of the course above hang into the gap
      if (p == 0) return tip == 0 ? 0 : 1;
      if (((u + p + row * 2) % 3) == 0) return p == 1 ? 0 : -1;
      return 0;
    }
    case RoofMat::Bark: {
      // big overlapping slabs of bark, irregular widths, deep shadow lines and fissures
      int row = (v + 63) / 5, p = (v + 63) % 5;
      int off = (int)(hash3(row, 9, seed) % 7);
      int w = 7 + (int)(hash3(row, 3, seed) % 4);
      int col = (u + off + 70) / w, q = (u + off + 70) % w;
      uint32_t h = hash3(col, row, seed);
      if (p == 4) return -1;
      if (q == 0 && p >= 2) return -1;
      if (p == 0) return (h % 3) ? 1 : 0;
      if ((h >> 4) % 4 == 0 && q == (int)((h >> 8) % (uint32_t)w) && p >= 1) return -1;   // a fissure
      return 0;
    }
    case RoofMat::Felt: {
      // felt: smooth, the binding ropes running round the roof in rings and down it from the crown
      if (((v + 64) % 9) == 0) return -1;
      if (((u % 11) + 11) % 11 == 0) return -1;
      if (((v + 64) % 9) == 1) return 1;
      return hash3(u / 3, v / 2, seed) % 29 == 0 ? -1 : 0;
    }
    case RoofMat::GlazedTile: {
      // glazed round tiles: lit crests, dark channels, the ends of each course, and glints of the glaze
      int q = ((u % 4) + 4) % 4, p = ((v + 64) % 4);
      int k = q == 1 ? 1 : (q == 3 ? -1 : 0);
      if (p == 0) k = q == 3 ? -1 : 1;
      if (q == 1 && p == 2 && hash3(u / 4, (v + 64) / 4, seed) % 3 == 0) special = true;
      return k;
    }
    case RoofMat::Leaf: {
      // elven living thatch: overlapping leaves in courses, each a pointed scale with a lit tip and a dark vein
      int row = (v + 64) / 4, p = (v + 64) % 4;
      int off = (row & 1) * 3;
      int q = (((u + off) % 6) + 6) % 6;
      int k = 0;
      if (p == 0) k = (q == 0 || q == 5) ? -1 : 1;
      else if (q == 0) k = -1;
      else if (p == 3 && (q == 2 || q == 3)) k = 1;
      if (hash3((u + off) / 6, row, seed) % 47 == 0) special = true;   // a leaf turned gold (M3 fixer: rarer: 1 in 13 streaked the roof)
      return k;
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
  std::vector<int> rubRow, rubStart;   // M3 rubble: the course of each facade row, and where each course starts
  if (m.wall == WallKind::Rubble) {
    rubRow.assign((size_t)F.h + 1, 0);
    int v = 0, r = 0;
    while (v <= F.h) {
      const int h = 3 + (int)(hash3(r, 9, m.seed) % 3);
      rubStart.push_back(v);
      for (int k = 0; k < h && v + k <= F.h; k++) rubRow[(size_t)(v + k)] = r;
      v += h;
      r++;
    }
    rubStart.push_back(v);
  }
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
          if (!m.barn) {   // M3 plain vertical boards: a dark gap between them, a split or a knot here and there
            k = q == 0 ? 0 : (q == 1 ? 3 : 2);
            uint32_t hb = hash3(u / 4, v / 7, m.seed + 3);
            if (q != 0 && hb % 9 == 0) k = 1;
            if (q == 2 && hb % 13 == 1 && v % 7 == 3) k = 0;
          }
          col = R[k];
          break;
        }
        case WallKind::Rubble: {
          // M3 random rubble: courses of irregular stones, each its own tone, a lit top and a dark foot, set in deep mortar
          const int row = rubRow[(size_t)v], hh = v - rubStart[(size_t)row], rh = rubStart[(size_t)row + 1] - rubStart[(size_t)row];
          int x = u + (int)(hash3(row, 1, m.seed) % 5), s = 0, s0 = 0, sw = 3;
          for (;;) {
            sw = 3 + (int)(hash3(s, row, m.seed + 7) % 5);
            if (x < s0 + sw) break;
            s0 += sw; s++;
          }
          const int q = x - s0;
          uint32_t hs = hash3(s, row, m.seed + 11);
          int base = 1 + (int)(hs % 3);
          k = base;
          if (hh == rh - 1) k = std::min(4, base + 1);
          if (hh == 0) k = std::max(0, base - 1);
          if (q == 1 && hh > 0) k = std::min(4, k + 1);
          col = R[k];
          bool mortar = q == 0 || hh == 0 && (q == 1 || q == sw - 1);
          if ((q == 0 || q == sw - 1) && (hh == rh - 1)) mortar = true;   // rounded stone corners
          if (mortar) col = mix(R[0], kSoil[1], 0.3f);
          break;
        }
        case WallKind::Wattle: {
          // M3 wattle and daub: lime-washed daub, worn through in places to the woven hazel beneath
          float n = vnoise(u / 6.0f, v / 5.0f, m.seed + 21);
          k = n < 0.22f ? 1 : (n > 0.78f ? 3 : 2);
          col = R[k];
          if (n > 0.66f && v > 3) {   // the weave: rods round upright stakes, over and under
            const int st = ((u % 7) + 7) % 7, band = (v / 2) & 1;
            const bool over = ((u / 7) + (v / 2)) & 1;
            if (st == 0) col = kWood[1];
            else col = over ? kWood[band ? 3 : 2] : kWoodDark[band ? 2 : 1];
            if (v % 2 == 0) col = darken(col, 0.25f);
          }
          break;
        }
        case WallKind::Felt: {
          // M3 felt stretched over the lattice: soft panels with seams, darker toward the ground
          const int seam = ((u + (int)(m.seed % 5)) % 13 + 13) % 13;
          k = 2;
          if (seam == 0) k = 1;
          if (seam == 1) k = 3;
          if (v < 3) k = 1;
          if (hash3(u / 3, v / 3, m.seed) % 17 == 0) k = std::max(1, k - 1);
          col = R[k];
          break;
        }
        case WallKind::Ashlar: {
          // M3 dressed ashlar: big squared blocks in level courses, fine joints, a lit arris on every course
          const int row = v / 5, hh = v % 5;
          const int bw = 10 + (int)(hash3(row, 2, m.seed) % 4);
          const int bx = u + (row & 1) * (bw / 2) + (int)(hash3(row, 4, m.seed) % 3), q = bx % bw;
          uint32_t hb = hash3(bx / bw, row, m.seed + 5);
          k = 2 + (hb % 5 == 0 ? 1 : 0) - (hb % 7 == 1 ? 1 : 0);
          if (hh == 4) k = 3;
          if (hh == 0 || q == 0) k = 1;
          col = R[k];
          break;
        }
        case WallKind::Living: {
          // M3 elven living wood: upright trunks grown side by side, each rounded (lit west, shaded east), bark running
          // up them, with dark seams where they meet
          int x = u, t = 0, t0 = 0, tw = 6;
          for (;;) {
            tw = 5 + (int)(hash3(t, 3, m.seed + 13) % 4);
            if (x < t0 + tw) break;
            t0 += tw; t++;
          }
          const float c = ((x - t0) + 0.5f) / tw * 2 - 1;   // -1 west edge .. 1 east edge
          k = c < -0.45f ? 3 : (c < 0.35f ? 2 : 1);
          if (x == t0) k = 0;
          if (((v + (int)(hash3(t, 5, m.seed) % 4)) % 5 == 0) && std::fabs(c) < 0.6f && hash3(x, v / 5, m.seed) % 3 == 0) k = std::max(0, k - 1);
          if (hash3(t, v / 9, m.seed + 17) % 23 == 0 && std::fabs(c) < 0.3f) k = 4;   // a knot catching the light
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
    } else if (m.wall == WallKind::Rubble || m.wall == WallKind::Ashlar) {
      // M3 big dressed quoins (long and short blocks) bond the corners of rubble and ashlar
      const Ramp& Q = m.wall == WallKind::Rubble ? kStoneWarm : R;
      int blk = (v / 5) & 1;
      if (v <= tl) for (int i = 0; i < (blk ? 4 : 7) && i < w; i++) F.set(i, v, Q[(v % 5 == 0) ? 1 : (i == 0 ? 4 : 3)]);
      if (v <= tr) for (int i = 0; i < (blk ? 7 : 4) && i < w; i++) F.set(w - 1 - i, v, Q[(v % 5 == 0) ? 0 : (i == 0 ? 1 : 2)]);
    } else if (m.wall == WallKind::Wattle || (m.wall == WallKind::Planks && !m.barn)) {
      // M3 corner posts (and, on wattle, the posts between the panels)
      if (v <= tl) { F.set(0, v, kBeam[3]); F.set(1, v, kBeam[2]); }
      if (v <= tr) { F.set(w - 2, v, kBeam[1]); F.set(w - 1, v, kBeam[0]); }
    } else if (m.wall == WallKind::Felt) {
      if (v <= tl) F.set(0, v, R[3]);
      if (v <= tr) F.set(w - 1, v, R[1]);
    } else if (m.wall == WallKind::Living) {
      // M3 the corner trunks are the oldest: thick, and lit / shaded round their curve
      if (v <= tl) { F.set(0, v, R[4]); F.set(1, v, R[3]); F.set(2, v, R[3]); F.set(3, v, R[2]); }
      if (v <= tr) { F.set(w - 4, v, R[2]); F.set(w - 3, v, R[1]); F.set(w - 2, v, R[1]); F.set(w - 1, v, R[0]); }
    }
  }
  // M3 wattle: the frame posts between its panels and the wall plate under the eave
  if (m.wall == WallKind::Wattle) {
    for (int u = 13 + (int)(m.seed % 5); u < w - 6; u += 14)
      for (int v = 0; v <= F.top[(size_t)u] && v < m.wallH; v++) { F.set(u, v, kBeam[3]); F.set(u + 1, v, kBeam[1]); }
    for (int u = 0; u < w; u++) if (m.wallH - 1 <= F.top[(size_t)u]) { F.set(u, m.wallH - 1, kBeam[2]); F.set(u, m.wallH - 2, kBeam[1]); }
  }
  // M3 felt: the bands of rope that hold the felt to the lattice, and the culture's painted band between them
  if (m.wall == WallKind::Felt) {
    const int b0 = m.wallH - 3, b1 = std::max(4, m.wallH / 2 - 1);
    for (int u = 0; u < w; u++) {
      for (int bv : {b0, b1}) {
        if (bv > F.top[(size_t)u]) continue;
        F.set(u, bv, ((u + bv) % 3 == 0) ? kWood[1] : kCloth[3]);
        F.set(u, bv - 1, ((u + bv) % 3 == 1) ? kWood[0] : kCloth[1]);
      }
      if (m.accent) {   // a band of the culture's ornament: a running meander in the accent colour
        const Ramp A = ramp(opaque(m.accent));
        for (int j = 0; j < 4; j++) {
          const int v = b1 + 2 + j;
          if (v >= b0 - 1 || v > F.top[(size_t)u]) continue;
          const int q = ((u % 8) + 8) % 8;
          const bool on = j == 0 || j == 3 || (j == 1 && (q == 1 || q == 2 || q == 3 || q == 5)) || (j == 2 && (q == 1 || q == 5 || q == 6 || q == 7));
          F.set(u, v, on ? A[(j == 0) ? 3 : 2] : (m.alt ? ramp(opaque(m.alt))[3] : R[3]));
        }
      }
    }
  }
  // timber frame: sill beam, posts, braces, head beam
  if (m.wall == WallKind::Timber) {
    int wh = m.wallH;
    auto beamH = [&](int v) { for (int u = 0; u < w; u++) { if (v <= F.top[(size_t)u]) F.set(u, v, kBeam[2]); if (v - 1 >= 0) F.set(u, v - 1, kBeam[1]); } };
    beamH(plinth + 1);
    beamH(wh - 1);
    if (m.floorV > 0) beamH(m.floorV + 1);          // M0b: the floor beam between the storeys
    else if (wh > 30 && !m.body) beamH(wh / 2 + 2);   // a tall wing
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
  if (m.wall == WallKind::Planks && m.barn) {   // barn: white trim boards at the corners and the eave line
    for (int v = 0; v < F.h; v++) {
      if (v <= F.top[0]) { F.set(0, v, kCloth[4]); F.set(1, v, kCloth[3]); }
      if (v <= F.top[(size_t)(w - 1)]) { F.set(w - 2, v, kCloth[2]); F.set(w - 1, v, kCloth[1]); }
    }
  }
  // M3 a wattle or rubble house's gable is boarded
  if (m.wall == WallKind::Wattle)
    for (int u = 0; u < w; u++)
      for (int v = m.wallH; v <= F.top[(size_t)u]; v++) F.set(u, v, kWood[u % 3 == 0 ? 1 : (u % 3 == 1 ? 3 : 2)]);
  // M3 painted bands (ORN_PAINTED_BANDS): a frieze under the eave in the accent colour with a pattern in the second
  // accent, and a dado stripe above the footing
  if ((m.orn & ORN_PAINTED_BANDS) && m.accent && m.wall != WallKind::Felt && !m.found && m.wallH > 14) {
    const Ramp A = ramp(opaque(m.accent)), B = m.alt ? ramp(opaque(m.alt)) : ramp(rgba(236, 226, 196));
    const int f0 = m.wallH - 6;
    for (int u = 0; u < w; u++) {
      for (int v = f0; v < f0 + 4; v++) {
        if (v > F.top[(size_t)u]) continue;
        const int q = ((u % 6) + 6) % 6;
        bool pat = (v == f0 + 1 || v == f0 + 2) && (q == 1 || q == 2 || (v == f0 + 2 && q == 3) || (v == f0 + 1 && q == 0));
        F.set(u, v, pat ? B[3] : A[v == f0 + 3 ? 3 : (v == f0 ? 1 : 2)]);
      }
      if (plinth + 2 <= F.top[(size_t)u]) { F.set(u, plinth + 2, A[2]); F.set(u, plinth + 1, A[1]); }
    }
  }
  // the plinth: a stone footing below wooden and plastered walls
  if (m.wall == WallKind::Timber || m.wall == WallKind::Plaster || m.wall == WallKind::Log || m.wall == WallKind::Planks || m.wall == WallKind::Wattle) {
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
  // the panes (not the mullion and transom): lit from inside at night
  for (int j = 0; j < wh; j++)
    for (int i = 0; i < ww; i++)
      if (i != ww / 2 && j != wh / 2) F.markGlass(u0 + i, v0 + j);
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

// ---------------------------------------------------------------- M3: windows and doors in the culture's shapes
// a pane colour: glass (sky reflected high, the room dark below) or, for paper lattices, warm rice paper
uint32_t paneCol(bool paper, int i, int j, int ww, int wh) {
  if (paper) return (i + j) % 5 == 0 ? rgba(222, 206, 168) : (j >= wh - 2 ? rgba(244, 234, 206) : rgba(232, 220, 188));
  int k = 2;
  if (j >= wh - 2 && i <= 1) k = 3;
  if (i == ww - 1 || j == wh - 1 || j == 0) k = 1;
  return kGlass[k];
}
void sillAndLintel(Mass& m, int u0, int v0, int ww, int wh, bool lintel) {
  Facade& F = m.f;
  const Ramp& T = m.tR;
  if (lintel) for (int i = -2; i <= ww + 1; i++) F.set(u0 + i, v0 + wh + 1, T[3]);
  for (int i = -2; i <= ww + 1; i++) { F.set(u0 + i, v0 - 1, T[4]); F.set(u0 + i, v0 - 2, T[1]); }
  for (int i = -1; i <= ww; i++) { uint32_t c = F.get(u0 + i, v0 - 3); if (c) F.set(u0 + i, v0 - 3, darken(c, 0.35f)); }
  if (m.snow) for (int i = -2; i <= ww + 1; i++) F.set(u0 + i, v0 - 1, kSnow[4]);
}
void facadeWindowS(Mass& m, WindowShape ws, int u0, int v0, int ww, int wh, uint32_t seed, bool shutters, bool flowers, uint32_t shutterCol) {
  Facade& F = m.f;
  const Ramp& T = m.tR;
  switch (ws) {
    case WindowShape::Arched: facadeWindow(m, u0, v0, ww, wh, seed, shutters, flowers, true, shutterCol); return;
    case WindowShape::Tall:   // tall sash windows with a transom light: town houses on the river, imperial fronts
      facadeWindow(m, u0, v0 - 1, ww, wh + 3, seed, shutters, flowers, false, shutterCol);
      for (int i = 0; i < ww; i++) F.set(u0 + i, v0 - 1 + wh, T[2]);
      return;
    case WindowShape::Slit: {   // an arrow slit: a deep dark splay with a lit sill
      const int uc = u0 + ww / 2;
      for (int j = 0; j < wh; j++) { F.set(uc - 1, v0 + j, m.wR[1]); F.set(uc, v0 + j, kInk); F.set(uc + 1, v0 + j, rgba(28, 22, 36)); F.set(uc + 2, v0 + j, m.wR[0]); }
      for (int i = -1; i <= 2; i++) { F.set(uc + i, v0 - 1, T[4]); F.set(uc + i, v0 + wh, T[3]); }
      return;
    }
    case WindowShape::Round: {   // a round window with a cross of glazing bars (elven halls, gable eyes)
      const float cx = u0 + ww * 0.5f, cy = v0 + wh * 0.5f, r = std::min(ww, wh) * 0.5f + 0.5f;
      for (int j = -1; j <= wh; j++)
        for (int i = -2; i <= ww + 1; i++) {
          const float dx = u0 + i + 0.5f - cx, dy = v0 + j + 0.5f - cy, d = std::sqrt(dx * dx + dy * dy);
          if (d > r + 1.3f) continue;
          if (d > r) { F.set(u0 + i, v0 + j, T[dx + dy < 0 ? 1 : 4]); continue; }   // the frame: shaded inside top-left
          const bool bar = std::fabs(dx) < 0.6f || std::fabs(dy) < 0.6f;
          F.set(u0 + i, v0 + j, bar ? T[2] : (dx < -0.5f && dy > 0.5f ? kGlass[3] : kGlass[d > r - 1.2f ? 1 : 2]));
          if (!bar) F.markGlass(u0 + i, v0 + j);
        }
      F.set((int)(cx - r * 0.45f), (int)(cy + r * 0.4f), kGlass[4]);
      return;
    }
    case WindowShape::Lattice: case WindowShape::Screen: {
      // Lattice: a square window of fine wooden lattice over paper (jade terraces) or glass; Screen: a carved wooden
      // grille box (mashrabiya) standing out from a dune house's wall, its shadow below it
      const bool paper = ws == WindowShape::Lattice && (m.cul == CU_JADE || m.cul == CU_IMPERIAL);
      const bool screen = ws == WindowShape::Screen;
      const Ramp& W = screen ? kWood : (m.cul == CU_JADE ? kLacquer : T);
      for (int j = -1; j <= wh; j++)
        for (int i = -1; i <= ww; i++) {
          const int u = u0 + i, v = v0 + j;
          if (i == -1 || i == ww || j == -1 || j == wh) { F.set(u, v, W[(i == -1 || j == wh) ? 3 : 1]); continue; }
          bool bar = screen ? ((i + j) % 3 == 0 || (i - j + 30) % 3 == 0) : (i % 2 == 1 || j % 2 == 1);
          if (bar) F.set(u, v, W[(j == wh - 1) ? 3 : 2]);
          else {
            F.set(u, v, screen ? rgba(30, 22, 34) : paneCol(paper, i, j, ww, wh));
            F.markGlass(u, v);
          }
        }
      if (screen) {   // its cap and corbel, and the shade it throws on the wall below
        for (int i = -2; i <= ww + 1; i++) { F.set(u0 + i, v0 + wh + 1, kWood[3]); F.set(u0 + i, v0 - 2, kWood[1]); }
        for (int i = -1; i <= ww; i++) { F.set(u0 + i, v0 - 3, kWood[0]); uint32_t c = F.get(u0 + i + 1, v0 - 4); if (c) F.set(u0 + i + 1, v0 - 4, darken(c, 0.35f)); }
      } else sillAndLintel(m, u0, v0, ww, wh, true);
      (void)shutters; (void)flowers; (void)seed; (void)shutterCol;
      return;
    }
    case WindowShape::Pointed: {   // a tall lancet with a pointed head and a Y of tracery (high elves)
      const int hh = wh + 4;
      for (int j = -1; j <= hh; j++)
        for (int i = -1; i <= ww; i++) {
          const float cx = (ww - 1) * 0.5f;
          const int top = hh - (int)std::lround(std::fabs(i - cx) * 1.3f);
          if (j > top) continue;
          const int u = u0 + i, v = v0 + j;
          if (i == -1 || i == ww || j == -1 || j == top) { F.set(u, v, T[(i == -1 || j == -1) ? 3 : (j == top ? 4 : 1)]); continue; }
          const bool trac = (i == ww / 2 && j < hh - 3) || (j >= hh - 3 && std::abs(i - ww / 2) == hh - 1 - j);
          if (trac) { F.set(u, v, T[2]); continue; }
          F.set(u, v, (j >= hh - 4 && i < ww / 2) ? kGlass[3] : kGlass[j == 0 || i == ww - 1 ? 1 : 2]);
          F.markGlass(u, v);
        }
      for (int i = -2; i <= ww + 1; i++) { F.set(u0 + i, v0 - 1, T[4]); F.set(u0 + i, v0 - 2, T[1]); }
      return;
    }
    default: facadeWindow(m, u0, v0, ww, wh, seed, shutters, flowers, false, shutterCol); return;
  }
}

// a door in the culture's shape, centred on facade column uc (the door stays in the bottom row at doorCol: frozen)
void facadeDoorS(Mass& m, DoorShape ds, int uc, int dw, int dh, bool arched, const Ramp& wood) {
  Facade& F = m.f;
  const Ramp& T = m.tR;
  const Ramp A = m.accent ? ramp(opaque(m.accent)) : kRed;
  switch (ds) {
    case DoorShape::Arched: facadeDoor(m, uc, dw, dh, true, wood); return;
    case DoorShape::Double: {   // two leaves under a lintel (or an arch on masonry), a dark seam between, two rings
      const int w2 = dw + 4;
      facadeDoor(m, uc, w2, dh, arched, m.accent ? A : wood);
      for (int v = 0; v < dh - 2; v++) F.set(uc - (w2 / 2) + w2 / 2, v, kInk);
      F.set(uc - 2, dh / 2, kGold[4]); F.set(uc + 1, dh / 2, kGold[3]);
      return;
    }
    case DoorShape::Curtain: case DoorShape::Flap: {
      // Curtain: a dark doorway hung with a cloth, drawn aside; Flap: a yurt's painted wooden door frame with the felt
      // flap rolled up over it
      const int u0 = uc - dw / 2;
      for (int j = 0; j < dh; j++)
        for (int i = -1; i <= dw; i++) {
          const int v = dh - 1 - j;
          if (i == -1 || i == dw) { F.set(u0 + i, v, ds == DoorShape::Flap ? A[i < 0 ? 3 : 1] : T[i < 0 ? 3 : 1]); continue; }
          uint32_t c = j < 2 ? rgba(20, 14, 26) : rgba(36, 26, 36);
          if (ds == DoorShape::Curtain) {
            // the curtain hangs over the left two thirds, gathered toward its foot
            const int edge = (dw * 2) / 3 - (j > dh - 5 ? (dh - j) / 2 : 0);
            if (i < edge) { const int f = (i + (j > dh / 2 ? 1 : 0)) % 3; c = A[f == 0 ? 3 : (f == 1 ? 2 : 1)]; if (i == edge - 1) c = A[0]; }
          } else if (j < 3) c = A[j == 0 ? 3 : 2];   // the painted lintel of the frame
          F.set(u0 + i, v, c);
        }
      if (ds == DoorShape::Flap)   // the rolled felt over the door
        for (int i = -1; i <= dw; i++) { F.set(u0 + i, dh, m.wR[3]); F.set(u0 + i, dh + 1, m.wR[4]); F.set(u0 + i, dh + 2, m.wR[2]); }
      else for (int i = -2; i <= dw + 1; i++) F.set(u0 + i, dh, T[3]);
      return;
    }
    case DoorShape::Round: case DoorShape::Moon: {
      // Round: a round elven door with a boss in the middle; Moon: a moon gate, a round opening in the wall, the court
      // beyond in shade
      const bool moon = ds == DoorShape::Moon;
      const float r = moon ? dh * 0.55f : dw * 0.55f + 0.5f;
      const float cx = (float)uc, cy = moon ? r - 1.0f : std::min(r, dh * 0.5f);
      for (int j = -2; j <= (int)(cy + r) + 2; j++)
        for (int i = -(int)r - 3; i <= (int)r + 3; i++) {
          const float dx = i + 0.5f, dy = j + 0.5f - cy, d = std::sqrt(dx * dx + dy * dy);
          const int u = (int)cx + i, v = j;
          if (v < 0) continue;
          if (d > r + 1.6f) continue;
          if (d > r) { F.set(u, v, moon ? (m.cul == CU_JADE ? kLacquer[dx + dy < 0 ? 1 : 3] : T[dx + dy < 0 ? 1 : 4]) : T[dx + dy < 0 ? 1 : 3]); continue; }
          if (moon) {
            // a pair of lattice doors standing in the round opening: lacquered frames, paper panels above, a seam
            const Ramp& L = m.cul == CU_JADE ? kLacquer : wood;
            uint32_t c;
            if (i == 0 || i == -1) c = i == 0 ? L[1] : L[0];                         // the meeting stiles
            else if (d > r - 1.2f) c = dx + dy < 0 ? L[0] : L[1];                    // the reveal in shadow
            else if (j < 3) c = L[2];                                                // kick panels
            else if (j > cy - 1 && ((i + 40) % 3 == 0 || (j % 3) == 0)) c = L[2];   // lattice over paper
            else if (j > cy - 1) { c = rgba(226, 212, 176); F.set(u, v, c); F.markGlass(u, v); continue; }
            else c = L[(i + 40) % 4 == 0 ? 1 : 2];
            F.set(u, v, c);
          }
          else {
            const int q = ((i + 40) % 3);
            uint32_t c = wood[q == 0 ? 1 : 2];
            if (d > r - 1.2f && dx + dy < 0) c = wood[0];
            F.set(u, v, c);
          }
        }
      if (!moon) { F.set(uc, (int)cy, kGold[4]); F.set(uc + 1, (int)cy, kGold[2]); }
      return;
    }
    default: facadeDoor(m, uc, dw, dh, arched, wood); return;
  }
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
  BuildingFacts facts;       // M0b: storeys (resolved, >= 1) and hearth
  int budget = 48;           // M0b: px the sprite may rise above the footprint's top edge (riseBudgetPx)
  uint32_t rowsShown = 0;    // M0b: bit s set when storey s shows on the body (a door or windows on the ground floor,
                             // a row of windows, slits or a band above it): BuildingInfo::storeys counts them
  bool balcony = false, hood = false;   // M0b: 2-storey variety (a balcony on the upper floor, a hood over the door)
  Building trade = Building::House;     // M1 economy: the type asked for (b is its frame, artBase); signs and machinery
  // M3
  int cul = CU_NONE;                    // the style's archetype (Cul)
  Foundation fnd = Foundation::None;    // a plinth / terrace / platform under the walls (p.zBase is its height)
  bool porch = false;                   // a columned porch before the door (ORN_PORCH_COLUMNS, facade variety)
  int winStep = 16, winOff = 0;         // the window rhythm along a facade (facade variety)
  int doorFrame = 0;                    // 0 plain, 1 pilasters, 2 a fanlight, 3 a painted surround
  uint32_t shutterCol = 0;              // 0: the seed's own
};

// M3: the culture's materials for each building type (the type only adds function, in the culture's idiom): the
// temple, keep and palace build in the culture's monumental material, the smithy in something that does not burn
void cultureTypeRules(Plan& p, Building b) {
  ArchStyle& st = p.st;
  const int cul = p.cul;
  const uint32_t seed = p.seed;
  auto weak = [](WallMat w) { return w == WallMat::Timber || w == WallMat::Plaster || w == WallMat::Log || w == WallMat::Plank || w == WallMat::Wattle; };
  auto monumentWall = [&]() -> WallMat {
    switch (cul) {
      case CU_FJORD: return b == Building::Temple ? WallMat::Plank : WallMat::Stone;
      case CU_HIGHLAND: return WallMat::Rubble;
      case CU_IMPERIAL: case CU_SUN: case CU_STAR: return WallMat::Ashlar;
      case CU_DUNE: return WallMat::Adobe;
      case CU_STEPPE: return b == Building::Temple ? WallMat::Felt : WallMat::Adobe;
      case CU_MARSH: return WallMat::Plank;
      case CU_JADE: return WallMat::Plaster;
      case CU_RIVER: return WallMat::Brick;
      case CU_SYLVAN: return WallMat::Living;
      default: return WallMat::Stone;
    }
  };
  switch (b) {
    case Building::StoneHouse:
      if (weak(st.wall) || st.wall == WallMat::Felt) {
        const WallMat mw = monumentWall();
        st.wall = (mw == WallMat::Plank || mw == WallMat::Felt || mw == WallMat::Plaster) ? ((seed & 8) ? WallMat::Brick : WallMat::Stone) : mw;
      }
      break;
    case Building::Temple: case Building::Keep: case Building::Tower: case Building::Palace: case Building::Barracks:
      if (weak(st.wall) || (st.wall == WallMat::Felt && b != Building::Temple)) st.wall = monumentWall();
      if (st.roofMat == RoofMat::Thatch || st.roofMat == RoofMat::Turf)
        st.roofMat = cul == CU_FJORD ? RoofMat::Shingle : (cul == CU_MARSH ? RoofMat::Thatch : RoofMat::Slate);
      if (b == Building::Palace && st.roofMat == RoofMat::Shingle && cul != CU_FJORD) st.roofMat = RoofMat::Slate;
      if (st.roof == RoofShape::Turf) st.roof = RoofShape::Steep;
      if (st.roof == RoofShape::Conical && !(cul == CU_STEPPE && b == Building::Temple)) st.roof = cul == CU_STEPPE ? RoofShape::FlatParapet : RoofShape::Hip;
      if (cul == CU_STEPPE && b != Building::Temple) { st.roofMat = RoofMat::Adobe; st.roof = RoofShape::FlatParapet; }
      break;
    case Building::Smithy:
      if (st.wall == WallMat::Timber || st.wall == WallMat::Plaster || st.wall == WallMat::Felt || st.wall == WallMat::Wattle)
        st.wall = cul == CU_HIGHLAND ? WallMat::Rubble : (cul == CU_DUNE || cul == CU_STEPPE ? WallMat::Adobe : (cul == CU_RIVER ? WallMat::Brick : WallMat::Stone));
      break;
    case Building::Hut:
      if (st.wall == WallMat::Stone || st.wall == WallMat::Brick || st.wall == WallMat::Ashlar) st.wall = cul == CU_HIGHLAND ? WallMat::Rubble : WallMat::Wattle;
      // (M3 fixer round 3, review: "Imperial towns use Heartland thatch") the Imperial and River peoples' poor still
      // roof in fired clay tile; the rest in their own straw
      if (cul == CU_IMPERIAL || cul == CU_RIVER) { if (st.roofMat != RoofMat::ClayTile) st.roofMat = RoofMat::ClayTile; }
      else if (st.roofMat == RoofMat::Slate || st.roofMat == RoofMat::ClayTile || st.roofMat == RoofMat::Copper || st.roofMat == RoofMat::GlazedTile)
        st.roofMat = cul == CU_SUN || cul == CU_MARSH ? RoofMat::Palm : RoofMat::Thatch;
      // (M3 fixer round 3, review: "Jade thatch roofs are ragged blobs with no ridge") straw is never laid on a pagoda's
      // sweep: a steep gable with a capped ridge
      if (st.roof == RoofShape::Pagoda && (st.roofMat == RoofMat::Thatch || st.roofMat == RoofMat::Palm)) { st.roof = RoofShape::Gable; st.pitch = std::max<uint8_t>(st.pitch, 3); }
      if (st.roof == RoofShape::Dome || st.roof == RoofShape::Onion || st.roof == RoofShape::Mansard || st.roof == RoofShape::Stepped)
        st.roof = st.roofMat == RoofMat::Adobe ? RoofShape::FlatParapet : RoofShape::Gable;
      break;
    default: break;
  }
}

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
    case RoofMat::Copper: r = tint ? ramp(mix(tint, rgba(82, 152, 130), 0.5f)) : kCopper; break;
    case RoofMat::Palm: r = kPalm; break;
    case RoofMat::Bark: r = kBarkRoof; break;
    case RoofMat::Felt: r = tint ? ramp(tint, 0.8f) : kFelt; break;
    case RoofMat::GlazedTile: r = tint ? ramp(tint) : kJadeTile; break;
    case RoofMat::Leaf: r = tint ? ramp(tint) : kLeafRoof; break;
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
    case WallMat::Rubble: return WallKind::Rubble;
    case WallMat::Plank: return WallKind::Planks;
    case WallMat::Wattle: return WallKind::Wattle;
    case WallMat::Felt: return WallKind::Felt;
    case WallMat::Ashlar: return WallKind::Ashlar;
    case WallMat::Living: return WallKind::Living;
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
    case WallKind::Planks: return tint ? ramp(tint, 0.9f) : kWood;   // (barns set kBarnRed themselves)
    case WallKind::Rubble: return tint ? ramp(mix(tint, rgba(136, 124, 112), 0.5f)) : kRubble;
    case WallKind::Wattle: return tint ? ramp(tint, 0.8f) : kDaub;
    case WallKind::Felt: return tint ? ramp(tint, 0.7f) : kFelt;
    case WallKind::Ashlar: return tint ? ramp(tint, 0.75f) : kAshlar;
    case WallKind::Living: return tint ? ramp(tint) : kLiving;
  }
  return kPlaster;
}
// the trim (window frames, sills, lintels, door frames) that goes with a wall
Ramp trimRampFor(WallKind w) {
  switch (w) {
    case WallKind::Stone: case WallKind::Adobe: case WallKind::Plaster: case WallKind::Brick: return kStoneWarm;
    case WallKind::Log: case WallKind::Planks: return kWoodDark;
    case WallKind::Rubble: return kStoneWarm;
    case WallKind::Wattle: return kBeam;
    case WallKind::Felt: return kWood;
    case WallKind::Ashlar: return kAshlar;
    case WallKind::Living: return kBark;
    default: return kBeam;
  }
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
  if (m.shape == RoofShape::Turf) m.slope = p.st.culture ? std::min(std::max(m.slope, slopeFor(2)), slopeFor(3)) : std::min(m.slope, slopeFor(1));
  m.wall = wallKindFor(p.st.wall);
  m.rR = roofRampFor(p.st.roofMat, p.st.roofTint, p.seed, p.st.weather);
  m.wR = wallRampFor(m.wall, p.st.wallTint, p.seed >> 3);
  m.tR = (m.wall == WallKind::Stone || m.wall == WallKind::Adobe) ? kStoneWarm : kBeam;
  if (m.wall == WallKind::Plaster || m.wall == WallKind::Brick) m.tR = kStoneWarm;
  if (m.wall == WallKind::Log) m.tR = kWoodDark;
  if ((int)m.wall >= (int)WallKind::Planks) m.tR = trimRampFor(m.wall);
  if (p.st.trimTint) m.tR = ramp(opaque(p.st.trimTint));
  m.snow = p.st.snow;
  m.moss = p.st.weather;
  m.seed = p.seed;
  m.cul = p.st.culture;
  m.orn = p.st.ornament;
  m.win = (uint8_t)p.st.window;
  m.door = (uint8_t)p.st.door;
  m.accent = p.st.accentTint;
  m.alt = p.st.altTint;
  if (m.shape == RoofShape::Turf) m.ov = 2;
  if (m.shape == RoofShape::Pagoda) { m.ov = 5; m.ovF = 4; }
  if (m.shape == RoofShape::Sweep) { m.ov = 4; m.ovF = 3; }
  if (m.shape == RoofShape::Mansard) { m.ov = 2; m.ovF = 2; }
  if (m.shape == RoofShape::Stepped) { m.ov = 1; m.ovF = 1; }
  if (m.shape == RoofShape::FlatParapet || m.shape == RoofShape::Dome || m.shape == RoofShape::Onion) { m.ov = 0; m.ovF = 0; }
  if (m.rmat == RoofMat::Thatch || m.rmat == RoofMat::Palm) { m.ov = 4; m.ovF = 3; }   // thick thatch hangs low
  if (m.ov > 0 || m.ovF > 0) { m.ov += p.st.eave; m.ovF += p.st.eave; }                 // M3: the culture's deep eaves
  if (flatShape(m.shape) && (p.st.ornament & ORN_CRENELS)) m.crenel = true;               // M3: crenellated parapets
  return m;
}

void finishRoofHeight(Mass& m) {
  if (m.round) {
    if (m.shape == RoofShape::Dome) m.roofH = m.r * 0.75f;
    else if (m.shape == RoofShape::Onion) m.roofH = (m.r + 1.5f) * 1.7f;
    else if (m.rmat == RoofMat::Felt && m.shape == RoofShape::Conical) m.roofH = (m.r + m.ov) * 0.62f;   // a yurt's low cone
    else if (m.shape == RoofShape::Conical || m.shape == RoofShape::Steep || m.shape == RoofShape::Pagoda || m.shape == RoofShape::Sweep) m.roofH = (m.r + m.ov) * std::max(1.3f, m.slope * 1.4f);
    return;
  }
  float span = m.alongY ? (m.x1 - m.x0 + 2 * m.ov) : (m.y1 - m.y0 + m.ov + m.ovF);
  if (m.shape == RoofShape::FlatParapet || m.found) m.roofH = 0;
  else if (m.shape == RoofShape::Dome) m.roofH = (std::min(m.x1 - m.x0, m.y1 - m.y0) * 0.5f - 7) * 0.8f;
  else if (m.shape == RoofShape::Onion) m.roofH = (std::min(m.x1 - m.x0, m.y1 - m.y0) * 0.5f - 6) * 1.6f;
  else if (m.shape == RoofShape::Stepped) m.roofH = span * 0.5f * std::max(0.7f, m.slope);
  else m.roofH = span * 0.5f * m.slope;
}

// ---------------------------------------------------------------- M3: the culture's idiom for each type
// the round tower roofs of a culture (keeps, palaces, mage towers): slender blue slate spires for the high elves, onion
// bulbs on the river and in the dunes, tiered pagoda caps on the jade terraces, leaf cones for the wood elves, flat
// fighting tops where the culture builds low and hard
void cultureTowerRoof(const Plan& p, Mass& t) {
  const ArchStyle& st = p.st;
  t.crenel = false;
  t.ov = 2;
  switch (p.cul) {
    case CU_DUNE: t.shape = RoofShape::Onion; t.rmat = RoofMat::GlazedTile; t.rR = st.roofTint ? ramp(st.roofTint) : ramp(rgba(62, 150, 168)); break;
    case CU_RIVER: t.shape = RoofShape::Onion; t.rmat = RoofMat::Slate; t.rR = kRoofSlate; break;
    case CU_STAR: t.shape = RoofShape::Conical; t.rmat = RoofMat::Slate; t.rR = st.roofTint ? ramp(st.roofTint) : ramp(rgba(92, 116, 178)); t.slope = 2.2f; break;
    case CU_JADE: t.shape = RoofShape::Pagoda; t.rmat = RoofMat::GlazedTile; t.rR = roofRampFor(RoofMat::GlazedTile, st.roofTint, p.seed, 0); t.ov = 4; break;
    case CU_SYLVAN: t.shape = RoofShape::Sweep; t.rmat = RoofMat::Leaf; t.rR = roofRampFor(RoofMat::Leaf, st.roofTint, p.seed, 0); t.ov = 3; break;
    case CU_IMPERIAL: t.shape = RoofShape::Conical; t.rmat = RoofMat::ClayTile; t.rR = roofRampFor(RoofMat::ClayTile, st.roofTint, p.seed, 0); t.slope = 0.9f; break;
    case CU_FJORD: t.shape = RoofShape::Conical; t.rmat = RoofMat::Shingle; t.rR = kRoofBrown; t.slope = 1.6f; break;
    case CU_MARSH: t.shape = RoofShape::Conical; t.rmat = RoofMat::Thatch; t.rR = kThatch; t.ov = 3; break;
    case CU_HIGHLAND: case CU_STEPPE: case CU_SUN: t.shape = RoofShape::FlatParapet; t.crenel = true; t.ov = 0; break;
    default: t.shape = RoofShape::Conical; t.rmat = RoofMat::Slate; t.rR = kRoofSlate; break;
  }
}

// the culture's monumental material on every mass of a temple, keep, palace or barracks (their painters force stone),
// the roofs of keeps and palaces in the culture's shape, and the round towers' roofs
void cultureMasses(Plan& p) {
  ArchStyle& st = p.st;
  std::vector<Mass>& ms = p.sc.ms;
  const Building b = p.b;
  const WallKind wk = wallKindFor(st.wall);
  const Ramp wR = wallRampFor(wk, st.wallTint, p.seed >> 3);
  const Ramp tR = st.trimTint ? ramp(opaque(st.trimTint)) : trimRampFor(wk);
  const bool mon = b == Building::Keep || b == Building::Palace || b == Building::Temple || b == Building::Barracks ||
                   (b == Building::Tower && p.trade != Building::Windmill);
  if (mon)
    for (Mass& m : ms) {
      if (m.chimney || m.found) continue;
      m.wall = wk; m.wR = wR; m.tR = tR;
    }
  const bool pitched = !flatShape(st.roof) && st.roof != RoofShape::Stepped;
  auto styleRoof = [&](Mass& m, bool hall) {
    if (!pitched) {
      if (p.cul == CU_SUN && hall) { m.shape = RoofShape::Stepped; m.rmat = RoofMat::Adobe; m.rR = wR; m.slope = 0.9f; }
      else if (p.cul == CU_DUNE && hall) { m.shape = RoofShape::Onion; m.rmat = RoofMat::GlazedTile; m.rR = st.roofTint ? ramp(st.roofTint) : ramp(rgba(62, 150, 168)); }
      else { m.shape = RoofShape::FlatParapet; m.crenel = true; }
      return;
    }
    const RoofShape s = st.roof;
    m.shape = (s == RoofShape::Pagoda || s == RoofShape::Mansard || s == RoofShape::Sweep || s == RoofShape::Steep || s == RoofShape::Gable) ? s : RoofShape::Hip;
    m.rmat = st.roofMat;
    m.rR = roofRampFor(st.roofMat, st.roofTint, p.seed, st.weather);
    if (m.shape == RoofShape::Pagoda) { m.ov = 5 + st.eave; m.ovF = 4 + st.eave; }
    if (m.shape == RoofShape::Sweep) { m.ov = 4 + st.eave; m.ovF = 3 + st.eave; }
  };
  if (b == Building::Keep && ms.size() >= 6) {
    if (pitched) styleRoof(ms[1], false);
    else if (p.cul == CU_DUNE) { ms[1].shape = RoofShape::Onion; ms[1].rmat = RoofMat::GlazedTile; ms[1].rR = st.roofTint ? ramp(st.roofTint) : ramp(rgba(62, 150, 168)); }
    for (size_t i = 2; i < 6; i++) cultureTowerRoof(p, ms[i]);
  } else if (b == Building::Palace && ms.size() >= 8) {
    styleRoof(ms[0], true);
    for (int k = 1; k <= 2; k++) {
      if (pitched) { styleRoof(ms[(size_t)k], false); ms[(size_t)k].alongY = ms[(size_t)k].shape != RoofShape::Pagoda && ms[(size_t)k].shape != RoofShape::Sweep; }
      else { ms[(size_t)k].shape = RoofShape::FlatParapet; ms[(size_t)k].crenel = true; }
    }
    Mass& et = ms[(size_t)p.doorMass];
    if (pitched) { styleRoof(et, false); if (et.shape == RoofShape::Hip || et.shape == RoofShape::Steep) et.slope = 1.9f; }
    for (size_t i = 4; i < ms.size(); i++) if (ms[i].round) cultureTowerRoof(p, ms[i]);
    // (M3 fixer round 2) the sun-temple palace's great hall steps up in crisp blocks (a flat-topped hall, a smaller
    // tier on it and a shrine room on top), not a terraced roof texture that read as soft blurred slabs
    if (p.cul == CU_SUN) {
      Mass& hall = ms[0];
      hall.shape = RoofShape::FlatParapet; hall.crenel = false; hall.ov = 0; hall.ovF = 0; hall.roofH = 0;
      const Mass h0 = hall;
      float ix = std::floor((h0.x1 - h0.x0) * 0.16f), iy = std::floor((h0.y1 - h0.y0) * 0.18f);
      Mass t1 = h0;
      t1.x0 = h0.x0 + ix; t1.x1 = h0.x1 - ix; t1.y0 = h0.y0 + 2; t1.y1 = h0.y1 - iy;
      t1.zBase = h0.zBase + h0.wallH; t1.wallH = 12; t1.body = false; t1.floorV = 0; t1.tier = true;
      ms.push_back(t1);
      Mass t2 = t1;
      const float jx = std::floor((t1.x1 - t1.x0) * 0.25f);
      t2.x0 = t1.x0 + jx; t2.x1 = t1.x1 - jx; t2.y0 = t1.y0 + 2; t2.y1 = t1.y1 - std::floor((t1.y1 - t1.y0) * 0.3f);
      t2.zBase = t1.zBase + t1.wallH; t2.wallH = 12;
      ms.push_back(t2);
    }
  } else if (b == Building::Tower && p.trade != Building::Windmill) {
    for (Mass& m : ms) if (m.round) { cultureTowerRoof(p, m); if (m.shape == RoofShape::FlatParapet) { m.shape = RoofShape::Conical; m.rmat = RoofMat::Slate; m.rR = kRoofPurple; } m.slope = std::max(m.slope, 1.5f); }
  } else if (b == Building::Barracks) {
    for (Mass& m : ms) if (!m.chimney && !m.crenel && pitched) styleRoof(m, false);
  }
  // (M3 fixer round 3, review: "the sun-temple capital's palace, keep and barracks are the European castle kit,
  // recoloured: grey stone, crenels, a grey flagged roof") the sun temples' seats of power are lime-plastered masonry
  // painted warm, flat-roofed behind a crest of stepped merlons (almenas), their roof terraces trowelled like the
  // houses', never grey castle stone
  if (p.cul == CU_SUN && (b == Building::Keep || b == Building::Palace || b == Building::Barracks)) {
    const uint32_t lime = st.wallTint ? mix(opaque(st.wallTint), rgba(232, 204, 158), 0.55f) : rgba(232, 204, 158);
    const Ramp sR = wallRampFor(WallKind::Adobe, lime, p.seed >> 3);
    for (Mass& m : ms) {
      if (m.chimney || m.found) continue;
      m.wall = WallKind::Adobe; m.wR = sR;
      m.tR = st.accentTint ? ramp(opaque(st.accentTint)) : kLacquer;
      if (m.shape == RoofShape::FlatParapet || m.shape == RoofShape::Stepped || (!pitched && !m.round && m.shape != RoofShape::Dome)) {
        m.shape = RoofShape::FlatParapet; m.rmat = RoofMat::Adobe; m.crenel = true; m.ov = 0; m.ovF = 0;
      }
    }
  }
  // houses: the culture's facade rhythm and porch from ArchStyle::variant (mixed with the seed, so a street of one
  // culture never repeats a facade even where the generator leaves variant at 0)
  const uint32_t v = (uint32_t)st.variant * 2654435761u ^ hash3((int)p.seed, 77, 0xFACADEu);
  // (M3 fixer round 3, review: "Starspire capital houses are identical clones in a grid: dome on a square, one window")
  // the high elves' square houses take one of four massings: the bulb on its drum in the middle, a steep blue hip with a
  // needle turret at one front corner, a flat white terrace with a slender corner tower, or the bulb set to one side
  // over a lower wing; their caps in one of three glazes (night blue, violet, verdigris)
  if (p.cul == CU_STAR && (b == Building::House || b == Building::StoneHouse || b == Building::Shop) && !ms.empty() && !ms[0].round &&
      flatShape(ms[0].shape)) {
    static const uint32_t glaze[3] = {rgba(70, 92, 160), rgba(104, 80, 150), rgba(70, 140, 128)};
    const Ramp gR = ramp(st.roofTint && ((v >> 12) % 3) == 0 ? opaque(st.roofTint) : glaze[(v >> 14) % 3]);
    Mass& m0 = ms[0];
    const int kind = (int)((v >> 10) & 3);
    const bool east = ((v >> 16) & 1) != 0;
    auto turret = [&](float cx, int extraH, RoofShape cap) {
      Mass t = baseMass(p, 0, 0, 0, 0, m0.wallH + extraH);
      t.round = true; t.r = 7.5f; t.cx = cx; t.cy = m0.y1 - 5.0f;   // proud of the front wall
      t.wall = m0.wall; t.wR = m0.wR; t.tR = m0.tR;
      t.shape = cap; t.rmat = RoofMat::Slate; t.rR = gR; t.slope = 1.9f; t.ov = 1; t.ovF = 1; t.crenel = false;
      ms.push_back(t);
    };
    if (kind == 1) {   // steep hip and a needle turret
      m0.shape = RoofShape::Hip; m0.rmat = RoofMat::Slate; m0.rR = gR; m0.slope = std::max(m0.slope, 1.6f); m0.ov = 2 + st.eave; m0.ovF = 2 + st.eave;
      m0.crenel = false; m0.wallH = std::max(16, m0.wallH - 4);
      turret(east ? m0.x1 - 5.5f : m0.x0 + 5.5f, 14, RoofShape::Onion);
    } else if (kind == 2) {   // a flat terrace and a slender corner tower under a spire
      m0.shape = RoofShape::FlatParapet; m0.crenel = false;
      turret(east ? m0.x1 - 5.5f : m0.x0 + 5.5f, 16, RoofShape::Onion);
    } else if (kind == 3 && m0.x1 - m0.x0 >= 44) {   // the bulb over a taller block to one side, a lower flat wing
      const float split = std::floor(m0.x0 + (m0.x1 - m0.x0) * 0.55f);
      Mass wg = m0;
      wg.shape = RoofShape::FlatParapet; wg.crenel = false; wg.wallH = std::max(16, m0.wallH - 10); wg.body = false;
      if (east) { wg.x1 = m0.x1; wg.x0 = split; m0.x1 = split; } else { wg.x0 = m0.x0; wg.x1 = m0.x1 - (split - m0.x0); m0.x0 = wg.x1; }
      wg.y0 = m0.y0 + 6;
      ms.push_back(wg);
      ms[0].rR = gR;
      if (p.doorX < ms[0].x0 + 6 || p.doorX > ms[0].x1 - 6) p.doorMass = (int)ms.size() - 1;   // the door in the wing
    } else {
      m0.rR = gR;
    }
  }
  static const int steps[4] = {16, 14, 18, 20};
  p.winStep = steps[v & 3];
  p.winOff = (int)((v >> 2) & 3) * 2 - 3;
  p.doorFrame = (int)((v >> 4) & 3);
  const bool porchy = (st.ornament & ORN_PORCH_COLUMNS) != 0;
  p.porch = porchy && (b == Building::House || b == Building::StoneHouse || b == Building::Inn || b == Building::Temple || b == Building::Shop) &&
            ((v >> 6) & 3) >= 2 && !st.awnings;   // (M3 fixer: one house in two, not three in four: a capital's street of
                                                   // porticoes read as one house repeated)
  if (b == Building::Temple && porchy) p.porch = true;
  if (st.accentTint || st.altTint) {
    const uint32_t opts[3] = {st.accentTint ? st.accentTint : st.altTint, st.altTint ? st.altTint : st.accentTint, rgba(70, 62, 56)};
    p.shutterCol = opts[(v >> 8) % 3];
  }
}

// a temple in the culture's idiom; false: the classic nave and steeple (or the desert dome) in the culture's materials
bool cultureTemple(Plan& p) {
  ArchStyle& st = p.st;
  std::vector<Mass>& ms = p.sc.ms;
  const int W = p.W, D = p.D;
  auto H = [&](uint32_t k, int n) { return (int)(hash3((int)k, 23, p.seed) % (uint32_t)n); };
  switch (p.cul) {
    case CU_IMPERIAL: {   // a rotunda: a drum of ashlar under a great dome, a columned portico before it (overlays)
      Mass h = baseMass(p, 0, (float)W, 0, (float)D - 8, 30);
      h.shape = RoofShape::Dome;
      h.rmat = RoofMat::Copper; h.rR = st.roofTint ? ramp(mix(st.roofTint, rgba(82, 152, 130), 0.5f)) : kCopper;
      ms.push_back(h);
      Mass pr = baseMass(p, (float)p.doorX - 22, (float)p.doorX + 22, (float)D - 14, (float)D, 26);   // the portico's pediment block
      pr.shape = RoofShape::Gable; pr.alongY = true; pr.slope = 0.5f;
      pr.rmat = RoofMat::ClayTile; pr.rR = roofRampFor(RoofMat::ClayTile, st.roofTint, p.seed, 0);
      ms.push_back(pr);
      p.doorMass = 1;
      p.porch = true;
      return true;
    }
    case CU_DUNE: {   // a domed prayer hall and a slender minaret with an onion cap at one front corner
      Mass h = baseMass(p, 0, (float)W, 0, (float)D, 30);
      h.shape = RoofShape::Dome;
      h.rmat = RoofMat::GlazedTile; h.rR = st.roofTint ? ramp(st.roofTint) : ramp(rgba(62, 150, 168));
      ms.push_back(h);
      p.doorMass = 0;
      Mass mi = baseMass(p, 0, 0, 0, 0, 64);
      mi.round = true;
      mi.r = 6;
      const bool east = H(1, 2) == 0;
      mi.cx = east ? W - 7.0f : 7.0f;
      mi.cy = D - 7.0f;
      mi.shape = RoofShape::Onion; mi.rmat = RoofMat::GlazedTile; mi.rR = h.rR;
      ms.push_back(mi);
      return true;
    }
    case CU_STEPPE: {   // the great yurt: a wide felt drum under a low cone with its crown ring
      Mass y = baseMass(p, 0, (float)W, 0, (float)D, 24);
      y.round = true;
      y.r = std::min((float)W, (float)D + 10) * 0.5f - 1;
      y.cx = W * 0.5f;
      y.cy = D - y.r - 0.5f;
      y.shape = RoofShape::Conical; y.rmat = RoofMat::Felt; y.rR = roofRampFor(RoofMat::Felt, st.roofTint, p.seed, 0);
      y.wall = WallKind::Felt; y.wR = wallRampFor(WallKind::Felt, st.wallTint, p.seed); y.tR = kWood;
      ms.push_back(y);
      p.doorMass = 0;
      return true;
    }
    case CU_JADE: {   // a pagoda: two or three tiers of glazed roofs with upswept corners, each tier set in
      const int tiers = W >= 80 ? 3 : 2;
      float x0 = 2, x1 = (float)W - 2, y0 = 4, y1 = (float)D;
      int zb = p.zBase;
      for (int k = 0; k < tiers; k++) {
        Mass t = baseMass(p, x0, x1, y0, y1, k == 0 ? 24 : 15);
        t.zBase = zb;
        t.shape = RoofShape::Pagoda;
        t.alongY = false;
        t.slope = 0.62f;
        t.tier = k > 0;
        if (k > 0) { t.ov = 3; t.ovF = 3; }
        finishRoofHeight(t);
        ms.push_back(t);
        x0 += 12; x1 -= 12; y0 += 6; y1 -= 10;
        // the next tier stands on this roof: its front wall's foot where it meets the slope
        Surf s;
        zb = (int)std::floor(massTop(t, (x0 + x1) * 0.5f, y1 - 0.5f, s)) - 2;
        if (x1 - x0 < 20 || y1 - y0 < 10) break;
      }
      p.doorMass = 0;
      return true;
    }
    case CU_SUN: {   // a stepped pyramid: a doorway into its foot, the great stair up its front, a shrine on top
      Mass py = baseMass(p, 0, (float)W, 0, (float)D, 16);
      py.shape = RoofShape::Stepped;
      py.rmat = RoofMat::Adobe; py.rR = ramp(mix(py.wR[2], rgba(186, 150, 104), 0.55f));
      py.slope = 1.1f;
      py.ov = 0; py.ovF = 0;
      py.stairX0 = (float)p.doorX - 7; py.stairX1 = (float)p.doorX + 7;
      finishRoofHeight(py);
      ms.push_back(py);
      p.doorMass = 0;
      const float sw = std::max(18.0f, W * 0.34f), sd = std::max(10.0f, D * 0.3f);
      Mass sh = baseMass(p, W * 0.5f - sw * 0.5f, W * 0.5f + sw * 0.5f, D * 0.5f - sd * 0.5f - 2, D * 0.5f + sd * 0.5f - 2, 12);
      sh.zBase = (int)std::floor(py.zTop() - 1 + py.roofH);
      sh.shape = RoofShape::FlatParapet; sh.crenel = true;
      sh.tier = true;
      ms.push_back(sh);
      return true;
    }
    case CU_SYLVAN: {   // a tree shrine: a round hall of living wood under a leaf cone, the sacred tree rising through it
      Mass h = baseMass(p, 0, (float)W, 0, (float)D, 26);
      h.round = true;
      h.r = std::min((float)W, (float)D + 8) * 0.5f - 2;
      h.cx = W * 0.5f;
      h.cy = D - h.r - 0.5f;
      h.shape = RoofShape::Sweep;
      h.rmat = RoofMat::Leaf; h.rR = roofRampFor(RoofMat::Leaf, st.roofTint, p.seed, 0);
      h.slope = 0.8f;
      ms.push_back(h);
      p.doorMass = 0;
      return true;
    }
    default: return false;   // the classic nave and steeple, in the culture's materials (cultureMasses) and windows
  }
}

// ---------------------------------------------------------------- M0b: the rise budget
// The world generator keeps every building's sprite clear of the fronts and doorsteps of its neighbours by assuming it
// rises at most this many px above the footprint's top edge. This MIRRORS world.h bldgRiseTiles(type, storeys) * 16 (a
// generator constant the art cannot include: rpg_art does not depend on rpg_sim); arch_gallery --check holds the two
// together on real worlds.
int riseBudgetPx(Building b, int storeys) {
  if (b == Building::Windmill) return 6 * 16;   // (M1 fixer) mirrors world.h bldgRiseTiles: the tall tower mill
  b = artBase(b);   // M1 economy: the production buildings rise like their frames (the windmill like a tower)
  int r = 3;
  if (b == Building::Tower || b == Building::Temple) r = 5;
  else if (b == Building::Keep || b == Building::Inn || b == Building::Barracks) r = 4;
  else if (b == Building::Palace) r = 7;   // M1: rpg/world/town_rules.h townRiseTiles
  if (storeys >= 2 && (b == Building::House || b == Building::StoneHouse || b == Building::Shop || b == Building::Farmhouse)) r = std::max(r, 4);
  return r * 16;
}

// how far a mass's top rises above the footprint's top edge (y = 0), px: the most of z - y over its roof. Sampled on a
// few columns across the roof (the ridge, hips, corners), every half pixel down the depth: cheap enough for
// buildingHeight, which the terrain calls per building.
float massRise(const Mass& m) {
  if (m.chimney) return m.zTop() + 1 - m.y0;
  float rx0, rx1, ry0, ry1;
  if (m.round) {
    float R = m.r + (float)std::max(m.ov, 1) + 1;
    rx0 = m.cx - R; rx1 = m.cx + R; ry0 = m.cy - R; ry1 = m.cy + R;
  } else {
    rx0 = m.x0 - m.ov; rx1 = m.x1 + m.ov; ry0 = m.y0 - m.ov - 1; ry1 = m.y1 + m.ovF + 1;
  }
  float best = m.zTop() + 6 - m.y1;   // the front wall's top (and a parapet's merlons)
  const int NX = 13;
  for (int i = 0; i < NX; i++) {
    float x = rx0 + 0.5f + (rx1 - rx0 - 1) * (float)i / (NX - 1);
    if (i == NX / 2) x = m.round ? m.cx : (m.x0 + m.x1) * 0.5f;
    for (float y = ry0; y <= ry1; y += 0.5f) {
      Surf s;
      float z = massTop(m, x, y, s);
      if (z >= 0) best = std::max(best, z - y);
    }
  }
  return best;
}

// lower a mass until it fits under lim: a north-south ridge is first hipped at the back (the street still sees its
// gable), then the roof is made flatter, then (last resort) the walls lower
void fitMass(Mass& m, float lim, int minWall) {
  if (m.chimney || massRise(m) <= lim) return;
  const bool pitched = !flatShape(m.shape);
  if (!m.round && m.alongY && pitched && m.roofH > 2) {
    const float ry0 = m.y0 - m.ov, dmax = (m.x1 - m.x0 + 2 * m.ov) * 0.5f;
    float lo = ry0 + 1, hi = m.y1 - 8;   // where the ridge begins; the front keeps at least 8 px of ridge
    if (hi > lo) {
      auto at = [&](float yh) { Mass t = m; t.backHip = dmax / std::max(1.0f, yh - ry0); return massRise(t); };
      if (at(hi) <= lim) {
        for (int it = 0; it < 12; it++) { float mid = (lo + hi) * 0.5f; if (at(mid) <= lim) hi = mid; else lo = mid; }
      }
      m.backHip = dmax / std::max(1.0f, hi - ry0);
    }
  }
  for (int it = 0; it < 6 && pitched && m.roofH > 3; it++) {
    float over = massRise(m) - lim;
    if (over <= 0) break;
    m.roofH = std::max(3.0f, m.roofH - over - 0.5f);
  }
  for (int it = 0; it < 4; it++) {
    float over = massRise(m) - lim;
    if (over <= 0 || m.wallH <= minWall) break;
    m.wallH = std::max(minWall, m.wallH - (int)std::ceil(over));
  }
}

Plan makePlanMasses(Building b, int wT, int hT, const ArchStyle& st0, uint32_t seed, const BuildingFacts& facts) {
  Plan p;
  const Building trade = b;
  b = artBase(b);
  p.b = b;
  p.trade = trade;
  p.facts = facts;
  if (p.facts.storeys <= 0) p.facts.storeys = defaultStoreys(b);
  // M0b: the storeys each type can show. Inns and keeps always 2, towers 3; houses, stone houses and shops 1 or 2 (the
  // generator's word); the rest are single halls (smithy, temple, barn, hut) whatever the facts say.
  switch (b) {
    case Building::House: case Building::StoneHouse: case Building::Shop: p.facts.storeys = std::clamp(p.facts.storeys, 1, 2); break;
    case Building::Inn: case Building::Keep: case Building::Palace: case Building::Barracks: p.facts.storeys = 2; break;
    case Building::Tower: p.facts.storeys = trade == Building::Windmill ? 2 : 3; break;
    default: p.facts.storeys = 1; break;
  }
  const int storeys = p.facts.storeys;
  p.seed = seed;
  p.st = st0;
  p.W = std::max(2, wT) * 16;
  p.D = std::max(1, hT) * 16;
  p.doorX = (std::max(2, wT) / 2) * 16 + 8;
  ArchStyle& st = p.st;
  const int W = p.W, D = p.D;
  // the building type only adds function; a few types also pick sturdier materials
  p.cul = st.culture;
  if (p.cul != CU_NONE) cultureTypeRules(p, b);
  else switch (b) {
    case Building::StoneHouse:
      if (st.wall == WallMat::Timber || st.wall == WallMat::Log || st.wall == WallMat::Plaster) st.wall = (seed & 8) ? WallMat::Brick : WallMat::Stone;
      break;
    case Building::Temple: case Building::Keep: case Building::Tower: case Building::Palace:
      if (st.wall != WallMat::Adobe) st.wall = WallMat::Stone;
      if (st.roofMat == RoofMat::Thatch || st.roofMat == RoofMat::Turf) st.roofMat = RoofMat::Slate;
      if (b == Building::Palace && st.roofMat == RoofMat::Shingle) st.roofMat = RoofMat::Slate;   // a king roofs in slate
      if (st.roof == RoofShape::Turf || st.roof == RoofShape::Conical) st.roof = RoofShape::Hip;
      break;
    case Building::Barracks:   // the garrison builds in stone, under slate or the region's tiles
      if (st.wall != WallMat::Adobe && st.wall != WallMat::Brick) st.wall = WallMat::Stone;
      if (st.roofMat == RoofMat::Thatch || st.roofMat == RoofMat::Turf) st.roofMat = RoofMat::Slate;
      if (st.roof == RoofShape::Turf || st.roof == RoofShape::Conical) st.roof = RoofShape::Hip;
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
  if (st.roofMat == RoofMat::Adobe && st.roof != RoofShape::Dome && st.roof != RoofShape::Onion && st.roof != RoofShape::Stepped) st.roof = RoofShape::FlatParapet;
  const bool monument = b == Building::Keep || b == Building::Temple || b == Building::Tower || b == Building::Palace;
  // (M3 fixer round 2) the sun temples' homes: a "stepped" roof on an ordinary house painted as soft terraced slabs
  // with no edge or form. Their houses are flat-roofed lime-plastered blocks behind a parapet; the stepped form comes
  // from a set-back upper room on most of them (below), not from a terraced roof texture
  const bool sunHome = p.cul == CU_SUN && !monument && b != Building::Barracks && st.roof == RoofShape::Stepped;
  if (sunHome) { st.roof = RoofShape::FlatParapet; st.roofMat = RoofMat::Adobe; }
  if (p.cul == CU_SUN && (b == Building::Palace || b == Building::Keep)) st.ornament = (uint16_t)(st.ornament & ~ORN_VINES);   // kept clean, painted
  p.stilts = (st.stilts || st.foundation == Foundation::Stilts) && !monument;
  p.zBase = p.stilts ? 7 : 0;
  // M3 foundations: a stone plinth or terrace a few px high, a raised platform (dais); stilts as before
  if (!p.stilts && st.foundation != Foundation::None && st.foundation != Foundation::Stilts && b != Building::Tower) {
    p.fnd = st.foundation;
    p.zBase = st.foundation == Foundation::Platform ? (monument ? 8 : 5) : (st.foundation == Foundation::Terrace ? 4 : 3);
  }
  auto H = [&](uint32_t k, int n) { return (int)(hash3((int)k, 17, seed) % (uint32_t)n); };
  int wallH = 27;
  switch (b) {
    case Building::Inn: wallH = 40; break;
    case Building::Hut: wallH = 20; break;
    case Building::Farmhouse: wallH = 28; break;
    case Building::Temple: wallH = 30; break;
    case Building::Shop: wallH = 33; break;   // a tall shop front: the awning hangs below the windows line, not under the eave
    default: break;
  }
  // M0b: a second storey: the walls rise to the inn's height (a floor line at mid-height, a row of windows above it)
  if (storeys >= 2 && (b == Building::House || b == Building::StoneHouse || b == Building::Barracks)) wallH = 40;
  if (storeys >= 2 && b == Building::Shop) wallH = 41;
  if (flatShape(st.roof)) wallH += 4;   // the parapet rises above the roof deck
  // M3: the culture's wall height (0.8x .. 1.3x), never below what the storeys need
  if (st.wallH) {
    const float k = 0.8f + st.wallH / 255.0f * 0.5f;
    wallH = std::max(storeys >= 2 ? 36 : 16, (int)std::lround(wallH * k));
  }
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
    Mass dj = baseMass(p, W * 0.5f - dw * 0.5f, W * 0.5f + dw * 0.5f, 4, 4 + dd, 50);
    dj.wall = WallKind::Stone; dj.wR = kStone; dj.tR = kStoneWarm;
    if (dj.shape == RoofShape::FlatParapet) dj.crenel = true;
    else { dj.shape = RoofShape::Hip; dj.slope = std::max(dj.slope, 1.0f); }
    ms.push_back(dj);
    // corner towers: real drum towers that stand proud of the curtain wall (the front pair projects into the sprite's
    // pads, so their stone bodies show in full beside and in front of the wall face), the back pair taller so its
    // bodies rise clear above the front cones. Slate cones: a stone keep never wears thatch.
    for (int k = 0; k < 4; k++) {
      const bool front = k < 2;
      Mass t = baseMass(p, 0, 0, 0, 0, front ? 50 : 58);
      t.round = true;
      t.r = front ? 11.0f : 10.0f;
      t.cx = (k & 1) ? W - (front ? 5.0f : 7.0f) : (front ? 5.0f : 7.0f);
      t.cy = front ? D - 7.0f : 9.0f;
      t.wall = WallKind::Stone; t.wR = kStone; t.tR = kStoneWarm;
      bool cone = !flatShape(st.roof);
      t.shape = cone ? RoofShape::Conical : RoofShape::FlatParapet;
      t.crenel = !cone;
      if (cone) { t.rmat = RoofMat::Slate; t.rR = kRoofSlate; }
      t.ov = 2;
      t.slope = 1.2f;
      if (p.cul == CU_SUN) {   // (M3 fixer round 3) square stepped towers, like the sun palace's, not drums
        const float hs = std::floor(t.r) - 1;
        t.round = false;
        t.x0 = std::max(0.0f, t.cx - hs); t.x1 = std::min((float)W, t.cx + hs);
        t.y0 = std::max(0.0f, t.cy - hs); t.y1 = std::min((float)D, t.cy + hs);
        t.shape = RoofShape::FlatParapet; t.crenel = true; t.ov = 0; t.ovF = 0;
      }
      ms.push_back(t);
    }
    p.doorMass = 0;
  } else if (b == Building::Palace) {
    // M1, VISION_PLAN 15.8: the king's palace. The great hall in the middle (two storeys of tall windows under a high
    // roof, its ridge across the view so the whole long slope shows), the wings either side a little lower and set
    // forward, the entrance tower before the hall with the great door, drum towers on the four corners (the back pair
    // taller, so they rise clear behind the front cones). Stone (adobe in the desert); the roofs in the region's
    // material, slate cones on the towers (copper domes in the desert).
    const bool flat = flatShape(st.roof);
    const WallKind wk = st.wall == WallMat::Adobe ? WallKind::Adobe : WallKind::Stone;
    auto dress = [&](Mass& m) {
      m.wall = wk;
      m.wR = wk == WallKind::Adobe ? wallRampFor(WallKind::Adobe, st.wallTint, seed >> 3) : ((seed & 2) ? kStoneWarm : kStone);
      m.tR = wk == WallKind::Adobe ? kWood : kStoneWarm;
    };
    const float wingW = std::floor(W * 0.27f);
    Mass hall = baseMass(p, wingW - 6, W - wingW + 6, 6, (float)D - 12, 46);
    if (flat) { hall.shape = RoofShape::Dome; hall.rmat = RoofMat::Copper; hall.rR = kCopper; }   // the desert: a great dome
    else { hall.shape = st.roof == RoofShape::Gable || st.roof == RoofShape::Steep ? RoofShape::Gable : RoofShape::Hip; hall.alongY = false; hall.slope = std::max(hall.slope, 1.0f); }
    dress(hall);
    ms.push_back(hall);
    for (int s = 0; s < 2; s++) {
      Mass wg = baseMass(p, s == 0 ? 12.0f : W - wingW - 2, s == 0 ? wingW + 2 : W - 12.0f, 16, (float)D - 4, 36);
      if (flat) { wg.shape = RoofShape::FlatParapet; wg.crenel = true; }
      else { wg.shape = RoofShape::Hip; wg.alongY = true; wg.slope = std::max(wg.slope, 0.9f); }
      dress(wg);
      ms.push_back(wg);
    }
    const float tw = 44;
    Mass et = baseMass(p, p.doorX - tw * 0.5f, p.doorX + tw * 0.5f, (float)D - 28, (float)D, 54);
    if (flat) { et.shape = RoofShape::FlatParapet; et.crenel = true; }
    else { et.shape = RoofShape::Hip; et.slope = 1.9f; et.ov = 2; et.ovF = 2; }
    dress(et);
    ms.push_back(et);
    p.doorMass = (int)ms.size() - 1;
    for (int k = 0; k < 4; k++) {
      const bool front = k < 2;
      Mass t = baseMass(p, 0, 0, 0, 0, front ? 56 : 66);
      t.round = true;
      t.r = front ? 12.0f : 11.0f;
      t.cx = (k & 1) ? W - (front ? 6.0f : 9.0f) : (front ? 6.0f : 9.0f);
      t.cy = front ? D - 7.0f : 11.0f;
      dress(t);
      t.shape = flat ? RoofShape::Dome : RoofShape::Conical;
      t.crenel = false;
      if (flat) { t.rmat = RoofMat::Copper; t.rR = kCopper; }
      else { t.rmat = RoofMat::Slate; t.rR = kRoofSlate; }
      t.ov = 2;
      t.slope = 1.25f;
      // (M3 fixer round 2) the sun temples raise square stepped towers, flat-topped behind a plain parapet, not
      // European drum towers (their palace read as a white castle)
      if (p.cul == CU_SUN) {
        const float hs = std::floor(t.r) - 1;
        t.round = false;
        t.x0 = std::max(0.0f, t.cx - hs); t.x1 = std::min((float)W, t.cx + hs);
        t.y0 = std::max(0.0f, t.cy - hs); t.y1 = std::min((float)D, t.cy + hs);
        t.shape = RoofShape::FlatParapet; t.crenel = false; t.ov = 0; t.ovF = 0;
      }
      ms.push_back(t);
    }
  } else if (b == Building::Tower && trade == Building::Windmill) {
    // M1 economy: a tower mill. A round tower of whitewashed rubble (or the region's stone) under a boarded or thatched
    // cap; the sails (overlays) turn on the cap's front. Two storeys: the stone floor with the sacks, the millstone loft.
    // (M1 fixer) it stands taller than the barns round it: a broad drum on a four-tile plot, sails longer than the
    // tower is wide
    Mass t = baseMass(p, 0, 0, 0, 0, 54 + H(1, 6));
    t.round = true;
    t.r = std::min(std::min(W, D) * 0.5f - 4.0f, 17.0f);
    t.cx = std::clamp((float)p.doorX, t.r + 3.0f, W - t.r - 3.0f);   // the door (the footprint's door tile) at its foot
    t.cy = D - t.r - 1;
    t.shape = RoofShape::Conical;
    const bool white = st0.wall != WallMat::Stone && st0.wall != WallMat::Brick && st0.wall != WallMat::Adobe;
    t.wall = white ? WallKind::Plaster : WallKind::Stone;
    t.wR = white ? kPlaster : (st0.wall == WallMat::Adobe ? kAdobe : kStoneWarm);
    t.tR = kWood;
    const bool thatch = st0.roofMat == RoofMat::Thatch || st0.roofMat == RoofMat::Turf;
    t.rmat = thatch ? RoofMat::Thatch : RoofMat::Shingle;
    t.rR = thatch ? kThatch : kRoofBrown;
    t.ov = 2;
    t.slope = 0.95f;
    ms.push_back(t);
    p.doorMass = 0;
  } else if (b == Building::Tower) {
    float r = std::min(W, D) * 0.5f - 1;
    if (W - 2 * r > 20) {   // low annexes either side
      Mass a = baseMass(p, 0, (float)W, D - 26.0f, (float)D, 22);
      a.shape = (flatShape(st.roof)) ? RoofShape::FlatParapet : RoofShape::Hip;
      ms.push_back(a);
    }
    Mass t = baseMass(p, 0, 0, 0, 0, 62 + H(1, 10));
    t.round = true;
    t.r = std::min(r, 20.0f);
    t.cx = W * 0.5f;
    t.cy = D - t.r - 1;
    t.shape = (flatShape(st.roof)) ? RoofShape::Dome : (st.roof == RoofShape::Pagoda ? RoofShape::Pagoda : RoofShape::Conical);
    if (t.shape == RoofShape::Dome && st.roofMat == RoofMat::Adobe) t.rmat = RoofMat::Copper, t.rR = kCopper;
    else if (st.roofMat == RoofMat::Slate || st.roofMat == RoofMat::Shingle) t.rR = kRoofPurple, t.rmat = RoofMat::Slate;
    t.ov = 3;
    t.slope = 1.5f;
    ms.push_back(t);
    p.doorMass = (int)ms.size() - 1;
  } else if (b == Building::Temple && p.cul != CU_NONE && cultureTemple(p)) {
    // M3: the temple in the culture's own idiom (cultureTemple built its masses)
  } else if (b == Building::Temple) {
    bool desert = flatShape(st.roof);
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
      float tw = p.cul == CU_STAR ? 16.0f : 22.0f;   // (M3: the high elves raise a needle)
      Mass t = baseMass(p, p.doorX - tw * 0.5f, p.doorX + tw * 0.5f, D - 24.0f, (float)D, 58);
      t.shape = RoofShape::Hip;
      t.hk = 1.0f;
      t.slope = p.cul == CU_STAR ? 3.4f : 2.3f;   // a tall spire
      t.ov = 2; t.ovF = 2;
      t.wall = WallKind::Stone; t.wR = kStoneWarm; t.tR = kStone;
      ms.push_back(t);
      p.doorMass = 1;
    }
  } else {
    // houses, inns, shops, smithies, barns, huts: a main block plus seeded wings, cross gables, dormers
    float mx0 = 0, mx1 = (float)W;
    int wing = 0;   // -1 west, +1 east
    bool pitched = !flatShape(st.roof);
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
      // the barn follows the local building culture: red board-and-batten in the farmlands, mud brick in the desert,
      // logs in the north and the swamps (a red barn among adobe houses breaks the village palette)
      // (M3: and the culture's own material where it builds in planks, wattle, felt, rubble, ashlar or living wood)
      if (p.cul != CU_NONE && st.wall != WallMat::Timber && st.wall != WallMat::Plaster && st.wall != WallMat::Stone && st.wall != WallMat::Brick &&
          st.wall != WallMat::Adobe && st.wall != WallMat::Log) { /* baseMass dressed it in the culture's wall already */ }
      else if (st.wall == WallMat::Adobe) { m.wall = WallKind::Adobe; m.wR = wallRampFor(WallKind::Adobe, st.wallTint, seed >> 3); m.tR = kWood; }
      else if (st.wall == WallMat::Log) { m.wall = WallKind::Log; m.wR = kLog; m.tR = kWoodDark; }
      else { m.wall = WallKind::Planks; m.wR = kBarnRed; m.tR = kCloth; m.barn = true; }
      if (pitched) { m.shape = RoofShape::Gable; m.alongY = true; m.gambrel = true; m.slope = std::max(m.slope, 1.0f); }
    }
    // narrow-and-deep or seeded: turn the ridge so the gable faces the street
    if (pitched && b != Building::Farmhouse && (m.shape != RoofShape::Turf || p.cul != CU_NONE) && !m.round && (mx1 - mx0) <= D + 24 && H(4, 3) != 0) m.alongY = true;
    if (b == Building::Hut && pitched) m.alongY = (mx1 - mx0) < 52;
    ms.push_back(m);
    p.doorMass = 0;
    if (b == Building::Barracks && W >= 80) {
      // the garrison's watchtower at one end: square, crenellated, a storey above the roofs
      const bool east = (seed >> 5) & 1;
      Mass t = baseMass(p, east ? W - 26.0f : 2.0f, east ? W - 2.0f : 26.0f, 2, (float)D - 2, 58);
      // (M1) the hall stops where the tower stands: its hip used to run on under the tower, so the tower's block
      // seemed to burst up through the hall's roof
      Mass& hall = ms.back();
      if (east) hall.x1 = std::min(hall.x1, W - 26.0f);
      else hall.x0 = std::max(hall.x0, 26.0f);
      t.shape = RoofShape::FlatParapet;
      t.crenel = true;
      t.wall = WallKind::Stone; t.wR = kStone; t.tR = kStoneWarm;
      ms.push_back(t);
    }
    if (wing) {
      float wx0 = wing == 1 ? mx1 : 0, wx1 = wing == 1 ? (float)W : mx0;
      // the smithy's forge bay is a lower, shallower lean-to against the main block (M0 round 3: at nearly the main
      // block's height its roof ran into the main hip, the main roof's shaded east plane cutting across the bay roof
      // with a seam; now the main roof stays whole above it and the bay roof tucks under the main eave)
      int wh = b == Building::Smithy ? wallH - 6 : wallH - 6 - (b == Building::Inn ? 10 : 0);
      // M0b: beside a 2-storey house the wing stays a single storey (the step in the massing reads at a glance)
      if (storeys >= 2 && (b == Building::House || b == Building::StoneHouse)) wh = 27 + (pitched ? 0 : 4);
      Mass wm = baseMass(p, wx0, wx1, b == Building::Smithy ? 10.0f : 6.0f, (float)D, wh);
      wm.alongY = false;
      if (wm.shape == RoofShape::Gable || wm.shape == RoofShape::Steep) wm.shape = RoofShape::Hip;
      if (b == Building::Smithy) { wm.shape = pitched ? RoofShape::Gable : RoofShape::FlatParapet; wm.wall = WallKind::Timber; wm.wR = kBeam; wm.tR = kBeam; }
      // (M3 fixer) a mud-brick people's forge bay is mud brick too (a timber-and-stone bay stood against the adobe house)
      if (b == Building::Smithy && st.wall == WallMat::Adobe) { wm.wall = WallKind::Adobe; wm.wR = wallRampFor(WallKind::Adobe, st.wallTint, seed >> 3); wm.tR = kWood; }
      // (M3 fixer) beside a round house (a yurt, a rondavel) the wing is a smaller round one under its own cone, not a
      // square block under a cone it cannot carry (it drew as a flat white box against the yurt)
      if (ms[0].round && b != Building::Smithy) {
        wm.round = true;
        wm.r = std::min(wx1 - wx0, (float)D) * 0.5f - 1;
        wm.cx = (wx0 + wx1) * 0.5f;
        wm.cy = D - wm.r - 0.5f;
        wm.shape = ms[0].shape; wm.rmat = ms[0].rmat; wm.rR = ms[0].rR;
        wm.wall = ms[0].wall; wm.wR = ms[0].wR; wm.tR = ms[0].tR;
        wm.slope = ms[0].slope;
      }
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
  if (sunHome && !ms.empty() && !ms[0].round && ms[0].x1 - ms[0].x0 >= 36 && H(8, 6) < 4) {
    // a set-back upper room: a smaller flat-roofed block standing on the back half of the roof (or on one side), so
    // the house steps up like a little temple; its width, side and depth vary by seed
    const Mass base = ms[0];
    const float bw = base.x1 - base.x0, bd = base.y1 - base.y0;
    const int kind = H(9, 3);
    float rw = kind == 0 ? std::floor(bw * 0.62f) : std::floor(bw * (0.38f + H(13, 3) * 0.06f));
    rw = std::max(20.0f, rw);
    float rx0 = kind == 0 ? std::floor(base.x0 + (bw - rw) * 0.5f) : (H(10, 2) ? base.x0 + 2 : base.x1 - 2 - rw);
    Mass rr = baseMass(p, rx0, rx0 + rw, base.y0 + 2, base.y0 + 2 + std::max(12.0f, std::floor(bd * (0.45f + H(14, 2) * 0.1f))), 14 + H(15, 3) * 2);
    rr.zBase = base.zBase + base.wallH;   // on the roof (as the stair-head room below)
    rr.shape = RoofShape::FlatParapet;
    rr.crenel = false;
    ms.push_back(rr);
  } else if (!ms.empty() && ms[0].shape == RoofShape::FlatParapet && !ms[0].round && b != Building::Keep && b != Building::Temple &&
      b != Building::Tower && b != Building::Palace && ms[0].x1 - ms[0].x0 >= 40 && H(8, 5) < 3) {
    const Mass base = ms[0];
    float rw = 18 + (float)H(9, 3) * 3;
    bool left = H(10, 2) == 0;
    float rx0 = left ? base.x0 + 3 : base.x1 - 3 - rw;
    Mass rr = baseMass(p, rx0, rx0 + rw, base.y0 + 3, base.y0 + 3 + std::min(16.0f, (base.y1 - base.y0) * 0.4f), 11);
    rr.zBase = base.zBase + base.wallH;
    rr.shape = RoofShape::FlatParapet;
    ms.push_back(rr);
  }
  if (p.cul != CU_NONE) cultureMasses(p);
  // roof heights
  for (Mass& m : ms) finishRoofHeight(m);
  // the cross gable's ridge stops at the main ridge
  if (ms.size() >= 2 && ms.back().alongY && !ms[0].alongY && !ms[0].round && b != Building::Temple && b != Building::Keep) {
    Mass& cg = ms.back();
    float mainRidge = ms[0].zTop() - 1 + ms[0].roofH;
    float cgRidge = cg.zTop() - 1 + cg.roofH;
    if (cgRidge > mainRidge - 1) { cg.roofH = mainRidge - 1 - (cg.zTop() - 1); }
    // (M3 fixer) and it starts at the main ridge line: running the whole depth, its back half showed again behind the
    // main ridge (a gilded ridge broke off and met the main one in a crooked L on the jade houses)
    if (!ms[0].alongY) cg.y0 = std::max(cg.y0, std::floor((ms[0].y0 + ms[0].y1) * 0.5f));
  }
  // M0b: the body (where the storeys show) and its floor line; a jettied upper floor on some timber and plaster houses
  p.budget = riseBudgetPx(b, storeys);
  for (int i = 0; i < (int)ms.size(); i++) {
    Mass& m = ms[i];
    bool body = i == 0 || (i == p.doorMass && b != Building::Temple);
    if (b == Building::Keep || b == Building::Palace) body = i == 0;
    if (b == Building::Tower) body = i == p.doorMass;
    if (!body && !m.round && m.alongY && m.wallH == ms[0].wallH && m.zBase == ms[0].zBase && i > 0 && b != Building::Temple && b != Building::Keep) body = true;   // the cross gable
    m.body = body;
    if (body && storeys >= 2 && b != Building::Tower) {
      int deck = (flatShape(m.shape)) && !m.round ? 4 : 0;
      m.floorV = (m.wallH - deck) / 2 + 1;
      if (b == Building::Keep) m.floorV = 19;
    }
  }
  if (storeys >= 2 && (b == Building::House || b == Building::Shop) && !ms[0].round && (ms[0].wall == WallKind::Timber || ms[0].wall == WallKind::Plaster) &&
      !flatShape(ms[0].shape) && H(11, 2) == 0)
    for (Mass& m : ms)
      if (m.body && m.floorV > 0) { m.jetty = 2; m.jettyV = m.floorV + 1; }
  if (storeys >= 2 && (b == Building::House || b == Building::StoneHouse) && !st.awnings && ms[p.doorMass].floorV > 0) {
    const Mass& m = ms[p.doorMass];
    int r = H(12, 5);
    int w = (int)std::lround(m.x1 - m.x0), du = p.doorX - (int)std::lround(m.x0);
    bool over = false;   // an upper window right over the door, to become the balcony door
    const int step = p.cul ? p.winStep : 16, off = p.cul ? p.winOff : 0;
    for (int u = 8 + off; u < w - 6; u += step) if (std::abs(u + (w % step) / 2 - du) <= 1) over = true;
    if (r < 2 && over && !m.round && p.doorX - 12 >= m.x0 + 2 && p.doorX + 12 <= m.x1 - 2) p.balcony = true;
    else if (r < 4) p.hood = true;
  }
  return p;
}

// M0b: everything stays within the generator's clearance (the rise budget). Finials, the keep's flag and the tower's
// spike are painted above their roofs, so those roofs keep room for them. (M3: one mass at a time, so the incremental
// paint can spread a palace's fitting over steps)
void fitPlanMass(Plan& p, int i) {
  const Building b = p.b;
  Mass& m = p.sc.ms[(size_t)i];
  float extra = 2;   // the outline, and rounding to rows
  if (b == Building::Keep && i == 1) extra += 9;
  if (b == Building::Palace && (m.round || i == p.doorMass)) extra += 10;   // the flags above the towers
  if (b == Building::Temple && i == 1 && !m.round) extra += 6;
  if (b == Building::Tower && m.round) extra += 5;
  int minWall = m.body ? m.wallH : std::max(10, m.wallH - 16);   // the body keeps its storeys; towers and wings give
  if (b == Building::Keep || b == Building::Tower || b == Building::Palace) minWall = std::max(20, m.wallH - 16);
  fitMass(m, (float)p.budget - extra, minWall);
}

// the masses fitted: chimneys, dormers and the picture's height
void makePlanFinish(Plan& p) {
  const Building b = p.b, trade = p.trade;
  const ArchStyle& st = p.st;
  const uint32_t seed = p.seed;
  const int D = p.D;
  std::vector<Mass>& ms = p.sc.ms;
  // chimneys: on the main block, by the ridge, placed by seed; the smithy's forge stack over the wing junction
  {
    int n = st.chimneys;
    if (b == Building::Smithy) n = 1;
    if (b == Building::Tower || b == Building::Temple || b == Building::Farmhouse) n = 0;
    if (b == Building::Keep) n = 1;   // M0b: the great hall's hearth, a stack on the donjon
    if (b == Building::Palace) n = 2;   // M1: the great hall's hearths
    if (b == Building::Barracks) n = 1;
    if (b == Building::Hut) n = (st.smoke || (seed & 4)) ? 1 : 0;
    if (!ms.empty() && ms[0].round && ms[0].rmat == RoofMat::Felt) n = 0;   // (M3: the yurt smokes through its crown)
    if (b == Building::Inn) n = std::max(n, 1);
    if (!p.facts.hearth) n = 0;   // M0b: chimney => hearth (VISION_PLAN 15.7)
    if (flatShape(st.roof)) n = std::min(n, 1);
    const Mass base = b == Building::Keep ? ms[1] : ms[0];
    for (int i = 0; i < n; i++) {
      float cw = b == Building::Smithy ? 8.0f : 6.0f, cd = 4.0f;
      if (flatShape(base.shape)) cw = 5.0f;   // a small flue on a flat roof
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
      bool flatTop = flatShape(base.shape);
      if (flatTop) cx = (hash3(i, 34, seed) & 1) ? base.x0 + 4 : base.x1 - cw - 4;   // in a corner of the deck
      cx = std::clamp(cx, base.x0 + 2, base.x1 - cw - 2);
      float cy = base.alongY ? base.y0 + (base.y1 - base.y0) * 0.30f + (hash3(i, 33, seed) % 6) : (base.y0 + base.y1) * 0.5f + 1 + (hash3(i, 32, seed) % 3);
      if (b == Building::Smithy) cy = 6;
      if (flatTop) cy = base.y0 + 3;
      if (b == Building::Keep) { cx = base.x0 + (base.x1 - base.x0) * ((hash3(i, 35, seed) & 1) ? 0.72f : 0.2f); cy = base.y0 + (base.y1 - base.y0) * 0.45f; }
      Mass c;
      c.chimney = true;
      // the stack rises a little above the ridge beside it; M0b: within the rise budget, sliding down the front slope
      // (toward the viewer) when the ridge leaves it no room
      float zr = 0;
      auto place = [&](float y) {
        Surf s;
        int mi;
        zr = p.sc.topAt(cx + cw * 0.5f, y + cd, mi, s);
        float ridge = 0;
        for (int k = 0; k <= 8; k++) { int mj; Surf s2; ridge = std::max(ridge, p.sc.topAt(cx + cw * 0.5f, y - 6 + k * 2.0f, mj, s2)); }
        c.x0 = cx; c.x1 = cx + cw; c.y0 = y; c.y1 = y + cd;
        c.zBase = (int)std::floor(std::max(0.0f, zr - 2));
        c.wallH = (int)std::ceil(ridge + (b == Building::Smithy ? 9 : (flatTop ? 5 : 6)) - c.zBase);
      };
      const float lim = (float)p.budget - 3;
      place(cy);
      for (int k = 0; k < 24 && c.zTop() + 1 - c.y0 > lim && c.y1 < base.y1 - 6; k++) place(c.y0 + 1);
      if (c.zTop() + 1 - c.y0 > lim) c.wallH = std::max((int)std::ceil(zr + 3) - c.zBase, (int)std::floor(lim + c.y0 - 1) - c.zBase);
      // (M3 fixer) the forge's stack in the people's own material where they build in mud brick (a grey stone stack
      // stood on the dune folk's adobe smithies)
      c.wall = st.wall == WallMat::Adobe ? WallKind::Adobe : ((b == Building::Smithy || st.wall == WallMat::Stone) ? WallKind::Stone : WallKind::Brick);
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
      D >= 48 && (b == Building::House || b == Building::StoneHouse || b == Building::Inn || b == Building::Shop) &&
      !st.snow) {   // (M0 round 3) no dormers under snow: the drift buried all but a dark, ragged gable that read as a
                    // burnt hole in the roof, with the dormer ridge a stray seam down the white front plane
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
      // always a front gable: the gable triangle is part of the dormer's own face, so its little roof sits right on
      // the window wall (a hipped dormer read as a box with a lid floating above the window)
      d.shape = RoofShape::Gable;
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
  // M3 a wind-catcher (ORN_WINDCATCHER): a square tower on a flat roof, slatted vents near its top, away from the flue
  if ((st.ornament & ORN_WINDCATCHER) && !ms.empty() && flatShape(ms[0].shape) && !ms[0].round && ms[0].x1 - ms[0].x0 >= 40 &&
      (b == Building::House || b == Building::StoneHouse || b == Building::Inn || b == Building::Shop)) {
    const Mass base = ms[0];
    bool left = (hash3(3, 41, seed) & 1) != 0;
    for (const Mass& o : ms) if (o.chimney) left = o.x0 > (base.x0 + base.x1) * 0.5f;
    for (const Mass& o : ms) if (!o.chimney && &o != &ms[0] && flatShape(o.shape) && o.zBase > base.zBase) left = o.x0 > (base.x0 + base.x1) * 0.5f;
    const float wx0 = left ? base.x0 + 4 : base.x1 - 14;
    Mass wc = baseMass(p, wx0, wx0 + 10, base.y0 + 5, base.y0 + 13, 15);
    wc.zBase = base.zBase + base.wallH;
    wc.shape = RoofShape::FlatParapet;
    wc.wall = base.wall; wc.wR = base.wR; wc.tR = base.tR;
    wc.tier = true;
    finishRoofHeight(wc);
    ms.push_back(wc);
  }
  // M3 the foundation: a flat-topped box of the culture's stone under every mass standing on it, a little wider than
  // the walls, deeper at the front for a terrace (the steps up to the door are cut into it: overlays)
  if (p.fnd != Foundation::None && p.zBase > 0) {
    float fx0 = 1e9f, fx1 = -1e9f, fy0 = 1e9f, fy1 = -1e9f;
    for (const Mass& m : ms) {
      if (m.chimney || m.zBase != p.zBase) continue;
      if (m.round) { fx0 = std::min(fx0, m.cx - m.r); fx1 = std::max(fx1, m.cx + m.r); fy0 = std::min(fy0, m.cy - m.r); fy1 = std::max(fy1, m.cy + m.r); }
      else { fx0 = std::min(fx0, m.x0); fx1 = std::max(fx1, m.x1); fy0 = std::min(fy0, m.y0); fy1 = std::max(fy1, m.y1); }
    }
    if (fx1 > fx0) {
      const int cul = p.cul;
      const WallKind fk = (cul == CU_JADE || cul == CU_SUN || cul == CU_STAR || cul == CU_IMPERIAL) ? WallKind::Ashlar : (cul == CU_HIGHLAND ? WallKind::Rubble : WallKind::Stone);
      Mass f = baseMass(p, std::max(-6.0f, fx0 - 2), std::min(p.W + 6.0f, fx1 + 2), std::max(0.0f, fy0 - 1),
                        std::min((float)D + 4, fy1 + (p.fnd == Foundation::Terrace ? 4.0f : (p.fnd == Foundation::Platform ? 3.0f : 2.0f))), p.zBase);
      f.zBase = 0;
      f.found = true;
      f.shape = RoofShape::FlatParapet;
      f.ov = f.ovF = 0;
      f.wall = fk;
      f.wR = fk == WallKind::Stone ? kStone : wallRampFor(fk, cul == CU_JADE ? 0u : 0u, seed);
      f.tR = kStoneWarm;
      f.roofH = 0;
      ms.push_back(f);
    }
  }
  // how far up the picture goes
  float hmax = 0;
  for (const Mass& m : ms) {
    float t = m.zTop() + m.roofH + 6;
    if (m.chimney) t = m.zTop() + 3;
    hmax = std::max(hmax, t);
  }
  p.sc.W = p.W;
  p.sc.D = D;
  p.sc.top = (int)std::ceil(hmax) + 4;
  // M1 economy: the windmill's sails reach well above its cap (windmillDress): room for the upper arm, within budget
  if (trade == Building::Windmill && !ms.empty() && ms[0].round) {
    const Mass& t = ms[0];
    const int need = (int)std::ceil(48.0f + t.zTop() - t.cy - t.r * 0.55f);
    p.sc.top = std::min(std::max(p.sc.top, need), p.budget - 2);
  }
}

Plan makePlan(Building b, int wT, int hT, const ArchStyle& st0, uint32_t seed, const BuildingFacts& facts = BuildingFacts{}) {
  Plan p = makePlanMasses(b, wT, hT, st0, seed, facts);
  for (int i = 0; i < (int)p.sc.ms.size(); i++) fitPlanMass(p, i);
  makePlanFinish(p);
  return p;
}

// ---------------------------------------------------------------- facades
void buildFacades(Plan& p, int m0 = 0, int m1 = 1 << 30) {
  const ArchStyle& st = p.st;
  const Building b = p.b;
  const int D = p.D;
  static const uint32_t shutterCols[5] = {rgba(64, 104, 146), rgba(72, 116, 74), rgba(150, 60, 52), rgba(96, 74, 120), rgba(70, 62, 56)};
  uint32_t shutterCol = shutterCols[hash3(1, 2, p.seed) % 5];
  for (int mi = m0; mi < std::min(m1, (int)p.sc.ms.size()); mi++) {
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
    if ((flatShape(m.shape)) && !m.chimney) {
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
    const int plinth = m.found ? 0 : (m.zBase > 0 ? 2 : 3);
    paintWallMat(m, plinth);
    if (m.found) {   // M3 a foundation: a lit lip along its top edge and a darker footing course
      for (int u = 0; u < w; u++) {
        m.f.set(u, m.wallH - 1, m.wR[4]);
        if (m.wallH >= 4) m.f.set(u, 0, m.wR[1]);
      }
      continue;
    }
    if (m.round && (b == Building::Tower || b == Building::Keep || mi != p.doorMass)) continue;   // towers and turrets: details are overlays
    // M3 an upper tier (a pagoda's, a shrine on a pyramid) or a wind-catcher: a band of small openings
    if (m.tier) {
      const bool vents = m.wallH >= 14 && m.x1 - m.x0 <= 12;
      if (vents) {   // the wind-catcher's slatted vents near its top
        for (int v = m.wallH - 9; v < m.wallH - 2; v++)
          for (int u = 2; u < w - 2; u++) m.f.set(u, v, (v % 2) ? rgba(30, 22, 34) : m.wR[1]);
        for (int u = 1; u < w - 1; u++) m.f.set(u, m.wallH - 10, m.wR[4]);
        continue;
      }
      const bool shrine = m.crenel;
      if (shrine) {   // the shrine on a pyramid: a dark doorway between painted jambs
        const int du = w / 2;
        for (int v = 0; v < std::min(9, m.wallH - 2); v++)
          for (int i = -2; i <= 2; i++) m.f.set(du + i, v, (i == -2 || i == 2) ? (m.accent ? ramp(opaque(m.accent))[2] : m.tR[3]) : (v < 2 ? kInk : rgba(30, 22, 34)));
        continue;
      }
      for (int u = 6; u + 4 < w - 4; u += 9) facadeWindowS(m, (WindowShape)m.win, u, 3, 4, std::max(3, m.wallH - 7), p.seed + (uint32_t)u, false, false, shutterCol);
      continue;
    }
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
    int du = p.doorX - (int)std::lround(m.round ? m.cx - m.r : m.x0);   // (M3: a round house's facade starts at its west edge)
    bool hasDoor = front && mi == p.doorMass && du >= 4 && du < w - 4;
    bool arched = m.wall == WallKind::Stone || m.wall == WallKind::Adobe || b == Building::Temple || b == Building::Keep || b == Building::Palace;
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
        int dw = b == Building::Hut ? 8 : (b == Building::Keep || b == Building::Temple ? 12 : (b == Building::Palace ? 16 : 10));
        int dh = b == Building::Hut ? 13 : (b == Building::Keep ? 22 : (b == Building::Temple ? 20 : (b == Building::Palace ? 26 : 16)));
        if (p.cul == CU_NONE) facadeDoor(m, du, dw, dh, arched, b == Building::Temple ? kWood : kWoodDark);
        else {
          // M3 the culture's door, painted in its accent where it paints its doors; a surround from the facade variety
          DoorShape ds = (DoorShape)m.door;
          if (ds == DoorShape::Plank && arched) ds = DoorShape::Arched;
          if ((b == Building::Keep || b == Building::Palace || b == Building::Temple) && (ds == DoorShape::Plank || ds == DoorShape::Curtain || ds == DoorShape::Flap) &&
              m.wall != WallKind::Felt)
            ds = arched ? DoorShape::Double : DoorShape::Double;
          if (b == Building::Hut && ds == DoorShape::Double) ds = DoorShape::Plank;
          dh = std::min(dh, std::max(10, m.wallH - 6));
          const Ramp DW = (m.accent && ((p.doorFrame & 1) || m.cul == CU_FJORD || m.cul == CU_JADE || m.cul == CU_STEPPE)) ? ramp(opaque(m.accent)) : (b == Building::Temple ? kWood : kWoodDark);
          if (p.doorFrame == 1 && ds != DoorShape::Moon && ds != DoorShape::Round)   // pilasters either side
            for (int v = 0; v < dh + 2; v++) { m.f.set(du - dw / 2 - 4, v, m.tR[4]); m.f.set(du - dw / 2 - 3, v, m.tR[2]); m.f.set(du + dw / 2 + 2, v, m.tR[2]); m.f.set(du + dw / 2 + 3, v, m.tR[1]); }
          facadeDoorS(m, ds, du, dw, dh, arched, DW);
          if (p.doorFrame == 2 && ds != DoorShape::Moon && ds != DoorShape::Round && dh + 6 < m.wallH) {   // a fanlight over the door
            for (int i = -dw / 2; i < dw / 2; i++) {
              const int hh = (int)std::lround(3 * std::sqrt(std::max(0.0f, 1 - (i + 0.5f) * (i + 0.5f) / (dw * dw / 4.0f))));
              for (int j = 0; j <= hh; j++) { m.f.set(du + i, dh + 2 + j, j == hh ? m.tR[4] : ((i + j) % 2 ? kGlass[3] : kGlass[1])); if (j < hh) m.f.markGlass(du + i, dh + 2 + j); }
            }
          }
        }
        if (b == Building::Temple && p.cul != CU_SUN && p.cul != CU_STEPPE && p.cul != CU_JADE) {   // rose window over the door
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
    if (hasDoor && m.body) p.rowsShown |= 1u;
    if (b == Building::Keep && mi == 0) {
      // M0b: the hall below and the lord's quarters above: two rows of arrow slits (deep, with a lit sill and a dark
      // splay) under a projecting string course, spaced between the banners
      auto slit = [&](int u, int v0, int hgt) {
        for (int v = v0; v < v0 + hgt; v++) { m.f.set(u - 1, v, m.wR[1]); m.f.set(u, v, kInk); m.f.set(u + 1, v, rgba(28, 22, 36)); m.f.set(u + 2, v, m.wR[0]); }
        for (int i = -1; i <= 2; i++) { m.f.set(u + i, v0 - 1, m.tR[4]); m.f.set(u + i, v0 + hgt, m.tR[3]); }
        m.f.set(u, v0 + hgt - 1, rgba(36, 30, 44)); m.f.set(u + 1, v0 + hgt - 1, kInk);
      };
      const int F = m.floorV > 0 ? m.floorV : 19;
      for (int u = 0; u < w; u++) {   // the string course: a lit top, its shadow on the wall below
        m.f.set(u, F + 1, m.tR[4]);
        m.f.set(u, F, m.tR[2]);
        uint32_t c = m.f.get(u, F - 1);
        if (c) m.f.set(u, F - 1, darken(c, 0.35f));
      }
      bool upper = false;
      for (int u = 13; u < w - 12; u += 24) {
        if (u < 16 || u > w - 18) continue;   // behind the front drum towers
        if (std::abs(u - du) >= 12) slit(u, 7, 8);
        if (std::abs(u - du) >= 6) { slit(u, F + 4, 8); upper = true; p.rowsShown |= 2u; }
      }
      if (!upper) { slit(du - 1, F + 6, 7); p.rowsShown |= 2u; }   // a narrow keep: one slit over the gate
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
    // M0b: the body shows the generator's storeys; wings, steeples and towers keep the old rule (a tall wall carries a
    // second row of windows)
    int storeys = m.body && m.floorV > 0 ? 2 : (m.body ? 1 : (m.wallH >= 34 ? 2 : 1));
    if (m.body && b == Building::Temple) storeys = 1;
    const bool feltWall = m.wall == WallKind::Felt;   // (M3: a yurt has no windows below: its light comes through the crown)
    if (p.cul && m.wallH < 26 && b != Building::Temple) { winH = std::clamp(m.wallH - 13, 4, winH); winV = std::min(winV, std::max(4, m.wallH - winH - 7)); }   // (M3: low walls, small windows)
    const int F = m.floorV;
    if (F > 0) {
      // the floor line between the storeys, in the wall's own language: a beam on timber (painted with the frame), a
      // projecting string course on stone, brick and plaster (lit top, shadow below), a plate log with joist ends on
      // log walls, a row of vigas on adobe
      auto course = [&](const Ramp& T) {
        for (int u = 0; u < w; u++) {
          m.f.set(u, F + 1, T[4]);
          m.f.set(u, F, T[2]);
          uint32_t c = m.f.get(u, F - 1);
          if (c) m.f.set(u, F - 1, darken(c, 0.3f));
        }
      };
      if (m.wall == WallKind::Stone || m.wall == WallKind::Brick || m.wall == WallKind::Rubble) course(kStoneWarm);
      else if (m.wall == WallKind::Ashlar) course(m.wR);
      else if (m.wall == WallKind::Wattle || m.wall == WallKind::Planks || m.wall == WallKind::Living) course(m.wall == WallKind::Living ? kBark : kBeam);
      else if (m.wall == WallKind::Plaster && !m.jetty) course(m.tR);
      else if (m.wall == WallKind::Log) {
        for (int u = 0; u < w; u++) { m.f.set(u, F + 1, kLog[4]); m.f.set(u, F, kLog[2]); m.f.set(u, F - 1, kLog[0]); }
        for (int u = 6; u < w - 5; u += 8) { m.f.set(u, F, kLog[4]); m.f.set(u + 1, F, kLog[3]); m.f.set(u, F + 1, kLog[3]); m.f.set(u + 1, F + 1, kLog[1]); }
      } else if (m.wall == WallKind::Adobe) {
        for (int u = 4; u < w - 3; u += 7) {
          m.f.set(u, F + 1, kWood[3]); m.f.set(u + 1, F + 1, kWood[2]); m.f.set(u, F, kWood[1]); m.f.set(u + 1, F, kWood[0]);
          uint32_t c = m.f.get(u + 1, F - 1);
          if (c) m.f.set(u + 1, F - 1, darken(c, 0.3f));
        }
      }
      if (m.jetty) {
        // the jettied floor: a heavy sill beam on the joist ends, and the ground floor below in the overhang's shade
        // (the rows the jetty hides are painted too; the renderer sets the lower wall back)
        const int J = m.jettyV;
        for (int u = 0; u < w; u++) { m.f.set(u, J + 1, kBeam[3]); m.f.set(u, J, kBeam[2]); }
        for (int u = 1; u < w - 1; u += 4) { m.f.set(u, J - 1, kBeam[3]); m.f.set(u + 1, J - 1, kBeam[1]); }
        for (int v = J - m.jetty - 4; v < J - 1; v++)
          for (int u = 0; u < w; u++) {
            if (v < 0) continue;
            int dd = (J - m.jetty - 1) - v;   // 0 right under the overhang
            uint32_t c = m.f.get(u, v);
            if (c && dd <= 3) m.f.set(u, v, darken(c, dd <= 0 ? 0.6f : (dd == 1 ? 0.45f : (dd == 2 ? 0.3f : 0.15f))));
          }
      }
    }
    bool donjon = b == Building::Keep && mi == 1;
    for (int s = 0; s < storeys; s++) {
      int v0 = s == 0 ? winV : m.wallH / 2 + 7;
      int ww = winW, wh0 = winH;
      if (F > 0 && s == 1) {   // the upper floor: a little smaller, centred over the windows below
        v0 = F + 5 + (m.jetty ? 1 : 0);
        wh0 = winH - 1;
        if (v0 + wh0 + 2 > m.wallH - (m.shape == RoofShape::FlatParapet ? 6 : 2)) v0 = m.wallH - (m.shape == RoofShape::FlatParapet ? 6 : 2) - wh0 - 2;
      }
      if (donjon) { if (s == 0) continue; v0 = m.wallH - 13; }
      if (feltWall && s == 0) continue;   // (M3: a two-storey yurt shows its upper floor by a band of round vents)
      const int step = p.cul ? p.winStep : 16, off = p.cul ? p.winOff : 0;   // (M3: the culture's facade rhythm)
      for (int u = 8 + off; u < w - 6; u += step) {
        int uc = u + ((w % step) / 2);
        if (uc < 6 || uc > w - 7) continue;
        if (hasDoor && s == 0 && std::abs(uc - du) < 11) continue;
        if (b == Building::Shop && s == 0 && std::abs(uc - du) < 20) {   // a wide shop window beside the door
          facadeWindow(m, uc - 5, v0 - 1, 10, 7, p.seed + u, false, false, false, shutterCol);
          if (m.body) p.rowsShown |= 1u << s;
          continue;
        }
        if (p.balcony && s == 1 && mi == p.doorMass && std::abs(uc - du) <= 1) {   // the glazed door onto the balcony
          facadeWindow(m, uc - 3, F + 3, 6, 11, p.seed + u, false, false, false, shutterCol);
          p.rowsShown |= 2u;
          continue;
        }
        bool sh = st.shutters && hash3(u, s, p.seed) % 3 != 0 && m.wall != WallKind::Adobe;
        bool fl = !st.snow && m.wall != WallKind::Adobe && hash3(u, s + 5, p.seed) % 4 == 0;
        bool ar = (m.wall == WallKind::Stone && b != Building::Smithy) || b == Building::Temple;
        int wh = b == Building::Temple ? 12 : wh0;
        if (p.cul != CU_NONE) {
          // M3 the culture's windows: its shape, shutters and flower boxes where it hangs them, its shutter colours
          WindowShape ws = (WindowShape)m.win;
          if (ws == WindowShape::Square && ar) ws = WindowShape::Arched;
          if (b == Building::Temple && ws != WindowShape::Pointed && ws != WindowShape::Lattice && ws != WindowShape::Screen && ws != WindowShape::Round) ws = WindowShape::Arched;
          sh = (st.shutters || (st.ornament & ORN_SHUTTERS)) && hash3(u, s, p.seed) % 3 != 0 && ws != WindowShape::Screen && ws != WindowShape::Slit &&
               m.wall != WallKind::Adobe && m.wall != WallKind::Felt;
          fl = !st.snow && (st.ornament & ORN_FLOWERBOX) && hash3(u, s + 5, p.seed) % 2 == 0 && ws != WindowShape::Screen && ws != WindowShape::Slit;
          int wx = ww, wy = wh;
          if (ws == WindowShape::Tall) { wx = 5; }
          if (ws == WindowShape::Round) { wx = 6; wy = 6; }
          if (ws == WindowShape::Pointed) { wx = 5; wy = std::max(5, wh - 2); }
          if (ws == WindowShape::Lattice || ws == WindowShape::Screen) { wx = 7; wy = 7; }
          facadeWindowS(m, ws, uc - wx / 2, v0 - (b == Building::Temple ? 2 : 0), wx, wy, p.seed + u * 7 + s, sh, fl, p.shutterCol ? p.shutterCol : shutterCol);
          if (m.body) p.rowsShown |= 1u << s;
          if (b == Building::Temple && (ws == WindowShape::Arched || ws == WindowShape::Pointed))
            for (int j = 1; j < wy - 1; j++)
              for (int i = 0; i < wx; i++) {
                if (i == wx / 2 || !m.f.isGlass(uc - wx / 2 + i, v0 - 2 + j)) continue;
                static const uint32_t sg[4] = {rgba(222, 74, 84), rgba(84, 132, 230), rgba(250, 204, 86), rgba(92, 192, 122)};
                m.f.set(uc - wx / 2 + i, v0 - 2 + j, sg[(i + j / 3) % 4]);
              }
          continue;
        }
        facadeWindow(m, uc - ww / 2, v0 - (b == Building::Temple ? 2 : 0), ww, wh, p.seed + u * 7 + s, sh, fl, ar, shutterCol);
        if (m.body) p.rowsShown |= 1u << s;
        if (b == Building::Temple)
          for (int j = 1; j < wh - 1; j++)
            for (int i = 0; i < ww; i++) {
              if (i == ww / 2) continue;
              static const uint32_t sg[4] = {rgba(222, 74, 84), rgba(84, 132, 230), rgba(250, 204, 86), rgba(92, 192, 122)};
              m.f.set(uc - ww / 2 + i, v0 - 2 + j, sg[(i + j / 3) % 4]);
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
  std::vector<uint8_t> glass;   // 1 = a visible window pane
  int ox = BLDG_PAD_X;
  int clipped = 0;              // pixels the rise budget cut off (the plan should leave none: arch_gallery --check)
  explicit Painter(Plan& pl) : p(pl) {
    c = Canvas(p.W + 2 * BLDG_PAD_X, p.sc.top + p.D + BLDG_PAD_B);
    layer.assign((size_t)c.w * c.h, 0);
    glass.assign((size_t)c.w * c.h, 0);
  }
  int rowOf(float y, float z) const { return (int)std::floor(p.sc.top + y - z); }
  void put(int x, int y, uint32_t col, uint8_t l, bool pane = false) {
    if (x < 0 || y < 0 || x >= c.w || y >= c.h || !chA(col)) return;
    if (y < p.sc.top - p.budget + 1) { clipped++; return; }   // M0b: never above the rise budget (the outline takes the last row)
    c.set(x, y, col);
    layer[(size_t)y * c.w + x] = l;
    glass[(size_t)y * c.w + x] = pane ? 1 : 0;
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
  if (s == Surf::Crown) {   // M3 a yurt's crown ring: a lit wooden ring round the dark smoke hole, its spokes
    const float dx = x - m.cx, dy = y - m.cy, rr = std::sqrt(dx * dx + dy * dy);
    if (rr < 1.9f) return std::fabs(dx) < 0.5f || std::fabs(dy) < 0.5f ? kWood[1] : mix(kInk, kSoil[0], 0.4f);
    return kWood[dx + dy < 0 ? 4 : (rr > 2.9f ? 1 : 2)];
  }
  if (m.found && s == Surf::Flat) {   // M3 the top of a foundation: big paving slabs, worn lighter toward the front
    const int gx = (int)std::floor(x), gy = (int)std::floor(y);
    int k = (gy % 4 == 0 || ((gx + (gy / 4) * 5) % 9 + 9) % 9 == 0) ? 2 : 3;
    if (y > m.y1 - 1.5f) k = 4;   // the lit front arris
    if (shade) k = std::max(0, k - 2);
    return m.wR[k];
  }
  if (s == Surf::Parapet) {
    // M0b fix round: the parapet has a body: its top lit on the outer (west/north) rims, the inner rim that drops to
    // the deck a step darker on the far side facing us, and its coping shaded on the east
    const Ramp& W = m.wR;
    if (!m.round && (flatShape(m.shape)) && !m.crenel) {
      float ex = x - m.x0, wx = m.x1 - 1 - x, ny0 = y - m.y0, sy = m.y1 - 1 - y;
      float e = std::min(std::min(ex, wx), std::min(ny0, sy));
      bool inner = e >= 1.0f;
      if (inner && ny0 < 2) return W[3];                   // the north parapet's inner face, toward us and the sun
      if (inner && wx < 2) return W[2];                    // the east parapet's inner face, in shade
      if (inner && ex < 2) return W[3];
      if (wx < 1 || sy < 1) return W[3];
      return W[4];
    }
    return W[4];
  }
  if (s == Surf::Flat) {
    // a roof deck: packed earth/plaster for adobe, flagstones for stone
    int k = 2;
    // the parapet (2 px thick, 3+ px tall) casts its shadow down-right onto the deck: a band inside the west and
    // north rims (integration: without it flat roofs read as a flat slab with a painted border)
    bool pshadow = false;
    if (!m.round && (flatShape(m.shape))) {
      pshadow = x - m.x0 < 5.0f || y - m.y0 < 4.0f;
    } else if (m.round && !m.chimney) {
      float dx = x - m.cx, dy = y - m.cy, rr = std::hypot(dx, dy);
      pshadow = rr > m.r - 5.0f && dx * -0.72f + dy * -0.45f > rr * 0.35f;
    }
    if (m.wall == WallKind::Adobe || m.rmat == RoofMat::Adobe) {
      int ix = (int)std::floor(x), iy = (int)std::floor(y);
      const Ramp& A = m.wall == WallKind::Adobe ? m.wR : kAdobe;
      // M0b fix round: a lived-on deck, not a blank slab. It falls gently toward its rain spouts (lighter at the
      // back, a step darker at the front), the trowelled mud shows patches and hairline cracks, the parapet's
      // shadow lies along its west and north rims, and the household keeps things up here: a hatch to the stair,
      // water jars, a mat of drying dates or chillies.
      float fx = (x - m.x0) / std::max(1.0f, m.x1 - m.x0), fy = (y - m.y0) / std::max(1.0f, m.y1 - m.y0);
      float n = vnoise(x / 6.0f, y / 4.0f, m.seed + 3) * 0.6f + vnoise(x / 2.5f, y / 2.0f, m.seed + 5) * 0.4f;
      float slope = 0.35f - fy * 0.55f - fx * 0.15f;
      float v = n + slope + (bayer(ix, iy) - 0.5f) * 0.12f;
      k = v < 0.3f ? 2 : (v > 0.8f ? 4 : 3);
      if (shade) k = std::max(0, k - 1);
      if (x - m.x0 < 6.0f || y - m.y0 < 5.0f) {   // the parapet's shadow, deepest along its foot
        float d = std::min(x - m.x0, (y - m.y0) * 1.25f);
        if (d < 4.0f) return mix(A[1], A[0], 0.3f);
        if (d < 6.0f) return A[std::max(1, k - 1)];
      }
      // hairline cracks in the mud
      float cr = vnoise(x / 3.0f + 11.0f, y / 3.0f, m.seed + 9);
      if (std::fabs(cr - 0.5f) < 0.018f && hash3(ix, iy, m.seed + 4) % 3) return A[std::max(0, k - 2)];
      // the deck's furniture, placed by the deck's own size (the dome's drum leaves no room)
      if (m.shape == RoofShape::FlatParapet && m.x1 - m.x0 >= 22 && m.y1 - m.y0 >= 14) {
        uint32_t hs = hash3((int)m.x0, (int)m.y0, m.seed + 21);
        // a wooden hatch over the stair, in the back half
        float hx0 = m.x0 + 8 + (float)(hs % 5), hy0 = m.y0 + 7;
        // (the deck is seen foreshortened: things on it are laid out half as tall again in y to keep their shape)
        if (x >= hx0 && x < hx0 + 10 && y >= hy0 && y < hy0 + 9) {
          int lx = (int)(x - hx0), ly = (int)((y - hy0) / 1.5f);
          if (lx == 0 || ly == 0) return kWood[3];
          if (lx == 9 || ly == 5) return kWood[0];
          if (lx == 5) return kWood[1];
          return (ly == 2 && (lx == 2 || lx == 7)) ? kIron[2] : kWood[2];
        }
        if (x >= hx0 + 10 && x < hx0 + 11.5f && y >= hy0 + 1.5f && y < hy0 + 10.5f) return mix(A[1], A[0], 0.4f);   // its shadow
        if (y >= hy0 + 9 && y < hy0 + 10.5f && x >= hx0 + 1 && x < hx0 + 11.5f) return mix(A[1], A[0], 0.4f);
        // two water jars against the east parapet
        for (int j = 0; j < 2; j++) {
          float jx = m.x1 - 5.5f - j * 4.0f, jy = m.y1 - 6.0f - (float)((hs >> 4) % 4);
          float dx = x - jx, dy = (y - jy) * 0.75f, r2 = dx * dx + dy * dy;
          if (r2 < 5.8f) return r2 < 0.9f ? kClay[0] : (dx < -0.4f && dy < 0.4f ? kClay[3] : (dx > 1.2f || dy > 1.4f ? kClay[1] : kClay[2]));
          if (dx > 0.8f && dx < 3.6f && dy > 0.8f && dy < 3.8f) return mix(A[1], A[0], 0.35f);
        }
        // a mat of drying fruit
        float mx0 = m.x0 + 4.0f + (float)((hs >> 8) % 6), my0 = m.y1 - 10.0f;
        if (((hs >> 12) & 1) && x >= mx0 && x < mx0 + 10 && y >= my0 && y < my0 + 7.5f) {
          int lx = (int)(x - mx0), ly = (int)((y - my0) / 1.5f);
          if (lx == 0 || ly == 0 || lx == 9 || ly == 4) return mix(kThatch[2], kThatch[1], 0.3f);
          uint32_t hh = hash3(ix, iy, m.seed + 31);
          return hh % 3 == 0 ? rgba(196, 64, 46) : (hh % 3 == 1 ? rgba(150, 40, 40) : mix(kThatch[3], kThatch[2], 0.5f));
        }
      }
      return A[k];
    }
    // (M3 fixer round 3, review: "flat roofs use wall-brick texture") a paved roof terrace, not a wall face: big
    // flagstones of uneven length in staggered courses, each slab its own tone, fine joints lit on their far (south)
    // lip, the deck a touch brighter toward the sunlit back where the parapet shadow ends. Plastered and dressed-stone
    // buildings pave it in their own pale stone; the rest in grey flags.
    const bool pale = m.wall == WallKind::Ashlar || m.wall == WallKind::Plaster || m.wall == WallKind::Brick;
    const Ramp& D = pale ? m.wR : kStone;
    const int gx = (int)std::floor(x), gy = (int)std::floor(y);
    const int row = (gy + 64) / 5, pr = (gy + 64) % 5;
    const int off = (int)(hash3(row, 3, m.seed) % 9);
    int sx = gx + 64 + off, sl = 0, slab = 0;
    {
      int acc = 0;
      for (int s = 0; s < 40; s++) {
        const int len = 7 + (int)(hash3(row, s, m.seed + 5) % 6);
        if (sx < acc + len) { slab = s; sl = sx - acc; break; }
        acc += len;
      }
    }
    const uint32_t hs = hash3(row, slab, m.seed + 9);
    k = pale ? 3 : 2;
    if (hs % 5 == 0) k--;
    else if (hs % 7 == 0 && k < 4) k++;
    if (pr == 0) k = std::max(0, k - 1);                // the joint between courses
    else if (pr == 4 && sl > 0) k = std::min(4, k + (pale ? 0 : 1) );   // the lit lip of the slab before it
    if (sl == 0 && pr != 0) k = std::max(0, k - 1);     // the joint between slabs
    if (shade) k = std::max(0, k - 1);
    if (pshadow) return mix(D[k], D[0], 0.42f);
    if (hash3(gx, gy, m.seed + 13) % 23 == 0) return D[std::max(0, k - 1)];   // wear
    return D[k];
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
  // (M3 fixer round 3, review: "domes are flat discs, a pale half and a yellow half like a coin") domes and onion bulbs
  // are lit as real spheres: a continuous light term dithered between the ramp steps (so the shading turns smoothly
  // round the curve), a specular glint up-left, the far rim falling into core shadow with a little bounced light at
  // its very edge, ribs (gores) running down from the crown on metal and glazed caps, and a finial at the top
  if (m.shape == RoofShape::Dome || m.shape == RoofShape::Onion) {
    float dcx, dcy, Rd;
    if (m.round) { dcx = m.cx; dcy = m.cy; Rd = m.r + (m.shape == RoofShape::Onion ? 1.5f : 1.0f); }
    else {
      dcx = (m.x0 + m.x1) * 0.5f; dcy = (m.y0 + m.y1) * 0.5f - 1;
      Rd = std::min(m.x1 - m.x0, m.y1 - m.y0) * 0.5f - (m.shape == RoofShape::Onion ? 6 : 7);
    }
    const float ddx = x - dcx, ddy = (y - dcy) / (m.round || m.shape == RoofShape::Onion ? 1.0f : 0.74f), rr = std::sqrt(ddx * ddx + ddy * ddy), tq = std::min(1.0f, rr / std::max(1.0f, Rd));
    const float lx = -0.62f, ly = -0.40f, lz = 0.68f;
    const float lam = nx * lx + ny * ly + nzz * lz;
    float val = 0.10f + 0.95f * std::max(0.0f, lam);
    if (lam < 0.05f && tq > 0.86f) val += 0.10f;   // bounced light along the shadowed rim
    const int ix = (int)std::floor(x), iy = (int)std::floor(y - z);
    float f = val * 4.0f;
    int kk = (int)std::floor(f);
    if (f - kk > 0.5f + (bayer(ix, iy) - 0.5f) * 0.45f) kk++;   // clean bands, dithered only along their seams
    kk = std::clamp(kk, 0, 4);
    if (m.snow && nzz > 0.55f) return kSnow[std::clamp(kk, 1, 4)];
    // the crown's finial
    if (tq < 0.07f) return (m.rmat == RoofMat::Adobe || m.rmat == RoofMat::Felt) ? m.tR[kk] : kGold[std::clamp(kk + 1, 2, 4)];
    // gores: 12 ribs down from the crown (metal, glaze, slate), lit on their west edge
    if (m.rmat == RoofMat::Copper || m.rmat == RoofMat::GlazedTile || m.rmat == RoofMat::Slate) {
      const float a = (std::atan2(ddy, ddx) + PI) / TAU * 12.0f;
      const float fr = a - std::floor(a);
      if (tq > 0.18f && tq < 0.97f) {
        if (fr < 0.07f) kk = std::max(0, kk - 1);
        else if (fr < 0.14f && kk < 4) kk++;
      }
      if (m.rmat == RoofMat::GlazedTile && ((int)std::floor(tq * 9.0f) & 1) && fr > 0.5f && fr < 0.56f) kk = std::max(0, kk - 1);
    }
    // a ring at the springing line (where the cap meets its drum): the eave of the dome in shadow
    if (tq > 0.94f && ny > 0.1f) kk = std::max(0, kk - 1);
    uint32_t col = R[kk];
    // the specular glint on smooth metal and glaze
    const float hx = lx, hy = ly, hz = lz + 1.0f, hl = std::sqrt(hx * hx + hy * hy + hz * hz);
    const float spec = (nx * hx + ny * hy + nzz * hz) / hl;
    if ((m.rmat == RoofMat::Copper || m.rmat == RoofMat::GlazedTile) && spec > 0.985f) col = mix(R[4], rgba(255, 255, 240), 0.55f);
    else if (spec > 0.992f) col = mix(R[4], rgba(255, 255, 240), 0.3f);
    if (shade) col = mix(col, R[0], 0.45f);
    return col;
  }
  float l = nx * -0.72f + ny * -0.36f + nzz * 0.60f;
  int k;
  if (l > 0.97f) k = 4;
  else if (l > 0.56f) k = 3;
  else if (l > 0.16f) k = 2;
  else if (l > -0.16f) k = 1;
  else k = 0;
  // a plane facing the viewer (south) is mid-tone, like the south walls, never in deep shade: steep front planes
  // otherwise sink to the darkest ramp step and read as a dark flat slab
  if (!m.round && ny > 0.2f && std::fabs(nx) < 0.3f) k = std::max(k, 2);
  // courses: v up the slope from the nearest eave, u along it
  bool facesY = std::fabs(gy) >= std::fabs(gx);
  int u = (int)std::floor(facesY ? x : y);
  int v = (int)std::floor(z - (m.zTop() - 1));
  if (m.round) { u = (int)std::floor(std::atan2(y - m.cy, x - m.cx) * m.r); v = (int)std::floor(z); }
  // thatch is laid screen-aligned on every plane (strands straight down, courses across, one course every 6 screen
  // rows), as a painter would. Laid along each slope it turned into horizontal dashes in vertical columns on the
  // east/west planes, and on shallow planes (a course stretched over 2-3 screen rows, its wavy edge stepping by whole
  // rows) into staggered blocks: both read as brickwork.
  int tu = u, tv = v;
  if (!m.round && (m.rmat == RoofMat::Thatch || m.rmat == RoofMat::Palm)) { tu = (int)std::floor(x); tv = (int)std::floor(z - y) + 400; }
  bool special = false;
  int t = roofTexel(m.rmat, tu, tv, m.seed, special);
  // keep the strongest highlight for the ridge itself
  if (k == 4 && t > 0) t = 0;
  // hip roofs: the courses on the two end planes run at right angles to the front's, and with the same shade lines on
  // all four planes the roof reads as nested trays. The end planes stay smooth (light and shade only), so the four
  // planes read as volumes and the front plane carries the courses.
  const bool hipRoof = !m.round && (m.shape == RoofShape::Hip || m.shape == RoofShape::Pagoda || m.shape == RoofShape::Mansard);
  const bool endPlane = hipRoof && (m.alongY ? facesY : !facesY);
  if (endPlane && t < 0 && m.rmat != RoofMat::Thatch && m.rmat != RoofMat::Palm) t = 0;   // (thatch keeps its strands on every plane)
  k = std::clamp(k + t, 0, 4);
  // hip lines: the capped edges where two planes meet catch the light (brightest on the west, the side facing the
  // sun), so the roof shows its four faces and its ridge even at 1x
  if (hipRoof && m.roofH > 4) {
    const float rx0 = m.x0 - m.ov, rx1 = m.x1 + m.ov, ry0 = m.y0 - m.ov, ry1 = m.y1 + m.ovF;
    const float ddy = std::min(y - ry0, ry1 - y), ddx = std::min(x - rx0, rx1 - x);
    const float a = m.alongY ? ddx : ddy, b = m.alongY ? ddy * m.hk : ddx * m.hk;
    const float dmax = m.alongY ? (rx1 - rx0) * 0.5f : (ry1 - ry0) * 0.5f;
    if (std::fabs(a - b) < 0.75f && std::min(a, b) > 0.6f && std::min(a, b) < dmax - 0.8f) {
      const bool west = x < (rx0 + rx1) * 0.5f;
      k = west ? 4 : std::max(k, 3);
      if (m.rmat == RoofMat::Thatch) k = west ? 3 : std::max(k, 2);   // thatch hips are rounded: a softer line
    }
  }
  if (special && m.rmat == RoofMat::Turf) { uint32_t fh = hash3(u, v, 5) % 3; return fh == 0 ? rgba(240, 214, 96) : (fh == 1 ? rgba(224, 228, 244) : rgba(196, 132, 210)); }
  // M3: glints on glaze, a gold leaf here and there on elven roofs, the great stair up a stepped pyramid
  if (special && m.rmat == RoofMat::GlazedTile && k >= 2 && !m.snow) return R[4];
  if (special && m.rmat == RoofMat::Leaf && !m.snow) return mix(R[std::clamp(k, 1, 4)], k >= 3 ? rgba(214, 196, 96) : rgba(150, 132, 56), 0.55f);   // (M3 fixer: muted)
  if (m.shape == RoofShape::Stepped && m.stairX1 > m.stairX0 && x >= m.stairX0 && x < m.stairX1 && y > (m.y0 + m.y1) * 0.5f) {
    if (x < m.stairX0 + 1.5f || x >= m.stairX1 - 1.5f) return m.tR[x < m.stairX0 + 1.5f ? 4 : 1];   // the balustrades
    const int zz = (int)std::floor(z);
    return R[(zz % 2 == 0) ? 1 : 3];   // treads and risers
  }
  // ridge cap: a lit capping along the ridge; thatch gets a woven ridge band
  float zt = m.zTop() - 1 + m.roofH;
  if (!m.round && m.shape != RoofShape::Turf && m.shape != RoofShape::Pagoda) {
    if (m.rmat == RoofMat::Thatch && m.cul == CU_JADE && z > zt - 2.4f) return kRoofGrey[z > zt - 1.0f ? 3 : 1];   // (M3 fixer r3) a tiled ridge cap
    if (m.rmat == RoofMat::Thatch && z > zt - 2.6f) k = ((u + (int)(z * 2)) % 4 < 2) ? 1 : 3;
    else if (z > zt - 1.0f) k = 4;
    else if (z > zt - 1.6f && ny > 0.2f) k = std::min(k, 2);
  }
  // sod: a sunlit crest of grass along the ridge, and the front slope falling into shade toward the eave, so the
  // hump reads as a roof and not as a lawn
  // (M0b fix round: every sod roof, low-pitched ones too, which read as a flat green mat: a lit crest, a dark seam
  // just under it where the hump turns, the slope lighter at the top and falling into shade toward the eave)
  if (!m.round && m.rmat == RoofMat::Turf && m.roofH > 1) {
    float tt = (z - (m.zTop() - 1)) / m.roofH;
    // (M3 fixer round 3, review: "Fjordfolk sod roofs are flat green slabs, no ridge or pitch") the front slope is a
    // full gradient from the sunlit crest down to the shaded eave (dithered seams), so the hump turns
    if (z > zt - 1.4f) k = std::min(4, std::max(k, 3) + ((hash3((int)std::floor(x), 3, m.seed) & 1) ? 1 : 0));
    else if (z > zt - 2.2f && ny > 0.2f) k = std::max(1, std::min(k, 2));
    else if (ny > 0.2f) {
      const float lin = 1.0f - std::sqrt(std::max(0.0f, 1.0f - tt));   // back from the sod hump's height to the run of the slope
      const float g = 0.5f + lin * 3.4f;
      int kb = (int)std::floor(g);
      if (g - kb > 0.5f + (bayer((int)std::floor(x), (int)std::floor(y - z)) - 0.5f) * 0.6f) kb++;
      k = std::clamp(kb + (t > 0 ? 1 : (t < 0 ? -1 : 0)), 0, 4);
    }
  }
  // front planes: lit toward the ridge, a soft fall-off toward the eave gives the slope its curve (a dithered seam
  // between the bands, so the plane reads as a volume turning toward the light, not one flat value)
  if (!m.round && ny > 0.2f && m.roofH > 4) {
    // (M0 round 3: the band edges are clean rows now. A per-pixel dither there broke the lit course lips into
    // isolated light rectangles scattered across slate and snow front planes.)
    float tt = (z - (m.zTop() - 1)) / m.roofH;
    if (tt < 0.16f) k = std::max(0, k - 1);
    else if (tt > 0.62f && k < 3) k++;
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
  // M3 roof stones (ORN_ROOF_STONES): ropes thrown over the thatch against the gales, weighted with stones at the eave
  if ((m.orn & ORN_ROOF_STONES) && (m.rmat == RoofMat::Thatch || m.rmat == RoofMat::Palm) && !m.round && !m.snow) {
    const int rx = ((int)std::floor(x) % 9 + 9) % 9;
    if (ny > 0.2f && v <= 2 && rx >= 0 && rx <= 2) return kStone[rx == 0 ? 4 : (rx == 1 ? 3 : 1)];
    if (rx == 1 && ny > -0.2f) return mix(R[std::max(0, k - 1)], kCloth[1], 0.5f);
  }
  // M3 a gilded ridge (ORN_GILDING)
  if ((m.orn & ORN_GILDING) && !m.round && m.shape != RoofShape::Turf && z > m.zTop() - 1 + m.roofH - 1.2f) return kGold[x < (m.x0 + m.x1) * 0.5f ? 4 : 3];
  // moss on the north (back) plane and the damp lower courses of old roofs
  if (m.moss >= 2 && (m.rmat == RoofMat::Thatch || m.rmat == RoofMat::Shingle || m.rmat == RoofMat::Slate)) {
    bool north = ny < -0.2f;
    if ((north && hash3(u / 3, v / 2, m.seed + 41) % 4 == 0) || (v < 3 && hash3(u / 4, 0, m.seed + 43) % 5 == 0))
      return mix(R[k], kMoss[std::clamp(k, 1, 3)], 0.55f);
  }
  if (m.snow && m.rmat != RoofMat::Turf && v > 1 + (int)(hash3(u / 3, 0, m.seed + 7) % 3)) {
    // snow lies on the roof, thin at the eaves and on steep planes facing the sun
    // (M0 round 3: k + 1 pushed the front plane and the lit west plane both to the two whitest steps, so snowed roofs
    // were nearly uniform white. The plane's own step now maps straight onto the snow ramp: the sunlit west plane
    // and the ridge band white, the front plane a cool blue-white, the east plane in blue shade.)
    int ks = std::clamp(k - std::max(t, 0), 1, 4);   // snow hides the courses' lit lips (no light dashes on it)
    if (t < 0 && hash3(u, v, m.seed) % 3 == 0) ks = std::max(1, ks - 1);
    return kSnow[ks];
  }
  // sod roofs: the turf sits on a log edge (the "torvtak" eave board), a dark lip that gives the roof its thickness;
  // the layer of soil shows above it, and grass hangs over it in tufts
  if (!m.round && m.rmat == RoofMat::Turf) {
    uint32_t hg = hash3((int)std::floor(x), 9, m.seed);
    if (v <= 0) return (hg % 4 == 0) ? R[1] : (ny > 0.2f ? kWood[1] : kWood[2]);
    if (v == 1) return (hg % 3 == 0) ? R[std::max(1, k - 1)] : mix(kSoil[2], kSoil[1], 0.5f);
    if (v == 2) return mix(R[std::max(0, k - 1)], kSoil[1], 0.35f);
  }
  // the lowest courses sit in the gloom under the next roof edge; the eave line darkens a little
  if (!m.round && v <= 1 && k > 1 && !endPlane) k--;
  if (shade) k = std::max(0, k - 1);
  (void)p; (void)mi;
  return R[k];
}

void renderColumns(Painter& P, int xa, int xb) {
  Plan& p = P.p;
  const Scene& sc = p.sc;
  // y range covered by the roofs and walls
  float yMin = -8, yMax = (float)p.D + 6;
  const float step = 0.25f;
  for (int x = std::max(xa, -BLDG_PAD_X); x < std::min(xb, p.W + BLDG_PAD_X); x++) {
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
          float fyv = e.y;
          if (m.jetty && v < m.jettyV) {   // M0b: the ground floor stands back (and in) under the jettied floor
            if (u < m.jetty || u >= m.f.w - m.jetty) continue;
            fyv -= (float)m.jetty;
          }
          P.put(P.ox + x, P.rowOf(fyv, (float)m.zBase + v), col, m.chimney ? 2 : 1, !m.chimney && m.f.isGlass(u, v));
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
      // (M3 fixer) a valley: where two roofs meet at one height (a wing's or a cross gable's roof running into the main
      // one) the join is a dark gutter line, so the two roofs read as two volumes and not one flat sheet
      if (prevMi >= 0 && prevMi != mi && s == Surf::Roof && prevS == Surf::Roof && !m.chimney && !sc.ms[prevMi].chimney && !m.round &&
          !sc.ms[prevMi].round && std::fabs(z - prevZ) < 1.6f && row >= prevRow)
        col = darken(col, 0.55f);
      P.put(P.ox + x, row, col, 2);
      prevRow = row; prevMi = mi; prevS = s; prevCol = col; prevZ = z;
    }
    if (prevMi >= 0) fascia(prevRow, prevMi, prevS);
  }
}
// eave shadow: the wall just under a roof edge is in the roof's shade (offset right with the light). Each pixel reads
// only its own colour and the layer map (which this pass never changes), so it runs in any row ranges.
void eaveShadow(Painter& P, int ya, int yb) {
  for (int y = std::max(0, ya); y < std::min(yb, P.c.h); y++)
    for (int x = 0; x < P.c.w; x++) {
      if (P.layer[(size_t)y * P.c.w + x] != 1) continue;
      int d = 0;
      for (int k = 1; k <= 3 && !d; k++) {
        int ax = x - (k > 1 ? 1 : 0);
        if (y - k >= 0 && ax >= 0 && P.layer[(size_t)(y - k) * P.c.w + ax] == 2) d = k;
      }
      if (d) P.c.set(x, y, darken(P.c.get(x, y), d == 1 ? 0.55f : (d == 2 ? 0.40f : 0.2f)));
    }
}

// details that stand proud of the walls, painted last: stilts, steps, signs, awnings, banners, lanterns
// ---------------------------------------------------------------- M1 economy: mill machinery
// The tower mill's door, windows and sails. The sails turn in the screen plane on the cap's front (the mill faces the
// wind from the south, the viewer): four stocks from a hub, each carrying a lattice sail of canvas on its trailing side,
// the upper-left ones catching the light.
template <class AT>
void windmillDress(Painter& P, AT&& at) {
  Plan& p = P.p;
  if (p.sc.ms.empty() || !p.sc.ms[0].round) return;
  const Mass& t = p.sc.ms[0];
  auto onFront = [&](float x, float z) { float dx = x - t.cx; return at(x, t.cy + std::sqrt(std::max(0.0f, t.r * t.r - dx * dx)) - 0.5f, z); };
  // the floor line between the stone floor and the loft: a timber band round the drum
  const float zc = std::floor(t.wallH * 0.48f);
  for (float x = t.cx - t.r + 0.5f; x < t.cx + t.r; x += 1.0f) {
    float tt = (x - t.cx) / t.r;
    auto q = onFront(x, zc);
    P.put(q.first, q.second, kWood[tt < -0.3f ? 3 : (tt < 0.5f ? 2 : 1)], 3);
    q = onFront(x, zc - 1);
    uint32_t c = P.c.get(q.first, q.second);
    if (chA(c)) P.put(q.first, q.second, darken(c, 0.25f), 1);
  }
  // small shuttered windows: one in the loft, one lighting the stair beside the door
  auto window = [&](float xc, float z0, int hgt) {
    for (int j = 0; j < hgt; j++)
      for (int k = -2; k <= 2; k++) {
        auto q = onFront(xc + k + 0.5f, z0 + hgt - 1 - j);
        uint32_t c = (k == -2 || k == 2 || j == 0) ? kWood[k == -2 ? 3 : 1] : (j < 2 ? kGlass[3] : kGlass[1]);
        P.put(q.first, q.second, c, 3, !(k == -2 || k == 2 || j == 0));
      }
    for (int k = -3; k <= 3; k++) { auto q = onFront(xc + k + 0.5f, z0 - 1); P.put(q.first, q.second, kWood[k < 0 ? 3 : 2], 3); }
  };
  window(t.cx + 2, zc + 6, 7);
  if (t.r >= 12) window(t.cx - t.r * 0.6f, 8, 5);
  p.rowsShown |= 3u;
  // the door at the foot
  const int dw = 9, dh = 14;
  for (int z = 0; z < dh; z++)
    for (int i = -dw / 2 - 1; i <= dw / 2; i++) {
      auto q = at((float)p.doorX + i + 0.5f, t.cy + t.r - 0.5f, (float)z);
      bool frame = i == -dw / 2 - 1 || i == dw / 2 || z == dh - 1;
      float ax = (i + 0.5f) / (dw * 0.5f), ay = (z - (dh - 5)) / 5.0f;
      if (z > dh - 5 && ax * ax + ay * ay > 1.0f) continue;
      P.put(q.first, q.second, frame ? kWood[3] : kWoodDark[(i % 3 == 0) ? 1 : 2], 3);
    }
  // the sails
  auto hub = at(t.cx, t.cy + t.r * 0.55f, t.zTop() + 5);
  const float hx = (float)hub.first + 0.5f, hy = (float)hub.second + 0.5f;
  const float L = std::min(41.0f, hy - 2.0f);
  const float a0 = 0.30f + (float)(p.seed % 5) * 0.11f;   // where the sails stopped turning
  for (int k = 0; k < 4; k++) {
    const float a = a0 + k * 1.5707963f;
    const float dx = std::cos(a), dy = std::sin(a), nx = -dy, ny = dx;   // n: the trailing side
    const bool lit = dx + dy < 0.2f;                                     // facing up-left: in the sun
    // the sail: a lattice frame on the trailing side of the stock from a fifth of the way out to the tip, spread with
    // canvas (lit on the arms facing the sun), its cross bars and outer frame in timber
    const float u0 = L * 0.2f;
    for (float u = u0; u <= L; u += 0.5f)
      for (float v = 1.0f; v <= 6.5f; v += 0.5f) {
        const int px = (int)std::floor(hx + dx * u + nx * v), py = (int)std::floor(hy + dy * u + ny * v);
        const bool frame = v > 5.9f || u > L - 0.9f || u < u0 + 0.6f;
        const bool bar = std::fmod(u - u0, 3.0f) < 0.6f && v > 1.4f;
        uint32_t c;
        if (frame) c = lit ? kWood[1] : kWoodDark[1];
        else if (bar) c = kCloth[lit ? 1 : 0];
        else c = kCloth[lit ? (v < 4.0f ? 3 : 2) : (v < 4.0f ? 2 : 1)];
        P.put(px, py, c, 3);
      }
    // the stock (the arm), drawn over the sail's leading edge
    for (float u = 0; u <= L + 1; u += 0.5f) {
      int px = (int)std::floor(hx + dx * u), py = (int)std::floor(hy + dy * u);
      P.put(px, py, kWood[lit ? 2 : 1], 3);
      P.put((int)std::floor(hx + dx * u - ny * 0.9f), (int)std::floor(hy + dy * u + nx * 0.9f), kWoodDark[1], 3);
    }
  }
  for (int j = -2; j <= 2; j++)
    for (int i = -2; i <= 2; i++) {
      if (i * i + j * j > 5) continue;
      P.put(hub.first + i, hub.second + j, i + j < -1 ? kWood[4] : (i + j > 1 ? kWoodDark[1] : kWood[2]), 3);
    }
  P.put(hub.first, hub.second, kIron[3], 3);
}

// ---------------------------------------------------------------- M3: the culture's ornament
// the ridge of a box mass: its two ends (x, y at the ridge, z on it); alongY ridges run north-south
struct Ridge { float xa, ya, xb, yb, z; bool ok; };
Ridge ridgeOf(const Mass& m) {
  Ridge r{0, 0, 0, 0, 0, false};
  if (m.round || m.chimney || flatShape(m.shape) || m.found) return r;
  Surf s;
  if (!m.alongY) {
    const float y = (m.y0 - m.ov + m.y1 + m.ovF) * 0.5f;
    float zmax = -1;
    for (float x = m.x0 - m.ov + 0.5f; x < m.x1 + m.ov; x += 1.0f) zmax = std::max(zmax, massTop(m, x, y, s));
    float xa = 1e9f, xb = -1e9f;
    for (float x = m.x0 - m.ov + 0.5f; x < m.x1 + m.ov; x += 1.0f)
      if (massTop(m, x, y, s) > zmax - 0.8f) { xa = std::min(xa, x); xb = std::max(xb, x); }
    r = Ridge{xa, y, xb, y, zmax, xb >= xa};
  } else {
    const float x = (m.x0 + m.x1) * 0.5f;
    float zmax = -1;
    for (float y = m.y0 - m.ov + 0.5f; y < m.y1 + m.ovF; y += 1.0f) zmax = std::max(zmax, massTop(m, x, y, s));
    float ya = 1e9f, yb = -1e9f;
    for (float y = m.y0 - m.ov + 0.5f; y < m.y1 + m.ovF; y += 1.0f)
      if (massTop(m, x, y, s) > zmax - 0.8f) { ya = std::min(ya, y); yb = std::max(yb, y); }
    r = Ridge{x, ya, x, yb, zmax, yb >= ya};
  }
  return r;
}

template <class AT>
void cultureOverlays(Painter& P, AT&& at) {
  Plan& p = P.p;
  const Building b = p.b;
  const int D = p.D;
  const ArchStyle& st = p.st;
  const uint16_t orn = st.ornament;
  const int cul = p.cul;
  if (p.sc.ms.empty()) return;
  const Mass& body = p.sc.ms[(size_t)p.doorMass];
  const Ramp A = st.accentTint ? ramp(opaque(st.accentTint)) : kRed;
  const Ramp B = st.altTint ? ramp(opaque(st.altTint)) : kGold;
  auto putG = [&](std::pair<int, int> q, uint32_t c, bool glass = false) { P.put(q.first, q.second, c, 3, glass); };
  // ---- a columned porch before the door: columns, an architrave and a low pent roof (or the temple's portico)
  if (p.porch && !body.round) {
    const bool temple = b == Building::Temple;
    const int half = temple ? 22 : 11;
    const float x0 = (float)p.doorX - half, x1 = (float)p.doorX + half;
    const float yf = (float)D + 2.5f, zb = (float)p.zBase;
    const int ch = temple ? 22 : std::min(19, body.wallH - 6);
    const bool lacquer = cul == CU_JADE;
    const Ramp& C = lacquer ? kLacquer : (cul == CU_IMPERIAL || cul == CU_RIVER || cul == CU_STAR ? kAshlar : kWood);
    // the porch floor's shade on the ground and its roof's shade on the wall
    for (int x = (int)x0; x < (int)x1; x++)
      for (int k = 1; k <= 3; k++) {
        auto q = at(x + 0.5f, body.y1 - 0.5f, zb + ch - k);
        uint32_t c = P.c.get(q.first, q.second);
        if (chA(c) && P.layer[(size_t)q.second * P.c.w + q.first] == 1) P.c.set(q.first, q.second, darken(c, k == 1 ? 0.5f : 0.3f));
      }
    const int nCol = temple ? 6 : 2;
    for (int i = 0; i < nCol; i++) {
      const float cx = nCol == 2 ? (i == 0 ? x0 + 1.5f : x1 - 2.5f) : x0 + 2 + i * (x1 - x0 - 5) / (nCol - 1);
      for (int z = 0; z < ch; z++)
        for (int k = 0; k < 3; k++) {
          auto q = at(cx + k, yf, zb + z);
          int kk = k == 0 ? 4 : (k == 1 ? 2 : 1);
          if (z < 2 || z >= ch - 2) kk = std::min(4, kk + (z == 0 || z == ch - 1 ? 0 : 1));   // base and capital
          putG(q, C[kk]);
        }
    }
    // the architrave, and a pediment (temples, the imperial idiom) or a pent roof in the roof's material
    for (int x = (int)x0 - 1; x <= (int)x1; x++) {
      putG(at(x + 0.5f, yf, zb + ch), C[3]);
      putG(at(x + 0.5f, yf, zb + ch + 1), lacquer ? B[3] : C[4]);
    }
    const Ramp& R = body.rR;
    if (temple || cul == CU_IMPERIAL) {
      const int ph = temple ? 9 : 6;
      for (int x = (int)x0 - 2; x <= (int)x1 + 1; x++) {
        const float t = 1 - std::fabs((x + 0.5f - p.doorX) / (half + 2.0f));
        const int hh = (int)std::lround(ph * t);
        for (int j = 0; j <= hh; j++) putG(at(x + 0.5f, yf, zb + ch + 2 + j), j == hh ? R[x < p.doorX ? 4 : 2] : (j == 0 ? C[1] : C[(j + x) % 7 == 0 ? 2 : 3]));
      }
    } else {
      for (int x = (int)x0 - 2; x <= (int)x1 + 1; x++)
        for (int j = 0; j < 4; j++) {
          auto q = at(x + 0.5f, yf - j * 0.7f, zb + ch + 2 + j);
          putG(q, body.snow ? kSnow[3] : R[j == 0 ? 1 : (j == 3 ? 4 : 3)]);
        }
    }
  }
  // ---- the main ridge's ends: dragon heads (jade terraces, fjords), finials, carved crossed gable boards
  const Mass& main = p.sc.ms[0];
  const Ridge rg = ridgeOf(main);
  if (rg.ok && (orn & (ORN_DRAGON_HEADS | ORN_FINIALS | ORN_CARVED_RIDGE))) {
    const Ramp& R = main.rR;
    for (int e = 0; e < 2; e++) {
      const float x = e == 0 ? rg.xa : rg.xb, y = e == 0 ? rg.ya : rg.yb;
      const int dir = main.alongY ? 0 : (e == 0 ? -1 : 1);
      auto q = at(x, y, rg.z);
      if ((orn & ORN_DRAGON_HEADS) && dir != 0) {
        // a curled ridge-end beast: a neck rising out of the ridge, the head turned outward, a curl over it
        static const char* dg[6] = {"..xx.", ".x..x", "...xx", "..xxx", ".xxx.", "xxx.."};
        for (int j = 0; j < 6; j++)
          for (int i = 0; i < 5; i++) {
            if (dg[j][i] != 'x') continue;
            const int px = q.first + (dir < 0 ? -i : i) - dir * 1, py = q.second - 5 + j;
            P.put(px, py, (orn & ORN_GILDING) ? kGold[j < 2 ? 4 : 2] : R[j < 3 ? 4 : (i < 2 ? 3 : 1)], 3);
          }
      } else if ((orn & ORN_CARVED_RIDGE) && main.alongY && e == 1) {
        // crossed barge boards at the gable's peak, carved into curling heads (the fjords' gable ends)
        const auto top = at(x, main.y1 + main.ovF - 0.6f, rg.z);
        for (int k = 0; k < 6; k++) {
          P.put(top.first - 2 + k, top.second - k, kWood[k < 3 ? 3 : 4], 3);
          P.put(top.first + 2 - k, top.second - k, kWood[k < 3 ? 2 : 1], 3);
        }
        P.put(top.first + 4, top.second - 6, kWood[4], 3); P.put(top.first - 4, top.second - 6, kWood[2], 3);
        P.put(top.first + 5, top.second - 5, A[3], 3); P.put(top.first - 5, top.second - 5, A[2], 3);   // painted eyes
      } else if (orn & ORN_FINIALS) {
        for (int k = 1; k <= 4; k++) P.put(q.first, q.second - k, (orn & ORN_GILDING) ? kGold[k == 4 ? 4 : 2] : (k == 4 ? R[4] : kIron[2]), 3);
        P.put(q.first - 1, q.second - 3, (orn & ORN_GILDING) ? kGold[3] : kIron[3], 3);
      }
    }
  }
  // finials on cones, domes and onion bulbs (and the spire's gilded cross / star)
  if (orn & (ORN_FINIALS | ORN_GILDING))
    for (const Mass& m : p.sc.ms) {
      if (m.chimney || m.found) continue;
      const bool cone = m.round && (m.shape == RoofShape::Conical || m.shape == RoofShape::Onion || m.shape == RoofShape::Dome || m.shape == RoofShape::Pagoda || m.shape == RoofShape::Sweep);
      const bool bulb = !m.round && (m.shape == RoofShape::Onion || m.shape == RoofShape::Dome);
      if (!cone && !bulb) continue;
      if (m.rmat == RoofMat::Felt) continue;
      Surf s;
      const float cx = m.round ? m.cx : (m.x0 + m.x1) * 0.5f, cy = m.round ? m.cy : (m.y0 + m.y1) * 0.5f - 1;
      const float z = massTop(m, cx, cy, s);
      auto q = at(cx, cy, z);
      const bool gold = (orn & ORN_GILDING) != 0;
      for (int k = 1; k <= 5; k++) P.put(q.first, q.second - k, gold ? kGold[k >= 4 ? 4 : 2] : kIron[k >= 4 ? 3 : 1], 3);
      P.put(q.first - 1, q.second - 4, gold ? kGold[3] : kIron[2], 3);
      P.put(q.first + 1, q.second - 4, gold ? kGold[1] : kIron[1], 3);
    }
  // ---- lanterns: paper lanterns under the eaves (jade, marsh, sun), elven glow-orbs, iron lamps elsewhere
  if ((orn & ORN_LANTERNS) && !body.round) {
    const bool elf = cul == CU_SYLVAN || cul == CU_STAR;
    const bool paper = cul == CU_JADE || cul == CU_MARSH || cul == CU_SUN || cul == CU_IMPERIAL;
    const float ly = body.y1 + std::max(1, body.ovF) - 0.5f;
    const float lz = (float)(body.zBase + body.wallH) - 1;
    const float xs[3] = {body.x0 + 3, body.x1 - 4, (float)p.doorX + (b == Building::House ? 9.0f : 12.0f)};
    for (int i = 0; i < 3; i++) {
      const float lx = xs[i];
      if (lx < body.x0 + 1 || lx > body.x1 - 2) continue;
      auto q = at(lx, ly, lz);
      P.put(q.first, q.second, kIron[1], 3);   // the cord
      P.put(q.first, q.second + 1, kIron[1], 3);
      if (elf) {
        P.put(q.first, q.second + 2, kGlowElf[3], 3, true); P.put(q.first - 1, q.second + 3, kGlowElf[3], 3, true);
        P.put(q.first, q.second + 3, kGlowElf[4], 3, true); P.put(q.first + 1, q.second + 3, kGlowElf[2], 3, true);
        P.put(q.first, q.second + 4, kGlowElf[2], 3, true);
      } else if (paper) {
        const Ramp& L = st.accentTint ? A : kLacquer;
        for (int j = 0; j < 5; j++)
          for (int k = -1; k <= 1; k++) {
            const bool cap = j == 0 || j == 4;
            P.put(q.first + k, q.second + 2 + j, cap ? kWoodDark[1] : L[k < 0 ? 4 : (k > 0 ? 2 : 3)], 3, !cap);
          }
        P.put(q.first - 2, q.second + 4, L[3], 3, true); P.put(q.first + 2, q.second + 4, L[1], 3, true);
      } else {
        for (int j = 0; j < 4; j++) { P.put(q.first - 1, q.second + 2 + j, kIron[1], 3); P.put(q.first + 1, q.second + 2 + j, kIron[0], 3); P.put(q.first, q.second + 2 + j, j == 0 ? kIron[2] : kGlow[3], 3, j > 0); }
      }
    }
  }
  // ---- prayer flags: strings of small flags from the roof's peak to the eave corners, sagging between
  if ((orn & ORN_PRAYER_FLAGS) && (rg.ok || main.round)) {
    static const uint32_t fc[5] = {rgba(56, 96, 196), rgba(240, 240, 232), rgba(206, 52, 46), rgba(70, 150, 74), rgba(240, 200, 60)};
    Surf s;
    const float px = main.round ? main.cx : (rg.xa + rg.xb) * 0.5f, py = main.round ? main.cy : rg.ya;
    const float pz = main.round ? massTop(main, main.cx, main.cy, s) : rg.z;
    auto top = at(px, py, pz + 3);
    const float ends[2][2] = {{main.round ? main.cx - main.r - 2 : main.x0 - 3, (float)D + 1}, {main.round ? main.cx + main.r + 2 : main.x1 + 2, (float)D + 1}};
    for (int e = 0; e < 2; e++) {
      auto en = at(ends[e][0], ends[e][1], (float)p.zBase + 12);
      const int n = std::abs(en.first - top.first);
      for (int i = 0; i <= n; i++) {
        const float t = n ? (float)i / n : 0;
        const int xx = top.first + (en.first > top.first ? i : -i);
        const int yy = (int)std::lround(top.second + (en.second - top.second) * t + 4 * t * (1 - t) * 4);
        P.put(xx, yy, kCloth[1], 3);
        if (i % 3 == 1 && i > 1 && i < n - 1) {
          const uint32_t c = fc[(i / 3 + e * 2) % 5];
          P.put(xx, yy + 1, c, 3); P.put(xx, yy + 2, darken(c, 0.25f), 3); P.put(xx + (e ? 1 : -1), yy + 1, darken(c, 0.15f), 3);
        }
      }
      for (int k = 0; k < 4; k++) P.put(top.first, top.second + k, kWood[2], 3);   // the pole at the peak
    }
  }
  // ---- climbing vines (ORN_VINES): ivy up the walls from the ground, thickest at the corners
  if (orn & ORN_VINES)
    for (const Mass& m : p.sc.ms) {
      if (m.chimney || m.found || m.tier || m.round) continue;
      const int w = (int)std::lround(m.x1 - m.x0);
      for (int k = 0; k < 4; k++) {
        const int u = k == 0 ? 1 : (k == 1 ? w - 3 : 6 + (int)(hash3(k, 61, p.seed) % (uint32_t)std::max(1, w - 12)));
        const int hgt = (int)(m.wallH * (k < 2 ? 0.85f : 0.45f + 0.1f * (hash3(k, 63, p.seed) % 3)));
        for (int z = 0; z < hgt; z++) {
          const int wob = (int)std::lround(std::sin(z * 0.5f + k) * 1.2f);
          for (int dx = -1; dx <= 1; dx++) {
            if (hash3(u + dx, z, p.seed + 67) % 3 == 0) continue;
            auto q = at(m.x0 + u + dx + wob + 0.5f, m.y1 - 0.5f, (float)(m.zBase + z));
            if (P.layer[(size_t)std::clamp(q.second, 0, P.c.h - 1) * P.c.w + std::clamp(q.first, 0, P.c.w - 1)] != 1) continue;
            putG(q, kLeaf[(dx < 0 ? 3 : 2) - (z % 4 == 0 ? 1 : 0)]);
          }
        }
      }
    }
  // ---- the wood elves' root buttresses: the walls' trunks flare into roots that grip the ground
  if (cul == CU_SYLVAN)
    for (const Mass& m : p.sc.ms) {
      if (m.wall != WallKind::Living || m.chimney || m.found || m.tier || m.zBase > p.zBase) continue;
      const float x0 = m.round ? m.cx - m.r : m.x0, x1 = m.round ? m.cx + m.r : m.x1;
      for (float rx = x0 + 1; rx < x1 - 1; rx += 9 + (float)(hash3((int)rx, 71, p.seed) % 5)) {
        if (std::fabs(rx - p.doorX) < 9) continue;
        const float fy = m.round ? m.cy + std::sqrt(std::max(0.0f, m.r * m.r - (rx - m.cx) * (rx - m.cx))) : m.y1;
        for (int k = 0; k < 4; k++)
          for (int z = 0; z <= 5 - k; z++) {
            putG(at(rx - k * 0.6f + 0.5f, fy + k * 0.8f, (float)(m.zBase + z)), kLiving[k == 0 ? 3 : 2]);
            putG(at(rx + 1.5f + k * 0.6f, fy + k * 0.8f, (float)(m.zBase + z)), kLiving[1]);
          }
      }
    }
  // ---- the sacred tree rising through a wood-elf shrine's roof
  if (cul == CU_SYLVAN && b == Building::Temple && main.round) {
    Surf s;
    const float zr = massTop(main, main.cx, main.cy, s);
    auto base = at(main.cx, main.cy, zr);
    const int topLim = p.sc.top - p.budget + 3;
    const int cy = std::max(topLim + 14, base.second - 22);
    for (int y = base.second; y >= cy; y--) { P.put(base.first - 1, y, kLiving[3], 3); P.put(base.first, y, kLiving[2], 3); P.put(base.first + 1, y, kLiving[1], 3); }
    for (int j = -14; j <= 12; j++)
      for (int i = -22; i <= 22; i++) {
        const float dx = i / 22.0f, dy = j / 13.0f;
        const float wob = 0.12f * std::sin(i * 0.7f + j * 0.3f) + 0.1f * std::sin(i * 0.31f - j * 0.9f);
        if (dx * dx + dy * dy > 1.0f + wob) continue;
        const int yy = cy + j;
        if (yy < topLim) continue;
        const float l = -dx * 0.6f - dy * 0.7f + (hashf(i / 2, j / 2, p.seed) - 0.5f) * 0.5f;
        const int k = l > 0.55f ? 4 : (l > 0.15f ? 3 : (l > -0.35f ? 2 : 1));
        P.put(base.first + i, yy, (hash3(i, j, p.seed + 3) % 37 == 0) ? rgba(236, 196, 90) : kLeafRoof[k], 3);
      }
    for (int k = 0; k < 7; k++) {   // glow-motes in the crown
      const int gx = base.first - 16 + (int)(hash3(k, 91, p.seed) % 32), gy = cy - 8 + (int)(hash3(k, 92, p.seed) % 16);
      if (gy >= topLim) P.put(gx, gy, kGlowElf[4], 3, true);
    }
  }
}

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
  } else if (p.fnd != Foundation::None && p.zBase > 0) {
    // M3 the steps up the foundation to the door: treads lit, risers in shade, a cheek wall either side
    const Mass* f = nullptr;
    for (const Mass& m : p.sc.ms) if (m.found) f = &m;
    const Ramp& S = f ? f->wR : kStone;
    const int h = p.zBase, n = (h + 1) / 2;
    const float y0 = (float)D, y1 = (float)D + 3.9f, dd = (y1 - y0) / n;
    const int sw = b == Building::Palace || b == Building::Temple || b == Building::Keep ? 9 : 6;
    for (int s = 0; s < n; s++) {
      const float zt = (float)h - s * ((float)h / n), zb = zt - (float)h / n;
      for (int i = -sw; i < sw; i++) {
        const float x = p.doorX + i + 0.5f;
        const bool cheek = i == -sw || i == sw - 1;
        for (float yy = y0 + s * dd; yy < y0 + (s + 1) * dd; yy += 0.5f) {
          auto q = at(x, yy, cheek ? (float)h - s * 0.5f : zt);
          P.put(q.first, q.second, cheek ? S[i < 0 ? 4 : 2] : S[i == -sw + 1 ? 4 : 3], 3);
        }
        for (float z = zt - 1; z >= std::max(0.0f, zb); z -= 1.0f) {
          auto q = at(x, y0 + (s + 1) * dd - 0.25f, z);
          P.put(q.first, q.second, cheek ? S[i < 0 ? 3 : 1] : S[1], 3);
        }
      }
    }
  } else if (b != Building::Farmhouse) {
    // a stone step before the door
    for (int i = -6; i <= 5; i++) {
      auto q = at(p.doorX + i + 0.5f, D + 0.5f, 0);
      P.put(q.first, q.second, kStone[i == -6 ? 4 : 3], 3);
      P.put(q.first, q.second + 1, kStone[1], 3);
    }
  }
  // M0b: the ground under a jettied floor (in front of the set-back ground floor and at its sides) lies in its shade
  for (const Mass& m : p.sc.ms) {
    if (!m.jetty) continue;
    for (int x = (int)m.x0; x < (int)m.x1; x++)
      for (int z = -1; z <= m.jettyV; z++) {
        auto q = at(x + 0.5f, m.y1 - 0.5f, (float)(m.zBase + z));
        if (!chA(P.c.get(q.first, q.second))) P.put(q.first, q.second, rgba(30, 26, 40, 170), 3);
      }
  }
  // M0b: 2-storey variety: a timber balcony on the upper floor over the door, or a little hood roof over the door
  if (p.balcony || p.hood) {
    const Mass& m = p.sc.ms[p.doorMass];
    const float y1 = m.y1;
    if (p.balcony) {
      const float zf = (float)(m.zBase + m.floorV + (m.jetty ? 3 : 2));
      const float bx0 = (float)p.doorX - 12, bx1 = (float)p.doorX + 12;
      const float dep = 4;
      // its shadow on the wall below, falling down-right
      for (int x = (int)bx0 + 1; x < (int)bx1 + 2; x++)
        for (int k = 1; k <= 4; k++) {
          auto q = at(x + 0.5f, y1 - 0.5f, zf - k);
          uint32_t c = P.c.get(q.first, q.second);
          if (chA(c) && P.layer[(size_t)q.second * P.c.w + q.first] == 1) P.c.set(q.first, q.second, darken(c, k <= 2 ? 0.5f : 0.3f));
        }
      // brackets
      for (int s = 0; s < 2; s++) {
        float x = s == 0 ? bx0 + 2 : bx1 - 3;
        for (int k = 0; k <= 4; k++) {
          auto q = at(x + 0.5f, y1 + k * dep / 5.0f, zf - 5 + k);
          P.put(q.first, q.second, kWood[s == 0 ? 3 : 2], 3);
          P.put(q.first + 1, q.second, kWood[1], 3);
        }
      }
      // the deck: planks seen from above, then its front board
      for (int x = (int)bx0; x < (int)bx1; x++) {
        for (float yy = y1; yy < y1 + dep; yy += 0.5f) {
          auto q = at(x + 0.5f, yy, zf);
          P.put(q.first, q.second, (x - (int)bx0) % 3 == 0 ? kWoodDark[1] : kWoodDark[2], 3);   // the floor, seen between the balusters
        }
        auto q = at(x + 0.5f, y1 + dep - 0.5f, zf);
        P.put(q.first, q.second + 1, kWood[1], 3);
        P.put(q.first, q.second + 2, kWood[0], 3);
      }
      // the railing: posts at the corners, balusters, a lit hand rail
      for (int x = (int)bx0; x < (int)bx1; x++) {
        bool post = x == (int)bx0 || x == (int)bx1 - 1;
        bool bal = post || ((x - (int)bx0) % 3 == 1);
        for (int z = 1; z <= 5; z++) {
          auto q = at(x + 0.5f, y1 + dep - 0.5f, zf + z);
          if (bal) P.put(q.first, q.second, kWood[post ? (x == (int)bx0 ? 4 : 1) : 3], 3);
        }
        auto q = at(x + 0.5f, y1 + dep - 0.5f, zf + 6);
        P.put(q.first, q.second, kWood[4], 3);
        P.put(q.first, q.second + 1, kWood[1], 3);
      }
      // the rail's side runs back to the wall
      for (int s = 0; s < 2; s++) {
        float x = s == 0 ? bx0 : bx1 - 1;
        for (float yy = y1; yy < y1 + dep; yy += 0.5f) {
          auto q = at(x + 0.5f, yy, zf + 6);
          P.put(q.first, q.second, kWood[s == 0 ? 4 : 2], 3);
        }
      }
    } else {
      // the hood: a short pent roof on two brackets over the door, in the roof's material, with its shadow
      const float zh = (float)(m.zBase + 19), hx0 = (float)p.doorX - 9, hx1 = (float)p.doorX + 9, dep = 6;
      const Ramp& R = m.rR;
      for (int x = (int)hx0 + 1; x < (int)hx1 + 2; x++)
        for (int k = 1; k <= 3; k++) {
          auto q = at(x + 0.5f, y1 - 0.5f, zh - k);
          uint32_t c = P.c.get(q.first, q.second);
          if (chA(c) && P.layer[(size_t)q.second * P.c.w + q.first] == 1) P.c.set(q.first, q.second, darken(c, k == 1 ? 0.55f : 0.3f));
        }
      for (int s = 0; s < 2; s++) {
        float x = s == 0 ? hx0 + 1 : hx1 - 2;
        for (int k = 0; k <= 3; k++) { auto q = at(x + 0.5f, y1 + k, zh - 5 + k); P.put(q.first, q.second, kWood[s == 0 ? 3 : 1], 3); }
      }
      for (int x = (int)hx0; x < (int)hx1; x++) {
        int lastRow = -1;
        for (float yy = y1 - 0.5f; yy < y1 + dep; yy += 0.25f) {
          float z = zh + 2 - (yy - y1) * 0.55f;
          auto q = at(x + 0.5f, yy, z);
          if (q.second == lastRow) continue;
          lastRow = q.second;
          float tt = (yy - y1) / dep;   // 0 at the wall, 1 at the eave
          int k = tt < 0.3f ? 4 : (tt < 0.75f ? 3 : 2);
          if (m.rmat != RoofMat::Thatch && (x - (int)hx0) % 4 == 3) k = std::max(1, k - 1);   // courses down the slope
          if (x == (int)hx0) k = 4;
          if (x == (int)hx1 - 1) k = 1;
          if (m.rmat == RoofMat::Thatch && ((x + q.second) & 1)) k = std::max(1, k - 1);
          P.put(q.first, q.second, m.snow ? kSnow[std::min(4, k + 1)] : R[k], 3);
        }
        auto q = at(x + 0.5f, y1 + dep - 0.5f, zh + 2 - dep * 0.55f);
        P.put(q.first, q.second + 1, R[0], 3);
      }
    }
  }
  const float frontZ0 = (float)p.zBase;
  // signs
  auto sign = [&](float x, float z, int icon, float sy0 = -1e9f) {
    auto q = at(x, sy0 > -1e8f ? sy0 : D + 1.5f, z);
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
      case 3:   // M1 economy: a loaf (bakery)
        for (int i = -1; i <= 4; i++) P.put(ix + i, iy + 2, kGold[1], 3);
        for (int i = -1; i <= 4; i++) P.put(ix + i, iy + 1, kGold[i < 1 ? 3 : 2], 3);
        for (int i = 0; i <= 3; i++) P.put(ix + i, iy, kGold[i < 2 ? 4 : 3], 3);
        P.put(ix, iy + 1, kGold[4], 3); P.put(ix + 2, iy + 1, kGold[1], 3);
        break;
      case 4:   // a ham on the bone (butcher)
        for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) P.put(ix + i, iy + j, kRed[j == 0 && i == 0 ? 4 : (i == 2 || j == 2 ? 1 : 3)], 3);
        P.put(ix + 3, iy + 2, kBone[3], 3); P.put(ix + 4, iy + 3, kBone[4], 3); P.put(ix + 3, iy + 3, kBone[2], 3);
        break;
      case 5:   // a fish (fishmonger)
        for (int i = -1; i <= 3; i++) { P.put(ix + i, iy + 1, kIron[3], 3); P.put(ix + i, iy + 2, kIron[2], 3); }
        P.put(ix, iy, kIron[4], 3); P.put(ix + 1, iy, kIron[3], 3);
        P.put(ix + 4, iy, kIron[2], 3); P.put(ix + 4, iy + 1, kIron[1], 3); P.put(ix + 4, iy + 2, kIron[1], 3); P.put(ix + 4, iy + 3, kIron[2], 3);
        P.put(ix - 1, iy + 1, kInk, 3);
        break;
      case 6:   // a ball of yarn and its spindle (weaver)
        for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) if (!((i == 0 || i == 3) && (j == 0 || j == 3))) P.put(ix + i, iy + j, kPurple[(i + j) % 2 ? 3 : 2], 3);
        P.put(ix + 1, iy, kPurple[4], 3); P.put(ix + 4, iy + 3, kWood[3], 3); P.put(ix + 5, iy + 3, kWood[2], 3);
        break;
      case 7:   // a stretched hide (tannery)
        for (int j = 0; j < 5; j++) {
          int hw = j == 0 || j == 4 ? 1 : 2;
          for (int i = -hw; i <= hw; i++) P.put(ix + 1 + i, iy - 1 + j, kLeather[i < 0 ? 4 : (i > 0 ? 2 : 3)], 3);
        }
        break;
      case 8:   // an ingot (smelter)
        for (int i = 0; i <= 3; i++) P.put(ix + i, iy + 1, kGold[4], 3);
        for (int i = -1; i <= 4; i++) P.put(ix + i, iy + 2, kGold[3], 3);
        for (int i = -1; i <= 4; i++) P.put(ix + i, iy + 3, kGold[1], 3);
        break;
      case 9:   // a sheaf of wheat (granary)
        for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++) if (j >= 2 || (i + j) % 2 == 0) P.put(ix - 1 + i, iy - 1 + j, kGold[j < 2 ? 4 : (j == 3 ? 1 : 3)], 3);
        break;
      case 10:   // a frame saw (sawmill)
        for (int i = -1; i <= 4; i++) P.put(ix + i, iy + 1, kIron[3], 3);
        for (int i = -1; i <= 4; i += 2) P.put(ix + i, iy + 2, kIron[2], 3);
        P.put(ix - 2, iy, kWood[3], 3); P.put(ix - 2, iy + 1, kWood[3], 3); P.put(ix + 5, iy, kWood[2], 3); P.put(ix + 5, iy + 1, kWood[2], 3);
        for (int i = -2; i <= 5; i++) P.put(ix + i, iy - 1, kWood[2], 3);
        break;
      case 11:   // a sack of flour (mills)
        for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) P.put(ix + i, iy + j, kCloth[i == 0 ? 4 : (i == 3 || j == 3 ? 2 : 3)], 3);
        P.put(ix + 1, iy - 1, kCloth[3], 3); P.put(ix + 2, iy - 1, kCloth[2], 3); P.put(ix + 1, iy, kWood[1], 3); P.put(ix + 2, iy, kWood[1], 3);
        break;
      default:  // coin pouch
        for (int j = 1; j < 4; j++) for (int i = 0; i < 4; i++) P.put(ix + i, iy + j, kLeather[3 - (i == 3)], 3);
        P.put(ix + 1, iy, kLeather[2], 3); P.put(ix + 2, iy, kLeather[2], 3); P.put(ix + 1, iy + 2, kGold[4], 3);
        break;
    }
  };
  // M1 economy: the trade's sign
  int tradeIcon = 2;
  switch (p.trade) {
    case Building::Bakery: tradeIcon = 3; break;
    case Building::Butcher: tradeIcon = 4; break;
    case Building::Fishmonger: tradeIcon = 5; break;
    case Building::Weaver: tradeIcon = 6; break;
    case Building::Tanner: tradeIcon = 7; break;
    case Building::Smelter: tradeIcon = 8; break;
    case Building::Granary: tradeIcon = 9; break;
    case Building::Sawmill: tradeIcon = 10; break;
    case Building::Watermill: case Building::Windmill: tradeIcon = 11; break;
    default: break;
  }
  if (p.trade == Building::Granary || p.trade == Building::Sawmill || p.trade == Building::Tanner || p.trade == Building::Watermill)
    sign((float)p.doorX + 10, frontZ0 + 20, tradeIcon);
  if (b == Building::Inn) sign((float)p.doorX + 9, frontZ0 + 26, 0);
  if (b == Building::Smithy) sign((float)p.doorX - 21, frontZ0 + 22, p.trade == Building::Smelter ? 8 : 1);
  // lanterns by the door
  if (b == Building::Inn || b == Building::Keep || b == Building::Temple || b == Building::Palace || b == Building::Barracks) {
    for (int s = -1; s <= 1; s += 2) {
      auto q = at((float)p.doorX + s * (b == Building::Keep ? 11 : (b == Building::Palace ? 13 : 9)), D + 0.5f, frontZ0 + (b == Building::Palace ? 18 : 15));
      P.put(q.first, q.second - 1, kIron[2], 3);
      P.put(q.first, q.second, kGlow[4], 3);
      P.put(q.first, q.second + 1, kGlow[2], 3);
      P.put(q.first - 1, q.second, kIron[1], 3);
      P.put(q.first + 1, q.second, kIron[1], 3);
    }
  }
  // awnings: a striped canopy over the shop front, plain cloth over desert doors and windows
  if (b == Building::Shop || ((p.st.awnings || (p.st.ornament & ORN_AWNINGS)) && b != Building::Keep && b != Building::Tower && (p.cul == CU_NONE || (b != Building::Palace && b != Building::Temple)))) {
    const Mass& m = p.sc.ms[p.doorMass];
    bool stripes = b == Building::Shop;
    static const Ramp* cloths[3] = {&kRed, &kCloth, &kPurple};
    const Ramp A = (p.cul && (p.st.accentTint || p.st.altTint)) ? ramp(opaque((hash3(4, 4, p.seed) & 1) && p.st.altTint ? p.st.altTint : (p.st.accentTint ? p.st.accentTint : p.st.altTint)))
                                                                : *cloths[hash3(4, 4, p.seed) % 3];   // (M3: the culture's cloth)
    // M1 economy: each trade's awning in its own colours (the general store keeps red and white)
    const Ramp SR = p.trade == Building::Bakery ? ramp(rgba(206, 150, 56)) : p.trade == Building::Fishmonger ? ramp(rgba(56, 96, 168))
                  : p.trade == Building::Weaver ? kPurple : p.trade == Building::Butcher ? ramp(rgba(150, 40, 44)) : kRed;
    float ax0 = b == Building::Shop ? m.x0 + 3 : p.doorX - 9.0f, ax1 = b == Building::Shop ? m.x1 - 3 : p.doorX + 9.0f;
    float az = frontZ0 + (b == Building::Shop ? 26.0f : 19.0f);   // the shop canopy hangs above its windows
    if (m.floorV > 0) az = frontZ0 + (float)std::min(m.floorV - 1, b == Building::Shop ? 26 : 19);   // M0b: under the floor line
    // M3 fixer: a round mass (yurt, tower) has a curved front that stands back from the plot edge and a low wall: the
    // canopy is a short hood over the door that follows the curve, hung under the eave, its posts and sign against it
    const bool rnd = m.round;
    if (rnd) {
      const float hw = std::min(b == Building::Shop ? 10.0f : 8.0f, m.r * 0.5f);
      ax0 = (float)p.doorX - hw; ax1 = (float)p.doorX + hw;
    }
    az = std::min(az, frontZ0 + (float)std::max(14, m.wallH - 3));   // never above the wall it hangs on
    const float fyc = rnd ? std::min((float)D, m.frontY((float)p.doorX + 0.5f)) : (float)D;   // straight, from the door's wall
    auto fy = [&](float) { return fyc; };
    for (int x = (int)ax0; x < (int)ax1; x++)
      for (int k = 0; k < 6; k++) {
        auto q = at(x + 0.5f, fy(x + 0.5f) + 0.5f + k * 0.5f, az - k);
        bool st = ((x / 4) & 1) == 0;
        const Ramp& R = stripes ? (st ? SR : kCloth) : A;
        int kk = k == 0 ? 4 : (k < 4 ? 3 : 2);
        P.put(q.first, q.second, R[kk], 3);
      }
    for (int x = (int)ax0; x < (int)ax1; x++) {
      auto q = at(x + 0.5f, fy(x + 0.5f) + 3.5f, az - 6);
      if ((x & 3) == 1 || (x & 3) == 2) P.put(q.first, q.second, (stripes && ((x / 4) & 1) == 0) ? SR[1] : (stripes ? kCloth[1] : A[1]), 3);
    }
    // posts at the awning's ends (a round wall's hood rides on two iron brackets instead)
    if (rnd)
      for (int s = 0; s < 2; s++) {
        const float x = s == 0 ? ax0 + 1 : ax1 - 2;
        for (int k = 0; k <= 3; k++) { auto q = at(x + 0.5f, fyc + 0.5f + k * 0.75f, az - 9 + k * 0.75f); P.put(q.first, q.second, kIron[s == 0 ? 2 : 1], 3); }
      }
    else
    for (int s = 0; s < 2; s++) {
      float x = s == 0 ? ax0 : ax1 - 1;
      for (int z = 0; z < (int)az - 5; z++) {
        auto q = at(x + 0.5f, fy(x + 0.5f) + 3.5f, (float)z);
        P.put(q.first, q.second, kWood[s == 0 ? 3 : 1], 3);
      }
    }
    // the shop's sign projects on its bracket from the east awning post, in front of the canopy (painted after it)
    if (b == Building::Shop) sign(rnd ? ax1 : ax1 - 2, rnd ? az - 3 : az - 1, tradeIcon, rnd ? fyc + 0.5f : -1e9f);
  }
  // M1 kingdom identity (VISION_PLAN 15.8): the ruling kingdom's colours (BuildingFacts::banner, banner2, emblem; red and
  // gold where the facts carry none). hang: a banner hanging flat on a front wall (face at ground y fy) from height zTop,
  // wd x ht px, a gilded rod on top and a swallow-tail; flag: a pennant flying east from a pole planted at (x, y, z).
  const bool kc = p.facts.banner != 0;
  const Ramp BF = kc ? ramp(opaque(p.facts.banner)) : kRed;
  const Ramp BT = kc ? ramp(opaque(p.facts.banner2 ? p.facts.banner2 : rgba(232, 200, 90))) : kGold;
  const int em = kc ? p.facts.emblem : 0;
  auto hang = [&](float x, float fy, float zTop, int wd, int ht) {
    for (int j = 0; j < ht; j++)
      for (int i = 0; i < wd; i++) {
        int mid = wd / 2;
        if (j >= ht - 2 && (i == mid || i == mid - 1)) continue;   // the swallow-tail
        if (j == ht - 1 && (i == mid - 2 || i == mid + 1)) continue;
        auto q = at(x + i + 0.5f, fy, zTop - j);
        int k = i == 0 ? 3 : (i == wd - 1 ? 1 : 2);
        uint32_t c = BF[k];
        if (j == 0 || i == 0 || i == wd - 1) c = BT[i == wd - 1 ? 1 : 2];   // the trim
        int ci = i - (wd - 5) / 2, cj = j - 3;
        if (wd >= 6 && heraldry::chargeAt(em, 5, ci, cj)) c = BT[std::max(1, heraldry::chargeShade(em, 5, ci, cj) - (i == wd - 1 ? 1 : 0))];
        P.put(q.first, q.second, c, 3);
      }
    auto q = at(x + 0.5f, fy, zTop);
    for (int i = -1; i <= wd; i++) P.put(q.first + i, q.second - 1, kGold[i == -1 ? 4 : 3], 3);
    P.put(q.first - 2, q.second - 1, kGold[2], 3); P.put(q.first + wd + 1, q.second - 1, kGold[2], 3);
  };
  auto flag = [&](float x, float y, float z, int len) {
    for (int k = -2; k < 10; k++) { auto q = at(x, y, z + k); P.put(q.first, q.second, k == 9 ? kGold[4] : kWood[k & 1 ? 1 : 2], 3); }
    for (int j = 0; j < 5; j++)
      for (int i = 1; i < len - j / 2; i++) {
        auto q = at(x + i, y, z + 8 - j);
        int wv = (int)std::floor(std::sin(i * 0.9f + (float)(p.seed & 7)) * 0.8f + 0.5f);
        uint32_t c = BF[(i + j) % 3 == 0 ? 3 : 2];
        if (j == 0) c = BF[3];
        if (j == 4) c = BF[1];
        if (i >= 3 && i <= 4 && j >= 1 && j <= 3) c = BT[3];   // a fleck of the charge colour
        P.put(q.first, q.second + wv, c, 3);
      }
  };
  // banners on the keep's front, and a flag on its donjon
  if (b == Building::Keep) {
    for (int x = 22; x < p.W - 22; x += 24) {
      if (std::abs(x + 3 - p.doorX) < 14) continue;
      hang((float)x, D + 0.5f, 31.0f, 6, 16);
    }
    const Mass& dj = p.sc.ms[1];
    flag((dj.x0 + dj.x1) * 0.5f, (dj.y0 + dj.y1) * 0.5f, dj.zTop() + dj.roofH, 9);
  }
  // the palace: banners on the entrance tower beside the great door and on both wings, the royal standard over the
  // entrance, a flag on every tower
  if (b == Building::Palace) {
    const Mass& et = p.sc.ms[(size_t)p.doorMass];
    for (int s = -1; s <= 1; s += 2) hang((float)p.doorX + (s < 0 ? -18.0f : 12.0f), et.y1 - 0.5f, et.zBase + 44.0f, 6, 18);
    for (int k = 1; k <= 2; k++) {
      const Mass& wg = p.sc.ms[(size_t)k];
      int wdt = (int)std::lround(wg.x1 - wg.x0);
      float x = k == 1 ? wg.x0 + 16 : wg.x1 - 22;
      if (wdt >= 40) hang(x, wg.y1 - 0.5f, wg.zBase + 31.0f, 6, 16);
    }
    flag((et.x0 + et.x1) * 0.5f, (et.y0 + et.y1) * 0.5f, et.zTop() - 1 + et.roofH, 12);
    for (const Mass& t : p.sc.ms)
      if (t.round) flag(t.cx, t.cy, t.zTop() - 1 + t.roofH, 9);
  }
  // the barracks: the colours either side of the door, a shield over it
  if (b == Building::Barracks) {
    const Mass& m = p.sc.ms[(size_t)p.doorMass];
    for (int s = -1; s <= 1; s += 2) hang((float)p.doorX + (s < 0 ? -15.0f : 9.0f), m.y1 - 0.5f, frontZ0 + 31.0f, 6, 15);
    auto q = at((float)p.doorX - 3, m.y1 + 0.5f, frontZ0 + 25.0f);
    for (int j = 0; j < 7; j++)
      for (int i = 0; i < 6; i++) {
        if (j >= 5 && (i == 0 || i == 5)) continue;
        if (j == 6 && (i == 1 || i == 4)) continue;
        uint32_t c = (i == 0 || j == 0) ? BF[3] : (i == 5 || j == 6 ? BF[1] : BF[2]);
        if (heraldry::chargeAt(em, 5, i, j - 1)) c = BT[heraldry::chargeShade(em, 5, i, j - 1)];
        P.put(q.first + i, q.second + j, c, 3);
      }
  }
  // an inn in a kingdom flies its colours from the front wall, the other side of the door from its sign
  if (b == Building::Inn && kc) hang((float)p.doorX - 18, D + 0.5f, frontZ0 + 33.0f, 6, 13);
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
  if (b == Building::Tower && p.trade == Building::Windmill) windmillDress(P, at);
  if (b == Building::Tower && p.trade != Building::Windmill) {
    const Mass& t = p.sc.ms.back().round ? p.sc.ms.back() : p.sc.ms[p.doorMass];
    if (t.round) {
      // M0b: three storeys (workroom, library, the mage's chamber), each a band of the drum between string courses
      // that follow its curve, lit on the west and turning into shade on the east; an arcane window in each upper band
      auto onFront = [&](float x, float z) { float dx = x - t.cx; return at(x, t.cy + std::sqrt(std::max(0.0f, t.r * t.r - dx * dx)) - 0.5f, z); };
      const float band = t.wallH / 3.0f;
      for (int k = 1; k <= 2; k++) {
        float zc = std::floor(band * k);
        for (float x = t.cx - t.r + 0.5f; x < t.cx + t.r; x += 1.0f) {
          float tt = (x - t.cx) / t.r;
          int kk = tt < -0.35f ? 4 : (tt < 0.45f ? 3 : 2);
          auto q = onFront(x, zc + 1);
          P.put(q.first, q.second, kStone[kk], 3);
          q = onFront(x, zc);
          P.put(q.first, q.second, kStone[std::max(0, kk - 2)], 3);
          q = onFront(x, zc - 1);
          uint32_t c = P.c.get(q.first, q.second);
          if (chA(c)) P.put(q.first, q.second, darken(c, 0.3f), 1);
        }
      }
      auto arcane = [&](float xc, float z0, int hgt, int wdt) {
        for (int j = 0; j < hgt; j++)
          for (int k = 0; k < wdt; k++) {
            if (j == hgt - 1 && (k == 0 || k == wdt - 1)) continue;   // a pointed head
            auto q = onFront(xc - wdt * 0.5f + k + 0.5f, z0 + hgt - 1 - j);
            uint32_t c = j < 3 ? rgba(176, 156, 255) : rgba(112, 92, 224);
            if (k == 0) c = rgba(70, 54, 140);
            P.put(q.first, q.second, c, 3, true);
          }
        for (int k = -1; k <= wdt; k++) { auto q = onFront(xc - wdt * 0.5f + k + 0.5f, z0 - 1); P.put(q.first, q.second, kStone[4], 3); }
      };
      for (int k = 1; k <= 2; k++) {
        float zb = std::floor(band * k) + 2, room = band - 4;
        int hgt = std::clamp((int)room, 5, 8);
        arcane(t.cx, zb + std::max(0.0f, (room - hgt) * 0.5f), hgt, 4);
        p.rowsShown |= 1u << k;
      }
      if (t.r >= 12) arcane(t.cx - t.r * 0.62f, 6, 6, 2);   // a slit lighting the stair beside the door
      p.rowsShown |= 1u;   // the ground floor: the door
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
  if (p.cul != CU_NONE) cultureOverlays(P, at);
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

// outline() (art_internal.h) over the rows ya..yb-1, reading the untouched picture src: the same pixels as outline(c,
// strength) in one go, so the incremental paint can split it
void outlineRows(Canvas& c, const Canvas& src, float strength, int ya, int yb) {
  for (int y = std::max(0, ya); y < std::min(yb, c.h); y++)
    for (int x = 0; x < c.w; x++) {
      if (solid(src, x, y)) continue;
      static const int nx[4] = {0, -1, 1, 0}, ny[4] = {1, 0, 0, -1};
      int found = -1;
      for (int k = 0; k < 4; k++)
        if (chA(src.get(x + nx[k], y + ny[k])) > 96) { found = k; break; }
      if (found < 0) continue;
      uint32_t n = src.get(x + nx[found], y + ny[found]);
      float s = strength;
      if (found == 0 || found == 2) s *= 0.88f;
      c.set(x, y, outlineOf(n, s));
    }
}

}  // namespace

// ---------------------------------------------------------------- incremental paints (M3, owner carry-over 1)
// A building paint in resumable phases: the plan (masses), the facades (a mass at a time), the roofs and walls (a few
// columns at a time), the eave shadow and the outline (a band of rows at a time), the overlays (signs, banners,
// machinery) and the facts (glass, storeys, chimneys). buildingSprite is this job run in one go, so the pixels are the
// same whichever budget the steps are given.
struct BuildingJob {
  Building b = Building::House;
  int w = 1, h = 1;
  ArchStyle style;
  uint32_t seed = 0;
  BuildingFacts facts;
  int phase = 0, cursor = 0;
  std::unique_ptr<Plan> plan;
  std::unique_ptr<Painter> P;
  Canvas src;          // the picture before the outline (outlineRows reads it)
  BuildingInfo info;
  bool done = false;
  double worstStepMs = 0;   // the longest single unit of work so far (tests)
  int worstPhase = -1;
};

std::shared_ptr<BuildingJob> beginBuilding(Building b, int wTiles, int hTiles, const ArchStyle& style, uint32_t seed,
                                           const BuildingFacts& facts) {
  auto j = std::make_shared<BuildingJob>();
  j->b = paintedAs(b); j->w = wTiles; j->h = hTiles; j->style = style; j->seed = seed; j->facts = facts;
  return j;
}

namespace {
// one unit of work of the job (the phases above); false once everything is done
bool stepUnit(BuildingJob& j) {
  switch (j.phase) {
    case 0:   // the masses
      j.plan = std::make_unique<Plan>(makePlanMasses(j.b, j.w, j.h, j.style, j.seed, j.facts));
      j.phase = 8; j.cursor = 0;
      return true;
    case 8:   // each mass fitted under the rise budget
      if (j.cursor < (int)j.plan->sc.ms.size()) fitPlanMass(*j.plan, j.cursor);
      if (++j.cursor >= (int)j.plan->sc.ms.size()) j.phase = 9;
      return true;
    case 9:   // chimneys, dormers, the picture's height
      makePlanFinish(*j.plan);
      j.phase = 1; j.cursor = 0;
      return true;
    case 1:   // facades, two masses a step
      buildFacades(*j.plan, j.cursor, j.cursor + 2);
      j.cursor += 2;
      if (j.cursor >= (int)j.plan->sc.ms.size()) { j.phase = 2; j.cursor = -BLDG_PAD_X; j.P = std::make_unique<Painter>(*j.plan); }
      return true;
    case 2: {   // roofs and walls, column by column, back to front
      const int n = 3;
      renderColumns(*j.P, j.cursor, j.cursor + n);
      j.cursor += n;
      if (j.cursor >= j.plan->W + BLDG_PAD_X) { j.phase = 3; j.cursor = 0; }
      return true;
    }
    case 3: {
      eaveShadow(*j.P, j.cursor, j.cursor + 24);
      j.cursor += 24;
      if (j.cursor >= j.P->c.h) j.phase = 4;
      return true;
    }
    case 4:
      overlays(*j.P, &j.info);
      j.src = j.P->c;
      j.phase = 5; j.cursor = 0;
      return true;
    case 5: {
      outlineRows(j.P->c, j.src, 0.95f, j.cursor, j.cursor + 24);
      j.cursor += 24;
      if (j.cursor >= j.P->c.h) j.phase = 6;
      return true;
    }
    case 6: {
      Plan& p = *j.plan;
      Painter& P = *j.P;
      if (P.clipped && std::getenv("EMB_ARCH_CLIPLOG"))
        std::printf("buildingSprite: type %d %dx%d storeys %d: %d px cut at the rise budget\n", (int)j.b, j.w, j.h, p.facts.storeys, P.clipped);
      BuildingInfo& info = j.info;
      info.height = p.sc.top;
      info.storeys = 0;   // M0b: the storeys really painted (a door or windows below, a row of windows per floor above)
      for (int k = 0; k < 4; k++) if (p.rowsShown & (1u << k)) info.storeys++;
      info.chimneys = 0;
      for (const Mass& m : p.sc.ms) if (m.chimney) info.chimneys++;
      info.glass = std::move(P.glass);
      for (size_t i = 0; i < info.glass.size(); i++) if (info.glass[i] && !chA(P.c.px[i])) info.glass[i] = 0;
      j.src = Canvas();
      j.phase = 7;
      j.done = true;
      return false;
    }
    default: return false;
  }
}
}  // namespace

bool stepBuilding(BuildingJob& j, double budgetMs) {
  using Clock = std::chrono::steady_clock;
  const auto t0 = Clock::now();
  while (!j.done) {
    const auto u0 = Clock::now();
    const int ph = j.phase;
    stepUnit(j);
    const auto u1 = Clock::now();
    const double ms = std::chrono::duration<double, std::milli>(u1 - u0).count();
    if (ms > j.worstStepMs) { j.worstStepMs = ms; j.worstPhase = ph; }
    if (std::chrono::duration<double, std::milli>(u1 - t0).count() >= budgetMs) break;
  }
  return j.done;
}

Canvas finishBuilding(BuildingJob& j, BuildingInfo* info) {
  while (!j.done) stepUnit(j);
  if (info) *info = std::move(j.info);
  Canvas c = std::move(j.P->c);
  j.P.reset();
  j.plan.reset();
  return c;
}

double buildingJobWorstStepMs(const BuildingJob& j) {
  if (std::getenv("EMB_ARCH_DBG")) std::printf("  (worst step %.2f ms in phase %d)\n", j.worstStepMs, j.worstPhase);
  return j.worstStepMs;
}

Canvas buildingSprite(Building b, int wTiles, int hTiles, const ArchStyle& style, uint32_t seed, BuildingInfo* info) {
  return buildingSprite(b, wTiles, hTiles, style, seed, info, BuildingFacts{});
}
Canvas buildingSprite(Building b, int wTiles, int hTiles, const ArchStyle& style, uint32_t seed, BuildingInfo* info,
                      const BuildingFacts& facts) {
  BuildingJob j;
  j.b = paintedAs(b); j.w = wTiles; j.h = hTiles; j.style = style; j.seed = seed; j.facts = facts;
  while (stepUnit(j)) {}
  return finishBuilding(j, info);
}

Canvas buildingNight(const Canvas& sprite, const std::vector<uint8_t>& glass, uint32_t seed) {
  Canvas c = sprite;
  if (glass.size() != c.px.size()) return c;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      if (!glass[(size_t)y * c.w + x]) continue;
      // lamplight behind the panes: brightest low in the window (the lamp on the table), a warm-orange top where the
      // curtain hangs, a pane here and there a little dimmer; the glints stay the brightest pixels
      uint32_t old = c.get(x, y);
      int lum = (int)(old & 255) + (int)((old >> 8) & 255) + (int)((old >> 16) & 255);
      bool upper = y > 0 && !glass[(size_t)(y - 1) * c.w + x];
      int k = 3;
      if (upper) k = 2;
      if (hash3(x / 3, y / 3, seed) % 5 == 0) k--;
      if (lum > 500) k = 4;   // the glint
      c.set(x, y, kGlow[std::clamp(k, 1, 4)]);
    }
  return c;
}

Canvas buildingSprite(Building b, int wTiles, int hTiles, uint32_t roofColor, uint32_t seed) {
  ArchStyle st = withRoofTint(archForBiome(2, seed), roofColor);
  return buildingSprite(b, wTiles, hTiles, st, seed, nullptr);
}

int buildingHeight(Building b, int wTiles, int hTiles, const ArchStyle& style) {
  return buildingHeight(b, wTiles, hTiles, style, BuildingFacts{});
}
int buildingHeight(Building b, int wTiles, int hTiles, const ArchStyle& style, const BuildingFacts& facts) {
  b = paintedAs(b);
  Plan p = makePlan(b, wTiles, hTiles, style, 1u, facts);
  return p.sc.top;
}

}  // namespace art
