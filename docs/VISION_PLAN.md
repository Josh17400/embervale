# EMBERVALE: the Endless Realms (vision plan and roadmap)

Written 2026-10-03 by the lead architect, after reading `docs/PLAN.md`, `docs/REVIEW.md` and all of `rpg/`, `engine/`,
`tools/` and `web/`. This is the long arc. `docs/PLAN.md` stays the plan for the work in flight: the harness, game feel and
save/world-gen versioning (its tasks 1, 2 and 8). Its tasks 3 to 6 (settlement archetypes, vignettes, dungeon variety and
quest variety) are folded into the milestones below so they are built once, on the endless world, instead of twice.

The non-negotiables stay: C++20 and SDL3, iPhone first (a web build on GitHub Pages today, TestFlight later), Windows for
development, and **every sprite, every note of music and every sound generated in code, with no asset files**.

---

## 0. Decisions at a glance

1. **The endless world is a sliding "Active Window" over a deterministic chunk source.** The sim keeps a flat 256×256-tile
   `Map` with a floating origin. That window is shifted in 64-tile steps as the player walks, and most of today's game
   code (collision, AI, spawning, A*, rendering) keeps working on it unchanged.
2. **There are four generation layers, and none depends on generation order.** The macro layer (continents, plates,
   climate) is a set of pure functions. Rivers come from spring tracing on the pure elevation field. The meso layer is
   *region plans* (256-tile regions holding sites, the road graph and POIs). The micro layer is *chunks* (32×32 tiles,
   the same size as today's terrain bake chunk). Every random choice comes from `cellSeed(world, layerTag, x, y)`.
3. **Structural generation uses integer and fixed-point math and is compiled with precise floating point.** A golden file of
   chunk hashes runs in CI on Windows and Linux, which fixes the "libm differences" caveat in `web.yml`.
4. **Old saves keep the classic 448×448 island** through a `LegacySource` that wraps today's `world.cpp` unchanged. New games
   are endless. During M1 the endless mode is a title-screen preview flag, and M2 makes it the default.
5. **Saves become per-region deltas** (tagged records that later builds can extend). Each region pins its generator
   version the first time it is visited, and the macro version is pinned per save. The target is under 512 KB of save
   after 50 hours.
6. **The culture engine is about 10 hand-authored archetypes plus procedural mutation, not pure noise.** Pure randomness
   makes cultures that don't hang together; archetype priors make them believable. Neighbours are guaranteed to differ by
   a *four-phase checkerboard maximin* selection on the culture lattice, which gives the same answer in any order, and
   the difference is measured by a numeric distance metric that CI fails on.
7. **The art becomes parameterised.** `ArchStyle`, `DressStyle`, `ArmsStyle`, `Heraldry`, `MusicStyle` and `FxStyle` are
   passed into the existing painters, and the sprite caches are keyed by a style hash. The roof redo is the first piece
   (M0).
8. **Kingdoms own settlements, not tiles.** A settlement's *identity and style* are generated and fixed. Its *owner and
   state* are simulated. Territory is drawn as a Voronoi diagram over owned settlements. An abstract daily sim runs for at
   most 48 active kingdoms, and its results appear on the ground within about 300 tiles of the player.
9. **The slow roll is enforced by formulas, not by hoping.** Danger rises with the log of the distance from the origin. An
   item's level comes from the danger where it was found. Equipment is level-synced to player level + 4. XP decays on
   weak kills. Hitting enemies far above your level costs damage. Shops stock for the local danger, never for the player's
   level. Rewards are horizontal (new options) before they are vertical (bigger numbers).
10. **Magic traditions are 12 hand-coded signature mechanics with generated spell lists, names and visuals.** The hidden
    traditions sit late in the game (danger 30+, player level 25+), behind discovery, reputation and a trial.
11. **Actors get a `faction` that replaces the bare `hostile` flag.** It is the basis for monsters attacking villagers,
    guard protection, sieges, wars and companions. It ships in M0 with town defence, so later systems don't retrofit it.
12. **Roadmap:** M0, the now lane (small queued requests and refactors that need no foundation), then M1 and M2 (the endless
    foundation, then parity and the zoomable map), M3 culture, M4 kingdoms, M5 NPC life, M6 arms and enemies, M7 home and
    farm, M8 companions, M9 magic, M10 sea and hidden lands, M11 terrain relief and M12 rise and fall. Each milestone is 1 to
    2 weeks of agent work and ships a playable build to the phone.

---

## 1. Pillars, player fantasy, uniqueness and the slow roll

### 1.1 Pillars
1. **The road always goes on.** Pick a direction and walk for an hour. There is always another valley, another banner on the
   horizon and another name you have never heard.
2. **Every land has a people.** Crossing a border changes the roofs, the clothes, the music, the names, the armour, the
   gods and the food. You notice it in the first 10 seconds.
3. **The world moves without you and remembers you.** Kingdoms meet, trade, fall out, fight and fall. The innkeeper tells
   you about it. Months later you walk past the burned village yourself.
4. **Power is earned.** A level-30 hero is a legend because getting there took 30 hours, not 3. Exploring rewards you with
   new options (magic, gear looks, companions, homes and maps) before it rewards you with bigger numbers.
5. **A place to come home to.** A house, a field, chickens and a companion waiting by the fire. The open road means more
   when there is a hearth.
6. **It reads in 2D on a phone.** The canvas is 480×270. Every system must read at that size through silhouette, colour and
   motion. Never depend on an illusion of height that 3/4 top-down pixel art can't sell (see section 11).

### 1.2 The player fantasy in one paragraph
You wake in a village in the temperate heartland. The first hour is today's "First Hold" (the dragon, the Jarl and the
three shards), and it teaches the game. Then you see that the road east never ends. Within 4 minutes you cross into a
second kingdom with different banners. Within 8 you meet a different people: turf roofs and horn music give way to flat
roofs, drums and curved blades. An innkeeper says the Khaganate has besieged a town two valleys over. You can sell your
sword to either side, buy a plot by the river and plant barley, or follow a sailor's rumour of a kingdom beyond the Mist
whose priests sing fire into being. Twenty hours later you sail there.

### 1.3 What makes each place unique: the uniqueness stack
Distinctness comes from **multiplying independent layers**, each tied to world logic so it reads as meaning, not noise
(PLAN.md principle 2).

| Layer | Driven by | Example | Variants (order of magnitude) |
|---|---|---|---|
| Land | Macro fields: elevation, climate, coast, rivers, ridges | River delta, highland moor, salt coast, ridge pass | 10 biomes now, 17 later, × relief × water |
| People | Culture family (2048-tile lattice) | Turf-roofed Nordic halls against adobe courtyard towns | 10 archetypes × mutation: effectively unbounded, adjacent ones guaranteed distinct |
| Realm | Kingdom (1024-tile lattice) and its dialect of the culture | Banner, palette accent, one building variant, a naming suffix, the government | about 1 per 4 minutes of walking |
| Settlement | Archetype × size × wealth × age × layout style | A poor old fishing hamlet on stilts against a rich young market town on a grid | archetype (8) × layout (7) × wealth (3) × age (3) |
| History | The realm sim | Banners of the conqueror, burned houses, a refugee camp, a new garrison tower | emergent |
| Names | Culture phonology plus landmark naming | "Hrafnstad on the Vela", "Qasr Amun at the Salt Gate" | thousands per culture, never repeated within 3000 tiles |
| Wonders | Rare set pieces, at most 1 per kingdom cell | A colossus, a crater lake, a titan's ribcage, a sunken city | 16 templates with variants |

