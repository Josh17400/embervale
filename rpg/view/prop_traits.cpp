// Prop traits for the renderer (see prop_traits.h).
#include "rpg/view/prop_traits.h"

using art::Prop;

bool flatProp(Prop p) {
  return p == Prop::Rug || p == Prop::LilyPad || p == Prop::Flowers1 || p == Prop::Flowers2 || p == Prop::Flowers3 || p == Prop::Bones ||
         p == Prop::SkullPile || p == Prop::Mushrooms || p == Prop::Ladder || p == Prop::Filler || p == Prop::StairsDown ||
         p == Prop::MineRail || p == Prop::Bedroll;
}
bool natureProp(Prop p) { return (int)p <= (int)Prop::Fern; }
bool treeProp(Prop p) {
  return p == Prop::OakTree || p == Prop::OakTree2 || p == Prop::PineTree || p == Prop::PineTree2 || p == Prop::SnowPine || p == Prop::BirchTree ||
         p == Prop::DeadTree || p == Prop::WillowTree || p == Prop::PalmTree || p == Prop::AutumnTree;
}
bool propLight(Prop p, float& r, Color& c) {
  switch (p) {
    case Prop::Torch: r = 56; c = Color(1.0f, 0.62f, 0.3f); return true;
    case Prop::Campfire: r = 90; c = Color(1.0f, 0.55f, 0.25f); return true;
    case Prop::Brazier: r = 70; c = Color(1.0f, 0.6f, 0.3f); return true;
    case Prop::Lamppost: r = 64; c = Color(1.0f, 0.8f, 0.5f); return true;
    case Prop::Fireplace: r = 80; c = Color(1.0f, 0.6f, 0.3f); return true;
    case Prop::Crystal: r = 44; c = Color(0.4f, 0.8f, 1.0f); return true;
    case Prop::Shrine: r = 40; c = Color(0.8f, 0.8f, 1.0f); return true;
    case Prop::Altar: r = 40; c = Color(0.9f, 0.6f, 1.0f); return true;
    case Prop::Cauldron: r = 36; c = Color(0.5f, 1.0f, 0.6f); return true;
    // M0 interiors: warm hearths, a smith's forge, wall candles and the scholar's desk candle
    case Prop::Hearth: r = 104; c = Color(1.0f, 0.58f, 0.28f); return true;
    case Prop::Forge: r = 92; c = Color(1.0f, 0.46f, 0.18f); return true;
    case Prop::Sconce: r = 46; c = Color(1.0f, 0.74f, 0.42f); return true;
    case Prop::TableWork: r = 30; c = Color(1.0f, 0.78f, 0.46f); return true;
    case Prop::HolySymbol: r = 34; c = Color(1.0f, 0.88f, 0.6f); return true;
    // M0b
    case Prop::Oven: r = 64; c = Color(1.0f, 0.55f, 0.25f); return true;
    case Prop::Candelabra: r = 52; c = Color(1.0f, 0.78f, 0.46f); return true;
    case Prop::Nightstand: r = 28; c = Color(1.0f, 0.78f, 0.46f); return true;
    // M2: a fallen star's shards glow cold
    case Prop::StarShard: r = 48; c = Color(0.55f, 0.75f, 1.0f); return true;
    default: return false;
  }
}

int propShadow(Prop p) {
  if (treeProp(p)) return 1;
  if (p == Prop::Boulder || p == Prop::Tent || p == Prop::Well || p == Prop::Fountain || p == Prop::Statue || p == Prop::Cart || p == Prop::RuinColumn) return 2;
  // M1 economy: a stall's awning throws a wide shade over its counter and the ground before it; the yard machinery
  if (art::isStall(p)) return 3;
  if (p == Prop::OreCart || p == Prop::LogPile || p == Prop::Sacks || p == Prop::MineEntrance || p == Prop::MarketCross) return 2;
  if (p == Prop::PenShelter) return 3;
  if (p == Prop::MarketTable || p == Prop::GroundCloth) return 4;   // (M1 fixer round 2) two tiles wide, on the west one
  if (p == Prop::Sheep || p == Prop::Cow) return 5;
  // M2 wayside places and wonders: a ground shadow as wide as the footprint (6), the broad crowns and spreads (7)
  if (p == Prop::ElderTree || p == Prop::DragonBones || p == Prop::GreatPeak) return 7;
  if (p == Prop::Peak || p == Prop::CaravanWreck || p == Prop::WatchtowerRuin || p == Prop::FishingShack || p == Prop::Colossus) return 6;
  if (p == Prop::StandingStone || p == Prop::GraveCairn || p == Prop::TollPost) return 2;
  return 0;
}
