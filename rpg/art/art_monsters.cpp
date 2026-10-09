// EMBERVALE art: monsters. See rpg/art.h for the contract and rpg/art/art_internal.h for the shared helpers.
#include "rpg/art/art_internal.h"

namespace art {

// =====================================================================================================
// (M6 BEASTS) anchors: while a variant look is painted, every painter reports where its head, eye and body are in the
// frame it just drew (and which ramps its hide is made of), so the overlays sit on the head in every pose (the wind-up,
// the hurt recoil, the body lying dead) and the tint recolours the hide by ramp. Recording never touches a pixel: the
// classic sheets stay identical (art_hash "monsters").
// =====================================================================================================
namespace {
struct Anchors {
  bool head = false, eye = false, body = false;
  float hx = 0, hy = 0, hr = 3;           // the head's centre and radius
  float ex = 0, ey = 0;                   // the (near) eye
  float bx = 0, by = 0, brx = 4, bry = 3; // the torso's ellipse
};
struct Recorder {
  Anchors a;
  Ramp body[4];        // the hide's ramps (the tint maps these step by step)
  int nBody = 0;
  Canvas* glow = nullptr;   // emissive pixels (eyes, ember cracks, crystal glints, a golem's rune)
};
thread_local Recorder* g_rec = nullptr;
inline void recHead(Vec2 c, float r) { if (g_rec) { g_rec->a.head = true; g_rec->a.hx = c.x; g_rec->a.hy = c.y; g_rec->a.hr = r; } }
inline void recEye(Vec2 e) { if (g_rec) { g_rec->a.eye = true; g_rec->a.ex = e.x; g_rec->a.ey = e.y; } }
inline void recBody(float x, float y, float rx, float ry) {
  if (g_rec) { g_rec->a.body = true; g_rec->a.bx = x; g_rec->a.by = y; g_rec->a.brx = rx; g_rec->a.bry = ry; }
}
inline void recRamp(const Ramp& r) {
  if (!g_rec || g_rec->nBody >= 4) return;
  for (int i = 0; i < g_rec->nBody; i++) if (g_rec->body[i].c[2] == r.c[2]) return;
  g_rec->body[g_rec->nBody++] = r;
}
// an emissive pixel: painted, and again on the glow sheet the view adds after the night's darkness
inline void glowSet(Canvas& c, int x, int y, uint32_t col) {
  c.set(x, y, col);
  if (g_rec && g_rec->glow) g_rec->glow->set(x, y, col);
}
}  // namespace

// =====================================================================================================
// 4. monsters
// =====================================================================================================
namespace {

// ------------------------------------------------------------------ quadrupeds (wolf, boar, bear)
struct Quad {
  Ramp fur, belly;
  uint32_t eye, nose;
  float bodyLen, bodyR, legLen, legW, headR, snout;
  int ears;      // 0 pointy, 1 round
  int tail;      // 0 bushy, 1 tuft, 2 stub
  bool tusks = false, mane = false, hump = false, saddle = false;
  uint32_t seed = 1;
  // (M3c LIFE) a sloping back (hyena: the shoulders stand `slope` px above the haunch), spots, a dark muzzle
  float slope = 0;
  uint32_t spot = 0;
  bool darkMuzzle = false;
};

void quadruped(Canvas& c, const Quad& q, int frame) {
  const int H = c.h;
  const float ground = H - 2.0f;
  const bool dead = frame == 7;
  const float cycle = (float)(frame & 3) * (PI * 0.5f);
  float bx = c.w * 0.44f, by = ground - q.legLen - q.bodyR * 0.55f;
  float headDX = 0, headDY = 0, jaw = 0;
  bool eyeShut = false;
  if (frame < 4) by += (frame & 1) ? -1.0f : 0.0f;
  if (frame == 4) { bx -= 2; by += 1; headDY = 2; headDX = -1; }
  if (frame == 5) { bx += 3; headDX = 2; jaw = 2.5f; }
  if (frame == 6) { bx -= 1; headDY = -2; eyeShut = true; jaw = 1; }
  if (dead) { by = ground - q.bodyR * 0.75f; headDY = q.bodyR * 0.5f + 1; headDX = 1; eyeShut = true; jaw = 1; }

  const Ramp& F = q.fur;
  const float hump = q.hump ? 2.0f : 0.0f;
  const float front = bx + q.bodyLen * 0.5f, back = bx - q.bodyLen * 0.5f;
  struct LegSpec { float ax, phase; bool nearSide, frontLeg; };
  const LegSpec legs[4] = {{back + 1.5f, cycle, false, false}, {front - 1.0f, cycle + PI, false, true},
                           {back + 0.5f, cycle + PI, true, false}, {front - 2.0f, cycle, true, true}};
  auto drawLeg = [&](Canvas& t, const LegSpec& l) {
    int bias = l.nearSide ? 0 : -1;
    Vec2 hip{l.ax, by + q.bodyR * 0.3f + (l.frontLeg ? -q.slope * 0.5f : q.slope * 0.7f)};
    if (dead) {
      Vec2 foot{l.ax + q.legLen * 0.9f + (l.nearSide ? 1 : -1), ground - 1 - (l.nearSide ? 0 : 2)};
      capsule(t, hip, foot, q.legW * 0.6f, q.legW * 0.45f, F, bias);
      return;
    }
    float fx = l.ax + std::cos(l.phase) * 2.6f;
    float fy = ground - std::max(0.0f, std::sin(l.phase)) * 2.0f;
    if (frame == 4) fx -= l.frontLeg ? 1 : 2;
    if (frame == 5) fx += l.frontLeg ? 3 : -2;
    if (frame == 4 && l.frontLeg && q.hump) fy -= 3;   // the bear raises a paw
    Vec2 foot{fx, fy};
    Vec2 knee = (hip + foot) * 0.5f + Vec2{l.frontLeg ? -0.6f : 1.2f, 0};
    capsule(t, hip, knee, q.legW * 0.62f, q.legW * 0.5f, F, bias);
    capsule(t, knee, foot, q.legW * 0.5f, q.legW * 0.42f, F, bias);
    int px0 = (int)std::floor(foot.x), py0 = (int)std::floor(foot.y);
    t.set(px0, py0, F[1 + bias]); t.set(px0 + 1, py0, F[1 + bias]);
    if (q.legW >= 3) t.set(px0 - 1, py0, F[1 + bias]);
    if (q.hump || q.tusks) t.set(px0 + 1, py0, q.nose);   // claws / hooves
  };
  // far legs
  drawLeg(c, legs[0]);
  drawLeg(c, legs[1]);
  // tail
  Vec2 tb{back - 0.5f, by - q.bodyR * 0.4f + q.slope * 0.8f};
  float wag = frame < 4 ? std::sin(cycle) : 0.0f;
  layered(c, [&](Canvas& t) {
    if (q.tail == 0) {
      Vec2 tm = tb + Vec2{-3.0f, 1.0f + wag}, tt = tb + Vec2{-6.0f, 4.0f + wag};
      if (dead) { tm = tb + Vec2{-3, 2}; tt = tb + Vec2{-7, 3}; }
      capsule(t, tb, tm, 1.6f, 2.2f, F, 0);
      capsule(t, tm, tt, 2.2f, 1.0f, F, 0);
      px(t, tt + Vec2{-0.5f, 0.2f}, q.belly[3]);
    } else if (q.tail == 1) {
      Vec2 tt = tb + Vec2{-3.0f, 3.0f + wag * 0.5f};
      capsule(t, tb, tt, 0.8f, 0.6f, F, -1);
      px(t, tt, F[0]);
      px(t, tt + Vec2{0, 1}, F[0]);
    } else {
      ball(t, tb.x, tb.y + 1, 1.6f, 1.4f, F);
    }
  }, 0.5f);
  // body: haunch, barrel, shoulders
  furBall(c, back + q.bodyR * 0.75f, by + q.slope * 0.75f, q.bodyR * 0.95f, q.bodyR * 0.9f, F, 0.2f, q.seed);
  furBall(c, bx + 0.5f, by + 0.3f + q.slope * 0.15f, q.bodyLen * 0.5f, q.bodyR * 0.88f, F, 0.2f, q.seed + 1);
  furBall(c, front - q.bodyR * 0.55f, by - hump * 0.5f - q.slope * 0.45f, q.bodyR, q.bodyR + hump * 0.5f + q.slope * 0.3f, F, 0.2f, q.seed + 2);
  // lighter underside: the lowest two rows of the body, and a pale chest
  for (int x = (int)(back + 1); x <= (int)(front + 1); x++) {
    int yb = -1;
    for (int y = H - 1; y >= 0; y--) if (solid(c, x, y) && y < by + q.bodyR + 1.5f + q.slope * 0.8f) { yb = y; break; }
    if (yb > by) { c.set(x, yb, q.belly[1]); c.set(x, yb - 1, q.belly[(x & 1) ? 2 : 1]); }
  }
  if (!q.tusks) ellipse(c, front + 0.5f, by + q.bodyR * 0.25f, q.bodyR * 0.45f, q.bodyR * 0.55f, q.belly[2]);
  // darker saddle along the back (wolf), bristly ridge (boar)
  for (int x = (int)back; x <= (int)front + 1; x++) {
    for (int y = 0; y < H; y++)
      if (solid(c, x, y)) {
        if (q.saddle) for (int k = 1; k <= 2; k++) if (((x + k) & 1) || k == 1) c.set(x, y + k, F[1]);
        if (hash3(x, 0, q.seed) % 3 == 0) c.set(x, y + 1, F[3]);    // fur tufts catching the light
        if (q.mane) { if (x & 1) c.set(x, y - 1, F[1]); c.set(x, y, F[1]); }
        break;
      }
  }
  // near legs
  layered(c, [&](Canvas& t) { drawLeg(t, legs[2]); }, 0.6f);
  layered(c, [&](Canvas& t) { drawLeg(t, legs[3]); }, 0.6f);
  // head (own layer so the jawline and cheek read against the shoulder)
  Vec2 neck{front - 0.5f, by - q.bodyR * 0.45f - hump * 0.6f - q.slope * 0.5f};
  Vec2 hc{front + q.headR * 0.6f + headDX, by - q.bodyR * 0.75f - hump * 0.4f + headDY - q.slope * 0.4f};
  if (q.tusks) hc.y += 2.0f;   // the boar carries its head low
  capsule(c, neck, hc, q.bodyR * 0.72f, q.headR * 0.85f, F, 0);
  layered(c, [&](Canvas& t) {
    furBall(t, hc.x, hc.y, q.headR, q.headR * 0.92f, F, 0.15f, q.seed + 3);
    Vec2 sn0 = hc + Vec2{q.headR * 0.5f, q.headR * 0.25f}, sn1 = sn0 + Vec2{q.snout, q.headR * 0.18f};
    if (jaw > 0) {   // open jaw: lower jaw drops, teeth + red mouth
      Vec2 j1 = sn0 + Vec2{q.snout * 0.85f, jaw + 1.0f};
      capsule(t, sn0 + Vec2{0, 1}, j1, q.headR * 0.4f, q.headR * 0.28f, q.belly, -1);
      for (int k = 0; k < (int)q.snout; k++) px(t, sn0 + Vec2{(float)k + 0.5f, q.headR * 0.42f + 0.6f}, rgba(120, 30, 50));
      for (int k = 1; k < (int)q.snout; k += 2) px(t, sn0 + Vec2{(float)k + 0.5f, q.headR * 0.42f}, kWhite);
    }
    capsule(t, sn0, sn1, q.headR * 0.55f, q.headR * 0.42f, F, 0);
    if (q.darkMuzzle) capsule(t, sn0 + Vec2{q.snout * 0.35f, 0.2f}, sn1, q.headR * 0.45f, q.headR * 0.4f, F, -2);
    // pale muzzle underside
    for (int k = 0; k <= (int)q.snout; k++) px(t, sn0 + Vec2{(float)k, q.headR * 0.45f}, q.belly[jaw > 0 ? 1 : 3]);
    Vec2 nose = sn1 + Vec2{q.headR * 0.3f, -q.headR * 0.2f};
    px(t, nose, q.nose);
    px(t, nose + Vec2{0, 1}, shade(q.nose, 0.8f));
    px(t, nose + Vec2{-1, -0.2f}, mix(q.nose, F[2], 0.5f));
    if (q.tusks) {
      Vec2 t0 = sn1 + Vec2{-1.0f, q.headR * 0.3f};
      px(t, t0, kBone[3]); px(t, t0 + Vec2{0.6f, -1}, kBone[4]); px(t, t0 + Vec2{1.4f, -1.8f}, kBone[4]);
    }
    Vec2 ear = hc + Vec2{-q.headR * 0.3f, -q.headR * 0.9f};
    if (q.ears == 0) {
      poly(t, {ear + Vec2{-1.8f, 1.5f}, ear + Vec2{0.2f, -3.0f}, ear + Vec2{1.6f, 1.2f}}, F[2]);
      px(t, ear + Vec2{0.0f, -0.2f}, F[0]);
      px(t, ear + Vec2{-0.6f, -1.6f}, F[4]);
    } else {
      ball(t, ear.x, ear.y + 0.6f, 1.6f, 1.5f, F);
      px(t, ear + Vec2{0.1f, 0.6f}, F[0]);
    }
    // eye with a bright glint so it reads on dark fur
    Vec2 e = hc + Vec2{q.headR * 0.35f, -q.headR * 0.2f};
    if (eyeShut) { px(t, e, F[0]); px(t, e + Vec2{-1, 0}, F[0]); }
    else { px(t, e, q.eye); px(t, e + Vec2{-1, 0}, kInk); px(t, e + Vec2{-1, -1}, F[3]); }
    if (dead) { px(t, e + Vec2{0, -1}, F[0]); px(t, e + Vec2{-1, 1}, F[0]); }
  }, 0.65f);
  recHead(hc, q.headR);
  recEye(hc + Vec2{q.headR * 0.35f, -q.headR * 0.2f});
  recBody(bx + 0.5f, by + q.slope * 0.2f, q.bodyLen * 0.5f + q.bodyR * 0.35f, q.bodyR);
  recRamp(F);
  recRamp(q.belly);
  // (M3c LIFE) spots: blotches of the spot colour on the coat's own tones (eyes, nose, teeth and belly untouched)
  if (q.spot) {
    for (int y = 0; y < H; y++)
      for (int x = 0; x < c.w; x++) {
        const uint32_t v = c.get(x, y);
        if (v != F[1] && v != F[2] && v != F[3]) continue;
        if (hash3(x / 2 + (y & 1), y / 2, q.seed + 77) % 5 != 0) continue;
        c.set(x, y, v == F[3] ? mix(q.spot, F[2], 0.45f) : v == F[2] ? q.spot : shade(q.spot, 0.8f));
      }
  }
}

// ------------------------------------------------------------------ slime
void slime(Canvas& c, int frame) {
  const Ramp G = ramp5(rgba(34, 82, 70), rgba(52, 132, 76), rgba(94, 186, 82), rgba(150, 222, 104), rgba(226, 250, 186));
  const int W = c.w;
  const float ground = c.h - 1.5f;
  static const float sx[8] = {7.0f, 6.2f, 5.6f, 6.4f, 8.2f, 5.0f, 6.8f, 8.6f};
  static const float sy[8] = {5.2f, 6.0f, 6.8f, 5.8f, 3.8f, 7.6f, 5.4f, 2.2f};
  static const float ox[8] = {0, 0, 0.5f, 0.5f, -1, 2.0f, -1, 0};
  float rx = sx[frame], ry = sy[frame], cx = W * 0.5f + ox[frame], cy = ground - ry;
  // body: dome (flat bottom)
  for (int y = (int)(cy - ry - 1); y <= (int)ground; y++)
    for (int x = (int)(cx - rx - 1); x <= (int)(cx + rx + 1); x++) {
      float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
      if (dy > 0) dy *= 0.55f;   // bottom flattens out
      if (dx * dx + dy * dy > 1) continue;
      float l = lightAt(dx * 0.9f, std::min(0.9f, dy * 0.9f));
      int k = lightIndex(l, x, y, 0.12f);
      if (y >= (int)ground - 1) k = std::max(0, k - 1);
      c.set(x, y, withA(G[k], 236));
    }
  // inner core + bubbles
  if (frame != 7) {
    ellipse(c, cx + 0.5f, cy + ry * 0.35f, rx * 0.45f, ry * 0.3f, withA(G[1], 236));
    c.set((int)(cx - rx * 0.3f), (int)(cy + ry * 0.2f), withA(G[3], 236));
    c.set((int)(cx + rx * 0.45f), (int)(cy + ry * 0.45f), withA(G[3], 236));
  }
  // gloss
  int gx = (int)(cx - rx * 0.5f), gy = (int)(cy - ry * 0.55f);
  c.set(gx, gy, kWhite); c.set(gx + 1, gy, G[4]); c.set(gx, gy + 1, G[4]);
  // face
  int ey = (int)(cy - ry * 0.05f), ex = (int)(cx + rx * 0.15f);
  recHead(Vec2{cx + rx * 0.2f, cy - ry * 0.2f}, std::max(2.5f, ry * 0.75f));
  recEye(Vec2{(float)ex + 3, (float)ey});
  recBody(cx, cy, rx, ry);
  recRamp(G);
  if (frame == 6 || frame == 7) {
    c.set(ex, ey, kInk); c.set(ex + 3, ey, kInk); c.set(ex + 1, ey + 1, kInk); c.set(ex + 4, ey + 1, kInk);
  } else {
    c.set(ex, ey, kInk); c.set(ex, ey + 1, kInk); c.set(ex + 3, ey, kInk); c.set(ex + 3, ey + 1, kInk);
    c.set(ex, ey, rgba(70, 60, 90));
    if (frame == 5) { c.set(ex + 1, ey + 3, kInk); c.set(ex + 2, ey + 3, kInk); c.set(ex + 1, ey + 4, rgba(150, 40, 60)); }
  }
  if (frame == 7)   // spilled droplets
    { c.set((int)(cx - rx - 2), (int)ground, G[2]); c.set((int)(cx + rx + 2), (int)ground, G[1]); }
}

// ------------------------------------------------------------------ spider
void spider(Canvas& c, int frame, bool frost) {
  const Ramp B = frost ? ramp5(rgba(60, 76, 120), rgba(102, 130, 176), rgba(156, 190, 220), rgba(206, 228, 244), rgba(250, 254, 255))
                       : ramp5(rgba(30, 24, 36), rgba(54, 40, 56), rgba(84, 62, 72), rgba(122, 94, 94), rgba(168, 136, 120));
  const Ramp Lg = frost ? ramp5(rgba(46, 58, 100), rgba(80, 104, 156), rgba(126, 156, 200), rgba(176, 204, 232), rgba(232, 246, 255))
                        : ramp5(rgba(26, 20, 32), rgba(46, 34, 48), rgba(72, 54, 62), rgba(104, 82, 84), rgba(140, 114, 106));
  const uint32_t eyeC = frost ? rgba(120, 230, 255) : rgba(250, 70, 60);
  const float ground = c.h - 2.0f;
  const bool dead = frame == 7;
  float bx = c.w * 0.40f, by = ground - 7.0f;
  if (frame < 4 && (frame & 1)) by -= 1;
  if (frame == 4) { bx -= 2; by -= 1; }
  if (frame == 5) bx += 3;
  if (frame == 6) bx -= 1;
  if (dead) by = ground - 3.5f;
  const Vec2 ceph{bx + 6.0f, by + 0.5f};
  // one leg: hip on the cephalothorax, knee arched high, foot on the ground
  auto leg = [&](Canvas& t, int side, int i) {
    int bias = side == 0 ? -1 : 0;
    float ph = (float)frame * PI * 0.5f + (i & 1) * PI + side * PI;
    Vec2 hip = ceph + Vec2{-2.0f + i * 1.2f, 0.5f};
    float spread = (i - 1.5f) * 4.6f + (side == 0 ? -1.2f : 1.2f);
    float stepX = frame < 4 ? std::cos(ph) * 1.4f : 0.0f;
    float lift = frame < 4 ? std::max(0.0f, std::sin(ph)) * 2.0f : 0.0f;
    Vec2 knee = hip + Vec2{spread * 0.62f + stepX * 0.5f, -6.5f - lift * 0.5f + (side == 0 ? -1.0f : 0.0f)};
    Vec2 foot{hip.x + spread * 1.5f + stepX, ground - lift};
    if (frame == 4 && i == 3) { knee = hip + Vec2{3, -8}; foot = hip + Vec2{7, -6}; }   // front legs raised to strike
    if (frame == 4 && i == 2) { knee = hip + Vec2{2, -7}; foot = hip + Vec2{6, -3}; }
    if (frame == 5 && i == 3) { knee = hip + Vec2{5, -4}; foot = hip + Vec2{9, 1}; }
    if (dead) { knee = hip + Vec2{spread * 0.35f, -4.5f}; foot = knee + Vec2{spread > 0 ? -1.5f : 1.5f, -1.5f}; }
    capsule(t, hip, knee, 0.8f, 0.6f, Lg, bias + 1);
    capsule(t, knee, foot, 0.6f, 0.45f, Lg, bias);
    px(t, knee + Vec2{-0.4f, -0.6f}, Lg[3 + bias]);
    px(t, foot, Lg[0]);
  };
  for (int i = 0; i < 4; i++) leg(c, 0, i);   // far legs behind the body
  // abdomen with a marking, cephalothorax
  furBall(c, bx - 0.5f, by - 1.0f, 5.2f, 4.2f, B, 0.18f, 7);
  const uint32_t mk = frost ? rgba(130, 230, 255) : rgba(214, 74, 52);
  const int ax = (int)bx - 2, ay = (int)by - 4;
  c.set(ax, ay, mk); c.set(ax + 1, ay, mk); c.set(ax, ay + 1, shade(mk, 0.8f)); c.set(ax + 1, ay + 1, shade(mk, 0.7f));
  c.set(ax - 2, ay + 2, shade(mk, 0.75f)); c.set(ax + 3, ay + 2, shade(mk, 0.7f)); c.set(ax, ay + 3, shade(mk, 0.6f));
  if (frost) {   // ice crystals growing on the back
    for (int k = 0; k < 3; k++) {
      int x = ax - 2 + k * 3, y = ay - 2 - (k == 1);
      c.set(x, y, kSnow[4]); c.set(x, y - 1, kSnow[3]); c.set(x + 1, y, kCrystal[3]);
    }
  }
  layered(c, [&](Canvas& t) { furBall(t, ceph.x, ceph.y, 2.8f, 2.4f, B, 0.1f, 8); }, 0.6f);
  for (int i = 0; i < 4; i++) layered(c, [&](Canvas& t) { leg(t, 1, i); }, 0.55f);
  // eyes + fangs
  int ex = (int)(ceph.x + 2), ey = (int)(ceph.y - 1);
  recHead(ceph, 2.8f);
  recEye(Vec2{(float)ex, (float)ey});
  recBody(bx - 0.5f, by - 1.0f, 5.2f, 4.2f);
  recRamp(B);
  recRamp(Lg);
  if (dead || frame == 6) { c.set(ex, ey, B[0]); c.set(ex - 1, ey, B[0]); }
  else { c.set(ex, ey, eyeC); c.set(ex - 1, ey, eyeC); c.set(ex, ey - 1, shade(eyeC, 0.7f)); c.set(ex - 2, ey, shade(eyeC, 0.6f)); c.set(ex + 1, ey, kWhite); }
  c.set(ex + 1, ey + 2, kBone[3]); c.set(ex + 2, ey + 2, kBone[2]);
  if (frame == 5) { c.set(ex + 2, ey + 3, kBone[4]); c.set(ex + 3, ey + 3, kBone[2]); }
}

// ------------------------------------------------------------------ bat
void bat(Canvas& c, int frame) {
  const Ramp B = ramp5(rgba(30, 22, 40), rgba(54, 38, 62), rgba(84, 60, 86), rgba(120, 88, 108), rgba(160, 124, 136));
  const Ramp M = ramp5(rgba(44, 26, 48), rgba(74, 40, 66), rgba(108, 60, 84), rgba(144, 86, 100), rgba(180, 120, 120));
  bool dead = frame == 7;
  float cx = c.w * 0.5f, cy = c.h * 0.45f;
  static const float bobs[8] = {0, 1, 2, 1, -1, 2, 0, 0};
  cy += bobs[frame];
  if (frame == 5) cx += 3;
  if (dead) cy = c.h - 4.0f;
  // wing pose: angle of the wing tip (radians up from horizontal)
  static const float wingA[8] = {0.8f, 0.2f, -0.7f, 0.2f, 0.9f, -0.4f, 0.6f, -0.1f};
  float a = wingA[frame];
  auto wing = [&](int dir, int bias) {
    float span = dead ? 6.5f : 9.0f;
    Vec2 root{cx + dir * 1.5f, cy};
    Vec2 tip = root + Vec2{dir * span * std::cos(a), -span * std::sin(a)};
    Vec2 elbow = root + Vec2{dir * span * 0.45f * std::cos(a * 0.7f), -span * 0.55f * std::sin(a) - 1.5f};
    Vec2 lower = root + Vec2{dir * span * 0.55f, 3.0f - std::sin(a) * 1.5f};
    if (dead) { tip = root + Vec2{dir * span, 1}; elbow = root + Vec2{dir * span * 0.5f, -2}; lower = root + Vec2{dir * span * 0.6f, 2}; }
    // membrane with scalloped trailing edge
    poly(c, {root + Vec2{0, -1}, elbow, tip, lower * 0.5f + tip * 0.5f + Vec2{0, 1.5f}, lower, root + Vec2{0, 2}}, M[2 + bias]);
    poly(c, {root, elbow, lower}, M[1 + bias]);
    line(c, (int)root.x, (int)root.y - 1, (int)elbow.x, (int)elbow.y, B[3 + bias]);
    line(c, (int)elbow.x, (int)elbow.y, (int)tip.x, (int)tip.y, B[2 + bias]);
    line(c, (int)elbow.x, (int)elbow.y, (int)(lower.x * 0.5f + tip.x * 0.5f), (int)(lower.y * 0.5f + tip.y * 0.5f + 1), B[1 + bias]);
    px(c, elbow + Vec2{0, -1}, B[4 + bias]);
  };
  wing(-1, -1);
  // body + head
  furBall(c, cx, cy + 1, 3.0f, 3.4f, B, 0.2f, 11);
  furBall(c, cx + 1, cy - 2.5f, 2.6f, 2.3f, B, 0.1f, 12);
  // ears
  c.set((int)cx - 1, (int)cy - 6, B[3]); c.set((int)cx - 1, (int)cy - 5, B[2]);
  c.set((int)cx + 2, (int)cy - 6, B[2]); c.set((int)cx + 2, (int)cy - 5, B[1]);
  wing(1, 0);
  // face
  int ex = (int)cx + 2, ey = (int)cy - 3;
  recHead(Vec2{cx + 1, cy - 2.5f}, 2.6f);
  recEye(Vec2{(float)ex, (float)ey});
  recBody(cx, cy + 1, 3.0f, 3.4f);
  recRamp(B);
  recRamp(M);
  if (frame == 6 || dead) { c.set(ex, ey, B[0]); c.set(ex - 2, ey, B[0]); }
  else { c.set(ex, ey, rgba(255, 70, 70)); c.set(ex - 2, ey, rgba(220, 50, 60)); }
  c.set(ex - 1, ey + 2, kWhite);
  if (frame == 5) c.set(ex + 1, ey + 2, kWhite);
  // feet
  c.set((int)cx - 1, (int)cy + 5, B[1]); c.set((int)cx + 1, (int)cy + 5, B[1]);
}

}  // namespace

namespace {

// ------------------------------------------------------------------ troll
void troll(Canvas& c, int frame) {
  // frost troll: shaggy pale fur over slate-dark hide, long knuckle-dragging arms, small low head
  const Ramp F = ramp5(rgba(70, 76, 100), rgba(118, 126, 142), rgba(170, 176, 182), rgba(206, 210, 208), rgba(236, 238, 230));
  const Ramp S = ramp5(rgba(34, 32, 46), rgba(56, 52, 66), rgba(84, 78, 88), rgba(114, 106, 110), rgba(146, 138, 134));
  const float ground = c.h - 2.0f;
  const bool dead = frame == 7;
  float bx = c.w * 0.42f, by = ground - 16.0f;
  if (frame < 4) by += (frame & 1) ? -1.0f : 0.0f;
  if (frame == 4) { bx -= 1; by -= 1; }
  if (frame == 5) { bx += 2; by += 1; }
  if (frame == 6) bx -= 1;
  const float ph = frame * PI * 0.5f;
  recRamp(F);
  recRamp(S);
  if (dead) {
    recHead(V(bx + 10, ground - 4), 3.4f);
    recEye(V(bx + 11, ground - 5));
    recBody(bx - 1, ground - 5, 11.0f, 5.0f);
    // sprawled face down
    capsule(c, V(bx - 9, ground - 2), V(bx - 14, ground - 1), 2.2f, 1.8f, S, -1);
    furBall(c, bx - 1, ground - 5, 11.0f, 5.0f, F, 0.35f, 21);
    capsule(c, V(bx + 6, ground - 2), V(bx + 13, ground - 1), 2.2f, 2.0f, F, 0);
    ball(c, bx + 14, ground - 2.5f, 2.6f, 2.0f, S);
    ball(c, bx + 10, ground - 4, 3.6f, 3.0f, S);
    c.set((int)bx + 11, (int)ground - 5, S[0]); c.set((int)bx + 12, (int)ground - 4, kBone[3]);
    return;
  }
  auto legAt = [&](Canvas& t, float phase, int bias) {
    Vec2 h{bx - 2.0f, by + 7.0f};
    Vec2 f{bx - 1.0f + std::cos(phase) * 3.0f, ground - std::max(0.0f, std::sin(phase)) * 2.0f};
    if (frame == 4) f.x -= 2;
    if (frame == 5) f.x += 2;
    Vec2 k = (h + f) * 0.5f + Vec2{2.0f, -0.5f};
    capsule(t, h, k, 3.2f, 2.6f, F, bias);
    capsule(t, k, f, 2.4f, 1.9f, S, bias);
    hline(t, (int)f.x - 2, (int)f.x + 3, (int)f.y, S[1 + bias]);
    t.set((int)f.x + 3, (int)f.y, kBone[2 + bias]);
  };
  auto armAt = [&](Canvas& t, Vec2 hand, int bias) {
    Vec2 s{bx + 4.0f, by - 5.0f};
    Vec2 e = (s + hand) * 0.5f + Vec2{-1.5f, 0.5f};
    capsule(t, s, e, 3.0f, 2.6f, F, bias);
    capsule(t, e, hand, 2.4f, 2.0f, S, bias);
    furBall(t, hand.x, hand.y, 2.8f, 2.5f, S, 0.15f, 22, bias);
    t.set((int)hand.x + 2, (int)hand.y + 2, kBone[3 + bias]); t.set((int)hand.x + 3, (int)hand.y + 1, kBone[2 + bias]);
  };
  Vec2 farHand{bx + 5 - std::cos(ph) * 3.0f, ground - 3.0f};
  Vec2 nearHand{bx + 8 + std::cos(ph) * 3.0f, ground - 3.5f};
  if (frame == 4) { nearHand = V(bx - 5, by - 13); farHand = V(bx + 4, ground - 5); }
  if (frame == 5) { nearHand = V(bx + 15, ground - 2.5f); farHand = V(bx + 7, ground - 5); }
  if (frame == 6) nearHand = V(bx + 3, by - 9);
  armAt(c, farHand, -1);
  legAt(c, ph + PI, -1);
  // hunched torso: belly, chest, shaggy shoulders
  furBall(c, bx, by + 1, 8.5f, 8.5f, F, 0.4f, 23);
  furBall(c, bx + 3.5f, by - 4.0f, 6.5f, 5.5f, F, 0.4f, 24);
  ellipse(c, bx + 3.0f, by + 4.5f, 4.0f, 3.5f, S[2]);       // bare belly hide
  ellipse(c, bx + 2.5f, by + 4.0f, 3.0f, 2.5f, S[3]);
  for (int x = (int)bx - 8; x <= (int)bx + 8; x += 2)        // fur tufts along the back
    for (int y = 0; y < c.h; y++) if (solid(c, x, y)) { c.set(x, y - 1, F[3]); c.set(x + 1, y, F[4]); break; }
  layered(c, [&](Canvas& t) { legAt(t, ph, 0); }, 0.6f);
  // head, low and forward
  Vec2 hc{bx + 9.0f, by - 3.0f + (frame == 6 ? -2.0f : 0.0f) + (frame == 5 ? 2.0f : 0.0f)};
  recHead(hc, 4.0f);
  recEye(hc + Vec2{2, -1});
  recBody(bx + 1.0f, by - 1.0f, 8.5f, 8.5f);
  layered(c, [&](Canvas& t) {
    ball(t, hc.x, hc.y, 4.0f, 3.6f, S);
    ball(t, hc.x + 2.6f, hc.y + 1.4f, 2.4f, 1.9f, S);
    furBall(t, hc.x - 1.0f, hc.y - 2.5f, 3.6f, 1.8f, F, 0.3f, 25);    // fur cap
    hline(t, (int)hc.x, (int)hc.x + 3, (int)hc.y - 2, S[0]);          // heavy brow
    if (frame == 6) t.set((int)hc.x + 2, (int)hc.y - 1, S[0]);
    else { t.set((int)hc.x + 2, (int)hc.y - 1, rgba(255, 196, 70)); t.set((int)hc.x + 1, (int)hc.y - 1, kInk); t.set((int)hc.x - 1, (int)hc.y - 3, rgba(255, 196, 70)); }
    t.set((int)hc.x + 3, (int)hc.y + 2, kBone[4]); t.set((int)hc.x + 3, (int)hc.y + 1, kBone[3]); t.set((int)hc.x + 5, (int)hc.y + 2, kBone[3]);
    hline(t, (int)hc.x + 2, (int)hc.x + 4, (int)hc.y + 3, frame == 5 ? rgba(130, 40, 56) : S[0]);
  }, 0.6f);
  layered(c, [&](Canvas& t) { armAt(t, nearHand, 0); }, 0.65f);
}

// ------------------------------------------------------------------ wraith
void wraith(Canvas& c, int frame) {
  const Ramp K = ramp5(rgba(22, 18, 42), rgba(40, 34, 72), rgba(66, 60, 110), rgba(104, 100, 150), rgba(150, 150, 196));
  bool dead = frame == 7;
  static const float bob[8] = {0, -1, -2, -1, -2, 0, -1, 0};
  float cx = c.w * 0.5f, top = 4.0f + bob[frame];
  if (frame == 5) cx += 3;
  if (frame == 6) cx -= 1;
  recRamp(K);
  if (dead) {
    recHead(V(cx + 2, c.h - 7.0f), 2.5f);
    recEye(V(cx + 3, c.h - 7.0f));
    recBody(cx, c.h - 5.0f, 7.0f, 2.5f);
  } else {
    recHead(V(cx + 1.5f, top + 4.0f), 3.0f);
    recEye(V(cx + 1, top + 4));
    recBody(cx, top + (c.h - 6) * 0.55f, 6.0f, (c.h - 6) * 0.42f);
  }
  if (dead) {
    // a crumpled empty cloak and a fading wisp
    for (int y = 0; y < 6; y++)
      for (int x = -8; x <= 8; x++) {
        float e = (float)(x * x) / 64.0f + (float)((y - 5) * (y - 5)) / 30.0f;
        if (e > 1) continue;
        int k = y < 2 ? 3 : (y < 4 ? 2 : 1);
        if (x > 3) k--;
        c.set((int)cx + x, c.h - 3 - 5 + y, withA(K[k], 220));
      }
    c.set((int)cx - 1, c.h - 12, withA(K[4], 140)); c.set((int)cx, c.h - 14, withA(K[3], 110));
    return;
  }
  // tattered cloak: a bell shape whose hem flutters
  int h = c.h - 6;
  for (int y = 0; y < h; y++) {
    float t = (float)y / h;
    float half = 2.8f + t * 6.0f;
    if (y < 7) half = 3.5f + std::sin(t * PI * 2.2f) * 0.6f;
    for (int x = (int)(cx - half); x <= (int)(cx + half); x++) {
      float fx = (x + 0.5f - cx) / half;
      int k = fx < -0.5f ? 3 : (fx < 0.35f ? 2 : 1);
      if (y < 3) k++;
      // ragged hem
      int tear = (int)(hash3(x, frame & 3, 31) % 4);
      if (y > h - 1 - tear) continue;
      int alpha = 230 - std::max(0, y - (h - 8)) * 18;
      c.set(x, (int)top + y, withA(K[k], alpha));
    }
  }
  // hood opening + eyes
  for (int y = 2; y < 7; y++)
    for (int x = -2; x <= 2; x++)
      if (x * x + (y - 4) * (y - 4) <= 5) c.set((int)cx + 1 + x, (int)top + y, rgba(10, 8, 20));
  uint32_t eye = rgba(120, 240, 220);
  if (frame != 6) { c.set((int)cx + 1, (int)top + 4, eye); c.set((int)cx + 3, (int)top + 4, eye); c.set((int)cx + 2, (int)top + 5, shade(eye, 0.5f)); }
  // skeletal hands
  float ay = top + 10, ax = cx + 5;
  if (frame == 4) { ay -= 5; ax -= 1; }
  if (frame == 5) { ay -= 1; ax += 4; }
  capsule(c, V(cx + 3, top + 8), V(ax, ay), 1.4f, 1.0f, K, 0);
  c.set((int)ax + 1, (int)ay, kBone[3]); c.set((int)ax + 2, (int)ay - 1, kBone[4]); c.set((int)ax + 2, (int)ay + 1, kBone[3]); c.set((int)ax + 1, (int)ay + 2, kBone[2]);
}

// ------------------------------------------------------------------ mudcrab
void mudcrab(Canvas& c, int frame) {
  const Ramp S = ramp5(rgba(60, 36, 40), rgba(104, 60, 48), rgba(150, 92, 60), rgba(190, 132, 80), rgba(226, 178, 118));
  const Ramp Lg = ramp5(rgba(70, 38, 42), rgba(120, 64, 50), rgba(170, 100, 66), rgba(206, 140, 88), rgba(236, 186, 128));
  const float ground = c.h - 2.0f;
  const bool dead = frame == 7;
  float cx = c.w * 0.42f, cy = ground - 4.5f;
  if (frame == 5) cx += 2;
  if (frame == 4) cx -= 1;
  if (frame < 4 && (frame & 1)) cy -= 0.5f;
  if (dead) cy = ground - 3.0f;
  // walking legs, three a side
  auto legs = [&](Canvas& t, int side) {
    int bias = side ? 0 : -1;
    for (int i = 0; i < 3; i++) {
      float ph = frame * PI * 0.5f + i * 2.1f + side * PI;
      float lx = cx - 4.0f + i * 3.2f + (side ? 0.6f : -0.6f);
      Vec2 hip{lx, cy + 1};
      Vec2 knee{lx - 2.0f + (frame < 4 ? std::cos(ph) : 0), cy - 1.0f};
      Vec2 foot{lx - 3.0f + (frame < 4 ? std::cos(ph) * 1.5f : 0), ground - (frame < 4 ? std::max(0.0f, std::sin(ph)) : 0)};
      if (dead) { knee = V(lx, cy - 4); foot = V(lx + 1, cy - 6); }
      capsule(t, hip, knee, 0.7f, 0.6f, Lg, bias);
      capsule(t, knee, foot, 0.6f, 0.4f, Lg, bias);
    }
  };
  // big pincer: arm, palm, fixed finger and moving finger (open on the wind-up)
  auto claw = [&](Canvas& t, int bias, float raise, bool open, float reach) {
    Vec2 s{cx + 5, cy};
    Vec2 e{cx + 8, cy - 1.5f - raise};
    Vec2 palm{cx + 10.5f + reach, cy - 2.5f - raise * 1.3f};
    capsule(t, s, e, 1.0f, 0.9f, Lg, bias);
    capsule(t, e, palm, 1.1f, 2.0f, Lg, bias);
    float gap = open ? 1.8f : 0.4f;
    capsule(t, palm + Vec2{1, -0.8f}, palm + Vec2{4.5f, -1.2f - gap}, 1.2f, 0.5f, Lg, bias);   // upper finger
    capsule(t, palm + Vec2{1, 0.9f}, palm + Vec2{4.0f, 0.8f + gap * 0.5f}, 0.9f, 0.4f, Lg, bias - 1);
    px(t, palm + Vec2{4.5f, -1.4f - gap}, Lg[4 + bias]);
  };
  const float raise = frame == 4 ? 3.0f : (frame == 5 ? 0.0f : 1.0f);
  const float reach = frame == 5 ? 2.5f : 0.0f;
  recHead(V(cx + 5, cy - 3), 2.2f);
  recEye(V(cx + 4, cy - 6 - (dead ? -3 : (frame & 1))));
  recBody(cx, cy - 1.5f, 8.0f, 4.0f);
  recRamp(S);
  recRamp(Lg);
  legs(c, 0);
  if (!dead) claw(c, -1, raise + 1.5f, frame == 4, reach * 0.5f);
  // shell: broad flat carapace with a lit rim, ridges and spikes
  layered(c, [&](Canvas& t) {
    for (int y = (int)(cy - 5); y <= (int)(cy + 2); y++)
      for (int x = (int)(cx - 8); x <= (int)(cx + 8); x++) {
        float dx = (x + 0.5f - cx) / 8.0f, dy = (y + 0.5f - cy) / (y < cy ? 5.0f : 2.4f);
        if (dx * dx + dy * dy > 1) continue;
        int k = lightIndex(lightAt(dx * 0.9f, std::clamp(dy * 0.9f, -0.9f, 0.9f)) + (hashf(x, y, 3) - 0.5f) * 0.2f, x, y, 0.1f);
        if (dead) k = std::max(0, k - 1);
        t.set(x, y, S[k]);
      }
    for (int x = (int)cx - 6; x <= (int)cx + 6; x += 3) {   // segment ridges
      for (int y = 0; y < c.h; y++) if (solid(t, x, y)) { t.set(x, y - 1, S[3]); for (int k = 1; k < 4; k++) t.set(x, y + k, S[1]); break; }
    }
    hline(t, (int)cx - 7, (int)cx + 7, (int)cy + 1, S[0]);
    hline(t, (int)cx - 6, (int)cx + 6, (int)cy, S[3]);       // lit lip of the shell
  }, 0.5f);
  legs(c, 1);
  if (!dead) {
    int ex = (int)cx + 4, ey = (int)cy - 6 - (frame & 1);
    vline(c, ex, ey + 1, ey + 2, Lg[2]); vline(c, ex + 2, ey + 1, ey + 2, Lg[1]);
    c.set(ex, ey, frame == 6 ? Lg[0] : kInk); c.set(ex + 2, ey, frame == 6 ? Lg[0] : kInk);
    layered(c, [&](Canvas& t) { claw(t, 0, raise, frame == 4, reach); }, 0.6f);
  }
}

// ------------------------------------------------------------------ sandworm
void sandworm(Canvas& c, int frame) {
  const Ramp W = ramp5(rgba(80, 48, 52), rgba(132, 80, 64), rgba(184, 122, 84), rgba(218, 166, 110), rgba(242, 208, 150));
  const float ground = c.h - 2.0f;
  bool dead = frame == 7;
  // spine curve rising out of a sand mound
  float sway = frame < 4 ? std::sin(frame * PI * 0.5f) * 2.0f : 0.0f;
  Vec2 base{c.w * 0.4f, ground - 2};
  Vec2 tip{c.w * 0.45f + sway, 6.0f};
  float lean = 0;
  if (frame == 4) { tip = V(c.w * 0.3f, 4.0f); lean = -3; }
  if (frame == 5) { tip = V(c.w * 0.72f, 11.0f); lean = 4; }
  if (frame == 6) { tip = V(c.w * 0.38f, 8.0f); }
  if (dead) { base = V(c.w * 0.2f, ground - 2); tip = V(c.w * 0.85f, ground - 3); }
  Vec2 mid = (base + tip) * 0.5f + Vec2{-2.0f - lean * 0.3f + sway * 0.5f, 1};
  if (dead) mid = (base + tip) * 0.5f + Vec2{0, -2};
  // sand mound
  if (!dead)
    for (int y = 0; y < 4; y++)
      for (int x = -9 + y; x <= 9 - y; x++) c.set((int)base.x + x, (int)ground - y, kSand[y == 3 ? 3 : (x < 0 ? 2 : 1)]);
  // segments: balls along a quadratic curve, tail first
  const int N = 9;
  for (int i = 0; i <= N; i++) {
    float t = (float)i / N;
    Vec2 p = base * ((1 - t) * (1 - t)) + mid * (2 * (1 - t) * t) + tip * (t * t);
    float r = lerpf(5.0f, 3.8f, t);
    ball(c, p.x, p.y, r, r * 0.9f, W);
    // segment ring
    for (int k = -2; k <= 2; k++) c.set((int)(p.x + k), (int)(p.y + r * 0.6f), W[1]);
  }
  // head: a flared, armoured hood around a round maw ringed with teeth
  Vec2 h = tip + Vec2{1.5f, 0};
  recHead(h, 5.0f);
  recEye(h + Vec2{-1.0f, -2.5f});
  {
    const Vec2 b = (base + tip) * 0.5f;
    recBody(b.x, b.y, std::max(4.5f, std::fabs(tip.x - base.x) * 0.5f + 2.0f), std::max(4.5f, std::fabs(tip.y - base.y) * 0.5f));
  }
  recRamp(W);
  ball(c, h.x - 0.5f, h.y, 5.0f, 4.8f, W);
  ball(c, h.x - 2.5f, h.y - 2.5f, 2.6f, 2.2f, W, 0.1f, 1);    // brow plate
  if (!dead) {
    const bool open = frame == 4 || frame == 5 || (frame & 1) == 0;
    const float ry = open ? 3.4f : 2.0f, rx = open ? 2.0f : 1.4f;
    const float mx = h.x + 3.2f, my = h.y + 0.5f;
    ellipse(c, mx, my, rx + 1.0f, ry + 1.0f, W[4]);                           // lip
    ellipse(c, mx + 0.4f, my, rx + 0.3f, ry + 0.3f, rgba(150, 40, 60));       // gums
    ellipse(c, mx + 0.8f, my, rx - 0.5f, ry - 0.6f, rgba(50, 14, 28));        // throat
    for (int k = 0; k < 7; k++) {   // teeth pointing inward
      float a2 = -PI * 0.5f + k * PI / 6.0f;
      c.set((int)(mx + 0.4f + std::cos(a2) * (rx + 0.3f) * 0.8f), (int)(my + std::sin(a2) * (ry + 0.3f)), kBone[4 - (k & 1)]);
    }
    if (frame == 5) c.set((int)mx + 1, (int)my, rgba(240, 90, 100));
  }
  // dorsal plates
  for (int i = 2; i < N; i += 2) {
    float t = (float)i / N;
    Vec2 p = base * ((1 - t) * (1 - t)) + mid * (2 * (1 - t) * t) + tip * (t * t);
    c.set((int)p.x - 3, (int)p.y - 1, W[4]);
  }
}

// ------------------------------------------------------------------ dragon (boss)
void dragon(Canvas& c, int frame) {
  // ember dragon: crimson scales, gold belly plates, wine-red wing membranes, bone horns and claws
  const Ramp R = ramp5(rgba(56, 18, 40), rgba(106, 28, 44), rgba(160, 44, 44), rgba(206, 84, 54), rgba(242, 146, 86));
  const Ramp Bl = ramp5(rgba(124, 66, 40), rgba(180, 112, 48), rgba(224, 162, 70), rgba(246, 206, 110), rgba(255, 238, 170));
  const Ramp Mb = ramp5(rgba(54, 18, 42), rgba(92, 28, 54), rgba(134, 44, 62), rgba(176, 74, 74), rgba(214, 118, 96));
  const Ramp& Hn = kBone;
  const float ground = c.h - 2.0f;
  const bool dead = frame == 7;
  float bx = 30, by = ground - 15;
  static const float bob[8] = {0, -1, -2, -1, -1, 1, 0, 0};
  by += bob[frame];
  if (frame == 5) bx += 2;
  if (frame == 6) bx -= 2;
  if (dead) by = ground - 8;

  // wing poses: elbow, wrist and four finger tips relative to the shoulder root
  struct WingPose { Vec2 elbow, wrist, tips[4]; };
  static const WingPose kUp = {{-6, -11}, {-2, -25}, {{-15, -27}, {-24, -20}, {-28, -10}, {-22, 0}}};
  static const WingPose kMid = {{-8, -7}, {-15, -16}, {{-33, -17}, {-36, -7}, {-31, 2}, {-20, 6}}};
  static const WingPose kDown = {{-8, -2}, {-16, 3}, {{-31, 4}, {-30, 12}, {-23, 17}, {-13, 14}}};
  static const WingPose kDead = {{-7, 1}, {-15, 6}, {{-27, 10}, {-24, 13}, {-18, 14}, {-11, 13}}};
  const WingPose* poses[8] = {&kUp, &kMid, &kDown, &kMid, &kUp, &kDown, &kMid, &kDead};
  const WingPose& wp = *poses[frame];

  auto wing = [&](Canvas& t, Vec2 root, int bias, float scale) {
    auto P = [&](Vec2 v) { return root + v * scale; };
    Vec2 elbow = P(wp.elbow), wrist = P(wp.wrist), tips[4];
    for (int i = 0; i < 4; i++) tips[i] = P(wp.tips[i]);
    Vec2 trail = P(Vec2{-9, 7}), body = root + Vec2{2, 3};
    // membrane panels between the fingers; trailing edges scalloped toward the wrist
    Vec2 sc[4];
    for (int i = 0; i < 3; i++) sc[i] = (tips[i] + tips[i + 1]) * 0.5f + (wrist - (tips[i] + tips[i + 1]) * 0.5f) * 0.22f;
    sc[3] = (tips[3] + trail) * 0.5f + (wrist - (tips[3] + trail) * 0.5f) * 0.18f;
    poly(t, {root, elbow, wrist, tips[0]}, Mb[3 + bias]);
    for (int i = 0; i < 3; i++) {
      poly(t, {wrist, tips[i], sc[i]}, Mb[(i & 1 ? 2 : 3) + bias]);
      poly(t, {wrist, sc[i], tips[i + 1]}, Mb[(i & 1 ? 3 : 2) + bias]);
    }
    poly(t, {wrist, tips[3], sc[3], trail}, Mb[1 + bias]);
    poly(t, {root, wrist, trail, body}, Mb[1 + bias]);
    poly(t, {root, elbow, wrist}, Mb[2 + bias]);
    // finger bones with lit veins
    for (int i = 0; i < 4; i++) {
      line(t, (int)wrist.x, (int)wrist.y, (int)tips[i].x, (int)tips[i].y, R[1 + bias]);
      line(t, (int)wrist.x, (int)wrist.y - 1, (int)tips[i].x, (int)tips[i].y - 1, R[3 + bias]);
    }
    capsule(t, root, elbow, 1.9f, 1.4f, R, bias);
    capsule(t, elbow, wrist, 1.4f, 1.0f, R, bias);
    px(t, wrist + Vec2{0, -1.5f}, Hn[3]);
    px(t, wrist + Vec2{1, -2.5f}, Hn[4]);   // wrist talon
  };

  Vec2 shoulder{bx + 6, by - 6};
  // far wing behind everything
  wing(c, shoulder + Vec2{4, -1}, -1, 0.82f);
  // tail: tapering chain sweeping back, plated ridge, spade tip
  float sw = frame < 4 ? std::sin(frame * PI * 0.5f) * 1.5f : 0;
  Vec2 t0{bx - 10, by + 2}, t1{bx - 19, by + 7 + sw}, t2{bx - 26, by + 2 - sw}, t3{bx - 30, by - 5};
  if (dead) { t1 = V(bx - 18, ground - 3); t2 = V(bx - 26, ground - 2); t3 = V(bx - 31, ground - 4); }
  const int TN = 16;
  for (int i = TN; i >= 0; i--) {
    float t = (float)i / TN;
    float u = 1 - t;
    Vec2 p = t0 * (u * u * u) + t1 * (3 * u * u * t) + t2 * (3 * u * t * t) + t3 * (t * t * t);
    float r = lerpf(5.0f, 1.2f, t);
    ball(c, p.x, p.y, r, r * 0.95f, R);
    c.set((int)(p.x + r * 0.2f), (int)(p.y + r * 0.8f), Bl[2]);
    if (i % 2 == 0 && i > 0 && i < TN) { c.set((int)p.x, (int)(p.y - r), R[4]); c.set((int)p.x, (int)(p.y - r - 1), Hn[2]); }
  }
  poly(c, {t3 + Vec2{1, 0}, t3 + Vec2{-4, -4}, t3 + Vec2{-2, 0}, t3 + Vec2{-4, 3}}, R[2]);
  px(c, t3 + Vec2{-3, -3}, R[4]);
  // legs
  const float ph = frame * PI * 0.5f;
  auto leg = [&](Canvas& t, Vec2 hip, float phase, int bias, bool hind) {
    Vec2 foot{hip.x + (frame < 4 ? std::cos(phase) * 2.0f : 0) + (hind ? -1 : 2), ground - (frame < 4 ? std::max(0.0f, std::sin(phase)) * 1.5f : 0)};
    if (dead) foot = hip + Vec2{6, 3};
    Vec2 knee = hind ? (hip + foot) * 0.5f + Vec2{4, -1} : (hip + foot) * 0.5f + Vec2{-1, 0};
    capsule(t, hip, knee, hind ? 4.4f : 2.6f, hind ? 2.6f : 2.0f, R, bias);
    capsule(t, knee, foot, hind ? 2.2f : 1.8f, 1.6f, R, bias);
    int fx = (int)foot.x, fy = (int)foot.y;
    hline(t, fx - 1, fx + 1, fy, R[1 + bias]);
    t.set(fx + 2, fy, Hn[2 + bias]); t.set(fx + 3, fy, Hn[3 + bias]); t.set(fx - 2, fy, Hn[2 + bias]);
  };
  leg(c, V(bx - 6, by + 3), ph + PI, -1, true);
  leg(c, V(bx + 9, by + 4), ph, -1, false);
  // body with belly plates and a spiked ridge
  furBall(c, bx - 3, by + 1, 12.5f, 8.6f, R, 0.2f, 41);
  furBall(c, bx + 7, by - 1, 8.5f, 8.0f, R, 0.2f, 42);
  for (int x = (int)bx - 12; x <= (int)bx + 13; x++) {
    int yb = -1;
    for (int y = c.h - 1; y > 0; y--) if (solid(c, x, y) && y < by + 10) { yb = y; break; }
    if (yb < 0) continue;
    for (int k = 0; k < 3; k++) {
      int k2 = k == 0 ? 1 : (k == 1 ? 2 : 3);
      if (x % 4 == 0) k2--;   // plate seams
      c.set(x, yb - k, Bl[std::max(0, k2)]);
    }
  }
  for (int x = (int)bx - 12; x <= (int)bx + 10; x += 3)
    for (int y = 0; y < c.h; y++)
      if (solid(c, x, y)) { c.set(x, y - 1, Hn[2]); c.set(x - 1, y, Hn[1]); c.set(x, y - 2, Hn[3]); break; }
  layered(c, [&](Canvas& t) { leg(t, V(bx - 5, by + 4), ph, 0, true); }, 0.6f);
  // neck + head pose
  Vec2 n0{bx + 11, by - 3};
  Vec2 head{bx + 25, by - 15};
  float jaw = 0;
  bool eyeShut = false, fire = false;
  if (frame < 4) head.y += (frame & 1) ? 1.0f : 0.0f;
  if (frame == 4) { head = V(bx + 17, by - 20); jaw = 1.0f; }
  if (frame == 5) { head = V(bx + 19, by - 9); jaw = 4.0f; fire = true; }
  if (frame == 6) { head = V(bx + 20, by - 19); eyeShut = true; jaw = 1.5f; }
  if (dead) { head = V(bx + 26, ground - 4); eyeShut = true; jaw = 0.5f; }
  Vec2 nc = (n0 + head) * 0.5f + (dead ? Vec2{0, 2} : Vec2{-4, -2});
  recHead(head + Vec2{1, 0}, 5.0f);
  recEye(head + Vec2{1, -1});
  recBody(bx, by, 13.0f, 8.6f);
  recRamp(R);
  layered(c, [&](Canvas& t) {
    const int NN = 10;
    for (int i = 0; i <= NN; i++) {
      float s = (float)i / NN;
      Vec2 p = n0 * ((1 - s) * (1 - s)) + nc * (2 * (1 - s) * s) + head * (s * s);
      float r = lerpf(5.4f, 3.2f, s);
      ball(t, p.x, p.y, r, r, R);
      t.set((int)(p.x + r * 0.5f), (int)(p.y + r * 0.7f), Bl[2]);
      t.set((int)(p.x + r * 0.2f), (int)(p.y + r * 0.85f), Bl[1]);
      if (i % 2 == 1) { t.set((int)(p.x - r * 0.6f), (int)(p.y - r * 0.75f), Hn[3]); t.set((int)(p.x - r * 0.6f) - 1, (int)(p.y - r * 0.75f) - 1, Hn[2]); }
    }
  }, 0.45f);
  layered(c, [&](Canvas& t) {
    // skull + long snout + lower jaw
    ball(t, head.x, head.y, 5.0f, 4.2f, R);
    capsule(t, head + Vec2{2, 0.5f}, head + Vec2{10, 1.5f}, 3.2f, 2.2f, R);
    if (jaw > 0) {
      capsule(t, head + Vec2{1, 2.5f}, head + Vec2{8.5f, 2.5f + jaw}, 2.1f, 1.3f, R, -1);
      for (int k = 2; k <= 9; k++) t.set((int)head.x + k, (int)(head.y + 3.0f + jaw * (k - 1) / 8.0f) - 1, rgba(110, 24, 40));
      for (int k = 3; k <= 9; k += 2) { t.set((int)head.x + k, (int)head.y + 3, kWhite); t.set((int)head.x + k, (int)(head.y + 2.5f + jaw * (k - 1) / 8.0f), kWhite); }
    } else {
      hline(t, (int)head.x + 2, (int)head.x + 10, (int)head.y + 3, R[0]);
      t.set((int)head.x + 8, (int)head.y + 3, kWhite); t.set((int)head.x + 5, (int)head.y + 3, kWhite);
    }
    t.set((int)head.x + 11, (int)head.y + 1, R[0]);   // nostril
    hline(t, (int)head.x + 4, (int)head.x + 10, (int)head.y - 1, R[4]);   // snout ridge highlight
    // brow, swept horns, cheek frill
    hline(t, (int)head.x - 1, (int)head.x + 4, (int)head.y - 3, R[1]);
    capsule(t, head + Vec2{-2, -2}, head + Vec2{-10, -6}, 1.5f, 0.5f, Hn, 0);
    capsule(t, head + Vec2{-1, -3}, head + Vec2{-6, -10}, 1.3f, 0.4f, Hn, 0);
    t.set((int)head.x - 4, (int)head.y + 1, R[3]); t.set((int)head.x - 5, (int)head.y + 2, R[2]); t.set((int)head.x - 6, (int)head.y + 3, R[1]);
    if (eyeShut) hline(t, (int)head.x, (int)head.x + 2, (int)head.y - 1, R[0]);
    else {
      t.set((int)head.x + 1, (int)head.y - 1, rgba(255, 220, 80)); t.set((int)head.x + 2, (int)head.y - 1, rgba(255, 250, 200));
      t.set((int)head.x + 1, (int)head.y - 2, R[0]); t.set((int)head.x, (int)head.y - 1, R[0]);
    }
    if (frame == 4)   // fire kindling in the throat
      for (int i = 0; i < 4; i++) t.set((int)head.x - 2 - i, (int)head.y + 4 + i, kFire[3 - (i & 1)]);
  }, 0.6f);
  layered(c, [&](Canvas& t) { leg(t, V(bx + 9, by + 3), ph + PI, 0, false); }, 0.6f);
  // near wing in front
  layered(c, [&](Canvas& t) { wing(t, shoulder, 0, 1.0f); }, 0.55f);
  // fire breath on the strike: a widening, flickering cone
  if (fire) {
    Vec2 m = head + Vec2{11, 3};
    for (int i = 0; i < 40; i++) {
      float t = i / 39.0f;
      Vec2 p = m + Vec2{t * (c.w - m.x - 3), t * 7.0f + std::sin(t * 9.0f) * 1.2f};
      float r = 1.4f + t * 6.0f;
      for (int y = (int)(p.y - r); y <= (int)(p.y + r); y++)
        for (int x = (int)(p.x - r); x <= (int)(p.x + r); x++) {
          float d = std::hypot(x + 0.5f - p.x, y + 0.5f - p.y) / r + (hashf(x, y, 61) - 0.5f) * 0.25f;
          if (d > 1) continue;
          int k = d < 0.35f ? 4 : (d < 0.7f ? 3 : 2);
          if (t > 0.75f) k--;
          uint32_t cur = c.get(x, y);
          bool hotter = false;
          for (int q = k + 1; q <= 4; q++) if (cur == kFire[q]) hotter = true;
          if (!hotter) c.set(x, y, kFire[k]);
        }
    }
  }
}

}  // namespace

// =====================================================================================================
// M3c Wildlands wildlife (LIFE lane): the seven creatures of the new biomes, in the classic monsters' style (side view
// facing right with the back and head tops lit from the top-left, 5-step ramps, soft layered contours, outline())
// =====================================================================================================
namespace {

// shaggy fringe: every lowest fur pixel of a run hangs a 1-2 px lock below it (the yeti's coat)
void shag(Canvas& c, const Ramp& F, uint32_t seed) {
  Canvas src = c;
  for (int y = 0; y < c.h - 2; y++)
    for (int x = 0; x < c.w; x++) {
      const uint32_t v = src.get(x, y);
      if (!chA(v) || solid(src, x, y + 1)) continue;
      bool fur = false;
      for (int k = 0; k < 5; k++) if (v == F[k]) fur = true;
      if (!fur) continue;
      const uint32_t h = hash3(x, y, seed);
      const int n = (int)(h % 3);
      for (int k = 1; k <= n && k <= 2; k++) c.set(x, y + k, F[k == 1 ? 1 : 0]);
    }
}

// a soft additive-looking glow disc painted in alpha (the wisp; the alpha is kept, outline is skipped for it)
void glowDisc(Canvas& c, float cx, float cy, float r, uint32_t col, int a0) {
  for (int y = (int)std::floor(cy - r - 1); y <= (int)std::ceil(cy + r + 1); y++)
    for (int x = (int)std::floor(cx - r - 1); x <= (int)std::ceil(cx + r + 1); x++) {
      const float d = std::hypot(x + 0.5f - cx, y + 0.5f - cy) / r;
      if (d >= 1) continue;
      const int a = (int)(a0 * (1 - d) * (1 - d * 0.35f));
      if (a <= 0) continue;
      const uint32_t cur = c.get(x, y);
      if (chA(cur) >= a) continue;
      c.set(x, y, withA(col, a));
    }
}

// ------------------------------------------------------------------ giant scorpion (dunes, badlands, scrub)
void scorpion(Canvas& c, int frame) {
  const Ramp C = ramp5(rgba(54, 28, 26), rgba(98, 52, 32), rgba(150, 92, 46), rgba(200, 142, 70), rgba(238, 198, 120));
  const Ramp Lg = ramp5(rgba(48, 26, 26), rgba(84, 46, 32), rgba(128, 80, 44), rgba(176, 124, 64), rgba(214, 176, 104));
  const Ramp Tip = ramp5(rgba(36, 18, 28), rgba(66, 30, 36), rgba(104, 46, 44), rgba(150, 74, 56), rgba(196, 120, 84));
  const float ground = c.h - 2.0f;
  const bool dead = frame == 7;
  float bx = 13.0f, by = ground - 4.5f;
  if (frame < 4 && (frame & 1)) by -= 0.5f;
  if (frame == 4) bx -= 1.5f;
  if (frame == 5) bx += 2.0f;
  if (frame == 6) bx -= 1.0f;
  if (dead) by = ground - 3.0f;
  const float ph = (float)frame * PI * 0.5f;

  // the eight walking legs: hip under the body's side, knee arched up and out, foot on the ground
  auto leg = [&](Canvas& t, int side, int i) {
    const int bias = side == 0 ? -1 : 0;
    const float p = ph + (i & 1) * PI + side * PI * 0.5f;
    Vec2 hip{bx + 4.5f - i * 2.6f, by + 1.0f};
    const float spread = (1.5f - i) * 2.4f + (side ? 0.6f : -0.6f);
    const float step = frame < 4 ? std::cos(p) * 1.2f : 0.0f, lift = frame < 4 ? std::max(0.0f, std::sin(p)) * 1.3f : 0.0f;
    Vec2 knee = hip + Vec2{spread * 0.8f + step * 0.4f, -2.6f - lift * 0.5f - (side ? 0.0f : 0.6f)};
    Vec2 foot{hip.x + spread * 1.5f + step, ground - lift};
    if (dead) { knee = hip + Vec2{spread * 0.4f, -3.5f}; foot = knee + Vec2{spread > 0 ? -1.2f : 1.2f, -1.8f}; }
    capsule(t, hip, knee, 0.6f, 0.5f, Lg, bias + 1);
    capsule(t, knee, foot, 0.5f, 0.35f, Lg, bias);
    px(t, knee + Vec2{-0.3f, -0.6f}, Lg[3 + bias]);
  };
  // the pedipalps: an arm forward and a heavy two-fingered claw (open on the wind-up, forward on the strike)
  auto claw = [&](Canvas& t, int bias, float raise, float reach, bool open) {
    Vec2 s{bx + 6.0f, by + 0.2f}, e{bx + 9.0f + reach * 0.4f, by - 1.6f - raise};
    Vec2 palm{bx + 12.0f + reach, by - 1.2f - raise * 1.2f};
    capsule(t, s, e, 1.0f, 0.9f, C, bias);
    capsule(t, e, palm, 0.9f, 1.4f, C, bias);
    ball(t, palm.x, palm.y, 3.0f, 2.2f, C, 0.08f, bias);
    const float gap = open ? 2.0f : 0.3f;
    capsule(t, palm + Vec2{1.6f, -1.0f}, palm + Vec2{5.0f, -1.6f - gap}, 1.3f, 0.45f, C, bias);   // fixed finger
    capsule(t, palm + Vec2{1.4f, 1.0f}, palm + Vec2{4.4f, 0.8f + gap * 0.5f}, 1.0f, 0.4f, C, bias - 1);
    px(t, palm + Vec2{5.0f, -1.6f - gap}, C[4 + bias]);
    px(t, palm + Vec2{-0.6f, -1.2f}, C[4 + bias]);
  };
  const float raise = frame == 4 ? 2.5f : frame == 5 ? -0.5f : frame == 6 ? 1.5f : (frame & 1) ? 0.5f : 0.0f;
  const float reach = frame == 5 ? 2.0f : frame == 4 ? -1.0f : 0.0f;
  recHead(V(bx + 5.0f, by - 0.3f), 3.0f);
  recEye(V(bx + 6.5f, by - 2.4f));
  recBody(bx - 1.5f, by, 7.0f, 3.4f);
  recRamp(C);
  recRamp(Lg);
  for (int i = 0; i < 4; i++) leg(c, 0, i);
  if (!dead) claw(c, -1, raise + 1.0f, reach * 0.6f - 1.0f, frame == 4 || frame == 6);

  // the tail: five segments arching up over the back to the bulb and its hooked sting
  Vec2 tp[7];
  {
    const float sway = frame < 4 ? std::sin(ph) * 0.6f : 0.0f;
    // control points (relative to the body's rear) per pose
    Vec2 k[7] = {{-6, -1}, {-9.5f, -4}, {-10, -8.5f}, {-8, -12}, {-4.5f, -14}, {-1.0f, -13.5f}, {1.0f, -11}};
    if (frame == 4) { Vec2 w[7] = {{-6, -1}, {-10, -4}, {-11.5f, -9}, {-10, -13.5f}, {-6.5f, -16}, {-3, -16.5f}, {-1, -14.5f}}; for (int i = 0; i < 7; i++) k[i] = w[i]; }
    if (frame == 5) { Vec2 w[7] = {{-6, -1}, {-8, -5}, {-6.5f, -10}, {-2.5f, -13}, {2.5f, -13.5f}, {7.0f, -11}, {9.5f, -7.5f}}; for (int i = 0; i < 7; i++) k[i] = w[i]; }
    if (frame == 6) { Vec2 w[7] = {{-6, -1}, {-9, -3.5f}, {-11, -7}, {-10.5f, -11}, {-8, -13}, {-5, -12.5f}, {-4, -10}}; for (int i = 0; i < 7; i++) k[i] = w[i]; }
    if (dead) { Vec2 w[7] = {{-6, 0}, {-9, 1}, {-11.5f, 1.5f}, {-13, 1}, {-13.5f, 0}, {-13, -1}, {-11.5f, -1.5f}}; for (int i = 0; i < 7; i++) k[i] = w[i]; }
    for (int i = 0; i < 7; i++) tp[i] = Vec2{bx, by} + k[i] + Vec2{i >= 3 ? sway : 0.0f, i >= 4 ? -std::fabs(sway) * 0.5f : 0.0f};
  }
  // body: the segmented abdomen behind the carapace
  layered(c, [&](Canvas& t) {
    for (int i = 0; i < 5; i++) {
      const float r = 2.4f - i * 0.18f;
      capsule(t, tp[i], tp[i + 1], r, r - 0.2f, C, 0);
      px(t, tp[i + 1] + Vec2{-0.4f, -r + 0.4f}, C[4]);              // the lit top of each joint
      px(t, tp[i] * 0.5f + tp[i + 1] * 0.5f + Vec2{r * 0.4f, r * 0.5f}, C[0]);   // the seam's shadow
    }
  }, 0.55f);
  ball(c, bx - 1.5f, by, 7.0f, 3.4f, C, 0.06f);
  // segment seams across the abdomen, lit top edge
  for (int k = 0; k < 4; k++) {
    const int x = (int)(bx - 5.5f + k * 2.6f);
    for (int y = 0; y < c.h; y++)
      if (solid(c, x, y) && y > by - 5) { c.set(x, y + 1, C[1]); c.set(x, y + 2, C[1]); break; }
  }
  // the head carapace (lighter, domed) with two pairs of eyes
  layered(c, [&](Canvas& t) { ball(t, bx + 5.0f, by - 0.3f, 3.6f, 2.8f, C, 0.06f); }, 0.5f);
  if (!dead) {
    const int ex = (int)(bx + 6.5f), ey = (int)(by - 2.4f);
    const uint32_t eye = frame == 6 ? C[0] : kInk;
    c.set(ex, ey, eye); c.set(ex + 1, ey, eye); c.set(ex - 2, ey + 1, eye);
    if (frame != 6) c.set(ex + 1, ey - 1, kWhite);
  }
  // the bulb and its sting
  layered(c, [&](Canvas& t) {
    const Vec2 bulb = tp[6];
    ball(t, bulb.x, bulb.y, 2.3f, 1.9f, Tip, 0.06f);
    Vec2 dir = norm(tp[6] - tp[5]);
    Vec2 down{-dir.y, dir.x};   // the sting hooks to the bulb's underside
    if (down.y < 0) down = down * -1.0f;
    const Vec2 s0 = bulb + dir * 1.8f, s1 = s0 + dir * 1.6f + down * 1.2f, s2 = s1 + down * 1.4f + dir * 0.3f;
    px(t, s0, Tip[1]); px(t, s1, Tip[0]); px(t, s2, kInk);
    px(t, bulb + Vec2{-0.8f, -1.2f}, Tip[4]);
    if (frame == 5 || frame == 4) px(t, s2 + down, rgba(196, 236, 96));   // a bead of venom
  }, 0.6f);
  for (int i = 0; i < 4; i++) layered(c, [&](Canvas& t) { leg(t, 1, i); }, 0.55f);
  if (!dead) layered(c, [&](Canvas& t) { claw(t, 0, raise, reach, frame == 4 || frame == 6); }, 0.6f);
  else {   // on its back: the pale underside up, claws curled
    for (int x = (int)bx - 6; x <= (int)bx + 6; x++)
      for (int y = 0; y < c.h; y++) if (solid(c, x, y)) { c.set(x, y + 1, mix(C[3], kBone[3], 0.5f)); break; }
    capsule(c, V(bx + 7, by), V(bx + 10, by - 2.5f), 1.2f, 1.0f, C, -1);
    ball(c, bx + 10.5f, by - 3.5f, 1.8f, 1.4f, C, 0.08f, -1);
  }
}

// ------------------------------------------------------------------ hyena / ember hound (quadrupeds)
void hyena(Canvas& c, int frame) {
  Quad q;
  q.fur = ramp5(rgba(74, 56, 44), rgba(124, 98, 66), rgba(170, 140, 92), rgba(204, 178, 122), rgba(230, 210, 158));
  q.belly = ramp5(rgba(112, 94, 74), rgba(160, 142, 108), rgba(202, 184, 144), rgba(226, 212, 176), rgba(244, 234, 208));
  q.eye = rgba(240, 196, 70); q.nose = kInk;
  q.bodyLen = 13; q.bodyR = 4.2f; q.legLen = 6.8f; q.legW = 2.3f; q.headR = 3.6f; q.snout = 3.4f;
  q.ears = 1; q.tail = 1; q.mane = true; q.seed = 37;
  q.slope = 2.6f; q.spot = rgba(66, 46, 40); q.darkMuzzle = true;
  quadruped(c, q, frame);
  // the dark crest of the mane down the neck to the shoulders (a hyena's ridge), standing up on the wind-up
  if (frame != 7) {
    const int x0 = (int)(c.w * 0.44f + 2), x1 = (int)(c.w * 0.44f + 9);
    for (int x = x0; x <= x1; x++)
      for (int y = 0; y < c.h; y++)
        if (solid(c, x, y)) {
          c.set(x, y, q.fur[0]);
          if (((x + frame) & 1) && y > 0) c.set(x, y - 1, frame == 4 ? q.fur[0] : q.fur[1]);
          if (frame == 4 && (x & 1) && y > 1) c.set(x, y - 2, q.fur[0]);
          break;
        }
  }
}

void emberHound(Canvas& c, int frame) {
  Quad q;
  q.fur = ramp5(rgba(16, 12, 18), rgba(32, 26, 32), rgba(52, 44, 48), rgba(78, 66, 66), rgba(108, 92, 86));
  q.belly = ramp5(rgba(30, 22, 26), rgba(48, 36, 38), rgba(70, 54, 52), rgba(94, 74, 66), rgba(120, 96, 82));
  q.eye = rgba(255, 196, 60); q.nose = rgba(60, 20, 20);
  q.bodyLen = 14; q.bodyR = 4.4f; q.legLen = 7.0f; q.legW = 2.4f; q.headR = 3.6f; q.snout = 4.0f;
  q.ears = 0; q.tail = 0; q.saddle = false; q.seed = 41;
  quadruped(c, q, frame);
  const bool dead = frame == 7;
  // glowing fissures across the hide: thin ridged-noise lines of embers on the body's own pixels
  Canvas src = c;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const uint32_t v = src.get(x, y);
      bool hide = false;
      for (int k = 0; k < 5; k++) if (v == q.fur[k] || v == q.belly[k]) hide = true;
      if (!hide) continue;
      const float n = vnoise(x * 0.22f, y * 0.30f, 913) * 0.8f + vnoise(x * 0.6f, y * 0.7f, 914) * 0.2f;
      const float ridge = std::fabs(n - 0.5f);
      if (ridge < (dead ? 0.02f : 0.032f)) c.set(x, y, dead ? rgba(110, 40, 26) : ridge < 0.014f ? kFire[3] : kFire[1]);
    }
  if (dead) return;
  // the smouldering mane and tail tip: flames licking up off the neck, shoulders and the end of the tail (they flicker
  // frame to frame)
  const int x0 = (int)(c.w * 0.44f - 3), x1 = (int)(c.w * 0.44f + 10);
  for (int x = x0; x <= x1; x++)
    for (int y = 0; y < c.h; y++)
      if (solid(src, x, y)) {
        const uint32_t h = hash3(x, frame, 4242);
        const int n = 1 + (int)(h % 3) + (x > x0 + 6 ? 1 : 0) + (frame == 4 ? 1 : 0);
        for (int k = 0; k < n; k++) {
          const int ky = y - k;
          if (ky < 0) break;
          c.set(x, ky, k == 0 ? kFire[2] : k == n - 1 ? withA(kFire[0], 220) : kFire[k == 1 ? 3 : 1]);
        }
        break;
      }
  // embers drifting off the mane
  for (int k = 0; k < 3; k++) {
    const uint32_t h = hash3(k, frame, 99);
    const int ex = x0 + (int)(h % (uint32_t)(x1 - x0)), ey = (int)(h >> 8) % 5;
    c.set(ex, ey, kFire[3 + (int)(h >> 16) % 2]);
  }
  // the eye burns
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++)
      if (src.get(x, y) == q.eye) { c.set(x, y, kFire[4]); c.set(x - 1, y, kFire[2]); }
}

