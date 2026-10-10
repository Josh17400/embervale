// M6b "Sagas": every archetype and twist, library order (rpg/story/arch/arch_tables.h). Lead, phase A: frozen.
#include <vector>
#include "rpg/story/arch/arch_tables.h"
#include "rpg/story/saga.h"

namespace story {
namespace saga {

const std::vector<Archetype>& archetypes() {
  static const std::vector<Archetype> v = [] {
    std::vector<Archetype> a;
    arch::addScripture(a);
    arch::addMyth(a);
    arch::addLegend(a);
    arch::addLewis(a);
    arch::addTolkien(a);
    arch::addFolk(a);
    arch::addMaas(a);
    arch::addGwynne(a);
    arch::addWorld(a);
    arch::addArcs(a);
    return a;
  }();
  return v;
}

const std::vector<Twist>& twists() {
  static const std::vector<Twist> v = [] {
    std::vector<Twist> t;
    arch::addTwists(t);
    return t;
  }();
  return v;
}

}  // namespace saga
}  // namespace story
