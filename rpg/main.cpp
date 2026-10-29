// EMBERVALE - SDL3 platform layer: window, events, fixed-timestep loop, autosave.
//   embervale.exe                                   play (autosaves; title offers Continue)
//   embervale.exe --play --seed 7                   skip the title, fresh world (never touches your save)
//   embervale.exe --play --goto city|town|cave|ruin|camp|lair [--enter] [--hour 22] [--menu 0-4]
//                 --shot out.png --after 2           screenshot after N seconds, then quit
//   embervale.exe --script file.txt [--seed 7] [--play]   drive the game from a timed script (test only, never saves)
//   embervale.exe --play --at 4100,-2300            (M1) start on that GLOBAL tile of the endless world
//   --perf                                          once a second: fps, frame work ms (avg / worst), NPCs awake and
//                                                   asleep, hostiles, active sites, chunks/s, worst shift ms, regions,
//                                                   settlements built; at exit a summary (worst frame at a window shift)
//   embervale.exe --witness                         (desktop, EMBERVALE_WITNESS) driven by the Witness test harness over a
//                                                   local socket (WITNESS_ADDR, set by `witness test|doctor|shot|...` from
//                                                   witness.json): headless, deterministic, never saves; see "Witness" below.
//                                                   WITNESS_WATCH=1 (`witness play start --watch`) shows the window and
//                                                   presents every step; real input is dropped; the sim is unchanged
//   WITNESS_RECORD=file.jsonl embervale.exe         normal play records a replay (one line per sim step, F8 marks a
//                                                   moment) that `witness replay` re-simulates exactly (the session runs
//                                                   as witness mode does: no stream worker, inline bakes, the 480 x 270
//                                                   canvas); at exit file.jsonl.state.json holds the end state to check.
//                                                   Keys, mouse, wheel and fingers are recorded (a system-ended finger as
//                                                   a cancel, with its pressure); a lost focus lets go of all that is held
//                                                   (fingers as cancels) and records its pause (tools/scripts/
//                                                   record/witness_record_touch.txt checks a touch session end to end)
//
// Script format: one command per line, "<time> <command> [args]". Times are seconds of script time; "+0.5" means
// 0.5 s after the previous line. '#' starts a comment. Script time stops while a walkto runs, so later lines keep
// their spacing however long the walk takes. Input is injected through the same paths real input uses: key
// presses become SDL key events (View::event), held keys are read by View::input, taps become SDL touch events.
//   seed 7                    world seed (a header line, no time; --seed on the command line wins). Every run is on the
//                             endless mainland (M2 retired the classic island: a "worldgen N" header is refused)
//   0.5 key E                 press and release a key (SDL key names: E, Return, Escape, Tab, Space, Up, Left Shift...)
//   2.0 hold W 1.5            hold a key down for 1.5 s
//   phone                     header line: run at the iPhone review size (window 2556x1179 unless --window is given,
//                             EMB_SCREEN=fill and EMB_SAFE=177,0,177,63 device px, 59,0,59,21 pt at 3x, unless already set): a 639x294 logical canvas
//   3 tap 424 222             touch tap at LOGICAL canvas coordinates. The canvas is the live logical size, which
//                             depends on the window and screen fit: 480x270 at the 1440x810 desktop default, 639x294
//                             with "phone" (the hamburger is at 581 12 there: the 44-px right safe inset moves it in
//                             from the edge; tools/scripts/m6_menu_tap_phone.txt opens the menu by that tap), so tap
//                             scripts are written for one size
//   3 click 240 135           left mouse click at logical coordinates (same space as tap)
//   3 press 424 222 1.2       (M9) a finger held down at logical coordinates for 1.2 s (a hold: the quick-cast upcast)
//   3 press quick 2 1.2       (M9) ... on quick-cast button 2 (1..4; 5 the sneak toggle: rpg/view/skills_ui.h quickBtnPos)
//   3 swipe 100 200 160 180 0.4   (M9) a finger down at the first point, dragged to the second, lifted after 0.4 s
//   3 swipe quick 2 -40 -10 0.4   (M9) ... from quick-cast button 2, dragged by (dx, dy): a swipe off it (dual-cast)
//   3 blur                    the window loses the focus (an alt-tab): play pauses even in a script, a WITNESS_RECORD
//                             recording lets go of every key, button and finger it holds (Escape resumes)
//   4 shot out.png            screenshot of this frame
//   5 walkto inn              autopilot over the tile grid, steering with held WASD: inn|shop|smithy|temple|keep|tower|
//                             house (walks in the door), innkeeper|merchant|smith|priest|jarl|guard|villager|mage (until
//                             they can be talked to), exit (walks out of a building or dungeon), or "x y" tiles.
//                             a person by NAME (walkto VIGRIMA: until they can be talked to), giver (M2: the nearest
//                             person with a job to offer or a reward waiting). Inside a building:
//                             upstairs|downstairs (walks onto the stairs; done when the floor changed).
//                             An optional trailing number is the timeout in seconds (default 40).
//   6 expect mode shop        check state: mode title|play|dialogue|menu|shop|levelup|dead|paused|creator, inside 0|1
//   6 newgame | goto ruin [enter] | talk 0|1|2 | fight wolf [n] | god [0|1] | hour 22 | menu 2 | log text
//                             goto city|town|village|cave|ruin|camp|shrine|lair|capital (capital: the nearest kingdom
//                             capital, M1; market: the nearest market town, M1 economy)
//   6 talkto VIGRIMA [30]     walkto a person and talk to them the moment they are in reach (E in that same frame)
//   6 at 4100 -2300           (M1) teleport to a GLOBAL tile (the window recentres there)
//   6 walk east 6             (M1) hold the direction (east|west|north|south) for 6 s (script time keeps running)
//   6 nearshift east          (M1) step to 2 tiles short of the endless window's shift line that way (shift reviews)
//   6 expect kingdom          (M1) the settlement the player stands in belongs to a kingdom (expect kingdom capital:
//                             ... and is its capital); expect npc king|royal: someone with that role / title is in play
//   6 kit                     give and equip the test kit (sword, bow, arrows, potions); fight does this when unarmed
//   6 gear 3 [weapon]         give and equip a full set of armour band 3 (1 leather, 2 iron, 3 steel, 4 gilded, 5 jade,
//                             6 obsidian, 7 emberforged): body, helmet, gloves, boots, cloak, shield, amulet, ring, plus a
//                             weapon of that tier (sword|axe|mace|dagger|greatsword, default sword) and a bow for the back
//   6 strip                   take every piece of equipment off (shirt and trousers only)
//   6 enter inn [floor] [n]   (M0b) step straight into the n-th nearest (default 0) inn|shop|smithy|temple|keep|tower|
//                             house|stonehouse|farmhouse|hut|palace|barracks|windmill|watermill|granary|bakery|butcher|
//                             tannery|fishmonger|smelter|sawmill|weaver, on that floor (default 0); fails if it has no such floor
//   6 floor 1                 (M0b) inside a building: go to that floor (arriving by its stairs)
//   6 expect floor 1          the floor the player is on (0 ground)
//   6 expect name ASTRID      the player's name (the character creator); also: expect background 3, expect slot armor 1|0
//   6 expect quest active BOUNTY: X   a quest whose title contains the words is active|complete|done (or none exists)
//   6 expect tracked CULL THE       the tracked quest's title contains the words
//   6 expect heard NOT YOUR TARGET  some notice since the script began contained the words
//   6 expect option COLLECT BOUNTY  the open dialogue offers an option containing the words (expect text: its text)
//   6 choose RENT A ROOM      (M0b) choose the open dialogue's option whose label contains the words
//   6 chestgo [hoard|explore|chest]   (M8b) stand beside the nearest closed chest (of that loot source), facing it:
//                             `key e` then opens it through the real Game::openChest; chestaway then steps 4 tiles
//                             clear of it (outside the pickups' pull) so a shot shows the find on the floor
// New Game from the title opens the character creator (Mode::Creator); --play and the newgame command skip it.
// M2: lanes register more commands (and expect checks) from rpg/view/*.cpp through rpg/view/script_api.h;
// `embervale --script-help` lists them.
//   9 quit                    (the script also quits 2 s after its last line)
// The exit code is 3 if any expect or walkto failed.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <functional>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "engine/pix.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/loot_ops.h"
#include "rpg/view/screen.h"
#include "rpg/view/script_api.h"
#include "rpg/view/skills_ui.h"
#include "rpg/view/view.h"
#include "rpg/sim/stream.h"
#include "rpg/world/source.h"
#ifdef EMB_WITNESS
#include <map>
#include <new>
#include "engine/witness.h"
#include "rpg/view/witness_state.h"
#include "rpg/sim/anim.h"   // (P2a) AnimOps::refreshHero (the reset's female)
#endif

namespace {
std::string g_savePath;
void newWorld(Game& g, uint64_t s) {
  g.newEndlessGame(s);
  if (!g.streamThreads) g.world.streamer.reset();   // (Witness) no prefetch worker: every chunk is made on demand, in order
}
int g_savesWritten = 0;   // saves written this session (the Witness snapshot reports it: a harness run never writes one)

// every event the game pushes itself (a script's or the harness's keys, clicks and fingers) goes through pushEvent.
// (Witness watch mode, WITNESS_WATCH=1) the window is shown, but only that injected input may reach gameplay: the
// event filter drops the real keyboard, mouse and touch, so a viewer's stray click cannot change a run whose replay
// must stay exact (and the window never takes the keyboard focus: SDL's key state stays empty)
bool g_dropRealInput = false, g_injecting = false;
void pushEvent(SDL_Event& ev) {
  g_injecting = true;
  SDL_PushEvent(&ev);   // (SDL calls the filter right here, synchronously)
  g_injecting = false;
}
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
  // persist the in-memory file system to IndexedDB (mounted at /save by the web shell)
  EM_ASM(FS.syncfs(false, function(err) { if (err) console.log('save sync failed', err); }););
#endif
}

#ifdef __EMSCRIPTEN__
// (P2a) a whole-number query parameter of the page's URL (?rig=2: the owner's preview of the v2 people); 0 when absent
EM_JS(int, emb_url_int, (const char* name), {
  try {
    var v = new URLSearchParams(window.location.search).get(UTF8ToString(name));
    var n = parseInt(v, 10);
    return isNaN(n) ? 0 : n;
  } catch (e) { return 0; }
});
#endif

// ---------------------------------------------------------------- test scripts
struct ScriptCmd {
  float t = 0;
  std::vector<std::string> a;   // a[0] is the command
  int line = 0;
  std::string text;
};
bool g_scriptPhone = false;   // a "phone" header line: run at the iPhone review size (see the header comment)
void setEnvIfUnset(const char* k, const char* v) {
  if (std::getenv(k)) return;
#ifdef _WIN32
  _putenv_s(k, v);
#else
  setenv(k, v, 0);
#endif
}

bool loadScript(const char* path, std::vector<ScriptCmd>& out, uint64_t& seed) {
  FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  std::string all;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) all.append(buf, n);
  std::fclose(f);
  float last = 0;
  int lineNo = 0;
  size_t pos = 0;
  while (pos <= all.size()) {
    size_t e = all.find('\n', pos);
    if (e == std::string::npos) e = all.size();
    std::string ln = all.substr(pos, e - pos);
    pos = e + 1;
    lineNo++;
    size_t hash = ln.find('#');
    if (hash != std::string::npos) ln.resize(hash);
    std::vector<std::string> tok;
    size_t i = 0;
    while (i < ln.size()) {
      while (i < ln.size() && (ln[i] == ' ' || ln[i] == '\t' || ln[i] == '\r')) i++;
      size_t j = i;
      while (j < ln.size() && ln[j] != ' ' && ln[j] != '\t' && ln[j] != '\r') j++;
      if (j > i) tok.push_back(ln.substr(i, j - i));
      i = j;
    }
    if (tok.empty()) continue;
    if (tok[0] == "seed" && tok.size() >= 2) { if (!seed) seed = (uint64_t)std::atoll(tok[1].c_str()); continue; }
    if (tok[0] == "phone") { g_scriptPhone = true; continue; }
    if (tok[0] == "worldgen") { std::printf("script %s:%d: 'worldgen' is retired (M2: every world is endless)\n", path, lineNo); return false; }
    char* endp = nullptr;
    bool rel = tok[0][0] == '+';
    float t = std::strtof(tok[0].c_str() + (rel ? 1 : 0), &endp);
    if (!endp || *endp || tok.size() < 2) { std::printf("script %s:%d: expected '<time> <command>': %s\n", path, lineNo, ln.c_str()); continue; }
    ScriptCmd c;
    c.t = rel ? last + t : t;
    last = c.t;
    c.a.assign(tok.begin() + 1, tok.end());
    for (auto& ch : c.a[0]) ch = (char)std::tolower((unsigned char)ch);
    c.line = lineNo;
    for (size_t k = 1; k < tok.size(); k++) { if (k > 1) c.text += ' '; c.text += tok[k]; }
    out.push_back(c);
  }
  std::stable_sort(out.begin(), out.end(), [](const ScriptCmd& x, const ScriptCmd& y) { return x.t < y.t; });
  return true;
}

SDL_Scancode keyFromName(std::string s) {
  std::string u = s;
  for (auto& ch : u) ch = (char)std::toupper((unsigned char)ch);
  if (u == "ENTER") s = "Return";
  else if (u == "ESC") s = "Escape";
  else if (u == "SHIFT") s = "Left Shift";
  else if (u == "LSHIFT") s = "Left Shift";
  else if (u == "RSHIFT") s = "Right Shift";
  else if (u == "CTRL") s = "Left Ctrl";
  else if (u == "ALT") s = "Left Alt";
  else if (u == "META" || u == "SUPER") s = "Left GUI";   // (Witness 0.7.3) the OS key is "meta"; "super" kept for old scripts
  else if (u == "MINUS") s = "-";        // (P2a) the ZOOM keys: - and =
  else if (u == "EQUALS" || u == "PLUS") s = "=";
  return SDL_GetScancodeFromName(s.c_str());
}

// breadth-first path over the current map's walkable tiles, toward the reachable tile nearest (gx, gy)
bool findPath(const Map& m, int sx, int sy, int gx, int gy, std::vector<int>& path) {
  path.clear();
  if (!m.in(sx, sy)) return false;
  const int R = 90;   // search box radius in tiles
  int x0 = std::max(0, std::min(sx, gx) - R), y0 = std::max(0, std::min(sy, gy) - R);
  int x1 = std::min(m.w - 1, std::max(sx, gx) + R), y1 = std::min(m.h - 1, std::max(sy, gy) + R);
  int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
  std::vector<int> from((size_t)bw * bh, -1);
  std::vector<int> q;
  auto id = [&](int x, int y) { return (y - y0) * bw + (x - x0); };
  q.push_back(id(sx, sy));
  from[(size_t)id(sx, sy)] = id(sx, sy);
  int best = id(sx, sy), bestD = std::abs(sx - gx) * std::abs(sx - gx) + std::abs(sy - gy) * std::abs(sy - gy);
  for (size_t h = 0; h < q.size(); h++) {
    int x = q[h] % bw + x0, y = q[h] / bw + y0;
    int d = (x - gx) * (x - gx) + (y - gy) * (y - gy);
    if (d < bestD) { bestD = d; best = q[h]; }
    if (d == 0) break;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      int nx = x + dx[k], ny = y + dy[k];
      if (nx < x0 || ny < y0 || nx > x1 || ny > y1 || from[(size_t)id(nx, ny)] >= 0 || m.blocked(nx, ny)) continue;
      // never path across a flight of stairs that is not the goal (stepping on it changes the floor)
      const int pr = m.propAt(nx, ny);
      if ((pr == (int)art::Prop::StairsDown + 1 || pr == (int)art::Prop::StairsUp + 1) && (nx != gx || ny != gy)) continue;
      // (owner) nor across a building's way in that is not the goal (an open front's bays: stepping in enters)
      if (m.kind == MapKind::Overworld && m.bldgAt[(size_t)ny * m.w + nx] >= 0 && (nx != gx || ny != gy)) continue;
      from[(size_t)id(nx, ny)] = q[h];
      q.push_back(id(nx, ny));
    }
  }
  for (int c = best; ; c = from[(size_t)c]) {
    path.push_back((c / bw + y0) * m.w + (c % bw + x0));
    if (from[(size_t)c] == c) break;
  }
  std::reverse(path.begin(), path.end());
  return true;
}
}  // namespace