// ------------------------------------------------------------------ mire lurker (a marsh crocodile)
// (M6 fixer round 2, review must: "the WYRM world boss reads as a small flat lizard and is lost under the jungle
// canopy") s scales the whole painting at native resolution (the draw scale is always 1, so a bigger beast is a bigger
// painting, never a stretched one); s > 1 also gives it the high walk (belly off the ground on straight legs, the head
// carried up), a taller double crest and a paler throat, so the great wyrm stands tall instead of lying flat.
// s == 1 is the ordinary lurker pixel for pixel.
void lurker(Canvas& c, int frame, float s = 1.0f) {
  const bool great = s > 1.01f;
  // the great wyrm's hide is blackwater slate over a bone-pale belly (a marsh-green one vanished into jungle and fen)
  const Ramp S = great ? ramp5(rgba(18, 20, 32), rgba(34, 40, 58), rgba(56, 66, 88), rgba(86, 100, 124), rgba(132, 148, 170))
                       : ramp5(rgba(22, 36, 30), rgba(38, 60, 40), rgba(62, 88, 50), rgba(94, 120, 64), rgba(136, 156, 92));
  const Ramp Bl = great ? ramp5(rgba(108, 96, 74), rgba(150, 136, 104), rgba(190, 176, 138), rgba(218, 206, 168), rgba(240, 232, 204))
                        : ramp5(rgba(92, 96, 60), rgba(136, 138, 86), rgba(176, 174, 114), rgba(204, 200, 142), rgba(228, 224, 176));
  const float ground = c.h - 2.0f;
  const bool dead = frame == 7;
  const float stance = great && !dead ? 2.2f * s : 0.0f;   // the high walk
  float bx = c.w * 0.42f, by = ground - 3.6f * s - stance;
  if (frame == 4) bx -= 2 * s;
  if (frame == 5) bx += 3 * s;
  if (frame == 6) bx -= 1 * s;
  if (frame < 4 && (frame & 1)) by -= 0.5f * s;
  const float ph = (float)frame * PI * 0.5f;
  // spine from the tail tip to the snout tip: the radius profile makes the shape
  const float sweep = frame < 4 ? std::sin(ph) * 1.0f : 0.0f;
  struct K { float x, y, r; };
  K sp[9] = {{-17, 1.2f + sweep * 0.6f, 0.7f}, {-13, 0.8f + sweep * 0.4f, 1.3f}, {-9, 0.2f, 2.0f}, {-4.5f, -0.2f, 3.0f},
             {0, -0.4f, 3.5f}, {4.5f, -0.5f, 3.2f}, {8.0f, -0.9f, 2.6f}, {11.5f, -0.7f, 1.9f}, {16.0f, -0.2f, 1.25f}};
  for (K& k : sp) { k.x *= s; k.y *= s; k.r *= s; }
  if (great && !dead) { sp[0].y += stance * 0.9f; sp[1].y += stance * 0.6f; sp[2].y += stance * 0.25f; }   // the tail drags
  float headUp = 0;
  if (frame == 4) headUp = 2.5f;
  if (frame == 6) headUp = 1.5f;
  if (great && !dead) headUp += 2.0f;   // the head carried high, watching
  headUp *= s;
  // legs: short, splayed out to the side (the far pair darker), elbows high
  auto leg = [&](Canvas& t, float ax, float p, bool nearSide, bool front) {
    const int bias = nearSide ? 0 : -1;
    ax *= s;
    Vec2 hip{bx + ax, by + 1.5f * s};
    float fx = ax + (frame < 4 ? std::cos(p) * 1.8f * s : (frame == 5 ? (front ? 2.0f : -1.5f) * s : 0.0f));
    Vec2 foot{bx + fx + (front ? 1.0f : -1.0f) * s, ground - (frame < 4 ? std::max(0.0f, std::sin(p)) * 1.2f * s : 0.0f)};
    Vec2 elbow = hip + Vec2{(front ? 1.2f : -1.4f) * s, 0.8f * s + stance * 0.45f};
    if (dead) { foot = hip + Vec2{(front ? 1.5f : -1.5f) * s, -4.0f * s}; elbow = hip + Vec2{0, -2.0f * s}; }
    capsule(t, hip, elbow, 1.3f * s, 1.1f * s, S, bias);
    capsule(t, elbow, foot, 1.0f * s, 0.8f * s, S, bias);
    const int fx0 = (int)std::floor(foot.x), fy0 = (int)std::floor(foot.y);
    if (!dead) {
      t.set(fx0 + 1, fy0, S[1 + bias]); t.set(fx0 + 2, fy0, kBone[1]); t.set(fx0 - 1, fy0, S[1 + bias]);
      if (great) { t.set(fx0 + 3, fy0, kBone[2]); t.set(fx0 + 2, fy0 - 1, kBone[3]); t.set(fx0 - 2, fy0, kBone[1]); }   // claws
    }
  };
  leg(c, -6.0f, ph, false, false);
  leg(c, 5.5f, ph + PI, false, true);
  // the body: balls along the spine, flattened (a croc lies low), belly a pale band underneath
  Canvas body(c.w, c.h);
  const int steps = great ? 8 : 4;
  for (int i = 0; i < 8; i++)
    for (int k = 0; k <= steps; k++) {
      const float t = k / (float)steps;
      const K& a = sp[i];
      const K& b = sp[i + 1];
      const float x = bx + lerpf(a.x, b.x, t), r = lerpf(a.r, b.r, t);
      float y = by + lerpf(a.y, b.y, t);
      if (i >= 5) y -= headUp * (lerpf((float)i, (float)i + 1, t) - 5) / 3.0f;
      furBall(body, x, y, r * 1.25f, r * (great ? 0.9f : 0.82f), dead ? Bl : S, 0.15f, 61);
    }
  // belly band and the jaw line
  for (int x = 0; x < c.w; x++) {
    int yb = -1;
    for (int y = c.h - 1; y >= 0; y--) if (solid(body, x, y)) { yb = y; break; }
    if (yb < 0) continue;
    const float sx = x - bx;
    if (!dead && sx > -12 * s && sx < 7 * s) {
      body.set(x, yb, Bl[1]);
      if (sx > -9 * s && sx < 5 * s) body.set(x, yb - 1, Bl[(x & 1) ? 2 : 1]);
      if (great && sx > -7 * s && sx < 13 * s) {   // a pale scaled throat and belly, banded across (the scale rows)
        body.set(x, yb - 1, Bl[(x % 3 == 0) ? 1 : 2]);
        body.set(x, yb - 2, Bl[(x % 3 == 0) ? 1 : 3]);
        if (sx > 7 * s) body.set(x, yb, Bl[2]);
      }
    }
    if (dead && sx > -14 * s && sx < 14 * s) { body.set(x, yb, S[2]); body.set(x, yb - 1, S[(x & 1) ? 3 : 2]); }
  }
  // the scutes: a double row of armoured bumps along the back, lit on top
  for (int x = (int)(bx - 15 * s); x <= (int)(bx + 8 * s); x++) {
    for (int y = 0; y < c.h; y++)
      if (solid(body, x, y)) {
        if (dead) break;
        if (great) {
          // the great wyrm's crest: paired keeled scutes, every third column a tall lit plate with a dark notch behind
          const int q = ((x + (int)bx) % 3 + 3) % 3;
          const bool tail = x < bx - 8 * s;
          if (q == 0) { body.set(x, y - 1, S[4]); body.set(x, y - 2, S[3]); body.set(x, y, S[1]); if (tail) body.set(x, y - 3, S[2]); }
          else if (q == 1) { body.set(x, y - 1, S[2]); body.set(x, y, S[0]); }
          else { body.set(x, y, S[4]); body.set(x, y + 2, S[3]); }   // the second (flank) row of scutes
          break;
        }
        const bool bump = ((x + (int)bx) % 2) == 0;
        if (bump) { body.set(x, y - 1, S[3]); body.set(x, y, S[1]); }
        else body.set(x, y, S[4]);
        if (x < bx - 8 && bump) body.set(x, y - 2, S[2]);   // the tail's crest stands taller
        break;
      }
  }
  blit(c, body, 0, 0);
  leg(c, -7.0f, ph + PI, true, false);
  leg(c, 4.5f, ph, true, true);
  recHead(V(bx + 11.0f * s, by - 1.6f * s - headUp * 0.7f), 2.4f * s);
  recEye(V(bx + 10.5f * s, by - 2.8f * s - headUp * 0.7f));
  recBody(bx - 1.0f * s, by, 9.0f * s, 3.0f * s);
  recRamp(S);
  if (dead) return;
  // the head: eye bump, nostril bump, jaws
  const Vec2 head{bx + 10.5f * s, by - 1.6f * s - headUp * 0.7f};
  const Vec2 tip{bx + 17.5f * s, by - 0.6f * s - headUp};
  layered(c, [&](Canvas& t) {
    ball(t, head.x, head.y - 0.8f * s, 1.6f * s, 1.3f * s, S);                     // the eye's brow bump
    const int ex = (int)head.x, ey = (int)(head.y - 1.2f * s);
    if (frame == 6) { t.set(ex, ey, S[0]); t.set(ex + 1, ey, S[0]); }
    else {
      t.set(ex, ey, rgba(236, 200, 60)); t.set(ex + 1, ey, kInk); t.set(ex, ey - 1, S[4]);
      if (great) { t.set(ex - 1, ey, rgba(180, 130, 40)); t.set(ex - 1, ey - 1, S[4]); t.set(ex + 1, ey - 1, S[3]); }   // a heavy lit brow
    }
    px(t, tip + Vec2{-0.5f, -1.3f} * s, S[3]);                              // nostril bump
    px(t, tip + Vec2{0.3f, -1.0f} * s, S[0]);
  }, 0.5f);
  // jaws: closed = a dark seam with interlocking teeth; open (wind-up) = the upper jaw lifted, pale mouth, teeth
  const float open = (frame == 4 ? 4.0f : frame == 5 ? 1.0f : frame == 6 ? 1.5f : 0.0f) * s;
  const Vec2 hinge{bx + 8.5f * s, by + 0.2f * s - headUp * 0.4f};
  if (open > 0.5f) {
    const Vec2 lowTip{bx + 17.0f * s, by + 1.0f * s - (great ? headUp * 0.5f : 0.0f)};
    // the mouth: a pale wedge between the jaws, teeth on both rims
    for (int x = (int)hinge.x; x <= (int)lowTip.x; x++) {
      const float t = (x - hinge.x) / (lowTip.x - hinge.x);
      const float yTop = lerpf(hinge.y - 0.5f, tip.y + 0.6f, t), yBot = lerpf(hinge.y + 0.5f, lowTip.y, t);
      for (int y = (int)std::ceil(yTop); y <= (int)std::floor(yBot); y++) c.set(x, y, y == (int)std::floor(yBot) ? rgba(196, 120, 120) : rgba(150, 70, 80));
      if ((x & 1) == 0 && x > hinge.x + 1) { c.set(x, (int)std::ceil(yTop), kBone[4]); c.set(x + 1, (int)std::floor(yBot) - 1, kBone[3]); }
    }
    capsule(c, hinge + Vec2{0, 1.0f * s}, lowTip, 1.1f * s, 0.7f * s, S, -1);
  } else {
    for (int x = (int)hinge.x; x <= (int)tip.x; x++) {
      const float t = (x - hinge.x) / (tip.x - hinge.x);
      const int y = (int)std::floor(lerpf(hinge.y + 0.6f * s, tip.y + 0.9f * s, t));
      c.set(x, y, S[0]);
      if ((x % 3) == 1) c.set(x, y + 1, kBone[3]);
      if ((x % 3) == 2) c.set(x, y - 1, kBone[2]);
      if (great && x < tip.x - 1) c.set(x, y + 1 + ((x % 3) == 1), Bl[1]);   // the pale lower jaw under the seam
    }
  }
}

