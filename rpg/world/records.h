// World records: the plain data the world generator and the sim share (MapKind, SiteType, Role, Bldg, Site, Kingdom,
// Spawn, Den, the WORLDGEN_* versions and the Map bit constants), plus siteTypeName and the generator constant
// bldgRiseTiles (art vocabulary only). No logic that calls into build or the painters: those helpers (bldgRequest,
// bldgBlueprint, bldgArch...) and Map are in rpg/world/map.h, World in rpg/sim/world.h; both include this.
// Any layer from world/ up may include it (SOLID 2a.3); it includes rpg/core and rpg/world only (SOLID 2c).
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include "rpg/core/common.h"
#include "rpg/core/art_ids.h"
#include "rpg/culture/style.h"   // art::ArchStyle (Bldg::arch)
#include "rpg/world/biomes.h"
#include "rpg/world/ids.h"

enum class MapKind : uint8_t { Overworld, Cave, Ruin, Interior };

// M2: Vignette = a small wayside point of interest (rpg/world/poi.h ew::VignetteKind in Site::kind: an overturned
// caravan, a hunter's camp, standing stones...); Wonder = a rare landmark of the world (ew::WonderKind in Site::kind)
enum class SiteType : uint8_t { City, Town, Village, Cave, Ruin, BanditCamp, Shrine, DragonLair, Vignette, Wonder, COUNT };

// Generator versions of the retired classic island (M2). They survive only as the interior / dungeon generator
// version a Bldg or Site carries (Bldg::genVer, Site::genVer): the endless world builds everything with
// WORLDGEN_LATEST, so genInterior runs the M0b rooms generator (genInteriorV4). The endless world's own version is
// ew::ENDLESS_GEN_VER (rpg/world/source.h).
constexpr int WORLDGEN_V1 = 1;       // the original generator: every save before SAVE_VER 2
constexpr int WORLDGEN_V2 = 2;       // + wilderness dens (Gen::dens, own "dens" stream)
constexpr int WORLDGEN_V3 = 3;       // M0: cluttered interiors (genInterior via Bldg::genVer) and the M0 building/wall
                                     // pass (city-wall joins, building footprints); every such change is gated on ver >= 3
constexpr int WORLDGEN_V4 = 4;       // M0 fix round: city walls are built before the town's buildings (final ring, gates and
                                     // side gates first; buildings, lamps and trees keep clear of the ring and every gate
                                     // approach; overland roads route through the real openings, which are paved)
constexpr int WORLDGEN_V5 = 5;       // M0 fix round 2: every road through a city wall gets a real gate (gatehouses searched
                                     // wider, a lane from each gate to the streets, street stubs outside the ring removed),
                                     // gates keep off rivers (a river crossing the ring keeps its water under a culvert arch),
                                     // buildings keep their roofs, sprites and doorsteps clear of each other, stalls keep
                                     // off facades
constexpr int WORLDGEN_V6 = 6;       // M0 fix round 3: worlds left with fewer than 8 caves get a top-up pass on its own
                                     // stream (rock edges scanned in a shuffled order, up to 10 caves); others are unchanged
constexpr int WORLDGEN_V7 = 7;       // M0b: storeys and rooms. Bldg::storeys / Bldg::hearth decided at generation (hashed on
                                     // their own "storeys" stream: no rng draw moves), 2-storey houses and shops rise a
                                     // tile higher in the V5 clearance test, and genInterior lays out rooms, partitions,
                                     // doors and real upper floors reached by stairs (genInteriorV4)
constexpr int WORLDGEN_LATEST = 7;   // what new games use; bump when generator output changes, gating the change on it

// King (M1): the ruler in a capital's palace (VISION_PLAN 15.8); the main quest's Jarl keeps a city's keep
// M2 wayside people (vignettes, rpg/world/poi.h): Hunter (a hunter's camp: sells pelts, gives a hunt), Fisher (a fishing
// hut: sells fish), Herbalist (an herb garden: sells herbs and potions), Traveller (a merchant caravan or pilgrim on the
// roads)
// M4 Banners (rpg/sim/realm.h, VISION_PLAN 4.5 / 4.6): Herald (a capital's crier: war proclamations, a fanfare; STORY
// lane), Soldier (a kingdom's man-at-arms: road patrols, siege camps, garrisons; WARDS lane), Captain (a siege camp's or
// a garrison's commander: "break the siege" / "join the assault"; WARDS lane), Refugee (driven from a burned village,
// walking to or sheltering in a refugee camp; WARDS lane). Saved only inside quest flags: append.
// M10 Beyond the Mist (VISION_PLAN 2.12, 15.4, 15.31): Harbourmaster (a harbour's office on the pier: berths, ferry
// passages, routes, the boats for sale at a town or city; SEA lane), Shipwright (the slipway: builds, repairs and
// upgrades boats), Sailor (a sailor in the sailors' tavern or on a pier: rumours of unmapped shores; the old sailor who
// starts a sea-chart chain; a lighthouse keeper), Pirate (hostile, bandit-like, at sea and in pirate coves).
enum class Role : uint8_t { Villager, Guard, Merchant, Smith, Innkeeper, Priest, Jarl, Farmer, Child, Mage, Bandit, King,
                            Hunter, Fisher, Herbalist, Traveller, Herald, Soldier, Captain, Refugee,
                            Harbourmaster, Shipwright, Sailor, Pirate,
                            Steward, Warlord, Envoy,   // (M12) the player's reeve in a held village, a warlord, a court's envoy
                            COUNT };

