// EMBERVALE art: effects. See rpg/art.h for the contract and rpg/art/art_internal.h for the shared helpers.
#include "rpg/art/art_internal.h"
#include "rpg/art/rig/rig_internal.h"   // (P2a style slice) rig2::steelArc, the Steel Arc

namespace art {

// =====================================================================================================
// 8. effects
// =====================================================================================================
namespace {

const Ramp kSteelFx = ramp5(rgba(90, 110, 160), rgba(150, 176, 214), rgba(200, 220, 240), rgba(232, 242, 252), rgba(255, 255, 255));

// sword arc: a crescent swept clockwise between tail and head angles (radians, 0 = right, +y down),
// rotated by `base`. Bright white at the leading edge, silver-blue and fading toward the tail.
void slashArc(Canvas& c, float cx, float cy, float R, float tail, float head, float width, float base) {
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      float r = std::sqrt(dx * dx + dy * dy);
      float a = std::atan2(dy, dx) - base;
      while (a < tail - PI) a += TAU;
      while (a > tail + PI) a -= TAU;
      if (a < tail || a > head) continue;
      float t = (a - tail) / std::max(0.01f, head - tail);     // 0 tail .. 1 head
      float w = width * (0.25f + 0.75f * t);
      if (r > R || r < R - w) continue;
      float edge = (R - r) / w;                                  // 0 outer rim .. 1 inner
      int k = t > 0.75f ? 4 : (t > 0.45f ? 3 : (t > 0.2f ? 2 : 1));
      if (edge > 0.6f) k = std::max(0, k - 1);
      int alpha = (int)(150 + 105 * t);
      if (edge < 0.25f && t > 0.3f) k = 4;   // bright cutting edge on the outer rim
      c.set(x, y, withA(kSteelFx[k], alpha));
    }
}

void puff(Canvas& c, float cx, float cy, float r, const Ramp& R, int alpha, uint32_t seed) {
  for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++)
    for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++) {
      float dx = (x + 0.5f - cx) / r, dy = (y + 0.5f - cy) / r;
      float d = dx * dx + dy * dy;
      float edge = 1.0f + (hashf(x, y, seed) - 0.5f) * 0.3f;
      if (d > edge) continue;
      int k = lightIndex(lightAt(dx * 0.85f, dy * 0.85f), x, y, 0.15f);
      c.set(x, y, withA(R[k], alpha));
    }
}

