// M9 "Arcana & Mastery" (VISION_PLAN 15.5, 15.29.7: "surfaces and combos read beautifully"): the spell surfaces on the
// ground (water, ice, oil, burning ground, poison, shocked water, steam, blood), animated pixel art with organic borders
// that join tile to tile and kind to kind. SPELLFX lane.
//
// Each surface tile is baked once into four animation frames (16 x 16 each) in a shared atlas, re-baked only when its
// neighbourhood changes (magic::Field::serial; at most a budget of tiles a frame, so a big spell never spikes a phone
// frame). Its pixels come from a metaball field over the 3 x 3 tiles round it (each covered tile a ball of radius 16 px
// at its centre; a lone tile is a round pool of radius ~6, neighbours merge into one smooth shape with no seam, since
// two tiles' fields agree on their shared edge), the threshold wobbled by noise in GLOBAL pixel coordinates (organic
// edges, continuous across tiles), the kind of each pixel the strongest field (two kinds meet along a natural line).
// The 3/4 light from the top left shapes every rim: a liquid's top-left inner bank in shade and its bottom-right bank
// lit, the ice sheet's top-left edge lit and its two-pixel side face below it in shade, a damp halo round water.
// Over the baked ground, drawn live after the lighting: flame tongues on burning ground, steam and poison clouds, arcs
// on shocked water, glints on ice; burning and shocked tiles also re-add their own pixels so they glow at night.
#pragma once
#include "rpg/view/magic_draw.h"