// ------------------------------------------------------------------ yeti (glacier, snowfields)
void yeti(Canvas& c, int frame) {
  // (M3c fixer round 2, review: "the yeti all but disappears against glacier snow and ice seracs") a cooler, darker
  // grey-blue coat: only the lit back and crown reach the snow's whites, the flanks sit two steps below the snow ground
  // and the underside falls into a deep slate shade, so the beast reads against the glacier at 1x
  const Ramp F = ramp5(rgba(70, 80, 116), rgba(112, 126, 162), rgba(150, 164, 196), rgba(196, 208, 230), rgba(240, 244, 252));
  const Ramp S = ramp5(rgba(38, 42, 66), rgba(62, 70, 98), rgba(92, 100, 130), rgba(126, 132, 160), rgba(164, 168, 190));
  const float ground = c.h - 2.0f;
  const bool dead = frame == 7;
  float bx = c.w * 0.42f, by = ground - 15.0f;
  if (frame < 4) by += (frame & 1) ? -1.0f : 0.0f;
  if (frame == 4) { bx -= 1; by -= 2; }
  if (frame == 5) { bx += 2; by += 2; }
  if (frame == 6) bx -= 1.5f;
  const float ph = (float)frame * PI * 0.5f;
  recRamp(F);
  recRamp(S);
  if (dead) {
    recHead(V(bx + 11, ground - 5), 3.6f);
    recEye(V(bx + 12, ground - 6));
    recBody(bx - 1, ground - 6, 12.0f, 6.0f);
    // face down in the snow: the white mound of its back, an arm flung out, a foot behind
    capsule(c, V(bx - 9, ground - 2), V(bx - 15, ground - 1.5f), 2.6f, 2.2f, S, -1);
    Canvas fur(c.w, c.h);
    furBall(fur, bx - 1, ground - 6, 12.0f, 6.0f, F, 0.4f, 71);
    furBall(fur, bx + 4, ground - 8, 6.0f, 4.0f, F, 0.4f, 72);
    shag(fur, F, 73);
    blit(c, fur, 0, 0);
    capsule(c, V(bx + 7, ground - 3), V(bx + 15, ground - 1.5f), 2.4f, 2.1f, F, 0);
    ball(c, bx + 16, ground - 2, 2.4f, 1.8f, S);
    ball(c, bx + 11, ground - 5, 3.6f, 3.0f, S);
    c.set((int)bx + 12, (int)ground - 6, S[0]);
    return;
  }
  auto legAt = [&](Canvas& t, float phase, int bias) {
    Vec2 h{bx - 2.0f, by + 8.0f};
    Vec2 f{bx - 1.0f + std::cos(phase) * 2.6f, ground - std::max(0.0f, std::sin(phase)) * 1.6f};
    if (frame == 4) f.x -= 1.5f;
    if (frame == 5) f.x += 1.5f;
    Vec2 k = (h + f) * 0.5f + Vec2{1.8f, -0.4f};
    capsule(t, h, k, 3.6f, 3.0f, F, bias);
    capsule(t, k, f, 2.8f, 2.2f, F, bias);
    // the broad dark sole and toes
    hline(t, (int)f.x - 2, (int)f.x + 3, (int)f.y, S[1 + bias]);
    hline(t, (int)f.x - 1, (int)f.x + 2, (int)f.y - 1, S[2 + bias]);
  };
  // arms: thick shaggy upper arm, long forearm, a big dark hand
  auto armAt = [&](Canvas& t, Vec2 hand, int bias) {
    Vec2 s{bx + 4.0f, by - 6.0f};
    Vec2 e = (s + hand) * 0.5f + Vec2{-1.6f, 0.8f};
    capsule(t, s, e, 3.6f, 3.0f, F, bias);
    capsule(t, e, hand, 3.0f, 2.2f, F, bias);
    furBall(t, hand.x, hand.y, 2.6f, 2.3f, S, 0.12f, 74, bias);
    t.set((int)hand.x + 2, (int)hand.y + 1, S[3 + bias]);
  };
  Vec2 farHand{bx + 6 - std::cos(ph) * 2.6f, ground - 3.0f};
  Vec2 nearHand{bx + 9 + std::cos(ph) * 2.6f, ground - 3.2f};
  if (frame == 4) { nearHand = V(bx - 3, by - 16); farHand = V(bx + 3, by - 17); }
  if (frame == 5) { nearHand = V(bx + 14, ground - 2.6f); farHand = V(bx + 11, ground - 2.8f); }
  if (frame == 6) nearHand = V(bx + 6, by - 11);
  Canvas fur(c.w, c.h);
  armAt(fur, farHand, -1);
  legAt(fur, ph + PI, -1);
  // the hulking torso: a barrel chest under great shoulders, the head sunk between them
  furBall(fur, bx, by + 1.5f, 9.0f, 8.6f, F, 0.45f, 75);
  furBall(fur, bx + 2.0f, by - 4.0f, 7.0f, 5.8f, F, 0.45f, 76);
  furBall(fur, bx - 3.0f, by - 3.0f, 5.0f, 5.5f, F, 0.45f, 77);   // the hunched back
  // tufts catching the light along the back and the crown
  for (int x = (int)bx - 8; x <= (int)bx + 9; x += 2)
    for (int y = 0; y < c.h; y++) if (solid(fur, x, y)) { fur.set(x, y - 1, F[3]); fur.set(x + 1, y, F[4]); break; }
  layered(fur, [&](Canvas& t) { legAt(t, ph, 0); }, 0.55f);
  shag(fur, F, 78);
  // the underside in shade: the coat below the chest steps one tone down (volume; the light is from above)
  for (int y = (int)(by + 4); y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const uint32_t v = fur.get(x, y);
      for (int k = 1; k < 5; k++) if (v == F[k]) { if (((x + y) & 1) || y > by + 7) fur.set(x, y, F[std::max(0, k - (y > by + 8 ? 2 : 1))]); break; }
    }
  blit(c, fur, 0, 0);
  // a pale-blue chest with a darker bare patch
  ellipse(c, bx + 3.5f, by + 3.0f, 3.4f, 4.0f, F[1]);
  ellipse(c, bx + 3.0f, by + 2.5f, 2.4f, 3.0f, mix(F[1], S[3], 0.35f));
  // the head: a dark, flat face under a heavy fur brow, small fierce eyes, fangs
  Vec2 hc{bx + 12.0f, by - 7.5f + (frame == 6 ? -2.0f : 0.0f) + (frame == 5 ? 3.0f : 0.0f) + (frame == 4 ? -1.0f : 0.0f)};
  recHead(hc + Vec2{0.5f, -0.5f}, 4.4f);
  recEye(hc + Vec2{2, 0});
  recBody(bx + 0.5f, by, 9.0f, 8.6f);
  layered(c, [&](Canvas& t) {
    Canvas hd(c.w, c.h);
    furBall(hd, hc.x - 1.5f, hc.y - 0.6f, 4.4f, 4.4f, F, 0.35f, 79);    // the shaggy hood of the head
    ball(hd, hc.x + 1.8f, hc.y + 0.8f, 3.2f, 3.0f, S);                  // the bare face
    ball(hd, hc.x + 3.4f, hc.y + 2.0f, 2.0f, 1.6f, S);                  // the muzzle
    furBall(hd, hc.x - 0.4f, hc.y - 3.0f, 3.8f, 1.8f, F, 0.3f, 80);     // the brow ridge of fur
    hline(hd, (int)hc.x + 1, (int)hc.x + 4, (int)hc.y - 1, S[0]);       // the brow's shadow
    if (frame == 6) { hd.set((int)hc.x + 2, (int)hc.y, S[0]); hd.set((int)hc.x + 3, (int)hc.y, S[0]); }
    else { hd.set((int)hc.x + 2, (int)hc.y, rgba(120, 220, 255)); hd.set((int)hc.x + 3, (int)hc.y, kInk); hd.set((int)hc.x + 2, (int)hc.y - 1, S[1]); }
    // mouth: a roar on the wind-up and the slam, shut otherwise (fangs over the lip)
    const bool roar = frame == 4 || frame == 5;
    if (roar) {
      hline(hd, (int)hc.x + 2, (int)hc.x + 4, (int)hc.y + 2, rgba(110, 40, 60));
      hline(hd, (int)hc.x + 2, (int)hc.x + 4, (int)hc.y + 3, rgba(150, 60, 70));
      hd.set((int)hc.x + 2, (int)hc.y + 2, kBone[4]); hd.set((int)hc.x + 4, (int)hc.y + 3, kBone[4]);
    } else {
      hline(hd, (int)hc.x + 2, (int)hc.x + 4, (int)hc.y + 3, S[0]);
      hd.set((int)hc.x + 3, (int)hc.y + 3, kBone[4]);
    }
    hd.set((int)hc.x + 4, (int)hc.y + 1, S[0]);   // nostril
    shag(hd, F, 81);
    blit(t, hd, 0, 0);
  }, 0.6f);
  // the near arm in front of everything; on the slam, shards of ice burst from under the fists
  layered(c, [&](Canvas& t) { Canvas a(c.w, c.h); armAt(a, nearHand, 0); shag(a, F, 82); blit(t, a, 0, 0); }, 0.85f);
  if (frame == 5) {
    const Vec2 hit{bx + 13.0f, ground - 1.0f};
    static const float dx[6] = {-5, -3, 2, 5, 7, 0}, dy[6] = {-3, -6, -7, -4, -2, -9};
    for (int k = 0; k < 6; k++) {
      const int x = (int)(hit.x + dx[k]), y = (int)(hit.y + dy[k]);
      c.set(x, y, kSnow[4]); c.set(x + 1, y, kSnow[2]); c.set(x, y + 1, kCrystal[3]);
    }
  }
}

