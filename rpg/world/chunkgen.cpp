// L2b, the chunk pipeline (VISION_PLAN 2.5), in a fixed order: base land, rivers and lakes, settlement buffers (cached),
// site and den stamps, roads and tracks (bridges, fords), relief (levels, cliff faces, ramps), vegetation. WORLD lane.
//
// Every step reads only plans and pure functions, so chunks come out the same in any order. The base land is computed
// for the chunk plus a 2-tile border, roads and relief bits for a 1-tile border: decisions that look at neighbours
// (cliffs, ramps, keeping trees off ramps and roads) see the same picture on both sides of a chunk edge, so there are
// no seams.
//
// Relief contract with the VIEW lane (VISION_PLAN 11): every tile on the LOWER side of a level step (4-neighbour) gets
// Map::HEIGHT_CLIFF (not walkable) unless it is a ramp (Map::HEIGHT_RAMP, walkable). Ramps sit on a global lattice:
// along a face running east-west (the higher tile north or south) a ramp is where (x + offset(y / 16)) mod 16 < 3;
// along a face running north-south the same with x and y swapped. Every run of 14 or more tiles gets a 3-tile ramp,
// stacked cliffs line up into stairways, and roads and tracks turn every cliff they touch into a ramp. Settlements
// and sites stand on one level: the land is flattened under them and terraced back to its own level around them.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>
#ifndef __EMSCRIPTEN__
#include <mutex>
#endif
#include "rpg/world/gen.h"
#include "rpg/world/town_gen.h"

