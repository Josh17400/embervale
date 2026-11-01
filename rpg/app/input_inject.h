// rpg/app/input_inject.h - input the game pushes itself (SOLID 3.1c): a script's or the Witness harness's keys, clicks,
// fingers and mouse go through the very SDL event paths real input takes (View::event; held keys through
// View::scriptHold), so a test drives the game as a player would. The App's members (setMove, pushKey, toWindow,
// pushFinger, pushClick, pushMouse) are defined in input_inject.cpp.
#pragma once
#include <SDL3/SDL.h>

class Pix;

// every event the game pushes itself (a script's or the harness's keys, clicks and fingers) goes through pushEvent.
// (Witness watch mode, WITNESS_WATCH=1) the window is shown, but only that injected input may reach gameplay: the
// event filter drops the real keyboard, mouse and touch, so a viewer's stray click cannot change a run whose replay
// must stay exact (and the window never takes the keyboard focus: SDL's key state stays empty)
void pushEvent(SDL_Event& ev);
// (Witness watch mode, WITNESS_WATCH=1) show the window but drop the real keyboard, mouse and touch (realInputFilter)
void watchWindow(Pix& pix);
