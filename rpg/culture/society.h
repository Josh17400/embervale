// The SOCIETY generator (VISION_PLAN 15.14, M3b "Builders & Societies"): how a culture is governed and organised, and
// so which buildings and spaces its settlements must have and what its seat of power is. Pure data, deterministic in
// the culture (archetype, values, seed): integer maths only, never saved (regenerated with the world, like the
// culture). The settlement generator (rpg/world/town_*.cpp) builds what requirements() asks for; the builder
// (rpg/build/blueprint.h) decides how each piece looks from the culture's parts; M4 (kingdoms, diplomacy) and M5
// (needs-driven citizens, 15.12) read the rest (law, inheritance, military, trade attitude, gathering places).
//
// M3b phase A (lead): this header is FROZEN for the lanes; the SOCIETY/TOWNS lane owns rpg/culture/society.cpp and may
// refine the choices (never the field meanings). A lane that needs a new field stubs locally and reports it.
#pragma once
#include <cstdint>
#include <vector>
#include "rpg/art/art_building.h"
#include "rpg/culture/culture.h"

namespace cult {

// ---------------------------------------------------------------- government and the seat of power
enum class Government : uint8_t {
  Monarchy,          // a crowned king or queen over lords (feudal heartland, imperial court, the jade throne)
  Jarldom,           // a jarl and the hall-moot of free men (fjordfolk): the ruler is chosen at the assembly
  Khanate,           // a khan and a moving court of clans (steppe)
  ClanElders,        // a council of clan heads, the eldest speaks (highland, marsh)
  Theocracy,         // the high priesthood rules in the god's name (sun temples, some dune and river dialects)
  MerchantRepublic,  // the great houses elect a doge / first merchant (river cities, some dune and heartland dialects)
  HighCouncil,       // the elven high council of the eldest houses (sylvan, starspire)
  COUNT
};
const char* governmentName(Government g);   // "MONARCHY", "JARLDOM", ...

// The seat of power a capital (and, smaller, a city) is built around. art::Building::Palace is the PURPOSE of the
// royal seat; its form comes from here (seatForm): a khan's palace is a court of great tents, a theocracy's a temple
// complex, a merchant republic's a guildhall with the exchange on its square.
enum class Seat : uint8_t {
  Castle,            // a walled keep and palace (heartland, highland tower-castles)
  CourtPalace,       // a palace round courtyards: colonnades, gardens (imperial, dune, jade)
  GreatHall,         // a timber great hall in a stockade (fjordfolk jarls)
  TentCourt,         // the khan's great tent in a ring of court tents and banners (steppe; it can move)
  TempleComplex,     // the high temple: a stepped pyramid or sanctuary in a walled precinct (theocracies)
  GuildExchange,     // the guildhall and the exchange facing one square (merchant republics)
  CouncilSpire,      // the starspire high council's white spire over a council hall
  TreePalace,        // the sylvan high council's halls grown round a colossal living tree
  StiltHall,         // the marsh elders' great stilt longhouse over the water
  COUNT
};
const char* seatName(Seat s);               // "CASTLE", "TENT COURT", ...

// ---------------------------------------------------------------- classes, institutions, gathering
// Society::classes bits: who stands above whom
enum : uint16_t {
  CLS_NOBLES = 1, CLS_PRIESTS = 2, CLS_MERCHANTS = 4, CLS_WARRIORS = 8, CLS_CRAFTSFOLK = 16, CLS_FARMERS = 32,
  CLS_HERDERS = 64, CLS_SCHOLARS = 128, CLS_SERFS = 256, CLS_SLAVES = 512, CLS_FREEHOLDERS = 1024
};
// Society::institutions bits: the bodies that own buildings and run part of life
enum : uint16_t {
  INST_GUILDS = 1,         // craft guilds: a guildhall in towns and cities
  INST_PRIESTHOOD = 2,     // an organised clergy: temples everywhere, a temple quarter in cities
  INST_WARRIOR_LODGE = 4,  // sworn warrior bands: a lodge (Building::Lodge) in towns, the garrison in cities
  INST_SCHOLARS = 8,       // an academy / library / mage college: the mage tower grows into a college in cities
  INST_MERCHANT_HOUSES = 16, // trading houses: an exchange in cities, warehouses at ports
  INST_ASSEMBLY = 32,      // a moot / thing / council of free folk: a meeting place (a moot stone, a council hall)
  INST_MONASTERY = 64,     // a monastic order: retreats in the wilds (M4 places them)
  INST_BATHS = 128,        // public baths are an institution (imperial, dune, jade, river)
  INST_COURT = 256,        // a royal / khan's court with courtiers (the palace's offices)
  INST_ELDERS = 512        // clan elders who judge disputes
};
// Where people gather of an evening (what M5's "social" need looks for); the settlement builds it as a building
// purpose (gatheringPurpose) beside, or instead of, the inn.
enum class Gathering : uint8_t {
  Tavern,            // an alehouse / tavern (the inn's common room serves)
  MeadHall,          // a long feasting hall round a fire (Building::MeadHall)
  Bathhouse,         // public baths (Building::Bathhouse)
  TeaHouse,          // a tea house (Building::TeaHouse)
  FeastTent,         // the steppe's great feasting tent (a MeadHall purpose in the Tent form)
  Grove,             // an open-air ring under trees (elven): a space, not a building
  Plaza,             // the square itself: a promenade and fountain (sun temples, some dune cities): a space
  COUNT
};
const char* gatheringName(Gathering g);
// the building purpose a gathering place is built as (Building::COUNT: it is a space, not a building)
art::Building gatheringPurpose(Gathering g);

// ---------------------------------------------------------------- law, inheritance, military, trade
enum class Justice : uint8_t { Fines, Stocks, Exile, Ordeal, Weregild, Labour, COUNT };   // the usual punishment
enum class Inheritance : uint8_t { Primogeniture, Elective, Gavelkind, Matrilineal, Merit, Tanistry, COUNT };
enum class Military : uint8_t {
  Levy,              // peasant levies under lords' knights (a barracks in cities)
  Knights,           // mounted orders (a barracks and a tiltyard)
  Huscarls,          // a jarl's sworn hall-guard (they live in the great hall / a lodge)
  HorseArchers,      // the clans ride (corrals, no barracks: tents)
  Legions,           // a standing army in forts (a barracks and a parade ground)
  TempleGuard,       // warrior-priests (quartered in the temple precinct)
  Wardens,           // elven wardens of the woods (a lodge)
  Mercenaries,       // hired companies (merchant republics: a barracks by the gate)
  COUNT
};
enum class TradeAttitude : uint8_t {
  Open,              // trade is welcome: big markets, inns for foreigners
  Mercantile,        // trade is the point: an exchange, warehouses, many stalls and shops
  Guarded,           // trade under licence: tolls at the gates, a customs house, fewer foreign stalls
  Tribute,           // goods move as tribute and gifts: small markets, a great storehouse at the seat
  Closed,            // outsiders are kept out of the heart: one market by the gate
  COUNT
};

// ---------------------------------------------------------------- settlement requirements
enum class SettleTier : uint8_t { Village, Town, City, Capital, COUNT };
// a coarse "where in the settlement" hint (the settlement generator maps it onto its own districts)
enum class Quarter : uint8_t { Any, Heart, Noble, Sacred, Market, Crafts, Gate, Edge, COUNT };
// owner of a required building (the settlement maps it to its Role: Innkeeper, Priest, Merchant, Smith, Guard, Jarl,
// King, Mage, Villager)
enum class Keeper : uint8_t { None, Innkeeper, Priest, Merchant, Smith, Guard, Lord, Ruler, Mage, Villager, COUNT };

struct BuildingReq {
  art::Building purpose = art::Building::House;
  uint8_t count = 1;           // how many (cities may want 2 inns)
  bool required = true;        // false: when there is room (placed after the homes)
  Quarter quarter = Quarter::Any;
  Keeper keeper = Keeper::None;
  uint8_t wTiles = 0, hTiles = 0;   // preferred footprint (0: the settlement's usual size for the purpose)
  uint8_t form = 0;            // bld::Form the builder should use (0 Auto); e.g. the seat's TentCourt compound
  uint8_t civic = 0;           // bld::CIVIC_* bits (the seat of power, the gathering place...)
};
// open spaces a settlement must leave (the square is always there)
enum class Space : uint8_t {
  MarketSquare,      // the market (its layout stays as the owner approved it: rows facing the aisles)
  Green,             // a village green / common
  ParadeGround,      // drill ground by the barracks (legions, knights)
  TempleCourt,       // a sacred precinct before the temple (theocracies, jade)
  Corral,            // horse corrals at the edge (steppe)
  MootRing,          // an assembly ring of stones or benches (jarldoms, clan elders)
  SacredGrove,       // a ring of old trees (sylvan, druidic highland)
  Gardens,           // palace / temple gardens (court palaces)
  Harbour,           // quays (ports only; the WORLD lane decides ports)
  COUNT
};
struct SpaceReq { Space space = Space::Green; bool required = true; Quarter quarter = Quarter::Any; };

// ---------------------------------------------------------------- the society of one culture
struct Society {
  Government government = Government::Monarchy;
  Seat seat = Seat::Castle;
  uint16_t classes = 0;        // CLS_* bits
  uint16_t institutions = 0;   // INST_* bits
  Gathering gathering = Gathering::Tavern;    // the usual evening place
  Gathering gathering2 = Gathering::Tavern;   // a second one in towns and cities (== gathering: none)
  Justice justice = Justice::Fines;
  uint8_t lawStrict = 128;     // 0..255 (Customs::lawStrict, sharpened by the government)
  uint8_t crimeTolerance = 64; // 0..255: how much petty crime goes unpunished (M5 thieves, beggars)
  Inheritance inheritance = Inheritance::Primogeniture;
  Military military = Military::Levy;
  TradeAttitude trade = TradeAttitude::Open;
  bool nomadic = false;        // the court moves with the seasons (steppe): its seat may be gone next year (M4)
  // titles (upper case): the ruler, the local lord, the seat itself ("KING", "JARL", "THE GREAT TENT")
  const char* rulerTitle = "KING";
  const char* lordTitle = "LORD";
  const char* seatTitle = "THE PALACE";
};

// The society of a culture (archetype priors mutated by the culture's values and seed). Cheap; callers may cache.
Society societyOf(const Culture& c);
// What a settlement of this tier must build (beyond the homes and the M1 economy's production buildings, which the
// settlement keeps choosing): services, institutions, the gathering place(s), the lord's or the ruler's seat. In the
// order to place them (most important first). archetype: the site's ew::Archetype (port, market, mining...) as int.
std::vector<BuildingReq> requiredBuildings(const Society& s, const Culture& c, SettleTier t, int archetype, uint32_t seed);
std::vector<SpaceReq> requiredSpaces(const Society& s, const Culture& c, SettleTier t, int archetype, uint32_t seed);
// the seat's purpose and builder form (bld::Form) for a capital (lord == false) or a city's lord (lord == true)
art::Building seatPurpose(Seat s, bool lord);
uint8_t seatForm(Seat s, bool lord);

}  // namespace cult