int main(int argc, char** argv) {
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
    else if (!std::strcmp(argv[i], "--script") && i + 1 < argc) scriptPath = argv[++i];
    else if (!std::strcmp(argv[i], "--script-help")) {
      std::printf("script commands registered by the lanes (rpg/view/script_api.h):\n");
      for (const ScriptExt& e : scriptExts()) std::printf("  %-22s %s\n", e.name, e.help);
      return 0;
    }
    else if (!std::strcmp(argv[i], "--at") && i + 1 < argc) {
      int x = 0, y = 0;
      if (std::sscanf(argv[++i], "%d,%d", &x, &y) == 2) { atSet = true; atX = x; atY = y; }
    }
    else if (!std::strcmp(argv[i], "--window") && i + 1 < argc) {
      int w = 0, h = 0;
      if (std::sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w >= 160 && h >= 90) { winW = w; winH = h; windowSet = true; }
    }
  }
  std::vector<ScriptCmd> script;
  if (scriptPath) {
    if (!loadScript(scriptPath, script, seed)) { std::printf("script %s: cannot open\n", scriptPath); return 2; }
    std::printf("script %s: %zu commands, seed %llu\n", scriptPath, script.size(), (unsigned long long)seed);
    if (g_scriptPhone) {   // the landscape iPhone 15 Pro: 2556x1179, fill, its safe-area insets (639x294 logical)
      if (!windowSet) { winW = 2556; winH = 1179; }
      setEnvIfUnset("EMB_SCREEN", "fill");
      setEnvIfUnset("EMB_SAFE", "177,0,177,63");   // (M6 fixer r2) device px at 3x: 59,0,59,21 points
    }
  }
  const bool scripted = scriptPath != nullptr;
  // (P2a test hook) EMB_LEGACY_OUTLINE=1: the character sheets with the pre-P2a selective outline, so a run's shots can be
  // compared byte for byte with a pre-P2a build (the ink outline is phase A's only intended pixel change)
  if (std::getenv("EMB_LEGACY_OUTLINE")) art::outlineLegacyForTests() = true;
  // (P2a test hook) EMB_RIG=1|2: the human rig (the preview of the v2 people), set before the first sheet is baked
  if (const char* e = std::getenv("EMB_RIG")) art::setRigVersion(std::atoi(e));
#ifdef __EMSCRIPTEN__
  if (const int r = emb_url_int("rig"); r > 0) art::setRigVersion(r);   // (P2a) the web's ?rig=2 (the preview link)
#endif
#ifdef EMB_WITNESS
  const bool witness = witness_active(argc, argv);   // a Witness harness drives this run (see "Witness" below)
  // WITNESS_WATCH=1 (`witness play start --watch`, Witness PROTOCOL.md section 9): a person watches the harness play.
  // The window is shown and every step is presented; real input is dropped; nothing else changes (no pacing: the
  // harness paces, and the sim is the headless one)
  const char* watchEnv = std::getenv("WITNESS_WATCH");
  const bool watch = witness && watchEnv && std::strcmp(watchEnv, "1") == 0;
#else
  const bool witness = false;
#endif
#ifdef EMB_WITNESS
  // WITNESS_RECORD=<file.jsonl> (normal play, not a harness run): the session must replay step for step, so it runs
  // as witness mode does: no stream worker (Game::streamThreads off, syncStreams on), the view's inline bakes and the
  // sim-ended arrival (View::witness), the 480 x 270 canvas, and one view update per sim step (see the frame loop)
  const char* recordEnv = std::getenv("WITNESS_RECORD");
  const bool recordSync = !witness && recordEnv && *recordEnv;
#else
  const bool recordSync = false;
#endif
  const bool noSave = play || shotPath || seed != 0 || scripted || witness;   // test runs never touch the player's save

  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
  SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) { SDL_Log("SDL_Init failed: %s", SDL_GetError()); return 1; }
#ifdef __EMSCRIPTEN__
  g_savePath = "/save/save.bin";
#else
  if (char* pref = SDL_GetPrefPath("Josh17400", "Embervale")) { g_savePath = std::string(pref) + "save.bin"; SDL_free(pref); }
#endif
  if (scripted || witness) g_savePath.clear();   // belt and braces: a script or harness never reads or writes the real save
  // test hook: read the title's save from another file (e.g. an old-format save, to see the title's notice). Test runs
  // (--shot, --play, --seed) still never write it.
  const char* saveOverride = std::getenv("EMB_SAVE_PATH");
  if (saveOverride && *saveOverride && !scripted && !witness) g_savePath = saveOverride;

  Pix pix;
  // (Witness) a hidden 960x540 window (the 480x270 canvas at 2x), no vsync, no audio device (the mix is rendered per step).
  // The web build's window is the page's canvas (SDL_WINDOW_FILL_DOCUMENT): shown, so the harness's browser lays it out
  // as a phone would (witness doctor --device), and the canvas stays pinned at 480 x 270 (screen::init below)
#ifdef __EMSCRIPTEN__
  constexpr bool kWitnessHidden = false;
#else
  constexpr bool kWitnessHidden = true;
#endif
  if (!(witness ? pix.init("EMBERVALE", 960, 540, false, kWitnessHidden) : pix.init("EMBERVALE", winW, winH, vsync))) return 1;
  if (witness) pix.markTextures();   // (a reset destroys every texture made after this: Pix::destroyUnmarked)
#ifdef EMB_WITNESS
  if (watch) {
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
#endif
  static Audio audio;   // ~175 KB: keep it off the stack
  if (!witness) audio.init();
#ifdef __EMSCRIPTEN__
  // (Witness 0.7.1, cpp GUIDE section 13) web witness mode opens the audio device too, fed SILENCE (no callback, never
  // SDL_PutAudioStreamData): the page gets its AudioContext, so doctor --no-auto-unlock and web.audio.state can check
  // the iPhone unlock path. What listen hears still comes from witness_audio_write() in step (deterministic).
  if (witness && (SDL_WasInit(SDL_INIT_AUDIO) || SDL_InitSubSystem(SDL_INIT_AUDIO))) {
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 1, 44100};
    if (SDL_AudioStream* dev = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr))
      SDL_ResumeAudioStreamDevice(dev);   // kept open for the page's life
  }
#endif

  uint64_t startSeed = seed ? seed : witness ? 1 : (uint64_t)SDL_GetTicks() * 2654435761ull + 12345;
  Game game(startSeed);
  if (witness || recordSync) { game.streamThreads = false; game.syncStreams = true; }   // runs never depend on worker timing
  newWorld(game, startSeed);   // title screen drifts over this world
  game.mode = Mode::Title;
  View view;
  view.init(pix, audio);
  view.witness = witness || recordSync;
#if defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_ANDROID)
  view.touchUI = true;
#endif
#ifdef EMB_WITNESS
  const bool touchUIStart = view.touchUI;   // (a Witness reset puts it back: a test's finger turns the controls on)
#endif
  {   // M1: screen fit and the player's settings (a small file of its own next to the save; scripts use defaults)
    std::string settingsPath;
#ifdef __EMSCRIPTEN__
    settingsPath = "/save/settings.bin";
#else
    if (char* pref = SDL_GetPrefPath("Josh17400", "Embervale")) { settingsPath = std::string(pref) + "settings.bin"; SDL_free(pref); }
#endif
    if (scripted || witness) settingsPath.clear();
    // (Witness, WITNESS_RECORD) the canvas pinned at 480 x 270: EMB_SCREEN / EMB_BORDER / EMB_HUD / EMB_SAFE and the
    // settings file never change what a run sees (the canvas size feeds the NPC sleep box: Game::sleepHalfW)
    screen::init(pix, settingsPath, view.touchUI, witness || recordSync);
  }

  bool hasSave = false;
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

#ifdef EMB_WITNESS
  int recordIds = 1;   // the actor id counter when the world in play began (a WITNESS_RECORD header carries it)
#endif
  auto startNew = [&](uint64_t s) {
#ifdef EMB_WITNESS
    recordIds = game.idCounter();
#endif
    newWorld(game, s);
    game.mode = Mode::Play;
    view.snap(game);
    audio.play(Sfx::QuestStart);
  };
#ifdef EMB_WITNESS
  // WITNESS_RECORD=<file.jsonl> in normal play: a replay `witness replay` re-simulates, one line per sim step. It begins
  // when play does (--play, or the creator handing over); the header holds the seed the world used and the params the
  // harness's reset needs to rebuild that start (scene, the creator's background)
  auto recordBegin = [&](const std::string& params) {
    if (!witness) witness_record_begin("Embervale", 60, (long long)game.seed, params.c_str());
  };
  auto jsonStr = [](const std::string& v) {
    std::string r = "\"";
    for (char ch : v) { if (ch == '"' || ch == '\\') r += '\\'; if ((unsigned char)ch >= 0x20) r += ch; }
    return r + "\"";
  };
#endif
  // test helpers shared by the command-line flags and the script commands
  auto doGoto = [&](const std::string& w, bool enterIt) {
    SiteType want = SiteType::City;
    if (w == "town" || w == "market") want = SiteType::Town; else if (w == "village") want = SiteType::Village; else if (w == "cave") want = SiteType::Cave;
    else if (w == "ruin") want = SiteType::Ruin; else if (w == "camp") want = SiteType::BanditCamp; else if (w == "lair") want = SiteType::DragonLair;
    else if (w == "shrine") want = SiteType::Shrine;
    int si = game.world.nearestSite(game.world.sites[game.world.startSite].ex, game.world.sites[game.world.startSite].ey, want);
    if (w == "capital") {   // M1: the nearest kingdom capital to the start (the king's palace)
      si = -1;
      int bd = 1 << 30;
      const Site& h = game.world.sites[game.world.startSite];
      for (int i = 0; i < (int)game.world.sites.size(); i++) {
        const Site& s = game.world.sites[i];
        if (!s.capital) continue;
        int d = (s.ex - h.ex) * (s.ex - h.ex) + (s.ey - h.ey) * (s.ey - h.ey);
        if (d < bd) { bd = d; si = i; }
      }
    }
    if (game.world.endless) {
      // the endless world: the nearest one in the region plans around the start (settlements are far apart: the nearest
      // town may lie beyond the records the window has loaded)
      const Site& h = game.world.sites[(size_t)game.world.startSite];
      int far = game.world.findSiteNear(game.world.ox + h.ex, game.world.oy + h.ey, w == "capital" ? SiteType::City : want, 8, w == "capital");
      if (w == "capital" && far < 0) far = game.world.findSiteNear(game.world.ox + h.ex, game.world.oy + h.ey, SiteType::Town, 8, true);
      if (w == "market") {   // (M1 economy) the nearest market town: a real crossroads of trade (region.cpp: six roads,
                             // or four and the town's own roll; a village at five roads and a rarer roll)
        far = game.world.findSiteNear(game.world.ox + h.ex, game.world.oy + h.ey, SiteType::Town, 12, false, (int)ew::Archetype::Market);
        if (far < 0) far = game.world.findSiteNear(game.world.ox + h.ex, game.world.oy + h.ey, SiteType::City, 12, false, (int)ew::Archetype::Market);
      }
      if (far >= 0) si = far;
    }
    if (si < 0) { std::printf("goto %s: none found\n", w.c_str()); return; }
    game.world.sites[si].discovered = true;
    game.fastTravel(si);
    if (enterIt && (want == SiteType::Cave || want == SiteType::Ruin)) {
      const Site& s = game.world.sites[si];
      game.pl().p = Vec2(s.ex * 16 + 8.0f, s.ey * 16 + 6.0f);
      game.update(SIM_DT, Input());
    }
    if (enterIt && (want == SiteType::City || want == SiteType::Town || want == SiteType::Village)) {
      const Site& s = game.world.sites[si];
      for (int b = s.bldgFirst; b < s.bldgFirst + s.bldgCount; b++)
        if (game.world.over.bldgs[b].type == art::Building::Inn) {
          game.pl().p = Vec2(game.world.over.bldgs[b].doorX() * 16 + 8.0f, game.world.over.bldgs[b].doorY() * 16 + 10.0f);
          game.update(SIM_DT, Input());
          break;
        }
    }
    game.sleepFade = 0;
    view.snap(game);
  };
  auto doTalk = [&](int step) {   // PT test-only: talk to the innkeeper (0 greeting, 1 job offer, 2 shop)
    if (!game.inside) return;
    for (size_t k = 1; k < game.actors.size(); k++)
      if (game.actors[k].role == Role::Innkeeper) {
        game.pl().p = game.actors[k].p + Vec2(0, 20);
        Input ti; ti.interact = true;
        game.update(SIM_DT, ti);
        if (game.mode == Mode::Dialogue && step >= 1) {
          const char* want = step == 1 ? "WORK" : "WARES";
          { const int o = dlgReach(game, [&](const DlgOpt& x) { return x.label.find(want) != std::string::npos; }, true); if (o >= 0) game.dialogueChoose(o); }
        }
        break;
      }
  };
  // test helper: a full armour set of one band (1 leather .. 7 emberforged), equipped, plus a weapon and a bow.
  // (M8b, LootSrc::Debug) made by the gear rules at the band's middle item level (never the classic makers): the armour,
  // weapon and bow Common, the ring and amulet Uncommon (a jewel's worth is its affix), so every piece passes lootValid
  int lastChestX = -1, lastChestY = -1;   // (M8b) the chest the script's chestgo stood at (chestaway steps clear of it)
  auto giveGear = [&](int band, const std::string& weaponName) {
    band = std::clamp(band, 1, 7);
    const int il = (gear::bandLo(band) + gear::bandHi(band)) / 2;
    Rng r(game.seed * 31 + (uint64_t)band);
    auto wear = [&](Item it) {
      game.inv.push_back(it);
      int idx = (int)game.inv.size() - 1;
      int* e = game.equipSlot(it.kind);
      if (e && *e != idx) game.useItem(idx);
    };
    const ItemKind kinds[] = {ItemKind::Armor, ItemKind::Helmet, ItemKind::Gloves, ItemKind::Boots, ItemKind::Cloak, ItemKind::Shield};
    for (int k = 0; k < 6; k++) wear(gear::makeGearC(r, kinds[k], 0, il, Rarity::Common, nullptr, band == 1 && kinds[k] != ItemKind::Cloak ? Mat::Leather : Mat::None));
    static const char* wn[] = {"sword", "axe", "mace", "dagger", "greatsword"};
    int wt = 0;
    for (int i = 0; i < 5; i++) if (weaponName == wn[i]) wt = i;
    wear(gear::makeGearC(r, ItemKind::Weapon, wt, il, Rarity::Common, nullptr));
    wear(gear::makeGearC(r, ItemKind::Bow, 0, il, Rarity::Common, nullptr));
    wear(gear::makeGearC(r, ItemKind::Amulet, 0, il, Rarity::Uncommon, nullptr));
    wear(gear::makeGearC(r, ItemKind::Ring, 0, il, Rarity::Uncommon, nullptr));
  };
  auto doFight = [&](const std::string& name, int n) {
    if (game.eqWeapon < 0) game.debugKit();   // the real start is shirt-only (M0): fights get the test kit
    static const char* names[] = {"wolf", "boar", "bear", "slime", "spider", "bat", "skeleton", "draugr", "goblin", "troll", "wraith", "mudcrab", "icewolf", "frostspider", "sandworm", "dragon",
                                  "scorpion", "hyena", "lurker", "yeti", "wisp", "emberhound", "blightspawn",   // (M3c wildlife)
                                  "harpy", "golem",   // (M6)
                                  "seaserpent", "kraken", "merrow"};   // (M10: they swim; a fight on land strands them)
    for (int m = 0; m < (int)art::Monster::COUNT && m < (int)(sizeof(names) / sizeof(names[0])); m++)
      if (name == names[m]) game.debugSpawn((art::Monster)m, n > 0 ? n : (m == (int)art::Monster::Dragon ? 1 : 3), 60);
  };

