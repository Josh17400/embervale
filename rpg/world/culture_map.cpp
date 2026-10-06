// M3 cultures on the land (VISION_PLAN 5.1, 5.6). CULTURE lane. Which culture a place belongs to, and the names that
// culture gives its settlements, kingdoms and dungeons. The cultures themselves live in rpg/culture (cult::Atlas).
//   - a kingdom's culture is the dialect of the family whose culture cell holds its seat (its capital);
//   - a place inside a kingdom takes that dialect;
//   - a place in the wildlands takes its own culture cell's FAMILY (the undialected parent style). A kingdom seated in
//     a cell is a dialect of that same family, so the wild country round a kingdom's heart already looks like its
//     cousins, and the look changes where it should: at kingdom borders and at culture-cell borders.
// Pure functions of (seed, coordinates): structural, integer maths only (this file builds with precise floats).
#include "rpg/world/gen.h"

#include "rpg/world/geology.h"

namespace ew {
using namespace gen;

cult::Atlas& EndlessSource::Impl::atlas() {
  if (!atlasPtr) {
    // the climate a family is born in: the coarse macro fields at its cell's centre (pure; no caches touched but the
    // geology province memo, itself a pure function). Two extras ride on the frozen ClimateSample:
    //   - coast: sea within ~700 tiles of the centre (fjord, river and imperial peoples like it);
    //   - the province's richest ore, packed into temp's top byte as ore + 1 (culture.cpp unpacks it): the ore the
    //     family's smiths favour (owner 15.11).
    atlasPtr = std::make_unique<cult::Atlas>(seed, [this](int32_t x, int32_t y) {
      cult::ClimateSample s;
      Coarse c = coarse(x, y);
      // a cell centred on the sea is peopled from its land: the first land point on rings out from the centre (fixed
      // order), so its family suits the shore it lives on (the coast flag still says "seafaring")
      if (c.e < ELEV_SEA) {
        static const int32_t ring[16][2] = {{500, 0}, {0, 500}, {-500, 0}, {0, -500}, {354, 354}, {-354, 354}, {-354, -354}, {354, -354},
                                            {900, 0}, {0, 900}, {-900, 0}, {0, -900}, {636, 636}, {-636, 636}, {-636, -636}, {636, -636}};
        for (const auto& o : ring) {
          const Coarse l = coarse(x + o[0], y + o[1]);
          if (l.e >= ELEV_SEA) { c = l; x += o[0]; y += o[1]; s.coast = true; break; }
        }
      }
      s.sea = c.e < ELEV_SEA;
      s.biome = (int)(c.rock > Q(0.5) && !s.sea ? Biome::Mountain : classify(c.e, c.t, c.m, x, y, s.sea));
      s.temp = c.t < 0 ? 0 : c.t > 0xFFFFFF ? 0xFFFFFF : c.t;
      s.moist = c.m;
      static const int32_t off[4][2] = {{700, 0}, {-700, 0}, {0, 700}, {0, -700}};
      for (const auto& o : off)
        if (coarse(x + o[0], y + o[1]).e < ELEV_SEA) s.coast = true;
      if (s.sea) s.coast = true;
      const Geology g = geology(x, y);
      s.temp |= (int32_t)(((uint32_t)g.primary() + 1u) << 24);
      return s;
    });
  }
  return *atlasPtr;
}

uint64_t EndlessSource::Impl::kingdomCulture(int32_t kx, int32_t ky) {
  KCell kc = kcell(kx, ky);
  if (!kc.capital) return 0;
  return cult::dialectId(floorDiv(kc.x, CCELL), floorDiv(kc.y, CCELL), kx, ky);
}

uint64_t EndlessSource::Impl::cultureAt(int32_t x, int32_t y) {
  const Gid k = kingdomAt(x, y);
  if (k) {
    const uint64_t c = kingdomCulture(idRx(k), idRy(k));
    if (c) return c;
  }
  return cult::familyId(floorDiv(x, CCELL), floorDiv(y, CCELL));
}

std::string EndlessSource::Impl::placeBaseName(const Node& n) {
  return cult::placeName(atlas().get(cultureAt(n.x, n.y)), n.seed ^ 0xA5A5u);
}

std::string EndlessSource::Impl::kingdomBaseName(int32_t kx, int32_t ky, uint32_t sd) {
  return cult::kingdomName(atlas().get(kingdomCulture(kx, ky)), sd);
}

// Dungeons, camps and shrines: about half keep the M2 biome-flavoured English names ("FROZEN BARROW", "HOWLING DEN":
// what travellers call them), half carry the local people's word ("VETHMAR BARROW", "TOMB OF ASKILD", "ORVAN'S
// HIDEOUT"); shrines are always to one of the culture's own gods. The dragon's peak keeps its per-world M2 name.
std::string EndlessSource::Impl::poiName(Rng& nr, SiteType t, int32_t x, int32_t y, Biome b) {
  if (t == SiteType::DragonLair) return makeDungeonName(nr, t, b);
  const uint32_t r = nr.next();
  const bool local = t == SiteType::Shrine || (r & 1u);
  if (!local) return makeDungeonName(nr, t, b);
  const cult::Culture& C = atlas().get(cultureAt(x, y));
  std::string s = cult::dungeonName(C, r ^ ((uint32_t)x * 73856093u) ^ ((uint32_t)y * 19349663u), (int)t);
  if (s.size() > 18) return makeDungeonName(nr, t, b);
  return s;
}

// ------------------------------------------------------------------ the public face (EndlessSource)
uint64_t EndlessSource::cultureAt(int32_t gx, int32_t gy) { return d_->cultureAt(gx, gy); }
const cult::Culture& EndlessSource::culture(uint64_t id) { return d_->atlas().get(id); }
cult::Atlas& EndlessSource::atlas() { return d_->atlas(); }

}  // namespace ew
