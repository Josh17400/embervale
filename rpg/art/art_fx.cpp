// EMBERVALE art: effects. See rpg/art.h for the contract and rpg/art/art_internal.h for the shared helpers.
#include "rpg/art/art_internal.h"

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

void paintFx(Canvas& c, Fx f, int frame) {
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

}  // namespace art
