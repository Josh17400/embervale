// The SETTINGS screen (M1, owner request 2026-10-04: "allow in settings to set screen border"): SCREEN mode (FILL /
// PIXEL PERFECT), BORDER, HUD MARGIN and the touch controls. Reached from the title and from the menu's SYSTEM tab;
// changes apply at once (screen.cpp refits on the next frame) and persist in the settings file, never the save.
// Touch: finger-sized rows with < > arrows either side of the value; a tap on a slider sets it where it lands.
// Keyboard: up/down pick a row, left/right change it, Enter toggles (or closes on DONE), Esc closes.
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include "rpg/view/screen.h"
#include "rpg/view/view.h"

namespace {
const Color kGold(0.98f, 0.82f, 0.42f), kText(0.93f, 0.9f, 0.82f), kDim(0.62f, 0.58f, 0.52f);
enum Row { R_SCREEN, R_BORDER, R_HUD, R_TOUCH, R_DONE, R_COUNT };
const char* kRowName[R_COUNT] = {"SCREEN", "BORDER", "HUD MARGIN", "TOUCH CONTROLS", ""};

// the layout on the 480 x 270 box, shared by drawing and taps
struct Lay { float x, y, w, h, rowY, pitch, rowH, arrowL, valX, valW, arrowR, arrowW; };
Lay lay(bool touch) {
  Lay L;
  L.x = 34; L.y = 12; L.w = 412; L.h = 246;
  L.rowY = 58;
  L.pitch = touch ? 30.0f : 26.0f;
  L.rowH = touch ? 26.0f : 22.0f;
  L.arrowW = touch ? 30.0f : 24.0f;
  L.arrowL = 196;
  L.valX = L.arrowL + L.arrowW + 4;
  L.arrowR = 412 - L.arrowW;
  L.valW = L.arrowR - 4 - L.valX;
  return L;
}
bool inR(Vec2 p, float x, float y, float w, float h) { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }

std::string valueText(int row, const screen::Settings& s, bool touch) {
  switch (row) {
    case R_SCREEN: return s.mode == screen::PixelPerfect ? "PIXEL PERFECT" : "FILL";
    case R_BORDER: return s.border == 0 ? "NONE" : std::to_string(s.border * 4) + " PX";
    case R_HUD: return s.hud < 0 ? "AUTO (" + std::to_string(screen::autoInset()) + ")" : std::to_string(s.hud * 4) + " PX";
    case R_TOUCH: return touch ? "ON" : "OFF";
    default: return "";
  }
}
const char* helpText(int row) {
  switch (row) {
    case R_SCREEN: return "FILL: THE PICTURE COVERS THE WHOLE SCREEN.  PIXEL PERFECT: EVERY PIXEL THE SAME SIZE, A THIN BLACK EDGE MAY SHOW.";
    case R_BORDER: return "A BLACK FRAME AROUND THE PICTURE. RAISE IT IF THE EDGES OF THE GAME HIDE UNDER THE NOTCH OR THE ROUNDED CORNERS.";
    case R_HUD: return "HOW FAR THE BARS, MAP AND BUTTONS KEEP FROM THE EDGES (THE GOLD CORNERS). AUTO FOLLOWS THE PHONE'S NOTCH.";
    case R_TOUCH: return "THE ON-SCREEN STICK AND BUTTONS. THEY ALSO TURN ON BY THEMSELVES WHEN YOU TOUCH THE SCREEN.";
    default: return "CHANGES APPLY AT ONCE AND ARE KEPT FOR NEXT TIME.";
  }
}
// a row's value one step left (-1) or right (+1)
void stepRow(int row, int dir, bool& touch) {
  screen::Settings s = screen::get();
  switch (row) {
    case R_SCREEN: s.mode = s.mode == screen::Fill ? screen::PixelPerfect : screen::Fill; break;
    case R_BORDER: s.border = std::clamp(s.border + dir, 0, screen::kBorderMax); break;
    case R_HUD: s.hud = std::clamp(s.hud + dir, -1, screen::kHudMax); break;
    case R_TOUCH: touch = !touch; break;
    default: break;
  }
  screen::set(s);
}
}  // namespace

