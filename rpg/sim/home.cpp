// M7 "Home": the data layer (rpg/sim/home.h). HOMESTEAD lane.
//
// Phase A (lead, 2026-10-09) implements for real: the calendar, the property / lot / shell / object / crop / animal /
// breed tables, the item conventions, the ingredient map and the base meals, ground-edit RLE, offline catch-up (crops,
// rain, the farmhand, troughs, products, night raids on exposed plots), placement with the gate-to-door BFS, the
// vacant houses for sale, the built house's Bldg, and the home block. The HOMESTEAD lane tunes the numbers (M6 bands,
// the rags-to-riches curve), the culture recipes and the rest of the rules.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <initializer_list>
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/home.h"
#include "rpg/world/coords.h"
#include "rpg/world/source.h"

namespace home {

namespace {
// (home block v1: phase A, refused since)
constexpr uint8_t HOME_BLOCK_V2 = 2;   // phase B: the plot's furnished / starter / realm / farmhand / raid / tax-notice
                                       // fields and the horse's whereabouts (only the current version loads)
constexpr uint8_t HOME_BLOCK = HOME_BLOCK_V2;
constexpr uint8_t SP = seasonBit(Season::Spring), SU = seasonBit(Season::Summer), AU = seasonBit(Season::Autumn), WI = seasonBit(Season::Winter);
inline uint64_t h64(uint64_t a, uint64_t b) { return ew::mix64(a ^ ew::mix64(b + 0x9E3779B97F4A7C15ull)); }
}  // namespace

const char* seasonName(Season s) {
  static const char* n[] = {"SPRING", "SUMMER", "AUTUMN", "WINTER"};
  return (int)s < 4 ? n[(int)s] : "";
}

// ---------------------------------------------------------------- property tables
const HouseInfo& houseInfo(HouseKind k) {
  static const HouseInfo t[] = {
      {"HUT", 3, 2, 900, 5, 4},
      {"COTTAGE", 4, 3, 2500, 15, 4 | 2},
      {"TOWNHOUSE", 5, 3, 6000, 30, 2 | 1},
      {"MANOR", 7, 4, 18000, 80, 1},
      {"ESTATE", 9, 4, 50000, 200, 1},
  };
  return t[std::min((int)k, (int)HouseKind::COUNT - 1)];
}
const LotInfo& lotInfo(LotSize s) {
  static const LotInfo t[] = {{"SMALL PLOT", 10, 8, 1500, 6}, {"MEDIUM PLOT", 13, 10, 3500, 12}, {"LARGE PLOT", 16, 12, 6000, 20}};
  return t[std::min((int)s, (int)LotSize::COUNT - 1)];
}
const ShellInfo& shellInfo(Shell s) {
  static const ShellInfo t[] = {
      {"HUT", 4, 3, 1, bld::Form::Auto, art::Building::Hut, 10, 2, 900, 15, 30, 2, LotSize::Small},
      {"COTTAGE", 5, 4, 1, bld::Form::Auto, art::Building::House, 20, 4, 2200, 15, 30, 3, LotSize::Small},
      {"LONGHOUSE", 8, 4, 1, bld::Form::Long, art::Building::House, 30, 6, 3600, 15, 30, 4, LotSize::Medium},
      {"TOWNHOUSE", 5, 4, 2, bld::Form::Auto, art::Building::StoneHouse, 25, 8, 4200, 15, 30, 4, LotSize::Medium},
      {"HALL", 7, 5, 2, bld::Form::Auto, art::Building::StoneHouse, 40, 12, 7500, 15, 30, 5, LotSize::Large},
  };
  return t[std::min((int)s, (int)Shell::COUNT - 1)];
}

float priceFactor(const Game& g, int site) {
  if (site < 0 || site >= (int)g.world.sites.size()) return 1.0f;
  const Site& s = g.world.sites[(size_t)site];
  float f = s.type == SiteType::City ? 1.2f : s.type == SiteType::Town ? 1.0f : 0.85f;
  if (const realm::SettlementState* st = g.realm.settlement(s.id)) f = 0.7f + 0.7f * (float)st->prosperity / 100.0f;
  return std::clamp(f, 0.7f, 1.4f);
}

// ---------------------------------------------------------------- objects
const ObjInfo& objInfo(Obj o) {
  using P = art::Prop;
  constexpr P NP = P::COUNT;
  static const ObjInfo t[] = {
      // outside
      {"FENCE", 1, 1, OBJ_OUTSIDE | OBJ_SOLID | OBJ_JOINS, 4, NP, 0},
      {"GATE", 1, 1, OBJ_OUTSIDE | OBJ_JOINS | OBJ_ROTATES, 15, NP, 0},
      {"PATH", 1, 1, OBJ_OUTSIDE | OBJ_GROUND | OBJ_JOINS, 1, NP, 0},
      {"FARMLAND", 1, 1, OBJ_OUTSIDE | OBJ_GROUND | OBJ_JOINS, 2, NP, 0},
      {"WELL", 2, 2, OBJ_OUTSIDE | OBJ_SOLID, 250, NP, 0},
      {"WOODPILE", 2, 1, OBJ_OUTSIDE | OBJ_SOLID | OBJ_ROTATES, 40, NP, 0},
      {"BEEHIVE", 1, 1, OBJ_OUTSIDE | OBJ_SOLID, 120, NP, 0},
      {"SCARECROW", 1, 1, OBJ_OUTSIDE | OBJ_SOLID, 30, NP, 0},
      {"CHICKEN COOP", 2, 2, OBJ_OUTSIDE | OBJ_SOLID | OBJ_ANIMALS, 300, NP, 6},
      {"ANIMAL PEN", 3, 2, OBJ_OUTSIDE | OBJ_SOLID | OBJ_ANIMALS, 400, NP, 4},
      {"STABLE", 3, 3, OBJ_OUTSIDE | OBJ_SOLID | OBJ_ANIMALS, 900, NP, 2},
      {"TROUGH", 2, 1, OBJ_OUTSIDE | OBJ_SOLID | OBJ_ROTATES, 60, NP, 0},
      {"WORKBENCH", 2, 1, OBJ_OUTSIDE | OBJ_SOLID | OBJ_ROTATES, 150, NP, 0},
      {"FORGE", 2, 2, OBJ_OUTSIDE | OBJ_SOLID, 1200, NP, 0},
      {"FLOWER BED", 1, 1, OBJ_OUTSIDE, 20, NP, 0},
      {"SAPLING", 1, 1, OBJ_OUTSIDE | OBJ_SOLID, 15, NP, 0},
      {"BENCH", 2, 1, OBJ_OUTSIDE | OBJ_SOLID | OBJ_ROTATES, 60, NP, 0},
      {"LANTERN", 1, 1, OBJ_OUTSIDE | OBJ_SOLID, 45, NP, 0},
      {"STATUE", 1, 1, OBJ_OUTSIDE | OBJ_SOLID, 600, NP, 0},
      {"BANNER", 1, 1, OBJ_OUTSIDE | OBJ_SOLID, 120, NP, 0},
      {"CAMPFIRE", 1, 1, OBJ_OUTSIDE | OBJ_SOLID | OBJ_COOK, 10, NP, 0},
      {"DOGHOUSE", 1, 1, OBJ_OUTSIDE | OBJ_SOLID, 80, NP, 0},
      {"HAY RACK", 2, 1, OBJ_OUTSIDE | OBJ_SOLID | OBJ_ROTATES, 90, NP, 0},
      {"SHIPPING CRATE", 1, 1, OBJ_OUTSIDE | OBJ_SOLID | OBJ_STORAGE, 50, NP, STORE_STACKS},
      // inside
      {"BED", 1, 2, OBJ_INSIDE | OBJ_SOLID | OBJ_BED | OBJ_ROTATES, 120, P::Bed, 0},
      {"TABLE", 1, 1, OBJ_INSIDE | OBJ_SOLID, 60, P::Table, 0},
      {"CHAIR", 1, 1, OBJ_INSIDE, 25, P::Chair, 0},
      {"SHELF", 1, 1, OBJ_INSIDE | OBJ_SOLID | OBJ_STORAGE, 70, P::Shelf, 12},
      {"CHEST", 1, 1, OBJ_INSIDE | OBJ_SOLID | OBJ_STORAGE, 80, P::Chest, STORE_STACKS},
      {"RUG", 1, 1, OBJ_INSIDE, 40, P::Rug, 0},
      {"HEARTH", 3, 1, OBJ_INSIDE | OBJ_SOLID | OBJ_COOK, 400, P::Hearth, 0},
      {"COOKING POT", 1, 1, OBJ_INSIDE | OBJ_SOLID | OBJ_COOK, 90, P::Cauldron, 0},
      {"ARMOUR STAND", 1, 1, OBJ_INSIDE | OBJ_SOLID, 150, P::TrainingDummy, 0},
      {"WEAPON RACK", 1, 1, OBJ_INSIDE | OBJ_SOLID, 140, P::WeaponRack, 0},
      {"TROPHY", 1, 1, OBJ_INSIDE | OBJ_WALLDECOR, 0, NP, 0},
      {"PAINTING", 1, 1, OBJ_INSIDE | OBJ_WALLDECOR, 200, P::Painting, 0},
      {"WARDROBE", 1, 1, OBJ_INSIDE | OBJ_SOLID | OBJ_STORAGE, 160, P::Wardrobe, 12},
      {"BOOKSHELF", 1, 1, OBJ_INSIDE | OBJ_SOLID, 140, P::Bookshelf, 0},
      {"PLANT POT", 1, 1, OBJ_INSIDE | OBJ_SOLID, 25, P::PlantPot, 0},
      {"CANDELABRA", 1, 1, OBJ_INSIDE | OBJ_SOLID, 70, P::Candelabra, 0},
      {"STOOL", 1, 1, OBJ_INSIDE, 15, P::Stool, 0},
      {"CUPBOARD", 1, 1, OBJ_INSIDE | OBJ_SOLID | OBJ_STORAGE, 110, P::Cupboard, 12},
  };
  static_assert(sizeof(t) / sizeof(t[0]) == (size_t)Obj::COUNT, "an ObjInfo for every Obj");
  return t[std::min((int)o, (int)Obj::COUNT - 1)];
}
art::FarmObj farmObjArt(Obj o) {
  using F = art::FarmObj;
  switch (o) {
    case Obj::Fence: return F::Fence;
    case Obj::Gate: return F::Gate;
    case Obj::Well: return F::Well;
    case Obj::Woodpile: return F::Woodpile;
    case Obj::Beehive: return F::Beehive;
    case Obj::Scarecrow: return F::Scarecrow;
    case Obj::Coop: return F::Coop;
    case Obj::Pen: return F::Pen;
    case Obj::Stable: return F::Stable;
    case Obj::Trough: return F::Trough;
    case Obj::Workbench: return F::Workbench;
    case Obj::Forge: return F::Forge;
    case Obj::FlowerBed: return F::FlowerBed;
    case Obj::Sapling: return F::Sapling;
    case Obj::Bench: return F::Bench;
    case Obj::Lantern: return F::Lantern;
    case Obj::Statue: return F::Statue;
    case Obj::Banner: return F::Banner;
    case Obj::Campfire: return F::Campfire;
    case Obj::Doghouse: return F::Doghouse;
    case Obj::HayRack: return F::HayRack;
    case Obj::ShippingCrate: return F::ShippingCrate;
    default: return F::COUNT;
  }
}

// ---------------------------------------------------------------- crops
const CropInfo& cropInfo(Crop c) {
  using G = ew::Good;
  static const CropInfo t[] = {
      {"WHEAT", 6, 1, 3, SP | SU | AU, false, false, false, G::Grain, 2, 4},
      {"BARLEY", 5, 1, 3, SP | SU | AU, false, false, false, G::Grain, 2, 4},
      {"OATS", 4, 1, 3, SP | SU | AU, false, false, false, G::Grain, 2, 3},
      {"RYE", 6, 1, 3, AU | WI | SP, false, false, false, G::Grain, 2, 4},
      {"POTATOES", 5, 2, 3, SP | SU | AU, false, false, false, G::Produce, 3, 5},
      {"TURNIPS", 3, 1, 2, SP | SU | AU | WI, false, false, false, G::Produce, 1, 3},
      {"CABBAGE", 5, 1, 2, SP | AU | WI, false, false, false, G::Produce, 3, 6},
      {"CARROTS", 4, 1, 3, SP | SU | AU, false, false, false, G::Produce, 2, 4},
      {"ONIONS", 5, 1, 3, SP | SU | AU | WI, false, false, false, G::Produce, 2, 4},
      {"FLAX", 6, 1, 2, SP | SU, false, false, false, G::Wool, 3, 6},
      {"BEANS", 4, 1, 3, SP | SU, false, false, false, G::Produce, 2, 4},
      {"GRAPES", 8, 2, 3, SU | AU, true, true, false, G::Produce, 8, 12},
      {"DATES", 8, 2, 3, SU | AU, true, true, false, G::Produce, 10, 14},
      {"MAIZE", 7, 1, 3, SP | SU, true, false, false, G::Grain, 3, 5},
      {"TEA", 7, 1, 2, SP | SU | AU, true, true, false, G::Produce, 8, 12},
      {"RICE", 7, 2, 3, SP | SU, true, false, true, G::Grain, 3, 5},
      {"HERBS", 3, 1, 2, SP | SU | AU, false, false, false, G::Produce, 2, 6},
  };
  static_assert(sizeof(t) / sizeof(t[0]) == (size_t)CROPS, "a CropInfo for every crop");
  return t[std::min((int)c, CROPS - 1)];
}

// ---------------------------------------------------------------- animals and horses
const AnimalInfo& animalInfo(Animal a) {
  using C = art::Critter;
  static const AnimalInfo t[] = {
      {"CHICKEN", C::Chicken, Obj::Coop, 40, 1, (uint8_t)Product::Egg, 1},
      {"GOAT", C::Goat, Obj::Pen, 250, 2, (uint8_t)Product::Milk, 1},
      {"SHEEP", C::Sheep, Obj::Pen, 300, 7, (uint8_t)Product::Wool, 1},
      {"COW", C::Cow, Obj::Pen, 600, 1, (uint8_t)Product::Milk, 1},
      {"PIG", C::Pig, Obj::Pen, 350, 5, (uint8_t)Product::Truffle, 1},
      {"HORSE", C::Horse, Obj::Stable, 900, 0, (uint8_t)Product::None, 1},
      {"DOG", C::Dog, Obj::COUNT, 150, 0, (uint8_t)Product::None, 0},
      {"DUCK", C::Duck, Obj::Coop, 50, 2, (uint8_t)Product::Egg, 1},
  };
  static_assert(sizeof(t) / sizeof(t[0]) == (size_t)Animal::COUNT, "an AnimalInfo for every animal");
  return t[std::min((int)a, (int)Animal::COUNT - 1)];
}
std::string animalName(const AnimalRec& a) {
  static const char* n[] = {"BRAMBLE", "CLOVER", "HAZEL", "PEPPER", "MOSS", "DAISY", "BARLEY", "SORREL", "ROWAN", "TANSY",
                            "BUTTONS", "FERN", "THISTLE", "MAPLE", "PIP", "SAGE", "WILLOW", "JUNIPER", "MARIGOLD", "ACORN",
                            "EMBER", "SOOT", "DUSTY", "HONEY"};
  return n[ew::mix64(a.seed) % (sizeof n / sizeof n[0])];
}
const BreedInfo& breedInfo(Breed b) {
  static const BreedInfo t[] = {
      {"HEARTLAND HORSE", 8.6f, false, 900, rgba(140, 90, 48)},     // bay
      {"STEPPE HORSE", 9.8f, false, 1800, rgba(180, 134, 78)},      // dun
      {"HIGHLAND PONY", 7.8f, true, 1100, rgba(90, 60, 36)},        // dark chestnut
      {"DUNE HORSE", 9.6f, false, 2600, rgba(180, 180, 176)},       // grey
      {"FJORD HORSE", 8.0f, true, 1200, rgba(216, 196, 140)},       // cream dun
      {"MARSH HORSE", 7.8f, false, 700, rgba(56, 48, 40)},          // black-brown
  };
  return t[std::min((int)b, (int)Breed::COUNT - 1)];
}
Breed breedOfCulture(uint64_t culture, const Game& g) {
  if (!culture || !g.world.src) return Breed::Heartland;
  const cult::Culture& C = g.world.src->culture(culture);
  switch (C.archetype) {
    case cult::Archetype::Steppe: return Breed::Steppe;
    case cult::Archetype::Highland: return Breed::Highland;
    case cult::Archetype::Dune: return Breed::Dune;
    case cult::Archetype::Fjordfolk: return Breed::Fjord;
    case cult::Archetype::Marsh: return Breed::Marsh;
    default: return Breed::Heartland;
  }
}
const char* dismountName(Dismount d) {
  static const char* n[] = {"PLAYER", "ATTACK", "HURT", "ENTER", "WATER", "TALK", "SLEEP", "TRAVEL"};
  return (int)d < (int)Dismount::COUNT ? n[(int)d] : "";
}

// ---------------------------------------------------------------- items
namespace {
bool cropEdible(Crop c) {
  switch (c) {
    case Crop::Turnip: case Crop::Cabbage: case Crop::Carrot: case Crop::Onion: case Crop::Beans: case Crop::Grapes: case Crop::Dates: return true;
    default: return false;
  }
}
art::Icon cropIcon(Crop c) {
  switch (c) {
    case Crop::Wheat: case Crop::Barley: case Crop::Oats: case Crop::Rye: case Crop::Maize: case Crop::Rice: case Crop::Flax: return art::Icon::Sheaf;
    case Crop::Grapes: case Crop::Dates: return art::Icon::Fruit;
    case Crop::Tea: case Crop::Herbs: return art::Icon::Herb;
    default: return art::Icon::Veg;
  }
}
uint32_t cropTint(Crop c) {
  static const uint32_t t[] = {rgba(222, 188, 92), rgba(206, 176, 98), rgba(214, 200, 140), rgba(170, 140, 80), rgba(176, 140, 96),
                               rgba(220, 200, 210), rgba(130, 180, 90), rgba(232, 128, 50), rgba(220, 190, 140), rgba(120, 150, 210),
                               rgba(110, 160, 70), rgba(110, 60, 130), rgba(150, 90, 50), rgba(236, 200, 70), rgba(80, 130, 70),
                               rgba(230, 226, 200), rgba(110, 170, 100)};
  return t[std::min((int)c, CROPS - 1)];
}
}  // namespace

Item makeCropItem(Crop c, int count, int quality) {
  const CropInfo& ci = cropInfo(c);
  Item it;
  const bool food = cropEdible(c);
  it.kind = food ? ItemKind::Food : ItemKind::Misc;
  it.sub = (uint8_t)((food ? FOOD_CROP : MISC_CROP) + (int)c);
  it.tier = (uint8_t)std::clamp(quality, 0, 3);
  it.count = std::max(1, count);
  it.name = ci.name;
  if (it.tier >= 2) it.name = std::string(it.tier == 3 ? "PRIZE " : "FINE ") + ci.name;
  it.icon = cropIcon(c);
  it.tint = cropTint(c);
  it.value = ci.value + ci.value * it.tier / 2;
  it.power = (int16_t)(food ? 6 + 2 * it.tier : 0);
  return it;
}
std::string seedName(Crop c) {
  switch (c) {
    case Crop::Potato: return "SEED POTATOES";
    case Crop::Grapes: return "GRAPEVINE CUTTINGS";
    case Crop::Tea: return "TEA CUTTINGS";
    case Crop::Dates: return "DATE PALM SEEDS";
    default: break;
  }
  std::string n = cropInfo(c).name;   // "TURNIPS" -> "TURNIP SEEDS", "OATS" -> "OAT SEEDS"
  if (n.size() > 3 && n.back() == 'S') n.pop_back();
  return n + " SEEDS";
}
Item makeSeeds(Crop c, int count) {
  Item it;
  it.kind = ItemKind::Misc;
  it.sub = (uint8_t)(MISC_SEED + (int)c);
  it.count = std::max(1, count);
  it.name = seedName(c);
  it.icon = art::Icon::Seeds;
  it.tint = cropTint(c);
  it.value = cropInfo(c).seedPrice;
  return it;
}
Item makeProduct(Product p, int count) {
  Item it;
  struct PI { const char* name; bool food; art::Icon icon; int value; int heal; uint32_t tint; };
  static const PI t[] = {{"", false, art::Icon::Bone, 1, 0, 0},
                         {"EGGS", true, art::Icon::Egg, 3, 6, rgba(240, 230, 210)},
                         {"MILK", true, art::Icon::Milk, 4, 8, rgba(240, 240, 236)},
                         {"WOOL", false, art::Icon::Wool, 8, 0, rgba(232, 226, 210)},
                         {"HAY", false, art::Icon::Hay, 1, 0, rgba(214, 190, 100)},
                         {"FLOUR", false, art::Icon::Flour, 4, 0, rgba(236, 230, 214)},
                         {"HONEY", true, art::Icon::Honey, 9, 10, rgba(232, 170, 50)},
                         {"CHEESE", true, art::Icon::Cheese, 10, 16, rgba(236, 200, 90)},
                         {"TRUFFLES", false, art::Icon::Veg, 30, 0, rgba(80, 60, 50)}};
  const int k = std::clamp((int)p, 0, (int)Product::COUNT - 1);
  it.kind = t[k].food ? ItemKind::Food : ItemKind::Misc;
  it.sub = (uint8_t)((t[k].food ? FOOD_PRODUCT : MISC_PRODUCT) + k);
  it.count = std::max(1, count);
  it.name = t[k].name; it.icon = t[k].icon; it.value = t[k].value; it.power = (int16_t)t[k].heal; it.tint = t[k].tint;
  return it;
}
Item makeTool(Tool t) {
  static const char* n[] = {"HOE", "WATERING CAN", "SICKLE", "GROOMING BRUSH"};
  static const art::Icon ic[] = {art::Icon::Hoe, art::Icon::WateringCan, art::Icon::Sickle, art::Icon::Brush};
  static const int v[] = {25, 30, 25, 15};
  const int k = std::clamp((int)t, 0, (int)Tool::COUNT - 1);
  Item it;
  it.kind = ItemKind::Misc;
  it.sub = (uint8_t)(MISC_TOOL + k);
  it.name = n[k]; it.icon = ic[k]; it.value = v[k];
  return it;
}
bool isSeed(const Item& it, Crop& c) {
  if (it.kind != ItemKind::Misc || it.sub < MISC_SEED || it.sub >= MISC_SEED + CROPS) return false;
  c = (Crop)(it.sub - MISC_SEED);
  return true;
}
bool isTool(const Item& it, Tool t) { return it.kind == ItemKind::Misc && it.sub == MISC_TOOL + (int)t; }
bool isHomeGood(const Item& it) {
  if (it.kind == ItemKind::Food) return it.sub >= FOOD_CROP && it.sub < FOOD_MEAL + 64;
  if (it.kind == ItemKind::Misc) return it.sub >= MISC_CROP && it.sub <= MISC_DEED;
  return false;
}
ew::Good goodOf(const Item& it) {
  if (it.kind == ItemKind::Food && it.sub >= FOOD_CROP && it.sub < FOOD_CROP + CROPS) return cropInfo((Crop)(it.sub - FOOD_CROP)).good;
  if (it.kind == ItemKind::Misc && it.sub >= MISC_CROP && it.sub < MISC_CROP + CROPS) return cropInfo((Crop)(it.sub - MISC_CROP)).good;
  if (it.kind == ItemKind::Misc && it.sub == MISC_PRODUCT + (int)Product::Wool) return ew::Good::Wool;
  if (it.kind == ItemKind::Misc && it.sub == MISC_PRODUCT + (int)Product::Flour) return ew::Good::Flour;
  if (it.kind == ItemKind::Food && it.sub >= FOOD_PRODUCT && it.sub < FOOD_MEAL) return ew::Good::Produce;
  return ew::Good::COUNT;
}

// ---------------------------------------------------------------- cooking
Ingr ingredientOf(const Item& it) {
  if (it.kind == ItemKind::Food) {
    if (it.sub < FOOD_CROP) {   // makeFood: bread, venison, apple, cheese, trout, smoked salmon, eel pie, smoked venison, honey cake
      static const Ingr t[] = {Ingr::Bread, Ingr::Meat, Ingr::Fruit, Ingr::Cheese, Ingr::Fish, Ingr::Fish, Ingr::None, Ingr::Meat, Ingr::None};
      return it.sub < 9 ? t[it.sub] : Ingr::None;
    }
    if (it.sub < FOOD_CROP + CROPS) {
      const Crop c = (Crop)(it.sub - FOOD_CROP);
      return (c == Crop::Grapes || c == Crop::Dates) ? Ingr::Fruit : Ingr::Veg;
    }
    if (it.sub >= FOOD_PRODUCT && it.sub < FOOD_MEAL) {
      switch ((Product)(it.sub - FOOD_PRODUCT)) {
        case Product::Egg: return Ingr::Egg;
        case Product::Milk: return Ingr::Milk;
        case Product::Honey: return Ingr::Honey;
        case Product::Cheese: return Ingr::Cheese;
        default: return Ingr::None;
      }
    }
    return Ingr::None;   // a meal is not an ingredient
  }
  if (it.kind == ItemKind::Misc) {
    if (it.sub >= MISC_CROP && it.sub < MISC_CROP + CROPS) {
      const Crop c = (Crop)(it.sub - MISC_CROP);
      if (c == Crop::Flax) return Ingr::None;
      if (c == Crop::Tea || c == Crop::Herbs) return Ingr::Herb;
      if (c == Crop::Potato) return Ingr::Veg;
      return Ingr::Grain;
    }
    if (it.sub == MISC_PRODUCT + (int)Product::Flour) return Ingr::Flour;
    if (it.sub == 7 || it.sub == 11 || it.sub == 12) return Ingr::Herb;   // makeMisc: mountain herb, blue mountain flower, lavender
  }
  return Ingr::None;
}
const std::vector<Meal>& baseMeals() {
  static const std::vector<Meal> m = [] {
    std::vector<Meal> v;
    auto add = [&](const char* id, const char* name, Ingr a, Ingr b, Ingr c, int q, int heal) {
      Meal x;
      x.id = id; x.name = name; x.in[0] = a; x.in[1] = b; x.in[2] = c; x.quality = (uint8_t)q; x.heal = heal;
      v.push_back(x);
    };
    add("bread", "FRESH BREAD", Ingr::Flour, Ingr::None, Ingr::None, 1, 18);
    add("porridge", "PORRIDGE", Ingr::Grain, Ingr::Milk, Ingr::None, 1, 20);
    add("broth", "VEGETABLE BROTH", Ingr::Veg, Ingr::Herb, Ingr::None, 1, 18);
    add("omelette", "OMELETTE", Ingr::Egg, Ingr::Milk, Ingr::None, 1, 22);
    add("toast", "CHEESE TOAST", Ingr::Bread, Ingr::Cheese, Ingr::None, 1, 24);
    add("stew", "HUNTER'S STEW", Ingr::Meat, Ingr::Veg, Ingr::None, 2, 34);
    add("fishsoup", "FISH SOUP", Ingr::Fish, Ingr::Veg, Ingr::None, 2, 30);
    add("roast", "HERB ROAST", Ingr::Meat, Ingr::Herb, Ingr::None, 2, 36);
    add("grilledfish", "GRILLED FISH", Ingr::Fish, Ingr::Herb, Ingr::None, 2, 28);
    add("pie", "MEAT PIE", Ingr::Flour, Ingr::Meat, Ingr::Veg, 3, 44);
    add("tart", "FRUIT TART", Ingr::Flour, Ingr::Fruit, Ingr::Honey, 3, 36);
    add("honeycake", "HONEY CAKE", Ingr::Flour, Ingr::Honey, Ingr::Egg, 3, 30);
    return v;
  }();
  return m;
}
std::vector<Meal> cultureMeals(uint64_t culture, const Game& g) {
  std::vector<Meal> v;
  if (!culture || !g.world.src) return v;
  const cult::Culture& C = g.world.src->culture(culture);
  // (phase B) each archetype's own three signature dishes, from the ingredients its land gives; the first carries the
  // culture's own name ("VETHMARKI FISH STEW"), so two peoples of one archetype do not share a cookbook
  struct Sig { const char* name; Ingr a, b, c; int heal; };
  using I = Ingr;
  static const Sig sig[(int)cult::Archetype::COUNT][3] = {
      /* Fjordfolk */ {{"FISH STEW", I::Fish, I::Veg, I::Milk, 40}, {"RYE CRISPBREAD", I::Grain, I::Cheese, I::None, 30}, {"MEAD-GLAZED HAM", I::Meat, I::Honey, I::None, 42}},
      /* Highland  */ {{"OATCAKES", I::Grain, I::Honey, I::None, 30}, {"CROFTER'S BROSE", I::Grain, I::Veg, I::Milk, 38}, {"HILL MUTTON PIE", I::Flour, I::Meat, I::Herb, 46}},
      /* Heartland */ {{"HARVEST LOAF", I::Flour, I::Egg, I::Milk, 34}, {"SHEPHERD'S POT", I::Meat, I::Veg, I::Herb, 44}, {"APPLE CRUMBLE", I::Flour, I::Fruit, I::Honey, 36}},
      /* Imperial  */ {{"WINE-BRAISED BEEF", I::Meat, I::Fruit, I::Herb, 46}, {"HERB FLATBREAD", I::Flour, I::Herb, I::Cheese, 34}, {"SENATOR'S CUSTARD", I::Egg, I::Milk, I::Honey, 34}},
      /* Dune      */ {{"DATE BREAD", I::Flour, I::Fruit, I::None, 30}, {"SPICED LAMB", I::Meat, I::Herb, I::Veg, 46}, {"MINT TEA CAKES", I::Flour, I::Herb, I::Honey, 32}},
      /* Steppe    */ {{"HORSEMAN'S STEW", I::Meat, I::Milk, I::Veg, 44}, {"CURD CAKES", I::Cheese, I::Flour, I::None, 32}, {"FIRE-ROAST", I::Meat, I::Herb, I::None, 40}},
      /* Marsh     */ {{"EEL CHOWDER", I::Fish, I::Milk, I::Veg, 40}, {"BOG-HERB BROTH", I::Herb, I::Veg, I::Egg, 32}, {"REED-SMOKED FISH", I::Fish, I::Herb, I::None, 36}},
      /* Jade      */ {{"RICE BOWL", I::Grain, I::Egg, I::Veg, 38}, {"STEAMED BUNS", I::Flour, I::Meat, I::Veg, 44}, {"TEA-SMOKED FISH", I::Fish, I::Herb, I::None, 38}},
      /* River     */ {{"RIVER FISH WITH DATES", I::Fish, I::Fruit, I::None, 36}, {"BEER BREAD", I::Flour, I::Grain, I::None, 30}, {"LENTIL POT", I::Veg, I::Herb, I::Grain, 36}},
      /* SunTemple */ {{"MAIZE CAKES", I::Grain, I::Egg, I::None, 32}, {"SUN STEW", I::Meat, I::Veg, I::Herb, 44}, {"HONEY TAMALES", I::Flour, I::Honey, I::Fruit, 36}},
      /* Sylvan    */ {{"GREENLEAF SALAD", I::Veg, I::Herb, I::Fruit, 32}, {"WAYBREAD", I::Flour, I::Honey, I::None, 34}, {"FOREST TART", I::Flour, I::Fruit, I::Egg, 38}},
      /* Starspire */ {{"STARGAZER'S BROTH", I::Veg, I::Herb, I::Milk, 34}, {"COMET CAKES", I::Flour, I::Egg, I::Honey, 34}, {"OBSERVATORY ROAST", I::Meat, I::Herb, I::Fruit, 44}},
  };
  const int ar = std::clamp((int)C.archetype, 0, (int)cult::Archetype::COUNT - 1);
  for (int i = 0; i < 3; i++) {
    const Sig& s = sig[ar][i];
    Meal m;
    m.id = std::to_string((unsigned long long)culture) + "." + std::to_string(i + 1);
    m.name = i == 0 ? C.adjective + " " + s.name : std::string(s.name);
    m.in[0] = s.a; m.in[1] = s.b; m.in[2] = s.c;
    m.culture = culture;
    m.quality = 3;
    m.heal = s.heal;
    v.push_back(m);
  }
  return v;
}
std::vector<Meal> knownMeals(const Game& g) {
  std::vector<Meal> v = baseMeals();
  for (const std::string& id : g.home.recipes) {
    const size_t dot = id.find('.');
    if (dot == std::string::npos) continue;
    const uint64_t cul = std::strtoull(id.substr(0, dot).c_str(), nullptr, 10);
    for (const Meal& m : cultureMeals(cul, g)) if (m.id == id) v.push_back(m);
  }
  return v;
}
namespace {
// (M7 fix) the dish's picture: its shape by what it is (a loaf, a pie, a cake, a roast, a fish, else a bowl), the
// bowl's colour by its main ingredient
bool hasWord(const std::string& s, const char* w) { return s.find(w) != std::string::npos; }
void mealLook(const Meal& m, art::Icon& icon, uint32_t& tint) {
  const std::string& n = m.name;
  bool fish = false, meat = false, flour = false;
  for (Ingr i : m.in) { fish |= i == Ingr::Fish; meat |= i == Ingr::Meat; flour |= i == Ingr::Flour || i == Ingr::Bread; }
  const bool bowl = hasWord(n, "SOUP") || hasWord(n, "STEW") || hasWord(n, "BROTH") || hasWord(n, "CHOWDER") || hasWord(n, "PORRIDGE") ||
                    hasWord(n, "BROSE") || hasWord(n, "POT") || hasWord(n, "BOWL");
  icon = art::Icon::Meal; tint = 0;
  if (!bowl) {
    if (hasWord(n, "PIE") || hasWord(n, "TART") || hasWord(n, "CRUMBLE")) { icon = art::Icon::Pie; tint = meat ? 0 : rgba(200, 80, 90); return; }
    if (hasWord(n, "CAKE") || hasWord(n, "CUSTARD")) { icon = art::Icon::Cake; tint = hasWord(n, "HONEY") || hasWord(n, "CUSTARD") ? rgba(236, 196, 96) : 0; return; }
    if (hasWord(n, "BREAD") || hasWord(n, "LOAF") || hasWord(n, "TOAST") || hasWord(n, "BUNS")) { icon = art::Icon::Bread; return; }
    if (fish) { icon = art::Icon::Fish; tint = rgba(196, 136, 76); return; }   // grilled / smoked: the golden-brown glaze
    if (meat) { icon = art::Icon::Roast; return; }
    if (flour) { icon = art::Icon::Bread; return; }
  }
  // a bowl: the colour of what is in it
  Ingr main = m.in[0];
  if (main == Ingr::Fish || (fish && main != Ingr::Meat)) tint = rgba(222, 206, 170);       // a pale fish broth
  else if (main == Ingr::Meat) tint = 0;                                                      // the brown stew (default)
  else if (main == Ingr::Veg || main == Ingr::Herb) tint = rgba(132, 168, 72);                // a green broth
  else if (main == Ingr::Egg) tint = rgba(240, 206, 92);                                      // an omelette's yellow
  else if (main == Ingr::Grain || main == Ingr::Milk || main == Ingr::Cheese) tint = rgba(230, 214, 176);   // porridge
  else if (main == Ingr::Fruit || main == Ingr::Honey) tint = rgba(214, 120, 70);
}
}  // namespace
Item makeMeal(const Meal& m, Station st) {
  static const int bonus[] = {0, 1, 1, 2};   // campfire, hearth, cooking pot, an inn's kitchen (15.2)
  const int q = std::clamp((int)m.quality - 1 + bonus[std::min((int)st, 3)], 0, 3);
  Item it;
  it.kind = ItemKind::Food;
  it.sub = (uint8_t)(FOOD_MEAL + (int)(ew::mix64(ew::tag(m.id.c_str())) % 64));
  it.tier = (uint8_t)q;
  it.name = m.name;
  mealLook(m, it.icon, it.tint);
  it.power = (int16_t)(m.heal + m.heal * q * 15 / 100);
  it.value = 4 + it.power / 3;
  it.culture = m.culture;
  it.seed = (uint32_t)ew::tag(m.id.c_str());
  return it;
}
void mealBuff(const Item& meal, float& fedHours, uint8_t& quality) {
  fedHours = 0; quality = 0;
  if (meal.kind != ItemKind::Food || meal.sub < FOOD_MEAL) return;
  quality = (uint8_t)std::min<int>(3, meal.tier);
  fedHours = 3.0f + 1.5f * quality;
}

// ---------------------------------------------------------------- ground edits (RLE)
namespace {
std::vector<uint8_t> unpack(const Plot& p) {
  std::vector<uint8_t> g((size_t)p.w * p.h, 0);
  size_t i = 0;
  for (size_t k = 0; k + 1 < p.ground.size() && i < g.size(); k += 2)
    for (int n = 0; n < p.ground[k] && i < g.size(); n++) g[i++] = p.ground[k + 1];
  return g;
}
void pack(Plot& p, const std::vector<uint8_t>& g) {
  p.ground.clear();
  for (size_t i = 0; i < g.size();) {
    size_t j = i;
    while (j < g.size() && g[j] == g[i] && j - i < 255) j++;
    p.ground.push_back((uint8_t)(j - i));
    p.ground.push_back(g[i]);
    i = j;
  }
}
}  // namespace
namespace {
bool storeUsed(const Plot& p, size_t k) {
  if (k == 0) return true;
  for (const std::vector<PlacedObj>* v : {&p.inside, &p.outside})
    for (const PlacedObj& o : *v)
      if ((objInfo((Obj)o.kind).flags & OBJ_STORAGE) && o.data == k) return true;
  return false;
}
}  // namespace
int allocStore(Plot& p) {
  if (p.stores.empty()) p.stores.push_back(Store());
  for (size_t k = 1; k < p.stores.size(); k++)
    if (p.stores[k].items.empty() && !storeUsed(p, k)) return (int)k;
  if ((int)p.stores.size() >= MAX_STORES) return -1;
  p.stores.push_back(Store());
  return (int)p.stores.size() - 1;
}
void trimStores(Plot& p) {
  while (p.stores.size() > 1 && p.stores.back().items.empty() && !storeUsed(p, p.stores.size() - 1)) p.stores.pop_back();
}
void updateDogFlag(Plot& p) {
  bool dog = false;
  for (const PlacedObj& o : p.outside) dog |= (Obj)o.kind == Obj::Doghouse;
  for (const AnimalRec& a : p.animals) dog |= (Animal)a.kind == Animal::Dog && !(a.flags & AF_LOST);
  if (dog) p.flags |= PF_DOG;
  else p.flags &= (uint8_t)~PF_DOG;
}
int groundAt(const Plot& p, int x, int y) {
  if (x < 0 || y < 0 || x >= p.w || y >= p.h) return 0;
  const size_t want = (size_t)y * p.w + x;
  size_t i = 0;
  for (size_t k = 0; k + 1 < p.ground.size(); k += 2) {
    if (want < i + p.ground[k]) return p.ground[k + 1];
    i += p.ground[k];
  }
  return 0;
}
void setGround(Plot& p, int x, int y, int kind) {
  if (x < 0 || y < 0 || x >= p.w || y >= p.h) return;
  std::vector<uint8_t> g = unpack(p);
  g[(size_t)y * p.w + x] = (uint8_t)kind;
  pack(p, g);
}

// ---------------------------------------------------------------- catch-up
int harvestYield(const CropInfo& ci, int quality, uint64_t roll) {
  int n = ci.yieldMin + (int)(roll % (uint64_t)(ci.yieldMax - ci.yieldMin + 1));
  if (quality >= 2) n = std::max(n, (ci.yieldMin + ci.yieldMax + 1) / 2);   // a well-kept crop never yields the least
  if (quality >= 3) n++;                                                     // a prize crop: one more
  return n;
}
bool raidRoll(const Plot& p, int day, uint64_t worldSeed) {
  if (!(p.flags & PF_EXPOSED)) return false;
  const uint64_t r = h64(worldSeed ^ ew::tag("raid"), p.id * 31ull + (uint64_t)(uint32_t)day);
  const int chance = (p.flags & PF_DOG) ? 25 : 50;   // per mille: 2.5 % / 5 %
  return (int)(r % 1000) < chance;
}
bool rainDay(uint64_t worldSeed, int32_t gx, int32_t gy, int day) {
  const uint64_t cell = ((uint64_t)(uint32_t)ew::floorDiv(gx, 64) << 32) | (uint32_t)ew::floorDiv(gy, 64);
  return h64(worldSeed ^ ew::tag("rain"), cell * 131ull + (uint64_t)(uint32_t)day) % 100 < 25;
}

void catchUp(Plot& p, int fromDay, int toDay, uint64_t worldSeed) {
  if (toDay <= fromDay) return;
  const int start = std::max(fromDay, toDay - CATCHUP_DAYS);
  const bool hand = p.farmhand != 0;
  // (phase B) the farmhand's harvest goes into the shipping crate when there is one, else the house's first store
  size_t into = 0;
  for (const PlacedObj& o : p.outside)
    if ((Obj)o.kind == Obj::ShippingCrate && o.data < p.stores.size()) { into = o.data; break; }
  for (int d = start + 1; d <= toDay; d++) {
    const bool rain = rainDay(worldSeed, p.gx, p.gy, d);
    const bool handToday = hand && d <= (int)p.farmhandUntil;
    const Season se = seasonOf(d);
    // crops
    for (size_t i = 0; i < p.crops.size();) {
      CropRec& c = p.crops[i];
      const CropInfo& ci = cropInfo((Crop)c.kind);
      if (rain || handToday) c.lastWaterDay = (uint16_t)d;
      const bool watered = d - (int)c.lastWaterDay <= 1;
      if (c.stage < 3 && (ci.seasons & seasonBit(se)) && watered && (int)c.lastGrowDay < d) {
        c.grown++;
        c.streak = (uint8_t)std::min(255, c.streak + 1);
        c.lastGrowDay = (uint16_t)d;
        c.stage = (uint8_t)(c.grown >= ci.days ? 3 : std::min(2, c.grown * 3 / std::max(1, (int)ci.days)));
        if (c.stage == 3) c.quality = (uint8_t)std::min(3, (int)c.streak * 3 / std::max(1, (int)ci.days));
      } else if (!watered) c.streak = 0;
      // the farmhand harvests what is ripe into the first store
      if (handToday && c.stage == 3 && into < p.stores.size() && (int)p.stores[into].items.size() < STORE_STACKS) {
        const uint64_t r = h64(worldSeed ^ p.id, (uint64_t)d * 977ull + i);
        const int n = harvestYield(ci, c.quality, r);
        Item it = makeCropItem((Crop)c.kind, n, c.quality);
        bool merged = false;
        for (Item& s : p.stores[into].items) if (s.same(it)) { s.count += it.count; merged = true; break; }
        if (!merged) p.stores[into].items.push_back(it);
        if (ci.perennial) { c.grown = (uint8_t)(ci.days / 2); c.stage = 1; c.streak = 0; i++; }
        else { p.crops.erase(p.crops.begin() + (std::ptrdiff_t)i); continue; }
      } else i++;
    }
    // animals: the trough, products, happiness
    if (!p.animals.empty()) {
      const bool fed = p.trough > 0;
      if (fed) p.trough--;
      for (AnimalRec& a : p.animals) {
        if (a.flags & AF_LOST) continue;
        const AnimalInfo& ai = animalInfo((Animal)a.kind);
        if (ai.hay == 0 || fed) { a.lastFedDay = (uint16_t)d; a.happiness = (uint8_t)std::min(100, a.happiness + 4); }
        else a.happiness = (uint8_t)std::max(0, a.happiness - 10);
        if (a.flags & AF_GROOMED) { a.happiness = (uint8_t)std::min(100, a.happiness + 3); a.flags &= (uint8_t)~AF_GROOMED; }   // (phase B) a day's grooming
        if (ai.produceEvery && d - (int)a.lastProduceDay >= ai.produceEvery && d - (int)a.lastFedDay <= 1) {
          a.produce = (uint8_t)std::min(5, a.produce + 1);
          a.lastProduceDay = (uint16_t)d;
        }
      }
      // a night raid on an exposed plot: at most one animal (never the dog or a horse)
      if (p.flags & PF_EXPOSED) {
        const uint64_t r = h64(worldSeed ^ ew::tag("raid"), p.id * 31ull + (uint64_t)(uint32_t)d);
        if (raidRoll(p, d, worldSeed)) {
          std::vector<size_t> prey;
          for (size_t k = 0; k < p.animals.size(); k++) {
            const Animal ak = (Animal)p.animals[k].kind;
            if (!(p.animals[k].flags & AF_LOST) && ak != Animal::Dog && ak != Animal::Horse) prey.push_back(k);
          }
          if (!prey.empty()) {
            p.animals[prey[(size_t)((r >> 20) % prey.size())]].flags |= AF_LOST;
            p.raidDay = (uint16_t)d;
            p.flags &= (uint8_t)~PF_RAIDED;   // news to tell
          }
        }
      }
    }
    // (phase B) beehives make a comb every HONEY_EVERY days outside winter
    if (se != Season::Winter)
      for (PlacedObj& o : p.outside) {
        if ((Obj)o.kind != Obj::Beehive) continue;
        const int last = (int)((o.data >> 8) & 0xFFFF);
        if (d - last >= HONEY_EVERY) o.data = (uint32_t)std::min(HONEY_MAX, honeyOf(o) + 1) | ((uint32_t)(uint16_t)d << 8);
      }
  }
  if (toDay > (int)p.lastSeenDay) p.lastSeenDay = (uint16_t)toDay;
}

// ---------------------------------------------------------------- placement
void gateOf(const Plot& p, int& x, int& y) {
  for (const PlacedObj& o : p.outside)
    if ((Obj)o.kind == Obj::Gate) { x = o.x; y = o.y; return; }
  x = p.w / 2; y = p.h - 1;
}
void doorOf(const Plot& p, int& x, int& y) {
  if (!p.hw || !p.hh) { x = -1; y = -1; return; }
  x = p.hx + p.hw / 2; y = p.hy + p.hh - 1;
}

namespace {
void footprint(Obj o, bool turned, int& w, int& h) {
  const ObjInfo& oi = objInfo(o);
  w = oi.w; h = oi.h;
  if (turned && (oi.flags & OBJ_ROTATES)) std::swap(w, h);
}
// the yard as a grid: 1 blocked (solid objects, the house but its door), 0 free
std::vector<uint8_t> yardGrid(const Plot& p) {
  std::vector<uint8_t> g((size_t)p.w * p.h, 0);
  auto mark = [&](int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h; y++)
      for (int x = x0; x < x0 + w; x++) if (x >= 0 && y >= 0 && x < p.w && y < p.h) g[(size_t)y * p.w + x] = 1;
  };
  if (p.hw && p.hh) {
    mark(p.hx, p.hy, p.hw, p.hh);
    int dx, dy;
    doorOf(p, dx, dy);
    if (dx >= 0 && dy >= 0 && dx < p.w && dy < p.h) g[(size_t)dy * p.w + dx] = 0;
  }
  for (const PlacedObj& o : p.outside) {
    if (!(objInfo((Obj)o.kind).flags & OBJ_SOLID)) continue;
    int w, h;
    footprint((Obj)o.kind, o.turned(), w, h);
    mark(o.x, o.y, w, h);
  }
  return g;
}
bool usable(Obj o) {
  const uint16_t f = objInfo(o).flags;
  return (f & (OBJ_STORAGE | OBJ_COOK | OBJ_ANIMALS)) || o == Obj::Well || o == Obj::Trough || o == Obj::Workbench || o == Obj::Forge ||
         o == Obj::Beehive || o == Obj::HayRack;
}
// every usable thing reachable from the gate (BFS over free tiles; a usable object needs one free neighbour reached)
bool yardReachable(const Plot& p, const std::vector<uint8_t>& g, std::string& why) {
  int gx, gy;
  gateOf(p, gx, gy);
  if (gx < 0 || gy < 0 || gx >= p.w || gy >= p.h || g[(size_t)gy * p.w + gx]) { why = "THAT WOULD BLOCK THE GATE"; return false; }
  std::vector<uint8_t> seen(g.size(), 0);
  std::deque<int> q;
  q.push_back(gy * p.w + gx);
  seen[(size_t)(gy * p.w + gx)] = 1;
  while (!q.empty()) {
    const int i = q.front();
    q.pop_front();
    const int x = i % p.w, y = i / p.w;
    const int nx[4] = {x + 1, x - 1, x, x}, ny[4] = {y, y, y + 1, y - 1};
    for (int k = 0; k < 4; k++) {
      if (nx[k] < 0 || ny[k] < 0 || nx[k] >= p.w || ny[k] >= p.h) continue;
      const int j = ny[k] * p.w + nx[k];
      if (seen[(size_t)j] || g[(size_t)j]) continue;
      seen[(size_t)j] = 1;
      q.push_back(j);
    }
  }
  int dx, dy;
  doorOf(p, dx, dy);
  if (dx >= 0) {
    const bool doorOk = dy + 1 < p.h ? seen[(size_t)(dy + 1) * p.w + dx] != 0 : true;   // the step in front of the door
    if (!doorOk) { why = "THE PATH TO THE DOOR WOULD BE BLOCKED"; return false; }
  }
  for (const PlacedObj& o : p.outside) {
    if (!usable((Obj)o.kind)) continue;
    int w, h;
    footprint((Obj)o.kind, o.turned(), w, h);
    bool ok = false;
    for (int y = o.y - 1; y <= o.y + h && !ok; y++)
      for (int x = o.x - 1; x <= o.x + w && !ok; x++) {
        const bool inside = x >= o.x && x < o.x + w && y >= o.y && y < o.y + h;
        if (inside || x < 0 || y < 0 || x >= p.w || y >= p.h) continue;
        if ((x == o.x - 1 || x == o.x + w) && (y == o.y - 1 || y == o.y + h)) continue;   // not the corners
        ok = seen[(size_t)y * p.w + x] != 0;
      }
    if (!ok) { why = std::string("THE ") + objInfo((Obj)o.kind).name + " COULD NOT BE REACHED"; return false; }
  }
  return true;
}
}  // namespace

