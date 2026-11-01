// rpg/app/script_runner.h - the test-script driver (SOLID 3.1d): the script file, its commands (App::runCmd, one
// branch per command; lanes add more through rpg/view/script_api.h) and the autopilot (walkto / talkto).
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
#pragma once
#include <SDL3/SDL.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "rpg/sim/game.h"

struct ScriptCmd {
  float t = 0;
  std::vector<std::string> a;   // a[0] is the command
  int line = 0;
  std::string text;
};
extern bool g_scriptPhone;   // a "phone" header line: run at the iPhone review size (see the header comment)
void setEnvIfUnset(const char* k, const char* v);
bool loadScript(const char* path, std::vector<ScriptCmd>& out, uint64_t& seed);
SDL_Scancode keyFromName(std::string s);
// (test only) breadth-first path over the current map's walkable tiles, toward the reachable tile nearest (gx, gy)
bool findPath(const Map& m, int sx, int sy, int gx, int gy, std::vector<int>& path);

// the sim's private helpers the commands use (rpg/sim/game_internal.h: dlgReach, faceVec), defined in rpg/main.cpp
int dlgReach(Game& g, const std::function<bool(const DlgOpt&)>& match, bool open);
Vec2 faceVec(int f);