// ---------------------------------------------------------------- (P2a RIG-ARMS) the family swing effects
// STYLE_P2 6 and 12: each melee family its own effect, painted facing right (the renderer turns the arcs, the glint and
// the streak a quarter for the down and up facings and mirrors them for the left), in the palette's named colours,
// opaque where the colour is named so it lands exactly (the Steel Arc's rule), with no gradients: a rim of core, a body
// of edge, one half-clear px inside. Every painter draws for its kFxInfo size and scales with the cell it is given, so a
// bigger copy (the finisher's 1.2x: fxSpriteSized) is painted natively at one pixel size, never stretched.
namespace {

const uint32_t kHeavyCore = rgba(255, 242, 208), kHeavyEdge = rgba(200, 168, 120);   // Heavy Arc #FFF2D0 / #C8A878
const uint32_t kSteelCore = rgba(244, 241, 230), kSteelEdge = rgba(184, 196, 216);   // Steel Arc #F4F1E6 / #B8C4D8
const uint32_t kChipWood = rgba(176, 122, 74), kChipBone = rgba(230, 220, 192);       // Chip #B07A4A / #E6DCC0
const uint32_t kSmashDust = rgba(200, 180, 150), kSpark = rgba(255, 216, 122);        // Smash Dust #C8B496, sparks #FFD87A
const uint32_t kGlintCore = rgba(255, 255, 255), kGlintHalo = rgba(255, 233, 168);    // Glint #FFFFFF / #FFE9A8
const uint32_t kThrustHead = rgba(232, 240, 255), kThrustTail = rgba(159, 180, 216);  // Thrust Streak #E8F0FF -> #9FB4D8
const uint32_t kSwish = rgba(216, 205, 184);                                           // Swish #D8CDB8 (drawn at 50 %)
const uint32_t kPunchCore = rgba(255, 240, 192), kPunchRim = rgba(240, 138, 60);      // Punch Star #FFF0C0 / #F08A3C
const uint32_t kTwang = rgba(232, 224, 200);                                           // Twang #E8E0C8

// a crescent on an ellipse round (cx, cy) between the angles a0 .. a1 (degrees, 0 = right, + = clockwise down the
// screen): wTail at a0 to wHead at a1 (moon: thin at both ends, wHead at the middle); the outer rim 1 px of core, the
// body edge, the innermost px half clear (a soft inner edge, no gradient)
void crescent(Canvas& c, float cx, float cy, float rx, float ry, float a0, float a1, float wTail, float wHead, bool moon, uint32_t core, uint32_t edge) {
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      const float a = std::atan2(dy / ry, dx / rx) * 57.29578f;
      if (a < a0 || a > a1) continue;
      const float t = (a - a0) / std::max(0.01f, a1 - a0);
      const float w = moon ? 1.0f + (wHead - 1.0f) * std::sin(t * 3.14159265f) : wTail + (wHead - wTail) * t;
      const float r = std::sqrt((dx / rx) * (dx / rx) + (dy / ry) * (dy / ry));
      if (r <= 0.001f) continue;
      const float d = (1.0f - r) * std::sqrt(dx * dx + dy * dy) / r;   // px in from the rim
      if (d < 0 || d >= w) continue;
      if (d < 1.0f) c.set(x, y, core);
      else if (d < w - 1.0f || w < 2.5f) c.set(x, y, edge);
      else c.set(x, y, withA(edge, 128));
    }
}
// a trailing ghost of the blade along the radius at angle a (from r0 to r1 of the ellipse): a wedge widening to 3 px at
// its outer end, half clear, its leading (clockwise) side one px of core: the afterimage of a heavy blade
void ghostBlade(Canvas& c, float cx, float cy, float rx, float ry, float a, float r0, float r1, uint32_t core, uint32_t edge, float k) {
  const float ca = std::cos(a / 57.29578f), sa = std::sin(a / 57.29578f);
  const int n = (int)std::ceil((r1 - r0) * std::max(rx, ry));
  for (int i = 0; i <= n; i++) {
    const float t = (float)i / std::max(1, n);
    const float r = r0 + (r1 - r0) * t;
    const float x = cx + ca * rx * r, y = cy + sa * ry * r;
    const int w = 1 + (int)std::lround(t * 2.0f * k);
    for (int j = 0; j <= w; j++) {   // across the blade, trailing side (counter-clockwise) first
      const int px = (int)std::floor(x + sa * (j - w)), py = (int)std::floor(y - ca * (j - w));
      if (c.get(px, py) >> 24) continue;
      c.set(px, py, j == w ? withA(core, 190) : withA(edge, 130));
    }
  }
}
void star(Canvas& c, int x, int y, int arm, uint32_t core, uint32_t halo) {   // a 4-point glint
  c.set(x, y, core);
  for (int k = 1; k <= arm; k++) {
    const uint32_t col = k < arm ? core : halo;
    c.set(x + k, y, col); c.set(x - k, y, col);
    if (k < arm) { c.set(x, y + k, col); c.set(x, y - k, col); }
  }
}

