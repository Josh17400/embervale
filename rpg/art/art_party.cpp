// M8 Fellowship art (rpg/art/art_party.h). ART lane, phase B: the bonded animal (a wolfhound raised from a pup, a grey
// wolf) and the graves of the fallen. The party portraits are painted in rpg/art/art_party_portrait.cpp.
//
// The hound is built like the M5 village animals (art_critters.cpp): shaded volumes (the haunch, the deep chest, the
// tucked waist, the neck, the long head and its muzzle), tapered two-jointed legs, the ears and the tail, in the game's
// high 3/4 view, lit from the top left, outlined like every sprite. A wolfhound is tall and lean with a rough, wiry coat,
// a long narrow head, small rose ears and a low curved tail; a wolf is heavier in the neck and the ruff, with erect ears,
// a broader muzzle and a bushy tail; a pup has a big head on a round body, short legs, big paws (a hound pup's ears
// flop). Coats and markings by seed (a bib, socks, a saddle, a mask, brindling), the collar by the player's mark. It
// reads as a beloved animal: bright eyes, a wagging tail, a panting tongue at rest and running.
//
// The graves: five rites on a one-tile mound in 3/4 view (a carved headstone, a cairn, a carved post, a standing
// stone, an urn niche), the fallen's weapon laid on it, fresh-turned earth for the first days (a grassed mound after),
// and flowers once some are laid.
#include "rpg/art/art_internal.h"
#include "rpg/culture/culture.h"

namespace art {

namespace {
uint64_t mixKey(uint64_t h, uint64_t v) {
  h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
  return h;
}
inline bool inEl(float px, float py, float cx, float cy, float rx, float ry) {
  const float dx = (px - cx) / rx, dy = (py - cy) / ry;
  return dx * dx + dy * dy <= 1.0f;
}
}  // namespace

uint64_t HoundLook::key() const {
  uint64_t h = 0x40D7ull;
  for (uint64_t v : {(uint64_t)species, (uint64_t)age, (uint64_t)coat, (uint64_t)coat2, (uint64_t)collar, (uint64_t)seed}) h = mixKey(h, v);
  return h;
}
uint64_t GraveLook::key() const {
  uint64_t h = 0x6EA7Eull;
  for (uint64_t v : {(uint64_t)rite, (uint64_t)stone, (uint64_t)weapon, (uint64_t)fresh, (uint64_t)flowers, (uint64_t)seed}) h = mixKey(h, v);
  return h;
}

// ================================================================================================ the hound
namespace {

struct HCoat {
  Ramp a, d, l;       // the main coat, the dark (saddle, mask, ear tips), the light (chest, belly, muzzle, socks)
  bool saddle = false, bib = false, socks = false, mask = false, brindle = false, blaze = false;
};
HCoat houndCoat(const HoundLook& L) {
  HCoat k;
  const uint32_t s = L.seed * 2654435761u + 0x9E37u;
  uint32_t main = L.coat, dark = L.coat2;
  if (L.species == 1) {   // grey wolf coats: grey, timber, pale, black, red
    static const uint32_t W[5][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};
    (void)W;
    switch (L.coat ? 9 : (int)(s % 5)) {
      case 0: main = rgba(138, 134, 128); dark = rgba(74, 70, 74); break;    // grey
      case 1: main = rgba(146, 122, 94); dark = rgba(80, 64, 54); break;     // timber
      case 2: main = rgba(206, 204, 196); dark = rgba(150, 148, 146); break; // pale (arctic)
      case 3: main = rgba(58, 54, 60); dark = rgba(36, 32, 40); break;       // black
      case 4: main = rgba(170, 112, 70); dark = rgba(96, 64, 50); break;     // red
      default: break;
    }
    if (!dark) dark = shade(main, 0.55f);
    k.saddle = true;
    k.bib = true;
    k.mask = false;
    k.l = ramp(mix(main, rgba(236, 230, 214), 0.62f), 0.8f);
  } else {   // wolfhound coats: grey, wheaten, brindle, red fawn, black, cream
    switch (L.coat ? 9 : (int)(s % 6)) {
      case 0: main = rgba(132, 128, 124); dark = rgba(84, 80, 84); break;    // grey
      case 1: main = rgba(196, 164, 112); dark = rgba(130, 100, 70); break;  // wheaten
      case 2: main = rgba(124, 98, 76); dark = rgba(62, 48, 44); k.brindle = true; break;   // brindle
      case 3: main = rgba(178, 112, 66); dark = rgba(104, 62, 44); break;    // red fawn
      case 4: main = rgba(60, 56, 62); dark = rgba(38, 34, 42); break;       // black
      case 5: main = rgba(224, 214, 190); dark = rgba(170, 156, 132); break; // cream
      default: break;
    }
    if (!dark) dark = shade(main, 0.6f);
    k.mask = ((s >> 8) & 3) == 0;
    k.saddle = ((s >> 10) & 3) == 1;
    k.bib = ((s >> 12) & 1) != 0;
    k.l = ramp(mix(main, rgba(240, 232, 214), 0.55f), 0.8f);
  }
  k.socks = ((s >> 14) & 3) == 0;
  k.blaze = ((s >> 16) & 7) == 0;
  if (L.age == 0) {
    // (fixer M8 r1) a pup's soft coat is a shade lighter than the grown dog's, with a pale bib and muzzle: the dark
    // coats no longer vanish into a stone floor or read as debris beside a person
    const int lum = (int)(((main >> 16) & 255) + ((main >> 8) & 255) + (main & 255)) / 3;
    if (lum < 110) { main = mix(main, rgba(150, 138, 124), 0.42f); dark = mix(dark, rgba(96, 86, 80), 0.3f); }
    k.bib = true;
    k.blaze = true;
    k.l = ramp(mix(main, rgba(246, 238, 220), 0.66f), 0.8f);
  }
  k.a = ramp(main, 0.9f);
  k.d = ramp(dark, 0.9f);
  return k;
}

struct HSpec {
  // side view, px at full size; x from the cell centre (+ toward the head), heights above the ground row
  float hauX, hauH, hauRx, hauRy;     // the haunch
  float chX, chH, chRx, chRy;         // the deep chest
  float waist;                        // the waist's radius
  float legTop;                       // where the legs leave the body
  float legR0, legR1;                 // the legs' radius at the top and at the paw
  float frontX, hindX;
  float neckR;
  float headX, headH, headR;          // the skull
  float muzL, muzR, muzDrop;          // the muzzle's length, radius, droop
  float tailR;                        // the tail's thickness (a wolf's brush is thick)
  float noise;                        // the coat's roughness
};
HSpec houndSpec(const HoundLook& L) {
  HSpec s{};
  if (L.species == 1) {   // the wolf: heavier, a thick neck and ruff, erect ears, a brush tail
    s = {-4.6f, 8.6f, 3.2f, 3.0f, 3.4f, 8.8f, 3.9f, 3.8f, 2.6f, 6.6f, 1.25f, 0.75f, 4.4f, -5.0f, 2.5f,
         7.6f, 12.6f, 2.5f, 2.7f, 1.35f, 0.5f, 1.45f, 0.28f};
  } else {                // the wolfhound: tall, lean, the deep chest and the tuck, the long narrow head
    s = {-5.2f, 9.4f, 3.0f, 2.7f, 3.6f, 9.2f, 3.4f, 3.7f, 1.9f, 7.4f, 1.1f, 0.65f, 4.2f, -5.6f, 1.8f,
         8.0f, 14.2f, 2.2f, 2.8f, 1.05f, 0.5f, 0.85f, 0.42f};
  }
  if (L.age == 0) {   // a pup: a big round head, a short muzzle, short legs, a round body
    s.headR *= 1.32f; s.muzL *= 0.55f; s.muzR *= 1.1f; s.legTop *= 0.72f; s.hauRy *= 1.15f; s.chRy *= 1.05f;
    s.waist *= 1.3f; s.hauH *= 0.8f; s.chH *= 0.8f; s.headH *= 0.8f; s.headX *= 0.86f; s.legR1 *= 1.35f; s.noise *= 0.5f;
    s.hauX *= 0.82f; s.chX *= 0.82f; s.frontX *= 0.82f; s.hindX *= 0.82f;
  } else if (L.age == 1) {   // half grown: all legs, a head still a little big
    s.headR *= 1.1f; s.muzL *= 0.85f; s.legR1 *= 1.12f;
  }
  return s;
}
// (fixer M8 r1, review: "the pup is an unreadable dark blob") a pup big enough to read beside a person at 1x
float ageScale(int age) { return age == 0 ? 0.7f : (age == 1 ? 0.84f : 1.0f); }

struct HoundPainter {
  Canvas& c;
  const HoundLook& L;
  HCoat K;
  HSpec S;
  float sc;           // the body's scale (the age)
  int f;              // frame column
  int view;           // 0 down, 1 up, 2 right
  bool wolf;
  HoundPainter(Canvas& c_, const HoundLook& l, int view_, int frame) : c(c_), L(l), f(frame), view(view_) {
    K = houndCoat(L);
    S = houndSpec(L);
    sc = ageScale(L.age);
    wolf = L.species == 1;
  }
  const int W = HOUND_W, H = HOUND_H;
  float gy() const { return H - 2 + 0.5f; }   // the paws stand on row H - 2
  void cap(float x0, float y0, float x1, float y1, float r0, float r1, const Ramp& R, int bias = 0) {
    capsule(c, V(x0, y0), V(x1, y1), r0, r1, R, bias, 0.06f);
  }
  void fur(float cx, float cy, float rx, float ry, const Ramp& R, uint32_t sd, int bias = 0) {
    furBall(c, cx, cy, rx, ry, R, S.noise, L.seed * 31u + sd, bias);
  }
  bool idleF() const { return f == HF_IDLE || f == HF_IDLE + 1; }
  bool runF() const { return f >= HF_RUN && f < HF_BITE; }
  bool biteF() const { return f == HF_BITE || f == HF_BITE + 1; }
  bool downF() const { return f == HF_DOWN; }
  bool sitF() const { return f == HF_SIT; }
  int runI() const { return f - HF_RUN; }
  bool panting() const { return f == HF_IDLE + 1 || (runF() && L.age > 0) || sitF(); }

