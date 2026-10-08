// M4 "Banners": the war made visible (VISION_PLAN 4.4 tier A "present", 4.5, 4.7; owner 2026-10-03: kingdom guards in
// their banner colours protect member settlements, frontier villages have militia only; monsters entering towns attack
// villagers, villagers defend or flee, guards protect). WARDS lane.
//
// Game::war holds this lane's runtime state (never saved: it is rebuilt from the realm, rpg/sim/realm.h, whenever the
// window or the realm changes). The WARDS lane owns this header and may change WarState freely; Game only stores it.
//
// What lives where (WARDS files):
//   war_game.cpp     the Game hooks (warStep / warTalk / warChoose / warKill / warPropKingdom / warHostile), the siege
//                    quests, the camps' people (soldiers, captains, refugees), road patrols and checkpoints
//   war_overlay.cpp  the overlays on the loaded world (charred buildings, rubble, ash, scaffolds, garrison towers, siege
//                    and refugee camps, closed gates, lowered banners), applied and restored idempotently
//   game.cpp         guard counts per settlement (kingdom watch, frontier militia), burned-out buildings barred
//   ai.cpp           soldiers and captains, guards fighting a kingdom's enemies, the town's defence
//   looks.cpp        soldiers, captains, refugees and heralds dressed in the owner culture's arms grammar
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "rpg/world/ids.h"

class Game;
struct Actor;

// One tile an overlay changed (global tile): what was there before and what it holds now (art::Prop + 1, 0 none).
struct WarTile {
  int32_t gx = 0, gy = 0;
  uint8_t was = 0, now = 0;
};

enum class CampKind : uint8_t { Siege, Refugee, Checkpoint };

// A camp the overlays laid out (global tiles). Its people are spawned near the player (war_game.cpp).
struct WarCamp {
  CampKind kind = CampKind::Siege;
  ew::Gid site = 0;              // the besieged settlement / the town sheltering refugees / (checkpoint) 0
  ew::Gid kingdom = 0;           // whose camp: the attacker / the refuge's owner / the checkpoint's owner
  ew::Gid other = 0;             // the defender / the burned settlement the refugees fled / the enemy across the border
  uint32_t siege = 0;            // realm::Siege::id (Siege camps)
  int32_t gx = 0, gy = 0;        // the camp's heart (the campfire)
  int32_t cmdX = 0, cmdY = 0;    // the command tent's tile (Siege) / the gathering spot
  std::vector<std::pair<int32_t, int32_t>> posts;   // tiles its people stand at or walk between
  std::vector<std::pair<int32_t, int32_t>> tents;   // tent anchor tiles (reinforcements come out of these)
  int32_t walkX = 0, walkY = 0;  // (Refugee) where the walkers head on the road toward the burned place
  int spawned = 0;               // people of this camp in play now (war_game.cpp)
};

// One settlement's applied overlay (so it can be restored exactly: identity never changes, only overlays)
struct WarOverlay {
  ew::Gid site = 0;
  std::vector<WarTile> tiles;                     // props placed, and what they replaced
  std::vector<std::pair<int, uint8_t>> charred;   // building handle, its charred before
  std::vector<std::pair<int, uint32_t>> banners;  // building handle, its banner before (abandoned places lower them)
  int tower = -1;                                 // a runtime garrison tower (World::over.bldgs handle), -1 none
};

// A road patrol of 2-4 soldiers (VISION_PLAN 4.5): walks the road between a kingdom's settlements near the player.
struct WarPatrol {
  std::vector<int> ids;                            // actor ids (the leader first)
  std::vector<std::pair<int32_t, int32_t>> path;   // global road tiles, the leader walks them in order
  int at = 0;                                      // the next path index
  ew::Gid kingdom = 0;
  bool hostile = false;                            // the player's reputation with them is <= -25 during a war
  float age = 0;
};

// The siege quests' progress (never saved: rebuilt from Game::quests, QType::War)
struct WarQuestRun {
  int quest = 0;
  bool captainDead = false;
  uint32_t siege = 0;
  bool defCaptainCounted = false;   // (fixer M4 r2) an assault counts the defenders' captain once, not each time he returns
};