void paintFamilyFx(Canvas& c, Fx f, int frame, float k) {
  switch (f) {
    case Fx::HeavyArc: {   // greatsword: a wide heavy crescent (48 x 40, 5 frames) with a trailing ghost blade
      // the blade's path seen side-on: an ellipse round the body (the cell's left-middle), wider than it is tall
      const float cx = 4.0f * k, cy = 20.0f * k, rx = 41.0f * k, ry = 19.0f * k;
      struct S { float a0, a1, wt, wh; bool moon; float ghost; };
      static const S kS[5] = {{-92, -60, 2, 5, false, -999}, {-92, -14, 2, 8, false, -36}, {-82, 36, 2, 10, true, 4},
                              {-30, 66, 1, 7, false, 44}, {26, 74, 1, 3, false, -999}};
      const S& s = kS[frame % 5];
      if (s.ghost > -900) ghostBlade(c, cx, cy, rx, ry, s.ghost, 0.40f, 0.96f, kHeavyCore, kHeavyEdge, k);
      crescent(c, cx, cy, rx, ry, s.a0, s.a1, s.wt * k, s.wh * k, s.moon, kHeavyCore, kHeavyEdge);
      if (frame % 5 == 1 || frame % 5 == 2) {   // the glint riding the head of the cut
        const float a = s.a1 / 57.29578f;
        star(c, (int)std::floor(cx + std::cos(a) * (rx - 1)), (int)std::floor(cy + std::sin(a) * (ry - 1)), 2, kGlintCore, kGlintHalo);
      }
      break;
    }
    case Fx::ChopArc: {   // axe: a short, thick, steep arc (28 x 28, 4 frames) biting down; chips fly on the last frames
      const float cx = 0.0f, cy = 13.0f * k, R = 14.0f * k;
      struct S { float a0, a1, wt, wh; bool moon; };
      static const S kS[4] = {{-78, -46, 2, 5, false}, {-78, 12, 2, 6, false}, {-48, 66, 2, 6, true}, {28, 70, 1, 3, false}};
      const S& s = kS[frame & 3];
      crescent(c, cx, cy, R, R, s.a0, s.a1, s.wt * k, s.wh * k, s.moon, kSteelCore, kSteelEdge);
      if ((frame & 3) >= 2) {   // chips of wood and bone thrown out past the bite
        const int f2 = (frame & 3) - 2;
        static const int8_t chip[4][3] = {{11, 22, 0}, {15, 18, 1}, {8, 25, 1}, {17, 24, 0}};
        for (int i = 0; i < 4; i++) {
          const int x = (int)std::lround((chip[i][0] + f2 * (2 + i)) * k), y = (int)std::lround((chip[i][1] - f2 * (i & 1 ? 2 : 0) + f2) * k);
          const uint32_t col = chip[i][2] ? kChipBone : kChipWood;
          c.set(x, y, col);
          c.set(x + 1, y, chip[i][2] ? col : mix(col, kChipBone, 0.35f));   // a 2 px chunk, its lit end lighter
          if (i == 0 && f2 == 0) c.set(x, y + 1, mix(col, kInk, 0.3f));
        }
      }
      break;
    }
    case Fx::SmashRing: {   // mace / hammer: the impact ring on the ground (32 x 16, 5 frames), dust, sparks
      const float cx = 16.0f * k, cy = 9.0f * k;
      static const float rr[5] = {4.0f, 8.0f, 11.5f, 13.5f, 15.0f};
      static const float ww[5] = {2.5f, 2.6f, 2.2f, 1.6f, 1.0f};
      const int fr = frame % 5;
      const float rx = rr[fr] * k, ry = rr[fr] * 0.45f * k, w = ww[fr] * k;
      for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++) {
          const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
          const float r = std::sqrt((dx / rx) * (dx / rx) + (dy / ry) * (dy / ry));
          const float d = (1.0f - r) * rx;   // px in from the rim, measured across the long axis
          if (d < -0.5f || d > w) continue;
          if (fr == 4 && ((x / 2 + y) & 1)) continue;          // the last ring breaks up
          const bool front = dy > 0;                            // the near arc a shade brighter (it faces the eye)
          c.set(x, y, d < 1.0f ? (front ? kWhite : kSmashDust) : withA(kSmashDust, front ? 220 : 170));
        }
      if (fr == 0) {   // the flash where the head lands
        star(c, (int)cx, (int)cy, std::max(2, (int)std::lround(2 * k)), kWhite, kSpark);
      }
      // dust puffs rising either side
      if (fr >= 1) {
        static const int8_t puffs[3][2] = {{-1, 0}, {1, 0}, {0, -1}};
        for (int i = 0; i < 3; i++) {
          const float px = cx + puffs[i][0] * (rx + 1.0f), py = cy - (fr - 1) * 1.2f * k + puffs[i][1] * 3.0f * k - 1.0f;
          const float pr = (1.2f + fr * 0.35f) * k;
          for (int y = (int)(py - pr); y <= (int)(py + pr); y++)
            for (int x = (int)(px - pr); x <= (int)(px + pr); x++) {
              const float ex = x + 0.5f - px, ey = y + 0.5f - py;
              if (ex * ex + ey * ey > pr * pr) continue;
              if (c.get(x, y) >> 24) continue;
              c.set(x, y, withA(ey < 0 ? kWhite : kSmashDust, fr >= 4 ? 120 : 190));
            }
        }
      }
      // three sparks off the metal, flying up and out
      if (fr <= 2) {
        static const int8_t sp[3][2] = {{-3, -2}, {3, -3}, {1, -4}};
        for (int i = 0; i < 3; i++) {
          const int x = (int)std::lround(cx + sp[i][0] * (1 + fr) * k), y = (int)std::lround(cy + sp[i][1] * (1 + fr) * 0.7f * k - 1);
          c.set(x, y, kSpark);
          if (fr < 2) c.set(x, y + 1, withA(kSpark, 150));
        }
      }
      break;
    }
    case Fx::StabGlint: {   // dagger: the Glint at the point (12 x 6, 2 frames), long along the stab
      const int cx = (int)std::lround(7 * k), cy = (int)std::lround(3 * k);
      if ((frame & 1) == 0) {
        star(c, cx, cy, std::max(2, (int)std::lround(2 * k)), kGlintCore, kGlintHalo);
      } else {
        const int arm = (int)std::lround(5 * k);
        for (int x = -arm; x <= arm; x++) c.set(cx + x, cy, std::abs(x) <= arm / 2 ? kGlintCore : kGlintHalo);
        for (int y = -2; y <= 2; y++) if (y) c.set(cx, cy + y, std::abs(y) == 1 ? kGlintCore : kGlintHalo);
        c.set(cx + 1, cy - 1, withA(kGlintHalo, 170)); c.set(cx - 1, cy + 1, withA(kGlintHalo, 170));
      }
      break;
    }
    case Fx::ThrustStreak: {   // spear: the Thrust Streak along the aim (24 x 5, 3 frames), bright at the point
      const int cy = c.h / 2, W = c.w;
      const int fr = frame % 3;
      const int head = fr == 0 ? W - 1 : W - 1, len = fr == 0 ? W / 3 : fr == 1 ? W - 1 : W - 4;
      for (int i = 0; i < len; i++) {
        const int x = head - i;
        const float t = (float)i / std::max(1, len - 1);   // 0 at the point .. 1 at the tail
        if (fr == 2 && (i % 4) == 3) continue;              // breaking up as it fades
        const uint32_t col = t < 0.34f ? kThrustHead : t < 0.67f ? mix(kThrustHead, kThrustTail, 0.5f) : kThrustTail;
        c.set(x, cy, col);
        const int thick = t < 0.2f ? 2 : t < 0.55f ? 1 : 0;   // tapering from 3 px at the point to 1 at the tail
        for (int j = 1; j <= thick && cy + j < c.h; j++) {
          c.set(x, cy - j, withA(col, fr == 2 ? 120 : 200));
          c.set(x, cy + j, withA(kThrustTail, fr == 2 ? 120 : 200));
        }
      }
      if (fr == 1) c.set(head, cy, kWhite);
      break;
    }
    case Fx::BluntSwish: {   // staff / walking stick: the Swish (28 x 28, 3 frames): three speed lines, no glint
      const float cx = 0.0f, cy = 14.0f * k, R = 13.5f * k;
      struct S { float a0, a1; };
      static const S kS[3] = {{-66, -18}, {-66, 42}, {-6, 64}};
      const S& s = kS[frame % 3];
      for (int line = 0; line < 3; line++) {
        const float r = R - line * 2.2f * k;
        const float b0 = s.a0 + line * 9.0f, b1 = s.a1 - line * 6.0f;   // the inner lines shorter: a sweep, not a ring
        for (int y = 0; y < c.h; y++)
          for (int x = 0; x < c.w; x++) {
            const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            const float a = std::atan2(dy, dx) * 57.29578f;
            if (a < b0 || a > b1) continue;
            const float d = std::sqrt(dx * dx + dy * dy) - r;
            if (d > -0.5f && d < (line == 0 ? 1.5f : 0.5f)) c.set(x, y, withA(kSwish, 128));
          }
      }
      break;
    }
    case Fx::PunchStar: {   // fists: the Punch Star (9 x 9, 2 frames)
      const int cx = c.w / 2, cy = c.h / 2;
      if ((frame & 1) == 0) {
        // a tight burst: a 3 x 3 core, four short points
        for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) c.set(cx + x, cy + y, (x == 0 || y == 0) ? kPunchCore : kPunchRim);
        c.set(cx + 2, cy, kPunchRim); c.set(cx - 2, cy, kPunchRim); c.set(cx, cy + 2, kPunchRim); c.set(cx, cy - 2, kPunchRim);
      } else {
        // the full star: eight points, the core opening out
        const int L = std::max(3, (int)std::lround(4 * k));
        for (int i = 1; i <= L; i++) {
          const uint32_t col = i <= L / 2 ? kPunchCore : kPunchRim;
          c.set(cx + i, cy, col); c.set(cx - i, cy, col); c.set(cx, cy + i, col); c.set(cx, cy - i, col);
          if (i <= L - 1) {
            const uint32_t d = i <= 1 ? kPunchCore : kPunchRim;
            c.set(cx + i, cy + i, d); c.set(cx - i, cy - i, d); c.set(cx + i, cy - i, d); c.set(cx - i, cy + i, d);
          }
        }
        c.set(cx, cy, kWhite);
      }
      break;
    }
    case Fx::Twang: {   // bow release: the Twang at the bow (8 x 8, 2 frames): the string's quiver as two / three arcs
      const int arcs = (frame & 1) ? 3 : 2;
      for (int a = 0; a < arcs; a++) {
        const int x0 = (int)std::lround((2 + a * 2 + (frame & 1)) * k);
        const int h = (int)std::lround((2 + a) * k);
        for (int y = -h; y <= h; y++) {
          const int x = x0 + (std::abs(y) == h ? -1 : 0);
          c.set(x, c.h / 2 + y, withA(kTwang, a == arcs - 1 ? 150 : 230));
        }
      }
      break;
    }
    default: break;
  }
}

}  // namespace

