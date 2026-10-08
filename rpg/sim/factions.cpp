// The faction hostility matrix (see factions.h). Owned by the town-defence lane in M0.
#include "rpg/sim/factions.h"

const char* factionName(Faction f) {
  static const char* n[] = {"PLAYER", "TOWN", "WILD", "MONSTER", "UNDEAD", "BANDIT", "DRAGON", "ARMY"};
  return (int)f < (int)Faction::COUNT ? n[(int)f] : "?";
}

bool factionsHostile(Faction a, Faction b) {
  if (a == b) return false;
  auto friendly = [](Faction f) { return factionCivil(f); };   // (M4: soldiers of every kingdom too; wars: war_game.cpp)
  if (friendly(a) && friendly(b)) return false;
  // everything else is hostile to the player and the towns; monster groups leave each other alone for now
  return friendly(a) || friendly(b);
}
