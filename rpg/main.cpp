// EMBERVALE - SDL3 platform layer: window, events, fixed-timestep loop, autosave.
//   embervale.exe                                   play (autosaves; title offers Continue)
//   embervale.exe --play --seed 7                   skip the title, fresh world (never touches your save)
//   embervale.exe --play --goto city|town|cave|ruin|camp|lair [--enter] [--hour 22] [--menu 0-4]
//                 --shot out.png --after 2           screenshot after N seconds, then quit
//   embervale.exe --script file.txt [--seed 7] [--play]   drive the game from a timed script (test only, never saves)
//
// Script format: one command per line, "<time> <command> [args]". Times are seconds of script time; "+0.5" means
// 0.5 s after the previous line. '#' starts a comment. Script time stops while a walkto runs, so later lines keep
// their spacing however long the walk takes. Input is injected through the same paths real input uses: key
// presses become SDL key events (View::event), held keys are read by View::input, taps become SDL touch events.
//   seed 7                    world seed (a header line, no time; --seed on the command line wins)
//   worldgen 6                world-generator version (a header line; default WORLDGEN_LATEST). Scripts that rely on
//                             facts of one world (names, coordinates) pin the generator they were written against
//   0.5 key E                 press and release a key (SDL key names: E, Return, Escape, Tab, Space, Up, Left Shift...)
//   2.0 hold W 1.5            hold a key down for 1.5 s
//   3 tap 424 222             touch tap at logical (480x270) coordinates
//   3 click 240 135           left mouse click at logical coordinates
//   4 shot out.png            screenshot of this frame
//   5 walkto inn              autopilot over the tile grid, steering with held WASD: inn|shop|smithy|temple|keep|tower|
//                             house (walks in the door), innkeeper|merchant|smith|priest|jarl|guard|villager|mage (until
//                             they can be talked to), exit (walks out of a building or dungeon), or "x y" tiles.
//                             a person by NAME (walkto VIGRIMA: until they can be talked to). Inside a building:
//                             upstairs|downstairs (walks onto the stairs; done when the floor changed).
//                             An optional trailing number is the timeout in seconds (default 40).
//   6 expect mode shop        check state: mode title|play|dialogue|menu|shop|levelup|dead|paused|creator, inside 0|1
//   6 newgame | goto ruin [enter] | talk 0|1|2 | fight wolf [n] | god [0|1] | hour 22 | menu 2 | log text
//   6 talkto VIGRIMA [30]     walkto a person and talk to them the moment they are in reach (E in that same frame)
//   6 kit                     give and equip the test kit (sword, bow, arrows, potions); fight does this when unarmed
//   6 gear 3 [weapon]         give and equip a full set of armour band 3 (1 leather, 2 iron, 3 steel, 4 gilded, 5 jade,
//                             6 obsidian, 7 emberforged): body, helmet, gloves, boots, cloak, shield, amulet, ring, plus a
//                             weapon of that tier (sword|axe|mace|dagger|greatsword, default sword) and a bow for the back
//   6 strip                   take every piece of equipment off (shirt and trousers only)
//   6 enter inn [floor] [n]   (M0b) step straight into the n-th nearest (default 0) inn|shop|smithy|temple|keep|tower|
//                             house|stonehouse|farmhouse|hut, on that floor (default 0); fails if it has no such floor
//   6 floor 1                 (M0b) inside a building: go to that floor (arriving by its stairs)
//   6 expect floor 1          the floor the player is on (0 ground)
//   6 expect name ASTRID      the player's name (the character creator); also: expect background 3, expect slot armor 1|0
//   6 expect quest active BOUNTY: X   a quest whose title contains the words is active|complete|done (or none exists)
//   6 expect tracked CULL THE       the tracked quest's title contains the words
//   6 expect heard NOT YOUR TARGET  some notice since the script began contained the words
//   6 expect option COLLECT BOUNTY  the open dialogue offers an option containing the words (expect text: its text)
//   6 choose RENT A ROOM      (M0b) choose the open dialogue's option whose label contains the words
// New Game from the title opens the character creator (Mode::Creator); --play and the newgame command skip it.
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
#include "rpg/view/view.h"

