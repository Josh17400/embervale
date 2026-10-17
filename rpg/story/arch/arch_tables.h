// M6b "Sagas": the archetype and twist tables, one function per file (rpg/story/saga.h). Lead, phase A: frozen.
//
// rpg_sim is a static library, so self-registering objects would be dropped by the linker: index.cpp calls each file's
// add function instead. A lane adds archetypes ONLY inside the files it owns (never here, never in index.cpp):
//   ARCHETYPES lane: scripture.cpp (scripture), myth.cpp (ancient myth), legend.cpp (epic and Arthurian legend),
//                    lewis.cpp, tolkien.cpp (themes only), twists.cpp (the twists library)
//   VOICE lane:      folk.cpp (folk and fairy tale), maas.cpp, gwynne.cpp (themes only), world.cpp (world-native)
//   CAMPAIGNS lane:  rpg/story/campaigns/arcs.cpp (tier-3 campaign arcs)
//   (M8 VOICE lane)  companion.cpp (the companions' personal quests: `hook companion`)
//   (M12 REALM lane) rise.cpp (realms that rise and fall, cast from the living sim)
// MSVC caps one string literal near 16 KB: give every template its own raw literal R"SAGA(...)SAGA".
#pragma once
#include <vector>
#include "rpg/story/saga.h"

namespace story {
namespace saga {
namespace arch {

void addScripture(std::vector<Archetype>& v);
void addMyth(std::vector<Archetype>& v);
void addLegend(std::vector<Archetype>& v);
void addLewis(std::vector<Archetype>& v);
void addTolkien(std::vector<Archetype>& v);
void addFolk(std::vector<Archetype>& v);
void addMaas(std::vector<Archetype>& v);
void addGwynne(std::vector<Archetype>& v);
void addWorld(std::vector<Archetype>& v);
void addArcs(std::vector<Archetype>& v);
void addCompanion(std::vector<Archetype>& v);   // (M8)
void addRise(std::vector<Archetype>& v);        // (M12 REALM lane) rise.cpp: realms that rise and fall
void addTwists(std::vector<Twist>& v);

}  // namespace arch
}  // namespace saga
}  // namespace story
