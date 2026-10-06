// The culture engine (VISION_PLAN section 5, M3 "Many Peoples"; owner addenda 15.6, 15.11). Pure data, generated from
// the world seed, never stored in saves (only ids). FROZEN after M3 phase A: the CULTURE lane implements it in
// rpg/culture/*.cpp; everybody else reads it.
//
// Two levels (5.1): a culture FAMILY per 2048-tile culture cell (ew::CCELL), and a DIALECT of the family per kingdom
// (the family of the culture cell holding the kingdom's seat, mutated by the kingdom's seed). A settlement in a kingdom
// takes the kingdom's dialect; a settlement in the wildlands takes its cell's family. So kingdom borders are where the
// look changes, and neighbouring kingdoms of one family are recognisably related.
//
// Peoples (owner 15.6): humans, half-breeds and elves from the start, sharing the human rig with per-people proportions,
// ears and colouring. Every culture has a people mix (most human cultures hold a few half-breeds and the odd elf; the
// elven archetypes are mostly elves). Other peoples later.
//
// Materials (owner 15.11): the universal tiers every culture knows are leather, bronze (copper + tin), iron and steel.
// On top of them each culture has its own alloys from regional ores, with its own look; their recipes are culture
// SECRETS the player can learn (M6 crafting reads this data; M3 only defines it).
//
// Determinism (VISION_PLAN 2.3, 2.4): every value is a pure function of (world seed, cell / kingdom cell). Structural
// choices (the maximin selection, the distance metric that drives it) use integer / Q16 maths only (rpg/world/dmath.h);
// rpg/culture/*.cpp compile with precise floating point (CMake rpg_culture). Order independent: asking for cultures in
// any order gives the same answers (5.4's four-phase checkerboard maximin).
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "engine/music_style.h"
#include "rpg/culture/style.h"

