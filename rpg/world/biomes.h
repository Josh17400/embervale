// M3c "Wildlands" (VISION_PLAN 15.15, 15.16): the biomes of the world. Shared contract, frozen after M3c phase A (lead,
// 2026-10-06); only the WORLD lane edits it afterwards (other lanes keep a local override table and report).
//
// Two layers per overworld tile:
//   Biome  the FAMILY (the 10 kinds every pre-M3c reader knows: Ocean, Beach, Plains, Forest, Autumn, Taiga, Snow, Swamp,
//          Desert, Mountain). Walkability rules, legacy flora fallbacks, snowlines, rock palettes, habitability and every
//          `b == Biome::Forest` test keep reading the family, so nothing that only knows families changes behaviour.
//   Eco    the BIOME PROPER (46 kinds below): what the land is (savanna, birch wood, peat bog, badlands, ash fields...).
//          Its family is ecoInfo(e).family, always equal to the tile's Biome. Readers that want the fine kind (the
//          terrain painter, flora, wildlife, weather, ambience, music, the maps, the HUD name, village trades, M6
//          resources) read Map::eco / ChunkData::eco / MacroSample::eco.
// Blends (M2 ecotones, VISION_PLAN 11.6): Map::blend keeps its meaning (bits 0-3 the neighbouring FAMILY, bits 4-7 the
// weight 1..8); Map::ecoNb holds the neighbouring ECO the same tile blends toward (with the same weight). A tile whose
// blend weight is 0 has ecoNb == its own eco. Two ecos of one family may blend too (blend's family nibble then equals
// the tile's own family; old readers see a same-family blend and draw their own ground, which is harmless).
//
// Determinism: the eco is chosen by the WORLD lane's classifier (rpg/world/macro.cpp ecoFor) from the climate fields
// (temperature, moisture, elevation and relief level, ridge, coast, geology, rare-noise fields, dragon lairs, culture
// for the elven silverwood) in integer / Q16 maths only. The tables here are data; nothing here is random.
#pragma once
#include <cstdint>
#include <cstring>

// Ground terrain. Order matters for rendering blend priority (higher index draws over lower at edges).
// (Moved here from rpg/sim/world.h in M3c phase A so the eco table can name its ground; world.h includes this header.)
enum class Ground : uint8_t {
  DeepWater, Water, Sand, Swamp, Grass, Meadow, ForestFloor, Autumn, Tundra, Snow, Dirt, Farmland, Road, Plaza,
  Rock,         // mountain, solid
  CaveFloor, CaveWall, WoodFloor, StoneFloor, InteriorWall, Bridge, Lava, Ice, Void,
  COUNT
};
inline bool groundSolid(Ground g) {
  return g == Ground::DeepWater || g == Ground::Water || g == Ground::Rock || g == Ground::CaveWall ||
         g == Ground::InteriorWall || g == Ground::Lava || g == Ground::Void;
}
inline bool groundWater(Ground g) { return g == Ground::DeepWater || g == Ground::Water; }

// The biome family (see above). Order is frozen (saved nowhere, but culture.h homeBiome and style.h archForBiome use the
// integer values).
enum class Biome : uint8_t { Ocean, Beach, Plains, Forest, Autumn, Taiga, Snow, Swamp, Desert, Mountain, COUNT };
const char* biomeName(Biome b);   // rpg/sim/world.cpp (the family's name)

// The biome proper. Append only (tests, goldens and the view's caches key on the values).
enum class Eco : uint8_t {
  Ocean,                                                                    // open sea
  // coast (family Beach)
  SandBeach, Shingle, CoralCoast, SeaCliffs,
  // grasslands (family Plains)
  Meadow, FlowerMeadow, Prairie, Steppe, Savanna, Heath, ChalkDowns, AlpineMeadow, StonePlains,
  // woods (family Forest; AutumnWood family Autumn)
  MixedForest, BirchWood, GiantForest, DarkForest, BlossomGrove, BambooForest, Jungle, MushroomForest, Silverwood, AutumnWood,
  // cold (Taiga / Snow families)
  Taiga, TaigaBog, SnowField, Tundra, Glacier, FrozenLakes,
  // wet (Swamp family; LakeDistrict family Plains)
  ReedMarsh, PeatBog, Mangrove, FloodedForest, LakeDistrict,
  // dry (Desert family)
  Dunes, StonyDesert, Badlands, SaltFlats, Scrubland, Oasis,
  // high rock (Mountain family)
  Mountain,
  // rare, wondrous (Desert / Plains families)
  AshFields, CrystalBarrens, PetrifiedForest, Blight,
  COUNT
};

