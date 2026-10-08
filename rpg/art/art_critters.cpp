// EMBERVALE art, M5 "Hearth and Hall": the village animals (art::Critter, rpg/art/art_life.h). ART lane.
// Dogs, cats, hens and the rooster, goats, pigs and ducks with real anatomy, painted from shaded volumes (body, chest,
// haunch, head, muzzle) and thin limbs in the game's high 3/4 view, lit from the top-left, outlined like every sprite.
// Each kind has several coats / plumages by variant. Sheet: CRITTER_FRAMES columns x 3 rows (down, up, right); columns
// 0 idle, 1-4 walk / trot, 5-6 the action (dog: sit and wag; cat: groom; hen and rooster: peck; goat: graze; pig: root;
// duck: dabble), 7 asleep. The feet stand on the cell's row h-2 (the view anchors the cell like a human's).
#include "rpg/art/art_internal.h"

namespace art {

int critterCellW(Critter c) {
  switch (c) {
    case Critter::Dog: return 22;
    case Critter::Cat: return 18;
    case Critter::Goat: return 22;
    case Critter::Pig: return 22;
    case Critter::Rooster: return 14;
    default: return 14;   // hen, duck
  }
}
int critterCellH(Critter c) {
  switch (c) {
    case Critter::Dog: return 18;
    case Critter::Cat: return 16;
    case Critter::Goat: return 22;
    case Critter::Pig: return 16;
    case Critter::Rooster: return 18;
    case Critter::Duck: return 13;
    default: return 15;   // hen
  }
}

namespace {

// ---------------------------------------------------------------- coats
struct Coat {
  Ramp a, b, c;        // main coat, second colour (patches, points, tabby stripes), light parts (chest, muzzle, belly)
  int marks = 0;       // 0 plain, 1 patches of b, 2 stripes of b, 3 b on the back (a saddle), 4 b points (ears, muzzle)
  bool floppy = true;  // ears (dogs)
};
uint32_t rgb(int r, int g, int b) { return rgba(r, g, b); }

Coat dogCoat(uint32_t v) {
  Coat k;
  switch (v % 6) {
    case 0: k.a = ramp(rgb(158, 104, 58)); k.b = ramp(rgb(96, 62, 40)); k.c = ramp(rgb(226, 196, 150)); k.marks = 0; k.floppy = true; break;   // brown hound
    case 1: k.a = ramp(rgb(54, 46, 50)); k.b = ramp(rgb(178, 112, 58)); k.c = ramp(rgb(196, 138, 80)); k.marks = 4; k.floppy = false; break;    // black and tan
    case 2: k.a = ramp(rgb(232, 226, 210)); k.b = ramp(rgb(120, 82, 52)); k.c = ramp(rgb(246, 242, 230)); k.marks = 1; k.floppy = true; break;  // white, brown patches
    case 3: k.a = ramp(rgb(138, 136, 140)); k.b = ramp(rgb(84, 82, 92)); k.c = ramp(rgb(214, 212, 206)); k.marks = 3; k.floppy = false; break;  // grey wolfish
    case 4: k.a = ramp(rgb(214, 160, 82)); k.b = ramp(rgb(170, 116, 56)); k.c = ramp(rgb(240, 214, 160)); k.marks = 0; k.floppy = true; break;  // golden
    default: k.a = ramp(rgb(40, 36, 44)); k.b = ramp(rgb(40, 36, 44)); k.c = ramp(rgb(230, 226, 214)); k.marks = 0; k.floppy = false; break;   // black, white chest
  }
  return k;
}
Coat catCoat(uint32_t v) {
  Coat k;
  switch (v % 6) {
    case 0: k.a = ramp(rgb(222, 146, 70)); k.b = ramp(rgb(170, 92, 44)); k.c = ramp(rgb(246, 218, 170)); k.marks = 2; break;   // orange tabby
    case 1: k.a = ramp(rgb(140, 136, 132)); k.b = ramp(rgb(76, 74, 80)); k.c = ramp(rgb(210, 204, 192)); k.marks = 2; break;   // grey tabby
    case 2: k.a = ramp(rgb(44, 40, 50)); k.b = ramp(rgb(44, 40, 50)); k.c = ramp(rgb(70, 64, 76)); k.marks = 0; break;         // black
    case 3: k.a = ramp(rgb(238, 234, 224)); k.b = ramp(rgb(200, 196, 190)); k.c = ramp(rgb(250, 248, 240)); k.marks = 0; break; // white
    case 4: k.a = ramp(rgb(236, 230, 216)); k.b = ramp(rgb(206, 132, 62)); k.c = ramp(rgb(60, 54, 60)); k.marks = 1; break;    // calico
    default: k.a = ramp(rgb(46, 42, 52)); k.b = ramp(rgb(46, 42, 52)); k.c = ramp(rgb(238, 234, 226)); k.marks = 0; break;     // tuxedo
  }
  return k;
}
Coat goatCoat(uint32_t v) {
  Coat k;
  switch (v % 4) {
    case 0: k.a = ramp(rgb(232, 228, 214)); k.b = ramp(rgb(200, 192, 176)); k.c = ramp(rgb(244, 240, 228)); break;               // white
    case 1: k.a = ramp(rgb(150, 100, 64)); k.b = ramp(rgb(60, 46, 42)); k.c = ramp(rgb(196, 150, 104)); k.marks = 4; break;       // brown, dark points
    case 2: k.a = ramp(rgb(62, 56, 62)); k.b = ramp(rgb(62, 56, 62)); k.c = ramp(rgb(110, 100, 104)); break;                     // black
    default: k.a = ramp(rgb(232, 226, 210)); k.b = ramp(rgb(146, 96, 60)); k.c = ramp(rgb(244, 240, 228)); k.marks = 1; break;   // piebald
  }
  return k;
}
Coat pigCoat(uint32_t v) {
  Coat k;
  switch (v % 4) {
    case 0: k.a = ramp(rgb(234, 170, 160)); k.b = ramp(rgb(214, 140, 134)); k.c = ramp(rgb(246, 200, 190)); break;   // pink
    case 1: k.a = ramp(rgb(234, 176, 164)); k.b = ramp(rgb(60, 50, 56)); k.c = ramp(rgb(246, 204, 194)); k.marks = 1; break;   // spotted
    case 2: k.a = ramp(rgb(64, 56, 62)); k.b = ramp(rgb(64, 56, 62)); k.c = ramp(rgb(112, 98, 100)); break;        // black
    default: k.a = ramp(rgb(196, 112, 62)); k.b = ramp(rgb(150, 80, 46)); k.c = ramp(rgb(224, 156, 104)); break;   // ginger
  }
  return k;
}

// ---------------------------------------------------------------- shared painting helpers
void limb(Canvas& c, float x0, float y0, float x1, float y1, const Ramp& r, int k, int w = 2) {
  const int n = (int)std::ceil(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0)));
  for (int i = 0; i <= n; i++) {
    const float t = n ? (float)i / n : 0;
    const int x = (int)std::floor(x0 + (x1 - x0) * t + 0.5f), y = (int)std::floor(y0 + (y1 - y0) * t + 0.5f);
    c.set(x, y, r[k]);
    if (w > 1) c.set(x + 1, y, r[std::max(0, k - 1)]);
  }
}
// recolour pixels of ramp a (already painted) to ramp b where mask says so (patches, stripes, saddles)
void markCoat(Canvas& c, const Coat& k, int x0, int y0, int x1, int y1, uint32_t seed, int view) {
  if (!k.marks || k.marks == 4) return;
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      const uint32_t p = c.get(x, y);
      int idx = -1;
      for (int i = 0; i < 5; i++) if (k.a[i] == p) idx = i;
      if (idx < 0) continue;
      bool m = false;
      switch (k.marks) {
        case 1: m = fbm(x * 0.32f + seed * 0.01f, y * 0.36f, seed) > 0.56f; break;   // patches
        case 2: m = view == 2 ? ((x + (y >> 1)) % 3 == 0) : ((y + (x >> 2)) % 3 == 0); break;   // tabby stripes
        case 3: m = y <= y0 + (y1 - y0) / 3; break;                                   // a darker saddle on the back
        default: break;
      }
      if (m) c.set(x, y, k.b[idx]);
    }
}
void eyeAt(Canvas& c, int x, int y, bool shut) { c.set(x, y, shut ? rgba(70, 50, 60) : kEye); }

