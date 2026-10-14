// M9 "Arcana & Mastery" (VISION_PLAN 15.29.7): the spells' life on screen, drawn through a painter (see magic_fx.h).
// SPELLFX lane. World::event turns an Ev::Spell phase into pooled effects, particles, decals and requests (sounds, the
// screen kick, the scene flash, the light-map lift); World::update ages them; the draw templates paint them:
//   drawGround   under the bodies: decals, the wind-up sigils, the ground rings and the impact's ground glow
//   drawFx       after the lighting: missiles' bodies, every phase's burst, particles, the sky's spectacles
//   drawMissile  one missile (its trail, its head, its glow), drawMissileShadow its shadow on the ground
//   drawStatus   a body's status overlays (flames, the ice shell, drips, arcs, bubbles, a fear swirl, hearts, stars)
//   lightsOf     the world-space lights for the light map
// Every school is told by its silhouette: fire boils and roars (a rising lumpy bloom cooling into smoke), frost
// shatters (crystal spikes erupt and burst into shards), lightning forks (jagged arcs, a sky strike, the scene flash),
// Restoration rises (a pillar of light, a turning sigil, warm motes), Conjuration tears (a rift opens; summoned forms
// climb out), Illusion shimmers (chromatic rings, a fear mask), Alteration pulses (stone spikes and rubble, a splash
// crown, hex shells), Starcalling draws star-lines in the sky. Size (FxStyle::size) and magnitude (upcast, dual-cast)
// scale every part; size 3-4 and the Meteor / Storm forms are spectacles.
#pragma once
#include "rpg/view/magic_fx.h"