namespace {
std::string g_savePath;
int g_worldgen = WORLDGEN_LATEST;   // test runs: the world-generator version (script header "worldgen N", --worldgen N)

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

// ---------------------------------------------------------------- test scripts
struct ScriptCmd {
  float t = 0;
  std::vector<std::string> a;   // a[0] is the command
  int line = 0;
  std::string text;
};

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
    if (tok[0] == "worldgen" && tok.size() >= 2) { g_worldgen = std::clamp(std::atoi(tok[1].c_str()), (int)WORLDGEN_V1, (int)WORLDGEN_LATEST); continue; }
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
    else if (!std::strcmp(argv[i], "--worldgen") && i + 1 < argc) g_worldgen = std::clamp(std::atoi(argv[++i]), (int)WORLDGEN_V1, (int)WORLDGEN_LATEST);
  }
  std::vector<ScriptCmd> script;
  if (scriptPath) {
    if (!loadScript(scriptPath, script, seed)) { std::printf("script %s: cannot open\n", scriptPath); return 2; }
    std::printf("script %s: %zu commands, seed %llu\n", scriptPath, script.size(), (unsigned long long)seed);
  }
  const bool scripted = scriptPath != nullptr;
  const bool noSave = play || shotPath || seed != 0 || scripted;   // test runs never touch the player's save

  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
  SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) { SDL_Log("SDL_Init failed: %s", SDL_GetError()); return 1; }
#ifdef __EMSCRIPTEN__
  g_savePath = "/save/save.bin";
#else
  if (char* pref = SDL_GetPrefPath("Josh17400", "Embervale")) { g_savePath = std::string(pref) + "save.bin"; SDL_free(pref); }