// walk: leg swing per frame (1..4) for the pair that moves together
inline int swingOf(int f) { static const int s[5] = {0, 2, 0, -2, 0}; return s[f & 7] ; }

// ================================================================ quadrupeds
// Built from volumes like a sculptor's maquette: the haunch and the deeper chest joined by a slimmer waist, the neck,
// the head with its muzzle, tapered legs (thick at the thigh, thin at the pastern), the ears and the tail. All
// positions are px from the cell's centre column (cx) and up from the feet's row (g).
enum Quad { kDog, kCat, kGoat, kPig };

struct QuadSpec {
  float hx, hy, hrx, hry;       // haunch centre (x from cx, height above g) and radii
  float chx, chy, crx, cry;     // chest
  float waist;                  // the waist's radius between them
  float legTop;                 // the legs leave the body this high
  float legR0, legR1;           // leg radius at the top / at the foot
  float frontX, hindX;          // the legs' x
  float headX, headY, hrr;      // head centre and radius
  float muzL, muzR, muzDrop;    // muzzle length, radius, how far it droops below the head's centre
  float neckR;                  // 0: no visible neck (cat, pig)
  // front / back views: the body's half-width and the back's length seen foreshortened
  float bodyW, backLen;
};
QuadSpec quadSpec(Quad q) {
  switch (q) {
    case kDog:  return {-3.0f, 6.4f, 2.6f, 2.5f,  2.2f, 6.6f, 2.8f, 3.0f, 2.0f, 5.0f, 1.05f, 0.65f,  2.6f, -3.3f,  5.2f, 10.6f, 2.2f,  2.3f, 1.05f, 0.6f, 1.6f,  2.9f, 2.6f};
    case kCat:  return {-2.4f, 4.4f, 2.1f, 2.0f,  1.7f, 4.4f, 2.0f, 2.1f, 1.5f, 3.5f, 0.75f, 0.55f, 2.0f, -2.6f,  4.0f, 6.8f, 2.0f,   0.9f, 0.8f, 0.5f, 0.0f,  2.2f, 2.0f};
    case kGoat: return {-3.2f, 8.6f, 2.6f, 2.5f,  2.3f, 8.8f, 2.8f, 3.0f, 2.2f, 7.0f, 0.95f, 0.55f, 2.8f, -3.4f,  5.2f, 13.4f, 1.9f,  2.0f, 1.0f, 0.9f, 1.4f,  2.9f, 2.8f};
    default:    return {-2.6f, 5.4f, 3.6f, 3.4f,  1.8f, 5.4f, 3.8f, 3.6f, 3.4f, 3.0f, 1.15f, 0.85f, 3.2f, -3.6f,  5.6f, 5.4f, 2.4f,   1.6f, 1.3f, 0.3f, 0.0f,  4.0f, 3.0f};
  }
}
const Ramp& legRampOf(const Coat& k) { return k.marks == 4 ? k.b : k.a; }
void cap(Canvas& c, float x0, float y0, float x1, float y1, float r0, float r1, const Ramp& R, int bias = 0) {
  capsule(c, V(x0, y0), V(x1, y1), r0, r1, R, bias, 0.06f);
}

