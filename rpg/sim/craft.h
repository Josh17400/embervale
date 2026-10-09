// M6 Steel: crafting on the economy's own production chains (owner 15.11: "the player can, if they want, craft
// everything, using the same production chains as the economy: ore -> smelter -> ingot -> smith -> item; hide ->
// tanner -> leather"), regional ores, the universal tiers leather -> bronze -> iron -> steel with each culture's alloys
// on top, and culture smithing SECRETS learnable by trust / apprenticeship, ruins lore, breaking pieces down, theft or
// quests. Shared contract, frozen after M6 phase A (lead, 2026-10-08): the NUMBERS lane (crafting is its "forge" half)
// is its only editor (it may ADD declarations) and implements it in rpg/sim/craft.cpp (data, recipes, knowledge) and
// rpg/sim/craft_game.cpp (the Game hooks: the smith's / smelter's / tanner's dialogue, learning, the forge screen).
//
// Phase A (lead) implements the data layer for real: the materials, their economy goods, the regional ore reading, the
// universal and culture recipes, the knowledge store and its save block, canMake / make. The NUMBERS lane wires it into
// play (dialogue and a thin forge screen, shops that sell local ores and ingots, learning hooks) and tunes it.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/sim/common.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/items.h"
#include "rpg/world/economy.h"
#include "rpg/world/geology.h"

namespace cult { struct Culture; }
namespace ew { class EndlessSource; }
class Game;
struct Actor;

namespace craft {

// ---- materials (Item kind Material, Item::sub = Stuff). Append only (saved).
enum class Stuff : uint8_t {
  CopperOre, TinOre, IronOre, Coal, SilverOre, RareOre,        // ores (ew::Ore order): mined where the geology favours them
  CopperIngot, TinIngot, BronzeIngot, IronIngot, SteelIngot, SilverIngot,
  AlloyIngot,                                                  // a culture's alloy (Item::culture + Item::alloy name it)
  Hide, Leather,                                               // raw hide (hunting, herders) and tanned leather
  Timber, Cloth,                                               // hafts, bows, grips / padding, cloaks
  Reagent,                                                     // an alloy's exotic ingredient (cult::Ingredient::reagent;
                                                               // Item::culture + alloy name which)
  COUNT
};
struct StuffInfo {
  const char* name;       // "COPPER ORE", "BRONZE INGOT"...
  art::Icon icon;
  uint32_t tint;          // rgba (alloy ingots and reagents: the alloy's colour instead)
  int value;              // list price, gold
  ew::Good good;          // the economy good it trades as (Ore, Ingot, Hides, Leather, Logs, Cloth)
};
const StuffInfo& stuffInfo(Stuff s);
// a stack of material (alloy ingots and reagents carry their culture and alloy: "VETHSTEEL INGOT")
Item makeStuff(Stuff s, int count, uint64_t culture = 0, uint8_t alloy = 0, const cult::Culture* c = nullptr);
inline bool isStuff(const Item& it, Stuff s) { return it.kind == ItemKind::Material && it.sub == (uint8_t)s; }
Stuff oreStuff(ew::Ore o);              // CopperOre .. RareOre
Stuff ingotOf(ew::Ore o);               // the ingot an ore smelts to (Coal: COUNT, it is fuel; Rare: AlloyIngot)

// ---- regional ores (15.11: "ores are regional: they follow geology and biome, so no region has everything")
struct OreSource { ew::Ore ore = ew::Ore::Iron; uint8_t richness = 0; };   // richness 0..255
// the ores worth mining at a global tile, richest first (province geology affinities, raised by the eco's ore bias,
// EcoInfo::ores); ores below a threshold are left out
std::vector<OreSource> oresAt(ew::EndlessSource& src, int32_t gx, int32_t gy);

// ---- recipes (one step of a chain at a workstation, as ew::Recipe: Smelter, Smithy, Tanner, Weaver, Sawmill...)
struct Need { Stuff stuff = Stuff::IronIngot; uint8_t count = 1; uint64_t culture = 0; uint8_t alloy = 0; };   // culture/alloy:
                                                                                                         // alloy ingots, reagents
struct Recipe {
  std::string id;                 // stable key: "smelt.bronze", "forge.iron.sword", "forge.alloy.<culture hex>.1.helmet"
  art::Building at = art::Building::Smithy;
  std::vector<Need> in;
  // the output: a material (out < COUNT) or a piece of gear (kind / sub / mat)
  Stuff out = Stuff::COUNT;
  uint8_t outCount = 1;
  ItemKind kind = ItemKind::Misc;
  uint8_t sub = 0;                // WeaponType for weapons
  Mat mat = Mat::None;
  uint64_t culture = 0;           // a culture's recipe (its alloy, its pattern): needs its secret
  uint8_t alloy = 0;              // 1-based index into that culture's alloys
  uint8_t skill = 0;              // smithing skill needed (0..100)
  bool gear() const { return out == Stuff::COUNT; }
};
// the universal recipes every smith knows: smelting copper, tin, iron, silver; bronze (copper + tin), steel (iron +
// coal); tanning; and every gear piece in leather, bronze, iron and steel
const std::vector<Recipe>& baseRecipes();
// a culture's own (none for an id of 0): smelting each of its alloys (its cult::Alloy::recipe in ores and reagents) and its pieces in them
std::vector<Recipe> cultureRecipes(const cult::Culture& c);

// ---- secrets (15.11: "a player can learn a culture's alloy recipes and its armour and weapon making")
enum class SecretKind : uint8_t { AlloyRecipe, ArmourPattern, WeaponPattern, COUNT };
// how a secret was (or can be) learned: the low four bits are cult::Alloy::learn's
enum : uint8_t { HOW_APPRENTICE = 1, HOW_RUINS = 2, HOW_BREAKDOWN = 4, HOW_THEFT = 8, HOW_QUEST = 16 };
struct Secret {
  uint64_t culture = 0;           // cult::CultureId (the FAMILY: dialects share their family's secrets)
  uint8_t alloy = 0;              // 1-based (0: the culture's patterns in the universal metals)
  SecretKind kind = SecretKind::AlloyRecipe;
  uint8_t progress = 0;           // 0..100; 100 = known
  uint8_t how = 0;                // HOW_* bits that contributed
};
struct Knowledge {
  uint16_t skill = 0;                     // smithing skill in tenths (0..1000 = 0..100)
  std::vector<Secret> secrets;
  std::map<uint64_t, uint8_t> trust;      // a master smith (Game::npcKey) -> trust 0..100 (apprenticeship)
  bool knows(uint64_t culture, uint8_t alloy, SecretKind k) const;
  int progress(uint64_t culture, uint8_t alloy, SecretKind k) const;
  // add progress (clamped to 100); true when this made it known
  bool advance(uint64_t culture, uint8_t alloy, SecretKind k, int amount, uint8_t how);
  int skillLevel() const { return skill / 10; }
  // (NUMBERS lane, M6 phase B) the player's M6 runtime state: potion cooldown, unique-power timers, the worn gear's
  // summed affixes, merchant purses. The timers are never saved; (M6 fixer r4) the purses and the per-restock trust
  // caps are (craft block v2). game.h is frozen for M6, so it rides here; a later milestone may move it onto Game.
  gear::Live live;
  // the craft block of the save (SAVE_VER 12): its own version byte first
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);
};
// the culture id secrets are kept under (a dialect's family)
uint64_t secretKey(uint64_t culture);