void paintFx(Canvas& c, Fx f, int frame) {
  // (P2a style slice) the sword's Steel Arc (STYLE_P2 6, 12: 32 x 32, 4 frames), painted with the v2 rig (rig_body.cpp)
  if (f == Fx::SwingArc) { rig2::steelArc(c, frame); return; }
  if ((int)f > (int)Fx::SwingArc && (int)f < (int)Fx::COUNT) { paintFamilyFx(c, f, frame, 1.0f); return; }
  switch (f) {
    case Fx::Slash: case Fx::SlashDown: case Fx::SlashUp: {
      float base = f == Fx::Slash ? 0.0f : (f == Fx::SlashDown ? PI * 0.5f : -PI * 0.5f);
      static const float tails[4] = {-1.9f, -1.7f, -0.6f, 0.5f};
      static const float heads[4] = {-1.1f, 0.1f, 1.1f, 1.5f};
      static const float widths[4] = {3.5f, 6.5f, 6.0f, 3.5f};
      slashArc(c, 10.0f, 12.0f, 11.0f, tails[frame], heads[frame], widths[frame], base);
      if (frame == 1 || frame == 2) {   // glint at the leading tip
        float a = heads[frame] + base;
        int gx = (int)(10.0f + std::cos(a) * 9.5f), gy = (int)(12.0f + std::sin(a) * 9.5f);
        c.set(gx, gy, kWhite); c.set(gx + 1, gy, withA(kWhite, 180)); c.set(gx - 1, gy, withA(kWhite, 180));
        c.set(gx, gy + 1, withA(kWhite, 180)); c.set(gx, gy - 1, withA(kWhite, 180));
      }
      break;
    }
    case Fx::Arrow:
      hline(c, 2, 9, 1, kWood[3]);
      c.set(11, 1, kIron[4]); c.set(10, 0, kIron[3]); c.set(10, 1, kIron[3]); c.set(10, 2, kIron[2]); c.set(9, 1, kIron[2]);
      c.set(0, 0, kCloth[4]); c.set(1, 0, kRed[3]); c.set(0, 2, kCloth[3]); c.set(1, 2, kRed[2]); c.set(2, 0, kRed[3]); c.set(2, 2, kRed[1]);
      c.set(0, 1, kWood[2]);
      break;
    case Fx::ArrowDown:
      vline(c, 1, 2, 9, kWood[3]);
      c.set(1, 11, kIron[4]); c.set(0, 10, kIron[3]); c.set(1, 10, kIron[3]); c.set(2, 10, kIron[2]); c.set(1, 9, kIron[2]);
      c.set(0, 0, kCloth[4]); c.set(0, 1, kRed[3]); c.set(2, 0, kCloth[3]); c.set(2, 1, kRed[2]); c.set(0, 2, kRed[3]); c.set(2, 2, kRed[1]);
      c.set(1, 0, kWood[2]);
      break;
    case Fx::Fireball: {
      // trailing flames to the left, bright core on the right
      for (int i = 0; i < 6; i++) {
        float t = i / 5.0f;
        float x = 7.5f - t * 6.0f, y = 6.0f + std::sin(frame * 1.6f + i * 1.3f) * t * 1.6f;
        float r = 3.6f - t * 2.6f;
        for (int yy = (int)(y - r); yy <= (int)(y + r); yy++)
          for (int xx = (int)(x - r); xx <= (int)(x + r); xx++) {
            float d = std::hypot(xx + 0.5f - x, yy + 0.5f - y) / r;
            if (d > 1) continue;
            int k = d < 0.4f ? 3 : 2;
            if (t > 0.5f) k--;
            c.set(xx, yy, withA(kFire[k], (int)(255 - t * 90)));
          }
      }
      ball(c, 8.0f, 6.0f, 3.4f, 3.4f, ramp5(kFire[1], kFire[2], kFire[3], kFire[4], kWhite), 0.2f, 1);
      c.set(7, 4 + (frame & 1), kWhite);
      break;
    }
    case Fx::Explosion: {
      float cx = 16, cy = 17;
      static const float fireR[6] = {5, 10, 13, 11, 0, 0};
      static const float smokeR[6] = {0, 0, 9, 12, 13, 12};
      static const int smokeA[6] = {0, 0, 200, 210, 160, 90};
      const Ramp Smoke = ramp5(rgba(40, 32, 44), rgba(70, 60, 70), rgba(104, 94, 100), rgba(140, 130, 132), rgba(176, 168, 166));
      if (smokeR[frame] > 0)
        for (int i = 0; i < 6; i++) {
          float a = i * TAU / 6 + frame * 0.3f;
          float d = smokeR[frame] * 0.55f;
          puff(c, cx + std::cos(a) * d, cy + std::sin(a) * d * 0.8f - frame, smokeR[frame] * 0.5f, Smoke, smokeA[frame], 7 + i);
        }
      if (fireR[frame] > 0) {
        float r = fireR[frame];
        for (int y = 0; y < c.h; y++)
          for (int x = 0; x < c.w; x++) {
            float d = std::hypot(x + 0.5f - cx, (y + 0.5f - cy) * 1.1f) / r;
            float n = (vnoise(x * 0.35f, y * 0.35f, 5 + frame) - 0.5f) * 0.5f;
            d += n;
            if (d > 1) continue;
            int k = d < 0.3f ? 4 : (d < 0.55f ? 3 : (d < 0.8f ? 2 : 1));
            if (frame == 3) k = std::max(0, k - 1);
            c.set(x, y, kFire[k]);
          }
      }
      if (frame == 0) for (int a = 0; a < 8; a++) {   // flash rays
        float ang = a * TAU / 8;
        for (int k = 5; k < 9; k++) c.set((int)(cx + std::cos(ang) * k), (int)(cy + std::sin(ang) * k), withA(kFire[4], 220 - k * 15));
      }
      if (frame >= 2 && frame <= 4)   // flying embers
        for (int i = 0; i < 8; i++) {
          float ang = hashf(i, 0, 3) * TAU, dist = 8 + frame * 3.0f + hashf(i, 1, 3) * 3;
          int x = (int)(cx + std::cos(ang) * dist), y = (int)(cy + std::sin(ang) * dist + frame);
          c.set(x, y, kFire[frame == 4 ? 2 : 3]);
        }
      break;
    }
    case Fx::Sparkle: {
      static const int sz[4] = {1, 3, 2, 1};
      static const int al[4] = {200, 255, 230, 140};
      int s = sz[frame];
      uint32_t gold = withA(kGold[4], al[frame]), white = withA(kWhite, al[frame]);
      for (int k = 1; k <= s; k++) { c.set(4 + k, 4, gold); c.set(3 - k + 1 - 1, 4, gold); c.set(4, 4 + k, gold); c.set(4, 4 - k, gold); }
      c.set(4, 4, white);
      if (frame == 1) { c.set(3, 3, withA(kGold[3], 160)); c.set(5, 5, withA(kGold[3], 160)); c.set(5, 3, withA(kGold[3], 160)); c.set(3, 5, withA(kGold[3], 160)); }
      if (frame == 3) { c.set(1, 6, withA(kGold[4], 120)); c.set(6, 1, withA(kGold[4], 100)); }
      break;
    }
    case Fx::Blood: {
      static const float spread[4] = {1.0f, 2.2f, 3.2f, 3.6f};
      static const float fall[4] = {0, 0.5f, 1.5f, 3.0f};
      for (int i = 0; i < 6; i++) {
        float ang = -PI * 0.15f - hashf(i, 0, 11) * PI * 0.7f;
        float d = spread[frame] * (0.6f + hashf(i, 1, 11) * 0.6f);
        int x = (int)(4 + std::cos(ang) * d), y = (int)(4 + std::sin(ang) * d + fall[frame]);
        uint32_t col = frame < 3 ? kRed[(i & 1) ? 2 : 1] : withA(kRed[1], 170);
        c.set(x, y, col);
        if (frame < 2) c.set(x, y + 1, kRed[0]);
      }
      if (frame == 0) { c.set(3, 4, kRed[3]); c.set(4, 4, kRed[2]); c.set(4, 3, kRed[2]); }
      if (frame >= 2) { c.set(3, 7, withA(kRed[1], 200)); c.set(4, 7, withA(kRed[0], 200)); }
      break;
    }
    case Fx::Dust: {
      const Ramp D = ramp5(rgba(120, 104, 90), rgba(156, 140, 118), rgba(190, 176, 150), rgba(214, 204, 180), rgba(236, 230, 210));
      static const float r[4] = {1.6f, 2.3f, 2.8f, 3.0f};
      static const int a[4] = {230, 200, 150, 80};
      puff(c, 3.0f, 5.5f - frame * 0.3f, r[frame] * 0.8f, D, a[frame], 3);
      puff(c, 5.2f, 5.0f - frame * 0.5f, r[frame] * 0.7f, D, a[frame], 4);
      break;
    }
    case Fx::Frost: {
      // spinning ice shard: diamond core + orbiting flakes
      poly(c, {{11.0f, 6.0f}, {6.0f, 3.5f}, {1.5f, 6.0f}, {6.0f, 8.5f}}, kCrystal[3]);
      poly(c, {{11.0f, 6.0f}, {6.0f, 3.5f}, {6.0f, 6.0f}}, kCrystal[4]);
      poly(c, {{6.0f, 6.0f}, {6.0f, 8.5f}, {11.0f, 6.0f}}, kCrystal[2]);
      c.set(9, 5, kWhite);
      for (int i = 0; i < 3; i++) {
        float ang = frame * 0.8f + i * TAU / 3;
        int x = (int)(5 + std::cos(ang) * 4.5f), y = (int)(6 + std::sin(ang) * 4.5f);
        c.set(x, y, withA(kSnow[4], 230));
        if (i == frame % 3) { c.set(x + 1, y, withA(kSnow[3], 160)); c.set(x, y + 1, withA(kSnow[3], 160)); }
      }
      break;
    }
    case Fx::Heal: {
      const Ramp G = ramp5(rgba(30, 110, 60), rgba(60, 170, 80), rgba(110, 220, 110), rgba(170, 250, 160), rgba(236, 255, 220));
      static const int ys[3] = {11, 7, 3};
      for (int i = 0; i < 3; i++) {
        int x = 3 + i * 5, y = ys[(i + frame) % 3] + (frame & 1);
        int alpha = 255 - ((i + frame) % 3) * 70;
        hline(c, x - 1, x + 1, y, withA(G[3], alpha)); vline(c, x, y - 1, y + 1, withA(G[3], alpha));
        c.set(x, y, withA(G[4], alpha));
        c.set(x + 1, y + 1, withA(G[1], alpha));
      }
      for (int i = 0; i < 2; i++) c.set(2 + i * 9 + frame, 14 - frame * 3 + i * 2, withA(G[4], 180));
      break;
    }
    default: break;
  }
}

