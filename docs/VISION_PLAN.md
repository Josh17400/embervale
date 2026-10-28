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
  - **Ruled 2026-10-28 (owner; 15.37): two ladders of metal words.** Each people names its own:
    - leather or bronze in band 1, iron in 2 and steel in 3;
    - from band 4, its everyday alloy, then its warriors' alloy in 5, then its masterwork in 6-7.
    Gilded, Obsidian, Emberforged and Starmetal are the old makers' metals: relics of old places, the same in every land,
    at bands 4, 5, 6 and 7. So gilded is band 4, not band 3, and "Jade" names a people, not a band. A relic's card reads
    "OLD MAKERS' WORK", and "Heartland" leaves the UI, because a heartland-born hero's people have alloys of their own.

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

*(Ruled 2026-10-28, 15.37: on the drawn figure a Legendary keeps its own metal's colour, outline and helmet, and its glow
is T6's finish (lit, breathing runes, motes and the edge spark). Its gold stays on the name, the frame, the icon border and
the light on the ground, never as a tint on the metal.)*

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
   *(Ruled 2026-10-28, 15.37 Q7; planned for the armour rules update after P2c):*
   - Shelves and smiths' commissions also stop at a people's warriors' alloy, with two exceptions:
     - a capital smith of that people who trusts the hero with his patterns may forge the masterwork on commission from
       band 6;
     - hidden-kingdom smiths follow the band rule.
   - The masterwork is otherwise earned: from band-6 drops or the hero's own forge.
   - The focus vendor stays as built: it sells its tradition's focus as a Rare piece at the hero's level + 2
     (magic_learn.cpp). So no Legendary is ever sold, and the exception above is retired.
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
    *(Ruled 2026-10-28, 15.26: a companion's own kit is made once, when they first join, and stays theirs while they
    live, and what you give them stays yours. They wear a gift only if it is better than what they wear. If they die,
    everything they carry drops where they fell. If they leave and come back, they come back as themselves, with their
    bond, their kit and their history: "they arent new".)*
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
     *(Ruled 2026-10-28, 15.26: their belongings drop where they fell, for the hero to gather; the grave holds none.)*
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
  - **Ruled 2026-10-28 (owner; 15.37), planned for the armour rules update after P2c DONE:**
    - *Great gear is earned.* Shelves and commissions stop at a people's warriors' alloy (7.5 rule 4). The masterwork
      comes from band-6 drops, the hero's forge, a trusting capital smith or a hidden-kingdom smith.
    - *Luck beats the gate a little, never a lot* (option b; 15.27).
    - *War pays a soldier's wage, not a fortune.* At a siege where the hero holds a War quest:
      - enemy soldiers drop a piece 10 % of the time, and captains 50 % (Uncommon or better);
      - the cap is 3 a siege, kept in the War quest's saved flags so a reload or a fresh wave can't reset it;
      - each piece rolls like a body at the siege's danger, with the hero's level as the cap;
      - joining by a service order alone pays none.
    - *No faucets:*
      - summons, raised thralls and decoys drop nothing;
      - undead carry old makers' metals only inside ruins and shrines;
      - a commission is seeded by the smith and the restock period, never the hero's purse;
      - a companion's kit never reaches the hero's pack while they live; at their death it drops once, as ordinary
        gear capped like any find, and a companion who leaves and comes back brings the same kit, never a second one
        (15.26).
    - *Measured:* the balance sim gains `rpg_test --ladder-pace`, which P2c builds on today's rules. It runs beside
      `--metrics` and `--power-curve` and reports the band, metal, look tier and rarity each pacing stage rolls, plus the
      relic share of an assumed hour.
- **Roadmap placement:**
  - the quest engine and story quests land with M4 (kingdoms give stories their actors);
  - campaigns arrive with M4/M12, the first campaign ships as soon as the engine exists;
  - the economy pass lands with M6 (items), and its balance sim is a CI gate from then on.
- **Story inspiration (owner):** draw on the whole world's storytelling (scripture and ancient myth, folk and fairy tales, epics,
  classic and modern fantasy such as C.S. Lewis, J.R.R. Tolkien, Sarah J. Maas and John Gwynne; owner 2026-10-09) to build a large library of **story archetypes and plot structures**.
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
- **Ruled 2026-10-28 (owner; 15.37): where you are decides the metal, luck decides the finish.** These are planned rules
  for the armour rules update after P2c DONE, except the crafted looks, which are look only and ship in P2c.
  - **Alloys in order.** From band 4 each people's alloys come in rank:
    - the everyday alloy at band 4, the warriors' alloy at 5, and the masterwork at 6-7;
    - a two-alloy people keeps its warriors' alloy from band 5;
    - an alloy is never weaker than its band's step;
    - crafting caps follow the rank: everyday band 4, warriors' band 5, masterwork band 6.
  - **The old makers' metals.** GILDED (band 4), OBSIDIAN (5), EMBERFORGED (6) and STARMETAL (7) are relics of old places,
    the same in every land.
    - From band 4, about half the metal gear in old places is a relic: ruin and shrine hoards and caches, caches at
      wonders, ruins and shrines, and undead and bosses inside ruins and shrines. All of a world boss's and a dragon
      lair's hoard is relics.
    - The target is about 1 armour find in 8. `--ladder-pace` fails outside 8-20 % of metal armour drops in its assumed
      hour, and the "half" is the knob.
    - A relic carries no people's id. It wears its hero style (PARADE, KNAPPED, FURNACE, STARFALL), and its card reads
      "OLD MAKERS' WORK: OBSIDIAN (2 OF 4)".
    - A relic is exactly as strong as a floored alloy of its band, so choosing one is taste, not power.
  - **Crafted looks** (P2c, look only). A crafted alloy draws T4 or more. A masterwork draws T5 only from band 4, so in an
    old save a masterwork forged in bands 1-3 drops one look step.
  - **The metal explained.** The card's maker row names the rank ("HIGHLAND WORK: GWESTEEL, WARRIORS' METAL (2 OF 3)"),
    and the HUD names the piece's own metal.
  - Still later, and not ruled now: leathers above band 1, alloy move quirks (1.4), per-material weapon shapes and a
    third alloy for two-alloy peoples.

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

### 15.15 M3b phase A (lead, 2026-10-06): the builder and society contracts