#ifdef EMB_WITNESS
  // (Witness) how a run starts: a harness reset's params, or a recorded --play session's flags (its replay header).
  // Both go through applyStart, so a replay begins exactly where the recording did: a brand-new Game (no timer, id
  // counter or cache of an earlier world), then the steps --play takes, in its order
  struct StartSpec {
    bool god = false, enter = false, creator = false, atSet = false;
    std::string scene, fight, name;
    float hour = -1;
    int32_t atX = 0, atY = 0;
    int talk = -1, menu = -1, count = 0, background = 0, ids = 0;
  };
  WitnessExtra wStart;   // where the start put the player (state.player.start / moved)
  auto applyStart = [&](uint64_t s, const StartSpec& sp) {
    game.~Game();
    new (&game) Game(s);
    game.streamThreads = false;
    game.syncStreams = true;
    if (sp.ids > 0) game.setIdCounter(sp.ids);
    startNew(s);
    if (sp.creator) {   // a recorded new game: the creator's choices that change play
      game.app.name = sp.name;
      game.background = (Background)sp.background;
      game.finishCreator();
    }
    game.godMode = sp.god;
    if (sp.hour >= 0) game.hour = sp.hour;
    if (sp.atSet) { game.teleportGlobal(sp.atX, sp.atY); view.snap(game); }
    if (!sp.scene.empty()) doGoto(sp.scene, sp.enter);
    if (sp.hour >= 0) game.hour = sp.hour;   // (a fast travel passes the hours of the journey: the hour asked for wins)
    if (sp.talk >= 0) doTalk(sp.talk);
    view.snap(game);
    if (sp.menu >= 0) view.openMenu(game, sp.menu);
    if (!sp.fight.empty()) doFight(sp.fight, sp.count);
    wStart = WitnessExtra();
    wStart.hasStart = !game.inside;
    wStart.startGX = game.world.ox + (int)std::floor(game.pl().p.x / TILE);
    wStart.startGY = game.world.oy + (int)std::floor((game.pl().p.y - 2) / TILE);
  };
  auto specJson = [&](const StartSpec& sp) {
    std::string p = "{\"god\":" + std::string(sp.god ? "true" : "false");
    if (!sp.scene.empty()) p += ",\"scene\":" + jsonStr(sp.scene) + ",\"enter\":" + (sp.enter ? "true" : "false");
    if (sp.hour >= 0) { char b[32]; std::snprintf(b, sizeof b, "%.9g", sp.hour); p += ",\"hour\":" + std::string(b); }   // (exact as a float)
    if (sp.atSet) p += ",\"at\":[" + std::to_string(sp.atX) + "," + std::to_string(sp.atY) + "]";
    if (sp.talk >= 0) p += ",\"talk\":" + std::to_string(sp.talk);
    if (sp.menu >= 0) p += ",\"menu\":" + std::to_string(sp.menu);
    if (!sp.fight.empty()) p += ",\"fight\":" + jsonStr(sp.fight) + ",\"count\":" + std::to_string(sp.count);
    if (sp.creator) p += ",\"creator\":true,\"background\":" + std::to_string(sp.background) + ",\"name\":" + jsonStr(sp.name);
    // (what the window decides, state.screen and state.player.sprite, differs between the player's window and the
    // replay's hidden one: witness.json "replay": {"ignore": [...]} leaves it out of the end-state check)
    return p + ",\"ids\":" + std::to_string(sp.ids > 0 ? sp.ids : 1) + "}";
  };
  if (play && recordSync) {
    StartSpec sp;
    sp.god = god; sp.enter = enter; sp.hour = hourSet; sp.atSet = atSet; sp.atX = atX; sp.atY = atY;
    sp.talk = talkStep; sp.menu = menuTab;
    if (gotoWhat) sp.scene = gotoWhat;
    if (fight) sp.fight = fight;
    applyStart(startSeed, sp);
    sp.ids = recordIds;
    recordBegin(specJson(sp));
  } else
#endif
  if (play) {
    startNew(startSeed);
    game.godMode = god;
    if (hourSet >= 0) game.hour = hourSet;
    if (atSet) { game.teleportGlobal(atX, atY); view.snap(game); }
    if (gotoWhat) doGoto(gotoWhat, enter);
    if (hourSet >= 0) game.hour = hourSet;   // (a fast travel passes the hours of the journey: the hour asked for wins)
    if (talkStep >= 0) doTalk(talkStep);
    view.snap(game);
    if (menuTab >= 0) view.openMenu(game, menuTab);
    if (fight) doFight(fight, 0);
  }

  bool running = true;
  float saveT = 0;
  Uint64 freq = SDL_GetPerformanceFrequency(), prev = SDL_GetPerformanceCounter(), t0 = prev;
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
  auto setMove = [&](bool up, bool left, bool down, bool right) {
    view.scriptHold(SDL_SCANCODE_W, up); view.scriptHold(SDL_SCANCODE_A, left);
    view.scriptHold(SDL_SCANCODE_S, down); view.scriptHold(SDL_SCANCODE_D, right);
  };
  auto pushKey = [&](SDL_Scancode sc, bool down) {
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    ev.key.timestamp = SDL_GetTicksNS();
    ev.key.windowID = SDL_GetWindowID(pix.window());
    ev.key.scancode = sc;
    ev.key.key = SDL_GetKeyFromScancode(sc, SDL_KMOD_NONE, false);
    ev.key.down = down;
    pushEvent(ev);
  };
  auto toWindow = [&](float lx, float ly, float& wx, float& wy) { SDL_RenderCoordinatesToWindow(pix.renderer(), lx, ly, &wx, &wy); };
  auto pushFinger = [&](SDL_EventType type, SDL_FingerID id, float lx, float ly, float pressure = 1.0f) {
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
  };
  auto pushClick = [&](float lx, float ly, bool down) {
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
  };
  auto fail = [&](int line, const std::string& msg) {
    std::printf("script FAIL (line %d): %s\n", line, msg.c_str());
    scriptFails++;
    lastFail = msg;
#ifdef EMB_WITNESS
    if (witness && !witnessInCall) witness_log(WITNESS_ERROR, "script: %s", msg.c_str());
#endif
  };
#ifdef EMB_WITNESS
  // (WITNESS_RECORD) what the recording holds: every key, mouse button and finger it recorded as pressed and not yet as
  // released. A lost focus lets go of all of it (recLetGo below), and the late release the platform may still send for
  // one of those afterwards is dropped, so neither the view nor the replay sees a release twice.
  struct RecHeld {
    struct Finger { bool on = false; SDL_TouchFingerEvent ev{}; };   // ev: its last event (the release goes there)
    std::vector<std::string> keys, keysLetGo;    // Witness key names
    bool button[3] = {}, buttonLetGo[3] = {};    // 0 left, 1 middle, 2 right
    SDL_MouseButtonEvent buttonEv[3] = {};       // each press (a release is made from it)
    float mouseX = 0, mouseY = 0;                // the pointer's last window position
    Finger finger[10];                           // slot i is the replay's finger i (0 = the first finger down)
    std::vector<SDL_FingerID> fingersLetGo;
  } rec;
  // window px -> the logical px the view sees under Witness (View::witnessSnap: the replay re-creates them exactly)
  auto recLogical = [&](float wx, float wy, float& lx, float& ly) {
    pix.windowToLogical(wx, wy, lx, ly);
    lx = View::witnessSnap(lx);
    ly = View::witnessSnap(ly);
  };
  auto recFinger = [&](const SDL_TouchFingerEvent& t, float& lx, float& ly) {   // as View::event converts a finger
    int ww = 1, wh = 1;
    SDL_GetWindowSize(pix.window(), &ww, &wh);
    recLogical(t.x * ww, t.y * wh, lx, ly);
  };
  // one event of a recording session, in Witness names and logical pixels (F8 marks the moment). false: the view must
  // not see it either (the late release of something a lost focus already let go of). A finger's CANCELED (the system
  // ended the touch: never a completed tap) is recorded as the protocol's cancel, which the replay injects as CANCELED.
  auto recordEvent = [&](SDL_Event& e) -> bool {
    auto has = [](const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); };
    auto drop = [](std::vector<std::string>& v, const std::string& s) { v.erase(std::remove(v.begin(), v.end(), s), v.end()); };
    switch (e.type) {
      case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: {
        if (e.key.repeat) return true;
        char k[32];
        const std::string name = witness_key_normalize(SDL_GetScancodeName(e.key.scancode), k, sizeof k);
        const bool down = e.type == SDL_EVENT_KEY_DOWN;
        if (name.empty()) return true;
        if (name != "f8") {   // (F8 only marks the moment: never held)
          if (down) { drop(rec.keysLetGo, name); if (!has(rec.keys, name)) rec.keys.push_back(name); }
          else if (has(rec.keysLetGo, name)) { drop(rec.keysLetGo, name); return true; }   // the view has no key-up handling
          else drop(rec.keys, name);
        }
        witness_record_key(name.c_str(), down);
        return true;
      }
      case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (e.button.which == SDL_TOUCH_MOUSEID) return true;   // made from a finger: the touch is what is recorded
        if (e.button.button < SDL_BUTTON_LEFT || e.button.button > SDL_BUTTON_RIGHT) return true;   // (the protocol has 3)
        const int b = e.button.button == SDL_BUTTON_LEFT ? 0 : e.button.button == SDL_BUTTON_MIDDLE ? 1 : 2;
        const bool down = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        if (down) { rec.buttonLetGo[b] = false; rec.button[b] = true; rec.buttonEv[b] = e.button; }
        else if (rec.buttonLetGo[b]) { rec.buttonLetGo[b] = false; return false; }
        else rec.button[b] = false;
        rec.mouseX = e.button.x; rec.mouseY = e.button.y;
        float lx, ly;
        recLogical(e.button.x, e.button.y, lx, ly);
        witness_record_mouse(lx, ly, b, down);
        return true;
      }
      case SDL_EVENT_MOUSE_MOTION: {
        if (e.motion.which == SDL_TOUCH_MOUSEID) return true;
        rec.mouseX = e.motion.x; rec.mouseY = e.motion.y;
        float lx, ly;
        recLogical(e.motion.x, e.motion.y, lx, ly);
        witness_record_mouse(lx, ly, -1, 0);
        return true;
      }
      case SDL_EVENT_MOUSE_WHEEL: {
        if (e.wheel.which == SDL_TOUCH_MOUSEID) return true;
        witness_event w{};
        w.type = WITNESS_EV_WHEEL; w.down = -1; w.button = -1;
        w.dx = -e.wheel.x * 100.0; w.dy = -e.wheel.y * 100.0;   // the protocol's sign: + is down / zoom out
        witness_record_event(&w);
        return true;
      }
      case SDL_EVENT_FINGER_DOWN: case SDL_EVENT_FINGER_MOTION: case SDL_EVENT_FINGER_UP: case SDL_EVENT_FINGER_CANCELED: {
        if (e.tfinger.touchID == SDL_MOUSE_TOUCHID) return true;   // made from the mouse: the mouse is what is recorded
        const bool ends = e.type == SDL_EVENT_FINGER_UP || e.type == SDL_EVENT_FINGER_CANCELED;
        const SDL_FingerID id = e.tfinger.fingerID;
        int slot = -1;
        for (int i = 0; i < 10; i++) if (rec.finger[i].on && rec.finger[i].ev.fingerID == id) slot = i;
        if (e.type == SDL_EVENT_FINGER_DOWN) {
          rec.fingersLetGo.erase(std::remove(rec.fingersLetGo.begin(), rec.fingersLetGo.end(), id), rec.fingersLetGo.end());
          for (int i = 0; i < 10 && slot < 0; i++) if (!rec.finger[i].on) slot = i;
          if (slot < 0) return true;   // an eleventh finger (the protocol has 10)
          rec.finger[slot].on = true;
        } else if (slot < 0) {
          // not a finger the recording pressed: one a lost focus let go of (its moves and its late release are dropped:
          // the view had that release) or one put down before the recording began
          auto it = std::find(rec.fingersLetGo.begin(), rec.fingersLetGo.end(), id);
          if (it == rec.fingersLetGo.end()) return true;
          if (ends) rec.fingersLetGo.erase(it);
          return false;
        }
        rec.finger[slot].ev = e.tfinger;
        float lx, ly;
        recFinger(e.tfinger, lx, ly);
        const int phase = e.type == SDL_EVENT_FINGER_DOWN ? WITNESS_TOUCH_DOWN : e.type == SDL_EVENT_FINGER_UP ? WITNESS_TOUCH_UP
                        : e.type == SDL_EVENT_FINGER_CANCELED ? WITNESS_TOUCH_CANCEL : WITNESS_TOUCH_MOVE;
        witness_record_touch_ex(slot, lx, ly, phase, e.tfinger.pressure);   // (pressure <= 0: not given; replays use 1)
        if (ends) rec.finger[slot].on = false;
        return true;
      }
      default: return true;
    }
  };
  // a lost focus during a recording (the protocol): everything the recording holds is let go of, in the view (which may
  // never hear of it: a release while another window has the focus is lost) and in the replay alike, and a script's held
  // keys with them, as the keyboard's own state is reset. Then the pause it causes is recorded as the action "pause",
  // which the replay applies at the same point of the input stream (a harness's window never loses the focus).
  auto recLetGo = [&](bool pause) {
    for (int i = 0; i < 10; i++) {
      if (!rec.finger[i].on) continue;
      SDL_Event ev;
      SDL_zero(ev);
      ev.tfinger = rec.finger[i].ev;
      ev.type = SDL_EVENT_FINGER_CANCELED;   // the system ended it (the protocol's cancel): the view takes it as no tap
      float lx, ly;
      recFinger(ev.tfinger, lx, ly);
      witness_record_touch_ex(i, lx, ly, WITNESS_TOUCH_CANCEL, ev.tfinger.pressure);
      rec.finger[i].on = false;
      rec.fingersLetGo.push_back(ev.tfinger.fingerID);
      view.event(ev, game);
    }
    for (int b = 0; b < 3; b++) {
      if (!rec.button[b]) continue;
      SDL_Event ev;
      SDL_zero(ev);
      ev.button = rec.buttonEv[b];
      ev.type = SDL_EVENT_MOUSE_BUTTON_UP;
      ev.button.down = false;
      ev.button.x = rec.mouseX; ev.button.y = rec.mouseY;   // where the pointer is (the replay moves it there first)
      float lx, ly;
      recLogical(ev.button.x, ev.button.y, lx, ly);
      witness_record_mouse(lx, ly, b, 0);
      rec.button[b] = false;
      rec.buttonLetGo[b] = true;
      view.event(ev, game);
    }
    for (const std::string& k : rec.keys) { witness_record_key(k.c_str(), 0); rec.keysLetGo.push_back(k); }
    rec.keys.clear();
    for (const Held& hd : held) view.scriptHold(hd.sc, false);
    held.clear();
    if (pause) {
      witness_event w{};
      w.type = WITNESS_EV_ACTION; w.down = -1; w.button = -1;
      std::snprintf(w.name, sizeof w.name, "%s", "pause");
      witness_record_event(&w);
    }
  };
