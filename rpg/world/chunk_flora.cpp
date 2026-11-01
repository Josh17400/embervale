// L2b, the chunk pipeline's step 7 (SOLID 3.2: moved from chunkgen.cpp, unchanged): the vegetation, the flora of each
// eco, the spacing rule. WORLD lane; gen::ChunkCtx carries the chunk's state between the stages.

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
constexpr int BR = ChunkCtx::BR;
constexpr int BW = ChunkCtx::BW;
constexpr int RW = ChunkCtx::RW;

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
}  // namespace

// 7: the vegetation by per-tile hash; the low flora off paving and snow; the gates kept clear
void EndlessSource::Impl::chunkFlora(ChunkCtx& X) {
  ChunkData& c = X.c;
  const int32_t x0 = X.x0;
  const int32_t y0 = X.y0;
  const BaseRect& B = X.B;
  std::vector<uint8_t>& reserved = X.reserved;
  std::vector<uint8_t>& road = X.road;
  std::vector<uint8_t>& bits = X.bits;
  auto ri = [&X](int32_t gx, int32_t gy) { return X.ri(gx, gy); };
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
}

}  // namespace ew