// ------------------------------------------------------------------ will-o'-wisp (a floating light; no outline)
void wisp(Canvas& c, int frame) {
  const Ramp G = ramp5(rgba(40, 110, 140), rgba(70, 172, 192), rgba(130, 228, 230), rgba(204, 255, 246), rgba(255, 255, 255));
  const bool dead = frame == 7;
  const float cx = c.w * 0.5f, cy = c.h * 0.5f;
  recRamp(G);
  {
    const float yb = dead ? c.h - 3.0f : cy + (frame < 4 ? std::sin(frame * PI * 0.5f) * 0.8f : 0.0f);
    recHead(V(cx, yb), dead ? 2.5f : 3.4f);
    recBody(cx, yb, 3.6f, 3.6f);
  }
  if (dead) {   // guttered out: a few fading motes on the ground
    for (int k = 0; k < 7; k++) {
      const uint32_t h = hash3(k, 7, 5151);
      const int x = (int)(cx - 6 + (h % 12)), y = c.h - 2 - (int)((h >> 8) % 4);
      c.set(x, y, withA(G[2 + (k & 1)], 120 + (int)((h >> 16) % 80)));
    }
    glowDisc(c, cx, c.h - 3.0f, 3.0f, G[1], 70);
    return;
  }
  static const float coreR[7] = {3.0f, 3.4f, 3.2f, 3.6f, 4.0f, 3.4f, 2.4f};
  static const float halo[7] = {8.0f, 8.6f, 8.2f, 8.8f, 9.5f, 10.0f, 6.5f};
  const float k = frame == 6 ? 0.55f : 1.0f;
  const float bob = frame < 4 ? std::sin(frame * PI * 0.5f) * 0.8f : 0.0f;
  const float y0 = cy + bob;
  // the trailing flame: a teardrop of light streaming back (left) and up, swaying frame to frame
  const float sway = frame < 4 ? std::sin(frame * PI * 0.5f + 1.0f) * 1.2f : 0.0f;
  for (int i = 0; i < 6; i++) {
    const float t = i / 5.0f;
    glowDisc(c, cx - 1.5f - t * 4.0f + sway * t, y0 - 1.0f - t * 7.0f, (4.0f - t * 2.4f), G[2], (int)(190 * k * (1 - t * 0.55f)));
  }
  glowDisc(c, cx, y0, halo[frame], G[0], (int)(90 * k));
  glowDisc(c, cx, y0, halo[frame] * 0.6f, G[1], (int)(150 * k));
  // the body of the light: a shaded orb, lit top-left like everything else, its heart white-hot
  Canvas orb(c.w, c.h);
  ball(orb, cx, y0, coreR[frame] + 0.8f, coreR[frame] + 1.0f, G, 0.1f, 1);
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) if (solid(orb, x, y)) c.set(x, y, withA(orb.get(x, y), (int)(255 * (frame == 6 ? 0.7f : 1.0f))));
  glowDisc(c, cx - 0.6f, y0 - 0.8f, coreR[frame] * 0.75f, G[4], 255);
  c.set((int)(cx - 1.4f), (int)(y0 - 2.0f), kWhite);
  // motes circling it (and drawn in toward it on the wind-up, flung out on the strike)
  for (int m = 0; m < 4; m++) {
    float a = m * (TAU / 4) + frame * 0.7f;
    float r = frame == 4 ? 5.0f : frame == 5 ? 9.0f : 7.0f + (m & 1);
    const int x = (int)(cx + std::cos(a) * r), y = (int)(y0 + std::sin(a) * r * 0.6f);
    c.set(x, y, withA(G[3], (int)(220 * k)));
  }
  if (frame == 5)   // the bolt leaves it: rays of light forward
    for (int r = 4; r < 9; r++) { c.set((int)cx + r, (int)y0, withA(G[4 - (r > 6)], 230 - r * 12)); c.set((int)cx + r - 1, (int)y0 - (r & 1), withA(G[2], 140)); }
}