void View::drawSafeGuides() {
  Pix& P = *pix_;
  const float L = (float)Pix::SL, T = (float)Pix::ST, R = (float)(Pix::W - Pix::SR) - 1, B = (float)(Pix::H - Pix::SB) - 1;
  const float a = 0.65f + 0.25f * std::sin(t_ * 3);
  const Color c(kGold.r, kGold.g, kGold.b, a), sh(0, 0, 0, 0.6f * a);
  const float n = 12;
  for (int pass = 0; pass < 2; pass++) {
    const Color k = pass == 0 ? sh : c;
    const float o = pass == 0 ? 1.0f : 0.0f;
    P.rect(L + o, T + o, n, 2, k); P.rect(L + o, T + o, 2, n, k);
    P.rect(R - n + 1 + o, T + o, n, 2, k); P.rect(R - 1 + o, T + o, 2, n, k);
    P.rect(L + o, B - 1 + o, n, 2, k); P.rect(L + o, B - n + 1 + o, 2, n, k);
    P.rect(R - n + 1 + o, B - 1 + o, n, 2, k); P.rect(R - 1 + o, B - n + 1 + o, 2, n, k);
  }
}

void View::drawSettings() {
  Pix& P = *pix_;
  P.rect(0, 0, Pix::W, Pix::H, Color(0, 0, 0, 0.45f));
  drawSafeGuides();
  const UiBox box = uiBox(270);
  P.pushBox(box.x, box.y, box.w, box.h);
  const Lay Y = lay(touchUI);
  const screen::Settings s = screen::get();
  setSel_ = std::clamp(setSel_, 0, R_COUNT - 1);
  panel(Y.x, Y.y, Y.w, Y.h, 0.95f);
  P.textS(240, Y.y + 10, "SETTINGS", 2, kGold, 1);
  P.text(240, Y.y + 30, "SCREEN " + screen::describe(), 1, kDim, 1);
  for (int r = 0; r < R_COUNT; r++) {
    const float y = Y.rowY + r * Y.pitch;
    const bool sel = r == setSel_;
    if (r == R_DONE) {
      button(240 - 60, y + 2, 120, Y.rowH, "DONE", sel);
      continue;
    }
    if (sel) P.rect(Y.x + 6, y, Y.w - 12, Y.rowH, Color(0.3f, 0.22f, 0.12f, 0.75f));
    else if (touchUI && (r & 1)) P.rect(Y.x + 6, y, Y.w - 12, Y.rowH, Color(0.16f, 0.12f, 0.08f, 0.35f));
    P.text(Y.x + 14, y + std::floor((Y.rowH - 7) / 2), kRowName[r], 1, sel ? kGold : kText);
    button(Y.arrowL, y + 2, Y.arrowW, Y.rowH - 4, "<", false);
    button(Y.arrowR, y + 2, Y.arrowW, Y.rowH - 4, ">", false);
    const std::string v = valueText(r, s, touchUI);
    if (r == R_BORDER || r == R_HUD) {
      // a slider: one notch per step (HUD MARGIN's first notch is AUTO), filled up to the value
      const int steps = r == R_BORDER ? screen::kBorderMax + 1 : screen::kHudMax + 2;
      const int at = r == R_BORDER ? s.border : s.hud + 1;
      const float nw = Y.valW / steps;
      for (int i = 0; i < steps; i++) {
        const float nx = Y.valX + i * nw;
        const bool on = i <= at;
        Color nc = on ? (i == at ? Color(1, 0.9f, 0.55f) : Color(0.72f, 0.55f, 0.26f)) : Color(0.2f, 0.17f, 0.14f);
        if (r == R_HUD && i == 0) nc = at == 0 ? Color(0.55f, 0.85f, 1.0f) : Color(0.25f, 0.4f, 0.5f);
        P.rect(nx + 1, y + Y.rowH - 7, nw - 2, 3, nc);
      }
      P.textS(Y.valX + Y.valW / 2, y + 4, v, 1, sel ? Color(1, 0.95f, 0.8f) : kText, 1);
    } else {
      P.textS(Y.valX + Y.valW / 2, y + std::floor((Y.rowH - 7) / 2), v, 1, sel ? Color(1, 0.95f, 0.8f) : kText, 1);
    }
  }
  // what the picked row does, wrapped under the rows
  const float hy = Y.rowY + R_COUNT * Y.pitch + 4;
  wrapText(Y.x + 14, hy, Y.w - 28, helpText(setSel_), kDim, -1, 9);
  if (!touchUI) P.text(240, Y.y + Y.h - 11, "UP/DOWN PICK   LEFT/RIGHT CHANGE   ESC CLOSE", 1, Color(kDim.r, kDim.g, kDim.b, 0.8f), 1);
  P.popBox();
}

