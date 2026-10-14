// M9 "Arcana & Mastery" (UI lane; VISION_PLAN 15.29.2, 15.30): the pixels of the skills and spellbook pages, painted
// into software Canvases so the game (rpg/view/skills_ui.cpp bakes them once per state into textures) and the review
// gallery (tools/preview/skills_gallery.cpp) show the very same art.
//
//   paintSky        a skill's night sky: banded dusk-to-night gradient, a nebula in the skill's colour, a starfield,
//                   the astrolabe ring and the skill's constellation figure (a sword, a kite shield, a sun ...), then
//                   the perk tree itself: the lines from each star to its parents and every perk star in its state
//                   (taken: bright and haloed; ready: lit and ringed; open: lit, no points; locked: dim)
//   perkLayout      where each perk's star sits in a sky of w x h (PerkDef x / y on the -8..8 grid)
//   perkState       a perk's state for the player (TAKEN / READY / OPEN / LOCKED)
//   paintSpellIcon  a spell's 16 x 16 orb: a dark disc rimmed in its school's ramp with its form / essence glyph
//                   (flame, frost shard, bolt, the mending cross, a ward, the spirit wolf, the eye, a rune ring...)
//   paintEye        the stealth eye (open: sneaking, watching; the lid low)
//   paintSpark      a small white sparkle (tinted when drawn: the live twinkles, the ready stars' pulse)
// Every colour is whole-pixel and dithered with a 4 x 4 Bayer matrix (16-bit look: no smooth gradients).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "engine/pix.h"
#include "rpg/sim/magic.h"
#include "rpg/sim/skills.h"

namespace skp {

constexpr float kPi = 3.14159265f;
enum PerkState : int { PS_LOCKED = 0, PS_OPEN = 1, PS_READY = 2, PS_TAKEN = 3 };

inline int cR(uint32_t c) { return (int)(c & 255); }
inline int cG(uint32_t c) { return (int)((c >> 8) & 255); }
inline int cB(uint32_t c) { return (int)((c >> 16) & 255); }
inline uint32_t rgbI(int r, int g, int b) { return rgba(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255), 255); }
inline uint32_t mixc(uint32_t a, uint32_t b, float t) {
  return rgbI((int)std::lround(cR(a) + (cR(b) - cR(a)) * t), (int)std::lround(cG(a) + (cG(b) - cG(a)) * t), (int)std::lround(cB(a) + (cB(b) - cB(a)) * t));
}
inline uint32_t mulc(uint32_t a, float k) { return rgbI((int)std::lround(cR(a) * k), (int)std::lround(cG(a) * k), (int)std::lround(cB(a) * k)); }
inline uint32_t hsh(uint32_t a, uint32_t b) {
  uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
  h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
  return h;
}
inline float bayer(int x, int y) {
  static const int m[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
  return (m[(y & 3) * 4 + (x & 3)] + 0.5f) / 16.0f;
}
// blend over an opaque pixel (a 0..1), additive brighten (k 0..1 of col)
inline void over(Canvas& c, int x, int y, uint32_t col, float a) {
  if (x < 0 || y < 0 || x >= c.w || y >= c.h || a <= 0) return;
  const uint32_t d = c.px[(size_t)y * c.w + x];
  c.px[(size_t)y * c.w + x] = mixc(d, col, std::min(1.0f, a)) | (d & 0xFF000000u ? (d & 0xFF000000u) : 0xFF000000u);
}
inline void addp(Canvas& c, int x, int y, uint32_t col, float k) {
  if (x < 0 || y < 0 || x >= c.w || y >= c.h || k <= 0) return;
  const uint32_t d = c.px[(size_t)y * c.w + x];
  c.px[(size_t)y * c.w + x] = rgbI(cR(d) + (int)(cR(col) * k), cG(d) + (int)(cG(col) * k), cB(d) + (int)(cB(col) * k));
}
// a soft additive glow quantised to whole steps with the dither (radius r, peak k)
inline void glow(Canvas& c, float cx, float cy, float r, uint32_t col, float k) {
  const int x0 = (int)std::floor(cx - r), x1 = (int)std::ceil(cx + r), y0 = (int)std::floor(cy - r), y1 = (int)std::ceil(cy + r);
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      const float d = std::sqrt((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy)) / r;
      if (d >= 1) continue;
      const float v = (1 - d) * (1 - d);
      const float q = std::floor(v * 4 + bayer(x, y)) / 4.0f;   // four steps
      addp(c, x, y, col, q * k);
    }
}
// a line with optional dots (every other pixel), a gap of `gap` px trimmed at both ends
inline void line(Canvas& c, float x0, float y0, float x1, float y1, uint32_t col, float a, bool dotted, float gap, bool glowSides = false) {
  const float dx = x1 - x0, dy = y1 - y0, L = std::sqrt(dx * dx + dy * dy);
  if (L < 1) return;
  const int n = (int)std::ceil(L);
  const bool steep = std::fabs(dy) > std::fabs(dx);
  int lastX = -99999, lastY = -99999, k = 0;
  for (int i = 0; i <= n; i++) {
    const float t = (float)i / (float)n;
    if (t * L < gap || (1 - t) * L < gap) continue;
    const int x = (int)std::floor(x0 + dx * t + 0.5f), y = (int)std::floor(y0 + dy * t + 0.5f);
    if (x == lastX && y == lastY) continue;
    lastX = x; lastY = y;
    if (!dotted || (k & 1) == 0) {
      over(c, x, y, col, a);
      if (glowSides) {
        if (steep) { over(c, x - 1, y, col, a * 0.28f); over(c, x + 1, y, col, a * 0.28f); }
        else { over(c, x, y - 1, col, a * 0.28f); over(c, x, y + 1, col, a * 0.28f); }
      }
    }
    k++;
  }
}
// value noise 0..1 on a lattice
inline float vnoise(float x, float y, uint32_t seed) {
  const int xi = (int)std::floor(x), yi = (int)std::floor(y);
  const float fx = x - xi, fy = y - yi;
  auto h = [&](int a, int b) { return (hsh(seed ^ (uint32_t)(a * 73856093), (uint32_t)(b * 19349663)) & 0xFFFF) / 65535.0f; };
  const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
  const float a = h(xi, yi), b = h(xi + 1, yi), c = h(xi, yi + 1), d = h(xi + 1, yi + 1);
  return (a + (b - a) * sx) * (1 - sy) + (c + (d - c) * sx) * sy;
}

