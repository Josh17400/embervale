// EMBERVALE art, M10 "Beyond the Mist" (rpg/art/art_sea.h): the sea props and the ships. ART lane.
//
// The SHIPS are modelled, not drawn: each hull is a small parametric solid (its half-beam and sheer along its length,
// flare, a transom or a double end, castles, posts) with its masts, yards, sails, rigging, oars and ornament, and a
// tiny oblique splat renderer puts it on the canvas in the game's high 3/4 view ((x, y, z) -> (x, y - z)) at any
// heading, with a depth buffer, the game's top-left sun, ordered dithering between ramp steps, a selective outline
// and the ship's shadow cast down-right onto the water. Every culture has its own hull form, sail cut, colours and
// figurehead (the archetype table below); the view fills ShipStyle from the yard's culture.
#include <algorithm>
#include <cmath>
#include <vector>
#include "rpg/art/art_internal.h"
#include "rpg/art/art_sea.h"

namespace art {

namespace {
struct Sz { int w, h, frames; };
Sz seaInfo(Prop p) {
  switch (p) {
    case Prop::MooringPost: return {14, 22, 1};
    case Prop::NetRack: return {32, 30, 1};
    case Prop::FishBaskets: return {22, 18, 1};
    case Prop::Buoy: return {16, 20, 4};
    case Prop::Anchor: return {22, 24, 1};
    case Prop::CargoPile: return {30, 28, 1};
    case Prop::Slipway: return {52, 46, 1};
    case Prop::HarbourDesk: return {34, 42, 1};
    case Prop::Lighthouse: return {40, 92, 1};
    case Prop::Wreck: return {58, 46, 1};
    default: return {16, 16, 1};
  }
}
const Ramp kRope = ramp5(rgba(70, 52, 40), rgba(112, 86, 58), rgba(152, 122, 80), rgba(188, 160, 110), rgba(220, 198, 150));
const Ramp kPaintRed = ramp5(rgba(70, 20, 30), rgba(120, 34, 36), rgba(170, 52, 44), rgba(206, 88, 64), rgba(236, 140, 104));
const Ramp kCanvasR = ramp5(rgba(120, 112, 104), rgba(168, 160, 142), rgba(206, 198, 176), rgba(232, 224, 202), rgba(250, 244, 226));
}  // namespace

int seaPropW(Prop p) { return seaInfo(p).w; }
int seaPropH(Prop p) { return seaInfo(p).h; }
int seaPropFrames(Prop p) { return seaInfo(p).frames; }

// paintSeaProp: at the end of the file (the props are modelled with the ships' renderer)

// =====================================================================================================================
// SHIPS
// =====================================================================================================================
namespace {

struct V3 { float x, y, z; };
inline V3 v3(float x, float y, float z) { return V3{x, y, z}; }
inline V3 operator+(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline V3 operator-(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline V3 operator*(V3 a, float k) { return v3(a.x * k, a.y * k, a.z * k); }
inline float dot3(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross3(V3 a, V3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
inline float len3(V3 a) { return std::sqrt(dot3(a, a)); }
inline V3 unit3(V3 a) { const float l = len3(a); return l > 1e-6f ? a * (1.0f / l) : v3(0, 0, 1); }
inline V3 lerp3(V3 a, V3 b, float t) { return a + (b - a) * t; }
inline V3 bez3(V3 a, V3 b, V3 c, float t) { return lerp3(lerp3(a, b, t), lerp3(b, c, t), t); }

// the game's sun (top-left, high) in the ship's world (X east, Y south, Z up) and the way to the viewer
const V3 kSun = unit3(v3(-0.52f, 0.20f, 0.83f));
const V3 kToEye = unit3(v3(0.0f, 1.0f, 1.0f));
inline float sunI(V3 n) { return 0.5f + 0.5f * dot3(n, kSun); }

// ---- the oblique splat renderer: a depth-tested canvas that remembers, per pixel, how far south its point stands
//      (the hero's layer split) and whether it lies at deck level; the shadow it casts on the water; the foam at its
//      waterline
struct SC {
  int w, h;
  float ox, oy;
  float sink = 0;          // the bob: the ship sits this much deeper
  float heel = 0;          // lateral lean per px of height (toward heelDir)
  V3 heelDir = v3(0, 1, 0);
  float pitch = 0;         // the hull raised along pitchDir per px (a wreck: the bow up on the reef, the stern sunk)
  V3 pitchDir = v3(1, 0, 0);
  std::vector<uint32_t> col;
  std::vector<float> zb, wy;
  std::vector<uint8_t> deck, sh, foam;
  SC(int w_, int h_, float ox_, float oy_)
      : w(w_), h(h_), ox(ox_), oy(oy_), col((size_t)w_ * h_, 0), zb((size_t)w_ * h_, -1e9f), wy((size_t)w_ * h_, 0),
        deck((size_t)w_ * h_, 0), sh((size_t)w_ * h_, 0), foam((size_t)w_ * h_, 0) {}
  // where a world point lands (after the bob and the heel); false under the water or off the canvas
  bool at(V3 p, V3& q, int& sx, int& sy) const {
    p.z -= sink;
    if (pitch != 0) p.z += (p.x * pitchDir.x + p.y * pitchDir.y) * pitch;
    if (p.z < -0.05f) return false;
    q = p + heelDir * (std::max(0.0f, p.z) * heel);
    sx = (int)std::floor(ox + q.x);
    sy = (int)std::floor(oy + q.y - q.z);
    return sx >= 0 && sy >= 0 && sx < w && sy < h;
  }
  void cast(V3 q) {
    const int x = (int)std::floor(ox + q.x + q.z * 0.40f), y = (int)std::floor(oy + q.y + q.z * 0.26f);
    if (x >= 0 && y >= 0 && x < w && y < h) sh[(size_t)y * w + x] = 1;
  }
  void putC(V3 p, uint32_t c, uint8_t dk = 0, bool shadowIt = true) {
    V3 q;
    int sx, sy;
    if (!at(p, q, sx, sy)) return;
    if (shadowIt) cast(q);
    const size_t i = (size_t)sy * w + sx;
    const float d = q.y + q.z;
    if (d < zb[i]) return;
    zb[i] = d; col[i] = c; wy[i] = q.y; deck[i] = dk;
  }
  // a ramp colour for light level I (0..1), dithered on the pixel it lands on
  void putL(V3 p, const Ramp& R, float I, float bias = 0, uint8_t dk = 0, bool shadowIt = true) {
    V3 q;
    int sx, sy;
    if (!at(p, q, sx, sy)) return;
    if (shadowIt) cast(q);
    const size_t i = (size_t)sy * w + sx;
    const float d = q.y + q.z;
    if (d < zb[i]) return;
    const float f = I * 4.2f - 0.45f + bias + (bayer(sx, sy) - 0.5f) * 0.42f;
    zb[i] = d; col[i] = R[(int)std::floor(f + 0.5f)]; wy[i] = q.y; deck[i] = dk;
  }
  void putFoam(V3 p) {
    const int x = (int)std::floor(ox + p.x), y = (int)std::floor(oy + p.y);
    if (x >= 0 && y >= 0 && x < w && y < h) foam[(size_t)y * w + x] = 1;
  }
};

// a round spar between two points (masts, yards, booms, oars, posts), lit as a cylinder
void rod(SC& s, V3 a, V3 b, float r, const Ramp& R, float bias = 0) {
  const V3 ax = b - a;
  const float L = len3(ax);
  if (L < 0.01f) return;
  const V3 t = ax * (1.0f / L);
  V3 u = std::fabs(t.z) < 0.9f ? unit3(cross3(t, v3(0, 0, 1))) : unit3(cross3(t, v3(1, 0, 0)));
  const V3 w = cross3(t, u);
  const int nl = std::max(2, (int)(L / 0.4f)), na = std::max(6, (int)(r * 14));
  for (int i = 0; i <= nl; i++)
    for (int k = 0; k < na; k++) {
      const float th = k * 6.2831853f / na;
      const V3 n = u * std::cos(th) + w * std::sin(th);
      s.putL(a + t * (L * i / nl) + n * r, R, sunI(n), bias);
    }
  if (r < 0.75f)   // a thin spar: its axis too, so it never breaks up into dots
    for (int i = 0; i <= nl; i++) s.putL(a + t * (L * i / nl), R, sunI(unit3(v3(-0.3f, 0.3f, 0.9f))), bias);
}
// a thin line of rope (rigging): dark, never lit
void rope(SC& s, V3 a, V3 b, uint32_t c) {
  const float L = len3(b - a);
  const int n = std::max(2, (int)(L / 0.35f));
  for (int i = 0; i <= n; i++) s.putC(lerp3(a, b, (float)i / n), c, 0, false);
}
void ball3(SC& s, V3 c, float r, const Ramp& R, float bias = 0) {
  const int n = std::max(8, (int)(r * 10));
  for (int i = 0; i <= n; i++)
    for (int k = 0; k < n * 2; k++) {
      const float ph = 3.14159265f * i / n, th = 3.14159265f * k / n;
      const V3 nn = v3(std::sin(ph) * std::cos(th), std::sin(ph) * std::sin(th), std::cos(ph));
      s.putL(c + nn * r, R, sunI(nn), bias);
    }
}

// ---- the archetype table: hull forms, rigs, colours, ornament
// culture: cult::Archetype + 1 (0 classic) -> 1 Fjordfolk, 2 Highland, 3 Heartland, 4 Imperial, 5 Dune, 6 Steppe,
// 7 Marsh, 8 Jade, 9 River, 10 SunTemple, 11 Sylvan, 12 Starspire
enum Cul : int { C_CLASSIC, C_FJORD, C_HIGHLAND, C_HEART, C_IMPERIAL, C_DUNE, C_STEPPE, C_MARSH, C_JADE, C_RIVER, C_SUN, C_SYLVAN, C_STAR };

struct Form {
  float L = 12, B = 4, F = 3;      // half length, half beam, freeboard amidships (px)
  float sheerBow = 1, sheerStern = 1;
  float bowP = 1.5f, sternP = 1.7f;  // waterline fullness at the ends (1.2 fine .. 3 bluff)
  float transom = 0;               // the stern's half-beam fraction at a transom (0: double-ended)
  float bowFlat = 0;               // a junk's flat bow
  float rake = 0;                  // the stem's overhang at the rail
  float railH = 1.2f;              // the bulwark over the deck
  float castleA = 0, castleAT = -0.52f;   // a raised stern deck from the stern to castleAT
  float castleF = 0, castleFT = 0.62f;    // a forecastle from castleFT to the bow
  int posts = 0;                   // 1 stem and stern posts curling up, 2 a swan's neck and tail
  float postH = 0;
  bool reed = false, outrigger = false, shields = false, eye = false;
  int strakes = 4;
};
struct MastDef { float t = 0, h = 0, sw = 0; uint8_t cut = 0; int courses = 1; };
struct Rig { MastDef m[3]; int n = 0; bool bowsprit = false, jib = false; };
struct Pal {
  Ramp hull, hullDk, trim, deck, sail, sail2, rope, spar, gold, wet;
  bool trimBand = true;
};

uint8_t sailCutOf(int cul) {
  switch (cul) {
    case C_DUNE: case C_RIVER: return 1;
    case C_JADE: return 2;
    case C_MARSH: case C_SUN: case C_SYLVAN: case C_STAR: return 3;
    default: return 0;
  }
}

Form formFor(int hull, int cul) {
  Form f;
  if (hull <= 0) {
    f.L = 11.5f; f.B = 4.3f; f.F = 2.7f; f.sheerBow = 1.3f; f.sheerStern = 1.0f; f.bowP = 1.5f; f.sternP = 1.8f;
    f.transom = 0.46f; f.railH = 1.5f; f.strakes = 3;
  } else if (hull == 1) {
    f.L = 21; f.B = 7.0f; f.F = 5.0f; f.sheerBow = 1.9f; f.sheerStern = 1.4f; f.bowP = 1.4f; f.sternP = 2.2f;
    f.transom = 0.56f; f.rake = 2; f.railH = 1.4f; f.strakes = 4;
  } else {
    f.L = 38; f.B = 11.5f; f.F = 8.0f; f.sheerBow = 2.4f; f.sheerStern = 1.6f; f.bowP = 1.5f; f.sternP = 2.7f;
    f.transom = 0.70f; f.rake = 4; f.railH = 1.8f; f.castleA = 7; f.castleF = 4; f.strakes = 5;
  }
  const float k = hull <= 0 ? 0.5f : hull == 1 ? 1.0f : 1.7f;   // ornament scale
  switch (cul) {
    case C_FJORD:   // a longship: double-ended, low, the dragon stem and the curling stern, shields along the rail
      f.transom = 0; f.bowP = f.sternP = 1.3f; f.F *= 0.85f; f.posts = 1; f.postH = 5.0f * k + 2; f.shields = hull >= 1;
      f.castleA = f.castleF = 0; f.sheerBow += 0.8f * k; f.sheerStern += 0.8f * k; f.rake = 0;
      break;
    case C_HIGHLAND:   // a birlinn: double-ended, a tall plain stem and stern
      f.transom = 0; f.bowP = f.sternP = 1.4f; f.posts = 1; f.postH = 3.5f * k + 1.5f; f.castleF = 0; f.castleA *= 0.5f;
      break;
    case C_IMPERIAL:   // a carrack: tall castles, a small aft deck even on a sloop
      f.castleA = hull >= 2 ? 9.5f : hull == 1 ? 2.6f : 0; f.castleF = hull >= 2 ? 5.5f : 0; f.sheerStern += 0.6f * k;
      break;
    case C_DUNE:   // a dhow: a long raked prow, a high transom
      f.rake = hull >= 2 ? 10 : hull == 1 ? 6 : 2.5f; f.bowP = 1.2f; f.sheerBow += 0.6f * k; f.sheerStern += 1.0f * k;
      f.transom = 0.62f; f.castleF = 0; f.castleA *= 0.6f;
      break;
    case C_STEPPE:   // a river barge: flat sheer, bluff ends, a carved horse at the stem
      f.sheerBow = 0.7f; f.sheerStern = 0.6f; f.bowP = 2.6f; f.sternP = 2.6f; f.transom = 0.72f; f.posts = 1;
      f.postH = 2.5f * k + 1; f.castleF = 0; f.castleA *= 0.5f; f.B *= 1.08f;
      break;
    case C_MARSH:   // a reed boat: bundled reeds, the ends curled up
      f.transom = 0; f.bowP = f.sternP = 1.25f; f.reed = true; f.posts = 1; f.postH = 4.0f * k + 1.5f; f.castleA = f.castleF = 0;
      f.railH = 0.8f; f.sheerBow += 0.5f * k; f.sheerStern += 0.5f * k;
      break;
    case C_JADE:   // a junk: a flat transom bow, a towering stern, the eyes painted on the bow
      f.bowFlat = 0.52f; f.transom = 0.82f; f.sheerStern += (hull >= 2 ? 4.5f : 2.2f); f.sheerBow += 0.6f * k; f.eye = true;
      f.castleF = 0; f.castleA = hull >= 2 ? 5 : 0; f.bowP = 2.0f; f.rake = 0;
      break;
    case C_RIVER:   // a felucca: an upswept bow, gilded lotus ends
      f.transom = 0; f.sheerBow += 1.4f * k; f.sheerStern += 0.9f * k; f.posts = 1; f.postH = 2.0f * k + 1; f.castleA *= 0.5f;
      f.castleF = 0; f.bowP = 1.3f; f.sternP = 1.4f;
      break;
    case C_SUN:   // an outrigger canoe: double-ended, the float to starboard, the sun disc at the stem
      f.transom = 0; f.bowP = f.sternP = 1.25f; f.outrigger = hull <= 1; f.posts = 1; f.postH = 2.0f * k + 1;
      f.castleA *= 0.5f; f.castleF = 0; f.B *= hull <= 1 ? 0.85f : 1.0f;
      break;
    case C_SYLVAN:   // a swan ship: the neck and head at the bow, the tail swept up astern
      f.transom = 0; f.posts = 2; f.postH = 6.0f * k + 3; f.bowP = 1.35f; f.sternP = 1.5f; f.sheerStern += 1.6f * k;
      f.castleA *= 0.4f; f.castleF = 0;
      break;
    case C_STAR:   // a white ship of the high elves: a tall swept stern, a crescent prow
      f.transom = 0; f.sheerStern += 2.6f * k; f.sheerBow += 1.0f * k; f.rake = 3.0f * k; f.bowP = 1.3f; f.sternP = 1.5f;
      f.castleA *= 0.5f; f.castleF = 0; f.posts = 0;
      break;
    default: break;
  }
  return f;
}

Rig rigFor(int hull, uint8_t cut) {
  Rig r;
  if (hull <= 0) return r;   // oars
  if (hull == 1) {
    r.n = 1;
    switch (cut) {
      case 1: r.m[0] = {0.24f, 26, 0, 1, 1}; break;
      case 2: r.m[0] = {0.16f, 40, 13, 2, 1}; break;
      case 3: r.m[0] = {0.10f, 28, 0, 3, 1}; break;
      default: r.m[0] = {0.10f, 44, 13, 0, 1}; r.bowsprit = true; r.jib = true; break;
    }
    return r;
  }
  switch (cut) {
    case 1: r.n = 3; r.m[0] = {0.46f, 46, 0, 1, 1}; r.m[1] = {0.04f, 56, 0, 1, 1}; r.m[2] = {-0.46f, 40, 0, 1, 1}; break;
    case 2: r.n = 3; r.m[0] = {0.48f, 50, 15, 2, 1}; r.m[1] = {0.06f, 62, 20, 2, 1}; r.m[2] = {-0.50f, 44, 13, 2, 1}; break;
    case 3: r.n = 2; r.m[0] = {0.30f, 50, 0, 3, 1}; r.m[1] = {-0.30f, 44, 0, 3, 1}; break;
    default:
      r.n = 3; r.m[0] = {0.42f, 56, 18, 0, 2}; r.m[1] = {0.04f, 66, 21, 0, 2}; r.m[2] = {-0.52f, 38, 0, 1, 1};
      r.bowsprit = true; r.jib = true;
      break;
  }
  return r;
}

Pal palFor(int cul, const ShipStyle& st) {
  Pal p;
  uint32_t hull = rgba(132, 86, 54), trim = rgba(150, 50, 44), sail = rgba(226, 216, 190), sail2 = rgba(168, 48, 42);
  switch (cul) {
    case C_FJORD: hull = rgba(84, 60, 48); trim = rgba(164, 48, 40); sail = rgba(222, 206, 176); sail2 = rgba(170, 44, 40); break;
    case C_HIGHLAND: hull = rgba(110, 78, 54); trim = rgba(54, 90, 70); sail = rgba(196, 160, 96); sail2 = rgba(70, 92, 66); break;
    case C_HEART: hull = rgba(136, 90, 56); trim = rgba(48, 72, 140); sail = rgba(228, 218, 192); sail2 = rgba(52, 78, 150); break;
    case C_IMPERIAL: hull = rgba(78, 50, 40); trim = rgba(214, 168, 64); sail = rgba(236, 230, 214); sail2 = rgba(160, 32, 40); break;
    case C_DUNE: hull = rgba(176, 128, 80); trim = rgba(58, 148, 146); sail = rgba(234, 222, 196); sail2 = rgba(196, 110, 50); break;
    case C_STEPPE: hull = rgba(122, 92, 62); trim = rgba(70, 104, 160); sail = rgba(170, 152, 128); sail2 = rgba(80, 110, 168); break;
    case C_MARSH: hull = rgba(184, 150, 78); trim = rgba(110, 82, 46); sail = rgba(156, 98, 64); sail2 = rgba(104, 64, 44); break;
    case C_JADE: hull = rgba(70, 50, 42); trim = rgba(186, 52, 40); sail = rgba(184, 86, 56); sail2 = rgba(66, 44, 36); break;
    case C_RIVER: hull = rgba(162, 120, 74); trim = rgba(46, 116, 150); sail = rgba(240, 234, 218); sail2 = rgba(214, 170, 64); break;
    case C_SUN: hull = rgba(170, 78, 46); trim = rgba(54, 150, 140); sail = rgba(214, 180, 116); sail2 = rgba(180, 60, 44); break;
    case C_SYLVAN: hull = rgba(168, 160, 118); trim = rgba(88, 140, 70); sail = rgba(150, 186, 102); sail2 = rgba(232, 204, 110); break;
    case C_STAR: hull = rgba(224, 222, 214); trim = rgba(104, 136, 196); sail = rgba(244, 242, 236); sail2 = rgba(226, 196, 112); break;
    default: break;
  }
  if (st.hull) hull = st.hull;
  if (st.trim) trim = st.trim;
  if (st.sail) sail = st.sail;
  if (st.sail2) sail2 = st.sail2;
  if (st.extras & SX_PIRATE) { hull = rgba(58, 44, 42); trim = rgba(112, 32, 36); sail = rgba(64, 58, 66); sail2 = rgba(222, 214, 196); }
  p.hull = ramp(hull, 0.85f);
  p.hullDk = ramp(darken(hull, 0.35f), 0.8f);
  p.trim = ramp(trim, 0.8f);
  p.deck = ramp(mix(lighten(hull, 0.12f), rgba(158, 128, 96), 0.40f), 0.55f);
  p.sail = ramp(sail, 0.62f);
  p.sail2 = ramp(sail2, 0.62f);
  p.rope = kRope;
  p.spar = kWoodDark;
  p.gold = kGold;
  p.wet = ramp(mix(darken(hull, 0.55f), rgba(40, 70, 70), 0.35f), 0.6f);
  p.trimBand = cul != C_MARSH;
  return p;
}

// ---- the ship being drawn
struct Ship {
  int hull = 1, cul = 0, cul2 = 0;
  Form f;
  Rig rig;
  Pal pal;
  ShipStyle st;
  V3 F, R;              // forward and starboard (ground plane)
  float fill = 1;       // 0..1 the sails' belly
  float trim = 0;       // radians the yards are braced
  int frame = 0;
  float k = 1;          // ornament scale
  V3 at(float u, float v, float z) const { return F * u + R * v + v3(0, 0, z); }
};

float beamAt(const Form& f, float t) {
  t = std::max(-1.0f, std::min(1.0f, t));
  const bool bow = t >= 0;
  const float a = bow ? t : -t;
  float w = std::pow(std::max(0.0f, 1.0f - std::pow(a, bow ? f.bowP : f.sternP)), 0.55f);
  const float flat = bow ? f.bowFlat : f.transom;
  if (flat > 0) w = flat + (1 - flat) * w;
  return f.B * w;
}
float railZ(const Form& f, float t) {
  float z = f.F + f.sheerBow * std::pow(std::max(0.0f, t), 2.2f) + f.sheerStern * std::pow(std::max(0.0f, -t), 2.2f);
  if (f.castleA > 0 && t < f.castleAT) z += f.castleA;
  if (f.castleF > 0 && t > f.castleFT) z += f.castleF;
  return z;
}
float deckZ(const Form& f, float t) { return railZ(f, t) - f.railH; }
// along-axis position of a hull station: the stem rakes forward with height
float stationU(const Form& f, float t, float s) { return t * f.L + (t > 0 ? f.rake * s * std::pow(t, 3.0f) : 0.0f); }

// the hull: sides (both), transom, deck, rails, inner bulwarks, castles' bulkheads
void paintHull(SC& s, const Ship& S) {
  const Form& f = S.f;
  const Pal& P = S.pal;
  const int nt = (int)(f.L * 2 / 0.32f);
  // ---- the sides
  for (int side = -1; side <= 1; side += 2) {
    for (int i = 0; i <= nt; i++) {
      const float t = -1.0f + 2.0f * i / nt;
      const float w = beamAt(f, t), zr = railZ(f, t);
      if (w < 0.05f && f.transom <= 0 && std::fabs(t) > 0.999f) continue;
      const int ns = std::max(3, (int)(zr / 0.35f));
      for (int j = 0; j <= ns; j++) {
        const float sv = (float)j / ns;
        auto pos = [&](float tt, float ss) {
          const float ww = beamAt(f, tt), zz = railZ(f, tt) * ss;
          return S.at(stationU(f, tt, ss), side * ww * (0.80f + 0.20f * std::sqrt(std::max(0.0f, ss))), zz);
        };
        const V3 p = pos(t, sv);
        const float e = 0.02f;
        V3 n = unit3(cross3(pos(std::min(1.0f, t + e), sv) - pos(std::max(-1.0f, t - e), sv), pos(t, std::min(1.0f, sv + 0.05f)) - pos(t, std::max(0.0f, sv - 0.05f))));
        if (dot3(n, S.R * (float)side) < 0) n = n * -1.0f;
        float I = sunI(n);
        const float zAbs = zr * sv;
        const float u = stationU(f, t, sv);
        // what the plank is: the boot-top at the waterline, strakes, the painted sheer strake, the wale
        const Ramp* R = &P.hull;
        float bias = 0;
        if (f.reed) {
          R = &P.hull;
          const float ring = std::fmod(std::fabs(u + 40.0f), 4.0f);
          if (ring < 0.9f) { R = &P.trim; bias = -0.2f; }
          else bias = (std::fmod(zAbs * 1.7f + u * 0.15f, 1.0f) < 0.35f) ? -0.25f : 0.05f;   // the bundled stems
        } else {
          const float strake = sv * f.strakes;
          const float fr = strake - std::floor(strake);
          if (fr < 1.0f / std::max(2.0f, zr / f.strakes)) bias -= 0.55f;   // the plank seam
          if (P.trimBand && sv > 0.72f && zAbs > zr - 1.6f * (S.hull >= 2 ? 1.6f : 1.0f)) R = &P.trim;
          if (S.hull >= 1 && std::fabs(sv - 0.62f) < 0.5f / std::max(1.0f, zr * 0.5f)) { R = &P.hullDk; bias -= 0.2f; }   // the wale
          if (S.st.extras & SX_PLATING) {
            const float strap = std::fmod(std::fabs(u + 64.0f), S.hull >= 2 ? 9.0f : 6.0f);
            if (strap < 1.2f && sv > 0.1f) { R = &kIron; bias = strap < 0.5f ? 0.4f : -0.2f; }
          }
          if (S.st.extras & SX_PIRATE) bias -= hashf((int)(u * 2), (int)(zAbs * 2), 4711) < 0.08f ? 0.6f : 0.0f;   // patched, weathered
        }
        if (zAbs < 1.1f && S.hull >= 1) { R = &P.wet; bias -= 0.1f; }
        if ((S.st.extras & SX_WRECKED) && sv > 0.3f && vnoise(u * 0.45f + side * 7.0f, zAbs * 0.6f, 3131) > 0.62f) {
          if (std::fmod(std::fabs(u) + 40.0f, 3.0f) < 0.8f) { R = &P.hullDk; bias = -0.2f; }   // a rib showing through
          else continue;
        }
        // the junk's eye, painted on each bow
        if (f.eye && t > 0.72f && t < 0.97f) {
          const float er = S.hull >= 2 ? 2.3f : 1.4f;
          const float du = u - f.L * 0.84f, dz = zAbs - (zr - er - 1.0f);
          const float d = std::sqrt(du * du + dz * dz);
          if (d < er) { s.putC(p, d < er * 0.45f ? kInk : kWhite); continue; }
        }
        s.putL(p, *R, I, bias - 0.32f * (1.0f - sv));
      }
      // the foam the hull pushes along its waterline
      const V3 wl = S.at(t * f.L, side * (w * 0.80f + 1.0f + (S.frame ? 0.5f : 0.0f)), 0);
      if (i % 2 == 0) s.putFoam(wl);
    }
  }
  // ---- the transom (a flat stern; a junk's flat bow)
  auto flatEnd = [&](float t, float flat, bool bowEnd) {
    if (flat <= 0) return;
    const float w = beamAt(f, t), zr = railZ(f, t);
    const V3 n = bowEnd ? S.F : S.F * -1.0f;
    const float I = sunI(n);
    const int nv = (int)(w * 2 / 0.3f), nz = (int)(zr / 0.3f);
    for (int j = 0; j <= nz; j++)
      for (int i = 0; i <= nv; i++) {
        const float zz = zr * j / nz, sv = zz / std::max(0.1f, zr);
        const float v = (-1.0f + 2.0f * i / nv) * w * (0.80f + 0.20f * std::sqrt(sv));
        const Ramp* R = &P.hull;
        float bias = (std::fmod(zz, 2.0f) < 0.5f) ? -0.4f : 0.0f;
        if (P.trimBand && zz > zr - 1.6f) R = &P.trim;
        if (zz < 1.1f) R = &P.wet;
        // a galleon's stern lights: a row of small windows under the rail of its castle
        if (!bowEnd && S.hull >= 2 && f.castleA > 0 && zz > zr - f.castleA + 1 && zz < zr - 2.5f) {
          const float cell = std::fmod(v + 40.0f, 4.0f);
          if (std::fabs(v) < w * 0.7f && cell > 1.0f && cell < 3.0f && zz < zr - 3.2f) { s.putC(S.at(stationU(f, t, sv), v, zz), rgba(250, 210, 120)); continue; }
          if (std::fabs(v) < w * 0.75f) { R = &P.trim; bias = 0.1f; }
        }
        s.putL(S.at(stationU(f, t, sv) + (bowEnd ? 0.05f : -0.05f), v, zz), *R, I, bias);
      }
  };
  flatEnd(-1.0f, f.transom, false);
  flatEnd(1.0f, f.bowFlat, true);
  // ---- the deck, the rail's cap and the inside of the bulwarks
  for (int i = 0; i <= nt; i++) {
    const float t = -1.0f + 2.0f * i / nt;
    const float w = beamAt(f, t);
    if (w < 0.4f) continue;
    const float zr = railZ(f, t), zd = deckZ(f, t);
    const float wi = std::max(0.0f, w - 0.7f);
    const float u = stationU(f, t, 1.0f);
    const int nv = std::max(2, (int)(wi * 2 / 0.3f));
    for (int j = 0; j <= nv; j++) {
      const float v = -wi + 2.0f * wi * j / nv;
      const float cam = 0.5f * (1.0f - (v / std::max(0.1f, wi)) * (v / std::max(0.1f, wi)));
      float bias = 0;
      const Ramp* R = &P.deck;
      if (S.hull <= 0) {   // a rowboat's bottom boards and its thwarts
        R = &P.hullDk;
        bias = (std::fmod(std::fabs(v) + 10.0f, 1.6f) < 0.45f) ? -0.4f : -0.1f;
      } else if (f.reed) {
        R = &P.hull; bias = (std::fmod(std::fabs(v) * 1.3f, 1.0f) < 0.4f) ? -0.3f : 0.0f;
      } else {
        const float pl = std::fmod(v + 40.0f, 2.0f);
        if (pl < 0.42f) bias -= 0.30f;   // seams along the length
        const int plank = (int)std::floor((v + 40.0f) / 2.0f);
        if (std::fmod(u + 41.0f + plank * 5.3f, S.hull >= 2 ? 13.0f : 9.0f) < 0.4f) bias -= 0.35f;   // butt joints
      }
      // the bulwarks shade the deck along its edges (deepest under the sunward, north-west rail)
      const float edge = wi - std::fabs(v);
      if (edge < 1.3f) bias -= (1.3f - edge) * 0.45f;
      if (edge < 2.2f && dot3(S.R * (v < 0 ? -1.0f : 1.0f), kSun) > 0.05f) bias -= 0.35f;
      if ((S.st.extras & SX_WRECKED) && vnoise(u * 0.3f, v * 0.4f, 3137) > 0.65f) {   // stove in: the dark of the flooded hold (fixer M10 r1: fewer, smaller)
        s.putC(S.at(u, v, zd - 1.5f), std::fmod(std::fabs(u) + 40.0f, 3.0f) < 0.8f ? P.hullDk[1] : rgba(30, 44, 58), 1, false);
        continue;
      }
      s.putL(S.at(u, v, zd + cam * (S.hull >= 1 ? 0.5f : 0.1f)), *R, sunI(unit3(v3(0, 0, 1))) * 0.92f, bias, 1, false);
    }
    // the rail's cap
    for (int side = -1; side <= 1; side += 2)
      for (float v = wi; v <= w + 0.01f; v += 0.3f) s.putL(S.at(u, side * v, zr), f.reed ? P.hull : (S.hull >= 1 ? P.trim : P.hull), 0.95f, 0.1f);
    // the inside of the bulwarks (the far one faces the viewer)
    for (int side = -1; side <= 1; side += 2)
      for (float z = zd; z <= zr; z += 0.3f) {
        const V3 n = unit3(S.R * (float)-side);
        s.putL(S.at(u, side * wi, z), P.hullDk, sunI(n) * 0.9f + 0.05f, 0, 1, false);
      }
  }
  // ---- the castles' bulkheads (the step where a raised deck begins), with a door and windows
  auto bulkhead = [&](float t, bool aft) {
    const float w = beamAt(f, t) - 0.7f;
    const float zLow = deckZ(f, aft ? t + 0.02f : t - 0.02f), zHigh = railZ(f, aft ? t - 0.02f : t + 0.02f);
    const V3 n = aft ? S.F : S.F * -1.0f;
    const float I = sunI(n);
    const float u = stationU(f, t, 1.0f);
    for (float z = zLow; z <= zHigh; z += 0.3f)
      for (float v = -w; v <= w; v += 0.3f) {
        float bias = (std::fmod(v + 40.0f, 2.0f) < 0.4f) ? -0.35f : 0.0f;
        const Ramp* R = &P.hull;
        if (z > zHigh - 1.4f) R = &P.trim;
        if (std::fabs(v) < 1.6f && z < zLow + 4.2f) { R = &P.hullDk; bias = -0.5f; }   // the door
        if (std::fabs(std::fabs(v) - w * 0.6f) < 0.9f && z > zLow + 2.0f && z < zLow + 3.6f) { s.putC(S.at(u, v, z), rgba(70, 60, 70)); continue; }
        s.putL(S.at(u, v, z), *R, I, bias);
      }
  };
  if (f.castleA > 0) bulkhead(f.castleAT, true);
  if (f.castleF > 0) bulkhead(f.castleFT, false);
}

// the stem and stern posts / the swan's neck, and the figurehead
void paintPosts(SC& s, const Ship& S) {
  const Form& f = S.f;
  const Pal& P = S.pal;
  const bool fig = (S.st.extras & SX_FIGUREHEAD) || S.hull >= 2;
  const float r = S.hull <= 0 ? 0.6f : S.hull == 1 ? 0.9f : 1.3f;
  const V3 bow = S.at(stationU(f, 1.0f, 1.0f), 0, railZ(f, 1.0f) - 0.3f);
  const V3 stern = S.at(stationU(f, -1.0f, 1.0f), 0, railZ(f, -1.0f) - 0.3f);
  const V3 up = v3(0, 0, 1);
  const Ramp& postR = f.reed ? P.hull : (S.cul == C_RIVER ? P.trim : P.hull);
  auto tube = [&](V3 a, V3 b, V3 c, float r0, float r1, const Ramp& R, int n) {
    for (int i = 0; i < n; i++) rod(s, bez3(a, b, c, (float)i / n), bez3(a, b, c, (float)(i + 1) / n), r0 + (r1 - r0) * (i + 0.5f) / n, R);
  };
  if (f.posts == 1) {
    const float H = f.postH;
    // the stem rises and curls back; the stern post mirrors it, lower
    const V3 b1 = bow + S.F * (H * 0.45f) + up * (H * 0.7f), b2 = bow + S.F * (H * 0.15f) + up * H;
    tube(bow, b1, b2, r * 1.15f, r * 0.85f, postR, 8);
    const V3 b3 = b2 - S.F * (H * 0.28f) + up * (H * 0.02f);
    tube(b2, b2 - S.F * (H * 0.10f) + up * (H * 0.06f), b3, r * 0.85f, r * 0.7f, postR, 4);
    const V3 s1 = stern - S.F * (H * 0.40f) + up * (H * 0.6f), s2 = stern - S.F * (H * 0.10f) + up * (H * 0.85f);
    tube(stern, s1, s2, r * 1.1f, r * 0.8f, postR, 8);
    tube(s2, s2 + S.F * (H * 0.10f) + up * (H * 0.05f), s2 + S.F * (H * 0.22f), r * 0.8f, r * 0.6f, postR, 4);
    if (S.cul == C_FJORD && fig) {   // the dragon's head on the stem, jaws forward, a red eye
      const V3 hd = b2 + S.F * (H * 0.16f) + up * (H * 0.06f);
      ball3(s, hd, r * 1.5f, P.hull);
      rod(s, hd, hd + S.F * (r * 2.6f) - up * (r * 0.4f), r * 0.75f, P.hull);
      s.putC(hd + S.F * (r * 0.4f) + S.R * (r * 1.2f) + up * (r * 0.6f), rgba(230, 60, 40));
      s.putC(hd + S.F * (r * 0.4f) - S.R * (r * 1.2f) + up * (r * 0.6f), rgba(230, 60, 40));
      rod(s, hd - S.F * (r * 0.6f) + up * (r * 1.0f), hd - S.F * (r * 2.2f) + up * (r * 2.3f), r * 0.4f, P.trim);   // a horn
    } else if (S.cul == C_STEPPE && fig) {   // a carved horse's head
      const V3 hd = b2 + S.F * (H * 0.1f);
      ball3(s, hd, r * 1.2f, P.hull, 0.2f);
      rod(s, hd, hd + S.F * (r * 2.0f) - up * (r * 1.0f), r * 0.7f, P.hull, 0.2f);
      rod(s, hd - S.F * (r * 0.8f), hd - S.F * (r * 2.6f) - up * (r * 1.2f), r * 0.55f, P.trim);   // the mane
    } else if (S.cul == C_RIVER || S.cul == C_SUN) {   // gilded tips: a lotus, a sun disc
      ball3(s, b3, r * (S.cul == C_SUN ? 1.6f : 1.1f), P.gold);
      ball3(s, s2 + S.F * (H * 0.22f), r * 0.9f, P.gold);
    }
  } else if (f.posts == 2) {   // the swan: an S-curved neck, the head and beak forward; a tail swept up astern
    const float H = f.postH;
    const V3 n1 = bow + S.F * (H * 0.55f) + up * (H * 0.25f), n2 = bow + S.F * (H * 0.05f) + up * (H * 0.75f);
    const V3 hd = bow + S.F * (H * 0.28f) + up * (H * 1.0f);
    tube(bow, n1, n2, r * 1.5f, r * 1.0f, kSnow, 8);
    tube(n2, n2 + up * (H * 0.18f), hd, r * 1.0f, r * 0.9f, kSnow, 5);
    ball3(s, hd, r * 1.25f, kSnow);
    rod(s, hd + S.F * (r * 0.8f), hd + S.F * (r * 2.6f) - up * (r * 0.5f), r * 0.55f, kGold);
    s.putC(hd + S.F * (r * 0.6f) + S.R * (r * 1.0f) + up * (r * 0.4f), kInk);
    s.putC(hd + S.F * (r * 0.6f) - S.R * (r * 1.0f) + up * (r * 0.4f), kInk);
    const V3 t1 = stern - S.F * (H * 0.35f) + up * (H * 0.35f), t2 = stern - S.F * (H * 0.18f) + up * (H * 0.7f);
    tube(stern, t1, t2, r * 1.4f, r * 0.5f, kSnow, 7);
  }
  // a figurehead on a plain stem: a gilded bust (classic, heartland, imperial), a crescent (starspire)
  if (fig && f.posts == 0 && S.hull >= 1) {
    const V3 tip = bow + S.F * (r * 1.2f) - up * (r * 0.9f);
    if (S.cul == C_STAR) {
      for (int i = 0; i <= 10; i++) {
        const float a = -1.2f + 2.4f * i / 10.0f;
        ball3(s, tip + S.F * (std::cos(a) * r * 2.2f) + up * (std::sin(a) * r * 2.6f), r * 0.55f, kSnow, 0.2f);
      }
    } else if (S.cul != C_JADE && S.cul != C_DUNE) {
      ball3(s, tip, r * 1.25f, P.gold);
      ball3(s, tip + S.F * (r * 0.9f) + up * (r * 0.9f), r * 0.85f, P.gold, 0.2f);
    }
  }
}

// shields along the rail (longships), an outrigger's float, a rowboat's thwarts and oars, lanterns, nets
void paintFittings(SC& s, const Ship& S) {
  const Form& f = S.f;
  const Pal& P = S.pal;
  if (f.shields) {
    const float rr = S.hull >= 2 ? 2.2f : 1.6f, step = rr * 2.05f;
    int idx = 0;
    for (float u = -f.L * 0.62f; u <= f.L * 0.6f; u += step, idx++)
      for (int side = -1; side <= 1; side += 2) {
        const float t = u / f.L, w = beamAt(f, t) + 0.35f, zc = railZ(f, t) - rr * 0.55f;
        const V3 n = unit3(S.R * (float)side);
        const Ramp& R = (idx & 1) ? P.sail2 : (idx % 3 == 0 ? P.trim : P.sail);
        for (float a = -rr; a <= rr; a += 0.3f)
          for (float b = -rr; b <= rr; b += 0.3f) {
            const float d = a * a + b * b;
            if (d > rr * rr) continue;
            const V3 pp = S.at(u + a, side * w, zc + b);
            if (d < 0.5f) s.putL(pp, kIron, 0.95f);
            else s.putL(pp, R, sunI(unit3(n + v3(a / rr, 0, b / rr) * 0.4f)), d > (rr - 0.5f) * (rr - 0.5f) ? -0.5f : 0.0f);
          }
      }
  }
  if (f.outrigger) {   // the float to starboard and its two booms
    const float off = f.B + (S.hull >= 1 ? 7.0f : 5.0f), lf = f.L * 0.62f, rf = S.hull >= 1 ? 1.4f : 1.0f;
    for (float u = -lf; u <= lf; u += 0.3f) {
      const float tt = u / lf, rr = rf * std::sqrt(std::max(0.0f, 1.0f - tt * tt));
      for (int k = 0; k < 10; k++) {
        const float th = k * 0.31416f;   // the upper half
        const V3 n = unit3(S.R * std::cos(th) + v3(0, 0, std::sin(th)));
        s.putL(S.at(u, off, 0.2f) + S.R * (std::cos(th) * rr) + v3(0, 0, std::sin(th) * rr), P.hullDk, sunI(n));
      }
    }
    for (int b = -1; b <= 1; b += 2) {
      const float u = b * f.L * 0.35f;
      rod(s, S.at(u, beamAt(f, u / f.L) - 0.5f, railZ(f, u / f.L)), S.at(u, off, rf + 0.6f), 0.55f, P.spar);
    }
  }
  if (S.hull <= 0) {
    // thwarts across the boat
    for (float tt : {0.36f, -0.12f, -0.62f}) {
      const float w = beamAt(f, tt) - 0.5f, z = deckZ(f, tt) + 0.9f;
      for (float du = -0.7f; du <= 0.7f; du += 0.3f)
        for (float v = -w; v <= w; v += 0.3f) s.putL(S.at(tt * f.L + du, v, z), P.deck, 0.9f, du < -0.4f ? -0.4f : 0.0f, 1);
    }
    // the oars: pulling (frames alternate the stroke) or shipped inside along the thwarts when moored
    for (int side = -1; side <= 1; side += 2) {
      if (S.st.furled) {
        rod(s, S.at(-f.L * 0.62f, side * 1.3f, deckZ(f, 0) + 1.6f), S.at(f.L * 0.55f, side * 1.0f, deckZ(f, 0) + 1.6f), 0.45f, kWood);
        continue;
      }
      const float stroke = S.frame == 0 ? 3.2f : -3.2f;
      const float w = beamAt(f, 0.02f);
      const V3 lock = S.at(0.3f, side * (w + 0.2f), railZ(f, 0.02f) + 0.3f);
      const V3 blade = S.at(0.3f + stroke, side * (w + 9.5f), 0.25f);
      const V3 grip = S.at(0.3f - stroke * 0.35f, side * (w * 0.15f), railZ(f, 0.02f) + 1.2f);
      rod(s, grip, blade, 0.42f, kWood, 0.2f);
      for (float a = -1.5f; a <= 1.5f; a += 0.3f)   // the blade, flat on the water
        for (float b = -0.6f; b <= 0.6f; b += 0.3f) s.putL(blade + S.F * b + (blade - lock) * (a * 0.06f), kWood, 0.8f);
      s.putC(lock, kIron[1]);
    }
  }
  // the stern lantern on its post; a bow lantern with SX_LANTERN
  auto lantern = [&](V3 base) {
    const float lz = S.hull >= 2 ? 3.6f : 2.6f;
    rod(s, base, base + v3(0, 0, lz), 0.35f, kIron);
    const V3 c = base + v3(0, 0, lz + 0.9f);
    for (float a = -0.8f; a <= 0.8f; a += 0.3f)
      for (float b = -1.1f; b <= 1.1f; b += 0.3f)
        for (float e = -0.8f; e <= 0.8f; e += 0.4f) s.putC(c + S.F * a + S.R * e + v3(0, 0, b), std::fabs(b) > 0.8f ? kIron[1] : kGlow[3]);
  };
  if (S.hull >= 1) lantern(S.at(stationU(f, -0.96f, 1.0f), 0, railZ(f, -0.96f)));
  if (S.st.extras & SX_LANTERN) lantern(S.at(stationU(f, 0.9f, 1.0f), 0, railZ(f, 0.9f)));
  // a sloop's tiller, a galleon's wheel and hatch gratings
  if (S.hull == 1) {
    const float t0 = -0.92f;
    rod(s, S.at(t0 * f.L, 0, deckZ(f, t0) + 1.6f), S.at(t0 * f.L + 5.0f, 0, deckZ(f, t0) + 1.9f), 0.45f, kWood);
  }
  if (S.hull >= 2) {
    for (float tt : {0.24f, -0.26f}) {
      const float z = deckZ(f, tt) + 0.6f;
      for (float du = -2.4f; du <= 2.4f; du += 0.3f)
        for (float v = -2.0f; v <= 2.0f; v += 0.3f) {
          const bool bar = std::fmod(du + 10.0f, 1.2f) < 0.4f || std::fmod(v + 10.0f, 1.2f) < 0.4f;
          s.putL(S.at(tt * f.L + du, v, z), bar ? P.hull : P.hullDk, bar ? 0.9f : 0.2f, 0, 1);
        }
    }
    const float tw = f.castleA > 0 ? f.castleAT - 0.08f : -0.8f;
    const V3 wc = S.at(tw * f.L, 0, deckZ(f, tw) + 2.4f);
    for (int k = 0; k < 16; k++) {
      const float a = k * 0.3927f;
      s.putL(wc + S.R * (std::cos(a) * 1.8f) + v3(0, 0, std::sin(a) * 1.8f), kWood, 0.7f);
    }
    rod(s, wc - S.R * 1.9f, wc + S.R * 1.9f, 0.3f, kWood);
  }
  // deck cargo: a pair of barrels and a lashed crate forward, a coil of rope aft (never where the hero stands)
  if (S.hull >= 1 && !(S.st.extras & SX_WRECKED)) {
    auto barrel = [&](float tt, float v) {
      const float z0 = deckZ(f, tt), r = S.hull >= 2 ? 1.5f : 1.2f, hgt = S.hull >= 2 ? 3.2f : 2.6f;
      const V3 c = S.at(tt * f.L, v, z0);
      for (float z = 0; z <= hgt; z += 0.3f) {
        const float bulge = 1.0f + 0.12f * std::sin(3.14159f * z / hgt);
        const bool hoop = std::fabs(z - hgt * 0.22f) < 0.3f || std::fabs(z - hgt * 0.78f) < 0.3f;
        for (int k = 0; k < 20; k++) {
          const float th = k * 0.314f;
          const V3 n = v3(std::cos(th), std::sin(th), 0);
          s.putL(c + n * (r * bulge) + v3(0, 0, z), hoop ? kIron : kWood, sunI(n), hoop ? -0.2f : 0.0f);
        }
      }
      for (float rr = 0; rr <= r; rr += 0.3f)
        for (int k = 0; k < 20; k++) s.putL(c + v3(std::cos(k * 0.314f) * rr, std::sin(k * 0.314f) * rr, hgt), kWood, 0.85f, rr > r - 0.5f ? -0.3f : 0.1f);
    };
    auto crate = [&](float tt, float v, float sz) {
      const float z0 = deckZ(f, tt);
      for (float a = -sz; a <= sz; a += 0.3f)
        for (float b = -sz; b <= sz; b += 0.3f) {
          const bool slat = std::fabs(std::fabs(a) - sz) < 0.35f || std::fabs(std::fabs(b) - sz) < 0.35f;
          s.putL(S.at(tt * f.L, v, z0 + sz * 1.6f) + S.F * a + S.R * b, kWood, 0.9f, slat ? -0.35f : 0.0f);   // the lid
        }
      for (int side = 0; side < 4; side++) {   // the four faces
        const V3 n = side == 0 ? S.F : side == 1 ? S.F * -1.0f : side == 2 ? S.R : S.R * -1.0f;
        const V3 t = side < 2 ? S.R : S.F;
        for (float a = -sz; a <= sz; a += 0.3f)
          for (float z = 0; z <= sz * 1.6f; z += 0.3f) {
            const bool slat = std::fabs(std::fabs(a) - sz) < 0.35f || std::fabs(z - sz * 0.8f) < 0.3f;
            s.putL(S.at(tt * f.L, v, z0 + z) + n * sz + t * a, kWood, sunI(n), slat ? -0.4f : 0.0f);
          }
      }
    };
    barrel(S.hull >= 2 ? 0.30f : 0.40f, -beamAt(f, 0.35f) * 0.45f);
    barrel(S.hull >= 2 ? 0.36f : 0.47f, -beamAt(f, 0.35f) * 0.10f);
    crate(S.hull >= 2 ? 0.27f : 0.40f, beamAt(f, 0.35f) * 0.38f, S.hull >= 2 ? 1.9f : 1.4f);
    // a coil of rope on the after deck
    const float tc = S.hull >= 2 ? -0.30f : -0.55f;
    const V3 cc = S.at(tc * f.L, beamAt(f, tc) * 0.45f, deckZ(f, tc) + 0.4f);
    for (float rr = 0.4f; rr <= (S.hull >= 2 ? 1.8f : 1.3f); rr += 0.45f)
      for (int k = 0; k < 24; k++) s.putL(cc + v3(std::cos(k * 0.2618f) * rr, std::sin(k * 0.2618f) * rr, 0), kRope, 0.75f, (std::fmod(rr, 0.9f) < 0.45f) ? -0.3f : 0.1f, 1);
  }
  if (S.st.extras & SX_NETS) {   // a heap of net and a basket of the catch
    const float tt = S.hull <= 0 ? -0.4f : -0.3f;
    const V3 c = S.at(tt * f.L, 0.6f, deckZ(f, tt) + 1.0f);
    for (int i = 0; i < 60; i++) {
      const float a = hashf(i, 1, 99) * 6.283f, rr = std::sqrt(hashf(i, 2, 99)) * (S.hull <= 0 ? 2.4f : 3.4f);
      ball3(s, c + S.F * (std::cos(a) * rr) + S.R * (std::sin(a) * rr * 0.7f), 0.6f, kRope, (i & 3) ? -0.4f : 0.2f);
    }
  }
}

// ---- the sails
struct SailPaint {
  const Ship* S = nullptr;
  int kind = 0;        // 0 square, 1 lateen, 2 junk, 3 crab-claw, 4 jib
  float widthPx = 10, heightPx = 10;
};
// colour of a sail at (a across -1..1, b down 0..1): the culture's device, panel seams, the bolt rope, reef points
void sailColour(const SailPaint& sp, float a, float b, const Ramp*& R, float& bias, bool& hole) {
  const Ship& S = *sp.S;
  const Pal& P = S.pal;
  R = &P.sail;
  bias = 0;
  hole = false;
  const float ax = a * sp.widthPx * 0.5f, by = b * sp.heightPx;
  // panel seams and the bolt rope
  if (sp.kind != 2 && std::fmod(ax + 64.0f, 3.0f) < 0.35f) bias -= 0.22f;
  if (std::fabs(a) > 1.0f - 1.1f / std::max(4.0f, sp.widthPx * 0.5f) || b > 1.0f - 1.0f / std::max(4.0f, sp.heightPx)) bias -= 0.35f;
  const bool pirate = (S.st.extras & SX_PIRATE) != 0;
  if (pirate) {
    if (hashf((int)std::floor((ax + 40) / 3.0f), (int)std::floor(by / 3.0f), 1717) < 0.12f) bias += 0.35f;   // patches
    // the skull and crossed bones
    const float cx = 0, cy = sp.heightPx * (sp.kind == 0 ? 0.45f : 0.55f), rs = std::max(2.2f, std::min(sp.widthPx, sp.heightPx) * 0.12f);
    const float dx = ax - cx, dy = by - cy;
    const float d = std::sqrt(dx * dx + dy * dy);
    const bool bones = std::fabs(std::fabs(dx) - (dy - rs * 1.4f) * 0.9f) < 0.55f && dy > rs * 0.6f && dy < rs * 2.6f;
    if ((d < rs && !(std::fabs(std::fabs(dx) - rs * 0.4f) < 0.5f && std::fabs(dy + rs * 0.05f) < 0.6f)) || bones) { R = &P.sail2; bias += 0.2f; }
    if (S.st.extras & SX_WRECKED) hole = hashf((int)(ax * 0.7f + 50), (int)(by * 0.7f), 4242) < 0.12f;
    return;
  }
  if (S.st.extras & SX_WRECKED) {
    hole = vnoise(ax * 0.35f + 10, by * 0.35f, 5151) > 0.66f;
    bias -= 0.2f;
  }
  const int dev = S.cul2 ? S.cul2 : S.cul;   // a hidden people dress their sails in their second line's device
  switch (dev) {
    case C_FJORD: if (sp.kind == 0 && ((int)std::floor((a + 1.0f) * 3.5f) & 1)) R = &P.sail2; break;   // stripes
    case C_HIGHLAND: if (std::fabs(b - 0.42f) < 0.09f) R = &P.sail2; break;                              // a band
    case C_HEART: case C_CLASSIC:
      if (sp.kind == 0 && (b > 0.84f || (b > 0.70f && b < 0.76f))) R = &P.sail2;   // two bands along the foot
      break;
    case C_IMPERIAL: {
      const float dy = by - sp.heightPx * 0.45f, rr = std::min(sp.widthPx, sp.heightPx) * 0.24f;
      if (sp.kind == 0 && ax * ax + dy * dy < rr * rr) R = &P.sail2;
      if (b > 0.88f) R = &P.sail2;
      break;
    }
    case C_DUNE: case C_RIVER: if (b > 0.86f) R = &P.sail2; break;
    case C_STEPPE: if (b < 0.12f || b > 0.88f) R = &P.sail2; break;
    case C_MARSH: if (((int)std::floor(ax + 40) + (int)std::floor(by)) & 1) bias -= 0.18f; break;   // woven matting
    case C_SUN: {   // chevrons
      const float c = std::fmod(by * 0.5f + std::fabs(ax) * 0.4f, 4.0f);
      if (c < 1.0f) R = &P.sail2;
      break;
    }
    case C_SYLVAN: {   // a leaf: the midrib and its veins
      const float mid = std::fabs(a);
      if (mid < 0.09f || std::fabs(std::fmod(by - mid * sp.widthPx * 0.45f + 40.0f, 5.0f) - 0.5f) < 0.45f) R = &P.sail2;
      break;
    }
    case C_STAR: {   // a star at the heart
      const float dy = by - sp.heightPx * 0.48f;
      const float r2 = std::sqrt(ax * ax + dy * dy), th = std::atan2(dy, ax);
      if (r2 < std::min(sp.widthPx, sp.heightPx) * 0.2f * (0.45f + 0.55f * std::fabs(std::cos(th * 4.0f)))) R = &P.sail2;
      break;
    }
    case C_JADE: break;
    default: break;
  }
}

template <class PosF>
void paintSail(SC& s, const SailPaint& sp, PosF&& pos, int nu, int nv, float aMin = -1.0f, float aMax = 1.0f) {
  for (int j = 0; j <= nv; j++)
    for (int i = 0; i <= nu; i++) {
      const float a = aMin + (aMax - aMin) * i / nu, b = (float)j / nv;
      const float ea = (aMax - aMin) / nu * 0.8f + 0.01f, eb = 1.0f / nv * 0.8f + 0.01f;
      bool ok = true;
      const V3 p = pos(a, b, ok);
      if (!ok) continue;
      bool o1 = true, o2 = true, o3 = true, o4 = true;
      const V3 tu = pos(std::min(aMax, a + ea), b, o1) - pos(std::max(aMin, a - ea), b, o2);
      const V3 tv = pos(a, std::min(1.0f, b + eb), o3) - pos(a, std::max(0.0f, b - eb), o4);
      V3 n = unit3(cross3(tu, tv));
      if (dot3(n, kToEye) < 0) n = n * -1.0f;   // canvas: the side we see
      float I = sunI(n);
      const float back = dot3(n, kSun);
      if (back < 0) I = std::max(I, 0.40f + 0.25f * -back);   // the sun through the cloth
      const Ramp* R;
      float bias;
      bool hole;
      sailColour(sp, a, b, R, bias, hole);
      if (hole) continue;
      if (sp.kind == 2) {   // a junk's battens: dark bars across the panels
        const float fb = b * 6.0f - std::floor(b * 6.0f);
        if (fb < 0.11f || b < 0.02f) { s.putL(p, sp.S->pal.spar, 0.5f, -0.2f); continue; }
      }
      s.putL(p, *R, I, bias);
    }
}

// a furled sail: a lumpy roll of canvas along a spar, tied with gaskets
void furl(SC& s, const Ship& S, V3 a, V3 b, float r) {
  const V3 ax = b - a;
  const float L = len3(ax);
  if (L < 0.5f) return;
  const V3 t = ax * (1.0f / L);
  const V3 u = std::fabs(t.z) < 0.9f ? unit3(cross3(t, v3(0, 0, 1))) : unit3(cross3(t, v3(1, 0, 0)));
  const V3 w = cross3(t, u);
  const int nl = (int)(L / 0.35f);
  for (int i = 0; i <= nl; i++) {
    const float x = L * i / nl;
    const float ends = std::min(1.0f, std::min(x, L - x) / std::max(1.0f, r * 1.6f));
    const float rr = r * 0.85f * (0.55f + 0.45f * ends) * (0.93f + 0.07f * std::sin(x * 1.3f));
    const bool tie = std::fmod(x + 0.5f, S.hull >= 2 ? 5.0f : 4.0f) < 0.7f;
    for (int k = 0; k < 18; k++) {
      const float th = k * 0.349f;
      const V3 n = u * std::cos(th) + w * std::sin(th);
      s.putL(a + t * x + n * rr - v3(0, 0, rr * 0.6f), tie ? S.pal.rope : S.pal.sail, sunI(n) * 0.85f, tie ? -0.6f : -0.45f);
    }
  }
}

// rotate a point about the vertical axis through `piv` by `th` radians (positive: toward starboard aft)
inline V3 swing(V3 p, V3 piv, float th) {
  const float dx = p.x - piv.x, dy = p.y - piv.y, c = std::cos(th), sn = std::sin(th);
  return v3(piv.x + dx * c - dy * sn, piv.y + dx * sn + dy * c, p.z);
}

struct Tops { V3 p[3]; int n = 0; };

void paintRig(SC& s, const Ship& S, Tops& tops, V3& bowspritEnd) {
  const Form& f = S.f;
  const Pal& P = S.pal;
  const bool furled = S.st.furled, wreck = (S.st.extras & SX_WRECKED) != 0;
  const V3 up = v3(0, 0, 1);
  const uint32_t ropeC = rgba(58, 44, 44);
  // the bowsprit
  bowspritEnd = S.at(stationU(f, 1.0f, 1.0f), 0, railZ(f, 1.0f));
  if (S.rig.bowsprit && !wreck) {
    const V3 b0 = S.at(stationU(f, 0.84f, 1.0f), 0, railZ(f, 0.84f) - 0.2f);
    const float lb = f.L * (S.hull >= 2 ? 0.34f : 0.33f);
    bowspritEnd = b0 + S.F * (lb * 0.95f) + up * (lb * 0.32f);
    rod(s, b0, bowspritEnd, S.hull >= 2 ? 0.9f : 0.6f, P.spar);
  }
  // which mast is the main (the tallest) and the foremost
  int mainI = 0, foreI = 0;
  for (int i = 0; i < S.rig.n; i++) {
    if (S.rig.m[i].h > S.rig.m[mainI].h) mainI = i;
    if (S.rig.m[i].t > S.rig.m[foreI].t) foreI = i;
  }
  for (int i = 0; i < S.rig.n; i++) {
    const MastDef& m = S.rig.m[i];
    const float u = m.t * f.L, z0 = deckZ(f, m.t);
    float h = m.h;
    if (wreck) h = z0 + (h - z0) * (i == mainI ? 0.55f : 0.3f);
    const float r = S.hull >= 2 ? (i == mainI ? 1.35f : 1.1f) : 0.85f;
    const V3 base = S.at(u, 0, z0);
    // the mast, tapering
    for (int k = 0; k < 6; k++) {
      const float za = z0 + (h - z0) * k / 6.0f, zb = z0 + (h - z0) * (k + 1) / 6.0f;
      rod(s, S.at(u, 0, za), S.at(u, 0, zb), r * (1.0f - 0.4f * k / 6.0f), P.spar);
    }
    tops.p[tops.n++] = S.at(u, 0, h);
    if (wreck) continue;
    // a galleon's fighting top
    if (S.hull >= 2 && m.cut == 0 && m.courses >= 2) {
      const float zt = h * 0.60f;
      for (float rr = 0; rr <= 3.2f; rr += 0.3f)
        for (int k = 0; k < 28; k++) {
          const float th = k * 0.2244f;
          const V3 p = S.at(u, 0, zt) + S.F * (std::cos(th) * rr) + S.R * (std::sin(th) * rr * 1.1f);
          s.putL(p, P.hull, rr > 2.7f ? 0.5f : 0.85f, rr > 2.7f ? -0.3f : 0.0f);
        }
      for (int k = 0; k < 28; k++) {   // its rim
        const float th = k * 0.2244f;
        for (float z = 0.3f; z <= 1.2f; z += 0.3f) {
          const V3 n = unit3(S.F * std::cos(th) + S.R * std::sin(th));
          s.putL(S.at(u, 0, zt + z) + S.F * (std::cos(th) * 3.2f) + S.R * (std::sin(th) * 3.5f), P.hull, sunI(n));
        }
      }
    }
    // shrouds down to the rails
    const int nsh = S.hull >= 2 ? 2 : 1;
    for (int side = -1; side <= 1; side += 2)
      for (int k = 0; k < nsh; k++) {
        const float tc = m.t - 0.05f - 0.06f * k;
        rope(s, S.at(u, 0, h * 0.86f), S.at(tc * f.L, side * beamAt(f, tc), railZ(f, tc)), ropeC);
      }
    // ---- the sail(s)
    SailPaint sp;
    sp.S = &S;
    sp.kind = m.cut;
    // which way the yards are braced and the sheets eased: never square, and to the side that shows the canvas best
    // (in the high 3/4 view a sail whose plane runs north-south is seen edge-on); the helm braces them harder
    const float hd = std::atan2(S.F.y, S.F.x);
    auto braceOf = [&](float mag) { return std::fabs(std::sin(hd - mag)) >= std::fabs(std::sin(hd + mag)) ? mag : -mag; };
    auto sheetOf = [&](float mag) { return std::fabs(std::cos(hd + mag)) >= std::fabs(std::cos(hd - mag)) ? mag : -mag; };
    const float tr = braceOf(0.42f + std::fabs(S.trim) * 0.6f);
    if (m.cut == 0) {   // square: courses hung from yards braced round by the trim
      const V3 Yd = unit3(S.R * std::cos(tr) + S.F * std::sin(tr));
      const V3 Fs = unit3(S.F * std::cos(tr) - S.R * std::sin(tr));
      struct Course { float zT, zB, hT, hB; };
      Course cs[2];
      int nc = 0;
      if (m.courses >= 2) {
        cs[nc++] = {h * 0.56f, z0 + 4.0f, m.sw * 0.92f, m.sw};
        cs[nc++] = {h * 0.93f, h * 0.60f, m.sw * 0.62f, m.sw * 0.80f};
      } else cs[nc++] = {h * 0.92f, z0 + 3.2f, m.sw * 0.86f, m.sw};
      for (int c = 0; c < nc; c++) {
        const Course& K = cs[c];
        const V3 yc = S.at(u, 0, K.zT) + Fs * (r + 0.3f);
        rod(s, yc - Yd * (K.hT + 1.4f), yc + Yd * (K.hT + 1.4f), S.hull >= 2 ? 0.75f : 0.55f, P.spar);
        if (furled) { furl(s, S, yc - Yd * (K.hT + 0.6f), yc + Yd * (K.hT + 0.6f), S.hull >= 2 ? 1.7f : 1.25f); continue; }
        sp.widthPx = (K.hT + K.hB);
        sp.heightPx = K.zT - K.zB;
        const float fill = S.fill;
        auto pos = [&](float a, float b, bool& ok) {
          ok = true;
          const float hw = K.hT + (K.hB - K.hT) * b, z = K.zT + (K.zB - K.zT) * b;
          const float sb = b < 0.62f ? std::sin(1.5708f * b / 0.62f) : 1.0f - 0.35f * ((b - 0.62f) / 0.38f) * ((b - 0.62f) / 0.38f);
          float belly = fill * (0.20f * hw + 0.6f) * (1.0f - a * a) * sb;
          if (fill < 0.3f) belly += (0.3f - fill) * 2.4f * std::sin(a * 7.0f + b * 3.0f) * b;   // luffing
          return S.at(u, 0, z) + Fs * (r + 0.3f + belly) + Yd * (a * hw);
        };
        paintSail(s, sp, pos, std::max(8, (int)(sp.widthPx / 0.38f)), std::max(6, (int)(sp.heightPx / 0.38f)));
        if (c == 0)   // the sheets from the clews to the rail
          for (int side = -1; side <= 1; side += 2) {
            bool ok;
            const float tc = m.t - 0.18f;
            rope(s, pos((float)side, 1.0f, ok), S.at(tc * f.L, side * beamAt(f, tc), railZ(f, tc)), ropeC);
          }
      }
    } else if (m.cut == 1) {   // lateen: a long slanted yard, the triangle under it, swung out to leeward
      const float D = (S.hull >= 2 ? 0.37f : 0.60f) * f.L;
      const float zA = z0 + 2.6f, hp = 2.0f * h - zA;
      const float th = sheetOf(0.55f + std::fabs(S.trim) * 0.6f);
      const V3 piv = S.at(u, 0, 0);
      // furled, the yard stays aloft, slanted fore and aft over the hull, the canvas bundled along it (fixer M10 r1:
      // lowered along the deck it read as a dismasted wreck, its ends far past bow and stern)
      const V3 A = swing(S.at(u + D * (furled ? 0.62f : 1.0f), 0, furled ? zA + 1.0f : zA), piv, furled ? 0.0f : th);
      const V3 B = swing(S.at(u - D * (furled ? 0.70f : 1.05f), 0, furled ? zA + (hp - zA) * 0.82f : hp), piv, furled ? 0.0f : th);
      const V3 C = swing(S.at(u - D * 0.95f, 0, z0 + 2.4f), piv, th);
      const V3 lee = unit3(S.R * std::cos(th) - S.F * std::sin(th)) * (th >= 0 ? 1.0f : -1.0f);
      rod(s, A - (B - A) * 0.04f, B + (B - A) * 0.03f, S.hull >= 2 ? 0.75f : 0.55f, P.spar);
      if (furled) { furl(s, S, A, B, S.hull >= 2 ? 1.5f : 1.1f); }
      else {
        sp.widthPx = len3(B - A);
        sp.heightPx = len3(C - (A + B) * 0.5f) * 0.9f;
        const float fill = S.fill;
        auto pos = [&](float a, float b, bool& ok) {
          ok = true;
          const float sAl = (a + 1.0f) * 0.5f;
          const V3 Yp = lerp3(A, B, sAl), Fp = lerp3(A, C, sAl);
          const float belly = fill * (0.10f * sp.widthPx * 0.5f + 0.5f) * std::sin(3.14159f * sAl) * std::sin(3.14159f * std::min(1.0f, b * 1.1f)) +
                              (fill < 0.3f ? (0.3f - fill) * 2.0f * std::sin(sAl * 9.0f) * b : 0.0f);
          return lerp3(Yp, Fp, b) + lee * belly;
        };
        paintSail(s, sp, pos, std::max(10, (int)(sp.widthPx / 0.38f)), std::max(8, (int)(sp.heightPx / 0.38f)));
        bool ok;
        rope(s, pos(1.0f, 1.0f, ok), S.at((m.t - 0.4f) * f.L, 0, railZ(f, m.t - 0.4f)), ropeC);
      }
    } else if (m.cut == 2) {   // a junk's battened lug: a fan of panels on battens, the mast through its fore part
      const float W = m.sw;
      const float th = sheetOf(0.50f + std::fabs(S.trim) * 0.5f);
      const V3 piv = S.at(u, 0, 0);
      const float zTop = h * 0.94f, zBot = z0 + 4.0f;
      const V3 lee = unit3(S.R * std::cos(th) - S.F * std::sin(th)) * (th >= 0 ? 1.0f : -1.0f);
      if (furled) {   // lowered: the battens stacked at the foot, the yard on top
        for (int k = 0; k < 5; k++) {
          const float z = zBot + 0.6f * k;
          rod(s, swing(S.at(u + W * 0.22f, 0, z), piv, th), swing(S.at(u - W * 0.82f, 0, z + 0.4f), piv, th), 0.7f, k == 4 ? P.spar : P.sail);
        }
      } else {
        sp.widthPx = W;
        sp.heightPx = zTop - zBot;
        const float fill = S.fill;
        auto pos = [&](float a, float b, bool& ok) {
          ok = true;
          const float al = (a + 1.0f) * 0.5f;   // 0 luff .. 1 leech
          const float zt = zTop + al * h * 0.10f;   // the yard rises aft
          const float z = zt + (zBot - zt) * b;
          const float du = W * 0.22f - al * W * (1.02f + 0.10f * std::sin(3.14159f * b));
          const float pan = b * 6.0f - std::floor(b * 6.0f);
          const float belly = fill * (0.35f + 0.06f * W) * std::sin(3.14159f * al) * (0.4f + 0.6f * std::sin(3.14159f * pan));
          return swing(S.at(u + du, 0, z), piv, th) + lee * belly;
        };
        paintSail(s, sp, pos, std::max(10, (int)(W / 0.36f)), std::max(10, (int)(sp.heightPx / 0.36f)));
        for (int k = 0; k <= 6; k++) {   // the sheets fanning down from the battens' ends
          bool ok;
          rope(s, pos(1.0f, k / 6.0f, ok), S.at((m.t - 0.32f) * f.L, 0, railZ(f, m.t - 0.32f) + 1.0f), ropeC);
        }
      }
    } else {   // crab-claw (and the elves' leaf and crescent): two curved spars from a tack low forward
      const float th = sheetOf(0.50f + std::fabs(S.trim) * 0.5f);
      const V3 piv = S.at(u, 0, 0);
      const bool star = S.cul == C_STAR, leaf = S.cul == C_SYLVAN;
      const float span = (S.hull >= 2 ? 0.46f : 0.55f) * f.L;
      const V3 T = swing(S.at(u + span * 0.55f, 0, z0 + 2.4f), piv, th);
      const V3 Eu = swing(S.at(u - span * 0.25f, 0, h * 1.55f), piv, th), El = swing(S.at(u - span * 1.05f, 0, h * 0.80f), piv, th);
      const float bow = star ? 0.30f : 0.16f;
      const V3 Mu = lerp3(T, Eu, 0.5f) + (swing(S.at(u - 1.0f, 0, 0), piv, th) - piv) * (span * bow), Ml = lerp3(T, El, 0.5f) + v3(0, 0, -span * bow * 0.5f);
      const V3 lee = unit3(S.R * std::cos(th) - S.F * std::sin(th)) * (th >= 0 ? 1.0f : -1.0f);
      auto spar = [&](V3 a, V3 b, V3 c) {
        for (int k = 0; k < 8; k++) rod(s, bez3(a, b, c, k / 8.0f), bez3(a, b, c, (k + 1) / 8.0f), S.hull >= 2 ? 0.65f : 0.5f, P.spar);
      };
      spar(T, Mu, Eu);
      spar(T, Ml, El);
      if (furled) furl(s, S, T, lerp3(Eu, El, 0.5f), S.hull >= 2 ? 1.5f : 1.1f);
      else {
        sp.widthPx = len3(Eu - El);
        sp.heightPx = len3(lerp3(Eu, El, 0.5f) - T);
        const float fill = S.fill;
        auto pos = [&](float a, float b, bool& ok) {
          const float al = (a + 1.0f) * 0.5f;   // 0 the lower spar .. 1 the upper
          const float notch = leaf ? -0.10f : (star ? 0.30f : 0.24f);
          ok = b <= 1.0f - notch * std::sin(3.14159f * al);
          const V3 p = lerp3(bez3(T, Ml, El, b), bez3(T, Mu, Eu, b), al);
          const float belly = fill * (0.10f * sp.widthPx + 0.6f) * std::sin(3.14159f * al) * std::sin(3.14159f * std::min(1.0f, b));
          return p + lee * belly;
        };
        paintSail(s, sp, pos, std::max(10, (int)(sp.widthPx / 0.36f)), std::max(10, (int)(sp.heightPx / 0.36f)));
        bool ok;
        rope(s, pos(-1.0f, 0.9f, ok), S.at((m.t - 0.45f) * f.L, 0, railZ(f, m.t - 0.45f)), ropeC);
      }
    }
  }
  if (wreck) return;
  // stays: the foremost masthead to the bowsprit, between the mastheads, the aftmost to the stern
  if (S.rig.n > 0) {
    const MastDef& fm = S.rig.m[foreI];
    const V3 fTop = S.at(fm.t * f.L, 0, fm.cut == 0 || fm.cut == 2 ? fm.h * 0.97f : fm.h);
    rope(s, fTop, bowspritEnd, ropeC);
    for (int i = 0; i < S.rig.n; i++)
      for (int j = 0; j < S.rig.n; j++)
        if (S.rig.m[j].t > S.rig.m[i].t && S.rig.m[j].t - S.rig.m[i].t < 0.6f)
          rope(s, S.at(S.rig.m[i].t * f.L, 0, S.rig.m[i].h * 0.9f), S.at(S.rig.m[j].t * f.L, 0, S.rig.m[j].h * 0.62f), ropeC);
    // the jib on the forestay
    if (S.rig.jib) {
      const V3 Hd = S.at(fm.t * f.L, 0, fm.h * (S.hull >= 2 ? 0.80f : 0.84f)) + S.F * 0.6f;
      const float th = S.trim * 0.8f + 0.12f;
      const V3 lee = unit3(S.R * (th >= 0 ? 1.0f : -1.0f));
      const V3 Cl = S.at(fm.t * f.L + f.L * 0.08f, 0, deckZ(f, fm.t) + 3.0f) + lee * (2.5f + 3.0f * S.fill);
      // (fixer M10 r1) furled, the jib is stowed on the bowsprit: a full-length roll of canvas from the masthead read
      // as a second spar lying across the ship (the forestay rope above still runs up to the mast)
      if (S.st.furled) furl(s, S, lerp3(Hd, bowspritEnd, 0.72f), bowspritEnd, S.hull >= 2 ? 0.9f : 0.7f);
      else {
        SailPaint sp;
        sp.S = &S;
        sp.kind = 4;
        sp.widthPx = len3(bowspritEnd - Hd);
        sp.heightPx = len3(Cl - lerp3(Hd, bowspritEnd, 0.5f));
        const float fill = S.fill;
        auto pos = [&](float a, float b, bool& ok) {
          ok = true;
          const float sl = (a + 1.0f) * 0.5f;
          const V3 L = lerp3(Hd, bowspritEnd, sl);
          return lerp3(L, Cl, b) + lee * (fill * 1.6f * std::sin(3.14159f * sl) * std::sin(3.14159f * b) * (1.0f - 0.5f * b));
        };
        paintSail(s, sp, pos, std::max(8, (int)(sp.widthPx / 0.38f)), std::max(6, (int)(sp.heightPx / 0.38f)));
      }
    }
    // the pennant at the main masthead: two colours, flying aft and to leeward (drooping when moored)
    const MastDef& mm = S.rig.m[mainI];
    const V3 top = S.at(mm.t * f.L, 0, mm.h + 0.5f);
    rod(s, top - v3(0, 0, 0.5f), top + v3(0, 0, 1.6f), 0.4f, P.spar);
    const uint32_t pa = S.st.pennantA ? S.st.pennantA : P.sail2[2], pb = S.st.pennantB ? S.st.pennantB : P.trim[3];
    const Ramp ra = ramp(pa, 0.6f), rb = ramp(pb, 0.6f);
    const float pl = S.hull >= 2 ? 13.0f : 8.5f;
    const V3 dir = unit3(S.F * -std::cos(S.trim * 0.8f) + S.R * std::sin(S.trim * 0.8f + 0.25f));
    for (float x = 0; x <= pl; x += 0.3f) {
      const float wv = std::sin(x * 0.75f + S.frame * 1.6f) * (0.35f + x * 0.05f);
      const float wd = (S.hull >= 2 ? 1.9f : 1.4f) * (1.0f - 0.65f * x / pl);
      V3 c = top + v3(0, 0, 1.0f) + dir * x + v3(0, 0, wv);
      if (S.st.furled) c = top + v3(0, 0, 1.0f) + dir * (x * 0.35f) - v3(0, 0, x * 0.75f) + S.R * (wv * 0.4f);
      const bool tail = x > pl * 0.5f;
      for (float y = -wd; y <= wd; y += 0.3f) s.putL(c + v3(0, 0, y), tail ? rb : ra, y < 0 ? 0.55f : 0.8f);
    }
  }
}

Ship makeShip(int hull, const ShipStyle& st, int fineDir, int frame) {
  Ship S;
  S.hull = std::max(0, std::min(2, hull));
  S.cul = st.culture <= 12 ? st.culture : 0;
  S.cul2 = st.culture2 <= 12 ? st.culture2 : 0;
  S.st = st;
  S.f = formFor(S.hull, S.cul);
  const uint8_t cut = st.sailCut <= 3 ? st.sailCut : 0;
  S.rig = rigFor(S.hull, cut);
  // lateen rigs carry short masts and long yards; the hero's place amidships stays clear of the masts
  for (int i = 0; i < S.rig.n; i++) {
    MastDef& m = S.rig.m[i];
    if (m.cut == 1 && S.hull >= 2) m.h = m.h * 0.72f;
    if (S.hull == 1 && m.cut == 0) m.t = 0.22f;
    if (S.hull >= 2 && m.cut == 0 && i == 1) m.t = 0.12f;
  }
  S.pal = palFor(S.cul, st);
  const float a = (float)(((fineDir % SHIP_FINE_DIRS) + SHIP_FINE_DIRS) % SHIP_FINE_DIRS) * 6.2831853f / SHIP_FINE_DIRS;
  S.F = v3(std::cos(a), std::sin(a), 0);
  S.R = v3(-std::sin(a), std::cos(a), 0);
  S.fill = st.furled ? 0.0f : std::min(2, (int)st.fill) / 2.0f;
  S.trim = std::max(-1, std::min(1, (int)st.brace)) * 0.34f;
  S.frame = frame & 1;
  S.k = S.hull <= 0 ? 0.5f : S.hull == 1 ? 1.0f : 1.7f;
  return S;
}

}  // namespace

// ---------------------------------------------------------------- the public ship API
int shipCellW(int hull) { return hull >= 2 ? 132 : hull == 1 ? 84 : 44; }
int shipCellH(int hull) { return hull >= 2 ? 140 : hull == 1 ? 92 : 40; }
int shipAnchorX(int hull) { return shipCellW(hull) / 2; }
int shipAnchorY(int hull) { return hull >= 2 ? 90 : hull == 1 ? 62 : 25; }
int shipDeckY(int hull) { return hull >= 2 ? 6 : hull == 1 ? 4 : 1; }

uint8_t shipSailCutFor(uint8_t culture) { return sailCutOf(culture <= 12 ? culture : 0); }

uint64_t shipStyleKey(int hull, const ShipStyle& st) {
  uint64_t k = 0xC6A4A7935BD1E995ull ^ (uint64_t)hull;
  auto mx = [&](uint64_t v) { k ^= v + 0x9E3779B97F4A7C15ull + (k << 6) + (k >> 2); };
  mx(st.hull); mx(st.trim); mx(st.sail); mx(st.sail2);
  mx((uint64_t)st.culture | (uint64_t)st.sailCut << 8 | (uint64_t)st.pennant << 16 | (uint64_t)(st.furled ? 1 : 0) << 24);
  mx((uint64_t)(uint8_t)st.brace |(uint64_t)st.fill << 8 | (uint64_t)st.culture2 << 16 | (uint64_t)st.extras << 24);
  mx((uint64_t)st.pennantA << 32 | st.pennantB);
  return k;
}

Canvas shipCell(int hull, const ShipStyle& st, int fineDir, int frame, int layer) {
  hull = std::max(0, std::min(2, hull));
  const int W = shipCellW(hull), H = shipCellH(hull);
  const Ship S = makeShip(hull, st, fineDir, frame);
  SC s(W, H, (float)shipAnchorX(hull), (float)shipAnchorY(hull));
  const bool wreck = (st.extras & SX_WRECKED) != 0;
  s.sink = (S.frame ? 0.7f : 0.0f) + (wreck ? 2.2f : 0.0f);
  const float heelSide = S.trim < 0 ? -1.0f : 1.0f;
  s.heel = (S.frame ? 0.018f : 0.0f) + 0.034f * S.fill * (S.rig.n > 0 ? 1.0f : 0.0f) + (wreck ? 0.11f : 0.0f);
  s.heelDir = S.R * heelSide;
  paintHull(s, S);
  paintPosts(s, S);
  paintFittings(s, S);
  Tops tops;
  V3 bse;
  paintRig(s, S, tops, bse);
  // ---- composite: the ship, its outline, the layer split, then the foam and the shadow under it
  Canvas c(W, H);
  for (size_t i = 0; i < c.px.size(); i++) c.px[i] = s.col[i];
  const Canvas solid = c;
  outline(c, 0.85f);
  auto frontAt = [&](size_t i) { return s.deck[i] ? s.wy[i] > 3.0f : s.wy[i] > 0.6f; };
  if (layer != SHIP_LAYER_ALL)
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        const size_t i = (size_t)y * W + x;
        if (!chA(c.px[i])) continue;
        bool front;
        if (chA(solid.px[i])) front = frontAt(i);
        else {
          static const int nx[4] = {0, -1, 1, 0}, ny[4] = {1, 0, 0, -1};
          front = false;
          for (int k = 0; k < 4; k++) {
            const int qx = x + nx[k], qy = y + ny[k];
            if (qx < 0 || qy < 0 || qx >= W || qy >= H) continue;
            const size_t j = (size_t)qy * W + qx;
            if (chA(solid.px[j])) { front = frontAt(j); break; }
          }
        }
        if ((layer == SHIP_LAYER_BACK) == front) c.px[i] = 0;
      }
  if (layer != SHIP_LAYER_FRONT)
    for (size_t i = 0; i < c.px.size(); i++) {
      if (chA(c.px[i])) continue;
      if (s.foam[i] && !wreck) c.px[i] = rgba(226, 242, 248, 150);
      else if (s.sh[i]) c.px[i] = rgba(12, 26, 58, 76);
    }
  return c;
}

Canvas shipSheet(int hull, const ShipStyle& st) {
  hull = std::max(0, std::min(2, hull));
  const int cw = shipCellW(hull), ch = shipCellH(hull);
  Canvas sheet(cw * SHIP_DIRS, ch * SHIP_FRAMES);
  for (int f = 0; f < SHIP_FRAMES; f++)
    for (int d = 0; d < SHIP_DIRS; d++) {
      const Canvas c = shipCell(hull, st, d * SHIP_FINE_DIRS / SHIP_DIRS, f, SHIP_LAYER_ALL);
      for (int y = 0; y < ch; y++)
        for (int x = 0; x < cw; x++) sheet.set(d * cw + x, f * ch + y, c.get(x, y));
    }
  return sheet;
}

int shipMastTops(int hull, const ShipStyle& st, int fineDir, int* xs, int* ys) {
  hull = std::max(0, std::min(2, hull));
  const Ship S = makeShip(hull, st, fineDir, 0);
  int n = 0;
  for (int i = 0; i < S.rig.n && n < 3; i++) {
    const MastDef& m = S.rig.m[i];
    const V3 p = S.at(m.t * S.f.L, 0, m.h);
    xs[n] = (int)std::floor(shipAnchorX(hull) + p.x);
    ys[n] = (int)std::floor(shipAnchorY(hull) + p.y - p.z);
    n++;
  }
  return n;
}

void shipLanternAt(int hull, const ShipStyle& st, int fineDir, int& x, int& y) {
  hull = std::max(0, std::min(2, hull));
  const Ship S = makeShip(hull, st, fineDir, 0);
  const float lz = railZ(S.f, -0.96f) + (hull >= 2 ? 4.5f : 3.5f);
  const V3 p = S.at(stationU(S.f, -0.96f, 1.0f), 0, hull <= 0 ? 2.0f : lz);
  x = (int)std::floor(shipAnchorX(hull) + p.x);
  y = (int)std::floor(shipAnchorY(hull) + p.y - p.z);
}

// ---------------------------------------------------------------- the sea props (the ships' renderer: one light, real volume)
namespace {
// a box standing on the ground: centre (cx, cy), half sizes along the axes F / R, height h; planks along F (seams)
void box3(SC& s, V3 c, V3 F, V3 R, float hf, float hr, float h, const Ramp& Rp, float seam = 0, float bias = 0) {
  const V3 up = v3(0, 0, 1);
  for (float a = -hf; a <= hf; a += 0.3f)   // the top
    for (float b = -hr; b <= hr; b += 0.3f) {
      const bool edge = std::fabs(std::fabs(a) - hf) < 0.35f || std::fabs(std::fabs(b) - hr) < 0.35f;
      s.putL(c + F * a + R * b + up * h, Rp, sunI(up) * 0.95f, bias + (edge ? 0.15f : 0.0f) + (seam > 0 && std::fmod(b + 20.0f, seam) < 0.35f ? -0.3f : 0.0f));
    }
  for (int side = 0; side < 4; side++) {
    const V3 n = side == 0 ? F : side == 1 ? F * -1.0f : side == 2 ? R : R * -1.0f;
    const V3 t = side < 2 ? R : F;
    const float off = side < 2 ? hf : hr, span = side < 2 ? hr : hf;
    for (float a = -span; a <= span; a += 0.3f)
      for (float z = 0; z <= h; z += 0.3f) {
        float bb = bias;
        if (seam > 0 && std::fmod(z + 20.0f, seam) < 0.35f) bb -= 0.35f;
        if (std::fabs(std::fabs(a) - span) < 0.35f) bb -= 0.15f;
        s.putL(c + n * off + t * a + up * z, Rp, sunI(n), bb);
      }
  }
}
// an upright cylinder (posts, towers, barrels): radius by height (taper), a colour by height (bands)
template <class ColF>
void cyl(SC& s, V3 base, float r0, float r1, float h, ColF&& col) {
  const int na = std::max(12, (int)(std::max(r0, r1) * 9));
  for (float z = 0; z <= h; z += 0.3f) {
    const float r = r0 + (r1 - r0) * (z / std::max(0.1f, h));
    for (int k = 0; k < na; k++) {
      const float th = k * 6.2831853f / na;
      const V3 n = unit3(v3(std::cos(th), std::sin(th), (r0 - r1) / std::max(0.1f, h)));
      float bias = 0;
      const Ramp& R = col(z, th, bias);
      s.putL(base + v3(std::cos(th) * r, std::sin(th) * r, z), R, sunI(n), bias);
    }
  }
}
void disc3(SC& s, V3 c, float r, const Ramp& R, float I, float bias = 0) {
  for (float rr = 0; rr <= r; rr += 0.3f)
    for (int k = 0; k < std::max(8, (int)(rr * 7)); k++) {
      const float th = k * 6.2831853f / std::max(8, (int)(rr * 7));
      s.putL(c + v3(std::cos(th) * rr, std::sin(th) * rr, 0), R, I, bias + (rr > r - 0.5f ? -0.25f : 0.0f));
    }
}
// the prop's canvas from the renderer: its pixels, the outline, its shadow on the ground
Canvas propOut(SC& s, bool shadowIt = true) {
  Canvas c(s.w, s.h);
  for (size_t i = 0; i < c.px.size(); i++) c.px[i] = s.col[i];
  outline(c, 0.9f);
  if (shadowIt)
    for (size_t i = 0; i < c.px.size(); i++)
      if (!chA(c.px[i]) && s.sh[i]) c.px[i] = rgba(20, 16, 40, 90);
  return c;
}
const Ramp kWhiteStone = ramp5(rgba(96, 96, 116), rgba(150, 150, 160), rgba(204, 202, 198), rgba(230, 228, 218), rgba(250, 248, 238));
const Ramp kWeed = ramp5(rgba(26, 46, 34), rgba(40, 74, 44), rgba(62, 104, 52), rgba(96, 136, 64), rgba(140, 170, 86));
const Ramp kWicker = ramp5(rgba(92, 62, 38), rgba(138, 98, 54), rgba(180, 136, 76), rgba(210, 172, 104), rgba(232, 204, 140));
const Ramp kFishR = ramp5(rgba(70, 82, 100), rgba(116, 134, 150), rgba(166, 184, 194), rgba(206, 220, 226), rgba(240, 248, 250));
}  // namespace

void paintSeaProp(Canvas& out, Prop p, int frame) {
  const int W = out.w, H = out.h;
  const bool wide = p == Prop::Slipway || p == Prop::Wreck;
  SC s(W, H, W * 0.5f, H - (wide ? 16.0f : 8.0f) + 3.0f);
  const V3 X = v3(1, 0, 0), Y = v3(0, 1, 0), up = v3(0, 0, 1);
  bool shadowIt = true;
  switch (p) {
    case Prop::MooringPost: {   // a blackened iron bollard bolted to the pier: a squat post under a mushroom cap, a hawser
      // looped on it and trailing off over the edge
      const Ramp kIronBl = ramp5(rgba(22, 22, 32), rgba(38, 40, 52), rgba(60, 62, 76), rgba(92, 96, 108), rgba(140, 146, 156));
      cyl(s, v3(0, 0, 0), 3.4f, 3.4f, 1.0f, [&](float, float, float& b) -> const Ramp& { b = -0.2f; return kIronBl; });   // the base plate
      // (squat: a knee-high bollard, never a figure's height, with a low cap, so at 1x it reads as iron on the pier)
      cyl(s, v3(0, 0, 1.0f), 2.4f, 2.1f, 3.4f, [&](float z, float, float& b) -> const Ramp& { b = z < 0.6f ? -0.3f : 0.0f; return kIronBl; });
      cyl(s, v3(0, 0, 4.4f), 2.3f, 2.8f, 1.0f, [&](float, float, float& b) -> const Ramp& { b = 0.1f; return kIronBl; });   // the cap's flare
      disc3(s, v3(0, 0, 5.4f), 2.8f, kIronBl, 0.9f, 0.15f);
      for (int k = 0; k < 6; k++) s.putC(v3(std::cos(k * 1.047f) * 2.9f, std::sin(k * 1.047f) * 2.9f, 0.9f), kIronBl[4]);   // the bolts
      for (int turn = 0; turn < 2; turn++)   // the hawser's loop round the post
        for (int k = 0; k < 28; k++) {
          const float th = k * 0.2244f;
          const V3 n = v3(std::cos(th), std::sin(th), 0);
          s.putL(v3(n.x * 2.8f, n.y * 2.8f, 2.2f + turn * 0.9f + std::sin(th) * 0.3f), kRope, sunI(unit3(n + up * 0.5f)), (k % 4 == 0) ? -0.35f : 0.1f);
        }
      for (int k = 0; k < 14; k++) s.putL(v3(2.8f + k * 0.45f, 1.2f + k * 0.2f, std::max(0.4f, 2.6f - k * 0.25f)), kRope, 0.8f, (k & 1) ? -0.2f : 0.1f);   // trailing off
      break;
    }
    case Prop::NetRack: {   // two poles and a bar, a brown net hung in folds to dry, cork floats along its edge
      for (int side = -1; side <= 1; side += 2) rod(s, v3(side * 11.0f, 0, 0), v3(side * 11.0f, 0, 17.0f), 1.0f, kWood);
      rod(s, v3(-12.5f, 0, 16.5f), v3(12.5f, 0, 16.5f), 0.8f, kWood);
      for (float x = -10.0f; x <= 10.0f; x += 0.3f)
        for (float z = 2.0f; z <= 16.0f; z += 0.3f) {
          const float sag = 1.6f * std::sin((x + 10.0f) / 20.0f * 3.14159f) * (16.0f - z) / 14.0f;
          const float fold = std::sin(x * 1.1f) * 0.8f;
          const float dropZ = z - 4.0f * (1.0f - std::cos((x + 10.0f) / 20.0f * 3.14159f * 2.0f)) * 0.25f;
          if (dropZ < 2.5f) continue;
          const float u = x * 0.7f + z * 0.7f, v = x * 0.7f - z * 0.7f;
          const bool knot = std::fabs(std::fmod(u + 40.0f, 2.2f)) < 0.42f || std::fabs(std::fmod(v + 40.0f, 2.2f)) < 0.42f;
          if (!knot) continue;
          s.putL(v3(x, sag + fold * 0.3f, dropZ), kRope, 0.55f + 0.3f * std::cos(x * 1.1f), -0.2f);
        }
      for (float x = -9.0f; x <= 9.0f; x += 3.0f) ball3(s, v3(x, 0.8f, 3.0f + std::fabs(std::sin(x)) * 0.8f), 0.9f, ramp(rgba(214, 130, 60), 0.6f));
      break;
    }
    case Prop::FishBaskets: {   // a round wicker basket brimming with silver fish, a crate of them beside
      cyl(s, v3(-3.0f, 0, 0), 4.4f, 5.0f, 6.0f, [&](float z, float th, float& b) -> const Ramp& {
        b = ((int)(z * 1.4f) + (int)(th * 5.0f)) % 2 ? -0.25f : 0.05f;   // the weave
        return kWicker;
      });
      for (int i = 0; i < 18; i++) {   // the catch heaped in it
        const float a = hashf(i, 3, 71) * 6.283f, rr = std::sqrt(hashf(i, 4, 71)) * 3.6f;
        const V3 c = v3(-3.0f + std::cos(a) * rr, std::sin(a) * rr, 6.2f + hashf(i, 5, 71) * 1.2f);
        rod(s, c - v3(std::cos(a * 2) * 1.6f, std::sin(a * 2) * 1.0f, 0), c + v3(std::cos(a * 2) * 1.6f, std::sin(a * 2) * 1.0f, 0), 0.75f, kFishR);
      }
      box3(s, v3(5.5f, 1.0f, 0), X, Y, 3.2f, 3.0f, 4.0f, kWood, 1.6f);
      for (int i = 0; i < 6; i++) rod(s, v3(3.5f + i * 0.8f, -0.8f + (i & 1), 4.4f), v3(3.5f + i * 0.8f, 2.6f + (i & 1) * 0.4f, 4.4f), 0.6f, kFishR, (i & 1) ? -0.2f : 0.1f);
      break;
    }
    case Prop::Buoy: {   // a red and white buoy rocking on the water, its little cage and bell on top
      shadowIt = false;
      const float rock = (frame == 1 ? 1 : frame == 3 ? -1 : 0) * 0.12f, bob = (frame == 1 || frame == 2) ? -0.6f : 0.0f;
      const V3 base = v3(0, 0, bob);
      s.heel = rock; s.heelDir = X;
      cyl(s, base, 4.2f, 1.6f, 8.0f, [&](float z, float, float& b) -> const Ramp& { b = 0; return ((int)(z / 2.4f) & 1) ? kWhiteStone : kPaintRed; });
      rod(s, base + up * 8.0f, base + up * 12.0f, 0.4f, kIron);
      for (int k = 0; k < 3; k++) rod(s, base + up * 8.0f + v3(std::cos(k * 2.1f) * 1.4f, std::sin(k * 2.1f) * 1.4f, 0), base + up * 11.5f, 0.3f, kIron);
      ball3(s, base + up * 10.0f, 0.9f, kBrass);
      for (int k = 0; k < 24; k++) {   // the water ringing it
        const float th = k * 0.2618f, rr = 5.5f + (frame & 1) * 0.7f;
        s.putFoam(v3(std::cos(th) * rr, std::sin(th) * rr * 0.6f, 0));
      }
      break;
    }
    case Prop::Anchor: {   // a great iron anchor leaning on a coil of chain, its wooden stock across the shank
      const Ramp Rust = ramp5(rgba(54, 34, 34), rgba(96, 56, 44), rgba(140, 82, 52), rgba(176, 116, 70), rgba(206, 156, 100));
      for (int turn = 0; turn < 3; turn++)   // the chain coil
        for (int k = 0; k < 26; k++) {
          const float th = k * 0.2417f, rr = 5.0f - turn * 1.2f;
          ball3(s, v3(-2.0f + std::cos(th) * rr, 2.0f + std::sin(th) * rr * 0.8f, 0.8f + turn * 1.1f), 0.75f, (k + turn) % 3 ? kIron : Rust);
        }
      const V3 crown = v3(3.0f, -1.0f, 1.2f), ring = v3(-1.5f, -1.0f, 17.0f);
      rod(s, crown, ring, 1.1f, kIron);
      rod(s, ring + v3(-4.5f, 0, -2.0f) + Y * 0.3f, ring + v3(4.5f, 0, -2.0f) + Y * 0.3f, 1.0f, kWood);   // the stock
      for (int k = 0; k < 16; k++) {   // the ring
        const float th = k * 0.3927f;
        s.putL(ring + v3(std::cos(th) * 1.6f, 0, std::sin(th) * 1.6f + 1.4f), kIron, 0.7f);
      }
      for (int side = -1; side <= 1; side += 2) {   // the arms curving up to the flukes
        const V3 a1 = crown + v3(side * 3.5f, 0.6f, 1.6f), a2 = crown + v3(side * 6.0f, 1.0f, 5.0f);
        rod(s, crown, a1, 1.0f, kIron);
        rod(s, a1, a2, 0.9f, kIron);
        for (float u = 0; u <= 1.0f; u += 0.1f) ball3(s, a2 + v3(side * u * 0.6f, 0, -u * 2.2f), 1.3f - u * 0.6f, Rust);
      }
      break;
    }
    case Prop::CargoPile: {   // crates stacked, a barrel, a heap of sacks, lashed with rope
      box3(s, v3(-5.0f, 1.5f, 0), X, Y, 4.5f, 4.0f, 7.0f, kWood, 2.0f);
      box3(s, v3(-4.5f, 1.2f, 7.0f), X, Y, 3.6f, 3.2f, 5.0f, kWoodDark, 1.7f);
      rope(s, v3(-9.5f, 5.6f, 6.8f), v3(-0.5f, 5.6f, 6.8f), kRope[1]);
      rope(s, v3(-5.0f, 5.6f, 0.4f), v3(-5.0f, 5.6f, 7.0f), kRope[1]);
      cyl(s, v3(5.0f, 3.0f, 0), 3.2f, 3.2f, 8.0f, [&](float z, float, float& b) -> const Ramp& {
        b = (std::fabs(z - 1.5f) < 0.5f || std::fabs(z - 6.5f) < 0.5f) ? -0.1f : 0.0f;
        const float bulge = std::sin(z / 8.0f * 3.14159f);
        (void)bulge;
        return (std::fabs(z - 1.5f) < 0.5f || std::fabs(z - 6.5f) < 0.5f) ? kIron : kWood;
      });
      disc3(s, v3(5.0f, 3.0f, 8.0f), 3.0f, kWood, 0.85f);
      for (int i = 0; i < 3; i++) {   // the sacks
        const V3 c = v3(4.0f + i * 2.8f - 2.0f, -3.0f + (i & 1), 2.0f);
        for (int k = 0; k < 3; k++) ball3(s, c + v3(0, 0, k * 0.9f), 2.6f - k * 0.5f, kCloth, k == 2 ? 0.2f : 0.0f);
      }
      break;
    }
    case Prop::Slipway: {   // the shipwright's slip: a sloop's hull up on its cradle, half planked (its ribs showing through
      // the gaps), shores propping its sides, the slip's timbers running down to the water, planks stacked by it
      for (float x = -23.0f; x <= 23.0f; x += 4.5f) box3(s, v3(x, 0, 0), X, Y, 1.0f, 10.0f, 1.2f, kWoodDark, 0, -0.15f);   // the sleepers
      for (int side = -1; side <= 1; side += 2) box3(s, v3(0, side * 4.5f, 1.2f), X, Y, 23.0f, 0.9f, 1.0f, kWood, 0);   // the ways
      for (float x = -14.0f; x <= 14.0f; x += 7.0f) box3(s, v3(x, 0, 2.2f), X, Y, 1.2f, 5.5f, 1.6f, kWood, 0, 0.1f);    // the cradle's chocks
      ShipStyle st;
      st.extras = SX_WRECKED;   // (the holes in a wrecked hull are the unplanked bays here)
      st.furled = true;
      Ship S = makeShip(1, st, 0, 0);
      S.rig.n = 0;              // no mast stepped yet
      s.sink = -3.6f;           // up on the cradle
      paintHull(s, S);
      paintPosts(s, S);
      s.sink = 0;
      for (int side = -1; side <= 1; side += 2)
        for (float x = -12.0f; x <= 12.0f; x += 8.0f) rod(s, v3(x, side * 12.5f, 0), v3(x, side * 7.4f, 7.5f), 0.6f, kWood);   // the shores
      box3(s, v3(-15.0f, 13.0f, 0), X, Y, 7.0f, 1.3f, 1.2f, kWood, 0);   // planks waiting
      box3(s, v3(-14.0f, 13.0f, 1.2f), X, Y, 6.0f, 1.1f, 1.0f, kWood, 0, 0.1f);
      box3(s, v3(16.0f, 12.5f, 0), X, Y, 2.0f, 2.0f, 3.0f, kWood, 1.5f);   // a crate of trenails
      break;
    }
    case Prop::HarbourDesk: {   // the harbourmaster's booth: a counter, a shingled roof on posts, a ledger, a lantern, the sign
      box3(s, v3(0, 1.5f, 0), X, Y, 10.0f, 3.0f, 8.0f, kWood, 2.0f);
      box3(s, v3(0, 1.5f, 8.0f), X, Y, 11.0f, 3.8f, 1.0f, kWoodDark, 0, 0.1f);   // the counter top
      for (int i = 0; i < 4; i++) rod(s, v3((i & 1) ? 10.0f : -10.0f, (i & 2) ? 4.0f : -4.0f, 0), v3((i & 1) ? 10.0f : -10.0f, (i & 2) ? 4.0f : -4.0f, 20.0f), 0.8f, kWood);
      // the roof: two shingled slopes over a ridge running north-south (the west one lit, the east in shade), the eaves
      // overhanging, the south gable toward the viewer
      for (float x = -14.0f; x <= 14.0f; x += 0.3f)
        for (float y = -7.5f; y <= 7.5f; y += 0.3f) {
          const float z = 28.0f - std::fabs(x) * 0.62f;
          const V3 n = unit3(v3(x < 0 ? -0.6f : 0.6f, 0, 0.8f));
          const bool row = std::fmod(std::fabs(x) + 20.0f, 2.0f) < 0.4f;
          const bool joint = std::fmod(y + 40.0f + (((int)(std::fabs(x) / 2.0f)) & 1) * 1.3f, 2.6f) < 0.35f;
          s.putL(v3(x, y, z), kPaintRed, sunI(n), (row ? -0.35f : 0.0f) + (joint ? -0.2f : 0.0f) + (std::fabs(x) > 13.3f ? -0.25f : 0.0f));
        }
      for (float y = -7.5f; y <= 7.5f; y += 0.3f) s.putL(v3(0, y, 28.3f), kWoodDark, 0.85f, 0.2f);   // the ridge cap
      for (float x = -12.6f; x <= 12.6f; x += 0.3f)   // the south gable under the eaves
        for (float z = 20.0f; z <= 27.6f - std::fabs(x) * 0.62f; z += 0.3f)
          s.putL(v3(x, 7.0f, z), kWood, sunI(Y), std::fmod(x + 20.0f, 2.0f) < 0.4f ? -0.3f : 0.0f);
      for (float x = -14.0f; x <= 14.0f; x += 0.3f) s.putL(v3(x, 7.6f, 28.0f - std::fabs(x) * 0.62f - 0.3f), kWoodDark, 0.6f);   // the barge board
      box3(s, v3(-4.0f, 2.0f, 9.0f), X, Y, 3.0f, 2.0f, 0.8f, kCanvasR, 0, 0.1f);   // the ledger, open
      for (float x = -6.5f; x <= -1.5f; x += 1.0f) s.putC(v3(x, 2.4f, 9.9f), kInk);
      ball3(s, v3(6.0f, 2.0f, 9.6f), 0.8f, kGlow);   // an inkwell's glint: the lamp's
      rod(s, v3(8.5f, 4.2f, 20.0f), v3(8.5f, 4.2f, 17.0f), 0.25f, kIron);   // the lantern hanging from the eave
      box3(s, v3(8.5f, 4.2f, 14.6f), X, Y, 1.0f, 1.0f, 2.4f, kGlow, 0, 0.2f);
      // the sign board on the front: an anchor painted on it
      for (float x = -5.0f; x <= 5.0f; x += 0.3f)
        for (float z = 3.0f; z <= 7.0f; z += 0.3f) {
          const float ax = std::fabs(x), az = z - 5.0f;
          const bool anchorMark = (ax < 0.4f && az > -1.6f && az < 1.6f) || (std::fabs(az + 1.2f) < 0.35f && ax < 1.8f) || (std::fabs(az - 1.0f) < 0.35f && ax < 1.0f);
          s.putL(v3(x, 4.7f, z), anchorMark ? kGold : kStoneWarm, sunI(Y), std::fabs(ax - 5.0f) < 0.4f || std::fabs(az) > 1.8f ? -0.4f : 0.0f);
        }
      break;
    }
    case Prop::Lighthouse: {   // a tapering white tower banded in red on a rocky foot, the gallery, the lit lantern, the cap
      for (int i = 0; i < 9; i++) {   // the rocks at its foot
        const float a = i * 0.698f;
        ball3(s, v3(std::cos(a) * 8.5f, std::sin(a) * 6.5f, 1.0f), 2.4f + hashf(i, 1, 33) * 1.4f, kStone, -0.1f);
      }
      const float Ht = 58.0f;
      cyl(s, v3(0, 0, 0), 7.5f, 4.6f, Ht, [&](float z, float th, float& b) -> const Ramp& {
        b = 0;
        if (std::fmod(z + 0.0f, 3.0f) < 0.35f) b -= 0.18f;   // the courses
        if (std::fmod(th * 3.82f + ((int)(z / 3.0f) & 1) * 0.5f, 1.0f) < 0.08f) b -= 0.12f;
        return (((int)(z / 11.0f)) & 1) ? kPaintRed : kWhiteStone;
      });
      // the door and a window or two up the face toward the viewer
      for (float x = -1.4f; x <= 1.4f; x += 0.3f)
        for (float z = 0.5f; z <= 5.0f; z += 0.3f) s.putL(v3(x, 7.6f, z), kWoodDark, sunI(Y), z > 4.4f ? 0.2f : -0.2f);
      for (int w = 0; w < 3; w++) {
        const float z = 16.0f + w * 14.0f, r = 7.5f - 2.9f * z / Ht + 0.1f, th = 1.2f + w * 0.6f;
        for (float dz = 0; dz <= 2.4f; dz += 0.3f)
          for (float dth = -0.08f; dth <= 0.08f; dth += 0.03f) s.putC(v3(std::cos(th + dth) * r, std::sin(th + dth) * r, z + dz), rgba(56, 46, 62));
      }
      // the gallery: a walk round the lantern room with a rail
      disc3(s, v3(0, 0, Ht), 6.5f, kIron, 0.85f);
      for (int k = 0; k < 40; k++) {
        const float th = k * 0.157f;
        rod(s, v3(std::cos(th) * 6.3f, std::sin(th) * 6.3f, Ht), v3(std::cos(th) * 6.3f, std::sin(th) * 6.3f, Ht + 2.4f), 0.2f, kIron);
        s.putL(v3(std::cos(th) * 6.3f, std::sin(th) * 6.3f, Ht + 2.5f), kIron, 0.8f);
      }
      // the lantern room: lit glass panes between iron mullions, the lamp's glow within
      cyl(s, v3(0, 0, Ht), 3.6f, 3.6f, 7.0f, [&](float z, float th, float& b) -> const Ramp& {
        b = 0.2f;
        if (std::fmod(th * 1.273f, 1.0f) < 0.10f || z < 0.5f || z > 6.5f) { b = 0; return kIron; }
        b = 0.9f;
        return kGlow;
      });
      cyl(s, v3(0, 0, Ht + 7.0f), 4.6f, 0.6f, 3.6f, [&](float, float, float& b) -> const Ramp& { b = 0; return kIron; });   // the cap
      ball3(s, v3(0, 0, Ht + 11.0f), 0.8f, kBrass);
      break;
    }
    case Prop::Wreck: {   // a sloop gone down by the stern on the reef: the bow lifted on the rocks, the deck stove in,
      // the mast snapped and its sail in rags, weed at the waterline, timbers strewn round it
      ShipStyle st;
      st.extras = SX_WRECKED;
      st.fill = 0;
      const Ship S = makeShip(1, st, 29, 0);
      s.ox = W * 0.5f + 2.0f; s.oy = H - 13.0f;
      s.pitch = 0.20f; s.pitchDir = S.F; s.sink = 2.2f; s.heel = 0.10f; s.heelDir = S.R;
      paintHull(s, S);
      paintPosts(s, S);
      Tops tops;
      V3 bse;
      paintRig(s, S, tops, bse);
      s.pitch = 0; s.sink = 0; s.heel = 0;
      const V3 bow = S.F * 20.0f;
      for (int i = 0; i < 6; i++) ball3(s, bow + v3(-3.0f + i * 1.6f, 2.0f + (i % 3) * 1.4f - 2.0f, 0.3f), 1.6f + (i & 1) * 0.9f, kStone, -0.15f);   // the reef
      for (int i = 0; i < 5; i++) rod(s, v3(-20.0f + i * 8.0f, 9.0f + (i & 1) * 2.0f, 0.0f), v3(-18.5f + i * 8.0f, 10.0f + (i & 1) * 2.0f, 0.6f), 0.5f, kWood, -0.2f);
      shadowIt = false;
      for (int k = 0; k < 60; k++) {   // the swell breaking on it
        const float th = k * 0.1047f;
        s.putFoam(S.F * (std::cos(th) * 18.0f) + S.R * (std::sin(th) * 8.5f));
      }
      break;
    }
    default: break;
  }
  out = propOut(s, shadowIt);
  if (p == Prop::Buoy || p == Prop::Wreck)   // the ring of foam round it, under it
    for (size_t i = 0; i < out.px.size(); i++)
      if (!chA(out.px[i]) && s.foam[i]) out.px[i] = rgba(226, 242, 248, 150);
  (void)frame; (void)W; (void)H;
}

// ---------------------------------------------------------------- the catch card's fish
Canvas fishSprite(uint32_t color, uint32_t seed, int rarity) {
  const int W = 40, H = 22;
  Canvas c(W, H);
  const uint32_t h = seed * 2654435761u + 12345u;
  const int shape = (int)((h >> 3) % 4);   // 0 trout, 1 a long pike / eel, 2 a deep bream, 3 a round tuna
  const float len = shape == 1 ? 17.5f : shape == 2 ? 13.0f : 15.0f;
  const float dep = shape == 1 ? 3.4f : shape == 2 ? 7.5f : shape == 3 ? 6.5f : 5.0f;
  const float cx = W * 0.47f, cy = H * 0.5f;
  const uint32_t base = (color ? color : rgba(120, 140, 150)) | 0xFF000000u;
  const Ramp R = ramp(base, 0.9f), Bel = ramp(mix(base, rgba(236, 230, 210), 0.65f), 0.5f);
  const bool spots = ((h >> 7) & 3) == 0, stripe = ((h >> 9) & 3) == 1;
  // the tail: a forked fan behind the body
  for (int y = -6; y <= 6; y++)
    for (int x = 0; x < 6; x++) {
      const float fork = std::fabs((float)y) / 6.0f;
      if (x > 1 + fork * 5.0f || std::fabs((float)y) > 1.5f + x * 1.0f) continue;
      c.set((int)(cx - len) - x + 1, (int)cy + y, R[(y < 0 ? 2 : 1) + (x == 0 ? 0 : 0)]);
    }
  // the body: a lit spindle, dark back, pale belly
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const float u = (x + 0.5f - cx) / len;
      if (u < -1 || u > 1) continue;
      const float prof = std::sqrt(std::max(0.0f, 1.0f - u * u)) * (u > 0 ? 1.0f - 0.15f * u : 1.0f - 0.35f * (-u) * (-u));
      const float v = (y + 0.5f - cy) / (dep * prof + 0.01f);
      if (v < -1 || v > 1 || prof < 0.08f) continue;
      const float l = lightAt(u * 0.5f, v * 0.9f);
      int k = lightIndex(l, x, y, 0.12f);
      uint32_t col = v > 0.25f ? Bel[std::min(4, k + 1)] : R[k];
      if (stripe && std::fabs(v - 0.05f) < 0.14f) col = darken(col, 0.4f);
      if (spots && v < 0.2f && hash3(x / 2, y / 2, h) % 7 == 0) col = darken(col, 0.6f);
      if (((x + (y & 1)) % 3 == 0) && v < 0.3f) col = mix(col, R[4], 0.18f);   // scales
      c.set(x, y, col);
    }
  // fins: the dorsal, the pectoral; the gill line and the eye
  for (int i = 0; i < 6; i++) c.set((int)(cx - 2 + i), (int)(cy - dep - 1 + (i > 3 ? 1 : 0)), R[1]);
  for (int i = 0; i < 4; i++) c.set((int)(cx - 1 + i), (int)(cy - dep - 2), R[0]);
  for (int i = 0; i < 3; i++) c.set((int)(cx + 2 + i), (int)(cy + dep * 0.55f + i * 0.5f), Bel[1]);
  for (int y = -2; y <= 2; y++) c.set((int)(cx + len * 0.55f), (int)cy + y, R[0]);
  c.set((int)(cx + len * 0.74f), (int)(cy - dep * 0.25f), kInk);
  c.set((int)(cx + len * 0.74f) - 1, (int)(cy - dep * 0.25f) - 1, kWhite);
  outline(c, 0.9f);
  if (rarity >= 3) {   // a legendary fish gleams: a gold rim and a few glints
    Canvas g2 = c;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++)
        if (!chA(g2.get(x, y)) && (chA(g2.get(x + 1, y)) || chA(g2.get(x - 1, y)) || chA(g2.get(x, y + 1)) || chA(g2.get(x, y - 1)))) c.set(x, y, kGold[3]);
    c.set(4, 3, kGold[4]); c.set(W - 5, H - 4, kGold[4]); c.set(W / 2, 1, kGold[4]);
  }
  return c;
}

// ---------------------------------------------------------------- a kraken's tentacle
Canvas seaTentacleSheet() {
  const int W = SEA_TENTACLE_W, H = SEA_TENTACLE_H, N = 8;
  Canvas sheet(W * N, H);
  const Ramp Sk = ramp5(rgba(46, 20, 44), rgba(86, 34, 60), rgba(132, 52, 70), rgba(176, 84, 86), rgba(214, 132, 116));
  const Ramp Su = ramp5(rgba(150, 106, 112), rgba(190, 146, 140), rgba(222, 186, 170), rgba(240, 214, 196), rgba(252, 238, 224));
  for (int f = 0; f < N; f++) {
    Canvas c(W, H);
    // the arm's spine: from the waterline (bottom centre) up, swaying; reared back on the wind-up, struck down on the slam
    // the spine is walked by its bending angle: up from the water, an S-sway, the tip hooked over in a curl
    float th0 = -1.5708f, bend = std::sin(f * 1.5708f) * 0.5f, curl = 2.6f, Lg = H - 12.0f;
    if (f == 4) { th0 = -1.85f; bend = -0.3f; curl = 3.2f; Lg = H - 8.0f; }   // reared back
    if (f == 5) { th0 = -1.3f; bend = 1.3f; curl = 0.7f; Lg = H - 10.0f; }    // slammed over and down
    if (f == 6) { th0 = -1.7f; bend = -0.6f; curl = 1.6f; Lg = H - 12.0f; }
    if (f == 7) { th0 = -0.35f; bend = 0.2f; curl = 0.8f; Lg = H * 0.55f; }   // limp along the water
    const int S = 60;
    float x = W * 0.38f, y = H - 1.0f;
    for (int i = 0; i <= S; i++) {
      const float t = (float)i / S;
      const float th = th0 + bend * std::sin(t * 3.14159f) + curl * std::pow(t, 3.0f);
      if (i > 0) { x += std::cos(th) * Lg / S; y += std::sin(th) * Lg / S; }
      const float r = 4.2f * (1.0f - t * 0.84f) + 0.5f;
      for (int dy = -(int)r - 1; dy <= (int)r + 1; dy++)
        for (int dx = -(int)r - 1; dx <= (int)r + 1; dx++) {
          const float d = std::sqrt((float)(dx * dx + dy * dy));
          if (d > r) continue;
          const float nx = dx / std::max(0.5f, r), ny = dy / std::max(0.5f, r);
          const int px = (int)std::floor(x + dx), py = (int)std::floor(y + dy);
          uint32_t col = Sk[lightIndex(lightAt(nx, ny * 0.6f), px, py, 0.1f)];
          // the suckers along the inner (right) side: pale rings
          if (nx > 0.35f && ((int)(t * 26) % 2 == 0) && d > r * 0.45f) col = Su[2 + (ny < 0 ? 1 : 0)];
          if (f == 6) col = mix(col, rgba(255, 230, 220), 0.25f);
          c.set(px, py, col);
        }
    }
    // water streaming off it
    if (f < 4 || f == 4) for (int k = 0; k < 4; k++) c.set((int)(W * 0.5f - 3 + k * 2 + (f & 1)), H - 3 - ((k + f) % 3) * 4, rgba(220, 240, 250, 200));
    outline(c, 0.9f);
    place(sheet, c, f, 0);
  }
  return sheet;
}

}  // namespace art
