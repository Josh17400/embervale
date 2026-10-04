// Pixel-art renderer: a logical canvas (480x270 by default; Pix::W / Pix::H are runtime values so the screen code can
// match the device's aspect, M1 phone screen fill), scaled to the window with nearest filtering.
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
  // The logical canvas size. Runtime values (M1): rpg/view/screen.cpp may change them (adaptive width for wide phones)
  // through setLogical(); everything that lays out UI reads them every frame, never caches them.
  static inline int W = 480, H = 270;
  // The safe-area (HUD) insets in logical px: the HUD, the touch controls and the menus keep inside
  // [SL, W - SR] x [ST, H - SB] (the iPhone notch, rounded corners, the home bar, or the player's HUD MARGIN).
  // Set by screen.cpp; zero inside a pushed UI box (the box already lies inside them).
  static inline int SL = 0, ST = 0, SR = 0, SB = 0;
  void setLogical(int w, int h, bool integerScale);   // simple fit (tools); the game uses present()
  // The presentation (rpg/view/screen.cpp, M1 phone screen fill): the logical canvas w x h is drawn at `scale`
  // device pixels per logical pixel with its top-left at device pixel (offX, offY); the rest of the window stays
  // black (the BORDER setting). A non-integer scale switches the sprites to SDL's pixel-art filter (crisp, even
  // pixel widths without shimmer); an integer scale keeps plain nearest sampling.
  void present(int w, int h, float scale, int offX, int offY);
  float presentScale() const { return scale_; }
  // UI boxes: draws go into the logical rectangle (x, y, w, h) of the screen, with (0, 0) at its top-left, clipped
  // to it, and Pix::W / Pix::H read the box size until popBox(). Menus and dialogues are laid out for 480 px of
  // width, so on a wide phone they sit in a centred box instead of stretching. Nested boxes are relative.
  void pushBox(int x, int y, int w, int h);
  void popBox();
  void outputSize(int& w, int& h) const;   // the window's drawable size in device pixels
  bool init(const char* title, int winW, int winH, bool vsync = true);
  void shutdown();
  void begin(Color clear);
  void end();

  Tex bake(const Canvas& c);
  void blit(const Tex& t, float x, float y, bool flipX = false, Color tint = Color(1, 1, 1, 1));
  void blitRegion(const Tex& t, int sx, int sy, int sw, int sh, float dx, float dy);
  // general blit: source region -> dest rect, optional flip, tint/alpha and blend (0 normal, 1 add, 2 mod, 3 mul)
  void blitEx(const Tex& t, int sx, int sy, int sw, int sh, float dx, float dy, float dw, float dh, bool flipX = false,
              Color tint = Color(1, 1, 1, 1), int blend = 0);
  Tex makeTarget(int w, int h);                 // render-target texture
  void setTarget(const Tex* t);                 // nullptr = back to the screen
  Tex makeStream(int w, int h);                 // CPU-updatable texture
  void updateStream(const Tex& t, const uint32_t* px);
  void destroy(Tex& t);
  void rect(float x, float y, float w, float h, Color c);
  void rectAdd(float x, float y, float w, float h, Color c);
  void frame(float x, float y, float w, float h, Color c);   // 1px outline
  void text(float x, float y, const std::string& s, int scale, Color c, int align = 0);  // 0 left 1 center 2 right
  void textS(float x, float y, const std::string& s, int scale, Color c, int align = 0) {   // with a 1px drop shadow
    text(x + scale, y + scale, s, scale, Color(0.02f, 0.02f, 0.04f, c.a * 0.85f), align);
    text(x, y, s, scale, c, align);
  }
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
  float scale_ = 1;
  bool pixelArt_ = false;            // sprites use SDL_SCALEMODE_PIXELART (non-integer scale)
  int baseX_ = 0, baseY_ = 0;        // the canvas viewport origin (logical units)
  struct BoxState { int vx, vy, vw, vh, W, H, SL, ST, SR, SB; };
  std::vector<BoxState> boxes_;
  int vpX_ = 0, vpY_ = 0;            // the current viewport origin (base + boxes)
  void applyViewport(int x, int y, int w, int h);
  void spriteScaleMode(SDL_Texture* t);
};