// ---- the constellation figures (15.29.2: "drawn as constellations or culture glyph trees"): one per skill, in the
//      perk grid's -8..8 space (y down), each a list of strokes. Encoded as { n, x0, y0, ..., n, ..., 0 }.
struct Fig { std::vector<std::vector<float>> strokes; };
inline Fig figDecode(const float* d) {
  Fig f;
  while (*d > 0.5f) {
    const int n = (int)*d++;
    std::vector<float> s;
    for (int i = 0; i < n; i++) { s.push_back(*d++); s.push_back(*d++); }
    f.strokes.push_back(s);
  }
  return f;
}
inline void figCircle(Fig& f, float cx, float cy, float r, int seg, float a0 = 0) {
  std::vector<float> s;
  for (int i = 0; i <= seg; i++) { const float a = a0 + 2 * kPi * i / seg; s.push_back(cx + std::cos(a) * r); s.push_back(cy + std::sin(a) * r); }
  f.strokes.push_back(s);
}
inline Fig figureOf(skills::Skill sk) {
  using skills::Skill;
  static const float sword[] = {5, -1, 3, -1, -6, 0, -8, 1, -6, 1, 3, 4, -4.5f, 2.2f, -3.5f, 3, 3.5f, 3, 4.5f, 2.2f, 2, 0, 3, 0, 6.5f,
                                5, 0, 6.5f, 1, 7.3f, 0, 8, -1, 7.3f, 0, 6.5f, 0};
  static const float axe[] = {2, 1.5f, 8, -1, -7.5f, 5, -0.6f, -6, -5.5f, -7.8f, -7, -3, -5.5f, 1.2f, -0.2f, -1, 3, 0, -5, 3.5f, -4.4f, 0.1f, -3.2f, 0};
  static const float mace[] = {2, 0, 8, 0, -1.6f, 7, -3, -2, -3, -6, 0, -8, 3, -6, 3, -2, 0, -1, -3, -2, 2, -4.6f, -4, -3, -4, 2, 3, -4, 4.6f, -4,
                               2, -1.2f, 5, 1.2f, 5, 0};
  static const float spear[] = {2, 0, 8.5f, 0, -3.6f, 5, 0, -3.6f, -1.6f, -5.6f, 0, -8.6f, 1.6f, -5.6f, 0, -3.6f, 2, -1.8f, -3.4f, 1.8f, -3.4f,
                                3, 0, -3.4f, -1.2f, -1.6f, -0.6f, 0.2f, 0};
  static const float dagger[] = {5, -1, 0.5f, -1, -4.5f, 0, -7, 1, -4.5f, 1, 0.5f, 4, -3.5f, -0.6f, -2.8f, 0.5f, 2.8f, 0.5f, 3.5f, -0.6f,
                                 2, 0, 0.5f, 0, 4.2f, 5, 0, 4.2f, 1, 5, 0, 6, -1, 5, 0, 4.2f, 0};
  static const float bow[] = {7, -3, -7.5f, 0.8f, -5.6f, 3, -2, 3.6f, 0, 3, 2, 0.8f, 5.6f, -3, 7.5f, 2, -3, -7.5f, -3, 7.5f, 2, -6.5f, 0, 5.6f, 0,
                              3, 4.4f, -1.1f, 5.6f, 0, 4.4f, 1.1f, 2, -6.5f, 0, -7.6f, -1.2f, 2, -6.5f, 0, -7.6f, 1.2f, 0};
  static const float paw[] = {9, -3, 2, -2, 0, 0, -0.6f, 2, 0, 3, 2, 2, 4.6f, 0, 5.2f, -2, 4.6f, -3, 2, 5, -4.6f, -2.6f, -3.6f, -3.8f, -4.6f, -1.4f,
                              -5.6f, -2.6f, -4.6f, -2.6f, 5, -1.6f, -4.8f, -0.6f, -6.2f, -1.6f, -7.4f, -2.6f, -6.2f, -1.6f, -4.8f,
                              5, 1.6f, -4.8f, 2.6f, -6.2f, 1.6f, -7.4f, 0.6f, -6.2f, 1.6f, -4.8f, 5, 4.6f, -2.6f, 5.6f, -3.8f, 4.6f, -5,
                              3.6f, -3.8f, 4.6f, -2.6f, 0};
  static const float shield[] = {7, 0, -7, -5, -6, -5.2f, 0, 0, 7.6f, 5.2f, 0, 5, -6, 0, -7, 2, 0, -7, 0, 7.6f, 2, -5.1f, -2, 5.1f, -2, 0};
  static const float feather[] = {2, 3, 7.5f, -3, -7.5f, 4, -3, -7.5f, -5.6f, -3, -4.2f, 1.5f, 0.6f, 4.6f, 4, -3, -7.5f, -0.4f, -5, 1.6f, -1, 2.3f, 3.6f,
                                  2, -4.9f, -1, -2.3f, -1.9f, 2, 0.9f, -2.1f, -1, -1.6f, 0};
  static const float vest[] = {12, -5, -5, -2, -6, 0, -4.5f, 2, -6, 5, -5, 5, -1, 3.5f, -1, 3.5f, 5, -3.5f, 5, -3.5f, -1, -5, -1, -5, -5,
                               2, -3.5f, 1, 3.5f, 1, 2, -3.5f, 3, 3.5f, 3, 2, 0, -4.5f, 0, 5, 0};
  static const float helm[] = {9, -4, -6, 4, -6, 5, -3, 5, 4, 3, 7, -3, 7, -5, 4, -5, -3, -4, -6, 2, -4.2f, -1, 4.2f, -1, 2, 0, -1, 0, 5.5f,
                               2, 0, -6, 0, -8.5f, 0};
  static const float banner[] = {2, -4, 8.5f, -4, -8.5f, 5, -4, -7, 5, -7, 3, -4, 5, -1, -4, -1, 5, 0, -5.6f, 1.1f, -4, 0, -2.4f, -1.1f, -4, 0, -5.6f, 0};
  static const float swift[] = {3, -7, -4, -4, 0, -7, 4, 3, -3, -4.6f, 0.4f, 0, -3, 4.6f, 3, 1.4f, -5.4f, 5.6f, 0, 1.4f, 5.4f, 2, -8.5f, 0, -6, 0, 0};
  static const float anvil[] = {12, -6.5f, -2, 5, -2, 7.2f, -3, 5.4f, 0, 2, 0, 1.4f, 3, 3.6f, 5, -3.6f, 5, -1.4f, 3, -2, 0, -5, 0, -6.5f, -2,
                                5, -2.2f, -8, 2.2f, -8, 2.2f, -6.2f, -2.2f, -6.2f, -2.2f, -8, 2, 0.6f, -6.2f, 3.8f, -3.2f, 0};
  static const float pot[] = {2, -5.5f, -1, 5.5f, -1, 6, -5, -1, -5.8f, 2, -3.6f, 5.5f, 3.6f, 5.5f, 5.8f, 2, 5, -1, 2, -3, 5.5f, -4, 7.5f, 2, 3, 5.5f, 4, 7.5f,
                              3, -2, -3, -1, -5, -2, -7, 3, 2, -3, 3, -5, 2, -7, 0};
  static const float sheaf[] = {2, 0, 8, 0, -3, 2, -1, 8, -3.4f, -2, 2, 1, 8, 3.4f, -2, 2, -1.8f, 3, 1.8f, 3, 5, 0, -3, -0.9f, -5, 0, -7.6f, 0.9f, -5, 0, -3,
                                5, -3.4f, -2, -4.6f, -3.6f, -4.6f, -6.2f, -3, -4.2f, -3.4f, -2, 5, 3.4f, -2, 4.6f, -3.6f, 4.6f, -6.2f, 3, -4.2f, 3.4f, -2, 0};
  static const float horse[] = {13, 2.5f, 8, 1.4f, 3, -2.6f, -0.6f, -6, 0.4f, -7.2f, -1.6f, -4.6f, -4.6f, -2.4f, -5.8f, -1.4f, -8, -0.2f, -6, 2, -6.2f,
                                4.2f, -3, 5.2f, 1.2f, 5.2f, 8, 3, 2, -6, 3.6f, -4.8f, 4.6f, -2.6f, 0};
  static const float bubble[] = {12, -6, -5, 6, -5, 7, -4, 7, 2, 6, 3, -1, 3, -4, 6.5f, -3, 3, -6, 3, -7, 2, -7, -4, -6, -5, 2, -4, -2, 4, -2, 2, -4, 0.2f, 2, 0.2f, 0};
  static const float moon[] = {7, 2, -7.4f, -2, -6.4f, -5, -3.4f, -6, 0, -5, 3.4f, -2, 6.4f, 2, 7.4f, 5, 2, -7.4f, -0.6f, -4, -1.8f, 0, -0.6f, 4, 2, 7.4f, 0};
  static const float key[] = {5, -5, -4.5f, -3, -6.5f, -1, -4.5f, -3, -2.5f, -5, -4.5f, 2, -1.6f, -3.2f, 5.5f, 4, 4, 3, 3, 1.6f, 3.4f, 3.4f, 4.4f, 2.6f,
                              4, 5.5f, 4, 4.6f, 5, 6.2f, 6.6f, 5.5f, 4, 0};
  static const float flame[] = {12, 0, -8, 1.6f, -4.5f, 4, -3, 4.6f, 1, 2.6f, 5, 0, 6, -2.6f, 5, -4.6f, 1, -3.6f, -3, -2, -1.6f, -1.6f, -5, 0, -8,
                                5, 0, 0, 1.2f, 2.6f, 0, 4.2f, -1.2f, 2.6f, 0, 0, 0};
  static const float mask[] = {8, -5, -5, 5, -5, 5, 0, 3, 5, 0, 6.6f, -3, 5, -5, 0, -5, -5, 3, -3.6f, -2, -1.6f, -2.6f, -1.6f, -1.4f,
                               3, 3.6f, -2, 1.6f, -2.6f, 1.6f, -1.4f, 3, -2.6f, 2, 0, 3.3f, 2.6f, 2, 0};
  static const float cube[] = {7, 0, -7, 6, -3.5f, 6, 3.5f, 0, 7, -6, 3.5f, -6, -3.5f, 0, -7, 2, 0, 0, 0, 7, 3, -6, -3.5f, 0, 0, 6, -3.5f, 0};
  Fig f;
  switch (sk) {
    case Skill::Sword: f = figDecode(sword); break;
    case Skill::Axe: f = figDecode(axe); break;
    case Skill::Mace: f = figDecode(mace); break;
    case Skill::Spear: f = figDecode(spear); break;
    case Skill::Dagger: f = figDecode(dagger); break;
    case Skill::Bow: f = figDecode(bow); break;
    case Skill::Unarmed: f = figDecode(paw); break;
    case Skill::Block: f = figDecode(shield); break;
    case Skill::LightArmour: f = figDecode(feather); break;
    case Skill::MediumArmour: f = figDecode(vest); break;
    case Skill::HeavyArmour: f = figDecode(helm); break;
    case Skill::Heritage: f = figDecode(banner); break;
    case Skill::Athletics: f = figDecode(swift); break;
    case Skill::Smithing: f = figDecode(anvil); break;
    case Skill::Cooking: f = figDecode(pot); break;
    case Skill::Farming: f = figDecode(sheaf); break;
    case Skill::Riding: f = figDecode(horse); break;
    case Skill::Speech: f = figDecode(bubble); break;
    case Skill::Stealth: f = figDecode(moon); break;
    case Skill::Lockpicking: f = figDecode(key); break;
    case Skill::Destruction: f = figDecode(flame); break;
    case Skill::Restoration: {   // the sun: a disc and eight rays
      figCircle(f, 0, 0, 3.2f, 12);
      for (int i = 0; i < 8; i++) {
        const float a = kPi / 8 + i * kPi / 4;
        f.strokes.push_back({std::cos(a) * 4.6f, std::sin(a) * 4.6f, std::cos(a) * 7.4f, std::sin(a) * 7.4f});
      }
      break;
    }
    case Skill::Conjuration: {   // the rift: a ring and a five-pointed star inside it
      figCircle(f, 0, 0, 6.6f, 20, -kPi / 2);
      std::vector<float> s;
      for (int i = 0; i <= 5; i++) { const float a = -kPi / 2 + i * 4 * kPi / 5; s.push_back(std::cos(a) * 6.6f); s.push_back(std::sin(a) * 6.6f); }
      f.strokes.push_back(s);
      break;
    }
    case Skill::Illusion: f = figDecode(mask); break;
    case Skill::Alteration: f = figDecode(cube); break;
    default: break;
  }
  return f;
}

