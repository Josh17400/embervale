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
#include "rpg/art/art_rig.h"

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
bool g_pinned = false;     // init(..., pinned): the 480 x 270 design canvas whatever the window, settings or env say

constexpr int kDesignW = 480, kDesignH = 270;   // the safe area always holds at least this (every screen fits)
constexpr int kMaxFillH = 312;                  // FILL takes an integer scale while the safe height stays <= this

// (P2a) the world scale: what the last fit computed for this screen
int g_d0 = 1;                 // D0
std::vector<int> g_levels;    // the allowed D, FAR first
int g_envZoom = 0;            // EMB_ZOOM: an absolute D (0: none)
bool g_witnessFree = false;   // (Witness) a reset asked for a window / safe area / mode: the fit runs unpinned
bool g_fakeSafeDev = false;   // (Witness) g_fakeIns are device px (EMB_SAFE gives points)
// (P2c ZOOM, CHARACTER_SIZE 7 (c)) phones start at NEAR: while the settings file holds no zoom choice (no file, or a
// version-1 file) the zoom follows the device on every fit: NEAR on a phone, NORMAL anywhere else. The first save
// writes the value in use, and from then on the file's value wins. Never for a pinned (Witness) or scripted run (no
// settings path), so Witness and scripts stay NORMAL.
bool g_zoomFirst = false;
bool g_coarse = false;        // a coarse pointer: touchUI (iOS, Android) or the web's (pointer: coarse)
float g_devDensity = 0;       // EMB_DEVICE=touch@<d>: the device px per point to judge the short side with (0: the window's)
int g_devForce = -1;          // EMB_DEVICE=desktop|phone|tablet / the firstlaunch command: the device class (-1: measured)
constexpr int kPhoneShortPt = 500;   // a phone: a coarse pointer and a short side under this many points (CSS px)

// EMBS file: version 1 = 8 bytes (mode, border, hud); version 2 (P2a) = 10 bytes (+ zoom + 1, ring)
void load() {
  if (g_path.empty()) return;
  FILE* f = std::fopen(g_path.c_str(), "rb");
  if (!f) return;
  unsigned char b[10] = {};
  size_t n = std::fread(b, 1, sizeof b, f);
  std::fclose(f);
  if (n < 8 || std::memcmp(b, "EMBS", 4) != 0 || (b[4] != 1 && b[4] != 2)) return;   // unknown: keep the defaults
  if (b[4] == 2 && n < 10) return;
  g_set.mode = b[5] == PixelPerfect ? PixelPerfect : Fill;
  g_set.border = std::clamp((int)b[6], 0, kBorderMax);
  g_set.hud = b[7] == 255 ? -1 : std::clamp((int)b[7], 0, kHudMax);
  if (b[4] == 2) {   // a version-1 file keeps the new fields' defaults
    g_set.zoom = std::clamp((int)b[8] - 1, kZoomMin, kZoomMax);
    g_set.ring = b[9] != 0;
    g_zoomFirst = false;   // the player's zoom is saved: it wins over the phone default
  }
}

// (P2c ZOOM) a phone: a coarse pointer and the window's short side under kPhoneShortPt points. An iPad (768+ points)
// and every desktop (a fine pointer) are not phones
bool isPhone(int pw, int ph, float density) {
  if (g_devForce >= 0) return g_devForce == DevicePhone;
  if (!g_coarse) return false;
  const float d = g_devDensity > 0 ? g_devDensity : std::max(0.01f, density);
  return std::min(pw, ph) / d < (float)kPhoneShortPt;
}

