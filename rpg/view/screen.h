// Screen fit and player settings (M1, owner request 2026-10-04: on the iPhone the game image is much smaller than the
// screen; the settings must let the player set the screen border). VIEW lane owns this header and screen.cpp: the
// logical canvas size (Pix::W x Pix::H, adaptive width so 19.5:9 phones get a wider view), FILL / PIXEL PERFECT modes,
// the border / safe-area inset, and the small settings file (never the world save).
// main.cpp calls init() once after Pix::init and frame() at the start of every frame (window resizes, rotation).
//
// The fit: the logical HEIGHT stays near 270 and the WIDTH follows the window's aspect (a 19.5:9 phone gets about
// 585..640 x 270..294). The HUD, the touch controls and the menus stay inside the safe area (Pix::SL/ST/SR/SB), which
// always leaves at least 480 x 270 logical px, so every screen laid out for 480 x 270 fits.
//   FILL           the image covers the whole window (behind the notch too; only the HUD keeps clear of it). An
//                  integer scale when one gives a height of 270..312, otherwise a fractional scale drawn with SDL's
//                  pixel-art filter. The default on touch devices.
//   PIXEL PERFECT  the largest integer scale that still shows 480 x 270 of safe area; the window's remainder (less
//                  than one scale step) stays black. The default on desktop.
//   BORDER         a black frame around the whole image, 0..kBorderMax steps of 4 points (CSS px on the web).
//   HUD MARGIN     AUTO (the device's safe area: env(safe-area-inset-*) on the web, SDL_GetWindowSafeArea natively)
//                  or 0..kHudMax steps of 4 logical px on every side.
// Changes apply on the next frame and are written to the settings file at once (scripted runs have no file).
//
// P2a Clear Sight (VISION_PLAN 15.36 item 2, STYLE_P2 1): the WORLD is drawn at a whole number D of device px per world
// px on every screen; the UI keeps the fit above. D0 = round(device px height / 240), ties up; ZOOM steps D by -1..+2
// from D0 (FAR, NORMAL, NEAR, CLOSE), each level only where the world view keeps V = h / D within [150, 300] rows. The
// zoom and the PLAYER RING live in the settings file (EMBS version 2; a version-1 file loads its fields and the new
// ones' defaults). Test hooks: EMB_ZOOM=<D> (an absolute world scale), EMB_RIG=1|2 (the human rig, art::setRigVersion;
// read by rpg/main.cpp before the first sheet is baked).
// Phase A: the numbers are computed and reported; LENS presents the world layer at worldScale() (engine/pix.h layers).
#pragma once
#include <string>
#include <vector>

class Pix;

namespace screen {

// load the settings, size the canvas. pinned (a Witness run or a WITNESS_RECORD session): always the 480 x 270 design
// canvas, no insets, scaled to fit the window; the settings file, EMB_SCREEN / EMB_BORDER / EMB_HUD / EMB_SAFE and
// set() change nothing
void init(Pix& pix, const std::string& settingsPath, bool touch, bool pinned = false);
void frame(Pix& pix);                                               // follow window size changes (cheap when unchanged)

enum Mode : int { Fill = 0, PixelPerfect = 1 };
constexpr int kBorderMax = 12;   // BORDER steps of 4 points
constexpr int kHudMax = 12;      // HUD MARGIN steps of 4 logical px (-1 = AUTO)
constexpr int kZoomMin = -1, kZoomMax = 2;   // ZOOM: FAR (D0 - 1), NORMAL (D0), NEAR (D0 + 1), CLOSE (D0 + 2)
constexpr int kZoomRowsMin = 150, kZoomRowsMax = 300;   // the world view's height in world px at any allowed D
struct Settings {
  int mode = Fill;
  int border = 0;   // 0..kBorderMax
  int hud = -1;     // -1 AUTO, 0..kHudMax
  int zoom = 0;     // (P2a) kZoomMin..kZoomMax, an offset from D0 (a level the screen does not allow takes the nearest one)
  bool ring = true; // (P2a) PLAYER RING: the gold ring at the hero's feet (STYLE_P2 8)
};
Settings get();
void set(const Settings& s);     // applies on the next frame() and saves the settings file
int autoInset();                 // the device safe area in logical px (the widest side), for "AUTO (n)"
std::string describe();          // "639 X 294 AT 4X" (the settings screen shows what the fit chose); (P2a) plus
                                 // " WORLD 512 X 236 AT 5X" once the world layer differs from the UI's fit
// ---- P2a: the world scale (device px per world px, always whole)
int defaultWorldScale();         // D0 for this screen (round(h / 240), ties up, kept inside the V bounds)
std::vector<int> zoomLevels();   // the D of each level this screen allows, FAR first (e.g. {4, 5, 6, 7})
int zoomLevelOf(int d);          // the Settings::zoom offset of a D (d - D0)
int worldScale();                // the D in use: D0 + Settings::zoom clamped to zoomLevels(), or EMB_ZOOM / the
                                 // Witness override when set
const char* zoomName(int offset);   // "FAR", "NORMAL", "NEAR", "CLOSE"
// ---- (Witness) a reset's screen: the window resized to w x h device px, the device safe area (left, top, right,
// bottom in device px; nullptr: none), FILL (0, also for -1: the phone default) / PIXEL PERFECT (1); zoom an offset (kZoomMin..Max),
// absent (keep = false, zoom = 0: NORMAL) or carried over from the last run (keep = true: as a restart reads the
// settings file). w <= 0: back to the pinned 480 x 270 design canvas in the 960 x 540 window.
void witnessScreen(Pix& pix, int w, int h, const int* safeDevicePx, int mode, int zoom, bool keepZoom);
// (tests) the D0 / zoom-level rule for a device height (no window): what defaultWorldScale / zoomLevels give there
int d0ForHeight(int h);
std::vector<int> levelsForHeight(int h);

}  // namespace screen
