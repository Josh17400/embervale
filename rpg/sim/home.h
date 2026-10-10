// M7 "Home": housing, land, building, farming, animals, cooking and mounts (VISION_PLAN 8, 13 M7, 15.2, 15.22; owner M7
// notes 2026-10-09). Shared contract written by the lead in M7 phase A (2026-10-09).
//
// What the player can do (owner): buy a cottage (a vacant house in a village or town) or a plot of land (a fenced lot
// on a settlement's outskirts, some on a river bank), build a house on a plot in ANY culture style they have
// discovered (the M3b builder designs it from a Bldg like every generated building: 3/4 view, depth, seamless walls;
// a house of 2+ storeys has stairs and an upper-floor map), decorate and store inside, lay out the yard (fences,
// gates, paths, farmland, well, coop, pen, stable, beehive...), plant ~17 crops with 4 growth stages through four
// seasons, keep chickens, goats, sheep, cows, pigs, a dog and horses (predators raid at night), hire a farmhand, cook
// meals (Well Fed; better at a hearth, cooking pot or an inn's kitchen than at a campfire) from crops, livestock, fish
// and the economy's own goods (grain -> flour -> bread at the mill and the oven), and ride a horse (8 tiles/s, +15 % on
// roads; dismount to fight, to enter a building or a site, in water).
//
// Data flow:
//   - generation (rpg/world, LAND lane): settlements carry fenced empty LOTS (ew::PlotPlan) in their chunk records;
//     World::lots holds the ones met (global tiles, append-only; World::lotHandle by id). Vacant houses are not
//     generated: forSaleHouses() picks them deterministically from a settlement's buildings (the census leaves them
//     empty).
//   - the player's property lives in Game::home (Homes, saved: the home block, SAVE_VER 14). A Plot is the record of
//     one property (a bought house or a bought lot), with its yard objects, ground edits, crops, animals, storage and,
//     on a lot, the house built on it (shell, culture style, seed: a Bldg made by houseBldg(), designed by the builder).
//   - the window: homeStep (HOMESTEAD lane, rpg/sim/home_game.cpp) stamps every owned plot the Active Window shows
//     whenever World::placeSerial changes (ground edits, Filler under solid yard objects, the built house's Bldg into
//     World::over.bldgs and bldgAt, FOR SALE signs on lots and vacant houses), so walking, saving and loading keep them.
//   - the view (VIEW lane, rpg/view/home_view.cpp) draws yard objects, crops, farm animals and the mounted rider from
//     Game::home through View::homeCollect (y-sorted with the scene) and the build / decorate / storage / cooking UI
//     (Mode::Build) through Homes::ui and the actions below; it never edits Homes directly.
//
// Rules: deterministic (catch-up and growth are integer maths on days; RNG draws sequenced explicitly, never two draws
// as arguments of one call); offline catch-up is O(crops + animals) per plot whatever the days away; a plot's record
// stays <= 4 KB (PLOT_BYTES_BUDGET; rpg_test --home measures a full farm); prices are in the M6 bands' slow
// rags-to-riches curve (property is the big money sink: a hut is weeks of a starting player's income).
//
// Ownership (M7 phase B): this header, home.cpp, home_game.cpp (and any rpg/sim/home_*.cpp) belong to the HOMESTEAD
// lane, which may ADD to this header (never rename, remove or change the meaning of what phase A put here). The
// art enums used here (art::Crop, art::FarmObj, the farm Critters, the M7 Icons) are rpg/art/art_home.h's (ART lane).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/build/blueprint.h"
#include "rpg/sim/common.h"
#include "rpg/sim/items.h"
#include "rpg/sim/world.h"
#include "rpg/world/economy.h"
#include "rpg/world/ids.h"

class Game;
struct Actor;
struct DlgOpt;