// ------------------------------------------------------------------ blightspawn (blighted land, dark forest)
void blightspawn(Canvas& c, int frame) {
  const Ramp B = ramp5(rgba(30, 20, 32), rgba(56, 36, 52), rgba(88, 58, 72), rgba(122, 88, 96), rgba(160, 124, 124));
  const Ramp T = ramp5(rgba(34, 26, 30), rgba(62, 48, 44), rgba(98, 78, 62), rgba(140, 116, 88), rgba(184, 160, 124));
  const uint32_t glow = rgba(196, 236, 96), glowD = rgba(120, 170, 60), pus = rgba(160, 84, 160);
  const float ground = c.h - 2.0f;
  const bool dead = frame == 7;
  float bx = c.w * 0.42f, by = ground - 11.0f;
  if (frame < 4) by += (frame & 1) ? -0.5f : 0.5f;
  if (frame == 4) { bx -= 1.5f; by -= 1.0f; }
  if (frame == 5) { bx += 2.0f; by += 1.0f; }
  if (frame == 6) bx -= 1.0f;
  const float ph = (float)frame * PI * 0.5f;
  recRamp(B);
  recRamp(T);
  if (dead) {
    recHead(V(bx + 4, ground - 3.5f), 2.5f);
    recEye(V(bx + 4, ground - 3));
    recBody(bx - 1, ground - 3.5f, 6.0f, 3.2f);
    // a heap of twisted roots and thorns, the glow gone out of it
    for (int i = 0; i < 6; i++) {
      const uint32_t h = hash3(i, 3, 6161);
      const float x0 = bx - 9 + (h % 14), y0 = ground - 1.5f - (float)((h >> 8) % 3);
      capsule(c, V(x0, y0), V(x0 + 4 + (h >> 12) % 4, y0 - 1.0f + ((h >> 16) % 3)), 1.2f, 0.7f, B, -1);
    }
    furBall(c, bx - 1, ground - 3.5f, 6.0f, 3.2f, B, 0.3f, 91);
    for (int k = 0; k < 4; k++) { const int x = (int)bx - 5 + k * 3; c.set(x, (int)ground - 7 + (k & 1), T[3]); c.set(x, (int)ground - 6 + (k & 1), T[1]); }
    c.set((int)bx + 4, (int)ground - 3, glowD);
    return;
  }
  // crooked legs (bent back at the knee, root-like feet)
  auto legAt = [&](Canvas& t, float phase, int bias) {
    Vec2 h{bx - 1.0f, by + 4.0f};
    Vec2 f{bx + std::cos(phase) * 2.6f, ground - std::max(0.0f, std::sin(phase)) * 1.4f};
    Vec2 k = (h + f) * 0.5f + Vec2{1.8f, 0.0f};
    capsule(t, h, k, 1.8f, 1.3f, B, bias);
    capsule(t, k, f, 1.2f, 0.9f, B, bias);
    t.set((int)f.x + 1, (int)f.y, T[1 + bias]); t.set((int)f.x + 2, (int)f.y, T[2 + bias]); t.set((int)f.x - 1, (int)f.y, B[1 + bias]);
  };
  // the long arm: root-thin, three thorn claws
  auto armAt = [&](Canvas& t, Vec2 hand, int bias) {
    Vec2 s{bx + 3.0f, by - 3.5f};
    Vec2 e = (s + hand) * 0.5f + Vec2{-1.0f, 1.0f};
    capsule(t, s, e, 1.6f, 1.2f, B, bias);
    capsule(t, e, hand, 1.2f, 0.9f, B, bias);
    const Vec2 d = norm(hand - e);
    const Vec2 n{-d.y, d.x};
    for (int k = -1; k <= 1; k++) {
      const Vec2 tip = hand + d * 2.6f + n * (k * 1.3f);
      line(t, (int)hand.x, (int)hand.y, (int)tip.x, (int)tip.y, T[2 + bias]);
      px(t, tip, T[4 + bias]);
    }
  };
  Vec2 farHand{bx + 6 - std::cos(ph) * 1.6f, by + 5.0f};
  Vec2 nearHand{bx + 7 + std::cos(ph) * 1.6f, by + 6.0f};
  if (frame == 4) { nearHand = V(bx - 5, by - 9); farHand = V(bx + 3, by + 3); }
  if (frame == 5) { nearHand = V(bx + 13, by + 4); farHand = V(bx + 6, by + 5); }
  if (frame == 6) nearHand = V(bx + 4, by - 6);
  armAt(c, farHand, -1);
  legAt(c, ph + PI, -1);
  // the bent torso and the great hump of the back
  furBall(c, bx, by + 0.5f, 5.0f, 5.0f, B, 0.35f, 92);
  furBall(c, bx - 2.5f, by - 2.0f, 5.4f, 4.0f, B, 0.35f, 93);
  // thorns out of the hump, raked back (bristling on the wind-up)
  static const float tdx[5] = {-5.5f, -3.0f, -0.5f, 2.0f, -6.5f}, tdy[5] = {-5.0f, -7.0f, -7.5f, -6.0f, -1.5f};
  for (int k = 0; k < 5; k++) {
    const Vec2 b0{bx - 1.0f + tdx[k] * 0.7f, by - 2.0f + tdy[k] * 0.45f};
    const float len = 3.5f + (k & 1) + (frame == 4 ? 1.5f : 0.0f);
    const Vec2 tip = b0 + norm(Vec2{-0.8f + k * 0.15f, -1.0f}) * len;
    capsule(c, b0, tip, 1.0f, 0.2f, T, 0);
    px(c, tip, T[4]);
  }
  // sickly growths: glowing pustules on the hump
  static const float gx[3] = {-4.5f, -1.0f, -6.5f}, gy[3] = {-4.0f, -1.5f, -0.5f};
  for (int k = 0; k < 3; k++) {   // (one lit, two dull: a pair of bright dots would read as a second face)
    const int x = (int)(bx + gx[k]), y = (int)(by + gy[k]);
    const uint32_t a = k == 0 ? glowD : pus;
    c.set(x, y, k == 0 ? shade(glowD, 0.8f) : a); c.set(x + 1, y, shade(a, 0.7f)); c.set(x, y + 1, shade(a, 0.55f));
  }
  // dripping root tendrils under the belly
  for (int k = 0; k < 3; k++) {
    const int x = (int)bx - 2 + k * 2, y0 = (int)(by + 4.5f);
    const int n = 2 + (int)((hash3(k, frame, 71) % 2));
    vline(c, x, y0, y0 + n, B[1]);
    c.set(x, y0 + n + 1, k == 1 ? glowD : B[0]);
  }
  layered(c, [&](Canvas& t) { legAt(t, ph, 0); }, 0.55f);
  // the head: low and forward, a hollow face with two green-lit eyes and a jagged maw
  Vec2 hc{bx + 6.0f, by - 1.0f + (frame == 6 ? -2.0f : 0.0f) + (frame == 4 ? -1.0f : 0.0f) + (frame == 5 ? 1.0f : 0.0f)};
  recHead(hc, 3.6f);
  recEye(hc + Vec2{1, -1});
  recBody(bx - 1.0f, by - 0.5f, 5.5f, 5.0f);
  layered(c, [&](Canvas& t) {
    furBall(t, hc.x, hc.y, 3.6f, 3.3f, B, 0.2f, 94);
    ball(t, hc.x + 2.2f, hc.y + 1.2f, 2.0f, 1.7f, B, 0.1f, -1);
    capsule(t, hc + Vec2{-1.0f, -2.0f}, hc + Vec2{-3.0f, -4.5f}, 0.9f, 0.2f, T, 0);   // a thorn horn
    if (frame == 6) { t.set((int)hc.x + 1, (int)hc.y - 1, B[0]); t.set((int)hc.x + 3, (int)hc.y - 1, B[0]); }
    else { t.set((int)hc.x + 1, (int)hc.y - 1, glow); t.set((int)hc.x + 3, (int)hc.y - 1, glow); t.set((int)hc.x + 2, (int)hc.y - 1, kInk); }
    const bool open = frame == 4 || frame == 5;
    hline(t, (int)hc.x + 1, (int)hc.x + 3, (int)hc.y + 1, open ? rgba(60, 20, 40) : B[0]);
    if (open) { hline(t, (int)hc.x + 1, (int)hc.x + 3, (int)hc.y + 2, rgba(90, 30, 50)); t.set((int)hc.x + 2, (int)hc.y + 1, T[4]); t.set((int)hc.x + 3, (int)hc.y + 2, T[4]); }
    else t.set((int)hc.x + 2, (int)hc.y + 1, T[3]);
  }, 0.6f);
  layered(c, [&](Canvas& t) { armAt(t, nearHand, 0); }, 0.65f);
}

}  // namespace


