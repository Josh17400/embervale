// M9 "Arcana & Mastery" (VISION_PLAN 15.29.7, 15.5, 15.30): the spells on screen. SPELLFX lane.
//
// The magic's look is written once, free of the View, for two painters: the game's (rpg/view/render_magic.cpp: Pix
// rects, blends and baked textures) and the magic gallery's (tools/preview/magic_gallery.cpp: a software Canvas), so the
// contact sheets the art is judged on are the screen's own pixels. Three headers:
//   magic_fx.h       (this) the looks (a SpellDef's identity: school, form, essence, FxStyle ramp / particle / motion /
//                    glyphs / size), the baked sheets (glows, smoke, runes, sigils, the ramp's flames, cores and bursts,
//                    decals), the World (pooled effects, particles and decals with hard caps; events in, requests out:
//                    sounds, the screen kick, the scene flash, the light-map lift)
//   magic_draw.h     the drawing (templates over a painter): every phase of a spell's life, the missiles, the decals,
//                    the status overlays
//   magic_surface.h  the surfaces' animated pixel art (per-tile baked frames with organic metaball borders that join
//                    tile to tile and kind to kind; flames, clouds and arcs over them)
//
// A painter Pt offers:
//   rect(x, y, w, h, Color)                       alpha-blended fill
//   add(x, y, w, h, Color)                        additive fill
//   blit(Sheet&, sx, sy, sw, sh, dx, dy, dw, dh, Color tint, int blend, bool flip)   blend 0 normal, 1 additive
//   creature(int monster, int frame, float feetX, float feetY, int rows, float scale, Color tint, int blend, bool flip)
//                                                 a monster sheet's cell standing on (feetX, feetY), only its top `rows`
//                                                 rows (a summon climbing out of its rift; -1 all of it)
//
// Everything is whole-pixel pixel art on the 3/4 view with the light from the top left (lit top-left edges, shaded
// bottom-right, cast shadows on the ground). No per-pixel full-screen loops: rects, a few dozen per effect, and blits of
// sheets baked once. Particles, effects and decals are pooled with hard caps (the phone's frame budget).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "engine/audio.h"
#include "engine/color.h"
#include "engine/mathx.h"
#include "engine/pix.h"
#include "rpg/sim/common.h"
#include "rpg/sim/magic.h"

