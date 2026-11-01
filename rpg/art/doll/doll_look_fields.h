// EMBERVALE art: the doll's ladder look fields (SOLID 5.4), defined in doll_look_fields.cpp (rpg_doll) next to
// applyLookField (doll.h). Who may include it: rpg/sim/gear_ladder.cpp (the looks macros) and tools/art_hash.cpp (the
// hash looks); both set the ladder fields through this one function.
#pragma once
#include <string>
#include "rpg/core/art_ids.h"

namespace art {
namespace doll {
// The ladder fields of a looks line, the ones applyLookField does not own: tierBody..tierBack (clamped 0..6), forgeBody..
// forgeBack (a packed style), rare mythic metalKind bladeKind motif kitVar floor alloyFam forgeKit alloyRank forgeGuise
// king forgeShape (strtol's forms) and gemColor ("#RRGGBB", or 0 for the glow colour). False for a field it does not know.
bool applyLadderLookField(HumanLook& look, const std::string& key, const std::string& value);
}  // namespace doll
}  // namespace art
