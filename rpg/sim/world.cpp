// World data shared by every map: names (towns, people, dungeons), biome / building / site type names, the generator
// sub-seed helper and the WORLDGEN_V7 storey and hearth decisions. M2 retired the classic 448x448 island generator
// (VISION_PLAN 15.10): every game, test and script runs on the endless mainland (rpg/world, rpg/sim/world_endless.cpp).
#include "rpg/sim/world.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

const char* biomeName(Biome b) {
  static const char* n[] = {"OCEAN", "COAST", "PLAINS", "FOREST", "AUTUMN WOODS", "TAIGA", "FROSTLANDS", "MARSH", "DUNES", "MOUNTAINS"};
  return n[(int)b];
}
const char* bldgTypeName(art::Building t) {
  static const char* n[] = {"HOUSE", "HOUSE", "INN", "SMITHY", "GENERAL GOODS", "TEMPLE", "THE KEEP", "MAGE TOWER", "FARMHOUSE", "HUT",
                            "THE PALACE", "BARRACKS", "WINDMILL", "WATERMILL", "GRANARY", "BAKERY", "BUTCHER", "TANNERY",
                            "FISHMONGER", "SMELTER", "SAWMILL", "WEAVER"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)art::Building::COUNT, "a name for every building type");
  int i = (int)t;
  return i >= 0 && i < (int)art::Building::COUNT ? n[i] : "HALL";
}
const char* siteTypeName(SiteType t) {
  static const char* n[] = {"CITY", "TOWN", "VILLAGE", "CAVE", "ANCIENT RUIN", "BANDIT CAMP", "SHRINE", "DRAGON LAIR", "LANDMARK", "WONDER"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)SiteType::COUNT, "a name for every site type");
  int i = (int)t;
  return i >= 0 && i < (int)SiteType::COUNT ? n[i] : "PLACE";
}

// ------------------------------------------------------------------ names
namespace {
const char* kPre[] = {"RIVER", "WHITE", "FROST", "STONE", "IRON", "WIND", "RAVEN", "OAK", "ELK", "WOLF", "DAWN", "MOSS",
                      "AMBER", "SALT", "HOLLOW", "MIST", "ASH", "SILVER", "THORN", "BRIGHT", "HAWK", "ELDER", "COLD", "RED"};
const char* kSuf[] = {"WOOD", "RUN", "HOLD", "FELL", "MOOR", "STEAD", "HAVEN", "FORD", "VALE", "MERE", "REACH", "WATCH",
                      "BROOK", "GATE", "HELM", "SHORE", "CREST", "DALE", "HEIM", "BURG"};
const char* kSyl1[] = {"BJ", "ER", "UL", "SV", "HR", "TH", "AL", "IN", "GU", "SI", "VI", "KA", "LY", "MA", "DA", "FRI", "YR", "HE", "OL", "AR"};
const char* kSyl2[] = {"OR", "AN", "UN", "IL", "AL", "IR", "EN", "OL", "ID", "UR", "EG", "ON", "ART", "ULF", "GRIM", "VAR", "MUND", "BERT", "STEN", "RIK"};
const char* kFem[] = {"A", "IA", "RA", "DIS", "HILD", "WYN", "LA", "GRID", "UN", "VEIG", "ETTE", "INA"};
const char* kMale[] = {"", "", "OR", "IN", "AR", "ULF", "ER", "ON", "MAR", "LEIF"};
}  // namespace