namespace ew {
using namespace gen;
using art::Monster;
using art::Prop;

namespace {
constexpr int BR = 2;                         // base border
constexpr int BW = CHUNK + 2 * BR;            // 36
constexpr int RW = CHUNK + 2;                 // relief / road grid (1-tile border): 34
inline bool waterG(uint8_t g) { return g == (uint8_t)Ground::Water || g == (uint8_t)Ground::DeepWater; }

// (M3 towns lane, carry-over 2) Settlements shared between the sources of one process. Natively the prefetcher's
// worker has its own EndlessSource (one instance is not thread-safe), so every town near the player was built twice,
// once per source, and the worker's first city on a cold source (20-40 ms) is what let the first shifts of a walk fall
// behind. A finished town is immutable and a pure function of (world seed, site), so the sources hand finished ones to
// each other through this small locked store (LRU, 24 towns). The web has one source and no threads: no store.
#ifndef __EMSCRIPTEN__
struct SharedTowns {
  std::mutex mu;
  struct E { std::shared_ptr<SettlementOut> out; uint64_t used = 0; };
  std::unordered_map<uint64_t, E> m;
  uint64_t clock = 0;
};
SharedTowns& sharedTowns() {
  static SharedTowns s;
  return s;
}
uint64_t sharedTownKey(uint64_t seed, Gid id) { return mix64(seed * 0x9E3779B97F4A7C15ull ^ id ^ 0x70A5E7ull); }
std::shared_ptr<SettlementOut> sharedTownGet(uint64_t seed, Gid id) {
  SharedTowns& s = sharedTowns();
  std::lock_guard<std::mutex> lk(s.mu);
  auto it = s.m.find(sharedTownKey(seed, id));
  if (it == s.m.end()) return nullptr;
  it->second.used = ++s.clock;
  return it->second.out;
}
void sharedTownPut(uint64_t seed, Gid id, std::shared_ptr<SettlementOut> out) {
  SharedTowns& s = sharedTowns();
  std::lock_guard<std::mutex> lk(s.mu);
  if (s.m.size() >= 24) {
    auto oldest = s.m.begin();
    for (auto i = s.m.begin(); i != s.m.end(); ++i) if (i->second.used < oldest->second.used) oldest = i;
    s.m.erase(oldest);
  }
  s.m[sharedTownKey(seed, id)] = SharedTowns::E{std::move(out), ++s.clock};
}
#endif

// ------------------------------------------------------------------ M3c Wildlands: the flora of each biome
// What one eco grows on one tile. r (Q16, the tile's main roll) picks what grows: the solid things (trees, then rocks
// and shrubs) always come from the low end of r, so the forest rule's candidates and the ecotones can reason about a
// tile without knowing its neighbours. q and pk vary the pick, dens is the woods' slow density noise, thin the
// highland thinning (1.0 in the lowlands, falling above level 4). g is the tile's ground: trees stand in the shallow
// water of a mangrove coast or a flooded wood, lily pads float on marsh pools, nothing grows on rock, lava or deep
// water. (x, y, vs: the few palettes that cluster their props draw a cluster noise.)
Prop ecoFlora(Eco e, int32_t r, int32_t q, uint64_t pk, int32_t dens, int32_t thin, int lv, Ground g, bool river, int32_t x, int32_t y, uint64_t vs) {
  auto pick = [&](std::initializer_list<Prop> l) { return *(l.begin() + (size_t)(pk % l.size())); };
  auto tr = [&](int32_t base, int32_t dmul) { return r < qm(base + qm(dens, dmul), thin); };
  auto open = [&](int32_t base) { return r < qm(base + std::max(0, (dens - Q(0.60)) / 2), thin); };   // lone trees on open land
  (void)lv;
  // (the open lands' ground covers grow in drifts, not an even sprinkle: above the solids' band (r >= 0.06) the roll is
  // stretched where the density noise is low (bare patches) and squeezed where it is high (deep drifts of grass,
  // heather or flowers). The woods keep r as it is: their undergrowth is picked by q.)
  int32_t rc = r;
  if (!ecoHas(e, EF_WOODED) && r > Q(0.06)) {
    // (M3c fixer round 3, review: "the same golden grass tuft stamped dozens of times at even spacing ... an even
    // sprinkle of identical clumps") a stronger swing: wider bare glades and denser drifts
    const int32_t mod = clampi(Q_ONE + 7 * (dens - Q(0.5)), Q(0.10), Q(2.6));
    rc = (int32_t)std::min<int64_t>(Q_ONE, Q(0.06) + ((int64_t)(r - Q(0.06)) << 16) / mod);
  }
  if (g == Ground::Water) {
    switch (e) {
      case Eco::Mangrove: if (!river && r < Q(0.20)) return Prop::MangroveTree; return Prop::COUNT;
      case Eco::FloodedForest: if (!river && r < Q(0.17)) return Prop::SwampCypress; if (r < Q(0.32)) return Prop::LilyPad; return Prop::COUNT;
      case Eco::ReedMarsh: case Eco::PeatBog: case Eco::TaigaBog: return rc < Q(0.14) ? Prop::LilyPad : Prop::COUNT;
      case Eco::Oasis: return rc < Q(0.10) ? Prop::LilyPad : Prop::COUNT;
      case Eco::Ocean: return Prop::COUNT;
      default: return r < Q(0.025) && !river ? Prop::LilyPad : Prop::COUNT;
    }
  }
  if (g == Ground::Ice) return e == Eco::Glacier && r < Q(0.012) ? Prop::IceSerac : Prop::COUNT;
  if (groundSolid(g)) return Prop::COUNT;
  switch (e) {
    // ---- grasslands
    case Eco::Meadow:
      if (open(Q(0.010))) return pick({Prop::OakTree, Prop::OakTree2, Prop::BirchTree});
      if (rc < Q(0.07)) return pick({Prop::Flowers1, Prop::Flowers2, Prop::Flowers3});
      if (rc < Q(0.16)) return Prop::TallGrass;
      if (rc < Q(0.172)) return pick({Prop::Bush, Prop::BerryBush});
      if (rc < Q(0.178) + (lv >= 4 ? Q(0.01) : 0)) return pick({Prop::Rock, Prop::Boulder});
      return Prop::COUNT;
    case Eco::FlowerMeadow:
      if (open(Q(0.006))) return pick({Prop::OakTree, Prop::BirchTree, Prop::BlossomTree});
      if (rc < Q(0.22)) return Prop::Wildflowers;
      if (rc < Q(0.29)) return pick({Prop::Flowers1, Prop::Flowers2, Prop::Flowers3});
      if (rc < Q(0.35)) return Prop::TallGrass;
      if (rc < Q(0.358)) return Prop::BerryBush;
      return Prop::COUNT;
    case Eco::Prairie:
      if (open(Q(0.004))) return pick({Prop::OakTree, Prop::OakTree2});
      if (r < Q(0.010)) return Prop::Bush;
      if (r < Q(0.020)) return Prop::DryBrush;
      if (rc < Q(0.29)) return Prop::PrairieGrass;
      if (rc < Q(0.33)) return Prop::TallGrass;
      if (rc < Q(0.35)) return pick({Prop::Flowers2, Prop::Wildflowers});
      return Prop::COUNT;
    case Eco::Steppe:
      if (r < qm(Q(0.003), thin)) return Prop::JuniperTree;
      if (r < Q(0.010)) return Prop::Thornbush;
      if (r < Q(0.016)) return pick({Prop::Boulder, Prop::Rock});
      if (r < Q(0.05)) return Prop::DryBrush;
      if (rc < Q(0.19)) return Prop::PrairieGrass;
      return Prop::COUNT;
    case Eco::Savanna:
      // acacias stand alone 1-3 % (a rare baobab), termite mounds, golden grass
      if (r < qm(Q(0.012) + std::max(0, dens - Q(0.55)) / 3, thin)) return (pk % 11) == 0 ? Prop::BaobabTree : Prop::AcaciaTree;
      if (r < Q(0.024)) return Prop::TermiteMound;
      if (r < Q(0.040)) return Prop::Thornbush;
      if (rc < Q(0.20)) return Prop::PrairieGrass;
      if (rc < Q(0.23)) return Prop::DryBrush;
      return Prop::COUNT;
    case Eco::Heath:
      if (r < qm(Q(0.006), thin)) return Prop::JuniperTree;
      if (r < Q(0.026)) return Prop::Gorse;
      if (r < Q(0.034)) return Prop::MossRock;
      if (rc < Q(0.25)) return Prop::Heather;
      if (rc < Q(0.31)) return Prop::TallGrass;
      return Prop::COUNT;
    case Eco::ChalkDowns:
      if (r < qm(Q(0.004), thin)) return Prop::JuniperTree;
      if (r < Q(0.016)) return Prop::Gorse;
      if (r < Q(0.020)) return Prop::Rock;
      if (rc < Q(0.09)) return Prop::Wildflowers;
      if (rc < Q(0.13)) return Prop::TallGrass;
      return Prop::COUNT;
    case Eco::AlpineMeadow: {
      // (M4, owner carry-over: the alpine meadow's flora repeated) the blooms grow in drifts (a slow cluster noise):
      // thick in the drifts and among the stones, almost none on the open turf between them; the drifts mix the
      // wildflower cushions with the classic flowers and cotton grass, lichen and moss on the stony ground
      if (r < qm(Q(0.012), thin)) return Prop::LarchTree;
      if (r < Q(0.030)) return pick({Prop::Boulder, Prop::Rock, Prop::MossRock, Prop::Rock});
      const int32_t drift = vnoiseQ(x, y, 7, vs ^ 0xA1F1u) + (vnoiseQ(x, y, 3, vs ^ 0xA1F3u) - Q(0.5)) / 3;
      if (drift > Q(0.53)) {
        if (rc < Q(0.26)) return Prop::Wildflowers;
        if (rc < Q(0.34)) return pick({Prop::Flowers1, Prop::Flowers2, Prop::Flowers3});
        if (rc < Q(0.38)) return Prop::CottonGrass;
      } else if (drift > Q(0.38)) {
        if (rc < Q(0.07)) return Prop::Wildflowers;
        if (rc < Q(0.10)) return Prop::TallGrass;
      } else if (rc < Q(0.015)) return Prop::Wildflowers;
      if (rc < Q(0.47) && rc >= Q(0.41)) return Prop::Lichen;
      if (rc < Q(0.50) && rc >= Q(0.47)) return Prop::TallGrass;
      return Prop::COUNT;
    }
    case Eco::StonePlains: {
      // standing stones in groups (rings and rows where a cluster noise peaks), long grass and heather between
      const int32_t cl = vnoiseQ(x, y, 3, vs ^ 0x5701u);
      const int32_t ring = cl > Q(0.70) ? Q(0.13) : Q(0.002);
      if (r < ring) return Prop::StandingStone;
      if (r < ring + Q(0.008)) return Prop::Boulder;
      if (rc < Q(0.26)) return Prop::TallGrass;
      if (rc < Q(0.30)) return pick({Prop::Heather, Prop::Lichen});
      return Prop::COUNT;
    }
    case Eco::LakeDistrict:
      if (open(Q(0.014))) return pick({Prop::BirchTree, Prop::BirchTree, Prop::OakTree});
      if (rc < Q(0.07)) return pick({Prop::Flowers1, Prop::Flowers3, Prop::Wildflowers});
      if (rc < Q(0.19)) return Prop::TallGrass;
      if (rc < Q(0.20)) return Prop::Bush;
      return Prop::COUNT;
    case Eco::Blight:
      if (r < qm(Q(0.05), thin)) return pick({Prop::GnarledTree, Prop::GnarledTree, Prop::DeadTree});
      if (r < Q(0.058)) return Prop::Rock;
      if (rc < Q(0.17)) return Prop::Blightweed;
      if (rc < Q(0.19)) return pick({Prop::Bones, Prop::Mushrooms});
      return Prop::COUNT;
    // ---- woods (the tree odds are high: the spacing rule keeps at most one tree in every few tiles)
    case Eco::MixedForest:
      if (tr(Q(0.28), Q(0.40))) return pick({Prop::OakTree, Prop::OakTree2, Prop::OakTree, Prop::BirchTree});
      if (r < Q(0.70)) { if (q < Q(0.22)) return Prop::Fern; if (q < Q(0.25)) return pick({Prop::Bush, Prop::Mushrooms}); if (q < Q(0.27)) return pick({Prop::Stump, Prop::Log, Prop::MossRock}); if (q < Q(0.45)) return Prop::TallGrass; }
      return Prop::COUNT;
    case Eco::BirchWood:
      if (tr(Q(0.26), Q(0.38))) return pick({Prop::BirchTree, Prop::BirchTree, Prop::BirchTree, Prop::BirchTree, Prop::OakTree2});
      if (r < Q(0.70)) { if (q < Q(0.26)) return Prop::Fern; if (q < Q(0.28)) return pick({Prop::Stump, Prop::MossRock}); if (q < Q(0.48)) return Prop::TallGrass; if (q < Q(0.54)) return pick({Prop::Flowers1, Prop::Wildflowers}); }
      return Prop::COUNT;
    case Eco::GiantForest:
      // the giants stand wide apart; ferns, fallen logs and mossy stones fill the ground between
      if (tr(Q(0.12), Q(0.14))) return (pk % 5) == 0 ? Prop::OakTree2 : Prop::GiantTree;
      if (r < Q(0.80)) { if (q < Q(0.36)) return Prop::Fern; if (q < Q(0.39)) return pick({Prop::Log, Prop::MossRock, Prop::Stump}); if (q < Q(0.41)) return Prop::Mushrooms; if (q < Q(0.56)) return Prop::TallGrass; }
      return Prop::COUNT;
    case Eco::DarkForest:
      if (tr(Q(0.26), Q(0.36))) return pick({Prop::GnarledTree, Prop::GnarledTree, Prop::GnarledTree, Prop::DeadTree});
      if (r < Q(0.70)) { if (q < Q(0.06)) return Prop::Mushrooms; if (q < Q(0.26)) return Prop::Fern; if (q < Q(0.32)) return pick({Prop::Stump, Prop::Log}); if (q < Q(0.36)) return Prop::Bones; }
      return Prop::COUNT;
    case Eco::BlossomGrove:
      if (tr(Q(0.18), Q(0.26))) return (pk % 6) == 0 ? Prop::OakTree : Prop::BlossomTree;
      if (r < Q(0.75)) { if (q < Q(0.34)) return Prop::Petals; if (q < Q(0.46)) return Prop::Wildflowers; if (q < Q(0.60)) return Prop::TallGrass; if (q < Q(0.61)) return Prop::Bush; }
      return Prop::COUNT;
    case Eco::BambooForest:
      if (tr(Q(0.32), Q(0.36))) return Prop::BambooClump;
      if (r < Q(0.72)) { if (q < Q(0.34)) return Prop::JungleFern; if (q < Q(0.36)) return Prop::MossRock; if (q < Q(0.50)) return Prop::TallGrass; }
      return Prop::COUNT;
    case Eco::Jungle:
      if (tr(Q(0.30), Q(0.40))) { const uint64_t k = pk % 8; return k < 6 ? Prop::JungleTree : k == 6 ? Prop::PalmTree : Prop::OakTree2; }
      if (r < Q(0.85)) { if (q < Q(0.42)) return Prop::JungleFern; if (q < Q(0.52)) return Prop::Fern; if (q < Q(0.54)) return pick({Prop::Bush, Prop::BerryBush}); if (q < Q(0.66)) return Prop::TallGrass; }
      return Prop::COUNT;
    case Eco::MushroomForest:
      if (tr(Q(0.22), Q(0.30))) return (pk % 7) == 0 ? Prop::GnarledTree : Prop::GiantMushroom;
      if (r < Q(0.72)) { if (q < Q(0.26)) return Prop::GlowCaps; if (q < Q(0.42)) return Prop::Mushrooms; if (q < Q(0.56)) return Prop::Fern; }
      return Prop::COUNT;
    case Eco::Silverwood:
      if (tr(Q(0.26), Q(0.36))) return (pk % 6) == 0 ? Prop::BirchTree : Prop::SilverTree;
      if (r < Q(0.72)) { if (q < Q(0.34)) return Prop::SilverFern; if (q < Q(0.42)) return Prop::Wildflowers; if (q < Q(0.56)) return Prop::TallGrass; }
      return Prop::COUNT;
    case Eco::AutumnWood:
      if (tr(Q(0.26), Q(0.38))) return pick({Prop::AutumnTree, Prop::AutumnTree, Prop::BirchTree});
      if (r < Q(0.70)) { if (q < Q(0.14)) return Prop::Fern; if (q < Q(0.20)) return pick({Prop::Bush, Prop::Mushrooms, Prop::Flowers3}); if (q < Q(0.21)) return Prop::Stump; if (q < Q(0.30)) return Prop::TallGrass; }
      return Prop::COUNT;
    // ---- cold
    case Eco::Taiga:
      if (tr(Q(0.20), Q(0.36))) return pick({Prop::PineTree, Prop::PineTree2});
      if (r < Q(0.60)) { if (q < Q(0.07)) return Prop::Fern; if (q < Q(0.09)) return pick({Prop::MossRock, Prop::Boulder, Prop::Stump}); if (q < Q(0.16)) return Prop::TallGrass; }
      return Prop::COUNT;
    case Eco::TaigaBog:
      if (tr(Q(0.10), Q(0.16))) return pick({Prop::LarchTree, Prop::LarchTree, Prop::PineTree2});
      if (r < Q(0.15)) return Prop::DeadTree;
      if (r < Q(0.42)) return Prop::CottonGrass;
      if (r < Q(0.52)) return Prop::Reeds;
      if (r < Q(0.53)) return Prop::MossRock;
      return Prop::COUNT;
    case Eco::SnowField:
      if (tr(Q(0.04), Q(0.22))) return Prop::SnowPine;
      if (r < Q(0.4)) { if (q < Q(0.03)) return Prop::SnowRock; if (q < Q(0.05)) return Prop::SnowBush; }
      return Prop::COUNT;
    case Eco::Tundra:
      if (r < qm(Q(0.004), thin)) return Prop::SnowPine;
      if (r < Q(0.014)) return pick({Prop::SnowRock, Prop::Boulder});
      if (r < Q(0.020)) return Prop::SnowBush;
      if (rc < Q(0.13)) return Prop::Lichen;   // (fixer r2: thinner, so the floor is not a carpet of one stone)
      if (rc < Q(0.24)) return Prop::CottonGrass;
      return Prop::COUNT;
    case Eco::Glacier:
      if (r < Q(0.020)) return Prop::IceSerac;
      if (r < Q(0.030)) return Prop::SnowRock;
      return Prop::COUNT;
    case Eco::FrozenLakes:
      if (r < qm(Q(0.03), thin)) return Prop::SnowPine;
      if (r < Q(0.040)) return Prop::SnowRock;
      if (r < Q(0.050)) return Prop::SnowBush;
      if (rc < Q(0.08)) return Prop::Lichen;
      return Prop::COUNT;
    // ---- wet
    case Eco::ReedMarsh:
      if (r < Q(0.04) + qm(dens, Q(0.06))) return pick({Prop::WillowTree, Prop::WillowTree, Prop::DeadTree});
      if (r < Q(0.52)) { if (q < Q(0.55)) return Prop::Reeds; if (q < Q(0.60)) return Prop::Mushrooms; if (q < Q(0.75)) return Prop::TallGrass; }
      return Prop::COUNT;
    case Eco::PeatBog:
      if (r < Q(0.025)) return Prop::DeadTree;
      if (rc < Q(0.24)) return Prop::CottonGrass;
      if (rc < Q(0.33)) return Prop::Heather;
      if (rc < Q(0.47)) return Prop::Reeds;
      if (rc < Q(0.49)) return Prop::Mushrooms;
      return Prop::COUNT;
    case Eco::Mangrove:
      if (r < Q(0.18) + qm(dens, Q(0.10))) return Prop::MangroveTree;
      if (r < Q(0.45)) { if (q < Q(0.40)) return Prop::Reeds; if (q < Q(0.60)) return Prop::JungleFern; }
      return Prop::COUNT;
    case Eco::FloodedForest:
      if (r < Q(0.12) + qm(dens, Q(0.14))) return (pk % 5) == 0 ? Prop::WillowTree : Prop::SwampCypress;
      if (r < Q(0.60)) { if (q < Q(0.35)) return Prop::Reeds; if (q < Q(0.45)) return Prop::Fern; if (q < Q(0.50)) return Prop::Mushrooms; }
      return Prop::COUNT;
    // ---- dry
    case Eco::Dunes:
      if (r < Q(0.002)) return Prop::Cactus;
      if (r < Q(0.008)) return Prop::DryBrush;
      return Prop::COUNT;
    case Eco::StonyDesert:
      if (r < Q(0.020)) return pick({Prop::Rock, Prop::Boulder, Prop::Rock});
      if (r < Q(0.030)) return Prop::Cactus;
      if (rc < Q(0.070)) return Prop::Agave;
      if (rc < Q(0.095)) return Prop::DryBrush;
      return Prop::COUNT;
    case Eco::Badlands: {
      // hoodoos in clusters, scrub between
      const int32_t cl = vnoiseQ(x, y, 3, vs ^ 0xB0D0u);
      const int32_t hood = cl > Q(0.62) ? Q(0.10) : Q(0.004);
      if (r < hood) return Prop::Hoodoo;
      if (r < hood + Q(0.016)) return Prop::Thornbush;
      if (r < hood + Q(0.024)) return Prop::Rock;
      if (rc < Q(0.17)) return Prop::DryBrush;
      return Prop::COUNT;
    }
    case Eco::SaltFlats:
      if (r < Q(0.003)) return Prop::Saltbush;
      return Prop::COUNT;
    case Eco::Scrubland:
      if (r < qm(Q(0.010), thin)) return Prop::AcaciaTree;
      if (r < Q(0.045)) return Prop::Thornbush;
      if (r < Q(0.050)) return Prop::Rock;
      if (rc < Q(0.090)) return Prop::Agave;
      if (rc < Q(0.17)) return Prop::DryBrush;
      if (rc < Q(0.26)) return Prop::PrairieGrass;
      return Prop::COUNT;
    case Eco::Oasis:
      if (r < Q(0.14) + qm(dens, Q(0.08))) return (pk % 6) == 0 ? Prop::AcaciaTree : Prop::PalmTree;
      if (r < Q(0.17)) return Prop::Bush;
      if (r < Q(0.46)) return Prop::TallGrass;
      if (r < Q(0.52)) return Prop::Reeds;
      return Prop::COUNT;
    case Eco::AshFields:
      if (r < Q(0.035)) return Prop::BasaltColumns;
      if (r < Q(0.050)) return Prop::AshVent;
      if (r < Q(0.056)) return Prop::Rock;
      return Prop::COUNT;
    case Eco::CrystalBarrens:
      if (r < Q(0.040)) return Prop::CrystalSpire;
      if (r < Q(0.050)) return pick({Prop::Rock, Prop::Boulder});
      if (rc < Q(0.090)) return Prop::Lichen;
      return Prop::COUNT;
    case Eco::PetrifiedForest:
      if (tr(Q(0.045), Q(0.06))) return Prop::PetrifiedTree;
      if (r < Q(0.15)) return Prop::Rock;
      if (r < Q(0.20)) return Prop::DryBrush;
      return Prop::COUNT;
    // ---- coast
    case Eco::SandBeach:
      if (r < Q(0.020)) return Prop::Rock;
      if (r < Q(0.045)) return Prop::Reeds;
      if (rc < Q(0.075)) return Prop::Wrack;
      return Prop::COUNT;
    case Eco::Shingle:
      if (r < Q(0.035)) return pick({Prop::Rock, Prop::Boulder, Prop::Rock});
      if (rc < Q(0.15)) return Prop::Wrack;
      return Prop::COUNT;
    case Eco::CoralCoast:
      if (r < qm(Q(0.025), thin)) return Prop::PalmTree;
      if (rc < Q(0.10)) return Prop::Shells;
      if (rc < Q(0.12)) return Prop::Wrack;
      return Prop::COUNT;
    case Eco::SeaCliffs:
      if (r < qm(Q(0.014), thin)) return Prop::JuniperTree;
      if (r < Q(0.030)) return Prop::BasaltColumns;
      if (r < Q(0.050)) return pick({Prop::Rock, Prop::Boulder});
      if (rc < Q(0.26)) return Prop::TallGrass;
      if (rc < Q(0.32)) return Prop::Heather;
      return Prop::COUNT;
    default: return Prop::COUNT;
  }
}

// the undergrowth a tree leaves when the forest rule gives its tile to a neighbour (u: 0..99)
Prop ecoUnder(Eco e, uint32_t u) {
  switch (e) {
    case Eco::MixedForest: return u < 38 ? Prop::Fern : u < 66 ? Prop::TallGrass : u < 68 ? Prop::Mushrooms : u < 72 ? Prop::Flowers3 : Prop::COUNT;
    case Eco::BirchWood: return u < 30 ? Prop::Fern : u < 60 ? Prop::TallGrass : u < 68 ? Prop::Flowers1 : Prop::COUNT;
    case Eco::GiantForest: return u < 50 ? Prop::Fern : u < 56 ? Prop::Mushrooms : u < 70 ? Prop::TallGrass : Prop::COUNT;
    case Eco::DarkForest: return u < 7 ? Prop::Mushrooms : u < 40 ? Prop::Fern : u < 52 ? Prop::TallGrass : Prop::COUNT;
    case Eco::BlossomGrove: return u < 45 ? Prop::Petals : u < 60 ? Prop::Wildflowers : u < 75 ? Prop::TallGrass : Prop::COUNT;
    case Eco::BambooForest: return u < 45 ? Prop::JungleFern : u < 65 ? Prop::TallGrass : Prop::COUNT;
    case Eco::Jungle: return u < 55 ? Prop::JungleFern : u < 70 ? Prop::Fern : u < 80 ? Prop::TallGrass : Prop::COUNT;
    case Eco::MushroomForest: return u < 30 ? Prop::GlowCaps : u < 55 ? Prop::Mushrooms : u < 70 ? Prop::Fern : Prop::COUNT;
    case Eco::Silverwood: return u < 45 ? Prop::SilverFern : u < 55 ? Prop::Wildflowers : u < 70 ? Prop::TallGrass : Prop::COUNT;
    case Eco::AutumnWood: return u < 32 ? Prop::Fern : u < 38 ? Prop::Mushrooms : u < 48 ? Prop::Flowers3 : u < 68 ? Prop::TallGrass : Prop::COUNT;
    case Eco::Taiga: return u < 30 ? Prop::Fern : u < 50 ? Prop::TallGrass : Prop::COUNT;
    case Eco::TaigaBog: return u < 35 ? Prop::CottonGrass : u < 50 ? Prop::Reeds : u < 60 ? Prop::TallGrass : Prop::COUNT;
    case Eco::ReedMarsh: case Eco::PeatBog: case Eco::Mangrove: case Eco::FloodedForest:
      return u < 35 ? Prop::Reeds : u < 50 ? Prop::Mushrooms : u < 65 ? Prop::TallGrass : Prop::COUNT;
    case Eco::Savanna: case Eco::Prairie: case Eco::Scrubland: return u < 50 ? Prop::PrairieGrass : Prop::COUNT;
    case Eco::Oasis: return u < 40 ? Prop::TallGrass : u < 50 ? Prop::Reeds : Prop::COUNT;
    case Eco::Blight: return u < 40 ? Prop::Blightweed : u < 50 ? Prop::Mushrooms : Prop::COUNT;
    case Eco::PetrifiedForest: return u < 20 ? Prop::DryBrush : Prop::COUNT;
    case Eco::AlpineMeadow: return u < 30 ? Prop::Wildflowers : u < 50 ? Prop::Lichen : Prop::COUNT;
    case Eco::Meadow: case Eco::FlowerMeadow: case Eco::LakeDistrict: case Eco::ChalkDowns:
      return u < 40 ? Prop::TallGrass : u < 55 ? Prop::Flowers1 : Prop::COUNT;
    default: return Prop::COUNT;
  }
}

// grass and reeds by the water's edge suit the temperate and wet lands (not the deserts' sand, the snow, the coasts)
bool watersideFlora(Eco e) {
  switch (ecoFamily(e)) {
    case Biome::Desert: return e == Eco::Oasis;
    case Biome::Snow: case Biome::Ocean: case Biome::Beach: case Biome::Mountain: return false;
    default: return e != Eco::Blight;
  }
}

// (M3c) half the ecotone band's width (tiles) between two ecos: wide and soft between grasslands and toward the
// deserts, narrow at marsh edges, middling between woods and open land (at most 4: the chunk's eco grid reaches 4)
int ecoHalfW(Eco a, Eco b) {
  const Biome fa = ecoFamily(a), fb = ecoFamily(b);
  if (fa == Biome::Swamp || fb == Biome::Swamp) return 2;
  const bool wa = ecoHas(a, EF_WET) && fa != Biome::Plains, wb = ecoHas(b, EF_WET) && fb != Biome::Plains;
  if (wa || wb) return 2;
  if (ecoHas(a, EF_RARE) || ecoHas(b, EF_RARE)) return 3;
  // (M4, owner carry-over: the heath "read as flat green blobs") the moor meets the grass along a tighter, ragged line:
  // a wide band scattered islands of bright turf over the dark heather
  if (a == Eco::Heath || b == Eco::Heath) return 2;
  const bool ga = fa == Biome::Plains, gb = fb == Biome::Plains;
  if (ga && gb) return 4;
  if ((ga && fb == Biome::Desert) || (gb && fa == Biome::Desert)) return 4;
  if (fa == Biome::Desert && fb == Biome::Desert) return 3;
  const bool woodA = ecoHas(a, EF_WOODED), woodB = ecoHas(b, EF_WOODED);
  if (woodA && woodB) return 3;
  if (woodA != woodB) return 3;
  if (fa == Biome::Desert || fb == Biome::Desert) return 3;
  return 2;
}
}  // namespace

// ------------------------------------------------------------------ steps 1-2: base land, rivers, lakes
void EndlessSource::Impl::baseRect(int32_t x0, int32_t y0, int w, int h, BaseRect& B) {
  baseRectRows(x0, y0, w, h, B, 0, h);
  baseRectWater(B);
}

void EndlessSource::Impl::baseRectRows(int32_t x0, int32_t y0, int w, int h, BaseRect& B, int yFrom, int yTo) {
  if (yFrom == 0) {
    B.x0 = x0; B.y0 = y0; B.w = w; B.h = h;
    const size_t n = (size_t)w * h;
    B.ground.assign(n, 0); B.biome.assign(n, 0); B.level.assign(n, 0); B.riverW.assign(n, 0); B.lake.assign(n, 0); B.bridge.assign(n, 0);
    B.eco.assign(n, 0);
  }
  for (int y = std::max(0, yFrom); y < std::min(h, yTo); y++)
    for (int x = 0; x < w; x++) {
      TileF f = tile(x0 + x, y0 + y);
      size_t i = (size_t)y * w + x;
      B.biome[i] = (uint8_t)f.biome;
      B.eco[i] = (uint8_t)f.eco;
      B.ground[i] = (uint8_t)groundFor(f.eco, f.e, f.t, x0 + x, y0 + y);
      B.level[i] = f.h;
    }
}

void EndlessSource::Impl::baseRectWater(BaseRect& B) {
  const int32_t x0 = B.x0, y0 = B.y0;
  const int w = B.w, h = B.h;
  const size_t n = (size_t)w * h;
  std::vector<uint8_t> deep(n, 0), path(n, 0);
  const int32_t x1 = x0 + w, y1 = y0 + h;
  for (int32_t ry = regionOf(y0 - 4); ry <= regionOf(y1 + 3); ry++)
    for (int32_t rx = regionOf(x0 - 4); rx <= regionOf(x1 + 3); rx++) {
      std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
      for (const RiverSeg& s : H->segs) {
        if (std::max(s.x0, s.x1) + 3 < x0 || std::min(s.x0, s.x1) - 3 >= x1 || std::max(s.y0, s.y1) + 3 < y0 || std::min(s.y0, s.y1) - 3 >= y1) continue;
        const int lo = s.w >= 3 ? -1 : 0, hi = s.w == 1 ? 0 : s.w == 4 ? 2 : 1;
        walk4(s.x0, s.y0, s.x1, s.y1, [&](int32_t x, int32_t y) {
          for (int oy = lo; oy <= hi; oy++)
            for (int ox = lo; ox <= hi; ox++) {
              if (!B.in(x + ox, y + oy)) continue;
              size_t i = B.at(x + ox, y + oy);
              if (B.biome[i] == (uint8_t)Biome::Ocean) continue;
              B.riverW[i] = std::max(B.riverW[i], s.w);
              if (s.w >= 3 && ox == 0 && oy == 0) deep[i] = 1;
            }
        });
      }
      for (const Lake& L : H->lakes) {
        const int32_t R = 2 * L.r + 2;
        if (L.x + R < x0 || L.x - R >= x1 || L.y + R < y0 || L.y - R >= y1) continue;
        if (lakeUnderSettlement(L)) continue;
        const uint8_t lakeLv = (uint8_t)fineLevel(L.x, L.y);
        for (int32_t y = std::max(y0, L.y - R); y < std::min(y1, L.y + R + 1); y++)
          for (int32_t x = std::max(x0, L.x - R); x < std::min(x1, L.x + R + 1); x++) {
            int32_t rr = lakeRadius(L, x, y);
            int64_t d2 = dist2(x, y, L.x, L.y);
            if (d2 >= (int64_t)rr * rr) continue;
            size_t i = B.at(x, y);
            if (B.biome[i] == (uint8_t)Biome::Ocean) continue;
            if (!B.lake[i] || lakeLv < B.level[i]) B.level[i] = lakeLv;
            B.lake[i] = 1;
            if (d2 < (int64_t)(rr / 2) * (rr / 2)) deep[i] = 1;
          }
      }
    }
  // footbridges: rivers cannot be swum, so every few pieces of a river's course a plank bridge crosses it (about one
  // every 40-50 tiles; roads and tracks add their own). Chosen by a hash of the piece, so every rect agrees.
  for (int32_t ry = regionOf(y0 - 14); ry <= regionOf(y1 + 13); ry++)
    for (int32_t rx = regionOf(x0 - 14); rx <= regionOf(x1 + 13); rx++) {
      std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
      for (const RiverSeg& s : H->segs) {
        if (std::max(s.x0, s.x1) + 14 < x0 || std::min(s.x0, s.x1) - 14 >= x1 || std::max(s.y0, s.y1) + 14 < y0 || std::min(s.y0, s.y1) - 14 >= y1) continue;
        if (mix64(key2(s.x0, s.y0) ^ mix64(key2(s.x1, s.y1)) ^ seed ^ tag("b.bridge")) % 6 != 0) continue;
        int32_t len = idist(s.x0, s.y0, s.x1, s.y1);
        if (len < 5) continue;
        int32_t mx = floorDiv(s.x0 + s.x1, 2), my = floorDiv(s.y0 + s.y1, 2);
        // (M2 fixer round 2) no free footbridge within 28 tiles of a troll's toll bridge (it made the toll pointless)
        {
          bool toll = false;
          for (int k = 0; k < 9 && !toll; k++) {
            std::shared_ptr<const RegionData> TD = regionData(regionOf(mx) - 1 + k % 3, regionOf(my) - 1 + k / 3);
            for (const SitePlan& p : TD->plan.sites)
              if (p.type == SiteType::Vignette && p.kind == (uint8_t)VignetteKind::TollBridge && dist2(p.ex, p.ey, mx, my) <= 28ll * 28ll) { toll = true; break; }
          }
          if (toll) continue;
        }
        int32_t px = -(s.y1 - s.y0), py = s.x1 - s.x0, reach = s.w + 2;
        // (M3b fixer) the deck runs straight along the grid, the way nearest the river's normal (a slanted footbridge was a
        // staircase of planks jogging a tile mid-deck); it reaches as far as the slant would have
        if (std::abs(px) >= std::abs(py)) { px = px >= 0 ? len : -len; py = 0; reach = reach + (s.w + 1) / 2; }
        else { py = py >= 0 ? len : -len; px = 0; reach = reach + (s.w + 1) / 2; }
        walk4(mx - px * reach / len, my - py * reach / len, mx + px * reach / len, my + py * reach / len, [&](int32_t x, int32_t y) {
          if (!B.in(x, y)) return;
          size_t i = B.at(x, y);
          if (B.riverW[i] && !B.lake[i] && B.biome[i] != (uint8_t)Biome::Ocean) B.bridge[i] = 1;
        });
        // (M1 round 3) a trodden path leads on from both ends of the footbridge (it stood alone in the grass)
        const int32_t far = reach + 5;
        walk4(mx - px * far / len, my - py * far / len, mx + px * far / len, my + py * far / len, [&](int32_t x, int32_t y) {
          if (!B.in(x, y)) return;
          size_t i = B.at(x, y);
          if (!B.riverW[i] && !B.lake[i] && B.biome[i] != (uint8_t)Biome::Ocean) path[i] = 1;
        });
      }
    }
  for (size_t i = 0; i < n; i++) {
    if (path[i] && !B.bridge[i] && !B.lake[i] && !B.riverW[i] && !groundSolid((Ground)B.ground[i]) && B.ground[i] != (uint8_t)Ground::Sand)
      B.ground[i] = (uint8_t)Ground::Dirt;
    if (B.bridge[i]) B.ground[i] = (uint8_t)Ground::Bridge;
    else if (B.lake[i] || B.riverW[i]) B.ground[i] = (uint8_t)(deep[i] ? Ground::DeepWater : Ground::Water);
  }
  // lake shores: sandy patches on open land (only where the whole neighbourhood is inside the rect)
  for (int y = 1; y < h - 1; y++)
    for (int x = 1; x < w - 1; x++) {
      size_t i = (size_t)y * w + x;
      if (B.lake[i] || B.riverW[i] || waterG(B.ground[i])) continue;
      bool shore = false;
      for (int dy = -1; dy <= 1 && !shore; dy++)
        for (int dx = -1; dx <= 1; dx++) if (B.lake[(size_t)(y + dy) * w + x + dx]) { shore = true; break; }
      if (!shore) continue;
      Biome b = (Biome)B.biome[i];
      if (b != Biome::Plains && b != Biome::Desert && b != Biome::Beach && b != Biome::Forest && b != Biome::Autumn) continue;
      if (hq(tileHash(seed ^ tag("b.shore"), x0 + x, y0 + y)) < Q(0.62)) B.ground[i] = (uint8_t)Ground::Sand;
    }
}

// ------------------------------------------------------------------ settlements (cached buffers)
// A settlement being built: the land under it, then the town generator's phases (town::Gen::step). town() runs a job
// to the end at once; prepareChunk (the web's streaming, no threads) runs it a phase at a time within a frame budget.
struct EndlessSource::Impl::TownJob {
  Gid id = 0;
  std::shared_ptr<const RegionData> D;   // keeps the plan (ctx.plan points into it) alive
  const SitePlan* p = nullptr;
  int flat = 0;
  std::shared_ptr<BaseRect> B;
  SettlementCtx ctx;
  PortPlan port;                         // (M10) a port's harbour (ctx.harbour points here)
  std::shared_ptr<SettlementOut> out;
  std::unique_ptr<town::Gen> gen;
  int stage = 0;                         // 0 the land, 1 the generator, 2 done
  int row = 0;                           // stage 0: the land's rows made so far (a city's land is ~40k tiles)
  double ms = 0;
};

std::shared_ptr<EndlessSource::Impl::TownJob> EndlessSource::Impl::townJobFor(const SitePlan& p, std::shared_ptr<const RegionData> D) {
  if (townJob && townJob->id == p.id) return townJob;
  auto J = std::make_shared<TownJob>();
  J->id = p.id;
  J->D = std::move(D);
  J->p = &p;
  for (size_t k = 0; k < J->D->plan.sites.size(); k++)
    if (J->D->plan.sites[k].id == p.id) { J->p = &J->D->plan.sites[k]; J->flat = J->D->flatLevel[k].first; }
  J->out = std::make_shared<SettlementOut>();
  return J;
}

// (M1) A river that ENDS inside a settlement's footprint (it fed a lake there, and lakeUnderSettlement drops such
// lakes) would leave a dead-end pool across the streets. Its reaches inside the footprint, followed back from the
// end until the river leaves the footprint, are drained on the town's land. A river that runs through stays (bridged).
void EndlessSource::Impl::drainDeadEndRivers(const SitePlan& p, BaseRect& B) {
  const int32_t ax0 = p.gx - 8, ay0 = p.gy - 8, ax1 = p.gx + p.w + 8, ay1 = p.gy + p.h + 8;
  auto inA = [&](int32_t x, int32_t y) { return x >= ax0 && y >= ay0 && x < ax1 && y < ay1; };
  std::vector<RiverSeg> segs;
  for (int32_t ry = regionOf(ay0); ry <= regionOf(ay1); ry++)
    for (int32_t rx = regionOf(ax0); rx <= regionOf(ax1); rx++) {
      std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
      for (const RiverSeg& s : H->segs) {
        if (!inA(s.x0, s.y0) && !inA(s.x1, s.y1)) continue;
        bool dup = false;
        for (const RiverSeg& o : segs) if (o.x0 == s.x0 && o.y0 == s.y0 && o.x1 == s.x1 && o.y1 == s.y1) { dup = true; break; }
        if (!dup) segs.push_back(s);
      }
    }
  if (segs.empty()) return;
  // endpoints shared by no other piece are the river's ends (or springs)
  auto uses = [&](int32_t x, int32_t y) {
    int n = 0;
    for (const RiverSeg& s : segs) n += (s.x0 == x && s.y0 == y) + (s.x1 == x && s.y1 == y);
    return n;
  };
  std::vector<uint8_t> dry(segs.size(), 0);
  for (size_t i = 0; i < segs.size(); i++) {
    const RiverSeg& s = segs[i];
    if (!inA(s.x0, s.y0) || !inA(s.x1, s.y1)) continue;
    if (uses(s.x0, s.y0) == 1 || uses(s.x1, s.y1) == 1) dry[i] = 1;
  }
  for (bool grew = true; grew;) {
    grew = false;
    for (size_t i = 0; i < segs.size(); i++) {
      if (dry[i] || !inA(segs[i].x0, segs[i].y0) || !inA(segs[i].x1, segs[i].y1)) continue;
      for (size_t j = 0; j < segs.size() && !dry[i]; j++) {
        if (!dry[j]) continue;
        const RiverSeg &a = segs[i], &b = segs[j];
        if ((a.x0 == b.x0 && a.y0 == b.y0) || (a.x0 == b.x1 && a.y0 == b.y1) || (a.x1 == b.x0 && a.y1 == b.y0) || (a.x1 == b.x1 && a.y1 == b.y1)) {
          dry[i] = 1;
          grew = true;
        }
      }
    }
  }
  for (size_t i = 0; i < segs.size(); i++) {
    if (!dry[i]) continue;
    const RiverSeg& s = segs[i];
    const int lo = s.w >= 3 ? -1 : 0, hi = s.w == 1 ? 0 : s.w == 4 ? 2 : 1;
    walk4(s.x0, s.y0, s.x1, s.y1, [&](int32_t x, int32_t y) {
      for (int oy = lo - 1; oy <= hi + 1; oy++)
        for (int ox = lo - 1; ox <= hi + 1; ox++) {
          const int32_t gx = x + ox, gy = y + oy;
          if (!B.in(gx, gy) || !inA(gx, gy)) continue;
          const size_t k = B.at(gx, gy);
          if (!B.riverW[k] || B.lake[k]) continue;
          B.riverW[k] = 0;
          B.bridge[k] = 0;
          const Biome bio = (Biome)B.biome[k];
          B.ground[k] = (uint8_t)(bio == Biome::Swamp ? Ground::Swamp : bio == Biome::Beach ? Ground::Sand : bio == Biome::Ocean ? (Ground)B.ground[k]
                                  : groundFor((Eco)B.eco[k], Q(0.5), Q(0.5), gx, gy));
        }
    });
  }
}

// one step of the job (false: finished, and the town is cached)
bool EndlessSource::Impl::townJobStep(TownJob& J) {
  Nest nest(*this);
  auto t0 = Clock::now();
  const SitePlan& p = *J.p;
  if (J.stage == 0) {
    // the land the town is built on: base terrain with its rivers and lakes, at the town's flattened level
    SettlementCtx& ctx = J.ctx;
    ctx.plan = &p;
    ctx.kingdom = kingdom(p.kingdom);
    ctx.culture = &atlas().get(p.culture ? p.culture : cultureAt(p.ex, p.ey));   // M3
    ctx.eco = tile(p.ex, p.ey).eco;                                                // M3c
    ctx.ecoAt = [this](int32_t gx, int32_t gy) { return tile(gx, gy).eco; };      // (M7c fixer: the waste land)
    ctx.worldSeed = seed;
    ctx.rx = J.D->plan.rx; ctx.ry = J.D->plan.ry;
    // (M10) a port town or city keeps its harbour's landing clear and joins it to its streets (sea_ports.cpp)
    if ((p.flags & SPF_PORT) && isSettlement(p.type) && harbourOf(Node{p.id, p.type, p.ex, p.ey, p.seed, p.flags}, J.port) && J.port.harbour)
      ctx.harbour = &J.port;
    auto bi = J.D->bearings.find(p.id);
    if (bi != J.D->bearings.end()) ctx.roadBearings = bi->second;
    // (M2 fixer round 3) the land is made a band of rows per step (a whole city's took ~18 ms in one web frame)
    const int pad = 48, bw = p.w + 2 * pad, bh = p.h + 2 * pad;
    if (J.row == 0) J.B = std::make_shared<BaseRect>();
    if (J.row < bh) {
      const int rows = std::max(8, 6000 / std::max(1, bw));
      baseRectRows(p.gx - pad, p.gy - pad, bw, bh, *J.B, J.row, J.row + rows);
      J.row += rows;
      J.ms += msSince(t0);
      return true;
    }
    baseRectWater(*J.B);
    std::shared_ptr<BaseRect> B = J.B;
    drainDeadEndRivers(p, *B);
    const int flat = J.flat;
    const SitePlan* pp = &p;
    ctx.base = [this, B, flat, pp](int32_t gx, int32_t gy, Ground& g, Biome& b, uint8_t& h) {
      if (gx > B->x0 && gy > B->y0 && gx < B->x0 + B->w - 1 && gy < B->y0 + B->h - 1) {
        size_t i = B->at(gx, gy);
        g = (Ground)B->ground[i];
        b = (Biome)B->biome[i];
      } else {
        TileF f = tile(gx, gy);
        g = groundFor(f.eco, f.e, f.t, gx, gy);
        b = f.biome;
      }
      bool inside = gx >= pp->gx - 8 && gy >= pp->gy - 8 && gx < pp->gx + pp->w + 8 && gy < pp->gy + pp->h + 8;
      h = (uint8_t)(inside ? flat : natLevel(gx, gy));
      // the town stands on dry land: marsh pools in its footprint are drained (a river still runs through, bridged)
      if (inside && g == Ground::Lava) g = Ground::Sand;   // (M3c) nor on an ash field's lava
      if (inside && (g == Ground::Water || g == Ground::DeepWater) && b != Biome::Ocean) {
        bool river = false;
        if (gx > B->x0 && gy > B->y0 && gx < B->x0 + B->w - 1 && gy < B->y0 + B->h - 1) river = B->riverW[B->at(gx, gy)] != 0;
        if (!river) g = b == Biome::Swamp ? Ground::Swamp : b == Biome::Beach ? Ground::Sand : Ground::Grass;
      }
    };
    // what buildSettlement does before its generator runs
    *J.out = SettlementOut();
    J.out->gx = p.gx - town::MARGIN;
    J.out->gy = p.gy - town::MARGIN;
    J.gen.reset(new town::Gen(J.ctx, *J.out));
    J.stage = 1;
  } else if (J.stage == 1) {
    if (!J.gen->step()) { J.gen.reset(); J.stage = 2; }
  }
  J.ms += msSince(t0);
  if (J.stage < 2) return true;
  stats.settlements++;
  stats.settlementMs += J.ms;
  stats.maxSettlementMs = std::max(stats.maxSettlementMs, J.ms);
  if (towns.size() >= 24) {
    auto oldest = towns.begin();
    for (auto i = towns.begin(); i != towns.end(); ++i) if (i->second.used < oldest->second.used) oldest = i;
    towns.erase(oldest);
  }
  towns[J.id] = TownEntry{J.out, ++townClock};
#ifndef __EMSCRIPTEN__
  sharedTownPut(seed, J.id, J.out);
#endif
  return false;
}

// (M3 towns lane) a town another source of this process has finished goes into this source's cache too (a template
// on the source's Impl, so gen.h needs no new member)
template <class ImplT>
static bool haveTown(ImplT& S, Gid id) {
  if (S.towns.count(id)) return true;
#ifndef __EMSCRIPTEN__
  if (std::shared_ptr<SettlementOut> o = sharedTownGet(S.seed, id)) {
    if (S.towns.size() >= 24) {
      auto oldest = S.towns.begin();
      for (auto i = S.towns.begin(); i != S.towns.end(); ++i) if (i->second.used < oldest->second.used) oldest = i;
      S.towns.erase(oldest);
    }
    S.towns[id] = {std::move(o), ++S.townClock};
    return true;
  }
#endif
  return false;
}

std::shared_ptr<SettlementOut> EndlessSource::Impl::town(const SitePlan& p, std::shared_ptr<const RegionData> D) {
  haveTown(*this, p.id);
  auto it = towns.find(p.id);
  if (it != towns.end()) { it->second.used = ++townClock; return it->second.out; }
  // (a phase-at-a-time build of this very town already under way is finished rather than started over)
  std::shared_ptr<TownJob> J = townJobFor(p, std::move(D));
  while (townJobStep(*J)) {}
  if (townJob == J) townJob.reset();
  return J->out;
}

bool EndlessSource::Impl::prepareChunk(int32_t cx, int32_t cy, double budgetMs) {
  auto t0 = Clock::now();
  makeStart();
  const int32_t x0 = cx * CHUNK, y0 = cy * CHUNK;
  const int32_t rx0 = regionOf(x0), ry0 = regionOf(y0);
  std::shared_ptr<const RegionData> RD[9];
  for (int k = 0; k < 9; k++) {
    if (msSince(t0) >= budgetMs) return false;   // (one region plan is one piece of work: a few ms at most)
    RD[k] = regionData(rx0 - 1 + k % 3, ry0 - 1 + k / 3);
  }
  // (M3 fixer round 2) and the regions a lake by this chunk may ask about (lakeUnderSettlement looks ~260 tiles round
  // a lake), so chunk() never plans a region inside itself
  for (int32_t ry = regionOf(y0 - 336); ry <= regionOf(y0 + CHUNK + 336); ry++)
    for (int32_t rx = regionOf(x0 - 336); rx <= regionOf(x0 + CHUNK + 336); rx++) {
      if (regions.get(key2(rx, ry))) continue;
      if (msSince(t0) >= budgetMs) return false;
      regionData(rx, ry);
    }
  // a town left half-built by an earlier call comes first (it is wanted now, or soon will be)
  if (townJob) {
    while (msSince(t0) < budgetMs)
      if (!townJobStep(*townJob)) { townJob.reset(); break; }
    if (townJob) return false;
  }
  // the settlements chunk() asks town() for
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites) {
      if (!isSettlement(p.type)) continue;
      const int margin = 32;
      if (p.gx - margin >= x0 + CHUNK + 1 || p.gy - margin >= y0 + CHUNK + 1 || p.gx + p.w + margin <= x0 - 1 || p.gy + p.h + margin <= y0 - 1) continue;
      if (haveTown(*this, p.id)) continue;
      // (M3 fixer round 2) the town's land reads the lakes round it, and each lake asks lakeUnderSettlement, which plans
      // every region within ~260 tiles of it: up to six region plans inside one unsplittable town step (14 ms on the
      // desktop's main thread, the web's walking hitch). Plan those regions first, one budget check apiece.
      for (int32_t ry = regionOf(p.gy - 336); ry <= regionOf(p.gy + p.h + 336); ry++)
        for (int32_t rx = regionOf(p.gx - 336); rx <= regionOf(p.gx + p.w + 336); rx++) {
          if (regions.get(key2(rx, ry))) continue;
          if (msSince(t0) >= budgetMs) return false;
          regionData(rx, ry);
        }
      townJob = townJobFor(p, RD[k]);
      for (;;) {
        if (msSince(t0) >= budgetMs) return false;
        if (!townJobStep(*townJob)) { townJob.reset(); break; }
      }
    }
  return true;
}

