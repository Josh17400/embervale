// EMBERVALE art: buildings, roofs, city walls and the gate. See rpg/art.h for the contract and rpg/art/art_internal.h for the shared helpers.
//
// Buildings are built from masses (boxes and cylinders: the main house, wings, cross gables, dormers, chimneys,
// turrets, steeples) and painted as real volumes in the oblique 3/4 view: each mass's front wall is a facade canvas
// (material, windows, doors) and all roofs form one height field, rendered column by column from back to front with
// per-pixel normals lit from the top-left, material texels laid along the courses, cast shadows from chimneys and
// turrets, fascia boards on every eave, and the eave's shadow on the wall below. M3b: every mass is a volume of the
// builder's blueprint (rpg/build/blueprint.h: form, massing, roofs, materials, faces, details); the purpose adds only
// its signage and machinery (signs, sails), and the ArchStyle its palette and texture.
#include "rpg/art/art_internal.h"
#include "rpg/art/art_heraldry.h"
#include "rpg/build/blueprint.h"

#include <array>
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
  // M3b: the blueprint volume the mass was made from (rpg/build/blueprint.h)
  bld::VolRole role = bld::VolRole::Body;
  bld::Face face = bld::Face::Windows;
  uint32_t feat = 0;        // bld::VF_* bits
  int storeys = 1;          // storeys its walls show (a floor line and a row of openings each)
  int dormers = 0, gable = 0;
  int pitch = 2;            // the volume's pitch (0..8)
  bool hasDoor = false;     // the entrance is in this mass's front
  bool encl = false;        // an enclosure wall of a compound (low, a coping or a stockade's points on top)
  bool tree = false;        // a colossal living tree's trunk (its crown is an overlay)
  bool open = false;        // an open front (colonnade, arcade, veranda): the recess behind it in shade
  std::vector<bld::Pillar> pillars;   // (owner) an open face's pillars (bld::openPillars: the walking's), px in the frame
  float joinL = 0, joinR = 0;   // (fixer) the roof runs on this far (px) into the taller roof beside it, unhipped: the two
                                // meet in a valley (an L's wing), not a shaded hip end wedged against the other roof
  float zTop() const { return (float)(zBase + wallH); }
  float frontY(float x) const {
    if (!round) return (x >= x0 && x < x1) ? y1 - 0.5f : -1e9f;
    float dx = x - cx;
    return std::fabs(dx) < r ? cy + std::sqrt(r * r - dx * dx) - 0.5f : -1e9f;
  }
};

// (M3c carry) an elven cone: the swept leaf roof, and the sylvan cultures' plain cones (bark huts, pods) drawn the same
// way, a true cone on a foreshortened eave (massTop)
bool elvenCone(const Mass& m);
// M3: roofs that are a flat deck behind a parapet (a dome or an onion bulb may stand on it)
inline bool flatShape(RoofShape r) { return r == RoofShape::FlatParapet || r == RoofShape::Dome || r == RoofShape::Onion; }

// ---------------------------------------------------------------- roof height field
// Height of mass m's top at ground point (x, y), or -1 where it has none. surf/facet say what is there.
// (fix) a stepped pyramid's terraces: kStepTiers treads, each kStepTread of the half-span deep, then the summit
constexpr int kStepTiers = 4;
constexpr float kStepTread = 0.14f, kStepSummit = kStepTiers * kStepTread;
inline float stepLevel(float t) { return t >= kStepSummit ? 1.0f : std::floor(t / kStepTread) / (float)kStepTiers; }

bool elvenCone(const Mass& m) {
  return m.round && (m.shape == RoofShape::Sweep || (m.shape == RoofShape::Conical && m.cul == CU_SYLVAN && m.rmat != RoofMat::Felt));
}

// (M3c fixer round 3, review: "the round huts have a teardrop roof: convex flanks bulge wider than the drum wall below,
// then pinch to a point ... the garlic bulb silhouette") In this view a round eave is a circle on the screen, and a cone
// on a circle is a teardrop: the eave's round bottom and fat shoulders bulge out before the flanks run up to the point.
// The cone now stands on a flat ELLIPSE (half as deep as it is wide, like a painter's cone in a 3/4 view) that is pushed
// forward so its front still laps the drum's front edge by a pixel; the point stays over the drum's centre (an oblique
// cone), and the eave overhangs the drum by a lip of ~2.5 px to the sides instead of a broad bell. Seen from the 3/4
// view the silhouette is two straight flanks from the point to the eave's ends and a shallow lip under them.
// coneT: the cone's gauge from its point, 0 at the point .. 1 on the eave ellipse (> 1 outside)
struct ConeEave { float R, Ry, e; };
inline ConeEave coneEave(const Mass& m) {
  const float R = m.r + 2.5f;
  const float Ry = std::max(R * 0.55f, (m.r + 1.5f) / 1.8f);
  const float e = std::max(0.0f, m.r + 1.5f - Ry);   // the ellipse's centre in front of the point (ground px)
  return ConeEave{R, Ry, e};
}
inline float coneT(const Mass& m, float x, float y) {
  const ConeEave c = coneEave(m);
  const float dx = x - m.cx, dy = y - m.cy;
  // the point P + (dx, dy) / t lies on the ellipse ((X/R)^2 + ((Y - e)/Ry)^2 = 1): solve for u = 1/t
  const float a = dx * dx / (c.R * c.R) + dy * dy / (c.Ry * c.Ry);
  if (a < 1e-9f) return 0.0f;
  const float b = -2.0f * c.e * dy / (c.Ry * c.Ry), cc = c.e * c.e / (c.Ry * c.Ry) - 1.0f;
  const float u = (-b + std::sqrt(std::max(0.0f, b * b - 4.0f * a * cc))) / (2.0f * a);
  return u > 1e-6f ? 1.0f / u : 1e9f;
}