namespace home {

// ---------------------------------------------------------------- calendar (VISION_PLAN 8.4)
constexpr int SEASON_DAYS = 28, YEAR_DAYS = 112;
enum class Season : uint8_t { Spring, Summer, Autumn, Winter, COUNT };
inline Season seasonOf(int day) { return (Season)((((day - 1) % YEAR_DAYS) + YEAR_DAYS) % YEAR_DAYS / SEASON_DAYS); }
const char* seasonName(Season s);   // "SPRING"...
constexpr uint8_t seasonBit(Season s) { return (uint8_t)(1u << (unsigned)s); }

// ---------------------------------------------------------------- property (VISION_PLAN 8.1, 8.2)
// a house bought as it stands (a vacant generated house; its footprint decides the kind)
enum class HouseKind : uint8_t { Hut, Cottage, Townhouse, Manor, Estate, COUNT };
struct HouseInfo {
  const char* name;       // "COTTAGE"
  uint8_t w, h;           // the smallest footprint of this kind (a vacant house of at least this size)
  int price;              // gold, before the settlement's wealth and the kingdom's tax policy (priceFactor)
  int taxWeek;            // gold per 7 days (paid at the reeve / steward; arrears are the HOMESTEAD lane's rule)
  uint8_t where;          // bit per SiteType: 1 City, 2 Town, 4 Village (where such houses are sold)
};
const HouseInfo& houseInfo(HouseKind k);
// a lot (ew::PlotPlan::size)
enum class LotSize : uint8_t { Small, Medium, Large, COUNT };
struct LotInfo { const char* name; uint8_t w, h; int price; int taxWeek; };   // 10x8 1500, 13x10 3500, 16x12 6000
const LotInfo& lotInfo(LotSize s);
// the house shells a lot can take (VISION_PLAN 8.2 step 1). The builder designs the house from houseBldg(): the
// shell gives the purpose, footprint, form and storeys; the culture gives the style (any culture the player has
// discovered: Homes::styles).
enum class Shell : uint8_t { Hut, Cottage, Longhouse, Townhouse, Hall, COUNT };
struct ShellInfo {
  const char* name;       // "COTTAGE"
  uint8_t w, h;           // footprint in tiles (the door at the bottom row's middle, Bldg::doorX)
  uint8_t storeys;        // 2+: stairs and an upper-floor map (VISION_PLAN 15.7)
  bld::Form form;         // Auto (the culture's own) or Long for the longhouse
  art::Building type;     // House / StoneHouse / Farmhouse
  uint8_t timber, iron;   // materials the player may bring (craft::Stuff::Timber, IronIngot as nails) to cut the price
  int gold;               // the builder's price with no materials brought (stone, roofing, wages, everything)
  int timberCredit, ironCredit;   // gold off per unit brought
  uint8_t days;           // construction time in days (2..5)
  LotSize minLot;         // the smallest lot it fits on
};
const ShellInfo& shellInfo(Shell s);

// what a settlement's wealth and its kingdom's policy do to a property price (0.7 .. 1.4 x, VISION_PLAN 8.1)
float priceFactor(const Game& g, int site);

// ---------------------------------------------------------------- yard and interior objects (VISION_PLAN 8.2, 8.3)
// Obj indexes the catalogue (saved as u8: append only). The art of each is art::FarmObj (rpg/art/art_home.h) or, for
// furniture, an existing art::Prop (ObjInfo::prop).
enum class Obj : uint8_t {
  // outside
  Fence, Gate, Path, Farmland, Well, Woodpile, Beehive, Scarecrow, Coop, Pen, Stable, Trough, Workbench, Forge,
  FlowerBed, Sapling, Bench, Lantern, Statue, Banner, Campfire, Doghouse, HayRack, ShippingCrate,
  // inside (furniture)
  Bed, Table, Chair, Shelf, Chest, Rug, Hearth, CookPot, Mannequin, WeaponRack, Trophy, Painting, Wardrobe,
  Bookshelf, PlantPot, Candelabra, Stool, Cupboard,
  COUNT
};
// ObjInfo::flags
enum : uint16_t {
  OBJ_OUTSIDE = 1,        // placed in the yard
  OBJ_INSIDE = 2,         // placed in the house
  OBJ_SOLID = 4,          // blocks walking (stamped as Filler / its prop)
  OBJ_ROTATES = 8,        // two-way (fences, benches, beds): PlacedObj::flags bit 0 = turned
  OBJ_STORAGE = 16,       // a store (PlacedObj::data = its storage index into Plot::stores)
  OBJ_COOK = 32,          // a cooking station (Hearth, CookPot, Campfire)
  OBJ_BED = 64,           // sleep here (Rested; the respawn point)
  OBJ_ANIMALS = 128,      // houses animals (Coop: chickens; Pen: goats, sheep, cows, pigs; Stable: horses)
  OBJ_GROUND = 256,       // a ground edit, not an object (Path, Farmland): Plot::ground
  OBJ_WALLDECOR = 512,    // hangs on a back wall (Trophy, Painting): placed on an interior's wall row
  OBJ_JOINS = 1024,       // joins its neighbours seamlessly (fences, gates, paths, farmland)
};
struct ObjInfo {
  const char* name;       // "CHICKEN COOP"
  uint8_t w, h;           // footprint in tiles (turned: h x w)
  uint16_t flags;         // OBJ_*
  int price;              // gold (selling back refunds 50 %)
  art::Prop prop;         // furniture drawn as this existing prop (Prop::COUNT: drawn as art::FarmObj by the view)
  uint8_t capacity;       // animals it houses (OBJ_ANIMALS), stacks it stores (OBJ_STORAGE)
};
const ObjInfo& objInfo(Obj o);
art::FarmObj farmObjArt(Obj o);   // the art of a yard object (FarmObj::COUNT: furniture, drawn as ObjInfo::prop)

struct PlacedObj {
  uint8_t kind = 0;       // Obj
  uint8_t x = 0, y = 0;   // outside: tile within the plot; inside: tile within the floor's interior map
  uint8_t flags = 0;      // bit 0 turned; bits 4-7: the floor (inside)
  uint32_t data = 0;      // OBJ_STORAGE: index into Plot::stores; Trophy: art::Monster; Painting: the site id's low bits
  int floor() const { return flags >> 4; }
  bool turned() const { return flags & 1; }
};

// ---------------------------------------------------------------- crops (VISION_PLAN 8.4)
// art::Crop (rpg/art/art_home.h): Wheat, Barley, Oats, Rye, Potato, Turnip, Cabbage, Carrot, Onion, Flax, Beans,
// Grapes, Dates, Maize, Tea, Rice, Herbs. 4 growth stages: 0 seedling, 1 growing, 2 ripening, 3 ripe (harvest).
using Crop = art::Crop;
constexpr int CROPS = (int)art::Crop::COUNT;
constexpr int CROP_STAGES = 4;
struct CropInfo {
  const char* name;       // "BARLEY"
  uint8_t days;           // watered days from planting to ripe (3..8)
  uint8_t yieldMin, yieldMax;
  uint8_t seasons;        // seasonBit mask it grows in (winter: the cold crops only)
  bool warm;              // a warm-land crop (seeds sold by cultures whose lands are warm)
  bool perennial;         // stays after harvest and ripens again (grapes, dates, tea)
  bool paddy;             // needs a water tile beside it (rice)
  ew::Good good;          // the economy good it trades as (Grain, Produce, Wool for flax)
  int seedPrice;          // gold per seed
  int value;              // list price of one harvested unit
};
const CropInfo& cropInfo(Crop c);
struct CropRec {
  uint8_t x = 0, y = 0;   // tile within the plot (on Farmland)
  uint8_t kind = 0;       // Crop
  uint8_t stage = 0;      // 0..3
  uint8_t grown = 0;      // watered days grown so far (stage = grown * 4 / days, 3 when ripe)
  uint8_t quality = 0;    // 0..3 from the watering streak
  uint8_t streak = 0;     // days watered in a row
  uint16_t plantedDay = 0, lastWaterDay = 0, lastGrowDay = 0;
  bool wilted(int today) const { return today - (int)lastWaterDay >= 2; }   // dry 2 days: wilts (nothing dies)
};

// ---------------------------------------------------------------- animals (VISION_PLAN 8.5)
enum class Animal : uint8_t { Chicken, Goat, Sheep, Cow, Pig, Horse, Dog, Duck, COUNT };
struct AnimalInfo {
  const char* name;       // "COW"
  art::Critter critter;   // its sprite sheet (art::critterSheet)
  Obj home;               // Coop / Pen / Stable (Obj::COUNT: none, the dog)
  int price;
  uint8_t produceEvery;   // days between products (0: none)
  uint8_t product;        // Product (below) it gives
  uint8_t hay;            // hay per day
};
const AnimalInfo& animalInfo(Animal a);
struct AnimalRec {
  uint8_t kind = 0;       // Animal
  uint8_t happiness = 50; // 0..100: fed, room, groomed -> produce quality
  uint8_t produce = 0;    // products waiting to be collected
  uint8_t flags = 0;      // AF_*
  uint8_t breed = 0;      // horses: Breed
  uint16_t boughtDay = 0, lastFedDay = 0, lastProduceDay = 0;
  uint32_t seed = 0;      // name and coat (nameOf(); art::critterSheet variant)
};
enum : uint8_t { AF_GROOMED = 1, AF_SICK = 2, AF_LOST = 4 /* taken by a predator: kept a day for the news, then removed */ };
std::string animalName(const AnimalRec& a);   // a pet name from its seed ("BRAMBLE")

// products of animals and the kitchen's goods (items: makeGood)
enum class Product : uint8_t { None, Egg, Milk, Wool, Hay, Flour, Honey, Cheese, Truffle, COUNT };

// ---------------------------------------------------------------- horses and riding (VISION_PLAN 8.5)
enum class Breed : uint8_t { Heartland, Steppe, Highland, Dune, Fjord, Marsh, COUNT };
struct BreedInfo {
  const char* name;       // "STEPPE HORSE"
  float speed;            // tiles per second (8 the common horse)
  bool sure;              // sure-footed: no slow-down on ramps and rough ground
  int price;              // 600 .. 3000 at town stables
  uint32_t coat;          // the usual coat (rgba; the sheet varies it by AnimalRec::seed)
};
const BreedInfo& breedInfo(Breed b);
Breed breedOfCulture(uint64_t culture, const Game& g);   // culture-bound breeds (steppe horses fastest...)
constexpr float RIDE_ROAD_BONUS = 0.15f;   // +15 % on Road / Plaza / Bridge ground
constexpr float WALK_SPEED_PX = 74.0f;     // the player's base walking speed (Actor::speed, player.cpp)
// why the rider got off (homeDismount; tests count them)
enum class Dismount : uint8_t { Player, Attack, Hurt, Enter, Water, Talk, Sleep, Travel, COUNT };
const char* dismountName(Dismount d);

// ---------------------------------------------------------------- cooking (VISION_PLAN 8.4, 15.2)
// Ingredients by kind: every food and good item maps to one (ingredientOf), so the existing foods (bread, venison,
// trout, cheese...), harvests, animal products and the economy's goods (flour from the mill) all cook.
enum class Ingr : uint8_t { None, Grain, Flour, Bread, Veg, Fruit, Meat, Fish, Egg, Milk, Cheese, Herb, Honey, COUNT };
Ingr ingredientOf(const Item& it);
// where a meal is cooked; better stations make better meals (owner 15.2: hearths, inns > campfires)
enum class Station : uint8_t { Campfire, Hearth, CookPot, InnKitchen, COUNT };
struct Meal {
  std::string id;         // "stew", "bread", "<culture>.1" (a culture's signature recipe)
  std::string name;       // "HUNTER'S STEW"
  Ingr in[3] = {Ingr::None, Ingr::None, Ingr::None};
  uint8_t quality = 1;    // 1..3 before the station bonus
  uint64_t culture = 0;   // a signature recipe (taught by that culture's innkeepers)
  int heal = 20;
};
const std::vector<Meal>& baseMeals();
std::vector<Meal> cultureMeals(uint64_t culture, const Game& g);   // the 3 signature recipes of a culture
// what the player can cook: every base meal, then each culture recipe learned (Homes::recipes ids, in learning order)
std::vector<Meal> knownMeals(const Game& g);
// the cooked item (ItemKind::Food, sub FOOD_MEAL + index): power = heal, tier = the final quality 0..3
Item makeMeal(const Meal& m, Station st);
// the Well Fed it gives (15.2): hours and the meal quality written into life::PlayerNeeds (homeAte)
void mealBuff(const Item& meal, float& fedHours, uint8_t& quality);

// ---------------------------------------------------------------- item conventions (no new ItemKind)
// sub ranges kept clear of makeFood (0..8), makeMisc (0..12) and craft::MISC_LORE (200)
constexpr uint8_t FOOD_CROP = 64;      // ItemKind::Food, sub FOOD_CROP + Crop: an edible harvest (veg, fruit)
constexpr uint8_t FOOD_PRODUCT = 96;   // ItemKind::Food, sub FOOD_PRODUCT + Product: eggs, milk, cheese, honey
constexpr uint8_t FOOD_MEAL = 128;     // ItemKind::Food, sub FOOD_MEAL + meal index: a cooked meal
constexpr uint8_t MISC_CROP = 64;      // ItemKind::Misc, sub MISC_CROP + Crop: grain, flax, tea (not eaten raw)
constexpr uint8_t MISC_SEED = 96;      // ItemKind::Misc, sub MISC_SEED + Crop: seeds
constexpr uint8_t MISC_PRODUCT = 128;  // ItemKind::Misc, sub MISC_PRODUCT + Product: wool, hay, flour
constexpr uint8_t MISC_TOOL = 144;     // ItemKind::Misc, sub MISC_TOOL + Tool: hoe, watering can, sickle, brush
constexpr uint8_t MISC_DEED = 160;     // ItemKind::Misc, sub MISC_DEED: a deed (Item::seed = the plot's index)
enum class Tool : uint8_t { Hoe, WateringCan, Sickle, Brush, COUNT };
Item makeCropItem(Crop c, int count, int quality);   // Food or Misc by the crop
std::string seedName(Crop c);   // "SEED POTATOES", "TURNIP SEEDS"...
Item makeSeeds(Crop c, int count);
Item makeProduct(Product p, int count);
Item makeTool(Tool t);
bool isSeed(const Item& it, Crop& c);
bool isTool(const Item& it, Tool t);
bool isHomeGood(const Item& it);       // any of the above (shops buy them; the economy's good: goodOf)
ew::Good goodOf(const Item& it);       // the economy good a home item sells as (Good::COUNT: none)

// ---------------------------------------------------------------- the property record (saved)
enum class PlotKind : uint8_t { House, Lot };
enum class PlotState : uint8_t { Owned, Building, Built };   // a Lot: Owned (bare) -> Building -> Built; a House: Built
constexpr int PLOT_BYTES_BUDGET = 4096;      // one plot's record in the home block, a full farm (rpg_test --home)
constexpr int STORE_STACKS = 24;             // stacks per storage object (a chest, a shelf, the shipping crate)
constexpr int MAX_OBJECTS = 96, MAX_CROPS = 120, MAX_ANIMALS = 16, MAX_PLOTS = 8;
struct Store { std::vector<Item> items; };
struct Plot {
  PlotKind kind = PlotKind::Lot;
  PlotState state = PlotState::Owned;
  ew::Gid id = 0;              // a Lot: its ew::PlotPlan id (or a script's made-up lot: IdKind::Plot, local >= 0x800);
                               // a House: the Bldg id bought
  ew::Gid site = 0;            // the settlement it belongs to
  int32_t gx = 0, gy = 0;      // the plot's top-left (global tile); a House: its yard's top-left (the footprint plus
  uint8_t w = 0, h = 0;        //   a margin the generator left free; w, h its size)
  uint8_t flags = 0;           // PF_*
  // the house (a Lot once built; a House: the bought building's own facts are regenerated, these stay 0)
  uint8_t shell = 0;           // Shell
  uint64_t style = 0;          // the culture whose style it is built in (cult::CultureId)
  uint32_t houseSeed = 0;      // Bldg::seed of the built house
  int8_t hx = 0, hy = 0;       // the house footprint's top-left within the plot
  uint8_t hw = 0, hh = 0;      // its size (a lot: the shell's; a bought house: its building's; 0: no house yet)
  uint16_t buildStartDay = 0, buildDoneDay = 0;
  // the yard and the house
  std::vector<PlacedObj> outside, inside;
  std::vector<uint8_t> ground; // RLE over the plot's w*h tiles, row-major: (count, kind) pairs; kind 0 none, 1 path,
                               // 2 farmland, 3 farmland watered today (the view shows it darker)
  std::vector<CropRec> crops;
  std::vector<AnimalRec> animals;
  std::vector<Store> stores;   // indexed by PlacedObj::data of OBJ_STORAGE objects
  uint8_t trough = 0;          // days of hay in the troughs (a trough holds 7 per animal home)
  uint16_t lastSeenDay = 0;    // the day the plot was last caught up (catchUp)
  uint16_t paidUntilDay = 0;   // tax paid up to this day
  uint32_t farmhand = 0;       // the hired farmhand (life::npcId low bits; 0 none)
  uint16_t farmhandUntil = 0;  // paid up to this day (10 gold a day)
  std::string name;            // the player's name for it ("BRYNJA'S FARM"); empty: "<SETTLEMENT> HOMESTEAD"
  // ---- M7 phase B (HOMESTEAD lane; home block v2)
  uint8_t furnished = 0;       // bit per floor: the bought house's generated furniture was taken into `inside` there
                               // (homeMapLoaded on the first entry; from then on Plot::inside is the furniture)
  uint8_t starter = 0;         // a lot's house: the starter bed and chest were stamped into `inside` on completion (1)
  ew::Gid realm = 0;           // the kingdom whose register the deed is in (the settlement's owner when bought or
                               // re-registered); another kingdom takes the place: honoured at rep >= 0, else PF_REREG
  uint16_t farmhandRes = 0xFFFF;   // the farmhand's census index in the plot's settlement (0xFFFF none)
  uint16_t raidDay = 0;        // the last night predators raided (an AF_LOST animal is kept a day for the news)
  uint16_t taxNoticeDay = 0;   // the last day an arrears reminder was given
};
enum : uint8_t {
  PF_RIVERSIDE = 1, PF_FENCED = 2, PF_DOG = 4,
  PF_RESPAWN = 8,     // the player respawns here
  PF_EXPOSED = 16,    // no fence ring and within 60 tiles of a den or the wildlands: predators may raid (homeStep sets it)
  PF_REREG = 32,      // (phase B) the settlement changed hands and the new kingdom does not honour the deed (rep < 0):
                      // the 25 % re-registration fee is owed at the seller (the house stays the player's; taxes wait)
  PF_RAIDED = 64,     // (phase B) the last raid's news was told (cleared when the lost animal is removed)
};
// (phase B) the PlacedObj::data conventions of the yard objects that keep state:
//   Sapling: the day it was planted (it is a grown tree from SAPLING_DAYS later: saplingGrown)
//   Beehive: honey waiting (low 8 bits, 0..HONEY_MAX) | the day of the last comb << 8 (catchUp fills it)
constexpr int SAPLING_DAYS = 10, HONEY_EVERY = 3, HONEY_MAX = 5;
inline bool saplingGrown(const PlacedObj& o, int today) { return today - (int)(o.data & 0xFFFF) >= SAPLING_DAYS; }
inline int honeyOf(const PlacedObj& o) { return (int)(o.data & 0xFF); }
// (phase B) property money: the price of a lot / vacant house / build here (x priceFactor), the week's tax of a plot,
// what is owed now (arrears: the weeks since paidUntilDay; 0 when paid up), the re-registration fee (25 % of its price)
int plotPrice(const Game& g, const Plot& p);
// (M7 fixer r2) a lot's size class from its w x h, and whether a shell may be built on it (ShellInfo::minLot and the
// footprint plus a yard row and the fence: startBuild and the shell screen share it)
LotSize lotSizeOf(const Plot& p);
bool shellFits(const Plot& p, Shell s);
int plotTaxWeek(const Plot& p);
int taxOwed(const Plot& p, int today);
int reregFee(const Game& g, const Plot& p);
// (M7 fix) arrears are enforced: a plot LIEN_WEEKS or more weeks behind puts a lien on the player's name in the
// register: no builder takes a commission and no seller sells more land until the taxes are paid (lienOwed: the gold
// owed on such plots, 0 when none)
constexpr int LIEN_WEEKS = 4;
int lienOwed(const Game& g);

// the ground edit at a tile of the plot (0 none, 1 path, 2 farmland, 3 farmland watered)
int groundAt(const Plot& p, int x, int y);
void setGround(Plot& p, int x, int y, int kind);
// the stores of storage objects: allocStore hands out a free slot (an empty store no storage object refers to; store 0
// stays the house's first chest), or -1 when the plot has MAX_STORES; trimStores drops the unreferenced empty tail
constexpr int MAX_STORES = 250;
int allocStore(Plot& p);
void trimStores(Plot& p);
// the yard watch: a doghouse in the yard or a living dog (PF_DOG follows them)
void updateDogFlag(Plot& p);

// what the build / decorate UI is doing (UI-facing, never saved; the VIEW lane drives it, the actions below apply it)
enum class UiMode : uint8_t { None, Shell, Yard, Decorate, Storage, Cook, Buy };
struct Ui {
  UiMode mode = UiMode::None;
  int plot = -1;               // Homes::plots index
  int sel = 0;                 // the catalogue row (an Obj, a Shell, a Meal...) the list shows highlighted
  int gx = 0, gy = 0;          // the ghost's tile (outside: within the plot; inside: within the floor's interior map)
  bool turned = false;
  uint64_t style = 0;          // Shell: the culture chosen
  int store = -1;              // Storage: the store open
  Station station = Station::Campfire;   // Cook: where
  std::string why;             // the last refusal ("THE PATH TO THE DOOR WOULD BE BLOCKED")
  // ---- phase B (HOMESTEAD lane)
  int timber = 0, iron = 0;    // Shell: the materials the player brings (startBuild's timber / iron; the screen may
                               // offer 0 .. ShellInfo::timber / iron, capped by what the pack holds: haveMaterials)
  int builder = -1;            // Shell: the builder's Actor::id who opened it (-1: opened from the yard)
  int64_t placedKey = -1;      // (M7 fix) the ghost (piece, turn, tile) last placed: while the ghost has not moved
                               // the screen says PLACED and hides the ghost (not a red "in the way" over the new piece)
  int64_t ghostKey() const { return ((int64_t)sel << 41) | ((int64_t)(turned ? 1 : 0) << 40) | ((int64_t)(gy & 0xFFFFF) << 20) | (int64_t)(gx & 0xFFFFF); }
  bool justPlaced() const { return placedKey >= 0 && placedKey == ghostKey(); }
};

// ---------------------------------------------------------------- the home store (Game::home; the home block)
struct Homes {
  std::vector<Plot> plots;
  std::vector<uint64_t> styles;   // the cultures whose styles the player has discovered (build in any of them)
  std::vector<std::string> recipes;   // culture signature meals learned (Meal::id; innkeepers teach them)
  int riding = -1;                // the horse ridden: plot * 64 + animal index (-1 on foot)
  uint8_t ridingBreed = 0;        // its Breed (kept here so riding needs no plot look-up per step)
  uint32_t ridingSeed = 0;        // its AnimalRec::seed (the coat)
  int dismounts[(int)Dismount::COUNT] = {};   // never saved (tests)
  Ui ui;                          // never saved
  // ---- phase B (HOMESTEAD lane; home block v2): where the horse last ridden is. horse: plot * 64 + animal of the
  // horse out of its stable (-1: every horse is at home); while ridden it is under the player. Dismounted it waits at
  // (horseGx, horseGy) (global tile; an actor while near), walks home when left far away (HORSE_LEFT tiles, or a
  // journey without it), or is stabled at an inn (horseInn: the settlement's id; 0 none) for a fee a night (while
  // stabled horseGy holds the day she came; the keep is paid on fetching her: stableOwed).
  int horse = -1;
  int32_t horseGx = 0, horseGy = 0;
  ew::Gid horseInn = 0;
  bool riding_() const { return riding >= 0; }
  int plotAt(int32_t gx, int32_t gy) const;   // the owned plot holding a global tile (-1 none)
  int plotById(ew::Gid id) const;
  bool knowsStyle(uint64_t culture) const;
  // the home block of the save: its own version byte first
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);
  static int plotBytes(const Plot& p);        // one plot's record size (the 4 KB budget)
};

