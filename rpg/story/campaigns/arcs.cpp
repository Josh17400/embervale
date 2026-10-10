// M6b "Sagas": the tier-3 campaign arcs (rpg/story/saga.h Archetype with tier 3; chained by rpg/story/chain.cpp).
// CAMPAIGNS lane. An arc's template uses %local names and leaves through @next; the campaign's head declares the
// recurring roles every arc may name. Each campaign file (rpg/story/campaigns/<campaign>.cpp) holds its plan (head,
// arc slots, finale) and its own arcs; this file gathers the arcs into the archetype table (arch/index.cpp addArcs).
#include <vector>
#include "rpg/story/arch/arch_tables.h"

namespace story {
namespace saga {
namespace camp {
void addBurdenArcs(std::vector<Archetype>& v);      // campaigns/burden.cpp     THE BURDEN ACROSS KINGDOMS
void addPlagueArcs(std::vector<Archetype>& v);      // campaigns/plague.cpp     THE PALE CHOIR (the plague cult)
void addFaeArcs(std::vector<Archetype>& v);         // campaigns/fae.cpp        THE COURT UNDER THE HILL
void addRebellionArcs(std::vector<Archetype>& v);   // campaigns/rebellion.cpp  THE IRON TITHE (the rebellion)
void addDragonArcs(std::vector<Archetype>& v);      // campaigns/dragon.cpp     THE TONGUE OF THE BEAST (the dragon cult)
}  // namespace camp

namespace arch {

void addArcs(std::vector<Archetype>& v) {
  camp::addBurdenArcs(v);
  camp::addPlagueArcs(v);
  camp::addFaeArcs(v);
  camp::addRebellionArcs(v);
  camp::addDragonArcs(v);
}

}  // namespace arch
}  // namespace saga
}  // namespace story