// ---- the sky's frame of reference: the perk grid (-8..8) scaled into a w x h sky, wider than tall at most 1.5x
struct SkyMap { float cx, cy, kx, ky; };
inline SkyMap skyMap(int w, int h) {
  SkyMap m;
  m.cx = std::floor(w / 2.0f); m.cy = std::floor(h / 2.0f);
  m.ky = (h - 24) / 16.0f;
  m.kx = std::min((w - 28) / 16.0f, m.ky * 1.5f);
  return m;
}
struct PerkVis { int x = 0, y = 0, state = PS_LOCKED; };
inline std::vector<PerkVis> perkLayout(const skills::PerkDef* P, int n, int w, int h) {
  const SkyMap m = skyMap(w, h);
  std::vector<PerkVis> v((size_t)std::max(0, n));
  for (int i = 0; i < n; i++) {
    v[(size_t)i].x = (int)std::lround(m.cx + P[i].x * m.kx);
    v[(size_t)i].y = (int)std::lround(m.cy + P[i].y * m.ky);
  }
  return v;
}
inline int perkState(const skills::Skills& k, skills::Skill s, int i) {
  int n = 0;
  const skills::PerkDef* P = skills::perkTable(s, n);
  if (i < 0 || i >= n) return PS_LOCKED;
  if (k.has(s, i)) return PS_TAKEN;
  if (skills::perkBlocked(k, s, i).empty()) return PS_READY;
  const skills::PerkDef& d = P[i];
  const bool parents = (d.parent < 0 || k.has(s, d.parent)) && (d.parent2 < 0 || k.has(s, d.parent2));
  return k.level(s) >= d.need && parents ? PS_OPEN : PS_LOCKED;
}