- `rpg/culture/society.h` (`cult::Society`, `societyOf`, `requiredBuildings`, `requiredSpaces`, `seatPurpose`,
  `seatForm`): government, seat of power, classes, institutions, gathering places, justice, inheritance, military, trade
  attitude, titles; per settlement tier (`cult::SettleTier`) the buildings and spaces it must have. Phase A gives every
  archetype its priors (fjordfolk jarldom and great hall, steppe khanate and tent court, sun-temple theocracy and temple
  complex, river merchant republic and guildhall + exchange, elven high councils with the council spire / tree palace,
  marsh elders' stilt hall...), bent by the culture's values.
- New purposes (`art::Building`, appended): Guildhall, Exchange, MeadHall, Bathhouse, TeaHouse, Lodge, CouncilHall (names,
  storeys, `artBase` stand-ins until the builder gives them their own massing).
- `rpg/build/blueprint.h` (`bld::Request`, `bld::Blueprint`, `bld::design`, `bld::validate`): forms (rect, round, L,
  courtyard, compound, tower, long, stepped, tent), volumes (shape, role, box in px, base height, wall height, storeys,
  roof kind and material, wall material, pitch, eave, windows, door, ornament, finery, tints), signage, yard and the
  interior shape (`InteriorShape`: floorplan, L corner, court size, floors, furniture, culture). `rpg/build/parts.h`:
  fortification parts (`FortParts`: wall, gate form, tower form, coping), road parts (paving, bond, width, kerbs, bridge)
  and monument parts. `rpg/build/registry.h`: every `art::Prop` classified (`bld::kindOfProp`). `rpg/build/*.cpp` build in
  the `rpg_culture` library (precise floating point; integer maths only).
- `Bldg::wealth / form / civic / seat`; `bldgRequest(b)` and `bldgBlueprint(b)` (`rpg/sim/world.h`).
- The art API paints blueprints only: `art::buildingSprite(const bld::Blueprint&, BuildingInfo*)`,
  `art::beginBuilding(const bld::Blueprint&)`, `art::buildingHeight(const bld::Blueprint&)`; `BuildingInfo::planKey`.
  Phase A paints them through the M3 type painters (pixels unchanged). The view, the galleries and the tools all go
  through `bld::design`.
- The culture furniture moved to `rpg/art/art_culture_furniture.cpp` (interiors), the street furniture stays in
  `art_culture_props.cpp`.
- Tests: `rpg_test --builder [--strict] [--society-strict]` (registry, design matrix, societies, placed blueprints; the
  capital repetition audit and the society's requirements placed are reported until the lanes gate them; in CI);
  `builder_gallery --check [--props-strict]` (every building painted from its blueprint, planKey, the seats' incremental
  paints, culture looks of every built prop) and `builder_gallery <dir> [--culture NAME] [--wealth]` (per-culture
  galleries). SAVE_VER 8 (fixture `tests/fixtures/save_v8.bin`), ENDLESS_GEN_VER 11.
- **M3b lanes (integrated 2026-10-06):**
  - BUILDER: `bld::design` is the real grammar (each culture's monumental style; seats built from the culture's own
    temple / guildhall / exchange; interior shape Round, L, Courtyard, Long, Cross). Blueprint additions: `Platform` and
    `Tree` volume roles, a `Face` per volume (shop front, forge bay, colonnade, arcade, iwan, gate...), detail flags
    (stilts, crenels, banners, guy ropes, steam, bell, stockade points...), dormers, gable outlines (stepped, bell,
    clipped), sign icons, `Blueprint::seat` / `culture`; `art::RoofShape::Tent` and `Spire`; `art::riseBudgetTiles`
    mirrors the generator's clearance. `validate` refuses a round body without a round interior, a chimney without a
    hearth and a tent of more than one storey. The painter draws only the blueprint's volumes.
  - TOWNS: `services()` builds what `requiredBuildings` asks (3528 of 3528 placed over `--towns --seeds 1..10`); seat
    compounds per `cult::Seat`; every capital also keeps a lord's Keep (no seat flag) for the main quest; storeys come
    from `bldgBlueprint(b).facts.storeys`; the region planner picks neighbouring settlements' looks together
    (`SitePlan::kind` = 1 + look; town audit worst group 2 over seeds 1..30).
  - FORTS: `FortParts` per culture and tier (11 gate forms, 7 tower forms, capitals' walls 3 px taller), paving bonds
    (sun-temple crazy paving), bridges by culture, per-tile standing-stone variants, street furniture from culture
    parts; wall keys carry a look slot in bits 29-31; building shadows cast per blueprint volume.
  - INTERIORS: floor plans read the blueprint (round, L, courtyard, long, cross), 14 culture inn plans, plans for the
    seven new purposes and every `cult::Seat`; a plan is refused unless every floor tile is reachable. A hall seat
    (great hall, tent court, stilt hall, council spire, tree palace) holds council in the hall itself: no separate
    council chamber is required there (castle and court palace keep the M1 contract).
  - FIXER (owner rule, 2026-10-06: "no door on an open building; pillars mean an open front, you just walk on in"):
    `bld::openFront` / `bld::openPillars` (blueprint.h) are the one contract the painter and the walking share. An
    entrance volume with a colonnade, arcade, veranda or iwan face (or such a portico before the door column) paints
    NO door anywhere; its pillars stand at the face's ends and on the 16 px tile boundaries, so every front-row tile with
    a walk-in bay (>= `OPEN_MIN_BAY` 12 px clear) is an entry: `Map::rebuildSolid` opens it, `bldgEntryAt` enters by
    it, `Game::pillarHit` blocks the body on the pillars alone. Open galleries beside a closed door are walked into the
    same way. The ground floor opens onto the porch in matching bays (`Map::exits`, `Map::isExit`; the interior's front
    wall shows an open threshold with the pillars' sections, `Piece::Door` a = 1); you arrive in the bay matching the
    one you walked in by and leave by it. Raised fronts (stilts, a plinth over 6 px) keep the ladder / steps at the door
    column. A door never stands behind another volume's pillars. Checks: `rpg_test --builder` (every design's bays,
    every placed open front walked in and out of its interior), `builder_gallery --check` (no door painted on an open
    entrance, painted pillars == walked pillars, no door behind a pillar), `tools/scripts/m3b_open_fronts.txt` (side bays
    of real capitals' colonnades). Also: round rooms' back arc on the true ellipse (`Piece::RoundBack`), seat courts'
    formal water kerbed in cut stone (`Map::POOL_MARK`, terrain.cpp `poolPixel`), road and river-footbridge decks laid
    straight across water (chunkgen drawRoad; `rpg_test --endless` fails a deck that jogs), elven round roofs a slender
    swept cone, L wings' roofs meeting the body's in a valley.
  - FIXER round 2 (2026-10-06): the roofs seen under a higher eave are painted (art_building.cpp renderColumns
    `underEave`: the stave church's upper stage, wing and tower eaves over a body's roof no longer show the ground
    through; `builder_gallery --check` fails any see-through hole onto a volume's footprint). An open front's bays read
    as the way in: each walk-in bay (`openWalkable`, the walking's own answer) is a dark doorway into the hall, a bay
    part nobody can walk through (a colonnade's narrow end bays) is closed by a balustrade, and a platform's steps run
    the width of the open bays (no door-sized step at the centre). The stave church's gallery is tall enough to show
    its arches under its eave; the starspire tea house is its own (white ashlar, slate hip, onion lantern; no jade
    pagoda or platform); a round room's back arc leaves the dark beyond it clear (no stepped panel). Scripts:
    `enterseat [floor]` enters the seat of power here; `enter <type>` and `gate` only take buildings / gates within
    160 tiles (and `gate` forgets a remembered gate once it is far); `expect bay` holds the player to floor 0 and the
    row inside the threshold; `m3b_open_fronts.txt` walks in one tile from the bay with a short push (passes seeds 1, 3,
    5, 7, 9, 11).

### 15.15 Walkable forests and many, many biomes (owner, 2026-10-06)

- **Forests must be walkable:** no two solid trees closer than one walkable tile (a person's width), checked by a BFS test;
  forests keep their dense look through overlapping crowns and walk-through undergrowth. (Fixed right away in a side pass.)
- **"Tons and tons of biomes"** (milestone M3c "Wildlands", after M3b). Today there are 10 (Ocean, Beach, Plains, Forest,
  Autumn, Taiga, Snow, Swamp, Desert, Mountain). Target 30 or more, each with its own ground, flora, fauna, weather,
  ambient sound, resources (feeds 15.11 ores, herbs, hides, timber) and blended ecotones. Candidates:
  - **Grasslands:** meadow / flower meadow, tall-grass prairie, steppe, savanna (acacias), heath and moor, chalk downs.
  - **Woods:** birch wood, old-growth giant forest, dark / dead forest, cherry or blossom grove, bamboo forest, jungle,
    mushroom forest (rare, magical), elven silverwood (culture-linked).
  - **Wet:** fen and reed marsh, peat bog, mangrove coast, flooded river forest, lake district.
  - **Dry:** badlands / mesa, salt flats, rocky scrubland, dunes vs. stony desert, oasis.
  - **Cold:** tundra, glacier and ice fields, alpine meadow, frozen lakes, taiga bog.
  - **Coast:** sea cliffs and fjords, coral and white-sand coast, shingle beaches, sea stacks.
  - **Wondrous (rare):** volcanic ash fields and lava rifts, crystal barrens, ancient petrified forest, standing-stone
    plains, blighted land around dragon lairs.
- Biomes come from climate (temperature, moisture, elevation, coast distance, geology), must stay deterministic, blend
  smoothly (ecotones), and settle cultures and village specialisations naturally (fishing on coasts, mining in badlands...).

### 15.16 M3c "Wildlands" phase A (lead, 2026-10-07): the biome contracts

Frozen for the M3c lanes (a lane that needs a change to a shared file reports it; the one owner named below may edit):
- `rpg/world/biomes.h` (WORLD lane is its only editor): `Ground` and `Biome` moved here from `rpg/sim/world.h`;
  **`Biome` is now the family** (the 10 kinds every older reader knows, unchanged) and **`Eco` is the biome proper**:
  46 kinds (open sea; sandy shore, shingle, coral strand, sea cliffs; meadows, flower meadows, tallgrass prairie, steppe,
  savanna, heath and moor, chalk downs, alpine meadows, standing-stone plains; forest, birch wood, old-growth forest,
  dark forest, blossom grove, bamboo, jungle, mushroom forest, silverwood, autumn woods; taiga, taiga bog, snowfields,
  tundra, glacier, frozen lakes; reed marsh, peat bog, mangrove coast, flooded forest, lake district; dune sea, stony
  desert, badlands, salt flats, scrubland, oasis; mountains; ash fields, crystal barrens, petrified forest, blighted
  land). `ecoInfo(e)`: name, script key, family (always the tile's Biome), base ground, map colour, flags (`EF_WOODED`,
  `EF_ROCKY`, wet, cold, hot, coast, open, rare, magic, hostile), habitability, the trades its villages lean to
  (`TR_*`), the weather profile (`Sky`), ambient bed (`Ambience`), music mood (`Mood`), and M6 resources (herbs,
  hides, timber, minerals, ore bias). `ecoOfFamily`, `ecoFromName`, `ecoHas`.
- Layers: `MacroSample::eco`, `ChunkData::eco / ecoNb`, `Map::eco / ecoNb` (`Map::ecoAt`, `ecoNbAt`), copied through
  the Active Window and the terrain bake snapshots. `Map::blend` keeps its meaning (bits 0-3 the neighbouring family);
  `ecoNb` is the neighbouring eco with the same weight (== eco where unblended). `gen::Stamp::eco()` sets eco and family
  together. `EndlessSource::ecoAt` (tile classifier) and `ecoFar` (far estimate from the raw macro fields).
- Classifier: `EndlessSource::Impl::ecoFor(family, e, t, m, ridge, level, x, y)` in `macro.cpp`, called by `tile()`,
  `biomeLite()` (the ecotone ring), `macroFar` / `ecoFar`. Phase A is a first cut inside each family (the families,
  and so every older behaviour, are unchanged); rare ecos come from slow noise fields with high thresholds. All 46
  appear over seeds 1..3 (`rpg_test --biomes --seeds 1..3 --strict`).
- Flora: 37 props appended to `art::Prop` (`AcaciaTree .. Petals`: 14 trees, 8 solid rocks / shrubs, 15 walk-through
  ground covers), painted in `rpg/art/art_flora.cpp` (stand-ins: recoloured classic sprites), drawn per tile as
  `art::floraVariant(p, v, eco)` (render.cpp cache key 0x5E). Shared predicates in `art_props.h`: `isTreeProp`,
  `isFloraProp`, `isWildSolidProp`, `isWildlandsFlora / Solid` (prop_traits, worldmap, town_market, game projectiles and
  `rpg_test --forest` use them). `bld::kindOfProp` classifies them, `propSolid` lets the ground covers be walked over.
  `peakLand` moved to `prop_traits.cpp` (0..15 lands; peak cache key widened).
- Wildlife: 7 monsters appended after the Dragon (Scorpion, Hyena, Lurker, Yeti, Wisp, EmberHound, Blightspawn; stats,
  names, plurals, factions, stand-in art). `rpg/world/wildlife.h`: `denOf` (the generator's dens, frozen table),
  `roamerOf` (Game's roaming wildlife), `huntTargets` (hunt quests), `caveTheme`.
- Audio: `Audio::setAmbient(kind, level)` and `Audio::setMood(mood)` (stored only in phase A).
- HUD: the location line names the eco (`SAVANNA`, `PEAT BOG`...).
- Scripts and tests: `gotoeco <biome> [n]`, `eco`, `expect eco <biome>`, `ecotour` (`rpg/view/script_eco.cpp`);
  `rpg_test --biomes [--seeds] [--radius] [--list] [--strict]` (census, layer consistency, determinism); CI runs it with
  `--strict` and `rpg_test --forest --seeds 1..10`. `tools/preview/flora_gallery <dir> [eco]`.
  `tools/scripts/m3c_lead_biomes.txt`.
- SAVE_VER 9 (fixture `tests/fixtures/save_v9.bin`, layout of 8), ENDLESS_GEN_VER 12 (goldens re-recorded).
- (Phase B, recorded by the integrator) WORLD: `SettlementCtx` gained `eco` and `worldSeed`; culture centres pack
  eco + 1 into the top byte of `ClimateSample.moist`; scripts `gotoecoborder <a> <b>`, `gotospecialty <trade> [n]`
  (the family-level `gotoecotone` stays in script_view.cpp). LAND: `floragrid`, `gotoecoedge <a> <b> [n]`
  (script_land.cpp), `EMB_BAKEBENCH=1` / `EMB_BAKE_CLASSIC=1` bake timing hooks. LIFE: `weather <kind>|auto`,
  `Audio::setDaylight`, `heavyWindup()`, first-visit biome banners remembered in `Game::marks`. CARRY:
  `Game::preparedInterior(int& bldg)` (the door ahead's ground floor, made ahead; map key 100000 + bldg),
  `Game::beginTravelTo(gx, gy)`, `Game::frameGapWorstMs`, scripts `perfmark`, `pickculture`, `travelpicked`.
  Integration: `View::prepareInterior` (render_deco.cpp) queues the prepared interior's terrain chunks under its map id
  (prefetch keeps them) and paints its deco / interior prop pieces into cachedTex ~1.5 ms a frame; `View::mapIdFor`;
  `Game::debugFell(id, byPlayer)`.

### 15.17 M4 "Banners" phase A (lead, 2026-10-07): the realm, war and story contracts

Section 15 binds M4 over section 4 where they differ: diplomacy is **slow and realistic** (15.6.3: famine -> a broken
food deal -> border tension -> skirmishes -> war; wars rare, with causes, foreshadowed by rumours, prices, refugees and
troop movements), not the "lively" default of 4.3. Phase A contracts (a lane may ADD to the header it owns, never rename,
remove or change the meaning of what is listed here):
- `rpg/sim/realm.h` (REALM lane): `realm::Realm` (Game::realm, saved): kingdoms (`KingdomState`: ruler with society
  title, pop / wealth / military / stability / exhaustion / food, culture values, tier, rep, settlements), relations
  (`Rel` + the `Tension` ladder + food deals), wars (with `WarCause` and a name), sieges (atk / def, the camp's tile, the
  player's side), per-settlement `SettlementState` (owner vs genesis home, damage, garrison, food, mood, flags
  `SS_BESIEGED / BURNED / ABANDONED / OCCUPIED / REFUGEES / FAMINE / UNREST / RUINED / FRONTIER / REBUILDING / GARRISON`),
  the event log (`WorldEvent`, `EvType` x24, heard flags), `RuinRecord` (15.3: who built it, when and how it fell, its last
  lord, clues), `Renown` (15.6.4 hooks: fame, a lordship, rank, sworn kingdom), `landOwner` (borders after conquests),
  `focus` / `advanceTo` (VISION_PLAN 4.4 / 4.8), and the forcing calls every lane tests with (`forceWar / Siege / Owner
  / Burn / Famine / Event`). Phase A's stub: genesis of the 3x3 kingdom cells round the player (from the cheap
  `EndlessSource::settlementsIn` lattice: no region plans), forcing, occupation / siege bookkeeping, the save block.
  The realm never changes generation (overlays only).
- `rpg/world/source.h`: `SettlementNode`, `EndlessSource::settlementsIn(x0, y0, x1, y1, withNames)`, `kingdomOfCell`,
  `kcellOf`, `kingdomAt`.
- `rpg/sim/world.h`: `Site::kingdom` is the CURRENT owner, `Site::homeKingdom` the genesis one; `World::setSiteOwner`
  (banners follow, also on buildings streamed in later), `World::addKingdomRecord` (rebel / warlord realms with no plan);
  `Bldg::charred` (0 whole, 1 scorched, 2 burned out, 3 rebuilding) -> `art::BuildingFacts::charred` (0 keeps every key
  and pixel); roles `Herald, Soldier, Captain, Refugee`; `Faction::Army` (soldiers; kingdom-vs-kingdom hostility is
  `Game::warHostile` from `Actor::realm` and the realm's wars).
- `rpg/sim/game.h`: `Game::realm`, `Game::story`, `Game::war` (WarState, rpg/sim/war.h, WARDS lane, never saved);
  `Ev::News` / `Ev::Border`; `QType::Story` / `QType::War` (+ `QF_WAR_ATTACK`); dialogue ranges `DLG_STORY` /
  `DLG_WAR`; hooks called from the core loop and defined in each lane's own file: `realmStep` / `realmSync`
  (rpg/sim/realm_game.cpp), `warStep / warTalk / warChoose / warKill / warHostile / warPropKingdom`
  (rpg/sim/war_game.cpp), `storyStep / storyTalk / storyChoose / storyKill / storyEntered / storyUseProp`
  (rpg/story/story_game.cpp). `Game::prepInteriorTick` and friends moved to rpg/sim/prep_interior.cpp.
- `rpg/story/story.h` (STORY lane, new library folder rpg/story built into rpg_sim): `story::Engine` (running
  `Instance`s with cast `Binding`s and variables, the "Lost History" `LoreEntry`s, its save block) and
  `story::newsLine(game, event, voiceCulture)`. Running stories mirror into `Game::quests` as `QType::Story`.
- Props (art_props.h, frozen footprints `m4Footprint`): war `WarTent, CommandTent, Catapult, Palisade, RefugeeTent,
  Rubble, Ash, Scaffold, Barricade` (rpg/art/art_war.cpp, `art::warPropSprite(p, field, trim)`), lore `NoticeBoard,
  Inscription, ToppledStatue, Mural, NamedGrave, LostJournal` (rpg/art/art_lore.cpp); stand-in pixels.
- Saves: SAVE_VER 10 (the realm and story blocks after marks, each length-prefixed with its own version byte; fixture
  `tests/fixtures/save_v10.bin` carries a forced famine and a reputation); ENDLESS_GEN_VER 13 (goldens unchanged by phase
  A; the VIEW lane's land fixes re-record them). Damaged saves can no longer stall the loader on garbage ids beyond
  World's End (`World::ensureSite / ensureDen`).
- Tests and scripts: `rpg_test --realm [--seeds]`; script commands `realm siege|take|burn|famine [n] | war | days N`,
  `expect realm <state> [n]` (rpg/view/script_realm.cpp); `tools/scripts/m4_lead_realm.txt`.

### 15.18 M5 "Hearth and Hall" phase A (lead, 2026-10-07): the life contracts

Section 15.12 (needs-driven citizens) and 15.2 (hunger and sleep as buffs) bind M5 over section 10 where they differ.
Phase A contracts (a lane may ADD to the header it is named the editor of, never rename, remove or change the meaning of
what is listed here):
- `rpg/sim/life.h` (CITIZENS lane is its only editor): `life::Job` (24 jobs: the 10.1 list plus server, servant,
  lamplighter, herder, labourer), `Act` (sleep, eat, work, wander, socialise, tavern, pray, patrol, play, perform, shop,
  home, beg, bathe, light lamps, brawl, emigrate), `Place` (home, work, gathering, gathering2, plaza, temple, field,
  gate, market, street, away), `Block` / `jobTemplate(job, shift)` (the 10.2 tables; guards' day and night shifts),
  `Need` x5 (hunger, rest, social, faith, money; 0..100, 100 satisfied), traits `TR_*`, resident flags `RF_*`, mood flags
  `MF_*` (content, festival, hungry, famine, war-torn, grief, brawls, emigrating, shuttered, raided), `Resident` (job,
  age, traits, home / work as building offsets from `Site::bldgFirst`, household, spouse, parent, the generator spawn it
  is: `keyBldg` / `keySlot`, name, needs, coin, the hour's act / place / building, mood, flags), `Tie` (friendships;
  `PLAYER_TIE`), `Census` (residents, ties, the gathering / temple / market / seat / inn offsets, stock and prices per
  `ew::Good`, mood and flags, hourly occupancy per building, the festival day), `npcId`, `Plan` and `Life::plan`,
  `PlayerNeeds` (Well Fed / Rested / Hungry / Weary clocks; `survival` stub), `lampLightHour` / `lampOutHour` /
  `lampLit` (the lamplighter's round by global tile), and `life::Life` (`census()` built on first use from the site's
  buildings, `tick()` called by the core loop, `residentDied`, the player's hooks `feed / employ / supply / befriend`,
  `friendsOf`, what the view reads: `mood`, `moodFlags`, `festival`, `occupants`; `resync`; the save block; stats).
  `buildCensus` (census.cpp) and `spawnResident` (which resident a generator spawn embodies).
- `rpg/sim/game.h` (TOWNSFOLK lane is its only editor): `Game::life`; `Actor::resident, posture, postureT, useX, useY,
  bubble, bubbleT, critter, critterVar, lifeBits, lifeT, lifeA, lifeB`; `QType::Supply` (radiant quests from unmet
  needs); `DLG_LIFE .. DLG_LIFE_END` (3000..4000); hooks called from the core loop and defined in each lane's own file:
  TOWNSFOLK (rpg/sim/life_game.cpp) `lifeStep, lifeFolk, lifeSpawnAllowed, lifeSpawned, lifeInterior, lifeTalk,
  lifeChoose, lifeKill, lifeUseProp, lifeAte, lifeSlept, lifeStaminaRegenMul, lifeHealthRegenMul, lifeXpMul` and the
  opaque `Game::LifeRuntime`; CITIZENS `raidStep` (rpg/sim/life_raids.cpp) and `lifeOffer` (rpg/sim/life_quests.cpp,
  asked first by hasOffer and makeOffer).
- `rpg/sim/realm.h`: `Realm::lifeReport(site, foodDelta, mood, day)` (rpg/sim/realm_life.cpp): the residents' day folded
  into `SettlementState::food / mood` (15.12 -> the 15.6.3 chain). Phase A never calls it.
- `rpg/art/art_life.h` (ART lane is its only editor; included by rpg/art.h): `art::Posture` (25: sit, sit and eat, sit
  and drink, eat, drink, cheer, hammer, hoe, sweep, chop, stir, carry, fish, sleep, wave, play, dance, lute, drum, flute,
  pray, beg, lamp, read; named Posture because the rig has an internal Pose), `PostureInfo` / `postureInfo`,
  `humanPostureSheet(look, posture)` (POSTURE_FRAMES x 3 facings), `Critter` (dog, cat, chicken, rooster, goat, pig,
  duck) / `critterSheet(kind, variant)`, `Bubble` (11 speech icons) / `bubbleSprite`, `festivalBunting`,
  `festivalLantern`. Phase A stand-ins (the standing sheet re-used, plain shaded animals).
- Audio: `Music::Tavern` (phase A plays the Town piece), `Sfx::Bark, Cluck, Meow, Cheer` (stand-in voices).
- Saves: SAVE_VER 11 (the life block after the story block, length-prefixed with its own version byte: the player's
  buff clocks, then per settlement whose census was built (or loaded and not rebuilt) its mood, flags, clock, festival
  day, stock, every resident's flags / grief day / needs / coin, and the player's friends); fixture
  `tests/fixtures/save_v11.bin` (regenerated from current code: the main quest's wording is fresh; carries Well Fed,
  Rested and a befriended villager). ENDLESS_GEN_VER 14 (goldens unchanged by phase A; a lane that changes generation
  re-records them).
- Tests and scripts: `rpg_test --life [--seeds] [--report]` (templates cover 24 h, census determinism and bounds, the
  inn's keeper keyed, asleep at 2:00 / working at 10:00, the hourly aggregate, the life block round trip, the
  120-resident plan budget; CI blocking); script commands `life [stats]`, `buff <state>`, `expect life residents |
  act | mood`, `expect buff` (rpg/view/script_life.cpp); `tools/scripts/m5_lead_life.txt`, `tools/scripts/town_day_night.txt`.

#### 15.18 addendum: M5 phase B/C (lanes + integration, 2026-10-08)
- Life block version 2 (still SAVE_VER 11): at most 48 settlement records; the 8 most recent keep every resident's needs,
  the rest sparse deltas. Fixture tests/fixtures/save_v11.bin also carries a death with grief and a famine.
- `Realm::lifeRaid` (add-only, CITIZENS): a failed far-away night defence costs prosperity/damage and may carry off a
  villager (QType::Missing rescue). `Realm::lifeReport` is now called once a day per simulated settlement.
- Generation: the steppe inn is a two-storey felt guest house (guest beds upstairs only); 18 of the 201 town goldens
  re-recorded; ENDLESS_GEN_VER stays 14.
- CI: `rpg_test --folk --seeds 1..5` added beside `--life`.
- Integration: children are drawn with a shortened body (rpg/view/render.cpp `childBody`: two torso rows and, unless
  seated, two leg rows taken out of each rig cell; the head keeps its size); animals say "E: PET"/"PET"; animal idle
  voices are the view's only (the sim sounds event barks).

### 15.19 M6 "Steel" phase A (lead, 2026-10-08): the arms, items, foes and forge contracts

Section 7 and 15.11 bind M6 (owner M6 notes: universal tiers leather -> bronze -> iron -> steel with culture alloys on
top; ores regional by geology and biome; each culture's arms and armour distinct; secrets learnable by trust / quests,
apprenticeship, ruins lore or theft; a thin crafting front end on the economy's own chains; the paper doll shows every
piece in its maker's style; the hero still starts in a shirt). Phase A contracts (a lane may ADD to the header it is
named the editor of, never rename, remove or change the meaning of what is listed here):
- `rpg/sim/items.h` (NUMBERS lane): `ItemKind::Material` (crafting stuff, stackable), `WeaponType::Spear` (random draws
  keep to the classic five so the random stream is unchanged), `Mat` (None, Leather, Bronze, Iron, Steel, Alloy, Cloth,
  Wood, Bone, Precious), `Affix` x18 + `affixInfo`, `ItemAffix`, `Unique` x33 (7.3's table) + `uniqueInfo`, `IF_*` flags,
  and the Item fields `ilvl, mat, alloy, culture, form, seed, affix[3], unique, flags`; `tier` is the band - 1 (0..6;
  STARMETAL added). writeItem / readItem carry them (SAVE_VER 12).
- `rpg/sim/gear.h` (NUMBERS lane; gear.cpp): bands (`bandOf / bandLo / bandHi`), level sync (`effLevel`, `statScale`),
  rarity (`rarityMult`, `affixSlots`, `rollRarity`, `DropSource`, `dropIlvl`), bases (`weaponBase`, `armourBase`,
  BOW / FOCUS), `mitigation` (70 % cap), `playerDamageMul`, the gap rules, enemy scaling by D, `killXp`, `xpForNext`,
  `attunementSlots`, POTION_COOLDOWN / POTION_CARRY, `merchantGold`, SHOP_BAND_SPREAD, DEATH_GOLD_PCT; generators
  `makeGear`, `rollDrop`, `gearName`, `bandWord`. Phase A: the formulas exactly as 7.x writes them; the generators stub
  the classic makers; NOTHING is wired into combat yet (the game plays as M5).
- `rpg/sim/craft.h` (NUMBERS lane, its forge half; craft.cpp, craft_game.cpp): `Stuff` x18 (ores in ew::Ore order,
  ingots, alloy ingots, hide, leather, timber, cloth, reagents) with `stuffInfo` (each trades as an ew::Good),
  `makeStuff`, `oresAt` (province geology + the eco's ore bias, richest first), `Recipe` (station = art::Building as
  ew::Recipe), `baseRecipes` (smelting copper / tin / iron / silver, bronze, steel, tanning, every piece in leather,
  bronze, iron, steel, bows, cloaks), `cultureRecipes` (the culture's patterns in the universal metals, its alloys and
  every piece in them; locked behind its secrets), `Secret` / `SecretKind` / `HOW_*`, `Knowledge` (skill, secrets per
  culture FAMILY, smiths' trust; the craft block), `canMake`, `make` (leaves emptied stacks at count 0 for the caller).
  `Game::craft`; hooks `craftTalk / craftChoose` (DLG_CRAFT 4500..5000) in rpg/sim/craft_game.cpp (no-ops).
- `rpg/sim/foes.h` (FOES lane; foes.cpp, foes_game.cpp, realm_beasts.cpp): `Rank`, elite `Affix` x12 + `affixInfo` /
  `auraColor`, `Variant` / `variantFor`, `NamedUnique` / `namedInRegion`, `WorldBoss` / `worldBossOf` / `worldBossAt`,
  `Phase` / `bossPhaseFor`, Game::marks tags 64..79. Actor fields (never saved): `rank, affixes, overlays, bodyTint,
  scalePct, bossPhase, affixT, affixT2, unique, pack, culture, dropD`. Hooks (no-ops in phase A): `foeSpawned(a, D)` (end
  of spawnMonster and the bandit branch of spawnHuman; must not add actors), `foeStep(dt)` (after raidStep),
  `foeHit(victim, dmg, attacker)` (damage(), after armour), `foeKilled(a, killer)` (kill(), before the loot; must not
  add actors: queue them for foeStep), `foeTalk / foeChoose` (DLG_FOES 4000..4500). Realm (add-only):
  `EvType::BeastRaid / BeastSlain`, `Realm::beastRaid / beastSlain` (realm_beasts.cpp), news lines in rumours.cpp.
- `rpg/sim/gear_look.h` (ARMS lane; gear_look.cpp): `appearanceLook` (moved from player.cpp), `WornGear`,
  `wearGear(look, worn, cultureOf)` (Game::recalcPlayer calls it; heartland pieces give the M5 pixels; culture pieces
  set the forms, ornament, plume, alloy tint and sheen, legendary glow), `gearIcon(item, maker)`.
- Art (rpg/art): `Monster::Harpy, Golem` (stand-ins: a recoloured bat and troll; stats, names, plurals, words in every
  table); `MonsterLook` + `MO_*` overlays (horns, spikes, crystal, moss, rime, ember, plates, eyes, bone mask) +
  `monsterSheetLook` (stand-in: the tint only); `Icon::Spear, Ingot` (stand-ins); `IconLook` + `itemIconLook` (stand-in:
  the classic icon); HumanLook `polearmForm, bowForm, armourTint, armourTint2, sheen, glow, glowColor` (in key() only when
  set: the M5 keys are unchanged; weapon 8 = the polearm); `AURA_FRAMES`, `auraH`, `auraSprite` (stand-in ring).
- View: `View::itemTex(g, item)` (every Item icon in the pack, shop, paper doll, pickups) and `View::monsterTex(actor)`
  (the variant sheet; render.cpp draws monsters with it).
- Saves: SAVE_VER 12 (the item layout above; the craft block after the life block, length-prefixed, own version byte);
  fixture `tests/fixtures/save_v12.bin` (a legendary culture spear with affixes, ore, the smith's skill, a secret, a
  smith's trust); ENDLESS_GEN_VER 15 (goldens unchanged by phase A; the FOES lane re-records them if dens or lairs change
  generation). `tests/fixtures/metrics_m5.txt`: the M5 `--metrics` baseline the first-hour +-10 % gate compares against.
- Tests and scripts: `rpg_test --gear [--seeds]` (CI): the 7.x formulas, the item layout, the crafting chain and
  secrets, regional ores, harpy / golem spawn. Script commands (rpg/view/script_gear.cpp): `gear6`, `kit6`, `stuff`,
  `elite`, `expect gearlook`; `tools/scripts/m6_lead_gear.txt`.
- **Ruled 2026-10-28 (owner; 15.37), planned for the armour rules update after P2c DONE.** These contracts gain the
  following. There is no SAVE_VER bump: 23 holds, and only free bits of bytes already saved are used. There is no new
  random draw beyond the commission seed and the spoils roll.
  - `items.h`: IF_KIT (16). IF_KEEPSAKE is not added: a death drops everything, so no grave keeps a keepsake (15.26).
  - `gear.h`:
    - `DropSource::relicPct`, runtime only and never saved;
    - the relic decision, which hashes draws the roll has already made;
    - the relic slot table: body 40, helm 25, shield 15, gloves 10, boots 10, never a cloak;
    - matFor by alloy rank, with the band-step strength floor and crafting caps by rank;
    - the "better" rule shared by the compare arrow and companions (15.26).
  - A companion's kit is made once, by makeGearC with their culture's data. Its item level is capped at the hero's
    level + 4, and its rarity rolls with the hero as the finder (15.26).
  - storyLoot passes the hook's people, and chests in town buildings take the town's maker.
  - For war spoils, spawnMan sets `Actor::dropD` and `culture` on siege soldiers, and the counter lives in two free bits of
    the War quest's saved flags.

### 15.20 M6b "Sagas": the story, mission and campaign generator (owner, 2026-10-08/09)

The owner wants thousands of unique, genuinely good side quests, missions and campaigns. M4 built the engine (the DSL in
`rpg/story/dsl.h`, the caster, the save block, `--story` path walking) but only 11 hand-written tales and one campaign
(`emptythrone`). After a few dozen hours the player would recognise every plot. M6b adds the **generator** on top of the
engine: stories are *composed* from a large archetype library, cast from the living world, and written in the culture's
voice, so no two playthroughs (and no two regions) tell the same tales.

**Inspiration (owner, extends 15.9):** real-world storytelling — the Bible and scripture, ancient myth (Greek, Norse,
Celtic, Mesopotamian, Persian, Indian, East Asian), folk and fairy tales, epics and Arthurian legend, and modern fantasy
such as **C.S. Lewis, J.R.R. Tolkien, Sarah J. Maas and John Gwynne**. **Rule (unchanged):** public-domain scripture, myth and folklore
may be retold closely; from modern works we take only structures, themes, motifs and feelings — never their names,
characters, places, invented terms or beat-for-beat plots.

#### 1. The generator: five layers
1. **Archetypes** (`rpg/story/arch/*.cpp`): plot skeletons as data. Each has roles (with constraints on what the caster
   may bind), 3–9 abstract beats (`seek`, `confront`, `reveal`, `choose`, `sacrifice`, `return`...), at least one moral
   choice with lasting consequences, 1–3 twist slots, tags (source tradition, themes: redemption, betrayal, burden,
   bargain, exile, judgement, mercy, pride, love, kinship, the return of the king...), tier (2 story, 3 campaign arc) and
   the world facts it needs (a war, a ruin, a famine, a fallen house, a sea...).
2. **Cast** (the M4 caster, extended): binds roles to real entities with motives drawn from the world: the M5 census
   (residents with jobs, households, friendships, needs, grief), M4 realm (rulers, wars, successions, ruins and their true
   records), M6 foes (named uniques, world bosses) and items (legendaries, culture alloys, smithing secrets as rewards).
3. **Complications and twists** (`rpg/story/twists.cpp`): a library of reusable turns (the giver lied; the monster was a
   cursed person; the heir is someone you already met; the reward is a trap; the rival wanted the same good thing; the
   prophecy meant someone else...) slotted where the archetype allows, with their consistency rules.
4. **Voice** (`rpg/story/voice.cpp`): phrase grammars per culture (M3 values and dialect), per speaker motive (fear,
   greed, grief, pride, devotion, love, duty) and per beat, plus proverbs, oaths and scripture-like sayings of each
   people's faith. Dialogue names real places, people and history. No generic filler ("PLEASE HELP ME, ADVENTURER").
5. **Composer** (`rpg/story/compose.cpp`): archetype + cast + twists + voice -> a DSL script, validated by the existing
   validator, then run by the existing engine. Deterministic from (world seed, hook entity, day bucket). A **repetition
   guard** remembers the archetypes, twists and phrase families the player has seen and steers away from them in the
   same region and recent days.

#### 2. First archetype batch (target: 70 at ship, 150+ by M12)
Scripture and ancient myth (close retellings allowed): the prodigal's return; brothers' betrayal and the pit (Joseph);
the giant and the shepherd (David); the loyal stranger who will not leave (Ruth); the tower of pride (Babel); the flood
and the ark-builder mocked; the exodus out of bondage; the prophet who runs from his calling (Jonah); the wise judgement
over a disputed child (Solomon); the strongman's secret betrayed (Samson); the plague as judgement and the intercessor;
the hidden queen who must speak (Esther); the descent to the underworld for a loved one (Orpheus); the labours set to
destroy the hero (Heracles); the long voyage home (Odyssey); the fire stolen from the gods (Prometheus); the doomed
twilight battle foreseen (Ragnarök); the trickster bound (Loki); the sword in the stone and the true king; the grail
quest and the unworthy knight; the green knight's beheading game.
Folk and fairy tale: the cursed gift; the deal with the fae and its hidden price; the changeling; the true name
that binds; the three tasks for a bride or groom; the youngest sibling who succeeds; the enchanted sleep; the beast who
was a man; the piper unpaid; the wolf at the door; the woodcutter's honest axe; the stolen shadow.
Lewis-like (themes only): the child or stranger through a hidden door into another realm; the guardian's sacrificial
death and return; a winter that never ends under a false ruler; the traitor redeemed by mercy; the last battle and the
door at the end.
Tolkien-like (themes only): a fellowship carrying a burden it must not use; the corrupting artifact; the line of a
fallen king returning; the small hero on the long road; the ancient evil stirring in a ruin; the oath-breakers' dead;
the old forest that remembers.
Maas-like (themes only): the court intrigue among rival fae houses; the bargain marked on the skin; enemies to allies to
a bond; the hidden queen and the price of the crown; a curse that only love or sacrifice breaks; the trial of the
under-mountain.
John Gwynne-like (themes only; owner 2026-10-09): a prophesied war where both sides believe they serve the light and the
player learns too late which champion is false; "truth and courage" — the oath-sworn warband and the shield wall that
holds; the bond between a hero and a great beast (a wolfhound or a bear raised from a pup); the ancient feud of a
dying elder race (giants) whose ruins and grudges still shape the kingdoms; the mentor who betrays or the betrayer who
was right; a winged host and a fallen host fighting their old war through mortal kingdoms; the villain's own chapter
(the player sees the enemy's reasons); the hunted band on the run through the wilds, picking up the broken and the
outcast; vengeance that costs the avenger everything.
Plus world-native ones (from the sims): the famine town and the hoarding lord; the deserter's choice; the refugee
family; the guild's betrayal; the alloy secret stolen; the beast that took the village's children.

#### 3. Campaigns (tier 3; 6 at ship)
Chains of 10–30 quests built from 4–8 archetype arcs with recurring characters, factions and a world-changing ending
(who rules, which town survives, which secret is lost or saved): the succession crisis (exists: `emptythrone`); the
burden across kingdoms (fellowship + corrupting artifact); the plague cult; the hidden heir; the fae-court intrigue;
the rebellion against a tyrant; the dragon cult (ties to Ashfang); the awakening of an old magic tradition (seeds M9).
Campaigns are stumbled into anywhere via rumours, heralds, ruins and realm events, and use the realm effects
(`realm war`, `realm succession`, `realm famine`, `realm peace`).

#### 4. Quality bar and tests
- Every generated script passes the validator; `rpg_test --sagas N --seeds A..B` composes N stories per seed (10,000 in
  CI), walks every path to an ending headlessly, and checks: no unfilled placeholder, no broken grammar markers, every
  bound entity exists and is reachable, rewards within the M6 bands, no two stories within 30 km share archetype+twist.
- Uniqueness metrics: distinct (archetype, twist, cast-shape) triples per 1,000 stories; phrase-family reuse rate.
- A writing-quality review lens reads 50 random stories per round (a reviewer agent scores specificity, voice and
  coherence; fails the round below the bar).
- Save block for generated stories (they are regenerated from their seed; only progress and choices are saved).

#### 5. Lanes (rpg/story/* only; no conflict with M6 code)
- **A, archetypes:** the archetype data format, the first 70 archetypes, the twists library.
- **B, composer and cast:** composer, caster extensions (census, foes, items), repetition guard, save block, tests.
- **C, voice:** culture and motive phrase grammars, proverbs and sayings, dialogue quality pass.
- **D, campaigns:** the campaign chainer and 5 new campaigns, realm consequences, rumour hooks.

### 15.21 M6b "Sagas" phase A (lead, 2026-10-09): the generator contracts

15.20 binds M6b. Phase A contracts (a lane may ADD to the files it is named the editor of, never rename, remove or change
the meaning of what is listed here):
- `rpg/story/saga.h` (frozen): the five layers. `Archetype` (id, name, `Source` x9, `Theme` bits, `Need` bits, tier 2
  story / 3 campaign arc, the giver `Motive`s that fit, the `TwistFamily` bits its slots take, the template body),
  `Twist` (id, name, one family, needs, `roles` it requires as "name:kind/kind", `excludes`, body), the voice
  (`VoiceCtx` voice family 0..7 x `Motive` x seed -> `Phrase` text <= 60 chars + phrase family; `voiceKeys`), the
  composer (`Spec` -> spec id "saga1~arch~t1~t2~t3~vN~mN~seedhex", campaigns "saga1~@id~arc.arc~~~v~m~seed";
  `expand`, `slotsOf`, `slotFamilies`, `twistFits` / `twistFitsSlot`, `compose`, `composeCampaign`, `script(id)` (the
  cache), `trimCache`), choosing from the world (`Hook`, `pick`, `pickCampaign`, `offerFor`, `offerForPlace`), the
  repetition `Guard` (Seen records, phrase families; GUARD_RADIUS 1024 tiles = "30 km", GUARD_DAYS 60), `report`,
  `CampaignPlan` / `ArcSlot`. SAGA_GEN_VER 1 is inside every spec id: a running saga of another version is dropped
  gracefully on load.
- THE TEMPLATE MARKUP (saga.h header): DSL text from `title` on, plus `[[A|B]]`, `[[name=A|B]]` / `[[=name]]`,
  `<<key>>` / `<<key:motive>>`, `%local` names (twists, arcs), `@t1..@t3` slot entries declared by
  `slot tN -> <stage> [family words]`, `@out` (twists), `@next` (arcs), `?flag` / `!flag` lines (t1..t3, tw_<id>,
  m_<motive>, v<N>, arc_<id>). Role conventions: every template declares `giver` (first) and `home`.
- DSL additions (`rpg/story/dsl.h`; COMPOSER lane is its only editor from now on): role kinds `resident
  any|kin|friend|mourner|needy|young|old [of <role>] [at <site>]` (M5 census), `beast near|far` (M6 named unique),
  `boss` (M6 world boss); effects `reward small|fair|rich|great` (inside the M6 bands), `secret <site>` (craft
  HOW_QUEST), `befriend <resident>`; placeholder fields `.job .kin` (resident), `.kind` (beast / boss), `.god .people`
  (anyone or anywhere with a culture), `.father .husband .sir .boy` (gendered words); `dsl::link`, `relationWord`,
  `rewardWord`. Phase A stand-ins: the caster binds residents from the census (relations kin / friend are "anyone"
  yet), beasts from namedInRegion, bosses from worldBossOf; `reward` pays band-scaled gold / XP (+ loot for rich and
  great); `secret` advances the place's culture's first alloy recipe by 40; `befriend` calls Life::befriend.
- Engine (`rpg/story/story.h`, `story_internal.h`): `scriptById` resolves saga ids; `Engine::guard_` (saved),
  `Engine::sagaOffers_` (runtime) and `SA_SAGA = 900` (DLG_STORY + SA_SAGA + i offers sagaOffers_[i], arg the hook site);
  `RESIDENT_CENSUS` (Binding::trade of a census-bound resident, id = life::npcId(site, idx)); `strHash64s`.
- Tables (`rpg/story/arch/arch_tables.h`, `arch/index.cpp`: frozen): one add function per file. ARCHETYPES lane:
  arch/scripture.cpp (holds the worked example `the_pit`, THE BROTHER IN THE PIT), myth.cpp, legend.cpp, lewis.cpp,
  tolkien.cpp, twists.cpp (the worked examples `rival_claim`, `road_omen`). VOICE lane: arch/folk.cpp, maas.cpp,
  gwynne.cpp, world.cpp, voice.cpp (phase A: the 24-key vocabulary of saga.h with 3 stand-in lines each). CAMPAIGNS lane: rpg/story/campaigns/arcs.cpp,
  chain.cpp (phase A: no plans). COMPOSER lane: compose.cpp (complete except pick / offers), guard.cpp (first cut).
  CMake globs rpg/story/*/*.cpp too.
- Saves: SAVE_VER 13 (the story block v3 = v2 + the guard; v1 / v2 blocks still load); fixture
  `tests/fixtures/save_v13.bin` carries a running saga (`saga1~the_pit~~~~v2~m2~0000a5a5` at stage `road`) and a
  guard record. ENDLESS_GEN_VER stays 15 (no generation change; goldens untouched).
- Tests and scripts: `rpg_test --sagalib` (the library lint: every archetype x voice x motive x fitting twist composes
  valid; banned modern names; voice phrases; campaign plans), `rpg_test --sagas [N] [--seeds A..B] [--sample K]`
  (composed stories cast on real worlds, every path walked, rewards, uniqueness metrics); CI runs `--sagalib` and
  `--sagas 2000 --seeds 1..5` (10,000). Script commands (rpg/view/script_saga.cpp): `saga start <arch> [twists] [vN]
  [mMOTIVE] [sSEED]`, `saga campaign <id> [arcs]`, `expect saga <stage|done|running>`; `story start` accepts spec ids;
  `tools/scripts/m6b_lead_saga.txt` (phone).

### 15.22 M7 "Home" phase A (lead, 2026-10-09): the housing, farming, animal and riding contracts

Section 8 and 13 M7 bind M7, with the owner's M7 notes (2026-10-09) on top: buy a cottage or a riverside plot, or build a
house in ANY culture style the player has discovered with the M3b builder (3/4 view, depth, seamless walls; 2+ storeys
have stairs and an upper-floor map); decorate and store; ~15 crops x 4 growth stages through the seasons; chickens,
cows, sheep, pigs, goats with predators; a hired farmhand; cooking (15.2: Well Fed, better at hearths and inns, crops /
livestock / fish feed it) on the economy's own goods and chains (grain -> flour -> bread, the forge) rather than
parallel systems; horses bought, mounted and ridden on roads (dismount rules), stabling; deterministic, cheap offline
catch-up; prices in the slow rags-to-riches curve (property is the big money sink, the M6 bands); phone-first build UI;
<= 4 KB per farm. Every new generation and sim RNG draw is sequenced explicitly (never two draws as arguments of one call,
one constructor or one concatenation: clang and MSVC must give the same goldens). Phase A contracts (a lane may ADD to the
header it is named the editor of, never rename, remove or change the meaning of what is listed here):
- `rpg/sim/home.h` (HOMESTEAD lane is its only editor): the calendar (`Season`, `seasonOf`: 4 x 28 days from day 1);
  property (`HouseKind` / `houseInfo`: the 8.1 table; `LotSize` / `lotInfo`: 10x8 1500, 13x10 3500, 16x12 6000;
  `Shell` / `shellInfo`: hut, cottage, longhouse, townhouse and hall with footprint, storeys, the builder's form and
  purpose, materials and their credits, days; `priceFactor` 0.7..1.4 by prosperity); the object catalogue (`Obj` x42:
  24 yard objects and 18 pieces of furniture, `ObjInfo` flags OBJ_* (outside / inside / solid / rotates / storage / cook /
  bed / animals / ground / wall decor / joins), price, the furniture's art::Prop, capacity; `farmObjArt`); `PlacedObj`;
  crops (`Crop` = art::Crop x17, `CropInfo`: days 3..8, yields 1..3, seasons, warm, perennial, paddy, its ew::Good,
  seed price, value; `CropRec`; wilting after 2 dry days, nothing dies); animals (`Animal` x8, `AnimalInfo`: critter,
  home, price, product and how often, hay; `AnimalRec`; `animalName`); `Product`; horses (`Breed` x6 / `breedInfo`:
  7.2..9.2 tiles/s, sure-footed, 700..2600 gold; `breedOfCulture`; RIDE_ROAD_BONUS 15 %; `Dismount` reasons); cooking
  (`Ingr` / `ingredientOf` over every food and good, `Station` campfire < hearth = pot < inn kitchen, `Meal` /
  `baseMeals` (12) / `cultureMeals` (3 per culture), `makeMeal`, `mealBuff`); the item conventions (no new ItemKind:
  Food / Misc sub ranges FOOD_CROP 64, FOOD_PRODUCT 96, FOOD_MEAL 128, MISC_CROP 64, MISC_SEED 96, MISC_PRODUCT 128,
  MISC_TOOL 144, MISC_DEED 160; `makeCropItem`, `makeSeeds`, `makeProduct`, `makeTool`, `isSeed`, `isTool`,
  `isHomeGood`, `goodOf`); the property record `Plot` (a bought House or a Lot: id, settlement, global rect, flags PF_*,
  the house's shell / style / seed / footprint / build days, outside and inside objects, RLE ground edits, crops,
  animals, stores, trough, catch-up day, tax, farmhand, name; MAX_* caps; PLOT_BYTES_BUDGET 4096, STORE_STACKS 24);
  `Ui` (the Mode::Build screens' state: None / Shell / Yard / Decorate / Storage / Cook / Buy); `Homes` (Game::home:
  plots, discovered styles, the horse ridden, dismount counts, ui; the home block); the pure rules (`catchUp`
  (bounded by CATCHUP_DAYS = a year, deterministic over split spans), `rainDay`, `canPlaceOutside` / `canPlaceInside`
  (overlap, bounds, and the gate-to-door / to-every-usable-object BFS), `gateOf`, `doorOf`, `houseBldg` (the built
  house as a Bldg the builder designs: Bldg::home 1 built / 3 building, id IdKind::Plot 0xC00+), `forSaleHouses` (0-1 per
  village at 40 %, 1-2 per town, 2-3 per city, deterministic), `houseKindOf`); the actions on a Game (buy a house or a
  lot, start a build, place / remove objects, till, plant, water, harvest, buy animals, collect, hay, the farmhand,
  cook, the stores, mount; `stampWindow`; `debugLot` for scripts and tests: the nearest clear ground).
  Phase A implements all of it as first cuts (home.cpp, home_game.cpp).
- `rpg/sim/game.h` (HOMESTEAD lane is its only editor): `Mode::Build`; `Game::home`; `DLG_HOME .. DLG_HOME_END`
  (5000..5500); hooks called from the core loop and defined in rpg/sim/home_game.cpp: `homeStep` (after foeStep, timed
  as PerfCounters::S_HOME), `homeInteract` (interact(), before props), `homeUseProp` (first prop handler),
  `homePropUsable` (interactProp), `homeMapLoaded` (enterBuilding / enterSite / changeFloor), `homeTalk` /
  `homeChoose` (after craftTalk; dialogueChoose routes the range), `homeAte` (useItem food), `homeShopStock`
  (shopStock), `homeBed` (bedIsYours); public `homeRiding`, `homeSpeedMul` (updatePlayer's pace), `homeDismount`
  (called on an attack / bow / spell / roll, a blow taken, entering a building or a site, resting; water in homeStep).
  `HomeOps` is the lane's friend; Game::marks tags 96..111 are the HOMESTEAD lane's. A save made inside the player's
  own house reloads inside it (the house is stamped before the building is entered). A house under construction
  (Bldg::home 3) cannot be entered.
- World (`rpg/sim/world.h`, `rpg/world/plots.h`, `rpg/world/source.h`; LAND lane is the only editor): `Bldg::home`
  (0 / 1 built / 2 bought / 3 under construction: not entered, drawn as a building site), `World::placeSerial` (bumped
  whenever the window's tiles are rewritten: the plots are stamped again), `World::lots` / `lotById` / `lotHandle`
  (append-only, global tiles), `ew::PlotPlan` (id, settlement, rect, LotSize, PLOT_RIVERSIDE / PLOT_FENCED, the gate;
  the layout contract in plots.h), `ChunkData::plots`. Phase A generates no lots yet.
- Art (`rpg/art/art_home.h`, ART lane is its only editor; included by rpg/art.h): `art::Crop` x17 / `cropSprite(crop,
  stage, variant, wilted)` (16x32), `farmlandTile` / `pathTile` (joins), `FarmObj` x22 / `FarmObjLook` (joins, turned,
  stage, the culture's ArchStyle, the player's arms) / `farmObjW/H` / `farmObjSprite`, `scaffoldSprite(w, h, storeys,
  progress 0..3)`, `horseSheet` (MOUNT_FRAMES 8 x 3 rows, 36x34) and `riderSeat`, `trophySprite`, `PaintingSpec` /
  `paintingSprite` (32x24), `Prop::ForSaleSign` (homePropW/H/Frames, paintHomeProp); `Critter::Cow, Sheep, Horse`;
  `Posture::Ride`; 16 `Icon`s (Seeds .. Deed). Phase A: stand-ins (art_home.cpp; the farm beasts on the goat's body;
  the icons as their nearest classic icon; Ride as Sit).
- View (`rpg/view/view.h`, VIEW lane is its only editor): `HomeDraw` / `homeDraws_` / `homeTex_`, `homeCollect`
  (drawWorld y-sorts its draws as Drawable kind 8; a Bldg with home 3 is not drawn as a building), `drawHomeDraw`,
  `drawMounted` (the player's figure when riding), `drawBuild` / `buildKey` / `buildTap` (Mode::Build; hud.cpp routes
  them). Phase A: rpg/view/home_view.cpp (ground edits, crops, joined fences, objects, the building site, the ghost,
  the horse and rider) and rpg/view/buildmode.cpp (the yard and shell screens).
- Saves: SAVE_VER 14 (the home block after the craft block, length-prefixed, its own version byte; one length-prefixed
  record per plot); fixture `tests/fixtures/save_v14.bin` (a large lot by the start village with a finished cottage in
  the local style, a barley field, coop / pen / stable, chickens, a cow, a horse being ridden, a chest with a harvest).
  ENDLESS_GEN_VER 16 (goldens unchanged by phase A; the LAND lane re-records them when lots change generation).
- Tests and scripts: `rpg_test --home [--seeds A..B] [--fuzz N]` (CI): the tables, the calendar, growth / wilting /
  seasons, catch-up determinism over split spans and a 1000-day catch-up under 5 ms, the 1000-placement fuzz checked by
  an independent BFS, the 4 KB plot budget, the home block round trip and damaged blocks, and per seed a cottage built
  in the local style (building site, finished on its day), a horse bought and ridden (x1.6+), the dismount triggers
  (attack, a blow, entering) and a byte-identical save round trip with the farm. Script commands (rpg/view/
  script_home.cpp): `home lot|buylot|buyhouse|build|place|fence|crop|field|animal|fill|tools|days|ride|dismount|yard|
  shell`, `expect home plots|riding|crops|stage|built|dismounts`; `tools/scripts/m7_lead_home.txt`.

### 15.23 M7b "Cities & Voice": planned cities and natural dialogue (owner, 2026-10-10)

Two owner complaints after M7, fixed before M8:

**1. Cities need structure and order.** Today cities wind like overgrown villages. Villages look great and must NOT change;
towns get only a slightly tidier main street and square; cities get a real plan (still medieval, still organic at the
edges, every culture still building in its own style):
- **Street hierarchy:** main avenues from each gate to the market and the seat of power (palace, temple complex, council
  spire...), cross streets off the avenues, then lanes and alleys. Wider, straighter, paved avenues; narrower lanes.
  Blocks of row houses that front the street (continuous frontages, shared walls, back yards), not scattered houses.
- **Sidewalks:** raised kerbed walkways or flagged edges along house fronts on avenues and cross streets, gutters or a
  central drain, corner stones and bollards at junctions; the paving reads in 3/4 view with depth like everything else.
- **Squares and open space:** a market square, a temple or cathedral close, a square before the seat of power, small
  well-squares at lane junctions; fountains, statues, a market cross.
- **Green:** tree-lined avenues, walled gardens behind rich houses, at least one park or green with paths and benches,
  orchards and kitchen gardens inside the walls, courtyard trees, planters and window boxes.
- **Districts:** rich quarter (larger houses, gardens, clean paving), craft quarter (workshops, yards), market district,
  poor quarter and outskirts (tighter, plainer, dirt lanes), each with its own density and upkeep.
- Per culture the plan bends: the dune city of courtyard houses and covered souks, the jade city of axial avenues and
  walled compounds, the steppe khan's tent city of radial rings, the elven city among the trees, the fjord city along the
  harbour. Walls, gates, the economy, lots for sale (M7), the M5 census and M6 guards keep working; town goldens are
  regenerated (ENDLESS_GEN_VER bump).

**2. Dialogue must read naturally.** The owner finds dialogue choppy, unreal and at times hard to follow. A full pass over
everything spoken: sagas and story quests (composer, voice grammars, templates), radiant and life quests, shop, inn,
smith, stablemaster and guard talk, greetings and barks.
- **Flow:** each line follows from the one before; people answer the option the player actually chose; no stitched
  fragments or abrupt jumps between unrelated phrase picks.
- **Clarity:** the player always knows who is speaking, who is being talked about, what happened, what is wanted and why,
  and where to go next. Names and places are introduced before they are relied on.
- **Voice:** a consistent tone per speaker (grief, greed, fear, pride, warmth) that fits their job and mood; culture
  flavour stays light and never at the cost of being understood.
- **Fewer, fuller lines:** prefer two or three complete natural sentences over many short fragments; cut filler.
- **Gate:** a dialogue-quality review reads hundreds of generated conversations as a player would (a sample per
  archetype, hook and NPC trade) and fails anything confusing, robotic or incoherent; `--sagas` and `--voice` gain
  coherence checks (no repeated phrase within a conversation, every role introduced before use, answers match options).

### 15.24 M7b "Cities & Voice" phase A (lead, 2026-10-10): the city and voice contracts

15.23 binds M7b (the owner's words win on conflicts). Every new generation RNG draw is sequenced explicitly (clang and
MSVC must give the same goldens). Phase A contracts (a lane may ADD to the file it is named the editor of, never rename,
remove or change the meaning of what is listed here):
- **Urban fabric** `rpg/world/urban.h` (CITY lane is its only editor; append-only): `ew::Urb` (None, Avenue, Street, Lane,
  Alley, Sidewalk, Gutter, Square, Close, Park, ParkPath, Garden, Court, Quay), `ew::Quarter` (Centre, Rich, Craft,
  Poor), the byte (bits 0-3 class, 4-5 upkeep 0..3, 6-7 quarter: `urbPack` / `urbClass` / `urbUpkeep` / `urbQuarter`),
  `urbCarriage` / `urbOpen` / `urbGreen`, `urbName`. Carried as `Map::urb` / `Map::urbAt` (settlement buffers and the
  endless window), `ChunkData::urb`, copied by chunkgen and the window (applyChunk, shifts); never saved. Written by
  `town::Gen::urbanFabric()` (rpg/world/town_urban.cpp, the last step of finish(), no Rng draw). Phase A fills it for
  cities from the M7 plan (main streets -> Avenue, a fronted edge of a 3-wide main street -> Sidewalk, lanes -> Lane /
  Alley, squares -> Square, the compound's paving -> Court; quarter from the district, upkeep from the quarter and the
  city's wealth). ALWAYS 0 in a village. Purely visual (walkability stays the ground's and the props').
- **Row houses** `Bldg::row` / `bld::Request::row` (bits `bld::ROW_W`, `ROW_E`, `ROW_YARD`; `bldgRequest` passes it;
  `Request::key` mixes it only when non-zero): a footprint abutting its neighbour on the same front row shares that wall
  (the builder drops the shared side walls, runs the eaves, ridge and foundation on as one terrace), ROW_YARD a walled
  back yard behind. Set by the CITY lane's generator (never in a village), drawn by the CITYART lane's builder / painter.
- **City props** (rpg/art/art_props.h, after ForSaleSign; `isCityProp`, frozen `cityFootprint`): Bollard, Planter,
  GardenWall (joins), Hedge (joins), FlowerBed, FruitTree (a tree: `isTreeProp`), Monument, Pavilion (3x2), Canopy
  (walked under, drawn over the walker; `propSolid` false). Painted by `rpg/art/art_city.h/.cpp` (CITYART lane; phase A
  stand-ins: `cityPropW/H/Frames`, `paintCityProp`); classified Kind::None in rpg/build/registry.cpp until their painters
  take the culture's parts.
- **Villages never change**: `rpg_test --villages` (tools/tests/test_villages.cpp + tests/fixtures/golden_villages.txt,
  LEAD-owned and FROZEN, recorded before any M7b change): 183 synthetic villages (every settlement archetype x land,
  every culture, every forced layout; seeds 1..3) and the 21 real villages of the endless world in the 5x5 regions round
  the origin (seeds 1..3) hash as in M7, with no `Map::urb` and no `Bldg::row`. rpg_test --towns hashes `urb` / `row`
  only where present (a village's town hash is unchanged).
- **Voice** `rpg/sim/talk.h/.cpp`, namespace `speech` (TALK lane is its only editor; the SAGA lane uses it read-only):
  the phone panel (`PANEL_CHARS` 70 x `PANEL_LINES` 5, `OPTION_CHARS` 66, `wrapLines`, `fitsPanel`, `fitsOption`); one
  line at a time (`Topic`, `Pri` P_GREETING 10 .. P_URGENT 100, `propose(Dialogue&, pri, topic, text)`: replaces
  `Dialogue::text` only at an equal or higher priority; `Dialogue::pri` / `topic` new, reset by talkTo); the talk memory
  `Memory` (`Met`: npcKey, first / last day, times, TM_INTRODUCED / TM_KNOWS_PLAYER / TM_THANKED / TM_SLIGHTED, last
  topic and line hash; 1024 people, the least recent forgotten; 64 recent line hashes: `saidRecently` / `said`) as
  `Game::talkMem`; the recorder (`Turn`, `Conversation`, `Recorder`; `Game::talkRec`, null except in tests); the lint
  (`lint(Conversation, LintCtx)`: C_EMPTY, C_PLACEHOLDER, C_CASE, C_PANEL, C_OPTION, C_REPEAT, C_DOUBLE, C_STITCH,
  C_FRAGMENT, C_NAME, C_ECHO, C_PUNCT, C_SELF; `formatPhone`). `Game::talkTo` is now talkToInner + the memory (every
  human spoken to is met) + the recorder; `dialogueChoose` is dialogueChooseInner + the recorder.
- **Saves**: SAVE_VER 15 (the talk block after the home block, length-prefixed, its own version byte 1); fixture
  `tests/fixtures/save_v15.bin` (save_v14.bin removed; a person met three times, introduced, knowing the player's name,
  their last topic and line, a bark said lately). ENDLESS_GEN_VER 17. Town goldens: the 69 city cases re-recorded (the
  urb layer); villages and towns unchanged.
- **Tests and scripts**: `rpg_test --villages` and `rpg_test --dialogue [--seeds A..B] [--dump FILE] [--sample N]
  [--strict]` (CI: real conversations in the start village, the nearest town and city, streets and the inn, shop,
  smithy, temple and seat; every option of every opening followed a step; linted; phase A gate: placeholder / case /
  empty only). Script commands (rpg/view/script_city.cpp): `urb [r]`, `expect urb <class|rows> <min>`, `cityprops`.
  `tools/scripts/m7b_lead_city.txt` (a heartland city, its fabric, the props on a stage), `m7b_lead_talk.txt` (the start
  inn's keeper at phone size). Phase A baseline of --dialogue seeds 1..3: 392 conversations with 130 people; 81 speakers
  naming themselves in the third person ("HAVE YOU DEALT WITH IT YET? QUASEAS IN VOND NEEDS 6 MEAT." said by QUASEAS),
  89 stitched fragments (journal notes in brackets inside speech, "AND PRAY ... KEEPS TO THE SOUTH." glued on), 27 names
  used before they are introduced, 13 repeated phrases.

### 15.25 M7b "Cities & Voice" phase B (lanes + integration, 2026-10-10)

Done as specified in 15.23 / 15.24; docs/PLAN.md "M7b, Phase B results and integration" lists what each lane built.
Contract additions: town mask kinds `K_PARK` / `K_ROWYARD` and a role per square (market, seat, close, well); the city
generator places the notice board; `bld::VF_PARTY_W` / `VF_PARTY_E` (builder `terrace()`, only when `Request::row` is
non-zero); `Bldg::row` is part of the open-front cache key; `art::cityPropStyled(p, style, variant, joins, snow)`;
`DlgOpt::grp` / `Dialogue::group/front/shows()` option groups (`A_GROUP` / `A_BACK`), `speech::FIRST_PAGE` 6,
`speech::C_ANSWER` (outside `C_ALL`); script commands `cityspot`, `expect urb none`, `gotourb`, `expect kerbs`.
Villages: `--villages` and `city_gallery --check` (village art) unchanged. SAVE_VER 15 / ENDLESS_GEN_VER 17 stand.

### 15.M7c "Order of Streets": one settlement planner for villages, towns and cities (owner, 2026-10-11)

M7b gave cities avenues, sidewalks, parks and terraces, but the streets branching off the avenues are still the old
winding lanes, so cities do not yet read as planned. The owner's concept image (`docs/ref/m7c_city_concept.png`,
generated with ChatGPT: "closer, not exact") shows the PRINCIPLE: a walled city where the main avenues AND the streets
off them are real streets forming regular blocks, a central plaza with the seat of power, districts, gardens, orchards
and fields set in blocks, a harbour on the lake. It is a principle, not a template: every city must be unique.

**One planner, an order dial.** Replace the separate village/town/city street logic with ONE settlement planner that
shares its vocabulary everywhere (road hierarchy main road > street > lane > footpath; frontage: houses face a road;
junctions and squares; the edge rule: built-up core loosening into outskirts and then fields), driven by an order
level from settlement size and culture:
- **Village (low order):** one spine road through the middle, houses scattered around it and loosely facing it,
  paths wandering to the well, mill and fields. It must keep the organic charm the owner loves (review compares
  before/after shots of the same villages; any village that looks worse fails). The byte-identical `--villages`
  lock is replaced by a village-character check (low structure score, organic placement, one spine).
- **Town (mid order):** a real main street and square, a few structured side streets with proper frontage, loosening
  into scattered houses and yards at the edges.
- **City (high order), inside the walls:** a full street network, not just avenues: avenues from each gate to the
  central plaza and seat; streets off them laid as a real grid of blocks (row houses fronting every street around
  shared back yards and gardens); intersections, corners and continuous frontages everywhere; whole blocks given a
  role (seat or temple precinct, market block, park, orchard and kitchen-garden blocks, craft yards). Not graph
  paper: grid sections may tilt where an avenue changes direction, streets bend gently along a river, lakeshore or
  hill, a few run diagonal where walls or terrain demand; but every street is clearly a street, never leftover space.
- **City, outside the walls:** faubourgs ribboning along each approach road (inns, smithies, stables, houses) dense
  near the gate and sparser further out, sporadic away from the road; then farmland (fields, orchards, pasture)
  organic like the countryside.

**Every city unique** (a generator, not a template): wall outline (round, oval, polygonal, squeezed along a river or
coast, following the land), 2-5 avenues at their own angles from that city's gates, block sizes, tilted grid
sections, rivers with bridges and quays or a lake harbour or none, where and how big the plaza and seat are (one
square or a chain), the district mix and where the park, orchards and temple precinct fall, culture plan types
(jade: strict axial plan with walled compounds; dune: courtyard blocks and covered souk streets; steppe: rings round
the khan's court; fjord: terraces along the harbour; heartland: market grid round a cathedral close; others by
their culture data), and age and size (an old capital denser and more layered than a young frontier city).

**Checks:**
- A structure score per settlement (share of buildings fronting a straight-ish street, block regularity, dead ends,
  street connectivity): cities clearly above towns, towns clearly above villages, every city above a floor.
- A uniqueness check: city plans across many seeds must not be near-duplicates (layout similarity metric).
- Review agents compare the city map view and street-level shots against the concept principle, and villages
  before/after; commercial 16-bit quality, 3/4 view, seamless walls and paving.
- Everything that works today keeps working: walls and gates, the economy and markets, M7 lots for sale, the M5
  census and life, M6 guards, interiors, sagas casting places; capital build-time budget (`--towns` gates);
  regenerate goldens and bump ENDLESS_GEN_VER.

### 15.M7c-A "Order of Streets" phase A (lead, 2026-10-11): the planner contracts

15.M7c binds M7c (the owner's words win on conflicts). Every new generation RNG draw is sequenced explicitly (clang and
MSVC must give the same goldens); integer / dmath maths only. Phase A contracts (LEAD-frozen unless a lane is named the
header's editor; a lane may ADD to a file it owns, never rename, remove or change the meaning of what is listed here):
- **The planner's vocabulary** `rpg/world/town_plan.h` (frozen): `ew::Rank` (None, Main, Street, Lane, Footpath,
  Square; `rankStreet` / `rankWay`), `ew::BlockRole` (Homes, Seat, Temple, Market, Park, Orchard, KitchenGarden,
  CraftYard, Compound, Barracks, Harbour, Faubourg, Farm), the order dial `orderFor(type, cArch, capital, age)`
  (villages 10..25, towns 40..60, cities 70..100; the culture bends it) and `cityAge(seed, capital)` (0 young .. 3
  ancient), `PlanTraits` (order, age, layout, planKind, WallShape, WaterKind, gates, avenues and their bearings,
  block size, tilted grid sections, plazas, the seat's offset, blocks per role, faubourgs).
- **SettlementOut** (rpg/world/settlement.h): `rank` and `role` (one byte per buffer tile: Rank, BlockRole) and
  `traits`. Written by `Gen::rankLayer()` (rpg/world/town_plan.cpp, the last step of finish(), no Rng draw): every
  street tile a planner did not rank itself (`Gen::setRank`) takes its rank from the city plan's class (avenue,
  gutter and sidewalk: their carriageway's) or the mask (main street Main, lane Lane, square Square). Never saved,
  never in the chunks. `Gen::order` / `Gen::age` set in land() after pickStyle.
- **The checks** `rpg/world/town_metrics.cpp` (read-only on SettlementOut): `structureScore` (0..100 = 30 frontage +
  20 straightness + 20 regularity + 10 dead ends + 20 connectivity, over the wall ring's box or the buildings'),
  `planSignature` / `planSimilarity` (32 x 32 cells of the core: Jaccard of the street cells and of the building
  cells, the wall's radial profile, the aspect; best of the 8 rotations and mirrors), `villageCharacter` (structure,
  main-road arms through the heart, setback spread, houses in rows, blocks, city fabric). Targets (town_plan.h):
  every city >= 68, cities' mean >= 78, towns' mean 35..62, every village <= 40 and their mean <= 28, gaps of 15;
  no two cities >= 0.70 alike, the mean <= 0.48.
- **Baseline (M7b, seeds 1..3 + the real settlements in the 5 x 5 regions round the origin)**: villages mean 28.7
  (radial / grid villages up to 63), towns 42.2, cities 70.7 (min 44.5: the steppe rings; imperial 89, river 85,
  starspire / steppe / sun-temple / marsh 56..61); city plans mean similarity 0.39, most alike 0.52.
- **Tests** (tools/tests/test_plan.cpp): `rpg_test --structure [--strict]`, `--city-unique [--strict]`, `--plan`
  (one line per settlement: where, order, traits, ranks, structure). `rpg_test --villages` is now the village-character
  check (a plain run fails city fabric in a village; `--strict` every character rule); the frozen M7b village hashes
  only REPORT which villages changed, for the before / after review. The plain runs report the phase B targets; CI
  turns on `--strict` at integration.
- **Generator split and hooks** (byte-identical: `--towns --golden` 201 hashes and `--villages` unchanged before the
  bump): the homes moved from town_build.cpp to `rpg/world/town_homes.cpp`; the planned city's part of them moved
  into CITY hooks in town_urban.cpp (`cityHomesWant`, `plannedHomes` (now leaves `faubourgWant` for outside the
  walls), `cityFront`, `cityHomesRound`, `cityHomesLast`); `Gen::cityAfter(phase)` (town_city.cpp) runs after every
  step() phase of a planned city (phase 15: just before finish); EDGE hooks `edgeBegin()` (phase 1, after the wall
  ring) and `outskirts()` (phase 9, before the top-up rounds) in `rpg/world/town_edge.cpp`; the EDGE lane's own state
  `Gen::edge` (`EdgeData`, rpg/world/town_edge.h).
- **Row-house faces** (rpg/build/blueprint.h): `bld::ROW_SIDE_W` / `ROW_SIDE_E` (a side wall looks onto a street:
  dressed as an elevation, never a blank party wall) and `ROW_BACK` (the back looks onto a street); the door stays in
  the front wall (interiors unchanged). Set by the CITY lane, drawn by the ART lane; 0 in every village.
- **World**: `EndlessSource::settlement(Gid)` returns a site's whole build (cached, else built now): the checks over
  real settlements, the world map's city plans, the `plan` script command.
- **Scripts** (rpg/view/script_plan.cpp): `plan` (the settlement here: order, traits, structure, ranks, roles) and
  `expect structure <min> [max]`. Review scripts `tools/scripts/m7c_villages_review_s1..3.txt` (the same villages shot
  before and after: the pre-M7c binaries are kept in `%TEMP%\claude\emb_M7c\before_bin\`) and `m7c_lead_plan.txt`.
- **Versions**: ENDLESS_GEN_VER 21 (so the lanes may change generation under it); SAVE_VER 15 unchanged
  (save_v15.bin re-recorded for the generator version).

### 15.26 M8 "Fellowship" phase A (lead, 2026-10-11): the companion contracts

Section 9, 13 M8 and the owner's 15.6.2 bind M8, with the owner's M8 notes (2026-10-11) on top: companions can die
PERMANENTLY (the player cannot), with deep bonding; they are recruited from the living world (tavern regulars and other
M5 census residents with their own job, home, family, friends, needs and history; mercenaries; people rescued in sagas)
and each has a personality, culture, background, opinions and a stake in the world (their burned home village, a war, a
lost kin); roles, commands (follow, wait, attack, defend, use skill), a phone-first command radial, the companion paper
doll with visible culture gear, party portraits; downed -> recovery window -> death if not saved, with grief in their
family and friends and a grave; approval and the deeds bus reacting to real choices (sagas, war, crime, mercy); bond
skills unlocked by trust; banter (culture borders, weather, places, each other) and personal quests GENERATED through
the M6b saga composer from their real history, all at the M7b speech bar; an optional bonded hound or wolf raised from
a pup; companions take 25-40 % of the damage across three bands, are never stuck more than 30 tiles behind, path through
cities, ramps and doors, ride with the M7 horse and can live at the player's house; villages unchanged. Every new RNG
draw is sequenced explicitly. Phase A contracts (a lane may ADD to the file it is named the editor of, never rename,
remove or change the meaning of what is listed here):
- **`rpg/sim/party.h`** (FELLOWS lane is its only editor; rules in party.cpp): `party::Vocation` (Warrior, Archer, Mage,
  Healer, Rogue, Hound), `Origin` (Tavern, Resident, Mercenary, Rescued, Saga, Animal), `State` (With, Downed, Waiting,
  Home, Left, Dead), `Cmd` (the radial: Follow, Wait, Attack, Defend, Skill, Passive, Home), `Hook` / `Stake` (the stake
  in the world: burned home, lost kin, war, revenge, heirloom, debt, exile, faith, ambition, love, with the site, kin,
  foe, kingdom and ruin ids), `Personality` (four axes -2..2, likes / dislikes as Deed bits), the deeds bus's `Deed` x24
  + `DeedInfo` (axis weights, DF_HOME, DF_LOUD) + `approvalDelta` (-15..15; a war against the homeland is a betrayal),
  `Skill` x18 (three per vocation at bond 20 / 50 / 80) + `SkillInfo` / `skillsOf` / `unlockedSkills`, `Slot` (the
  player's eleven), the tuning constants (MAX_RECORDS 8, PARTY_PEOPLE 1 / 2 from LEADERSHIP_LEVEL 20, APPROVAL_COMPLAIN
  -40 / LEAVE -70 / QUEST 50, DOWNED_SECS 30, REVIVE_SECS 2, REVIVE_HP_PCT 30, LOW_HP_PCT 35, FOLLOW_TILES 1.5,
  CATCH_UP_TILES 30, BANTER_GAP_SECS 90, wages 30..120, SHARE_LO/HI_PCT 25 / 40), `Memory` (deeds remembered), the
  `Member` record (identity from the census: npcId, site, census index, name, job, culture, look seed; personality,
  stake; approval, bond, MF_* flags; health, the bleeding-out clock, wounds; their own pack and eleven slots; horse,
  wage; where they wait or live (global tile or building / site map, the player's plot); their record together; the
  VOICE fields: personal quest spec and instance, talk bits, last line), `Fallen` (the grave: name, census identity,
  global tile, day, cause, epitaph seed, belongings, GF_* flags), `Stats` (damage taken / dealt by player and party, the
  worst lag, catch-ups, downs, revives, deaths, deeds, banter), `Party` (members, fallen, nextId, the pending deeds,
  the banter clock; the party block, its own VERSION 1, records length-prefixed), `capacity`, `levelFor`, `Body` /
  `bodyFor` (the balance knobs), `vocationFor` / `personalityFor`, the names; and the VOICE lane's entry points
  `banterStep`, `deedLine`, `cryLine`, `epitaph`, `aboutLine` (rpg/sim/banter.cpp).
- **`rpg/sim/game.h`** (FELLOWS lane is its only editor): `Game::party`; `Actor::comp` (members index + 1) and
  `Actor::partyTag` (the lane's scratch); `Ev::Banter` (a member speaks) and `Ev::PartyAlert` (low health, downed,
  revived, died, joined, left, complains, a skill learned); `DLG_PARTY .. DLG_PARTY_END` (5500..6000);
  `PerfCounters::S_PARTY`; the public API (rpg/sim/party_game.cpp): `deed` (the bus), `PartyOffer` / `partyOffer`,
  `partyRecruit`, `partyCommand`, `partyEquip` / `partyUnequip` / `partyGive`, `partyDismiss`, `partySay`, `partyLook`,
  and `dressAsResident` (rpg/sim/life_game.cpp: a census resident's street look on any actor); the hooks called from
  the core loop: `partyStep` (after homeStep, timed), `partyActor` (updateActor: a companion's whole behaviour),
  `partyFell` (kill(), first: downed instead of dead), `partyHit` (damage(), after foeHit), `partyKilled` (kill(),
  after foeKilled), `partyAggro` (pickTarget; inside caves, ruins and buildings hostiles now consider companions too),
  `partyQuest` (completeQuest / questFail), `partyRested` (rest()), `partyInteract` (interact(), first), and the VOICE
  lane's `partyTalk` (talkToInner, right after the reset), `partyFolkTalk` (a section-7 hook, Topic::Companion) and
  `partyChoose` (rpg/sim/party_talk.cpp); `PartyOps` / `PartyTalkOps` friends. Deeds wired in phase A: kills
  (partyKilled), quests done and failed, a smith's patterns stolen, a war's side joined, feeding / giving coin to /
  befriending townsfolk, the inn's meal, a shrine's blessing, the first time into a ruin or a cave. A downed companion
  is not smashed like a skeleton's bones; `sleepy` never puts a companion to sleep; speakerTitle names a companion.
- **Life** (`rpg/sim/life.h`): `RF_COMPANION` (512, saved; always with RF_AWAY while they travel; RF_AWAY alone is cleared
  when they go home so their census life resumes there).
- **Speech** (`rpg/sim/talk.h`): `speech::Topic::Companion`.
- **Art** (`rpg/art/art_party.h`, ART lane is its only editor; included by rpg/art.h): `portraitSprite(look, mood)`
  (PORTRAIT_W/H 24), `HoundLook` / `houndSheet` (HOUND_W/H 28x22, HOUND_FRAMES 10: idle 2, run 4, bite 2, down 1, sit 1)
  / `houndPortrait`, `GraveLook` / `graveSprite` (GRAVE_RITES 5, 24x28); `Posture::Downed`, `Posture::Kneel`
  (art_life.h). Phase A: stand-ins (a crop of the standing sheet; a plain shaded beast; the NamedGrave prop).
- **Audio** (engine/audio.h): `Sfx::Revive`, `Knell`, `Howl` (stand-ins on Heal, Bell, Bark).
- **Saves**: SAVE_VER 16 (the party block after the talk block, length-prefixed, its own version byte 1); fixture
  `tests/fixtures/save_v16.bin` (save_v15.bin removed: a census resident of the start village travelling with the
  player with approval 37, bond 23, their own gear, a remembered deed and the VOICE fields; a fallen mercenary's grave
  with belongings; the census knows them away). ENDLESS_GEN_VER 21 (no generation change: goldens untouched).
- **Tests and scripts**: `rpg_test --companion [--seeds A..B] [--strict-share]` (CI): tables, approval bounds and the
  homeland rule, the party block round trip and 400 damaged blocks, per seed a census recruit (away in the census)
  following a 90-tile walk and the inn's door, deeds applied as approvalDelta says, downed -> bound in REVIVE_SECS -> 30 %
  health, the save round trip with a party, downed and left -> dead for good with a grave record and the census's death;
  the damage share in bands D 3 / 12 / 25 (reported; a FAIL with --strict-share until FELLOWS tunes it: phase A measures
  0 / 0 / 20 %). `rpg_test --banter [--seeds]` (CI): the VOICE entry points' lines through speech::lint and the phone
  panel, a banter line on a night road. Script commands (rpg/view/script_party.cpp): `party recruit|cmd|deed|down|kill|
  revive|near|talk|approval|bond|stats`, `expect party members|with|fallen|state|approval|near|banter|share`;
  `tools/scripts/m8_lead_party.txt`.
- **Ruled 2026-10-28 (owner; the armour proposal's question 11 with the owner's amendment; 15.37): a companion's kit is
  theirs while they live, and what you give them is yours.** The owner: "we probably will need to allow for players to
  give their companions better armor than what they start with though, so maybe you can't take their defaults but you
  can give them better armor and they will equip what's better." The same day the owner settled the two points the plan
  had read for itself: "Made once, and if they die they drop everything" (items 1, 5 and 6). Then the owner answered the
  four points left to confirm (15.37 items 4-7), and reversed the plan's reading of a companion who comes back: "well if
  they re join they should stay at the level they left, and shouldnt get treated as a new join since they arent new."
  (item 8). These are planned rules for the armour rules update after P2c DONE, not P2c work.
  docs/ARMOUR_SPAWN_RARITY.md section 6 has the detail.
  1. **Their kit.** It is made once, when they join (ruled: "Made once"), in their own people's make, so a later recruit
     starts higher on the metal ladder. Every piece carries IF_KIT.
     - It is never remade as they level: the hero's gifts are the only way up.
     - Its item level is the place's danger, as today, capped at the hero's level + 4 like any find, and its rarity rolls
       with the hero as the finder, so the luck gate (15.37's ruling 6) reads the hero. A death drops the kit as
       ordinary gear (5), so it may hold nothing a find there could not. At the join this costs the companion nothing:
       level sync already uses a piece at no more than the hero's level + 3 (effLevel, gear.cpp:36). Ruled: "that works
       for me".
     - While they live, the hero can't take a kit piece into the pack by any road (TAKE BACK, a swap or a parting), and
       can't sell it or break it down.
     - TAKE BACK on a kit piece is greyed, with their line ("THAT'S MINE. I CAME WITH IT.").
     - A companion who parts while living and later comes back is the same companion (ruled: "they arent new"). They
       come back in the kit they left in, which is never made again, with their level and their record (8). The plan had
       read a return as a new join, with the kit made again from its seed; the owner reversed it.
  2. **Giving.** The hero may give any piece they can wear: armour, a weapon, a ring or an amulet.
     - They wear it only if it is better than what they wear in that slot. "Better" means gear::powerScore at their level
       (party::levelFor, the level PartyOps::body syncs their gear to) more than 3 % over the worn piece's. That is the
       compare arrow's rule (lootp::verdict), moved into gear so the sim and the arrow share it.
     - An empty slot always takes the piece. Within 3 % is a tie, and they keep their own.
     - Otherwise they decline in their own voice (a gear line in banter.cpp, at the M7b speech bar), and nothing leaves
       the hero's pack.
     - The party page's arrow is read at their level, so a green arrow promises they will take it.
  3. **A worn gift.** Their own piece for that slot is stowed with them: it stays in their pack, unworn, with IF_KIT kept,
     and never goes to the hero. A gift displaced by a better gift goes back to the hero.
  4. **Taking it back.** A gift stays the hero's. TAKE BACK returns it, and they put their stowed piece back on, or leave
     the slot empty if they had none. Only what the hero gave can come back.
  5. **Parting and death.**
     - At every parting (dismissed, unpaid, or leaving in anger), they hand every gift back into the hero's pack at that
       moment, with their line and an alert naming the count, and put their own stowed piece back on in each slot a
       gift held, so they leave in their own kit. The pack has no gear cap (Game::addItem).
     - Their own kit, and the potions and food they were given, leave with them and come back with them (8).
     - A companion sent home is still a member, and their gifts can be taken back from the party page.
     - **At death they drop everything** (ruled: "if they die they drop everything"). Everything they carry (their kit,
       worn and stowed, the hero's gifts, and their belongings) drops on the ground where they fell, as the game's own
       ground loot: one Pickup per item (game.h:245-254), laid as a body's loot is (game_rpg.cpp:217-221). It keeps that
       loot's rules. It flies to the hero within 40 px and is taken at 9 px (game.cpp:1710-1732), and it lies for 240 s
       (game.cpp:1734). It is never saved, so it is gone once the hero changes map (clearNonPlayer, game.cpp:1946) or
       loads. Each kit piece drops without IF_KIT, as ordinary gear that sells and breaks down. Nothing goes into the
       hero's pack at the death itself.
     - Where they fell is the body that went down: their party body, or for a death in their census life on the hero's
       map, their census body. A death off the hero's map (a companion at home carried off in a raid whose rescue then
       fails) has no ground to drop on, so what they carried is lost with them.
     - Ruled, the pile's rules and the raid at home with them: "yep thats good".
  6. **The grave holds no keepsake.** Everything they had lies where they fell, so the grave is laid with nothing to
     take, and IF_KEEPSAKE is not added. The grave stays as the remembrance (15.6.2): their people's stone with their
     weapon laid on it (a look, not the item), the epitaph, flowers and words, their kin's grief and the party page's
     roll of the fallen. None of these reads the keepsake. A grave laid before the update keeps what it holds and its
     one keepsake, as today. A sellsword's inn slot still retires when he dies (MERC_FALLEN), and a census companion dies
     once, so each kit can drop only once. The one-time find that is left is accepted: "thats fine if someone wants to
     farm that way they can but there are defintly better avenues to make money".
  7. **Sellswords.** A sellsword who parts while living (dismissed, unpaid or angry) waits a week before he can be
     rehired: every such parting sets his hire mark to that day, where today dismissal erases it and an angry leave leaves
     it at his hire day. A rehire brings back the same man, with his record and the kit he left with (8), for a day's
     wage up front, as any hire. Nothing is made again, so nothing doubles.
  8. **Leaving and coming back** (ruled: "well if they re join they should stay at the level they left, and shouldnt get
     treated as a new join since they arent new.").
     - **Today a parting forgets them.** partyDismiss removes the whole record (party_game.cpp:914-922): bond, approval,
       memories, flags, the personal quest's state, the horse and the id go with the pack. An unpaid sellsword, a
       personal quest's farewell and a sellsword's angry leave go the same way. Asked again, they are built anew with
       their kit made again (party_recruit.cpp:359-375). Only a census companion who leaves in anger is kept, as
       State::Left, until the apology.
     - **Where they wait.** Their record waits whole in a new saved roll of the party block, `Party::parted`, beside the
       fallen, in a member's saved form so it comes back as after a load. The census can't hold it: it is rebuilt from
       seeds and saves only a few flags per resident. A census companion is found again by their npc id, a sellsword by
       his inn and slot. Their body lives on as today: a census companion in their own life at home, a sellsword at his
       inn table once his week is out, in the kit he left with. Those with nowhere to be met again (the animal, a saga's
       ally, a stranger rescued with no census home) go as today.
     - **What comes back.** The same id, so their personal quest finds them again; their bond and the skills it opened;
       approval, memories, flags, days, kills and talk; the horse the hero bought them; and their kit, worn as they left,
       with the potions and food they were given. The kit is never made again. Bond does not grow for the days away. The
       alert reads "JOINS YOU AGAIN."
     - **Their level** is never stored: it tracks the hero's, one below (section 9: "Level tracks player level − 1"). It
       can't fall while they are away, so they come back at the hero's level less one, never below the level they left
       at, and their kit keeps the item levels it was made at ("Made once"). The plan reads "stay at the level they left"
       as "nothing they earned resets" (15.37 item 8).
     - **Whether they come** is unchanged: a census companion needs what any recruit needs (alive, of age, off duty,
       room, and a reason to come: befriended, helped, or a regular at the inn), and a sellsword his week, room and wage.
       Joining never befriends them, so a raid captive who joined as Rescued may have no reason left (15.37 item 10).
     - **Angry leaves.** A census companion who left in anger comes back as today, by the apology, refused the first
       time and taken on a later day, which lifts approval to at least -30. No bond bar applies, and bond is untouched.
       A sellsword who left in anger keeps his record, angry, so his table says "YOU OWE ME AN APOLOGY FIRST" until the
       same apology is made there (15.37 item 9).
     - **Death while away** is a census death, as today: their kin grieve, nothing drops, no grave is laid, and their
       record leaves the roll.
     - **Old saves.** Anyone dismissed before the update joins anew, once. A companion travelling in an old save hands
       back all their gear as gifts at their first parting (it has no IF_KIT), so if they come back holding no kit piece,
       their kit is made then, once.
     - **Size.** MAX_PARTED 32; a record is at most about 1.2 KB, so a full roll adds at most about 38 KB to a save.
       MAX_RECORDS (8) still counts only members, so parting still frees a place. Past 32 partings, the friend parted
       longest ago is forgotten. The party block steps to version 5 and still reads 4, so save_v23 loads; SAVE_VER 23
       holds.
  - **The old exploit, closed.**
    - *The path today.* partyEquip calls partyUnequip on a filled slot, and partyUnequip calls addItem
      (party_game.cpp:871, 888). Any gift for a filled slot therefore drops their kit piece into the hero's pack, where
      Game::sell sells it and the smith's CR_STUDY breaks it down for the people's patterns. TAKE BACK on the party page
      does the same directly. Also, partyDismiss drops the hero's gifts along with the member, and a death makes them
      belongings.
    - *The fix.* partyEquip reads the verdict first, so a decline moves nothing. On accept it never calls partyUnequip:
      a worn IF_KIT piece is stowed in place (only `eq` moves), and a worn gift goes back to the hero.
    - *One door.* Every move from a companion to the hero's pack goes through one door that refuses IF_KIT: TAKE BACK,
      the gift swap and every parting. A death moves nothing into the pack: it lays everything on the ground with
      IF_KIT cleared. As a second lock, Game::sell and CraftOps::studyable refuse IF_KIT.
    - *Old saves.* A kit made before the update has no IF_KIT, so it counts as a gift, once. That is accepted: there is no
      SAVE_VER change, and a gift can't be told from a kit in an old save.
    - *A parting forgot them.* A dismissed companion asked again was built anew, which reset an unhappy companion's
      approval and dropped MF_QUEST_DONE, so a finished personal quest could be offered again (party_talk.cpp:200). The
      parted roll (8) keeps the record, which closes both, and a kit made again at a higher cap after the hero levels.
  - **Checked by** `rpg_test --companion` (CI). Its cases:
    - TAKE BACK refused on a kit piece;
    - a worse or equal gift declined, with both packs unchanged;
    - a better gift worn, with the kit stowed;
    - a gift taken back, with the kit re-worn;
    - a gift-for-gift swap;
    - each kind of parting returning exactly the gifts (item seeds compared);
    - a death dropping exactly what they carried, kit worn and stowed, gifts and belongings, as ground loot within a
      tile of where they fell (item seeds and counts compared), with no IF_KIT on any dropped piece, nothing added to
      the hero's pack, and a grave with nothing to take;
    - the kit's item level at most the hero's level + 4, on every recruit path;
    - the hire mark set on every living parting of a sellsword, his slot empty for 7 days from it, and MERC_FALLEN after
      his death (his slot never stands again);
    - leaving and coming back (8), for a census companion asked back at home and a sellsword dismissed, unpaid or angry
      and rehired after his week, with the hero 10 levels higher in between: their level the hero's less one and never
      below the level they left at, with every kit piece at its old item level; the same kit seeds in the same slots,
      with IF_KIT, and no kit made again; the same bond, skills, id, approval, memories and flags, with no bond for the
      days away; the gifts back in the hero's pack exactly once;
    - an angry census companion's record kept through the apology (approval lifted to -30, nothing else changed); an
      angry sellsword answering ANGRY at his table until his apology;
    - the parted roll through a save and load, save_v23 loading with an empty roll; a parted census companion's death
      removing their record, with no pile and no grave;
    - a seeded fuzz of give, take back, swap, dismiss, leave, rehire, rejoin and death over seeds 1..20 that never puts an
      IF_KIT item in the hero's pack or on the ground, and never makes a kit twice.
    `--loot-sources` gains kit and fallen-companion rows. Witness plays it once: give a companion a worse piece and then
    a better one, take it back, dismiss them, and read the pack; ask them back at their home and read their page (the
    same bond, skills and kit); then lose a companion in a fight and gather what they drop.

### 15.27 M8b "Treasure": the loot overhaul (owner, 2026-10-12)

The owner, in their words: "items need to have effects if they have a rarity. im getting a lot of rare items and almost none of them actually do anything, at least it doesnt say they do anything. one thing we want to make sure of is that we dont over saturate the player with a bunch of rare equipment right off the bat." The owner gave the level-10 iron sword example as a feel to aim for, not a rule: "you would need to build it out in a way that makes sense."

1. **The rarity ladder, with six steps.** Each step must be much harder to find than the one before.

   | Step | Colour |
   |---|---|
   | Common | white |
   | Uncommon | green |
   | Rare | blue |
   | Epic | purple |
   | Legendary | gold |
   | Mythic (new) | a flaming red-and-orange, animated shimmer on the name, frame and icon border |

   Every item drawn on screen must use these colours: its name, frame and icon, the loot beam or sparkle, the paper doll, the shop, the chest and the forge.
2. **Every rarity above Common does something, and the game says so.**
   - Each step adds real, scaled affixes. Uncommon gets one minor affix. Mythic gets several strong ones, including one unique, named power with a gameplay effect, not just a stat. Examples: chain lightning on hit, a burning trail, lifesteal, a thorns aura, a time slow on a perfect dodge.
   - Every affix must work in combat or the sim.
   - Every affix must show in the item tooltip and compare panel as plain words, for example "+12% DAMAGE VS BEASTS" or "BURNS FOES FOR 6 DMG/S".
   - No item above Common may roll without at least one working, visible affix. Audit every source that makes items: drops, chests, shops, quest and saga rewards, the forge, companions' gear and merchants. Fix any that give rarity without affixes.
3. **Item level matters as much as rarity.**
   - Every item gets an item level from the player level and the source's danger.
   - Base power scales with item level and material tier.
   - The rarity's affix budget scales with item level too.
   - The feel to aim for: a level-10 Common iron sword is roughly as strong as a level-2 Uncommon or Rare bronze sword, but the low-level rare can still win if its perks suit the build.
   - Show the item level in the tooltip, and compare against what is equipped (green or red arrows).
4. **No early saturation. Rarity is gated by player level** and by how hard the find was.
   - Rough level floors: Uncommon from level 1 but uncommon in fact; Rare from about level 5; Epic from about 12; Legendary from about 20; Mythic from about 30.
   - Near the floor a rarity is very rare, and it ramps up after.
   - Exploration, harder dungeons, bosses, finishing missions, and saga and campaign finales give guaranteed or boosted rolls. A campaign's last reward is the main way to get a Legendary or Mythic early. These are the meaningful ways to beat the gate a little, never a lot.
   - The current flat roll (55/28/12/4/1 %) is replaced.
   - A `--loot` (or extended `--loot-audit`) suite must prove all of the following:
     - The rarity spread per level band: by level 5, most gear is Common and Uncommon; Rare is rare; Epic and above are absent outside rewards.
     - Every non-Common item has at least one visible, working affix.
     - Power curves by item level and rarity, including the iron and bronze example.
     - No duplicate or contradictory affixes.
     - Affixes are deterministic and the RNG draws are sequenced.
5. **Keep it fair and saved.**
   - Affixes and item level are saved (bump SAVE_VER and regenerate the fixtures; old items load as item level from the player's level at load).
   - Companions use the same rules.
   - Balance gates (`--metrics`, `--power-curve`, `--foes`, `--arms`, `--companion`) stay green. Item power must not break the time-to-kill bands.

**Ruled 2026-10-28 (owner; 15.37).** The armour proposal keeps this section and sharpens it.
- **One channel each** (item 1; question 3; looks, inside P2c).
  - Rarity's colours stay everywhere item 1 lists them: the name, frame, icon border, loot light, paper doll screen,
    shop, chest and forge.
  - On the drawn figure, rarity never tints the metal. It shows as finish in the piece's own metal:
    - a Rare wears the next tier's trim, plus the glyph at T5;
    - an Epic is one tier finer and bulkier; a band-1 Epic adds trim, and an Epic at the T5 cap adds the glyph and an
      unlit gold inlay;
    - a Legendary is T6, with lit runes, motes and the edge spark;
    - the flame is the Mythic's.
  - Common may match Uncommon, and each step above reads finer at 1x.
- **"A little, never a lot"** (item 4; question 6, option b; planned for the armour rules update after P2c DONE).
  - Ordinary finds wait for their floors: Epic 12, Legendary 20, Mythic 30.
  - Luck (hoards, caches, bosses and named foes, best in a harder place) still lifts a find one step at any level up to
    Epic. It lifts to Legendary only from level 17, and to Mythic only from 27.
  - Campaign finales stay ungated: always a Legendary, with a Mythic chance from level 20. The dragon's two named
    Legendaries stay a set piece.
  - The proposal's model gives about 0.4 Legendaries before level 20 and 0.2 Mythics before 30 for a dungeon-goer. Today's
    whole-step bypass (gear.cpp's rarityAllowedTop) gives 0.9 and 0.5. `--ladder-pace` measures it.
- **Item 2's audit grows** with the new sources: war spoils, relics, companion kits and a fallen companion's pile (15.28).
- **Item 5, "companions use the same rules":** a kit rolls as a find, in their own people's make. It is made once, at
  their first join, with its item level capped at the hero's level + 4 and its rarity rolled with the hero as the
  finder, because a death drops it as ordinary gear. A companion who leaves and comes back brings it back (15.26).

### 15.28 M8b "Treasure" phase A (lead, 2026-10-12): the loot contracts

15.27 binds M8b (the owner's words win). Phase A contracts (a lane may ADD to a file it is the editor of, never rename,
remove or change the meaning of what is listed here):
- **`rpg/sim/items.h`** (NUMBERS lane is its only editor; items.cpp): `Rarity::Mythic` (5) and `Rarity::COUNT`,
  `RARITIES`, `rarityName`; `rarityColor` is the 15.27 ladder (white, green, blue, purple, gold, Mythic's flaming red-
  orange) and `rarityColorAt(r, t, phase)` its animated form (Mythic shimmers ember -> orange -> hot yellow; phase runs
  it along a name or a frame). Eight appended affixes, rolled only once the POWERS lane makes them work: `VsBeasts`,
  `VsUndead`, `VsPeople`, `Burn` (dmg/s), `AttackSpeed`, `ArmourPct`, `HealthRegen` (every 5 s), `Stamina`.
  `affixText(a, value)`: the plain words every surface shows ("+12% DAMAGE VS BEASTS", "BURNS FOES FOR 6 DMG/S").
  Eight appended Mythic powers (`Unique::Tempest, Inferno, Soulfeast, BrambleCrown, Chronos, Phoenix, Starfall,
  Wolfking`; `uniqueIsMythic`). `kItemAffixes` 4 (Mythic carries four strong affixes and its power). The Item layout
  is frozen at SAVE_VER 19 for M8b (no new Item fields).
- **`rpg/sim/gear.h`** (NUMBERS lane; gear.cpp): `LootSrc` (Body, Chest, Hoard, Explore, Boss, WorldBoss, Shop,
  Merchant, Quest, Saga, SagaFinale, CampaignFinale, Forge, Companion, Debug) and `lootSrcName`; `DropSource` gains
  `src`, `boost` (rarity luck steps), `atLeast` (a guaranteed floor), `atMost` (a cap), and `playerLevel` now counts
  (15.27.3: the item level and the gate read it); `rarityFloorLevel` (1, 1, 5, 12, 20, 30), `rarityOdds(s, pct[6])`
  (rollRarity draws exactly these, one draw), `itemLevelFor(s)` (dropIlvl = it); `rarityMult` / `affixSlots` for six
  steps; `attunes` counts Mythic; `affixImplemented` and `uniqueImplemented` (DEFINED in rpg/sim/powers.cpp by the
  POWERS lane: only what works rolls; rollAffixes filters by it); `mythicPowersFor(kind)`; `affixConflicts(a, b)`
  (rollAffixes never puts two conflicting affixes on one piece); `lootValid(it, why)` (the 15.27.2 audit: above Common
  an item level, at least one affix, each allowed, working, worded, none in conflict; Legendary / Mythic a working
  power, a Mythic one on a Mythic); `adoptClassic(it, playerLevel)` (a classic ilvl-0 piece made M6 gear with the
  affixes its rarity owes: Game::addItem applies it to everything that enters the pack, the safety net under every
  source); `powerScore(it, playerLevel)` (one comparable worth for the compare panel's arrow and the iron / bronze
  example); `Live::fx` (powers::State). Phase A keeps the M6 numbers (55 / 28 / 12 / 4 / 1 %, never Mythic from a roll)
  plus the floor and cap: the NUMBERS lane replaces them.
- **`rpg/sim/powers.h`** (POWERS lane; powers.cpp): `affixWorks`, `uniqueWorks` (phase A: the 18 M6 affixes and the 28
  M6 powers that had code; none of the M8b ones), `State` (Inferno trail, Bramble aura, regen, time slow, Chronos and
  Phoenix cooldowns, the dodge window, hits, the Wolfking summon), `Fx` (Lightning, FirePatch, Burn, Thorns, TimeSlow,
  Lifesteal, Phoenix, Star, Wolf, Proc).
- **`rpg/sim/game.h`** (POWERS lane is its only editor): `Ev::Power` (a = powers::Fx, p, the new `Event::q`, f, id),
  `Actor::powTag / powT / powBy` (never saved), `friend struct PowerOps`, `friend struct LootOps`
  (rpg/sim/loot_ops.h, SOURCES lane: dropLoot / openChest for the audit).
- **View** (`rpg/view/view.h`, VIEW lane is its only editor except the M8b POWERS block: `PowerFx`, `powerFx_`,
  `powerEvent`, `powerUpdate`, `drawPowerFx`, defined in rpg/view/render_powers.cpp; render.cpp calls the first two,
  View::draw the last, after companionOverlays). hud.cpp's item lines use `affixText` and `rarityName`.
- **Audio** (engine/audio.h, POWERS lane): `Sfx::Zap`, `Rebirth`, `LootChime` (stand-ins on Frost, LevelUp, Discover;
  the VIEW lane plays LootChime for Epic and better finds).
- **Saves**: SAVE_VER 19 (items write kItemAffixes = 4 affix pairs; the Mythic rarity): fixture
  `tests/fixtures/save_v19.bin` (save_v18.bin removed; a Mythic ring with four affixes, the fourth an M8b one, and the
  Phoenix power). ENDLESS_GEN_VER unchanged (M7c owns generation).
- **Tests and scripts**: `rpg_test --loot` (NUMBERS: every rarity x kind x item level valid, the odds, determinism, the
  spread by level band printed: phase A measures 54.7 / 28.4 / 12.2 / 4.7 % at levels 1-4, the saturation the owner
  felt), `--loot-sources [--seeds]` (SOURCES: real bodies of every rank and every shop in real games through
  lootValid), `--powers` (POWERS: which affixes and powers work; phase A 18 / 28 / 0 Mythic), all three in CI ("Loot
  test"). Script commands (rpg/view/script_loot.cpp): `lootdrop <kind> [rarity N|name] [ilvl N] [n K]`, `lootroll <n>
  [level L] [D d] [rank r] [src s]` (pieces laid outside the pickup pull), `expect lootvalid`, `expect lootpickups
  <n>`; `gear6` takes rarity 0-5; `tools/scripts/m8b_lead_loot.txt`.
- **Ruled 2026-10-28 (owner; 15.37), planned for the armour rules update after P2c DONE:**
  - `rarityAllowedTop` takes option (b): the luck lift stays one step above the level gate's top, but it reaches Legendary
    only from level 17 and Mythic only from 27. CampaignFinale stays ungated.
  - War spoils roll as `LootSrc::Body`, with D the siege's level, `playerLevel` the hero's and `culture` the soldier's
    people. A captain adds an elite's luck and `atLeast` Uncommon.
  - `dropLoot` skips actors with magicTag MT_SUMMON or MT_DECOY.
  - `--loot-sources` adds WAR SPOILS, RELIC, kit and fallen-companion rows. A source the game uses that the audit never
    ran still fails.
  - The `--loot` goldens are re-recorded once, because new band-4+ pieces get new names and strength. The random stream is
    unchanged.

### 15.29 M9 "Arcana & Mastery": magic plus a learn-by-use skill and perk system for everything (owner, 2026-10-12)

The owner, in their words: "a skill tree like in skyrim so not just for magic but also for skills like movement speed, damage with swords axes maces ect ect each weapon type has advatages and disadvantages agaist certain enemys get more skilled with armor types or specific culture armors or more broad skilling you get what im saying?"

M9 is the M9 "Arcana" magic plan (the milestone section and 15.5) plus a Skyrim-style progression system that covers the whole game.

1. **Skills levelled by use.** Each skill has a level from 1 to 100 that rises from actually doing the thing, at a pace that slows as it climbs. Character level rises from total skill gains, alongside the existing XP. That ties into the M8b loot gates, which read the player level.
   - **Weapon skills, one per weapon family:** swords, axes, maces/hammers, spears/polearms, daggers, bows, and unarmed or shields if they fit. Each family has real advantages and disadvantages against enemy types, shown in the UI. Examples:
     - maces crush armoured and skeletal foes, but are slow;
     - axes cleave and bleed beasts and the unarmoured;
     - spears outreach and stop charges (cavalry, wolves);
     - daggers backstab and crit the unaware;
     - swords are balanced, with parry;
     - bows have range and suit beasts, but are weak against shields.
     These must work through foe tags (beast, undead, armoured, construct, people, flying...) with the M6 foe roster and the M8b "vs" affixes.
   - **Armour skills:** light, medium and heavy. Each has its own trade-offs:
     - stamina and noise;
     - roll and dodge;
     - speed;
     - damage reduction.
   - **Material and culture mastery:** wearing a culture's armour or using a material tier builds familiarity. Perks bonus a full culture set, and a perk tree branch works with M3 culture gear.
   - **Movement and athletics:** run speed, sprint stamina, climbing ramps, swimming if it exists, and carry weight.
   - **Broad skills:**
     - smithing (M6 forge);
     - cooking (M5 needs and food);
     - farming and husbandry (M7);
     - riding (M7 horse);
     - speech, covering prices, persuasion and dialogue options in the M7b voice system;
     - stealth, covering sneaking, pickpocketing and the crime system;
     - lockpicking if it fits;
     - the magic schools.
2. **Perk trees.** Every skill has a perk tree. A perk point comes on each character level, and each perk requires a skill level.
   - Perks change how the game plays, not just numbers. Examples:
     - power attacks that stagger;
     - a dagger silent kill;
     - a heavy-armour charge;
     - the bow's slow-time draw;
     - a speech "intimidate";
     - smithing improvements;
     - dual-casting.
   - The trees are drawn as constellations or culture glyph trees, readable and tappable on the phone. The phone-first UI must be beautiful, matching the quality bar.
   - Respec is costly and rare: a shrine or a trainer.
3. **Magic.** Follow the existing M9 "Arcana" section and 15.5:
   - the Skyrim schools levelled by use, plus the culture traditions;
   - tomes and teachers;
   - dual-cast, upcast, surfaces and combos, concentration, statuses;
   - 3–4 quick-cast slots on the phone.
   Magic skills are just more skills in the same system.
4. **Trainers and books.** Trainers in towns and cities (via M5 census jobs) sell a few levels for gold, capped per level. Skill books are found as loot (M8b sources). Companions (M8) can also teach their vocation.
5. **Balance and tests.**
   - A `--skills` suite proves:
     - each skill gains from its own action and from no other;
     - the gain curve takes about 10–20 h of play to master a skill;
     - each weapon family wins and loses against the right foe tags (an A/B time-to-kill table);
     - the armour trade-offs hold;
     - perks apply and save.
   - `--spells` covers each spell within ±5% of its budget.
   - These stay green: `--metrics`, `--power-curve`, `--foes`, `--arms` and `--companion`. Adjust their baselines only with a written reason.
   - Bump SAVE_VER, regenerate the fixtures, and give old saves default skills based on their level.
6. **Testing scope (owner, 2026-10-12).**
   - Test what this milestone changes.
   - The full script regression runs once, at integration, via `tools/run_scripts.sh`.
   - Fixers and reviewers stay on the milestone's features.
7. **Magic must look absolutely awesome (owner, 2026-10-12: "i want the magic to look absoluly awesome like sick").** This is the headline visual of M9. The bar is top-tier pixel-art action games (think Hyper Light Drifter, Dead Cells, Eastward), not placeholder circles.
   - **The life of every spell.** Each spell gets a wind-up (the hands or staff glow, runes gather, a hum), a launch (a flash, a recoil, a small screen kick), travel (a lit trail, particles, sub-pixel motion), an impact (a burst, sparks or debris, a light flash on nearby tiles, a hit-stop of a few frames, a light screen shake), and an aftermath (scorch marks, frost decals, puddles, lingering embers or smoke).
   - **Real light.** Spells light the world through the existing lighting system. A fireball at night lights the walls it passes. Lightning flashes the whole scene for a frame.
   - **School identity.** Each school and each culture tradition has its own colours, shapes, motion, glyphs and sound motif, so you can tell a spell apart by its silhouette alone. For example:
     - Destruction fire boils and roars;
     - frost shatters in crisp shards;
     - lightning forks and chains;
     - Restoration rises in warm motes;
     - Conjuration tears a rift with summoned forms climbing out;
     - Illusion shimmers and warps;
     - Starcalling draws star-lines in the sky at night.
   - **Big moments.** Charged, upcast and dual-cast spells grow visibly bigger and more spectacular. A top-tier spell is a moment (a meteor, a storm cell, a summoned great beast) that makes the player go "whoa".
   - **Surfaces and combos read beautifully.** Oil sheen, burning ground, electrified water, freezing puddles and poison clouds each have animated pixel art and clear borders.
   - **Phone performance.** Particle and effect budgets must hold the web/iPhone frame budget (`--perf`, `--window`). Use pooled particles and baked effect sheets; it should look sick at 60 fps.
   - **Review.** The visual reviewer must judge every school's spell at 1x on a phone frame, by day and at night, and fail anything that looks flat, small, muddy or generic.

### 15.30 M9 "Arcana & Mastery" phase A (lead, 2026-10-12): the skill and magic contracts

15.29 binds M9 (the owner's words win), with the M9 "Arcana" milestone section and 15.5. Phase A contracts (a lane may ADD
to a file it is the editor of, never rename, remove or change the meaning of what is listed here):
- **`rpg/sim/skills.h`** (SKILLS lane is its only editor; skills.cpp, skills_perks.cpp, skills_game.cpp): `Skill` (25,
  saved by index: Sword, Axe, Mace, Spear, Dagger, Bow, Unarmed, Block, Light / Medium / Heavy armour, Heritage (material
  and culture mastery), Athletics, Smithing, Cooking, Farming, Riding, Speech, Stealth, Lockpicking, and the five
  schools Destruction .. Alteration), `SkillInfo` (name, what raises it, group, star colour, glyph seed), the curve
  (`xpToNext`, `xpToReach`: phase A 0.2 L^1.6 + 2 use points, 12 220 acts from 5 to 100), `charXpForSkillUp` (phase A 0),
  perks (`PerkDef`: name, plain-word line, level needed, parents, the star's place on a -8..8 grid, cost; up to 32 per
  skill, saved as bits; `perkTable` routes the schools to `magic::schoolPerks`; `perkBlocked`), foe tags (`FoeTag`:
  beast, undead, skeletal, armoured, people, construct, flying, charger, shielded, unarmoured, spirit, huge, dragon,
  unaware; `tagsOf(kind...)` / `tagsOf(Actor)`, beast / undead / people agreeing with the M8b vs-affixes), weapon
  families (`familyOf`, `FamilyTraits` swing time / reach / stamina / crit, `familyVs` rows "+35% VS ARMOURED: CRUSHES
  PLATE", `familyVsPct`, `familyVsLines`), armour classes (`Weight`, `weightOf` by material, `armourTrade`: speed, roll,
  roll cost, stamina regen, noise, damage reduction, softened by skill), the state (`Track`, `Skills`: levels, XP,
  perks, material and culture familiarity, perk points, trainer counts, books read, respecs, use counts, `live`
  runtime), `startLevels(background)`, `defaultsForLevel` (15.29.5), skill books (`ItemKind::Book`, `BOOK_SKILL`,
  `makeSkillBook`).
- **`rpg/sim/skill_ops.h`** (SKILLS lane): `SkillOps`, Game's friend, every hook already called where it acts: `use`
  (the one doorway, `Game::skillUse`), `strikeMul` / `struck` (meleeHit, a hero's arrow), `takenMul` / `taken`
  (damage()), `swingTime` / `reachAdd` / `swingCost`, `speedMul` (recalcPlayer), `rollMul` / `rollCostMul` / `rolled`,
  `staminaRegenMul`, `drawMul` / `loosed` (shootArrow), `detectMul` (ai.cpp's sense), `step`, `applyPerks`
  (recalcPlayer, after sumGear), `leveledUp` (gainXp: a perk point), `buyMul` (priceFactor) / `sellMul` / `traded`,
  `talk` / `choose` (talkTo's section-7 hooks, `DLG_SKILLS` 6000..6500), `shopStock`, `readBook` (useItem), `loot`
  (dropLoot, openChest), `forged` (the forge's give), `cooked` (home::cook), `farmed` (till / plant / water / harvest),
  and for tests and scripts `setLevel`, `takePerk`, `respec` / `respecCost`. Phase A: the gains count and level the
  skills (Ev::Skill), perks can be taken and respecced, every multiplier is neutral and no character XP comes from
  skills, so every balance gate reads exactly as before.
- **`rpg/sim/magic.h`** (MAGIC lane; magic.cpp, magic_perks.cpp, magic_game.cpp): `School` (+ `schoolSkill`,
  `schoolPerks`), the section 6 grammar (`Form` + Storm / Meteor, `Essence` + Arcane, `Mod`, `Resource`, `Signature`),
  `Status` (the nine of 15.5, on `Actor::status` / `statusT`), `Surface` (water, ice, oil, fire, poison, shocked water,
  steam, blood) and `comboOf`, the look's identity (`FxStyle`: a 5-colour ramp, `Particle`, `Motion`, glyph seed, sound
  motif, size), `SpellDef` (power at level 1 is what the budget checks; cost, cooldown, cast time, range, radius,
  duration, speed, concentration / upcast / dual / reaction flags, the surface it leaves, statuses, resource,
  tradition), `budgetOf` (6.3: basePower(form) x essenceMult x tier x rank x 0.85 per mod, x1.1 signature),
  `schoolSpells` (phase A: the classic three with their pre-M9 numbers and one first spell of every other school),
  `spellById`, the classic ids 1..3 (`legacySpellId` / `legacySpellOf` / `syncLegacy`), `Tradition` +
  `traditionOf` / `commonArcana` (phase A: Common Arcana everywhere), tomes (`BOOK_TOME`, `makeTome`), the saved
  `Book` (known, `QUICK_SLOTS` = 4 slots, the selected slot, mastery, attunements, trials, the non-magicka pools), the
  runtime `Live` (`Cast`, `Conc`, the `Field` of surface tiles with a serial for the view's caches, `Stats`, slot
  cooldowns), `Arcana` (book + live), the `Phase`s of a spell's life for `Ev::Spell` and `magnitudeOf` (upcast +0.35 a
  level, dual-cast x1.6).
- **`rpg/sim/magic_ops.h`** (MAGIC lane): `MagicOps`, Game's friend: `input` (updatePlayer: quick slots, the SPELL
  button, SWAP), `cast` / `classicCast` (Game::castSpell), `step`, `actorStep` (updateActor: statuses tick, frozen
  holds), `aiOverride` (updateAI first: frightened flees, frozen waits), `projHit` / `projEnd` (updateProjectiles),
  `hurtMul` / `hurt` (damage(): a frozen body shatters, a heavy hit breaks concentration), `learn`, `readTome`,
  `prepare` (the UI's quick-slot change; the rest-or-camp rule is magic's), `talk` / `choose` (`DLG_MAGIC`
  6500..7000), `shopStock`, `loot`, `rested`, `reset` / `background`, `syncLegacy`, `clearMap`, `shift`, `emit`,
  `laySurface`, `applyStatus`.
- **`rpg/sim/game.h`**: `Input::castSlot / castHold / castDual / sneak`; `Actor::status / statusT / statusBy`, the
  MAGIC scratch `magicTag / magicT / magicBy`, the SKILLS bleed `bleedDps / bleedT / bleedBy` and scratch `skillTag /
  skillT` (never saved); `ProjKind::Spell`, `Projectile::spell / mag / upcast / dual`; `Ev::Spell` (a = Phase,
  `Event::spell`, p, q, f, id) and `Ev::Skill` (a = skill, f = kind: 1 level, 2 perk point, 3 perk taken, 4 book, 5
  trained, 6 mastered, 7 respec); `DLG_SKILLS`, `DLG_MAGIC`; `Game::skills`, `Game::arcana`, `Game::skillUse`;
  `friend struct SkillOps, MagicOps`. `ItemKind::Book` (items.h). The classic `Game::spellsKnown` / `Game::spell` stay
  as mirrors of the book's classic spells (tests, debugKit, the old HUD line); the book is the truth.
- **View** (`rpg/view/view.h`, frozen: each block keeps its state in an opaque struct defined in its own .cpp and reaches
  View's internals through its friend Ops struct). SPELLFX block (rpg/view/render_magic.cpp; `MagicFxOps`):
  `magicEvent` (Ev::Spell), `magicUpdate`, `magicShift` (WindowShift), `drawMagicGround` (drawWorld, before the
  sorted pass: surfaces, decals), `drawSpellProj` (the sorted pass, ProjKind::Spell), `magicLights` (drawLighting),
  `drawMagicFx` (View::draw after the lighting), `magicFxStats`. UI block (rpg/view/skills_ui.cpp; `SkillsUiOps`):
  `kSkillsTab` 7, `kSpellsTab` 8, `skillEvent` (Ev::Skill: phase A a toast in the skill's colour),
  `drawSkillsPage` / `skillsPageKey` / `skillsPageTap`, `drawSpellsPage` / `spellsPageKey` / `spellsPageTap`. Phase A
  draws placeholders only (a glow, a ring, tinted tiles, a text list).
- **Saves**: SAVE_VER 20, the skills block and the magic block after the party block (each its own version byte 1);
  `tests/fixtures/save_v20.bin` (a level and XP, a perk, perk points, a book read, familiarity, SPARK and FEAR known in
  the slots, mastery). Owner 15.29.5 ("give old saves default skills based on their level") is honoured literally: a
  SAVE_VER 19 save still loads (the same layout without the two blocks) with `defaultsForLevel` and its classic spells
  in the book; `tests/fixtures/save_v19.bin` is kept as that check's input; 18 and older are refused as before.
  Game::serialize writes the classic fields and the book synced (magic::syncLegacy on copies), so a load is byte-exact.
  ENDLESS_GEN_VER unchanged (22): M9 changes no generation.
- **Tests and scripts**: `rpg_test --skills` (tables, trees, curve, tags, families win and lose, armour trade-offs, gains
  from the right act only, the block, perks and respec, a save) and `--spells` (every spell within +-5 % of its budget,
  ids, statuses, surfaces and combos, the book, casting through the quick slots), both in CI ("Skills and spells
  test"). Script commands: `skill set|use|perk <skill> <n>`, `skill points <n>`, `expect skill`, `expect perk`
  (script_skills.cpp); `spell learn|slot|cast|mp`, `surface <kind> <dx> <dy> [s]`, `status <name> <secs>`,
  `expect spell|status|surface` (script_magic.cpp); `tools/scripts/m9_lead_arcana.txt`.
- Phase A measured (MSVC, BDIR=build): rpg_test --seeds 1..20 20/20; save_test ALL OK; --skills, --spells, --powers (86
  A/B runs), --foes 1..3, --arms 1..3, --companion 1..3, --power-curve, --loot, --loot-sources 1..3, --loot-audit, --gear
  1..3, --villages --strict: 0 failures; --metrics vs M5: the same 3 lines as before M9 (first weapon 5.5 s vs 8.0, first
  level-up +10.2 %, 3 wolves lvl 3 died 5 % vs 17: the M7c start village, unchanged by phase A); m9_lead_arcana and the
  fight / m8b_powers / creator / cooking scripts 0 failures.

### 15.31 M10 "Beyond the Mist" phase A (lead, 2026-10-16): the sea, ship and hidden-land contracts

13 M10 binds the milestone, with 2.12 (the sea, boats, ferries, discovery, the Mist, first contact), 15.4 (harbours,
boats and fishing: fishing was never pulled earlier, so it lands here), 7.6 (the sea serpent, kraken and merrow; one
signature monster per isolated culture), 6.2 / 6.4 (the hidden signatures 2, 3, 4, 10, 11, 12 and their trials), 15.1
(the Sailor: swims farther, buys and sails boats early; the Marked one's hook) and 15.11 (the hidden kingdoms' metals and
gear never seen at home). Owner rules for M10: the full every-system regression runs ONCE, by the integrator; fixes are
verified with targeted checks only; the 16-bit quality bar and natural / organic world design stand. Phase A contracts
(a lane may ADD to a file it is the editor of, never rename, remove or change the meaning of what is listed here):
- **World (`rpg/world/source.h`, WORLD lane; implemented in `rpg/world/sea_world.cpp`)**: ENDLESS_GEN_VER 23. A LAND is
  one LCELL continent cell's shape (`LandPlan`: id = `landIdOf(cellX, cellY)`, kind Continent / Archipelago, centre and
  radius, origin, hidden, the Mist ring `mistIn` .. `mistOut` with `MIST_RING` = 300, a name); `landAt`, `land(id)`,
  `landsIn`, `hiddenLandAt` (shape or ring), `mistAt` (Q16: thickening over the ring's outer 60 tiles, thick, thinning
  over its inner 60), `seaAt` (`SeaKind` Land / Shallow / Deep / Storm), `portsIn` (`PortPlan`: the site, the pier head,
  the berth, harbour or jetty, the land, the kingdom, the name, `moorings` for the view's moored ships), `chartPlaces`
  (a hidden land's wreck and sea cave) and `trialOf` (a hidden kingdom's two trials; both stubs return false in phase A).
  `landmass()` stays the fast-travel connectivity id. `KingdomPlan::hiddenLand` (set in phase A from `hiddenLandAt` of
  the seat), SitePlan flags `SPF_HIDDEN`, `SPF_TRIAL`, `SPF_CHART` (+ `SitePlan::chartLand`), `SPF_PIRATE`,
  `SPF_SEACAVE`, `VignetteKind::Shipwreck` / `Lighthouse` (sea places: the land planner takes only the first
  `LAND_VIGNETTES` kinds, so phase A changes no generation). Phase A's lands: `landCell` draws what `continent()` draws
  and marks about one land cell in four (never the origin) hidden; nothing about the land's shape changed yet.
- **Cultures (`rpg/culture/culture.h`)**: `Culture::hidden` / `hiddenLand` (set by the WORLD lane BEFORE `applyPriors`, so
  the priors may branch on it; mainland cultures must come out bit-identical) and `ClimateSample::hiddenLand`.
- **Roles (`rpg/sim/world.h`)**: `Harbourmaster`, `Shipwright`, `Sailor`, `Pirate` (talk names and trade words added).
- **Monsters (`rpg/art/art_monsters.h`)**: `SeaSerpent`, `Kraken`, `Merrow` (stand-ins reuse the sandworm, spider and
  goblin painters; stats, names, plurals, script words, foe tags added). They swim: `Actor::swim`.
- **Props (`rpg/art/art_props.h`, painted in `rpg/art/art_sea.cpp`)**: `MooringPost`, `NetRack`, `FishBaskets`, `Buoy`,
  `Anchor`, `CargoPile`, `Slipway` (3x2), `HarbourDesk`, `Lighthouse`, `Wreck` (3x2); `isSeaProp`, `seaFootprint`
  (frozen), the builder registry classifies them (Kind::None). Piers are `Ground::Bridge` tiles (walkable; a wall to a
  boat). **Ships (`rpg/art/art_sea.h`)**: `shipSheet(hull, ShipStyle)` (8 headings x 2 bob frames, `shipCellW/H`,
  `shipAnchorX/Y`, `shipDeckY`), `ShipStyle` (hull / trim / sail colours, the culture's form and sail cut, a pennant,
  furled), `shipStyleKey`.
- **Sea (`rpg/sim/sea.h`, SEA lane; sea.cpp, sea_game.cpp)**: `Hull` (Rowboat, Sloop, Galleon: 2.12's prices, waters
  `W_SHALLOW / W_DEEP / W_STORM`, speed, wind gain, turn, hull, crew, cargo, level and the Sailor's earlier level, the
  body radius aboard), the upgrades `UP_*`, `Boat` (id, hull, name, the yard's culture, the dock, the tile, heading, hp,
  upgrades, sunk), the chart chain `Chain` (None, Sailor, Wreck, Cave, Chart) and `LandRecord` (`LF_RUMOURED`,
  `LF_CHART`, `LF_LANDED`, `LF_WORD`, `LF_MAPPED`, contactDay), fish `Species` / `Catch` (`FW_*`, `FC_*`, `FH_*`;
  phase A 9 species), `Sea` (saved: boats, aboard, ports known, lands, catches, counters; runtime `live`: speed,
  heading, helm, wind, storm, stormDark, mist, mistLand, mistT, turningBack, the fishing `Line`, swimT, the ferry
  `Voyage`, the autopilot, serial), `MIST_TURN_SECS` 20, `SWIM_SECS` 6 / `SAILOR_SWIM_SECS` 15, `fareOf` (20..400),
  `windAt`, `windFactor`, `SeaEv` for `Ev::Sea`.
- **`rpg/sim/sea_ops.h` (SEA lane)**: `SeaOps`, Game's friend, every hook already called where it acts: `input`
  (updatePlayer, after the regen block: aboard / fishing / swimming own the step), `passable` (moveActor, the player on
  a solid tile: swimming), `step` (update, after MagicOps), `aiOverride` (updateAI, after MagicOps), `interact`
  (Game::interact, after homeInteract), `interactLabel` (the HUD's use verb), `talk` / `choose` (`DLG_SEA`
  7000..7500), `shopStock`, `shift`, `clearMap`, `reset`, `background` (15.1 Sailor), `loaded`, `saving`; the working
  core `giveBoat`, `boatNear`, `board`, `disembark`, `landing`, `sailable`, `aboardBoat`, `heroGx / heroGy`. Phase A
  plays: a boat given, boarded (the player actor IS the boat: `Actor::swim` 1, the hull's radius, recalcPlayer keeps
  it), steered with the stick against the wind's push, kept to its waters (a rowboat runs aground at the deep water's
  edge), left at a landing; the Mist turns a chartless boat back after 20 s of fog (and parts for the chart); the boat
  aboard is saved at the hero's tile and a save at sea loads at sea.
- **`rpg/sim/game.h`**: `Input::reel`; `Actor::swim` (0 land, 1 the sea's own, 2 amphibious; `Game::swimSolid`,
  `bodyFree(..., swim)`); `Ev::Sea`; `DLG_SEA`; `Game::sea`; `friend struct SeaOps`. `realm::KingdomState::isolated`,
  `hiddenLand`, `contactDay` (set by `Realm::instantiate` from the plan; the realm block v3).
- **View (`rpg/view/view.h`, frozen)**: the SEA block (`rpg/view/render_sea.cpp`, `SeaViewOps`, opaque
  `SeaViewState`): `seaEvent`, `seaUpdate`, `seaShift`, `drawSeaWater` (after the terrain), `seaSorted` /
  `drawSeaSorted` (Drawable kind 10: the boats; the one aboard sorts just before the hero), `seaLights`, `drawSeaOver`
  (after the weather: the Mist, storms), `drawSeaHud` (drawHud), `seaFillInput` (Input::reel),
  `seaViewStats`. hud.cpp's use verb is `useVerbHere` (home's, else the sea's). Phase A draws stand-in ships, a grey
  Mist veil and a one-line sail panel.
- **Saves**: SAVE_VER 21, the sea block after the magic block (its own version byte 1); `tests/fixtures/save_v21.bin`
  (a sloop moored at a port, a port known, a land's chain at the sea cave, a catch). The M9 exception (SAVE_VER 19 still
  loading) ended with the generator change: every older save is refused ("start a new adventure");
  `save_v19.bin` / `save_v20.bin` retired. save_test adds a save made aboard (loads aboard, byte-identical).
- **Tests and scripts**: `rpg_test --sea [--seeds A..B]` (CI "Sea test", 1..3): hulls and fish, the lands and the Mist
  ring round the start, the sea block, boarding / sailing out of a port / saving and loading at sea / coming ashore, the
  Mist's turn-back without the chart and its parting with it. Script commands (script_sea.cpp): `boat give|board|leave`,
  `sea port|mistedge|chart|face|helm`, `expect sea aboard|mist|turned|boats|speed`; `tools/scripts/m10_lead_sea.txt`.
- **Test once (owner)**: the every-system regression (`rpg_test --seeds 1..20` and the full sweep of scripts and
  commands) runs ONCE per milestone, by the integrator. Lanes and fixers run only the checks that cover the code they
  changed, and re-run a check only after changing code it covers.

### 15.32 M11 "High Ground" phase A (lead, 2026-10-16): the relief contracts

13 M11 binds the milestone, with section 11 (height levels, edges between levels, readability cues, the walkability
guarantee, the map), 2.5 L2b step 7 (relief: cliffs, ramps and slopes from height levels), 2.7 (ramps on a global
lattice), 15.10 ("relief instead of brown mountain blobs": `Map::height`), 15.29 (athletics: climbing ramps), 15.31
(the M10 coasts, harbours, piers, islands, the Mist and hidden lands, which relief must respect: boats are never
stranded by height) and the M1 risk notes on bake cost and the iPhone frame budget (12.3, 14.1). What the owner sees on
the phone: hills, cliffs, plateaus and waterfalls; climbing to a peak shrine is a journey; mountains read as height.
Owner rules for M11: the every-system regression runs ONCE, by the integrator; fixes are verified with targeted checks
only; the 16-bit quality bar (one 3/4 top-down perspective and top-left light across every sprite and face, depth and
volume, cast shadows, a cohesive palette, no boxy repetition) and natural / organic world design stand. What M2's first
aid already built stays (levels 0..7 per tile, cliffs on the lower tile of every step, the 16-tile ramp lattice, the
sliver passes, flats under settlements, faces drawn from a smooth level field, peaks and massifs, the map's hillshade,
contours and mountain glyphs); M11 deepens it. Phase A contracts (a lane may ADD to a file it is the editor of, never
rename, remove or change the meaning of what is listed here):
- **The height byte (`rpg/sim/world.h` Map, frozen)**: bits 0-2 the level, `HEIGHT_CLIFF` 8 (a face: not walkable),
  `HEIGHT_RAMP` 16 (a way up: walkable) and three new bits, every step bit on the LOWER tile of a 4-neighbour step:
  `HEIGHT_SLOPE` 32 (a gentle one-level step, a hillside: walkable, never with CLIFF or RAMP), `HEIGHT_FALL` 64 (a
  water tile a river or a plateau lake's spill pours over a step into, always with CLIFF: a waterfall, a wall to
  walkers, swimmers and boats), `HEIGHT_STAIR` 128 (with RAMP: built stone stairs; a RAMP without it is a worn earth
  ramp). `HEIGHT_STEP` = CLIFF | RAMP | SLOPE (any step). `Map::reliefSpeed(bits)` is the one definition of the walking
  factor (hillside 0.8, earth ramp 0.8, stairs 0.9, else 1); `reliefSpeedAt`, `fallAt`. Chunkgen, war camps and home
  plots test `HEIGHT_STEP` where they meant "a step".
- **World (`rpg/world/source.h`, WORLD lane)**: ENDLESS_GEN_VER 24. `SPF_SUMMIT` 4096: a summit shrine
  (`SiteType::Shrine`) on the top level of a named peak or a range's crest, reached by a trail of stairs and
  switchbacks up the stacked levels. Phase A's generation (chunkgen.cpp step 6, the first rules the WORLD lane tunes):
  a road's or track's way up is `HEIGHT_STAIR`; a river (not a bridge) on the lower tile of a step is `HEIGHT_FALL`; a
  one-level step of the low hills (both levels 3 or lower), off every settlement's terraces, where the clean elevation
  (`reliefE`) climbs less than a fifth of a level over 4 tiles, is `HEIGHT_SLOPE`. No plan changed (region goldens
  hold; the chunk goldens were re-recorded).
- **Props (`rpg/art/art_props.h`, painted in `rpg/art/art_relief.cpp`, FALLS lane)**: `SummitShrine` (3x2, solid, a
  light), `Cairn`, `PrayerFlags` (4 frames); `isReliefProp`, `reliefFootprint` (frozen); the builder registry
  classifies them (Kind::None); phase A paints stand-in blocks.
- **View (`rpg/view/view.h`)**: the RELIEF block (`rpg/view/render_relief.cpp`, FALLS lane; `ReliefViewOps`, opaque
  `ReliefViewState`): `reliefUpdate` (after seaUpdate), `reliefShift`, `drawReliefWater` (after the terrain, before
  the sea's water), `reliefSorted` / `drawReliefSorted` (Drawable kind 11), `drawReliefOver` (hud.cpp, after the
  weather, before the sea's Mist), `reliefViewStats` (falls drawn, mist puffs, sprays, the roar, ms). Phase A draws
  pale stripes over each fall tile and hands the nearest fall's roar to the audio. The baked look of the relief
  (faces, slopes, stairs, grading, cast shadows, the mountains' rock) stays in terrain.cpp (TERRAIN lane).
- **Audio (`engine/audio.h`)**: `Audio::setFalls(level, pan, size)` (the falls' roar; FALLS lane renders it).
- **Tests and scripts**: `rpg_test --relief [--seeds A..B] [--windows N] [--size T] [--map dir] [--no-budget]`
  (tools/tests/test_relief.cpp, WORLD lane): per seed the start's window and one on each of the 3 named ranges / peaks
  nearest it (512 x 512 tiles); FAILS on any walled plateau (a walkable component of 24+ tiles, off the edge and the
  water, not the largest, whose dry boundary is at least half cliff), prints the census (levels, cliffs, ramps, stairs,
  slopes, falls, stacked faces, plateaus and the smallest) and FAILS when the chunk average rises over 1.2 x the
  baseline. `rpg_test --climb` (tools/tests/test_climb.cpp, CLIMB lane; phase A: the speed contract; its header lists
  the pathing protocol). Script commands (rpg/view/script_relief.cpp, WORLD lane): `gotorelief
  cliff|stack|ramp|stair|slope|fall|high|plateau|summit [n]`, `expect relief walk|level N|near <kind>`, `expect
  reliefview falls N`. Scripts: `tools/scripts/m11_lead_relief.txt` (the relief tour every lane reviews before and
  after), `tools/scripts/m11_lead_bake.txt` (the terrain bake bench).
- **Baselines (phase A, MSVC Release, the dev PC)**: chunk generation 0.790 ms average over the --relief windows of
  seeds 1..3 (3072 chunks; budget 0.948); the terrain bake 275 ms average per chunk over m11_lead_bake.txt with
  EMB_BAKEBENCH=1 (41 bakes; budget 330). The census before M11: every step one level (0 stacked faces in 12
  windows), 0 slopes, 0 falls, smallest plateau 24 tiles, the top levels wide flat fields; where the Mountain eco's
  rock ground lies (`gotoeco mountain`) it still paints as a faceted grey rock field (the "brown blob" 11.2 forbids).
- **Saves**: SAVE_VER stays 21 (no format change); `tests/fixtures/save_v21.bin` re-made under ENDLESS_GEN_VER 24 (its
  FIX6 facts unchanged). A save from generator 23 is refused ("start a new adventure").
- **Test once (owner)**: the every-system regression (`rpg_test --seeds 1..20` and the full sweep of scripts and
  commands) runs ONCE per milestone, by the integrator. Lanes and fixers run only the checks that cover the code they
  changed, and re-run a check only after changing code it covers.

- **Phase B additions (recorded at integration, 2026-10-16)**: `RiverSeg::lv` and `lvEnd` (a segment's surface level
  and the next one's start); Impl `reliefLevels` (the chunks' level: natLevel compressed above 5, ridged, contour
  pairs), `fineLevel`, `hydroLv`, `planSummits`; summit trails use ids 0x800 | (0x340 + k); script commands
  `gotorelief switchback|roadsw`, `gotofall`, `fallfx`, `expect fallfx` (relief_script.cpp), `climbto|climbguard|
  climbparty|climbhero|climbseek|climbnav`, `expect climbup|climbnav|climbpace` (script_climb.cpp), `reliefstamp`,
  `gotochunkcliff`, `gotorock`, `reliefground`, `reliefrebake`, `weather clear` (TERRAIN, test only); `--relief
  --no-order`, `--probe X Y`; `Actor::reliefK`, the per-actor path cache and `Game::navStats` (CLIMB); chunkgen step 6
  works the levels on a 6-tile border with four sliver passes (integration). Open at integration: perched rivers on
  the compressed high ground (PLAN.md M11 Integration).

### 15.33 M12 "Rise and Fall" phase A (lead, 2026-10-16): the deep sim, service and lordship contracts

13 M12 binds the milestone, with 4.3 "Rise" (civil wars split realms; a strong bandit camp cleared by nobody for 30
days can become a warlord realm; abandoned places are resettled after 60 days), 4.4 / 4.8 (tiers and budgets), 4.5 (the
overlays: banners follow, charred and rebuilding places), 4.6 (every event is a rumour), 4.7 (enlisting: Recruit,
Sergeant, Captain, Marshal with a wage, campaign quests generated from active sieges; "a diplomacy quest forces a peace
roll"; kingdom-making), 7.6 (world bosses raid settlements as a sim event), 15.3 (investigable ruins, which M12
deepens: ruins of realms that fell DURING play, resettled ruins), 15.6.3 (slow, realistic drama: every war has a cause
and is foreshadowed; no "lively" default), 15.6.4 and the owner's decision of 2026-10-16 (YES to village lordship and
kingdom-making: own a village, pay tribute or declare independence, grow it into a kingdom or conquer one), 15.8
(kingdom identity: every new realm has its own name, arms and banner), 15.9 (campaigns arrive with M4 / M12; stories
cast from the living world), 15.20 (archetypes toward 150 by M12: M12 adds the rise-and-fall ones), 15.31 (hidden
kingdoms stay isolated until contact; islands and harbours) and 15.32 (relief). Verification: `--history 5000`, drama
metrics, `--save-soak 100` at <= 1 MB. What the owner sees on the phone: realms that rise and fall around him, and a
place in that history. Owner rules for M12: the every-system regression runs ONCE, by the integrator; fixes are
verified with targeted checks only; earlier milestones' polish is deferred until after M12; the 16-bit quality bar and
natural / organic world design stand. Phase A contracts (a lane may ADD to a file it is the editor of, never rename,
remove or change the meaning of what is listed here):
- **The realm (`rpg/sim/realm.h`, REALM lane; realm block v4)**: `KingdomState::origin` (`KO_GENESIS / KO_REBEL /
  KO_WARLORD / KO_SUCCESSOR / KO_PLAYER`; `rebel` stays true for every realm the generator never planned), `overlord`
  and `tribute` (a vassal), `claim` (a successor's or pretender's claim), `camp` (a warlord's camp), `player` (the
  player rules it: the tick never declares its wars, kills its ruler, splits it or makes its peace; it is always
  Active). `Camp` (`CF_CLEARED / WARBAND / WARLORD / WILD`; noted by realmNoteSites for every loaded
  `SiteType::BanditCamp`, cleared with the site) and `Lair` (a world boss's lair: woke, next raid, raids, slain);
  `noteCamp`, `campCleared`, `camps`, `camp`, `noteLair`, `lairSlain`, `lairs`. Forcing: `forceWarlord` (the camp's
  warband takes the nearest village: a KO_WARLORD realm), `forceSuccessor` (a fallen realm's remnant crowns a successor
  with its claim and old colour), `forceResettle`, `forceAlliance`, `forceVassal`, `forceBossRaid`. Diplomacy rolls the
  SERVICE lane's missions drive: `peaceRoll`, `allianceRoll`, `tributeRoll` (each records TermsCarried). Private
  `newRealmId` and `foundRealm` (every realm the generator never planned comes through it; `splitRealm` may move to it).
  `EvType` appended (saved as u8): `WarlordRose, WarbandRaid, SuccessorRose, Reunified, AllianceMade, AllyJoined,
  Vassalised, TributePaid, TributeRefused, Independence, FiefGranted, Enlisted, Promoted, TermsCarried, BossWoke,
  Rebuilt` (names in `evTypeName`, story DSL words in dsl.cpp). Phase A runs NONE of this from the tick: the M4 history
  targets are unchanged.
- **Renown (`realm.h`)**: the realm-visible mirror: `rank` / `sworn` (kept by the SERVICE lane), `lordOf`, `liege`,
  `realmOf` (kept by the LORD lane).
- **The player's realm (`rpg/sim/realm_player.cpp`, LORD lane)**: `grantFief(site, liege, how)`, `revokeFief`,
  `declareIndependence(name, ruler, female, colours, emblem)` (a KO_PLAYER realm holding every fief; the liege's
  relation at TradeBroken, cause Succession), `playerRealm`, `payTribute`, `refuseTribute` (the overlord's tension climbs
  a rung), `playerTakes` (a conquest by the player's banner).
- **The service (`rpg/sim/service.h`, `service_ops.h`, `service.cpp`, `service_game.cpp`, SERVICE lane)**: `Rank`
  (None, Recruit, Sergeant, Captain, Marshal; `rankName`, `wageOf` 12 / 30 / 70 / 150 gold a week, `meritToRise` 30 / 80
  / 180, `squadOf` 0 / 0 / 2 / 4 / 6), `OrderKind` (BreakSiege, Assault, Supply, Scout, Assassinate, Escort, Hold),
  `Order`, `MissionKind` (CarryTerms, SeekAlliance, DemandTribute, FirstEnvoy), `Mission`, `Past` (discharged,
  deserted, disbanded), `Service` (saved). `ServiceOps` (Game's friend) hooks, all wired: `step` (update, after
  LordOps::step), `talk` (P_SERVICE: captains / guards ENLIST, REPORTING, LEAVE; rulers and envoys hear missions),
  `choose` (`DLG_SERVICE` 7500..8000), `kill` (after warKill), `questDone`, `reset`, `loaded`; the core `enlist`,
  `addMerit`, `promote`, `leave`, `giveOrder` / `finishOrder` (QType::Campaign), `giveMission` / `arrive`
  (QType::Envoy), `sync`, `rise`.
- **The lordship (`rpg/sim/lordship.h`, `lord_ops.h`, `lordship.cpp`, `lord_game.cpp`, LORD lane)**: `How` (Granted,
  Charter, Resettled, Conquered, Founded), works `LW_BANNER / PALISADE / HALL / GRANARY / MILITIA / MARKET / WATCHTOWER
  / WALLS / SHRINE` (`workInfo`), `charterPrice` (a village 2500), `CHARTER_FAME` 300, `GRANT_RANK` 3 (Captain),
  `Holding`, `Lordship` (saved: coffers, liege, tribute due = a quarter of the taxes since the last tribute day, every
  30 days, the player's realm and its name). `LordOps` hooks, all wired: `step` (update, after SeaOps::step), `talk`
  (P_SERVICE: the steward / elder / innkeeper of a holding: COFFERS, TAKE, PAY THE TRIBUTE, RAISE MY OWN BANNER; rulers:
  ASK FOR A LORDSHIP (a Captain of theirs), BUY A CHARTER (fame 300)), `choose` (`DLG_LORD` 8000..8500), `questDone`,
  `reset`, `loaded`; the core `grant`, `independence` (the player's own heraldry), `collect`, `payTribute`, `take`,
  `heldSite`, `sync`, `rise`. Villages only until the crown (the LORD lane decides towns).
- **`rpg/sim/game.h`**: `Game::service`, `Game::lordship`; `friend struct ServiceOps / LordOps`; `Ev::Rise` with
  `RiseEv` (`rpg/sim/rise.h`, frozen: Enlisted, Promoted, Wage, Order, OrderDone, OrderFailed, Discharged, Deserted,
  Mission, MissionDone, FiefGranted, Tax, Tribute, TributeRefused, Independence, Crowned, WarlordNear, BossRaid);
  `QType::Campaign / Envoy / Fief` (quests.cpp gives them markers, lines and the giver's thanks); `DLG_SERVICE`,
  `DLG_LORD`. **`rpg/sim/world.h`**: roles `Steward`, `Warlord`, `Envoy` (talk names added; looks are the VIEW lane's).
- **View (`rpg/view/view.h`)**: the RISE block (`rpg/view/render_rise.cpp`, VIEW lane; `RiseViewOps`, opaque
  `RiseViewState`): `riseEvent` (render.cpp's event pass), `riseUpdate` (after reliefUpdate), `drawRiseHud` (hud.cpp,
  after drawSeaHud), `riseViewStats`. The journal's fourth section STANDING (`setJournalSection(3)`, `journal
  standing`; realm_hud.cpp draws it). Phase A shows each moment as a toast.
- **Props (`rpg/art/art_props.h`, painted in `rpg/art/art_rise.cpp`, VIEW lane)**: `LordBanner` (4 frames, tinted by
  its owner's arms in the view) and `Watchtower`; `isRiseProp`, `riseFootprint` (frozen, 1 x 1); the builder registry
  classifies them (Kind::None). **Prop budget**: Prop is a uint8_t and the chunk caches store prop + 1, so
  `Prop::COUNT` (now 254) may not pass 255; the one slot left is reserved. Everything else M12 shows (works, warlord
  standards and forts, resettlers, raid scars) is drawn by the view or reuses existing props.
- **Saves of phase B**: a lane that must change its block's layout bumps that block's own version byte and keeps
  reading the phase A layout, so `save_v22.bin` keeps loading; lanes never regenerate the fixture or edit
  tools/save_test.cpp (the integrator regenerates the fixture once and drops the old layouts).
- **Saves**: SAVE_VER 22 (the service block, then the lordship block, after the sea block; each its own version byte
  1); the realm block v4; `tests/fixtures/save_v22.bin` (sworn as a sergeant with an order, a mission and a deserted
  army; the start village held with coffers, tribute owed and a palisade going up; the renown mirror, a camp and a
  lair); save_v21.bin retired, SAVE_VER 21 refused. ENDLESS_GEN_VER stays 26: **M12 changes no generation** (identity
  never changes, only overlays: 4.5). A lane that must change generation asks the integrator first.
- **Tests and scripts**: `rpg_test --rise [--seeds A..B]` (tools/tests/test_rise.cpp: a warlord from a camp with its
  banner, an alliance, a vassal, a successor, a resettlement, a boss raid, the diplomacy rolls, enlisting to Sergeant,
  an order and a mission, the wage, a lordship's taxes and tribute, independence and 120 ticked days that leave the
  player's realm alone, the byte-identical save); **contract change (M12 review round 1, 2026-10-17)**: the player's
  realm no longer stays Active with no land: losing its last holding ends it (`playerRealmEnds`: KingdomFell news, its
  wars end, the lordship clears), and `--rise` accepts that fall (it checks the news, no open war, no lordship left) or
  else requires the realm to still hold land; `rpg_test --save-soak N [--seeds] [--days D] [--budget KB]`
  (tools/tests/test_soak.cpp: N save / load cycles while the world plays on, travel every 5th, an M12 act every 10th,
  play continuing on the loaded copy; FAILS over 1 MB or on any difference); both in CI ("Rise test"). `--history N`
  (M4) gains the M12 drama metrics (REALM lane). Script commands: `rise camp|warlord|successor|resettle|ally|vassal|
  bossraid|stats`, `expect rise warlords|successors|vassals|alliances|camps|lairs|player` (script_rise.cpp, REALM);
  `enlist`, `promote`, `merit`, `order <kind>|done|fail`, `mission <kind>|arrive`, `discharge [desert]`, `expect service
  rank|sworn|orders|missions` (script_service.cpp, SERVICE); `lord grant|free|collect|tribute|take`, `expect lord
  holdings|realm|treasury|liege` (script_lord.cpp, LORD); `journal standing`. `tools/scripts/m12_lead_rise.txt` (the
  tour every lane reviews before and after).
- **Test once (owner)**: the every-system regression (`rpg_test --seeds 1..20` and the full sweep of scripts and
  commands) runs ONCE per milestone, by the integrator. Lanes and fixers run only the checks that cover the code they
  changed, and re-run a check only after changing code it covers.

### 15.34 P1 "Fresh Roads": the first post-M12 polish batch (owner, 2026-10-17), and its phase A contracts (lead)

Owner playtest feedback (friends): (a) the same few quests repeat constantly, and some are weird; (b) in towns and
cities lots of NPCs stand still, which breaks immersion: NPCs must go about their lives, and the player should STUMBLE
across quests rather than interrogate statues. There is no section 13 entry for this batch: this section is its spec.
The measured survey (2026-10-17, scratchpad survey/SURVEY_REPORTS.md) found five offer sources with their own pickers;
a teller whose weekly saga roll fails (90 % of villagers) falls back to a hand-written tale even when it was finished,
so every villager in the world shares one tale and every farmer another; boards, ruins and famines offer their library
tale forever, which blocks the FAE, BURDEN and PLAGUE campaigns; radiant jobs come from a fixed 3-kind menu per trade
with lifetime caps per person and a 1-in-3 hash; Hunt jobs have no target; 34-36 % of town folk are trades with no
workplace (`work = -1`) parked WORK@STREET for the whole work block; arrival places everyone mid-task, so a town is a
still picture between schedule changes; quest givers are pinned to the plaza 07-21; quests are pull-only.

Scope (Batch 1 of the survey plan; NOT in scope: character art size / zoom, animations, the character creator, the
world-spanning main campaign; story engine changes must stay compatible with a future main-campaign act chain):
- **STORY** (rpg/story/**): finished library tales are never offered again; at most one teller per tale per town;
  boards, ruins, famines and the news fall through to generated sagas and campaigns when their tale is played; ruin- and
  world-event-started single stories; "NOBODY ELSE DARES NOT TO" (arch/lewis.cpp:399); the tone clash of injected
  phrases (voice_wisdom.cpp:447: the piper's "I WON'T HURT THEM" then "BLOOD WANTS BLOOD. YOURS WILL DO.").
- **JOBS** (sim/quests.cpp, life_quests.cpp, render_markers.cpp, hud.cpp): wider per-trade job menus; a weekly job pool
  per settlement driven by real needs instead of lifetime per-person caps and the 1-in-3 hash; Hunt jobs get a real
  target area and a marker; journal LEADS for rumours.
- **LIFE** (sim/census.cpp, life.cpp, life_game.cpp): workplace-less trades get a real day (market stalls, errands,
  deliveries and carry routes between buildings, shopping, visiting); staggered arrival placement so some people are
  always walking; no quest-giver plaza pin (life_game.cpp:467-481) and no generator-spawn giver pin, so givers live
  their schedule (the tracked quest's pin follows them); stumble-on hooks: overheard gossip naming a need becomes a
  journal LEAD; street incidents (a thief running, a broken cart, a brawl) open a quest; pathBudget checked on
  phone-class perf.

DONE gates (measured before / after with the two P1 harnesses below; phase A baselines on MSVC, BDIR=build):
| gate | target | survey | phase A baseline |
|---|---|---|---|
| finished tales re-offered (`--quest-mix`) | 0 % | 44 % | 57.9 % (17,138 of 29,591) |
| top-4 tales of story offers | <= 25 % | 68 % | 63.3 % (PARDON 14,030, FOUNDLING, CURSED GIFT, WAGER) |
| generated campaign plans started over 20 seeds | >= 5 of 6 | 3 (analytic) | 1 (REBELLION 11 seeds; the library's EMPTY THRONE 20, reported apart) |
| top-3 job kinds | <= 55 % | 80 % | 75.6 % (Hunt 15,238, Missing, Deliver of 37,855) |
| farmers' share of jobs | <= 30 % | 58 % | 30.9 % (with towns and the city in the visit set; 58 % start-village only) |
| Hunt offers without a target (no marker) | 0 | 100 % | 100 % (15,238) |
| town centre walking 08-18 (`--town-motion --city`, 8 seeds) | >= 15 % | 0-19 % | 15.4 % (arrival 10.1 %) |
| people static for 2 game hours (not sentries / posted workers in a work pose) | 0 | most | 692 (MERCHANT / TAILOR / SCHOLAR / STABLEHAND / SMITH WORK@NOWORK 410, generator spawns 157) |
| chunk / frame perf (`--window`, the capital timing gates, `--folk` perf) | flat | | the integrator's sweep |

Phase A contracts (a lane may ADD to a file it is the editor of, never rename, remove or change the meaning of what is
listed here):
- **One story picker (`rpg/story/story.h` `Offer`, `tellerOffer`, `placeOffer`; `rpg/story/offers.cpp`, STORY lane)**:
  storyTalk, the tale scan (the scroll marker), the notice boards, the heralds, the ruins' words, the news
  (eventStories) and `rpg_test --quest-mix` all ask here; the STORY lane changes the policy inside these two functions
  (phase A kept the M6b policy exactly: the talk numbers match the survey to the offer). `Offer::lib` (a library script
  index), `saga` (a spec id), `replay`, `campaign()`, `id()`.
- **Jobs (`rpg/sim/jobs.h`, `jobs_ops.h`, `jobs.cpp`, JOBS lane)**: `jobs::Book` (saved: the jobs block, its own version
  byte `JOBS_BLOCK_VER` 1) with `Pool` (a settlement's jobs for a week: `site`, `week` = day / 7, `PoolJob` qtype / need /
  state PJ_OPEN, PJ_GIVEN, PJ_DONE / giver npcKey), `Lead` (`from` Rumour / Gossip / Incident / Board / Teller, qtype
  (QType::COUNT: no job yet), state LS_OPEN / LS_TAKEN / LS_STALE, day, site, who (npcKey), hasPos / gx / gy, text),
  `LEAD_MAX` 24, `LEAD_DAYS` 10, `ErrandKind` (Thief, Cart, Brawl, Lost, Help). `Game::jobBook`. `JobOps` hooks, all
  wired: `step` (update, after ServiceOps::step), `accepted` (acceptQuest: the pool's job given out, the leads naming
  its giver taken), `questDone` (completeQuest), `reset`, `loaded`. **`Game::hasOffer` moved to quests.cpp** (phase A:
  the M2 rule unchanged) so the JOBS lane owns who offers work without touching game_rpg.cpp.
- **Leads (`Game::addLead(const jobs::Lead&)`, jobs.cpp)**: any lane may add one (LIFE: overheard gossip, incidents;
  JOBS: rumours; STORY: tellers); id / day / state filled in, a duplicate of an open lead (same person and quest type, or
  same text) is not added twice, the newest 24 kept, "NEW LEAD: ..." said. The journal shows them on the QUESTS page
  (hud.cpp, JOBS lane); no new journal section (the phone strip is full).
- **Errands (`QType::Errand`, appended; `Game::openErrand(const Quest&)` / `finishErrand(qid, success)`, game_rpg.cpp)**:
  a street incident's quest, taken on the spot (LIFE lane): Quest::stage the ErrandKind, giver the victim, subject the
  journal line, hasPos / tgx / tgy the place; finishErrand pays at once (completeQuest) or fails it (QF_FAILED).
  quests.cpp gives it a marker (questTargetM2), a journal line (subject) and a thanks line.
- **The giver's pin (`Game::questTarget`, game_rpg.cpp; `Game::lifeGiverSpot`, life_game.cpp, LIFE lane)**: a finished
  job points at the giver themself while they are in the map, then where the census says they are (`lifeGiverSpot`:
  phase A stub, false), and only then at their door.
- **Campaigns**: THE WAR OF THE TWO CROWNS (`succession`, written by the M12 REALM lane) was never listed in
  `campaignPlans()`; phase A listed it (chain.cpp), so the six generated plans are BURDEN, PLAGUE, FAE, REBELLION, DRAGON
  and SUCCESSION (`--campaigns 1..5`: 60 walks, 0 failures; `--sagalib` 0 failures).
- **Marks**: Mk tags 128..143 JOBS, 144..159 LIFE (game_internal.h comment); STORY keeps its tags below 32.
- **Saves**: SAVE_VER 23 (the jobs block after the lordship block); `tests/fixtures/save_v23.bin` (the start village's
  pool with a job given and one open, an open and a taken lead, a street errand running); save_v22.bin retired, SAVE_VER
  22 refused. ENDLESS_GEN_VER 27 (the census changes: LIFE lane), SAGA_GEN_VER 2 (the STORY lane's composer changes;
  the save fixture's saga is "saga2~..."). Old saves are refused gracefully ("start a new adventure"). Lanes never
  regenerate the fixture or the goldens; a lane that changes its block's layout bumps the block's own version byte and
  says so; the integrator regenerates once.
- **Tests and scripts**: `rpg_test --quest-mix [--seeds 1..20 --weeks 12 --settle 6 --takes 3 --no-towns --gates
  --report]` (tools/tests/test_quest_mix.cpp: per seed the 6 settlements nearest the start from the region plans plus the
  nearest town and city, 12 weeks, every person on the street and indoors asked, the board, the nearest capital's herald,
  2 ruins within 160 tiles and the village news read, up to 3 offers a visit taken and finished, campaigns always taken;
  the survey probe read World::nearSites, which at the start holds only the start village); `rpg_test --town-motion
  [--seeds 1..8 --city --from 8 --to 18 --radius 24 --gates --report]` (tools/tests/test_town_motion.cpp: arrive at the
  heart at 08:00 as a player does, stand there to 18:00, look every 5 game minutes at everyone outdoors within 24 tiles;
  exempt from "static": guards, soldiers, captains, heralds at their post, and a posted worker (stall keeper, a resident
  at their own workplace, a field / yard trade, a bard playing, a beggar) in an animated work pose the whole time); both
  in CI with `--gates` ("Quest mix and town motion test"). Both test files are the lead's (frozen for the lanes: report a
  harness problem to the integrator instead of editing it). `tools/scripts/p1_lead_roads.txt` (seed 7: DERGZALT at 11:00
  and 13:00, the city at 14:00, `folk` snapshots, the journal): every lane reviews it before and after.
- **Test once (owner)**: the every-system regression runs ONCE per milestone, by the integrator. Lanes run only the
  checks that cover the code they changed, and re-run a check only after changing code it covers.

Phase B lane contracts (added by the integrator from the lanes' reports, 2026-10-17):
- **STORY**: the picker's policy (offers.cpp): a finished library tale is never offered again, by anyone; at most one
  teller per tale (and per generated archetype) per town, held in `Game::marks` (`MK_TALE_TELLER` 11, `MK_TALE_HEARD`
  12), passed on after 14 days or when the holder dies or starts another story; places offer a library campaign, then a
  generated campaign, then their own unplayed tale, then a generated story; `eventTaleFresh` lives in the picker.
  Campaigns may have alternative openings with their own world conditions, recorded in the campaign id (old ids without
  one still read): PLAGUE (a healer's notice on a poor town's board), SUCCESSION (a defaced crown notice on a market
  town's board, never the capital). arch/roads.cpp: 6 ruin-started and 6 realm-news-started single stories. Injected
  phrases may carry a "harm" tag (never after a "won't hurt" promise); twists never cut into a conversation. Story
  stand-ins walk a three-stop round by day and wait at a doorstep by night. Passing within 6 tiles of a teller adds a
  Teller lead (once per 5 days per teller). Tests: `rpg_test --story-p1`, `--story-events`; script `storyp1`.
- **JOBS**: weekly pools per settlement from real needs (dens, camps, caves, ruins, delivery towns, trade goods, famine,
  war), frozen for the week; 3-5 jobs a village, 6-10 a town, 10-16 a city, plus the board's two (drawn from the pool);
  givers picked by trade, last week's givers rest a week; no lifetime caps, no 1-in-3 hash (`hasOffer` reads the pool
  through a cached index). Every trade has >= 4 job kinds. Every Hunt targets a den or hunting ground (marker, "THIN THE
  X BY Y 2/5"). Rumours add place leads naming real jobs; a lead's person wears a small parchment "?"; town givers wear
  no "!". `JOBS_BLOCK_VER` stays 1 (runtime-only fields added). Tests: `rpg_test --jobs`; scripts `jobs`, `jobsleads`,
  `expect jobs`.
- **LIFE**: an outdoor resident's block is cut into 22-57 game-minute segments with their own phase and pace, each a
  task (a new spot, a stall with stock trips, carry routes between real doors, sweeping, reading or preaching, a visit, a
  round of the stalls); a fresh street puts 42 % of people part-way to their stop; spots unreachable from the town's
  heart are dropped; a stuck walker tries a spot in plain sight, then one step out of the snag. No giver pins:
  `lifeGiverSpot` returns the giver's planned spot, else the door they are behind. ai.cpp: stall keepers take 9-12
  minute stock breaks, wave their wares and can step out of the stall to flee; square folk path round stalls and re-goal
  after 1.5 s without progress; anyone more than 12 tiles from home flees to the nearest door. Gossip: a Gossip lead
  naming a real giver, once per giver per week, two per town per day, lines <= 30 characters. Street incidents (thief,
  cart, brawl) at most once per town-hour: stepping in opens and pays an Errand, ignoring one leaves an Incident lead;
  the plea is P_URGENT. Census, life block and goldens unchanged; Mk tags 144/145. Workplace-less trades were NOT given
  census workplaces (that would move the goldens): their day runs on street routes. Tests: `rpg_test --life-p1
  [--hooks --city --motion --report]`; script `tools/scripts/p1_life_hooks.txt`.
- **Integrator**: a long lead's "NEW LEAD:" notice is cut at a word to ~50 characters (the journal keeps the whole
  text); the board labels only a Bounty "BOUNTY: "; a sworn kingdom's guards pick their greeting from eight lines (the
  culture's salute is one of them); a spell lesson the player can't afford names the price and the shortfall and leaves
  the list; the Heirloom acceptance line no longer echoes the ask ("I'LL BE WAITING FOR IT."); script `saga kill
  [role]` kills the census resident bound to a saga role (the M6b fix scripts no longer name residents).

### 15.35 P1b "New Roads Content": the second post-M12 polish batch (owner, 2026-10-17), and its phase A contracts (lead)

Owner request: 15 NEW JOB TYPES, 30 NEW TOWN / CITY REQUESTS (10 village / hamlet, 10 market town, 10 city / capital,
some port-only) and 20 NEW ONE-OFF TALES, "well done, nothing crappy or half baked": every item complete, logical in the
world, with its full dialogue variety, reachable and finishable in play. There is no section 13 entry: the owner note is
the spec. The content is already designed and judged: the scratchpad `content/` folder (INDEX.md: the 65-row table and
the consolidated NEW-MECHANIC list; jobs_a/b/c.md, req_village/town/city.md, tales_a/b/c/d.md, each with a sibling
`.judge.md` of the quality judge's FINAL notes). EVERY judge note is resolved during implementation (fix the content, or
replace an unsalvageable item with an equally strong one, keeping the counts 15 / 30 / 20). Builds on P1 (15.34). Not in
scope: art size / animations, the creator, the main campaign. No version bump unless new save data needs it (phase A
needed none: SAVE_VER 23, ENDLESS_GEN_VER 27, SAGA_GEN_VER 2, the story block v4 and JOBS_BLOCK_VER 1 are unchanged).

**The phase A design (lead): ONE runtime for all 65 items, the story engine.** The NEW-MECHANICs the designers listed
(SITE-TALK, TURN-OPTS, the request frame with deadlines and census outcomes, escort / drove followers, BOUT-RULES,
SETTLE-TOWN, the city raise test, the port flag, carried body, hostage clock, beacon, filters...) are not 20 bespoke
systems: they are additions to the M4 story DSL (rpg/story/dsl.h documents every word), so every item, job, request or
tale, is a dsl::Script with talk stages and choices, goals, clocks and effects; the validator checks every line; one
headless walker plays every branch of every item; save data stays the M4 Instance (cast, stage, vars). Requests and
tales are library scripts (`family request` / `family tale`); the 15 job kinds are DSL TEMPLATES composed per offer into
a script named by a spec id, as sagas are (rpg/story/radiant.h). A mechanic that is cut degrades to what the DSL already
does (a person `in` a cave stands at its mouth; a bout lost to a kill still completes; an un-cast filter makes the item
not offered, never wrong).

The language added in phase A (parser and validator: dsl.cpp; runtime: story.cpp unless marked ENGINE):
| word | what | runtime |
|---|---|---|
| `family job / request / tale / test` | what the item is (reports, tests); `test` is never offered | phase A |
| `settle village town city capital port coast` | where it may be OFFERED: tiers OR'd, port / coast AND'd (offerGates) | phase A |
| `needs nowar / war / coast / season .. / stock <good> below / above n / mood below / above n / flag <moodflag> / building <type> [n] / level n` | more offer gates, AND'd | phase A |
| `once world / site`, `cooldown <days>` | once per world (default) or per settlement; a settlement's gap between cooldown scripts | phase A (marks 13, 14) |
| `gossip "<line>"` (0-3, <= 38, no placeholder) | overheard lines while the item is on offer in a town: a Gossip LEAD to its teller | REQUESTS lane (life gossip) |
| `person male / female [young] in <cave / ruin / camp>` | SITE-TALK: the person waits inside that site's map; young: a child or youth | ENGINE (phase A: at the mouth) |
| `site <type> ... with beast <m> / min n / max n` | a cave or ruin of that creature; a distance band | phase A |
| `site watchtower / dwelling`, `with road / flag <moodflag> / makes <good>` | vignette places; a settlement's census flag or product | ENGINE (refused until cast) |
| stage `deadline <days> -> <stage>` | a hard clock on a talk or goal stage; the tracker shows the days left | phase A |
| goal `escort <person> to <site> [wagon / drove / walk]` + stage `else <stage>` | the person follows the player; lost if they die | ENGINE walks them (phase A: done at the place, lost on death) |
| goal `bout <person / foe>` + `else` | not to the death: yield at 25 % (done), the player at 0 kept at 1 (else) | ENGINE (phase A: a kill wins) |
| goal `time <from> <to>` | done when the time of day is in range (dusk, the small hours) | phase A |
| conditions `is <var> n`, `days n`, `nodays n`, `season s`, `hour a b` | equality (`var x 0` / `novar x 0` are refused: the judge's always-true gates), days since the story began, the season, the time | phase A |
| effects `days <var>`, `census <site> mood / stock <good> / price <good> / famine / festival / refugees`, `reward <band> <pct>`, `ambush <monster / bandit / beast> <n>` | soft clocks, census outcomes, pay multipliers, a road ambush | phase A |
| effects `follow / stay <person>`, `hostile <person>`, `calm <site>`, `give ... heavy` | a walker behind the player; a person turns on the player (slay works on them); a site stands down; a burden (Engine::burdened) | ENGINE (phase A: recorded in vars) |
| `{var.<name>}`; icons cheese bread meat apple pelt ore gloves cloak potion tonic seeds sheaf veg ingot boots; props beacon crate barrel chest well brazier sacks; trades harbourmaster shipwright sailor | words | phase A (the props' interaction: ENGINE) |
| epilogue | an ending reached off a dialogue (goal / clock / lost) with a say is queued (Engine::epilogues_) and shown as a closing scene (storyStep) | phase A, P1b families only (the judge's "an end after a goal skips its say") |

Contracts (a lane may ADD to a file it owns, never rename, remove or change the meaning of what is listed here):
- **rpg/story/radiant.h** (JOBS lane owns radiant.cpp + rpg/story/jobs/*.cpp): spec id
  `job<RADIANT_GEN_VER>~<kind>~<flags>~<seed>`, `script(id)` (cached composition; scriptById resolves job ids), `kindCount /
  kindId / kindTitle / kindOf`, `offerFor(g, a)` (the weekly pool's radiant job for a person: a PoolJob with qtype
  QType::Story and need JN_RADIANT0 + kind), `composeFor`, `variantCount / composeVariant` (tests). A running job is an
  ordinary Instance; a finished one does not count in Engine::played_; it never uses up the giver's one story
  (MK_STORY_GIVER). Engine::start casts a job id with `offerCastKey(g, script, giver, site)` (story_internal.h, shared
  with the sagas), so radiant::offerFor try-casts (castScript with that key) exactly the people the job will begin with.
- **"ANY WORK GOING?"** (game_rpg.cpp A_ASK, phase A): `radiant::offerFor` first; a job starts through
  `Game::storyBeginJob(actor, id)` (story_game.cpp), whose opening talk stage is the pitch with its own ACCEPT / DECLINE
  (a decline closes quietly). Otherwise the old makeOffer path, unchanged.
- **The library sources** (dsl.cpp library()): `requestsVillageSource / requestsTownSource / requestsCitySource`
  (rpg/story/req/*.cpp, REQUESTS lane), `tales2Source` (rpg/story/tales2/lib_tales2.cpp, TALES lane), `p1bTestSource`
  (rpg/story/lib_p1b_test.cpp, lead: `p1b_mechanics`, every new word used once; family test).
- **The offer policy moved to rpg/story/offers.cpp** (TALES lane): Engine::offerFor / offerForPlace, their precheck,
  `offerPrecheck`, `offerGates(g, script, site, &why)` and `nearCoast`. Gates apply to OFFERS only; a forced start (tests,
  scripts) casts anywhere, so `rpg_test --story` and `--content-p1b` walk every script. **Requests are raised apart**
  (REQUESTS lane, rpg/story/req/req_offers.cpp): Engine::offerFor asks `requestOfferFor(g, a)` first (before the
  teller's tale roll: requests are need-driven) and offerForPlace asks `requestOfferForPlace`; neither offers a `family
  request` itself. Phase A's policy there: the first unplayed request of the person's trade whose gates hold, one teller
  per request per town (tellerClaim).
- **Engine additions** (story.h): `goalCanFail / debugFail`, `stageDeadline / debugDeadline`, `burdened()`, `epilogues_`,
  `choosing_`. Instance vars the runtime keys on: `_start` (the start day), `_follow_<role>`, `_hostile_<role>`,
  `_calm_<site role>`, `_heavy:<item>`, `_won_bout` / `_lost_bout` (ENGINE sets them), `_escort_far` (ENGINE: the escort
  is not with the player), `_dead_<role>` (now for persons too).
- **Marks**: story tags 13 MK_P1B_PLAYED, 14 MK_P1B_COOLDOWN, 15 MK_P1B_GOSSIP (story_internal.h); 16..23 ENGINE lane;
  160..175 JOBS lane (radiant), 176..191 REQUESTS lane, 192..199 TALES lane.
- **Text rule (owner: lines <= 38, titles <= 22 WITH the longest names)**, made mechanical in `rpg_test --content-p1b`:
  every placeholder filled with the longest name of its kind (the table in test_content_p1b.cpp, measured over seeds
  1..20: person 10, settlement 12, city 11, cave 15, ruin 18, camp 18, kingdom 12, ruler 29, god 8, people 12, ruin lord
  31, builder 27, clue 93; the table adds 1-2 and the test fails if any seed's world has a longer one); title <= 22, pitch
  and option <= 40, every say / hint / memory fits the phone panel (speech::PANEL_CHARS 70 x PANEL_LINES 5), journal <=
  150, fact <= 160, notice <= 60, item <= 32; single-line texts (gossip, a spoken bubble, a bark) <= 38; no word over 38.
- **Tests**: `rpg_test --content-p1b [--seeds 1..3 --gates --names --only <id>]` (lead's file, frozen for the lanes):
  the families, the text rule, every P1b script and every composed variant of every job kind cast at a settlement its
  `settle` allows and played through EVERY branch (options incl. hidden ones and both check outcomes, goals, lost
  escorts / bouts, clocks), no soft lock (a talk shows an option in the state a path reached it; a goal completes),
  journal text on every live stage, every ending reached, success endings pay, census effects move the census,
  epilogues queued; --gates: 30 requests (10 per source), 20 tales, 15 job kinds. In CI ("New Roads content test"; the
  integrator adds --gates). `rpg_test --quest-mix` reports the P1b items by name (`--p1b`) and `--gates-p1b` enforces:
  every job kind and every request offered over the seeds, top-3 job kinds < 53.6 % (P1), top-4 tales < 13.6 % (P1); the
  visit set can grow (`--more-towns N`, `--ports`). `--story`'s walker also takes else and deadline branches. Script
  `tools/scripts/p1b_lead_mechanics.txt` (seed 7: the test script's pitch, tracker, ambush, keeper, ending).
- **Phase A baselines** (MSVC, BDIR=build): --story 1..5 65 scripts cast, 19,385 paths, 0 failures; --content-p1b 1..3
  0 failures (p1b_mechanics: 10 paths, 7 endings a seed); --sagalib, --sagas 300 1..2, --campaigns 1..5, --voice,
  --story-p1, --dialogue 1..3 0 failures; save_test ALL OK; --quest-mix seed 1, 2 weeks: identical to before phase A (231
  story offers, top-4 13.4 %, 83 radiant jobs, top-3 54.2 %). Seen in the lead script: the tracker shows "TAKE <driver>
  TO <town>" but cuts the clock's "(6 DAYS LEFT)" at desktop width (ENGINE lane: make the clock readable).
- **Integration rules (integrator)**: radiant jobs are not offered on days 1-2; a road ambush (`do ambush` in a stage
  with an escort goal or a `do follow` on) raised inside a settlement waits until the player is outside every
  settlement, and is dropped if the stage changes first; ambushes with nobody walking along (a beast at the village
  gate) still fall where the player stands. `--quest-mix --ports` reaches 10 regions for the nearest harbour.

### 15.36 P2 "Clear Sight": big, crisp, living people (owner, 2026-10-18); the plan for P2a, P2b and P2c (planner)

Owner playtest feedback (friends): characters are too small and blurry; the dead look like they are still standing;
every weapon has the same swing. Owner decisions (final): a TALLER, DETAILED character rig with readable faces and
hands PLUS camera zoom; whole-number scaling with no fractional blur; outlines and contrast; a player ring. The
per-weapon swings and the death fall are painted on the new rig, once. A male and a female body base (the P3 creator
picks; `app.female` is never set today, so every hero is crowned KING). There is no section 13 entry: this section is
the spec. The art direction is **docs/STYLE_P2.md** (binding on every P2 art area). Each batch ships on its own and
leaves the game fully playable with one rig visible to players.

**Dependency (wave 0).** A test-tooling change is in flight (Witness: rpg/main.cpp, rpg/sim/travel.cpp,
rpg/view/terrain.cpp, the particles in rpg/view/render.cpp, rpg/view/screen.cpp, engine/pix.*, engine/witness*,
rpg/view/witness_state.*, view.h). It is committed first; P2a phase A branches from that commit and nobody edits those
files before it lands.

**Measured today** (the 2026-10-17 survey and a code read on 2026-10-18):
| fact | value | where |
|---|---|---|
| canvas and fit | 480x270 design; the height stays 270..312; FILL takes an integer scale only while the safe height is 312 or less, else a fractional scale with SDL's PIXELART filter; one scale for world and UI, drawn straight to the window | screen.cpp:51-52, 101-149; pix.cpp:62-81 |
| scales | desktop 1440x810 480x270 @3; iPhone 15 Pro Home Screen app 639x295 @4; Safari 721x287 @3.54; iPhone 11 615x284 @2.91; SE 480x270 @2.78; iPad 480x333 @4.92 | EMB_SCREEN_LOG |
| people | one 16x24 cell, 8 frames x 3 rows (left = the right row mirrored); figure about 13x22 incl. outline; head 8x8 of skin, eyes 1x2 each, no brows or mouth | art_human.h:13; art_human.cpp:61-93 |
| on screen | figure 8.1 % of the screen height (desktop), 7.5 % (phone app), 5.7 % (laptop) | survey |
| frames | walk 1-4 at 9 fps; windup and cast share 5; strike and recover share 6; hurt and dead share 7 | render.cpp:2992-3000 |
| the dead | the hurt frame, 3 px lower, faded 4-6 s, erased at 6 s | render.cpp:2978, 3075; game.cpp:726 |
| swings | every melee weapon uses the same raise and strike in the down and up views; one 24x24 silver arc for all, timed at stT / 0.11 whatever the family; the greatsword is drawn as a sword (art id 1) | art_human.cpp:3394-3533; render.cpp:3257-3267; gear_look.cpp:257 |
| timing | the hero: windup 0.07 (0.05 unarmed), strike 0.11, recover 0.13 s, each x the family pace (sword 1.0, axe 1.08, mace 1.25, spear 1.1, dagger 0.8, fists 0.9); enemy humans wind up 0.30 s whatever they hold | game.cpp:834-845; skills.cpp:186-193; ai.cpp:518 |
| speeds | the hero 74 px/s; an NPC 40 by default; bandit aggro 130 px | player.cpp:45; game.h:83; game.cpp:1763 |
| world | tile 16 px; house door 16x10 (13x8 small, 20x12 temple, 22x12 grand, 26x16 palace); one-storey wall 24 px; bed 20x32 on a 2-tile footprint; seat line row 19 of 24; ridden horse 36x36; ladder 16x24; stair flight 16x40 | art_building.cpp:86, 2231-2235; art_life.h; art_home.h:90; art_props.h |
| creatures | wolf 32x22, bear 36x28, troll 36x34, yeti 38x38, golem 40x50, dragon 80x56; skeleton, draugr and goblin painted by the human rig at 24x24 | art_monsters.cpp:2807-2859 |
| outlines | selective (the neighbour darkened, 45 % ink); torso luma 131 vs ground 129 by day, 52 vs 61 at night | art_internal.h:248-270; survey |
| budgets | phone (web): textures 64 MB or less, render 6 ms or less; caches 320 human sheets, 160 pose sheets | 12.3; render.cpp:725-726 |

**The design.** Two layers. The WORLD layer is drawn at a whole number D of device px per world px, chosen per
screen, so every world pixel is an exact D x D block on every device; the UI layer keeps today's fit and size (480x270
safe boxes, every tap script unchanged). People move to rig v2: 32 px tall (2 tiles; a woman 31), a 12 px head with a
real face (brows, 1x2 eyes, nose, mouth), 2x2 hands, four facings painted natively (the light stays top-left, the
weapon stays in the right hand), an ink outer outline; every weapon class has its own three-hit swing and its own
effect; every human falls and lies. What a body uses (doors, walls, seats, beds, mounts, boats, carts, ladders,
gates) scales with the rig by k = 32 / 22 = 1.45; footprints in tiles never change, so world generation, saves and
pathing are untouched. **The compatibility path**: v2 and the k-scaled holders are built behind `rig 2` (preview:
EMB_RIG=2, web `?rig=2`, script `rig 2`) while v1 stays the shipped default through P2a; P2b flips every human at once
when coverage reaches 100 %, and v1 is deleted. No shipped default ever shows both rigs. (Re-planned 2026-10-18:
the v2 people are now the doll's hand-placed A3 part maps, and the flip moves to P2c; see "The art-direction
change" below.)

**Not in scope**: the creator's flow and layout (P3: it sets `app.female` and picks the body; P2 provides the bodies);
the world-spanning campaign; collision radii and any balance change beyond the DPS-neutral items named below; a new
UI design (the UI keeps its size and layout).

**Constants that interact**
| constant | value | arithmetic |
|---|---|---|
| tile, footprints | 16 px, unchanged | layouts, collision and saves untouched |
| adult figure | 32 px (woman 31, elf +1, youth 26, child 21) incl. the outline | 32 / 22 = 1.45 = k for body-holders; head 12 = face 10 x 9 |
| default world scale | D0 = round(h_device / 240), ties up | v2 figure share 32 D0 / h = 11.6-14.8 % on the 10 reference screens (STYLE_P2 §1) |
| zoom bounds | V = h / D in [150, 300] world rows | FAR never draws more than about today's phone canvas (295 rows): render cost never rises |
| aggro vs view | bandit aggro 130 px; half the view at NORMAL 108-138 px | foes can notice the hero from off the view: pips are required |
| walk cycle | 6 frames over a 28 px cycle (5/4/5 px a stance), keyed to the whole px drawn along the step's main axis | 74 px/s = 15.9 fps (about 11 on a 45-degree diagonal), 40 px/s = 8.6 fps; side views register the planted foot within 1 px at each frame change (restated 2026-10-18, main session) |
| hero swing | 0.07 / 0.11 / 0.13 s x pace over 2 / 2 / 2 frames | the windup is 4 sim frames: a family reads in its strike, follow-through and effect |
| enemy windups | 0.22-0.38 s by family | spawn-weighted mean held at 0.30 +- 0.01 s (no balance change) |
| fall | 0.34 s (0.10 + 0.12 + 0.12) | gate 0.45 s |
| corpses | 60 s, at most 24 | 24 DEATH atlases x 48 KB = 1.2 MB |
| people textures | 24 MB or less of the phone's 64 MB | 80 folk x LOCO 128 KB = 10 MB + 12 fighters x ACTION 512 KB = 6 MB + 40 postures x 64 KB = 2.5 MB + deaths 1.2 MB = 20 MB |
| bake | folk entering the 34-tile stream radius bake 2 ms a frame (1.2 ms web), nearest first | 544 px of stream radius vs at most about 320 px of half-view: folk are baked before they are seen |
| ring | 24 x 9 under a 16 px-shouldered figure | flares 1 s when hit or entering a crowd (4 within 3 tiles) |

(P2b and P2c re-plan, 2026-10-18: the walk cycle now comes from A3's drawings, man 19 px and woman 20 px;
people above 54 px/s run on a cycle of about 36 px; a RUN atlas of 64 KB joins the budgets. "Constants for the doll"
below has the arithmetic.)

**The split**
- **P2a "Lens and Rig"** ships the lens: whole-number world scale on every screen, ZOOM, the player ring, the ink outer
  outline on today's v1 people and creatures, the night lift, off-screen pips, per-family effects and timing, per-family
  enemy windups. It builds (judged, behind `rig 2`) the v2 body, faces, hands, hair, cloth, headwear, locomotion, the
  fall and lying corpse, every swing family, every posture, children and the humanoid monsters, and the k-scaled
  body-holders. Players see today's people, crisp and easy to find; the owner can preview the new people on the phone
  with `?rig=2`.
- **P2b "New People"** (re-planned 2026-10-18 after the owner's art-direction change, below): the hand-drawn folk. The
  doll (hand-placed part maps composed in layers and recoloured by ramps) replaces the procedural rig behind `rig 2`; the
  A3 style slice is signed off by the owner first; then every civilian look of the census and the creator is drawn and
  covered: bodies, head types, hair, beards, garments fitted to man and woman, headwear, cloaks, postures, riders, rowers,
  sleepers, deaths, kids and elders. Players still see v1.
- **P2c "Arms and Armour"**: armour fitted to man and woman, helms, shields, the weapon library, every action family and
  its effect, the humanoid monsters, every remaining draw path on the doll; coverage 100 %, then the flip: the doll is the
  only rig, and v1 and the procedural rig are deleted.
- **P2d "Proportion"** (was P2c): the creatures and the leftovers: big monsters and village animals re-proportioned to the
  32 px people, plus every prop the P2c judge lists as reading wrong.

#### P2a "Lens and Rig"

Phase A (lead, serial; afterwards these are frozen for the lanes, who may ADD but never rename or change meaning):
1. **Layers** (engine/pix.h): `Pix::Layer {w, h, scale, offX, offY}`, `presentLayers(world, ui)`, `beginWorld()` /
   `endWorld()` (inside: Pix::W/H read the world view in world px, the insets are 0, the render scale is the integer D,
   `setTarget(nullptr)` returns to the world layer), `worldLayer()`, `uiLayer()`, `readWorld(rgba, w, h)` (the world
   layer at world px). Phase A stub: world = UI, nothing moves.
2. **Screen** (rpg/view/screen.h): `Settings::zoom` (-1..2, an offset from D0) and `Settings::ring`; settings file
   EMBS version 2 (a version-1 file loads with the defaults); `worldScale()`, `defaultWorldScale()`, `zoomLevels()`;
   `describe()` adds the world ("WORLD 512 X 236 AT 5X"); test hooks EMB_ZOOM (an absolute D) and EMB_RIG.
3. **Projection** (view.h): `View::toUi(Vec2 world)`, `View::toWorld(Vec2 ui)`, `View::uiPerWorld()`; the list of
   world-anchored call sites (hud.cpp speech plates, combat numbers, name tags and E: TALK, door and quest labels;
   party_hud.cpp banter; the elite name plates' text; buildmode.cpp taps; script_test.cpp label taps).
4. **Rig contract** (new rpg/art/art_rig.h): `rigVersion()` / `setRigVersion()`, `bodyScale()` (1, or 32 / 22 at rig 2),
   `RigMetrics rig(v)` (figure height by body, age and people; head top, eye row, hand row, marker, bubble, plate and
   bar heights; ring and shadow sizes; reach per family; the bathing and swimming crop rows), `enum class Clip` (Idle,
   Walk, Hurt, Windup, Strike, Follow, Cast, Draw, Release, Block, Roll, Fall, Lying), `enum class Family` (Unarmed,
   Dagger, Sword, Greatsword, Axe, Mace, Spear, Staff, Bow), `familyOf(look)`, `FrameRect {x, y, w, h, px, py}` (pivot =
   the point between the feet on the sole row), `RigSheet {atlas, frames, frame(facing, clip, variant, i)}`,
   `humanRig(look, part)` for parts LOCO / ACTION / DEATH, `humanPosture2`, `sleeper2`, `rig2Covers(look)` (stub
   false). HumanLook gains `body` (0 man, 1 woman) and `age` (0 adult, 1 child, 2 youth), mixed into key(); weapon art
   id 7 = greatsword (gear_look's map becomes {1, 2, 6, 5, 7, 8}; v1 paints 7 as 1). looks.cpp sets body from the
   census `female`, gear_look.cpp from `app.female` (v1 ignores both: no v1 pixel moves). `humanFigureStill` (statues)
   and `humanoidCell` with the three humanoid monsters' cell sizes route by rigVersion() to v2 painters (RIG-BODY and
   POSES fill them). `View::rigTex(look, part, child)` returns the baked atlas and its frames (LENS implements the
   caches and the bake queue; render_anim.cpp draws from it).
5. **Effects** (art_fx.h, appended): SwingArc, HeavyArc, ChopArc, SmashRing, StabGlint, ThrustStreak, BluntSwish,
   PunchStar, Twang (phase A stubs copy Slash).
6. **Animation pick** (new rpg/sim/anim.h/.cpp, pure, testable headless): `animPick(game, actor)` returns {clip,
   family, variant, frame, frames, t}; phase A reproduces today's v1 columns exactly at rig 1.
7. **Moves, pixel-identical at rig 1**: the human frame pick, body draw, slash effect and dead offset leave render.cpp
   for new rpg/view/render_anim.cpp; the story hand-cart model (render.cpp hc*) leaves for new rpg/art/art_handcart.cpp.
8. **bodyScale plumbing** (identity at 1): door and wall sizes in art_building.cpp, furniture sizes in art_props.cpp,
   fits in art_life.cpp, the horse cell in art_home.cpp, deck fits in art_sea.cpp read `art::bodyScale()`; crude at
   k (enough for the slice's cottage), refined by HOLDERS; golden_village_art identical at rig 1.
9. **The ink outline policy** (STYLE_P2 §8) for every character sheet that exists today (v1 people, monsters, critters,
   horses, the hound) through one `outlineInk()` helper in art_internal.h. The only intended pixel change of phase A;
   art_hash re-recorded.
10. **Test hooks**: Witness reset params `window [w, h]`, `safe [l, t, r, b]` (device px), `screen` fill / pixel,
    `zoom`, `rig` 1 / 2, `female`; state additions `state.screen {window, ui {w, h, scale, integer}, world {w, h,
    scale, integer, d0, levels}, zoom, ring, v1Draws, peopleMB}`, `state.player.sprite {rig, figH, box (device px),
    pivot}`, `state.player.anim {clip, family, variant, frame, frames}`, `entities[].anim`, `entities[].corpse {age}`;
    calls and script commands `zoom`, `rig`, `body`, `pose <clip> [family] [variant] [frame]` (freeze the hero's
    frame), `hideplayer`, `ui 0|1`, `ring`, with `expect screen | zoom | rig | anim | corpse` checks; keys minus and
    equals in the hello list; **tools/p2_measure.mjs** (Node 18,
    no dependencies: PNG decode, the figure mask from a shot pair with and without the hero, its box and height, the
    D x D block test, outline-ring, ground-ring and figure luma, the colour-subset check).
11. **Harness** (the lead's, frozen; a lane reports a harness problem instead of editing it): `rpg_test --rig`,
    `--anim`, `--rig-coverage` (tools/tests/test_rig.cpp, test_anim.cpp), with rpg_art linked into rpg_test (the
    #include hack in test_hero / test_arms retired) or a separate rig_test tool if SDL in CI costs too much; CMake
    globs rpg/art/rig/*.cpp; tools/scripts/p2a_lead_lens.txt and _phone; witness/p2a_lens.witness.json; one gallery
    skeleton per art area (tools/preview/rig_gallery.cpp, rigarms_gallery.cpp, rigposes_gallery.cpp,
    holders_gallery.cpp), each handed to its area.
12. **Baselines** recorded in PLAN.md: per reference screen the UI fit and figure share; `--perf` render ms in the
    capital at 15:00; v1 sheet bake ms p50 / p95 (EMB_TIMING); people texture MB; art_hash.

Areas (one owner each; files disjoint; "done" refers to the gates below):
- **LENS** (engineer-high). Files: engine/pix.cpp; rpg/view/screen.cpp, settings.cpp, render.cpp, hud.cpp,
  render_markers.cpp, party_hud.cpp, home_view.cpp, party_view.cpp, buildmode.cpp, script_test.cpp, terrain.cpp
  (culling only); rpg/main.cpp; web/index.html. Brief: the two layers and everything that rides on them.
  - core: the world layer at integer D on every screen (the camera rounded once a frame, the world rect covering the
    window behind the notch, the light map, weather and vignette inside it); ZOOM by the D0 rule and the V bounds,
    kept in the settings file, - and = keys and the wheel in play; every text draw in the UI layer through toUi, taps
    through toWorld; the hero's ring; the night lift (each visible human added again in Moonwash after the light map);
    off-screen pips; the NPC sleep box from the world view (main.cpp sleepHalfW/H); rig metrics in every world draw
    path (markers, bubbles, bars, plates, the hidden-hero silhouette, bathing and swimming crops, riders, companions);
    the `rig 2` preview of riders and companions; `View::rigTex` with people caches capped in MB and a bake queue for
    folk entering the stream radius (2 ms a frame, 1.2 ms on the web, nearest first); web `?rig=2`.
  - depth: a two-finger pinch on touch (both fingers clear of the stick and buttons); the camera eases 0.15 s on a zoom
    change so the hero stays put; sub-pixel camera smoothing through a world target, kept only if the judge's motion
    check shows no sprite wobble.
  - delight: the ring flares when the hero is hit or walks into a crowd; pips pulse faster as the foe closes; SETTINGS
    shows the world fit beside the UI fit.
  - done: L1-L9.
- **ANIM** (engineer). Files: rpg/sim/anim.cpp, rpg/view/render_anim.cpp, rpg/sim/game.cpp, rpg/sim/ai.cpp,
  rpg/sim/skills.cpp. Brief: when each frame and effect shows, and what the sim does with the dead.
  - core: animPick for every human state on v2 per STYLE_P2 §10 (walk by distance, idle phases per actor, windup /
    strike / follow split by the family's pace, the combo variant from Actor::combo, cast, bow, fall, lying), v1
    untouched; render_anim.cpp draws a v1 cell or, at rig 2 where rig2Covers(look), the v2 atlas frame by its pivot
    (never flipped); effects by family through the new Fx ids, progress = stT / (0.11 x pace), the finisher's tint and
    1.2x;
    enemy windups by family (dagger 0.22, unarmed 0.24, sword 0.28, spear and staff 0.30, axe 0.32, mace 0.36,
    greatsword 0.38 s) scaled so the spawn-weighted mean stays 0.30 +- 0.01 s; at rig 2 the dead stay 60 s (fade
    55-60 s), block nothing, at most 24 (the oldest fade first), and at rig 1 they leave at 6 s as today; the hero's
    fall plays under the death screen on view time.
  - depth: the greatsword's own pace 1.18 with damage x 1.18 (DPS unchanged; --power-curve, --skills and --arms stay in
    their bands); a parry pose if the sword's timed parry already exists in the sim; the roll clip.
  - delight: the impact flash and a one-frame camera nudge on heavy finishers (greatsword, mace); a dust puff where a
    body lands.
  - done: A1-A5.
- **RIG-BODY** (artist-xhigh). Files: new rpg/art/rig/rig_core.cpp, rig_body.cpp, rig_face.cpp, rig_hair.cpp,
  rig_cloth.cpp, rig_death.cpp; tools/preview/rig_gallery.cpp. Brief: the skeleton and everything a civilian is;
  the STYLE SLICE first (section "The style slice" below).
  - core: the skeleton and pose solver (per-frame anchors: head box, neck, shoulders, elbows, hands with grip angle,
    hips, knees, feet, the back slot; the depth order; four native facings), the atlas packer with pivots, the v2
    outline; man and woman x slim / average / broad x human / half-breed / elf; skins S1-S8; the faces and hands of
    STYLE_P2 §4-5; the 8 hair styles and the beard; Tunic, Dress, Robe (hood optional), Rags, the 8 culture cuts and the
    patterns; the 8 culture headwear kinds and the legacy hood; face paint and jewellery; Idle 3, Walk 6, Hurt; Fall 3
    and Lying 2.
  - depth: secondary motion (a ponytail or braid swings on walk frames, hems lag one frame); elves' finer features.
  - delight: the blink; the hair's sheen moves with the head.
  - done: R1-R6.
- **RIG-ARMS** (artist-xhigh; starts after the slice passes). Files: new rpg/art/rig/rig_arms.cpp, rig_actions.cpp;
  rpg/art/art_fx.cpp; tools/preview/rigarms_gallery.cpp. Brief: the swings, their weapons and their effects
  (STYLE_P2 §11-12).
  - core: the 10 base weapons rasterized at 16 angles with the grip and depth rules (the greatsword its own art id 7);
    the action library: unarmed, dagger, sword, greatsword, axe, mace, spear, staff x 3 variants x (Windup 2, Strike 2,
    Follow 2) x 4 facings; Cast 2; the bow's Draw 3 and Release 1 in every facing; the 9 effects and the finisher look.
  - depth: Block 1 and Roll 4; the glaive and halberd sweeps as the spear's hit 3 (base forms; culture forms in P2b).
  - delight: the dagger's glint, sparks when a mace meets metal, chips from the axe.
  - done: R7-R10.
- **POSES** (artist; starts after the slice passes). Files: new rpg/art/rig/rig_poses.cpp, rig_sleep.cpp, rig_kin.cpp,
  rig_humanoid.cpp; rpg/art/art_life.cpp (the v2 tables only; the v1 tables stay byte-identical);
  tools/preview/rigposes_gallery.cpp. Brief: what town folk do with their bodies, on v2.
  - core: the 31 postures with today's bard and kneel variants and frame counts, four facings (Ride on the new saddle
    line); sleeper sprites for the 10 berths; youth and child bodies; skeleton, draugr and goblin on the skeleton in the
    monster sheet layout; SeatFit2 and BedFit2 tables agreed with HOLDERS.
  - depth: every culture instrument variant; carry loads (sack, basket, crate).
  - delight: drinkers out of step; a sleeper's chest rising; children hopping at tag.
  - done: P1-P3.
- **HOLDERS** (artist; starts after the slice passes). Files: rpg/art/art_building.cpp, art_props.cpp,
  art_culture_furniture.cpp, art_market.cpp, art_home.cpp, art_sea.cpp, art_parts_gates.cpp, art_walls.cpp (gate
  passages only), art_city.cpp (fences, hedges, garden walls, square benches), art_critters.cpp (the loose horse only),
  art_party.cpp (graves only), art_handcart.cpp; tools/preview/holders_gallery.cpp. Brief: everything a body uses, at
  k, behind `rig 2`, pixel-identical at rig 1 (STYLE_P2 §1 and §16).
  - core: doors and walls of every building kind and culture (windows, porches and arcades follow the wall); chairs,
    stools, benches, thrones, cushions, tables, counters, the classic and culture beds, bunks, hammocks, mats and
    bedrolls; stall counters and canopies; carts and the hand cart; the ridden horse (horseSheet, riderSeat) and the
    loose horse to match; boat thwarts, decks, rails and crew spots; ladders and stair flights; gate passages and
    arches; fences and hedges; graves.
  - depth: the well's windlass, the anvil and forge at working height, bath pools' waterline, festival poles.
  - delight: a stable door that reads big enough for a rider.
  - done: H1-H4.

Waves: phase A -> **wave 1**: LENS, ANIM, RIG-BODY (the slice first) -> **the slice gate** (judged; STYLE_P2 then
records its measured numbers) -> **wave 2**: RIG-ARMS, POSES, HOLDERS start; RIG-BODY completes the base; LENS and ANIM
finish -> integration (lead: one full regression, one script sweep, art_hash once, the web build, the owner's `?rig=2`
link) -> judge rounds (at most 4).

P2a DONE gates (before / after with the phase A harness; MSVC, BDIR=build):
| gate | target | today | P2a integration, 2026-10-18 (measured) |
|---|---|---|---|
| L1 world scale whole-number on the 10 reference screens (STYLE_P2 §1) | 10 / 10 | 6 / 10 |
 10 / 10 (Witness p2a_lens 15 / 15, lane 2 / 2): PASS |
| L2 noon device shots, `ui 0`: world pixels in exact D x D blocks (p2_measure) | 100 % | a 3.54x crop had 938 colours vs 353 at 4x |
 100 % on all 52 `ui 0` shots (desktop D3 / D4 / D5, phone D4-D7, day and night): PASS |
| L3 D0 and the zoom levels per the table; ZOOM survives a restart; - / = and the wheel step it | exact | no zoom |
 exact on the 10 screens; ZOOM survives a reset; - / = and the wheel step it: PASS |
| L4 UI canvas and scale per screen; every existing tap script | identical to the phase A baseline | |
 UI fit unchanged on the 10 screens; 359 / 359 scripts exit 0 (view_settings taps re-aimed at the 7-row panel): PASS |
| L5 text glyph height at every zoom (speech plate, name tag, combat number) | 7 x U device px | |
 21 device px (7 x 3) at NORMAL, NEAR and CLOSE (1440x810): PASS |
| L6 day edge contrast (grass, road, sand, snow); at 23:00 figure median vs ground, and edge contrast, for the hero and an unlit villager | 40 or more; \|figure - ground\| 10 or more or edge 40 or more (restated 2026-10-18; was +10 or more and 25 or more) | torso vs ground +2 by day, -9 at night |
 day 85-155: PASS; 23:00 figure / edge: grass +21.9 / 41, road +15.6 / 41, sand +8.2 / 56.5, snow -12.0 / 74.5 (phone alike): FAIL on sand and snow (a look call: more lift pales torch-lit folk). RESTATED 2026-10-18 (fix round 1, L6-NIGHT-BRIGHT-GROUND; STYLE_P2 section 8): day edge 40 or more; night (23:00, 5 tiles or more from any lamp) for the hero (p2a_night) AND an unlit villager 8 tiles from the hero (p2a_night_npc, `npcstage` / `npcpair`), 4 grounds x D3 / D5: \|figure - ground\| 10 or more OR edge 40 or more (an unlit figure's ink, about 10 luma, against unlit ground at 18-30 can never make an edge of 25). Fix round 1 measured (MSVC): hero D3 / D5 grass +21.2 / +21.6 (edge 43.1 / 41.7), road +13.3 / +13.8, sand +2.5 / +7.5 with edge 64.8 / 56.4, snow -17.2 / -11.6: PASS 8 / 8; unlit villager (D3 = D5) grass +14.4, road +11.9, sand +7.1 (edge 16.7), snow +2.8 (edge 20.1): FAIL on sand and snow. Fix (LENS, render.cpp drawNightLift): the lift's floor away from the hero 0.4 -> 0.6; emulated (EMB_LIFTK / EMB_LIFTF x 1.5) the villager reads grass +23.9, road +21.4, sand +16.6, snow +12.3, not ghostly; the torch-lit hero keeps full strength |
| L7 ring contrast against the ground on the 4 grounds, day and night; PLAYER RING toggles | 35 or more, 8 / 8 | no ring |
 8 / 8 (lowest 39.0, desktop and phone); toggles: PASS (the capital crowd shot reads 16: the ring half hidden by a passer-by on red brick) |
| L8 a pip for every aggro'd hostile outside the view within its aggro radius | every one | none |
 a pip for each of the 2 hunters out of view inside their radius; none for the one in view or the one beyond its radius: PASS (the top pip sits under the region banner while it shows) |
| L9 capital 15:00: render ms at FAR / NORMAL; people textures with rig 2; folk baking per frame | baseline +10 % / baseline or less; 24 MB or less; 2 ms or less | |
 NORMAL 0.56 ms (baseline 0.49), FAR 0.70 ms (limit 0.66): FAIL, RULING 2026-10-18 (main session): accepted, FAR may be up to 0.75 ms (0.04 ms is far inside the frame budget); NORMAL must stay within baseline +15 %; people 3.4 MB rig 1, 8.5 MB rig 2: PASS; bake queue capped at 2 ms (1.2 ms web); v1 sheet bake p50 0.270 / p95 0.349 ms |
| A1 v1 frame pick at rig 1 (--anim golden) | identical | |
 identical (21,630 samples, 56fb08be96b73a1a): PASS |
| A2 9 distinct effects by family; the effect ends with the strike state within 1 frame (dagger 0.8, mace 1.25) | yes | 1 effect, fixed 0.11 s |
 9 effects painted by family (fx+new 061fd76c895fbd6d); the effect ends 0.7 (dagger), 0.8 (mace), 0.4 (sword) frames from the strike: PASS |
| A3 enemy windup mean (spawn-weighted); --foes, --arms, the companion suites | 0.30 +- 0.01 s; green | 0.30 for all |
 0.359 s over 182 bandits (phase A re-based the target to 0.36 +- 0.01); --foes, --arms, --companion green: PASS |
| A4 rig 2: lying on screen after the killing blow; corpse life; cap; collision | 0.45 s or less; 60 s; 24; none | standing, 6 s |
 lying 0.35 s; gone at 60.0 s; 24 with 30 killed; walked through: PASS |
| A5 the hero's fall under the death screen | plays | none |
 plays, lying at 0.35 s: PASS |
| R1 the style slice | PASS | |
 FAIL on SLICE-WALK-SLIDE only (the planner's restatement of the 1 px slide; the code meets the proposed wording). Fix round 1 (2026-10-18): point 8 restated here, in STYLE_P2 section 10 and section 19 (registration 1 px or less at every frame change; slip one frame step, 4.5 px, or less within a held frame). walk60 on the MSVC build: registration 1 px at worst (motion_reg.py); slip 4 px at worst, contact and any frame, 8 occluded held frames skipped (motion.py); hero pivot changed on 0 of 119 frames; camera steps {-2: 28, -1: 91}: meets the restated bar |
| R2 v2 heights (STYLE_P2 §3) for every body, build, people and age, all facings | exact | |
 216 checks, 0 failed: PASS |
| R3 looks of a 20-seed census with no armour, helm, cloak, shield or culture weapon form, drawn on v2 (rig2Covers) | 99 % or more | 0 |
 461 of 461 (100 %), seeds 1..20: PASS |
| R4 brows, two 1x2 eyes, nose and mouth on every down frame but blink, hurt and lying; two hands in front and back idle and walk | all | eyes only |
 faces 8 / 8, hands 18 / 18: PASS |
| R5 no holes, nothing touching the scratch edge, pivots steady on contact frames | 0 failures | |
 880 checks, 5 failed (the bow's Draw frames: 5-29 px enclosed between string and stave): FAIL (the lead's call). RULING 2026-10-18 (main session): the gap between a drawn bow's stave and string is correct art, not a hole; exempt the bow Draw/Release frames' string-stave enclosure from R5 in the check (keepOpen stays true) |
| R6 atlas size per look: LOCO / DEATH / POSTURE / ACTION; one facing's LOCO bake | 128 / 48 / 64 / 512 KB or less; v1 sheet p95 or less | |
 90 checks, 0 failed; LOCO bake p50 0.09 ms vs v1 p95 0.10: PASS |
| R7 Strike frames of every pair of the 8 melee families differ in 25 % or more of their union, per facing | 28 x 4 | 0 |
 224 checks, 0 failed: PASS |
| R8 tip paths (STYLE_P2 §12) | every family | |
 9 families, 0 failed: PASS |
| R9 a family's 3 combo variants differ pairwise (25 % or more) | all | |
 96 checks, 0 failed: PASS |
| R10 left frames painted, not mirrored; the weapon stays in the right hand | all clips | mirrored |
 344 checks, 0 failed: PASS |
| P1 31 postures with their variants, 4 facings, today's frame counts, on v2 | all | |
 120 checks, 0 failed: PASS |
| P2 seated hips on the seat (0 px) for every seat and facing it takes; sleepers' heads on the pillow in every berth | all | |
 26 checks, 0 failed: PASS |
| P3 youth 26 and child 21 painted natively; skeleton, draugr, goblin on v2 | yes | rows dropped from the adult |
 5 checks, 0 failed: PASS |
| H1 rig 1: golden_village_art and the v1 art_hash categories | identical | |
 golden_village_art 65 groups, 0 changed; v1 art_hash TOTAL 317eef6efe4d8229: PASS |
| H2 rig 2 ratios from painter metadata: door 0.70-0.76, wall 1.00-1.12, seat 0.31-0.37, table 0.44-0.50, horse withers 0.80-0.90, gate clearance 1.25 or more, fence 0.45-0.60, rungs 6 px | all in range | |
 23 checks, 6 failed (gate, arch and city-gate clearance and the hedge not measured; rowboat gunwale and deck rail 0.06): FAIL |
| H3 rig 2: the rider on every horse frame, the rower on the thwart, the walker under every arch | 0 px off | |
 not met: riders still on the v1 horse (home_view / party_view), gate passages 22-24 px: FAIL |
| H4 rig 2: nothing jammed in village, town, city and port streets (the judge's look review) | PASS | |
 the judge's look review (not run by the integrator) |

#### The art-direction change (owner, 2026-10-18): hand-placed people

After P2a shipped (6b40fb2), the owner rejected the procedural rig's look: "they look bad, they just aren't sharp or
clear" (proportions, shading, faces). The reference wanted is Sea of Stars and Chained Echoes. An art study produced
hand-placed pixel mockups. The owner picked take A (32 px, chunky proportions that "fit nice with what we have"),
rejected the slimmer A2 ("the skinny legs look bad"), and APPROVED **A3**: A's exact silhouette with better rendering
inside (eye whites, iris and glint, brows, mouth, hue-shifted 5-step ramps, cloth, leather and metal kept apart, lit-edge
coloured outlines, left facings repainted for the top-left light, zero orphan pixels, secondary motion). The approved
reference is locked in docs/ref/style_a3/ (the maps, sheets, GIFs, notes, the 64 frames at 1x, palettes.json), and
docs/STYLE_P2.md is rewritten for it, with what is superseded listed at its top.

Owner requirements for the people (2026-10-18), binding on P2b and P2c:
1. **Head types**: distinct face and head shapes, not one head recoloured (round, long or square jaws, different noses
   and brows, age marks for the old, elf and half-breed features), all inside A3's silhouette rules.
2. **Hair**: more than the current 8 styles, for men and women (short, cropped, shaved or bald, spiky, curly, long and
   loose, ponytail, braid, twin braids, bun, mohawk-ish), beards and moustaches of several kinds; each in 4 facings with
   walk secondary motion where it has hanging parts.
3. **Clothing and armour fitted to both bodies**, not one shared shape (tunics, dresses, robes, work clothes, culture
   cuts; light, medium and heavy armour: padded, leather, mail, plate; helmets and hoods), covering every gear look
   gear_look.cpp and looks.cpp produce, so equipping armour visibly changes the hero, man or woman.
4. "Nothing crappy or half baked": A3's quality on every part.

Consequences: P2b and P2c are re-planned around a new production method (the doll, below); the old P2c creature work
moves to P2d; P2a's lens, animation timing, holders and bake machinery stay.

#### What P2a keeps, adapts and retires

**Kept as built**: the lens (layers, integer D, ZOOM, toUi / toWorld, the ring, the night lift and its lift masks, pips,
text in the UI layer); bodyScale / bodyPx and every k-scaled holder; the view's atlas cache and bake queue (View::rigTex,
rigPoseTex, rigBakeQueue with its 2 ms / 1.2 ms budget, trimRigCaches and its MB cap, liftMaskFor); the night lift's
BlitRec recording; the animation pick (AnimPick, Clip, Family, variants) and its timing (windup, strike and recover by
pace, enemy windups by family, the greatsword's pace, the effects' progress, corpse life, cap and fade, the hero's fall
under the death screen); art_rig.h's public types (FrameRect, RigAnchors, RigSheet, RigPart, RigMetrics, Holder and
holderPx, the seatFit2 and bedFit2 signatures); weapon art id 7; the Witness hooks and checks P2a added; tools/
p2_measure.mjs; the p2a scripts; `--anim` and its golden; holders_gallery.

**Adapted**:
- art_rig.cpp's dispatch (humanRig, humanPosture2, sleeper2, humanFigureStill2, humanoidCell2) calls the doll instead of
  rpg/art/rig; rig2Covers becomes the doll's resolver (dollCovers per look and part).
- RigMetrics r2 takes A3's rows (crown F-31 / F-30, eyes, hands); RigAnchors adds `topY` (the frame's top, so markers
  and plates clear hair and hats).
- The walk table (kWalkAt, kWalkCycle 28) becomes the clip tables' plant anchors (man 19 px, woman 20 px), plus the run.
- SheetBuilder (trim, pivot, dedupe, pack) moves from rig_core.cpp into the doll as its atlas packer; seatFit2 and bedFit2
  move from rig_poses.cpp and rig_sleep.cpp into the doll with their numbers unchanged.
- P2a's pose tables (rig_actions.cpp's families x variants x keys, rig_poses.cpp's postures, rig_death.cpp's fall) are
  exported once as number drafts for the clip tables; the weapon rasterizer (rig_arms.cpp) and the 9 effect painters
  (art_fx.cpp) are exported once as pixel drafts for the weapon and arc maps.
- The `--rig` gates (R2, R4-R10, P1-P3) are restated for A3 and pointed at the doll's atlases; R3 becomes the doll's
  coverage census; rig_gallery, rigarms_gallery and rigposes_gallery give way to doll_gallery.

**Retired**: the procedural painters rig_core.cpp (skeleton, pose solver, limb painter), rig_body.cpp, rig_face.cpp,
rig_hair.cpp, rig_cloth.cpp, rig_death.cpp, rig_poses.cpp, rig_sleep.cpp, rig_kin.cpp, rig_humanoid.cpp, rig_arms.cpp,
rig_actions.cpp and rig_internal.h. P2b phase A detaches them from `rig 2` (the game never draws them again); they stay
compiled only for the draft tool until the P2c flip deletes them with v1. The procedural effects in art_fx.cpp follow
at the flip.

#### The production method: the doll

**What it is.** A person is composed at bake time from hand-drawn part maps in layers (a paper doll), recoloured by
ramps: legs, arms, torso garment, hang (what falls below the waist), head (head type, nose, brows, eyes, ears, marks),
beard, hair, headwear or helm, neckwear and belt items, cloak, held item, shield. Rigid parts are drawn once per facing
and moved by the clip's anchors; only legs, arms and hangs are drawn per key. Every colour comes from a ramp, so one
drawing serves every skin, hair colour, culture cloth and metal. STYLE_P2 section 24 is the artists' rulebook.

**The authoring format.** `.pxm` text files under rpg/art/doll/maps/: one character per pixel, `.` keeps what is below
and `-` erases it. One part per file, with its views (D, U, R, L), keys, anchor, layer, origin and legend; `when`
overlays for optional regions; build masks; the stretch row. The legends (rpg/art/doll/maps/legend/) map each character to
a slot or region and a step (STYLE_P2 section 6), keeping A3's hero characters for their slots. Clip tables are text too
(rpg/art/doll/clips/*.clip): per body set, clip, facing and frame, the leg, arm, hang, hair and tuft keys, bob, lean, head
offset, expression, weapon angle and depth, the effect's frame, and the plant anchor (walk and run).

**Compiled and embedded.** `node tools/doll/pxc.mjs` (Node 18, no dependencies, like tools/p2_measure.mjs) validates
every map (legend, views, keys, STYLE_P2 section 24's craft checks) and writes one generated source,
rpg/art/doll/gen/doll_pool.cpp: a packed byte pool (one byte per pixel, slot or region x step; transparent runs
compressed) and a directory of parts, views, keys and clip tables, in arrays of 64 KB or less (MSVC's limits). The
generated file is committed, so building needs no Node; CI runs `pxc.mjs --check` and fails when it is stale. Nothing is
read from disk at run time (the web build has no asset loading). **Size**: about 3,400 maps holding about 310,000 opaque
pixels in boxes of about 620,000 cells: about 0.5 MB in the binary (budget 1.0 MB) and about 0.2 MB more over the wire
gzipped for the web (budget: embervale.wasm grows by 1.2 MB or less); about 2 MB of generated source in one chunked
translation unit.

**Bake and atlas.** `doll::compose(look, part)` fills a RigSheet as P2a's painters did. For each frame of the clip table,
each layer in the facing's order is placed at its anchor (plus the view's origin, the bob, lean and breathe), its pixels
mapped through the legend to the look's ramps: regions resolved by sleeve, legwear, gloves and boots; optional regions by
ornaments and dials; build masks; the elf stretch row; derived sways; erases applied. Then the finishing pass (A3's
selout) runs over the composite, and P2a's SheetBuilder trims, pivots, records the RigAnchors (head, face, hands, hips,
feet, grip, tip, top), dedupes and packs. Lying: the side view composed, turned a quarter, mirrored for head west and
finished again. The view's cache, bake queue, MB cap and lift masks take the atlas unchanged. Parts: LOCO (idle, walk,
hurt; sheathed), RUN (a new RigPart, baked for movers above 54 px/s), ACTION (the drawn idle, walk and run, the family's
swings, cast, bow, block, roll), DEATH, POSTURE (per posture), the sleeper canvas, the statue figure.

**The switch (rig 1 / rig 2).** Through P2b and P2c the shipped default stays rig 1 (v1). `rig 2` (EMB_RIG=2, web
`?rig=2`, script `rig 2`, Witness reset `rig: 2`) draws the doll for every look whose needed parts it covers (LOCO and
DEATH; ACTION for anyone who fights; POSTURE while posing) and v1 whole otherwise; from P2b phase A it never draws the
rejected procedural rig. The k-scaled holders stay bound to rig 2. **The flip** is P2c's: when the coverage census reads
100 % and the judge passes the gear gates, rig 2 becomes the only rig, `rig 1` is removed, v1 (art_human.cpp's sheets,
humanSheet, humanPostureSheet) and rpg/art/rig/* are deleted, and art_hash and golden_village_art are re-recorded once
with before / after pairs. No shipped default ever shows two kinds of people.

**Coverage** (`rpg_test --doll-coverage [--seeds A..B] [--scope civil|all] [--gates]`): the resolver names every field
value of a look that no authored part covers (field = value, part, body, facing, key). The census of looks has five
sources: (A) the world: every human of the seeds' starting regions after 60 frames (P2a's R3 loop) and every census
resident of the start region's settlements through makeLook; (B) cultures x roles: census::dress for every culture of the
seeds x every Role x both sexes x child, youth, adult and elder x 8 person seeds; (C) gear: every item gear.cpp can roll
for each maker culture (kind x material x rarity x form; bands 1-7, bronze, each alloy), worn through wearGear by a man
and by a woman; (D) the creator: every hair, beard, build, people, skin and sex through appearanceLook, with and without
each homeland; (E) the special: party members, story people, the humanoid monsters, statues. The census also reports dead
parts (authored maps no look reaches): the gate wants 0.

**The look fields** (phase A; derived, never saved; v1 ignores them; mixed into key() only at rig 2, so rig 1's keys,
caches and goldens stay identical until the flip): `head` (head type), `nose`, `brow`, `marks` (freckles, scar, beauty
mark, weathered, lined, gaunt), `hairVar` (the style within the census's hair family), `beardStyle`, `neck` (scarf,
kerchief, collar); `age` gains 3 = elder, and youth (2) comes into use for census children aged 10-14. looks.cpp fills
them from a new hash stream at the end of census::dress, after every existing draw, so no other look moves; gear_look.cpp
fills the hero's from a hash of the Appearance until P3's creator saves real choices.

**Constants for the doll** (they interact):
| constant | value | arithmetic |
|---|---|---|
| body box | man 32, woman 31 (hair up to 2 more); youth 26, child 21; elf +1 | A3 measured; P2a's holders were sized for 32 (k = 32 / 22), so every ratio holds |
| walk cycle | 18 px on both bodies, 6 frames (the owner's walk fix, 2026-10-19; first planned as man 19 px, woman 20 px) | redrawn with the idle's leg mass, flat boots, a 0 / +1 / 0 bob; at 40 px/s 13.3 fps (2.2 cycles a second); gated by `--doll --gates` leg mass, not A3 parity |
| run cycle | 36 px, 6 frames (signed off above 54 px/s) | about 10 px of stance + 8 px of flight a half-cycle; at 74 px/s 2.06 cycles a second and 12.3 fps (A3's approved walk GIF: 2.6) |
| gait switch | 54 px/s | about the geometric mean of 40 and 74: the walk there would be 2.8 cycles a second (its top), the run 1.5 (its bottom) |
| A3's sword swing | windup 70, strike 110, follow-through 70, recover 60 ms | the hero's 0.07 / 0.11 / 0.13 s at the sword's pace 1.0; other families x their pace |
| atlases per look | LOCO 128, RUN 64, ACTION 512, DEATH 48, POSTURE 64 KB | the capital at 15:00: 80 folk x 100 + 12 fighters x 300 + 10 runners x 50 + 40 postures x 50 + 24 dead x 40 KB = about 15 MB of the phone's 24 MB |
| bake | one facing's LOCO 0.1 ms or less; the queue 2 ms a frame (1.2 web) | a look's LOCO about 0.4 ms: about 5 people a frame entering the 34-tile stream radius, before they are seen |
| pool | about 0.5 MB (budget 1.0 MB) | about 3,400 maps, 620,000 cells, runs compressed |
| colours | 48 or fewer per figure | A3: hero 40, woman 37 |

#### The variety solution

| axis | how the doll covers it | count |
|---|---|---|
| man and woman | two adult body sets; garments and armour fitted to each; the legs shared in trousers (A3's leg rows are the same) with painted overrides; shoe tips under long skirts | 2 |
| builds | average (A3), slim, broad: build masks in torsos, hangs and pauldrons; the arms placed at the shoulders | 3 |
| human, half-breed, elf | 3 ear sets; head types (elves 3 of their own, half-breeds all 9); the elf stretch row and one build slimmer | 3 |
| adult, youth, child, elder | four body sets (adult man, adult woman, youth, child); the youth on the adult head box; elders as the adult body with the stoop, age marks and grey ramps | 4 + elder |
| skins | curated A3 ramps: the culture's 12 steps and the creator's 8; elf tints | 20 |
| head types | man 9 and woman 9 (6 human + 3 elven each), the youth the soft 4 of its sex, the child 4; noses 5 (man) / 4 (woman), brows 4, ears 3, marks 6, expressions 6 | 180 man faces and 144 woman faces before marks, age and eye colour |
| hair | 24 adult styles (20 + 4 depth) shared by man, woman and youth; 10 child styles; hang keys on the 8 hanging styles, tuft keys on 3, capped variants for volume | 34 |
| beards and moustaches | stubble, short boxed, full, long, braided, goatee, moustache, Van Dyke, mutton chops (+ handlebar) | 9 |
| clothes, cuts and cultures | civilian garments: man 13, woman 14, youth 8, child 7, the 8 culture cuts among them; apron, tabard, sash and baldric overlays; 5 neckwear; 7 patterns as tiles, edges and motifs; culture colours by ramp | 42 |
| headwear | 8 culture kinds + the legacy and cloak hoods, shared by adults; 4 for children | 14 |
| armour (P2c) | 8 body forms x man and woman = 16 fitted torso sets; 7 bands, bronze, alloys, sheens and Mythic tempering by ramp; 9 pauldron parts; 2 armour hang families; gloves and boots by region; 10 ornaments, rank marks and glints as optional regions | 16 |
| helms (P2c) | on the adult head box, with crest, plume and horsetail regions and face and hair masks | 12 |
| cloaks | travel (hooded), fur mantle, long noble, cape | 4 |
| shields (P2c) | round, heater, kite, tower, crescent, oval, buckler, leaf; field, device and rim regions | 8 |
| weapons and things in hand | 10 base weapons x 16 angles; 8 blade, 2 polearm and 5 bow forms; ledger, scroll, horn; 22 props and the culture instruments; 2 back items | |
| postures | 31 + Row: 6 leg modes (stand, chair, floor, kneel, ride, ground) x posture arm keys x props | 32 |
| riders and rowers | Ride on HOLDERS' k-scaled horse (riderSeat); Row on the rowboat's thwart, synced to the oars | |
| deaths and corpses | recoil, buckle, topple; lying composed and turned; gear lies with the body | |
| motion | 4 facings; walk 6 + run 6, idle 3, hurt, 9 families x 3 variants, cast, bow, block, roll, fall 3 x 2 sides, lying 2 | |

**How the census picks.** makeLook's existing draws are untouched (their order is a contract). The new stream picks a head
type by people and sex, culture-weighted (Highland favours Craggy and Square, the elven peoples their own three); a nose
and brows; marks (freckles more among the fair, scars among fighters, age lines from 60); the hair variant within the
drawn family (Bald: Bald, Shaved; Short: Short, Cropped, Tousled, Spiky, Receding; Long: Long loose, Long wavy, Shag, Bob;
Ponytail: Ponytail, High ponytail, Pigtails; Mohawk: Mohawk, Topknot; Braids: Braid, Twin braids, Locs, Crown braid; Bun:
Bun, Top bun; Curls: Curly, Big curls) by archetype weights (Fjordfolk braids and braided beards, Steppe topknots and
shaved heads, Jade top buns, Sylvan long loose hair and crown braids); the beard's style when `beard` is set; neckwear for
commoners. Roles map onto garments through looks.cpp's role cases (smith and shipwright the work apron, farmer the smock,
hunter the jerkin).

#### The authoring volume, and how it is cut

| category | P2b maps | P2c maps | what |
|---|---|---|---|
| legs | 213 | 32 | adults shared 81 + the woman's overrides 30 + shoe tips 16; youth 45, child 41; P2c the action stances |
| arms | 494 | 292 | adult LOCO, fall, elder and posture keys 274; youth 110, child 110; P2c action arms 184 and drawn-weapon LOCO arms 108 |
| torsos and overlays | 236 | 120 | civilian man 52, woman 56, youth 32, child 28; overlays, neckwear and belt items 68; P2c armour 64, pauldrons 36, sheaths 20 |
| hangs | 312 | 96 | 4 families x 24 keys per adult body, 3 x 20 per kid body; P2c 2 armour families x 24 keys x 2 bodies |
| heads | 249 | 20 | adult head types, noses, brows, ears, expressions, age, marks and paint 193; child 56; P2c draugr and goblin heads |
| hair and beards | 277 | | adult 176 (styles, hangs, tufts, capped, lying), child 64, beards 37 |
| headwear and helms | 61 | 60 | culture headwear and hoods; P2c 12 helms and their lying views |
| cloaks | 64 | | 4 styles x 16 keys |
| held items and props | 115 | 399 | civilian items at rest, the slice's sword, 22 props and the instruments; P2c weapons at 16 angles with forms and back items |
| shields | | 80 | 8 shapes x 9 views, 8 devices |
| effects | 8 | 122 | A3's arc; P2c 8 families x 16 maps + the twang |
| humanoid monsters | | 152 | the skeleton body set and its gear |
| **total** | **about 2,030** | **about 1,370** | **about 3,400 maps (maps = parts x facings x keys), about 310,000 hand-placed pixels; 60 % of the maps under 60 px** |

The cuts that make it affordable, with what each saves against a layer system without it:
1. **Rigid parts per facing**: torsos, heads, hair, headwear and cloaks drawn once per facing and moved by the clip's
   anchors (bob, lean, breathe, stoop), never per frame: about 15,000 maps (10 LOCO + 65 posture + 155 action frames a
   facing).
2. **One adult head box**: hair, headwear, helms, beards, ears and marks shared by the man, the woman and the youth (A3's
   heads are the same 12 rows; the woman's sits one row lower): about 450.
3. **One adult arm library with regions** (upper arm, forearm, cuff, hand, gauntlet flare) for both bodies and every
   sleeve: about 500.
4. **Legs with regions** (thigh, knee, shin, boot): trousers, bare knees, tall boots and greaves recolour one set of leg
   drawings, shared by man and woman in trousers: about 900.
5. **Hang families** shared by garments of one silhouette, the front and back sways derived by row shifts: about 400.
6. **Ramps for every colour**: skin, hair, cloth, leather, the metal bands and alloys never need a new map.
7. **Optional regions** for ornaments, dials, sheaths and glints inside one map: about 500.
8. **Build masks and the elf stretch row** instead of build and people variants: about 600.
9. **Lying by turning the composed side view** (sleepers alike), painted lying views only where the judge asks: about 400.
10. **Drafts**: left views from mirror-and-relight, weapons and arcs from P2a's rasterizers, clip tables from P2a's pose
    numbers; the blocking-in is free and every committed map is still hand-placed and hand-checked.
11. **Overlays at fixed face anchors** for expressions, marks and paint (every head type keeps A3's eye and mouth rows).
Without the doll, whole frames for even one look per role (40 roles x 2 bodies x about 1,100 frames) would be 88,000
frames; with it, the whole range of the census, the creator and the gear system is about 3,400 maps.

#### P2b "New People": the hand-drawn folk

Goal: every civilian of the census, the creator's hero and the A3 hero with his sword drawn by the doll at `rig 2`,
measured, judged and signed off. Players still see v1 (crisp, ringed, lifted at night). Armour, helms, shields and the
action families are P2c's.

**Dependency (wave 0).** Another agent is changing game balance and quest files (ai.cpp, foes_game.cpp, the animation
tuning in anim.cpp, quest tracking). P2b phase A branches after that work is committed; no P2b area edits those files
before it lands, and only FOLK-SIM edits anim.cpp after it.

Phase A (lead, serial; frozen afterwards: areas ADD, never rename or change meaning):
1. **The reference**: docs/ref/style_a3 (copied 2026-10-18 by the planner, with the frames at 1x); `tools/doll/
   from_a3.py` writes `.pxm` drafts of A3's named parts (hero.py's D_LEGS, R_LEGS, L_LEGS, R_TORSO, R_ARM, ARM_FWD /
   ARM_BACK, U_ARM, TAIL_R / TAIL_L, FAR_HAND_L, R_HILT and the scabbards; woman.py's shoes, arm patches and braid swings;
   swing.py's overlays and arcs).
2. **The doll contract** (new rpg/art/doll/doll.h): the slots, regions and legends; the layer table; the anchors;
   `DollRecipe resolve(const HumanLook&)`, `dollCovers(look, part)`, `dollMissing(look)`, `compose(look, part)` ->
   RigSheet; `walkTable(bodySet, gait)` (the frame starts from the plant anchors); the clip-table schema; `rampA3(base,
   material)` and the curated tables. art_rig.cpp's dispatch goes to the doll; the procedural painters are detached.
3. **The tools**: tools/doll/pxc.mjs (`--check`; `--report` with the pool's size and every map's craft checks),
   tools/doll/draft.mjs (mirror-and-relight), tools/preview/rigdraft.cpp (P2a's pose numbers and the weapon and arc drafts
   for P2c); CMake globs rpg/art/doll/*.cpp and gen/*.cpp.
4. **Look fields** (art_human.h): head, nose, brow, marks, hairVar, beardStyle, neck; age 3; mixed into key() only at rig
   2. RigPart::Run; RigAnchors::topY; Posture::Row (appended); RigMetrics r2 to A3.
5. **Test hooks** (rpg/view/script_p2b.cpp, witness_state.cpp): `camhold 1|0` (freeze the camera, so `witness measure
   motion` sees a body cross still ground), `lineup <archetype> <role> [n] [female|male|mixed] [age]` (a row of census
   people), `dollpart <part> <view> [key]` (one part on the hero for review); state `player.sprite.doll` and
   `entities[].doll` (covered, the parts missing); `expect doll covered|missing`.
6. **Harness** (the lead's, frozen; an area reports a harness problem instead of editing it): `rpg_test --doll [--gates]
   [--parity] [--report]` (tools/tests/test_doll.cpp: parity with docs/ref/style_a3/frames, heights, faces, hands, craft
   checks, pivots, atlas KB, bake ms, the ramp calibration, the man / woman fit; P2c adds families, tips, left frames and
   the armour change), `--doll-coverage`, `--doll-variety` (per settlement: distinct head recipes, the largest shared
   group, styles seen); `--rig` restated for A3; tools/preview/doll_gallery.cpp; scripts p2b_slice, p2b_folk, p2b_poses,
   p2b_walk, p2b_holders (each with `_phone`); witness/p2b_doll.witness.json; the playtest goals (tools/scripts/
   p2b_playtests.txt).
7. **Baselines** in PLAN.md: coverage (0 %); people MB and render ms in the capital at 15:00 at rig 1 and rig 2; bake ms.

Areas (one owner each; files disjoint; maps are folders under rpg/art/doll/maps/; "done" refers to the gates below):
- **DOLL** (engineer-high). Files: rpg/art/doll/*.cpp and *.h (not the maps), rpg/art/doll/gen/*, tools/doll/*,
  rpg/art/art_rig.cpp, CMakeLists.txt (the doll lines), tools/preview/doll_gallery.cpp. Brief: the compiler, the pool,
  the compositor, the resolver and coverage, the ramp generator, the atlas, the RUN part, bake speed.
  - core: every format feature of STYLE_P2 section 24; the finishing pass identical to A3's selout (parity proves it); the
    turned lying figure and the sleepers; regions, optional regions, build masks, stretch rows, derived sways; the
    resolver naming every missing part; the generator within delta E 6 of A3's 14 ramps; the atlas budgets.
  - depth: an incremental compile (changed maps only) and a `--watch` that re-renders doll_gallery on save.
  - delight: the gallery's onion-skin view of a walk (the frames at 30 % over each other), so registration shows at once.
  - done: B1 (parity), B6, B8, B10.
- **FOLK-BODY** (artist-xhigh). Wave 1: **the style slice** (STYLE_P2 section 19; it owns every map it makes until the
  slice gate, then hands each folder to its wave-2 area). Wave 2 files: maps/body/adult/**, maps/legs/adult/loco/**,
  maps/arms/adult/loco/**, maps/held/weapons/sword* and maps/belt/sheaths/** (the A3 sword and scabbard; to WEAPONS in
  P2c), clips/loco.clip, clips/death.clip. Brief: the adult bodies: idle, the walk (6 drawings a side), the run, hurt,
  fall and lying for both bodies and both sides, the elder's stoop, shoe tips, the woman's leg overrides.
  - core: A3 parity; registration; the run; deaths.
  - depth: secondary motion of every hanging style and garment checked in motion with HAIR and the GARB areas.
  - delight: a dust-puff frame where a running foot lands.
  - done: B1, B2, B5, B11 (deaths).
- **FACES** (artist-xhigh). Files: maps/head/adult/**. Brief: the 9 + 9 head types, noses, brows, the three ear sets,
  expressions, marks, age, face paint, earrings (STYLE_P2 section 4).
  - core: every type in D, R and L (U by jaw class); every expression fits every type.
  - depth: elven fineness (thinner lashes, upswept brows) that reads at phone size.
  - delight: the talk mouth while a speech plate shows; a wince that differs by type.
  - done: B7 (faces), B8.
- **HAIR** (artist). Files: maps/hair/adult/**, maps/beard/**. Brief: 24 adult styles with hangs, tufts, capped variants
  and lying views where needed; the 9 beards (STYLE_P2 section 21).
  - core: 4 facings each; hangs swing in the walk and the run; capped under every headwear.
  - depth: the crown's sheen dipping on the walk's down frames.
  - delight: twin braids swinging out of phase.
  - done: B7 (hair), B8.
- **GARB-M** (artist-xhigh). Files: maps/wear/man/**. Brief: the man's 13 civilian garments: torsos with build masks,
  sleeve styles, hangs in every key, the overlays' man cuts, the patterns on each.
  - core: fitted to the man in every key; depth: the robe's cuff flare; delight: a coat's skirt kicking out in the run.
  - done: B7 (garments), B8, B9.
- **GARB-W** (artist-xhigh). Files: maps/wear/woman/**, maps/neck/**, maps/belt/items/**. Brief: the woman's 14 (A3's
  dress first) and the overlays' woman cuts; neckwear (A3's scarf first); belt items.
  - core: fitted to the woman in every key; depth: A3's apron bow from behind; delight: the scarf's tail in side views.
  - done: B7 (garments), B8, B9.
- **OUTER** (artist). Files: maps/headwear/cloth/**, maps/cloak/**. Brief: the culture headwear and the hoods with their
  hair masks; the 4 cloaks with sway, run flare, sitting and riding drapes.
  - done: B7 (headwear), B8.
- **POSES-B** (artist). Files: maps/legs/adult/pose/**, maps/arms/adult/pose/**, clips/postures.clip. Brief: the 31
  postures and Row on the adult bodies; riders on HOLDERS' horse, rowers on the thwart, sleepers in the 10 berths.
  - core: P2a's P1 / P2 restated (hips on seats 0 px, heads on pillows); every frame count and pace.
  - depth: carried loads that sway; drinkers out of step.
  - delight: a sleeper's chest rising.
  - done: B11.
- **PROPS** (artist). Files: maps/held/props/**, maps/held/civil/** (the stick, staff, carried bow, hammer, ledger,
  scroll and horn at rest). Brief: everything a civilian holds, in its own ramps, the hand over the grip; the culture
  instruments.
  - done: B8, B11.
- **KIN-YOUTH** (artist). Files: maps/kin/youth/**, clips/kin_youth.clip. Brief: the youth body (26 px) in every LOCO,
  DEATH and posture key; its 8 garments and hangs; on the adult head box.
  - done: B2, B7, B8.
- **KIN-CHILD** (artist). Files: maps/kin/child/**, clips/kin_child.clip. Brief: the child body (21 px): legs, arms,
  7 garments and hangs, the 4 head types, 10 hair styles, child headwear, the kids' postures (play and tag, carry, sit,
  eat, wave).
  - delight: children hopping at tag.
  - done: B2, B7, B8.
- **HOLDERS-B** (artist). Files: rpg/art/art_parts_gates.cpp, art_walls.cpp (gate passages), art_sea.cpp (gunwale and
  rail), art_props.cpp (holderPx). Brief: P2a's open H2 / H3: gate, arch and city-gate clearance 1.25 of the body or more
  (22-24 px today), the hedge measured, the rowboat's gunwale and the deck rail in their ranges.
  - done: B12.
- **FOLK-SIM** (engineer). Files: rpg/sim/looks.cpp, gear_look.cpp, anim.cpp (after wave 0), life_game.cpp (the spawn's
  age). Brief: the new look fields from the census and the hero; elders and youths from census ages; the role garments;
  the walk and run tables from walkTable; the gait switch at 54 px/s; the sheathed / drawn pick (Idle and Walk variant 0
  sheathed, 1 drawn: drawn for 4 s after a swing or while a hostile is within its aggro radius); the elder's walk.
  - done: B5, B7.
- **VIEW-B** (engineer). Files: rpg/view/render_anim.cpp, render.cpp, home_view.cpp, party_view.cpp, render_sea.cpp (the
  rower only). Brief: the doll's frames on every civilian draw path at rig 2 (per-part coverage, v1 whole otherwise); the
  RUN part in the bake queue; markers and plates on RigAnchors::topY; riders on the doll on HOLDERS' saddle (P2a H3); Row
  synced to the oars; the posture path at rig 2; the hidden-hero silhouette from a doll frame; the contact shadow under
  corpses.
  - done: B5, B10, B11.

Waves: wave 0 (the balance and quest work lands) -> phase A -> **wave 1**: FOLK-BODY (the slice), DOLL, FOLK-SIM (the
look fields and census picks; anim.cpp after wave 0) -> **the slice gate** (measured, judged, then the owner's sign-off
from the owner pack; no production map is drawn before it) -> **wave 2**: FACES, HAIR, GARB-M, GARB-W, OUTER, POSES-B,
PROPS, KIN-YOUTH, KIN-CHILD, HOLDERS-B and VIEW-B start; FOLK-BODY completes the adult bodies; DOLL and FOLK-SIM finish ->
integration (lead: one full regression, one script sweep, `pxc.mjs --check`, art_hash once (the doll's new categories;
v1's unchanged), the web build and its size, the owner's `?rig=2` link) -> judge rounds (at most 4).

**The slice gate** (all must hold; each failure comes back with a repro and a check):
1. Parity: with A3's palettes, the doll's frames equal docs/ref/style_a3/frames with 0 px differing for the 8 idles and
   the 8 swing keys (16 frames; `rpg_test --doll --parity`). **Amended 2026-10-19 (the owner's walk fix, signed off):**
   the walk and run frames, side, front and back, were redrawn at the owner's request (chunky legs, flat boots never
   rotated, clean knees, a 0 / +1 / 0 hip bob, both bodies on an 18 px walk cycle, the run 36 px) and are no longer A3's;
   they are reported beside A3 for review and GATED by the leg-mass / hip-row / stance checks of `rpg_test --doll
   --gates` (in test_doll.cpp "B11 leg mass": legs 90 % or more of the idle's area, the hip row at most 1 px narrower than
   the idle's, no leg run under 3 px, the hips within 1 px of the contact frame, a down frame no wider than its contact,
   the woman also in trousers and bare legs), not by A3 parity. (Originally: the 24 front and back walk frames and the
   8 side contact frames at 0 px too, the 16 re-registered side frames reviewed.)
2. Size (`witness measure box` on `shotpair` shots, `ui 0`): the body box 32 (man) and 31 (woman) art px, the hero 34 with
   his cowlick: 102 / 93 device px at D3, 136 / 124 at D4, 170 / 155 at D5; the child 21; the elder by STYLE_P2 section 3.
3. Crisp (`witness measure scale --k D`): every D x D cell one colour in the 11:00 `ui 0` shots at D3, D4 and D5.
4. Contrast (tools/p2_measure.mjs contrast; `witness measure contrast --a diff:<without> --b ring:2` for the record): the
   day edge 40 or more on grass and road; 22:00 per STYLE_P2 section 8; the ring 35 or more.
5. Motion (`witness measure motion --json` on `camhold` runs at 60 Hz: a villager walking across at 40 px/s; the hero
   walking at 40 and running at 74; side, front and back views): the planted foot within 1 px at every side frame change
   (the lowest row's span against the centroid), slip one frame step or less, front and back walking in place, the pivot
   steady (2 or fewer changes in 119 frames).
6. Coverage: every slice look resolves; no dead part among the slice's maps.
7. Craft: STYLE_P2 section 24's checks at 0 failures on every slice map and composed frame; the ramp generator within delta
   E 6 per step of A3's ramps.
8. The look review (the judge, on the owner pack): A3 held on every new part; the top-left light in all four facings; mail
   and the helm read as armour on both bodies; the lineup shows 12 different people; the hero found first.
9. **The owner's sign-off** on the owner pack (STYLE_P2 section 19), which also settles the run above 54 px/s and sheathed
   weapons out of combat.
After the sign-off STYLE_P2 records the slice's measured numbers, and every later part matches the slice.

P2b DONE gates (before / after with the phase A harness; MSVC, BDIR=build; Witness at rig 2):
| gate | target | measured (P2b integration, 2026-10-19; MSVC BDIR=build, WSL clang 18) |
|---|---|---|
| B1 the slice gate | PASS, signed off | PASS: `--doll --gates` parity 16 / 16 frames at 0 px; B11 leg mass 192 / 0 (the owner-signed walk) |
| B2 heights (`--doll --gates` for every body set, people, age and build in every facing; `witness measure box` on the lineups) | exact, by STYLE_P2 section 3 | PASS: 288 / 0 (every body set, people, age, build, facing). P3 kin in `--rig --gates` fails "age 2: 27 px (26)": its default tousled hair adds the cowlick (harness). Witness shot-pair boxes (D3 = D5; idle incl. the breathe frame +1): hero 34 front, man 35 / woman 32 side, youth boy 29 (26 + cowlick), youth girl 27, child 21-22, elder 30 |
| B3 crisp: every `ui 0` world shot at rig 2 (`witness measure scale`) | 100 % whole cells | PASS: 181 `ui 0` world shots, 0 mixed cells (D3 141, D4 1, D5 28, D6 1; 11:00 and 22:00-23:00) |
| B4 contrast on the four grounds by day and at night for a villager, an elder, a child and the hero (p2_measure, L6's rule) | all | PASS (marginal): day edges hero 72-135, villager 40.4-147, elder 58-141, child 69-130 (on a patch off the verge: the first `gotoground road` straddles grass, 32.6); 23:00 \|figure - ground\| hero 5.3-25.4 (sand: edge 38.8), villager 9.5-20.4 (snow 9.5 once, re-measured 10.5 / 10.6 / 10.5), elder 17.8-27.7, child 28.6-37.7 |
| B5 motion: walk and run registration for the man, woman, youth, child and elder (`witness measure motion` on camhold runs); the gait switch at 54 px/s; the sheathed / drawn pick | 1 px and one frame step; switch and pick as specified | FAIL (1 item): the planted foot (sole overlap on 30-frame shot-pair bursts, camhold, side views E and W): man, woman, youth boy, child boy and girl, elder walks 0 px (youth boy once 1), runs 0-1 px (flight frames skipped); **youth girl walk 2 px** at two frame changes each way (KIN-YOUTH). Gait run 74 / 58 px/s, walk 50, back under 52; drawn 4.15 s after a swing (`--folk-sim` 31 / 31; Witness B5 targets pass) |
| B6 coverage: `--doll-coverage --scope civil --seeds 1..20 --gates` over sources A (residents through Game::dressAsResident), B, D and E (no armour, helm, shield or culture weapon form); dead parts over every recipe the game draws on the doll at rig 2 (those sources plus roles, towns, every archetype's musicians, companions and gear, each in its postures with the prop in hand); `--report` names the sources and lists P2c's parts apart (armour roles, and parts wanted only by looks held back by armour or the action families) | 100 %, 0 dead parts | FAIL: 57,606 / 57,606 looks covered (100 %), 69 dead parts: 11 outside the civil scope (mail, nasal helm, the A3 sword, sheath and arc), 9 reached only by gear, roles or royals (4 cloaks and fronts, legacyhood), 38 props and civil held items (the harness counts neither withPosture recipes nor makeLook roles), 12 never rolled in seeds 1..20 (3 adult and 3 child face paints, 4 dots tiles, 2 amulets) |
| B7 variety (`--doll-variety --seeds 1..20`): no head recipe (type, nose, brows, hair style and colour, beard, headwear) shared by more than 2 folk of a village or 3 % of a town or city; 18 or more of the 24 hair styles and all 9 beards seen; every head type seen on each body; every civilian garment seen on each body that wears it; peoples told apart by their lineups in 10 seconds (the judge) | all | PASS (harness): 24 / 0; 23 of 24 styles, 10 beards, heads man 9 woman 9, garments man 13 / 13, woman 14 / 14; the 10-second read is the judge's |
| B8 craft: STYLE_P2 section 24's checks on every map and on the composed frames of the census's looks | 0 failures | PASS: 1120 / 0; pxc 403 parts, 0 errors, 2 craft warnings, `--check` up to date |
| B9 the man / woman fit: every civilian garment a fitted pair of maps, the woman's per STYLE_P2 section 14 (the judge's lineup review) | PASS | PASS (structure): 101 / 0; the lineup review is the judge's |
| B10 atlas and speed: LOCO 128, RUN 64, DEATH 48, POSTURE 64 KB per look or less; one facing's LOCO bake p95 at or under the v1 sheet's (0.385 ms); people 24 MB or less in the capital at 15:00 at rig 2; render ms at NORMAL within baseline + 15 %; the pool 1.0 MB or less; embervale.wasm + 1.2 MB or less | all | FAIL (1 item): harness 92 / 0; LOCO bake p95 0.126-0.171 ms (v1 0.104-0.129); people 10.3 MB at rig 2 (rig 1 3.7); draw at NORMAL rig 2 0.30 ms vs rig 1 0.29-0.30 (+3 %); pool 0.68 MB; wasm about +0.85 MB (estimate: pool 0.68 + the doll's code; the deployed part-1 wasm is 15.15 MB); EMBERVALE_WITNESS=OFF build links. **An armed look's RUN atlas is about 152 KB** (the drawn run; the gate samples unarmed looks) |
| B11 postures, riders, rowers, sleepers and deaths: P1 / P2 restated on the doll; the rider on every horse frame and the rower on the thwart (0 px); lying 0.45 s or less after the blow, 16 px tall or less, head toward the fall side | all | PASS: P1 124 / 0, P2 26 / 0; Witness: the fall and lying target passes, the rider target passes after the integration fix; Row and sleepers by the VIEW-B / POSES-B scripts (0 failures); the rider is one Ride frame placed per horse frame (not measured per frame) |
| B12 holders (P2a H2 / H3 closed): every ratio in range (chair, stool and bench seats restated P2b fix r1 to 0.14-0.20, A3's knee, STYLE_P2 1; hips on seats 0 px); gate passages 1.25 of the body or more | all | FAIL: H2 20 / 23: chair, stool and bench seats 5 px = 0.16 (0.31-0.37; the slice lowered kSeat to A3's knee: the planner restates); gate 1.28, arch 1.31, city gate 1.28, hedge 0.53, gunwale 0.41, rail 0.50 |
| B13 playtests (below): every goal met by 2 of 3 testers; every note with a shot triaged (fixed or ruled) | PASS | FAIL: T1 smith and innkeeper 3 / 3 (by place, not by look), an elder 1 / 3; T2 two peoples 1 / 3 (the others found only Vontve towns); T3 0 / 3 (no player sit action, the rent missed through the testers' choose, no horse or boat from a fresh start; the bandit corpse lies 20 s in p2a_death). Every note triaged in PLAN.md P2b integration |

**Playtests** (`witness play`, headless; Sonnet at low effort as the testers, briefed by `witness play brief`; the phone
window and `rig: 2` in the reset params; two sessions at a time): T1 newbie, "find the smith, the innkeeper and an elder
in the start village and talk to each" (the roles must read from their looks; the tester checks with `witness play
shot`); T2 explorer, "visit towns of two different peoples and say how their people dress"; T3 breaker, "sit in the
tavern, sleep in a bed, ride a horse, take a rowboat out, and watch a bandit fall". Each tester notes anything that looks
broken (floating, missing, flickering, on the wrong layer) with a shot; the judge triages every note.
**Restated at P2b fix round 1** (the goals as written could not be met: no player sit action, a horse or boat needs a
homestead or a purchase, census elders are rare, and 639 x 295 UI-canvas shots drew people about 20 px). The goals are
tools/scripts/p2b_playtests.txt, run with `witness play batch --config witness.play.json` (the phone window at rig 2,
god, `play.allowCalls`, and EMB_WPLAY_SHOT world:3: a tester's shot is the world layer at 3 x 3 per art px, 1536 x 708,
people 96 px): T1 "find the smith, the innkeeper and a child in OSLARS, describe each one's look from a shot, then talk"
(an old person is a bonus shot); T2 "OSLARS' folk, then `call gotoculture other town` (the fast-travel stand-in) and a
crowd there: how each people dresses"; T3 the five body states through named setup calls (`seatall` sitters; renting
and sleeping, then `seatall sleep` sleepers at 23:00; `home lot / fill / ride`; `sea port`, `boat give rowboat / board`;
`gotosite other` and a bandit's fall). Each goal met by 2 of 3; every note with a shot triaged in PLAN.md.

#### P2c "Arms and Armour": steel on the new folk, and the flip

(Re-planned 2026-10-23 on the shipped doll and motion method: docs/PLAN.md "## P2c" is the plan, with docs/STYLE_P2.md
25-26 and docs/P2C_CONTRACTS.md; where they differ from the text below, they win. The humanoid monsters moved to P2d.)

Goal: every fighter on the doll (armour fitted to man and woman, helms, shields, every weapon and form, every action
family and its effect, the humanoid monsters), every remaining draw path on the doll, coverage 100 %, and the flip: rig 2
the only rig, v1 and the procedural rig deleted.

Phase A (lead): the armour contracts (form, band, dial, ornament and glint regions; the armour hang families; the gauntlet
and greave regions); the helm and shield anchors and masks; the weapon library's directory (art id x form x 16 angles,
grip and tip anchors); the action clip schema (families x variants x facings, cast, bow, block, roll and the drawn LOCO);
the effects as maps (kFxInfo's sizes kept); the drafts exported by rigdraft (P2a's weapons, effects and pose numbers);
`--doll-coverage --scope all` adds source C (every gear roll); `--doll --gates` adds the families (R7-R10 restated), the
weapon library's craft and the armour change; scripts p2c_gear (guards of three peoples, soldiers of two realms, bandits
by family, the hero and a woman knight in every band), p2c_swings (each family live and frozen with `pose`), p2c_menus,
p2c_world (each with `_phone`); witness/p2c_switch.witness.json (v1Draws 0, coverage, menus); baselines.

Areas (maps under rpg/art/doll/maps/):
- **ARMOUR-M** (artist-xhigh). Files: maps/armour/man/**, maps/armour/shared/** (pauldrons, the tabard over armour, rank
  marks). Brief: the 8 forms fitted to the man in 4 facings, their sleeve and leg regions, the armour hangs in every key,
  ornaments and glints as optional regions. Depth: mail that glints per frame. Delight: plate catching the lit edge as the
  body turns. Done: C1, C3, C4.
- **ARMOUR-W** (artist-xhigh). Files: maps/armour/woman/**. Brief: the same 8 forms fitted to the woman (STYLE_P2 section
  14: shaped by a step-1 row, never sculpted). Done: C1, C3, C4.
- **HELMS** (artist). Files: maps/headwear/helm/**. Brief: 12 helms with crest, plume and horsetail regions, face and hair
  masks, lying views where turning fails. Depth: hair under open helms by style. Delight: plumes that bob on the walk.
  Done: C1, C3.
- **WEAPONS** (artist-xhigh). Files: maps/held/weapons/**, maps/belt/sheaths/**, maps/back/**. Brief: the 10 base weapons
  at 16 angles, the 8 blade, 2 polearm and 5 bow forms, the crossbow's crank, a sheath for every sheathed kind, quivers and
  staffs on the back, legendary and Mythic glints. Delight: arrows visible in the quiver. Done: C1, C6.
- **SHIELDS-FX** (artist). Files: maps/shield/**, maps/fx/**. Brief: the 8 shields (at rest, raised, flat), field, device
  and rim regions; the 9 family effects re-cut to A3's three-tone arc (side and down orientations; the finisher by tint).
  Delight: sparks when a mace meets metal. Done: C1, C5 (effects).
- **ACTIONS** (artist-xhigh). Files: maps/arms/adult/action/**, maps/arms/adult/armed/**, maps/legs/adult/action/**,
  clips/actions/*.clip, clips/armed.clip. Brief: every family x 3 variants x 4 facings, cast, the bow's draw and release,
  block and roll, and the drawn idle, walk and run, on A3's reach (STYLE_P2 section 12), starting from P2a's pose
  numbers. Depth: a parry pose; a shield bash. Delight: the impact flash and a one-frame camera nudge on heavy finishers.
  Done: C5.
- **HUMANOIDS** (artist). Files: maps/humanoid/**, clips/humanoid.clip. Brief: the skeleton's bone body set; the draugr
  (the adult body with its skin, head and horns); the goblin (the youth body with its head, ears, skin and scraps); in the
  monster sheet layout through humanoidCell2. Done: C1.
- **DOLL-C** (engineer-high). Files: rpg/art/doll/*.cpp and *.h (not the maps), tools/doll/*, rpg/art/art_fx.cpp. Brief:
  weapons by angle and depth, shields, glints by frame, the drawn LOCO in ACTION, the effects drawn from their maps, the
  atlas and bake budgets with gear. Done: C3, C8.
- **FOLK-SIM-C** (engineer). Files: rpg/sim/gear_look.cpp, looks.cpp, anim.cpp. Brief: the sheath kind of each weapon;
  the drawn state for foes, guards and companions; any new derived arms fields. Done: C5.
- **SWITCH** (engineer-high). Files: rpg/view/paperdoll.cpp, creator.cpp, loot_ui.cpp, hud.cpp, party_hud.cpp,
  render_anim.cpp, render.cpp; rpg/art/art_party_portrait.cpp, art_human.cpp (deleted), rpg/art/rig/* (deleted),
  art_rig.cpp; tools/preview/rigdraft.cpp, rig_gallery.cpp, rigarms_gallery.cpp, rigposes_gallery.cpp (deleted);
  CMakeLists.txt. Brief: every remaining human draw path on the doll (the paperdoll, the creator preview, the HUD hero and
  the party list at whole multiples, portraits by A3's face, loot previews, statues, the Mythic aura per doll frame);
  then the flip. Depth: the creator preview animating idle, walk, run and a swing. Delight: the paperdoll turning to the
  facing of the last tap. Done: C2, C7, C9.

Waves: phase A -> one wave (all areas; ACTIONS composes on the weapon drafts until WEAPONS replaces them; SWITCH's
plumbing behind the flag) -> **the flip** when C1 reads 100 % and C3-C5 pass -> integration (the one full regression,
art_hash and golden_village_art re-recorded once with before / after pairs, the web build) -> judge rounds (at most 4).

P2c DONE gates:
| gate | target |
|---|---|
| C1 coverage: `--doll-coverage --scope all --seeds 1..20 --gates` over sources A-E | 100 %, 0 dead parts |
| C2 after the flip: frames drawn from a v1 sheet over the full script sweep and every Witness test (`v1Draws`) | 0 |
| C3 gear on every frame (LOCO, RUN, ACTION, DEATH, POSTURE; sleepers where worn): no holes; the helm on the head box; the shield on the off arm; the cloak behind or in front by facing; a lying body keeps its gear | 0 failures |
| C4 armour visibly changes the hero: every form x band on the man and on the woman changes 25 % or more of the idle figure's pixels (15 % for padded and leather) and the silhouette of the medium and heavy forms; equipped in game (`gear` script, shot pairs, `witness measure box` and diff); the judge's fit review | all |
| C5 families: the strike frames of every family pair differ in 25 % or more of their union per facing; tip paths; variants 25 % apart; left frames painted, the weapon in the right hand; the effect ends with the strike within a frame | all |
| C6 the weapon library: every map at 16 angles, the lit edge toward the top-left, a tip pixel, no orphans or doubled pixels | 0 failures |
| C7 menus and portraits on the doll; the Mythic aura hugs doll frames | all |
| C8 speed: ACTION 512 KB or less per look; people 24 MB or less in the capital; the city-arrival worst frame within baseline + 4 ms; render ms within baseline + 10 % | all |
| C9 art_hash and golden_village_art | re-recorded once, with before / after pairs |
| C10 playtests: T4 fighter ("fight bandits with each weapon family: does each swing look different?"), T5 outfitter ("as a man, then as a woman (the `female` reset), buy and equip leather, mail and plate and take a shot after each"), T6 watch ("find guards in three peoples' towns and soldiers of two realms"): every goal met by 2 of 3; notes triaged | PASS |

**Ruled 2026-10-28 (owner; the armour proposal's questions 3, 4, 5, 8, 9 and 12; 15.37): the look-only parts ship inside
P2c.** These are owner rulings, so docs/PLAN.md takes them up. Its wave-2 line "Not in wave 2: the loot proposal's twelve
questions" predates the answers. PLAN.md places them in P2c's waves and decides the how. Every part:
- is look only: no rule, no item field, no save change and no new draw;
- leaves wearGear's, census::dress's and gearIcon's outputs unchanged;
- belongs to the P2c area that already owns its files (docs/ARMOUR_SPAWN_RARITY.md section 8);
- fits in about 0-15 KB of pool, inside D70.

The parts:
- **The marks** (Q3).
  - A band-1 Epic wears trim, a Rare at T5 adds the glyph, and an Epic held at the T5 cap adds the glyph and an unlit gold
    inlay. HumanLook gains an `epic` bit per slot (rig 2's key only).
  - Each step reads finer at 1x than the one below, with no new light or motion. A T5 Epic with its marks still reads
    below T6 in D61's blind read. `--doll-ladder` checks the invariant.
  - (DOLL-C; FINISH)
- **Every metal keeps its helmet** (Q4).
  - A Mythic culture-alloy piece is drawn as its alloy (helm family, outline and colour). On top it wears the ember seams
    and rims, the ember collar and the flame crown, seated on its own helm with no flame within 2 rows of the eye row.
  - The black MYTHIC look (A5) stays for Mythic bronze, iron, steel and the relics. D32 is restated for culture alloys.
    HELMS seats the crown on every alloy helm family that lacks a crest socket.
  - Gilded wears PARADE's onion helm and starmetal STARFALL's crystal helm at T5-T6, in tier views with the tier's finish.
    So the hero's road shows the onion helm at T5 and the crystal helm at T6. This replaces the envelope's plumed great
    helm and winged helm there, and D61's "part by the helm family", which D66 left waiting on this question.
  - D44's measures still bind: consecutive tiers 0.10 head OD apart, and head area 5 % or more over the tier below. The
    family's tier views meet them, as D68 does.
  - The great helm and the winged helm stay drawn (D49). D68's open point closes: a Legendary keeps its metal's helm
    family.
  - (DOLL-C; HELMS)
- **Bulk on the old makers' ladder** (Q5, "grow it").
  - Obsidian draws one shoulder size above gilded.
  - Starmetal's crystal shoulders are the largest shoulder mass at every tier they draw. STARFALL keeps its crystal helm,
    slim ribbed waist and long split skirt, so it reads broad above and slim below.
  - Each relic band reads bulkier than the band below (front walk f0, opaque px).
  - All of this stays inside kEnvelope's tier caps (D43), and growth ADDs sizes and views (D49). A cap that blocks a step
    goes to the planner with its numbers (D67).
  - (DOLL-C; ARMOUR-X)
- **Crafted looks** (Q8). An alloy draws T4 or more, and a masterwork draws T5 only from band 4. This restates the
  ladder's signed line "a masterwork alloy is T5 at least". (DOLL-C)
- **Far lands** (Q9; band 4 and up).
  - Bandits draw T2, and soldiers and the village watch T3, with no alloy. This is the regional lift FOLK-SIM-C already
    builds.
  - Bandit chiefs draw T2 at home and T3 far, in the plate they already wear; only the ladder's tier field changes. That
    line is in game.cpp, which no wave-2 area owns, so the P2c planner assigns it.
  - A two-alloy realm's king wears the warriors' alloy at T5, with crest, gold, gem and the finish on his over-layer's
    hem, and no surcoat or full cape (D38).
  - (FOLK-SIM-C; DOLL-C)
- **Checks:**
  - `--doll-ladder` (the invariant);
  - `--doll-coverage` source L for the far-land tiers, Mythic alloys and relic helms;
  - a relic band that is not bulkier than the band below fails;
  - C14's face line on every crown seat.
  Old saves change in looks only. The rules wait for P2c DONE (15.37).

#### P2d "Proportion" (was P2c)

Unchanged in content: the creatures and village animals re-proportioned to the 32 px people, and every prop the P2c judge
lists as reading wrong. (2026-10-23: plus the humanoid monsters, skeleton, draugr and goblin, moved here from P2c; their
v1 painter moves unchanged to art_monsters.cpp at the P2c flip.) It follows STYLE_P2's ramps and outlines; whether creatures move to hand-placed maps is decided at
P2d's phase A from the P2c judge's notes.

Phase A (lead): creature heights from painter metadata (`rpg_test --creatures`), scripts p2d_creatures.txt and _phone
(every monster and great form beside the hero; a village's animals; the hound), witness/p2d.witness.json. Areas:
**CREATURES** (artist; art_monsters.cpp, art_critters.cpp, art_party.cpp): troll, yeti, golem, harpy, bear, boar, the
dragon and the great forms re-proportioned in bigger cells on the one pixel grid; dog, cat, chicken, rooster, goat, pig,
duck, cow, sheep; the hound. **POLISH** (artist): only the files of the props the P2c judge listed, none of CREATURES'.

P2d DONE gates: D1 heights against the 32 px adult: troll 1.45-1.6, yeti 1.5-1.7, golem 1.6-1.8, harpy 0.95-1.1, bear on
all fours 0.6-0.75, wolf at the shoulder 0.45-0.55, boar 0.4-0.5, goblin 0.7-0.8, dragon length 3.0 or more; dog 0.3-0.4,
chicken 0.2-0.25, goat 0.45-0.55, cow 0.7-0.8, the loose horse equal to the ridden one. D2 telegraphs, name plates and bars
sit on the new heights; the elite scale (115-130 %) still reads on top. D3 every P2c "reads wrong" item closed.

**Tests and scripts** (each script has a `_phone` twin; screenshots go to %TEMP% and are deleted after review; an
`eyes` agent turns shots into text):
- rpg_test `--rig` (P2a's contract, kept; its procedural gates retired at P2b phase A and restated for A3), `--anim
  [--seeds 1..3] [--gates]` (kept; P2b adds the run, the gait switch and the sheathed / drawn pick), `--doll [--gates]
  [--parity] [--report]`, `--doll-coverage [--seeds A..B] [--scope civil|all] [--gates]`, `--doll-variety [--seeds
  A..B]`, `--creatures` (P2d). CI step "Clear Sight rig and animation test": P2a's lines, plus `node tools/doll/pxc.mjs
  --check`, `--doll` and `--doll-coverage --seeds 1..3` (P2b); each integrator adds `--gates`.
- Scripts: the p2a_* scripts (kept); p2b_slice, p2b_folk (lineups of every people by role, sex and age at 11:00 and
  22:00), p2b_poses (tavern, smithy, farm, bedroom, temple, festival, the bard, a stable ride, a rowboat), p2b_walk
  (`camhold` walk and run runs), p2b_holders; p2c_gear, p2c_swings, p2c_menus, p2c_world (a war camp, a market, a
  harbour); p2d_creatures.
- Witness (`node ../witness/bin/witness.js test | shot | measure | play`): witness/p2a_lens.witness.json (kept),
  witness/p2b_doll.witness.json (rig 2: covered looks, the gait by speed, the drawn pick, corpses, riders), witness/
  p2c_switch.witness.json, witness/p2d.witness.json; `witness measure scale | box | contrast | motion` for B2-B5 and C4;
  `witness play` for B13 and C10.
- The judge measures on device-resolution shots (the `shot` call), never on the canvas-sampled Witness screenshot (it
  samples UI px, so a world at D != U would alias).

**Saves, versions and goldens.** No SAVE_VER, ENDLESS_GEN_VER, SAGA_GEN_VER or block-version bump in P2a, P2b, P2c or P2d:
footprints and generation are untouched; HumanLook is derived on load and the doll's new fields are derived, never saved;
the census's existing draws are untouched (the new stream runs last); postures are not saved; the Appearance block is
unchanged in P2 (P3's creator appends its choices); the zoom and ring live in the settings file (EMBS version 2). Old
saves keep working; no new adventure is forced. tests/fixtures/save_v23.bin and the town, world and endless goldens never
change. The doll's fields enter key() only at rig 2, so rig 1's keys and the `--arms` golden keys hold until the flip.
art_hash was re-recorded by P2a phase A (the ink outline) and its integrator; the P2b integrator adds the doll's own
categories; v1's categories stay frozen until P2c deletes them and re-records once at the flip. golden_village_art
("villages never change", M7b) stays identical through P2b and is re-recorded once at the P2c flip with before / after
pairs. Areas never re-record a golden or a fixture.

**The P2a style slice** (history: P2a wave 1, RIG-BODY, artist-xhigh; passed 2026-10-18; its procedural look is
superseded by A3, and its protocol is the model for P2b's slice gate). The hero as a man and as a woman
(same look, a sword) in Idle, Walk, Hurt, the sword's three variants, Fall and Lying in four facings with the Steel Arc;
one village woman (Dress, a culture cut, a pattern) in Idle and Walk; one start-village cottage with its door and wall
at k, a fence run, the ring. Scripts p2a_slice.txt and _phone (seed 7, OSLARS, rig 2): desktop 1440x810 at NORMAL (D3)
and NEAR (D4), phone 2556x1179 at NORMAL (D5), 11:00 and 22:00. The judge's acceptance protocol (all must hold; each
failure comes back with a repro and a check):
1. Size: man 32, woman and villager 31 art px (the shot-pair mask's height / D): 96 / 93 device px at D3, 128 / 124 at
   D4, 160 / 155 at D5 (11.9 / 11.5 % and 13.6 / 13.1 % of the screen height).
2. Crisp: in the 11:00 shots with `ui 0`, every world pixel an exact D x D block; the hero crop from the knee up uses
   only colours of the baked frame and of the ground.
3. Face (down idle): brows on F-26, two 1x2 eyes at face columns 3 and 6, a nose pixel, a 2 px mouth; eye-to-skin
   contrast 60 luma or more; the side frame has eye, nose and ear; the back frame no face.
4. Hands: two 2x2 hands in down and up idle and on every walk frame; the sword hand over the grip, guard and pommel
   either side.
5. Outline: outer-silhouette pixels at luma 60 or less (100 or less on Lit Ink edges); no #000000.
6. Contrast: day edge contrast 40 or more on grass and road; at 22:00, 5 tiles or more from a lamp, the figure median
   10 or more off the ground (|figure - ground|) or edge contrast 40 or more (restated 2026-10-18 with L6 and STYLE_P2
   section 8); the ring 35 or more on both.
7. Bodies: shoulders differ by 2 px or more; hands, hips and feet within 1 px; man and woman read at phone size and
   neither is exaggerated (review).
8. Walk: 6 frames; side views (W/E): the planted foot lands within 1 px of its spot at each frame change (registration,
   .tmp/judge/motion_reg.py) and slips no more than one frame step (4.5 px of travel) within a held frame (motion.py;
   held frames whose sole row an occluder cuts are not measured); front and back views walk in place (the 16-bit convention); the hero stays steady on screen
   (pivot changes on 2 or fewer of 119 frames for W, E and S; N one way, 0 reversals) over a 2 s walk at 74 px/s (60 Hz
   frame shots) (restated 2026-10-18, main session: 1 px with 6 rigid frames is not reachable within R6's budget); breathe and blink both seen in 6 s of idle.
9. Swing: the three variants differ by 25 % or more; the tip arc 120 degrees or more; the arc effect ends within one
   frame of the strike state.
10. Fall: lying on screen 0.45 s or less after the kill, 16 px tall or less, head toward the fall side, the sword
    beside the hand.
11. World fit: the cottage door 23 px (0.72), the wall 35 (1.09), the fence 0.60 or less of the figure; the hero
    walks through the door without looking taller than the eaves.
12. Look review first: STYLE_P2's palette, top-left light on all four facings, no pillow shading, banding, orphan
    pixels or jaggies, the hero found first in the village shot, and the side-by-side with a v1 villager showing the
    upgrade.
After PASS, STYLE_P2 recorded the slice's measured numbers (now its appendix).

**What the judge looks at.** Look and feel first, from the state a player starts in (a new game, NORMAL zoom, the phone
size; `?rig=2` until the flip), then the gates. P2a: is the world crisp on every screen; is the hero found at once by day,
at night and in a crowd; do text plates stay readable at every zoom; do the holders fit the new bodies; is perf flat. P2b:
does every person look like A3 (faces, ramps, coloured outlines, the top-left light) in every facing and motion; can you
tell people apart in a village and peoples apart in 10 seconds (pillar 2); do men and women each wear their own cut; are
walks planted and runs purposeful; do people sit, sleep, ride, row and fall right; perf. P2c: does each weapon swing its
own way; does armour visibly change a man and a woman; are guards and soldiers of different peoples told apart; menus;
no v1 anywhere after the flip; perf. P2d: creatures keep their menace beside the new people. The judge states the full
protocol in round 1 and reruns it every round; fixers rerun the judge's harness before claiming a fix; the every-system
regression runs once per batch (integrator).

**Risks.** P2a's risks (bake time and memory on the phone, facades growing 11 px a storey, the hero's 4-frame windup,
crowds overlapping, two rigs side by side, Witness's window) still hold where they apply. For P2b and P2c: (1) Volume:
about 3,400 maps. The cuts above, the drafts, the slice first, ten artist areas in parallel in P2b and the compiler's
checks on every save keep it bounded. (2) Coherence across ten artists: STYLE_P2 plus the signed slice; the gallery's
lineup pages; the judge compares every area with the slice. (3) The format cannot express something A3 did: parity on 48
frames finds it in wave 1, while the format can still change. (4) The run and sheathing are new looks: the owner decides
at the slice, with A3's walk and P2a's always-in-hand as the fallbacks. (5) Gear breadth: the coverage census names every
missing field, value and part, and the flip waits for 100 %. (6) Wasm size and compile time: the pool's budget and
chunking; the compiler reports both. (7) A quarter-turned lying part may light wrong: the judge flags it and the part
gets a painted lying view. (8) Another agent's balance and quest work: wave 0.

**For the owner to confirm** (the plan proceeds on these defaults). Items 1-3 are P2a's and stand; items 4-5 are new
with the re-plan and are settled at the P2b slice sign-off from the owner pack.
1. On Safari, 2x iPhones and the iPad the UI keeps today's size, while the world is whole-number everywhere.
2. Default zoom D0 = round(screen px height / 240).
3. Facades grow with the rig (door 16 -> 23 px, one-storey wall 24 -> 35 px), so the "villages never change" golden is
   re-recorded once, now at the P2c flip.
4. **The run**: people above 54 px/s run on a cycle of about 36 px, so the hero's legs cycle about twice a second at
   74 px/s. The alternative is A3's walk at its natural stride, which puts the hero at 3.9 cycles a second.
5. **Sheathed weapons out of combat**: A3's sword at the hip with the hand on the hilt, drawn while fighting; legendary and
   Mythic hilts glint. The alternative keeps P2a's weapon in hand at all times.

### 15.37 Armour spawning, alloys and rarity (owner, 2026-10-28): the rulings, and the armour rules update (planned)

The owner asked how armour should spawn and how the advanced alloys should work with rarity. The proposal is
**docs/ARMOUR_SPAWN_RARITY.md** (revision 3 of 2026-10-25, approved as revision 4; revisions 5 and 6 add the owner's
follow-up and answers on companions). It holds the design, with file:line citations at df2a155. On its twelve questions
the owner answered "Defaults is good" on 2026-10-28, which accepts every recommended default, and amended question 11
(companions). Later that day the owner settled the plan's two companion readings, "Made once, and if they die they drop
everything", and then answered the four companion points left to confirm, ruling that a companion who comes back is
the same companion (items 4-7 below). This section is the index. Each ruling is written into the section it changes.

**The rule:** where you are decides the metal, luck decides the finish, old places hold the old makers' metals, and
every metal keeps its own helmet.

**How it should feel:**
- Starting armour is plain and slim, and every step up is more detailed, more alive and bulkier.
- Each land has its metal, and a lucky find is the same metal, finer. Because no roll spends the next land's reveal, a
  new land's shop window is always new.
- A ruin in any land can hold a black glass breastplate.
- A companion arrives in their own people's kit, and the hero can do better by them. If they fall, everything they
  carried lies where they fell.
- A friend who leaves and comes back is the same friend: their bond, their kit and their story come back with them.

| # | ruling (2026-10-28) | ships in | written into |
|---|---|---|---|
| 1 | each people's alloys in order (everyday band 4, warriors' 5, masterwork 6-7), never weaker than the band; crafting caps by rank | the rules update | 15.11 |
| 2 | gilded, obsidian, emberforged and starmetal are old-place relics from band 4, about 1 armour find in 8 | the rules update | 7.2, 15.11 |
| 3 | rarity is finish in the piece's own metal, with marks at the top; its colours stay on names, frames and ground light | P2c (looks) | 7.3, 15.27, 15.36 |
| 4 | every metal keeps its helmet: a Mythic alloy keeps its alloy; gilded and starmetal keep their own helms at T5-T6 | P2c (looks) | 15.36 |
| 5 | grow starmetal (the largest shoulders), obsidian one size over gilded | P2c (looks) | 15.36 |
| 6 | option (b): luck lifts to Legendary only from level 17 and to Mythic only from 27; campaign finales aside | the rules update | 15.9, 15.27, 15.28 |
| 7 | shelves and commissions stop at the warriors' alloy; a trusting capital smith and hidden-kingdom smiths excepted | the rules update | 7.5, 15.9 |
| 8 | a crafted alloy draws T4 or more; a masterwork draws T5 only from band 4 | P2c (looks) | 15.11, 15.36 |
| 9 | far-land folk one step finer without alloys; bandit chiefs T2 (T3 far); two-alloy kings per D38 | P2c (looks) | 15.36 |
| 10 | war spoils: soldiers 10 %, captains 50 % (Uncommon or better), 3 a siege, capped by the hero's level | the rules update | 15.9, 15.28 |
| 11 | companions (amended): their kit is made once and stays theirs while they live; a gift stays the hero's and is worn only if better; at death they drop everything where they fell, the kit as ordinary gear, and the grave keeps no keepsake (the owner's follow-up, the same day); a companion who leaves and comes back keeps their level, kit and record (the owner's answers, the same day) | the rules update | 9, 15.6, 15.9, 15.19, 15.26, 15.27, 15.28 |
| 12 | looks inside P2c; the rules as their own update after P2c DONE, with their own full test run | - | here, 15.36 |

**The armour rules update (planned; not P2c and not P2c's wave 2).** It starts after P2c is DONE, as its own update with:
- its own areas;
- a verifier gate;
- one full regression at its integration (the owner's rule: once per update, and fix rounds verify only their fixes);
- judge rounds, at most 4.
Its files are disjoint from P2d's.
- **IN:**
  - rulings 1, 2, 6, 7, 10 and 11;
  - the metal explained on the card and the HUD (15.11);
  - saga loot and building chests given a maker;
  - no loot from summons, thralls or decoys;
  - the commission seeded per smith and restock;
  - optionally, the everyday alloy's lore line "HARDER THAN STEEL, AND CHEAPER TO POUR" (the same pick, so no culture
    changes).
- **OUT:**
  - a SAVE_VER bump (23 holds) and new item fields (the party block's own version steps to 5 for the parted roll, and
    still reads 4);
  - changes to the alloy generator;
  - leathers above band 1 (PLAN.md D23), alloy move quirks and weapon styles (D34);
  - a third alloy for two-alloy peoples, deferred until the playtests;
  - a relic's own colour per slot (D15, the owner's later call);
  - kits that remake themselves as companions level or come back (ruled 2026-10-28: "Made once"; "they arent new").
- **Areas** (one owner per file):
  - **LOOT** (engineer-high): gear.{h,cpp}, items.{h,cpp}, game_rpg.cpp, story.cpp, story_game.cpp. It publishes the item
    flags, the DropSource fields and the shared "better" rule first.
  - **PARTY** (engineer-high): party_game.cpp, party.{h,cpp}, party_recruit.cpp, party_talk.cpp, banter.cpp (the
    companions' gear lines and their cry on coming back only). Split from PARTY-WAR on 2026-10-28, because leaving and
    coming back adds the party block, the recruit paths and the apology at a sellsword's table.
  - **WAR-CRAFT** (engineer): craft_game.cpp, war_game.cpp, game.h (the spoils counter's bits).
  - **UI** (engineer): loot_paint.h, hud.cpp, paperdoll.cpp (the card's metal row, the HUD's metal word, and the party
    page's greyed TAKE BACK, decline note and arrow at the companion's level).
  - **TEST** (engineer): test_loot_sources.cpp, test_loot.cpp, test_party.cpp, test_wards.cpp, the file that holds
    P2c's `--ladder-pace`, and two hooks in script_party.cpp for the script sweep and Witness: `party snap <name>` and
    `expect party same <name>` (id, bond, approval, memories, and the kit's seeds by slot).
  - Wave 1 is LOOT's contract block. Then PARTY, WAR-CRAFT, UI and TEST run in parallel.
  - Core: the IN list. Depth: a companion declines a piece their vocation never uses. Delight: the party page marks the
    pieces the hero lent and lists the friends you parted from and where they wait; a parted sellsword waits at his
    table in the kit he left with.
- **Done** (the judge measures from a new game and from save_v23; ARMOUR_SPAWN_RARITY.md section 9 has the full lists):
  - `rpg_test --ladder-pace --worlds 1..20 --report` passes its fail list:
    - no rarer piece drawn plainer;
    - every alloy dropping at its band, at its band's strength or more;
    - no shelf or commission masterwork outside the exceptions;
    - relics only from band 4, only from old places and never with a people's id, at 8-20 % of metal armour drops in the
      assumed hour;
    - no Legendary below level 17 and no Mythic below 27 outside campaign finales and the dragon's hoard.
  - These stay green:
    - `--loot-sources` (with the new rows) and `--loot`;
    - `--power-curve` (same-band trash in 3-5 hits; the best allowed rarity 2.5 or more);
    - `--metrics`, `--foes` and `--arms`;
    - `--companion` (15.26's cases and fuzz);
    - `--wards` (the spoils counter);
    - save_test (save_v23 loads, its party block with an empty parted roll, and the new bits round-trip);
    - `rpg_test --seeds 1..20`, the script sweep and Witness.
  - The judge plays:
    - an explorer finds an old maker's piece in a ruin by band 4;
    - no shop sells a masterwork outside the exceptions;
    - a companion takes a better gift, declines a worse one, and hands it back when dismissed;
    - the same companion, asked back at their home after the hero has gained levels, comes back with the same bond,
      skills and kit (`expect party same`), and a sellsword rehired after his week the same;
    - a fallen companion's kit, gifts and belongings lie where they fell, and the hero gathers them;
    - the playtests watch whether the everyday alloy reads as a step down after steel.

**Docs still to write to the answers.** This is the proposal's doc batch, for the P2c planner, who owns these files during
wave 2. The VISION part is written above.
- docs/PLAN.md:
  - "a masterwork alloy is T5 at least" (Q8);
  - the T6 row's "level 20 at the earliest" (Q6: 17 by luck once the rules update lands, 20 for ordinary finds);
  - the shop and focus-vendor lines (Q7);
  - D32 restated for culture alloys, and A5 limited to the MYTHIC style (Q4);
  - the hero road's T5-T6 helms, D61's parting and D66 item 2 (Q4, Q5);
  - one statement of gilded's tier (band 4: T4 as found, T5 as an Epic) in place of "PARADE (T5), STARFALL (T6)";
  - the wave-2 line "Not in wave 2: the loot proposal's twelve questions" (Q12);
  - an owner for the bandit chiefs' line in game.cpp.
- docs/STYLE_P2.md: "the Heartland's T5 is gilded", 28.4's PARADE and STARFALL helm lines (Q4), and STARFALL's and
  KNAPPED's shoulder lines (Q5).
- docs/P2C_CONTRACTS.md: HumanLook's `epic` bit, census::Rank's `far` field, and the Mythic-alloy and relic-helm paths.
- D27 is already closed by D46 and D48.

**For the owner to confirm** (the plan proceeds on these readings). On 2026-10-28 the owner answered items 1 and 2 ("Made
once, and if they die they drop everything"), then items 4-7 the same day. Item 3 is still open, and items 8-10 are new
with that last answer.
1. "Their kit climbs with them" is read as "made at their level when they join", so a later recruit starts higher on the
   ladder, and the hero's gifts carry them after that. **Ruled 2026-10-28: "Made once."** The kit is made when they join
   and never remade as they level; gifts are the only way up (15.26).
2. Gifts come back to the hero at a companion's death as well as at every parting, so a fallen friend's grave holds only
   their own things and the keepsake. **Replaced 2026-10-28: "if they die they drop everything."** Everything a fallen
   companion carries (kit, gifts and belongings) drops where they fell for the hero to pick up, the kit as ordinary
   gear, and the grave holds no keepsake. Partings while they live still hand the gifts back (15.26).
3. Where the rules update sits in the roadmap: before, beside or after P2d. Its files are disjoint from P2d's.
4. (2026-10-28, from item 2) A dropped kit is ordinary gear, so the kit's item level is capped at the hero's level + 4
   and its rarity rolls with the hero as the finder. Today a kit takes the place's danger, so a level-10 hero who
   recruits in a D-30 town and lets the companion die would gather item-level-30 gear rolled past ruling 6's gate. The
   companion's strength at the join is unchanged, because level sync already caps how they use a piece. Reading "their
   level" as the companion's own level (one below the hero's) would close it too, but would dress a band-1 villager in
   alloy. **Ruled 2026-10-28: "that works for me".** The cap and the hero as the finder stand (15.26).
5. (2026-10-28) The pile keeps the ground loot's own rules: it lies 240 s and is gone when the hero changes map or loads,
   because ground loot is never saved. A death off the hero's map (a companion at home carried off by raiders whose
   rescue fails) has no ground, so what they carried is lost. A longer-lived pile would be a new rule. **Ruled
   2026-10-28: "yep thats good".** The 240 s, the map change and the load, and the raid at home all stand (15.26).
6. (2026-10-28) Hire-and-let-die now pays once per sellsword: a warrior's Common kit sells for about 2.5 times his first
   day's wage (*model*: 176 gold against a 70-gold hire at a D-10 inn). MERC_FALLEN retires his slot for good, so it
   never repeats; a census companion's kit costs no gold but a friend. The plan accepts this as a one-time find per
   companion. **Ruled 2026-10-28, accepted: "thats fine if someone wants to farm that way they can but there are
   defintly better avenues to make money".**
7. (2026-10-28) A companion who parts and later joins again is a new join: their record starts again, bond included,
   and the kit that left with them is made again from its seed. Nothing doubles. **Reversed 2026-10-28: "well if they
   re join they should stay at the level they left, and shouldnt get treated as a new join since they arent new."** A
   companion who parts while living and later comes back keeps their level, the kit they left in (never made again) and
   their record: bond, approval, memories and history. The record waits in the party block's new parted roll. Gifts
   still come back to the hero at the parting (15.26 rule 8).
8. (2026-10-28, from item 7) A companion's level is never stored: it tracks the hero's, one below (section 9: "Level
   tracks player level − 1"). So "stay at the level they left" is read as "nothing they earned resets": they come back
   at the hero's level less one, never below where they left, with their bond, the skills it opened and their kit as
   it was. A level frozen while they are away would need a stored level and a catch-up rule, and would bring them back
   weaker than a stranger hired that day.
9. (2026-10-28, from item 7) A sellsword who left in anger now keeps his record, his anger included, and rehired as he
   is he would walk out again at the next deed. So his inn table says "YOU OWE ME AN APOLOGY FIRST" until the census
   companion's apology (refused once, taken on a later day) is made there. Today he is simply rehired with a clean
   slate once his slot stands again.
10. (2026-10-28, from item 7) Whether a friend agrees to come back keeps today's rules: a census companion still needs a
    recruit's reason to come (befriended, helped, or a regular at the inn). Joining never befriends them, so a raid
    captive who joined as Rescued may have none, though their farewell says "YOU KNOW WHERE TO FIND ME". The plan keeps
    the rule; counting the kept record as a reason to come would be a small new rule.
    **Ruled 2026-10-28: items 8, 9 and 10 confirmed as written (owner: "sounds good").** Level reads as "nothing
    they earned resets"; an angry sellsword keeps his anger and wants the apology; return keeps today's reasons to come.
