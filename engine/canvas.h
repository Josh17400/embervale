// The CPU pixel canvas painters draw into (Canvas) and the RGBA8 packing (rgba()), with no SDL behind them.
// Who may include it: any painter or tool that only fills a Canvas (rpg/art, rpg/culture, rpg/build, tools). Code that
// bakes, blits or presents textures needs engine/pix.h (which includes this).
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

inline uint32_t rgba(int r, int g, int b, int a = 255) {
  return (uint32_t)(r & 255) | (uint32_t)(g & 255) << 8 | (uint32_t)(b & 255) << 16 | (uint32_t)(a & 255) << 24;
}

// CPU pixel canvas used to bake textures.
struct Canvas {
  int w = 0, h = 0;
  std::vector<uint32_t> px;
  Canvas() = default;
  Canvas(int w_, int h_) : w(w_), h(h_), px((size_t)w_ * h_, 0) {}
  void set(int x, int y, uint32_t c) { if (x >= 0 && y >= 0 && x < w && y < h) px[(size_t)y * w + x] = c; }
  uint32_t get(int x, int y) const { return (x >= 0 && y >= 0 && x < w && y < h) ? px[(size_t)y * w + x] : 0; }
  void rect(int x, int y, int rw, int rh, uint32_t c) { for (int j = 0; j < rh; j++) for (int i = 0; i < rw; i++) set(x + i, y + j, c); }
  void disc(int cx, int cy, int r, uint32_t c) {
    for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++) if (i * i + j * j <= r * r + r / 2) set(cx + i, cy + j, c);
  }
};
