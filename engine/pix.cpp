#include "engine/pix.h"
#include <SDL3/SDL.h>
#include <cmath>
#include "engine/font5x7.h"

using namespace font5x7;

namespace {
constexpr int kCols = 16, kRows = 6;
}  // namespace

bool Pix::init(const char* title, int winW, int winH, bool vsync) {
  SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#ifdef __EMSCRIPTEN__
  flags |= SDL_WINDOW_FILL_DOCUMENT;   // the canvas takes the whole page and follows the browser size
#endif
  if (!SDL_CreateWindowAndRenderer(title, winW, winH, flags, &win_, &ren_)) {
    SDL_Log("window/renderer failed: %s", SDL_GetError());
    return false;
  }
  SDL_SetRenderLogicalPresentation(ren_, W, H, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);   // until present()
  SDL_SetRenderVSync(ren_, vsync ? 1 : 0);
  Canvas f(kCols * kCellW, kRows * kCellH);
  for (int gi = 0; gi < kGlyphCount; gi++) {
    int ox = (gi % kCols) * kCellW, oy = (gi / kCols) * kCellH;
    for (int row = 0; row < 7; row++)
      for (int col = 0; col < 5; col++)
        if (kFont[gi].rows[row] & (0x10 >> col)) f.set(ox + col, oy + row, rgba(255, 255, 255));
  }
  Tex ft = bake(f);
  font_ = ft.t;
  return font_ != nullptr;
}