#endif
  if (scripted) g_savePath.clear();   // belt and braces: a script can never read or write the real save

  Pix pix;
  if (!pix.init("EMBERVALE", 1440, 810, vsync)) return 1;
  static Audio audio;   // ~175 KB: keep it off the stack
  audio.init();

  uint64_t startSeed = seed ? seed : (uint64_t)SDL_GetTicks() * 2654435761ull + 12345;
  Game game(startSeed);
  game.newGame(startSeed, g_worldgen);   // title screen drifts over this world
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
    game.newGame(s, g_worldgen);
    game.mode = Mode::Play;
    view.snap(game);
    audio.play(Sfx::QuestStart);
  };
  // test helpers shared by the command-line flags and the script commands
  auto doGoto = [&](const std::string& w, bool enterIt) {
    SiteType want = SiteType::City;
    if (w == "town") want = SiteType::Town; else if (w == "village") want = SiteType::Village; else if (w == "cave") want = SiteType::Cave;
    else if (w == "ruin") want = SiteType::Ruin; else if (w == "camp") want = SiteType::BanditCamp; else if (w == "lair") want = SiteType::DragonLair;
    else if (w == "shrine") want = SiteType::Shrine;
    int si = game.world.nearestSite(game.world.sites[game.world.startSite].ex, game.world.sites[game.world.startSite].ey, want);
    if (si < 0) return;
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
          for (size_t o = 0; o < game.dlg.opts.size(); o++) if (game.dlg.opts[o].label.find(want) != std::string::npos) { game.dialogueChoose((int)o); break; }
        }
        break;
      }
  };
  // test helper: a full armour set of one band (1 leather .. 7 emberforged), equipped, plus a weapon and a bow
  auto giveGear = [&](int band, const std::string& weaponName) {
    band = std::clamp(band, 1, 7);
    int tier = band <= 2 ? 0 : band - 2;
    Rng r(game.seed * 31 + (uint64_t)band);
    auto wear = [&](Item it) {
      game.inv.push_back(it);
      int idx = (int)game.inv.size() - 1;
      int* e = game.equipSlot(it.kind);
      if (e && *e != idx) game.useItem(idx);
    };
    std::string pre = band == 1 ? "LEATHER " : (band == 2 ? "IRON " : std::string(tierName(tier)) + " ");
    static const char* suffix[] = {"ARMOR", "HELMET", "GAUNTLETS", "BOOTS", "", "SHIELD"};
    const ItemKind kinds[] = {ItemKind::Armor, ItemKind::Helmet, ItemKind::Gloves, ItemKind::Boots, ItemKind::Cloak, ItemKind::Shield};
    for (int k = 0; k < 6; k++) {
      Item it = makeArmor(r, 1 + tier * 5, kinds[k]);
      it.tier = (uint8_t)tier;
      it.ench = Ench::None; it.enchPow = 0; it.rarity = Rarity::Common;
      if (kinds[k] != ItemKind::Cloak) {
        it.name = pre + suffix[k];
        it.tint = band == 1 ? rgba(150, 100, 62) : tierTint(tier);
      }
      wear(it);
    }
    static const char* wn[] = {"sword", "axe", "mace", "dagger", "greatsword"};
    int wt = 0;
    for (int i = 0; i < 5; i++) if (weaponName == wn[i]) wt = i;
    Item w = makeWeapon(r, 1 + tier * 5, wt, false);
    w.tier = (uint8_t)tier; w.tint = tierTint(tier);
    wear(w);
    wear(makeBow(r, 1 + tier * 5));
    Item j = makeJewel(r, 5); j.kind = ItemKind::Amulet; j.icon = art::Icon::Amulet; wear(j);
    Item ring = makeJewel(r, 5); ring.kind = ItemKind::Ring; ring.icon = art::Icon::Ring; wear(ring);
  };
  auto doFight = [&](const std::string& name, int n) {
    if (game.eqWeapon < 0) game.debugKit();   // the real start is shirt-only (M0): fights get the test kit
    static const char* names[] = {"wolf", "boar", "bear", "slime", "spider", "bat", "skeleton", "draugr", "goblin", "troll", "wraith", "mudcrab", "icewolf", "frostspider", "sandworm", "dragon"};
    for (int m = 0; m < (int)art::Monster::COUNT && m < (int)(sizeof(names) / sizeof(names[0])); m++)
      if (name == names[m]) game.debugSpawn((art::Monster)m, n > 0 ? n : (m == (int)art::Monster::Dragon ? 1 : 3), 60);
  };

  if (play) {
    startNew(startSeed);
    game.godMode = god;
    if (hourSet >= 0) game.hour = hourSet;
    if (gotoWhat) doGoto(gotoWhat, enter);
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
  std::vector<std::string> heard;   // every notice (Game::say) shown since the script began, for "expect heard"
  std::string pendingShot;
  struct Held { SDL_Scancode sc; float until; };
  std::vector<Held> held;
  struct Lift { SDL_FingerID id; float x, y, at; };
  std::vector<Lift> lifts;
  SDL_FingerID nextFinger = 101;
  struct Walk {
    bool on = false;
    std::string what;
    int kind = 0;            // 0 tile, 1 building door, 2 actor, 3 exit, 4 stairs (M0b)
    int floor0 = 0;          // stairs: the floor the walk started on
    int tx = 0, ty = 0, actorId = -1, bldg = -1;
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
    SDL_PushEvent(&ev);
  };
  auto toWindow = [&](float lx, float ly, float& wx, float& wy) { SDL_RenderCoordinatesToWindow(pix.renderer(), lx, ly, &wx, &wy); };
  auto pushFinger = [&](SDL_EventType type, SDL_FingerID id, float lx, float ly) {
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
    ev.tfinger.pressure = 1;
    SDL_PushEvent(&ev);
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
    SDL_PushEvent(&ev);
  };
  auto fail = [&](int line, const std::string& msg) {
    std::printf("script FAIL (line %d): %s\n", line, msg.c_str());
    scriptFails++;
  };
  auto plTile = [&](int& x, int& y) { x = (int)std::floor(game.pl().p.x / TILE); y = (int)std::floor((game.pl().p.y - 2) / TILE); };
  auto findActorIdx = [&](int id) { for (size_t k = 1; k < game.actors.size(); k++) if (game.actors[k].id == id) return (int)k; return -1; };

  auto startWalk = [&](const ScriptCmd& c) {
    walk = Walk();
    walk.on = true;
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
        {"keep", art::Building::Keep}, {"tower", art::Building::Tower}, {"house", art::Building::House}, {"farmhouse", art::Building::Farmhouse}};
    static const struct { const char* n; Role r; } rt[] = {
        {"innkeeper", Role::Innkeeper}, {"merchant", Role::Merchant}, {"smith", Role::Smith}, {"priest", Role::Priest},
        {"jarl", Role::Jarl}, {"guard", Role::Guard}, {"villager", Role::Villager}, {"mage", Role::Mage}, {"farmer", Role::Farmer}};
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
  auto stepWalk = [&](float dt) {
    walk.t += dt;
    if (walk.t > walk.timeout) { endWalk(false, "timed out"); return; }
    if (game.mode != Mode::Play) { setMove(false, false, false, false); return; }   // a dialogue or menu is up: wait
    int px, py;
    plTile(px, py);
    // arrived?
    if (walk.kind == 1 && game.inside) { endWalk(true, ""); return; }
    if (walk.kind == 3 && !game.inside) { endWalk(true, ""); return; }
    if (walk.kind == 4 && game.subFloor != walk.floor0) { endWalk(true, ""); return; }
    if (walk.kind == 2) {
      int k = findActorIdx(walk.actorId);
      if (k < 0) { endWalk(false, "they left"); return; }
      // in reach: keep closing in a moment longer (a step from the edge of reach, a wandering innkeeper or a neighbour
      // who comes nearer turns the next key press into the wrong conversation), unless already close
      if (game.interactTarget() == walk.actorId) {
        walk.closeT += dt;
        if (len(game.actors[k].p - game.pl().p) < 20.0f || walk.closeT > 0.6f) { endWalk(true, ""); return; }
      }
      walk.tx = (int)std::floor(game.actors[k].p.x / TILE); walk.ty = (int)std::floor((game.actors[k].p.y - 2) / TILE);
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
  auto modeByName = [](const std::string& s, Mode& m) {
    static const char* n[] = {"title", "play", "dialogue", "menu", "shop", "levelup", "dead", "paused", "creator"};
    for (int i = 0; i < 9; i++) if (s == n[i]) { m = (Mode)i; return true; }
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
    } else if (op == "shot") {
      pendingShot = arg(1).empty() ? "shot.png" : arg(1);
    } else if (op == "quit") {
      running = false;
    } else if (op == "walkto") {
      startWalk(c);
    } else if (op == "talkto") {   // walkto a person, then talk in the very frame they come in reach (no one else steps in)
      startWalk(c);
      if (walk.on && walk.kind == 2) walk.talk = true;
      else if (walk.on) { walk.on = false; setMove(false, false, false, false); fail(c.line, "talkto needs a person"); }
    } else if (op == "newgame") {
      startNew(startSeed);
    } else if (op == "goto") {
      doGoto(arg(1), arg(2) == "enter");
    } else if (op == "enter") {   // M0b: enter <type> [floor] [nth]
      static const struct { const char* n; art::Building b; } et[] = {
          {"inn", art::Building::Inn}, {"shop", art::Building::Shop}, {"smithy", art::Building::Smithy}, {"temple", art::Building::Temple},
          {"keep", art::Building::Keep}, {"tower", art::Building::Tower}, {"house", art::Building::House},
          {"stonehouse", art::Building::StoneHouse}, {"farmhouse", art::Building::Farmhouse}, {"hut", art::Building::Hut}};
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
      if (want < 0) fail(c.line, "enter: unknown building type '" + arg(1) + "'");
      else if (nth >= (int)cand.size()) fail(c.line, "enter " + arg(1) + ": no such building with floor " + arg(2));
      else if (!game.debugEnterBuilding(cand[(size_t)nth].second, fl)) fail(c.line, "enter " + arg(1) + ": could not enter");
      else { game.mode = Mode::Play; view.snap(game); }
    } else if (op == "floor") {
      int fl = std::atoi(arg(1).c_str());
      if (!game.inside || game.subBldg < 0 || fl < 0 || fl >= game.world.over.bldgs[game.subBldg].floors()) fail(c.line, "floor " + arg(1) + ": not a floor here");
      else { game.changeFloor(fl); view.snap(game); }
    } else if (op == "choose") {   // M0b: choose the open dialogue's option whose label contains the words
      std::string t;
      for (size_t k = 1; k < c.a.size(); k++) { if (k > 1) t += ' '; t += c.a[k]; }
      int pickI = -1;
      for (size_t o = 0; o < game.dlg.opts.size() && pickI < 0; o++) if (game.dlg.opts[o].label.find(t) != std::string::npos) pickI = (int)o;
      if (game.mode != Mode::Dialogue || pickI < 0) fail(c.line, "choose: no dialogue option '" + t + "'");
      else { game.dialogueChoose(pickI); view.snap(game); }
    } else if (op == "talk") {
      doTalk(std::atoi(arg(1).c_str()));
    } else if (op == "fight") {
      doFight(arg(1), std::atoi(arg(2).c_str()));
    } else if (op == "kit") {
      game.debugKit();
    } else if (op == "gear") {
      giveGear(std::atoi(arg(1).c_str()), arg(2));
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
    } else if (op == "expect") {
      std::string what = arg(1), want = arg(2);
      if (what == "mode") {
        Mode m;
        static const char* n[] = {"title", "play", "dialogue", "menu", "shop", "levelup", "dead", "paused", "creator"};
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
          bool ok = false;
          for (const DlgOpt& o : game.dlg.opts) if (o.label.find(t) != std::string::npos) ok = true;
          if (game.mode != Mode::Dialogue || !ok) fail(c.line, "no dialogue option '" + t + "'" + (game.dlg.opts.empty() ? std::string() : " (first: " + game.dlg.opts[0].label + ")"));
        } else {   // text: the open dialogue's text contains these words
          std::string t = rest(2);
          if (game.mode != Mode::Dialogue || game.dlg.text.find(t) == std::string::npos) fail(c.line, "dialogue text lacks '" + t + "': " + game.dlg.text);
        }
      } else fail(c.line, "expect: unknown check '" + what + "'");
    } else {
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
    for (size_t i = 0; i < lifts.size();) {
      if (scriptT >= lifts[i].at) { pushFinger(SDL_EVENT_FINGER_UP, lifts[i].id, lifts[i].x, lifts[i].y); lifts.erase(lifts.begin() + i); }
      else i++;
    }
    if (walk.on) { stepWalk(dt); return; }   // script time stands still while walking
    while (scriptNext < script.size() && script[scriptNext].t <= scriptT && running) {
      runCmd(script[scriptNext++]);
      if (walk.on) return;
    }
    if (scriptNext >= script.size() && held.empty() && lifts.empty()) {
      if (scriptEndT < 0) scriptEndT = scriptT;
      if (scriptT - scriptEndT > 2.0f) { std::printf("script: end of script\n"); running = false; }
    }
    scriptT += dt;
  };

  // one frame of the game; desktop loops on it, the browser calls it once per animation frame
  auto frame = [&]() {
    if (scripted) {
      Uint64 nowS = SDL_GetPerformanceCounter();
      static Uint64 prevS = nowS;
      float dtS = (float)std::min(0.1, (double)(nowS - prevS) / (double)freq);
      prevS = nowS;
      scriptStep(dtS);
    }
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_EVENT_QUIT: running = false; break;
        case SDL_EVENT_WILL_ENTER_BACKGROUND: if (!noSave) writeSave(game); break;
        case SDL_EVENT_WINDOW_FOCUS_LOST: case SDL_EVENT_WINDOW_HIDDEN:
          if (!shotPath && !scripted && game.mode == Mode::Play) game.mode = Mode::Paused;
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
      else startNew(seed ? seed : (uint64_t)SDL_GetTicks() * 2654435761ull + 777);
    }
    if (view.wantNewGame) {
      view.wantNewGame = false;
      startNew(seed ? seed : (uint64_t)SDL_GetTicks() * 2654435761ull + 777);   // --seed / a script's seed: reproducible
      game.beginCreator();   // the character creator; the first save happens when it hands over to play
    }
    {   // the creator just finished: save the new character right away
      static Mode prevMode = Mode::Title;
      if (prevMode == Mode::Creator && game.mode == Mode::Play) {
        if (!noSave) writeSave(game);
        hasSave = !noSave;
        view.snap(game);
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
  if (scripted) {
    std::printf("script: %d failure(s)\n", scriptFails);
    return scriptFails ? 3 : 0;
  }
  return 0;
}