const char* siteTypeName(SiteType t);

// WORLDGEN_V5 placement: how many tiles a building's sprite may rise above its footprint's top row (roof, steeple,
// cone), generous on purpose. A generator constant (never measured from the art), so art changes never move buildings.
inline int bldgRiseTiles(art::Building t) {
  if (t == art::Building::Windmill) return 6;   // (M1 fixer) a tall tower mill whose sails reach over its cap
  switch (art::artBase(t)) {   // M1 economy: the production buildings rise like their frames (the windmill like a tower)
    case art::Building::Tower: case art::Building::Temple: return 5;
    case art::Building::Keep: case art::Building::Inn: return 4;
    default: return 3;
  }
}
// WORLDGEN_V7: the same with the building's storeys: a 2-storey house, stone house or shop rises like an inn.
inline int bldgRiseTiles(art::Building t, int storeys) {
  int r = bldgRiseTiles(t);
  t = art::artBase(t);
  if (storeys >= 2 && (t == art::Building::House || t == art::Building::StoneHouse || t == art::Building::Shop || t == art::Building::Farmhouse))
    r = std::max(r, 4);
  return r;
}

struct Bldg {
  ew::Gid id = 0;          // M1: stable identity (endless worlds: makeId(region, IdKind::Bldg, ...); classic: legacyBldgId)
  art::Building type = art::Building::House;
  IRect r;                 // footprint in tiles
  uint32_t roof = 0;
  uint32_t seed = 0;
  int site = -1;
  Role owner = Role::Villager;   // who lives/works here
  int genVer = WORLDGEN_LATEST;  // generator version of the world it belongs to (gates genInterior changes)
  // M0b. storeys: how many storeys the exterior shows; from WORLDGEN_V7 also how many interior floors it has (older
  // worlds: the type's classic look, art::defaultStoreys, over a single interior floor). hearth: something inside burns
  // a fire (hearth, oven, forge), so the roof may carry a chimney; without it the exterior shows none (VISION_PLAN
  // 15.7: chimney => hearth). biome: where it stands (its ArchStyle and its interior's regional style). All three are
  // set for every generator version and never feed back into generation.
  uint8_t storeys = 1;
  bool hearth = true;
  Biome biome = Biome::Plains;
  // M1 kingdom identity: the ruling kingdom's banner (rgba; banner == 0: none). Set by the settlement generator; keeps,
  // palaces, barracks, gatehouses and inns may fly it (the painter decides where, BuildingFacts carries it)
  uint32_t banner = 0, banner2 = 0;
  uint8_t emblem = 0;
  // M1: how urban the place it stands in is (art::urbanize): 0 countryside / village, 1 town, 2 city, 3 a capital
  uint8_t urban = 0;
  // M1 economy: type-specific detail the exterior shows (art::BuildingFacts::variant; the watermill: bit 0 = its wheel
  // on the west side, where its river runs)
  uint8_t variant = 0;
  // M4 Banners (VISION_PLAN 4.5): the war's mark on it, an overlay set at runtime from the realm's SettlementState
  // (WARDS lane, never saved, never generated): 0 whole, 1 scorched (soot, a singed roof), 2 burned out (a roof open to
  // the rafters, black walls: cannot be entered, its people absent), 3 being rebuilt (scaffolding). bldgFacts passes it
  // to the painter (BuildingFacts::charred; 0 keeps every key and pixel as before).
  uint8_t charred = 0;
  // M3 culture engine: the building's architecture, decided by the settlement generator from its culture
  // (cult::buildingArch: the family's style varied per building, VISION_PLAN 5.7). styled == false (buildings made
  // before M3 code paths set it, wayside huts...): bldgArch falls back to the biome style. Never saved (regenerated).
  art::ArchStyle arch;
  bool styled = false;
  // M3b builder (VISION_PLAN 15.14): what the settlement generator asked the builder for, beyond the type, footprint,
  // style and facts: wealth 0 poor .. 3 rich (cult::buildingArch's; size and finery), the form (bld::Form; 0 Auto: the
  // builder picks from the culture's parts), its civic role (bld::CIVIC_* bits) and, for the seat of power, the
  // society's seat (cult::Seat + 1). Never saved (regenerated); bldgBlueprint designs the building from these.
  uint8_t wealth = 1;
  uint8_t form = 0;
  uint8_t civic = 0;
  uint8_t seat = 0;
  // (owner 2026-10-06) the open front (bld::openFront): an open building has no door, its front row's walk-in bays are
  // the entrance. A cache filled by bldgOpenFront from the blueprint (never saved): entry tiles (bit i: column r.x + i
  // of the front row r.y + r.h - 1; the door column is always one), the pillars standing on them (px from the
  // footprint's left edge) and the open span between the outer pillars.
  struct Open {
    uint64_t key = 0;                    // bldgOpenKey of the inputs it was made from (0: not made)
    uint32_t mask = 0;
    bool open = false;                   // an open entrance (no door)
    bool raised = false;                 // stilts / a high plinth: entered at the door column only
    std::array<uint16_t, 32> solid{};    // per entry column: the px columns its pillars (and the wall beyond) block
  };
  mutable Open open;
  // M7 Home (rpg/sim/home.h): 1 a house the player built on a lot (home::houseBldg; added to over.bldgs at runtime by
  // home::stampWindow, outside every site's bldgFirst range, Bldg::site -1): genInterior lays out its shell's rooms
  // (stairs, doors, windows, the style's wall dressing) WITHOUT furniture, the player's own is stamped by the HOMESTEAD
  // lane; 2 a generated house the player bought (furnished as generated until rearranged); 3 a player's house under
  // construction (the view draws art::scaffoldSprite on its footprint instead of the building; it cannot be entered).
  // Never saved (the home block is).
  uint8_t home = 0;
  // M7b Cities (VISION_PLAN 15.24): a row house's party walls (bld::ROW_* bits). ROW_W: the footprint abuts its western
  // neighbour's (same front row, shared wall: no gap, no side wall of its own shown there); ROW_E likewise to the east;
  // ROW_YARD: a walled back yard behind it (the rows above the footprint, kept free by the generator). The
  // settlement generator sets it (cities; never a village); bldgRequest passes it to the builder (bld::Request::row),
  // which drops the shared side walls and runs the eaves, ridge and frontage on as one terrace. (M7c) ROW_SIDE_W /
  // ROW_SIDE_E / ROW_BACK: a side or the back looks onto a street (rpg/build/blueprint.h). Never saved.
  uint8_t row = 0;
  int floors() const { return genVer >= WORLDGEN_V7 ? std::max(1, (int)storeys) : 1; }
  int doorX() const { return r.x + r.w / 2; }
  int doorY() const { return r.y + r.h - 1; }
};

