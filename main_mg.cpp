// HOLDLINE - SDL3 platform layer: window, input (keyboard/mouse/touch), fixed-timestep loop, autosave.
//   holdline.exe                         play (autosaves; Space continues)
//   holdline.exe --bot [--god]           autoplay (never touches your save)
//   holdline.exe --bot --ff 120 --shot out.png --after 2    simulate 120s, then screenshot
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "engine/pix.h"
#include "game2/mg_bot.h"
#include "game2/mg_game.h"
#include "game2/mg_render.h"

namespace {
std::string g_savePath;

bool readSave(std::vector<uint8_t>& out) {
  if (g_savePath.empty()) return false;
  FILE* f = std::fopen(g_savePath.c_str(), "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (n <= 0 || n > 4 * 1024 * 1024) { std::fclose(f); return false; }
  out.resize((size_t)n);
  size_t got = std::fread(out.data(), 1, out.size(), f);
  std::fclose(f);
  return got == out.size();
}
void writeSave(const MGame& g) {
  if (g_savePath.empty()) return;
  std::vector<uint8_t> buf;
  g.serialize(buf);
  std::string tmp = g_savePath + ".tmp";
  if (FILE* f = std::fopen(tmp.c_str(), "wb")) {
    std::fwrite(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    std::remove(g_savePath.c_str());
    std::rename(tmp.c_str(), g_savePath.c_str());
  }
}
}  // namespace

int main(int argc, char** argv) {
  bool bot = false, vsync = true, god = false, perf = false, fresh = false;
  const char* shotPath = nullptr;
  float shotAfter = 10.0f, fastForward = 0;
  uint64_t seed = 0;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--bot")) bot = true;
    else if (!std::strcmp(argv[i], "--god")) god = true;
    else if (!std::strcmp(argv[i], "--novsync")) vsync = false;
    else if (!std::strcmp(argv[i], "--perf")) perf = true;
    else if (!std::strcmp(argv[i], "--fresh")) fresh = true;
    else if (!std::strcmp(argv[i], "--shot") && i + 1 < argc) shotPath = argv[++i];
    else if (!std::strcmp(argv[i], "--after") && i + 1 < argc) shotAfter = (float)std::atof(argv[++i]);
    else if (!std::strcmp(argv[i], "--ff") && i + 1 < argc) fastForward = (float)std::atof(argv[++i]);
    else if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint64_t)std::atoll(argv[++i]);
  }
  const bool noSave = bot || shotPath || seed != 0;   // test runs must never touch the player's save

  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) { SDL_Log("SDL_Init failed: %s", SDL_GetError()); return 1; }
  if (char* pref = SDL_GetPrefPath("Josh17400", "Holdline")) { g_savePath = std::string(pref) + "save.bin"; SDL_free(pref); }

  Pix pix;
  if (!pix.init("HOLDLINE", 1440, 810, vsync)) return 1;
  Audio audio;
  audio.init();

  uint64_t startSeed = seed ? seed : (uint64_t)SDL_GetTicks() * 2654435761ull + 12345;
  MGame game(startSeed);
  MView view;
  view.init(pix, game);

  bool hasSave = false;
  if (!noSave && !fresh) {
    std::vector<uint8_t> buf;
    if (readSave(buf)) {
      MGame probe(1);
      hasSave = probe.deserialize(buf);
    }
  }
  MBot botAI;
  if (bot) {
    game.mode = MMode::Play;
    game.godMode = god;
    while (game.time < fastForward) { game.update(SIM_DT2, botAI.act(game, SIM_DT2)); game.events.clear(); }
    view.snapCamera(game);
  }

  auto startGame = [&](bool cont) {
    if (cont && hasSave) {
      std::vector<uint8_t> buf;
      if (readSave(buf) && game.deserialize(buf)) { /* loaded */ }
      else game.generate(startSeed);
    } else {
      game.generate((uint64_t)SDL_GetTicks() * 2654435761ull + 777);
    }
    game.mode = MMode::Play;
    view.snapCamera(game);
    audio.play(Sfx::Start);
  };

  bool running = true;
  Vec2 mouse(Pix::W * 0.5f, Pix::H * 0.5f);
  int dragId = -1;
  Vec2 dragStart;
  bool dragMoved = false;
  SDL_FingerID dragFinger = 0, stickFinger = 0;
  bool dragIsTouch = false, hasStick = false;
  Vec2 stickOrigin, stickCur;
  float saveT = 0;
  Uint64 freq = SDL_GetPerformanceFrequency(), prev = SDL_GetPerformanceCounter(), t0 = prev;
  double acc = 0, fpsT = 0;
  int fpsN = 0;
  bool shotTaken = false;

  auto endDrag = [&](Vec2 at) {
    if (dragId < 0) return;
    int id = dragId;
    dragId = -1;
    if (game.mode != MMode::Play) return;
    if (!dragMoved) { game.clickUnit(id); return; }
    Vec2 w = view.screenToWorld(at);
    int target = -1; float bd = 16.0f * 16.0f;
    for (const Unit& u : game.units) {
      if (u.id == id || u.st == UState::Wild) continue;
      float d = len2(u.p + Vec2(0, -5) - w);
      if (d < bd) { bd = d; target = u.id; }
    }
    if (target >= 0) game.tryMerge(id, target);
  };
  auto beginDrag = [&](Vec2 at) {
    if (game.mode != MMode::Play) return;
    int id = game.unitAt(view.screenToWorld(at), 12.0f);
    if (id >= 0) { dragId = id; dragStart = at; dragMoved = false; }
  };

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_EVENT_QUIT: running = false; break;
        case SDL_EVENT_WINDOW_FOCUS_LOST: if (!bot && game.mode == MMode::Play) game.mode = MMode::Paused; break;
        case SDL_EVENT_KEY_DOWN:
          if (e.key.repeat) break;
          switch (e.key.key) {
            case SDLK_ESCAPE:
              if (game.mode == MMode::Play) game.mode = MMode::Paused;
              else if (game.mode == MMode::Paused) game.mode = MMode::Play;
              break;
            case SDLK_F11: {
              bool fs = (SDL_GetWindowFlags(pix.window()) & SDL_WINDOW_FULLSCREEN) != 0;
              SDL_SetWindowFullscreen(pix.window(), !fs);
              break;
            }
            case SDLK_SPACE: case SDLK_RETURN:
              if (game.mode == MMode::Title) startGame(true);
              break;
            case SDLK_N:
              if (game.mode == MMode::Title) startGame(false);
              break;
            case SDLK_E: if (game.mode == MMode::Play) game.lodgeParty(); break;
            case SDLK_B: if (game.mode == MMode::Play) game.hire(); break;
            case SDLK_M: if (game.mode == MMode::Play) game.autoMerge(); break;
            default: break;
          }
          break;
        case SDL_EVENT_MOUSE_MOTION:
          SDL_ConvertEventToRenderCoordinates(pix.renderer(), &e);
          mouse = {e.motion.x, e.motion.y};
          if (dragId >= 0 && !dragIsTouch && len(mouse - dragStart) > 4.0f) dragMoved = true;
          break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
          SDL_ConvertEventToRenderCoordinates(pix.renderer(), &e);
          mouse = {e.button.x, e.button.y};
          if (e.button.button == SDL_BUTTON_LEFT) {
            if (game.mode == MMode::Title) startGame(true);
            else { dragIsTouch = false; beginDrag(mouse); }
          }
          break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
          SDL_ConvertEventToRenderCoordinates(pix.renderer(), &e);
          mouse = {e.button.x, e.button.y};
          if (e.button.button == SDL_BUTTON_LEFT && !dragIsTouch) endDrag(mouse);
          break;
        case SDL_EVENT_FINGER_DOWN: {
          int ww, wh;
          SDL_GetWindowSize(pix.window(), &ww, &wh);
          float lx, ly;
          pix.windowToLogical(e.tfinger.x * ww, e.tfinger.y * wh, lx, ly);
          Vec2 p(lx, ly);
          if (game.mode == MMode::Title) { startGame(true); break; }
          if (game.mode != MMode::Play) break;
          int id = game.unitAt(view.screenToWorld(p), 14.0f);
          if (id >= 0 && dragId < 0) { dragIsTouch = true; dragFinger = e.tfinger.fingerID; dragId = id; dragStart = p; dragMoved = false; mouse = p; }
          else if (!hasStick) { hasStick = true; stickFinger = e.tfinger.fingerID; stickOrigin = stickCur = p; }
          break;
        }
        case SDL_EVENT_FINGER_MOTION: {
          int ww, wh;
          SDL_GetWindowSize(pix.window(), &ww, &wh);
          float lx, ly;
          pix.windowToLogical(e.tfinger.x * ww, e.tfinger.y * wh, lx, ly);
          Vec2 p(lx, ly);
          if (dragId >= 0 && dragIsTouch && e.tfinger.fingerID == dragFinger) { mouse = p; if (len(p - dragStart) > 5.0f) dragMoved = true; }
          if (hasStick && e.tfinger.fingerID == stickFinger) stickCur = p;
          break;
        }
        case SDL_EVENT_FINGER_UP: {
          if (dragId >= 0 && dragIsTouch && e.tfinger.fingerID == dragFinger) endDrag(mouse);
          if (hasStick && e.tfinger.fingerID == stickFinger) hasStick = false;
          break;
        }
        default: break;
      }
    }

    Uint64 now = SDL_GetPerformanceCounter();
    double frameDt = std::min(0.1, (double)(now - prev) / (double)freq);
    prev = now;
    double wall = (double)(now - t0) / (double)freq;
    fpsT += frameDt; fpsN++;
    if (fpsT >= 1.0) {
      if (perf) std::printf("t=%.0f fps=%d units=%zu enemies=%zu\n", game.time, (int)(fpsN / fpsT + 0.5), game.units.size(), game.enemies.size());
      fpsT = 0; fpsN = 0;
    }

    // ---- input -> hero movement
    Vec2 move;
    if (bot) move = botAI.act(game, (float)frameDt);
    else {
      const bool* k = SDL_GetKeyboardState(nullptr);
      if (k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT]) move.x -= 1;
      if (k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT]) move.x += 1;
      if (k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP]) move.y -= 1;
      if (k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN]) move.y += 1;
      if (hasStick) {
        Vec2 d = stickCur - stickOrigin;
        float l = len(d);
        if (l > 5.0f) move = d * (1.0f / l) * std::min(1.0f, l / 24.0f);
        if (l > 36.0f) stickOrigin += d * ((l - 36.0f) / l);
      }
    }

    if (game.mode == MMode::Play) {
      acc += frameDt;
      int steps = 0;
      while (acc >= SIM_DT2 && steps < 12) { game.update(SIM_DT2, move); acc -= SIM_DT2; steps++; }
      if (steps == 12) acc = 0;
      saveT += (float)frameDt;
      if (!noSave && saveT > 10.0f) { saveT = 0; writeSave(game); }
    } else acc = 0;
    if (dragId >= 0 && !game.unitById(dragId)) dragId = -1;

    view.handleEvents(game, audio);
    view.update(game, audio, (float)frameDt);
    view.draw(game, pix, mouse, dragId, hasSave);

    if (shotPath && !shotTaken && wall >= shotAfter) {
      shotTaken = pix.screenshot(shotPath);
      std::printf("screenshot %s: %s\n", shotPath, shotTaken ? "ok" : "FAILED");
      running = false;
    }
    pix.end();
  }

  if (!noSave && game.mode != MMode::Title) writeSave(game);
  audio.shutdown();
  pix.shutdown();
  SDL_Quit();
  return 0;
}
