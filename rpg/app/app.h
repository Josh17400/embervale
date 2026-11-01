// rpg/app/app.h - the running game's shell (SOLID 3.1): one App holds the platform objects (Pix, Audio, Game, View)
// and the state the frame loop, the test-script driver, the WITNESS_RECORD recorder and the Witness handlers share.
// main() fills an AppSetup from the command line, makes the platform objects in its fixed order, then hands them to
// the App. Members are defined in rpg/main.cpp (the frame loop) and in the rpg/app/*.cpp files named by each block.
#pragma once
#include <SDL3/SDL.h>
#include <cstdint>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "engine/pix.h"
#include "rpg/app/script_runner.h"
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#ifdef EMB_WITNESS
#include <map>
#include "engine/witness.h"
#include "rpg/app/recorder.h"
#include "rpg/view/witness_state.h"
#endif


// the command line and what it decides, before any platform object exists (main fills it, the App keeps it)
struct AppSetup {
  bool vsync = true, perf = false, play = false, enter = false, god = false;
  const char* shotPath = nullptr;
  const char* gotoWhat = nullptr;
  const char* scriptPath = nullptr;
  float shotAfter = 3.0f, hourSet = -1;
  int menuTab = -1;
  const char* fight = nullptr;
  int talkStep = -1;   // PT test-only: 0 = innkeeper greeting, 1 = job offer, 2 = shop
  uint64_t seed = 0;
  bool windowSet = false;
  int winW = 1440, winH = 810;   // --window WxH: the initial window size in pixels (screen-fit tests: 2556x1179, 852x393...)
  bool atSet = false;
  int32_t atX = 0, atY = 0;      // --at GX,GY: start on that global tile (M1)
  std::vector<ScriptCmd> script;
  bool scripted = false;
  bool witness = false;          // a Witness harness drives this run (see "Witness" in main.cpp)
  bool watch = false;            // WITNESS_WATCH=1: a person watches the harness play
  const char* recordEnv = nullptr;   // WITNESS_RECORD=<file.jsonl>
  bool recordSync = false;       // a WITNESS_RECORD session (normal play that must replay step for step)
  bool noSave = false;           // test runs never touch the player's save
  const char* saveOverride = nullptr;   // EMB_SAVE_PATH (test hook: the title reads its save from another file)
  uint64_t startSeed = 0;
  bool touchUIStart = false;     // (a Witness reset puts it back: a test's finger turns the controls on)

  int parse(int argc, char** argv);           // -1: go on; else the exit code (--script-help 0, a script that fails 2)
  void witnessFlags(int argc, char** argv);   // witness, watch, recordEnv, recordSync
};

struct App : AppSetup {
  App(AppSetup s, Pix& p, Audio& a, Game& g, View& v);
  Pix& pix;
  Audio& audio;
  Game& game;
  View& view;
  bool hasSave = false;
#ifdef EMB_WITNESS
  int recordIds = 1;   // the actor id counter when the world in play began (a WITNESS_RECORD header carries it)
#endif
  int lastChestX = -1, lastChestY = -1;   // (M8b) the chest the script's chestgo stood at (chestaway steps clear of it)

  // ---- the frame loop
  bool running = true;
  float saveT = 0;
  Uint64 freq = 0, prev = 0, t0 = 0;
  double acc = 0, fpsT = 0;
  int fpsN = 0;
  bool shotTaken = false;