  // ---------------------------------------------------------------- the side view (facing right)
  void side() {
    const float cx = W * 0.5f - (biteF() ? 1.5f : 0.0f) - (wolf ? 0.0f : 0.5f), g = gy();
    const float s = sc;
    auto X = [&](float v) { return cx + v * s; };
    if (downF()) { sideDown(cx, g); return; }
    if (sitF()) { sideSit(cx, g); return; }
    float bob = 0.0f, flex = 0.0f;
    // the legs: foot x offsets (from the leg's top) and lifts, far / near, front / hind
    float fFar = 0, fNear = 0, hFar = 0, hNear = 0, lfF = 0, lfN = 0, lhF = 0, lhN = 0;
    float headDX = 0, headDH = 0;
    if (runF()) {
      switch (runI()) {
        case 0: fNear = 3.2f; fFar = 2.2f; hNear = -3.0f; hFar = -2.0f; break;                                   // stretched out
        case 1: fNear = -1.0f; fFar = -2.0f; hNear = 2.4f; hFar = 3.2f; lfN = 2; lfF = 1.5f; lhN = 1; lhF = 0.5f; bob = -1.0f; flex = 0.9f; break;   // gathered
        case 2: fNear = 1.2f; fFar = 2.8f; hNear = -1.5f; hFar = -0.5f; lhF = 1; break;
        default: fNear = -2.2f; fFar = -1.2f; hNear = 3.0f; hFar = 2.0f; lfN = 1; bob = -1.0f; flex = 0.5f; break;
      }
      headDX = 0.0f; headDH = -1.2f;
    } else if (biteF()) {
      if (f == HF_BITE) { fNear = -1.0f; fFar = -0.5f; hNear = 1.0f; hFar = 1.5f; headDX = -1.0f; headDH = -2.2f; bob = 0.8f; }   // crouched, drawing back
      else { fNear = 2.6f; fFar = 2.0f; hNear = -2.4f; hFar = -1.8f; headDX = 1.4f; headDH = -2.0f; }                         // the lunge
    }
    const Ramp& A = K.a;
    auto Y = [&](float h) { return g - h * s + bob; };
    const float legTopY = Y(S.legTop);
    // a two-jointed leg from its top to the paw: forelegs nearly straight, hind legs back to the hock
    auto leg = [&](float topX, bool fore, float footDX, float lift, bool near) {
      const int bias = near ? 0 : -1;
      const float o = near ? 0.0f : -1.0f;
      const float tx = X(topX) + o, ty0 = legTopY + (fore ? 0.0f : -0.8f * s);
      const float fx = tx + footDX * s, fy = g - 0.4f - lift * s;
      const Ramp& LR = A;
      if (fore) {
        const float ex = (tx + fx) * 0.5f - 0.3f * s, ey = (ty0 + fy) * 0.5f - 0.2f;
        cap(tx, ty0, ex, ey, S.legR0 * s * 1.15f, S.legR0 * s * 0.85f, LR, bias);
        cap(ex, ey, fx, fy, S.legR0 * s * 0.8f, S.legR1 * s, LR, bias);
      } else {
        const float kx = tx + 1.0f * s + footDX * 0.3f * s, ky = ty0 + (fy - ty0) * 0.42f;      // the stifle
        const float hx2 = tx - 1.1f * s + footDX * 0.6f * s, hy2 = fy - (fy - ty0) * 0.3f;     // the hock
        cap(tx, ty0, kx, ky, S.legR0 * s * 1.5f, S.legR0 * s * 1.0f, LR, bias);
        cap(kx, ky, hx2, hy2, S.legR0 * s * 0.85f, S.legR1 * s * 0.95f, LR, bias);
        cap(hx2, hy2, fx, fy, S.legR1 * s, S.legR1 * s, LR, bias);
      }
      // the paw, a little forward of the leg
      const Ramp& P = K.socks ? K.l : A;
      const int px = (int)std::floor(fx + 0.5f), py = (int)std::floor(fy + 0.2f);
      c.set(px, py, P[2 + bias]); c.set(px + 1, py, P[1 + bias]);
      if (L.age == 0) c.set(px - 1, py, P[2 + bias]);   // the pup's big paws
      if (K.socks) { c.set(px, py - 1, P[3 + bias]); }
    };
    // the far legs
    leg(S.frontX, true, fFar, lfF, false);
    leg(S.hindX, false, hFar, lhF, false);
    // the tail behind the haunch
    tailSide(X(S.hauX - S.hauRx * 0.85f), Y(S.hauH + S.hauRy * 0.35f), s);
    // the body: the haunch, the waist, the deep chest
    const float hauX = S.hauX + flex;
    fur(X(hauX), Y(S.hauH), S.hauRx * s, S.hauRy * s, A, 1);
    cap(X(hauX + 0.6f), Y(S.hauH + 0.5f), X(S.chX - 0.8f), Y(S.chH + 0.6f), S.waist * s, S.waist * s * 1.05f, A, 0);
    fur(X(S.chX), Y(S.chH - 0.2f), S.chRx * s, S.chRy * s, A, 2);
    if (wolf) fur(X(S.chX + 1.2f), Y(S.chH + 1.6f), 2.6f * s, 3.0f * s, A, 3);   // the ruff over the shoulders
    // the light underside: the chest bib and the belly line
    const float bibX = X(S.chX + S.chRx * 0.55f), bibY = Y(S.chH - S.chRy * 0.35f);
    if (K.bib || wolf) ball(c, bibX, bibY, 1.5f * s + 0.3f, 2.2f * s, K.l, 0.05f);
    for (int x = (int)X(hauX); x <= (int)X(S.chX + S.chRx * 0.5f); x++)
      for (int y = (int)Y(S.chH) + 1; y < H; y++)
        if (solid(c, x, y) && !solid(c, x, y + 1)) { c.set(x, y, K.l[x > X(S.chX) - 1 ? 2 : 1]); break; }
    // the near legs
    leg(S.frontX, true, fNear, lfN, true);
    leg(S.hindX, false, hNear, lhN, true);
    // the neck and the head
    const float hx = X(S.headX + headDX), hy = Y(S.headH + headDH);
    const float ncx = X(S.chX + 1.2f), ncy = Y(S.chH + 1.4f);
    cap(ncx, ncy, hx - 0.8f * s, hy + 0.6f * s, S.neckR * s, S.neckR * s * 0.85f, A, 0);
    collarSide(ncx, ncy, hx - 0.8f * s, hy + 0.6f * s, S.neckR * s);
    headSide(hx, hy, s);
    markSide();
  }
  void tailSide(float tx, float ty, float s) {
    const Ramp& A = K.a;
    float wag = 0.0f;
    if (idleF()) wag = f == HF_IDLE ? 0.0f : 1.4f;
    if (runF()) wag = (runI() & 1) ? 1.0f : -0.5f;
    if (wolf) {   // the brush: thick, hanging, its dark tip
      const float ex = tx - 3.0f * s, ey = ty + (biteF() ? 1.0f : 4.6f) * s - wag * 0.8f;
      cap(tx + 0.5f, ty, ex, ey, S.tailR * s, S.tailR * s * 1.15f, A, 0);
      const int ix = (int)std::floor(ex), iy = (int)std::floor(ey);
      c.set(ix, iy, K.d[1]); c.set(ix, iy + 1, K.d[0]); c.set(ix + 1, iy + 1, K.d[1]);
    } else {      // the hound's long tail, low and curving up at its end (it wags)
      const float mx = std::max(3.0f, tx - 2.4f * s), my = ty + (biteF() ? 1.0f : 3.4f) * s - wag;
      const float ex = std::max(2.2f, mx - 1.0f * s), ey = my - (1.6f + wag) * s;
      cap(tx + 0.4f, ty, mx, my, S.tailR * s * 1.1f, S.tailR * s, A, 0);
      cap(mx, my, ex, ey, S.tailR * s, S.tailR * s * 0.7f, A, 0);
    }
  }
  void collarSide(float ax, float ay, float bx, float by, float r) {
    if (!L.collar) return;
    const float t = 0.45f;
    const float px = ax + (bx - ax) * t, py = ay + (by - ay) * t;
    const float dx = bx - ax, dy = by - ay, l = std::max(0.01f, std::sqrt(dx * dx + dy * dy));
    const float ux = dx / l, uy = dy / l;
    const Ramp& Cl = L.collar == 2 ? kLeather : ramp(rgba(150, 52, 40));
    for (int k = -1; k <= 1; k++) {
      const float rr = r + 0.4f;
      for (float q = -rr; q <= rr; q += 0.5f) {
        const float x = px + ux * k * 0.5f - uy * q, y = py + uy * k * 0.5f + ux * q;
        c.set((int)std::floor(x), (int)std::floor(y), Cl[q < -0.4f ? 3 : (q > rr - 0.8f ? 1 : 2)]);
      }
    }
    // the studs or the tag at its lowest point
    const int lx = (int)std::floor(px - uy * r), ly = (int)std::floor(py + ux * r + 0.6f);
    if (L.collar == 2) { c.set((int)std::floor(px + uy * 0.3f), (int)std::floor(py - 0.4f), kBrass[4]); c.set(lx, ly - 1, kBrass[3]); }
    c.set(lx, ly, kGold[L.collar == 2 ? 2 : 4]);
  }
  void headSide(float hx, float hy, float s) {
    const Ramp& A = K.a;
    const float hr = S.headR * s;
    fur(hx, hy, hr, hr * 0.9f, A, 7);
    const bool open = biteF() || panting();
    // the muzzle forward and a little down; the jaw parts when it bites or pants
    const float ml = S.muzL * s, mr = S.muzR * s;
    const float mx0 = hx + hr * 0.45f, my0 = hy + S.muzDrop * s;
    const float mx1 = mx0 + ml, my1 = my0 + (biteF() && f == HF_BITE + 1 ? -0.3f : 0.4f);
    const Ramp& Mz = K.mask ? K.d : (wolf ? K.l : A);
    cap(mx0, my0, mx1, my1, mr, mr * 0.8f, Mz, 0);
    const int nx = (int)std::floor(mx1 + mr * 0.7f), ny = (int)std::floor(my1 - mr * 0.5f);
    c.set(nx, ny, kInk); c.set(nx - 1, ny, mix(kInk, Mz[1], 0.4f));   // the nose
    const int jy = (int)std::floor(my1 + mr * 0.6f) + (open ? 1 : 0);
    if (open) {   // the open jaw: the dark mouth, the tongue or the teeth
      for (int x = (int)std::floor(mx0 + 0.5f); x <= nx - 1; x++) c.set(x, jy - 1, rgba(90, 30, 40));
      for (int x = (int)std::floor(mx0); x <= nx - 2; x++) c.set(x, jy, Mz[1]);
      if (biteF()) { c.set(nx - 1, jy - 1, kWhite); c.set(nx - 2, jy, kWhite); }
      else { c.set(nx - 3, jy + 1, rgba(214, 96, 110)); c.set(nx - 2, jy + 1, rgba(186, 70, 90)); }   // the tongue lolls
    } else {
      c.set((int)std::floor(mx0 + ml * 0.5f), jy - 1, Mz[0]);   // the mouth's line
    }
    if (!wolf && L.age > 0) c.set((int)std::floor(mx1), jy, A[1]);   // the wolfhound's beard
    // the eye: dark and bright, under a brow
    const int ex = (int)std::floor(hx + hr * 0.35f), ey = (int)std::floor(hy - hr * 0.15f);
    if (biteF()) { c.set(ex, ey, kEye); c.set(ex - 1, ey - 1, A[0]); }
    else { c.set(ex, ey, kEye); c.set(ex, ey - 1, A[biteF() ? 0 : 3]); if (L.age == 0) c.set(ex + 1, ey, kEye); }
    if (K.blaze) c.set(ex + 1, ey - 2, K.l[4]);
    // the ears
    const int ax = (int)std::floor(hx - hr * 0.35f), ay = (int)std::floor(hy - hr * 0.75f);
    if (wolf) {   // erect and pointed (laid back when it bites)
      if (biteF()) { c.set(ax, ay + 1, K.d[2]); c.set(ax - 1, ay + 1, K.d[1]); c.set(ax - 2, ay + 2, K.d[1]); }
      else {
        const int up = L.age == 0 ? 2 : 3;
        for (int k = 0; k < up; k++) { c.set(ax, ay - k, K.d[k == up - 1 ? 3 : 2]); if (k < up - 1) c.set(ax + 1, ay - k, A[1]); }
        c.set(ax - 1, ay, K.d[1]);
      }
    } else if (L.age == 0) {   // the pup's soft flopped ear
      cap(hx - hr * 0.3f, hy - hr * 0.5f, hx - hr * 0.55f, hy + hr * 0.45f, 0.9f, 0.8f, K.d, 0);
    } else {   // the rose ear folded back along the skull
      c.set(ax, ay, K.d[2]); c.set(ax - 1, ay, K.d[1]); c.set(ax - 1, ay + 1, K.d[1]); c.set(ax - 2, ay + 1, K.d[0]);
      if (runF() || biteF()) c.set(ax - 2, ay, K.d[1]);
    }
  }
  void sideSit(float cx, float g) {
    const float s = sc;
    const Ramp& A = K.a;
    auto X = [&](float v) { return cx + v * s; };
    // the tail along the ground behind, the haunch down, the chest upright, the forelegs straight
    const float hx0 = X(-2.6f), hr = 3.2f * s;
    cap(hx0 - hr + 1.0f, g - 0.8f, hx0 - hr - 3.0f * s, g - 1.4f, S.tailR * s, S.tailR * s * 0.8f, A, 0);
    fur(hx0, g - hr * 0.95f, hr, hr * 0.95f, A, 1);
    fur(X(1.4f), g - hr * 2.0f, hr * 0.9f, hr * 1.2f, A, 2);
    if (wolf) fur(X(2.0f), g - hr * 2.6f, hr * 0.8f, hr * 0.9f, A, 3);
    ball(c, X(2.6f), g - hr * 1.7f, 1.2f * s + 0.4f, hr * 0.75f, K.l, 0.05f);
    // the forelegs and the paws
    const Ramp& P = K.socks ? K.l : A;
    cap(X(3.0f) - 1.0f, g - hr * 1.3f, X(3.0f) - 1.0f, g - 0.4f, S.legR0 * s, S.legR1 * s, A, -1);
    cap(X(3.0f), g - hr * 1.3f, X(3.0f), g - 0.4f, S.legR0 * s, S.legR1 * s, A, 0);
    c.set((int)std::floor(X(3.0f)) + 1, (int)std::floor(g), P[2]);
    c.set((int)std::floor(X(3.0f)), (int)std::floor(g), P[3]);
    // the hind paw before the haunch
    c.set((int)std::floor(hx0 + hr * 0.6f), (int)std::floor(g), P[2]); c.set((int)std::floor(hx0 + hr * 0.6f) + 1, (int)std::floor(g), P[1]);
    // the neck and the head held high, looking at its master
    const float hx = X(S.headX - 4.2f), hy = g - (S.headH + 0.6f) * s;
    cap(X(2.0f), g - hr * 2.4f, hx - 0.6f * s, hy + 0.6f * s, S.neckR * s, S.neckR * s * 0.9f, A, 0);
    collarSide(X(2.0f), g - hr * 2.4f, hx - 0.6f * s, hy + 0.6f * s, S.neckR * s);
    headSide(hx, hy, s);
    markSide();
  }
  void sideDown(float cx, float g) {
    // lying hurt on its side: flat along the ground, the legs out, the head down, the eyes shut, the tail limp
    const float s = sc;
    g -= 0.6f;   // the flank's lowest row on the ground row
    const Ramp& A = K.a;
    auto X = [&](float v) { return cx + v * s; };
    for (int k = 0; k < 2; k++) {
      cap(X(3.6f + k * 1.6f), g - 2.2f * s, X(6.6f + k * 1.6f), g - 0.6f, S.legR0 * s, S.legR1 * s, A, -1 + k);   // the forelegs out
      cap(X(-4.6f + k * 1.6f), g - 2.0f * s, X(-1.6f + k * 1.2f), g - 0.4f, S.legR0 * s * 1.2f, S.legR1 * s, A, -1 + k);   // the hind legs
    }
    cap(X(-7.0f), g - 2.0f * s, X(-10.0f), g - 0.6f, S.tailR * s, S.tailR * s * 0.8f, A, 0);
    fur(X(-1.0f), g - 3.2f * s, 7.4f * s, 3.0f * s, A, 4);
    ball(c, X(0.6f), g - 1.6f * s, 4.6f * s, 1.2f * s, K.l, 0.0f);   // the pale belly turned up a little
    // the head down on the ground ahead, the ear flat, the eye shut
    const float hx = X(7.6f), hy = g - 2.2f * s;
    fur(hx, hy, S.headR * s, S.headR * s * 0.8f, A, 7);
    cap(hx + S.headR * s * 0.4f, hy + 0.4f, hx + (S.headR * 0.4f + S.muzL) * s, hy + 0.8f, S.muzR * s, S.muzR * s * 0.8f, K.mask ? K.d : (wolf ? K.l : A), 0);
    c.set((int)std::floor(hx + (S.headR * 0.4f + S.muzL) * s + S.muzR * 0.6f), (int)std::floor(hy + 0.4f), kInk);
    c.set((int)std::floor(hx + S.headR * s * 0.3f), (int)std::floor(hy - 0.4f), mix(kEye, A[1], 0.4f));
    c.set((int)std::floor(hx + S.headR * s * 0.3f) - 1, (int)std::floor(hy - 0.4f), mix(kEye, A[1], 0.4f));
    c.set((int)std::floor(hx - S.headR * s * 0.4f), (int)std::floor(hy - S.headR * s * 0.6f), K.d[1]);
    c.set((int)std::floor(hx - S.headR * s * 0.4f) - 1, (int)std::floor(hy - S.headR * s * 0.5f), K.d[0]);
    if (L.collar) {
      const Ramp& Cl = L.collar == 2 ? kLeather : ramp(rgba(150, 52, 40));
      const int nx = (int)std::floor(hx - S.headR * s * 1.1f);
      for (int y = (int)std::floor(hy - 1.2f * s); y <= (int)std::floor(hy + 1.6f * s); y++) c.set(nx, y, Cl[2]);
      c.set(nx, (int)std::floor(hy + 1.6f * s) + 1, kGold[3]);
    }
    markSide();
  }
  // the coat's markings over the painted volumes: a saddle on the back, brindle stripes
  void markSide() {
    if (!K.saddle && !K.brindle) return;
    int x0, y0, x1, y1;
    bounds(c, x0, y0, x1, y1);
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        const uint32_t p = c.get(x, y);
        int idx = -1;
        for (int i = 0; i < 5; i++) if (K.a[i] == p) idx = i;
        if (idx < 0) continue;
        bool m = false;
        if (K.saddle) {   // the upper part of the back and the neck
          int top = y;
          while (top > 0 && solid(c, x, top - 1)) top--;
          m = y - top <= (int)(1.4f * sc + 0.5f) && y < y1 - 3;
        }
        if (K.brindle && ((x + (y >> 1)) % 3 == 0)) m = true;
        if (m) c.set(x, y, K.d[std::min(4, idx + (K.saddle ? 1 : 0))]);
      }
  }

