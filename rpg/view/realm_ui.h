// M4 "Banners" VIEW lane: what the HUD, the journal and the world map read off the realm (rpg/sim/realm.h), in one
// place so every reader names a kingdom, its society's word and a settlement's state the same way (VISION_PLAN 4.5,
// 4.6, 15.8: kingdom identity must be obvious).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/sim/game.h"

namespace cult { struct Culture; struct Heraldry; }

namespace rui {

// the society's word for a realm: KINGDOM, JARLDOM, KHAGANATE, CLANLANDS, THEOCRACY, REPUBLIC, HIGH REALM
const char* realmWord(uint8_t government);

// a kingdom as the view shows it: the realm's record when it has one (its name, arms and government), else the
// world's (World::kingdoms), else the generator's plan. ok == false: nobody (the wildlands) or unknown.
struct Look {
  bool ok = false;
  ew::Gid id = 0;
  std::string name;
  uint32_t color = 0, color2 = 0;
  uint8_t emblem = 0;
  uint8_t gov = 0;
  int handle = -1;                       // World::kingdoms index (-1: not loaded)
  const cult::Culture* culture = nullptr;
  std::string ruler;                     // "KHAN TEMUR" (empty: not known)
};
Look look(const Game& g, ew::Gid id);
// "THE KHAGANATE OF ASHMARK" / "KINGDOM OF ASHMARK" (the = false)
std::string realmTitle(const Look& k, bool the);
// the kingdom's arms (its culture's heraldry, else its two colours and emblem)
void arms(const Look& k, cult::Heraldry& out);

// a settlement's state in words, the worst first ("BESIEGED BY QIBA", "OCCUPIED BY QIBA", "BURNED", "HUNGRY",
// "SHELTERS REFUGEES"); empty when at peace. col: its colour (rgba).
std::string stateLine(const Game& g, ew::Gid site, uint32_t* col = nullptr);

// every settlement the realm has a state for that the view can reach: the loaded sites, the kingdoms' holdings, the
// sieges and the events (deduplicated; cheap enough to call a few times a second)
std::vector<const realm::SettlementState*> knownStates(const Game& g);

// changes whenever an owner, a flag or a siege changes (the map repaints its border tiles)
uint64_t signature(const Game& g);

// the realm's day (Game::day) a heard event happened, as "TODAY" / "YESTERDAY" / "5 DAYS AGO"
std::string daysAgo(int today, int day);

// whose land a global tile is now (World / realm: Realm::landOwner of the generator's kingdom there)
ew::Gid landOwnerAt(const Game& g, int32_t gx, int32_t gy);

}  // namespace rui
