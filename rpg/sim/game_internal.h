// Helpers shared by the simulation's .cpp files (game.cpp, ai.cpp, game_rpg.cpp...). Not used by the view.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>
#include "rpg/culture/society.h"
#include "rpg/sim/game.h"
#include "rpg/world/ids.h"
#include "rpg/world/wildlife.h"

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
      // M3c Wildlands wildlife (LIFE lane): fair against the classic beasts of the same depth (ai.cpp gives each its
      // move: the scorpion's every-third heavy sting, the hyena pack's circling, the lurker's lunge from the water, the
      // yeti's frost slam, the wisp's kited bolts, the hound's fire bite, the blightspawn's spore burst)
      {52, 10, 48, 16, 100, 6, 0.42f, 24, false, false},   // Scorpion
      {28, 7, 72, 13, 150, 5, 0.28f, 11, false, false},    // Hyena (packs of 3-4)
      {78, 15, 36, 17, 70, 8, 0.50f, 32, false, false},    // Lurker (slow ashore, a fast lunge)
      {108, 17, 46, 19, 120, 9, 0.55f, 64, false, false},  // Yeti
      {26, 7, 58, 12, 150, 4, 0.50f, 20, true, true},      // Wisp (ranged bolts, floats)
      {48, 11, 74, 13, 150, 5, 0.30f, 32, false, false},   // EmberHound
      {50, 10, 40, 15, 130, 6, 0.45f, 28, false, false},   // Blightspawn
      // M6 Steel (FOES lane; ai.cpp gives each its moves): the harpy circles on the wing, dives (a swoop you roll
      // through: it bites on contact) and shrieks (a short slow); the golem is slow stone (double armour, foes_game.cpp),
      // every second blow its wide telegraphed ground slam, and fire and frost slide off it sooner
      {30, 9, 72, 12, 160, 5, 0.30f, 22, false, true},     // Harpy (packs of 2-3 on the cliffs and crags)
      {160, 19, 28, 19, 90, 9, 0.70f, 76, false, false},   // Golem (alone: crystal barrens, mountains, ruin wardens)
  };
  static_assert(sizeof(t) / sizeof(t[0]) == (size_t)Monster::COUNT, "stats for every monster");
  return t[(int)m];
}
// the wind-up of a heavy, telegraphed blow (the brutes' slam, the scorpion's sting, the blightspawn's spore burst);
// the view's danger ring fills over the same time
inline float heavyWindup(Monster m) {
  switch (m) {
    case Monster::Troll: case Monster::Yeti: return 0.9f;
    case Monster::Blightspawn: return 0.75f;
    case Monster::Golem: return 1.0f;
    default: return 0.8f;
  }
}
inline const char* monsterName(Monster m) {
  static const char* n[] = {"WOLF", "BOAR", "CAVE BEAR", "SLIME", "GIANT SPIDER", "BAT", "SKELETON", "DRAUGR", "GOBLIN", "TROLL",
                            "WRAITH", "MUDCRAB", "ICE WOLF", "RIME SPIDER", "SANDWORM", "ASHFANG THE DRAGON",
                            "GIANT SCORPION", "HYENA", "MIRE LURKER", "YETI", "WILL-O'-WISP", "EMBER HOUND", "BLIGHTSPAWN",
                            "HARPY", "STONE GOLEM"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Monster::COUNT, "a name for every monster");
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
    case Monster::Scorpion: case Monster::Hyena: case Monster::Lurker: case Monster::Yeti:   // (M3c wildlife)
      return Faction::Wild;
    case Monster::Skeleton: case Monster::Draugr: case Monster::Wraith: return Faction::Undead;
    case Monster::Dragon: return Faction::Dragon;
    default: return Faction::Monster;
  }
}

