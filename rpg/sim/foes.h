// M6 Steel (VISION_PLAN 7.6): elites with affixes, champion packs, named uniques with rumours, dungeon boss phases,
// roaming world bosses that raid settlements, and the new families' looks. Shared contract, frozen after M6 phase A
// (lead, 2026-10-08): the FOES lane is its only editor (it may ADD declarations; never rename, remove or change the
// meaning of what is listed here) and implements it in rpg/sim/foes.cpp (data, generators) and
// rpg/sim/foes_game.cpp (the Game hooks).
//
// Runtime state lives on the Actor (game.h: rank, affixes, overlays, bodyTint, scalePct, bossPhase, affixT / affixT2,
// unique, pack, culture, dropD; never saved). What must persist (a named unique or a world boss slain) goes in
// Game::marks under the FOES tags (MK_FOES_* below, 64..79), so M6 needs no foes save block.
//
// Determinism: who is an elite, which affixes, where the named uniques and world bosses live, their names, are pure
// functions of the world seed and stable ids / global tiles (ew::cellSeed, hash streams), never of Game::rng_ (the
// player's fights must not shift any other random stream; rpg_test --metrics compares against M5 within 10 %).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/world/ids.h"

namespace ew { class EndlessSource; }

namespace foes {

// ---- ranks (7.6 "Frequency")
enum class Rank : uint8_t {
  Normal,      // an ordinary pack member
  Elite,       // D >= 5, 6 % of pack members: 1-2 affixes, HP x2.2, damage x1.3, +1 rarity roll
  Champion,    // D >= 12, 1 % of packs: a pack of 3 sharing 3 affixes, HP x3.5
  Named,       // a named unique (1-2 per region): 2-3 affixes and a title, HP x6, damage x1.5, Rare+ loot, 15 % legendary
  Boss,        // a dungeon boss (phases at 66 % and 33 %)
  WorldBoss,   // one per kingdom cell: D + 5, HP x25, roams a territory, raids settlements
  COUNT
};
const char* rankName(Rank r);   // "", "ELITE", "CHAMPION", "", "BOSS", "WORLD BOSS"

// ---- elite affixes (7.6 table; each has a tint for its aura)
enum class Affix : uint8_t {
  Swift, Armoured, Vampiric, Frenzied, Splitting, Burning, Frostbound, Warded, Summoner, Blinking, Regenerating, Volatile,
  COUNT
};
constexpr uint16_t affixBit(Affix a) { return (uint16_t)(1u << (unsigned)a); }
struct AffixInfo {
  const char* name;     // "SWIFT", "ARMOURED"... (the name plate: "SWIFT VAMPIRIC WOLF")
  uint32_t aura;        // rgba: the aura's colour (art::auraSprite)
  const char* line;     // what it does, one HUD line
};
const AffixInfo& affixInfo(Affix a);
// the colour an actor's aura takes from its affix bits (the first set affix; 0 none)
uint32_t auraColor(uint16_t affixes);

// ---- variant looks (7.6 "Variant looks"): a body tint and art::MO_* overlays from the land and the affixes, so an
//      elite of the ash fields is an "Ashen Wolf" with ember cracks and a frost-bound one wears rime
struct Variant {
  uint32_t tint = 0;        // art::MonsterLook::tint (0 none)
  uint16_t overlays = 0;    // art::MO_* bits
  uint8_t scalePct = 0;     // draw scale (0 = 100)
  std::string prefix;       // "ASHEN", "CRYSTAL", "MOSSBACK" ("" none)
};
// the variant a monster of `mon` takes at a global tile's eco (as int: rpg/world/biomes.h Eco) with these affixes and rank
Variant variantFor(art::Monster mon, int eco, uint16_t affixes, Rank rank, uint32_t seed);

// ---- named uniques (7.6: 1 to 2 per region, a culture name and title, a lair vignette, a rumour that names it)
struct NamedUnique {
  ew::Gid id = 0;           // stable (ew::makeId(rx, ry, IdKind::Poi, 0x800 + n)): Game::marks keys its death on it
  art::Monster mon = art::Monster::Wolf;
  std::string name;         // "OLD NINEFANGS", "SATHRA THE UNBOWED"
  std::string title;        // "THE UNBOWED" ("" when the name carries it)
  uint16_t affixes = 0;     // Affix bits (2..3)
  Variant look;
  int32_t gx = 0, gy = 0;   // its lair (global tile; a free overworld tile, never in a settlement)
  int D = 1;                // the danger there (+3 on its loot's item level)
  std::string rumour;       // what an innkeeper says of it ("THEY CALL IT OLD NINEFANGS. IT TOOK THE MILLER'S BOY")
};
// the named uniques of region (rx, ry) (deterministic; phase A: none)
std::vector<NamedUnique> namedInRegion(ew::EndlessSource& src, int32_t rx, int32_t ry);

// ---- world bosses (7.6: one per kingdom cell: a dragon, a frost giant, a lich, a wyrm, a behemoth...; D + 5, x25 HP,
//      roams a territory, has a lair, raids settlements as a realm event, news across the horizon when slain)
struct WorldBoss {
  ew::Gid id = 0;           // stable (ew::makeId on the kingdom cell, IdKind::Poi, 0xF00)
  art::Monster mon = art::Monster::Dragon;
  std::string name;         // "ASHFANG", "KHORVAL THE WHITE"
  Variant look;
  int32_t lairX = 0, lairY = 0;   // global tile
  int32_t range = 0;        // territory radius in tiles
  int D = 1;
  // ---- FOES lane additions (M6 phase B)
  std::string title;        // "THE WHITE" (part of name; "" none)
  std::string kind;         // what folk call its kind: "FROST GIANT", "WYRM", "LICH", "BEHEMOTH", "DRAGON"
  uint64_t culture = 0;     // the culture whose tongue named it (cult::CultureId)
  static constexpr int ROUTE = 6;
  int32_t routeX[ROUTE] = {}, routeY[ROUTE] = {};   // its roaming route (global tiles; [0] the lair), routeN used
  int routeN = 0;
};
// the world boss of kingdom cell (kx, ky) (ew::KCELL tiles; deterministic). ok == false: none there (phase A: none)
WorldBoss worldBossOf(ew::EndlessSource& src, int32_t kx, int32_t ky, bool& ok);
// where it is on day `day` at hour `hour` (it roams its territory on a deterministic route: the realm's raids and the
// map's rumour markers agree with where the player meets it)
void worldBossAt(const WorldBoss& b, int day, float hour, int32_t& gx, int32_t& gy);

// ---- dungeon boss phases (7.6: at 66 % and 33 % HP a behaviour from {summon adds, enrage +20 % speed, arena hazard,
//      shield phase})
enum class Phase : uint8_t { None, SummonAdds, Enrage, Hazard, Shield, COUNT };
Phase bossPhaseFor(art::Monster mon, int phaseIndex, uint32_t seed);   // phaseIndex 1 (66 %) or 2 (33 %)

// ---- Game::marks tags for the FOES lane (rpg/sim/game_internal.h Mk: 64..79 belong to M6 FOES)
constexpr uint64_t MK_FOES_NAMED_SLAIN = 64;    // a named unique (its id): the day it was slain
constexpr uint64_t MK_FOES_BOSS_SLAIN = 65;     // a world boss (its id): the day it was slain
constexpr uint64_t MK_FOES_RUMOUR = 66;         // a named unique (its id): the player has heard its rumour (1)
// ---- FOES lane additions (M6 phase B)
constexpr uint64_t MK_FOES_RAIDDAY = 67;        // key id 0: the last game day whose world-boss raids were rolled

// ---- the spawn rolls (pure; foes_game.cpp feeds them hashes of the world seed, the spawn's global tile, the species
//      and the day, so the same den on the same day gives the same pack)
constexpr int ELITE_MIN_D = 5, CHAMPION_MIN_D = 12;
constexpr int ELITE_PER_10K = 600;      // 6 % of pack members
constexpr int CHAMPION_PER_10K = 100;   // 1 % of packs
struct FoeRoll {
  Rank rank = Rank::Normal;
  uint16_t affixes = 0;
};
// a fresh pack member's rank: Elite (6 % at D >= 5, 1-2 affixes) or Normal. human: a bandit (no splitting, summoning,
// blinking or volatile); the dragon and bosses never roll
FoeRoll rollFoe(uint64_t h, int D, art::Monster mon, bool human);
// a champion pack (1 % of packs at D >= 12, keyed by the pack's hash): its 3 shared affixes and a pack id (> 0)
bool championPack(uint64_t packKey, int D, art::Monster mon, bool human, uint16_t& affixes, int& packId);
// can this affix go on this species (the dragon none; bandits no splitting, summoning, blinking or volatile)
bool affixAllowed(Affix f, art::Monster mon, bool human);
// n distinct allowed affixes from hash h
uint16_t pickAffixes(uint64_t h, int n, art::Monster mon, bool human);
// the rank's multipliers (7.6 table): HP x2.2 / x3.5 / x6 / x25, damage x1.3 / x1.3 / x1.5 / x1.6, XP
float rankHpMul(Rank r);
float rankDmgMul(Rank r);
float rankXpMul(Rank r);
// the name plate: "<AFFIXES> <VARIANT PREFIX> <BASE>" ("SWIFT VAMPIRIC ASHEN WOLF")
std::string foeName(const std::string& base, uint16_t affixes, const std::string& prefix);
// what a summoner (or a boss's summon phase) calls, and a splitting foe splits into
art::Monster minionOf(art::Monster m);
// the name of the world boss or named unique of species `mon` (-1: any) that roams near (gx, gy) ("" none): the news
// of a raid or a kill names the beast (rpg/story/rumours.cpp)
// (M6 fixer r3) rank: (int)Rank::WorldBoss looks only at world bosses, (int)Rank::Named only at named uniques, -1 both
std::string beastNameNear(ew::EndlessSource& src, int mon, int32_t gx, int32_t gy, int rank = -1);
// the kind word of a world boss's species ("FROST GIANT", "WYRM", "LICH", "BEHEMOTH", "DRAGON")
const char* worldBossKind(art::Monster m);
// tests: forget the memoised named uniques and world bosses (a second ask then regenerates them)
void clearMemo();
// (M6 integration) whether a region's named uniques / a kingdom cell's world boss are already memoised: the game step
// warms at most one cold generator per tick (a cold one costs a few ms; nine or twenty-five at once hitched the frame)
bool namedReady(ew::EndlessSource& src, int32_t rx, int32_t ry);
bool bossReady(ew::EndlessSource& src, int32_t kx, int32_t ky);
// (M6 fixer r4) one slice of a cold kingdom cell's world boss (its lair search, then on the next call the rest), so a
// tick never pays for both; true once worldBossOf(kx, ky) is memoised (ready)
bool bossWarmStep(ew::EndlessSource& src, int32_t kx, int32_t ky);

}  // namespace foes
