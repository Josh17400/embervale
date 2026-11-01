// M9 magic's statuses (SOLID 4.1: split from magic.h so that game_types.h, for Actor::status / statusT, needs no lane
// header). magic.h includes it; everything else of magic stays there.
#pragma once
#include <cstdint>

namespace magic {

// ---- statuses (15.5: burning, chilled, frozen, wet, shocked, frightened, charmed, poisoned, blinded). On Actor::status
//      (bits) and Actor::statusT (seconds left each); never saved.
enum class Status : uint8_t { Burning, Chilled, Frozen, Wet, Shocked, Frightened, Charmed, Poisoned, Blinded, COUNT };
constexpr int STATUSES = (int)Status::COUNT;
constexpr uint16_t statusBit(Status s) { return (uint16_t)(1u << (unsigned)s); }

}  // namespace magic