// dialogue option actions (DlgOpt::action). M2 appends the wayside and quest ones.
enum DlgAct { A_BYE, A_TRADE, A_REST, A_RUMOR, A_ACCEPT, A_TURNIN, A_MAIN, A_HEAL, A_LEARN, A_CHAT, A_DECLINE, A_ASK, A_BED,
              A_DELIVER, A_ESCORT, A_TOLLPAY, A_TOLLREFUSE, A_NEWS, A_GRAVE,
              A_GROUP, A_BACK };   // (M7b) an option group's header (arg: the group) and the way back to the first page

// (fixer M7b r1, review: "script choose can pick options hidden in a closed option group") the option a player can
// reach by tapping: one on show now, else one behind a header on show (the header is tapped first), else one back on
// the first page or in another group (SOMETHING ELSE. first). Test scripts choose through this, so they walk the
// player's own way through the option groups. open: tap the way there and return the option's index once it is on
// show; !open: only say whether it is reachable (its index, the dialogue untouched). -1: the player cannot reach it.
template <class Match>
inline int dlgReach(Game& g, Match match, bool open) {
  auto shownAt = [&]() {
    for (size_t o = 0; o < g.dlg.opts.size(); o++) if (g.dlg.shows(g.dlg.opts[o]) && match(g.dlg.opts[o])) return (int)o;
    return -1;
  };
  for (int step = 0; step < 3; step++) {
    const int k = shownAt();
    if (k >= 0) return k;
    int target = -1;
    for (size_t o = 0; o < g.dlg.opts.size() && target < 0; o++) if (match(g.dlg.opts[o])) target = (int)o;
    if (target < 0) return -1;
    const int grp = g.dlg.opts[(size_t)target].grp;
    int tap = -1;
    for (size_t h = 0; h < g.dlg.opts.size() && tap < 0; h++) {
      const DlgOpt& hd = g.dlg.opts[h];
      if (!g.dlg.shows(hd)) continue;
      if (hd.action == A_GROUP && hd.arg == grp) tap = (int)h;   // its header is on this page
    }
    for (size_t h = 0; h < g.dlg.opts.size() && tap < 0; h++) {
      const DlgOpt& hd = g.dlg.opts[h];
      if (g.dlg.shows(hd) && hd.action == A_BACK) tap = (int)h;   // back to the first page, then on from there
    }
    if (tap < 0) return -1;
    if (!open) {   // (a dry run: its header is on the first page, or on this one)
      if (g.dlg.opts[(size_t)tap].action == A_GROUP || grp == 0) return target;
      for (const DlgOpt& hd : g.dlg.opts) if (hd.action == A_GROUP && hd.arg == grp && hd.grp == 0) return target;
      return -1;
    }
    g.dialogueChoose(tap);
    if (g.mode != Mode::Dialogue) return -1;
  }
  return -1;
}

// (M7b) the kind of a townsfolk option (life_game.cpp): 1 a service (the inn's meal, food for the stores), 2 a kindness
// (share food, a coin, a day's hire), 3 the chat ("HOW ARE YOU KEEPING?"), 0 anything else
int lifeOptKind(int action);

// (M7b) a count of goods as people say it: "SIX CUTS OF MEAT", "FOUR SACKS OF FLOUR" (good: ew::goodName's word)
inline std::string goodUnits(int n, const std::string& good) {
  static const char* const k[][2] = {{"GRAIN", "SACKS OF GRAIN"}, {"FLOUR", "SACKS OF FLOUR"}, {"BREAD", "LOAVES OF BREAD"},
                                     {"PRODUCE", "BASKETS OF PRODUCE"}, {"FISH", "BASKETS OF FISH"}, {"ORE", "LOADS OF ORE"},
                                     {"INGOTS", "INGOTS"}, {"TOOLS", "SETS OF TOOLS"}, {"LOGS", "LOADS OF LOGS"}, {"PLANKS", "BUNDLES OF PLANKS"},
                                     {"WOOL", "BALES OF WOOL"}, {"CLOTH", "BOLTS OF CLOTH"}, {"LIVESTOCK", "HEAD OF LIVESTOCK"},
                                     {"MEAT", "CUTS OF MEAT"}, {"HIDES", "HIDES"}, {"LEATHER", "ROLLS OF LEATHER"}, {"POTTERY", "PIECES OF POTTERY"}};
  static const char* const w[] = {"NO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN", "TWELVE"};
  const std::string num = n >= 0 && n <= 12 ? std::string(w[n]) : std::to_string(n);
  for (const auto& e : k) if (good == e[0]) return num + " " + e[1];
  return num + " LOADS OF " + good;
}