// =====================================================================================================
// M6 Steel (BEASTS lane): the harpy and the golem, in the classic monsters' style (side on facing right, the tops of
// backs and heads lit from the top-left, 5-step ramps, soft layered contours, outline())
// =====================================================================================================
namespace {

// ------------------------------------------------------------------ harpy (sea cliffs, crags, badlands, alpine meadows)
// A winged woman-bird: wings for arms (a bony leading edge, warm coverts, long barred primaries fanning from the
// wrist), a buff feathered breast, feathered thighs over scaly yellow shanks and black talons, a pale fierce face under
// a mane of wild dark hair streaming back. 0-3 the wingbeat (up, down-stroke, bottom, folding recovery), 4 rears back
// with the wings high for the swoop, 5 the dive (pitched forward, wings swept, talons thrust out), 6 hurt, 7 fallen.
const Ramp kHarpyFe = ramp5(rgba(46, 28, 36), rgba(84, 50, 44), rgba(130, 80, 54), rgba(174, 122, 72), rgba(212, 168, 106));
const Ramp kHarpyBu = ramp5(rgba(104, 74, 66), rgba(156, 116, 88), rgba(200, 162, 120), rgba(228, 200, 156), rgba(246, 232, 200));
// (M6 finish fixer, review: "the harpy reads as a brown smear at 1x, no clear face or torso") the woman's skin is a
// pale, cool ramp well clear of the buff breast feathers, and her hair a violet-black mane that frames the face against
// the brown wings, so head and shoulders separate from the bird at 16 px
const Ramp kHarpySk = ramp5(rgba(112, 76, 92), rgba(176, 130, 136), rgba(226, 186, 176), rgba(244, 216, 202), rgba(255, 240, 230));
const Ramp kHarpyHr = ramp5(rgba(16, 10, 26), rgba(34, 20, 48), rgba(58, 32, 76), rgba(92, 54, 108), rgba(140, 96, 150));
const Ramp kHarpyLg = ramp5(rgba(92, 54, 30), rgba(150, 96, 38), rgba(204, 146, 56), rgba(234, 190, 90), rgba(250, 224, 150));
const uint32_t kTalon = rgba(34, 26, 36);

// one wing from the shoulder S: th the leading edge's angle (degrees up from pointing straight back), L the span,
// fold 0 (spread) .. 1 (folded), bias the ramp step (-1 for the far wing)
void harpyWing(Canvas& t, Vec2 S, float thDeg, float L, float fold, int bias, float side = -1.0f) {
  const Ramp& Fe = kHarpyFe;
  const float th = thDeg * PI / 180.0f;
  const Vec2 d{side * std::cos(th), -std::sin(th)};   // along the leading edge: out to the side (side -1 left) and up
  const Vec2 n = side < 0 ? Vec2{d.y, -d.x} : Vec2{-d.y, d.x};   // the trailing side (below the arm)
  const Vec2 E = S + norm(d - n * 0.45f) * (L * 0.30f);
  const Vec2 W = E + d * (L * (0.30f - fold * 0.06f));
  // primaries: the long dark flight feathers fanning back from the wrist, pale bars across them
  for (int i = 4; i >= 0; i--) {
    const Vec2 dir = norm(d * (1.0f - i * 0.17f) + n * (0.10f + i * (0.30f - fold * 0.12f)));
    const float len = L * (0.58f - i * 0.05f) * (1.0f - fold * 0.42f);
    const Vec2 tip = W + dir * len;
    capsule(t, W, tip, 1.3f, 0.55f, Fe, bias - (i & 1), 0.0f);
    for (float s = 3.0f; s < len - 1.5f; s += 3.0f) px(t, W + dir * s, Fe[3 + bias]);
    px(t, tip, Fe[0]);
  }
  // secondaries: a row of broad feathers along the forearm and the upper arm, hanging to the trailing side
  for (int k = 5; k >= 0; k--) {
    const float s = 0.12f + k * 0.17f;
    const Vec2 b = s < 0.5f ? S + (E - S) * (s * 2.0f) : E + (W - E) * ((s - 0.5f) * 2.0f);
    const Vec2 dir = norm(n + d * (0.15f + s * 0.2f));
    const float len = L * 0.36f * (1.0f - fold * 0.45f) * (0.75f + 0.25f * s);
    capsule(t, b, b + dir * len, 1.5f, 0.8f, Fe, bias - 1 + (k & 1), 0.0f);
    px(t, b + dir * len, Fe[1 + bias]);
  }
  // the coverts: a warm band over the arm, scalloped
  const Vec2 ca = S + n * (L * 0.17f), cb = E + n * (L * 0.21f), cc = W + n * (L * 0.15f);
  poly(t, {S, E, W, cc, cb, ca}, Fe[3 + bias]);
  for (int k = 0; k < 6; k++) {
    const float s = (k + 0.5f) / 6.0f;
    const Vec2 a0 = s < 0.5f ? S + (E - S) * (s * 2.0f) : E + (W - E) * ((s - 0.5f) * 2.0f);
    const Vec2 a1 = s < 0.5f ? ca + (cb - ca) * (s * 2.0f) : cb + (cc - cb) * ((s - 0.5f) * 2.0f);
    px(t, a1 - n * 0.6f, Fe[2 + bias]);
    px(t, (a0 + a1) * 0.5f, Fe[2 + bias] == Fe[3 + bias] ? Fe[1] : Fe[2 + bias]);
  }
  // the bony leading edge, lit along its top
  capsule(t, S, E, 1.6f, 1.3f, Fe, bias + 1, 0.0f);
  capsule(t, E, W, 1.3f, 1.0f, Fe, bias + 1, 0.0f);
  px(t, E - n * 0.8f, Fe[4 + bias]);
  px(t, (S + E) * 0.5f - n * 0.9f, Fe[4 + bias]);
  px(t, W + d * 0.5f, Fe[3 + bias]);
}

void harpy(Canvas& c, int frame) {
  const Ramp &Fe = kHarpyFe, &Bu = kHarpyBu, &Sk = kHarpySk, &Hr = kHarpyHr, &Lg = kHarpyLg;
  const bool dead = frame == 7;
  const float ground = c.h - 2.0f;
  static const float bob[8] = {0, 1, 2, 1, -1, 1, 0, 0};
  static const float tiltT[8] = {0.12f, 0.16f, 0.20f, 0.14f, -0.30f, 0.85f, -0.24f, 1.40f};
  static const float wingT[8] = {58, 12, -30, 26, 66, 22, 44, 8};
  static const float foldT[8] = {0.0f, 0.0f, 0.1f, 0.45f, 0.0f, 0.55f, 0.35f, 0.1f};
  float cx = c.w * 0.5f - 1.0f, cy = 17.0f + bob[frame];
  if (frame == 5) { cx += 3.0f; cy += 1.0f; }
  if (frame == 6) cx -= 1.5f;
  if (dead) { cx = c.w * 0.5f + 1.0f; cy = ground - 3.5f; }
  const float tilt = tiltT[frame];
  const Vec2 C{cx, cy};
  const Vec2 u{std::sin(tilt), -std::cos(tilt)};   // the body's up (toward the head)
  const Vec2 r{-u.y, u.x};                          // its front
  const Vec2 T = C + u * 1.5f;                      // the breast
  const Vec2 Hp = C - u * 3.2f;                     // the hips
  const Vec2 hf = norm(r + Vec2{1.0f, 0.0f});       // where the face looks
  const Vec2 Hd = C + u * 7.4f + hf * 1.0f;         // the head
  const Vec2 S = T + u * 2.6f - r * 0.3f;           // the wings' shoulders (the back, between the blades)
  const float L = 14.0f;
  const bool swept = frame == 5;                    // the dive: both wings swept back
  recHead(Hd, 2.6f);
  recBody(C.x, C.y, 3.6f, 4.6f);
  recRamp(Fe);
  recRamp(Bu);

  // far wing, behind everything
  // the far (left, back) wing, behind everything
  harpyWing(c, S - r * 1.2f, wingT[frame] + (swept ? 14.0f : 4.0f), L * 0.95f, foldT[frame], -1, -1.0f);
  // the mane of hair streaming behind the head (swaying with the wingbeat; back and up in the dive)
  static const float flowX[8] = {-1.0f, -1.0f, -1.0f, -1.0f, -0.65f, -1.0f, -0.9f, -0.4f};
  static const float flowY[8] = {0.35f, 0.5f, 0.2f, 0.55f, 0.85f, -0.4f, 0.6f, 0.9f};
  const Vec2 flow = norm(Vec2{flowX[frame], flowY[frame]});
  const Vec2 fn{-flow.y, flow.x};
  // (M6 fixer r4, review: "the harpy's face still barely reads at 1x") a lighter mane: three locks, shorter, from
  // behind the crown, so the hair no longer outweighs the face
  for (int k = 0; k < 3; k++) {
    const Vec2 b = Hd - hf * 1.6f + u * (1.0f - k * 0.9f);
    const float len = 4.6f + k * 0.6f;
    const float wave = std::sin(k * 1.3f + frame * 1.57f) * 1.1f;
    const Vec2 mid = b + flow * (len * 0.5f) + fn * wave;
    const Vec2 tip = b + flow * len + fn * (-wave * 0.6f + (k - 2) * 0.5f);
    capsule(c, b, mid, 1.4f, 1.1f, Hr, (k & 1) ? -1 : 0, 0.0f);
    capsule(c, mid, tip, 1.1f, 0.45f, Hr, (k & 1) ? -1 : 0, 0.0f);
    px(c, mid - fn * 0.6f, Hr[3]);
  }
  // the tail: a fan of four long feathers from the hips
  {
    const Vec2 td = norm(Vec2{0, 0} - u - r * 0.55f);
    const Vec2 tn{-td.y, td.x};
    for (int k = 0; k < 4; k++) {
      const Vec2 dir = norm(td + tn * ((k - 1.5f) * 0.2f));
      const float len = 6.8f - std::fabs(k - 1.5f) * 0.9f;
      capsule(c, Hp, Hp + dir * len, 1.6f, 0.8f, Fe, k == 0 ? -1 : 0, 0.0f);
      px(c, Hp + dir * (len * 0.6f), Fe[3]);
      px(c, Hp + dir * len, Fe[0]);
    }
  }
  // the legs: feathered thighs, scaly shanks, three talons forward and one back (spread open to strike)
  auto leg = [&](Canvas& t, Vec2 H, int bias, float lagX) {
    Vec2 K, A;
    const float sw = std::sin(frame * PI * 0.5f) * 0.7f;
    switch (frame) {
      case 4: K = H + Vec2{3.0f, 0.6f}; A = K + Vec2{2.6f, -1.4f}; break;
      case 5: K = H + Vec2{2.4f, 2.6f}; A = K + Vec2{3.2f, 2.0f}; break;
      case 6: K = H + Vec2{0.4f, 3.4f}; A = K + Vec2{-1.6f, 2.8f}; break;
      case 7: K = H + Vec2{0.6f, -2.6f}; A = K + Vec2{1.8f, -1.6f}; break;
      default: K = H + Vec2{1.2f + sw + lagX, 3.2f}; A = K + Vec2{-1.0f + sw * 0.5f, 3.0f}; break;
    }
    capsule(t, H, K, 1.9f, 1.3f, Fe, bias, 0.0f);
    capsule(t, K + (A - K) * 0.25f, A, 0.8f, 0.7f, Lg, bias, 0.0f);
    const bool open = frame == 4 || frame == 5;
    const Vec2 toes[4] = {open ? Vec2{2.5f, -0.8f} : Vec2{2.0f, 0.7f}, open ? Vec2{2.7f, 1.2f} : Vec2{1.3f, 1.8f},
                          open ? Vec2{0.8f, 2.5f} : Vec2{0.2f, 2.0f}, open ? Vec2{-1.7f, 0.3f} : Vec2{-1.4f, 0.9f}};
    for (const Vec2& o : toes) {
      line(t, (int)std::floor(A.x), (int)std::floor(A.y), (int)std::floor(A.x + o.x * 0.8f), (int)std::floor(A.y + o.y * 0.8f), Lg[2 + bias]);
      px(t, A + o, kTalon);
    }
    px(t, A, Lg[3 + bias]);
  };
  const Vec2 hip = Hp + r * 0.6f + Vec2{0, 1.0f};
  leg(c, hip + Vec2{-1.4f, -0.6f}, -1, -0.8f);
  // the body: the back's brown feathers, the hips' feathered trousers, the buff breast with scalloped plumage
  capsule(c, Hp + u * 0.5f, T + u * 1.6f, 3.1f, 3.5f, Fe, 0, 0.06f);
  ball(c, Hp.x, Hp.y, 3.0f, 2.7f, Fe, 0.08f);
  layered(c, [&](Canvas& t) {
    capsule(t, Hp + u * 1.0f + r * 1.0f, T + u * 1.2f + r * 1.1f, 2.1f, 2.7f, Bu, 0, 0.06f);
    for (int y = 0; y < t.h; y++)
      for (int x = 0; x < t.w; x++) {
        const uint32_t v = t.get(x, y);
        if (!chA(v) || ((x + (y >> 1) * 2) & 3) != 0 || !(y & 1)) continue;
        for (int k = 1; k < 5; k++) if (v == Bu[k]) { t.set(x, y, Bu[k - 1]); break; }
      }
  }, 0.4f);
  // (M6 fixer r3, review: "the harpy reads as an owl or a hawk") the woman in the bird: bare pale shoulders and a
  // breast band of skin over the feathers, so the torso reads as a person's at 1x
  layered(c, [&](Canvas& t) {
    capsule(t, T + u * 0.2f + r * 1.3f, T + u * 2.4f + r * 0.9f, 2.3f, 2.6f, Sk, 0, 0.05f);   // (fixer r4: broader)
    px(t, T + u * 2.2f + r * 0.2f, Sk[4]);                       // the lit shoulder
    px(t, T + u * 0.4f + r * 2.4f, Sk[1]);                       // the shade under the breast
    capsule(t, T - u * 0.4f + r * 0.2f, T - u * 0.3f + r * 2.6f, 0.7f, 0.7f, Bu, -1, 0.0f);   // the feather line below
  }, 0.5f);
  layered(c, [&](Canvas& t) { leg(t, hip, 0, 0.0f); }, 0.6f);
  // the near wing over the body: spread out to the front (right), or swept back alongside the far one in the dive
  layered(c, [&](Canvas& t) {
    if (swept) harpyWing(t, S + r * 0.8f + u * 0.5f, wingT[frame] - 6.0f, L * 0.9f, foldT[frame], 0, -1.0f);
    else harpyWing(t, S + r * 1.2f, wingT[frame] - (dead ? 16.0f : 0.0f), L * 0.92f, foldT[frame], 0, 1.0f);
  }, 0.55f);
  // neck, a ruff of feathers at its root, the head
  layered(c, [&](Canvas& t) {
    capsule(t, T + u * 2.6f, Hd - u * 1.0f, 1.4f, 1.2f, Sk, 0, 0.0f);
    capsule(t, T + u * 2.4f - r * 1.2f, T + u * 2.6f + r * 1.4f, 1.5f, 1.4f, Bu, 0, 0.0f);
    px(t, T + u * 3.4f - r * 0.5f, Bu[4]);
    ball(t, Hd.x, Hd.y, 3.8f, 3.6f, Sk, 0.05f);   // (fixer r4: a bigger head)
    capsule(t, Hd + hf * 1.3f + Vec2{0, 0.2f}, Hd + hf * 3.2f + Vec2{0, 1.0f}, 1.4f, 0.8f, Sk, 0, 0.0f);   // the jaw and nose
    // (fixer r4) the lit plane of the face, the palest skin on the beast, so it reads against the brown wings at 1x
    { const Vec2 fp = Hd + hf * 1.5f + Vec2{0, -0.2f}; ball(t, fp.x, fp.y, 1.9f, 2.0f, Sk, 0.0f); px(t, fp + Vec2{-0.5f, -1.0f}, Sk[4]); }
    // the hair's crown, swept back off the brow so the pale face shows under it
    furBall(t, Hd.x - hf.x * 2.4f, Hd.y - 2.2f, 2.1f, 1.4f, Hr, 0.15f, 101);
    px(t, Hd + hf * 1.1f + Vec2{0, -2.2f}, Hr[2]);
    px(t, Hd + Vec2{-0.4f, -2.8f}, Hr[4]);
    // the face: a fierce gold eye under a dark brow, a mouth that opens to shriek on the swoop and the dive
    const Vec2 e = Hd + hf * 1.1f + Vec2{0, -0.5f};
    if (frame == 6 || dead) { px(t, e, Sk[0]); px(t, e + Vec2{-1, 0}, Sk[1]); }
    else { px(t, e, kInk); px(t, e + hf * 0.9f, rgba(255, 200, 70)); px(t, e + Vec2{0, -1}, Hr[0]); px(t, e + hf * 0.9f + Vec2{0, -1}, Hr[1]); }
    const Vec2 m = Hd + hf * 2.0f + Vec2{0, 1.5f};
    if (frame == 4 || frame == 5) { px(t, m, rgba(110, 28, 46)); px(t, m + Vec2{0, 1}, rgba(150, 50, 60)); px(t, m + Vec2{-1, 0}, rgba(110, 28, 46)); }
    else px(t, m, Sk[0]);
  }, 0.6f);
  recEye(Hd + hf * 1.1f + Vec2{0, -0.5f});
  // the near wing, over the body
  if (dead) {   // a few loose feathers on the ground
    for (int k = 0; k < 3; k++) {
      const int x = (int)cx - 9 + k * 7, y = (int)ground - (k & 1);
      c.set(x, y, Fe[2]); c.set(x + 1, y, Fe[3]); c.set(x + 2, y - 1, Fe[2]);
    }
  }
}

// ------------------------------------------------------------------ golem (ruins, mountains, crystal barrens)
// Stacked, chiselled blocks with a glowing core rune: a broad chest (its top face catching the light in the 3/4 view), a
// small head sunk between the shoulders with two burning eye slits, boulder shoulders, arms of stacked blocks to great
// fists that hang to the ground, stumpy legs. 0-3 the heavy stomp, 4 both fists raised over the head, 5 the ground
// slam (debris and dust kicked up), 6 hurt (chips fly, the rune flickers), 7 collapsed into rubble. The material
// (art::golemMaterial of the look's tint) is painted, not washed: stone (lichen, cracks), crystal (facets and glints),
// clay (smoothed, incised glyphs), iron (riveted plates, a furnace heart).
struct GolemMat {
  Ramp R;
  uint32_t rune, runeHi;
  int kind;   // 0 stone, 1 crystal, 2 clay, 3 iron
};
GolemMat golemMat(int kind) {
  GolemMat M;
  M.kind = kind;
  switch (kind) {
    case 1:
      M.R = ramp5(rgba(46, 38, 100), rgba(70, 78, 166), rgba(102, 136, 212), rgba(156, 198, 238), rgba(230, 246, 255));
      M.rune = rgba(255, 110, 210); M.runeHi = rgba(255, 220, 248);
      break;
    case 2:
      M.R = ramp5(rgba(78, 38, 38), rgba(128, 66, 46), rgba(176, 104, 64), rgba(210, 146, 92), rgba(236, 190, 134));
      M.rune = rgba(255, 186, 64); M.runeHi = rgba(255, 244, 186);
      break;
    case 3:
      M.R = ramp5(rgba(32, 32, 44), rgba(58, 60, 76), rgba(94, 98, 114), rgba(142, 146, 160), rgba(204, 208, 216));
      M.rune = rgba(255, 112, 36); M.runeHi = rgba(255, 226, 140);
      break;
    default:
      M.R = ramp5(rgba(52, 48, 64), rgba(90, 84, 92), rgba(130, 122, 118), rgba(168, 160, 148), rgba(204, 196, 178));
      M.rune = rgba(86, 214, 255); M.runeHi = rgba(220, 255, 255);
      break;
  }
  return M;
}

// a chiselled block: centre, half extents, rotation (its local v axis points down the screen at ang 0); bevelled edges
// lit from the top-left, a top face `topH` px deep (the 3/4 view looks down on it), chipped corners, the material's
// surface (stone speckle and a crack, crystal facets, smooth clay, riveted iron)
void gblock(Canvas& t, const GolemMat& M, Vec2 ctr, float hw, float hh, float ang, uint32_t seed, int bias, float topH) {
  const float ca = std::cos(ang), sa = std::sin(ang);
  const float ext = hw + hh + 1.5f;
  Vec2 fs[5];
  for (int i = 0; i < 5; i++) fs[i] = Vec2{(hashf(i, 1, seed) * 2 - 1) * hw * 0.9f, (hashf(i, 2, seed) * 2 - 1) * hh * 0.9f};
  const float noiseAmt = M.kind == 0 ? 0.26f : M.kind == 2 ? 0.10f : M.kind == 3 ? 0.05f : 0.0f;
  for (int y = (int)std::floor(ctr.y - ext); y <= (int)std::ceil(ctr.y + ext); y++)
    for (int x = (int)std::floor(ctr.x - ext); x <= (int)std::ceil(ctr.x + ext); x++) {
      const float dx = x + 0.5f - ctr.x, dy = y + 0.5f - ctr.y;
      const float u = dx * ca + dy * sa, v = -dx * sa + dy * ca;
      if (std::fabs(u) > hw || std::fabs(v) > hh) continue;
      const float el = u + hw, er = hw - u, et = v + hh, eb = hh - v;
      const float corner = std::min(el, er) + std::min(et, eb);
      if (corner < 1.1f + hashf((int)(u > 0) + 2 * (int)(v > 0), 3, seed) * 1.2f) continue;   // chipped corners
      float nx = 0, ny = 0;
      const float bev = 1.4f;
      bool top = false, edge = false;
      if (et < topH) { ny = -0.84f; top = true; }
      else {
        if (el < bev) { nx = -0.66f; edge = true; } else if (er < bev) { nx = 0.70f; edge = true; }
        if (eb < bev) { ny = 0.70f; edge = true; } else if (topH > 0 && et < topH + 1.0f) { ny = 0.35f; edge = true; }   // under the top's lip
      }
      if (M.kind == 1 && !top && !edge) {   // crystal: flat-shaded facets
        int best = 0, second = 0;
        float bd = 1e9f, sd = 1e9f;
        for (int i = 0; i < 5; i++) {
          const float d2 = len2(Vec2{u, v} - fs[i]);
          if (d2 < bd) { sd = bd; second = best; bd = d2; best = i; } else if (d2 < sd) { sd = d2; second = i; }
        }
        (void)second;
        nx = (hashf(best, 4, seed) - 0.5f) * 1.1f;
        ny = (hashf(best, 5, seed) - 0.6f) * 1.0f;
        if (std::sqrt(sd) - std::sqrt(bd) < 0.8f) { t.set(x, y, M.R[(nx + ny < 0 ? 4 : 1) + bias]); continue; }   // a facet edge
      }
      const float snx = nx * ca - ny * sa, sny = nx * sa + ny * ca;
      const float l = lightAt(clampf(snx, -0.95f, 0.95f), clampf(sny, -0.95f, 0.95f)) + (hashf(x / 2, y, seed) - 0.5f) * noiseAmt;
      t.set(x, y, M.R[lightIndex(l, x, y, 0.06f) + bias]);
    }
  auto at = [&](float u, float v) { return ctr + Vec2{u * ca - v * sa, u * sa + v * ca}; };
  const float area = hw * hh;
  if (M.kind == 0 && area > 10) {   // a crack down from the top edge, its lit lip beside it
    float u = (hashf(1, 7, seed) - 0.5f) * hw;
    for (float v = -hh + topH + 0.5f; v < hh * 0.3f; v += 1.0f) {
      u += (hashf((int)(v * 3), 8, seed) - 0.5f) * 1.4f;
      if (std::fabs(u) > hw - 1.5f) break;
      px(t, at(u, v), M.R[0 + bias]);
      if (((int)v & 1) == 0) px(t, at(u - 1, v), M.R[3 + bias]);
    }
  }
  if (M.kind == 2 && area > 12) {   // clay: an incised glyph line and smoothing strokes
    const float v = (hashf(2, 9, seed) - 0.3f) * hh * 0.6f;
    for (float u = -hw + 2; u < hw - 2; u += 1.0f) if (hashf((int)u + 9, 10, seed) > 0.25f) px(t, at(u, v), M.R[1 + bias]);
    px(t, at(-hw + 2.5f, v - 2), M.R[3 + bias]);
    px(t, at(-hw + 3.5f, v - 3), M.R[3 + bias]);
  }
  if (M.kind == 3 && area > 8) {   // iron: rivets in the corners and a plate seam
    for (int k = 0; k < 4; k++) {
      const Vec2 p = at((k & 1) ? hw - 1.8f : -hw + 1.8f, (k & 2) ? hh - 1.8f : -hh + topH + 1.8f);
      px(t, p, M.R[4 + bias]);
      px(t, p + Vec2{1, 1}, M.R[0 + bias]);
    }
    if (hh > 4)
      for (float u = -hw + 1.5f; u < hw - 1.5f; u += 1.0f) { px(t, at(u, 0.5f), M.R[0 + bias]); px(t, at(u, -0.5f), M.R[3 + bias]); }
  }
}

// a crystal shard (a prism seen side on: a lit left face, a shaded right face, a pointed tip that glints)
void shard(Canvas& c, const Ramp& R, float x, float y, float h, float w, float lean, bool glint) {
  const Vec2 b0{x - w * 0.5f, y + 1}, b1{x + w * 0.5f, y + 1}, mb{x, y + 1};
  const Vec2 t0{x - w * 0.5f + lean * h, y - h + w * 0.7f}, t1{x + w * 0.5f + lean * h, y - h + w * 0.7f}, tip{x + lean * h, y - h};
  const Vec2 mt{x + lean * h, y - h + w * 0.7f};
  poly(c, {b0, b1, t1, tip, t0}, R[1]);
  poly(c, {b0, mb, mt, tip, t0}, R[3]);
  line(c, (int)std::floor(mb.x), (int)std::floor(mb.y - 1), (int)std::floor(mt.x), (int)std::floor(mt.y), R[2]);
  px(c, tip + Vec2{-0.3f, 0.6f}, R[4]);
  if (glint) {
    const Vec2 g = tip + Vec2{-0.3f, 1.2f};
    glowSet(c, (int)std::floor(g.x), (int)std::floor(g.y), kWhite);
  }
}

void golem(Canvas& c, int frame, int kind) {
  const GolemMat M = golemMat(kind);
  const Ramp& R = M.R;
  const float ground = c.h - 2.0f;
  const bool dead = frame == 7;
  float bx = c.w * 0.45f, by = ground - 21.0f, lean = 0;
  if (frame < 4) by += (frame & 1) ? 1.0f : 0.0f;   // it sinks onto each heavy step
  if (frame == 4) { by -= 1.0f; lean = -0.10f; bx -= 1.0f; }
  if (frame == 5) { by += 3.0f; lean = 0.22f; bx += 2.0f; }
  if (frame == 6) { lean = -0.16f; bx -= 1.5f; }
  recRamp(R);
  const uint32_t runeDim = mix(M.rune, R[0], 0.55f);
  if (dead) {
    // collapsed: a heap of blocks, the head tumbled on top, a last ember of the rune
    struct Blk { float x, y, hw, hh, a; };
    static const Blk heap[] = {{-9, -3, 4.5f, 3.0f, 0.20f}, {-2, -3.5f, 5.0f, 3.5f, -0.10f}, {5, -3, 4.0f, 3.0f, 0.30f},
                               {12, -2.5f, 3.6f, 2.6f, 0.0f}, {-5, -8.5f, 4.6f, 3.2f, 0.15f}, {2, -9, 4.0f, 3.0f, -0.25f}};
    for (int i = 0; i < 6; i++) {
      const Blk& b = heap[i];
      layered(c, [&](Canvas& t) { gblock(t, M, V(bx + b.x, ground + b.y), b.hw, b.hh, b.a, 300 + i, i < 4 ? -1 : 0, 1.5f); }, 0.6f);
    }
    const Vec2 hd = V(bx + 8.5f, ground - 8.0f);
    layered(c, [&](Canvas& t) { gblock(t, M, hd, 3.4f, 3.0f, 0.45f, 310, 0, 1.5f); }, 0.6f);
    c.set((int)hd.x + 1, (int)hd.y, R[0]); c.set((int)hd.x + 2, (int)hd.y + 1, R[0]);
    glowSet(c, (int)bx - 4, (int)ground - 9, runeDim);
    glowSet(c, (int)bx - 3, (int)ground - 9, mix(M.rune, R[1], 0.3f));
    for (int k = 0; k < 5; k++) {   // pebbles
      const uint32_t h = hash3(k, 7, 333);
      const int x = (int)bx - 14 + (int)(h % 30), y = (int)ground - (int)((h >> 8) % 2);
      c.set(x, y, R[(h >> 12) % 2 ? 1 : 2]); c.set(x + 1, y, R[1]);
    }
    recHead(hd, 3.2f);
    recEye(hd + Vec2{1.5f, 0});
    recBody(bx - 1.5f, ground - 6.0f, 10.0f, 5.0f);
    return;
  }
  const float ph = frame * PI * 0.5f;
  auto rot = [&](Vec2 p, Vec2 o) {   // lean the upper body about the waist
    const Vec2 d = p - o;
    return o + Vec2{d.x * std::cos(lean) - d.y * std::sin(lean), d.x * std::sin(lean) + d.y * std::cos(lean)};
  };
  const Vec2 waist{bx - 1.0f, by + 5.0f};
  const Vec2 chest = rot(V(bx, by - 3.0f), waist);
  const Vec2 head = rot(V(bx + 6.0f, by - 11.5f), waist);
  const Vec2 shN = rot(V(bx + 4.5f, by - 6.5f), waist), shF = rot(V(bx - 5.0f, by - 7.5f), waist);
  // where the fists are
  // walking, the great fists swing low, the near one forward of the chest (the rune shows), the far one behind
  Vec2 handN{bx + 10.0f + std::cos(ph) * 2.5f, by + 7.0f - std::max(0.0f, std::sin(ph)) * 1.0f}, handF{bx - 6.0f - std::cos(ph) * 2.5f, by + 6.0f};
  if (frame == 4) { handN = V(bx + 2.0f, by - 18.0f); handF = V(bx + 5.5f, by - 19.0f); }   // (fixer r4: 1.5 px lower, in a taller cell)
  if (frame == 5) { handN = V(bx + 14.5f, ground - 3.6f); handF = V(bx + 11.0f, ground - 4.2f); }
  if (frame == 6) { handN = V(bx + 8.0f, by - 7.0f); handF = V(bx - 6.0f, by + 4.0f); }
  recHead(head, 3.4f);
  recEye(head + Vec2{2.0f, -0.2f});
  recBody(chest.x, chest.y, 8.5f, 7.0f);
  auto arm = [&](Canvas& t, Vec2 sh, Vec2 hand, int bias, uint32_t seed) {
    Vec2 mid = (sh + hand) * 0.5f;
    Vec2 dir = norm(hand - sh), perp{-dir.y, dir.x};
    if (perp.x > 0) perp = perp * -1.0f;   // the elbow bends back
    const Vec2 el = mid + perp * 2.2f;
    auto seg = [&](Vec2 a, Vec2 b, float hw, uint32_t s) {
      const Vec2 dd = b - a;
      const float ang = std::atan2(dd.y, dd.x) - PI * 0.5f;
      gblock(t, M, (a + b) * 0.5f, hw, len(dd) * 0.5f + 1.0f, ang, s, bias, 0.0f);
    };
    seg(sh, el, 2.6f, seed);
    seg(el, hand, 3.0f, seed + 1);
    const Vec2 fd = hand - el;
    gblock(t, M, hand + norm(fd) * 1.0f, 3.7f, 3.1f, std::atan2(fd.y, fd.x) - PI * 0.5f, seed + 2, bias, 1.2f);
    gblock(t, M, sh, 4.6f, 4.0f, 0.25f + lean, seed + 3, bias, 1.8f);   // the boulder of the shoulder
  };
  auto legAt = [&](Canvas& t, float hx, float phase, int bias, uint32_t seed) {
    const Vec2 hip{hx, by + 7.5f};
    Vec2 foot{hx + 1.0f + std::cos(phase) * 3.0f, ground - 1.6f - std::max(0.0f, std::sin(phase)) * 2.0f};
    if (frame == 5) foot.x += 1.5f;
    if (frame == 4) foot.x -= 1.0f;
    const Vec2 knee = (hip + foot) * 0.5f + Vec2{1.0f, 0};
    auto seg = [&](Vec2 a, Vec2 b, float hw, uint32_t s) {
      const Vec2 dd = b - a;
      gblock(t, M, (a + b) * 0.5f, hw, len(dd) * 0.5f + 1.0f, std::atan2(dd.y, dd.x) - PI * 0.5f, s, bias, 0.0f);
    };
    seg(hip, knee, 3.1f, seed);
    seg(knee, foot + Vec2{0, -1}, 3.4f, seed + 1);
    gblock(t, M, foot + Vec2{1.2f, 0.0f}, 4.6f, 1.7f, 0.0f, seed + 2, bias, 1.0f);
  };
  // far arm and leg, the torso, the near leg, the head, the near arm
  arm(c, shF, handF, -1, 200);
  legAt(c, bx - 4.0f, ph + PI, -1, 210);
  gblock(c, M, rot(V(bx - 1.0f, by + 5.5f), waist), 6.4f, 3.2f, lean * 0.5f, 220, -1, 0.0f);   // the pelvis
  layered(c, [&](Canvas& t) { gblock(t, M, chest, 8.6f, 6.6f, lean, 221, 0, 2.6f); }, 0.6f);
  // the core rune on the chest: a lozenge glyph, its light spilling over the stone round it
  {
    const Vec2 rc = rot(V(bx + 1.0f, by - 2.5f), waist);
    const int rx = (int)std::floor(rc.x), ry = (int)std::floor(rc.y);
    const bool hurt = frame == 6;
    const uint32_t core = hurt ? runeDim : M.runeHi, rim = hurt ? mix(runeDim, R[0], 0.4f) : M.rune;
    for (int y = ry - 4; y <= ry + 4; y++)
      for (int x = rx - 4; x <= rx + 4; x++) {
        const float d = std::hypot(x - rx, y - ry);
        if (d > 4.2f || !solid(c, x, y)) continue;
        c.set(x, y, mix(c.get(x, y), M.rune, hurt ? 0.12f : 0.32f * (1.0f - d / 4.2f)));
      }
    static const int g[5][5] = {{0, 0, 1, 0, 0}, {0, 1, 2, 1, 0}, {1, 2, 2, 2, 1}, {0, 1, 2, 1, 0}, {0, 0, 1, 0, 0}};
    for (int y = 0; y < 5; y++)
      for (int x = 0; x < 5; x++)
        if (g[y][x]) glowSet(c, rx - 2 + x, ry - 2 + y, g[y][x] == 2 && !(x == 2 && y != 2) ? core : rim);
    if (kind == 3)   // iron: the furnace grille over the heart
      for (int x = rx - 3; x <= rx + 3; x += 2) { c.set(x, ry - 3, R[0]); c.set(x, ry + 3, R[0]); }
  }
  // crystal: shards grown out of the back and the shoulders; stone: lichen on the lit tops
  if (kind == 1) {
    const Ramp Cr = ramp5(rgba(70, 50, 140), rgba(110, 96, 210), rgba(150, 160, 240), rgba(200, 220, 252), rgba(250, 252, 255));
    const Vec2 b0 = rot(V(bx - 6.5f, by - 8.5f), waist), b1 = rot(V(bx - 3.0f, by - 9.5f), waist), b2 = rot(V(bx - 8.5f, by - 5.0f), waist);
    shard(c, Cr, b2.x, b2.y, 5.0f, 2.6f, -0.45f, frame % 3 == 2);
    shard(c, Cr, b0.x, b0.y, 8.0f, 3.2f, -0.25f, frame % 3 == 0);
    shard(c, Cr, b1.x, b1.y, 5.5f, 2.6f, 0.05f, frame % 3 == 1);
  }
  if (kind == 0)
    for (int y = 0; y < c.h; y++)
      for (int x = 0; x < c.w; x++) {
        const uint32_t v = c.get(x, y);
        if ((v == R[3] || v == R[4]) && hash3(x, y, 77) % 11 == 0) c.set(x, y, hash3(x, y, 78) & 1 ? rgba(150, 160, 98) : rgba(176, 182, 116));
      }
  layered(c, [&](Canvas& t) { legAt(t, bx + 1.5f, ph, 0, 230); }, 0.6f);
  layered(c, [&](Canvas& t) { arm(t, shN, handN, 0, 250); }, 0.65f);
  // the head, sunk between the shoulders: a heavy brow over two burning slits
  layered(c, [&](Canvas& t) {
    gblock(t, M, head, 3.6f, 3.1f, lean + 0.05f, 240, 0, 1.8f);
    const int hx = (int)std::floor(head.x), hy = (int)std::floor(head.y);
    hline(t, hx, hx + 3, hy - 1, R[0]);
    const uint32_t eye = frame == 6 ? runeDim : M.runeHi;
    glowSet(t, hx + 1, hy, eye); glowSet(t, hx + 3, hy, eye); glowSet(t, hx + 2, hy, M.rune);
  }, 0.6f);
  if (frame == 5) {   // the slam: rubble and dust thrown up round the fists
    static const float dx[7] = {-4, -2, 1, 3, 6, 8, 0}, dy[7] = {-2, -6, -8, -5, -3, -7, -10};
    for (int k = 0; k < 7; k++) {
      const int x = (int)(handN.x + dx[k]), y = (int)(ground - 2 + dy[k]);
      c.set(x, y, R[k & 1 ? 3 : 2]); c.set(x + 1, y, R[1]); if (k < 3) c.set(x, y + 1, R[1]);
    }
    for (int k = 0; k < 9; k++) {
      const uint32_t h = hash3(k, 5, 909);
      const int x = (int)handN.x - 8 + (int)(h % 18), y = (int)ground - 1 - (int)((h >> 8) % 4);
      c.set(x, y, withA(mix(R[4], rgba(214, 190, 150), 0.5f), 90));
    }
  }
  if (frame == 6)   // chips knocked off the shoulder
    for (int k = 0; k < 3; k++) { const int x = (int)shN.x - 6 - k * 2, y = (int)shN.y - 4 - (k & 1) * 2; c.set(x, y, R[3]); c.set(x + 1, y, R[1]); }
}

}  // namespace

