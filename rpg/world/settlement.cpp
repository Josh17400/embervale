// buildSettlement (rpg/world/settlement.h): the TOWNS lane's settlement generator on its local buffer. The work is done
// by ew::town::Gen (rpg/world/town_gen.h): town_layout.cpp (land, roads, squares, streets, walls, gates),
// town_build.cpp (services, homes by district, the capital's palace compound) and town_dress.cpp (squares, banners,
// signposts, yards, fields, archetypes, greenery, people, relief and the used mask).
// Scale (VISION_PLAN 15.8): villages 10-15 homes, towns 40-60, cities 160-220 with districts, several squares and
// markets, a curved wall with towers, gatehouses where the roads arrive and side gates.
#include "rpg/world/settlement.h"
#include <algorithm>
#include "rpg/world/town_gen.h"

namespace ew {

void settlementFootprint(SiteType type, Archetype arch, uint32_t seed, int& w, int& h) {
  // a little variety per site (even sizes keep the heart on a tile centre), sized so the scale targets fit with the
  // owner's organic spacing: frontage plots, gardens, squares, the V5 roof clearance, fields at the edge
  const uint32_t hs = hash32(seed ^ 0xF00715EEu);
  const int jw = (int)(hs % 3u) * 2, jh = (int)((hs >> 4) % 3u) * 2;
  switch (type) {
    case SiteType::City: w = 164 + jw; h = 130 + jh; break;   // room for the capital's palace compound in any city
    case SiteType::Town: w = 90 + jw; h = 72 + jh; break;
    default: w = 44 + jw; h = 34 + jh; break;
  }
  switch (arch) {
    case Archetype::Market: w += type == SiteType::Village ? 4 : 8; h += type == SiteType::Village ? 2 : 6; break;
    case Archetype::Farming: if (type != SiteType::City) { w += 6; h += 4; } break;   // more fields round the farms
    case Archetype::Port: case Archetype::Fishing: w += type == SiteType::City ? 12 : 6; h += type == SiteType::City ? 10 : 4; break;   // the water
    case Archetype::HillFort: if (type == SiteType::Town) { w += 8; h += 6; } break;   // the ring wall
    case Archetype::RiverCrossing: w += type == SiteType::City ? 8 : 4; h += type == SiteType::City ? 6 : 2; break;   // the river
    default: break;
  }
}

void buildSettlement(const SettlementCtx& ctx, SettlementOut& out) {
  out = SettlementOut();
  if (!ctx.plan) return;
  out.gx = ctx.plan->gx - town::MARGIN;
  out.gy = ctx.plan->gy - town::MARGIN;
  town::Gen g(ctx, out);
  g.run();
}

}  // namespace ew