std::string makeTownName(Rng& r) {
  std::string s = kPre[r.irange(24)];
  s += kSuf[r.irange(20)];
  return s;
}
std::string makePersonName(Rng& r, bool female) {
  std::string s = kSyl1[r.irange(20)];
  s += kSyl2[r.irange(20)];
  s += female ? kFem[r.irange(12)] : kMale[r.irange(10)];
  std::string out;
  out += s[0];
  for (size_t i = 1; i < s.size(); i++) if (!(s[i] == s[i - 1] && (s[i] == 'A' || s[i] == 'I'))) out += s[i];
  return out;
}
std::string makeDungeonName(Rng& r, SiteType t, Biome b) {
  static const char* adj[] = {"BLEAK", "HOWLING", "SUNDERED", "DRAUGR", "BLACK", "FROZEN", "WEEPING", "BROKEN", "SILENT", "BLOOD", "SHADOW", "BONE", "GLOOM", "EMBER", "WRAITH", "HOLLOW"};
  static const char* caveN[] = {"CAVE", "GROTTO", "DEN", "HOLLOW", "CAVERN", "WARREN", "MINE", "DEEP"};
  static const char* ruinN[] = {"BARROW", "CRYPT", "SANCTUM", "TOMB", "VAULT", "HALLS", "SEPULCHER", "SPIRE"};
  static const char* campN[] = {"CAMP", "HIDEOUT", "REDOUBT", "LOOKOUT", "STOCKADE", "OUTPOST"};
  std::string a = adj[r.irange(16)];
  // "FROZEN" belongs in the cold north: elsewhere pick again
  bool cold = b == Biome::Snow || b == Biome::Taiga;
  for (int k = 0; k < 6 && a == "FROZEN" && !cold; k++) a = adj[r.irange(16)];
  if (a == "FROZEN" && !cold) a = "SILENT";
  if (t == SiteType::Cave) return a + " " + caveN[r.irange(8)];
  if (t == SiteType::Ruin) return a + " " + ruinN[r.irange(8)];
  if (t == SiteType::BanditCamp) return a + " " + campN[r.irange(6)];
  if (t == SiteType::Shrine) { static const char* g[] = {"SHRINE OF SOLMIR", "SHRINE OF VEYNA", "SHRINE OF HALDRUN", "SHRINE OF ORISSA", "SHRINE OF KEVRAN", "SHRINE OF ILMATH", "SHRINE OF BRANNOCK", "SHRINE OF ESKARA"}; return g[r.irange(8)]; }
  // the dragon's peak: a name of its own in every world
  static const char* peak[] = {"SKYFANG", "ASHCROWN", "CINDERHORN", "WYRMSPIRE", "STORMTOOTH", "EMBERCREST", "GREYFANG", "DRAKEHOLM"};
  static const char* kind[] = {"PEAK", "SPIRE", "CRAG", "PEAK"};
  return std::string(peak[r.irange(8)]) + " " + kind[r.irange(4)];
}

uint64_t genSubSeed(uint64_t seed, const char* feature) {
  uint64_t h = 1469598103934665603ull ^ (seed * 0x9E3779B97F4A7C15ull);
  for (const char* c = feature; *c; c++) { h ^= (uint8_t)*c; h *= 1099511628211ull; }
  h ^= h >> 33; h *= 0xFF51AFD7ED558CCDull; h ^= h >> 33;
  return h;
}

// ---- WORLDGEN_V7 (M0b): storeys and hearths. h is a per-building hash; the answers must never change for V7.
int bldgStoreysV7(art::Building t, int wTiles, int hTiles, uint32_t h) {
  (void)hTiles;
  float f = (h >> 8) * (1.0f / 16777216.0f);
  switch (t) {
    case art::Building::Inn: case art::Building::Keep: return 2;   // the public room below, rented rooms / quarters above
    case art::Building::Tower: return 3;                           // workroom, library, the mage's chamber at the top
    case art::Building::House: return wTiles >= 5 ? (f < 0.65f ? 2 : 1) : (wTiles >= 4 ? (f < 0.4f ? 2 : 1) : 1);
    case art::Building::StoneHouse: return wTiles >= 4 ? (f < 0.6f ? 2 : 1) : (f < 0.3f ? 2 : 1);   // narrow town houses too
    case art::Building::Shop: return wTiles >= 4 && f < 0.7f ? 2 : 1;   // the shopkeeper lives above the shop
    // M1 economy: the trades' shop fronts are shops (the family above in the wider ones); the windmill's tower holds
    // the millstone loft over the stone floor
    case art::Building::Bakery: case art::Building::Butcher: case art::Building::Fishmonger: case art::Building::Weaver:
      return wTiles >= 4 && f < 0.6f ? 2 : 1;
    case art::Building::Windmill: return 2;
    default: return 1;   // smithy, temple, farmhouse, hut, watermill, granary, tannery, smelter, sawmill
  }
}
bool bldgHearthV7(art::Building t, int storeys, uint32_t h) {
  switch (t) {
    case art::Building::Temple: case art::Building::Tower: return false;   // braziers and a cauldron, no chimney
    case art::Building::Shop: return storeys >= 2 || (h >> 8) % 100 < 40;   // living quarters cook; a lock-up shop may not
    case art::Building::Butcher: case art::Building::Fishmonger: case art::Building::Weaver: return storeys >= 2 || (h >> 8) % 100 < 50;
    // M1 economy: mills, the granary and the sawmill burn nothing (flour dust, grain, sawdust); the bakery's ovens, the
    // smelter's furnace and the tannery's vats do
    case art::Building::Windmill: case art::Building::Watermill: case art::Building::Granary: case art::Building::Sawmill: return false;
    default: return true;   // homes, the inn's kitchen, the smithy's forge, the keep's great hearth
  }
}