// ---- one perk star painted into the sky (col: the skill's star colour)
inline void paintStar(Canvas& c, int x, int y, uint32_t col, int state) {
  const uint32_t white = rgbI(255, 252, 240);
  const uint32_t hot = mixc(col, white, 0.55f);
  // (M9 fixer) every perk is a node, never mistaken for a sky star: a dark socket disc (it clears the starfield and the
  // lines under it) rimmed by a solid pixel ring, dim and cool when locked, in the skill's colour once within reach
  {
    const uint32_t dimRim = mixc(mulc(col, 0.6f), rgbI(110, 118, 160), 0.55f);
    const uint32_t rim = state == PS_LOCKED ? dimRim : state == PS_OPEN ? mixc(col, dimRim, 0.35f) : mixc(col, white, state == PS_TAKEN ? 0.35f : 0.15f);
    const float rimA = state == PS_LOCKED ? 0.8f : 1.0f;
    for (int dy = -6; dy <= 6; dy++)
      for (int dx = -6; dx <= 6; dx++) {
        const float d = std::sqrt((float)(dx * dx + dy * dy));
        if (d <= 4.6f) over(c, x + dx, y + dy, rgbI(8, 8, 20), 0.88f);
        else if (d <= 5.6f) over(c, x + dx, y + dy, rim, rimA);
        else if (d <= 6.4f && (dy > 0 || dx > 0)) over(c, x + dx, y + dy, rgbI(0, 0, 4), 0.55f);   // its shadow, lower right
      }
    // the lit upper-left of the rim (the page's light)
    for (int dy = -6; dy <= 0; dy++)
      for (int dx = -6; dx <= 0; dx++) {
        const float d = std::sqrt((float)(dx * dx + dy * dy));
        if (d > 4.6f && d <= 5.6f && dx + dy <= -4) over(c, x + dx, y + dy, white, state == PS_LOCKED ? 0.18f : 0.4f);
      }
  }
  switch (state) {
    case PS_TAKEN: {
      glow(c, x + 0.5f, y + 0.5f, 10, col, 0.42f);
      for (int d = 1; d <= 4; d++) {
        const uint32_t ac = d == 1 ? hot : d == 2 ? col : mulc(col, d == 3 ? 0.78f : 0.55f);
        const float aa = d == 4 ? 0.7f : 1.0f;
        over(c, x + d, y, ac, aa); over(c, x - d, y, ac, aa); over(c, x, y + d, ac, aa); over(c, x, y - d, ac, aa);
      }
      for (int dx = -1; dx <= 1; dx += 2) for (int dy = -1; dy <= 1; dy += 2) over(c, x + dx, y + dy, hot, 0.8f);
      over(c, x, y, white, 1);
      // a thin halo ring outside the rim
      for (int k = 0; k < 48; k++) {
        const float a = 2 * kPi * k / 48;
        over(c, (int)std::lround(x + std::cos(a) * 7.5f), (int)std::lround(y + std::sin(a) * 7.5f), col, 0.4f);
      }
      break;
    }
    case PS_READY: case PS_OPEN: {
      const float k = state == PS_READY ? 1.0f : 0.72f;
      glow(c, x + 0.5f, y + 0.5f, 6, col, 0.22f * k);
      for (int d = 1; d <= 3; d++) {
        const uint32_t ac = d == 1 ? mixc(col, white, 0.3f) : mulc(col, d == 2 ? 0.85f * k : 0.55f * k);
        over(c, x + d, y, ac, k); over(c, x - d, y, ac, k); over(c, x, y + d, ac, k); over(c, x, y - d, ac, k);
      }
      over(c, x, y, state == PS_READY ? white : mixc(col, white, 0.4f), 1);
      for (int s = 0; s < 40; s++) {   // a dotted outer ring: the star is within reach
        if (s & 1) continue;
        const float a = 2 * kPi * s / 40;
        over(c, (int)std::lround(x + std::cos(a) * 7.5f), (int)std::lround(y + std::sin(a) * 7.5f), col, state == PS_READY ? 0.75f : 0.35f);
      }
      break;
    }
    default: {   // locked: a small dim diamond in a cooled colour
      const uint32_t dim = mixc(mulc(col, 0.7f), rgbI(96, 104, 150), 0.5f);
      over(c, x, y, mixc(dim, white, 0.45f), 1.0f);
      over(c, x + 1, y, dim, 0.9f); over(c, x - 1, y, dim, 0.9f); over(c, x, y + 1, dim, 0.9f); over(c, x, y - 1, dim, 0.9f);
      over(c, x + 2, y, dim, 0.45f); over(c, x - 2, y, dim, 0.45f); over(c, x, y + 2, dim, 0.45f); over(c, x, y - 2, dim, 0.45f);
      break;
    }
  }
}