// ---- presentation profiles (the LIFE lane renders them; a lane may refine them in its own table)
// the weather a biome tends to: what falls, how often, and its fog
enum class Sky : uint8_t {
  Temperate,   // showers now and then, clear days
  Showery,     // frequent rain, low cloud
  Misty,       // fog banks, drizzle (marsh, bog, moor, flooded wood)
  Dry,         // rare rain, heat shimmer
  Arid,        // no rain, dust devils and sand storms
  Snowy,       // snow showers
  Blizzard,    // driving snow, white-outs (glacier, high snowfields)
  Monsoon,     // warm downpours, steam (jungle, mangrove)
  Ashfall,     // drifting ash and embers, a red-brown haze
  Eerie,       // motes, spores, glints or a sickly haze (mushroom wood, crystal barrens, silverwood, blight)
  COUNT
};
// the ambient sound bed (engine/audio.h Audio::setAmbient takes the value)
enum class Ambience : uint8_t {
  Breeze,      // wind over open grass, crickets at night
  Meadow,      // skylarks, bees, crickets
  Woods,       // songbirds, rustling leaves; owls at night
  DeepWoods,   // creaking trunks, distant calls, dripping
  Jungle,      // insects, frogs, exotic birds, monkeys
  Marsh,       // frogs, reed warblers, bubbling
  Surf,        // waves on sand, gulls
  Cliffs,      // wind, crashing surf below, sea birds
  DesertWind,  // dry gusts, hissing sand
  ColdWind,    // howling wind, creaking ice
  Volcanic,    // deep rumble, hissing vents, crackling
  Crystal,     // a faint glassy hum and chimes
  Blight,      // a low drone, flies, near silence
  Bamboo,      // hollow knocks, rustling stems, water
  Mystic,      // shimmering tones, soft wind chimes
  COUNT
};
// the wilderness music's mood (Audio::setMood; the culture's MusicStyle still sets scale and instruments)
enum class Mood : uint8_t { Pastoral, Woodland, Deep, Exotic, Wetland, Coastal, Arid, Frozen, Highland, Wondrous, Ominous, COUNT };

// ---- resources (data for M6 crafting, VISION_PLAN 15.11: production chains shared with the village economy)
enum : uint16_t {   // herbs
  HB_HEALWORT = 1 << 0, HB_MOONPETAL = 1 << 1, HB_BLOODMOSS = 1 << 2, HB_FROSTCAP = 1 << 3, HB_SUNBLOOM = 1 << 4,
  HB_MARSHROOT = 1 << 5, HB_SILVERLEAF = 1 << 6, HB_EMBERWEED = 1 << 7, HB_GLOWCAP = 1 << 8, HB_SALTWORT = 1 << 9,
  HB_HEATHER = 1 << 10, HB_BLOSSOM = 1 << 11, HB_THISTLE = 1 << 12, HB_KELP = 1 << 13, HB_CRYSTALBLOOM = 1 << 14,
  HB_NIGHTSHADE = 1 << 15,
};
enum : uint16_t {   // hides and animal goods
  HD_DEER = 1 << 0, HD_WOLF = 1 << 1, HD_BEAR = 1 << 2, HD_BOAR = 1 << 3, HD_FUR = 1 << 4, HD_SCALE = 1 << 5,
  HD_CHITIN = 1 << 6, HD_GOAT = 1 << 7, HD_SPOTTED = 1 << 8, HD_SEAL = 1 << 9, HD_FEATHER = 1 << 10, HD_SILK = 1 << 11,
};
enum : uint16_t {   // timber
  TB_OAK = 1 << 0, TB_PINE = 1 << 1, TB_BIRCH = 1 << 2, TB_REDWOOD = 1 << 3, TB_EBONY = 1 << 4, TB_CHERRY = 1 << 5,
  TB_BAMBOO = 1 << 6, TB_MAHOGANY = 1 << 7, TB_SILVER = 1 << 8, TB_MANGROVE = 1 << 9, TB_DRIFT = 1 << 10,
  TB_ACACIA = 1 << 11, TB_LARCH = 1 << 12, TB_CYPRESS = 1 << 13, TB_PETRIFIED = 1 << 14, TB_MAPLE = 1 << 15,
};
enum : uint16_t {   // minerals and other ground goods (ores themselves come from the province geology, geology.h)
  MN_SALT = 1 << 0, MN_CLAY = 1 << 1, MN_PEAT = 1 << 2, MN_FLINT = 1 << 3, MN_CHALK = 1 << 4, MN_OBSIDIAN = 1 << 5,
  MN_SULFUR = 1 << 6, MN_CRYSTAL = 1 << 7, MN_AMBER = 1 << 8, MN_PEARL = 1 << 9, MN_GLASSSAND = 1 << 10, MN_ICE = 1 << 11,
  MN_CORAL = 1 << 12, MN_RESIN = 1 << 13, MN_BONE = 1 << 14,
};
// ore bias on top of the province geology (bit = 1 << ew::Ore index: Copper, Tin, Iron, Coal, Silver, Rare)
enum : uint8_t { OB_COPPER = 1, OB_TIN = 2, OB_IRON = 4, OB_COAL = 8, OB_SILVER = 16, OB_RARE = 32 };
// the trades a settlement in this biome leans to (the WORLD lane maps them to ew::Specialty: fishing on coasts, mining
// in badlands and mountains, herding on steppe and prairie, lumber in big forests...)
enum : uint8_t { TR_FARM = 1, TR_FISH = 2, TR_MINE = 4, TR_LUMBER = 8, TR_HERD = 16, TR_HUNT = 32 };