  // ---- script driver state
  size_t scriptNext = 0;
  float scriptT = 0, scriptEndT = -1;
  int scriptFails = 0;
  std::string lastFail;        // the last failure's message (a Witness call reports it)
#ifdef EMB_WITNESS
  bool witnessInCall = false;  // failing inside a call: the call's error says it; later failures are logged
#endif
  std::vector<std::string> heard;   // every notice (Game::say) shown since the script began, for "expect heard"
  std::string pendingShot;
  struct Held { SDL_Scancode sc; float until; };
  std::vector<Held> held;
  struct Lift { SDL_FingerID id; float x, y, at; };
  std::vector<Lift> lifts;
  std::vector<Lift> drags;   // (M9) a swipe's finger motion due at `at` (to x, y)
  SDL_FingerID nextFinger = 101;
  struct Walk {
    bool on = false;
    std::string what;
    int kind = 0;            // 0 tile, 1 building door, 2 actor, 3 exit, 4 stairs (M0b), 5 waiting for a giver (M2)
    int floor0 = 0;          // stairs: the floor the walk started on
    int tx = 0, ty = 0, actorId = -1, bldg = -1;
    int32_t wox = 0, woy = 0;   // M1: the endless window's origin the target tiles are relative to (shifts move them)
    float t = 0, timeout = 40, replanT = 0, finalT = 0, closeT = 0;
    bool talk = false;       // talkto: press E on arrival
    std::vector<int> path;
    size_t step = 0;
    int line = 0;
  } walk;
  struct ExtRun { const ScriptExt* e = nullptr; ScriptCmd cmd; float waited = 0; } extRun;
  struct PerfAcc {
    double workSum = 0, workWorst = 0;        // this second
    double drawSum = 0, drawWorst = 0;        // (P2a) View::draw's share of it (the render baseline, L9)
    double worstAll = 0, worstShift = 0;      // the whole run
    double worstAfterShift = 0;               // the second after a walking shift (the view re-bakes what moved)
    int afterShift = 0;
    int frames = 0, shiftFrames = 0;
    long long chunks0 = 0;                    // chunks generated (main source + streamer) at the last print
  } pacc;
#ifdef EMB_WITNESS
  WitnessExtra wStart;   // where the start put the player (state.player.start / moved)
  RecHeld rec;
  // ---- Witness (the harness's handlers and what they hold between commands)
  std::vector<SDL_Scancode> wHeld;   // keys the harness holds down
  std::vector<Lift> wFingers;        // fingers the harness holds down (witness finger n is SDL finger n + 1)
  static constexpr Sint32 kPauseCode = 0x57504155;   // SDL_EVENT_USER code: a recording's "pause" action (the lost focus)
  std::vector<float> wMix;
  std::vector<uint8_t> wShot;
  double mouseX = 0, mouseY = 0;
  std::vector<int> wButtons;   // mouse buttons the harness holds down (0 left, 1 middle, 2 right)
  std::map<std::string, std::string> wMatch;
  uint64_t wAudioHash = 1469598103934665603ull;   // (audiomatch) FNV-1a of every sample mixed since the reset
  long long wAudioN = 0;
  witness::Handlers h;
#endif

  // test helpers shared by the command-line flags and the script commands
  void startNew(uint64_t s);
  void doGoto(const std::string& w, bool enterIt);
  void doTalk(int step);
  void giveGear(int band, const std::string& weaponName);
  void doFight(const std::string& name, int n);
  void probeSave();
  void startPlay();
  void startClock();
  // injected input
  void setMove(bool up, bool left, bool down, bool right);
  void pushKey(SDL_Scancode sc, bool down);
  void toWindow(float lx, float ly, float& wx, float& wy);
  void pushFinger(SDL_EventType type, SDL_FingerID id, float lx, float ly, float pressure = 1.0f);
  void pushClick(float lx, float ly, bool down);
  void pushMouse(SDL_EventType type, float lx, float ly, int button);
  // the script driver
  void fail(int line, const std::string& msg);
  void focusLost(bool force);
  void plTile(int& x, int& y);
  int findActorIdx(int id);
  void startWalk(const ScriptCmd& c);
  void endWalk(bool ok, const char* why);
  int findGiver();
  void stepWalk(float dt);
  void callExt(float dt);
  bool startExt(const std::string& name, const ScriptCmd& c);
  static bool modeByName(const std::string& s, Mode& m);
  void runCmd(const ScriptCmd& c);
  void scriptStep(float dt);
  // --perf / script summary
  long long chunksMade();
  void printSummary();
  // the frame loop
  void frame();
  int shutdown();
#ifdef EMB_WITNESS
  // WITNESS_RECORD and the replay's start
  void recordBegin(const std::string& params);
  static std::string jsonStr(const std::string& v);
  void applyStart(uint64_t s, const StartSpec& sp);
  std::string specJson(const StartSpec& sp);
  void recLogical(float wx, float wy, float& lx, float& ly);
  void recFinger(const SDL_TouchFingerEvent& t, float& lx, float& ly);
  bool recordEvent(SDL_Event& e);
  void recLetGo(bool pause);
  void simStep(float dt);
  // the Witness handlers
  void drawFrame();
  void drainEvents();
  void wReset(long long s, const char* scene, witness::Value params);
  void wInput(const witness_event& e);
  void wStep(double dt);
  void wSnapshot(witness::Json& j);
  bool wScreenshot(witness_image& img);
  void wCall(const char* name, witness::Value args, witness::Json& result);
  void runWitness();
#endif
};