bool canPlaceOutside(const Plot& p, Obj o, int x, int y, bool turned, std::string& why) {
  const ObjInfo& oi = objInfo(o);
  if (!(oi.flags & OBJ_OUTSIDE)) { why = "THAT BELONGS INDOORS"; return false; }
  int w, h;
  footprint(o, turned, w, h);
  if (x < 0 || y < 0 || x + w > p.w || y + h > p.h) { why = "IT MUST STAND INSIDE YOUR PLOT"; return false; }
  if ((int)p.outside.size() >= MAX_OBJECTS && !(oi.flags & OBJ_GROUND)) { why = "THE YARD IS FULL"; return false; }
  std::vector<uint8_t> g = yardGrid(p);
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) {
      if (g[(size_t)yy * p.w + xx]) { why = "SOMETHING IS IN THE WAY"; return false; }
      for (const PlacedObj& q : p.outside) {   // nothing on top of a walk-over object either (a gate, a flower bed)
        int qw, qh;
        footprint((Obj)q.kind, q.turned(), qw, qh);
        if (xx >= q.x && xx < q.x + qw && yy >= q.y && yy < q.y + qh) { why = "SOMETHING IS IN THE WAY"; return false; }
      }
      if (oi.flags & OBJ_SOLID)
        for (const CropRec& c : p.crops) if (c.x == xx && c.y == yy) { why = "A CROP GROWS THERE"; return false; }
    }
  if (oi.flags & OBJ_GROUND) return true;   // paths and farmland never block
  if (!(oi.flags & OBJ_SOLID) && o != Obj::Gate) return true;
  Plot t = p;
  PlacedObj po;
  po.kind = (uint8_t)o; po.x = (uint8_t)x; po.y = (uint8_t)y; po.flags = turned ? 1 : 0;
  t.outside.push_back(po);
  return yardReachable(t, yardGrid(t), why);
}