struct Site {
  ew::Gid id = 0;     // M1: stable identity (endless worlds: the SitePlan id; classic: legacySiteId(index))
  SiteType type = SiteType::Village;
  std::string name;
  IRect r;            // area in tiles (overworld)
  int ex = 0, ey = 0; // entrance / centre tile (caves: the tile in front of the entrance)
  int level = 1;      // difficulty
  bool discovered = false;
  bool cleared = false;   // dungeon boss / camp chief dead
  int bldgFirst = 0, bldgCount = 0;
  uint32_t seed = 0;
  bool mainQuest = false; // one of the ancient ruins holding an Ember Shard
  art::Monster theme = art::Monster::Spider;   // dungeon inhabitants
  int genVer = WORLDGEN_LATEST;  // generator version of the world it belongs to (gates genCave/genRuin changes)
  // M1 kingdom identity (VISION_PLAN 15.8, M4 groundwork): which kingdom holds it (World::kingdoms index, -1 none /
  // wildlands) and whether it is that kingdom's capital (the king's palace stands there)
  // M4 Banners: `kingdom` is the CURRENT owner (the realm sim may change it: a conquest, a treaty, a fall; World::
  // setSiteOwner keeps the banners in step), homeKingdom the genesis owner from the generator's plan (never changes).
  // Readers that show who rules (banners, tabards, the HUD, the map) read kingdom; history and lore read homeKingdom.
  int kingdom = -1;
  int homeKingdom = -1;
  bool capital = false;
  bool start = false;            // M1: the start village of an endless world
  uint8_t archetype = 0;         // M1: ew::Archetype (fishing, mining, farming...; rpg/world/source.h)
  // M1 economy (rpg/world/economy.h): what the settlement lives from (ew::Specialty), and the goods it makes and must
  // buy (ew::goodBit masks), for the economy (M4) and crafting (M6)
  uint8_t special = 0;
  uint32_t produces = 0, needs = 0;
  // M2: the template of a Vignette (ew::VignetteKind) or a Wonder (ew::WonderKind); 0 for every other type
  uint8_t kind = 0;
  // M2 rumours (VISION_PLAN 2.11): heard of but not found yet (an innkeeper's directions, a grave's inscription). The
  // map shows it as a "?" near where it lies; finding it sets discovered. Saved with the site flags.
  bool rumoured = false;
  // M3: its culture (cult::CultureId; ew::SitePlan::culture): World::cultureOf(site) gives the Culture
  uint64_t culture = 0;
  bool settlement() const { return type == SiteType::City || type == SiteType::Town || type == SiteType::Village; }
};

