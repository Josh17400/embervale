// The archetype priors (VISION_PLAN 5.3, owner 15.6 / 15.11 / 15.12; M3, CULTURE lane). Each archetype is a set of
// DISTRIBUTIONS the generator draws inside: weighted discrete choices (roof forms, walls, cuts, helms...) and ranged
// continuous dials (pitch, eaves, wall height, tempo...). Atlas::make calls applyPriors with the family's seed, so two
// families of one archetype are related but never identical; the maximin in culture.cpp keeps neighbours apart.
//
// Palettes: every archetype has three hand-tuned SCHEMES (a cohesive set of roof / wall / trim / accent / cloth hues).
// A culture takes one, jittered, and its roof and wall colours are the material's natural colour pulled toward the
// scheme's hue, so a turf roof stays turf-green and a slate roof stays slate, but a whole town shares one light.
//
// Pure integer maths (a splitmix stream); nothing here reads libm.
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"

namespace cult {
namespace detail {

// ------------------------------------------------------------------ small helpers (shared with culture.cpp by name)
uint64_t smix(uint64_t z) {
  z += 0x9E3779B97F4A7C15ull;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

namespace {
uint32_t rgb(int r, int g, int b) { return (uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16 | 0xFF000000u; }
int cr(uint32_t c) { return (int)(c & 255); }
int cg(uint32_t c) { return (int)((c >> 8) & 255); }
int cb(uint32_t c) { return (int)((c >> 16) & 255); }
int clamp8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
// a + (b - a) * t / 256
uint32_t mixC(uint32_t a, uint32_t b, int t) {
  return rgb(cr(a) + (cr(b) - cr(a)) * t / 256, cg(a) + (cg(b) - cg(a)) * t / 256, cb(a) + (cb(b) - cb(a)) * t / 256);
}

struct Pk {
  uint64_t s;
  explicit Pk(uint64_t seed) : s(seed ? seed : 0x1234567ull) {}
  uint32_t next() { s = smix(s); return (uint32_t)(s >> 32); }
  int pick(int n) { return n <= 1 ? 0 : (int)(next() % (uint32_t)n); }
  bool chance(int p256) { return (int)(next() & 255) < p256; }
  int range(int lo, int hi) { return hi <= lo ? lo : lo + pick(hi - lo + 1); }
  // weighted choice among options (weights > 0)
  template <class T>
  T w(std::initializer_list<T> opts, std::initializer_list<int> wts) {
    int sum = 0;
    for (int x : wts) sum += x;
    int r = sum > 0 ? pick(sum) : 0;
    auto o = opts.begin();
    for (int x : wts) {
      if (r < x) return *o;
      r -= x;
      ++o;
    }
    return *opts.begin();
  }
  // a colour jittered by up to pct % per channel (brightness shared, a little per-channel hue drift)
  uint32_t jit(uint32_t c, int pct) {
    const int k = 1000 + (range(-pct * 10, pct * 10));
    const int dr = range(-pct, pct), dg = range(-pct, pct), db = range(-pct, pct);
    return rgb(clamp8(cr(c) * k / 1000 + dr), clamp8(cg(c) * k / 1000 + dg), clamp8(cb(c) * k / 1000 + db));
  }
};

std::vector<std::string> split(const char* s) {
  std::vector<std::string> v;
  std::string cur;
  for (const char* p = s; ; p++) {
    if (*p == ',' || *p == 0) {
      v.push_back(cur);
      cur.clear();
      if (!*p) break;
    } else {
      cur += *p;
    }
  }
  return v;
}
// keep a random subset of a weighted list: drop up to `drop` entries (never the first `keep`), then let a couple of
// neighbours swap places (the weights are by order, so this shifts the family's sound inside its archetype)
std::vector<std::string> mutateList(std::vector<std::string> v, Pk& p, int drop, int keep, int swaps) {
  for (int d = 0; d < drop && (int)v.size() > keep + 2; d++) v.erase(v.begin() + keep + p.pick((int)v.size() - keep));
  for (int k = 0; k < swaps && v.size() > 2; k++) {
    const int i = p.pick((int)v.size() - 1);
    std::swap(v[(size_t)i], v[(size_t)i + 1]);
  }
  return v;
}

// ------------------------------------------------------------------ materials' natural colours
uint32_t roofNatural(art::RoofMat m) {
  using art::RoofMat;
  switch (m) {
    case RoofMat::Thatch: return rgb(190, 156, 92);
    case RoofMat::Shingle: return rgb(112, 80, 58);
    case RoofMat::Slate: return rgb(86, 94, 112);
    case RoofMat::ClayTile: return rgb(174, 86, 58);
    case RoofMat::Turf: return rgb(92, 122, 60);
    case RoofMat::Adobe: return rgb(198, 160, 112);
    case RoofMat::Copper: return rgb(84, 156, 136);
    case RoofMat::Palm: return rgb(178, 150, 88);
    case RoofMat::Bark: return rgb(104, 78, 58);
    case RoofMat::Felt: return rgb(222, 210, 186);
    case RoofMat::GlazedTile: return rgb(52, 136, 122);
    case RoofMat::Leaf: return rgb(82, 128, 62);
    default: return rgb(112, 80, 58);
  }
}
uint32_t wallNatural(art::WallMat m) {
  using art::WallMat;
  switch (m) {
    case WallMat::Timber: return rgb(222, 210, 184);   // the infill between the beams
    case WallMat::Plaster: return rgb(230, 224, 208);
    case WallMat::Stone: return rgb(138, 134, 126);
    case WallMat::Brick: return rgb(156, 78, 58);
    case WallMat::Log: return rgb(120, 86, 56);
    case WallMat::Adobe: return rgb(210, 170, 120);
    case WallMat::Rubble: return rgb(128, 122, 110);
    case WallMat::Plank: return rgb(132, 102, 72);
    case WallMat::Wattle: return rgb(164, 138, 100);
    case WallMat::Felt: return rgb(226, 216, 194);
    case WallMat::Ashlar: return rgb(220, 212, 192);
    case WallMat::Living: return rgb(146, 126, 90);   // grown timber: pale, silvery-brown (not the dark of tarred logs)
    default: return rgb(222, 210, 184);
  }
}
}  // namespace

uint32_t roofColourFor(art::RoofMat m, uint32_t hue) { return mixC(roofNatural(m), hue, 96); }
uint32_t wallColourFor(art::WallMat m, uint32_t hue) {
  // painted / limed surfaces take the scheme's colour strongly; raw materials keep most of their own
  const bool painted = m == art::WallMat::Plaster || m == art::WallMat::Timber || m == art::WallMat::Adobe ||
                       m == art::WallMat::Ashlar || m == art::WallMat::Felt;
  return mixC(wallNatural(m), hue, painted ? 150 : 90);
}

namespace {
// ------------------------------------------------------------------ palettes
struct Scheme {
  uint32_t roof, wall, trim, accent, alt;
  uint32_t cloth[6];
};
#define C3(r, g, b) rgb(r, g, b)
const Scheme kSchemes[(int)Archetype::COUNT][3] = {
    {   // Fjordfolk: tarred timber, turf, oxblood and woad
     {C3(70, 62, 50), C3(88, 64, 46), C3(52, 40, 32), C3(146, 50, 38), C3(214, 196, 150),
      {C3(132, 128, 118), C3(146, 58, 44), C3(66, 86, 124), C3(204, 190, 160), C3(92, 104, 72), C3(112, 82, 58)}},
     {C3(96, 110, 96), C3(150, 132, 106), C3(74, 60, 48), C3(58, 88, 140), C3(226, 220, 200),
      {C3(98, 110, 130), C3(186, 176, 150), C3(150, 70, 52), C3(70, 74, 82), C3(160, 136, 90), C3(210, 204, 190)}},
     {C3(84, 78, 60), C3(104, 78, 54), C3(40, 34, 30), C3(196, 146, 58), C3(128, 48, 40),
      {C3(170, 150, 110), C3(110, 60, 48), C3(84, 96, 110), C3(142, 140, 128), C3(60, 58, 56), C3(196, 170, 120)}}},
    {   // Highland: grey stone, golden thatch, tartan
     {C3(176, 142, 84), C3(132, 128, 118), C3(70, 58, 46), C3(40, 90, 70), C3(150, 40, 44),
      {C3(46, 84, 64), C3(40, 56, 96), C3(150, 44, 46), C3(120, 110, 92), C3(90, 70, 100), C3(190, 176, 140)}},
     {C3(110, 104, 96), C3(150, 140, 124), C3(60, 52, 44), C3(36, 62, 112), C3(190, 150, 70),
      {C3(36, 58, 104), C3(60, 96, 72), C3(170, 150, 96), C3(120, 60, 52), C3(84, 82, 90), C3(200, 190, 160)}},
     {C3(150, 122, 80), C3(118, 112, 104), C3(52, 44, 38), C3(120, 40, 60), C3(210, 190, 130),
      {C3(110, 46, 60), C3(70, 90, 60), C3(150, 130, 100), C3(48, 48, 60), C3(170, 100, 60), C3(186, 178, 160)}}},
    {   // Heartland: whitewash, dark timber, red-brown roofs, heraldic blue and red
     {C3(128, 74, 54), C3(232, 222, 196), C3(78, 56, 40), C3(44, 70, 140), C3(180, 40, 40),
      {C3(60, 80, 140), C3(170, 50, 46), C3(120, 100, 70), C3(220, 210, 180), C3(60, 100, 60), C3(150, 120, 60)}},
     {C3(96, 84, 72), C3(226, 206, 160), C3(66, 50, 38), C3(40, 110, 70), C3(210, 170, 70),
      {C3(70, 110, 70), C3(170, 140, 80), C3(110, 70, 50), C3(210, 196, 170), C3(80, 80, 120), C3(160, 60, 50)}},
     {C3(150, 96, 60), C3(230, 196, 186), C3(88, 60, 44), C3(130, 40, 50), C3(230, 220, 190),
      {C3(140, 50, 60), C3(80, 90, 110), C3(200, 180, 140), C3(100, 80, 60), C3(60, 90, 110), C3(190, 150, 90)}}},
    {   // Imperial: cream ashlar, terracotta, purple and gold
     {C3(190, 92, 56), C3(230, 220, 196), C3(150, 120, 80), C3(120, 40, 110), C3(212, 172, 60),
      {C3(236, 230, 214), C3(150, 40, 46), C3(110, 46, 110), C3(212, 172, 72), C3(120, 100, 80), C3(60, 60, 90)}},
     {C3(178, 100, 70), C3(238, 232, 220), C3(120, 96, 70), C3(160, 36, 40), C3(230, 200, 110),
      {C3(230, 226, 210), C3(170, 40, 40), C3(200, 160, 60), C3(70, 70, 110), C3(150, 140, 120), C3(90, 40, 60)}},
     {C3(200, 120, 70), C3(220, 196, 160), C3(110, 80, 60), C3(40, 90, 120), C3(220, 190, 120),
      {C3(220, 210, 190), C3(40, 90, 130), C3(180, 140, 70), C3(130, 50, 60), C3(100, 110, 90), C3(240, 236, 226)}}},
    {   // Dune: sand and ochre, turquoise and lapis glaze, saffron and indigo
     {C3(210, 170, 120), C3(216, 176, 124), C3(140, 100, 64), C3(40, 140, 150), C3(46, 64, 140),
      {C3(230, 220, 196), C3(50, 60, 120), C3(220, 160, 50), C3(170, 60, 40), C3(40, 120, 120), C3(190, 150, 100)}},
     {C3(200, 140, 100), C3(226, 190, 150), C3(120, 80, 56), C3(30, 90, 160), C3(220, 180, 80),
      {C3(240, 236, 220), C3(30, 70, 140), C3(200, 120, 40), C3(140, 40, 60), C3(110, 140, 90), C3(210, 180, 140)}},
     {C3(180, 150, 110), C3(198, 150, 104), C3(100, 70, 50), C3(180, 70, 40), C3(40, 140, 130),
      {C3(220, 200, 170), C3(160, 60, 40), C3(60, 50, 90), C3(230, 190, 90), C3(50, 110, 100), C3(120, 80, 60)}}},
    {   // Steppe: off-white felt, saffron, red and sky blue, horse-hide
     {C3(224, 212, 186), C3(210, 196, 170), C3(150, 60, 40), C3(40, 80, 160), C3(220, 160, 40),
      {C3(40, 70, 140), C3(170, 50, 40), C3(90, 60, 40), C3(200, 180, 140), C3(60, 110, 90), C3(220, 170, 60)}},
     {C3(200, 188, 164), C3(190, 170, 140), C3(40, 90, 150), C3(190, 50, 50), C3(240, 220, 180),
      {C3(150, 40, 40), C3(60, 90, 60), C3(120, 90, 60), C3(40, 50, 90), C3(210, 200, 170), C3(180, 120, 40)}},
     {C3(230, 220, 200), C3(160, 130, 100), C3(190, 130, 40), C3(60, 120, 110), C3(160, 40, 60),
      {C3(90, 40, 60), C3(190, 150, 60), C3(50, 100, 110), C3(140, 110, 80), C3(230, 220, 200), C3(60, 60, 70)}}},
    {   // Marsh: weathered grey wood, reed, olive and moss, faded indigo, rust
     {C3(170, 150, 96), C3(120, 110, 94), C3(70, 64, 52), C3(170, 90, 50), C3(80, 100, 120),
      {C3(110, 120, 80), C3(80, 90, 120), C3(180, 110, 70), C3(200, 190, 150), C3(130, 90, 60), C3(90, 110, 100)}},
     {C3(140, 130, 84), C3(140, 120, 92), C3(60, 56, 44), C3(60, 110, 90), C3(200, 170, 90),
      {C3(60, 100, 80), C3(180, 150, 80), C3(120, 70, 60), C3(200, 200, 180), C3(70, 70, 100), C3(150, 130, 90)}},
     {C3(150, 120, 80), C3(108, 96, 84), C3(50, 46, 40), C3(150, 60, 80), C3(220, 210, 170),
      {C3(150, 60, 80), C3(90, 110, 70), C3(60, 90, 110), C3(190, 170, 120), C3(110, 90, 70), C3(210, 140, 90)}}},
    {   // Jade: jade glaze, vermilion, white plaster, black lacquer, gold
     {C3(52, 130, 116), C3(232, 226, 212), C3(40, 34, 32), C3(186, 48, 36), C3(212, 170, 60),
      {C3(60, 120, 110), C3(190, 60, 40), C3(230, 220, 200), C3(40, 50, 70), C3(200, 160, 70), C3(130, 70, 110)}},
     {C3(70, 80, 100), C3(210, 190, 160), C3(120, 40, 30), C3(40, 110, 90), C3(230, 220, 200),
      {C3(50, 60, 100), C3(200, 190, 170), C3(150, 40, 40), C3(80, 120, 90), C3(40, 40, 44), C3(180, 140, 80)}},
     {C3(40, 90, 130), C3(236, 232, 224), C3(160, 50, 40), C3(220, 180, 70), C3(40, 40, 44),
      {C3(40, 80, 130), C3(200, 80, 60), C3(230, 226, 214), C3(100, 150, 120), C3(70, 40, 60), C3(190, 170, 120)}}},
    {   // River: red brick, dark slate, white trim, green shutters, ochre
     {C3(88, 90, 100), C3(150, 74, 56), C3(236, 232, 220), C3(40, 100, 70), C3(220, 170, 60),
      {C3(40, 50, 70), C3(230, 226, 210), C3(140, 60, 40), C3(200, 160, 60), C3(60, 100, 80), C3(110, 90, 70)}},
     {C3(170, 90, 60), C3(176, 110, 80), C3(60, 50, 44), C3(200, 120, 40), C3(60, 80, 130),
      {C3(200, 120, 40), C3(60, 70, 120), C3(220, 216, 200), C3(90, 60, 50), C3(150, 40, 40), C3(120, 130, 110)}},
     {C3(70, 74, 84), C3(120, 64, 52), C3(220, 214, 196), C3(40, 70, 120), C3(190, 40, 40),
      {C3(40, 70, 120), C3(230, 230, 220), C3(170, 50, 40), C3(60, 60, 60), C3(180, 150, 100), C3(90, 110, 80)}}},
    {   // Sun-temple: limestone, painted red and teal bands, gold, jungle green
     {C3(190, 170, 130), C3(226, 212, 180), C3(170, 50, 40), C3(40, 140, 130), C3(220, 180, 60),
      {C3(230, 220, 200), C3(190, 60, 40), C3(40, 130, 120), C3(230, 180, 60), C3(60, 100, 60), C3(120, 60, 100)}},
     {C3(170, 120, 80), C3(200, 170, 130), C3(60, 100, 80), C3(200, 80, 40), C3(60, 60, 140),
      {C3(200, 90, 40), C3(60, 60, 140), C3(220, 200, 160), C3(40, 110, 80), C3(180, 40, 60), C3(240, 210, 90)}},
     {C3(150, 150, 130), C3(214, 200, 170), C3(120, 40, 80), C3(230, 190, 60), C3(50, 120, 160),
      {C3(240, 200, 70), C3(50, 120, 160), C3(150, 40, 80), C3(220, 210, 190), C3(90, 140, 60), C3(180, 90, 50)}}},
    {   // Sylvan: moss and leaf, birch white, amber glow, russet
     {C3(82, 124, 64), C3(150, 132, 96), C3(70, 58, 40), C3(220, 170, 70), C3(190, 200, 180),
      {C3(70, 110, 60), C3(150, 90, 50), C3(190, 170, 110), C3(60, 80, 70), C3(210, 200, 170), C3(120, 60, 40)}},
     {C3(150, 110, 60), C3(180, 170, 150), C3(80, 70, 50), C3(170, 60, 40), C3(110, 150, 90),
      {C3(170, 90, 40), C3(90, 120, 70), C3(220, 190, 110), C3(110, 70, 50), C3(200, 200, 190), C3(60, 70, 60)}},
     {C3(60, 100, 80), C3(136, 124, 100), C3(52, 46, 36), C3(140, 170, 200), C3(200, 180, 120),
      {C3(60, 90, 80), C3(140, 160, 190), C3(190, 180, 150), C3(100, 80, 110), C3(70, 100, 60), C3(220, 210, 190)}}},
    {   // Starspire: white marble, deep blue and violet, silver and gold
     {C3(60, 70, 130), C3(240, 236, 228), C3(190, 190, 200), C3(212, 178, 80), C3(110, 80, 160),
      {C3(240, 236, 228), C3(60, 70, 140), C3(110, 80, 160), C3(200, 180, 110), C3(140, 170, 210), C3(60, 60, 80)}},
     {C3(90, 70, 130), C3(232, 228, 236), C3(200, 170, 90), C3(80, 140, 200), C3(230, 230, 240),
      {C3(230, 226, 240), C3(90, 70, 140), C3(80, 140, 200), C3(190, 170, 100), C3(60, 50, 90), C3(180, 190, 210)}},
     {C3(70, 120, 130), C3(236, 232, 214), C3(170, 180, 190), C3(60, 90, 160), C3(230, 200, 120),
      {C3(236, 232, 214), C3(60, 110, 130), C3(60, 80, 150), C3(220, 200, 140), C3(120, 140, 170), C3(90, 60, 90)}}},
};
const uint32_t kHair[(int)Archetype::COUNT][4] = {
    {C3(230, 204, 140), C3(196, 120, 64), C3(150, 110, 70), C3(232, 226, 200)},   // Fjordfolk
    {C3(150, 70, 40), C3(80, 52, 36), C3(40, 32, 30), C3(196, 120, 60)},          // Highland
    {C3(110, 76, 48), C3(60, 42, 30), C3(196, 160, 100), C3(150, 80, 40)},        // Heartland
    {C3(50, 36, 28), C3(90, 60, 40), C3(30, 26, 26), C3(130, 90, 56)},            // Imperial
    {C3(30, 24, 22), C3(60, 40, 30), C3(80, 56, 40), C3(40, 30, 28)},             // Dune
    {C3(24, 20, 20), C3(50, 36, 28), C3(70, 50, 36), C3(36, 30, 28)},             // Steppe
    {C3(40, 30, 26), C3(70, 50, 36), C3(110, 70, 44), C3(30, 26, 24)},            // Marsh
    {C3(24, 22, 24), C3(40, 32, 28), C3(30, 28, 30), C3(60, 44, 34)},             // Jade
    {C3(196, 170, 110), C3(130, 96, 60), C3(90, 64, 42), C3(220, 196, 140)},      // River
    {C3(24, 20, 18), C3(40, 30, 24), C3(30, 24, 22), C3(56, 40, 30)},             // SunTemple
    {C3(230, 210, 150), C3(210, 200, 190), C3(150, 70, 40), C3(40, 32, 30)},      // Sylvan
    {C3(236, 232, 220), C3(200, 204, 214), C3(30, 30, 40), C3(230, 200, 140)},    // Starspire
};
#undef C3

// art::Hair bits: Bald 1, Short 2, Long 4, Ponytail 8, Mohawk 16, Braids 32, Bun 64, Curls 128
enum : uint16_t { H_BALD = 1, H_SHORT = 2, H_LONG = 4, H_PONY = 8, H_MOHAWK = 16, H_BRAIDS = 32, H_BUN = 64, H_CURLS = 128 };
// Religion::domains bits
enum : uint16_t { D_SUN = 1, D_MOON = 2, D_SEA = 4, D_WAR = 8, D_HARVEST = 16, D_DEATH = 32, D_CRAFT = 64, D_WISDOM = 128,
                  D_STORM = 256, D_FOREST = 512, D_HEARTH = 1024, D_STARS = 2048 };

// the phonology, reagents and words of an archetype (strings; comma separated, weighted by order)
struct Tongue {
  const char *on, *nu, *co;
  uint8_t sylMin, sylMax, pattern;
  const char *place, *sufF, *sufM, *epithets;
  uint8_t joiner;
  const char* forbid;
  const char* reagents;   // exotic alloy reagents of these lands
  const char* roots;      // English-ish alloy roots this people would also use ("SUN", "RIME")
  const char* lore;       // alloy lore lines
};
// place suffixes beginning with '^' are prefixes ("^DUN" -> DUNCARRA; "^QASR " -> QASR AMUN)
const Tongue kTongue[(int)Archetype::COUNT] = {
    {"V,H,S,T,R,B,G,K,SK,SV,TH,BJ,FR,GR,ST,,EI", "A,E,O,I,U,Y,EI,AU,JA", "R,N,LD,RN,ND,K,LF,RG,ST,NG,LL,TH", 1, 2, 15,
     "HEIM,VIK,STAD,FJORD,HOLM,NES,BY,GARD,DAL", "A,RID,HILD,DIS,UN", ",R,ULF,AR,MUND", "THE BOLD,IRONSIDE,FAIRHAIR,THE GREY",
     0, "QXZC", "RIME SALT,BLACK SAND,WHALE OIL", "RIME,STORM,FROST,WOLF", "QUENCHED IN SNOWMELT UNDER THE AURORA"},
    {"D,GL,K,M,BR,T,C,F,L,R,DR,CR,GW,,", "A,AI,O,U,E,EA,IO,OI,AO", "N,CH,RR,G,CK,LL,NN,R,DH", 1, 2, 15,
     "MORE,^DUN,^GLEN,ACH,BRAE,^KIL,VAR,^INVER", "A,AG,ETH,INA", ",AN,OCH,AIG,ACH", "OF THE GLEN,THE RED,MOR,THE STEADFAST",
     0, "QXZJV", "PEAT ASH,HEATHER CHAR,BOG IRON", "PEAT,CRAG,THORN,STAG", "HAMMERED BY CLAN SMITHS WHERE THE PEAT FIRES NEVER DIE"},
    {"W,ST,B,C,H,M,N,SH,TH,GR,BR,D,L,R,AL", "E,A,I,O,U,EA,OO", "N,LL,RD,TH,ST,M,NT,CK,LD,RN", 1, 2, 7,
     "FORD,TON,BURY,WICK,LEY,HAM,FIELD,BROOK,STEAD,WELL", "A,WYN,ETH,ICE", ",RIC,WIN,ALD,ERT", "THE GOOD,THE WISE,OF THE VALE",
     0, "QXZJ", "BONE ASH,OAK CHAR,HOLY WATER", "CROWN,OAK,LION,DAWN", "FORGED FOR THE CROWN'S OWN KNIGHTS"},
    {"V,C,S,T,L,M,P,N,R,D,QU,AUR,,", "A,I,E,U,O,AE", "S,R,N,X,L,M", 2, 3, 3,
     "IUM,ARA,ONA,ENTUM,ICA,ANUM,ALIA,OPOL", "A,IA,INA,ILLA", "US,IUS,O,AN,IAN", "MAGNUS,THE JUST,PRIMUS",
     0, "KWYJZ", "CINNABAR,MARBLE LIME,VERMILION", "AUREL,LEGION,EAGLE,SOL", "CAST IN THE LEGION FOUNDRIES BY IMPERIAL DECREE"},
    {"Q,Z,KH,S,M,R,H,J,B,D,N,SH,T,F,,", "A,I,U,AA,A,AI", "R,N,D,SH,M,L,B,Z", 2, 2, 7,
     "^QASR ,^AIN ,^DAR ,^BAB ,ABAD,IYA,ARA", "A,IYA,AH,INA", ",IR,AN,UD,IM", "THE PATIENT,OF THE SANDS,AL-HAKIM",
     3, "VPXGCW", "DESERT GLASS,SALAMANDER ASH,DATE PALM CHAR", "SUN,SAND,DUNE,FALCON", "FOLDED A THOUSAND TIMES IN SALAMANDER ASH"},
    {"T,B,KH,S,Y,CH,M,Z,G,K,,", "A,U,O,E,I,UU,AA", "N,R,Y,GH,L,T,SH,Z,NG", 2, 2, 15,
     "TAU,BULAK,KENT,OOL,SU,TAI", "AI,GUL,A,ANA", "BEK,TAI,UR,,AN", "THE RIDER,SKYBORN,OF THE HORSE",
     0, "WVPFQ", "HORSE BONE,SKY STONE,TAMARISK ASH", "SKY,WIND,HORSE,ARROW", "BEATEN FROM STARS THAT FELL ON THE GRASS SEA"},
    {"M,W,L,P,N,B,K,T,H,NG,,", "O,U,A,EE,OO,I,E", ",,M,NG,L", 2, 3, 5,
     "OLO,UMA,FEN,MERE,WA,EE", "A,EE,ULA", ",O,UM,AL", "OF THE REEDS,THE QUIET,MUDFOOT",
     0, "QXZVJR", "BOG RESIN,EEL OIL,REED ASH", "REED,EEL,HERON,MIST", "TEMPERED IN BLACK BOG WATER AT THE NEW MOON"},
    {"J,SH,L,K,T,H,X,M,Y,Z,CH,,", "IA,AO,U,EN,AI,O,I,A,EI,UN", "N,NG,,", 2, 2, 7,
     "SHAN,LING,KOU,MEN,ZHOU,TAI", ",,", ",,", "OF THE NINE GATES,THE SERENE,JADE-HAND",
     0, "VRDBPF", "JADE DUST,LACQUER,CINNABAR", "JADE,CRANE,DRAGON,PEARL", "FOLDED WITH JADE DUST IN THE TERRACE FORGES"},
    {"V,B,R,D,G,M,W,Z,K,SL,ST,BR,H", "E,A,O,AU,OO,EE,IJ,UI", "L,RG,N,CK,NT,RS,LT,M,K", 1, 2, 7,
     "HAVEN,WIJK,DAM,DORP,HOVEN,MOND,LO", "A,JE,INE", ",ERT,OLD,EN", "THE PROSPEROUS,OF THE LOCKS,GOLDHAND",
     0, "QXCJ", "ALUM,RIVER CLAY,TIN SALT", "GUILD,LOCK,COIN,TIDE", "SMELTED UNDER GUILD SEAL IN THE RIVER FORGES"},
    {"T,X,CH,Y,TZ,K,P,M,N,H,TL,,", "A,I,O,U,E", ",,L,N,X,K", 2, 3, 5,
     "TLAN,PAN,KAL,IXA,TEK,COA", "IXA,EL,A", "AK,OL,,IN", "OF THE SUN,JAGUAR-HEART,THE RADIANT",
     0, "VRDGBFJ", "OBSIDIAN,GOLD DUST,COPAL RESIN", "SUN,JAGUAR,QUETZ,DAWN", "BLESSED ON THE TEMPLE STAIR AT NOON"},
    {"L,S,TH,F,N,R,M,,EL,AL,GW", "IA,AE,E,I,A,IE,EI", "L,N,TH,R,S,,", 2, 3, 15,
     "WEN,ADEL,IND,ARA,IEL,WYN,ODH", "IEL,WEN,A,ITH", "AS,OR,ION,", "LEAFSONG,THE GREEN,OF THE DEEP WOOD",
     0, "KQXZJBP", "AMBER,HEARTWOOD SAP,MOONMOSS", "LEAF,AMBER,BRIAR,STAG", "GROWN, NOT FORGED: SAP SET ROUND RIVER SILVER"},
    {"S,V,C,M,T,L,N,QU,EL,AR,IL,,", "A,E,IO,EA,I,AI", "N,R,S,TH,L,,", 2, 3, 15,
     "ISTA,IEL,ANTHE,ARION,ESSE,ALIS,OND", "IEL,ETH,A", "ION,AS,OR,", "STARBORN,THE LUMINOUS,OF THE SPIRE",
     0, "KQXZJBPW", "STARGLASS DUST,MOONWATER,SILVERED QUARTZ", "STAR,MOON,DAWN,SPIRE", "DRAWN FROM MOONWATER UNDER A CLEAR NIGHT SKY"},
};

// ------------------------------------------------------------------ one archetype's draws
void common(Culture& c, Pk& p, int scheme) {
  const int a = (int)c.archetype;
  const Scheme& S = kSchemes[a][scheme];
  // palette: jittered scheme (+-6 %), roof and wall from the materials pulled toward it
  const uint32_t roofHue = p.jit(S.roof, 6), wallHue = p.jit(S.wall, 6);
  c.arch.roofTint = roofColourFor(c.arch.roofMat, roofHue);
  c.arch.wallTint = wallColourFor(c.arch.wall, wallHue);
  c.arch.trimTint = p.jit(S.trim, 6);
  c.arch.accentTint = p.jit(S.accent, 7);
  c.arch.altTint = p.jit(S.alt, 7);
  for (int i = 0; i < 6; i++) c.dress.cloth[i] = p.jit(S.cloth[i], 6);
  // a family shuffles which of its cloths is the everyday one (cloth[0..1]: most common)
  if (p.chance(110)) std::swap(c.dress.cloth[0], c.dress.cloth[1 + p.pick(3)]);
  for (int i = 0; i < 4; i++) c.dress.hairCols[i] = p.jit(kHair[a][i], 5);
  c.dress.trim = p.chance(128) ? c.arch.accentTint : c.arch.altTint;
  // props share the palette
  c.props.culture = (uint8_t)(a + 1);
  c.props.awningA = c.arch.accentTint;
  c.props.awningB = p.chance(140) ? c.arch.altTint : c.dress.cloth[3];
  c.props.cloth = c.dress.cloth[1];
  c.props.wood = c.arch.trimTint;
  c.props.stone = mixC(wallColourFor(art::WallMat::Stone, wallHue), rgb(140, 136, 128), 128);
  c.props.metal = c.arms.metal;
  c.props.fence = c.town.fence;
  c.town.centre = c.props.centre;
  c.arch.culture = (uint8_t)(a + 1);
  c.arch.furniture = c.customs.furniture;
  c.arms.cloth = c.dress.cloth[p.pick(3)];
  if (!c.arms.plume) c.arms.plume = c.arch.accentTint;

  // phonology
  const Tongue& T = kTongue[a];
  Phonology& ph = c.phon;
  ph.onsets = mutateList(split(T.on), p, 2, 3, 3);
  ph.nuclei = mutateList(split(T.nu), p, 1, 2, 2);
  ph.codas = mutateList(split(T.co), p, 1, 2, 2);
  ph.sylMin = T.sylMin;
  ph.sylMax = T.sylMax;
  ph.pattern = T.pattern;
  ph.placeSuffix = mutateList(split(T.place), p, 2, 2, 2);
  ph.personSuffixF = split(T.sufF);
  ph.personSuffixM = split(T.sufM);
  ph.epithets = split(T.epithets);
  ph.joiner = T.joiner;
  ph.forbid = T.forbid;
}

void fjordfolk(Culture& c, Pk& p, int& scheme) {
  using namespace art;
  auto& A = c.arch;
  A.roof = p.w({RoofShape::Turf, RoofShape::Steep, RoofShape::Gable}, {5, 4, 1});
  c.altRoof = A.roof == RoofShape::Turf ? RoofShape::Steep : RoofShape::Turf;
  A.roofMat = A.roof == RoofShape::Turf ? RoofMat::Turf : p.w({RoofMat::Shingle, RoofMat::Turf, RoofMat::Bark}, {5, 3, 2});
  A.wall = p.w({WallMat::Log, WallMat::Plank, WallMat::Timber}, {5, 4, 1});
  A.window = p.w({WindowShape::Slit, WindowShape::Square}, {3, 2});
  A.door = p.w({DoorShape::Plank, DoorShape::Arched}, {3, 1});
  A.foundation = p.w({Foundation::Plinth, Foundation::None}, {3, 2});
  A.ornament = ORN_CHIMNEY | (p.chance(200) ? ORN_DRAGON_HEADS : 0) | (p.chance(140) ? ORN_CARVED_RIDGE : 0) |
               (p.chance(80) ? ORN_ROOF_STONES : 0) | (p.chance(60) ? ORN_PAINTED_BANDS : 0);
  A.pitch = (uint8_t)p.range(3, 4); A.eave = (uint8_t)p.range(1, 3); A.wallH = (uint8_t)p.range(60, 90);
  A.chimneys = 1; A.smoke = true; A.shutters = p.chance(90); A.weather = (uint8_t)p.range(1, 2);
  c.customs.furniture = Furniture::Benches;
  c.props.well = 1; c.props.lamp = (uint8_t)p.w({5, 2}, {3, 2}); c.props.bench = 3;
  c.props.centre = (uint8_t)p.w({6, 4, 1}, {4, 3, 1}); c.props.awning = (uint8_t)p.w({5, 1}, {3, 2});
  c.town.layout = p.w({Layout::Linear, Layout::Organic}, {3, 2});
  c.town.altLayout = c.town.layout == Layout::Linear ? Layout::Organic : Layout::Linear;
  c.town.wall = CityWall::Palisade; c.town.fence = p.w({Fence::Wattle, Fence::StoneDyke}, {3, 2});
  c.town.density = (uint8_t)p.range(70, 110); c.town.trees = (uint8_t)p.range(40, 90); c.town.paving = (uint8_t)p.w({2, 4}, {3, 2});
  auto& D = c.dress;
  D.cutM = p.w({Cut::Tunic, Cut::Coat}, {4, 1}); D.cutF = p.w({Cut::Robe, Cut::Gown}, {4, 1});
  D.head[0] = Headwear::FurHat; D.head[1] = Headwear::Hood; D.head[2] = Headwear::None; D.headP = (uint8_t)p.range(110, 160);
  D.pattern = p.w({Pattern::BorderTrim, Pattern::Plain}, {3, 1});
  D.skinLo = 0; D.skinHi = (uint8_t)p.range(2, 4); D.hairStyles = H_LONG | H_BRAIDS | H_PONY | H_SHORT | H_BALD;
  D.beardP = (uint8_t)p.range(180, 230); D.jewellery = (uint8_t)p.range(60, 110); D.facePaint = 0;
  auto& R = c.arms;
  R.helm[0] = p.w({HelmForm::Spangen, HelmForm::Nasal}, {3, 1}); R.helm[1] = HelmForm::Nasal;
  R.body[0] = BodyArm::Mail; R.body[1] = p.w({BodyArm::Leather, BodyArm::Padded}, {3, 1});
  R.shield = ShieldForm::Round; R.blade = p.w({Blade::Broad, Blade::Straight}, {3, 1}); R.polearm = Polearm::Spear; R.bow = BowKind::Self;
  R.metal = rgb(150, 150, 156); R.leather = rgb(96, 66, 44);
  R.ornament = ARM_FUR_TRIM | ARM_ETCHING | (p.chance(100) ? ARM_RIVETS : 0);
  R.pauldron = 1; R.skirt = 2; R.crest = 0; R.cape = 2;
  auto& M = c.music;
  M.scale = p.w({Scale::Dorian, Scale::Minor}, {3, 1}); M.bpm = (uint8_t)p.range(70, 84); M.meter = (uint8_t)p.w({3, 4}, {3, 1});
  M.lead = p.w({LeadInst::Horn, LeadInst::Fiddle}, {4, 1}); M.pad = PadInst::Drone; M.bass = BassInst::Drone;
  M.perc = PercKind::Bodhran; M.swing = 0; M.ornament = (uint8_t)p.range(2, 5); M.drone = (uint8_t)p.range(9, 13);
  c.faith.kind = 0; c.faith.domains = D_STORM | D_SEA | D_WAR | D_DEATH; c.faith.shrineForm = 1; c.faith.burial = (uint8_t)p.w({2, 4}, {1, 1});
  c.customs.staple[0] = 1; c.customs.staple[1] = 0; c.customs.staple[2] = 4; c.customs.drink = 1;
  c.customs.lawStrict = (uint8_t)p.range(90, 140); c.customs.xenophobia = (uint8_t)p.range(60, 120); c.customs.festivalMonth = 11;
  c.heraldry.shape = 1;
  scheme = p.pick(3);
  const uint8_t v[8] = {220, 110, 120, 60, 230, 150, 80, 200};
  for (int i = 0; i < 8; i++) c.values[i] = v[i];
}

void highland(Culture& c, Pk& p, int& scheme) {
  using namespace art;
  auto& A = c.arch;
  A.roof = p.w({RoofShape::Gable, RoofShape::Steep, RoofShape::Hip}, {6, 3, 1});
  c.altRoof = A.roof == RoofShape::Gable ? RoofShape::Steep : RoofShape::Gable;
  A.roofMat = p.w({RoofMat::Thatch, RoofMat::Slate}, {6, 4});   // (turf roofs are the fjordfolk's signature)
  A.wall = p.w({WallMat::Rubble, WallMat::Stone, WallMat::Wattle}, {6, 3, 1});
  A.window = WindowShape::Square; A.door = DoorShape::Plank; A.foundation = Foundation::None;
  A.ornament = ORN_CHIMNEY | (p.chance(170) ? ORN_ROOF_STONES : 0) | (p.chance(90) ? ORN_SHUTTERS : 0) | (p.chance(70) ? ORN_FLOWERBOX : 0);
  A.pitch = (uint8_t)p.range(3, 4); A.eave = (uint8_t)p.range(0, 1); A.wallH = (uint8_t)p.range(70, 95);
  A.chimneys = (uint8_t)p.range(1, 2); A.smoke = true; A.shutters = p.chance(90); A.weather = (uint8_t)p.range(1, 3);
  c.customs.furniture = Furniture::Stools;
  c.props.well = (uint8_t)p.w({1, 0}, {3, 1}); c.props.lamp = 5; c.props.bench = 1;
  c.props.centre = (uint8_t)p.w({6, 1, 2}, {4, 2, 1}); c.props.awning = 1;
  c.town.layout = Layout::Compound; c.town.altLayout = Layout::Organic;
  c.town.wall = p.w({CityWall::Rampart, CityWall::Stone}, {3, 2}); c.town.fence = Fence::StoneDyke;
  c.town.density = (uint8_t)p.range(60, 100); c.town.trees = (uint8_t)p.range(60, 110); c.town.paving = 2;
  auto& D = c.dress;
  D.cutM = Cut::Kilt; D.cutF = p.w({Cut::Robe, Cut::Kilt}, {3, 1});
  D.head[0] = Headwear::Hood; D.head[1] = Headwear::Cap; D.head[2] = Headwear::None; D.headP = (uint8_t)p.range(90, 140);
  D.pattern = Pattern::Checks;
  D.skinLo = 0; D.skinHi = (uint8_t)p.range(3, 4); D.hairStyles = H_CURLS | H_LONG | H_BRAIDS | H_SHORT | H_PONY;
  D.beardP = (uint8_t)p.range(150, 200); D.jewellery = (uint8_t)p.range(30, 70); D.facePaint = p.chance(40) ? 3 : 0;
  auto& R = c.arms;
  R.helm[0] = p.w({HelmForm::Kettle, HelmForm::Nasal}, {3, 1}); R.helm[1] = HelmForm::Nasal;
  R.body[0] = p.w({BodyArm::Padded, BodyArm::Mail}, {2, 1}); R.body[1] = BodyArm::Leather;
  R.shield = ShieldForm::Buckler; R.blade = Blade::Broad; R.polearm = p.w({Polearm::Halberd, Polearm::Spear}, {2, 1}); R.bow = BowKind::Longbow;
  R.metal = rgb(140, 140, 146); R.leather = rgb(90, 60, 40);
  R.ornament = ARM_STUDS | (p.chance(120) ? ARM_FUR_TRIM : 0);
  R.pauldron = 0; R.skirt = 1; R.crest = 0; R.cape = 3;
  auto& M = c.music;
  M.scale = p.w({Scale::Mixolydian, Scale::Dorian}, {3, 1}); M.bpm = (uint8_t)p.range(92, 108); M.meter = 6;
  M.lead = LeadInst::Pipes; M.pad = PadInst::Drone; M.bass = BassInst::Drone;
  M.perc = PercKind::Bodhran; M.swing = 0; M.ornament = (uint8_t)p.range(9, 13); M.drone = (uint8_t)p.range(11, 15);
  c.faith.kind = 2; c.faith.domains = D_DEATH | D_HEARTH | D_STORM | D_WAR; c.faith.shrineForm = 1; c.faith.burial = 1;
  c.customs.staple[0] = 4; c.customs.staple[1] = 0; c.customs.staple[2] = 8; c.customs.drink = 0;
  c.customs.lawStrict = (uint8_t)p.range(80, 130); c.customs.xenophobia = (uint8_t)p.range(90, 150); c.customs.festivalMonth = 10;
  c.heraldry.shape = 2;
  scheme = p.pick(3);
  const uint8_t v[8] = {200, 80, 120, 70, 60, 90, 170, 230};
  for (int i = 0; i < 8; i++) c.values[i] = v[i];
}

void heartland(Culture& c, Pk& p, int& scheme) {
  using namespace art;
  auto& A = c.arch;
  A.roof = p.w({RoofShape::Hip, RoofShape::Gable}, {1, 1});
  c.altRoof = A.roof == RoofShape::Hip ? RoofShape::Gable : RoofShape::Hip;
  A.roofMat = p.w({RoofMat::Shingle, RoofMat::Thatch, RoofMat::ClayTile}, {4, 3, 2});
  A.wall = p.w({WallMat::Timber, WallMat::Plaster}, {3, 1});
  A.window = p.w({WindowShape::Square, WindowShape::Lattice}, {2, 1});
  A.door = p.w({DoorShape::Plank, DoorShape::Arched}, {3, 1});
  A.foundation = p.w({Foundation::None, Foundation::Plinth}, {2, 1});
  A.ornament = ORN_CHIMNEY | ORN_SHUTTERS | (p.chance(150) ? ORN_FLOWERBOX : 0) | (p.chance(60) ? ORN_PORCH_COLUMNS : 0);
  A.pitch = (uint8_t)p.range(2, 3); A.eave = (uint8_t)p.range(0, 1); A.wallH = (uint8_t)p.range(95, 112);
  A.chimneys = (uint8_t)p.range(1, 2); A.smoke = false; A.shutters = true; A.weather = (uint8_t)p.range(0, 2);
  c.customs.furniture = Furniture::Chairs;
  c.props.well = 0; c.props.lamp = 0; c.props.bench = 0; c.props.centre = (uint8_t)p.w({0, 1, 2}, {3, 1, 1}); c.props.awning = 0;
  c.town.layout = Layout::Organic; c.town.altLayout = Layout::Linear;
  c.town.wall = CityWall::Stone; c.town.fence = p.w({Fence::Picket, Fence::Wattle}, {3, 1});
  c.town.density = (uint8_t)p.range(110, 150); c.town.trees = (uint8_t)p.range(110, 160); c.town.paving = 0;
  auto& D = c.dress;
  D.cutM = Cut::Tunic; D.cutF = p.w({Cut::Robe, Cut::Gown}, {3, 1});
  D.head[0] = Headwear::None; D.head[1] = Headwear::Hood; D.head[2] = Headwear::Cap; D.headP = (uint8_t)p.range(60, 100);
  D.pattern = p.w({Pattern::Plain, Pattern::BorderTrim}, {2, 1});
  D.skinLo = 1; D.skinHi = (uint8_t)p.range(5, 7); D.hairStyles = 0xFF & ~H_MOHAWK;
  D.beardP = (uint8_t)p.range(100, 140); D.jewellery = (uint8_t)p.range(20, 60); D.facePaint = 0;
  auto& R = c.arms;
  R.helm[0] = p.w({HelmForm::Nasal, HelmForm::GreatHelm}, {3, 2}); R.helm[1] = HelmForm::Kettle;
  R.body[0] = p.w({BodyArm::Mail, BodyArm::Plate}, {3, 1}); R.body[1] = BodyArm::Padded;
  R.shield = p.w({ShieldForm::Kite, ShieldForm::Heater}, {3, 2}); R.blade = Blade::Straight; R.polearm = Polearm::Halberd;
  R.bow = p.w({BowKind::Longbow, BowKind::Crossbow}, {3, 1});
  R.metal = rgb(176, 178, 186); R.leather = rgb(110, 76, 50);
  R.ornament = ARM_RIVETS | (p.chance(90) ? ARM_PLUMES : 0);
  R.pauldron = 2; R.skirt = 1; R.crest = 0; R.cape = 1;
  auto& M = c.music;
  M.scale = p.w({Scale::Major, Scale::Mixolydian}, {4, 1}); M.bpm = (uint8_t)p.range(84, 100); M.meter = (uint8_t)p.w({6, 4}, {1, 1});
  M.lead = p.w({LeadInst::Lute, LeadInst::Flute}, {3, 2}); M.pad = PadInst::Strings; M.bass = BassInst::Plucked;
  M.perc = PercKind::Frame; M.swing = 0; M.ornament = (uint8_t)p.range(3, 6); M.drone = (uint8_t)p.range(0, 2);
  c.faith.kind = 1; c.faith.domains = D_SUN | D_HARVEST | D_HEARTH | D_WISDOM; c.faith.shrineForm = 0; c.faith.burial = 0;
  c.customs.staple[0] = 0; c.customs.staple[1] = 5; c.customs.staple[2] = 8; c.customs.drink = 0;
  c.customs.lawStrict = (uint8_t)p.range(120, 170); c.customs.xenophobia = (uint8_t)p.range(40, 80); c.customs.festivalMonth = 8;
  c.heraldry.shape = 0;
  scheme = p.pick(3);
  const uint8_t v[8] = {140, 150, 160, 110, 80, 130, 60, 170};
  for (int i = 0; i < 8; i++) c.values[i] = v[i];
}

void imperial(Culture& c, Pk& p, int& scheme) {
  using namespace art;
  auto& A = c.arch;
  A.roof = p.w({RoofShape::Hip, RoofShape::FlatParapet}, {5, 2});
  c.altRoof = p.w({RoofShape::Dome, RoofShape::Gable}, {1, 1});
  A.roofMat = RoofMat::ClayTile;
  A.wall = p.w({WallMat::Ashlar, WallMat::Plaster}, {1, 1});
  A.window = WindowShape::Arched; A.door = p.w({DoorShape::Double, DoorShape::Arched}, {1, 1});
  A.foundation = p.w({Foundation::Plinth, Foundation::Platform}, {3, 1});
  A.ornament = ORN_PORCH_COLUMNS | (p.chance(150) ? ORN_PAINTED_BANDS : 0) | (p.chance(80) ? ORN_FINIALS : 0) |
               (p.chance(70) ? ORN_FLOWERBOX : 0) | (p.chance(60) ? ORN_CRENELS : 0);
  A.pitch = (uint8_t)p.range(1, 2); A.eave = 1; A.wallH = (uint8_t)p.range(115, 140);
  A.chimneys = 0; A.smoke = false; A.shutters = p.chance(100); A.weather = 0;
  c.customs.furniture = p.w({Furniture::Chairs, Furniture::Benches}, {1, 1});
  c.props.well = 4; c.props.lamp = (uint8_t)p.w({2, 0}, {1, 1}); c.props.bench = 1;
  c.props.centre = (uint8_t)p.w({1, 5, 0}, {3, 2, 2}); c.props.awning = (uint8_t)p.w({1, 3}, {2, 1});
  c.town.layout = Layout::Grid; c.town.altLayout = Layout::Radial;
  c.town.wall = CityWall::Stone; c.town.fence = p.w({Fence::Hedge, Fence::StoneDyke}, {1, 1});
  c.town.density = (uint8_t)p.range(150, 200); c.town.trees = (uint8_t)p.range(60, 110); c.town.paving = 1;
  auto& D = c.dress;
  D.cutM = Cut::Robe; D.cutF = p.w({Cut::Gown, Cut::Robe}, {1, 1});
  D.head[0] = Headwear::None; D.head[1] = Headwear::Circlet; D.head[2] = Headwear::Veil; D.headP = (uint8_t)p.range(40, 80);
  D.pattern = Pattern::BorderTrim;
  D.skinLo = 2; D.skinHi = (uint8_t)p.range(6, 8); D.hairStyles = H_SHORT | H_CURLS | H_BUN | H_BALD;
  D.beardP = (uint8_t)p.range(30, 80); D.jewellery = (uint8_t)p.range(80, 140); D.facePaint = 0;
  auto& R = c.arms;
  R.helm[0] = HelmForm::Crested; R.helm[1] = HelmForm::Plumed;
  R.body[0] = p.w({BodyArm::Scale, BodyArm::Plate, BodyArm::Brigandine}, {2, 2, 1}); R.body[1] = BodyArm::Mail;
  R.shield = ShieldForm::Tower; R.blade = Blade::Straight; R.polearm = Polearm::Spear; R.bow = BowKind::Composite;
  R.metal = rgb(196, 160, 96); R.leather = rgb(120, 60, 40); R.plume = rgb(170, 36, 40);
  R.ornament = ARM_PLUMES | ARM_GILDING | (p.chance(100) ? ARM_ETCHING : 0);
  R.pauldron = 2; R.skirt = 2; R.crest = 3; R.cape = 2;
  auto& M = c.music;
  M.scale = p.w({Scale::Lydian, Scale::Major}, {3, 1}); M.bpm = (uint8_t)p.range(96, 112); M.meter = 4;
  M.lead = LeadInst::Brass; M.pad = p.w({PadInst::Organ, PadInst::Strings}, {2, 1}); M.bass = BassInst::Horn;
  M.perc = PercKind::Kettle; M.swing = 0; M.ornament = (uint8_t)p.range(1, 4); M.drone = 0;
  c.faith.kind = 0; c.faith.domains = D_SUN | D_WAR | D_WISDOM | D_CRAFT | D_SEA; c.faith.shrineForm = 4; c.faith.burial = 0;
  c.customs.staple[0] = 0; c.customs.staple[1] = 6; c.customs.staple[2] = 1; c.customs.drink = 2;
  c.customs.lawStrict = (uint8_t)p.range(170, 220); c.customs.xenophobia = (uint8_t)p.range(30, 80); c.customs.festivalMonth = 3;
  c.heraldry.shape = 0;
  scheme = p.pick(3);
  const uint8_t v[8] = {190, 170, 140, 170, 120, 220, 40, 150};
  for (int i = 0; i < 8; i++) c.values[i] = v[i];
}

void dune(Culture& c, Pk& p, int& scheme) {
  using namespace art;
  auto& A = c.arch;
  A.roof = p.w({RoofShape::FlatParapet, RoofShape::Dome}, {4, 1});
  c.altRoof = A.roof == RoofShape::Dome ? RoofShape::FlatParapet : p.w({RoofShape::Dome, RoofShape::Onion}, {3, 1});
  A.roofMat = A.roof == RoofShape::Dome ? p.w({RoofMat::GlazedTile, RoofMat::Adobe, RoofMat::Copper}, {2, 2, 1}) : RoofMat::Adobe;
  A.wall = p.w({WallMat::Adobe, WallMat::Plaster}, {3, 1});
  A.window = p.w({WindowShape::Screen, WindowShape::Pointed, WindowShape::Arched}, {3, 2, 1});
  A.door = p.w({DoorShape::Arched, DoorShape::Curtain}, {2, 1});
  A.foundation = p.w({Foundation::Plinth, Foundation::None}, {1, 1});
  A.ornament = ORN_AWNINGS | (p.chance(160) ? ORN_WINDCATCHER : 0) | (p.chance(130) ? ORN_PAINTED_BANDS : 0) |
               (p.chance(60) ? ORN_CRENELS : 0) | (p.chance(60) ? ORN_LANTERNS : 0);
  A.pitch = 0; A.eave = 0; A.wallH = (uint8_t)p.range(100, 130);
  A.chimneys = 0; A.smoke = false; A.awnings = true; A.shutters = false; A.weather = (uint8_t)p.range(0, 1);
  c.customs.furniture = Furniture::Cushions;
  c.props.well = (uint8_t)p.w({2, 4}, {3, 2}); c.props.lamp = 2; c.props.bench = 2;
  c.props.centre = (uint8_t)p.w({0, 5, 2}, {3, 1, 1}); c.props.awning = (uint8_t)p.w({4, 1}, {2, 1});
  c.town.layout = Layout::Compound; c.town.altLayout = Layout::Grid;
  c.town.wall = CityWall::Adobe; c.town.fence = Fence::StoneDyke;
  c.town.density = (uint8_t)p.range(170, 220); c.town.trees = (uint8_t)p.range(20, 60); c.town.paving = 3;
  auto& D = c.dress;
  D.cutM = Cut::Kaftan; D.cutF = p.w({Cut::Kaftan, Cut::Robe}, {3, 1});
  D.head[0] = Headwear::Turban; D.head[1] = Headwear::Headscarf; D.head[2] = Headwear::Veil; D.headP = (uint8_t)p.range(180, 230);
  D.pattern = p.w({Pattern::Stripes, Pattern::Embroidery}, {1, 1});
  D.skinLo = (uint8_t)p.range(4, 5); D.skinHi = (uint8_t)p.range(8, 10); D.hairStyles = H_SHORT | H_LONG | H_BRAIDS | H_CURLS;
  D.beardP = (uint8_t)p.range(170, 220); D.jewellery = (uint8_t)p.range(120, 180); D.facePaint = 0;
  auto& R = c.arms;
  R.helm[0] = HelmForm::ConicalAventail; R.helm[1] = HelmForm::Masked;
  R.body[0] = p.w({BodyArm::Mail, BodyArm::Scale}, {2, 1}); R.body[1] = BodyArm::Padded;
  R.shield = p.w({ShieldForm::Crescent, ShieldForm::Round}, {2, 1}); R.blade = Blade::Scimitar; R.polearm = Polearm::Spear; R.bow = BowKind::Composite;
  R.metal = rgb(190, 176, 150); R.leather = rgb(140, 90, 50);
  R.ornament = ARM_TASSELS | ARM_ETCHING | (p.chance(100) ? ARM_GILDING : 0);
  R.pauldron = 1; R.skirt = 2; R.crest = 1; R.cape = 3;
  auto& M = c.music;
  M.scale = p.w({Scale::Hijaz, Scale::HarmonicMinor}, {4, 1}); M.bpm = (uint8_t)p.range(96, 116); M.meter = (uint8_t)p.w({7, 4}, {2, 1});
  M.lead = LeadInst::Oud; M.pad = PadInst::Drone; M.bass = BassInst::Plucked;
  M.perc = p.w({PercKind::Hand, PercKind::Frame}, {2, 1}); M.swing = 0; M.ornament = (uint8_t)p.range(10, 14); M.drone = (uint8_t)p.range(5, 9);
  c.faith.kind = p.w<uint8_t>({1, 4}, {2, 1}); c.faith.domains = D_SUN | D_MOON | D_STARS | D_WISDOM; c.faith.shrineForm = 5; c.faith.burial = 0;
  c.customs.staple[0] = 3; c.customs.staple[1] = 4; c.customs.staple[2] = 6; c.customs.drink = 3;
  c.customs.lawStrict = (uint8_t)p.range(150, 200); c.customs.xenophobia = (uint8_t)p.range(50, 100); c.customs.festivalMonth = 5;
  c.heraldry.shape = 1;
  scheme = p.pick(3);
  const uint8_t v[8] = {150, 220, 190, 150, 90, 120, 90, 180};
  for (int i = 0; i < 8; i++) c.values[i] = v[i];
}

void steppe(Culture& c, Pk& p, int& scheme) {
  using namespace art;
  auto& A = c.arch;
  A.roof = RoofShape::Conical;
  c.altRoof = p.w({RoofShape::Dome, RoofShape::Gable}, {3, 1});
  A.roofMat = p.w({RoofMat::Felt, RoofMat::Bark}, {5, 1});
  A.wall = p.w({WallMat::Felt, WallMat::Wattle}, {4, 1});
  A.window = WindowShape::Round; A.door = DoorShape::Flap; A.foundation = Foundation::None;
  A.ornament = ORN_PAINTED_BANDS | (p.chance(170) ? ORN_PRAYER_FLAGS : 0) | (p.chance(120) ? ORN_CHIMNEY : 0) | (p.chance(60) ? ORN_FINIALS : 0);
  A.pitch = (uint8_t)p.range(1, 2); A.eave = 0; A.wallH = (uint8_t)p.range(55, 80);
  A.chimneys = 1; A.smoke = true; A.shutters = false; A.weather = (uint8_t)p.range(0, 2);
  c.customs.furniture = Furniture::Cushions;
  c.props.well = 1; c.props.lamp = (uint8_t)p.w({2, 5}, {1, 1}); c.props.bench = 2;
  c.props.centre = (uint8_t)p.w({4, 6}, {1, 1}); c.props.awning = 5;
  c.town.layout = Layout::Radial; c.town.altLayout = Layout::Organic;
  c.town.wall = p.w({CityWall::Palisade, CityWall::Thorn}, {2, 1}); c.town.fence = Fence::Rope;
  c.town.density = (uint8_t)p.range(40, 80); c.town.trees = (uint8_t)p.range(10, 40); c.town.paving = 2;
  auto& D = c.dress;
  D.cutM = Cut::Coat; D.cutF = Cut::Coat;
  D.head[0] = Headwear::FurHat; D.head[1] = Headwear::Conical; D.head[2] = Headwear::Cap; D.headP = (uint8_t)p.range(190, 230);
  D.pattern = Pattern::BorderTrim;
  D.skinLo = 3; D.skinHi = (uint8_t)p.range(6, 8); D.hairStyles = H_BRAIDS | H_PONY | H_BALD | H_LONG;
  D.beardP = (uint8_t)p.range(70, 120); D.jewellery = (uint8_t)p.range(90, 150); D.facePaint = p.chance(40) ? 1 : 0;
  auto& R = c.arms;
  R.helm[0] = p.w({HelmForm::Plumed, HelmForm::ConicalAventail}, {3, 1}); R.helm[1] = HelmForm::Spangen;
  R.body[0] = BodyArm::Lamellar; R.body[1] = p.w({BodyArm::Leather, BodyArm::Padded}, {1, 1});
  R.shield = p.w({ShieldForm::Round, ShieldForm::Buckler}, {1, 1}); R.blade = Blade::Curved; R.polearm = Polearm::Spear; R.bow = BowKind::Composite;
  R.metal = rgb(150, 140, 120); R.leather = rgb(130, 80, 44);
  R.ornament = ARM_HORSETAIL | ARM_STUDS | (p.chance(120) ? ARM_FUR_TRIM : 0);
  R.pauldron = 1; R.skirt = 2; R.crest = 2; R.cape = 0;
  auto& M = c.music;
  M.scale = p.w({Scale::PentaMinor, Scale::Dorian}, {4, 1}); M.bpm = (uint8_t)p.range(104, 124); M.meter = (uint8_t)p.w({4, 6}, {2, 1});
  M.lead = p.w({LeadInst::Fiddle, LeadInst::Voice}, {1, 1}); M.pad = PadInst::Drone; M.bass = BassInst::Drone;
  M.perc = PercKind::Frame; M.swing = (uint8_t)p.range(2, 5); M.ornament = (uint8_t)p.range(5, 9); M.drone = (uint8_t)p.range(12, 15);
  c.faith.kind = 3; c.faith.domains = D_SUN | D_STORM | D_STARS | D_WAR; c.faith.shrineForm = 1; c.faith.burial = 3;
  c.customs.staple[0] = 4; c.customs.staple[1] = 8; c.customs.staple[2] = 3; c.customs.drink = 4;
  c.customs.lawStrict = (uint8_t)p.range(60, 110); c.customs.xenophobia = (uint8_t)p.range(80, 140); c.customs.festivalMonth = 6;
  c.heraldry.shape = 3;
  scheme = p.pick(3);
  const uint8_t v[8] = {230, 100, 80, 40, 20, 220, 70, 190};
  for (int i = 0; i < 8; i++) c.values[i] = v[i];
}

void marsh(Culture& c, Pk& p, int& scheme) {
  using namespace art;
  auto& A = c.arch;
  A.roof = p.w({RoofShape::Steep, RoofShape::Gable}, {3, 1});
  c.altRoof = A.roof == RoofShape::Steep ? p.w({RoofShape::Hip, RoofShape::Conical}, {1, 1}) : RoofShape::Steep;
  A.roofMat = p.w({RoofMat::Thatch, RoofMat::Palm, RoofMat::Bark}, {3, 2, 1});
  A.wall = p.w({WallMat::Plank, WallMat::Wattle}, {3, 2});
  A.window = p.w({WindowShape::Square, WindowShape::Lattice}, {1, 1});
  A.door = DoorShape::Curtain; A.foundation = Foundation::Stilts; A.stilts = true;
  A.ornament = (p.chance(160) ? ORN_LANTERNS : 0) | (p.chance(150) ? ORN_VINES : 0) | (p.chance(90) ? ORN_CARVED_RIDGE : 0) |
               (p.chance(60) ? ORN_PRAYER_FLAGS : 0);
  A.pitch = 4; A.eave = (uint8_t)p.range(2, 3); A.wallH = (uint8_t)p.range(70, 95);
  A.chimneys = 0; A.smoke = false; A.shutters = p.chance(100); A.weather = (uint8_t)p.range(2, 3);
  c.customs.furniture = Furniture::Hammocks;
  c.props.well = 1; c.props.lamp = (uint8_t)p.w({1, 5}, {2, 1}); c.props.bench = 3;
  c.props.centre = 3; c.props.awning = 2;
  c.town.layout = Layout::Stilt; c.town.altLayout = Layout::Linear;
  c.town.wall = p.w({CityWall::Thorn, CityWall::Palisade}, {1, 1}); c.town.fence = p.w({Fence::Bamboo, Fence::Rope}, {2, 1});
  c.town.density = (uint8_t)p.range(60, 100); c.town.trees = (uint8_t)p.range(170, 230); c.town.paving = 4;
  auto& D = c.dress;
  D.cutM = Cut::Wrap; D.cutF = Cut::Wrap;
  D.head[0] = Headwear::Headscarf; D.head[1] = Headwear::Conical; D.head[2] = Headwear::None; D.headP = (uint8_t)p.range(100, 160);
  D.pattern = p.w({Pattern::Dots, Pattern::Stripes}, {1, 1});
  D.skinLo = (uint8_t)p.range(4, 5); D.skinHi = (uint8_t)p.range(8, 10); D.hairStyles = H_BUN | H_LONG | H_BRAIDS | H_CURLS | H_SHORT;
  D.beardP = (uint8_t)p.range(40, 80); D.jewellery = (uint8_t)p.range(100, 150); D.facePaint = p.chance(80) ? 2 : 0;
  auto& R = c.arms;
  R.helm[0] = HelmForm::Kettle; R.helm[1] = HelmForm::Nasal;
  R.body[0] = BodyArm::Padded; R.body[1] = BodyArm::Leather;
  R.shield = ShieldForm::Oval; R.blade = Blade::Leaf; R.polearm = Polearm::Spear; R.bow = BowKind::Self;
  R.metal = rgb(130, 120, 100); R.leather = rgb(100, 80, 50);
  R.ornament = ARM_TASSELS | (p.chance(120) ? ARM_STUDS : 0);
  R.pauldron = 0; R.skirt = 1; R.crest = 0; R.cape = 0;
  auto& M = c.music;
  M.scale = p.w({Scale::PentaMajor, Scale::Mixolydian}, {4, 1}); M.bpm = (uint8_t)p.range(80, 96); M.meter = 4;
  M.lead = LeadInst::Reed; M.pad = PadInst::Bowed; M.bass = BassInst::Hand;
  M.perc = PercKind::Frame; M.swing = (uint8_t)p.range(5, 9); M.ornament = (uint8_t)p.range(4, 8); M.drone = (uint8_t)p.range(2, 5);
  c.faith.kind = p.w<uint8_t>({3, 2}, {2, 1}); c.faith.domains = D_MOON | D_SEA | D_FOREST | D_DEATH; c.faith.shrineForm = 2; c.faith.burial = 4;
  c.customs.staple[0] = 1; c.customs.staple[1] = 2; c.customs.staple[2] = 7; c.customs.drink = 3;
  c.customs.lawStrict = (uint8_t)p.range(50, 100); c.customs.xenophobia = (uint8_t)p.range(110, 170); c.customs.festivalMonth = 4;
  c.heraldry.shape = 2;
  scheme = p.pick(3);
  const uint8_t v[8] = {80, 110, 140, 60, 150, 40, 200, 110};
  for (int i = 0; i < 8; i++) c.values[i] = v[i];
}

void jade(Culture& c, Pk& p, int& scheme) {
  using namespace art;
  auto& A = c.arch;
  A.roof = RoofShape::Pagoda;
  c.altRoof = p.w({RoofShape::Hip, RoofShape::Gable}, {2, 1});
  A.roofMat = p.w({RoofMat::GlazedTile, RoofMat::ClayTile}, {3, 1});
  A.wall = p.w({WallMat::Plaster, WallMat::Brick}, {5, 2});   // (integration: no timber frame, it read as Heartland under clay tile)
  A.window = p.w({WindowShape::Lattice, WindowShape::Round}, {3, 1});
  A.door = p.w({DoorShape::Moon, DoorShape::Double}, {1, 2});
  A.foundation = p.w({Foundation::Terrace, Foundation::Platform}, {1, 1});
  A.ornament = ORN_LANTERNS | (p.chance(170) ? ORN_DRAGON_HEADS : 0) | (p.chance(140) ? ORN_CARVED_RIDGE : 0) |
               (p.chance(120) ? ORN_PAINTED_BANDS : 0) | (p.chance(60) ? ORN_PORCH_COLUMNS : 0);
  A.pitch = 3; A.eave = (uint8_t)p.range(2, 3); A.wallH = (uint8_t)p.range(90, 110);
  A.chimneys = 0; A.smoke = false; A.shutters = false; A.weather = (uint8_t)p.range(0, 1);
  c.customs.furniture = p.w({Furniture::Cushions, Furniture::Stools}, {2, 1});
  c.props.well = (uint8_t)p.w({4, 1}, {1, 1}); c.props.lamp = (uint8_t)p.w({3, 1}, {1, 1}); c.props.bench = 1;
  c.props.centre = (uint8_t)p.w({3, 1}, {1, 1}); c.props.awning = (uint8_t)p.w({4, 3}, {2, 1});
  c.town.layout = Layout::Terraced; c.town.altLayout = Layout::Grid;
  // (M3 fixer round 2) blue-grey brick under green glazed tile, pagoda caps on the towers (CityWall::Jade), never the
  // heartland's grey drum-towered curtain; the draw stays so every later draw of the culture is unchanged
  (void)p.w({CityWall::Stone, CityWall::Rampart}, {2, 1});
  c.town.wall = CityWall::Jade; c.town.fence = Fence::Bamboo;
  c.town.density = (uint8_t)p.range(120, 170); c.town.trees = (uint8_t)p.range(90, 150); c.town.paving = 8;
  auto& D = c.dress;
  D.cutM = Cut::Robe; D.cutF = p.w({Cut::Robe, Cut::Gown}, {2, 1});
  D.head[0] = Headwear::Conical; D.head[1] = Headwear::Cap; D.head[2] = Headwear::None; D.headP = (uint8_t)p.range(110, 170);
  D.pattern = Pattern::Embroidery;
  D.skinLo = 1; D.skinHi = (uint8_t)p.range(4, 6); D.hairStyles = H_BUN | H_LONG | H_PONY | H_SHORT;
  D.beardP = (uint8_t)p.range(50, 100); D.jewellery = (uint8_t)p.range(70, 120); D.facePaint = 0;
  auto& R = c.arms;
  R.helm[0] = HelmForm::Masked; R.helm[1] = HelmForm::Crested;
  R.body[0] = BodyArm::Lamellar; R.body[1] = BodyArm::Scale;
  R.shield = p.w({ShieldForm::None, ShieldForm::Oval}, {1, 1}); R.blade = Blade::Curved; R.polearm = Polearm::Glaive;
  R.bow = p.w({BowKind::Longbow, BowKind::Composite}, {2, 1});
  R.metal = rgb(80, 84, 92); R.leather = rgb(110, 50, 40);
  R.ornament = ARM_SCALLOPS | ARM_TASSELS | (p.chance(120) ? ARM_GILDING : 0);
  R.pauldron = 3; R.skirt = 2; R.crest = 2; R.cape = 0;
  auto& M = c.music;
  M.scale = p.w({Scale::InSen, Scale::Hirajoshi}, {3, 2}); M.bpm = (uint8_t)p.range(64, 80); M.meter = 4;
  M.lead = p.w({LeadInst::Bells, LeadInst::Flute}, {3, 2}); M.pad = PadInst::Shimmer; M.bass = BassInst::Plucked;
  M.perc = PercKind::Gong; M.swing = 0; M.ornament = (uint8_t)p.range(6, 10); M.drone = (uint8_t)p.range(2, 5);
  c.faith.kind = p.w<uint8_t>({2, 4}, {1, 1}); c.faith.domains = D_WISDOM | D_MOON | D_HARVEST | D_DEATH; c.faith.shrineForm = 4; c.faith.burial = 0;
  c.customs.staple[0] = 2; c.customs.staple[1] = 1; c.customs.staple[2] = 6; c.customs.drink = 3;
  c.customs.lawStrict = (uint8_t)p.range(170, 230); c.customs.xenophobia = (uint8_t)p.range(90, 150); c.customs.festivalMonth = 1;
  c.heraldry.shape = 3;
  scheme = p.pick(3);
  const uint8_t v[8] = {140, 140, 170, 220, 70, 90, 150, 230};
  for (int i = 0; i < 8; i++) c.values[i] = v[i];
}

void river(Culture& c, Pk& p, int& scheme) {
  using namespace art;
  auto& A = c.arch;
  A.roof = p.w({RoofShape::Mansard, RoofShape::Steep, RoofShape::Gable}, {4, 2, 1});
  c.altRoof = A.roof == RoofShape::Mansard ? RoofShape::Steep : RoofShape::Mansard;
  A.roofMat = p.w({RoofMat::Slate, RoofMat::ClayTile}, {1, 1});
  A.wall = p.w({WallMat::Brick, WallMat::Timber}, {5, 1});
  A.window = WindowShape::Tall; A.door = p.w({DoorShape::Double, DoorShape::Plank}, {2, 1}); A.foundation = Foundation::Plinth;
  A.ornament = ORN_SHUTTERS | ORN_FLOWERBOX | ORN_CHIMNEY | (p.chance(110) ? ORN_AWNINGS : 0) | (p.chance(100) ? ORN_PAINTED_BANDS : 0) |
               (p.chance(70) ? ORN_FINIALS : 0);
  A.pitch = (uint8_t)p.range(3, 4); A.eave = 0; A.wallH = (uint8_t)p.range(125, 150);
  A.chimneys = (uint8_t)p.range(1, 2); A.smoke = false; A.shutters = true; A.weather = (uint8_t)p.range(0, 1);
  c.customs.furniture = Furniture::Chairs;
  c.props.well = (uint8_t)p.w({0, 4}, {1, 1}); c.props.lamp = 0; c.props.bench = 0;
  c.props.centre = (uint8_t)p.w({0, 1}, {1, 1}); c.props.awning = 0;
  c.town.layout = Layout::Linear; c.town.altLayout = Layout::Grid;
  c.town.wall = CityWall::Stone; c.town.fence = Fence::Picket;
  c.town.density = (uint8_t)p.range(180, 230); c.town.trees = (uint8_t)p.range(70, 120); c.town.paving = p.w({0, 1}, {2, 1}) == 0 ? 9 : 0;
  auto& D = c.dress;
  D.cutM = Cut::Coat; D.cutF = p.w({Cut::Gown, Cut::Robe}, {1, 1});
  D.head[0] = Headwear::Cap; D.head[1] = Headwear::None; D.head[2] = Headwear::Hood; D.headP = (uint8_t)p.range(120, 170);
  D.pattern = p.w({Pattern::Stripes, Pattern::Plain}, {1, 1});
  D.skinLo = 0; D.skinHi = (uint8_t)p.range(4, 6); D.hairStyles = H_SHORT | H_LONG | H_BUN | H_CURLS | H_PONY;
  D.beardP = (uint8_t)p.range(80, 130); D.jewellery = (uint8_t)p.range(60, 110); D.facePaint = 0;
  auto& R = c.arms;
  R.helm[0] = p.w({HelmForm::Kettle, HelmForm::Crested}, {2, 1}); R.helm[1] = HelmForm::Nasal;
  R.body[0] = BodyArm::Brigandine; R.body[1] = BodyArm::Padded;
  R.shield = ShieldForm::Heater; R.blade = p.w({Blade::Falchion, Blade::Straight}, {2, 1}); R.polearm = Polearm::Halberd; R.bow = BowKind::Crossbow;
  R.metal = rgb(170, 172, 180); R.leather = rgb(120, 80, 50);
  R.ornament = ARM_RIVETS | ARM_PLUMES;
  R.pauldron = 2; R.skirt = 1; R.crest = 1; R.cape = 0;
  auto& M = c.music;
  M.scale = p.w({Scale::Major, Scale::Mixolydian}, {3, 1}); M.bpm = (uint8_t)p.range(104, 124); M.meter = (uint8_t)p.w({4, 3}, {3, 1});
  M.lead = LeadInst::Fiddle; M.pad = PadInst::Organ; M.bass = BassInst::Plucked;
  M.perc = PercKind::Wood; M.swing = (uint8_t)p.range(7, 11); M.ornament = (uint8_t)p.range(3, 6); M.drone = 0;
  c.faith.kind = p.w<uint8_t>({1, 0}, {1, 1}); c.faith.domains = D_SEA | D_CRAFT | D_HEARTH | D_HARVEST; c.faith.shrineForm = 0; c.faith.burial = 0;
  c.customs.staple[0] = 0; c.customs.staple[1] = 8; c.customs.staple[2] = 1; c.customs.drink = 0;
  c.customs.lawStrict = (uint8_t)p.range(130, 180); c.customs.xenophobia = (uint8_t)p.range(20, 60); c.customs.festivalMonth = 7;
  c.heraldry.shape = 0;
  scheme = p.pick(3);
  const uint8_t v[8] = {90, 240, 100, 150, 200, 110, 40, 120};
  for (int i = 0; i < 8; i++) c.values[i] = v[i];
}

void sunTemple(Culture& c, Pk& p, int& scheme) {
  using namespace art;
  auto& A = c.arch;
  // stone houses under stepped or flat stucco roofs; the palm-thatched hut is the alternate (and the poor man's) house
  A.roof = p.w({RoofShape::Stepped, RoofShape::FlatParapet}, {5, 3});
  c.altRoof = p.w({RoofShape::Steep, RoofShape::Dome}, {3, 1});
  A.roofMat = A.roof == RoofShape::Steep ? RoofMat::Palm : RoofMat::Adobe;
  A.wall = p.w({WallMat::Ashlar, WallMat::Plaster}, {3, 2});
  A.window = p.w({WindowShape::Slit, WindowShape::Square}, {1, 1});
  A.door = p.w({DoorShape::Curtain, DoorShape::Arched}, {2, 1});
  A.foundation = p.w({Foundation::Platform, Foundation::Terrace}, {2, 1});
  A.ornament = ORN_PAINTED_BANDS | (p.chance(150) ? ORN_CRENELS : 0) | (p.chance(110) ? ORN_GILDING : 0) | (p.chance(110) ? ORN_VINES : 0);
  A.pitch = 2; A.eave = (uint8_t)p.range(0, 1); A.wallH = (uint8_t)p.range(85, 110);
  A.chimneys = 0; A.smoke = false; A.shutters = false; A.weather = (uint8_t)p.range(1, 2);
  c.customs.furniture = Furniture::Hammocks;
  c.props.well = 3; c.props.lamp = 2; c.props.bench = 1;
  c.props.centre = (uint8_t)p.w({5, 4, 1}, {2, 2, 1}); c.props.awning = (uint8_t)p.w({2, 1}, {1, 1});
  c.town.layout = Layout::Radial; c.town.altLayout = Layout::Terraced;
  // (M3 fixer round 2) the sun temples build their city walls as they build everything: plastered mud brick and
  // stone under a lime skin, beam ends showing (CityWall::Adobe), never a European drum-towered curtain or a turf
  // bank. The draw stays so every later draw of the culture is unchanged.
  (void)p.w({CityWall::Stone, CityWall::Rampart}, {2, 1});
  c.town.wall = CityWall::Talud; c.town.fence = Fence::Hedge;   // (M3 fixer round 3: its own gate, CityWall::Talud)
  c.town.density = (uint8_t)p.range(110, 160); c.town.trees = (uint8_t)p.range(150, 210); c.town.paving = 7;
  auto& D = c.dress;
  D.cutM = Cut::Wrap; D.cutF = p.w({Cut::Wrap, Cut::Poncho}, {2, 1});
  D.head[0] = Headwear::Circlet; D.head[1] = Headwear::Headscarf; D.head[2] = Headwear::None; D.headP = (uint8_t)p.range(100, 160);
  D.pattern = p.w({Pattern::BorderTrim, Pattern::Embroidery}, {1, 1});
  D.skinLo = (uint8_t)p.range(5, 6); D.skinHi = (uint8_t)p.range(9, 11); D.hairStyles = H_LONG | H_BUN | H_MOHAWK | H_BRAIDS;
  D.beardP = (uint8_t)p.range(10, 50); D.jewellery = (uint8_t)p.range(170, 230); D.facePaint = 4;
  auto& R = c.arms;
  R.helm[0] = p.w({HelmForm::Winged, HelmForm::Plumed}, {1, 1}); R.helm[1] = HelmForm::Masked;
  R.body[0] = BodyArm::Padded; R.body[1] = BodyArm::Leather;
  R.shield = ShieldForm::Round; R.blade = Blade::Broad; R.polearm = Polearm::Spear; R.bow = BowKind::Self;
  R.metal = rgb(200, 170, 90); R.leather = rgb(150, 100, 60); R.plume = rgb(40, 150, 110);
  R.ornament = ARM_PLUMES | ARM_GILDING | ARM_TASSELS;
  R.pauldron = 0; R.skirt = 1; R.crest = 3; R.cape = 1;
  auto& M = c.music;
  M.scale = p.w({Scale::Phrygian, Scale::Hirajoshi}, {3, 1}); M.bpm = (uint8_t)p.range(96, 116); M.meter = (uint8_t)p.w({4, 5}, {2, 1});
  M.lead = LeadInst::Marimba; M.pad = PadInst::None; M.bass = BassInst::Hand;
  M.perc = PercKind::Hand; M.swing = (uint8_t)p.range(2, 5); M.ornament = (uint8_t)p.range(2, 5); M.drone = (uint8_t)p.range(0, 3);
  c.faith.kind = p.w<uint8_t>({0, 5}, {2, 1}); c.faith.domains = D_SUN | D_HARVEST | D_WAR | D_DEATH | D_STARS; c.faith.shrineForm = 5; c.faith.burial = 0;
  c.customs.staple[0] = 9; c.customs.staple[1] = 6; c.customs.staple[2] = 7; c.customs.drink = 5;
  c.customs.lawStrict = (uint8_t)p.range(160, 220); c.customs.xenophobia = (uint8_t)p.range(90, 150); c.customs.festivalMonth = 6;
  c.heraldry.shape = 3;
  scheme = p.pick(3);
  const uint8_t v[8] = {190, 120, 240, 130, 50, 160, 110, 190};
  for (int i = 0; i < 8; i++) c.values[i] = v[i];
}

void sylvan(Culture& c, Pk& p, int& scheme) {
  using namespace art;
  auto& A = c.arch;
  A.roof = p.w({RoofShape::Sweep, RoofShape::Conical}, {4, 1});
  c.altRoof = A.roof == RoofShape::Sweep ? RoofShape::Conical : RoofShape::Sweep;
  A.roofMat = p.w({RoofMat::Leaf, RoofMat::Bark}, {3, 2});
  A.wall = p.w({WallMat::Living, WallMat::Plank}, {4, 1});
  A.window = p.w({WindowShape::Round, WindowShape::Arched}, {2, 1});
  A.door = p.w({DoorShape::Round, DoorShape::Arched}, {2, 1});
  A.foundation = p.w({Foundation::None, Foundation::Stilts}, {4, 1});
  A.ornament = ORN_VINES | (p.chance(180) ? ORN_LANTERNS : 0) | (p.chance(130) ? ORN_CARVED_RIDGE : 0) | (p.chance(70) ? ORN_FINIALS : 0);
  A.pitch = (uint8_t)p.range(2, 3); A.eave = (uint8_t)p.range(2, 3); A.wallH = (uint8_t)p.range(80, 105);
  A.chimneys = 0; A.smoke = false; A.shutters = false; A.weather = 1;
  c.customs.furniture = p.w({Furniture::Hammocks, Furniture::Cushions}, {1, 1});
  c.props.well = 3; c.props.lamp = 4; c.props.bench = 3;
  c.props.centre = 3; c.props.awning = (uint8_t)p.w({1, 4}, {2, 1});
  c.town.layout = Layout::Organic; c.town.altLayout = Layout::Linear;
  c.town.wall = CityWall::Thorn; c.town.fence = Fence::Hedge;
  c.town.density = (uint8_t)p.range(40, 80); c.town.trees = (uint8_t)p.range(220, 255); c.town.paving = 5;
  auto& D = c.dress;
  D.cutM = p.w({Cut::Coat, Cut::Tunic}, {2, 1}); D.cutF = Cut::Gown;
  D.head[0] = Headwear::None; D.head[1] = Headwear::Hood; D.head[2] = Headwear::Circlet; D.headP = (uint8_t)p.range(60, 110);
  D.pattern = Pattern::Vines;
  D.skinLo = 0; D.skinHi = (uint8_t)p.range(4, 6); D.hairStyles = H_LONG | H_BRAIDS | H_PONY;
  D.beardP = (uint8_t)p.range(0, 15); D.jewellery = (uint8_t)p.range(90, 140); D.facePaint = p.chance(60) ? 1 : 0;
  auto& R = c.arms;
  R.helm[0] = p.w({HelmForm::Crested, HelmForm::Winged}, {3, 1}); R.helm[1] = HelmForm::Masked;
  R.body[0] = BodyArm::Leaf; R.body[1] = BodyArm::Leather;
  R.shield = ShieldForm::Leaf; R.blade = Blade::Leaf; R.polearm = Polearm::Glaive; R.bow = BowKind::Longbow;
  R.metal = rgb(150, 170, 130); R.leather = rgb(90, 70, 46); R.plume = rgb(200, 170, 80);
  R.ornament = ARM_ETCHING | ARM_FILIGREE;
  R.pauldron = 1; R.skirt = 2; R.crest = 1; R.cape = 2;
  auto& M = c.music;
  M.scale = p.w({Scale::Minor, Scale::PentaMajor}, {3, 1}); M.bpm = (uint8_t)p.range(72, 88); M.meter = (uint8_t)p.w({6, 3}, {2, 1});
  M.lead = p.w({LeadInst::Flute, LeadInst::Harp}, {3, 1}); M.pad = PadInst::Shimmer; M.bass = BassInst::Plucked;
  M.perc = PercKind::Bells; M.swing = 0; M.ornament = (uint8_t)p.range(5, 9); M.drone = (uint8_t)p.range(1, 4);
  c.faith.kind = 3; c.faith.domains = D_FOREST | D_MOON | D_HARVEST | D_STARS; c.faith.shrineForm = 2; c.faith.burial = 0;
  c.customs.staple[0] = 6; c.customs.staple[1] = 7; c.customs.staple[2] = 0; c.customs.drink = 5;
  c.customs.lawStrict = (uint8_t)p.range(70, 120); c.customs.xenophobia = (uint8_t)p.range(140, 200); c.customs.festivalMonth = 4;
  c.heraldry.shape = 2;
  c.peopleMix[0] = 18; c.peopleMix[1] = 50; c.peopleMix[2] = 230;
  scheme = p.pick(3);
  const uint8_t v[8] = {110, 60, 150, 140, 40, 30, 230, 160};
  for (int i = 0; i < 8; i++) c.values[i] = v[i];
}

void starspire(Culture& c, Pk& p, int& scheme) {
  using namespace art;
  auto& A = c.arch;
  A.roof = p.w({RoofShape::Onion, RoofShape::Conical}, {1, 1});
  c.altRoof = A.roof == RoofShape::Onion ? RoofShape::Conical : RoofShape::Onion;
  A.roofMat = p.w({RoofMat::Slate, RoofMat::Copper, RoofMat::GlazedTile}, {3, 1, 1});
  A.wall = p.w({WallMat::Ashlar, WallMat::Plaster}, {4, 1});
  A.window = p.w({WindowShape::Pointed, WindowShape::Tall}, {2, 1});
  A.door = p.w({DoorShape::Arched, DoorShape::Double}, {2, 1});
  A.foundation = p.w({Foundation::Platform, Foundation::Plinth}, {2, 1});
  A.ornament = ORN_FINIALS | (p.chance(150) ? ORN_GILDING : 0) | (p.chance(120) ? ORN_PAINTED_BANDS : 0) | (p.chance(90) ? ORN_CARVED_RIDGE : 0) |
               (p.chance(70) ? ORN_LANTERNS : 0);
  A.pitch = 4; A.eave = (uint8_t)p.range(0, 1); A.wallH = (uint8_t)p.range(130, 160);
  A.chimneys = 0; A.smoke = false; A.shutters = false; A.weather = 0;
  c.customs.furniture = Furniture::Chairs;
  c.props.well = (uint8_t)p.w({4, 3}, {1, 1}); c.props.lamp = (uint8_t)p.w({4, 3}, {2, 1}); c.props.bench = 1;
  c.props.centre = (uint8_t)p.w({5, 1, 0}, {2, 1, 1}); c.props.awning = 4;
  c.town.layout = Layout::Radial; c.town.altLayout = Layout::Grid;
  c.town.wall = CityWall::WhiteStone; c.town.fence = p.w({Fence::Hedge, Fence::StoneDyke}, {1, 1});
  c.town.density = (uint8_t)p.range(110, 150); c.town.trees = (uint8_t)p.range(120, 170); c.town.paving = 6;
  auto& D = c.dress;
  D.cutM = Cut::Robe; D.cutF = Cut::Gown;
  D.head[0] = Headwear::Circlet; D.head[1] = Headwear::None; D.head[2] = Headwear::Veil; D.headP = (uint8_t)p.range(110, 170);
  D.pattern = Pattern::Embroidery;
  D.skinLo = 0; D.skinHi = (uint8_t)p.range(2, 4); D.hairStyles = H_LONG | H_BUN | H_BRAIDS;
  D.beardP = 0; D.jewellery = (uint8_t)p.range(170, 230); D.facePaint = 0;
  auto& R = c.arms;
  R.helm[0] = HelmForm::Winged; R.helm[1] = HelmForm::Crested;
  R.body[0] = p.w({BodyArm::Plate, BodyArm::Scale}, {2, 1}); R.body[1] = BodyArm::Mail;
  R.shield = ShieldForm::Kite; R.blade = Blade::Glaive; R.polearm = Polearm::Glaive; R.bow = BowKind::Longbow;
  R.metal = rgb(214, 218, 230); R.leather = rgb(70, 60, 90); R.plume = rgb(230, 230, 240);
  R.ornament = ARM_FILIGREE | ARM_GILDING | (p.chance(120) ? ARM_PLUMES : 0);
  R.pauldron = 2; R.skirt = 2; R.crest = 2; R.cape = 2;
  auto& M = c.music;
  M.scale = p.w({Scale::Lydian, Scale::HarmonicMinor}, {3, 1}); M.bpm = (uint8_t)p.range(60, 74); M.meter = (uint8_t)p.w({3, 4}, {2, 1});
  M.lead = p.w({LeadInst::Harp, LeadInst::Bells}, {2, 1}); M.pad = PadInst::Choir; M.bass = BassInst::Bowed;
  M.perc = PercKind::Bells; M.swing = 0; M.ornament = (uint8_t)p.range(3, 6); M.drone = (uint8_t)p.range(3, 6);
  c.faith.kind = 5; c.faith.domains = D_STARS | D_MOON | D_WISDOM | D_SUN; c.faith.shrineForm = 5; c.faith.burial = 3;
  c.customs.staple[0] = 6; c.customs.staple[1] = 0; c.customs.staple[2] = 8; c.customs.drink = 2;
  c.customs.lawStrict = (uint8_t)p.range(180, 230); c.customs.xenophobia = (uint8_t)p.range(150, 210); c.customs.festivalMonth = 12;
  c.heraldry.shape = 3;
  c.peopleMix[0] = 14; c.peopleMix[1] = 40; c.peopleMix[2] = 240;
  scheme = p.pick(3);
  const uint8_t v[8] = {130, 90, 160, 240, 60, 70, 210, 200};
  for (int i = 0; i < 8; i++) c.values[i] = v[i];
}
}  // namespace

// ------------------------------------------------------------------ alloys, faith and the finishing touches
std::string rootWord(const Culture& c, uint32_t seed, int maxLen);   // names.cpp: a short word in the culture's tongue
bool nameBanned(const std::string& s);                                // names.cpp

namespace {
struct AlloyLook { Sheen sheen; uint32_t light, dark; };
const AlloyLook kAlloyLook[(int)Archetype::COUNT][2] = {
    {{Sheen::Dark, rgb(120, 126, 140), rgb(44, 46, 56)}, {Sheen::Banded, rgb(176, 186, 200), rgb(70, 78, 96)}},        // Fjordfolk
    {{Sheen::Matte, rgb(150, 146, 140), rgb(64, 60, 58)}, {Sheen::Burnished, rgb(196, 170, 120), rgb(96, 74, 46)}},      // Highland
    {{Sheen::Bright, rgb(206, 210, 218), rgb(98, 104, 118)}, {Sheen::Burnished, rgb(230, 200, 120), rgb(130, 96, 40)}},  // Heartland
    {{Sheen::Burnished, rgb(226, 186, 104), rgb(126, 86, 36)}, {Sheen::Bright, rgb(236, 220, 170), rgb(150, 116, 60)}},  // Imperial
    {{Sheen::Bright, rgb(236, 198, 110), rgb(140, 96, 40)}, {Sheen::Banded, rgb(214, 214, 220), rgb(96, 90, 100)}},      // Dune
    {{Sheen::Dark, rgb(120, 110, 100), rgb(50, 44, 40)}, {Sheen::Iridescent, rgb(150, 160, 190), rgb(60, 56, 80)}},      // Steppe
    {{Sheen::Matte, rgb(120, 130, 110), rgb(52, 60, 48)}, {Sheen::Dark, rgb(90, 110, 100), rgb(30, 40, 36)}},            // Marsh
    {{Sheen::Dark, rgb(70, 76, 90), rgb(24, 26, 34)}, {Sheen::Glowing, rgb(120, 210, 170), rgb(30, 90, 70)}},            // Jade
    {{Sheen::Bright, rgb(200, 204, 212), rgb(90, 96, 108)}, {Sheen::Burnished, rgb(220, 150, 90), rgb(120, 70, 40)}},    // River
    {{Sheen::Burnished, rgb(232, 190, 80), rgb(130, 90, 30)}, {Sheen::Dark, rgb(70, 60, 70), rgb(20, 16, 24)}},          // SunTemple
    {{Sheen::Pale, rgb(200, 214, 180), rgb(100, 120, 84)}, {Sheen::Glowing, rgb(230, 200, 110), rgb(140, 100, 40)}},     // Sylvan
    {{Sheen::Pale, rgb(226, 232, 244), rgb(120, 130, 170)}, {Sheen::Iridescent, rgb(190, 180, 240), rgb(80, 70, 140)}},  // Starspire
};
const char* const kLoreCommon[4] = {"THE EVERYDAY METAL OF THEIR SMITHIES", "POURED IN CLAY MOULDS PASSED FROM MASTER TO APPRENTICE",
                                    "HARDER THAN TRADE BRONZE, CHEAPER THAN STEEL", "A GUILD MIX: EVERY SMITH SWEARS BY THEIR OWN MEASURE"};
const char* const kLoreMid[4] = {"ITS TEMPER IS A FAMILY SECRET", "BLUED IN THE FORGE SMOKE UNTIL IT SINGS",
                                 "STRUCK ONLY ON HOLY DAYS", "THE WARRIORS' METAL: NEVER SOLD TO OUTSIDERS"};

void makeAlloys(Culture& c, Pk& p, int favouredOre) {
  const int a = (int)c.archetype;
  const Tongue& T = kTongue[a];
  const std::vector<std::string> reag = split(T.reagents), roots = split(T.roots);
  std::vector<Alloy>& out = c.arms.alloys;
  out.clear();
  const int n = (c.archetype == Archetype::Sylvan || c.archetype == Archetype::Starspire || c.isolated) ? 3 : (p.chance(150) ? 3 : 2);
  // ore ids (ew::Ore): Copper 0, Tin 1, Iron 2, Coal 3, Silver 4, Rare 5
  for (int k = 0; k < n; k++) {
    Alloy A;
    std::string word;
    if (k == 0) {   // the everyday alloy: a better bronze or a hard iron
      const bool bronze = favouredOre == 0 || favouredOre == 1 || (favouredOre < 0 && p.chance(128));
      A.baseTier = bronze ? Tier::Bronze : Tier::Iron;
      A.tierStep = 1;
      if (bronze) {
        A.recipe.push_back({0, "", 3});
        A.recipe.push_back({1, "", 1});
        word = p.chance(128) ? "BRASS" : "BRONZE";
      } else {
        A.recipe.push_back({2, "", 3});
        A.recipe.push_back({3, "", 1});
        word = "IRON";
      }
      if (p.chance(150)) A.recipe.push_back({-1, reag[(size_t)p.pick((int)reag.size())], 1});
      A.secrecy = (uint8_t)p.range(30, 90);
      A.learn = 1 | 4;
      A.lore = kLoreCommon[p.pick(4)];
    } else if (k == 1) {   // the warriors' steel
      A.baseTier = Tier::Steel;
      A.tierStep = (uint8_t)p.range(1, 2);
      A.recipe.push_back({2, "", 4});
      A.recipe.push_back({3, "", 2});
      const int extra = favouredOre >= 0 && favouredOre != 2 && favouredOre != 3 ? favouredOre : (p.chance(128) ? 4 : 0);
      A.recipe.push_back({(int8_t)extra, "", 1});
      word = p.chance(140) ? "STEEL" : (extra == 4 ? "SILVER" : "STEEL");
      A.secrecy = (uint8_t)p.range(120, 180);
      A.learn = 1 | 2 | 4;
      A.lore = n == 2 ? T.lore : kLoreMid[p.pick(4)];
    } else {   // the masterwork: rare metal and a reagent only these lands yield
      A.baseTier = Tier::Steel;
      A.tierStep = 3;
      A.recipe.push_back({5, "", 2});
      A.recipe.push_back({p.chance(128) ? (int8_t)2 : (int8_t)4, "", 2});
      A.recipe.push_back({-1, reag[(size_t)p.pick((int)reag.size())], 1});
      if (p.chance(100)) A.recipe.push_back({-1, reag[(size_t)p.pick((int)reag.size())], 1});
      if (A.recipe.size() == 4 && A.recipe[3].reagent == A.recipe[2].reagent) A.recipe.pop_back();
      static const char* const w3[] = {"STEEL", "SILVER", "GLASS", "IRON"};
      word = w3[p.pick(c.archetype == Archetype::Starspire || c.archetype == Archetype::Sylvan ? 3 : 2)];
      A.secrecy = (uint8_t)p.range(200, 250);
      A.learn = 2 | 8;
      A.lore = T.lore;
    }
    const AlloyLook& L0 = kAlloyLook[a][0];
    const AlloyLook& L1 = kAlloyLook[a][1];
    if (k == 0) { A.sheen = L0.sheen; A.color = p.jit(L0.light, 5); A.color2 = p.jit(L0.dark, 5); }
    else if (k == 1) {
      A.sheen = L1.sheen == Sheen::Glowing || L1.sheen == Sheen::Iridescent ? Sheen::Bright : L1.sheen;
      A.color = p.jit(mixC(L0.light, L1.light, 128), 5);
      A.color2 = p.jit(mixC(L0.dark, L1.dark, 128), 5);
    } else { A.sheen = L1.sheen; A.color = p.jit(L1.light, 5); A.color2 = p.jit(L1.dark, 5); }
    if (c.isolated && k == 2) { A.sheen = Sheen::Glowing; A.color = rgb(110, 220, 210); A.color2 = rgb(80, 40, 140); }
    // the name: the culture's own root, or an English root these smiths would use, + the metal word (<= 12 letters)
    const bool own = p.chance(150);
    std::string root = own ? rootWord(c, (uint32_t)p.next(), 12 - (int)word.size()) : roots[(size_t)p.pick((int)roots.size())];
    if ((int)(root.size() + word.size()) > 12) root = root.substr(0, (size_t)(12 - (int)word.size()));
    auto isV = [](char ch) { return ch == 'A' || ch == 'E' || ch == 'I' || ch == 'O' || ch == 'U' || ch == 'Y'; };
    // a clean seam: no vowel pile-up, and at most one consonant before the metal word's own cluster
    while (root.size() > 1 && isV(root.back()) && (isV(word[0]) || isV(root[root.size() - 2]))) root.pop_back();
    while (own && root.size() > 2 && !isV(root.back()) && !isV(root[root.size() - 2])) root.pop_back();
    A.name = root + word;
    bool dup = false;
    for (const Alloy& o : out) dup |= o.name == A.name;
    if (dup || nameBanned(A.name) || root.size() < 2) A.name = roots[(size_t)(k % (int)roots.size())] + word;
    out.push_back(A);
  }
  c.arms.favouredOre = (int8_t)favouredOre;
}

void makeFaith(Culture& c, Pk& p) {
  Religion& F = c.faith;
  F.names.clear();
  const int gods = F.kind == 1 ? 1 : F.kind == 4 ? 2 : F.kind == 2 ? p.range(1, 2) : p.range(3, 4);
  for (int g = 0; g < gods; g++) {
    std::string n = personName(c, (uint32_t)p.next() ^ 0x60D5u, (g & 1) != 0);
    if (n.size() > 8) n = n.substr(0, 8);
    bool dup = false;
    for (const std::string& o : F.names) dup |= o == n;
    if (!dup) F.names.push_back(n);
  }
  F.glyphSeed = (uint32_t)p.next() | 1u;
  F.colour = mixC(c.arch.accentTint, rgb(236, 232, 220), 120);
  F.colour2 = c.arch.altTint;
  c.customs.greetingSet = (uint8_t)((int)c.archetype * 2 + p.pick(2));
  c.customs.festivalMonth = (uint8_t)((c.customs.festivalMonth + p.range(-1, 1) + 11) % 12 + 1);
  c.magicTradition = 0;
}

void archetypeDraws(Culture& c, Archetype a, Pk& p, int& scheme) {
  switch (a) {
    case Archetype::Fjordfolk: fjordfolk(c, p, scheme); break;
    case Archetype::Highland: highland(c, p, scheme); break;
    case Archetype::Heartland: heartland(c, p, scheme); break;
    case Archetype::Imperial: imperial(c, p, scheme); break;
    case Archetype::Dune: dune(c, p, scheme); break;
    case Archetype::Steppe: steppe(c, p, scheme); break;
    case Archetype::Marsh: marsh(c, p, scheme); break;
    case Archetype::Jade: jade(c, p, scheme); break;
    case Archetype::River: river(c, p, scheme); break;
    case Archetype::SunTemple: sunTemple(c, p, scheme); break;
    case Archetype::Sylvan: sylvan(c, p, scheme); break;
    default: starspire(c, p, scheme); break;
  }
}

// an isolated family crosses two archetypes: the second's roof, headwear, helm and lead instrument over the first's
// walls, layout and tongue; a rarer scale; and the magic-touched teal and violet palette (VISION_PLAN 5.3)
void crossWith(Culture& c, Archetype b, Pk& p) {
  Culture o;
  o.archetype = b;
  int sch = 0;
  archetypeDraws(o, b, p, sch);
  c.archetype2 = b;
  c.isolated = true;
  c.arch.roof = o.arch.roof;
  c.arch.roofMat = o.arch.roofMat;
  c.arch.pitch = o.arch.pitch;
  c.altRoof = o.altRoof;
  c.arch.ornament = (uint16_t)(c.arch.ornament | (o.arch.ornament & (art::ORN_DRAGON_HEADS | art::ORN_FINIALS | art::ORN_LANTERNS)));
  c.dress.head[0] = o.dress.head[0];
  c.arms.helm[0] = o.arms.helm[0];
  c.music.lead = o.music.lead;
  static const Scale rare[4] = {Scale::Hirajoshi, Scale::HarmonicMinor, Scale::Phrygian, Scale::Hijaz};
  c.music.scale = rare[p.pick(4)];
  c.arch.roofTint = roofColourFor(c.arch.roofMat, mixC(c.arch.roofTint, rgb(60, 130, 140), 150));
  c.arch.accentTint = p.jit(p.chance(128) ? rgb(70, 170, 170) : rgb(120, 70, 170), 6);
  c.arch.altTint = p.jit(p.chance(128) ? rgb(150, 110, 200) : rgb(60, 150, 140), 6);
  c.dress.cloth[0] = mixC(c.dress.cloth[0], rgb(70, 60, 130), 140);
  c.dress.cloth[2] = mixC(c.dress.cloth[2], rgb(50, 140, 140), 140);
  c.props.awningA = c.arch.accentTint;
  c.props.awningB = c.arch.altTint;
}
}  // namespace

// The whole culture of one archetype from one seed (Atlas::make and the maximin's candidates), in two stages so the
// maximin can score its candidates cheaply: applyPriors draws everything the distance metric reads (architecture,
// dress, arms, music, faith kind, palette, phonology); finishCulture (from its own stream) adds what only the winner
// needs (its name, alloys, gods). cross >= 0: an isolated family crossed with that archetype.
void applyPriors(Culture& c, Archetype a, uint32_t seed, int favouredOre, int cross) {
  Pk p(((uint64_t)seed << 8) ^ 0xC0170A11ull ^ ((uint64_t)a << 40));
  c.archetype = c.archetype2 = a;
  int scheme = 0;
  archetypeDraws(c, a, p, scheme);
  // values and the people mix wander a little inside the archetype
  for (uint8_t& v : c.values) v = (uint8_t)clamp8((int)v + p.range(-30, 30));
  if (a != Archetype::Sylvan && a != Archetype::Starspire) {
    c.peopleMix[0] = (uint8_t)p.range(215, 240);
    c.peopleMix[1] = (uint8_t)p.range(12, 30);
    c.peopleMix[2] = (uint8_t)p.range(2, 9);
  }
  common(c, p, scheme);
  if (cross >= 0 && cross != (int)a) crossWith(c, (Archetype)cross, p);
  c.arms.favouredOre = (int8_t)favouredOre;
}

void finishCulture(Culture& c) {
  Pk p(((uint64_t)c.seed << 12) ^ 0xF1A15EDull ^ ((uint64_t)c.archetype << 44));
  const Archetype a = c.archetype;
  // the culture's own name and adjective
  for (int tries = 0; tries < 8; tries++) {
    c.name = kingdomName(c, (uint32_t)p.next() ^ 0x0A3Eu);
    if (c.name.size() >= 4 && c.name.size() <= 9 && c.name.find_first_of(" -'") == std::string::npos) break;
  }
  std::string clean;
  for (char ch : c.name) if (ch >= 'A' && ch <= 'Z') clean += ch;
  c.name = clean.size() > 9 ? clean.substr(0, 9) : clean;
  if (c.name.size() < 3) c.name += "AR";
  static const char* const adj[(int)Archetype::COUNT] = {"SK", "ACH", "ISH", "IAN", "I", "I", "AN", "ESE", "ER", "ECA", "ARI", "IAN"};
  const char last = c.name.back();
  const bool vowel = last == 'A' || last == 'E' || last == 'I' || last == 'O' || last == 'U' || last == 'Y';
  c.adjective = c.name + (vowel ? std::string("N") : std::string(adj[(int)a]));
  makeAlloys(c, p, c.arms.favouredOre);
  makeFaith(c, p);
  c.props.metal = c.arms.metal;
}

}  // namespace detail
}  // namespace cult