struct FxInfo { uint8_t w, h, frames; };
const FxInfo kFxInfo[(int)Fx::COUNT] = {
  {24, 24, 4}, {24, 24, 4}, {24, 24, 4}, {12, 3, 1}, {3, 12, 1}, {12, 12, 4}, {32, 32, 6}, {8, 8, 4}, {8, 8, 4}, {8, 8, 4}, {12, 12, 4}, {16, 16, 4},
  // the family effects at their STYLE_P2 12 sizes and frame counts: SwingArc (the style slice's Steel Arc), HeavyArc,
  // ChopArc, SmashRing, StabGlint, ThrustStreak, BluntSwish, PunchStar, Twang
  {32, 32, 4}, {48, 40, 5}, {28, 28, 4}, {32, 16, 5}, {12, 6, 2}, {24, 5, 3}, {28, 28, 3}, {9, 9, 2}, {8, 8, 2},
};

}  // namespace

int fxW(Fx f) { return (int)f < (int)Fx::COUNT ? kFxInfo[(int)f].w : 8; }
int fxH(Fx f) { return (int)f < (int)Fx::COUNT ? kFxInfo[(int)f].h : 8; }
int fxFrames(Fx f) { return (int)f < (int)Fx::COUNT ? kFxInfo[(int)f].frames : 1; }