// ---------------------------------------------------------------- pure rules (home.cpp; tests drive them headlessly)
// growth and production from `fromDay` to `toDay` (exclusive of fromDay's own growth, inclusive of toDay's): rain
// waters on the days rainDay() says, the farmhand waters and harvests into the first store (when hired for those
// days), animals eat from the trough and give their products, a PF_EXPOSED plot may lose one animal to a night raid
// (5 %, 2.5 % with a dog). Cheap whatever the time away: at most CATCHUP_DAYS days are walked (O(crops + animals) a
// day; a plot left longer settles to the same state). Deterministic: catchUp(a, b) then catchUp(b, c) == catchUp(a, c)
// for spans under CATCHUP_DAYS.
constexpr int CATCHUP_DAYS = YEAR_DAYS;
// (phase B) the harvest of one ripe crop: yieldMin..yieldMax by the roll, never the least at quality 2+, one more at 3
int harvestYield(const CropInfo& ci, int quality, uint64_t roll);
// (phase B) the night before `day` brings predators to an exposed plot (5 %, 2.5 % with a dog): catchUp's roll, and the
// live raid's when the player is near
bool raidRoll(const Plot& p, int day, uint64_t worldSeed);
void catchUp(Plot& p, int fromDay, int toDay, uint64_t worldSeed);
bool rainDay(uint64_t worldSeed, int32_t gx, int32_t gy, int day);   // the region's weather that day (deterministic)
// can an object go at (x, y) (plot tiles, outside) / (x, y, floor) inside a map: no overlap, inside the plot, and the
// BFS from the plot's gate to the house door and to every usable object must survive. why: the refusal.
bool canPlaceOutside(const Plot& p, Obj o, int x, int y, bool turned, std::string& why);
bool canPlaceInside(const Plot& p, const Map& floorMap, Obj o, int x, int y, bool turned, std::string& why);
// the yard's gate (plot tiles; the fence's opening, or the middle of the side facing the street) and the house door
// (plot tiles; -1 when no house)
void gateOf(const Plot& p, int& x, int& y);
void doorOf(const Plot& p, int& x, int& y);
// the Bldg of a built (or building: scaffolding) house on a lot, in GLOBAL tiles (World::over wants local: subtract
// ox / oy): purpose, footprint, the culture's style (cult::buildingArch), storeys, wealth, the builder's form. id is
// makeId(plot region, IdKind::Plot, 0xC00 | plot local) so it never collides with a generated building.
Bldg houseBldg(const Plot& p, const Game& g);
// the vacant houses a settlement sells (VISION_PLAN 8.1: 0-1 per village (40 %), 1-2 per town, 2-3 per city),
// deterministic from the site's id: indices into World::over.bldgs (the site's range)
std::vector<int> forSaleHouses(const World& w, int site);
HouseKind houseKindOf(const Bldg& b);