// the side view (facing right). frame: 0 idle, 1-4 walk, 5-6 action, 7 asleep
void quadSide(Canvas& c, Quad q, const Coat& k, int f, uint32_t var) {
  const QuadSpec s = quadSpec(q);
  const int W = c.w, g = c.h - 2;
  const float cx = W * 0.5f - (q == kCat ? 0.5f : 1.0f), gy = g + 0.5f;
  const bool walk = f >= 1 && f <= 4, act = f == 5 || f == 6, sleep = f == 7;
  const float bob = walk && (f & 1) ? -1.0f : 0.0f;
  const Ramp& A = k.a;
  const Ramp& LR = legRampOf(k);
  auto Y = [&](float h) { return gy - h + bob; };
  if (sleep) {
    if (q == kGoat) {   // a goat lies with its legs folded under and its head up, dozing
      furBall(c, cx - 0.5f, gy - 2.8f, 5.2f, 2.8f, A, 0.2f, var);
      cap(c, cx + 3.0f, gy - 4.0f, cx + 4.6f, gy - 7.2f, 1.3f, 1.1f, A);
      furBall(c, cx + 5.0f, gy - 7.8f, 1.8f, 1.7f, A, 0.15f, var + 5);
      cap(c, cx + 5.6f, gy - 7.3f, cx + 7.0f, gy - 6.6f, 0.9f, 0.8f, A);
      c.set((int)(cx + 5.4f), (int)(gy - 8.0f), rgba(70, 50, 60));
      c.set((int)(cx + 4.2f), (int)(gy - 10.0f), kBone[3]); c.set((int)(cx + 3.2f), (int)(gy - 10.4f), kBone[2]);
      cap(c, cx + 1.5f, gy - 0.6f, cx + 4.5f, gy - 0.6f, 0.7f, 0.6f, LR, -1);
    } else if (q == kPig) {   // flat out on its side, the trotters out
      for (int i = 0; i < 2; i++) cap(c, cx - 2.0f + i * 4.5f, gy - 3.0f, cx - 1.0f + i * 4.5f, gy - 0.5f, 0.9f, 0.8f, LR, -1);
      furBall(c, cx, gy - 3.6f, 6.4f, 3.4f, A, 0.12f, var);
      furBall(c, cx + 5.8f, gy - 3.4f, 2.4f, 2.3f, A, 0.12f, var + 5);
      ball(c, cx + 8.0f, gy - 3.0f, 1.0f, 1.3f, k.c, 0.0f);
      c.set((int)(cx + 6.0f), (int)(gy - 4.0f), rgba(70, 50, 60));
    } else {   // curled up nose to tail
      const float r = q == kCat ? 3.6f : 4.8f;
      furBall(c, cx - 0.5f, gy - r * 0.55f, r, r * 0.55f, A, 0.22f, var);
      furBall(c, cx + r * 0.62f, gy - r * 0.42f, s.hrr * 0.95f, s.hrr * 0.8f, A, 0.18f, var + 5);
      cap(c, cx - r + 0.5f, gy - 0.8f, cx + r * 0.4f, gy - 0.3f, 0.75f, 0.6f, q == kCat && k.marks == 2 ? k.b : A, 1);   // the tail round the front
      const int hx = (int)(cx + r * 0.62f), hy = (int)(gy - r * 0.42f);
      c.set(hx + 1, hy, rgba(70, 50, 60));
      if (q == kCat) { c.set(hx - 1, hy - 2, A[3]); c.set(hx + 1, hy - 2, A[2]); }
      else if (!k.floppy) c.set(hx - 1, hy - 2, A[3]);
      else { c.set(hx - 1, hy - 1, k.b[1]); c.set(hx - 1, hy, k.b[1]); }
    }
    markCoat(c, k, 0, 0, W - 1, c.h - 1, var, 2);
    return;
  }
  const bool sitDog = q == kDog && act, catSit = q == kCat && act, graze = (q == kGoat || q == kPig) && act;
  const float sw = walk ? (float)swingOf(f) * 0.6f : 0.0f;
  if (sitDog || catSit) {
    // sitting: the haunch on the ground, the chest raised, the forelegs straight
    const float hx0 = cx - (q == kCat ? 1.4f : 2.0f), chx = cx + (q == kCat ? 1.2f : 1.6f);
    const float hr = q == kCat ? 2.2f : 2.8f;
    // the tail along the ground behind (the dog wags it)
    const float wag = sitDog ? (f == 5 ? -1.6f : 0.4f) : 0.0f;
    cap(c, hx0 - hr + 1.0f, gy - 0.8f, hx0 - hr - 1.0f, gy - 1.6f + wag, 0.7f, 0.55f, q == kCat && k.marks == 2 ? k.b : A, 0);
    furBall(c, hx0, gy - hr * 0.9f, hr, hr * 0.9f, A, 0.2f, var);
    furBall(c, chx, gy - hr * 1.9f, hr * 0.85f, hr * 1.1f, A, 0.2f, var + 1);
    ball(c, chx + hr * 0.45f, gy - hr * 1.7f, hr * 0.42f, hr * 0.7f, k.c, 0.05f);   // the chest's light bib
    cap(c, chx + hr * 0.3f, gy - hr * 1.3f, chx + hr * 0.4f, gy - 0.4f, s.legR0, s.legR1, LR, 0);
    c.set((int)(chx + hr * 0.4f) + 1, g, LR[1]);
  } else {
    // far legs (a step behind, in shade), body, near legs
    auto legPair = [&](bool near) {
      const float o = near ? 0.0f : -1.0f;
      const int bias = near ? 0 : -1;
      const float fs = near ? sw : -sw, hs = near ? -sw : sw;
      const float top = Y(s.legTop);
      // the forelegs: straight down; the hind legs: back to the hock, then down
      cap(c, cx + s.frontX + o, top, cx + s.frontX + o + fs, gy - 0.5f - (fs > 0.5f && walk ? 1 : 0), s.legR0, s.legR1, LR, bias);
      const float hockX = cx + s.hindX + o - 0.8f + hs * 0.5f, hockY = gy - s.legTop * 0.45f;
      cap(c, cx + s.hindX + o + 0.4f, top, hockX, hockY, s.legR0 * 1.15f, s.legR1, LR, bias);
      cap(c, hockX, hockY, hockX + 0.5f + hs * 0.5f, gy - 0.5f - (hs > 0.5f && walk ? 1 : 0), s.legR1, s.legR1, LR, bias);
      if (q == kGoat || q == kPig) {   // the cloven hooves
        c.set((int)std::floor(cx + s.frontX + o + fs + 0.5f), g, kInk);
        c.set((int)std::floor(hockX + 0.5f + hs * 0.5f + 0.5f), g, kInk);
      }
    };
    legPair(false);
    // the tail
    const float tx = cx + s.hx - s.hrx + 0.6f, ty0 = Y(s.hy + s.hry * 0.5f);
    switch (q) {
      case kDog: { const float wag = walk ? ((f & 2) ? 0.8f : -0.8f) : 0.0f; cap(c, tx, ty0, tx - 2.2f, ty0 - 2.8f + wag, 0.8f, 0.55f, A); c.set((int)(tx - 2.2f), (int)(ty0 - 2.8f + wag), k.c[3]); break; }
      case kCat: { const Ramp& T = k.marks == 2 ? k.b : A; cap(c, tx, ty0 + 0.5f, tx - 2.0f, ty0 - 0.8f, 0.7f, 0.6f, T); cap(c, tx - 2.0f, ty0 - 0.8f, tx - 1.6f, ty0 - 4.2f + (walk ? (f & 1) : 0), 0.6f, 0.6f, T); break; }
      case kGoat: cap(c, tx, ty0, tx - 0.8f, ty0 - 1.8f, 0.7f, 0.5f, A, 1); break;
      default: c.set((int)tx - 1, (int)ty0, A[2]); c.set((int)tx - 2, (int)ty0 - 1, A[2]); c.set((int)tx - 1, (int)ty0 - 2, A[1]); c.set((int)tx - 2, (int)ty0 - 2, A[3]); break;
    }
    // the body
    if (q == kPig) {
      furBall(c, cx + 0.2f, Y(s.hy), 6.2f, 3.6f, A, 0.12f, var);
    } else {
      furBall(c, cx + s.hx, Y(s.hy), s.hrx, s.hry, A, 0.2f, var);
      cap(c, cx + s.hx + 0.5f, Y(s.hy + 0.4f), cx + s.chx - 0.5f, Y(s.chy + 0.4f), s.waist, s.waist, A, 0);
      furBall(c, cx + s.chx, Y(s.chy), s.crx, s.cry, A, 0.2f, var + 1);
      // the light underside: the chest and the belly line
      for (int x = (int)(cx + s.hx); x <= (int)(cx + s.chx + s.crx * 0.6f); x++)
        for (int y = (int)Y(s.chy) + 1; y < c.h; y++)
          if (solid(c, x, y) && !solid(c, x, y + 1)) { c.set(x, y, k.c[x > cx + s.chx - 1 ? 2 : 1]); break; }
    }
    legPair(true);
  }
  // the neck and the head
  float hx = cx + s.headX, hy = Y(s.headY);
  if (sitDog) { hx = cx + 3.4f; hy = gy - 11.0f; }
  if (catSit) { hx = cx + 2.6f; hy = gy - 7.4f + (f == 6 ? 1.6f : 0.0f); }
  if (graze) { hx = cx + s.headX + (q == kPig ? 0.8f : 0.6f); hy = gy - s.hrr - (f == 6 && q == kGoat ? 1.6f : 0.3f); }
  if (s.neckR > 0 && !sitDog) cap(c, cx + s.chx + 0.8f, Y(s.chy + 1.2f), hx - 0.6f, hy + 0.6f, s.neckR, s.neckR * 0.9f, A, 0);
  if (sitDog) cap(c, cx + 2.0f, gy - 7.5f, hx - 0.4f, hy + 0.5f, s.neckR, s.neckR * 0.9f, A, 0);
  const Ramp& HR = (q == kGoat && k.marks == 4) ? k.b : A;
  furBall(c, hx, hy, s.hrr, s.hrr * 0.92f, HR, 0.12f, var + 7);
  // the muzzle, pointing forward and a little down
  const float mx = hx + s.hrr * 0.55f + s.muzL * 0.5f, my = hy + s.muzDrop;
  if (q == kPig) {
    cap(c, hx + s.hrr * 0.4f, hy + 0.2f, hx + s.hrr + s.muzL - 0.6f, hy + 0.4f, s.muzR, s.muzR, A, 0);
    ball(c, hx + s.hrr + s.muzL - 0.2f, hy + 0.4f, 0.9f, s.muzR, k.c, 0.0f);   // the snout's disc
    c.set((int)(hx + s.hrr + s.muzL), (int)(hy + 0.4f), k.b[0]);
  } else if (q == kCat) {
    c.set((int)(hx + s.hrr) , (int)(hy + 0.6f), k.c[3]);
    c.set((int)(hx + s.hrr) + 1, (int)(hy + 0.6f), rgba(200, 120, 130));
  } else {
    cap(c, hx + s.hrr * 0.4f, hy + s.muzDrop * 0.5f, mx + s.muzL * 0.4f, my, s.muzR, s.muzR * 0.85f, k.marks == 4 ? k.b : (q == kDog ? k.c : HR), 0);
    c.set((int)std::floor(mx + s.muzL * 0.4f + 0.6f), (int)std::floor(my - 0.4f), kInk);   // the nose
  }
  const int hxi = (int)std::floor(hx), hyi = (int)std::floor(hy);
  eyeAt(c, hxi + 1, hyi - (q == kCat ? 0 : 1), false);
  switch (q) {
    case kDog:
      if (k.floppy) { cap(c, hx - 0.6f, hy - 1.4f, hx - 1.0f, hy + 1.4f, 0.8f, 0.7f, k.b, 0); }
      else { c.set(hxi - 1, hyi - 2, A[3]); c.set(hxi - 1, hyi - 3, A[4]); c.set(hxi, hyi - 2, A[2]); c.set(hxi - 2, hyi - 2, A[1]); }
      if (k.marks == 4) c.set(hxi + 1, hyi - 2, k.b[3]);
      break;
    case kCat:
      c.set(hxi - 1, hyi - 2, A[3]); c.set(hxi - 1, hyi - 3, A[4]); c.set(hxi, hyi - 2, A[3]);
      c.set(hxi + 1, hyi - 2, A[2]); c.set(hxi + 1, hyi - 3, A[2]);
      if (catSit && f == 5) cap(c, cx + 2.4f, gy - 3.2f, hx + 1.4f, hy + 1.4f, 0.6f, 0.6f, k.c, 0);   // a paw licked
      if (catSit && f == 6) c.set(hxi + 2, hyi + 2, rgba(220, 120, 140));
      break;
    case kGoat:
      c.set(hxi, hyi - 2, kBone[4]); c.set(hxi - 1, hyi - 3, kBone[3]); c.set(hxi - 2, hyi - 3, kBone[2]); c.set(hxi - 3, hyi - 2, kBone[1]);
      c.set(hxi - 2, hyi, HR[1]); c.set(hxi - 1, hyi, HR[2]);                                 // the ear out sideways
      c.set((int)(mx), (int)(my + 1.2f), k.b[2]); c.set((int)(mx), (int)(my + 2.2f), k.b[1]);   // the beard
      if (graze && f == 6) c.set((int)(mx + 1), (int)(my + 0.8f), kLeaf[3]);
      break;
    default:
      c.set(hxi, hyi - 3, A[3]); c.set(hxi + 1, hyi - 2, A[2]); c.set(hxi - 1, hyi - 2, A[3]);   // the ear flopped forward
      if (graze && f == 6) { c.set((int)(hx + s.hrr + s.muzL), g, kBark[2]); c.set((int)(hx + s.hrr + s.muzL) - 1, g, kBark[1]); }   // turned earth
      break;
  }
  markCoat(c, k, 0, 0, W - 1, c.h - 1, var, 2);
}