namespace mfx {

constexpr int SURF_FR = 4, SURF_AW = 512, SURF_AH = 512, SURF_COLS = SURF_AW / (16 * SURF_FR), SURF_SLOTS = SURF_COLS * (SURF_AH / 16);

// the colour of a covered pixel. e: edges (bit 0 uncovered above, 1 below, 2 left, 3 right; bits 4..7 the same two
// pixels away), din: distance in (1..4), gx / gy: global pixel, f: the frame
inline uint32_t surfInside(int kind, int e, int din, int gx, int gy, int f, uint32_t seed) {
  const bool upE = e & 1, dnE = e & 2, lfE = e & 4, rtE = e & 8, up2 = e & 16, dn2 = e & 32, lf2 = e & 64, rt2 = e & 128;
  const float n = vnoise((float)gx * 0.22f, (float)gy * 0.3f, seed);
  auto cell = [&](float cs) {   // cellular cracks: F2 - F1 on a jittered grid
    const float px = (float)gx / cs, py = (float)gy / cs;
    const int cx = (int)std::floor(px), cy = (int)std::floor(py);
    float f1 = 9, f2 = 9;
    for (int j = -1; j <= 1; j++)
      for (int i = -1; i <= 1; i++) {
        const uint32_t h = hash2(cx + i, cy + j, seed ^ 0xC311u);
        const float dx = (float)(cx + i) + hf(h) - px, dy = (float)(cy + j) + hf(h >> 16) - py;
        const float d = std::sqrt(dx * dx + dy * dy);
        if (d < f1) { f2 = f1; f1 = d; } else if (d < f2) f2 = d;
      }
    return f2 - f1;
  };
  switch ((Surface)kind) {
    case Surface::Water: case Surface::Shock: {
      const bool shock = kind == (int)Surface::Shock;
      Color c = shock ? Color(0.20f, 0.24f, 0.56f) : Color(0.17f, 0.36f, 0.58f);
      if (din >= 4) c = mulC(c, 0.88f);                               // deeper in the middle
      if (upE || lfE) c = mulC(c, 0.62f);                             // the top-left bank's shade
      else if (up2 || lf2) c = mulC(c, 0.8f);
      if (dnE || rtE) c = lerpC(c, shock ? Color(0.6f, 0.65f, 1.0f) : Color(0.55f, 0.76f, 0.88f), 0.6f);   // the lit far bank
      const float rip = vnoise(((float)gx + (float)f * 2.5f) * 0.2f, (float)gy * 0.55f, seed ^ 0x51u);
      if (din >= 2 && rip > 0.74f) c = lerpC(c, shock ? Color(0.75f, 0.8f, 1.0f) : Color(0.62f, 0.82f, 0.94f), 0.7f);   // ripples
      if (din >= 2 && (hash2(gx, gy, seed + (uint32_t)f * 7919u) & 1023) < 5) c = Color(1, 1, 1);   // a sparkle
      if (shock) {   // arcs: squiggly isolines of a noise that changes every frame
        const float a = vnoise((float)gx * 0.15f, (float)gy * 0.2f, seed + 101u * (uint32_t)(f + 1));
        if (std::fabs(a - 0.5f) < 0.022f) c = Color(0.86f, 0.9f, 1.0f);
        else if (std::fabs(a - 0.5f) < 0.05f) c = lerpC(c, Color(0.5f, 0.55f, 1.0f), 0.6f);
      }
      const int al = (upE || dnE || lfE || rtE) ? 235 : 210;
      return pk(withA(c, al / 255.0f));
    }
    case Surface::Ice: {
      Color c = Color(0.70f, 0.87f, 0.95f);
      if (n < 0.35f) c = Color(0.62f, 0.81f, 0.93f);
      const float cr = cell(6.5f);
      if (cr < 0.07f && din >= 2) c = Color(0.36f, 0.56f, 0.78f);       // cracks
      else if (cr < 0.13f && din >= 2) c = Color(0.86f, 0.95f, 1.0f);   // their lit lip
      if (upE || lfE) c = Color(0.96f, 0.99f, 1.0f);                  // the lit top-left edge
      else if (dnE || rtE) c = Color(0.5f, 0.7f, 0.88f);              // the shaded bottom-right edge
      const int band = ((gx + gy) - f * 12) % 64;
      if (band >= 0 && band < 2 && din >= 2) c = lerpC(c, Color(1, 1, 1), 0.7f);   // a glint sweeping across
      if ((hash2(gx, gy, seed + 17u + (uint32_t)f * 31u) & 511) < 3) c = Color(1, 1, 1);
      return pk(withA(c, 0.93f));
    }
    case Surface::Oil: {
      Color c = Color(0.09f, 0.07f, 0.1f);
      const float m = vnoise((float)gx * 0.14f, (float)gy * 0.22f, seed ^ 0x0111u);
      if (m > 0.55f && din >= 2) {   // the sheen: muted rainbow bands drifting across
        const float hue = std::fmod(vnoise((float)gx * 0.08f, (float)gy * 0.12f, seed) * 2.0f + (float)(gx + gy) * 0.02f + (float)f * 0.25f, 1.0f);
        static const Color bands[4] = {Color(0.36f, 0.16f, 0.44f), Color(0.14f, 0.34f, 0.44f), Color(0.42f, 0.38f, 0.14f), Color(0.42f, 0.16f, 0.24f)};
        const Color b = bands[(int)(hue * 4) & 3];
        c = lerpC(c, b, 0.55f * (m - 0.55f) * 2.2f + 0.2f);
      }
      if (upE || lfE) c = Color(0.36f, 0.33f, 0.4f);                 // the glossy lit lip
      if (dnE || rtE) c = Color(0.04f, 0.03f, 0.05f);
      return pk(withA(c, 0.9f));
    }
    case Surface::Fire: {   // burning ground: charred earth split by glowing seams that pulse
      Color c = n > 0.5f ? Color(0.17f, 0.08f, 0.06f) : Color(0.12f, 0.06f, 0.05f);
      const float cr = cell(7.0f);
      const float heat = vnoise((float)gx * 0.25f, (float)gy * 0.25f + (float)f * 0.8f, seed ^ 0xF17Eu);
      if (cr < 0.08f) c = heat > 0.55f ? Color(1.0f, 0.86f, 0.42f) : heat > 0.3f ? Color(0.98f, 0.52f, 0.14f) : Color(0.7f, 0.2f, 0.06f);
      else if (cr < 0.15f) c = lerpC(c, Color(0.5f, 0.13f, 0.05f), 0.6f * heat);
      else if (n > 0.7f) c = Color(0.24f, 0.2f, 0.18f);   // ash
      if (upE || dnE || lfE || rtE) c = heat > 0.45f ? Color(0.95f, 0.45f, 0.12f) : Color(0.5f, 0.14f, 0.05f);   // the ember rim
      return pk(withA(c, 0.95f));
    }
    case Surface::Poison: {
      Color c = n > 0.5f ? Color(0.3f, 0.46f, 0.14f) : Color(0.25f, 0.4f, 0.12f);
      if (n > 0.78f) c = Color(0.48f, 0.64f, 0.22f);                  // scum
      if (upE || lfE) c = Color(0.16f, 0.27f, 0.08f);
      if (dnE || rtE) c = Color(0.56f, 0.74f, 0.28f);
      // bubbles: each on a 6-px cell, growing over the frames and popping
      const int bx = (int)std::floor((float)gx / 6), by = (int)std::floor((float)gy / 6);
      const uint32_t h = hash2(bx, by, seed ^ 0xB0BBu);
      if ((h & 3) == 0 && din >= 2) {
        const float cx = (float)bx * 6 + 1 + hf(h >> 4) * 4, cy = (float)by * 6 + 1 + hf(h >> 12) * 4;
        const int ph = (f + (int)(h >> 20)) % SURF_FR;
        const float r = 0.6f + (float)ph * 0.6f, d = std::sqrt(((float)gx - cx) * ((float)gx - cx) + ((float)gy - cy) * ((float)gy - cy));
        if (ph < 3 && std::fabs(d - r) < 0.55f) c = Color(0.74f, 0.92f, 0.42f);
        if (ph < 3 && d < 0.6f && ph > 0) c = Color(0.9f, 1.0f, 0.65f);
      }
      return pk(withA(c, 0.85f));
    }
    case Surface::Steam: {   // a pale condensation film, beaded
      Color c(0.82f, 0.86f, 0.9f);
      if ((hash2(gx, gy, seed) & 63) < 3) c = Color(1, 1, 1);
      const float a = (upE || dnE || lfE || rtE) ? 0.22f : 0.36f;
      return pk(withA(c, a));
    }
    case Surface::Blood: {   // (M9 fixer, review: "the blood pool is one flat fill with square specks") a viscous pool:
      // mottled and deeper toward the middle, a shaded top-left bank, a thin lit far rim, slow glossy highlights
      // drifting over it (soft streaks, a white-hot glint at their hearts) and dark clots
      const float m = vnoise((float)gx * 0.12f, (float)gy * 0.18f, seed ^ 0xB10Du);
      Color c = lerpC(Color(0.47f, 0.06f, 0.07f), Color(0.22f, 0.02f, 0.04f), std::clamp(0.18f * (float)din + (m - 0.5f) * 0.9f, 0.0f, 1.0f));
      if (din >= 3 && cell(5.0f) < 0.07f) c = mulC(c, 0.68f);         // clots
      if (upE || lfE) c = Color(0.19f, 0.02f, 0.03f);                  // the top-left bank's shade
      else if (up2 || lf2) c = mulC(c, 0.8f);
      if (dnE || rtE) c = lerpC(c, Color(0.74f, 0.24f, 0.23f), 0.7f);  // the lit far rim
      const float sh = vnoise(((float)gx + (float)f * 0.8f) * 0.09f, (float)gy * 0.34f, seed ^ 0x5EE1u);
      if (din >= 2 && sh > 0.66f) c = lerpC(c, Color(0.74f, 0.27f, 0.28f), std::min(0.7f, (sh - 0.66f) * 2.4f));   // the sheen
      if (din >= 3 && sh > 0.9f) c = lerpC(c, Color(0.98f, 0.74f, 0.7f), 0.6f);   // its glint
      return pk(withA(c, din <= 1 ? 0.84f : 0.94f));
    }
    default: return 0;
  }
  (void)dn2; (void)rt2;
}

// bake one tile's four frames into dst at (dx, dy) (frames side by side). kinds: the 3 x 3 neighbourhood (0 none),
// gtx / gty its global tile
inline void bakeSurfaceTile(Canvas& dst, int dx, int dy, const uint8_t kinds[3][3], int gtx, int gty) {
  constexpr int M = 3, G = 16 + 2 * M;   // a margin round the tile for the rim tests
  constexpr float R2 = 16.0f * 16.0f;
  uint8_t kg[G][G];
  for (int y = 0; y < G; y++)
    for (int x = 0; x < G; x++) {
      const int gx = gtx * 16 + x - M, gy = gty * 16 + y - M;
      // a domain warp in GLOBAL coordinates (continuous across tiles; at most 7 px, so the 3 x 3 neighbourhood still
      // decides it): the shores meander and two kinds meet along a ragged natural line, never a tile's straight side
      const float wx = 11.0f * (vnoise((float)gx / 9.0f, (float)gy / 9.0f, 0xED6Eu) - 0.5f) + 3.0f * (vnoise((float)gx / 3.0f, (float)gy / 3.0f, 0x1D6Eu) - 0.5f);
      const float wy = 11.0f * (vnoise((float)gx / 9.0f, (float)gy / 9.0f, 0xAD7Fu) - 0.5f) + 3.0f * (vnoise((float)gx / 3.0f, (float)gy / 3.0f, 0x3D7Fu) - 0.5f);
      const float px = (float)(x - M) + 0.5f + wx, py = (float)(y - M) + 0.5f + wy;
      float tot = 0, nearD = 1e9f;
      int best = 0;
      for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++) {
          const int k = kinds[j][i];
          if (!k) continue;
          const float ddx = px - (8.0f + 16.0f * (float)(i - 1)), ddy = py - (8.0f + 16.0f * (float)(j - 1));
          const float d2 = ddx * ddx + ddy * ddy;
          if (d2 < nearD) { nearD = d2; best = k; }
          const float q = 1.0f - d2 / R2;
          if (q <= 0) continue;
          tot += q * q;
        }
      const float t = 0.7f;
      kg[y][x] = tot > t ? (uint8_t)best : 0;
    }
  auto cov = [&](int x, int y) { return x >= 0 && y >= 0 && x < G && y < G && kg[y][x] != 0; };
  for (int f = 0; f < SURF_FR; f++)
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 16; x++) {
        const int X = x + M, Y = y + M;
        const int gx = gtx * 16 + x, gy = gty * 16 + y;
        uint32_t out = 0;
        if (kg[Y][X]) {
          const int k = kg[Y][X];
          auto other = [&](int xx, int yy) { return !cov(xx, yy) || kg[yy][xx] != k; };
          const int e = (other(X, Y - 1) ? 1 : 0) | (other(X, Y + 1) ? 2 : 0) | (other(X - 1, Y) ? 4 : 0) | (other(X + 1, Y) ? 8 : 0) |
                        (other(X, Y - 2) ? 16 : 0) | (other(X, Y + 2) ? 32 : 0) | (other(X - 2, Y) ? 64 : 0) | (other(X + 2, Y) ? 128 : 0);
          int din = 4;
          for (int r = 1; r <= 3 && din == 4; r++)
            if (!cov(X, Y - r) || !cov(X, Y + r) || !cov(X - r, Y) || !cov(X + r, Y)) din = r;
          out = surfInside(k, e & 255, din, gx, gy, f, 0x5F00u + (uint32_t)k * 977u);
          // where two kinds meet: a darker seam on the shaded side
          if (((e & 2) && cov(X, Y + 1)) || ((e & 8) && cov(X + 1, Y))) {
            const Color c = cu(out);
            out = pk(withA(mulC(c, 0.72f), c.a));
          }
        } else {
          // outside: ice stands up two pixels (its shaded side face), liquids darken the ground round them (damp)
          if (cov(X, Y - 1) && kg[Y - 1][X] == (uint8_t)Surface::Ice) out = rgba(84, 128, 176, 240);
          else if (cov(X, Y - 2) && kg[Y - 2][X] == (uint8_t)Surface::Ice) out = rgba(58, 92, 138, 230);
          else if (cov(X, Y - 3) && kg[Y - 3][X] == (uint8_t)Surface::Ice) out = rgba(10, 18, 30, 70);   // its shadow
          else {
            int near = 0;
            for (int j = -2; j <= 2 && !near; j++)
              for (int i = -2; i <= 2; i++) if (cov(X + i, Y + j)) { near = kg[Y + j][X + i]; break; }
            if (near == (int)Surface::Water || near == (int)Surface::Shock || near == (int)Surface::Blood || near == (int)Surface::Oil || near == (int)Surface::Poison)
              out = rgba(8, 10, 16, near == (int)Surface::Oil ? 80 : 56);
            else if (near == (int)Surface::Fire) out = rgba(14, 8, 6, 90);   // soot
          }
        }
        dst.set(dx + f * 16 + x, dy + y, out);
      }
}

