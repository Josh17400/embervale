// M3: the cultures of the world's records (World::cultureOf...), read through the endless generator's atlas.
// The generator (src) is the main thread's: call these from the main thread only (the view, the sim).
#include "rpg/culture/culture.h"
#include "rpg/sim/world.h"
#include "rpg/world/source.h"

const cult::Culture* World::cultureOf(int site) const {
  if (!src || site < 0 || site >= (int)sites.size()) return nullptr;
  const uint64_t id = sites[(size_t)site].culture;
  return id ? &src->culture(id) : nullptr;
}

const cult::Culture* World::cultureOfKingdom(int kingdom) const {
  if (!src || kingdom < 0 || kingdom >= (int)kingdoms.size()) return nullptr;
  const uint64_t id = kingdoms[(size_t)kingdom].culture;
  return id ? &src->culture(id) : nullptr;
}

const cult::Culture* World::cultureAtTile(int tx, int ty) const {
  if (!src) return nullptr;
  return &src->culture(src->cultureAt(ox + tx, oy + ty));
}