void Pix::setLogical(int w, int h, bool integerScale) {
  W = w; H = h;
  if (ren_) SDL_SetRenderLogicalPresentation(ren_, W, H, integerScale ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE : SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

void Pix::outputSize(int& w, int& h) const {
  w = h = 0;
  if (ren_) SDL_GetCurrentRenderOutputSize(ren_, &w, &h);
}

// Sprites follow the presentation: nearest sampling at an integer scale, SDL's pixel-art filter (nearest with a
// one-device-pixel blend at texel edges) at a non-integer one. Linear textures (the light map) are left alone.
void Pix::spriteScaleMode(SDL_Texture* t) {
  if (!t) return;
  SDL_ScaleMode m = SDL_SCALEMODE_NEAREST;
  if (!SDL_GetTextureScaleMode(t, &m) || m == SDL_SCALEMODE_LINEAR) return;
  SDL_SetTextureScaleMode(t, pixelArt_ ? SDL_SCALEMODE_PIXELART : SDL_SCALEMODE_NEAREST);
}

void Pix::applyViewport(int x, int y, int w, int h) {
  vpX_ = x; vpY_ = y;
  SDL_Rect r{x, y, w, h};
  SDL_SetRenderViewport(ren_, &r);
}

void Pix::present(int w, int h, float scale, int offX, int offY) {
  if (!ren_) return;
  W = w; H = h;
  scale_ = scale > 0 ? scale : 1;
  // the canvas is drawn straight to the window at this scale (no intermediate texture): SDL's own logical
  // presentation is off, the render scale does the scaling and the viewport places (and clips) the canvas
  SDL_SetRenderLogicalPresentation(ren_, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
  SDL_SetRenderScale(ren_, scale_, scale_);
  baseX_ = (int)std::lround(offX / scale_);
  baseY_ = (int)std::lround(offY / scale_);
  boxes_.clear();
  applyViewport(baseX_, baseY_, W, H);
  const bool pa = std::fabs(scale_ - std::round(scale_)) > 0.01f;
  if (pa != pixelArt_) {
    pixelArt_ = pa;
    for (SDL_Texture* t : owned_) spriteScaleMode(t);
    spriteScaleMode(font_);
    spriteScaleMode(mini_);
  }
}

void Pix::pushBox(int x, int y, int w, int h) {
  boxes_.push_back({vpX_, vpY_, W, H, W, H, SL, ST, SR, SB});
  W = w; H = h;
  SL = ST = SR = SB = 0;
  applyViewport(vpX_ + x, vpY_ + y, w, h);
}

void Pix::popBox() {
  if (boxes_.empty()) return;
  BoxState b = boxes_.back();
  boxes_.pop_back();
  W = b.W; H = b.H;
  SL = b.SL; ST = b.ST; SR = b.SR; SB = b.SB;
  applyViewport(b.vx, b.vy, b.vw, b.vh);
}

void Pix::shutdown() {
  for (SDL_Texture* t : owned_) SDL_DestroyTexture(t);
  owned_.clear();
  if (mini_) SDL_DestroyTexture(mini_);
  if (ren_) SDL_DestroyRenderer(ren_);
  if (win_) SDL_DestroyWindow(win_);
  mini_ = nullptr; ren_ = nullptr; win_ = nullptr; font_ = nullptr;
}

void Pix::begin(Color c) {
  SDL_SetRenderDrawColorFloat(ren_, c.r, c.g, c.b, 1.0f);
  SDL_RenderClear(ren_);
}
void Pix::end() { SDL_RenderPresent(ren_); }

Tex Pix::bake(const Canvas& c) {
  Tex t;
  SDL_Surface* s = SDL_CreateSurfaceFrom(c.w, c.h, SDL_PIXELFORMAT_RGBA32, (void*)c.px.data(), c.w * 4);
  t.t = SDL_CreateTextureFromSurface(ren_, s);
  SDL_DestroySurface(s);
  t.w = c.w; t.h = c.h;
  if (t.t) {
    SDL_SetTextureScaleMode(t.t, pixelArt_ ? SDL_SCALEMODE_PIXELART : SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(t.t, SDL_BLENDMODE_BLEND);
    owned_.push_back(t.t);
  }
  return t;
}

void Pix::blit(const Tex& t, float x, float y, bool flipX, Color tint) {
  if (!t.t) return;
  SDL_SetTextureColorModFloat(t.t, tint.r, tint.g, tint.b);
  SDL_SetTextureAlphaModFloat(t.t, tint.a);
  SDL_FRect d{std::floor(x), std::floor(y), (float)t.w, (float)t.h};
  SDL_RenderTextureRotated(ren_, t.t, nullptr, &d, 0.0, nullptr, flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void Pix::blitRegion(const Tex& t, int sx, int sy, int sw, int sh, float dx, float dy) {
  if (!t.t) return;
  SDL_SetTextureColorModFloat(t.t, 1, 1, 1);
  SDL_SetTextureAlphaModFloat(t.t, 1);
  SDL_FRect s{(float)sx, (float)sy, (float)sw, (float)sh};
  SDL_FRect d{std::floor(dx), std::floor(dy), (float)sw, (float)sh};
  SDL_RenderTexture(ren_, t.t, &s, &d);
}

void Pix::blitEx(const Tex& t, int sx, int sy, int sw, int sh, float dx, float dy, float dw, float dh, bool flipX, Color tint, int blend) {
  if (!t.t) return;
  static const SDL_BlendMode modes[] = {SDL_BLENDMODE_BLEND, SDL_BLENDMODE_ADD, SDL_BLENDMODE_MOD, SDL_BLENDMODE_MUL};
  SDL_SetTextureBlendMode(t.t, modes[blend & 3]);
  SDL_SetTextureColorModFloat(t.t, tint.r, tint.g, tint.b);
  SDL_SetTextureAlphaModFloat(t.t, tint.a);
  SDL_FRect s{(float)sx, (float)sy, (float)sw, (float)sh};
  SDL_FRect d{std::floor(dx), std::floor(dy), dw, dh};
  SDL_RenderTextureRotated(ren_, t.t, &s, &d, 0.0, nullptr, flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
  if (blend) SDL_SetTextureBlendMode(t.t, SDL_BLENDMODE_BLEND);
}

Tex Pix::makeTarget(int w, int h) {
  Tex t;
  t.t = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
  t.w = w; t.h = h;
  if (t.t) { SDL_SetTextureScaleMode(t.t, SDL_SCALEMODE_LINEAR); owned_.push_back(t.t); }
  return t;
}
void Pix::setTarget(const Tex* t) { SDL_SetRenderTarget(ren_, t ? t->t : nullptr); }
Tex Pix::makeStream(int w, int h) {
  Tex t;
  t.t = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
  t.w = w; t.h = h;
  if (t.t) { SDL_SetTextureScaleMode(t.t, pixelArt_ ? SDL_SCALEMODE_PIXELART : SDL_SCALEMODE_NEAREST); SDL_SetTextureBlendMode(t.t, SDL_BLENDMODE_BLEND); owned_.push_back(t.t); }
  return t;
}
void Pix::updateStream(const Tex& t, const uint32_t* px) { if (t.t) SDL_UpdateTexture(t.t, nullptr, px, t.w * 4); }
void Pix::destroy(Tex& t) {
  if (!t.t) return;
  for (size_t i = 0; i < owned_.size(); i++) if (owned_[i] == t.t) { owned_.erase(owned_.begin() + i); break; }
  SDL_DestroyTexture(t.t);
  t.t = nullptr;
}

void Pix::rect(float x, float y, float w, float h, Color c) {
  SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColorFloat(ren_, c.r, c.g, c.b, c.a);
  SDL_FRect r{std::floor(x), std::floor(y), std::floor(w), std::floor(h)};
  SDL_RenderFillRect(ren_, &r);
}
void Pix::rectAdd(float x, float y, float w, float h, Color c) {
  SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_ADD);
  SDL_SetRenderDrawColorFloat(ren_, c.r, c.g, c.b, c.a);
  SDL_FRect r{std::floor(x), std::floor(y), std::floor(w), std::floor(h)};
  SDL_RenderFillRect(ren_, &r);
}
void Pix::frame(float x, float y, float w, float h, Color c) {
  rect(x, y, w, 1, c); rect(x, y + h - 1, w, 1, c);
  rect(x, y, 1, h, c); rect(x + w - 1, y, 1, h, c);
}

void Pix::text(float x, float y, const std::string& s, int scale, Color c, int align) {
  int w = textW(s, scale);
  if (align == 1) x -= w * 0.5f; else if (align == 2) x -= w;
  x = std::floor(x); y = std::floor(y);
  SDL_SetTextureColorModFloat(font_, c.r, c.g, c.b);
  SDL_SetTextureAlphaModFloat(font_, c.a);
  for (char ch : s) {
    int gi = glyphIndex(ch);
    if (gi >= 0) {
      SDL_FRect src{(float)((gi % kCols) * kCellW), (float)((gi / kCols) * kCellH), 5, 7};
      SDL_FRect dst{x, y, (float)(5 * scale), (float)(7 * scale)};
      SDL_RenderTexture(ren_, font_, &src, &dst);
    }
    x += 6 * scale;
  }
}

void Pix::miniInit(int w, int h) {
  miniW_ = w; miniH_ = h;
  mini_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
  if (mini_) { SDL_SetTextureScaleMode(mini_, pixelArt_ ? SDL_SCALEMODE_PIXELART : SDL_SCALEMODE_NEAREST); SDL_SetTextureBlendMode(mini_, SDL_BLENDMODE_BLEND); }
}
void Pix::miniUpdate(const uint32_t* px) {
  if (mini_) SDL_UpdateTexture(mini_, nullptr, px, miniW_ * 4);
}
void Pix::miniDraw(float x, float y, float scale) {
  if (!mini_) return;
  SDL_FRect d{std::floor(x), std::floor(y), miniW_ * scale, miniH_ * scale};
  SDL_RenderTexture(ren_, mini_, nullptr, &d);
}

void Pix::windowToLogical(float wx, float wy, float& lx, float& ly) const {
  SDL_RenderCoordinatesFromWindow(ren_, wx, wy, &lx, &ly);
}

// the whole window as the player sees it (border included), at device resolution
bool Pix::screenshot(const char* path) {
  SDL_Rect vp{};
  float sx = 1, sy = 1;
  SDL_GetRenderViewport(ren_, &vp);
  SDL_GetRenderScale(ren_, &sx, &sy);
  SDL_SetRenderScale(ren_, 1, 1);
  SDL_SetRenderViewport(ren_, nullptr);
  SDL_Surface* s = SDL_RenderReadPixels(ren_, nullptr);
  SDL_SetRenderScale(ren_, sx, sy);
  SDL_SetRenderViewport(ren_, &vp);
  if (!s) return false;
  bool ok = SDL_SavePNG(s, path);
  SDL_DestroySurface(s);
  return ok;
}