// ------------------------------------------------------------------ site stamps
void EndlessSource::Impl::stampSite(const SitePlan& p, Stamp& S) {
  const int32_t x = p.ex, y = p.ey;
  for (int32_t yy = p.gy - 2; yy < p.gy + p.h + 2; yy++)
    for (int32_t xx = p.gx - 2; xx < p.gx + p.w + 2; xx++) S.reserve(xx, yy);
  switch (p.type) {
    case SiteType::Cave: {
      // the mouth in a real cliff face (relief): the entrance tile stays walkable, its approach clear. Where the land
      // has no face here (a start-plan cave on the flat), the mouth opens in a rocky knoll.
      // (M1) the mouth is always set into rock: where a face exists, a crag of rock rises behind it on the level above
      // (M1 round 3) the crag is an outcrop, not a box: broad at its foot, narrowing row by row to a peak that leans
      // one way or the other, each flank ragged on its own (it was an 11x5 block with straight sides)
      {
        const bool face = fineLevel(x, y - 1) > fineLevel(x, y);
        const int32_t yBot = face ? y - 1 : y;
        Rng cr(p.seed ^ 0xC4A6u);
        const int32_t hgt = 5 + cr.irange(3);
        const int lean = cr.irange(3) - 1;
        const int baseL = 5 + cr.irange(3), baseR = 5 + cr.irange(3);
        for (int32_t yy = std::max(y - 7, yBot - hgt); yy <= yBot; yy++) {
          const int32_t r = yBot - yy;                                   // rows above the foot
          const int32_t cx = x + (lean * r) / 3;
          const int jl = (int)(tileHash(p.seed ^ 0x51u, 0, yy) % 3) - 1, jr = (int)(tileHash(p.seed ^ 0x52u, 0, yy) % 3) - 1;
          const int32_t hl = std::max<int32_t>(1, baseL - (baseL - 1) * r / hgt + jl);
          const int32_t hr = std::max<int32_t>(1, baseR - (baseR - 1) * r / hgt + jr);
          for (int32_t xx = std::max(x - 8, cx - hl); xx <= std::min(x + 8, cx + hr); xx++) {
            // (M2 fixer) where the mouth is in a face, the crag stands on the level above it only: rock on the low
            // land below the face drew a second face under the first (two stacked ledges beside the mouth)
            if (face && fineLevel(xx, yy) < fineLevel(x, y - 1)) continue;
            S.g(xx, yy, Ground::Rock); S.clear(xx, yy);
          }
        }
      }
      S.g(x, y, Ground::Dirt);
      S.p(x, y, Prop::CaveEntrance);
      for (int k = -1; k <= 1; k++)
        for (int j = 1; j <= 2; j++) {
          S.clear(x + k, y + j);
          Ground gg = S.at(x + k, y + j);
          if (gg == Ground::Rock || gg == Ground::Swamp) S.g(x + k, y + j, Ground::Dirt);
        }
      if (S.in(x, y + 1) && (tileHash(p.seed, x, y) & 1) && !(p.flags & SPF_SEACAVE)) S.g(x, y + 1, Ground::Dirt);
      // (M10) a sea cave's mouth opens on the water: no bones or boulder in the sea before it
      if (!(p.flags & SPF_SEACAVE)) S.p(x + 2, y + 1, (p.seed >> 3) & 1 ? Prop::Bones : Prop::Boulder);
      break;
    }
    case SiteType::Ruin: {
      // (M1 round 3) the remains of an old hall: the door down into the vaults stands in what is left of its north
      // wall (always on the site's entrance tile, so World never has to paint a second one), broken walls run round a
      // floor of cracked flags that grass and earth are taking back, columns stand or lie snapped off in two rows, the
      // south wall has mostly fallen (the way in), and flags and rubble lie scattered outside the walls. Every wall
      // tile and column is drawn with its own broken top (the view picks a variant per tile).
      const int32_t rx = p.gx, ry = p.gy;
      const int32_t dx0 = p.ex, wy = p.ey;                  // the door, in the north wall's line
      const int32_t hx0 = rx - 1, hx1 = rx + 9, hy1 = wy + 7;  // walls: x hx0 / hx1, rows wy (north) and hy1 (south)
      const uint64_t rs = p.seed ^ 0x52u;
      for (int32_t yy = ry - 3; yy <= hy1 + 3; yy++)
        for (int32_t xx = hx0 - 2; xx <= hx1 + 2; xx++) S.reserve(xx, yy);
      // the floor and the ground under the walls
      for (int32_t yy = wy; yy <= hy1; yy++)
        for (int32_t xx = hx0; xx <= hx1; xx++) {
          S.clear(xx, yy);
          const int32_t n = vnoiseQ(xx, yy, 2, mix64(rs ^ 0x9Eu));
          const uint32_t h = (uint32_t)(tileHash(rs, xx, yy) & 255);
          const bool wallLine = xx == hx0 || xx == hx1 || yy == wy || yy == hy1;
          Ground g = Ground::StoneFloor;
          if (!wallLine) {
            if (n > 47000 && h < 200) g = Ground::Grass;
            else if (n > 40000 || h < 22) g = Ground::Dirt;
          }
          S.g(xx, yy, g);
        }
      // behind the north wall and round the outside: trodden earth and rubble fallen from the walls
      for (int32_t yy = ry - 2; yy <= hy1 + 2; yy++)
        for (int32_t xx = hx0 - 2; xx <= hx1 + 2; xx++) {
          if (xx >= hx0 && xx <= hx1 && yy >= wy && yy <= hy1) continue;
          const Ground at = S.at(xx, yy);
          if (groundSolid(at) || at == Ground::Bridge || at == Ground::Road) continue;
          const int dist = std::max({hx0 - xx, xx - hx1, wy - yy, yy - hy1, 0});
          const uint32_t h = (uint32_t)(tileHash(rs ^ 0x11u, xx, yy) & 255);
          const bool behind = yy < wy && xx >= hx0 && xx <= hx1;   // the vault's mound behind the door wall
          if (behind ? h < 170u : h < (dist <= 1 ? 120u : 50u)) { S.clear(xx, yy); S.g(xx, yy, Ground::Dirt); }
          if ((behind && h >= 200) || (!behind && h >= 236 && dist <= 2))
            S.p(xx, yy, (h & 1) ? Prop::Rock : (h & 2) ? Prop::MossRock : Prop::Boulder);
        }
      // the walls: the north one stands best, one side has fallen further than the other, the south one is mostly gone
      Rng rr(p.seed ^ 0x2B1Fu);
      const bool westWorse = rr.irange(2) != 0;
      auto rubble = [&](int32_t xx, int32_t yy, uint32_t h) {
        if (h & 64) S.p(xx, yy, (h & 1) ? Prop::Rock : (h & 2) ? Prop::MossRock : Prop::Boulder);
        else S.g(xx, yy, (h & 128) ? Ground::Dirt : Ground::StoneFloor);
      };
      for (int32_t yy = wy; yy <= hy1; yy++)
        for (int32_t xx = hx0; xx <= hx1; xx++) {
          const bool n = yy == wy, so = yy == hy1, w = xx == hx0, e = xx == hx1;
          if (!n && !so && !w && !e) continue;
          if (xx == dx0 && n) continue;                                 // the door
          const uint32_t h = (uint32_t)(tileHash(rs ^ 0x33u, xx, yy) & 255);
          if ((n || so) && (w || e)) {                                  // corners: a column, a wall end or a heap
            if (h < 150) S.p(xx, yy, Prop::RuinColumn);
            else if (h < 215) S.p(xx, yy, Prop::RuinWall);
            else rubble(xx, yy, h);
            continue;
          }
          uint32_t keep = 200;                                          // out of 256
          if (so) keep = std::abs(xx - dx0) <= 1 ? 0 : 96;              // the way in
          else if (w || e) keep = (w == westWorse) ? 90 : 185;
          else if (std::abs(xx - dx0) == 1) keep = 256;                 // the door's jambs always stand
          if (h < keep) S.p(xx, yy, Prop::RuinWall);
          else rubble(xx, yy, h);
        }
      S.g(dx0, wy, Ground::StoneFloor);
      S.p(dx0, wy, Prop::IronDoor);
      // inside: columns (some standing, some snapped off, some lying in pieces), braziers by the door
      // (M7 LAND, owner leftover "ruin entrances repeat across seeds") the hall is laid out its own way from the site's
      // seed and id (every draw from its own hash, so nothing else moves): two rows of columns (the old hall), a nave of
      // three pairs close to the aisle, pilasters engaged in the side walls, or no columns at all and a heap where the
      // roof came down; one corner fallen in for good (the walls and the floor there gone back to earth and grass); the
      // braziers by the door or a pair of statues; and what lies inside (graves, an urn and bones, a toppled statue,
      // coffins and skulls, a fallen column across the floor)
      const uint64_t ls = mix64(p.seed ^ (uint64_t)p.id * 0x9E3779B97F4A7C15ull ^ 0x7255u);
      const uint32_t L0 = (uint32_t)(ls & 0xFFFFFFFFu), L1 = (uint32_t)(ls >> 32);
      const int colPlan = (int)(L0 % 4u), corner = (int)((L0 >> 4) % 5u), dress = (int)((L0 >> 8) % 5u);
      const int32_t c0 = wy + 2 + rr.irange(2), c1 = c0 + 3;
      auto column = [&](int32_t cx, int32_t cy) {
        if (cx == dx0 || (std::abs(cx - dx0) == 1 && cy <= wy + 1)) return;   // never in the aisle to the door
        const uint32_t h = (uint32_t)(tileHash(rs ^ 0x44u, cx, cy) & 255);
        if (h < 190) S.p(cx, cy, Prop::RuinColumn);
        else S.p(cx, cy, (h & 1) ? Prop::Rock : (h & 2) ? Prop::MossRock : Prop::Boulder);
      };
      if (colPlan == 0) {
        for (int32_t cy : {c0, c1}) for (int32_t cx : {rx + 1, rx + 7}) column(cx, cy);
      } else if (colPlan == 1) {
        for (int32_t cy = wy + 2; cy <= hy1 - 2; cy += 2) for (int32_t cx : {dx0 - 2, dx0 + 2}) column(cx, cy);
      } else if (colPlan == 2) {
        for (int32_t cy = wy + 2; cy <= hy1 - 1; cy += 2) for (int32_t cx : {hx0, hx1}) column(cx, cy);
      } else {
        // the roof's fall: a heap of stone off the aisle and earth round it
        const int32_t fx = (L1 & 1) ? rx + 2 : rx + 6, fy = wy + 3 + (int32_t)((L1 >> 1) % 2u);
        for (int32_t yy = fy - 1; yy <= fy + 1; yy++)
          for (int32_t xx = fx - 1; xx <= fx + 1; xx++) {
            if (std::abs(xx - dx0) <= 1) continue;
            const uint32_t h = (uint32_t)(tileHash(rs ^ 0x45u, xx, yy) & 255);
            S.g(xx, yy, h < 128 ? Ground::Dirt : Ground::StoneFloor);
            if (h < 150) S.p(xx, yy, (h & 1) ? Prop::Rock : (h & 2) ? Prop::Rubble : Prop::Boulder);
          }
      }
      if (corner < 4) {   // a corner fallen in: its walls down, the ground taken back
        const bool east = (corner & 1) != 0, south = (corner & 2) != 0;
        const int32_t kx = east ? hx1 : hx0, ky = south ? hy1 : wy + 1;
        for (int32_t yy = ky - 2; yy <= ky + 2; yy++)
          for (int32_t xx = kx - 2; xx <= kx + 2; xx++) {
            if (yy <= wy || yy > hy1 || xx < hx0 || xx > hx1) continue;
            if (std::abs(xx - kx) + std::abs(yy - ky) > 3 || std::abs(xx - dx0) <= 1) continue;
            const uint32_t h = (uint32_t)(tileHash(rs ^ 0x46u, xx, yy) & 255);
            S.clear(xx, yy);
            S.g(xx, yy, h < 110 ? Ground::Grass : Ground::Dirt);
            if (h >= 210) S.p(xx, yy, (h & 1) ? Prop::MossRock : Prop::Rock);
          }
      }
      if (dress == 0) { S.p(dx0 - 1, wy + 1, Prop::Statue); S.p(dx0 + 1, wy + 1, Prop::Statue); }
      else { S.p(dx0 - 2, wy + 1, Prop::Brazier); S.p(dx0 + 2, wy + 1, Prop::Brazier); }
      auto put = [&](int32_t xx, int32_t yy, Prop q) {   // inside the walls, off the aisle, on a free floor tile
        if (xx <= hx0 || xx >= hx1 || yy <= wy || yy >= hy1 || std::abs(xx - dx0) <= 1) return;
        if (S.in(xx, yy) && S.c.prop[S.idx(xx, yy)]) return;
        S.p(xx, yy, q);
      };
      const int layout = rr.irange(3);
      const int pick = (layout + dress) % 5;
      if (pick == 0) { put(rx + 2 + (int32_t)(L1 % 2u) * 4, hy1 - 1, Prop::Gravestone); put(rx, c0 + 1, Prop::Urn); put(rx + 8, c1 - 1, Prop::Bones); }
      else if (pick == 1) { put(rx, c0 + 1, Prop::Gravestone); put(rx + 8, c0 + 2, Prop::Gravestone); put(rx + 2, c1 + 1, Prop::Coffin); put(rx + 6, c0 + 1, Prop::SkullPile); }
      else if (pick == 2) { put((L1 & 4) ? rx + 2 : rx + 6, c0 + 1, Prop::ToppledStatue); put((L1 & 4) ? rx + 7 : rx + 1, hy1 - 1, Prop::Urn); }
      else if (pick == 3) { for (int32_t xx = rx + 1; xx <= rx + 3; xx++) put(xx, c1, Prop::RuinColumn); put(rx + 7, c0, Prop::Bones); }
      else { put(rx + 1, hy1 - 1, Prop::Coffin); put(rx + 7, hy1 - 1, Prop::Coffin); put(rx + 4 + ((L1 & 8) ? 2 : -2), c0 + 1, Prop::SkullPile); }
      // the way from the fallen south wall to the door stays open
      for (int32_t yy = wy + 1; yy <= hy1 + 1; yy++)
        for (int32_t xx = dx0 - 1; xx <= dx0 + 1; xx++) {
          const int pr = S.in(xx, yy) ? S.c.prop[S.idx(xx, yy)] : 0;
          if (pr && propSolid((Prop)(pr - 1)) && !(yy == wy + 1 && xx != dx0)) S.clear(xx, yy);
        }
      break;
    }
    case SiteType::BanditCamp: {
      const int32_t bx = p.gx, by = p.gy;
      // (M1) a trampled clearing with a ragged edge: the dirt and the cleared ground follow noisy ovals instead of
      // the old rectangle
      for (int32_t yy = by; yy < by + 9; yy++)
        for (int32_t xx = bx; xx < bx + 11; xx++) {
          int32_t dx = 2 * (xx - (bx + 5)), dy = 2 * (yy - (by + 4));
          const int32_t d = dx * dx + (dy * dy * 169) / 100;
          const int32_t j = (int32_t)(tileHash(p.seed ^ 0xCA3Bu, xx, yy) % 28);
          const bool inner = xx > bx && yy > by && xx < bx + 10 && yy < by + 8;
          if (d < 70 + j || (inner && groundSolid(S.at(xx, yy)))) S.clear(xx, yy);
          if (d < 46 + j) S.g(xx, yy, Ground::Dirt);
          else if (inner && groundSolid(S.at(xx, yy))) S.g(xx, yy, Ground::Dirt);
        }
      S.p(bx + 5, by + 4, Prop::Campfire);
      S.p(bx + 2, by + 2, Prop::Tent); S.p(bx + 8, by + 2, Prop::Tent);
      S.p(bx + 2, by + 6, Prop::Crate); S.p(bx + 3, by + 7, Prop::Barrel);
      S.p(bx + 8, by + 6, Prop::Chest); S.p(bx + 9, by + 7, Prop::Woodpile);
      S.p(bx + 1, by + 4, Prop::Torch); S.p(bx + 9, by + 4, Prop::Torch);
      // (M10) a pirate cove: the sea's plunder about the fire (an anchor, cargo, nets) instead of the woodpile
      const bool pirate = (p.flags & SPF_PIRATE) != 0;
      if (pirate) {
        S.p(bx + 9, by + 7, Prop::Anchor); S.p(bx + 2, by + 6, Prop::CargoPile); S.p(bx + 5, by + 1, Prop::NetRack);
      }
      // the camp's bandits (the whole site's, so they arrive with any chunk the camp touches)
      Rng r(p.seed ^ 0xBA4Du);
      int n = 4 + r.irange(3);
      for (int k = 0; k < n; k++) {
        SpawnPlan s; s.siteId = p.id;
        s.sp.bandit = true; s.sp.slot = k;
        if (pirate) s.sp.role = Role::Pirate;
        s.sp.x = bx + 3 + r.irange(5); s.sp.y = by + 3 + r.irange(3);
        S.c.spawns.push_back(s);
      }
      SpawnPlan chief; chief.siteId = p.id;
      chief.sp.bandit = true; chief.sp.boss = true; chief.sp.slot = 9; chief.sp.x = bx + 6; chief.sp.y = by + 3;
      if (pirate) chief.sp.role = Role::Pirate;
      S.c.spawns.push_back(chief);
      break;
    }
    case SiteType::Shrine: {
      if (p.flags & SPF_SUMMIT) {
        // (M11) a summit shrine: a 7 x 5 floor on the top level (its flat), the shrine (3 x 2) at the back on a paved
        // court, prayer flags either side of it, a cairn by the front where the trail arrives (at x, y + 3)
        // (M11 fixer r1) the floor's bare rock or bog takes the ground of the land round it (snow among snow and ice:
        // a block of tundra on a glacier read as a pasted rectangle); the ring is read from the plan's own tiles
        Ground fill = Ground::Tundra;
        {
          int cold = 0, warm = 0;
          for (int32_t k = -1; k <= p.w; k++)
            for (int32_t yy : {p.gy - 2, p.gy + p.h + 1}) {
              const TileF f = tile(p.gx + k, yy);
              const Ground q = groundFor(f.eco, f.e, f.t, p.gx + k, yy);
              if (q == Ground::Snow || q == Ground::Ice) cold++; else if (!groundSolid(q)) warm++;
            }
          if (cold > warm) fill = Ground::Snow;
        }
        for (int32_t yy = p.gy; yy < p.gy + p.h; yy++)
          for (int32_t xx = p.gx; xx < p.gx + p.w; xx++) {
            S.clear(xx, yy);
            const Ground g0 = S.at(xx, yy);
            if (groundSolid(g0) || g0 == Ground::Swamp) S.g(xx, yy, fill);
          }
        // (M11 fixer r1) no paved court: a flat slab of paving with a ragged edge read as pasted on. The shrine's own
        // stepped dais is its foundation; before it the ground is trodden earth, the trail's last tiles running up to
        // its steps (the trail is clipped at the floor's edge)
        for (int32_t xx = x - 1; xx <= x + 1; xx++) S.g(xx, y + 1, Ground::Dirt);
        for (int32_t yy = y + 2; yy <= y + 3; yy++) S.g(x, yy, Ground::Dirt);
        for (int32_t yy = y - 1; yy <= y; yy++)
          for (int32_t xx = x - 1; xx <= x + 1; xx++) S.p(xx, yy, Prop::Filler);
        S.p(x, y, Prop::SummitShrine);
        S.p(x - 3, y - 1, Prop::PrayerFlags);
        S.p(x + 3, y - 1, Prop::PrayerFlags);
        S.p((p.seed & 2) ? x - 2 : x + 2, y + 2, Prop::Cairn);
        break;
      }
      const int32_t sx = p.gx, sy = p.gy;
      for (int32_t yy = sy; yy < sy + 5; yy++) for (int32_t xx = sx; xx < sx + 5; xx++) { S.clear(xx, yy); if (groundSolid(S.at(xx, yy))) S.g(xx, yy, Ground::Grass); }
      for (int32_t yy = sy + 1; yy < sy + 4; yy++) for (int32_t xx = sx + 1; xx < sx + 4; xx++) S.g(xx, yy, Ground::Plaza);
      S.p(sx + 2, sy + 1, Prop::Shrine);
      S.p(sx + 1, sy + 3, Prop::Flowers2); S.p(sx + 3, sy + 3, Prop::Flowers3);
      break;
    }
    case SiteType::DragonLair: {
      // a scorched eyrie on the ridge: a fire-blackened floor round an old dais, bones and rubble strewn, and crags of
      // bare rock standing round its back and sides (open to the south, where the track climbs in)
      // (M1 round 3) the scorch fades into the land's own ground (snow, tundra) along a noisy edge instead of a tan
      // blob with a hard rim, and the crags really ring the summit (the old ring lay almost wholly outside its box)
      const uint64_t ls = mix64(p.seed ^ 0x1A11u);
      for (int32_t yy = y - 7; yy <= y + 7; yy++)
        for (int32_t xx = x - 8; xx <= x + 8; xx++) {
          int32_t dx = xx - x, dy = yy - y;
          int32_t d2 = dx * dx * 100 + dy * dy * 132;
          int32_t lim = 76 + (int32_t)(tileHash(p.seed ^ 61u, xx, yy) % 15);
          if (d2 >= lim * lim) continue;
          S.clear(xx, yy);
          const uint32_t fh = (uint32_t)(tileHash(p.seed ^ 66u, xx, yy) & 15);
          const int32_t q = d2 * 100 / (lim * lim);                    // 0 at the heart .. 100 at the rim
          const int32_t n = vnoiseQ(xx, yy, 2, ls) >> 8;               // 0..255
          const bool dais = std::abs(dx) <= 2 - (dy == -6 || dy == -3 ? 1 : 0) && dy >= -6 && dy <= -3 && fh > 2;
          const Ground nat = S.at(xx, yy);
          if (dais) S.g(xx, yy, Ground::StoneFloor);
          else if (q < 30 || n * 100 > (q - 20) * 330 || groundSolid(nat) || nat == Ground::Grass || nat == Ground::Meadow) S.g(xx, yy, Ground::Dirt);
          // else: the land's own snow or tundra, swept by the scorch's edge
          if (d2 > 52 * 52 && (tileHash(p.seed ^ 62u, xx, yy) % 11) == 0) {
            const uint32_t k = (uint32_t)(tileHash(p.seed ^ 63u, xx, yy) % 5);
            S.p(xx, yy, k == 0 ? Prop::Boulder : k == 1 ? Prop::Bones : k == 2 ? Prop::SnowRock : k == 3 ? Prop::SkullPile : Prop::Rock);
          }
        }
      for (int32_t yy = y - 12; yy <= y + 4; yy++)
        for (int32_t xx = x - 13; xx <= x + 13; xx++) {
          int32_t dx = xx - x, dy = yy - y;
          int32_t d2 = dx * dx * 100 + dy * dy * 132;
          int32_t lim = 76 + (int32_t)(tileHash(p.seed ^ 61u, xx, yy) % 15);
          const int32_t outer = 100 + (vnoiseQ(xx, yy, 2, ls ^ 0x77u) >> 11);   // 100..131: the crags' ragged back
          if (d2 < (lim + 6) * (lim + 6) || d2 > outer * outer) continue;
          if (dy > 1 && std::abs(dx) < 9) continue;                              // the way in from the south
          if (dy > 3) continue;
          S.clear(xx, yy);
          S.g(xx, yy, Ground::Rock);
        }
      {
        Rng lr(p.seed ^ 0x1A12u);
        const int32_t ax = x, sp = 2 + lr.irange(2);
        const int32_t skY = y - 2 + lr.irange(2);   // (draws sequenced: the right argument's first, as MSVC/GCC)
        const int32_t skX = x - 3 - lr.irange(2);
        S.p(skX, skY, Prop::SkullPile); S.p(x + 3 + lr.irange(2), y + 1, Prop::Bones); S.p(x - 5, y + 2 + lr.irange(2), Prop::Bones);
        S.p(ax + sp, y - 4, Prop::Brazier); S.p(ax - sp, y - 4, Prop::Brazier); S.p(ax, y - 5, Prop::Altar);
      }
      break;
    }
    case SiteType::Vignette: stampVignette(p, S); break;   // (M2, vignettes.cpp)
    case SiteType::Wonder: stampWonder(p, S); break;
    default: break;
  }
}

