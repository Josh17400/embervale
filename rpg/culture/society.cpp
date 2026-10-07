// The society generator (VISION_PLAN 15.14, M3b). See rpg/culture/society.h. Phase A (lead): archetype priors mutated
// by the culture's values (martial, mercantile, pious, scholarly, seafaring, expansionist, isolationist, honour) and
// seed, and the requirement tables per settlement tier. The SOCIETY/TOWNS lane owns this file and refines it.
// Pure integer maths (a splitmix stream), deterministic in the culture.
#include "rpg/culture/society.h"
#include <cstdint>
#include <vector>

namespace cult {
namespace detail {
uint64_t smix(uint64_t z);   // priors.cpp
}  // namespace detail

namespace {
using art::Building;
struct Pk {
  uint64_t s;
  explicit Pk(uint64_t seed) : s(seed ? seed : 0x5EC1E7ull) {}
  uint32_t next() { s = detail::smix(s); return (uint32_t)(s >> 32); }
  int pick(int n) { return n <= 1 ? 0 : (int)(next() % (uint32_t)n); }
  bool chance(int p256) { return (int)(next() & 255) < p256; }
};
enum V { V_MARTIAL, V_MERCANTILE, V_PIOUS, V_SCHOLARLY, V_SEAFARING, V_EXPANSIONIST, V_ISOLATIONIST, V_HONOUR };
uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }
}  // namespace

const char* governmentName(Government g) {
  static const char* n[] = {"MONARCHY", "JARLDOM", "KHANATE", "CLAN ELDERS", "THEOCRACY", "MERCHANT REPUBLIC", "HIGH COUNCIL"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Government::COUNT, "a name for every government");
  return (int)g < (int)Government::COUNT ? n[(int)g] : "RULE";
}
const char* seatName(Seat s) {
  static const char* n[] = {"CASTLE", "COURT PALACE", "GREAT HALL", "TENT COURT", "TEMPLE COMPLEX", "GUILDHALL AND EXCHANGE",
                            "COUNCIL SPIRE", "TREE PALACE", "STILT HALL"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Seat::COUNT, "a name for every seat");
  return (int)s < (int)Seat::COUNT ? n[(int)s] : "SEAT";
}
const char* gatheringName(Gathering g) {
  static const char* n[] = {"TAVERN", "MEAD HALL", "BATHHOUSE", "TEA HOUSE", "FEAST TENT", "GROVE", "PLAZA"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Gathering::COUNT, "a name for every gathering place");
  return (int)g < (int)Gathering::COUNT ? n[(int)g] : "COMMONS";
}
Building gatheringPurpose(Gathering g) {
  switch (g) {
    case Gathering::Tavern: return Building::Inn;
    case Gathering::MeadHall: case Gathering::FeastTent: return Building::MeadHall;
    case Gathering::Bathhouse: return Building::Bathhouse;
    case Gathering::TeaHouse: return Building::TeaHouse;
    default: return Building::COUNT;   // a space
  }
}

// bld::Form values (rpg/build/blueprint.h; this library sits under it, so they are spelled as numbers here):
// 0 Auto, 1 Rect, 2 Round, 3 L, 4 Courtyard, 5 Compound, 6 Tower, 7 Long, 8 Stepped, 9 Tent
namespace { enum : uint8_t { F_AUTO = 0, F_RECT, F_ROUND, F_L, F_COURT, F_COMPOUND, F_TOWER, F_LONG, F_STEPPED, F_TENT }; }

