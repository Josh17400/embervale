// M7c the planner's vocabulary on the generator (rpg/world/town_plan.h, VISION_PLAN 15.M7c). EDGE lane (phase B).
// rankLayer: the last step of finish() (after urbanFabric): every street tile a planner did not rank itself (setRank)
// takes the rank its mask kind or the city plan's class says; the rank and role layers and the plan's traits go to
// SettlementOut. No Rng draw (nothing it decides moves a single tile of the built town).
#include <algorithm>
#include <cstdlib>
#include "rpg/world/town_gen.h"
#include "rpg/world/urban.h"

namespace ew {
namespace town {

namespace {
Rank rankOfUrb(Urb u) {
  switch (u) {
    case Urb::Avenue: return Rank::Main;
    case Urb::Street: return Rank::Street;
    case Urb::Lane: case Urb::Alley: return Rank::Lane;
    case Urb::Square: case Urb::Close: case Urb::Court: case Urb::Quay: return Rank::Square;
    case Urb::ParkPath: return Rank::Footpath;
    default: return Rank::None;
  }
}
}  // namespace

void Gen::rankLayer() {
  const size_t n = (size_t)W * H;
  if (rank.size() != n) rank.assign(n, 0);
  if (role.size() != n) role.assign(n, 0);
  const bool hasPlan = planned && plan.size() == n;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const size_t i = I(x, y);
      if (rank[i]) continue;
      Rank r = Rank::None;
      if (hasPlan && plan[i]) {
        const Urb u = (Urb)(plan[i] & URB_CLASS_MASK);
        r = rankOfUrb(u);
        // a sidewalk or a gutter is its carriageway's: the rank of the nearest carriageway tile within two
        if (u == Urb::Sidewalk || u == Urb::Gutter) {
          r = Rank::Main;
          int bestD = 99;
          for (int oy = -2; oy <= 2; oy++)
            for (int ox = -2; ox <= 2; ox++) {
              if (!in(x + ox, y + oy)) continue;
              const Urb v = (Urb)(plan[I(x + ox, y + oy)] & URB_CLASS_MASK);
              if (!urbCarriage(v)) continue;
              const int d = std::max(std::abs(ox), std::abs(oy));
              if (d < bestD) { bestD = d; r = rankOfUrb(v); }
            }
        }
      }
      if (r == Rank::None)
        switch (mask[i]) {
          case K_MAIN: r = Rank::Main; break;
          case K_LANE: r = Rank::Lane; break;
          case K_SQUARE: r = Rank::Square; break;
          default: break;
        }
      rank[i] = (uint8_t)r;
    }
  O.rank = rank;
  O.role = role;
  O.traits.order = (uint8_t)order;
  O.traits.age = (uint8_t)age;
  O.traits.layout = (uint8_t)style;
}

}  // namespace town
}  // namespace ew