// ---- the whole sky of a skill (static parts only: the view adds the pulse, the selection and the twinkles live)
inline Canvas paintSky(int w, int h, skills::Skill sk, const skills::PerkDef* P, int n, const int* state) {
  Canvas c(w, h);
  const skills::SkillInfo& inf = skills::skillInfo(sk);
  const uint32_t col = inf.color | 0xFF000000u, seed = inf.glyphSeed * 2654435761u + 17u;
  // the night: six bands from a dusky indigo at the top to near black, dithered across each band edge
  const uint32_t top = mixc(rgbI(22, 18, 44), col, 0.08f), bot = rgbI(6, 6, 14);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const float t = (float)y / std::max(1, h - 1);
      const float band = std::floor(t * 6 + bayer(x, y) - 0.5f) / 5.0f;
      c.px[(size_t)y * w + x] = mixc(top, bot, std::clamp(band, 0.0f, 1.0f));
    }
  // the nebula in the skill's colour: two octaves of value noise, three dithered steps
  const float ox = (float)(seed & 255), oy = (float)((seed >> 8) & 255);
  const uint32_t neb = mixc(col, rgbI(40, 30, 80), 0.35f);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const float nz = 0.62f * vnoise((x + ox) / 34.0f, (y + oy) / 22.0f, seed) + 0.38f * vnoise((x + ox) / 13.0f, (y + oy) / 9.0f, seed ^ 0xABCDu);
      // a broad band across the sky (the milky way's lean) so the cloud gathers rather than speckles
      const float lean = 1.0f - std::fabs(((float)y - h * 0.5f - ((float)x - w * 0.5f) * 0.35f) / (h * 0.55f));
      const float s = std::clamp((nz * 0.75f + lean * 0.45f - 0.55f) / 0.4f, 0.0f, 1.0f);
      const float q = std::floor(s * 3 + bayer(x, y) - 0.25f) / 3.0f;
      if (q > 0) over(c, x, y, neb, q * 0.26f);
    }
  // the starfield: tiny stars of three brightnesses, a few crossed ones
  const int nStars = w * h / 70;
  for (int i = 0; i < nStars; i++) {
    const uint32_t r = hsh(seed, (uint32_t)i * 7u + 1u);
    const int x = (int)(r % (uint32_t)w), y = (int)((r >> 12) % (uint32_t)h);
    const int b = (int)((r >> 24) % 10);
    const uint32_t sc = mixc(rgbI(200, 210, 255), col, 0.25f);
    if (b < 6) over(c, x, y, sc, 0.22f);
    else if (b < 9) over(c, x, y, sc, 0.45f);
    else { over(c, x, y, rgbI(255, 255, 250), 0.75f); over(c, x + 1, y, sc, 0.25f); over(c, x - 1, y, sc, 0.25f); over(c, x, y + 1, sc, 0.25f); over(c, x, y - 1, sc, 0.25f); }
  }
  // the vignette: the edges fall away into the night
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const float dx = (x - w / 2.0f) / (w / 2.0f), dy = (y - h / 2.0f) / (h / 2.0f);
      const float d = std::sqrt(dx * dx * 0.8f + dy * dy * 0.8f);
      const float v = std::clamp((d - 0.72f) / 0.5f, 0.0f, 1.0f);
      const float q = std::floor(v * 3 + bayer(x + 1, y + 2)) / 3.0f;
      if (q > 0) over(c, x, y, rgbI(2, 2, 6), q * 0.5f);
    }
  const SkyMap m = skyMap(w, h);
  // the astrolabe: an outer ring with ticks every 10 degrees (longer every 30), a dotted inner ring
  const float R = std::min(h / 2.0f - 5, w / 2.0f - 5);
  const uint32_t ringC = mixc(col, rgbI(230, 200, 140), 0.5f);
  for (int k = 0; k < 360; k++) {
    const float a = 2 * kPi * k / 360;
    over(c, (int)std::lround(m.cx + std::cos(a) * R), (int)std::lround(m.cy + std::sin(a) * R), ringC, 0.13f);
    if (k % 4 == 0) over(c, (int)std::lround(m.cx + std::cos(a) * (R - 4)), (int)std::lround(m.cy + std::sin(a) * (R - 4)), ringC, 0.09f);
  }
  for (int k = 0; k < 36; k++) {
    const float a = 2 * kPi * k / 36;
    const float r0 = R - (k % 3 == 0 ? 3.0f : 1.5f);
    for (float rr = r0; rr <= R + 0.01f; rr += 0.5f)
      over(c, (int)std::lround(m.cx + std::cos(a) * rr), (int)std::lround(m.cy + std::sin(a) * rr), ringC, k % 3 == 0 ? 0.22f : 0.14f);
  }
  // the figure: faint strokes, its vertices small dim stars (isotropic: never stretched)
  const Fig f = figureOf(sk);
  const float kf = m.ky * 0.98f;
  const uint32_t figC = mixc(col, rgbI(255, 255, 255), 0.2f);
  for (const std::vector<float>& s : f.strokes) {
    for (size_t i = 0; i + 3 < s.size(); i += 2)
      line(c, m.cx + s[i] * kf, m.cy + s[i + 1] * kf, m.cx + s[i + 2] * kf, m.cy + s[i + 3] * kf, figC, 0.09f, false, 0);   // (M9 fixer) fainter: the tree's own lines lead
    for (size_t i = 0; i + 1 < s.size(); i += 2) {
      const int x = (int)std::lround(m.cx + s[i] * kf), y = (int)std::lround(m.cy + s[i + 1] * kf);
      over(c, x, y, figC, 0.24f);
    }
  }
  // the tree: the lines to the parents (both lit: a bright line with a soft edge; the parent lit: a quieter line; else
  // dots), then the stars
  const std::vector<PerkVis> pv = perkLayout(P, n, w, h);
  for (int i = 0; i < n; i++)
    for (int pp = 0; pp < 2; pp++) {
      const int par = pp == 0 ? P[i].parent : P[i].parent2;
      if (par < 0 || par >= n) continue;
      const int sa = state[i], sb = state[par];
      const PerkVis& a = pv[(size_t)i];
      const PerkVis& b = pv[(size_t)par];
      // (M9 fixer) the tree's edges read above the figure: solid from rim to rim, brighter as the path opens
      if (sa == PS_TAKEN && sb == PS_TAKEN) line(c, (float)a.x, (float)a.y, (float)b.x, (float)b.y, mixc(col, rgbI(255, 250, 235), 0.3f), 0.9f, false, 6, true);
      else if (sb == PS_TAKEN) line(c, (float)a.x, (float)a.y, (float)b.x, (float)b.y, col, 0.75f, false, 6, true);
      else line(c, (float)a.x, (float)a.y, (float)b.x, (float)b.y, mixc(col, rgbI(150, 160, 215), 0.55f), 0.55f, false, 6);
    }
  for (int i = 0; i < n; i++) paintStar(c, pv[(size_t)i].x, pv[(size_t)i].y, col, state[i]);
  return c;
}