Canvas fxSprite(Fx f) {
  const int w = fxW(f), h = fxH(f), n = fxFrames(f);
  Canvas sheet(w * n, h);
  for (int i = 0; i < n; i++) {
    Canvas cell(w, h);
    paintFx(cell, f, i);
    place(sheet, cell, i, 0);
  }
  return sheet;
}
Canvas fxSpriteSized(Fx f, int w, int h) {
  if ((int)f < (int)Fx::SwingArc || (int)f >= (int)Fx::COUNT || w <= 0 || h <= 0) return Canvas();
  const int n = fxFrames(f);
  Canvas sheet(w * n, h);
  for (int i = 0; i < n; i++) {
    Canvas cell(w, h);
    if (f == Fx::SwingArc) rig2::steelArc(cell, i);   // the arc's radius and widths follow the cell's height
    else paintFamilyFx(cell, f, i, (float)h / (float)std::max(1, fxH(f)));   // (RIG-ARMS) every family effect, the same rule
    place(sheet, cell, i, 0);
  }
  return sheet;
}

// M6 Steel (BEASTS lane): an elite's aura. The ground part is a flat ellipse of light round the feet (half as tall as it
// is wide: the 3/4 camera's foreshortening): a soft rim brightest just inside its edge, the near (lower) arc a touch
// brighter as it faces the camera, a faint fill under the feet, in four dithered alpha steps (pixel-art light, no
// smooth gradient); round the rim, glints that walk round the ellipse; above it motes of light rising and fading. Six
// frames loop (the pulse and the glints repeat every six, each mote rises over the six). The renderer adds it.
int auraH(int w) { return w / 2 + 8; }
Canvas auraSprite(uint32_t color, int w) {
  w = std::max(12, w);
  const int h = auraH(w);
  Canvas c(w * AURA_FRAMES, h);
  const uint32_t base = opaque(color), hot = mix(base, kWhite, 0.45f), deep = shade(base, 0.8f);
  const float rx = w * 0.5f - 1.0f, ry = w * 0.25f - 0.5f, cx = w * 0.5f, cy = h - w * 0.25f - 1.0f;
  static const int kA[4] = {0, 70, 140, 215};
  auto put = [&](int f, int x, int y, uint32_t col) { if (x >= 0 && x < w) c.set(f * w + x, y, col); };
  for (int f = 0; f < AURA_FRAMES; f++) {
    const float pulse = 0.86f + 0.14f * std::cos(f * TAU / AURA_FRAMES);
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry, d = std::sqrt(dx * dx + dy * dy);
        if (d > 1.0f) continue;
        const float ring = std::exp(-((d - 0.80f) / 0.15f) * ((d - 0.80f) / 0.15f));
        float a = (ring * 0.95f + 0.06f * (1.0f - d)) * pulse;
        if (dy > 0) a *= 1.0f + 0.25f * dy;                       // the near arc faces the camera
        const float q = a * 3.0f + (bayer(x, y) - 0.5f) * 0.9f;   // four dithered steps
        const int k = std::clamp((int)std::floor(q + 0.5f), 0, 3);
        if (!k) continue;
        put(f, x, y, withA(k == 3 ? hot : k == 2 ? base : deep, kA[k]));
      }
    // glints walking round the rim
    for (int g = 0; g < 4; g++) {
      const float an = (g * 0.25f + f / (float)AURA_FRAMES * 0.25f) * TAU;
      const int x = (int)std::floor(cx + std::cos(an) * rx * 0.82f), y = (int)std::floor(cy + std::sin(an) * ry * 0.82f);
      put(f, x, y, withA(mix(base, kWhite, 0.75f), 230));
      if (std::sin(an) > 0) { put(f, x - 1, y, withA(hot, 150)); put(f, x + 1, y, withA(hot, 150)); }
    }
    // motes rising off the rim, fading as they go (a short bright streak at the head of each)
    const float rise = h - w * 0.25f - 3.0f;
    for (int m = 0; m < 5; m++) {
      const float ph = std::fmod((f + m * 1.37f) / AURA_FRAMES, 1.0f);
      const float an = m * 2.39f + 0.6f;
      const int x = (int)std::floor(cx + std::cos(an) * rx * 0.72f + std::sin(ph * TAU + m) * 0.8f);
      const int y = (int)std::floor(cy + std::sin(an) * ry * 0.72f - ph * rise);
      const int a = (int)(235 * (1.0f - ph) * (ph < 0.12f ? ph / 0.12f : 1.0f));
      if (a < 20) continue;
      put(f, x, y, withA(mix(base, kWhite, 0.6f), a));
      put(f, x, y + 1, withA(base, a * 2 / 3));
      put(f, x + 1, y + 1, withA(base, a / 3));
    }
  }
  return c;
}

}  // namespace art