#endif
  // the window lost the focus (or was hidden; a script's `blur` forces it): play pauses (not under --shot or a script,
  // whose windows often sit behind others), and the game is saved (phones kill backgrounded web apps without warning)
  auto focusLost = [&](bool force) {
    const bool pause = game.mode == Mode::Play && (force || (!shotPath && !scripted));
#ifdef EMB_WITNESS
    if (witness_recording()) recLetGo(pause);
#endif
    if (pause) game.mode = Mode::Paused;
    if (!noSave) writeSave(game);
  };
  auto plTile = [&](int& x, int& y) { x = (int)std::floor(game.pl().p.x / TILE); y = (int)std::floor((game.pl().p.y - 2) / TILE); };
  auto findActorIdx = [&](int id) { for (size_t k = 1; k < game.actors.size(); k++) if (game.actors[k].id == id) return (int)k; return -1; };

  auto startWalk = [&](const ScriptCmd& c) {
    walk = Walk();
    walk.on = true;
    walk.wox = game.world.ox; walk.woy = game.world.oy;
    walk.line = c.line;
    std::vector<std::string> a(c.a.begin() + 1, c.a.end());
    auto num = [](const std::string& v) { return !v.empty() && (std::isdigit((unsigned char)v[0]) || v[0] == '.'); };
    bool tiles = a.size() >= 2 && num(a[0]) && num(a[1]);
    size_t nTarget = tiles ? 2 : 1;
    if (a.size() > nTarget && num(a[nTarget])) walk.timeout = (float)std::atof(a[nTarget].c_str());
    if (a.empty()) { walk.on = false; fail(c.line, "walkto needs a target"); return; }
    walk.what = a[0];
    int px, py;
    plTile(px, py);
    static const struct { const char* n; art::Building b; } bt[] = {
        {"inn", art::Building::Inn}, {"shop", art::Building::Shop}, {"smithy", art::Building::Smithy}, {"temple", art::Building::Temple},
        {"keep", art::Building::Keep}, {"tower", art::Building::Tower}, {"house", art::Building::House}, {"farmhouse", art::Building::Farmhouse},
        {"palace", art::Building::Palace}, {"barracks", art::Building::Barracks}};
    static const struct { const char* n; Role r; } rt[] = {
        {"innkeeper", Role::Innkeeper}, {"merchant", Role::Merchant}, {"smith", Role::Smith}, {"priest", Role::Priest},
        {"jarl", Role::Jarl}, {"guard", Role::Guard}, {"villager", Role::Villager}, {"mage", Role::Mage}, {"farmer", Role::Farmer},
        {"king", Role::King}, {"captain", Role::Captain}};
    if (tiles) { walk.kind = 0; walk.tx = std::atoi(a[0].c_str()); walk.ty = std::atoi(a[1].c_str()); return; }
    if (walk.what == "upstairs" || walk.what == "downstairs") {
      bool up = walk.what == "upstairs";
      const Stairs& s = up ? game.sub.up : game.sub.down;
      if (!game.inside || game.subBldg < 0 || !s.valid()) { walk.on = false; fail(c.line, "walkto " + walk.what + ": no stairs here"); return; }
      walk.kind = 4; walk.tx = s.x; walk.ty = s.y; walk.floor0 = game.subFloor; return;
    }
    if (walk.what == "exit") {
      if (!game.inside) { walk.on = false; fail(c.line, "walkto exit: not inside"); return; }
      walk.kind = 3; walk.tx = game.sub.exitX; walk.ty = game.sub.exitY; return;
    }
    for (auto& b : bt)
      if (walk.what == b.n) {
        if (game.inside) { walk.on = false; fail(c.line, "walkto " + walk.what + ": already inside"); return; }
        int best = -1; float bd = 1e30f;
        const auto& B = game.world.over.bldgs;
        for (size_t i = 0; i < B.size(); i++) {
          if (B[i].type != b.b) continue;
          float d = std::hypot((float)(B[i].doorX() - px), (float)(B[i].doorY() - py));
          if (d < bd) { bd = d; best = (int)i; }
        }
        if (best < 0 || bd > 120) { walk.on = false; fail(c.line, "walkto " + walk.what + ": none nearby"); return; }
        walk.kind = 1; walk.bldg = best; walk.tx = B[best].doorX(); walk.ty = B[best].doorY() + 1;
        return;
      }
    for (auto& r : rt)
      if (walk.what == r.n) {
        int best = -1; float bd = 1e30f;
        for (size_t k = 1; k < game.actors.size(); k++) {
          const Actor& ac = game.actors[k];
          if (!ac.npc || ac.role != r.r || ac.st == AState::Dead) continue;
          float d = len2(ac.p - game.pl().p);
          if (d < bd) { bd = d; best = ac.id; }
        }
        if (best < 0) { walk.on = false; fail(c.line, "walkto " + walk.what + ": nobody with that role here"); return; }
        walk.kind = 2; walk.actorId = best;
        return;
      }
    // (M2) giver: the nearest person with a job to offer or a reward waiting (any role). When nobody is in play yet
    // (townsfolk who hid from a fight come back out a while later: on a loaded machine the sim runs behind script
    // time) the walk waits for one (kind 5) until its timeout.
    if (walk.what == "giver") {
      walk.kind = 5;
      return;
    }
    // a person by name (as rpg_test's EMB_SCRIPT_INFO lists them): walkto VIGRIMA
    for (size_t k = 1; k < game.actors.size(); k++) {
      const Actor& ac = game.actors[k];
      if (ac.npc && ac.st != AState::Dead && ac.name == walk.what) { walk.kind = 2; walk.actorId = ac.id; return; }
    }
    walk.on = false;
    fail(c.line, "walkto: unknown target '" + walk.what + "'");
  };
  auto endWalk = [&](bool ok, const char* why) {
    setMove(false, false, false, false);
    if (!ok) fail(walk.line, "walkto " + walk.what + ": " + why);
    else std::printf("script %.2f: walkto %s done in %.1f s\n", scriptT, walk.what.c_str(), walk.t);
    if (ok && walk.talk) { pushKey(SDL_SCANCODE_E, true); pushKey(SDL_SCANCODE_E, false); }
    walk.on = false;
  };
  auto findGiver = [&]() {
    int best = -1; float bd = 1e30f;
    for (int pass = 0; pass < 2 && best < 0; pass++)   // a reward waiting first, then a job on offer
      for (size_t k = 1; k < game.actors.size(); k++) {
        const Actor& ac = game.actors[k];
        if (!ac.npc || ac.st == AState::Dead || !(pass == 0 ? game.rewardWaiting(ac) : game.offersWork(ac))) continue;
        float d = len2(ac.p - game.pl().p);
        if (d < bd) { bd = d; best = ac.id; }
      }
    return best;
  };
  auto stepWalk = [&](float dt) {
    walk.t += dt;
    if (!game.inside && (game.world.ox != walk.wox || game.world.oy != walk.woy)) {
      // the endless window moved under the walk: the target's window tiles moved with it, the path is stale
      walk.tx -= game.world.ox - walk.wox; walk.ty -= game.world.oy - walk.woy;
      walk.wox = game.world.ox; walk.woy = game.world.oy;
      walk.path.clear(); walk.step = 0; walk.replanT = 0;
    }
    if (walk.kind == 5) {   // walkto giver: waiting for someone with work or a reward to be in play
      const int id = findGiver();
      if (id >= 0) { walk.kind = 2; walk.actorId = id; }
      else if (walk.t > walk.timeout) {
        // say where the givers of the quests waiting to be turned in are (a test that fails here needs to know)
        for (const Quest& q : game.quests) {
          if (q.state != QState::Complete) continue;
          int found = 0;
          for (size_t k = 1; k < game.actors.size(); k++) {
            const Actor& ac = game.actors[k];
            if (ac.name != q.giverName) continue;
            found++;
            std::printf("script:   giver %s of '%s': in play, %.0f tiles away, state %d, site %d bldg %d slot %d (quest: %d %d %d)\n", ac.name.c_str(),
                        q.title.c_str(), len(ac.p - game.pl().p) / TILE, (int)ac.st, ac.site, ac.bldg, ac.slot, q.giverSite, q.giverBldg, q.giverSlot);
          }
          if (!found) std::printf("script:   giver %s of '%s': not in play\n", q.giverName.c_str(), q.title.c_str());
        }
        endWalk(false, "nobody here offers work or holds a reward");
        return;
      }
      else { setMove(false, false, false, false); return; }
    }
    if (walk.t > walk.timeout) {
      // (M7c integration) where it stuck: the player (@), the target (T), blocked tiles (#) round the target
      {
        int qx, qy;
        plTile(qx, qy);
        const Map& mm = game.map();
        std::printf("script:   walk stuck at %d,%d, target %d,%d\n", qx, qy, walk.tx, walk.ty);
        for (int y = walk.ty - 6; y <= walk.ty + 6; y++) {
          std::string row = "script:   ";
          for (int x = walk.tx - 12; x <= walk.tx + 12; x++)
            row += (x == qx && y == qy) ? '@' : (x == walk.tx && y == walk.ty) ? 'T' : !mm.in(x, y) ? ' ' : mm.blocked(x, y) ? '#' : '.';
          std::printf("%s\n", row.c_str());
        }
        // (M8b merge) a walk to someone: why the prompt is not theirs (who has it, a foe in reach, the mode)
        if (walk.kind == 2) {
          const int k = findActorIdx(walk.actorId);
          const int it = game.interactTarget();
          const int ik = it >= 0 ? findActorIdx(it) : -1;
          std::printf("script:   walking to %s, %.1f px off; the prompt is %s; a foe in reach %d; mode %d\n",
                      k >= 0 ? game.actors[(size_t)k].name.c_str() : "?", k >= 0 ? len(game.actors[(size_t)k].p - game.pl().p) : -1.0f,
                      ik >= 0 ? game.actors[(size_t)ik].name.c_str() : "nobody's", (int)game.foeInReach(), (int)game.mode);
          for (size_t a = 1; a < game.actors.size(); a++) {
            const Actor& ac = game.actors[a];
            if (ac.st == AState::Dead || len(ac.p - game.pl().p) > 60.0f) continue;
            std::printf("script:     near: %s npc %d hostile %d aggro %d target-me %d comp %d %.1f px\n", ac.name.c_str(), (int)ac.npc, (int)ac.hostile,
                        (int)ac.aggro, (int)(ac.target == game.pl().id), (int)ac.comp, len(ac.p - game.pl().p));
          }
        }
      }
      endWalk(false, "timed out");
      return;
    }
    if (game.mode != Mode::Play) { setMove(false, false, false, false); return; }   // a dialogue or menu is up: wait
    int px, py;
    plTile(px, py);
    // arrived?
    if (walk.kind == 1 && game.inside) { endWalk(true, ""); return; }
    if (walk.kind == 3 && !game.inside) { endWalk(true, ""); return; }
    if (walk.kind == 4 && game.subFloor != walk.floor0) { endWalk(true, ""); return; }
    if (walk.kind == 2) {
      int k = findActorIdx(walk.actorId);
      if (k < 0 && walk.what == "giver") {
        // (M7b) the giver walked off (indoors, out of the window): look again for the nearest one until the timeout
        walk.kind = 5; walk.actorId = -1; walk.path.clear(); walk.step = 0; walk.replanT = 0;
        setMove(false, false, false, false);
        return;
      }
      if (k < 0) { endWalk(false, "they left"); return; }
      // in reach: keep closing in a moment longer (a step from the edge of reach, a wandering innkeeper or a neighbour
      // who comes nearer turns the next key press into the wrong conversation), unless already close
      if (game.interactTarget() == walk.actorId) {
        walk.closeT += dt;
        if (len(game.actors[k].p - game.pl().p) < 20.0f || walk.closeT > 0.6f) { endWalk(true, ""); return; }
      }
      // (M8b merge, m2_sim_quests) a keeper at their stall is talked to only from the customers' side (interactTarget):
      // the walk makes for a free tile in front of the counter, not the keeper's own tile (from beside or behind the
      // stall, 9 px off, the prompt was nobody's and the turn-in timed out)
      const Actor& A = game.actors[(size_t)k];
      Vec2 at = A.p;
      if (A.stallKeeper && len2(A.p - A.home) < 4.0f) {
        const Map& mm = game.map();
        for (float tiles : {2.0f, 1.0f, 2.5f}) {
          const Vec2 q = A.p + faceVec(A.face) * (tiles * TILE);
          const int qx = (int)std::floor(q.x / TILE), qy = (int)std::floor((q.y - 2) / TILE);
          if (mm.in(qx, qy) && !mm.blocked(qx, qy)) { at = q; break; }
        }
      }
      walk.tx = (int)std::floor(at.x / TILE); walk.ty = (int)std::floor((at.y - 2) / TILE);
    }
    if (walk.kind == 0 && px == walk.tx && py == walk.ty) { endWalk(true, ""); return; }
    if (walk.kind == 4 && walk.what == "downstairs" && game.stairsAsleep()) {
      // just climbed: the stairwell sleeps until you have stepped off the tile you arrived on. Step to a free
      // neighbour (south first) and the stairs are awake again.
      static const int nd[4][2] = {{0, 1}, {1, 0}, {-1, 0}, {0, -1}};
      for (auto& d : nd) {
        int nx = px + d[0], ny = py + d[1];
        int pr = game.sub.propAt(nx, ny);
        if (!game.sub.in(nx, ny) || game.sub.blocked(nx, ny) || pr == (int)art::Prop::StairsDown + 1 || pr == (int)art::Prop::StairsUp + 1) continue;
        setMove(d[1] < 0, d[0] < 0, d[1] > 0, d[0] > 0);
        return;
      }
    }
    if (walk.kind == 4 && px == walk.tx && py == walk.ty) {   // on the steps: keep walking onto them until the floor changes
      // (up: into the flight, north; down: a step south across the stairwell, as a player coming from its foot would)
      float cx = walk.tx * TILE + 8.0f - game.pl().p.x;
      bool up = walk.what == "upstairs";
      setMove(up, cx < -2, !up, cx > 2);
      return;
    }
    // at the approach tile: the last step is a push through the door (up) or onto the exit (down)
    if ((walk.kind == 1 || walk.kind == 3) && px == walk.tx && (py == walk.ty || (walk.kind == 3 && py == walk.ty - 1))) {
      walk.finalT += dt;
      float cx = walk.tx * TILE + 8.0f - game.pl().p.x;
      setMove(walk.kind == 1, cx < -2, walk.kind == 3, cx > 2);
      return;
    }
    walk.replanT -= dt;
    if (walk.replanT <= 0 || walk.step >= walk.path.size()) {
      findPath(game.map(), px, py, walk.tx, walk.ty, walk.path);
      walk.step = 0;
      walk.replanT = walk.kind == 2 ? 0.4f : 1.0f;
    }
    const Map& m = game.map();
    while (walk.step < walk.path.size()) {
      int wx = walk.path[walk.step] % m.w, wy = walk.path[walk.step] / m.w;
      Vec2 tgt(wx * TILE + 8.0f, wy * TILE + 10.0f);
      Vec2 d = tgt - game.pl().p;
      if (std::fabs(d.x) <= 2.5f && std::fabs(d.y) <= 2.5f) { walk.step++; continue; }
      setMove(d.y < -2.5f, d.x < -2.5f, d.y > 2.5f, d.x > 2.5f);
      return;
    }
    setMove(false, false, false, false);   // the nearest reachable tile: wait there (an actor may still come within reach)
  };
  // M2: commands registered through rpg/view/script_api.h. A command that returns false is retried every frame (script
  // time stands still) until it returns true or its timeout passes.
  struct ExtRun { const ScriptExt* e = nullptr; ScriptCmd cmd; float waited = 0; } extRun;
  auto callExt = [&](float dt) {
    ScriptCtx ctx{game, view, extRun.cmd.a, extRun.cmd.line, scriptT, extRun.waited, [&](const std::string& m) { fail(extRun.cmd.line, m); },
                  [&](float x, float y) {
                    SDL_FingerID id = nextFinger++;
                    pushFinger(SDL_EVENT_FINGER_DOWN, id, x, y);
                    lifts.push_back({id, x, y, scriptT + 0.08f});
                  }};
    if (extRun.e->fn(ctx)) { extRun.e = nullptr; return; }
    extRun.waited += dt;
    if (extRun.waited > extRun.e->timeout) { fail(extRun.cmd.line, std::string(extRun.e->name) + ": timed out"); extRun.e = nullptr; }
  };
  auto startExt = [&](const std::string& name, const ScriptCmd& c) {
    for (const ScriptExt& e : scriptExts())
      if (name == e.name) {
        extRun.e = &e; extRun.cmd = c; extRun.waited = 0;
        callExt(0);
        return true;
      }
    return false;
  };
  auto modeByName = [](const std::string& s, Mode& m) {
    static const char* n[] = {"title", "play", "dialogue", "menu", "shop", "levelup", "dead", "paused", "creator", "forge", "build"};
    for (int i = 0; i < (int)(sizeof n / sizeof n[0]); i++) if (s == n[i]) { m = (Mode)i; return true; }
    return false;
  };
  auto runCmd = [&](const ScriptCmd& c) {
    const std::string& op = c.a[0];
    auto arg = [&](size_t i) { return i < c.a.size() ? c.a[i] : std::string(); };
    std::printf("script %.2f: %s\n", scriptT, c.text.c_str());
    if (op == "key") {
      SDL_Scancode sc = keyFromName(arg(1));
      if (sc == SDL_SCANCODE_UNKNOWN) { fail(c.line, "unknown key '" + arg(1) + "'"); return; }
      pushKey(sc, true); pushKey(sc, false);
    } else if (op == "hold") {
      SDL_Scancode sc = keyFromName(arg(1));
      if (sc == SDL_SCANCODE_UNKNOWN) { fail(c.line, "unknown key '" + arg(1) + "'"); return; }
      pushKey(sc, true);
      view.scriptHold(sc, true);
      held.push_back({sc, scriptT + (float)std::atof(arg(2).c_str())});
    } else if (op == "tap") {
      float x = (float)std::atof(arg(1).c_str()), y = (float)std::atof(arg(2).c_str());
      SDL_FingerID id = nextFinger++;
      pushFinger(SDL_EVENT_FINGER_DOWN, id, x, y);
      lifts.push_back({id, x, y, scriptT + 0.08f});
    } else if (op == "click") {
      float x = (float)std::atof(arg(1).c_str()), y = (float)std::atof(arg(2).c_str());
      pushClick(x, y, true); pushClick(x, y, false);
    } else if (op == "press" || op == "swipe") {   // (M9) a held finger / a drag (the quick-cast hold and swipe)
      size_t k = 1;
      float x = 0, y = 0;
      if (arg(1) == "quick") {
        const int b = std::atoi(arg(2).c_str()) - 1;
        const QuickBtn q = quickBtnPos(b < 0 ? 0 : b > 4 ? 4 : b);
        x = q.x; y = q.y; k = 3;
      } else {
        x = (float)std::atof(arg(1).c_str()); y = (float)std::atof(arg(2).c_str()); k = 3;
      }
      float x1 = x, y1 = y;
      if (op == "swipe") {
        if (arg(1) == "quick") { x1 = x + (float)std::atof(arg(k).c_str()); y1 = y + (float)std::atof(arg(k + 1).c_str()); }
        else { x1 = (float)std::atof(arg(k).c_str()); y1 = (float)std::atof(arg(k + 1).c_str()); }
        k += 2;
      }
      const float secs = std::max(0.05f, (float)std::atof(arg(k).c_str()));
      SDL_FingerID id = nextFinger++;
      pushFinger(SDL_EVENT_FINGER_DOWN, id, x, y);
      if (op == "swipe") {   // the drag in four steps over the first half of the gesture
        for (int s = 1; s <= 4; s++) drags.push_back({id, x + (x1 - x) * s / 4.0f, y + (y1 - y) * s / 4.0f, scriptT + secs * 0.5f * s / 4.0f});
      }
      lifts.push_back({id, x1, y1, scriptT + secs});
    } else if (op == "blur") {   // the window loses the focus (an alt-tab): play pauses, a recording lets go of what it holds
      if (witness) { fail(c.line, "blur: a harness's hidden window never loses the focus"); return; }
      focusLost(true);
    } else if (op == "shot") {
      pendingShot = arg(1).empty() ? "shot.png" : arg(1);
    } else if (op == "quit") {
      running = false;
    } else if (op == "walkto") {
      startWalk(c);
    } else if (op == "talkto") {   // walkto a person, then talk in the very frame they come in reach (no one else steps in)
      startWalk(c);
      if (walk.on && (walk.kind == 2 || walk.kind == 5)) walk.talk = true;
      else if (walk.on) { walk.on = false; setMove(false, false, false, false); fail(c.line, "talkto needs a person"); }
    } else if (op == "newgame") {
      startNew(startSeed);
    } else if (op == "goto") {
      doGoto(arg(1), arg(2) == "enter");
    } else if (op == "enter") {   // M0b: enter <type> [floor] [nth]
      static const struct { const char* n; art::Building b; } et[] = {
          {"inn", art::Building::Inn}, {"shop", art::Building::Shop}, {"smithy", art::Building::Smithy}, {"temple", art::Building::Temple},
          {"keep", art::Building::Keep}, {"tower", art::Building::Tower}, {"house", art::Building::House},
          {"stonehouse", art::Building::StoneHouse}, {"farmhouse", art::Building::Farmhouse}, {"hut", art::Building::Hut},
          {"palace", art::Building::Palace}, {"barracks", art::Building::Barracks},
          // M1 economy: the production buildings
          {"windmill", art::Building::Windmill}, {"watermill", art::Building::Watermill}, {"granary", art::Building::Granary},
          {"bakery", art::Building::Bakery}, {"butcher", art::Building::Butcher}, {"tannery", art::Building::Tanner},
          {"tanner", art::Building::Tanner}, {"fishmonger", art::Building::Fishmonger}, {"smelter", art::Building::Smelter},
          {"sawmill", art::Building::Sawmill}, {"weaver", art::Building::Weaver},
          // M3b: the society's purposes
          {"guildhall", art::Building::Guildhall}, {"exchange", art::Building::Exchange}, {"meadhall", art::Building::MeadHall},
          {"bathhouse", art::Building::Bathhouse}, {"teahouse", art::Building::TeaHouse}, {"lodge", art::Building::Lodge},
          {"councilhall", art::Building::CouncilHall}};
      int want = -1;
      for (auto& e : et) if (arg(1) == e.n) want = (int)e.b;
      int fl = std::atoi(arg(2).c_str()), nth = std::atoi(arg(3).c_str());
      int px, py;
      if (game.inside && game.subBldg >= 0) { px = game.world.over.bldgs[game.subBldg].doorX(); py = game.world.over.bldgs[game.subBldg].doorY(); }
      else plTile(px, py);
      std::vector<std::pair<float, int>> cand;
      const auto& B = game.world.over.bldgs;
      for (size_t i = 0; i < B.size(); i++)
        if ((int)B[i].type == want && fl < B[i].floors()) cand.push_back({std::hypot((float)(B[i].doorX() - px), (float)(B[i].doorY() - py)), (int)i});
      std::sort(cand.begin(), cand.end());
      // (fixer r2) the seat of power is the settlement's own: "enter palace" never walks across the map into another
      // realm's (enterseat enters whatever the society built here). Other types still take the nearest in the window.
      if (want == (int)art::Building::Palace) while (!cand.empty() && cand.back().first >= 160.0f) cand.pop_back();
      if (want < 0) fail(c.line, "enter: unknown building type '" + arg(1) + "'");
      else if (nth >= (int)cand.size()) fail(c.line, "enter " + arg(1) + ": no such building with floor " + arg(2) + (want == (int)art::Building::Palace ? " near (the seat of power here: enterseat)" : ""));
      else if (!game.debugEnterBuilding(cand[(size_t)nth].second, fl)) fail(c.line, "enter " + arg(1) + ": could not enter");
      else { game.mode = Mode::Play; view.snap(game); }
    } else if (op == "floor") {
      int fl = std::atoi(arg(1).c_str());
      if (!game.inside || game.subBldg < 0 || fl < 0 || fl >= game.world.over.bldgs[game.subBldg].floors()) fail(c.line, "floor " + arg(1) + ": not a floor here");
      else { game.changeFloor(fl); view.snap(game); }
    } else if (op == "choose") {   // M0b: choose the open dialogue's option whose label contains the words
      std::string t;
      for (size_t k = 1; k < c.a.size(); k++) { if (k > 1) t += ' '; t += c.a[k]; }
      // (fixer M7b r1) as the player would: an option on show, or the header (or SOMETHING ELSE.) that shows it tapped first
      int pickI = -1;
      if (game.mode == Mode::Dialogue) pickI = dlgReach(game, [&](const DlgOpt& o) { return o.label.find(t) != std::string::npos; }, true);
      if (game.mode != Mode::Dialogue || pickI < 0) fail(c.line, "choose: no dialogue option '" + t + "' the player can reach");
      else { game.dialogueChoose(pickI); view.snap(game); }
    } else if (op == "at") {   // M1: a global tile
      game.teleportGlobal((int32_t)std::atoi(arg(1).c_str()), (int32_t)std::atoi(arg(2).c_str()));
      game.mode = Mode::Play;
      view.snap(game);
    } else if (op == "nearshift") {   // M1: step to 2 tiles short of the window's east|west|north|south shift line
      int tx = 0, ty = 0;
      plTile(tx, ty);
      const int lo = World::WIN_SHIFT, hi = World::WIN - World::WIN_SHIFT;
      int ddx = 0, ddy = 0;
      if (arg(1) == "east") { tx = hi - 2; ddx = 1; } else if (arg(1) == "west") { tx = lo + 1; ddx = -1; }
      else if (arg(1) == "south") { ty = hi - 2; ddy = 1; } else if (arg(1) == "north") { ty = lo + 1; ddy = -1; }
      // a lane where the next few tiles across the line are open (so the walk really crosses it)
      const Map& M = game.world.over;
      for (int off = 0; off < 120; off++) {
        int o = (off & 1) ? -(off + 1) / 2 : off / 2;
        int x0 = tx + (ddy ? o : 0), y0 = ty + (ddx ? o : 0);
        bool open = true;
        for (int k = -1; k <= 6 && open; k++)
          if (M.blocked(x0 + ddx * k, y0 + ddy * k)) open = false;
        if (open) { tx = x0; ty = y0; break; }
      }
      { game.teleportGlobal(game.world.ox + tx, game.world.oy + ty); game.mode = Mode::Play; view.snap(game); }
    } else if (op == "walk") {   // M1: walk east|west|north|south SECS (a held direction key)
      static const struct { const char* n; SDL_Scancode sc; } dirs[] = {
          {"east", SDL_SCANCODE_D}, {"west", SDL_SCANCODE_A}, {"north", SDL_SCANCODE_W}, {"south", SDL_SCANCODE_S}};
      SDL_Scancode sc = SDL_SCANCODE_UNKNOWN;
      for (auto& d : dirs) if (arg(1) == d.n) sc = d.sc;
      if (sc == SDL_SCANCODE_UNKNOWN) fail(c.line, "walk: unknown direction '" + arg(1) + "'");
      else {
        pushKey(sc, true);
        view.scriptHold(sc, true);
        held.push_back({sc, scriptT + (float)std::atof(arg(2).c_str())});
      }
    } else if (op == "talk") {
      doTalk(std::atoi(arg(1).c_str()));
    } else if (op == "fight") {
      doFight(arg(1), std::atoi(arg(2).c_str()));
    } else if (op == "kit") {
      game.debugKit();
    } else if (op == "gear") {
      giveGear(std::atoi(arg(1).c_str()), arg(2));
    } else if (op == "chestgo") {
      // (M8b SOURCES) chestgo [hoard|explore|chest]: stand beside the nearest closed chest of the current map (of that
      // loot source: LootOps::chestSource), facing it, so `key e` opens it through the real Game::openChest
      const std::string want = arg(1);
      Map& m = game.map();
      const Vec2 pp = game.pl().p;
      int bx = -1, by = -1;
      float best = 1e30f;
      for (int y = 0; y < m.h; y++)
        for (int x = 0; x < m.w; x++) {
          if (m.propAt(x, y) != (int)art::Prop::Chest + 1) continue;
          const gear::LootSrc s = LootOps::chestSource(game, x, y).src;
          if (want == "hoard" && s != gear::LootSrc::Hoard) continue;
          if (want == "explore" && s != gear::LootSrc::Explore) continue;
          if (want == "chest" && s != gear::LootSrc::Chest) continue;
          const float dx = x * TILE + 8.0f - pp.x, dy = y * TILE + 8.0f - pp.y;
          float d = dx * dx + dy * dy;
          // one well inside the map first (the camera stops at the map's edge, where the HUD panels cover a find)
          if (x < 10 || y < 7 || x >= m.w - 10 || y >= m.h - 7) d += 1e9f;
          if (d < best) { best = d; bx = x; by = y; }
        }
      bool placed = false;
      if (bx >= 0) {
        static const int sides[4][2] = {{0, 1}, {-1, 0}, {1, 0}, {0, -1}};
        for (auto& s : sides) {
          if (m.blocked(bx + s[0], by + s[1])) continue;
          game.pl().p = Vec2((bx + s[0]) * TILE + 8.0f, (by + s[1]) * TILE + 10.0f);
          game.pl().aim = Vec2((float)-s[0], (float)-s[1]);
          game.pl().face = s[1] > 0 ? 1 : s[1] < 0 ? 0 : (s[0] > 0 ? 3 : 2);
          view.snap(game);
          placed = true;
          std::printf("script: chestgo: a %s chest at %d %d\n", gear::lootSrcName(LootOps::chestSource(game, bx, by).src), bx, by);
          break;
        }
      }
      if (!placed) fail(c.line, "chestgo " + want + ": no reachable closed chest here");
      else { lastChestX = bx; lastChestY = by; }
    } else if (op == "chestaway") {
      // (M8b SOURCES) chestaway: right after `key e`, step 4 tiles clear of the chest chestgo found (outside the
      // pickups' 40 px pull), toward the map's middle, so a shot shows the find lying on the floor
      Map& m = game.map();
      bool moved = false;
      std::vector<std::pair<int, int>> dirs = {{0, 1}, {1, 0}, {-1, 0}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
      const int cx = m.w / 2 - lastChestX, cy = m.h / 2 - lastChestY;
      std::stable_sort(dirs.begin(), dirs.end(), [&](const std::pair<int, int>& a, const std::pair<int, int>& b) {
        return a.first * cx + a.second * cy > b.first * cx + b.second * cy;
      });
      for (int dist = 4; dist <= 6 && !moved && lastChestX >= 0; dist++)
        for (const auto& d : dirs) {
          const int x = lastChestX + d.first * dist, y = lastChestY + d.second * dist;
          if (m.blocked(x, y)) continue;
          game.pl().p = Vec2(x * TILE + 8.0f, y * TILE + 10.0f);
          game.pl().aim = Vec2((float)-d.first, (float)-d.second);
          moved = true;
          break;
        }
      if (!moved) fail(c.line, "chestaway: no free tile 4-6 tiles from the chest");
    } else if (op == "strip") {
      for (ItemKind k : {ItemKind::Weapon, ItemKind::Bow, ItemKind::Staff, ItemKind::Armor, ItemKind::Helmet, ItemKind::Shield,
                         ItemKind::Ring, ItemKind::Amulet, ItemKind::Gloves, ItemKind::Boots, ItemKind::Cloak})
        if (int* e = game.equipSlot(k)) if (*e >= 0) game.useItem(*e);
    } else if (op == "god") {
      game.godMode = arg(1).empty() || arg(1) != "0";
    } else if (op == "hour") {
      game.hour = (float)std::atof(arg(1).c_str());
    } else if (op == "menu") {
      view.openMenu(game, std::atoi(arg(1).c_str()));
    } else if (op == "log") {
      // printed above
    } else if (op == "pos") {   // M1: print where the player is (window tile, global tile, window origin, site)
      int tx = 0, ty = 0;
      plTile(tx, ty);
      std::printf("script: pos window tile %d,%d global %d,%d origin %d,%d inside %d site %s\n", tx, ty, game.world.ox + tx, game.world.oy + ty,
                  game.world.ox, game.world.oy, game.inside ? 1 : 0, game.curSite >= 0 ? game.world.sites[(size_t)game.curSite].name.c_str() : "-");
      if (game.curSite >= 0) {
        int guards = 0, folk = 0;
        for (size_t k = 1; k < game.actors.size(); k++)
          if (game.actors[k].npc && game.actors[k].site == game.curSite) (game.actors[k].role == Role::Guard ? guards : folk)++;
        const Site& cs = game.world.sites[(size_t)game.curSite];
        auto it = game.world.siteSpawns.find(game.curSite);
        std::printf("script:   %s %s, %d buildings, %zu spawns; in play %d townsfolk, %d guards\n", siteTypeName(cs.type), cs.name.c_str(), cs.bldgCount,
                    it == game.world.siteSpawns.end() ? (size_t)0 : it->second.size(), folk, guards);
      }
      if (arg(1) == "map") {   // the tiles around: '#' blocked, '.' open, '@' the player
        for (int y = ty - 3; y <= ty + 3; y++) {
          std::string row;
          for (int x = tx - 8; x <= tx + 8; x++) row += (x == tx && y == ty) ? '@' : (game.map().blocked(x, y) ? '#' : '.');
          std::printf("script:   %s\n", row.c_str());
        }
        std::printf("script:   player px %.1f,%.1f state %d mode %d\n", game.pl().p.x, game.pl().p.y, (int)game.pl().st, (int)game.mode);
      }
    } else if (op == "expect") {
      std::string what = arg(1), want = arg(2);
      if (what == "mode") {
        Mode m;
        static const char* n[] = {"title", "play", "dialogue", "menu", "shop", "levelup", "dead", "paused", "creator", "forge", "build"};
        if (!modeByName(want, m)) fail(c.line, "expect mode: unknown mode '" + want + "'");
        else if (game.mode != m) fail(c.line, "expected mode " + want + ", got " + n[(int)game.mode]);
      } else if (what == "gold") {
        if (game.gold != std::atoi(want.c_str())) fail(c.line, "expected gold " + want + ", got " + std::to_string(game.gold));
      } else if (what == "name") {
        if (game.app.name != want) fail(c.line, "expected name " + want + ", got " + game.app.name);
      } else if (what == "background") {
        if ((int)game.background != std::atoi(want.c_str())) fail(c.line, "expected background " + want + ", got " + std::to_string((int)game.background));
      } else if (what == "slot") {
        static const struct { const char* n; ItemKind k; } sk[] = {
            {"weapon", ItemKind::Weapon}, {"bow", ItemKind::Bow}, {"staff", ItemKind::Staff}, {"armor", ItemKind::Armor},
            {"helmet", ItemKind::Helmet}, {"shield", ItemKind::Shield}, {"ring", ItemKind::Ring}, {"amulet", ItemKind::Amulet},
            {"gloves", ItemKind::Gloves}, {"boots", ItemKind::Boots}, {"cloak", ItemKind::Cloak}};
        int* e = nullptr;
        for (auto& q : sk) if (want == q.n) e = game.equipSlot(q.k);
        bool on = arg(3) != "0";
        if (!e) fail(c.line, "expect slot: unknown slot '" + want + "'");
        else if ((*e >= 0) != on) fail(c.line, "expected slot " + want + (on ? " worn" : " empty"));
      } else if (what == "floor") {
        if (game.subFloor != std::atoi(want.c_str())) fail(c.line, "expected floor " + want + ", got " + std::to_string(game.subFloor));
      } else if (what == "kingdom") {   // M1: the settlement here belongs to a kingdom (expect kingdom capital: its capital)
        int si = game.inside && game.subBldg >= 0 ? game.world.over.bldgs[(size_t)game.subBldg].site : game.curSite;
        const Kingdom* K = game.world.kingdomOf(si);
        if (!K) fail(c.line, "expected a kingdom's settlement here");
        else if (want == "capital" && !game.world.sites[(size_t)si].capital) fail(c.line, "expected " + game.world.sites[(size_t)si].name + " to be the capital of " + K->name);
        else std::printf("script: %s, kingdom of %s%s\n", game.world.sites[(size_t)si].name.c_str(), K->name.c_str(), game.world.sites[(size_t)si].capital ? " (capital)" : "");
      } else if (what == "npc") {   // M1: someone in play with this role (king, guard...) or title word (royal)
        bool ok = false;
        for (size_t k = 1; k < game.actors.size(); k++) {
          const Actor& ac = game.actors[k];
          if (!ac.npc || ac.st == AState::Dead) continue;
          if ((want == "king" && ac.role == Role::King) || (want == "guard" && ac.role == Role::Guard) || (want == "jarl" && ac.role == Role::Jarl) ||
              (want == "innkeeper" && ac.role == Role::Innkeeper) || (want == "royal" && ac.name.find("ROYAL") != std::string::npos))
            ok = true;
        }
        if (!ok) fail(c.line, "expected an npc '" + want + "' in play");
      } else if (what == "inside") {
        if ((want != "0") != game.inside) fail(c.line, std::string("expected inside ") + want + ", got " + (game.inside ? "1" : "0"));
      } else if (what == "quest" || what == "heard" || what == "option" || what == "text" || what == "tracked") {
        // the rest of the line (original case) is a substring to look for
        auto rest = [&](size_t from) { std::string r; for (size_t k = from; k < c.a.size(); k++) { if (k > from) r += ' '; r += c.a[k]; } return r; };
        if (what == "quest") {   // expect quest active|complete|done|none <title words>
          std::string t = rest(3);
          const Quest* q = nullptr;
          for (const Quest& x : game.quests) if (x.title.find(t) != std::string::npos) q = &x;
          static const char* sn[] = {"active", "complete", "done"};
          std::string got = q ? sn[(int)q->state] : "none";
          if (got != want) fail(c.line, "expected quest '" + t + "' " + want + ", got " + got);
        } else if (what == "tracked") {   // expect tracked <title words>
          std::string t = rest(2), got = "none";
          for (const Quest& x : game.quests) if (x.id == game.trackedQuest) got = x.title;
          if (got.find(t) == std::string::npos) fail(c.line, "expected tracked quest '" + t + "', got " + got);
        } else if (what == "heard") {   // a notice (say) since the script began contains these words
          std::string t = rest(2);
          bool ok = false;
          for (const std::string& h : heard) if (h.find(t) != std::string::npos) ok = true;
          if (!ok) fail(c.line, "never heard '" + t + "'" + (heard.empty() ? std::string() : " (last: " + heard.back() + ")"));
        } else if (what == "option") {   // the open dialogue offers an option containing these words
          std::string t = rest(2);
          // (fixer M7b r1) on show, or behind a header the player can tap (never one no page shows)
          const bool ok = game.mode == Mode::Dialogue && dlgReach(game, [&](const DlgOpt& o) { return o.label.find(t) != std::string::npos; }, false) >= 0;
          if (!ok) fail(c.line, "no dialogue option '" + t + "' the player can reach" + (game.dlg.opts.empty() ? std::string() : " (first: " + game.dlg.opts[0].label + ")"));
        } else {   // text: the open dialogue's text contains these words
          std::string t = rest(2);
          if (game.mode != Mode::Dialogue || game.dlg.text.find(t) == std::string::npos) fail(c.line, "dialogue text lacks '" + t + "': " + game.dlg.text);
        }
      } else if (!startExt("expect:" + what, c)) fail(c.line, "expect: unknown check '" + what + "'");
    } else if (!startExt(op, c)) {
      fail(c.line, "unknown command '" + op + "'");
    }
  };
  auto scriptStep = [&](float dt) {
    if (game.noticeT > 0 && !game.notice.empty() && (heard.empty() || heard.back() != game.notice)) heard.push_back(game.notice);
    // releases first, so a hold ending this frame lets go before new presses
    for (size_t i = 0; i < held.size();) {
      if (scriptT >= held[i].until) {
        view.scriptHold(held[i].sc, false);
        pushKey(held[i].sc, false);
        held.erase(held.begin() + i);
      } else i++;
    }
    for (size_t i = 0; i < drags.size();) {
      if (scriptT >= drags[i].at) { pushFinger(SDL_EVENT_FINGER_MOTION, drags[i].id, drags[i].x, drags[i].y); drags.erase(drags.begin() + i); }
      else i++;
    }
    for (size_t i = 0; i < lifts.size();) {
      if (scriptT >= lifts[i].at) { pushFinger(SDL_EVENT_FINGER_UP, lifts[i].id, lifts[i].x, lifts[i].y); lifts.erase(lifts.begin() + i); }
      else i++;
    }
    if (walk.on) { stepWalk(dt); return; }   // script time stands still while walking
    if (extRun.e) { callExt(dt); return; }   // ... and while a registered command is still busy
    while (scriptNext < script.size() && script[scriptNext].t <= scriptT && running) {
      runCmd(script[scriptNext++]);
      if (walk.on || extRun.e) return;
    }
    if (!witness && scriptNext >= script.size() && held.empty() && lifts.empty()) {
      if (scriptEndT < 0) scriptEndT = scriptT;
      if (scriptT - scriptEndT > 2.0f) { std::printf("script: end of script\n"); running = false; }
    }
    scriptT += dt;
  };

  // --perf / script summary (M1): the CPU work of each frame (everything before the present), the worst one, and the
  // worst one in which the endless window shifted under a walking player (the "no hitch at shifts" check)
  struct PerfAcc {
    double workSum = 0, workWorst = 0;        // this second
    double drawSum = 0, drawWorst = 0;        // (P2a) View::draw's share of it (the render baseline, L9)
    double worstAll = 0, worstShift = 0;      // the whole run
    double worstAfterShift = 0;               // the second after a walking shift (the view re-bakes what moved)
    int afterShift = 0;
    int frames = 0, shiftFrames = 0;
    long long chunks0 = 0;                    // chunks generated (main source + streamer) at the last print
  } pacc;
  auto chunksMade = [&]() -> long long {
    long long n = 0;
    if (game.world.src) n += game.world.src->stats().chunks;
    if (game.world.streamer) n += game.world.streamer->stats().chunksMade;
    return n;
  };
  auto printSummary = [&]() {
    const World::StreamStats& ss = game.world.sstats;
    std::printf("perf summary: %d frames, worst frame work %.1f ms, worst frame with a walking window shift %.1f ms (%d such frames), "
                "worst in the second after one %.1f ms; "
                "shifts %d (walking %d), worst shift %.1f ms, worst recentre %.1f ms, chunks prefetched %d, generated in a move %d "
                "(%d in walking shifts), records recycled %d\n",
                pacc.frames, pacc.worstAll, pacc.worstShift, pacc.shiftFrames, pacc.worstAfterShift, ss.shifts, ss.walkShifts, ss.worstShiftMs, ss.worstRecentreMs,
                ss.prefetched, ss.syncChunks, ss.syncInShifts, ss.recycled);
  };

#ifdef EMB_WITNESS
  // (Witness) one sim step as a harness takes it, and as a WITNESS_RECORD session takes each of its steps (so a replay
  // re-runs the very same calls): the view's input, the sim (in play), the view's update at the step's dt
  auto simStep = [&](float dt) {
    Input in = view.input(game);
    if (game.mode == Mode::Play) game.update(dt, in);
    else if (game.mode == Mode::Dead) game.update(dt, Input());   // (P2b B11) the hero's fall runs on the sim's clock
    else if (game.mode == Mode::Title) game.time += dt;
    view.update(game, dt);
  };
#endif
  // one frame of the game; desktop loops on it, the browser calls it once per animation frame
  auto frame = [&]() {
    const Uint64 work0 = SDL_GetPerformanceCounter();
    const int walkShifts0 = game.world.sstats.walkShifts;
    screen::frame(pix);
    game.frameWork(3.0);   // M1 streaming: collect the worker's chunks (native) or generate within ~3 ms (web)
    // NPC LOD: off screen is beyond the view (P2a: the world view, in world px, at the zoom in use)
    game.sleepHalfW = std::max(22.0f, view.viewW() * 0.5f / TILE + 4.0f);
    game.sleepHalfH = std::max(13.0f, view.viewH() * 0.5f / TILE + 4.0f);
    if (scripted) {
      Uint64 nowS = SDL_GetPerformanceCounter();
      static Uint64 prevS = nowS;
      float dtS = (float)std::min(0.1, (double)(nowS - prevS) / (double)freq);
      prevS = nowS;
      scriptStep(dtS);
    }
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
#ifdef EMB_WITNESS
      // a key's auto-repeat is not recorded (a replay presses each key once), so it does not reach the view either
      if (recordSync && (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) && e.key.repeat) continue;
      if (witness_recording() && !recordEvent(e)) continue;   // (a late release of what a lost focus let go of)
#endif
      switch (e.type) {
        case SDL_EVENT_QUIT: running = false; break;
        case SDL_EVENT_WILL_ENTER_BACKGROUND: if (!noSave) writeSave(game); break;
        case SDL_EVENT_WINDOW_FOCUS_LOST: case SDL_EVENT_WINDOW_HIDDEN: focusLost(false); break;
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
      else startNew(seed ? seed : (uint64_t)SDL_GetTicks() * 2654435761ull + 777);
    }
    // NEW ADVENTURE: making the world takes a few hundred ms on a phone, so the "FORGING THE WORLD" card is put on
    // screen first (this frame) and the world is made at the top of the next one, behind the card
    static int newGameIn = 0;
    if (newGameIn > 0 && --newGameIn == 0) {
      startNew(seed ? seed : (uint64_t)SDL_GetTicks() * 2654435761ull + 777);   // --seed / a script's seed: reproducible
      game.beginCreator();   // the character creator; the first save happens when it hands over to play
      view.creatorPrepare(game);   // (its homeland choices read the cultures round the start: behind the card too)
      view.loadingCard.clear();
    }
    if (view.wantNewGame) {
      view.wantNewGame = false;
      view.loadingCard = "FORGING THE WORLD";
      newGameIn = 2;
    }
    {   // the creator just finished: save the new character right away
      static Mode prevMode = Mode::Title;
      if (prevMode == Mode::Creator && game.mode == Mode::Play) {
        if (!noSave) writeSave(game);
        hasSave = !noSave;
        view.snap(game);
#ifdef EMB_WITNESS
        {
          StartSpec sp;
          sp.creator = true; sp.background = (int)game.background; sp.name = game.app.name;
          sp.ids = recordIds;   // the title world handed out ids before this one began: the replay's fresh Game starts there
          recordBegin(specJson(sp));
          rec = RecHeld();   // (a new playthrough's recording holds nothing yet)
          wStart = WitnessExtra();
          wStart.hasStart = !game.inside;
          wStart.startGX = game.world.ox + (int)std::floor(game.pl().p.x / TILE);
          wStart.startGY = game.world.oy + (int)std::floor((game.pl().p.y - 2) / TILE);
        }
#endif
      }
      prevMode = game.mode;
    }
    if (view.wantSave) { view.wantSave = false; if (!noSave) writeSave(game); }

    Uint64 now = SDL_GetPerformanceCounter();
    double frameDt = std::min(0.1, (double)(now - prev) / (double)freq);
    prev = now;
    double wall = (double)(now - t0) / (double)freq;
    fpsT += frameDt; fpsN++;
    if (fpsT >= 1.0) {
      if (perf) {
        const World::StreamStats& ss = game.world.sstats;
        long long ch = chunksMade();
        int regions = 0, settlements = 0;
        if (game.world.src) { regions = game.world.src->stats().regions; settlements = game.world.src->stats().settlements; }
        if (game.world.streamer) regions += game.world.streamer->stats().regionsMade;
        std::printf("perf fps=%d work avg %.2f ms worst %.2f ms | actors %zu npc awake %d asleep %d hostiles %d active sites %d | "
                    "chunks/s %lld worst shift %.1f ms (sync %d) recentre %.1f ms | regions %d settlements %d | window %d,%d mode %d\n",
                    (int)(fpsN / fpsT + 0.5), fpsN ? pacc.workSum / fpsN : 0.0, pacc.workWorst, game.actors.size(),
                    game.perf.npcAwake, game.perf.npcAsleep, game.perf.hostiles, game.perf.activeSites, (long long)((ch - pacc.chunks0) / fpsT),
                    ss.worstShiftMs, ss.syncInShifts, ss.worstRecentreMs, regions, settlements, game.world.ox, game.world.oy, (int)game.mode);
        pacc.chunks0 = ch;
        // (M6 fixer) the worst step of each world subsystem this second (only the ones over 2 ms: a hitch's suspects)
        static const char* const sysName[Game::PerfCounters::S_COUNT] = {"quests", "realm", "war", "story", "lifeStep", "raids", "foes", "lifeTick", "spawning", "home", "party"};
        std::string slow;
        for (int k = 0; k < Game::PerfCounters::S_COUNT; k++) {
          if (game.perf.sysWorstMs[k] >= 2.0) {
            char b[64];
            std::snprintf(b, sizeof b, " %s %.1f", sysName[k], game.perf.sysWorstMs[k]);
            slow += b;
          }
          game.perf.sysWorstMs[k] = 0;
        }
        if (!slow.empty()) std::printf("perf   worst step ms:%s\n", slow.c_str());
        std::printf("perf   draw avg %.2f ms worst %.2f ms | people textures %.1f MB\n", fpsN ? pacc.drawSum / fpsN : 0.0, pacc.drawWorst, view.peopleMB());   // (P2a)
        if (art::rigVersion() >= 2)   // (P2c SWITCH, C8) the rig-2 atlas cache: its MB and cap, evictions, atlases baked again after one
          std::printf("perf   rig2 cache %.1f MB cap %.0f | evictions %d rebakes %d | slowest ACTION atlas %.2f ms\n", view.lensStats.rigMB, view.rigCapMB(),
                      view.lensStats.rigEvictions, view.lensStats.rigRebakes, view.lensStats.actionMsMax);
      }
      pacc.workSum = 0; pacc.workWorst = 0;
      pacc.drawSum = 0; pacc.drawWorst = 0;
      fpsT = 0; fpsN = 0;
    }

    // EMB_TIMING: the 12 frames after a journey's fade lifts, split into sim steps, view update and draw
    static int afterTravel = 0;
    static bool wasTravelling = false;
    auto timingMark = [&]() {
      if (wasTravelling && !game.travelling()) afterTravel = 12;
      wasTravelling = game.travelling();
      return SDL_GetPerformanceCounter();
    };
    Uint64 simT0 = 0, simT1 = 0;
#ifdef EMB_WITNESS
    if (recordSync) {
      simT0 = timingMark();
      // WITNESS_RECORD: every recorded step is the replay's step (simStep: input, sim, view update), so a replay
      // re-runs the session call for call; menus and dialogue step once a frame at its dt, a lost-focus pause not at all
      if (game.mode == Mode::Play) {
        acc += frameDt;
        int steps = 0;
        while (acc >= SIM_DT && steps < 8) {
          simStep(SIM_DT);
          witness_record_frame(SIM_DT);
          acc -= SIM_DT; steps++;
        }
        if (steps == 8) acc = 0;
        saveT += (float)frameDt;
        if (!noSave && saveT > 20.0f) { saveT = 0; writeSave(game); }
      } else {
        acc = 0;
        if (game.mode == Mode::Title) { game.time += (float)frameDt; view.update(game, (float)frameDt); }
        else if (game.mode != Mode::Paused) { simStep((float)frameDt); witness_record_frame(frameDt); }
      }
      simT1 = SDL_GetPerformanceCounter();
    } else
#endif
    {
    Input in = view.input(game);
    // one-shot presses must survive render frames in which no sim step runs (on 120/144 Hz displays, about half
    // of all frames): the view clears them every frame, so carry them until a step actually consumes them
    static Input latched;
    in.attack |= latched.attack; in.bow |= latched.bow; in.spell |= latched.spell; in.roll |= latched.roll;
    in.interact |= latched.interact; in.potion |= latched.potion; in.swapSpell |= latched.swapSpell;
    // (M9) the quick-cast press and the sneak toggle are one-shots too; a dual-cast's release (castDual with castHold
    // off) must reach a step as well
    if (in.castSlot < 0) in.castSlot = latched.castSlot;
    in.sneak |= latched.sneak;
    if (!in.castHold) in.castDual |= latched.castDual && !latched.castHold;
    latched = Input();
    simT0 = timingMark();
    if (game.mode == Mode::Play) {
      acc += frameDt;
      int steps = 0;
      bool first = true;
      while (acc >= SIM_DT && steps < 8) {
        game.update(SIM_DT, in);
        if (first) { in.attack = in.bow = in.spell = in.roll = in.interact = in.potion = in.swapSpell = false; in.castSlot = -1; in.sneak = false; first = false; }
        acc -= SIM_DT; steps++;
      }
      if (steps == 0) { latched = in; latched.move = Vec2(); }
      if (steps == 8) acc = 0;
      saveT += (float)frameDt;
      if (!noSave && saveT > 20.0f) { saveT = 0; writeSave(game); }
    } else {
      acc = 0;
      if (game.mode == Mode::Title) game.time += (float)frameDt;
      // (P2b B11) under the death screen the sim steps only the hero's fall (Game::update: stT, then it returns)
      else if (game.mode == Mode::Dead) game.update((float)frameDt, Input());
    }
    simT1 = SDL_GetPerformanceCounter();
    view.update(game, (float)frameDt);
    }
    const Uint64 updT1 = SDL_GetPerformanceCounter();
    view.draw(game, hasSave);
    {   // (P2a) the draw's own time (the --perf render baseline)
      const double dms = (double)(SDL_GetPerformanceCounter() - updT1) * 1000.0 / (double)freq;
      pacc.drawSum += dms;
      pacc.drawWorst = std::max(pacc.drawWorst, dms);
    }
    {
      static const bool timing = std::getenv("EMB_TIMING") != nullptr;
      auto ms = [&](Uint64 a, Uint64 b) { return (double)(b - a) * 1000.0 / (double)freq; };
      const bool slow = timing && ms(work0, SDL_GetPerformanceCounter()) > 15.0;   // (and any frame over 15 ms)
      const bool after = afterTravel > 0;
      if (after) afterTravel--;
      if (timing && (after || slow))
        std::printf("%s frame %d: pre %.2f ms, sim %.2f ms, view update %.2f ms, draw %.2f ms (npc awake %d)\n", after ? "after arrival" : "slow", 12 - afterTravel,
                    ms(work0, simT0), ms(simT0, simT1), ms(simT1, updT1), ms(updT1, SDL_GetPerformanceCounter()), game.perf.npcAwake);
    }
    {   // the frame's CPU work: everything before the present (vsync waits) and before a test screenshot (PNG writing)
      double workMs = (double)(SDL_GetPerformanceCounter() - work0) * 1000.0 / (double)freq;
      pacc.frames++;
      pacc.workSum += workMs;
      pacc.workWorst = std::max(pacc.workWorst, workMs);
      if (pacc.frames > 30) pacc.worstAll = std::max(pacc.worstAll, workMs);   // (not the first frames: start-up)
      if (game.world.sstats.walkShifts != walkShifts0) { pacc.shiftFrames++; pacc.worstShift = std::max(pacc.worstShift, workMs); pacc.afterShift = 60; }
      else if (pacc.afterShift > 0) { pacc.afterShift--; pacc.worstAfterShift = std::max(pacc.worstAfterShift, workMs); }
    }

    if (!pendingShot.empty()) {
      bool ok = pix.screenshot(pendingShot.c_str());
      std::printf("screenshot %s: %s\n", pendingShot.c_str(), ok ? "ok" : "FAILED");
      if (!ok) scriptFails++;
      pendingShot.clear();
    }
    if (shotPath && !scripted && !shotTaken && wall >= shotAfter) {
      shotTaken = pix.screenshot(shotPath);
      std::printf("screenshot %s: %s\n", shotPath, shotTaken ? "ok" : "FAILED");
      running = false;
    }
    pix.end();
    std::fflush(stdout);
  };
#ifdef EMB_WITNESS
  if (witness) {
    // ---- Witness (engine/witness.h; github.com/Josh17400/witness): a harness launched this run with WITNESS_ADDR set
    // (or --witness) and drives it over a local socket. The game steps only when told to, at the dt it is given (no
    // clock, no vsync); input goes through the script driver's paths (pushed SDL events, View::scriptHold); the mixer is
    // rendered into the capture buffer each step (no audio device); a frame is drawn only for a screenshot (the window
    // is hidden). `call` runs any script command, built in (runCmd) or registered (rpg/view/script_api.h); one that takes
    // time (walkto, talkto, a registered command that waits) carries on over the next steps (state.script.busy) and a
    // later failure is logged as an error (it fails the test).
    std::vector<SDL_Scancode> wHeld;   // keys the harness holds down
    std::vector<Lift> wFingers;        // fingers the harness holds down (witness finger n is SDL finger n + 1)
    constexpr Sint32 kPauseCode = 0x57504155;   // SDL_EVENT_USER code: a recording's "pause" action (the lost focus)
    std::vector<float> wMix;
    std::vector<uint8_t> wShot;
    auto drawFrame = [&]() { screen::frame(pix); view.settleBakes(); view.draw(game, false); };
    auto pushMouse = [&](SDL_EventType type, float lx, float ly, int button) {
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
    };
    double mouseX = 0, mouseY = 0;
    std::vector<int> wButtons;   // mouse buttons the harness holds down (0 left, 1 middle, 2 right)
    // the queued events, through the view (the hidden window's own focus / quit events are never a pause or a quit)
    auto drainEvents = [&]() {
      SDL_Event ev;
      while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_FOCUS_LOST || ev.type == SDL_EVENT_WINDOW_HIDDEN ||
            ev.type == SDL_EVENT_WILL_ENTER_BACKGROUND) continue;
        // a replayed "pause": the recorded session lost the focus here and paused, as focusLost does
        if (ev.type == SDL_EVENT_USER && ev.user.code == kPauseCode) { if (game.mode == Mode::Play) game.mode = Mode::Paused; continue; }
        view.event(ev, game);
      }
    };
    // `statematch` / `shotmatch` (a test's determinism checks): the first run of a label in this process is kept, every
    // later run of it must be identical (never reset: tests compare across resets)
    std::map<std::string, std::string> wMatch;
    uint64_t wAudioHash = 1469598103934665603ull;   // (audiomatch) FNV-1a of every sample mixed since the reset
    long long wAudioN = 0;

    witness::Handlers h;
    h.reset = [&](long long s, const char* scene, witness::Value params) {
      // nothing of the last run may leak into this one. Whatever it held is let go first, through the view as a
      // player's release would be: keys (held by the harness or a script), fingers still down (a tap's or a swipe's
      // pending lift), mouse buttons. Then the walk, a waiting command and every queued event go
      for (SDL_Scancode sc : wHeld) { view.scriptHold(sc, false); pushKey(sc, false); }
      for (const Held& hd : held) { view.scriptHold(hd.sc, false); pushKey(hd.sc, false); }
      for (const Lift& l : lifts) pushFinger(SDL_EVENT_FINGER_UP, l.id, l.x, l.y);
      for (const Lift& l : wFingers) pushFinger(SDL_EVENT_FINGER_UP, l.id, l.x, l.y);
      for (int b : wButtons) pushMouse(SDL_EVENT_MOUSE_BUTTON_UP, (float)mouseX, (float)mouseY, b);
      setMove(false, false, false, false);
      drainEvents();
      wHeld.clear(); held.clear(); lifts.clear(); drags.clear(); wButtons.clear(); wFingers.clear();
      walk = Walk(); extRun = ExtRun();
      heard.clear(); pendingShot.clear(); lastFail.clear();
      scriptFails = 0; scriptT = 0;
      SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
      // a freshly made view and mixer: no clock (t_, the particles' seed), effect, notice, banner, fade, menu cursor,
      // texture cache, voice or piece of music of the last run carries over (View::witnessRelease)
      view.witnessRelease();
      view.~View();
      audio.~Audio();
      // (P2a) the screen and the rig this run asks for, before the view is rebuilt (VISION_PLAN 15.36 item 10):
      //   window [w, h] device px (the hidden window resized; the fit runs unpinned), safe [l, t, r, b] device px,
      //   screen "fill" | "pixel", zoom -1..2 or "keep" (the last run's, as a restart reads the settings file),
      //   rig 1 | 2, female (the hero's body base)
      {
        int ww = 0, wh = 0, safe[4] = {0, 0, 0, 0}, mode = -1, zoom = 0;
        bool hasSafe = false, keep = false;
        if (params["window"].size() == 2) { ww = (int)params["window"][0].integer(); wh = (int)params["window"][1].integer(); }
        if (params["safe"].size() == 4) { hasSafe = true; for (int k = 0; k < 4; k++) safe[k] = (int)params["safe"][k].integer(); }
        if (params["screen"].exists()) mode = std::string(params["screen"].str("fill")) == "pixel" ? (int)screen::PixelPerfect : (int)screen::Fill;
        // (P2b integration, test only) defaults for a run whose harness cannot pass params (witness play batch testers:
        // a --params JSON in their start line is denied): EMB_WPLAY_WINDOW "2556x1179", EMB_WPLAY_SAFE "l,t,r,b",
        // EMB_WPLAY_SCREEN fill|pixel, EMB_WPLAY_RIG, EMB_WPLAY_HOUR, EMB_WPLAY_GOD; a param that is given wins
        if (const char* e = std::getenv("EMB_WPLAY_WINDOW"); e && params["window"].size() != 2) std::sscanf(e, "%dx%d", &ww, &wh);
        if (const char* e = std::getenv("EMB_WPLAY_SAFE"); e && !hasSafe)
          hasSafe = std::sscanf(e, "%d,%d,%d,%d", &safe[0], &safe[1], &safe[2], &safe[3]) == 4;
        if (const char* e = std::getenv("EMB_WPLAY_SCREEN"); e && !params["screen"].exists())
          mode = std::string(e) == "pixel" ? (int)screen::PixelPerfect : (int)screen::Fill;
        if (params["zoom"].exists()) {
          if (params["zoom"].type() == WITNESS_STRING) keep = std::string(params["zoom"].str("")) == "keep";
          else zoom = (int)params["zoom"].integer(0);
        }
        if ((hasSafe || mode >= 0) && ww <= 0) { ww = 960; wh = 540; }   // a safe area or a mode alone: the harness window, unpinned
        screen::witnessScreen(pix, ww, wh, hasSafe ? safe : nullptr, mode, zoom, keep);
        const char* envRig = std::getenv("EMB_WPLAY_RIG");
        art::setRigVersion((int)params["rig"].integer(envRig ? std::atoi(envRig) : 1));   // a fresh view follows: no cache carries the other rig's pixels
      }
      new (&audio) Audio();
      new (&view) View();
      view.init(pix, audio);
      view.witness = true;
      view.touchUI = touchUIStart;
      // Witness's reserved reset params (PROTOCOL.md reset): camera "hold" keeps the camera where the reset puts it for the
      // whole run (no follow, no shake: the `camhold` script command's flag, View::P2bHooks::camHold); hide [ids or type
      // names] leaves those actors undrawn (drawing only: View::wHide), so `measure figure` diffs an exact silhouette
      view.p2b.camHold = params["camera"].type() == WITNESS_STRING && std::string(params["camera"].str("")) == "hold";
      {
        const witness::Value hide = params["hide"];
        auto hideOne = [&](const witness::Value& e) {
          if (e.type() == WITNESS_STRING) view.wHide.types.push_back(e.str(""));
          else if (e.type() == WITNESS_NUMBER) view.wHide.ids.push_back((int)e.integer(-1));
        };
        if (hide.type() == WITNESS_ARRAY) for (size_t k = 0; k < hide.size(); k++) hideOne(hide.at(k));
        else hideOne(hide);   // (a lone id or type name)
      }
      mouseX = mouseY = 0;
      wAudioHash = 1469598103934665603ull;
      wAudioN = 0;
      startSeed = (uint64_t)s;
      // then a brand-new Game and the start the params ask for (applyStart: the same steps a recorded --play took)
      StartSpec sp;
      sp.god = params["god"].boolean() || (!params["god"].exists() && std::getenv("EMB_WPLAY_GOD"));
      sp.scene = scene && *scene ? scene : params["scene"].str("");
      sp.enter = params["enter"].boolean();
      if (params["hour"].exists()) sp.hour = (float)params["hour"].num();
      else if (const char* e = std::getenv("EMB_WPLAY_HOUR")) sp.hour = (float)std::atof(e);
      if (params["at"].size() == 2) { sp.atSet = true; sp.atX = (int32_t)params["at"][0].integer(); sp.atY = (int32_t)params["at"][1].integer(); }
      sp.talk = (int)params["talk"].integer(-1);
      sp.menu = (int)params["menu"].integer(-1);
      sp.fight = params["fight"].str("");
      sp.count = (int)params["count"].integer(0);
      sp.creator = params["creator"].boolean();
      sp.background = (int)params["background"].integer(0);
      sp.name = params["name"].str("");
      sp.ids = (int)params["ids"].integer(0);   // a recording's id counter at its start (absent: a fresh Game's)
      applyStart(startSeed, sp);
      if (params["female"].boolean()) { game.app.female = true; AnimOps::refreshHero(game); }   // (P2a) the woman's body base
    };
    h.input = [&](const witness_event& e) {
      if (e.type == WITNESS_EV_KEY && e.down >= 0) {
        const SDL_Scancode sc = keyFromName(e.name);
        if (sc == SDL_SCANCODE_UNKNOWN) { witness_log(WITNESS_WARN, "witness: unknown key '%s'", e.name); return; }
        const bool down = e.down == 1;
        pushKey(sc, down);          // one-shot presses (View::event)
        view.scriptHold(sc, down);  // and the held state (View::input)
        wHeld.erase(std::remove(wHeld.begin(), wHeld.end(), sc), wHeld.end());
        if (down) wHeld.push_back(sc);
      } else if (e.type == WITNESS_EV_MOUSE) {
        if (e.has_pos) { mouseX = e.x; mouseY = e.y; pushMouse(SDL_EVENT_MOUSE_MOTION, (float)mouseX, (float)mouseY, 0); }
        if (e.down >= 0) {
          const int b = e.button == 1 || e.button == 2 ? e.button : 0;
          pushMouse(e.down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP, (float)mouseX, (float)mouseY, b);
          wButtons.erase(std::remove(wButtons.begin(), wButtons.end(), b), wButtons.end());
          if (e.down) wButtons.push_back(b);
        }
      } else if (e.type == WITNESS_EV_WHEEL) {
        SDL_Event ev;
        SDL_zero(ev);
        ev.type = SDL_EVENT_MOUSE_WHEEL;
        ev.wheel.windowID = SDL_GetWindowID(pix.window());
        ev.wheel.x = (float)(-e.dx / 100.0); ev.wheel.y = (float)(-e.dy / 100.0);   // the protocol's + is down
        pushEvent(ev);
      } else if (e.type == WITNESS_EV_TOUCH) {
        // a finger, through the touch path SDL delivers one by (View::event's FINGER_* cases; hello.touch: no mouse copy).
        // cancel (the system ended the touch) arrives as SDL's CANCELED: the finger is lifted, but it is never a tap
        const SDL_FingerID id = (SDL_FingerID)e.id + 1;   // (SDL never uses finger id 0)
        const float x = (float)e.x, y = (float)e.y;
        const SDL_EventType t = e.phase == WITNESS_TOUCH_DOWN ? SDL_EVENT_FINGER_DOWN : e.phase == WITNESS_TOUCH_UP ? SDL_EVENT_FINGER_UP
                              : e.phase == WITNESS_TOUCH_CANCEL ? SDL_EVENT_FINGER_CANCELED : SDL_EVENT_FINGER_MOTION;
        pushFinger(t, id, x, y, e.pressure > 0 ? (float)e.pressure : 1.0f);
        wFingers.erase(std::remove_if(wFingers.begin(), wFingers.end(), [&](const Lift& l) { return l.id == id; }), wFingers.end());
        if (t == SDL_EVENT_FINGER_DOWN || t == SDL_EVENT_FINGER_MOTION) wFingers.push_back({id, x, y, 0.0f});
      } else if (e.type == WITNESS_EV_ACTION && std::strcmp(e.name, "pause") == 0) {
        SDL_Event ev;   // in the event stream, so it lands between the inputs around it exactly as it did when recorded
        SDL_zero(ev);
        ev.type = SDL_EVENT_USER;
        ev.user.code = kPauseCode;
        pushEvent(ev);
      } else {
        witness_log(WITNESS_WARN, "witness: input type %d is not used by Embervale (keys, mouse, wheel, touch and the "
                    "\"pause\" action are)", (int)e.type);
      }
    };
    h.step = [&](double dt) {
      const float fdt = (float)dt;
      scriptStep(fdt);   // held keys, a walk or a registered command under way (script time)
      drainEvents();
      screen::frame(pix);   // (P2a) a ZOOM step re-presents the layers now: the camera and the NPC LOD frame the world view in use
      view.wantSave = false;
      if (view.wantsQuit) { view.wantsQuit = false; game.mode = Mode::Title; }
      if (view.wantNewGame || view.wantContinue) { view.wantNewGame = view.wantContinue = false; startNew(startSeed); }
      game.sleepHalfW = std::max(22.0f, view.viewW() * 0.5f / TILE + 4.0f);   // as frame(): the NPC LOD follows the world view
      game.sleepHalfH = std::max(13.0f, view.viewH() * 0.5f / TILE + 4.0f);
      simStep(fdt);
      const int n = witness_audio_frames_for_step(dt);   // exactly the samples this step covers
      wMix.resize((size_t)n);
      for (int k = 0; k < n; k += 1024) audio.render(wMix.data() + k, std::min(1024, n - k));
      witness_audio_write(wMix.data(), n);
      for (int k = 0; k < n; k++) {
        uint32_t u;
        std::memcpy(&u, &wMix[(size_t)k], 4);
        for (int b = 0; b < 4; b++) { wAudioHash ^= (u >> (8 * b)) & 255u; wAudioHash *= 1099511628211ull; }
      }
      wAudioN += n;
      if (watch) { drawFrame(); pix.end(); }   // (watch mode) the step on screen (drawing never feeds the sim)
    };
    h.snapshot = [&](witness::Json& j) {
      WitnessExtra x = wStart;
      x.scriptBusy = walk.on || extRun.e != nullptr;
      x.scriptFails = scriptFails;
      x.lastFail = lastFail;
      x.savesWritten = g_savesWritten;
      x.saveEnabled = !noSave;
      witnessState(j, game, view, x);
    };
    h.screenshot = [&](witness_image& img) -> bool {
      drawFrame();
      int w = 0, hh = 0;
      // (P2b fix round 1, test only) EMB_WPLAY_SHOT "world:K": a playtester's shot is the world layer at world px (one
      // pixel per art px, the UI left out), each pixel repeated K x K (1-4), so people read at 32 x K px instead of the
      // UI canvas's ~20 px (the phone window's UI scale samples the world below its own resolution)
      int worldK = 0;
      if (const char* e = std::getenv("EMB_WPLAY_SHOT"); e && std::strncmp(e, "world", 5) == 0)
        worldK = std::clamp(e[5] == ':' ? std::atoi(e + 6) : 1, 1, 4);
      bool ok;
      if (worldK > 0) {
        std::vector<uint8_t> src;
        ok = pix.readWorld(src, w, hh);
        if (ok && worldK > 1) {
          const int W = w * worldK, H = hh * worldK;
          wShot.assign((size_t)W * H * 4, 0);
          for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
              std::memcpy(&wShot[((size_t)y * W + x) * 4], &src[((size_t)(y / worldK) * w + x / worldK) * 4], 4);
          w = W; hh = H;
        } else if (ok) wShot.swap(src);
      } else ok = pix.readCanvas(wShot, w, hh);
      pix.end();
      if (!ok) { witness::fail(std::string("screenshot: ") + SDL_GetError()); return false; }
      img.rgba = wShot.data(); img.width = w; img.height = hh;
      return true;
    };
    h.call = [&](const char* name, witness::Value args, witness::Json& result) {
      ScriptCmd c;
      c.a.push_back(name);
      for (auto& ch : c.a[0]) ch = (char)std::tolower((unsigned char)ch);
      for (size_t i = 0; i < args.size(); i++) {
        const witness::Value v = args.at(i);
        if (v.type() == WITNESS_STRING) { c.a.push_back(v.str()); continue; }
        char b[40];
        const double d = v.num();
        if (d == std::floor(d) && std::fabs(d) < 1e15) std::snprintf(b, sizeof b, "%lld", (long long)d);
        else std::snprintf(b, sizeof b, "%g", d);
        c.a.push_back(b);
      }
      for (size_t k = 0; k < c.a.size(); k++) { if (k) c.text += ' '; c.text += c.a[k]; }
      if (c.a[0] == "quit") { witness::fail("quit: the harness ends the run (its quit command)"); return; }
      if (c.a[0] == "statematch" || c.a[0] == "shotmatch" || c.a[0] == "audiomatch") {
        // the determinism checks: this moment's full state (the snapshot plus every actor exactly: witnessDigest), this
        // frame's canvas (RGBA) or every sample mixed since the reset, against the first run of the label in this process
        const bool shot = c.a[0] == "shotmatch";
        if (c.a.size() < 2) { witness::fail(c.a[0] + ": a label (the runs that must match share it)"); return; }
        std::string now;
        if (shot) {
          drawFrame();
          int w = 0, hh = 0;
          const bool ok = pix.readCanvas(wShot, w, hh);
          pix.end();
          if (!ok) { witness::fail(std::string("shotmatch: ") + SDL_GetError()); return; }
          now.assign((const char*)wShot.data(), wShot.size());
        } else if (c.a[0] == "audiomatch") {
          char ab[48];
          std::snprintf(ab, sizeof ab, "%016llx over %lld samples", (unsigned long long)wAudioHash, wAudioN);
          now = ab;
        } else {
          now = witnessJsonString([&](witness::Json& j) { h.snapshot(j); }) + "\n" + witnessDigest(game);
        }
        uint64_t hash = 1469598103934665603ull;   // FNV-1a 64
        for (unsigned char ch : now) { hash ^= ch; hash *= 1099511628211ull; }
        const std::string key = c.a[0] + ":" + c.a[1];
        auto it = wMatch.find(key);
        const bool first = it == wMatch.end();
        if (first) wMatch[key] = now;
        else if (it->second != now) {
          size_t at = 0;
          while (at < now.size() && at < it->second.size() && now[at] == it->second[at]) at++;
          std::string msg = c.a[0] + " " + c.a[1] + ": this run differs from the first one";
          if (shot) {
            const size_t px = at / 4;
            msg += " (first differing pixel x " + std::to_string(px % (size_t)std::max(1, Pix::W)) + ", y " + std::to_string(px / (size_t)std::max(1, Pix::W)) + ")";
          } else {
            const size_t from = at > 60 ? at - 60 : 0;
            msg += " at char " + std::to_string(at) + ": first ..." + it->second.substr(from, 140) + "... now ..." + now.substr(from, 140) + "...";
          }
          witness::fail(msg);
          return;
        }
        char hb[24];
        std::snprintf(hb, sizeof hb, "%016llx", (unsigned long long)hash);
        result.begin_object().field("label", c.a[1]).field("first", first).field("hash", std::string(hb)).end_object();
        return;
      }
      if (walk.on || extRun.e) { witness::fail("busy: the last call is still running (step until state.script.busy is false)"); return; }
      const int fails0 = scriptFails;
      witnessInCall = true;
      runCmd(c);
      witnessInCall = false;
      if (scriptFails > fails0) { witness::fail(lastFail); return; }
      if (!pendingShot.empty()) {   // `shot file.png`: this frame, to a file
        drawFrame();
        const bool ok = pix.screenshot(pendingShot.c_str());
        pix.end();
        const std::string path = pendingShot;
        pendingShot.clear();
        if (!ok) { witness::fail("shot " + path + ": " + SDL_GetError()); return; }
      }
      result.begin_object().field("busy", walk.on || extRun.e != nullptr).field("mode", (int)game.mode).end_object();
    };

    // the calls: the built-in script commands, then every registered one (the registry's expect checks ride on `expect`)
    static const struct { const char* name; const char* args; const char* help; } builtin[] = {
        {"goto", "city|town|village|cave|ruin|camp|lair|shrine|capital|market [enter]", "fast travel to the nearest such place"},
        {"walkto", "inn|shop|innkeeper|giver|exit|NAME|x y [timeout]", "autopilot (takes steps: state.script.busy)"},
        {"talkto", "NAME|role [timeout]", "walk to someone and talk to them"},
        {"talk", "0|1|2", "the innkeeper: greeting, job offer, shop"},
        {"choose", "words", "choose the open dialogue's option containing the words"},
        {"fight", "wolf|boar|bear|...|dragon [n]", "spawn monsters nearby (gives the test kit if unarmed)"},
        {"enter", "inn|shop|smithy|temple|house|... [floor] [n]", "step into the n-th nearest building of that type"},
        {"floor", "n", "inside a building: go to that floor"},
        {"at", "gx gy", "teleport to a global tile"},
        {"hour", "h", "set the time of day"},
        {"god", "[0|1]", "invulnerability"},
        {"kit", "", "the test kit: sword, bow, arrows, potions"},
        {"gear", "band [weapon]", "a full armour set of band 1..7"},
        {"strip", "", "take off every piece of equipment"},
        {"menu", "tab", "open the menu on a tab"},
        {"newgame", "", "a fresh world (the reset seed)"},
        {"key", "name", "press and release a key"},
        {"hold", "key secs", "hold a key"},
        {"walk", "east|west|north|south secs", "hold a direction"},
        {"tap", "x y", "a touch tap at logical coordinates"},
        {"click", "x y", "a left click at logical coordinates"},
        {"chestgo", "[hoard|explore|chest]", "stand beside the nearest closed chest"},
        {"pos", "[map]", "print where the player is (game output)"},
        {"shot", "file.png", "save this frame to a file"},
        {"expect", "mode|gold|quest|heard|option|inside|... args", "a script check (fails the call)"},
        {"statematch", "label", "the full state (every actor exactly) must equal the first run of this label in this process"},
        {"shotmatch", "label", "this frame's canvas must equal the first run of this label in this process"},
        {"audiomatch", "label", "every sample mixed since the reset must equal the first run of this label in this process"}};
    std::vector<witness_call_info> calls;
    for (const auto& b : builtin) calls.push_back({b.name, b.args, b.help});
    for (const ScriptExt& e : scriptExts())
      if (std::strncmp(e.name, "expect:", 7) != 0) calls.push_back({e.name, nullptr, e.help});
    static const char* const keys[] = {"w", "a", "s", "d", "up", "down", "left", "right", "space", "e", "j", "k", "l", "z", "q", "f", "r",
                                       "i", "m", "tab", "escape", "enter", "shift", "alt", "1", "2", "3", "4",
                                       "minus", "equals"};   // (P2a) ZOOM out / in
    witness_hello hello = witness_hello();
    hello.name = "Embervale";
    hello.engine = "custom C++/SDL3";
    hello.language = "c++";
    hello.fps = 60;
    screen::frame(pix);   // the canvas size (Pix::W x Pix::H) the harness sees
    hello.width = Pix::W;
    hello.height = Pix::H;
    hello.calls = calls.data();
    hello.call_count = (int)calls.size();
    hello.keys = keys;
    hello.key_count = (int)(sizeof keys / sizeof keys[0]);
    hello.audio_sample_rate = Audio::kSampleRate;
    hello.audio_channels = 1;
    hello.touch = 1;   // touch events reach the touch path; Witness sends no mouse copy of finger 0
    // (Witness 0.6) every sound the game starts is a protocol "sound" event (its name and volume, stamped with the step's
    // frame), so `witness listen` / `effect` can name sounds and match them to their causes. Nothing else changes
    Audio::setPlayListener([](const char* name, float vol) { witness_sound_ex(name, vol, 0, 0.0, 0.0); });
    if (witness::connect(hello, h)) {
#ifdef __EMSCRIPTEN__
      // (web, EMBERVALE_WITNESS_WEB) the page's ?witness=host:port: witness_run() hands control back to the browser and
      // keeps the runtime alive (emscripten_exit_with_live_runtime, like simulate_infinite_loop: main's frame and
      // everything the handlers reference stay alive, nothing after this line runs). The game then steps only inside the
      // harness's commands (witness_emscripten.js calls in): emscripten_set_main_loop is never started in witness mode
      witness_run();
#else
      // (watch mode) between commands the window's events keep being pumped about every 30 ms, so the OS never marks
      // it "not responding" and it can be moved; queued input stays queued for the next step, real input is filtered
      if (watch) while (witness_poll(30)) SDL_PumpEvents();
      else witness_run();
#endif
    } else std::printf("witness: could not connect to the harness\n");
    witness_shutdown();
  } else
