// rpg/app/save_store.h - the player's save file and the small files beside it (SOLID 3.1b). The web build keeps them
// in the in-memory file system mounted at /save (IndexedDB, IDBFS: the web shell mounts it), and writeSave syncs it.
// Tests never write the save: main clears g_savePath for scripts and harness runs, and noSave keeps writeSave unused.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

class Game;

extern std::string g_savePath;   // the save file; empty: no save is read or written
extern int g_savesWritten;       // saves written this session (the Witness snapshot reports it: a harness run never writes one)

// a file in the player's own folder (desktop: SDL's pref path, created on first use; web: /save/<file>); "" when the OS
// gives no such folder
std::string userFilePath(const char* file);
bool readSave(std::vector<uint8_t>& out);
void writeSave(const Game& g);
