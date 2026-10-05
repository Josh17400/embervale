// Settlement economy facts (owner 2026-10-05, VISION_PLAN 15.8): what each settlement lives from, what it makes and what
// it must buy, and the shared production chains (ore -> ingot -> tools, grain -> flour -> bread, ...). The region plan
// picks a settlement's specialisation from its surroundings (SitePlan::special); the settlement generator builds the
// matching production buildings (rpg/world/town_build.cpp); the world records keep it (Site::special, produces, needs) for
// the economy (M4) and crafting (M6), which read the same recipe table so the player crafts with the chains the villages
// use. Header-only and pure: rpg_sim, the generator, the HUD and the tests all read it.
#pragma once
#include <cstdint>
#include "rpg/art.h"

namespace ew {

// What a settlement lives from. Villages are one thing; towns and cities add their region's trade to the full shop set.
enum class Specialty : uint8_t {
  None,       // not decided (synthetic test plans): the generator derives it from the archetype and the land
  Farming,    // fields, a mill (water or wind), the granary
  Fishing,    // coast, lake or river: jetties, drying racks, the fishmonger
  Mining,     // under a ridge: the mine, ore carts, the smelter
  Lumber,     // in the forest: log piles, the sawmill
  Herding,    // open grassland and hills: pens and troughs, the tanner
  COUNT
};
inline const char* specialtyName(Specialty s) {
  static const char* n[] = {"", "FARMING", "FISHING", "MINING", "LUMBER", "HERDING"};
  return (int)s < (int)Specialty::COUNT ? n[(int)s] : "";
}

// Trade goods (bits in Site::produces / Site::needs)
enum class Good : uint8_t {
  Grain, Flour, Bread, Produce, Fish, Ore, Ingot, Tools, Logs, Planks, Wool, Cloth, Livestock, Meat, Hides, Leather, Pottery,
  COUNT
};
constexpr uint32_t goodBit(Good g) { return 1u << (uint32_t)g; }
inline const char* goodName(Good g) {
  static const char* n[] = {"GRAIN", "FLOUR", "BREAD", "PRODUCE", "FISH", "ORE", "INGOTS", "TOOLS", "LOGS", "PLANKS", "WOOL",
                            "CLOTH", "LIVESTOCK", "MEAT", "HIDES", "LEATHER", "POTTERY"};
  return (int)g < (int)Good::COUNT ? n[(int)g] : "";
}

// One step of a production chain: `in` becomes `out` at a building of type `at` (the same table drives the village
// economy now and the player's crafting in M6)
struct Recipe { Good in; Good out; art::Building at; };
inline const Recipe* recipes(int& n) {
  static const Recipe r[] = {
      {Good::Grain, Good::Flour, art::Building::Windmill},  {Good::Grain, Good::Flour, art::Building::Watermill},
      {Good::Flour, Good::Bread, art::Building::Bakery},    {Good::Ore, Good::Ingot, art::Building::Smelter},
      {Good::Ingot, Good::Tools, art::Building::Smithy},    {Good::Logs, Good::Planks, art::Building::Sawmill},
      {Good::Wool, Good::Cloth, art::Building::Weaver},     {Good::Hides, Good::Leather, art::Building::Tanner},
      {Good::Livestock, Good::Meat, art::Building::Butcher}, {Good::Livestock, Good::Hides, art::Building::Butcher},
  };
  n = (int)(sizeof r / sizeof r[0]);
  return r;
}

// What a settlement of this specialisation makes (raw goods and what its own workshops turn them into) and what it must
// buy to live. Towns and cities (big = true) also make the crafted goods of the full shop set.
inline uint32_t specialtyProduces(Specialty s, bool big) {
  uint32_t p = 0;
  switch (s) {
    case Specialty::Farming: p = goodBit(Good::Grain) | goodBit(Good::Flour) | goodBit(Good::Produce); break;
    case Specialty::Fishing: p = goodBit(Good::Fish); break;
    case Specialty::Mining: p = goodBit(Good::Ore) | goodBit(Good::Ingot); break;
    case Specialty::Lumber: p = goodBit(Good::Logs) | goodBit(Good::Planks); break;
    case Specialty::Herding: p = goodBit(Good::Livestock) | goodBit(Good::Wool) | goodBit(Good::Hides) | goodBit(Good::Leather); break;
    default: break;
  }
  if (big) p |= goodBit(Good::Bread) | goodBit(Good::Tools) | goodBit(Good::Meat) | goodBit(Good::Cloth) | goodBit(Good::Pottery);
  return p;
}
inline uint32_t specialtyNeeds(Specialty s, bool big) {
  uint32_t n = goodBit(Good::Tools);   // everyone wears out tools
  switch (s) {
    case Specialty::Farming: n |= goodBit(Good::Fish) | goodBit(Good::Planks); break;
    case Specialty::Fishing: n |= goodBit(Good::Grain) | goodBit(Good::Planks) | goodBit(Good::Cloth); break;   // boats, nets
    case Specialty::Mining: n |= goodBit(Good::Bread) | goodBit(Good::Logs) | goodBit(Good::Meat); break;     // pit props, food
    case Specialty::Lumber: n |= goodBit(Good::Bread) | goodBit(Good::Ingot); break;
    case Specialty::Herding: n |= goodBit(Good::Grain) | goodBit(Good::Produce); break;
    default: break;
  }
  if (big) n = (n & ~goodBit(Good::Tools)) | goodBit(Good::Grain) | goodBit(Good::Ore) | goodBit(Good::Logs) | goodBit(Good::Wool) |
               goodBit(Good::Hides) | goodBit(Good::Livestock);   // a town's workshops buy the countryside's raw goods
  return n & ~specialtyProduces(s, big);
}

// Market stall trades (the stall props art::Prop::StallProduce ... StallTimber, in this order)
enum class StallTrade : uint8_t { Produce, Fish, Cloth, Pottery, Meat, Bread, Tools, Timber, COUNT };

// (M1 fixer) when a market stall packs up for the night: an hour between 18:30 and 21:30 by the stall's global tile
// (the keepers of a market do not all leave at once). The view lights the stall's lantern until then at dusk; the
// keeper leaves the counter after it and is back at 7:00.
inline float stallCloseHour(int32_t gx, int32_t gy) {
  uint32_t h = (uint32_t)gx * 374761393u + (uint32_t)gy * 668265263u + 0x5747u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return 18.5f + (float)(h % 13u) * 0.25f;
}
inline bool stallOpen(int32_t gx, int32_t gy, float hour) { return hour >= 7.0f && hour < stallCloseHour(gx, gy); }

// (M1 fixer round 2) how a market's pieces look, by their global tile. Shared by the settlement generator (which picks
// the tiles so that no two open tables or cloths of one town sell the same goods) and the view (which paints them).
inline uint32_t marketLookHash(int32_t gx, int32_t gy) {
  uint32_t h = (uint32_t)gx * 2246822519u ^ ((uint32_t)gy * 3266489917u + 0x6D2B79F5u);
  h = (h ^ (h >> 15)) * 2654435761u;
  h ^= h >> 13;
  h *= 0x85EBCA77u;
  return h ^ (h >> 16);
}
inline int tableGoodsAt(int32_t gx, int32_t gy) { return (int)(marketLookHash(gx, gy) % (uint32_t)art::kTableGoods); }
inline int tableShadeAt(int32_t gx, int32_t gy) { return (int)((marketLookHash(gx, gy) >> 8) % (uint32_t)art::kTableShades); }
inline int clothGoodsAt(int32_t gx, int32_t gy) { return (int)(marketLookHash(gx, gy) % (uint32_t)art::kClothGoods); }
inline int clothColourAt(int32_t gx, int32_t gy) { return (int)((marketLookHash(gx, gy) >> 8) % 4u); }
// every stall of a row is built alike (the cloth booths, the canvas tents or the shingled timber booths): by its row
inline int stallFormAt(int32_t gy) { return (int)((marketLookHash(0x51A11, gy) >> 4) % (uint32_t)art::kStallForms); }

}  // namespace ew