// ---- the forge screen (Mode::Forge; Game::bench): the station the player works at and what can be made there.
//      UI-facing, never saved; the NUMBERS lane fills it from craftChoose and draws it in rpg/view/craft_ui.cpp.
struct Bench {
  int actor = -1;                 // the smith / smelter / tanner who lent the station (-1: a station on its own)
  art::Building at = art::Building::Smithy;
  std::string title;              // "THE SMITHY OF ..."
  std::vector<Recipe> recipes;    // what this station makes for this player (known ones first)
  // (NUMBERS lane additions) the settlement the station stands in (World::sites handle, -1 none) and its culture
  int site = -1;
  uint64_t culture = 0;
  std::string lastMade;           // the last thing made here ("MADE BRONZE SWORD"), for the screen's status line
  int focus = -1;                 // a recipe (index) the screen should show selected next frame (scripts, benchMake); -1 none
};

// ---- making
// whether the pack holds every input (and the secret / skill it needs is known); why: a short reason when not
bool canMake(const Recipe& r, const std::vector<Item>& inv, const Knowledge& k, std::string* why = nullptr);
// consume the inputs and make the output (gear at item level `ilvl`; its rarity from the skill and the rng). False
// (nothing consumed) when canMake fails. `c`: the recipe's culture (for names and alloys), may be null.
bool make(const Recipe& r, std::vector<Item>& inv, Knowledge& k, Rng& rng, int ilvl, const cult::Culture* c, Item& out);

// =====================================================================================================================
// NUMBERS lane additions (M6 phase B). Added only; nothing above changed meaning.
// =====================================================================================================================

// ---- stations: the buildings whose workers lend their workstation (as ew::Recipe: the economy's own chains)
bool isStation(art::Building b);            // Smithy, Smelter, Tanner, Weaver, Sawmill
const char* stationVerb(art::Building b);   // "WORK THE FORGE", "WORK THE SMELTER", "WORK THE TANNERY", "WORK THE LOOM",
                                            // "WORK THE BOWYER'S BENCH"
