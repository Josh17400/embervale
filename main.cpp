// TAILSPIN - SDL3 platform layer: window, input, fixed-timestep loop, saves.
//   tailspin.exe                  play
//   tailspin.exe --bot            autoplay (demo / testing)
//   tailspin.exe --bot --shot out.png --after 20   autoplay, save a screenshot after 20s, quit
//   tailspin.exe --novsync        uncapped frame rate (perf testing)
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "engine/audio.h"
#include "engine/gfx.h"
#include "game/bot.h"
#include "game/game.h"
#include "game/render.h"

namespace {
std::string g_savePath;

void loadSave(SaveData& s) {
  if (g_savePath.empty()) return;
  if (FILE* f = std::fopen(g_savePath.c_str(), "r")) {
    std::fscanf(f, "%f %d %d %d", &s.bestTime, &s.bestKills, &s.bestLoop, &s.runs);
    std::fclose(f);
  }
}
void writeSave(const SaveData& s) {
  if (g_savePath.empty()) return;
  if (FILE* f = std::fopen(g_savePath.c_str(), "w")) {
    std::fprintf(f, "%f %d %d %d\n", s.bestTime, s.bestKills, s.bestLoop, s.runs);
    std::fclose(f);
  }
}

struct Touch {
  SDL_FingerID stick = 0, boost = 0;
  bool hasStick = false, hasBoost = false;
  Vec2 origin, cur;
};
}  // namespace