void EndlessSource::Impl::stampDen(const DenPlan& d, Stamp& S) {
  Rng r(mix64(seed ^ d.id));
  for (int oy = -3; oy <= 3; oy++) for (int ox = -3; ox <= 3; ox++) if (ox * ox + oy * oy <= 10) S.reserve(d.x + ox, d.y + oy);
  // (M10) a merrow den lies in the sea by a wreck: no lair props (the wreck is their hoard)
  if (d.mon == Monster::Merrow) return;
  Biome b = S.in(d.x, d.y) ? (Biome)S.c.biome[S.idx(d.x, d.y)] : tile(d.x, d.y).biome;
  bool snowy = b == Biome::Snow;
  Prop rock = snowy ? Prop::SnowRock : (b == Biome::Forest || b == Biome::Swamp || b == Biome::Taiga) ? Prop::MossRock : Prop::Boulder;
  std::vector<Prop> big, small;
  switch (d.mon) {
    case Monster::Wolf: case Monster::IceWolf: big = {rock, Prop::Boulder, Prop::DeadTree}; small = {Prop::Bones, Prop::SkullPile, Prop::Bones}; break;
    case Monster::Goblin: big = {Prop::Tent, Prop::Crate, Prop::Woodpile}; small = {Prop::Bones, Prop::Barrel}; break;
    case Monster::Skeleton: big = {Prop::Gravestone, Prop::Gravestone, Prop::DeadTree}; small = {Prop::SkullPile, Prop::Bones, Prop::Bones}; break;
    case Monster::Spider: case Monster::FrostSpider: big = {Prop::DeadTree, rock}; small = {Prop::Cobweb, Prop::Cobweb, Prop::Bones}; break;
    case Monster::Troll: big = {rock, Prop::Boulder, rock}; small = {Prop::SkullPile, Prop::Bones, Prop::Bones}; break;
    default: big = {rock, Prop::Boulder, Prop::Log}; small = {Prop::Bones, Prop::Bones}; break;
  }
  if (d.mon == Monster::Goblin) S.p(d.x, d.y, Prop::Campfire);
  static const int chestAt[3][2] = {{0, -2}, {1, -2}, {-1, -2}};
  const int* ch = chestAt[r.irange(3)];
  S.p(d.x + ch[0], d.y + ch[1], Prop::Chest);
  static const int bigAt[3][2] = {{-2, -2}, {2, -1}, {-3, 0}};
  for (size_t k = 0; k < big.size() && k < 3; k++) S.p(d.x + bigAt[k][0], d.y + bigAt[k][1], big[k]);
  static const int smallAt[4][2] = {{1, 1}, {-1, 1}, {2, 2}, {-2, 1}};
  for (int k = 0; k < 4; k++) if (r.next() % 10 < 7) S.p(d.x + smallAt[k][0], d.y + smallAt[k][1], small[(size_t)r.irange((int)small.size())]);
}