namespace mfx {

struct Ctx {
  Vec2 cam;                // world px of the screen's top-left
  float W = 480, H = 270;  // the canvas
  float dark = 0;          // 0 day .. 1 deep night (what glows stands out as much)
  float lightK = 1;        // non-emissive colour under the night (1 day .. 0.25 night)
  float t = 0;             // seconds (animation clock)
};
inline Vec2 scr(const Ctx& C, Vec2 w) { return Vec2(std::floor(w.x - C.cam.x), std::floor(w.y - C.cam.y)); }
inline bool onScr(const Ctx& C, Vec2 s, float m) { return s.x > -m && s.y > -m && s.x < C.W + m && s.y < C.H + m; }

// ------------------------------------------------------------------ primitives
template <class Pt> void dot(Pt& P, float x, float y, Color c, bool add) {
  if (add) P.add(std::floor(x), std::floor(y), 1, 1, c); else P.rect(std::floor(x), std::floor(y), 1, 1, c);
}
// a 1-px Bresenham line, its pixels merged into horizontal (shallow) or vertical (steep) runs: one rect a run
template <class Pt> void line(Pt& P, int x0, int y0, int x1, int y1, Color c, bool add) {
  int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
  const bool shallow = dx >= -dy;
  int rx = x0, ry = y0, rl = 0;   // the current run's start and length
  auto flush = [&] {
    if (rl <= 0) return;
    const float fx = shallow ? (float)std::min(rx, rx + (rl - 1) * sx) : (float)rx, fy = shallow ? (float)ry : (float)std::min(ry, ry + (rl - 1) * sy);
    const float w = shallow ? (float)rl : 1.0f, h = shallow ? 1.0f : (float)rl;
    if (add) P.add(fx, fy, w, h, c); else P.rect(fx, fy, w, h, c);
  };
  for (int n = 0; n < 600; n++) {
    if (rl == 0) { rx = x0; ry = y0; }
    rl++;
    if (x0 == x1 && y0 == y1) break;
    const int e2 = 2 * err;
    const int ox = x0, oy = y0;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
    if ((shallow && y0 != oy) || (!shallow && x0 != ox)) { flush(); rl = 0; }
  }
  flush();
}
template <class Pt> void icon(Pt& P, World& W, Ico i, float x, float y, Color c, int blend, int sc = 1) {
  P.blit(W.common(), i.x, i.y, i.w, i.h, std::floor(x), std::floor(y), (float)(i.w * sc), (float)(i.h * sc), c, blend, false);
}
// a glow of radius r (the banded 16-bit glow, or the soft one), additive
template <class Pt> void glow(Pt& P, World& W, float x, float y, float r, Color c, bool soft = false) {
  if (r < 1 || c.a <= 0.004f) return;
  const float d = std::floor(r * 2 + 0.5f);
  const Ico i = soft ? I_GLOWS : I_GLOWB;
  P.blit(W.common(), i.x, i.y, i.w, i.h, std::floor(x - d * 0.5f), std::floor(y - d * 0.5f), d, d, c, 1, false);
}
// a glow on the ground (twice as wide as deep)
template <class Pt> void gglow(Pt& P, World& W, float x, float y, float rx, Color c, bool soft = true) {
  if (rx < 1 || c.a <= 0.004f) return;
  const float w = std::floor(rx * 2 + 0.5f), h = std::floor(rx + 0.5f);
  const Ico i = soft ? I_GGLOWS : I_GGLOWB;
  P.blit(W.common(), i.x, i.y, i.w, i.h, std::floor(x - w * 0.5f), std::floor(y - h * 0.5f), w, h, c, 1, false);
}
// (M9 integration) a soft vertical beam: 1-px columns whose strength falls off from the centre line (a rounded
// profile, no hard box edges) and fades out toward the top; add: additive, else painted
template <class Pt> void beam(Pt& P, float cx, float top, float bottom, float w, Color c, bool add) {
  if (w < 1 || bottom <= top || c.a <= 0.004f) return;
  const int half = std::max(1, (int)std::floor(w * 0.5f + 0.5f));
  const float H = bottom - top;
  // (M9 fixer, review: "the light column is still hard-sided") a steeper bell (cubed) so the sides melt away, and
  // finer steps up the height so the fade toward the sky has no visible bands
  const int SEG = std::clamp((int)(H / 10.0f), 5, 14);
  for (int dx = -half; dx < half; dx++) {
    const float q = ((float)dx + 0.5f) / (float)half;
    const float b = 1 - q * q, prof = b * b * b;
    if (prof <= 0.02f) continue;
    for (int sg = 0; sg < SEG; sg++) {
      const float y0 = std::floor(top + H * (float)sg / (float)SEG), y1 = std::floor(top + H * (float)(sg + 1) / (float)SEG);
      if (y1 <= y0) continue;
      const float fade = 0.15f + 0.85f * ((float)sg + 0.5f) / (float)SEG;   // faint at the top, full at the foot
      const Color cc(c.r, c.g, c.b, c.a * prof * fade);
      if (add) P.add(std::floor(cx) + (float)dx, y0, 1, y1 - y0, cc);
      else P.rect(std::floor(cx) + (float)dx, y0, 1, y1 - y0, cc);
    }
  }
}
// an ellipse ring of single pixels (wob: a wobble of the radius; gap: a dash pattern)
template <class Pt> void ring(Pt& P, float cx, float cy, float rx, float ry, Color c, bool add, int thick = 1, float wob = 0, float ph = 0, int gap = 0) {
  if (rx < 0.5f || c.a <= 0.004f) return;
  const int n = std::clamp((int)(rx * 2.6f), 12, 260);
  for (int i = 0; i < n; i++) {
    if (gap && (i / gap) % 2) continue;
    const float a = kTau * (float)i / (float)n;
    const float w = 1.0f + wob * std::sin(a * 6 + ph);
    const float x = cx + std::cos(a) * rx * w, y = cy + std::sin(a) * ry * w;
    dot(P, x, y, c, add);
    if (thick > 1) dot(P, x, y + 1, withA(c, c.a * 0.55f), add);
  }
}
// a jagged lightning arc a -> b, re-struck by seed: a halo, a violet rim either side, a white-hot core; forks
template <class Pt> void bolt(Pt& P, World& W, Vec2 a, Vec2 b, uint32_t seed, Color halo, Color rim, Color core, float al, int forks, float jag = 1) {
  const Vec2 d = b - a;
  const float l = std::max(1.0f, len(d));
  const Vec2 nr(-d.y / l, d.x / l);
  const int ns = std::clamp((int)(l / 6.0f), 3, 60);
  Vec2 pts[62];
  for (int i = 0; i <= ns; i++) {
    const float t = (float)i / (float)ns, env = std::sin(t * kPi);
    const float zig = ((i & 1) ? 1.0f : -1.0f) * (0.35f + 0.65f * hf(hmix(seed, (uint32_t)i))) * (2.5f + l * 0.05f) * jag;
    const float wan = (hf(hmix(seed ^ 0x77u, (uint32_t)(i / 3))) - 0.5f) * (5.0f + l * 0.1f) * jag;
    pts[i] = a + d * t + nr * ((zig + wan) * env);
  }
  const bool lite = l < 28;   // a short arc (a status's crackle, a tile's spark): a core and one rim, no halo chain
  if (!lite) for (int i = 0; i < ns; i += 2) glow(P, W, pts[i].x, pts[i].y, 5.0f, withA(halo, 0.3f * al));
  else glow(P, W, (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, l * 0.5f + 2, withA(halo, 0.25f * al));
  for (int i = 0; i < ns; i++) {
    const Vec2 p0 = pts[i], p1 = pts[i + 1];
    line(P, (int)p0.x - 1, (int)p0.y, (int)p1.x - 1, (int)p1.y, withA(rim, 0.8f * al), false);
    if (!lite) line(P, (int)p0.x + 1, (int)p0.y + 1, (int)p1.x + 1, (int)p1.y + 1, withA(mulC(rim, 0.6f), 0.7f * al), false);
  }
  for (int i = 0; i < ns; i++) {
    const Vec2 p0 = pts[i], p1 = pts[i + 1];
    line(P, (int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y, withA(core, al), false);
  }
  for (int f = 0; f < forks; f++) {
    const int at = std::clamp(ns * (f + 1) / (forks + 1), 1, ns - 1);
    const uint32_t h = hmix(seed, 0xF0u + (uint32_t)f);
    Vec2 p = pts[at];
    Vec2 dir = norm(d * (1.0f / l) * 0.5f + nr * ((h & 1) ? 1.0f : -1.0f));
    for (int s = 0; s < 3; s++) {
      const Vec2 q = p + dir * (4.0f + hf(h >> (s * 5)) * 5.0f) + nr * ((hf(h >> (s * 3 + 2)) - 0.5f) * 4.0f);
      line(P, (int)p.x + 1, (int)p.y + 1, (int)q.x + 1, (int)q.y + 1, withA(rim, 0.6f * al), false);
      line(P, (int)p.x, (int)p.y, (int)q.x, (int)q.y, withA(core, 0.85f * al * (1.0f - 0.25f * (float)s)), false);
      p = q;
    }
  }
}
// a spike erupting from the ground at (x, y), h tall, w wide at its foot, in 3/4: the left face lit (the light from the
// top left), the right face in shade, a dark right edge, the tip bright; its shadow falls to the bottom right
template <class Pt> void spike(Pt& P, float x, float y, float h, float w, Color lit, Color mid, Color dark, Color tip, float shadowA) {
  if (h < 1) return;
  const int hw = std::max(1, (int)std::floor(w * 0.5f));
  if (shadowA > 0) {
    for (int k = 0; k < (int)(h * 0.35f) + 2; k++) P.rect(std::floor(x + (float)k * 0.8f), std::floor(y + (float)k * 0.35f), 2, 1, Color(0.04f, 0.04f, 0.08f, shadowA * (1.0f - (float)k / (h * 0.35f + 2))));
  }
  for (int i = -hw; i <= hw; i++) {
    const float ch = std::floor(h * (1.0f - std::fabs((float)i) / (float)(hw + 1)));
    if (ch < 1) continue;
    const Color c = i < 0 ? (i == -hw ? mulC(lit, 0.85f) : lit) : i == hw ? dark : mid;
    P.rect(std::floor(x) + (float)i, std::floor(y) - ch, 1, ch, c);
    if (i == -1 || (hw == 1 && i == 0)) P.rect(std::floor(x) + (float)i, std::floor(y) - ch, 1, std::max(1.0f, ch * 0.5f), lerpC(lit, tip, 0.6f));   // the lit ridge
  }
  P.rect(std::floor(x) - (hw > 1 ? 1.0f : 0.0f), std::floor(y) - std::floor(h), 1, 1, tip);
  P.rect(std::floor(x) - (float)hw, std::floor(y) - 1, (float)(2 * hw + 1), 1, mulC(dark, 0.8f));   // the foot in the ground
}
// a flame tongue from a ramp sheet standing on (x, y)
template <class Pt> void flame(Pt& P, Sheet& rs, int size, int frame, float x, float y, float alpha, int blend = 0) {
  const FlameCell& F = FLAME[std::clamp(size, 0, 2)];
  const int fr = ((frame % FLAME_FR) + FLAME_FR) % FLAME_FR;
  P.blit(rs, fr * F.w, F.y, F.w, F.h, std::floor(x - F.w * 0.5f), std::floor(y - (float)F.h + 1), (float)F.w, (float)F.h, Color(1, 1, 1, alpha), blend, false);
}

// ------------------------------------------------------------------ the phases: event and update
inline Look statusLook(Status s) {
  Look L;
  const uint32_t c = magic::statusColor(s);
  const Color b = cu(c);
  auto P = [](Color x) { return pk(withA(x, 1)); };
  L.ramp[0] = P(mulC(b, 0.3f)); L.ramp[1] = P(mulC(b, 0.6f)); L.ramp[2] = P(b); L.ramp[3] = P(lerpC(b, Color(1, 1, 1), 0.45f)); L.ramp[4] = P(lerpC(b, Color(1, 1, 1), 0.85f));
  L.ess = s == Status::Burning ? Essence::Fire : s == Status::Frozen || s == Status::Chilled ? Essence::Frost : s == Status::Shocked ? Essence::Storm : Essence::Arcane;
  L.motion = Motion::Pulse;
  return L;
}
inline Look fireLook() {
  Look L;
  const magic::FxStyle f = essenceStyle(Essence::Fire, School::Destruction);
  for (int i = 0; i < 5; i++) L.ramp[i] = f.ramp[i];
  L.ess = Essence::Fire; L.motion = Motion::Boil; L.part = MPart::Ember;
  return L;
}
inline int statusIndex(const std::string& s) {
  for (int i = 0; i < magic::STATUSES; i++) if (s == magic::statusName((Status)i)) return i;
  return -1;
}
inline int surfaceIndex(const std::string& s) {
  for (int i = 1; i < (int)Surface::COUNT; i++) if (s == magic::surfaceName((Surface)i)) return i;
  if (s == "SHOCK") return (int)Surface::Shock;
  return 0;
}
inline uint32_t surfCol(Surface s, int i) {   // the surfaces' identity colours (dark, mid, light)
  static const uint32_t c[(int)Surface::COUNT][3] = {
    {0, 0, 0}, {rgba(24, 56, 98), rgba(52, 110, 162), rgba(170, 214, 236)}, {rgba(80, 130, 184), rgba(170, 218, 240), rgba(244, 252, 255)},
    {rgba(20, 16, 22), rgba(70, 52, 80), rgba(160, 150, 190)}, {rgba(120, 30, 10), rgba(250, 120, 30), rgba(255, 230, 140)},
    {rgba(40, 70, 20), rgba(110, 170, 50), rgba(200, 240, 120)}, {rgba(40, 40, 140), rgba(130, 150, 255), rgba(235, 240, 255)},
    {rgba(150, 160, 170), rgba(220, 226, 232), rgba(255, 255, 255)}, {rgba(60, 6, 8), rgba(130, 16, 20), rgba(210, 70, 70)}};
  return c[(int)s % (int)Surface::COUNT][i];
}

// a burst of particles shaped by the look's motion (the silhouette's sheddings)
inline void burstParts(World& W, const Look& L, Vec2 p, float S, int n, bool big) {
  const PartKind pk0 = partOf(L.part);
  for (int i = 0; i < n; i++) {
    const float a = W.rnd.r(0, kTau), sp = W.rnd.r(30, 110) * std::sqrt(S) * (big ? 1.4f : 1.0f);
    Vec2 v(std::cos(a) * sp, std::sin(a) * sp * 0.6f - 20.0f);
    switch (L.motion) {
      case Motion::Boil: {   // embers flung, falling, and a few rising smoke puffs
        if (Part* q = W.spawn(PK_EMBER, p, v * 1.2f, W.rnd.r(0.35f, 0.8f), L.ramp[1], L.ramp[3], L.ramp[4], W.rnd.f() < 0.3f ? 2.0f : 1.0f, 150)) q->drag = 1.2f;
        if (i % 5 == 0) if (Part* q = W.spawn(PK_SMOKE, p + Vec2(W.rnd.r(-8, 8) * S, -12.0f * S - W.rnd.r(0, 6)), Vec2(W.rnd.r(-8, 8), W.rnd.r(-26, -12)), W.rnd.r(0.8f, 1.3f), rgba(70, 64, 62), rgba(132, 122, 116), rgba(170, 160, 150), S * W.rnd.r(0.6f, 0.9f), -6, PF_DIM)) q->life = q->max * 0.999f;
        break;
      }
      case Motion::Shatter: {   // shards on ballistic arcs that land and glint
        if (Part* q = W.spawn(pk0 == PK_BONE ? PK_BONE : PK_SHARD, p + Vec2(0, 8), Vec2(v.x * 1.1f, v.y * 0.4f), W.rnd.r(0.5f, 1.0f), L.ramp[1], L.ramp[3], L.ramp[4], W.rnd.f() < 0.4f ? 2.0f : 1.0f, 0, PF_Z | PF_DIM)) {
          q->z = 8; q->vz = W.rnd.r(40, 110); q->grav = 260;
        }
        if (i % 6 == 0) W.spawn(PK_PUFF, p + Vec2(W.rnd.r(-10, 10), W.rnd.r(0, 8)) * S, Vec2(W.rnd.r(-14, 14), W.rnd.r(-6, -1)), W.rnd.r(0.4f, 0.7f), L.ramp[1], L.ramp[2], L.ramp[3], 0.5f * S, -2, PF_ADD);   // frost mist
        break;
      }
      case Motion::Fork:   // sparks: fast, short, streaking
        if (Part* q = W.spawn(PK_SPARK, p, v * 1.8f, W.rnd.r(0.15f, 0.4f), L.ramp[1], L.ramp[3], L.ramp[4], 1, 220)) q->drag = 2.0f;
        break;
      case Motion::Rise:   // warm motes rising and swaying
        W.spawn(pk0 == PK_SPARK ? PK_MOTE : pk0, p + Vec2(W.rnd.r(-10, 10), W.rnd.r(-4, 6)) * S, Vec2(W.rnd.r(-8, 8), W.rnd.r(-60, -25)), W.rnd.r(0.7f, 1.4f), L.ramp[1], L.ramp[3], L.ramp[4], W.rnd.f() < 0.3f ? 2.0f : 1.0f, -10, PF_ADD | PF_FLICKER);
        break;
      case Motion::Tear:   // pulled into the rift: inward, then gone
        if (Part* q = W.spawn(i % 3 == 0 ? PK_GLYPH : PK_MOTE, p + Vec2(std::cos(a), std::sin(a) * 0.6f) * (22.0f * S), Vec2(-std::cos(a), -std::sin(a) * 0.6f) * (50.0f * std::sqrt(S)), W.rnd.r(0.35f, 0.6f), L.ramp[1], L.ramp[3], L.ramp[4], 1, 0, PF_ADD | PF_INWARD))
          q->glyph = L.glyph * 8u + (uint32_t)(i & 7);
        break;
      case Motion::Shimmer:   // petals spiralling out: (M9 fixer, review: "a cloud of random pink and white pixels") half as
        // many, launched on a turning ring (outward and round) so they read as a designed swirl, not as render noise
        if (i % 2 == 0)
          if (Part* q = W.spawn(pk0 == PK_SPARK ? PK_PETAL : pk0, p + Vec2(std::cos(a), std::sin(a) * 0.6f) * (6.0f * S),
                                (Vec2(std::cos(a), std::sin(a) * 0.6f) * 0.55f + Vec2(-std::sin(a), std::cos(a) * 0.6f)) * (62.0f * std::sqrt(S)),
                                W.rnd.r(0.5f, 0.8f), L.ramp[1], L.ramp[2], L.ramp[3], 1, 0, PF_ADD))
            q->drag = 2.2f;
        break;
      case Motion::Pulse: {   // stone: rubble with weight; tide: droplets; the rest: chips of the essence
        const bool tide = L.ess == Essence::Tide;
        if (Part* q = W.spawn(tide ? PK_DROP : PK_ROCK, p + Vec2(0, 8), Vec2(v.x, v.y * 0.4f), W.rnd.r(0.5f, 1.0f), tide ? L.ramp[1] : rgba(70, 52, 38), tide ? L.ramp[3] : rgba(140, 110, 78), tide ? L.ramp[4] : rgba(196, 168, 128), W.rnd.f() < 0.4f ? 2.0f : 1.0f, 0, PF_Z | PF_DIM)) {
          q->z = 6; q->vz = W.rnd.r(50, 130) * (big ? 1.3f : 1.0f); q->grav = 300;
        }
        if (!tide && i % 7 == 0) W.spawn(PK_SMOKE, p + Vec2(W.rnd.r(-14, 14) * S, W.rnd.r(4, 10)), Vec2(W.rnd.r(-18, 18), W.rnd.r(-8, -2)), W.rnd.r(0.4f, 0.7f), rgba(90, 76, 62), rgba(150, 132, 110), rgba(190, 170, 146), 0.55f * S, -3, PF_DIM);
        break;
      }
      case Motion::Fall:   // stars / petals raining down onto the mark
        W.spawn(pk0, p + Vec2(W.rnd.r(-26, 26) * S, W.rnd.r(-80, -30)), Vec2(W.rnd.r(-6, 6), W.rnd.r(60, 110)), W.rnd.r(0.4f, 0.8f), L.ramp[1], L.ramp[3], L.ramp[4], 1, 40, PF_ADD | PF_FLICKER);
        break;
      default:   // Spiral, Drift: the essence's particles swirling out and floating
        if (Part* q = W.spawn(pk0, p, Vec2(std::cos(a + 1.2f), std::sin(a + 1.2f) * 0.6f) * (70.0f * std::sqrt(S)), W.rnd.r(0.6f, 1.2f), L.ramp[1], L.ramp[2], L.ramp[4], 1, L.motion == Motion::Drift ? 12.0f : 0.0f, (pk0 == PK_LEAF || pk0 == PK_FEATHER || pk0 == PK_SMOKE) ? PF_DIM : PF_ADD)) q->drag = 1.4f;
        break;
    }
  }
}

inline void spawnImpact(World& W, const Look& L, Vec2 p, Vec2 q, float f, float S) {
  const bool big = spectacle(L);
  float life = L.motion == Motion::Tear ? 0.9f : L.motion == Motion::Rise ? 0.85f : L.motion == Motion::Shatter ? 0.75f : L.motion == Motion::Boil ? 0.62f : 0.6f;
  if (big) life *= 1.35f;
  if (Fx* x = W.addFx((uint8_t)Phase::Impact, L, p, q, life, f)) (void)x;
  burstParts(W, L, p, S, (int)(12 + 10 * S) + (big ? 16 : 0), big);
  W.kick((L.motion == Motion::Boil || L.motion == Motion::Pulse ? 2.0f : 1.4f) * std::min(3.5f, S) + (big ? 3.5f : 0.0f));
  W.sound(impactSound(L), std::clamp(1.0f / std::sqrt(S), 0.55f, 1.2f), std::min(1.3f, 0.75f + 0.2f * S));
  if (L.ess == Essence::Storm) { W.flash(Color(0.75f, 0.8f, 1.0f), std::min(0.5f, 0.2f + 0.1f * S)); W.lightLift(Color(0.6f, 0.65f, 1.0f), 0.9f); }
  else if (big) W.flash(L.c(3), 0.22f);
  if (L.ess == Essence::Storm && (L.size >= 2 || f >= 1.6f)) W.addFx(FX_STRIKE, L, p, p, 0.3f, f);
  if (L.form == Form::Storm) { if (Fx* x = W.addFx(FX_STORMCELL, L, p, p, 2.6f, f)) x->delay = 0; }
  if (L.ess == Essence::Star) {
    bool have = false;
    for (const Fx& o : W.fx) have |= o.kind == FX_STARS && len2(o.p - p) < 60 * 60;
    if (!have) W.addFx(FX_STARS, L, p, p, 1.6f, f);
  }
}

// (the launch spray's spread: a narrow fan forward)
inline float W_SPRAY(int i) { return 0.8f + 0.4f * (float)((i * 7) % 5) / 4.0f; }

inline void World::event(Phase ph, const Look& L, Vec2 p, Vec2 q, float f, uint32_t follow, const std::string& s) {
  events++;
  if (!(f > 0)) f = 1;
  const float S = baseScale(L) * std::max(0.6f, f);
  switch (ph) {
    case Phase::WindUp: {
      for (Fx& o : fx) if (o.kind == (uint8_t)Phase::WindUp && o.follow == follow && follow) o.life = std::min(o.life, o.t + 0.1f);
      if (Fx* x = addFx((uint8_t)Phase::WindUp, L, p, p + Vec2(0, 8), 5.0f, f, follow)) (void)x;
      sound(Sfx::SpellHum, humPitch(L), std::min(1.2f, 0.55f + 0.15f * S));
      if (L.ess == Essence::Star) addFx(FX_STARS, L, p, p, 2.4f, f);
      break;
    }
    case Phase::Charge: {
      Fx* w = nullptr;
      for (Fx& o : fx) if (o.kind == (uint8_t)Phase::WindUp && o.follow == follow) w = &o;
      if (w) { w->charge += 1; w->mag = f; w->v = Vec2(w->t, 0); w->p = p; }
      for (int i = 0; i < 10 + (int)(4 * S); i++) {
        const float a = kTau * (float)i / (10.0f + 4 * S);
        spawn(PK_SPARK, p, Vec2(std::cos(a), std::sin(a) * 0.6f) * 70.0f, 0.22f, L.ramp[1], L.ramp[3], L.ramp[4], 1, 0);
      }
      sound(Sfx::SpellCharge, 0.85f + 0.18f * (w ? w->charge : 1), 0.8f);
      kick(0.8f + 0.3f * (w ? w->charge : 1));
      break;
    }
    case Phase::Launch: {
      for (Fx& o : fx) if (o.kind == (uint8_t)Phase::WindUp && o.follow == follow && follow) o.life = std::min(o.life, o.t + 0.08f);
      addFx((uint8_t)Phase::Launch, L, p, q, 0.3f, f, follow);
      Vec2 dir = len2(q - p) > 1 ? norm(q - p) : Vec2(0, -1);
      for (int i = 0; i < 8 + (int)(5 * S); i++) {   // the muzzle spray forward, a recoil puff back
        const Vec2 v = (dir * W_SPRAY(i) + Vec2(rnd.r(-0.5f, 0.5f), rnd.r(-0.5f, 0.5f))) * rnd.r(60, 140);
        spawn(L.motion == Motion::Boil ? PK_EMBER : PK_SPARK, p + dir * 4.0f, v, rnd.r(0.12f, 0.3f), L.ramp[1], L.ramp[3], L.ramp[4], 1, 0);
      }
      for (int i = 0; i < 3; i++) spawn(PK_PUFF, p - dir * 3.0f, dir * -rnd.r(10, 24) + Vec2(rnd.r(-8, 8), rnd.r(-8, 0)), rnd.r(0.3f, 0.5f), L.ramp[0], L.ramp[2], L.ramp[3], 0.5f, 0, PF_ADD);
      kick((spectacle(L) ? 2.4f : 1.2f) + 0.4f * S);
      sound(castSound(L), std::clamp(1.0f / std::sqrt(S), 0.6f, 1.2f), std::min(1.2f, 0.7f + 0.15f * S));
      motif(L.motif, timbreOf(L), 0.55f);
      if (L.form == Form::Meteor) {   // the meteor sets out toward the aim; an Impact retargets it to its true mark
        const Vec2 mark = len2(q - p) > 30 * 30 ? q : p + dir * 80.0f;
        if (Fx* m = addFx(FX_METEOR, L, mark + Vec2(-150, -250) * std::min(1.4f, S * 0.6f), mark, 0.7f, f, 0)) { m->v = m->p; }
      }
      break;
    }
    case Phase::Impact: {
      if (L.form == Form::Meteor) {
        Fx* m = nullptr;
        for (Fx& o : fx) if (o.kind == FX_METEOR && !o.hit && o.L.spell == L.spell) m = &o;
        if (m) {   // the mark is known now: fall there, and land soon
          const float e = sat(m->t / m->life);
          m->v = m->v + (m->q - m->v) * (e * e);   // (from where it is now)
          m->q = p; m->t = 0; m->life = std::max(0.12f, (1.0f - e) * 0.5f);
          m->mag = std::max(m->mag, f);
        } else if (Fx* n = addFx(FX_METEOR, L, p + Vec2(-110, -210), p, 0.4f, f, 0)) {
          n->v = n->p;
        }
        break;
      }
      spawnImpact(*this, L, p, q, f, S);
      break;
    }
    case Phase::Aftermath: {
      const DecalKind dk = decalFor(L);
      addDecal(dk, p, std::min(2.4f, 0.7f + 0.3f * S), (dk == D_CRATER ? 22.0f : 12.0f) + 3.0f * S, L);
      for (int i = 0; i < (int)(4 + 3 * S); i++) {   // what lingers: embers, smoke, mist, motes
        const Vec2 at = p + Vec2(rnd.r(-10, 10), rnd.r(-4, 4)) * S;
        if (L.motion == Motion::Boil) {
          spawn(PK_EMBER, at, Vec2(rnd.r(-4, 4), rnd.r(-22, -8)), rnd.r(0.8f, 1.8f), L.ramp[1], L.ramp[2], L.ramp[3], 1, -4, PF_ADD | PF_FLICKER);
          if (i % 2 == 0) spawn(PK_SMOKE, at, Vec2(rnd.r(-5, 5), rnd.r(-16, -8)), rnd.r(1.2f, 2.2f), rgba(36, 32, 32), rgba(70, 64, 62), rgba(104, 96, 92), rnd.r(0.7f, 1.1f), -2, PF_DIM);
        } else if (L.motion == Motion::Shatter) {
          if (i % 2 == 0) spawn(PK_PUFF, at, Vec2(rnd.r(-5, 5), rnd.r(-4, -1)), rnd.r(0.8f, 1.4f), L.ramp[1], L.ramp[2], L.ramp[3], 0.45f, 0, PF_ADD);
          spawn(PK_STAR, at, Vec2(0, rnd.r(-4, 0)), rnd.r(0.4f, 1.2f), L.ramp[2], L.ramp[3], L.ramp[4], 0.8f, 0, PF_ADD | PF_FLICKER);
        } else {
          spawn(PK_MOTE, at, Vec2(rnd.r(-4, 4), rnd.r(-16, -6)), rnd.r(0.8f, 1.6f), L.ramp[1], L.ramp[2], L.ramp[3], 1, -2, PF_ADD | PF_FLICKER);
        }
      }
      break;
    }
    case Phase::Fizzle: {
      for (int i = 0; i < 4; i++) spawn(PK_SMOKE, p + Vec2(rnd.r(-3, 3), rnd.r(-3, 3)), Vec2(rnd.r(-10, 10), rnd.r(-20, -8)), rnd.r(0.5f, 0.8f), rgba(60, 58, 64), rgba(110, 106, 116), rgba(150, 146, 156), 0.5f, -4, PF_DIM);
      for (int i = 0; i < 3; i++) spawn(PK_SPARK, p, Vec2(rnd.r(-30, 30), rnd.r(-30, 0)), 0.3f, L.ramp[0], L.ramp[2], L.ramp[3], 1, 160);
      sound(Sfx::SpellFizzle, 1.0f, 0.8f);
      break;
    }
    case Phase::Channel: {
      Fx* c = nullptr;
      for (Fx& o : fx) if (o.kind == (uint8_t)Phase::Channel && o.follow == follow && o.L.spell == L.spell) c = &o;
      if (c) { c->p = p; c->q = q; c->life = c->t + 0.16f; c->mag = f; }
      else if (Fx* x = addFx((uint8_t)Phase::Channel, L, p, q, 0.16f, f, follow)) { (void)x; sound(castSound(L), 1.1f, 0.6f); }
      for (int i = 0; i < 2; i++) spawn(L.motion == Motion::Boil ? PK_EMBER : PK_SPARK, q, Vec2(rnd.r(-60, 60), rnd.r(-60, 20)), 0.25f, L.ramp[1], L.ramp[3], L.ramp[4], 1, 120);
      break;
    }
    case Phase::Release: {
      addFx((uint8_t)Phase::Release, L, p, p, 0.4f, f, follow);
      burstParts(*this, L, p + Vec2(0, -8), 1, 8, false);
      sound(Sfx::Shimmer, 0.7f, 0.5f);
      break;
    }
    case Phase::Combo: {
      const int si = surfaceIndex(s);
      if (Fx* x = addFx((uint8_t)Phase::Combo, L, p, q, 0.9f, f)) x->sub = (uint8_t)si;
      const Surface sf = (Surface)si;
      for (int i = 0; i < 14; i++) {
        const float a = rnd.r(0, kTau), sp = rnd.r(20, 70);
        if (sf == Surface::Steam || sf == Surface::Poison)
          spawn(PK_PUFF, p + Vec2(rnd.r(-14, 14), rnd.r(-6, 6)), Vec2(std::cos(a) * sp * 0.4f, -rnd.r(14, 36)), rnd.r(0.9f, 1.6f), surfCol(sf, 1), sf == Surface::Steam ? rgba(255, 255, 255) : surfCol(sf, 2), surfCol(sf, 2), rnd.r(1.0f, 1.6f), -4, PF_DIM);
        else if (sf == Surface::Fire)
          spawn(PK_EMBER, p, Vec2(std::cos(a) * sp, std::sin(a) * sp * 0.5f - 40), rnd.r(0.4f, 0.9f), surfCol(sf, 0), surfCol(sf, 1), surfCol(sf, 2), 1.5f, 110);
        else if (sf == Surface::Shock)
          spawn(PK_SPARK, p, Vec2(std::cos(a) * sp * 2, std::sin(a) * sp), 0.3f, surfCol(sf, 0), surfCol(sf, 1), surfCol(sf, 2), 1, 160);
        else if (Part* q2 = spawn(sf == Surface::Ice ? PK_SHARD : PK_DROP, p + Vec2(0, 6), Vec2(std::cos(a) * sp, std::sin(a) * sp * 0.4f), rnd.r(0.5f, 0.9f), surfCol(sf, 0), surfCol(sf, 1), surfCol(sf, 2), 1, 0, PF_Z | PF_DIM)) {
          q2->z = 4; q2->vz = rnd.r(40, 90); q2->grav = 260;
        }
      }
      if (sf == Surface::Steam) sound(Sfx::Sizzle, 1.0f, 0.9f);
      else if (sf == Surface::Shock) { sound(Sfx::StormCast, 0.8f, 0.9f); flash(Color(0.7f, 0.75f, 1.0f), 0.22f); lightLift(Color(0.55f, 0.6f, 1.0f), 0.6f); }
      else if (sf == Surface::Fire) { sound(Sfx::FireBurst, 0.9f, 0.9f); kick(2.5f); }
      else if (sf == Surface::Ice) sound(Sfx::FrostShatter, 1.2f, 0.7f);
      else sound(Sfx::Sizzle, 1.3f, 0.6f);
      break;
    }
    case Phase::StatusOn: {
      const int si = statusIndex(s);
      if (si < 0) break;
      const Look SL = statusLook((Status)si);
      if (Fx* x = addFx((uint8_t)Phase::StatusOn, SL, p, p, 0.55f, 1, follow)) x->sub = (uint8_t)si;
      for (int i = 0; i < 8; i++) {
        const float a = kTau * (float)i / 8.0f;
        spawn(si == (int)Status::Charmed ? PK_HEART : si == (int)Status::Burning ? PK_EMBER : PK_MOTE, p + Vec2(0, -12), Vec2(std::cos(a) * 40, std::sin(a) * 26 - 10), 0.45f, SL.ramp[1], SL.ramp[3], SL.ramp[4], 1, 0);
      }
      if (si == (int)Status::Frozen) sound(Sfx::FrostShatter, 1.5f, 0.45f);
      break;
    }
    case Phase::Summon: {
      if (Fx* x = addFx((uint8_t)Phase::Summon, L, p, q, spectacle(L) ? 2.4f : 1.7f, f, follow)) (void)x;
      for (int i = 0; i < (int)(10 * S); i++) {   // glyphs and motes rising out of the rift
        const float a = rnd.r(0, kTau), r = rnd.r(2, 14) * S;
        if (Part* g2 = spawn(i % 3 == 0 ? PK_GLYPH : PK_MOTE, p + Vec2(std::cos(a) * r, std::sin(a) * r * 0.42f), Vec2(rnd.r(-6, 6), rnd.r(-50, -20)), rnd.r(0.6f, 1.2f), L.ramp[1], L.ramp[2], L.ramp[4], 1, -10, PF_ADD | PF_FLICKER))
          g2->glyph = L.glyph * 8u + (uint32_t)(i & 7);
      }
      sound(Sfx::RiftTear, 0.9f / std::sqrt(S), 0.9f);
      sound(Sfx::SummonRise, 1.0f / std::sqrt(S), 0.8f);
      motif(L.motif, timbreOf(L), 0.5f);
      kick(1.5f + (spectacle(L) ? 3.0f : 0.0f));
      break;
    }
    case Phase::Ward: {
      addFx((uint8_t)Phase::Ward, L, p, p, 1.2f, f, follow);
      sound(Sfx::WardRing, 1.0f / std::sqrt(S), 0.85f);
      break;
    }
    case Phase::Blink: {
      addFx((uint8_t)Phase::Blink, L, p, q, 0.5f, f, follow);
      for (int i = 0; i < 12; i++) {
        const float a = kTau * (float)i / 12.0f;
        spawn(PK_MOTE, q + Vec2(0, -10), Vec2(std::cos(a) * 50, std::sin(a) * 32), 0.35f, L.ramp[1], L.ramp[3], L.ramp[4], 1, 0);
      }
      sound(Sfx::Shimmer, 1.5f, 0.8f);
      break;
    }
    case Phase::Reaction: {
      addFx((uint8_t)Phase::Reaction, L, p, q, 0.6f, f, follow);
      sound(Sfx::WardRing, 1.4f, 0.9f);
      kick(1.0f);
      break;
    }
    default: break;
  }
}

inline void World::update(float dt, const std::function<bool(uint32_t, Vec2&)>& feetOf) {
  t += dt;
  for (size_t i = 0; i < fx.size(); i++) {
    Fx& f = fx[i];
    if (f.delay > 0) { f.delay -= dt; continue; }
    f.t += dt;
    Vec2 feet;
    if (f.follow && feetOf && feetOf(f.follow, feet)) {
      if (f.kind == (uint8_t)Phase::WindUp || f.kind == (uint8_t)Phase::Launch) { f.p = feet + Vec2(0, -8); f.q = f.kind == (uint8_t)Phase::WindUp ? feet : f.q; }
      else if (f.kind == (uint8_t)Phase::Ward || f.kind == (uint8_t)Phase::StatusOn || f.kind == (uint8_t)Phase::Release) { f.p = feet; f.q = feet; }
    }
    const float S = baseScale(f.L) * std::max(0.6f, f.mag);
    switch (f.kind) {
      case (uint8_t)Phase::WindUp: {   // particles drawn in to the hands
        const float rate = (26.0f + 10.0f * f.charge) * S;
        int n = (int)(rate * dt);
        if (rnd.f() < rate * dt - (float)n) n++;
        for (int k = 0; k < n; k++) {
          const float a = rnd.r(0, kTau), r = rnd.r(14, 24) * S;
          const Vec2 at = f.p + Vec2(std::cos(a) * r, std::sin(a) * r * 0.6f);
          Part* q = spawn(f.L.motion == Motion::Boil ? PK_EMBER : f.L.motion == Motion::Fall ? PK_STAR : PK_MOTE, at, (f.p - at) * 3.2f, 0.3f, f.L.ramp[1], f.L.ramp[3], f.L.ramp[4], 1, 0, PF_ADD | PF_INWARD);
          (void)q;
        }
        break;
      }
      case FX_METEOR: {
        const float e = sat(f.t / f.life);
        const Vec2 head = f.v + (f.q - f.v) * (e * e);
        for (int k = 0; k < 3; k++) {
          spawn(PK_EMBER, head + Vec2(rnd.r(-4, 4), rnd.r(-4, 4)), Vec2(rnd.r(-20, 20), rnd.r(-30, 10)), rnd.r(0.3f, 0.6f), f.L.ramp[1], f.L.ramp[3], f.L.ramp[4], 2, 30);
        }
        if (rnd.f() < 0.7f) spawn(PK_SMOKE, head, Vec2(rnd.r(-8, 8), rnd.r(-14, -4)), rnd.r(0.8f, 1.3f), rgba(40, 34, 34), rgba(80, 70, 66), rgba(120, 108, 100), 1.2f * S, -4, PF_DIM);
        if (!f.hit && f.t >= f.life) {
          f.hit = true;
          Look L = f.L;
          L.size = std::max<uint8_t>(L.size, 3);
          const float S2 = baseScale(L) * std::max(1.0f, f.mag);
          spawnImpact(*this, L, f.q, f.q, std::max(1.0f, f.mag) * 1.15f, S2 * 1.15f);
          addDecal(D_CRATER, f.q + Vec2(0, 4), std::min(2.4f, 0.8f + 0.3f * S2), 26.0f, L);
          kick(7.0f);
          flash(Color(1.0f, 0.62f, 0.3f), 0.42f);
          lightLift(Color(1.0f, 0.6f, 0.3f), 0.7f);
          sound(Sfx::Explode, 0.7f, 1.2f);
          f.life = f.t + 0.25f;   // the head's afterglow, then gone
        }
        break;
      }
      case FX_STORMCELL: {   // strikes at seeded moments round the mark
        const int k = (int)(f.t / 0.32f);
        if (k > (int)f.sub && f.t < f.life - 0.4f) {
          f.sub = (uint8_t)k;
          const uint32_t h = hmix(f.seed, (uint32_t)k);
          const float a = hf(h) * kTau, r = std::sqrt(hf(h >> 9)) * (f.L.radius > 0 ? f.L.radius : 60.0f) * std::sqrt(S) * 0.8f;
          const Vec2 at = f.p + Vec2(std::cos(a) * r, std::sin(a) * r * 0.6f);
          if (Fx* s = addFx(FX_STRIKE, f.L, at, at, 0.28f, f.mag)) (void)s;
          burstParts(*this, f.L, at, 1, 8, false);
          addDecal(D_CRACKS, at + Vec2(0, 2), 0.8f, 8.0f, f.L);
          flash(Color(0.75f, 0.8f, 1.0f), 0.32f);
          lightLift(Color(0.6f, 0.65f, 1.0f), 0.95f);
          kick(2.5f);
          if (k % 2 == 0) sound(Sfx::Thunder, 0.85f + 0.3f * hf(h >> 3), 0.9f);
        }
        break;
      }
      default: break;
    }
  }
  fx.erase(std::remove_if(fx.begin(), fx.end(), [](const Fx& f) { return f.delay <= 0 && f.t >= f.life; }), fx.end());
  for (size_t i = 0; i < parts.size();) {
    Part& q = parts[i];
    q.life -= dt;
    if (q.life <= 0) { q = parts.back(); parts.pop_back(); continue; }
    if (q.drag > 0) q.v = q.v * std::max(0.0f, 1.0f - q.drag * dt);
    q.p += q.v * dt;
    if (q.flags & PF_Z) {
      q.vz -= q.grav * dt;
      q.z += q.vz * dt;
      if (q.z < 0) { q.z = 0; q.vz = -q.vz * 0.32f; q.v = q.v * 0.55f; if (q.vz < 12) q.vz = 0; }
    } else {
      q.v.y += q.grav * dt;
    }
    i++;
  }
  for (size_t i = 0; i < decals.size();) {
    decals[i].t += dt;
    if (decals[i].t >= decals[i].life) { decals.erase(decals.begin() + (std::ptrdiff_t)i); continue; }
    i++;
  }
}

inline void World::trail(const Look& L, Vec2 p, Vec2 v, float mag, float dt, uint32_t seed) {
  (void)seed;
  const float S = baseScale(L) * std::max(0.6f, mag);
  const float rate = (34.0f + 16.0f * (float)L.size) * std::max(0.7f, mag);
  int n = (int)(rate * dt);
  if (rnd.f() < rate * dt - (float)n) n++;
  const Vec2 dir = norm(v), nr(-dir.y, dir.x);
  for (int k = 0; k < n; k++) {
    const Vec2 at = p - dir * rnd.r(2, 7) * S + nr * rnd.r(-2.5f, 2.5f) * S;
    const Vec2 vv = v * -0.08f + nr * rnd.r(-14, 14) + Vec2(0, rnd.r(-12, 4));
    switch (L.motion) {
      case Motion::Boil:
        spawn(PK_EMBER, at, vv + Vec2(0, -10), rnd.r(0.2f, 0.45f), L.ramp[1], L.ramp[3], L.ramp[4], rnd.f() < 0.25f ? 2.0f : 1.0f, -20, PF_ADD | PF_FLICKER);
        if (k % 4 == 0) spawn(PK_SMOKE, at - dir * 5.0f, vv * 0.3f + Vec2(0, -10), rnd.r(0.3f, 0.5f), rgba(80, 72, 68), rgba(140, 130, 122), rgba(180, 170, 160), 0.35f * S, -6, PF_DIM);
        break;
      case Motion::Shatter:
        spawn(k % 2 ? PK_STAR : PK_SHARD, at, vv * 0.5f, rnd.r(0.2f, 0.4f), L.ramp[1], L.ramp[3], L.ramp[4], 1, 40, PF_ADD | PF_FLICKER);
        if (k % 3 == 0) spawn(PK_PUFF, at, vv * 0.2f, rnd.r(0.3f, 0.5f), L.ramp[1], L.ramp[3], L.ramp[4], 0.35f * S, 0, PF_ADD);
        break;
      case Motion::Fork: spawn(PK_SPARK, at, vv * 2.0f + nr * rnd.r(-40, 40), rnd.r(0.08f, 0.2f), L.ramp[1], L.ramp[3], L.ramp[4], 1, 0); break;
      case Motion::Tear:
        if (k % 7 == 3) { if (Part* g2 = spawn(PK_GLYPH, at, vv * 0.4f + Vec2(0, -12), rnd.r(0.35f, 0.55f), L.ramp[0], L.ramp[1], L.ramp[2], 1, -4, PF_ADD)) g2->glyph = L.glyph * 8u + (uint32_t)(k & 7); }
        else if (k % 2 == 0) spawn(PK_SMOKE, at, vv * 0.4f, rnd.r(0.3f, 0.5f), L.ramp[0], L.ramp[1], L.ramp[2], 0.4f * S, -4, PF_DIM);
        else spawn(PK_MOTE, at, vv * 0.6f, rnd.r(0.2f, 0.4f), L.ramp[1], L.ramp[2], L.ramp[3], 1, 0, PF_ADD | PF_FLICKER);
        break;
      default:
        spawn(partOf(L.part), at, vv * 0.6f, rnd.r(0.25f, 0.5f), L.ramp[1], L.ramp[3], L.ramp[4], 1, L.motion == Motion::Rise ? -20.0f : 0.0f, PF_ADD | PF_FLICKER);
        break;
    }
  }
}
inline void World::missileEnd(const Look& L, Vec2 p, Vec2 v, float mag) {
  event(Phase::Impact, L, p, p - norm(v) * 8.0f, mag, 0, "");
  event(Phase::Aftermath, L, p + Vec2(0, 8), p, mag, 0, "");
}
// a body's status sheddings (every frame): embers off the burning, drips off the wet, bubbles off the poisoned, frost
// motes off the chilled, sparks off the shocked, hearts off the charmed
inline void statusTick(World& W, Vec2 feet, uint16_t bits, float bw, float bh, float dt) {
  auto chance = [&](float rate) { return W.rnd.f() < rate * dt; };
  auto on = [&](Status s) { return (bits & magic::statusBit(s)) != 0; };
  const Vec2 body(feet.x, feet.y - bh * 0.5f);
  if (on(Status::Burning) && chance(26))
    W.spawn(PK_EMBER, body + Vec2(W.rnd.r(-bw, bw) * 0.5f, W.rnd.r(-bh, bh) * 0.35f), Vec2(W.rnd.r(-8, 8), W.rnd.r(-40, -20)), W.rnd.r(0.3f, 0.6f), rgba(150, 30, 10), rgba(250, 140, 40), rgba(255, 230, 150), 1, -10, PF_ADD | PF_FLICKER);
  if (on(Status::Burning) && chance(5))
    W.spawn(PK_SMOKE, Vec2(feet.x + W.rnd.r(-3, 3), feet.y - bh), Vec2(W.rnd.r(-4, 4), -18), 0.9f, rgba(40, 36, 36), rgba(70, 64, 62), rgba(100, 94, 90), 0.5f, -2, PF_DIM);
  if (on(Status::Wet) && chance(7))
    if (Part* q = W.spawn(PK_DROP, Vec2(feet.x + W.rnd.r(-bw, bw) * 0.4f, feet.y), Vec2(0, 0), 0.5f, rgba(30, 70, 120), rgba(90, 160, 220), rgba(220, 244, 255), 1, 0, PF_Z | PF_DIM)) { q->z = bh * W.rnd.r(0.3f, 0.6f); q->grav = 200; }
  if (on(Status::Poisoned) && chance(9))
    W.spawn(PK_MOTE, body + Vec2(W.rnd.r(-bw, bw) * 0.4f, W.rnd.r(0, bh * 0.4f)), Vec2(W.rnd.r(-3, 3), -16), W.rnd.r(0.5f, 0.9f), rgba(40, 80, 20), rgba(130, 200, 60), rgba(210, 250, 140), 1, -4, PF_ADD | PF_FLICKER);
  if ((on(Status::Chilled) || on(Status::Frozen)) && chance(6))
    W.spawn(PK_STAR, body + Vec2(W.rnd.r(-bw, bw) * 0.6f, W.rnd.r(-bh, bh) * 0.5f), Vec2(0, 4), W.rnd.r(0.3f, 0.6f), rgba(80, 140, 220), rgba(190, 230, 255), rgba(255, 255, 255), 0.8f, 0, PF_ADD | PF_FLICKER);
  if (on(Status::Chilled) && chance(1.2f))
    W.spawn(PK_PUFF, Vec2(feet.x + 3, feet.y - bh + 4), Vec2(8, -3), 0.7f, rgba(170, 200, 230), rgba(220, 238, 250), rgba(255, 255, 255), 0.35f, 0, PF_DIM);
  if (on(Status::Shocked) && chance(14))
    W.spawn(PK_SPARK, body + Vec2(W.rnd.r(-bw, bw) * 0.5f, W.rnd.r(-bh, bh) * 0.4f), Vec2(W.rnd.r(-60, 60), W.rnd.r(-60, 30)), 0.15f, rgba(60, 50, 160), rgba(170, 180, 255), rgba(255, 255, 255), 1, 0);
  if (on(Status::Charmed) && chance(3))
    W.spawn(PK_HEART, Vec2(feet.x + W.rnd.r(-4, 4), feet.y - bh - 2), Vec2(W.rnd.r(-4, 4), -18), 0.9f, rgba(120, 20, 60), rgba(240, 90, 150), rgba(255, 200, 230), 1, 0, PF_ADD);
}

// ------------------------------------------------------------------ drawing: decals and the ground layer
template <class Pt> void drawGround(Pt& P, World& W, const Ctx& C) {
  for (const Decal& d : W.decals) {
    const Vec2 s = scr(C, d.p);
    const int w = decalW(d.kind), h = decalH(d.kind);
    const float sc = d.size;
    const float dw = std::floor((float)w * sc * 0.5f) * 2, dh = std::floor((float)h * sc * 0.5f) * 2;
    if (!onScr(C, s, dw)) continue;
    const float a = sat(d.t / 0.15f) * sat((d.life - d.t) / 3.0f);
    Sheet& sh = W.decalSheet(d.kind, d.seed, d.ramp);
    P.blit(sh, 0, 0, w, h, s.x - dw * 0.5f, s.y - dh * 0.5f, dw, dh, Color(1, 1, 1, a), 0, (d.seed & 4) != 0);
  }
  for (const Fx& f : W.fx) {
    if (f.delay > 0) continue;
    const Look& L = f.L;
    const float S = baseScale(L) * std::max(0.6f, f.mag);
    const float u = sat(f.t / f.life), k = 1 - u;
    switch (f.kind) {
      case (uint8_t)Phase::WindUp: {   // the sigil under the caster, turning and growing with the charge
        const Vec2 s = scr(C, f.q);
        const float in = sat(f.t / 0.2f) * sat((f.life - f.t) / 0.1f);
        const float grow = std::min(2.6f, (0.55f + 0.3f * sat(f.t / 0.7f) + 0.18f * f.charge) * S);
        const float w = std::floor(SIG_W * grow * 0.5f) * 2, h = std::floor(SIG_H * grow * 0.5f) * 2;
        const int fr = (int)(C.t * 9.0f) % SIG_FR;
        Sheet& sg = W.sigil(L.glyph);
        gglow(P, W, s.x, s.y, w * 0.45f, L.c(2, 0.28f * in));
        P.blit(sg, fr * SIG_W, 0, SIG_W, SIG_H, s.x - w * 0.5f, s.y - h * 0.5f + 1, w, h, L.c(0, 0.5f * in), 0, false);   // its shadow-line
        P.blit(sg, fr * SIG_W, 0, SIG_W, SIG_H, s.x - w * 0.5f, s.y - h * 0.5f, w, h, L.c(3, 0.75f * in), 0, false);   // (its own hue on any ground)
        P.blit(sg, fr * SIG_W, 0, SIG_W, SIG_H, s.x - w * 0.5f, s.y - h * 0.5f, w, h, L.c(4, 0.3f * in), 1, false);
        break;
      }
      case (uint8_t)Phase::Impact: {
        const Vec2 s = scr(C, f.p);
        if (!onScr(C, s, 80 * S)) break;
        gglow(P, W, s.x, s.y + 7, 24.0f * S * (0.6f + 0.4f * k), L.c(2, 0.45f * k));
        ring(P, s.x, s.y + 7, (6 + 30 * u) * S, (6 + 30 * u) * S * 0.42f, L.c(3, 0.9f * k), true, 2);
        if (spectacle(L)) ring(P, s.x, s.y + 7, (10 + 60 * u) * S, (10 + 60 * u) * S * 0.42f, L.c(2, 0.6f * k), true, 2, 0.04f, C.t * 9);
        if (L.motion == Motion::Rise || L.motion == Motion::Spiral) {   // the healing sigil blooming on the ground
          const float g = std::min(2.4f, (0.7f + 0.4f * u) * S);
          const float w = std::floor(SIG_W * g * 0.5f) * 2, h = std::floor(SIG_H * g * 0.5f) * 2;
          P.blit(W.sigil(L.glyph), ((int)(C.t * 10) % SIG_FR) * SIG_W, 0, SIG_W, SIG_H, s.x - w * 0.5f, s.y + 6 - h * 0.5f, w, h, L.c(3, 0.75f * k), 0, false);
          P.blit(W.sigil(L.glyph), ((int)(C.t * 10) % SIG_FR) * SIG_W, 0, SIG_W, SIG_H, s.x - w * 0.5f, s.y + 6 - h * 0.5f, w, h, L.c(4, 0.3f * k), 1, false);
        }
        break;
      }
      case (uint8_t)Phase::Summon: {   // the rift on the ground: a dark well, swirling glyphs, a lit rim
        const Vec2 s = scr(C, f.p);
        const float open = sat(f.t / 0.25f) * sat((f.life - f.t) / 0.35f);
        const float rx = 15.0f * S * open, ry = rx * 0.42f;
        if (rx < 1) break;
        gglow(P, W, s.x, s.y, rx * 1.6f, L.c(2, 0.4f * open));
        for (int j = -(int)ry; j <= (int)ry; j++) {
          const float tt = (float)j / std::max(1.0f, ry), ww = rx * std::sqrt(std::max(0.0f, 1 - tt * tt));
          P.rect(std::floor(s.x - ww), std::floor(s.y + (float)j), std::floor(ww * 2 + 1), 1, Color(0.03f, 0.01f, 0.06f, 0.92f * open));
        }
        const float w = std::floor(rx * 2 * 0.95f), h = std::floor(ry * 2 * 0.95f);
        P.blit(W.sigil(L.glyph), ((int)(C.t * 14) % SIG_FR) * SIG_W, 0, SIG_W, SIG_H, s.x - w * 0.5f, s.y - h * 0.5f, w, h, L.c(2, 0.7f * open), 1, true);
        ring(P, s.x, s.y, rx, ry, L.c(2, 0.9f * open), false, 2, 0.05f, C.t * 12);
        ring(P, s.x, s.y, rx, ry, L.c(4, 0.5f * open), true, 1, 0.05f, C.t * 12);
        ring(P, s.x, s.y - 1, rx + 1, ry + 0.5f, L.c(4, 0.5f * open), true, 1, 0.08f, -C.t * 9);
        if (spectacle(L))   // the ground cracking open round a great summoning
          for (int c = 0; c < 8; c++) {
            const float a = kTau * (float)c / 8.0f + hf(f.seed) * 2;
            const float l = rx * (1.3f + 0.9f * hf(hmix(f.seed, (uint32_t)c))) * sat(f.t / 0.5f);
            line(P, (int)(s.x + std::cos(a) * rx), (int)(s.y + std::sin(a) * ry), (int)(s.x + std::cos(a) * l), (int)(s.y + std::sin(a) * l * 0.42f), Color(0.05f, 0.02f, 0.08f, 0.8f * open), false);
            line(P, (int)(s.x + std::cos(a) * rx), (int)(s.y + std::sin(a) * ry) - 1, (int)(s.x + std::cos(a) * l), (int)(s.y + std::sin(a) * l * 0.42f) - 1, L.c(3, 0.6f * open), true);
          }
        break;
      }
      case FX_METEOR: {   // its mark: a red sigil and a growing shadow
        if (f.hit) break;
        const Vec2 s = scr(C, f.q);
        const float e = sat(f.t / f.life);
        const float r = (10 + 24 * e) * S * 0.6f;
        gglow(P, W, s.x, s.y, r, Color(0, 0, 0, 0.25f + 0.35f * e), true);
        const float pulse = 0.6f + 0.4f * std::sin(C.t * 14);
        ring(P, s.x, s.y, r, r * 0.42f, L.c(1, 0.8f), false, 2, 0, 0, 3);
        const float w = std::floor(SIG_W * S * 0.45f) * 2, h = std::floor(SIG_H * S * 0.45f) * 2;
        P.blit(W.sigil(L.glyph), ((int)(C.t * 12) % SIG_FR) * SIG_W, 0, SIG_W, SIG_H, s.x - w * 0.5f, s.y - h * 0.5f, w, h, L.c(1, 0.55f + 0.3f * e), 0, false);
        P.blit(W.sigil(L.glyph), ((int)(C.t * 12) % SIG_FR) * SIG_W, 0, SIG_W, SIG_H, s.x - w * 0.5f, s.y - h * 0.5f, w, h, L.c(2, (0.2f + 0.3f * e) * pulse), 1, false);
        break;
      }
      case (uint8_t)Phase::Launch: {
        if (L.form == Form::Mend || L.form == Form::Ward || L.form == Form::Aura || L.form == Form::Nova) {   // a ring from the feet
          const Vec2 s = scr(C, f.p + Vec2(0, 8));
          ring(P, s.x, s.y, (5 + 18 * u) * S, (5 + 18 * u) * S * 0.42f, L.c(3, k), true, 2);
        }
        break;
      }
      default: break;
    }
  }
}

// ------------------------------------------------------------------ drawing: missiles
template <class Pt> void drawMissileShadow(Pt& P, World& W, const Ctx& C, const Look& L, Vec2 p, float mag) {
  const Vec2 s = scr(C, p + Vec2(0, 9));
  const float S = baseScale(L) * std::max(0.6f, mag);
  gglow(P, W, s.x, s.y, 5.0f * S, Color(0, 0, 0, 0.4f), true);
  gglow(P, W, s.x, s.y - 1, 9.0f * S, L.c(2, 0.25f));   // its light on the ground under it
}
template <class Pt> void drawMissile(Pt& P, World& W, const Ctx& C, const Look& L, Vec2 p, Vec2 v, float mag, float age, uint32_t seed, float flown = 1e9f) {
  const Vec2 s = scr(C, p);
  const float S = baseScale(L) * std::max(0.6f, mag);
  if (!onScr(C, s, 50 * S)) return;
  const Vec2 dir = norm(v), nr(-dir.y, dir.x);
  const float tt = C.t + hf(seed) * 3;
  Sheet& rs = W.rampSheet(L);
  const bool lance = L.form == Form::Lance || L.motion == Motion::Shatter;
  // ---- the trail: stacked bands from hot to dark back along the path, wobbling by the motion. (M9 fixer) It never
  //      reaches back past the launch point (flown: the distance from it), and the additive hot half is weighed by how
  //      much its squares overlap, fading toward the tail, so a big upcast reads as a tapering streak, not a white slab.
  const float step = 2.0f * std::sqrt(S);
  const int n = std::min((int)(14 * S) + 6, (int)(std::max(0.0f, flown) / step));
  for (int i = n; i >= 1; i--) {
    const float q = (float)i / (float)n;   // 0 head .. 1 tail
    float wob = 0;
    switch (L.motion) {
      case Motion::Boil: wob = std::sin(tt * 22 - (float)i * 0.9f) * 1.6f * q * S; break;
      case Motion::Fork: wob = (hf(hmix(seed ^ (uint32_t)(tt * 25), (uint32_t)i)) - 0.5f) * 5.0f * q * S; break;
      case Motion::Spiral: case Motion::Shimmer: wob = std::sin(tt * 16 - (float)i * 0.6f) * 3.0f * q * S; break;
      case Motion::Tear: wob = std::sin((float)i * 1.3f) * 1.2f * q * S; break;
      default: wob = std::sin(tt * 10 - (float)i * 0.5f) * 0.8f * q * S; break;
    }
    const Vec2 at = s - dir * ((float)i * 2.0f * std::sqrt(S)) + nr * wob;
    const float w = std::max(1.0f, (lance ? 3.0f : 5.0f) * S * (1 - q * 0.85f));
    const Color c = q < 0.25f ? L.c(4, 0.9f) : q < 0.5f ? L.c(3, 0.8f) : q < 0.75f ? L.c(2, 0.65f * (1.2f - q)) : L.c(1, 0.6f * (1.1f - q));
    if (L.motion == Motion::Tear && q > 0.3f) P.rect(std::floor(at.x - w * 0.5f), std::floor(at.y - w * 0.5f), std::floor(w), std::floor(w), L.c(0, 0.7f * (1 - q)));
    else if (q < 0.5f) {
      const float cover = std::min(1.0f, 1.3f * step / w) * (1.0f - 1.2f * q);   // the overlap of the squares, the taper
      const float cw = std::max(1.0f, std::floor(w * 0.55f));                     // a narrower hot core over a softer band
      P.add(std::floor(at.x - w * 0.5f), std::floor(at.y - w * 0.5f), std::floor(w), std::floor(w), withA(c, c.a * cover * 0.7f));
      P.add(std::floor(at.x - cw * 0.5f), std::floor(at.y - cw * 0.5f), cw, cw, withA(L.c(4), 0.9f * cover));
    }
    else P.rect(std::floor(at.x - w * 0.5f), std::floor(at.y - w * 0.5f), std::floor(w), std::floor(w), withA(L.c(q < 0.75f ? 3 : 2), 0.55f * (1.1f - q)));   // the cool tail keeps its hue on any ground
    if (L.motion == Motion::Shimmer && i % 2 == 0) {   // the chromatic split
      P.add(std::floor(at.x - w * 0.5f) - 2, std::floor(at.y - w * 0.5f), 1, std::floor(w), Color(1.0f, 0.4f, 0.8f, 0.5f * (1 - q)));
      P.add(std::floor(at.x + w * 0.5f) + 1, std::floor(at.y - w * 0.5f), 1, std::floor(w), Color(0.4f, 0.9f, 1.0f, 0.5f * (1 - q)));
    }
  }
  // ---- the glow round the head
  glow(P, W, s.x, s.y, 9.0f * S, L.c(2, 0.42f));
  glow(P, W, s.x, s.y, 4.5f * S, L.c(3, 0.55f), true);
  // ---- the head
  if (lance) {   // a crystal lance: a long diamond, its upper-left facet lit, its lower facet in shade, a white ridge
    const float Ln = 13.0f * S, Wd = 2.6f * S;
    for (float a = -Ln * 0.35f; a <= Ln * 0.65f; a += 0.7f) {
      const float tq = a < 0 ? 1 + a / (Ln * 0.35f) : 1 - a / (Ln * 0.65f);
      const float hw = std::max(0.5f, Wd * tq);
      for (float b = -hw; b <= hw; b += 0.9f) {
        const Vec2 at = s + dir * a + nr * b;
        const float side = dot(nr * b, Vec2(-0.7f, -0.7f));
        const Color c = std::fabs(b) < 0.6f ? L.c(4) : side > 0 ? L.c(3) : (std::fabs(b) > hw - 0.9f ? L.c(1) : L.c(2));
        P.rect(std::floor(at.x), std::floor(at.y), 1, 1, c);
      }
    }
    dot(P, s.x + dir.x * Ln * 0.65f, s.y + dir.y * Ln * 0.65f, Color(1, 1, 1), false);
  } else if (L.motion == Motion::Fork) {   // a crackling ball: a white core and short arcs re-struck every 40 ms
    const uint32_t st = hmix(seed, (uint32_t)(tt * 25));
    for (int a = 0; a < 4; a++) {
      const float an = hf(hmix(st, (uint32_t)a)) * kTau;
      const Vec2 e = s + Vec2(std::cos(an), std::sin(an)) * (5.0f + 4.0f * hf(hmix(st, 9u + (uint32_t)a))) * S;
      bolt(P, W, s, e, hmix(st, (uint32_t)a + 40u), L.c(2), L.c(1), L.c(4), 0.9f, 0, 0.5f);
    }
    const int cs = S >= 1.6f ? 1 : 0;
    const int sz = CORE_S[cs];
    P.blit(rs, ((int)(tt * 20) % CORE_FR) * 15, CORE_Y[cs], sz, sz, s.x - sz * 0.5f, s.y - sz * 0.5f, (float)sz, (float)sz, Color(1, 1, 1, 1), 0, false);
  } else {   // a core from the ramp's sheet (boiling, smooth), with flame tongues streaming back for fire
    const int cs = S >= 1.25f ? 1 : 0;
    const int sz = CORE_S[cs];
    const float sc = S >= 2.2f ? 2.0f : 1.0f;
    if (L.motion == Motion::Boil)
      for (int fl = 0; fl < 3; fl++) {
        const Vec2 at = s - dir * (3.0f + 2.0f * (float)fl) * S + nr * ((float)(fl - 1) * 2.0f * S);
        flame(P, rs, cs + (sc > 1 ? 1 : 0), (int)(tt * 14) + fl * 3, at.x, at.y + 3, 0.85f);
      }
    P.blit(rs, ((int)(tt * 14) % CORE_FR) * 15, CORE_Y[cs], sz, sz, s.x - sz * sc * 0.5f, s.y - sz * sc * 0.5f, sz * sc, sz * sc, Color(1, 1, 1, 1), 0, false);
    if (L.motion == Motion::Tear) {   // a void core with a bright rim
      P.rect(s.x - 1, s.y - 1, 3, 3, Color(0.02f, 0.0f, 0.04f, 0.9f));
    }
    if (L.form == Form::Mark || L.part == MPart::Glyph) {   // a spinning rune riding the core
      const int ri = (int)(tt * 8) & 7;
      P.blit(W.runes(L.glyph), ri * RUNE, 0, RUNE, RUNE, s.x - 4, s.y - 4, RUNE, RUNE, L.c(4, 0.9f), 1, false);
    }
    if (L.form == Form::Orb)   // motes orbiting a slow orb
      for (int m = 0; m < 4; m++) {
        const float a = tt * 5 + (float)m * kTau / 4;
        dot(P, s.x + std::cos(a) * 7 * S, s.y + std::sin(a) * 4 * S, L.c(4), true);
      }
  }
  (void)age;
}

// ------------------------------------------------------------------ drawing: the phases after the lighting
template <class Pt> void drawWindUp(Pt& P, World& W, const Ctx& C, const Fx& f) {
  const Look& L = f.L;
  const Vec2 h = scr(C, f.p);
  const float in = sat(f.t / 0.15f) * sat((f.life - f.t) / 0.1f);
  const float c = sat(f.t / 0.7f);
  const float S = baseScale(L) * std::max(0.6f, f.mag) * (1 + 0.2f * f.charge);
  const float pulse = 0.78f + 0.22f * std::sin(C.t * 15);
  const bool dual = f.mag >= 1.5f;
  for (int hand = 0; hand < (dual ? 2 : 1); hand++) {   // the hands' glow: a pulsing bloom and a white-hot core
    const float hx = h.x + (dual ? (hand ? 4.0f : -4.0f) : 0.0f);
    glow(P, W, hx, h.y, (5 + 4 * c) * S * pulse, L.c(3, 0.75f * in));
    glow(P, W, hx, h.y, (2.5f + 2 * c) * S, L.c(4, 0.8f * in), true);
    P.rect(hx - 1, h.y - 1, 3, 3, L.c(4, in));
  }
  // the runes gathering: orbiting the hands, drawing in as the charge builds
  const int n = 3 + std::min(4, (int)f.charge + (int)(c * 2));
  Sheet& rn = W.runes(L.glyph);
  const int rs = S >= 1.9f ? 2 : 1;
  for (int i = 0; i < n; i++) {
    const float a = C.t * (2.0f + 0.6f * f.charge) + (float)i * kTau / (float)n;
    const float r = (19.0f - 9.0f * c) * S;
    const float x = h.x + std::cos(a) * r, y = h.y - 2 + std::sin(a) * r * 0.55f;
    const float fa = in * sat(f.t * 4 - (float)i * 0.25f);
    P.blit(rn, (i & 7) * RUNE, 0, RUNE, RUNE, x - RUNE * rs * 0.5f + 1, y - RUNE * rs * 0.5f + 1, (float)(RUNE * rs), (float)(RUNE * rs), L.c(0, 0.6f * fa), 0, false);
    P.blit(rn, (i & 7) * RUNE, 0, RUNE, RUNE, x - RUNE * rs * 0.5f, y - RUNE * rs * 0.5f, (float)(RUNE * rs), (float)(RUNE * rs), L.c(3, 0.95f * fa), 1, false);
  }
  // the school's flavour while it gathers
  switch (L.motion) {
    case Motion::Boil: { Sheet& rsh = W.rampSheet(L); for (int k = 0; k < 2; k++) flame(P, rsh, 0, (int)(C.t * 16) + k * 4, h.x + (k ? 3.0f : -3.0f), h.y + 1, 0.9f * in); break; }
    case Motion::Fork: {
      const uint32_t st = hmix(f.seed, (uint32_t)(C.t * 22));
      const float a = hf(st) * kTau, r = (10 + 6 * c) * S;
      bolt(P, W, h, h + Vec2(std::cos(a) * r, std::sin(a) * r * 0.6f), st, L.c(2), L.c(1), L.c(4), 0.85f * in, 0, 0.5f);
      break;
    }
    case Motion::Rise: {   // a soft column of light from the feet to the hands and above
      const Vec2 ft = scr(C, f.q);
      const float w = 6 * S * in;
      beam(P, ft.x, h.y - 18 * S, ft.y, w * 1.3f, L.c(3, 0.26f * in), true);
      beam(P, ft.x, h.y - 12 * S, ft.y, w * 0.5f, L.c(4, 0.3f * in), true);
      break;
    }
    case Motion::Tear: {   // a slit of the void flickering at the hands
      const float hh = (5 + 6 * c) * S;
      P.rect(h.x - 1, std::floor(h.y - hh * 0.5f), 2, std::floor(hh), Color(0.02f, 0, 0.05f, 0.9f * in));
      P.add(h.x - 2, std::floor(h.y - hh * 0.5f), 1, std::floor(hh), L.c(3, 0.8f * in));
      P.add(h.x + 1, std::floor(h.y - hh * 0.5f), 1, std::floor(hh), L.c(3, 0.6f * in));
      break;
    }
    case Motion::Shimmer:
      ring(P, h.x - 1, h.y, (6 + 3 * std::sin(C.t * 9)) * S, (4 + 2 * std::sin(C.t * 9)) * S, Color(1.0f, 0.45f, 0.85f, 0.6f * in), true, 1, 0.08f, C.t * 10);
      ring(P, h.x + 1, h.y, (6 + 3 * std::sin(C.t * 9)) * S, (4 + 2 * std::sin(C.t * 9)) * S, Color(0.45f, 0.9f, 1.0f, 0.6f * in), true, 1, 0.08f, C.t * 10 + 1);
      break;
    case Motion::Pulse:
      for (int k = 0; k < 6; k++) {
        const float a = (float)k * kTau / 6 + C.t;
        icon(P, W, I_HEX, h.x + std::cos(a) * 9 * S - 3, h.y + std::sin(a) * 6 * S - 3, L.c(3, (0.4f + 0.4f * std::sin(C.t * 8 + (float)k)) * in), 1);
      }
      break;
    default: break;
  }
  // a charge step: a ring snapping out, brighter each level
  if (f.charge > 0) {
    const float since = f.t - f.v.x;
    if (since >= 0 && since < 0.25f) {
      const float q = since / 0.25f;
      ring(P, h.x, h.y, (4 + 26 * q) * S, (4 + 26 * q) * S * 0.6f, L.c(4, (1 - q)), true, 2);
      glow(P, W, h.x, h.y, 14 * S * (1 - q), L.c(3, 0.6f * (1 - q)));
    }
    for (int k = 0; k < (int)f.charge && k < 3; k++)   // the level's pips over the hands
      P.add(h.x - 4 + (float)k * 4, h.y - 10 * S - 4, 2, 2, L.c(4, in));
  }
}

template <class Pt> void drawLaunch(Pt& P, World& W, const Ctx& C, const Fx& f) {
  const Look& L = f.L;
  const Vec2 s = scr(C, f.p);
  const float u = sat(f.t / f.life), k = 1 - u;
  const float S = baseScale(L) * std::max(0.6f, f.mag);
  const Vec2 dir = len2(f.q - f.p) > 1 ? norm(f.q - f.p) : Vec2(0, -1);
  if (u < 0.4f) {   // the flash: a bloom and a four-point star
    const float a = 1 - u / 0.4f;
    glow(P, W, s.x, s.y, 15 * S * (0.6f + 0.4f * a), L.c(3, 0.85f * a));
    const float r = 11 * S * a + 2, d = 5 * S * a + 1;
    P.add(std::floor(s.x - r), s.y, std::floor(r * 2 + 1), 1, L.c(4, a));
    P.add(s.x, std::floor(s.y - r), 1, std::floor(r * 2 + 1), L.c(4, a));
    for (int i = 1; i < (int)d; i++) {
      dot(P, s.x + (float)i, s.y + (float)i, L.c(3, a * 0.8f), true); dot(P, s.x - (float)i, s.y - (float)i, L.c(3, a * 0.8f), true);
      dot(P, s.x + (float)i, s.y - (float)i, L.c(3, a * 0.8f), true); dot(P, s.x - (float)i, s.y + (float)i, L.c(3, a * 0.8f), true);
    }
    P.rect(s.x - 1, s.y - 1, 3, 3, Color(1, 1, 1, a));
  }
  // the muzzle streaks along the aim, and a ring kicking out
  for (int i = 0; i < 3; i++) {
    const Vec2 nr(-dir.y, dir.x);
    const Vec2 a0 = s + dir * 3.0f + nr * ((float)(i - 1) * 2.0f);
    const Vec2 a1 = a0 + dir * (14.0f * S * k) + nr * ((float)(i - 1) * 3.0f * u);
    line(P, (int)a0.x, (int)a0.y, (int)a1.x, (int)a1.y, i == 1 ? L.c(4, k) : L.c(3, 0.7f * k), true);
  }
  ring(P, s.x, s.y, (3 + 14 * u) * S, (3 + 14 * u) * S * 0.6f, L.c(2, 0.8f * k), true);
  if (L.form == Form::Mend || L.form == Form::Aura || L.form == Form::Ward) {   // a self spell: light rising round the body
    const float w = 14 * S;
    P.add(std::floor(s.x - w * 0.5f), std::floor(s.y - 18 * S * u - 6), std::floor(w), std::floor(26 * S * (0.4f + 0.6f * u)), L.c(3, 0.22f * k));
  }
}

template <class Pt> void drawImpactFx(Pt& P, World& W, const Ctx& C, const Fx& f) {
  const Look& L = f.L;
  const Vec2 c = scr(C, f.p);
  const float S = baseScale(L) * std::max(0.6f, f.mag);
  if (!onScr(C, c, 90 * S)) return;
  const float u = sat(f.t / f.life), k = 1 - u;
  const bool big = spectacle(L);
  // the impact frame: a solid white-hot disc for the first frames (the view's own hit-stop punch), a big bloom
  if (f.t < 0.07f) {
    const float r = 5.0f * S;
    for (int j = -(int)r; j <= (int)r; j++) {
      const float w = std::sqrt(std::max(0.0f, r * r - (float)(j * j)));
      P.rect(std::floor(c.x - w), c.y + (float)j, std::floor(w * 2 + 1), 1, L.c(4));
    }
  }
  glow(P, W, c.x, c.y, 24.0f * S * (0.5f + 0.5f * k), L.c(2, 0.42f * k * k));
  glow(P, W, c.x, c.y, 10.0f * S * (0.5f + 0.5f * k), L.c(3, 0.6f * k * k));
  switch (L.motion) {
    case Motion::Boil: {   // a rising, boiling bloom cooling into smoke (the ramp's burst), with sister bursts when big
      Sheet& rs = W.rampSheet(L);
      auto burst = [&](float x, float y, float uu, float sc, float a) {
        if (uu < 0 || uu >= 1) return;
        const int fr = std::min(BURST_FR - 1, (int)(uu * BURST_FR * 1.08f));
        const float d = BURST * sc;
        P.blit(rs, fr * BURST, BURST_Y, BURST, BURST, std::floor(x - d * 0.5f), std::floor(y - d * 0.62f), d, d, Color(1, 1, 1, a), 0, false);
        P.blit(rs, fr * BURST, BURST_Y, BURST, BURST, std::floor(x - d * 0.5f), std::floor(y - d * 0.62f), d, d, Color(1, 1, 1, 0.35f * (1 - uu)), 1, false);
      };
      // (M9 fixer, review: "the meteor's fireball is square 2x blobs with a flat red cap") a big blast is a cluster of
      // 1x bursts (the world's pixel size) staggered in time, rising and spreading, never one sprite blown up 2-3x
      const int n = S >= 2.6f ? 8 : S >= 1.5f ? 5 : 1;
      const float spread = (float)BURST * 0.3f * std::min(2.2f, std::sqrt(S));
      for (int i = n - 1; i >= 1; i--) {
        const uint32_t hh = hmix(f.seed, 300u + (uint32_t)i);
        const float a = kTau * ((float)i / (float)(n - 1)) + hf(hh) * 0.6f;
        const float r = spread * (0.55f + 0.45f * hf(hh >> 7));
        burst(c.x + std::cos(a) * r, c.y + std::sin(a) * r * 0.55f - 4.0f * (float)(i & 1), (f.t - 0.03f * (float)i) / f.life, 1.0f, 0.95f);
      }
      if (n > 1) burst(c.x, c.y - spread * 0.45f, (f.t - 0.05f) / f.life, 1.0f, 0.95f);   // the plume climbing out of it
      burst(c.x, c.y, u, 1.0f, 1.0f);
      if (S > 1.2f || big) {
        burst(c.x - 11 * S, c.y + 2, (f.t - 0.07f) / f.life, 1.0f, 0.95f);
        burst(c.x + 12 * S, c.y + 3, (f.t - 0.12f) / f.life, 1.0f, 0.95f);
      }
      break;
    }
    case Motion::Shatter: {   // crystal spikes erupt from the ground in a ring, then burst into shards
      const int n = 6 + (int)(4 * S);
      const float grow = sat(f.t / 0.07f) * (u < 0.6f ? 1.0f : sat((1 - u) / 0.4f));
      const Color lit = L.c(3), mid = L.c(2), dk = L.c(1), tip = L.c(4);
      struct Sp { float x, y, h, w; } sp[24];
      int m = 0;
      for (int i = 0; i < n && m < 21; i++) {
        const uint32_t hh = hmix(f.seed, (uint32_t)i);
        const float a = kTau * (float)i / (float)n + hf(hh) * 0.5f;
        const float r = (8.0f + 6.0f * hf(hh >> 5)) * S;
        sp[m++] = Sp{c.x + std::cos(a) * r, c.y + 7 + std::sin(a) * r * 0.45f, (7.0f + 9.0f * hf(hh >> 11)) * S * grow, 3.0f + 2.0f * hf(hh >> 17) * S};
      }
      sp[m++] = Sp{c.x - 2, c.y + 6, 16 * S * grow, 5 * S};   // the heart of the cluster
      sp[m++] = Sp{c.x + 3, c.y + 8, 12 * S * grow, 4 * S};
      std::sort(sp, sp + m, [](const Sp& a, const Sp& b) { return a.y < b.y; });
      for (int i = 0; i < m; i++) spike(P, sp[i].x, sp[i].y, sp[i].h, sp[i].w, lit, mid, dk, tip, 0.35f * grow);
      for (int i = 0; i < m; i += 2) dot(P, sp[i].x - 1, sp[i].y - sp[i].h * 0.7f, Color(1, 1, 1, grow * (0.5f + 0.5f * std::sin(C.t * 20 + (float)i))), true);
      break;
    }
    case Motion::Fork: {   // radial arcs re-struck every 45 ms, a white node at the heart
      const int nf = 4 + (int)(2 * S);
      const uint32_t st = hmix(f.seed, (uint32_t)(f.t / 0.045f));
      const float al = (u < 0.15f ? 1.0f : k) * (((int)(f.t * 40) & 1) ? 1.0f : 0.72f);
      for (int i = 0; i < nf; i++) {
        const float a = kTau * (float)i / (float)nf + hf(hmix(st, (uint32_t)i)) * 0.8f;
        const float l = (14.0f + 18.0f * hf(hmix(st, 100u + (uint32_t)i))) * S;
        bolt(P, W, c, c + Vec2(std::cos(a) * l, std::sin(a) * l * 0.6f), hmix(st, (uint32_t)i), L.c(2), L.c(1), L.c(4), al, 1, 0.8f);
      }
      glow(P, W, c.x, c.y, 8 * S, L.c(4, 0.8f * al), true);
      break;
    }
    case Motion::Rise: {   // a pillar of light, twinkling crosses
      const float w = 11.0f * S * sat(f.t / 0.1f) * (1 - u * 0.6f), Hh = 64.0f * S;
      {   // (painted then added: warm on any ground) soft-edged, fading toward the sky
        const float top = c.y + 8 - Hh, foot = c.y + 8;
        beam(P, c.x, top, foot, w * 1.4f, L.c(3, 0.3f * k), false);
        beam(P, c.x, top, foot, w * 0.8f, L.c(4, 0.45f * k), false);
        beam(P, c.x, top, foot, std::max(2.0f, w * 0.35f), L.c(4, 0.8f * k), true);
        gglow(P, W, c.x, foot - 1, w * 1.6f, L.c(4, 0.5f * k));   // where it meets the ground
      }
      for (int i = 0; i < 6; i++) {
        const uint32_t hh = hmix(f.seed, (uint32_t)i);
        const float x = c.x + (hf(hh) - 0.5f) * 30 * S, y = c.y - hf(hh >> 8) * 40 * S - u * 14;
        const float tw = 0.5f + 0.5f * std::sin(C.t * 12 + (float)i * 2);
        icon(P, W, I_STARS, x - 2, y - 2, L.c(4, k * tw), 1);
      }
      break;
    }
    case Motion::Tear: {   // the rift: a jagged slit of the void with a burning rim, tendrils, a collapse flash
      const float open = std::sin(kPi * std::min(1.0f, u * 1.15f));
      const float Hh = 28.0f * S, Wd = 7.0f * S * open;
      for (int j = 0; j < (int)Hh; j++) {
        const float yy = (float)j / Hh * 2 - 1;
        const float jag = (hf(hmix(f.seed ^ (uint32_t)(C.t * 18), (uint32_t)j)) - 0.5f) * 2.0f;
        const float w = std::max(0.0f, Wd * (1 - yy * yy) + jag * open);
        const float y = c.y - Hh * 0.75f + (float)j;
        const float x = c.x + std::sin((float)j * 0.35f) * 1.5f;
        if (w < 0.5f) continue;
        P.rect(std::floor(x - w), y, std::floor(w * 2 + 1), 1, Color(0.02f, 0.0f, 0.05f, 0.95f));
        P.add(std::floor(x - w) - 1, y, 1, 1, L.c(3, open));
        P.add(std::floor(x + w) + 1, y, 1, 1, L.c(3, open));
        if (j % 3 == 0) P.add(std::floor(x - w) - 2, y, 1, 1, L.c(2, 0.6f * open));
      }
      for (int i = 0; i < 4; i++) {   // tendrils curling out
        const uint32_t hh = hmix(f.seed, (uint32_t)i + 50u);
        Vec2 p0(c.x, c.y - Hh * 0.75f + hf(hh) * Hh);
        float a = (i & 1) ? 0.0f : kPi;
        for (int s2 = 0; s2 < 6; s2++) {
          a += (hf(hh >> s2) - 0.5f) * 1.2f + std::sin(C.t * 6 + (float)i) * 0.3f;
          const Vec2 p1 = p0 + Vec2(std::cos(a), std::sin(a) * 0.7f) * (2.5f * S * open);
          line(P, (int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y, L.c(2, 0.7f * open * (1 - (float)s2 / 6)), true);
          p0 = p1;
        }
      }
      if (u > 0.82f) glow(P, W, c.x, c.y - Hh * 0.25f, 18 * S * (1 - u) * 5, L.c(4, 0.8f * (1 - u) * 5));
      break;
    }
    case Motion::Shimmer: {   // shimmering chromatic rings; a fear mask rising for the frightening
      for (int i = 0; i < 3; i++) {
        const float r = (8 + 28 * u * (1 + 0.35f * (float)i)) * S;
        const float a = k * (1 - 0.25f * (float)i);
        ring(P, c.x - 2, c.y, r, r * 0.55f, Color(1.0f, 0.42f, 0.85f, 0.8f * a), true, 2, 0.07f, C.t * 18 + (float)i);
        ring(P, c.x + 2, c.y, r, r * 0.55f, Color(0.42f, 0.9f, 1.0f, 0.8f * a), true, 2, 0.07f, C.t * 18 + (float)i + 0.6f);
        ring(P, c.x, c.y, r, r * 0.55f, L.c(2, 0.85f * a), false, 2, 0.07f, C.t * 18 + (float)i + 0.3f);
      }
      if ((L.statuses & magic::statusBit(Status::Frightened)) || L.ess == Essence::Dream) {
        // (M9 fixer, review: "a flat pink 2-3x mask") the fear mask at the world's own pixel size, rising out of a dark
        // halo with two wisps curling up beside it and its chromatic ghosts trailing a pixel apart
        const float y = std::floor(c.y - 20 - 18 * u), mx = std::floor(c.x - 4);
        glow(P, W, mx + 4, y + 4, 11, Color(0.22f, 0.04f, 0.3f, 0.5f * k), true);
        for (int sd = -1; sd <= 1; sd += 2)
          for (int j = 0; j < 9; j++) {
            const float t = (float)j / 8.0f;
            dot(P, std::floor(mx + 4 + (float)sd * (7 + 3 * std::sin(t * 5 + C.t * 6))), std::floor(y + 12 - t * 16), Color(0.62f, 0.4f, 0.95f, 0.75f * k * (1 - t)), true);
          }
        icon(P, W, I_MASK, mx + 1, y + 1, Color(0.08f, 0.0f, 0.12f, 0.6f * k), 0);
        icon(P, W, I_MASK, mx - 1, y, Color(1.0f, 0.4f, 0.8f, 0.35f * k), 1);   // its ghost, split
        icon(P, W, I_MASK, mx + 1, y, Color(0.4f, 0.9f, 1.0f, 0.35f * k), 1);
        icon(P, W, I_MASK, mx, y, L.c(3, 0.95f * k), 0);
        icon(P, W, I_MASK, mx, y, L.c(4, 0.25f * k), 1);
      }
      break;
    }
    case Motion::Pulse: {
      if (L.ess == Essence::Tide) {   // a splash crown and ripples
        for (int i = 0; i < 9; i++) {
          const float a = kTau * (float)i / 9.0f;
          const float r = (5 + 10 * u) * S, hgt = std::sin(kPi * std::min(1.0f, u * 1.4f)) * 14 * S * (0.7f + 0.3f * hf(hmix(f.seed, (uint32_t)i)));
          const float x = c.x + std::cos(a) * r, y = c.y + 7 + std::sin(a) * r * 0.42f;
          P.rect(std::floor(x), std::floor(y - hgt), 1, std::floor(std::max(1.0f, hgt * 0.6f)), L.c(3, 0.9f * k));
          dot(P, x, y - hgt - 1, L.c(4, k), false);
        }
        ring(P, c.x, c.y + 7, (8 + 26 * u) * S, (8 + 26 * u) * S * 0.42f, L.c(4, 0.7f * k), false);
        ring(P, c.x, c.y + 7, (4 + 16 * u) * S, (4 + 16 * u) * S * 0.42f, L.c(3, 0.6f * k), false);
      } else if (L.ess == Essence::Stone || L.ess == Essence::Rust || L.ess == Essence::Bone) {   // stone spikes and rubble
        const int n = 5 + (int)(3 * S);
        const float grow = sat(f.t / 0.08f) * (u < 0.7f ? 1.0f : sat((1 - u) / 0.3f));
        struct Sp { float x, y, h, w; } sp[20];
        int m = 0;
        for (int i = 0; i < n && m < 20; i++) {
          const uint32_t hh = hmix(f.seed, (uint32_t)i);
          const float a = kTau * (float)i / (float)n + hf(hh);
          const float r = (6.0f + 9.0f * hf(hh >> 4)) * S;
          sp[m++] = Sp{c.x + std::cos(a) * r, c.y + 7 + std::sin(a) * r * 0.42f, (8.0f + 9.0f * hf(hh >> 9)) * S * grow, 5.0f + 3.0f * hf(hh >> 15) * S};
        }
        std::sort(sp, sp + m, [](const Sp& a, const Sp& b) { return a.y < b.y; });
        const Color lit(0.76f, 0.64f, 0.48f), mid(0.52f, 0.41f, 0.29f), dk(0.28f, 0.21f, 0.15f), tip(0.9f, 0.82f, 0.66f);
        for (int i = 0; i < m; i++) spike(P, sp[i].x, sp[i].y, sp[i].h, sp[i].w, mulC(lit, C.lightK * 0.6f + 0.4f), mulC(mid, C.lightK * 0.6f + 0.4f), mulC(dk, C.lightK * 0.6f + 0.4f), tip, 0.4f * grow);
      } else {   // hex cells flaring on an expanding shell
        for (int i = 0; i < 10; i++) {
          const float a = kTau * (float)i / 10.0f;
          const float r = (8 + 22 * u) * S;
          icon(P, W, I_HEX, c.x + std::cos(a) * r - 3, c.y + std::sin(a) * r * 0.55f - 3, L.c(3, k), 1);
        }
      }
      ring(P, c.x, c.y, (6 + 20 * u) * S, (6 + 20 * u) * S * 0.6f, L.c(3, 0.7f * k), true, 1, 0.0f, 0, 2);
      break;
    }
    default: {   // Spiral / Drift / Fall: a turning rune ring and the essence's own sheddings (particles)
      const int n = 8;
      Sheet& rn = W.runes(L.glyph);
      for (int i = 0; i < n; i++) {
        const float a = (float)i * kTau / (float)n + C.t * (L.motion == Motion::Spiral ? 4.0f : 1.5f);
        const float r = (6 + 20 * u) * S;
        P.blit(rn, (i & 7) * RUNE, 0, RUNE, RUNE, c.x + std::cos(a) * r - 4, c.y + std::sin(a) * r * 0.55f - 4, RUNE, RUNE, L.c(3, k), 1, false);
      }
      break;
    }
  }
}

template <class Pt> void drawSky(Pt& P, World& W, const Ctx& C, const Fx& f) {
  const Look& L = f.L;
  const float S = baseScale(L) * std::max(0.6f, f.mag);
  const float u = sat(f.t / f.life), k = 1 - u;
  switch (f.kind) {
    case FX_METEOR: {
      const float e = sat(f.t / std::max(0.01f, f.life));
      if (f.hit) {   // the landing's afterglow: a white column for a moment
        const Vec2 s = scr(C, f.q);
        const float a = sat((f.life - f.t) / 0.25f);
        // (M9 fixer, review: "the meteor's shaft is a hard-edged flat band") a soft beam fading toward the sky
        beam(P, s.x, -8, s.y, 9.0f * S, L.c(3, 0.22f * a), true);
        beam(P, s.x, s.y * 0.35f, s.y, 3.5f * S, L.c(4, 0.3f * a), true);
        break;
      }
      const Vec2 from = f.v, to = f.q;
      const Vec2 head = from + (to - from) * (e * e);
      const Vec2 s = scr(C, head);
      const Vec2 dir = norm(to - from), nr(-dir.y, dir.x);
      Sheet& rs = W.rampSheet(L);
      for (int i = 40; i >= 1; i--) {   // the long trail of fire and smoke
        const float q = (float)i / 40.0f;
        const Vec2 at = s - dir * ((float)i * 2.6f * S) + nr * std::sin(C.t * 20 - (float)i * 0.7f) * 2.0f * q * S;
        const float w = std::max(1.0f, 9.0f * S * (1 - q * 0.8f));
        // (M9 integration) round puffs, not square blocks: the banded glow for the fire, the soft one darkened for smoke
        if (q > 0.6f) {
          const float d = std::floor(w * 1.6f + 0.5f);
          P.blit(W.common(), I_GLOWS.x, I_GLOWS.y, I_GLOWS.w, I_GLOWS.h, std::floor(at.x - d * 0.5f), std::floor(at.y - d * 0.5f), d, d, Color(0.16f, 0.13f, 0.13f, 0.55f * (1 - q)), 0, false);
        } else if (i % 2 == 0 || q < 0.2f) glow(P, W, at.x, at.y, w * 0.75f, q < 0.15f ? L.c(4, 0.9f) : q < 0.35f ? L.c(3, 0.8f) : L.c(2, 0.6f));
      }
      glow(P, W, s.x, s.y, 22 * S, L.c(3, 0.7f));
      for (int fl = 0; fl < 4; fl++) {
        const Vec2 at = s - dir * (4.0f + 3.0f * (float)fl) * S + nr * ((float)fl - 1.5f) * 2.5f * S;
        flame(P, rs, 2, (int)(C.t * 16) + fl * 2, at.x, at.y + 6, 0.9f);
      }
      // the head: a white-hot ball in banded rings (a sprite blown up 2-3x read as coarse blocks beside the 1x world)
      {   // its body first (painted, not added: it must read against a sunlit meadow), then the heat over it
        const float d = std::floor(30.0f * S + 0.5f);
        P.blit(W.common(), I_GLOWB.x, I_GLOWB.y, I_GLOWB.w, I_GLOWB.h, std::floor(s.x - d * 0.5f), std::floor(s.y - d * 0.5f), d, d, L.c(2, 1.0f), 0, false);
        const float e2 = std::floor(18.0f * S + 0.5f);
        P.blit(W.common(), I_GLOWB.x, I_GLOWB.y, I_GLOWB.w, I_GLOWB.h, std::floor(s.x - e2 * 0.5f), std::floor(s.y - e2 * 0.5f), e2, e2, L.c(3, 1.0f), 0, false);
      }
      glow(P, W, s.x, s.y, 16.0f * S, L.c(3, 0.8f));
      glow(P, W, s.x, s.y, 9.0f * S, L.c(4, 1.0f));
      glow(P, W, s.x, s.y, 4.5f * S, Color(1, 1, 0.9f, 1));
      const int sz = CORE_S[1];
      P.blit(rs, ((int)(C.t * 14) % CORE_FR) * 15, CORE_Y[1], sz, sz, std::floor(s.x - sz * 0.5f), std::floor(s.y - sz * 0.5f), (float)sz, (float)sz, Color(1, 1, 1, 0.9f), 0, false);
      break;
    }
    case FX_STRIKE: {   // a bolt from the sky: the whole height of the screen above the mark
      const Vec2 s = scr(C, f.p);
      const float al = (u < 0.2f ? 1.0f : k) * (((int)(f.t * 40) & 1) ? 1.0f : 0.75f);
      const uint32_t st = hmix(f.seed, (uint32_t)(f.t / 0.05f));
      bolt(P, W, Vec2(s.x + (hf(f.seed) - 0.5f) * 40, std::min(-10.0f, s.y - 230)), s, st, L.c(2), L.c(1), L.c(4), al, 3, 1.4f);
      glow(P, W, s.x, s.y, 16 * S, L.c(4, 0.7f * al));
      gglow(P, W, s.x, s.y + 4, 22 * S, L.c(3, 0.5f * al));
      break;
    }
    case FX_STORMCELL: {   // the sky darkens; a cloud mass over the mark lit from within; rain
      const Vec2 s = scr(C, f.p);
      const float a = sat(f.t / 0.35f) * sat((f.life - f.t) / 0.5f);
      P.rect(0, 0, C.W, C.H, Color(0.04f, 0.04f, 0.12f, 0.22f * a));
      const float lit = (f.t - 0.32f * (float)f.sub) < 0.08f ? 1.0f : 0.0f;
      // (M9 fixer, review: "the storm's clouds are blurry 2-3x blobs") a towering mass of 1x cloud puffs in rows:
      // dark, heavy underside, lighter crowns (lit from the top-left), a flicker inside where the next bolt brews
      const float sp = std::sqrt(S);
      for (int row = 0; row < 4; row++) {
        const int n = 9 - row;
        for (int i = 0; i < n; i++) {
          const uint32_t h = hmix(f.seed, (uint32_t)(row * 16 + i));
          const float wid = (126.0f - 16.0f * (float)row) * sp;
          const float x = s.x + ((float)i + 0.5f - (float)n * 0.5f) / (float)n * wid + (hf(h) - 0.5f) * 12 + std::sin(C.t * 0.8f + (float)(i + row)) * 2;
          const float y = s.y - 106 - 9.0f * (float)row + (hf(h >> 8) - 0.5f) * 6;
          const float tone = (0.16f + 0.07f * (float)row) * C.lightK + 0.06f;   // the crown rows lighter
          icon(P, W, iCloud((int)(h >> 3)), std::floor(x - 12), std::floor(y - 8), Color(tone, tone, tone * 1.25f + 0.03f, 0.9f * a), 0);
          if (row >= 2) icon(P, W, iCloud((int)(h >> 5)), std::floor(x - 13), std::floor(y - 9), Color(0.5f, 0.52f, 0.6f, 0.16f * a * C.lightK), 1);   // a top-left rim
          if (lit > 0 && row <= 1) icon(P, W, iCloud((int)(h >> 3)), std::floor(x - 12), std::floor(y - 8), L.c(2, 0.5f * a), 1);
        }
      }
      for (int i = 0; i < 46; i++) {   // rain, slanting
        const uint32_t h = hmix(f.seed ^ 0xA1u, (uint32_t)i);
        const float ph = std::fmod(C.t * (1.6f + hf(h)) + hf(h >> 6), 1.0f);
        const float x = s.x + (hf(h >> 12) - 0.5f) * 150 * std::sqrt(S) - ph * 12, y = s.y - 110 + ph * 130;
        line(P, (int)x, (int)y, (int)x - 2, (int)y + 5, Color(0.6f, 0.7f, 0.9f, 0.45f * a), false);
      }
      break;
    }
    case FX_STARS: {   // a constellation drawn star by star, its lines between them (brighter by night), and a falling star
      const Vec2 s = scr(C, f.p);
      const float night = 0.45f + 0.55f * C.dark;
      const int n = 7;
      Vec2 pt[7];
      for (int i = 0; i < n; i++) {
        const uint32_t h = hmix(f.seed, (uint32_t)i);
        pt[i] = Vec2(std::floor(s.x - 60 + (float)i * 20 + (hf(h) - 0.5f) * 14), std::floor(std::max(10.0f, s.y - 120) + (hf(h >> 8) - 0.5f) * 40));
      }
      const float draw = sat(f.t / (f.life * 0.55f)) * (float)(n - 1);
      const float fade = sat((f.life - f.t) / 0.4f);
      for (int i = 0; i < n - 1; i++) {
        const float part = sat(draw - (float)i);
        if (part <= 0) break;
        const Vec2 a = pt[i], b = a + (pt[i + 1] - a) * part;
        line(P, (int)a.x, (int)a.y, (int)b.x, (int)b.y, L.c(2, 0.55f * night * fade), true);
        line(P, (int)a.x, (int)a.y + 1, (int)b.x, (int)b.y + 1, L.c(1, 0.3f * night * fade), true);
      }
      for (int i = 0; i < n; i++) {
        if ((float)i > draw + 0.5f) break;
        const float tw = 0.7f + 0.3f * std::sin(C.t * 9 + (float)i * 1.7f);
        glow(P, W, pt[i].x, pt[i].y, 6, L.c(3, 0.5f * night * fade * tw));
        icon(P, W, i % 3 == 0 ? I_STAR : I_STARS, pt[i].x - (i % 3 == 0 ? 4.0f : 2.0f), pt[i].y - (i % 3 == 0 ? 4.0f : 2.0f), L.c(4, night * fade * tw), 1);
      }
      const float fs = (f.t - f.life * 0.55f) / (f.life * 0.3f);   // the falling star from the last star to the mark
      if (fs > 0 && fs < 1) {
        const Vec2 a = pt[n - 1], b = s;
        const Vec2 h = a + (b - a) * (fs * fs);
        const Vec2 dd = norm(a - b);
        for (int i = 0; i < 24; i++) P.add(std::floor(h.x + dd.x * (float)i * 1.6f), std::floor(h.y + dd.y * (float)i * 1.6f), i < 6 ? 2.0f : 1.0f, 1, L.c(i < 8 ? 4 : 3, 0.9f * (1 - (float)i / 24)));
        glow(P, W, h.x, h.y, 7, L.c(4, 0.9f));
      }
      (void)S;
      break;
    }
    default: break;
  }
}

template <class Pt> void drawPhaseFx(Pt& P, World& W, const Ctx& C, const Fx& f) {
  const Look& L = f.L;
  const float S = baseScale(L) * std::max(0.6f, f.mag);
  const float u = sat(f.t / f.life), k = 1 - u;
  switch (f.kind) {
    case (uint8_t)Phase::WindUp: drawWindUp(P, W, C, f); break;
    case (uint8_t)Phase::Launch: drawLaunch(P, W, C, f); break;
    case (uint8_t)Phase::Impact: drawImpactFx(P, W, C, f); break;
    case (uint8_t)Phase::Channel: {   // a beam: a wavering band with a white core, nodes racing along it, a splash at its end
      const Vec2 a = scr(C, f.p), b = scr(C, f.q);
      const Vec2 d = b - a;
      const float l = std::max(1.0f, len(d));
      const Vec2 dir = d * (1.0f / l), nr(-dir.y, dir.x);
      for (float s = 0; s < l; s += 2) {
        const float wob = std::sin(s * 0.3f - C.t * 30) * 1.4f * S;
        const Vec2 p = a + dir * s + nr * wob;
        const float w = 4.0f * S;
        P.add(std::floor(p.x - w * 0.5f), std::floor(p.y - w * 0.5f), std::floor(w), std::floor(w), L.c(2, 0.3f));
        P.add(std::floor(p.x), std::floor(p.y), 2, 2, L.c(4, 0.9f));
      }
      for (int nd = 0; nd < (int)(l / 16) + 1; nd++) {
        const float s = std::fmod(C.t * 160 + (float)nd * 16, l);
        const Vec2 p = a + dir * s;
        glow(P, W, p.x, p.y, 3.5f * S, L.c(3, 0.7f));
      }
      glow(P, W, b.x, b.y, 9 * S, L.c(3, 0.8f));
      glow(P, W, a.x, a.y, 6 * S, L.c(4, 0.7f));
      break;
    }
    case (uint8_t)Phase::Release: {
      const Vec2 s = scr(C, f.p + Vec2(0, -8));
      ring(P, s.x, s.y, (4 + 20 * u) * S, (4 + 20 * u) * S * 0.6f, L.c(3, 0.8f * k), true, 1, 0.1f, C.t * 10, 2);
      break;
    }
    case (uint8_t)Phase::Combo: {
      const Vec2 s = scr(C, f.p);
      const Surface sf = (Surface)f.sub;
      const Color c1 = cu(surfCol(sf, 1)), c2 = cu(surfCol(sf, 2));
      if (sf == Surface::Shock) {
        const uint32_t st = hmix(f.seed, (uint32_t)(f.t / 0.05f));
        for (int i = 0; i < 5; i++) {
          const uint32_t h = hmix(st, (uint32_t)i);
          const Vec2 a = s + Vec2((hf(h) - 0.5f) * 40, (hf(h >> 8) - 0.5f) * 18), b = s + Vec2((hf(h >> 16) - 0.5f) * 40, (hf(h >> 4) - 0.5f) * 18);
          bolt(P, W, a, b, h, c1, Color(0.3f, 0.3f, 0.9f), c2, k, 0, 0.6f);
        }
      } else if (sf == Surface::Fire) {
        Look FL = fireLook();
        Sheet& rs = W.rampSheet(FL);
        const int fr = std::min(BURST_FR - 1, (int)(u * BURST_FR));
        P.blit(rs, fr * BURST, BURST_Y, BURST, BURST, s.x - 48, s.y - 60, 96, 96, Color(1, 1, 1, 1), 0, false);
      }
      glow(P, W, s.x, s.y, 22 * k + 4, withA(c2, 0.6f * k));
      ring(P, s.x, s.y + 4, 6 + 30 * u, (6 + 30 * u) * 0.42f, withA(c2, 0.9f * k), true, 2);
      break;
    }
    case (uint8_t)Phase::StatusOn: {
      const Vec2 s = scr(C, f.p + Vec2(0, -12));
      ring(P, s.x, s.y, 4 + 14 * u, (4 + 14 * u) * 0.7f, L.c(3, k), true);
      glow(P, W, s.x, s.y, 10 * k + 2, L.c(3, 0.5f * k));
      Ico ic = I_STAR;
      switch ((Status)f.sub) {
        case Status::Charmed: ic = I_HEART; break;
        case Status::Frightened: ic = I_MASK; break;
        case Status::Blinded: ic = I_EYE; break;
        case Status::Poisoned: ic = I_DIAM; break;
        default: break;
      }
      icon(P, W, ic, s.x - (float)ic.w * 0.5f, s.y - 14 - 6 * u, L.c(4, k), 1);
      break;
    }
    case (uint8_t)Phase::Summon: break;   // (drawn by the host: the creature climbing out needs its sheet; the rift is ground)
    case (uint8_t)Phase::Ward: {   // a shell of hex cells round the body, a light sweeping over it, a bright rim
      const Vec2 s = scr(C, f.p + Vec2(0, -11));
      const float grow = sat(f.t / 0.16f), fade = sat((f.life - f.t) / 0.35f);
      const float rx = 12 * S * grow, ry = 15 * S * grow;
      for (int gy = -(int)(ry / 5); gy <= (int)(ry / 5); gy++)
        for (int gx = -(int)(rx / 6) - 1; gx <= (int)(rx / 6) + 1; gx++) {
          const float x = (float)gx * 6 + ((gy & 1) ? 3.0f : 0.0f), y = (float)gy * 5;
          const float d = (x * x) / (rx * rx + 0.01f) + (y * y) / (ry * ry + 0.01f);
          if (d > 1.0f) continue;
          const float wave = 0.5f + 0.5f * std::sin(std::sqrt(x * x + y * y) * 0.45f - f.t * 14);
          icon(P, W, I_HEX, s.x + x - 3, s.y + y - 3, L.c(3, (0.15f + 0.45f * wave * (0.4f + 0.6f * d)) * fade), 1);
        }
      ring(P, s.x, s.y, rx, ry, L.c(4, 0.9f * fade), true, 2);
      glow(P, W, s.x - rx * 0.4f, s.y - ry * 0.45f, 4, Color(1, 1, 1, 0.6f * fade));   // the lit top-left glint
      break;
    }
    case (uint8_t)Phase::Blink: {
      const Vec2 a = scr(C, f.p + Vec2(0, -10)), b = scr(C, f.q + Vec2(0, -10));
      if (u < 0.5f) {
        const float q = u / 0.5f;
        ring(P, a.x, a.y, 14 * (1 - q) + 1, 18 * (1 - q) + 1, L.c(3, 1 - q), true);
        P.add(a.x, std::floor(a.y - 18 * (1 - q)), 1, std::floor(36 * (1 - q)) + 1, L.c(4, 1 - q));
      }
      ring(P, b.x, b.y, 2 + 16 * u, 3 + 20 * u, L.c(3, k), true);
      for (int i = 0; i < 10; i++) {
        const Vec2 p = a + (b - a) * ((float)i / 10.0f);
        dot(P, p.x, p.y + std::sin((float)i + C.t * 20) * 2, L.c(3, 0.7f * k), true);
      }
      break;
    }
    case (uint8_t)Phase::Reaction: {
      const Vec2 s = scr(C, f.p + Vec2(0, -10));
      const Vec2 dir = len2(f.q - f.p) > 1 ? norm(f.q - f.p) : Vec2(1, 0);
      const Vec2 at = s + dir * 10.0f;
      P.blit(W.runes(L.glyph), ((int)(f.seed & 7)) * RUNE, 0, RUNE, RUNE, at.x - RUNE, at.y - RUNE, RUNE * 2, RUNE * 2, L.c(4, k), 1, false);
      ring(P, at.x, at.y, 6 + 12 * u, 9 + 16 * u, L.c(3, k), true, 2);
      glow(P, W, at.x, at.y, 12 * k, L.c(3, 0.7f * k));
      break;
    }
    default: drawSky(P, W, C, f); break;
  }
  (void)S;
}


// the summoned form climbing out of its rift (Phase::Summon): the creature's sheet rising through the rift line in the
// school's light, a column of light over the rift, then it fades into the real summon (cellH: the monster's cell height)
template <class Pt> void drawSummonForm(Pt& P, World& W, const Ctx& C, const Fx& f, int monster, int cellH, int animFrame) {
  const Look& L = f.L;
  const float u = sat(f.t / f.life);
  const float S2 = baseScale(L) * std::max(0.6f, f.mag);
  // (M9 fixer, review: "the great summon is a scaled-up flat silhouette") always at the world's own pixel size, in
  // its own shading lit by the school's colour (a 2x flat-tinted sheet broke the pixel density); the spectacle is the
  // column, the rift and the light, not a blown-up sprite
  const float sc = 1.0f;
  const float rise = sat((u - 0.12f) / 0.45f);
  const int rows = (int)std::floor((float)cellH * rise);
  const Vec2 s = scr(C, f.p);
  const float out = rise >= 1 ? sat((1 - u) / 0.35f) : 1.0f;
  const float open = sat(f.t / 0.25f) * sat((f.life - f.t) / 0.35f);
  const float cw = 12.0f * S2 * open;
  beam(P, s.x, s.y - 46 * S2, s.y, cw * 1.4f, L.c(2, 0.24f * open), true);
  beam(P, s.x, s.y - 40 * S2, s.y, cw * 0.5f, L.c(3, 0.32f * open), true);
  const bool flip = f.q.x > f.p.x;
  const int fr = rise < 1 ? 4 : animFrame;
  const float feetY = s.y + 2 + (float)(cellH - rows) * sc;
  const Color tint = lerpC(Color(1, 1, 1), L.c(3), 0.4f - 0.15f * rise);
  const float lk = 0.2f + 0.8f * C.lightK;
  P.creature(monster, fr, s.x, feetY, rows, sc, Color(tint.r * lk, tint.g * lk, tint.b * lk, out), 0, flip);
  P.creature(monster, fr, s.x, feetY, rows, sc, L.c(3, (0.5f * (1 - rise) + 0.18f) * out), 1, flip);   // the school's light on it
  if (S2 >= 1.7f && rows > 0) {   // a great summoning: a lit rim on the rift line where it climbs through, and motes
    const float rw = 9.0f + 0.25f * (float)cellH;
    for (int i = 0; i < 9; i++) {
      const float ph = std::fmod(C.t * 0.9f + (float)i * 0.37f, 1.0f);
      dot(P, std::floor(s.x + (hf(hmix(f.seed, (uint32_t)i)) - 0.5f) * rw * 2), std::floor(feetY - ph * (float)rows * 1.1f), L.c(4, 0.8f * (1 - ph) * out), true);
    }
  }
  if (rows > 0 && rise < 1) glow(P, W, s.x, s.y, 10 * S2, L.c(3, 0.6f));
}

// ------------------------------------------------------------------ drawing: particles
template <class Pt> void drawParts(Pt& P, World& W, const Ctx& C) {
  Sheet& cs = W.common();
  for (const Part& q : W.parts) {
    const Vec2 s0 = scr(C, q.p);
    const Vec2 s(s0.x, s0.y - std::floor(q.z));
    if (!onScr(C, s, 20)) continue;
    const float r = q.life / std::max(0.01f, q.max);
    float a = std::min(1.0f, r * 2.2f);
    if (q.flags & PF_FLICKER) a *= 0.65f + 0.35f * std::sin(C.t * 37 + q.spin * 5);
    const bool add = (q.flags & PF_ADD) != 0;
    const float dim = (q.flags & PF_DIM) ? C.lightK : 1.0f;
    Color c0 = cu(q.c0), c1 = cu(q.c1), c2 = cu(q.c2);
    if (dim < 1) { c0 = mulC(c0, dim); c1 = mulC(c1, dim); c2 = mulC(c2, std::min(1.0f, dim * 1.2f)); }
    if ((q.flags & PF_Z) && q.z > 1.5f) P.rect(s0.x, s0.y, 2, 1, Color(0, 0, 0, 0.3f * a));
    auto put = [&](float x, float y, float w, float h, Color c) { if (add) P.add(x, y, w, h, c); else P.rect(x, y, w, h, c); };
    switch (q.kind) {
      case PK_EMBER: {
        const Color c = r > 0.6f ? c2 : r > 0.3f ? c1 : c0;
        const Vec2 tail = norm(q.v) * -1.0f;
        if (q.size > 1.5f) { put(s.x, s.y, 2, 2, withA(c, a)); put(s.x, s.y, 1, 1, withA(c2, a)); }
        else put(s.x, s.y, 1, 1, withA(c, a));
        put(std::floor(s.x + tail.x * 1.5f), std::floor(s.y + tail.y * 1.5f), 1, 1, withA(c0, 0.6f * a));
        break;
      }
      case PK_SPARK: {
        const Vec2 b = s - q.v * 0.028f;
        line(P, (int)s.x, (int)s.y, (int)b.x, (int)b.y, withA(c1, a), true);
        P.add(s.x, s.y, 1, 1, withA(c2, a));
        break;
      }
      case PK_SHARD: {
        if (q.size > 1.5f) P.blit(cs, iSprite(1).x, iSprite(1).y, 8, 8, s.x - 4, s.y - 4, 8, 8, withA(c1, a), add ? 1 : 0, q.spin > kPi);
        else {
          const bool hz = std::fabs(q.v.x) > std::fabs(q.v.y);
          put(s.x, s.y, hz ? 2.0f : 1.0f, hz ? 1.0f : 2.0f, withA(c1, a));
          P.add(s.x, s.y, 1, 1, withA(c2, a * (0.5f + 0.5f * std::sin(C.t * 25 + q.spin))));
        }
        break;
      }
      case PK_PETAL: {
        const bool flat = std::sin(C.t * 8 + q.spin) > 0;
        put(s.x, s.y, flat ? 2.0f : 1.0f, flat ? 1.0f : 2.0f, withA(c1, a));
        put(s.x, s.y, 1, 1, withA(c2, a));
        break;
      }
      case PK_LEAF: case PK_FEATHER: case PK_BONE: case PK_DROP: {
        const int si = q.kind == PK_LEAF ? 3 : q.kind == PK_FEATHER ? 4 : q.kind == PK_BONE ? 5 : 6;
        if (q.size > 1.5f) P.blit(cs, iSprite(si).x, iSprite(si).y, 8, 8, s.x - 4, s.y - 4, 8, 8, withA(c1, a), add ? 1 : 0, std::sin(C.t * 6 + q.spin) > 0);
        else { put(s.x, s.y, 1, q.kind == PK_DROP ? 2.0f : 1.0f, withA(c1, a)); put(s.x, s.y, 1, 1, withA(c2, a)); if (q.kind != PK_DROP) put(s.x + 1, s.y, 1, 1, withA(c0, a)); }
        break;
      }
      case PK_GLYPH: {
        P.blit(W.runes(q.glyph / 8u), (int)(q.glyph & 7) * RUNE, 0, RUNE, RUNE, s.x - 4, s.y - 4, RUNE, RUNE, withA(c2, a), 1, false);
        break;
      }
      case PK_NOTE: icon(P, W, I_NOTE, s.x - 2, s.y - 3, withA(c2, a), 1); break;
      case PK_HEART: icon(P, W, I_HEART, s.x - 3, s.y - 3, withA(c1, a), 1); P.add(s.x - 2, s.y - 2, 1, 1, withA(c2, a)); break;
      case PK_STAR:
        if (q.size > 1.2f) icon(P, W, I_STARS, s.x - 2, s.y - 2, withA(c2, a), 1);
        else icon(P, W, I_CROSS, s.x - 1, s.y - 1, withA(c2, a), 1);
        break;
      case PK_MOTE:
        P.add(s.x - (q.size > 1.5f ? 1.0f : 0.0f), s.y, q.size > 1.5f ? 3.0f : 2.0f, q.size > 1.5f ? 2.0f : 1.0f, withA(c1, 0.7f * a));
        P.add(s.x, s.y, 1, 1, withA(c2, a));
        break;
      case PK_SMOKE: case PK_PUFF: {
        const float sc = q.size * (1.0f + (1.0f - r) * 0.9f);
        const Ico ic = q.kind == PK_SMOKE ? iPuff((int)(q.spin * 2) & 3) : iCloud((int)(q.spin * 2) & 1);
        const float al = (q.kind == PK_SMOKE ? 0.45f : 0.5f) * std::min(1.0f, r * 1.6f) * std::min(1.0f, (1 - r) * 8 + 0.2f);
        if (sc <= 1.5f) {
          const float w = std::floor((float)ic.w * sc), h = std::floor((float)ic.h * sc);
          if (w < 2) break;
          P.blit(cs, ic.x, ic.y, ic.w, ic.h, s.x - w * 0.5f, s.y - h * 0.5f, w, h, withA(c1, al), add ? 1 : 0, false);
          break;
        }
        // (M9 fixer, review: "the meteor's smoke is square 2x blobs") a big puff is a cluster of 1x puffs (the
        // world's pixel size), the lower ones darker (c0) and the top-left ones lighter (c2)
        const int n = std::min(7, 2 + (int)(sc * sc * 0.6f));
        const uint32_t hs = (uint32_t)(q.spin * 9973.0f);
        for (int i = 0; i < n; i++) {
          const uint32_t hh = hmix(hs, (uint32_t)i);
          const float ox = (hf(hh) - 0.5f) * (float)ic.w * (sc - 1) * 0.9f, oy = (hf(hh >> 8) - 0.5f) * (float)ic.h * (sc - 1) * 0.8f;
          const Color cc = oy > 2 ? c0 : (ox < 0 && oy < 0) ? c2 : c1;
          P.blit(cs, ic.x, ic.y, ic.w, ic.h, std::floor(s.x + ox - (float)ic.w * 0.5f), std::floor(s.y + oy - (float)ic.h * 0.5f), (float)ic.w, (float)ic.h, withA(cc, al * 0.85f), add ? 1 : 0, false);
        }
        break;
      }
      case PK_ROCK: {
        const float w = q.size > 1.5f ? 3.0f : 2.0f;
        P.rect(s.x, s.y, w, 2, withA(c1, a));
        P.rect(s.x, s.y, 1, 1, withA(c2, a));
        P.rect(s.x + w - 1, s.y + 1, 1, 1, withA(c0, a));
        break;
      }
      default: put(s.x, s.y, 1, 1, withA(c1, a)); break;
    }
  }
}

// ------------------------------------------------------------------ drawing: status overlays on a body
// feet: the body's ground point (screen), bw / bh its size; returns the overlays drawn
template <class Pt> int drawStatus(Pt& P, World& W, const Ctx& C, Vec2 feet, float bw, float bh, uint16_t bits, uint32_t seed) {
  int n = 0;
  auto on = [&](Status s) { return (bits & magic::statusBit(s)) != 0; };
  const float x = std::floor(feet.x), y = std::floor(feet.y);
  const float lk = 0.4f + 0.6f * C.lightK;
  if (on(Status::Burning)) {   // flames licking up the body, a glow
    Look FL = fireLook();
    Sheet& rs = W.rampSheet(FL);
    const int nf = bw > 14 ? 3 : 2;
    for (int i = 0; i < nf; i++) {
      const uint32_t h = hmix(seed, (uint32_t)i);
      const float ox = ((float)i - (float)(nf - 1) * 0.5f) * bw * 0.32f + (hf(h) - 0.5f) * 2;
      const float oy = -bh * (0.15f + 0.3f * hf(h >> 6));
      flame(P, rs, i == nf / 2 && bw > 14 ? 1 : 0, (int)(C.t * 15) + (int)(h & 7), x + ox, y + oy, 0.92f);
    }
    glow(P, W, x, y - bh * 0.45f, bw * 0.7f, Color(1.0f, 0.5f, 0.15f, 0.35f));
    n++;
  }
  if (on(Status::Frozen)) {   // the ice shell: a cluster of crystal columns round the body, jagged points on top, each
                              // column lit on its left face and shaded on its right, a bright ridge, a glint sweeping
    const int w = (int)std::floor(bw + 6), x0 = (int)x - w / 2;
    const float base = y + 1;
    const float sweep = std::fmod(C.t * 0.8f + hf(seed), 1.0f) * (float)(w + 20) - 10;
    for (int i = 0; i < w; i++) {
      const float cx = ((float)i + 0.5f) / (float)w * 2 - 1;   // -1 .. 1 across
      const int cwid = std::max(4, w / 4);                     // three or four crystals across the body
      const int crystal = i / cwid;
      const uint32_t h = hmix(seed, (uint32_t)crystal);
      const float local = ((float)(i % cwid) + 0.5f) / (float)cwid;   // 0..1 inside its crystal
      const float peak = 1.0f - std::fabs(local - 0.5f) * 1.6f;                         // pointed tops
      const float hgt = (bh + 2) * (0.62f + 0.38f * hf(h)) * (1.0f - 0.3f * cx * cx) + peak * 4.0f;
      const float top = std::floor(base - hgt);
      const bool litFace = local < 0.5f;
      Color c = litFace ? Color(0.70f, 0.88f, 0.98f, 0.62f) : Color(0.44f, 0.64f, 0.88f, 0.62f);
      if (i == 0 || (litFace && local < 0.2f)) c = Color(0.9f, 0.97f, 1.0f, 0.7f);
      if (i == w - 1) c = Color(0.3f, 0.48f, 0.76f, 0.75f);
      P.rect((float)(x0 + i), top, 1, base - top, mulC(c, lk));
      P.rect((float)(x0 + i), top, 1, 1, mulC(Color(1, 1, 1, 0.9f), lk));                          // the bright tip / ridge
      if (litFace) P.rect((float)(x0 + i), top + 1, 1, 2, mulC(Color(0.92f, 0.98f, 1.0f, 0.55f), lk));
      if (std::fabs((float)i - sweep) < 1.0f) P.add((float)(x0 + i), top + 1, 1, base - top - 2, Color(1, 1, 1, 0.3f));
    }
    P.rect((float)x0, base - 1, (float)w, 1, mulC(Color(0.22f, 0.36f, 0.62f, 0.8f), lk));   // its foot
    P.rect((float)x0 + 2, base, (float)w - 1, 1, Color(0.02f, 0.04f, 0.1f, 0.25f));         // a shadow to the bottom right
    P.rect((float)x0 + 1, base, 1, 2, mulC(Color(0.8f, 0.92f, 1.0f, 0.8f), lk));            // icicles
    P.rect((float)(x0 + w - 3), base, 1, 3, mulC(Color(0.8f, 0.92f, 1.0f, 0.8f), lk));
    n++;
  } else if (on(Status::Chilled)) {   // rime: a pale rim of frost glints
    for (int i = 0; i < 5; i++) {
      const uint32_t h = hmix(seed, 20u + (uint32_t)i);
      const float tw = 0.5f + 0.5f * std::sin(C.t * 6 + (float)i * 1.3f);
      dot(P, x + (hf(h) - 0.5f) * bw, y - hf(h >> 7) * bh, Color(0.85f, 0.95f, 1.0f, 0.8f * tw), true);
    }
    gglow(P, W, x, y, bw * 0.8f, Color(0.5f, 0.75f, 1.0f, 0.2f));
    n++;
  }
  if (on(Status::Wet)) {   // a wet sheen: a few blue highlights down the body
    for (int i = 0; i < 4; i++) {
      const uint32_t h = hmix(seed, 40u + (uint32_t)i);
      P.rect(std::floor(x + (hf(h) - 0.5f) * bw * 0.7f), std::floor(y - bh * (0.2f + 0.6f * hf(h >> 6))), 1, 2, mulC(Color(0.6f, 0.82f, 1.0f, 0.7f), lk));
    }
    n++;
  }
  if (on(Status::Shocked)) {   // arcs crawling over the body, re-struck every 60 ms
    const uint32_t st = hmix(seed, (uint32_t)(C.t / 0.06f));
    for (int i = 0; i < 2; i++) {
      const uint32_t h = hmix(st, (uint32_t)i);
      const Vec2 a(x + (hf(h) - 0.5f) * bw, y - hf(h >> 5) * bh), b(x + (hf(h >> 10) - 0.5f) * bw, y - hf(h >> 15) * bh);
      bolt(P, W, a, b, h, Color(0.5f, 0.55f, 1.0f), Color(0.3f, 0.3f, 0.9f), Color(0.92f, 0.95f, 1.0f), 0.9f, 0, 0.4f);
    }
    n++;
  }
  if (on(Status::Poisoned)) {   // a sickly glow at the feet, the body tinged green
    gglow(P, W, x, y, bw * 0.9f, Color(0.45f, 0.85f, 0.2f, 0.45f));
    glow(P, W, x, y - bh * 0.5f, bw * 0.6f, Color(0.4f, 0.8f, 0.15f, 0.25f));
    for (int i = 0; i < 3; i++) {
      const float ph = std::fmod(C.t * 0.9f + (float)i * 0.33f + hf(seed), 1.0f);
      const float r = 1 + ph * 2;
      ring(P, x + ((float)i - 1) * bw * 0.3f, y - bh * 0.3f - ph * bh * 0.6f, r, r, Color(0.6f, 0.95f, 0.3f, 0.8f * (1 - ph)), false);
    }
    n++;
  }
  if (on(Status::Frightened)) {   // a dark violet swirl over the head and the fear mask flickering
    const float hy = y - bh - 5;
    for (int i = 0; i < 10; i++) {
      const float a = C.t * 5 + (float)i * kTau / 10;
      dot(P, x + std::cos(a) * 7, hy + std::sin(a) * 2.5f, Color(0.55f, 0.25f, 0.75f, 0.9f), false);
      dot(P, x + std::cos(a) * 7, hy + std::sin(a) * 2.5f, Color(0.6f, 0.3f, 0.9f, 0.4f), true);
    }
    if (std::sin(C.t * 7) > -0.3f) {
      icon(P, W, I_MASK, x - 3, hy - 10, Color(0.08f, 0.02f, 0.12f, 0.6f), 0);
      icon(P, W, I_MASK, x - 4, hy - 11, Color(0.62f, 0.36f, 0.86f, 0.95f), 0);
      icon(P, W, I_MASK, x - 4, hy - 11, Color(0.5f, 0.3f, 0.8f, 0.3f), 1);
    }
    P.rect(x + bw * 0.4f, y - bh * 0.8f, 1, 2, Color(0.7f, 0.85f, 1.0f, 0.8f * lk));   // a bead of sweat
    n++;
  }
  if (on(Status::Charmed)) {   // a pink ring of hearts over the head
    const float hy = y - bh - 5;
    for (int i = 0; i < 3; i++) {
      const float a = C.t * 2.5f + (float)i * kTau / 3;
      icon(P, W, I_HEART, x + std::cos(a) * 7 - 3, hy + std::sin(a) * 2.5f - 3, Color(1.0f, 0.45f, 0.7f, 0.9f), 1);
    }
    n++;
  }
  if (on(Status::Blinded)) {   // a dark band over the eyes and stars reeling round the head
    P.rect(x - 4, y - bh + 6, 8, 2, Color(0.1f, 0.08f, 0.1f, 0.75f));
    for (int i = 0; i < 3; i++) {
      const float a = C.t * 4 + (float)i * kTau / 3;
      icon(P, W, I_STARS, x + std::cos(a) * 7 - 2, y - bh - 2 + std::sin(a) * 2.5f, Color(1.0f, 0.95f, 0.7f, 0.9f), 1);
    }
    n++;
  }
  return n;
}

// ------------------------------------------------------------------ lights (world space): fn(point, radius, colour, k)
template <class F> int lightsOf(World& W, F light) {
  int n = 0;
  for (const Fx& f : W.fx) {
    if (f.delay > 0) continue;
    const Look& L = f.L;
    const float S = baseScale(L) * std::max(0.6f, f.mag);
    const float u = sat(f.t / f.life), k = 1 - u;
    switch (f.kind) {
      case (uint8_t)Phase::WindUp: light(f.p, (34 + 10 * f.charge) * S, L.c(2), 0.75f * sat(f.t / 0.15f)); n++; break;
      case (uint8_t)Phase::Launch: light(f.p, 70 * S, L.c(3), 0.9f * k * k); n++; break;
      case (uint8_t)Phase::Impact: light(f.p, (90 + 40 * (spectacle(L) ? 1.0f : 0.0f)) * S, L.c(3), k * k * 1.1f); light(f.p + Vec2(0, 6), 50 * S, L.c(2), k); n += 2; break;
      case (uint8_t)Phase::Channel: light(f.p, 40 * S, L.c(2), 0.7f); light(f.q, 50 * S, L.c(3), 0.8f); n += 2; break;
      case (uint8_t)Phase::Summon: light(f.p, 70 * S, L.c(2), 0.8f * sat((f.life - f.t) / 0.4f)); n++; break;
      case (uint8_t)Phase::Ward: light(f.p + Vec2(0, -10), 46 * S, L.c(3), 0.6f * k); n++; break;
      case (uint8_t)Phase::Combo: light(f.p, 80, cu(surfCol((Surface)f.sub, 2)), k); n++; break;
      case FX_METEOR: {
        if (f.hit) break;
        const float e = sat(f.t / f.life);
        light(f.v + (f.q - f.v) * (e * e), 130 * S * 0.6f, L.c(3), 1.0f);
        light(f.q, 60 * S * e, L.c(2), 0.6f * e);
        n += 2;
        break;
      }
      case FX_STRIKE: light(f.p, 150, L.c(3), k * 1.2f); n++; break;
      case FX_STARS: light(f.p, 60, L.c(3), 0.5f * k); n++; break;
      default: break;
    }
  }
  return n;
}

}  // namespace mfx
