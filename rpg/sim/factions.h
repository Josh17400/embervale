// Factions: who fights whom. Actor::faction is the source of truth for hostility from M0 on; the older
// Actor::hostile flag stays in sync (hostile == hostile to the player) so existing code keeps working while the
// town-defence lane migrates AI targeting to factionsHostile().
#pragma once
#include <cstdint>

enum class Faction : uint8_t {
  Player,     // the player (and later companions)
  Town,       // villagers, merchants, guards, the jarl: a settlement's people
  Wild,       // wolves, bears, boars, spiders... hunt anything nearby, the player first
  Monster,    // goblins, trolls, slimes and other non-undead monsters
  Undead,     // skeletons, draugr, wraiths
  Bandit,     // bandits and their chiefs
  Dragon,     // Ashfang
  COUNT
};

const char* factionName(Faction f);
// The hostility matrix (rpg/sim/factions.cpp). Symmetric. Town and Player are allies.
bool factionsHostile(Faction a, Faction b);
