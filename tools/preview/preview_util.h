// Shared helpers for the per-lane art galleries in tools/preview/*.cpp (each .cpp there becomes its own executable,
// linked with rpg_art; see CMakeLists.txt). A Board is a grass-coloured canvas to place sprites and labels on;
// savePng writes it scaled up with nearest neighbour. Galleries write to %TEMP% paths given on the command line.
#pragma once
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "engine/pix.h"
#include "rpg/art.h"

namespace pv {
#include "engine/font5x7.h"
}

namespace {

constexpr int kScale = 3;

uint32_t hash2(int x, int y) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

// A board is a canvas with a grass background onto which sprites and labels are placed.
struct Board {
  Canvas c;
  Board(int w, int h) : c(w, h) {
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        uint32_t n = hash2(x / 2, y / 2) % 100;
        uint32_t col = rgba(86, 140, 62);
        if (n < 18) col = rgba(78, 130, 58);
        else if (n > 92) col = rgba(100, 154, 68);
        uint32_t t = hash2(x, y * 7 + 3) % 400;
        if (t == 0) { c.set(x, y, rgba(118, 170, 76)); c.set(x, y - 1, rgba(118, 170, 76)); continue; }
        c.set(x, y, col);
      }
  }
  void put(const Canvas& s, int x, int y) {
    for (int j = 0; j < s.h; j++)
      for (int i = 0; i < s.w; i++) {
        uint32_t p = s.get(i, j);
        int a = (int)(p >> 24);
        if (!a) continue;
        if (a == 255) { c.set(x + i, y + j, p); continue; }
        c.set(x + i, y + j, art::mix(c.get(x + i, y + j), p | 0xFF000000u, a / 255.0f));
      }
  }
  // a darker strip behind a sprite so its canvas bounds are visible
  void frame(int x, int y, int w, int h) {
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++)
        if (i == 0 || j == 0 || i == w - 1 || j == h - 1) {
          uint32_t p = c.get(x + i, y + j);
          c.set(x + i, y + j, art::shade(p, 0.82f));
        }
  }
  void text(int x, int y, const std::string& s, uint32_t col = rgba(255, 250, 230)) {
    for (char ch : s) {
      int gi = pv::font5x7::glyphIndex(ch);
      if (gi >= 0)
        for (int r = 0; r < 7; r++)
          for (int k = 0; k < 5; k++)
            if (pv::font5x7::kFont[gi].rows[r] & (0x10 >> k)) {
              c.set(x + k + 1, y + r + 1, rgba(30, 40, 30));
              c.set(x + k, y + r, col);
            }
      x += 6;
    }
  }
};

bool savePng(const Canvas& src, const std::string& path, int scale) {
  Canvas big(src.w * scale, src.h * scale);
  for (int y = 0; y < big.h; y++)
    for (int x = 0; x < big.w; x++) big.px[(size_t)y * big.w + x] = src.get(x / scale, y / scale) | 0xFF000000u;
  SDL_Surface* s = SDL_CreateSurfaceFrom(big.w, big.h, SDL_PIXELFORMAT_RGBA32, big.px.data(), big.w * 4);
  if (!s) return false;
  bool ok = SDL_SavePNG(s, path.c_str());
  SDL_DestroySurface(s);
  std::printf("%s %s (%dx%d)\n", ok ? "wrote" : "FAILED", path.c_str(), big.w, big.h);
  return ok;
}
}  // namespace