namespace mfx {

using magic::Essence;
using magic::Form;
using magic::Motion;
using magic::Phase;
using magic::School;
using magic::Status;
using magic::Surface;
using MPart = magic::Particle;

constexpr float kPi = 3.14159265f, kTau = 6.2831853f;

// ------------------------------------------------------------------ small helpers
inline float sat(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
inline float smooth01(float t) { t = sat(t); return t * t * (3 - 2 * t); }
inline Color cu(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
inline uint32_t pk(Color c) {
  auto b = [](float v) { return (int)std::lround(sat(v) * 255.0f); };
  return rgba(b(c.r), b(c.g), b(c.b), b(c.a));
}
inline Color lerpC(Color a, Color b, float t) { return Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t); }
inline Color mulC(Color c, float k) { return Color(std::min(1.0f, c.r * k), std::min(1.0f, c.g * k), std::min(1.0f, c.b * k), c.a); }
inline Color withA(Color c, float a) { return Color(c.r, c.g, c.b, a); }
inline uint32_t hmix(uint32_t a, uint32_t b) { return hash32(a * 0x9E3779B1u ^ (b + 0x7F4A7C15u)); }
inline float hf(uint32_t h) { return (float)(h & 0xFFFF) / 65535.0f; }
struct Rnd {
  uint32_t s = 0x2545F491u;
  uint32_t u() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
  float f() { return (float)(u() >> 8) * (1.0f / 16777216.0f); }
  float r(float lo, float hi) { return lo + (hi - lo) * f(); }
};
// smooth value noise on integer lattice cells (global coordinates: continuous across tiles)
inline float vnoise(float x, float y, uint32_t seed) {
  const float fx = std::floor(x), fy = std::floor(y);
  const int ix = (int)fx, iy = (int)fy;
  const float tx = smooth01(x - fx), ty = smooth01(y - fy);
  auto h = [&](int a, int b) { return hf(hash2(a, b, seed)); };
  const float a = h(ix, iy), b = h(ix + 1, iy), c = h(ix, iy + 1), d = h(ix + 1, iy + 1);
  return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

// ------------------------------------------------------------------ the look of a spell
struct Look {
  uint32_t spell = 0, glyph = 0, tradition = 0;
  School school = School::Destruction;
  Form form = Form::Bolt;
  Essence ess = Essence::Arcane;
  MPart part = MPart::Spark;
  Motion motion = Motion::Spiral;
  Surface leaves = Surface::None;
  uint16_t mods = 0, statuses = 0;
  uint8_t motif = 0, size = 1;
  uint32_t ramp[5] = {};
  float radius = 0;
  Color c(int i, float a = 1) const { return cu(ramp[i < 0 ? 0 : i > 4 ? 4 : i], a); }
  uint32_t rampKey() const {
    uint32_t h = 0x811C9DC5u;
    for (uint32_t v : ramp) h = hmix(h, v);
    return h;
  }
};
// the base scale of a look (FxStyle::size 1 a bolt .. 4 a spectacle)
inline float baseScale(const Look& L) {
  static const float k[5] = {1.0f, 1.0f, 1.3f, 1.75f, 2.3f};
  return k[L.size > 4 ? 4 : L.size];
}
inline bool spectacle(const Look& L) { return L.size >= 3 || L.form == Form::Meteor || L.form == Form::Storm; }

// the palette of an essence (a synthetic spell's style, and any spell with no ramp of its own); the school tunes the
// motion (Restoration rises, Conjuration tears, Illusion shimmers, Alteration pulses; Destruction keeps the essence's)
inline magic::FxStyle essenceStyle(Essence e, School s) {
  auto c = [](int r, int g, int b) { return (uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16 | 0xFF000000u; };
  magic::FxStyle f;
  auto set = [&](uint32_t a, uint32_t b, uint32_t cc, uint32_t d, uint32_t e5, MPart p, Motion m) {
    f.ramp[0] = a; f.ramp[1] = b; f.ramp[2] = cc; f.ramp[3] = d; f.ramp[4] = e5; f.particle = p; f.motion = m;
  };
  switch (e) {
    case Essence::Fire: set(c(90, 14, 8), c(190, 40, 12), c(250, 120, 30), c(255, 210, 90), c(255, 250, 220), MPart::Ember, Motion::Boil); break;
    case Essence::Frost: set(c(16, 30, 90), c(40, 90, 190), c(110, 190, 250), c(200, 240, 255), c(255, 255, 255), MPart::Shard, Motion::Shatter); break;
    case Essence::Storm: set(c(40, 20, 90), c(90, 70, 220), c(150, 170, 255), c(220, 230, 255), c(255, 255, 255), MPart::Spark, Motion::Fork); break;
    case Essence::Stone: set(c(44, 30, 20), c(100, 72, 46), c(162, 124, 80), c(214, 186, 136), c(250, 236, 200), MPart::Shard, Motion::Pulse); break;
    case Essence::Tide: set(c(10, 30, 64), c(24, 82, 134), c(60, 150, 204), c(150, 220, 240), c(235, 250, 255), MPart::Droplet, Motion::Pulse); break;
    case Essence::Venom: set(c(20, 40, 12), c(60, 110, 24), c(120, 190, 44), c(190, 240, 110), c(240, 255, 200), MPart::Smoke, Motion::Drift); break;
    case Essence::Shadow: set(c(14, 8, 26), c(52, 26, 84), c(112, 62, 172), c(182, 142, 240), c(240, 226, 255), MPart::Smoke, Motion::Tear); break;
    case Essence::Light: set(c(120, 70, 10), c(220, 150, 40), c(255, 210, 100), c(255, 240, 180), c(255, 255, 240), MPart::Mote, Motion::Rise); break;
    case Essence::Blood: set(c(52, 4, 8), c(122, 10, 22), c(200, 30, 42), c(250, 110, 110), c(255, 222, 222), MPart::Droplet, Motion::Spiral); break;
    case Essence::Bone: set(c(52, 46, 38), c(112, 102, 86), c(182, 172, 152), c(230, 224, 206), c(255, 252, 240), MPart::Bone, Motion::Shatter); break;
    case Essence::Wind: set(c(30, 62, 62), c(72, 132, 122), c(142, 202, 182), c(212, 246, 232), c(250, 255, 250), MPart::Feather, Motion::Spiral); break;
    case Essence::Verdant: set(c(22, 52, 18), c(52, 112, 32), c(112, 182, 62), c(192, 236, 122), c(246, 255, 216), MPart::Leaf, Motion::Drift); break;
    case Essence::Star: set(c(12, 12, 44), c(44, 44, 124), c(112, 122, 222), c(222, 222, 255), c(255, 250, 222), MPart::Star, Motion::Fall); break;
    case Essence::Song: set(c(42, 32, 12), c(132, 102, 42), c(222, 192, 92), c(250, 236, 172), c(255, 255, 242), MPart::Note, Motion::Spiral); break;
    case Essence::Dream: set(c(60, 10, 60), c(160, 50, 160), c(240, 120, 220), c(255, 200, 245), c(255, 250, 255), MPart::Petal, Motion::Shimmer); break;
    case Essence::Rust: set(c(52, 22, 10), c(122, 52, 22), c(192, 102, 42), c(236, 172, 102), c(255, 232, 192), MPart::Spark, Motion::Pulse); break;
    default: set(c(22, 14, 52), c(72, 52, 172), c(132, 122, 242), c(192, 222, 255), c(250, 250, 255), MPart::Glyph, Motion::Spiral); break;
  }
  switch (s) {
    case School::Restoration: f.motion = Motion::Rise; if (f.particle == MPart::Spark) f.particle = MPart::Mote; break;
    case School::Conjuration: f.motion = Motion::Tear; break;
    case School::Illusion: f.motion = Motion::Shimmer; break;
    case School::Alteration: f.motion = Motion::Pulse; break;
    default: break;
  }
  f.glyphSeed = hmix((uint32_t)e * 977u + 13u, (uint32_t)s * 131u + 7u);
  f.motif = (uint8_t)((int)s * 7 + (int)e * 3);
  return f;
}

// ---- synthetic spells: the gallery and the staging scripts (script_magicfx.cpp) make any Form x Essence x School the
//      sim has no spell for yet, under ids the sim never hands out (SYNTH_BASE + n). Missiles carrying one fly through
//      the sim like any ProjKind::Spell; the view gives them their impact when they end (the sim knows no def for them).
constexpr uint32_t SYNTH_BASE = 0x7F000000u;
inline std::vector<magic::SpellDef>& synthSpells() { static std::vector<magic::SpellDef> v; return v; }
inline const magic::SpellDef* defOf(uint32_t id) {
  if (const magic::SpellDef* d = magic::spellById(id)) return d;
  if (id >= SYNTH_BASE && id - SYNTH_BASE < synthSpells().size()) return &synthSpells()[id - SYNTH_BASE];
  return nullptr;
}
inline bool isSynth(uint32_t id) { return id >= SYNTH_BASE && !magic::spellById(id); }
inline uint32_t makeSynth(Form f, Essence e, School s, int size, const std::string& name) {
  std::vector<magic::SpellDef>& v = synthSpells();
  for (size_t i = 0; i < v.size(); i++)
    if (v[i].form == f && v[i].ess == e && v[i].school == s && v[i].fx.size == (uint8_t)size) return SYNTH_BASE + (uint32_t)i;
  magic::SpellDef d;
  d.id = SYNTH_BASE + (uint32_t)v.size();
  d.name = name;
  d.school = s; d.form = f; d.ess = e;
  d.fx = essenceStyle(e, s);
  d.fx.size = (uint8_t)std::clamp(size, 1, 4);
  d.power = 1; d.cost = 0;
  d.speed = f == Form::Lance ? 280.0f : f == Form::Orb ? 140.0f : f == Form::Bolt || f == Form::Mark ? 220.0f : 0.0f;
  d.range = 200; d.radius = f == Form::Nova || f == Form::Storm || f == Form::Meteor ? 56.0f + 14.0f * (float)size : 24.0f;
  d.castTime = f == Form::Meteor || f == Form::Storm || size >= 3 ? 0.8f : f == Form::Summon ? 0.6f : 0.3f;
  switch (e) {
    case Essence::Fire: d.leaves = Surface::Fire; break;
    case Essence::Frost: d.leaves = Surface::Ice; break;
    case Essence::Tide: d.leaves = Surface::Water; break;
    case Essence::Venom: d.leaves = Surface::Poison; break;
    case Essence::Blood: d.leaves = Surface::Blood; break;
    default: break;
  }
  v.push_back(d);
  return d.id;
}
inline Look lookOf(const magic::SpellDef* d) {
  Look L;
  if (!d) {
    const magic::FxStyle f = essenceStyle(Essence::Arcane, School::Destruction);
    for (int i = 0; i < 5; i++) L.ramp[i] = f.ramp[i];
    L.part = f.particle; L.motion = f.motion; L.glyph = f.glyphSeed;
    return L;
  }
  L.spell = d->id; L.school = d->school; L.form = d->form; L.ess = d->ess; L.leaves = d->leaves;
  L.mods = d->mods; L.statuses = d->statuses; L.tradition = d->tradition; L.radius = d->radius;
  magic::FxStyle f = d->fx;
  bool any = false;
  for (uint32_t c : f.ramp) any |= (c & 0xFFFFFFu) != 0;
  if (!any) { const magic::FxStyle e = essenceStyle(d->ess, d->school); for (int i = 0; i < 5; i++) f.ramp[i] = e.ramp[i]; }
  for (int i = 0; i < 5; i++) L.ramp[i] = f.ramp[i];
  L.part = f.particle; L.motion = f.motion; L.glyph = f.glyphSeed ? f.glyphSeed : hmix(d->id, 0x51u);
  L.motif = f.motif; L.size = std::max<uint8_t>(1, f.size);
  return L;
}

// ------------------------------------------------------------------ baked sheets
// A sheet is a Canvas baked once (the game turns it into a texture the first time it is drawn; a stream sheet is
// re-uploaded when dirty: the surfaces' atlas). Keys: the kind in the top byte, its seed or ramp below.
struct Sheet {
  Canvas c;
  Tex tex;
  bool stream = false, dirty = true;
};
enum SheetKind : uint64_t { SK_COMMON = 1, SK_RUNES = 2, SK_SIGIL = 3, SK_RAMP = 4, SK_DECAL = 5, SK_SURF = 6 };
inline uint64_t sheetKey(SheetKind k, uint64_t v) { return (uint64_t)k << 56 | (v & 0x00FFFFFFFFFFFFFFull); }

// ---- the common sheet (white masks, tinted when drawn): 128 x 96
//   (0,0)   32x32 banded glow (four stepped rings, dithered at the steps: the 16-bit glow)
//   (32,0)  32x32 soft glow ((1-d)^2)
//   (64,0)  32x16 ground glow (an ellipse twice as wide as deep: the 3/4 view), banded
//   (64,16) 32x16 ground glow, soft
//   (0,32)  4 x 16x16 smoke puffs (lit top left, shaded bottom right; grey, tinted)
//   (64,32) 2 x 24x16 cloud puffs
//   (0,48)  9x9 four-point star, (9,48) 5x5 small star, (14,48) 3x3 cross, (17,48) 7x6 heart, (24,48) 5x7 note,
//           (29,48) 9x9 mask (fear), (38,48) 9x5 eye, (47,48) 7x7 hex cell, (54,48) 5x5 diamond
//   (0,64)  8 x 8x8 ember/spark sprites: 0 ember, 1 shard, 2 petal, 3 leaf, 4 feather, 5 bone, 6 droplet, 7 mote
constexpr int CS_W = 128, CS_H = 96;
struct Ico { int x, y, w, h; };
constexpr Ico I_GLOWB{0, 0, 32, 32}, I_GLOWS{32, 0, 32, 32}, I_GGLOWB{64, 0, 32, 16}, I_GGLOWS{64, 16, 32, 16};
constexpr Ico I_STAR{0, 48, 9, 9}, I_STARS{9, 48, 5, 5}, I_CROSS{14, 48, 3, 3}, I_HEART{17, 48, 7, 6}, I_NOTE{24, 48, 5, 7};
constexpr Ico I_MASK{29, 48, 9, 9}, I_EYE{38, 48, 9, 5}, I_HEX{47, 48, 7, 7}, I_DIAM{54, 48, 5, 5};
inline Ico iPuff(int i) { return Ico{(i & 3) * 16, 32, 16, 16}; }
inline Ico iCloud(int i) { return Ico{64 + (i & 1) * 24, 32, 24, 16}; }
inline Ico iSprite(int i) { return Ico{(i & 7) * 8, 64, 8, 8}; }

inline void stamp(Canvas& c, int x0, int y0, const char* const* rows, int h, uint32_t on, uint32_t mid) {
  for (int j = 0; j < h; j++)
    for (int i = 0; rows[j][i]; i++) {
      const char ch = rows[j][i];
      if (ch == '#') c.set(x0 + i, y0 + j, on);
      else if (ch == '+') c.set(x0 + i, y0 + j, mid);
    }
}
inline Canvas bakeCommon() {
  Canvas c(CS_W, CS_H);
  const uint32_t W = rgba(255, 255, 255);
  // the banded glow and the soft glow
  for (int y = 0; y < 32; y++)
    for (int x = 0; x < 32; x++) {
      const float d = std::sqrt((x - 15.5f) * (x - 15.5f) + (y - 15.5f) * (y - 15.5f)) / 16.0f;
      static const int band[5] = {255, 150, 72, 26, 0};
      int b = d < 0.25f ? 0 : d < 0.5f ? 1 : d < 0.75f ? 2 : d < 1.0f ? 3 : 4;
      const float fr = d * 4 - std::floor(d * 4);
      if (b < 4 && fr > 0.86f && ((x + y) & 1)) b++;   // dithered steps
      if (band[b]) c.set(x, y, rgba(255, 255, 255, band[b]));
      const float s = d < 1 ? (1 - d) * (1 - d) : 0;
      if (s > 0.004f) c.set(32 + x, y, rgba(255, 255, 255, (int)(255 * s)));
    }
  // the ground glows
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 32; x++) {
      const float u = (x - 15.5f) / 16.0f, v = (y - 7.5f) / 8.0f, d = std::sqrt(u * u + v * v);
      static const int band[5] = {200, 112, 50, 18, 0};
      int b = d < 0.3f ? 0 : d < 0.55f ? 1 : d < 0.8f ? 2 : d < 1.0f ? 3 : 4;
      if (band[b]) c.set(64 + x, y, rgba(255, 255, 255, band[b]));
      const float s = d < 1 ? (1 - d) * (1 - d) : 0;
      if (s > 0.004f) c.set(64 + x, 16 + y, rgba(255, 255, 255, (int)(230 * s)));
    }
  // smoke puffs: a union of three to four discs, lit from the top left
  for (int p = 0; p < 6; p++) {
    const bool cloud = p >= 4;
    const int w = cloud ? 24 : 16, h = 16, ox = cloud ? 64 + (p - 4) * 24 : p * 16, oy = 32;
    struct D { float x, y, r; } ds[5];
    const int nd = cloud ? 5 : 3 + (p & 1);
    for (int k = 0; k < nd; k++) {
      const uint32_t hh = hmix(0x5A0Eu + (uint32_t)p * 17u, (uint32_t)k);
      ds[k].x = (float)w * 0.5f + (hf(hh) - 0.5f) * (cloud ? 12.0f : 6.0f);
      ds[k].y = 8.5f + (hf(hh >> 7) - 0.5f) * 4.0f + (cloud ? 1.0f : 0.0f);
      ds[k].r = (cloud ? 4.5f : 3.6f) + hf(hh >> 13) * 2.2f;
    }
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        float best = -1;
        for (int k = 0; k < nd; k++) {
          const float dx = x + 0.5f - ds[k].x, dy = y + 0.5f - ds[k].y;
          const float in = ds[k].r - std::sqrt(dx * dx + dy * dy);
          if (in > best) best = in;
        }
        if (best < 0) continue;
        // lit side: the top-left of the puff; shaded: the bottom-right
        const float lx = (x + 0.5f - (float)w * 0.5f) / (float)w, ly = (y + 0.5f - 8.0f) / 16.0f;
        const float lit = -(lx + ly);
        int g = lit > 0.12f ? 236 : lit > -0.05f ? 196 : lit > -0.2f ? 160 : 128;
        const int a = best < 1.0f ? 120 : best < 2.0f ? 200 : 236;
        c.set(ox + x, oy + y, rgba(g, g, g, a));
      }
  }
  // icons
  {
    static const char* star[9] = {"....#....", "....#....", "...+#+...", "..+###+..", "######## ", "..+###+..", "...+#+...", "....#....", "....#...."};
    stamp(c, 0, 48, star, 9, W, rgba(255, 255, 255, 140));
    c.set(8, 52, W);   // (the row's last pixel: the star's right arm)
    static const char* sstar[5] = {"..#..", ".+#+.", "#####", ".+#+.", "..#.."};
    stamp(c, 9, 48, sstar, 5, W, rgba(255, 255, 255, 140));
    static const char* cross[3] = {".#.", "###", ".#."};
    stamp(c, 14, 48, cross, 3, W, W);
    static const char* heart[6] = {".##.##.", "#######", "#######", ".#####.", "..###..", "...#..."};
    stamp(c, 17, 48, heart, 6, W, W);
    static const char* note[7] = {"..##.", "..#.#", "..#..", "..#..", "###..", "###..", ".#..."};
    stamp(c, 24, 48, note, 7, W, W);
    static const char* mask[9] = {".#######.", "#########", "##..#..##", "#+..#..+#", "#########", ".###.###.", ".#.#.#.#.", "..#####..", "...+++..."};
    stamp(c, 29, 48, mask, 9, W, rgba(255, 255, 255, 120));
    static const char* eye[5] = {"..#####..", ".#.....#.", "#...#...#", ".#.....#.", "..#####.."};
    stamp(c, 38, 48, eye, 5, W, W);
    static const char* hex[7] = {"..###..", ".#...#.", "#.....#", "#.....#", "#.....#", ".#...#.", "..###.."};
    stamp(c, 47, 48, hex, 7, W, W);
    static const char* diam[5] = {"..#..", ".###.", "#####", ".###.", "..#.."};
    stamp(c, 54, 48, diam, 5, W, W);
  }
  // 8x8 particle sprites (white body '#', a dimmer rim '+')
  {
    static const char* spr[8][8] = {
      {"........", "...+....", "..+#+...", ".+###+..", "..+#+...", "...+....", "........", "........"},   // ember
      {"........", "......#.", ".....##.", "....##+.", "...##+..", "..#+....", "........", "........"},   // shard
      {"........", "..++....", ".+##+...", ".+###+..", "..+##+..", "...++...", "........", "........"},   // petal
      {"........", "....+#..", "...+##..", "..+##+..", ".+##+...", ".#+.....", "#.......", "........"},   // leaf
      {"........", "......+#", ".....+#+", "...+##+.", ".+##+...", "+#+.....", "#.......", "........"},   // feather
      {"........", "........", ".#....#.", "+######+", ".#....#.", "........", "........", "........"},   // bone
      {"........", "...+....", "...#....", "..+#+...", "..###...", "..+#+...", "........", "........"},   // droplet
      {"........", "........", "...++...", "..+##+..", "..+##+..", "...++...", "........", "........"},   // mote
    };
    for (int s = 0; s < 8; s++) stamp(c, s * 8, 64, spr[s], 8, W, rgba(255, 255, 255, 150));
  }
  return c;
}

// ---- runes (a glyph seed's eight runes, 9 x 9 cells in one row: strokes on a 3 x 3 lattice, a stave and branches,
//      with a faint halo) and the ground sigil (8 frames of 64 x 32, turning: rings, a star polygon, runelets)
constexpr int RUNE = 9;
inline void lineC(Canvas& c, int x0, int y0, int x1, int y1, uint32_t col) {
  int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
  for (int n = 0; n < 256; n++) {
    c.set(x0, y0, col);
    if (x0 == x1 && y0 == y1) break;
    const int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}
inline Canvas bakeRunes(uint32_t seed) {
  Canvas c(RUNE * 8, RUNE);
  for (int r = 0; r < 8; r++) {
    Canvas g(RUNE, RUNE);
    const uint32_t h = hmix(seed, (uint32_t)r * 0x9E37u + 1u);
    static const int L[3] = {1, 4, 7};
    const int sx = (h & 3) == 0 ? 1 : (h & 3) == 1 ? 7 : 4;
    lineC(g, sx, 1, sx, 7, rgba(255, 255, 255));
    const int nb = 1 + (int)((h >> 3) % 3);
    for (int b = 0; b < nb; b++) {
      const uint32_t hb = hmix(h, (uint32_t)b + 11u);
      const int fy = L[hb % 3], tx = L[(hb >> 3) % 3], ty = L[(hb >> 6) % 3];
      if (tx == sx && ty == fy) continue;
      lineC(g, sx, fy, tx == sx ? (sx == 4 ? 7 : 4) : tx, ty, rgba(255, 255, 255));
    }
    if ((h >> 12) & 1) g.set(sx + ((h >> 13) & 1 ? 2 : -2), 4, rgba(255, 255, 255));   // a dot
    for (int y = 0; y < RUNE; y++)
      for (int x = 0; x < RUNE; x++) {
        if (g.get(x, y)) { c.set(r * RUNE + x, y, rgba(255, 255, 255)); continue; }
        bool near = false;
        for (int j = -1; j <= 1 && !near; j++) for (int i = -1; i <= 1; i++) if (g.get(x + i, y + j) >> 24) { near = true; break; }
        if (near) c.set(r * RUNE + x, y, rgba(255, 255, 255, 70));
      }
  }
  return c;
}
constexpr int SIG_W = 64, SIG_H = 32, SIG_FR = 8;
inline Canvas bakeSigil(uint32_t seed) {
  Canvas c(SIG_W * SIG_FR, SIG_H);
  const int k = 5 + (int)(seed % 2);            // a pentagram or a hexagram
  const int skip = k == 5 ? 2 : 2;
  const int nlets = 2 * k;
  const uint32_t ring = rgba(255, 255, 255, 230), poly = rgba(255, 255, 255, 150), let = rgba(255, 255, 255);
  for (int f = 0; f < SIG_FR; f++) {
    const int ox = f * SIG_W;
    const float rot = (float)f / (float)SIG_FR * kTau / (float)k;
    auto P = [&](float a, float r) { return Vec2(32.0f + std::cos(a) * 30.0f * r, 16.0f + std::sin(a) * 14.0f * r); };
    for (int i = 0; i < 220; i++) {   // the rings
      const float a = kTau * (float)i / 220.0f;
      const Vec2 p0 = P(a, 1.0f), p1 = P(a, 0.78f), p2 = P(a, 0.27f);
      c.set(ox + (int)p0.x, (int)p0.y, ring);
      c.set(ox + (int)p1.x, (int)p1.y, ring);
      if (i % 2 == 0) c.set(ox + (int)p2.x, (int)p2.y, poly);
    }
    for (int i = 0; i < k; i++) {     // the star polygon
      const Vec2 a = P(rot + kTau * (float)i / (float)k - kPi / 2, 0.62f), b = P(rot + kTau * (float)((i + skip) % k) / (float)k - kPi / 2, 0.62f);
      lineC(c, ox + (int)a.x, (int)a.y, ox + (int)b.x, (int)b.y, poly);
    }
    for (int i = 0; i < nlets; i++) {   // the runelets between the rings: three-pixel marks from the seed
      const float a = -rot * 0.5f + kTau * (float)i / (float)nlets;
      const Vec2 p = P(a, 0.89f);
      const uint32_t h = hmix(seed, (uint32_t)i);
      const int x = ox + (int)p.x, y = (int)p.y;
      c.set(x, y, let);
      switch (h % 4) {
        case 0: c.set(x, y - 1, let); c.set(x, y + 1, let); break;
        case 1: c.set(x - 1, y, let); c.set(x + 1, y - 1, let); break;
        case 2: c.set(x - 1, y - 1, let); c.set(x + 1, y + 1, let); break;
        default: c.set(x, y - 1, let); c.set(x + 1, y, let); break;
      }
    }
  }
  return c;
}

// ---- a ramp's sheet: flame tongues (3 sizes x 8 frames), cores (2 sizes x 6 frames), the burst (8 frames of 48 x 48)
struct FlameCell { int y, w, h; };
constexpr FlameCell FLAME[3] = {{0, 7, 11}, {11, 9, 15}, {26, 13, 21}};
constexpr int FLAME_FR = 8, CORE_FR = 6, CORE_Y[2] = {47, 56}, CORE_S[2] = {9, 15}, BURST_Y = 71, BURST = 48, BURST_FR = 8;
constexpr int RAMP_W = BURST * BURST_FR, RAMP_H = BURST_Y + BURST;
inline uint32_t rampPx(const uint32_t* ramp, float heat, int a = 255) {
  const int i = heat > 0.8f ? 4 : heat > 0.6f ? 3 : heat > 0.4f ? 2 : heat > 0.2f ? 1 : 0;
  return (ramp[i] & 0xFFFFFFu) | (uint32_t)a << 24;
}
inline Canvas bakeRamp(const uint32_t* ramp, Motion motion) {
  Canvas c(RAMP_W, RAMP_H);
  // flame tongues: rows narrowing upward, swaying, the tip tearing off on some frames; lit left edge, shaded right
  for (int s = 0; s < 3; s++) {
    const FlameCell& F = FLAME[s];
    for (int f = 0; f < FLAME_FR; f++) {
      const int ox = f * F.w;
      const float ph = (float)f / FLAME_FR * kTau;
      for (int j = 0; j < F.h; j++) {
        const float t = (float)j / (float)(F.h - 1);   // 0 base .. 1 tip
        if (t > 0.72f && t < 0.8f && (f & 3) == 1) continue;   // the tip tearing away
        const float hw = (float)F.w * 0.5f * std::pow(1.0f - t, 1.25f) * (1.0f + 0.14f * std::sin(ph * 2 + t * 7)) * (j == 0 ? 0.85f : 1.0f);
        if (hw < 0.45f) continue;
        const float cx = (float)F.w * 0.5f + std::sin(ph + t * 4.0f) * 1.3f * t * t * (float)(s + 1) * 0.7f;
        const int x0 = (int)std::floor(cx - hw + 0.5f), x1 = (int)std::floor(cx + hw - 0.5f);
        for (int x = x0; x <= x1; x++) {
          const float dx = std::fabs((float)x + 0.5f - cx) / std::max(0.5f, hw);
          float heat = 1.05f - t * 0.95f - dx * 0.45f;
          if (x == x0) heat += 0.12f;            // the lit edge (top-left light)
          if (x == x1 && x1 > x0) heat -= 0.2f;  // the shade side
          const int a = t > 0.85f ? 170 : 255;
          c.set(ox + x, F.y + (F.h - 1 - j), rampPx(ramp, heat, a));
        }
      }
    }
  }
  // cores: a lumpy disc, boiling (fire), faceted (frost), crackling (storm) or smooth (the rest), hot toward the top left
  for (int s = 0; s < 2; s++) {
    const int S = CORE_S[s];
    const float R = (float)S * 0.5f - 0.3f;
    for (int f = 0; f < CORE_FR; f++) {
      const int ox = f * 15;
      const float ph = (float)f / CORE_FR * kTau;
      for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
          const float dx = x + 0.5f - S * 0.5f, dy = y + 0.5f - S * 0.5f;
          const float d = std::sqrt(dx * dx + dy * dy), an = std::atan2(dy, dx);
          float rr = R;
          if (motion == Motion::Boil) rr *= 0.86f + 0.14f * std::sin(an * 5 + ph) * std::sin(an * 3 - ph * 2);
          else if (motion == Motion::Shatter) rr *= 0.8f + 0.2f * std::fabs(std::cos(an * 3 + ph * 0.5f));
          else if (motion == Motion::Fork) rr *= 0.8f + 0.25f * (hf(hmix((uint32_t)(an * 6 + 7), (uint32_t)f)) - 0.3f);
          else rr *= 0.93f + 0.07f * std::sin(an * 4 + ph);
          if (d > rr) continue;
          const float hx = dx + R * 0.3f, hy = dy + R * 0.3f;
          float heat = 1.0f - std::sqrt(hx * hx + hy * hy) / (R * 1.35f);
          if (motion == Motion::Boil) heat += 0.12f * std::sin(dx * 1.7f + ph) * std::sin(dy * 1.9f - ph);
          if (motion == Motion::Shatter && (std::fabs(dx - dy) < 0.6f)) heat += 0.25f;   // a facet's lit ridge
          const int a = d > rr - 1.0f ? 220 : 255;
          c.set(ox + x, CORE_Y[s] + y, rampPx(ramp, 0.12f + heat * 0.95f, a));
        }
    }
  }
  // the burst: a cluster of round lobes that bloom out and rise, banded hot to dark with a crisp deep-red rim and a hot
  // spot toward the top left (the light); one by one the lobes cool into smoke puffs (lit top-left, shaded bottom-right)
  // that drift higher and thin out, so the bloom breaks up into its own clouds (an energy burst: the lobes just fade)
  struct Lobe { float ax, ay, r, fireT; };
  Lobe lobes[8];
  for (int k = 0; k < 8; k++) {
    const uint32_t h = hmix(0xB1A5u, (uint32_t)k);
    const float an = k == 0 ? 0.0f : kTau * (float)(k - 1) / 7.0f + hf(h) * 0.6f;
    const float rad = k == 0 ? 0.0f : 0.55f + 0.35f * hf(h >> 6);
    lobes[k] = Lobe{std::cos(an) * rad, std::sin(an) * rad * 0.8f - (k == 0 ? 0.0f : 0.15f), k == 0 ? 1.0f : 0.6f + 0.3f * hf(h >> 12), 0.55f + 0.25f * hf(h >> 18) + (k == 0 ? 0.12f : 0.0f)};
  }
  const bool smoky = motion == Motion::Boil;
  for (int f = 0; f < BURST_FR; f++) {
    const int ox = f * BURST;
    const float u = ((float)f + 0.5f) / (float)BURST_FR;
    const float grow = std::sqrt(std::min(1.0f, u * 1.7f));
    const float R = 7.0f + 10.0f * grow;               // the cluster's radius (px)
    const float rise = u * 10.0f;
    for (int y = 0; y < BURST; y++)
      for (int x = 0; x < BURST; x++) {
        float bestF = -1, bestS = -1;
        float fdx = 0, fdy = 0, fr = 1, sdx = 0, sdy = 0, sr = 1;
        for (int k = 0; k < 8; k++) {
          const Lobe& L = lobes[k];
          const bool fire = u < L.fireT;
          float r = R * 0.62f * L.r * (fire ? 1.0f : std::max(0.0f, 0.8f - (u - L.fireT) * 1.6f));
          if (!fire && !smoky) r *= std::max(0.0f, 1.0f - (u - L.fireT) * 3.0f);
          if (r < 0.8f) continue;
          const float cx = 24.0f + L.ax * R * (0.6f + 0.5f * u), cy = 30.0f - rise - (fire ? 0.0f : (u - L.fireT) * 14.0f) + L.ay * R;
          const float dx = x + 0.5f - cx, dy = (y + 0.5f - cy) * 1.1f;
          const float sIn = 1.0f - std::sqrt(dx * dx + dy * dy) / r;
          if (sIn <= 0) continue;
          if (fire) { if (sIn > bestF) { bestF = sIn; fdx = dx; fdy = dy; fr = r; } }
          else if (sIn > bestS) { bestS = sIn; sdx = dx; sdy = dy; sr = r; }
        }
        if (bestF > 0) {   // fire: bands by the distance from the lobe's hot spot (up-left of its centre)
          const float hx = fdx + fr * 0.28f, hy = fdy + fr * 0.28f;
          const float d = std::sqrt(hx * hx + hy * hy) / (fr * 1.25f);
          float heat = 1.2f - d * 0.95f - u * 0.45f;
          if (bestF < 0.12f) heat = std::min(heat, 0.3f);                     // the crisp rim
          const float q = heat * 5.0f;
          if (q - std::floor(q) < 0.14f && ((x + y) & 1)) heat -= 0.2f;      // a checker on each band's edge
          c.set(ox + x, BURST_Y + y, rampPx(ramp, heat, 255));
        } else if (bestS > 0 && smoky) {   // smoke: three greys lit from the top left, a darker rim, thinning with age
          const float lit = -(sdx + sdy) / (sr * 1.4f);
          const int g = bestS < 0.15f ? 92 : lit > 0.25f ? 168 : lit > -0.15f ? 136 : 110;
          const uint32_t base = ramp[0];
          const int r = (int)((float)g * 0.82f + (float)(base & 255) * 0.18f), gg = (int)((float)g * 0.82f + (float)((base >> 8) & 255) * 0.18f),
                    b = (int)((float)g * 0.82f + (float)((base >> 16) & 255) * 0.18f) + 4;
          const int al = (int)(200.0f * std::min(1.0f, (1.0f - u) * 2.0f));
          if (al < 40 || (al < 120 && ((x + y) & 1))) continue;
          c.set(ox + x, BURST_Y + y, rgba(r, gg, b, al));
        }
      }
  }
  return c;
}

// ---- decals: what a spell leaves on the ground (scorch, frost, cracks, crater, puddle, stain, shadow, petals, blood,
//      bones, stardust, a burned sigil, ash), 64 x 32 (the crater 96 x 48), the 3/4 view's flattened ground
enum DecalKind : uint8_t { D_SCORCH, D_FROST, D_CRACKS, D_CRATER, D_PUDDLE, D_STAIN, D_SHADOW, D_PETALS, D_BLOOD, D_BONES,
                           D_STARDUST, D_SIGIL, D_ASH, D_COUNT };
inline int decalW(int k) { return k == D_CRATER ? 96 : 64; }
inline int decalH(int k) { return k == D_CRATER ? 48 : 32; }
inline DecalKind decalFor(const Look& L) {
  switch (L.ess) {
    case Essence::Fire: return D_SCORCH;
    case Essence::Frost: return D_FROST;
    case Essence::Storm: return D_CRACKS;
    case Essence::Stone: case Essence::Rust: return L.form == Form::Meteor || L.size >= 3 ? D_CRATER : D_ASH;
    case Essence::Tide: return D_PUDDLE;
    case Essence::Venom: return D_STAIN;
    case Essence::Shadow: return D_SHADOW;
    case Essence::Light: case Essence::Verdant: case Essence::Song: case Essence::Dream: return D_PETALS;
    case Essence::Blood: return D_BLOOD;
    case Essence::Bone: return D_BONES;
    case Essence::Star: return D_STARDUST;
    case Essence::Wind: return D_ASH;
    default: return D_SIGIL;
  }
}
inline Canvas bakeDecal(int kind, uint32_t seed, const uint32_t* ramp) {
  const int W = decalW(kind), H = decalH(kind);
  Canvas c(W, H);
  const float cx = W * 0.5f, cy = H * 0.5f, RX = W * 0.36f, RY = H * 0.36f;
  Rnd r{seed | 1u};
  auto inside = [&](float x, float y, float k) {
    const float dx = (x - cx) / (RX * k), dy = (y - cy) / (RY * k);
    const float an = std::atan2(dy, dx);
    const float lump = 1.0f + 0.22f * std::sin(an * 3 + hf(seed) * 6) * std::sin(an * 5 + hf(seed >> 8) * 6);
    return std::sqrt(dx * dx + dy * dy) / lump;
  };
  auto blend = [&](int x, int y, uint32_t col) {   // over what is there
    const uint32_t o = c.get(x, y);
    const int ao = (int)(o >> 24), an = (int)(col >> 24);
    if (!ao || an >= 250) { c.set(x, y, col); return; }
    const float t = an / 255.0f;
    auto ch = [&](int s) { return (int)(((o >> s) & 255) * (1 - t) + ((col >> s) & 255) * t); };
    c.set(x, y, rgba(ch(0), ch(8), ch(16), std::max(ao, an)));
  };
  const Color c1 = cu(ramp[1]), c2 = cu(ramp[2]), c3 = cu(ramp[3]);
  switch (kind) {
    case D_SCORCH: case D_ASH: case D_SHADOW: case D_STAIN: case D_PUDDLE: case D_BLOOD: {
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
          const float d = inside(x + 0.5f, y + 0.5f, 1.0f);
          const float n = vnoise(x * 0.35f, y * 0.5f, seed);
          if (d > 1.0f + (n - 0.5f) * 0.35f) continue;
          const float k = sat(1.0f - d);
          switch (kind) {
            case D_SCORCH: {
              const int g = 16 + (int)(n * 14);
              c.set(x, y, rgba(g + 8, g, g - 4, (int)(90 + 130 * k)));
              if (n > 0.72f && k > 0.3f) c.set(x, y, rgba(70, 62, 58, 160));   // ash
              if (n < 0.2f && k > 0.45f) c.set(x, y, pk(withA(mulC(c1, 0.75f), 0.75f)));   // a cinder (the glow is drawn live)
              break;
            }
            case D_ASH: { const int g = 80 + (int)(n * 40); c.set(x, y, rgba(g, g - 6, g - 12, (int)(50 + 90 * k))); break; }
            case D_SHADOW: c.set(x, y, rgba(30, 16, 42, (int)(70 + 120 * k))); if (n > 0.75f) c.set(x, y, pk(withA(c1, 0.6f))); break;
            case D_STAIN: c.set(x, y, rgba(54, 92, 26, (int)(60 + 110 * k))); if (n > 0.7f) c.set(x, y, rgba(100, 150, 40, 170)); if (n < 0.18f) c.set(x, y, rgba(28, 48, 14, 170)); break;
            case D_PUDDLE: c.set(x, y, rgba(34, 78, 124, (int)(70 + 100 * k))); if (k < 0.12f) c.set(x, y, rgba(20, 40, 70, 140)); else if (n > 0.8f) c.set(x, y, rgba(160, 210, 236, 200)); break;
            default: {   // blood: a pool and droplets
              c.set(x, y, rgba(96 + (int)(n * 30), 10, 14, (int)(140 + 100 * k)));
              if (n > 0.82f && k > 0.4f) c.set(x, y, rgba(190, 60, 60, 220));
              break;
            }
          }
        }
      if (kind == D_SCORCH)   // the blast's streaks out over the ground
        for (int s = 0; s < 9; s++) {
          const float an = kTau * (float)s / 9.0f + r.r(-0.2f, 0.2f), l = r.r(1.05f, 1.45f);
          for (float t = 0.5f; t < l; t += 0.04f) {
            const int x = (int)(cx + std::cos(an) * RX * t), y = (int)(cy + std::sin(an) * RY * t);
            blend(x, y, rgba(22, 16, 12, (int)(110 * (1.0f - (t - 0.5f) / l))));
          }
        }
      if (kind == D_BLOOD)
        for (int s = 0; s < 10; s++) {
          const float an = r.r(0, kTau), d = r.r(1.0f, 1.5f);
          const int x = (int)(cx + std::cos(an) * RX * d), y = (int)(cy + std::sin(an) * RY * d);
          blend(x, y, rgba(110, 12, 16, 230)); blend(x + 1, y, rgba(80, 8, 10, 200));
        }
      break;
    }
    case D_FROST: {   // a frost star: crystalline rays with fern branches over a pale rime
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
          const float d = inside(x + 0.5f, y + 0.5f, 0.8f);
          if (d < 1.0f) c.set(x, y, rgba(220, 240, 255, (int)(40 + 50 * (1 - d))));
        }
      const int rays = 7 + (int)(seed % 3);
      for (int s = 0; s < rays; s++) {
        const float an = kTau * (float)s / (float)rays + r.r(-0.15f, 0.15f), l = r.r(0.85f, 1.3f);
        Vec2 prev(cx, cy);
        for (float t = 0.05f; t < l; t += 0.03f) {
          const Vec2 p(cx + std::cos(an) * RX * t, cy + std::sin(an) * RY * t);
          blend((int)p.x, (int)p.y + 1, rgba(110, 160, 214, 150));
          blend((int)p.x, (int)p.y, rgba(232, 246, 255, 235));
          if (std::fmod(t, 0.24f) < 0.03f && t > 0.2f)   // a twig either side
            for (int side = -1; side <= 1; side += 2) {
              const float ba = an + side * 1.0f;
              for (float q = 0; q < 0.22f * (l - t + 0.3f); q += 0.035f) {
                const int bx = (int)(p.x + std::cos(ba) * RX * q), by = (int)(p.y + std::sin(ba) * RY * q);
                blend(bx, by, rgba(200, 232, 255, 200));
              }
            }
          prev = p;
        }
      }
      break;
    }
    case D_CRACKS: {   // storm: a small scorch and branching cracks, their edges tinged with the bolt's colour
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
          const float d = inside(x + 0.5f, y + 0.5f, 0.55f);
          if (d < 1.0f) c.set(x, y, rgba(26, 20, 28, (int)(60 + 120 * (1 - d))));
        }
      for (int s = 0; s < 6; s++) {
        float an = kTau * (float)s / 6.0f + r.r(-0.3f, 0.3f);
        float x = cx, y = cy;
        const int n = 10 + (int)r.r(0, 10);
        for (int i = 0; i < n; i++) {
          an += r.r(-0.6f, 0.6f);
          const float nx = x + std::cos(an) * 2.0f, ny = y + std::sin(an) * 1.0f;
          lineC(c, (int)x, (int)y, (int)nx, (int)ny, rgba(14, 10, 18, 230));
          blend((int)nx, (int)ny - 1, pk(withA(c2, 0.45f)));
          x = nx; y = ny;
        }
      }
      break;
    }
    case D_CRATER: {   // a crater: the raised rim lit on its top left, the inner wall shaded top-left and lit bottom-right,
                       // a dark floor with glowing seams, rock chips flung round it
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
          const float d = inside(x + 0.5f, y + 0.5f, 1.0f);
          const float n = vnoise(x * 0.4f, y * 0.6f, seed);
          const float dx = (x + 0.5f - cx) / RX, dy = (y + 0.5f - cy) / RY;
          const float lit = -(dx + dy) / std::max(0.2f, std::sqrt(dx * dx + dy * dy));
          if (d < 0.62f) {   // the floor
            const int g = 30 + (int)(n * 16);
            c.set(x, y, rgba(g + 6, g, g - 4, 235));
            if (n > 0.72f) c.set(x, y, pk(withA(mulC(c1, 0.8f), 0.9f)));
          } else if (d < 0.86f) {   // the inner wall: shaded where it faces away from the light
            const int g = lit > 0.2f ? 46 : lit < -0.2f ? 120 : 82;
            c.set(x, y, rgba(g + 18, g + 8, g, 240));
          } else if (d < 1.0f) {    // the rim
            const int g = lit > 0.2f ? 168 : lit < -0.2f ? 92 : 128;
            c.set(x, y, rgba(g, g - 22, g - 44, 235));
          } else if (d < 1.25f + (n - 0.5f) * 0.3f) {   // scorched ground and the ejecta
            c.set(x, y, rgba(36, 28, 22, (int)(160 * (1.25f - d) * 4 > 160 ? 160 : 160 * (1.25f - d) * 4)));
          }
        }
      for (int s = 0; s < 22; s++) {
        const float an = r.r(0, kTau), d = r.r(1.1f, 1.45f);
        const int x = (int)(cx + std::cos(an) * RX * d), y = (int)(cy + std::sin(an) * RY * d);
        blend(x, y, rgba(150, 128, 104, 255)); blend(x + 1, y, rgba(96, 80, 64, 255)); blend(x, y + 1, rgba(60, 48, 40, 200));
      }
      break;
    }
    case D_PETALS: case D_STARDUST: case D_BONES: {
      const int n = kind == D_BONES ? 14 : 26;
      for (int y = 0; y < H; y++)   // a faint halo on the ground
        for (int x = 0; x < W; x++) {
          const float d = inside(x + 0.5f, y + 0.5f, 0.9f);
          if (d < 1.0f && kind != D_BONES) c.set(x, y, pk(withA(c2, 0.08f + 0.1f * (1 - d))));
        }
      for (int s = 0; s < n; s++) {
        const float an = r.r(0, kTau), d = std::sqrt(r.f()) * 1.1f;
        const int x = (int)(cx + std::cos(an) * RX * d), y = (int)(cy + std::sin(an) * RY * d);
        if (kind == D_PETALS) {
          const Color pc = (s % 3 == 0) ? Color(0.36f, 0.6f, 0.22f) : (s & 1) ? c3 : c2;
          blend(x, y, pk(pc)); blend(x + 1, y, pk(mulC(pc, 0.7f)));
          if (s % 3) blend(x, y - 1, pk(mulC(pc, 1.15f)));
        } else if (kind == D_STARDUST) {
          blend(x, y, pk(withA(c3, 0.9f)));
          if (s % 4 == 0) { blend(x - 1, y, pk(withA(c2, 0.6f))); blend(x + 1, y, pk(withA(c2, 0.6f))); }
        } else {
          blend(x, y, rgba(226, 218, 196)); blend(x + 1, y, rgba(196, 186, 162)); blend(x + 2, y, rgba(226, 218, 196));
          blend(x + 1, y + 1, rgba(70, 60, 50, 160));
        }
      }
      break;
    }
    default: {   // a burned sigil: the ring of runes scorched into the ground, tinged with the spell's colour
      const Canvas sg = bakeSigil(seed);
      for (int y = 0; y < SIG_H; y++)
        for (int x = 0; x < SIG_W; x++) {
          const uint32_t p = sg.get(x, y);
          if (!(p >> 24)) continue;
          blend(x + (W - SIG_W) / 2, y + (H - SIG_H) / 2 + 1, rgba(30, 22, 22, 150));
          blend(x + (W - SIG_W) / 2, y + (H - SIG_H) / 2, pk(withA(mulC(c1, 0.9f), 0.7f)));
        }
      break;
    }
  }
  return c;
}