  // ---------------------------------------------------------------- facing the camera
  void front() {
    // facing the camera from the high 3/4 view: the head nearest and highest, the chest under it, the forelegs down to
    // the ground; the back beyond the head shows only as the shoulders' line and the haunches at the sides
    const float cx = W * 0.5f, g = gy(), s = sc;
    const Ramp& A = K.a;
    auto lay = [&](auto&& fn) { layered(c, fn, 0.6f); };
    const bool run = runF();
    const float bob = run && (runI() & 1) ? -1.0f : 0.0f;
    const float bw = (S.chRx + 0.4f) * s;
    if (downF()) {   // lying on its side across the view: the body long, the head on its paws at screen left
      cap(cx + 7.0f * s, g - 1.6f * s, cx + 10.2f * s, g - 0.6f, S.tailR * s, S.tailR * s * 0.8f, A, -1);
      fur(cx + 1.4f * s, g - 2.6f * s, 7.0f * s, 2.8f * s, A, 4);
      ball(c, cx + 1.8f * s, g - 1.2f * s, 4.8f * s, 1.0f * s + 0.2f, K.l, 0.0f);
      for (int k = 0; k < 2; k++) cap(cx - 2.4f * s + k * 4.6f * s, g - 1.0f, cx - 3.4f * s + k * 4.6f * s, g - 0.3f, S.legR1 * s * 1.3f, S.legR1 * s * 1.1f, A, -1);
      lay([&](Canvas& p) { furBall(p, cx - 6.0f * s, g - 2.0f * s, S.headR * s, S.headR * s * 0.8f, A, S.noise, L.seed + 9, 0); });
      const int hx = (int)std::floor(cx - 6.0f * s), hy = (int)std::floor(g - 2.0f * s);
      c.set(hx - 1, hy, mix(kEye, A[1], 0.4f)); c.set(hx + 1, hy, mix(kEye, A[1], 0.4f));
      c.set(hx, hy + 1 + (L.age > 0), kInk);
      c.set(hx - 2, hy - 1, K.d[1]); c.set(hx + 2, hy - 1, K.d[0]);
      markFront();
      return;
    }
    const bool sit = sitF();
    const float lift = sit ? 2.6f * s : 0.0f;   // sitting: the chest and the head held higher
    // the tail: a hound's wagging tip shows past its flank (a wolf carries its brush low, out of sight)
    if (!wolf && !sit) {
      float wag = 0.0f;
      if (idleF()) wag = f == HF_IDLE ? -1.0f : 1.0f;
      if (run) wag = (runI() & 1) ? 1.0f : -1.0f;
      const float tx = cx + (bw + 0.6f) * (wag >= 0 ? 1.0f : -1.0f);
      cap(tx, g - (S.legTop + 1.6f) * s + bob, tx + wag * 1.6f * s, g - (S.legTop + 4.2f) * s + bob, S.tailR * s, S.tailR * s * 0.7f, A, wag >= 0 ? -1 : 0);
    }
    // the back beyond the head: the shoulders' and the loins' line
    fur(cx, g - (S.legTop + 1.2f) * s + bob - lift * 0.5f, bw * 0.85f, (wolf ? 2.4f : 2.0f) * s, A, 1);
    // the hind legs at the sides (the haunches when sitting)
    if (!sit) {
      const float lf = run ? ((runI() & 1) ? 1.0f : 0.0f) : 0.0f;
      // the hind feet behind and outside the forelegs, in the body's shade
      cap(cx - bw * 0.85f, g - 3.6f * s + bob, cx - bw * 0.85f, g - 1.0f - lf, S.legR1 * s * 1.2f, S.legR1 * s, A, -2);
      cap(cx + bw * 0.85f, g - 3.6f * s + bob, cx + bw * 0.85f, g - 1.0f - (run ? 1.0f - lf : 0.0f), S.legR1 * s * 1.2f, S.legR1 * s, A, -2);
    } else {
      lay([&](Canvas& p) {
        furBall(p, cx - bw * 0.95f, g - 1.8f * s, 2.0f * s, 1.8f * s, A, S.noise, L.seed + 3, 0);
        furBall(p, cx + bw * 0.95f, g - 1.8f * s, 2.0f * s, 1.8f * s, A, S.noise, L.seed + 3, -1);
      });
    }
    // the chest (its pale bib) and the forelegs
    const float chestY = g - (S.legTop - 0.6f) * s + bob - lift;
    lay([&](Canvas& p) {
      furBall(p, cx, chestY, bw * 0.8f, (wolf ? 2.8f : 2.5f) * s, A, S.noise, L.seed + 2, 0);
      if (K.bib || wolf) ball(p, cx, chestY + 0.6f * s, bw * 0.45f, 1.7f * s, K.l, 0.04f);
    });
    {
      const float lx = bw * 0.5f;
      const float lfA = run ? ((runI() & 1) ? 1.5f : 0.0f) : 0.0f, lfB = run ? ((runI() & 1) ? 0.0f : 1.5f) : 0.0f;
      const float spread = biteF() ? 0.8f * s : 0.0f;
      const Ramp& P = K.socks ? K.l : A;
      cap(cx - lx - spread, chestY + 1.0f * s, cx - lx - spread, g - 0.5f - lfA, S.legR0 * s, S.legR1 * s, A, 0);
      cap(cx + lx + spread, chestY + 1.0f * s, cx + lx + spread, g - 0.5f - lfB, S.legR0 * s, S.legR1 * s, A, -1);
      const int pl = (int)std::floor(cx - lx - spread), pr = (int)std::floor(cx + lx + spread);
      c.set(pl, (int)std::floor(g - lfA), P[3]); c.set(pl - 1, (int)std::floor(g - lfA), P[2]);
      c.set(pr, (int)std::floor(g - lfB), P[1]); c.set(pr + 1, (int)std::floor(g - lfB), P[0]);
    }
    // the head over the chest, high: the camera looks down on its brow
    float hy = g - (S.legTop + 3.2f) * s + bob - lift;
    if (biteF()) hy += (f == HF_BITE + 1 ? 2.0f : 0.8f) * s;   // the head lowered to bite
    if (run) hy += 0.4f * s;                                      // stretched forward
    if (L.collar) {
      const Ramp& Cl = L.collar == 2 ? kLeather : ramp(rgba(150, 52, 40));
      const int yy = (int)std::floor(hy + S.headR * s * 1.15f);
      for (int x = (int)std::floor(cx - S.neckR * s - 0.5f); x <= (int)std::floor(cx + S.neckR * s + 0.5f); x++) c.set(x, yy, Cl[x < cx - 1 ? 3 : (x > cx + 1 ? 1 : 2)]);
      c.set((int)std::floor(cx), yy + 1, kGold[L.collar == 2 ? 3 : 4]);
      if (L.collar == 2) { c.set((int)std::floor(cx) - 2, yy, kBrass[4]); c.set((int)std::floor(cx) + 2, yy, kBrass[3]); }
    }
    headFront(cx, hy, s);
    markFront();
  }
  void headFront(float cx, float hy, float s) {
    const Ramp& A = K.a;
    const float hr = (S.headR + 0.3f) * s;
    layered(c, [&](Canvas& p) { furBall(p, cx, hy, hr * (wolf ? 1.05f : 0.92f), hr * 0.95f, A, S.noise, L.seed + 9, 0); }, 0.6f);
    // the muzzle toward the camera (light or the dark mask), the nose at its tip
    const Ramp& Mz = K.mask ? K.d : (wolf ? K.l : A);
    const float my = hy + hr * 0.55f, mrx = (wolf ? 1.8f : 1.4f) * s + 0.3f, mry = (S.muzL * 0.45f) * s + 0.5f;
    layered(c, [&](Canvas& p) { ball(p, cx, my, mrx, mry, Mz, 0.0f); }, 0.6f);
    const int hxi = (int)std::floor(cx), nyi = (int)std::floor(my + mry * 0.4f);
    c.set(hxi, nyi, kInk); c.set(hxi - 1, nyi, mix(kInk, Mz[2], 0.35f));
    if (biteF() || panting()) {
      c.set(hxi, nyi + 1, rgba(90, 30, 40)); c.set(hxi - 1, nyi + 1, rgba(90, 30, 40));
      if (biteF()) { c.set(hxi - 2, nyi + 1, kWhite); c.set(hxi + 1, nyi + 1, kWhite); }
      else { c.set(hxi, nyi + 2, rgba(214, 96, 110)); c.set(hxi - 1, nyi + 2, rgba(186, 70, 90)); }
    }
    // the eyes: dark and bright, the brows over them
    const int ey = (int)std::floor(hy - hr * 0.1f);
    const int ex = std::max(1, (int)std::lround(hr * 0.5f));
    c.set(hxi - ex, ey, kEye); c.set(hxi + ex - 1, ey, kEye);
    if (!biteF()) { c.set(hxi - ex, ey - 1, A[4]); c.set(hxi + ex - 1, ey - 1, A[3]); }
    if (K.blaze) { c.set(hxi - 1, ey - 1, K.l[4]); c.set(hxi - 1, ey, K.l[3]); }
    // the ears
    const int ay = (int)std::floor(hy - hr * 0.8f);
    const int ax = (int)std::lround(hr * 0.85f);
    if (wolf && !biteF()) {
      const int up = L.age == 0 ? 2 : 3;
      for (int k = 0; k < up; k++) {   // erect, two pixels at the base narrowing to the tip, the inner ear in shade
        const int in = k == up - 1 ? 1 : 0;
        c.set(hxi - ax - 1 + in, ay - k, K.d[3]); if (!in) c.set(hxi - ax, ay - k, K.d[1]);
        c.set(hxi + ax - in, ay - k, K.d[2]); if (!in) c.set(hxi + ax - 1, ay - k, K.d[0]);
      }
    } else if (!wolf && L.age == 0) {   // floppy pup ears hanging at the cheeks
      cap(cx - hr + 0.2f, hy - hr * 0.5f, cx - hr - 0.3f, hy + hr * 0.4f, 0.9f * s + 0.2f, 0.8f * s + 0.2f, K.d, -1);
      cap(cx + hr - 0.2f, hy - hr * 0.5f, cx + hr + 0.3f, hy + hr * 0.4f, 0.9f * s + 0.2f, 0.8f * s + 0.2f, K.d, -2);
    } else {   // folded rose ears (or a wolf's laid back to bite)
      c.set(hxi - ax, ay + 1, K.d[2]); c.set(hxi - ax - 1, ay + 1, K.d[1]); c.set(hxi - ax - 1, ay + 2, K.d[0]);
      c.set(hxi + ax - 1, ay + 1, K.d[1]); c.set(hxi + ax, ay + 1, K.d[0]); c.set(hxi + ax, ay + 2, K.d[0]);
    }
  }
  void markFront() {
    if (!K.saddle && !K.brindle) return;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        const uint32_t p = c.get(x, y);
        int idx = -1;
        for (int i = 0; i < 5; i++) if (K.a[i] == p) idx = i;
        if (idx < 0) continue;
        bool m = K.brindle && ((y + (x >> 1)) % 3 == 0);
        if (m) c.set(x, y, K.d[idx]);
      }
  }

