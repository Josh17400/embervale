// EMBERVALE art: props (nature, camp/town, dungeon, interior furniture). See rpg/art.h for the contract and rpg/art/art_internal.h for the shared helpers.
#include "rpg/art/art_internal.h"

namespace art {

// =====================================================================================================
// 5. props
// =====================================================================================================
namespace {

// ---- nature building blocks ------------------------------------------------------------------------
struct Blob { float x, y, r; };

// Lush leaf canopy made of overlapping clumps. Clumps are painted back (top) to front (bottom); each has
// its own lit top-left and shaded underside, a scalloped edge, clumpy leaf texture and sparse highlights.
void canopy(Canvas& c, std::vector<Blob> blobs, const Ramp& R, uint32_t seed, float squash = 0.88f) {
  std::stable_sort(blobs.begin(), blobs.end(), [](const Blob& a, const Blob& b) { return a.y < b.y; });
  float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
  for (auto& b : blobs) { minX = std::min(minX, b.x - b.r); maxX = std::max(maxX, b.x + b.r); minY = std::min(minY, b.y - b.r); maxY = std::max(maxY, b.y + b.r); }
  float gcx = (minX + maxX) * 0.5f, gcy = (minY + maxY) * 0.5f, gw = std::max(1.0f, maxX - minX), gh = std::max(1.0f, maxY - minY);
  int bi = 0;
  for (auto& b : blobs) {
    float ph = hashf(bi, 3, seed) * TAU, ph2 = hashf(bi, 5, seed) * TAU;
    bi++;
    float ry = b.r * squash;
    for (int y = (int)std::floor(b.y - ry - 2); y <= (int)std::ceil(b.y + ry + 2); y++)
      for (int x = (int)std::floor(b.x - b.r - 2); x <= (int)std::ceil(b.x + b.r + 2); x++) {
        float dx = (x + 0.5f - b.x) / b.r, dy = (y + 0.5f - b.y) / ry;
        float d = std::sqrt(dx * dx + dy * dy);
        float ang = std::atan2(dy, dx);
        float edge = 1.0f + 0.09f * std::sin(ang * 6 + ph) + 0.05f * std::sin(ang * 11 + ph2);
        if (d > edge) continue;
        float nx = dx / edge, ny = dy / edge;
        float l = lightAt(nx * 0.82f, ny * 0.82f);
        l += ((gcx - (x + 0.5f)) / gw + (gcy - (y + 0.5f)) / gh) * 0.35f;     // whole-crown light
        l += (vnoise(x * 0.55f, y * 0.55f, seed) - 0.5f) * 0.55f;            // clumpy leaves
        int k = lightIndex(l, x, y, 0.12f);
        if (d > edge - 0.12f && ny > 0.2f) k = std::max(0, k - 1);              // dark rim on the underside
        c.set(x, y, R[k]);
      }
  }
  // sparkle of sunlit leaves on the light side
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      uint32_t p = c.get(x, y);
      if (p != R[3]) continue;
      uint32_t h = hash3(x, y, seed + 99);
      if (h % 9 == 0) { c.set(x, y, R[4]); if (h % 2) c.set(x + 1, y, R[4]); }
      else if (h % 13 == 0 && c.get(x, y + 1) == R[3]) c.set(x, y + 1, R[2]);
    }
}

// trunk: vertical cylinder in bark with streaks, flared roots at the base
void trunk(Canvas& c, float cx, int baseY, int topY, float w, const Ramp& R, uint32_t seed) {
  for (int y = topY; y <= baseY; y++) {
    float t = (float)(y - topY) / std::max(1, baseY - topY);
    float hw = w * 0.5f + (t > 0.75f ? (t - 0.75f) * 7.0f : 0.0f);
    int x0 = (int)std::floor(cx - hw), x1 = (int)std::ceil(cx + hw) - 1;
    for (int x = x0; x <= x1; x++) {
      float u = (x + 0.5f - cx) / std::max(0.5f, hw);
      int k = lightIndex(lightAt(u * 0.9f, 0) + (hashf(x, y / 3, seed) - 0.5f) * 0.25f, x, y, 0.0f);
      if (hash3(x, y / 2, seed) % 7 == 0) k = std::max(0, k - 1);   // bark streaks
      c.set(x, y, R[k]);
    }
  }
}

// little clump of grass blades at the base of things (anchors props visually)
void grassTuft(Canvas& c, int x, int y, uint32_t seed, int n = 4) {
  for (int i = 0; i < n; i++) {
    int bx = x + i * 2 - n + (int)(hash3(i, 0, seed) % 2);
    int h = 2 + (int)(hash3(i, 1, seed) % 3);
    for (int j = 0; j < h; j++) c.set(bx + (j == h - 1 && (i & 1) ? 1 : 0), y - j, kLeaf[j == h - 1 ? 3 : 2]);
  }
}

// lays a blanket of snow on the upward-facing top of a shape (gentle slopes only, so sides stay bare)
void snowCap(Canvas& c, int depth, uint32_t seed) {
  std::vector<int> top(c.w, -1);
  for (int x = 0; x < c.w; x++)
    for (int y = 0; y < c.h; y++) if (solid(c, x, y)) { top[x] = y; break; }
  for (int x = 0; x < c.w; x++) {
    if (top[x] < 0) continue;
    int l = x > 0 && top[x - 1] >= 0 ? top[x - 1] : top[x] + 9, r = x + 1 < c.w && top[x + 1] >= 0 ? top[x + 1] : top[x] + 9;
    int slope = std::max(std::abs(l - top[x]), std::abs(r - top[x]));
    if (slope > 2) continue;
    int d = depth + (int)(hash3(x, 0, seed) % 2) - (slope == 2 ? 1 : 0);
    for (int k = 0; k < d && solid(c, x, top[x] + k); k++) c.set(x, top[x] + k, kSnow[k == 0 ? 4 : (k == d - 1 ? 2 : 3)]);
  }
}

// ---- trees ------------------------------------------------------------------------------------------
void oakTree(Canvas& c, const Ramp& leaf, uint32_t seed, int variant) {
  const int W = c.w, H = c.h;
  float cx = W * 0.5f;
  trunk(c, cx, H - 2, H / 2, 6.0f, kBark, seed);
  // a couple of branches reaching into the crown
  thickLine(c, cx - 1, H * 0.62f, cx - 7, H * 0.42f, 2.0f, kBark[1]);
  thickLine(c, cx + 1, H * 0.6f, cx + 8, H * 0.44f, 2.0f, kBark[1]);
  std::vector<Blob> b;
  if (variant == 0) {
    b = {{cx, 11, 9}, {cx - 9, 15, 8}, {cx + 9, 14, 8}, {cx - 13, 22, 6.5f}, {cx + 13, 22, 6.5f},
         {cx - 5, 22, 9}, {cx + 5, 23, 9}, {cx, 28, 8}, {cx - 10, 29, 6}, {cx + 10, 29, 6}, {cx, 17, 8}};
  } else if (variant == 1) {
    b = {{cx + 1, 10, 8}, {cx - 8, 14, 7}, {cx + 9, 15, 7.5f}, {cx - 11, 22, 6}, {cx + 11, 23, 6},
         {cx - 4, 21, 8.5f}, {cx + 5, 22, 8}, {cx - 1, 27, 7.5f}, {cx + 8, 28, 5}};
  } else {
    b = {{cx - 1, 11, 9}, {cx - 10, 16, 7.5f}, {cx + 9, 13, 8}, {cx - 12, 24, 6}, {cx + 13, 21, 6.5f},
         {cx - 5, 23, 9}, {cx + 6, 22, 8.5f}, {cx, 29, 7.5f}, {cx - 9, 30, 5.5f}, {cx + 10, 28, 5.5f}};
  }
  canopy(c, b, leaf, seed);
  // shadow the trunk right under the crown
  for (int x = 0; x < W; x++)
    for (int y = (int)(H * 0.62f); y < (int)(H * 0.72f); y++)
      if (c.get(x, y) == kBark[2] || c.get(x, y) == kBark[3]) c.set(x, y, kBark[1]);
  grassTuft(c, (int)cx - 4, H - 2, seed, 3);
  grassTuft(c, (int)cx + 5, H - 2, seed + 1, 2);
}

void pineTree(Canvas& c, const Ramp& P, uint32_t seed, int tiers, bool snow) {
  const int W = c.w, H = c.h;
  float cx = W * 0.5f;
  trunk(c, cx, H - 2, H - 9, 4.0f, kBark, seed);
  float top = 2, bottom = H - 7.0f;
  for (int t = 0; t < tiers; t++) {
    float f = (float)t / (tiers - 1);
    float ty0 = top + (bottom - top) * f * 0.78f;
    float ty1 = ty0 + (bottom - top) * (0.30f + 0.06f * f);
    float hw = 3.0f + f * (W * 0.5f - 4.0f);
    for (int y = (int)ty0; y <= (int)ty1; y++) {
      float u = (y - ty0) / std::max(1.0f, ty1 - ty0);
      float half = hw * (0.15f + 0.85f * u);
      // jagged lower edge
      for (int x = (int)(cx - half - 1); x <= (int)(cx + half + 1); x++) {
        float rel = (x + 0.5f - cx) / std::max(1.0f, half);
        if (std::fabs(rel) > 1.0f) continue;
        float jag = (float)(hash3(x, t, seed) % 3);
        if (y > ty1 - jag - std::fabs(rel) * 1.5f) continue;
        float l = lightAt(rel * 0.8f, (u - 0.5f) * 0.9f) + (hashf(x, y, seed) - 0.5f) * 0.3f;
        int k = lightIndex(l, x, y, 0.15f);
        if (((x + y * 2) % 5) == 0 && k > 0) k--;                 // needle strokes
        if (y > ty1 - jag - std::fabs(rel) * 1.5f - 1.5f) k = std::max(0, k - 1);
        // snow settles on the upper part of every tier, its lower edge ragged
        if (snow && u < (t == 0 ? 0.12f : 0.26f) + 0.25f * hashf(x / 2, t, seed) - std::fabs(rel) * 0.15f) {
          c.set(x, y, kSnow[std::clamp(k + 1, 1, 4)]);
          continue;
        }
        c.set(x, y, P[k]);
      }
    }
  }
  c.set((int)cx, 1, P[3]);
  if (snow) {
    for (int x = (int)cx - 5; x <= (int)cx + 5; x++) c.set(x, H - 2, kSnow[(x & 1) ? 3 : 2]);
  }
}

void birchTree(Canvas& c, uint32_t seed) {
  const int W = c.w, H = c.h;
  float cx = W * 0.5f;
  const Ramp Bw = ramp5(rgba(90, 90, 108), rgba(160, 160, 166), rgba(214, 212, 204), rgba(236, 234, 224), rgba(252, 250, 242));
  trunk(c, cx, H - 2, H / 3, 4.0f, Bw, seed);
  for (int y = H / 3; y < H - 2; y++)   // black bark marks
    if (hash3(0, y, seed) % 4 == 0) { int x = (int)cx - 2 + (int)(hash3(1, y, seed) % 3); c.set(x, y, kInk); c.set(x + 1, y, kBark[0]); }
  thickLine(c, cx, H * 0.5f, cx - 6, H * 0.36f, 1.4f, Bw[2]);
  thickLine(c, cx, H * 0.46f, cx + 5, H * 0.34f, 1.4f, Bw[1]);
  std::vector<Blob> b = {{cx, 9, 6.5f}, {cx - 6, 13, 6}, {cx + 6, 12, 6}, {cx - 8, 20, 5}, {cx + 8, 19, 5.5f},
                         {cx - 2, 18, 6.5f}, {cx + 3, 24, 5.5f}, {cx - 5, 25, 4.5f}};
  canopy(c, b, kBirchLeaf, seed, 0.95f);
  grassTuft(c, (int)cx - 3, H - 2, seed, 3);
}

void deadTree(Canvas& c, uint32_t seed) {
  const int W = c.w, H = c.h;
  const Ramp D = ramp5(rgba(40, 32, 40), rgba(70, 56, 58), rgba(104, 86, 78), rgba(136, 116, 98), rgba(166, 146, 122));
  float cx = W * 0.5f;
  trunk(c, cx, H - 2, H / 2, 5.0f, D, seed);
  struct Br { Vec2 a; float ang, len, w; int depth; };
  std::vector<Br> st = {{{cx, H * 0.55f}, -1.62f, 13.0f, 3.2f, 0}};
  while (!st.empty()) {
    Br b = st.back();
    st.pop_back();
    Vec2 e = b.a + Vec2{std::cos(b.ang), std::sin(b.ang)} * b.len;
    capsule(c, b.a, e, b.w * 0.5f, b.w * 0.36f, D, 0, 0);
    if (b.depth < 3) {
      float s = 0.45f + hashf(b.depth, (int)b.len, seed) * 0.3f;
      st.push_back({e, b.ang - s, b.len * 0.66f, b.w * 0.7f, b.depth + 1});
      st.push_back({e, b.ang + s * 0.9f, b.len * 0.6f, b.w * 0.66f, b.depth + 1});
      if (b.depth == 0) st.push_back({b.a + (e - b.a) * 0.45f, b.ang + 0.9f, b.len * 0.55f, b.w * 0.6f, 2});
    }
  }
  // knot hole
  c.set((int)cx, H - 12, kInk); c.set((int)cx, H - 11, D[0]);
  grassTuft(c, (int)cx - 4, H - 2, seed, 2);
}

void willowTree(Canvas& c, uint32_t seed) {
  const int W = c.w, H = c.h;
  float cx = W * 0.5f;
  trunk(c, cx, H - 2, H / 2, 7.0f, kBark, seed);
  std::vector<Blob> b = {{cx, 11, 10}, {cx - 10, 15, 8}, {cx + 10, 15, 8}, {cx - 4, 17, 9}, {cx + 5, 18, 9}};
  canopy(c, b, kWillow, seed, 0.8f);
  // hanging fronds
  for (int x = 2; x < W - 2; x++) {
    int top = -1;
    for (int y = 0; y < H; y++) if (solid(c, x, y) && chA(c.get(x, y)) && c.get(x, y) != kBark[2]) { top = y; break; }
    if (top < 0 || (x % 2 && hash3(x, 0, seed) % 3)) continue;
    int startY = 0;
    for (int y = H - 1; y >= 0; y--) { uint32_t p = c.get(x, y); bool leaf = false; for (int k = 0; k < 5; k++) if (p == kWillow[k]) leaf = true; if (leaf) { startY = y; break; } }
    int len = 8 + (int)(hash3(x, 1, seed) % 12);
    if (std::fabs(x - cx) < 4) len /= 3;
    for (int j = 0; j < len && startY + j < H - 3; j++) {
      int k = j < 3 ? 2 : (j < len - 2 ? 1 : 2);
      if (j % 4 == 1) k = 3;
      c.set(x, startY + j, kWillow[k]);
    }
  }
  grassTuft(c, (int)cx - 4, H - 2, seed, 3);
}

void palmTree(Canvas& c, uint32_t seed) {
  const int W = c.w, H = c.h;
  const Ramp Pt = ramp5(rgba(70, 48, 44), rgba(118, 84, 60), rgba(162, 122, 80), rgba(196, 160, 106), rgba(224, 196, 140));
  const Ramp Pl = ramp5(rgba(26, 64, 50), rgba(40, 104, 56), rgba(70, 148, 60), rgba(124, 186, 72), rgba(184, 220, 104));
  // curved trunk of stacked rings
  Vec2 base{W * 0.42f, (float)H - 2}, topP{W * 0.56f, 14.0f};
  for (int i = 0; i <= 16; i++) {
    float t = i / 16.0f;
    Vec2 p = base * (1 - t) + topP * t + Vec2{std::sin(t * PI) * -3.0f, 0};
    float r = lerpf(2.6f, 1.8f, t);
    ellipse(c, p.x, p.y, r, 1.6f, Pt[2]);
    for (int x = (int)(p.x - r); x <= (int)(p.x + r); x++) {
      float u = (x + 0.5f - p.x) / r;
      c.set(x, (int)p.y, Pt[lightIndex(lightAt(u * 0.9f, 0), x, 0, 0)]);
      if (i % 2 == 0) c.set(x, (int)p.y + 1, Pt[1]);
    }
  }
  // fronds
  Vec2 crown = topP + Vec2{0, -1};
  for (int f = 0; f < 7; f++) {
    float a = -PI * 0.5f + (f - 3) * 0.52f + (hashf(f, 0, seed) - 0.5f) * 0.2f;
    float L = 13.0f - std::fabs(f - 3.0f) * 0.4f;
    Vec2 prev = crown;
    int bias = (f < 3) ? 0 : (f == 3 ? 1 : -1);
    for (int s = 1; s <= 12; s++) {
      float t = s / 12.0f;
      Vec2 p = crown + Vec2{std::cos(a) * L * t, std::sin(a) * L * t + t * t * 11.0f};
      line(c, (int)prev.x, (int)prev.y, (int)p.x, (int)p.y, Pl[2 + bias]);
      // leaflets hanging below the rib
      int ll = (int)(3.5f * (1 - t * 0.6f));
      for (int k = 1; k <= ll; k++) {
        c.set((int)p.x + (s & 1), (int)p.y + k, Pl[(k == 1 ? 3 : 1) + bias]);
        if (s % 3 == 0) c.set((int)p.x - 1, (int)p.y + k, Pl[1 + bias]);
      }
      prev = p;
    }
  }
  // coconuts
  ball(c, crown.x - 1.5f, crown.y + 2.5f, 1.6f, 1.6f, kWoodDark);
  ball(c, crown.x + 1.5f, crown.y + 3.0f, 1.6f, 1.6f, kWoodDark);
}

// ---- plants -----------------------------------------------------------------------------------------
void bush(Canvas& c, const Ramp& leaf, uint32_t seed) {
  float cx = c.w * 0.5f, by = c.h - 2.0f;
  std::vector<Blob> b = {{cx - 4, by - 6, 5}, {cx + 4, by - 6, 5}, {cx, by - 9, 5.5f}, {cx - 1, by - 4, 5.5f}, {cx + 5, by - 3, 3.5f}, {cx - 6, by - 3, 3.5f}};
  canopy(c, b, leaf, seed, 0.85f);
}

void flowers(Canvas& c, int kind, uint32_t seed) {
  static const uint32_t petals[3][3] = {
    {rgba(232, 70, 60), rgba(250, 200, 70), rgba(250, 130, 60)},
    {rgba(110, 150, 240), rgba(250, 248, 240), rgba(170, 200, 255)},
    {rgba(190, 100, 210), rgba(250, 150, 190), rgba(240, 220, 120)}};
  // leaves first
  for (int i = 0; i < 7; i++) {
    int x = 2 + (int)(hash3(i, 0, seed) % 12), y = c.h - 2 - (int)(hash3(i, 1, seed) % 3);
    c.set(x, y, kLeaf[2]); c.set(x + 1, y, kLeaf[1]); c.set(x, y - 1, kLeaf[3]);
  }
  const int pos[6][2] = {{3, 6}, {8, 4}, {12, 7}, {6, 9}, {11, 2}, {2, 2}};
  for (int i = 0; i < 6; i++) {
    int x = pos[i][0] + (int)(hash3(i, 2, seed) % 2), y = pos[i][1] + 1;
    uint32_t pc = petals[kind][i % 3];
    Ramp pr = ramp(pc, 0.8f);
    vline(c, x, y + 1, c.h - 2, kLeaf[1 + (i & 1)]);
    c.set(x, y - 1, pr[3]); c.set(x - 1, y, pr[2]); c.set(x + 1, y, pr[1]); c.set(x, y + 1, pr[1]);
    c.set(x, y, kind == 1 && (i % 3) == 1 ? rgba(250, 210, 80) : pr[4]);
    if (kind == 1 && (i % 3) != 1) c.set(x, y, rgba(250, 230, 120));
  }
}

void tallGrass(Canvas& c, uint32_t seed) {
  for (int i = 0; i < 9; i++) {
    float x = 1.5f + i * 1.6f;
    int h = 7 + (int)(hash3(i, 0, seed) % 6);
    float lean = (hashf(i, 1, seed) - 0.5f) * 4.0f;
    for (int j = 0; j < h; j++) {
      float t = (float)j / h;
      int px2 = (int)(x + lean * t * t);
      int k = t < 0.3f ? 1 : (t < 0.75f ? 2 : 3);
      if (i % 3 == 0) k = std::max(0, k - 1);
      c.set(px2, c.h - 2 - j, kLeaf[k]);
      if (j < 3) c.set(px2 + 1, c.h - 2 - j, kLeaf[1]);
    }
  }
}

void reeds(Canvas& c, uint32_t seed) {
  const Ramp Rd = ramp5(rgba(40, 70, 52), rgba(68, 108, 58), rgba(104, 146, 64), rgba(150, 180, 82), rgba(196, 212, 120));
  for (int i = 0; i < 7; i++) {
    int x = 2 + i * 2, h = 10 + (int)(hash3(i, 0, seed) % 7);
    int lean = (int)(hash3(i, 1, seed) % 3) - 1;
    for (int j = 0; j < h; j++) c.set(x + (j > h * 2 / 3 ? lean : 0), c.h - 2 - j, Rd[j > h / 2 ? 3 : 2]);
    if (i % 2 == 0) {   // cattail
      int tx = x + lean, ty = c.h - 2 - h;
      for (int k = 0; k < 3; k++) { c.set(tx, ty + k, kWoodDark[3 - (k == 2)]); c.set(tx + 1, ty + k, kWoodDark[2]); }
      c.set(tx, ty - 1, Rd[3]);
    }
  }
  // water ripple at the base
  for (int x = 1; x < c.w - 1; x++) if (x % 3) c.set(x, c.h - 1, withA(kWater[3], 180));
}

void cactus(Canvas& c) {
  const Ramp Cg = ramp5(rgba(30, 70, 56), rgba(50, 108, 62), rgba(80, 148, 70), rgba(126, 182, 86), rgba(176, 214, 120));
  int cx = c.w / 2;
  auto column = [&](int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        float u = (x + 0.5f - (x0 + x1 + 1) * 0.5f) / ((x1 - x0 + 1) * 0.5f);
        int k = lightIndex(lightAt(u * 0.9f, y == y0 ? -0.6f : 0), x, y, 0);
        if ((x - x0) % 2 == 1 && k > 1) k--;   // ribs
        c.set(x, y, Cg[k]);
      }
    for (int x = x0; x <= x1; x++) c.set(x, y0 - 1, (x == x0 || x == x1) ? 0 : Cg[3]);
  };
  column(cx - 2, 3, cx + 2, c.h - 2);
  column(cx - 7, 9, cx - 5, 15); hline(c, cx - 6, cx - 3, 15, Cg[2]); hline(c, cx - 7, cx - 3, 16, Cg[1]);
  column(cx + 5, 6, cx + 7, 12); hline(c, cx + 3, cx + 6, 12, Cg[2]); hline(c, cx + 3, cx + 7, 13, Cg[1]);
  for (int y = 5; y < c.h - 3; y += 3) { c.set(cx - 3, y, kBone[4]); c.set(cx + 3, y + 1, kBone[3]); }
  c.set(cx, 1, rgba(250, 120, 160)); c.set(cx - 1, 2, rgba(230, 90, 140)); c.set(cx + 1, 2, rgba(230, 90, 140));
}

void mushrooms(Canvas& c) {
  const Ramp Cap = ramp5(rgba(96, 26, 40), rgba(156, 40, 44), rgba(210, 62, 52), rgba(236, 110, 80), rgba(252, 170, 130));
  auto shroom = [&](int x, int y, int r, const Ramp& cap) {
    int stemH = r + 1;
    for (int j = 0; j < stemH; j++) { c.set(x - 1, y - j, kBone[3]); c.set(x, y - j, kBone[2]); }
    int top = y - stemH - r + 1;
    for (int j = 0; j < r; j++)
      for (int i = -r; i <= r; i++) {
        if (i * i + (r - j) * (r - j) * 2 > r * r * 2 + r) continue;
        float u = (float)i / r;
        c.set(x + i, top + j, cap[lightIndex(lightAt(u * 0.8f, (float)(j - r) / r * 0.8f), x + i, top + j, 0.1f)]);
      }
    hline(c, x - r + 1, x + r - 1, top + r - 1, cap[0]);
    if (r >= 3) { c.set(x - 1, top + 1, kWhite); c.set(x + 2, top + 2, kWhite); c.set(x - r + 1, top + 2, kBone[3]); }
  };
  const Ramp Brown = ramp5(rgba(70, 44, 40), rgba(110, 72, 50), rgba(150, 104, 66), rgba(186, 140, 90), rgba(214, 176, 124));
  shroom(11, c.h - 2, 2, Brown);
  shroom(5, c.h - 2, 4, Cap);
  shroom(9, c.h - 2, 3, Cap);
}

void lilyPad(Canvas& c) {
  auto pad = [&](float x, float y, float r, float notch) {
    for (int j = (int)(y - r); j <= (int)(y + r); j++)
      for (int i = (int)(x - r); i <= (int)(x + r); i++) {
        float dx = (i + 0.5f - x) / r, dy = (j + 0.5f - y) / (r * 0.6f);
        if (dx * dx + dy * dy > 1) continue;
        float a = std::atan2(dy, dx);
        if (std::fabs(a - notch) < 0.35f) continue;
        int k = lightIndex(lightAt(dx * 0.6f, dy * 0.6f), i, j, 0.1f);
        c.set(i, j, kLeaf[std::max(1, k)]);
      }
  };
  pad(6, 6, 5.5f, 0.6f);
  pad(12, 4, 3.5f, 2.4f);
  pad(12, 8, 2.8f, -1.0f);
  // flower
  int fx = 6, fy = 4;
  c.set(fx, fy - 1, rgba(252, 220, 230)); c.set(fx - 1, fy, rgba(240, 150, 180)); c.set(fx + 1, fy, rgba(230, 130, 170));
  c.set(fx, fy, rgba(250, 230, 120)); c.set(fx, fy + 1, rgba(220, 120, 160));
}

void fern(Canvas& c, uint32_t seed) {
  // arching fronds with alternating leaflets; outer fronds darker, the central ones catch the light
  Vec2 base{c.w * 0.5f, (float)c.h - 2};
  const int order[7] = {0, 6, 1, 5, 2, 4, 3};
  for (int oi = 0; oi < 7; oi++) {
    int f = order[oi];
    float a = PI + 0.3f + f * (PI - 0.6f) / 6.0f;
    float L = 7.0f + (f == 3 ? 1.5f : (f % 2) * 1.0f) + hashf(f, 0, seed);
    int bias = (f == 2 || f == 3 || f == 4) ? 1 : (f == 0 || f == 6 ? -1 : 0);
    Vec2 prev = base;
    for (int s2 = 1; s2 <= 10; s2++) {
      float t = s2 / 10.0f;
      Vec2 p = base + Vec2{std::cos(a) * L * t, std::sin(a) * L * t + t * t * 5.0f};
      line(c, (int)prev.x, (int)prev.y, (int)p.x, (int)p.y, kLeaf[1 + bias]);
      if (s2 >= 2 && s2 <= 9 && (s2 & 1) == 0) {
        float pa = a + PI * 0.5f;
        float ll = 2.2f * (1.0f - t * 0.5f);
        c.set((int)(p.x + std::cos(pa) * ll), (int)(p.y + std::sin(pa) * ll), kLeaf[3 + bias]);
        c.set((int)(p.x + std::cos(pa) * ll * 0.5f), (int)(p.y + std::sin(pa) * ll * 0.5f), kLeaf[2 + bias]);
        c.set((int)(p.x - std::cos(pa) * ll), (int)(p.y - std::sin(pa) * ll), kLeaf[2 + bias]);
        c.set((int)(p.x - std::cos(pa) * ll * 0.5f), (int)(p.y - std::sin(pa) * ll * 0.5f), kLeaf[1 + bias]);
      }
      prev = p;
    }
  }
}

// ---- camp / town ------------------------------------------------------------------------------------
// box seen in 3/4 view: top face (lighter) + front face
void plankBox(Canvas& c, int x0, int y0, int w, int topH, int frontH, const Ramp& R) {
  for (int y = 0; y < topH; y++)
    for (int x = 0; x < w; x++) {
      int k = (x == 0) ? 4 : (x == w - 1 ? 2 : 3);
      if (y == 0) k = std::min(4, k + 1);
      c.set(x0 + x, y0 + y, R[k]);
    }
  for (int y = 0; y < frontH; y++)
    for (int x = 0; x < w; x++) {
      int k = (x == 0) ? 2 : (x >= w - 2 ? 1 : 2);
      if (y == frontH - 1) k = 0;
      if (y == 0) k = 1;
      c.set(x0 + x, y0 + topH + y, R[k]);
    }
}

void chest(Canvas& c, bool open) {
  const Ramp& Wd = kWood;
  int x0 = 1, w = c.w - 2;
  int base = c.h - 2;
  // body front
  int bodyTop = base - 7;
  for (int y = bodyTop; y <= base; y++)
    for (int x = x0; x < x0 + w; x++) {
      int k = x == x0 ? 3 : (x >= x0 + w - 2 ? 1 : 2);
      if ((y - bodyTop) % 3 == 2) k = std::max(0, k - 1);   // planks
      if (y == base) k = 0;
      c.set(x, y, Wd[k]);
    }
  // iron bands + lock
  for (int y = bodyTop; y <= base; y++) { c.set(x0 + 2, y, kIron[2]); c.set(x0 + w - 3, y, kIron[1]); }
  if (!open) {
    // rounded lid
    for (int y = bodyTop - 5; y < bodyTop; y++)
      for (int x = x0; x < x0 + w; x++) {
        int j = y - (bodyTop - 5);
        if (j == 0 && (x == x0 || x == x0 + w - 1)) continue;
        int k = j <= 1 ? 4 : (j == 4 ? 1 : 3);
        if (x == x0 + w - 1) k--;
        c.set(x, y, Wd[k]);
      }
    hline(c, x0, x0 + w - 1, bodyTop, Wd[0]);
    for (int y = bodyTop - 5; y < bodyTop; y++) { c.set(x0 + 2, y, kIron[3]); c.set(x0 + w - 3, y, kIron[2]); }
    c.set(x0 + 2, bodyTop - 5, kIron[4]);
    box(c, x0 + w / 2 - 1, bodyTop - 1, x0 + w / 2, bodyTop + 1, kGold[3]);
    c.set(x0 + w / 2, bodyTop + 1, kGold[1]); c.set(x0 + w / 2 - 1, bodyTop + 2, kGold[1]);
  } else {
    // open lid tilted back, gold heap inside
    for (int y = bodyTop - 10; y < bodyTop - 3; y++)
      for (int x = x0; x < x0 + w; x++) {
        int j = y - (bodyTop - 10);
        int k = j == 0 ? 3 : 1;
        if (x == x0 || x == x0 + w - 1) k = 0;
        c.set(x, y, Wd[k]);
      }
    for (int y = bodyTop - 10; y < bodyTop - 3; y++) { c.set(x0 + 2, y, kIron[1]); c.set(x0 + w - 3, y, kIron[0]); }
    // inside rim (dark) + gold
    for (int y = bodyTop - 3; y < bodyTop; y++)
      for (int x = x0; x < x0 + w; x++) c.set(x, y, (x == x0 || x == x0 + w - 1) ? Wd[2] : Wd[0]);
    for (int x = x0 + 2; x < x0 + w - 2; x++) {
      int hgt = 1 + (int)(hash3(x, 0, 5) % 2);
      for (int k = 0; k < hgt; k++) c.set(x, bodyTop - 2 - k, kGold[(x + k) % 3 == 0 ? 4 : 3]);
    }
    c.set(x0 + 4, bodyTop - 4, kWhite); c.set(x0 + w - 5, bodyTop - 3, rgba(120, 220, 255));
    hline(c, x0, x0 + w - 1, bodyTop, Wd[3]);
    box(c, x0 + w / 2 - 1, bodyTop + 1, x0 + w / 2, bodyTop + 2, kGold[2]);
  }
}

void barrel(Canvas& c, int x0, int y0, int w, int h) {
  // staves with a bulge, iron hoops, elliptical top
  float cx = x0 + w * 0.5f;
  for (int y = y0 + 2; y < y0 + h; y++) {
    float t = (float)(y - y0 - 2) / (h - 3);
    float bulge = 1.0f - (t - 0.5f) * (t - 0.5f) * 1.2f;
    float hw = w * 0.5f * (0.86f + 0.14f * bulge);
    for (int x = (int)std::floor(cx - hw); x < (int)std::ceil(cx + hw); x++) {
      float u = (x + 0.5f - cx) / hw;
      int k = lightIndex(lightAt(u * 0.9f, 0), x, y, 0);
      if ((x - x0) % 3 == 0) k = std::max(0, k - 1);   // stave seams
      c.set(x, y, kWood[k]);
    }
  }
  for (int hy : {y0 + 4, y0 + h - 4})
    for (int x = x0; x < x0 + w; x++) if (solid(c, x, hy)) c.set(x, hy, kIron[x < cx - 2 ? 3 : (x < cx + 2 ? 2 : 1)]);
  ellipse(c, cx, y0 + 2.5f, w * 0.43f, 2.0f, kWood[3]);
  ellipse(c, cx, y0 + 2.5f, w * 0.30f, 1.2f, kWood[2]);
  hline(c, (int)(cx - w * 0.43f), (int)(cx + w * 0.43f) - 1, y0 + 4, kWood[1]);
  c.set((int)cx - 2, y0 + 2, kWood[4]);
}