namespace {
// (M7 fix) a round room (a yurt, a round hall): interior_v4's disc (Plan::inShape, Floorplan::Round: floor where the
// tile centre is inside the ellipse inscribed in columns 1..W-2, rows 2..H-2). The view bends the wall along the true
// ellipse, so a rim tile whose corner pokes past it is partly under the wall: furniture there would stand on it.
bool inDisc(const Map& m, int64_t dx2, int64_t dy2, int num, int den) {
  const int64_t rx = m.w - 2, ry = m.h - 3;
  return dx2 * dx2 * ry * ry * den + dy2 * dy2 * rx * rx * den <= rx * rx * ry * ry * num;
}
bool roundRoom(const Map& m) {
  if (m.kind != MapKind::Interior || m.w < 6 || m.h < 7) return false;
  bool voids = false;
  for (int y = 0; y < m.h && !voids; y++)
    for (int x = 0; x < m.w && !voids; x++) voids = m.at(x, y) == Ground::Void;
  if (!voids) return false;
  // every floor tile lies in the disc and every tile of the disc's box outside it is not floor (an L or a cross has
  // floor in the corners of its box)
  for (int y = 2; y <= m.h - 2; y++)
    for (int x = 1; x <= m.w - 2; x++) {
      const bool disc = inDisc(m, 2 * x + 1 - m.w, 2 * y - m.h, 21, 20);
      const Ground g = m.at(x, y);
      const bool floor = g != Ground::Void && g != Ground::InteriorWall;
      if (floor && !disc) return false;
    }
  return m.at(1, 2) == Ground::Void || m.at(1, 2) == Ground::InteriorWall;
}
// all four corners of the tile inside the drawn ellipse (a hair of slack: the rim's own line)
bool wholeInDisc(const Map& m, int x, int y) {
  for (int cy = 0; cy < 2; cy++)
    for (int cx = 0; cx < 2; cx++)
      if (!inDisc(m, 2 * x + 2 * cx - m.w, 2 * y + 2 * cy - 1 - m.h, 41, 40)) return false;
  return true;
}
}  // namespace