// EcoInfo::flags
enum : uint16_t {
  EF_WOODED = 1 << 0,    // trees are its main flora (rpg_test --forest covers it: no two solid trees touching)
  EF_ROCKY = 1 << 1,     // solid rocks / rock spires are a main flora (rpg_test --forest covers it too)
  EF_WET = 1 << 2,       // standing water, mud, reeds
  EF_COLD = 1 << 3,      // snow / frost (cold palettes, snow on props, Sky::Snowy or Blizzard)
  EF_HOT = 1 << 4,       // heat (dry palettes, heat shimmer)
  EF_COAST = 1 << 5,     // lies along the sea
  EF_OPEN = 1 << 6,      // open country (grassland, steppe, tundra, flats): long sight lines, few trees
  EF_RARE = 1 << 7,      // rare by design (wondrous, magical): the classifier keeps it to small, far-apart patches
  EF_MAGIC = 1 << 8,     // magical (glow at night, Sky::Eerie, Ambience::Mystic / Crystal)
  EF_HOSTILE = 1 << 9,   // poor land: no villages (habit 0..20), more dangerous spawns
};

struct EcoInfo {
  const char* name;      // HUD / map name (upper case, <= 22 chars)
  const char* key;       // script / test name (lower case, no spaces): "gotoeco savanna"
  Biome family;          // ALWAYS the Biome the tile carries
  Ground ground;         // its base natural ground (the WORLD lane's groundFor; the painter keys its look on the eco)
  uint8_t r, g, b;       // world-map / minimap colour (and the map legend)
  uint16_t flags;        // EF_*
  uint8_t habit;         // habitability 0..100 (settlement placement multiplier; 0: never settled)
  uint8_t trades;        // TR_* the local villages lean to
  Sky sky;
  Ambience amb;
  Mood mood;
  uint16_t herbs, hides, timber, minerals;   // HB_*, HD_*, TB_*, MN_*
  uint8_t ores;          // OB_*: ores this land favours beyond its province geology
};

