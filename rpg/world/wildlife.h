// M3c Wildlands: who lives in each biome (rpg/world/biomes.h Eco). One table for the generator's dens (deterministic,
// integer odds) and the game's roaming wildlife and hunt quests (runtime). LIFE lane owns rpg/world/wildlife.cpp; the
// DEN table is frozen by phase A (the WORLD lane's goldens hash the dens): a change to it is reported, not made.
#pragma once
#include <cstdint>
#include "rpg/art/art_monsters.h"
#include "rpg/world/biomes.h"

namespace ew {

// A wilderness den in biome e: its species and pack size from q (Q16 0..65535, the den's own hash). False: no dens in
// this biome (open sea, beaches, cliffs, bare mountain rock).
bool denOf(Eco e, int32_t q, art::Monster& mon, uint8_t& pack);

// A lone roaming creature (Game's wildlife spawner): its species from roll in [0, 1). False: nothing roams here.
bool roamerOf(Eco e, bool night, float roll, art::Monster& mon);

// The beasts a hunt quest may name in biome e (3 entries, a beast may repeat). Each one can be met there (canMeet),
// unless no beast lives in e at all (open sea, bare rock): then a generic trio.
void huntTargets(Eco e, art::Monster out[3]);

// (M3c fixer) monster m can come out in biome e: from one of its dens (denOf) or as a lone roamer (roamerOf, day or
// night) that the spawner lets out near home (big brutes come only from dens there).
bool canMeet(Eco e, art::Monster m);

// A cave's / camp's dungeon theme beast in biome e (the region's POIs), from q (Q16); false: keep the generic themes.
bool caveTheme(Eco e, int32_t q, art::Monster& mon);

}  // namespace ew