bool canPlaceInside(const Plot& p, const Map& m, Obj o, int x, int y, bool turned, std::string& why) {
  const ObjInfo& oi = objInfo(o);
  if (!(oi.flags & OBJ_INSIDE)) { why = "THAT BELONGS OUTSIDE"; return false; }
  if ((int)p.inside.size() >= MAX_OBJECTS) { why = "THE HOUSE IS FULL"; return false; }
  int w, h;
  footprint(o, turned, w, h);
  const bool round = !(oi.flags & OBJ_WALLDECOR) && roundRoom(m);
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) {
      if (!m.in(xx, yy)) { why = "NOT THERE"; return false; }
      if (round && !wholeInDisc(m, xx, yy)) { why = "THE ROUND WALL IS IN THE WAY"; return false; }
      if (oi.flags & OBJ_WALLDECOR) {
        if (m.at(xx, yy) != Ground::InteriorWall || groundSolid(m.at(xx, yy + 1)) || m.propAt(xx, yy)) { why = "IT HANGS ON A BACK WALL"; return false; }
        continue;
      }
      if (m.blocked(xx, yy) || m.propAt(xx, yy) || m.isExit(xx, yy) || (xx == m.up.x && yy == m.up.y) || (xx == m.down.x && yy == m.down.y)) {
        why = "SOMETHING IS IN THE WAY";
        return false;
      }
    }
  if (oi.flags & OBJ_WALLDECOR) return true;
  if (!(oi.flags & OBJ_SOLID)) return true;
  // the BFS: from the way in (the door, or the stairs up here) to the stairs and to every bed, store and hearth
  std::vector<uint8_t> blk((size_t)m.w * m.h, 0);
  for (int yy = 0; yy < m.h; yy++)
    for (int xx = 0; xx < m.w; xx++) blk[(size_t)yy * m.w + xx] = m.blocked(xx, yy) ? 1 : 0;
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) blk[(size_t)yy * m.w + xx] = 1;
  int sx = m.exitY >= 0 ? m.exitX : m.down.x, sy = m.exitY >= 0 ? m.exitY - 1 : m.down.y;
  if (!m.in(sx, sy) || blk[(size_t)sy * m.w + sx]) { why = "THAT WOULD BLOCK THE WAY IN"; return false; }
  std::vector<uint8_t> seen(blk.size(), 0);
  std::deque<int> q;
  q.push_back(sy * m.w + sx);
  seen[(size_t)(sy * m.w + sx)] = 1;
  while (!q.empty()) {
    const int i = q.front();
    q.pop_front();
    const int cx = i % m.w, cy = i / m.w;
    const int nx[4] = {cx + 1, cx - 1, cx, cx}, ny[4] = {cy, cy, cy + 1, cy - 1};
    for (int k = 0; k < 4; k++) {
      if (!m.in(nx[k], ny[k])) continue;
      const int j = ny[k] * m.w + nx[k];
      if (seen[(size_t)j] || blk[(size_t)j]) continue;
      seen[(size_t)j] = 1;
      q.push_back(j);
    }
  }
  auto reached = [&](int ox, int oy, int ow, int oh) {
    for (int yy = oy - 1; yy <= oy + oh; yy++)
      for (int xx = ox - 1; xx <= ox + ow; xx++) {
        if (xx >= ox && xx < ox + ow && yy >= oy && yy < oy + oh) continue;
        if (m.in(xx, yy) && seen[(size_t)yy * m.w + xx]) return true;
      }
    return false;
  };
  if (m.up.x >= 0 && !reached(m.up.x, m.up.y, 1, 1)) { why = "THE STAIRS WOULD BE BLOCKED"; return false; }
  Plot t = p;
  PlacedObj po;
  po.kind = (uint8_t)o; po.x = (uint8_t)x; po.y = (uint8_t)y; po.flags = (uint8_t)((turned ? 1 : 0) | (m.floor << 4));
  t.inside.push_back(po);
  for (const PlacedObj& q2 : t.inside) {
    if (q2.floor() != m.floor) continue;
    const uint16_t f = objInfo((Obj)q2.kind).flags;
    if (!(f & (OBJ_BED | OBJ_STORAGE | OBJ_COOK))) continue;
    int qw, qh;
    footprint((Obj)q2.kind, q2.turned(), qw, qh);
    if (!reached(q2.x, q2.y, qw, qh)) { why = std::string("THE ") + objInfo((Obj)q2.kind).name + " COULD NOT BE REACHED"; return false; }
  }
  return true;
}

