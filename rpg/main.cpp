// EMBERVALE - SDL3 platform layer: window, events, fixed-timestep loop, autosave.
//   embervale.exe                                   play (autosaves; title offers Continue)
//   embervale.exe --play --seed 7                   skip the title, fresh world (never touches your save)
//   embervale.exe --play --goto city|town|cave|ruin|camp|lair [--enter] [--hour 22] [--menu 0-4]
//                 --shot out.png --after 2           screenshot after N seconds, then quit
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <functional>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "engine/pix.h"
#include "rpg/sim/game.h"
#include "rpg/view/view.h"

namespace {
std::string g_savePath;

bool readSave(std::vector<uint8_t>& out) {
  if (g_savePath.empty()) return false;
  FILE* f = std::fopen(g_savePath.c_str(), "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (n <= 0 || n > 16 * 1024 * 1024) { std::fclose(f); return false; }
  out.resize((size_t)n);
  size_t got = std::fread(out.data(), 1, out.size(), f);
  std::fclose(f);
  return got == out.size();
}
void writeSave(const Game& g) {
  if (g_savePath.empty() || g.mode == Mode::Title) return;
  std::vector<uint8_t> buf;
  g.serialize(buf);
  std::string tmp = g_savePath + ".tmp";
  if (FILE* f = std::fopen(tmp.c_str(), "wb")) {
    std::fwrite(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    std::remove(g_savePath.c_str());
    std::rename(tmp.c_str(), g_savePath.c_str());
  }
#ifdef __EMSCRIPTEN__
  // persist the in-memory file system to IndexedDB (mounted at /save by the web shell)
  EM_ASM(FS.syncfs(false, function(err) { if (err) console.log('save sync failed', err); }););
#endif
}
}  // namespace

int main(int argc, char** argv) {
  bool vsync = true, perf = false, play = false, enter = false, god = false;
  const char* shotPath = nullptr;
  const char* gotoWhat = nullptr;
  float shotAfter = 3.0f, hourSet = -1;
  int menuTab = -1;
  const char* fight = nullptr;
  int talkStep = -1;   // PT test-only: 0 = innkeeper greeting, 1 = job offer, 2 = shop
  uint64_t seed = 0;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--novsync")) vsync = false;
    else if (!std::strcmp(argv[i], "--perf")) perf = true;
    else if (!std::strcmp(argv[i], "--play")) play = true;
    else if (!std::strcmp(argv[i], "--enter")) enter = true;
    else if (!std::strcmp(argv[i], "--god")) god = true;
    else if (!std::strcmp(argv[i], "--shot") && i + 1 < argc) shotPath = argv[++i];
    else if (!std::strcmp(argv[i], "--after") && i + 1 < argc) shotAfter = (float)std::atof(argv[++i]);
    else if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint64_t)std::atoll(argv[++i]);
    else if (!std::strcmp(argv[i], "--goto") && i + 1 < argc) gotoWhat = argv[++i];
    else if (!std::strcmp(argv[i], "--hour") && i + 1 < argc) hourSet = (float)std::atof(argv[++i]);
    else if (!std::strcmp(argv[i], "--menu") && i + 1 < argc) menuTab = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--fight") && i + 1 < argc) fight = argv[++i];
    else if (!std::strcmp(argv[i], "--talk") && i + 1 < argc) talkStep = std::atoi(argv[++i]);   // PT test-only
  }
  const bool noSave = play || shotPath || seed != 0;   // test runs never touch the player's save

  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
  SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) { SDL_Log("SDL_Init failed: %s", SDL_GetError()); return 1; }
#ifdef __EMSCRIPTEN__
  g_savePath = "/save/save.bin";
#else
  if (char* pref = SDL_GetPrefPath("Josh17400", "Embervale")) { g_savePath = std::string(pref) + "save.bin"; SDL_free(pref); }
#endif

  Pix pix;
  if (!pix.init("EMBERVALE", 1440, 810, vsync)) return 1;
  static Audio audio;   // ~175 KB: keep it off the stack
  audio.init();

  uint64_t startSeed = seed ? seed : (uint64_t)SDL_GetTicks() * 2654435761ull + 12345;
  Game game(startSeed);
  game.newGame(startSeed);   // title screen drifts over this world
  game.mode = Mode::Title;
  View view;
  view.init(pix, audio);