  // ---------------------------------------------------------------- walking away
  void back() {
    // walking away: the rump nearest the camera (lowest on the screen), the back rising beyond it, the head furthest
    const float cx = W * 0.5f, g = gy(), s = sc;
    const Ramp& A = K.a;
    auto lay = [&](auto&& fn) { layered(c, fn, 0.6f); };
    const bool run = runF();
    const float bob = run && (runI() & 1) ? -1.0f : 0.0f;
    const float bw = (S.hauRx + 0.6f) * s;
    const float hr = S.headR * s;
    auto ears = [&](int hxi, int hyi, bool laid) {
      const int r = (int)std::lround(hr);
      if (wolf && !laid) { const int up = L.age == 0 ? 2 : 3; for (int k = 0; k < up; k++) { c.set(hxi - r + 1, hyi - r + 1 - k, K.d[3]); c.set(hxi + r - 1, hyi - r + 1 - k, K.d[2]); } }
      else if (!wolf && L.age == 0) { c.set(hxi - r - 1, hyi, K.d[2]); c.set(hxi + r, hyi, K.d[1]); c.set(hxi - r - 1, hyi + 1, K.d[1]); c.set(hxi + r, hyi + 1, K.d[0]); }
      else { c.set(hxi - r, hyi - 1, K.d[2]); c.set(hxi + r - 1, hyi - 1, K.d[1]); c.set(hxi - r - 1, hyi, K.d[1]); c.set(hxi + r, hyi, K.d[0]); }
    };
    if (downF()) {   // lying on its side, the head on its paws at the far end
      cap(cx + 6.6f * s, g - 1.6f * s, cx + 10.0f * s, g - 0.6f, S.tailR * s, S.tailR * s * 0.8f, A, 0);
      fur(cx + 0.8f * s, g - 2.6f * s, 7.0f * s, 2.8f * s, A, 4);
      lay([&](Canvas& p) { furBall(p, cx - 6.0f * s, g - 2.6f * s, hr * 0.95f, hr * 0.8f, A, S.noise, L.seed + 9, 0); });
      ears((int)std::floor(cx - 6.0f * s), (int)std::floor(g - 2.6f * s), true);
      markBack();
      return;
    }
    const bool sit = sitF();
    // the head beyond the back, the ears; then the collar showing at the nape
    float hy = g - (S.legTop + 5.2f) * s + bob - (sit ? 3.0f * s : 0.0f) + (biteF() ? 2.4f * s : 0.0f);
    fur(cx, hy, hr, hr * 0.9f, A, 9);
    const int hxi = (int)std::floor(cx), hyi = (int)std::floor(hy);
    ears(hxi, hyi, biteF());
    // the back from the shoulders to the rump (sitting: sloping up from the haunches to the shoulders)
    const float backY = sit ? g - 6.4f * s : g - (S.legTop + 2.4f) * s + bob;
    lay([&](Canvas& p) { furBall(p, cx, backY, bw * 0.92f, (sit ? 4.6f : 3.4f) * s, A, S.noise, L.seed + 1, 0); });
    if (L.collar) {
      const Ramp& Cl = L.collar == 2 ? kLeather : ramp(rgba(150, 52, 40));
      const int yy = (int)std::floor(hy + hr * 0.95f);
      for (int x = hxi - (int)(S.neckR * s); x <= hxi + (int)(S.neckR * s); x++) if (solid(c, x, yy)) c.set(x, yy, Cl[x < hxi ? 3 : 1]);
    }
    // the hind legs and the rump nearest the camera
    const float lf = run ? ((runI() & 1) ? 1.4f : 0.0f) : 0.0f;
    if (!sit) {
      cap(cx - bw * 0.55f, g - S.legTop * s + bob, cx - bw * 0.55f, g - 0.5f - lf, S.legR0 * s * 1.2f, S.legR1 * s, A, 0);
      cap(cx + bw * 0.55f, g - S.legTop * s + bob, cx + bw * 0.55f, g - 0.5f - (run ? 1.4f - lf : 0.0f), S.legR0 * s * 1.2f, S.legR1 * s, A, -1);
    }
    const float rumpY = sit ? g - 2.2f * s : g - S.legTop * s + 0.4f + bob;
    lay([&](Canvas& p) { furBall(p, cx, rumpY, bw * (sit ? 1.15f : 1.0f), S.hauRy * 0.85f * s, A, S.noise, L.seed + 5, 0); });
    // the tail toward the camera (it wags)
    float wag = 0.0f;
    if (idleF()) wag = f == HF_IDLE ? -1.8f : 1.8f;
    if (run) wag = (runI() & 1) ? 1.0f : -1.0f;
    const float ty0 = rumpY - S.hauRy * 0.4f * s;
    if (sit) cap(cx + bw * 0.6f, g - 0.8f, cx + bw * 1.2f + 1.0f, g - 0.6f, S.tailR * s, S.tailR * s * 0.8f, A, 0);
    else if (wolf) cap(cx, ty0, cx + wag * 0.6f * s, ty0 + 3.6f * s, S.tailR * s, S.tailR * s * 1.2f, A, 0);
    else cap(cx, ty0, cx + wag * s, ty0 - 3.2f * s, S.tailR * s * 1.1f, S.tailR * s * 0.7f, A, 1);
    markBack();
  }
  void markBack() {
    if (!K.saddle && !K.brindle) return;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        const uint32_t p = c.get(x, y);
        int idx = -1;
        for (int i = 0; i < 5; i++) if (K.a[i] == p) idx = i;
        if (idx < 0) continue;
        bool m = K.brindle && ((y + (x >> 1)) % 3 == 0);
        if (K.saddle && std::fabs(x + 0.5f - W * 0.5f) < 2.2f * sc + 0.6f && y < H - 6) m = true;   // the dark line down the back
        if (m) c.set(x, y, K.d[std::min(4, idx + 1)]);
      }
  }

  void paint() {
    if (view == 0) front();
    else if (view == 1) back();
    else side();
  }
};