// ---------------------------------------------------------------- houses
Bldg houseBldg(const Plot& p, const Game& g) {
  Bldg b;
  const ShellInfo& si = shellInfo((Shell)p.shell);
  const int32_t rx = ew::idRx(p.id), ry = ew::idRy(p.id);
  b.id = ew::makeId(rx, ry, ew::IdKind::Plot, 0xC00u | (ew::idLocal(p.id) & 0x3FFu));
  b.type = si.type;
  b.r = IRect{p.gx + p.hx, p.gy + p.hy, p.hw ? p.hw : si.w, p.hh ? p.hh : si.h};
  b.seed = p.houseSeed;
  b.site = -1;
  b.storeys = si.storeys;
  b.hearth = true;
  b.form = (uint8_t)si.form;
  b.wealth = 2;
  b.urban = 0;
  b.home = p.state == PlotState::Built ? 1 : 3;
  const int lx = b.r.x - g.world.ox, ly = b.r.y - g.world.oy;
  if (g.world.over.in(lx, ly) && !g.world.over.biome.empty()) b.biome = (Biome)g.world.over.biome[(size_t)ly * g.world.over.w + lx];
  if (p.style && g.world.src) {
    const cult::Culture& C = g.world.src->culture(p.style);
    b.arch = cult::buildingArch(C, (int)b.biome, b.urban, b.wealth, b.seed);
    b.styled = true;
  }
  return b;
}