// ---- spell icons: a 12 x 12 glyph ('1'..'5': the ramp, dark to hot) on a 16 x 16 orb
enum class Glyph : uint8_t { Flame, Shard, Bolt, Cross, Shield, Wolf, Eye, Ring, Rock, Drop, Star, Rune };
inline Glyph glyphOf(const magic::SpellDef& d) {
  using magic::Essence; using magic::Form;
  switch (d.form) {
    case Form::Mend: return Glyph::Cross;
    case Form::Ward: return Glyph::Shield;
    case Form::Summon: return Glyph::Wolf;
    case Form::Aura: return Glyph::Ring;
    default: break;
  }
  switch (d.ess) {
    case Essence::Fire: return Glyph::Flame;
    case Essence::Frost: return Glyph::Shard;
    case Essence::Storm: return Glyph::Bolt;
    case Essence::Stone: case Essence::Rust: return Glyph::Rock;
    case Essence::Tide: case Essence::Venom: case Essence::Blood: return Glyph::Drop;
    case Essence::Dream: case Essence::Shadow: case Essence::Song: return Glyph::Eye;
    case Essence::Light: case Essence::Star: return Glyph::Star;
    default: return Glyph::Rune;
  }
}
inline const char* const* glyphMap(Glyph g) {
  static const char* flame[12] = {"......4.....", ".....44.....", ".....454....", "....4454....", "...34554..4.", "...345543.4.",
                                  "..2345553.3.", "..23455543..", ".223455543..", ".2234555432.", "..22344432..", "...222222..."};
  static const char* shard[12] = {"..........55", "........5543", ".5.....5443.", "......5443..", ".....5443...", "....5443....",
                                  "...4433.....", "..4332......", ".3322.......", ".322.....5..", "322.........", "21.........."};
  static const char* bolt[12] = {".......5554.", "......5543..", ".....5543...", "....5543....", "...55555554.", "......5543..",
                                 ".....5543...", "....5443....", "...543......", "..543.......", ".443........", ".3.........."};
  static const char* shield[12] = {".2444444442.", ".4555543333.", ".4555543333.", ".4555543333.", ".4555543333.", ".4555543332.",
                                   "..45554333..", "..45554333..", "...455433...", "...455433...", "....4543....", ".....43....."};
  static const char* wolf[12] = {".2........2.", ".32......23.", ".333....333.", ".3433333343.", ".3444444443.", "..34544543..",
                                 "..34444443..", "...344443...", "...323323...", "....3443....", ".....22.....", "............"};
  static const char* eye[12] = {"............", "............", "....3333....", "..33444433..", ".3445555443.", "344551155443",
                                "344551155443", ".3445555443.", "..33444433..", "....3333....", "............", "............"};
  static const char* rock[12] = {"............", "....3444....", "...345554...", "..34555443..", ".3455544433.", ".3455444332.",
                                 ".3454443322.", ".3344333221.", "..33333221..", "...222221...", "............", "............"};
  static const char* drop[12] = {".....4......", ".....44.....", "....3453....", "....3453....", "...345543...", "..34555443..",
                                 "..34554443..", "..34544433..", "..33444333..", "...333332...", "....2222....", "............"};
  switch (g) {
    case Glyph::Flame: return flame;
    case Glyph::Shard: return shard;
    case Glyph::Bolt: return bolt;
    case Glyph::Shield: return shield;
    case Glyph::Wolf: return wolf;
    case Glyph::Eye: return eye;
    case Glyph::Rock: return rock;
    case Glyph::Drop: return drop;
    default: return nullptr;
  }
}
// the procedural glyphs (geometric ones): a ramp index 0 (none) .. 5 for each cell of 12 x 12
inline int glyphCell(Glyph g, int x, int y) {
  const float fx = x - 5.5f, fy = y - 5.5f;
  switch (g) {
    case Glyph::Cross: {   // the mending cross, lit from the top-left
      const bool v = x >= 4 && x <= 7, hz = y >= 4 && y <= 7;
      if (!(v || hz)) return 0;
      if ((x == 4 && v) || (y == 4 && hz)) return 5;
      if ((x == 7 && v && !hz) || (y == 7 && hz && !v) || (x == 7 && y == 7)) return 3;
      return 4;
    }
    case Glyph::Ring: {    // an aura: a ring with four bright nodes
      const float r = std::sqrt(fx * fx + fy * fy);
      const bool node = (std::fabs(fx) < 1 && std::fabs(std::fabs(fy) - 4.5f) < 1.2f) || (std::fabs(fy) < 1 && std::fabs(std::fabs(fx) - 4.5f) < 1.2f);
      if (node) return 5;
      if (r > 3.6f && r < 5.4f) return fx + fy < 0 ? 4 : 3;
      if (r < 1.6f) return 4;
      return 0;
    }
    case Glyph::Star: {    // a four-pointed sparkle
      const float ax = std::fabs(fx), ay = std::fabs(fy);
      if (ax + ay < 2.2f) return 5;
      if ((ax < 0.9f && ay < 5.6f) || (ay < 0.9f && ax < 5.6f)) return ax + ay < 4 ? 5 : 4;
      if (ax + ay < 3.4f) return 3;
      return 0;
    }
    default: {             // a rune: a diamond with a cross cut in it
      const float d = std::fabs(fx) + std::fabs(fy);
      if (d > 5.6f) return 0;
      if (std::fabs(fx) < 0.6f || std::fabs(fy) < 0.6f) return d < 4.6f ? 5 : 0;
      return d > 4.6f ? (fx + fy < 0 ? 4 : 2) : 3;
    }
  }
}
inline Canvas paintSpellIcon(const magic::SpellDef& d) {
  Canvas c(16, 16);
  uint32_t ramp[5];
  for (int i = 0; i < 5; i++) ramp[i] = d.fx.ramp[i] ? (d.fx.ramp[i] | 0xFF000000u) : rgbI(60 + 45 * i, 60 + 45 * i, 70 + 45 * i);
  // the orb: a dark disc, its rim in the ramp, a lit arc at the top-left and a dark one at the bottom-right
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      const float dx = x + 0.5f - 8, dy = y + 0.5f - 8, r = std::sqrt(dx * dx + dy * dy);
      if (r > 8.0f) continue;
      uint32_t p;
      if (r > 7.0f) p = rgbI(10, 8, 14);
      else if (r > 6.0f) p = (dx + dy < -2) ? ramp[2] : (dx + dy > 3 ? mulc(ramp[0], 0.8f) : ramp[1]);
      else {
        p = mixc(mulc(ramp[0], 0.55f), rgbI(8, 6, 12), 0.35f);
        if (dx + dy < -5.5f && r > 4.5f) p = mixc(p, ramp[1], 0.45f);
      }
      c.set(x, y, p);
    }
  // the glyph and its dark outline
  int cell[12][12] = {};
  const char* const* map = glyphMap(glyphOf(d));
  for (int y = 0; y < 12; y++)
    for (int x = 0; x < 12; x++) {
      if (map) { const char ch = map[y][x]; cell[y][x] = ch >= '1' && ch <= '5' ? ch - '0' : 0; }
      else cell[y][x] = glyphCell(glyphOf(d), x, y);
    }
  for (int y = 0; y < 12; y++)
    for (int x = 0; x < 12; x++) {
      if (cell[y][x]) continue;
      bool edge = false;
      for (int k = 0; k < 4 && !edge; k++) {
        const int nx = x + (k == 0) - (k == 1), ny = y + (k == 2) - (k == 3);
        if (nx >= 0 && ny >= 0 && nx < 12 && ny < 12 && cell[ny][nx]) edge = true;
      }
      if (edge) c.set(x + 2, y + 2, mixc(mulc(ramp[0], 0.5f), rgbI(4, 2, 8), 0.5f));
    }
  for (int y = 0; y < 12; y++)
    for (int x = 0; x < 12; x++)
      if (cell[y][x]) c.set(x + 2, y + 2, ramp[cell[y][x] - 1]);
  return c;
}