void crate(Canvas& c) {
  int x0 = 1, w = c.w - 2, topH = 5, frontH = c.h - 2 - topH;
  plankBox(c, x0, 1, w, topH, frontH, kWood);
  for (int y = 1; y < 1 + topH; y++) c.set(x0 + w / 2, y, kWood[2]);
  int fy = 1 + topH;
  // frame + cross brace
  for (int y = fy; y < fy + frontH; y++) { c.set(x0, y, kWood[3]); c.set(x0 + 1, y, kWood[3]); c.set(x0 + w - 1, y, kWood[1]); c.set(x0 + w - 2, y, kWood[1]); }
  for (int x = x0; x < x0 + w; x++) { c.set(x, fy, kWood[3]); c.set(x, fy + frontH - 2, kWood[2]); }
  line(c, x0 + 2, fy + 1, x0 + w - 3, fy + frontH - 3, kWood[3]);
  line(c, x0 + 2, fy + 2, x0 + w - 4, fy + frontH - 3, kWood[1]);
  for (int x = x0 + 2; x < x0 + w - 2; x++) if ((x & 3) == 0) c.set(x, fy + 1, kIron[2]);
}

void flame(Canvas& c, double cxd, double based, double hd, double wd, int frame, uint32_t seed) {
  const float cx = (float)cxd, base = (float)based, h = (float)hd, w = (float)wd;
  // teardrop flame with flicker; layered from outer red to white-hot core
  for (int layer = 0; layer < 4; layer++) {
    float s = 1.0f - layer * 0.22f;
    float fh = h * s, fw = w * s;
    for (int y = (int)(base - fh - 1); y <= (int)base; y++) {
      float t = (base - y) / fh;   // 0 bottom .. 1 tip
      if (t < 0 || t > 1) continue;
      float sway = std::sin(t * 3.2f + frame * 1.7f + seed) * t * w * 0.35f;
      float half = fw * 0.5f * std::sqrt(std::max(0.0f, 1 - t)) * (t < 0.2f ? 0.6f + t * 2 : 1.0f);
      for (int x = (int)std::floor(cx + sway - half); x <= (int)std::ceil(cx + sway + half) - 1; x++)
        c.set(x, y, kFire[layer + 1]);
    }
  }
  // sparks
  for (int i = 0; i < 2; i++) {
    uint32_t hsh = hash3(frame, i, seed);
    int sx = (int)cx - 2 + (int)(hsh % 5), sy = (int)(base - h - 1 - (hsh >> 8) % 4);
    c.set(sx, sy, kFire[3 + (int)(hsh >> 4) % 2]);
  }
}

void torch(Canvas& c, int frame) {
  int cx = c.w / 2;
  // pole + bracket
  for (int y = 9; y < c.h - 1; y++) { c.set(cx - 1, y, kWood[3]); c.set(cx, y, kWood[1]); }
  box(c, cx - 2, 8, cx + 1, 10, kIron[2]);
  hline(c, cx - 2, cx + 1, 8, kIron[3]);
  c.set(cx - 1, c.h - 1, kWood[0]); c.set(cx, c.h - 1, kWood[0]);
  static const float hs[4] = {7.0f, 8.0f, 6.5f, 7.5f};
  flame(c, cx, 8.0f, hs[frame], 5.0f, frame, 3);
}

void campfire(Canvas& c, int frame) {
  float cx = c.w * 0.5f, by = c.h - 3.0f;
  // ring of stones
  for (int i = 0; i < 9; i++) {
    float a = PI * (0.05f + i / 8.0f * 0.9f);
    float sx = cx + std::cos(a) * 7.5f, sy = by - 1 + std::sin(a) * 2.0f;
    ball(c, sx, sy, 2.0f, 1.6f, kStone);
  }
  // logs crossed
  capsule(c, V(cx - 6, by - 1), V(cx + 5, by - 3), 1.4f, 1.2f, kWood);
  capsule(c, V(cx + 6, by - 1), V(cx - 5, by - 3), 1.4f, 1.2f, kWoodDark);
  c.set((int)cx + 5, (int)by - 3, kWood[4]); c.set((int)cx - 6, (int)by - 1, kWood[4]);
  // embers
  for (int x = (int)cx - 3; x <= (int)cx + 3; x++) c.set(x, (int)by - 2, kFire[(x + frame) % 3 == 0 ? 3 : 1]);
  static const float hs[4] = {11.0f, 13.0f, 10.5f, 12.5f};
  flame(c, cx, by - 2.0f, hs[frame], 9.0f, frame, 11);
  // stones in front of the fire
  for (int i = 0; i < 3; i++) ball(c, cx - 4 + i * 4, by + 1, 2.1f, 1.5f, kStone);
}

void signpost(Canvas& c) {
  int cx = c.w / 2;
  for (int y = 4; y < c.h - 1; y++) { c.set(cx - 1, y, kWood[3]); c.set(cx, y, kWood[1]); }
  // two arrow boards
  auto board = [&](int y, int dir, int len) {
    int x0 = dir > 0 ? cx - 2 : cx - len + 1;
    for (int j = 0; j < 4; j++)
      for (int i = 0; i < len; i++) {
        int x = x0 + i;
        bool tipZone = dir > 0 ? (i >= len - 2) : (i <= 1);
        if (tipZone && (j == 0 || j == 3) && ((dir > 0 && i == len - 1) || (dir < 0 && i == 0))) continue;
        int k = j == 0 ? 4 : (j == 3 ? 1 : 3);
        c.set(x, y + j, kWood[k]);
      }
    // writing marks
    for (int i = 2; i < len - 3; i += 2) c.set(x0 + i + (dir < 0 ? 1 : 0), y + 1 + (i % 4 == 0), kWood[0]);
  };
  board(3, 1, 11);
  board(9, -1, 10);
  c.set(cx - 1, c.h - 1, kWood[0]);
  grassTuft(c, cx - 2, c.h - 1, 9, 3);
}

void well(Canvas& c) {
  float cx = c.w * 0.5f;
  int base = c.h - 2;
  // stone ring: back rim, water, front wall
  ellipse(c, cx, base - 8, 11.5f, 4.5f, kStone[3]);
  ellipse(c, cx, base - 8, 8.5f, 3.0f, kWater[0]);
  ellipse(c, cx + 1, base - 7.5f, 6.0f, 1.6f, kWater[1]);
  c.set((int)cx - 2, base - 8, kWater[3]); c.set((int)cx - 1, base - 8, kWater[3]);
  for (int y = base - 8; y <= base; y++)
    for (int x = (int)(cx - 11.5f); x <= (int)(cx + 11.5f); x++) {
      float dx = (x + 0.5f - cx) / 11.5f;
      float yy = base - 8 + std::sqrt(std::max(0.0f, 1 - dx * dx)) * 4.5f;
      if (y < yy) continue;
      int row = (int)(y - yy) / 3;
      int brick = (x + row * 3) / 5;
      int k = lightIndex(lightAt(dx * 0.85f, 0.1f), x, y, 0);
      if (((x + row * 3) % 5) == 0 || (int)(y - yy) % 3 == 0) k = std::max(0, k - 1);
      (void)brick;
      c.set(x, y, kStone[k]);
    }
  // posts + roof
  for (int y = 6; y <= base - 8; y++) { c.set((int)cx - 10, y, kWood[3]); c.set((int)cx - 9, y, kWood[1]); c.set((int)cx + 9, y, kWood[2]); c.set((int)cx + 10, y, kWood[0]); }
  hline(c, (int)cx - 9, (int)cx + 9, 8, kWood[2]);   // crossbar
  vline(c, (int)cx, 9, base - 13, kCloth[2]);           // rope
  box(c, (int)cx - 1, base - 13, (int)cx + 1, base - 11, kWood[2]);   // bucket
  hline(c, (int)cx - 1, (int)cx + 1, base - 13, kIron[3]);
  for (int y = 0; y < 7; y++)
    for (int x = -13 + (6 - y); x <= 13 - (6 - y); x++) {
      int k = y < 2 ? 4 : (y < 5 ? 3 : 1);
      if ((x + 13) % 3 == 0 && y > 1) k--;
      c.set((int)cx + x, y + 1, kBrick[k]);
    }
  hline(c, (int)cx - 13, (int)cx + 13, 7, kBrick[0]);
}

void fence(Canvas& c, bool vertical) {
  if (!vertical) {
    int base = c.h - 2;
    for (int px2 : {1, c.w - 3}) {
      for (int y = base - 11; y <= base; y++) { c.set(px2, y, kWood[3]); c.set(px2 + 1, y, kWood[1]); }
      c.set(px2, base - 11, kWood[4]); c.set(px2 + 1, base - 11, kWood[2]);
    }
    for (int ry : {base - 9, base - 4})
      for (int x = 0; x < c.w; x++) {
        if (x == 1 || x == 2 || x == c.w - 3 || x == c.w - 2) continue;
        c.set(x, ry, kWood[3]); c.set(x, ry + 1, kWood[1]);
      }
  } else {
    // a run going north: two rails seen end-on stack into a pair of planks rising from the post
    int cx = c.w / 2, base = c.h - 2;
    for (int y = 0; y < base - 6; y++) {
      c.set(cx - 2, y, kWood[3]); c.set(cx - 1, y, kWood[2]);
      c.set(cx + 1, y, kWood[2]); c.set(cx + 2, y, kWood[1]);
      if (y % 6 == 3) { c.set(cx - 2, y, kWood[4]); c.set(cx + 1, y, kWood[3]); }   // plank joints catch light
    }
    for (int y = base - 12; y <= base; y++) { c.set(cx - 1, y, kWood[3]); c.set(cx, y, kWood[2]); c.set(cx + 1, y, kWood[1]); }
    hline(c, cx - 1, cx + 1, base - 12, kWood[4]);
    grassTuft(c, cx - 2, base, 31, 3);
  }
}

void lamppost(Canvas& c) {
  int cx = c.w / 2;
  for (int y = 9; y < c.h - 2; y++) { c.set(cx - 1, y, kIron[2]); c.set(cx, y, kIron[1]); }
  box(c, cx - 2, c.h - 3, cx + 1, c.h - 2, kIron[1]);
  hline(c, cx - 2, cx + 1, c.h - 3, kIron[2]);
  // lantern
  hline(c, cx - 3, cx + 2, 2, kIron[2]); hline(c, cx - 2, cx + 1, 1, kIron[3]);
  for (int y = 3; y <= 7; y++) {
    c.set(cx - 3, y, kIron[1]); c.set(cx + 2, y, kIron[0]);
    for (int x = cx - 2; x <= cx + 1; x++) c.set(x, y, kGlow[y < 5 ? 4 : 3]);
  }
  c.set(cx - 1, 5, kWhite);
  hline(c, cx - 3, cx + 2, 8, kIron[1]);
}

void tent(Canvas& c) {
  const Ramp Cv = kCloth;
  float cx = c.w * 0.5f;
  int base = c.h - 2, top = 3;
  // A-frame: left face lit, right face shaded, dark triangular opening at the front
  for (int y = top; y <= base; y++) {
    float t = (float)(y - top) / (base - top);
    int half = (int)(1 + t * (c.w * 0.5f - 2));
    for (int x = (int)cx - half; x <= (int)cx + half; x++) {
      int k = x < cx ? 3 : 1;
      if (x == (int)cx - half) k = 2;
      if ((x + y) % 6 == 0) k = std::max(0, k - 1);    // fabric seams
      c.set(x, y, Cv[k]);
    }
  }
  for (int y = top + 8; y <= base; y++) {
    int half = (int)((y - top - 8) * 0.45f);
    for (int x = (int)cx - half; x <= (int)cx + half; x++) c.set(x, y, y > base - 2 ? kInk : rgba(46, 34, 44));
    c.set((int)cx - half - 1, y, Cv[4]);
  }
  vline(c, (int)cx, top - 2, top + 1, kWood[2]);
  // guy ropes + pegs
  line(c, (int)cx - 3, top + 4, 0, base, Cv[1]);
  line(c, (int)cx + 3, top + 4, c.w - 1, base, Cv[0]);
  hline(c, (int)cx - 14, (int)cx + 14, base, Cv[0]);
}

void marketStall(Canvas& c) {
  int W = c.w, base = c.h - 2;
  // posts
  for (int y = 10; y <= base; y++) { c.set(2, y, kWood[3]); c.set(3, y, kWood[1]); c.set(W - 4, y, kWood[2]); c.set(W - 3, y, kWood[0]); }
  // counter with goods
  plankBox(c, 1, base - 11, W - 2, 4, 8, kWood);
  const uint32_t goods[4] = {rgba(220, 60, 50), rgba(250, 180, 60), rgba(120, 180, 70), rgba(160, 90, 160)};
  for (int i = 0; i < 6; i++) {
    float gx = 6 + i * 5.3f, gy = base - 11.5f;
    Ramp g = ramp(goods[i % 4], 0.8f);
    ball(c, gx, gy, 2.0f, 1.6f, g);
    ball(c, gx + 1.5f, gy - 1, 1.4f, 1.2f, g);
  }
  // striped awning
  for (int y = 1; y <= 10; y++)
    for (int x = 0; x < W; x++) {
      bool red = ((x / 4) & 1) == 0;
      Ramp r = red ? kRed : kCloth;
      int k = y < 3 ? 4 : (y < 8 ? 3 : 2);
      if (y == 10) k = 1;
      c.set(x, y, r[k]);
    }
  // scalloped edge
  for (int x = 0; x < W; x++) if ((x % 4) == 1 || (x % 4) == 2) c.set(x, 11, ((x / 4) & 1) == 0 ? kRed[1] : kCloth[1]);
}

void haystack(Canvas& c) {
  float cx = c.w * 0.5f;
  int base = c.h - 2;
  for (int y = 1; y <= base; y++)
    for (int x = 0; x < c.w; x++) {
      float dx = (x + 0.5f - cx) / (c.w * 0.48f), dy = (y + 0.5f - base) / (base - 1.0f);
      if (dx * dx + dy * dy > 1) continue;
      float l = lightAt(dx * 0.85f, dy * 0.85f) + (hashf(x, y / 2, 3) - 0.5f) * 0.3f;
      int k = lightIndex(l, x, y, 0.12f);
      if (hash3(x, y, 4) % 5 == 0) k = std::max(0, k - 1);   // straw strands
      c.set(x, y, kThatch[k]);
    }
  for (int i = 0; i < 9; i++) {   // stray straws
    int x = 2 + (int)(hash3(i, 0, 7) % (c.w - 4)), y = 3 + (int)(hash3(i, 1, 7) % (base - 4));
    if (solid(c, x, y)) { c.set(x, y, kThatch[4]); c.set(x + 1, y - 1, kThatch[3]); }
  }
}

void anvil(Canvas& c) {
  int cx = c.w / 2, base = c.h - 2;
  // stump
  for (int y = base - 5; y <= base; y++)
    for (int x = cx - 5; x <= cx + 4; x++) c.set(x, y, kWood[x < cx - 2 ? 3 : (x > cx + 2 ? 1 : 2)]);
  hline(c, cx - 5, cx + 4, base - 5, kWood[4]);
  // anvil body: horn to the right
  box(c, cx - 3, base - 8, cx + 2, base - 6, kIron[1]);
  box(c, cx - 6, base - 11, cx + 4, base - 9, kIron[2]);
  hline(c, cx - 6, cx + 4, base - 11, kIron[4]);
  hline(c, cx - 5, cx + 3, base - 10, kIron[3]);
  c.set(cx + 5, base - 10, kIron[2]); c.set(cx + 6, base - 10, kIron[2]); c.set(cx + 7, base - 9, kIron[1]); c.set(cx + 5, base - 11, kIron[3]);
  box(c, cx - 4, base - 6, cx + 3, base - 5, kIron[1]);
}

void cart(Canvas& c) {
  int base = c.h - 2;
  // bed with sides and a load of sacks
  plankBox(c, 3, base - 15, c.w - 8, 4, 7, kWood);
  for (int x = 3; x < c.w - 5; x += 4) vline(c, x, base - 11, base - 5, kWood[1]);
  for (int i = 0; i < 3; i++) ball(c, 9 + i * 7.0f, base - 16.0f, 3.6f, 3.0f, kCloth);
  c.set(9, base - 19, kCloth[4]);
  // shafts to the right
  line(c, c.w - 5, base - 8, c.w - 1, base - 6, kWood[2]);
  // wheels
  for (int wx : {9, c.w - 13}) {
    for (int y = -5; y <= 5; y++)
      for (int x = -5; x <= 5; x++) {
        int d2 = x * x + y * y;
        if (d2 > 26) continue;
        if (d2 >= 15) c.set(wx + x, base - 4 + y, kWood[d2 > 22 ? 1 : 3]);
        else if (x == 0 || y == 0 || x == y || x == -y) c.set(wx + x, base - 4 + y, kWood[2]);
      }
    c.set(wx, base - 4, kIron[3]);
  }
}

void fountain(Canvas& c, int frame) {
  float cx = c.w * 0.5f;
  int base = c.h - 2;
  // basin: back rim, water, front wall
  ellipse(c, cx, base - 9, 19.0f, 6.5f, kStone[3]);
  ellipse(c, cx, base - 9, 16.0f, 5.0f, kWater[1]);
  for (int i = 0; i < 6; i++) {   // ripples move outward per frame
    float r = 3.0f + ((i * 3 + frame * 2) % 13);
    for (int a = 0; a < 40; a++) {
      float ang = a / 40.0f * TAU;
      int x = (int)(cx + std::cos(ang) * r * 1.3f), y = (int)(base - 9 + std::sin(ang) * r * 0.38f);
      if (c.get(x, y) == kWater[1] && (a + i) % 3 == 0) c.set(x, y, kWater[2]);
    }
  }
  for (int y = base - 9; y <= base; y++)
    for (int x = (int)(cx - 19); x <= (int)(cx + 19); x++) {
      float dx = (x + 0.5f - cx) / 19.0f;
      float yy = base - 9 + std::sqrt(std::max(0.0f, 1 - dx * dx)) * 6.5f - 1;
      if (y < yy) continue;
      int k = lightIndex(lightAt(dx * 0.85f, 0.1f), x, y, 0);
      if (y - (int)yy == 0) k = 4;
      if ((x % 6) == 0 && y - yy > 1) k = std::max(0, k - 1);
      c.set(x, y, kStone[k]);
    }
  // central pillar + upper bowl
  cylinder(c, (int)cx - 2, base - 22, (int)cx + 1, base - 8, kStone);
  ellipse(c, cx, base - 21, 6.0f, 2.0f, kStone[3]);
  ellipse(c, cx, base - 21, 4.5f, 1.2f, kWater[2]);
  hline(c, (int)cx - 5, (int)cx + 4, base - 19, kStone[1]);
  // spout + falling arcs
  vline(c, (int)cx, base - 27 + (frame & 1), base - 22, kWater[4]);
  for (int side = -1; side <= 1; side += 2)
    for (int i = 0; i < 9; i++) {
      float t = i / 8.0f;
      int x = (int)(cx + side * (2 + t * 7)), y = (int)(base - 26 + t * t * 14 - (1 - t) * 2);
      if ((i + frame) % 4 == 0) continue;
      c.set(x, y, kWater[(i + frame) % 2 ? 4 : 3]);
    }
  for (int side = -1; side <= 1; side += 2) { c.set((int)cx + side * 9, base - 11 + (frame % 2), kWater[4]); c.set((int)cx + side * 10, base - 12, kWhite); }
}

void statue(Canvas& c) {
  int cx = c.w / 2, base = c.h - 2;
  plankBox(c, cx - 8, base - 9, 16, 3, 7, kStone);
  // stone knight leaning on a sword
  const Ramp& S = kStoneWarm;
  HumanLook l;
  l.outfit = Outfit::Plate; l.helmet = true; l.cape = true; l.weapon = 1;
  Canvas fig = humanFigureStill(l, 0);
  // recolour the figure to stone by luminance
  for (auto& p : fig.px)
    if (chA(p)) { float lu = luma(p); p = S[std::clamp((int)(lu * 5.2f), 0, 4)]; }
  blit(c, fig, cx - 8, base - 9 - HUMAN_H + 1);
}

void banner(Canvas& c, int frame) {
  int base = c.h - 1;
  for (int y = 2; y <= base; y++) { c.set(2, y, kWood[3]); c.set(3, y, kWood[1]); }
  c.set(2, 1, kGold[4]); c.set(3, 1, kGold[2]); c.set(2, 0, kGold[3]);
  hline(c, 3, c.w - 2, 3, kWood[2]);
  // cloth hanging from the crossbar, rippling
  for (int y = 4; y < 22; y++) {
    float t = (y - 4) / 18.0f;
    int off = (int)std::lround(std::sin(t * 4.0f + frame * 1.6f) * 1.2f * t);
    for (int x = 4; x < c.w - 1; x++) {
      int k = 2;
      float wave = std::sin((x - 4) * 0.9f + frame * 1.6f + t * 2);
      if (wave > 0.5f) k = 3;
      if (wave < -0.5f) k = 1;
      int yy = y;
      if (y > 18) {   // swallow-tail end
        int mid = (4 + c.w - 2) / 2;
        if (std::abs(x - mid) < (y - 18)) continue;
      }
      c.set(x + off, yy, kRed[k]);
    }
  }
  // emblem
  int ex = (4 + c.w - 2) / 2;
  c.set(ex, 9, kGold[4]); c.set(ex - 1, 10, kGold[3]); c.set(ex + 1, 10, kGold[2]); c.set(ex, 10, kGold[3]); c.set(ex, 11, kGold[2]); c.set(ex, 12, kGold[1]);
}

void woodpile(Canvas& c) {
  int base = c.h - 2;
  const int rows[3] = {4, 3, 2};
  for (int r = 0; r < 3; r++)
    for (int i = 0; i < rows[r]; i++) {
      float x = 4.0f + i * 5.2f + r * 2.6f, y = base - 2.5f - r * 4.2f;
      // log end: bark ring + rings
      ellipse(c, x, y, 2.6f, 2.3f, kBark[1]);
      ellipse(c, x - 0.2f, y - 0.2f, 1.8f, 1.6f, kWood[3]);
      c.set((int)x, (int)y, kWood[2]);
      c.set((int)x - 1, (int)y - 1, kWood[4]);
    }
}

void gravestone(Canvas& c) {
  int cx = c.w / 2, base = c.h - 2;
  for (int y = 2; y <= base; y++)
    for (int x = cx - 5; x <= cx + 4; x++) {
      int j = y - 2;
      if (j < 3) { float dx = (x + 0.5f - cx) / 5.0f; if (dx * dx + (3 - j) * (3 - j) / 9.0f > 1.0f) continue; }
      int k = x == cx - 5 ? 3 : (x >= cx + 3 ? 1 : 2);
      if (j == 0) k = 3;
      c.set(x, y, kStone[k]);
    }
  // engraved cross
  vline(c, cx, 5, 10, kStone[0]); hline(c, cx - 2, cx + 1, 7, kStone[0]);
  vline(c, cx + 1, 6, 10, kStone[3]);
  c.set(cx - 4, 4, kMoss[2]); c.set(cx - 4, 5, kMoss[1]); c.set(cx + 3, base - 1, kMoss[2]);
  grassTuft(c, cx - 4, base, 21, 4);
}

void shrine(Canvas& c) {
  int cx = c.w / 2, base = c.h - 2;
  plankBox(c, cx - 10, base - 8, 20, 3, 6, kStone);
  // pillars + roof
  for (int px2 : {cx - 9, cx + 7}) cylinder(c, px2, 10, px2 + 2, base - 8, kStone);
  for (int y = 3; y <= 9; y++) {
    int half = 6 + (y - 3) * 1;
    for (int x = cx - half; x <= cx + half - 1; x++) c.set(x, y, kStone[y < 5 ? 4 : (y < 8 ? 3 : 1)]);
  }
  hline(c, cx - 12, cx + 11, 10, kStone[0]);
  // glowing idol / gem
  ball(c, cx - 0.5f, base - 12.0f, 2.5f, 3.0f, kCrystal);
  c.set(cx - 1, base - 14, kWhite);
  for (int i = 0; i < 3; i++) c.set(cx - 5 + i * 4, base - 9, kGlow[4]);   // candles
  for (int i = 0; i < 3; i++) c.set(cx - 5 + i * 4, base - 8, kCloth[3]);
}

// ---- dungeon ----------------------------------------------------------------------------------------
void caveEntrance(Canvas& c) {
  const int W = c.w, H = c.h;
  rock(c, W * 0.5f, H * 0.52f, W * 0.5f, H * 0.5f, kStone, 31, 11);
  rock(c, W * 0.16f, H * 0.7f, 8.0f, 9.0f, kStone, 32, 5);
  rock(c, W * 0.85f, H * 0.68f, 8.0f, 9.5f, kStone, 33, 5);
  // the dark opening: arched, fading to black inside
  float ox = W * 0.5f, ow = 11.0f, top = H * 0.32f;
  for (int y = (int)top; y < H; y++)
    for (int x = (int)(ox - ow); x <= (int)(ox + ow); x++) {
      float dx = (x + 0.5f - ox) / ow;
      float arch = top + (1 - std::sqrt(std::max(0.0f, 1 - dx * dx))) * 8.0f;
      if (y < arch) continue;
      float depth = (y - arch) / (H - arch);
      uint32_t col = depth < 0.15f ? rgba(40, 32, 50) : rgba(14, 10, 20);
      if (std::fabs(dx) > 0.82f) col = rgba(52, 44, 62);
      c.set(x, y, col);
    }
  // stalactite teeth on the arch
  for (int i = -3; i <= 3; i++) {
    int x = (int)ox + i * 3;
    float dx = (x + 0.5f - ox) / ow;
    int ay = (int)(top + (1 - std::sqrt(std::max(0.0f, 1 - dx * dx))) * 8.0f);
    for (int k = 0; k < 2 + (i & 1); k++) c.set(x, ay + k, kStone[k == 0 ? 2 : 1]);
  }
  // moss tufts on top
  for (int x = 4; x < W - 4; x += 3)
    for (int y = 0; y < H; y++) if (solid(c, x, y)) { if (hash3(x, 0, 3) % 2) { c.set(x, y, kMoss[3]); c.set(x + 1, y, kMoss[2]); c.set(x, y + 1, kMoss[1]); } break; }
}

void ladder(Canvas& c) {
  for (int y = 0; y < c.h - 1; y++) { c.set(3, y, kWood[3]); c.set(4, y, kWood[1]); c.set(11, y, kWood[2]); c.set(12, y, kWood[0]); }
  for (int y = 2; y < c.h - 1; y += 4) { hline(c, 5, 10, y, kWood[3]); hline(c, 5, 10, y + 1, kWood[1]); }
  // light from above
  for (int x = 5; x <= 10; x++) c.set(x, 0, withA(kGlow[4], 160));
}

void stalagmite(Canvas& c) {
  int cx = c.w / 2, base = c.h - 2;
  for (int y = 1; y <= base; y++) {
    float t = (float)(y - 1) / (base - 1);
    float half = 0.6f + t * t * (c.w * 0.45f);
    for (int x = (int)(cx - half); x <= (int)(cx + half); x++) {
      float u = (x + 0.5f - cx) / std::max(0.6f, half);
      int k = lightIndex(lightAt(u * 0.9f, -0.2f) + (hashf(x, y / 3, 5) - 0.5f) * 0.25f, x, y, 0.1f);
      if (y % 5 == 0 && k > 0) k--;
      c.set(x, y, kStoneWarm[k]);
    }
  }
  // small sibling
  for (int y = base - 7; y <= base; y++) {
    float half = (y - (base - 7)) * 0.4f;
    for (int x = (int)(cx + 4 - half); x <= (int)(cx + 4 + half); x++) c.set(x, y, kStoneWarm[x < cx + 4 ? 3 : 1]);
  }
}

void crystalCluster(Canvas& c) {
  int base = c.h - 2;
  struct Sh { float x, h, w, lean; };
  Sh sh[5] = {{8, 19, 3.2f, 0.0f}, {4, 12, 2.6f, -0.35f}, {12, 13, 2.6f, 0.35f}, {6, 8, 2.0f, -0.6f}, {11, 7, 2.0f, 0.6f}};
  for (int i = 4; i >= 0; i--) {
    const Sh& s = sh[i];
    Vec2 b{s.x, (float)base}, t{s.x + s.lean * s.h, base - s.h};
    Vec2 dir = norm(t - b), perp{-dir.y, dir.x};
    std::vector<Vec2> left = {b + perp * -s.w, t + perp * -s.w * 0.7f, t + dir * 2.5f, t, b};
    std::vector<Vec2> right = {b, t, t + dir * 2.5f, t + perp * s.w * 0.7f, b + perp * s.w};
    poly(c, left, kCrystal[3]);
    poly(c, right, kCrystal[1]);
    line(c, (int)b.x, (int)b.y, (int)t.x, (int)t.y, kCrystal[4]);
  }
  c.set(7, 3, kWhite); c.set(3, base - 9, kWhite);
}

void bones(Canvas& c) {
  int base = c.h - 2;
  capsule(c, V(2, base - 1), V(10, base - 3), 0.8f, 0.8f, kBone);
  ball(c, 2, base - 1.5f, 1.3f, 1.3f, kBone); ball(c, 10, base - 3.5f, 1.3f, 1.3f, kBone);
  capsule(c, V(8, base), V(15, base - 1), 0.7f, 0.7f, kBone, -1);
  ball(c, 13, base - 5.0f, 3.0f, 2.6f, kBone);
  c.set(12, base - 5, kInk); c.set(14, base - 5, kInk); c.set(13, base - 3, kBone[1]);
  for (int i = 0; i < 3; i++) c.set(4 + i * 2, base, kBone[3]);
}

void skullPile(Canvas& c) {
  int base = c.h - 2;
  const float sk[6][2] = {{4, 0}, {10, 0}, {16, 0}, {7, -4}, {13, -4}, {10, -8}};
  for (auto& s : sk) {
    float x = s[0], y = base - 3 + s[1];
    ball(c, x, y, 3.0f, 2.8f, kBone);
    c.set((int)x - 1, (int)y, kInk); c.set((int)x + 1, (int)y, kInk);
    c.set((int)x, (int)y + 2, kBone[1]);
    c.set((int)x - 1, (int)y - 2, kBone[4]);
  }
}

void cobweb(Canvas& c) {
  uint32_t silk = withA(rgba(230, 232, 240), 170), silk2 = withA(rgba(210, 214, 228), 120);
  // corner web anchored top-left
  for (int i = 0; i < 5; i++) {
    float a = i / 4.0f * PI * 0.5f;
    line(c, 0, 0, (int)(std::cos(a) * 15), (int)(std::sin(a) * 15), silk);
  }
  for (int r = 4; r <= 14; r += 4)
    for (int i = 0; i < 4; i++) {
      float a0 = i / 4.0f * PI * 0.5f, a1 = (i + 1) / 4.0f * PI * 0.5f;
      float sag = 0.85f;
      int x0 = (int)(std::cos(a0) * r), y0 = (int)(std::sin(a0) * r), x1 = (int)(std::cos(a1) * r), y1 = (int)(std::sin(a1) * r);
      int mx = (int)((x0 + x1) * 0.5f * sag), my = (int)((y0 + y1) * 0.5f * sag);
      line(c, x0, y0, mx, my, silk2);
      line(c, mx, my, x1, y1, silk2);
    }
}

void brazier(Canvas& c, int frame) {
  int cx = c.w / 2, base = c.h - 1;
  // tripod legs
  line(c, cx - 1, base - 7, cx - 5, base, kIron[2]);
  line(c, cx, base - 7, cx + 4, base, kIron[1]);
  vline(c, cx - 1, base - 7, base - 1, kIron[1]);
  // bowl
  for (int y = 0; y < 4; y++) {
    int half = 6 - y;
    for (int x = cx - half; x < cx + half; x++) c.set(x, base - 11 + y, kIron[x < cx - 2 ? 3 : (x < cx + 2 ? 2 : 1)]);
  }
  hline(c, cx - 6, cx + 5, base - 11, kIron[4]);
  for (int x = cx - 5; x < cx + 5; x++) c.set(x, base - 12, kFire[1 + ((x + frame) & 1)]);
  static const float hs[4] = {8.0f, 9.5f, 7.5f, 9.0f};
  flame(c, cx, base - 12.0f, hs[frame], 8.0f, frame, 17);
}

