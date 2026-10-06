// Helpers shared by the simulation's .cpp files (game.cpp, ai.cpp, game_rpg.cpp...). Not used by the view.
#pragma once
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string>
#include "rpg/sim/game.h"
#include "rpg/world/ids.h"

namespace gsim {
using art::Monster;

struct MStat { float hp, dmg, speed, range, aggro, radius, windup; int xp; bool ranged, flying; };
inline const MStat& mstat(Monster m) {
  static const MStat t[] = {
      {34, 13, 66, 13, 120, 5, 0.32f, 12, false, false},   // Wolf
      {36, 8, 56, 13, 90, 6, 0.40f, 14, false, false},     // Boar
      {95, 15, 46, 17, 100, 8, 0.50f, 38, false, false},   // Bear
      {22, 4, 30, 11, 80, 5, 0.45f, 6, false, false},      // Slime
      {34, 7, 58, 13, 110, 6, 0.34f, 15, true, false},     // Spider
      {14, 3, 76, 10, 120, 4, 0.25f, 5, false, true},      // Bat
      {35, 8, 40, 15, 120, 5, 0.42f, 18, false, false},    // Skeleton
      {70, 12, 38, 17, 120, 6, 0.48f, 30, false, false},   // Draugr
      {26, 6, 60, 12, 120, 5, 0.30f, 10, false, false},    // Goblin
      {105, 17, 44, 19, 110, 9, 0.55f, 60, false, false},  // Troll
      {60, 10, 48, 15, 140, 6, 0.40f, 36, true, true},     // Wraith
      {22, 5, 30, 11, 60, 6, 0.40f, 6, false, false},      // Mudcrab
      {46, 9, 66, 13, 130, 5, 0.30f, 20, false, false},    // IceWolf
      {62, 10, 52, 15, 120, 7, 0.36f, 30, true, false},    // FrostSpider
      {85, 14, 40, 18, 100, 8, 0.50f, 42, false, false},   // Sandworm
      {1500, 30, 72, 34, 320, 18, 0.60f, 1500, true, true},// Dragon
  };
  return t[(int)m];
}
inline const char* monsterName(Monster m) {
  static const char* n[] = {"WOLF", "BOAR", "CAVE BEAR", "SLIME", "GIANT SPIDER", "BAT", "SKELETON", "DRAUGR", "GOBLIN", "TROLL",
                            "WRAITH", "MUDCRAB", "ICE WOLF", "RIME SPIDER", "SANDWORM", "ASHFANG THE DRAGON"};
  return n[(int)m];
}
inline Vec2 faceVec(int f) {
  static const Vec2 v[4] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
  return v[f & 3];
}
inline int faceOf(Vec2 d) {
  if (std::fabs(d.x) > std::fabs(d.y) * 1.05f) return d.x > 0 ? 2 : 3;
  return d.y > 0 ? 0 : 1;
}

// the faction a monster species belongs to (factions.h)
inline Faction monsterFaction(Monster m) {
  switch (m) {
    case Monster::Wolf: case Monster::Boar: case Monster::Bear: case Monster::Spider: case Monster::Bat: case Monster::Mudcrab:
    case Monster::IceWolf: case Monster::FrostSpider: case Monster::Sandworm:
      return Faction::Wild;
    case Monster::Skeleton: case Monster::Draugr: case Monster::Wraith: return Faction::Undead;
    case Monster::Dragon: return Faction::Dragon;
    default: return Faction::Monster;
  }
}

// dialogue option actions (DlgOpt::action). M2 appends the wayside and quest ones.
enum DlgAct { A_BYE, A_TRADE, A_REST, A_RUMOR, A_ACCEPT, A_TURNIN, A_MAIN, A_HEAL, A_LEARN, A_CHAT, A_DECLINE, A_ASK, A_BED,
              A_DELIVER, A_ESCORT, A_TOLLPAY, A_TOLLREFUSE, A_NEWS, A_GRAVE };

// ---- quest text helpers (game_rpg.cpp, quests.cpp, wayside.cpp)
inline const char* monsterPlural(Monster m) {
  static const char* mn[] = {"WOLVES", "BOARS", "BEARS", "SLIMES", "SPIDERS", "BATS", "SKELETONS", "DRAUGR", "GOBLINS", "TROLLS", "WRAITHS", "MUDCRABS", "ICE WOLVES", "RIME SPIDERS", "SANDWORMS", "DRAGONS"};
  return mn[(int)m];
}
// compass direction of a tile offset (north is up the map): "NORTH-EAST" or, short, "NE"
inline std::string dirWord(int dx, int dy, bool shortForm) {
  static const char* lf[8] = {"EAST", "NORTH-EAST", "NORTH", "NORTH-WEST", "WEST", "SOUTH-WEST", "SOUTH", "SOUTH-EAST"};
  static const char* sf[8] = {"E", "NE", "N", "NW", "W", "SW", "S", "SE"};
  if (dx == 0 && dy == 0) return shortForm ? "HERE" : "RIGHT HERE";
  float a = std::atan2((float)-dy, (float)dx);
  int k = ((int)std::lround(a / (3.14159265f / 4)) % 8 + 8) % 8;
  return shortForm ? sf[k] : lf[k];
}
// the giver's town name ("" when unknown)
inline std::string giverTown(const World& w, const Quest& q) {
  return q.giverSite >= 0 && q.giverSite < (int)w.sites.size() ? w.sites[q.giverSite].name : std::string();
}
// "BOUNTY READY: RETURN TO X IN Y": shown as a notice when a radiant quest's objective is done
inline std::string rewardReadyMsg(const World& w, const Quest& q) {
  std::string town = giverTown(w, q);
  const bool bounty = q.type == QType::Bounty || q.type == QType::Hunt || q.type == QType::NamedBandit;
  if (q.flags & QF_GRAVE) return "FOUND: " + q.subject + ". LAY IT ON THE GRAVE";
  return std::string(bounty ? "BOUNTY READY" : "REWARD READY") + ": RETURN TO " + q.giverName + (town.empty() ? "" : " IN " + town);
}
// the first of these that fits a journal line (36 characters), else the last one cut down
inline std::string fitLine(std::initializer_list<std::string> opts) {
  std::string last;
  for (const std::string& o : opts) { if (o.size() <= 36) return o; last = o; }
  return last.substr(0, 36);
}
// where the player is on the overworld (the door or entrance they went in by, when inside)
inline void overworldTile(const Game& g, int& x, int& y) {
  if (g.inside && g.subBldg >= 0) { x = g.world.over.bldgs[g.subBldg].doorX(); y = g.world.over.bldgs[g.subBldg].doorY() + 1; return; }
  if (g.inside && g.subSite >= 0) { x = g.world.sites[g.subSite].ex; y = g.world.sites[g.subSite].ey; return; }
  x = (int)std::floor(g.pl().p.x / TILE); y = (int)std::floor(g.pl().p.y / TILE);
}
// is this NPC the one who gave the quest? (the opening's giver is whoever keeps the start village inn)
inline bool isGiver(const Quest& q, const Actor& a) {
  if (!a.npc) return false;
  if (q.giverSlot < 0) return a.role == Role::Innkeeper && a.bldg >= 0 && a.bldg == q.giverBldg;
  return q.giverSite == a.site && q.giverBldg == a.bldg && q.giverSlot == a.slot;
}

// M2 (SIM lane): Game::marks key spaces. A key is a stable id (a site's Gid, an npcKey, a quest id) mixed with a tag,
// so facts about different things never collide and survive saves (marks is saved as u64 -> i32).
enum class Mk : uint64_t {
  Stones = 1,      // a standing-stones vignette (site id): the day its heart stone last blessed the player
  Toll = 2,        // a toll bridge (site id): the day the toll was paid (free passage that day)
  Grave = 3,       // a lone grave (site id): read (1)
  News = 4,        // a traveller (npcKey): the day they last told the player a rumour
  HeirChest = 5,   // an Heirloom quest (quest id): the tile (y * w + x) of its chest in the dungeon map
  LastOffer = 6,   // a quest giver (npcKey): 1 + the QType of the last job they gave (the next one differs)
  Ambush = 7,      // a caravan vignette (site id): the ambush has sprung (1)
};
inline uint64_t markKey(uint64_t id, Mk tag) { return ew::mix64(id ^ ((uint64_t)tag * 0xD1B54A32D192ED03ull)); }

// M2 travel behind the fade (travel.cpp): the fade to black, and frameWork's generation budget once it is black
constexpr float kTravelFadeOut = 0.25f;
constexpr double kTravelBlackBudgetMs = 11.0;
}  // namespace gsim
using namespace gsim;