// M1 (VISION_PLAN 15.8, groundwork for M4): a kingdom. Every settlement records its kingdom; the colours fly on its
// banners, gates, keeps and signs, the name label and the world map show it.
struct Kingdom {
  ew::Gid id = 0;
  std::string name;              // "ASHMARK" (the HUD says "KINGDOM OF ASHMARK")
  uint32_t color = 0;            // banner field (rgba)
  uint32_t color2 = 0;           // banner charge / trim (rgba)
  uint8_t emblem = 0;            // the charge painted on the banner (art decides what each index looks like)
  ew::Gid capitalId = 0;         // its capital's Site id (0: none known yet)
  int32_t gx = 0, gy = 0;        // global tile of its seat (the capital, or the kingdom cell's centre)
  uint64_t culture = 0;          // M3: its culture (a dialect; World::cultureOfKingdom)
  // (fixer M4 r3) the colour its guards' tabards wear: the field, unless the field is pale (white, silver, cream: it
  // reads as bare mail on a phone) and the charge is the stronger colour
  uint32_t tabard() const {
    auto sat = [](uint32_t c) { const int r = (int)(c & 255), g = (int)((c >> 8) & 255), b = (int)((c >> 16) & 255);
                                const int mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
                                return mx - mn; };
    auto lum = [](uint32_t c) { return (int)(((c & 255) * 3 + ((c >> 8) & 255) * 6 + ((c >> 16) & 255)) / 10); };
    return (color2 && sat(color) < 60 && lum(color) > 150 && sat(color2) > sat(color)) ? color2 : color;
  }
};

// Something to populate when a map becomes active (deterministic per map).
struct Spawn {
  int x = 0, y = 0;
  bool npc = false;
  art::Monster mon = art::Monster::Wolf;
  Role role = Role::Villager;
  bool boss = false;
  bool bandit = false;   // human enemy (mon ignored)
  int site = -1;
  int slot = 0;          // stable identity within the map (names/looks/quests derive from it)
};

// A wilderness den (WORLDGEN_V2+): the home of a wild pack. Its props (bones, boulders, a chest...) are on the
// overworld prop layer; the pack spawns here when the player comes near and walks back here when it loses them.
// Index = identity (Game::killedSlots[-1] records cleared dens by index).
struct Den {
  ew::Gid id = 0;                          // M1: stable identity (endless worlds; classic dens keep their index)
  int x = 0, y = 0;                        // centre tile
  art::Monster mon = art::Monster::Wolf;   // who lives here
  uint8_t pack = 3;                        // members at full strength (trimmed near the start village)
};

// The Map bit constants (Map derives from MapBits, so Map::HEIGHT_CLIFF, Map::PAVE_MARK... read as before). The
// layers they mark are documented on Map's members in rpg/sim/world.h (blend: the PAVE / BOARDWALK / POOL marks;
// height: the HEIGHT_* bits). An empty base: Map's layout is unchanged (static_asserts there).
struct MapBits {
  static constexpr uint8_t PAVE_MARK = 0xF0, BOARDWALK_MARK = 0xE0, POOL_MARK = 0xD0;
  static constexpr uint8_t HEIGHT_LEVEL = 7, HEIGHT_CLIFF = 8, HEIGHT_RAMP = 16;
  static constexpr uint8_t HEIGHT_SLOPE = 32, HEIGHT_FALL = 64, HEIGHT_STAIR = 128;   // M11 (above)
  static constexpr uint8_t HEIGHT_STEP = HEIGHT_CLIFF | HEIGHT_RAMP | HEIGHT_SLOPE;      // the lower side of any step
};