void coffin(Canvas& c) {
  // lying N-S, seen from above-front: hexagonal lid + front face
  int cx = c.w / 2;
  std::vector<Vec2> lid = {{cx - 3.0f, 1}, {cx + 3.0f, 1}, {cx + 7.0f, 7}, {cx + 5.0f, 22}, {cx - 5.0f, 22}, {cx - 7.0f, 7}};
  poly(c, lid, kWoodDark[3]);
  std::vector<Vec2> right = {{cx + 1.0f, 1}, {cx + 3.0f, 1}, {cx + 7.0f, 7}, {cx + 5.0f, 22}, {cx + 1.0f, 22}};
  poly(c, right, kWoodDark[2]);
  for (int y = 22; y < c.h - 1; y++) for (int x = cx - 5; x < cx + 5; x++) c.set(x, y, kWoodDark[x < cx ? 1 : 0]);
  vline(c, cx - 1, 5, 17, kGold[3]); hline(c, cx - 3, cx + 1, 8, kGold[3]); vline(c, cx, 6, 17, kGold[1]);
}

void urn(Canvas& c) {
  const Ramp Cl = ramp5(rgba(84, 40, 40), rgba(132, 66, 48), rgba(178, 98, 62), rgba(210, 138, 86), rgba(234, 178, 120));
  float cx = c.w * 0.5f;
  int base = c.h - 2;
  for (int y = 2; y <= base; y++) {
    float t = (float)(y - 2) / (base - 2);
    float hw = t < 0.2f ? 2.5f : 2.0f + std::sin((t - 0.1f) * PI) * 3.5f;
    if (t > 0.92f) hw = 3.0f;
    for (int x = (int)(cx - hw); x < (int)std::ceil(cx + hw); x++) {
      float u = (x + 0.5f - cx) / hw;
      c.set(x, y, Cl[lightIndex(lightAt(u * 0.9f, (t - 0.5f) * 0.6f), x, y, 0.08f)]);
    }
  }
  ellipse(c, cx, 2.5f, 2.6f, 1.0f, Cl[0]);
  hline(c, (int)cx - 2, (int)cx + 2, 1, Cl[3]);
  for (int x = (int)cx - 4; x <= (int)cx + 4; x++) if (solid(c, x, 8) && x % 2) c.set(x, 8, kStone[4]);   // painted band
  for (int x = (int)cx - 4; x <= (int)cx + 4; x++) if (solid(c, x, 10)) c.set(x, 10, Cl[1]);
}

void ironDoor(Canvas& c) {
  // stone frame + iron-banded door
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      bool frame = x < 2 || x > c.w - 3 || y < 2;
      if (!frame) continue;
      int k = ((x + (y / 3) * 2) % 4 == 0) ? 1 : (x < 2 ? 3 : 2);
      c.set(x, y, kStone[k]);
    }
  for (int y = 2; y < c.h - 1; y++)
    for (int x = 2; x < c.w - 2; x++) {
      int k = ((x - 2) % 3 == 2) ? 0 : 1;
      c.set(x, y, kWoodDark[k + (x < 5 ? 1 : 0)]);
    }
  for (int by : {5, 12, 19}) for (int x = 2; x < c.w - 2; x++) c.set(x, by, kIron[x < 5 ? 3 : 2]);
  for (int by : {5, 12, 19}) for (int x = 3; x < c.w - 2; x += 4) c.set(x, by, kIron[4]);
  c.set(c.w - 5, 13, kIron[4]); c.set(c.w - 5, 14, kIron[1]);
  hline(c, 0, c.w - 1, c.h - 1, kStone[0]);
}

void altar(Canvas& c) {
  int base = c.h - 2;
  plankBox(c, 1, base - 13, c.w - 2, 5, 9, kStone);
  // cloth draped over the top and front
  for (int y = base - 13; y < base - 5; y++)
    for (int x = 7; x < c.w - 7; x++) {
      bool front = y >= base - 8;
      c.set(x, y, front ? kRed[(x < 10 ? 2 : 1)] : kRed[3]);
    }
  for (int x = 7; x < c.w - 7; x += 2) c.set(x, base - 5, kGold[3]);
  // candles + relic
  for (int x : {4, c.w - 6}) { vline(c, x, base - 17, base - 14, kCloth[3]); c.set(x, base - 18, kGlow[4]); c.set(x, base - 19, kFire[3]); }
  ball(c, c.w * 0.5f, base - 15.0f, 2.2f, 2.0f, kGold);
  c.set(c.w / 2 - 1, base - 16, kWhite);
}

// ---- interior furniture -----------------------------------------------------------------------------
// M0b fix round: a bed in 3/4 view, head to the wall: turned posts, a panelled headboard, a long pillow, the sheet
// turned down over a plain wool quilt that falls in soft folds and drapes over the foot rail, the frame's front face.
// 20x32 (one tile; the headboard rises against the wall above) or 20x48 (the two-tile bed of the M0b rooms: the head
// tile and the bed's tile, the headboard against the wall above the head tile).
void bedPaint(Canvas& c, const Ramp& Q) {
  const int W = c.w, H = c.h;
  const Ramp& R = kWood;
  const bool lng = H >= 48;
  const int hb0 = lng ? 5 : 2, hb1 = lng ? 18 : 11;   // the headboard's rows
  // headboard: an arched top rail and a sunk panel between the two back posts
  for (int y = hb0; y <= hb1; y++)
    for (int x = 2; x <= W - 3; x++) {
      float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f - 2);
      int arch = hb0 + (int)(dx * dx * 2.5f);
      if (y < arch) continue;
      int k = 2;
      if (y == arch) k = 4;
      else if (y == arch + 1) k = 3;
      bool inset = x >= 4 && x <= W - 5 && y >= arch + 3 && y <= hb1 - 1;
      if (inset) k = (x == 4 || y == arch + 3) ? 1 : (x == W - 5 ? 3 : 2);
      if (x >= W - 4 && !inset) k = std::max(0, k - 1);
      c.set(x, y, R[k]);
    }
  // the back posts with ball finials
  for (int side = 0; side < 2; side++) {
    int x0 = side ? W - 2 : 0;
    for (int y = hb0; y <= hb1 + 2; y++) { c.set(x0, y, R[side ? 2 : 4]); c.set(x0 + 1, y, R[side ? 1 : 2]); }
    c.set(x0, hb0 - 1, R[side ? 3 : 4]); c.set(x0 + 1, hb0 - 1, R[side ? 2 : 3]); c.set(x0, hb0 - 2, R[3]); c.set(x0 + 1, hb0 - 2, R[2]);
  }
  // the mattress top: pillow, the sheet's turn-down, the quilt
  const int py0 = hb1, py1 = hb1 + (lng ? 5 : 5), sy = py1 + 1, qy0 = sy + 2, qy1 = H - 7;
  for (int y = py0; y <= qy1 + 1; y++)
    for (int x = 1; x <= W - 2; x++) {
      uint32_t col;
      if (y <= py1) {   // a long bolster pillow, plump, lit from the top-left
        float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f - 2.0f), dy = (y - (py0 + py1) * 0.5f) / ((py1 - py0) * 0.5f + 0.6f);
        if (dx * dx * dx * dx + dy * dy > 1.0f) { col = kCloth[2]; c.set(x, y, col); continue; }
        int k = lightIndex(lightAt(dx * 0.55f, dy * 0.9f), x, y, 0.0f);
        col = k >= 3 ? kWhite : (k == 2 ? kCloth[4] : kCloth[3]);
        if (y == py1 && x > 3 && x < W - 4) col = kCloth[2];
      } else if (y <= sy + 1) {   // the sheet turned down over the quilt's edge
        col = y == sy ? kWhite : kCloth[3];
        if (x == 1) col = kCloth[4];
        if (x >= W - 3) col = kCloth[2];
      } else {   // the quilt: broad soft folds running down the bed, a stitched border, darker toward the foot
        float fold = std::sin((x - 1) * 0.9f + (y - qy0) * 0.25f) * 0.6f + std::sin((y - qy0) * 0.7f) * 0.3f;
        int k = 2 + (fold > 0.45f ? 1 : (fold < -0.5f ? -1 : 0));
        if (x <= 2) k = std::min(4, k + 1);
        if (x >= W - 3) k = std::max(0, k - 1);
        if (y == qy0) k = std::max(0, k - 1);
        if (y >= qy1) k = std::max(0, k - 1);
        if ((x == 3 || x == W - 4) && y > qy0) k = std::max(0, k - 1);   // the border's stitching
        col = Q[std::clamp(k, 0, 4)];
      }
      c.set(x, y, col);
    }
  // the side rails under the quilt's edge
  for (int y = hb1 + 1; y <= H - 6; y++) { c.set(0, y, R[3]); c.set(W - 1, y, R[1]); }
  // the foot: the quilt drapes over the rail; the rail's front face and the front posts
  for (int x = 1; x <= W - 2; x++) { c.set(x, H - 6, Q[1]); c.set(x, H - 5, Q[0]); }
  for (int y = H - 4; y <= H - 1; y++)
    for (int x = 0; x < W; x++) {
      int k = y == H - 4 ? 3 : (y == H - 1 ? 0 : 2);
      if (x <= 1) k = std::min(4, k + 1);
      if (x >= W - 2) k = std::max(0, k - 1);
      c.set(x, y, R[k]);
    }
  for (int side = 0; side < 2; side++) {
    int x0 = side ? W - 2 : 0;
    for (int y = H - 8; y <= H - 1; y++) { c.set(x0, y, R[side ? 2 : 4]); c.set(x0 + 1, y, R[side ? 1 : 3]); }
    c.set(x0, H - 9, R[side ? 3 : 4]); c.set(x0 + 1, H - 9, R[side ? 2 : 3]);
  }
}
void bed(Canvas& c) { bedPaint(c, ramp(rgba(58, 92, 150))); }
// the two-tile bed of the M0b rooms in one of four quilts (Piece::Styled 4)
Canvas longBedPiece(int variant) {
  static const uint32_t quilt[4] = {rgba(58, 92, 150), rgba(150, 52, 50), rgba(64, 120, 76), rgba(176, 128, 52)};
  Canvas c(20, 48);
  bedPaint(c, ramp(quilt[variant & 3]));
  // the shade the bed casts on the floor along its right side
  for (int y = 22; y < 47; y++) if (!chA(c.get(19, y))) c.set(19, y, withA(rgba(34, 20, 46), 90));   // kShadowCol
  outline(c);
  return c;
}

void tableSeg(Canvas& c, int part);
void counterSeg(Canvas& c, int part, int goods = -1);
void table(Canvas& c) {
  Canvas l(16, 18), r(16, 18);
  tableSeg(l, 0); tableSeg(r, 2);
  blit(c, l, 0, c.h - 18); blit(c, r, 16, c.h - 18);
}

void chair(Canvas& c) {
  int W = c.w, base = c.h - 2;
  for (int y = 0; y < 8; y++) { c.set(1, y, kWood[3]); c.set(W - 2, y, kWood[1]); }
  for (int y = 1; y < 7; y += 2) hline(c, 2, W - 3, y, kWood[2]);
  plankBox(c, 1, 8, W - 2, 3, 2, kWood);
  for (int y = 13; y <= base; y++) { c.set(1, y, kWood[2]); c.set(W - 2, y, kWood[0]); }
}

void shelfWithJars(Canvas& c) {
  int W = c.w;
  for (int y = 0; y < c.h - 1; y++) { c.set(0, y, kWood[3]); c.set(W - 1, y, kWood[1]); }
  for (int s = 0; s < 3; s++) {
    int sy = 8 + s * 9;
    hline(c, 0, W - 1, sy, kWood[3]); hline(c, 0, W - 1, sy + 1, kWood[1]);
    for (int i = 0; i < 4; i++) {
      int x = 3 + i * 5 + (int)(hash3(i, s, 9) % 2);
      uint32_t col = (i + s) % 3 == 0 ? rgba(120, 170, 200) : ((i + s) % 3 == 1 ? rgba(180, 110, 70) : rgba(130, 170, 90));
      Ramp r = ramp(col, 0.8f);
      int h = 4 + (int)(hash3(i, s, 4) % 3);
      for (int y = sy - h; y < sy; y++) { c.set(x, y, r[3]); c.set(x + 1, y, r[2]); c.set(x + 2, y, r[1]); }
      hline(c, x, x + 2, sy - h - 1, kWood[2]);
      c.set(x, sy - h + 1, r[4]);
    }
  }
}

void fireplace(Canvas& c, int frame) {
  int W = c.w, base = c.h - 2;
  // stone chimney breast
  for (int y = 0; y <= base; y++)
    for (int x = 0; x < W; x++) {
      int row = y / 4;
      bool mortar = (y % 4 == 3) || ((x + (row & 1) * 3) % 6 == 0);
      int k = mortar ? 1 : (x < 3 ? 3 : (x > W - 4 ? 1 : 2));
      if (!mortar && hash3(x / 6, row, 7) % 3 == 0) k++;
      c.set(x, y, kStoneWarm[std::min(k, 4)]);
    }
  // mantel
  for (int x = 0; x < W; x++) { c.set(x, 9, kWood[3]); c.set(x, 10, kWood[2]); c.set(x, 11, kWood[0]); }
  // hearth opening with fire
  for (int y = 13; y <= base; y++)
    for (int x = 6; x < W - 6; x++) {
      float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f - 6);
      if (y < 13 + dx * dx * 4) continue;
      c.set(x, y, y < 16 ? rgba(40, 24, 30) : rgba(22, 14, 22));
    }
  capsule(c, V(10, base - 1), V(W - 10, base - 2), 1.2f, 1.2f, kWood);
  static const float hs[4] = {9.0f, 11.0f, 8.5f, 10.5f};
  flame(c, W * 0.5f - 3, base - 2.0f, hs[frame] * 0.8f, 6.0f, frame, 23);
  flame(c, W * 0.5f + 3, base - 2.0f, hs[(frame + 2) & 3] * 0.7f, 5.0f, frame + 1, 29);
  // warm glow on the stones around the opening
  for (int y = 12; y <= base; y++)
    for (int x = 3; x < W - 3; x++) {
      uint32_t p = c.get(x, y);
      bool stone = false;
      for (int k = 0; k < 5; k++) if (p == kStoneWarm[k]) stone = true;
      if (stone && (x < 8 || x > W - 9)) c.set(x, y, mix(p, kFire[2], 0.25f));
    }
}

void rug(Canvas& c) {
  int W = c.w, H = c.h;
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      int bx = std::min(x - 1, W - 2 - x), by = std::min(y - 1, H - 2 - y);
      int b = std::min(bx, by);
      uint32_t col;
      if (b == 0) col = kRed[1];
      else if (b == 1) col = kGold[2];
      else if (b == 2) col = kRed[2];
      else {
        int dx = std::abs(x - W / 2), dy = std::abs(y - H / 2);
        col = ((dx + dy * 2) % 6 < 2) ? kGold[3] : (((dx + dy * 2) % 6 < 3) ? kRed[3] : kRed[2]);
        if (dx + dy * 2 < 4) col = kGold[4];
      }
      c.set(x, y, col);
    }
  for (int y = 1; y < H - 1; y += 2) { c.set(0, y, kCloth[3]); c.set(W - 1, y, kCloth[3]); }   // tassels
}

void counter(Canvas& c) {
  Canvas l(16, 20), r(16, 20);
  counterSeg(l, 0); counterSeg(r, 2);
  blit(c, l, 0, c.h - 20); blit(c, r, 16, c.h - 20);
}

void kegRack(Canvas& c) {
  // a keg lying on a cradle, its round end to the viewer with a brass tap
  const float cx = c.w * 0.5f, cy = c.h - 7.0f;
  for (int y = 2; y <= (int)cy; y++)   // the barrel's top, running back toward the wall
    for (int x = (int)(cx - 5); x <= (int)(cx + 4); x++) {
      int k = x < cx - 3 ? 3 : (x > cx + 2 ? 1 : 2);
      if ((x - (int)(cx - 5)) % 3 == 0) k = std::max(0, k - 1);
      if (y == 4 || y == 7) k = 0;   // hoops
      c.set(x, y, y == 4 || y == 7 ? kIron[x < cx ? 3 : 1] : kWood[k]);
    }
  for (int y = (int)(cy - 6); y <= (int)(cy + 6); y++)
    for (int x = (int)(cx - 7); x <= (int)(cx + 7); x++) {
      float dx = (x + 0.5f - cx) / 6.2f, dy = (y + 0.5f - cy) / 5.6f;
      float d = dx * dx + dy * dy;
      if (d > 1) continue;
      uint32_t col;
      if (d > 0.72f) col = kIron[(dx + dy < 0) ? 3 : 1];             // the end hoop
      else if (d > 0.55f) col = kWood[(dx + dy < 0) ? 1 : 0];        // the chime
      else col = ((x - (int)cx + 8) % 3 == 0) ? kWood[2] : kWood[3];  // head boards
      c.set(x, y, col);
    }
  box(c, (int)cx - 1, (int)cy + 1, (int)cx, (int)cy + 2, kBrass[3]); c.set((int)cx, (int)cy + 3, kBrass[1]); c.set((int)cx - 1, (int)cy + 1, kBrass[4]);
  // cradle legs
  line(c, (int)cx - 6, c.h - 2, (int)cx - 3, c.h - 4, kWoodDark[2]); line(c, (int)cx + 5, c.h - 2, (int)cx + 2, c.h - 4, kWoodDark[1]);
  hline(c, (int)cx - 7, (int)cx - 5, c.h - 1, kWoodDark[1]); hline(c, (int)cx + 4, (int)cx + 6, c.h - 1, kWoodDark[0]);
}

void plantPot(Canvas& c) {
  int cx = c.w / 2, base = c.h - 2;
  const Ramp Cl = ramp5(rgba(84, 40, 40), rgba(132, 66, 48), rgba(178, 98, 62), rgba(210, 138, 86), rgba(234, 178, 120));
  for (int y = base - 5; y <= base; y++)
    for (int x = cx - 4 + (y - (base - 5)) / 3; x <= cx + 3 - (y - (base - 5)) / 3; x++) c.set(x, y, Cl[x < cx - 1 ? 3 : (x > cx + 1 ? 1 : 2)]);
  hline(c, cx - 5, cx + 4, base - 6, Cl[4]);
  std::vector<Blob> b = {{cx - 2.5f, base - 11.0f, 3.5f}, {cx + 2.5f, base - 11.0f, 3.5f}, {cx + 0.0f, base - 14.0f, 3.5f}, {cx + 0.0f, base - 9.0f, 3.0f}};
  canopy(c, b, kLeaf, 77, 0.9f);
}

void cauldron(Canvas& c, int frame) {
  float cx = c.w * 0.5f;
  int base = c.h - 1;
  // split logs and a low fire under the pot
  capsule(c, V(cx - 7, base - 1), V(cx + 6, base - 1.5f), 1.1f, 1.0f, kWood);
  flame(c, cx - 3, base - 1.0f, 4.0f + (frame & 1), 3.5f, frame, 41);
  flame(c, cx + 3, base - 1.0f, 4.5f - (frame & 1) * 0.5f, 3.5f, frame + 2, 43);
  flame(c, cx, base - 1.0f, 3.0f + ((frame + 1) & 1), 3.0f, frame + 1, 47);
  // iron pot on stubby legs
  const Ramp K = ramp5(rgba(20, 16, 26), rgba(34, 30, 42), rgba(54, 50, 64), rgba(84, 80, 96), rgba(126, 124, 140));
  vline(c, (int)cx - 5, base - 5, base - 3, K[2]); vline(c, (int)cx + 4, base - 5, base - 3, K[1]);
  ball(c, cx, base - 9.0f, 7.5f, 5.5f, K);
  ellipse(c, cx, base - 13.5f, 7.0f, 2.0f, K[3]);
  ellipse(c, cx, base - 13.5f, 5.6f, 1.3f, rgba(110, 200, 90));
  hline(c, (int)cx - 3, (int)cx + 1, base - 14, rgba(160, 236, 120));
  c.set((int)cx - 5, base - 11, K[4]);
  // bubbles + rising steam
  static const int bx[4][2] = {{-3, 1}, {2, -1}, {-1, 3}, {3, -3}};
  for (int i = 0; i < 2; i++) {
    int x = (int)cx + bx[(frame + i * 2) & 3][i], y = base - 14 - ((frame + i) & 1);
    c.set(x, y, rgba(190, 250, 150)); c.set(x + 1, y, rgba(140, 220, 110));
  }
  for (int i = 0; i < 2; i++) c.set((int)cx - 2 + i * 4, 1 + ((frame + i) % 3), withA(rgba(170, 240, 150), 150));
}

void bookshelf(Canvas& c) {
  int W = c.w, H = c.h;
  for (int y = 0; y < H - 1; y++) for (int x = 0; x < W; x++) c.set(x, y, kWoodDark[1]);
  for (int y = 0; y < H - 1; y++) { c.set(0, y, kWood[3]); c.set(1, y, kWood[2]); c.set(W - 2, y, kWood[1]); c.set(W - 1, y, kWood[0]); }
  hline(c, 0, W - 1, 0, kWood[4]);
  const uint32_t spines[6] = {rgba(150, 40, 46), rgba(50, 90, 150), rgba(60, 120, 70), rgba(170, 130, 50), rgba(110, 60, 130), rgba(140, 90, 60)};
  for (int s = 0; s < 3; s++) {
    int top = 2 + s * 10, bottom = top + 8;
    int x = 2;
    int i = 0;
    while (x < W - 3) {
      int w = 2 + (int)(hash3(i, s, 13) % 2);
      int h = 5 + (int)(hash3(i, s, 17) % 3);
      if (x + w > W - 2) break;
      Ramp r = ramp(spines[hash3(i, s, 19) % 6], 0.8f);
      bool lean = hash3(i, s, 23) % 9 == 0;
      for (int y = bottom - h; y < bottom; y++)
        for (int k = 0; k < w; k++) c.set(x + k + (lean && y < bottom - 3 ? 1 : 0), y, r[k == 0 ? 3 : (k == w - 1 ? 1 : 2)]);
      c.set(x, bottom - h + 1, kGold[3]);
      x += w;
      i++;
    }
    hline(c, 0, W - 1, bottom, kWood[3]); hline(c, 0, W - 1, bottom + 1, kWood[1]);
  }
}

void throne(Canvas& c) {
  int W = c.w, base = c.h - 2;
  // tall back
  for (int y = 1; y < 20; y++)
    for (int x = 3; x < W - 3; x++) {
      int k = x < 5 ? 3 : (x > W - 6 ? 1 : 2);
      c.set(x, y, kGold[k]);
    }
  for (int y = 4; y < 19; y++) for (int x = 6; x < W - 6; x++) c.set(x, y, kRed[x < 9 ? 3 : 2]);
  // crest spikes
  for (int i = 0; i < 3; i++) { int x = 5 + i * 7; c.set(x, 0, kGold[4]); c.set(x + 1, 0, kGold[2]); }
  c.set(W / 2 - 1, 7, kGold[4]); c.set(W / 2, 8, kGold[3]); c.set(W / 2 - 1, 9, kGold[3]);
  // seat + arms
  plankBox(c, 1, 19, W - 2, 4, base - 22, kGold);
  for (int y = 20; y < 23; y++) for (int x = 5; x < W - 5; x++) c.set(x, y, kRed[3]);
  for (int ax : {0, W - 4}) { box(c, ax, 15, ax + 3, 18, kGold[ax == 0 ? 3 : 1]); hline(c, ax, ax + 3, 15, kGold[4]); }
}

// =====================================================================================================
// M0 interiors: wall decor, furniture, room surfaces (walls, caps, floors, rugs, shadows) and floor clutter.
// Conventions: 3/4 view, light from the top-left. A piece of furniture shows a lit top surface with a bright
// front lip, a front face (left column lit, right column in shade) and a dark foot row. Wall decor canvases are
// 16x30 with the bottom row at the foot of the back wall (the wall face spans canvas rows 3..29).
// =====================================================================================================
// M0b fix round: the boards underfoot are an older, greyer oak a step darker than the warm wood of the walls and
// furniture, so walls, wall tops, stairs and furniture stand off the floor
const Ramp kFloorWood = ramp5(rgba(50, 36, 40), rgba(82, 60, 54), rgba(114, 86, 68), rgba(140, 110, 84), rgba(166, 138, 106));
const Ramp kFloorOld = ramp5(rgba(58, 44, 46), rgba(94, 74, 64), rgba(130, 106, 84), rgba(160, 134, 106), rgba(188, 164, 132));
const Ramp kPlaster = ramp5(rgba(124, 98, 94), rgba(170, 146, 124), rgba(206, 188, 156), rgba(226, 212, 178), rgba(242, 232, 204));
const Ramp kBlueCloth = ramp5(rgba(30, 36, 76), rgba(44, 62, 118), rgba(64, 96, 160), rgba(104, 140, 194), rgba(160, 192, 226));
const Ramp kGreenCloth = ramp5(rgba(26, 50, 46), rgba(40, 82, 56), rgba(62, 118, 66), rgba(102, 154, 80), rgba(158, 194, 108));
const Ramp kClay = ramp5(rgba(84, 40, 40), rgba(132, 66, 48), rgba(178, 98, 62), rgba(210, 138, 86), rgba(234, 178, 120));
const Ramp kGlassG = ramp5(rgba(26, 54, 50), rgba(40, 90, 70), rgba(66, 134, 92), rgba(124, 186, 136), rgba(206, 238, 206));
const Ramp kSky = ramp5(rgba(70, 96, 150), rgba(96, 136, 190), rgba(132, 176, 218), rgba(176, 210, 236), rgba(226, 240, 250));
const Ramp kPewter = ramp5(rgba(52, 52, 68), rgba(88, 90, 106), rgba(130, 132, 144), rgba(170, 172, 180), rgba(214, 216, 220));
const Ramp kSoot = ramp5(rgba(30, 28, 38), rgba(52, 50, 60), rgba(80, 76, 84), rgba(110, 104, 108), rgba(144, 136, 134));
// M0b regional walls: lime-washed mud (Adobe), whitewash (Plaster), the adobe's ochre dado, terracotta floor tiles
const Ramp kMud = ramp5(rgba(104, 70, 62), rgba(150, 108, 82), rgba(192, 150, 110), rgba(218, 184, 140), rgba(238, 214, 174));
const Ramp kLime = ramp5(rgba(126, 112, 120), rgba(176, 166, 164), rgba(212, 204, 192), rgba(232, 226, 212), rgba(248, 244, 232));
const Ramp kOchre = ramp5(rgba(92, 46, 44), rgba(136, 70, 50), rgba(174, 100, 62), rgba(202, 132, 80), rgba(226, 170, 110));
const Ramp kTerra = ramp5(rgba(94, 50, 46), rgba(138, 74, 54), rgba(176, 102, 68), rgba(202, 132, 88), rgba(226, 168, 120));
const Ramp kPaint = ramp5(rgba(36, 50, 58), rgba(52, 76, 82), rgba(72, 106, 106), rgba(100, 138, 128), rgba(146, 176, 156));
const uint32_t kVoid = rgba(18, 13, 20);
const uint32_t kShadowCol = rgba(34, 20, 46);

inline uint32_t tone(const Ramp& R, int k, float t) {   // base step k nudged toward a neighbour step by t (-1..1)
  if (t > 0) return mix(R[k], R[k + 1], t);
  return mix(R[k], R[k - 1], -t);
}

// a 3/4 block: top surface (topH rows, bright front lip on its last row) over a front face (frontH rows)
void block34(Canvas& c, int x0, int y0, int w, int topH, int frontH, const Ramp& R, int topK = 3, int frontK = 2) {
  for (int y = 0; y < topH; y++)
    for (int x = 0; x < w; x++) {
      int k = topK;
      if (x == 0) k = topK + 1;
      else if (x == w - 1) k = topK - 1;
      if (y == topH - 1) k = 4;
      c.set(x0 + x, y0 + y, R[k]);
    }
  for (int y = 0; y < frontH; y++)
    for (int x = 0; x < w; x++) {
      int k = frontK;
      if (x == 0) k = frontK + 1;
      else if (x >= w - 1) k = frontK - 1;
      if (y == 0) k = std::max(0, k - 1);              // the lip casts a hairline shadow on the face
      if (y == frontH - 1) k = std::max(0, frontK - 2);
      c.set(x0 + x, y0 + topH + y, R[k]);
    }
}

// small tabletop things (a few pixels each); (x, y) is the item's base on the table surface
void plateAt(Canvas& c, int x, int y, uint32_t food) {
  hline(c, x - 2, x + 2, y, kBone[2]); hline(c, x - 2, x + 2, y - 1, kBone[4]); hline(c, x - 1, x + 1, y - 2, kBone[3]);
  c.set(x - 1, y - 1, food); c.set(x, y - 1, food); c.set(x, y - 2, mix(food, kWhite, 0.4f));
}
void mugAt(Canvas& c, int x, int y, bool foam) {
  vline(c, x, y - 3, y, kPewter[3]); vline(c, x + 1, y - 3, y, kPewter[2]); vline(c, x + 2, y - 3, y, kPewter[1]);
  c.set(x + 3, y - 2, kPewter[1]);
  if (foam) { hline(c, x, x + 2, y - 4, kWhite); c.set(x + 1, y - 3, kCloth[4]); }
  else hline(c, x, x + 2, y - 3, kPewter[0]);
}
void candleAt(Canvas& c, int x, int y, int h) {
  vline(c, x, y - h + 1, y, kCloth[4]); vline(c, x + 1, y - h + 1, y, kCloth[2]);
  c.set(x, y - h, kFire[3]); c.set(x, y - h - 1, kFire[4]); c.set(x + 1, y - h, kFire[2]);
  c.set(x, y - h - 2, withA(kGlow[4], 140));
}
void breadAt(Canvas& c, int x, int y) {
  hline(c, x - 2, x + 2, y, kThatch[1]); hline(c, x - 2, x + 2, y - 1, kThatch[2]); hline(c, x - 1, x + 1, y - 2, kThatch[3]);
  c.set(x - 1, y - 1, kThatch[4]); c.set(x + 1, y - 2, kThatch[2]);
}
void jugAt(Canvas& c, int x, int y) {
  ball(c, x + 0.5f, y - 2.0f, 2.2f, 2.4f, kClay);
  vline(c, x, y - 5, y - 4, kClay[2]); c.set(x + 1, y - 5, kClay[1]); c.set(x, y - 6, kClay[3]);
  c.set(x + 3, y - 3, kClay[1]);
}
void bookAt(Canvas& c, int x, int y, const Ramp& R) {   // a closed book lying flat, 5x3
  hline(c, x, x + 4, y - 2, R[3]); hline(c, x, x + 4, y - 1, R[2]); hline(c, x, x + 4, y, R[1]);
  hline(c, x + 1, x + 4, y - 1, kCloth[4]); c.set(x, y - 2, R[4]);
}

// ---- wall decor (16x30, hangs on the back wall) -------------------------------------------------------
void tapestry(Canvas& c) {
  const Ramp& F = kRed;
  hline(c, 1, 14, 3, kWoodDark[3]); hline(c, 1, 14, 4, kWoodDark[1]);
  c.set(0, 3, kBrass[4]); c.set(0, 4, kBrass[2]); c.set(15, 3, kBrass[3]); c.set(15, 4, kBrass[1]);
  for (int y = 5; y <= 22; y++)
    for (int x = 2; x <= 13; x++) {
      if (y >= 20 && ((x + (y - 20)) % 3 == 0 || y == 22) && (x & 1)) continue;   // tasselled lower edge
      int k = 2;
      float fold = std::sin((x - 2) * 1.15f + 0.6f);
      if (fold > 0.55f) k = 3;
      else if (fold < -0.6f) k = 1;
      if (x == 2) k = std::min(4, k + 1);
      if (x == 13) k = std::max(0, k - 1);
      bool border = x <= 3 || x >= 12 || y <= 6 || y >= 18;
      uint32_t col = F[k];
      if (border && !(x == 2 || x == 13)) col = (x + y) % 3 == 0 ? kGold[k + 1 > 4 ? 4 : k + 1] : kGold[std::max(1, k)];
      c.set(x, y, col);
    }
  // emblem: a stag's head in gold on the field
  const int ex = 8, ey = 12;
  c.set(ex - 3, ey - 3, kGold[3]); c.set(ex - 2, ey - 2, kGold[3]); c.set(ex + 2, ey - 3, kGold[2]); c.set(ex + 1, ey - 2, kGold[2]);
  c.set(ex - 4, ey - 4, kGold[4]); c.set(ex + 3, ey - 4, kGold[3]);
  box(c, ex - 1, ey - 1, ex, ey + 2, kGold[3]); c.set(ex - 1, ey - 1, kGold[4]); c.set(ex, ey + 2, kGold[1]);
  c.set(ex - 2, ey, kGold[2]); c.set(ex + 1, ey, kGold[2]);
}

void wallShelf(Canvas& c) {
  // plank on two brackets with jars, a bowl and a bottle
  hline(c, 1, 14, 13, kWood[4]); hline(c, 1, 14, 14, kWood[2]); hline(c, 1, 14, 15, kWood[1]);
  for (int bx : {3, 12}) { vline(c, bx, 16, 18, kWoodDark[2]); c.set(bx + 1, 16, kWoodDark[1]); }
  // clay jar
  for (int y = 9; y <= 12; y++) { c.set(2, y, kClay[3]); c.set(3, y, kClay[2]); c.set(4, y, kClay[1]); }
  hline(c, 2, 4, 8, kClay[4]); c.set(3, 7, kCloth[3]); c.set(2, 10, kClay[4]);
  // green bottle
  for (int y = 8; y <= 12; y++) { c.set(6, y, kGlassG[3]); c.set(7, y, kGlassG[1]); }
  c.set(6, 7, kGlassG[2]); c.set(6, 6, kWood[2]); c.set(6, 9, kGlassG[4]);
  // bowl and a round pot
  hline(c, 9, 13, 12, kWood[1]); hline(c, 9, 13, 11, kWood[3]); c.set(9, 11, kWood[4]); hline(c, 10, 12, 10, rgba(200, 70, 60)); c.set(11, 9, rgba(240, 120, 90));
  ball(c, 12.5f, 9.5f, 1.6f, 1.6f, kPewter);
}