void View::settingsKey(int key) {
  bool touch = touchUI;
  if (key == SDLK_ESCAPE || key == SDLK_TAB) { settingsOpen_ = false; audio_->play(Sfx::MenuBack); return; }
  if (key == SDLK_UP || key == SDLK_W) { setSel_ = (setSel_ + R_COUNT - 1) % R_COUNT; audio_->play(Sfx::MenuMove); return; }
  if (key == SDLK_DOWN || key == SDLK_S) { setSel_ = (setSel_ + 1) % R_COUNT; audio_->play(Sfx::MenuMove); return; }
  if (key == SDLK_LEFT || key == SDLK_A || key == SDLK_RIGHT || key == SDLK_D) {
    if (setSel_ != R_DONE) { stepRow(setSel_, (key == SDLK_LEFT || key == SDLK_A) ? -1 : 1, touch); touchUI = touch; audio_->play(Sfx::MenuMove); }
    return;
  }
  if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_KP_ENTER) {
    if (setSel_ == R_DONE) { settingsOpen_ = false; audio_->play(Sfx::MenuBack); return; }
    if (setSel_ == R_SCREEN || setSel_ == R_TOUCH) { stepRow(setSel_, 1, touch); touchUI = touch; audio_->play(Sfx::MenuSelect); }
    else { setSel_++; audio_->play(Sfx::MenuMove); }   // on a slider Enter moves on (left/right set it)
  }
}

void View::settingsTap(Vec2 p) {
  const UiBox box = uiBox(270);
  p = inBox(box, p);
  const Lay Y = lay(touchUI);
  bool touch = touchUI;
  for (int r = 0; r < R_COUNT; r++) {
    const float y = Y.rowY + r * Y.pitch;
    if (!inR(p, Y.x, y - (Y.pitch - Y.rowH) / 2, Y.w, Y.pitch)) continue;   // each row owns its whole pitch
    setSel_ = r;
    if (r == R_DONE) { if (std::fabs(p.x - 240) < 80) { settingsOpen_ = false; audio_->play(Sfx::MenuBack); } return; }
    if (p.x >= Y.arrowL - 6 && p.x < Y.arrowL + Y.arrowW + 2) stepRow(r, -1, touch);
    else if (p.x >= Y.arrowR - 2 && p.x < Y.arrowR + Y.arrowW + 6) stepRow(r, 1, touch);
    else if (p.x >= Y.valX && p.x < Y.valX + Y.valW) {
      if (r == R_BORDER || r == R_HUD) {   // a slider: the notch under the finger
        const int steps = r == R_BORDER ? screen::kBorderMax + 1 : screen::kHudMax + 2;
        const int i = std::clamp((int)((p.x - Y.valX) / (Y.valW / steps)), 0, steps - 1);
        screen::Settings s = screen::get();
        if (r == R_BORDER) s.border = i; else s.hud = i - 1;
        screen::set(s);
      } else stepRow(r, 1, touch);
    }
    touchUI = touch;
    audio_->play(Sfx::MenuMove);
    return;
  }
}