// The seat's purpose: the ruler's seat of a capital (lord false) is the PALACE purpose (throne hall, royal
// apartments: the king holds court there) built in the seat's form, except a merchant republic's, whose doge sits in
// the guildhall on the market square. A city's lord (lord true) sits in the society's own house of rule: a keep, a
// jarl's mead hall, a temple lord's temple, an elders' or speakers' council hall, a provost's guildhall.
Building seatPurpose(Seat s, bool lord) {
  switch (s) {
    case Seat::GuildExchange: return Building::Guildhall;
    case Seat::TempleComplex: return lord ? Building::Temple : Building::Palace;
    case Seat::CouncilSpire: case Seat::TreePalace: case Seat::StiltHall: return lord ? Building::CouncilHall : Building::Palace;
    case Seat::GreatHall: return lord ? Building::MeadHall : Building::Palace;
    default: return lord ? Building::Keep : Building::Palace;
  }
}
// The seat's form (bld::Form): the royal seat is one building of the seat's compound (the settlement lays out the
// rest: the court tents round the khan's great tent, the priests' houses round the stepped temple, the longhouses in
// the jarl's stockade), so it is built as that one building: a great tent, a stepped temple, a long hall, a spire...
uint8_t seatForm(Seat s, bool lord) {
  switch (s) {
    case Seat::Castle: return lord ? F_TOWER : F_AUTO;   // (the palace of a castle seat: the culture's own palace)
    case Seat::CourtPalace: return F_COURT;
    case Seat::GreatHall: return F_LONG;
    case Seat::TentCourt: return F_TENT;
    case Seat::TempleComplex: return F_STEPPED;
    case Seat::GuildExchange: return lord ? F_RECT : F_COURT;   // (the doge's guildhall round its court)
    case Seat::CouncilSpire: return F_TOWER;
    case Seat::TreePalace: return F_ROUND;
    case Seat::StiltHall: return F_LONG;
    default: return F_AUTO;
  }
}