void herbBundle(Canvas& c) {
  // a peg rail with three bunches of drying herbs hanging upside down
  hline(c, 1, 14, 3, kWood[3]); hline(c, 1, 14, 4, kWood[1]);
  const Ramp* rs[3] = {&kGreenCloth, &kPurple, &kThatch};
  for (int i = 0; i < 3; i++) {
    int x = 3 + i * 5, top = 5 + (i == 1 ? 1 : 0);
    vline(c, x, top, top + 1, kCloth[1]);
    hline(c, x - 1, x + 1, top + 2, kRed[2]);   // tie
    const Ramp& R = *rs[i];
    for (int j = 0; j < 7; j++) {
      int half = j < 2 ? 1 : (j < 5 ? 2 : 1);
      for (int dx = -half; dx <= half; dx++) {
        int k = dx < 0 ? 3 : (dx > 0 ? 1 : 2);
        if ((j + dx + i) % 3 == 0) k = std::max(0, k - 1);
        c.set(x + dx, top + 3 + j, R[k]);
      }
    }
    c.set(x - 1, top + 3, R[4]);
    if (i != 2) c.set(x, top + 10, R[1]);
  }
}

void antlers(Canvas& c) {
  // shield-shaped plaque with a pair of antlers
  for (int y = 10; y <= 16; y++) {
    int half = y < 15 ? 3 : (y == 15 ? 2 : 1);
    for (int x = 8 - half; x <= 7 + half; x++) c.set(x, y, kWoodDark[x < 7 ? 3 : (x > 8 ? 1 : 2)]);
  }
  hline(c, 5, 10, 10, kWood[3]);
  ball(c, 8.0f, 11.5f, 1.6f, 1.4f, kLeather);
  for (int s = -1; s <= 1; s += 2) {
    Vec2 a{8.0f + s * 1.0f, 10.0f}, b{8.0f + s * 5.5f, 4.0f};
    capsule(c, a, b, 0.8f, 0.6f, kBone);
    capsule(c, V(8 + s * 3.0f, 7.4f), V(8 + s * 2.5f, 3.0f), 0.55f, 0.5f, kBone);
    capsule(c, V(8 + s * 5.0f, 5.0f), V(8 + s * 7.0f, 3.5f), 0.55f, 0.5f, kBone);
    c.set(8 + s * 6 - (s < 0 ? 1 : 0), 3, kBone[4]);
  }
}

void painting(Canvas& c) {
  int x0 = 2, y0 = 4, x1 = 13, y1 = 14;
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      bool frame = x <= x0 + 1 || x >= x1 - 1 || y <= y0 + 1 || y >= y1 - 1;
      if (frame) {
        int k = (x == x0 || y == y0) ? 4 : ((x == x1 || y == y1) ? 1 : 3);
        if ((x == x0 + 1 || y == y0 + 1) && k == 3) k = 2;
        c.set(x, y, kGold[k]);
        continue;
      }
      // a little landscape: sky, a far ridge, a green meadow and a lone tree
      float fy = (float)(y - (y0 + 2)) / (y1 - y0 - 4);
      uint32_t col = fy < 0.45f ? (fy < 0.2f ? kSky[2] : kSky[3]) : kGreenCloth[3];
      int ridge = y0 + 5 + ((x * 3) % 5 == 0 ? 0 : 1) - (x > 6 && x < 10 ? 1 : 0);
      if (y >= ridge && fy < 0.62f) col = kBlueCloth[3];
      if (fy >= 0.62f) col = (x + y) % 4 == 0 ? kGreenCloth[2] : kGreenCloth[3];
      if (y == y1 - 2) col = kGreenCloth[2];
      c.set(x, y, col);
    }
  c.set(10, 7, kGreenCloth[2]); c.set(10, 8, kGreenCloth[1]); c.set(9, 8, kGreenCloth[2]); c.set(10, 9, kWood[1]);
  c.set(5, 6, kWhite);   // a cloud
  c.set(6, 6, kSky[4]);
}

void sconce(Canvas& c, int frame) {
  // iron back plate and arm holding a candle cup
  vline(c, 7, 11, 15, kIron[2]); vline(c, 8, 11, 15, kIron[1]); c.set(7, 11, kIron[3]);
  c.set(7, 16, kIron[1]); c.set(8, 16, kIron[0]);
  hline(c, 6, 9, 10, kIron[2]); c.set(6, 10, kIron[3]); c.set(9, 10, kIron[0]);
  hline(c, 6, 9, 9, kIron[3]);
  vline(c, 7, 5, 8, kCloth[4]); vline(c, 8, 5, 8, kCloth[2]);
  c.set(7, 4, kCloth[3]);
  static const int fl[4][3] = {{3, 0, 0}, {4, -1, 0}, {3, 0, 1}, {4, 1, 0}};
  int fh = fl[frame & 3][0], sx = fl[frame & 3][1];
  for (int k = 0; k < fh; k++) c.set(7 + (k == fh - 1 ? sx : 0), 3 - k, kFire[k == 0 ? 2 : (k == fh - 1 ? 4 : 3)]);
  c.set(8, 3, kFire[1 + fl[frame & 3][2]]);
  for (int k = 0; k < 2; k++) { c.set(6, 2 - k, withA(kGlow[3], 90)); c.set(9, 2 - k, withA(kGlow[3], 90)); }
}

void windowProp(Canvas& c) {
  int x0 = 2, x1 = 13, y0 = 3, y1 = 15;
  // deep wooden frame (the wall is thick: a lit sill and a shaded top reveal)
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      bool frame = x == x0 || x == x1 || y == y0 || y == y1;
      if (frame) { c.set(x, y, kWood[(x == x0 || y == y0) ? 3 : 1]); continue; }
      bool mull = x == (x0 + x1) / 2 || y == (y0 + y1) / 2 + 1;
      if (mull) { c.set(x, y, kWood[x == (x0 + x1) / 2 ? 2 : 3]); continue; }
      float t = (float)(y - y0) / (y1 - y0);
      uint32_t col = t < 0.3f ? kSky[2] : (t < 0.6f ? kSky[3] : kSky[4]);
      if (y == y0 + 1) col = kSky[1];                               // the reveal shades the top of the glass
      if ((x - y) % 7 == 0 || (x - y) % 7 == 1) col = mix(col, kWhite, 0.55f);   // reflections
      c.set(x, y, col);
    }
  // sill
  hline(c, x0 - 1, x1 + 1, y1 + 1, kWood[4]); hline(c, x0 - 1, x1 + 1, y1 + 2, kWood[2]);
  // curtains gathered at the sides
  for (int y = y0 - 1; y <= y1 - 1; y++) {
    int wl = y < y0 + 6 ? 2 : (y < y1 - 3 ? 1 : 2);
    for (int i = 0; i < wl; i++) { c.set(x0 - 1 + i, y, kRed[i == 0 ? 3 : 2]); c.set(x1 + 1 - i, y, kRed[i == 0 ? 1 : 2]); }
  }
  hline(c, x0 - 2, x1 + 2, y0 - 2, kWoodDark[2]);   // curtain rod
  c.set(x0 - 2, y0 - 2, kBrass[3]); c.set(x1 + 2, y0 - 2, kBrass[2]);
}

void wallShield(Canvas& c) {
  // crossed swords behind a round painted shield
  line(c, 2, 2, 13, 16, kIron[3]); line(c, 3, 2, 14, 16, kIron[1]);
  line(c, 13, 2, 2, 16, kIron[3]); line(c, 14, 2, 3, 16, kIron[1]);
  for (int s : {0, 1}) { int hx = s ? 12 : 3; hline(c, hx - 1, hx + 1, 15, kBrass[2]); c.set(hx, 16, kLeather[2]); c.set(hx + (s ? 1 : -1), 17, kBrass[3]); }
  for (int y = 4; y <= 15; y++)
    for (int x = 2; x <= 13; x++) {
      float dx = (x + 0.5f - 8.0f) / 5.6f, dy = (y + 0.5f - 9.8f) / 5.6f;
      float d2 = dx * dx + dy * dy;
      if (d2 > 1) continue;
      int k = lightIndex(lightAt(dx * 0.8f, dy * 0.8f), x, y, 0.0f);
      bool rim = d2 > 0.72f;
      bool quarter = (x < 8) == (y < 10);
      c.set(x, y, rim ? kIron[std::min(4, k + 1)] : (quarter ? kRed[k] : kBlueCloth[k]));
    }
  ball(c, 8.0f, 9.8f, 1.6f, 1.6f, kBrass);
  c.set(7, 9, kWhite);
}

void toolRack(Canvas& c) {
  // a pegged board with a hammer, tongs and a saw
  for (int y = 4; y <= 14; y++)
    for (int x = 1; x <= 14; x++) c.set(x, y, kWood[(y == 4) ? 3 : ((x % 5 == 0) ? 1 : 2)]);
  hline(c, 1, 14, 14, kWood[1]);
  for (int px2 : {3, 8, 12}) c.set(px2, 5, kWoodDark[0]);
  // hammer
  vline(c, 3, 6, 15, kWood[3]); vline(c, 4, 6, 15, kWoodDark[2]); hline(c, 1, 5, 6, kIron[3]); hline(c, 1, 5, 7, kIron[1]);
  // tongs
  line(c, 8, 6, 7, 17, kIron[2]); line(c, 9, 6, 10, 17, kIron[1]); c.set(8, 6, kIron[4]);
  // saw
  for (int y = 7; y <= 16; y++) { c.set(12, y, kIron[3]); c.set(13, y, y % 2 ? kIron[1] : kIron[2]); }
  box(c, 11, 5, 13, 6, kWood[2]); c.set(11, 5, kWood[4]);
}

void panRack(Canvas& c) {
  hline(c, 1, 14, 3, kIron[3]); hline(c, 1, 14, 4, kIron[1]);
  // frying pan
  vline(c, 3, 5, 9, kIron[2]); c.set(3, 5, kIron[3]);
  ellipse(c, 3.5f, 12.0f, 3.0f, 2.6f, kSoot[2]); ellipse(c, 3.2f, 11.6f, 2.0f, 1.6f, kSoot[3]); c.set(2, 11, kSoot[4]);
  // pot with handle
  line(c, 8, 5, 6, 8, kIron[1]); line(c, 8, 5, 11, 8, kIron[1]);
  for (int y = 9; y <= 13; y++) for (int x = 6; x <= 11; x++) c.set(x, y, kSoot[x == 6 ? 3 : (x == 11 ? 1 : 2)]);
  hline(c, 6, 11, 9, kSoot[4]); hline(c, 6, 11, 13, kSoot[1]);
  // ladle
  vline(c, 13, 5, 11, kPewter[3]); ellipse(c, 13.5f, 12.5f, 1.5f, 1.2f, kPewter[2]); c.set(13, 12, kPewter[4]);
}

void wreath(Canvas& c) {
  // dried-flower wreath with a ribbon bow
  for (int y = 3; y <= 15; y++)
    for (int x = 2; x <= 13; x++) {
      float dx = (x + 0.5f - 8.0f) / 5.6f, dy = (y + 0.5f - 9.0f) / 5.6f;
      float d = std::sqrt(dx * dx + dy * dy);
      if (d > 1.0f || d < 0.5f) continue;
      float l = lightAt(dx * 0.8f, dy * 0.8f) + (hashf(x, y, 61) - 0.5f) * 0.6f;
      c.set(x, y, kMoss[lightIndex(l, x, y, 0.1f)]);
    }
  for (int i = 0; i < 9; i++) {
    float a = i / 9.0f * TAU + 0.3f;
    int x = (int)std::lround(8 + std::cos(a) * 4.2f - 0.5f), y = (int)std::lround(9 + std::sin(a) * 4.2f - 0.5f);
    c.set(x, y, i % 3 == 0 ? rgba(214, 90, 70) : (i % 3 == 1 ? kGold[4] : rgba(170, 120, 190)));
  }
  c.set(6, 15, kRed[3]); c.set(7, 15, kRed[2]); c.set(8, 15, kRed[2]); c.set(9, 15, kRed[1]);
  c.set(5, 16, kRed[2]); c.set(10, 16, kRed[1]); c.set(6, 17, kRed[1]); c.set(9, 17, kRed[0]);
}

void holySymbol(Canvas& c) {
  // a gilded sun disc with rays
  for (int i = 0; i < 12; i++) {
    float a = i / 12.0f * TAU;
    float r1 = i % 2 ? 6.0f : 7.2f;
    line(c, 8, 9, (int)std::lround(7.5f + std::cos(a) * r1), (int)std::lround(9 + std::sin(a) * r1 * 0.95f), kGold[i % 2 ? 2 : 3]);
  }
  ball(c, 8.0f, 9.5f, 3.6f, 3.6f, kGold);
  ball(c, 8.0f, 9.5f, 1.8f, 1.8f, kGlow);
  c.set(6, 7, kWhite);
}

// ---- furniture ---------------------------------------------------------------------------------------
// table tops for 1-tile tables and the joining segments (left/right ends optional)
void tableTop(Canvas& c, int x0, int x1, int top, int depth, bool leftEnd, bool rightEnd, const Ramp& R) {
  for (int y = top; y < top + depth; y++)
    for (int x = x0; x <= x1; x++) {
      int k = 3;
      if ((y - top) % 3 == 2) k = 2;                       // board seams run along the table
      if (y == top) k = 3;
      if (y == top + depth - 1) k = 4;                     // lit front lip
      if (leftEnd && x == x0) k = std::min(4, k + 1);
      if (rightEnd && x == x1) k = std::max(1, k - 1);
      c.set(x, y, R[k]);
    }
  for (int y = top + depth; y < top + depth + 2; y++)       // apron
    for (int x = x0; x <= x1; x++) {
      int k = y == top + depth ? 1 : 2;
      if (leftEnd && x == x0) k = 2;
      if (rightEnd && x == x1) k = 0;
      c.set(x, y, R[k]);
    }
}
void tableLegs(Canvas& c, int x, int y0, int y1, const Ramp& R, bool back) {
  for (int y = y0; y <= y1; y++) { c.set(x, y, R[back ? 1 : 3]); c.set(x + 1, y, R[back ? 0 : 1]); }
  if (!back) c.set(x, y1, R[0]);
}

void tableSmall(Canvas& c, int kind) {
  const Ramp& R = kWood;
  int top = c.h - 15;
  // back legs peek out under the apron, front legs stand on the tile
  tableLegs(c, 3, top + 8, c.h - 4, R, true);
  tableLegs(c, 11, top + 8, c.h - 4, R, true);
  tableTop(c, 1, 14, top, 8, true, true, R);
  tableLegs(c, 1, top + 10, c.h - 2, R, false);
  tableLegs(c, 13, top + 10, c.h - 2, R, false);
  if (kind == 1) {          // a meal: plate of stew, a mug of ale, bread and cheese
    plateAt(c, 5, top + 5, rgba(170, 96, 52));
    mugAt(c, 10, top + 5, true);
    breadAt(c, 11, top + 2);
    c.set(4, top + 2, kGold[4]); c.set(5, top + 2, kGold[3]); c.set(4, top + 3, kGold[2]); c.set(5, top + 3, kGold[2]);
  } else if (kind == 2) {   // a scholar's evening: open book, candle, inkwell and quill
    for (int x = 3; x <= 9; x++) { c.set(x, top + 3, kWhite); c.set(x, top + 4, x == 6 ? kCloth[2] : kCloth[4]); c.set(x, top + 5, kCloth[3]); }
    c.set(6, top + 3, kCloth[2]); c.set(4, top + 4, kCloth[1]); c.set(8, top + 4, kCloth[1]);
    hline(c, 3, 9, top + 6, kRed[1]);
    candleAt(c, 11, top + 5, 4);
    c.set(12, top + 6, kBrass[2]); c.set(10, top + 6, kBrass[3]);
    c.set(2, top + 2, kInk); c.set(3, top + 2, kSoot[2]); line(c, 3, top + 1, 1, top - 2, kWhite);
  }
}

void tableSeg(Canvas& c, int part) {   // 0 left, 1 middle, 2 right
  const Ramp& R = kWood;
  int top = c.h - 15;
  int x0 = part == 0 ? 1 : 0, x1 = part == 2 ? 14 : 15;
  if (part == 0) tableLegs(c, 3, top + 8, c.h - 4, R, true);
  if (part == 2) tableLegs(c, 11, top + 8, c.h - 4, R, true);
  tableTop(c, x0, x1, top, 8, part == 0, part == 2, R);
  if (part == 0) tableLegs(c, 1, top + 10, c.h - 2, R, false);
  if (part == 2) tableLegs(c, 13, top + 10, c.h - 2, R, false);
  if (part == 1) {   // a place setting on every middle segment: a feast table
    plateAt(c, 7, top + 5, rgba(196, 120, 64));
    mugAt(c, 12, top + 5, (hash3(part, 0, 5) & 1) != 0);
    c.set(3, top + 4, kPewter[3]); c.set(3, top + 5, kPewter[2]);   // knife
  } else if (part == 0) {
    candleAt(c, 5, top + 4, 4);
    c.set(4, top + 5, kBrass[3]); c.set(6, top + 5, kBrass[1]);
    breadAt(c, 11, top + 5);
  } else {
    jugAt(c, 5, top + 5);
    c.set(9, top + 4, kGold[4]); c.set(10, top + 4, kGold[3]); c.set(9, top + 5, kGold[2]); c.set(10, top + 5, kGold[2]); c.set(11, top + 5, kGold[1]);
  }
}

// M0b fix round 2: a shop's counter carries the trade's things, not the inn's tankards: an open ledger and quill at
// the left end, the shop bell at the right, and along the middle brass scales, bolts of cloth, a tray of coins or a
// folded cloth (goods 0..3)
void shopCounterGoods(Canvas& c, int top, int part, int goods) {
  if (part == 0) {   // the ledger, open, and an inkpot with its quill
    for (int y = top; y <= top + 3; y++)
      for (int x = 3; x <= 11; x++) {
        int k = x == 7 ? 1 : (y == top ? 4 : (x < 7 ? 3 : 2));
        c.set(x, y, kBone[k]);
      }
    for (int x : {4, 5, 6, 8, 9, 10}) c.set(x, top + 1, kBone[1]);   // lines of writing
    for (int x : {4, 5, 8, 9, 10}) c.set(x, top + 2, kBone[1]);
    hline(c, 3, 11, top + 4, kLeather[1]);
    c.set(13, top + 2, kWoodDark[0]); c.set(14, top + 2, kWoodDark[0]); c.set(13, top + 1, kWoodDark[1]); c.set(14, top + 1, kPewter[3]);
    c.set(14, top - 1, kBone[4]); c.set(14, top, kBone[3]); c.set(15, top - 2, kBone[4]);
    return;
  }
  if (part == 2) {
    ball(c, 8.0f, top + 1.5f, 3.0f, 2.2f, kBrass);   // the shop bell
    c.set(7, top - 1, kBrass[4]); vline(c, 8, top - 2, top - 1, kBrass[2]);
    return;
  }
  switch (goods & 3) {
    case 0:   // brass scales: a post, the beam, two pans on chains
      vline(c, 8, top - 7, top + 2, kBrass[2]); c.set(7, top - 6, kBrass[4]); c.set(7, top - 5, kBrass[3]);
      hline(c, 7, 9, top + 3, kBrass[1]); hline(c, 6, 10, top + 3, kBrass[1]);
      hline(c, 3, 13, top - 7, kBrass[3]); c.set(3, top - 7, kBrass[4]); c.set(13, top - 7, kBrass[1]);
      for (int sx : {3, 13}) {
        vline(c, sx, top - 6, top - 2, kBrass[1]);
        hline(c, sx - 2, sx + 2, top - 1, kBrass[3]); hline(c, sx - 1, sx + 1, top, kBrass[1]);
        c.set(sx - 2, top - 1, kBrass[4]);
      }
      c.set(12, top - 2, kGold[4]); c.set(13, top - 2, kGold[3]); c.set(14, top - 2, kGold[2]);   // a weight on one pan
      break;
    case 1:   // bolts of cloth lying on the counter, ends towards us
      for (int b = 0; b < 2; b++) {
        const Ramp& R = b == 0 ? kRed : kBlueCloth;
        int x0 = 2 + b * 6;
        for (int y = top - 2; y <= top + 2; y++)
          for (int x = x0; x <= x0 + 4; x++) {
            int k = y == top - 2 ? 4 : (y == top + 2 ? 1 : (x == x0 ? 3 : (x == x0 + 4 ? 1 : 2)));
            c.set(x, y, R[k]);
          }
        c.set(x0 + 2, top, R[0]); c.set(x0 + 1, top - 1, R[4]);
      }
      break;
    case 2:   // a tray of coins and a small strongbox
      for (int x = 2; x <= 8; x++) { c.set(x, top + 2, kWoodDark[1]); c.set(x, top + 1, kWoodDark[3]); }
      for (int i = 0; i < 4; i++) { c.set(3 + i * 1 + (i & 1), top + (i & 1), kGold[3 + (i & 1)]); c.set(3 + i + (i & 1), top + 1 - (i & 1), kGold[2]); }
      for (int y = top - 3; y <= top + 2; y++)
        for (int x = 10; x <= 14; x++) c.set(x, y, y == top - 3 ? kWood[4] : (x == 14 ? kWood[1] : (y == top - 1 ? kIron[2] : kWood[2])));
      c.set(12, top, kGold[4]);
      break;
    default:   // a folded cloth and a jar of buttons
      for (int y = top; y <= top + 2; y++)
        for (int x = 3; x <= 10; x++) c.set(x, y, kCloth[y == top ? 4 : (y == top + 2 ? 1 : 3)]);
      jugAt(c, 12, top + 2);
      break;
  }
}

void counterSeg(Canvas& c, int part, int goods) {
  const Ramp& R = kWood;
  int top = c.h - 18, depth = 5, fh = c.h - top - depth;
  int x0 = part == 0 ? 1 : 0, x1 = part == 2 ? 14 : 15;
  for (int y = top; y < top + depth; y++)
    for (int x = x0; x <= x1; x++) {
      int k = (y == top + depth - 1) ? 4 : 3;
      if (y == top + 2) k = 2;
      if (part == 0 && x == x0) k = 4;
      if (part == 2 && x == x1) k = 2;
      c.set(x, y, kWoodDark[std::min(4, k + 1)]);
    }
  for (int y = top + depth; y < top + depth + fh; y++)
    for (int x = x0; x <= x1; x++) {
      int ly = y - (top + depth);
      int k = 2;
      int px2 = (x + 4) % 8;                                // raised panels, 8 px apart
      bool rail = ly == 0 || ly == 1 || ly >= fh - 3;
      if (!rail) {
        if (px2 == 0) k = 1;
        else if (px2 == 1) k = 3;
        else if (ly == 2) k = 1;
        else if (ly == fh - 4) k = 3;
      } else k = ly == 0 ? 1 : (ly == 1 ? 3 : (ly == fh - 1 ? 0 : 2));
      if (part == 0 && x == x0) k = 3;
      if (part == 2 && x == x1) k = std::min(k, 1);
      c.set(x, y, R[k]);
    }
  if (goods >= 0) { shopCounterGoods(c, top, part, goods); return; }
  if (part == 1) {
    mugAt(c, 3, top + 3, true);
    mugAt(c, 9, top + 3, false);
    c.set(14, top + 3, kGold[4]); c.set(13, top + 3, kGold[3]);
  } else if (part == 0) {
    for (int y = top - 3; y <= top + 3; y++) { c.set(6, y, kGlassG[y < top - 1 ? 2 : 3]); c.set(7, y, kGlassG[1]); }
    c.set(6, top - 4, kWood[2]); c.set(6, top, kGlassG[4]);
  } else {
    ball(c, 8.0f, top + 1.5f, 3.0f, 2.2f, kBrass);   // a brass bell for service
    c.set(7, top - 1, kBrass[4]); vline(c, 8, top - 2, top - 1, kBrass[2]);
  }
}

void cupboard(Canvas& c) {
  const Ramp& R = kWood;
  int W = c.w, base = c.h - 1, top = 1;
  // crown, a dresser top with open plate shelves, a work ledge, and two panelled doors below
  for (int x = 0; x < W; x++) { c.set(x, top, R[4]); c.set(x, top + 1, R[2]); c.set(x, top + 2, R[1]); }
  for (int y = top + 3; y <= base; y++)
    for (int x = 1; x < W - 1; x++) c.set(x, y, R[x == 1 ? 3 : (x == W - 2 ? 1 : 2)]);
  // open shelves (dark back) with plates and jugs
  for (int s = 0; s < 2; s++) {
    int y0 = top + 4 + s * 6, y1 = y0 + 4;
    for (int y = y0; y <= y1; y++) for (int x = 3; x <= W - 4; x++) c.set(x, y, y == y0 ? kWoodDark[0] : kWoodDark[1]);
    hline(c, 2, W - 3, y1 + 1, R[3]);
    if (s == 0) {
      for (int i = 0; i < 3; i++) {   // plates standing on edge
        int x = 4 + i * 3;
        ellipse(c, x + 0.5f, y0 + 2.6f, 1.6f, 2.2f, kBone[i == 0 ? 4 : 3]);
        c.set(x, y0 + 2, kBlueCloth[2]);
      }
    } else {
      jugAt(c, 5, y1);
      c.set(9, y1, kPewter[2]); c.set(9, y1 - 1, kPewter[3]); c.set(10, y1, kPewter[1]); c.set(10, y1 - 1, kPewter[2]);
      c.set(11, y1, kClay[2]); c.set(11, y1 - 1, kClay[3]);
    }
  }
  int ledge = top + 16;
  for (int x = 0; x < W; x++) { c.set(x, ledge, R[4]); c.set(x, ledge + 1, R[2]); c.set(x, ledge + 2, R[0]); }
  // doors
  for (int d = 0; d < 2; d++) {
    int x0 = 2 + d * 6, x1 = x0 + 5;
    for (int y = ledge + 4; y <= base - 3; y++)
      for (int x = x0; x <= x1; x++) {
        bool edge = x == x0 || x == x1 || y == ledge + 4 || y == base - 3;
        c.set(x, y, edge ? (x == x0 || y == ledge + 4 ? R[1] : R[3]) : R[2]);
      }
    c.set(d ? x0 + 1 : x1 - 1, (ledge + base) / 2 + 1, kBrass[3]);
  }
  for (int x = 1; x < W - 1; x++) { c.set(x, base - 1, R[1]); c.set(x, base, R[0]); }
  c.set(1, base, R[1]); c.set(W - 2, base, R[0]);
}

void wardrobe(Canvas& c) {
  const Ramp& R = kWoodDark;
  int W = c.w, base = c.h - 1;
  // corniced top (overhangs by a pixel), two tall doors with raised panels, a plinth on little feet
  for (int x = 0; x < W; x++) { c.set(x, 1, kWood[3]); c.set(x, 2, kWood[4]); c.set(x, 3, kWood[1]); }
  for (int y = 4; y <= base - 2; y++)
    for (int x = 1; x < W - 1; x++) c.set(x, y, R[x == 1 ? 4 : (x == W - 2 ? 1 : 3)]);
  for (int d = 0; d < 2; d++) {
    int x0 = 2 + d * 6, x1 = x0 + 5;
    for (int pnl = 0; pnl < 2; pnl++) {
      int y0 = 6 + pnl * 12, y1 = y0 + 9;
      for (int y = y0; y <= y1; y++)
        for (int x = x0 + 1; x <= x1 - 1; x++) {
          int k = 3;
          if (y == y0 || x == x0 + 1) k = 2;
          if (y == y1 || x == x1 - 1) k = 4;
          c.set(x, y, R[k]);
        }
    }
    vline(c, d ? x0 : x1, 4, base - 2, R[1]);
  }
  c.set(7, 15, kBrass[4]); c.set(8, 15, kBrass[3]); c.set(7, 16, kBrass[2]); c.set(8, 16, kBrass[1]);
  for (int x = 0; x < W; x++) { c.set(x, base - 1, kWood[2]); c.set(x, base, kWood[0]); }
  c.set(1, base, kWood[1]); c.set(W - 2, base, kWood[0]);
}

void stool(Canvas& c) {
  int cx = c.w / 2, base = c.h - 1;
  line(c, cx - 3, base - 5, cx - 4, base, kWood[2]);
  line(c, cx + 2, base - 5, cx + 3, base, kWood[1]);
  vline(c, cx, base - 4, base - 1, kWood[1]);
  hline(c, cx - 3, cx + 2, base - 2, kWood[1]);
  ellipse(c, cx, base - 6.5f, 4.5f, 2.2f, kWood[3]);
  ellipse(c, cx - 0.5f, base - 7.0f, 3.4f, 1.4f, kWood[4]);
  hline(c, cx - 3, cx + 3, base - 5, kWood[1]);
  c.set(cx - 4, base - 6, kWood[2]);
}

void bench(Canvas& c) {
  int W = c.w, base = c.h - 1;
  // a long plank seat on two trestles; joins seamlessly with its neighbours
  for (int x = 0; x < W; x++) {
    c.set(x, base - 7, kWood[3]); c.set(x, base - 6, kWood[3]); c.set(x, base - 5, kWood[4]);
    c.set(x, base - 4, kWood[1]); c.set(x, base - 3, kWood[0]);
  }
  for (int x = 0; x < W; x += 5) c.set(x, base - 6, kWood[2]);
  for (int lx : {2, W - 4}) { for (int y = base - 2; y <= base; y++) { c.set(lx, y, kWood[2]); c.set(lx + 1, y, kWood[0]); } }
}

void cradle(Canvas& c) {
  int W = c.w, base = c.h - 1;
  // rockers
  for (int x = 1; x < W - 1; x++) {
    float t = (x - 1.0f) / (W - 3) * 2 - 1;
    int y = base - (int)std::lround((1 - t * t) * 0.0f) - (std::fabs(t) > 0.8f ? 1 : 0);
    c.set(x, y, kWood[1]); c.set(x, y - 1, kWood[2]);
  }
  // body: rounded box with spindle sides, a hood at the head end (left)
  for (int y = base - 8; y <= base - 2; y++)
    for (int x = 2; x <= W - 3; x++) {
      int k = (y == base - 8) ? 4 : ((x - 2) % 2 == 0 ? 2 : 3);
      if (y >= base - 3) k = 1;
      if (x == W - 3) k = std::min(k, 1);
      c.set(x, y, kWood[k]);
    }
  for (int y = base - 12; y <= base - 7; y++)
    for (int x = 2; x <= 6; x++) {
      if (y == base - 12 && x == 2) continue;
      c.set(x, y, kWood[y == base - 12 ? 4 : (x == 2 ? 3 : 2)]);
    }
  // blanket and a tiny pillow
  for (int x = 7; x <= W - 4; x++) { c.set(x, base - 9, kBlueCloth[4]); c.set(x, base - 8, kBlueCloth[3]); }
  for (int x = 8; x <= W - 4; x += 3) c.set(x, base - 8, kBlueCloth[2]);
  c.set(6, base - 9, kWhite); c.set(6, base - 8, kCloth[3]);
}

void spinningWheel(Canvas& c) {
  int base = c.h - 1;
  // the bench (slanted), three legs, the big wheel on two uprights, the distaff with wool on the left
  line(c, 2, base - 6, 13, base - 8, kWood[3]); line(c, 2, base - 5, 13, base - 7, kWood[1]);
  line(c, 3, base - 5, 2, base, kWood[2]); line(c, 12, base - 7, 13, base, kWood[1]); line(c, 8, base - 6, 8, base, kWood[1]);
  vline(c, 9, base - 13, base - 8, kWood[2]); vline(c, 11, base - 13, base - 8, kWood[1]);
  float wx = 10.0f, wy = base - 13.0f, r = 6.0f;
  for (int i = 0; i < 6; i++) {
    float a = i / 6.0f * PI;
    line(c, (int)std::lround(wx + std::cos(a) * r), (int)std::lround(wy + std::sin(a) * r), (int)std::lround(wx - std::cos(a) * r), (int)std::lround(wy - std::sin(a) * r), kWood[2]);
  }
  for (int k = 0; k < 64; k++) {
    float a = k / 64.0f * TAU;
    int x = (int)std::lround(wx + std::cos(a) * r - 0.5f), y = (int)std::lround(wy + std::sin(a) * r - 0.5f);
    c.set(x, y, kWood[(std::cos(a) < -0.2f || std::sin(a) < -0.3f) ? 4 : 1]);
  }
  ball(c, wx - 0.5f, wy - 0.5f, 1.2f, 1.2f, kWoodDark);
  vline(c, 3, base - 14, base - 6, kWood[2]);
  ball(c, 3.0f, base - 13.0f, 2.2f, 2.6f, kCloth, 0.10f, 1);
  c.set(2, base - 15, kWhite);
}

