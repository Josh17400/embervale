// screen.h: the adaptive canvas, FILL / PIXEL PERFECT, BORDER, HUD MARGIN and the settings file.
#include "rpg/view/screen.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include "engine/pix.h"

#ifdef __EMSCRIPTEN__
// The page's safe area (the notch, the rounded corners and the home bar in landscape) in CSS px: a probe element
// padded by env(safe-area-inset-*) (web/index.html sets viewport-fit=cover, without which env() reads 0). The probe
// hangs off <html>, not <body>: SDL's fill-document mode moves every <body> child into a hidden div.
EM_JS(int, emb_safe_inset, (int side), {
  var d = document.getElementById('emb-safe-probe');
  if (!d) {
    d = document.createElement('div');
    d.id = 'emb-safe-probe';
    d.style.cssText = 'position:fixed;left:0;top:0;width:0;height:0;visibility:hidden;pointer-events:none;' +
                      'padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left)';
    document.documentElement.appendChild(d);
  }
  var cs = window.getComputedStyle(d);
  var v = side == 0 ? cs.paddingLeft : side == 1 ? cs.paddingTop : side == 2 ? cs.paddingRight : cs.paddingBottom;
  return Math.round(parseFloat(v) || 0);
});
// a phone or tablet (the default SCREEN mode is FILL there)
EM_JS(int, emb_coarse_pointer, (), {
  return (window.matchMedia && window.matchMedia('(pointer: coarse)').matches) ? 1 : 0;
});
#endif