Society societyOf(const Culture& c) {
  Society s;
  Pk p(detail::smix((uint64_t)c.seed * 0x9E3779B97F4A7C15ull ^ 0x50C1E7Aull ^ (uint64_t)c.archetype << 40));
  const uint8_t* v = c.values;
  s.lawStrict = c.customs.lawStrict;
  switch (c.archetype) {
    case Archetype::Fjordfolk:
      s.government = Government::Jarldom; s.seat = Seat::GreatHall;
      s.classes = CLS_NOBLES | CLS_WARRIORS | CLS_FREEHOLDERS | CLS_CRAFTSFOLK | CLS_FARMERS;
      s.institutions = INST_ASSEMBLY | INST_WARRIOR_LODGE | INST_PRIESTHOOD;
      s.gathering = Gathering::MeadHall; s.gathering2 = Gathering::Tavern;
      s.justice = Justice::Weregild; s.inheritance = Inheritance::Elective; s.military = Military::Huscarls;
      s.trade = TradeAttitude::Open;
      s.rulerTitle = "HIGH JARL"; s.lordTitle = "JARL"; s.seatTitle = "THE GREAT HALL";
      break;
    case Archetype::Highland:
      s.government = Government::ClanElders; s.seat = Seat::Castle;
      s.classes = CLS_NOBLES | CLS_WARRIORS | CLS_HERDERS | CLS_FARMERS | CLS_CRAFTSFOLK;
      s.institutions = INST_ELDERS | INST_ASSEMBLY | INST_PRIESTHOOD;
      s.gathering = Gathering::Tavern; s.gathering2 = Gathering::MeadHall;
      s.justice = Justice::Exile; s.inheritance = Inheritance::Tanistry; s.military = Military::Levy;
      s.trade = TradeAttitude::Guarded;
      s.rulerTitle = "HIGH CHIEF"; s.lordTitle = "CHIEF"; s.seatTitle = "THE TOWER HOUSE";
      break;
    case Archetype::Heartland:
      s.government = Government::Monarchy; s.seat = Seat::Castle;
      s.classes = CLS_NOBLES | CLS_PRIESTS | CLS_MERCHANTS | CLS_CRAFTSFOLK | CLS_FARMERS | CLS_SERFS;
      s.institutions = INST_GUILDS | INST_PRIESTHOOD | INST_COURT;
      s.gathering = Gathering::Tavern; s.gathering2 = Gathering::Tavern;
      s.justice = Justice::Stocks; s.inheritance = Inheritance::Primogeniture; s.military = Military::Knights;
      s.trade = TradeAttitude::Open;
      s.rulerTitle = "KING"; s.lordTitle = "LORD"; s.seatTitle = "THE CASTLE";
      break;
    case Archetype::Imperial:
      s.government = Government::Monarchy; s.seat = Seat::CourtPalace;
      s.classes = CLS_NOBLES | CLS_PRIESTS | CLS_MERCHANTS | CLS_CRAFTSFOLK | CLS_FARMERS | CLS_SCHOLARS | CLS_SLAVES;
      s.institutions = INST_GUILDS | INST_PRIESTHOOD | INST_BATHS | INST_COURT | INST_SCHOLARS;
      s.gathering = Gathering::Bathhouse; s.gathering2 = Gathering::Tavern;
      s.justice = Justice::Labour; s.inheritance = Inheritance::Primogeniture; s.military = Military::Legions;
      s.trade = TradeAttitude::Mercantile;
      s.rulerTitle = "EMPEROR"; s.lordTitle = "PREFECT"; s.seatTitle = "THE IMPERIAL PALACE";
      break;
    case Archetype::Dune:
      s.government = Government::Monarchy; s.seat = Seat::CourtPalace;
      s.classes = CLS_NOBLES | CLS_PRIESTS | CLS_MERCHANTS | CLS_CRAFTSFOLK | CLS_HERDERS | CLS_SCHOLARS;
      s.institutions = INST_MERCHANT_HOUSES | INST_BATHS | INST_PRIESTHOOD | INST_COURT | INST_SCHOLARS;
      s.gathering = Gathering::Bathhouse; s.gathering2 = Gathering::TeaHouse;
      s.justice = Justice::Fines; s.inheritance = Inheritance::Gavelkind; s.military = Military::Mercenaries;
      s.trade = TradeAttitude::Mercantile;
      s.rulerTitle = "SULTAN"; s.lordTitle = "EMIR"; s.seatTitle = "THE PALACE OF COURTS";
      break;
    case Archetype::Steppe:
      s.government = Government::Khanate; s.seat = Seat::TentCourt; s.nomadic = true;
      s.classes = CLS_NOBLES | CLS_WARRIORS | CLS_HERDERS | CLS_CRAFTSFOLK;
      s.institutions = INST_COURT | INST_ELDERS | INST_WARRIOR_LODGE;
      s.gathering = Gathering::FeastTent; s.gathering2 = Gathering::TeaHouse;
      s.justice = Justice::Ordeal; s.inheritance = Inheritance::Elective; s.military = Military::HorseArchers;
      s.trade = TradeAttitude::Tribute;
      s.rulerTitle = "KHAN"; s.lordTitle = "NOYAN"; s.seatTitle = "THE GREAT TENT";
      break;
    case Archetype::Marsh:
      s.government = Government::ClanElders; s.seat = Seat::StiltHall;
      s.classes = CLS_FARMERS | CLS_CRAFTSFOLK | CLS_PRIESTS | CLS_FREEHOLDERS;
      s.institutions = INST_ELDERS | INST_PRIESTHOOD | INST_ASSEMBLY;
      s.gathering = Gathering::Tavern; s.gathering2 = Gathering::MeadHall;
      s.justice = Justice::Exile; s.inheritance = Inheritance::Matrilineal; s.military = Military::Levy;
      s.trade = TradeAttitude::Guarded;
      s.rulerTitle = "ELDER MOTHER"; s.lordTitle = "ELDER"; s.seatTitle = "THE LONG HALL";
      break;
    case Archetype::Jade:
      s.government = Government::Monarchy; s.seat = Seat::CourtPalace;
      s.classes = CLS_NOBLES | CLS_SCHOLARS | CLS_FARMERS | CLS_CRAFTSFOLK | CLS_MERCHANTS;
      s.institutions = INST_COURT | INST_SCHOLARS | INST_PRIESTHOOD | INST_BATHS | INST_MONASTERY;
      s.gathering = Gathering::TeaHouse; s.gathering2 = Gathering::Bathhouse;
      s.justice = Justice::Labour; s.inheritance = Inheritance::Primogeniture; s.military = Military::Legions;
      s.trade = TradeAttitude::Guarded;
      s.rulerTitle = "CELESTIAL MONARCH"; s.lordTitle = "MAGISTRATE"; s.seatTitle = "THE JADE COURT";
      break;
    case Archetype::River:
      s.government = Government::MerchantRepublic; s.seat = Seat::GuildExchange;
      s.classes = CLS_MERCHANTS | CLS_CRAFTSFOLK | CLS_PRIESTS | CLS_FARMERS | CLS_FREEHOLDERS;
      s.institutions = INST_GUILDS | INST_MERCHANT_HOUSES | INST_PRIESTHOOD | INST_BATHS;
      s.gathering = Gathering::Tavern; s.gathering2 = Gathering::Bathhouse;
      s.justice = Justice::Fines; s.inheritance = Inheritance::Gavelkind; s.military = Military::Mercenaries;
      s.trade = TradeAttitude::Mercantile;
      s.rulerTitle = "DOGE"; s.lordTitle = "PROVOST"; s.seatTitle = "THE GUILDHALL";
      break;
    case Archetype::SunTemple:
      s.government = Government::Theocracy; s.seat = Seat::TempleComplex;
      s.classes = CLS_PRIESTS | CLS_NOBLES | CLS_WARRIORS | CLS_FARMERS | CLS_CRAFTSFOLK;
      s.institutions = INST_PRIESTHOOD | INST_COURT | INST_SCHOLARS;
      s.gathering = Gathering::Plaza; s.gathering2 = Gathering::Tavern;
      s.justice = Justice::Labour; s.inheritance = Inheritance::Merit; s.military = Military::TempleGuard;
      s.trade = TradeAttitude::Tribute;
      s.rulerTitle = "SUN PRIEST"; s.lordTitle = "TEMPLE LORD"; s.seatTitle = "THE HIGH TEMPLE";
      break;
    case Archetype::Sylvan:
      s.government = Government::HighCouncil; s.seat = Seat::TreePalace;
      s.classes = CLS_NOBLES | CLS_CRAFTSFOLK | CLS_SCHOLARS | CLS_WARRIORS;
      s.institutions = INST_ELDERS | INST_WARRIOR_LODGE | INST_SCHOLARS;
      s.gathering = Gathering::Grove; s.gathering2 = Gathering::MeadHall;
      s.justice = Justice::Exile; s.inheritance = Inheritance::Merit; s.military = Military::Wardens;
      s.trade = TradeAttitude::Closed;
      s.rulerTitle = "ELDER OF THE GROVE"; s.lordTitle = "WARDEN"; s.seatTitle = "THE TREE PALACE";
      break;
    case Archetype::Starspire:
      s.government = Government::HighCouncil; s.seat = Seat::CouncilSpire;
      s.classes = CLS_NOBLES | CLS_SCHOLARS | CLS_PRIESTS | CLS_CRAFTSFOLK;
      s.institutions = INST_COURT | INST_SCHOLARS | INST_PRIESTHOOD | INST_ASSEMBLY;
      s.gathering = Gathering::TeaHouse; s.gathering2 = Gathering::Plaza;
      s.justice = Justice::Exile; s.inheritance = Inheritance::Merit; s.military = Military::Wardens;
      s.trade = TradeAttitude::Guarded;
      s.rulerTitle = "FIRST SPEAKER"; s.lordTitle = "SPEAKER"; s.seatTitle = "THE COUNCIL SPIRE";
      break;
    default: break;
  }
  // the culture's values bend the priors: a mercantile dialect of the heartland elects its doge, a pious dune
  // dialect is ruled from the temple, a martial river people keep a standing army; isolated families cross archetypes
  // only through their values. A people whose seat is its identity (the khan's moving court, the elven councils, the
  // sun priests, the marsh elders over their water) keeps it; the others may change how they are ruled.
  const bool fixedSeat = c.archetype == Archetype::Steppe || c.archetype == Archetype::Sylvan || c.archetype == Archetype::Starspire ||
                         c.archetype == Archetype::SunTemple || c.archetype == Archetype::Marsh;
  if (!fixedSeat && c.archetype != Archetype::River && v[V_MERCANTILE] >= 200 && p.chance(96)) {
    s.government = Government::MerchantRepublic; s.seat = Seat::GuildExchange;
    s.institutions |= INST_GUILDS | INST_MERCHANT_HOUSES; s.trade = TradeAttitude::Mercantile;
    s.inheritance = Inheritance::Elective;
    s.rulerTitle = c.archetype == Archetype::Dune ? "GRAND FACTOR" : (c.archetype == Archetype::Jade ? "FIRST MERCHANT" : "DOGE");
    s.lordTitle = "PROVOST"; s.seatTitle = "THE GUILDHALL";
    if (s.military == Military::Levy || s.military == Military::Knights) s.military = Military::Mercenaries;
  } else if (!fixedSeat && v[V_PIOUS] >= 210 && p.chance(80)) {
    s.government = Government::Theocracy; s.seat = Seat::TempleComplex;
    s.institutions |= INST_PRIESTHOOD; s.military = Military::TempleGuard; s.classes |= CLS_PRIESTS;
    s.inheritance = Inheritance::Merit;
    s.rulerTitle = c.archetype == Archetype::Fjordfolk ? "HIGH GODI" : "HIGH PRIEST"; s.lordTitle = "PRELATE"; s.seatTitle = "THE HIGH TEMPLE";
  } else if (c.archetype == Archetype::River && v[V_MARTIAL] >= 200 && p.chance(110)) {
    // a martial river people crowned their condottiere: a prince in a court palace over the old republic
    s.government = Government::Monarchy; s.seat = Seat::CourtPalace;
    s.institutions |= INST_COURT; s.inheritance = Inheritance::Primogeniture; s.military = Military::Legions;
    s.rulerTitle = "PRINCE"; s.lordTitle = "PODESTA"; s.seatTitle = "THE PRINCE'S PALACE";
  } else if (c.archetype == Archetype::Highland && v[V_EXPANSIONIST] >= 190 && p.chance(110)) {
    // the clans of an expanding highland people bent the knee to one house: a king in his tower castle
    s.government = Government::Monarchy; s.inheritance = Inheritance::Primogeniture;
    s.institutions |= INST_COURT; s.rulerTitle = "HIGH KING"; s.seatTitle = "THE HIGH KING'S CASTLE";
  } else if (c.archetype == Archetype::Heartland && v[V_ISOLATIONIST] >= 190 && v[V_HONOUR] >= 150 && p.chance(110)) {
    // an inward-looking heartland held by its old warrior houses: a jarl-like elective crown and a great hall
    s.government = Government::Jarldom; s.seat = Seat::GreatHall; s.inheritance = Inheritance::Elective;
    s.institutions |= INST_ASSEMBLY | INST_WARRIOR_LODGE; s.rulerTitle = "HIGH THANE"; s.lordTitle = "THANE"; s.seatTitle = "THE MOOT HALL";
    s.gathering = Gathering::MeadHall;
  }
  if (v[V_MARTIAL] >= 200 && s.military == Military::Levy) s.military = Military::Knights;
  if (v[V_MARTIAL] >= 215 && s.military == Military::Mercenaries) s.military = Military::Legions;   // (they keep their own regiments)
  if (v[V_SCHOLARLY] >= 180) s.institutions |= INST_SCHOLARS;
  if (v[V_MERCANTILE] >= 170) s.institutions |= INST_MERCHANT_HOUSES;
  if (v[V_MERCANTILE] >= 185 && (s.classes & CLS_CRAFTSFOLK)) s.institutions |= INST_GUILDS;
  if (v[V_HONOUR] >= 200 && (s.institutions & INST_WARRIOR_LODGE) == 0 && p.chance(120)) s.institutions |= INST_WARRIOR_LODGE;
  if (v[V_PIOUS] >= 190 && p.chance(100)) s.institutions |= INST_MONASTERY;
  if (v[V_ISOLATIONIST] >= 170 && s.trade == TradeAttitude::Open) s.trade = TradeAttitude::Guarded;
  if (v[V_ISOLATIONIST] >= 215 && s.trade == TradeAttitude::Guarded) s.trade = TradeAttitude::Closed;
  if (v[V_SEAFARING] >= 190 && s.trade == TradeAttitude::Guarded && !(v[V_ISOLATIONIST] >= 170)) s.trade = TradeAttitude::Open;
  if (v[V_EXPANSIONIST] >= 200 && s.military == Military::Levy) s.military = Military::Legions;
  // the evening: the people's drink sets its second gathering place (a tea people keep tea houses, a mead people mead
  // halls), unless their society already has it
  {
    const uint8_t drink = c.customs.drink;   // 0 ale, 1 mead, 2 wine, 3 tea, 4 kumis, 5 nectar
    Gathering want = s.gathering2;
    if (drink == 3) want = Gathering::TeaHouse;
    else if (drink == 1 || drink == 4) want = s.seat == Seat::TentCourt ? Gathering::FeastTent : Gathering::MeadHall;
    if (want != s.gathering && gatheringPurpose(want) != gatheringPurpose(s.gathering) && p.chance(160)) s.gathering2 = want;
  }
  // classes the institutions imply
  if (s.institutions & INST_GUILDS) s.classes |= CLS_CRAFTSFOLK | CLS_MERCHANTS;
  if (s.institutions & INST_SCHOLARS) s.classes |= CLS_SCHOLARS;
  if (s.government == Government::Theocracy) s.classes |= CLS_PRIESTS;
  if (s.government == Government::MerchantRepublic) s.classes |= CLS_MERCHANTS;
  // the law: the customs' strictness, sharpened by rulers who rule by fear or by god
  int law = c.customs.lawStrict;
  if (s.government == Government::Theocracy || s.government == Government::Monarchy) law += 20;
  if (s.government == Government::ClanElders || s.government == Government::HighCouncil) law -= 10;
  s.lawStrict = clamp8(law + (int)(p.next() % 21) - 10);
  s.crimeTolerance = clamp8(200 - s.lawStrict + (int)(p.next() % 31) - 15);
  return s;
}

