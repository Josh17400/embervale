// rpg/view/scripts/script_util.h: helpers that were byte-identical in two or more script command files (SOLID 1.5), one
// copy here. Only for rpg/view/scripts/*.cpp. The functions sit in an anonymous namespace, as their copies did, so each
// file keeps its own internal-linkage copy; a helper whose copies differ (lower, nearestSettlement, playerGlobal, dress
// ...) keeps them in its files.
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "engine/pix.h"
#include "rpg/sim/game.h"

// (rpg/view/worldmap.cpp)
void worldMapCentreTile(double gx, double gy);

namespace {

inline std::string low(std::string s) {
  for (char& ch : s) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
  return s;
}
inline std::string lw(std::string s) {
  for (char& ch : s) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
  return s;
}
inline std::string up(std::string s) {
  for (char& ch : s) if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
  return s;
}
inline uint32_t hexc(uint32_t rgb) { return rgba((int)(rgb >> 16 & 255), (int)(rgb >> 8 & 255), (int)(rgb & 255)); }
// loaded settlements, nearest to the player first
inline std::vector<int> nearestSettlements(Game& g) {
  std::vector<std::pair<float, int>> d;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  for (size_t i = 0; i < g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (!s.settlement()) continue;
    const float dx = s.ex - px, dy = s.ey - py;
    d.push_back({dx * dx + dy * dy, (int)i});
  }
  std::sort(d.begin(), d.end());
  std::vector<int> out;
  for (auto& e : d) out.push_back(e.second);
  return out;
}

}  // namespace
