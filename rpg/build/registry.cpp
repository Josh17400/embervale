// The builder's registry (VISION_PLAN 15.14): see rpg/build/registry.h. FROZEN after M3b phase A: a new art::Prop
// is classified here (the switch has no default, so the compiler and rpg_test --builder catch an unclassified one).
#include "rpg/build/registry.h"

namespace bld {

const char* kindName(Kind k) {
  static const char* n[] = {"none", "furniture", "building", "city wall", "gatehouse", "wall tower", "palisade", "bridge", "road",
                            "paving", "fence", "monument", "well", "shrine", "lamp", "bench", "market stall", "sign", "banner",
                            "tent", "work yard"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Kind::COUNT, "a name for every kind");
  return (int)k < (int)Kind::COUNT ? n[(int)k] : "?";
}

Kind kindOfProp(art::Prop p) {
  using art::Prop;
  switch (p) {
    // nature and terrain
    case Prop::OakTree: case Prop::OakTree2: case Prop::PineTree: case Prop::PineTree2: case Prop::SnowPine: case Prop::BirchTree:
    case Prop::DeadTree: case Prop::WillowTree: case Prop::PalmTree: case Prop::AutumnTree: case Prop::Bush: case Prop::BerryBush:
    case Prop::SnowBush: case Prop::Boulder: case Prop::Rock: case Prop::MossRock: case Prop::SnowRock: case Prop::Stump: case Prop::Log:
    case Prop::Flowers1: case Prop::Flowers2: case Prop::Flowers3: case Prop::TallGrass: case Prop::Reeds: case Prop::Cactus:
    case Prop::Mushrooms: case Prop::LilyPad: case Prop::Fern: case Prop::Peak: case Prop::GreatPeak: case Prop::ElderTree:
    case Prop::StarShard: case Prop::DragonBones:
      return Kind::None;
    // (M3c) the Wildlands flora: plants and rocks, never built
    case Prop::AcaciaTree: case Prop::BaobabTree: case Prop::GiantTree: case Prop::GnarledTree: case Prop::BlossomTree:
    case Prop::BambooClump: case Prop::JungleTree: case Prop::GiantMushroom: case Prop::SilverTree: case Prop::MangroveTree:
    case Prop::SwampCypress: case Prop::PetrifiedTree: case Prop::LarchTree: case Prop::JuniperTree: case Prop::Hoodoo:
    case Prop::CrystalSpire: case Prop::BasaltColumns: case Prop::IceSerac: case Prop::TermiteMound: case Prop::AshVent:
    case Prop::Gorse: case Prop::Thornbush: case Prop::Heather: case Prop::Wildflowers: case Prop::PrairieGrass:
    case Prop::CottonGrass: case Prop::Agave: case Prop::DryBrush: case Prop::Saltbush: case Prop::Lichen: case Prop::Wrack:
    case Prop::Shells: case Prop::GlowCaps: case Prop::Blightweed: case Prop::SilverFern: case Prop::JungleFern: case Prop::Petals:
      return Kind::None;
    // loose goods, creatures, camp and dungeon dressing, ruins of the ancients, the invisible filler
    case Prop::Chest: case Prop::ChestOpen: case Prop::Barrel: case Prop::Crate: case Prop::Torch: case Prop::Campfire:
    case Prop::Haystack: case Prop::Anvil: case Prop::Cart: case Prop::Woodpile: case Prop::CaveEntrance: case Prop::Ladder:
    case Prop::Stalagmite: case Prop::Crystal: case Prop::Bones: case Prop::SkullPile: case Prop::Cobweb: case Prop::Coffin:
    case Prop::Urn: case Prop::IronDoor: case Prop::Filler: case Prop::RuinWall: case Prop::RuinColumn: case Prop::Sacks:
    case Prop::Baskets: case Prop::OreCart: case Prop::OrePile: case Prop::LogPile: case Prop::Sheep: case Prop::Cow:
    case Prop::MineRail: case Prop::CaravanWreck: case Prop::WatchtowerRuin: case Prop::Bedroll: case Prop::Colossus:
      return Kind::None;
    // interior furniture, wall decor, stairs and interior doors (the culture's furniture kit)
    case Prop::Bed: case Prop::Table: case Prop::Chair: case Prop::Shelf: case Prop::Fireplace: case Prop::Rug: case Prop::Counter:
    case Prop::Barrel2: case Prop::PlantPot: case Prop::Cauldron: case Prop::Bookshelf: case Prop::Throne: case Prop::Altar:
    case Prop::Tapestry: case Prop::WallShelf: case Prop::HerbBundle: case Prop::Antlers: case Prop::Painting: case Prop::Sconce:
    case Prop::Window: case Prop::WallShield: case Prop::ToolRack: case Prop::PanRack: case Prop::Wreath: case Prop::HolySymbol:
    case Prop::Cupboard: case Prop::Wardrobe: case Prop::Stool: case Prop::Cradle: case Prop::SpinningWheel:
    case Prop::Loom: case Prop::Desk: case Prop::Workbench: case Prop::Hearth: case Prop::Forge: case Prop::TableSmall:
    case Prop::TableMeal: case Prop::TableWork: case Prop::TableL: case Prop::TableM: case Prop::TableR: case Prop::CounterL:
    case Prop::CounterM: case Prop::CounterR: case Prop::StairsUp: case Prop::StairsDown: case Prop::DoorH: case Prop::DoorV:
    case Prop::Nightstand: case Prop::Washstand: case Prop::Oven: case Prop::PrepTable: case Prop::BottleShelf:
    case Prop::WeaponRack: case Prop::Lectern: case Prop::Candelabra: case Prop::DisplayTable: case Prop::QuenchTub:
    case Prop::Grindstone: case Prop::Pillar: case Prop::BunkBed: case Prop::Dresser: case Prop::Cushion: case Prop::LowTable:
    case Prop::Hammock: case Prop::SleepingMat:
    case Prop::FirePitL: case Prop::FirePitM: case Prop::FirePitR: case Prop::Stove: case Prop::TrainingDummy:   // (M3b interiors)
    case Prop::FoldScreen:
      return Kind::Furniture;
    // built in the open by a culture: drawn from the builder's parts
    case Prop::Signpost: case Prop::TollPost: return Kind::Sign;
    case Prop::Well: return Kind::Well;
    case Prop::FenceH: case Prop::FenceV: return Kind::Fence;
    case Prop::Lamppost: case Prop::Brazier: return Kind::Lamp;
    case Prop::Tent: return Kind::Tent;
    case Prop::MarketStall: case Prop::StallProduce: case Prop::StallFish: case Prop::StallCloth: case Prop::StallPottery:
    case Prop::StallMeat: case Prop::StallBread: case Prop::StallTools: case Prop::StallTimber: case Prop::MarketTable:
    case Prop::GroundCloth:
      return Kind::MarketStall;
    case Prop::Fountain: case Prop::Statue: case Prop::Gravestone: case Prop::MarketCross: case Prop::StandingStone:
    case Prop::GraveCairn:
      return Kind::Monument;
    case Prop::Bench: return Kind::Bench;   // the street bench (indoors the furniture kit draws its own)
    case Prop::Banner: return Kind::Banner;
    case Prop::Shrine: return Kind::Shrine;
    case Prop::DryingRack: case Prop::HideRack: case Prop::MineEntrance: case Prop::Trough: case Prop::WaterWheel:
    case Prop::PenShelter: case Prop::MineHill: case Prop::FishingShack: case Prop::HerbBed:
      return Kind::WorkYard;
    case Prop::COUNT: break;
  }
  return Kind::COUNT;   // unclassified: rpg_test --builder fails
}

}  // namespace bld