// ---------------------------------------------------------------- actions on a Game (home_game.cpp; UI, scripts, tests)
// each returns false with `why` set when refused (not enough gold, blocked path, wrong season...)
bool buyHouse(Game& g, int bldg, std::string& why);
bool buyLot(Game& g, int lot, std::string& why);            // World::lots index
bool startBuild(Game& g, int plot, Shell s, uint64_t culture, int timber, int iron, std::string& why);
bool placeObj(Game& g, int plot, Obj o, int x, int y, bool turned, bool inside, std::string& why);
bool removeObj(Game& g, int plot, int x, int y, bool inside, std::string& why);   // refunds 50 %
bool till(Game& g, int plot, int x, int y, std::string& why);
bool plant(Game& g, int plot, int x, int y, Crop c, std::string& why);
bool water(Game& g, int plot, int x, int y, std::string& why);
bool harvest(Game& g, int plot, int x, int y, std::string& why);
bool buyAnimal(Game& g, int plot, Animal a, std::string& why);
bool collect(Game& g, int plot, std::string& why);          // eggs, milk, wool waiting
bool feedHay(Game& g, int plot, int hay, std::string& why);
bool hireFarmhand(Game& g, int plot, int days, std::string& why);
bool cook(Game& g, const Meal& m, Station st, std::string& why);   // takes the ingredients from the pack
bool storePut(Game& g, int plot, int store, int invIndex, std::string& why);
bool storeTake(Game& g, int plot, int store, int index, std::string& why);
bool mount(Game& g, int plot, int animal, std::string& why);
// the stamp of the owned plots (and FOR SALE signs) on the window: homeStep calls it when World::placeSerial changed
void stampWindow(Game& g);
// (scripts, tests) a made-up lot of this size at a global tile, owned (no price), so a farm can be staged anywhere
int debugLot(Game& g, int32_t gx, int32_t gy, LotSize s);
// what the use button would do on the player's property right now ("PLANT BARLEY", "WATER", "HARVEST CARROTS", "TILL",
// "COLLECT EGGS", "RIDE"...; "" nothing): the HUD's prompt (VIEW lane) says exactly what homeInteract will do
std::string interactLabel(const Game& g);
// (M7 fix) a yard use (crop, coop, stable...) lies in front of the player: Game::interactTarget then passes over
// townsfolk (a passer-by in the yard never takes the use button from the farm work)
bool yardUseAhead(const Game& g);