#if defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_ANDROID)
  view.touchUI = true;
#endif

  bool hasSave = false;
  if (!noSave) {
    std::vector<uint8_t> buf;
    if (readSave(buf)) { Game probe(1); hasSave = probe.deserialize(buf); }
  }

  auto startNew = [&](uint64_t s) {
    game.newGame(s);
    game.mode = Mode::Play;
    view.snap(game);
    audio.play(Sfx::QuestStart);
  };
  if (play) {
    startNew(startSeed);
    game.godMode = god;
    if (hourSet >= 0) game.hour = hourSet;
    if (gotoWhat) {
      SiteType want = SiteType::City;
      std::string w = gotoWhat;
      if (w == "town") want = SiteType::Town; else if (w == "village") want = SiteType::Village; else if (w == "cave") want = SiteType::Cave;
      else if (w == "ruin") want = SiteType::Ruin; else if (w == "camp") want = SiteType::BanditCamp; else if (w == "lair") want = SiteType::DragonLair;
      else if (w == "shrine") want = SiteType::Shrine;
      int si = game.world.nearestSite(game.world.sites[game.world.startSite].ex, game.world.sites[game.world.startSite].ey, want);
      if (si >= 0) {
        game.world.sites[si].discovered = true;
        game.fastTravel(si);
        if (enter && (want == SiteType::Cave || want == SiteType::Ruin)) {
          const Site& s = game.world.sites[si];
          game.pl().p = Vec2(s.ex * 16 + 8.0f, s.ey * 16 + 6.0f);
          game.update(SIM_DT, Input());
        }
        if (enter && (want == SiteType::City || want == SiteType::Town || want == SiteType::Village)) {
          const Site& s = game.world.sites[si];
          for (int b = s.bldgFirst; b < s.bldgFirst + s.bldgCount; b++)
            if (game.world.over.bldgs[b].type == art::Building::Inn) {
              game.pl().p = Vec2(game.world.over.bldgs[b].doorX() * 16 + 8.0f, game.world.over.bldgs[b].doorY() * 16 + 10.0f);
              game.update(SIM_DT, Input());
              break;
            }
        }
        game.sleepFade = 0;
      }
    }
    if (talkStep >= 0 && game.inside) {   // PT test-only: talk to the innkeeper
      for (size_t k = 1; k < game.actors.size(); k++)
        if (game.actors[k].role == Role::Innkeeper) {
          game.pl().p = game.actors[k].p + Vec2(0, 20);
          Input ti; ti.interact = true;
          game.update(SIM_DT, ti);
          if (game.mode == Mode::Dialogue && talkStep >= 1) {
            const char* want = talkStep == 1 ? "WORK" : "WARES";
            for (size_t o = 0; o < game.dlg.opts.size(); o++) if (game.dlg.opts[o].label.find(want) != std::string::npos) { game.dialogueChoose((int)o); break; }
          }
          break;
        }
    }
    view.snap(game);
    if (menuTab >= 0) view.openMenu(game, menuTab);
    if (fight) {
      static const char* names[] = {"wolf", "boar", "bear", "slime", "spider", "bat", "skeleton", "draugr", "goblin", "troll", "wraith", "mudcrab", "icewolf", "frostspider", "sandworm", "dragon"};
      for (int m = 0; m < (int)art::Monster::COUNT; m++)
        if (!std::strcmp(fight, names[m])) game.debugSpawn((art::Monster)m, m == (int)art::Monster::Dragon ? 1 : 3, 60);
    }
  }
  (void)menuTab;

  bool running = true;
  float saveT = 0;
  Uint64 freq = SDL_GetPerformanceFrequency(), prev = SDL_GetPerformanceCounter(), t0 = prev;
  double acc = 0, fpsT = 0;
  int fpsN = 0;
  bool shotTaken = false;

  // one frame of the game; desktop loops on it, the browser calls it once per animation frame
  auto frame = [&]() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_EVENT_QUIT: running = false; break;
        case SDL_EVENT_WILL_ENTER_BACKGROUND: if (!noSave) writeSave(game); break;
        case SDL_EVENT_WINDOW_FOCUS_LOST: case SDL_EVENT_WINDOW_HIDDEN:
          if (!shotPath && game.mode == Mode::Play) game.mode = Mode::Paused;
          if (!noSave) writeSave(game);   // phones kill backgrounded web apps without warning
          break;
        case SDL_EVENT_KEY_DOWN:
          if (e.key.key == SDLK_F11) {
            bool fs = (SDL_GetWindowFlags(pix.window()) & SDL_WINDOW_FULLSCREEN) != 0;
            SDL_SetWindowFullscreen(pix.window(), !fs);
            break;
          }
          view.event(e, game);
          break;
        default: view.event(e, game); break;
      }
    }
    if (view.wantsQuit) {
      if (game.mode == Mode::Title) running = false;
      else { if (!noSave) writeSave(game); hasSave = !noSave; game.mode = Mode::Title; view.wantsQuit = false; }
    }
    if (view.wantContinue) {
      view.wantContinue = false;
      std::vector<uint8_t> buf;
      if (readSave(buf) && game.deserialize(buf)) { game.mode = Mode::Play; view.snap(game); audio.play(Sfx::Discover); }
      else startNew((uint64_t)SDL_GetTicks() * 2654435761ull + 777);
    }
    if (view.wantNewGame) { view.wantNewGame = false; startNew((uint64_t)SDL_GetTicks() * 2654435761ull + 777); if (!noSave) writeSave(game); hasSave = !noSave; }
    if (view.wantSave) { view.wantSave = false; if (!noSave) writeSave(game); }

    Uint64 now = SDL_GetPerformanceCounter();
    double frameDt = std::min(0.1, (double)(now - prev) / (double)freq);
    prev = now;
    double wall = (double)(now - t0) / (double)freq;
    fpsT += frameDt; fpsN++;
    if (fpsT >= 1.0) {
      if (perf) std::printf("fps=%d actors=%zu mode=%d\n", (int)(fpsN / fpsT + 0.5), game.actors.size(), (int)game.mode);
      fpsT = 0; fpsN = 0;
    }

    Input in = view.input(game);
    // one-shot presses must survive render frames in which no sim step runs (on 120/144 Hz displays, about half
    // of all frames): the view clears them every frame, so carry them until a step actually consumes them
    static Input latched;
    in.attack |= latched.attack; in.bow |= latched.bow; in.spell |= latched.spell; in.roll |= latched.roll;
    in.interact |= latched.interact; in.potion |= latched.potion; in.swapSpell |= latched.swapSpell;
    latched = Input();
    if (game.mode == Mode::Play) {
      acc += frameDt;
      int steps = 0;
      bool first = true;
      while (acc >= SIM_DT && steps < 8) {
        game.update(SIM_DT, in);
        if (first) { in.attack = in.bow = in.spell = in.roll = in.interact = in.potion = in.swapSpell = false; first = false; }
        acc -= SIM_DT; steps++;
      }
      if (steps == 0) { latched = in; latched.move = Vec2(); }
      if (steps == 8) acc = 0;
      saveT += (float)frameDt;
      if (!noSave && saveT > 20.0f) { saveT = 0; writeSave(game); }
    } else {
      acc = 0;
      if (game.mode == Mode::Title) game.time += (float)frameDt;
    }
    view.update(game, (float)frameDt);
    view.draw(game, hasSave);

    if (shotPath && !shotTaken && wall >= shotAfter) {
      shotTaken = pix.screenshot(shotPath);
      std::printf("screenshot %s: %s\n", shotPath, shotTaken ? "ok" : "FAILED");
      running = false;
    }
    pix.end();
  };
#ifdef __EMSCRIPTEN__
  // simulate_infinite_loop keeps main's stack frame (and everything the lambda references) alive
  static std::function<void()>* loopFn = new std::function<void()>(frame);
  emscripten_set_main_loop([] { (*loopFn)(); }, 0, 1);
#else
  while (running) frame();
#endif

  if (!noSave && game.mode != Mode::Title) writeSave(game);
  view.shutdown();
  audio.shutdown();
  pix.shutdown();
  SDL_Quit();
  return 0;
}
