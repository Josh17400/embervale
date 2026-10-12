// M8b Treasure (VISION_PLAN 15.27, 15.28): the worn powers' effects in the world. POWERS lane.
// Two sources: Ev::Power events (short effects: a lightning arc, a falling star, lifesteal motes, the phoenix's burst,
// a wolf called or fading, a proc's spark; View::powerEvent keeps them in powerFx_), and the sim's live state, drawn
// every frame (the Inferno / Fire Trail patches in gear::Live::fx, burning foes, the Bramble Crown's thorn ring round
// whoever wears it, Chronos' time slow, the spectral wolves). Everything is pixel work on the 480 x 270 grid at whole
// pixels: rect / rectAdd runs and the actors' own sheets re-blitted additively, never a per-pixel full-screen loop
// (the phone's frame budget). The light comes from the top left like every sprite; the palette is the game's warm
// fire ramp (ember red -> orange -> straw -> white), the storm ramp (violet-blue -> ice -> white), the thorn greens and
// the spectral cyan.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include "rpg/art.h"
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/powers.h"
#include "rpg/view/view.h"

namespace {
using powers::Fx;

float frac01(uint32_t h) { return (float)(h & 0xFFFF) / 65535.0f; }
uint32_t mix(uint32_t a, uint32_t b) { return hash32(a * 0x9E3779B1u ^ (b + 0x7F4A7C15u)); }
float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

// one pixel, normal or additive
void dot(Pix& P, float x, float y, Color c, bool add) {
  if (add) P.rectAdd(std::floor(x), std::floor(y), 1, 1, c); else P.rect(std::floor(x), std::floor(y), 1, 1, c);
}
// a 1-px line (Bresenham on whole pixels)
void line(Pix& P, int x0, int y0, int x1, int y1, Color c, bool add) {
  int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
  for (int n = 0; n < 512; n++) {
    if (add) P.rectAdd((float)x0, (float)y0, 1, 1, c); else P.rect((float)x0, (float)y0, 1, 1, c);
    if (x0 == x1 && y0 == y1) break;
    const int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}
// a soft additive glow: three nested discs drawn as row runs (a round falloff, at most a few dozen rects)
void glow(Pix& P, float x, float y, float r, Color c) {
  if (r < 1.0f) { P.rectAdd(std::floor(x), std::floor(y), 1, 1, c); return; }
  const float a = c.a;
  for (int ring = 0; ring < 3; ring++) {
    const float rr = r * (ring == 0 ? 1.0f : ring == 1 ? 0.62f : 0.32f);
    const int ir = std::max(1, (int)rr);
    const int step = std::max(1, ir / 5);
    for (int j = -ir; j <= ir; j += step) {
      const float t = (float)j / rr;
      const float w = rr * std::sqrt(std::max(0.0f, 1.0f - t * t));
      if (w < 0.5f) continue;
      P.rectAdd(std::floor(x - w), std::floor(y + (float)j), std::floor(w * 2 + 1), (float)step, c.withA(a * 0.34f));
    }
  }
}
// a flat glow on the ground (the 3/4 view: twice as wide as it is deep), in rows
void groundGlow(Pix& P, float cx, float cy, float rx, Color c) {
  const float ry = rx * 0.45f;
  const int rows = std::max(2, (int)ry);
  for (int j = -rows; j <= rows; j++) {
    const float t = (float)j / (float)rows;
    const float w = rx * std::sqrt(std::max(0.0f, 1.0f - t * t));
    P.rectAdd(std::floor(cx - w), std::floor(cy + t * ry), std::floor(w * 2), 1, c.withA(c.a * (1.0f - 0.55f * std::fabs(t))));
  }
}

// the fire ramp by heat (1 white-hot core .. 0 a dark ember tip)
Color fire(float heat, float a) {
  heat = clamp01(heat);
  if (heat > 0.8f) return Color(1.0f, 0.97f, 0.78f, a);
  if (heat > 0.6f) return Color(1.0f, 0.86f, 0.38f, a);
  if (heat > 0.4f) return Color(1.0f, 0.6f, 0.16f, a);
  if (heat > 0.2f) return Color(0.92f, 0.32f, 0.08f, a);
  return Color(0.58f, 0.14f, 0.06f, a);
}
// one flame tongue, base at (x, y), h tall: rows narrowing upward, the core hottest, a lean that sways, its top-left
// edge a shade brighter (the light) and its right edge a shade darker
void tongue(Pix& P, float x, float y, float h, float w, float sway, float a) {
  const int n = std::max(3, (int)h);
  for (int j = 0; j < n; j++) {
    const float t = (float)j / (float)n;                 // 0 base .. 1 tip
    const float ww = std::max(1.0f, w * (1.0f - t * t) * (j == 0 ? 0.8f : 1.0f));
    const float cx = x + sway * t * t * 3.0f;
    const float yy = y - (float)j;
    const float heat = 1.0f - t * 1.05f;
    P.rect(std::floor(cx - ww * 0.5f), std::floor(yy), std::floor(std::max(1.0f, ww)), 1, fire(heat - 0.25f, a));
    if (ww >= 3) {
      P.rect(std::floor(cx - ww * 0.5f + 1), std::floor(yy), std::floor(std::max(1.0f, ww - 2)), 1, fire(heat, a));
      P.rect(std::floor(cx - ww * 0.5f), std::floor(yy), 1, 1, fire(heat + 0.15f, a * 0.9f));      // the lit edge (top-left light)
      P.rect(std::floor(cx + ww * 0.5f - 1), std::floor(yy), 1, 1, fire(heat - 0.45f, a * 0.9f));  // the shade side
    }
  }
}

// the spectral wolf's frame (as the renderer picks a monster's: render.cpp)
int monsterFrame(const Actor& a) {
  switch (a.st) {
    case AState::Walk: return (int)(a.animT * 8) % 4;
    case AState::Windup: return 4;
    case AState::Strike: return 5;
    case AState::Hurt: return 6;
    case AState::Dead: case AState::Down: return 7;
    default: return (int)(a.animT * 3) % 2;
  }
}
const Actor* actorById(const Game& g, uint32_t id) {
  if (!id) return nullptr;
  for (const Actor& a : g.actors) if ((uint32_t)a.id == id) return &a;
  return nullptr;
}
bool onScreen(Vec2 s, float m) { return s.x > -m && s.y > -m && s.x < (float)Pix::W + m && s.y < (float)Pix::H + m; }
}  // namespace

void View::powerEvent(Game& g, const Event& e) {
  (void)g;
  PowerFx f;
  f.kind = e.a;
  f.p = e.p;
  f.q = e.q;
  f.mag = e.f;
  f.follow = e.id;
  switch ((Fx)e.a) {
    case Fx::Lightning: f.life = 0.24f; break;
    case Fx::Lifesteal: f.life = 0.62f; break;
    case Fx::Phoenix: f.life = 1.3f; break;
    case Fx::Star: f.life = powers::STAR_DELAY + 0.5f; break;
    case Fx::TimeSlow: f.life = 0.9f; break;
    case Fx::Wolf: f.life = 0.7f; break;
    case Fx::Thorns: f.life = 0.45f; break;
    case Fx::Proc: f.life = 0.4f; break;
    default: f.life = 0.4f; break;
  }
  f.seed = hash32((uint32_t)powerFx_.size() * 2654435761u ^ (uint32_t)(t_ * 1000.0f) ^ (uint32_t)(e.p.x * 31.0f) ^ (uint32_t)(e.q.y * 17.0f));
  if (powerFx_.size() < 96) powerFx_.push_back(f);
}

void View::powerUpdate(Game& g, float dt) {
  (void)g;
  for (PowerFx& f : powerFx_) f.t += dt;
  powerFx_.erase(std::remove_if(powerFx_.begin(), powerFx_.end(), [](const PowerFx& f) { return f.t >= f.life; }), powerFx_.end());
}

void View::drawPowerFx(Game& g, Vec2 cam) {
  Pix& P = *pix_;
  const powers::State& F = g.craft.live.fx;
  auto S = [&](Vec2 w) { return Vec2(std::floor(w.x - cam.x), std::floor(w.y - cam.y)); };

  // ================================================================ Chronos: the world goes cold and slow
  if (F.slowT > 0) {
    const float in = clamp01((powers::CHRONOS_SECS - F.slowT) / 0.25f), out = clamp01(F.slowT / 0.4f);
    const float k = std::min(in, out);
    // one cool wash (multiplied darker blue, then a faint additive ice) and a vignette of four bands
    P.rect(0, 0, (float)Pix::W, (float)Pix::H, Color(0.12f, 0.20f, 0.38f, 0.30f * k));
    P.rectAdd(0, 0, (float)Pix::W, (float)Pix::H, Color(0.06f, 0.12f, 0.22f, 0.12f * k));
    // the slowed: every hunting beast glazed in ice light, held a beat behind itself (an after-image)
    for (const Actor& a : g.actors) {
      if (!a.hostile || a.human || a.st == AState::Dead || a.critter) continue;
      const Vec2 s0 = S(a.p);
      if (!onScreen(s0, 40)) continue;
      art::MonsterLook L;
      L.base = a.mon; L.overlays = a.overlays; L.tint = a.bodyTint; L.scale = a.scalePct;
      const int cw = art::lookCellW(L), chh = art::lookCellH(L);
      const Tex& t = monsterTex(a);
      const int fr = monsterFrame(a);
      const bool flip = a.aim.x < 0;
      const float x = std::floor(a.p.x - (float)cw / 2.0f - cam.x + 0.5f), y = std::floor(a.p.y - (float)chh + 2 - cam.y + 0.5f);
      P.blitEx(t, fr * cw, 0, cw, chh, x, y, (float)cw, (float)chh, flip, Color(0.25f, 0.45f, 0.8f, 0.45f * k), 1);
      const Vec2 back = norm(a.vel + a.aim * -1.0f + Vec2(0.0001f, 0)) * 4.0f;
      P.blitEx(t, fr * cw, 0, cw, chh, x + std::floor(back.x), y + std::floor(back.y * 0.5f), (float)cw, (float)chh, flip, Color(0.2f, 0.35f, 0.7f, 0.22f * k), 1);
    }
    for (int b = 0; b < 4; b++) {
      const float d = (float)(b * 6), a = 0.11f * k * (1.0f - 0.22f * (float)b);
      P.rect(0, d, (float)Pix::W, 6, Color(0.06f, 0.10f, 0.24f, a));
      P.rect(0, (float)Pix::H - d - 6, (float)Pix::W, 6, Color(0.06f, 0.10f, 0.24f, a));
      P.rect(d, 0, 6, (float)Pix::H, Color(0.06f, 0.10f, 0.24f, a));
      P.rect((float)Pix::W - d - 6, 0, 6, (float)Pix::H, Color(0.06f, 0.10f, 0.24f, a));
    }
    // the slow pulse: a ring keeps breathing out from the hero every 0.75 s, and clock ticks hang in the air
    const Vec2 c = S(g.pl().p);
    const float ph = std::fmod(t_ * 1.33f, 1.0f);
    for (int ring = 0; ring < 2; ring++) {
      const float pr = std::fmod(ph + 0.5f * (float)ring, 1.0f);
      const float rx = 24.0f + pr * 170.0f;
      const int n = std::max(48, (int)(rx * 2.4f));
      for (int i = 0; i < n; i++) {
        const float an = 6.2831853f * (float)i / (float)n;
        const float x = c.x + std::cos(an) * rx, y = c.y - 6 + std::sin(an) * rx * 0.55f;
        dot(P, x, y, Color(0.6f, 0.85f, 1.0f, 0.55f * k * (1.0f - pr)), true);
        dot(P, x, y + 1, Color(0.3f, 0.5f, 1.0f, 0.3f * k * (1.0f - pr)), true);
      }
    }
    for (int i = 0; i < 12; i++) {
      const uint32_t h = mix(0xC405u, (uint32_t)i);
      const float ax = c.x - 90 + frac01(h) * 180, ay = c.y - 70 + frac01(h >> 8) * 110 + std::sin(t_ * 0.7f + (float)i) * 2.0f;
      const float tw = 0.5f + 0.5f * std::sin(t_ * 3.0f + (float)i * 1.7f);
      dot(P, ax, ay, Color(0.7f, 0.9f, 1.0f, 0.55f * k * tw), true);
      dot(P, ax + 1, ay, Color(0.4f, 0.6f, 1.0f, 0.25f * k * tw), true);
    }
  }

  // ================================================================ fire patches (Inferno, Fire Trail)
  for (const auto& pa : F.patches) {
    const Vec2 s = S(pa.p);
    if (!onScreen(s, 24)) continue;
    const float age = pa.t, left = pa.life - pa.t;
    const float grow = clamp01(age / 0.18f), fade = clamp01(left / 0.7f);
    const float k = grow * fade;
    // the scorched ground (normal blend: a dark, ragged smudge) and the warm light it throws
    for (int j = -2; j <= 2; j++) {
      const float w = (j == 0 ? 9.0f : std::fabs((float)j) == 1 ? 8.0f : 5.0f) * (0.6f + 0.4f * grow);
      P.rect(std::floor(s.x - w), std::floor(s.y + (float)j), std::floor(w * 2), 1, Color(0.09f, 0.05f, 0.04f, 0.42f * fade));
    }
    groundGlow(P, s.x, s.y, 12.0f * (0.7f + 0.3f * k), Color(1.0f, 0.42f, 0.12f, 0.22f * k));
    // three tongues of their own sizes and clocks
    for (int f = 0; f < 3; f++) {
      const uint32_t h = mix(pa.seed, (uint32_t)f);
      const float ox = ((float)f - 1.0f) * 4.5f + (frac01(h) - 0.5f) * 2.0f;
      const float speed = 9.0f + frac01(h >> 4) * 5.0f;
      const float fl = 0.75f + 0.25f * std::sin(t_ * speed + frac01(h >> 9) * 6.28f);
      const float hgt = (f == 1 ? 10.0f : 7.0f) * fl * k + 1.0f;
      const float sway = std::sin(t_ * 5.0f + (float)f * 2.1f) * 0.7f;
      tongue(P, s.x + ox, s.y + (f == 1 ? 1.0f : 0.0f), hgt, f == 1 ? 5.0f : 4.0f, sway, 0.95f * std::max(0.25f, k));
    }
    glow(P, s.x, s.y - 4, 7.0f, Color(1.0f, 0.55f, 0.2f, 0.35f * k));
    // embers: each rises, drifts and winks out on its own cycle
    for (int e = 0; e < 4; e++) {
      const uint32_t h = mix(pa.seed ^ 0xE3B3u, (uint32_t)e);
      const float cyc = 0.6f + frac01(h) * 0.5f;
      const float ph = std::fmod(age + frac01(h >> 6) * cyc, cyc) / cyc;
      const float ex = s.x + (frac01(h >> 12) - 0.5f) * 12.0f + std::sin(ph * 6.0f + (float)e) * 2.0f;
      const float ey = s.y - 4.0f - ph * 18.0f;
      dot(P, ex, ey, fire(1.0f - ph, (1.0f - ph) * fade), true);
    }
  }

  // ================================================================ burning foes (Affix::Burn, Ember, the patches)
  for (const Actor& a : g.actors) {
    if (a.st == AState::Dead || (a.burnDpsT <= 0 && a.burnT <= 0) || a.player) continue;
    const Vec2 s = S(a.p);
    if (!onScreen(s, 30)) continue;
    const bool strong = a.burnDpsT > 0;
    const float body = a.human ? 22.0f : 14.0f;
    const uint32_t sd = (uint32_t)a.id * 0x2545F491u;
    for (int f = 0; f < (strong ? 3 : 2); f++) {
      const uint32_t h = mix(sd, (uint32_t)f);
      const float ox = (frac01(h) - 0.5f) * (a.human ? 8.0f : 12.0f);
      const float oy = -2.0f - frac01(h >> 5) * body * 0.5f;
      const float fl = 0.6f + 0.4f * std::sin(t_ * (11.0f + (float)f * 3.0f) + (float)a.id);
      tongue(P, s.x + ox, s.y + oy, (strong ? 7.0f : 5.0f) * fl + 1, 3.0f, std::sin(t_ * 6 + (float)f) * 0.6f, 0.85f);
    }
    glow(P, s.x, s.y - body * 0.45f, strong ? 8.0f : 6.0f, Color(1.0f, 0.5f, 0.15f, 0.30f));
    for (int e = 0; e < 3; e++) {
      const uint32_t h = mix(sd ^ 0xB0Bu, (uint32_t)e);
      const float ph = std::fmod(t_ * (1.3f + frac01(h)) + frac01(h >> 7), 1.0f);
      dot(P, s.x + (frac01(h >> 13) - 0.5f) * 10.0f, s.y - body * 0.6f - ph * 14.0f, fire(1.0f - ph, 1.0f - ph), true);
    }
  }

  // ================================================================ the Bramble Crown: a ring of thorns on the ground
  auto bramble = [&](const Actor& w, float pulse) {
    const Vec2 c = S(w.p);
    if (!onScreen(c, 40)) return;
    const float rx = powers::BRAMBLE_REACH, ry = powers::BRAMBLE_REACH / 1.25f * 0.62f;   // (the 3/4 view: flattened)
    const int n = 104;
    const float rot = t_ * 0.30f;
    groundGlow(P, c.x, c.y, rx + 3, Color(0.45f, 0.75f, 0.25f, 0.05f + 0.14f * pulse));
    // the bodies standing on the ring (the wearer, foes, friends): the vine behind their feet passes behind them
    struct Occ { float x0, x1, y0, y1; };
    Occ occ[12];
    int no = 0;
    for (const Actor& o : g.actors) {
      if (no >= 12 || o.st == AState::Dead || o.indoors || o.fly) continue;
      const Vec2 so = S(o.p);
      if (std::fabs(so.x - c.x) > rx + 12 || std::fabs(so.y - c.y) > ry + 26) continue;
      const float hw = o.human || o.player || o.comp ? 6.0f : 10.0f, hh = o.human || o.player || o.comp ? 22.0f : 14.0f;
      occ[no++] = Occ{so.x - hw, so.x + hw, so.y - hh, so.y - 1};
    }
    auto hidden = [&](float x, float y) {
      for (int k = 0; k < no; k++) if (x >= occ[k].x0 && x <= occ[k].x1 && y >= occ[k].y0 && y <= occ[k].y1) return true;
      return false;
    };
    // two bark strands twisting round each other, each with a cast shadow below it, thorns, leaves and rose hips
    for (int strand = 0; strand < 2; strand++)
      for (int i = 0; i < n; i++) {
        const float an = 6.2831853f * (float)i / (float)n + rot;
        const float tw = std::sin(an * 9.0f + (float)strand * 3.14159f + t_ * 0.5f);   // the twist
        const float rr = rx + tw * 1.4f, rv = ry + tw * 0.7f;
        const float x = std::floor(c.x + std::cos(an) * rr), y = std::floor(c.y + std::sin(an) * rv);
        if (hidden(x, y)) continue;   // (behind someone's body)
        const bool top = tw > 0;                                  // this strand lies over the other here
        dot(P, x + 1, y + 2, Color(0.05f, 0.04f, 0.03f, 0.40f), false);   // its shadow on the ground (light from the top left)
        // a 2-px thick stem: the lit upper row, the shaded under row (round in section)
        const Color bark = top ? Color(0.46f, 0.29f, 0.19f, 1.0f) : Color(0.30f, 0.18f, 0.13f, 1.0f);
        const Color under = top ? Color(0.27f, 0.15f, 0.11f, 1.0f) : Color(0.18f, 0.10f, 0.08f, 1.0f);
        dot(P, x, y + 1, under, false);
        dot(P, x, y, bark, false);
        if (top && std::cos(an) < 0.3f) dot(P, x, y - 1, Color(0.66f, 0.48f, 0.31f, 0.85f), false);   // lit from the top left
        dot(P, x, y, Color(0.5f, 0.85f, 0.3f, 0.10f + 0.35f * pulse), true);                          // the crown's green fire
        if (strand == 0 && i % 4 == 0) {   // a thorn, out and up, bone-pale at the tip (a sting drives them out red)
          const float ox = std::cos(an), oy = std::sin(an) * 0.55f - 0.75f;
          const float l = 2.5f + 2.5f * pulse;
          dot(P, x + ox, y + oy, Color(0.40f, 0.24f, 0.16f, 1.0f), false);
          dot(P, x + ox + 1, y + oy, Color(0.30f, 0.17f, 0.12f, 1.0f), false);
          dot(P, x + ox * 2.0f, y + oy * 2.0f, Color(0.56f, 0.40f, 0.28f, 1.0f), false);
          dot(P, x + ox * l, y + oy * l, Color(0.92f, 0.88f, 0.72f, 1.0f), false);
          if (pulse > 0.2f) dot(P, x + ox * (l + 1), y + oy * (l + 1), Color(1.0f, 0.25f, 0.2f, 0.9f * pulse), true);
        }
        if (strand == 1 && i % 9 == 4) {   // a leaf: dark underside, mid blade, a lit top-left edge
          const float sx = std::cos(an) > 0 ? 1.0f : -1.0f;
          dot(P, x + sx, y - 1, Color(0.22f, 0.40f, 0.16f, 1.0f), false);
          dot(P, x + sx * 2, y - 1, Color(0.40f, 0.64f, 0.26f, 1.0f), false);
          dot(P, x + sx * 2, y - 2, Color(0.62f, 0.84f, 0.38f, 1.0f), false);
          dot(P, x + sx * 3, y - 2, Color(0.46f, 0.70f, 0.28f, 1.0f), false);
          dot(P, x + sx * 1, y - 2, Color(0.70f, 0.90f, 0.44f, 1.0f), false);
        }
        if (strand == 0 && i % 26 == 11) {   // a rose hip: red with a glint
          dot(P, x, y - 1, Color(0.72f, 0.10f, 0.12f, 1.0f), false);
          dot(P, x + 1, y - 1, Color(0.55f, 0.06f, 0.08f, 1.0f), false);
          dot(P, x, y - 2, Color(1.0f, 0.55f, 0.5f, 1.0f), false);
        }
      }
  };
  {
    float pulseP = 0;
    for (const PowerFx& f : powerFx_) if (f.kind == (int)Fx::Thorns && f.follow == (uint32_t)g.pl().id) pulseP = std::max(pulseP, (1.0f - f.t / f.life) * f.mag);
    if (g.craft.live.stats.has(Unique::BrambleCrown) && g.pl().st != AState::Dead) bramble(g.pl(), pulseP);
    for (const Actor& a : g.actors) {
      if (!a.comp || a.st == AState::Dead) continue;
      float pulse = -1;
      for (const PowerFx& f : powerFx_) if (f.kind == (int)Fx::Thorns && f.follow == (uint32_t)a.id) pulse = std::max(pulse, (1.0f - f.t / f.life) * f.mag);
      if (pulse >= 0) bramble(a, pulse);   // (a companion's ring shows while their aura stings)
    }
  }

  // ================================================================ spectral wolves
  for (const Actor& a : g.actors) {
    if (a.powTag != powers::PT_WOLF || a.st == AState::Dead) continue;
    art::MonsterLook L;
    L.base = a.mon; L.tint = a.bodyTint;
    const int cw = art::lookCellW(L), chh = art::lookCellH(L);
    const Vec2 s = S(a.p);
    if (!onScreen(s, 40)) continue;
    const Tex& t = monsterTex(a);
    const int fr = monsterFrame(a);
    const bool flip = a.aim.x < 0;
    const float x = std::floor(a.p.x - (float)cw / 2.0f - cam.x + 0.5f), y = std::floor(a.p.y - (float)chh + 2 - cam.y + 0.5f);
    const float life = clamp01(a.powT / 1.0f);                              // it thins out in its last second
    const float fl = 0.75f + 0.25f * std::sin(t_ * 17.0f + (float)a.id) * std::sin(t_ * 5.3f);
    groundGlow(P, s.x, s.y, 11.0f, Color(0.35f, 0.7f, 1.0f, 0.22f * life));
    // the ghost glow: its own sheet added back over it in the spectral cyan (it reads as light, not hide)
    P.blitEx(t, fr * cw, 0, cw, chh, x, y, (float)cw, (float)chh, flip, Color(0.30f, 0.62f, 1.0f, 0.42f * fl * life), 1);
    // an after-image a few pixels behind when it runs
    if (a.st == AState::Walk) {
      const float bx = flip ? 3.0f : -3.0f;
      P.blitEx(t, fr * cw, 0, cw, chh, x + bx, y, (float)cw, (float)chh, flip, Color(0.25f, 0.5f, 1.0f, 0.25f * life), 1);
      P.blitEx(t, fr * cw, 0, cw, chh, x + bx * 2.2f, y, (float)cw, (float)chh, flip, Color(0.2f, 0.4f, 0.9f, 0.12f * life), 1);
    }
    // wisps rising off its back and its eyes' cold light
    for (int w = 0; w < 5; w++) {
      const uint32_t h = mix((uint32_t)a.id * 7u, (uint32_t)w);
      const float ph = std::fmod(t_ * (0.9f + frac01(h) * 0.6f) + frac01(h >> 8), 1.0f);
      const float wx = s.x + (frac01(h >> 14) - 0.5f) * (float)cw * 0.7f + std::sin(ph * 7.0f + (float)w) * 1.5f;
      const float wy = s.y - (float)chh * 0.45f - ph * 14.0f;
      dot(P, wx, wy, Color(0.6f, 0.9f, 1.0f, 0.7f * (1.0f - ph) * life), true);
    }
    const float ex = s.x + (flip ? -1.0f : 1.0f) * (float)cw * 0.3f, ey = y + (float)chh * 0.42f;
    glow(P, ex, ey, 2.5f, Color(0.7f, 0.95f, 1.0f, 0.7f * life));
  }

  // ================================================================ the events
  for (const PowerFx& f : powerFx_) {
    const float u = f.t / std::max(0.01f, f.life);   // 0 .. 1 through its life
    const float k = 1.0f - u;
    switch ((Fx)f.kind) {
      case Fx::Lightning: {
        // a jagged, branching arc re-struck every 50 ms: a wide dim glow, a bright core, white-hot nodes
        const Vec2 a = S(f.p), b = S(f.q);
        const Vec2 d = b - a;
        const float l = std::max(1.0f, len(d));
        const Vec2 nrm(-d.y / l, d.x / l);
        const int segs = std::max(4, (int)(l / 6.0f));
        const uint32_t strike = mix(f.seed, (uint32_t)(f.t * 20.0f));
        Vec2 pts[80];
        const int ns = std::min(segs, 78);
        for (int i = 0; i <= ns; i++) {
          const float t = (float)i / (float)ns;
          const float env = std::sin(t * 3.14159f);                       // pinned at both ends
          // a zig-zag (each kink the other way, its size random) on a slow random wander: jagged, never a straight rod
          const float zig = ((i & 1) ? 1.0f : -1.0f) * (0.35f + 0.65f * frac01(mix(strike, (uint32_t)i))) * (3.0f + l * 0.05f);
          const float wander = (frac01(mix(strike ^ 0x77u, (uint32_t)(i / 3))) - 0.5f) * (6.0f + l * 0.1f);
          const float off = (zig + wander) * env;
          pts[i] = a + d * t + nrm * off;
        }
        const float al = (u < 0.15f ? 1.0f : k) * (((int)(f.t * 40.0f) & 1) ? 1.0f : 0.7f);   // the flicker
        // pass 1: the halo (additive, wide and soft); pass 2: a solid violet-blue rim either side (normal blend, so the
        // bolt reads over bright grass and snow alike); pass 3: the 2-px white-hot core with its lit top-left pixel
        for (int i = 0; i < ns; i++) glow(P, pts[i].x, pts[i].y, 5.0f, Color(0.35f, 0.45f, 1.0f, 0.22f * al));
        for (int i = 0; i < ns; i++) {
          const Vec2 p0 = pts[i], p1 = pts[i + 1];
          line(P, (int)p0.x, (int)p0.y - 1, (int)p1.x, (int)p1.y - 1, Color(0.55f, 0.62f, 1.0f, 0.85f * al), false);
          line(P, (int)p0.x + 1, (int)p0.y + 2, (int)p1.x + 1, (int)p1.y + 2, Color(0.30f, 0.26f, 0.80f, 0.75f * al), false);
          line(P, (int)p0.x - 1, (int)p0.y, (int)p1.x - 1, (int)p1.y, Color(0.50f, 0.58f, 1.0f, 0.7f * al), false);
          line(P, (int)p0.x + 2, (int)p0.y + 1, (int)p1.x + 2, (int)p1.y + 1, Color(0.34f, 0.30f, 0.85f, 0.6f * al), false);
        }
        for (int i = 0; i < ns; i++) {
          const Vec2 p0 = pts[i], p1 = pts[i + 1];
          line(P, (int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y, Color(0.92f, 0.97f, 1.0f, al), false);
          line(P, (int)p0.x + 1, (int)p0.y + 1, (int)p1.x + 1, (int)p1.y + 1, Color(0.78f, 0.88f, 1.0f, al), false);
          if (i > 0) { dot(P, p0.x, p0.y, Color(1, 1, 1, al), false); glow(P, p0.x, p0.y, 2.5f, Color(0.8f, 0.9f, 1.0f, 0.5f * al)); }
        }
        // a fork or two off the middle, shorter and fainter
        for (int br = 0; br < 2; br++) {
          const int at = std::clamp(ns / 3 + br * ns / 3, 1, ns - 1);
          const uint32_t h = mix(strike, 0xB0u + (uint32_t)br);
          Vec2 p = pts[at];
          const float side = (h & 1) ? 1.0f : -1.0f;
          Vec2 dir = norm(d * (1.0f / l) * 0.6f + nrm * side);
          for (int s2 = 0; s2 < 3; s2++) {
            const Vec2 q = p + dir * (4.0f + frac01(h >> (s2 * 5)) * 4.0f) + nrm * ((frac01(h >> (s2 * 3 + 2)) - 0.5f) * 4.0f);
            const float fa = al * (1.0f - 0.28f * (float)s2);
            line(P, (int)p.x + 1, (int)p.y + 1, (int)q.x + 1, (int)q.y + 1, Color(0.36f, 0.32f, 0.88f, 0.6f * fa), false);
            line(P, (int)p.x, (int)p.y, (int)q.x, (int)q.y, Color(0.80f, 0.88f, 1.0f, 0.9f * fa), false);
            p = q;
          }
        }
        // the struck foe lit up: a hot white bloom and a flash on the ground under it
        glow(P, b.x, b.y, 9.0f, Color(0.6f, 0.75f, 1.0f, 0.7f * al));
        glow(P, b.x, b.y, 3.0f, Color(1.0f, 1.0f, 1.0f, 0.8f * al));
        groundGlow(P, b.x, b.y + 8, 12.0f, Color(0.45f, 0.6f, 1.0f, 0.35f * al));
        glow(P, a.x, a.y, 5.0f, Color(0.6f, 0.75f, 1.0f, 0.5f * al));
        // the struck end's sparks
        for (int sp = 0; sp < 4; sp++) {
          const uint32_t h = mix(strike, 0x5Fu + (uint32_t)sp);
          const float an = frac01(h) * 6.2831853f, r = 2.0f + u * 8.0f;
          dot(P, b.x + std::cos(an) * r, b.y + std::sin(an) * r * 0.7f, Color(0.85f, 0.95f, 1.0f, k), true);
        }
        break;
      }
      case Fx::Lifesteal: {
        // crimson motes curl from the struck foe to the wearer, a pale-rose core and a two-pixel trail
        const Actor* w = actorById(g, f.follow);
        const Vec2 to = S(w ? w->p + Vec2(0, -9) : f.q), from0 = S(f.p);
        const int nm = 3 + (int)std::min(4.0f, f.mag * 2.0f);
        for (int m = 0; m < nm; m++) {
          const uint32_t h = mix(f.seed, (uint32_t)m);
          const float delay = (float)m / (float)nm * 0.22f + frac01(h) * 0.05f;   // a stream, one mote after another
          const float tt = clamp01((f.t - delay) / std::max(0.05f, f.life - 0.27f));
          const Vec2 from = from0 + Vec2((frac01(h >> 20) - 0.5f) * 8.0f, (frac01(h >> 3) - 0.5f) * 6.0f);   // torn from all over the body
          if (tt <= 0 || tt >= 1) continue;
          const float e = tt * tt * (3 - 2 * tt);
          const Vec2 mid = (from + to) * 0.5f + Vec2((frac01(h >> 6) - 0.5f) * 26.0f, -10.0f - frac01(h >> 12) * 12.0f);
          const Vec2 pa = from + (mid - from) * e, pb = mid + (to - mid) * e;
          const Vec2 p = pa + (pb - pa) * e;
          for (int tr = 2; tr >= 1; tr--) {
            const float e2 = std::max(0.0f, e - 0.06f * (float)tr);
            const Vec2 qa = from + (mid - from) * e2, qb = mid + (to - mid) * e2;
            const Vec2 q = qa + (qb - qa) * e2;
            P.rectAdd(std::floor(q.x), std::floor(q.y), 2, 1, Color(0.85f, 0.10f, 0.18f, 0.5f / (float)tr));
          }
          glow(P, p.x, p.y, 3.0f, Color(0.9f, 0.1f, 0.2f, 0.4f));
          P.rect(std::floor(p.x) - 1, std::floor(p.y), 3, 1, Color(0.78f, 0.08f, 0.16f, 1.0f));
          P.rect(std::floor(p.x), std::floor(p.y) - 1, 1, 3, Color(0.78f, 0.08f, 0.16f, 1.0f));
          dot(P, p.x, p.y, Color(1.0f, 0.62f, 0.66f, 1.0f), false);
          dot(P, p.x - 1, p.y - 1, Color(1.0f, 0.85f, 0.85f, 0.9f), true);   // the glint, top left
        }
        if (u > 0.75f) glow(P, to.x, to.y, 6.0f * (1.0f - u) * 4.0f, Color(1.0f, 0.3f, 0.35f, 0.5f));
        break;
      }
      case Fx::Phoenix: {
        const Actor* w = actorById(g, f.follow);
        const Vec2 c = S(w ? w->p : f.p);
        // the flash, a ring of fire racing out over the ground, a pillar of flame, two wings sweeping up, embers
        if (f.t < 0.12f) P.rectAdd(0, 0, (float)Pix::W, (float)Pix::H, Color(1.0f, 0.55f, 0.2f, 0.22f * (1.0f - f.t / 0.12f)));
        glow(P, c.x, c.y - 10, 18.0f * (1.0f - u * 0.5f), Color(1.0f, 0.6f, 0.2f, 0.6f * k));
        const float rr = 8.0f + u * powers::PHOENIX_REACH * 1.2f;
        for (int i = 0; i < 48; i++) {
          const float an = 6.2831853f * (float)i / 48.0f;
          const float x = c.x + std::cos(an) * rr, y = c.y + std::sin(an) * rr * 0.45f;
          tongue(P, x, y, (4.0f + 3.0f * std::sin(an * 3 + t_ * 9)) * k + 1, 2.0f, 0, 0.9f * k);
        }
        const float ph = std::min(1.0f, f.t / 0.35f);
        for (int col = -2; col <= 2; col++) {
          const float hh = (26.0f - std::fabs((float)col) * 6.0f) * ph * (u < 0.6f ? 1.0f : k / 0.4f);
          tongue(P, c.x + (float)col * 3.0f, c.y, hh, 4.0f, std::sin(t_ * 7 + (float)col) * 0.8f, 0.95f);
        }
        for (int side = -1; side <= 1; side += 2) {
          for (int fth = 0; fth < 10; fth++) {   // the wing: feathers of flame along a rising arc, the long ones outermost
            const float q = (float)fth / 9.0f;
            const float spread = ph * (6.0f + q * 28.0f);
            const float wx = c.x + (float)side * spread;
            const float wy = c.y - 16.0f - std::sin(q * 2.6f) * 14.0f * ph + q * q * 6.0f;
            const float fh = (5.0f + q * 7.0f) * (u < 0.55f ? 1.0f : k / 0.45f) + 1;
            tongue(P, wx, wy, fh, 3.0f, (float)side * (0.6f + q), 0.95f * std::min(1.0f, k * 1.6f));
          }
          glow(P, c.x + (float)side * 18.0f * ph, c.y - 26.0f * ph, 10.0f, Color(1.0f, 0.5f, 0.15f, 0.35f * k));
        }
        for (int e = 0; e < 16; e++) {
          const uint32_t h = mix(f.seed, (uint32_t)e);
          const float an = frac01(h) * 6.2831853f, sp = 20.0f + frac01(h >> 8) * 40.0f;
          const float ex = c.x + std::cos(an) * sp * u, ey = c.y - 10 + std::sin(an) * sp * u * 0.6f - u * u * 30.0f;
          dot(P, ex, ey, fire(1.0f - u, k), true);
        }
        break;
      }
      case Fx::Star: {
        const Vec2 a = S(f.p), b = S(f.q);
        const float fall = powers::STAR_DELAY;
        if (f.t < fall) {
          // the streak: a white-gold head with a long fading tail back along its path
          const float e = f.t / fall;
          const Vec2 h = a + (b - a) * (e * e);
          const Vec2 dir = norm(a - b);
          for (int i = 0; i < 40; i++) {
            const Vec2 q = h + dir * (float)i * 1.5f;
            const float tk = 1.0f - (float)i / 40.0f;
            const float wdt = i < 10 ? 3.0f : i < 22 ? 2.0f : 1.0f;
            P.rectAdd(std::floor(q.x - wdt * 0.5f), std::floor(q.y), wdt, 1, Color(1.0f, 0.55f + 0.4f * tk, 0.25f + 0.5f * tk, 0.9f * tk));
            if (i % 4 == 2) dot(P, q.x + (i % 8 < 4 ? 2.0f : -2.0f), q.y + 1, Color(1.0f, 0.75f, 0.35f, 0.6f * tk), true);   // sparks shed
          }
          glow(P, h.x, h.y, 8.0f, Color(1.0f, 0.85f, 0.5f, 0.9f));
          P.rect(std::floor(h.x) - 1, std::floor(h.y) - 1, 3, 3, Color(1.0f, 0.97f, 0.85f, 1));
          P.rect(std::floor(h.x) - 2, std::floor(h.y), 5, 1, Color(1.0f, 0.95f, 0.75f, 1));
          P.rect(std::floor(h.x), std::floor(h.y) - 2, 1, 5, Color(1.0f, 0.95f, 0.75f, 1));
          // its shadow on the mark grows as it comes
          groundGlow(P, b.x, b.y, 4.0f + e * 10.0f, Color(1.0f, 0.8f, 0.4f, 0.15f + 0.25f * e));
        } else {
          // the impact: a white flash, a shock ring over the ground, rays and falling sparks
          const float v = (f.t - fall) / std::max(0.01f, f.life - fall), kk = 1.0f - v;
          glow(P, b.x, b.y - 6, 14.0f * kk + 2, Color(1.0f, 0.95f, 0.8f, 0.9f * kk));
          const float rr = 6.0f + v * powers::STAR_REACH * 1.3f;
          for (int i = 0; i < 40; i++) {
            const float an = 6.2831853f * (float)i / 40.0f;
            dot(P, b.x + std::cos(an) * rr, b.y + std::sin(an) * rr * 0.45f, Color(1.0f, 0.85f, 0.5f, 0.85f * kk), true);
          }
          for (int r = 0; r < 8; r++) {
            const float an = 6.2831853f * (float)r / 8.0f + 0.3f;
            const float r0 = 3.0f + v * 6.0f, r1 = 8.0f + v * 22.0f;
            line(P, (int)(b.x + std::cos(an) * r0), (int)(b.y - 6 + std::sin(an) * r0 * 0.7f), (int)(b.x + std::cos(an) * r1), (int)(b.y - 6 + std::sin(an) * r1 * 0.7f),
                 Color(1.0f, 0.9f, 0.6f, 0.6f * kk), true);
          }
          groundGlow(P, b.x, b.y, 16.0f, Color(1.0f, 0.6f, 0.25f, 0.3f * kk));
        }
        break;
      }
      case Fx::TimeSlow: {
        // the moment it triggers: a bright ice ring snaps out from the hero (the lasting tint is drawn above)
        const Vec2 c = S(f.p);
        const float rr = 6.0f + u * 120.0f;
        for (int i = 0; i < 80; i++) {
          const float an = 6.2831853f * (float)i / 80.0f;
          const float x = c.x + std::cos(an) * rr, y = c.y - 6 + std::sin(an) * rr * 0.55f;
          dot(P, x, y, Color(0.8f, 0.95f, 1.0f, 0.9f * k), true);
          if (i % 2 == 0) dot(P, x, y + 1, Color(0.4f, 0.6f, 1.0f, 0.5f * k), true);
        }
        glow(P, c.x, c.y - 10, 10.0f * k + 2, Color(0.6f, 0.85f, 1.0f, 0.7f * k));
        break;
      }
      case Fx::Wolf: {
        // called: a swirl of spectral light coalescing; fading (mag 0): the wisps scatter
        const Vec2 c = S(f.p);
        const bool call = f.mag > 0.5f;
        for (int i = 0; i < 14; i++) {
          const uint32_t h = mix(f.seed, (uint32_t)i);
          const float an = frac01(h) * 6.2831853f + (call ? -1.0f : 1.0f) * u * 4.0f;
          const float r = call ? (1.0f - u) * 18.0f + 2 : 3.0f + u * 18.0f;
          const float x = c.x + std::cos(an) * r, y = c.y - 7 + std::sin(an) * r * 0.6f - (call ? 0.0f : u * 10.0f);
          dot(P, x, y, Color(0.6f, 0.9f, 1.0f, 0.9f * k), true);
          dot(P, x, y + 1, Color(0.3f, 0.6f, 1.0f, 0.4f * k), true);
        }
        glow(P, c.x, c.y - 7, (call ? 10.0f * u : 8.0f * k) + 2, Color(0.45f, 0.8f, 1.0f, 0.6f * k));
        break;
      }
      case Fx::Thorns: {
        // the sting: the struck ring flares (drawn by the ring), plus a puff of torn leaves round the wearer
        const Actor* w = actorById(g, f.follow);
        const Vec2 c = S(w ? w->p : f.p);
        if (f.mag < 0.5f) break;
        for (int i = 0; i < 10; i++) {
          const uint32_t h = mix(f.seed, (uint32_t)i);
          const float an = frac01(h) * 6.2831853f, r = powers::BRAMBLE_REACH * (0.6f + 0.6f * u);
          const float x = c.x + std::cos(an) * r, y = c.y + std::sin(an) * r * 0.5f - u * 6.0f;
          dot(P, x, y, (i & 1) ? Color(0.45f, 0.7f, 0.25f, k) : Color(0.75f, 0.2f, 0.15f, k), false);
        }
        break;
      }
      case Fx::Proc: default: {
        // a small bright burst (Split Arrows' split, Reflection's turn, Shadowstep's vanish)
        const Vec2 c = S(f.p);
        for (int i = 0; i < 8; i++) {
          const float an = 6.2831853f * (float)i / 8.0f;
          const float r = 2.0f + u * 9.0f;
          dot(P, c.x + std::cos(an) * r, c.y - 6 + std::sin(an) * r * 0.7f, Color(0.85f, 0.85f, 1.0f, k), true);
        }
        glow(P, c.x, c.y - 6, 4.0f * k + 1, Color(0.7f, 0.7f, 1.0f, 0.6f * k));
        break;
      }
    }
  }
}