// ---------------------------------------------------------------- phase B additions (HOMESTEAD lane)
constexpr int HORSE_LEFT = 120;            // tiles: a horse left farther than this walks home to its stable
constexpr int STABLE_INN_FEE = 5;          // gold a night at an inn's stable
constexpr int FARMHAND_WAGE = 10;          // gold a day
constexpr int REREG_PCT = 25;
constexpr int MILL_FEE = 1;                // gold per sack the miller asks to grind grain into flour
// a building no resident lives in: a vacant house for sale or one the player owns (census.cpp, life_game.cpp)
bool houseEmpty(const World& w, const Game* g, int bldg);
// the player's own house the current interior belongs to (Homes::plots index; -1 none)
int plotOfInterior(const Game& g);
// the station a cooking prop makes (false: not a cooking place). Outdoors only the campfire; inside an inn the
// inn's kitchen (Station::InnKitchen)
bool stationOf(const Game& g, art::Prop p, Station& st);
// the crops whose seeds a culture's merchants sell (its staples first; warm crops only where its land is warm)
std::vector<Crop> seedCrops(uint64_t culture, const Game& g);
// how much a merchant of `culture` pays for a home good over its list price (percent: 0, or 25..50 for a crop
// foreign to that culture)
int foreignPremiumPct(const Item& it, uint64_t culture, const Game& g);
// what a meal would need that the pack does not have (empty: it can be cooked)
std::string missingFor(const Game& g, const Meal& m);
// the materials in the pack (startBuild may take up to the shell's)
void haveMaterials(const Game& g, int& timber, int& iron);
// the build price with these materials brought (the builders' wages and everything else; x priceFactor)
int buildPrice(const Game& g, int plot, Shell s, int timber, int iron);
// a crop that wants water beside it (rice): is there a water tile next to plot tile (x, y) in the window?
bool paddyOk(const Game& g, const Plot& p, int x, int y);
// buy a horse of a breed at a stablemaster (it goes to the plot's stable, or is stabled at this inn when none has
// room: the player's horse either way). site: the stable's settlement handle
// the plot whose stable has room for one more horse (-1: none; a horse is only sold to a player with one)
int horseRoomPlot(const Game& g);
bool buyHorse(Game& g, Breed b, int site, std::string& why);
// stable the horse ridden (or waiting) at an inn of `site` / take it out again (mounted at the inn's door)
bool stableAtInn(Game& g, int site, std::string& why);
int stableOwed(const Game& g);   // the inn stable's keep owed on fetching (STABLE_INN_FEE a night after the first)
// the actor of the horse waiting in the world (-1 none in play)
int horseActor(const Game& g);
// mount the waiting horse (an actor beside the player): RIDE
bool mountWaiting(Game& g, std::string& why);
// a night raid on plot p now (tests, scripts): wolves come for the pen when the player is near, else catchUp's rule
// takes one animal. Returns the wolves spawned (0: the offline rule ran)
int forceRaid(Game& g, int plot);
// the property's name ("RIVERHOLM HOMESTEAD", or Plot::name)
std::string propertyName(const Game& g, const Plot& p);

}  // namespace home