void loom(Canvas& c) {
  int W = c.w, base = c.h - 1;
  // two uprights and a top beam against the wall, warp threads, a band of woven cloth, the breast beam and a stool seat
  for (int y = 2; y <= base; y++) { c.set(1, y, kWood[3]); c.set(2, y, kWood[1]); c.set(W - 3, y, kWood[2]); c.set(W - 2, y, kWood[0]); }
  for (int x = 0; x < W; x++) { c.set(x, 2, kWood[4]); c.set(x, 3, kWood[2]); c.set(x, 4, kWood[1]); }
  for (int y = 5; y <= base - 9; y++)
    for (int x = 3; x <= W - 4; x++) {
      if (y < base - 15) { if (x % 2 == 1) c.set(x, y, kCloth[3]); continue; }   // bare warp
      bool weft = (x + y) % 2 == 0;
      int band = (y - (base - 15)) / 2;
      const Ramp& R = band % 3 == 0 ? kRed : (band % 3 == 1 ? kCloth : kBlueCloth);
      c.set(x, y, R[weft ? 3 : 2]);
    }
  for (int x = 0; x < W; x++) { c.set(x, base - 8, kWood[4]); c.set(x, base - 7, kWood[2]); c.set(x, base - 6, kWood[0]); }
  // the shuttle resting on the cloth
  hline(c, 5, 10, base - 10, kWood[4]); c.set(5, base - 10, kWood[2]); c.set(10, base - 10, kWood[1]);
}

void desk(Canvas& c) {
  int W = c.w, base = c.h - 1;
  int top = base - 12;
  // slanted writing top with a paper, inkwell and quill; drawers below; stout legs
  for (int y = top; y < top + 5; y++)
    for (int x = 1; x <= W - 2; x++) c.set(x, y, kWoodDark[y == top + 4 ? 4 : (x == 1 ? 4 : 3)]);
  for (int y = top + 1; y <= top + 3; y++) for (int x = 4; x <= 9; x++) c.set(x, y, y == top + 1 ? kWhite : kCloth[4]);
  hline(c, 5, 8, top + 2, kCloth[2]);
  c.set(11, top + 2, kInk); c.set(12, top + 2, kSoot[2]); c.set(11, top + 1, kSoot[3]);
  line(c, 12, top + 1, 14, top - 3, kWhite); c.set(14, top - 3, kCloth[3]);
  for (int y = top + 5; y <= top + 9; y++)
    for (int x = 1; x <= W - 2; x++) {
      int k = 2;
      if (y == top + 5) k = 1;
      if (x == 1) k = 3;
      if (x == W - 2) k = 1;
      if (x == W / 2) k = 1;
      c.set(x, y, kWoodDark[k]);
    }
  c.set(4, top + 7, kBrass[3]); c.set(11, top + 7, kBrass[3]);
  for (int lx : {1, W - 3}) for (int y = top + 10; y <= base; y++) { c.set(lx, y, kWoodDark[3]); c.set(lx + 1, y, kWoodDark[1]); }
}

void workbench(Canvas& c) {
  int W = c.w, base = c.h - 1;
  int top = base - 12;
  // a heavy top on splayed legs, a vise at the left, a saw and shavings, planks on the lower shelf
  block34(c, 0, top, W, 6, 2, kWood);
  for (int x = 2; x < W - 1; x += 4) c.set(x, top + 2, kWood[2]);
  box(c, 0, top - 2, 2, top + 1, kIron[2]); hline(c, 0, 2, top - 2, kIron[3]); c.set(3, top - 1, kIron[1]);
  for (int x = 6; x <= 13; x++) c.set(x, top + 3, x % 2 ? kIron[2] : kIron[3]);
  box(c, 12, top + 2, 14, top + 3, kWood[1]);
  c.set(5, top + 1, kThatch[4]); c.set(7, top, kThatch[3]); c.set(9, top + 1, kThatch[4]);
  for (int lx : {1, W - 3}) for (int y = top + 8; y <= base; y++) { c.set(lx, y, kWood[2]); c.set(lx + 1, y, kWood[0]); }
  for (int x = 3; x < W - 3; x++) { c.set(x, base - 3, kWood[3]); c.set(x, base - 2, kWood[1]); }
}

void hearth(Canvas& c, int frame) {
  const int W = c.w, H = c.h, base = H - 1;
  const Ramp& S = kStoneWarm;
  const int wallFoot = H - 16;
  // chimney breast rising up the wall to above its top
  for (int y = 0; y <= wallFoot; y++)
    for (int x = 12; x <= 35; x++) {
      int row = y / 4, shift = (row & 1) * 3;
      bool mortar = (y % 4 == 3) || ((x + shift) % 6 == 0);
      int k = mortar ? 1 : 2;
      if (!mortar && hash3((x + shift) / 6, row, 7) % 3 == 0) k = 3;
      if (x == 12) k = 4;
      if (x >= 34) k = std::min(k, 1);
      c.set(x, y, S[k]);
    }
  for (int x = 11; x <= 36; x++) c.set(x, 0, S[4]);
  // firebox surround
  int sTop = wallFoot - 14;
  for (int y = sTop; y <= wallFoot + 2; y++)
    for (int x = 4; x <= 43; x++) {
      int row = (y - sTop) / 4, shift = (row & 1) * 4;
      bool mortar = ((y - sTop) % 4 == 3) || ((x + shift) % 8 == 0);
      int k = mortar ? 1 : 3;
      if (!mortar && hash3((x + shift) / 8, row, 11) % 4 == 0) k = 2;
      if (x == 4) k = 4;
      if (x >= 42) k = std::min(k, 1);
      c.set(x, y, S[k]);
    }
  // mantel shelf with candles and a jug
  for (int x = 2; x <= 45; x++) { c.set(x, sTop - 3, kWood[4]); c.set(x, sTop - 2, kWood[3]); c.set(x, sTop - 1, kWood[1]); }
  c.set(2, sTop - 1, kWood[2]); c.set(45, sTop - 1, kWood[0]);
  candleAt(c, 8, sTop - 4, 4); candleAt(c, 38, sTop - 4, 3);
  jugAt(c, 30, sTop - 4);
  c.set(18, sTop - 4, kPewter[3]); c.set(19, sTop - 4, kPewter[2]); c.set(18, sTop - 5, kPewter[2]);
  // the opening: an arch, a sooty back, the grate, the fire
  int ox0 = 12, ox1 = 35, oTop = sTop + 4;
  for (int y = oTop; y <= wallFoot + 2; y++)
    for (int x = ox0; x <= ox1; x++) {
      float dx = (x + 0.5f - (ox0 + ox1 + 1) * 0.5f) / ((ox1 - ox0 + 1) * 0.5f);
      if (y < oTop + dx * dx * 4.0f) continue;
      float depth = (float)(y - oTop) / (wallFoot + 2 - oTop);
      c.set(x, y, depth < 0.35f ? rgba(36, 22, 28) : rgba(24, 14, 22));
    }
  for (int x = ox0; x <= ox1; x++) {   // lit arch voussoirs
    float dx = (x + 0.5f - (ox0 + ox1 + 1) * 0.5f) / ((ox1 - ox0 + 1) * 0.5f);
    int y = (int)(oTop + dx * dx * 4.0f) - 1;
    c.set(x, y, S[(x / 3) % 2 ? 4 : 3]);
  }
  capsule(c, V(16, wallFoot + 1), V(31, wallFoot), 1.4f, 1.4f, kWood);
  capsule(c, V(19, wallFoot - 1), V(29, wallFoot + 1), 1.2f, 1.2f, kWoodDark);
  static const float hs[4] = {10.0f, 12.0f, 9.5f, 11.5f};
  flame(c, 20.0f, wallFoot - 0.0f, hs[frame] * 0.85f, 7.0f, frame, 23);
  flame(c, 27.0f, wallFoot + 0.0f, hs[(frame + 2) & 3] * 0.8f, 6.5f, frame + 1, 29);
  flame(c, 24.0f, wallFoot + 1.0f, hs[(frame + 1) & 3] * 0.6f, 5.0f, frame + 2, 31);
  for (int x = ox0 + 2; x <= ox1 - 2; x++) c.set(x, wallFoot + 2, kFire[(x + frame) % 3 == 0 ? 3 : 1]);
  // warm firelight on the surround
  for (int y = sTop; y <= wallFoot + 2; y++)
    for (int x = 4; x <= 43; x++) {
      if (x >= ox0 && x <= ox1 && y >= oTop) continue;
      float d = std::hypot((x - 24.0f) / 22.0f, (y - (wallFoot - 2.0f)) / 14.0f);
      if (d < 1.0f) c.set(x, y, mix(c.get(x, y), kFire[2], 0.28f * (1 - d)));
    }
  // raised hearthstone slab in front, then the floor tile
  for (int y = wallFoot + 3; y <= base - 1; y++)
    for (int x = 1; x <= 46; x++) {
      int ly = y - (wallFoot + 3);
      int k = ly == 0 ? 4 : (ly < 9 ? 3 : (ly < 11 ? 2 : 1));
      if ((x + (ly / 5) * 7) % 11 == 0 && ly < 10) k = 2;
      if (ly == 9) k = 4;     // the slab's front lip
      if (ly > 9) k = ly == 10 ? 2 : 1;
      if (x == 1) k = std::min(4, k + 1);
      if (x == 46) k = std::max(0, k - 1);
      float d = std::hypot((x - 24.0f) / 20.0f, ly / 10.0f);
      uint32_t col = S[k];
      if (d < 1.0f && ly < 9) col = mix(col, kFire[2], 0.22f * (1 - d));
      c.set(x, y, col);
    }
  for (int x = 2; x <= 45; x++) c.set(x, base, S[0]);
  // a poker leaning on the side and a few embers on the slab
  line(c, 41, wallFoot - 8, 44, wallFoot + 6, kIron[2]); c.set(41, wallFoot - 9, kIron[4]);
  c.set(22, wallFoot + 4, kFire[3 - (frame & 1)]); c.set(28, wallFoot + 5, kFire[2]);
}

void forge(Canvas& c, int frame) {
  const int W = c.w, H = c.h, base = H - 1;
  const int wallFoot = H - 16;
  const Ramp& S = kBrick;
  // hood and flue up the wall
  for (int y = 0; y <= wallFoot - 10; y++) {
    int half = y < wallFoot - 22 ? 6 : 6 + (y - (wallFoot - 22)) * 1;
    for (int x = 24 - half; x <= 23 + half; x++) {
      int k = x == 24 - half ? 3 : (x >= 22 + half ? 0 : ((x + y) % 7 == 0 ? 1 : 2));
      c.set(x, y, kSoot[k]);
    }
  }
  for (int x = 10; x <= 37; x++) { c.set(x, wallFoot - 9, kIron[3]); c.set(x, wallFoot - 8, kIron[1]); }
  // brick body
  for (int y = wallFoot - 6; y <= base - 1; y++)
    for (int x = 6; x <= 41; x++) {
      int row = y / 3, shift = (row & 1) * 3;
      bool mortar = (y % 3 == 2) || ((x + shift) % 6 == 0);
      int k = mortar ? 1 : 2;
      if (!mortar && hash3((x + shift) / 6, row, 3) % 3 == 0) k = 3;
      if (x == 6) k = 4;
      if (x >= 40) k = std::min(k, 1);
      if (y >= base - 2) k = std::min(k, 1);
      c.set(x, y, S[k]);
    }
  // fire bed: a top rim and glowing coals
  for (int x = 6; x <= 41; x++) { c.set(x, wallFoot - 7, kStone[4]); c.set(x, wallFoot - 6, kStone[3]); }
  for (int y = wallFoot - 5; y <= wallFoot; y++)
    for (int x = 10; x <= 37; x++) {
      uint32_t h = hash3(x / 2, y, 77 + frame);
      int k = (h % 5 == 0) ? 4 : (h % 3 == 0 ? 3 : 1);
      if (y == wallFoot) k = std::min(k, 1);
      c.set(x, y, kFire[k]);
    }
  static const float hs[4] = {7.0f, 8.5f, 6.5f, 8.0f};
  flame(c, 18.0f, wallFoot - 4.0f, hs[frame] * 0.8f, 5.0f, frame, 51);
  flame(c, 28.0f, wallFoot - 4.0f, hs[(frame + 2) & 3] * 0.7f, 5.0f, frame + 1, 53);
  // bellows on the right
  for (int y = wallFoot - 2; y <= wallFoot + 6; y++)
    for (int x = 42; x <= 47; x++) {
      int ly = y - (wallFoot - 2);
      if (x - 42 > ly + 2 || x - 42 > 10 - ly) continue;
      c.set(x, y, kLeather[(ly % 3 == 0) ? 3 : 2]);
    }
  line(c, 42, wallFoot + 2, 38, wallFoot - 1, kWoodDark[2]);
  // a bucket of quench water in front-left
  for (int y = base - 7; y <= base - 1; y++) for (int x = 1; x <= 5; x++) c.set(x, y, kWood[x == 1 ? 3 : (x == 5 ? 1 : 2)]);
  hline(c, 1, 5, base - 5, kIron[2]); hline(c, 2, 4, base - 7, kWater[2]); c.set(2, base - 7, kWater[4]);
}

void filler(Canvas& c) { (void)c; }

// ---- room surfaces ------------------------------------------------------------------------------------
const Ramp& capRamp(art::RoomStyle s) {
  switch (s) {
    case RoomStyle::Stone: case RoomStyle::Hall: return kStoneWarm;
    case RoomStyle::Soot: case RoomStyle::Arcane: return kStone;
    case RoomStyle::Adobe: return kMud;
    default: return kWood;
  }
}

uint32_t floorColor(FloorStyle fs, int gx, int gy) {
  switch (fs) {
    case FloorStyle::Planks: case FloorStyle::OldPlanks: {
      const Ramp& R = fs == FloorStyle::Planks ? kFloorWood : kFloorOld;
      int br = gy >> 2, wy = gy & 3;
      int L = 22 + (int)(hash3(br, 0, 901) % 4) * 6;
      int o = (int)(hash3(br, 1, 903) % (uint32_t)L);
      int s = (gx + o) / L, pos = (gx + o) % L;
      float t = hashf(s, br, 907) * 2 - 1;
      uint32_t col = tone(R, 2, t * 0.45f);
      if (wy == 0) col = tone(R, 3, -0.3f + t * 0.2f);                 // lit top edge of each board
      if (wy == 3) col = R[0];                                         // the gap
      if (pos == 0 && wy != 3) col = R[1];                             // butt joint
      if (pos == 1 && wy != 3) col = mix(col, R[3], 0.35f);
      if ((pos == 2 || pos == L - 2) && wy == 1) col = R[1];           // nail heads
      float grain = vnoise(gx * 0.22f + s * 13.0f, br * 3.0f + wy * 0.35f, 911);
      if (wy != 3 && wy != 0 && grain > 0.68f) col = mix(col, R[1], 0.45f);
      if (fs == FloorStyle::OldPlanks && wy != 3 && hash3(gx, gy, 913) % 23 == 0) col = mix(col, R[1], 0.6f);   // worn spots
      return col;
    }
    case FloorStyle::Flagstone: {
      // irregular stones: a jittered grid, the nearest point owns the pixel; mortar where two owners meet
      const int S = 8;
      int cx = (int)std::floor(gx / (float)S), cy = (int)std::floor(gy / (float)S);
      float best = 1e9f, second = 1e9f;
      int bid = 0;
      for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) {
          int qx = cx + ox, qy = cy + oy;
          float fx = qx * S + 1.5f + hashf(qx, qy, 921) * (S - 3), fy = qy * S + 1.5f + hashf(qx, qy, 923) * (S - 3);
          float d = std::hypot(gx + 0.5f - fx, (gy + 0.5f - fy) * 1.15f);
          if (d < best) { second = best; best = d; bid = (int)hash3(qx, qy, 925); }
          else if (d < second) second = d;
        }
      float t = ((bid & 255) / 255.0f) * 2 - 1;
      uint32_t col = tone(kStoneWarm, 2, t * 0.5f);
      if ((bid >> 8) % 5 == 0) col = mix(col, kStone[2], 0.45f);       // a few cooler slabs
      float e = second - best;
      if (e < 0.8f) return mix(kStoneWarm[1], kStoneWarm[0], 0.35f);
      if (e < 1.6f) col = mix(col, kStoneWarm[1], 0.3f);
      if (hash3(gx, gy, 927) % 17 == 0) col = mix(col, kStoneWarm[1], 0.5f);
      return col;
    }
    case FloorStyle::Terracotta: {
      // square fired-clay tiles (8 px) in lime grout, each a little different; a bevel lit from the top-left
      int cx = gx >> 3, cy = gy >> 3, lx = gx & 7, ly = gy & 7;
      if (lx == 7 || ly == 7) return mix(kLime[1], kTerra[1], 0.35f);
      float t = hashf(cx, cy, 941) * 2 - 1;
      uint32_t col = tone(kTerra, 2, t * 0.55f);
      uint32_t h = hash3(cx, cy, 943);
      if (h % 9 == 0) col = mix(col, kOchre[3], 0.35f);           // a paler, sun-faded tile
      if (h % 13 == 1) col = mix(col, kTerra[1], 0.4f);           // a darker, scorched one
      if (lx == 0 || ly == 0) col = mix(col, kTerra[4], 0.35f);
      else if (lx == 6 || ly == 6) col = mix(col, kTerra[1], 0.3f);
      if (hash3(gx, gy, 945) % 19 == 0) col = mix(col, kTerra[1], 0.45f);   // wear pits
      return col;
    }
    case FloorStyle::Rushes: {
      // old boards strewn with rushes and straw: short stalks scattered over the planks
      uint32_t col = floorColor(FloorStyle::OldPlanks, gx, gy);
      for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) {
          int cx = (gx >> 2) + ox, cy = (gy >> 2) + oy;
          uint32_t h = hash3(cx, cy, 951);
          if (h % 100 >= 11) continue;
          int sx = cx * 4 + (int)(h >> 8) % 4, sy = cy * 4 + (int)(h >> 12) % 4, len = 3 + (int)(h >> 16) % 3, dir = (int)(h >> 20) % 4;
          static const int ddx[4] = {1, 1, 1, 0}, ddy[4] = {0, 1, -1, 1};
          for (int k = 0; k < len; k++)
            if (sx + ddx[dir] * k == gx && sy + ddy[dir] * k == gy)
              return mix(col, (h >> 24) % 3 == 0 ? kThatch[3] : (k == 0 ? kThatch[1] : kThatch[2]), 0.75f);
        }
      return col;
    }
    case FloorStyle::Slab:
    default: {
      // large dressed slabs, offset per row, two alternating stones
      int row = gy >> 4, shift = (row & 1) * 8;
      int sx = (gx + shift) >> 4, lx = (gx + shift) & 15, ly = gy & 15;
      bool alt = ((sx + row) & 1) != 0;
      const Ramp& R = kStoneWarm;
      float t = hashf(sx, row, 931) * 2 - 1;
      uint32_t col = alt ? tone(R, 3, -0.35f + t * 0.15f) : tone(R, 2, 0.25f + t * 0.15f);
      if (lx == 0 || ly == 0) col = R[1];
      else if (lx == 1 || ly == 1) col = mix(col, R[4], 0.35f);
      else if (lx == 15 || ly == 15) col = mix(col, R[1], 0.4f);
      if (hash3(gx, gy, 933) % 29 == 0) col = mix(col, R[1], 0.4f);
      return col;
    }
  }
}

Canvas floorPiece(FloorStyle fs, int mask, int tx, int ty) {
  Canvas c(16, 16);
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      uint32_t col = floorColor(fs, tx * 16 + x, ty * 16 + y);
      // ambient occlusion and the shadows the walls cast (light from the top-left)
      float k = 0;
      if (mask & 1) { static const float f[6] = {0.75f, 0.58f, 0.42f, 0.28f, 0.16f, 0.07f}; if (y < 6) k = std::max(k, f[y]); }
      if (mask & 2) { static const float f[5] = {0.62f, 0.44f, 0.28f, 0.15f, 0.06f}; if (x < 5) k = std::max(k, f[x]); }
      if (mask & 4) { if (x == 15) k = std::max(k, 0.30f); else if (x == 14) k = std::max(k, 0.12f); }
      if (mask & 8) { if (y == 15) k = std::max(k, 0.25f); }
      if (k > 0) col = darken(col, k);
      c.set(x, y, col);
    }
  return c;
}

// the material of a wall face at (gx, row) in a 16x48 back-wall piece (rows 0..4 are the wall top, 47 the foot)
uint32_t wallFaceColor(RoomStyle rs, int gx, int r) {
  switch (rs) {
    case RoomStyle::Timber: {
      if (r <= 7) return r == 5 ? kWoodDark[4] : (r == 7 ? kWoodDark[1] : kWoodDark[2]);   // header beam
      int bay = gx / 48, bx = gx % 48;
      bool post = bx < 4;
      if (r >= 30 && r <= 31) return r == 30 ? kWood[3] : kWood[1];                         // chair rail
      if (r >= 32 && r <= 43) {                                                              // wainscot boards
        int b = gx / 4, wx = gx % 4;
        if (wx == 0) return kWoodDark[1];
        uint32_t col = tone(kWood, 2, (hashf(b, 0, 941) * 2 - 1) * 0.35f);
        if (wx == 1) col = mix(col, kWood[3], 0.5f);
        if (r == 32) col = darken(col, 0.45f);
        if (r == 33) col = darken(col, 0.18f);
        return col;
      }
      if (r >= 44) return r == 47 ? kWoodDark[0] : (r == 44 ? kWoodDark[4] : kWoodDark[2]);   // skirting
      if (post) {
        static const int pk[4] = {3, 2, 2, 1};
        uint32_t col = kWood[pk[bx]];
        if (r == 8 && bx < 3) col = kWood[1];
        if ((r == 11 || r == 26) && bx == 2) col = kWoodDark[0];                              // pegs
        return col;
      }
      // a mid rail and diagonal braces in some bays (timber framing)
      if (r == 19 || r == 20) return r == 19 ? kWood[3] : kWood[1];
      if (hash3(bay, 0, 943) % 2 == 0) {
        bool lower = r > 20;
        int t = lower ? (r - 21) : (r - 8);
        int d = (bx - 4) - (lower ? t : 10 - t) * 2;
        if (hash3(bay, 1, 943) & 1) d = (44 - bx) - (lower ? t : 10 - t) * 2;
        if (d >= 0 && d < 4) return kWood[d == 0 ? 3 : (d == 3 ? 1 : 2)];
      }
      float n = vnoise(gx * 0.3f, r * 0.45f, 945);
      uint32_t col = n > 0.62f ? kPlaster[3] : (n < 0.3f ? tone(kPlaster, 2, -0.25f) : kPlaster[2]);
      if (r == 8 || r == 21) col = kPlaster[1];                                              // under the beams
      if (r == 9 || r == 22) col = mix(col, kPlaster[1], 0.5f);
      if (r == 29) col = mix(col, kPlaster[1], 0.3f);
      if (hash3(gx, r, 947) % 31 == 0) col = kPlaster[1];                                    // pits in the lime
      return col;
    }
    case RoomStyle::Log: {
      if (r >= 44) return r == 47 ? kWoodDark[0] : (r == 44 ? kWoodDark[3] : kWoodDark[2]);
      int lr = (r - 5) / 6, ly = (r - 5) % 6;
      int off = (int)(hash3(lr, 0, 951) % 40);
      float t = (hashf((gx + off) / 40, lr, 953) * 2 - 1) * 0.3f;
      static const int lk[6] = {3, 3, 2, 2, 1, 0};
      uint32_t col = tone(kBark, lk[ly], ly == 5 ? 0 : t);
      if (ly == 0) col = mix(col, kBark[4], 0.3f);
      if (ly == 5) col = mix(kCloth[1], kBark[1], 0.5f);                                     // chinking
      if (ly > 0 && ly < 4 && vnoise(gx * 0.25f, lr * 5.0f + ly * 0.3f, 955) > 0.7f) col = mix(col, kBark[1], 0.5f);
      int knot = (gx + off) % 40;
      if ((knot == 17 || knot == 18) && (ly == 2 || ly == 3) && (lr & 1)) col = kBark[1];
      return col;
    }
    case RoomStyle::Hall: {
      if (r >= 41) {   // plinth
        if (r == 41) return kStoneWarm[4];
        if (r == 47) return kStoneWarm[0];
        return (gx % 24 == 0) ? kStoneWarm[1] : (r == 42 ? kStoneWarm[3] : kStoneWarm[2]);
      }
      if (r >= 9 && r <= 12) {   // carved frieze with dentils
        if (r == 9) return kStoneWarm[4];
        if (r == 12) return kStoneWarm[1];
        return (gx % 4 < 2) ? (r == 10 ? kStoneWarm[3] : kStoneWarm[2]) : kStoneWarm[1];
      }
      int px2 = gx % 64;
      if (px2 < 6) {   // pilaster
        static const int pk[6] = {4, 3, 3, 3, 2, 1};
        return kStoneWarm[r == 13 ? std::max(1, pk[px2] - 1) : pk[px2]];
      }
      int base = r < 9 ? 5 : 13;
      int course = (r - base) / 7, cy = (r - base) % 7;
      int shift = (course & 1) * 8;
      int bx = (gx + shift) % 16;
      if (cy == 6 || bx == 15) return kStoneWarm[1];
      uint32_t col = tone(kStoneWarm, 3, -0.2f + (hashf((gx + shift) / 16, course + base, 961) * 2 - 1) * 0.25f);
      if (cy == 0 || bx == 0) col = mix(col, kStoneWarm[4], 0.4f);
      if (r == 13) col = darken(col, 0.3f);
      return col;
    }
    case RoomStyle::Adobe: {
      // lime-washed mud brick: the ends of the roof beams (vigas) under the ceiling, a soft mottled wash with the
      // bricks showing where it has flaked, an arched niche now and then, and an ochre dado at the foot
      int bay = gx / 24, bx = gx % 24;
      if (r <= 9) {
        uint32_t col = mix(kMud[2], kMud[1], 0.35f + (r - 5) * 0.04f);
        float dx = bx - 11.5f, dy = r - 7.0f;
        if (dx * dx + dy * dy * 1.4f < 9.5f) {                          // a round beam end
          col = kWood[dx < -0.5f && dy < 0.5f ? 3 : 2];
          if (dx * dx + dy * dy * 1.4f < 2.0f) col = kWood[1];
          if (dx > 1.5f || dy > 1.5f) col = kWood[1];
        }
        if (r == 9) col = darken(col, 0.2f);
        return col;
      }
      if (r >= 38) {
        // fix round 3: the dado a deep earth red-brown, clearly darker than the terracotta floor it meets, under a
        // lit lime bead, so a partition's face reads as wall and never as more floor tiles
        if (r == 38) return kLime[4];
        if (r == 39) return mix(kOchre[0], kMud[0], 0.4f);
        if (r >= 46) return r == 47 ? mix(kOchre[0], kVoid, 0.35f) : kOchre[0];
        float n = vnoise(gx * 0.2f, r * 0.5f, 973);
        uint32_t col = n > 0.66f ? kOchre[2] : (n < 0.3f ? kOchre[0] : kOchre[1]);
        return r == 40 ? mix(col, kOchre[0], 0.5f) : col;
      }
      int nb = gx / 80, nx = gx % 80;                                    // niches, one per five tiles
      if (hash3(nb, 0, 975) % 3 != 0 && nx >= 32 && nx <= 46 && r >= 14 && r <= 31) {
        float cx = 39.0f, ax = std::fabs(nx - cx);
        int topY = 14 + (int)(ax * ax / 9.0f);
        if (r >= topY) {
          if (r >= 30) return r == 30 ? kMud[4] : kMud[2];               // the lit sill
          if (nx == 32 || r == topY) return kMud[1];
          if (nx == 46) return kMud[3];
          return mix(kMud[1], kVoid, 0.25f + (r < 20 ? 0.15f : 0.0f));
        }
      }
      float n = vnoise(gx * 0.12f, r * 0.2f, 977) * 0.7f + vnoise(gx * 0.5f, r * 0.6f, 979) * 0.3f;
      uint32_t col = n > 0.62f ? kLime[4] : (n > 0.4f ? kLime[3] : mix(kLime[3], kMud[3], 0.5f));
      if (vnoise(gx * 0.07f + 3.0f, r * 0.11f, 981) > 0.72f) {         // the wash has flaked: mud bricks show
        int course = r / 5, cy = r % 5, shift = (course & 1) * 6;
        int bxx = (gx + shift) % 12;
        col = (cy == 4 || bxx == 11) ? kMud[1] : tone(kMud, 2, (hashf((gx + shift) / 12, course, 983) * 2 - 1) * 0.4f);
        if (cy == 0 && bxx != 11) col = mix(col, kMud[3], 0.5f);
      }
      if (r == 10) col = mix(col, kMud[1], 0.35f);
      if (r == 37) col = mix(col, kOchre[1], 0.45f);
      if (hash3(gx, r, 985) % 29 == 0) col = mix(col, kMud[2], 0.6f);
      return col;
    }
    case RoomStyle::Plaster: {
      // whitewashed plaster over a panelled wood wainscot, a moulded cornice under the ceiling
      if (r <= 8) {
        static const int ck[4] = {4, 2, 3, 1};
        if (r <= 6) return kLime[r == 5 ? 1 : (r == 6 ? 4 : 2)];
        return kLime[ck[(r - 5) & 3]];
      }
      if (r >= 28) {
        if (r == 28) return kWood[4];
        if (r == 29) return kWood[2];
        if (r == 30) return kWoodDark[1];
        if (r >= 44) return r == 47 ? kWoodDark[0] : (r == 44 ? kWood[3] : kWood[1]);
        int px2 = gx % 16, ly = r - 31;                                  // raised panels between stiles and rails
        if (px2 <= 1 || ly <= 1 || ly >= 11) {
          int k = px2 == 0 ? 3 : (ly == 0 ? 3 : 2);
          if (ly == 12) k = 1;
          return kWood[k];
        }
        int ix = px2 - 2, iy = ly - 2;
        if (iy == 0 || ix == 0) return kWood[1];
        if (iy == 8 || ix == 13) return kWood[3];
        if (iy == 1 || ix == 1) return kWood[3];
        return tone(kWood, 2, (hashf(gx / 16, 0, 987) * 2 - 1) * 0.25f);
      }
      float n = vnoise(gx * 0.1f, r * 0.16f, 989);
      uint32_t col = n > 0.6f ? kLime[4] : (n < 0.32f ? kLime[2] : kLime[3]);
      if (r == 9) col = kLime[1];
      if (r == 10) col = mix(col, kLime[1], 0.5f);
      if (r == 27) col = mix(col, kLime[1], 0.4f);
      if (hash3(gx, r, 991) % 37 == 0) col = kLime[2];
      return col;
    }
    case RoomStyle::Stone: case RoomStyle::Soot: case RoomStyle::Arcane: default: {
      const Ramp& R = rs == RoomStyle::Stone ? kStoneWarm : kStone;
      if (rs == RoomStyle::Stone && r >= 32) {   // a wooden wainscot keeps a stone home warm
        if (r == 32) return kWoodDark[3];
        if (r >= 44) return r == 47 ? kWoodDark[0] : (r == 44 ? kWoodDark[4] : kWoodDark[2]);
        int wx = gx % 5;
        uint32_t col = tone(kWood, 2, (hashf(gx / 5, 1, 965) * 2 - 1) * 0.3f);
        if (wx == 0) col = kWoodDark[1];
        if (wx == 1) col = mix(col, kWood[3], 0.45f);
        if (r == 33) col = darken(col, 0.35f);
        return col;
      }
      if (r >= 45) return r == 47 ? R[0] : R[1];
      int course = (r - 5) / 6, cy = (r - 5) % 6;
      const int ch = 6;
      int len = 10 + (int)(hash3(course, 0, 967) % 3) * 2;
      int shift = (int)(hash3(course, 1, 969) % (uint32_t)len);
      int bx = (gx + shift) % len;
      int bid = (gx + shift) / len;
      if (cy == ch - 1 || bx == len - 1) return mix(R[0], R[1], 0.5f);
      uint32_t col = tone(R, 2, (hashf(bid, course, 971) * 2 - 1) * 0.45f);
      if (cy == 0 || bx == 0) col = mix(col, R[4], 0.45f);
      else if (cy == ch - 2 || bx == len - 2) col = mix(col, R[1], 0.4f);
      if (rs == RoomStyle::Soot) col = darken(col, std::max(0.0f, 0.6f - (r - 5) * 0.018f));
      if (rs == RoomStyle::Arcane && course == 3 && cy == 2 && (gx % 6) < 2) col = mix(col, kCrystal[3], 0.6f);   // a rune band
      return col;
    }
  }
}