namespace cult {

// ---------------------------------------------------------------- identity
// A culture id: a family (kind 1: the culture cell ci, cj) or a kingdom's dialect (kind 2: also the kingdom cell kx,
// ky, ew::KCELL). 0 = none (callers fall back to the heartland look).
//   [63..60 kind][59..46 ci:14][45..32 cj:14][31..16 kx:16][15..0 ky:16]
using CultureId = uint64_t;
inline CultureId familyId(int32_t ci, int32_t cj) {
  return (1ull << 60) | ((uint64_t)(uint32_t)(ci & 0x3FFF) << 46) | ((uint64_t)(uint32_t)(cj & 0x3FFF) << 32);
}
inline CultureId dialectId(int32_t ci, int32_t cj, int32_t kx, int32_t ky) {
  return (2ull << 60) | ((uint64_t)(uint32_t)(ci & 0x3FFF) << 46) | ((uint64_t)(uint32_t)(cj & 0x3FFF) << 32) |
         ((uint64_t)(uint32_t)(kx & 0xFFFF) << 16) | (uint64_t)(uint32_t)(ky & 0xFFFF);
}
inline int cultureKind(CultureId id) { return (int)(id >> 60); }   // 0 none, 1 family, 2 dialect
inline int32_t sx14(uint32_t v) { return (v & 0x2000u) ? (int32_t)(v | 0xFFFFC000u) : (int32_t)v; }
inline int32_t sx16(uint32_t v) { return (v & 0x8000u) ? (int32_t)(v | 0xFFFF0000u) : (int32_t)v; }
inline int32_t cultureCi(CultureId id) { return sx14((uint32_t)(id >> 46) & 0x3FFFu); }
inline int32_t cultureCj(CultureId id) { return sx14((uint32_t)(id >> 32) & 0x3FFFu); }
inline int32_t cultureKx(CultureId id) { return sx16((uint32_t)(id >> 16) & 0xFFFFu); }
inline int32_t cultureKy(CultureId id) { return sx16((uint32_t)id & 0xFFFFu); }
inline CultureId familyOf(CultureId id) { return id ? familyId(cultureCi(id), cultureCj(id)) : 0; }

// ---------------------------------------------------------------- the peoples (owner 15.6)
enum class People : uint8_t { Human, HalfBreed, Elf, COUNT };
const char* peopleName(People p);   // "HUMAN", "HALF-BREED", "ELF"

// ---------------------------------------------------------------- archetypes (5.3 + the elven ones)
// Each is a set of distributions the generator mutates inside. Sylvan: wood elves (living timber halls under leaf and
// bark roofs with sweeping ridges, glow-lamps, flutes and harps). Starspire: high elves (white ashlar, slender onion
// and conical spires, pointed windows, bells and choir).
enum class Archetype : uint8_t { Fjordfolk, Highland, Heartland, Imperial, Dune, Steppe, Marsh, Jade, River, SunTemple,
                                 Sylvan, Starspire, COUNT };
const char* archetypeName(Archetype a);   // "FJORDFOLK", ...

enum class Layout : uint8_t { Organic, Grid, Radial, Linear, Compound, Terraced, Stilt, COUNT };
enum class Cut : uint8_t { Tunic, Robe, Kaftan, Wrap, Coat, Kilt, Poncho, Gown, COUNT };   // Gown: elven long gown
enum class Headwear : uint8_t { None, Hood, Cap, Turban, FurHat, Veil, Circlet, Conical, Headscarf, COUNT };
enum class HelmForm : uint8_t { Nasal, Kettle, GreatHelm, Spangen, Horned, Plumed, ConicalAventail, Masked, Crested, Winged, COUNT };
enum class BodyArm : uint8_t { Padded, Leather, Mail, Scale, Lamellar, Brigandine, Plate, Leaf, COUNT };   // Leaf: elven
                                                                                                        // overlapping leaf plates
enum class ShieldForm : uint8_t { Round, Kite, Heater, Tower, Crescent, Oval, Buckler, Leaf, None, COUNT };
enum class Blade : uint8_t { Straight, Leaf, Falchion, Scimitar, Khopesh, Wavy, Broad, Curved, Glaive, COUNT };
enum class Polearm : uint8_t { Spear, Glaive, Halberd, None, COUNT };
enum class BowKind : uint8_t { Self, Recurve, Composite, Crossbow, Longbow, COUNT };
// cloth patterns (DressStyle::pattern)
enum class Pattern : uint8_t { Plain, Stripes, Checks, BorderTrim, Dots, Embroidery, Vines, COUNT };
// armour / weapon ornament bits (ArmsStyle::ornament)
enum : uint16_t { ARM_PLUMES = 1, ARM_FUR_TRIM = 2, ARM_GILDING = 4, ARM_HORSETAIL = 8, ARM_STUDS = 16, ARM_ETCHING = 32,
                  ARM_TASSELS = 64, ARM_SCALLOPS = 128, ARM_RIVETS = 256, ARM_FILIGREE = 512 };

// ---------------------------------------------------------------- materials and alloys (owner 15.11; crafting is M6)
// The universal tiers known by every culture, in order (an item's base material before any culture alloy)
enum class Tier : uint8_t { Leather, Bronze, Iron, Steel, COUNT };
const char* tierName(Tier t);   // "LEATHER", "BRONZE", "IRON", "STEEL"
// What an alloy looks like on armour and weapons (the art lane's painters key on it, M6)
enum class Sheen : uint8_t { Matte, Bright, Dark, Banded, Iridescent, Glowing, Pale, Burnished, COUNT };
// One ingredient: a regional ore (ew::Ore, rpg/world/geology.h, as int so this header needs no world include) or an
// exotic reagent only some lands yield (M6 places them; 'reagent' names it)
struct Ingredient {
  int8_t ore = -1;           // ew::Ore index, or -1 for a reagent
  std::string reagent;       // "STARGLASS DUST", "SALAMANDER ASH" (when ore < 0)
  uint8_t parts = 1;         // relative amount
};
struct Alloy {
  std::string name;          // the culture's word: "VETHSTEEL", "SUNBRASS", "MOONSILVER"
  std::vector<Ingredient> recipe;   // 2..4 ingredients
  Tier baseTier = Tier::Steel;      // the universal tier it improves on (sits above it)
  uint8_t tierStep = 1;      // 1..3: how far above the base tier it stands (M6 item level)
  Sheen sheen = Sheen::Bright;
  uint32_t color = 0, color2 = 0;   // the metal's light and dark key (rgba): painters build ramps from them
  uint8_t secrecy = 128;     // 0..255: how closely guarded (trust needed to be taught; theft costs goodwill)
  uint8_t learn = 0;         // bits: how it can be learned: 1 apprenticeship, 2 ruin lore, 4 breaking down pieces, 8 theft
  std::string lore;          // one line for the codex ("Quenched in snowmelt under the aurora")
};

// ---------------------------------------------------------------- names (5.2 Phonology)
struct Phonology {
  std::vector<std::string> onsets, nuclei, codas;   // weighted by order (earlier = more common)
  uint8_t sylMin = 2, sylMax = 3;                   // syllables per name
  uint8_t pattern = 15;                             // allowed syllables, bits: 1 CV, 2 CVC, 4 V, 8 VC
  std::vector<std::string> placeSuffix, personSuffixF, personSuffixM, epithets;
  uint8_t joiner = 0;                               // 0 none, 1 apostrophe, 2 hyphen, 3 space ("QASR AMUN")
  std::string forbid;                               // banned bigrams, pairs of letters back to back ("qkxz")
};

// ---------------------------------------------------------------- the look of people (5.2 DressStyle)
struct DressStyle {
  Cut cutM = Cut::Tunic, cutF = Cut::Tunic;
  Headwear head[3] = {Headwear::None, Headwear::Hood, Headwear::Cap};   // weighted choices (first most common)
  uint8_t headP = 64;               // 0..255 chance a resident wears headwear
  uint32_t cloth[6] = {};           // the cloth palette (rgba)
  Pattern pattern = Pattern::Plain;
  uint32_t trim = 0;                // pattern / trim colour (rgba; 0 = a darker cloth colour)
  uint8_t skinLo = 2, skinHi = 6;   // index range into kSkinRamp (wide variation within any people)
  uint16_t hairStyles = 0xFF;       // bitset over art::Hair
  uint32_t hairCols[4] = {};        // the hair colours seen most (rgba)
  uint8_t beardP = 110;             // 0..255 beard probability (men)
  uint8_t jewellery = 0;            // 0..255 probability
  uint8_t facePaint = 0;            // 0 none, else a style id 1..4 (stripes, dots, mask band, sun mark)
};
// the 12-step skin ramp DressStyle::skinLo..skinHi index (pale to deep); elves bias pale-to-golden, unless the culture
// says otherwise
uint32_t skinRamp(int i);           // i 0..11
constexpr int kSkinSteps = 12;

// ---------------------------------------------------------------- arms and armour (5.2 ArmsStyle, owner 15.11)
// The culture's distinctive silhouettes, ornament and palette for armour and weapons. Guards wear their OWNER
// kingdom's arms (so occupation shows), bandits a poor local version. M3: data + guard looks; M6 builds items on it.
struct ArmsStyle {
  HelmForm helm[2] = {HelmForm::Nasal, HelmForm::Kettle};
  BodyArm body[2] = {BodyArm::Mail, BodyArm::Leather};
  ShieldForm shield = ShieldForm::Kite;
  Blade blade = Blade::Straight;
  Polearm polearm = Polearm::Spear;
  BowKind bow = BowKind::Self;
  uint32_t metal = 0, leather = 0, cloth = 0, plume = 0;   // colours (rgba)
  uint16_t ornament = 0;            // ARM_* bits
  // silhouette dials 0..3 the painters read (what makes a culture's soldier recognisable at 16 px)
  uint8_t pauldron = 1;             // shoulder guards: 0 none .. 3 huge
  uint8_t skirt = 1;                // tassets / mail skirt length: 0 none .. 3 to the knee
  uint8_t crest = 0;                // helm crest / plume height: 0 none .. 3 tall
  uint8_t cape = 0;                 // 0 none, 1 short cape, 2 long cloak, 3 sash
  std::vector<Alloy> alloys;        // the culture's own alloys (1..3), best last
  int8_t favouredOre = -1;          // ew::Ore the culture's smiths prize (the region's richest at its birth), -1 none
};

// ---------------------------------------------------------------- settlements (5.6, 5.7)
struct TownStyle {
  Layout layout = Layout::Organic;
  Layout altLayout = Layout::Organic;   // 1 in 4 settlements of the culture use it
  art::CityWall wall = art::CityWall::Stone;
  art::Fence fence = art::Fence::Picket;
  uint8_t centre = 0;               // art::PropStyle::centre for the main square
  uint8_t density = 128;            // 0..255: how tightly houses pack (setbacks shrink, gardens shrink)
  uint8_t trees = 128;              // 0..255: street trees and gardens
  uint8_t paving = 0;               // 0 cobbles (today), 1 flagstones, 2 packed earth, 3 sand brick, 4 boardwalk, 5 moss
                                    // stones, 6 pale dressed stone, 7 terracotta tiles, 8 slate, 9 red brick (M3 fixer:
                                    // the settlement writes it into Map::blend on its tiles, Map::PAVE_MARK; the view
                                    // paints its Plaza / Road ground in it)
};

// ---------------------------------------------------------------- faith and customs (5.2)
struct Religion {
  uint8_t kind = 0;                 // 0 Pantheon, 1 Monotheist, 2 Ancestors, 3 Animist, 4 Dualist, 5 StarCult
  std::vector<std::string> names;   // the god(s) / spirits
  uint16_t domains = 0;             // bits: sun, moon, sea, war, harvest, death, craft, wisdom, storm, forest, hearth, stars
  uint32_t glyphSeed = 0;           // its holy symbol (art::glyphSprite)
  uint32_t colour = 0, colour2 = 0; // vestments
  uint8_t shrineForm = 0;           // 0 altar, 1 standing stone, 2 tree shrine, 3 fire bowl, 4 statue, 5 obelisk
  uint8_t burial = 0;               // 0 grave, 1 cairn, 2 pyre, 3 sky, 4 sea
};
struct Customs {
  uint8_t staple[3] = {};           // food ids (M5 reads them): 0 bread, 1 fish, 2 rice, 3 flatbread, 4 mutton, 5 stew,
                                    // 6 fruit, 7 roots, 8 cheese, 9 maize
  uint8_t drink = 0;                // 0 ale, 1 mead, 2 wine, 3 tea, 4 kumis, 5 nectar
  uint8_t greetingSet = 0;          // index into the greeting tables (dialogue)
  uint8_t festivalMonth = 0;
  uint8_t lawStrict = 128, xenophobia = 64;   // 0..255
  art::Furniture furniture = art::Furniture::Chairs;
};

// ---------------------------------------------------------------- heraldry (5.5, kingdoms and the player)
// field division: 0 plain, 1 per pale, 2 per fess, 3 quarterly, 4 chevron, 5 bend, 6 saltire, 7 bordure
struct Heraldry {
  uint32_t field = 0, field2 = 0;   // tinctures (rgba): field2 is the division's second tincture
  uint32_t charge = 0;              // the charge's colour (rgba)
  uint8_t division = 0;
  uint8_t chargeKind = 0;           // 0 an emblem (art/art_heraldry.h: crown, sword, tower, star, tree, sun, moon, eagle),
                                    // 1 a glyph (glyphSeed)
  uint8_t emblem = 0;
  uint32_t glyphSeed = 0;
  uint8_t shape = 0;                // banner shape: 0 hanging square, 1 swallow-tail, 2 pennant, 3 gonfalon (tails)
  bool empty() const { return field == 0; }
  uint64_t key() const {
    uint64_t k = 0x84222325CBF29CE4ull;
    auto mx = [&](uint64_t v) { k ^= v; k *= 0x100000001B3ull; };
    mx((uint64_t)field << 32 | field2);
    mx((uint64_t)charge << 32 | glyphSeed);
    mx((uint64_t)division | (uint64_t)chargeKind << 8 | (uint64_t)emblem << 16 | (uint64_t)shape << 24);
    return k;
  }
};

// ---------------------------------------------------------------- the culture
struct Culture {
  CultureId id = 0;
  uint32_t seed = 0;
  Archetype archetype = Archetype::Heartland;
  Archetype archetype2 = Archetype::Heartland;   // isolated families cross two archetypes (== archetype otherwise)
  bool isolated = false;
  std::string name, adjective;      // "VETHMARK", "VETHMARKI" (upper case, the HUD's font)
  uint8_t values[8] = {};           // martial, mercantile, pious, scholarly, seafaring, expansionist, isolationist, honour
  uint8_t peopleMix[3] = {230, 20, 6};   // relative weights of People::Human, HalfBreed, Elf among its residents
  Phonology phon;
  art::ArchStyle arch;              // the family's base architecture (buildingArch varies it per building)
  art::RoofShape altRoof = art::RoofShape::Gable;   // 5.7: 1 in 5 buildings use the alternate roof form
  art::PropStyle props;             // fences, wells, lamps, benches, centrepiece, awnings
  TownStyle town;
  DressStyle dress;
  ArmsStyle arms;
  MusicStyle music;
  Religion faith;
  Customs customs;
  Heraldry heraldry;                // a dialect: its kingdom's arms (families: the family's own device)
  uint32_t magicTradition = 0;      // 0 = none (VISION_PLAN 6, M9)
  uint8_t homeBiome = 2;            // Biome it was born in (sampled at the culture cell's centre)
};

// ---------------------------------------------------------------- the atlas: every culture of one world
// One per ew::EndlessSource (and so one per thread: the prefetcher's worker has its own source). Memoised; not
// thread-safe. Climate comes from a callback so this library needs nothing from rpg/world.
struct ClimateSample { int biome = 2; int32_t temp = 32768, moist = 32768; bool coast = false, sea = false; };
class Atlas {
 public:
  // climate(gx, gy): the macro climate at a global tile (EndlessSource passes its macro sampler). May be empty: then
  // every family is born in temperate plains (tools, galleries).
  explicit Atlas(uint64_t worldSeed, std::function<ClimateSample(int32_t, int32_t)> climate = {});
  ~Atlas();
  Atlas(const Atlas&) = delete;
  Atlas& operator=(const Atlas&) = delete;
  uint64_t seed() const;
  // the family of culture cell (ci, cj): ew::CCELL tiles each (global tile gx lies in cell floorDiv(gx, CCELL))
  const Culture& family(int32_t ci, int32_t cj);
  // a kingdom's dialect (kx, ky: its kingdom cell; ci, cj: the culture cell holding its seat)
  const Culture& dialect(int32_t ci, int32_t cj, int32_t kx, int32_t ky);
  // either, by id (0 or an unknown kind: a plain heartland culture)
  const Culture& get(CultureId id);
  // for tests: build a family from an explicit archetype and seed, bypassing the lattice (galleries, --cultures)
  static Culture make(Archetype a, uint32_t seed, int homeBiome = 2);
  struct Stats { int families = 0, dialects = 0, candidates = 0; double ms = 0; };
  const Stats& stats() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> d_;
};

// ---------------------------------------------------------------- what reads a culture
// 5.4 distance in Q16 (0 .. 65536 = 1.0), integer maths only (drives the maximin); and its headline differences
// (palette, roof form, wall material, names, music scale: how many of the 5 differ)
int32_t distanceQ(const Culture& a, const Culture& b);
inline float distance(const Culture& a, const Culture& b) { return distanceQ(a, b) / 65536.0f; }
int headlineDiffs(const Culture& a, const Culture& b);

// names in the culture's phonology, upper case, deterministic in (culture, seed). kind for landmarkName: the
// ew::LandmarkKind index; for dungeonName: the SiteType index.
std::string placeName(const Culture& c, uint32_t seed);
std::string personName(const Culture& c, uint32_t seed, bool female);
std::string kingdomName(const Culture& c, uint32_t seed);
std::string landmarkName(const Culture& c, uint32_t seed, int kind);
std::string dungeonName(const Culture& c, uint32_t seed, int siteType);

// 5.7: one building's architecture in this culture: the family's base style, varied by the building's seed (1 in 5 use
// the alternate roof, palette jitter, window and door rhythm in ArchStyle::variant), by wealth (0 poor .. 3 rich:
// poor builds in wattle and thatch substitutes, rich in stone with more ornament) and urbanity (0 village .. 3 capital),
// in the climate it stands in (biome: snow on roofs, smoke in the cold). roofTint: the generator's per-house tint
// (Bldg::roof) or 0.
art::ArchStyle buildingArch(const Culture& c, int biome, int urban, int wealth, uint32_t seed, uint32_t roofTint = 0);

}  // namespace cult