// =====================================================================================================
// M6 Steel (BEASTS lane): variant looks. The species' painter runs with the anchor recorder on; the hide is recoloured
// by ramp (the tint); the overlays are painted on the frame at its anchors (so horns sit on the head in every pose, and
// the plates ride the back as it walks); then the usual outline. Emissive pixels also go on a glow sheet.
// =====================================================================================================
namespace {

// the great wyrm (MO_GREAT lurker): 1.75x the lurker's painting in its own cell
constexpr int kGreatLurkerW = 78, kGreatLurkerH = 30;
constexpr float kGreatLurkerS = 1.75f;

const Ramp kHornR = ramp5(rgba(46, 34, 42), rgba(86, 70, 68), rgba(136, 118, 100), rgba(188, 172, 142), rgba(234, 226, 200));

// one cell of a species' sheet (no outline); golemKind: the golem's material
Canvas paintCell(Monster m, int f, int golemKind, bool great = false) {
  great = great && m == Monster::Lurker;
  const int cw = great ? kGreatLurkerW : monsterCellW(m), chh = great ? kGreatLurkerH : monsterCellH(m);
  Canvas cell(cw, chh);
  switch (m) {
    case Monster::Wolf: case Monster::IceWolf: {
      Quad q;
      bool ice = m == Monster::IceWolf;
      q.fur = ice ? ramp5(rgba(92, 108, 150), rgba(146, 166, 200), rgba(198, 214, 232), rgba(230, 240, 248), rgba(255, 255, 255))
                  : ramp5(rgba(44, 40, 60), rgba(78, 74, 90), rgba(118, 112, 118), rgba(158, 150, 146), rgba(198, 190, 176));
      q.belly = ice ? kSnow : ramp5(rgba(110, 96, 96), rgba(160, 146, 132), rgba(204, 192, 170), rgba(226, 218, 196), rgba(244, 238, 220));
      q.eye = ice ? rgba(110, 220, 255) : rgba(250, 200, 60);
      q.nose = kInk;
      q.bodyLen = 13; q.bodyR = 4.2f; q.legLen = 7; q.legW = 2.4f; q.headR = 3.4f; q.snout = 4.0f;
      q.ears = 0; q.tail = 0; q.saddle = true; q.seed = ice ? 7 : 3;
      quadruped(cell, q, f);
      break;
    }
    case Monster::Boar: {
      Quad q;
      q.fur = ramp5(rgba(46, 30, 38), rgba(78, 50, 46), rgba(112, 74, 56), rgba(148, 104, 72), rgba(182, 140, 98));
      q.belly = ramp5(rgba(80, 56, 52), rgba(122, 88, 70), rgba(160, 120, 92), rgba(190, 152, 116), rgba(214, 184, 146));
      q.eye = rgba(240, 90, 60); q.nose = rgba(196, 120, 120);
      q.bodyLen = 12; q.bodyR = 5.0f; q.legLen = 4.5f; q.legW = 2.4f; q.headR = 3.6f; q.snout = 3.0f;
      q.ears = 0; q.tail = 1; q.tusks = true; q.mane = true; q.seed = 5;
      quadruped(cell, q, f);
      break;
    }
    case Monster::Bear: {
      Quad q;
      q.fur = ramp5(rgba(46, 28, 36), rgba(80, 46, 40), rgba(120, 72, 48), rgba(158, 104, 64), rgba(194, 142, 92));
      q.belly = ramp5(rgba(80, 50, 44), rgba(118, 78, 56), rgba(150, 104, 70), rgba(180, 134, 90), rgba(206, 166, 116));
      q.eye = rgba(40, 20, 24); q.nose = kInk;
      q.bodyLen = 16; q.bodyR = 6.5f; q.legLen = 6.5f; q.legW = 3.6f; q.headR = 4.4f; q.snout = 3.0f;
      q.ears = 1; q.tail = 2; q.hump = true; q.seed = 9;
      quadruped(cell, q, f);
      break;
    }
    case Monster::Slime: slime(cell, f); break;
    case Monster::Spider: spider(cell, f, false); break;
    case Monster::FrostSpider: spider(cell, f, true); break;
    case Monster::Bat: bat(cell, f); break;
    case Monster::Skeleton: cell = humanoidCell(kSkeletonSp, f); break;
    case Monster::Draugr: cell = humanoidCell(kDraugrSp, f); break;
    case Monster::Goblin: cell = humanoidCell(kGoblinSp, f); break;
    case Monster::Troll: troll(cell, f); break;
    case Monster::Wraith: wraith(cell, f); break;
    case Monster::Mudcrab: mudcrab(cell, f); break;
    case Monster::Sandworm: sandworm(cell, f); break;
    case Monster::Dragon: dragon(cell, f); break;
    // ---- M3c Wildlands wildlife (LIFE lane)
    case Monster::Scorpion: scorpion(cell, f); break;
    case Monster::Hyena: hyena(cell, f); break;
    case Monster::EmberHound: emberHound(cell, f); break;
    case Monster::Lurker: lurker(cell, f, great ? kGreatLurkerS : 1.0f); break;
    case Monster::Yeti: yeti(cell, f); break;
    case Monster::Wisp: wisp(cell, f); break;
    case Monster::Blightspawn: blightspawn(cell, f); break;
    // ---- M6 Steel (BEASTS lane)
    case Monster::Harpy: harpy(cell, f); break;
    case Monster::Golem: golem(cell, f, golemKind); break;
    default: break;
  }
  return cell;
}

// the humanoid rig's monsters (art_human.cpp paints them): the head is found on the figure itself (the topmost skin /
// bone / helm rows over the torso column; the weapon is ignored by keeping to the figure's middle columns)
void humanoidAnchors(const Canvas& c, int frame, Anchors& A) {
  const int mid = c.w / 2;
  if (frame == 7) {   // lying: the rig's dead frame lies along the ground, head at the right end
    int x1 = -1, yTop = c.h;
    for (int x = c.w - 1; x >= 0 && x1 < 0; x--)
      for (int y = 0; y < c.h; y++) if (chA(c.get(x, y)) == 255) { x1 = x; break; }
    for (int x = 0; x < c.w; x++)
      for (int y = 0; y < c.h; y++) if (chA(c.get(x, y)) == 255) { yTop = std::min(yTop, y); break; }
    A.head = true; A.hx = x1 - 3.0f; A.hy = c.h - 5.0f; A.hr = 2.8f;
    A.eye = true; A.ex = A.hx + 1; A.ey = A.hy - 1;
    A.body = true; A.bx = c.w * 0.5f; A.by = c.h - 4.5f; A.brx = 7.0f; A.bry = std::max(2.5f, (c.h - yTop) * 0.4f);
    return;
  }
  int top = -1;
  for (int y = 0; y < c.h && top < 0; y++)
    for (int x = mid - 3; x <= mid + 2; x++) if (chA(c.get(x, y)) == 255) { top = y; break; }
  if (top < 0) return;
  float sx = 0;
  int n = 0;
  for (int y = top; y < top + 5; y++)
    for (int x = mid - 5; x <= mid + 4; x++) if (chA(c.get(x, y)) == 255) { sx += x; n++; }
  const float hx = n ? sx / n + 0.5f : (float)mid;
  A.head = true; A.hx = hx; A.hy = top + 3.4f; A.hr = 3.2f;
  A.eye = true; A.ex = hx + 1.5f; A.ey = top + 3.0f;
  A.body = true; A.bx = hx - 0.5f; A.by = top + 10.5f; A.brx = 3.6f; A.bry = 4.2f;
}

int backEnd(const Anchors& A) {
  float x1 = A.bx + A.brx * 0.85f;
  if (A.head && A.hx > A.bx + 1.0f) x1 = std::min(x1, A.hx - A.hr * 1.05f);
  return (int)std::floor(x1);
}
// the top of the back at column x: the body ellipse's top, snapped to the first painted pixel near it (-1 none)
int backTop(const Canvas& c, const Anchors& A, int x) {
  const float dx = (x + 0.5f - A.bx) / std::max(1.0f, A.brx);
  if (std::fabs(dx) > 1.0f) return -1;
  const int yE = (int)std::floor(A.by - A.bry * std::sqrt(std::max(0.0f, 1.0f - dx * dx)));
  for (int y = yE - 2; y <= yE + 3; y++) if (y >= 0 && chA(c.get(x, y)) == 255) return y;
  return -1;
}

// the hide: pixels whose hue matches one of the painter's body ramps (eyes, teeth, claws, fire are left alone)
std::vector<int8_t> bodyMask(const Canvas& c, const Recorder& R, std::vector<int8_t>& stepOut) {
  std::vector<int8_t> m(c.px.size(), -1);
  stepOut.assign(c.px.size(), 2);
  if (!R.nBody) return m;
  auto chroma = [](uint32_t p, float out[3]) {
    const float l = luma(p) * 255.0f;
    out[0] = chR(p) - l; out[1] = chG(p) - l; out[2] = chB(p) - l;
  };
  float rc[4][5][3], rl[4][5];
  for (int j = 0; j < R.nBody; j++)
    for (int k = 0; k < 5; k++) { chroma(R.body[j].c[k], rc[j][k]); rl[j][k] = luma(R.body[j].c[k]); }
  for (size_t i = 0; i < c.px.size(); i++) {
    const uint32_t p = c.px[i];
    if (chA(p) < 200) continue;
    float cp[3];
    chroma(p, cp);
    const float lp = luma(p);
    float best = 1e9f;
    int bj = -1, bk = 2;
    for (int j = 0; j < R.nBody; j++)
      for (int k = 0; k < 5; k++) {
        const float d0 = cp[0] - rc[j][k][0], d1 = cp[1] - rc[j][k][1], d2 = cp[2] - rc[j][k][2];
        const float dc = d0 * d0 + d1 * d1 + d2 * d2;
        const float dl = (lp - rl[j][k]) * 255.0f;
        const float score = dc + dl * dl * 0.35f;
        if (dc < 26.0f * 26.0f && score < best) { best = score; bj = j; bk = k; }
      }
    if (bj >= 0) { m[i] = (int8_t)bj; stepOut[i] = (int8_t)bk; }
  }
  return m;
}
uint32_t atLuma(uint32_t col, float L) {
  const float lc = std::max(0.02f, luma(col));
  const float f = L / lc;
  float r = chR(col) * f, g = chG(col) * f, b = chB(col) * f;
  const float over = std::max(r, std::max(g, b)) - 255.0f;
  if (over > 0) { const float w = std::min(1.0f, over / 255.0f); r = lerpf(std::min(r, 255.0f), 255, w * 0.6f); g = lerpf(std::min(g, 255.0f), 255, w * 0.6f); b = lerpf(std::min(b, 255.0f), 255, w * 0.6f); }
  return rgba(clamp255(r), clamp255(g), clamp255(b));
}
// cast the hide toward the land's tint (M6 fixer): a regional variant is the SAME beast weathered by its land, so the
// species' own palette stays (a white yeti stays white, an ember hound stays black and lava): each hide pixel moves
// a third of the way toward the tint ramp's colour at its own ramp step and its own luminance, and the luminance
// shifts only a little toward the tint's. The modelling, fur breakup and contours survive; the family still reads.
void tintCell(Canvas& c, const Recorder& R, const std::vector<int8_t>& mask, const std::vector<int8_t>& step, uint32_t tint) {
  const Ramp T = ramp(tint, 1.0f);
  const float gain = clampf(lerpf(1.0f, luma(tint) / std::max(0.05f, luma(R.body[0].c[2])), 0.12f), 0.85f, 1.15f);
  const float k = 0.34f;
  for (size_t i = 0; i < c.px.size(); i++) {
    if (mask[i] < 0) continue;
    const uint32_t p = c.px[i];
    const float L = luma(p) * gain;
    const uint32_t t = atLuma(T[step[i]], L);
    const uint32_t keep = atLuma(p, L);
    c.px[i] = withA(rgba(clamp255(lerpf((float)chR(keep), (float)chR(t), k)), clamp255(lerpf((float)chG(keep), (float)chG(t), k)),
                         clamp255(lerpf((float)chB(keep), (float)chB(t), k))),
                    chA(p));
  }
}

// ---- the overlays
void ovHorn(Canvas& t, const Anchors& A, bool nearSide, int bias) {
  // a horn swept back from the brow, rising then curving down to a pale point (a ram's sweep, side on)
  const float s = clampf(A.hr / 3.0f, 0.8f, 1.8f);
  const Vec2 B{A.hx - A.hr * 0.05f + (nearSide ? 0.0f : 1.4f * s), A.hy - A.hr * 0.72f + (nearSide ? 0.0f : -0.3f)};
  const Vec2 p0 = B, p1 = B + Vec2{-0.8f, -2.8f} * s, p2 = B + Vec2{-3.8f, -3.9f} * s, p3 = B + Vec2{-5.8f, -2.0f} * s;
  Vec2 prev = p0;
  const int N = 10;
  for (int i = 1; i <= N; i++) {
    const float u = (float)i / N, w = 1 - u;
    const Vec2 p = p0 * (w * w * w) + p1 * (3 * w * w * u) + p2 * (3 * w * u * u) + p3 * (u * u * u);
    const float ra = lerpf(1.25f, 0.3f, (float)(i - 1) / N) * s, rb = lerpf(1.25f, 0.3f, u) * s;
    capsule(t, prev, p, std::max(0.5f, ra), std::max(0.45f, rb), kHornR, bias + 1, 0.0f);
    if (i % 3 == 2 && i < N - 1) px(t, p + Vec2{0.3f, 0.6f}, kHornR[1 + bias]);   // the growth ridges
    prev = p;
  }
  px(t, p3, kHornR[4 + bias]);
}
void ovSpikes(Canvas& c, const Anchors& A) {
  if (!A.body) return;
  const float s = clampf(A.bry / 4.5f, 0.6f, 1.8f);
  const int x0 = (int)std::ceil(A.bx - A.brx * 0.8f), x1 = backEnd(A) - 1;
  if (x1 <= x0) return;
  const int stepX = std::max(3, (int)std::lround(3.4f * s));
  Canvas sp(c.w, c.h);
  for (int x = x0; x <= x1; x += stepX) {
    const int y = backTop(c, A, x);
    if (y < 0) continue;
    const float tt = (float)(x - x0) / std::max(1, x1 - x0);
    const float h = (1.8f + 2.2f * std::sin(tt * PI * 0.85f + 0.3f)) * s;
    const Vec2 b0{x - 1.0f * s, y + 1.4f}, b1{x + 1.2f * s, y + 1.4f}, tip{x - 0.5f * h, y - h};
    poly(sp, {b0, b1, tip}, kHornR[1]);
    line(sp, (int)std::floor(b0.x + 0.5f), (int)std::floor(b0.y - 0.5f), (int)std::floor(tip.x), (int)std::floor(tip.y), kHornR[3]);
    line(sp, (int)std::floor(b1.x - 0.5f), (int)std::floor(b1.y - 0.5f), (int)std::floor(tip.x + 0.5f), (int)std::floor(tip.y + 0.5f), kHornR[0]);
    px(sp, tip, kHornR[4]);
  }
  layered(c, [&](Canvas& t) { blit(t, sp, 0, 0); }, 0.5f);
}
void ovMoss(Canvas& c, const Anchors& A, uint32_t seed) {
  if (!A.body) return;
  const int x0 = (int)std::floor(A.bx - A.brx * 0.95f), x1 = backEnd(A);
  for (int x = x0; x <= x1; x++) {
    const int y = backTop(c, A, x);
    if (y < 0) continue;
    const float n = vnoise(x * 0.45f, 0.5f, seed);
    const float edge = std::min(x - x0, x1 - x) / 2.0f;
    const int thick = std::min((int)edge + 1, 1 + (n > 0.42f) + (n > 0.66f));
    for (int k = 0; k <= thick; k++)
      if (chA(c.get(x, y + k)) == 255) c.set(x, y + k, k == 0 ? kMoss[(hash3(x, y, seed) % 3) ? 3 : 4] : kMoss[k == thick ? 1 : 2]);
    if (n > 0.58f && edge >= 1) { c.set(x, y - 1, kMoss[3]); if (n > 0.74f) c.set(x, y - 2, kMoss[4]); }
    if (hash3(x, 2, seed) % 9 == 0 && edge >= 1) {   // a trailing strand
      int yb = y + thick + 1;
      for (int k = 0; k < 2; k++) if (chA(c.get(x, yb + k)) == 255) c.set(x, yb + k, kMoss[1]);
    }
  }
  // lichen flecks over the lower flanks
  for (int y = (int)A.by; y <= (int)(A.by + A.bry); y++)
    for (int x = (int)(A.bx - A.brx); x <= (int)(A.bx + A.brx); x++)
      if (chA(c.get(x, y)) == 255 && hash3(x, y, seed + 5) % 17 == 0) c.set(x, y, kMoss[(x + y) & 1 ? 2 : 3]);
}
void ovRime(Canvas& c, const Anchors& A, uint32_t seed) {
  Canvas src = c;
  // frost settles on every upward face: the top pixel of each column, a dusting under it
  for (int x = 0; x < c.w; x++)
    for (int y = 0; y < c.h; y++) {
      if (chA(src.get(x, y)) < 200) continue;
      c.set(x, y, hash3(x, y, seed) % 3 ? kSnow[3] : kSnow[4]);
      if (chA(src.get(x, y + 1)) == 255 && (hash3(x, y, seed + 1) & 1)) c.set(x, y + 1, mix(src.get(x, y + 1), kSnow[2], 0.55f));
      break;
    }
  // icicles hanging under the belly (not off the legs: a column that runs on to the ground is a leg)
  if (A.body) {
    for (int x = (int)std::ceil(A.bx - A.brx * 0.8f); x <= (int)(A.bx + A.brx * 0.8f); x++) {
      if (hash3(x, 3, seed) % 3 == 0) continue;
      int y = (int)A.by;
      if (chA(src.get(x, y)) < 200) continue;
      while (y < c.h - 1 && chA(src.get(x, y + 1)) >= 200) y++;
      if (y > A.by + A.bry + 1.5f || y >= c.h - 3) continue;
      const int n = 1 + (int)(hash3(x, 4, seed) % 3);
      for (int k = 1; k <= n; k++) c.set(x, y + k, k == n ? kSnow[4] : k == 1 ? kSnow[2] : kCrystal[3]);
    }
  }
  // a frost sparkle or two on the hide
  for (int k = 0; k < 3; k++) {
    const uint32_t h = hash3(k, 9, seed);
    const int x = (int)(A.bx - A.brx * 0.6f) + (int)(h % (uint32_t)std::max(1.0f, A.brx * 1.2f)), y = (int)(A.by - A.bry * 0.3f) + (int)((h >> 8) % 3);
    if (chA(c.get(x, y)) == 255) c.set(x, y, kWhite);
  }
}
void ovCrystal(Canvas& c, const Anchors& A, int frame) {
  if (!A.body) return;
  const float s = clampf(A.brx / 6.0f, 0.6f, 1.7f);
  static const float fx[3] = {-0.45f, 0.05f, -0.8f}, hs[3] = {6.5f, 5.0f, 4.0f}, ln[3] = {-0.25f, 0.10f, -0.45f};
  const int x1 = backEnd(A);
  for (int k = 2; k >= 0; k--) {
    const int x = (int)std::floor(A.bx + A.brx * fx[k]);
    if (x > x1) continue;
    const int y = backTop(c, A, x);
    if (y < 0) continue;
    shard(c, kCrystal, (float)x + 0.5f, (float)y + 1.0f, hs[k] * s, std::max(2.2f, 2.8f * s), ln[k], frame % 3 == k);
  }
}
void ovEmber(Canvas& c, const std::vector<int8_t>& mask, int frame, uint32_t seed) {
  Canvas src = c;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const size_t i = (size_t)y * c.w + x;
      const bool hide = !mask.empty() ? mask[i] >= 0 : chA(src.px[i]) == 255;
      if (!hide) continue;
      // (M6 fixer r3, review: "burning turns a golem into orange mush at 1x") a few broad glowing veins, not a speckle:
      // a low-frequency field, thin hot cores, a charred rim beside them
      const float n = vnoise(x * 0.15f, y * 0.21f, seed) * 0.88f + vnoise(x * 0.5f, y * 0.55f, seed + 1) * 0.12f;
      const float ridge = std::fabs(n - 0.5f);
      if (ridge < 0.016f) glowSet(c, x, y, ridge < 0.007f ? kFire[3 + (int)(hash3(x, y, seed + frame) % 2)] : kFire[2]);
      else if (ridge < 0.034f) c.set(x, y, darken(src.px[i], 0.3f));
    }
}
// (M6 fixer round 2, review: "plates on reptiles and four-legged beasts are still a flat grey bar") a beast on all
// fours wears a CAPARISON of iron lames fitted to its back: every column of the back takes iron from its own top
// contour down a depth that tapers to rounded ends (so the armour rides the spine's curve, over the shoulders and the
// haunch, and its lower edge runs parallel to the back instead of a straight line), cut across into lames that overlap
// toward the tail. Each lame is a little barrel: lit along the spine and on its forward (left, toward the light) lip,
// falling through the iron ramp down the flank to a dark lower rim, its trailing edge a shadowed seam over the next
// lame, rivets along the spine. The whole caparison is lighter at the top-left end and darker at the far end, its lower
// edge is a leather trim with brass rivets, it casts a shadow on the hide, and a girth strap with a buckle holds it.
// The head stays bare.
void ovPlates(Canvas& c, const Anchors& A, const std::vector<int8_t>& mask, const std::vector<int8_t>& step, uint32_t seed) {
  if (!A.body) return;
  const bool useMask = !mask.empty() && std::any_of(mask.begin(), mask.end(), [](int8_t v) { return v >= 0; });
  auto hide = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= c.w || y >= c.h) return false;
    const size_t i = (size_t)y * c.w + x;
    return useMask ? mask[i] >= 0 : chA(c.px[i]) == 255;
  };
  auto headAt = [&](int x, int y) {
    if (!A.head) return false;
    const float hx = (x + 0.5f - A.hx) / (A.hr * 1.15f), hy = (y + 0.5f - A.hy) / (A.hr * 1.15f);
    return hx * hx + hy * hy < 1.0f;
  };
  auto stepAt = [&](int x, int y) -> int {
    const size_t i = (size_t)y * c.w + x;
    if (useMask && !step.empty()) return std::clamp((int)step[i], 0, 4);
    return std::clamp((int)std::floor(luma(c.px[i]) * 5.5f), 0, 4);
  };
  const int x0 = std::max(0, (int)std::floor(A.bx - A.brx * 0.80f)), x1 = std::min(c.w - 1, (int)std::ceil(A.bx + A.brx * 0.62f));
  if (x1 - x0 < 3) return;
  const float mid = (x0 + x1) * 0.5f, half = std::max(1.0f, (x1 - x0) * 0.5f);
  const int yTop = std::max(0, (int)std::floor(A.by - A.bry * 2.4f)), yLow = std::min(c.h - 1, (int)std::ceil(A.by + A.bry * 0.55f));
  const int depthMax = std::max(3, (int)std::lround(A.bry * 1.15f));
  std::vector<int8_t> dep(c.px.size(), -1);
  std::vector<int8_t> colD(c.w, 0);   // each column's own depth (its shading runs top to bottom of it)
  for (int x = x0; x <= x1; x++) {
    int top = -1;
    for (int y = yTop; y <= yLow; y++)
      if (hide(x, y) && !headAt(x, y)) { top = y; break; }
    if (top < 0) continue;
    const float u = (x + 0.5f - mid) / half;
    const int d = std::max(2, (int)std::lround(depthMax * std::sqrt(std::max(0.0f, 1.0f - u * u * 0.8f)))) + (int)(hash3(x, 5, seed) % 2u);
    int n = 0;
    for (int y = top; y <= yLow && y - top < d; y++) {
      if (!hide(x, y) || headAt(x, y)) break;
      dep[(size_t)y * c.w + x] = (int8_t)(y - top);
      n++;
    }
    colD[x] = (int8_t)n;
  }
  auto in = [&](int x, int y) { return x >= 0 && y >= 0 && x < c.w && y < c.h && dep[(size_t)y * c.w + x] >= 0; };
  const int lw = std::max(3, (int)std::lround(A.brx * 0.42f));   // the width of one lame
  const Canvas src = c;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      if (!in(x, y)) continue;
      const int d = dep[(size_t)y * c.w + x], n = std::max(1, (int)colD[x]);
      const float v = n > 1 ? (float)d / (float)(n - 1) : 0.0f;   // 0 on the spine, 1 at the lower rim
      const float u = (x + 0.5f - mid) / half;
      // the barrel: lit on the spine, through the mid tones down the flank
      int k = v < 0.22f ? 3 : v < 0.55f ? 2 : 1;
      if (u < -0.45f && k < 3) k++;                       // the near end, toward the light
      if (u > 0.55f) k = std::max(1, k - 1);              // the far end turns away
      if (stepAt(x, y) == 0 && k > 1) k--;                // the beast's deep shadow (the underside) darkens it too
      const int lx = (x - x0) % lw;
      const bool lip = lx == 0 && x > x0, seam = lx == lw - 1 && x < x1;
      if (lip) k = std::min(4, k + 1);                    // the lame's lit forward lip
      else if (seam) k = std::max(0, k - 1);              // its trailing edge, shadowed over the next lame
      if (d == 0) k = std::min(4, k + (u < 0.4f ? 2 : 1));   // the spine catches the light
      if (!hide(x - 1, y) && d > 0) k = std::min(4, k + 1);  // the lit near edge
      c.set(x, y, kIron[k]);
      // rivets down the spine, one a lame
      if (d == 1 && lx == lw / 2 && n >= 3) { c.set(x, y, kIron[4]); if (in(x, y + 1)) c.set(x, y + 1, kIron[0]); }
      // the lower edge: a leather trim, a brass rivet on each lame
      if (!in(x, y + 1) && n >= 3) c.set(x, y, lx == lw / 2 ? kBrass[3] : kLeather[(x & 1) ? 1 : 2]);
    }
  // the cast shadow on the hide under the caparison's lower edge (light from the top left)
  for (int y = 1; y < c.h; y++)
    for (int x = 1; x < c.w; x++) {
      if (in(x, y) || !hide(x, y) || headAt(x, y)) continue;
      if (in(x, y - 1) || in(x - 1, y - 1)) c.set(x, y, darken(src.get(x, y), 0.38f));
    }
  // a girth strap round the belly with a brass buckle, behind the forelegs
  const int sx = (int)std::floor(A.bx + A.brx * 0.25f);
  int ys = -1;
  for (int y = 0; y < c.h; y++) if (in(sx, y)) ys = y;
  if (ys >= 0)
    for (int y = ys + 1; y < c.h && hide(sx, y) && y <= A.by + A.bry + 1; y++) {
      c.set(sx, y, kLeather[2]);
      if (hide(sx + 1, y)) c.set(sx + 1, y, kLeather[1]);
      if (y == ys + 2) { c.set(sx, y, kBrass[4]); if (hide(sx + 1, y)) c.set(sx + 1, y, kBrass[2]); }
    }
}
// (M6 finish fixer, review must: "the plates are still a flat grey half-dome pasted on a troll's / golem's chest") an
// upright beast (troll, golem, yeti, the humanoids) is not a barrel: its armour is a MANTLE fitted to its own outline.
// Every column of the upper body takes iron from its top contour down a fixed depth, so the pauldrons cap the shoulders
// and the upper arms, a gorget closes under the chin and the lames run parallel to the silhouette (they bow over each
// shoulder instead of lying in flat hoops). Each pixel keeps the hide's modelling (its ramp step) blended with the
// mantle's own top-left light; the lames have lit lips, shaded undersides and rivets, the mantle's lower edge casts a
// shadow on the hide. No belly strap (on a standing beast it ran down between the legs like a stick).
void ovPlatesUpright(Canvas& c, const Anchors& A, const std::vector<int8_t>& mask, const std::vector<int8_t>& step, uint32_t seed) {
  if (!A.body) return;
  const bool useMask = !mask.empty() && std::any_of(mask.begin(), mask.end(), [](int8_t v) { return v >= 0; });
  auto hide = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= c.w || y >= c.h) return false;
    const size_t i = (size_t)y * c.w + x;
    return useMask ? mask[i] >= 0 : chA(c.px[i]) == 255;
  };
  auto headAt = [&](int x, int y) {
    if (!A.head) return false;
    const float hx = (x + 0.5f - A.hx) / (A.hr * 1.1f), hy = (y + 0.5f - A.hy) / (A.hr * 1.1f);
    return hx * hx + hy * hy < 1.0f;
  };
  auto stepAt = [&](int x, int y) -> int {
    const size_t i = (size_t)y * c.w + x;
    if (useMask && !step.empty()) return std::clamp((int)step[i], 0, 4);
    return std::clamp((int)std::floor(luma(c.px[i]) * 5.5f), 0, 4);
  };
  const int x0 = std::max(0, (int)std::floor(A.bx - A.brx * 1.45f)), x1 = std::min(c.w - 1, (int)std::ceil(A.bx + A.brx * 1.45f));
  const int yTop = std::max(0, (int)std::floor(A.by - A.bry * 1.6f));
  const int yLow = std::min(c.h - 1, (int)std::floor(A.by + A.bry * 0.25f));   // never below the waist
  const int depth = std::max(4, (int)std::lround(A.bry * 0.95f));
  std::vector<int8_t> dep(c.px.size(), -1);   // the depth under the column's contour, -1 off the mantle
  for (int x = x0; x <= x1; x++) {
    int top = -1;
    for (int y = yTop; y <= yLow; y++)
      if (hide(x, y) && !headAt(x, y)) { top = y; break; }
    if (top < 0) continue;
    const int wob = (int)(hash3(x, 7, seed) % 2);   // a worn, uneven lower edge
    for (int y = top; y <= yLow && y - top < depth + wob; y++) {
      if (!hide(x, y) || headAt(x, y)) break;
      dep[(size_t)y * c.w + x] = (int8_t)(y - top);
    }
  }
  auto in = [&](int x, int y) { return x >= 0 && y >= 0 && x < c.w && y < c.h && dep[(size_t)y * c.w + x] >= 0; };
  auto band = [&](int x, int y) { return dep[(size_t)y * c.w + x] / 3; };
  const Canvas src = c;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      if (!in(x, y)) continue;
      const float dxs = (x + 0.5f - A.bx) / std::max(2.0f, A.brx * 1.3f), dys = (y + 0.5f - A.by) / std::max(2.0f, A.bry * 1.3f);
      const int ks = lightIndex(lightAt(dxs * 0.8f, dys * 0.8f), x, y, 0.0f);
      // the hide's fur or stone grain would speckle the iron: the mantle is shaded by its own curvature alone (the
      // silhouette it follows gives it the beast's form); the hide's step only darkens it where the beast is in deep
      // shadow (the far arm, the underside)
      int k = std::clamp(ks, 1, 3);
      if (stepAt(x, y) == 0 && k > 1) k--;
      const int b = band(x, y);
      const bool lip = dep[(size_t)y * c.w + x] % 3 == 0;          // the lit upper lip of each lame
      const bool under = in(x, y + 1) && band(x, y + 1) != b;        // the shadowed lower edge over the next one
      if (dep[(size_t)y * c.w + x] == 0) k = std::min(4, k + 1);    // the top contour catches the light
      else if (lip) k = std::min(4, k + 1);
      else if (under) k = std::max(0, k - 1);
      if (!in(x, y + 1)) k = std::max(0, k - 1);                     // the mantle's lower rim
      if (!hide(x + 1, y)) k = std::max(0, k - 1);                   // the far (shadow) side turns away
      if (!hide(x - 1, y)) k = std::min(4, k + 1);                   // the lit near edge
      c.set(x, y, kIron[k]);
      if (under && ((x + b * 2 + (int)seed) % 3 == 0) && in(x - 1, y) && in(x + 1, y)) c.set(x, y, kIron[4]);   // rivets
    }
  // the cast shadow on the hide under the mantle (light from the top left)
  for (int y = 1; y < c.h; y++)
    for (int x = 1; x < c.w; x++) {
      if (in(x, y) || !hide(x, y) || headAt(x, y)) continue;
      if (in(x, y - 1) || in(x - 1, y - 1)) c.set(x, y, darken(src.get(x, y), 0.34f));
    }
}
bool uprightBeast(Monster m) {
  return m == Monster::Troll || m == Monster::Golem || m == Monster::Yeti || m == Monster::Skeleton || m == Monster::Draugr ||
         m == Monster::Goblin || m == Monster::Wraith;
}
void ovMask(Canvas& c, const Anchors& A) {
  if (!A.head) return;
  const float mx = A.hx + A.hr * 0.3f, my = A.hy + A.hr * 0.05f, rx = A.hr * 0.9f, ry = A.hr * 0.82f;
  Canvas src = c;
  for (int y = (int)std::floor(my - ry - 1); y <= (int)std::ceil(my + ry + 1); y++)
    for (int x = (int)std::floor(mx - rx - 1); x <= (int)std::ceil(mx + rx + 1); x++) {
      const float dx = (x + 0.5f - mx) / rx, dy = (y + 0.5f - my) / ry, d2 = dx * dx + dy * dy;
      if (d2 > 1.0f) continue;
      if (chA(src.get(x, y)) != 255 && d2 > 0.55f) continue;   // fitted to the head, a little proud of it in the middle
      c.set(x, y, kBone[lightIndex(lightAt(dx * 0.85f, dy * 0.85f), x, y, 0.06f)]);
    }
  const float ex = A.eye ? A.ex : mx + rx * 0.2f, ey = A.eye ? A.ey : my - ry * 0.25f;
  const int sx = (int)std::floor(ex), sy = (int)std::floor(ey);
  c.set(sx, sy, kInk); c.set(sx - 1, sy, mix(kInk, kBone[1], 0.4f)); c.set(sx, sy + 1, kBone[0]);   // the socket
  if (A.hr >= 3.0f) {
    c.set((int)std::floor(mx + rx * 0.62f), (int)std::floor(my + ry * 0.25f), kInk);                // the nose
    const int ty = (int)std::floor(my + ry * 0.7f);
    for (int x = (int)std::floor(mx); x <= (int)std::floor(mx + rx * 0.75f); x++)                     // the teeth
      if (chA(c.get(x, ty)) == 255) c.set(x, ty, (x & 1) ? kInk : kBone[4]);
    c.set((int)std::floor(mx - rx * 0.1f), (int)std::floor(my - ry * 0.75f), kBone[1]);              // a crack
    c.set((int)std::floor(mx - rx * 0.2f), (int)std::floor(my - ry * 0.55f), kBone[1]);
  }
}
void ovEyes(Canvas& c, const Anchors& A, uint32_t col) {
  if (!A.head && !A.eye) return;
  const float ex = A.eye ? A.ex : A.hx + A.hr * 0.4f, ey = A.eye ? A.ey : A.hy - A.hr * 0.2f;
  const int x = (int)std::floor(ex), y = (int)std::floor(ey);
  const uint32_t hot = mix(col, kWhite, 0.6f);
  static const int hx[6] = {-1, 2, 0, 1, 0, 1}, hy[6] = {0, 0, -1, -1, 1, 1};
  for (int k = 0; k < 6; k++) {
    const int qx = x + hx[k], qy = y + hy[k];
    if (chA(c.get(qx, qy)) == 0) {
      c.set(qx, qy, withA(col, 80));
      if (g_rec && g_rec->glow) g_rec->glow->set(qx, qy, withA(col, 150));
    } else c.set(qx, qy, mix(c.get(qx, qy), col, 0.3f));
  }
  glowSet(c, x, y, hot);
  glowSet(c, x + 1, y, col);
}