// is (x, y) on the wall top (cap)? the room is around the tile per the mask; outside the tile the strips continue
bool capAt(int mask, int x, int y) {
  int cx = std::clamp(x, 0, 15), cy = std::clamp(y, 0, 15);
  bool e = (mask & CapE) && cx >= 10, w = (mask & CapW) && cx <= 5, n = (mask & CapN) && cy <= 4, s = (mask & CapS) && cy >= 11;
  bool ne = (mask & CapNE) && cx >= 10 && cy <= 4, nw = (mask & CapNW) && cx <= 5 && cy <= 4;
  bool se = (mask & CapSE) && cx >= 10 && cy >= 11, sw = (mask & CapSW) && cx <= 5 && cy >= 11;
  return e || w || n || s || ne || nw || se || sw;
}
bool roomPx(int mask, int x, int y) {
  if (x >= 16) return y < 0 ? (mask & CapNE) != 0 : (y >= 16 ? (mask & CapSE) != 0 : (mask & CapE) != 0);
  if (x < 0) return y < 0 ? (mask & CapNW) != 0 : (y >= 16 ? (mask & CapSW) != 0 : (mask & CapW) != 0);
  if (y < 0) return (mask & CapN) != 0;
  if (y >= 16) return (mask & CapS) != 0;
  return false;
}
// the top of a partition: a section through the wall, a step darker than the room's floor so it reads as wall
uint32_t partTop(RoomStyle rs, int x, int y, bool ns, int seed) {
  const Ramp& R = capRamp(rs);
  int a = ns ? y : x, b = ns ? x : y;   // a runs along the wall, b across it
  switch (rs) {
    case RoomStyle::Timber: case RoomStyle::Plaster: {
      // M0b fix round: the wall in section: the pale lime-plaster infill between the two oak plates that carry it, so
      // a wall top reads as wall at a glance against the boards of the floor (never as more floor boards)
      if (b <= 2 || b >= 13) {   // the plates: lit along their outer (top-left) edge
        int k = (b == 1 || b == 13) ? 3 : 2;
        if (b == 0 || b == 15) k = 1;
        // fix round 2: the light comes from the top-left: the west / north plate a step lighter, the other a step darker
        if (b <= 2 && b > 0) k = std::min(4, k + 1);
        if (b >= 13) k = std::max(1, k - 1);
        uint32_t col = tone(kWood, k, (vnoise(a * 0.2f + seed * 3.1f, b * 0.5f, 981) - 0.5f) * 0.4f);
        if ((a + seed * 5) % 23 == 0 && (b == 1 || b == 14)) col = kWoodDark[1];   // pegs
        return col;
      }
      float n = vnoise(a * 0.21f + seed * 2.7f, b * 0.33f, 983);
      uint32_t col = n > 0.62f ? kLime[4] : (n < 0.3f ? kLime[2] : kLime[3]);
      if (b == 3) col = mix(col, kWoodDark[1], 0.45f);   // the plaster sits a hair below the plates
      if (b == 12) col = mix(col, kLime[1], 0.5f);
      if (hash3(a, b + seed * 16, 985) % 29 == 0) col = kLime[1];
      return col;
    }
    case RoomStyle::Log: {
      // M0b fix round: the top of the log wall: one round log seen from above, a lit ridge along its crown falling
      // into shade on both flanks, its bark split here and there; chinking where it beds on the log below
      float u = (b - 7.5f) / 7.5f;   // -1 .. 1 across the log
      int k = u < -0.65f ? 2 : (u < -0.15f ? 4 : (u < 0.35f ? 3 : (u < 0.75f ? 2 : 1)));
      uint32_t col = tone(kBark, k, (vnoise(a * 0.18f + seed * 3.3f, b * 0.4f, 987) - 0.5f) * 0.35f);
      // fix round 2: the wall tops a clear step lighter than the rush-strewn boards (adzed, weathered pale crown), so
      // the rooms read at 1x in a log house
      col = mix(col, kWood[4], k >= 3 ? 0.34f : 0.18f);
      if (b == 0 || b == 15) col = mix(kCloth[1], kBark[1], 0.5f);
      if ((a + seed * 7) % 17 == 0 && b >= 4 && b <= 11) col = kBark[1];                    // a split in the bark
      if ((a + seed * 7) % 31 == 3 && b >= 5 && b <= 9) col = mix(kBark[4], kWood[4], 0.5f);   // a pale knot
      return col;
    }
    case RoomStyle::Adobe: {
      // fix round 3: the mud wall's rounded top in section: a lit west / north shoulder, a pale crown, the far
      // shoulder falling into shade, so the top reads as a solid mass (it was one flat tan, read as a floor runner).
      // Straw flecks and small pits in the render.
      float u = (b - 7.5f) / 7.5f;
      int k = u < -0.72f ? 3 : (u < -0.3f ? 4 : (u < 0.25f ? 3 : (u < 0.65f ? 2 : 1)));
      float t = vnoise(a * 0.22f + seed * 2.3f, b * 0.4f, 995);
      uint32_t col = tone(kMud, k, (t - 0.5f) * 0.35f);
      if ((a * 7 + b * 3 + seed * 11) % 37 == 0 && k >= 2) col = mix(col, kOchre[4], 0.5f);    // straw
      if (hash3(a, b + seed * 16, 996) % 41 == 0 && k >= 2) col = mix(col, kMud[1], 0.5f);   // a pit in the render
      return col;
    }
    default: {   // coping stones laid across the wall
      int len = 8, sh = (seed * 3) % len;
      int blk = (a + sh) / len, ba = (a + sh) % len;
      if (ba == len - 1) return R[0];
      uint32_t col = mix(tone(R, 2, (hashf(blk, seed, 997) * 2 - 1) * 0.35f), R[1], rs == RoomStyle::Hall ? 0.5f : 0.3f);
      if (ba == 0) col = mix(col, R[3], 0.45f);
      if (hash3(x, y + seed * 16, 999) % 23 == 0) col = mix(col, R[1], 0.5f);
      return col;
    }
  }
}

// the wall top of the shell (and the back wall's top rows): the same section as a partition's top
uint32_t capSurface(RoomStyle rs, int x, int y, bool along, int seed) { return partTop(rs, x, y, along, seed & 31); }

Canvas capPiece(RoomStyle rs, int mask, int seed, int topFlags, int part) {
  Canvas c(16, 16);
  const Ramp& R = capRamp(rs);
  // a neighbouring back wall's top (its rows 0..4) counts as cap, so the side strip joins it without a seam; so does a
  // partition's top next to the shell (part: 1 E, 2 W, 4 N)
  auto cap = [&](int x, int y) {
    if ((part & 1) && x >= 16 && y >= 0 && y < 16) return true;
    if ((part & 2) && x < 0 && y >= 0 && y < 16) return true;
    if ((part & 4) && y < 0 && x >= 0 && x < 16) return true;
    if (y < 0 && (topFlags & 4)) return false;
    if (x >= 16 && y >= 0 && y <= 4 && (topFlags & 1)) return true;
    if (x < 0 && y >= 0 && y <= 4 && (topFlags & 2)) return true;
    if (x >= 0 && x < 16 && y >= 0 && y < 16) return capAt(mask, x, y);
    if (roomPx(mask, x, y)) return false;
    return capAt(mask, x, y);
  };
  auto room = [&](int x, int y) { return !cap(x, y) && roomPx(mask, x, y); };
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      if (!cap(x, y)) { c.set(x, y, kVoid); continue; }
      bool along = ((mask & (CapE | CapW)) && (x >= 10 || x <= 5)) && !((mask & (CapN | CapS)) && (y <= 4 || y >= 11));
      uint32_t col = capSurface(rs, x, y, along, seed);
      // edges: room to the east/south -> a dark drop; room to the west/north -> a lit edge; void -> rim light up-left, shade down-right
      if (room(x + 1, y) || room(x, y + 1)) col = R[1];
      else if (room(x - 1, y) || room(x, y - 1)) col = R[4];
      else if (!cap(x - 1, y) || !cap(x, y - 1)) col = R[4];
      else if (!cap(x + 1, y) || !cap(x, y + 1)) col = R[1];
      c.set(x, y, col);
    }
  return c;
}

Canvas backWallPiece(RoomStyle rs, int ends, int tx) {
  Canvas c(16, 48);
  const Ramp& R = capRamp(rs);
  for (int y = 0; y < 48; y++)
    for (int x = 0; x < 16; x++) {
      int gx = tx * 16 + x;
      uint32_t col;
      if (y <= 4) {   // the top of the wall, seen from above
        col = capSurface(rs, x, y, false, tx);
        if (y == 0) col = R[4];
        if (y == 4) col = R[1];
      } else {
        col = wallFaceColor(rs, gx, y);
        float k = 0;
        if (ends & 16) { static const float f[6] = {0.55f, 0.42f, 0.3f, 0.2f, 0.11f, 0.05f}; if (x < 6) k = f[x]; }
        if (ends & 32) { if (x == 15) k = 0.25f; else if (x == 14) k = 0.1f; }
        // M0b: a low partition joins the face's foot at the left (64: its shadow falls on the face) or right (128)
        if ((ends & 64) && y >= 32) { static const float f[5] = {0.42f, 0.3f, 0.2f, 0.11f, 0.05f}; if (x < 5) k = std::max(k, f[x] * (y >= 34 ? 1.0f : 0.6f)); }
        if ((ends & 128) && y >= 32 && x == 15) k = std::max(k, 0.2f);
        if (y == 5) k = std::max(k, 0.15f);
        if (k > 0) col = darken(col, k);
        if (rs == RoomStyle::Log && y >= 44) {   // fix round 2: the fieldstone sill course the partitions share
          int course = (gx + (y >= 46 ? 3 : 0)) % 7;
          uint32_t st = tone(kStone, y == 44 ? 3 : 2, (hashf(gx / 7, (y - 32) / 2, 1013) * 2 - 1) * 0.3f);
          if (course == 0 || y == 45) st = kStone[1];
          if (y == 47) st = kStone[0];
          if (k > 0) st = darken(st, k);
          col = st;
        }
      }
      c.set(x, y, col);
    }
  return c;
}

Canvas doorPiece(RoomStyle rs) {
  Canvas c(16, 16);
  const Ramp& R = rs == RoomStyle::Timber || rs == RoomStyle::Log ? kWood : kStoneWarm;
  for (int x = 0; x < 16; x++) {
    c.set(x, 0, R[1]); c.set(x, 1, R[4]); c.set(x, 2, R[3]); c.set(x, 3, R[2]); c.set(x, 4, R[0]);
  }
  for (int y = 5; y < 16; y++) {
    int a = 40 + (y - 5) * 16;
    for (int x = 0; x < 16; x++) {
      int aa = a;
      if (x < 2 || x > 13) aa = std::min(255, aa + 50);
      c.set(x, y, withA(kVoid, std::min(235, aa)));
    }
  }
  return c;
}

// ---- M0b partitions: cut-away walls one tile high (rpg/sim/rooms.h). The face is the foot of the room's back wall
//      (its last 16 rows), so a partition reads as the same wall cut down; the top is the wall's section seen from
//      above, lit on its top-left rims, with a bright lip where it turns down into its own face.
uint32_t edgeInk(const Ramp& R) { return mix(R[0], kInk, 0.45f); }

Canvas partFacePiece(RoomStyle rs, int endL, int endR, int tx, int ty) {
  Canvas c(16, 16);
  const Ramp& R = capRamp(rs);
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      int gx = tx * 16 + x;
      uint32_t col = wallFaceColor(rs, gx, 32 + y);
      float k = 0;
      if (rs == RoomStyle::Hall) k = 0.16f;  // pale dressed stone: a touch of shade so the wall stands off the slabs
      if (rs == RoomStyle::Log) k = 0.22f;   // fix round 2: log faces a step darker than the boards and the tops
      if (y == 0) k = 0.42f;                 // the lip of the top casts a hairline shadow
      else if (y == 1) k = 0.14f;
      if (endL == PartEndWall) { static const float f[5] = {0.5f, 0.36f, 0.24f, 0.13f, 0.05f}; if (x < 5) k = std::max(k, f[x]); }
      if (endR == PartEndWall) { if (x == 15) k = std::max(k, 0.26f); else if (x == 14) k = std::max(k, 0.1f); }
      if (k > 0) col = darken(col, k);
      if (rs == RoomStyle::Log && y >= 12) {   // the logs sit on a fieldstone sill course
        int course = (gx + (y >= 14 ? 3 : 0)) % 7;
        uint32_t st = tone(kStone, y == 12 ? 3 : 2, (hashf(gx / 7, y / 2, 1013) * 2 - 1) * 0.3f);
        if (course == 0 || y == 13) st = kStone[1];
        if (y == 15) st = kStone[0];
        col = st;
      }
      if (endL == PartEndFree) { if (x == 0) col = edgeInk(R); else if (x == 1) col = lighten(col, 0.4f); }
      if (endR == PartEndFree) { if (x == 15) col = edgeInk(R); else if (x == 14) col = darken(col, 0.3f); }
      if (y == 15 && (endL == PartEndFree || endR == PartEndFree)) col = darken(col, 0.2f);
      c.set(x, y, col);
    }
  (void)ty;
  return c;
}

Canvas partCapPiece(RoomStyle rs, int seed, int open, int faces) {
  Canvas c(16, 16);
  const Ramp& R = capRamp(rs);
  auto dirBit = [](int ox, int oy) {
    if (ox == 0 && oy == 0) return 0;
    if (oy == 0) return ox > 0 ? (int)CapE : (int)CapW;
    if (ox == 0) return oy > 0 ? (int)CapS : (int)CapN;
    if (oy < 0) return ox > 0 ? (int)CapNE : (int)CapNW;
    return ox > 0 ? (int)CapSE : (int)CapSW;
  };
  // what lies at pixel (x, y) of the 3x3 tile block around this one: 0 wall top, 1 floor, 2 a face
  auto at = [&](int x, int y) {
    int ox = x < 0 ? -1 : (x >= 16 ? 1 : 0), oy = y < 0 ? -1 : (y >= 16 ? 1 : 0);
    int b = dirBit(ox, oy);
    if (!b || !(open & b)) return 0;
    return (faces & b) ? 2 : 1;
  };
  // a run going north-south: open east or west, and nothing but faces (the back wall behind, a face it meets) north
  // and south. (Fix round 2: a face counted as "open" turned the run's first tiles into E-W pieces with plates across
  // them, so the top of every N-S partition began with two separate blocks.)
  bool ns = (open & (CapE | CapW)) && !((open & ~faces) & (CapN | CapS));
  const uint32_t ink = edgeInk(R);
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      uint32_t col = partTop(rs, x, y, ns, seed);
      if (rs == RoomStyle::Adobe) {}   // the rounded mud top carries its own light and shade (partTop)
      else if (ns && x >= 12) col = darken(col, x >= 14 ? 0.16f : 0.08f);   // the run's east side falls into shade
      // edges, nearest first
      int s1 = at(x, y + 1), n1 = at(x, y - 1), w1 = at(x - 1, y), e1 = at(x + 1, y);
      if (w1) col = ink;
      else if (e1) col = ink;
      else if (s1 == 2) col = R[4];                        // the lip where the top turns down into its face
      else if (s1 == 1) col = ink;
      else if (n1 == 2) col = mix(R[0], kVoid, 0.3f);      // the foot of the tall face behind
      else if (n1 == 1) col = ink;
      else if (at(x - 1, y - 1) || at(x + 1, y - 1) || at(x - 1, y + 1) || at(x + 1, y + 1)) {
        int dg = at(x - 1, y + 1) ? at(x - 1, y + 1) : (at(x + 1, y + 1) ? at(x + 1, y + 1) : 0);
        col = (dg == 2 && !at(x - 1, y - 1) && !at(x + 1, y - 1)) ? R[4] : ink;
      } else {
        int s2 = at(x, y + 2), n2 = at(x, y - 2), w2 = at(x - 2, y), e2 = at(x + 2, y);
        if (w2 || n2 == 1) col = mix(col, R[4], 0.7f);    // the lit rim on the top-left
        else if (e2) col = mix(col, R[1], 0.6f);          // the shaded rim on the right
        else if (s2 == 2) col = mix(col, R[4], 0.35f);
        else if (n2 == 2) col = darken(col, 0.3f);
      }
      c.set(x, y, col);
    }
  return c;
}

// the threshold of an interior doorway: worn boards (or a stone sill) across the wall's line
Canvas sillPiece(RoomStyle rs, int vertical) {
  Canvas c(16, 16);
  bool stone = rs == RoomStyle::Stone || rs == RoomStyle::Hall || rs == RoomStyle::Soot || rs == RoomStyle::Arcane || rs == RoomStyle::Adobe;
  const Ramp& R = stone ? (rs == RoomStyle::Adobe ? kMud : kStoneWarm) : kWoodDark;
  if (!vertical) {
    // a sill board across the doorway at the wall's foot, the passage floor shaded by the jambs either side
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 16; x++) {
        int a = 0;
        if (x <= 2 || x >= 13) a = x <= 2 ? 90 - x * 25 : 40 + (x - 13) * 20;
        if (y >= 10 && y <= 14 && x >= 1 && x <= 14) {
          int k = y == 10 ? 4 : (y == 11 ? 3 : (y == 14 ? 0 : 2));
          if (x == 1) k = std::min(4, k + 1);
          if (x == 14) k = std::max(0, k - 1);
          uint32_t col = R[k];
          if (!stone && y > 10 && y < 14 && (x == 5 || x == 10)) col = R[1];
          c.set(x, y, col);
          continue;
        }
        if (y == 15 && x >= 1 && x <= 14) { c.set(x, y, withA(kShadowCol, 110)); continue; }
        if (a > 0) c.set(x, y, withA(kShadowCol, a));
      }
  } else {
    // fix round 3: the threshold of an N-S doorway spans the wall's full width (the casing rails stand on its edges),
    // so the wall's line runs on unbroken under the door: worn boards running along the wall (a stone sill in
    // masonry rooms), lit at the top-left, a little darker where feet have worn it in the middle
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 16; x++) {
        int k = 2;
        if (x == 0 || y == 0) k = 3;
        if (x == 15 || y == 15) k = 1;
        if (!stone && (x == 5 || x == 10)) k = std::max(0, k - 1);   // board seams
        if (stone && y == 7) k = 1;                                    // the joint between two sill stones
        uint32_t col = R[k];
        if (x >= 6 && x <= 9 && y >= 3 && y <= 12) col = darken(col, 0.10f);   // worn in the middle
        c.set(x, y, col);
      }
  }
  return c;
}

// ---- M0b stairs and interior doors, painted in the room's material (Piece::Styled; the prop sprites are the timber
//      look). Same canvases and anchors as the props.
struct DoorLook {
  const Ramp* frame;   // jambs, lintel, stringers
  const Ramp* leaf;    // the door leaf
  const Ramp* strap;   // hinges and straps
  bool stone;          // stone stairs and a masonry frame
};
DoorLook doorLook(RoomStyle rs) {
  switch (rs) {
    case RoomStyle::Log: return {&kBark, &kWood, &kIron, false};
    case RoomStyle::Stone: return {&kStoneWarm, &kWood, &kIron, true};
    case RoomStyle::Hall: return {&kStoneWarm, &kWoodDark, &kIron, true};
    case RoomStyle::Soot: return {&kStone, &kWoodDark, &kIron, true};
    case RoomStyle::Arcane: return {&kStone, &kPurple, &kBrass, true};
    case RoomStyle::Adobe: return {&kMud, &kBlueCloth, &kIron, true};
    case RoomStyle::Plaster: return {&kWood, &kPaint, &kBrass, false};
    default: return {&kWood, &kWood, &kIron, false};
  }
}

// M0b fix round: a flight is two tiles wide (32 px) where the plan fits it; the two tiles are painted as the halves of
// one 32 px flight (variant 2: left half, 4: right half, neither: the narrow one-tile flight of the plain fallback).
// The runner down the middle of a dressed flight: red wool in inns, homes and halls, none on bare work stairs.
const Ramp* stairRunner(RoomStyle rs) {
  switch (rs) {
    case RoomStyle::Timber: case RoomStyle::Plaster: case RoomStyle::Hall: return &kRed;
    case RoomStyle::Stone: return &kBlueCloth;
    case RoomStyle::Arcane: return &kPurple;
    default: return nullptr;
  }
}
// copies columns [x0, x0 + c.w) of the full painting into c
void sliceInto(Canvas& c, const Canvas& full, int x0) {
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) c.set(x, y, full.get(x0 + x, y));
}

// a flight climbing north into the wall face above its tile (16x40 per tile; the tile is canvas rows 24..39).
// variant & 1: under a partition's short face (the flight and its opening stay below the partition's top).
// Read at 1x as stairs, not a ladder: a wide flight between solid stringers, every step a lit tread and nosing over a
// riser in the shade of the step above, a warm light falling down the flight from the landing above, a balustrade
// with a turned newel post at the foot on each open side, the runner held by brass rods.
void stairsUpPaint(Canvas& c, RoomStyle rs, int variant) {
  const int half = variant & 2 ? 0 : (variant & 4 ? 1 : -1);
  const int Wd = half >= 0 ? 32 : 16;
  Canvas f(Wd, c.h);
  const DoorLook L = doorLook(rs);
  const Ramp& F = *L.frame;
  const Ramp& T = L.stone ? F : kWood;
  const Ramp& D = L.stone ? F : kWoodDark;
  const Ramp* run = stairRunner(rs);
  const int H = f.h, top = (variant & 1) ? 9 : 0;   // the lintel's top row
  const int sx0 = 4, sx1 = Wd - 5;                   // the treads, between the stringers
  const int oy0 = top + 3, oy1 = top + ((variant & 1) ? 8 : 12);   // the opening under the lintel
  // the opening the flight climbs into: dark, with the lamplight of the floor above caught on its top steps
  for (int y = oy0; y <= oy1 + 4; y++)
    for (int x = 2; x < Wd - 2; x++) {
      float t = (float)(y - oy0) / (float)std::max(1, oy1 - oy0);
      uint32_t col = mix(kVoid, D[0], 0.25f + 0.2f * t);
      if (y == oy0 || y == oy0 + 1) col = mix(col, rgba(196, 140, 78), y == oy0 ? 0.45f : 0.25f);   // light from upstairs
      f.set(x, y, col);
    }
  // the frame: a heavy lintel (lit top, dark underside) on two jambs
  for (int x = 0; x < Wd; x++) {
    f.set(x, top, F[4]); f.set(x, top + 1, F[3]); f.set(x, top + 2, F[1]);
    if (L.stone && x % 8 == 4) { f.set(x, top + 1, F[1]); }
  }
  for (int y = top + 3; y < H; y++) {
    f.set(0, y, D[0]); f.set(1, y, F[3]);
    f.set(Wd - 2, y, F[1]); f.set(Wd - 1, y, D[0]);
  }
  // the steps, from the floor up into the opening: nosing, tread, riser (5 rows a step); the risers sit in the shade of
  // the step above, the treads catch the light; dimmer toward the opening
  for (int k = 0; k < 12; k++) {
    int yb = H - 1 - k * 5;
    for (int r = 0; r < 5; r++) {
      int y = yb - r;
      if (y < oy0 + 2) continue;
      static const int kk[5] = {0, 1, 1, 3, 4};      // riser foot, riser, riser top, tread, nosing
      for (int x = sx0; x <= sx1; x++) {
        int k2 = kk[r];
        if (x == sx0 && r >= 3) k2 = std::min(4, k2 + 1);
        if (x >= sx1 - 1 && r >= 3) k2 = std::max(0, k2 - 1);
        uint32_t col = r <= 2 ? D[k2 + 1] : T[k2];
        if (L.stone && r <= 2 && ((x + k * 5) % 9 == 0)) col = F[0];   // block joints
        if (run && Wd >= 32 && x >= Wd / 2 - 6 && x <= Wd / 2 + 5) {
          int rk = r >= 3 ? (r == 4 ? 4 : 3) : (r == 0 ? 0 : 1);
          if (x == Wd / 2 - 6 && r >= 3) rk = 4;
          if (x == Wd / 2 + 5) rk = std::max(0, rk - 1);
          col = (*run)[rk];
          if (r == 3 && (x == Wd / 2 - 6 || x == Wd / 2 + 5)) col = kGold[4];   // brass stair rods
        }
        float dim = y < oy1 + 10 ? (oy1 + 10 - y) * 0.075f : 0.0f;
        if (dim > 0) col = darken(col, std::min(0.75f, dim));
        f.set(x, y, col);
      }
    }
  }
  // the stringers: a lit board on the left, a shaded one on the right, a dark seam against the treads
  for (int y = oy0 + 2; y < H; y++) {
    float dim = y < oy1 + 6 ? (oy1 + 6 - y) * 0.1f : 0.0f;
    auto put = [&](int x, uint32_t col) { f.set(x, y, dim > 0 ? darken(col, std::min(0.8f, dim)) : col); };
    put(2, T[4]); put(3, D[1]);
    put(Wd - 4, D[0]); put(Wd - 3, T[1]);
  }
  // a balustrade down both outer sides: a handrail (lit top) over the stringer on turned balusters, and a newel post
  // at the foot rising above the bottom step, capped
  const Ramp& Rr = (L.stone && rs != RoomStyle::Adobe) ? kIron : kWood;
  const Ramp& Rd = (L.stone && rs != RoomStyle::Adobe) ? kIron : kWoodDark;
  for (int side = 0; side < 2; side++) {
    int x = side ? Wd - 3 : 2;
    int o = side ? 1 : -1;   // the outer side
    for (int y = oy1 + 2; y <= H - 20; y++) { f.set(x, y, Rr[side ? 2 : 4]); f.set(x + o, y, Rd[side ? 0 : 1]); }
    for (int y = oy1 + 5; y <= H - 20; y += 4) f.set(x, y, Rd[2]);   // the balusters' turnings
    for (int y = H - 19; y < H; y++) {
      f.set(x - 1, y, Rr[side ? 2 : 4]); f.set(x, y, Rr[side ? 1 : 3]); f.set(x + 1, y, Rd[side ? 0 : 2]);
    }
    for (int y = H - 15; y < H; y += 5) { f.set(x - 1, y, Rr[side ? 1 : 3]); f.set(x + 1, y, Rd[0]); }   // turnings
    f.set(x - 1, H - 20, Rr[4]); f.set(x, H - 20, Rr[4]); f.set(x + 1, H - 20, Rr[2]);   // the cap
    f.set(x, H - 21, Rr[4]);
    f.set(x - 1, H - 1, Rd[0]); f.set(x, H - 1, Rd[0]); f.set(x + 1, H - 1, Rd[0]);
  }
  sliceInto(c, f, half == 1 ? 16 : 0);
}

// the head of a flight going down: a stairwell opening in the floor with a balustrade round its far and outer sides,
// open toward the south where you step down (16x24 per tile; the tile is canvas rows 8..23)
void stairsDownPaint(Canvas& c, RoomStyle rs, int variant) {
  const int half = variant & 2 ? 0 : (variant & 4 ? 1 : -1);
  const int Wd = half >= 0 ? 32 : 16;
  Canvas f(Wd, c.h);
  const DoorLook L = doorLook(rs);
  const Ramp& F = *L.frame;
  const Ramp& T = L.stone ? F : kWood;
  const Ramp* run = stairRunner(rs);
  const int x0 = 3, x1 = Wd - 4, y0 = 9, y1 = 23;
  // the well: the top step at the near (south) edge, steps going down and away (north) into the floor below's
  // lamplight, each nosing lit
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      int d = y1 - y;
      int step = d / 3, r = d % 3;
      int k = r == 0 ? 4 : (r == 1 ? 3 : 1);
      if (x == x0) k = std::min(4, k + 1);
      if (x == x1) k = std::max(0, k - 2);
      uint32_t col = T[k];
      if (run && Wd >= 32 && x >= Wd / 2 - 6 && x <= Wd / 2 + 5) col = (*run)[r == 0 ? 3 : (r == 1 ? 2 : 1)];
      col = darken(col, std::min(0.9f, 0.1f + step * 0.17f));
      if (d >= 12) col = mix(mix(kVoid, T[0], 0.3f), rgba(120, 80, 50), 0.25f);
      f.set(x, y, col);
    }
  // the stringers down either side, in the well's shade
  for (int y = y0; y <= y1; y++) {
    f.set(x0 - 1, y, darken(F[2], 0.3f)); f.set(x0 - 2, y, darken(F[3], 0.2f));
    f.set(x1 + 1, y, darken(F[1], 0.45f)); f.set(x1 + 2, y, darken(F[1], 0.3f));
  }
  // the cut floor edge: a lit lip along the near side, a shaded one along the far side
  for (int x = x0 - 2; x <= x1 + 2; x++) { f.set(x, y0 - 1, F[1]); f.set(x, y0, darken(F[2], 0.25f)); }
  // the balustrade: posts at the far corners and at the near ends, a rail 7 px up along the far side and down both
  // outer sides, balusters between
  const Ramp& P = (L.stone && rs != RoomStyle::Adobe) ? kIron : kWood;
  const int xl = x0 - 2, xr = x1 + 2;
  for (int x = xl; x <= xr; x++) {   // the far rail, its top lit, with its balusters to the floor edge
    f.set(x, y0 - 8, P[4]); f.set(x, y0 - 7, P[2]);
    if ((x - xl) % 3 == 1) for (int y = y0 - 6; y <= y0 - 2; y++) f.set(x, y, P[(x - xl) % 6 == 1 ? 3 : 1]);
  }
  for (int side = 0; side < 2; side++) {
    int x = side ? xr : xl;
    for (int y = y0 - 7; y <= y1 - 6; y++) { f.set(x, y, P[side ? 2 : 4]); f.set(x + (side ? -1 : 1), y, P[side ? 1 : 3]); }
    for (int y = y0 + 1; y <= y1; y += 3) for (int yy = y - 5; yy <= y; yy++) f.set(x, yy, P[side ? 1 : 3]);   // balusters
    for (int yy : {y0 - 1, y1 + 1}) {   // newel posts at the far corner and at the near end
      int yb = std::min(yy, f.h - 1);
      for (int y = yb - 9; y <= yb; y++) { f.set(x, y, P[side ? 2 : 3]); f.set(x + (side ? 1 : -1), y, P[side ? 0 : 2]); }
      f.set(x, yb - 10, P[4]); f.set(x + (side ? 1 : -1), yb - 10, P[3]);
    }
  }
  sliceInto(c, f, half == 1 ? 16 : 0);
}

// an interior door in an E-W partition (16x36: the face row is canvas rows 20..35, the cap row 4..19). The frame
// stands in the doorway; its lintel spans the top of the cap row. open: the leaf stands open, swung in on its hinge;
// closed: the leaf fills the opening.
void doorHPaint(Canvas& c, RoomStyle rs, bool open) {
  const DoorLook L = doorLook(rs);
  const Ramp& F = *L.frame;
  const Ramp& Lf = *L.leaf;
  const int H = c.h;
  if (open) {
    // a narrow panel swung in toward the room beyond: its free edge is further away, so higher on the screen
    for (int x = 3; x <= 8; x++) {
      int lift = (x - 3) * 3 / 2;
      int yb = H - 1 - lift, yt = yb - 25;
      for (int y = std::max(7, yt); y <= yb; y++) {
        int k = x == 3 ? 3 : (x == 8 ? 1 : 2);
        if (y == yt) k = 4;
        if (x == 6 && y > yt) k = 1;                   // a plank seam
        c.set(x, y, darken(Lf[k], 0.18f + (x - 3) * 0.04f));   // in the doorway's shade
      }
      if (x == 7) { c.set(x, yb - 12, L.strap->c[3]); c.set(x, yb - 11, L.strap->c[1]); }   // ring pull
    }
  } else {
    for (int y = 7; y < H; y++)
      for (int x = 3; x <= 12; x++) {
        int k = 2;
        if ((x - 3) % 3 == 2) k = 1;                   // plank seams
        if (x == 3) k = 3;
        if (x == 12) k = 1;
        if (y == 7 || y == H - 1) k = 0;
        uint32_t col = Lf[k];
        if ((y == 12 || y == H - 7) && x <= 9) col = L.strap->c[y == 12 ? 2 : 1];   // strap hinges
        c.set(x, y, col);
      }
    c.set(10, H - 15, L.strap->c[4]); c.set(10, H - 14, L.strap->c[2]); c.set(11, H - 14, L.strap->c[1]);   // latch
  }
  // shade under the lintel, inside the doorway
  for (int y = 7; y <= 10; y++)
    for (int x = 3; x <= 12; x++)
      if (!chA(c.get(x, y))) c.set(x, y, withA(kShadowCol, 130 - (y - 7) * 30));
  // jambs
  for (int y = 4; y < H; y++) {
    c.set(0, y, F[3]); c.set(1, y, F[2]); c.set(2, y, F[1]);
    c.set(13, y, F[3]); c.set(14, y, F[2]); c.set(15, y, F[1]);
    if (L.stone && y % 6 == 0) { c.set(0, y, F[1]); c.set(1, y, F[1]); c.set(13, y, F[1]); c.set(14, y, F[1]); }
  }
  for (int x : {0, 1, 2, 13, 14, 15}) c.set(x, H - 1, F[0]);
  // lintel: its top seen from above, its face below
  for (int x = 0; x < 16; x++) {
    c.set(x, 0, F[4]); c.set(x, 1, F[3]); c.set(x, 2, F[3]);
    c.set(x, 3, F[2]); c.set(x, 4, F[2]); c.set(x, 5, F[2]); c.set(x, 6, F[1]);
    if (L.stone && x == 8) { c.set(x, 3, F[4]); c.set(x, 4, F[3]); c.set(x, 5, F[3]); }   // keystone
  }
  c.set(0, 3, F[3]); c.set(15, 3, F[1]); c.set(15, 4, F[1]); c.set(15, 5, F[1]);
}