void save() {
  if (g_path.empty()) return;   // scripted runs: defaults, never written
  g_zoomFirst = false;          // (P2c ZOOM) the zoom in use is written, so it is the player's from now on
  unsigned char b[10] = {'E', 'M', 'B', 'S', 2, (unsigned char)g_set.mode, (unsigned char)g_set.border,
                         (unsigned char)(g_set.hud < 0 ? 255 : g_set.hud), (unsigned char)(g_set.zoom + 1), (unsigned char)(g_set.ring ? 1 : 0)};
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
  if (g_fakeSafe) {
    std::memcpy(out, g_fakeIns, sizeof g_fakeIns);
    if (g_fakeSafeDev && win) {   // (Witness) device px to points
      int pw = 0, ph = 0;
      SDL_GetWindowSizeInPixels(win, &pw, &ph);
      const float d = ww > 0 && pw > 0 ? (float)pw / (float)ww : 1.0f;
      for (int i = 0; i < 4; i++) out[i] = (int)std::lround(g_fakeIns[i] / d);
    }
    (void)wh;
    return;
  }
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

// the window resized to w x h device px: SDL sizes it in points, so a high-density display divides; one correction pass
void resizeWindow(Pix& pix, int w, int h) {
  SDL_Window* win = pix.window();
  if (!win) return;
  int pw = 0, ph = 0, ww = 0, wh = 0;
  SDL_SetWindowSize(win, w, h);
  SDL_SyncWindow(win);
  SDL_GetWindowSizeInPixels(win, &pw, &ph);
  SDL_GetWindowSize(win, &ww, &wh);
  if ((pw != w || ph != h) && pw > 0 && ph > 0 && ww > 0 && wh > 0) {
    SDL_SetWindowSize(win, (int)std::lround(w * (double)ww / pw), (int)std::lround(h * (double)wh / ph));
    SDL_SyncWindow(win);
  }
}

// (P2a) the world scale numbers for a device height (the whole window behind the notch, inside the BORDER)
void worldFit(int hDevice) {
  g_d0 = d0ForHeight(hDevice);
  g_levels = levelsForHeight(hDevice);
}
// (P2a) " WORLD 512 X 236 AT 5X" when the world layer is not the UI's fit (phase A: never, so the UI text is unchanged)
void describeWorld(Pix& pix) {
  const Pix::Layer& w = pix.worldLayer();
  const Pix::Layer& u = pix.uiLayer();
  if (w.w == u.w && w.h == u.h && std::fabs(w.scale - u.scale) < 1e-4f) return;
  char buf[64];
  std::snprintf(buf, sizeof buf, " WORLD %d X %d AT %dX", w.w, w.h, (int)std::lround(w.scale));
  g_desc += buf;
}

// (P2a LENS) both layers: the UI's fit, and the world at the whole number D covering the device rect (ax, ay, aw, ah)
// (the window inside the BORDER, behind the notch too), centred on it, its last partial world pixels cropped
void presentBoth(Pix& pix, int uw, int uh, float s, int offX, int offY, int ax, int ay, int aw, int ah) {
  const int d = std::max(1, worldScale());
  Pix::Layer ui, wl;
  ui.w = uw; ui.h = uh; ui.scale = s; ui.offX = offX; ui.offY = offY;
  wl.w = std::max(1, (aw + d - 1) / d); wl.h = std::max(1, (ah + d - 1) / d); wl.scale = (float)d;
  wl.offX = ax + (int)std::floor((aw - wl.w * d) / 2.0); wl.offY = ay + (int)std::floor((ah - wl.h * d) / 2.0);
  pix.presentLayers(wl, ui);
  // presentLayers puts the origin on a whole multiple of D: one more world pixel where that left a sliver uncovered
  const Pix::Layer& got = pix.worldLayer();
  if (got.offX + got.w * d < ax + aw || got.offY + got.h * d < ay + ah || got.offX > ax || got.offY > ay) {
    wl.w += 1 + (got.offX > ax ? 1 : 0); wl.h += 1 + (got.offY > ay ? 1 : 0);
    if (got.offX > ax) wl.offX -= d;
    if (got.offY > ay) wl.offY -= d;
    pix.presentLayers(wl, ui);
  }
  // the world shows where the UI canvas lies (so a fade, the pause dim or the loading card covers all of it): PIXEL
  // PERFECT's black remainder stays black, a BORDER stays black
  const int ux0 = std::max(ax, offX), uy0 = std::max(ay, offY);
  const int ux1 = std::min(ax + aw, offX + (int)std::lround(uw * s)), uy1 = std::min(ay + ah, offY + (int)std::lround(uh * s));
  pix.setWorldClip(ux0, uy0, std::max(0, ux1 - ux0), std::max(0, uy1 - uy0));
}

void fit(Pix& pix, int pw, int ph, float density, const int insPt[4]) {
  if (g_pinned && !g_witnessFree) {   // (Witness) the design canvas, centred, at whatever scale fits; no border, no insets
    const float s = std::max(0.25f, std::min(pw / (float)kDesignW, ph / (float)kDesignH));
    worldFit(ph);
    presentBoth(pix, kDesignW, kDesignH, s, (int)std::lround((pw - kDesignW * s) / 2), (int)std::lround((ph - kDesignH * s) / 2), 0, 0, pw, ph);
    Pix::SL = Pix::ST = Pix::SR = Pix::SB = 0;
    g_autoInset = 0;
    char buf[64];
    std::snprintf(buf, sizeof buf, "%d X %d AT %.2fX (PINNED)", kDesignW, kDesignH, s);
    g_desc = buf;
    describeWorld(pix);
    return;
  }
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
  worldFit((int)ah);
  if (g_zoomFirst) g_set.zoom = isPhone(pw, ph, density) ? 1 : 0;   // (P2c ZOOM) no saved choice: NEAR on a phone
  presentBoth(pix, W, H, s, offX, offY, (int)bpx, (int)bpx, (int)aw, (int)ah);
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
  describeWorld(pix);
  if (std::getenv("EMB_SCREEN_LOG")) {
    std::printf("screen: window %dx%d px (density %.2f), insets %d %d %d %d pt, border %d -> %s, safe L%d T%d R%d B%d, offset %d,%d\n",
                pw, ph, density, insPt[0], insPt[1], insPt[2], insPt[3], g_set.border, buf, L, T, R, B, offX, offY);
    std::printf("screen: world D0 %d, levels", g_d0);   // (P2a)
    for (int d : g_levels) std::printf(" %d", d);
    std::printf(", in use %d (zoom %d%s)\n", worldScale(), g_set.zoom, g_zoomFirst ? ", device default" : "");
  }
}
}  // namespace