// ------------------------------------------------------------------ the world: pooled effects, particles, decals
enum PartKind : uint8_t { PK_EMBER, PK_SHARD, PK_PETAL, PK_GLYPH, PK_NOTE, PK_STAR, PK_DROP, PK_SMOKE, PK_SPARK, PK_MOTE,
                          PK_BONE, PK_LEAF, PK_FEATHER, PK_ROCK, PK_PUFF, PK_HEART, PK_COUNT };
inline PartKind partOf(MPart p) {
  switch (p) {
    case MPart::Ember: return PK_EMBER;
    case MPart::Shard: return PK_SHARD;
    case MPart::Petal: return PK_PETAL;
    case MPart::Glyph: return PK_GLYPH;
    case MPart::Note: return PK_NOTE;
    case MPart::Star: return PK_STAR;
    case MPart::Droplet: return PK_DROP;
    case MPart::Smoke: return PK_SMOKE;
    case MPart::Spark: return PK_SPARK;
    case MPart::Mote: return PK_MOTE;
    case MPart::Bone: return PK_BONE;
    case MPart::Leaf: return PK_LEAF;
    case MPart::Feather: return PK_FEATHER;
    default: return PK_SPARK;
  }
}
enum PartFlag : uint8_t { PF_ADD = 1, PF_Z = 2, PF_DIM = 4, PF_INWARD = 8, PF_FLICKER = 16 };
struct Part {
  Vec2 p, v;                     // p: the ground point under it when PF_Z (z its height), else its point in the air
  float z = 0, vz = 0, life = 1, max = 1, size = 1, grav = 0, drag = 0, spin = 0;
  uint32_t c0 = 0, c1 = 0, c2 = 0;   // dark, body, hot
  uint32_t glyph = 0;            // PK_GLYPH: the rune seed and index
  uint8_t kind = 0, flags = 0;
  uint16_t pad = 0;
};
enum FxKind : uint8_t {          // the Phase values, then the view's own
  FX_METEOR = 20,                // a meteor falling to its mark (q), then the spectacle's landing
  FX_STRIKE = 21,                // a bolt from the sky to p
  FX_STORMCELL = 22,             // a storm cell over p: a dark cloud, strikes, rain
  FX_STARS = 23,                 // a constellation drawn in the sky over p, star-lines at night, a falling star to p
  FX_TRAILFLASH = 24             // a missile's lit body after it ends: a short afterglow
};
struct Fx {
  uint8_t kind = 0, sub = 0;     // sub: a status / surface index (StatusOn, Combo), a strike's index
  Look L;
  Vec2 p, q, v;
  float t = 0, life = 0.5f, mag = 1, delay = 0, charge = 0;   // delay: seconds before it starts; charge: wind-up level
  uint32_t follow = 0, seed = 0;
  bool hit = false;              // FX_METEOR: landed
};
struct Decal { Vec2 p; float t = 0, life = 12, size = 1; uint8_t kind = 0; uint32_t seed = 0; uint32_t ramp[5] = {}; Look L; };
struct SoundReq { Sfx s = Sfx::SpellHum; float pitch = 1, vol = 1; bool motif = false; uint8_t motifId = 0, timbre = 0; };

