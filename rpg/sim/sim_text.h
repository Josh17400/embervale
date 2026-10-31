// Text helpers the sim, scripts and tests share: goods counted as people say them, monster plurals, compass words, the
// giver's town, the lord's and ruler's titles, the reward-ready notice and journal-line fitting (SOLID 2a.4 SIM-SEAMS;
// moved verbatim from rpg/sim/game_internal.h, which includes this file).
// Who may include it: rpg/sim, rpg/story, rpg/view/scripts and tools/tests.
#pragma once
#include <cmath>
#include <initializer_list>
#include <string>
#include "rpg/culture/society.h"
#include "rpg/sim/game.h"
#include "rpg/sim/monster_stats.h"

namespace gsim {

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
                             "HARPIES", "GOLEMS", "SEA SERPENTS", "KRAKENS", "MERROW"};
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
}  // namespace gsim
