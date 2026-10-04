// Prop traits for the renderer (see prop_traits.h).
#include "rpg/view/prop_traits.h"

using art::Prop;

bool flatProp(Prop p) {
  return p == Prop::Rug || p == Prop::LilyPad || p == Prop::Flowers1 || p == Prop::Flowers2 || p == Prop::Flowers3 || p == Prop::Bones ||
         p == Prop::SkullPile || p == Prop::Mushrooms || p == Prop::Ladder || p == Prop::Filler || p == Prop::StairsDown;
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
    default: return false;
  }
}

int propShadow(Prop p) {
  if (treeProp(p)) return 1;
  if (p == Prop::Boulder || p == Prop::Tent || p == Prop::Well || p == Prop::Fountain || p == Prop::Statue || p == Prop::Cart) return 2;
  return 0;
}