**Wonders** are the memory anchors of an endless world. Each kingdom cell rolls a 35 % chance of one, the template is picked
by biome, and none repeats within 3 cells. The list: Weeping Colossus (a toppled statue in the people's style), crater lake,
petrified forest, titan's ribcage (bone arches over a road), great chasm with a rope bridge, smoking volcano, salt flats with
mirages, crystal field, sunken city (a coastal ruin at low tide), a world tree, a stone circle of giants, a frozen waterfall,
a canyon of carved faces, a whale-bone shrine, a lightning-struck tower, an obsidian glass desert. Every wonder has a name, a
lore line spread by rumour, a small reward (a lore item or a blessing) and a map icon.

### 1.4 The slow roll philosophy
- **Vertical power** (bigger numbers) comes from three places, each capped: player level (small, +1.5 % damage per level),
  item level (bounded by where you have been, and synced to your level + 4) and magic mastery (gated by use and by trials).
- **Horizontal power** (new options) is the main reward for exploring: magic traditions, culture gear sets with their own
  move quirks, companions, mounts, ships, maps, houses and recipes.
- **The world does not scale to you.** Danger is a property of the place, so walking back home at level 30 feels earned and
  the frontier never stops being frightening.
- **Target pacing** for an average player, measured by the bot and tuned in M6:

| Level | Hours played | Where you usually are | Danger band there |
|---|---|---|---|
| 5 | 0.5 | start kingdom | 1 to 6 |
| 10 | 3 | neighbouring kingdoms | 6 to 12 |
| 15 | 7 | far side of the start culture | 12 to 18 |
| 20 | 12 | second and third culture families | 16 to 22 |
| 25 | 19 | edge of the start continent, first sea crossing | 20 to 27 |
| 30 | 28 | a second continent, a hidden valley | 25 to 32 |
| 40 | 55 | far continents, the Mist | 32 to 42 |
| 50 (cap) | 100 | wherever you like | up to 60 |

---

## 2. Endless world architecture

### 2.1 Units and coordinates

| Unit | Size | Walk time (4.6 tiles/s, today's 74 px/s) | Role |
|---|---|---|---|
| Tile | 16 px | | movement, collision, ground |
| Chunk | 32×32 tiles (512 px) | 7 s | micro generation, terrain bake, streaming, delta storage |
| Region | 256×256 tiles (8×8 chunks) | 56 s | meso plan: sites, road graph, POIs, dens, river index |
| Kingdom cell | 1024 tiles (4×4 regions) | 3.7 min | one kingdom capital candidate |
| Culture cell | 2048 tiles (8×8 regions) | 7.4 min | one culture family |
| Continent cell | 6144 tiles | 22 min | continent, archipelago or open ocean |

- **Global tile coordinates are `int32_t`.** Their range of ±2.1·10⁹ tiles is about 14 years of walking, so 64-bit tile
  coordinates are unnecessary. Positions in saves are `(int32 tile, float sub-tile)`. **IDs are 64-bit** (§2.2). The seed is
  64-bit.
- **Floor division uses arithmetic shifts** (`t >> 5` for the chunk, `t >> 8` for the region), which are well defined for
  negative numbers in C++20. Never use `/` on coordinates that can be negative.
- **Floating origin.** Actor, projectile and pickup positions stay `float` pixels, but *local to the Active Window* (§2.9).
  Float precision at a world distance of 10⁶ tiles (1.6·10⁷ px) is 1 px, which would make movement jitter. Window-local
  coordinates never exceed 4096 px, where precision is 1/2048 px.
- **There is a soft world edge at |coordinate| = 1,000,000 tiles** (about 60 hours of straight walking): "the World's End",
  a storm wall with lore. Nobody will reach it, but it bounds every formula.

```cpp
// rpg/world/coords.h
constexpr int CHUNK = 32, REGION = 256, KCELL = 1024, CCELL = 2048, LCELL = 6144;
struct GTile { int32_t x = 0, y = 0; };
inline int32_t chunkOf(int32_t t)  { return t >> 5; }    // C++20: arithmetic shift == floor division
inline int32_t regionOf(int32_t t) { return t >> 8; }
inline int32_t floorDiv(int32_t a, int32_t b) { int32_t q = a / b; return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q; }
```

### 2.2 Stable identities
Today identity is the *index* into `World::sites` or `Map::bldgs`, plus `npcKey = site<<24 ^ bldg<<12 ^ slot`. In an
endless world indices are per window and short-lived. The fix is 64-bit IDs that encode where a thing was generated:

```cpp
// rpg/world/ids.h   layout: [63..40 rx:24][39..16 ry:24][15..12 kind:4][11..0 local:12]
enum class IdKind : uint8_t { None = 0, Site = 1, Bldg = 2, Den = 3, Npc = 4, Poi = 5, Plot = 6, Edge = 7, Kingdom = 8, Legacy = 15 };
using Gid = uint64_t;
inline Gid makeId(int32_t rx, int32_t ry, IdKind k, uint32_t local) {
  return ((uint64_t)(uint32_t)(rx & 0xFFFFFF) << 40) | ((uint64_t)(uint32_t)(ry & 0xFFFFFF) << 16) |
         ((uint64_t)k << 12) | (local & 0xFFF);
}
inline int32_t idRx(Gid g) { return (int32_t)((g >> 40) << 8) >> 8; }    // sign-extend 24 bits
inline int32_t idRy(Gid g) { return (int32_t)(((g >> 16) & 0xFFFFFF) << 8) >> 8; }
inline IdKind  idKind(Gid g) { return (IdKind)((g >> 12) & 15); }
```
- 24 bits of region coordinate cover ±8.4·10⁶ regions = ±2.1·10⁹ tiles, which matches `int32` tiles.
- At most 4096 sites, buildings, NPCs or POIs per region. A dense region holds about 40 sites, 250 buildings and 600
  residents.
- **Legacy saves** map onto `makeId(0, 0, IdKind::Legacy, index)` (sites) and `Legacy | 0x800 | bldg` (buildings), so one
  code path serves both world types.
- **Runtime handles.** Game code keeps small `int` handles into a `SiteTable` (a slot array with a free list, keyed by
  `Gid`). A handle stays valid while its site is loaded (the window plus one ring of regions), so `world.sites[i]`-style code
  keeps compiling. `Actor::site` stays an `int` handle, and `Actor::siteId` (a `Gid`) is added for persistence.

### 2.3 Seeds and determinism
```cpp
inline uint64_t mix64(uint64_t z) {          // splitmix64 finaliser
  z += 0x9E3779B97F4A7C15ull; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31);
}
constexpr uint64_t tag(const char* s) { uint64_t h = 1469598103934665603ull; while (*s) { h ^= (uint8_t)*s++; h *= 1099511628211ull; } return h; }
inline uint64_t cellSeed(uint64_t world, uint64_t layerTag, int32_t x, int32_t y) {
  return mix64(world ^ mix64(layerTag ^ mix64(((uint64_t)(uint32_t)x << 32) | (uint32_t)y)));
}
// e.g. Rng r(cellSeed(seed, tag("region.villages"), cellX, cellY));
```
**The order-independence contract.** A generated thing may depend only on the following:
1. The pure macro functions.
2. The plans of the cells it overlaps and their 1-ring neighbours, which are themselves built only from (1) and their own
   seeds.
3. Its own cell seed.

It must never depend on which chunk was generated first, the cache state, the player's path, or the shared `rng` stream that
`world.cpp` uses today (versioning rule 2, extended to everything). This is what makes chunks identical however you walk
into them, and §12 tests it.

### 2.4 Deterministic math and noise
- **Structural generation** (anything that decides *where* something is or *whether* it exists: placement, IDs,
  connectivity, walkability) uses integer and fixed-point Q16 math. Cosmetic per-pixel shading in `terrain.cpp` may stay
  float.
- **Fixed-point value noise** is exact on MSVC, clang (iOS) and Emscripten:
```cpp
inline int32_t h16(int32_t x, int32_t y, uint64_t s) { return (int32_t)(mix64(s ^ ((uint64_t)(uint32_t)x << 32 | (uint32_t)y)) >> 48); }
inline int32_t smoothQ(int32_t t) { return (int32_t)(((int64_t)t * t >> 16) * (3 * 65536 - 2 * t) >> 16); }   // 3t²-2t³, Q16
// value noise in Q16 [0, 65536); lattice spacing 2^shift tiles (shift <= 12)
inline int32_t vnoiseQ(int32_t x, int32_t y, int shift, uint64_t s) {
  int32_t m = (1 << shift) - 1, cx = x >> shift, cy = y >> shift;
  int32_t fx = smoothQ(((x & m) << 16) >> shift), fy = smoothQ(((y & m) << 16) >> shift);
  int32_t a = h16(cx, cy, s), b = h16(cx + 1, cy, s), c = h16(cx, cy + 1, s), d = h16(cx + 1, cy + 1, s);
  int32_t ab = a + (int32_t)(((int64_t)(b - a) * fx) >> 16), cd = c + (int32_t)(((int64_t)(d - c) * fx) >> 16);
  return ab + (int32_t)(((int64_t)(cd - ab) * fy) >> 16);
}
// fbmQ = sum of octaves with halving amplitude; domain warp = add (vnoiseQ - 32768) * amp >> 16 to x, y
```
- **Code that keeps floats** (`stampSettlement`'s wandering streets, `blob()`) is cross-platform deterministic as long as
  it uses only + − × ÷ and `sqrt` (IEEE-exact) and no transcendental functions. Replace `std::sin`, `cos`, `atan2`, `pow`,
  `hypot` and `fmod` in `rpg/world/*` with `dsin`, `dcos` and `datan2` (fixed polynomial approximations in
  `rpg/world/dmath.h`).
- **CMake:** compile `rpg/world/*.cpp` with `/fp:precise` on MSVC (the project default is `/fp:fast`) and with
  `-ffp-contract=off` on clang and Emscripten. FMA contraction is the usual cross-platform culprit.
- **Cosmetic per-pixel noise** in `terrain.cpp` takes coordinates *wrapped* at 2²⁰ px. Its float inputs (`px / 7.0f`)
  lose precision after about 200k tiles otherwise. A one-pixel texture seam every 65,536 tiles is invisible.

### 2.5 The generation layer stack

| # | Layer | Unit | Contents | Pure or cached | Budget (native / web) |
|---|---|---|---|---|---|
| L0 | **Macro fields** | any tile | continentalness `C`, plates and ridges, elevation `E`, temperature `T`, moisture `M`, biome | pure functions, plus a region sample cache at 8-tile resolution (32×32 samples) | 0.3 / 1 ms per region of samples |
| L0.5 | **Hydrology** | spring lattice (384 tiles) | river polylines from steepest-descent tracing, lakes at minima | traces cached by spring ID, plus a per-region river index | ≤ 15 ms on a cold miss, amortised ~1 ms |
| L1a | **Lattices** | kingdom cell, culture cell, continent cell | capital candidates, culture family seeds, continent cores | pure, memoised | negligible |
| L1b | **Region plan** | 256×256 | `SitePlan`s (settlements, dungeons, camps, shrines, lair, wonders), dens, vignettes, landmark names, road edges touching the region | cached LRU (64), about 10 to 40 KB each | 3 / 10 ms, sliceable |
| L1c | **Road edges** | edge | Gabriel-graph edges between settlements, a coarse A* path, a Chaikin-smoothed polyline | cached by edge ID | 0.5 / 2 ms per edge |
| L2a | **Settlement buffer** | the settlement's footprint + 6-tile margin | ground, props, walls, buildings, streets mask, census, gate points (today's `stampSettlement`) | cached LRU (16) | 4 / 12 ms per city |
| L2b | **Chunk** | 32×32 | final ground, prop, biome, height, flags, building refs | cached LRU (256 chunks, about 9 KB each) | 1 / 3 ms |
| L3 | **Delta overlay** | region | looted, killed, edits, plots, sim overlays | the save (§3) | ≤ 0.1 ms per chunk |

**L0, the macro fields (pure).**
- *Continent cells (6144 tiles):* each cell's hash picks Continent (55 %), Archipelago (20 %) or Ocean (25 %). A Continent
  cell has a core jittered ±1000 tiles from the cell centre and a radius `R` from 1800 to 3000. **The origin cell is forced
  to a Continent with its core at (0,0) and R = 3200**, so the start continent is about 22 minutes of walking across with
  about 25 kingdoms.
- `C(p) = max over the 3×3 continent cells of (1 − smoothstep(0.75, 1.15, |p + warp(p) − core| / R))`. The warp is a
  600-tile-amplitude domain warp at 2000-tile scale, which makes peninsulas and bays. Add coast detail `0.12·fbm(p/300) +
  0.05·fbm(p/60)`. Archipelago cells add `islands = fbm(p/400) > 0.62`. A tile is land when `C > 0.5`.
- *Plates (1536-tile jittered lattice):* each plate has a drift vector. The distance to a boundary comes from the Voronoi
  F2−F1 distance. Convergent boundaries (the drift dot product with the boundary normal below −0.2) raise **ridges**,
  `ridge = conv · exp(−(d/70)²) · (0.6 + 0.4·ridged(p/120))`. Divergent boundaries lower rift valleys (lake chains). **This
  is what turns today's single grey massif into long ranges with passes.** A pass lattice (280 tiles) knocks a 50-tile notch
  out of the ridge near each pass point.
- `E = 0.30 + 0.5·(C − 0.5) + 0.18·hills(p/180) + ridge − riverCarve`. Ocean depth is the raw `C`. **Rock** (impassable) is
  only where `E > 0.80`. The ridge amplitude is tuned so rock stays at **10 % of land or less**. Most high ground is
  walkable highland with cliffs (section 11).
- `T = 0.55 + 0.35·(2·fbm(p/3500) − 1) − 1.4·max(0, E − 0.45)`. Within 1500 tiles of the origin, `T` is pulled 60 % toward
  0.55 (a temperate start).
- `M = 0.5 + 0.3·(2·fbm(p/2200) − 1) + 0.2·coastProximity − 0.15·rainShadow`. The rain shadow is the ridge value sampled
  200 tiles upwind on a fixed westerly wind, which puts deserts behind ranges.
- **Biome** comes from today's `classify` thresholds on (E, T, M). Each tile also stores a **biome blend byte**, the distance
  to the nearest threshold. It drives the ecotone transitions (the queued "smoother biome transitions", §11.6).

**L0.5, hydrology (spring tracing).** Springs sit on a 384-tile jittered lattice and exist only where `E` is between 0.62 and
0.78. Each spring traces steepest descent on a 16-tile coarse grid of `E' = E + 0.15·C`. The `C` term gives everything a
gentle slope toward its coast and so removes most noise pits. A trace stops at the sea, at a lake, or after 3000 tiles.
Traces that reach the same coarse cell merge naturally, which gives tributaries and confluences. **Width** in tiles is
`1 + floor(log2(number of traces through the cell))`, capped at 4. **A lake** fills a local minimum up to its spill height
within a radius of 40 tiles. The polyline goes through the cell centres with ±6 tiles of hashed jitter and two Chaikin
passes. Everything depends only on the pure `E`. The carving (`E −= 0.04·exp(−d/24)` near rivers) is applied *after*
tracing and is never fed back. Cost: the springs within 3000 tiles of a region (about 200) are traced once each, and an LRU
of traces by spring ID amortises that across regions.

**L1a, lattices.**
- *Kingdom cells (1024 tiles):* the capital is the highest-scoring city candidate in the cell, scored by habitability × hash.
  25 % of cells have no capital ("wildlands", danger +3).
- *Culture cells (2048 tiles):* each cell is a culture family, chosen by the distinctness algorithm in §5.4.
- *Hidden lands:* islands or continents in Ocean or Archipelago cells outside the origin continent cell (r ≥ about 4000, D 30+) get `hidden = true` (25 %
  of eligible ones). Their culture comes from a reserved "isolated" pool and is never shared with the mainland. *Hidden
  valleys* are landlocked basins on the start continent ringed by ridge with a single pass, in 1 of 12 kingdom cells. They
  are the mid-game version of the same fantasy.

**L1b, the region plan.**
```cpp
struct SitePlan {
  Gid id; SiteType type; int32_t gx, gy;  // footprint origin (global tile)
  uint8_t w, h; uint16_t danger; uint32_t seed;
  uint32_t culture;      // founding culture (fixed forever)
  uint32_t genesisOwner; // kingdom at world genesis (the sim may change the current owner)
  uint8_t archetype;     // Fishing, Port, Mining, Farming, RiverCrossing, HillFort, Market, Plain
  uint8_t layoutStyle;   // from the culture: Organic, Grid, Radial, Linear, Compound, Terraced, Stilt
  uint8_t wealth, age, flags;   // flags: mainQuest, wonder, hidden, capital, port
  std::string name;
};
struct RegionPlan {
  int32_t rx, ry; uint16_t genVer; uint32_t fingerprint;
  std::vector<SitePlan> sites; std::vector<DenPlan> dens; std::vector<PoiPlan> vignettes;
  std::vector<Gid> roadEdges;         // IDs into the edge cache (edges whose polyline touches the region)
  std::vector<RiverSeg> rivers;       // clipped polylines + widths, from the trace cache
  std::vector<Landmark> landmarks;    // named peaks, lakes, forests (culture phonology)
};
```
- **Settlement placement uses central-place lattices with built-in spacing.** Each class has one candidate per lattice cell,
  jittered only inside the cell's centre so that spacing across cells is guaranteed. The point lies in `[m, S − m]` with
  `m = minSpacing / 2`.

| Class | Cell S | Min spacing | Accept probability | Footprint |
|---|---|---|---|---|
| City | 512 | 110 | 0.65 · habitability, also a capital candidate | 46×36 |
| Town | 128 | 64 | 0.55 · habitability | 32×24 |
| Village | 96 | 44 | 0.45 · habitability | 24×18 |
| Hamlet or farmstead | 64 | 24 | 0.30 · habitability | 12×10 |

  `SiteType` gains `Hamlet` and `Wonder` here, and `RuinedTown` in M4. A port is an archetype, not a type.

  Habitability is `flat · water(river/lake/coast within 12) · climate(T, M) · (1 − rock) · (E < 0.7)`. **Conflicts between
  classes** go to the higher class: a lower-class candidate checks only higher-class candidates in neighbouring cells, which
  are pure, so no chain forms. The expected yield per region is about 0.35 cities, 1.2 towns, 3.5 villages and 5 hamlets.
  That matches today's density (18 settlements per 448²) and adds hamlets.
- **The settlement archetype comes from context:**

| Archetype | Condition |
|---|---|
| Fishing (village) or Port (town and up) | coast within 6 tiles |
| River crossing | a road edge crosses a river inside the footprint |
| Mining | rock or ridge within 20 tiles |
| Hill fort | a local `E` maximum |
| Market | road degree 3 or more |
| Farming | high `M`, low slope |
| Plain | otherwise |

  The archetype changes the street generator's parameters, the building mix and the props, as in PLAN.md task 3.
- **Dungeons and other sites per region**, with the counts scaled by biome and danger:

| Site kind | Per region | Placement rule (today's) |
|---|---|---|
| Cave | about 6 | cliff faces (same rule as today) |
| Ruin | about 3 | open ground |
| Bandit camp | about 3 | open ground |
| Shrine | about 2 | open ground |
| Den | about 12 | today's `dens()` rules |
| Vignette | about 10 | PLAN.md task 4 templates |

  Each site is placed in its own sub-cell lattice with its own tag. One dragon lair per 2×2 kingdom cells, on the highest
  ridge cell.
- **Names** come from the founding culture's phonology (§5.2). No duplicates are allowed inside the 3×3 regions: a lower-ID
  duplicate gets a deterministic disambiguator ("Upper", "Old", "East" and so on, in the culture's own words).

**L1c, roads.**
- **Graph.** Gabriel edges between settlements, capped at 300 tiles. The Gabriel test for edge AB needs every node inside the
  circle on AB. With AB ≤ 300 that circle lies within one region of A, so the 3×3 region neighbourhood suffices and the
  graph is computable anywhere. Gabriel graphs contain the Euclidean MST, so settlements on one landmass stay connected.
- **Classes.** City to city or town is a *highway* (2 wide, `Road`). A village edge is a *road* (1 wide, `Road`). A
  hamlet edge or POI spur is a *track* (`Dirt`). Every non-settlement site gets a spur to the nearest edge within 80 tiles.
- **Path.** A* on a 4-tile coarse grid over the edge's bounding box plus 64 tiles, 8-connected. Costs: flat 1, slope ×(1 +
  8·slope), forest 1.4, swamp 2.5, a river crossing 12 (a ford if the width is 1, a bridge otherwise, 30 for width 4 or
  more), rock 6 (cuts a pass), and +0.25 per turn. Then two Chaikin passes and 8-connected rasterisation with diagonal
  steps filled so the road stays 4-connected (PLAN.md's curving-roads item). The costs come only from pure fields, so any
  region computes the same road.
- **Settlement gates.** The end point of a road at a settlement is the footprint edge point facing the neighbour. The
  settlement generator points its main streets at its actual road bearings. Today it uses a random `a0`.

**L2a, the settlement buffer.** `stampSettlement` moves, nearly verbatim, into `rpg/world/settlement.cpp`. It operates on a
local `Map` the size of the footprint plus a 6-tile margin, pre-filled from the base terrain (L0 plus rivers). Its inputs
are the `SitePlan`, the culture's `ArchStyle` and layout style, and the road gate bearings. Its outputs are tiles, buildings
(as `Gid`s), the streets mask, the census (section 10) and props. The organic generator, the owner's standing direction,
survives intact and gains styles.

**L2b, the chunk pipeline.** Each step reads only plans and pure functions, so the order of steps is fixed but the order of
chunks is free:
1. Base: for each tile, `E`, `T` and `M` bilinear from the region sample cache (9×9 samples per chunk), then the biome,
   ground, height level (0 to 7) and blend byte.
2. Rivers and lakes rasterised from the region's `RiverSeg`s (water, banks, fords).
3. Settlement buffers that overlap the chunk are copied in (generated on a miss).
4. Site and vignette templates are stamped for the part of their footprint inside the chunk (each stamp is a pure function
   of its plan).
5. Roads and tracks are rasterised, clearing props, with bridges over water.
6. Vegetation by per-tile hash (a port of `vegetation()`), skipping reserved tiles, roads and margins.
7. Relief (M11): cliffs, ramps and slopes from height levels.
8. The region delta is applied (§3).

### 2.6 The start guarantee
The first hour must stay as good as PLAN.md's "First Hold" demands. Within 1500 tiles of the origin, the macro bias above
makes the land temperate. The origin region uses a **start plan** that searches the macro field, deterministically, for:
- the start village in Plains or Forest, at least 80 tiles from the sea;
- a city with a Keep (the capital of the start kingdom) 120 to 220 tiles away;
- 3 ruins at 150 to 350 tiles for the Ember Shards, at danger 4 to 9;
- a ridge cell 300 to 500 tiles away for Ashfang's lair;
- at least 8 POI kinds within 60 tiles (today's audit), with vignettes forced in if needed;
- no rock wall between the start and the capital, guaranteed by the road graph.

The main quest code keeps working through `world.capital`, `world.lair` and `mainQuest` flags, which now point at `SitePlan`
IDs.

### 2.7 Connectivity by construction
`connectAll()` floods the whole 448² map. An endless world cannot flood everything, so walkability is guaranteed by these
rules instead:
1. Roads always carve through rock and props (the `road(..., allowRock)` behaviour today).
2. Every site has a spur to the road graph, or sits within 80 tiles of an edge with no rock on the line.
3. Relief ramps sit on a global lattice (§11.4).
4. Dens and vignettes only put solid props on tiles whose eight neighbours are open (today's rule).

**The test:** `rpg_test --endless` floods 1024×1024 windows at 30 sampled places per seed and fails if any settlement or
story site in the inner 768×768 can't be reached from the road graph on the same landmass.

### 2.8 Streaming, caches and budgets

| Cache | Entries | Memory | Eviction |
|---|---|---|---|
| Macro samples (per region, 32×32 × 8 B) | 64 | 0.5 MB | LRU |
| Region plans | 64 | about 2 MB | LRU (never evict the 3×3 regions around the window) |
| Road edges | 512 | about 1 MB | LRU |
| River traces | 512 | about 1 MB | LRU |
| Settlement buffers | 16 | about 0.6 MB | LRU (pinned while overlapping the window) |
| Chunks (9 B per tile) | 256 | 2.3 MB | LRU (window + 2-chunk prefetch ring pinned) |
| Terrain textures (512² RGBA) | 16 on web, 24 native | 16 to 24 MB GPU | LRU (today's `chunks_`) |
| Building, human and prop sprites by style key | 400 | ≤ 12 MB GPU | LRU |

**Throughput.** On foot the player crosses a chunk every 7 s; mounted (8 tiles/s) it is 4 s. A window shift needs 16 new
chunks (two columns of 8), *already prefetched*. Steady state is at most 2 chunks/s when mounted (16 chunks per 64-tile shift every 8 s), about 6 ms/s on web, well inside a
3 ms-per-frame budget.

**Jobs.** `rpg/world/jobs.h` defines `struct Job { virtual bool step(double budgetMs) = 0; int priority; }`.
- *Native (desktop, iOS):* one generation worker thread owns the world caches. The main thread posts requests and receives
  finished `ChunkData` through a done queue, the same pattern as today's bake worker.
- *Web (no pthreads):* GitHub Pages can't send the COOP/COEP headers that SharedArrayBuffer needs, so the main loop calls
  `jobs.pump(3.0)` for generation and `pumpBake(3.0)` for terrain each frame. Every job is sliced: one region plan is
  split into sites, then roads per edge, then dens.
- *Teleports* (fast travel, respawn, load) happen behind the existing fade. The 3×3 chunks around the target and their
  region plans are generated synchronously. The budget is 300 ms on web.

**Terrain bake.** Today's per-chunk bake takes a `shared_ptr<const Map>` snapshot of the *whole* map. That becomes a
**34×34-tile snapshot** (the chunk plus a 1-tile margin, enough for the domain warp of ≤ 7 px and the `rockLevel` look-up of
≤ 3 tiles; use a 4-tile margin to be safe, 40×40). The texture key becomes the *global* chunk coordinate, so textures
survive window shifts.

### 2.9 The Active Window, and what the 448×448 `World` becomes
```cpp
// rpg/world/window.h
struct ActiveWindow {
  static constexpr int N = 256;             // tiles; 8x8 chunks, 590 KB of tile data
  int32_t ox = 0, oy = 0;                   // global tile of local (0,0); always a multiple of 32
  Map map;                                  // exactly today's Map: ground/prop/solid/wall/bldgAt/biome (+height, +flags)
  GTile toGlobal(int lx, int ly) const { return {ox + lx, oy + ly}; }
  bool  toLocal(GTile g, int& lx, int& ly) const { lx = g.x - ox; ly = g.y - oy; return map.in(lx, ly); }
};
```
- `World::over` *is* `window.map`. Every `world.over.at(tx, ty)` with window-local tiles keeps working.
- **Recentre.** Keep the player inside the central 128×128 box. When they leave it, shift by ±64 tiles on that axis:

```cpp
void Game::maybeRecentre() {
  int tx = (int)(pl().p.x / TILE), ty = (int)(pl().p.y / TILE);
  int sx = tx < 64 ? -64 : tx >= 192 ? 64 : 0, sy = ty < 64 ? -64 : ty >= 192 ? 64 : 0;
  if (!sx && !sy) return;
  world.shiftWindow(sx, sy);        // memmove kept rows/cols, copy 16 prefetched chunks, rebuildSolid on new strips,
                                    // rebuild bldgs/bldgAt for the window, refresh the SiteTable for regions that entered or left
  Vec2 d(-sx * (float)TILE, -sy * (float)TILE);
  for (Actor& a : actors) { a.p += d; a.home += d; a.goal += d; }
  for (auto& pr : projs) pr.p += d;
  for (auto& pk : pickups) pk.p += d;
  emit(Ev::WindowShift, d);         // View: camera, particles and float texts move by d; chunk textures are keyed globally
}
```
- **Interiors, caves and ruins stay separate sub-maps** (`Game::sub`), exactly as now. Their seeds come from `Gid`s.
- **`World` becomes a façade:**

```cpp
struct WorldSource {                               // rpg/world/source.h
  virtual ~WorldSource() = default;
  virtual void chunk(int32_t cx, int32_t cy, ChunkData& out) = 0;
  virtual const RegionPlan& region(int32_t rx, int32_t ry) = 0;
  virtual int danger(GTile t) = 0;
  virtual MacroSample macro(GTile t) = 0;            // world map, minimap, rumours
};
class LegacySource;   // wraps today's world.cpp: generate(seed, genVer) once, serve 448² cut into chunks, DeepWater outside
class EndlessSource;  // L0..L2 above
```

### 2.10 Migration path from the current code (the M1 tasks, in order)
1. **Mechanical prep (M0):** split `art.cpp` (5225 lines) into `rpg/art/*.cpp`. Add `coords.h`, `ids.h`, `dmath.h`, the noise
   functions and `jobs.h` (unused). Add chunk-hash test plumbing. Nothing changes in behaviour.
2. **IDs everywhere.** Add `Site::id`, `Bldg::id` and `Actor::siteId`. Quests store `targetId` and `giverId` (and keep the
   ints for v1 and v2 loads). Replace `npcKey` with `NpcId`. `killedSlots`, `looted` and `npcQuestsDone` are re-keyed by
   `Gid`. **`LegacySource` produces the legacy IDs**, so classic games behave bit-for-bit as before (`save_test` must stay
   green).
3. **`SiteTable` with handles** replaces `std::vector<Site>`. `siteAt`, `nearestSite` and `zoneLevel` become spatial queries
   over loaded regions using global tiles.
4. **Active Window and floating origin** (§2.9), first with `LegacySource`: the classic island streams through the window.
   This proves the plumbing on a world whose correct output is known.
5. **View:** chunk textures keyed by global chunk coordinates, the per-chunk bake snapshot, camera and particle shift on
   `Ev::WindowShift`, and the minimap reading the window.
6. **The endless SAVE_VER** (§3; 3 or 4, depending on whether M0's appearance bump lands first), which loads v1 and v2 into `LegacySource` games.
7. **`EndlessSource` L0 and L2 base:** endless terrain, biomes and vegetation.
8. **L1b, L2a and L2b settlements:** the region plan's settlement lattice, `settlement.cpp` (extracted), and the census as
   today's folk and guard spawns.
9. **The title screen:** "NEW JOURNEY (PREVIEW)" starts an endless world; "NEW GAME" stays classic until M2.

### 2.11 Quests, markers, fast travel and the map in an unbounded world
- **Quests** store `targetId` (`Gid`) and `targetPos` (`GTile`), so a marker never needs the target generated. The marker
  vector is `targetPos − playerGlobal`, computed with `int64`. Distance is shown as walking time ("~3 MIN").
- **Radiant target search** queries region plans within 768 tiles (cheap: no tiles needed) and filters by danger ≤ player
  level + 3. Anything harder is offered with the warning "IT IS DANGEROUS COUNTRY" and +50 % reward.
- **The known-sites table** in the save (`Gid`, type, `GTile`, name, flags) feeds the map and fast travel without
  regenerating anything.
- **Fast travel** goes to discovered settlements, ports and cleared dungeons *on the same landmass*. The travel time is
  `distance / (road speed 30 tiles/hour)`, so long hops cost days, and the sim ticks during them. A "carriage" option in
  towns costs `0.1 gold per tile` (a gold sink) and needs a road connection. Over the sea it is ferries (§2.12).
- **The zoomable map** (queued request, M2) has four zoom levels, rendered into cached 128×128-px tiles (a quadtree) and
  pannable by drag or the D-pad:

| Zoom | Scale | Source | Shows |
|---|---|---|---|
| Z0 | 1 px = 1 tile | chunk data | streets, buildings, props |
| Z1 | 1 px = 4 tiles | chunk data where explored, macro where not | roads, rivers, sites |
| Z2 | 1 px = 16 tiles | macro cache + region plans | kingdom borders, named landmarks |
| Z3 | 1 px = 64 tiles | macro sampled on the fly | continents, seas, culture regions you have heard of |

  Fog of war is an *explored mask* of 1 bit per 8×8-tile cell (128 B per region). Unexplored areas are parchment with only
  rumoured icons, drawn as a "?" circle. Pinch-zoom on touch, mouse wheel and Q/E on desktop. The map adds hillshading and
  mountain glyphs (§11.5).
- **Markers on the map:** the quest target, discovered sites, the player's house and plots, companions waiting somewhere,
  known wars (crossed swords), rumours (dashed circles) and wonders.

### 2.12 Sea travel and the discovery of distant and hidden lands
- **Boats.** Boarding is an Actor mode `Sailing`. Water tiles become walkable, land tiles solid, and a ship sprite replaces
  the body (generated, with the culture's sail colours and heraldry).

| Vessel | Speed | Can enter | Bought at | Price (gold) |
|---|---|---|---|---|
| Rowboat | 6 tiles/s | `Water` only (rivers, lakes, coast) | any fishing village | 300 |
| Sloop | 10 tiles/s, ±4 with the wind (a weather field) | `DeepWater` | ports | 4,000 |
| Galleon | 9 tiles/s, holds companions and cargo | storm seas | port cities | 20,000 |

  Boats are moored at the dock they were left at (persisted as a `Gid` of the dock plus a position).
- **Ferries:** known port-to-port routes act as fast travel by sea, at 20 to 400 gold depending on distance. A route is
  learned by visiting both ends or from harbourmasters.
- **Sea content:** shipwrecks (a loot vignette on reefs), sea serpents and krakens (world bosses, §7.6), pirates (a
  faction), uncharted islets (one POI each), and storms (a weather front event that pushes the ship and darkens the screen).
- **Discovery.** Distant lands show on the Z3 map only once rumoured. Sources: tavern talk, cartographers (selling a
  kingdom's map for 200 to 1500 gold, revealing its macro and major sites), and explorers' guild quests.
- **The Mist (hidden lands).** Hidden continents and islands are surrounded by a Mist ring 300 tiles wide. Sailing into it
  without the land's *sea-chart* turns the ship around after 20 s of fog. The chart is the reward of a 3-step rumour chain
  (an old sailor, a wreck, a sunken chart in a sea cave). Hidden valleys use the same idea on land: the single pass is
  hidden behind a waterfall or a rockslide prop that the chart lets you clear.
- **First contact.** A hidden kingdom has no relations with the mainland sim (`isolated = true`) until the player lands. It
  then appears on the map, and the player can carry word home ("Tell King X of the land beyond the Mist"). That is the
  owner's "kingdoms discover kingdoms", with the player as the spark.

---

## 3. Persistence: delta saves per region

### 3.1 What is stored and what is regenerated

| Regenerated from the seed (never stored) | Stored |
|---|---|
| Terrain, biomes, rivers, roads | Player: stats, inventory, equipment, global position, window origin |
| Settlement layouts, buildings, interiors | Quests (`Gid` targets + `GTile` positions) |
| Dungeon layouts, dens, vignettes | Known sites and the explored mask |
| Census (who lives where), base looks | Region deltas: flags, kills, loot, edits, plots, overlays |
| Cultures, kingdom genesis state, history pre-roll | Realm sim state: kingdoms, relations, settlement states, events, RNG state |
| Shop stock (hash of NPC + day / 2) | Shop caches currently open (as now, by `NpcId`) |
| | Companions, boats, mounts, houses and farms |

### 3.2 Save layout (the endless SAVE_VER, called v3 below)
This builds directly on the versioning landing now (SAVE_VER 2 with genVersion and fingerprint, `tests/fixtures/save_v1.bin`
and `save_test`).
```
header   magic 'EMBV', ver=3, worldMode (0 Legacy | 1 Endless), seed u64,
         legacyGenVer u32 (Legacy) | macroVer u16 + latestRegionGen u16 (Endless), fingerprint u32 (Legacy)
player   as v2, but position = (i32 gx, i32 gy, f32 fx, f32 fy); window origin (i32, i32); appearance block (M0 creator)
quests   v2 fields + targetId u64, targetPos (i32, i32), giverId u64 (NpcId)
known    count, then {Gid, u8 type, i32 gx, i32 gy, str name, u8 flags}            // map + fast travel
explored count, then {i32 rx, i32 ry, 128 B bitmask}                                  // RLE'd if mostly full
regions  count, then {i32 rx, i32 ry, u16 genVer, u32 planFingerprint, u16 nRecords, records[]}
realm    sim block (section 4): kingdoms[], relations[], settlementStates[], wars[], events[] (last 256), rng u64
home     plots[], houses[], storage[] (or inside region records), boats[], mounts[]
party    companions[]
```
**Region records** are tagged (`u8 tag, u16 len, payload`). Unknown tags are skipped, so a save from build N loads in build N+1
and keeps unknown records verbatim when re-saved.

| Tag | Payload | Typical size |
|---|---|---|
| `SITE_FLAGS` | for each site touched: local u12 + {discovered, cleared, looted boss chest} | 2 B per site |
| `KILLED` | site local u12 + a bitset of spawn slots (no respawn), or a den + the day it was cleared | 4 to 12 B |
| `LOOTED` | chunk index (u6) + chunk-local tile (u10) per chest | 2 B per chest |
| `INTERIOR_LOOT` | Bldg local u12 + tile u16 list | 4 B + 2 B per chest |
| `TILE_EDIT` | chunk-local tile + new ground/prop (player edits, farmland) | 4 B per tile |
| `PLOT` | plot ID, owner flag, house blueprint, placed objects[], crops[], animals[] (section 8) | 0.2 to 3 KB |
| `NPC_STATE` | NpcId local + {dead, recruited, moved-to Gid, mood} | 3 to 11 B |
| `SETTLEMENT_OVERLAY` | site local + the materialised sim state (owner, damage, siege camp seed) | 12 B |

### 3.3 Size budgets

| Content | Budget |
|---|---|
| Untouched visited region | 0 B (only the explored mask) |
| Lightly played region | 50 to 300 B |
| Region with the player's farm | ≤ 4 KB |
| Realm sim (48 active + 200 dormant kingdoms × about 200 B, 2000 settlement states × 12 B, 256 events × 24 B) | about 80 KB |
| **Whole save** | **≤ 512 KB at 50 hours; hard cap 4 MB** (IndexedDB copes; the iPhone app container does too) |

Write cost: autosave every 5 minutes and on `WILL_ENTER_BACKGROUND`, as now, at ≤ 10 ms to serialise 512 KB. An optional
LZ-style compressor can come later. It isn't needed below 1 MB.

### 3.4 Versioning: three version numbers
1. **`SAVE_VER`** covers the byte layout. Today's rules apply: a bump, `ver >= N`-gated reads, fixtures in `tests/fixtures/`
   and `save_test` round trips. Add the fixtures `save_v2.bin` (classic) and `save_endless.bin` (endless, with plots,
   companions and a war).
2. **`macroVer`** covers L0 and L0.5: continents, rivers and the road graph, everything that crosses region borders. It is
   **pinned per save at new game** and never changes for that save. A new macro generator is a new world type; old saves
   keep theirs. Code keeps every `macroVer` path (`if (macroVer >= 2)`), exactly like the WORLDGEN rule in `world.cpp`.
3. **`RegionDelta.genVer`** covers L1b and L2 inside a region: settlement styles, props, vignettes, dungeon templates.
   Pinning:
   - A region is pinned to `ENDLESS_GEN_LATEST` the **first time a chunk of it enters the Active Window**, and that pin is
     written into the region record.
   - Unvisited regions always generate with the latest version, so updates enrich the unexplored world.
   - Visited regions keep the exact layout their quests, chests and NPC identities point to.
   - A small seam at a border between regions of two versions (vegetation density, say) is accepted. Cross-border structure
     belongs to `macroVer`, so roads and rivers always line up.
4. **The plan fingerprint** is a hash of site IDs, types and positions, stored per region. A mismatch on load means a
   generator broke its version rule. The game keeps the save, drops the index-based records for that region, and shows
   "the land has shifted" once. CI fails before that can ship (`save_test` over the fixtures).

**Legacy saves (v1 and v2)** load into `LegacySource` at their `WORLDGEN_V1/V2`. `site index i` becomes `makeId(0, 0,
Legacy, i)`, `mapKey` becomes a Gid, and `looted` tile indices become global tiles (the island sits at the origin, so the
global tile equals the old tile). The fingerprint check works as it does today.

---

## 4. The living world simulation

### 4.1 Data model
```cpp
enum class Gov : uint8_t { Monarchy, Council, Theocracy, Horde, MerchantRepublic, Magocracy };
enum class Rel : uint8_t { Unknown, Contact, Peace, Trade, Alliance, Rivalry, War, Vassal, Overlord };
struct Ruler { std::string name; uint8_t age; uint8_t traits; /* Aggressive, Cautious, Greedy, Pious, Scholarly, Honourable */ uint16_t sinceDay; };
struct Kingdom {
  Gid id; Gid capital; uint32_t culture; std::string name; Heraldry arms; Gov gov; Ruler ruler;
  float pop;          // thousands (village 0.1-0.3, town 0.6-1.5, city 3-8)
  float wealth;       // treasury, gold-equivalent / 100
  float military;     // strength points (1 point = a company of ~25)
  float stability;    // 0..1
  float exhaustion;   // war weariness 0..1
  uint8_t values[8];  // from culture: martial, mercantile, pious, scholarly, seafaring, expansionist, isolationist, honour (0..255)
  bool isolated, fallen, dormant;
  uint16_t foundedDay; int16_t playerRep;   // -100..100
  std::vector<Gid> settlements;              // owned (the territory)
};
struct Relation { Gid a, b; Rel state; int8_t opinion; uint16_t sinceDay; uint8_t border; /* shared border settlements */ };
struct War { uint32_t id; Gid attacker, defender; uint16_t startDay; float score; /* -1..1, +attacker */ std::vector<uint32_t> sieges; uint8_t goal; /* Conquer, Raid, Tribute, Subjugate */ };
struct Siege { uint32_t id; Gid site, attackerK; float atk, def; uint16_t startDay, resolveDay; bool playerJoined; uint8_t side; };
struct SettlementState { Gid site; Gid owner; uint8_t popPct, prosperity, damage, garrison; uint16_t changedDay; uint8_t flags; /* Besieged, Burned, Abandoned, Occupied, RefugeeCamp */ };
struct WorldEvent { uint16_t day; uint8_t type; Gid a, b, site; int16_t mag; };
```

### 4.2 Genesis and the history pre-roll (deterministic, lazy)
- A kingdom is **instantiated** when its capital comes within 3 kingdom cells (about 3000 tiles) of the player. Genesis is a
  pure function of its seed and the *genesis* data of its neighbours: its settlements (the nearest-capital assignment over
  pure `SitePlan`s, weighted by capital size, ×2 cost across water, with a maximum reach of 900 tiles), its population,
  wealth and military from settlement sizes and culture values, its government from its values, and a ruler.
- A **history pre-roll** produces 40 to 200 years of *text-only* backstory from hashed templates over neighbours' genesis
  names: "the War of the Salt Crown, 40 winters ago". It also decides which nearby ruins belong to *extinct* kingdoms,
  with their names and the culture of their draugr. This gives lore at zero simulation cost.
- **The live sim starts** at the instantiation day. Relations begin at `Contact` with genesis neighbours (shared border
  settlements) and at `Unknown` otherwise.

### 4.3 The daily tick (one per in-game day, 14 real minutes)
Kingdoms are processed in `Gid` order, using the sim's own saved RNG stream. Abstract numbers:

| Quantity | Daily update |
|---|---|
| Population | `pop += pop·0.002·(prosperity − 0.4)·stability`, capped at the summed settlement capacity |
| Income | `pop·tax(gov)·(0.6 + 0.4·tradePartners/4)` |
| Upkeep | `military·0.8` |
| Wealth | `wealth += income − upkeep` |
| Recruiting | when `wealth > reserve`: `military += min(pop·0.01·martial, wealth·0.05)` |
| Stability drift | toward `0.7 + 0.2·rulerHonour − 0.3·exhaustion − 0.2·(recent capital loss)` |
| Opinion drift | toward `base = −40·cultureDistance + 15·sameReligion + 10·trade − 8·borderFriction` |
| Discovery | kingdom A *contacts* B when A's reach (300 + 100·seafaring/255·hasPort + 50·mercantile/255 tiles around its settlements) overlaps B's territory; across water only with seafaring ≥ 128 and a port. First contact is an event and a rumour |
| Trade | `Contact` becomes `Trade` with `P = 0.02·mercantile·(opinion > 0)` per day, which gives +income for both |
| War declaration | `P/day = 0.006·σ(aggr + (opinion < −30) + 1.5·(strengthRatio > 1.3) + borderDispute − 2·alliance)`, where `aggr` comes from ruler traits and values. A Horde government declares *raids* (goal Raid) |
| Siege start | each war day, the stronger side starts a siege on the enemy's nearest border settlement with `P = 0.25·strengthRatio`. Strength committed is `min(0.5·military, 4·garrison)` |
| Siege resolution | after `3 + garrison/2 + 4·walls` days, a roll weighted `atk/(atk + def·(walls ? 1.5 : 1))`. The player's contribution adds to `atk` or `def` |
| On capture (Conquer) | the owner changes, `damage += 30`, `popPct −= 15`, and the settlement is `Occupied` for 30 days (resentment, rebellion risk) |
| On capture (Raid) | `Burned` (`damage = 80`, `popPct −= 50`), and refugees go to the nearest friendly settlement (`RefugeeCamp` flag there) |
| Peace | when `exhaustion > 0.7` or `|score| > 0.6`, a treaty cedes the occupied settlements, plus tribute (wealth transfer) |
| Fall | 0 settlements makes a kingdom `fallen`. Losing the capital means a new capital, stability −0.4, and `P(civil war) = 0.05/day` while `stability < 0.2` |
| Rise | a civil war splits the realm: the rebel half becomes a new kingdom with the same culture, a new dynasty name and new heraldry, and a new `Gid` from `makeId(rx, ry, Kingdom, 0x800 + n)`. A strong bandit camp (cleared by nobody for 30 days) in wildlands can become a warlord realm. An abandoned settlement is resettled after 60 days by the nearest kingdom with `expansionist > 160` |

**The tuning target is "lively", one of the owner's questions in §14.** In a 48-kingdom horizon:
- 2 to 5 wars are active at once, each lasting 15 to 40 days;
- a settlement changes hands somewhere in the horizon about every 4 to 8 days;
- one kingdom falls or splits every 40 to 90 days;
- one realm in 4 is still alive after 300 days with its original ruler.

The soak tests check these (§12.5).

### 4.4 Level of detail

| Tier | Where | What runs |
|---|---|---|
| A, present | within about 300 tiles of the player (window + 1 region) | concrete: siege camps, soldiers, refugees and patrols are actors; fights between actors feed `atk` and `def` |
| B, active | kingdoms with a capital within 3 kingdom cells, **at most 48** (nearest first), plus any the player has reputation with | the daily abstract tick |
| C, dormant | instantiated but beyond B | frozen. On re-entry, a catch-up of `min(daysAway / 7, 50)` weekly ticks at 7× rates |
| D, latent | never instantiated | nothing; genesis happens on demand |

Wars between an active and a dormant kingdom pause on the dormant side.

### 4.5 How sim outcomes appear on the map
**Identity never changes. Only overlays do.** When a settlement buffer or chunk is generated, or when the player comes
within 300 tiles, the `SettlementState` is applied:
- **Owner:** banner props, the keep's wall banners, guards' tabards and helmets (the owner culture's arms grammar), gate
  heraldry and the map border colour. A conqueror of a different culture adds a *garrison tower* in its own architecture
  at a hash-chosen free lot. That is a striking image: a turf-roofed town with a domed watchtower over it.
- **Damage** (0 to 100): `damage/100 × 0.7` of the buildings, chosen by hash, draw a *charred variant* of their sprite
  (blackened roof with holes, soot). Some become `Rubble` props with `Ash` ground decals. Burned houses can't be entered and
  their residents are absent. Damage heals by 1 point per day while not besieged, with scaffolding props while it rebuilds.
- **Besieged:** a ring of siege-camp stamps 14 to 30 tiles outside the walls in the attacker's colours (tents, campfires,
  a catapult prop, a stake palisade), attacker soldiers patrolling, guards on the walls, closed gates (a wall piece that
  opens if the player helps the defenders), and the music switches to `Siege`. The player can talk to either commander:
  "Break the siege" (kill 12 attackers and the captain, which adds `def`) or "Join the assault" (open the gate, which adds
  `atk`).
- **Burned or abandoned:** after 30 days with nobody resettling, the settlement becomes a new site type `RuinedTown`
  (bandits or monsters move in, the name becomes "the ruins of Hrafnstad"), a dungeon-like quest target.
- **Refugees:** walkers on roads with carts, heading for the refugee camp (tents outside a friendly town). They can give
  quests ("my daughter is still in the village").
- **War fronts:** patrols of the warring kingdoms walk the roads between the two realms. Checkpoints sit at border bridges.
  Enemy patrols are hostile if the player's reputation with them is ≤ −25.

### 4.6 Events and rumours: how the player hears about it
- Each `WorldEvent` produces a rumour. The rumour's **reach** grows by 30 tiles per day from the event site, and it expires
  after 30 days (60 for a fall or a first contact).
- **Who talks:** innkeepers (70 % chance of a rumour line), travellers on roads (50 %), guards (60 %, war news only),
  villagers (25 %) and bards (a sung version).
- **Text** comes from templates in the *speaker's* culture voice: "THEY SAY THE {VERB} OF {SITE} FELL TO {KINGDOM} {N} DAYS
  AGO." A rumour the player has heard becomes *known*: it gets a map marker and a journal line under "News".
- **Notice boards** (a new prop in town squares) list bounties, war proclamations, missing persons, and houses and plots
  for sale. Heralds in capitals announce declarations of war, with a fanfare sound.

### 4.7 Player influence
- **Reputation** runs from −100 to 100 per kingdom: +2 to +10 per quest for its people, +1 per enemy of theirs killed in a
  siege, −20 for attacking its guards. **Fame** is global and opens dialogue.
- **War effort:** the sim takes the player's actions as numbers. Each siege enemy killed is ±0.5 strength. Delivering
  supplies (a quest) is +5 % `def` for 3 days. Assassinating a captain cuts `atk` by 15 %. A diplomacy quest ("carry the
  terms") forces a peace roll.
- **Enlisting** (M12): ranks of Recruit, Sergeant, Captain and Marshal, with a wage. Campaign quests are generated from
  active sieges.
- **Kingdom-making** (late M12, if the owner wants it, §14): own a village (lordship) and pay tribute or declare
  independence.

### 4.8 Performance budget
The daily tick for 48 kingdoms, 300 relations and 2000 settlement states takes under 1 ms on web, so it runs inline at the
day boundary. Resting or fast travel that crosses N days runs N ticks, capped at 60 per call, with weekly ticks beyond that.
Sim memory is under 200 KB. Materialisation happens as part of settlement and chunk generation (≤ 0.2 ms extra) plus actor
spawning near the player.

---

## 5. The culture engine

### 5.1 Two levels: families and dialects
- A **culture family**, one per 2048-tile culture cell, is the big change: architecture grammar, dress cut, the arms
  silhouette family, the music scale and instruments, the phonology, the religion type.
- A **dialect**, one per kingdom, mutates the family's continuous parameters by ±15 % and swaps one or two discrete
  choices. Examples: palette accent, a roof ornament, the town layout variant, the place-name suffix set, heraldry. Every
  kingdom is recognisably its own, and its relatives look related, as they do in real history.
- **Isolated families** for hidden lands come from a reserved pool with exotic archetype mixes.

### 5.2 Data model
```cpp
// rpg/culture/culture.h  (pure data; generated; never stored in saves, only its 32-bit id)
enum class RoofForm : uint8_t { Hip, Gable, Steep, Flat, Dome, Conical, Turf, Pagoda, Mansard, Onion, COUNT };
enum class RoofMat  : uint8_t { Shingle, Thatch, Slate, ClayTile, Turf, Palm, Copper, Bark, COUNT };
enum class WallMat  : uint8_t { TimberFrame, Plaster, Ashlar, Rubble, Adobe, Log, Plank, Brick, Wattle, COUNT };
enum class Layout   : uint8_t { Organic, Grid, Radial, Linear, Compound, Terraced, Stilt, COUNT };
enum class Cut      : uint8_t { Tunic, Robe, Kaftan, Wrap, Coat, Kilt, Poncho, COUNT };
enum class Headwear : uint8_t { None, Hood, Cap, Turban, FurHat, Veil, Circlet, Conical, Headscarf, COUNT };
enum class HelmForm : uint8_t { Nasal, Kettle, GreatHelm, Spangen, Horned, Plumed, ConicalAventail, Masked, Crested, COUNT };
enum class BodyArm  : uint8_t { Padded, Leather, Mail, Scale, Lamellar, Brigandine, Plate, COUNT };
enum class Shield   : uint8_t { Round, Kite, Heater, Tower, Crescent, Oval, Buckler, None, COUNT };
enum class Blade    : uint8_t { Straight, Leaf, Falchion, Scimitar, Khopesh, Wavy, Broad, COUNT };
enum class Scale    : uint8_t { Major, Minor, Dorian, Phrygian, Mixolydian, Lydian, HarmonicMinor, Hijaz, PentaMajor, PentaMinor, Hirajoshi, InSen, COUNT };

struct Phonology {
  std::vector<std::string> onsets, nuclei, codas;    // weighted by order
  uint8_t sylMin, sylMax;                            // syllables per name
  uint8_t pattern;                                   // bits: CV, CVC, V, VC allowed
  std::vector<std::string> placeSuffix, personSuffixF, personSuffixM, epithets;
  uint8_t joiner;                                    // none, apostrophe, hyphen, space ("Qasr Amun")
  std::string forbid;                                // banned bigrams
};
struct ArchStyle {
  RoofForm roof; RoofMat roofMat; WallMat wall; Layout layout;
  uint32_t roofCol[3], wallCol[3], trimCol;          // palette keys; ramps built with art::ramp()
  uint8_t pitch;          // 0..255 -> roof height 0.6..1.6 x today
  uint8_t eave;           // overhang px 0..3
  uint8_t wallH;          // 0..255 -> 0.8..1.3 x today
  uint8_t window, door;   // shape ids (square, arched, slit, round, lattice / plank, arched, curtain, double)
  uint16_t ornament;      // bits: chimney, finials, carved ridge (dragon heads), awnings, crenels, shutters, flower boxes,
                          //       wind-catcher, prayer flags, painted bands, roof stones, porch columns, stilts
  uint8_t foundation;     // none, plinth, stilts, terrace
  uint8_t cityWall;       // palisade, stone curtain, earthen rampart, thorn hedge, none
  uint8_t fence;          // wattle, picket, stone dyke, bamboo, rope
};
struct DressStyle {
  Cut cutM, cutF; Headwear head[3];                  // weighted choices
  uint32_t cloth[6];                                 // palette for clothes
  uint8_t pattern;                                   // plain, stripes, checks, border trim, dots, embroidery
  uint8_t skinLo, skinHi;                            // index range into a 12-step skin ramp (wide variation within any people)
  uint8_t hairStyles;                                // bitset over art::Hair (+ new: Topknot, Shaved, Locs, Bun)
  uint8_t beardP;                                    // beard probability
  uint8_t jewellery, facePaint;                      // probability / style
};
struct ArmsStyle {
  HelmForm helm[2]; BodyArm body[2]; Shield shield; Blade blade;
  uint8_t polearm;        // spear, glaive, halberd, none
  uint8_t bow;            // self, recurve, composite, crossbow
  uint32_t metal, leather, cloth, plume;             // colours
  uint8_t ornament;       // plumes, fur trim, scale gilding, horse-tail, studs
};
struct MusicStyle { Scale scale; uint8_t bpm; uint8_t meter; /* 3,4,5,6,7 */ uint8_t lead, pad, bass, perc; uint8_t swing, ornament, drone; };
struct Religion  { uint8_t kind; /* Pantheon, Monotheist, Ancestors, Animist, Dualist, StarCult */ uint8_t gods; std::vector<std::string> names;
                   uint16_t domains; Glyph symbol; uint8_t shrineForm; uint8_t burial; /* grave, cairn, pyre, sky, sea */ uint16_t taboos; };
struct Customs   { uint8_t staple[3]; uint8_t drink; uint8_t greetingSet; uint8_t festivalMonth; uint8_t lawStrict; uint8_t xenophobia; uint8_t furniture; /* chairs, cushions, benches, hammocks */ };
struct Culture {
  uint32_t id; uint32_t seed; uint8_t archetype; bool isolated;
  std::string name, adjective;                       // "Vethmark", "Vethmarki"
  uint8_t values[8];                                 // martial, mercantile, pious, scholarly, seafaring, expansionist, isolationist, honour
  Phonology phon; ArchStyle arch; DressStyle dress; ArmsStyle arms; MusicStyle music; Religion faith; Customs customs;
  uint32_t magicTradition;                           // 0 = none (section 6)
  uint8_t homeBiome;                                 // the climate it was born in (sampled at the culture cell centre)
};
const Culture& cultureById(uint32_t id);            // memoised; pure function of (seed, cell)
Culture dialectOf(const Culture& family, uint64_t kingdomSeed);
```

### 5.3 Archetype priors (hand-authored; the generator mutates inside them)
Each archetype is a set of *distributions*, not fixed values. Each has a climate affinity, and the generator weights
archetypes by the culture cell's homeland climate.

| Archetype | Climate | Roofs | Walls | Layout | Dress | Arms | Music |
|---|---|---|---|---|---|---|---|
| Fjordfolk | cold coast | Turf, Steep | Log, Plank | Linear, Organic | Tunic, fur hats | Spangen, round shield, axe | Dorian, horn + drone, 3/4 |
| Highland clans | cool hills | Thatch, Gable | Rubble | Compound | Kilt, hoods | Kettle, buckler, broad blade | Mixolydian, pipes, 6/8 |
| Heartland crown | temperate | Hip, Gable, Shingle | TimberFrame | Organic (today's) | Tunic, dress | Nasal, kite, straight | Major, lute + flute |
| Imperial | warm temperate | ClayTile, Hip | Ashlar, Plaster | Grid, Radial | Robe, circlet | Crested, tower, straight | Lydian, brass |
| Dune kingdoms | hot dry | Flat, Dome | Adobe, Plaster | Compound (courtyards) | Kaftan, turban, veil | ConicalAventail, crescent, scimitar | Hijaz, oud-like pluck, 7/8 |
| Steppe horde | dry grass | Conical (yurts) | Wattle, felt | Radial camp | Coat, fur hat | Lamellar, composite bow | PentaMinor, throat drone |
| Marsh folk | wet warm | Thatch, Steep | Plank on stilts | Stilt, Linear | Wrap, headscarf | Padded, oval, spear | PentaMajor, reeds + frame drum |
| Jade terraces | warm hills | Pagoda | Plaster, Brick | Terraced | Robe, Conical hat | Masked, lamellar, curved | InSen, bells |
| River merchants | temperate river | Mansard, ClayTile | Brick | Linear along the river | Coat, cap | Brigandine, heater | Major + swing, fiddle |
| Sun-temple realm | hot wet | Stepped Flat, Dome | Ashlar | Radial around a temple | Wrap, circlet, face paint | Plumed, padded, macuahuitl-like broad blade | Phrygian, marimba-like + drums |

Isolated families draw two archetypes and cross them (adobe walls with pagoda roofs, for instance). They are given the rarer
scales, magic-touched palettes (teal and violet) and their own glyph alphabet.

### 5.4 Distinctness: the four-phase checkerboard maximin (order-independent and guaranteed)
- The culture cell `(i, j)` has phase `φ = (i & 1) + 2·(j & 1)`. In a 2×2 phase tiling, **all 8 neighbours of a cell have a
  different phase** from it, so every adjacent pair is checked by whichever has the later phase.
- **Phase 0** cells pick candidate 0 (their own seed alone). **Phase k** cells generate `K = 12` candidates from their own
  seed and keep the one that maximises
  `score = minDistance(candidate, every lower-phase neighbour) + 0.15·climateFit − 0.5·(any headline component equal to a neighbour's)`.
- The dependency depth is at most 3 (phase 3 depends on phase 0 to 2, which depend on lower phases) and is memoised, so a
  culture costs at most about 30 candidate evaluations. The result is identical whatever the evaluation order. The same
  scheme runs on the **kingdom lattice for dialects and heraldry**.

**The distance metric** `d(A, B)` lies in [0, 1] and is a weighted sum:

| Component | Measure | Weight |
|---|---|---|
| Palette | mean OKLab ΔE of the 5 key colours after optimal matching (5! = 120 permutations), ÷ 50, capped at 1 | 0.20 |
| Roof form | equal = 0, else 1 | 0.15 |
| Names | Jensen–Shannon divergence of character-bigram distributions over 200 sample names | 0.10 |
| Wall material | 0/1 | 0.08 |
| Layout | 0/1 | 0.06 |
| Roof material | 0/1 | 0.05 |
| Clothing cut | 0/1 | 0.05 |
| Helm form | 0/1 | 0.05 |
| Headwear | 0/1 | 0.04 |
| Blade | 0/1 | 0.04 |
| Music scale | 0/1 | 0.04 |
| Ornament | Jaccard distance | 0.04 |
| Shield | 0/1 | 0.03 |
| Lead instrument | 0/1 | 0.03 |
| Meter | 0/1 | 0.02 |
| Religion kind | 0/1 | 0.02 |

**Thresholds (CI fails below these):**
- Neighbouring families: `d ≥ 0.45`, and at least 3 of the 5 *headline components* (palette, roof form, wall material, names,
  music scale) differ.
- Dialects inside one family: `d ≥ 0.10`, and the heraldry differs in at least 2 of {tincture pair, division, charge}.
- Over 20 seeds × 400 cells, the 5th-percentile neighbour distance is reported, and the target is ≥ 0.5.

### 5.5 How the art becomes culture-parameterised
The public API in `rpg/art.h` grows *style* parameters, and every sprite cache keys on a 64-bit hash of (kind, size, style,
seed).

| Today | Becomes | Notes |
|---|---|---|
| `buildingSprite(Building, w, h, roofColor, seed)` | `buildingSprite(Building, w, h, const ArchStyle&, uint32_t seed, uint8_t damage = 0)` | **The roof redo:** a roof library of hip (today's `hipRoof`), gable, steep, flat with parapet, dome, conical, turf (grass texture plus a soil edge), pagoda (upturned eaves), mansard and onion, each × `RoofMat` texels (today's shingle, thatch and slate, plus clay tile, turf, palm, copper and bark). Walls: one painter per `WallMat`. The building *type* decides only functional decoration (inn sign, smithy chimney, temple steeple and so on). The damage variant blackens and holes the roof |
| `humanSheet(HumanLook)` | `HumanLook` gains `Garment top{cut, col, trim, pattern}`, `Headwear`, `ArmourLook{helm, body, gloves, boots, cloak, shield}`, `WeaponLook{family, blade, guard, length, col}`, `facePaint`, `jewellery` | Painters per garment and armour piece over today's rig and poses. Sheets cache by a look hash (`humans_` already does this) |
| `propSprite(Prop)` | `propSprite(Prop, const PropStyle* = nullptr)` | Style-variant props: fences, wells, market stalls (awning colours), lampposts, statues (the deity's glyph), shrines, banners, beds, chairs, tables, rugs (the culture's pattern) |
| `wallPiece(mask)`, `gatePiece()` | `wallPiece(mask, WallStyle)`, `gatePiece(WallStyle, Heraldry)` | Palisade, stone curtain, earthen rampart, thorn hedge |
| `itemIcon(Icon, tint)` | `itemIcon(const ItemLook&)` | Weapon icons drawn from `WeaponLook` (curved scimitar against a leaf blade), armour icons from `ArmourLook` |
| (new) `bannerSprite(Heraldry)` | | Field division (plain, per pale, per fess, quarterly, chevron, bend, saltire) × 2 to 3 tinctures × a charge (a beast silhouette from the monster painters at 1/4 scale, or a glyph) |
| (new) `glyph(seed, style)` | | Stroke glyphs on a 5×5 grid with symmetry rules: holy symbols, runes, magic sigils, shop signs |
| `Audio::setMusic(Music)` | `setMusic(Music, const MusicStyle*)` | `kScale` grows from 3 to 12 scales. The `Town` and `Wild` pieces take the culture's scale, tempo, meter, lead instrument and percussion. `Combat` and `Boss` keep their structure but take the culture's percussion |

### 5.6 Inheritance: what reads the culture
- **Settlements:** the founding culture, through the dialect of its genesis kingdom, sets `ArchStyle`, the layout style,
  fences, centrepieces (fountain, statue, well, sacred tree, fire bowl), the city wall style, and the names of the
  settlement, its streets and its landmarks.
- **NPCs:** census residents get dress, skin and hair ranges, names, greetings and food. Guards get the *owner* kingdom's
  arms (so occupation shows). Bandits get the local culture's poor gear. Priests get the religion's colours and symbol.
- **Items:** shop stock uses the settlement culture's `WeaponLook` and `ArmourLook`. Loot from a draugr uses the extinct
  culture of its ruin. Names come from culture phonology and material words ("Vethmarki bearded axe", "Qasri sunsteel
  scimitar"). Tier names become **culture material words per band** (§7.2).
- **Interiors:** furniture style (chairs against cushions and low tables against hammocks), rugs and tapestries in the
  culture's patterns, the shrine corner, food on the tables (the culture's staples).
- **Music:** town and wild pieces switch at culture borders with today's 2-second crossfade. That border-crossing moment
  is the cheapest "new land" signal there is.
- **Monsters:** each culture cell has a *beast ecology* tweak. Its war-dogs and hunting birds are tamed versions of local
  wildlife, and undead wear the culture of their dead.

### 5.7 Variety within a culture (so its own towns are not clones)
For each settlement, from its seed:
- wealth (poor: wattle and thatch substitutes; rich: stone, extra ornaments, glazing);
- age (old: weathered colours, more trees, crooked streets; new: straight streets, fewer trees);
- the archetype building mix (§2.5);
- the centrepiece;
- 1 in 5 buildings use the family's *alternate* roof form;
- a palette jitter of ±6 % per building.

The repetition audit counts *(culture, archetype, layout, wealth)* signatures: no more than 2 identical within 1500 tiles.

---

## 6. The magic system: traditions bound to cultures

### 6.1 Model
A **tradition** is a culture's way of doing magic. Its *mechanic* is hand-coded and everything else is generated: names,
spell list, glyphs, colours, sounds and which essences it uses. Today's three spells (Firebolt, Mend, Frost Lance) become
the **Common Arcana** tradition, which every mage in the heartland teaches.
```cpp
enum class Form : uint8_t { Bolt, Lance, Nova, Cone, Wall, Rune, Aura, Summon, Ward, Blink, Beam, Orb, Mark, Mend, Totem, COUNT };
enum class Essence : uint8_t { Fire, Frost, Storm, Stone, Tide, Venom, Shadow, Light, Blood, Bone, Wind, Verdant, Star, Song, Dream, Rust, COUNT };
enum class Mod : uint16_t { Chain = 1, Split = 2, Delay = 4, Echo = 8, Homing = 16, Linger = 32, Drain = 64, Knock = 128, Root = 256, Pull = 512 };
enum class Resource : uint8_t { Magicka, Health, Heat, Charge, Notes, Starlight };
struct FxStyle { uint32_t ramp[5]; uint8_t particle; /* ember, shard, petal, glyph, note, star, droplet, smoke */ uint32_t glyphSeed; };
struct SpellDef {
  uint32_t id; std::string name; Form form; Essence ess; uint16_t mods; uint8_t rank;   // 1..3 (rank 3 = signature)
  float power, cost, cooldown, castTime, range, radius; FxStyle fx; uint8_t sfxMotif;
};
struct Tradition {
  uint32_t id; uint32_t culture; std::string name;          // "The Ninefold Choir", "Ashwake Rites"
  uint8_t signature;                                         // index into the 12 hand-coded mechanics below
  Resource res; Essence ess[3];
  std::vector<SpellDef> spells;                              // 8..12: 3 at rank 1, 3 to 5 at rank 2, 2 to 3 at rank 3 incl. one signature spell
  Glyph sigil; bool hidden; uint8_t minLevel;                // 1 (common), 12 (foreign mainland), 15 (hidden valley), 25 (beyond the Mist)
};
```
**Spell list generation.** The tradition's essences and signature constrain the forms it may use (Song favours Nova, Aura
and Echo; Blood favours Drain and Mark). The spell list draws `form × essence × 0 to 2 mods` against a **power budget**
(§6.3). Names come from the culture's phonology plus an essence word ("Vel'ka's Ember Choir"). The FX ramp comes from the
essence plus the culture palette, and the particle shape from the signature. The sound motif plays the culture's scale
(the audio engine already synthesises notes).

### 6.2 Signature mechanics (code; one per tradition, 12 to start)

| # | Signature | Resource | Rule |
|---|---|---|---|
| 1 | **Common Arcana** | Magicka | the baseline (today's spells) |
| 2 | **Blood rites** | Health | spells cost HP. Power is ×(1 + 0.8·(1 − hp/maxHp)), so it is strongest near death. Healing is halved |
| 3 | **Runecarving** | Charge | cast runes on the ground. Enemies that step on them trigger them. 2 runes within 3 tiles combine (fire + frost = steam burst) |
| 4 | **Songweaving** | Notes | each cast plays a note of the culture's scale. 3 casts that form the tradition's motif (shown as a 3-note glyph) trigger a free *Refrain* at ×2 power. The player *hears* it working |
| 5 | **Starcalling** | Starlight | the pool fills only at night (in proportion to darkness) and on clear nights. ×1.5 power under a full moon, which comes every 8 days |
| 6 | **Tidebinding** | Magicka | ×1.6 power within 4 tiles of water or in rain. Can freeze or part shallow water (walkable for 10 s) |
| 7 | **Ancestor calls** | Charge (from kills) | summon spirit echoes of the dead. Stronger near graves, ruins and burial cairns (the culture's burial props). Echoes take the dead culture's armour |
| 8 | **Emberforge** | Heat | melee hits build Heat, and spells spend it. Spells imbue your weapon (the next 5 hits gain the essence). Overheating at 100 burns you |
| 9 | **Beastbinding** | Magicka | bind a non-boss beast below your level for 60 s as an ally. Rank 3 lets you briefly take a beast's form (wolf: speed; bear: armour) |
| 10 | **Maskwork** | Charge | equip one of 3 spirit masks (a visible headwear overlay) that remaps the attack button to a spell-strike combo |
| 11 | **Verdant shaping** | Magicka | alters terrain: thorn walls (temporary solid props), root snares, a bridge of vines over 3 water tiles. The only magic that edits the map, and it reverts after 30 s |
| 12 | **Dreamweft** | Magicka | illusions (decoys pull aggro) and a time-slow bubble (enemies at 40 % for 3 s; uses the existing `slowMo` machinery locally) |

Mainland cultures get traditions 1 and 5 to 9. Hidden kingdoms get the strange ones (3, 4, 10, 11, 12, 2), and every hidden
kingdom's tradition has a signature no mainland culture has. That is the owner's "special form of magic", and it is
special in *mechanics*, not just numbers.

### 6.3 Balance and the power budget
- Spell power is `basePower(form) × essenceMult × (1 + 0.09·(focusILvl − 1)) × rankMult`, where `rankMult` is 1.00, 1.15 or
  1.30. The **focus** (a staff, wand, totem or instrument) is an item with an item level like any weapon (§7), so magic
  follows the same level-synced curve as steel. Player level adds +1.5 % per level, the same as melee.
- **Mods cost budget:** each mod is ×0.85 power. Signature (rank 3) spells get ×1.1 budget with a 60 to 120 s cooldown.
- **Hidden traditions get no more than +10 % raw budget.** Their power is in mechanics: synergy, utility, map editing,
  summons.
- **Resource pools** grow with level-up choices (today's +12 magicka). The non-magicka resources have fixed caps (Heat 100,
  Charge 5, Notes 3).
- **Attunement:** 1 tradition slot at start, a 2nd at level 15 and a 3rd at level 35. Swapping costs a 1-hour rest at a
  shrine of that tradition.

### 6.4 Learning
- **Common and mainland traditions:** buy rank-1 tomes from mages in that culture's towns (rank 1 for 300, rank 2 for
  1500 with reputation Friendly). Rank 3 needs a trial: a small dungeon template with the tradition's theme.
- **Mastery XP:** each tradition levels by *use*. A spell that hits an enemy gives 1 XP, and a kill under the tradition's
  affinity condition gives 5. Rank 2 is at 300 XP, rank 3 at 1200 XP plus the trial.
- **A hidden kingdom's tradition takes five steps:**
  1. Discover the land (the Mist or a hidden valley, §2.12).
  2. Reach reputation Friendly (25) through 3 to 4 local quests. The people are wary, with `xenophobia` high.
  3. Be at least level 25 for a land beyond the Mist (danger 30+ makes that natural), or level 15 for a hidden valley's minor tradition.
  4. Pass the **Trial of the Threshold**, a unique dungeon whose puzzle *teaches* the signature mechanic. A Songweaving trial
     has doors that open to the 3-note motif.
  5. A teacher NPC grants rank 1. The rank-3 signature spell needs reputation Honoured (60) and the second trial.

  The tradition's focus item (a unique visual: a singing bowl, a rune-knife, a mask) can only be bought there.
- **Companions** from that land can cast its rank-1 spells. That previews the tradition before you can learn it.

---

## 7. Itemisation and progression

### 7.1 Danger by place
```
r = |player global tile| (Euclidean from the origin)
D_base(r) = round(1 + 11.5 · ln(1 + r / 350))   // lookup table of r thresholds (no ln in sim code)
D = clamp(D_base + mod, 1, 60)
mod: kingdom heartland (≤150 tiles from a city of a realm with above-median military) −2; wildlands +3;
     dread zone (≤200 tiles from a world-boss lair or necropolis) +6; hidden lands +4; open sea +3; night (wild spawns) +1
```

| r (tiles) | 0 | 100 | 200 | 350 | 600 | 1000 | 1500 | 2000 | 3000 | 6000 | 10000 | 30000 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Walk time | 0 | 22 s | 43 s | 1.3 min | 2.2 min | 3.6 min | 5.4 min | 7.2 min | 11 min | 22 min | 36 min | 1.8 h |
| D_base | 1 | 4 | 6 | 9 | 12 | 17 | 20 | 23 | 27 | 34 | 40 | 52 |

Today's `zoneLevel = 1 + d/22` (with +2 in snow and mountains) reaches level 11 at 224 tiles. The new curve is gentler near
home and open-ended. **Danger is shown to the player:** crossing into a new region shows a skull icon beside the location
name, coloured by `D − playerLevel` (≤ 0 green, ≤ 4 yellow, ≤ 10 red, otherwise purple). NPCs warn about far roads.

### 7.2 Gear tiers and item level
- **Item level** (`iLvl`) is the `D` of the source. Bosses add 2 and named uniques add 3. Shops stock the settlement's band
  ±2.
- **Band names are culture material words.** A heartland culture calls bands 1 to 7 Iron, Steel, Gilded, Jade, Obsidian,
  Emberforged and Starmetal (today's six names plus one). A Qasri culture might say "copper, bronze, sunsteel…". The band
  of `iLvl` decides the tint ramp, as `tierTint` does today.

| Band | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| iLvl | 1 to 8 | 9 to 16 | 17 to 24 | 25 to 32 | 33 to 42 | 43 to 52 | 53 to 60 |

- **Weapon damage** is `base(type) × (1 + 0.09·(eff − 1)) × rarityMult`. The bases are sword 8, war axe 9, mace 9, dagger 6,
  greatsword 14, spear 10, bow 9 and focus 12 (spell power).
- **Armour** is `slotBase × (1 + 0.09·(eff − 1)) × rarityMult`. The slot bases are body 10, helmet 4, shield 5, gloves 2,
  boots 2 and cloak 1.
- **Mitigation** is `AR / (AR + 10·attackerLevel + 30)`, capped at 70 %. This replaces today's flat `armor`. Full
  matching gear keeps mitigation at about 20 to 35 % at every band, so it never trivialises damage.
- **Level sync (the core anti-creep rule):** `eff = min(iLvl, playerLevel + 4)`. A band-5 sword found at level 8 works like
  an iLvl-12 sword until you grow into it. It is exciting but not breaking, and it still sells for its full value.
- **Rarity:**

| Rarity | Power | Affixes | Drop odds |
|---|---|---|---|
| Common | ×1.00 | 0 | 55 % |
| Uncommon | ×1.05 | 1 | 28 % |
| Rare | ×1.10 | 2 | 12 % |
| Epic | ×1.15 | 3 | 4 % |
| Legendary | ×1.20 | 2 + a unique power | 1 %, and only from champions, named uniques, bosses and world bosses; never below D 10 |

- **Affix pool** (ranges scale with `eff`):
  - elemental damage, 3 to 12 %;
  - life drain, 2 to 4 %;
  - critical chance and critical damage;
  - stamina regeneration;
  - +5 % move speed;
  - roll distance;
  - thorns;
  - fire, frost and shadow resistance, 10 to 30 %;
  - spell power and magicka;
  - potion effect;
  - gold find, at most 15 %.

### 7.3 Generated uniques and legendaries
A legendary is a base item, two rolled affixes and one **unique power** from a hand-coded table of about 30. Examples:
- every third hit chains lightning to 2 foes;
- rolling leaves a fire trail;
- kills raise a spectral wolf for 10 s;
- arrows split in three beyond 6 tiles;
- below 30 % HP, time slows for 2 s (60 s cooldown);
- blocking reflects projectiles;
- spells cost HP at +40 % power;
- standing still for 1 s makes the next hit crit.

Each legendary gets a culture-phonology name ("Hrothgar's Oath", "The Salt Widow"), a lore line tying it to a place or
event, a unique tint and glow on the paper doll, and an icon. **Attunement** limits legendaries equipped at once: 1 at
level 10, 2 at level 25, 3 at level 40.

### 7.4 Player power and the XP curve
- **Per level:** +1.5 % damage (today 4 %, which compounds with gear into about 16× at level 50) and +4 HP. The level-up
  choice of +12 health, magicka or stamina stays.
- **XP for a kill** is `base × (1 + 0.12·(Le − 1)) × clamp(1 − 0.12·max(0, Lp − Le − 2), 0.05, 1)`. Farming the start zone
  at level 30 earns 5 %.
- **Level-gap penalty:** against an enemy with `Le > Lp + 4`, the player's damage is ×max(0.4, 1 − 0.04·(Le − Lp − 4)), and
  the enemy's is ×(1 + 0.03·(Le − Lp − 4)). With the HP ratio, this makes a zone 15 levels up about 4× harder: a wall that
  readable danger tells you about.
- **The XP curve** keeps today's shape, `xpForNext = a + b·(L − 1) + c·(L − 1)²`. Its constants are tuned by
  `rpg_test --power-curve` to hit the hours table in §1.4. Level cap 50.
- **Enemies at danger D:** HP ×(1 + 0.10·(D − 1)) (as today), damage ×(1 + 0.09·(D − 1)) (today 0.13, which made sense
  without mitigation), armour value 1.5·D, XP ×(1 + 0.12·(D − 1)).

**Expected time to kill** (a same-band player against same-band trash) stays at 3 to 4 hits, and a same-band brute at 8
to 12, at every band. `--power-curve` prints a table of levels {1, 5, 10, 20, 30, 40, 50} × D {L−10, L, L+5, L+10, L+20} and
fails if same-band trash falls outside 3 to 5 hits or same-band death time falls outside 6 to 20 s.

### 7.5 Anti-power-creep rules (each is a test or a code rule)
1. The world never scales to the player (spawns use `D` only).
2. Level sync on every equipped item (`eff`).
3. The level-gap damage penalty and XP decay (§7.4).
4. **Shops stock by local band, never by player level.** This fixes today's leak in `shopStock`
   (`lvl = max(plLevel, site level)`). Merchants top out at Rare, and capitals offer Epic at reputation Honoured.
   Legendaries are never sold, except by the hidden-kingdom focus vendor.
5. **Quest rewards scale with the quest's danger, not the player.** This fixes `makeOffer`'s `max(home.level, plLevel)` and
   `completeQuest`'s `randomLoot(plLevel + 1)`.
6. Legendary attunement (§7.3) and tradition slots (§6.3).
7. A **potion cooldown** of 8 s shared, and at most 10 potions carried.
8. **Merchant gold** is 300 (village) to 3000 (capital) per restock, so selling a haul of loot can't fund everything at
   once.
9. **Gold sinks** (§8 and §2.12): houses, plots, building, horses, ships, ferries, carriages, training, property tax.
10. **The death penalty:** lose 10 % of carried gold and wake at the last town (today's respawn).

### 7.6 Enemy generation
- **Roster by biome × band × culture ecology:** today's 15 monster types, plus about 2 new families per content milestone:
  - M6: harpy and golem;
  - M10: sea serpent, kraken and merrow;
  - the hidden lands: one signature monster per isolated culture.
- **Variant looks:** `MonsterLook { Monster base; Ramp body; uint8 scale; uint16 overlays }`. The overlays are horns,
  spikes, crystal growths, moss, rime, ember cracks, armour plates, glowing eyes and a bone mask, drawn on top of the
  existing painters, so a Crystal Troll or an Ashen Wolf costs no new rig.
- **Elite affixes** (12, each with a tint or aura):

| Affix | Effect |
|---|---|
| Swift | +30 % speed |
| Armoured | +60 % armour, −20 % speed |
| Vampiric | heals 30 % of the damage it deals |
| Frenzied | +40 % attack speed below half HP |
| Splitting | becomes 2 small ones on death |
| Burning | leaves a fire trail |
| Frostbound | slows on hit |
| Warded | shields allies within 5 tiles |
| Summoner | 2 minions every 12 s |
| Blinking | teleports behind you every 6 s |
| Regenerating | 2 % HP/s unless burning |
| Volatile | explodes 1 s after death, telegraphed |

- **Frequency:**

| Kind | From | Rate | Affixes | HP | Damage | Loot |
|---|---|---|---|---|---|---|
| Elite | D ≥ 5 | 6 % of pack members | 1 to 2 | ×2.2 | ×1.3 | +1 rarity roll |
| Champion pack | D ≥ 12 | 1 % (packs of 3 sharing affixes) | 3 | ×3.5 | | |
| Named unique | | 1 to 2 per region | 2 to 3 + a title | ×6 | ×1.5 | Rare+ guaranteed, 15 % legendary |

  A named unique also has a culture name ("Old Ninefangs", "Sathra the Unbowed"), a lair vignette and a rumour that names it.
- **Dungeon bosses get phases:** at 66 % and 33 % HP they add a behaviour from {summon adds, enrage +20 % speed, arena
  hazard, shield phase}.
- **World bosses:** one per kingdom cell (a dragon, as Ashfang is today; a frost giant; a lich; a wyrm; a behemoth; a kraken
  at sea). Each is at D + 5 with ×25 HP, roams a territory, has a lair, and can *raid settlements* as a sim event ("Ashfang
  burned Kettlebrook"). Killing one is news across the horizon (+fame, and a legendary).
- **Soldiers and bandits** wear their culture's arms grammar, and bandits get the poorer bodies. Every new land therefore
  brings new-looking enemies at zero extra art cost.

---

## 8. Housing, land, building, farming and animals

### 8.1 Buying homes

| Home | Footprint | Where | Price (gold) | Tax per week |
|---|---|---|---|---|
| Rented room | an inn bed | any inn | 20 per night | |
| Hut | 3×2 | villages | 900 | 5 |
| Cottage | 4×3 | villages, towns | 2,500 | 15 |
| Townhouse | 5×3 | towns, cities | 6,000 | 30 |
| Manor | 7×4 + yard | cities | 18,000 | 80 |
| Estate | 9×4 + walls | capitals, reputation Honoured | 50,000 | 200 |

- **Prices** are × settlement wealth (0.7 to 1.4) × kingdom tax policy.
- **For sale:** the census marks 0 or 1 vacant house per village (40 %), 1 to 2 per town and 2 to 3 per city,
  deterministically. A "FOR SALE" sign prop sits by the door, and the notice board lists them. The steward (keep), the
  innkeeper (villages) or a new **Reeve** role sells the deed.
- **Owning one** gives a persistent storage chest, a bed (rest, spawn point), decorating (§8.3), and a place for companions
  to wait. A kingdom that takes the town honours your deed if your reputation with it is ≥ 0; otherwise you can pay a 25 %
  "re-registration" fee. That gives the sim personal stakes.

### 8.2 Land and building on it
- **Plots.** From the v-next settlement generator, villages and towns have 1 to 3 fenced empty lots on the outskirts beside
  a street, with a "FOR SALE" sign. The sizes are small 10×8 (1,500), medium 13×10 (3,500) and large 16×12 (6,000), × the
  settlement's wealth. The plot is a `PLOT` record in its region's delta (§3.2).
- **Build mode** (touch-first) works in four steps:
  1. **Choose a blueprint.** Shells sized to the plot: hut, cottage, longhouse, townhouse, hall. Any **culture style you have
     discovered** can be used (`ArchStyle` from the culture table). Building a domed Qasri house in a Nordic village is an
     exploration reward that shows.
  2. **Place the shell** as a ghost footprint (green or red). It needs a free door tile and a BFS path from the plot gate to
     the door.
  3. **Pay materials** (wood, stone, thatch or tiles, nails: bought, or gathered from felled trees and quarried boulders as
     a gold-saving option) plus builder wages.
  4. **Construction** takes 2 to 5 in-game days. A scaffolding sprite stands in for the house, and builder NPCs work it on
     their schedule.
- **Placing outside objects:** a catalogue with footprints:
  - fences and gates;
  - path tiles (ground edit);
  - farmland;
  - well, woodpile, beehive, scarecrow, chicken coop, pen, stable, workbench, forge, flower beds;
  - saplings (grow into trees in 10 days);
  - benches, lanterns, statues, and a banner with **your own heraldry**, designed in the character creator from M3.

  Rules: inside the plot; no overlap with anything solid; the gate-to-door path, and a path to every usable object, must
  survive (the same BFS). Selling an object back refunds 50 %. The touch UI drags a ghost, with buttons for Rotate (two-way
  objects), Place, Remove and Done.

```cpp
struct PlacedObj { uint16_t kind; uint8_t x, y, flags; uint32_t data; };        // data: storage id, crop/animal index...
struct Crop   { uint8_t x, y, kind, stage, quality; uint16_t plantedDay, lastWaterDay; };
struct Animal { uint8_t kind; std::string name; uint16_t boughtDay, lastFedDay; uint8_t happiness, produce; };
struct Plot {
  Gid id, settlement; int32_t gx, gy; uint8_t w, h, state;   // ForSale, Owned, Building, Built
  uint8_t blueprint; uint32_t styleCulture; uint32_t houseSeed; uint16_t buildDoneDay;
  std::vector<PlacedObj> outside, inside; std::vector<uint8_t> groundEdits;   // RLE per tile
  std::vector<Crop> crops; std::vector<Animal> animals; uint32_t storage;
};
```

### 8.3 Interior decorating
- The same catalogue and UI work on interior tiles, with the furniture from §10.5: beds, tables, chairs, shelves, chests
  (storage), rugs, hearth, cooking pot, an armour mannequin (shows a stored gear set) and a weapon rack.
- **Trophies:** a named unique or world boss drops a trophy item (a head or a horn) for the wall.
- **Paintings of places you have visited:** a 32×24 landscape rendered from the world map data of a wonder or city you
  discovered. That is a generated souvenir, and it is pure code.

### 8.4 Farming loop
- **Tools** are items that change the interact action on soil: a hoe (till grass or dirt to `Farmland`), a watering can, a
  sickle.
- **Crops** are grown from seeds:
  - Temperate: wheat, barley, oats, rye, potatoes, turnips, cabbage, carrots, onions, flax and beans.
  - Warmer: grapes, dates (a tree), maize, tea and rice (a paddy that needs water tiles).
  - Herbs (for alchemy).

  **Seeds are sold by cultures whose staples they are,** so exploring unlocks crops.
- **Growth:** 4 stages, advancing once per day while watered within the last day (rain counts). A crop takes 3 to 8 days
  to harvest and yields 1 to 3, with quality from watering streaks.
- **Seasons:** `day % 112` gives 4 seasons of 28 days. Winter allows only cold crops. A crop left unwatered for 2 days wilts
  and recovers when watered; nothing dies.
- **Away from home:** growth catches up from `today − lastSeenDay` when the plot loads.
- **A farmhand** (hired from the census, 10 gold a day) waters and harvests into the storage chest while you travel.
- **Cooking** at a hearth or cooking pot combines 2 to 3 ingredients into meals: 10-minute buffs (+HP regen, +stamina,
  +resistance) that don't stack with potions. Each culture has 3 signature recipes, taught by innkeepers.
- **Selling:** merchants pay more for crops foreign to their culture, which makes a small trading game.

### 8.5 Livestock and mounts

| Animal | Needs | Produces |
|---|---|---|
| Chicken | coop | eggs daily |
| Goat | pen | milk |
| Sheep | pen | wool weekly |
| Cow | pen | milk |
| Pig | pen | sells well |
| Horse | stable | mount |
| Dog | | guards the farm, can follow you as the animal companion (§9) |

- **Feed:** 1 hay per animal per day. A trough holds 7 days, and hay comes from the harvest or from markets. Water comes from
  a well on the plot. Happiness (fed, room, a groomed horse) sets produce quality.
- **AI:** animals wander inside their pen's bounds, and chickens peck around the yard.
- **Sprites:** reuse the quadruped painter (wolf, boar, bear) for the cow, sheep, goat, pig and horse, plus a small bird
  painter for chickens.
- **Predators:** a fenceless plot within 60 tiles of a den or wildlands has a 5 % chance per night of a raid. Wolves try for
  the pen, and you can defend it. A loss costs one animal at most. A dog halves the chance.
- **Horses** are the endless world's legs: 8 tiles/s, +15 % on roads. You dismount automatically to fight or to enter a
  building or dungeon. Breeds are culture-bound (steppe horses are fastest, highland ponies are sure-footed on slopes) and
  cost 600 to 3,000 at town stables.

---

## 9. Companions

- **Recruitment:**

| Source | How often |
|---|---|
| Tavern regulars | 1 candidate per town (40 %) and per city (2), from the census, with a flag and a backstory |
| Quest-chain companions | the PLAN.md inn chains' payoff |
| Hirelings | mercenaries for 30 to 120 gold a day, no personality arc |
| Animal companion | a dog bought at farms, or a wolf pup tamed at a cleared den |
| Hidden-land companion | a practitioner of that land's tradition, once you are Friendly |

- **Party size:** 1 companion + 1 animal, then 2 + 1 from level 20 (the "Leadership" perk).

```cpp
struct Personality { int8_t brave, kind, pious, lawful; uint32_t likes, dislikes; };   // axes -2..2; tag bitsets
struct Companion {
  NpcId id; std::string name; uint32_t culture; art::HumanLook look; uint8_t cls;   // Warrior, Archer, Mage, Healer, Rogue
  int8_t approval;            // -100..100
  uint8_t bond;               // 0..100, unlocks skills
  Personality p; uint16_t homeSite; uint8_t hook;   // backstory hook: revenge, lost kin, heirloom, burned home
  int eq[6];                  // weapon, armour, helmet, shield, ring, amulet (indices into its own small inventory)
  std::vector<Item> inv;
  uint8_t state;              // Following, Waiting(at Gid), Downed, Recovering(until day), Left
  uint16_t questStage;
};
```
- **AI:**
  - follow at a formation offset of 1.5 tiles behind the player;
  - engage what the player attacks, or what attacks the player;
  - role behaviour reusing existing code: the *Warrior* taunts (pulls aggro within 5 tiles); the *Archer* keeps its distance
    (the bandit-archer kiting); the *Mage* casts its culture tradition's rank-1 spells; the *Healer* heals below 50 % HP; the
    *Rogue* flanks (the wolf circling logic);
  - avoid friendly fire, because player projectiles ignore companions;
  - drink potions you have given it below 25 % HP.
- **Commands:** a touch radial (hold on the companion portrait) with Follow, Wait here, Attack my target, Stay passive and
  Go home.
- **Progression:**
  - Level tracks player level − 1, so there is no grind.
  - Power comes from gear (the same paper doll as yours) and from **skills unlocked by bond, not XP**. For the warrior: Taunt
    at bond 20, Shield Wall at 50, Last Stand at 80.
  - Bond grows with time together (+1 per day) and with approval.
- **Personality and approval:**
  - Player deeds carry tags, from a deed event bus on top of `Ev`: helped refugees, spared, killed civilians, stole, joined
    the war against my homeland, cleared bandits, explored a ruin, drank in a tavern, gave gold.
  - The companion's likes and dislikes turn deeds into approval of ±1 to ±15.
  - Below −40 it complains. Below −70 it leaves (goes home, recruitable again after an apology quest). At 50 or more its
    personal quest unlocks.
- **Banter:**
  - Template lines triggered by context, at most 1 line every 90 s.
  - The best trigger is *crossing a culture border*: the companion compares it to home ("These Qasri build houses like
    bread ovens. Cool inside, though.").
  - Other triggers: weather, night, after a hard fight, at a wonder, low HP, approval thresholds, news of a war involving
    its homeland.
- **Personal quests come from the hook:** avenge my village against a named unique; find my sibling in the refugee camps (it
  uses the sim's refugee flags); reclaim the family heirloom from a ruin; rebuild my burned home village (a housing plot,
  given to you).
- **Death: "downed, not dead" by default.** At 0 HP a companion kneels. Hold Interact for 2 s within 30 s to revive it at
  30 % HP. Otherwise it limps home to recover for a day, at −5 approval. An optional **Hardcore** setting makes death
  permanent (owner question, §14).

---

## 10. NPC life

### 10.1 The census: who lives here
Each settlement buffer generates a census when it is built (deterministic):
```cpp
struct Resident { NpcId id; uint16_t home, work; Role job; uint8_t age, traits; bool female; uint32_t lookSeed; uint16_t household; };
```
- **Households per building:** a hut holds 1 to 2, a house 2 to 4, a townhouse 3 to 5, a farmhouse 3 to 6. The inn holds the
  innkeeper and 1 to 2 staff, the smithy the smith and an apprentice, the temple a priest and an acolyte, the keep the Jarl,
  a steward, 2 servants and the guards.
- **Jobs** are assigned by archetype: Farmer, Fisher (docks), Miner (the mine mouth), Woodcutter, Hunter, Smith, Merchant,
  Baker, Tailor, Stablehand, Bard, Scholar, Priest, Guard, Child, Elder, Beggar, Noble.
- **The sim's `popPct`** decides who is present: the first N residents in hash order. Burned villages are emptier, and
  their residents appear as refugees elsewhere.
- **Today's random folk spawns** (`folk = 16/10/5`) become residents who are outdoors by schedule.

### 10.2 Schedules
```cpp
enum class Act : uint8_t { Sleep, Eat, Work, Wander, Socialise, Tavern, Pray, Patrol, Play, Perform, Shop, Home };
struct Block { uint8_t from, to; Act act; uint8_t place; /* Home, Work, Tavern, Plaza, Temple, Field, Gate, Market */ uint8_t prob; };
```
Templates per job, in in-game hours:

| Job | Morning | Midday | Afternoon | Evening | Night |
|---|---|---|---|---|---|
| Farmer | 5–6 home, eat; 6–12 field | 12–13 home, eat | 13–18 field | 18–21 tavern (40 %) or home | 21–5 sleep |
| Smith | 7–12 anvil | 12–13 tavern, eat | 13–19 anvil | 19–22 tavern (50 %) | 22–7 sleep |
| Merchant | 8–12 stall or shop | 12–13 eat | 13–18 stall | 18–21 tavern or plaza | sleep |
| Guard (day shift) | 6–18 patrol or gate | | | 18–22 tavern or barracks | sleep |
| Guard (night shift) | sleep | sleep | 18–6 patrol, lighting lamps at 19:00 | | |
| Child | 7–12 plaza (play) | 12–13 home | 13–18 plaza or fields | 18–20 home | sleep |
| Priest | 6–20 temple; 10:00 sermon in the plaza on festival days | | | | sleep |
| Bard | sleep until 11 | wander | | 18–24 tavern, performing | sleep |

Each resident gets ±30 minutes of jitter and trait modifiers (sociable residents go to the tavern more). **Taverns fill at
night.**

- **Level of detail:** residents are *not* actors until the player is within 46 tiles (today's activation radius). They
  then spawn **where their schedule puts them at this hour**, part-way along their route, so a town you walk into is already
  mid-day. An interior's population is the residents whose current block points at that building, which replaces fixed
  interior spawns.
- **Paths:** door to door over the town's street mask (from `stampSettlement`), A* on the window map with routes cached per
  building pair. At most 2 path requests per frame and 40 moving residents updated per frame (time-sliced).
- **New human frames** (art): sit, eat or drink (mug in hand), work (hammer, hoe, sweep: today's attack frames with a tool
  in the weapon slot), sleep (drawn over the bed), wave and play.

### 10.3 Taverns and social life
- **The tavern:** chairs are assigned to seats, and residents sit, order (the innkeeper walks to the table), eat and drink.
  Pairs chat (facing, with speech-bubble icons: "…", "!", "♪", a mug).
- **The bard** plays a *culture-style tavern piece*: a new `Music::Tavern` that takes the culture's `MusicStyle`, with volume
  by distance. Festivals bring dancing.
- **Closing time** is 24:00, and stragglers walk home.
- **Gossip:** residents share rumours (§4.6). Close to the player, two NPCs talking may surface a rumour line as overheard
  text.
- **Households and friends:** spouses walk home together; children follow a parent at times. NPCs greet friends and
  relatives by name ("EVENING, HALLA").
- **Small life:** lamplighters, market stalls opening and closing (prop states), dogs, cats and chickens in villages,
  children playing tag in the plaza, guards changing shift at the gate.

### 10.4 Defence when monsters or raiders come
This is the queued request, built in two steps.
- **M0, on today's world:**
  - Add `Actor::faction` (Player, Folk, Guard or Kingdom, Wild, Bandit, plus kingdom IDs later) and a hostility matrix.
    Wild and bandit actors are hostile to the player, folk and guards. Guards are hostile to wild and bandit actors, and to
    the player only for crime.
  - Monsters pick the nearest hostile, weighted 1.5× toward the player so the player stays the main target. **Villagers do
    get attacked.**
  - Villagers flee to their home door (entering it, and coming out 30 s after the threat ends). Brave adults with a tool
    (smith, hunter, farmer) become **militia**: weak fighters with a pitchfork or hammer.
  - Guards converge from across town when **the town bell** rings: 3 or more hostiles inside the footprint ring a bell
    sound, show "THE TOWN IS UNDER ATTACK!" and switch to combat music.
  - Monsters chasing the player keep their leash only outside settlements.
- **M5, raids by pressure:**
  - Each settlement's **monster pressure** is `uncleared dens and lairs within 60 tiles × (1 − kingdom military factor)
    × season`. It gives 2 to 8 % per night of a raid.
  - If the player is within 120 tiles at night, a raid party arrives along a road: a den pack plus an elite.
  - If the defence fails (no defenders standing, or 3 or more raiders inside for 30 s), prosperity drops by 10, damage rises
    by 5, and maybe a villager is taken, which starts a rescue quest.
  - When the player is far away, the outcome is decided in the abstract (pressure against garrison).
- **M4, kingdom guard protection:** member settlements get guards scaled by the kingdom's military. Villages go from 0 to
  1–2, towns have 3–5, cities 7–12. Patrols of 2 to 3 soldiers walk the roads between member settlements. Independent
  settlements have militia only, which makes them visibly more vulnerable. That makes kingdom membership mean something.

### 10.5 Cluttered, lived-in interiors (M0)
The generator runs in passes over `genInterior`'s room (versioned through `Bldg::genVer`):
1. **Anchors** placed by template against the walls: bed, hearth, counter, altar, anvil.
2. **Function groups:** a table with 2 to 4 chairs; a shelf with crates; a desk with a bookshelf; a loom with a basket; a
   cradle beside a bed. Each is placed with a 1-tile gap rule.
3. **Wall decor** on the top wall row (the only wall visible in 3/4): tapestries, wall shelves with jars, hanging herbs,
   antlers, paintings, candle sconces, a window.
4. **Small clutter** on a new non-solid `deco` layer (a byte per tile, drawn on top of furniture): mugs, plates, bottles,
   candles, books, baskets, sacks, pots and pans, laundry, tools, bread, cheese, a fruit bowl, scrolls, an inkwell. It goes
   on tables and counters and in floor corners.
5. **Validation:** a BFS from the door to every usable object, and at least 60 % of the floor walkable. Otherwise the last
   placements are removed.

Density follows household wealth and job: a smith's home has tools, a scholar's has books, a farmer's has sacks and
baskets. There are about 34 new props: 12 wall, 16 clutter and 6 furniture (cupboard, wardrobe, stool, bench, cradle,
spinning wheel). From M3 the furniture style comes from the culture (`Customs::furniture`: chairs, cushions with low tables,
benches, hammocks). Bigger buildings (inn, keep, manor) get **multi-room** layouts from partition walls, and an inn has a
stairs prop up to a rented-rooms floor (a second interior sub-map).

---

## 11. 2D terrain relief (instead of brown mountain blobs)

**Principle:** don't fake a 3D mountain in 3/4 view. Use what pixel-art RPGs do well: **discrete height levels with cliff
faces, ramps and stairs, aerial colour grading, cast shadows, waterfalls, and big y-sorted peak sprites**. Show the large
scale where it reads, which is on the map (hillshade and mountain glyphs). Today's `rockLevel` terraces with cliff faces are
the seed of this, applied only to rock.

### 11.1 Height levels
Every land tile gets `H = clamp((E_lowpass − 0.33) / 0.065, 0, 7)`. Here `E_lowpass` is `E` filtered at an 8-tile scale,
so no plateau is smaller than about 8×8 tiles. `H` is stored in `ChunkData` from M1, even before it is rendered.

| H | Meaning |
|---|---|
| 0 to 1 | lowland |
| 2 to 3 | hills |
| 4 to 5 | highland plateaus |
| 6 | mountain shoulders (snow in cool climates) |
| 7 | peaks; rock only where also steep |

### 11.2 Edges between levels
- **A cliff** is where `H` drops toward the south and the 4-tile slope is above the threshold. The face is drawn in the
  upper part of the lower tile, 12 px per level and up to 2 levels per tile row, with today's face shading (lit lip,
  vertical fissures, a contact shadow at the base). The face material follows the biome: earth bank (grass), sandstone
  (desert), granite (temperate), basalt (volcanic), ice (snow).
- **East and west drops** get a 3-px side lip. **North drops** get a 2-px rim highlight with a thin shadow.
- **A slope** is where `H` changes by 1 and the 4-tile slope is below the threshold. It is drawn as a darker hatch band
  (diagonal shading), is walkable at 0.8× speed, and reads as a hillside.
- **Rock** (impassable) is only the steep cores at `H ≥ 6`, drawn as *stacked cliffs*, never as a flat grey area.

### 11.3 Readability cues
- **Aerial colour grade** per level: +4 % lightness and −6 % saturation, with a slight blue shift from `H ≥ 5`. Higher
  ground looks higher at a glance.
- **Cast shadows:** below south faces, darken a 4-px band, and for 2 or more stacked levels cast a soft shadow 6 px to the
  south-east (the light is top-left, as in art.cpp).
- **Snowline:** `H ≥ 6` with `T < 0.45` turns the ground to snow, and snow overhangs on faces (today's code).
- **Peaks:** a local maximum with `H ≥ 6` gets a `Peak` prop (a 32×48 rock spire with a snow cap and ridgelines, y-sorted
  like trees, with a shadow), at most 1 per 6×6 tiles. Ridge crest tiles get a light crest line. *This is what makes a
  range read as a range.*
- **Waterfalls:** a river crossing a cliff draws an animated waterfall over the face (3 frames), mist particles and a
  looping sound. Lakes on plateaus spill over in falls.
- **Stairs and switchbacks:** a road crossing a cliff stamps stone stairs or a 2-tile zig-zag ramp.

### 11.4 Walkability guarantee
Ramps sit on a **global lattice**. Along every south-facing cliff run, the lattice points are `x ≡ hash(row) mod 16`.
Where a face exists at a lattice point, a 3-tile ramp is carved. East and west faces use the same rule on y. This is
deterministic, independent of chunk order, and leaves at most 16 to 24 tiles between ways up. A plateau component smaller
than 24 tiles is flattened to its surroundings (the low-pass filter makes this rare). `rpg_test --endless` floods each
sampled window and checks every plateau of 24 tiles or more is reachable.

### 11.5 On the map
- **Hillshade:** `shade = clamp(0.5 + k·(−∂E/∂x − ∂E/∂y))`, a north-west light, multiplied into the map colour.
- **Faint contour lines** every 2 levels.
- **Mountain glyphs** (little triangle peaks with snow tips) on ridge crests at zooms Z2 and Z3, the classic fantasy-map
  look.
- **Named ranges and passes** ("The Grey Teeth", "Wolfgate Pass") from the landmark names.

Scale and height become legible exactly where the player plans routes.

### 11.6 Smoother biome transitions (queued request)
- **Macro level:** the climate fields are smooth, so biomes run in natural sequences (forest, autumn wood, taiga, snow) with
  wide **ecotone bands** instead of hard jumps.
- **Pixel level:** inside a band 3 to 8 tiles wide (by biome pair, from the blend byte), `groundPixel` mixes the two
  biomes' ground functions with a 4×4 Bayer dither driven by a smooth weight. Today a hard switch is softened only by a
  domain warp of 5 to 7 px.
- **Transition flora:** vegetation density and species ramp across the band: lone trees and bushes from forest to plains,
  dry grass and scrub from plains to desert, snow patches and dwarf pines from taiga to snow.

---

## 12. Tech

### 12.1 Engine and code changes (by area)
- **New SDL-free libraries, linked into `rpg_sim` so `rpg_test` runs everything headless:**
  - `rpg/world/` holds `coords.h`, `ids.h`, `dmath.h`, `noise.h`, `jobs.h`, `source.h`, `window.*`, `macro.cpp`, `hydro.cpp`,
    `region.cpp`, `roads.cpp`, `settlement.cpp` (a *copy* of `stampSettlement`; `world.cpp` stays frozen for
    `LegacySource`), `sites.cpp` (cave, ruin, camp, shrine, lair, den, vignette and wonder stamps), `chunkgen.cpp`,
    `relief.cpp`, `cache.cpp` and `legacy.cpp`.
  - `rpg/culture/` holds `style.h` (`ArchStyle` and friends, included by `art.h` as well), `culture.*`, `archetypes.cpp`,
    `names.cpp`, `heraldry.cpp` and `distance.cpp`.
- **The sim gains** `realm.cpp`, `events.cpp`, `census.cpp`, `schedule.cpp`, `housing.cpp`, `farming.cpp`,
  `companions.cpp`, `magic.cpp` and `factions.h`. `game.cpp` (1465 lines) has its AI moved out to `ai.cpp` in M0, so the
  lanes stop colliding.
- **Art:** `art.cpp` (5225 lines) is split in M0 into `rpg/art/art_core.cpp`, `art_internal.h`, `art_human.cpp`,
  `art_monster.cpp`, `art_nature.cpp`, `art_props.cpp`, `art_building.cpp`, `art_items.cpp`, `art_fx.cpp` and later
  `art_heraldry.cpp`. The public `rpg/art.h` gains style parameters (§5.5). The split must be **pixel-identical**:
  `art_preview --hash` dumps a hash of every canvas before and after.
- **View:** `worldmap.cpp` (zoom, fog, quadtree tiles), `paperdoll.cpp`, `creator.cpp`, `buildmode.cpp`. Terrain chunk
  textures are keyed by global chunk coordinates, the bake snapshot is per chunk, and the view handles `Ev::WindowShift`.
- **Audio:** `MusicStyle` parameters and 12 scales in `kScale`, plus `Music::Tavern` and `Music::Siege`, and
  `setMusic(Music, const MusicStyle*)`. The audio thread copies the style by value when a piece starts.
- **CMake:**
  - Glob `rpg/world/*.cpp`, `rpg/culture/*.cpp` and `rpg/art/*.cpp`.
  - Change the `if(EXISTS rpg/art.cpp)` guards to `rpg/art.h`.
  - Set `/fp:precise` on MSVC and `-ffp-contract=off` on clang and Emscripten for `rpg/world/*` and `rpg/culture/*`.
  - The web build keeps `-sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728`.

### 12.2 Threading: desktop and iOS against the web
| | Desktop and iOS native | Web (Emscripten on GitHub Pages) |
|---|---|---|
| Sim | main thread, fixed step (as now) | same |
| World generation | 1 worker thread owns the caches; requests and results go through queues | `jobs.pump(3 ms)` per frame, sliced jobs |
| Terrain bake | 1 worker (today's `workerLoop`) | `pumpBake(3 ms)` per frame (today's incremental 16-row path) |
| Teleport or load | synchronous 3×3 chunks behind the fade | same, ≤ 300 ms |

There are no pthreads on the web, because Pages can't send COOP/COEP headers. The `coi-serviceworker` trick is an option
later and shouldn't be relied on. All generation code is reentrant over immutable inputs, so the same jobs run either way.

### 12.3 Memory and performance targets
| Device | FPS | Sim | Gen | Bake | Render | Worst hitch | Memory |
|---|---|---|---|---|---|---|---|
| Windows dev PC | 144 (vsync) | ≤ 1 ms | worker | worker | ≤ 2 ms | 16 ms | – |
| iPhone 12 to 16, Safari (web) | 60 | ≤ 3 ms | ≤ 3 ms/frame | ≤ 3 ms/frame | ≤ 6 ms | 50 ms on a window shift, 300 ms on a teleport behind the fade | heap ≤ 256 MB, textures ≤ 64 MB |
| iPhone (TestFlight, native) | 60 (120 optional) | ≤ 2 ms | worker | worker | ≤ 5 ms | 33 ms | ≤ 300 MB resident |
| Older phone (iPhone XR or 11) on web | 60 target, 30 floor | | 2 ms | 2 ms | | | |

New-game startup should take ≤ 1.5 s on web: macro samples, the start plan, the 3×3 chunks and the settlement buffer.
Measure everything with `EMB_TIMING=1` and the `--perf` overlay, which gains `gen` and `bake` ms per frame and chunks per
second.

### 12.4 Testing strategy
Every new system ships with a headless check in `rpg_test`. `tools/rpg_sim.cpp` is split into `tools/tests/test_*.cpp`
(world, culture, realm, power, life, home, save) so that lanes don't collide.

| Command | Asserts |
|---|---|
| `rpg_test --seeds 1..20` | today's suite on classic (must stay green forever) |
| `rpg_test --endless --seeds 1..10 --walk 6000` | chunk determinism (3 random orders with cache eviction give byte-identical chunk hashes); seam continuity (ground, rivers and roads at chunk and region borders); settlement density per region in [3, 9]; the start guarantee; reachability in 30 sampled 1024² windows per seed; no NaN or infinity in any field |
| `rpg_test --golden tests/fixtures/chunks_mv1.txt` | chunk hashes at fixed coordinates match on Windows *and* Linux CI (cross-platform determinism) |
| `rpg_test --cultures 2000 --seeds 1..20` | the distinctness thresholds of §5.4; the distance histogram; banned-name check (IP list from R20 + a profanity list) |
| `rpg_test --audit-endless 3000` | the repetition audit, extended. POI kinds within 60 tiles of the start ≥ 8; (culture, archetype, layout, wealth) duplicates within 1500 tiles ≤ 2; settlement-name duplicates within 3000 tiles = 0; vignette kind repeats within 120 tiles ≤ 1; greeting variety per town ≥ 70 % distinct |
| `rpg_test --history 2000 --seeds 1..20` | the realm soak: event rates inside the §4.3 ranges; no kingdom above 40 % of horizon settlements (in at most 2 of 20 seeds); every number bounded; tick ≤ 1 ms |
| `rpg_test --power-curve` | the time-to-kill and time-to-die table of §7.4 |
| `rpg_test --metrics --seeds 1..5` | today's game-feel table (the first hour must not regress by more than 10 %) |
| `rpg_test --life --seeds 1..5` | 3 simulated days in towns: schedule adherence ≥ 90 %, no NPC stuck more than 20 s, tavern fill at 20:00 ≥ 40 %, a forced raid is defended |
| `rpg_test --home` | plot purchase, building, crops over 20 offline days, animals fed and producing, a placement fuzz of 1000 placements keeping paths valid |
| `rpg_test --save-soak 100` | 100 synthetic hours (teleports, kills, loot, plots, companions, wars): save ≤ 1 MB, round trip byte-identical, every fixture (`save_v1`, `v2`, `v3`, …) loads |
| `rpg_test --perf-gen` | ms per macro sample block, region plan, edge, settlement, chunk and bake (fails on more than 2× the budget) |
| `embervale --script tools/scripts/*.txt` | screenshot tours per milestone (`endless_walk`, `culture_tour`, `siege`, `town_day_night`, `farm`, `sail`). A reviewer agent looks at them, then they are deleted |
| CI (`web.yml`) | build, `--seeds 1..20`, `--endless --seeds 1..3 --walk 2000`, `--golden`, `--cultures 200` and `save_test` before the Pages deploy. The golden test becomes blocking once it is green on Linux |

### 12.5 How workflows and agents split the work safely
- **Phases per milestone:**
  - **A, lead (serial, 0.5 to 2 days).** All shared-header and interface changes (`game.h`, `world.h`, `ids.h`,
    `source.h`, `style.h`, `art.h`, `items.h`), stub implementations, a SAVE_VER bump if needed, everything compiling and
    every test green. **After phase A the shared headers are frozen** for the milestone.
  - **B, lanes (at most 3 in parallel).** Each lane owns a disjoint set of files (listed per milestone). A lane may not edit
    a file it doesn't own. If it needs a header change, it stubs locally and reports it, and the lead batches it.
  - **C, integration (lead).** Merge the lanes, run the full suite, the screenshot review and the perf check, then build for
    the web.
  - **D, owner build.** Push to `main`; `web.yml` deploys to Pages; the owner plays on the phone. His feedback goes into the
    next milestone's phase A.
- **Worktrees:** each lane works in its own git worktree with its own build directory (`build-<lane>`, Ninja). The worktree
  and its build directory are deleted at the end of the milestone, since each build is about 300 MB and the owner watches
  disk use.
- **The process cap (about 3 machine-wide)** is enforced by a lock script, `tools/slot.bat` / `tools/slot.sh`. It takes one
  of 3 slots by `mkdir build\.slots\N` (an atomic operation), runs the command, then removes the directory. Every
  `rpg_test`, `embervale --script`, `art_preview` and build goes through a slot. No agent starts headless browsers.
- **Screenshots** go to `%TEMP%` and are deleted after review (the owner's standing preference).
- **Effort:** medium effort for lanes; high effort for phase A of M1 and M4 (the riskiest refactors).

---

## 13. Roadmap: playable slices to the phone

Order of priority: **foundation, then the identity of places, then life, then depth.**
- The endless foundation (M1 and M2) comes right after a short M0, so cultures (M3) and kingdoms (M4) are built on region
  plans and IDs from day one.
- Relief (M11) is deliberately late. The owner said not to worry about mountains, and M2's first aid (ranges, passes, snow
  peaks, hillshade) removes the brown blobs cheaply.
- Every milestone ends with a web build on the owner's phone, and the classic island keeps working throughout.

### Where the queued requests land

| Queued request | Milestone | Notes |
|---|---|---|
| Visible equipment for every slot + paper-doll screen | **M0** (now) | culture armour silhouettes extend it in M6 |
| Character creator | **M0** (now) | homeland culture and personal heraldry added in M3 |
| Cluttered, lived-in interiors | **M0** (now) | culture furniture in M3; NPCs using furniture in M5 |
| Monsters entering towns attack villagers, who flee or defend; guards protect | **M0** (now), on today's world | night raids by monster pressure in M5 |
| Bounty turn-in clarity | **M0** (now) | |
| Redo building roofs | **M0** (roof library, chosen by biome) | culture-driven in M3, with no rework because the API is `ArchStyle` from day one |
| Zoomable world map with detail | **M2** | an endless world needs it anyway |
| Smoother biome transitions | **M2** | macro ecotones + dithered blending |
| Organic, non-boxy settlements (standing direction) | **M1** (keeps the organic generator), **M2** (curved roads), **M3** (layout styles all built organically) | |
| Kingdom system with guard protection for member settlements | **M4** | |
| NPC jobs + daily schedules, taverns in the evening, eating, socialising | **M5** | needs the census and NPC IDs (M1) |

**Run now, before the foundation:** all of M0. It touches only art, HUD, interiors, AI and quest UI, which are independent
of world structure. **Don't run before the foundation:** NPC schedules (they need `NpcId` and the census), kingdoms (they
need the lattices), and culture-driven roofs beyond the biome table (they need the culture engine).

M0 starts **after the in-flight workflow (harness, saves and world-gen versioning, combat feel) lands**, because M0's lane
C edits `game.cpp`'s AI.

**Calendar:** about 23 weeks of agent work at 1 to 2 weeks per milestone (M0 1.5, M1 2, M2 2, M3 2, M4 2, M5 1.5, M6 2,
M7 2, M8 1.5, M9 1.5, M10 2, M11 1.5, M12 1.5).

---

### M0, "Now": the queued wins that need no foundation (1 to 1.5 weeks)
**Goal:** visible wins on today's world, plus the refactors that make parallel work possible later.

**Phase A, lead:**
1. Split `art.cpp` into `rpg/art/*` with a pixel-identical hash check. Move `game.cpp`'s AI to `ai.cpp`. Split
   `tools/rpg_sim.cpp` into `tools/tests/`.
2. Add headers:
   - `Actor::faction` + `factions.h`;
   - `Map::deco` (a byte per tile);
   - new item slots: `ItemKind::Gloves, Boots, Cloak` and `eqGloves`, `eqBoots`, `eqCloak`;
   - `HumanLook` armour and weapon looks;
   - `rpg/culture/style.h` with `ArchStyle`;
   - the appearance block.
3. The next SAVE_VER (appearance + 3 new slots) with a fixture.
4. Prep, unused by gameplay: `rpg/world/{coords,ids,dmath,noise,jobs}.h`, a golden test of the noise, and the
   `tools/slot` lock.

**Phase B, lanes:**

| Lane | Owns | Tasks |
|---|---|---|
| A, hero | `rpg/art/art_human.cpp`, `rpg/art/art_items.cpp`, `rpg/view/paperdoll.cpp` (new), `rpg/view/creator.cpp` (new), `rpg/view/hud.cpp`, `rpg/sim/items.cpp`; in `game.cpp` only `resetPlayer`/`recalcPlayer` | Visible equipment for every slot: helmet styles by band, body armour by band, gloves, boots, cloak, the shield shape, the bow carried on the back, the staff, an amulet glint. Paper-doll screen: a 4× rotating preview, slots arranged around it, stat deltas, touch-sized targets. Character creator after New Game: build, skin (8 tones), hair style (6 + 2 new), hair colour, beard, clothing colour, a name (random button + letter picker) |
| B, homes | `rpg/art/art_building.cpp`, `rpg/art/art_props.cpp`, `rpg/sim/dungeon.cpp` (`genInterior`), `rpg/view/render.cpp` (`deco` drawing) | Cluttered interiors: the 5-pass generator of §10.5 with about 34 new props, gated by `Bldg::genVer` (the next WORLDGEN version), with BFS validation. The roof library: hip, gable, steep, flat with parapet, dome, conical, turf, pagoda × the roof materials, plus a temporary **biome table**: snow gets steep slate with smoking chimneys, desert flat adobe with a parapet and awnings, swamp steep thatch on stilts, autumn timber and thatch, plains mixed hip and gable. This is PLAN.md task 3's regional palettes on the final API |
| C, town defence | `rpg/sim/ai.cpp`, `rpg/sim/game.cpp` (spawning, kill), `rpg/sim/game_rpg.cpp` (quests, dialogue) | Factions and the hostility matrix. Monsters target villagers and guards (1.5× weight on the player). Villagers flee home; militia; guards converge; the town bell; leash rules inside towns. **Bounty clarity:** a completed quest's marker points at the giver, a world-space "!" over givers with a reward waiting (drawn by lane B in `render.cpp` from a sim flag), the first dialogue option "COLLECT BOUNTY (+N GOLD)", the toast "BOUNTY READY: RETURN TO X IN Y" and a journal line |

**Verification:**
- `rpg_test --seeds 1..20` all OK. `save_test` loads v1 and v2 and round-trips the new fixture.
- A new interior check: 200 interiors per seed are BFS-valid with at least 60 % of the floor free.
- A new defence check: 3 wolves in the start village are killed by the guards within 30 s, with at most 1 villager down.
- Scripts `paperdoll.txt`, `creator.txt`, `interiors.txt` (inn, house and smithy on seeds 2, 7 and 99) and `defence.txt`.
- An `art_preview` gallery of roofs, armour pieces and clutter props.

**What the owner sees on the phone:** a character creator; the hero visibly wearing every piece of gear, on a proper equip
screen; homes, inns and smithies full of mugs, books, herbs and laundry; snow towns with steep slate roofs and desert towns
with flat ones; wolves chasing villagers who run indoors while the guards fight and the bell rings; bounties he can't miss.

---

### M1, "Beyond the Edge": the endless foundation as a preview (2 weeks)
**Goal:** "NEW JOURNEY (PREVIEW)" on the title screen. Walk forever across endless terrain with rivers, coasts and the
organic towns. The classic game and old saves are unchanged.

**Phase A, lead (high effort, 3 days):** the §2.10 steps 2 to 4 and 6: IDs everywhere, `SiteTable` handles, the
`WorldSource` interface with `LegacySource`, the Active Window running the classic island, and save loading of v1 and v2
into `LegacySource`. **Gate:** classic worlds are bit-identical (fingerprints on 36 seeds), `save_test` is green, and
`--seeds 1..20` is green.

**Phase B, lanes:**

| Lane | Owns | Tasks |
|---|---|---|
| A, world | `rpg/world/macro.cpp`, `hydro.cpp`, `region.cpp`, `chunkgen.cpp`, `settlement.cpp`, `cache.cpp` | L0 continents, plates, ridges and passes, climate (with the origin bias). L0.5 spring-traced rivers and lakes. The L1b settlement lattices and archetype tagging. The L2 chunk pipeline steps 1 to 3 and 6. `settlement.cpp` (a copy of `stampSettlement` on a local buffer, main streets aimed at real neighbour bearings, the census as today's folk and guards). Start guarantee part 1 (the start village, the capital with a Keep). Dens per region (a port of `dens()`) |
| B, sim | `rpg/world/window.*`, `rpg/sim/game.cpp` (spawning and maps), `rpg/sim/game_rpg.cpp` (fast travel, respawn, saves), `rpg/main.cpp` | Recentring and shifting of actors, projectiles and pickups. Site activation, dens and wild spawns on global coordinates. Discovery through `SiteTable`. Interiors from `Gid` seeds. Fast travel and respawn within known sites. Region deltas in the SAVE_VER after M0's. `--endless` and `--at X,Y` flags; the title option |
| C, view | `rpg/view/terrain.cpp`, `render.cpp`, `worldmap.cpp` (new), `hud.cpp` | Global chunk texture keys, the per-chunk bake snapshot, `Ev::WindowShift` (camera, particles, texts), the minimap from the window, a first world map (Z2 from macro samples + the explored mask), the title entry, and the `--perf` gen/bake counters |

**Verification:**
- The classic suite is unchanged.
- `--endless --seeds 1..10 --walk 6000` (determinism, seams, density, start guarantee).
- `--golden` on Windows, and Linux in CI.
- `--perf-gen` within budget.
- The `endless_walk.txt` script: 3 minutes heading east, a shot every 20 s, reviewed for seams. A window-shift stress test
  (circling across thresholds) with no position jumps.
- The web build in Chrome at 4× CPU throttle: no hitch over 50 ms on a shift.

**What the owner sees on the phone:** a new title option. He walks or runs in any direction and it never ends. Rivers wind
to real coasts, mountain ranges have passes, and new villages and towns with their own names keep appearing, with
innkeepers, shops, interiors and wolf dens. He has no dungeons or roads yet in the preview, and he says so in feedback.

---

### M2, "Wayfinder": endless parity, the zoomable map, endless as the default (2 weeks)
**Goal:** NEW GAME becomes endless, with everything the island had. The zoomable map arrives.

| Lane | Owns | Tasks |
|---|---|---|
| A, world | `rpg/world/roads.cpp`, `sites.cpp`, `region.cpp`, `macro.cpp` | Road graph (Gabriel, coarse A*, Chaikin, bridges and fords, gate bearings). Site lattices for caves, ruins, camps, shrines and lairs, plus wonders v0 (4 templates). Vignettes (PLAN.md task 4 templates as POIs). Start guarantee complete (shard ruins, the lair, at least 8 POI kinds within 60 tiles). Mountain first aid (rock ≤ 10 %, peak props, snowcaps). The biome blend byte |
| B, sim | `rpg/sim/game_rpg.cpp`, `game.cpp` (quests and travel), `rpg/sim/events.cpp` (new) | The main quest on endless (from the start plan). Radiant quests over region plans with `Gid` targets, plus **PLAN.md task 6** quest types (deliver, heirloom, missing person, named bandit, protect the farm) with a text grammar. Fast travel with the landmass rule and travel time. The known-sites table. Rumours v0 (innkeepers point at undiscovered sites). Switch the default to endless |
| C, view | `rpg/view/worldmap.cpp`, `terrain.cpp`, `hud.cpp`, `rpg/art/art_nature.cpp` | **The zoomable map:** Z0 to Z3, quadtree tile textures, pinch, drag, wheel, Q and E, fog of war, markers, legend, hillshade, mountain glyphs, named ranges. **Biome transitions:** dithered ecotones in `groundPixel` and transition flora. A peak sprite with a snow cap |

**Verification:**
- Every classic check ported to endless (reachability, the main quest end to end on 10 seeds with a teleporting bot, quest
  flows per type, death and respawn, shop restock).
- `--audit-endless` thresholds.
- `--save-soak 20` keeps the save ≤ 150 KB.
- Map screenshots at each zoom; touch pinch checked on the phone by the owner.

**What the owner sees on the phone:** New Game drops him into an endless world with the same first hour (the Jarl, three
shards, Ashfang). Curving roads run between towns forever, with dungeons, camps and shrines everywhere. A map he can pinch
from street level up to the continent, with fog of war, rivers, roads and named mountain ranges. Biomes blend instead of
switching. Mountains are snowy ranges, not brown blobs.

---

### M3, "Many Peoples": the culture engine v1 (2 weeks)
**Goal:** crossing a border changes everything you see and hear.

| Lane | Owns | Tasks |
|---|---|---|
| A, culture | `rpg/culture/*` | The data model, 10 archetypes, mutation, dialects, phonology (place, person, landmark and dungeon names for endless; the classic keeps `kPre`/`kSuf`), heraldry, the distance metric, the four-phase maximin on the culture and kingdom lattices, basic religion and customs, and the `--cultures` test |
| B, art | `rpg/art/art_building.cpp`, `art_human.cpp`, `art_props.cpp`, `art_heraldry.cpp` (new) | The building painter fully driven by `ArchStyle` (wall materials, windows, doors, ornaments, foundations including stilts, the damage variant later). City wall styles, fences, style-variant props, the dress grammar (cuts, headwear, patterns, skin and hair ranges), banners, glyphs, and an `art_preview --cultures` gallery |
| C, world and audio | `rpg/world/settlement.cpp`, `engine/audio.*`, `rpg/sim/dungeon.cpp` (furniture styles) | Layout styles (Grid, Radial, Linear, Compound, Terraced, Stilt, all with organic wobble, irregular lots and setbacks). **PLAN.md task 3 archetypes** (fishing with docks, mining with a mine mouth and carts, farming, river crossing, hill fort, market town). Census looks from the culture. `MusicStyle` with 12 scales and the border switch of music. Interior furniture by culture. The character creator gains a homeland choice and personal heraldry |

**Verification:**
- `--cultures 2000 --seeds 1..20` meets the thresholds.
- The `--audit-endless` signature duplicate rule.
- Gallery PNGs of 12 random cultures, reviewed.
- A `culture_tour.txt` script crossing 3 family borders with screenshots.
- Name checks (the IP list, profanity).
- 10 town pieces rendered to WAV by `audio_preview` for the owner to judge by ear.

**What the owner sees on the phone:** walk 8 minutes east and the turf-roofed log halls, horn music and furs give way to
flat-roofed courtyards, a drum-and-oud piece, kaftans and turbans, and names like "Qasr Amun". Every kingdom flies its own
banner. Imperial cities are laid out on grids, marsh villages stand on stilts, mining towns cling to ridges.

---

### M4, "Banners": kingdoms and the living world v1 (2 weeks)
**Goal:** a world that moves, with war you can join, and guards that protect member settlements.

| Lane | Owns | Tasks |
|---|---|---|
| A, realm | `rpg/sim/realm.cpp` (new) | Genesis, history pre-roll, the daily tick (§4.3: economy, relations, discovery, trade, wars, sieges, capture or raid, peace, fall), the LOD tiers, the save block, the event log, and `--history` with tuning to the §4.3 targets |
| B, materialisation | `rpg/sim/game.cpp` (spawning), `rpg/world/settlement.cpp` (overlays), `rpg/sim/ai.cpp` (kingdom factions) | `SettlementState` overlays: owner banners and guard gear, garrison towers, charred buildings (with art help), siege camps, refugees. **Kingdom guard protection:** guard counts scaled by the kingdom's military, villages guarded at last, road patrols, independents with militia only. Reputation; siege quests ("break the siege", "join the assault") feeding `atk`/`def` |
| C, UI | `rpg/view/worldmap.cpp`, `hud.cpp`, `rpg/sim/events.cpp` | Borders on the map (Voronoi of owned settlements), war markers, rumours and gossip lines, notice boards, heralds, a "News" journal tab, the border-crossing banner ("ENTERING THE KHAGANATE OF …"), the danger skull |

**Verification:**
- `--history 2000 --seeds 1..20` inside the target ranges, with nothing unbounded.
- The tick takes ≤ 1 ms.
- Save round trips with the realm block.
- `siege.txt` (forced with `--event siege`): the player joins the defence and `def` rises. Screenshots of an occupied town,
  a burned village and a refugee camp.

**What the owner sees on the phone:** a herald's banner at every border. Villages now have guards in their kingdom's
colours, and patrols walk the roads. Taverns talk about a war two valleys over. He finds the siege, picks a side and turns
it. A week later a burned village and its refugee camp, and a conquered town flying new colours with a foreign watchtower.

---

### M5, "Hearth and Hall": NPC life (1.5 weeks)
**Goal:** towns live by the clock.

| Lane | Owns | Tasks |
|---|---|---|
| A, life | `rpg/sim/census.cpp`, `schedule.cpp`, `ai.cpp` (residents) | Census use, schedule templates, spawning by schedule, door-to-door routes, interiors populated by schedule, tavern seating and service, chat pairs, gossip, raids by monster pressure, militia, the bell |
| B, art | `rpg/art/art_human.cpp`, `art_props.cpp`, `art_monster.cpp` | Frames for sitting, eating and drinking, working with tools, sleeping, waving and playing; tool props; village dogs, cats and chickens |
| C, ambience | `engine/audio.*`, `rpg/view/render.cpp` | `Music::Tavern` in the culture's style, the bard, festival dancing, speech-bubble icons, lamplighting (the lamppost's lit state), stalls opening and closing, window light by occupancy |

**Verification:**
- `--life --seeds 1..5` thresholds.
- 120 residents cost ≤ 1.5 ms.
- `town_day_night.txt`: screenshots at 6:00, 12:00, 20:00 and 2:00 in 3 cultures.

**What the owner sees on the phone:** the smith hammering at noon, farmers in the fields, a full tavern at night with a bard
playing the local tune, children playing tag, lamps lit at dusk, and a bell ringing when wolves come at midnight.

---

### M6, "Steel": arms, armour, items and enemies (2 weeks)

| Lane | Owns | Tasks |
|---|---|---|
| A, numbers | `rpg/sim/items.*`, `game_rpg.cpp` (shops, rewards), `game.cpp` (damage, XP) | Item level and bands, level sync, the rarity budget, affixes, legendaries and attunement, **the shop-band and quest-reward fixes**, XP decay, the level-gap penalty, mitigation, the potion cooldown, merchant gold. `--power-curve` and tuning to the §1.4 hours |
| B, arms art | `rpg/art/art_human.cpp` (arms), `art_items.cpp`, `art_monster.cpp` | The `ArmsStyle` grammar (9 helm forms, 7 bodies, 8 shields, 7 blades, polearms, bows), culture weapon icons, monster variant overlays, elite auras, 2 new families (harpy, golem) |
| C, foes | `rpg/sim/ai.cpp`, `rpg/world/region.cpp` (uniques) | Elite affix behaviours, champion packs, named uniques with their rumours, boss phases, world bosses (roaming, lairs, the raid hook into the realm), soldiers and bandits in culture arms |

**Verification:**
- `--power-curve` within targets.
- `--metrics` for the first hour within ±10 % of M5.
- A loot audit of 10,000 drops per band (rarity distribution, no legendary below D 10, no shop item above band + 2).
- A gallery of 10 culture armour sets.

**What the owner sees on the phone:** every land's soldiers and bandits look different. Loot carries local names and
materials. Glowing elites and named beasts appear that innkeepers warn about. The skull icon turns red as he heads into far
country, and it means it.

---

### M7, "Home": housing, land, farming, animals and mounts (2 weeks)
- **Lane A, sim** (`rpg/sim/housing.cpp`, `farming.cpp`, saves): houses for sale, plots, build jobs, crops, seasons, the
  farmhand, cooking, livestock, predators, horses and mounting, the offline catch-up.
- **Lane B, view** (`rpg/view/buildmode.cpp`, `hud.cpp`): the build and decorating UI and the storage UI.
- **Lane C, art** (`rpg/art/*`): about 15 crops × 4 growth stages, 7 animals, farm objects, scaffolding, house shells per
  culture, trophies and paintings.

**Verification:**
- `--home`, including a fuzz of 1000 placements.
- A save of ≤ 4 KB per farm region.
- Mount rules (the dismount triggers).
- `farm.txt` screenshots.

**What the owner sees on the phone:** buying a cottage or a riverside plot, building a house in any style he has
discovered, planting barley, keeping chickens and a cow, and riding a horse down an endless road.

---

### M8, "Fellowship": companions (1.5 weeks)
- **Lane A, sim** (`rpg/sim/companions.cpp`, `ai.cpp`): data, roles, commands, the deeds bus and approval, bond skills,
  downed and recovery.
- **Lane B, dialogue** (`rpg/sim/events.cpp`, `game_rpg.cpp`): the banter grammar (culture-border comments first),
  personal quests generated from hooks.
- **Lane C, view** (`hud.cpp`): the command radial, the companion paper doll, party portraits.

**Verification:** `--companion` (25 to 40 % of the damage share across 3 bands, no follow stuck more than 30 tiles, approval
reacts to scripted deeds).

**What the owner sees on the phone:** a tavern regular joins him, fights beside him, grumbles about the desert heat, and asks
for help avenging their burned home village.

---

### M9, "Arcana": magic traditions (1.5 weeks)
- **Lane A, sim** (`rpg/sim/magic.cpp`): the tradition model, spell grammar and budget, the mainland signatures (1 and 5 to
  9), mastery and attunement.
- **Lane B, FX and audio:** `FxStyle` effects, glyphs, spell motifs in the culture's scale.
- **Lane C, content:** tomes, mage teachers, 3 trial templates (`dungeon.cpp`).

**Verification:** `--spells` (every spell within ±5 % of its budget, tradition DPS within ±12 % of Common at equal item
level), plus a screenshot of each signature.

**What the owner sees on the phone:** learning Starcalling in a northern kingdom (spells that only charge at night) and
seeing and hearing spells in each culture's colours and scale.

---

### M10, "Beyond the Mist": the sea, ships and hidden lands (2 weeks)
- **Lane A, world:** second continents and archipelagos, hidden lands and the Mist, sea POIs, ports and docks.
- **Lane B, sim:** sailing, boats, ferries, storms, sea encounters, the sea-chart chain, first contact, the hidden
  signatures (2, 3, 4, 10, 11 and 12) and their trials.
- **Lane C, art and audio:** ships in each culture's style, the serpent, kraken and merrow, waves, foam and mist, and the
  look of the isolated cultures.

**Verification:**
- A `--sail` bot reaches continent 2.
- The Mist turns the ship back without the chart and lets it through with it.
- A hidden kingdom stays isolated until contact.
- Streaming throughput at sea.
- `sail.txt` screenshots.

**What the owner sees on the phone:** a rowboat, then a sloop, open sea, storms and islands. After a long rumour chain, the
Mist parts on a kingdom nobody knew, whose priests sing fire into being.

---

### M11, "High Ground": terrain relief (1.5 weeks)
- **Lane A, world:** height levels, the ramp lattice, plateau minimums, waterfalls, road stairs.
- **Lane B, view:** cliff faces for every ground, slopes, aerial grading, cast shadows, waterfall animation, peaks.
- **Lane C, sim:** slope speed, AI pathing over ramps, map contours.

**Verification:** 100 % reachability of plateaus of 24 tiles or more in the sampled windows; guards and companions path
across ramps; the bake cost rises by at most 20 %.

**What the owner sees on the phone:** hills, cliffs, plateaus and waterfalls. Climbing to a peak shrine is a journey, and
mountains read as height.

---

### M12, "Rise and Fall": the deep world sim (1.5 weeks)
- Civil wars and successor states, warlord realms from bandit camps, resettlement of ruins, world bosses raiding towns.
- Enlisting with ranks and campaign quests, diplomacy quests.
- Village lordship, if the owner wants it (§14).

**Verification:** `--history 5000`, drama metrics, and `--save-soak 100` at ≤ 1 MB.

**What the owner sees on the phone:** realms that rise and fall around him, and a place in that history.

**Filler lanes** (can run whenever an agent slot is free, because they are independent of world structure):
- PLAN.md task 5, dungeon variety (`dungeon.cpp`: room templates, cave themes, keys and levers);
- weather fronts and seasons (before M7);
- ambient wildlife;
- the iOS and TestFlight pipeline (PLAN.md task 7; the owner sets the secrets).

---

## 14. Risks, open questions and what to cut

### 14.1 Risks and mitigations
| Risk | Impact | Mitigation |
|---|---|---|
| Generation and terrain baking on the web main thread are too slow on an iPhone | Hitches when walking or riding | Measure in M1 phase A with Chrome at 4× throttle and on the phone. Sliced jobs at 3 ms per frame. A 2-chunk prefetch ring. If needed, 256-px chunk textures on the web and per-tile pre-computation in `groundPixel` |
| Order-dependent generation (a hidden use of a shared RNG or a cache state) | Saves pointing at things that moved | The order-independence contract (§2.3), 3-order shuffle tests, golden hashes, region plan fingerprints |
| The ID, handle and window refactor breaks the classic game | Lost saves, regressions | `LegacySource` first, bit-identical fingerprints on 36 seeds, the save fixtures, `world.cpp` frozen |
| Generated cultures look like noise, not design | Places feel random, not distinct | Archetype priors, curated palettes, gallery review every milestone, owner veto on archetypes |
| "Procedural oatmeal": endless but samey | Boredom after hour 3 | The uniqueness stack (§1.3), wonders, named uniques, sim history, audits that fail CI |
| Sim runaway (one empire eats everything, or nothing happens) | A dull or broken world | The §4.3 targets in soak tests, tuning knobs (`--sim-pace`), fall and split mechanics |
| Save bloat over long play | Slow saves, storage limits | Tagged-record budgets, `--save-soak` caps |
| The art workload (hundreds of new sprites) | Slow milestones | Parameterised painters and overlays, rig reuse, `art_preview` galleries |
| Parallel lanes clobbering each other, or the machine overloaded | Lost work; a PC crash (as before) | Worktrees, file ownership, frozen headers, the 3-slot lock |
| Float precision far from the origin | Jitter, broken noise | Floating origin, integer structural noise, wrapped cosmetic noise |
| IP lookalikes (Elder Scrolls names) | Store and legal risk | The R20 policy as a banned-word list in the name generators, tested in `--cultures` |
| The owner pivots mid-arc | Wasted depth | Each milestone is a playable slice behind flags, and the cut list below |

### 14.2 Questions only the owner can answer
1. **The start of a new game:** begin on the endless mainland (recommended), or keep the classic island as a starting isle
   you sail away from into the endless world?
2. **Companion death:** "downed, then recovers" by default with an optional Hardcore permadeath (recommended), or always
   permadeath?
3. **How dramatic the world should be:** "lively" (a war nearby most weeks, kingdoms can fall within a couple of in-game
   weeks; recommended as the default, with a setting), or "slow and realistic"?
4. **The late game:** should you be able to become a lord (own a village, collect taxes, raise your banner, maybe found a
   kingdom)? Yes adds about 1.5 weeks to M12.
5. **Peoples:** humans only, with huge cultural variety (recommended for art cost), or one non-human people in a hidden
   land (a new body rig, about 1 extra week)?

### 14.3 If scope must shrink (cut in this order)
1. M12's extras (civil wars, warlords, enlisting, lordship). Wars v1 from M4 stay.
2. M11's full relief. M2's first aid (ranges, peaks, snowcaps, hillshade) stays.
3. Galleons and storms. Rowboats, ferries and **hidden valleys** (instead of the Mist) stay.
4. Animals other than chickens and horses; paintings and trophies.
5. Companion personal quests and the banter grammar. Hirelings plus 3 hand-written companions stay.
6. Magic: 6 signatures instead of 12 (Common, Starcalling, Tidebinding, Emberforge, Songweaving, Runecarving).
7. Kingdom dialects. Families stay, and each kingdom's identity is its heraldry alone.

**Never cut:**
- the M1 and M2 foundation (with its determinism and save versioning);
- M3 culture families;
- M4 kingdoms with guard protection;
- the slow-roll rules of §7.5 (lane A of M6);
- the test suite.

## 15. Addendum: owner requests received while this plan was being written (2026-10-03)

These are binding inputs. Each is mapped onto the milestones above. Where it conflicts with the main text, this section wins.

### 15.1 Character creator and backgrounds (M0)
- **Everyone starts with just the shirt on their back.** No starting weapon or armour beyond a basic shirt and trousers. Backgrounds grant a *special trait*, never gear.
- Looks: name, skin tone, hair style and colour, beard, eye colour, clothing colours, randomize. A large animated preview.
- Backgrounds each give one distinctive perk plus a small story hook. Candidates:
  - Blacksmith's child: cheaper smithing, can repair gear.
  - Hunter: animals are noticed sooner, better pelts, wolves slower to aggro.
  - Temple novice: knows a weak heal, longer shrine blessings.
  - Street urchin: better shady prices, simple lockpicking.
  - Farmhand: stamina regen, food heals more.
  - Exiled noble: more quest offers from guards and Jarls, a bounty in one kingdom.
  - Sailor: swims farther, can buy and sail boats early (ties to M10).
  - Marked one: rare, a faint affinity with a lost magic tradition (a hook into M9 and M10).
- The early balance must be retuned for an unarmed start (the first weapon comes within the first 5 minutes from the start village: a quest, chest or loot).

### 15.2 Hunger and sleep: buffs, not chores (M5, with taverns and food)
- Meals give "Well Fed" (regen and stamina for a while). A long time without food gives "Hungry" (slower stamina regen). **It never kills the player** in the default mode.
- Sleeping in a bed (inn, home, bedroll) gives "Rested" (a small XP bonus) and skips the night. Days without sleep give "Weary" (a mild penalty).
- Cooking at campfires and inns makes better meals. Fish, crops and livestock (M7, M10) feed into it.
- An optional "Survival" setting adds real hunger, thirst and cold.

### 15.3 Investigable ruins from simulated history (M4 creates them, M12 deepens them; the M3 culture data makes them readable)
- Ruins are the remains of settlements and kingdoms that actually fell in the pre-play history sim or during play: war, plague, flood, collapse. Every ruin has a true record behind it (who built it, when, how it fell).
- What the player finds:
  - journals, letters, culture-script inscriptions, murals, graves with names and dates;
  - toppled statues of named rulers;
  - relics in that culture's style: arms, coins, jewelry and unique items.
- Investigation fills a "Lost History" journal entry. Clues lead onward: map fragments across the sea, half-translated scrolls of dead magic traditions, living descendants in present-day cities, vaults that need keys from other ruins.
- Living NPCs and kingdoms remember this history: dialogue references, and collectors or museums that pay for relics.

### 15.4 Harbors, boats and fishing (M10, with fishing pulled earlier into M5 or M7 as a standalone loop)
- Coastal cities get harbors: docks, moored ships, fishermen, harbourmaster, sailors' tavern.
- Buy passage to known ports. The voyage plays out on deck with random events (pirates, storm, sea serpent). Captains sell rumours of unmapped shores.
- Own boats in tiers: rowboat (lakes, rivers, coast), sailboat (seas), ship (cargo, companions). Sail them manually, with wind, deep-water danger tiers, reefs, storms, pirates and sea monsters. Boats can sink, can be docked, can be upgraded, and can have a dock at a coastal home.
- Fishing mini-game (cast, bite, then a timing/tension reel). Species vary by water type, biome, season and time of day, with rare and legendary fish. They feed cooking, selling and fishmonger quests.

### 15.5 Magic: Skyrim plus Baldur's Gate 3, many builds (M9; this extends section 6)
- **Skyrim layer:**
  - Schools levelled by use: Destruction, Restoration, Conjuration, Illusion, Alteration. Section 6's culture-bound traditions sit on top as additional schools.
  - Spell tomes.
  - Dual-casting (combine or overcharge).
  - Perk trees per school.
- **BG3 layer:**
  - Elemental surfaces and combos: ice, water, oil, fire, poison clouds, electrified water, rain dousing fire.
  - Concentration spells that break on heavy hits.
  - Upcasting: hold to pour in more power.
  - A limited prepared-spell loadout, swapped at rest or camp. This is also what keeps the phone UI to 3–4 quick-cast buttons.
  - Reaction casts (counterspell/shield on a telegraphed enemy cast).
  - Rich status effects: burning, chilled, frozen, wet, shocked, frightened, charmed, poisoned, blinded.
- **Builds to support and test:** spellblade, necromancer, battlemage, illusionist, elementalist, paladin, ranger-mage, lost-tradition mystic.
- **Slow roll:** start with nothing (background aside). Schools level slowly. Strong spells come from rare tomes and higher skill. The most powerful magic is gated behind ruins, lost traditions and distant lands.
- **Touch input:** tap to cast, hold to upcast, swipe gestures for dual-cast.

### 15.6 Owner answers to the section 14 questions (2026-10-03)
1. **Start:** new games start on the endless mainland (no starter island).
2. **Companions: permadeath.** A companion who dies is gone for good (the player character is NOT permadeath). This makes the player
   careful and makes the bond matter. The companion system must therefore be deep enough to build attachment:
   - a personal history and personality, opinions on the player's choices;
   - approval and loyalty;
   - personal quests;
   - banter with each other and with the world;
   - progression and a gear loadout;
   - clear danger feedback, so a death is never a cheap surprise:
     - low-HP warnings;
     - a "downed, bleeding out" window (a few seconds to revive or carry them out) before the death is final;
     - an option to tell them to hold back or wait at camp, an inn or home;
   - meaningful remembrance after a death: a grave you can visit, their belongings, and other NPCs who remember them.
3. **World drama: slow and realistic.** Diplomacy erodes gradually through causes:
   - a famine;
   - a failed harvest that breaks a food trade deal;
   - border tension;
   - skirmishes;
   - war.
   
   Wars are rare, have reasons, and are foreshadowed by rumours, prices, refugees and troop movements. There is no "lively" default.
4. **Late game: yes.** After a long, long time the player's renown can become great enough to found a village and grow it into a
   kingdom, or to conquer an existing one. This adds roughly 1.5 weeks to M12, and its renown and land hooks should be designed in from M4 and M7.
5. **Peoples:** humans, half-breeds and elves from the start (all humanoid, so they share the human rig with per-people proportions,
   ears and colouring). Other peoples later.

### 15.7 Interiors must make logical sense (owner, 2026-10-03). This binds every interior generator and art pass.
- **Multi-storey buildings have real upper floors.** If a building's exterior shows 2+ storeys (inns, taverns, keeps, larger houses,
  towers), its interior has stairs (a staircase prop on the ground floor) leading to a separate upper-floor map, and the upper floor is
  entered and left by those stairs.
- **Inns and taverns:** the ground floor is the public room: bar/counter, kitchen corner with hearth, tables, benches, patrons. Guest
  **beds are never in the common room.** They are upstairs in separate rented rooms, each with walls, a door, a bed, a chest and a small
  table or candle. The innkeeper's own quarters are separate. The room you rent is the one you sleep in.
- **Every room has a purpose that the layout explains:**
  - kitchens next to a hearth/oven;
  - bedrooms private (walls and doors), not sharing open space with the shop counter;
  - shops have the counter between the customer and the stock;
  - smithies have the forge vented at a wall;
  - temples are oriented to the altar;
  - keeps have a throne hall plus private quarters.
- **Interior walls and doors partition rooms.** Furniture is placed against walls in plausible ways (beds head-to-wall, shelves on
  walls, tables with chairs around them), and paths stay clear from the entrance to every room.
- **Exterior and interior must agree:** footprint size, number of storeys, chimney ⇒ hearth, and shop sign ⇒ shop interior.
- **Verification:** an automated check per building type (no bed in a common room; every room reachable; stairs present iff the
  exterior has 2+ storeys), plus a screenshot review of each type.

### 15.8 Settlement scale and spacing, plus kingdom identity (owner, 2026-10-03). This binds M1 and later.
- **Spacing:** settlements must be **much, much further apart** than in the classic island. Travel between them should feel like a
  journey through wilderness: roads with points of interest, camps, dens and ruins in between. Use the endless world's room for
  this; don't pack sites together.
- **Size targets (houses/homes, not counting service buildings):**
  - villages: **10–15** houses;
  - towns: **40–60** homes;
  - cities: **huge, 160–220** homes, with districts, several squares, markets, walls and gates.
  
  Perf and mobile budgets must be planned for big cities (chunk streaming, actor LOD and sleeping NPCs).
- Every village gets its core services. The owner's sentence was cut off ("have all villages have..."); assume at least an inn or
  tavern, a well or green, and a shop or smith. Confirm with the owner.
- **Kingdom identity must be obvious:**
  - banners, guard tabards and colours of the ruling kingdom in every settlement;
  - a sign or arrival banner naming the town and its kingdom;
  - the map shows ownership.
  
  **Capital cities are unmistakable:** the king's palace (a large walled palace or castle compound with a throne hall, gardens,
  barracks and a royal guard), grander walls and the royal banner.

### 15.9 Quest and campaign engine, and a rags-to-riches economy (owner, 2026-10-03)
- **Three quest tiers:**
  1. **Generic radiant quests** (bounties, hunts, deliveries), kept, but phrased and framed with variety.
  2. **Story quests:** small hand-authored-style stories generated from templates, with real dialogue (branching, characters with
     motives, a twist or a choice, consequences that persist: an NPC remembers, a family moves, a shop changes hands).
  3. **Campaigns:** large, multi-stage story arcs (10–30+ quests) that the player can stumble into anywhere in the endless world.
     Examples:
     - a succession crisis;
     - a plague cult;
     - a lost heir;
     - a rebellion;
     - a dragon cult;
     - an ancient magic tradition awakening.
     
     They have recurring characters, factions, betrayals, branching outcomes that change the world (who rules, which town survives),
     and companions with stakes in them.
- **Engine requirements:** a data-driven quest/campaign DSL (stages, objectives, conditions, dialogue trees, variables, world-state
  hooks), plus a generator that casts real world entities (this kingdom, that ruin, this NPC, the history sim's events) into roles, so
  stories come out of the living world rather than floating above it. A writing-quality bar: no generic filler in tier 2 and 3
  dialogue; templates with enough authored variation and specificity. Tests must walk every campaign path headlessly.
- **Economy: slow rags to riches.** The player must not get rich fast or get great gear fast.
  - Money sinks: lodging, food, repairs, training, property, taxes, bribes.
  - Prices follow the region's economy (scarcity and wars move them).
  - Rewards scale with place and danger, not player level.
  - Great gear is rare and earned (dungeons, campaigns, crafting with rare materials), and selling loot has diminishing returns
    (merchant gold limits, saturation).
  - A balance sim (bot playthroughs measuring gold/hour and gear tier over 1, 5, 20 and 50 hours) gates every economy change against
    target curves.
- **Roadmap placement:**
  - the quest engine and story quests land with M4 (kingdoms give stories their actors);
  - campaigns arrive with M4/M12, the first campaign ships as soon as the engine exists;
  - the economy pass lands with M6 (items), and its balance sim is a CI gate from then on.
- **Story inspiration (owner):** draw on the whole world's storytelling (scripture and ancient myth, folk and fairy tales, epics,
  classic and modern fantasy such as C.S. Lewis and Sarah J. Maas) to build a large library of **story archetypes and plot structures**.
  Examples:
  - the prodigal's return, a betrayal by a brother, an exile and a homecoming;
  - a flood or plague as judgement, a prophecy misread, a rightful heir in hiding;
  - a deal with a fae court, a portal to another realm, a hidden queen, an enemies-to-allies bond;
  - the trickster's bargain, a cursed gift, a sacrificial stand.
  
  The generator combines archetype, cast (from the living world), setting, twist and moral choice, giving effectively millions of
  distinct quests. **Rule:** borrow structures, themes and motifs, never copyrighted names, characters, places or verbatim plots of
  modern works. Public-domain myth, scripture and folklore can be referenced more directly.

### 15.10 Owner rules for M1 (2026-10-04) and what Phase A built
These bind M1 and override the M1 section above where they differ.
- **Old saves are not a concern.** Only the current `SAVE_VER` (5) loads; an older save is refused and the title says
  "THIS SAVE IS FROM AN OLDER VERSION - START A NEW ADVENTURE". `save_test` checks the current format only
  (`tests/fixtures/save_v5.bin` holds format-level facts; round trips are byte-identical). The CI save step is blocking.
- **New games start on the endless mainland** (15.6). `Game::newEndlessGame`; the classic island stays only as a test
  fixture during M1 (`rpg_test --seeds`, scripts with a `worldgen 7` header, `--classic`) and is retired in M2.
- **Scale (15.8):** settlements far apart (a real walk with wilderness, roads, camps, ruins and caves between them);
  villages 10-15 houses, towns 40-60, cities 160-220 buildings; every village has an inn or tavern, a well or green and a
  shop or smith; organic layouts; big cities stay fast on iPhone web (measure with `--perf`).
- **Kingdom identity (M4 groundwork):** every settlement records its kingdom and whether it is the capital (`Site::kingdom`,
  `Site::capital`, `World::kingdoms`); banners in kingdom colours on gates, keeps, palaces and signs (`Bldg::banner`,
  `art::kingdomBanner`); the name label and the world map show the kingdom; capitals get a large multi-storey palace
  (`art::Building::Palace`, `Role::King`) with a throne hall built with the M0b rooms system.
- **Deterministic generation:** integer / `rpg/world/dmath.h` maths only in world generation; `rpg/world/*.cpp` compile
  without fast-math or FMA contraction; golden fixtures must pass on Linux CI.
- **Relief instead of brown mountain blobs** (section 11): `Map::height` carries the level (bits 0-2), cliff faces
  (`HEIGHT_CLIFF`, not walkable) and ramps (`HEIGHT_RAMP`).
- **The world map works with the endless world:** zoomable, panning, discovered areas (`Game::explored`, 1 bit per 8x8
  tiles, saved), kingdom labels.
- **Endless saves store deltas keyed by stable ids** (`Gid`): sites, buildings, dens, map keys and overworld chests
  (`Game::lootKey`) are saved by id, never by handle.
- **Phone screen fill (owner request):** the game image must fill the iPhone screen. High-DPI web canvas, an adaptive
  logical width (the height stays near 270), and a SETTINGS option SCREEN (FILL default on phones, PIXEL PERFECT) with a
  SCREEN BORDER / SAFE AREA slider, saved in its own small settings file (`rpg/view/screen.h`).

**Phase A (lead) contracts:** `rpg/world/source.h` (the generator: macro samples, region plans, chunks, kingdoms, the start
plan; pimpl so internals change freely), `rpg/world/settlement.h` (one settlement on a local buffer),
`rpg/sim/world_endless.cpp` (the Active Window: `World::WIN` = 256 tiles, shifts of 64, append-only records with `Gid`
maps, `Ev::WindowShift`), `rpg/sim/explored.h`, `rpg/view/screen.h`, `RPG_TEST_CMD` in `tools/tests/tests.h`
(`rpg_test --endless`, `rpg_test --window`). The stub generator (`rpg/world/endless.cpp`) and the classic-stamp settlement
bridge (`rpg/world/settlement.cpp` via `classicTownStamp`) are placeholders for the WORLD and TOWNS lanes.

### 15.11 Owner direction, 2026-10-05: markets, village economies, ores, alloys and crafting

- **Markets (binds M1 onward):** the old ring of identical stalls around the square was rejected ("copy pasted in circles,
  doesn't fit, sloppy"). Markets are laid out like real ones: rows along the sides of a square or a street, facing the
  walking space, with clear aisles. Each stall shows a specific trade and its goods. Villages get one cart or one or two
  stalls at most; towns get a cluster; cities and capitals get a proper market square. Other squares get wells, trees,
  benches or statues, never filler stalls.
- **Every village has what it needs to function** (this completes the cut-off 15.8 sentence): a well, an inn or tavern,
  a smith and a small market, plus a mill where it fits (a watermill on a river, a windmill on farmland).
- **Village specialisation:** each village has a trade chosen from its surroundings (farming, fishing, mining, lumber,
  herding...) with the matching production buildings. It records what it produces and what it needs; the M4 economy
  (famine -> trade collapse -> war) runs on this data. Mining villages sell or refine their ore as their main income.
- **Crafting (M6 Steel, with workstations in M7 Home):** a deep, robust system in which the player can, if they want,
  craft everything. Crafting uses **the same production chains as the economy** (ore -> smelter -> ingot -> smith -> item;
  grain -> mill -> flour -> baker -> bread; hide -> tanner -> leather -> leatherworker). The player may do any step or buy
  any intermediate. Material quality, workstation and skill decide the result.
- **Materials:** the universal tiers, known by every culture, are **leather, bronze (copper + tin), iron, steel**. Above
  them, each culture (M3 culture engine) has its own alloys made from regional ores, with its own distinctive look for
  armour and weapons. Travelling far, and sailing to the hidden kingdoms (M10), reveals metals and gear never seen at home.
- **Ores are regional:** they follow geology and biome, so no region has everything and trade and travel matter. The
  generator should record a per-region geology value early (M2) so ores sit in the right places when M6 lands.
- **Culture secrets are discoverable knowledge:** a player can learn a culture's alloy recipes and its armour and weapon
  making by earning trust and apprenticing with a master smith, from lore in ruins (journals, moulds), by slowly breaking
  down pieces at a forge, or by stealing from guarded workshops (at the cost of that culture's goodwill).

### 15.12 Needs-driven citizens (owner, 2026-10-05). This upgrades section 10 for M5.

The owner wants citizens with simulated lives ("needing to eat, going to work, needing a social life") to bring cities to
life. Section 10's job schedules stay the backbone, but behaviour is now driven by **needs**:
- **Needs per resident:** hunger, rest, social, faith/comfort and money (0-100, decaying at trait-dependent rates). Each
  hour a resident picks the activity that best serves its most pressing need, within its job's schedule window
  (utility scoring, deterministic per resident and hour). A hungry worker buys bread at the market on the way, a lonely one
  goes to the tavern, a broke one works longer or begs, and a devout one stops at the temple.
- **Food and goods are real:** residents buy meals and goods from the settlement's bakers, stalls and inn. That consumes the
  stock the village specialisations (15.11) produce, so shortages travel along the trade links. A failed harvest means
  hungry towns, higher prices and falling mood: the start of the M4 famine -> trade collapse -> war chain.
- **Mood is visible:** each settlement has a mood (from its residents' needs, safety and prosperity) that the player can
  see. Content towns have full taverns, music, festivals and children playing. Hungry or war-torn towns have beggars,
  shuttered stalls, brawls, emigrants on the roads and higher crime. Individual NPCs show their state in barks and
  dialogue ("haven't eaten since yesterday").
- **Relationships:** friendships and households from shared work, tavern visits and neighbours; residents seek out friends
  when lonely, greet them by name, and grieve when one dies.
- **Level of detail:** residents near the player are simulated in full as actors; the rest of a settlement is simulated in
  aggregate per hour (needs and stock as totals); far settlements update daily in the world sim. It must stay cheap on
  iPhone web.
- **Player hooks:** the player can feed, employ, supply or befriend residents; radiant quests come from unmet needs
  ("the baker has no flour"), and the player's trade can rescue or starve a town.

### 15.13 M3 phase A (lead, 2026-10-06): the culture contracts

Frozen for the M3 lanes (a lane that needs a change stubs locally and reports it):
- `rpg/culture/culture.h`: the data model of section 5.2 plus the owner's additions: `People` (human, half-breed, elf;
  every culture has a `peopleMix`), 12 archetypes (the 10 of 5.3 plus the elven **Sylvan** and **Starspire**),
  `ArmsStyle` with silhouette dials and the culture's own `Alloy` recipes on top of the universal `Tier`s (leather,
  bronze, iron, steel; owner 15.11; crafting reads them in M6), `TownStyle`, `PropStyle`, `Heraldry`, `MusicStyle`
  (`engine/music_style.h`), and the `cult::Atlas` (one per `EndlessSource`, memoised, order independent). Culture ids:
  a family per 2048-tile cell, a dialect per kingdom (the family of the cell holding its seat). Places in a kingdom
  take its dialect; the wildlands take their cell's family.
- `rpg/culture/style.h`: `ArchStyle` grows window, door, foundation, ornament bits, eave, wall height, furniture,
  accents and a `variant` byte for facade variety; new roofs (Mansard, Onion, Stepped, Sweep), roof materials and wall
  materials (including the elven Living wood and Leaf roofs); `CityWall` and `Fence` styles. Defaults = the M2 look.
- `Bldg::arch` / `Bldg::styled`: the settlement generator stores each building's culture style
  (`cult::buildingArch`); `bldgArch()` and the view use it. `Site::culture`, `Kingdom::culture`,
  `SitePlan::culture`, `KingdomPlan::culture` and `heraldry`, `SettlementCtx::culture`, `World::cultureOf()`.
- `art::HumanLook` gains people, cut, headwear, pattern, face paint, jewellery and the culture arms forms (helm, body,
  shield, blade, ornament, pauldron, skirt, crest); 0 = the M2 pixels. `rpg/art/art_culture.h`: banners and shield
  arms from `Heraldry`, glyphs, culture-styled props, and incremental building paints (`BuildingJob`, for the 41 ms
  palace paint). `Map::wall` = 1 + `CityWall` (the view puts it in the wall key at `WALL_STYLE_SHIFT`).
- `Audio::setMusic(Music, const MusicStyle*)`: the view passes the culture of the place (settlement, else land).
- SAVE_VER 7 (the appearance block: people, homeland, personal heraldry; fixture `tests/fixtures/save_v7.bin`);
  ENDLESS_GEN_VER 10 (culture names and arms). `Game::makeLook` moved to `rpg/sim/looks.cpp`.
- Tests and scripts: `rpg_test --cultures [N] --seeds A..B [--report]` (5.4 thresholds; CI runs it with `--report`
  until the engine meets them), script commands `gotoculture <archetype|other> [type] [n]`, `culture`,
  `expect culture <archetype>`.

### 15.14 M3b "Builders & Societies": two generators that partner with the culture engine (owner, 2026-10-06)

M3 review showed the root problem: the culture engine decides how a people *looks*, but each building type still has its
own hand-written painter, and any type nobody re-wired (palace, keep, gatehouse) stayed European. The owner approved two
new generators. They sit between the culture engine and the settlement and wild generators, and **everything built in the
world goes through them**:

- **Society generator** (per culture, deterministic, data only): government (monarchy, khanate with a moving court,
  elven high council, theocracy, merchant republic, clan elders...), classes and institutions (nobles, guilds, priesthood,
  warrior lodges...), where people gather (tavern, mead hall, bathhouse, tea house...), law and crime, inheritance,
  military organisation, trade attitude. It outputs **which buildings and spaces a settlement of each size must have** and
  what the seat of power is: a khan's capital centres on a great tent court, a theocracy's on a temple complex, an elven
  capital on a council spire or tree palace, a merchant republic's on a guildhall and exchange. M4 (kingdoms, diplomacy)
  and M5 (needs-driven citizens, 15.12) read this data.
- **Builder generator** (the construction grammar): assembles every building, wall, gate, tower, bridge, road, paving,
  fence and monument from parts chosen by culture x purpose x wealth: footprint and massing (rectangle, round, L,
  courtyard, tower, compound), storeys, roof kind (gable, hipped, dome, flat terrace, sod, tent canvas, spire, pagoda),
  walls and materials, doors, windows, ornament, palette and signage. Each part is drawn properly in the game's high 3/4
  top-down view with top-left light (domes with volume, flat roofs with parapets, no wall texture on roofs). Wealth sets
  size and finery, so a poor house and a noble manor differ within one culture. Interiors derive from the same plan (a
  round yurt has a round interior). Fortifications (walls, gatehouses, towers, palisades, earthworks) and roads (paving
  type, width, kerbs, bridges) come from the same culture parts. Every building is assembled, never copied, so no two
  neighbours are clones, and every new culture or building type inherits all of this.
- Acceptance: no building kind may bypass the builder (a test enumerates every building/prop type); a repetition audit
  over capitals of every culture; per-culture screenshot galleries judged against the owner's quality bar; performance on
  iPhone web (incremental paints).
