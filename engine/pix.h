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
  // ---- P2a Clear Sight (VISION_PLAN 15.36 item 1): two layers. The WORLD layer is drawn at a whole number D of device
  // px per world px (every world pixel an exact D x D block); the UI layer keeps the fit present() gives today (480 x 270
  // safe boxes, fractional where it is fractional today). A layer: w x h logical px at `scale` device px each, its
  // logical (0, 0) at device px (offX, offY). FROZEN after phase A (LENS implements; lanes ADD, never rename).
  struct Layer { int w = 0, h = 0; float scale = 1; int offX = 0, offY = 0; };
  // both layers at once (screen.cpp); phase A stub: the world layer IS the UI layer (present(ui...)), nothing moves
  void presentLayers(const Layer& world, const Layer& ui);
  // draws between beginWorld() and endWorld() go to the world layer: inside, Pix::W / Pix::H read the world view in world
  // px, the insets are 0, the render scale is the integer D, and setTarget(nullptr) returns to the world layer (not the
  // window). Outside, everything is the UI layer as today. Phase A stub: a flag only (world == UI).
  void beginWorld();
  void endWorld();
  bool inWorld() const { return inWorld_; }
  const Layer& worldLayer() const { return world_; }
  const Layer& uiLayer() const { return ui_; }
  // the world layer as RGBA8 at world px (w x h = worldLayer().w x h): each world pixel's centre sampled from the device
  // pixels, so a D x D block reads back as one pixel. Phase A stub: readCanvas (world == UI).
  bool readWorld(std::vector<uint8_t>& rgba, int& w, int& h);
  // (P2a LENS, added) the device rect the world may cover (the window inside the BORDER): endWorld() keeps the rest black
  void setWorldClip(int x, int y, int w, int h);
  // (P2a LENS, added) the UI layer's safe-area insets (Pix::SL, ST, SR, SB as the UI sees them), also inside
  // beginWorld / endWorld where Pix::SL..SB read 0 (convert with View::uiInsetsToWorld)
  void uiInsets(int& l, int& t, int& r, int& b) const {
    if (inWorld_) { l = saved_.SL; t = saved_.ST; r = saved_.SR; b = saved_.SB; } else { l = SL; t = ST; r = SR; b = SB; }
  }
  // (P2a LENS, added) a record of the normal-blend blitEx calls made while a list is set (the night lift re-adds the
  // people drawn in the sorted pass after the light map); nullptr stops recording
  // occ (P2a LENS fix r4): on a recordBlits entry, how many recordOthers entries had been made when it was blitted
  // (kNoOcc: no recordOthers list was set); the night lift replays the two records in their true draw order from it
  static constexpr uint32_t kNoOcc = 0xFFFFFFFFu;
  struct BlitRec { SDL_Texture* t = nullptr; int sx = 0, sy = 0, sw = 0, sh = 0; float dx = 0, dy = 0, dw = 0, dh = 0, a = 1; bool flip = false; uint32_t occ = kNoOcc; };
  void recordBlits(std::vector<BlitRec>* out) { rec_ = out; }
  // (P2a LENS fix r3, added) a second record: every normal-blend blit, blitRegion and blitEx made while this list is set
  // and recordBlits has none (the night lift's occluders: what the sorted pass draws round the people); nullptr stops it
  void recordOthers(std::vector<BlitRec>* out) { oth_ = out; }
  void blitRec(const BlitRec& r, Color tint, int blend);
  // UI boxes: draws go into the logical rectangle (x, y, w, h) of the screen, with (0, 0) at its top-left, clipped
  // to it, and Pix::W / Pix::H read the box size until popBox(). Menus and dialogues are laid out for 480 px of
  // width, so on a wide phone they sit in a centred box instead of stretching. Nested boxes are relative.
  void pushBox(int x, int y, int w, int h);
  void popBox();
  void outputSize(int& w, int& h) const;   // the window's drawable size in device pixels
  bool init(const char* title, int winW, int winH, bool vsync = true, bool hidden = false);   // hidden: a test harness (Witness)
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
  // the logical canvas as RGBA8 (Pix::W x Pix::H, top row first) read back from this frame before end(): each logical
  // pixel's centre sampled from the device pixels (the Witness test harness's screenshot)
  bool readCanvas(std::vector<uint8_t>& rgba, int& w, int& h);
  // (Witness) a test harness's reset: markTextures() remembers the textures alive now (the font, before any View);
  // destroyUnmarked() destroys every texture made since and bumps texEpoch, which a function-static texture checks
  // to know it is gone. Never called in normal play.
  void markTextures() { marked_ = owned_; }
  void destroyUnmarked();
  static inline int texEpoch = 0;
  SDL_Window* window() const { return win_; }
  SDL_Renderer* renderer() const { return ren_; }

 private:
  SDL_Window* win_ = nullptr;
  SDL_Renderer* ren_ = nullptr;
  SDL_Texture* font_ = nullptr;
  SDL_Texture* mini_ = nullptr;
  int miniW_ = 0, miniH_ = 0;
  std::vector<SDL_Texture*> owned_;
  std::vector<SDL_Texture*> marked_;   // (Witness) markTextures()
  float scale_ = 1;
  Layer world_, ui_;                 // (P2a) the two layers (phase A: equal)
  bool inWorld_ = false;             // (P2a) between beginWorld() and endWorld()
  bool pixelArt_ = false;            // sprites use SDL_SCALEMODE_PIXELART (non-integer scale)
  int baseX_ = 0, baseY_ = 0;        // the canvas viewport origin (logical units)
  struct BoxState { int vx, vy, vw, vh, W, H, SL, ST, SR, SB; };
  std::vector<BoxState> boxes_;
  int vpX_ = 0, vpY_ = 0;            // the current viewport origin (base + boxes)
  void applyViewport(int x, int y, int w, int h);
  void spriteScaleMode(SDL_Texture* t);
  // (P2a LENS) the world layer's viewport origin in render units (x D), the UI state beginWorld() put aside, the world's
  // device clip (w < 0: none), the blit record
  int wBaseX_ = 0, wBaseY_ = 0;
  SDL_Texture* worldTex_ = nullptr;   // the world's render target (w x h world px; never in owned_: a Witness reset keeps it)
  int worldTexW_ = 0, worldTexH_ = 0;
  bool useTex_ = false;               // this beginWorld draws into worldTex_
  BoxState saved_{};
  int clipX_ = 0, clipY_ = 0, clipW_ = -1, clipH_ = -1;
  std::vector<BlitRec>* rec_ = nullptr;
  std::vector<BlitRec>* oth_ = nullptr;
  void sampleFor(SDL_Texture* t);
};