// ---- the stealth eye, 11 x 7: open (watching) or with the lid low (sneaking: hidden)
inline Canvas paintEye(bool lidded, uint32_t tint) {
  static const char* open[7] = {"...ooooo...", ".oo#####oo.", "o##WIIIW##o", "o##IPPPI##o", "o##WIIIW##o", ".oo#####oo.", "...ooooo..."};
  static const char* low[7] = {"...ooooo...", ".ooLLLLLoo.", "oLLLLLLLLLo", "o##IPPPI##o", "o##WIIIW##o", ".oo#####oo.", "...ooooo..."};
  const char* const* m = lidded ? low : open;
  Canvas c(11, 7);
  const uint32_t t = tint | 0xFF000000u;
  for (int y = 0; y < 7; y++)
    for (int x = 0; x < 11; x++) {
      const char ch = m[y][x];
      uint32_t p = 0;
      if (ch == 'o') p = rgbI(14, 12, 22);
      else if (ch == '#') p = mixc(t, rgbI(235, 232, 245), 0.55f);
      else if (ch == 'L') p = mulc(t, 0.62f);
      else if (ch == 'I') p = t;
      else if (ch == 'W') p = mixc(t, rgbI(255, 255, 255), 0.5f);
      else if (ch == 'P') p = rgbI(16, 12, 30);
      if (p) c.set(x, y, p);
    }
  return c;
}
// a 7 x 7 white sparkle (tinted when drawn)
inline Canvas paintSpark() {
  Canvas c(7, 7);
  const uint32_t w = rgba(255, 255, 255, 255), m = rgba(255, 255, 255, 150), d = rgba(255, 255, 255, 70);
  c.set(3, 3, w);
  c.set(2, 3, m); c.set(4, 3, m); c.set(3, 2, m); c.set(3, 4, m);
  c.set(1, 3, d); c.set(5, 3, d); c.set(3, 1, d); c.set(3, 5, d);
  c.set(0, 3, rgba(255, 255, 255, 30)); c.set(6, 3, rgba(255, 255, 255, 30)); c.set(3, 0, rgba(255, 255, 255, 30)); c.set(3, 6, rgba(255, 255, 255, 30));
  return c;
}

