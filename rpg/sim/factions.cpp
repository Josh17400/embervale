// The faction hostility matrix (see factions.h). Owned by the town-defence lane in M0.
#include "rpg/sim/factions.h"

const char* factionName(Faction f) {
  static const char* n[] = {"PLAYER", "TOWN", "WILD", "MONSTER", "UNDEAD", "BANDIT", "DRAGON"};
  return (int)f < (int)Faction::COUNT ? n[(int)f] : "?";
}

bool factionsHostile(Faction a, Faction b) {
  if (a == b) return false;
  auto friendly = [](Faction f) { return f == Faction::Player || f == Faction::Town; };
  if (friendly(a) && friendly(b)) return false;
  // everything else is hostile to the player and the towns; monster groups leave each other alone for now
  return friendly(a) || friendly(b);
}