struct Caps { int parts = 900, fx = 160, decals = 64; };
inline Caps capsFor(bool phone) { return phone ? Caps{560, 120, 40} : Caps{900, 160, 64}; }

struct World {
  std::vector<Fx> fx;
  std::vector<Part> parts;
  std::vector<Decal> decals;
  std::unordered_map<uint64_t, std::unique_ptr<Sheet>> sheets;
  Rnd rnd;
  float t = 0;
  Caps caps;
  // requests for the host this frame (it reads and clears them)
  float shake = 0;                       // a screen kick (px)
  float flashA = 0;                      // the scene flash (additive, full screen, this frame)
  Color flashC = Color(1, 1, 1);
  float lift = 0;                        // the light map's lift (lightning: the night lit for a frame)
  Color liftC = Color(1, 1, 1);
  std::vector<SoundReq> sounds;
  // counters (scripts, the perf report)
  int dropped = 0, spawned = 0, events = 0;

  World() { fx.reserve(160); parts.reserve(900); decals.reserve(64); }

  Sheet& sheet(uint64_t key, const std::function<Canvas()>& bake) {
    auto it = sheets.find(key);
    if (it != sheets.end()) return *it->second;
    auto s = std::make_unique<Sheet>();
    s->c = bake();
    Sheet& r = *s;
    sheets.emplace(key, std::move(s));
    return r;
  }
  Sheet& common() { return sheet(sheetKey(SK_COMMON, 0), [] { return bakeCommon(); }); }
  Sheet& runes(uint32_t seed) { return sheet(sheetKey(SK_RUNES, seed), [seed] { return bakeRunes(seed); }); }
  Sheet& sigil(uint32_t seed) { return sheet(sheetKey(SK_SIGIL, seed), [seed] { return bakeSigil(seed); }); }
  Sheet& rampSheet(const Look& L) {
    const Motion m = L.motion;
    uint32_t r[5];
    for (int i = 0; i < 5; i++) r[i] = L.ramp[i];
    return sheet(sheetKey(SK_RAMP, (uint64_t)L.rampKey() << 8 | (uint64_t)m), [r, m] { return bakeRamp(r, m); });
  }
  Sheet& decalSheet(int kind, uint32_t seed, const uint32_t* ramp) {
    uint32_t rk = 0x811C9DC5u;
    for (int i = 0; i < 5; i++) rk = hmix(rk, ramp[i]);
    uint32_t r[5];
    for (int i = 0; i < 5; i++) r[i] = ramp[i];
    const uint32_t v = seed & 3;
    return sheet(sheetKey(SK_DECAL, (uint64_t)rk << 12 | (uint64_t)kind << 4 | v), [kind, v, r] { return bakeDecal(kind, 0xDECA1u + v * 7919u + (uint32_t)kind * 31u, r); });
  }