// ---- words shared by the skills page and the item cards
struct Line { std::string s; int tone = 0; };   // tone: 1 good (green), -1 bad (red), 0 plain, 2 quiet
// the armour class's trade-offs in plain words at a skill level
inline std::vector<Line> tradeLines(skills::Weight w, int level) {
  std::vector<Line> v;
  if (w == skills::Weight::None) return v;
  const skills::ArmourTrade t = skills::armourTrade(w, level);
  auto pct = [](float m) { return (int)std::lround((m - 1.0f) * 100.0f); };
  const int sp = pct(t.speed), ro = pct(t.roll), rc = pct(t.rollCost), sr = pct(t.stRegen), dr = (int)std::lround(t.dr * 100.0f);
  if (sp) v.push_back({sp > 0 ? "MOVE " + std::to_string(sp) + "% FASTER" : "MOVE " + std::to_string(-sp) + "% SLOWER", sp > 0 ? 1 : -1});
  if (ro) v.push_back({ro > 0 ? "ROLL " + std::to_string(ro) + "% FURTHER" : "ROLL " + std::to_string(-ro) + "% SHORTER", ro > 0 ? 1 : -1});
  if (rc) v.push_back({rc > 0 ? "ROLLS COST " + std::to_string(rc) + "% MORE" : "ROLLS COST " + std::to_string(-rc) + "% LESS", rc > 0 ? -1 : 1});
  if (sr) v.push_back({sr > 0 ? "STAMINA BACK " + std::to_string(sr) + "% FASTER" : "STAMINA BACK " + std::to_string(-sr) + "% SLOWER", sr > 0 ? 1 : -1});
  if (t.noise < 0.25f) v.push_back({"QUIET: HARD TO HEAR", 1});
  else if (t.noise < 0.6f) v.push_back({"SOME NOISE AS YOU MOVE", 0});
  else v.push_back({"LOUD: FOES HEAR YOU COME", -1});
  if (dr > 0) v.push_back({"STOPS " + std::to_string(dr) + "% MORE OF A BLOW", 1});
  return v;
}
// the weapon family's lines: "+35% VS ARMOURED: CRUSHES PLATE" (tone from the sign)
inline std::vector<Line> familyLines(skills::Skill fam) {
  std::vector<Line> v;
  for (const std::string& s : skills::familyVsLines(fam)) v.push_back({s, !s.empty() && s[0] == '-' ? -1 : 1});
  return v;
}
// a family's short name for an item card ("SWORD", "MACE"...): the skill's name without its plural
inline std::string familyWord(skills::Skill fam) {
  switch (fam) {
    case skills::Skill::Sword: return "SWORD";
    case skills::Skill::Axe: return "AXE";
    case skills::Skill::Mace: return "MACE";
    case skills::Skill::Spear: return "SPEAR";
    case skills::Skill::Dagger: return "DAGGER";
    case skills::Skill::Bow: return "BOW";
    case skills::Skill::Unarmed: return "FISTS";
    default: return skills::skillInfo(fam).name;
  }
}

}  // namespace skp