// ---- quest text helpers (game_rpg.cpp, quests.cpp, wayside.cpp)
inline const char* monsterPlural(Monster m) {
  static const char* mn[] = {"WOLVES", "BOARS", "BEARS", "SLIMES", "SPIDERS", "BATS", "SKELETONS", "DRAUGR", "GOBLINS", "TROLLS", "WRAITHS", "MUDCRABS", "ICE WOLVES", "RIME SPIDERS", "SANDWORMS", "DRAGONS",
                             "SCORPIONS", "HYENAS", "MIRE LURKERS", "YETIS", "WISPS", "EMBER HOUNDS", "BLIGHTSPAWN",
                             "HARPIES", "GOLEMS"};
  static_assert(sizeof(mn) / sizeof(mn[0]) == (size_t)Monster::COUNT, "a plural for every monster");
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
// (M3b fixer) the society's titles where a site lies (the kingdom's culture, else the site's own): the lord's ("JARL",
// "NOYAN", "EMIR", "PROVOST"...) and the ruler's ("KING", "KHAN"...). Unknown land keeps the old "JARL" / "KING".
inline const cult::Culture* titleCulture(const World& w, int site) {
  if (site < 0 || site >= (int)w.sites.size()) return nullptr;
  if (const cult::Culture* c = w.cultureOfKingdom(w.sites[(size_t)site].kingdom)) return c;
  return w.cultureOf(site);
}
inline std::string lordTitleAt(const World& w, int site) {
  const cult::Culture* c = titleCulture(w, site);
  return c ? std::string(cult::societyOf(*c).lordTitle) : std::string("JARL");
}
inline std::string rulerTitleAt(const World& w, int site) {
  const cult::Culture* c = titleCulture(w, site);
  return c ? std::string(cult::societyOf(*c).rulerTitle) : std::string("KING");
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
  if (q.giverSite != a.site) return false;
  return (q.giverBldg == a.bldg && q.giverSlot == a.slot) || (a.altSlot >= 0 && q.giverBldg == a.altBldg && q.giverSlot == a.altSlot);
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
  // M4 STORY lane (rpg/story/story_internal.h mirrors these as story::MK_*)
  StoryGiver = 8,  // an NPC (npcKey) who started a story: the day (one story per giver, then they remember)
  StoryMark = 9,   // a story's persistent mark (script, mark name, its giver), an event story's last event id
  BoardTaken = 10, // a notice board notice (site id ^ entry): the day it was taken
  // M4: the STORY lane (owner of this file in M4) appends its tags here, below 32. The WARDS lane uses tags 32..47 and
  // the REALM lane 48..63, each defined in its own files as (Mk)(32 + n), so nobody else edits this enum.
  // M6: the FOES lane uses 64..79 (rpg/sim/foes.h MK_FOES_*), the NUMBERS / FORGE lane 80..95 (craft_game.cpp).
  // M7: the HOMESTEAD lane uses 96..111 (rpg/sim/home_game.cpp).
  // M8: the FELLOWS lane uses 112..127 (rpg/sim/party_*.cpp: PartyOps::MK_*).
};
inline uint64_t markKey(uint64_t id, Mk tag) { return ew::mix64(id ^ ((uint64_t)tag * 0xD1B54A32D192ED03ull)); }

// M2 travel behind the fade (travel.cpp): the fade to black, and frameWork's generation budget once it is black
constexpr float kTravelFadeOut = 0.25f;
constexpr double kTravelBlackBudgetMs = 9.0;   // (11 let one chunk job overrun the 16 ms step on the web after M7b cities)
}  // namespace gsim
using namespace gsim;