  // ---- pools
  Part* spawn(PartKind k, Vec2 p, Vec2 v, float life, uint32_t c0, uint32_t c1, uint32_t c2, float size = 1, float grav = 0, uint8_t flags = PF_ADD) {
    if ((int)parts.size() >= caps.parts) { dropped++; return nullptr; }
    Part q;
    q.kind = k; q.p = p; q.v = v; q.life = q.max = life; q.c0 = c0; q.c1 = c1; q.c2 = c2; q.size = size; q.grav = grav; q.flags = flags;
    q.spin = rnd.r(0, kTau);
    parts.push_back(q);
    spawned++;
    return &parts.back();
  }
  Fx* addFx(uint8_t kind, const Look& L, Vec2 p, Vec2 q, float life, float mag, uint32_t follow = 0) {
    if ((int)fx.size() >= caps.fx) { dropped++; return nullptr; }
    Fx f;
    f.kind = kind; f.L = L; f.p = p; f.q = q; f.life = life; f.mag = mag; f.follow = follow;
    f.seed = rnd.u();
    fx.push_back(f);
    return &fx.back();
  }
  void addDecal(int kind, Vec2 p, float size, float life, const Look& L) {
    // (M9 fixer) a crater on a crater: one scorched pit, not rims stacked over each other's floors. The older pits
    // under the new one's floor go, and the new pit grows to take in what they covered (and their remaining time)
    if (kind == D_CRATER)
      for (size_t i = 0; i < decals.size();) {
        const Decal& o = decals[i];
        const float r = (float)decalW(D_CRATER) * 0.5f * std::max(o.size, size) * 0.72f;
        const Vec2 dd = o.p - p;
        if (o.kind == D_CRATER && dd.x * dd.x + 4.0f * dd.y * dd.y < r * r) {
          const float reach = std::sqrt(dd.x * dd.x + 4.0f * dd.y * dd.y) / ((float)decalW(D_CRATER) * 0.5f * 0.72f);
          size = std::min(2.8f, std::max(size, o.size + 0.5f * reach));
          life = std::max(life, o.life - o.t);
          decals.erase(decals.begin() + (std::ptrdiff_t)i);
        } else i++;
      }
    if ((int)decals.size() >= caps.decals) decals.erase(decals.begin());   // the oldest goes
    Decal d;
    d.kind = (uint8_t)kind; d.p = p; d.size = size; d.life = life; d.seed = rnd.u(); d.L = L;
    for (int i = 0; i < 5; i++) d.ramp[i] = L.ramp[i];
    decals.push_back(d);
  }
  void sound(Sfx s, float pitch, float vol) { if (sounds.size() < 24) sounds.push_back(SoundReq{s, pitch, vol, false, 0, 0}); }
  void motif(uint8_t id, uint8_t timbre, float vol) { if (sounds.size() < 24) sounds.push_back(SoundReq{Sfx::SpellMotif, 1, vol, true, id, timbre}); }
  void kick(float px) { shake = std::max(shake, px); }
  void flash(Color c, float a) { if (a > flashA) { flashA = a; flashC = c; } }
  void lightLift(Color c, float a) { if (a > lift) { lift = a; liftC = c; } }