struct WarState {
  // the realm event serial and window shift count the overlays were last applied at (rebuild when either moves)
  uint32_t appliedSerial = 0;
  int appliedShifts = -1;
  // settlements (site handles) whose overlay (charred buildings, garrison tower, siege camp, refugee camp, lowered or
  // replaced banners) is applied in the window now
  std::vector<int> overlaid;
  float patrolT = 0;           // road patrols: time to the next spawn check
  float campT = 0;             // siege / refugee camps: time to the next people check
  // ---- WARDS lane (M4 phase B)
  uint64_t appliedSig = 0;     // signature of the settlement states the overlays were applied from
  float sigT = 0;              // time to the next signature check
  bool dirty = true;           // re-apply on the next step (a script, a test or a load)
  std::vector<WarOverlay> ov;  // applied overlays, one per settlement (and one, site 0, for the checkpoints)
  std::vector<WarCamp> camps;
  std::unordered_map<uint64_t, ew::Gid> propKingdom;   // warTileKey(global) -> the kingdom whose camp the war prop is
  std::vector<int> barred;     // building handles the player cannot enter (burned out, garrison towers)
  std::vector<int> towersFree; // retired runtime tower records (World::over.bldgs handles) ready for reuse
  std::vector<WarPatrol> patrols;
  std::unordered_map<int, ew::Gid> guardOwner;         // site handle -> the owner its guards in play serve
  std::vector<WarQuestRun> runs;
  std::unordered_map<uint64_t, std::vector<int>> campPeople;   // a camp (siege id, or refugee site ^ tag) -> its actor ids
  std::unordered_map<uint64_t, float> reinforceT;             // a siege camp's next reinforcement from its tents
  std::vector<ew::Gid> gatesOpen;   // (runtime cache; warGatesOpen also derives it from the saved War quests)
  // (fixer M4 r1) the barricade tiles of closed gates (warTileKey, global): they stop everyone but the player, who is
  // never shut in or out of a town by a siege that began round them (the defenders let a traveller through)
  std::unordered_set<uint64_t> gateTiles;
  // where a camp or a garrison tower was laid out (global tile, by site id and kind): chosen once with the whole place in
  // the window and kept, so a camp never jumps when the window moves (identity in global terms)
  std::unordered_map<uint64_t, std::pair<int32_t, int32_t>> spots;   // besieged settlements whose gates the player has had opened (barricades lifted)
  int nextSlot = 0;           // runtime people of camps and patrols (actor slots 20000+)
  float barredSayT = 0;        // the "burned out" notice is not repeated every frame
  // costs (tests report them: rpg_test --wards)
  double stepMs = 0, worstStepMs = 0, applyMs = 0, worstApplyMs = 0;
  int steps = 0, applies = 0, appliedSites = 0;
  int patrolsSpawned = 0, checkpoints = 0;
};

// ---- helpers the other WARDS files and the tests share (war_game.cpp / war_overlay.cpp)
uint64_t warTileKey(int32_t gx, int32_t gy);
// who fights whom: the faction matrix, the kingdoms' wars (Actor::realm), and the player's side (Actor::hostile)
bool warFoes(const Game& g, const Actor& a, const Actor& b);
// the player cannot go in: a burned-out building (Bldg::charred 2) or a garrison tower
bool warBarred(const Game& g, int bldg);
// a settlement no kingdom holds (the wildlands, SS_FRONTIER): militia only, no kingdom guards (owner 2026-10-03)
bool warFrontier(const Game& g, int site);
// the kingdom guards a settlement keeps on its streets: villages 2-3, towns 3-5, cities 7-12 (VISION_PLAN 10.4), from
// the realm's garrison and the owner's military; 0 for a frontier, abandoned or ruined place
int warGuardsWanted(const Game& g, int site);
bool warKeyPerson(const Game& g, int site, int slot);   // an open quest's giver or a running story's actor (street slot): always spawns
// M0 town defence's militia rule with the M4 frontier: brave adults with a tool (smiths, farmers, a third of the villagers)
// take up arms when monsters come; in a frontier village (no kingdom's watch) most villagers do, the innkeeper, the
// merchants and the hunters too, and they hit harder. `key` is the person's identity hash (Game::spawnHuman's)
void warArmMilitia(const Game& g, Actor& a, uint64_t key);
// nobody lives there now (abandoned, ruined): no people, no banners
bool warEmpty(const Game& g, int site);
// a settlement's people may come out (false: its home burned out, or the realm's population loss leaves them gone)
bool warSpawnAllowed(const Game& g, int site, int tx, int ty, int slot);
// (fixer M4 r1) a besieged settlement's gates stand open for the player: a BREAK THE SIEGE quest taken there, or a won
// assault on it (derived from the saved War quests, so a reload keeps them open)
bool warGatesOpen(const Game& g, ew::Gid site);
// a closed gate's barricade the player may pass (local tile)
bool warGatePass(const Game& g, int tx, int ty);
// (fixer M4 r1) the besiegers' captain of a siege fell to the player (QF_WAR_CAPDEAD on its War quest: saved)
bool warCaptainDead(const Game& g, uint32_t siege);
// the overlays (war_overlay.cpp): restore everything applied, then apply the realm's states to every loaded
// settlement near the window. Idempotent; cheap when nothing changed (Game::warStep calls it on a change).
void warApplyOverlays(Game& g);
void warRestoreOverlays(Game& g);
// the camp laid out for a siege (nullptr none in the window)
const WarCamp* warCampOf(const Game& g, uint32_t siege);
const WarCamp* warRefugeCampOf(const Game& g, ew::Gid site);
// the free ground test the overlays use (no road, water, building, wall, cliff or solid prop)
bool warFreeTile(const Game& g, int tx, int ty);