// the front view (facing the camera): the head forward and low over the chest, the back rising beyond it on the screen
// (the high 3/4 camera looks down on it), the tail over the back. Parts overlap with a soft contour (layered) so the
// head reads against the body.
void quadFront(Canvas& c, Quad q, const Coat& k, int f, uint32_t var) {
  const QuadSpec s = quadSpec(q);
  const int W = c.w, g = c.h - 2;
  const float cx = W * 0.5f, gy = g + 0.5f;
  const bool walk = f >= 1 && f <= 4, act = f == 5 || f == 6, sleep = f == 7;
  const float bob = walk && (f & 1) ? -1.0f : 0.0f;
  const Ramp& A = k.a;
  const Ramp& LR = legRampOf(k);
  auto lay = [&](auto&& fn) { layered(c, fn, 0.65f); };
  if (sleep) {
    const float r = q == kPig ? 5.6f : (q == kCat ? 3.4f : 4.4f);
    furBall(c, cx, gy - 2.6f, r, 2.6f, A, 0.2f, var);
    if (q == kCat || q == kDog) cap(c, cx - r + 0.6f, gy - 0.6f, cx + r - 0.6f, gy - 0.6f, 0.6f, 0.6f, q == kCat && k.marks == 2 ? k.b : A, 1);
    lay([&](Canvas& p) { furBall(p, cx + (q == kCat ? 0.8f : 0.4f), gy - 1.9f, s.hrr, s.hrr * 0.8f, A, 0.15f, var + 4); });
    c.set((int)cx - 1, (int)(gy - 2.2f), rgba(70, 50, 60)); c.set((int)cx + 1, (int)(gy - 2.2f), rgba(70, 50, 60));
    if (q == kGoat) { c.set((int)cx - 2, (int)(gy - 4.0f), kBone[3]); c.set((int)cx + 2, (int)(gy - 4.0f), kBone[2]); }
    markCoat(c, k, 0, 0, W - 1, c.h - 1, var, 0);
    return;
  }
  const bool sit = (q == kDog || q == kCat) && act, graze = (q == kGoat || q == kPig) && act;
  const float bw = s.bodyW * 1.15f, backR = s.backLen * 0.75f + 0.5f;
  const float backY = gy - s.legTop - 3.0f + bob - (sit ? 1.0f : 0.0f) - (q == kGoat ? 0.5f : 0.0f);
  // the tail over the back
  if (q == kDog) { const float wag = act ? (f == 5 ? -1.8f : 1.8f) : (walk ? ((f & 2) ? 1.0f : -1.0f) : 0.0f); cap(c, cx, backY - backR + 0.8f, cx + wag, backY - backR - 2.0f, 0.75f, 0.5f, A, 1); }
  if (q == kCat) cap(c, cx + 0.8f, backY - backR + 0.8f, cx + 2.2f, backY - backR - 2.2f, 0.6f, 0.55f, k.marks == 2 ? k.b : A, 1);
  furBall(c, cx, backY, bw * 0.95f, backR, A, 0.22f, var);
  // the hind legs peeking out at the sides (or the haunches when sitting)
  if (!sit) {
    cap(c, cx - bw + 0.5f, gy - s.legTop + bob, cx - bw + 0.5f, gy - 0.8f, s.legR0 * 0.85f, s.legR1 * 0.9f, LR, -1);
    cap(c, cx + bw - 0.5f, gy - s.legTop + bob, cx + bw - 0.5f, gy - 0.8f, s.legR0 * 0.85f, s.legR1 * 0.9f, LR, -2);
  } else {
    lay([&](Canvas& p) { furBall(p, cx - bw * 0.85f, gy - 1.7f, 1.7f, 1.5f, A, 0.15f, var + 3); furBall(p, cx + bw * 0.85f, gy - 1.7f, 1.7f, 1.5f, A, 0.15f, var + 3); });
  }
  // the chest and the forelegs
  const float lf = walk ? ((f & 2) ? 1.0f : 0.0f) : 0.0f, rf = walk ? ((f & 2) ? 0.0f : 1.0f) : 0.0f;
  const float legX = bw * 0.45f;
  lay([&](Canvas& p) {
    furBall(p, cx, gy - s.legTop + 0.2f + bob, bw * 0.8f, 1.9f, A, 0.2f, var + 2);
    if (q != kPig) ball(p, cx, gy - s.legTop + 0.4f + bob, bw * 0.42f, 1.3f, k.c, 0.04f);   // the light chest
  });
  cap(c, cx - legX, gy - s.legTop + 1.0f + bob, cx - legX, gy - 0.5f - lf, s.legR0, s.legR1, LR, 0);
  cap(c, cx + legX, gy - s.legTop + 1.0f + bob, cx + legX, gy - 0.5f - rf, s.legR0, s.legR1, LR, -1);
  // the head, forward and low; the muzzle toward the camera
  float hy = gy - s.legTop - 1.6f + bob - (sit ? 1.0f : 0.0f);
  if (graze) hy = gy - s.hrr - 0.4f;
  if (q == kGoat && !graze) hy -= 1.2f;
  const Ramp& HR = (q == kGoat && k.marks == 4) ? k.b : A;
  const float hr = s.hrr + 0.3f;
  lay([&](Canvas& p) { furBall(p, cx, hy, hr, hr * 0.92f, HR, 0.1f, var + 9); });
  const int hxi = (int)std::floor(cx), hyi = (int)std::floor(hy);
  switch (q) {
    case kDog:
      lay([&](Canvas& p) { ball(p, cx, hy + 1.5f, 1.6f, 1.15f, k.marks == 4 ? k.b : k.c, 0.0f); });
      c.set(hxi - 1, hyi + 1, kInk); c.set(hxi, hyi + 1, kInk);
      eyeAt(c, hxi - 2, hyi - 1, false); eyeAt(c, hxi + 1, hyi - 1, false);
      if (k.floppy) { cap(c, cx - hr + 0.3f, hy - 1.6f, cx - hr - 0.2f, hy + 0.8f, 0.8f, 0.75f, k.b, -1); cap(c, cx + hr - 0.3f, hy - 1.6f, cx + hr + 0.2f, hy + 0.8f, 0.8f, 0.75f, k.b, -2); }
      else { c.set(hxi - 2, hyi - 3, A[4]); c.set(hxi - 2, hyi - 4, A[3]); c.set(hxi - 3, hyi - 3, A[3]); c.set(hxi + 1, hyi - 3, A[2]); c.set(hxi + 1, hyi - 4, A[2]); c.set(hxi + 2, hyi - 3, A[1]); }
      break;
    case kCat:
      c.set(hxi - 2, hyi - 2, A[4]); c.set(hxi - 2, hyi - 3, A[3]); c.set(hxi + 1, hyi - 2, A[2]); c.set(hxi + 1, hyi - 3, A[2]);
      eyeAt(c, hxi - 1, hyi, false); eyeAt(c, hxi + 1, hyi, false);
      if (k.c[2] != k.a[2]) { c.set(hxi - 1, hyi + 1, k.c[3]); c.set(hxi + 1, hyi + 1, k.c[2]); }
      c.set(hxi, hyi + 1, rgba(200, 120, 130));
      if (act && f == 5) cap(c, cx - 1.2f, gy - 1.5f, cx - 1.0f, hy + 1.5f, 0.6f, 0.6f, k.c, 0);
      break;
    case kGoat:
      lay([&](Canvas& p) { ball(p, cx, hy + 1.8f, 1.2f, 1.2f, HR, 0.0f); });
      eyeAt(c, hxi - 2, hyi - 1, false); eyeAt(c, hxi + 1, hyi - 1, false);
      c.set(hxi - 1, hyi - 2, kBone[4]); c.set(hxi - 2, hyi - 3, kBone[3]); c.set(hxi + 1, hyi - 2, kBone[2]); c.set(hxi + 2, hyi - 3, kBone[2]);
      c.set(hxi - 3, hyi - 1, HR[2]); c.set(hxi + 2, hyi - 1, HR[1]);
      c.set(hxi, hyi + 3, k.b[1]);
      break;
    default:
      lay([&](Canvas& p) { ball(p, cx, hy + 1.0f, 1.6f, 1.15f, k.c, 0.0f); });
      c.set(hxi - 1, hyi + 1, k.b[0]); c.set(hxi, hyi + 1, k.b[0]);
      eyeAt(c, hxi - 2, hyi - 1, false); eyeAt(c, hxi + 1, hyi - 1, false);
      c.set(hxi - 3, hyi - 2, A[3]); c.set(hxi - 2, hyi - 3, A[3]); c.set(hxi + 2, hyi - 2, A[2]); c.set(hxi + 1, hyi - 3, A[2]);
      break;
  }
  markCoat(c, k, 0, 0, W - 1, c.h - 1, var, 0);
}