// the dog's head and shoulders at portrait scale: a three-quarter head turned toward screen right, the muzzle reaching
// out, the ears by species and age, the collar at the neck
void houndBust(Canvas& c, const HoundLook& L, int mood) {
  const HCoat K = houndCoat(L);
  const bool wolf = L.species == 1, pup = L.age == 0;
  const Ramp& A = K.a;
  const float hs = pup ? 1.08f : 1.0f;
  auto fur = [&](Canvas& t, float cx, float cy, float rx, float ry, const Ramp& R, uint32_t sd) { furBall(t, cx, cy, rx, ry, R, wolf ? 0.3f : 0.45f, L.seed * 7u + sd); };
  // the chest and the shoulders
  fur(c, 11.5f, 21.5f, 8.4f, 5.0f, A, 1);
  if (wolf) fur(c, 10.5f, 17.0f, 7.0f, 4.6f, A, 2);   // the ruff
  if (K.bib || wolf) ball(c, 13.5f, 21.0f, 3.2f, 3.4f, K.l, 0.04f);
  // the neck and the collar
  fur(c, 11.0f, 16.0f, 4.4f, 4.4f, A, 3);
  if (L.collar) {
    const Ramp& Cl = L.collar == 2 ? kLeather : ramp(rgba(150, 52, 40));
    for (int x = 6; x <= 16; x++) {
      const int y = 17 + (x - 6) / 4;
      if (!solid(c, x, y)) continue;
      c.set(x, y, Cl[x < 9 ? 3 : (x > 13 ? 1 : 2)]); c.set(x, y + 1, Cl[x < 9 ? 2 : (x > 13 ? 0 : 1)]);
      if (L.collar == 2 && (x % 3 == 0)) c.set(x, y, kBrass[4]);
    }
    c.set(13, 21, kGold[4]); c.set(13, 22, kGold[2]); c.set(12, 21, kGold[3]);
  }
  // the head
  const float hx = 10.5f, hy = pup ? 9.4f : 9.0f, hr = (wolf ? 5.0f : 4.6f) * hs;
  layered(c, [&](Canvas& t) { fur(t, hx, hy, hr, hr * 0.92f, A, 5); }, 0.55f);
  // the muzzle toward screen right and down
  const Ramp& Mz = K.mask ? K.d : (wolf ? K.l : A);
  const float ml = pup ? 3.6f : (wolf ? 6.4f : 7.0f);
  const float mx1 = hx + 2.6f + ml, my1 = hy + 3.4f;
  layered(c, [&](Canvas& t) { capsule(t, V(hx + 2.4f, hy + 2.0f), V(mx1, my1), wolf ? 2.6f : 2.2f, wolf ? 1.9f : 1.6f, Mz, 0, 0.06f); }, 0.55f);
  const int nx = (int)std::floor(mx1 + 0.8f), ny = (int)std::floor(my1 - 1.2f);
  c.set(nx, ny, kInk); c.set(nx - 1, ny, kInk); c.set(nx, ny + 1, mix(kInk, Mz[1], 0.4f)); c.set(nx - 1, ny - 1, mix(kWhite, Mz[3], 0.5f));   // the wet nose
  const bool glad = mood == 3, hurt = mood == 1, down = mood == 2;
  const int mouthY = (int)std::floor(my1 + 1.0f);
  if (glad) {   // the mouth open, the tongue lolling
    for (int x = (int)hx + 3; x <= nx - 1; x++) c.set(x, mouthY, rgba(90, 30, 40));
    c.set(nx - 3, mouthY + 1, rgba(214, 96, 110)); c.set(nx - 4, mouthY + 1, rgba(214, 96, 110)); c.set(nx - 4, mouthY + 2, rgba(186, 70, 90));
    c.set(nx - 3, mouthY + 2, rgba(186, 70, 90));
  } else {
    for (int x = (int)hx + 4; x <= nx - 2; x++) c.set(x, mouthY, Mz[0]);
  }
  if (!wolf && !pup) { c.set(nx - 1, mouthY + 1, A[1]); c.set(nx - 2, mouthY + 1, A[2]); }   // the beard
  // the eye: big and bright (shut when downed), the brow over it
  const int ex = (int)std::floor(hx + 1.6f), ey = (int)std::floor(hy - 0.6f);
  if (down) { c.set(ex, ey, mix(kEye, A[1], 0.3f)); c.set(ex + 1, ey, mix(kEye, A[1], 0.3f)); }
  else {
    c.set(ex, ey, kEye); c.set(ex + 1, ey, kEye); c.set(ex, ey + 1, kEye); c.set(ex + 1, ey + 1, mix(kEye, rgba(120, 80, 50), 0.5f));
    c.set(ex, ey, kWhite);   // the catchlight
    c.set(ex, ey - 1, hurt ? A[0] : A[4]); c.set(ex + 1, ey - 1, A[3]);
    if (hurt) c.set(ex + 1, ey - 1, A[0]);
  }
  // the far eye just showing past the brow
  if (!down) c.set(ex + 4, ey, kEye); else c.set(ex + 4, ey, mix(kEye, A[1], 0.3f));
  if (K.blaze) { c.set(ex + 2, ey - 2, K.l[4]); c.set(ex + 3, ey - 1, K.l[3]); }
  // the ears
  if (wolf) {
    const bool back = hurt || down;
    for (int k = 0; k < (pup ? 4 : 6); k++) {
      const int x = back ? 6 - k : 6 - k / 3, y = back ? 5 - k / 3 : 4 - k;
      c.set(x, y, K.d[3]); c.set(x + 1, y, K.d[2]); if (k < 3) c.set(x + 2, y, A[1]);
    }
    c.set(12 + (back ? 1 : 0), 2 + (back ? 2 : 0), K.d[2]); c.set(13 + (back ? 1 : 0), 3 + (back ? 2 : 0), K.d[1]);
  } else if (pup) {
    layered(c, [&](Canvas& t) { capsule(t, V(6.5f, 6.0f), V(4.5f, 13.0f), 1.9f, 1.6f, K.d, 0, 0.06f); }, 0.55f);
  } else {   // the rose ear folded back
    c.set(6, 5, K.d[3]); c.set(5, 5, K.d[2]); c.set(4, 6, K.d[2]); c.set(5, 6, K.d[1]); c.set(4, 7, K.d[1]); c.set(3, 7, K.d[0]);
    if (glad) { c.set(6, 4, K.d[3]); c.set(5, 4, K.d[2]); }
  }
  if (hurt) { c.set(8, 12, kRed[2]); c.set(9, 13, kRed[1]); c.set(8, 13, kRed[0]); }
  if (K.saddle || K.brindle)
    for (int y = 0; y < c.h; y++)
      for (int x = 0; x < c.w; x++) {
        const uint32_t p = c.get(x, y);
        int idx = -1;
        for (int i = 0; i < 5; i++) if (A[i] == p) idx = i;
        if (idx < 0) continue;
        if ((K.brindle && ((x + (y >> 1)) % 3 == 0)) || (K.saddle && y >= 14 && x <= 8)) c.set(x, y, K.d[idx]);
      }
  if (down)
    for (int y = 0; y < c.h; y++)
      for (int x = 0; x < c.w; x++) {
        const uint32_t p = c.get(x, y);
        if (!chA(p)) continue;
        const int l = (int)(luma(p) * 255);
        c.set(x, y, mix(p, rgba(std::min(255, l * 9 / 10 + 8), std::min(255, l * 9 / 10 + 10), std::min(255, l + 18)), 0.62f));
      }
}

}  // namespace

