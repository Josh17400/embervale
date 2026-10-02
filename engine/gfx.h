// 2D renderer: glow sprites, ribbons, polygons, bitmap text. Backed by SDL3's GPU-accelerated
// renderer today; game code only sees this interface (so an SDL_GPU/Metal backend can replace it).
#pragma once
#include <string>
#include <vector>
#include "engine/mathx.h"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Vertex;

struct Color {
  float r = 1, g = 1, b = 1, a = 1;
  Color() = default;
  Color(float r_, float g_, float b_, float a_ = 1) : r(r_), g(g_), b(b_), a(a_) {}
  Color withA(float na) const { return {r, g, b, na}; }
  Color scaled(float s) const { return {r * s, g * s, b * s, a}; }
};

constexpr int VIEW_W = 1920;   // logical resolution (16:9); letterboxed to the real window
constexpr int VIEW_H = 1080;

class Gfx {
 public:
  bool init(const char* title, int winW, int winH, bool vsync = true);
  void shutdown();
  void begin(Color clear);
  void end();

  // camera: world -> logical screen
  Vec2 cam;
  float zoom = 1.0f;
  Vec2 shake;
  Vec2 toScreen(Vec2 w) const { return {(w.x - cam.x) * zoom + VIEW_W * 0.5f + shake.x, (w.y - cam.y) * zoom + VIEW_H * 0.5f + shake.y}; }
  Vec2 toWorld(Vec2 s) const { return {(s.x - VIEW_W * 0.5f - shake.x) / zoom + cam.x, (s.y - VIEW_H * 0.5f - shake.y) / zoom + cam.y}; }
  bool onScreen(Vec2 w, float margin) const;

  // world-space drawing (additive unless noted)
  void glow(Vec2 w, float radius, Color c);                        // soft additive blob
  void disc(Vec2 w, float radius, Color c, bool additive = false); // crisp round disc
  void line(Vec2 a, Vec2 b, float width, Color c, bool additive = true);
  void ring(Vec2 w, float radius, float thick, Color c, int segs = 40);
  void ribbon(const std::vector<Vec2>& pts, const std::vector<float>& width, const std::vector<Color>& col, bool additive = true);
  void polyFill(const std::vector<Vec2>& poly, Color c, bool additive = true);
  void rectW(Vec2 tl, Vec2 br, Color c, bool additive = false);

  // screen-space (logical 1920x1080) drawing
  void rectS(float x, float y, float w, float h, Color c, bool additive = false);
  void frameS(float x, float y, float w, float h, float t, Color c);
  void glowS(float x, float y, float radius, Color c);
  void discS(float x, float y, float r, Color c);
  void text(float x, float y, const std::string& s, float scale, Color c, int align = 0);  // 0 left 1 center 2 right
  float textWidth(const std::string& s, float scale) const;

  // input coordinate conversion (window pixels -> logical)
  void windowToLogical(float wx, float wy, float& lx, float& ly) const;
  bool screenshot(const char* path);
  SDL_Window* window() const { return win_; }
  SDL_Renderer* renderer() const { return ren_; }

 private:
  SDL_Window* win_ = nullptr;
  SDL_Renderer* ren_ = nullptr;
  SDL_Texture* glowTex_ = nullptr;
  SDL_Texture* discTex_ = nullptr;
  SDL_Texture* fontTex_ = nullptr;
  void geometry(const std::vector<SDL_Vertex>& v, const std::vector<int>& idx, bool additive);
};
