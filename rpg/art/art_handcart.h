// EMBERVALE art API: the story's hand cart (P1b ENGINE lane; moved out of rpg/view/render.cpp in P2a phase A, painted
// in art_handcart.cpp). Part of rpg/art.h (include that). P2a HOLDERS scales it with the rig (art::bodyScale()).
#pragma once
#include <cstdint>
#include "engine/pix.h"

namespace art {

// The wagon an escort's driver pulls (goal escort ... wagon): a two-wheeled hand cart on raised shafts, modelled in 3/4
// like the market's carts and painted for the four headings, its spokes in four turns. Its ground shadow is cast from
// the same model. key: dir (0 east, 1 west, 2 north, 3 south) | load << 2 (0 sacks, 1 crates, 2 barrels, 3 hay) |
// turn << 4 | shadow << 6. The canvas is HANDCART_W x HANDCART_H with the cart's ground point (its axle's foot) at
// (HANDCART_OX, HANDCART_OY).
constexpr int HANDCART_W = 64, HANDCART_H = 50, HANDCART_OX = 32, HANDCART_OY = 38;
Canvas handcartSprite(uint64_t key);
// which of the four paintings a heading (radians, toward the driver) takes
int handcartDir(float ang);

}  // namespace art