Canvas houndSheetRaw(const HoundLook& L) {
  Canvas sheet(HOUND_W * HOUND_FRAMES, HOUND_H * 3);
  for (int row = 0; row < 3; row++)
    for (int f = 0; f < HOUND_FRAMES; f++) {
      Canvas cell(HOUND_W, HOUND_H);
      HoundPainter hp(cell, L, row, f);
      hp.paint();
      place(sheet, cell, f, row);
    }
  return sheet;
}
Canvas houndSheet(const HoundLook& L) {
  Canvas sheet(HOUND_W * HOUND_FRAMES, HOUND_H * 3);
  for (int row = 0; row < 3; row++)
    for (int f = 0; f < HOUND_FRAMES; f++) {
      Canvas cell(HOUND_W, HOUND_H);
      HoundPainter hp(cell, L, row, f);
      hp.paint();
      outline(cell);
      place(sheet, cell, f, row);
    }
  return sheet;
}
Canvas houndPortrait(const HoundLook& L, int mood) {
  Canvas c(PORTRAIT_W, PORTRAIT_H);
  houndBust(c, L, std::clamp(mood, 0, 3));
  // keep the bust off the canvas's side edges (room for the outline)
  for (int y = 0; y < c.h; y++) { c.set(0, y, 0); c.set(c.w - 1, y, 0); }
  for (int x = 0; x < c.w; x++) c.set(x, 0, 0);
  outline(c);
  return c;
}

