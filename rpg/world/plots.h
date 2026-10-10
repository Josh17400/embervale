// M7 "Home" (VISION_PLAN 8.2, 15.22): a settlement's lots for sale, as the generator plans them. Lead, M7 phase A.
// The LAND lane makes them (rpg/world: 1 to 3 fenced empty lots on a village's or town's outskirts beside a street, a
// FOR SALE sign by the gate; some on a river bank: PLOT_RIVERSIDE) and records them in the chunk records like the
// buildings (ew::ChunkData::plots: the whole list of every settlement touching the chunk); World::lots keeps the ones
// met (rpg/sim/world_endless.cpp). Deterministic generation; never saved (the player's property is home::Plot).
// Layout contract (the HOMESTEAD lane's buyLot and stampWindow rely on it): the lot's ground is clear (no building,
// wall, water, cliff, street or field inside it); a PLOT_FENCED lot's fence ring stands on its own border tiles (inside
// gx .. gx + w - 1, gy .. gy + h - 1) with one gap at (gateX, gateY) on the border facing the street, which a BFS
// reaches from the settlement's streets; the FOR SALE sign (art::Prop::ForSaleSign) stands inside the lot beside the
// gate. Once the lot is bought, home::stampWindow clears every generated prop inside the rect (the fence ring becomes
// the player's own Fence / Gate objects).
#pragma once
#include <cstdint>
#include "rpg/world/ids.h"

namespace ew {

enum : uint8_t {
  PLOT_RIVERSIDE = 1,   // a long side lies on a river or lake bank (water reachable from the plot's edge)
  PLOT_FENCED = 2,      // the generator fenced it (a fence ring with a gate on the street side)
};
struct PlotPlan {
  Gid id = 0;           // makeId(rx, ry, IdKind::Plot, local) (local < 0x800: generated; 0x800+ scripts' made-up lots,
                        // 0xC00+ the houses built on lots: home::houseBldg)
  Gid site = 0;         // the settlement it belongs to
  int32_t gx = 0, gy = 0;   // top-left (global tile)
  uint8_t w = 0, h = 0;     // size in tiles (home::lotInfo: small 10x8, medium 13x10, large 16x12)
  uint8_t size = 0;     // home::LotSize
  uint8_t flags = 0;    // PLOT_*
  int32_t gateX = 0, gateY = 0;   // the gate tile in the fence (global): where the street meets the lot
};

}  // namespace ew