// ------------------------------------------------------------------ requirements
// The requirement tables (15.14; 15.8 scale, 15.11 village essentials). In placing order: the seat of power, then
// lodging and the gathering places, the essentials (smith, shops, faith), the institutions, the garrison. Footprints
// are the society's preferred sizes in tiles (the settlement shrinks a required one a step when the land is tight).
//   village   an inn (or tavern), a smith, a general store where trade is welcome; a theocracy's chapel
//   town      the inn, the gathering place when it is not the inn's common room, shops, the smith, the temple, the
//             guildhall of a guild people, the warrior lodge, the elders' council hall of an assembly people
//   city      the lord's seat (seatPurpose(seat, true)), two inns, both gathering places, three shops, two smiths,
//             the temple, the mage tower (an academy for scholars), the guildhall, the exchange, the lodge, the baths,
//             the garrison of a standing army
//   capital   the ruler's seat (seatPurpose(seat, false), the palace purpose in the seat's form), the lord's keep (the
//             capital's steward: the main quest's jarl holds it), all a city has, the garrison always
std::vector<BuildingReq> requiredBuildings(const Society& s, const Culture& c, SettleTier t, int archetype, uint32_t seed) {
  std::vector<BuildingReq> out;
  Pk p(detail::smix((uint64_t)seed ^ 0xB11D5EEDull));
  auto add = [&](Building b, Keeper k, Quarter q, bool req, uint8_t n, uint8_t w, uint8_t h, uint8_t form = F_AUTO, uint8_t civic = 0) {
    BuildingReq r;
    r.purpose = b; r.keeper = k; r.quarter = q; r.required = req; r.count = n; r.wTiles = w; r.hTiles = h; r.form = form; r.civic = civic;
    out.push_back(r);
  };
  const bool village = t == SettleTier::Village, town = t == SettleTier::Town, city = t >= SettleTier::City, capital = t == SettleTier::Capital;
  const Building gath = gatheringPurpose(s.gathering);
  const Building gath2 = gatheringPurpose(s.gathering2);
  const uint8_t CIV_SEAT = 1, CIV_GATHER = 2, CIV_INST = 4, CIV_SACRED = 8;   // bld::CIVIC_*
  const bool port = archetype == 3, market = archetype == 7, mining = archetype == 4, crossing = archetype == 5;   // ew::Archetype
  // ---- the seat of power first (it shapes the heart). The capital's ruler sits in the palace purpose built in the
  //      seat's form (a merchant republic's doge in the guildhall on the square), its steward / jarl in the keep (the
  //      quests' lord of the capital); a city's lord in the society's own house of rule
  if (capital) {
    const Building seat = seatPurpose(s.seat, false);
    add(seat, Keeper::Ruler, s.seat == Seat::GuildExchange ? Quarter::Market : Quarter::Noble, true, 1, seat == Building::Palace ? 15 : 11,
        seat == Building::Palace ? 7 : 5, seatForm(s.seat, false), CIV_SEAT);
    // (the steward's keep is no seat of power of its own: the ruler's seat is the capital's one CIVIC_SEAT, so game
    // code finds the throne by bldgIsRoyalSeat and the capital's lord by its keep)
    add(Building::Keep, Keeper::Lord, Quarter::Noble, true, 1, 9, 4, s.seat == Seat::Castle ? F_TOWER : F_AUTO, 0);
  } else if (city) {
    const Building seat = seatPurpose(s.seat, true);
    const uint8_t w = seat == Building::Keep ? 9 : (seat == Building::Temple ? 8 : (seat == Building::MeadHall ? 10 : 9));
    add(seat, Keeper::Lord, s.seat == Seat::GuildExchange ? Quarter::Market : (s.seat == Seat::TempleComplex ? Quarter::Sacred : Quarter::Noble),
        true, 1, w, seat == Building::Temple ? 5 : 4, seatForm(s.seat, true), CIV_SEAT);
  }
  if (s.seat == Seat::GuildExchange && city) add(Building::Exchange, Keeper::Merchant, Quarter::Market, true, 1, capital ? 8 : 7, 4, F_AUTO, CIV_INST);
  // ---- lodging and the gathering places (15.11: an inn or tavern in every village). The inn is the lodging
  //      everywhere (its innkeeper lets the rooms); a mead hall, bathhouse or tea house is kept by its hall keeper
  //      (Keeper::Merchant: they sell the food and drink), its patrons are M5's evening crowd
  add(Building::Inn, Keeper::Innkeeper, Quarter::Heart, true, 1, village ? (uint8_t)(5 + p.pick(2)) : 6, 3, F_AUTO, gath == Building::Inn ? CIV_GATHER : 0);
  if (city) add(Building::Inn, Keeper::Innkeeper, Quarter::Gate, true, 1, (uint8_t)(5 + p.pick(2)), 3);   // (the gate inn)
  if (gath != Building::Inn && gath != Building::COUNT && !village) {
    const uint8_t w = gath == Building::MeadHall ? (city ? 9 : 8) : (gath == Building::TeaHouse ? 5 : 7);
    add(gath, gath == Building::Bathhouse ? Keeper::Villager : Keeper::Merchant, Quarter::Heart, true, 1, w, gath == Building::TeaHouse ? 3 : 4, s.gathering == Gathering::FeastTent ? F_TENT : F_AUTO, CIV_GATHER);
  }
  if (gath2 != Building::Inn && gath2 != Building::COUNT && gath2 != gath && !village) {
    const uint8_t w = gath2 == Building::MeadHall ? 8 : (gath2 == Building::TeaHouse ? 5 : 7);
    add(gath2, gath2 == Building::Bathhouse ? Keeper::Villager : Keeper::Merchant, Quarter::Market, city, 1, w, gath2 == Building::TeaHouse ? 3 : 4, s.gathering2 == Gathering::FeastTent ? F_TENT : F_AUTO, CIV_GATHER);
  }
  if (!village && (port || market || crossing)) add(Building::Inn, Keeper::Innkeeper, Quarter::Gate, false, 1, 5, 3);   // (travellers' inns)
  // ---- the essentials: the smith, the shops (a closed people keeps fewer, a mercantile one more)
  add(Building::Smithy, Keeper::Smith, Quarter::Crafts, true, 1, 5, 3);
  if (city || mining) add(Building::Smithy, Keeper::Smith, Quarter::Crafts, city, 1, 5, 3);
  {
    const bool welcome = s.trade == TradeAttitude::Open || s.trade == TradeAttitude::Mercantile;
    if (village) { if (welcome || market || p.chance(100)) add(Building::Shop, Keeper::Merchant, Quarter::Market, false, 1, 4, 3); }
    else {
      add(Building::Shop, Keeper::Merchant, Quarter::Market, true, 1, 4, 3);
      if (city) add(Building::Shop, Keeper::Merchant, Quarter::Market, true, 1, 4, 3);
      const int extra = (city ? 1 : 0) + (s.trade == TradeAttitude::Mercantile ? 1 : 0) + (market ? 1 : 0) + (welcome && town && p.chance(128) ? 1 : 0) -
                        (s.trade == TradeAttitude::Closed ? 1 : 0);
      for (int k = 0; k < extra; k++) add(Building::Shop, Keeper::Merchant, k & 1 ? Quarter::Crafts : Quarter::Market, false, 1, (uint8_t)(4 + p.pick(2)), 3);
    }
  }
  // ---- faith: a temple from towns up (the chief temple is sacred); a theocracy's chapel in its villages too
  if (!village || s.government == Government::Theocracy)
    add(Building::Temple, Keeper::Priest, Quarter::Sacred, !village, 1, city ? 7 : (village ? 5 : 6), city ? 5 : (village ? 3 : 4), F_AUTO, CIV_SACRED);
  if (capital && (s.institutions & INST_PRIESTHOOD) && s.government == Government::Theocracy)
    add(Building::Temple, Keeper::Priest, Quarter::Sacred, false, 1, 6, 4);   // (a second sanctuary in a god's capital)
  // ---- institutions
  if ((s.institutions & INST_GUILDS) && !village && s.seat != Seat::GuildExchange)
    add(Building::Guildhall, Keeper::Merchant, Quarter::Crafts, city, 1, 7, 4, F_AUTO, CIV_INST);
  if ((s.institutions & INST_MERCHANT_HOUSES) && city && s.seat != Seat::GuildExchange)
    add(Building::Exchange, Keeper::Merchant, Quarter::Market, s.trade == TradeAttitude::Mercantile, 1, 6, 4, F_AUTO, CIV_INST);
  if ((s.institutions & INST_WARRIOR_LODGE) && !village)
    add(Building::Lodge, Keeper::Guard, Quarter::Gate, city, 1, 6, 4, F_AUTO, CIV_INST);
  if ((s.institutions & (INST_ELDERS | INST_ASSEMBLY)) && town && s.government != Government::Monarchy)
    add(Building::CouncilHall, Keeper::Lord, Quarter::Heart, true, 1, 7, 4, F_AUTO, CIV_INST);
  if ((s.institutions & INST_BATHS) && city && gath != Building::Bathhouse && gath2 != Building::Bathhouse)
    add(Building::Bathhouse, Keeper::Villager, Quarter::Heart, capital, 1, 7, 4, F_AUTO, CIV_INST);
  if (city) add(Building::Tower, Keeper::Mage, Quarter::Sacred, true, 1, 3, 3, F_AUTO, (s.institutions & INST_SCHOLARS) ? CIV_INST : 0);
  // ---- the garrison: barracks for standing armies (a capital's royal guard in the seat's grounds); the khan's
  //      riders in tents at the edge; huscarls live in the great hall, temple guards in the precinct, wardens in
  //      their lodge
  if (city) {
    switch (s.military) {
      case Military::Levy: case Military::Knights: case Military::Legions: case Military::Mercenaries:
        add(Building::Barracks, Keeper::Guard, Quarter::Gate, capital, 1, 7, 4); break;
      // (the riders' quarters at the edge by the horse lines: a one-storey barracks tent, its dormitory at the back)
      case Military::HorseArchers: add(Building::Barracks, Keeper::Guard, Quarter::Edge, capital, 1, 6, 4, F_TENT); break;
      default: break;
    }
  }
  (void)c;
  return out;
}