// ================================================================================================ graves
namespace {

struct GravePainter {
  Canvas& c;
  const GraveLook& G;
  Ramp St;              // the stone (or the post's wood)
  const int W = GRAVE_W, H = GRAVE_H;
  const int cx = GRAVE_W / 2;
  GravePainter(Canvas& c_, const GraveLook& g) : c(c_), G(g) {
    uint32_t base = G.stone;
    if (!base) {
      switch (G.rite) {
        case 1: base = rgba(132, 128, 120); break;   // cairn: field stones
        case 2: base = rgba(122, 86, 58); break;     // carved post: weathered oak
        case 3: base = rgba(120, 122, 128); break;   // standing stone: grey granite
        case 4: base = rgba(178, 164, 138); break;   // urn niche: pale dressed stone
        default: base = rgba(150, 146, 140); break;  // headstone
      }
    }
    St = ramp(base, 0.85f);
  }
  // the mound: one tile of earth, rounded, its long axis north-south, lit on its top-left; grassed over or fresh
  void mound() {
    // (fixer M8 r3, review: "the plot is a 1-px grey rectangle, it reads as a selection box; the mound sits apart")
    // a grave's raised earth running north-south from the marker toward the camera: a soft rounded oblong (never a
    // box), its crown in the open light, the north-west shoulder lit, the east flank and the south end falling into
    // shade; fresh-turned loam with clods, or older earth the turf is creeping back over from the rim. Round it, a
    // broken ring of separate field stones (each lit top-left, its own shadow down-right), open at the head for the
    // marker. All of it is the sprite: the view draws only the ground shadow under it.
    const Ramp Earth = ramp5(rgba(52, 34, 30), rgba(80, 54, 40), rgba(110, 76, 52), rgba(140, 102, 68), rgba(168, 130, 88));
    const Ramp Gr = ramp5(rgba(44, 78, 46), rgba(60, 102, 52), rgba(78, 128, 58), rgba(100, 150, 64), rgba(136, 178, 82));
    const float mcx = cx - 0.5f, mcy = H - 10.5f, rx = 5.6f, ry = 7.6f;   // (r3: a body's length toward the camera)
    auto shape = [&](float px, float py) {
      const float dx = std::fabs((px - mcx) / rx), dy = std::fabs((py - mcy) / ry);
      return dx * dx * dx + dy * dy * dy;   // <= 1 inside (a rounded oblong)
    };
    auto inside = [&](float px, float py) { return shape(px, py) <= 1.0f; };
    for (int y = 0; y < H; y++)
      for (int x = 1; x < W - 1; x++) {
        const float px = x + 0.5f, py = y + 0.5f;
        const float e = shape(px, py);
        if (e > 1.0f) continue;
        // the light on a low dome: its slope faces up-left on the north-west, away on the south-east
        const float nx = (px - mcx) / rx, ny = (py - mcy) / ry;
        const float lit = -(nx * 0.7f + ny * 0.9f) * std::min(1.0f, e * 1.6f);
        int k = 2;
        if (lit > 0.45f) k = 4; else if (lit > 0.12f) k = 3; else if (lit < -0.55f) k = 0; else if (lit < -0.18f) k = 1;
        if (!inside(px, py + 1.0f)) k = std::max(0, k - 1);   // the south lip, a step down in shade
        uint32_t col = Earth[(size_t)k];
        const uint32_t h = hash3(x, y, G.seed + 3);
        if (h % 6 == 0) col = Earth[(size_t)std::min(4, k + 1)];       // clods catching the light
        else if (h % 9 == 1) col = Earth[(size_t)std::max(0, k - 1)];  // their dark sides
        if (!G.fresh) {   // the turf creeps back from the rim; the crown stays bare longest
          const float grow = (e - 0.35f) * 1.6f;
          if (hashf(x, y, G.seed + 9) < grow) col = Gr[(size_t)std::clamp(k + (h % 5 == 0 ? 1 : 0), 0, 4)];
        }
        c.set(x, y, col);
      }
    // the ring of field stones: a stone every few pixels along the rim, sizes and greys by seed, a gap now and then
    const Ramp Kb = ramp(rgba(152, 148, 138), 0.9f);
    const float PI2 = 6.2831853f;
    int n = 0;
    for (float t = 0.0f; t < PI2; t += 0.36f, n++) {
      const float sx = std::cos(t), sy = std::sin(t);
      if (sy < -0.55f) continue;                                      // the head end: the marker stands there
      const uint32_t h = hash3(n, 7, G.seed + 21);
      if (h % 7 == 0) continue;                                       // a stone gone (sunk, moved): never a drawn box
      const int x0 = (int)std::floor(mcx + (rx + 0.9f) * sx - 0.5f + (float)((h >> 4) % 2) * 0.5f);
      const int y0 = (int)std::floor(mcy + (ry + 0.6f) * sy - 0.5f);
      const int sw = 2 + (int)((h >> 8) % 2), sh = 2;
      const uint32_t base = Kb[(size_t)(1 + (h >> 12) % 3)];
      // its shadow on the ground (down-right), then the stone, lit on its top-left
      for (int j = 0; j < sh; j++) if ((c.get(x0 + sw, y0 + j + 1) >> 24) == 0) c.set(x0 + sw, y0 + j + 1, rgba(28, 32, 20, 150));
      for (int i = 0; i < sw; i++) if ((c.get(x0 + i + 1, y0 + sh) >> 24) == 0) c.set(x0 + i + 1, y0 + sh, rgba(28, 32, 20, 150));
      for (int j = 0; j < sh; j++)
        for (int i = 0; i < sw; i++) {
          uint32_t col = base;
          if (i == 0 && j == 0) col = Kb[4];                                   // the lit corner
          else if (i == sw - 1 && j == sh - 1) col = Kb[0];                    // the shaded one
          else if (j == sh - 1) col = shade(base, 0.82f);
          c.set(x0 + i, y0 + j, col);
        }
    }
  }
  // ---- the five rites (the marker stands at the head of the grave, the north end of the mound)
  void headstone(int baseY) {
    // a slab with a rounded top seen in 3/4: the face toward the camera, its top edge and the right side in depth
    const int x0 = cx - 4, x1 = cx + 3, top = baseY - 13;
    for (int y = top; y <= baseY; y++)
      for (int x = x0; x <= x1 + 1; x++) {
        const float t = (x + 0.5f - (x0 + x1 + 1) * 0.5f) / ((x1 - x0 + 2) * 0.5f);
        const float arch = top + 2.6f * (1.0f - std::sqrt(std::max(0.0f, 1.0f - t * t)));
        if (y + 0.5f < arch) continue;
        int k;
        if (x == x1 + 1) k = 0;                       // the slab's side in depth
        else if (y + 0.5f < arch + 1.0f) k = 4;       // its rounded top lit
        else k = x <= x0 + 1 ? 3 : (x >= x1 - 1 ? 1 : 2);
        if (k >= 1 && k <= 3 && hash3(x, y, G.seed + 11) % 13 == 0) k--;   // the weathering
        c.set(x, y, St[k]);
      }
    // the carving: a ring cross, a sun, a tree or a knot (by seed), its lines cut and shaded
    const int m = (int)(G.seed % 4), mx = cx - 1, my = top + 5;
    const uint32_t cut = St[0], lip = St[4];
    switch (m) {
      case 0: for (int k = -2; k <= 2; k++) { c.set(mx, my + k, cut); c.set(mx + k, my, cut); } c.set(mx - 1, my - 1, lip); c.set(mx + 1, my + 1, cut); break;
      case 1: c.set(mx, my, cut); c.set(mx - 1, my, cut); c.set(mx, my - 1, cut); c.set(mx - 1, my - 1, cut);
              c.set(mx - 3, my, cut); c.set(mx + 2, my, cut); c.set(mx - 1, my - 3, cut); c.set(mx, my + 2, cut); break;
      case 2: for (int y = my - 1; y <= my + 3; y++) c.set(mx, y, cut); c.set(mx - 1, my - 1, cut); c.set(mx + 1, my - 1, cut); c.set(mx - 2, my - 2, cut); c.set(mx + 2, my - 2, cut); break;
      default: for (int k = 0; k < 4; k++) { c.set(mx - 2 + k, my - 1 + (k & 1), cut); c.set(mx - 2 + k, my + 2 - (k & 1), cut); } break;
    }
    for (int x = x0 + 1; x <= x1 - 1; x++) if ((x + G.seed) % 2 == 0) c.set(x, top + 9 + (x & 1) * 0, St[1]);   // the name's line
    for (int x = x0 + 1; x <= x1 - 2; x++) if ((x + G.seed) % 3 != 0) c.set(x, top + 11, St[1]);
    // the base stone it is set in
    for (int x = x0 - 1; x <= x1 + 2; x++) { c.set(x, baseY + 1, St[x <= x0 ? 3 : (x >= x1 + 1 ? 0 : 2)]); c.set(x, baseY, x == x1 + 2 ? St[0] : c.get(x, baseY)); }
    if (!G.fresh) for (int x = x0; x <= x1; x += 2) if (hash3(x, 0, G.seed) & 1) c.set(x, baseY - 1, kMoss[2]);   // moss at its foot
  }
  void cairn(int baseY) {
    // rounded stones heaped in courses, the biggest at the foot, a capstone
    struct S { float x, y, rx, ry; };
    const S stones[] = {{-4.5f, 0.0f, 2.6f, 1.9f}, {-0.5f, 0.4f, 2.8f, 2.0f}, {3.8f, 0.0f, 2.4f, 1.8f},
                        {-2.8f, -2.8f, 2.4f, 1.8f}, {1.6f, -2.9f, 2.5f, 1.8f}, {-0.6f, -5.4f, 2.4f, 1.7f},
                        {0.2f, -7.6f, 1.8f, 1.4f}};
    for (int i = 0; i < 7; i++) {
      const S& s = stones[i];
      const uint32_t tint = hash3(i, 0, G.seed) % 3;
      Ramp R = St;
      if (tint == 1) R = ramp(shade(St[2], 0.9f), 0.85f);
      if (tint == 2) R = ramp(mix(St[2], rgba(150, 134, 110), 0.3f), 0.85f);
      layered(c, [&](Canvas& p) { rock(p, cx + s.x, baseY - 1.0f + s.y, s.rx, s.ry, R, G.seed + i * 17, 5); }, 0.8f);
    }
    if (!G.fresh) { c.set(cx - 5, baseY - 1, kMoss[2]); c.set(cx + 3, baseY - 3, kMoss[3]); c.set(cx - 1, baseY - 6, kMoss[2]); }
  }
  void post(int baseY) {
    // a carved wooden grave post: tall and narrow, a crossbar, a little gabled roof over its head (the marker of the
    // steppe and the marsh), knotwork bands, a carved head (a bird / a horse / a sun by seed)
    // (fixer M8 r1) taller and lighter than the mound, a crossbar: it reads as a marker, never a jug or a sack
    St = ramp(mix(St[2], rgba(170, 150, 120), 0.35f), 0.85f);
    const int x0 = cx - 2, x1 = cx + 1, top = baseY - 13;
    for (int y = top + 3; y <= top + 5; y++)   // the crossbar, through the post, its ends cut square
      for (int x = cx - 6; x <= cx + 5; x++) {
        int k = y == top + 3 ? 4 : (y == top + 5 ? 1 : 2);
        if (x == cx + 5) k = 0;
        if (x == cx - 6) k = std::min(4, k + 1);
        c.set(x, y, St[k]);
      }
    for (int y = top; y <= baseY; y++)
      for (int x = x0; x <= x1 + 1; x++) {
        int k = x == x0 ? 3 : (x == x1 + 1 ? 0 : (x == x1 ? 1 : 2));
        if ((y - top) % 4 == 0 && x <= x1) k = std::min(4, k + 1);   // the bands' raised edges
        if ((y - top) % 4 == 2 && ((x + y) & 1) && x <= x1) k = std::max(0, k - 1);   // the knotwork's cuts
        c.set(x, y, St[k]);
      }
    for (int y = top - 2; y <= top; y++)   // the gabled roof-cap over the head, overhanging, lit on its left pitch
      for (int h = 1 + (3 * (y - top + 2)) / 2, x = cx - 1 - h; x <= cx + h; x++)
        c.set(x, y, St[y == top ? 1 : (x < cx ? 4 : 2)]);
    // the carving under the crossbar: a bird, a horse's head or a sun (by seed), cut into the face
    const int m = (int)(G.seed % 3), my = top + 8;
    if (m == 0) { c.set(cx - 1, my, St[0]); c.set(cx, my, St[0]); c.set(cx - 2, my - 1, St[0]); c.set(cx + 1, my - 1, St[0]); }
    else if (m == 1) { c.set(cx - 1, my - 1, St[0]); c.set(cx, my, St[0]); c.set(cx - 1, my + 1, St[0]); }
    else { c.set(cx - 1, my, St[0]); c.set(cx, my, St[0]); c.set(cx - 1, my - 1, St[4]); c.set(cx, my + 1, St[0]); }
    // a strip of cloth tied round it (the colour by seed)
    static const uint32_t cloth[4] = {rgba(170, 50, 46), rgba(60, 96, 160), rgba(220, 190, 80), rgba(236, 232, 220)};
    const Ramp Cl = ramp(cloth[(G.seed >> 3) & 3]);
    for (int x = x0; x <= x1 + 1; x++) c.set(x, top + 6, Cl[x == x0 ? 3 : (x == x1 + 1 ? 0 : 2)]);
    c.set(x1 + 2, top + 7, Cl[1]); c.set(x1 + 2, top + 8, Cl[2]); c.set(x1 + 3, top + 9, Cl[1]);
  }
  void standingStone(int baseY) {
    // a tall rough menhir, leaning a touch, a spiral or runes cut into its face, lichen
    rock(c, cx - 0.5f, baseY - 7.2f, 4.0f, 7.8f, St, G.seed + 5, 6);
    const int m = (int)(G.seed % 2);
    if (m == 0) {   // a spiral
      const int sx = cx - 1, sy = baseY - 8;
      const int pts[9][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 2}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -2}, {2, -1}};
      for (auto& p : pts) c.set(sx + p[0], sy + p[1], St[0]);
    } else {        // runes down its face
      for (int y = baseY - 12; y <= baseY - 4; y += 3) { c.set(cx - 1, y, St[0]); c.set(cx - 1, y + 1, St[0]); c.set(cx - 2 + (y & 1) * 2, y, St[0]); }
    }
    for (int k = 0; k < 4; k++) {   // lichen
      const int lx = cx - 3 + (int)(hash3(k, 1, G.seed) % 6), ly = baseY - 3 - (int)(hash3(k, 2, G.seed) % 10);
      if (solid(c, lx, ly)) c.set(lx, ly, k & 1 ? rgba(176, 182, 110) : rgba(140, 160, 96));
    }
  }
  void urnNiche(int baseY) {
    // a little stone shrine on a plinth: a gabled cap over an open niche, the urn inside it
    const int x0 = cx - 5, x1 = cx + 4, top = baseY - 12;
    for (int y = top + 2; y <= baseY; y++)
      for (int x = x0; x <= x1 + 1; x++) {
        int k = x == x1 + 1 ? 0 : (x <= x0 + 1 ? 3 : (x >= x1 - 1 ? 1 : 2));
        if (y == baseY || y == baseY - 1) k = std::max(0, k - 1);
        if ((y - top) % 4 == 0 && x <= x1) k = std::max(1, k - 1);   // the courses' joints
        c.set(x, y, St[k]);
      }
    for (int y = top - 1; y <= top + 2; y++)   // the gabled cap, overhanging
      for (int x = x0 - 1; x <= x1 + 2; x++) {
        const int d = std::abs(x * 2 + 1 - (x0 + x1 + 1));
        if (y < top + 2 - (11 - d) / 4) continue;
        c.set(x, y, St[y == top + 2 ? 1 : (x < cx ? 4 : 2)]);
      }
    // the niche: dark inside, the urn's round body lit from the left, its lip
    for (int y = top + 4; y <= baseY - 3; y++) for (int x = cx - 3; x <= cx + 2; x++) c.set(x, y, mix(kInk, St[0], 0.4f));
    const Ramp U = ramp(rgba(170, 96, 60), 0.9f);
    ball(c, cx - 0.5f, baseY - 5.5f, 2.2f, 2.0f, U, 0.0f);
    c.set(cx - 1, baseY - 8, U[3]); c.set(cx, baseY - 8, U[2]); c.set(cx - 2, baseY - 8, U[2]);
    c.set(cx - 1, baseY - 4, kGold[3]); c.set(cx, baseY - 4, kGold[2]);   // a painted band
  }
  // ---- the laid weapon, across the mound (hilt / haft toward the north-west)
  void weapon(int my) {
    // laid along the grave from its head toward the camera: the hilt / the haft's end under the marker
    (void)my;
    const int w = G.weapon;
    if (!w) return;
    const Ramp& M = kIron;
    const int x = cx - 1, y0 = H - 13;
    auto shaft = [&](int y1, int y2, const Ramp& R, int k) { for (int y = y1; y <= y2; y++) { c.set(x, y, R[k + 1]); c.set(x + 1, y, R[k - 1]); } };
    switch (w) {
      case 1: case 5: {   // a sword (a dagger is short): the pommel, the grip, the cross-guard, the blade to its point
        const int len = w == 1 ? 8 : 5;
        c.set(x, y0, kBrass[4]); c.set(x + 1, y0, kBrass[2]);
        c.set(x, y0 + 1, kLeather[2]); c.set(x + 1, y0 + 1, kLeather[1]);
        for (int k = -2; k <= 3; k++) c.set(x + k, y0 + 2, kBrass[k < 0 ? 4 : (k < 2 ? 3 : 1)]);
        for (int y = y0 + 3; y < y0 + 3 + len; y++) { c.set(x, y, M[4]); c.set(x + 1, y, M[2]); }
        c.set(x, y0 + 3 + len, M[3]);
        break;
      }
      case 2: case 6: {   // an axe or a hammer: the haft, its head at the foot of the grave
        shaft(y0, y0 + 9, kWood, 2);
        const int hy = y0 + 8;
        if (w == 2) { for (int y = hy - 1; y <= hy + 2; y++) { c.set(x + 2, y, M[y == hy - 1 ? 4 : 3]); c.set(x + 3, y, M[y < hy + 1 ? 2 : 1]); } c.set(x + 4, hy, M[2]); c.set(x + 4, hy + 1, M[1]); }
        else for (int k = -1; k <= 2; k++) { c.set(x + k, hy, M[k < 1 ? 4 : 2]); c.set(x + k, hy + 1, M[k < 1 ? 2 : 1]); }
        break;
      }
      case 3: {   // an unstrung bow along the grave, the string coiled beside it
        for (int y = y0; y <= y0 + 11; y++) { const int bx = x + ((y > y0 + 2 && y < y0 + 9) ? 1 : 0); c.set(bx, y, kWood[3]); c.set(bx + 1, y, kWood[1]); }
        c.set(x + 3, y0 + 5, kCloth[3]); c.set(x + 3, y0 + 6, kCloth[2]); c.set(x + 4, y0 + 6, kCloth[2]);
        break;
      }
      default: {   // a staff (its stone), a spear (its head), a walking stick
        shaft(y0, y0 + 9, kWood, w == 9 ? 1 : 2);
        if (w == 8) { c.set(x, y0 + 10, M[4]); c.set(x + 1, y0 + 10, M[2]); c.set(x, y0 + 11, M[3]); }
        if (w == 4) { c.set(x, y0 - 1, kCrystal[4]); c.set(x + 1, y0 - 1, kCrystal[2]); }
        break;
      }
    }
  }
  // ---- flowers laid at the marker's foot: a small bunch, the colours by seed
  void flowers(int y) {
    // a small bunch laid at the marker's foot: green stems gathered, the blooms in one or two colours, a tie
    if (!G.flowers) return;
    static const uint32_t pet[5] = {rgba(240, 214, 110), rgba(236, 236, 240), rgba(206, 92, 140), rgba(150, 124, 222), rgba(236, 142, 72)};
    const uint32_t a = pet[G.seed % 5], b = pet[(G.seed / 5 + 2) % 5];
    const int fx = cx + 2;
    c.set(fx - 2, y + 2, kLeaf[2]); c.set(fx - 1, y + 2, kLeaf[1]); c.set(fx, y + 2, kLeaf[1]);   // the stems
    c.set(fx + 1, y + 1, kLeaf[2]); c.set(fx - 1, y + 1, kLeaf[3]);
    c.set(fx - 1, y + 2, kCloth[3]);                                                            // the tie
    c.set(fx + 1, y, a); c.set(fx + 2, y, shade(a, 0.8f)); c.set(fx + 2, y + 1, a); c.set(fx + 3, y + 1, shade(a, 0.75f));
    c.set(fx + 1, y - 1, lighten(a, 0.3f));
    c.set(fx + 3, y, b); c.set(fx + 3, y - 1, lighten(b, 0.3f)); c.set(fx + 4, y, shade(b, 0.75f));
    c.set(fx + 2, y - 1, kGold[4]);
  }
  void paint() {
    mound();
    const int baseY = H - 16;   // the marker stands at the head of the grave, the mound's north end
    switch (G.rite) {
      case 1: cairn(baseY + 1); break;
      case 2: post(baseY); break;
      case 3: standingStone(baseY + 1); break;
      case 4: urnNiche(baseY); break;
      default: headstone(baseY); break;
    }
    weapon(H - 7);
    flowers(baseY + 1);
  }
};

}  // namespace