HouseKind houseKindOf(const Bldg& b) {
  const int a = b.r.w * b.r.h;
  if (a <= 6) return HouseKind::Hut;
  if (a <= 12 && b.storeys <= 1) return HouseKind::Cottage;
  if (a <= 20) return HouseKind::Townhouse;
  if (a <= 30) return HouseKind::Manor;
  return HouseKind::Estate;
}

std::vector<int> forSaleHouses(const World& w, int site) {
  std::vector<int> out;
  if (site < 0 || site >= (int)w.sites.size()) return out;
  const Site& s = w.sites[(size_t)site];
  if (!s.settlement()) return out;
  std::vector<int> cand;
  for (int i = 0; i < s.bldgCount; i++) {
    const int bi = s.bldgFirst + i;
    if (bi < 0 || bi >= (int)w.over.bldgs.size()) break;
    const Bldg& b = w.over.bldgs[(size_t)bi];
    if (b.site != site || b.civic) continue;
    if (b.type == art::Building::House || b.type == art::Building::StoneHouse || b.type == art::Building::Hut) cand.push_back(bi);
  }
  if (cand.empty()) return out;
  const uint64_t r = h64(s.id, ew::tag("forsale"));
  int n = 0;
  if (s.type == SiteType::Village) n = (r % 100) < 40 ? 1 : 0;
  else if (s.type == SiteType::Town) n = 1 + (int)((r >> 8) % 2);
  else n = 2 + (int)((r >> 16) % 2);
  n = std::min(n, (int)cand.size());
  uint64_t q = r;
  for (int k = 0; k < n; k++) {
    q = ew::mix64(q + (uint64_t)k);
    const int bi = cand[(size_t)(q % cand.size())];
    if (std::find(out.begin(), out.end(), bi) == out.end()) out.push_back(bi);
  }
  return out;
}