void init(Pix& pix, const std::string& settingsPath, bool touch, bool pinned) {
  g_pinned = pinned;
  g_zoomFirst = false;
  // (P2a) test hook: EMB_ZOOM=<D> an absolute world scale (EMB_RIG is read by rpg/main.cpp before the first bake)
  if (const char* e = std::getenv("EMB_ZOOM")) g_envZoom = std::clamp(std::atoi(e), 0, 16);
  if (pinned) {   // (Witness) nothing read, nothing written: the defaults, and fit() keeps the design canvas
    g_path.clear();
    g_set = Settings();
    g_fakeSafe = false;
    g_dirty = true;
    frame(pix);
    return;
  }
  g_path = settingsPath;
  // test hook: EMB_SETTINGS_FILE gives a scripted run a settings file of its own (persistence tests; never the player's)
  if (const char* e = std::getenv("EMB_SETTINGS_FILE")) g_path = e;
  bool coarse = touch;
#ifdef __EMSCRIPTEN__
  coarse = coarse || emb_coarse_pointer() != 0;
#endif
  // (P2c test hook) EMB_DEVICE=phone|tablet|desktop (the device class, whatever the window) or touch[@<density>] | mouse
  // (the pointer, and the device px per point the short side is measured in): the device a desktop run stands in for,
  // as a first launch. Without EMB_SETTINGS_FILE such a run never reads or writes the player's settings file.
  // e.g. EMB_DEVICE=phone --window 2556x1179 (or EMB_DEVICE=touch@3) is a first-launch iPhone 15 Pro: NEAR, D 6
  bool deviceHook = false;
  if (const char* e = std::getenv("EMB_DEVICE"); e && *e) {
    deviceHook = true;
    const int dev = deviceArg(e);
    if (dev >= 0) { g_devForce = dev; coarse = dev != DeviceDesktop; }
    else {
      coarse = std::strncmp(e, "touch", 5) == 0;
      if (coarse && e[5] == '@') g_devDensity = (float)std::max(0.0, std::atof(e + 6));
    }
    if (!std::getenv("EMB_SETTINGS_FILE")) g_path.clear();
  }
  g_coarse = coarse;
  g_set = Settings();
  g_set.mode = coarse ? Fill : PixelPerfect;   // phones fill the screen; a desktop keeps whole pixels
  g_zoomFirst = !g_path.empty() || deviceHook;   // (P2c ZOOM) cleared by load() when the file holds a zoom
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
  if (g_pinned && !g_witnessFree) {   // (Witness) the canvas stays pinned and nothing is saved; (P2a) ZOOM and RING apply
    g_set.zoom = std::clamp(s.zoom, kZoomMin, kZoomMax);
    g_set.ring = s.ring;
    g_dirty = true;
    return;
  }
  Settings n = s;
  n.mode = n.mode == PixelPerfect ? PixelPerfect : Fill;
  n.border = std::clamp(n.border, 0, kBorderMax);
  n.hud = n.hud < 0 ? -1 : std::clamp(n.hud, 0, kHudMax);
  n.zoom = std::clamp(n.zoom, kZoomMin, kZoomMax);
  if (n.mode == g_set.mode && n.border == g_set.border && n.hud == g_set.hud && n.zoom == g_set.zoom && n.ring == g_set.ring) return;
  g_set = n;
  g_dirty = true;
  g_zoomFirst = false;   // (P2c ZOOM) the zoom in use is the player's now (saved below, or kept for a scripted run)
  save();
}