float massTop(const Mass& m, float x, float y, Surf& surf) {
  const float zt = m.zTop();
  surf = Surf::Roof;
  if (m.chimney) {
    if (x < m.x0 || x >= m.x1 || y < m.y0 || y >= m.y1) return -1;
    bool rim = x < m.x0 + 1 || x >= m.x1 - 1 || y < m.y0 + 1 || y >= m.y1 - 1;
    surf = rim ? Surf::CapRim : Surf::CapMouth;
    return rim ? zt + 1 : zt;
  }
  if (m.found || m.tree || (m.encl && ((m.feat & bld::VF_POINTS) || m.face == bld::Face::Lattice))) {
    // M3: a foundation's flat top (M3b: round platforms, a tree trunk's cut top under its crown, a stockade's log ends,
    // a lattice fence's felt band)
    if (m.round) { if (std::hypot(x - m.cx, y - m.cy) >= m.r) return -1; }
    else if (x < m.x0 || x >= m.x1 || y < m.y0 || y >= m.y1) return -1;
    surf = Surf::Flat;
    return zt;
  }
  if (m.round) {
    float rr = std::hypot(x - m.cx, y - m.cy);
    if (m.shape == RoofShape::Tent) {
      // M3b a tent of canvas or felt on its round frame: a cone that sags between the ribs from the peak, its hem cut
      // into a scalloped valance
      const float a = std::atan2(y - m.cy, x - m.cx);
      const float R = m.r + m.ov - 0.9f * (1.0f - std::fabs(std::cos(a * 6.0f)));
      if (rr > R) return -1;
      float t = 1 - rr / R;
      t = t * t * 0.35f + t * 0.65f;
      return zt - 1 + m.roofH * t;
    }
    if (elvenCone(m)) {
      // M3b an elven canopy roof of leaf or bark on a round hall. (fixer: the broad dome with a knop at its crown read
      // as a garlic bulb) A swept cone: its sides curve in as they climb (a concave profile, the leaves laid like a
      // fir's), so it rises to a slender point, and its eave kicks out and up a little all round
      // (M3c carry, owner: "the cones still read as garlic bulbs") the concave profile left a broad, nearly flat disc
      // round a thin spike, which in the 3/4 view is exactly a bulb. Now a TRUE cone: straight flanks from the point
      // down to a bell-cast eave (the last fifth of the run flares out at half the pitch), so the silhouette is the
      // eave's curve and two straight lines up to the point, a witch's hat of leaves
      // its eave an ellipse: the full overhang to the sides, foreshortened in depth to just past the drum (a cone's
      // round base seen from the 3/4 view; a full circle there gave the cone a bulb's fat round bottom)
      const float t = coneT(m, x, y);   // 0 at the point .. 1 at the eave
      if (t > 1.0f) return -1;
      constexpr float tf = 0.86f, hf = 0.07f;   // where the flare starts, and the height left there (of roofH)
      float prof;
      if (t <= tf) prof = hf + (1.0f - hf) * (1.0f - t / tf);
      else prof = hf * (1.0f - (t - tf) / (1.0f - tf));
      // round off the knee between the cone and its flare (a 2 px blend), so no crease line rings the roof
      const float kd = (t - tf) / 0.05f;
      if (std::fabs(kd) < 1.0f) {
        const float upper = (1.0f - hf) / tf, lower = hf / (1.0f - tf);
        prof -= (upper - lower) * 0.05f * 0.25f * (1.0f - kd * kd) * (1.0f - std::fabs(kd)) ;
      }
      return zt - 1 + m.roofH * prof;
    }
    if (m.shape == RoofShape::Conical || m.shape == RoofShape::Steep || m.shape == RoofShape::Pagoda || m.shape == RoofShape::Spire) {
      float R = m.r + m.ov;
      if (rr > R) return -1;
      float t = 1 - rr / R;
      if (m.shape == RoofShape::Pagoda || m.shape == RoofShape::Sweep) t = t * t * 0.55f + t * 0.45f;   // a swept, concave cone
      if (m.rmat == RoofMat::Felt && m.shape == RoofShape::Conical) {
        // M3 a yurt: a low felt cone that bellies out (rafters bent over the lattice), the crown ring (toono) at its top
        const float CR = R >= 14 ? 4.4f : 3.6f;   // (the crown ring: big enough that its spokes read as a wheel)
        if (rr < CR) { surf = Surf::Crown; return zt - 1 + m.roofH * (1 - CR / R) + 1.0f; }
        t = 1 - (1 - t) * (1 - t) * 0.35f - (1 - t) * 0.65f;   // a slight dome to the slope
        return zt - 1 + m.roofH * std::min(t, 1 - CR / R);
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
      // (M3b) a hemisphere in the 3/4 view: its ground circle foreshortened in depth like the box domes', so it reads as
      // a dome and not an egg; the drum's top shows round it as a ring (a gallery) at the front and back
      const float R = m.r + 0.5f, e = std::hypot(x - m.cx, (y - m.cy) / 0.8f);
      if (e < R) { const float t = e / R; return zt + 1 + m.roofH * std::sqrt(std::max(0.0f, 1 - t * t)); }
      if (rr > m.r) return -1;
      surf = rr > m.r - 1.5f ? Surf::Parapet : Surf::Flat;
      return zt + (surf == Surf::Parapet ? 1.0f : 0.0f);
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
  const float rx0 = m.x0 - m.ov - m.joinL, rx1 = m.x1 + m.ov + m.joinR, ry0 = m.y0 - m.ov, ry1 = m.y1 + m.ovF;
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
  // (fixer) a joined end is not hipped: the ridge runs on into the roof beside it
  if (m.joinL > 0 && m.joinR > 0) dx = 1e6f;
  else if (m.joinR > 0) dx = x - rx0;
  else if (m.joinL > 0) dx = rx1 - x;
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
    case RoofShape::Spire:   // M3b a needle: four equal steep planes to one point
      d = std::min(dx, dy);
      dmax = std::min(rx1 - rx0, ry1 - ry0) * 0.5f;
      break;
    case RoofShape::Tent: {   // M3b a ridge tent: two canvas planes sagging between the poles, the ends falling to the hem
      d = m.alongY ? dx : dy;
      dmax = m.alongY ? (rx1 - rx0) * 0.5f : (ry1 - ry0) * 0.5f;
      d = std::min(d, (m.alongY ? dy : dx) * 0.9f + dmax * 0.25f);
      const float along = m.alongY ? (y - ry0) / std::max(1.0f, ry1 - ry0) : (x - rx0) / std::max(1.0f, rx1 - rx0);
      const float tt = std::clamp(d / std::max(1.0f, dmax), 0.0f, 1.0f);
      const float sag = 1.6f * std::sin(along * PI) * tt;   // the ridge rope sags between its poles
      return zt - 1 + m.roofH * (tt * tt * 0.35f + tt * 0.65f) - sag;
    }
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
    // (fix) four narrow treads round a broad summit: each riser stands taller than its tread is deep, so the
    // pyramid's outline steps at its shoulders and every terrace shows a lit top and a riser below it
    prof = stepLevel(t);
    if (m.stairX1 > m.stairX0 && x >= m.stairX0 && x < m.stairX1 && y > (m.y0 + m.y1) * 0.5f) prof = std::min(1.0f, t / kStepSummit);   // the great stair
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
          bool mortar = q == 0 || (hh == 0 && (q == 1 || q == sw - 1));
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
      // (fixer M4 r1, review: "seed 5 settlements light no windows at night") the slit's dark is its pane: lamplight
      // shows in it at night like any window (and war soot rises from it)
      for (int j = 0; j < wh; j++) { F.markGlass(uc, v0 + j); F.markGlass(uc + 1, v0 + j); }
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

// ---------------------------------------------------------------- the plan: masses from the blueprint's volumes
// M3b (VISION_PLAN 15.14): every mass is a volume of the builder's blueprint (rpg/build/blueprint.h): its box, height,
// storeys, roof kind and materials, its face and details. The building's purpose adds only signs and machinery
// (overlays).
struct Plan {
  Scene sc;
  Building b = Building::House;   // the purpose (signage and machinery only)
  ArchStyle st;
  uint32_t seed = 0;
  int W = 0, D = 0;
  int doorX = 0;             // door centre, px
  int zBase = 0;             // the door mass's base height
  int doorMass = 0;
  BuildingFacts facts;       // storeys (the body's) and hearth
  int budget = 48;           // px the sprite may rise above the footprint's top edge (riseBudgetTiles)
  uint32_t rowsShown = 0;    // bit s set when storey s shows on the body (a door or windows on the ground floor, a row of
                             // windows, slits or a band above it): BuildingInfo::storeys counts them
  int cul = CU_NONE;
  int winStep = 16, winOff = 0;         // the window rhythm along a facade (facade variety)
  int doorFrame = 0;                    // 0 plain, 1 pilasters, 2 a fanlight, 3 a painted surround
  uint32_t shutterCol = 0;              // 0: the seed's own
  bld::Signage sign;
  int seat = 0;                         // cult::Seat + 1 of a seat of power
  int nVols = 0;                        // masses made from volumes (the dormers follow them)
  bld::OpenFront open;                  // (owner 2026-10-06) the open front: no door anywhere, walked in between its pillars
  std::vector<std::array<int, 3>> doorsPainted;   // doors painted: x0, x1 (frame px), the wall face's y (BuildingInfo)
};

float slopeFor(int pitch) {
  static const float s[9] = {0.45f, 0.62f, 0.80f, 1.05f, 1.40f, 1.90f, 2.30f, 2.80f, 3.40f};
  return s[std::clamp(pitch, 0, 8)];
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
    case RoofMat::Adobe: r = tint ? ramp(tint, 0.8f) : kAdobe; break;
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
    case WallKind::Brick: return tint ? ramp(mix(tint, rgba(156, 80, 58), 0.6f)) : kBrick;
    case WallKind::Log: return kLog;
    case WallKind::Adobe: return tint ? ramp(tint, 0.8f) : kAdobe;
    case WallKind::Planks: return tint ? ramp(tint, 0.9f) : kWood;
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

// a mass from a blueprint volume (MIRRORS the builder's overhang and roof-height estimates, rpg/build/builder.cpp)
Mass volumeMass(const Plan& p, const bld::Volume& v) {
  const ArchStyle& st = p.st;
  Mass m;
  m.role = v.role;
  m.face = v.face;
  m.feat = v.feat;
  m.storeys = std::max(1, (int)v.storeys);
  m.dormers = v.dormers;
  m.gable = v.gable;
  m.hasDoor = v.doorHere;
  m.round = v.shape != bld::VolShape::Box;
  m.x0 = v.x0; m.x1 = v.x1; m.y0 = v.y0; m.y1 = v.y1;
  if (m.round) { m.r = std::min(m.x1 - m.x0, m.y1 - m.y0) * 0.5f; m.cx = (m.x0 + m.x1) * 0.5f; m.cy = m.y1 - m.r; }
  m.zBase = v.z0;
  m.wallH = v.wallH;
  m.shape = v.roof;
  if (m.round && (m.shape == RoofShape::Hip || m.shape == RoofShape::Gable || m.shape == RoofShape::Turf || m.shape == RoofShape::Mansard)) m.shape = RoofShape::Conical;
  m.rmat = v.roofMat;
  m.alongY = v.ridgeNS;
  m.slope = slopeFor(v.pitch);
  m.pitch = v.pitch;
  if (m.shape == RoofShape::Steep) m.slope = std::max(m.slope, slopeFor(4));
  if (m.shape == RoofShape::Turf) m.slope = std::min(std::max(m.slope, slopeFor(2)), slopeFor(3));
  m.wall = wallKindFor(v.wall);
  // the style's tints belong to the style's own materials; a volume in another material keeps that material's ramp
  const uint32_t wt = v.wallTint ? v.wallTint : (v.wall == st.wall ? st.wallTint : 0);
  const uint32_t rt = v.roofTint ? v.roofTint : (v.roofMat == st.roofMat ? st.roofTint : 0);
  m.rR = roofRampFor(v.roofMat, rt, p.seed, st.weather);
  m.wR = wallRampFor(m.wall, wt, p.seed >> 3);
  m.tR = (m.wall == WallKind::Stone || m.wall == WallKind::Adobe || m.wall == WallKind::Plaster || m.wall == WallKind::Brick) ? kStoneWarm : kBeam;
  if (m.wall == WallKind::Log) m.tR = kWoodDark;
  if ((int)m.wall >= (int)WallKind::Planks) m.tR = trimRampFor(m.wall);
  if (m.wall == WallKind::Adobe && m.cul == CU_NONE) m.tR = kStoneWarm;
  if (v.trimTint || st.trimTint) m.tR = ramp(opaque(v.trimTint ? v.trimTint : st.trimTint));
  if (v.feat & bld::VF_BARNBOARDS) { m.barn = true; m.wR = v.wallTint ? ramp(opaque(v.wallTint)) : kBarnRed; m.tR = kCloth; }
  m.snow = st.snow;
  m.moss = st.weather;
  m.seed = p.seed;
  m.cul = st.culture;
  m.orn = v.ornament;
  m.win = (uint8_t)v.window;
  m.door = (uint8_t)v.door;
  m.accent = st.accentTint;
  m.alt = st.altTint;
  m.crenel = (v.feat & bld::VF_CRENELS) != 0;
  m.gambrel = (v.feat & bld::VF_GAMBREL) != 0;
  m.found = v.role == bld::VolRole::Platform;
  m.tier = v.role == bld::VolRole::Tier;
  m.chimney = v.role == bld::VolRole::Chimney;
  m.encl = v.role == bld::VolRole::Enclosure;
  m.tree = v.role == bld::VolRole::Tree;
  m.open = v.face == bld::Face::Colonnade || v.face == bld::Face::Arcade || v.face == bld::Face::Veranda;
  if (m.chimney) m.shape = RoofShape::FlatParapet;
  // eave overhangs (as the builder estimates them)
  m.ov = 3; m.ovF = 2;
  switch (m.shape) {
    case RoofShape::Turf: m.ov = 2; break;
    case RoofShape::Pagoda: m.ov = 5; m.ovF = 4; break;
    case RoofShape::Sweep: m.ov = 4; m.ovF = 3; break;
    case RoofShape::Mansard: m.ov = 2; m.ovF = 2; break;
    case RoofShape::Stepped: m.ov = 1; m.ovF = 1; break;
    case RoofShape::Tent: case RoofShape::Spire: m.ov = 1; m.ovF = 1; break;
    case RoofShape::FlatParapet: case RoofShape::Dome: case RoofShape::Onion: m.ov = 0; m.ovF = 0; break;
    default: break;
  }
  if ((m.rmat == RoofMat::Thatch || m.rmat == RoofMat::Palm) && !flatShape(v.roof) && v.roof != RoofShape::Stepped && v.roof != RoofShape::Tent && v.roof != RoofShape::Spire) { m.ov = 4; m.ovF = 3; }
  if (m.ov > 0 || m.ovF > 0) { m.ov += v.eave; m.ovF += v.eave; }
  if (m.chimney || m.found || m.encl || m.tree) { m.ov = 0; m.ovF = 0; }
  if (m.alongY && (m.gable == 1 || m.gable == 2)) m.ovF = 0;   // a crow-stepped or bell gable rises clear of the roof
  if (v.feat & bld::VF_STAIR) {
    m.stairX0 = std::max(m.x0 + 2, (float)p.doorX - 7); m.stairX1 = std::min(m.x1 - 2, (float)p.doorX + 7);
  }
  return m;
}

void finishRoofHeight(Mass& m) {
  m.roofH = 0;
  if (m.chimney || m.found || m.tree) return;
  if (m.round) {
    switch (m.shape) {
      case RoofShape::Dome: m.roofH = m.r * 0.6f; break;
      case RoofShape::Onion: m.roofH = (m.r + 1.5f) * 1.7f; break;
      case RoofShape::FlatParapet: m.roofH = 0; break;
      case RoofShape::Stepped: m.roofH = m.r * 0.6f; break;
      case RoofShape::Tent: m.roofH = (m.r + m.ov) * 0.95f; break;
      // (M3c carry) a true cone tall enough that its point stands well clear of the back eave (at 1.15 of the run it
      // barely topped the eave's circle: a bulb); fitMass lowers it where the building's clearance is short
      case RoofShape::Sweep: m.roofH = (m.r + m.ov) * 1.75f; break;
      default:
        if (m.rmat == RoofMat::Felt && m.shape == RoofShape::Conical) m.roofH = (m.r + m.ov) * (0.52f + 0.08f * (float)std::min(3, m.pitch));   // a yurt's low cone
        else m.roofH = (m.r + m.ov) * std::max(1.3f, m.slope * 1.4f);
        break;
    }
    return;
  }
  const float w = m.x1 - m.x0, d = m.y1 - m.y0;
  const float span = m.alongY ? (w + 2 * m.ov) : (d + m.ov + m.ovF);
  switch (m.shape) {
    case RoofShape::FlatParapet: m.roofH = 0; break;
    case RoofShape::Dome: m.roofH = std::max(0.0f, std::min(w, d) * 0.5f - 7) * 0.8f; break;
    case RoofShape::Onion: m.roofH = std::max(0.0f, std::min(w, d) * 0.5f - 6) * 1.6f; break;
    case RoofShape::Stepped: m.roofH = span * 0.5f * std::max(0.7f, m.slope); break;
    case RoofShape::Spire: m.roofH = std::min(w + 2 * m.ov, d + m.ov + m.ovF) * 0.5f * m.slope; break;
    case RoofShape::Tent: m.roofH = span * 0.5f * 0.6f; break;
    default: m.roofH = span * 0.5f * m.slope; break;
  }
}

// ---------------------------------------------------------------- the rise budget
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
    const float r0 = massRise(m), over = r0 - lim;
    if (over <= 0) break;
    Mass t = m;
    t.roofH = std::max(3.0f, m.roofH - over - 0.5f);
    if (massRise(t) >= r0 - 0.25f) break;   // (M3b) the roof's top is not what rises (an upswept eave corner): keep its pitch
    m.roofH = t.roofH;
  }
  for (int it = 0; it < 4; it++) {
    float over = massRise(m) - lim;
    if (over <= 0 || m.wallH <= minWall) break;
    m.wallH = std::max(minWall, m.wallH - (int)std::ceil(over));
  }
  for (int it = 0; it < 4 && m.zBase > 0 && (m.tier || m.role == bld::VolRole::Drum); it++) {   // an upper tier sinks onto its roof
    float over = massRise(m) - lim;
    if (over <= 0) break;
    m.zBase = std::max(0, m.zBase - (int)std::ceil(over));
  }
}

// the floor line of storey k (1 .. storeys-1) on a mass's facade (rows from the ground)
int floorLine(const Mass& m, int k) {
  const int deck = flatShape(m.shape) && !m.round ? 4 : 0;
  return k * (m.wallH - deck) / std::max(1, m.storeys) + 1;
}

Plan planFromBlueprint(const bld::Blueprint& bp) {
  Plan p;
  p.b = bp.req.purpose;
  p.st = bp.style;
  p.seed = bp.req.seed;
  p.facts = bp.facts;
  const int wt = std::max(1, bp.req.wTiles), ht = std::max(1, bp.req.hTiles);
  p.W = wt * 16;
  p.D = ht * 16;
  p.doorX = (wt / 2) * 16 + 8;
  p.cul = bp.style.culture;
  p.sign = bp.sign;
  p.seat = bp.seat;
  const int asked = bp.req.facts.storeys > 0 ? bp.req.facts.storeys : defaultStoreys(p.b);
  p.budget = riseBudgetTiles(p.b, asked) * 16;
  // the culture's facade rhythm and door surround from ArchStyle::variant (mixed with the seed, so a street of one
  // culture never repeats a facade)
  const ArchStyle& st = p.st;
  const uint32_t vv = (uint32_t)st.variant * 2654435761u ^ hash3((int)p.seed, 77, 0xFACADEu);
  static const int steps[4] = {16, 14, 18, 20};
  p.winStep = steps[vv & 3];
  p.winOff = (int)((vv >> 2) & 3) * 2 - 3;
  p.doorFrame = (int)((vv >> 4) & 3);
  if (st.accentTint || st.altTint) {
    const uint32_t opts[3] = {st.accentTint ? st.accentTint : st.altTint, st.altTint ? st.altTint : st.accentTint, rgba(70, 62, 56)};
    p.shutterCol = opts[(vv >> 8) % 3];
  }
  std::vector<Mass>& ms = p.sc.ms;
  ms.reserve(bp.vols.size() + 4);
  for (size_t i = 0; i < bp.vols.size(); i++) {
    Mass m = volumeMass(p, bp.vols[i]);
    m.body = i == 0;
    if (bp.vols[i].doorHere) p.doorMass = (int)i;
    bld::openPillars(bp.vols[i], p.doorX, m.pillars);
    ms.push_back(m);
  }
  p.nVols = (int)ms.size();
  p.open = bld::openFront(bp);
  if (ms.empty()) { Mass m; m.x1 = (float)p.W; m.y1 = (float)p.D; m.body = true; ms.push_back(m); p.nVols = 1; }
  p.zBase = ms[(size_t)p.doorMass].zBase;
  for (Mass& m : ms) finishRoofHeight(m);
  // a cross gable's (or an L wing's) ridge stops at the body's ridge, and starts at it
  const Mass& b0 = ms[0];
  for (size_t i = 1; i < ms.size(); i++) {
    Mass& g = ms[i];
    if (g.round || b0.round || !g.alongY || b0.alongY || g.zBase != b0.zBase || g.chimney || g.found || flatShape(g.shape) || flatShape(b0.shape)) continue;
    if (g.x0 < b0.x0 - 0.5f || g.x1 > b0.x1 + 0.5f) continue;
    const float mainRidge = b0.zTop() - 1 + b0.roofH, gRidge = g.zTop() - 1 + g.roofH;
    if (gRidge > mainRidge - 1) g.roofH = std::max(2.0f, mainRidge - 1 - (g.zTop() - 1));
    g.y0 = std::max(g.y0, std::floor((b0.y0 + b0.y1) * 0.5f));
  }
  // (fixer) an L's wing or a side wing standing against the body under a lower roof: its roof runs on into the body's
  // and meets it in a valley (a hip end against the body's roof showed as a dark wedge between the two)
  for (size_t i = 1; i < ms.size(); i++) {
    Mass& g = ms[i];
    if (g.round || b0.round || g.chimney || g.found || g.encl || g.tree || g.tier || flatShape(g.shape) || flatShape(b0.shape)) continue;
    if (g.role != bld::VolRole::Wing && g.role != bld::VolRole::Annex) continue;
    if (g.alongY && !b0.alongY) {
      // an L's wing with its ridge running back beside the body: the body's roof is not hipped down against it (the
      // hip and the wing's shaded slope made a dark wedge between them); its end stands on the wing, which hides it
      if (std::abs(g.zBase - b0.zBase) > 2) continue;
      const float ovy = std::min(g.y1, b0.y1) - std::max(g.y0, b0.y0);
      if (ovy < (b0.y1 - b0.y0) * 0.6f) continue;
      if (g.x1 >= b0.x0 - 1 && g.x1 <= b0.x0 + 6 && g.x0 < b0.x0) ms[0].joinL = std::max(ms[0].joinL, 0.5f);
      else if (g.x0 <= b0.x1 + 1 && g.x0 >= b0.x1 - 6 && g.x1 > b0.x1) ms[0].joinR = std::max(ms[0].joinR, 0.5f);
      continue;
    }
    if (g.alongY) continue;
    if (g.shape == RoofShape::Spire || g.shape == RoofShape::Tent || g.shape == RoofShape::Stepped || g.shape == RoofShape::Turf) continue;
    if (std::abs(g.zBase - b0.zBase) > 2) continue;
    const float ov = std::min(g.y1, b0.y1) - std::max(g.y0, b0.y0);
    if (ov < (g.y1 - g.y0) * 0.6f) continue;   // side by side along most of the wing's depth
    // the lower roof runs on into the taller one (as far as the taller one's ridge); two of a height meet ridge to ridge
    const float gRidge = g.zTop() - 1 + g.roofH, bRidge = b0.zTop() - 1 + b0.roofH;
    const bool west = g.x1 >= b0.x0 - 1 && g.x1 <= b0.x0 + 6 && g.x0 < b0.x0;
    const bool east = !west && g.x0 <= b0.x1 + 1 && g.x0 >= b0.x1 - 6 && g.x1 > b0.x1;
    if (!west && !east) continue;
    const float bHalf = (b0.x1 - b0.x0) * 0.5f, gHalf = (g.x1 - g.x0) * 0.5f;
    float& gj = west ? g.joinR : g.joinL;
    float& bj = west ? ms[0].joinL : ms[0].joinR;
    if (gRidge <= bRidge - 1) gj = bHalf;
    else if (bRidge <= gRidge - 1) bj = gHalf;
    else { gj = 1.0f; bj = 1.0f; }
  }
  // the floor lines: a jettied upper floor on the volumes that ask for it
  for (Mass& m : ms) {
    if (m.chimney || m.found || m.encl || m.tree || m.tier || m.storeys < 2) continue;
    m.floorV = floorLine(m, 1);
    if ((m.feat & bld::VF_JETTY) && !m.round && !flatShape(m.shape)) { m.jetty = 2; m.jettyV = m.floorV + 1; }
  }
  return p;
}

// everything stays within the generator's clearance (the rise budget). Flags, finials and the tower's spike are
// painted above their roofs, so those roofs keep room for them. One mass at a time (the incremental paint spreads a
// palace's fitting over steps).
void fitPlanMass(Plan& p, int i) {
  Mass& m = p.sc.ms[(size_t)i];
  if (m.chimney) return;
  float extra = 2;   // the outline, and rounding to rows
  if (m.feat & bld::VF_FLAG) extra += 11;
  if (m.round && (m.shape == RoofShape::Conical || m.shape == RoofShape::Spire || m.shape == RoofShape::Onion || m.shape == RoofShape::Dome)) extra += 4;
  if (!m.round && (m.shape == RoofShape::Spire || m.shape == RoofShape::Onion)) extra += 4;
  if (m.feat & bld::VF_STEAM) extra += 3;
  const int need = m.storeys <= 1 ? 16 : 22 + 14 * (m.storeys - 1) - 4;
  int minWall = m.body ? std::max(std::min(m.wallH, need), 10) : std::max(10, m.wallH - 16);
  if (!m.body && m.storeys >= 2) minWall = std::max(minWall, std::min(m.wallH, need));
  fitMass(m, (float)p.budget - extra, minWall);
}

// the masses fitted: chimneys onto the roofs, dormers, the picture's height
void makePlanFinish(Plan& p) {
  const Building b = p.b;
  const ArchStyle& st = p.st;
  const uint32_t seed = p.seed;
  const int D = p.D;
  std::vector<Mass>& ms = p.sc.ms;
  auto topNC = [&](float x, float y) {   // the roofs' height, chimneys left out
    float best = -1;
    for (const Mass& m : ms) {
      if (m.chimney) continue;
      Surf s;
      best = std::max(best, massTop(m, x, y, s));
    }
    return best;
  };
  auto flatAt = [&](float x, float y) {
    float best = -1;
    bool fl = false;
    for (const Mass& m : ms) {
      if (m.chimney) continue;
      Surf s;
      const float z = massTop(m, x, y, s);
      if (z > best) { best = z; fl = s == Surf::Flat || s == Surf::Parapet; }
    }
    return fl;
  };
  int nChim = 0;
  for (Mass& c : ms) {
    if (!c.chimney) continue;
    const float cw = c.x1 - c.x0, cd = c.y1 - c.y0, cx = c.x0;
    const int stand = c.wallH;   // how far the stack stands above the ridge beside it (the builder's)
    const bool flatTop = flatAt(cx + cw * 0.5f, c.y0 + cd * 0.5f);
    float zr = 0;
    auto place = [&](float y) {
      zr = std::max(0.0f, topNC(cx + cw * 0.5f, y + cd));
      float ridge = 0;
      for (int k = 0; k <= 8; k++) ridge = std::max(ridge, topNC(cx + cw * 0.5f, y - 6 + k * 2.0f));
      c.y0 = y; c.y1 = y + cd;
      c.zBase = (int)std::floor(std::max(0.0f, zr - 2));
      c.wallH = (int)std::ceil(ridge + (flatTop ? std::min(5, stand) : stand) - c.zBase);
    };
    const float lim = (float)p.budget - 3;
    float base1 = 1e9f;
    for (const Mass& m : ms) if (!m.chimney && !m.found && cx + cw * 0.5f >= (m.round ? m.cx - m.r : m.x0) && cx + cw * 0.5f < (m.round ? m.cx + m.r : m.x1)) base1 = std::min(base1, m.round ? m.cy + m.r : m.y1);
    if (base1 > 1e8f) base1 = (float)D;
    place(c.y0);
    for (int k = 0; k < 24 && c.zTop() + 1 - c.y0 > lim && c.y1 < base1 - 6; k++) place(c.y0 + 1);
    if (c.zTop() + 1 - c.y0 > lim) c.wallH = std::max((int)std::ceil(zr + 3) - c.zBase, (int)std::floor(lim + c.y0 - 1) - c.zBase);
    c.wR = c.wall == WallKind::Stone ? kStone : (c.wall == WallKind::Adobe ? kAdobe : kBrick);
    c.tR = kStone;
    c.smoke = st.smoke || b == Building::Smithy || b == Building::Smelter || b == Building::Bakery || (b == Building::Inn && nChim == 0);
    c.snow = st.snow;
    c.seed = seed + 99u + (uint32_t)nChim;
    nChim++;
  }
  // dormers: small front gables on the front plane of the body's deep pitched roof
  const Mass base = ms[0];
  if (base.dormers > 0 && !base.alongY && !base.round && (base.shape == RoofShape::Gable || base.shape == RoofShape::Hip || base.shape == RoofShape::Steep || base.shape == RoofShape::Mansard) &&
      base.y1 - base.y0 >= 40 && !st.snow) {
    const int nd = std::min(2, base.dormers);
    for (int i = 0; i < nd; i++) {
      float dw = 13;
      float span = base.x1 - base.x0 - 24;
      if (span < dw) break;
      float t = nd == 1 ? 0.5f + ((int)(hash3(i, 9, seed) % 30) - 15) / 100.0f : (i == 0 ? 0.22f : 0.78f);
      float dx0 = base.x0 + 12 + span * t - dw * 0.5f;
      bool clash = false;
      for (const Mass& o : ms) if ((o.chimney || (o.role == bld::VolRole::Wing && o.alongY) || o.role == bld::VolRole::Tower) && dx0 < o.x1 + 2 && dx0 + dw > o.x0 - 2 && o.y1 > base.y1 - 20) clash = true;
      if (clash) continue;
      float dfy = base.y1 - 6;   // front of the dormer, a little behind the eave
      float zr = topNC(dx0 + dw * 0.5f, dfy + 1);
      Mass d = base;
      d.role = bld::VolRole::Annex; d.face = bld::Face::Windows; d.feat = 0; d.storeys = 1; d.dormers = 0; d.gable = 0; d.hasDoor = false;
      d.body = false; d.floorV = 0; d.jetty = 0; d.jettyV = 0; d.backHip = 0; d.found = false; d.tier = false; d.open = false; d.crenel = false; d.gambrel = false;
      d.x0 = dx0; d.x1 = dx0 + dw; d.y0 = dfy - 10; d.y1 = dfy;
      d.zBase = (int)std::floor(zr - 3);
      d.wallH = 11;
      d.alongY = true;
      // always a front gable: the gable triangle is part of the dormer's own face, so its little roof sits right on
      // the window wall (a hipped dormer read as a box with a lid floating above the window)
      d.shape = RoofShape::Gable;
      d.ov = 1; d.ovF = 1;
      d.slope = std::max(0.9f, base.slope);
      if (d.wall == WallKind::Stone || d.wall == WallKind::Brick || d.wall == WallKind::Log || d.wall == WallKind::Rubble || d.wall == WallKind::Ashlar) { d.wall = WallKind::Plaster; d.wR = kPlaster; }
      finishRoofHeight(d);
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
    if (m.feat & bld::VF_FLAG) t += 12;
    if (m.feat & bld::VF_STEAM) t += 8;
    if (m.tree) t = m.zTop() + (m.r * 1.7f + 9) * 1.6f;
    hmax = std::max(hmax, t);
  }
  p.sc.W = p.W;
  p.sc.D = D;
  p.sc.top = (int)std::ceil(hmax) + 4;
  // M1 economy: the windmill's sails reach well above its cap (windmillDress): room for the upper arm, within budget
  if (b == Building::Windmill && !ms.empty() && ms[0].round) {
    const Mass& t = ms[0];
    const int need = (int)std::ceil(48.0f + t.zTop() - t.cy - t.r * 0.55f);
    p.sc.top = std::min(std::max(p.sc.top, need), p.budget - 2);
  }
  p.sc.top = std::min(p.sc.top, p.budget + 8);
}

Plan makePlan(const bld::Blueprint& bp) {
  Plan p = planFromBlueprint(bp);
  for (int i = 0; i < (int)p.sc.ms.size(); i++) fitPlanMass(p, i);
  makePlanFinish(p);
  return p;
}

// ---------------------------------------------------------------- facades
// M3b open fronts: a colonnade (columns before a shaded recess), an arcade (arches on piers), a veranda (posts and a
// railing). (owner 2026-10-06) An open front has NO door: it is walked into between its pillars, which stand where
// bld::openPillars puts them (the face's ends and the tile boundaries between), the same pillars the walking blocks
// on. A veranda's railing runs only where nobody walks in (a gallery that is not the entrance; a raised one beside
// its ladder).
// (fixer r2) is frame px column x of mass mi's open face a way in? The same answer as the walking's (bld::openFront):
// a front-row tile that is an entry, a px column of it that no pillar (nor the wall beyond the open span) blocks, and
// mass mi the frontmost volume there. A raised front is entered by its ladder or steps alone (false here).
bool openWalkable(const Plan& p, int mi, int x) {
  if (p.open.raised || !p.open.gaps || mi < 0 || mi >= p.nVols || x < 0) return false;
  const int t = x >> 4;
  if (t >= 32 || !((p.open.gaps >> t) & 1u) || ((p.open.solid[(size_t)t] >> (x & 15)) & 1u)) return false;
  const Mass& m = p.sc.ms[(size_t)mi];
  if (!m.open || m.round || m.y1 < (float)p.D - 16) return false;
  const float cx = t * 16 + 8.0f;
  if (cx < m.x0 || cx >= m.x1) return false;
  for (int i = 0; i < p.nVols; i++) {
    const Mass& o = p.sc.ms[(size_t)i];
    if (i == mi || o.round || o.found || o.chimney || o.tree) continue;
    if (cx >= o.x0 && cx < o.x1 && o.y1 > m.y1) return false;
  }
  return true;
}

void openFace(Plan& p, Mass& m, int w, bool entrance, int openTop = -1) {
  Facade& F = m.f;
  const int H = openTop > 0 ? openTop : m.wallH - (flatShape(m.shape) && !m.round ? 4 : 0);
  const bool lacq = m.cul == CU_JADE;
  const bool masonry = m.wall == WallKind::Ashlar || m.wall == WallKind::Stone || m.wall == WallKind::Plaster || m.wall == WallKind::Adobe ||
                       m.wall == WallKind::Brick || m.wall == WallKind::Rubble;
  const Ramp C = lacq ? kLacquer : (masonry ? (m.wall == WallKind::Brick || m.wall == WallKind::Rubble ? kAshlar : m.wR) : kWood);
  const int ou = (int)std::lround(m.round ? m.cx - m.r : m.x0);   // the face's origin in the frame
  (void)entrance;   // (fixer r2) the walking's bays decide where the face is open (openWalkable)
  std::vector<std::pair<int, int>> pil;   // the pillars on the face: [u0, u1)
  for (const bld::Pillar& q : m.pillars) pil.push_back({q.x0 - ou, q.x1 - ou});
  if (pil.size() < 2) pil = {{0, 3}, {w - 3, w}};
  // (fixer r2) which of the face's px columns lead in (the walking's bays, openWalkable), and which stand closed: a bay
  // part over a tile that is no entry (a colonnade's narrow end bay), or the whole face where nobody walks in
  const int mi = (int)(&m - p.sc.ms.data());
  const bool raised = p.open.raised;
  const int dcu = p.doorX - ou;
  std::vector<char> walk((size_t)std::max(0, w), 0);
  for (int u = 0; u < w; u++) walk[(size_t)u] = openWalkable(p, mi, u + ou) ? 1 : 0;
  auto inPillar = [&](int u) { for (const auto& q : pil) if (u >= q.first && u < q.second) return true; return false; };
  auto closedAt = [&](int u) {   // between the end pillars, no pillar, nobody walks in (a raised front's ladder / steps excepted)
    if (u < pil.front().second || u >= pil.back().first || inPillar(u) || walk[(size_t)u]) return false;
    return !(raised && std::abs(u - dcu) < 7);
  };
  // the recess: the back wall in the porch roof's shade (its own courses, darkened; deepest under the ceiling), the
  // daylight on the floor and the foot of the back wall, the floor's lit edge. (fixer r2) In a bay that leads in, the
  // back wall stands open: a doorway as wide as the bay into the dark hall beyond, its floor running in under the
  // daylight, the back wall's lintel over it in the ceiling's shade
  // (fixer round 3) a doorway is a door's height, never the porch's full height: a tall portico (two storeys) keeps
  // its back wall in shade above the lintel, so the way in reads as a door and not a slot through the house
  const int oTop = std::max(6, std::min(H - 6, 26));
  auto walkAt = [&](int u) { return u >= 0 && u < w && walk[(size_t)u]; };
  auto recess = [&](int u, int v) {
    const float t = (float)v / std::max(1, H);
    if (walk[(size_t)u] && v < oTop) {
      // (fixer round 3) the hall beyond: its floor running in from the sunlit threshold and fading into the room, the
      // warm glow of its lamps low down, darkness under its ceiling; the reveals of the opening lit on the west side
      // and shaded on the east, the lintel's soffit over it. A doorway with a floor and depth, not a flat black hole.
      const float d = std::min(1.0f, (float)std::max(0, v - 4) / std::max(1, oTop - 6));
      uint32_t c = mix(rgba(74, 54, 44), rgba(14, 10, 17), std::min(1.0f, 0.15f + d * 1.05f));
      const uint32_t floorLit = mix(rgba(168, 140, 104), m.wR[2], 0.35f);
      if (v == 0) c = mix(kStone[3], m.wR[3], 0.4f);                                // the threshold stone in the sun
      else if (v <= 4) c = mix(floorLit, rgba(46, 36, 38), (float)(v - 1) / 4.0f);   // the floor running in
      if (v >= 1 && v <= 3 && (u + v) % 5 == 0) c = mix(c, kInk, 0.18f);              // the boards' / flags' joints
      if (!walkAt(u - 1) && v > 0) c = mix(m.wR[3], c, 0.35f);                        // the west reveal, lit
      else if (!walkAt(u + 1) && v > 0) c = mix(m.wR[1], kInk, 0.45f);               // the east reveal, in shade
      if (v == oTop - 1 && v > 3) c = mix(m.wR[1], kInk, 0.62f);                      // the lintel's soffit
      F.set(u, v, c);
      return;
    }
    const uint32_t under = F.get(u, v);
    float k = 0.48f + 0.26f * t;
    if (v >= H - 6) k += 0.08f;   // the ceiling's shadow
    uint32_t c = mix(chA(under) ? under : m.wR[1], kInk, std::min(0.86f, k));
    if (v == 0) c = mix(kStone[2], m.wR[2], 0.4f);
    else if (v <= 3) c = mix(c, kStone[2], 0.30f - 0.07f * v);   // daylight on the floor
    F.set(u, v, c);
  };
  // a closed bay part: a balustrade across it (a plinth, balusters, a lit handrail), so it reads as no way in
  auto balustrade = [&](const Ramp& R) {
    for (int u = 0; u < w; u++) {
      if (!closedAt(u)) continue;
      F.set(u, 0, R[1]);
      F.set(u, 1, R[2]);
      for (int v = 2; v <= 5; v++) {
        const int k = ((u - pil.front().second) % 3 + 3) % 3;
        if (k != 2) F.set(u, v, R[k == 0 ? 4 : 2]);
      }
      F.set(u, 6, R[2]);
      F.set(u, 7, R[4]);
    }
  };
  if (m.face == bld::Face::Arcade) {
    // an arch in each bay between the piers (the wall's own material in the spandrels)
    for (size_t i = 0; i + 1 < pil.size(); i++) {
      const int a0 = pil[i].second, a1 = pil[i + 1].first;
      if (a1 - a0 < 4) continue;
      const float acx = (a0 + a1 - 1) * 0.5f, ar = (a1 - a0) * 0.5f;
      const int spring = std::max(4, H - 3 - (int)std::ceil(ar));
      for (int u = a0; u < a1; u++)
        for (int v = 0; v < H - 2; v++) {
          if (v > spring) { const float dx = u - acx, dy = (float)(v - spring); if (dx * dx + dy * dy > ar * ar) continue; }
          recess(u, v);
        }
      // the arch ring: lit on its upper west curve, the keystone
      for (int u = a0 - 1; u <= a1; u++) {
        const float dx = u - acx;
        if (std::fabs(dx) > ar + 0.5f) continue;
        const int vv = spring + (int)std::floor(std::sqrt(std::max(0.0f, ar * ar - dx * dx))) + 1;
        F.set(u, vv, C[dx < 0 ? 4 : 2]);
      }
    }
    balustrade(C);
    // the piers: a lit west face up to where the arches spring
    for (size_t i = 0; i < pil.size(); i++) {
      const auto& q = pil[i];
      const int nb = i + 1 < pil.size() ? pil[i + 1].first - q.second : (i > 0 ? q.first - pil[i - 1].second : 8);
      const int top = std::max(4, H - 3 - (nb + 1) / 2);
      for (int v = 0; v <= top; v++)
        for (int u = q.first; u < q.second; u++) F.set(u, v, C[u == q.first ? 4 : (u == q.second - 1 ? 2 : 3)]);
    }
    for (int u = 0; u < w; u++) { F.set(u, H - 1, C[4]); F.set(u, H - 2, C[2]); }   // the cornice
  } else {
    for (int u = 0; u < w; u++)
      for (int v = 0; v < H - 3; v++) recess(u, v);
    if (m.face == bld::Face::Colonnade) {
      balustrade(C);
      for (const auto& q : pil) {
        const int cw = q.second - q.first;
        for (int v = 0; v < H - 3; v++)
          for (int k = 0; k < cw; k++) {
            int kk = k == 0 ? 4 : (k == cw - 1 ? 1 : 3);
            if (v < 2 || v >= H - 6) kk = std::min(4, kk + 1);   // the base and the capital
            F.set(q.first + k, v, C[kk]);
          }
        // the capital's and the base's spread
        F.set(q.first - 1, H - 5, C[3]); F.set(q.second, H - 5, C[2]); F.set(q.first - 1, 0, C[3]); F.set(q.second, 0, C[2]);
      }
      for (int u = 0; u < w; u++) { F.set(u, H - 3, C[3]); F.set(u, H - 2, lacq ? kGold[3] : C[4]); F.set(u, H - 1, C[2]); }   // the architrave
    } else {   // a veranda: posts, a railing where nobody walks in (the walking's bays: openWalkable), a lit floor board
      const Ramp& P = lacq ? kLacquer : kWood;
      for (int u = 0; u < w; u++) {
        F.set(u, 0, P[3]);
        if (!closedAt(u)) continue;
        F.set(u, 6, P[4]); F.set(u, 5, P[1]);
        if (u % 3 == 0) for (int v = 1; v <= 4; v++) F.set(u, v, P[2]);
      }
      for (const auto& q : pil)
        for (int v = 0; v < H - 3; v++)
          for (int u = q.first; u < q.second; u++) F.set(u, v, P[u == q.first ? 3 : 1]);
      for (int u = 0; u < w; u++) { F.set(u, H - 3, P[2]); F.set(u, H - 2, P[3]); F.set(u, H - 1, P[1]); }
    }
  }
  // a pediment or gable over the colonnade: a cornice under it
  if (m.alongY && openTop <= 0) for (int u = 0; u < w; u++) { F.set(u, m.wallH, C[4]); F.set(u, m.wallH + 1, C[2]); }
}

// M3b a compound's gateway: an arch (a timber frame on log and plank walls) with its two leaves
void gateFace(Mass& m, int w, int du) {
  Facade& F = m.f;
  const bool timber = m.wall == WallKind::Log || m.wall == WallKind::Planks || m.wall == WallKind::Felt;
  const int ow = std::min(w - 6, 18), oh = std::min(m.wallH - 5, 24);
  if (timber) {
    const int u0 = du - ow / 2;
    for (int v = 0; v < oh + 3; v++) {   // the gate posts
      F.set(u0 - 2, v, kWood[3]); F.set(u0 - 1, v, kWood[2]); F.set(u0 + ow, v, kWood[1]); F.set(u0 + ow + 1, v, kWood[0]);
    }
    for (int u = u0 - 3; u <= u0 + ow + 2; u++) { F.set(u, oh + 1, kWood[4]); F.set(u, oh, kWood[2]); F.set(u, oh - 1, kWood[1]); }
    for (int v = 0; v < oh - 1; v++)
      for (int i = 0; i < ow; i++) {
        uint32_t c = kWoodDark[(i % 3 == 0) ? 1 : 2];
        if (i == ow / 2 || i == ow / 2 - 1) c = kInk;
        const int a = i < ow / 2 ? i : i - ow / 2, diag = a * (oh - 2) / std::max(1, ow / 2 - 1);
        if (std::abs(v - diag) <= 0 && i != ow / 2 && i != ow / 2 - 1) c = kWood[2];
        if (v == 3 || v == oh - 5) c = kIron[1];
        F.set(u0 + i, v, c);
      }
    return;
  }
  facadeDoorS(m, DoorShape::Double, du, ow - 4, oh, true, m.accent ? ramp(opaque(m.accent)) : kWoodDark);
  // the voussoirs over the arch, a keystone
  for (int i = -ow / 2 - 1; i <= ow / 2; i++) {
    const float cx = -0.5f, rr = ow * 0.5f + 1.5f, dx = i - cx;
    const int vv = oh - (int)(ow * 0.5f) + (int)std::sqrt(std::max(0.0f, rr * rr - dx * dx)) + 1;
    F.set(du + i, vv, m.tR[(i & 1) ? 3 : 4]);
    F.set(du + i, vv + 1, m.tR[2]);
  }
  F.set(du, oh + 2, m.tR[4]); F.set(du - 1, oh + 2, m.tR[4]);
}

// M3b an iwan: a tall pointed arch recessed in a rectangular frame (pishtaq) of glazed tile, the door at its foot
void iwanFace(Mass& m, int w, int du) {
  Facade& F = m.f;
  const int fw = std::min(w - 4, 26), top = std::min(m.wallH - 3, (int)m.f.h - 2);
  const Ramp A = m.accent ? ramp(opaque(m.accent)) : ramp(rgba(62, 150, 168));
  const Ramp B = m.alt ? ramp(opaque(m.alt)) : kGold;
  const int u0 = du - fw / 2, u1 = u0 + fw;
  for (int v = 0; v <= top; v++)
    for (int u = u0; u < u1; u++) {
      const bool band = u < u0 + 2 || u >= u1 - 2 || v > top - 2;
      if (band) F.set(u, v, ((u + v) & 1) ? A[3] : B[2]);
    }
  const int iw = fw - 6;
  const float cx = du - 0.5f;
  for (int v = 0; v < top - 3; v++)
    for (int u = u0 + 3; u < u1 - 3; u++) {
      const float dx = std::fabs(u - cx);
      const int head = top - 4 - (int)std::lround(dx * 1.4f);
      if (v > head && v > top - 4 - iw) continue;
      if (v > top - 4 - (int)(iw * 0.55f) && v > head) continue;
      const float t = (float)v / std::max(1, top);
      uint32_t c = mix(m.wR[1], kInk, 0.25f + 0.35f * t);
      if (v > top - 10 && ((u + v) % 3 == 0)) c = mix(m.wR[2], A[2], 0.4f);   // the muqarnas
      F.set(u, v, c);
    }
  // (owner 2026-10-06) no door: the iwan is an open vaulted hall, walked into. Its floor's lit edge, the hall's dim back
  for (int u = u0 + 3; u < u1 - 3; u++) F.set(u, 0, mix(kStone[2], m.wR[2], 0.4f));
}

void buildFacades(Plan& p, int m0 = 0, int m1 = 1 << 30) {
  const ArchStyle& st = p.st;
  const Building b = p.b;
  static const uint32_t shutterCols[5] = {rgba(64, 104, 146), rgba(72, 116, 74), rgba(150, 60, 52), rgba(96, 74, 120), rgba(70, 62, 56)};
  uint32_t shutterCol = shutterCols[hash3(1, 2, p.seed) % 5];
  for (int mi = m0; mi < std::min(m1, (int)p.sc.ms.size()); mi++) {
    Mass& m = p.sc.ms[mi];
    int w = m.round ? (int)std::ceil(m.r * 2) : (int)std::lround(m.x1 - m.x0);
    if (w <= 0) continue;
    // facade height: walls, and for front-facing gables the triangle up to the roof
    int hTop = m.wallH - 1;
    std::vector<int> tops((size_t)w, hTop);
    const bool gableFront = !m.round && m.alongY && !m.chimney && !m.found && !m.encl && !flatShape(m.shape) && m.shape != RoofShape::Stepped;
    if (gableFront) {
      for (int u = 0; u < w; u++) {
        Surf s;
        float z = massTop(m, m.x0 + u + 0.5f, m.y1 - 0.5f, s);
        tops[(size_t)u] = std::max(hTop, (int)std::floor(z - m.zBase) - 1);
      }
      int peak = hTop;
      for (int t : tops) peak = std::max(peak, t);
      if (m.gable == 1 && peak > hTop + 4) {   // crow steps: the gable rises in steps clear of the roof planes
        for (int u = 0; u < w; u++) tops[(size_t)u] = std::min(peak + 2, hTop + ((tops[(size_t)u] - hTop + 4) / 4) * 4);
      } else if (m.gable == 2 && peak > hTop + 6) {   // a bell gable: curved shoulders, a little pediment on top
        for (int u = 0; u < w; u++) {
          const float x = std::fabs((u + 0.5f) / w * 2 - 1);
          const float f = x < 0.32f ? 1.0f : (x < 0.62f ? 0.62f + 0.38f * std::cos((x - 0.32f) / 0.30f * 1.5708f) : 0.62f * std::max(0.0f, std::cos((x - 0.62f) / 0.38f * 1.5708f)));
          tops[(size_t)u] = std::max(tops[(size_t)u] + 1, hTop + (int)std::lround((peak + 2 - hTop) * f));
        }
      } else if (m.gable == 3 && peak > hTop + 6) {   // a clipped gable (jerkinhead)
        for (int u = 0; u < w; u++) tops[(size_t)u] = std::min(tops[(size_t)u], hTop + (peak - hTop) * 3 / 4);
      }
    }
    if ((flatShape(m.shape)) && !m.chimney && !m.found && !m.tree) {
      for (int u = 0; u < w; u++) {
        if (m.round) { tops[(size_t)u] = m.wallH + 1; continue; }
        int along = (int)std::floor(m.x0 + u);
        bool merlon = m.crenel && (((along % 8) + 8) % 8) < 4;
        if (m.crenel && m.cul == CU_SUN) {   // stepped merlons
          static const int alm[8] = {4, 6, 8, 8, 6, 4, 2, 2};
          tops[(size_t)u] = m.wallH - 1 + alm[((along % 8) + 8) % 8] - 1;
          continue;
        }
        tops[(size_t)u] = m.wallH - 1 + (merlon ? 6 : 3);
      }
    }
    if (m.encl && (m.feat & bld::VF_POINTS))
      for (int u = 0; u < w; u++) tops[(size_t)u] = m.wallH - 1 + (((u % 4) == 1 || (u % 4) == 2) ? 2 : 0);
    if (m.encl && m.face == bld::Face::Lattice) for (int u = 0; u < w; u++) tops[(size_t)u] = m.wallH - 1;
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
    if (m.encl && m.face == bld::Face::Lattice) {   // a felt-and-lattice fence: crossed laths, felt bands, painted posts
      const Ramp A = m.accent ? ramp(opaque(m.accent)) : kRed;
      for (int u = 0; u < w; u++)
        for (int v = 0; v <= tops[(size_t)u]; v++) {
          uint32_t c = 0;
          if (((u + v) % 4) == 0 || ((u - v + 64) % 4) == 0) c = kWood[((u + v) % 8) < 4 ? 3 : 2];
          if (v == m.wallH - 1 || v == m.wallH - 2) c = m.wR[v == m.wallH - 1 ? 4 : 2];
          if (v == 0) c = m.wR[1];
          if (u % 14 == 0 || u % 14 == 1) c = A[u % 14 == 0 ? 3 : 1];
          if (c) m.f.set(u, v, c);
        }
      continue;
    }
    const int plinth = (m.found || m.encl || m.tree) ? 0 : (m.zBase > 0 ? 2 : 3);
    paintWallMat(m, plinth);
    if (m.crenel && m.cul == CU_SUN && !m.round) {
      // (fix) the stepped merlons modelled in the light: each step's top a lit coping, the step's east cheek in shade,
      // a shadow line along the parapet's foot where the merlons stand on the wall walk (no flat cream zigzag)
      for (int u = 0; u < w; u++) {
        const int tp = tops[(size_t)u];
        if (tp < m.wallH) continue;
        const int tn = u + 1 < w ? tops[(size_t)u + 1] : tp, tw = u > 0 ? tops[(size_t)u - 1] : tp;
        for (int v = m.wallH - 1; v <= tp; v++) {
          if (!m.f.in(u, v)) continue;
          uint32_t c = m.wR[2];
          if (v == m.wallH - 1) c = m.wR[1];
          if (v > tn) c = m.wR[1];             // the east cheek of a step down
          if (v > tw && u > 0) c = m.wR[3];    // the west cheek, toward the light
          if (v == tp) c = m.wR[4];            // the lit coping
          m.f.set(u, v, c);
        }
      }
    }
    if (m.found) {   // M3 a foundation: a lit lip along its top edge and a darker footing course
      for (int u = 0; u < w; u++) {
        m.f.set(u, m.wallH - 1, m.wR[4]);
        if (m.wallH >= 4) m.f.set(u, 0, m.wR[1]);
      }
      continue;
    }
    if (m.encl || m.tree) {   // a compound's wall: its coping's lit lip; a stockade's log ends; a trunk's bark
      if (m.encl && !(m.feat & bld::VF_POINTS)) for (int u = 0; u < w; u++) if (m.f.in(u, m.wallH - 1)) m.f.set(u, m.wallH - 1, m.wR[4]);
      if (m.encl && (m.feat & bld::VF_POINTS))
        for (int u = 0; u < w; u++)
          for (int v = 0; v <= tops[(size_t)u]; v++) {
            const int q = u % 4;
            m.f.set(u, v, kLog[q == 0 ? 1 : (q == 3 ? 1 : (q == 1 ? 3 : 2))]);
            if (v == tops[(size_t)u] && q != 0) m.f.set(u, v, kLog[4]);
          }
      continue;
    }
    // a crow-stepped gable's copings, a bell gable's lit rim
    if (gableFront && (m.gable == 1 || m.gable == 2))
      for (int u = 0; u < w; u++) { m.f.set(u, tops[(size_t)u], m.tR[4]); if (tops[(size_t)u] > hTop + 1) m.f.set(u, tops[(size_t)u] - 1, m.tR[2]); }
    int du = p.doorX - (int)std::lround(m.round ? m.cx - m.r : m.x0);
    bool hasDoor = m.hasDoor && du >= 4 && du < w - 4;
    bool vDoor = !hasDoor && (m.feat & bld::VF_DOOR) != 0;
    if (vDoor) du = w / 2;
    // (owner 2026-10-06) an open building has no door at all: the open front is the entrance. Nor does a door ever
    // stand behind another volume's pillars (a hall behind its portico): the pillars would stand over it.
    if ((hasDoor || vDoor) && (p.open.open() || m.open || m.face == bld::Face::Iwan)) { hasDoor = false; vDoor = false; }
    if (hasDoor || vDoor) {
      const float dx0 = (m.round ? m.cx - m.r : m.x0) + du - 7, dx1 = dx0 + 14;
      for (const Mass& o : p.sc.ms)
        if (&o != &m && (o.open || o.face == bld::Face::Iwan) && o.y1 > m.y1 - 0.5f && o.x0 < dx1 && o.x1 > dx0) { hasDoor = false; vDoor = false; }
    }
    // M3 an upper tier (a pagoda's, a shrine on a pyramid): a band of small openings; a shrine's doorway
    if (m.tier) {
      const bool shrine = m.crenel || (m.feat & bld::VF_DOOR);
      if (shrine) {
        const int dd = w / 2;
        for (int v = 0; v < std::min(9, m.wallH - 2); v++)
          for (int i = -2; i <= 2; i++) m.f.set(dd + i, v, (i == -2 || i == 2) ? (m.accent ? ramp(opaque(m.accent))[2] : m.tR[3]) : (v < 2 ? kInk : rgba(30, 22, 34)));
        continue;
      }
      if (m.wallH >= 10)
        for (int u = 6; u + 4 < w - 4; u += 9) facadeWindowS(m, (WindowShape)m.win, u, 3, 4, std::max(3, m.wallH - 7), p.seed + (uint32_t)u, false, false, shutterCol);
      continue;
    }
    if (m.face == bld::Face::Vents) {   // slatted vents near the top (a wind-catcher), a belfry with its bell
      if (m.feat & bld::VF_BELL) {
        const int ow = std::min(w - 6, 9), oh = std::min(10, m.wallH - 6);
        const int u0 = w / 2 - ow / 2, v0 = m.wallH - oh - 3;
        for (int v = v0; v < v0 + oh; v++)
          for (int i = 0; i < ow; i++) {
            const float dx = i - (ow - 1) * 0.5f;
            if (v > v0 + oh - 4 && dx * dx + (v - (v0 + oh - 4)) * (v - (v0 + oh - 4)) * 1.6f > (ow * 0.5f) * (ow * 0.5f)) continue;
            m.f.set(u0 + i, v, kInk);
          }
        for (int j = 0; j < 5; j++)
          for (int i = -2; i <= 2; i++) {
            if (j > 3 && std::abs(i) > 1) continue;
            m.f.set(w / 2 + i - (ow % 2 == 0 ? 1 : 0), v0 + oh - 4 - j, kGold[i < 0 ? 4 : (i > 0 ? 2 : 3)]);
          }
        for (int i = -1; i <= ow; i++) m.f.set(u0 + i, v0 - 1, m.tR[4]);
      } else if (m.wallH >= 12) {
        for (int v = m.wallH - 9; v < m.wallH - 2; v++)
          for (int u = 2; u < w - 2; u++) m.f.set(u, v, (v % 2) ? rgba(30, 22, 34) : m.wR[1]);
        for (int u = 1; u < w - 1; u++) m.f.set(u, m.wallH - 10, m.wR[4]);
      }
      if (!hasDoor) continue;
    }
    // dormers and roof rooms: one small window, a dark doorway onto the roof
    if (mi >= p.nVols || (m.wallH <= 11 && m.zBase > 0 && !m.body && !m.hasDoor)) {
      if (m.shape == RoofShape::FlatParapet) {
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
    bool openGround = false;   // an open ground floor (a loggia) under closed upper storeys
    if (m.open) {
      if (m.storeys >= 2) {
        openFace(p, m, w, m.hasDoor, floorLine(m, 1) - 1);
        openGround = true;
        if (m.body) p.rowsShown |= 1u;
      } else {
        openFace(p, m, w, m.hasDoor);
        if (m.body) p.rowsShown |= (1u << m.storeys) - 1u;
        continue;
      }
    }
    if (m.face == bld::Face::Gate) {
      const int gu = hasDoor || vDoor ? du : w / 2, gx = (int)std::lround(m.round ? m.cx - m.r : m.x0) + gu;
      p.doorsPainted.push_back({gx - 9, gx + 9, (int)std::lround(m.round ? m.cy + m.r : m.y1)});
      gateFace(m, w, gu);
      if (m.body) p.rowsShown |= 1u;
      continue;
    }
    if (m.face == bld::Face::Iwan) {   // (owner) the open recess: its centre between its jambs (bld::openPillars), no door
      const int ou = (int)std::lround(m.round ? m.cx - m.r : m.x0);
      iwanFace(m, w, m.pillars.size() == 2 ? (m.pillars[0].x0 + m.pillars[1].x1) / 2 - ou : w / 2);
      if (m.body) p.rowsShown |= 1u;
      continue;
    }
    if (m.face == bld::Face::Lattice) continue;
    if (m.face == bld::Face::ForgeBay) {   // the open forge: dark bay, glowing hearth, anvil, bellows
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
    if (m.face == bld::Face::Blank && !hasDoor && !vDoor) continue;
    const bool fh2 = fh > m.wallH + 8;
    // gable vent / attic window
    if (m.alongY && fh2 && m.face != bld::Face::Blank && m.gable != 1) facadeWindow(m, w / 2 - 2, m.wallH + 2, 5, 5, p.seed + 3, false, false, m.face == bld::Face::Sacred, shutterCol);
    if (m.alongY && fh2 && m.gable == 1) facadeWindowS(m, WindowShape::Round, w / 2 - 3, m.wallH + 3, 6, 6, p.seed + 3, false, false, shutterCol);
    const bool sacred = m.face == bld::Face::Sacred;
    const bool masonry = m.wall == WallKind::Stone || m.wall == WallKind::Adobe || m.wall == WallKind::Ashlar || m.wall == WallKind::Rubble;
    const bool grand = b == Building::Palace || b == Building::Keep || p.seat || sacred;
    bool arched = masonry || grand;
    if ((hasDoor || vDoor) && !openGround) {
      {
        const int dx = (int)std::lround(m.round ? m.cx - m.r : m.x0) + du;
        p.doorsPainted.push_back({dx - 6, dx + 6, (int)std::lround(m.round ? m.cy + m.r : m.y1)});
      }
      if (m.face == bld::Face::BarnDoor) {
        // big barn doors with X braces
        const Ramp& BD = m.barn ? kBarnRed : m.wR;
        const Ramp& BT = m.barn ? kCloth : kWood;
        int u0 = du - 8, dh = std::min(19, m.wallH - 4);
        for (int v = 0; v < dh; v++)
          for (int i = 0; i < 16; i++) {
            int k = (i % 4 == 0) ? 1 : 2;
            uint32_t c = m.barn ? BD[k] : kWoodDark[k + 1];
            if (i == 0 || i == 15 || i == 7 || i == 8 || v == dh - 1 || v == 0 || v == dh / 2) c = BT[(i == 0 || v == dh - 1) ? 4 : 2];
            int a = (i < 8 ? i : i - 8), diag1 = a * (dh - 1) / 7, diag2 = (7 - a) * (dh - 1) / 7;
            if (std::abs(v - diag1) <= 0 || std::abs(v - diag2) <= 0) c = BT[3];
            m.f.set(u0 + i, v, c);
          }
        for (int i = -1; i <= 16; i++) m.f.set(u0 + i, dh, BT[1]);
        // hay loft door in the gable
        if (fh > m.wallH + 10)
          for (int v = m.wallH + 1; v < m.wallH + 9; v++)
            for (int i = 0; i < 9; i++) {
              uint32_t c = (i == 0 || i == 8 || v == m.wallH + 8) ? BT[3] : kWoodDark[1];
              if (v < m.wallH + 3 && i > 0 && i < 8) c = kThatch[3 + ((i + v) & 1)];
              m.f.set(du - 4 + i, v, c);
            }
      } else {
        int dw = 10, dh = 16;
        if (m.wallH < 22) { dw = 8; dh = 13; }
        if (sacred) { dw = 12; dh = 20; }
        if (b == Building::Palace && hasDoor) { dw = 16; dh = 26; }
        else if (grand && hasDoor && !sacred) { dw = 12; dh = 22; }
        const int room = m.storeys >= 2 && m.floorV > 0 ? m.floorV - 2 : m.wallH - 6;
        dh = std::min(dh, std::max(10, room));
        if (p.cul == CU_NONE) facadeDoor(m, du, dw, dh, arched, sacred ? kWood : kWoodDark);
        else {
          // M3 the culture's door, painted in its accent where it paints its doors; a surround from the facade variety
          DoorShape ds = (DoorShape)m.door;
          if (ds == DoorShape::Plank && arched) ds = DoorShape::Arched;
          if (grand && (ds == DoorShape::Plank || ds == DoorShape::Curtain || ds == DoorShape::Flap) && m.wall != WallKind::Felt) ds = DoorShape::Double;
          if (b == Building::Hut && ds == DoorShape::Double) ds = DoorShape::Plank;
          const Ramp DW = (m.accent && ((p.doorFrame & 1) || m.cul == CU_FJORD || m.cul == CU_JADE || m.cul == CU_STEPPE)) ? ramp(opaque(m.accent)) : (sacred ? kWood : kWoodDark);
          if (p.doorFrame == 1 && ds != DoorShape::Moon && ds != DoorShape::Round)   // pilasters either side
            for (int v = 0; v < dh + 2; v++) { m.f.set(du - dw / 2 - 4, v, m.tR[4]); m.f.set(du - dw / 2 - 3, v, m.tR[2]); m.f.set(du + dw / 2 + 2, v, m.tR[2]); m.f.set(du + dw / 2 + 3, v, m.tR[1]); }
          facadeDoorS(m, ds, du, dw, dh, arched, DW);
          if (p.doorFrame == 2 && ds != DoorShape::Moon && ds != DoorShape::Round && dh + 6 < m.wallH && (m.storeys < 2 || dh + 6 < m.floorV)) {   // a fanlight
            for (int i = -dw / 2; i < dw / 2; i++) {
              const int hh = (int)std::lround(3 * std::sqrt(std::max(0.0f, 1 - (i + 0.5f) * (i + 0.5f) / (dw * dw / 4.0f))));
              for (int j = 0; j <= hh; j++) { m.f.set(du + i, dh + 2 + j, j == hh ? m.tR[4] : ((i + j) % 2 ? kGlass[3] : kGlass[1])); if (j < hh) m.f.markGlass(du + i, dh + 2 + j); }
            }
          }
        }
        if (sacred && p.cul != CU_SUN && p.cul != CU_STEPPE && p.cul != CU_JADE && dh + 14 < m.wallH) {   // a rose window over the door
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
    if (m.face == bld::Face::Blank) { if (m.body) p.rowsShown |= 1u; continue; }
    uint32_t shown = (hasDoor || vDoor) ? 1u : 0u;
    // the floor lines between the storeys, in the wall's own language: a beam on timber (painted with the frame), a
    // projecting string course on stone, brick and plaster (lit top, shadow below), a plate log with joist ends on log
    // walls, a row of vigas on adobe
    for (int k = 1; k < m.storeys; k++) {
      const int F = floorLine(m, k);
      auto course = [&](const Ramp& T) {
        for (int u = 0; u < w; u++) {
          m.f.set(u, F + 1, T[4]);
          m.f.set(u, F, T[2]);
          uint32_t c = m.f.get(u, F - 1);
          if (c) m.f.set(u, F - 1, darken(c, 0.3f));
        }
      };
      if (m.face == bld::Face::Arcane || m.face == bld::Face::Slits) course(m.wall == WallKind::Ashlar ? m.wR : kStoneWarm);
      else if (m.wall == WallKind::Stone || m.wall == WallKind::Brick || m.wall == WallKind::Rubble) course(kStoneWarm);
      else if (m.wall == WallKind::Ashlar) course(m.wR);
      else if (m.wall == WallKind::Wattle || m.wall == WallKind::Planks || m.wall == WallKind::Living) course(m.wall == WallKind::Living ? kBark : kBeam);
      else if (m.wall == WallKind::Plaster && !m.jetty) course(m.tR);
      else if (m.wall == WallKind::Timber) { for (int u = 0; u < w; u++) { if (F + 1 <= m.f.top[(size_t)u]) m.f.set(u, F + 1, kBeam[2]); m.f.set(u, F, kBeam[1]); } }
      else if (m.wall == WallKind::Felt) { for (int u = 0; u < w; u++) { m.f.set(u, F + 1, kWood[3]); m.f.set(u, F, kWood[1]); } }
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
    }
    if (m.jetty) {
      // the jettied floor: a heavy sill beam on the joist ends, and the ground floor below in the overhang's shade
      const int J = m.jettyV;
      for (int u = 0; u < w; u++) { m.f.set(u, J + 1, kBeam[3]); m.f.set(u, J, kBeam[2]); }
      for (int u = 1; u < w - 1; u += 4) { m.f.set(u, J - 1, kBeam[3]); m.f.set(u + 1, J - 1, kBeam[1]); }
      for (int v = J - m.jetty - 4; v < J - 1; v++)
        for (int u = 0; u < w; u++) {
          if (v < 0) continue;
          int dd = (J - m.jetty - 1) - v;
          uint32_t c = m.f.get(u, v);
          if (c && dd <= 3) m.f.set(u, v, darken(c, dd <= 0 ? 0.6f : (dd == 1 ? 0.45f : (dd == 2 ? 0.3f : 0.15f))));
        }
    }
    int winV = 8 + (m.zBase > 0 ? 0 : 1), winW = 6, winH = 8;
    if (b == Building::Hut) { winV = 6; winW = 5; winH = 6; }
    if (m.wall == WallKind::Adobe) { winW = 5; winH = 7; }
    if (p.cul && m.wallH < 26 && !sacred && m.storeys <= 1) { winH = std::clamp(m.wallH - 13, 4, winH); winV = std::min(winV, std::max(4, m.wallH - winH - 7)); }
    const bool feltWall = m.wall == WallKind::Felt;   // (M3: a yurt has no windows below: its light comes through the crown)
    const int deck = flatShape(m.shape) && !m.round ? 6 : 2;
    const int uLo = m.round ? (int)(w * 0.18f) : 0, uHi = m.round ? (int)(w * 0.82f) : w;   // the front of a drum
    for (int s = 0; s < m.storeys; s++) {
      const int Fs = s == 0 ? 0 : floorLine(m, s);
      const int Fn = s == m.storeys - 1 ? m.wallH - deck : floorLine(m, s + 1) - 1;
      int v0 = s == 0 ? winV : Fs + (m.storeys == 2 ? 5 : 4) + (s == 1 && m.jetty ? 1 : 0);
      int wh0 = s == 0 ? winH : winH - 1;
      if (m.storeys >= 3 && s > 0) wh0 = std::min(wh0, std::max(3, Fn - v0 - 2));
      if (v0 + wh0 + 2 > Fn) { if (s > 0) v0 = std::max(Fs + 2, Fn - wh0 - 2); wh0 = std::max(3, std::min(wh0, Fn - v0 - 1)); }
      if (feltWall && s == 0) continue;   // (M3: a two-storey yurt shows its upper floor by a band of round vents)
      if (openGround && s == 0) continue;
      if (m.face == bld::Face::Slits) {
        // arrow slits (deep, with a lit sill and a dark splay), spaced along the storey
        auto slit = [&](int u, int vs, int hgt) {
          for (int v = vs; v < vs + hgt; v++) { m.f.set(u - 1, v, m.wR[1]); m.f.set(u, v, kInk); m.f.set(u + 1, v, rgba(28, 22, 36)); m.f.set(u + 2, v, m.wR[0]); }
          for (int i = -1; i <= 2; i++) { m.f.set(u + i, vs - 1, m.tR[4]); m.f.set(u + i, vs + hgt, m.tR[3]); }
          shown |= 1u << s;
        };
        const int hgt = std::clamp(Fn - v0 - 2, 4, 8);
        bool any = false;
        if (w < 34) { const int u = w / 2 + ((s & 1) ? 3 : -4); if (!(s == 0 && (hasDoor || vDoor) && std::abs(u - du) < 8)) { slit(u, v0, hgt); any = true; } }
        else for (int u = 13; u < w - 12; u += 24) {
          if (s == 0 && (hasDoor || vDoor) && std::abs(u - du) < 12) continue;
          slit(u, v0, hgt); any = true;
        }
        if (!any && s > 0) slit(du - 1, v0, hgt);
        continue;
      }
      if (m.face == bld::Face::Arcane) {
        // the mage tower: a pointed window of violet glass in each upper storey, a slit lighting the stair below
        auto arcane = [&](int uc, int vs, int hgt, int wdt) {
          for (int j = 0; j < hgt; j++)
            for (int k = 0; k < wdt; k++) {
              if (j == hgt - 1 && (k == 0 || k == wdt - 1)) continue;   // a pointed head
              uint32_t c = j >= hgt - 3 ? rgba(176, 156, 255) : rgba(112, 92, 224);
              if (k == 0) c = rgba(70, 54, 140);
              m.f.set(uc - wdt / 2 + k, vs + j, c);
              m.f.markGlass(uc - wdt / 2 + k, vs + j);
            }
          for (int k = -1; k <= wdt; k++) m.f.set(uc - wdt / 2 + k, vs - 1, kStone[4]);
          shown |= 1u << s;
        };
        if (s == 0) { if (w >= 24) arcane(w / 2 - (int)(w * 0.3f), 6, std::min(6, Fn - 7), 2); }
        else arcane(w / 2, v0, std::clamp(Fn - v0 - 1, 5, 8), 4);
        continue;
      }
      const int step = p.cul ? p.winStep : 16, off = p.cul ? p.winOff : 0;
      for (int u = 8 + off; u < w - 6; u += step) {
        int uc = u + ((w % step) / 2);
        if (uc < 6 || uc > w - 7 || uc < uLo || uc > uHi) continue;
        if ((hasDoor || vDoor) && s == 0 && std::abs(uc - du) < 11) continue;
        if (m.face == bld::Face::Shopfront && s == 0 && std::abs(uc - du) < 20) {   // a wide shop window beside the door
          facadeWindow(m, uc - 5, v0 - 1, 10, 7, p.seed + u, false, false, false, shutterCol);
          shown |= 1u << s;
          continue;
        }
        if ((m.feat & bld::VF_BALCONYDOOR) && s == 1 && std::abs(uc - du) <= 1) {   // the glazed door onto the balcony
          facadeWindow(m, uc - 3, m.floorV + 3, 6, std::min(11, Fn - m.floorV - 4), p.seed + u, false, false, false, shutterCol);
          shown |= 2u;
          continue;
        }
        bool sh = st.shutters && hash3(u, s, p.seed) % 3 != 0 && m.wall != WallKind::Adobe;
        bool fl = !st.snow && m.wall != WallKind::Adobe && hash3(u, s + 5, p.seed) % 4 == 0;
        bool ar = (masonry && m.face != bld::Face::Shopfront && b != Building::Smithy) || sacred;
        int wh = sacred ? std::min(12, Fn - v0 + 1) : wh0;
        if (p.cul != CU_NONE) {
          // M3 the culture's windows: its shape, shutters and flower boxes where it hangs them, its shutter colours
          WindowShape ws = (WindowShape)m.win;
          if (ws == WindowShape::Square && ar) ws = WindowShape::Arched;
          if (sacred && ws != WindowShape::Pointed && ws != WindowShape::Lattice && ws != WindowShape::Screen && ws != WindowShape::Round) ws = WindowShape::Arched;
          if (feltWall) ws = WindowShape::Round;
          sh = (st.shutters || (st.ornament & ORN_SHUTTERS)) && hash3(u, s, p.seed) % 3 != 0 && ws != WindowShape::Screen && ws != WindowShape::Slit &&
               m.wall != WallKind::Adobe && m.wall != WallKind::Felt;
          fl = !st.snow && (st.ornament & ORN_FLOWERBOX) && hash3(u, s + 5, p.seed) % 2 == 0 && ws != WindowShape::Screen && ws != WindowShape::Slit;
          int wx = winW, wy = wh;
          if (ws == WindowShape::Tall) { wx = 5; }
          if (ws == WindowShape::Round) { wx = 6; wy = 6; }
          if (ws == WindowShape::Pointed) { wx = 5; wy = std::max(5, wh - 2); }
          if (ws == WindowShape::Lattice || ws == WindowShape::Screen) { wx = 7; wy = std::min(7, std::max(5, wh)); }
          facadeWindowS(m, ws, uc - wx / 2, v0 - (sacred ? 2 : 0), wx, wy, p.seed + u * 7 + s, sh, fl, p.shutterCol ? p.shutterCol : shutterCol);
          shown |= 1u << s;
          if (sacred && (ws == WindowShape::Arched || ws == WindowShape::Pointed))
            for (int j = 1; j < wy - 1; j++)
              for (int i = 0; i < wx; i++) {
                if (i == wx / 2 || !m.f.isGlass(uc - wx / 2 + i, v0 - 2 + j)) continue;
                static const uint32_t sg[4] = {rgba(222, 74, 84), rgba(84, 132, 230), rgba(250, 204, 86), rgba(92, 192, 122)};
                m.f.set(uc - wx / 2 + i, v0 - 2 + j, sg[(i + j / 3) % 4]);
              }
          continue;
        }
        facadeWindow(m, uc - winW / 2, v0 - (sacred ? 2 : 0), winW, wh, p.seed + u * 7 + s, sh, fl, ar, shutterCol);
        shown |= 1u << s;
        if (sacred)
          for (int j = 1; j < wh - 1; j++)
            for (int i = 0; i < winW; i++) {
              if (i == winW / 2) continue;
              static const uint32_t sg[4] = {rgba(222, 74, 84), rgba(84, 132, 230), rgba(250, 204, 86), rgba(92, 192, 122)};
              m.f.set(uc - winW / 2 + i, v0 - 2 + j, sg[(i + j / 3) % 4]);
            }
      }
      // a storey too narrow for the rhythm still shows a window (the storeys always read)
      if (!(shown & (1u << s)) && !(s == 0 && (hasDoor || vDoor))) {
        const int uc = (hasDoor || vDoor) && s == 0 ? (du > w / 2 ? du - 12 : du + 12) : w / 2 + (s & 1 ? 3 : -3);
        const WindowShape ws = feltWall ? WindowShape::Round : (p.cul ? (WindowShape)m.win : WindowShape::Square);
        const int wx = ws == WindowShape::Round ? 6 : 5, wy = ws == WindowShape::Round ? 6 : std::max(4, wh0 - 1);
        if (uc - wx / 2 >= 2 && uc + wx / 2 < w - 2) { facadeWindowS(m, ws, uc - wx / 2, v0, wx, wy, p.seed + 31u * (uint32_t)s, false, false, shutterCol); shown |= 1u << s; }
      }
    }
    // adobe: wooden beam ends (vigas) poking out under the roofline, and a rain spout
    if (m.wall == WallKind::Adobe && !m.round) {
      for (int u = 5; u < w - 3; u += 9) { m.f.set(u, m.wallH - 6, kWood[3]); m.f.set(u + 1, m.wallH - 6, kWood[1]); m.f.set(u, m.wallH - 7, kWood[0]); m.f.set(u + 1, m.wallH - 7, kWood[0]); }
    }
    if (m.body) p.rowsShown |= shown;
    if (m.body && m.shape == RoofShape::Stepped) p.rowsShown |= (1u << m.storeys) - 1u;   // the shrine on top is its upper floor
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

// is a roof point shaded by something taller standing up-left of it (chimneys, dormers, towers, higher wings)? (M3b:
// asks only the masses that can stand there, `near`)
bool roofShadowed(const Scene& sc, const std::vector<int>& near, float x, float y, float z, int self) {
  for (int k = 2; k <= 10; k += 2) {
    float sx = x - k * 0.75f, sy = y - k * 0.45f, need = z + k * 0.9f;
    for (int i : near) {
      if (i == self) continue;
      const Mass& m = sc.ms[(size_t)i];
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
    // (fix) a wheel: the rim lit on its top-left and shaded below right, eight thin spokes out of a small hub, the
    // smoke-hole's dusk between them (never two dark quarters and a bar, which read as a face)
    const float dx = x - m.cx, dy = y - m.cy, rr = std::sqrt(dx * dx + dy * dy);
    const float CR = m.r + m.ov >= 14 ? 4.4f : 3.6f, rim = CR - 1.25f;
    if (rr >= rim) return rr > CR - 0.45f ? kWood[1] : kWood[dx + dy < -0.5f ? 4 : (dx + dy > 0.5f ? 2 : 3)];
    if (rr < 0.75f) return kWood[3];
    const float a = std::atan2(dy, dx), sp = std::fabs(std::sin(a * 4.0f)) * rr;
    if (sp < 0.42f) return kWood[(dx + dy < 0) ? 3 : 2];
    return mix(kInk, kSoil[1], 0.5f);
  }
  if (m.encl && s == Surf::Flat) {   // M3b a stockade's sharpened log ends, a lattice fence's felt band
    if (m.feat & bld::VF_POINTS) { const int q = (((int)std::floor(x)) % 4 + 4) % 4; return kLog[q == 0 ? 1 : (q == 2 ? 4 : 3)]; }
    return m.wR[y - m.y0 < 1.0f ? 4 : 3];
  }
  if (m.tree && s == Surf::Flat) return kLiving[2];
  if (m.found && s == Surf::Flat && (m.feat & bld::VF_GARDEN)) {
    // M3b a court laid out as a garden: lawns quartered by paths, a pool at the crossing, the walls' shade along the edges
    const float cx = (m.x0 + m.x1) * 0.5f, cy = (m.y0 + m.y1) * 0.5f;
    const float dx = std::fabs(x - cx), dy = std::fabs(y - cy);
    const float pr = std::min(7.0f, std::min(m.x1 - m.x0, m.y1 - m.y0) * 0.22f);
    if (dx * dx + dy * dy * 1.6f < pr * pr) return (dx * dx + dy * dy * 1.6f > (pr - 1.2f) * (pr - 1.2f)) ? kStoneWarm[4] : kWater[((int)(x + y) % 5 == 0) ? 3 : 2];
    if (dx < 1.5f || dy < 1.2f) return kStoneWarm[shade ? 2 : 3];
    const uint32_t h = hash3((int)std::floor(x), (int)std::floor(y), m.seed + 61);
    int k = (h % 5 == 0) ? 3 : 2;
    if (h % 23 == 0) return rgba(236, 120, 150);   // flowers
    if (shade) k = 1;
    return kLeaf[k];
  }
  if (m.found && s == Surf::Flat && (m.wall == WallKind::Adobe || m.wall == WallKind::Planks || m.wall == WallKind::Living)) {
    // M3b packed earth (a court, a tent ground) or boards (a boardwalk, a ring platform in a tree)
    const int gx = (int)std::floor(x), gy = (int)std::floor(y);
    if (m.wall != WallKind::Adobe) {
      const int q = ((gy % 3) + 3) % 3;
      int k = q == 0 ? 1 : 3;
      if (hash3(gx / 7, gy / 3, m.seed) % 5 == 0 && q != 0) k = 2;
      if (shade) k = std::max(0, k - 1);
      return (m.wall == WallKind::Living ? kLiving : kWood)[k];
    }
    const float n = vnoise(x / 5.0f, y / 4.0f, m.seed + 17);
    int k = n < 0.3f ? 1 : (n > 0.75f ? 3 : 2);
    if (shade) k = std::max(0, k - 1);
    return mix(m.wR[k], kSoil[k], 0.35f);
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
    // (M3b round 3) in the snow a flat deck carries it too (the terraces stayed bare tan beside snowed pitched roofs):
    // a smooth white field, drifted deeper against the parapets, blue in the parapets' shadow, a trodden path to the
    // hatch showing the deck through it here and there
    if (m.snow) {
      const float n = vnoise(x / 5.0f, y / 3.5f, m.seed + 51);
      int ks = n > 0.62f ? 4 : 3;
      if (pshadow) ks = 1;
      else if (!m.round && (x - m.x0 < 7.0f || y - m.y0 < 6.0f)) ks = 2;
      if (shade) ks = std::max(1, ks - 1);
      return kSnow[ks];
    }
    if (m.wall == WallKind::Adobe || (m.rmat == RoofMat::Adobe && (m.wall == WallKind::Wattle || m.wall == WallKind::Plaster || m.wall == WallKind::Felt))) {
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
    // (fix) the deck is a top face in the full light: a step brighter and softer than the same stone's wall face below
    // it, so a set-back upper storey's front wall reads as a wall and not as more roof
    const float lift = shade ? 0.12f : 0.32f;
    if (hash3(gx, gy, m.seed + 13) % 23 == 0) return mix(D[std::max(0, k - 1)], D[4], lift);   // wear
    return mix(D[k], D[4], lift);
  }
  // (fix) a stepped pyramid's terraces, coloured from where the point lies on its tier (never from the height field's
  // gradient, which smeared each riser into a soft band): the tread's top lit from the top-left, its outer edge a
  // bright lip, its inner edge in the contact shadow of the riser above (deepest on the east and south sides, away
  // from the light), the summit paved in slabs
  if (m.shape == RoofShape::Stepped && s == Surf::Roof && !(m.stairX1 > m.stairX0 && x >= m.stairX0 && x < m.stairX1 && y > (m.y0 + m.y1) * 0.5f)) {
    const float rx0 = m.x0 - m.ov, rx1 = m.x1 + m.ov, ry0 = m.y0 - m.ov, ry1 = m.y1 + m.ovF;
    const float dy = std::min(y - ry0, ry1 - y), dx = std::min(x - rx0, rx1 - x);
    const float a = m.alongY ? dx : dy, b = m.alongY ? dy * m.hk : dx * m.hk;
    const float d = std::min(a, b), dmax = m.alongY ? (rx1 - rx0) * 0.5f : (ry1 - ry0) * 0.5f;
    const float t = std::clamp(d / std::max(1.0f, dmax), 0.0f, 1.0f);
    const bool side = b < a;   // on an east / west flank
    const bool east = side && x > (rx0 + rx1) * 0.5f, south = !side && y > (ry0 + ry1) * 0.5f;
    const int ix = (int)std::floor(x), iy = (int)std::floor(y);
    int k;
    if (t >= kStepSummit) {   // the summit: big slabs, a lit rim
      const float e = (t - kStepSummit) * dmax;
      k = ((ix % 7 + 7) % 7 == 0 || ((iy + (ix / 7) * 3) % 5 + 5) % 5 == 0) ? 2 : 3;
      if (e < 1.0f) k = east || south ? 3 : 4;
      if (hash3(ix, iy, m.seed + 41) % 19 == 0) k = 2;
    } else {
      const float f = t / kStepTread - std::floor(t / kStepTread), dep = kStepTread * dmax, inPx = f * dep;
      k = 3;
      if (((side ? iy : ix) % 6 + 6) % 6 == 0) k = 2;                     // the joints between the coping stones
      if (inPx < 1.0f) k = east ? 3 : 4;                                  // the lit lip of the tread's edge
      else if (dep - inPx < (east || south ? 2.2f : 1.1f)) k = 1;         // the riser above casts its contact shadow
      else if (east && dep - inPx < 3.6f) k = 2;
    }
    if (shade) k = std::max(0, k - 2);
    return R[k];
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
    if (m.round) { dcx = m.cx; dcy = m.cy; Rd = m.r + (m.shape == RoofShape::Onion ? 1.5f : 0.5f); }
    else {
      dcx = (m.x0 + m.x1) * 0.5f; dcy = (m.y0 + m.y1) * 0.5f - 1;
      Rd = std::min(m.x1 - m.x0, m.y1 - m.y0) * 0.5f - (m.shape == RoofShape::Onion ? 6 : 7);
    }
    const float ddx = x - dcx, ddy = (y - dcy) / (m.shape == RoofShape::Onion ? 1.0f : (m.round ? 0.8f : 0.74f)), rr = std::sqrt(ddx * ddx + ddy * ddy), tq = std::min(1.0f, rr / std::max(1.0f, Rd));
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
    // M3b a hammam's dome studded with glass oculi: rings of small lit lenses
    if ((m.feat & bld::VF_OCULI) && tq > 0.2f && tq < 0.85f) {
      const float a = (std::atan2(ddy, ddx) + PI) / TAU * 10.0f, ring = tq * 4.0f;
      if (std::fabs(a - std::floor(a) - 0.5f) < 0.14f && std::fabs(ring - std::floor(ring) - 0.5f) < 0.16f) return kk >= 2 ? kGlass[4] : kGlass[2];
    }
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
  // (M3c carry) a round cone turns smoothly round its axis: the light term dithered between the ramp steps along their
  // seams (as the domes are), so the lit flank (west, the sun's side) rolls over into the shaded east flank instead of
  // standing in five hard wedges; the eave flare catches a little more light than the flank above it
  if (m.round && elvenCone(m) && !m.snow) {
    float lv = (l + 0.30f) * 3.0f;
    const float tq = coneT(m, x, y);
    // (M3c fixer round 3) a clear lit-west / shaded-east split round the cone's axis (the review read the flanks as one
    // flat green): the side of the axis the pixel lies on, as a fraction of the cone's width at its height
    const float side = std::clamp((x - m.cx) / std::max(1.0f, (m.r + 2.5f) * tq), -1.0f, 1.0f);
    lv -= side * 0.95f;
    if (tq > 0.88f && tq < 0.97f) lv += 0.3f;
    int kb = (int)std::floor(lv);
    if (lv - kb > 0.5f + (bayer((int)std::floor(x), (int)std::floor(y - z)) - 0.5f) * 0.5f) kb++;
    k = std::clamp(kb, 1, 4);   // (the shaded flank keeps a little bounced light: never the darkest step)
    if (tq >= 0.97f) k = std::max(0, std::min(k, 3) - 1);   // the drip edge: a darker line where the leaves end
  }
  // (M3c fixer round 2, review: "snow-covered Sylvan cone roofs still read as garlic bulbs ... near-flat white teardrop,
  // almost no light/shadow banding and no visible eave thickness") a snowed cone keeps the same light rolling round
  // it as the bare one, on the snow's own ramp: the lit west flank white, the east flank falling through two cool blue
  // steps into shade (so the flanks read straight and the cone turns), and at the eave the snow ends in a shaded lip
  // over a band of the leaves / thatch under it (the roof's thickness), with a dark drip line below
  if (m.round && elvenCone(m) && m.snow) {
    float lv = (l + 0.42f) * 3.2f;
    const float tq = coneT(m, x, y);
    int kb = (int)std::floor(lv);
    if (lv - kb > 0.5f + (bayer((int)std::floor(x), (int)std::floor(y - z)) - 0.5f) * 0.5f) kb++;
    kb = std::clamp(kb, 0, 4);
    if (shade) kb = std::max(0, kb - 1);
    if (tq >= 0.965f) return R[std::clamp(kb - 2, 0, 1)];                 // the drip line
    if (tq >= 0.90f) return R[std::clamp(kb - 1, 1, 3)];                  // the leaves / thatch under the snow
    if (tq >= 0.86f) return kSnow[std::clamp(kb - 1, 0, 3)];              // the snow's shaded lip
    return kSnow[kb];
  }
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
  // M3b a tent's canvas or felt: smooth, sewn in panels (seams down from the peak on a round tent, across a ridge
  // tent), a hem of the culture's colours along the valance, the ridge pole's end caught by the light
  if (m.shape == RoofShape::Tent) {
    const float tt = (z - (m.zTop() - 1)) / std::max(1.0f, m.roofH);
    int kt = std::clamp(k, 1, 4);
    bool seam;
    if (m.round) { const float a = (std::atan2(y - m.cy, x - m.cx) + PI) / TAU * 12.0f; seam = std::fabs(a - std::floor(a) - 0.5f) > 0.44f; }
    else seam = (((int)std::floor(m.alongY ? y : x)) % 7 + 7) % 7 == 0;
    if (seam && tt > 0.12f) kt = std::max(0, kt - 1);
    if (tt < 0.14f) {   // the valance: the accent and a pale band
      const Ramp A = m.accent ? ramp(opaque(m.accent)) : kRed;
      const int q = (int)std::floor((m.round ? std::atan2(y - m.cy, x - m.cx) * m.r : (m.alongY ? y : x)) / 3.0f);
      return tt < 0.06f ? A[std::clamp(k, 1, 3)] : ((q & 1) ? A[std::clamp(k + 1, 2, 4)] : kCloth[std::clamp(k + 1, 2, 4)]);
    }
    if (shade) kt = std::max(0, kt - 1);
    return R[kt];
  }
  // (M3c carry) an elven cone keeps its leaf courses as shadow lines only: the lit flecks on every scale drowned the
  // light turning round the cone (the volume) in speckle
  if (elvenCone(m) && t > 0) t = 0;
  // keep the strongest highlight for the ridge itself
  if (k == 4 && t > 0) t = 0;
  // hip roofs: the courses on the two end planes run at right angles to the front's, and with the same shade lines on
  // all four planes the roof reads as nested trays. The end planes stay smooth (light and shade only), so the four
  // planes read as volumes and the front plane carries the courses.
  const bool hipRoof = !m.round && (m.shape == RoofShape::Hip || m.shape == RoofShape::Pagoda || m.shape == RoofShape::Mansard);
  const bool endPlane = hipRoof && (m.alongY ? facesY : !facesY);
  if (endPlane && t < 0 && m.rmat != RoofMat::Thatch && m.rmat != RoofMat::Palm) t = 0;   // (thatch keeps its strands on every plane)
  // (M3b round 3) a tiled hip's end planes keep no lit tile lines either: the barrel tiles' crests, laid across the
  // sloping facet, read at 1x as fine diagonal hatching that beat against the front plane's courses (a moire shimmer
  // on every jade hall)
  if (endPlane && t > 0 && (m.rmat == RoofMat::GlazedTile || m.rmat == RoofMat::ClayTile)) t = 0;
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
    // (M3b fixer: the very crest a pale line of light, so the hump's ridge still reads under the night grade, where
    // the greens of a sod roof all sank to one dark slab)
    if (z > zt - 0.7f && !m.snow && ny > -0.2f) return mix(R[4], rgba(232, 238, 204), 0.42f);
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
  // (M3b round 3) the snow keeps the roof's volume (gable and sod roofs went to flat white slabs with rows of dashes):
  // each plane takes its own light from its normal (the courses' lips are buried), the front plane rounds from a cool
  // blue shade at the eave up to white below the ridge (a dithered seam between the bands), the ridge a white crest
  // with a soft shadow line under it, the back plane bright, the east planes in blue shade
  auto snowK = [&]() {
    int ks = l > 0.56f ? 4 : (l > 0.16f ? 3 : (l > -0.16f ? 2 : 1));
    const float tts = std::clamp((z - (m.zTop() - 1)) / std::max(1.0f, m.roofH), 0.0f, 1.0f);
    if (!m.round && ny > 0.2f && std::fabs(nx) < 0.3f) {
      const float g = 1.0f + tts * 2.5f;
      int kb = (int)std::floor(g);
      if (g - kb > 0.5f + (bayer((int)std::floor(x), (int)std::floor(y - z)) - 0.5f) * 0.5f) kb++;
      ks = std::clamp(kb, 1, 4);
    }
    if (!m.round && m.shape != RoofShape::Pagoda) {
      if (z > zt - 1.0f) ks = 4;
      else if (z > zt - 1.7f && ny > 0.2f) ks = std::min(ks, 2);
    }
    if (!m.round && v <= 2 && ny > 0.2f) ks = std::max(1, ks - 1);   // the snow's thin, shaded edge over the eave
    if (shade) ks = std::max(1, ks - 1);
    return ks;
  };
  // snow lies on the sod too: the hump white above its soil and log edge, a tuft of grass showing through here and there
  if (m.snow && m.rmat == RoofMat::Turf && !m.round && v > 2) {
    if (hash3(u, v, m.seed + 11) % 13 == 0) return R[std::clamp(k - 1, 1, 3)];
    return kSnow[snowK()];
  }
  if (m.snow && m.rmat != RoofMat::Turf && v > 1 + (int)(hash3(u / 3, 0, m.seed + 7) % 3)) return kSnow[snowK()];
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
  const int nm = (int)sc.ms.size();
  // M3b: each mass's extent (walls, roofs, overhangs), so a column asks only the masses over it (a palace or a tent
  // court has twenty volumes) and the y range covered by the roofs and walls
  std::vector<float> ex0((size_t)nm), ex1((size_t)nm);
  float yMin = -8, yMax = (float)p.D + 6;
  for (int i = 0; i < nm; i++) {
    const Mass& m = sc.ms[(size_t)i];
    if (m.round) {
      const float R = m.r + (float)std::max(m.ov, 1) + 2.5f;
      ex0[(size_t)i] = m.cx - R; ex1[(size_t)i] = m.cx + R;
      yMin = std::min(yMin, m.cy - R - 1); yMax = std::max(yMax, m.cy + R + 1);
    } else {
      ex0[(size_t)i] = m.x0 - m.ov - 1; ex1[(size_t)i] = m.x1 + m.ov + 1;
      yMin = std::min(yMin, m.y0 - m.ov - 1); yMax = std::max(yMax, m.y1 + m.ovF + 1);
    }
  }
  yMin = std::max(yMin, -24.0f);
  yMax = std::min(yMax, (float)p.D + BLDG_PAD_B + 6);
  const float step = 0.25f;
  std::vector<int> cov, nearM;
  for (int x = std::max(xa, -BLDG_PAD_X); x < std::min(xb, p.W + BLDG_PAD_X); x++) {
    const float fx = x + 0.5f;
    cov.clear(); nearM.clear();
    for (int i = 0; i < nm; i++) {
      if (ex0[(size_t)i] <= fx && fx < ex1[(size_t)i]) cov.push_back(i);
      if (ex0[(size_t)i] <= fx + 1 && fx - 8.5f < ex1[(size_t)i]) nearM.push_back(i);
    }
    if (cov.empty()) continue;
    auto topAt = [&](float xx, float yy, int& mi, Surf& s) {
      float best = -1;
      mi = -1;
      s = Surf::None;
      for (int i : cov) {
        Surf ss;
        const float z = massTop(sc.ms[(size_t)i], xx, yy, ss);
        if (z > best) { best = z; mi = i; s = ss; }
      }
      return best;
    };
    // wall events in this column, by depth
    struct Ev { float y; int mi; };
    std::vector<Ev> evs;
    for (int mi : cov) {
      float fy = sc.ms[(size_t)mi].frontY(fx);
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
    // (fixer r2) the roofs seen under an eave: the column asks each y only for its highest roof, so where a higher
    // eave (a stave church's upper stage on the nave's ridge, a wing's or a tower's eave over the body's roof) ends,
    // the lower roof it overhangs, seen below the eave, was never painted and the ground showed through the building.
    // Walk back from y along the highest of the OTHER roofs and paint the rows (fromRow, toRow] it shows, only where
    // nothing is painted yet; the first rows under the eave lie in its shade.
    auto underEave = [&](int fromRow, int toRow, float y, int excl) {
      int need = toRow;
      const int cx = P.ox + x;
      if (cx < 0 || cx >= P.c.w) return;
      for (float yb = y - step; yb > y - 48 && need > fromRow; yb -= step) {
        int bm = -1;
        Surf sb = Surf::None;
        float zb = -1;
        for (int i : cov) {
          if (i == excl || sc.ms[(size_t)i].chimney) continue;
          Surf ss;
          const float zz = massTop(sc.ms[(size_t)i], fx, yb, ss);
          if (zz > zb) { zb = zz; bm = i; sb = ss; }
        }
        if (bm < 0 || zb < 0) continue;
        const int rb = P.rowOf(yb, zb);
        if (rb > need) continue;
        const Mass& mb = sc.ms[(size_t)bm];
        for (int r = std::max(rb, fromRow + 1); r <= need; r++) {
          if (r < 0 || r >= P.c.h || chA(P.c.get(cx, r))) continue;
          const bool sh = r <= fromRow + 3 || roofShadowed(sc, nearM, fx, yb, zb, bm);
          P.put(cx, r, roofColor(p, mb, bm, fx, yb, zb, sb, sh), 2);
        }
        need = rb - 1;
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
      float z = topAt(fx, y, mi, s);
      if (mi < 0 || z < 0) {
        if (prevMi >= 0 && prevRow != INT32_MAX) underEave(prevRow, P.rowOf(y, 0), y, prevMi);
        if (prevMi >= 0) fascia(prevRow, prevMi, prevS);
        prevRow = INT32_MAX; prevMi = -1; prevS = Surf::None;
        continue;
      }
      const Mass& m = sc.ms[mi];
      int row = P.rowOf(y, z);
      bool flatTop = s == Surf::Flat || s == Surf::Parapet;
      bool shade = (m.chimney ? false : roofShadowed(sc, nearM, fx, y, z, flatTop ? -1 : mi));
      uint32_t col = roofColor(p, m, mi, fx, y, z, s, shade);
      if (prevMi == mi && prevRow != INT32_MAX && row > prevRow + 1) {
        bool solid = s != Surf::Roof || prevS != Surf::Roof || prevZ > z + 2.5f;
        for (int r = prevRow + 1; r < row; r++) {
          uint32_t fc = col;
          if (solid) fc = (prevS == Surf::Parapet || prevS == Surf::CapRim) ? m.wR[std::max(0, (m.wall == WallKind::Adobe ? 2 : 1))] : darken(prevCol, 0.3f);
          if (solid && m.shape == RoofShape::Stepped && prevS == Surf::Roof) {
            // (fix) a terrace's riser: a face toward the viewer in coursed masonry, mid-tone like the south walls, a
            // dark course joint every third row and staggered head joints, its foot in shade
            const int rr = r - prevRow - 1, rows = row - prevRow - 1;
            int k = 2;
            if (rr % 3 == 2) k = 1;
            else if (((x + (rr / 3) * 4) % 8 + 8) % 8 == 0) k = 1;
            if (rr >= rows - 1) k = 0;
            fc = m.rR[k];
          }
          P.put(P.ox + x, r, fc, 2);
        }
      } else if (prevMi >= 0 && prevMi != mi && row > prevRow + 1) {
        underEave(prevRow, row - 1, y, prevMi);
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
  const int cul = p.cul;
  std::vector<Mass>& ms = p.sc.ms;
  if (ms.empty()) return;
  const Mass& main = ms[0];
  const Ramp A = st.accentTint ? ramp(opaque(st.accentTint)) : kRed;
  auto putG = [&](std::pair<int, int> q, uint32_t c, bool glass = false) { P.put(q.first, q.second, c, 3, glass); };
  // ---- ridge ends: dragon heads (jade terraces, fjords), finials, carved crossed gable boards (the body, its tiers,
  //      porches and gates)
  for (size_t i = 0; i < ms.size(); i++) {
    const Mass& mm = ms[i];
    if (!(i == 0 || mm.tier || mm.role == bld::VolRole::Porch || mm.role == bld::VolRole::Gate)) continue;
    const uint16_t orn = mm.orn;
    if (!(orn & (ORN_DRAGON_HEADS | ORN_FINIALS | ORN_CARVED_RIDGE))) continue;
    const Ridge rg = ridgeOf(mm);
    if (!rg.ok) continue;
    const Ramp& R = mm.rR;
    for (int e = 0; e < 2; e++) {
      const float x = e == 0 ? rg.xa : rg.xb, y = e == 0 ? rg.ya : rg.yb;
      const int dir = mm.alongY ? 0 : (e == 0 ? -1 : 1);
      auto q = at(x, y, rg.z);
      if ((orn & ORN_DRAGON_HEADS) && dir != 0) {
        // a curled ridge-end beast: a neck rising out of the ridge, the head turned outward, a curl over it
        static const char* dg[6] = {"..xx.", ".x..x", "...xx", "..xxx", ".xxx.", "xxx.."};
        for (int j = 0; j < 6; j++)
          for (int k = 0; k < 5; k++) {
            if (dg[j][k] != 'x') continue;
            const int px = q.first + (dir < 0 ? -k : k) - dir * 1, py = q.second - 5 + j;
            P.put(px, py, (orn & ORN_GILDING) ? kGold[j < 2 ? 4 : 2] : R[j < 3 ? 4 : (k < 2 ? 3 : 1)], 3);
          }
      } else if ((orn & ORN_CARVED_RIDGE) && mm.alongY && e == 1) {
        // crossed barge boards at the gable's peak, carved into curling heads (the fjords' gable ends)
        const auto top = at(x, mm.y1 + mm.ovF - 0.6f, rg.z);
        for (int k = 0; k < 6; k++) {
          P.put(top.first - 2 + k, top.second - k, kWood[k < 3 ? 3 : 4], 3);
          P.put(top.first + 2 - k, top.second - k, kWood[k < 3 ? 2 : 1], 3);
        }
        P.put(top.first + 4, top.second - 6, kWood[4], 3); P.put(top.first - 4, top.second - 6, kWood[2], 3);
        P.put(top.first + 5, top.second - 5, A[3], 3); P.put(top.first - 5, top.second - 5, A[2], 3);   // painted eyes
      } else if ((orn & ORN_FINIALS) && !(orn & ORN_DRAGON_HEADS)) {
        for (int k = 1; k <= 4; k++) P.put(q.first, q.second - k, (orn & ORN_GILDING) ? kGold[k == 4 ? 4 : 2] : (k == 4 ? R[4] : kIron[2]), 3);
        P.put(q.first - 1, q.second - 3, (orn & ORN_GILDING) ? kGold[3] : kIron[3], 3);
      }
    }
  }
  // finials on cones, domes, onion bulbs and spires (the spire's gilded vane)
  for (const Mass& m : ms) {
    if (m.chimney || m.found || m.encl || m.tree) continue;
    const bool tower = b == Building::Tower && m.body;
    if (!(m.orn & (ORN_FINIALS | ORN_GILDING)) && !tower && m.shape != RoofShape::Spire && m.shape != RoofShape::Onion) continue;
    const bool cone = m.round && (m.shape == RoofShape::Conical || m.shape == RoofShape::Onion || m.shape == RoofShape::Dome || m.shape == RoofShape::Pagoda ||
                                  m.shape == RoofShape::Sweep || m.shape == RoofShape::Spire);
    const bool bulb = !m.round && (m.shape == RoofShape::Onion || m.shape == RoofShape::Dome || m.shape == RoofShape::Spire);
    if (!cone && !bulb) continue;
    if (m.rmat == RoofMat::Felt || (m.feat & bld::VF_FLAG)) continue;
    Surf s;
    const float cx = m.round ? m.cx : (m.x0 + m.x1) * 0.5f, cy = m.round ? m.cy : (m.y0 + m.y1) * 0.5f - (m.shape == RoofShape::Spire ? 0 : 1);
    const float z = massTop(m, cx, cy, s);
    auto q = at(cx, cy, z);
    const bool gold = (m.orn & ORN_GILDING) != 0 || tower;
    for (int k = 1; k <= 5; k++) P.put(q.first, q.second - k, gold ? kGold[k >= 4 ? 4 : 2] : kIron[k >= 4 ? 3 : 1], 3);
    P.put(q.first - 1, q.second - 4, gold ? kGold[3] : kIron[2], 3);
    P.put(q.first + 1, q.second - 4, gold ? kGold[1] : kIron[1], 3);
  }
  // ---- lanterns under the front eaves: paper lanterns (jade, marsh, sun), elven glow-orbs, iron lamps elsewhere
  for (size_t i = 0; i < ms.size(); i++) {
    const Mass& body = ms[i];
    if (!((body.feat & bld::VF_LANTERNS) || (i == 0 && (body.orn & ORN_LANTERNS)))) continue;
    if (body.chimney || body.found || body.encl) continue;
    const bool elf = cul == CU_SYLVAN || cul == CU_STAR;
    const bool paper = cul == CU_JADE || cul == CU_MARSH || cul == CU_SUN || cul == CU_IMPERIAL || cul == CU_STEPPE;
    const float bx0 = body.round ? body.cx - body.r * 0.62f : body.x0 + 3, bx1 = body.round ? body.cx + body.r * 0.62f : body.x1 - 4;
    const float lz = (float)(body.zBase + body.wallH) - 1;
    const float xs[3] = {bx0, bx1, (float)p.doorX + 12.0f};
    for (int k = 0; k < 3; k++) {
      const float lx = xs[k];
      if (lx < bx0 - 0.5f || lx > bx1 + 0.5f) continue;
      const float ly = body.round ? body.frontY(lx) + 1.0f : body.y1 + std::max(1, body.ovF) - 0.5f;
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
          for (int kk = -1; kk <= 1; kk++) {
            const bool cap = j == 0 || j == 4;
            P.put(q.first + kk, q.second + 2 + j, cap ? kWoodDark[1] : L[kk < 0 ? 4 : (kk > 0 ? 2 : 3)], 3, !cap);
          }
        P.put(q.first - 2, q.second + 4, L[3], 3, true); P.put(q.first + 2, q.second + 4, L[1], 3, true);
      } else {
        for (int j = 0; j < 4; j++) { P.put(q.first - 1, q.second + 2 + j, kIron[1], 3); P.put(q.first + 1, q.second + 2 + j, kIron[0], 3); P.put(q.first, q.second + 2 + j, j == 0 ? kIron[2] : kGlow[3], 3, j > 0); }
      }
    }
  }
  // ---- prayer flags: strings of small flags from the roof's peak to the eave corners, sagging between
  const Ridge rg = ridgeOf(main);
  if ((main.orn & ORN_PRAYER_FLAGS) && (rg.ok || main.round)) {
    static const uint32_t fc[5] = {rgba(56, 96, 196), rgba(240, 240, 232), rgba(206, 52, 46), rgba(70, 150, 74), rgba(240, 200, 60)};
    Surf s;
    const float px = main.round ? main.cx : (rg.xa + rg.xb) * 0.5f, py = main.round ? main.cy : rg.ya;
    const float pz = main.round ? massTop(main, main.cx, main.cy, s) : rg.z;
    auto top = at(px, py, pz + 3);
    const float ends[2][2] = {{main.round ? main.cx - main.r - 2 : main.x0 - 3, (float)D + 1}, {main.round ? main.cx + main.r + 2 : main.x1 + 2, (float)D + 1}};
    for (int e = 0; e < 2; e++) {
      auto en = at(ends[e][0], ends[e][1], (float)main.zBase + 12);
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
  for (const Mass& m : ms) {
    if (!(m.orn & ORN_VINES) || m.chimney || m.found || m.tier || m.encl || m.tree || m.open) continue;
    const float mx0 = m.round ? m.cx - m.r * 0.8f : m.x0, mx1 = m.round ? m.cx + m.r * 0.8f : m.x1;
    const int w = (int)std::lround(mx1 - mx0);
    if (w < 12) continue;
    for (int k = 0; k < 4; k++) {
      const int u = k == 0 ? 1 : (k == 1 ? w - 3 : 6 + (int)(hash3(k, 61, p.seed) % (uint32_t)std::max(1, w - 12)));
      const int hgt = (int)(m.wallH * (k < 2 ? 0.85f : 0.45f + 0.1f * (hash3(k, 63, p.seed) % 3)));
      for (int z = 0; z < hgt; z++) {
        const int wob = (int)std::lround(std::sin(z * 0.5f + k) * 1.2f);
        for (int dx = -1; dx <= 1; dx++) {
          if (hash3(u + dx, z, p.seed + 67) % 3 == 0) continue;
          const float xx = mx0 + u + dx + wob + 0.5f;
          // (fixer round 3) no ivy floating over an open front's doorway or recess: it grows on wall, not on air
          bool overOpen = false;
          for (const Mass& o : ms)
            if (o.open && !o.round && o.y1 >= m.y1 - 0.5f && xx >= o.x0 - 1 && xx < o.x1 + 1 && z <= o.zBase + o.wallH) overOpen = true;
          if (overOpen) continue;
          auto q = at(xx, m.round ? m.frontY(xx) : m.y1 - 0.5f, (float)(m.zBase + z));
          if (P.layer[(size_t)std::clamp(q.second, 0, P.c.h - 1) * P.c.w + std::clamp(q.first, 0, P.c.w - 1)] != 1) continue;
          putG(q, kLeaf[(dx < 0 ? 3 : 2) - (z % 4 == 0 ? 1 : 0)]);
        }
      }
    }
  }
  // ---- the wood elves' root buttresses: the walls' trunks flare into roots that grip the ground
  if (cul == CU_SYLVAN)
    for (const Mass& m : ms) {
      if (m.wall != WallKind::Living || m.chimney || m.found || m.tier || m.encl || m.zBase > 0) continue;
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
  // ---- a colossal tree's crown (the tree palace, a tree shrine): a broad canopy of leaves over its trunk, lit from the
  //      top-left, gold leaves and glow-motes in it, a few great boughs
  for (const Mass& m : ms) {
    if (!m.tree) continue;
    // (M3c carry) a trunk rising through a roof (a tree temple's cone) carries its crown above that roof's point, not
    // stuck on its flank
    float zc = m.zTop();
    for (const Mass& o : ms) {
      if (&o == &m || o.tree) continue;
      Surf so;
      const float zo = massTop(o, m.cx, m.cy, so);
      if (zo > zc) zc = zo - 3.0f;
    }
    auto base = at(m.cx, m.cy, zc);
    const int topLim = p.sc.top - p.budget + 3;
    const int rx = (int)(m.r * 3.0f + 12), ry = (int)(m.r * 1.7f + 9);
    const int cy = std::max(topLim + ry - 2, base.second - ry / 2);
    for (int k = 0; k < 4; k++) {   // the boughs
      const int dir = (k & 1) ? 1 : -1, len = rx * (6 + (k / 2) * 2) / 10;
      for (int i = 0; i < len; i++) {
        const int yy = base.second - i * (2 + k / 2) / 3 - 2;
        if (yy < topLim) break;
        P.put(base.first + dir * i, yy, kLiving[i < len / 2 ? 2 : 1], 3);
        P.put(base.first + dir * i, yy + 1, kLiving[1], 3);
      }
    }
    for (int j = -ry; j <= ry; j++)
      for (int i = -rx; i <= rx; i++) {
        const float dx = (float)i / rx, dy = (float)j / ry;
        const float wob = 0.12f * std::sin(i * 0.7f + j * 0.3f) + 0.1f * std::sin(i * 0.31f - j * 0.9f) + 0.06f * std::sin(i * 1.3f + j * 1.7f);
        if (dx * dx + dy * dy > 1.0f + wob) continue;
        const int yy = cy + j;
        if (yy < topLim) continue;
        // clumps: the canopy is several lobes, each lit on its upper left
        const float lob = std::sin(i * 0.33f + (float)(p.seed % 7)) * std::sin(j * 0.45f + 1.3f);
        const float l = -dx * 0.6f - dy * 0.7f + lob * 0.35f + (hashf(i / 2, j / 2, p.seed) - 0.5f) * 0.4f;
        int k = l > 0.55f ? 4 : (l > 0.15f ? 3 : (l > -0.35f ? 2 : 1));
        if (dx * dx + dy * dy > 0.86f + wob && dy > 0) k = std::max(0, k - 1);   // the canopy's underside in shade
        uint32_t c = kLeafRoof[k];
        if (hash3(i, j, p.seed + 3) % 41 == 0) c = rgba(236, 196, 90);
        P.put(base.first + i, yy, c, 3);
      }
    for (int k = 0; k < 9; k++) {   // glow-motes in the crown
      const int gx = base.first - rx * 2 / 3 + (int)(hash3(k, 91, p.seed) % (uint32_t)(rx * 4 / 3)), gy = cy - ry / 2 + (int)(hash3(k, 92, p.seed) % (uint32_t)ry);
      if (gy >= topLim) P.put(gx, gy, kGlowElf[4], 3, true);
    }
  }
}

// a sign's icon, drawn into a box of about 6 x 5 px with its top-left at (ix, iy)
template <class PUT>
void drawIcon(PUT&& put, int ix, int iy, int icon) {
  switch (icon) {
    case bld::ICON_MUG:   // a mug of ale
      for (int j = 0; j < 4; j++) for (int i = 0; i < 3; i++) put(ix + i, iy + j, j == 0 ? kWhite : kGold[3 - (i == 2)]);
      put(ix + 3, iy + 1, kGold[2]); put(ix + 3, iy + 2, kGold[2]);
      break;
    case bld::ICON_ANVIL:
      for (int i = -1; i < 5; i++) put(ix + i, iy + 1, kIron[3]);
      for (int i = 0; i < 4; i++) put(ix + i, iy + 2, kIron[2]);
      put(ix + 1, iy + 3, kIron[1]); put(ix + 2, iy + 3, kIron[1]);
      break;
    case bld::ICON_LOAF:
      for (int i = -1; i <= 4; i++) put(ix + i, iy + 2, kGold[1]);
      for (int i = -1; i <= 4; i++) put(ix + i, iy + 1, kGold[i < 1 ? 3 : 2]);
      for (int i = 0; i <= 3; i++) put(ix + i, iy, kGold[i < 2 ? 4 : 3]);
      put(ix, iy + 1, kGold[4]); put(ix + 2, iy + 1, kGold[1]);
      break;
    case bld::ICON_HAM:
      for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) put(ix + i, iy + j, kRed[j == 0 && i == 0 ? 4 : (i == 2 || j == 2 ? 1 : 3)]);
      put(ix + 3, iy + 2, kBone[3]); put(ix + 4, iy + 3, kBone[4]); put(ix + 3, iy + 3, kBone[2]);
      break;
    case bld::ICON_FISH:
      for (int i = -1; i <= 3; i++) { put(ix + i, iy + 1, kIron[3]); put(ix + i, iy + 2, kIron[2]); }
      put(ix, iy, kIron[4]); put(ix + 1, iy, kIron[3]);
      put(ix + 4, iy, kIron[2]); put(ix + 4, iy + 1, kIron[1]); put(ix + 4, iy + 2, kIron[1]); put(ix + 4, iy + 3, kIron[2]);
      put(ix - 1, iy + 1, kInk);
      break;
    case bld::ICON_YARN:
      for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) if (!((i == 0 || i == 3) && (j == 0 || j == 3))) put(ix + i, iy + j, kPurple[(i + j) % 2 ? 3 : 2]);
      put(ix + 1, iy, kPurple[4]); put(ix + 4, iy + 3, kWood[3]); put(ix + 5, iy + 3, kWood[2]);
      break;
    case bld::ICON_HIDE:
      for (int j = 0; j < 5; j++) {
        int hw = j == 0 || j == 4 ? 1 : 2;
        for (int i = -hw; i <= hw; i++) put(ix + 1 + i, iy - 1 + j, kLeather[i < 0 ? 4 : (i > 0 ? 2 : 3)]);
      }
      break;
    case bld::ICON_INGOT:
      for (int i = 0; i <= 3; i++) put(ix + i, iy + 1, kGold[4]);
      for (int i = -1; i <= 4; i++) put(ix + i, iy + 2, kGold[3]);
      for (int i = -1; i <= 4; i++) put(ix + i, iy + 3, kGold[1]);
      break;
    case bld::ICON_SHEAF:
      for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++) if (j >= 2 || (i + j) % 2 == 0) put(ix - 1 + i, iy - 1 + j, kGold[j < 2 ? 4 : (j == 3 ? 1 : 3)]);
      break;
    case bld::ICON_SAW:
      for (int i = -1; i <= 4; i++) put(ix + i, iy + 1, kIron[3]);
      for (int i = -1; i <= 4; i += 2) put(ix + i, iy + 2, kIron[2]);
      put(ix - 2, iy, kWood[3]); put(ix - 2, iy + 1, kWood[3]); put(ix + 5, iy, kWood[2]); put(ix + 5, iy + 1, kWood[2]);
      for (int i = -2; i <= 5; i++) put(ix + i, iy - 1, kWood[2]);
      break;
    case bld::ICON_SACK:
      for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) put(ix + i, iy + j, kCloth[i == 0 ? 4 : (i == 3 || j == 3 ? 2 : 3)]);
      put(ix + 1, iy - 1, kCloth[3]); put(ix + 2, iy - 1, kCloth[2]); put(ix + 1, iy, kWood[1]); put(ix + 2, iy, kWood[1]);
      break;
    case bld::ICON_SCALES:   // a balance: the beam, two pans, the post
      for (int i = -1; i <= 5; i++) put(ix + i, iy, kGold[3]);
      for (int j = 0; j < 4; j++) put(ix + 2, iy + j, kGold[2]);
      put(ix - 1, iy + 2, kGold[4]); put(ix, iy + 2, kGold[3]); put(ix + 4, iy + 2, kGold[3]); put(ix + 5, iy + 2, kGold[2]);
      put(ix + 1, iy + 3, kGold[1]); put(ix + 3, iy + 3, kGold[1]);
      break;
    case bld::ICON_KEY:   // the guild's key
      put(ix, iy, kGold[4]); put(ix + 1, iy, kGold[3]); put(ix, iy + 1, kGold[3]); put(ix + 1, iy + 1, kGold[1]);
      for (int i = 2; i <= 5; i++) put(ix + i, iy + 1, kGold[2]);
      put(ix + 4, iy + 2, kGold[2]); put(ix + 5, iy + 2, kGold[1]);
      break;
    case bld::ICON_STEAM:   // water and its steam
      for (int i = 0; i < 5; i++) put(ix + i, iy + 3, kWater[i & 1 ? 2 : 3]);
      put(ix + 1, iy + 1, kWhite); put(ix + 1, iy, kCloth[3]); put(ix + 3, iy + 2, kWhite); put(ix + 3, iy + 1, kCloth[3]); put(ix + 3, iy, kWhite);
      break;
    case bld::ICON_CUP:   // a tea bowl and its steam
      for (int i = 0; i < 5; i++) put(ix + i, iy + 2, kWhite);
      for (int i = 1; i < 4; i++) put(ix + i, iy + 3, kCloth[3]);
      put(ix + 1, iy + 1, kLeaf[3]); put(ix + 2, iy + 1, kLeaf[2]); put(ix + 3, iy + 1, kLeaf[3]);
      put(ix + 2, iy - 1, kCloth[3]); put(ix + 3, iy - 2, kCloth[2]);
      break;
    case bld::ICON_SHIELD:   // a round shield on crossed axes
      for (int j = 0; j < 5; j++) for (int i = 0; i < 5; i++) { const int dx = i - 2, dy = j - 2; if (dx * dx + dy * dy <= 5) put(ix + i, iy + j - 1, dx * dx + dy * dy <= 1 ? kGold[4] : kRed[dx + dy < 0 ? 3 : 2]); }
      put(ix - 1, iy - 2, kIron[3]); put(ix + 5, iy - 2, kIron[3]);
      break;
    case bld::ICON_HORN:   // a drinking horn
      put(ix, iy, kBone[4]); put(ix + 1, iy + 1, kBone[3]); put(ix + 2, iy + 2, kBone[3]); put(ix + 3, iy + 2, kBone[2]); put(ix + 4, iy + 1, kGold[3]); put(ix + 5, iy, kGold[4]);
      put(ix + 1, iy, kBone[3]); put(ix + 2, iy + 1, kBone[2]); put(ix + 3, iy + 1, kBone[2]);
      break;
    case bld::ICON_CIRCLE:   // the moot ring
      for (int j = 0; j < 5; j++) for (int i = 0; i < 5; i++) { const int dx = i - 2, dy = j - 2, d = dx * dx + dy * dy; if (d >= 3 && d <= 5) put(ix + i, iy + j - 1, kGold[dx + dy < 0 ? 4 : 2]); }
      put(ix + 2, iy + 1, kGold[3]);
      break;
    case bld::ICON_STAR:
      put(ix + 2, iy - 1, kGold[4]); for (int i = 0; i < 5; i++) put(ix + i, iy + 1, kGold[3]);
      put(ix + 2, iy, kGold[4]); put(ix + 2, iy + 2, kGold[2]); put(ix + 1, iy + 3, kGold[2]); put(ix + 3, iy + 3, kGold[1]);
      break;
    default:  // a coin pouch
      for (int j = 1; j < 4; j++) for (int i = 0; i < 4; i++) put(ix + i, iy + j, kLeather[3 - (i == 3)]);
      put(ix + 1, iy, kLeather[2]); put(ix + 2, iy, kLeather[2]); put(ix + 1, iy + 2, kGold[4]);
      break;
  }
}

void overlays(Painter& P, BuildingInfo* info) {
  Plan& p = P.p;
  const Building b = p.b;
  const int ox = P.ox;
  const int D = p.D;
  std::vector<Mass>& ms = p.sc.ms;
  auto at = [&](float x, float y, float z) { return std::pair<int, int>(ox + (int)std::floor(x), P.rowOf(y, z)); };
  const Mass& dm = ms[(size_t)p.doorMass];
  const float doorY = dm.round ? dm.frontY((float)p.doorX + 0.5f) + 0.5f : dm.y1;   // the door's wall face
  // ---- stilts and the shade beneath a raised floor
  for (const Mass& m : ms) {
    if (!(m.feat & bld::VF_STILTS) || m.zBase <= 0 || m.chimney) continue;
    const float fx0 = m.round ? m.cx - m.r : m.x0, fx1 = m.round ? m.cx + m.r : m.x1;
    for (int x = (int)std::floor(fx0); x < (int)std::ceil(fx1); x++) {
      const float fy = m.round ? m.frontY(x + 0.5f) : m.y1 - 0.5f;
      if (fy < -1e8f) continue;
      for (int z = 0; z < m.zBase; z++) {
        auto q = at(x + 0.5f, fy, (float)z);
        if (!chA(P.c.get(q.first, q.second))) P.put(q.first, q.second, rgba(30, 26, 40, 150), 3);
      }
    }
    for (float x = fx0 + 1; x < fx1 - 1; x += 13) {
      const float fy = m.round ? m.frontY(x + 0.5f) : m.y1 - 0.5f;
      if (fy < -1e8f) continue;
      for (int z = -2; z < m.zBase; z++) {
        auto q = at(x + 0.5f, fy, (float)z);
        P.put(q.first, q.second, kLog[3], 3);
        P.put(q.first + 1, q.second, kLog[1], 3);
      }
    }
  }
  if ((dm.feat & bld::VF_STILTS) && dm.zBase > 0) {   // a ladder down from the door
    for (int z = -2; z < dm.zBase; z++) {
      auto q = at(p.doorX - 3.5f, doorY + 1.5f, (float)z);
      P.put(q.first, q.second, kWood[3], 3);
      P.put(q.first + 6, q.second, kWood[1], 3);
      if ((z & 1) == 0) for (int i = 1; i < 6; i++) P.put(q.first + i, q.second, kWood[2], 3);
    }
  }
  // (fixer r2) an open front on the ground: the px span of its ways in (openWalkable), for the steps and the threshold
  int wx0 = 1 << 30, wx1 = -1;
  for (int i = 0; i < p.nVols; i++) {
    if (!ms[(size_t)i].open) continue;
    for (int x = (int)ms[(size_t)i].x0; x < (int)ms[(size_t)i].x1; x++)
      if (openWalkable(p, i, x)) { wx0 = std::min(wx0, x); wx1 = std::max(wx1, x); }
  }
  // ---- steps up a platform to the door: treads lit, risers in shade, a cheek wall either side
  for (const Mass& f : ms) {
    if (!f.found || !(f.feat & bld::VF_STEPS) || (f.feat & bld::VF_STILTS) || f.round) continue;
    const int h = (int)f.zTop();
    if (h < 2) continue;
    const Ramp& S = f.wR;
    const int n = (h + 1) / 2;
    float sx = (p.doorX >= f.x0 + 8 && p.doorX < f.x1 - 8) ? (float)p.doorX : (f.x0 + f.x1) * 0.5f;
    const float y1 = f.y1 - 0.1f, y0 = std::max(f.y0 + 0.5f, f.y1 - 3.9f), dd = (y1 - y0) / n;
    int sw = b == Building::Palace || b == Building::Temple || b == Building::Keep || p.seat ? 9 : 6, sa = -sw;
    if (wx0 < wx1) {   // (fixer r2) an open front on the ground: the flight runs the width of its bays, no door hinted
      sx = (float)wx0; sa = -1; sw = wx1 - wx0 + 1;
      sx = std::max(sx, f.x0 + 1.0f);
      sw = std::min(sw, (int)(f.x1 - 1 - sx));
    }
    for (int s = 0; s < n; s++) {
      const float zt = (float)h - s * ((float)h / n), zb = zt - (float)h / n;
      for (int i = sa; i < sw; i++) {
        const float x = sx + i + 0.5f;
        const bool cheek = i == sa || i == sw - 1;
        for (float yy = y0 + s * dd; yy < y0 + (s + 1) * dd; yy += 0.5f) {
          auto q = at(x, yy, cheek ? (float)h - s * 0.5f : zt);
          P.put(q.first, q.second, cheek ? S[i == sa ? 4 : 2] : S[i == sa + 1 ? 4 : 3], 3);
        }
        for (float z = zt - 1; z >= std::max(0.0f, zb); z -= 1.0f) {
          auto q = at(x, y0 + (s + 1) * dd - 0.25f, z);
          P.put(q.first, q.second, cheek ? S[i == sa ? 3 : 1] : S[1], 3);
        }
      }
    }
  }
  // ---- a stone step before a door on the ground
  if (dm.zBase == 0 && dm.face != bld::Face::BarnDoor && !dm.open && dm.face != bld::Face::Gate && !dm.found && !p.open.open()) {
    for (int i = -6; i <= 5; i++) {
      auto q = at(p.doorX + i + 0.5f, doorY + 0.5f, 0);
      P.put(q.first, q.second, kStone[i == -6 ? 4 : 3], 3);
      P.put(q.first, q.second + 1, kStone[1], 3);
    }
  }
  // ---- the ground under a jettied floor (in front of the set-back ground floor and at its sides) lies in its shade
  for (const Mass& m : ms) {
    if (!m.jetty) continue;
    for (int x = (int)m.x0; x < (int)m.x1; x++)
      for (int z = -1; z <= m.jettyV; z++) {
        auto q = at(x + 0.5f, m.y1 - 0.5f, (float)(m.zBase + z));
        if (!chA(P.c.get(q.first, q.second))) P.put(q.first, q.second, rgba(30, 26, 40, 170), 3);
      }
  }
  // ---- two-storey variety: a timber balcony on the upper floor over the door, or a little hood roof over the door
  for (const Mass& m : ms) {
    if (!(m.feat & (bld::VF_BALCONYDOOR | bld::VF_HOOD)) || m.round) continue;
    const float y1 = m.y1;
    if ((m.feat & bld::VF_BALCONYDOOR) && m.floorV > 0) {
      const float zf = (float)(m.zBase + m.floorV + (m.jetty ? 3 : 2));
      const float bx0 = (float)p.doorX - 12, bx1 = (float)p.doorX + 12;
      const float dep = 4;
      for (int x = (int)bx0 + 1; x < (int)bx1 + 2; x++)   // its shadow on the wall below, falling down-right
        for (int k = 1; k <= 4; k++) {
          auto q = at(x + 0.5f, y1 - 0.5f, zf - k);
          uint32_t c = P.c.get(q.first, q.second);
          if (chA(c) && P.layer[(size_t)q.second * P.c.w + q.first] == 1) P.c.set(q.first, q.second, darken(c, k <= 2 ? 0.5f : 0.3f));
        }
      for (int s = 0; s < 2; s++) {   // brackets
        float x = s == 0 ? bx0 + 2 : bx1 - 3;
        for (int k = 0; k <= 4; k++) {
          auto q = at(x + 0.5f, y1 + k * dep / 5.0f, zf - 5 + k);
          P.put(q.first, q.second, kWood[s == 0 ? 3 : 2], 3);
          P.put(q.first + 1, q.second, kWood[1], 3);
        }
      }
      for (int x = (int)bx0; x < (int)bx1; x++) {   // the deck, then its front board
        for (float yy = y1; yy < y1 + dep; yy += 0.5f) {
          auto q = at(x + 0.5f, yy, zf);
          P.put(q.first, q.second, (x - (int)bx0) % 3 == 0 ? kWoodDark[1] : kWoodDark[2], 3);
        }
        auto q = at(x + 0.5f, y1 + dep - 0.5f, zf);
        P.put(q.first, q.second + 1, kWood[1], 3);
        P.put(q.first, q.second + 2, kWood[0], 3);
      }
      for (int x = (int)bx0; x < (int)bx1; x++) {   // the railing: posts at the corners, balusters, a lit hand rail
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
      for (int s = 0; s < 2; s++) {
        float x = s == 0 ? bx0 : bx1 - 1;
        for (float yy = y1; yy < y1 + dep; yy += 0.5f) {
          auto q = at(x + 0.5f, yy, zf + 6);
          P.put(q.first, q.second, kWood[s == 0 ? 4 : 2], 3);
        }
      }
    } else if (m.feat & bld::VF_HOOD) {
      // the hood: a short pent roof on two brackets over the door, in the roof's material, with its shadow
      const float zh = (float)(m.zBase + std::min(19, m.wallH - 2)), hx0 = (float)p.doorX - 9, hx1 = (float)p.doorX + 9, dep = 6;
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
          if (m.rmat != RoofMat::Thatch && (x - (int)hx0) % 4 == 3) k = std::max(1, k - 1);
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
  const float frontZ0 = (float)dm.zBase;
  const int doorTop = std::min(dm.wallH - 3, dm.storeys >= 2 && dm.floorV > 0 ? dm.floorV - 1 : 22);
  auto putO = [&](int x, int y, uint32_t c) { P.put(x, y, c, 3); };
  // ---- signs (bld::Signage: the trade's icon on a hanging board, a board, a banner, an awning, lanterns, a carved
  //      panel or a totem pole)
  auto hanging = [&](float x, float z, int icon, float sy0 = -1e9f) {
    auto q = at(x, sy0 > -1e8f ? sy0 : doorY + 1.5f, z);
    int sx = q.first, sy = q.second;
    for (int i = 0; i <= 10; i++) P.put(sx + i, sy, kIron[2], 3);
    P.put(sx, sy + 1, kIron[1], 3);
    P.put(sx + 2, sy + 1, kIron[3], 3); P.put(sx + 9, sy + 1, kIron[3], 3);
    for (int j = 0; j < 8; j++)
      for (int i = 0; i < 11; i++) {
        int k = (i == 0 || j == 0) ? 4 : ((i == 10 || j == 7) ? 1 : 3);
        P.put(sx + 1 + i, sy + 2 + j, kWood[k], 3);
      }
    drawIcon(putO, sx + 5, sy + 4, icon);
  };
  const int icon = p.sign.icon;
  const float sideX = (p.doorX + 22 < (dm.round ? dm.cx + dm.r * 0.8f : dm.x1)) ? (float)p.doorX + 9 : (float)p.doorX - 21;
  const Ramp SA = p.st.accentTint ? ramp(opaque(p.st.accentTint)) : kRed;
  switch (p.sign.kind) {
    case bld::Sign::Hanging: hanging(sideX, frontZ0 + std::min(26, std::max(12, doorTop + 4)), icon); break;
    case bld::Sign::Board: {   // a board over the door
      auto q = at((float)p.doorX - 9, doorY + 0.5f, frontZ0 + doorTop + 9);
      for (int j = 0; j < 8; j++)
        for (int i = 0; i < 19; i++) P.put(q.first + i, q.second + j, (i == 0 || j == 0) ? kWood[4] : ((i == 18 || j == 7) ? kWood[1] : kWoodDark[2]), 3);
      drawIcon(putO, q.first + 7, q.second + 2, icon);
      break;
    }
    case bld::Sign::Banner: {   // a long banner on a pole beside the door, the icon at its head
      const float bx = (float)p.doorX + 11;
      auto base = at(bx, doorY + 1.5f, frontZ0);
      const int ph = std::min(30, doorTop + 10);
      for (int k = 0; k < ph; k++) { P.put(base.first, base.second - k, kWood[k == ph - 1 ? 4 : 2], 3); }
      for (int j = 0; j < 15; j++)
        for (int i = 1; i <= 6; i++) {
          if (j >= 13 && (i == 3 || i == 4)) continue;
          P.put(base.first + i, base.second - ph + 2 + j, (i == 1 || j == 0) ? SA[3] : (i == 6 ? SA[1] : SA[2]), 3);
        }
      drawIcon(putO, base.first + 1, base.second - ph + 4, icon);
      break;
    }
    case bld::Sign::Lantern: {   // red paper lanterns either side of the door, a narrow board with the icon over it
      for (int s = -1; s <= 1; s += 2) {
        auto q = at((float)p.doorX + s * 9, doorY + 1.0f, frontZ0 + doorTop + 4);
        P.put(q.first, q.second - 1, kIron[1], 3);
        for (int j = 0; j < 6; j++)
          for (int k = -1; k <= 1; k++) {
            const bool cap = j == 0 || j == 5;
            P.put(q.first + k, q.second + j, cap ? kWoodDark[1] : kLacquer[k < 0 ? 4 : (k > 0 ? 2 : 3)], 3, !cap);
          }
        P.put(q.first - 2, q.second + 2, kLacquer[3], 3, true); P.put(q.first + 2, q.second + 2, kLacquer[1], 3, true);
        P.put(q.first, q.second + 6, kGold[3], 3);
      }
      auto q = at((float)p.doorX - 5, doorY + 0.5f, frontZ0 + doorTop + 9);
      for (int j = 0; j < 7; j++) for (int i = 0; i < 11; i++) P.put(q.first + i, q.second + j, (i == 0 || i == 10 || j == 0 || j == 6) ? kLacquer[1] : kWoodDark[1], 3);
      drawIcon(putO, q.first + 3, q.second + 2, icon);
      break;
    }
    case bld::Sign::Carved: {   // a carved and painted panel over the door, curls at its ends
      auto q = at((float)p.doorX - 8, doorY + 0.5f, frontZ0 + doorTop + 8);
      for (int j = 0; j < 7; j++)
        for (int i = 0; i < 17; i++) {
          if ((i == 0 || i == 16) && (j == 0 || j == 6)) continue;
          P.put(q.first + i, q.second + j, (j == 0 || i == 0) ? kWood[4] : ((j == 6 || i == 16) ? kWood[1] : SA[1]), 3);
        }
      P.put(q.first - 1, q.second + 1, kWood[3], 3); P.put(q.first - 2, q.second + 2, kWood[3], 3); P.put(q.first - 1, q.second + 3, kWood[2], 3);
      P.put(q.first + 17, q.second + 1, kWood[2], 3); P.put(q.first + 18, q.second + 2, kWood[2], 3); P.put(q.first + 17, q.second + 3, kWood[1], 3);
      drawIcon(putO, q.first + 6, q.second + 2, icon);
      break;
    }
    case bld::Sign::Totem: {   // a pole planted by the door: a horse-tail standard, the icon on a disc
      auto base = at((float)p.doorX + 12, doorY + 2.0f, 0);
      for (int k = 0; k < 28; k++) { P.put(base.first, base.second - k, kWood[3], 3); P.put(base.first + 1, base.second - k, kWood[1], 3); }
      for (int j = 0; j < 8; j++) for (int i = -1 - j / 3; i <= 2 + j / 3; i++) P.put(base.first + i, base.second - 26 + j, ((i + j) & 1) ? kInk : rgba(60, 44, 40), 3);
      for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) { const int dx = i - 3, dy = j - 3; if (dx * dx + dy * dy <= 10) P.put(base.first - 2 + i, base.second - 36 + j, dx * dx + dy * dy >= 8 ? kGold[2] : kWood[1], 3); }
      drawIcon(putO, base.first - 1, base.second - 35, icon);
      break;
    }
    default: break;
  }
  // ---- lanterns by the door of the inn and the great houses
  if ((dm.feat & bld::VF_LAMPS) || (ms[0].feat & bld::VF_LAMPS) || b == Building::Keep || b == Building::Temple || b == Building::Palace || b == Building::Barracks) {
    if (p.sign.kind != bld::Sign::Lantern && !dm.open && dm.face != bld::Face::Iwan)
      for (int s = -1; s <= 1; s += 2) {
        auto q = at((float)p.doorX + s * (b == Building::Palace ? 13 : (b == Building::Keep ? 11 : 9)), doorY + 0.5f, frontZ0 + std::min(doorTop - 1, b == Building::Palace ? 18 : 15));
        P.put(q.first, q.second - 1, kIron[2], 3);
        P.put(q.first, q.second, kGlow[4], 3, true);
        P.put(q.first, q.second + 1, kGlow[2], 3, true);
        P.put(q.first - 1, q.second, kIron[1], 3);
        P.put(q.first + 1, q.second, kIron[1], 3);
      }
  }
  // ---- awnings: a striped canopy over a shop front, plain cloth over desert doors and windows
  const bool homeish = b == Building::House || b == Building::StoneHouse || b == Building::Hut || b == Building::Inn || b == Building::Tanner || b == Building::TeaHouse;
  if ((p.sign.kind == bld::Sign::Awning || ((p.st.awnings || (p.st.ornament & ORN_AWNINGS)) && homeish)) && !dm.open && dm.face != bld::Face::Gate) {
    const Mass& m = dm;
    const bool shopfront = p.sign.kind == bld::Sign::Awning;
    static const Ramp* cloths[3] = {&kRed, &kCloth, &kPurple};
    const Ramp A = (p.cul && (p.st.accentTint || p.st.altTint)) ? ramp(opaque((hash3(4, 4, p.seed) & 1) && p.st.altTint ? p.st.altTint : (p.st.accentTint ? p.st.accentTint : p.st.altTint)))
                                                                : *cloths[hash3(4, 4, p.seed) % 3];
    // each trade's awning in its own colours (the general store keeps red and white)
    const Ramp SR = b == Building::Bakery ? ramp(rgba(206, 150, 56)) : b == Building::Fishmonger ? ramp(rgba(56, 96, 168))
                  : b == Building::Weaver ? kPurple : b == Building::Butcher ? ramp(rgba(150, 40, 44)) : kRed;
    float ax0 = shopfront ? m.x0 + 3 : p.doorX - 9.0f, ax1 = shopfront ? m.x1 - 3 : p.doorX + 9.0f;
    float az = frontZ0 + (shopfront ? 26.0f : 19.0f);
    if (m.floorV > 0) az = frontZ0 + (float)std::min(m.floorV - 1, shopfront ? 26 : 19);
    const bool rnd = m.round;
    if (rnd) {   // a round mass: a short hood over the door that follows the curve, hung under the eave
      const float hw = std::min(shopfront ? 10.0f : 8.0f, m.r * 0.5f);
      ax0 = (float)p.doorX - hw; ax1 = (float)p.doorX + hw;
    }
    az = std::min(az, frontZ0 + (float)std::max(14, m.wallH - 3));
    const float fyc = rnd ? std::min((float)D, m.frontY((float)p.doorX + 0.5f)) : m.y1;
    // (M5 fixer, review: "the awning is a flat, see-through checker band with no valance, thickness or shadow") the
    // shadow it casts on the wall below it first (top-left light: down the face, a little to the right), then the
    // canvas as a solid sloping sheet (sampled finer than a pixel row: no gaps for the window to show through), lit at
    // its top and darker toward its front edge, then a valance hanging from the front edge with a scalloped hem
    if (!rnd)
      for (int x = (int)ax0 + 1; x <= (int)ax1; x++)
        for (int j = 0; j < 5; j++) {
          auto q = at(x + 0.5f, fyc + 0.2f, az - 4 - j);
          if (q.first < 0 || q.second < 0 || q.first >= P.c.w || q.second >= P.c.h) continue;
          if (P.layer[(size_t)q.second * P.c.w + q.first] != 1) continue;
          const uint32_t w0 = P.c.get(q.first, q.second);
          P.c.set(q.first, q.second, mix(w0, kInk, j < 3 ? 0.42f : 0.22f));
        }
    for (int x = (int)ax0; x < (int)ax1; x++)
      for (int k = 0; k <= 20; k++) {
        const float t = k * 0.25f;   // 0 at the wall, 5 at the front edge
        auto q = at(x + 0.5f, fyc + 0.5f + t * 0.5f, az - t);
        bool stp = ((x / 4) & 1) == 0;
        const Ramp& R = shopfront ? (stp ? SR : kCloth) : A;
        int kk = t < 0.6f ? 4 : (t < 3.2f ? 3 : 2);
        if (x == (int)ax1 - 1 && kk > 1) kk--;   // the right end in shade
        P.put(q.first, q.second, R[kk], 3);
      }
    for (int x = (int)ax0; x < (int)ax1; x++) {
      const bool stp = shopfront && ((x / 4) & 1) == 0;
      const Ramp& R = shopfront ? (stp ? SR : kCloth) : A;
      for (int j = 0; j < 2; j++) { auto q = at(x + 0.5f, fyc + 3.0f, az - 5 - j); P.put(q.first, q.second, R[j == 0 ? 2 : 1], 3); }
      // the hem: a scallop every four pixels, its tips dark
      if ((x & 3) == 1 || (x & 3) == 2) { auto q = at(x + 0.5f, fyc + 3.0f, az - 7); P.put(q.first, q.second, R[0], 3); }
    }
    if (rnd)
      for (int s = 0; s < 2; s++) {
        const float x = s == 0 ? ax0 + 1 : ax1 - 2;
        for (int k = 0; k <= 3; k++) { auto q = at(x + 0.5f, fyc + 0.5f + k * 0.75f, az - 9 + k * 0.75f); P.put(q.first, q.second, kIron[s == 0 ? 2 : 1], 3); }
      }
    else
      for (int s = 0; s < 2; s++) {
        float x = s == 0 ? ax0 : ax1 - 1;
        for (int z = (int)frontZ0; z < (int)az - 5; z++) {
          auto q = at(x + 0.5f, fyc + 3.5f, (float)z);
          P.put(q.first, q.second, kWood[s == 0 ? 3 : 1], 3);
        }
      }
    if (shopfront) hanging(rnd ? ax1 : ax1 - 2, rnd ? az - 3 : az - 1, icon, (rnd ? fyc : m.y1) + 0.5f);
  }
  // ---- the ruler's colours (BuildingFacts::banner, banner2, emblem; red and gold where the facts carry none): banners
  //      hanging on the fronts that carry them, flags flying from the tops that carry them
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
  for (const Mass& m : ms) {
    if ((m.feat & bld::VF_BANNERS) && m.wallH >= 22 && !m.open && !m.encl) {
      const int ht = std::clamp(m.wallH * 4 / 10, 10, 18);
      const float zt = (float)m.zBase + std::min(m.wallH - 4, m.storeys >= 2 ? m.wallH - 4 : 33);
      if (m.round) {
        if (m.r >= 10) for (int s = -1; s <= 1; s += 2) { const float x = m.cx + s * m.r * 0.55f - 3; hang(x, m.frontY(x + 3) + 0.5f, zt, 6, ht); }
      } else {
        const int w = (int)(m.x1 - m.x0);
        int n = 0;
        for (int x = (int)m.x0 + 8; x < (int)m.x1 - 12 && n < 6; x += 22) {
          if (std::abs(x + 3 - p.doorX) < 14 && m.hasDoor) continue;
          if (m.face == bld::Face::Gate && std::abs(x + 3 - p.doorX) < 14) continue;
          hang((float)x, m.y1 + 0.5f, zt, 6, ht);
          n++;
        }
        if (!n && w >= 20) hang((float)(m.hasDoor ? (p.doorX + 8 < m.x1 - 7 ? p.doorX + 8 : p.doorX - 14) : (m.x0 + m.x1) * 0.5f - 3), m.y1 + 0.5f, zt, 6, ht);
      }
    }
    if (m.feat & bld::VF_FLAG) {
      Surf s;
      const float fx = m.round ? m.cx : (m.x0 + m.x1) * 0.5f, fy = m.round ? m.cy : (m.y0 + m.y1) * 0.5f;
      float z = massTop(m, fx, fy, s);
      if (z < 0) z = m.zTop();
      flag(fx, fy, z - 1, m.body ? 12 : 9);
    }
  }
  // ---- the barracks: a shield over the door
  if (b == Building::Barracks) {
    auto q = at((float)p.doorX - 3, doorY + 0.5f, frontZ0 + std::min(25, dm.wallH - 3));
    for (int j = 0; j < 7; j++)
      for (int i = 0; i < 6; i++) {
        if (j >= 5 && (i == 0 || i == 5)) continue;
        if (j == 6 && (i == 1 || i == 4)) continue;
        uint32_t c = (i == 0 || j == 0) ? BF[3] : (i == 5 || j == 6 ? BF[1] : BF[2]);
        if (heraldry::chargeAt(em, 5, i, j - 1)) c = BT[heraldry::chargeShade(em, 5, i, j - 1)];
        P.put(q.first + i, q.second + j, c, 3);
      }
  }
  // ---- belfries: an arched opening near the top of the wall with the bell in it (the vents' belfries paint their own)
  for (const Mass& t : ms) {
    if (!(t.feat & bld::VF_BELL) || t.face == bld::Face::Vents || t.round || t.wallH < 30) continue;
    float cx = (t.x0 + t.x1) * 0.5f;
    for (int z = t.wallH - 13; z < t.wallH - 4; z++)
      for (int i = -4; i <= 4; i++) {
        auto q = at(cx + i, t.y1 - 0.5f, (float)(t.zBase + z));
        bool arch = z > t.wallH - 7 && std::abs(i) > 4 - (t.wallH - 4 - z);
        if (arch) continue;
        P.put(q.first, q.second, kInk, 3);
      }
    for (int j = 0; j < 5; j++)
      for (int i = -2; i <= 2; i++) {
        if (j < 1 && std::abs(i) > 1) continue;
        auto q = at(cx + i, t.y1 - 0.5f, (float)t.zBase + t.wallH - 6.0f - j);
        P.put(q.first, q.second, kGold[i < 0 ? 4 : (i > 0 ? 2 : 3)], 3);
      }
  }
  // ---- a minaret's (a spire's) ring balcony near its top: a corbelled gallery with a lit railing
  for (const Mass& t : ms) {
    if (!(t.feat & bld::VF_BALCONY) || !t.round) continue;
    const float zb = (float)(t.zBase + t.wallH - std::max(7, t.wallH / 6));
    const float R = t.r + 2.0f;
    for (float x = t.cx - R + 0.5f; x < t.cx + R; x += 1.0f) {
      const float dx = x - t.cx, fy = t.cy + std::sqrt(std::max(0.0f, R * R - dx * dx)) - 0.5f;
      const float tt = dx / R;
      const int kk = tt < -0.35f ? 4 : (tt < 0.45f ? 3 : 1);
      const Ramp& W = t.wR;
      P.put(at(x, fy, zb).first, at(x, fy, zb).second, W[kk], 3);
      P.put(at(x, fy, zb - 1).first, at(x, fy, zb - 1).second, W[std::max(0, kk - 2)], 3);
      P.put(at(x, fy, zb - 2).first, at(x, fy, zb - 2).second, darken(W[std::max(0, kk - 2)], 0.3f), 3);
      if (((int)std::floor(x)) % 2 == 0) for (int z = 1; z <= 3; z++) P.put(at(x, fy, zb + z).first, at(x, fy, zb + z).second, W[std::min(4, kk)], 3);
      P.put(at(x, fy, zb + 4).first, at(x, fy, zb + 4).second, W[std::min(4, kk + 1)], 3);
    }
  }
  // ---- steam rising from the baths' vents
  for (const Mass& m : ms) {
    if (!(m.feat & bld::VF_STEAM)) continue;
    const float sx0 = m.round ? m.cx - m.r * 0.5f : m.x0 + (m.x1 - m.x0) * 0.25f, sx1 = m.round ? m.cx + m.r * 0.5f : m.x1 - (m.x1 - m.x0) * 0.25f;
    for (int k = 0; k < 2; k++) {
      const float vx = k == 0 ? sx0 : sx1, vy = m.round ? m.cy : (m.y0 + m.y1) * 0.5f;
      Surf s;
      const float z = std::max(m.zTop(), massTop(m, vx, vy, s));
      auto q = at(vx, vy, z);
      for (int i = -1; i <= 1; i++) { P.put(q.first + i, q.second, kStone[i < 0 ? 3 : 1], 3); P.put(q.first + i, q.second - 1, kStone[i < 0 ? 4 : 2], 3); }
      for (int puff = 0; puff < 3; puff++) {
        const int py = q.second - 4 - puff * 4, px = q.first + puff * 2 + (int)(hash3(k, puff, p.seed) % 2);
        const int r = 2 + puff / 2;
        for (int j = -r; j <= r; j++)
          for (int i = -r; i <= r; i++) {
            if (i * i + j * j > r * r) continue;
            if (py + j < p.sc.top - p.budget + 2) continue;
            P.put(px + i, py + j, (i + j < 0) ? rgba(248, 248, 252, 170) : rgba(214, 220, 232, 150), 3);
          }
      }
    }
  }
  // ---- a tent's guy ropes and pegs
  for (const Mass& m : ms) {
    if (!(m.feat & bld::VF_ROPES)) continue;
    const float ez = (float)m.zTop() - 1;
    auto rope = [&](float x0, float y0, float x1, float y1) {
      auto a = at(x0, y0, ez), c = at(x1, y1, 0.0f);
      const int n = std::max(std::abs(c.first - a.first), std::abs(c.second - a.second));
      for (int i = 0; i <= n; i++) {
        const float t = n ? (float)i / n : 0;
        P.put((int)std::lround(a.first + (c.first - a.first) * t), (int)std::lround(a.second + (c.second - a.second) * t), mix(kCloth[1], kWoodDark[2], 0.4f), 3);
      }
      P.put(c.first, c.second, kWood[3], 3); P.put(c.first, c.second + 1, kWood[1], 3);
    };
    if (m.round) {
      for (int k = 0; k < 4; k++) {
        const float a = PI * (0.15f + 0.233f * k);
        const float R = m.r + m.ov;
        rope(m.cx - std::cos(a) * R, m.cy + std::sin(a) * R, m.cx - std::cos(a) * (R + 5), m.cy + std::sin(a) * (R + 3));
      }
    } else {
      rope(m.x0 - m.ov, m.y1 + m.ovF, m.x0 - m.ov - 4, std::min((float)D + 3, m.y1 + m.ovF + 3));
      rope(m.x1 + m.ov, m.y1 + m.ovF, m.x1 + m.ov + 3, std::min((float)D + 3, m.y1 + m.ovF + 3));
      rope(m.x0 - m.ov, m.y0, m.x0 - m.ov - 4, m.y0 + 4);
      rope(m.x1 + m.ov, m.y0, m.x1 + m.ov + 3, m.y0 + 4);
    }
  }
  // ---- horse-tail standards and pennants along a court's fence and at its gate
  for (const Mass& m : ms) {
    if (!(m.feat & bld::VF_PENNANTS) || m.x1 - m.x0 < m.y1 - m.y0) continue;
    static const uint32_t pc[3] = {rgba(206, 52, 46), rgba(240, 200, 60), rgba(56, 96, 196)};
    const bool gate = m.role == bld::VolRole::Gate;
    for (float x = m.x0 + 2; x < m.x1 - 1; x += gate ? (m.x1 - m.x0 - 4) : 16) {
      auto base = at(x, m.y1 - 0.5f, (float)m.zBase);
      const int ph = gate ? m.wallH + 12 : m.wallH + 8;
      for (int k = 0; k < ph; k++) P.put(base.first, base.second - k, kWood[k == ph - 1 ? 4 : 2], 3);
      for (int j = 0; j < 6; j++) for (int i = -1; i <= 1; i++) if (j > 1 || i == 0) P.put(base.first + i, base.second - ph + 2 + j, (i + j) & 1 ? kInk : rgba(64, 48, 44), 3);
      const uint32_t c = pc[(int)(x / 16) % 3];
      for (int j = 0; j < 3; j++) for (int i = 1; i <= 5 - j; i++) P.put(base.first + i, base.second - ph + 1 + j, j == 0 ? c : darken(c, 0.2f), 3);
    }
  }
  // ---- carved bargeboards on the street gables of fjord and marsh halls; carved horns on their ridge ends
  for (const Mass& m : ms) {
    if (!(m.feat & bld::VF_CARVED) || m.round || flatShape(m.shape)) continue;
    const Ridge rg = ridgeOf(m);
    if (!rg.ok) continue;
    if (m.alongY) {
      const auto top = at(rg.xb, m.y1 + m.ovF - 0.6f, rg.z);
      for (int k = 0; k < 5; k++) {
        P.put(top.first - 2 + k, top.second - k, kWood[k < 3 ? 3 : 4], 3);
        P.put(top.first + 2 - k, top.second - k, kWood[k < 3 ? 2 : 1], 3);
      }
    } else {
      for (int e = 0; e < 2; e++) {
        const auto q = at(e ? rg.xb : rg.xa, rg.ya, rg.z);
        const int dir = e ? 1 : -1;
        for (int k = 0; k < 4; k++) P.put(q.first + dir * (k / 2), q.second - 1 - k, kWood[k < 2 ? 3 : 4], 3);
        P.put(q.first + dir * 2, q.second - 4, kWood[4], 3);
      }
    }
  }
  if (b == Building::Windmill) windmillDress(P, at);
  if (p.cul != CU_NONE || std::any_of(ms.begin(), ms.end(), [](const Mass& m) { return m.tree; })) cultureOverlays(P, at);
  else {   // the classic look: finials on the mage tower and spires
    for (const Mass& m : ms) {
      if (!(m.round || m.shape == RoofShape::Spire) || m.chimney || m.found || (m.feat & bld::VF_FLAG)) continue;
      if (!(m.body && b == Building::Tower) && m.shape != RoofShape::Spire) continue;
      Surf s;
      const float cx = m.round ? m.cx : (m.x0 + m.x1) * 0.5f, cy = m.round ? m.cy : (m.y0 + m.y1) * 0.5f;
      auto q = at(cx, cy, massTop(m, cx, cy, s));
      for (int k = 1; k < 6; k++) P.put(q.first, q.second - k, kGold[k == 3 || k == 5 ? 4 : 2], 3);
      P.put(q.first - 1, q.second - 4, kGold[3], 3); P.put(q.first + 1, q.second - 4, kGold[1], 3);
    }
  }
  // chimney mouths that smoke
  if (info) {
    info->doors = p.doorsPainted;
    info->pillars.clear();
    for (const Mass& m : ms)
      if (m.open || m.face == bld::Face::Iwan)
        for (const bld::Pillar& q : m.pillars) info->pillars.push_back({q.x0, q.x1, (int)std::lround(m.round ? m.cy + m.r : m.y1)});
    info->smokeN = 0;
    for (const Mass& m : ms) {
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

// ---------------------------------------------------------------- M4 Banners: the war's mark (Bldg::charred)
// Painted over the finished picture of the blueprint (before the outline), reading only what the painter laid down:
// the layer map (wall, roof, overlay) and the panes. So it works for every form the builder makes, and charred 0 never
// gets here (every key and pixel as before).
//   1 scorched:   soot plumes rising from every window over the wall and the eave above it, singed patches on the roof
//   2 burned out: walls blackened to charcoal and ash-grey (the light still reads on them), the windows gaping black with
//                 an ember here and there (no glass: nothing is lit at night), the roof holed to the rafters: charred
//                 rafters and purlins over the dark inside, the far rim of each hole showing the roof's cut edge in the
//                 light, the near rim scorched; what is left of the roof blackened
//   3 rebuilding: a scorched shell with its holes boarded in fresh pale planks, and timber scaffolding up its front
//                 (poles, ledgers at the storey lines, a brace, a plank deck), lit on the left like everything else
uint32_t burnt(uint32_t col, float amt) {
  const float l = luma(col);
  const uint32_t target = mix(rgba(30, 24, 30), rgba(126, 112, 104), std::clamp(l * 1.15f, 0.0f, 1.0f));
  return mix(col, target, std::clamp(amt, 0.0f, 1.0f));
}
void warDamage(Painter& P, int level, uint32_t seed) {
  Canvas& c = P.c;
  const int W = c.w, H = c.h;
  auto lay = [&](int x, int y) -> int { return (x < 0 || y < 0 || x >= W || y >= H) ? 0 : P.layer[(size_t)y * W + x]; };
  // soot: plumes from the panes, rising and widening a little, fading as they climb (over walls and on onto the roof)
  std::vector<float> S((size_t)W * H, 0.0f);
  for (int y = H - 1; y >= 0; y--)
    for (int x = 0; x < W; x++) {
      const size_t i = (size_t)y * W + x;
      if (!lay(x, y)) continue;
      float s = P.glass[i] ? 1.0f : 0.0f;
      if (y + 1 < H) {
        const float b0 = S[i + (size_t)W], bl = x > 0 ? S[i + (size_t)W - 1] : 0, br = x + 1 < W ? S[i + (size_t)W + 1] : 0;
        const float j = 0.91f + 0.06f * vnoise(x * 0.5f, y * 0.25f, seed + 41);
        s = std::max(s, std::max(b0 * j, std::max(bl, br) * (j - 0.12f)));
      }
      S[i] = s;
    }
  const float sootK = level == 2 ? 0.85f : (level == 3 ? 0.5f : 0.8f);
  // where the roofs are: a roof pixel's depth inside the roof (chamfer distance to anything that is not roof), and the
  // bottom of the walls in its column (the plinth and steps below a building are no roof)
  std::vector<int> wallBot((size_t)W, -1);
  for (int x = 0; x < W; x++) for (int y = 0; y < H; y++) if (lay(x, y) == 1) wallBot[(size_t)x] = y;
  std::vector<uint8_t> depth((size_t)W * H, 0);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (lay(x, y) != 2 || wallBot[(size_t)x] < 0 || y > wallBot[(size_t)x] - 3) continue;
      int d = 1;
      if (y > 0 && x > 0) d = 1 + std::min(depth[(size_t)(y - 1) * W + x], depth[(size_t)y * W + x - 1]);
      depth[(size_t)y * W + x] = (uint8_t)std::min(d, 60);
    }
  for (int y = H - 1; y >= 0; y--)
    for (int x = W - 1; x >= 0; x--) {
      const size_t i = (size_t)y * W + x;
      if (!depth[i]) continue;
      int d = depth[i];
      if (y + 1 < H) d = std::min(d, depth[i + (size_t)W] + 1);
      if (x + 1 < W) d = std::min(d, depth[i + 1] + 1);
      depth[i] = (uint8_t)d;
    }
  // the holes (2) or the patches boarded over (3): broad noise biased into the roof's middle (eaves, ridges and gable
  // edges stay, so the building keeps its shape); the boarding (3) on a grid of planks, so the patches are square-cut
  auto holeAt = [&](int x, int y) {
    if (level == 1) return false;
    const int d = depth[(size_t)y * W + x];
    if (d < 3) return false;
    const int qx = level == 3 ? (x / 5) * 5 : x, qy = level == 3 ? (y / 4) * 4 : y;
    const float n = vnoise(qx * 0.075f, qy * 0.10f, seed + 3) * 0.7f + vnoise(qx * 0.21f, qy * 0.21f, seed + 9) * 0.3f;
    const float bias = std::min(1.0f, (d - 3) / 7.0f) * 0.14f;
    return n + bias > (level == 2 ? 0.58f : 0.66f);
  };
  std::vector<uint8_t> hole((size_t)W * H, 0);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) hole[(size_t)y * W + x] = holeAt(x, y) ? 1 : 0;
  auto isHole = [&](int x, int y) { return x >= 0 && y >= 0 && x < W && y < H && hole[(size_t)y * W + x]; };
  Canvas src = c;
  int windowsLit = 0;
  // soot and char in three crisp steps with an ordered dither between them (pixel art, not a smooth blur)
  auto qz = [](float v, int x, int y) {
    const float s = std::clamp(v, 0.0f, 1.0f) * 3.0f + (bayer(x, y) - 0.5f) * 0.45f;
    return std::clamp(std::floor(s), 0.0f, 3.0f) / 3.0f;
  };
  if (level == 1) {
    // (fixer M4 r1, review: "a flat grey checkerboard smudge") scorched, drawn along the building's own shapes: a soot
    // streak climbing from each window (narrow, darkest at the lintel, tapering and fading as it climbs, two solid tones
    // with dithering only where they meet), smoke-stained panes, the eave above a sooty window charred along its edge,
    // the roof's lower edge singed in broken runs, and a few roof tiles broken through. No blotch crosses an edge.
    std::vector<float> S1((size_t)W * H, 0.0f);
    for (int y = H - 1; y >= 0; y--)
      for (int x = 0; x < W; x++) {
        const size_t i = (size_t)y * W + x;
        const int L = lay(x, y);
        if (!L) continue;   // (the windows' frames and shutters sit on the overlay layer: the streak starts there too)
        float s = P.glass[i] ? 1.0f : 0.0f;
        if (y + 1 < H && lay(x, y + 1)) {
          const float b0 = S1[i + (size_t)W];
          const float bl = x > 0 ? S1[i + (size_t)W - 1] : 0, br = x + 1 < W ? S1[i + (size_t)W + 1] : 0;
          const float j = (L == 2 ? 0.62f : 0.90f) + 0.05f * vnoise(x * 0.6f, y * 0.3f, seed + 41);
          s = std::max(s, std::max(b0 * j, std::max(bl, br) * (j - 0.22f)));
        }
        S1[i] = s;
      }
    // one scorch on the roof (two on a wide one): an irregular burnt patch well inside the roof, a dark core with a
    // ring of singed tiles round it (never across the roof's edges: only pixels 2+ deep in the roof take it)
    struct Scorch { int x, y; float r; };
    std::vector<Scorch> scorch;
    {
      std::vector<int> cand;
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) if (depth[(size_t)y * W + x] >= 7) cand.push_back(y * W + x);
      const int n = cand.empty() ? 0 : (W > 70 ? 2 : 1);
      for (int k = 0; k < n; k++) {
        const int pick = cand[(size_t)(hash3(k, 7, seed + 67) % (uint32_t)cand.size())];
        const int d = depth[(size_t)pick];
        scorch.push_back({pick % W, pick / W, std::min(9.0f, 3.0f + d * 0.6f)});
      }
    }
    auto q2 = [](float v, int x, int y) {   // 0, 1/2, 1: solid tones, a little dither on the boundary only
      const float s = std::clamp(v, 0.0f, 1.0f) * 2.0f + (bayer(x, y) - 0.5f) * 0.3f;
      return std::clamp(std::floor(s), 0.0f, 2.0f) / 2.0f;
    };
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        const size_t i = (size_t)y * W + x;
        const int L = lay(x, y);
        if (!L || L == 3) continue;
        uint32_t col = src.get(x, y);
        if (L == 1) {
          if (P.glass[i]) col = darken(col, 0.4f);
          else col = darken(col, 0.5f * q2(S1[i] * 1.15f - 0.1f, x, y));
        } else {
          // the eave: the roof's lowest rows above a wall
          int eave = 0;
          for (int k = 1; k <= 3; k++) if (lay(x, y + k) == 1) { eave = k; break; }
          col = darken(col, 0.5f * q2(S1[i] * 1.2f - 0.08f, x, y));
          if (eave) {
            const float run = vnoise(x * 0.16f, 3.0f, seed + 57);   // broken runs along the edge (1-D: follows the line)
            if (run > 0.56f && eave <= (run > 0.68f ? 2 : 1)) col = burnt(col, eave == 1 ? 0.8f : 0.55f);
          }
          if (depth[i] >= 2)
            for (const Scorch& sc : scorch) {
              const float dx = (x - sc.x) / sc.r, dy = (y - sc.y) / (sc.r * 0.75f);
              const float dd = std::sqrt(dx * dx + dy * dy) + 0.45f * (vnoise(x * 0.35f, y * 0.35f, seed + 71) - 0.5f);
              if (dd < 0.45f) col = mix(burnt(col, 1.0f), kInk, 0.45f);
              else if (dd < 0.75f) col = mix(burnt(col, 0.8f), kInk, 0.15f);
              else if (dd < 1.0f && bayer(x, y) < 0.5f) col = burnt(col, 0.3f);
              // the core's upper rim catches the light (a shallow dish of fallen tiles), a cinder in it now and then
              if (dd < 0.45f && hash3(x, y, seed + 73) % 19 == 0) col = rgba(150, 64, 36);
            }
        }
        c.set(x, y, col);
      }
    return;
  }
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const size_t i = (size_t)y * W + x;
      const int L = lay(x, y);
      if (!L) {
        // (fixer M4 r2) paint laid outside the layer map (a door leaf, its frame): a burned-out shell's coloured woodwork
        // chars with the rest; the plinth's stone and the shadows (unsaturated) are left alone
        if (level != 2) continue;
        const uint32_t q = src.get(x, y);
        if ((q >> 24) < 200) continue;
        const int cr = (int)(q & 255), cg = (int)((q >> 8) & 255), cb = (int)((q >> 16) & 255);
        if (std::max({cr, cg, cb}) - std::min({cr, cg, cb}) <= 46 || (cr > cg && cg > cb)) continue;
        c.set(x, y, burnt(q, qz(0.75f + 0.2f * vnoise(x * 0.22f, y * 0.22f, seed + 17), x, y)));
        continue;
      }
      uint32_t col = src.get(x, y);
      const float n = vnoise(x * 0.22f, y * 0.22f, seed + 17);
      if (L == 3) {
        // (fixer M4 r2, review: "a burned shell keeps a pristine teal door canopy") the overlay's awnings, pergolas,
        // door frames and shutters burn with the house: charred to the same ash and charcoal (the light still reads),
        // soot from the windows over them; a rebuilding shell's are only smoke-stained
        if (c.get(x, y) >> 24 == 0) continue;
        if (level == 2) col = burnt(col, qz(0.62f + 0.25f * n + 0.15f * S[i], x, y));
        else col = burnt(col, qz(0.22f + 0.15f * n, x, y));
        col = darken(col, sootK * 0.6f * qz(S[i], x, y));
        c.set(x, y, col);
        continue;
      }
      if (L == 1) {   // the walls
        if (P.glass[i] && level == 2) {
          // a gaping window: the black of a gutted room; one window in a few still holds an ember low down
          col = mix(kInk, rgba(52, 28, 30), 0.3f);
          const bool sill = lay(x, y + 1) == 1 && !P.glass[i + (size_t)W];
          if (sill && hash3(x / 6, y / 6, seed) % 6 == 0 && hash3(x, y, seed) % 3 == 0 && windowsLit < 3) { col = kFire[1]; windowsLit++; }
          P.glass[i] = 0;
        } else if (P.glass[i]) {
          col = darken(col, level == 3 ? 0.3f : 0.45f);   // smoke-stained panes
        } else {
          // the fire's heat blackens the top of a wall most, under the eaves
          const int wb = wallBot[(size_t)x];
          const float up = wb >= 0 ? std::clamp((wb - y) / 30.0f, 0.0f, 1.0f) : 0.0f;
          if (level == 2) {
            // (fixer M4 r2) painted wood (a teal door, coloured shutters) does not keep its colour through a fire
            const int cr = (int)(col & 255), cg = (int)((col >> 8) & 255), cb = (int)((col >> 16) & 255);
            const int sat = std::max({cr, cg, cb}) - std::min({cr, cg, cb});
            const float paint = sat > 46 && !(cr > cg && cg > cb) ? 0.35f : 0.0f;   // (warm ochre plaster and brick keep their hue)
            col = burnt(col, qz(0.40f + paint + 0.25f * n + 0.25f * up + 0.2f * S[i], x, y));
          }
          if (level == 3) col = burnt(col, qz(0.10f + 0.12f * n + 0.08f * up, x, y));
          col = darken(col, sootK * qz(S[i] * (0.7f + 0.3f * n), x, y));
        }
      } else {   // the roofs
        if (isHole(x, y)) {
          if (level == 3) {   // boarded over: fresh planks in rows, a nail now and then, the old edge dark round them
            const int row = (y + (int)(seed & 3)) % 4;
            const uint32_t plank = mix(kWood[3], kStoneWarm[3], 0.35f);
            col = row == 0 ? kWood[1] : (row == 3 ? darken(plank, 0.15f) : plank);
            if (((x + y * 3) % 11) == 0 && row == 1) col = kIron[2];
            if (((x * 5 + y) % 7) == 0 && row == 2) col = mix(plank, kWood[2], 0.5f);   // the grain
            if (!isHole(x, y - 1) || !isHole(x - 1, y)) col = kWood[1];
          } else {
            // depth under the far rim: the first rows lie in the roof's shadow; further down the dark of the gutted room
            int d = 0;
            while (d < 8 && isHole(x, y - d - 1)) d++;
            col = d < 3 ? rgba(16, 12, 18) : mix(rgba(22, 16, 22), rgba(58, 40, 36), std::min(1.0f, (d - 3) / 10.0f));
            // charred rafters running down the slope, two pixels wide, lit on their left; a purlin across them here
            // and there; some burned through and fallen
            const int rx = (x + (int)(seed % 7)) % 7;
            const bool purlin = ((y + (int)(seed % 9)) % 9) == 0 && hash3(x / 4, y, seed) % 3 != 0;
            const bool gone = hash3(x / 7, (y + 3) / 9, seed + 5) % 5 == 0;
            if (!gone) {
              if (rx == 0) col = rgba(96, 70, 56);
              else if (rx == 1) col = rgba(54, 38, 36);
              else if (purlin) col = rgba(72, 52, 44);
            }
            if (rx == 1 && !gone && hash3(x, y, seed + 11) % 31 == 0) col = kFire[1];   // an ember in the char
            // the far rim: the roof's cut edge facing the viewer, in the light
            if (!isHole(x, y - 1)) col = lighten(burnt(src.get(x, std::max(0, y - 1)), 0.45f), 0.1f);
          }
        } else {
          // the roof that is left: blackened (2) round its holes, singed patches, soot from the walls below
          float near = 0;
          if (level == 2)
            for (int k = 1; k <= 4; k++)
              if (isHole(x, y - k) || isHole(x - k, y) || isHole(x + k, y) || isHole(x, y + k)) { near = std::max(near, 1.0f - (k - 1) * 0.25f); }
          if (level == 2) col = burnt(col, qz(0.50f + 0.25f * n + 0.25f * near, x, y));
          if (level == 2 && isHole(x, y - 1)) col = mix(kInk, rgba(60, 34, 30), 0.35f);   // the near rim: charred
          const float patch = vnoise(x * 0.13f, y * 0.17f, seed + 23);
          if (patch > 0.58f) {
            const float k = std::min(1.0f, (patch - 0.58f) / 0.25f);
            col = mix(col, burnt(col, 0.9f), (level == 3 ? 0.45f : 0.75f) * qz(k, x, y));
          }
          col = darken(col, sootK * 0.7f * qz(S[i], x, y));
        }
      }
      c.set(x, y, col);
    }
  if (level != 3) return;
  // scaffolding up the front walls: poles at the ends and the middle, ledgers at the storey lines, a brace, a deck
  std::vector<int> top((size_t)W, -1), bot((size_t)W, -1);
  int xmin = W, xmax = -1;
  for (int x = 0; x < W; x++)
    for (int y = 0; y < H; y++)
      if (lay(x, y) == 1) { if (top[(size_t)x] < 0) top[(size_t)x] = y; bot[(size_t)x] = y; xmin = std::min(xmin, x); xmax = std::max(xmax, x); }
  if (xmax - xmin < 10) return;
  std::vector<int> poles = {xmin + 1, xmax - 2};
  if (xmax - xmin > 34) poles.insert(poles.begin() + 1, (xmin + xmax) / 2);
  auto putS = [&](int x, int y, uint32_t col) { if (x >= 0 && x < W && y >= 0 && y < H) P.put(x, y, col, 3); };
  for (int px : poles) {
    if (top[(size_t)px] < 0) continue;
    for (int y = top[(size_t)px] - 3; y <= bot[(size_t)px] + 1; y++) { putS(px, y, kWood[3]); putS(px + 1, y, kWood[1]); }
    putS(px, top[(size_t)px] - 4, kWood[4]);
  }
  for (int lv = 1; lv <= 2; lv++) {
    for (int x = poles.front() - 1; x <= poles.back() + 2; x++) {
      if (x < 0 || x >= W || bot[(size_t)x] < 0) continue;
      const int y = bot[(size_t)x] - lv * 11;
      if (y < top[(size_t)x] + 1) continue;
      putS(x, y, kWood[4]); putS(x, y + 1, kWood[2]);
      if (lv == 1) putS(x, y + 2, kWood[1]);   // the plank deck's edge
    }
  }
  {   // a diagonal brace between the first two poles, a lashing at each joint
    const int a = poles[0], b = poles[1];
    if (bot[(size_t)a] >= 0 && bot[(size_t)b] >= 0)
      for (int x = a + 1; x < b; x++) {
        const float t = (float)(x - a) / std::max(1, b - a);
        const int y = (int)std::lround(bot[(size_t)a] - 1 - t * 10);
        if (y > top[(size_t)x]) putS(x, y, kWood[2]);
      }
    for (int px : poles)
      if (bot[(size_t)px] >= 0) for (int lv = 1; lv <= 2; lv++) { const int y = bot[(size_t)px] - lv * 11 - 1; if (y > top[(size_t)px]) putS(px, y, kCloth[1]); }
  }
}

}  // namespace

// ---------------------------------------------------------------- incremental paints (M3, owner carry-over 1)
// A building paint in resumable phases: the plan (masses), the facades (a mass at a time), the roofs and walls (a few
// columns at a time), the eave shadow and the outline (a band of rows at a time), the overlays (signs, banners,
// machinery) and the facts (glass, storeys, chimneys). buildingSprite is this job run in one go, so the pixels are the
// same whichever budget the steps are given.
struct BuildingJob {
  bld::Blueprint bp;        // M3b: the blueprint painted
  int phase = 0, cursor = 0;
  std::unique_ptr<Plan> plan;
  std::unique_ptr<Painter> P;
  Canvas src;          // the picture before the outline (outlineRows reads it)
  BuildingInfo info;
  bool done = false;
  double worstStepMs = 0;   // the longest single unit of work so far (tests)
  int worstPhase = -1;
};

// M3b: a job paints a blueprint (the builder's, rpg/build/blueprint.h): its volumes become the masses
std::shared_ptr<BuildingJob> beginBuilding(const bld::Blueprint& bp) {
  auto j = std::make_shared<BuildingJob>();
  j->bp = bp;
  return j;
}

namespace {
// one unit of work of the job (the phases above); false once everything is done
bool stepUnit(BuildingJob& j) {
  switch (j.phase) {
    case 0:   // the masses
      j.plan = std::make_unique<Plan>(planFromBlueprint(j.bp));
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
      const int n = 2;   // (M3b: two columns a step: a palace or a tent court has twenty volumes)
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
      if (j.bp.req.facts.charred) warDamage(*j.P, std::clamp(j.bp.req.facts.charred, 1, 3), j.bp.req.seed ^ 0xC4A2u);   // (M4)
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
        std::printf("buildingSprite: type %d %dx%d storeys %d: %d px cut at the rise budget\n", (int)j.bp.req.purpose, j.bp.req.wTiles, j.bp.req.hTiles, p.facts.storeys, P.clipped);
      BuildingInfo& info = j.info;
      info.planKey = j.bp.key;
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

Canvas buildingSprite(const bld::Blueprint& bp, BuildingInfo* info) {
  BuildingJob j;
  j.bp = bp;
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

int buildingHeight(const bld::Blueprint& bp) {
  Plan p = makePlan(bp);
  return p.sc.top;
}

}  // namespace art