// ---------------------------------------------------------------- phase B: money, staples, the kitchen
namespace {
HouseKind kindOfPlotHouse(const Plot& p) {
  Bldg b;
  b.r = IRect{0, 0, p.hw, p.hh};
  b.storeys = p.hw * p.hh > 12 ? 2 : 1;
  return houseKindOf(b);
}
}  // namespace
LotSize lotSizeOf(const Plot& p) {
  for (int s = (int)LotSize::COUNT - 1; s >= 0; s--)
    if (p.w >= lotInfo((LotSize)s).w && p.h >= lotInfo((LotSize)s).h) return (LotSize)s;
  return LotSize::Small;
}
bool shellFits(const Plot& p, Shell s) {
  const ShellInfo& si = shellInfo(s);
  return (int)lotSizeOf(p) >= (int)si.minLot && p.w >= si.w + 2 && p.h >= si.h + 3;
}

int plotPrice(const Game& g, const Plot& p) {
  const int si = g.world.siteHandle(p.site);
  const float f = priceFactor(g, si);
  if (p.kind == PlotKind::House) return (int)std::lround(houseInfo(kindOfPlotHouse(p)).price * f);
  int v = lotInfo(lotSizeOf(p)).price;
  if (p.state != PlotState::Owned) v += shellInfo((Shell)p.shell).gold;
  return (int)std::lround(v * f);
}
int plotTaxWeek(const Plot& p) {
  if (p.kind == PlotKind::House) return houseInfo(kindOfPlotHouse(p)).taxWeek;
  static const int shellTax[] = {5, 15, 20, 30, 50};
  return lotInfo(lotSizeOf(p)).taxWeek + (p.state != PlotState::Owned ? shellTax[std::min<int>(p.shell, 4)] : 0);
}
int taxOwed(const Plot& p, int today) {
  if (today <= (int)p.paidUntilDay) return 0;
  const int weeks = (today - (int)p.paidUntilDay + 6) / 7;
  return weeks * plotTaxWeek(p);
}
int lienOwed(const Game& g) {
  int owed = 0;
  for (const Plot& p : g.home.plots) {
    const int o = taxOwed(p, g.day);
    if (o >= LIEN_WEEKS * std::max(1, plotTaxWeek(p))) owed += o;
  }
  return owed;
}
int reregFee(const Game& g, const Plot& p) { return std::max(1, plotPrice(g, p) * REREG_PCT / 100); }

bool houseEmpty(const World& w, const Game* g, int bldg) {
  if (bldg < 0 || bldg >= (int)w.over.bldgs.size()) return false;
  const Bldg& b = w.over.bldgs[(size_t)bldg];
  if (b.home) return true;
  if (g && g->home.plotById(b.id) >= 0) return true;
  if (b.site < 0) return false;
  const std::vector<int> sale = forSaleHouses(w, b.site);
  return std::find(sale.begin(), sale.end(), bldg) != sale.end();
}

std::vector<Crop> seedCrops(uint64_t culture, const Game& g) {
  using C = Crop;
  static const std::vector<C> t[(int)cult::Archetype::COUNT] = {
      /* Fjordfolk */ {C::Barley, C::Rye, C::Turnip, C::Cabbage, C::Onion},
      /* Highland  */ {C::Oats, C::Potato, C::Turnip, C::Rye, C::Herbs},
      /* Heartland */ {C::Wheat, C::Barley, C::Carrot, C::Cabbage, C::Beans, C::Onion, C::Potato},
      /* Imperial  */ {C::Wheat, C::Grapes, C::Onion, C::Beans, C::Herbs},
      /* Dune      */ {C::Dates, C::Wheat, C::Onion, C::Beans},
      /* Steppe    */ {C::Barley, C::Oats, C::Onion, C::Herbs},
      /* Marsh     */ {C::Oats, C::Turnip, C::Flax, C::Cabbage, C::Herbs},
      /* Jade      */ {C::Rice, C::Tea, C::Cabbage, C::Beans},
      /* River     */ {C::Wheat, C::Dates, C::Flax, C::Onion, C::Beans},
      /* SunTemple */ {C::Maize, C::Beans, C::Potato, C::Herbs},
      /* Sylvan    */ {C::Herbs, C::Flax, C::Carrot, C::Beans},
      /* Starspire */ {C::Herbs, C::Wheat, C::Carrot},
  };
  int ar = (int)cult::Archetype::Heartland;
  if (culture && g.world.src) ar = std::clamp((int)g.world.src->culture(culture).archetype, 0, (int)cult::Archetype::COUNT - 1);
  return t[ar];
}

int foreignPremiumPct(const Item& it, uint64_t culture, const Game& g) {
  int c = -1;
  if (it.kind == ItemKind::Food && it.sub >= FOOD_CROP && it.sub < FOOD_CROP + CROPS) c = it.sub - FOOD_CROP;
  if (it.kind == ItemKind::Misc && it.sub >= MISC_CROP && it.sub < MISC_CROP + CROPS) c = it.sub - MISC_CROP;
  if (c < 0) return 0;
  const std::vector<Crop> own = seedCrops(culture, g);
  if (std::find(own.begin(), own.end(), (Crop)c) != own.end()) return 0;
  return 25 + (int)(h64(culture, (uint64_t)c * 31ull + 7ull) % 26ull);
}

std::string missingFor(const Game& g, const Meal& m) {
  static const char* nm[] = {"", "GRAIN", "FLOUR", "BREAD", "VEGETABLES", "FRUIT", "MEAT", "FISH", "EGGS", "MILK", "CHEESE", "HERBS", "HONEY"};
  int need[(int)Ingr::COUNT] = {};
  for (Ingr i : m.in) if (i != Ingr::None) need[(int)i]++;
  std::string out;
  for (int k = 1; k < (int)Ingr::COUNT; k++) {
    if (!need[k]) continue;
    int have = 0;
    for (const Item& it : g.inv) if ((int)ingredientOf(it) == k) have += it.stackable() ? it.count : 1;
    if (have < need[k]) out += (out.empty() ? "" : ", ") + std::string(nm[k]);
  }
  return out;
}