int main(int argc, char** argv) {
  bool bot = false, vsync = true, noPick = false, god = false, perf = false;
  float fastForward = 0;
  const char* shotPath = nullptr;
  float shotAfter = 15.0f;
  uint64_t seed = (uint64_t)SDL_GetTicks() + 1;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--bot")) bot = true;
    else if (!std::strcmp(argv[i], "--novsync")) vsync = false;
    else if (!std::strcmp(argv[i], "--god")) god = true;
    else if (!std::strcmp(argv[i], "--perf")) perf = true;
    else if (!std::strcmp(argv[i], "--ff") && i + 1 < argc) fastForward = (float)std::atof(argv[++i]);
    else if (!std::strcmp(argv[i], "--nopick")) noPick = true;   // bot leaves level-up / death screens up (screenshots)
    else if (!std::strcmp(argv[i], "--shot") && i + 1 < argc) shotPath = argv[++i];
    else if (!std::strcmp(argv[i], "--after") && i + 1 < argc) shotAfter = (float)std::atof(argv[++i]);
    else if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint64_t)std::atoll(argv[++i]);
  }

  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");   // we handle fingers ourselves
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return 1;
  }
  if (char* pref = SDL_GetPrefPath("Josh17400", "Tailspin")) { g_savePath = std::string(pref) + "save.txt"; SDL_free(pref); }

  Gfx gfx;
  if (!gfx.init("TAILSPIN", 1280, 720, vsync)) return 1;
  Audio audio;
  audio.init();
  Game game;
  View view;
  loadSave(view.save);
  game.nextSeed = seed;
  Bot botAI;
  Touch touch;

  if (bot) {
    game.startRun();
    game.god = god;
    // --ff N: simulate N seconds up front so screenshots/perf checks can land in the late game
    while (game.stats.time < fastForward && game.mode != Mode::Dead) {
      if (game.mode == Mode::LevelUp) { game.pickUpgrade(botAI.r.irange(game.numChoices)); continue; }
      game.update(SIM_DT, botAI.act(game, SIM_DT));
      game.events.clear();
    }
  }

  bool running = true, wasDead = false;
  float deadT = 0;
  Vec2 mouse(VIEW_W * 0.5f, VIEW_H * 0.5f);
  bool mouseDown = false, rightDown = false;
  Uint64 freq = SDL_GetPerformanceFrequency(), prev = SDL_GetPerformanceCounter(), t0 = prev;
  double acc = 0, fpsT = 0; int fpsN = 0;
  bool shotTaken = false;

  auto clickAt = [&](Vec2 p) {
    switch (game.mode) {
      case Mode::Title: game.startRun(); audio.play(Sfx::Start); break;
      case Mode::LevelUp:
        for (int i = 0; i < game.numChoices; i++) {
          float x, y, w, h;
          cardRect(i, game.numChoices, x, y, w, h);
          if (p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h) { game.pickUpgrade(i); break; }
        }
        break;
      case Mode::Dead:
        if (deadT > 0.35f && p.x >= 680 && p.x <= 1240 && p.y >= 800 && p.y <= 910) { game.startRun(); audio.play(Sfx::Start); }
        break;
      default: break;
    }
  };

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_EVENT_QUIT: running = false; break;
        case SDL_EVENT_WINDOW_FOCUS_LOST: if (!bot) game.setPaused(true); break;
        case SDL_EVENT_KEY_DOWN:
          if (e.key.repeat) break;
          switch (e.key.key) {
            case SDLK_ESCAPE:
              if (game.mode == Mode::Play) game.setPaused(true);
              else if (game.mode == Mode::Paused) game.setPaused(false);
              break;
            case SDLK_F11: {
              bool fs = (SDL_GetWindowFlags(gfx.window()) & SDL_WINDOW_FULLSCREEN) != 0;
              SDL_SetWindowFullscreen(gfx.window(), !fs);
              break;
            }
            case SDLK_1: case SDLK_2: case SDLK_3:
              if (game.mode == Mode::LevelUp) game.pickUpgrade(e.key.key - SDLK_1);
              break;
            case SDLK_SPACE: case SDLK_RETURN: case SDLK_R:
              if (game.mode == Mode::Title) { game.startRun(); audio.play(Sfx::Start); }
              else if (game.mode == Mode::Dead && deadT > 0.35f) { game.startRun(); audio.play(Sfx::Start); }
              break;
            default: break;
          }
          break;
        case SDL_EVENT_MOUSE_MOTION:
          SDL_ConvertEventToRenderCoordinates(gfx.renderer(), &e);
          mouse = {e.motion.x, e.motion.y};
          break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
          SDL_ConvertEventToRenderCoordinates(gfx.renderer(), &e);
          mouse = {e.button.x, e.button.y};
          if (e.button.button == SDL_BUTTON_LEFT) { mouseDown = true; clickAt(mouse); }
          if (e.button.button == SDL_BUTTON_RIGHT) rightDown = true;
          break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
          if (e.button.button == SDL_BUTTON_LEFT) mouseDown = false;
          if (e.button.button == SDL_BUTTON_RIGHT) rightDown = false;
          break;
        case SDL_EVENT_FINGER_DOWN: {
          int ww, wh;
          SDL_GetWindowSize(gfx.window(), &ww, &wh);
          float lx, ly;
          gfx.windowToLogical(e.tfinger.x * ww, e.tfinger.y * wh, lx, ly);
          Vec2 p(lx, ly);
          if (game.mode != Mode::Play) { clickAt(p); break; }
          if (!touch.hasStick) { touch.hasStick = true; touch.stick = e.tfinger.fingerID; touch.origin = touch.cur = p; }
          else if (!touch.hasBoost) { touch.hasBoost = true; touch.boost = e.tfinger.fingerID; }
          break;
        }
        case SDL_EVENT_FINGER_MOTION:
          if (touch.hasStick && e.tfinger.fingerID == touch.stick) {
            int ww, wh;
            SDL_GetWindowSize(gfx.window(), &ww, &wh);
            float lx, ly;
            gfx.windowToLogical(e.tfinger.x * ww, e.tfinger.y * wh, lx, ly);
            touch.cur = {lx, ly};
          }
          break;
        case SDL_EVENT_FINGER_UP:
          if (touch.hasStick && e.tfinger.fingerID == touch.stick) touch.hasStick = false;
          if (touch.hasBoost && e.tfinger.fingerID == touch.boost) touch.hasBoost = false;
          break;
        default: break;
      }
    }

    // ---- timing
    Uint64 now = SDL_GetPerformanceCounter();
    double frameDt = std::min(0.1, (double)(now - prev) / (double)freq);
    prev = now;
    double wall = (double)(now - t0) / (double)freq;
    fpsT += frameDt; fpsN++;
    if (fpsT >= 1.0) {
      char title[96];
      std::snprintf(title, sizeof title, "TAILSPIN  -  %d fps  -  %zu enemies", (int)(fpsN / fpsT + 0.5), game.enemies.size());
      SDL_SetWindowTitle(gfx.window(), title);
      if (perf) std::printf("t=%.0f fps=%d enemies=%zu gems=%zu\n", game.stats.time, (int)(fpsN / fpsT + 0.5), game.enemies.size(), game.gems.size());
      fpsT = 0; fpsN = 0;
    }

    // ---- input -> sim
    Input in;
    if (bot) {
      in = botAI.act(game, SIM_DT);
      if (!noPick) {
        if (game.mode == Mode::LevelUp) game.pickUpgrade(botAI.r.irange(game.numChoices));
        if (game.mode == Mode::Dead) game.startRun();
      }
    } else {
      const bool* keys = SDL_GetKeyboardState(nullptr);
      bool kl = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
      bool kr = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
      if (kl != kr) in.steer = fromAngle(game.heading + (kr ? 1.0f : -1.0f));
      else if (touch.hasStick) {
        Vec2 d = touch.cur - touch.origin;
        float l = len(d);
        if (l > 14.0f) in.steer = d * (1.0f / l) * std::min(1.0f, l / 70.0f);
        if (l > 120.0f) touch.origin += d * ((l - 120.0f) / l);   // floating stick follows the thumb
      } else {
        Vec2 headScr = gfx.toScreen(game.head);
        Vec2 d = mouse - headScr;
        float l = len(d);
        if (l > 24.0f) in.steer = d * (1.0f / l);
      }
      in.boost = keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_LSHIFT] || rightDown || touch.hasBoost;
      (void)mouseDown;
    }

    if (game.mode == Mode::Dead) deadT += (float)frameDt; else deadT = 0;

    if (view.hitstop > 0) {
      view.hitstop -= (float)frameDt;
      acc = 0;
    } else {
      acc += frameDt;
      int steps = 0;
      while (acc >= SIM_DT && steps < 12) {
        game.update(SIM_DT, in);
        acc -= SIM_DT;
        steps++;
        if (view.hitstop > 0 || game.mode != Mode::Play) { acc = 0; break; }
        if (!game.events.empty()) view.handleEvents(game, audio);   // lets hitstop kick in mid-frame
      }
      if (steps == 12) acc = 0;
    }
    view.handleEvents(game, audio);

    if (game.mode == Mode::Dead && !wasDead) {
      SaveData& s = view.save;
      s.runs++;
      view.newBest = game.stats.time > s.bestTime;
      if (view.newBest) s.bestTime = game.stats.time;
      s.bestKills = std::max(s.bestKills, game.stats.kills);
      s.bestLoop = std::max(s.bestLoop, game.stats.bestLoop);
      writeSave(s);
    }
    wasDead = game.mode == Mode::Dead;

    view.update(game, audio, (float)frameDt);
    view.draw(game, gfx, audio.beat(), mouse);

    if (shotPath && !shotTaken && wall >= shotAfter) {
      shotTaken = gfx.screenshot(shotPath);   // read back before present
      std::printf("screenshot %s: %s\n", shotPath, shotTaken ? "ok" : "FAILED");
      running = false;
    }
    gfx.end();
  }

  audio.shutdown();
  gfx.shutdown();
  SDL_Quit();
  return 0;
}