#endif
  {
#ifdef __EMSCRIPTEN__
    // simulate_infinite_loop keeps main's stack frame (and everything the lambda references) alive
    static std::function<void()>* loopFn = new std::function<void()>(frame);
    emscripten_set_main_loop([] { (*loopFn)(); }, 0, 1);
#else
    while (running) frame();
#endif
  }

#ifdef EMB_WITNESS
  if (recordSync && witness_recording()) {
    // the state the recording ended in, next to it (<file>.state.json, the harness's snapshot): a replay of the file
    // can be checked against it for an exact match (`witness replays` lists only the .jsonl)
    WitnessExtra x = wStart;
    x.scriptBusy = false;   // (a harness replay has no script of its own)
    x.savesWritten = g_savesWritten;
    x.saveEnabled = !noSave;
    const std::string st = witnessJsonString([&](witness::Json& j) { witnessState(j, game, view, x); });
    if (FILE* f = std::fopen((std::string(recordEnv) + ".state.json").c_str(), "wb")) {
      std::fwrite(st.data(), 1, st.size(), f);
      std::fclose(f);
    }
  }
#endif
  if (!noSave && game.mode != Mode::Title) writeSave(game);
  view.shutdown();
  audio.shutdown();
  pix.shutdown();
  SDL_Quit();
#ifdef EMB_WITNESS
  witness_record_stop();
#endif
  if (perf || scripted) printSummary();
  if (scripted) {
    std::printf("script: %d failure(s)\n", scriptFails);
    return scriptFails ? 3 : 0;
  }
  return 0;
}