// ------------------------------------------------------------------ the chunk
void EndlessSource::Impl::chunk(int32_t cx, int32_t cy, ChunkData& c) {
  makeStart();
  auto t0 = Clock::now();
  const double nested0 = nestedMs;
  c = ChunkData();
  c.cx = cx; c.cy = cy;
  const int32_t x0 = cx * CHUNK, y0 = cy * CHUNK;
  // (M10) the open sea: deep water through and through (oceanChunk: every coarse sample the tiles read lies well
  // below the deep-water line, and nothing of the 3x3 regions round it reaches it), made without the land's pipeline
  if (oceanFast && oceanChunk(cx, cy)) {
    for (int i = 0; i < ChunkData::N; i++) {
      c.ground[i] = (uint8_t)Ground::DeepWater;
      c.biome[i] = (uint8_t)Biome::Ocean;
      c.eco[i] = c.ecoNb[i] = (uint8_t)Eco::Ocean;
    }
    const double ms = msSince(t0) - (nestedMs - nested0);
    stats.chunks++;
    stats.chunkMs += ms;
    stats.maxChunkMs = std::max(stats.maxChunkMs, ms);
    return;
  }
  // 1-2: base land, rivers, lakes (with a 2-tile border)
  BaseRect B;
  baseRect(x0 - BR, y0 - BR, BW, BW, B);
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) {
      size_t bi = (size_t)(ly + BR) * BW + (lx + BR);
      c.ground[c.at(lx, ly)] = B.ground[bi];
      c.biome[c.at(lx, ly)] = B.biome[bi];
      c.eco[c.at(lx, ly)] = c.ecoNb[c.at(lx, ly)] = B.eco[bi];
    }
  std::vector<uint8_t> reserved((size_t)ChunkData::N, 0);
  std::vector<uint8_t> used(RW * RW, 0), road(RW * RW, 0), entrance(RW * RW, 0);
  std::vector<uint8_t> trailT(RW * RW, 0);   // (M11 fixer r2) a summit trail's tiles: its ways up are stairs
  // (M3 towns lane) a terraced town's own relief (its height byte + 1; 0: none): it cut its terraces, faces and
  // stairs itself, so its tiles keep them (its outer band is the plan's flat level, which the land meets); every other
  // town keeps the relief made here
  std::vector<uint8_t> townH(RW * RW, 0);
  auto rin = [&](int32_t gx, int32_t gy) { return gx >= x0 - 1 && gy >= y0 - 1 && gx < x0 + CHUNK + 1 && gy < y0 + CHUNK + 1; };
  auto ri = [&](int32_t gx, int32_t gy) { return (size_t)(gy - (y0 - 1)) * RW + (gx - (x0 - 1)); };
  Stamp S{c, x0, y0, reserved};
  const int32_t rx0 = regionOf(x0), ry0 = regionOf(y0);
  std::shared_ptr<const RegionData> RD[9];
  for (int k = 0; k < 9; k++) RD[k] = regionData(rx0 - 1 + k % 3, ry0 - 1 + k / 3);
  const RegionData& own = *RD[4];
  // (M3c fixer round 2, review: "a dune capital stands in green oak woodland while the HUD says The Dune Sea") the dune
  // folk carry their desert with them: round a dune-culture settlement that stands in green land (plains, forest, the
  // autumn woods), the land within about 12 tiles of its outline, with a ragged edge, is THEIR land: open sand dunes
  // near the walls, rocky scrubland further out, meeting the woods through the ordinary ecotone. The base layers change
  // before anything reads them (the flora, the ecotones, the HUD), so the trees round the town are the desert's.
  // A pure function of the tile and the site plans (every chunk computes the same answer); water, rock, marsh, snow
  // and coasts are left as they are.
  struct DuneApron { int32_t cx, cy, rx, ry; };
  std::vector<DuneApron> aprons;
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites) {
      if (!isSettlement(p.type) || !p.culture) continue;
      const int32_t ax = p.gx + p.w / 2, ay = p.gy + p.h / 2, arx = p.w / 2 + 12, ary = p.h / 2 + 12;
      if (ax + arx + 6 < x0 - 8 || ax - arx - 6 >= x0 + CHUNK + 8 || ay + ary + 6 < y0 - 8 || ay - ary - 6 >= y0 + CHUNK + 8) continue;
      if (atlas().get(p.culture).archetype != cult::Archetype::Dune) continue;
      const Biome hb = tile(p.ex, p.ey).biome;
      if (hb == Biome::Snow || hb == Biome::Taiga) continue;   // a dune town on the snow is a snow town (town_layout)
      aprons.push_back(DuneApron{ax, ay, std::max<int32_t>(1, arx), std::max<int32_t>(1, ary)});
    }
  const uint64_t apronSeed = mix64(seed ^ tag("dune.apron"));
  // the apron's eco at (x, y) for land of family fam, or Eco::COUNT outside every apron
  auto apronEco = [&](int32_t x, int32_t y, Biome fam) {
    if (aprons.empty() || (fam != Biome::Plains && fam != Biome::Forest && fam != Biome::Autumn)) return Eco::COUNT;
    for (const DuneApron& a : aprons) {
      const int64_t dx = x - a.cx, dy = y - a.cy;
      if (std::abs(dx) > a.rx + 6 || std::abs(dy) > a.ry + 6) continue;
      const int64_t d = dx * dx * 1024 / ((int64_t)a.rx * a.rx) + dy * dy * 1024 / ((int64_t)a.ry * a.ry);
      const int64_t T = 800 + (int64_t)fbmQ(x, y, 3, 2, apronSeed) * 480 / 65536;   // a ragged edge (0.80 .. 1.27)
      if (d < T) return d < T * 2 / 5 ? Eco::Dunes : Eco::Scrubland;
    }
    return Eco::COUNT;
  };
  if (!aprons.empty()) {
    for (int32_t y = B.y0; y < B.y0 + B.h; y++)
      for (int32_t x = B.x0; x < B.x0 + B.w; x++) {
        const size_t bi = B.at(x, y);
        const Ground g = (Ground)B.ground[bi];
        if (g != Ground::Grass && g != Ground::Meadow && g != Ground::ForestFloor && g != Ground::Autumn) continue;
        const Eco e = apronEco(x, y, (Biome)B.biome[bi]);
        if (e == Eco::COUNT) continue;
        B.eco[bi] = (uint8_t)e;
        B.biome[bi] = (uint8_t)Biome::Desert;
        B.ground[bi] = (uint8_t)Ground::Sand;
      }
    for (int ly = 0; ly < CHUNK; ly++)
      for (int lx = 0; lx < CHUNK; lx++) {
        size_t bi = (size_t)(ly + BR) * BW + (lx + BR);
        c.ground[c.at(lx, ly)] = B.ground[bi];
        c.biome[c.at(lx, ly)] = B.biome[bi];
        c.eco[c.at(lx, ly)] = c.ecoNb[c.at(lx, ly)] = B.eco[bi];
      }
  }
  // 3: settlement buffers
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites) {
      if (!isSettlement(p.type)) continue;
      const int margin = 32;
      if (p.gx - margin >= x0 + CHUNK + 1 || p.gy - margin >= y0 + CHUNK + 1 || p.gx + p.w + margin <= x0 - 1 || p.gy + p.h + margin <= y0 - 1) continue;
      std::shared_ptr<SettlementOut> T = town(p, RD[k]);
      const Map& b = T->buf;
      // (M3) a terraced town (the only kind with cliff faces of its own) brings its relief along
      bool terraced = false;
      for (size_t q = 0; q < b.height.size() && !terraced; q++) terraced = (b.height[q] & Map::HEIGHT_CLIFF) != 0;
      // (M3c fixer, review: "a near-black hole inside a brick capital") a yard or garden inside a town that lies on a
      // harsh land (a bog, the dark forest, the blight, the ash...) is laid as the town's civic ground: the eco and
      // family of the town's heart, or that family's classic land where the heart itself is harsh
      auto harsh = [](Eco e) {
        switch (e) {
          case Eco::PeatBog: case Eco::TaigaBog: case Eco::ReedMarsh: case Eco::Mangrove: case Eco::FloodedForest: case Eco::DarkForest:
          case Eco::Blight: case Eco::AshFields: case Eco::CrystalBarrens: case Eco::MushroomForest: case Eco::PetrifiedForest:
          case Eco::SaltFlats: case Eco::Glacier: return true;
          default: return false;
        }
      };
      Biome civB = Biome::Plains;
      Eco civE = Eco::COUNT;
      {
        const int hx = std::clamp(p.gx + p.w / 2 - T->gx, 0, b.w - 1), hy = std::clamp(p.gy + p.h / 2 - T->gy, 0, b.h - 1);
        if (!b.biome.empty()) civB = (Biome)b.biome[(size_t)hy * b.w + hx];
        if (!b.eco.empty()) civE = (Eco)b.eco[(size_t)hy * b.w + hx];
        if (civB == Biome::Swamp || civB == Biome::Ocean || civB == Biome::Mountain || civB == Biome::Beach) civB = Biome::Plains;
        if (civE == Eco::COUNT || harsh(civE) || ecoFamily(civE) != civB) civE = ecoOfFamily(civB);
      }
      for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
        for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++) {
          int32_t bx = gx - T->gx, by = gy - T->gy;
          if (!b.in(bx, by)) continue;
          size_t bi = (size_t)by * b.w + bx;
          if (!T->used.empty() && !T->used[bi]) continue;   // outside the town: the wild land stays
          used[ri(gx, gy)] = 1;
          if (terraced) townH[ri(gx, gy)] = (uint8_t)(b.height[bi] + 1);
          if (!S.in(gx, gy)) continue;
          int i = S.idx(gx, gy);
          c.ground[i] = b.ground[bi];
          c.prop[i] = b.prop[bi];
          c.wall[i] = b.wall[bi];
          if (!b.biome.empty()) c.biome[i] = b.biome[bi];
          // (M3c) a town buffer with its own eco layer brings it; else the land's eco stays, unless the town changed
          // the tile's family (then that family's classic eco)
          if (!b.eco.empty()) c.eco[i] = c.ecoNb[i] = b.eco[bi];
          else if (ecoFamily((Eco)c.eco[i]) != (Biome)c.biome[i]) c.eco[i] = c.ecoNb[i] = (uint8_t)ecoOfFamily((Biome)c.biome[i]);
          if (harsh((Eco)c.eco[i]) && !groundWater((Ground)c.ground[i])) {
            c.eco[i] = c.ecoNb[i] = (uint8_t)civE;
            c.biome[i] = (uint8_t)civB;
            if ((Ground)c.ground[i] == Ground::Swamp) c.ground[i] = (uint8_t)Ground::Grass;
          }
          if (!b.blend.empty()) c.blend[i] = b.blend[bi];   // (M3 fixer) the paving / boardwalk mark (Map::PAVE_MARK)
          if (!b.urb.empty()) c.urb[i] = b.urb[bi];         // (M7b) the urban fabric (rpg/world/urban.h)
          reserved[(size_t)i] = 1;
        }
      if (p.gx - 24 >= x0 + CHUNK || p.gy - 24 >= y0 + CHUNK || p.gx + p.w + 24 <= x0 || p.gy + p.h + 24 <= y0) continue;
      // the whole site's records travel with every chunk it touches, so a site's buildings arrive together
      for (const Bldg& bl : b.bldgs) {
        Bldg g = bl;
        g.r.x += T->gx; g.r.y += T->gy;
        g.site = -1;
        c.bldgs.push_back(g);
        c.bldgSite.push_back(p.id);
        int bidx = (int)c.bldgs.size();
        for (int yy = g.r.y; yy < g.r.y + g.r.h; yy++)
          for (int xx = g.r.x; xx < g.r.x + g.r.w; xx++)
            if (S.in(xx, yy)) c.bldg[S.idx(xx, yy)] = (uint16_t)bidx;
      }
      for (const Spawn& sp0 : b.spawns) {
        SpawnPlan sp;
        sp.sp = sp0;
        sp.sp.x += T->gx; sp.sp.y += T->gy;
        sp.siteId = p.id;
        c.spawns.push_back(sp);
      }
      if (p.flags & SPF_PORT) {   // (M10) the harbour's keepers travel with the town's records
        PortPlan PP;
        if (harbourOf(Node{p.id, p.type, p.ex, p.ey, p.seed, p.flags}, PP)) stampHarbour(PP, T.get(), S, &c.spawns);
      }
      for (auto& gt : T->gates) c.gates.push_back(GTile{gt.first + T->gx, gt.second + T->gy});
      for (IRect r : T->wallGaps) { r.x += T->gx; r.y += T->gy; c.wallGaps.push_back(r); }
      for (const PlotPlan& lp : T->plots) c.plots.push_back(lp);   // (M7) its lots for sale (global tiles)
    }
  // 3b (M10): the harbours (sea_ports.cpp): the dredged basin and channel, the mole and its T-head, the dress
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites) {
      if (!(p.flags & SPF_PORT) || !isSettlement(p.type)) continue;
      PortPlan PP;
      if (!harbourOf(Node{p.id, p.type, p.ex, p.ey, p.seed, p.flags}, PP) || !PP.harbour) continue;
      const int32_t reach = PP.pierLen + 12 + harbourDredgeLen(PP);
      if (PP.rootX + reach < x0 - 4 || PP.rootX - reach >= x0 + CHUNK + 4 || PP.rootY + reach < y0 - 4 || PP.rootY - reach >= y0 + CHUNK + 4) continue;
      const int margin = 32;
      const bool nearTown = !(p.gx - margin >= x0 + CHUNK + 1 || p.gy - margin >= y0 + CHUNK + 1 || p.gx + p.w + margin <= x0 - 1 || p.gy + p.h + margin <= y0 - 1);
      std::shared_ptr<SettlementOut> T = nearTown ? town(p, RD[k]) : nullptr;
      stampHarbour(PP, T.get(), S, nullptr);
    }
  // 4: site and den stamps
  for (int k = 0; k < 9; k++) {
    for (const SitePlan& p : RD[k]->plan.sites) {
      if (isSettlement(p.type)) continue;
      if (p.type == SiteType::Cave && rin(p.ex, p.ey)) entrance[ri(p.ex, p.ey)] = 1;
      if (p.gx - 6 >= x0 + CHUNK || p.gy - 6 >= y0 + CHUNK || p.gx + p.w + 6 <= x0 || p.gy + p.h + 6 <= y0) continue;
      stampSite(p, S);
    }
    for (const DenPlan& d : RD[k]->plan.dens) {
      if (d.x + 5 < x0 || d.y + 5 < y0 || d.x - 5 >= x0 + CHUNK || d.y - 5 >= y0 + CHUNK) continue;
      stampDen(d, S);
    }
  }
  // 5: roads (this region's graph roads) and tracks (every nearby site's spur), clipped at settlement footprints
  struct Clip { Gid id; int32_t x0, y0, x1, y1; };
  std::vector<Clip> clips;
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites) {
      // (M11 fixer r1: a summit shrine's trail runs up to its floor, the trodden earth before the shrine)
      int pad = isSettlement(p.type) || (p.type == SiteType::Shrine && (p.flags & SPF_SUMMIT)) ? 0 : 1;
      // (M1) a cave's track runs right up to the trodden ground before its mouth (it stopped at the footprint, a few
      // tiles short): only the rows down to the mouth's are kept clear
      const int32_t yEnd = p.type == SiteType::Cave ? std::min(p.gy + p.h + pad, p.ey + 1) : p.gy + p.h + pad;
      clips.push_back(Clip{p.id, p.gx - pad, p.gy - pad, p.gx + p.w + pad, yEnd});
    }
  std::vector<Clip> keeps;   // wayside places and wonders whose props the roads leave standing
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites)
      if (p.type == SiteType::Wonder || (p.type == SiteType::Vignette && p.kind != (uint8_t)VignetteKind::TollBridge))
        if (p.gx <= x0 + CHUNK && p.gy <= y0 + CHUNK && p.gx + p.w >= x0 && p.gy + p.h >= y0) keeps.push_back(Clip{p.id, p.gx, p.gy, p.gx + p.w, p.gy + p.h});
  auto clipOf = [&](Gid id) -> const Clip* {
    for (const Clip& cl : clips) if (cl.id == id) return &cl;
    return nullptr;
  };
  auto drawRoad = [&](const RoadPlan& rp) {
    const Clip* ca = rp.a ? clipOf(rp.a) : nullptr;
    const Clip* cb = rp.b ? clipOf(rp.b) : nullptr;
    const uint8_t code = rp.cls == 0 ? 3 : rp.cls == 1 ? 2 : 1;
    const int hi = rp.cls == 0 ? 1 : 0;
    // (M3b fixer) the road's raster near the chunk, in unbroken pieces, with a wide margin so a crossing is judged
    // whole (every chunk sees the same crossing and lays the same deck)
    const int32_t MG = 64;
    std::vector<std::vector<GTile>> pieces;
    bool open = false;
    for (size_t n = 0; n + 1 < rp.pts.size(); n++) {
      const GTile &a = rp.pts[n], &b = rp.pts[n + 1];
      if (std::max(a.x, b.x) + 2 < x0 - MG || std::min(a.x, b.x) - 2 > x0 + CHUNK + MG || std::max(a.y, b.y) + 2 < y0 - MG ||
          std::min(a.y, b.y) - 2 > y0 + CHUNK + MG) { open = false; continue; }
      if (!open) { pieces.emplace_back(); open = true; }
      std::vector<GTile>& P = pieces.back();
      walk4(a.x, a.y, b.x, b.y, [&](int32_t x, int32_t y) {
        if (P.empty() || P.back().x != x || P.back().y != y) P.push_back(GTile{x, y});
      });
    }
    // (M3b fixer) a bridge runs straight: where the road crosses water (a pond, a lake, a stream) on a slant, the turns
    // are taken on dry land at the banks and the deck laid straight over the water (a staircase of deck tiles jogged
    // mid-deck, or broke into stubs where the bank is ragged). Only where no straight line with dry banks exists does
    // the slanting deck stay (the view draws a true diagonal as one span).
    // the water a road meets, from the plans and the land's own ground (a marsh's pools, the sea): a pure function of
    // the global tile, so every chunk judges a crossing alike
    std::unordered_map<uint64_t, bool> wetMemo;
    auto wetAt = [&](int32_t x, int32_t y) {
      const uint64_t key = (uint64_t)(uint32_t)x << 32 | (uint32_t)y;
      auto it = wetMemo.find(key);
      if (it != wetMemo.end()) return it->second;
      bool w = waterAtPlan(x, y);
      if (!w) { const TileF f = tile(x, y); const Ground gg = groundFor(f.eco, f.e, f.t, x, y); w = gg == Ground::Water || gg == Ground::DeepWater; }
      wetMemo.emplace(key, w);
      return w;
    };
    auto dryFoot = [&](int32_t x, int32_t y) {
      for (int oy = 0; oy <= hi; oy++)
        for (int ox = 0; ox <= hi; ox++) if (wetAt(x + ox, y + oy)) return false;
      return true;
    };
    // the deck from bank tile e to bank tile x on one line across the water, its turns on dry land (W: the tiles after
    // e up to and including x)
    auto crossing = [&](const GTile e, const GTile x, std::vector<GTile>& W) {
      const int32_t dx = x.x - e.x, dy = x.y - e.y;
      const int32_t adx = std::abs(dx), ady = std::abs(dy), sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
      const bool horiz = adx >= ady;
      // the line nearest the middle (rows between the banks for a crossing that runs east-west, columns for one that
      // runs north-south) whose legs on both banks are dry
      const int32_t lo = horiz ? std::min(e.y, x.y) : std::min(e.x, x.x), hiR = horiz ? std::max(e.y, x.y) : std::max(e.x, x.x);
      const int32_t mid = lo + (hiR - lo) / 2;
      for (int32_t off = 0; off <= hiR - lo; off++)
        for (int side = 0; side < 2; side++) {
          const int32_t r = side == 0 ? mid - off : mid + off + 1;
          if (r < lo || r > hiR || (off == 0 && side == 1 && mid + 1 > hiR)) continue;
          bool ok = true;
          W.clear();
          if (horiz) {
            for (int32_t y = e.y; y != r && ok; y += r > e.y ? 1 : -1) { const int32_t ny = y + (r > e.y ? 1 : -1); ok = dryFoot(e.x, ny); W.push_back(GTile{e.x, ny}); }
            for (int32_t k = 1; k <= adx && ok; k++) W.push_back(GTile{e.x + sx * k, r});
            ok = ok && dryFoot(x.x, r);
            for (int32_t y = r; y != x.y && ok; y += x.y > r ? 1 : -1) { const int32_t ny = y + (x.y > r ? 1 : -1); ok = dryFoot(x.x, ny); W.push_back(GTile{x.x, ny}); }
          } else {
            for (int32_t xx = e.x; xx != r && ok; xx += r > e.x ? 1 : -1) { const int32_t nx = xx + (r > e.x ? 1 : -1); ok = dryFoot(nx, e.y); W.push_back(GTile{nx, e.y}); }
            for (int32_t k = 1; k <= ady && ok; k++) W.push_back(GTile{r, e.y + sy * k});
            ok = ok && dryFoot(r, x.y);
            for (int32_t xx = r; xx != x.x && ok; xx += x.x > r ? 1 : -1) { const int32_t nx = xx + (x.x > r ? 1 : -1); ok = dryFoot(nx, x.y); W.push_back(GTile{nx, x.y}); }
          }
          if (ok) return true;
        }
      return false;
    };
    // (M11) a summit's trail (relief_plan.cpp: its legs end on dry ground on the mountain, and it carries the spurs' id
    // scheme with the shrine's local id, 0x340 ..) is laid as planned: no crossing to straighten
    const bool summitTrail = rp.cls == 2 && rp.a && (idLocal(rp.id) & 0x7F8u) == 0x340u;
    for (std::vector<GTile>& P : pieces) {
      if (summitTrail) break;
      for (size_t i = 1; i + 1 < P.size(); i++) {
        if (dryFoot(P[i].x, P[i].y) || !dryFoot(P[i - 1].x, P[i - 1].y)) continue;
        size_t j = i;
        while (j + 1 < P.size() && !dryFoot(P[j].x, P[j].y)) j++;
        if (!dryFoot(P[j].x, P[j].y)) break;   // no dry far bank in this piece
        const GTile e0 = P[i - 1], x0t = P[j];   // the last dry tile before, the first dry tile after
        const int32_t adx = std::abs(x0t.x - e0.x), ady = std::abs(x0t.y - e0.y);
        bool straight = true;   // is the run already a straight line?
        for (size_t k = i; k < j && straight; k++) if (P[k].x != e0.x && P[k].y != e0.y) straight = false;
        if (straight || std::min(adx, ady) == 0) { i = j; continue; }
        // a marsh's bank is ragged: when no line works between the banks' first dry tiles, the turns may move a few
        // tiles further onto the land either side
        std::vector<GTile> W;
        size_t ia = 0, jb = 0;
        bool found = false;
        for (size_t ext = 0; ext <= 4 && !found; ext++) {
          if (i - 1 < ext || j + ext >= P.size()) break;
          ia = i - 1 - ext; jb = j + ext;
          if (!dryFoot(P[ia].x, P[ia].y) || !dryFoot(P[jb].x, P[jb].y)) continue;
          found = crossing(P[ia], P[jb], W);
        }
        if (!found) { i = j; continue; }
        std::vector<GTile> Q(P.begin(), P.begin() + (long)ia + 1);
        for (const GTile& t : W) if (t.x != P[jb].x || t.y != P[jb].y) Q.push_back(t);
        const size_t resume = Q.size();
        Q.insert(Q.end(), P.begin() + (long)jb, P.end());
        P.swap(Q);
        i = resume;
      }
    }
    for (const std::vector<GTile>& P : pieces)
      for (const GTile& t : P) {
        if (t.x + 2 < x0 - 1 || t.x - 2 > x0 + CHUNK || t.y + 2 < y0 - 1 || t.y - 2 > y0 + CHUNK) continue;
        for (int oy = 0; oy <= hi; oy++)
          for (int ox = 0; ox <= hi; ox++) {
            int32_t gx = t.x + ox, gy = t.y + oy;
            if (!rin(gx, gy)) continue;
            if (ca && gx >= ca->x0 && gy >= ca->y0 && gx < ca->x1 && gy < ca->y1) continue;
            if (cb && gx >= cb->x0 && gy >= cb->y0 && gx < cb->x1 && gy < cb->y1) continue;
            size_t ii = ri(gx, gy);
            if (used[ii]) continue;
            road[ii] = std::max(road[ii], code);
            if (summitTrail) trailT[ii] = 1;
          }
      }
  };
  for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
    for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++)
      if (B.bridge[B.at(gx, gy)] && !used[ri(gx, gy)]) road[ri(gx, gy)] = 1;
  for (const RoadPlan& rp : own.plan.roads) if (rp.cls != 2) drawRoad(rp);
  for (int k = 0; k < 9; k++)
    for (const RoadPlan& rp : RD[k]->plan.roads) if (rp.cls == 2) drawRoad(rp);
  // (M2) where a troll keeps a toll, the road crosses even a stream by a bridge (never a ford)
  std::vector<GTile> tolls;
  for (int k = 0; k < 9; k++)
    for (const SitePlan& p : RD[k]->plan.sites)
      if (p.type == SiteType::Vignette && p.kind == (uint8_t)VignetteKind::TollBridge && p.ex > x0 - 12 && p.ey > y0 - 12 && p.ex < x0 + CHUNK + 12 &&
          p.ey < y0 + CHUNK + 12)
        tolls.push_back(GTile{p.ex, p.ey});
  // (M2 fixer round 2) a toll crossing is one straight bridge: from the head (two tiles from the post toward the road)
  // straight over the water the way the road steps onto it, then a short stretch of road on the far bank to the road's
  // own line there. The road's other wet tiles near it (a slanting road's staircase of deck pieces) stay water. All of it
  // is judged from the plans (waterAtPlan, the roads' rasters), so every chunk lays the same bridge.
  {
    static const int TDX[4] = {1, 0, -1, 0}, TDY[4] = {0, 1, 0, -1};
    for (int k = 0; k < 9; k++)
      for (const SitePlan& p : RD[k]->plan.sites) {
        if (p.type != SiteType::Vignette || p.kind != (uint8_t)VignetteKind::TollBridge) continue;
        if (p.ex < x0 - 16 || p.ey < y0 - 16 || p.ex >= x0 + CHUNK + 16 || p.ey >= y0 + CHUNK + 16) continue;
        const int side = (int)((p.seed >> 24) & 3), along = (int)((p.seed >> 26) & 3);
        const int32_t hx = p.ex - TDX[side] * 2, hy = p.ey - TDY[side] * 2;
        std::vector<GTile> deck;
        int32_t lx = 0, ly = 0;
        bool landed = false;
        for (int s = 1; s <= 8; s++) {
          const int32_t tx = hx + TDX[along] * s, ty = hy + TDY[along] * s;
          if (waterAtPlan(tx, ty)) deck.push_back(GTile{tx, ty});
          else { lx = tx; ly = ty; landed = true; break; }
        }
        if (!landed || deck.empty()) continue;
        // the road's line on the far bank: the nearest dry raster tile of any road within 6 tiles of the landing,
        // beyond the water (the roads' rasters walked from their plans, as drawRoad lays them)
        int32_t rx = lx, ry = ly;
        int64_t best = INT64_MAX;
        uint8_t rcode = 1;
        const int64_t far0 = (int64_t)(lx - hx) * TDX[along] + (int64_t)(ly - hy) * TDY[along];
        for (int q = 0; q < 9; q++)
          for (const RoadPlan& rp : RD[q]->plan.roads)
            for (size_t n = 0; n + 1 < rp.pts.size(); n++) {
              const GTile &a = rp.pts[n], &b = rp.pts[n + 1];
              if (std::max(a.x, b.x) < lx - 6 || std::min(a.x, b.x) > lx + 6 || std::max(a.y, b.y) < ly - 6 || std::min(a.y, b.y) > ly + 6) continue;
              walk4(a.x, a.y, b.x, b.y, [&](int32_t qx, int32_t qy) {
                if (std::abs(qx - lx) > 6 || std::abs(qy - ly) > 6) return;
                if ((int64_t)(qx - hx) * TDX[along] + (int64_t)(qy - hy) * TDY[along] < far0) return;
                if (waterAtPlan(qx, qy)) return;
                const int64_t d = dist2(qx, qy, lx, ly) * 4096 + (int64_t)(qy - ly + 8) * 64 + (qx - lx + 8);
                if (d < best) { best = d; rx = qx; ry = qy; rcode = rp.cls == 0 ? 3 : rp.cls == 1 ? 2 : 1; }
              });
            }
        // the slanting road's own wet tiles round the crossing are dropped (the straight deck replaces them)
        for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
          for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++) {
            if (!road[ri(gx, gy)]) continue;
            bool nearDeck = false, onDeck = false;
            for (const GTile& t : deck) {
              if (t.x == gx && t.y == gy) onDeck = true;
              if (std::abs(t.x - gx) <= 3 && std::abs(t.y - gy) <= 3) nearDeck = true;
            }
            if (nearDeck && !onDeck && waterAtPlan(gx, gy)) road[ri(gx, gy)] = 0;
          }
        for (const GTile& t : deck) if (rin(t.x, t.y)) road[ri(t.x, t.y)] = std::max<uint8_t>(road[ri(t.x, t.y)], 1);
        walk4(lx, ly, rx, ry, [&](int32_t qx, int32_t qy) {
          if (rin(qx, qy) && !waterAtPlan(qx, qy) && !used[ri(qx, qy)]) road[ri(qx, qy)] = std::max(road[ri(qx, qy)], rcode);
        });
      }
  }
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) {
      uint8_t code = road[ri(x0 + lx, y0 + ly)];
      if (!code) continue;
      int i = c.at(lx, ly);
      size_t bi = (size_t)(ly + BR) * BW + (lx + BR);
      Ground g = (Ground)c.ground[i];
      if (g == Ground::Water || g == Ground::DeepWater) {
        bool ford = B.riverW[bi] == 1 && !B.lake[bi];
        for (const GTile& t : tolls) if (dist2(t.x, t.y, x0 + lx, y0 + ly) <= 7 * 7) ford = false;
        c.ground[i] = (uint8_t)(ford ? Ground::Dirt : Ground::Bridge);
      } else if (g == Ground::Bridge || g == Ground::Road) {
        // keep
      } else {
        c.ground[i] = (uint8_t)(code >= 2 ? Ground::Road : Ground::Dirt);
      }
      // (M1 round 3) a road that runs up to a dungeon's door leads to it, it does not pave it over
      // (M2) nor does a road or another place's track that crosses a wayside place or a wonder wipe its stamped props
      // (the lone grave's cairn, the camp's fire): it runs between them. A toll bridge's road is its own.
      bool keepProp = c.prop[i] == (uint8_t)((int)Prop::IronDoor + 1) || c.prop[i] == (uint8_t)((int)Prop::CaveEntrance + 1);
      for (size_t k = 0; !keepProp && c.prop[i] && k < keeps.size(); k++)
        keepProp = x0 + lx >= keeps[k].x0 && y0 + ly >= keeps[k].y0 && x0 + lx < keeps[k].x1 && y0 + ly < keeps[k].y1;
      if (!keepProp) c.prop[i] = 0;
      reserved[(size_t)i] = 1;
    }
  // 6: relief. Settlements and sites stand on one level, terraced back to the land's own level around them.
  // the flat zone is a rounded rectangle around the footprint with a wobbling edge, so the terraces around a town
  // follow the land's grain instead of drawing a box
  // (M2) The levels are worked on a wider grid (LB = 6 tiles round the chunk): the base rect's levels where it has
  // them, the natural level (and the lakes' level) on the outer ring. After the flats, four passes take out the relief
  // slivers (owner note 3: cliffs on open ground that read as thin lines): a tile with three or four neighbours on one
  // other level (a 1-wide finger, notch, island or pit) joins them, a 1-tile ridge or channel joins its sides, and a
  // 1-tile-deep terrace is raised into its upper step (a stacked cliff). Each pass needs one more tile of border, so
  // the levels are exact out to 3 tiles round the chunk, so the final fix-up below is exact out to 2, as the cliff and
  // ramp bits (and the landings, which read 2 tiles out) need. (M11 integration: LB 6 and a fourth pass, from 5 and
  // three: the stacked tiers left 9-10 slivers a window on seeds 6 and 8. M11 fixer r1: LB 7, the same four passes, so
  // the fix-up covers the full 2-tile ring: it ran within 1 tile, and the chunks either side of a seam disagreed on a
  // landing 2 tiles out, leaving a level step with no step bit on its lower tile.)
  constexpr int LB = 7, LW = CHUNK + 2 * LB;
  std::vector<uint8_t> lev((size_t)LW * LW), lakeL((size_t)LW * LW, 0);
  {
    const int32_t lx0 = x0 - LB, ly0 = y0 - LB;
    std::vector<uint8_t> lakeMin((size_t)LW * LW, 255);
    for (int32_t ry = regionOf(ly0 - 4); ry <= regionOf(ly0 + LW + 3); ry++)
      for (int32_t rx = regionOf(lx0 - 4); rx <= regionOf(lx0 + LW + 3); rx++) {
        std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
        for (const Lake& Lk : H->lakes) {
          const int32_t R = 2 * Lk.r + 2;
          if (Lk.x + R < lx0 || Lk.x - R >= lx0 + LW || Lk.y + R < ly0 || Lk.y - R >= ly0 + LW) continue;
          if (lakeUnderSettlement(Lk)) continue;
          const uint8_t lakeLv = (uint8_t)fineLevel(Lk.x, Lk.y);
          for (int32_t y = std::max(ly0, Lk.y - R); y < std::min(ly0 + LW, Lk.y + R + 1); y++)
            for (int32_t x = std::max(lx0, Lk.x - R); x < std::min(lx0 + LW, Lk.x + R + 1); x++) {
              const int32_t rr = lakeRadius(Lk, x, y);
              if (dist2(x, y, Lk.x, Lk.y) >= (int64_t)rr * rr) continue;
              uint8_t& m = lakeMin[(size_t)(y - ly0) * LW + (x - lx0)];
              m = std::min(m, lakeLv);
            }
        }
      }
    for (int y = 0; y < LW; y++)
      for (int x = 0; x < LW; x++) {
        const int32_t gx = lx0 + x, gy = ly0 + y;
        const size_t i = (size_t)y * LW + x;
        if (B.in(gx, gy)) {
          lev[i] = B.level[B.at(gx, gy)];
          lakeL[i] = B.lake[B.at(gx, gy)];
        } else {
          lakeL[i] = lakeMin[i] != 255;
          // (the sea is level 0, as tile() makes it; land of level 2 or more is never sea, so only low land asks)
          if (lakeL[i]) lev[i] = lakeMin[i];
          else {
            const int nl = fineLevel(gx, gy);
            lev[i] = (uint8_t)(nl == 0 || (nl == 1 && waterE(gx, gy) < ELEV_SEA) ? 0 : nl);
          }
        }
      }
  }
  // (M11, VISION_PLAN 11.3 and 15.32: a river never climbs a step) the rivers' beds: every hydro segment near the
  // grid is walked from its start with its surface level (RiverSeg::lv) falling to the relief level of each tile it
  // passes (fineLevel, the base level: a pure function, so every chunk walks a segment alike); the river's tiles take
  // the lower of their own level and the river's. Where the land rises across a river's course the bed is cut down
  // through it (a gorge with the banks as faces); where the river drops, its water pours over the step (a fall).
  // rivG: the river's tiles on the grid (the sliver passes leave them alone); flowG: the flow there (1 +x, 2 -x,
  // 3 +y, 4 -y: the segment's dominant axis).
  std::vector<uint8_t> rivG((size_t)LW * LW, 0), flowG((size_t)LW * LW, 0);
  {
    const int32_t lx0 = x0 - LB, ly0 = y0 - LB;
    std::vector<uint8_t> rl((size_t)LW * LW, 255);
    auto baseLv = [&](int32_t gx, int32_t gy) {
      if (gx >= lx0 && gy >= ly0 && gx < lx0 + LW && gy < ly0 + LW) {
        const size_t i = (size_t)(gy - ly0) * LW + (gx - lx0);
        if (!lakeL[i]) return (int)lev[i];
      }
      return fineLevel(gx, gy);
    };
    std::vector<GTile> P;
    std::vector<uint8_t> Ps;
    for (int32_t ry = regionOf(ly0 - 4); ry <= regionOf(ly0 + LW + 3); ry++)
      for (int32_t rx = regionOf(lx0 - 4); rx <= regionOf(lx0 + LW + 3); rx++) {
        std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
        for (const RiverSeg& s : H->segs) {
          if (std::max(s.x0, s.x1) + 3 < lx0 || std::min(s.x0, s.x1) - 3 >= lx0 + LW || std::max(s.y0, s.y1) + 3 < ly0 || std::min(s.y0, s.y1) - 3 >= ly0 + LW) continue;
          const int lo = s.w >= 3 ? -1 : 0, hi = s.w == 1 ? 0 : s.w == 4 ? 2 : 1;
          const int32_t ddx = s.x1 - s.x0, ddy = s.y1 - s.y0;
          const uint8_t fl = (uint8_t)(std::abs(ddx) >= std::abs(ddy) ? (ddx >= 0 ? 1 : 2) : (ddy >= 0 ? 3 : 4));
          // every way the water runs here (high nibble: 1 +x, 2 -x, 4 +y, 8 -y): the main axis, and the other where the
          // segment slants by a third or more
          uint8_t dirs = 0;
          if (3 * std::abs(ddx) >= std::abs(ddy) && ddx) dirs |= ddx > 0 ? 1 : 2;
          if (3 * std::abs(ddy) >= std::abs(ddx) && ddy) dirs |= ddy > 0 ? 4 : 8;
          const uint8_t flw = (uint8_t)(fl | dirs << 4);
          // the surface down the segment: falling with the land from lv, never below lvEnd (the next segment's start)
          P.clear();
          walk4(s.x0, s.y0, s.x1, s.y1, [&](int32_t x, int32_t y) { P.push_back(GTile{x, y}); });
          Ps.assign(P.size(), 0);
          int cur = s.lv;
          for (size_t k = 0; k < P.size(); k++) {
            if (cur > s.lvEnd) cur = std::max<int>(s.lvEnd, std::min(cur, baseLv(P[k].x, P[k].y)));
            Ps[k] = (uint8_t)cur;
          }
          // two drops within 6 tiles become one fall (taller) at the first: no pool shut in between two falls
          {
            int nd = (int)P.size(), ndv = s.lvEnd;
            if (Ps.empty() || ndv >= Ps.back()) nd = -1;
            for (int k = (int)P.size() - 1; k >= 1; k--) {
              if (Ps[(size_t)k] >= Ps[(size_t)k - 1]) continue;
              if (nd >= 0 && nd - k < 6) {
                for (int q = k; q < nd; q++) Ps[(size_t)q] = (uint8_t)ndv;
              } else ndv = Ps[(size_t)k];
              nd = k;
            }
            if (nd >= 0 && nd < 6 && !Ps.empty()) {   // (a drop at the start: the segment before carries it)
              for (int q = 0; q < nd; q++) Ps[(size_t)q] = (uint8_t)ndv;
            }
          }
          for (size_t k = 0; k < P.size(); k++) {
            const int32_t x = P[k].x, y = P[k].y;
            for (int oy = lo; oy <= hi; oy++)
              for (int ox = lo; ox <= hi; ox++) {
                const int32_t gx = x + ox, gy = y + oy;
                if (gx < lx0 || gy < ly0 || gx >= lx0 + LW || gy >= ly0 + LW) continue;
                const size_t i = (size_t)(gy - ly0) * LW + (gx - lx0);
                // (the sea is the sea: tile()'s own test, so the ring beyond the base rect agrees with it)
                if (B.in(gx, gy) ? B.biome[B.at(gx, gy)] == (uint8_t)Biome::Ocean : (lev[i] == 0 && waterE(gx, gy) < ELEV_SEA)) continue;
                if (Ps[k] < rl[i]) { rl[i] = Ps[k]; flowG[i] = flw; }
                else if (Ps[k] == rl[i]) flowG[i] |= (uint8_t)(dirs << 4);
                rivG[i] = 1;
              }
          }
        }
      }
    // the river's surface on its tiles; a lake keeps its own level where a river runs into it or out of it, and the
    // river within 2 tiles of a lake runs at the lake's level at least (a lake spills over in a fall a little way down
    // its outflow, never sideways along a shore)
    for (size_t i = 0; i < rl.size(); i++)
      if (rivG[i] && rl[i] != 255 && !lakeL[i]) lev[i] = rl[i];
    for (int y = 0; y < LW; y++)
      for (int x = 0; x < LW; x++) {
        const size_t i = (size_t)y * LW + x;
        if (!rivG[i] || lakeL[i]) continue;
        int lk = -1;
        for (int oy = -2; oy <= 2; oy++)
          for (int ox = -2; ox <= 2; ox++) {
            const int xx = x + ox, yy = y + oy;
            if (xx < 0 || yy < 0 || xx >= LW || yy >= LW) continue;
            const size_t j = (size_t)yy * LW + xx;
            if (lakeL[j]) lk = std::max(lk, (int)lev[j]);
          }
        if (lk > (int)lev[i]) lev[i] = (uint8_t)lk;
      }
  }
  // (M11 fixer r2) level 7 is the summits' own: the land itself tops out at level 6, and only a summit shrine's crown
  // (or a place's flat the plans set on the top level) stands on 7. The big ranges' crests used to run out as broad
  // level-7 fields, and a summit shrine on one could get no crown at all (its tiers cannot rise past 7): it stood on a
  // flat white plain. Now every summit stands at least one tier over the top of its mountain. (Clamped after the
  // rivers and lakes, so a river, its banks and a lake on the crest stay level with each other.)
  for (uint8_t& v : lev) if (v > 6) v = 6;
  struct Flat { int32_t cx2, cy2, hw2, hh2, rc; int lv, step; uint64_t wob; bool crown = false; };   // centre and half sizes doubled
  std::vector<Flat> flats;
  // (M3c) the tiles inside some flat's terraces: their faces get a ramp twice as often (every 8 tiles, not 16), so a
  // knoll the terraces trim down to a small pocket of its old level is never left walled in by cliffs
  std::vector<uint8_t> terr((size_t)LW * LW, 0);
  // (M11) the tiles on some place's own flat (its footprint and the first terrace): no waterfall there (a town drains
  // the rivers that end in it, so a fall at its edge would pour into a stub)
  std::vector<uint8_t> coreG((size_t)LW * LW, 0);
  for (int k = 0; k < 9; k++) {
    const RegionData& D = *RD[k];
    for (size_t s = 0; s < D.plan.sites.size(); s++) {
      int lv = D.flatLevel[s].first;
      if (lv < 0) continue;
      const SitePlan& p = D.plan.sites[s];
      const bool st = isSettlement(p.type);
      const int pad = st ? 10 : 3;
      Flat f{2 * p.gx + p.w, 2 * p.gy + p.h, p.w + 2 * pad, p.h + 2 * pad, 0, lv, st ? 14 : 6, mix64(p.id ^ 0xF1A7u)};
      f.rc = std::min(f.hw2, f.hh2) * (st ? 7 : 10) / 10;   // corner radius (doubled units); small sites: a stadium
      // (M11 fixer r1) a summit shrine stands on a crown: its floor two levels over the flat, the first terrace one
      // (never above level 7), so the last of the climb is a stairway up two tiers to the shrine
      f.crown = p.type == SiteType::Shrine && (p.flags & SPF_SUMMIT) != 0;
      if (f.crown) f.step = 3;   // (its terraces 3 tiles apart: a stepped top, not a broad flat 20 tiles across)
      const int32_t reach = 4 * f.step + 14;
      if ((f.cx2 - f.hw2) / 2 - reach > x0 + CHUNK + LB || (f.cy2 - f.hh2) / 2 - reach > y0 + CHUNK + LB ||
          (f.cx2 + f.hw2) / 2 + reach < x0 - LB || (f.cy2 + f.hh2) / 2 + reach < y0 - LB)
        continue;
      flats.push_back(f);
    }
  }
  if (!flats.empty())
    for (int y = 0; y < LW; y++)
      for (int x = 0; x < LW; x++) {
        const int32_t gx = x0 - LB + x, gy = y0 - LB + y;
        int lo = 0, hi = 7, bestD = INT32_MAX, bestLv = -1, crownLv = -1;
        bool any = false, core = false;
        for (const Flat& f : flats) {
          // distance (tiles) outside the rounded rectangle, plus a slow wobble of +-5 tiles
          int32_t qx = std::max(0, std::abs(2 * gx + 1 - f.cx2) - (f.hw2 - f.rc)), qy = std::max(0, std::abs(2 * gy + 1 - f.cy2) - (f.hh2 - f.rc));
          int32_t out2 = (int32_t)isqrt((uint64_t)qx * qx + (uint64_t)qy * qy) - f.rc;   // doubled units
          if (out2 / 2 - 12 >= 4 * f.step) continue;   // (beyond its terraces whatever the wobble: skip the noise)
          // (two octaves, +-12 tiles: with one gentle octave the terraces ran as long straight lines parallel to
          // the town's rectangle, the "grid crack" through open plain)
          int32_t wob = (int32_t)(((int64_t)(vnoiseQ(gx, gy, 5, f.wob) - 32768) * 18) >> 16) +
                        (int32_t)(((int64_t)(vnoiseQ(gx, gy, 3, f.wob ^ 0x5A5Au) - 32768) * 6) >> 16);
          if (f.crown) wob /= 3;   // (the summit's tiers keep close to its floor)
          int32_t d = std::max(0, out2 / 2 + wob);
          int dev = d / f.step;
          if (dev >= 4) continue;
          any = true;
          if (dev == 0) core = true;
          // (the crown's own tiers: tight round the floor, its edge wobbling a little: the top tier reaches 3 tiles
          // past the floor's pad, the next 4 more, so both stand in view of the shrine)
          if (f.crown) {
            // (M11 fixer r2) the crown reaches level 7 whatever the mountain's top (the land stops at 6): two tiers
            // over a crest of level 5 or lower, one over a crest of 6 (a floor and a first tier both on 7 made a
            // broad flat field instead of a crown)
            const int32_t cd = std::max(0, out2 / 2 + wob / 4);
            const int top = std::min(7, f.lv + 2);
            if (cd < 3) crownLv = std::max(crownLv, top);
            else if (cd < 7 && top - 1 > f.lv) crownLv = std::max(crownLv, top - 1);
          }
          lo = std::max(lo, f.lv - dev);
          hi = std::min(hi, f.lv + dev);
          if (d < bestD) { bestD = d; bestLv = f.lv; }
        }
        if (!any) continue;
        terr[(size_t)y * LW + x] = 1;
        coreG[(size_t)y * LW + x] = core;
        uint8_t& L = lev[(size_t)y * LW + x];
        // (M11) a river's bed is never raised by a place's terraces (it would climb onto them); on the place's own
        // flat it runs at the flat's level (its bridges stay level with the streets)
        if (rivG[(size_t)y * LW + x] && !core) { if (lo <= hi) L = (uint8_t)std::min((int)L, hi); }
        else if (lo <= hi) L = (uint8_t)std::clamp((int)L, lo, hi);
        else L = (uint8_t)bestLv;
        if (crownLv > (int)L && !rivG[(size_t)y * LW + x] && !lakeL[(size_t)y * LW + x]) L = (uint8_t)crownLv;
      }
  // cliff and ramp bits on the 1-tile border grid
  std::vector<uint8_t> bits(RW * RW, 0);
  // (M11) a chunk whose levels are one level all through (with a tile round it) has no step: the relief passes below
  // (knobs, landings, knolls) skip it (the sliver passes never make a step where there was none)
  bool anyStep = false;
  {
    const int l0 = lev[(size_t)(LB - 2) * LW + (LB - 2)];
    for (int32_t gy = y0 - 2; gy < y0 + CHUNK + 2 && !anyStep; gy++)
      for (int32_t gx = x0 - 2; gx < x0 + CHUNK + 2; gx++) if ((int)lev[(size_t)(gy - (y0 - LB)) * LW + (gx - (x0 - LB))] != l0) { anyStep = true; break; }
  }
  // (M11, 11.4: the plateau minimum) a raised knob of under 24 tiles (one level, every tile round it lower, off the
  // water) lying wholly inside this chunk at least 2 tiles from its edges is flattened into its surroundings (the
  // highest level round it), before the sliver passes (which then tidy what it leaves). No other chunk reads those
  // tiles' levels, so the decision is this chunk's alone; a knob nearer an edge keeps its level and gets a way up
  // below (the knoll pass).
  if (anyStep) {
    auto LI0 = [&](int32_t gx, int32_t gy) { return (size_t)(gy - (y0 - LB)) * LW + (gx - (x0 - LB)); };
    auto L0 = [&](int32_t gx, int32_t gy) { return (int)lev[LI0(gx, gy)]; };
    static const int DX4[4] = {1, -1, 0, 0}, DY4[4] = {0, 0, 1, -1};
    std::vector<uint8_t> seenK(ChunkData::N, 0);
    std::vector<int> comp;
    for (int sy = 2; sy < CHUNK - 2; sy++)
      for (int sx = 2; sx < CHUNK - 2; sx++) {
        if (seenK[(size_t)sy * CHUNK + sx]) continue;
        const int l = L0(x0 + sx, y0 + sy);
        if (l == 0) continue;
        comp.assign(1, sy * CHUNK + sx);
        seenK[(size_t)sy * CHUNK + sx] = 1;
        bool ok = true;
        int around = -1;
        for (size_t h = 0; h < comp.size() && ok; h++) {
          const int qx = comp[h] % CHUNK, qy = comp[h] / CHUNK;
          if (qx < 2 || qy < 2 || qx >= CHUNK - 2 || qy >= CHUNK - 2 || comp.size() >= 24 || lakeL[LI0(x0 + qx, y0 + qy)] ||
              rivG[LI0(x0 + qx, y0 + qy)] || B.lake[B.at(x0 + qx, y0 + qy)] || B.riverW[B.at(x0 + qx, y0 + qy)] || waterG(B.ground[B.at(x0 + qx, y0 + qy)])) { ok = false; break; }
          for (int k = 0; k < 4; k++) {
            const int nx = qx + DX4[k], ny = qy + DY4[k];
            const int ln = L0(x0 + nx, y0 + ny);
            if (ln > l) { ok = false; break; }
            if (ln < l) { around = std::max(around, ln); continue; }
            if (!seenK[(size_t)ny * CHUNK + nx]) { seenK[(size_t)ny * CHUNK + nx] = 1; comp.push_back(ny * CHUNK + nx); }
          }
        }
        if (!ok || around < 0) continue;
        for (int q : comp) lev[LI0(x0 + q % CHUNK, y0 + q / CHUNK)] = (uint8_t)around;
      }
  }
  // the sliver passes (pass k is exact for tiles within LB - k of the chunk)
  for (int pass = 1; pass <= LB - 3; pass++) {
    std::vector<uint8_t> nl = lev;
    for (int y = pass; y < LW - pass; y++)
      for (int x = pass; x < LW - pass; x++) {
        const size_t i = (size_t)y * LW + x;
        if (lakeL[i] || rivG[i]) continue;
        const int l = lev[i], n = lev[i - LW], s = lev[i + LW], w = lev[i - 1], e = lev[i + 1];
        const int nb[4] = {n, s, w, e};
        int hi = 0, lo = 0, minHi = 99, maxLo = -1;
        for (int k = 0; k < 4; k++) {
          if (nb[k] > l) { hi++; minHi = std::min(minHi, nb[k]); }
          if (nb[k] < l) { lo++; maxLo = std::max(maxLo, nb[k]); }
        }
        int to = l;
        if (hi >= 3) to = minHi;                                         // a notch, a pit
        else if (lo >= 3) to = maxLo;                                    // a finger, an island
        else if ((n > l && s < l) || (n < l && s > l)) to = std::max(n, s);   // a 1-deep terrace: one stacked cliff
        else if ((w > l && e < l) || (w < l && e > l)) to = std::max(w, e);
        else if (n > l && s > l) to = std::min(n, s);                   // a 1-wide channel
        else if (w > l && e > l) to = std::min(w, e);
        // a 1-wide ridge is lowered only where it ends (a neck joining two parts of a plateau along its line stays: lowering
        // it would cut the plateau in two, and the part beyond might keep no ramp)
        else if (n < l && s < l && !(w >= l && e >= l)) to = std::max(n, s);
        else if (w < l && e < l && !(n >= l && s >= l)) to = std::max(w, e);
        // (M11) a 1-deep terrace running on the diagonal (higher on two adjacent sides, lower on the other two): it
        // joins its upper step, a stacked face, like the straight ones
        else if (hi == 2 && lo == 2 && !(n > l && s > l) && !(w > l && e > l)) to = minHi;
        nl[i] = (uint8_t)to;
      }
    lev.swap(nl);
  }
  // (M11) a pass changes every tile at once, so a 1-deep terrace raised into its upper step while that step's tip was
  // lowered leaves a 1-tile island (or pit) behind: within 2 tiles of the chunk, where the levels round it are exact,
  // it joins its surroundings (the highest level round an island, the lowest round a pit)
  {
    std::vector<std::pair<size_t, uint8_t>> fix;
    for (int y = LB - 2; y < LB + CHUNK + 2; y++)
      for (int x = LB - 2; x < LB + CHUNK + 2; x++) {
        const size_t i = (size_t)y * LW + x;
        if (lakeL[i] || rivG[i]) continue;
        const int l = lev[i], n = lev[i - LW], s = lev[i + LW], w = lev[i - 1], e = lev[i + 1];
        if (n < l && s < l && w < l && e < l) fix.push_back({i, (uint8_t)std::max(std::max(n, s), std::max(w, e))});
        else if (n > l && s > l && w > l && e > l) fix.push_back({i, (uint8_t)std::min(std::min(n, s), std::min(w, e))});
        else {
          // (M11 integration) the last pass's own leftovers, seen by --endless on seeds 6 and 8 (10 slivers a window):
          // a 1-tile notch or finger (three sides on one other level) joins them, a 1-deep terrace its upper step
          const int nb[4] = {n, s, w, e};
          int hi = 0, lo = 0, minHi = 99, maxLo = -1;
          for (int k = 0; k < 4; k++) {
            if (nb[k] > l) { hi++; minHi = std::min(minHi, nb[k]); }
            if (nb[k] < l) { lo++; maxLo = std::max(maxLo, nb[k]); }
          }
          if (hi >= 3) fix.push_back({i, (uint8_t)minHi});
          else if (lo >= 3) fix.push_back({i, (uint8_t)maxLo});
          else if ((n > l && s < l) || (n < l && s > l)) fix.push_back({i, (uint8_t)std::max(n, s)});
          else if ((w > l && e < l) || (w < l && e > l)) fix.push_back({i, (uint8_t)std::max(w, e)});
        }
      }
    for (const auto& f : fix) lev[f.first] = f.second;
  }
  auto LI = [&](int32_t gx, int32_t gy) { return (size_t)(gy - (y0 - LB)) * LW + (gx - (x0 - LB)); };
  auto L = [&](int32_t gx, int32_t gy) { return (int)lev[LI(gx, gy)]; };
  const uint64_t rampSeed = seed ^ tag("relief.ramp");
  static const int DX4[4] = {1, -1, 0, 0}, DY4[4] = {0, 0, 1, -1};
  // the base rect's water at a tile of the 1-tile border grid: a river, a lake or the land's own water (not a footbridge)
  auto wetAtB = [&](int32_t gx, int32_t gy) {
    const size_t bi = B.at(gx, gy);
    return !B.bridge[bi] && (B.lake[bi] || B.riverW[bi] || waterG(B.ground[bi]));
  };
  // the ramp lattice (11.4): a south / north face's ways up at x = hash(row block) mod 16 (every 8 inside a place's
  // terraces, and (M11) on the mountains' upper tiers, level 5 and up, where the crests are small), an east / west
  // face's at y = hash(column block) mod 16; 3 tiles wide
  auto lattice = [&](int32_t gx, int32_t gy, bool ns) {
    const int per = terr[LI(gx, gy)] || L(gx, gy) >= 5 ? 7 : 15;
    if (ns) return ((gx + (int32_t)(tileHash(rampSeed, 0, gy >> 4) & 15)) & per) < 3;
    return ((gy + (int32_t)(tileHash(rampSeed, 1, gx >> 4) & 15)) & per) < 3;
  };
  // (M11, 11.3 and 11.4: stairs and switchbacks) a way up a stacked face (2 levels in one tile row) is a stairway in two
  // flights with a landing: the way-up tile (a lattice point, or a road or track crossing the face) is raised one level,
  // so it climbs the upper level from the landing and the tile below it (forced to a ramp) climbs to the landing. A
  // pure function of the levels within 2 tiles, which are exact here, so the chunks on both sides agree.
  // (after the sliver passes, which may carry a step a few tiles in from the ring: the test again for the passes below;
  // anyStacked: a face of 2 levels or more within 2 tiles (the landings); maxL: the highest level there (the knolls'
  // pass runs on the high ground, level 4 and up, where the ridges and the stacked tiers make knolls the lattice misses)
  bool anyStacked = false;
  int maxL = 0;
  {
    const int l0 = L(x0 - 2, y0 - 2);
    anyStep = false;
    for (int32_t gy = y0 - 2; gy < y0 + CHUNK + 2; gy++)
      for (int32_t gx = x0 - 2; gx < x0 + CHUNK + 2; gx++) {
        const int l = L(gx, gy);
        maxL = std::max(maxL, l);
        if (l != l0) anyStep = true;
        if (gx + 1 < x0 + CHUNK + 2 && std::abs(L(gx + 1, gy) - l) >= 2) anyStacked = true;
        if (gy + 1 < y0 + CHUNK + 2 && std::abs(L(gx, gy + 1) - l) >= 2) anyStacked = true;
      }
  }
  std::vector<uint8_t> forceRamp(RW * RW, 0);
  if (anyStacked) {
    std::vector<std::pair<size_t, uint8_t>> raise;
    for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
      for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++) {
        const int l = L(gx, gy);
        if (entrance[ri(gx, gy)] || wetAtB(gx, gy) || rivG[LI(gx, gy)]) continue;
        for (int k = 0; k < 4; k++) {
          const int32_t hx = gx + DX4[k], hy = gy + DY4[k];
          if (L(hx, hy) < l + 2) continue;
          const bool roadHere = road[ri(gx, gy)] != 0;
          if (!roadHere && !lattice(gx, gy, k >= 2)) continue;
          // the tile below the landing must be dry land of this level, clear of the face
          const int32_t sx = gx - DX4[k], sy = gy - DY4[k];
          if (L(sx, sy) != l || rivG[LI(sx, sy)] || (B.in(sx, sy) && wetAtB(sx, sy))) continue;
          raise.push_back({LI(gx, gy), (uint8_t)(l + 1)});
          if (rin(sx, sy)) forceRamp[ri(sx, sy)] = 1;
          forceRamp[ri(gx, gy)] = 1;
          break;
        }
      }
    for (const auto& r : raise) lev[r.first] = r.second;
  }
  // the way the water runs at a grid tile (from flowG), as a step
  auto flowDX = [&](size_t i) { const uint8_t f = flowG[i] & 15; return f == 1 ? 1 : f == 2 ? -1 : 0; };
  auto flowDY = [&](size_t i) { const uint8_t f = flowG[i] & 15; return f == 3 ? 1 : f == 4 ? -1 : 0; };
  // does the water at grid tile i run one step (dx, dy)?
  auto flowsTo = [&](size_t i, int dx, int dy) {
    const uint8_t d = flowG[i] >> 4;
    return (dx > 0 && (d & 1)) || (dx < 0 && (d & 2)) || (dy > 0 && (d & 4)) || (dy < 0 && (d & 8));
  };
  // the river runs on below a fall for 4 tiles at least (its own course, tile by tile; or a lake takes it): a river that
  // ends just below a step (a stub) pours into no fall
  auto runsOn = [&](int32_t gx, int32_t gy) {
    size_t i = LI(gx, gy);
    int fx = flowDX(i), fy = flowDY(i);
    for (int s = 0; s < 4; s++) {
      if (!fx && !fy) return false;
      gx += fx; gy += fy;
      i = LI(gx, gy);
      if (lakeL[i]) return true;
      if (!rivG[i]) return false;
      if (flowG[i]) { fx = flowDX(i); fy = flowDY(i); }
    }
    return true;
  };
  // the hillside test (11.2: "a slope is where H changes by 1 and the 4-tile slope is below the threshold"), judged
  // once per 6 x 6 cell of the land at the cell's middle, so a hillside runs on unbroken instead of flickering
  // between bank and slope tile by tile: the clean elevation climbs less than 0.4 of a level over 6 tiles
  struct GentleMemo { int32_t cx = INT32_MIN, cy = 0; bool v = false; };
  GentleMemo gm[16];
  // (M11 fixer r1) judged per 16 x 16 cell over 16 tiles (the same gradient threshold): on 6-tile cells a bank changed
  // between face and hillside every few tiles and read as a row of separate logs and mounds
  auto gentle = [&](int32_t gx, int32_t gy) {
    const int32_t cxx = floorDiv(gx, 16), cyy = floorDiv(gy, 16);
    GentleMemo& m = gm[(size_t)((cxx & 3) | (cyy & 3) << 2)];
    if (m.cx != cxx || m.cy != cyy) {
      const int32_t mx = cxx * 16 + 8, my = cyy * 16 + 8;
      const int32_t ex = std::abs(reliefE(mx + 8, my) - reliefE(mx - 8, my)), ey = std::abs(reliefE(mx, my + 8) - reliefE(mx, my - 8));
      m.cx = cxx; m.cy = cyy; m.v = ex + ey < H_STEP * 16 / 15;
    }
    return m.v;
  };
  for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
    for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++) {
      const int l = L(gx, gy);
      const int ln = L(gx, gy - 1), ls = L(gx, gy + 1), lw = L(gx - 1, gy), le = L(gx + 1, gy);
      uint8_t b = (uint8_t)l;
      size_t bi = (size_t)(gy - (y0 - BR)) * BW + (gx - (x0 - BR));
      const bool water = wetAtB(gx, gy);
      // (M11) a road's or track's deck over the water (a bridge, a ford) is a way, not water: where it meets a step it
      // climbs by stairs like the road on land
      const bool deck = water && road[ri(gx, gy)] != 0;
      const bool wet = water && !deck;
      if (ln > l || ls > l || lw > l || le > l) {
        bool ramp = false;
        if (!wet && !entrance[ri(gx, gy)]) {
          ramp = lattice(gx, gy, ln > l || ls > l);
          // (M3c) a mesa's stairway up its south side
          if (!ramp && ln > l && mesaStair(gx, gy)) ramp = true;
          // roads and tracks climb by stairs: the road's own tiles only (M11 fixer r1: the flanking tiles took stairs
          // too, so a flight ran three times the road's width and the road met it at a corner; a 4-connected road
          // always crosses a step on a road tile of its own)
          if (road[ri(gx, gy)]) ramp = true;
          if (forceRamp[ri(gx, gy)] || B.bridge[bi]) ramp = true;   // (a landing's flights; a footbridge's deck)
          // (M11 fixer r1, 11.2) a way up climbs one level: a ramp beside a tile 2 or more levels higher (a lattice
          // point, a road's flank or a stair's end whose landing could not be raised above) would let a walker skip
          // the landing, so it stays a face
          if (ramp && std::max(std::max(ln, ls), std::max(lw, le)) >= l + 2) ramp = false;
        }
        // (M11, VISION_PLAN 15.32) a road's or track's way up is built stone stairs (HEIGHT_STAIR); a gentle one-level
        // step of the lowlands and hills, away from any settlement's terraces, is a walkable hillside (HEIGHT_SLOPE)
        // where the land is gentle (below); a river's water pouring over the step
        // into this tile is a waterfall (HEIGHT_FALL, with the cliff: never under a bridge, a ford or a road, never on
        // the sea)
        bool roadUp = false, slope = false, fall = false;
        // (M11 fixer r2) built stone stairs where a paved road or a summit's trail climbs; a plain dirt track's way up
        // is a worn earth ramp (its own trodden earth: a second stone flight on a loose dirt track two tiles from the
        // road's stair read as a stair-bank-stair gate)
        if (ramp) roadUp = road[ri(gx, gy)] >= 2 || trailT[ri(gx, gy)] != 0 || (road[ri(gx, gy)] && (deck || B.bridge[bi]));
        const int hiN = std::max(std::max(ln, ls), std::max(lw, le));
        if (!ramp && !wet && !deck && !entrance[ri(gx, gy)] && hiN == l + 1 && hiN <= 3 && !terr[LI(gx, gy)]) {
          slope = gentle(gx, gy);
        }
        if (wet && B.biome[bi] != (uint8_t)Biome::Ocean && (B.riverW[bi] || B.lake[bi]) && !coreG[LI(gx, gy)]) {
          const size_t ti = LI(gx, gy);
          for (int k = 0; k < 4 && !fall; k++) {
            const int32_t ux = gx + DX4[k], uy = gy + DY4[k];
            const size_t ui = LI(ux, uy);
            if (lev[ui] <= l || !B.in(ux, uy)) continue;
            const size_t ub = B.at(ux, uy);
            if (B.bridge[ub] || (rin(ux, uy) && road[ri(ux, uy)])) continue;   // (no fall from under a bridge or a ford)
            if (B.lake[ub] || !B.riverW[ub] || !rivG[ui]) continue;            // the water above: the river's own course
            // the river above flows into this tile, and runs on below it (a lake takes it, or 4 tiles of river)
            fall = flowsTo(ui, -DX4[k], -DY4[k]) && (B.lake[bi] || (rivG[ti] && runsOn(gx, gy)));
          }
        }
        if (entrance[ri(gx, gy)]) b = (uint8_t)l;
        else if (slope) b |= Map::HEIGHT_SLOPE;
        else {
          b |= ramp ? Map::HEIGHT_RAMP : Map::HEIGHT_CLIFF;
          if (roadUp) b |= Map::HEIGHT_STAIR;
          if (fall && !ramp) b |= Map::HEIGHT_FALL;
        }
      }
      bits[ri(gx, gy)] = b;
    }
  // (M11, 11.4: every plateau reachable) a knoll or crest the lattice missed: a raised component (one level, every
  // tile round it lower) that lies within 4 tiles of this chunk and has no way up (no ramp or hillside on the tiles
  // below its edge that this chunk knows) gets one: its edge tile in this chunk with the lowest hash becomes a ramp.
  // Each chunk decides for its own tiles only, so a knoll across a chunk edge may get a way up from both sides; the
  // levels 3 and 4 tiles out are not exact here, which can only add a needless ramp or miss one the lattice (every 8
  // tiles on the upper tiers) all but always gives.
  if (anyStep && maxL >= 4) {
    constexpr int E = 4, EW = CHUNK + 2 * E;
    std::vector<uint8_t> seenK((size_t)EW * EW, 0);
    std::vector<int32_t> comp, edgeT;
    for (int sy = 0; sy < EW; sy++)
      for (int sx = 0; sx < EW; sx++) {
        if (seenK[(size_t)sy * EW + sx]) continue;
        const int32_t gx0 = x0 - E + sx, gy0 = y0 - E + sy;
        const int l = L(gx0, gy0);
        if (l == 0) continue;
        comp.assign(1, sy * EW + sx);
        seenK[(size_t)sy * EW + sx] = 1;
        bool whole = true, raised = true, way = false;
        edgeT.clear();
        for (size_t h = 0; h < comp.size() && raised; h++) {
          const int qx = comp[h] % EW, qy = comp[h] / EW;
          const int32_t gx = x0 - E + qx, gy = y0 - E + qy;
          // (water: an island, a lake's level, not a knoll; the base rect holds 2 tiles round the chunk, the grid's
          // lake and river marks the rest)
          if (B.in(gx, gy) ? wetAtB(gx, gy) : (lakeL[LI(gx, gy)] || rivG[LI(gx, gy)])) { raised = false; break; }
          for (int k = 0; k < 4; k++) {
            const int nx = qx + DX4[k], ny = qy + DY4[k];
            const int32_t ngx = gx + DX4[k], ngy = gy + DY4[k];
            const int ln = L(ngx, ngy);
            if (ln > l) { raised = false; break; }
            if (ln == l) {
              if (nx < 0 || ny < 0 || nx >= EW || ny >= EW) { whole = false; continue; }
              if (!seenK[(size_t)ny * EW + nx]) { seenK[(size_t)ny * EW + nx] = 1; comp.push_back(ny * EW + nx); }
              continue;
            }
            // a tile below the edge: its step bit is a way up, or a candidate for one
            if (rin(ngx, ngy)) {
              const uint8_t nb = bits[ri(ngx, ngy)];
              if (nb & (Map::HEIGHT_RAMP | Map::HEIGHT_SLOPE)) way = true;
              else if ((nb & Map::HEIGHT_CLIFF) && !(nb & Map::HEIGHT_FALL) && !wetAtB(ngx, ngy) && !entrance[ri(ngx, ngy)] &&
                       ngx >= x0 && ngy >= y0 && ngx < x0 + CHUNK && ngy < y0 + CHUNK &&
                       // (M11 fixer r1) a way up climbs one level: never beside a tile 2 levels higher
                       std::max(std::max(L(ngx + 1, ngy), L(ngx - 1, ngy)), std::max(L(ngx, ngy + 1), L(ngx, ngy - 1))) == ln + 1)
                edgeT.push_back(ri(ngx, ngy));
            }   // (beyond the 1-tile border its bit is not known here: another way up there only adds one)
          }
        }
        if (!raised || !whole || way || edgeT.empty() || comp.size() > 400) continue;
        const uint64_t ks = rampSeed ^ 0x6B4Eu;
        int32_t best = edgeT[0];
        uint64_t bh = UINT64_MAX;
        for (int32_t t : edgeT) {
          const int32_t tx = x0 - 1 + t % RW, ty = y0 - 1 + t / RW;
          const uint64_t hh = tileHash(ks, tx, ty);
          if (hh < bh) { bh = hh; best = t; }
        }
        bits[(size_t)best] = (uint8_t)((bits[(size_t)best] & ~Map::HEIGHT_CLIFF) | Map::HEIGHT_RAMP);
      }
  }
  // (M11) the plunge pool: the river tile a fall pours into, and its neighbours across the stream, are open deep water
  // (never a bridge, a ford or a road)
  for (int32_t gy = y0 - 1; gy < y0 + CHUNK + 1; gy++)
    for (int32_t gx = x0 - 1; gx < x0 + CHUNK + 1; gx++) {
      if (!(bits[ri(gx, gy)] & Map::HEIGHT_FALL)) continue;
      const size_t ti = LI(gx, gy);
      const int fx = flowDX(ti), fy = flowDY(ti);
      if (!fx && !fy) continue;
      for (int side = -1; side <= 1; side++) {
        const int32_t px = gx + fx + side * fy, py = gy + fy + side * fx;
        if (px < x0 || py < y0 || px >= x0 + CHUNK || py >= y0 + CHUNK) continue;
        const size_t pb = B.at(px, py);
        if (!B.riverW[pb] || B.bridge[pb] || road[ri(px, py)] || (bits[ri(px, py)] & (Map::HEIGHT_FALL | Map::HEIGHT_RAMP))) continue;
        const int pi = c.at(px - x0, py - y0);
        if (groundWater((Ground)c.ground[pi])) c.ground[pi] = (uint8_t)Ground::DeepWater;
      }
    }
  // (M11, 11.2: rock only on the steep cores, drawn as stacked cliffs) the mountains' rock stays only on their stacked
  // faces (macro.cpp groundFor makes the rest of their high ground walkable scree, turf and snow)
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) {
      const int32_t gx = x0 + lx, gy = y0 + ly;
      const uint8_t hb = bits[ri(gx, gy)];
      if (!(hb & Map::HEIGHT_CLIFF) || (hb & Map::HEIGHT_FALL)) continue;
      const int i = c.at(lx, ly);
      if (c.eco[i] != (uint8_t)Eco::Mountain || reserved[(size_t)i] || used[ri(gx, gy)] || groundWater((Ground)c.ground[i])) continue;
      const int l = hb & Map::HEIGHT_LEVEL;
      bool stacked = false;
      for (int k = 0; k < 4 && !stacked; k++) stacked = L(gx + DX4[k], gy + DY4[k]) >= l + 2;
      if (stacked && l >= 4) c.ground[i] = (uint8_t)Ground::Rock;
    }
  // (M3) the towns' own relief on their tiles (the terraces of a terraced town; elsewhere identical)
  for (size_t k = 0; k < townH.size(); k++) if (townH[k]) bits[k] = (uint8_t)(townH[k] - 1);
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) c.height[c.at(lx, ly)] = bits[ri(x0 + lx, y0 + ly)];
  // 6b (M2): ecotones (VISION_PLAN 11.6). Along a border between two land biomes, a band 4 to 8 tiles wide (by the
  // pair) carries the other biome and its weight in Map::blend: bits 0-3 the other biome, 4-7 its weight, 4 at the
  // border falling to 0 at the band's edge, so both sides meet half and half. Read from the eco field on a grid 4
  // tiles wider than the chunk (the base rect's ecos, then biomeLite on the outer ring), so chunk seams agree. The
  // view dithers the ground with it; the vegetation takes the other side's flora at the same odds.
  // (M3c) Between ANY two ecos, also two of one family (meadow and prairie, birch wood and the mixed forest): ecoNb is
  // the partner eco, the blend's family nibble its family (the tile's own family for a same-family pair), and the
  // band's width is the pair's (ecoHalfW: wide and soft between grasslands, narrow at the marsh edges).
  {
    constexpr int EB = 4, EW = CHUNK + 2 * EB;
    // (a chunk whose base rect holds one eco has no border within 2 tiles of it: only the outermost tiles of a
    // neighbour's band would reach in, at weight 1, so the ring is not worth building)
    bool mixed = false;
    for (size_t k = 1; k < B.eco.size() && !mixed; k++) if (B.eco[k] != B.eco[0]) mixed = true;
    if (mixed) {
      std::vector<uint8_t> ee((size_t)EW * EW);
      for (int y = 0; y < EW; y++)
        for (int x = 0; x < EW; x++) {
          const int32_t gx = x0 - EB + x, gy = y0 - EB + y;
          const size_t k = (size_t)y * EW + x;
          if (B.in(gx, gy)) ee[k] = B.eco[B.at(gx, gy)];
          else {
            Eco e = Eco::Meadow;
            biomeLite(gx, gy, &e);
            const Eco ae = apronEco(gx, gy, ecoFamily(e));   // (fixer r2) a dune town's apron, as the base rect has it
            ee[k] = (uint8_t)(ae != Eco::COUNT ? ae : e);
          }
        }
      // land ecos that blend (the sea, the beaches and bare mountain rock keep their hard edges, as before)
      auto blends = [](uint8_t v) {
        const Biome q = ecoFamily((Eco)v);
        return q == Biome::Plains || q == Biome::Forest || q == Biome::Autumn || q == Biome::Taiga || q == Biome::Snow || q == Biome::Swamp || q == Biome::Desert;
      };
      // the disc of offsets nearest first (built once; a function-local static is initialised thread-safely)
      struct Off { int dx, dy, d2; };
      static const std::vector<Off> offs = [] {
        std::vector<Off> v;
        for (int dy = -EB; dy <= EB; dy++)
          for (int dx = -EB; dx <= EB; dx++)
            if ((dx || dy) && dx * dx + dy * dy <= EB * EB) v.push_back(Off{dx, dy, dx * dx + dy * dy});
        std::stable_sort(v.begin(), v.end(), [](const Off& a, const Off& b) { return a.d2 < b.d2; });
        return v;
      }();
      for (int ly = 0; ly < CHUNK; ly++)
        for (int lx = 0; lx < CHUNK; lx++) {
          const uint8_t self = ee[(size_t)(ly + EB) * EW + lx + EB];
          if (!blends(self)) continue;
          if (used[ri(x0 + lx, y0 + ly)]) continue;   // town ground keeps its own look
          for (const Off& of : offs) {
            const uint8_t o = ee[(size_t)(ly + EB + of.dy) * EW + lx + EB + of.dx];
            if (o == self || !blends(o)) continue;
            const int hw = ecoHalfW((Eco)self, (Eco)o);
            const int32_t dd = (int32_t)isqrt((uint64_t)of.d2 * 100);   // tenths of a tile
            const int32_t w = (4 * (hw * 10 - dd + 10) + hw * 5) / (hw * 10);   // 4 beside the border .. 0 past hw
            if (w > 0) {
              c.blend[c.at(lx, ly)] = (uint8_t)((std::min<int32_t>(w, 8) << 4) | ((int)ecoFamily((Eco)o) & 15));
              c.ecoNb[c.at(lx, ly)] = o;
            }
            break;
          }
        }
    }
  }
  // 6a (M4, owner carry-over: the marsh boardwalks): a road or track over the open water of a marsh (the Swamp family's
  // pools, not a river) is a boardwalk on posts, not a river bridge: its tiles carry Map::BOARDWALK_MARK with the
  // marsh's plank paving (4), so the view draws them as the stilt towns' walks and joins them seamlessly. Where the road
  // steps diagonally, the inner corner of each step on its south side is decked too (water with the walk to its north,
  // to one side and on the diagonal between), so the staircase is a walk two tiles wide that the view lays as one
  // straight diagonal deck with every part of it walkable; a turn gets its inner corner the same way. Decided from the
  // plan's roads and the base water only (the road array and the base rect cover the chunk's 1-tile margin), so the
  // chunks on either side of an edge agree.
  {
    auto marshWater = [&](int32_t gx, int32_t gy) {
      if (!B.in(gx, gy)) return false;
      const size_t b = B.at(gx, gy);
      const Ground g0 = (Ground)B.ground[b];
      return (g0 == Ground::Water || g0 == Ground::DeepWater) && ecoFamily((Eco)B.eco[b]) == Biome::Swamp && B.riverW[b] == 0 && !B.bridge[b];
    };
    auto deck = [&](int32_t gx, int32_t gy) { return rin(gx, gy) && road[ri(gx, gy)] && !used[ri(gx, gy)] && marshWater(gx, gy); };
    const uint8_t mark = (uint8_t)(Map::BOARDWALK_MARK | 4);
    for (int ly = 0; ly < CHUNK; ly++)
      for (int lx = 0; lx < CHUNK; lx++) {
        const int32_t gx = x0 + lx, gy = y0 + ly;
        const int i = c.at(lx, ly);
        if (deck(gx, gy)) {
          if (c.ground[i] != (uint8_t)Ground::Bridge) continue;
          c.blend[i] = mark;
          c.ecoNb[i] = c.eco[i];
          continue;
        }
        if (!rin(gx, gy) || road[ri(gx, gy)] || used[ri(gx, gy)] || !marshWater(gx, gy)) continue;
        if (!groundWater((Ground)c.ground[i])) continue;
        const bool fill = deck(gx, gy - 1) && ((deck(gx + 1, gy) && deck(gx + 1, gy - 1)) || (deck(gx - 1, gy) && deck(gx - 1, gy - 1)));
        if (!fill) continue;
        c.ground[i] = (uint8_t)Ground::Bridge;
        c.prop[i] = 0;
        c.blend[i] = mark;
        c.ecoNb[i] = c.eco[i];
        reserved[(size_t)i] = 1;
      }
  }
  // 6b (M2 fixer round 2): massifs. Every named summit (a Peak landmark: the region's highest point) and every range's
  // labelled crest stands as a mountain you can see: a GreatPeak (7 x 3) at the label point with lesser spires on its
  // flanks and behind it (a range's crest gets a longer chain). The layout is a pure function of the label point; the
  // label point is first nudged so the great peak's footprint lies inside one chunk; each spire is placed by the chunk
  // that holds its whole footprint, and only on level, dry, open ground (a spire that does not fit is left out, and
  // the great peak tries a few nearby spots in a fixed order).
  {
    struct Spire { int dx, dy; bool great; };
    static const Spire kSummit[] = {{0, 0, true}, {-6, 1, false}, {6, 0, false}, {-4, -3, false}, {4, -4, false}};
    static const Spire kRange[] = {{0, 0, true}, {-6, 1, false}, {6, 0, false}, {-4, -3, false}, {4, -4, false},
                                   {-10, -1, false}, {10, -2, false}, {-13, 1, false}, {13, 1, false}};
    // (M11 fixer r1) a summit shrine's floor and the ground before it (where its trail arrives) stay open: a spire
    // stood right in front of the shrine, hiding the last of the climb
    std::vector<std::array<int32_t, 4>> summitKeep;
    for (int k = 0; k < 9; k++)
      for (const SitePlan& sp0 : RD[k]->plan.sites)
        if (sp0.type == SiteType::Shrine && (sp0.flags & SPF_SUMMIT))
          summitKeep.push_back({sp0.gx - 3, sp0.gy - 1, sp0.gx + sp0.w + 3, sp0.gy + sp0.h + 6});
    auto fits = [&](int32_t ax, int32_t ay, Prop p, bool strict) {
      int fw = 1, fh = 1;
      art::wildFootprint(p, fw, fh);
      const int lx0 = ax - fw / 2 - x0, lx1 = ax + fw / 2 - x0, ly0 = ay - fh + 1 - y0, ly1 = ay - y0;
      if (lx0 < 0 || ly0 < 0 || lx1 >= CHUNK || ly1 >= CHUNK) return false;
      for (const auto& r : summitKeep)
        if (ax + fw / 2 >= r[0] && ax - fw / 2 < r[2] && ay >= r[1] && ay - fh + 1 < r[3]) return false;
      const int lv = c.height[c.at(ax - x0, ay - y0)] & Map::HEIGHT_LEVEL;
      for (int ly = ly0; ly <= ly1; ly++)
        for (int lx = lx0; lx <= lx1; lx++) {
          const int ii = c.at(lx, ly);
          const int32_t gx = x0 + lx, gy = y0 + ly;
          const uint8_t hb = c.height[ii];
          if ((hb & Map::HEIGHT_STEP) || (hb & Map::HEIGHT_LEVEL) != lv) return false;
          if (reserved[(size_t)ii] || c.prop[ii] || c.wall[ii] || c.bldg[ii] || used[ri(gx, gy)] || road[ri(gx, gy)]) return false;
          const Ground gg = (Ground)c.ground[ii];
          if (groundSolid(gg) || waterG(c.ground[ii]) || gg == Ground::Bridge || gg == Ground::Road || gg == Ground::Dirt || gg == Ground::Plaza ||
              gg == Ground::Farmland)
            return false;
        }
      if (strict)   // the way over the range (a road) stays open: no spire within 3 tiles of one
        for (int ly = ly0 - 3; ly <= ly1 + 3; ly++)
          for (int lx = lx0 - 3; lx <= lx1 + 3; lx++) {
            const int32_t gx = x0 + lx, gy = y0 + ly;
            if (rin(gx, gy) && road[ri(gx, gy)]) return false;
          }
      return true;
    };
    auto place = [&](int32_t ax, int32_t ay, Prop p) {
      int fw = 1, fh = 1;
      art::wildFootprint(p, fw, fh);
      for (int32_t y = ay - fh + 1; y <= ay; y++)
        for (int32_t x = ax - fw / 2; x <= ax + fw / 2; x++) {
          const int ii = c.at(x - x0, y - y0);
          c.prop[ii] = (uint8_t)((int)(x == ax && y == ay ? p : Prop::Filler) + 1);
          reserved[(size_t)ii] = 1;
        }
    };
    for (int k = 0; k < 9; k++)
      for (const LandmarkPlan& l : RD[k]->plan.landmarks) {
        const bool range = l.kind == LandmarkKind::Range;
        if (!range && !(l.kind == LandmarkKind::Peak && l.size == 60)) continue;   // (a wonder's label is a Peak of size 24)
        if (l.x < x0 - 24 || l.x >= x0 + CHUNK + 24 || l.y < y0 - 12 || l.y >= y0 + CHUNK + 12) continue;
        // the great peak's footprint inside one chunk: x - 3 .. x + 3, y - 2 .. y
        int32_t gx = l.x, gy = l.y + 2;
        const int32_t mx = floorMod(gx, CHUNK), my = floorMod(gy, CHUNK);
        if (mx < 3) gx += 3 - mx; else if (mx > CHUNK - 4) gx -= mx - (CHUNK - 4);
        if (my < 2) gy += 2 - my;
        // a few spots round it in a fixed order: the first that fits (judged by the chunk that holds it) carries the
        // massif. Every chunk walks the same order, and a spot in another chunk is that chunk's to judge: this chunk
        // only places what lies inside it, so a massif is never doubled (the spots are a chunk's width apart or inside
        // the same chunk: the nudge keeps them all in the great peak's own chunk).
        static const int kTry[][2] = {{0, 0}, {-2, 0}, {2, 0}, {0, 2}, {-2, 2}, {2, 2}, {0, -2}};
        const int32_t cxg = floorDiv(gx, CHUNK), cyg = floorDiv(gy, CHUNK);
        if (cxg != cx || cyg != cy) {
          // the great peak's chunk is another one: only the spires that fall in this chunk, around the spot that
          // chunk would choose. Without its data the choice is not knowable here, so the flanks are laid only round the
          // first spot (a pure choice), and only where they fit.
          const Spire* sp = range ? kRange : kSummit;
          const int ns = range ? (int)(sizeof(kRange) / sizeof(kRange[0])) : (int)(sizeof(kSummit) / sizeof(kSummit[0]));
          for (int s = 1; s < ns; s++) {
            const int32_t ax = gx + sp[s].dx, ay = gy + sp[s].dy;
            if (fits(ax, ay, Prop::Peak, true)) place(ax, ay, Prop::Peak);
          }
          continue;
        }
        int32_t bx = INT32_MIN, by = 0;
        for (auto& t : kTry) {
          const int32_t tx = gx + t[0], ty = gy + t[1];
          if (floorDiv(tx - 3, CHUNK) != cx || floorDiv(tx + 3, CHUNK) != cx || floorDiv(ty - 2, CHUNK) != cy || floorDiv(ty, CHUNK) != cy) continue;
          if (fits(tx, ty, Prop::GreatPeak, true)) { bx = tx; by = ty; break; }
        }
        if (bx == INT32_MIN) { bx = gx; by = gy; }   // no room for the great one: its flanks still mark the summit
        else place(bx, by, Prop::GreatPeak);
        const Spire* sp = range ? kRange : kSummit;
        const int ns = range ? (int)(sizeof(kRange) / sizeof(kRange[0])) : (int)(sizeof(kSummit) / sizeof(kSummit[0]));
        for (int s = 1; s < ns; s++) {
          const int32_t ax = gx + sp[s].dx, ay = gy + sp[s].dy;
          if (fits(ax, ay, Prop::Peak, true)) place(ax, ay, Prop::Peak);
        }
        (void)by;
      }
  }
  // 6c (M2): peaks (VISION_PLAN 11.3): rock spires (Prop::Peak, 3 x 2 with Filler) along the crests of the high ground,
  // so a range reads as a range. The land is cut into 8 x 8 cells (aligned to chunks, so a cell never straddles two);
  // a cell's candidate is its highest point of the relief field among the tiles of level 6 or more (a pure choice), and
  // it stands if it is a crest (the relief falls away on both sides along some direction) and no neighbouring cell's
  // candidate within 5 tiles stands higher (both chunks compute the same pure answers). The chunk then vetoes a spire
  // whose footprint (and a tile round it) is not level, dry and clear: cliffs, ramps, roads, anything built or stamped.
  {
    bool high = false;
    for (int ly = 0; ly < CHUNK && !high; ly++)
      for (int lx = 0; lx < CHUNK; lx++) if ((c.height[c.at(lx, ly)] & Map::HEIGHT_LEVEL) >= 6) { high = true; break; }
    if (high) {
      constexpr int NC = CHUNK / 8 + 2;   // the chunk's cells and one round them
      struct PC { int32_t x = 0, y = 0, e = INT32_MIN; bool crest = false; };
      PC pc[NC * NC];
      const int32_t ci0 = x0 / 8 - 1, cj0 = y0 / 8 - 1;
      const uint64_t ts = seed ^ tag("peak.tie");
      for (int j = 0; j < NC; j++)
        for (int i = 0; i < NC; i++) {
          PC& p = pc[j * NC + i];
          const int32_t bx = (ci0 + i) * 8, by = (cj0 + j) * 8;
          for (int32_t y = by + 2; y <= by + 6; y++)
            for (int32_t x = bx + 1; x <= bx + 6; x++) {
              const int32_t er = reliefE(x, y);
              if (levelOf(er) < 6) continue;   // (natLevel is levelOf(reliefE))
              const int32_t e = er + (int32_t)(tileHash(ts, x, y) & 63);
              if (e > p.e) { p.e = e; p.x = x; p.y = y; }
            }
          if (p.e == INT32_MIN) continue;
          // the spine of a range (the plate ridge field strong here): every cell's high point is a peak, so the range
          // stands as a wall of spires; elsewhere a crest: some direction along which the relief falls on both sides
          if (tile(p.x, p.y).ridge >= Q(0.42)) { p.crest = true; continue; }
          const int32_t e0 = reliefE(p.x, p.y), dlt = Q(0.0035);
          static const int dd[4][2] = {{5, 0}, {0, 5}, {4, 4}, {4, -4}};
          for (int k = 0; k < 4 && !p.crest; k++)
            if (reliefE(p.x + dd[k][0], p.y + dd[k][1]) < e0 - dlt && reliefE(p.x - dd[k][0], p.y - dd[k][1]) < e0 - dlt) p.crest = true;
          // (M2 fixer) and the land must fall a level close by (within 4 tiles): a spire in the middle of a flat
          // high snowfield stood there like a cone set down on the ground
          if (p.crest) {
            const int lv0 = levelOf(e0);
            bool edge = false;
            static const int rd[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
            for (int k = 0; k < 8 && !edge; k++)
              for (int r = 2; r <= 4 && !edge; r++)
                if (levelOf(reliefE(p.x + rd[k][0] * r, p.y + rd[k][1] * r)) < lv0) edge = true;
            p.crest = edge;
          }
        }
      for (int j = 1; j < NC - 1; j++)
        for (int i = 1; i < NC - 1; i++) {
          const PC& p = pc[j * NC + i];
          if (!p.crest) continue;
          bool yield = false;
          for (int oj = -1; oj <= 1 && !yield; oj++)
            for (int oi = -1; oi <= 1; oi++) {
              if (!oi && !oj) continue;
              const PC& q = pc[(j + oj) * NC + i + oi];
              if (q.crest && (q.e > p.e || (q.e == p.e && (q.x < p.x || (q.x == p.x && q.y < p.y)))) && std::abs(q.x - p.x) <= 5 && std::abs(q.y - p.y) <= 5) { yield = true; break; }
            }
          if (yield) continue;
          const int bx = p.x - x0, by = p.y - y0;
          const int lv = c.height[c.at(bx, by)] & Map::HEIGHT_LEVEL;
          if (lv < 6) continue;
          bool ok = true;
          for (int oy = -2; oy <= 1 && ok; oy++)
            for (int ox = -2; ox <= 2 && ok; ox++) {
              const int lx = bx + ox, ly = by + oy;
              const int32_t gx = x0 + lx, gy = y0 + ly;
              if (!rin(gx, gy)) { ok = false; break; }
              const uint8_t hb = bits[ri(gx, gy)];
              const bool foot = oy >= -1 && oy <= 0 && ox >= -1 && ox <= 1;
              if ((hb & Map::HEIGHT_STEP) || (hb & Map::HEIGHT_LEVEL) != lv) { if (foot || (hb & Map::HEIGHT_RAMP)) ok = false; continue; }
              if (road[ri(gx, gy)] || used[ri(gx, gy)]) ok = false;
              if (!foot) continue;
              const int ii = c.at(lx, ly);
              if (reserved[(size_t)ii] || c.prop[ii] || c.wall[ii] || c.bldg[ii] || groundSolid((Ground)c.ground[ii]) || waterG(c.ground[ii]) ||
                  c.ground[ii] == (uint8_t)Ground::Bridge || c.ground[ii] == (uint8_t)Ground::Road || c.ground[ii] == (uint8_t)Ground::Dirt)
                ok = false;
            }
          // never on a pass: a road within 4 tiles means the way over the range runs here
          for (int oy = -4; oy <= 4 && ok; oy++)
            for (int ox = -4; ox <= 4 && ok; ox++)
              if (rin(p.x + ox, p.y + oy) && road[ri(p.x + ox, p.y + oy)]) ok = false;
          if (!ok) continue;
          for (int oy = -1; oy <= 0; oy++)
            for (int ox = -1; ox <= 1; ox++) { c.prop[c.at(bx + ox, by + oy)] = (uint8_t)((int)Prop::Filler + 1); reserved[(size_t)c.at(bx + ox, by + oy)] = 1; }
          c.prop[c.at(bx, by)] = (uint8_t)((int)Prop::Peak + 1);
          // (M2 fixer) scree at its foot: a few rocks fallen from the spire tie it to the ground
          {
            const Biome pb = (Biome)c.biome[c.at(bx, by)];
            const bool snowy = pb == Biome::Snow || pb == Biome::Taiga || pb == Biome::Mountain;
            static const int sc[6][2] = {{-2, 0}, {2, 0}, {-1, 1}, {1, 1}, {-2, -1}, {2, 1}};
            for (int k = 0; k < 6; k++) {
              const uint32_t h = (uint32_t)tileHash(ts ^ 0x5C2Eu, p.x + sc[k][0], p.y + sc[k][1]);
              if (h % 3 == 0) continue;
              const int lx = bx + sc[k][0], ly = by + sc[k][1];
              if (lx < 0 || ly < 0 || lx >= CHUNK || ly >= CHUNK) continue;
              const int ii = c.at(lx, ly);
              if (reserved[(size_t)ii] || c.prop[ii] || (c.height[ii] & Map::HEIGHT_STEP) || (c.height[ii] & Map::HEIGHT_LEVEL) != lv ||
                  groundSolid((Ground)c.ground[ii]) || waterG(c.ground[ii]) || c.ground[ii] == (uint8_t)Ground::Road || c.ground[ii] == (uint8_t)Ground::Dirt)
                continue;
              c.prop[ii] = (uint8_t)((int)(snowy ? ((h >> 4) & 1 ? Prop::SnowRock : Prop::Rock) : ((h >> 4) & 1 ? Prop::Boulder : Prop::Rock)) + 1);
              reserved[(size_t)ii] = 1;
            }
          }
        }
    }
  }
  // 6d (M11): cairns along the summit trails (relief_plan.cpp): every 14 tiles of a trail's raster, two tiles to one
  // side of it (by a hash of the place), on free, level, dry, open ground. Decided from the plan's raster, so every
  // chunk agrees on where a cairn may stand; the chunk that holds the tile places it.
  for (int k = 0; k < 9; k++)
    for (const RoadPlan& rp : RD[k]->plan.roads) {
      // (a summit's trail carries the spurs' id scheme: 0x800 | the shrine's local id, 0x340 ..; relief_plan.cpp)
      if (rp.cls != 2 || !rp.a || (idLocal(rp.id) & 0x7F8u) != 0x340u) continue;
      const SitePlan* sp = nullptr;
      for (const SitePlan& s : RD[k]->plan.sites) if (s.id == rp.a) { sp = &s; break; }
      if (!sp || !(sp->flags & SPF_SUMMIT)) continue;
      int32_t bx0 = INT32_MAX, by0 = INT32_MAX, bx1 = INT32_MIN, by1 = INT32_MIN;
      for (const GTile& t : rp.pts) { bx0 = std::min(bx0, t.x); by0 = std::min(by0, t.y); bx1 = std::max(bx1, t.x); by1 = std::max(by1, t.y); }
      if (bx1 + 3 < x0 || by1 + 3 < y0 || bx0 - 3 >= x0 + CHUNK || by0 - 3 >= y0 + CHUNK) continue;
      int n = 0;
      for (size_t q = 0; q + 1 < rp.pts.size(); q++) {
        const GTile &a = rp.pts[q], &b = rp.pts[q + 1];
        const bool horiz = std::abs(b.x - a.x) >= std::abs(b.y - a.y);
        walk4(a.x, a.y, b.x, b.y, [&](int32_t tx, int32_t ty) {
          if (n++ % 14 != 9) return;
          const int side = (tileHash(sp->seed ^ 0xCA12u, tx, ty) & 1) ? 1 : -1;
          const int32_t cxx = horiz ? tx : tx + 2 * side, cyy = horiz ? ty + 2 * side : ty;
          if (cxx < x0 || cyy < y0 || cxx >= x0 + CHUNK || cyy >= y0 + CHUNK) return;
          const int ii = c.at(cxx - x0, cyy - y0);
          if (reserved[(size_t)ii] || c.prop[ii] || c.wall[ii] || c.bldg[ii] || road[ri(cxx, cyy)] || used[ri(cxx, cyy)]) return;
          if (c.height[ii] & (Map::HEIGHT_STEP | Map::HEIGHT_FALL)) return;
          for (int d = 0; d < 4; d++)   // (never in the way of a ramp)
            if (bits[ri(cxx + (d == 0) - (d == 1), cyy + (d == 2) - (d == 3))] & Map::HEIGHT_RAMP) return;
          const Ground gg = (Ground)c.ground[ii];
          if (groundSolid(gg) || waterG(c.ground[ii]) || gg == Ground::Bridge || gg == Ground::Road || gg == Ground::Dirt) return;
          c.prop[ii] = (uint8_t)((int)Prop::Cairn + 1);
          reserved[(size_t)ii] = 1;
        });
      }
    }
  // 7: vegetation by per-tile hash (off reserved tiles, roads, cliffs, ramps and their approaches)
  //
  // (forest fixer, Oct 2026) "Trees shouldn't get closer than one person wide": no two solid wild props (trees, bushes,
  // stumps, logs, rocks, cacti) ever touch, diagonals included, so there is always a free tile between two trunks and
  // a wood can be walked through. Every tile's roll is a pure function of the tile (its hash, its eco, the relief
  // level, the density noise, its base ground); a tile is a solid CANDIDATE when its roll could put something solid
  // there. A solid prop is placed only when its key beats the key of every candidate among its 8 neighbours (key:
  // trees over the rest, then a jittered one-tile-in-four lattice preference that packs the woods tighter, then a
  // hash). Two touching solids would each have to beat the other, so it cannot happen, and since the candidates are
  // computed the same way from either side of a chunk edge, it holds across edges too. A suppressed tree leaves
  // walkable undergrowth (ferns, grass, flowers, mushrooms) on its tile, so the woods keep their density; the crowns
  // (2-3 tiles wide) still close over the gaps.
  //
  // (M3c Wildlands) The flora is the eco's own (ecoFlora). In an ecotone the tile, at the blend's odds, mixes in its
  // partner eco's flora: a tree of the other kind where both grow trees, the open side's grass where a wood thins out,
  // a lone tree from the wood (only from the low end of the roll: r < 0.05) on the open side, the partner's ground
  // cover half the time. So the solids an ecotone can add are bounded whatever the partner is: a candidate is the
  // tile's own solid roll, or, where the ecotone gate could pass (its hash < 0.5, the most any blend weight allows), a
  // tree candidate if r < 0.05 or its own roll is solid. That needs no knowledge of the neighbour chunk's blends.
  const uint64_t vs = mix64(seed ^ tag("veg"));
  auto thinOf = [](int lv) { return lv >= 7 ? Q(0.16) : lv >= 6 ? Q(0.32) : lv >= 5 ? Q(0.55) : lv >= 4 ? Q(0.8) : Q_ONE; };
  // the eco's own roll plus the high ground's scree (rocks and boulders strewn over the mountains' shoulders)
  auto ownRoll = [&](int32_t x, int32_t y, Eco e, int32_t r, int32_t q, uint64_t pk, int32_t dens, int lv, Ground g, bool river) {
    Prop p = ecoFlora(e, r, q, pk, dens, thinOf(lv), lv, g, river, x, y, vs);
    const Biome b = ecoFamily(e);
    if (p == Prop::COUNT && lv >= 6 && b != Biome::Swamp && g != Ground::Water && !groundSolid(g) && r > Q(0.80) && r < Q(0.80) + (lv >= 7 ? Q(0.022) : Q(0.012))) {
      auto pick = [&](std::initializer_list<Prop> l) { return *(l.begin() + (size_t)(pk % l.size())); };
      p = b == Biome::Snow || b == Biome::Taiga ? pick({Prop::SnowRock, Prop::SnowRock, Prop::Boulder}) : pick({Prop::Rock, Prop::Boulder, Prop::MossRock});
    }
    return p;
  };
  auto solidP = [](Prop p) { return p != Prop::COUNT && propSolid(p); };
  // (M11) each tile's own roll from the candidates' pass below (Prop + 1; 0: not worked), kept for the scatter: the
  // same roll on the same base ground need not be worked twice
  std::vector<uint16_t> ownA((size_t)RW * RW, 0);
  auto roll = [&](int32_t x, int32_t y, int32_t dens, Eco e, int lv, int ew, Eco nb, Ground g, bool nearWater, bool river) {
    const int32_t r = hq(tileHash(vs, x, y));
    const int32_t q = hq(tileHash(vs ^ 0x400u, x, y));
    const uint64_t pk = tileHash(vs ^ 0x300u, x, y);
    const bool wet = g == Ground::Water || g == Ground::DeepWater;
    Prop p;
    if (nearWater && !wet && !groundSolid(g) && g != Ground::Ice && watersideFlora(e) && r < Q(0.30)) p = q < Q(0.6) ? Prop::Reeds : Prop::TallGrass;
    else {
      const uint16_t oc = ownA[ri(x, y)];
      p = oc && g == (Ground)B.ground[(size_t)(y - (y0 - BR)) * BW + (x - (x0 - BR))] ? (Prop)(oc - 1) : ownRoll(x, y, e, r, q, pk, dens, lv, g, river);
    }
    // the salt flats' rims: saltbush where the pan meets other land
    if (e == Eco::SaltFlats && ew && r < Q(0.09)) p = Prop::Saltbush;
    if (ew && nb != e && !wet && hq(tileHash(vs ^ 0x7E0u, x, y)) < ew * Q(0.125)) {
      const Prop pn = ownRoll(x, y, nb, r, q, pk, dens, lv, g, river);
      const bool so = solidP(p), sn = solidP(pn);
      if (so) p = pn;                                  // the other wood's tree, or the open side's grass
      else if (sn) { if (r < Q(0.05)) p = pn; }        // a lone outlier from the wood
      else if ((pk >> 9) & 1) p = pn;                  // the partner's ground cover, half the time
    }
    return p;
  };
  auto solidRank = [&](Prop p) { return p == Prop::COUNT || !propSolid(p) ? 0 : art::isTreeProp(p) ? 2 : 1; };
  // a solid prop's key: rank (trees first), the jittered lattice slot (one tile of every 2 x 2 cell), a hash
  auto keyOf = [&](int32_t x, int32_t y, int rank) {
    const uint64_t cell = tileHash(vs ^ 0x5107u, x >> 1, y >> 1);
    const bool slot = (int)(x & 1) == (int)(cell & 1) && (int)(y & 1) == (int)((cell >> 1) & 1);
    return ((uint64_t)rank << 62) | ((uint64_t)slot << 61) | (tileHash(vs ^ 0x9A1u, x, y) >> 3);
  };
  // the solid candidates over the chunk and a one-tile ring (base eco and ground, never a town's: town ground grows no
  // scatter)
  std::vector<uint64_t> candKey((size_t)RW * RW, 0);   // 0: not a candidate
  std::vector<int32_t> densA((size_t)RW * RW, 0);      // the woods' density noise, worked once per tile
  for (int32_t y = y0 - 1; y < y0 + CHUNK + 1; y++)
    for (int32_t x = x0 - 1; x < x0 + CHUNK + 1; x++) {
      const size_t bi = (size_t)(y - (y0 - BR)) * BW + (x - (x0 - BR));
      const Eco e = (Eco)B.eco[bi];
      const int lv = bits[ri(x, y)] & Map::HEIGHT_LEVEL;
      const int32_t dens = densA[ri(x, y)] = fbmQ(x, y, 3, 3, vs ^ 0xD3u);
      const int32_t r = hq(tileHash(vs, x, y));
      const Prop po = ownRoll(x, y, e, r, hq(tileHash(vs ^ 0x400u, x, y)), tileHash(vs ^ 0x300u, x, y), dens, lv, (Ground)B.ground[bi], B.riverW[bi] != 0);
      ownA[ri(x, y)] = (uint16_t)((int)po + 1);
      int rank = solidRank(po);
      const Ground bg = (Ground)B.ground[bi];
      if (bg != Ground::Water && bg != Ground::DeepWater && hq(tileHash(vs ^ 0x7E0u, x, y)) < Q(0.5) && (r < Q(0.05) || rank)) rank = 2;
      if (rank) candKey[ri(x, y)] = keyOf(x, y, rank);
    }
  // what stood before the scatter (dens, scree, vignettes): the scatter keeps its solids off those too (within the chunk)
  std::vector<uint8_t> pre(c.prop, c.prop + ChunkData::N);
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) {
      const int i = c.at(lx, ly);
      if (reserved[(size_t)i] || c.prop[i] || c.wall[i] || c.bldg[i]) continue;
      const int32_t x = x0 + lx, y = y0 + ly;
      if (bits[ri(x, y)] & (Map::HEIGHT_CLIFF | Map::HEIGHT_RAMP)) continue;
      bool nearRamp = false, nearRoad = false, nearWater = false, nearCliff = false;
      for (int k = 0; k < 8; k++) {
        static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1}, dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        int32_t nx = x + dx[k], ny = y + dy[k];
        uint8_t nb = bits[ri(nx, ny)];
        if (nb & Map::HEIGHT_RAMP) nearRamp = true;
        if ((nb & Map::HEIGHT_CLIFF) && k < 4) nearCliff = true;
        if (road[ri(nx, ny)]) nearRoad = true;
        size_t bi = (size_t)(ny - (y0 - BR)) * BW + (nx - (x0 - BR));
        if (k < 4 && (B.lake[bi] || B.riverW[bi] || waterG(B.ground[bi]))) nearWater = true;   // (M3c: the land's own ponds too)
      }
      if (nearRamp) continue;
      // (M11 fixer r2) nothing tall stands over a waterfall: no tree or rock beside it or up to 3 tiles south of it (a
      // crown there hid the falling water); judged on this chunk's own tiles
      {
        bool byFall = false;
        for (int oy = -1; oy <= 3 && !byFall; oy++)
          for (int ox = -2; ox <= 2; ox++) {
            const int qx = lx + ox, qy = ly - oy;
            if (qx < 0 || qy < 0 || qx >= CHUNK || qy >= CHUNK) continue;
            if (c.height[c.at(qx, qy)] & Map::HEIGHT_FALL) { byFall = true; break; }
          }
        if (byFall) continue;
      }
      Ground g = (Ground)c.ground[i];
      if (g == Ground::Road || g == Ground::Bridge || g == Ground::Plaza || g == Ground::Farmland || g == Ground::StoneFloor || g == Ground::Dirt) continue;
      const size_t bci = (size_t)(ly + BR) * BW + lx + BR;
      const Eco e = (Eco)B.eco[bci];
      const int lv = bits[ri(x, y)] & Map::HEIGHT_LEVEL;
      const uint8_t bl = c.blend[i];
      const int ew = (bl >> 4) <= 8 ? bl >> 4 : 0;   // (M3 fixer: above 8 a town's paving mark)
      Prop p = roll(x, y, densA[ri(x, y)], e, lv, ew, (Eco)c.ecoNb[i], g, nearWater, B.riverW[bci] != 0);
      if (p == Prop::COUNT) continue;
      // keep the road verges and cliff feet free of anything solid
      if ((nearRoad || nearCliff) && propSolid(p)) continue;
      // (M3c fixer round 2, review: seed 18's shingle beach only 95.67 % walkable) nor a neck of land between waters
      // (water within 2 tiles on both sides, west and east or north and south): a boulder there sealed off the spit
      // beyond it. The base rect's 2-tile border holds every tile looked at.
      if (propSolid(p)) {
        auto wB = [&](int32_t qx, int32_t qy) { const size_t q = (size_t)(qy - (y0 - BR)) * BW + (qx - (x0 - BR)); return waterG(B.ground[q]) || B.lake[q] || B.riverW[q]; };
        // (the four axes through the tile, the diagonals too: a spit running north-east is a neck as well)
        bool neck = false;
        static const int ax4[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
        for (int k = 0; k < 4 && !neck; k++) {
          const int dx = ax4[k][0], dy = ax4[k][1];
          neck = (wB(x - dx, y - dy) || wB(x - 2 * dx, y - 2 * dy)) && (wB(x + dx, y + dy) || wB(x + 2 * dx, y + 2 * dy));
        }
        if (neck) continue;
        // nor a boulder or bush hard against a town wall or gate tower (2 tiles clear, within the chunk)
        bool byWall = false;
        for (int oy = -2; oy <= 2 && !byWall; oy++)
          for (int ox = -2; ox <= 2 && !byWall; ox++) {
            const int lx2 = lx + ox, ly2 = ly + oy;
            if (lx2 >= 0 && ly2 >= 0 && lx2 < CHUNK && ly2 < CHUNK && c.wall[c.at(lx2, ly2)]) byWall = true;
          }
        if (byWall) continue;
      }
      // (forest fixer) the spacing rule: a solid only where it outranks every solid candidate round it (and nothing
      // solid already stands beside it)
      const int rank = solidRank(p);
      if (rank) {
        const uint64_t k = keyOf(x, y, rank);
        bool wins = true;
        for (int oy = -1; oy <= 1 && wins; oy++)
          for (int ox = -1; ox <= 1 && wins; ox++) {
            if (!ox && !oy) continue;
            const uint64_t nk = candKey[ri(x + ox, y + oy)];
            if (nk > k || (nk == k && (oy < 0 || (oy == 0 && ox < 0)))) wins = false;   // (equal keys: the earlier tile wins)
            const int lx2 = lx + ox, ly2 = ly + oy;
            if (lx2 >= 0 && ly2 >= 0 && lx2 < CHUNK && ly2 < CHUNK) {
              const int pp = pre[(size_t)c.at(lx2, ly2)];
              if (pp && propSolid((Prop)(pp - 1))) wins = false;
            }
          }
        if (!wins) {
          // a tree that gives way leaves the undergrowth of its wood; a bush or rock leaves the ground bare
          if (rank < 2 || g == Ground::Water || g == Ground::DeepWater) continue;
          p = ecoUnder(e, (uint32_t)(tileHash(vs ^ 0x6A3u, x, y) % 100));
          if (p == Prop::COUNT) continue;
        }
      }
      c.prop[i] = (uint8_t)((int)p + 1);
    }
  // (M2 fixer round 2) the low summer flora (grass tufts, flowers, ferns, mushrooms) never stands on paving or on snow,
  // whoever put it there (a town's dressing before its streets were paved, a vignette's sprinkle, the scatter on ground
  // the view covers with snow: taiga and snowfields from level 4, mountains from 5). A leafy bush on snow wears snow.
  // (M3c) The Wildlands' summer covers likewise; the cold-hardy ones (lichen, cotton grass) keep to the snow.
  for (int ly = 0; ly < CHUNK; ly++)
    for (int lx = 0; lx < CHUNK; lx++) {
      const int i = c.at(lx, ly);
      const int pr = c.prop[i];
      if (!pr) continue;
      const Prop pp = (Prop)(pr - 1);
      const bool summer = pp == Prop::Flowers1 || pp == Prop::Flowers2 || pp == Prop::Flowers3 || pp == Prop::TallGrass || pp == Prop::Fern ||
                          pp == Prop::Mushrooms || pp == Prop::Wildflowers || pp == Prop::PrairieGrass || pp == Prop::Heather ||
                          pp == Prop::JungleFern || pp == Prop::SilverFern || pp == Prop::Petals || pp == Prop::GlowCaps || pp == Prop::DryBrush ||
                          pp == Prop::Agave || pp == Prop::Blightweed;
      const bool hardy = pp == Prop::Lichen || pp == Prop::CottonGrass || pp == Prop::Saltbush || pp == Prop::Wrack || pp == Prop::Shells;
      const bool leafy = pp == Prop::Bush || pp == Prop::BerryBush;
      if (!summer && !hardy && !leafy) continue;
      const Ground g = (Ground)c.ground[i];
      const Biome b = (Biome)c.biome[i];
      const int lv = c.height[i] & Map::HEIGHT_LEVEL;
      const bool paved = g == Ground::Road || g == Ground::Plaza || g == Ground::StoneFloor || g == Ground::Bridge || g == Ground::WoodFloor;
      const bool snowy = g == Ground::Snow || g == Ground::Ice || ((b == Biome::Snow || b == Biome::Taiga) && lv >= 4) || (b == Biome::Mountain && lv >= 5);
      if ((summer || hardy) && paved) c.prop[i] = 0;
      else if (summer && snowy) c.prop[i] = 0;
      else if (hardy && g == Ground::Ice) c.prop[i] = 0;
      else if (leafy && snowy) c.prop[i] = (uint8_t)((int)Prop::SnowBush + 1);
    }
  // (M3c fixer, seed 34: a bush in a forest capital's gate passage) whoever put it there (a town's dressing, a
  // vignette, the scatter at a town's edge), nothing solid stands in a gate's three passage tiles or on the middle
  // tile's approaches: the gate stays a clean opening under its arch
  for (const GTile& gt : c.gates) {
    static const int gox[5] = {0, 1, 2, 1, 1}, goy[5] = {0, 0, 0, -1, 1};
    for (int k = 0; k < 5; k++) {
      const int32_t lx = gt.x + gox[k] - x0, ly = gt.y + goy[k] - y0;
      if (lx < 0 || ly < 0 || lx >= CHUNK || ly >= CHUNK) continue;
      const int i = c.at(lx, ly);
      if (c.prop[i] && propSolid((Prop)(c.prop[i] - 1))) c.prop[i] = 0;
    }
  }
  double ms = msSince(t0) - (nestedMs - nested0);
  stats.chunks++;
  stats.chunkMs += ms;
  stats.maxChunkMs = std::max(stats.maxChunkMs, ms);
}

}  // namespace ew