std::vector<SpaceReq> requiredSpaces(const Society& s, const Culture& c, SettleTier t, int archetype, uint32_t seed) {
  (void)c; (void)seed;
  std::vector<SpaceReq> out;
  auto add = [&](Space sp, bool req, Quarter q) { SpaceReq r; r.space = sp; r.required = req; r.quarter = q; out.push_back(r); };
  const bool city = t >= SettleTier::City;
  add(Space::MarketSquare, true, Quarter::Market);
  if (t == SettleTier::Village) add(Space::Green, true, Quarter::Heart);
  if (archetype == 3 /* ew::Archetype::Port */) add(Space::Harbour, true, Quarter::Edge);
  if (city && (s.military == Military::Legions || s.military == Military::Knights)) add(Space::ParadeGround, t == SettleTier::Capital, Quarter::Gate);
  if (s.military == Military::HorseArchers || (s.classes & CLS_HERDERS && s.nomadic)) add(Space::Corral, t != SettleTier::Village, Quarter::Edge);
  if (s.government == Government::Theocracy || s.seat == Seat::TempleComplex) add(Space::TempleCourt, t != SettleTier::Village, Quarter::Sacred);
  if ((s.institutions & INST_ASSEMBLY) || s.government == Government::Jarldom || s.government == Government::ClanElders)
    add(Space::MootRing, s.government == Government::Jarldom && t != SettleTier::Village, Quarter::Edge);
  if (s.gathering == Gathering::Grove || s.seat == Seat::TreePalace) add(Space::SacredGrove, t != SettleTier::Village, Quarter::Sacred);
  if (t == SettleTier::Capital && (s.seat == Seat::CourtPalace || s.seat == Seat::Castle || s.seat == Seat::CouncilSpire || s.seat == Seat::TreePalace))
    add(Space::Gardens, true, Quarter::Noble);
  return out;
}

}  // namespace cult