const char* stationName(art::Building b);   // "FORGE", "SMELTER", "TANNERY", "LOOM", "BOWYER'S BENCH"
// what a station makes for this player: the base recipes at that station, then the station culture's own (its patterns
// and alloys) whose secrets the player knows or has begun to learn; makeable ones first
std::vector<Recipe> benchRecipes(art::Building at, const Knowledge& k, const cult::Culture* c, const std::vector<Item>& inv);
// a recipe's display name ("BRONZE INGOT x3", "VETHMARKI IRON SPANGENHELM", "VETHSTEEL GLAIVE")
std::string recipeName(const Recipe& r, const cult::Culture* c);
// which secret a recipe needs (gear: the culture's weapon / armour pattern; smelting a culture alloy: the alloy)
SecretKind secretOf(const Recipe& r);

// ---- learning culture secrets (owner 15.11: trust / apprenticeship, ruins lore, breaking pieces down, theft, quests).
// THE HOOK FOR THE STORY ENGINE (and any quest): call
//     craft::learnSecret(game.craft, culture, alloy, kind, amount, craft::HOW_QUEST)
// to teach (amount 100) or advance a culture's secret; it returns true when the secret became known. `alloy` is 1-based
// for an alloy recipe (0: the culture's armour / weapon pattern in the universal metals). Respects cult::Alloy::learn: an
// alloy that cannot be learned that way gains nothing (HOW_QUEST always may).
bool learnSecret(Knowledge& k, const cult::Culture& c, uint8_t alloy, SecretKind kind, int amount, uint8_t how);
bool howAllowed(const cult::Culture& c, uint8_t alloy, SecretKind kind, uint8_t how);
// the name of a secret for notices ("THE VETHMARKI WEAPON PATTERN", "THE SECRET OF VETHSTEEL")
std::string secretName(const cult::Culture& c, uint8_t alloy, SecretKind kind);
// apprenticeship thresholds (trust with a master smith): the patterns at 60, an alloy at 100
constexpr int TRUST_PATTERN = 60, TRUST_ALLOY = 100;
// (M6 fixer r4) a commission earns trust once per restock: TRUST_COMMISSION below the pattern threshold, then
// TRUST_COMMISSION_HIGH (so patterns take ~5 restocks of commissions, an alloy many more with buying on top)
constexpr int TRUST_COMMISSION = 12, TRUST_COMMISSION_HIGH = 6;
constexpr int TRUST_BUY_CAP = 6;   // (M6 fixer r3) the most trust buying a smith's gear earns per restock (2 per piece)
constexpr int STUDY_PROGRESS = 25;          // breaking a foreign piece down at a forge
constexpr int LORE_PROGRESS = 50;           // reading alloy notes / a mould found in a ruin

// ---- ruins lore: "ALLOY NOTES" and moulds (ItemKind::Misc, sub MISC_LORE; Item::culture / alloy / form = SecretKind)
constexpr uint8_t MISC_LORE = 200;
Item makeLore(const cult::Culture& c, uint8_t alloy, SecretKind kind, uint32_t seed);
inline bool isLore(const Item& it) { return it.kind == ItemKind::Misc && it.sub == MISC_LORE; }

// ---- the forge screen's actions on a Game (rpg/sim/craft_game.cpp; the view and scripts call them; game.h is frozen)
//   benchMake: make Game::bench.recipes[index] (craft::make on the pack), give the result (gear at the settlement's
//   danger, capped by the material's band), remove the emptied stacks keeping the eq* indices valid, grow the smithing
//   skill. false (and *why) when it cannot be made. *made: the item made.
bool benchMake(Game& g, int index, std::string* why = nullptr, Item* made = nullptr);
//   openBench: the forge screen for a station worker (actor id) at a station (art::Building), as craftChoose opens it;
//   scripts and tests use it directly. false when that actor is not in play.
bool openBench(Game& g, int actorId, art::Building at);
//   the culture a bench's station belongs to (its settlement's; nullptr: none)
const cult::Culture* benchCulture(const Game& g);
//   the settlement danger a bench crafts at (crafted gear's item level before the material's cap)
int benchDanger(const Game& g);
//   (tests: rpg_test --loot-audit) what this NPC's shop would stock today (Game::shopStock, which is private)
std::vector<Item> stockOf(Game& g, const Actor& a);
//   (scripts) open the shop of this NPC (actor id) as A_TRADE does; false when the actor is not in play
bool openShopOf(Game& g, int actorId);

}  // namespace craft