Canvas graveRaw(const GraveLook& g) {
  Canvas c(GRAVE_W, GRAVE_H);
  GravePainter gp(c, g);
  gp.paint();
  return c;
}
Canvas graveSprite(const GraveLook& g) {
  const Canvas raw = graveRaw(g);
  Canvas c = raw;
  outline(c);
  // (fixer M8 r1, review: "the grave reads as a brown sack or jug") the marker keeps its ink line, but the mound lies
  // in the ground: round the plot the hard outline becomes a soft damp shadow, so the whole never reads as a pot
  const int y0 = GRAVE_H - 16 + 1;
  for (int y = y0; y < GRAVE_H; y++)
    for (int x = 0; x < GRAVE_W; x++)
      if ((raw.get(x, y) >> 24) == 0 && (c.get(x, y) >> 24) != 0) c.set(x, y, rgba(24, 30, 18, 96));
  return c;
}

GraveLook graveLookFor(const cult::Culture* c, uint32_t seed) {
  GraveLook g;
  g.seed = seed;
  if (!c) return g;
  using cult::Archetype;
  // the faith's burial custom first (a cairn, ashes in an urn, the sky burial's post, the sea's marker stone), then the
  // people's own way of marking a grave
  switch (c->faith.burial) {
    case 1: g.rite = 1; break;   // cairn
    case 2: g.rite = 4; break;   // pyre: the ashes in an urn niche
    case 3: g.rite = 2; break;   // sky burial: a carved post where the body was laid out
    case 4: g.rite = 3; break;   // the sea: a standing stone on the shore
    default:
      switch (c->archetype) {
        case Archetype::Fjordfolk: g.rite = (seed & 1) ? 3 : 1; break;
        case Archetype::Highland: g.rite = 1; break;
        case Archetype::Steppe: case Archetype::Marsh: case Archetype::Sylvan: g.rite = 2; break;
        case Archetype::Jade: case Archetype::Dune: g.rite = 4; break;
        case Archetype::Starspire: case Archetype::SunTemple: g.rite = 3; break;
        default: g.rite = 0; break;
      }
      break;
  }
  // the stone or the wood: the land's building stone where it builds in stone, a weathered timber for the posts
  const art::ArchStyle& a = c->arch;
  if (g.rite == 2) {
    g.stone = a.wall == WallMat::Living ? rgba(116, 104, 70) : (a.wall == WallMat::Log ? rgba(110, 76, 52) : rgba(122, 86, 58));
  } else if (a.wall == WallMat::Ashlar) {
    g.stone = rgba(206, 198, 178);
  } else if (a.wall == WallMat::Adobe) {
    g.stone = rgba(190, 150, 110);
  } else if (a.wall == WallMat::Brick) {
    g.stone = rgba(150, 100, 84);
  } else if (a.wall == WallMat::Stone || a.wall == WallMat::Rubble) {
    g.stone = a.wallTint ? mix(opaque(a.wallTint), rgba(140, 140, 140), 0.5f) : 0;
  }
  return g;
}

}  // namespace art