uint32_t eyeColourFor(const MonsterLook& L) {
  if (L.overlays & MO_EMBER) return rgba(255, 150, 40);
  if (L.overlays & MO_RIME) return rgba(130, 230, 255);
  if (L.overlays & MO_CRYSTAL) return rgba(190, 150, 255);
  if (L.base == Monster::Wraith || L.base == Monster::Draugr) return rgba(120, 240, 220);
  return rgba(255, 64, 48);
}

Canvas lookSheet(const MonsterLook& L, Canvas* glowOut) {
  const int cw = lookCellW(L), chh = lookCellH(L);
  Canvas sheet(cw * MONSTER_FRAMES, chh);
  if (glowOut) *glowOut = Canvas(cw * MONSTER_FRAMES, chh);
  const int gk = L.base == Monster::Golem ? golemMaterial(L.tint) : 0;
  const uint32_t tint = gk ? 0u : L.tint;
  const uint32_t seed = 0x51u + (uint32_t)L.base * 131u;
  for (int f = 0; f < MONSTER_FRAMES; f++) {
    Recorder rec;
    Canvas glow(cw, chh);
    rec.glow = &glow;
    g_rec = &rec;
    Canvas cell = paintCell(L.base, f, gk, (L.overlays & MO_GREAT) != 0);
    if (!rec.a.head && (L.base == Monster::Skeleton || L.base == Monster::Draugr || L.base == Monster::Goblin)) {
      humanoidAnchors(cell, f, rec.a);
      if (L.base == Monster::Skeleton) recRamp(kBone);
      if (L.base == Monster::Goblin) recRamp(ramp5(rgba(46, 70, 40), rgba(80, 112, 50), rgba(124, 160, 72), rgba(168, 196, 98), rgba(206, 226, 140)));
    }
    std::vector<int8_t> step;
    const std::vector<int8_t> mask = bodyMask(cell, rec, step);
    if (tint) tintCell(cell, rec, mask, step, tint);
    const Anchors& A = rec.a;
    const uint16_t o = overlaysFit(L.base, L.overlays);
    if ((o & MO_HORNS) && A.head) {   // the far horn goes behind the head
      Canvas far(cw, chh);
      ovHorn(far, A, false, -1);
      for (size_t i = 0; i < far.px.size(); i++) if (chA(far.px[i]) && !chA(cell.px[i])) cell.px[i] = far.px[i];
    }
    if (o & MO_MOSS) ovMoss(cell, A, seed + 11);
    if (o & MO_EMBER) ovEmber(cell, mask, f, seed + 23);
    if (o & MO_PLATES) {
      if (uprightBeast(L.base)) ovPlatesUpright(cell, A, mask, step, seed + 31);
      else ovPlates(cell, A, mask, step, seed + 31);
    }
    if (o & MO_RIME) ovRime(cell, A, seed + 41);
    if (o & MO_SPIKES) ovSpikes(cell, A);
    if (o & MO_CRYSTAL) ovCrystal(cell, A, f);
    if (o & MO_BONEMASK) ovMask(cell, A);
    if ((o & MO_HORNS) && A.head) layered(cell, [&](Canvas& t) { ovHorn(t, A, true, 0); }, 0.6f);
    if (o & MO_EYES) ovEyes(cell, A, eyeColourFor(L));
    g_rec = nullptr;
    if (L.base != Monster::Wisp) outline(cell);
    place(sheet, cell, f, 0);
    if (glowOut) place(*glowOut, glow, f, 0);
  }
  return sheet;
}

}  // namespace

int monsterCellW(Monster m) {
  switch (m) {
    case Monster::Wolf: case Monster::IceWolf: return 32;
    case Monster::Boar: return 28;
    case Monster::Bear: return 36;
    case Monster::Slime: return 20;
    case Monster::Spider: case Monster::FrostSpider: return 28;
    case Monster::Bat: return 24;
    case Monster::Skeleton: case Monster::Draugr: case Monster::Goblin: return 24;
    case Monster::Troll: return 36;
    case Monster::Wraith: return 24;
    case Monster::Mudcrab: return 26;
    case Monster::Sandworm: return 28;
    case Monster::Dragon: return 80;
    case Monster::Scorpion: return 32;
    case Monster::Hyena: return 32;
    case Monster::Lurker: return 44;
    case Monster::Yeti: return 38;
    case Monster::Wisp: return 20;
    case Monster::EmberHound: return 32;
    case Monster::Blightspawn: return 26;
    case Monster::Harpy: return 34;    // (M6 BEASTS: room for the wingspan)
    case Monster::Golem: return 40;
    default: return 24;
  }
}
int monsterCellH(Monster m) {
  switch (m) {
    case Monster::Wolf: case Monster::IceWolf: return 22;
    case Monster::Boar: return 20;
    case Monster::Bear: return 28;
    case Monster::Slime: return 16;
    case Monster::Spider: case Monster::FrostSpider: return 18;
    case Monster::Bat: return 20;
    case Monster::Skeleton: case Monster::Draugr: case Monster::Goblin: return 24;
    case Monster::Troll: return 34;
    case Monster::Wraith: return 28;
    case Monster::Mudcrab: return 16;
    case Monster::Sandworm: return 30;
    case Monster::Dragon: return 56;
    case Monster::Scorpion: return 24;
    case Monster::Hyena: return 22;
    case Monster::Lurker: return 16;
    case Monster::Yeti: return 38;
    case Monster::Wisp: return 24;
    case Monster::EmberHound: return 22;
    case Monster::Blightspawn: return 28;
    case Monster::Harpy: return 32;
    case Monster::Golem: return 50;    // (the fists rise over the head on the wind-up; fixer r4: 44 clipped them flat)
    default: return 24;
  }
}

Canvas monsterSheet(Monster m) {
  const int cw = monsterCellW(m), chh = monsterCellH(m);
  Canvas sheet(cw * MONSTER_FRAMES, chh);
  for (int f = 0; f < MONSTER_FRAMES; f++) {
    Canvas cell = paintCell(m, f, 0);
    if (m != Monster::Wisp) outline(cell);   // (the wisp is light: a dark rim would put it out)
    place(sheet, cell, f, 0);
  }
  return sheet;
}

int golemMaterial(uint32_t tint) {
  if (!tint) return 0;
  const int r = (int)(tint & 255), g = (int)((tint >> 8) & 255), b = (int)((tint >> 16) & 255);
  const int mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
  if (b > r + 40 && b >= g) return 1;              // crystal
  if (r > b + 50 && r > g + 20) return 2;          // clay
  if (mx - mn < 26 && (r * 3 + g * 6 + b) / 10 < 128) return 3;   // iron
  return 0;                                        // stone (recoloured by the tint)
}

int lookCellW(const MonsterLook& look) {
  return look.base == Monster::Lurker && (look.overlays & MO_GREAT) ? kGreatLurkerW : monsterCellW(look.base);
}
int lookCellH(const MonsterLook& look) {
  return look.base == Monster::Lurker && (look.overlays & MO_GREAT) ? kGreatLurkerH : monsterCellH(look.base);
}

Canvas monsterSheetLook(const MonsterLook& look) {
  if (look.classic()) return monsterSheet(look.base);
  return lookSheet(look, nullptr);
}

bool monsterLookGlows(const MonsterLook& look) {
  return look.base == Monster::Golem || (overlaysFit(look.base, look.overlays) & (MO_EYES | MO_EMBER | MO_CRYSTAL)) != 0;
}
Canvas monsterGlowLook(const MonsterLook& look) {
  Canvas glow;
  lookSheet(look, &glow);
  return glow;
}

}  // namespace art