struct SurfLayer {
  Sheet atlas;
  struct Slot { int16_t tx = 0, ty = 0; uint64_t sig = 0; bool used = false, baked = false; };
  Slot slots[SURF_SLOTS];
  std::unordered_map<uint32_t, int> at;   // tile -> slot
  uint32_t serial = 0xFFFFFFFFu;
  int ox = 1 << 30, oy = 1 << 30;
  int baked = 0, pending = 0, full = 0;   // stats: tiles baked so far, waiting, refused (atlas full)
  SurfLayer() { atlas.c = Canvas(SURF_AW, SURF_AH); atlas.stream = true; }
  static uint32_t key(int tx, int ty) { return (uint32_t)(uint16_t)tx | (uint32_t)(uint16_t)ty << 16; }
  // the cells drawn this frame: every surface tile and the empty cells round them (a pool's warped shore spills into
  // its neighbours' ground: no straight cut along the tile grid), with the alpha they fade by
  struct Cell { int16_t tx, ty; float a; };
  std::vector<Cell> cells;
  static float alphaOf(const magic::SurfaceTile& t) { return sat(t.age / 0.35f) * sat(t.life / 1.2f); }
  // follow the field: free the slots of cells gone, bake (up to budget) the cells whose neighbourhood changed
  void sync(const magic::Field& F, int gox, int goy, int budget) {
    if (gox != ox || goy != oy) {   // a window shift: the global noise moved under the tiles: bake again
      ox = gox; oy = goy;
      for (Slot& s : slots) s.baked = false, s.sig = 0;
      serial = 0xFFFFFFFFu;
    }
    cells.clear();
    if (F.tiles.empty() && at.empty()) return;
    std::unordered_map<uint32_t, uint8_t> kindAt;
    std::unordered_map<uint32_t, float> alphaAt;   // a cell's alpha: its own tile's, or its brightest neighbour's
    kindAt.reserve(F.tiles.size() * 2);
    for (const magic::SurfaceTile& t : F.tiles) kindAt[key(t.tx, t.ty)] = (uint8_t)t.kind;
    for (const magic::SurfaceTile& t : F.tiles) {
      const float a = alphaOf(t);
      for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
          const uint32_t k = key(t.tx + i, t.ty + j);
          if ((i || j) && kindAt.count(k)) continue;
          float& v = alphaAt[k];
          v = (i || j) ? std::max(v, a) : a;
        }
    }
    if (F.serial != serial) {
      serial = F.serial;
      for (int i = 0; i < SURF_SLOTS; i++)
        if (slots[i].used && !alphaAt.count(key(slots[i].tx, slots[i].ty))) { at.erase(key(slots[i].tx, slots[i].ty)); slots[i] = Slot(); }
    }
    pending = 0;
    int done = 0;
    cells.reserve(alphaAt.size());
    for (const auto& kv : alphaAt) {
      const int tx = (int16_t)(kv.first & 0xFFFF), ty = (int16_t)(kv.first >> 16);
      uint8_t kinds[3][3];
      uint64_t sig = 0;
      for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++) {
          auto it = kindAt.find(key(tx + i - 1, ty + j - 1));
          kinds[j][i] = it == kindAt.end() ? 0 : it->second;
          sig = sig << 4 | kinds[j][i];
        }
      sig |= 1ull << 63;
      int s;
      auto it = at.find(kv.first);
      if (it == at.end()) {
        s = -1;
        for (int i = 0; i < SURF_SLOTS; i++) if (!slots[i].used) { s = i; break; }
        if (s < 0) { full++; continue; }
        slots[s].used = true; slots[s].tx = (int16_t)tx; slots[s].ty = (int16_t)ty; slots[s].baked = false; slots[s].sig = 0;
        at[kv.first] = s;
      } else s = it->second;
      if (!(slots[s].baked && slots[s].sig == sig)) {
        if (done >= budget) { pending++; continue; }
        bakeSurfaceTile(atlas.c, (s % SURF_COLS) * 16 * SURF_FR, (s / SURF_COLS) * 16, kinds, tx + ox, ty + oy);
        slots[s].sig = sig; slots[s].baked = true;
        atlas.dirty = true;
        done++; baked++;
      }
      cells.push_back(Cell{(int16_t)tx, (int16_t)ty, kv.second});
    }
    // a stable order (the map's is not): drawn row by row
    std::sort(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) { return a.ty != b.ty ? a.ty < b.ty : a.tx < b.tx; });
  }
  int frame(float time) const { return (int)(time * 6.0f) % SURF_FR; }
  // the baked ground (under the bodies, before the lighting); returns tiles drawn
  template <class Pt> int draw(Pt& P, const Ctx& C, const magic::Field& F) {
    (void)F;
    int n = 0;
    const int fr = frame(C.t);
    for (const Cell& c : cells) {
      const float x = (float)c.tx * 16.0f - C.cam.x, y = (float)c.ty * 16.0f - C.cam.y;
      if (x < -16 || y < -16 || x > C.W || y > C.H) continue;
      auto it = at.find(key(c.tx, c.ty));
      if (it == at.end() || !slots[it->second].baked) continue;
      const int s = it->second;
      P.blit(atlas, (s % SURF_COLS) * 16 * SURF_FR + fr * 16, (s / SURF_COLS) * 16, 16, 16, std::floor(x), std::floor(y), 16, 16, Color(1, 1, 1, c.a), 0, false);
      n++;
    }
    return n;
  }
  // over the bodies, after the lighting: flames, clouds, arcs, glints; burning and shocked tiles glow by night
  //   (M9 fixer) hide (index-aligned with F.tiles, may be null): 1 = the tile stands behind a tall prop. A pass draws
  //   only the tiles whose flag equals `behind` (the ground pass draws the hidden ones under the scene, the over pass
  //   the rest), so a tree or a stall in front covers the flames behind it.
  template <class Pt> void drawOver(Pt& P, World& W, const Ctx& C, const magic::Field& F, const uint8_t* hide = nullptr, bool behind = false) {
    const int fr = frame(C.t);
    Look FL = fireLook();
    Sheet& rs = W.rampSheet(FL);
    for (size_t ti = 0; ti < F.tiles.size(); ti++) {
      const magic::SurfaceTile& t = F.tiles[ti];
      if ((hide && hide[ti] != 0) != behind) continue;
      const float x = std::floor((float)t.tx * 16.0f - C.cam.x), y = std::floor((float)t.ty * 16.0f - C.cam.y);
      if (x < -24 || y < -40 || x > C.W + 8 || y > C.H + 8) continue;
      const float a = alphaOf(t);
      const uint32_t h = t.seed ? t.seed : hash2(t.tx, t.ty, 0x7u);
      auto it = at.find(key(t.tx, t.ty));
      const bool baked = it != at.end() && slots[it->second].baked;
      switch (t.kind) {
        case Surface::Fire: {
          if (baked && C.dark > 0.05f) {
            const int s = it->second;
            P.blit(atlas, (s % SURF_COLS) * 16 * SURF_FR + fr * 16, (s / SURF_COLS) * 16, 16, 16, x, y, 16, 16, Color(1, 1, 1, 0.55f * a * C.dark), 1, false);
          }
          gglow(P, W, x + 8, y + 9, 12, Color(1.0f, 0.45f, 0.12f, 0.28f * a));
          const int nf = 1 + (int)t.power;
          for (int i = 0; i < nf && i < 3; i++) {
            const uint32_t hh = hmix(h, (uint32_t)i);
            flame(P, rs, i == 0 ? 1 + (t.power > 2 ? 1 : 0) : 0, (int)(C.t * 14 + hf(hh) * 8), x + 3 + hf(hh >> 4) * 10, y + 5 + hf(hh >> 9) * 9, 0.92f * a);
          }
          break;
        }
        case Surface::Shock: {
          if (baked && C.dark > 0.05f) {
            const int s = it->second;
            P.blit(atlas, (s % SURF_COLS) * 16 * SURF_FR + fr * 16, (s / SURF_COLS) * 16, 16, 16, x, y, 16, 16, Color(1, 1, 1, 0.45f * a * C.dark), 1, false);
          }
          const uint32_t st = hmix(h, (uint32_t)(C.t / 0.07f));
          if ((st & 3) == 0) {
            const Vec2 p0(x + hf(st >> 2) * 16, y + hf(st >> 8) * 16), p1(x + hf(st >> 14) * 16 + 6, y + hf(st >> 20) * 16 - 2);
            bolt(P, W, p0, p1, st, Color(0.5f, 0.55f, 1.0f), Color(0.3f, 0.3f, 0.85f), Color(0.92f, 0.95f, 1.0f), 0.9f * a, 0, 0.5f);
          }
          break;
        }
        case Surface::Steam: case Surface::Poison: {   // clouds drifting up off it, looping
          const bool steam = t.kind == Surface::Steam;
          for (int i = 0; i < 2; i++) {
            const uint32_t hh = hmix(h, 7u + (uint32_t)i);
            const float ph = std::fmod(C.t * (steam ? 0.45f : 0.25f) + hf(hh), 1.0f);
            const float cx = x + 8 + (hf(hh >> 4) - 0.5f) * 10 + std::sin(C.t + (float)i * 2) * 2;
            const float cy = y + 8 - ph * (steam ? 26.0f : 8.0f);
            const float sc = (steam ? 1.0f : 0.9f) + ph * 0.8f;
            const float w = std::floor(24 * sc), hgt = std::floor(16 * sc);
            const float ca = a * std::sin(ph * kPi) * (steam ? 0.45f : 0.38f);
            const Color col = steam ? mulC(Color(0.92f, 0.94f, 0.97f), 0.45f + 0.55f * C.lightK) : mulC(Color(0.5f, 0.75f, 0.25f), 0.45f + 0.55f * C.lightK);
            P.blit(W.common(), iCloud(i).x, iCloud(i).y, 24, 16, cx - w * 0.5f, cy - hgt * 0.5f, w, hgt, withA(col, ca), 0, (hh & 1) != 0);
          }
          break;
        }
        case Surface::Ice: {
          const float tw = std::sin(C.t * 3 + hf(h) * 6);
          if (tw > 0.9f) icon(P, W, I_STARS, x + 2 + hf(h >> 6) * 10, y + 2 + hf(h >> 12) * 9, Color(1, 1, 1, (tw - 0.9f) * 10 * a), 1);
          break;
        }
        case Surface::Water: {
          const float ph = std::fmod(C.t * 0.5f + hf(h), 1.0f);
          if ((h & 3) == 0) ring(P, x + 8, y + 8, 1 + ph * 6, (1 + ph * 6) * 0.45f, Color(0.75f, 0.9f, 1.0f, 0.35f * (1 - ph) * a * C.lightK), false);
          break;
        }
        default: break;
      }
    }
  }
  template <class F> int lights(const magic::Field& Fd, float time, F light) {
    int n = 0, i = 0;
    const int step = Fd.tiles.size() > 24 ? 2 : 1;
    for (const magic::SurfaceTile& t : Fd.tiles) {
      if (t.kind != Surface::Fire && t.kind != Surface::Shock) continue;
      if ((i++ % step) != 0 || n >= 32) continue;
      const Vec2 p((float)t.tx * 16 + 8, (float)t.ty * 16 + 8);
      const float a = alphaOf(t);
      if (t.kind == Surface::Fire) light(p, 34.0f * (float)step, Color(1.0f, 0.55f, 0.22f), (0.55f + 0.15f * std::sin(time * 11 + (float)t.tx)) * a);
      else light(p, 30.0f * (float)step, Color(0.5f, 0.55f, 1.0f), (0.4f + 0.3f * std::sin(time * 23 + (float)t.ty)) * a);
      n++;
    }
    return n;
  }
};

}  // namespace mfx