// an interior door in an N-S partition (16x32: canvas rows 0..15 lie on the wall's last tile north of the doorway,
// rows 16..31 on the doorway tile). M0b fix round 3: the old sprite stood a front-facing cased doorway on the end of
// the wall, which cut the wall's top short, read as a door leading north into the wall, and left the doorway tile
// as bare floor with a thin pale sill (an orphan post). Now the doorway is seen as the 3/4 view shows an N-S wall:
// from above, along the wall. The wall's top runs on to the frame; the frame's head block caps the wall's end; two
// casing rails run along the wall's lit (west) and shaded (east) edges down the whole doorway, so the wall line
// never breaks; a foot block meets the next run of wall. Between the rails lies the threshold (the floor layer's
// sill). Open: the leaf stands swung back against the east rail, its top edge and its lit face seen from above.
// Shut: the leaf fills the opening along the wall's line, planks running north-south, strap hinges across.
void doorVPaint(Canvas& c, RoomStyle rs, bool open) {
  const DoorLook L = doorLook(rs);
  const Ramp& F = *L.frame;
  const Ramp& Lf = *L.leaf;
  const Ramp& S = *L.strap;
  const int top = 16, bot = 31;   // the doorway tile
  // the head block on the wall's end: its top lit, its south face in shade (it rises over the doorway)
  for (int y = 11; y <= 17; y++)
    for (int x = 0; x <= 15; x++) {
      int k;
      if (y == 11) k = 4;
      else if (y <= 13) k = 3;
      else if (y <= 15) k = 2;
      else k = 1;                          // the face of the head, falling into the doorway's shade
      if (x == 0 && y > 11) k = std::min(4, k + 1);
      if (x == 15) k = std::max(0, k - 1);
      uint32_t col = F[k];
      if (L.stone && y >= 14 && (x == 5 || x == 10)) col = F[std::max(0, k - 1)];   // dressed blocks
      if (!L.stone && y == 13 && x % 5 == 2) col = F[2];                             // grain
      c.set(x, y, col);
    }
  // casing rails along both edges of the wall, down the doorway: lit west rail, shaded east rail
  for (int y = 18; y <= bot - 3; y++) {
    c.set(0, y, F[3]); c.set(1, y, F[4]); c.set(2, y, F[2]);
    c.set(13, y, F[2]); c.set(14, y, F[1]); c.set(15, y, F[0]);
    if (L.stone && y % 5 == 1) { c.set(1, y, F[3]); c.set(14, y, F[0]); }
  }
  // the foot block where the frame meets the next run of wall
  for (int y = bot - 2; y <= bot; y++)
    for (int x = 0; x <= 15; x++) {
      int k = y == bot - 2 ? 4 : (y == bot - 1 ? 3 : 2);
      if (x == 15) k = std::max(0, k - 2);
      c.set(x, y, F[k]);
    }
  // the head's shadow falls into the opening (soft, so the player walking through stays clear)
  for (int x = 3; x <= 12; x++) {
    c.set(x, 18, withA(kShadowCol, 120));
    c.set(x, 19, withA(kShadowCol, 70));
    c.set(x, 20, withA(kShadowCol, 30));
  }
  if (open) {
    // the leaf swung back flat against the east rail: its top edge (lit) and a sliver of its face
    for (int y = 18; y <= bot - 3; y++) {
      c.set(11, y, Lf[3]);
      c.set(12, y, Lf[1]);
      if (y == 18) { c.set(11, y, Lf[4]); c.set(12, y, Lf[3]); }
    }
    c.set(11, 22, S.c[3]); c.set(11, bot - 6, S.c[3]);   // hinge straps catching the light
  } else {
    // shut: the leaf in the wall's line, seen from above: planks running north-south, straps across, a ring pull
    for (int y = 18; y <= bot - 3; y++)
      for (int x = 3; x <= 12; x++) {
        int k = 2;
        if (x == 3) k = 4;
        else if (x == 4) k = 3;
        else if (x == 12) k = 0;
        else if (x == 11) k = 1;
        else if ((x - 3) % 3 == 0) k = 1;   // plank seams
        if (y == 18) k = std::max(0, k - 1);
        uint32_t col = Lf[k];
        if ((y == 21 || y == bot - 6) && x >= 3 && x <= 12) col = S.c[x <= 4 ? 3 : (x >= 11 ? 0 : 1)];   // straps
        c.set(x, y, col);
      }
    c.set(8, 24, S.c[4]); c.set(8, 25, S.c[2]); c.set(9, 25, S.c[1]);   // the ring pull
  }
}

Canvas longBedPiece(int variant);
Canvas styledPiece(RoomStyle rs, int which, int variant) {
  if (which == 4) return longBedPiece(variant);
  if (which == 5) {   // a shop's counter segment: variant = part (0..2) | goods << 2
    Canvas c(propW(Prop::CounterM), propH(Prop::CounterM));
    counterSeg(c, variant & 3, (variant >> 2) & 3);
    outline(c);
    return c;
  }
  static const Prop props[4] = {Prop::StairsUp, Prop::StairsDown, Prop::DoorH, Prop::DoorV};
  Prop p = props[std::clamp(which, 0, 3)];
  Canvas c(propW(p), propH(p));
  switch (which) {
    case 0: stairsUpPaint(c, rs, variant & 7); break;
    case 1: stairsDownPaint(c, rs, variant & 6); break;
    case 2: doorHPaint(c, rs, (variant & 2) == 0); break;
    default: doorVPaint(c, rs, (variant & 2) == 0); break;
  }
  outline(c);
  return c;
}

// wall decor fitted to a partition's short face (16x32; the face is canvas rows 16..31): the decor's own painting moved
// down onto the face and kept below the partition's top; the tall pieces get a small version of their own
Canvas lowDecorPiece(int idx) {
  Canvas c(16, 32);
  Prop p = (Prop)((int)Prop::Tapestry + std::clamp(idx, 0, (int)Prop::HolySymbol - (int)Prop::Tapestry));
  Canvas t(16, 30);
  switch (p) {
    case Prop::Tapestry: {   // a short pennant on a rod
      hline(t, 2, 13, 1, kWoodDark[3]); hline(t, 2, 13, 2, kWoodDark[1]);
      for (int y = 3; y <= 12; y++)
        for (int x = 4; x <= 11; x++) {
          int tip = 12 - std::abs(x * 2 - 15) / 3;
          if (y > tip) continue;
          int k = x == 4 ? 3 : (x == 11 ? 1 : 2);
          bool border = x == 5 || x == 10 || y == 4;
          t.set(x, y, border ? kGold[3] : kRed[k]);
        }
      t.set(7, 7, kGold[4]); t.set(8, 7, kGold[3]); t.set(7, 8, kGold[2]); t.set(8, 8, kGold[2]);
      break;
    }
    case Prop::Window: {     // a shuttered hatch through the wall
      for (int y = 2; y <= 11; y++)
        for (int x = 3; x <= 12; x++) {
          bool fr = x == 3 || x == 12 || y == 2 || y == 11;
          uint32_t col = fr ? kWood[(x == 3 || y == 2) ? 3 : 1] : mix(kVoid, kWoodDark[1], 0.4f);
          if (!fr && (x <= 4 || x >= 11)) col = kWood[x <= 4 ? 2 : 1];   // shutters folded back
          t.set(x, y, col);
        }
      hline(t, 2, 13, 12, kWood[4]); hline(t, 2, 13, 13, kWood[2]);
      break;
    }
    default: {
      switch (p) {
        case Prop::WallShelf: wallShelf(t); break;
        case Prop::HerbBundle: herbBundle(t); break;
        case Prop::Antlers: antlers(t); break;
        case Prop::Painting: painting(t); break;
        case Prop::Sconce: sconce(t, 0); break;
        case Prop::WallShield: wallShield(t); break;
        case Prop::ToolRack: toolRack(t); break;
        case Prop::PanRack: panRack(t); break;
        case Prop::Wreath: wreath(t); break;
        default: holySymbol(t); break;
      }
      break;
    }
  }
  // fit: the decor's top sits a pixel under the face's top edge; nothing below the face's skirting
  int y0 = 99, y1 = -1;
  for (int y = 0; y < t.h; y++)
    for (int x = 0; x < t.w; x++)
      if (chA(t.get(x, y))) { y0 = std::min(y0, y); y1 = std::max(y1, y); }
  if (y1 < 0) return c;
  int dy = 18 - y0;
  for (int y = y0; y <= y1; y++)
    for (int x = 0; x < 16; x++)
      if (y + dy <= 29 && chA(t.get(x, y))) c.set(x, y + dy, t.get(x, y));
  outline(c);
  return c;
}

Canvas rugPiece(int colour, int nm) {
  Canvas c(16, 16);
  static const Ramp* fields[4] = {&kRed, &kBlueCloth, &kGreenCloth, &kGold};
  static const Ramp* borders[4] = {&kGold, &kCloth, &kGold, &kRed};
  const Ramp& F = *fields[colour & 3];
  const Ramp& B = *borders[colour & 3];
  bool n = nm & 1, e = nm & 2, s = nm & 4, w = nm & 8;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      // distance to the rug's outer edge on the open sides (the rug is inset 1px from the tile edge there)
      int d = 99;
      if (!n) d = std::min(d, y - 1);
      if (!s) d = std::min(d, 14 - y);
      if (!w) d = std::min(d, x - 1);
      if (!e) d = std::min(d, 14 - x);
      if (d < 0) {
        bool fringe = (!n && y == 0 && x % 2 == 0 && x > 0 && x < 15) || (!s && y == 15 && x % 2 == 1 && x > 0 && x < 15);
        if (fringe) c.set(x, y, kCloth[3]);
        continue;
      }
      uint32_t col;
      if (d == 0) col = F[0];
      else if (d <= 2) col = d == 1 ? B[2] : B[3];
      else if (d == 3) col = F[1];
      else {
        int u = (x + 4) % 8, v = (y + 4) % 8;
        int dd = std::abs(u - 4) + std::abs(v - 4);
        col = dd == 4 ? F[3] : (dd == 1 ? B[3] : (dd == 0 ? B[4] : F[2]));
        if (dd == 2 && ((x + y) & 1)) col = F[1];
      }
      c.set(x, y, col);
    }
  return c;
}

Canvas shadowPiece(int w, int h) {
  Canvas c(std::max(1, w), std::max(1, h));
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      float dx = (x + 0.5f - w * 0.5f) / (w * 0.5f), dy = (y + 0.5f - h * 0.5f) / (h * 0.5f);
      float d = std::pow(std::fabs(dx), 3.0f) + std::pow(std::fabs(dy), 2.2f);
      if (d > 1.0f) continue;
      c.set(x, y, withA(kShadowCol, d < 0.45f ? 120 : 76));
    }
  return c;
}

// ---- floor clutter (16x16, anchored like a prop: the bottom row is the tile's bottom) --------------------
void groundShadow(Canvas& c, float cx, float cy, float rx, float ry) {
  for (int y = (int)(cy - ry - 1); y <= (int)(cy + ry + 1); y++)
    for (int x = (int)(cx - rx - 1); x <= (int)(cx + rx + 1); x++) {
      float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
      if (dx * dx + dy * dy > 1 || chA(c.get(x, y))) continue;
      c.set(x, y, withA(kShadowCol, 96));
    }
}
void wicker(Canvas& c, int x0, int y0, int x1, int y1) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      if ((y == y1) && (x == x0 || x == x1)) continue;
      int k = ((x + y * 2) % 3 == 0) ? 1 : 3;
      if (x == x0) k = 3;
      if (x == x1) k = 1;
      if (y == y0) k = 4;
      if (y == y1) k = 1;
      c.set(x, y, kThatch[k]);
    }
}
void clutter(Canvas& c, int id, float& shx, float& shy, float& shrx, float& shry) {
  const int B = 13;   // ground contact row
  shx = 8.5f; shy = B + 0.5f; shrx = 5.5f; shry = 1.6f;
  switch (id) {
    case 0: {   // basket of apples and greens
      for (int i = 0; i < 5; i++) ball(c, 5.0f + i * 1.6f, B - 6.0f + (i & 1), 1.5f, 1.4f, i == 2 ? kGreenCloth : kRed);
      wicker(c, 3, B - 5, 12, B);
      c.set(5, B - 7, kWhite);
      break;
    }
    case 1: {   // two grain sacks
      for (int s = 0; s < 2; s++) {
        float x = s ? 10.5f : 5.5f, y = B - (s ? 3.5f : 4.5f);
        ball(c, x, y, s ? 3.2f : 3.6f, s ? 3.4f : 4.2f, kCloth);
        int tx = (int)x;
        c.set(tx, (int)(y - (s ? 3.4f : 4.2f)) - 1, kCloth[2]); c.set(tx - 1, (int)(y - (s ? 3.4f : 4.2f)) - 2, kCloth[3]); c.set(tx + 1, (int)(y - (s ? 3.4f : 4.2f)) - 2, kCloth[2]);
        hline(c, tx - 1, tx + 1, (int)(y - (s ? 3.4f : 4.2f)), kLeather[2]);
      }
      c.set(4, B - 4, kRed[2]); c.set(5, B - 4, kRed[2]); c.set(4, B - 3, kRed[2]);   // a stencilled mark
      shrx = 6.5f;
      break;
    }
    case 2: {   // a lidded pot with a pan leaning on it
      ball(c, 6.5f, B - 3.0f, 3.6f, 3.0f, kSoot);
      hline(c, 3, 10, B - 5, kSoot[3]); hline(c, 4, 9, B - 6, kSoot[4]); c.set(6, B - 7, kIron[3]);
      ellipse(c, 11.5f, B - 4.0f, 2.6f, 3.4f, kIron[1]); ellipse(c, 11.3f, B - 4.2f, 1.7f, 2.4f, kIron[2]);
      vline(c, 12, B - 10, B - 8, kWoodDark[2]);
      break;
    }
    case 3: {   // laundry basket with folded linen
      wicker(c, 2, B - 4, 13, B);
      for (int x = 3; x <= 12; x++) { c.set(x, B - 5, x < 8 ? kWhite : kBlueCloth[4]); c.set(x, B - 6, x < 8 ? kCloth[4] : kBlueCloth[3]); }
      c.set(4, B - 7, kWhite); c.set(5, B - 7, kCloth[4]); c.set(10, B - 7, kBlueCloth[4]);
      shrx = 6.5f;
      break;
    }
    case 4: {   // smith's tools on the floor: a hammer and a pair of tongs
      // tongs: two long jaws, lying diagonally
      line(c, 3, B, 12, B - 4, kIron[1]); line(c, 3, B - 1, 12, B - 5, kIron[3]);
      c.set(12, B - 5, kIron[4]); c.set(13, B - 5, kIron[2]); c.set(13, B - 4, kIron[1]);
      // hammer: an ash handle and a squared iron head
      line(c, 5, B - 3, 10, B - 8, kWood[3]); line(c, 6, B - 3, 11, B - 8, kWood[1]);
      box(c, 9, B - 11, 13, B - 9, kIron[2]); hline(c, 9, 13, B - 11, kIron[4]); hline(c, 9, 13, B - 9, kIron[1]);
      c.set(9, B - 10, kIron[3]); c.set(13, B - 10, kIron[0]);
      break;
    }
    case 5: {   // a stack of books and one lying open
      const Ramp* cols[4] = {&kRed, &kBlueCloth, &kGreenCloth, &kLeather};
      for (int i = 0; i < 4; i++) bookAt(c, 2 + (i & 1), B - i * 2, *cols[i]);
      for (int x = 9; x <= 13; x++) { c.set(x, B - 1, x == 11 ? kCloth[2] : kWhite); c.set(x, B, kCloth[3]); }
      c.set(10, B - 1, kCloth[1]); c.set(12, B - 1, kCloth[1]);
      break;
    }
    case 6: {   // bottles
      const Ramp* g[3] = {&kGlassG, &kClay, &kGlassG};
      for (int i = 0; i < 3; i++) {
        int x = 4 + i * 3, h = i == 1 ? 6 : 8;
        for (int y = B - h + 3; y <= B; y++) { c.set(x, y, (*g[i])[3]); c.set(x + 1, y, (*g[i])[1]); }
        vline(c, x, B - h, B - h + 2, (*g[i])[2]); c.set(x, B - h - 1, kWood[2]);
        c.set(x, B - h + 4, (*g[i])[4]);
      }
      break;
    }
    case 7: {   // a dish of candles
      ellipse(c, 8.0f, B - 1.0f, 5.0f, 1.8f, kBrass[2]); hline(c, 4, 12, B - 2, kBrass[3]);
      candleAt(c, 5, B - 2, 5); candleAt(c, 8, B - 2, 7); candleAt(c, 11, B - 2, 4);
      break;
    }
    case 8: {   // scrolls, an inkwell and a quill
      for (int i = 0; i < 3; i++) {
        int y = B - i * 2, x0 = 2 + i, x1 = 10 + i;
        for (int x = x0; x <= x1; x++) { c.set(x, y - 1, kCloth[4]); c.set(x, y, kCloth[2]); }
        c.set(x0, y - 1, kCloth[3]); c.set(x1, y, kCloth[1]);
        c.set((x0 + x1) / 2, y - 1, kRed[2]); c.set((x0 + x1) / 2, y, kRed[1]);
      }
      ball(c, 13.0f, B - 1.5f, 1.6f, 1.6f, kSoot); c.set(13, B - 3, kInk);
      line(c, 13, B - 3, 15, B - 8, kWhite);
      break;
    }
    case 9: {   // a jug and two mugs
      jugAt(c, 6, B);
      mugAt(c, 9, B, false); mugAt(c, 12, B - 1, false);
      break;
    }
    case 10: {   // a stack of plates and bowls
      for (int i = 0; i < 4; i++) { hline(c, 3, 9, B - i, i == 3 ? kBone[4] : kBone[2 + (i & 1)]); c.set(3, B - i, kBone[3]); }
      hline(c, 4, 8, B - 4, kBlueCloth[3]);
      ellipse(c, 12.0f, B - 1.0f, 2.6f, 1.5f, kWood[2]); hline(c, 10, 14, B - 2, kWood[3]);
      break;
    }
    case 11: {   // a bread basket: crusty loaves standing above the rim
      for (int i = 0; i < 3; i++) {
        float x = 5.0f + i * 3.0f, y = B - 4.5f - (i == 1 ? 1.0f : 0.0f);
        ball(c, x, y, 2.2f, 1.9f, kLeather);
        c.set((int)x - 1, (int)y - 1, kThatch[3]); c.set((int)x, (int)y - 1, kLeather[4]);
        c.set((int)x + 1, (int)y, kLeather[1]);
      }
      wicker(c, 3, B - 3, 12, B);
      break;
    }
    case 12: {   // cheese wheels on a board
      hline(c, 2, 13, B, kWood[1]); hline(c, 2, 13, B - 1, kWood[3]);
      ball(c, 6.0f, B - 3.5f, 3.6f, 2.6f, kGold);
      hline(c, 3, 9, B - 4, kGold[4]);
      for (int y = B - 5; y <= B - 2; y++) for (int x = 10; x <= 12; x++) c.set(x, y, x == 10 ? kGold[3] : kGold[2]);
      c.set(10, B - 5, kGold[4]); c.set(12, B - 2, kGold[1]); c.set(11, B - 4, kGold[1]);
      break;
    }
    case 13: {   // a wooden bowl of fruit
      ellipse(c, 8.0f, B - 1.5f, 5.0f, 2.2f, kWood[2]); hline(c, 4, 12, B - 2, kWood[3]); hline(c, 4, 12, B, kWood[1]);
      ball(c, 6.0f, B - 3.5f, 1.6f, 1.5f, kRed); ball(c, 9.0f, B - 4.0f, 1.6f, 1.5f, kGold); ball(c, 11.0f, B - 3.0f, 1.5f, 1.4f, kGreenCloth);
      ball(c, 7.5f, B - 5.5f, 1.4f, 1.3f, kPurple);
      break;
    }
    case 14: {   // a water bucket
      for (int y = B - 6; y <= B; y++) for (int x = 4; x <= 11; x++) c.set(x, y, kWood[x == 4 ? 3 : (x == 11 ? 1 : ((x % 2) ? 2 : 3))]);
      hline(c, 4, 11, B - 5, kIron[3]); hline(c, 4, 11, B - 1, kIron[1]);
      hline(c, 5, 10, B - 6, kWater[2]); c.set(6, B - 6, kWater[4]);
      line(c, 4, B - 7, 7, B - 10, kIron[2]); line(c, 11, B - 7, 8, B - 10, kIron[1]);
      break;
    }
    case 15: {   // scattered straw
      for (int i = 0; i < 16; i++) {
        int x = 2 + (int)(hash3(i, 0, 991) % 12), y = B - 3 + (int)(hash3(i, 1, 991) % 4);
        int dx = (hash3(i, 2, 991) & 1) ? 1 : -1;
        c.set(x, y, kThatch[3 + (i & 1)]); c.set(x + dx, y - 1, kThatch[2]);
      }
      shry = 0.0f;
      break;
    }
    case 16: {   // a broom leaning on the wall corner
      line(c, 12, 1, 6, B - 4, kWood[3]); line(c, 13, 1, 7, B - 4, kWood[1]);
      for (int y = B - 4; y <= B; y++) {
        int half = 1 + (y - (B - 4)) / 2;
        for (int x = 6 - half; x <= 6 + half; x++) c.set(x, y, kThatch[(x + y) % 3 == 0 ? 1 : (x < 6 ? 4 : 3)]);
      }
      hline(c, 5, 7, B - 4, kLeather[2]);
      shrx = 3.5f;
      break;
    }
    default: {  // kindling: split logs stacked two and one, end grain to the viewer
      static const float L[3][2] = {{5.5f, B - 2.5f}, {10.5f, B - 2.5f}, {8.0f, B - 6.5f}};
      for (auto& l : L) {
        ellipse(c, l[0], l[1], 2.7f, 2.4f, kBark[1]);
        ellipse(c, l[0] - 0.3f, l[1] - 0.3f, 2.0f, 1.7f, kWood[3]);
        ellipse(c, l[0] - 0.3f, l[1] - 0.3f, 1.0f, 0.8f, kWood[2]);
        c.set((int)(l[0] - 1.3f), (int)(l[1] - 1.3f), kWood[4]);
      }
      shrx = 6.0f;
      break;
    }
  }
}
Canvas clutterPiece(int id) {
  Canvas c(16, 16);
  float sx, sy, rx, ry;
  clutter(c, id, sx, sy, rx, ry);
  outline(c);
  if (ry > 0) groundShadow(c, sx + 1.0f, sy, rx, ry);
  return c;
}

// ---- M0b furniture (3/4 view, light from the top-left; outlined by propSprite) --------------------------------
void flameAt(Canvas& c, int x, int y, int frame) {   // a candle flame whose base is at (x, y)
  static const int fl[4][2] = {{3, 0}, {4, -1}, {3, 1}, {4, 0}};
  int h = fl[frame & 3][0], sx = fl[frame & 3][1];
  for (int k = 0; k < h; k++) c.set(x + (k == h - 1 ? sx : 0), y - k, kFire[k == 0 ? 2 : (k == h - 1 ? 4 : 3)]);
  c.set(x, y - h, withA(kGlow[4], 120));
}

void nightstand(Canvas& c) {
  const int W = c.w, base = c.h - 1;
  block34(c, 1, 6, W - 2, 4, base - 9, kWood);
  // a drawer with a brass knob, an open shelf below
  for (int x = 2; x <= W - 3; x++) { c.set(x, 11, kWood[1]); c.set(x, 13, kWood[3]); }
  c.set(W / 2 - 1, 12, kBrass[4]); c.set(W / 2, 12, kBrass[2]);
  for (int y = 14; y <= base - 1; y++) for (int x = 2; x <= W - 3; x++) c.set(x, y, y == 14 ? kWoodDark[0] : kWoodDark[1]);
  bookAt(c, 3, base - 1, kBlueCloth);
  for (int x = 1; x <= W - 2; x++) c.set(x, base, kWood[0]);
  // a candle in a brass dish
  hline(c, 2, 5, 8, kBrass[1]); hline(c, 2, 5, 7, kBrass[3]);
  candleAt(c, 3, 6, 4);
  c.set(8, 8, kCloth[3]); c.set(9, 8, kCloth[2]); c.set(8, 7, kCloth[4]);   // a folded note
}

void washstand(Canvas& c) {
  const int W = c.w, base = c.h - 1;
  block34(c, 1, 9, W - 2, 4, base - 12, kWood);
  // an open shelf with a folded towel and a soap dish
  for (int y = 15; y <= base - 2; y++) for (int x = 2; x <= W - 3; x++) c.set(x, y, y == 15 ? kWoodDark[0] : kWoodDark[1]);
  for (int y = 16; y <= base - 2; y++) for (int x = 3; x <= 7; x++) c.set(x, y, y == 16 ? kWhite : (x == 7 ? kCloth[2] : kCloth[3]));
  hline(c, 9, 11, base - 2, kPewter[3]); c.set(10, base - 3, kBone[4]);
  for (int x = 1; x <= W - 2; x++) c.set(x, base, kWood[0]);
  // the basin on top: a glazed bowl with water
  ellipse(c, 6.0f, 10.5f, 4.6f, 2.2f, kBone[1]);
  ellipse(c, 6.0f, 10.2f, 4.0f, 1.7f, kBone[4]);
  ellipse(c, 6.0f, 10.6f, 3.0f, 1.1f, kWater[3]);
  c.set(4, 10, kWater[4]);
  hline(c, 3, 9, 12, kBone[2]);
  // a jug beside it
  ball(c, 11.5f, 8.5f, 2.0f, 2.6f, kBlueCloth);
  vline(c, 11, 4, 6, kBlueCloth[2]); c.set(12, 4, kBlueCloth[1]); c.set(11, 3, kBlueCloth[3]);
  c.set(13, 7, kBlueCloth[1]); c.set(13, 8, kBlueCloth[0]);
  // a towel over the side rail
  for (int y = 13; y <= 18; y++) { c.set(W - 2, y, y % 3 == 0 ? kRed[2] : kWhite); c.set(W - 1, y, kCloth[2]); }
}

void oven(Canvas& c, int frame) {
  const int W = c.w, base = c.h - 1;
  // a stone hearth slab, a brick body with a dome on top and a smoke flue, the arched mouth glowing
  for (int y = base - 3; y <= base; y++)
    for (int x = 0; x < W; x++) c.set(x, y, kStone[y == base - 3 ? 4 : (y == base ? 0 : (x == 0 ? 3 : (x == W - 1 ? 1 : 2)))]);
  for (int y = 9; y <= base - 4; y++)
    for (int x = 1; x <= W - 2; x++) {
      float t = (x - 1 + 0.5f) / (W - 2) * 2 - 1;
      int k = lightIndex(lightAt(t * 0.9f, 0), x, y, 0.05f);
      int row = (y - 9) / 3, sh = (row & 1) * 3;
      bool mortar = (y - 9) % 3 == 2 || (x + sh) % 6 == 0;
      c.set(x, y, mortar ? kBrick[std::max(0, k - 1)] : kBrick[k]);
    }
  for (int y = 1; y <= 12; y++)
    for (int x = 1; x <= W - 2; x++) {
      float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f - 1), dy = (y - 11.0f) / 9.5f;
      if (dx * dx + dy * dy > 1.0f || y > 10) continue;
      int k = lightIndex(lightAt(dx * 0.9f, dy * 0.9f), x, y, 0.08f);
      c.set(x, y, kBrick[k]);
    }
  for (int x = 1; x <= W - 2; x++) c.set(x, 10, kBrick[1]);
  for (int y = 0; y <= 3; y++) { c.set(9, y, kSoot[3]); c.set(10, y, kSoot[2]); c.set(11, y, kSoot[1]); }
  hline(c, 9, 11, 0, kSoot[4]);
  // the mouth: an arch of headers, the fire inside, embers spilling onto the slab
  for (int y = 13; y <= base - 4; y++)
    for (int x = 4; x <= W - 5; x++) {
      float ax = std::fabs(x + 0.5f - W * 0.5f);
      if (y < 13 + (int)(ax * ax / 5.0f)) continue;
      int d = base - 4 - y;
      uint32_t col = mix(kVoid, kBrick[0], 0.4f);
      int flick = (int)(hash3(x, frame, 1001) % 3);
      if (d <= 1 + flick) col = kFire[d == 0 ? 3 : 2];
      else if (d <= 3 + flick) col = kFire[1];
      if (d == 0 && (x + frame) % 3 == 0) col = kGlow[4];
      c.set(x, y, col);
    }
  for (int x = 3; x <= W - 4; x++) {
    float ax = std::fabs(x + 0.5f - W * 0.5f);
    int y = 12 + (int)(ax * ax / 5.0f);
    if (y < base - 4) c.set(x, y, kBrick[4]);
  }
  c.set(5 + frame % 3, base - 3, kFire[3]); c.set(10 - frame % 2, base - 3, kFire[2]);
  // a peel leaning on the side
  line(c, W - 2, 6, W - 1, base - 4, kWood[3]);
}

void prepTable(Canvas& c) {
  tableSmall(c, 0);
  const int top = c.h - 15;
  // a chopping board with a cleaver, carrots, an onion and a cabbage
  for (int y = top + 2; y <= top + 5; y++) for (int x = 2; x <= 8; x++) c.set(x, y, y == top + 5 ? kWood[2] : (x == 2 || y == top + 2 ? kWood[4] : kThatch[3]));
  hline(c, 4, 7, top + 3, kPewter[4]); hline(c, 4, 7, top + 4, kPewter[2]); c.set(3, top + 4, kWoodDark[1]); c.set(3, top + 3, kWoodDark[2]);
  for (int i = 0; i < 2; i++) { int y = top + 6 + i; hline(c, 3 + i, 6 + i, y, kOchre[3]); c.set(7 + i, y, kGreenCloth[3]); }
  ball(c, 11.5f, top + 3.0f, 2.4f, 2.2f, kGreenCloth);
  c.set(11, top + 2, kGreenCloth[4]);
  ball(c, 12.5f, top + 6.5f, 1.4f, 1.3f, kBone);
  c.set(13, top + 5, kGreenCloth[2]);
}

void bottleShelf(Canvas& c) {
  const int W = c.w, base = c.h - 1;
  const Ramp& R = kWoodDark;
  // a crowned back shelf: two shelves of bottles, mugs on the lowest, a closed cupboard below
  for (int x = 0; x < W; x++) { c.set(x, 0, kWood[4]); c.set(x, 1, kWood[2]); c.set(x, 2, kWood[0]); }
  for (int y = 3; y <= base; y++) { c.set(0, y, R[4]); c.set(1, y, R[3]); c.set(W - 2, y, R[2]); c.set(W - 1, y, R[1]); }
  static const Ramp* glass[5] = {&kGlassG, &kRed, &kBrass, &kBlueCloth, &kPurple};
  for (int s = 0; s < 3; s++) {
    int y0 = 3 + s * 7, y1 = y0 + 6;
    for (int y = y0; y < y1; y++) for (int x = 2; x <= W - 3; x++) c.set(x, y, y == y0 ? R[0] : R[1]);
    hline(c, 1, W - 2, y1, kWood[4]);
    if (s < 2) {
      for (int i = 0; i < 4; i++) {
        int x = 3 + i * 3, h = 4 + (int)(hash3(i, s, 1011) % 2);
        const Ramp& G = *glass[(i + s * 2) % 5];
        for (int y = y1 - h; y < y1; y++) { c.set(x, y, G[2]); c.set(x + 1, y, G[1]); }
        c.set(x, y1 - h + 1, G[4]);
        c.set(x, y1 - h - 1, kWood[2]);   // the cork
        if (i == 1 && s == 0) c.set(x + 1, y1 - h - 1, kWood[1]);
      }
    } else {
      mugAt(c, 3, y1 - 1, false);
      mugAt(c, 8, y1 - 1, false);
      c.set(12, y1 - 1, kClay[2]); c.set(12, y1 - 2, kClay[3]); c.set(13, y1 - 1, kClay[1]); c.set(12, y1 - 3, kClay[3]);
    }
  }
  for (int y = 25; y <= base - 1; y++)
    for (int x = 2; x <= W - 3; x++) {
      bool edge = x == 2 || x == W - 3 || y == 25 || y == base - 1 || x == W / 2;
      c.set(x, y, edge ? kWood[x == 2 || y == 25 ? 3 : 1] : kWood[2]);
    }
  c.set(W / 2 - 2, 29, kBrass[4]); c.set(W / 2 + 1, 29, kBrass[3]);
  for (int x = 0; x < W; x++) c.set(x, base, R[0]);
}