  void shift(Vec2 d) {
    for (Fx& f : fx) { f.p += d; f.q += d; }
    for (Part& q : parts) q.p += d;
    for (Decal& dc : decals) dc.p += d;
  }
  void clear() { fx.clear(); parts.clear(); decals.clear(); }

  // ---- the phases (magic_draw.h: World::event's body, the update and the drawing)
  void event(Phase ph, const Look& L, Vec2 p, Vec2 q, float f, uint32_t follow, const std::string& s);
  void update(float dt, const std::function<bool(uint32_t, Vec2&)>& feetOf);
  void trail(const Look& L, Vec2 p, Vec2 v, float mag, float dt, uint32_t seed);   // a missile's sheddings, every frame
  void missileEnd(const Look& L, Vec2 p, Vec2 v, float mag);                        // a synthetic missile's impact
};

// the sounds a look makes: its school's / essence's launch and impact, the hum's pitch, the motif's voice
inline Sfx castSound(const Look& L) {
  if (L.form == Form::Summon) return Sfx::RiftTear;
  if (L.form == Form::Ward || L.form == Form::Aura) return Sfx::WardRing;
  if (L.form == Form::Meteor) return Sfx::MeteorFall;
  switch (L.ess) {
    case Essence::Fire: case Essence::Blood: return Sfx::FireCast;
    case Essence::Frost: case Essence::Bone: return Sfx::FrostCast;
    case Essence::Storm: return Sfx::StormCast;
    case Essence::Light: case Essence::Verdant: case Essence::Song: return Sfx::HolyCast;
    case Essence::Shadow: return Sfx::RiftTear;
    case Essence::Dream: case Essence::Arcane: case Essence::Wind: return Sfx::Shimmer;
    case Essence::Stone: case Essence::Tide: case Essence::Rust: case Essence::Venom: return Sfx::StoneGrind;
    case Essence::Star: return Sfx::StarChime;
    default: return Sfx::Shimmer;
  }
}
inline Sfx impactSound(const Look& L) {
  if (L.form == Form::Summon) return Sfx::SummonRise;
  if (L.form == Form::Ward || L.form == Form::Aura) return Sfx::WardRing;
  switch (L.ess) {
    case Essence::Fire: case Essence::Blood: return Sfx::FireBurst;
    case Essence::Frost: case Essence::Bone: return Sfx::FrostShatter;
    case Essence::Storm: return Sfx::Thunder;
    case Essence::Light: case Essence::Verdant: case Essence::Song: return Sfx::HolyBloom;
    case Essence::Shadow: case Essence::Dream: case Essence::Arcane: case Essence::Wind: return Sfx::DreamPulse;
    case Essence::Stone: case Essence::Tide: case Essence::Rust: case Essence::Venom: return Sfx::StoneSlam;
    case Essence::Star: return Sfx::StarChime;
    default: return Sfx::DreamPulse;
  }
}
inline uint8_t timbreOf(const Look& L) {
  switch (L.school) {
    case School::Destruction: return 0;
    case School::Restoration: return 1;
    case School::Conjuration: return 2;
    case School::Illusion: return 3;
    case School::Alteration: return 4;
    default: return 6;
  }
}
inline float humPitch(const Look& L) {
  static const float k[magic::SCHOOLS] = {0.9f, 1.25f, 0.7f, 1.1f, 0.8f};
  return k[(int)L.school % magic::SCHOOLS] / std::sqrt(baseScale(L));
}

}  // namespace mfx