int autoInset() { return g_autoInset; }
std::string describe() { return g_desc; }

// ---------------------------------------------------------------- P2a: the world scale
int d0ForHeight(int h) {
  h = std::max(1, h);
  int d = std::max(1, (h + 120) / 240);   // round(h / 240), ties up
  while (d > 1 && h > d * kZoomRowsMax) d++;   // kept inside the V bounds (never more than 300 rows, never under 150)
  while (d > 1 && h < d * kZoomRowsMin) d--;
  return d;
}
std::vector<int> levelsForHeight(int h) {
  std::vector<int> v;
  const int d0 = d0ForHeight(h);
  for (int z = kZoomMin; z <= kZoomMax; z++) {
    const int d = d0 + z;
    if (d >= 1 && d * kZoomRowsMin <= h && h <= d * kZoomRowsMax) v.push_back(d);
  }
  if (v.empty()) v.push_back(d0);   // a tiny window: the one scale it has
  return v;
}
int defaultWorldScale() { return g_d0; }
std::vector<int> zoomLevels() { return g_levels.empty() ? std::vector<int>{g_d0} : g_levels; }
int zoomLevelOf(int d) { return d - g_d0; }
int worldScale() {
  if (g_envZoom > 0) return g_envZoom;
  const std::vector<int> lv = zoomLevels();
  const int want = g_d0 + g_set.zoom;
  int best = lv.front();
  for (int d : lv) if (std::abs(d - want) < std::abs(best - want)) best = d;
  return best;
}
const char* zoomName(int offset) {
  switch (offset) {
    case -1: return "FAR";
    case 1: return "NEAR";
    case 2: return "CLOSE";
    default: return "NORMAL";
  }
}

int deviceArg(const char* name) {
  if (!name) return -1;
  if (!std::strcmp(name, "desktop")) return DeviceDesktop;
  if (!std::strcmp(name, "phone")) return DevicePhone;
  if (!std::strcmp(name, "tablet")) return DeviceTablet;
  return -1;
}

void launchAs(Pix& pix, int device, int w, int h, bool fresh) {
  if (g_pinned && !g_witnessFree) return;   // (Witness) the pinned canvas: the harness's own reset sizes the screen
  if (w > 0 && h > 0) resizeWindow(pix, w, h);
  g_devForce = std::clamp(device, (int)DeviceDesktop, (int)DeviceTablet);
  g_coarse = g_devForce != DeviceDesktop;
  if (fresh) {   // no settings file: the defaults, the mode by the pointer (EMB_SCREEN still wins, as at start)
    const Settings keep = g_set;
    g_set = Settings();
    g_set.mode = g_coarse ? Fill : PixelPerfect;
    if (const char* e = std::getenv("EMB_SCREEN")) g_set.mode = (e[0] == 'p' || e[0] == 'P') ? PixelPerfect : Fill;
    g_set.border = keep.border;   // (EMB_BORDER / EMB_HUD runs keep theirs)
    g_set.hud = keep.hud;
    g_zoomFirst = true;
  }
  g_dirty = true;
  frame(pix);
}

void witnessScreen(Pix& pix, int w, int h, const int* safeDevicePx, int mode, int zoom, bool keepZoom) {
  SDL_Window* win = pix.window();
  const int keptZoom = g_set.zoom;
  const bool keptRing = g_set.ring;
  g_set = Settings();
  g_set.zoom = keepZoom ? keptZoom : std::clamp(zoom, kZoomMin, kZoomMax);
  g_set.ring = keepZoom ? keptRing : true;
  g_zoomFirst = false;   // (P2c) a reset's zoom is the one asked for (a launchAs from the last test does not carry over)
  g_devForce = -1;
  g_fakeSafe = false;
  g_fakeSafeDev = false;
  g_witnessFree = w > 0 && h > 0;
  int want[2] = {g_witnessFree ? w : 960, g_witnessFree ? h : 540};
  if (win) resizeWindow(pix, want[0], want[1]);
  if (g_witnessFree) {
    g_set.mode = mode == PixelPerfect ? PixelPerfect : Fill;
    if (safeDevicePx) { g_fakeSafe = true; g_fakeSafeDev = true; std::memcpy(g_fakeIns, safeDevicePx, sizeof g_fakeIns); }
  }
  g_dirty = true;
  frame(pix);
}

}  // namespace screen
