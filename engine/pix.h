// Pixel-art renderer: a 480x270 logical canvas, integer-scaled to the window with nearest filtering.
// Sprites/terrain are baked from code into textures (no art files).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "engine/color.h"
#include "engine/mathx.h"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

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

struct Tex { SDL_Texture* t = nullptr; int w = 0, h = 0; };

class Pix {
 public:
  static constexpr int W = 480, H = 270;
  bool init(const char* title, int winW, int winH, bool vsync = true);
  void shutdown();
  void begin(Color clear);
  void end();

  Tex bake(const Canvas& c);
  void blit(const Tex& t, float x, float y, bool flipX = false, Color tint = Color(1, 1, 1, 1));
  void blitRegion(const Tex& t, int sx, int sy, int sw, int sh, float dx, float dy);
  void rect(float x, float y, float w, float h, Color c);
  void rectAdd(float x, float y, float w, float h, Color c);
  void frame(float x, float y, float w, float h, Color c);   // 1px outline
  void text(float x, float y, const std::string& s, int scale, Color c, int align = 0);  // 0 left 1 center 2 right
  int textW(const std::string& s, int scale) const { return (int)s.size() * 6 * scale - scale; }

  // streaming minimap texture
  void miniInit(int w, int h);
  void miniUpdate(const uint32_t* px);
  void miniDraw(float x, float y, float scale);

  void windowToLogical(float wx, float wy, float& lx, float& ly) const;
  bool screenshot(const char* path);
  SDL_Window* window() const { return win_; }
  SDL_Renderer* renderer() const { return ren_; }

 private:
  SDL_Window* win_ = nullptr;
  SDL_Renderer* ren_ = nullptr;
  SDL_Texture* font_ = nullptr;
  SDL_Texture* mini_ = nullptr;
  int miniW_ = 0, miniH_ = 0;
  std::vector<SDL_Texture*> owned_;
};