// ---- M7 HOMESTEAD lane: the home's internals (Game's friend). Defined in rpg/sim/home_game.cpp (the actions' helpers,
//      riding), home_talk.cpp (deeds, taxes, builders, stables, recipes, the miller, the farmhand's hire),
//      home_actors.cpp (farm animals, the horse, builders, the farmhand, predators) and home_interior.cpp (the player's
//      own house: furniture, stores, beds, cooking places).
struct HomeOps {
  static void say(Game& g, const std::string& s) { g.say(s); }
  static void give(Game& g, const Item& it) { g.addItem(it); }
  static void notice(Game& g, const std::string& s, uint32_t colour) { g.emit(Ev::Notice, g.pl().p, (int)colour, 0, s); }
  static void news(Game& g, const std::string& s) { g.emit(Ev::News, g.pl().p, 0, 1, s); }
  static void sfx(Game& g, int s, Vec2 p, float pitch = 1, float vol = 1) { g.sfx(s, p, pitch, vol); }
  // take n of the pack items matching pred (false, nothing taken: not enough)
  template <class F>
  static bool take(Game& g, int n, F&& pred) {
    int have = 0;
    for (const Item& it : g.inv) if (pred(it)) have += it.stackable() ? it.count : 1;
    if (have < n) return false;
    for (size_t i = 0; i < g.inv.size() && n > 0;) {
      Item& it = g.inv[i];
      if (!pred(it)) { i++; continue; }
      const int k = std::min(n, it.stackable() ? it.count : 1);
      n -= k;
      it.count -= k;
      if (it.count <= 0 || !it.stackable()) dropAt(g, (int)i);
      else i++;
    }
    return true;
  }
  template <class F>
  static int count(const Game& g, F&& pred) {
    int have = 0;
    for (const Item& it : g.inv) if (pred(it)) have += it.stackable() ? it.count : 1;
    return have;
  }
  static void dropAt(Game& g, int i);
  static bool hasTool(const Game& g, home::Tool t);
  static int settlementOf(const Game& g, int32_t gx, int32_t gy);
  static void restamp(Game& g) { g.homeSerial_ = g.world.placeSerial + 0x80000000u; }
  static int nextId(Game& g) { return g.nextId_++; }
  static int findActor(const Game& g, int id) { return g.findActor(id); }
  static void makeLook(Game& g, Actor& a, Role r, Rng& rr) { g.makeLook(a, r, rr); }
  static int spawnMonster(Game& g, art::Monster m, Vec2 p, int level) { return g.spawnMonster(m, p, level, false); }
  static void moveActor(Game& g, Actor& a, Vec2 d) { g.moveActor(a, d); }
  static bool bodyFree(const Game& g, Vec2 p, float r) { return g.bodyFree(p, r, false); }
  static const std::vector<int>& hostiles(const Game& g) { return g.hostiles_; }
  static void travelHomeOnRespawn(Game& g, int32_t gx, int32_t gy);
  // the census and resident an actor embodies (false: not a resident)
  static bool residentOf(Game& g, const Actor& a, life::Census*& c, int& idx, int& site);
  // the settlement an actor belongs to (its site, or its building's site; -1 none)
  static int siteOfActor(const Game& g, const Actor& a);
  // ---- home_talk.cpp
  static void talk(Game& g, Actor& a);
  static bool choose(Game& g, const DlgOpt& o);
  // ---- home_actors.cpp
  static void actorsStep(Game& g, float dt);
  static bool folk(Game& g, Actor& a, float dt);       // a home actor's behaviour (lifeFolk routes LB_HOMESTEAD here)
  static bool critterUse(Game& g, Actor& a);           // talkTo on a farm animal or the waiting horse: true handled
  static std::string critterLabel(const Game& g, const Actor& a);   // what using it does ("RIDE", "PET CLOVER")
  static void dismounted(Game& g, home::Dismount why); // the horse stays where the rider got off (or goes home)
  static void dayTurned(Game& g, int plot, int day);   // a new day on a plot near the player: the live raid roll
  static void clearHomeActors(Game& g);
  static int raidStart(Game& g, int plot);             // home::forceRaid
  static void wakeActors(Game& g) { g.homeActT_ = 0; } // the home actors are reconciled on the next step
  static Vec2 freeSpot(const Game& g, int tx, int ty) { return g.freeSpot(tx, ty); }
  static void talkTo(Game& g, Actor& a) { g.talkTo(a); }   // (tests, scripts) open a conversation
  static void interact(Game& g) { g.interact(); }           // (tests, scripts) the use button
  // ---- home_interior.cpp
  static void mapLoaded(Game& g);
  static bool useProp(Game& g, art::Prop p, int tx, int ty);
  static bool propUsable(const Game& g, art::Prop p, int tx, int ty);
  static bool bed(const Game& g, int tx, int ty);
  static void stampObj(Map& m, const home::PlacedObj& o, bool clear);   // one piece of furniture into / out of a map
  static int insideObjAt(const Game& g, int plot, int tx, int ty);     // Plot::inside index at a tile of this floor
};
// Actor::lifeBits bit of the actors the HOMESTEAD lane spawns (farm animals, the waiting horse, builders, the
// farmhand): lifeFolk hands them to HomeOps::folk. Actor::slot tells which (HOME_SLOT_*).
constexpr uint32_t LB_HOMESTEAD = 1u << 20;
constexpr int HOME_SLOT_ANIMAL = 8000;   // + plot * 64 + animal
constexpr int HOME_SLOT_HORSE = 8999;    // the horse out of its stable, waiting
constexpr int HOME_SLOT_BUILDER = 9000;  // + plot * 4 + k
constexpr int HOME_SLOT_HAND = 9100;     // + plot