namespace screen {

namespace {
std::string g_path;
Settings g_set;
bool g_dirty = true;
int g_autoInset = 0;
std::string g_desc;
// what the last fit was computed from (frame() recomputes only when one of these changes)
int g_pw = -1, g_ph = -1, g_ww = -1, g_wh = -1, g_ins[4] = {-1, -1, -1, -1};
int g_poll = 0;
bool g_fakeSafe = false;   // EMB_SAFE (a test hook): these insets instead of the device's
int g_fakeIns[4] = {0, 0, 0, 0};

constexpr int kDesignW = 480, kDesignH = 270;   // the safe area always holds at least this (every screen fits)
constexpr int kMaxFillH = 312;                  // FILL takes an integer scale while the safe height stays <= this

void load() {
  if (g_path.empty()) return;
  FILE* f = std::fopen(g_path.c_str(), "rb");
  if (!f) return;
  unsigned char b[8] = {};
  size_t n = std::fread(b, 1, sizeof b, f);
  std::fclose(f);
  if (n < 8 || std::memcmp(b, "EMBS", 4) != 0 || b[4] != 1) return;   // unknown: keep the defaults
  g_set.mode = b[5] == PixelPerfect ? PixelPerfect : Fill;
  g_set.border = std::clamp((int)b[6], 0, kBorderMax);
  g_set.hud = b[7] == 255 ? -1 : std::clamp((int)b[7], 0, kHudMax);
}

void save() {
  if (g_path.empty()) return;   // scripted runs: defaults, never written
  unsigned char b[8] = {'E', 'M', 'B', 'S', 1, (unsigned char)g_set.mode, (unsigned char)g_set.border,
                        (unsigned char)(g_set.hud < 0 ? 255 : g_set.hud)};
  std::string tmp = g_path + ".tmp";
  FILE* f = std::fopen(tmp.c_str(), "wb");
  if (!f) return;
  std::fwrite(b, 1, sizeof b, f);
  std::fclose(f);
  std::remove(g_path.c_str());
  std::rename(tmp.c_str(), g_path.c_str());
#ifdef __EMSCRIPTEN__
  EM_ASM(FS.syncfs(false, function(err) { if (err) console.log('settings sync failed', err); }););
#endif
}

// the device safe area in window coordinates (points / CSS px): left, top, right, bottom
void safeInsets(SDL_Window* win, int ww, int wh, int out[4]) {
  out[0] = out[1] = out[2] = out[3] = 0;
  if (g_fakeSafe) { std::memcpy(out, g_fakeIns, sizeof g_fakeIns); return; }
#ifdef __EMSCRIPTEN__
  (void)win; (void)ww; (void)wh;
  for (int i = 0; i < 4; i++) out[i] = std::max(0, emb_safe_inset(i));
#else
  SDL_Rect r;
  if (win && SDL_GetWindowSafeArea(win, &r) && r.w > 0 && r.h > 0) {
    out[0] = std::max(0, r.x);
    out[1] = std::max(0, r.y);
    out[2] = std::max(0, ww - (r.x + r.w));
    out[3] = std::max(0, wh - (r.y + r.h));
  }
#endif
}

void fit(Pix& pix, int pw, int ph, float density, const int insPt[4]) {
  const float bpx = std::floor(g_set.border * 4 * density);   // the BORDER, per side, in device px
  const float aw = std::max(64.0f, pw - 2 * bpx), ah = std::max(36.0f, ph - 2 * bpx);
  // the device safe area inside the image (a border already keeps part of the notch clear)
  float ins[4];
  for (int i = 0; i < 4; i++) ins[i] = std::max(0.0f, insPt[i] * density - bpx);
  const bool manual = g_set.hud >= 0;
  const float m = manual ? g_set.hud * 4.0f : 0.0f;
  // the largest scale that leaves 480 x 270 logical px of safe area
  const float s0 = manual ? std::min(aw / (kDesignW + 2 * m), ah / (kDesignH + 2 * m))
                          : std::min((aw - ins[0] - ins[2]) / kDesignW, (ah - ins[1] - ins[3]) / kDesignH);
  float s = s0;
  if (s0 >= 1) {
    const float si = std::floor(s0 + 1e-4f);
    if (g_set.mode == PixelPerfect) s = si;
    else {
      const float safeH = manual ? ah / si - 2 * m : (ah - ins[1] - ins[3]) / si;
      if (safeH <= kMaxFillH) s = si;   // an integer scale fills the window with at most a few px of slack
    }
  }
  s = std::max(0.25f, s);
  const bool integer = std::fabs(s - std::round(s)) < 1e-4f;
  int W = (int)std::floor(aw / s + 1e-3f), H = (int)std::floor(ah / s + 1e-3f);
  // FILL at an integer scale: round the canvas up, so the last fraction of a logical pixel is cropped, not left black
  if (integer && g_set.mode == Fill) { W = (int)std::ceil(aw / s - 1e-3f); H = (int)std::ceil(ah / s - 1e-3f); }
  W = std::max(W, 1); H = std::max(H, 1);
  // FILL with a fractional scale: stretch the last fraction of a pixel so the image meets every window edge
  if (!integer && g_set.mode == Fill) s = std::min(aw / W, ah / H);
  const int offX = (int)std::lround(bpx + (aw - W * s) / 2), offY = (int)std::lround(bpx + (ah - H * s) / 2);
  pix.present(W, H, s, offX, offY);
  int L, T, R, B;
  if (manual) L = T = R = B = (int)m;
  else {
    L = (int)std::ceil(ins[0] / s - 0.05f); T = (int)std::ceil(ins[1] / s - 0.05f);
    R = (int)std::ceil(ins[2] / s - 0.05f); B = (int)std::ceil(ins[3] / s - 0.05f);
  }
  // the 480 x 270 guarantee wins over rounding (a pixel of the inset, never a pixel of a menu)
  while (W - L - R < kDesignW && (L > 0 || R > 0)) { if (L >= R) L--; else R--; }
  while (H - T - B < kDesignH && (T > 0 || B > 0)) { if (T >= B) T--; else B--; }
  Pix::SL = L; Pix::ST = T; Pix::SR = R; Pix::SB = B;
  g_autoInset = (int)std::ceil(std::max(std::max(insPt[0], insPt[2]), std::max(insPt[1], insPt[3])) * density / s - 0.05f);
  char buf[96];
  if (integer) std::snprintf(buf, sizeof buf, "%d X %d AT %dX", W, H, (int)std::lround(s));
  else std::snprintf(buf, sizeof buf, "%d X %d AT %.2fX", W, H, s);
  g_desc = buf;
  if (std::getenv("EMB_SCREEN_LOG"))
    std::printf("screen: window %dx%d px (density %.2f), insets %d %d %d %d pt, border %d -> %s, safe L%d T%d R%d B%d, offset %d,%d\n",
                pw, ph, density, insPt[0], insPt[1], insPt[2], insPt[3], g_set.border, buf, L, T, R, B, offX, offY);
}
}  // namespace

void init(Pix& pix, const std::string& settingsPath, bool touch) {
  g_path = settingsPath;
  // test hook: EMB_SETTINGS_FILE gives a scripted run a settings file of its own (persistence tests; never the player's)
  if (const char* e = std::getenv("EMB_SETTINGS_FILE")) g_path = e;
  bool coarse = touch;
#ifdef __EMSCRIPTEN__
  coarse = coarse || emb_coarse_pointer() != 0;
#endif
  g_set = Settings();
  g_set.mode = coarse ? Fill : PixelPerfect;   // phones fill the screen; a desktop keeps whole pixels
  load();
  // test hooks (screen-fit screenshots on a desktop): EMB_SCREEN=fill|pixel, EMB_BORDER=0..12, EMB_HUD=-1..12, and
  // EMB_SAFE=l,t,r,b (a phone's safe-area insets in points, e.g. 59,0,59,21 for a landscape iPhone 15 Pro)
  if (const char* e = std::getenv("EMB_SCREEN")) g_set.mode = (e[0] == 'p' || e[0] == 'P') ? PixelPerfect : Fill;
  if (const char* e = std::getenv("EMB_BORDER")) g_set.border = std::clamp(std::atoi(e), 0, kBorderMax);
  if (const char* e = std::getenv("EMB_HUD")) g_set.hud = std::clamp(std::atoi(e), -1, kHudMax);
  if (const char* e = std::getenv("EMB_SAFE")) {
    int v[4] = {0, 0, 0, 0};
    if (std::sscanf(e, "%d,%d,%d,%d", &v[0], &v[1], &v[2], &v[3]) == 4) { g_fakeSafe = true; std::memcpy(g_fakeIns, v, sizeof v); }
  }
  g_dirty = true;
  frame(pix);
}

void frame(Pix& pix) {
  SDL_Window* win = pix.window();
  if (!win) return;
  int pw = 0, ph = 0, ww = 0, wh = 0;
  pix.outputSize(pw, ph);
  SDL_GetWindowSize(win, &ww, &wh);
  if (pw <= 0 || ph <= 0 || ww <= 0 || wh <= 0) return;
  int ins[4] = {g_ins[0], g_ins[1], g_ins[2], g_ins[3]};
  // the safe area moves with the notch when the phone turns from landscape-left to landscape-right (same size): poll it
  if (g_dirty || pw != g_pw || ph != g_ph || ww != g_ww || wh != g_wh || ++g_poll >= 30) {
    g_poll = 0;
    safeInsets(win, ww, wh, ins);
  }
  const bool insChanged = std::memcmp(ins, g_ins, sizeof ins) != 0;
  if (!g_dirty && !insChanged && pw == g_pw && ph == g_ph && ww == g_ww && wh == g_wh) return;
  g_dirty = false;
  g_pw = pw; g_ph = ph; g_ww = ww; g_wh = wh;
  std::memcpy(g_ins, ins, sizeof ins);
  fit(pix, pw, ph, (float)pw / (float)ww, ins);
}

Settings get() { return g_set; }

void set(const Settings& s) {
  Settings n = s;
  n.mode = n.mode == PixelPerfect ? PixelPerfect : Fill;
  n.border = std::clamp(n.border, 0, kBorderMax);
  n.hud = n.hud < 0 ? -1 : std::clamp(n.hud, 0, kHudMax);
  if (n.mode == g_set.mode && n.border == g_set.border && n.hud == g_set.hud) return;
  g_set = n;
  g_dirty = true;
  save();
}

int autoInset() { return g_autoInset; }
std::string describe() { return g_desc; }

}  // namespace screen