// the back view (walking away): the rump nearest the camera, the back rising beyond it to the head
void quadBack(Canvas& c, Quad q, const Coat& k, int f, uint32_t var) {
  const QuadSpec s = quadSpec(q);
  const int W = c.w, g = c.h - 2;
  const float cx = W * 0.5f, gy = g + 0.5f;
  const bool walk = f >= 1 && f <= 4, act = f == 5 || f == 6, sleep = f == 7;
  const float bob = walk && (f & 1) ? -1.0f : 0.0f;
  const Ramp& A = k.a;
  const Ramp& LR = legRampOf(k);
  auto lay = [&](auto&& fn) { layered(c, fn, 0.65f); };
  const float bw = s.bodyW * 1.15f, backR = s.backLen * 0.75f + 0.5f;
  if (sleep) {
    const float r = q == kPig ? 5.6f : (q == kCat ? 3.4f : 4.4f);
    furBall(c, cx - 0.5f, gy - 4.4f, s.hrr * 0.85f, s.hrr * 0.7f, A, 0.15f, var + 4);
    lay([&](Canvas& p) { furBall(p, cx, gy - 2.6f, r, 2.6f, A, 0.2f, var); });
    markCoat(c, k, 0, 0, W - 1, c.h - 1, var, 1);
    return;
  }
  const bool sit = (q == kDog || q == kCat) && act, graze = (q == kGoat || q == kPig) && act;
  // the head beyond the back (grazing: hidden but for the ears and horns)
  const float hy = gy - s.legTop - 5.0f + bob - (sit ? 1.0f : 0.0f) + (graze ? 2.0f : 0.0f) - (q == kGoat ? 0.8f : 0.0f);
  furBall(c, cx, hy, s.hrr, s.hrr * 0.9f, A, 0.12f, var + 9);
  const int hxi = (int)std::floor(cx), hyi = (int)std::floor(hy);
  switch (q) {
    case kDog: if (k.floppy) { c.set(hxi - 3, hyi, k.b[2]); c.set(hxi + 2, hyi, k.b[1]); c.set(hxi - 3, hyi + 1, k.b[1]); c.set(hxi + 2, hyi + 1, k.b[0]); } else { c.set(hxi - 2, hyi - 3, A[3]); c.set(hxi + 1, hyi - 3, A[2]); c.set(hxi - 2, hyi - 2, A[3]); c.set(hxi + 1, hyi - 2, A[2]); } break;
    case kCat: c.set(hxi - 2, hyi - 2, A[3]); c.set(hxi + 1, hyi - 2, A[2]); c.set(hxi - 2, hyi - 3, A[4]); c.set(hxi + 1, hyi - 3, A[3]); break;
    case kGoat: c.set(hxi - 1, hyi - 2, kBone[3]); c.set(hxi - 2, hyi - 3, kBone[3]); c.set(hxi + 1, hyi - 2, kBone[2]); c.set(hxi + 2, hyi - 3, kBone[2]); c.set(hxi - 3, hyi, A[2]); c.set(hxi + 2, hyi, A[1]); break;
    default: c.set(hxi - 2, hyi - 2, A[3]); c.set(hxi + 2, hyi - 2, A[2]); break;
  }
  // the back, the hind legs, then the rump nearest the camera
  lay([&](Canvas& p) { furBall(p, cx, gy - s.legTop - 2.4f + bob, bw * 0.92f, backR, A, 0.22f, var); });
  const float lf = walk ? ((f & 2) ? 1.0f : 0.0f) : 0.0f, rf = walk ? ((f & 2) ? 0.0f : 1.0f) : 0.0f;
  if (!sit) {
    cap(c, cx - bw * 0.5f, gy - s.legTop + bob, cx - bw * 0.5f, gy - 0.5f - lf, s.legR0 * 1.1f, s.legR1, LR, 0);
    cap(c, cx + bw * 0.5f, gy - s.legTop + bob, cx + bw * 0.5f, gy - 0.5f - rf, s.legR0 * 1.1f, s.legR1, LR, -1);
  }
  const float rumpY = gy - s.legTop + 0.2f + bob + (sit ? s.legTop - 2.2f : 0.0f);
  lay([&](Canvas& p) { furBall(p, cx, rumpY, bw, s.hry * 0.85f, A, 0.2f, var + 1); });
  // the tail toward the camera
  const float ry = rumpY - s.hry * 0.5f;
  switch (q) {
    case kDog: { const float wag = act ? (f == 5 ? -2.0f : 2.0f) : (walk ? ((f & 2) ? 1.0f : -1.0f) : 0.0f); cap(c, cx, ry, cx + wag, ry - 3.0f, 0.8f, 0.55f, A, 1); c.set((int)(cx + wag), (int)(ry - 3.0f), k.c[3]); break; }
    case kCat: cap(c, cx, ry, cx + 1.4f, ry - 3.6f, 0.6f, 0.55f, k.marks == 2 ? k.b : A, 1); break;
    case kGoat: c.set((int)cx, (int)ry - 1, A[4]); c.set((int)cx, (int)ry - 2, A[3]); break;
    default: c.set((int)cx, (int)ry, A[1]); c.set((int)cx + 1, (int)ry - 1, A[1]); c.set((int)cx, (int)ry - 2, A[3]); break;
  }
  markCoat(c, k, 0, 0, W - 1, c.h - 1, var, 1);
}