// ---- M8 FELLOWS lane: the companions' internals (Game's friend). Defined across rpg/sim/party_game.cpp (records,
//      the hooks' bodies, deeds, downed and death), party_ai.cpp (fighting by vocation and stance, the bond skills,
//      potions), party_move.cpp (following, the trail, the catch-up, waiting, living at the player's house, riding,
//      the carry), party_recruit.cpp (offers, recruits, tavern regulars, sellswords, the rescued, the animal, the
//      stake from the real world) and party_graves.cpp (graves sited, stamped and visited; the looks VIEW reads).
struct PartyOps {
  // Game::marks tags (Mk 112..127)
  static constexpr uint64_t MK_HIRED = 112;   // a sellsword (inn Bldg id * 4 + slot): the day they were hired (gone a while)
  static constexpr int32_t MERC_FALLEN = 2147483647;   // MK_HIRED's value once that sellsword died: the slot stays empty
  static constexpr uint64_t MK_PUP = 113;     // a wolf den (Den id) whose pup was taken / a seller's pup (npcKey): 1
  static constexpr uint8_t PT_WALKOFF = 16;   // Actor::partyTag: a body walking home out of sight (left, dismissed, sent home)
  // ---- records (party_game.cpp)
  static party::Member* of(Game& g, const Actor& a);
  static int indexOf(const Game& g, const party::Member& m) { return (int)(&m - g.party.members.data()); }
  static ew::Gid homeland(const Game& g, const party::Member& m);
  static int actorOf(Game& g, int mi);                 // actors index of member mi (-1 not in play)
  static void removeMember(Game& g, int mi);
  static void dropActor(Game& g, int mi);              // the member's body leaves play (the record stays)
  static life::Resident* resident(Game& g, const party::Member& m);
  static void alert(Game& g, int mi, int kind, const std::string& s);
  static void kit(Game& g, party::Member& m, int level);
  static void body(Game& g, const party::Member& m, Actor& a);
  static bool baseLook(Game& g, const party::Member& m, art::HumanLook& out);   // (fixer M8 r1) street look, before gear
  static void fixLook(Game& g, party::Member& m);      // Member::look fixed once their census has dressed them
  static int spawn(Game& g, int mi, Vec2 at);          // the member's actor at `at` (map pixels): its Actor::id
  static void dieForGood(Game& g, int mi, const std::string& cause);
  static void censusDeaths(Game& g);                   // (fixer M8 r1) members who died in their census life: Fallen
  static void applyDeed(Game& g, const party::Party::Pending& d);
  static void daily(Game& g);
  static void downedStep(Game& g, float dt);           // bleeding out, the binding, the player's kneel
  static void revive(Game& g, int mi, Actor& a, bool fromAfar);
  static bool actorStep(Game& g, Actor& a, float dt);  // Game::partyActor
  static void leave(Game& g, int mi, bool angry);      // Left (angry) or dismissed: the body walks home out of sight
  // ---- fighting (party_ai.cpp)
  static int pickFoe(Game& g, const Actor& a, const party::Member& m);
  static bool fight(Game& g, Actor& a, party::Member& m, int mi, float dt);   // true: the fight drove the body this step
  static bool useSkill(Game& g, Actor& a, party::Member& m, int k, bool ordered);   // skill k of skillsOf(voc) now
  static void autoSkills(Game& g, Actor& a, party::Member& m, int mi);
  static void potion(Game& g, Actor& a, party::Member& m);
  static void heal(Game& g, Actor& a, party::Member& m, int mi);   // the healer's mend (base and Mend)
  static float tauntT(const party::Member& m);         // > 0 while the warrior's taunt holds
  static bool ranged(const party::Member& m) { return m.voc == party::Vocation::Archer || m.voc == party::Vocation::Mage; }
  static void strike(Game& g, Actor& a, party::Member& m);         // the blow / the shot at the end of a windup
  // ---- moving (party_move.cpp)
  static Vec2 slotBehind(const Game& g, int mi);
  static Vec2 spotBehind(Game& g, int mi);
  static void trailStep(Game& g);                      // the player's footsteps (Party::trail)
  static bool moveTo(Game& g, Actor& a, party::Member& m, Vec2 goal, float speed, float dt);   // navStep, else the trail
  static void follow(Game& g, Actor& a, party::Member& m, int mi, float dt);
  static Vec2 outOfSight(Game& g, int mi, int& crumb); // where a catch-up lands: behind the player, off the screen
  static void keepUp(Game& g, float dt);               // members with the player brought into play, the catch-up
  static void placesStep(Game& g);                     // waiting members and those at the player's house in and out of play
  static void homeLife(Game& g, Actor& a, party::Member& m, float dt);   // a member at the player's house: their day
  static int homePlotOf(const Game& g, const party::Member& m);         // Homes::plots index of Member::homePlot (-1)
  static float riderSpeed(const Game& g);              // the player's pace on horseback (px/s)
  // ---- recruiting (party_recruit.cpp)
  static void mapChanged(Game& g);                     // a new map: sellswords at an inn
  static int mercsAt(const Game& g, int bldg);         // sellswords an inn keeps (0..MERC_MAX)
  static int mercWage(const Game& g, int site);        // gold a day by the place's danger
  static party::Vocation mercVoc(uint32_t seed);
  static bool folk(Game& g, Actor& a, float dt);       // Game::partyFolk
  static bool denInteract(Game& g);                    // a cleared wolf den's pup (interact)
  // ---- graves (party_graves.cpp)
  static void siteGrave(Game& g, party::Fallen& f);   // into their home's temple yard / graveyard, or where they fell
  static void gravesStep(Game& g);                     // unsited graves sited when their home loads; stamping
  static int graveNear(const Game& g, Vec2 p, float r);   // a sited grave within r px of p (Party::fallen index, -1)
};
// (M8 FELLOWS, life_game.cpp) a companion's body laid in the bed at interior tile (x, y) / sat on the seat there (the
// TOWNSFOLK rules' fit: the head on the pillow, the seat line on the seat), or false when that prop is no bed / seat
bool lifeLayInBed(Game& g, Actor& a, int x, int y);
bool lifeSitOn(Game& g, Actor& a, int x, int y, art::Posture post);
