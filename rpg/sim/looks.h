// People's looks: the census dress (census::Rank, census::dress), the census age on a look (census::ageLook), and the
// player's chosen appearance as a look (appearanceLook) (SOLID 2a.4 SIM-SEAMS). Defined in rpg/sim/looks.cpp (census)
// and rpg/sim/gear_look.cpp (appearanceLook). Pure functions of their arguments: the galleries, the doll scripts and
// the tests call them too.
// Who may include it: rpg/sim, rpg/view (creator, paperdoll, scripts) and tools (galleries, tests).
#pragma once
#include <cstdint>
#include <string>
#include "rpg/art/art_human.h"
#include "rpg/sim/appearance.h"

enum class Role : uint8_t;   // rpg/sim/world.h
namespace cult { struct Culture; }

namespace census {
struct Rank { bool royal = false, capital = false; };
// Dress a person in a culture (C: their people; owner: the kingdom whose arms a guard wears) and name them
void dress(art::HumanLook& L, std::string& name, Role r, bool female, const cult::Culture& C, const cult::Culture& owner,
           uint64_t seed, const Rank& rank);
// the census age on a look: child, youth, adult, elder
void ageLook(art::HumanLook& L, int years, bool childRole, bool elder, uint64_t seed);
}  // namespace census

// The chosen appearance in the shirt and trousers it starts with (rpg/sim/gear_look.cpp; gear_look.h declares it too)
art::HumanLook appearanceLook(const Appearance& app, const cult::Culture* home);