// ================================================================ birds: hen, rooster, duck
struct Plume { Ramp body, wing, tail, light; uint32_t comb = 0, beak = 0, legs = 0; bool speckle = false; uint32_t head = 0; };
Plume henPlume(uint32_t v) {
  Plume p;
  p.comb = rgba(212, 44, 48); p.beak = rgba(236, 188, 72); p.legs = rgba(226, 176, 70);
  switch (v % 5) {
    case 0: p.body = ramp(rgb(244, 240, 228)); p.wing = ramp(rgb(226, 220, 204)); p.tail = p.body; p.light = ramp(rgb(252, 250, 244)); break;     // white
    case 1: p.body = ramp(rgb(170, 100, 52)); p.wing = ramp(rgb(140, 78, 42)); p.tail = ramp(rgb(70, 50, 44)); p.light = ramp(rgb(206, 140, 80)); break;   // brown
    case 2: p.body = ramp(rgb(64, 60, 70)); p.wing = ramp(rgb(54, 50, 60)); p.tail = p.body; p.light = ramp(rgb(220, 216, 210)); p.speckle = true; break;   // speckled
    case 3: p.body = ramp(rgb(228, 186, 116)); p.wing = ramp(rgb(206, 160, 94)); p.tail = ramp(rgb(170, 120, 66)); p.light = ramp(rgb(244, 214, 156)); break;   // buff
    default: p.body = ramp(rgb(48, 46, 58)); p.wing = ramp(rgb(40, 50, 60)); p.tail = ramp(rgb(30, 54, 58)); p.light = ramp(rgb(70, 74, 86)); break;   // black
  }
  return p;
}
Plume roosterPlume(uint32_t v) {
  Plume p;
  p.comb = rgba(222, 40, 44); p.beak = rgba(236, 188, 72); p.legs = rgba(226, 176, 70);
  switch (v % 3) {
    case 0: p.body = ramp(rgb(176, 72, 40)); p.wing = ramp(rgb(120, 52, 36)); p.tail = ramp(rgb(30, 60, 62)); p.light = ramp(rgb(234, 160, 70)); p.head = rgba(232, 150, 60); break;   // red
    case 1: p.body = ramp(rgb(222, 170, 80)); p.wing = ramp(rgb(170, 110, 52)); p.tail = ramp(rgb(36, 40, 48)); p.light = ramp(rgb(246, 210, 120)); p.head = rgba(246, 204, 110); break;   // golden
    default: p.body = ramp(rgb(244, 240, 228)); p.wing = ramp(rgb(220, 214, 198)); p.tail = ramp(rgb(232, 228, 214)); p.light = ramp(rgb(252, 250, 244)); p.head = rgba(250, 248, 240); break; // white
  }
  return p;
}
Plume duckPlume(uint32_t v) {
  Plume p;
  p.beak = rgba(236, 150, 50); p.legs = rgba(236, 140, 50);
  switch (v % 3) {
    case 0: p.body = ramp(rgb(244, 242, 232)); p.wing = ramp(rgb(226, 222, 208)); p.tail = p.body; p.light = ramp(rgb(252, 252, 246)); p.head = 0; break;   // white
    case 1: p.body = ramp(rgb(150, 140, 132)); p.wing = ramp(rgb(110, 92, 80)); p.tail = ramp(rgb(60, 56, 62)); p.light = ramp(rgb(200, 190, 176)); p.head = rgba(40, 110, 70); p.beak = rgba(220, 200, 80); break;   // mallard drake
    default: p.body = ramp(rgb(150, 112, 74)); p.wing = ramp(rgb(120, 86, 58)); p.tail = ramp(rgb(96, 70, 50)); p.light = ramp(rgb(196, 160, 116)); p.head = 0; p.beak = rgba(210, 130, 60); break;   // brown hen
  }
  return p;
}

