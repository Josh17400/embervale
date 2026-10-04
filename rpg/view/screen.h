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
#pragma once
#include <string>

class Pix;

namespace screen {

void init(Pix& pix, const std::string& settingsPath, bool touch);   // load the settings, size the canvas
void frame(Pix& pix);                                               // follow window size changes (cheap when unchanged)

enum Mode : int { Fill = 0, PixelPerfect = 1 };
constexpr int kBorderMax = 12;   // BORDER steps of 4 points
constexpr int kHudMax = 12;      // HUD MARGIN steps of 4 logical px (-1 = AUTO)
struct Settings {
  int mode = Fill;
  int border = 0;   // 0..kBorderMax
  int hud = -1;     // -1 AUTO, 0..kHudMax
};
Settings get();
void set(const Settings& s);     // applies on the next frame() and saves the settings file
int autoInset();                 // the device safe area in logical px (the widest side), for "AUTO (n)"
std::string describe();          // "639 X 294 AT 4X" (the settings screen shows what the fit chose)

}  // namespace screen
