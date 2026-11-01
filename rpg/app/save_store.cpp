// rpg/app/save_store.cpp - see save_store.h
#include "rpg/app/save_store.h"
#include <SDL3/SDL.h>
#include <cstdio>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include "rpg/app/app.h"
#include "rpg/sim/game.h"

std::string g_savePath;
int g_savesWritten = 0;
static bool webSavesOff();

std::string userFilePath(const char* file) {
#ifdef __EMSCRIPTEN__
  return std::string("/save/") + file;
#else
  std::string path;
  if (char* pref = SDL_GetPrefPath("Josh17400", "Embervale")) { path = std::string(pref) + file; SDL_free(pref); }
  return path;
#endif
}

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
  if (g_savePath.empty() || g.mode == Mode::Title || g.mode == Mode::Creator) return;   // the creator saves when done
  // (M2 fixer round 2) never mid-journey: the fare (or a death's lost gold and full health) is taken when the journey
  // begins but the move happens at the arrival, and the journey is not saved, so a save on the road kept the cost and
  // left the hero where they started. The last save (from before the journey) stands; the next one follows the arrival.
  if (g.travelling()) return;
  std::vector<uint8_t> buf;
  g.serialize(buf);
  std::string tmp = g_savePath + ".tmp";
  if (FILE* f = std::fopen(tmp.c_str(), "wb")) {
    std::fwrite(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    std::remove(g_savePath.c_str());
    std::rename(tmp.c_str(), g_savePath.c_str());
    g_savesWritten++;
  }
#ifdef __EMSCRIPTEN__
  // persist the in-memory file system to IndexedDB (mounted at /save by the web shell). (P3 B3) A failed sync marks the
  // page's Module.embSaves 'failed'; the next save says so once (the sync answers later than this call). With saves off
  // the title already said so, and nothing is synced
  if (webSavesOff()) return;
  EM_ASM(FS.syncfs(false, function(err) { if (err) { Module.embSaves = 'failed'; console.log('save sync failed', err); } }););
  if (EM_ASM_INT({ if (Module.embSaves !== 'failed' || Module.embSaveFailSaid) return 0; Module.embSaveFailSaid = 1; return 1; })) {
    Game& said = const_cast<Game&>(g);   // (always the frame loop's game, never a const object) the notice line, as Game::say
    said.notice = "THE GAME COULD NOT BE SAVED";
    said.noticeT = 3.5f;
  }
#endif
}

// (P3 B3) the web shell's save store is off this visit (Module.embSaves, web/index.html); desktop builds: never
static bool webSavesOff() {
#ifdef __EMSCRIPTEN__
  return EM_ASM_INT({ return Module.embSaves === 'off' ? 1 : 0; }) != 0;
#else
  return false;
#endif
}

// the title's save: whether Continue is offered, and the notice when the file is old or damaged
void App::probeSave() {
  hasSave = false;
  if (!noSave || (saveOverride && *saveOverride)) {
    std::vector<uint8_t> buf;
    if (readSave(buf) && !buf.empty()) {
      Game probe(1);
      hasSave = probe.deserialize(buf);
      int v = Game::saveVersion(buf);
      // owner, 2026-10-04: old saves are not carried over; say so instead of silently starting fresh (a damaged or
      // newer file gets a notice too: it is never loaded, and the next save replaces it)
      if (!hasSave && ((v > 0 && v < Game::currentSaveVersion()) || Game::saveFromOlderGenerator(buf))) view.titleNote = "THIS SAVE IS FROM AN OLDER VERSION - START A NEW ADVENTURE";
      else if (!hasSave) view.titleNote = "THIS SAVE COULD NOT BE LOADED - START A NEW ADVENTURE";
    }
  }
  if (noSave) hasSave = false;   // (the override above only shows the title's notice; a test run never continues it)
  // (P3 B3) a browser with no save store (private window, IndexedDB blocked): say so on the title, before anyone plays
  if (!noSave && webSavesOff()) view.titleNote = "SAVES ARE OFF IN THIS BROWSER - PROGRESS WILL NOT BE KEPT";
}
