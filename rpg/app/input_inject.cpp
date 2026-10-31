// rpg/app/input_inject.cpp - see input_inject.h
#include "rpg/app/input_inject.h"
#include <algorithm>
#include "engine/pix.h"
#include "rpg/app/app.h"

namespace {
bool g_dropRealInput = false, g_injecting = false;
bool SDLCALL realInputFilter(void*, SDL_Event* e) {
  if (!g_dropRealInput || g_injecting) return true;
  switch (e->type) {
    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: case SDL_EVENT_TEXT_INPUT: case SDL_EVENT_TEXT_EDITING:
    case SDL_EVENT_MOUSE_MOTION: case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP: case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_FINGER_DOWN: case SDL_EVENT_FINGER_UP: case SDL_EVENT_FINGER_MOTION: case SDL_EVENT_FINGER_CANCELED:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN: case SDL_EVENT_GAMEPAD_BUTTON_UP: case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    case SDL_EVENT_PEN_DOWN: case SDL_EVENT_PEN_UP: case SDL_EVENT_PEN_MOTION:
    case SDL_EVENT_DROP_FILE: case SDL_EVENT_DROP_TEXT:
      return false;
    default: return true;
  }
}
}  // namespace

void pushEvent(SDL_Event& ev) {
  g_injecting = true;
  SDL_PushEvent(&ev);   // (SDL calls the filter right here, synchronously)
  g_injecting = false;
}

void watchWindow(Pix& pix) {
  g_dropRealInput = true;
  SDL_SetEventFilter(realInputFilter, nullptr);
  SDL_SetWindowFocusable(pix.window(), false);   // (no keyboard focus: real keys never reach SDL's key state)
  SDL_SetWindowResizable(pix.window(), false);
  SDL_SetWindowTitle(pix.window(), "EMBERVALE (Witness: watching)");
  // Windows starts the harness's process with its first window hidden (the first ShowWindow call is turned into a
  // hide), so the window is shown, hidden and shown again
  SDL_ShowWindow(pix.window());
  SDL_HideWindow(pix.window());
  SDL_ShowWindow(pix.window());
}

void App::setMove(bool up, bool left, bool down, bool right) {
  view.scriptHold(SDL_SCANCODE_W, up); view.scriptHold(SDL_SCANCODE_A, left);
  view.scriptHold(SDL_SCANCODE_S, down); view.scriptHold(SDL_SCANCODE_D, right);
}
void App::pushKey(SDL_Scancode sc, bool down) {
  SDL_Event ev;
  SDL_zero(ev);
  ev.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
  ev.key.timestamp = SDL_GetTicksNS();
  ev.key.windowID = SDL_GetWindowID(pix.window());
  ev.key.scancode = sc;
  ev.key.key = SDL_GetKeyFromScancode(sc, SDL_KMOD_NONE, false);
  ev.key.down = down;
  pushEvent(ev);
}
void App::toWindow(float lx, float ly, float& wx, float& wy) { SDL_RenderCoordinatesToWindow(pix.renderer(), lx, ly, &wx, &wy); }
void App::pushFinger(SDL_EventType type, SDL_FingerID id, float lx, float ly, float pressure) {
  float wx, wy;
  int ww = 1, wh = 1;
  toWindow(lx, ly, wx, wy);
  SDL_GetWindowSize(pix.window(), &ww, &wh);
  SDL_Event ev;
  SDL_zero(ev);
  ev.type = type;
  ev.tfinger.timestamp = SDL_GetTicksNS();
  ev.tfinger.windowID = SDL_GetWindowID(pix.window());
  ev.tfinger.touchID = 1;
  ev.tfinger.fingerID = id;
  ev.tfinger.x = wx / (float)std::max(1, ww);
  ev.tfinger.y = wy / (float)std::max(1, wh);
  ev.tfinger.pressure = pressure;
  pushEvent(ev);
}
void App::pushClick(float lx, float ly, bool down) {
  float wx, wy;
  toWindow(lx, ly, wx, wy);
  SDL_Event ev;
  SDL_zero(ev);
  ev.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
  ev.button.timestamp = SDL_GetTicksNS();
  ev.button.windowID = SDL_GetWindowID(pix.window());
  ev.button.which = 0;
  ev.button.button = SDL_BUTTON_LEFT;
  ev.button.down = down;
  ev.button.clicks = 1;
  ev.button.x = wx; ev.button.y = wy;
  pushEvent(ev);
}
void App::pushMouse(SDL_EventType type, float lx, float ly, int button) {
  float wx, wy;
  toWindow(lx, ly, wx, wy);
  SDL_Event ev;
  SDL_zero(ev);
  ev.type = type;
  if (type == SDL_EVENT_MOUSE_MOTION) {
    ev.motion.windowID = SDL_GetWindowID(pix.window());
    ev.motion.x = wx; ev.motion.y = wy;
  } else {
    ev.button.windowID = SDL_GetWindowID(pix.window());
    ev.button.button = (Uint8)(button == 1 ? SDL_BUTTON_MIDDLE : button == 2 ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT);
    ev.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    ev.button.clicks = 1;
    ev.button.x = wx; ev.button.y = wy;
  }
  pushEvent(ev);
}