enum Bird { kHen, kRooster, kDuck };

void birdSide(Canvas& c, Bird b, const Plume& p, int f, uint32_t var) {
  const int W = c.w, g = c.h - 2;
  const bool walk = f >= 1 && f <= 4, act = f == 5 || f == 6, sleep = f == 7;
  const bool duck = b == kDuck, roo = b == kRooster;
  const int bob = walk && (f & 1) ? -1 : 0;
  const float cx = W * 0.5f - (duck ? 1.0f : 0.5f);
  const float bodyRx = duck ? 3.6f : (roo ? 3.4f : 3.2f), bodyRy = duck ? 2.2f : (roo ? 2.8f : 2.6f);
  const float legH = duck ? 1.0f : (roo ? 3.2f : 2.4f);
  const float by = g - legH - bodyRy + 0.5f + bob + (sleep ? legH : 0.0f);
  // legs (the near one a step ahead on the walk)
  if (!sleep) {
    const int sw = walk ? swingOf(f) / 2 : 0;
    const Ramp L = ramp(p.legs);
    limb(c, cx - 0.5f, by + bodyRy - 0.5f, cx - 0.5f - sw, (float)g, L, 1, 1);
    limb(c, cx + 0.5f, by + bodyRy - 0.5f, cx + 0.5f + sw, (float)g - (sw > 0 ? 1 : 0), L, 2, 1);
    c.set((int)(cx + 1.5f + sw), g, L[2]);
  }
  // the tail feathers
  const bool dabble = duck && act;
  if (roo) {   // the sickles: tall arcs of dark glossy feathers
    for (int i = 0; i < 3; i++) limb(c, cx - bodyRx + 1.0f, by - 0.5f, cx - bodyRx - 1.5f + i * 0.6f, by - 5.5f + i, p.tail, 3 - (i & 1), 1);
    c.set((int)(cx - bodyRx - 2), (int)(by - 3), p.tail[4]);
  } else if (duck) {
    c.set((int)(cx - bodyRx), (int)by - (dabble ? 2 : 1), p.tail[2]); c.set((int)(cx - bodyRx) - 1, (int)by - (dabble ? 3 : 1), p.tail[1]);
  } else {
    ball(c, cx - bodyRx + 0.8f, by - 1.8f, 1.4f, 2.0f, p.tail, 0.0f);
  }
  // the body: a plump egg, the wing folded on its side
  if (dabble) {   // up-ended in the water: the tail high, the head down
    ball(c, cx, by + 0.5f, bodyRx * 0.9f, bodyRy * 1.1f, p.body, 0.08f);
    ball(c, cx - 0.5f, by + 0.3f, bodyRx * 0.6f, bodyRy * 0.7f, p.wing, 0.05f);
  } else {
    ball(c, cx, by, bodyRx, bodyRy, p.body, 0.08f);
    ball(c, cx - 0.6f, by + 0.2f, bodyRx * 0.62f, bodyRy * 0.62f, p.wing, 0.05f);
  }
  if (p.speckle) for (int y = (int)(by - bodyRy); y <= (int)(by + bodyRy); y++) for (int x = 0; x < W; x++) if (solid(c, x, y) && hash3(x, y, var) % 4 == 0) c.set(x, y, p.light[2]);
  // neck and head
  float hx = cx + bodyRx - 0.8f, hyy = by - bodyRy - (duck ? 0.6f : 1.3f);
  const bool peck = !duck && act;
  if (peck) { hx = cx + bodyRx + 0.5f; hyy = f == 5 ? by + 0.5f : (float)g - 1.2f; }
  if (dabble) { hx = cx + bodyRx + 0.2f; hyy = by + bodyRy + 0.4f; }
  if (sleep) { hx = cx + bodyRx * 0.4f; hyy = by - bodyRy + 0.6f; }   // tucked in
  const Ramp H = p.head ? ramp(p.head) : p.body;
  ball(c, hx, hyy, duck ? 1.6f : 1.5f, duck ? 1.4f : 1.5f, H, 0.0f);
  const int hxi = (int)std::floor(hx), hyi = (int)std::floor(hyy);
  if (!sleep) {
    eyeAt(c, hxi + (duck ? 0 : 0), hyi - (duck ? 0 : 1), false);
    if (duck) { c.set(hxi + 2, hyi, p.beak); c.set(hxi + 3, hyi, p.beak); c.set(hxi + 2, hyi + 1, shade(p.beak, 0.8f)); c.set(hxi + 3, hyi + 1, shade(p.beak, 0.7f)); }
    else {
      c.set(hxi + 2, hyi, p.beak); c.set(hxi + 2, hyi - (peck ? 0 : 1), shade(p.beak, 1.08f));
      const Ramp Cm = ramp(p.comb);
      c.set(hxi, hyi - 2, Cm[3]); c.set(hxi + 1, hyi - 2, Cm[2]);
      if (roo) { c.set(hxi - 1, hyi - 2, Cm[2]); c.set(hxi, hyi - 3, Cm[3]); c.set(hxi + 1, hyi - 3, Cm[2]); c.set(hxi + 1, hyi + 1, Cm[1]); c.set(hxi + 1, hyi + 2, Cm[1]); }
      else c.set(hxi + 1, hyi + 1, Cm[1]);
    }
    if (duck && p.head) { c.set(hxi - 1, hyi + 2, kWhite); }   // the drake's white collar
  } else {
    if (!duck) { const Ramp Cm = ramp(p.comb); c.set(hxi, hyi - 2, Cm[2]); }
  }
}