void weaponRack(Canvas& c) {
  const int W = c.w, base = c.h - 1;
  // spears behind, a frame of two uprights with a slotted top rail and a foot box, swords in the slots, a shield
  for (int sx : {3, 12}) {
    vline(c, sx, 3, base - 3, kWood[2]); vline(c, sx + 1, 3, base - 3, kWood[1]);
    c.set(sx, 0, kIron[4]); c.set(sx, 1, kIron[3]); c.set(sx + 1, 1, kIron[2]); c.set(sx, 2, kIron[2]); c.set(sx + 1, 2, kIron[1]);
  }
  for (int sx : {6, 9}) {
    hline(c, sx - 1, sx + 1, 9, kBrass[3]); c.set(sx + 1, 9, kBrass[1]);
    vline(c, sx, 6, 8, kLeather[2]); c.set(sx, 5, kBrass[4]);
    for (int y = 10; y <= base - 5; y++) { c.set(sx, y, kIron[4]); c.set(sx + 1, y, kIron[2]); }
  }
  for (int uy : {0, 1}) {
    int x = uy ? W - 2 : 0;
    for (int y = 6; y <= base; y++) { c.set(x, y, kWood[uy ? 1 : 3]); c.set(x + 1, y, kWood[uy ? 0 : 2]); }
    c.set(x, 5, kWood[4]); c.set(x + 1, 5, kWood[3]);
  }
  for (int x = 0; x < W; x++) { c.set(x, 11, kWood[x == 0 ? 4 : 3]); c.set(x, 12, kWood[1]); }
  block34(c, 1, base - 5, W - 2, 2, 4, kWood);
  // a round shield leaning against the foot
  ball(c, 11.0f, base - 6.0f, 4.0f, 4.6f, kRed);
  for (int a = 0; a < 24; a++) {
    float t = a / 24.0f * 6.2832f;
    c.set((int)std::lround(11.0f + std::cos(t) * 3.8f), (int)std::lround(base - 6.0f + std::sin(t) * 4.4f), kIron[a < 12 ? 2 : 3]);
  }
  ball(c, 11.0f, base - 6.0f, 1.3f, 1.3f, kIron);
}

void lectern(Canvas& c) {
  const int W = c.w, base = c.h - 1;
  // a slanted reading desk on a turned post, a big book open on it, a ribbon hanging down
  for (int y = 2; y <= 8; y++)
    for (int x = 0; x < W; x++) {
      int k = y == 8 ? 4 : (y >= 7 ? 1 : 3);
      if (x == 0) k = std::min(4, k + 1);
      if (x == W - 1) k = std::max(0, k - 1);
      c.set(x, y, kWood[k]);
    }
  for (int y = 9; y <= 10; y++) for (int x = 1; x < W - 1; x++) c.set(x, y, kWood[y == 9 ? 1 : 2]);
  for (int y = 1; y <= 6; y++)
    for (int x = 1; x <= W - 2; x++) {
      bool gutter = x == W / 2 || x == W / 2 - 1;
      uint32_t col = gutter ? kCloth[2] : (y == 1 ? kCloth[3] : kWhite);
      if (!gutter && y >= 2 && y <= 5 && x % 2 == 0 && x != 1 && x != W - 2) col = kCloth[3];   // lines of text
      c.set(x, y, col);
    }
  vline(c, W / 2, 7, 12, kRed[2]);
  for (int y = 11; y <= base - 2; y++) { c.set(W / 2 - 1, y, kWood[3]); c.set(W / 2, y, kWood[2]); c.set(W / 2 + 1, y, kWood[1]); }
  c.set(W / 2 - 1, 14, kWood[4]);
  for (int x = 2; x <= W - 3; x++) { c.set(x, base - 1, kWood[x == 2 ? 3 : 2]); c.set(x, base, kWood[0]); }
}

void candelabra(Canvas& c, int frame) {
  const int W = c.w, base = c.h - 1;
  const Ramp& R = kBrass;
  // a tripod foot, a turned stem with knops, two arms and a central socket, three candles
  for (int k = 0; k < 3; k++) { c.set(W / 2 - 1 - k, base - 2 + k, R[3]); c.set(W / 2 + k, base - 2 + k, R[1]); }
  c.set(W / 2, base, R[2]);
  for (int y = 9; y <= base - 2; y++) { c.set(W / 2 - 1, y, R[3]); c.set(W / 2, y, R[1]); }
  for (int ky : {14, 20}) { c.set(W / 2 - 2, ky, R[4]); c.set(W / 2 + 1, ky, R[1]); c.set(W / 2 - 1, ky, R[4]); }
  for (int x = 1; x <= W - 2; x++) c.set(x, 10, x < W / 2 ? R[3] : R[2]);
  c.set(1, 9, R[3]); c.set(W - 2, 9, R[2]);
  for (int cx : {1, W / 2 - 1, W - 3}) {
    int cy = cx == W / 2 - 1 ? 8 : 9, h = cx == W / 2 - 1 ? 5 : 4;
    hline(c, cx - (cx == 1 ? 0 : 0), cx + 1, cy, R[4]);
    vline(c, cx, cy - h, cy - 1, kCloth[4]); vline(c, cx + 1, cy - h, cy - 1, kCloth[2]);
    flameAt(c, cx, cy - h - 1, frame + cx);
  }
}

void displayTable(Canvas& c) {
  const int W = c.w, base = c.h - 1;
  const int top = c.h - 15;
  // a trestle table under a green cloth that hangs to a fringed edge, wares laid out on it
  tableLegs(c, 2, top + 9, base, kWood, false);
  tableLegs(c, 12, top + 9, base, kWood, false);
  for (int y = top; y <= top + 11; y++)
    for (int x = 0; x < W; x++) {
      int k = 3;
      if (y >= top + 7) k = (x == 0 ? 3 : (x == W - 1 ? 1 : 2));
      if (y == top + 6) k = 4;
      if (y >= top + 7 && (x % 4 == 1)) k = std::max(0, k - 1);   // folds
      if (y == top + 11) { if (x % 2) continue; k = 1; }
      c.set(x, y, kGreenCloth[k]);
    }
  for (int x = 1; x < W - 1; x++) c.set(x, top + 7, kGold[x % 3 == 0 ? 3 : 2]);   // a braided hem
  // wares: a little pile of coins, a red potion, a gem, a dagger, a scroll
  c.set(3, top + 4, kGold[4]); c.set(4, top + 4, kGold[3]); c.set(3, top + 5, kGold[2]); c.set(4, top + 5, kGold[2]); c.set(5, top + 5, kGold[1]); c.set(4, top + 3, kGold[4]);
  ball(c, 8.0f, top + 3.5f, 1.5f, 1.6f, kRed); c.set(8, top + 1, kWood[2]); c.set(7, top + 3, kRed[4]);
  c.set(12, top + 2, kCrystal[4]); c.set(11, top + 3, kCrystal[3]); c.set(12, top + 3, kCrystal[2]); c.set(13, top + 3, kCrystal[1]);
  hline(c, 10, 14, top + 5, kIron[4]); c.set(9, top + 5, kLeather[2]); c.set(10, top + 4, kBrass[3]); c.set(10, top + 6, kBrass[2]);
  hline(c, 2, 5, top + 1, kCloth[4]); c.set(2, top + 2, kCloth[2]); c.set(5, top + 2, kCloth[2]);
}

void quenchTub(Canvas& c) {
  const int W = c.w, base = c.h - 1;
  // a low cooper's tub with iron hoops, dark water in it, tongs hanging over the rim
  for (int y = 7; y <= base; y++)
    for (int x = 1; x <= W - 2; x++) {
      float t = (x - 1 + 0.5f) / (W - 2) * 2 - 1;
      int k = lightIndex(lightAt(t * 0.95f, 0), x, y, 0.04f);
      uint32_t col = kWood[k];
      if ((x - 1) % 3 == 2) col = kWood[std::max(0, k - 1)];
      if (y == 9 || y == base - 2) col = kIron[std::min(4, k + 1)];
      if (y == base) col = kWood[0];
      c.set(x, y, col);
    }
  ellipse(c, W * 0.5f, 6.5f, W * 0.5f - 0.5f, 2.6f, kWood[3]);
  ellipse(c, W * 0.5f, 6.6f, W * 0.5f - 2.0f, 1.8f, kWater[0]);
  hline(c, 5, 8, 6, kWater[2]); c.set(6, 5, kWater[3]);
  for (int x = 2; x <= W - 3; x++) c.set(x, 4, kWood[4]);
  // tongs
  line(c, 10, 6, 13, 0, kIron[3]); line(c, 11, 6, 14, 1, kIron[1]);
  c.set(13, 0, kIron[4]); c.set(14, 0, kIron[2]);
}

void grindstone(Canvas& c) {
  const int W = c.w, base = c.h - 1;
  // a sandstone wheel on an axle in a trestle frame, a trough under it, a crank on the side
  for (int y = 12; y <= base; y++) {
    c.set(2, y, kWood[3]); c.set(3, y, kWood[1]);
    c.set(W - 4, y, kWood[2]); c.set(W - 3, y, kWood[0]);
  }
  block34(c, 3, 14, W - 6, 2, 3, kWood);
  for (int x = 4; x <= W - 5; x++) c.set(x, 14, kWater[x % 3 == 0 ? 3 : 2]);
  // the wheel stands across our view: we see its rim edge-on, a worn grey sandstone band
  for (int y = 2; y <= 15; y++)
    for (int x = 5; x <= W - 6; x++) {
      float dy = (y + 0.5f - 8.5f) / 6.8f, dx = (x + 0.5f - W * 0.5f) / 3.2f;
      if (dx * dx + dy * dy > 1.0f) continue;
      int k = lightIndex(lightAt(dx * 0.8f, dy * 0.7f), x, y, 0.08f);
      c.set(x, y, (y % 3 == 0 && k > 1) ? kStone[k - 1] : kStone[k]);
    }
  hline(c, 1, W - 2, 9, kIron[1]); c.set(1, 9, kIron[3]);
  vline(c, W - 1, 9, 12, kIron[2]); c.set(W - 1, 12, kWood[3]); c.set(W - 1, 13, kWood[2]);
}

void pillar(Canvas& c) {
  const int W = c.w, base = c.h - 1;
  const Ramp& R = kStoneWarm;
  // abacus and capital, a fluted shaft lit from the left, a moulded base on a plinth
  for (int y = 0; y <= 2; y++) for (int x = 0; x < W; x++) c.set(x, y, R[y == 0 ? 4 : (y == 2 ? 2 : 3)]);
  for (int y = 3; y <= 4; y++) for (int x = 0; x < W; x++) c.set(x, y, R[y == 3 ? 2 : 1]);
  for (int y = 5; y <= 7; y++)
    for (int x = 1 + (y - 5); x <= W - 2 - (y - 5); x++) c.set(x, y, R[x < W / 2 ? 3 : 2]);
  for (int y = 8; y <= base - 7; y++)
    for (int x = 3; x <= W - 4; x++) {
      float t = (x - 3 + 0.5f) / (W - 6) * 2 - 1;
      int k = lightIndex(lightAt(t * 0.95f, 0), x, y, 0.0f);
      if ((x - 3) % 3 == 2) k = std::max(0, k - 1);   // flutes
      c.set(x, y, R[k]);
    }
  for (int x = 3; x <= W - 4; x++) { c.set(x, 8, R[1]); c.set(x, base - 7, R[1]); }
  for (int y = base - 6; y <= base - 4; y++) for (int x = 2; x <= W - 3; x++) c.set(x, y, R[y == base - 6 ? 4 : (x < W / 2 ? 3 : 2)]);
  for (int y = base - 3; y <= base; y++) for (int x = 0; x < W; x++) c.set(x, y, R[y == base - 3 ? 4 : (y == base ? 0 : (x == 0 ? 3 : (x == W - 1 ? 1 : 2)))]);
}

void bunkBed(Canvas& c) {
  const int W = c.w, H = c.h;
  // the lower bunk is an ordinary bed; the upper deck stands over its head end on four tall posts
  Canvas lower(20, 32);
  bed(lower);
  blit(c, lower, 0, H - 32);
  const int d0 = 3, d1 = 19;   // the upper mattress rows
  for (int y = d0; y <= d1; y++)
    for (int x = 2; x <= W - 3; x++) {
      uint32_t col;
      if (y <= d0 + 4) {   // pillow
        float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f - 3), dy = (y - d0 - 2.0f) / 2.4f;
        col = dx * dx * dx * dx + dy * dy > 1.0f ? kCloth[2] : (dy < 0 ? kWhite : kCloth[4]);
      } else {
        bool patch = (((x - 2) / 4) + ((y - d0 - 5) / 4)) % 2 == 0;
        int k = patch ? 2 : 3;
        if (x == 2) k++;
        if (x >= W - 4) k--;
        if (y == d0 + 5) k--;
        if (((x - 2) % 4 == 0 || (y - d0 - 5) % 4 == 0) && x > 2) k--;
        col = patch ? kGreenCloth[std::clamp(k, 0, 4)] : kCloth[std::clamp(k, 0, 4)];
      }
      c.set(x, y, col);
    }
  // the deck's side rail and its shadow on the lower bunk
  for (int x = 1; x <= W - 2; x++) { c.set(x, d1 + 1, kWood[4]); c.set(x, d1 + 2, kWood[2]); c.set(x, d1 + 3, kWood[1]); }
  for (int y = d1 + 4; y <= d1 + 7; y++)
    for (int x = 2; x <= W - 3; x++) c.set(x, y, darken(c.get(x, y), 0.5f - (y - d1 - 4) * 0.12f));
  // posts and the upper headboard
  for (int side = 0; side < 2; side++) {
    int x0 = side ? W - 2 : 0;
    for (int y = 0; y < H; y++) { c.set(x0, y, kWood[side ? 2 : 4]); c.set(x0 + 1, y, kWood[side ? 0 : 2]); }
    c.set(x0, 0, kWood[4]); c.set(x0 + 1, 0, kWood[3]);
  }
  for (int x = 2; x <= W - 3; x++) { c.set(x, 1, kWood[4]); c.set(x, 2, kWood[2]); }
  // a ladder up the right side
  for (int y = d1 + 2; y <= H - 4; y++) { c.set(W - 6, y, kWood[3]); c.set(W - 3, y, kWood[1]); }
  for (int y = d1 + 4; y <= H - 5; y += 4) { c.set(W - 5, y, kWood[3]); c.set(W - 4, y, kWood[2]); }
}

void dresser(Canvas& c) {
  const int W = c.w, base = c.h - 1;
  // a chest of three drawers with a framed mirror standing on it
  for (int y = 1; y <= 12; y++)
    for (int x = 3; x <= W - 4; x++) {
      float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f - 3), dy = (y - 6.5f) / 6.0f;
      float d = dx * dx + dy * dy;
      if (d > 1.0f) continue;
      uint32_t col;
      if (d > 0.62f) col = kWood[(dx < 0 && dy < 0.3f) ? 4 : (dx > 0.3f ? 1 : 2)];
      else {
        col = dy < -0.2f ? kSky[3] : (dy < 0.4f ? kSky[2] : kSky[1]);
        if (std::fabs(dx + dy * 0.8f + 0.2f) < 0.16f) col = kSky[4];   // a glint
      }
      c.set(x, y, col);
    }
  vline(c, W / 2 - 1, 12, 13, kWood[2]); vline(c, W / 2, 12, 13, kWood[1]);
  block34(c, 1, 14, W - 2, 3, base - 16, kWood);
  for (int d = 0; d < 3; d++) {
    int y = 18 + d * 4;
    for (int x = 2; x <= W - 3; x++) { c.set(x, y, kWood[1]); c.set(x, y + 1, kWood[3]); }
    c.set(W / 2 - 3, y + 2, kBrass[4]); c.set(W / 2 + 2, y + 2, kBrass[3]);
  }
  for (int x = 1; x <= W - 2; x++) c.set(x, base, kWood[0]);
  c.set(1, base, kWood[1]);
  // a comb and a little bottle of scent on top
  hline(c, 3, 5, 15, kBone[3]); c.set(11, 14, kPurple[3]); c.set(11, 15, kPurple[2]); c.set(11, 13, kBrass[3]);
}

// prop sizes ----------------------------------------------------------------------------------------
struct PropInfo { uint8_t w, h, frames; };
const PropInfo kPropInfo[(int)Prop::COUNT] = {
  // nature
  {40, 48, 1}, {36, 44, 1}, {26, 46, 1}, {22, 40, 1}, {26, 46, 1}, {28, 44, 1}, {30, 40, 1}, {40, 46, 1}, {32, 44, 1}, {38, 46, 1},
  {20, 16, 1}, {20, 16, 1}, {20, 16, 1}, {28, 22, 1}, {14, 10, 1}, {18, 14, 1}, {18, 14, 1}, {16, 12, 1}, {28, 12, 1},
  {16, 12, 1}, {16, 12, 1}, {16, 12, 1}, {16, 14, 1}, {16, 20, 1}, {16, 24, 1}, {16, 12, 1}, {16, 10, 1}, {18, 14, 1},
  // camp / civilisation
  {16, 14, 1}, {16, 18, 1}, {14, 18, 1}, {16, 18, 1}, {8, 20, 4}, {20, 20, 4}, {16, 22, 1}, {28, 34, 1}, {16, 16, 1}, {16, 24, 1}, {12, 32, 1}, {36, 28, 1},
  {40, 36, 1}, {24, 20, 1}, {18, 14, 1}, {36, 24, 1}, {40, 32, 4}, {20, 36, 1}, {14, 32, 4}, {24, 16, 1}, {14, 16, 1}, {24, 32, 1},
  // dungeon
  {48, 32, 1}, {16, 24, 1}, {14, 24, 1}, {16, 22, 1}, {18, 10, 1}, {20, 14, 1}, {16, 16, 1}, {16, 22, 4}, {16, 28, 1}, {12, 16, 1}, {16, 24, 1}, {32, 22, 1},
  // interior
  {20, 32, 1}, {32, 20, 1}, {12, 16, 1}, {24, 28, 1}, {32, 32, 4}, {32, 20, 1}, {32, 20, 1}, {16, 16, 1}, {12, 18, 1}, {20, 18, 4}, {28, 32, 1}, {24, 32, 1},
  // M0 wall decor
  {16, 42, 1}, {16, 42, 1}, {16, 42, 1}, {16, 42, 1}, {16, 42, 1}, {16, 42, 4}, {16, 42, 1}, {16, 42, 1}, {16, 42, 1}, {16, 42, 1}, {16, 42, 1}, {16, 42, 1},
  // M0 furniture: Cupboard Wardrobe Stool Bench Cradle SpinningWheel Loom Desk Workbench Hearth Forge
  {16, 34, 1}, {16, 36, 1}, {12, 12, 1}, {16, 12, 1}, {16, 16, 1}, {16, 22, 1}, {16, 30, 1}, {16, 20, 1}, {16, 20, 1}, {48, 64, 4}, {48, 60, 4},
  // TableSmall TableMeal TableWork TableL TableM TableR CounterL CounterM CounterR Filler
  {16, 18, 1}, {16, 18, 1}, {16, 18, 1}, {16, 18, 1}, {16, 18, 1}, {16, 18, 1}, {16, 22, 1}, {16, 22, 1}, {16, 22, 1}, {1, 1, 1},
  // M0b: StairsUp StairsDown DoorH DoorV Nightstand Washstand Oven PrepTable BottleShelf WeaponRack Lectern Candelabra
  //      DisplayTable QuenchTub Grindstone Pillar BunkBed Dresser
  {16, 40, 1}, {16, 24, 1}, {16, 36, 1}, {16, 32, 1}, {12, 16, 1}, {14, 20, 1}, {16, 28, 4}, {16, 18, 1}, {16, 34, 1}, {16, 30, 1},
  {12, 20, 1}, {10, 28, 4}, {16, 18, 1}, {16, 16, 1}, {16, 20, 1}, {16, 48, 1}, {20, 40, 1}, {16, 30, 1},
};

void m0bProp(Canvas& c, Prop p, int frame) {
  switch (p) {
    case Prop::StairsUp: stairsUpPaint(c, RoomStyle::Timber, 0); break;
    case Prop::StairsDown: stairsDownPaint(c, RoomStyle::Timber, 0); break;
    case Prop::DoorH: doorHPaint(c, RoomStyle::Timber, true); break;
    case Prop::DoorV: doorVPaint(c, RoomStyle::Timber, true); break;
    case Prop::Nightstand: nightstand(c); break;
    case Prop::Washstand: washstand(c); break;
    case Prop::Oven: oven(c, frame); break;
    case Prop::PrepTable: prepTable(c); break;
    case Prop::BottleShelf: bottleShelf(c); break;
    case Prop::WeaponRack: weaponRack(c); break;
    case Prop::Lectern: lectern(c); break;
    case Prop::Candelabra: candelabra(c, frame); break;
    case Prop::DisplayTable: displayTable(c); break;
    case Prop::QuenchTub: quenchTub(c); break;
    case Prop::Grindstone: grindstone(c); break;
    case Prop::Pillar: pillar(c); break;
    case Prop::BunkBed: bunkBed(c); break;
    case Prop::Dresser: dresser(c); break;
    default: break;
  }
}

void paintProp(Canvas& c, Prop p, int frame) {
  switch (p) {
    case Prop::OakTree: oakTree(c, kLeaf, 101, 0); break;
    case Prop::OakTree2: oakTree(c, kLeafDark, 202, 1); break;
    case Prop::AutumnTree: oakTree(c, kAutumn, 303, 2); break;
    case Prop::PineTree: pineTree(c, kPine, 11, 5, false); break;
    case Prop::PineTree2: pineTree(c, kPine, 12, 4, false); break;
    case Prop::SnowPine: pineTree(c, kPine, 13, 5, true); break;
    case Prop::BirchTree: birchTree(c, 21); break;
    case Prop::DeadTree: deadTree(c, 31); break;
    case Prop::WillowTree: willowTree(c, 41); break;
    case Prop::PalmTree: palmTree(c, 51); break;
    case Prop::Bush: bush(c, kLeaf, 61); break;
    case Prop::BerryBush:
      bush(c, kLeafDark, 62);
      for (int i = 0; i < 9; i++) {
        int x = 3 + (int)(hash3(i, 0, 63) % 14), y = 3 + (int)(hash3(i, 1, 63) % 10);
        if (solid(c, x, y) && solid(c, x + 1, y + 1)) { c.set(x, y, rgba(220, 50, 70)); c.set(x + 1, y, rgba(150, 30, 60)); c.set(x, y - 1, rgba(255, 170, 170)); }
      }
      break;
    case Prop::SnowBush: bush(c, kPine, 64); snowCap(c, 2, 65); break;
    case Prop::Boulder: rock(c, c.w * 0.5f, c.h * 0.55f, c.w * 0.46f, c.h * 0.45f, kStone, 71, 8); break;
    case Prop::Rock: rock(c, c.w * 0.5f, c.h * 0.55f, c.w * 0.44f, c.h * 0.42f, kStone, 72, 4); break;
    case Prop::MossRock:
      rock(c, c.w * 0.5f, c.h * 0.55f, c.w * 0.45f, c.h * 0.44f, kStone, 73, 5);
      for (int x = 0; x < c.w; x++)
        for (int y = 0; y < c.h; y++) if (solid(c, x, y)) {
          int d = 2 + (int)(hash3(x, 0, 74) % 3);
          for (int k = 0; k < d; k++) if (solid(c, x, y + k)) c.set(x, y + k, kMoss[k == 0 ? 3 : 2 - (k == d - 1)]);
          break;
        }
      break;
    case Prop::SnowRock: rock(c, c.w * 0.5f, c.h * 0.55f, c.w * 0.45f, c.h * 0.44f, kStone, 75, 5); snowCap(c, 2, 76); break;
    case Prop::Stump: {
      float cx = c.w * 0.5f;
      trunk(c, cx, c.h - 2, 4, 10.0f, kBark, 81);
      ellipse(c, cx, 4.5f, 5.5f, 2.6f, kWood[3]);
      ellipse(c, cx, 4.5f, 3.5f, 1.6f, kWood[2]);
      ellipse(c, cx, 4.5f, 1.5f, 0.8f, kWood[3]);
      c.set((int)cx - 3, 3, kWood[4]);
      grassTuft(c, (int)cx - 5, c.h - 2, 82, 3);
      break;
    }
    case Prop::Log: {
      int y0 = 2, y1 = c.h - 3;
      for (int y = y0; y <= y1; y++)
        for (int x = 1; x < c.w - 4; x++) {
          float v = (y + 0.5f - (y0 + y1 + 1) * 0.5f) / ((y1 - y0 + 1) * 0.5f);
          int k = lightIndex(lightAt(0, v * 0.9f) + (hashf(x / 3, y, 83) - 0.5f) * 0.3f, x, y, 0);
          c.set(x, y, kBark[k]);
        }
      ellipse(c, c.w - 4.5f, (y0 + y1 + 1) * 0.5f, 3.0f, (y1 - y0 + 1) * 0.5f, kWood[3]);
      ellipse(c, c.w - 4.5f, (y0 + y1 + 1) * 0.5f, 1.6f, (y1 - y0 + 1) * 0.3f, kWood[2]);
      c.set(c.w - 5, (y0 + y1) / 2, kWood[4]);
      c.set(8, y0 - 1, kLeaf[3]); c.set(9, y0 - 1, kLeaf[2]); c.set(9, y0 - 2, kLeaf[3]);   // sprout
      break;
    }
    case Prop::Flowers1: flowers(c, 0, 91); break;
    case Prop::Flowers2: flowers(c, 1, 92); break;
    case Prop::Flowers3: flowers(c, 2, 93); break;
    case Prop::TallGrass: tallGrass(c, 94); break;
    case Prop::Reeds: reeds(c, 95); break;
    case Prop::Cactus: cactus(c); break;
    case Prop::Mushrooms: mushrooms(c); break;
    case Prop::LilyPad: lilyPad(c); break;
    case Prop::Fern: fern(c, 96); break;
    case Prop::Chest: chest(c, false); break;
    case Prop::ChestOpen: chest(c, true); break;
    case Prop::Barrel: barrel(c, 0, 0, c.w, c.h - 1); break;
    case Prop::Crate: crate(c); break;
    case Prop::Torch: torch(c, frame); break;
    case Prop::Campfire: campfire(c, frame); break;
    case Prop::Signpost: signpost(c); break;
    case Prop::Well: well(c); break;
    case Prop::FenceH: fence(c, false); break;
    case Prop::FenceV: fence(c, true); break;
    case Prop::Lamppost: lamppost(c); break;
    case Prop::Tent: tent(c); break;
    case Prop::MarketStall: marketStall(c); break;
    case Prop::Haystack: haystack(c); break;
    case Prop::Anvil: anvil(c); break;
    case Prop::Cart: cart(c); break;
    case Prop::Fountain: fountain(c, frame); break;
    case Prop::Statue: statue(c); break;
    case Prop::Banner: banner(c, frame); break;
    case Prop::Woodpile: woodpile(c); break;
    case Prop::Gravestone: gravestone(c); break;
    case Prop::Shrine: shrine(c); break;
    case Prop::CaveEntrance: caveEntrance(c); break;
    case Prop::Ladder: ladder(c); break;
    case Prop::Stalagmite: stalagmite(c); break;
    case Prop::Crystal: crystalCluster(c); break;
    case Prop::Bones: bones(c); break;
    case Prop::SkullPile: skullPile(c); break;
    case Prop::Cobweb: cobweb(c); break;
    case Prop::Brazier: brazier(c, frame); break;
    case Prop::Coffin: coffin(c); break;
    case Prop::Urn: urn(c); break;
    case Prop::IronDoor: ironDoor(c); break;
    case Prop::Altar: altar(c); break;
    case Prop::Bed: bed(c); break;
    case Prop::Table: table(c); break;
    case Prop::Chair: chair(c); break;
    case Prop::Shelf: shelfWithJars(c); break;
    case Prop::Fireplace: fireplace(c, frame); break;
    case Prop::Rug: rug(c); break;
    case Prop::Counter: counter(c); break;
    case Prop::Barrel2: kegRack(c); break;
    case Prop::PlantPot: plantPot(c); break;
    case Prop::Cauldron: cauldron(c, frame); break;
    case Prop::Bookshelf: bookshelf(c); break;
    case Prop::Throne: throne(c); break;
    case Prop::Tapestry: { Canvas t(16, 30); tapestry(t); blit(c, t, 0, 3); break; }
    case Prop::WallShelf: { Canvas t(16, 30); wallShelf(t); blit(c, t, 0, 3); break; }
    case Prop::HerbBundle: { Canvas t(16, 30); herbBundle(t); blit(c, t, 0, 3); break; }
    case Prop::Antlers: { Canvas t(16, 30); antlers(t); blit(c, t, 0, 3); break; }
    case Prop::Painting: { Canvas t(16, 30); painting(t); blit(c, t, 0, 3); break; }
    case Prop::Sconce: { Canvas t(16, 30); sconce(t, frame); blit(c, t, 0, 3); break; }
    case Prop::Window: { Canvas t(16, 30); windowProp(t); blit(c, t, 0, 3); break; }
    case Prop::WallShield: { Canvas t(16, 30); wallShield(t); blit(c, t, 0, 3); break; }
    case Prop::ToolRack: { Canvas t(16, 30); toolRack(t); blit(c, t, 0, 3); break; }
    case Prop::PanRack: { Canvas t(16, 30); panRack(t); blit(c, t, 0, 3); break; }
    case Prop::Wreath: { Canvas t(16, 30); wreath(t); blit(c, t, 0, 3); break; }
    case Prop::HolySymbol: { Canvas t(16, 30); holySymbol(t); blit(c, t, 0, 3); break; }
    case Prop::Cupboard: cupboard(c); break;
    case Prop::Wardrobe: wardrobe(c); break;
    case Prop::Stool: stool(c); break;
    case Prop::Bench: bench(c); break;
    case Prop::Cradle: cradle(c); break;
    case Prop::SpinningWheel: spinningWheel(c); break;
    case Prop::Loom: loom(c); break;
    case Prop::Desk: desk(c); break;
    case Prop::Workbench: workbench(c); break;
    case Prop::Hearth: hearth(c, frame); break;
    case Prop::Forge: forge(c, frame); break;
    case Prop::TableSmall: tableSmall(c, 0); break;
    case Prop::TableMeal: tableSmall(c, 1); break;
    case Prop::TableWork: tableSmall(c, 2); break;
    case Prop::TableL: tableSeg(c, 0); break;
    case Prop::TableM: tableSeg(c, 1); break;
    case Prop::TableR: tableSeg(c, 2); break;
    case Prop::CounterL: counterSeg(c, 0); break;
    case Prop::CounterM: counterSeg(c, 1); break;
    case Prop::CounterR: counterSeg(c, 2); break;
    case Prop::Filler: filler(c); break;
    default: if ((int)p >= (int)Prop::StairsUp) m0bProp(c, p, frame); break;
  }
}

}  // namespace

int propW(Prop p) { return (int)p < (int)Prop::COUNT ? kPropInfo[(int)p].w : 16; }
int propH(Prop p) { return (int)p < (int)Prop::COUNT ? kPropInfo[(int)p].h : 16; }
int propFrames(Prop p) { return (int)p < (int)Prop::COUNT ? kPropInfo[(int)p].frames : 1; }

Canvas propSprite(Prop p) {
  const int w = propW(p), h = propH(p), n = propFrames(p);
  Canvas sheet(w * n, h);
  for (int f = 0; f < n; f++) {
    Canvas cell(w, h);
    paintProp(cell, p, f);
    if (p != Prop::Cobweb && p != Prop::Rug) outline(cell);
    place(sheet, cell, f, 0);
  }
  return sheet;
}

Canvas interiorPiece(uint32_t key) {
  int kind = (int)(key & 255), style = (int)((key >> 8) & 255), a = (int)((key >> 16) & 255), b = (int)((key >> 24) & 255);
  switch ((Piece)kind) {
    case Piece::Floor: return floorPiece((FloorStyle)(style & 7), style >> 3, a, b);
    case Piece::BackWall: return backWallPiece((RoomStyle)(style & 15), style & 240, a);
    case Piece::Cap: return capPiece((RoomStyle)(style & 7), a, b & 127, (style >> 3) & 7, ((style & 64) ? 1 : 0) | ((style & 128) ? 2 : 0) | ((b & 128) ? 4 : 0));
    case Piece::PartFace: return partFacePiece((RoomStyle)(style & 7), (style >> 3) & 3, (style >> 5) & 3, a, b);
    case Piece::PartCap: return partCapPiece((RoomStyle)(style & 7), style >> 3, a, b);
    case Piece::Sill: return sillPiece((RoomStyle)(style & 7), a);
    case Piece::Styled: return styledPiece((RoomStyle)(style & 7), a, b);
    case Piece::LowDecor: return lowDecorPiece(style);
    case Piece::Shadow: return shadowPiece(a, b);
    case Piece::Rug: return rugPiece(style, a);
    case Piece::Clutter: return clutterPiece(style);
    case Piece::Door: return doorPiece((RoomStyle)(style & 7));
    default: return Canvas(1, 1);
  }
}

}  // namespace art