std::string propertyName(const Game& g, const Plot& p) {
  if (!p.name.empty()) return p.name;
  const int si = g.world.siteHandle(p.site);
  if (si >= 0) return g.world.sites[(size_t)si].name + (p.kind == PlotKind::House ? " HOUSE" : " HOMESTEAD");
  return p.kind == PlotKind::House ? "YOUR HOUSE" : "YOUR HOMESTEAD";
}

// ---------------------------------------------------------------- the home block
int Homes::plotAt(int32_t gx, int32_t gy) const {
  for (size_t i = 0; i < plots.size(); i++) {
    const Plot& p = plots[i];
    if (gx >= p.gx && gy >= p.gy && gx < p.gx + p.w && gy < p.gy + p.h) return (int)i;
  }
  return -1;
}
int Homes::plotById(ew::Gid id) const {
  for (size_t i = 0; i < plots.size(); i++) if (plots[i].id == id) return (int)i;
  return -1;
}
bool Homes::knowsStyle(uint64_t culture) const { return std::find(styles.begin(), styles.end(), culture) != styles.end(); }

namespace {
void writePlot(BinW& w, const Plot& p) {
  w.u8((uint8_t)p.kind); w.u8((uint8_t)p.state); w.u64(p.id); w.u64(p.site); w.i32(p.gx); w.i32(p.gy); w.u8(p.w); w.u8(p.h); w.u8(p.flags);
  w.u8(p.shell); w.u64(p.style); w.u32(p.houseSeed); w.u8((uint8_t)p.hx); w.u8((uint8_t)p.hy); w.u8(p.hw); w.u8(p.hh);
  w.u16(p.buildStartDay); w.u16(p.buildDoneDay);
  auto objs = [&](const std::vector<PlacedObj>& v) {
    w.u16((uint16_t)v.size());
    for (const PlacedObj& o : v) { w.u8(o.kind); w.u8(o.x); w.u8(o.y); w.u8(o.flags); w.u32(o.data); }
  };
  objs(p.outside);
  objs(p.inside);
  w.u16((uint16_t)p.ground.size());
  for (uint8_t b : p.ground) w.u8(b);
  w.u16((uint16_t)p.crops.size());
  for (const CropRec& c : p.crops) {
    w.u8(c.x); w.u8(c.y); w.u8(c.kind); w.u8(c.stage); w.u8(c.grown); w.u8(c.quality); w.u8(c.streak);
    w.u16(c.plantedDay); w.u16(c.lastWaterDay); w.u16(c.lastGrowDay);
  }
  w.u8((uint8_t)p.animals.size());
  for (const AnimalRec& a : p.animals) {
    w.u8(a.kind); w.u8(a.happiness); w.u8(a.produce); w.u8(a.flags); w.u8(a.breed);
    w.u16(a.boughtDay); w.u16(a.lastFedDay); w.u16(a.lastProduceDay); w.u32(a.seed);
  }
  w.u8((uint8_t)p.stores.size());
  for (const Store& s : p.stores) {
    w.u8((uint8_t)s.items.size());
    for (const Item& it : s.items) writeItem(w, it);
  }
  w.u8(p.trough); w.u16(p.lastSeenDay); w.u16(p.paidUntilDay); w.u32(p.farmhand); w.u16(p.farmhandUntil);
  w.str(p.name);
  w.u8(p.furnished); w.u8(p.starter); w.u64(p.realm); w.u16(p.farmhandRes); w.u16(p.raidDay); w.u16(p.taxNoticeDay);
}
bool readPlot(BinR& r, Plot& p) {
  p = Plot();
  p.kind = (PlotKind)std::min<int>(r.u8(), 1); p.state = (PlotState)std::min<int>(r.u8(), 2);
  p.id = r.u64(); p.site = r.u64(); p.gx = r.i32(); p.gy = r.i32(); p.w = r.u8(); p.h = r.u8(); p.flags = r.u8();
  p.shell = r.u8(); p.style = r.u64(); p.houseSeed = r.u32(); p.hx = (int8_t)r.u8(); p.hy = (int8_t)r.u8(); p.hw = r.u8(); p.hh = r.u8();
  p.buildStartDay = r.u16(); p.buildDoneDay = r.u16();
  if (p.shell >= (uint8_t)Shell::COUNT) p.shell = 0;
  auto objs = [&](std::vector<PlacedObj>& v) {
    const int n = r.u16();
    if (n > MAX_OBJECTS * 4) { r.bad = true; return; }
    for (int i = 0; i < n && !r.bad; i++) {
      PlacedObj o;
      o.kind = r.u8(); o.x = r.u8(); o.y = r.u8(); o.flags = r.u8(); o.data = r.u32();
      if (o.kind < (uint8_t)Obj::COUNT) v.push_back(o);
    }
  };
  objs(p.outside);
  objs(p.inside);
  int n = r.u16();
  for (int i = 0; i < n && !r.bad; i++) p.ground.push_back(r.u8());
  n = r.u16();
  if (n > MAX_CROPS * 4) return false;
  for (int i = 0; i < n && !r.bad; i++) {
    CropRec c;
    c.x = r.u8(); c.y = r.u8(); c.kind = r.u8(); c.stage = r.u8(); c.grown = r.u8(); c.quality = r.u8(); c.streak = r.u8();
    c.plantedDay = r.u16(); c.lastWaterDay = r.u16(); c.lastGrowDay = r.u16();
    if (c.kind < CROPS) { c.stage = std::min<uint8_t>(c.stage, 3); c.quality = std::min<uint8_t>(c.quality, 3); p.crops.push_back(c); }
  }
  n = r.u8();
  for (int i = 0; i < n && !r.bad; i++) {
    AnimalRec a;
    a.kind = r.u8(); a.happiness = r.u8(); a.produce = r.u8(); a.flags = r.u8(); a.breed = r.u8();
    a.boughtDay = r.u16(); a.lastFedDay = r.u16(); a.lastProduceDay = r.u16(); a.seed = r.u32();
    if (a.kind < (uint8_t)Animal::COUNT) { if (a.breed >= (uint8_t)Breed::COUNT) a.breed = 0; p.animals.push_back(a); }
  }
  n = r.u8();
  for (int i = 0; i < n && !r.bad; i++) {
    Store s;
    const int m = r.u8();
    for (int k = 0; k < m && !r.bad; k++) s.items.push_back(readItem(r));
    p.stores.push_back(std::move(s));
  }
  p.trough = r.u8(); p.lastSeenDay = r.u16(); p.paidUntilDay = r.u16(); p.farmhand = r.u32(); p.farmhandUntil = r.u16();
  p.name = r.str();
  p.furnished = r.u8(); p.starter = r.u8(); p.realm = r.u64(); p.farmhandRes = r.u16(); p.raidDay = r.u16(); p.taxNoticeDay = r.u16();
  return !r.bad;
}
}  // namespace

int Homes::plotBytes(const Plot& p) {
  std::vector<uint8_t> b;
  BinW w(b);
  writePlot(w, p);
  return (int)b.size();
}

void Homes::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  w.u8(HOME_BLOCK);
  w.u16((uint16_t)styles.size());
  for (uint64_t s : styles) w.u64(s);
  w.u16((uint16_t)recipes.size());
  for (const std::string& r : recipes) w.str(r);
  w.i32(riding); w.u8(ridingBreed); w.u32(ridingSeed);
  w.i32(horse); w.i32(horseGx); w.i32(horseGy); w.u64(horseInn);
  w.u8((uint8_t)plots.size());
  for (const Plot& p : plots) {
    std::vector<uint8_t> rec;
    BinW b(rec);
    writePlot(b, p);
    w.u16((uint16_t)rec.size());
    for (uint8_t c : rec) w.u8(c);
  }
}

bool Homes::deserialize(const std::vector<uint8_t>& in) {
  BinR r(in);
  const uint8_t v = r.u8();
  if (r.bad || v != HOME_BLOCK) return false;   // an older block is refused (the save says "start a new adventure")
  Homes h;
  const int ns = r.u16();
  if (ns > 4096) return false;
  for (int i = 0; i < ns && !r.bad; i++) h.styles.push_back(r.u64());
  const int nr = r.u16();
  if (nr > 4096) return false;
  for (int i = 0; i < nr && !r.bad; i++) h.recipes.push_back(r.str());
  h.riding = r.i32(); h.ridingBreed = r.u8(); h.ridingSeed = r.u32();
  if (h.ridingBreed >= (uint8_t)Breed::COUNT) h.ridingBreed = 0;
  h.horse = r.i32(); h.horseGx = r.i32(); h.horseGy = r.i32(); h.horseInn = r.u64();
  const int np = r.u8();
  if (np > MAX_PLOTS * 4) return false;
  for (int i = 0; i < np && !r.bad; i++) {
    const int len = r.u16();
    if (r.bad || r.p + (size_t)len > in.size()) return false;
    std::vector<uint8_t> rec(in.begin() + (std::ptrdiff_t)r.p, in.begin() + (std::ptrdiff_t)(r.p + (size_t)len));
    r.p += (size_t)len;
    BinR pr(rec);
    Plot p;
    if (!readPlot(pr, p)) return false;
    h.plots.push_back(std::move(p));
  }
  if (r.bad) return false;
  if (h.riding >= (int)h.plots.size() * 64 || h.riding < -1) h.riding = -1;
  if (h.horse >= (int)h.plots.size() * 64 || h.horse < -1) h.horse = -1;
  if (h.riding >= 0 && ((size_t)(h.riding % 64) >= h.plots[(size_t)(h.riding / 64)].animals.size())) h.riding = -1;
  if (h.horse >= 0 && ((size_t)(h.horse % 64) >= h.plots[(size_t)(h.horse / 64)].animals.size())) h.horse = -1;
  *this = std::move(h);
  return true;
}

}  // namespace home
