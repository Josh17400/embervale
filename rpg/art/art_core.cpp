// EMBERVALE art: colour math exported to the renderer. See rpg/art.h for the contract and rpg/art/art_internal.h for the shared helpers.
#include "rpg/art/art_internal.h"

namespace art {

uint32_t shade(uint32_t c, float k) {
  return rgba(clamp255(chR(c) * k), clamp255(chG(c) * k), clamp255(chB(c) * k), chA(c));
}

uint32_t mix(uint32_t a, uint32_t b, float t) {
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  return rgba(clamp255(chR(a) + (chR(b) - chR(a)) * t), clamp255(chG(a) + (chG(b) - chG(a)) * t),
              clamp255(chB(a) + (chB(b) - chB(a)) * t), chA(a));
}

}  // namespace art