void birdFront(Canvas& c, Bird b, const Plume& p, int f, uint32_t var, bool back) {
  const int W = c.w, g = c.h - 2;
  const bool walk = f >= 1 && f <= 4, act = f == 5 || f == 6, sleep = f == 7;
  const bool duck = b == kDuck, roo = b == kRooster;
  const int bob = walk && (f & 1) ? -1 : 0;
  const float cx = W * 0.5f - 0.5f;
  const float bodyRx = duck ? 2.8f : 2.6f, bodyRy = duck ? 2.6f : (roo ? 3.0f : 2.7f);
  const float legH = duck ? 1.0f : (roo ? 3.0f : 2.2f);
  const float by = g - legH - bodyRy + 0.5f + bob + (sleep ? legH : 0.0f);
  if (!sleep) {
    const Ramp L = ramp(p.legs);
    const int lf = walk ? ((f & 2) ? 1 : 0) : 0;
    limb(c, cx - 1.0f, by + bodyRy - 0.5f, cx - 1.0f, (float)g - lf, L, 2, 1);
    limb(c, cx + 1.0f, by + bodyRy - 0.5f, cx + 1.0f, (float)g - (1 - lf) * (walk ? 1 : 0), L, 1, 1);
  }
  const bool peck = !duck && act, dabble = duck && act;
  const float hyy = back ? by - bodyRy - 0.8f : (peck ? (f == 5 ? by + 1.0f : (float)g - 1.0f) : (dabble ? by + 1.5f : by - bodyRy - 0.6f));
  const Ramp H = p.head ? ramp(p.head) : p.body;
  if (back) {
    ball(c, cx, hyy, 1.5f, 1.5f, H, 0.0f);
    if (!duck) { const Ramp Cm = ramp(p.comb); c.set((int)cx, (int)hyy - 2, Cm[3]); if (roo) c.set((int)cx, (int)hyy - 3, Cm[2]); }
  }
  ball(c, cx, by, bodyRx, bodyRy, p.body, 0.08f);
  if (back) {   // the tail toward the camera
    if (roo) for (int i = -1; i <= 1; i++) limb(c, cx + i, by - 1.0f, cx + i * 2.0f, by - 4.5f + std::abs(i), p.tail, 3 - std::abs(i), 1);
    else ball(c, cx, by - 1.2f, 1.5f, 1.3f, p.tail, 0.0f);
    ball(c, cx - bodyRx + 0.8f, by + 0.4f, 1.0f, bodyRy * 0.6f, p.wing, 0.0f);
    ball(c, cx + bodyRx - 0.8f, by + 0.4f, 1.0f, bodyRy * 0.6f, p.wing, 0.0f);
  } else {
    if (roo) for (int i = -1; i <= 1; i += 2) limb(c, cx + i * 0.5f, by - 1.0f, cx + i * 2.5f, by - 5.0f, p.tail, 2, 1);
    ball(c, cx - bodyRx + 0.6f, by + 0.3f, 0.9f, bodyRy * 0.6f, p.wing, 0.0f);
    ball(c, cx + bodyRx - 0.6f, by + 0.3f, 0.9f, bodyRy * 0.6f, p.wing, 0.0f);
    if (!sleep) {
      ball(c, cx, hyy, 1.6f, 1.5f, H, 0.0f);
      const int hxi = (int)std::floor(cx + 0.5f), hyi = (int)std::floor(hyy);
      eyeAt(c, hxi - 2 + 1, hyi - 1, false); eyeAt(c, hxi + 1, hyi - 1, false);
      if (duck) { c.set(hxi - 1, hyi + 1, p.beak); c.set(hxi, hyi + 1, p.beak); c.set(hxi - 1, hyi + 2, shade(p.beak, 0.8f)); c.set(hxi, hyi + 2, shade(p.beak, 0.7f)); }
      else {
        c.set(hxi, hyi + 1, p.beak); c.set(hxi - 1, hyi + 1, shade(p.beak, 1.1f));
        const Ramp Cm = ramp(p.comb);
        c.set(hxi - 1, hyi - 2, Cm[3]); c.set(hxi, hyi - 2, Cm[2]);
        if (roo) { c.set(hxi - 1, hyi - 3, Cm[3]); c.set(hxi, hyi - 3, Cm[2]); }
        c.set(hxi, hyi + 2, Cm[1]);
      }
      if (duck && p.head) { c.set(hxi - 1, hyi + 3, kWhite); c.set(hxi, hyi + 3, kWhite); }
    } else {
      ball(c, cx, by - bodyRy + 1.0f, 1.4f, 1.2f, H, 0.0f);
      if (!duck) { const Ramp Cm = ramp(p.comb); c.set((int)cx, (int)(by - bodyRy - 0.5f), Cm[2]); }
    }
  }
  if (p.speckle) for (int y = 0; y < c.h; y++) for (int x = 0; x < W; x++) if (solid(c, x, y) && c.get(x, y) != p.beak && hash3(x, y, var) % 4 == 0) {
    const uint32_t q = c.get(x, y);
    for (int i = 0; i < 5; i++) if (q == p.body[i]) c.set(x, y, p.light[std::min(4, i + 1)]);
  }
}

}  // namespace

Canvas critterSheet(Critter cr, uint32_t variant) {
  const int cw = critterCellW(cr), ch = critterCellH(cr);
  Canvas out(cw * CRITTER_FRAMES, ch * 3);
  const uint32_t v = variant;
  for (int row = 0; row < 3; row++)
    for (int f = 0; f < CRITTER_FRAMES; f++) {
      Canvas cell(cw, ch);
      switch (cr) {
        case Critter::Dog: case Critter::Cat: case Critter::Goat: case Critter::Pig: {
          const Quad q = cr == Critter::Dog ? kDog : cr == Critter::Cat ? kCat : cr == Critter::Goat ? kGoat : kPig;
          const Coat k = q == kDog ? dogCoat(v) : q == kCat ? catCoat(v) : q == kGoat ? goatCoat(v) : pigCoat(v);
          if (row == 0) quadFront(cell, q, k, f, v);
          else if (row == 1) quadBack(cell, q, k, f, v);
          else quadSide(cell, q, k, f, v);
          break;
        }
        default: {
          const Bird b = cr == Critter::Duck ? kDuck : (cr == Critter::Rooster ? kRooster : kHen);
          const Plume p = b == kDuck ? duckPlume(v) : (b == kRooster ? roosterPlume(v) : henPlume(v));
          if (row == 2) birdSide(cell, b, p, f, v);
          else birdFront(cell, b, p, f, v, row == 1);
          break;
        }
      }
      outline(cell, 0.9f);
      place(out, cell, f, row);
    }
  return out;
}

}  // namespace art