// The table (index = Eco). Colours are the M3c defaults the LIFE lane's maps start from. (M3c fixer round 2, review:
// "legend swatches for several biome pairs cannot be told apart on the phone": spread apart so every pair of map
// colours is at least ~31 apart in RGB, each kept near its old hue) (M3c fixer round 3: bamboo a bright lime apart from
// the meadows, the prairie a mustard gold, the savanna a tawny orange, the steppe a grey khaki)
inline const EcoInfo& ecoInfo(Eco e) {
  static const EcoInfo T[] = {
      // name                    key              family          ground               r    g    b    flags                                   habit trades                 sky              amb                    mood              herbs                                  hides                         timber                    minerals                      ores
      {"OPEN SEA",               "ocean",         Biome::Ocean,   Ground::Water,        52,  92, 148, 0,                                       0,  TR_FISH,               Sky::Showery,    Ambience::Surf,        Mood::Coastal,    HB_KELP,                               HD_SEAL,                      0,                        MN_PEARL,                     0},
      {"SANDY SHORE",            "sandbeach",     Biome::Beach,   Ground::Sand,       226, 210, 161, EF_COAST | EF_OPEN,                      55, TR_FISH,               Sky::Temperate,  Ambience::Surf,        Mood::Coastal,    HB_KELP | HB_SALTWORT,                 HD_SEAL | HD_FEATHER,         TB_DRIFT,                 MN_GLASSSAND | MN_PEARL,      0},
      {"SHINGLE BEACH",          "shingle",       Biome::Beach,   Ground::Sand,       169, 159, 153, EF_COAST | EF_OPEN | EF_ROCKY,           45, TR_FISH,               Sky::Showery,    Ambience::Surf,        Mood::Coastal,    HB_KELP | HB_SALTWORT,                 HD_SEAL,                      TB_DRIFT,                 MN_FLINT,                     0},
      {"CORAL STRAND",           "coralcoast",    Biome::Beach,   Ground::Sand,       250, 245, 212, EF_COAST | EF_HOT | EF_OPEN,             60, TR_FISH,               Sky::Dry,        Ambience::Surf,        Mood::Exotic,     HB_KELP,                               HD_SCALE | HD_FEATHER,        TB_DRIFT,                 MN_CORAL | MN_PEARL,          0},
      {"SEA CLIFFS",             "seacliffs",     Biome::Beach,   Ground::Grass,      115, 131, 105, EF_COAST | EF_ROCKY,                     35, TR_FISH,               Sky::Showery,    Ambience::Cliffs,      Mood::Coastal,    HB_SALTWORT | HB_THISTLE,              HD_FEATHER | HD_GOAT,         0,                        MN_FLINT | MN_CHALK,          OB_TIN},
      {"MEADOWS",                "meadow",        Biome::Plains,  Ground::Grass,       88, 152,  66, EF_OPEN,                                100, TR_FARM | TR_HERD,     Sky::Temperate,  Ambience::Meadow,      Mood::Pastoral,   HB_HEALWORT | HB_THISTLE,              HD_DEER | HD_BOAR | HD_WOLF,  0,                        MN_CLAY,                      0},
      {"FLOWER MEADOWS",         "flowermeadow",  Biome::Plains,  Ground::Meadow,     158, 206,  96, EF_OPEN,                                 95, TR_FARM,               Sky::Temperate,  Ambience::Meadow,      Mood::Pastoral,   HB_HEALWORT | HB_MOONPETAL | HB_SUNBLOOM, HD_DEER | HD_FEATHER,       0,                        MN_CLAY,                      0},
      {"TALLGRASS PRAIRIE",      "prairie",       Biome::Plains,  Ground::Grass,      180, 178,  52, EF_OPEN,                                 85, TR_HERD | TR_FARM,     Sky::Temperate,  Ambience::Breeze,      Mood::Pastoral,   HB_THISTLE | HB_SUNBLOOM,              HD_DEER | HD_WOLF,            0,                        MN_CLAY,                      0},
      {"STEPPE",                 "steppe",        Biome::Plains,  Ground::Grass,      160, 150, 120, EF_OPEN,                                 60, TR_HERD,               Sky::Dry,        Ambience::Breeze,      Mood::Highland,   HB_THISTLE,                            HD_WOLF | HD_GOAT | HD_SPOTTED, 0,                      MN_FLINT,                     OB_COPPER},
      {"SAVANNA",                "savanna",       Biome::Plains,  Ground::Grass,      216, 156,  90, EF_OPEN | EF_HOT,                        60, TR_HERD | TR_HUNT,     Sky::Dry,        Ambience::Breeze,      Mood::Exotic,     HB_SUNBLOOM,                           HD_SPOTTED | HD_SCALE,        TB_ACACIA,                MN_CLAY,                      0},
      {"HEATH AND MOOR",         "heath",         Biome::Plains,  Ground::Grass,      136, 105, 113, EF_OPEN,                                 40, TR_HERD,               Sky::Misty,      Ambience::Breeze,      Mood::Highland,   HB_HEATHER | HB_BLOODMOSS,             HD_GOAT | HD_DEER,            0,                        MN_PEAT,                      OB_TIN},
      {"CHALK DOWNS",            "chalkdowns",    Biome::Plains,  Ground::Grass,      192, 204, 168, EF_OPEN,                                 75, TR_HERD | TR_FARM,     Sky::Temperate,  Ambience::Meadow,      Mood::Pastoral,   HB_THISTLE | HB_HEALWORT,              HD_GOAT,                      0,                        MN_CHALK | MN_FLINT,          0},
      {"ALPINE MEADOWS",         "alpinemeadow",  Biome::Plains,  Ground::Meadow,     127, 180, 123, EF_OPEN | EF_COLD,                       40, TR_HERD,               Sky::Temperate,  Ambience::Breeze,      Mood::Highland,   HB_FROSTCAP | HB_MOONPETAL,            HD_GOAT | HD_FUR,             TB_LARCH,                 0,                            OB_SILVER},
      {"STANDING-STONE PLAINS",  "stoneplains",   Biome::Plains,  Ground::Grass,      142, 148, 100, EF_OPEN | EF_RARE | EF_MAGIC,            45, TR_HERD,               Sky::Misty,      Ambience::Mystic,      Mood::Wondrous,   HB_MOONPETAL | HB_THISTLE,             HD_DEER,                      0,                        MN_FLINT,                     0},
      {"FOREST",                 "mixedforest",   Biome::Forest,  Ground::ForestFloor,  59, 112,  54, EF_WOODED,                              80, TR_LUMBER | TR_HUNT,   Sky::Temperate,  Ambience::Woods,       Mood::Woodland,   HB_HEALWORT | HB_BLOODMOSS,            HD_DEER | HD_WOLF | HD_BOAR | HD_BEAR, TB_OAK | TB_MAPLE, MN_RESIN,                  0},
      {"BIRCH WOOD",             "birchwood",     Biome::Forest,  Ground::ForestFloor,115, 148,  78, EF_WOODED,                              80, TR_LUMBER,             Sky::Temperate,  Ambience::Woods,       Mood::Woodland,   HB_HEALWORT | HB_MOONPETAL,            HD_DEER | HD_WOLF,            TB_BIRCH,                 MN_RESIN,                     0},
      {"OLD-GROWTH FOREST",      "giantforest",   Biome::Forest,  Ground::ForestFloor,  42,  85,  44, EF_WOODED,                              55, TR_LUMBER | TR_HUNT,   Sky::Misty,      Ambience::DeepWoods,   Mood::Deep,       HB_BLOODMOSS | HB_GLOWCAP,             HD_BEAR | HD_DEER,            TB_REDWOOD,               MN_RESIN | MN_AMBER,          0},
      {"DARK FOREST",            "darkforest",    Biome::Forest,  Ground::ForestFloor,  50,  58,  58, EF_WOODED | EF_HOSTILE,                 15, TR_HUNT,               Sky::Misty,      Ambience::DeepWoods,   Mood::Ominous,    HB_NIGHTSHADE | HB_BLOODMOSS,          HD_WOLF | HD_SILK,            TB_EBONY,                 MN_RESIN,                     0},
      {"BLOSSOM GROVE",          "blossomgrove",  Biome::Forest,  Ground::Meadow,     206, 156, 176, EF_WOODED,                              85, TR_FARM | TR_LUMBER,   Sky::Temperate,  Ambience::Woods,       Mood::Pastoral,   HB_BLOSSOM | HB_MOONPETAL,             HD_DEER | HD_FEATHER,         TB_CHERRY,                MN_CLAY,                      0},
      {"BAMBOO FOREST",          "bamboo",        Biome::Forest,  Ground::ForestFloor,178, 212,  58, EF_WOODED | EF_HOT,                     65, TR_LUMBER,             Sky::Showery,    Ambience::Bamboo,      Mood::Exotic,     HB_HEALWORT,                           HD_BOAR | HD_SPOTTED,         TB_BAMBOO,                MN_CLAY,                      0},
      {"JUNGLE",                 "jungle",        Biome::Forest,  Ground::ForestFloor,  18,  93,  63, EF_WOODED | EF_HOT | EF_WET,            35, TR_HUNT | TR_LUMBER,   Sky::Monsoon,    Ambience::Jungle,      Mood::Exotic,     HB_SUNBLOOM | HB_BLOODMOSS | HB_NIGHTSHADE, HD_SCALE | HD_SPOTTED | HD_FEATHER, TB_MAHOGANY, MN_AMBER | MN_RESIN,       0},
      {"MUSHROOM FOREST",        "mushroomforest",Biome::Forest,  Ground::ForestFloor,128,  92, 141, EF_WOODED | EF_RARE | EF_MAGIC,         20, TR_HUNT,               Sky::Eerie,      Ambience::Mystic,      Mood::Wondrous,   HB_GLOWCAP | HB_MOONPETAL,             HD_SILK | HD_CHITIN,          0,                        MN_SULFUR,                    0},
      {"SILVERWOOD",             "silverwood",    Biome::Forest,  Ground::ForestFloor,154, 184, 174, EF_WOODED | EF_RARE | EF_MAGIC,         70, TR_LUMBER | TR_HUNT,   Sky::Eerie,      Ambience::Mystic,      Mood::Wondrous,   HB_SILVERLEAF | HB_MOONPETAL,          HD_DEER | HD_FEATHER,         TB_SILVER,                MN_AMBER,                     OB_SILVER},
      {"AUTUMN WOODS",           "autumnwood",    Biome::Autumn,  Ground::Autumn,     183, 110,  46, EF_WOODED,                              80, TR_LUMBER | TR_HUNT,   Sky::Temperate,  Ambience::Woods,       Mood::Woodland,   HB_BLOODMOSS | HB_HEALWORT,            HD_DEER | HD_BOAR | HD_BEAR,  TB_MAPLE | TB_OAK,        MN_RESIN,                     0},
      {"TAIGA",                  "taiga",         Biome::Taiga,   Ground::Tundra,       71, 108,  86, EF_WOODED | EF_COLD,                    55, TR_LUMBER | TR_HUNT,   Sky::Snowy,      Ambience::Woods,       Mood::Frozen,     HB_FROSTCAP,                           HD_WOLF | HD_BEAR | HD_FUR,   TB_PINE | TB_LARCH,       MN_RESIN,                     0},
      {"TAIGA BOG",              "taigabog",      Biome::Taiga,   Ground::Tundra,      100, 103,  97, EF_WOODED | EF_COLD | EF_WET,           25, TR_HUNT,               Sky::Misty,      Ambience::Marsh,       Mood::Frozen,     HB_FROSTCAP | HB_MARSHROOT,            HD_FUR | HD_DEER,             TB_LARCH,                 MN_PEAT,                      OB_IRON},
      {"SNOWFIELDS",             "snowfield",     Biome::Snow,    Ground::Snow,       231, 237, 246, EF_COLD | EF_OPEN,                      20, TR_HUNT,               Sky::Snowy,      Ambience::ColdWind,    Mood::Frozen,     HB_FROSTCAP,                           HD_FUR,                       TB_PINE,                  MN_ICE,                       0},
      {"TUNDRA",                 "tundra",        Biome::Snow,    Ground::Tundra,     147, 156, 130, EF_COLD | EF_OPEN,                      25, TR_HERD | TR_HUNT,     Sky::Snowy,      Ambience::ColdWind,    Mood::Frozen,     HB_FROSTCAP | HB_BLOODMOSS,            HD_FUR | HD_DEER,             0,                        MN_PEAT | MN_BONE,            0},
      {"GLACIER",                "glacier",       Biome::Snow,    Ground::Snow,       202, 224, 246, EF_COLD | EF_ROCKY | EF_HOSTILE,        0,  0,                     Sky::Blizzard,   Ambience::ColdWind,    Mood::Frozen,     HB_FROSTCAP,                           HD_FUR,                       0,                        MN_ICE | MN_CRYSTAL,          OB_SILVER},
      {"FROZEN LAKES",           "frozenlakes",   Biome::Snow,    Ground::Snow,       184, 206, 228, EF_COLD | EF_OPEN | EF_WET,             25, TR_FISH,               Sky::Snowy,      Ambience::ColdWind,    Mood::Frozen,     HB_FROSTCAP,                           HD_FUR | HD_SEAL,             TB_PINE,                  MN_ICE,                       0},
      {"REED MARSH",             "reedmarsh",     Biome::Swamp,   Ground::Swamp,        95, 123,  73, EF_WET,                                 35, TR_FISH | TR_HUNT,     Sky::Misty,      Ambience::Marsh,       Mood::Wetland,    HB_MARSHROOT | HB_HEALWORT,            HD_FEATHER | HD_SCALE,        TB_CYPRESS,               MN_CLAY | MN_PEAT,            0},
      {"PEAT BOG",               "peatbog",       Biome::Swamp,   Ground::Swamp,      103,  92,  67, EF_WET | EF_HOSTILE,                    15, TR_HUNT,               Sky::Misty,      Ambience::Marsh,       Mood::Ominous,    HB_MARSHROOT | HB_BLOODMOSS,           HD_FEATHER,                   0,                        MN_PEAT | MN_BONE,            OB_IRON},
      {"MANGROVE COAST",         "mangrove",      Biome::Swamp,   Ground::Swamp,        82, 125, 111, EF_WET | EF_HOT | EF_COAST | EF_WOODED, 30, TR_FISH,               Sky::Monsoon,    Ambience::Jungle,      Mood::Wetland,    HB_MARSHROOT | HB_KELP,                HD_SCALE | HD_FEATHER,        TB_MANGROVE,              MN_PEARL | MN_CLAY,           0},
      {"FLOODED FOREST",         "floodedforest", Biome::Swamp,   Ground::Swamp,        48,  90,  74, EF_WET | EF_WOODED,                     25, TR_FISH | TR_LUMBER,   Sky::Misty,      Ambience::Marsh,       Mood::Wetland,    HB_MARSHROOT | HB_GLOWCAP,             HD_SCALE | HD_FEATHER,        TB_CYPRESS,               MN_CLAY,                      0},
      {"LAKE DISTRICT",          "lakedistrict",  Biome::Plains,  Ground::Grass,       99, 153, 123, EF_WET | EF_OPEN,                       85, TR_FISH | TR_FARM,     Sky::Showery,    Ambience::Meadow,      Mood::Pastoral,   HB_MARSHROOT | HB_HEALWORT,            HD_FEATHER | HD_DEER,         TB_BIRCH,                 MN_CLAY,                      0},
      {"DUNE SEA",               "dunes",         Biome::Desert,  Ground::Sand,       232, 202, 131, EF_HOT | EF_OPEN | EF_HOSTILE,          10, TR_HERD,               Sky::Arid,       Ambience::DesertWind,  Mood::Arid,       HB_SUNBLOOM,                           HD_SCALE | HD_CHITIN,         0,                        MN_GLASSSAND,                 0},
      {"STONY DESERT",           "stonydesert",   Biome::Desert,  Ground::Sand,       196, 170, 130, EF_HOT | EF_OPEN | EF_ROCKY,            20, TR_MINE | TR_HERD,     Sky::Arid,       Ambience::DesertWind,  Mood::Arid,       HB_SUNBLOOM,                           HD_SCALE | HD_CHITIN,         0,                        MN_FLINT,                     OB_COPPER},
      {"BADLANDS",               "badlands",      Biome::Desert,  Ground::Sand,       193, 112,  76, EF_HOT | EF_ROCKY,                      25, TR_MINE,               Sky::Dry,        Ambience::DesertWind,  Mood::Arid,       HB_SUNBLOOM | HB_THISTLE,              HD_SCALE | HD_GOAT,           0,                        MN_CLAY | MN_SULFUR,          OB_COPPER | OB_IRON},
      {"SALT FLATS",             "saltflats",     Biome::Desert,  Ground::Sand,       230, 222, 218, EF_HOT | EF_OPEN | EF_HOSTILE,          10, TR_MINE,               Sky::Arid,       Ambience::DesertWind,  Mood::Arid,       HB_SALTWORT,                           HD_CHITIN,                    0,                        MN_SALT,                      0},
      {"SCRUBLAND",              "scrubland",     Biome::Desert,  Ground::Sand,       175, 141,  92, EF_HOT | EF_OPEN | EF_ROCKY,            45, TR_HERD,               Sky::Dry,        Ambience::DesertWind,  Mood::Arid,       HB_THISTLE | HB_SUNBLOOM,              HD_GOAT | HD_SCALE,           TB_ACACIA,                MN_FLINT | MN_CLAY,           0},
      {"OASIS",                  "oasis",         Biome::Desert,  Ground::Grass,        91, 163,  93, EF_HOT | EF_WET | EF_RARE,              90, TR_FARM | TR_HERD,     Sky::Dry,        Ambience::Meadow,      Mood::Exotic,     HB_SUNBLOOM | HB_HEALWORT,             HD_FEATHER,                   TB_ACACIA,                MN_CLAY,                      0},
      {"MOUNTAINS",              "mountain",      Biome::Mountain,Ground::Rock,       130, 129, 133, EF_ROCKY | EF_COLD,                     10, TR_MINE,               Sky::Snowy,      Ambience::ColdWind,    Mood::Highland,   HB_FROSTCAP,                           HD_GOAT | HD_FUR,             0,                        MN_FLINT | MN_CRYSTAL,        OB_IRON | OB_SILVER},
      {"ASH FIELDS",             "ashfields",     Biome::Desert,  Ground::Sand,         76,  76,  71, EF_HOT | EF_ROCKY | EF_RARE | EF_HOSTILE, 5, TR_MINE,              Sky::Ashfall,    Ambience::Volcanic,    Mood::Ominous,    HB_EMBERWEED,                          HD_SCALE,                     0,                        MN_OBSIDIAN | MN_SULFUR,      OB_IRON | OB_RARE},
      {"CRYSTAL BARRENS",        "crystalbarrens",Biome::Desert,  Ground::Sand,      172, 162, 204, EF_ROCKY | EF_RARE | EF_MAGIC | EF_HOSTILE, 5, TR_MINE,             Sky::Eerie,      Ambience::Crystal,     Mood::Wondrous,   HB_CRYSTALBLOOM,                       HD_CHITIN,                    0,                        MN_CRYSTAL | MN_GLASSSAND,    OB_RARE | OB_SILVER},
      {"PETRIFIED FOREST",       "petrifiedforest",Biome::Desert, Ground::Sand,      160, 129, 118, EF_ROCKY | EF_WOODED | EF_RARE | EF_HOT, 15, TR_MINE,              Sky::Dry,        Ambience::DesertWind,  Mood::Wondrous,   HB_SUNBLOOM,                           HD_SCALE | HD_CHITIN,         TB_PETRIFIED,             MN_AMBER | MN_FLINT,          OB_COPPER},
      {"BLIGHTED LAND",          "blight",        Biome::Plains,  Ground::Grass,       101,  69,  89, EF_RARE | EF_HOSTILE,                   0,  0,                     Sky::Eerie,      Ambience::Blight,      Mood::Ominous,    HB_NIGHTSHADE,                         HD_CHITIN, 0,                        MN_BONE | MN_SULFUR,          0},
  };
  static_assert(sizeof(T) / sizeof(T[0]) == (size_t)Eco::COUNT, "an EcoInfo for every Eco");
  return T[(int)e < (int)Eco::COUNT ? (int)e : (int)Eco::Meadow];
}
inline Biome ecoFamily(Eco e) { return ecoInfo(e).family; }
inline const char* ecoName(Eco e) { return ecoInfo(e).name; }
inline bool ecoHas(Eco e, uint16_t flag) { return (ecoInfo(e).flags & flag) != 0; }
// the eco a pre-M3c family stands for (the classic look: what a family-only generator or a test map gets)
inline Eco ecoOfFamily(Biome b) {
  switch (b) {
    case Biome::Ocean: return Eco::Ocean;
    case Biome::Beach: return Eco::SandBeach;
    case Biome::Plains: return Eco::Meadow;
    case Biome::Forest: return Eco::MixedForest;
    case Biome::Autumn: return Eco::AutumnWood;
    case Biome::Taiga: return Eco::Taiga;
    case Biome::Snow: return Eco::SnowField;
    case Biome::Swamp: return Eco::ReedMarsh;
    case Biome::Desert: return Eco::Dunes;
    case Biome::Mountain: return Eco::Mountain;
    default: return Eco::Meadow;
  }
}
// scripts and tests: an eco by its key (or its name, any case, spaces / dashes / underscores ignored); COUNT if none
inline Eco ecoFromName(const char* s) {
  auto norm = [](const char* a, char* out, int cap) {
    int n = 0;
    for (; *a && n < cap - 1; a++) {
      char ch = *a;
      if (ch == ' ' || ch == '-' || ch == '_') continue;
      out[n++] = (char)(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch);
    }
    out[n] = 0;
  };
  char want[40], k[40];
  norm(s, want, 40);
  for (int i = 0; i < (int)Eco::COUNT; i++) {
    norm(ecoInfo((Eco)i).key, k, 40);
    if (!std::strcmp(k, want)) return (Eco)i;
    norm(ecoInfo((Eco)i).name, k, 40);
    if (!std::strcmp(k, want)) return (Eco)i;
  }
  return Eco::COUNT;
}
