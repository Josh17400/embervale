# EMBERVALE: plan for the next step

Written 2026-10-03, after the first review pass (`docs/REVIEW.md`). The game already runs end to end:
procedural world, organic settlements, caves and ruins, combat, quests, shops, a main quest that can now be
finished, and save/continue. The next step is not more systems. It is making the first hour **feel**
hand-made in every seed, and getting that onto an iPhone through TestFlight.

## The next milestone: "The First Hold" vertical slice (v0.2)

**Goal:** a new player on an iPhone plays from the start village to their first Ember Shard (about 45 to 60
minutes) and never sees the same thing twice in a way that feels copy-pasted. Every minute should offer
something to walk toward.

**Done means:**
1. A TestFlight build installs and plays on the owner's iPhone in landscape, with touch controls, a safe-area
   HUD and audio.
2. In 5 random seeds, the region within about 60 tiles of the start holds at least 8 distinct kinds of point of
   interest, and no two settlements in that region share a layout archetype plus a building palette.
3. Combat at levels 1 to 5 meets the targets in "Game-feel and balance targets" below, measured by the bot.
4. rpg_test passes on 20 seeds in CI, including the reachability and main-quest checks added in the review. The scripted playthrough (task 1) passes on 5.

## Ordered tasks

Each task has its own verification step. Nothing moves on until that step is green. The effort figures are
for one focused agent session plus review.

### 1. Test harness first (0.5 to 1 day)
- `--script file.txt` for embervale.exe: timed lines (`0.5 key E`, `2.0 hold W 1.5`, `4 shot out.png`), test
  only, never touching the save. The playtester's `--talk N` flag becomes one line of this.
- rpg_test: `--seeds A..B` runs a range and prints one summary line per seed. Add a **repetition audit**:
  POI types near the start, layout archetypes per settlement, distinct greeting lines per town, and the
  largest group of buildings with identical size and palette.
- **Verify:** `rpg_test --seeds 1..20` passes all of them. A script that walks into the inn, talks, buys and
  leaves produces the expected screenshots.

### 2. Game feel and early balance (1.5 days)
- Enemy behaviour sets: wolves circle and lunge from the flank, goblins swarm and then flee at low HP,
  skeletons get a slow reassemble chance, bandit archers keep their distance (they already back off), and
  bears and trolls get a telegraphed heavy attack you roll through.
- Encounter pacing: wild spawns come from "dens" (a visible prop the pack returns to) rather than the air
  17 to 24 tiles away. Clearing a den is a small reward.
- Feedback: a stamina-empty flash, a perfect-roll slow-mo blip, a pickup magnet sound, a louder level-up.
- **Verify:** the bot metrics in "targets" below, plus screenshots of each behaviour through `--fight` and the
  new `--script`.

### 3. Living settlements (2 to 3 days): the biggest anti-repetition win
- **Settlement archetypes**, chosen by biome, position and seed: fishing village (coast or lake: docks,
  drying racks, boats), mining town (mountain edge: mine entrance, ore carts, a smelter), farming hamlet
  (fields first, a mill), river crossing (a bridge-centred town), hill fort (walled town on high ground),
  market town (a big square and a caravan yard). Each changes the street generator, the building mix and the
  props.
- **Regional building palettes**: snow (stone, steep dark roofs, chimneys smoking), desert (adobe, flat roofs,
  awnings, palms), swamp (stilt houses, boardwalks), autumn (timber frame, thatch), plains (mixed). Today
  roofs vary only by colour.
- **New building types**: stable, mill, chapel, guard tower, warehouse, dock house, lumber camp.
- **NPC schedules**: work by day (smith at the anvil, farmer in the field, merchant at a stall), the inn in
  the evening, home at night (doors "locked", so talk to them in the morning). Guards light lamps at dusk.
- **Verify:** a screenshot gallery of 3 seeds × 3 settlement types × day and night. The repetition audit shows
  no repeated archetype plus palette within 120 tiles. The bot still reaches every shopkeeper (an rpg_test
  check that walks to each `Role` in the start town).

### 4. Wilderness points of interest (1.5 to 2 days)
- Procedural vignettes, each a small template with parameters: an overturned caravan (loot plus a bandit
  ambush), a hunter's camp (a friendly NPC who sells pelts and gives a hunt), standing stones (a
  once-a-day buff), a ruined watchtower (an archer nest and a chest), a wolf den, a fishing hut, a toll
  bridge with a troll under it, a herb garden, a lone grave with a rumour hook, a merchant caravan travelling
  the roads between towns.
- The rumour system points at these: innkeepers sell directions to vignettes as well as dungeons.
- **Verify:** the repetition audit counts at least 8 POI kinds within 60 tiles of the start. The playtester
  walks one road for 2 minutes and meets at least 2 things.

### 5. Dungeon variety (2 days)
- Ruins: room templates (pillared hall, burial niches, flooded crypt, trap corridor with dart plates, a
  lever-and-portcullis puzzle, the boss arena with braziers).
- Caves: themed variants (ice cave, abandoned mine with rails and ore veins, fungal grotto with glowing
  mushrooms, a lava fissure for the late game).
- Locked doors with keys from mini-bosses, an optional hidden room behind a cracked wall (bomb arrow or a
  heavy attack).
- **Verify:** the dungeon walkability check already exists. Extend it so every key is reachable before its
  door and the boss room is reachable. Screenshot each template.

### 6. Quest variety (1.5 days)
- New radiant types: deliver (a parcel to another town's NPC), retrieve an heirloom (from a named chest in a
  named dungeon), a missing person (found alive in a cave and escorted out), a named bandit (a unique chief
  with a title), protect the farm (a night wave at a settlement field).
- A templated text grammar so offers read differently: the giver's mood, the reason, the reward phrasing.
- Two-step town chains: each town's innkeeper hands out a 2 to 3 quest arc with a payoff, such as a discount,
  a house key or a companion.
- **Verify:** an rpg_test flow per quest type (offer, accept, objective, turn-in), driven from code like the
  existing innkeeper and main-quest tests.

### 7. iOS and TestFlight, copying The Fort (2 to 3 days, can run alongside tasks 3 to 6)
See "iOS path" below.

### 8. Save and world-gen versioning (0.5 day, do it first among the generator tasks)
- Today `SAVE_VER = 1` and any change to the format breaks old saves. Add `ver`-gated reads, keep the
  loader for v1, and add a round-trip test that loads a checked-in v1 save blob.
- Store a **world-gen version** in the save and keep the old generator path reachable, or migrate by site
  and building identity. Tasks 3 to 5 change the generator's RNG stream, which reshuffles buildings and breaks
  quest-giver and interior indices in existing saves. The review avoided this by hand (deferred prop removal,
  hashed choices), which will not scale.
- **Move this task before task 3.**

## Content and variety work against procedural repetition

Principles (the owner's "natural, organic, not boxy" applied everywhere):
1. **Vary structure, not just colour.** Archetypes and templates change silhouettes and layouts. Today's
   variation (roof colour, wobble) is necessary but nowhere near enough.
2. **Tie variety to the world.** Biome, elevation, coast, rivers and the distance from the capital should
   decide what a place is, so the variety reads as logic rather than noise.
3. **A no-repeat-within-radius rule** for anything memorable (settlement archetype plus palette, vignette
   kind, dungeon template, quest type from the same giver).
4. **Named things.** Name dungeons, bosses, bandit chiefs, rivers and peaks, and refer to them in dialogue.
   The first greeting pass does this already: NPCs mention the nearest dungeon and what lives there.
5. **Measure it.** The repetition audit in rpg_test turns "feels repetitive" into numbers that fail CI.

**Curving roads** (Should): replace the 4-connected L-shaped A* with 8-connected search plus Chaikin smoothing
when painting, filling diagonal steps so roads stay 4-connected and walkable. Keep the reachability test green.
**Continent variety**: archipelagos, peninsulas, inland seas, and a cap on single mountain masses (some seeds
are 40 % rock). The review's `connectAll` pass guarantees walkability, but huge massifs are still dull.

A short backlog of quick wins: more name syllables per biome (Nordic in the north, desert names in the
south), weather that lasts (fronts that move across the map rather than a per-3-hour hash), seasonal palette
shifts by day count, and ambient creatures (birds that scatter, fish jumping, deer that flee).

## Game-feel and balance targets

Measure these with the bot (`rpg_test --mortal`, improved to fight sensibly) and by playtester feel.

| Area | Target | Today (bot plus playtester) |
|---|---|---|
| First level-up | 3 to 5 minutes | about 3 to 6 minutes (bot) |
| Level 5 | about 25 to 35 minutes | 10 minutes (bot, 600 s run): too fast |
| Hits to kill a same-level wolf, boar or goblin | 3 to 4 | 3 to 4 |
| Hits to kill a same-level bear or troll | 8 to 12, with one roll needed | troll regen makes it 15+ early |
| Dying to 3 same-level wolves without potions | about 25 % | about 0 % (the bot never needed a potion in 10 min) |
| Gold for a first gear upgrade | after 1 to 2 quests | about right (quest 120 to 200, gear 50 to 200) |
| Boss fight length (dungeon) | 45 to 90 s | not measured yet |
| Ashfang fight length at level 12 | 2 to 4 minutes | the HP fix (1420 at level 12) makes this plausible: measure it |

Levers, in order: wild pack size and spawn cadence by zone, enemy damage per level (`applyLevel` 0.11/level),
player HP regen (0.7/s is generous), XP curve (`xpForNext`), potion prices.

## iOS path (copying The Fort)

The Fort ships with `.github/workflows/ios.yml`, which runs `fastlane ios beta` from `native/ios` on a
`macos-26` runner with six secrets shared across the team: `ASC_KEY_ID`, `ASC_ISSUER_ID`, `ASC_KEY_P8`,
`APPLE_TEAM_ID`, `IOS_CERT_P12_B64` and `CERT_EXPORT_PASS`. The Fastfile does manual signing: it imports the
p12 and the WWDR G3 intermediate into a temporary keychain, fetches an App Store profile through the ASC API
key, sets manual signing on the project, runs `build_app` with export method `app-store`, and finishes with
`upload_to_testflight`. Embervale reuses this exactly. Only the "make an Xcode project" part differs,
because this is CMake and SDL3 rather than Capacitor.

Steps (the owner creates the repo and sets the secrets; agents never touch secrets):
1. **Repo:** the owner creates the private `Josh17400/embervale`, commits this tree and pushes. Add a
   `.gitignore` for `build*/`, `*.p8`, `*.p12`, `*.mobileprovision` and `*.cer`.
2. **CI for tests first:** `.github/workflows/ci.yml` on `windows-latest` builds `rpg_test` and runs it over
   20 seeds on every push. No secrets needed. This catches regressions before iOS work starts.
3. **CMake for iOS:** in `CMakeLists.txt`, when `CMAKE_SYSTEM_NAME STREQUAL iOS`, make `embervale` a
   `MACOSX_BUNDLE` with `ios/Info.plist.in` (landscape only, `UIRequiresFullScreen`, status bar hidden,
   `UILaunchScreen` empty dict, `CFBundleShortVersionString` from the project and the build number from
   `CURRENT_PROJECT_VERSION`). Set `XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY=1,2`, deployment target 15.0 and
   the bundle id `com.embervale.game` (the owner registers it in the developer portal and creates the App Store
   Connect record, as for The Fort).
4. **App icon from code:** `tools/make_icon.cpp` renders a 1024×1024 icon with `art.cpp` (a dragon over
   Embervale's peaks) into `ios/Assets.xcassets/AppIcon.appiconset`. No art files, consistent with the
   project.
5. **Platform code:** call `SDL_GetWindowSafeArea` and inset the HUD and touch buttons by it. Set
   `SDL_HINT_AUDIO_CATEGORY=ambient` so the silent switch is respected. `touchUI` already defaults on for iOS.
   Saving on `WILL_ENTER_BACKGROUND` already exists. Consider a dynamic logical width (270 high, about 585 wide
   on 19.5:9 phones) so iPhones see more world rather than black bars. That is a contained change in `Pix`
   plus HUD anchoring.
6. **Main loop:** SDL3 still supports a classic `main()` loop on iOS, but the recommended route is
   `SDL_MAIN_USE_CALLBACKS`. If the classic loop stutters or misbehaves on device, move `main.cpp` to
   `SDL_AppInit` / `SDL_AppIterate` / `SDL_AppEvent` / `SDL_AppQuit`. The loop body maps one to one.
7. **Workflow:** copy The Fort's `ios.yml` to `.github/workflows/ios.yml` with the same trigger
   (`workflow_dispatch`, which uploads on every run), the same runner and the same secrets. Replace the
   Node/Capacitor steps with: build and run `rpg_test` on macOS over 5 seeds (a clang build doubles as a
   portability check), then
   `cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0`, then
   `fastlane ios beta` from `ios/`.
8. **Fastfile:** copy `the-fort/native/ios/fastlane/Fastfile` to `ios/fastlane/Fastfile` and change only
   `path:`/`project:` to `../build-ios/embervale.xcodeproj`, `scheme:`/`targets:` to `embervale`, and the Appfile
   bundle id. Keep the p12 import, WWDR download, profile fetch, `export_method: "app-store"` and the
   build-number xcarg exactly as they are. **Do not** run `mint_cert`: the team already has a distribution
   certificate, and The Fort's CI_SETUP.md explains why a second one is harmful.
9. **Secrets:** the owner copies the same six values used by the-fort and euchre-unleashed (same team, same
   certificate) with `gh secret set ... --repo Josh17400/embervale`.
10. **Ship:** `gh workflow run ios.yml --repo Josh17400/embervale`, then install from TestFlight.
- **Verify:** CI is green. The TestFlight build launches, shows the title, starts a new game, plays with touch
  for 5 minutes, saves on background and continues after a cold start. Also check FPS on device (the target is
  a solid 60, with terrain chunk bakes off the main thread, as they already are).

## Risks

| Risk | Impact | Mitigation |
|---|---|---|
| Terrain baking is all per-pixel CPU work (512×512 chunk ≈ 262k `groundPixel` calls) and may be slow on an older iPhone | Hitches when entering maps | It already bakes on a worker. Measure with `EMB_TIMING=1`. Shrink chunks to 16 tiles on iOS, or cache rock levels |
| The SDL3 iOS main loop or audio session behaves differently on device | Black screen or no sound on TestFlight | The callback fallback (step 6). Test on device before content work depends on it |
| An integer-scaled 480×270 canvas leaves wide black bars on phones | Feels cramped and small | Dynamic logical width (step 5) |
| Save format changes break the owner's saves | Lost progress, frustration | Task 8 before any TestFlight build |
| Variety work multiplies generation paths, so bugs hide in rare combinations | Soft-locks in specific seeds | Range runs of rpg_test over 20 to 100 seeds in CI, plus walkability, reachability and quest-completion checks for every new template |
| The owner pivots ("judges by feel") | Wasted depth | Ship small slices: each task above ends in screenshots and a playable build the owner can feel the same day |
| Disk use from screenshots and build dirs | Owner annoyance | Screenshots go to Temp and are deleted. Builds reuse `build/`. Any temporary second build dir (~300 MB) gets deleted at the end of the session |

## Verification summary per step

- Every task: rpg_test across the seed range (all OK), plus at least one scripted embervale run with
  screenshots that a reviewer looks at, then deletes.
- Generation tasks (3 to 5): the repetition audit numbers go up, walkability and reachability checks stay
  green.
- Feel tasks (2): the bot metrics table, plus a playtester session on feel.
- iOS (7): a device run of the checklist above. A build number that matches the GitHub run number shows in
  the System tab, as in The Fort.

## Progress

**2026-10-03: tasks 1, 8 and 2 are done.** On MSVC, `rpg_test --seeds 1..20` passes 20/20, `save_test` prints ALL OK, and both example scripts exit 0 with no failures.

- **Task 1 (harness):**
  - `embervale --script file.txt` sends real key, mouse and touch input. It adds `walkto`, `expect`, `goto`, `fight`, `talk`, `hour` and `shot`, never saves, and exits 3 on any failure. The examples are `tools/scripts/inn.txt` and `fight.txt`.
  - `rpg_test --seeds A..B` runs a range, with a repetition audit. Today's baseline over seeds 1..20:
    - POI kinds within 60 tiles: average 3.4, minimum 2 (target 8).
    - Settlement shapes: 46 % distinct.
    - Greetings: 72 % distinct.
    - Largest group of identical buildings: 23 on average.
- **Task 8 (versioning):**
  - `SAVE_VER` is 2. The loader still reads v1. Saves record the world-gen version and a world fingerprint.
  - `WORLDGEN_LATEST` is 2: dens, gated on `ver >= 2`, using their own RNG stream.
  - `tools/save_test` loads the checked-in v1 fixture (`tests/fixtures/save_v1.bin`), checks it field by field, round-trips v2 saves and rejects corrupt ones.
  - Old saves keep their v1 world, which has no dens.
- **Task 2 (feel and balance):**
  - Per-species behaviour:
    - wolves flank and lunge;
    - goblins surround, then flee;
    - skeletons can reassemble;
    - archers keep their distance;
    - bears and trolls telegraph a slam that the player rolls through.
  - About 60 dens per world, with leash and reset. A cleared den gives XP and stays empty for 3 days.
  - Feedback: a stamina flash, perfect-roll slow motion, a pickup sound and a bigger level-up.
  - Rebalanced HP regen, XP curve and monster scaling.
  - `rpg_test --metrics` gives:
    - first level-up at 2.6 min;
    - level 5 at about 32 min;
    - 3 wolves kill a potionless player 10 to 26 % of the time;
    - 3 to 4.5 hits to kill a wolf, boar, goblin or skeleton, and about 11 for a bear or troll.
- **CI:** `web.yml` now runs `rpg_test --seeds 1..20` on Linux. `save_test` runs there as a non-blocking step, because the fixture's world hashes were recorded with MSVC and may differ under glibc's libm. Make it blocking once it is green.

**Remaining:**
- The bot still dies about 6 times per 40 minutes, because it wanders into camps and over-levelled zones. Its routing needs work.
- Potion prices are untouched.
- Dens don't count toward the POI audit yet.
- The "scripted playthrough passes on 5 seeds" check and the Windows `ci.yml` are not wired up.
- No Emscripten build was run locally (no emsdk or clang on this PC). Code was reviewed by hand, and the next push's `web.yml` is the real check.
- Tasks 3 to 7 are untouched. The repetition-audit numbers above are their baseline.

**2026-10-03: M0 phase A (lead) is done** (VISION_PLAN §13 M0). Behaviour is unchanged: `rpg_test --seeds 1..20` gives the
same numbers as before, `art_hash` is identical, `save_test` prints ALL OK and both example scripts pass.
- `rpg/art.cpp` is split into `rpg/art/*.cpp`, with shared helpers in `rpg/art/art_internal.h` and one public header per
  area (`art_human.h`, `art_props.h`, `art_building.h`...; `rpg/art.h` includes them all). `tools/art_hash` hashes every
  sprite by category, and the split is pixel-identical.
- The AI is in `rpg/sim/ai.cpp`, the player's look and stats in `rpg/sim/player.cpp`, `propSolid` in
  `rpg/sim/prop_rules.cpp`, and the renderer's prop traits in `rpg/view/prop_traits.cpp`. `tools/rpg_sim.cpp` is now
  `tools/tests/*.cpp`, with a per-seed hook file for each lane.
- New shared state: `Actor::faction` (`factions.h`), `Map::deco` (`deco.h`), `ItemKind::Gloves/Boots/Cloak` with
  `eqGloves/eqBoots/eqCloak`, `HumanLook` equipment fields with `key()`, `Appearance`, `Background`, `Game::storyFlags`,
  `Mode::Creator`, `rpg/culture/style.h` (`ArchStyle`), and the view entry points for the creator, paper doll, deco and
  markers.
- **SAVE_VER 3** appends a character block (the new slots, background, story flags and a length-prefixed appearance).
  The fixtures are `tests/fixtures/save_v2.bin` (generator v2) and `save_v3.bin`. **WORLDGEN_V3** is reserved for the
  M0 interiors and the building/wall pass.
- Prep for M1: `rpg/world/{coords,ids,dmath,noise,jobs}.h`. `rpg_test --golden` pins them against
  `tests/fixtures/golden_world.txt`.
- `tools/slot.sh` is the machine-wide 3-slot lock. Wrap every build, game run and test in it.

### M0 integration (2026-10-03)
- All four lanes (arch, homes, hero, town) are merged and build clean with BDIR=build. Results: rpg_test --seeds 1..20
  20/20, save_test ALL OK, --golden ok, --metrics first weapon avg 6 s, and all 12 tools/scripts exit 0.
- Integration fixes:
  - gatehouse towers were clipped at gy 18 (ground showed through their lower fronts), so the canvas is now 80 px tall;
  - gate flanks joined to the ring only diagonally keep their wall piece (no ground sliver at the tower);
  - thatch texel calmed so hip/gable planes read as lit volumes;
  - flat roofs get a parapet shadow band, and adobe decks sit a step lighter than their walls;
  - quest "!" markers are drawn after the night pass, the alarm bell forces combat music, and the world pin is hidden
    under a giver's "!" bubble;
  - the blade hand-off line names the actual tracked follow-up quest;
  - Bell was added to audio_preview, and interiors.txt now starts itself.
- **Freeze WORLDGEN_V3 now** (the arch wall pass and the homes genInterior both gate on it).

## M0b, "Rooms and Storeys" (owner notes 2026-10-04; binds VISION_PLAN 15.7)

Goal: interiors that make logical sense and feel lived in. Multi-storey buildings (inns, keeps, towers, larger houses and
shops) get real upper floors reached by stairs; inns keep guests upstairs in rented rooms with walls and doors; every
room has a purpose; layouts vary per building and per regional style; interior walls, doors and stairs reach the
commercial 16-bit bar.

### Phase A (lead), 2026-10-04: done
- **WORLDGEN_V7** (`world.h`): `Bldg::storeys`, `Bldg::hearth` and `Bldg::biome`, decided at generation on their own
  hashed "storeys" stream (`bldgStoreysV7`, `bldgHearthV7`): inns and keeps 2 storeys, mage towers 3, houses of width 5
  65 % / width 4 40 %, stone houses 60 % (narrow ones 30 %), shops of width 4+ 70 %; temples and towers have no hearth,
  lock-up shops 60 % none. `bldgRiseTiles(type, storeys)` lets 2-storey houses and shops rise 4 tiles in the V5
  clearance test. `Bldg::floors()` is `storeys` from V7 and 1 before. Older worlds are unchanged (storeys = the type's
  classic look, `art::defaultStoreys`).
- **Rooms contract** (`rpg/sim/rooms.h`): `Stairs`, `RoomKind`, `RoomInfo`, `Map::floor/up/down/rooms/roomAt`, and the
  geometry contract (2-row E-W partitions, 1-column N-S partitions, 1-tile doorways with `DoorH`/`DoorV`, `StairsUp`
  under a wall face, `StairsDown` stairwells, spawn slots 16*floor+k). `genInterior(m, b, seed, floor)` dispatches
  `genInteriorV4` (a PHASE A STUB for the rooms lane to replace) for V7 buildings; V2 and V3 rooms are frozen and
  hash-locked (`kGen2Hash`, new `kGen6Hash` in test_interiors.cpp).
- **Game**: `Game::subFloor`, per-floor `mapKey()`, stairs trigger (walk onto the steps), `changeFloor`,
  `debugEnterBuilding(bi, floor)`, `Lodging` (the rented room) with `lodgingActive()`, the "- UPSTAIRS" label.
- **SAVE_VER 4**: a length-prefixed lodging block (floor, rented building/floor/room/until-day). Fixture
  `tests/fixtures/save_v4.bin` (seed 707, generator v7, upstairs in the start inn with a rented room) with
  `GENLOCK_V7` and a storeys/hearth hash; every older fixture still loads and round-trips (cut back with `asV3`).
- **Art API**: `art::BuildingFacts {storeys, hearth}` into `buildingSprite`/`buildingHeight` (the view passes
  `bldgFacts(b)`; `hearth == false` already removes chimneys), `BuildingInfo::storeys/chimneys`; 18 new props
  (stairs, doors, nightstand, washstand, oven, prep table, bottle shelf, weapon rack, lectern, candelabra, display table,
  quench tub, grindstone, pillar, bunk bed, dresser) with placeholder painters; `RoomStyle::Adobe/Plaster` +
  `Deco::WallAdobe/WallPlaster`.
- **Scripts**: `worldgen N` header / `--worldgen N` (bounty, arch_walls*, interiors pinned to v6), `enter <type>
  [floor] [nth]`, `floor N`, `expect floor N`, `walkto upstairs|downstairs`.
- Verified: rpg_test --seeds 1..20 20/20, save_test ALL OK, --golden ok, all 13 tools/scripts exit 0, a stairs tour
  (inn up and down, keep floor 1, tower floor 2 and back down) passes.

### Phase B lanes (disjoint files; shared headers frozen)
- **rooms** (build_rooms): the V4 interior generator, renting/sleeping in your room, per-type checks, tours.
- **intart** (build_intart): partition walls, doorways, doors, stairs and the new furniture at the quality bar.
- **exterior** (build_ext): exteriors that show their storeys and chimneys honestly; the agreement check.

### Integration, 2026-10-04: done
- Clean build of every target (MSVC /W3: only the old int-to-float notes). The changed sources also parse cleanly under
  clang 22 (-std=c++20 -fno-ms-compatibility -Wall -Wextra) through clang-tidy; there is still no local Emscripten run.
- rpg_test --seeds 1..20: 20 passed; --golden ok; save_test ALL OK; all 18 tools/scripts exit 0; the five m0b_* tours
  also exit 0 on seeds 19 (desert) and 10 (taiga). arch_gallery --check 1..10: ALL OK.
- save_v4.bin was regenerated (it was saved at a tile that the rooms lane's layout turned into a wall); save_test now
  checks that the fixture stands on a free tile and that its rented room is a guest room.
- kGen7Hash (tools/tests/test_interiors.cpp) locks the M0b interiors of v7 worlds: every floor, stairs and rooms, seeds
  1..3. Once M0b ships, any change to them needs a new WORLDGEN version (EMB_INTERIOR_V7HASH=1 prints the values).

### Fix round 3, 2026-10-04
- Stairs (phone flow): upstairs you now arrive beside the stairwell (`arrivalOf`, down: beside the opening first, also
  beside the flight's second tile), facing away from it. Until you have stood on another tile, a push north into the
  stairwell is turned back by its railing (`Game::stairsArrive_`, `stairsAsleep()`); a push across or south still takes
  it. `m0b_stairs_tap.txt` (climb, lift the thumb, push on: stay upstairs; inn, house, stone house, shop, keep, tower)
  and the updated `m0b_stairs_hold.txt`.
- Touch: a use tap can no longer turn into a swing (a held finger only swings while the button reads ATTACK, and a use
  wins over a swing in the same step); a short still tap on a "TAP: SLEEP" label or the person in reach on the stick's
  half is a use (`m0b_touch_use.txt`, `m0b_touch_label.txt`).
- Lodging: from 11:00 on the due day the room has no sleep left, so the innkeeper lets a fresh room (paid) instead of
  offering GO UP TO BED (`m0b_inn_noon.txt`). A gold arrow marks the stairs toward your room on other floors (clamped
  into the clear area, pointing at them, when they are under the HUD).
- Art: N-S doors drawn along the wall (head block, casing rails, foot block, full-width sill; the leaf open against a
  rail or shut in the wall's line), so the wall line never breaks; adobe wall tops shaded as a rounded mass, the
  adobe dado a deep earth red below a lime bead; inn plaques an oak board with an inset brass plate.
- Rooms: per-building seat styles and per-table variation, firewood and hay at most two per floor, no stacked wall
  pieces or clutter on the tile above a piece, no scholar's table in smithies (meal/plain table, display table,
  bench, a second work station in big forges), a gallery table and columns in the keep's upper hall, a mage's study
  on the tower's top floor, round tower floors (front corners walled on a curve).
- HUD indoors: the minimap is dropped and the location/quest column fades when the rooms run on under them.
- kGen7Hash re-recorded (M0b is not shipped yet).

## M2, "Wayfinder" (VISION_PLAN 13 M2, 15.10, 15.11; owner notes 2026-10-05)

Goal: endless parity and the zoomable map, plus the M1 carry-overs: menus that use the phone's width, travel without
a hitch, readable relief, a per-region geology, the classic island retired, livelier capitals.

### Phase A (lead), 2026-10-05: done
- **The classic island is retired** (15.10). `World::generate`, `Game::newGame`, `--classic`, `--worldgen` and the
  scripts' `worldgen N` header are gone (a script with that header is refused); the pre-M0b interior generators (V2,
  V3) went with it (every building is furnished by `genInteriorV4`). rpg_test's lane checks run on the endless world:
  interiors on the start window (kGen7Hash re-recorded), defence in the nearest endless town, architecture on the
  nearest city brought into the window, metrics arenas on endless ground. Porting them exposed M1 issues, reported as
  WARN until a lane fixes them: 3-4 townsfolk fall before the guards of a 40-60-home town arrive (SIM lane); city
  rings: an overland road or a paved square meeting the wall away from an opening on seeds 10, 12, 19, and the
  ring flood leaving the footprint on seed 19 (WORLD lane).
- **SAVE_VER 6** (`tests/fixtures/save_v6.bin`): the header drops the classic fingerprint (world kind must be 1);
  quests gain `targetId`, `hasPos`/`tgx`/`tgy` (a global objective tile), `giverId`, `subject`, `destBldg`,
  `deadlineDay`, `flags`; site flags gain 4 = rumoured (`Site::rumoured`); a `marks` block (`Game::marks`, u64 -> i32
  facts keyed by stable ids). save_test checks the new fields, refuses v5 and classic saves.
- **ENDLESS_GEN_VER 6** (goldens unchanged so far: the lanes change the output under 6 and re-record them).
- **Contracts**: `rpg/world/poi.h` (VignetteKind x9, WonderKind x4, names, `poiKindKey`, the spawn-slot contract),
  `SiteType::Vignette` / `Wonder`, `Site::kind` / `SitePlan::kind`, `Role::Hunter/Fisher/Herbalist/Traveller`;
  `rpg/world/geology.h` + `EndlessSource::geology()` (first version in `rpg/world/geology.cpp`: jittered-lattice
  provinces, rock and ore affinities) and `RegionPlan::geology`; `LandmarkPlan` / `RegionPlan::landmarks` (named
  ranges, peaks, passes, lakes...; empty until the WORLD lane fills them); `ChunkData::blend` -> `Map::blend` (the
  ecotone byte, zero until the WORLD lane writes it); `EndlessSource::landmass()` (stub: 1 for any land); 13 new
  props (`Peak .. DragonBones`, frozen footprints in `art::wildFootprint`, placeholders in `rpg/art/art_wild.cpp`,
  `art::peakVariant`); `Game::Travel` / `beginTravel` / `finishTravel` / `travelQuote` (stub: the old immediate fast
  travel) and `View::travelArrive`; `QType` Deliver / Heirloom / Missing / NamedBandit / Protect; the MAP tab moved
  whole into `worldmap.cpp` (`drawMapTab`, `mapTabTap`, `mapTabKey`).
- **Tests and scripts**: `rpg/view/script_api.h` lets every lane register script commands and `expect` checks from
  its own `rpg/view/*.cpp` (`embervale --script-help` lists them). `rpg/view/script_test.cpp` adds `gate`, `taplabel`,
  `givequest`, `gotosite`, `fightquest`, `expect sidequest`; `walkto/talkto giver`. The classic-pinned scripts
  (arch_walls*, bounty, opening, dialogue_tap, m0b_touch_label) were ported. The repetition audit counts POI kinds
  (site types, vignette and wonder kinds, dens) within 120 and 60 tiles of the start village (targets 8 and 3).
- Verified: every target builds (MSVC), rpg_test --seeds 1..20 20/20, save_test ALL OK, --golden ok, --endless and
  --endless --golden ok, --towns and --towns --golden ok, --window ok, all 40 tools/scripts exit 0.

### Phase B lanes (disjoint files; shared headers frozen, one owner each where a lane must touch one)
- **world** (build_world): vignettes, wonders, the start guarantee, peaks, ecotones, geology, landmarks, capitals'
  squares, relief cleanup, city-ring warnings.
- **sim** (build_sim): travel behind the fade, the landmass rule and carriages, new quest types with a text grammar,
  radiant targets over region plans, rumours, vignette life, town defence in big towns.
- **view** (build_view): the zoomable map (hillshade, glyphs, labels, markers, rumours, travel panel, geology debug),
  ecotone dithering, readable cliffs, the M2 art, the arrival bake.
- **ui** (build_ui): menus, shop, level-up, dialogue, creator and settings that use the full logical width and safe
  area; the script suite.

### Lane results, 2026-10-05 (recorded at integration)
- **world**: 9 vignette kinds (`rpg/world/vignettes.cpp`, avg 3.5 per land region), 4 wonders on a 640-tile lattice
  (one forced within 400 tiles of the start), the start guarantee (poi kinds within 120 tiles of the start village:
  avg 10.4 / min 10, target 8; within 60: 3 on every seed), peaks on ridge crests, ecotone blend bytes, geology
  provinces (primary ore copper 22 %, tin 15 %, iron 16 %, coal 33 %, silver 9 %, rare 3 %), real `landmass()`,
  landmarks (`rpg/world/landmarks.cpp`), capital plazas (largest empty paved block 7; at least 24 people round a
  capital's square), relief slivers 0-3 per window (FAIL above 8), city-ring checks back to FAIL. Goldens re-recorded.
  **Decision for the lead:** toll bridges may also stand on streams (a plank bridge instead of a ford), against the
  Phase A rule "at least 2 tiles wide"; without it toll bridges were almost absent (now ~70 over 20 seeds).
- **sim**: travel behind the fade (`travel.cpp`: gather over frames, cheap arrival, clock moves on), the landmass rule,
  carriages (60 tiles/h, 0.1 gold a tile, min 5), the 5 new quest types (`quests.cpp`), rumours and wayside life
  (`wayside.cpp`), town defence FAIL restored (big towns lose at most 1 villager to 3 wolves), capital squares.
  `test_interiors` now hashes a frozen sample of 35 building inputs (it only changes when the rooms generator does).
- **view**: the zoomable map (`worldmap.cpp`), geology view (G / `mapgeo 1`), ecotone dithering, one face per drop,
  graded snow levels, the 13 M2 props, the arrival bake budgeted across frames, discovery banners.
- **ui**: every panel uses the safe area, tab strip, wider ITEMS / QUESTS / SHOP / DIALOGUE, the HERO tab, travel
  screen, far-quest arrow, wayside markers.

### Integration, 2026-10-05: done
- Clean build of every target (MSVC /W3: only the old int-to-float notes). A clang 22 frontend pass (clang-tidy,
  -std=c++20 -fno-ms-compatibility -Wall -Wextra) over every rpg/, engine/ and tools/ source, plus the
  `__EMSCRIPTEN__` paths of game, stream, travel, terrain and pix, found one hard error that MSVC only warned about
  (C4838: a non-constant int narrowed to float in a braced list, `art_wild.cpp` graveCairn); fixed. No local emcc.
- rpg_test --seeds 1..20: 20 passed; --golden, --endless, --endless --golden (36), --towns, --towns --golden (30),
  --window, --wayfinder 1..20, --geology, --landmarks, --specialties 1..20, --quests 1..10, --mainquest 1..10: all 0
  failures. save_test ALL OK. All 51 tools/scripts exit 0 (run 3 at a time through tools/slot.sh).
- Fixes: `dialogue_tap.txt` taps the FAREWELL band at y 249 (the new bottom-anchored dialogue panel); cliffs' east /
  west drops are a 5-7 px side wall (lit when west-facing, shaded when east-facing) instead of a 3-px hairline (stairs
  keep their 3-px cut); the map no longer overprints two forced labels (the selected place and the quest); the HUD's
  location line cuts long names between words and drops a dangling "ON THE"; the arrival notice says "A DAY" /
  "TWO DAYS" by the real hours (23 h no longer reads "AFTER DAYS").
- Open: north-facing drops on snowfields still read as a thin rim (by design in 3/4 view, but weak on white); the
  arrival seconds of a long journey still show 27-36 ms worst frames on desktop (`m2_sim_travel.txt --perf`, target
  25 ms); the Colossus face and the caravan are the weakest M2 props; plaza features stand on bare paving (a planter
  sprite would help); Colossus rubble blocks read square; the level-up banner's text runs past its gold rules.

## M3c, "Wildlands" (VISION_PLAN 15.15, owner 2026-10-06: "tons and tons of biomes")

### Phase A (lead), 2026-10-07: done
- The contracts are recorded in VISION_PLAN 15.16: `rpg/world/biomes.h` (Biome = family, Eco = 46 biomes with their
  profiles and resources), the eco layers through chunks / window / bake, the first-cut classifier, 37 flora props with
  stand-in art and shared tree / flora predicates, 7 wildlife monsters with `rpg/world/wildlife.h`, the audio ambience /
  mood API, `gotoeco` / `eco` / `expect eco` / `ecotour`, `rpg_test --biomes`, `flora_gallery`, SAVE_VER 9,
  ENDLESS_GEN_VER 12.
- Verified (MSVC, BDIR=build): every target builds; rpg_test --seeds 1..20 20/20; save_test ALL OK; --golden ok;
  --endless --golden (39) ok; --endless 1..10, --window 1..5, --towns 1..10, --towns --golden (201), --cultures 400
  1..5, --builder 1..12 --strict --society-strict, --forest 1..10, --biomes 1..3 --strict: 0 failures; 7 / 99 / 12345
  --secs 60 ALL OK; builder_gallery --check 0 failures; m3c_lead_biomes.txt and m2_world_peaks.txt exit 0.

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **world** (build_world): the real classifier, eco ground, eco flora placement and stamps, ecotones between ecos,
  settlements / trades / cultures / landmarks from biomes, the forest rule over every wooded and rocky biome, goldens.
- **land** (build_land): the eco terrain painting and ecotone dithering, the 37 flora props for real, peaks in the
  Wildlands' rocks, prop lights, phone bake budget.
- **life** (build_life): the 7 creatures painted, roaming wildlife and hunts, weather per eco, ambient beds and moods,
  minimap / world-map colours and legend, biome banner.
- **carry** (build_carry): the seat-of-power interior hitch, teleport / fast-travel stalls in capitals, the Sylvan
  cone roofs.

### Integration (2026-10-07): done
- Clean MSVC build of every target (only the old int-to-float C4244 noise). Fixes: the `prepinterior` check raced the
  worker thread (4000 sim steps ran in ~10 ms of wall time; it now steps slowly up to 3 s); `View::prepareInterior`
  pre-bakes / pre-paints the door ahead's interior (seat entry 6.7-12.8 ms over the 6 capitals, was 37-63 ms);
  hyenas / lurkers / yetis / ember hounds bleed, hyenas and ember hounds follow the wolves' nerve rules; `geology.h`
  includes `<cstddef>`; `killquest` finishes hunts. Scripts updated for the new generation: m1_trades (fishmonger and
  smelter by `gotospecialty`, watermill dropped), m3_arch_capital (seed 7's capital has fewer gates), sim_kingdom (a
  KHAN now), sim_rooms (the inn's rooms are on the ground floor), towns_tour (no upper floors), opening (a pen at the
  village's south edge).
- Verified: rpg_test --seeds 1..20 20/20 (x6 runs); save_test ALL OK; --endless 1..10, --window 1..5, --towns 1..10,
  --cultures 400, --builder 1..12 strict, --biomes 1..5 strict (46/46), --forest 1..10 (402 windows, 0 touching pairs),
  --specialties 1..20, --golden, --endless --golden (39), --towns --golden (201): 0 failures; 7 / 99 / 12345 --secs 60
  ALL OK; builder_gallery --check 0 failures; all 84 tools/scripts exit 0.

## M4, "Banners" (VISION_PLAN 13 M4, 15.3, 15.6, 15.8, 15.9, 15.12, 15.14; owner notes 2026-10-07)

Goal: kingdoms and the living world v1 that a phone player can read: kingdom guards in their colours protecting member
settlements (frontier villages: militia only), monsters raiding towns met by guards and militia, slow and realistic
diplomacy with rare, foreshadowed wars, sieges you can join, burned villages and refugee camps, conquered towns under
new banners, rumours and news, the quest and campaign engine with its first campaign, ruins with a true history. Plus the
M3c carry-overs (web seat-entry hitch, boardwalk diagonals, heath cover, meadow map colours, alpine flora, far-zoom beach).

### Phase A (lead), 2026-10-07: done
- The contracts are recorded in VISION_PLAN 15.17 (realm.h, source.h settlementsIn, Site::homeKingdom /
  World::setSiteOwner, Bldg::charred, roles and Faction::Army, Game hooks per lane file, story.h, the M4 props,
  SAVE_VER 10 / ENDLESS_GEN_VER 13, `rpg_test --realm`, the `realm` script commands). tools/slot.sh already existed.
- Verified (MSVC, BDIR=build): every target builds; save_test ALL OK (fixture v10; damaged-save loads no longer plan
  regions for garbage ids beyond World's End, which had stalled the test for minutes once the layout grew); rpg_test
  --seeds 1..20 20/20, 7 / 99 / 12345 --secs 60 ALL OK, --golden, --endless --golden (39), --towns --golden (201),
  --endless 1..10, --window 1..5, --towns 1..10, --realm 1..10, --cultures 400 1..5, --builder 1..12 strict,
  --biomes 1..3 strict, --forest 1..10, --specialties 1..20: 0 failures; builder_gallery --check 0 failures; all 85
  tools/scripts exit 0 (m4_lead_realm.txt included). Realm genesis round the start costs 1.3-3.8 ms (9 kingdoms).

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **realm** (build_realm): genesis at full scope, the history pre-roll and ruin records, the slow daily tick and its
  tuning (`--history`), renown hooks, the save block and fixture.
- **wards** (build_wards): kingdom guards and patrols, town defence, overlays (charred, garrison towers, siege and
  refugee camps), siege quests, the war and charred-building art.
- **story** (build_story): the quest DSL, caster and first campaign, story quests, rumours / news / heralds / notice
  boards, lore in ruins.
- **view** (build_view): borders, war markers, the border banner, News and Lost History in the journal, war props in
  their colours; the M3c carry-overs and the goldens.

## M5, "Hearth and Hall" (VISION_PLAN 13 M5, 10, 15.2, 15.12, 15.18; owner notes 2026-10-07)

Goal: towns live by the clock and by their needs. Needs-driven citizens (hunger, rest, social, faith, money) choosing
inside their job's schedule windows; residents buying food and goods from the settlement's real stock so shortages feed
the M4 famine -> trade collapse -> war chain; a visible settlement mood (content: full taverns, music, festivals,
children playing; hungry or war-torn: beggars, shuttered stalls, brawls, emigrants); households, friends greeted by
name, grief; LOD (full actors near the player, hourly aggregate for the rest, the realm's daily tick far away; 120
residents <= 1.5 ms on iPhone web); the player's hooks (feed, employ, supply, befriend; radiant quests from unmet
needs); 15.2 hunger and sleep as light buffs (Well Fed, Rested; mild Hungry, Weary; inn beds upstairs, home, bedroll;
Survival stubbed); interiors populated by schedule, NPCs using furniture, village animals, lamplighting, stalls
opening and closing, window light by occupancy, the bard's tavern music in the culture's style, the alarm bell on night
raids. Plus the leftover: the web seat-of-power entry hitch under EMB_WEBSIM=1.

### Phase A (lead), 2026-10-07: done
- The contracts are recorded in VISION_PLAN 15.18: `rpg/sim/life.h` (census, jobs, templates, needs, plans, moods,
  player buffs, lamps, the Life store and its save block), the Game hooks and Actor fields, `QType::Supply`,
  `DLG_LIFE`, `Realm::lifeReport`, `rpg/art/art_life.h` (postures, critters, bubbles, festival dressing; stand-ins),
  `Music::Tavern` and four animal / crowd sounds, SAVE_VER 11 (fixture `tests/fixtures/save_v11.bin`, regenerated:
  the stale main-quest wording of save_v10.bin is gone), ENDLESS_GEN_VER 14, `rpg_test --life` (CI), the `life`,
  `buff`, `expect life`, `expect buff` script commands, `tools/scripts/m5_lead_life.txt`, `town_day_night.txt`.
  tools/slot.sh already existed.
- Verified (MSVC, BDIR=build): every target builds clean (only the old int-to-float notes); save_test ALL OK (fixture
  v11); rpg_test --life 1..5 0 failures (start villages 34-46 residents, all asleep at 2:00, 33-40 at work at 10:00,
  8-13 in the tavern at 20:00; census build <= 1.3 ms, a 745-resident city's included; 120 residents' plans ~0.001 ms
  per hour); rpg_test --seeds 1..20 20/20, 7 / 99 / 12345 --secs 60 ALL OK, --golden, --endless --golden (39),
  --towns --golden (201), --endless 1..10, --window 1..5, --towns 1..10, --realm 1..10, --history 2000 1..20, --wards
  1..10, --story 1..5, --quests, --mainquest, --cultures 400 1..5, --builder 1..12 strict, --biomes 1..5 strict,
  --forest 1..20, --specialties 1..20: 0 failures; builder_gallery --check 0 failures; all 109 tools/scripts exit 0
  (m5_lead_life.txt and town_day_night.txt included).

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **citizens** (build_citizens): the census for real, needs and utility scoring, the economy (purchases, production,
  prices, shortages along trade links, the realm report), moods and their flags, festivals, relationships and grief,
  emigrants, LOD and the 1.5 ms budget, night raids by monster pressure, Supply quests from unmet needs, the life block
  and fixture, `--life` thresholds.
- **townsfolk** (build_folk): the residents as actors (spawned by plan, door-to-door routes, interiors by schedule,
  furniture and postures, tavern seating and service, the bard, chat pairs, greetings by name, barks, gossip, grief,
  beggars, brawls, emigrants on the roads, children's tag, guards' shifts, the lamplighter, village animals), the
  player's hooks in dialogue, the 15.2 buffs (inn beds upstairs only, bedrolls), key quest people kept reachable.
- **art** (build_art): every posture painted on the rig (seated, eating and drinking, tools, sleeping in bed,
  instruments by culture), the seven village animals, speech bubbles, festival dressing, life_gallery.
- **view** (build_view): drawing postures, animals and bubbles; lamps by the lamplighter's round; stalls and festival
  dressing by mood; window light by occupancy; the visible mood and the buff icons; the bard's tavern music, animal and
  crowd sounds, the raid bell; the web seat-entry hitch; town_day_night.txt reviews.
- **integration (2026-10-08)**: clean build of every target; all suites green (see VISION_PLAN 15.18 addendum); `--folk`
  in CI; children drawn shorter; PET prompt for animals; animal voices de-duplicated. Open: art seat/bed numbers are
  mirrored in rpg/sim/life_game.cpp (rpg_sim does not link rpg_art) and must follow art_life.cpp; occasional >16 ms
  building-paint steps on web seat entry (art::stepBuilding); no local clang/Emscripten compile (clang frontend parse
  via clang-tidy was clean).

## M6, "Steel" (VISION_PLAN 13 M6, 7, 15.11, 15.19; owner notes 2026-10-08)

Goal: arms, armour, items and enemies with real numbers and a strong look: item level and bands, level sync, the rarity
budget, affixes, legendaries and attunement, mitigation, XP decay and the level-gap penalty, potion cooldown, merchant
gold, the shop-band and quest-reward fixes; the universal tiers leather -> bronze -> iron -> steel with each culture's
alloys on top, regional ores, crafting on the economy's own chains and learnable culture secrets; elites with affixes,
champion packs, named uniques with rumours, boss phases, roaming world bosses that raid towns; soldiers and bandits in
their culture's arms; harpies and golems; every culture's arms distinct on the paper doll and in the world.

### Phase A (lead), 2026-10-08: done
- The contracts are recorded in VISION_PLAN 15.19 (items.h, gear.h, craft.h, foes.h, gear_look.h, the art contracts,
  View::itemTex / monsterTex, the Game hooks and Actor fields, SAVE_VER 12 / ENDLESS_GEN_VER 15, `rpg_test --gear`, the
  M6 script commands). tools/slot.sh already existed. The M5 metrics baseline is tests/fixtures/metrics_m5.txt.

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **numbers** (build_numbers): the 7.x numbers wired into play and tuned, shops / rewards / merchants, potions, the
  loot audit and --power-curve, the crafting front end (forge dialogue + thin screen) and secret learning.
- **foes** (build_foes): elites, champions, named uniques and rumours, boss phases, world bosses and raids, harpy and
  golem behaviour and dens, soldiers and bandits in culture arms.
- **arms** (build_arms): the ArmsStyle grammar on the rig (helms, bodies, shields, blades, polearms, bows, alloys and
  sheens), culture item icons, the paper doll, the 10-culture armour gallery.
- **beasts** (build_beasts): harpy and golem art, variant overlays, elite auras, the view of ranked foes (auras, name
  plates, boss bars, scale), and the optional garrison tower / curtain wall polish.

### Integration, 2026-10-09
- Clean build of every target (MSVC, BDIR=build): 0 errors; the warnings are all C4244 int/float conversions (none
  real). Fixes: Game::xpForNext() now returns gear::xpForNext; a dragon world boss no longer drops Ashfang's crown
  and greatsword (a world boss's hoard instead); hud.cpp skips its bottom boss bar for world bosses (render.cpp draws
  theirs at the top); audio_preview names the four M6 Sfx (static_assert on the table); the new Sfx clear the music by
  6 dB on a phone (trims, midrange in BossRoar / GolemSlam); the duplicate `bossphase` script command (BEASTS and
  FOES both registered one, so which ran depended on static-init order) is split: FOES' is now `bosshp <pct>`;
  `kit6` / `gear6 equip` no longer leave a stale look when attunement refuses a legendary; foeStep warms named
  uniques / world bosses one cold generator per tick (9-25 at once hitched a step by 40-80 ms); CI runs
  --power-curve, --loot-audit, --foes 1..3 and --arms 1..3.
- Verified: rpg_test --seeds 1..20 20/20; save_test ALL OK; goldens (8 / 39 / 201) ok; 0 failures in --endless,
  --towns (alone; under load its 25 ms build budget trips), --window (alone), --realm, --history, --wards, --story,
  --life, --folk, --gear, --quests, --mainquest, --cultures, --builder --strict, --biomes --strict, --forest,
  --specialties, --foes, --arms, --power-curve, --loot-audit, 7 / 99 / 12345 --secs 60; all 131 tools/scripts exit 0;
  audio_preview all checks passed; art_hash TOTAL dc421234dd3f2fe6 unchanged.
- Open: `--metrics` vs M5 fails 2 of 8 lines (first level-up +35 %, level-3 wolf deaths 17 -> 5 %): an owner call on
  the 7.x numbers (enemy damage x1.09 / +4 HP per level) or a re-recorded baseline; cold world-boss generation can
  still cost ~35 ms in one step (far macro sampling); steel kits of Highland / Marsh / River read alike; no local
  clang/Emscripten compile (manual portability review only).

### Finish fixer (round-3 review), 2026-10-09
- **First-hour numbers (decision).** Owner direction: a slow rags-to-riches economy, but the first hour must feel like
  M5. The economy (prices, shops, loot bands, xpForNext from level 3 on) is untouched; two early-only dials bring the
  `--metrics` gate back inside +-10 % of M5: `gear::earlyXpMul` (kill XP x1.35 at level 1, x1.15 at level 2, x1 after;
  the game's kill XP and `gear::killXp` both apply it; xpForNext(1) stays 120) and the **early bite** in
  `gear::enemyDamageMul` (+0.09 a level for the first four levels above 1, then held: D 3 x1.36, D 5 x1.72, D 50
  x5.77 vs x5.41). Result: first level-up 204 s vs M5 192 (+6 %), 3 wolves died 25 / 12.5 / 7.5 % vs 25 / 17 / 12
  (gate PASS); --power-curve and --loot-audit unchanged in outcome (0 failures).
- Soldiers: every people's common soldier wears its SIGNATURE helm and body (census::dress, looks.cpp table): fjord
  horns, highland bonnet (no helm), heartland nasal, imperial crest, dune aventail, steppe spired spangenhelm in
  lamellar, marsh boiled-leather kettle, jade mask, river bright kettle in brigandine, sun-temple plume, sylvan crest
  over leaf, starspire wings; dark hose so the legs part from the harness. The town watch wear the short cape (the
  long livery cloak is the royal guard's).
- Monster overlays: plates on an upright beast (troll, golem, yeti, the humanoids) are a mantle fitted to its own
  outline (pauldrons, a gorget, lames parallel to the contour, its own top-left light, a cast shadow; no belly strap);
  quadrupeds keep the saddle plates. Harpy: a pale cool skin ramp, a violet-black mane, a dark pupil, a smaller hair
  crown so the face shows.
- Ground and props: a mountain town's snowline cover no longer lies on swept streets and squares (only the deepest
  cover in the truly cold lands); village dirt paths take the paved streets' organic edge; snowed flat decks lie in
  wind drifts; plain stall cloth (booth roof and awning) shows sewn widths, billows and a sag.
- test_gear's trust check opens the shop through the dialogue (Game::openShop is private: rpg_test did not compile).
- Open: foeStep's worst step on a 56 s overland walk is ~10 ms native (was 18): one cold region generation per tick is
  now the floor; a split generator would be the next step. The imperial watch's scutum still covers most of the figure
  at 1x.

### Finish fixer round 2, 2026-10-09
- **WYRM (must).** The draw scale is always 1, so scalePct never enlarged anything: a world-boss lurker is now the
  GREAT form (art::MO_GREAT, set by foes::variantFor): the lurker painted at 1.75x native in its own 78x30 cell
  (art::lookCellW / lookCellH; render.cpp sizes every beast by its look), standing high on straight legs with the head
  carried up, a keeled double crest, a pale banded belly, in a blackwater-slate hide with a bone crest, horns and burning
  eyes (no plates or moss). The script's `beast ... worldboss` takes the game's rank scale and form (was 200%).
- Overlays fit the body (art::overlaysFit, inline in art_monsters.h, applied by the painter and variantFor): a wisp
  takes only ember / rime, a slime no horns, plates, spikes or mask, bat / harpy no plates or horns, the shelled
  crawlers no horns or plates.
- Quadruped plates are a caparison fitted to the back contour (tapered rounded ends, lames across, lit spine, rivets,
  leather trim with brass, cast shadow, girth strap).
- Bandits wear their people's own poor kit (looks.cpp table: fur hat, bonnet, greenwood hood, leather crest, turban,
  leather spangenhelm, marsh hood, straw hat, bandana, plumed leather cap, circlet, cowl; padded or leather in the
  land's cloth; their own shield or none; a few keep a looted iron helm).
- Soldiers of dark skin with hair no lighter go clean-shaven (the sun-temple black face); the plumed helm's plume is an
  upright fan, not a brow band.
- Snowed stalls lie in per-stall drifts (setMarketSnow seed): a lit lip, soft mounds and hollows, a pooled sag.
- A worn padded coat or brigandine is the maker's cloth (as its icon); quilting is vertical channels plus a waist
  stitch, never the ramp's near-black; no studs on padding.
- foeStep makes cold named-unique regions and world-boss cells on a worker thread with its own generator (results in
  foes.cpp's memo); the web build keeps the one-slice-a-tick path. endless_walk --perf: foes no longer in the worst
  steps (worst step realm 2.1 ms; was foes 16-30 ms).
- Script header: the phone hamburger is at 581 12; tools/scripts/m6_menu_tap_phone.txt opens the menu by tap and shows
  the worn gambeson. gear.cpp turns off FP contraction by source pragma on clang/GCC (no flag changes).
- Open: still no clang/Emscripten compile on this machine.

## M6b, "Sagas" (VISION_PLAN 15.20, 15.21; owner notes 2026-10-09)

Goal: a generator that composes thousands of unique, genuinely well-written side quests, missions and campaigns:
70 archetypes at ship from scripture, myth, folklore and legend retold closely plus Lewis / Tolkien / Maas / Gwynne
THEMES (never their names, characters, places, invented terms or plots), cast from the living world (census residents,
realm rulers / wars / ruins, named uniques, world bosses, alloy secrets), a twists library, culture x motive voice
grammars, a deterministic composer feeding the M4 validator and engine, a repetition guard, 6 campaigns incl.
emptythrone, 10,000 composed stories walked in CI, uniqueness metrics and a writing-quality review lens.

### Phase A (lead), 2026-10-09: done
- Contracts in VISION_PLAN 15.21 (saga.h, the template markup, the DSL additions, the engine routing of spec ids, the
  arch tables, SAVE_VER 13 + fixture, --sagalib / --sagas, the saga script commands). tools/slot.sh already existed.
- End to end already: THE BROTHER IN THE PIT with two twists composes, validates, casts on seeds 1..3 and walks every
  path (300 stories, 0 failures, ~4.7 ms a story); m6b_lead_saga.txt plays it at phone size.

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **archetypes** (build_archetypes): 35 archetypes (scripture, myth, legend, Lewis-like, Tolkien-like) and 24+ twists.
- **composer** (build_composer): pick / offers in play, the full caster (kin, friends, mourners, beasts, bosses,
  rewards in bands, secrets), the guard, the DSL and engine, saves, --sagas at 10,000, the review sampler.
- **voice** (build_voice): the voice grammars, 35 archetypes (folk, Maas-like, Gwynne-like, world-native), the
  quality pass on the 11 hand-written tales.
- **campaigns** (build_campaigns): the chainer, 5 new campaigns with arcs, realm consequences, rumour and herald hooks.

### Integration, 2026-10-09: done
- Clean MSVC build of embervale / rpg_test / save_test / art_preview; the M6b files (rpg/story/**, script_saga,
  the saga tests, save_test, game_rpg) also pass a clang front-end parse (VS clang-tidy, -std=c++20
  -fno-ms-compatibility -Wall -Wextra) with 0 diagnostics.
- CI saga step also runs --campaigns --seeds 1..5, --voice and --voice --lane. All 138 tools/scripts exit 0;
  tour scripts m6b_int_tour_a/b/c (inn at night, outdoors by day and dusk, temple at midnight, desktop size).
- Fixes: piper opening over 300 characters; {%name} placeholders upper-cased by expand(); plea phrases no longer
  assume a journey; GUNSHOTS anachronism; HOLE UP typo; a misleading-indentation line in dsl.cpp.

## M7, "Home" (VISION_PLAN 8, 13 M7, 15.2, 15.22; owner notes 2026-10-09)

Goal: the player can buy a cottage or a (riverside) lot, build a house in any culture style they have discovered (the
M3b builder: 3/4 view, depth, seamless walls; 2+ storeys with stairs and an upper floor), decorate and store, lay out a
yard, farm ~17 crops in 4 stages through the seasons, keep chickens, goats, sheep, cows, pigs, ducks, a dog and horses
(predators raid exposed farms), hire a farmhand, cook meals (15.2 Well Fed; better at hearths and inns) from crops,
livestock, fish and the economy's goods and chains, and ride a horse down the roads (dismount rules, stabling). Offline
catch-up deterministic and cheap; property the big money sink of the slow rags-to-riches curve; phone-first build UI;
<= 4 KB per farm. Optional leftovers: ruin entrance layouts repeating across seeds, masonry ruin side walls without a
top, the steppe yurt temple's screen in the middle of the floor.

### Phase A (lead), 2026-10-09: done
- The contracts are recorded in VISION_PLAN 15.22: `rpg/sim/home.h` (+ home.cpp / home_game.cpp first cuts), the Game
  hooks, `Mode::Build`, `DLG_HOME`, the world records (`Bldg::home`, `World::placeSerial`, `World::lots`,
  `ew::PlotPlan` in rpg/world/plots.h, `ChunkData::plots`), `rpg/art/art_home.h` (+ stand-ins), the farm Critters,
  `Posture::Ride`, 16 Icons, `Prop::ForSaleSign`, the view's home draws / mounted rider / build screens (home_view.cpp,
  buildmode.cpp), SAVE_VER 14 + `tests/fixtures/save_v14.bin` (save_v13.bin removed), ENDLESS_GEN_VER 16,
  `rpg_test --home` (CI), the `home` script commands, `tools/scripts/m7_lead_home.txt`. tools/slot.sh already existed.

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **homestead** (build_homestead): property sales and taxes, builders, farming rules and the economy, animals and
  predators, the farmhand, cooking, horses and stabling, interiors' furniture, saves and --home.
- **land** (build_land): lots for sale in generation (riverside lots), the builder's player houses in every culture and
  shell (empty-shell interiors, upper floors), goldens, the ruin / yurt-temple leftovers.
- **art** (build_art): crops, farmland and paths, yard objects in culture style, scaffolding, horse and rider, the farm
  beasts, trophies, paintings, the sign, the icons, home_gallery.
- **view** (build_view): the phone-first build / decorate / storage / cooking screens, home draws in the world, the
  mounted rider, HUD prompts and buttons, the map's home marker.

### Phase B results, 2026-10-10: done (all four lanes)
- **homestead**: `home_talk.cpp` (deeds, taxes, re-registration, builders, stablemasters, recipes, the miller, the
  farmhand), `home_actors.cpp` (farm animals, the waiting horse, builders, the farmhand, live wolf raids),
  `home_interior.cpp` (furniture stamping and capture, starter bed and chest, stores, beds, cooking places);
  `home_game.cpp` rewritten (homeInteract and interactLabel share one decision). The **home block is v2** (the plot's
  furnished / starter / realm / farmhand / raid / tax-notice fields; v1 is refused). New FIX6 line in save_test:
  `{1, 20, 30, 8, 4, 1, 0, 0, 2184.000f, 2346.000f, -256, 8.867f, 1}`. Selling crops foreign to the merchant's people
  pays 25-50 % more; journeys on horseback take ~0.63x the hours; respawn at your own bed once slept in. A full farm's
  plot record is ~2461 bytes (budget 4096). Prices: hut ~765, vacant cottage 2125-2625, large lot 6300, horses
  700-2600; tax 5-35 a week.
- **land**: `rpg/world/town_lots.cpp`: every village and town sells 1-3 fenced lots (a hash of the plan's seed, never
  the shared RNG); palisaded villages place them before the palisade. Lots are **not saved** (regenerated with their
  chunks); generated plot ids use local values below 0x800 (site slot x 8 + n). Player houses in all 12 cultures x 5
  shells (`Request::home`), empty-shell interiors keeping the fixed parts (doors, stairs, windows, sconces, the fire);
  steppe townhouse / hall / longhouse redesigned, dune huts mud brick. Leftovers done: ruin entrance halls vary, the
  folding-screen nook must lean on a wall (steppe yurt temple). Goldens: towns 132 of 201 re-recorded (village / town
  cases), endless 2 of 39 (ruin layouts); --golden unchanged. New flags `--lots`, `--home-places`.
- **art**: `art_home.cpp` rewritten (17 crops x 4 stages + wilted, farmland and 8 paving looks, 22 yard objects in 13
  culture styles, 7 fence kinds with every join, scaffold in 4 stages, the riding horse in 6 breeds / 3 gaits, trophies,
  paintings, the FOR SALE sign); real Cow / Sheep / Horse critters, `Posture::Ride`, 16 farm icons (`paintFarmIcon`,
  outside `paintIcon` so the art TOTAL hash is unchanged). `tools/preview/home_gallery.cpp` (`--check`).
- **view**: `home_view.cpp` and `buildmode.cpp` rewritten (YARD / DECORATE / SHELL / STORAGE / COOK screens, touch
  and keys), house windows lit at night, yard lights, hoofbeats (`Sfx::Hoof`), the minimap / world-map house marker,
  `script_buildmode.cpp` (`build ...` / `expect build ...`).

### Integration, 2026-10-10
- Fixes: the HUD names the player's own animals' action (RIDE <NAME>, GROOM) instead of PET; the COOK screen works
  without a plot (inn kitchens, campfires); the SHELL screen shows and charges the real price (priceFactor, the
  materials brought: `ui.timber` / `ui.iron`); a house built on a lot keeps the builder's hearth / cook pot (adopted
  into `Plot::inside`); `debugLot` keeps clear of generated lots; no footsteps under hoofbeats (the RNG draw is kept);
  north-south bamboo fences (the yard's and the world's `Fence::Bamboo`) have a body; `dumptiles r d` prints deco ids (debug).
- `m6b_fix_cast_gone.txt` kills IKHUR (the herb's old resident since vacant houses for sale stand empty).
- Verified on one clean MSVC build: `--seeds 1..20` 20/20, `--home 1..5`, `--golden`, `--endless --golden`,
  `--towns --golden`, `--life` / `--folk 1..5`, `--gear` / `--foes` / `--arms 1..3`, `--power-curve`, `--loot-audit`,
  `--sagas 2000 1..5`, `--sagalib`, `--quests`, `--mainquest`, `--story 1..5`, `--builder 1..12 --strict
  --society-strict`, `--lots`, `--endless --no-budget`, `--biomes --strict`, 7 / 99 / 12345 `--secs 60`, `--towns
  1..10` (alone), `save_test`, `home_gallery --check`: all 0 failures; all 176 scripts exit 0; clang-tidy parse
  (-fno-ms-compatibility -Wall -Wextra) of every M7 file: 0 diagnostics.
- Open: `life_gallery --check` 23 failures (bare-ground seat facing up, feet leave the tile: no M7 change touches
  it, believed older); `--window` web-travel 16 ms step noise; `--towns` city / capital build budget under load; the
  steppe hall's round room shows a stray blue felt panel at the NE junction of the back wall and the curve (no prop
  or deco there: a shell piece); north-south ruin wall runs have no top (`art::ruinVariant`); townsfolk can walk
  through a yard's gate; stablemasters / millers only tested by their actions; town fields and pastures outside the
  styled area keep plain wooden fences.

## M7b, "Cities & Voice" (VISION_PLAN 15.23, 15.24; owner notes 2026-10-10)

Goal: (1) cities with real structure and order: avenues from the gates to the market and the seat, cross streets,
lanes, continuous row-house frontages, kerbed sidewalks with gutters and corner stones in true 3/4 view, the market
square, temple close, palace square and well-squares, tree-lined avenues, walled gardens, a park with paths and
benches, orchards, districts with their own density and upkeep, per-culture plans (dune souks and courtyards, jade axial
compounds, steppe tent rings, the elven city in the trees, the fjord harbour); towns only a tidier main street and
square; VILLAGES UNCHANGED. (2) Dialogue that reads naturally: one coherent line per opening, answers that follow the
option chosen, names introduced before they are used, a steady voice per speaker, fewer and fuller sentences, light
culture flavour; coherence checks in --sagas / --voice and a dialogue-quality review over hundreds of real
conversations at phone size.

### Phase A (lead), 2026-10-10: done
- Contracts in VISION_PLAN 15.24: `rpg/world/urban.h` + `Map::urb` / `ChunkData::urb` through the pipeline +
  `town_urban.cpp` (cities' first cut), `Bldg::row` / `bld::Request::row`, nine city props + `art_city.h/.cpp`
  stand-ins, `rpg/sim/talk.h/.cpp` (namespace `speech`: panel, propose / priorities, the talk memory, the recorder, the
  lint), `Game::talkMem` / `talkRec`, talkTo / dialogueChoose wrappers, SAVE_VER 15 + `save_v15.bin`, ENDLESS_GEN_VER 17,
  `rpg_test --villages` (frozen village golden), `rpg_test --dialogue`, script commands `urb` / `expect urb` /
  `cityprops`, `m7b_lead_city.txt`, `m7b_lead_talk.txt`; CI runs --villages and --dialogue. tools/slot.sh already existed.

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **city** (build_city): the city plan in rpg/world/town_*.cpp, the urban fabric, row houses, squares, parks, gardens,
  districts, per-culture plans, the towns' tidier street; goldens; --towns timing.
- **cityart** (build_cityart): kerbs, sidewalks, gutters, paving by upkeep (terrain.cpp), row-house terraces and window
  boxes (builder / art_building), the city props (art_city.cpp), the souk canopy and joined garden walls / hedges in the
  view; village art frozen.
- **saga** (build_saga): the sagas, story quests, campaigns, heralds, notice boards, rumours and lore rewritten for flow
  and clarity (rpg/story/**); coherence checks in --sagas / --voice.
- **talk** (build_talk): every other spoken line (greetings, barks, radiant / life / war quests, shop, inn, smith,
  stablemaster, guard), talkTo's composition by priority, introductions, the HUD's dialogue panel, --dialogue as the
  full gate.

### Phase B results and integration, 2026-10-10: done
- **city**: `town_urban.cpp` plans the whole city: gates planned where `cityWall` opens them (3-wide throat); 5-wide
  avenues (central drain, two carriageways, two sidewalks; straight with 45-degree chamfered bends) from every gate to
  the market and one to the seat; the seat's / palace square and the temple close (kept plots, lane round); a park in
  the rich quarter (pavilion, gravel paths, benches, trees, hedges); organic cross streets every 11-13 rows or a grid's
  N-S lanes and E-W streets; well-squares; dirt alleys in the poor quarter; terraces of 5-9 row houses (`ROW_W` /
  `ROW_E`, corner houses, `ROW_YARD` walled back yards, rich-quarter fruit gardens). Per culture: jade axis to the
  seat, sylvan narrow winding avenues round preserved great trees, fjord / river harbour spines with quays, dune souk
  canopies, steppe tent rings. Towns: a straighter 3-wide high street and a tidier square. New mask kinds `K_PARK`,
  `K_ROWYARD`, square roles, a cheaper identical `blob` comparison, the generator places the city notice board
  (places.cpp finds it). Urban classes used: Avenue, Gutter, Sidewalk, Street, Lane, Alley, Square, Close, Park,
  ParkPath, Garden, Court, Quay. 135 town / city golden keys re-recorded; the 66 village keys and `--villages` unchanged.
- **cityart**: terrain.cpp paints `Map::urb` (only where non-None): fan-laid avenue setts, cobbled lanes, worn alleys,
  wear by upkeep, raised kerbed sidewalks with lit / shaded faces and rounded corner stones, wet gutters with grates at
  squares, kerb-framed squares, closes with lawn beds, parks, kitchen gardens, quays; soft edges onto open ground.
  Row houses: builder `terrace()` (party walls, flush fronts, gables, own limewash, window boxes, yard wall), roofs stop
  flush at a party wall. The nine city props painted per culture (`cityPropStyled`), garden walls / hedges join, the
  canopy draws over walkers and fades near the hero, tree pits on paving. `city_gallery --check`; village art frozen
  (`tests/fixtures/golden_village_art.txt`, 65 groups).
- **saga**: tellers and kin introduce themselves once (talk memory), spoken hand-offs instead of bracketed journal notes,
  directions after goal stages, narrated endings shown under the tale's title, hooks by priority, news lines as whole
  sentences, voice phrases never repeated within a story; ~40 name drops, ~25 teaser repeats, 177 ". AND" joins, ~60
  fragments fixed; `--sagas` lints every walked conversation (hard gates at 0), `--review`, `--talk-report`.
- **talk**: talkToInner composes the opening by priority; first meetings introduce the speaker (light culture hello),
  later ones greet as an acquaintance; at most 6 options on a first page (services under "WHAT CAN YOU OFFER?", house
  business under "WHAT ELSE DO YOU DO HERE?", kindnesses after "HOW ARE YOU KEEPING?"); quest pitches in order (why,
  what happened, where, what is wanted, risk, pay), first-person reminders and accept lines, reward lines naming the
  gear; HUD title "SUSUT, INNKEEPER" once introduced. `--dialogue` strict by default (`--lenient`, `--saga-strict`).
- **integration**: `bldgOpenKey` mixes `Bldg::row` (stale open fronts on row houses: `--builder` 312 -> 0 failures);
  test_arch's front-cover rule allows a row house's party-wall margin; a two-tile-thick drain band drew a channel across
  every tile (now one channel along the band); the fan seams down an avenue softened; `speakerTitle()` shows a
  narrated line's tale title; identical "BUY THE ... TO THE ..." labels say their distance; "THE TOMB OF X" spoken with
  its article and named once per pitch ("THE PLACE IS..."); delivery recipients introduced by trade; word-like friend
  names ("AND") not spoken; script `walkto giver` looks again when the giver walks off (opening.txt flake).

### Fixer round 2, 2026-10-11
- **Cities**: no terraced city (a terracing people builds its city in its usual plan, else walled compounds: the jade
  city) so no terrain ledge crosses an avenue; radial spokes octilinear; a slanting band's walk knitted corner to corner
  (laid without joining the street lists); avenues jog onto the heart's row instead of running a tile beside another;
  kerbed scraps (walk with carriageway on opposite sides, runs of three or less fronting nothing) made carriageway; a
  town's walk scraps (under six tiles) made street and its street and square kept swept (upkeep 2); one stone deck
  where an avenue crosses a stream (`bridgeCrossings`); a harbour gate when a road meets an all-wet ring; a third mine
  pass for planned cities; no house whose roof would hide a stream; lamps behind a walk step onto it; junction bollards
  only where the walk runs two tiles along both streets; a capital's rich and temple-quarter houses a tile narrower, and a
  city still short of homes lets its wide houses (six tiles or more) as two. ENDLESS_GEN_VER 19, town goldens and save_v15.bin re-recorded.
- **City art**: marching-squares cuts (`msCut`, `kerbLine` in terrain.cpp) lay slanting kerbs, walk back edges and
  soft paving edges on the slant; a square's edge is the kerb's return across a kerbed carriageway's mouth; the drain
  runs straight into its grate and its joints follow a slanting channel; paving kinds meet on their built edge (no
  dithered warp between two pavings); river cities' carriageways in grey setts; calmer park gravel; the silverwood's
  moss blotches fade inside a city (a smooth lattice field); city decks take their rails from the water on their
  flanks; a thorn hedge wall parts where a river runs through; the domed pavilion is a lit hemisphere on a roof slab
  (no see-through rows); a row house's street-facing hip end carries its courses; highland / fjord pavilions thatched /
  turfed. Villages unchanged (`--villages`, `city_gallery --check`).
- **Voice**: cave names never double the adjective ("HOLLOW HOLLOW"); horse prices spoken, not bracketed; a parcel's
  recipient never shares the giver's name; a giver's own home is "HERE" / "OUR" in their pitch; a giver not yet
  introduced says who they are on accepting; beasts' lairs and a stranger's treasure hunt reworded (no phrase twice in a
  talk); hand-offs don't repeat a way just told; `urb [r] map` prints the fabric as letters (debug); the repeat lint reads phrases within one sentence; kin words and "THEY
  CALL IT X" count as introductions; --dialogue knows a name introduced in an earlier conversation.
- **Phone**: m7b_talk_phone / m7b_saga_phone re-assert the touch UI before each shot; with the touch rows the dialogue's
  options take at most half the screen (MORE pages the rest); the minimap stands down under the tall touch panel.

### Fixer round 3, 2026-10-11
- **Cities**: one build per terrace (its units share depth, storeys and kind; the first unit's roof tint, style and wealth
  for all; the builder's `terrace()` builds only the body, chimneys, plinth and yard wall to a measure of what the units
  share: wall height, depth, pitch, gable roof along the street, covering), each front its own by `Bldg::variant` (trim
  paint, shutters, creeper, window boxes, limewash, numbered along the row); a rich house's own size (its case had
  fallen through to the centre's); a split row unit keeps the row's storeys; lamps three tiles apart (two from a tree),
  no tree on a kerb's face; an avenue's bands end on one line at its square (the drain's one grate) behind a threshold
  course of setts; bare dirt is not a kerbed low; rich-quarter yards and compounds laid to lawn; the souk canopy one
  cloth (sewn widths lit and shaded, a ridge, a shaded hem on poles, no per-tile outline). ENDLESS_GEN_VER 20, town
  goldens (the 69 city keys) and save_v15.bin re-recorded; villages unchanged.
- **Voice**: the HUD font has `;`, `[` and `]`; the lint's C_CASE fails any character the font cannot draw (and --voice
  checks it); "I'M <TRADE>." is C_SELF; story hooks and givers' intros never name a guard, soldier or bandit by their
  trade; an intro leaves out the town when the hint disowns it (the exile's hint reworded); a quest giver not yet met
  says who they are before the pitch ("I NEED SOMEONE LIKE THAT"); the HUD's place / quest column shows whole above an
  open dialogue or not at all.
- **Tests**: `--towns` times its slowest build per type again (best of four more) before failing the 60 ms gate, and
  takes a bare `A..B` seed range.
- **Open**: seed 26's steppe organic city still builds 139 homes (its open ground lies in strips too narrow for a 3x2
  house); side-facing market stalls (shared with the frozen village art) and the diagonal avenues' walk-to-lane edge
  are unchanged.

## M8, "Fellowship" (VISION_PLAN 9, 13 M8, 15.6.2, 15.26; owner notes 2026-10-11)

Goal: companions who are real people of the living world and can die for good (the player cannot): tavern regulars and
other census residents (their own job, home, family, friends, needs and history), mercenaries, people rescued in sagas,
and a bonded hound or wolf raised from a pup; each with a personality, culture, background, opinions and a stake in the
world (a burned home village, a war, a lost kin). Vocations and bond skills unlocked by trust; orders from a phone-first
radial (follow, wait, attack, defend, a skill, hold back, go home); their own paper doll in visible culture gear; party
portraits; downed -> a bleeding-out window -> bound back up or dead, with grief in their family and friends, a grave and
their belongings; approval moved by the player's real deeds (sagas, war, crime, mercy) through the deeds bus; banter
(culture borders first, weather, places, each other) and personal quests generated through the M6b saga composer from
their real history, at the M7b speech bar; 25-40 % of the damage across three bands, never more than 30 tiles behind,
pathing through cities, ramps and doors, riding with the M7 horse, living at the player's house. Villages unchanged.

### Phase A (lead), 2026-10-11: done
- The contracts are recorded in VISION_PLAN 15.26: `rpg/sim/party.h` (+ party.cpp: the tables, approval, personality,
  the fighting numbers, the party block), the Game hooks and API (+ party_game.cpp: a working first cut of recruiting a
  census person, following with the catch-up rule, fighting by vocation, deeds, bond, downed / bound / dead for good,
  the paper doll's equip, wait / home / dismiss), the companions' talk (party_talk.cpp) and banter (banter.cpp) first
  cuts, `RF_COMPANION`, `Topic::Companion`, `rpg/art/art_party.h` (+ stand-ins), `Posture::Downed / Kneel`,
  `Sfx::Revive / Knell / Howl`, SAVE_VER 16 + `tests/fixtures/save_v16.bin` (save_v15.bin removed), ENDLESS_GEN_VER 21,
  `rpg_test --companion` and `--banter` (CI), the `party` script commands, `tools/scripts/m8_lead_party.txt`.
  tools/slot.sh already existed.
- Measured: the walk keeps a recruit within 3-7 tiles (no catch-ups needed) on seeds 1..3; the damage share is 0 / 0 /
  20 % in bands D 3 / 12 / 25 (the FELLOWS lane tunes it into 25..40 %).

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **fellows** (build_fellows): the companions' simulation: recruiting from the census, inns, mercenaries, rescues and
  the animal; the real stake profile; AI by vocation and stance, bond skills, potions; following through cities, ramps,
  doors, stairs, caves, journeys and on horseback; waiting, living at the player's house; downed, bound, carried, dead,
  graves and belongings; the deeds bus everywhere; the damage-share gate; the party block and the fixture.
- **voice** (build_voice): everything companions and the people round them say: recruiting, a companion's own
  conversation, opinions of the player's deeds, complaints, leaving and amends, banter grammar, cries, epitaphs, grief,
  personal quests composed by the M6b saga composer from their history, the DSL's deed and recruit effects; --dialogue
  and --banter as the gates.
- **view** (build_view): party portraits, the command radial, the companion paper doll, banter subtitles and alerts, the
  downed companion's prompt and binding ring, companions and graves on the maps, the hound and mounted companions drawn.
- **art** (build_art): portraits, the hound / wolf in three ages, graves in five rites, the Downed and Kneel postures,
  the M8 sounds, and the owner's standing visual fixes in rpg/art (ruin wall tops, the life_gallery failures).

### Integration, 2026-10-11
- The party block is VERSION 2 (FELLOWS: Member leftDay / hiredAt / mercSlot / mountedDay; Fallen rite / weapon /
  species / wasAt); SAVE_VER stays 16, `tests/fixtures/save_v16.bin` regenerated by FELLOWS (21310 bytes).
- Wiring: Event::id carries the speaker's Member::id for Ev::Banter / Ev::PartyAlert (a member who dies that step no
  longer hands their last words to whoever moved into their index); graves are drawn only once sited (GF_SITED), from
  Game::partyGraveLook plus art::graveLookFor for a person's rite; the VISIT prompt uses the sim's faced reach; the
  hound's look is Game::partyHoundLook; ruin walls draw with art::ruinVariantRun (joins N1 E2 S4 W8);
  Game::dressAsResident seeds the dress like the street body (slot LIFE_SLOT0 + idx, bldg -1), so a recruit keeps
  their face. New script pair `tools/scripts/m8_integ_tour_s3.txt` / `_s12.txt`.
- CI candidate (needs SDL in the native job): `party_gallery --check`.

## M8b, "Treasure" (VISION_PLAN 15.27, 15.28; owner notes 2026-10-12)

Goal: the loot overhaul in the owner's words: "items need to have effects if they have a rarity ... we dont over
saturate the player with a bunch of rare equipment right off the bat." A six-step ladder (Common white, Uncommon green,
Rare blue, Epic purple, Legendary gold, Mythic flaming red-orange and animated) drawn in its colours everywhere; every
step above Common has real, scaled, working affixes in plain words in the tooltip and the compare panel (Mythic: a
named power with a gameplay effect); every item source audited; item level from the player's level and the source's
danger, base power by item level and material, the affix budget by item level (a level-10 Common iron sword about a
level-2 Uncommon / Rare bronze one); rarity gated by player level (Rare ~5, Epic ~12, Legendary ~20, Mythic ~30) with
boosted or guaranteed rolls from exploration, bosses, missions and saga / campaign finales; companions by the same
rules; saved; every balance gate green. M7c (city planner) runs alongside in tailspin-m7c: M8b stays out of rpg/world,
rpg/build, view/terrain.cpp, view/worldmap.cpp, art/art_building.cpp and never bumps ENDLESS_GEN_VER.

### Phase A (lead), 2026-10-12: done
- The contracts are recorded in VISION_PLAN 15.28 (items.h: Mythic, the eight M8b affixes and Mythic powers,
  affixText, rarityName / rarityColorAt, four affix slots; gear.h: LootSrc and the DropSource rules, the gate and odds,
  item level, lootValid, affixConflicts, adoptClassic, powerScore, Live::fx; powers.h + powers.cpp: what works;
  Ev::Power, Event::q, Actor scratch, PowerOps / LootOps; the view's power-effects block + render_powers.cpp; Sfx Zap /
  Rebirth / LootChime), SAVE_VER 19 + `tests/fixtures/save_v19.bin`, `--loot`, `--loot-sources`, `--powers` (CI), the
  loot script commands and `tools/scripts/m8b_lead_loot.txt`. tools/slot.sh already existed (shared with M7c).
- Measured: ordinary drops at levels 1-4 are 54.7 / 28.4 / 12.2 / 4.7 % Common / Uncommon / Rare / Epic: the
  saturation the owner reported (NUMBERS replaces it). 727 real items (524 above Common) audited on seeds 1..3: all
  valid (the M6 generator already rolls affixes; the classic makers, randomLoot and some reward paths still give
  rarity without them: NUMBERS / SOURCES).
- Verified (MSVC, BDIR=build): rpg_test --seeds 1..20 20/20; save_test ALL OK; --loot, --loot-sources 1..3, --powers,
  --loot-audit, --power-curve, --gear 1..3, --foes 1..3, --arms 1..3, --companion 1..3, --villages 0 failures;
  --metrics M5 GATE PASS; audio_preview all checks passed; m8b_lead_loot.txt 0 failures.

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **numbers** (build_numbers): the ladder, the level gate and the source rules, item level, base power by item level and
  material, affix budgets and conflicts, Mythic naming, the classic makers routed through gear, `--loot` as the gate,
  --loot-audit / --gear / --power-curve / --metrics updated and green.
- **sources** (build_sources): every item source names itself (LootSrc) with its boost / floor / cap: bodies, chests
  and hoards, exploration, bosses and world bosses, shops and merchants, quests, sagas and finales, campaign finales,
  the forge, the start weapon; `--loot-sources` covering every source in real games.
- **powers** (build_powers): every M8b affix, the M6 TODO powers and the eight Mythic powers working in combat for the
  player and for companions (whose kits roll by the same rules), their effects and sounds at the 16-bit bar,
  `--powers` A/B fights as the gate, the time-to-kill bands green.
- **view** (build_view): the ladder drawn everywhere (names, frames, icon borders, loot beams, notices, paper dolls,
  shop, forge; Mythic animated), the tooltip with item level and plain-word affixes, the compare panel with green / red
  arrows (desktop and phone), the loot gallery and the screenshots.

## M7c, "Order of Streets" (VISION_PLAN 15.M7c, 15.M7c-A; owner notes 2026-10-11)

Goal: ONE settlement planner with an order dial. Villages (low): one spine road, houses scattered round it and loosely
facing it, the organic charm kept (before / after review of the same villages). Towns (mid): a real main street and
square, a few structured side streets with frontage, loosening at the edges. Cities (high): inside the walls a full
street network of real blocks (avenues gate -> plaza -> seat, streets off them, row houses fronting every street,
blocks with roles, gentle tilts / bends / diagonals where land or walls demand); outside the walls faubourgs along the
approach roads, sporadic houses away from them, organic farmland. Every city unique. Concept: docs/ref/m7c_city_concept.png
(the principle, not a template). Runs in the worktree tailspin-m7c (branch m7c) beside M8; merged after M8.

### Phase A (lead), 2026-10-11: done
- Contracts in VISION_PLAN 15.M7c-A: `rpg/world/town_plan.h` (Rank, BlockRole, orderFor / cityAge, PlanTraits, the
  checks and their targets), `SettlementOut::rank / role / traits` + `Gen::rankLayer` (town_plan.cpp),
  `rpg/world/town_metrics.cpp` (structure score, plan signature / similarity, village character),
  `EndlessSource::settlement(Gid)`, `bld::ROW_SIDE_W / ROW_SIDE_E / ROW_BACK`, the homes split into town_homes.cpp
  with the planned city's part behind CITY hooks (byte-identical), `Gen::cityAfter(phase)` (town_city.cpp), EDGE hooks
  `edgeBegin` / `outskirts` + `Gen::edge` (town_edge.cpp / .h), `rpg_test --structure / --city-unique / --plan`,
  `--villages` as the village-character check (M7b hashes only report changes), script `plan` / `expect structure`,
  review scripts `m7c_villages_review_s1..3.txt`, `m7c_lead_plan.txt`; ENDLESS_GEN_VER 21, save_v15.bin re-recorded.
- The pre-M7c binaries for the before / after reviews: `%TEMP%\claude\emb_M7c\before_bin\` (embervale.exe,
  rpg_test.exe, city_gallery.exe; static SDL). tools/slot.sh already existed (machine-wide, shared with M8).

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **city** (build_city): the planned city inside the walls: the street network of blocks, row houses on every
  street (corner / side / back faces), block roles, per-culture plan types, tilted sections, water and hills bending
  the grid, plazas and the seat, age and size, PlanTraits; uniqueness.
- **edge** (build_edge): the order dial's low and mid end and every settlement's edge: villages' spine and charm,
  towns' main street, square and side streets, the city's wall outline (round / oval / polygonal / squeezed by river
  or coast / following the land) and gates, the faubourg belt, faubourgs, outskirts and farmland.
- **art** (build_art): the streets, junctions and paving of the new network in 3/4 view, the row houses' new faces and
  corners, faubourg buildings, walls seamless on every new outline and gate join, the quality bar across all sprites.
- **survey** (build_survey): the world map's city plans (the concept's map view, from the real build), the checks
  (structure, uniqueness, village character) hardened against gaming, the review packs (villages before / after, city
  maps and street shots across seeds and cultures), --towns upkeep.

### Integration, 2026-10-11
- CI runs the M7c checks strict: `--villages --strict`, `--structure --strict`, `--city-unique --strict`, `--plan-controls`.
- The structure score takes a walled place's frontage over the ring's box only (the faubourgs outside the gates are
  organic by design). Row houses: a terrace's numbering steps on until none of its fronts repeats a same-size house
  of an earlier row within 14 tiles; stone / brick fronts get a lighter or darker quarrying by the house's seed
  (`--builder --strict` clones). A planned city's notice board comes down when a later step crowds it (places.cpp
  stands it at runtime). Pens keep their herders' spots reachable from the gate (`Gen::penOpen`), and finish() opens
  any pocket sealed by movable clutter (a quest giver was boxed into a pen's corner, m2_sim_quests). No one idles on
  a burned-out shell's doorstep (life_game spotOk). A bought house whose own furniture has no chest (a hut) gets one
  that can be reached (home_interior). Scripts whose seed now has one gatehouse use `gate 0`; the kerb-face check
  skips the two tiles where a kerb eases off a slant (M7c ART stairEdge); walkto knows `captain`.
- Goldens (towns, endless) and save_v15.bin re-recorded once; SAVE_VER stays 15, ENDLESS_GEN_VER 21.

### Merge with M8 "Fellowship", 2026-10-12
- Conflicts: PLAN / VISION_PLAN keep both sides (15.M7c, 15.M7c-A beside 15.26, 15.27); ENDLESS_GEN_VER 22 (both
  sides had taken 21); `tests/fixtures/save_v18.bin` re-recorded with the merged generator (SAVE_VER 18, M8's; the
  printed FIX6 is unchanged). The world, endless and towns goldens re-recorded: no value changed (M8 changes no
  generation).
- Fixes the merge needed: Game::interactTarget gives the player's own waiting horse, turned to, the prompt over a
  passer-by about as near (m7_homestead_ride: an M7c village's foot traffic took E: TALK where the rider got off; the
  script turns to her first, `home face east`); Defend's way back round a trunk (party_ai defendGoal: a step round it
  when no spot at the player's side is straight; a sprint near the leash's end with the way shut): `--companion
  --seeds 12, 17` strayed 3.4 tiles in a birch wood; `--companion`'s damage share measured over 32 waves, not 16
  (2 of 60 runs over seeds 1..20 sat at 22-24 % on the lie of the land; tuning only moved the misses; bounds kept).
- Scripts the merged world moved: `voice banter border` falls back to the region plans for a people of another family
  (seed 7 loads only the start's own); `life kill companion` (the recruit's census name changed: m8_fixer1_rules);
  m8_fixer3_rules expects the merged city inn's 1 sellsword, then 0; `war goto patrol` steps to the roadside ahead of
  a patrol walking elsewhere (the road it takes is a coin toss), and m4_wards_patrol moved to seed 13 (no road crosses
  seed 2's warring border near the seat line now).
- Open: `--metrics` vs M5 fails 3 lines on the merged world (first weapon 5.5 s vs 8.0: the start village's new
  layout; first level-up +10.2 %; 3 wolves lvl 3 died 5 % vs 17), while the pre-M7c binary passes: bot-seed noise on
  a changed world (an owner call: re-record the baseline or not).

## M9, "Arcana & Mastery" (VISION_PLAN 13 M9, 6, 15.5, 15.29, 15.30; owner notes 2026-10-12)

Goal: two pillars in the owner's words. (1) "a skill tree like in skyrim so not just for magic but also for skills like
movement speed, damage with swords axes maces ect ect each weapon type has advatages and disadvantages agaist certain
enemys get more skilled with armor types or specific culture armors or more broad skilling": every skill 1..100 by use
(10-20 h to master), the character level from skill gains, weapon families against foe tags shown in plain words,
light / medium / heavy armour trade-offs, material and culture mastery with set perks, athletics, the crafts, riding,
speech, stealth, the magic schools; perk trees that change play, drawn as constellations tappable on the phone;
trainers, books, companions who teach. (2) "i want the magic to look absoluly awesome like sick": the Skyrim schools
and the culture traditions, tomes and teachers, upcast, dual-cast, surfaces and combos, concentration, statuses,
3-4 quick-cast slots; every spell's full life on screen with real light, school silhouettes, spectacle top tiers,
inside the phone frame budget.

### Phase A (lead), 2026-10-12: done
- The contracts are recorded in VISION_PLAN 15.30: `rpg/sim/skills.h` + skill_ops.h (SkillOps hooks wired, neutral in
  numbers), `rpg/sim/magic.h` + magic_ops.h (the book, quick slots, the classic three through it with their old
  numbers, Ev::Spell phases, surfaces and combos, statuses), game.h (Input slots / sneak, Actor statuses and scratch,
  ProjKind::Spell, Ev::Spell / Ev::Skill, DLG ranges, Game::skills / arcana / skillUse), ItemKind::Book, the view's
  SPELLFX and UI blocks (render_magic.cpp, skills_ui.cpp; placeholders), SAVE_VER 20 + save_v20.bin (SAVE_VER 19 still
  loads with default skills: owner 15.29.5), `--skills`, `--spells` (CI), the script commands and
  `tools/scripts/m9_lead_arcana.txt`. tools/slot.sh already existed.
- Verified (MSVC, BDIR=build): rpg_test --seeds 1..20 20/20; save_test ALL OK; --skills, --spells, --powers, --foes,
  --arms, --companion, --power-curve, --loot, --loot-sources, --loot-audit, --gear, --villages --strict 0 failures;
  --metrics: the same 3 lines as before M9 (the M7c start village).

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **skills** (build_skills): every SkillOps hook switched on (families vs tags, armour trade-offs, stealth), the perk
  trees filled and every perk working, heritage / material / culture mastery and set perks, the crafts, riding, speech,
  trainers, books, companions teaching, the character XP of skill gains (the M5 baseline re-recorded once, with its
  reason), the full `--skills` (mastery time, the A/B time-to-kill table, armour in play, perks saved).
- **magic** (build_magic): the five schools' novice..master lists with spectacle top tiers, upcast, dual-cast,
  concentration, reactions, surfaces and combos, statuses, summons and wards, the traditions and their mainland
  signatures, mastery and attunement, tomes and teachers, the full `--spells` (+-5 % budgets, traditions within
  +-12 % of Common).
- **spellfx** (build_spellfx): the magic on screen and in the ear (render_magic.cpp + its own files, render.cpp,
  engine/audio): wind-up, launch, lit trails, impacts with light / hit-stop / shake, aftermath decals, school and
  tradition identities, upcast / dual-cast scale, animated surfaces and status overlays, the frame budget; the
  magic gallery and the day / night phone review.
- **ui** (build_ui): the SKILLS and SPELLS pages (constellation perk trees, skill progress, vs-foe lines, the
  spellbook and slots), the phone's quick-cast buttons (tap / hold to upcast / swipe to dual-cast), keys 1-4 and sneak,
  the HUD's spell and skill moments, the item cards' family / armour-class lines.

## M10, "Beyond the Mist" (VISION_PLAN 2.12, 13 M10, 15.4, 7.6, 6.2 / 6.4, 15.1, 15.11, 15.31; owner notes 2026-10-16)

Goal, as the roadmap has it: the sea, ships and hidden lands. Second continents and archipelagos, hidden lands behind
the Mist, sea POIs, ports and harbours (lane A, world); sailing with the wind, boats bought / upgraded / sunk, ferries
that play out on deck, storms, sea encounters (the sea serpent, the kraken, merrow, pirates), the sea-chart chain, the
Mist's turn-back, first contact, fishing (15.4: cast, bite, a timing / tension reel; species by water, biome, season and
hour) and the Sailor's hook (lane B, sim); the hidden signatures 2, 3, 4, 10, 11, 12 with their trials, the hidden
kingdoms' metals and gear (lane B's magic half, a lane of its own here); ships in every culture's style, the serpent,
kraken and merrow, waves, foam and mist, the isolated cultures' look, the sea's sound (lane C, art and audio). What the
owner sees on the phone: a rowboat, then a sloop, open sea, storms and islands; after a long rumour chain the Mist
parts on a kingdom nobody knew, whose priests sing fire into being. Owner notes: the every-system regression runs ONCE,
by the integrator; fixes are verified with targeted checks only; the 16-bit quality bar stands.

### Phase A (lead), 2026-10-16: done
- The contracts are recorded in VISION_PLAN 15.31: the world's lands / Mist / seas / ports (`rpg/world/source.h`,
  `rpg/world/sea_world.cpp`; ENDLESS_GEN_VER 23, no generation changed yet), hidden cultures' flags, the four sea roles,
  the three sea monsters (stand-ins), the ten sea props and the ships (`rpg/art/art_sea.h/.cpp`, stand-ins), the sea
  (`rpg/sim/sea.h`, `sea_ops.h`, `sea.cpp`, `sea_game.cpp`: boats boarded and sailed with the wind, kept to their
  waters, left ashore; the Mist's turn-back and its parting for the chart; saves at sea), `Actor::swim` and
  `Game::swimSolid`, `Input::reel`, `Ev::Sea`, `DLG_SEA`, `Game::sea`, the realm's isolated kingdoms (realm block v3),
  the view's SEA block (`rpg/view/render_sea.cpp`: stand-in ships, a Mist veil, a sail line), SAVE_VER 21 +
  `save_v21.bin` (v19 / v20 retired: old saves refused), `rpg_test --sea` (CI "Sea test"), the script commands and
  `tools/scripts/m10_lead_sea.txt`. tools/slot.sh already existed.
- Verified (MSVC, BDIR=build): rpg_test --seeds 1..20 20/20; save_test ALL OK (fixture save_v21, a save aboard loads
  aboard); --sea 1..3 OK (seeds 1-2 sail 609 / 276 px out of a port in 12 s, save at sea byte-identical; the Mist turns
  a chartless sloop back after 20.2 s of fog on all three seeds and parts for the chart; seed 3 has no coastal
  settlement within 6000 tiles of the start: play SKIPPED until the WORLD lane's harbour guarantee); --endless --golden
  39 hashes ok (no generation changed); --wayfinder 1..4, --builder 1..12 --strict --society-strict, --skills 0
  failures; --villages --strict exit 0 (its informational "196 changed since M7b" line is not from M10: phase A changes
  no village); m10_lead_sea 0 failures.

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **world** (build_world, code): the lands for real (a second continent across open sea, hidden lands set apart by a
  300-tile ring of open water, archipelagos and uncharted islets, guarantees near the start), names, the hidden
  peoples (isolated, their own alloys from a hidden-land ore), harbours in coastal towns and cities (piers, berths,
  moorings, harbourmaster, shipwright, sailors' tavern), sea POIs (wrecks, lighthouses, sea caves, pirate coves), the
  chart chain's places and the trials' sites, cheap ocean chunks for streaming at sea; `--seaworld`; goldens
  re-recorded with reasons (villages never change).
- **sea** (build_sea, code): sailing feel, buying / upgrading / repairing / sinking boats, the Sailor, swimming,
  storms, ferries on deck, encounters (serpent, kraken, merrow, pirates; the world bosses), the chart chain, the Mist,
  first contact and isolation in the realm, fishing and fishmongers, the hidden land's signature beast, the `--sail`
  bot and streaming at sea; `--sea` grown.
- **arcana** (build_arcana, code): the six hidden signatures (Blood, Rune, Song, Mask, Verdant, Dream) with their
  mechanics and on-screen identities, hidden traditions (level 25, Friendly, the two trials, the teacher, the focus
  sold only there, companions casting rank 1), the Trial of the Threshold dungeons, the priests who sing fire into
  being; `--spells` grown.
- **art** (build_art, visual): the ships of every culture (rowboat, sloop, galleon; 8 headings; the hero on deck),
  sea props, the serpent, kraken and merrow, waves, foam, surf, wakes, the Mist, storms, the isolated cultures' look,
  the sail and fishing HUD, the world map's seas and unknown lands, the sea's sound; a sea gallery and the phone review.

## M11, "High Ground" (VISION_PLAN 11, 13 M11, 2.7, 15.10, 15.29, 15.31, 15.32; owner notes 2026-10-16)

Goal, as the roadmap has it: terrain relief. Height levels, the ramp lattice, plateau minimums, waterfalls, road stairs
(lane A, world); cliff faces for every ground, slopes, aerial grading, cast shadows, waterfall animation, peaks (lane B,
view: split here into the baked TERRAIN and the live FALLS lanes); slope speed, AI pathing over ramps, map contours
(lane C, the CLIMB lane). Verification: 100 % reachability of plateaus of 24+ tiles in the sampled windows; guards and
companions path across ramps; the bake cost rises by at most 20 %. What the owner sees on the phone: hills, cliffs,
plateaus and waterfalls; climbing to a peak shrine is a journey; mountains read as height. Relief must live with M10's
coasts, harbours, piers, islands, the Mist and the hidden lands, and boats are never stranded by height. Owner notes:
the every-system regression runs ONCE, by the integrator; fixes are verified with targeted checks only; the 16-bit
quality bar and natural / organic world design stand.

### Phase A (lead), 2026-10-16: done
- The contracts are recorded in VISION_PLAN 15.32: the height byte's three new bits (`Map::HEIGHT_SLOPE`, `HEIGHT_FALL`,
  `HEIGHT_STAIR`, the `HEIGHT_STEP` mask) and `Map::reliefSpeed` (rpg/sim/world.h), ENDLESS_GEN_VER 24 and `SPF_SUMMIT`
  (rpg/world/source.h), first generation rules for stairs, falls and hillsides (chunkgen.cpp step 6), the summit props
  (`SummitShrine`, `Cairn`, `PrayerFlags`: rpg/art/art_relief.h/.cpp stand-ins, registry classified), the view's
  RELIEF block (rpg/view/render_relief.cpp: stand-in falls, the roar handed on; hooks in render.cpp and hud.cpp),
  `Audio::setFalls`, `rpg_test --relief` (walled plateaus, the census, the chunk cost against the baseline) and
  `rpg_test --climb` (the speed contract), script commands `gotorelief`, `expect relief`, `expect reliefview`
  (rpg/view/script_relief.cpp), `tools/scripts/m11_lead_relief.txt` and `m11_lead_bake.txt`. SAVE_VER stays 21; the
  fixture save_v21.bin was re-made under generator 24 (FIX6 unchanged); the endless chunk goldens re-recorded (region
  plans unchanged). tools/slot.sh already existed.
- Baselines: chunk generation 0.790 ms average over --relief seeds 1..3 (budget 0.948); terrain bake 275 ms per chunk
  over m11_lead_bake.txt (EMB_BAKEBENCH=1, 41 bakes; budget 330). Census before M11: every step one level (0 stacked
  faces in 12 windows), 0 slopes, 0 falls.
- Verified (MSVC, BDIR=build, everything built): rpg_test --seeds 1..20 20/20; save_test ALL OK; --sea 1..3 OK;
  --villages --strict exit 0; --endless --golden 39 hashes ok; --endless --seeds 1..3: reachability 0 failures (its
  one FAIL, "region plans average 3.42 ms (budget 3)" on seed 1, was there before M11: 3.41 ms at the baseline, region
  plans unchanged); --relief 1..3 0 failures (after phase A: slopes 0..1287, stairs 0..91, falls 0..35 per window, chunk
  avg 0.799 ms, x1.01); --climb 0 failures; --builder 1..2 --strict 0 failures; m11_lead_relief 0 failures.

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **world** (build_world, code): the levels for real (hills and highland plateaus that read, mountains rising in
  stacked tiers, 2-level faces on steep ground, rock only on the steep cores), the plateau minimum (24 tiles), slopes
  tuned, the ramp lattice on every face incl. stacked ones (switchbacks), waterfalls (rivers over steps, plateau lakes'
  spills, pools at the foot), road stairs and switchbacks, summit shrines with their trails, coasts / harbours / islands
  / the Mist kept whole (boats never stranded); --relief grown; goldens re-recorded with reasons.
- **terrain** (build_terrain, visual): the baked look: faces for every ground, tall stacked faces, side lips and rims
  with thickness, hillsides, earth ramps and stone stairs and switchbacks cut into the faces, the aerial grade, cast
  shadows, snow overhangs, the mountains as stacked cliffs and scree instead of the faceted rock field, the wet face
  under a fall; the bake within 1.2 x the baseline.
- **falls** (build_falls, visual): the live relief: animated waterfalls with foam, spray and mist, the falls' roar,
  cloud shadows and haze over the high ground, the peaks that read as height, the summit shrine, cairns and prayer flags.
- **climb** (build_climb, code): slope speed for everyone who walks, AI pathing over ramps (guards, folk, companions,
  mounts), falls as walls to swimmers and boats, the world map and minimap relief (contours from the final relief,
  falls and summit marks); --climb grown into the movement check.

### Integration (2026-10-16): done
- Clean MSVC build of every target (BDIR=build): 0 errors, no new warnings that are bugs (int-to-float C4244 only).
  Portability pass by inspection (no clang/emsdk on the dev PC): missing standard headers added (render_relief.cpp,
  art_relief.cpp `<cstdlib>`; script_climb.cpp, test_relief.cpp `<utility>`; engine/audio.cpp `<algorithm>`); threads
  stay under `#ifndef __EMSCRIPTEN__`; no MSVC-only constructs in the M11 files. CI (web.yml) gained the step "Relief
  and climb test" (`--relief --seeds 1..3 --no-budget`, `--climb --seeds 1..3`).
- Fixed: `--endless --seeds 1..10` failed on seeds 6 and 8 (10 relief slivers a window, at most 8): the stacked tiers
  left notches, fingers and 1-deep terraces from the last sliver pass. chunkgen.cpp step 6 now works the levels on a
  6-tile border (LB 6) with four passes, and the exact final fix-up (within a tile of the chunk) also joins 3-sided
  notches / fingers and straight 1-deep terraces: 60 slivers over the 20 windows became 43, at most 7 a window. No
  golden moved (endless 39, towns 201, region 8 ok; save fixture ok). Chunk cost +3 % (see open issues).
- Fixed scripts: m11_falls_tour's second fall ("gotofall any 1 far" found none within 25 windows of the first: falls
  are rare) now teleports to (-510,-100) first; m3c_land_ecotones' savanna/dunes edge had drifted past the 12000-tile
  search from the previous stop, so it starts from (0,0).
- Verified once: rpg_test --seeds 1..20 20/20; save_test ALL OK; every CI suite in web.yml passes (timing gates re-run
  alone on a quiet machine: an external ffmpeg at 91 % CPU had failed --towns and --window's web-travel step);
  full script sweep 317 scripts, 2 failed (both fixed above; re-run 0 failed); after the chunkgen fix the generation
  suites and the m11_/m10_/relief/water scripts were re-run: 0 failures. --relief 1..3: 0 failures, chunk avg 0.919 ms
  (x1.15 of the phase A baseline, budget x1.20). Terrain bake 274.2 ms over 41 bakes (baseline 275, budget 330).
  --climb 1..3: guards 36/36, far ways up 5/5, companions 6/6, worst search 0.26 ms.
- Open: (1) perched rivers: rivers on the compressed high ground ride a level above both banks (0..214 water tiles a
  window, now reported by --relief as "perched water tiles"); their falls are not drawn (render_relief skips a step
  with water on both sides). Cause and the two tried fixes are in test_relief.cpp next to the census. (2) dev-PC
  budgets in --endless (CI runs --no-budget): region plans 3.45 ms on seed 1 (3.41 before M11), 3.01 on seed 3; chunk
  average 1.03 ms on seed 2's window (budget 1; phase A 0.90, WORLD lane 0.99, +3 % from LB 6). (3) snow / glacier
  stacked faces barely read on white ground, and small level-5 bowls sunk into level-7 summit fields read as white
  pits ringed by ramps (seed 5 near -53,1469). (4) falls stay rare (0-2 a window; seed 5 has none within 600 tiles
  of its summit); the gotofall "any 1 far" style lookups are fragile. (5) Hillsides 25-45 % of 1-level hill steps,
  roads do not prefer existing ramps.

### Review and fixes, 2026-10-16
- Review (2 lenses, diff-scoped): 11 findings, 4 must. Fixer r1 + one re-check of only those findings; fixer r2 on the 2
  musts still open. No second full sweep (owner rule: the every-system regression runs once, at integration).
- ENDLESS_GEN_VER 24 -> 25 (fixer r1: 2-level ramps become cliffs, level border 7, earth-ramp banks) -> 26 (fixer r2:
  the land tops out at level 6 and level 7 is kept for summit crowns; packed-snow trails). Chunk hashes in
  golden_endless.txt re-recorded each time (region plans unchanged); save_v21.bin re-made under 26 (FIX6 unchanged).
- Fixed: summit shrine PRAY + blessing, 2-level ramps (0 a window, --relief fails on any), chunk-seam steps, earth-bank
  slabs, road stairs only on paved roads / summit trails / bridges, shrine slab, granite see-through band, hillside
  rims, falls kept clear of props and lit at night, world-map street-zoom hillshade and inked cliffs.
- Still open (post-M12 polish list): broad level-6 tops read flat (macro.cpp reliefLevels); the straight trench below a
  fall and rare falls (hydro.cpp river levels from coarse cells; seed 3's range has none); perched river tiles (up to
  214 a window, report only in --relief); the 1-tile road jog at a stair's foot; chunk cost x1.18 of the x1.20 budget;
  dev-PC --endless timing budgets (region plans 3.0-3.5 ms vs 3; CI runs --no-budget).

## M12, "Rise and Fall" (VISION_PLAN 13 M12, 4.3, 4.7, 7.6, 15.3, 15.6.3, 15.6.4, 15.9, 15.20, 15.33; owner notes 2026-10-16)

Goal, as the roadmap has it: the deep world sim. Civil wars and successor states, warlord realms grown from bandit
camps, resettlement of ruins, world bosses raiding towns (lane A, realm); enlisting with ranks and campaign quests,
diplomacy quests (lane B, service); village lordship and kingdom-making (owner decision 2026-10-16: YES: own a
village, pay tribute or declare independence, grow it into a kingdom; lane C, lord); what the owner sees on the phone
(lane D, view). Verification: `--history 5000`, drama metrics, `--save-soak 100` at <= 1 MB. What the owner sees on
the phone: realms that rise and fall around him, and a place in that history. Realms and wars must work with hidden
kingdoms (isolated until contact), islands, harbours and relief; wars v1 from M4 stay. Owner notes: the every-system
regression runs ONCE, by the integrator; fixes are verified with targeted checks only; all polish of earlier milestones
is deferred until after M12; the 16-bit quality bar and natural / organic world design stand. Fragile gates: the
--companion damage share (25-40 %), --history realm counts, timing gates (re-run alone before chasing).

### Phase A (lead), 2026-10-16: done
- The contracts are recorded in VISION_PLAN 15.33: the realm's records (`KingdomState::origin / overlord / tribute /
  claim / camp / player`, `Camp`, `Lair`, 16 new `EvType`s, the renown mirror `realmOf / liege`) with forcing calls and
  diplomacy rolls (rpg/sim/realm_rise.cpp) and the player's realm (rpg/sim/realm_player.cpp: grantFief,
  declareIndependence, tribute, playerTakes); the tick leaves a KO_PLAYER realm to the player (no war declared, no ruler
  death, no split; always Active); realmNoteSites notes every loaded bandit camp. The service (rpg/sim/service.h,
  service_ops.h, service.cpp, service_game.cpp: ranks with wages, merit, enlisting at captains and guards, discharge and
  desertion, orders and missions as Campaign / Envoy quests, the realm's rolls at a court) and the lordship
  (rpg/sim/lordship.h, lord_ops.h, lordship.cpp, lord_game.cpp: a village granted / chartered, daily taxes into the
  coffers, tribute every 30 days, the steward's talk, independence under the player's heraldry). `Ev::Rise` + `RiseEv`
  (rpg/sim/rise.h), `QType::Campaign / Envoy / Fief`, `DLG_SERVICE` / `DLG_LORD`, roles Steward / Warlord / Envoy, the
  view's RISE block (rpg/view/render_rise.cpp: toasts) and the journal's STANDING section slot, two props
  (`LordBanner`, `Watchtower`: rpg/art/art_rise.h/.cpp stand-ins; Prop::COUNT is now 254 of the uint8 limit 255, the
  last slot reserved), SAVE_VER 22 (service and lordship blocks; realm block v4) with `tests/fixtures/save_v22.bin`
  (save_v21.bin retired), `rpg_test --rise` and `--save-soak` (CI "Rise test"), script commands (script_rise.cpp,
  script_service.cpp, script_lord.cpp, `journal standing`) and `tools/scripts/m12_lead_rise.txt`. ENDLESS_GEN_VER stays
  26: M12 changes no generation. tools/slot.sh already existed.
- Verified (MSVC, BDIR=build, everything built): rpg_test --seeds 1..20 20/20; save_test ALL OK (fixture save_v22,
  FIX6 unchanged); --history 2000 --seeds 1..20 0 failures (unchanged: wars 0.28 avg, 278 wars all foreshadowed, 64 %
  food, hands every 124 days, falls every 909 days, rulers kept 97 %, block 47130 B, tick 0.014 ms); --rise 1..3 0
  failures (a warlord, a successor, a sergeant, a lordship, independence on every seed; saves 21.6-31.7 KB);
  --save-soak 100 0 failures (seed 1, 700 days: largest save 39281 B of the 1 MB budget, load 694 ms average); --realm
  1..3 ok; --endless --golden 39 hashes ok; --loot-sources 0 failures; --builder 1..2 --strict 0 failures;
  m12_lead_rise 0 failures.
- Seen in m12_lead_rise (seed 7, for the lanes): independence while sworn to the liege's army leaves the player
  enlisted in the army of the realm they defied (the LORD / SERVICE lanes: desertion or discharge first); the start
  kingdom, left with no settlement by the warlord and the independence, fell ("YOUR ARMY IS NO MORE"); the player's
  realm inherits its liege's government word ("REPUBLIC OF THE EMBER MARCH"); toasts stack at the left (the VIEW lane's
  herald banners replace them).

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **realm** (build_realm, code): the tick grows it all by itself under the owner's slow, realistic rules: civil wars
  that leave successor states and pretenders, wars of reunification, warlords grown from camps nobody clears (warbands
  raiding first), ruins resettled and rebuilt (and their ruins' records kept as Lost History), world bosses waking and
  raiding from lairs, alliances called to war, vassals and tribute, the player's realm as a neighbour; hidden kingdoms
  apart until contact; the rise-and-fall rumours, ruins of realms fallen in play (15.3), rise-and-fall story archetypes
  and one campaign; `--history 5000` with the drama metrics; `--save-soak` kept.
- **service** (build_service, code): orders generated from the sworn kingdom's sieges and wars (break the siege,
  assault, supply, scout, assassinate, escort, hold) played with actors, the squad that marches with a sergeant or
  captain, wages, ranks, the uniform and the oath (desertion, discharge, disbanding), diplomacy missions to courts
  (carry the terms, seek an alliance, demand tribute, greet a hidden kingdom after contact); `--service`.
- **lord** (build_lord, code): the paths to a lordship (granted to a captain, a charter bought, a ruin resettled, a
  conquest), the steward, taxes and loyalty, works raised in the village (banner, palisade, watchtower, granary,
  militia, market, shrine, walls) stamped into the world, tribute or defiance, the liege's war for its land, the
  player's realm growing by conquest, the crown; `--lord`.
- **view** (build_view, visual): the herald's banners and fanfares of the player's rise, rank insignia and the sworn
  realm's tabard, the STANDING page, the world map's rising and falling realms (warlords, successors, vassals, the
  player's realm and banner) and the realm's timeline, the lordship's works and banner on the village, warlord camps,
  resettlers and boss-raid scars, the two new props and the three new roles' looks; the phone review.

### Review and fixes, 2026-10-17
- Review (2 lenses, diff-scoped): 12 findings, 3 must (zombie player realm, realm name cut off in the HUD, broken lord's
  walls). Fixer r1, one re-check, fixer r2 on the wall gate. The r2 fixer ended before its checks; the main session ran
  them once (m12_ 16/16, seeds 20/20 MSVC + WSL clang, goldens, --lord, --service, save_test). --rise had broken: the test
  asserted the old "player realm always stays Active" contract the zombie fix changed; the test now requires a fallen
  player realm to have KingdomFell news, no land, no open war and no leftover lordship (VISION_PLAN 15.33).
- Post-M12 polish list: diagonal palisade/stone-wall runs stepped with end caps; stone-wall road gaps have piers but no
  lintel; steward menu rows small on phone layout; realm label can cover YOUR SEAT on the region map; no test reaches
  first contact with a hidden kingdom; no Warlord/Envoy actors spawn; ~67 of 150 archetypes; BALANCE: a newly
  independent one-village realm is retaken by its old liege within ~3 days of war on all test seeds.

## P1, "Fresh Roads": the first post-M12 polish batch (VISION_PLAN 15.34; owner notes 2026-10-17)

Goal: the owner's friends' playtest. Quests stop repeating (finished tales never come back, no four tales hogging the
offers, campaigns that actually start, varied jobs from what a settlement really needs, every hunt with a place and a
marker) and towns live (workplace-less trades with a real day, people walking when the player arrives, givers living
their schedule, quests the player stumbles on: overheard gossip as journal LEADS, street incidents). Not in scope:
character art size / zoom, animations, the character creator, the world-spanning main campaign. Owner notes: the
every-system regression runs ONCE, by the integrator; fixes are verified with targeted checks only; the 16-bit quality
bar and natural / organic world design stand; old saves are not a concern (refused gracefully).

### Phase A (lead), 2026-10-17: done
- The contracts are recorded in VISION_PLAN 15.34: one story picker (`story::Offer`, `tellerOffer`, `placeOffer` in
  rpg/story/offers.cpp; storyTalk, the tale scan, boards, heralds, ruins and the news call it; the M6b policy kept
  exactly), the jobs records (rpg/sim/jobs.h, jobs_ops.h, jobs.cpp: weekly `Pool`s, journal `Lead`s, `ErrandKind`;
  `Game::jobBook`; `JobOps` step / accepted / questDone / reset / loaded wired), `Game::hasOffer` moved to quests.cpp,
  `Game::addLead`, `QType::Errand` with `Game::openErrand` / `finishErrand` (marker, journal line and thanks in
  quests.cpp), the tracked quest's pin following the giver (questTarget: the live giver, then `lifeGiverSpot`, a LIFE
  stub, then the door), Mk tag ranges for the lanes, SAVE_VER 23 (the jobs block) with `tests/fixtures/save_v23.bin`
  (save_v22.bin retired), ENDLESS_GEN_VER 27, SAGA_GEN_VER 2, `rpg_test --quest-mix` and `--town-motion` (CI "Quest
  mix and town motion test", with --gates) and `tools/scripts/p1_lead_roads.txt`. THE WAR OF THE TWO CROWNS
  (`succession`, M12) was never listed in campaignPlans(): listed now (six generated plans). tools/slot.sh already
  existed.
- Verified (MSVC, BDIR=build, every target built; clang -fsyntax-only on every changed file in WSL): rpg_test --seeds
  1..20 20/20; save_test ALL OK (fixture save_v23, FIX6 re-recorded: one more quest, the errand); --quests 0 failures;
  --story 1..5 0 failures; --campaigns 1..5 0 failures (60 walks with SUCCESSION); --sagalib 0 failures; --sagas 300 1..2 0 failures; --voice 0
  failures; --save-soak 20 ok; --endless --golden 39 hashes ok; --golden ok. --quest-mix with the survey's visit set
  reproduces the survey's talk numbers exactly (2,149 story offers, 3,425 jobs, Hunt 1,066): the picker refactor
  changed nothing.
- Baselines (the lanes' starting point; DONE gates in VISION_PLAN 15.34): `--quest-mix --seeds 1..20` (1,488 visits:
  984 village, 252 town, 252 city; 257 s): finished tales re-offered 57.9 %, top-4 tales 63.3 % (PARDON alone 14,030 of
  29,591), generated campaigns started 1 of 6 (REBELLION; the library's EMPTY THRONE every seed; BURDEN, PLAGUE, FAE,
  DRAGON, SUCCESSION never offered: every board
  offers MISSING CHILD and every ruin THE CURSED GIFT; no village news story was ever offered), top-3 job kinds 75.6 %,
  farmers 30.9 %, every one of 15,238 hunts without a target. `--town-motion --seeds 1..8 --city` (31 s): walking
  15.4 % (on arrival 10.1 %; DERGZALT in the tour: 0 of 54 walking at 11:04), 692 people static for 2 game hours
  (MERCHANT 170, TAILOR 78, SCHOLAR 79, STABLEHAND 52, SMITH 45, BAKER 23 ... all WORK with no workplace; 157 generator
  spawns standing in pose none).
- Note for the lanes: the survey probe's "6 nearest settlements" came from World::nearSites, which at the start holds
  only the start village (it measured 244 visits, not 1,440); --quest-mix reads the region plans and adds the nearest
  town and city (the opening sends the player to the capital; boards stand only in towns and cities). `--no-towns`
  keeps villages as they come.

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **story** (build_story, code): the picker's policy (finished tales never return, one teller per tale per town, places
  falling through to sagas and campaigns), ruin- and event-started stories, the five blocked campaigns reachable, the
  double negative and the tone tags, story stand-ins that live a day; `--quest-mix` story gates.
- **jobs** (build_jobs, code): weekly need-driven job pools, wider trade menus, Hunt targets with markers, journal
  LEADS (rumours, the QUESTS page), errands' polish; `--quest-mix` job gates.
- **life** (build_life, code, senior): a real day for workplace-less trades (stalls, errands, carry routes, shopping,
  visiting), staggered arrival, no giver pins, generator spawns that move, overheard gossip as leads, street incidents
  as errands, pathBudget on phone-class perf; `--town-motion` gates, perf flat.
- Lane results (MSVC, `--quest-mix --seeds 1..20 --gates` and `--town-motion --seeds 1..8 --city --gates`): finished
  tales re-offered 0.0 % (13,050 story offers), top-4 tales 13.6 %, generated campaigns started 5 of 6 (BURDEN, PLAGUE,
  FAE, REBELLION, SUCCESSION; DRAGON 0: realm events near the start are too rare and its arcs all describe a raid just
  happened), top-3 job kinds 53.6 % (6,492 radiant jobs), farmers 4.6 %, hunts without a target 0; town centres walking
  33.1 % (arrival 24.8 %), static for 2 h 0, life step worst 0.6 ms.

### Integration, 2026-10-17: done
- Clean MSVC build of every target (only pre-existing int-to-float warnings); WSL clang 18 build of rpg_test clean of
  new warnings; `rpg_test --seeds 1..20` 20/20 on MSVC and 20/20 on clang; save_test ALL OK; every CI suite in web.yml
  passes (60 commands incl. --quest-mix and --town-motion gates, --window p99.9 2.19-2.91 ms, --golden, --endless
  --golden) plus --jobs, --life-p1 --hooks, --story-p1, --story-events.
- Full script sweep (335 scripts, once): 7 failures, 6 fixed: dialogue_tap / m2_phone_touch (the nearest giver now
  has five options, no second page), m4_story_board (the board pins SUCCESSION's defaced proclamation before MISSING
  CHILD), m6b_fix_cast_gone / m6b_fix_giver_dead (new `saga kill [role]` instead of resident names), m7b_saga_phone (a
  twist no longer cuts into a conversation: the omen's sign follows at once). Open: m9_lead_arcana line 29 (the
  firebolt auto-aims at the nearest frightened wolf, which now sits north behind seed 7's bamboo; the bolt dies on a
  stalk; the shared dice moved with the town's new day; not a P1 code path).
- `--dialogue 1..3` fixed (7 failures -> 0): guard salutes, the spell-lesson refusal, the Heirloom acceptance echo.
- Screenshot tour (seeds 3 desktop and 12 phone; town 09/11/22 h, inn, city 15/21 h, board, overheard lead, thief,
  hunt HUD, journal leads, map): staffed stalls, people walking and carrying, no "BOUNTY:" on non-bounty notices.

## P1b, "New Roads Content": the second post-M12 polish batch (VISION_PLAN 15.35; owner notes 2026-10-17)

Goal: 15 new job kinds, 30 new town / city requests (10 village, 10 market town, 10 city / capital incl. port-only) and
20 new one-off tales, all already designed and judged (scratchpad content/INDEX.md and the per-area specs with their
`.judge.md` notes, every note resolved), each complete, logical in the world, with its full dialogue variety, offered,
reachable and finishable. Not in scope: art size / animations, the creator, the main campaign. Owner notes: the
every-system regression runs ONCE, by the integrator; fixers verify only their fixes; the 16-bit quality bar stands; old
saves are not a concern.

### Phase A (lead), 2026-10-17: done
- One runtime for all 65 items: the story engine. The designers' NEW-MECHANICs became DSL words (dsl.h, VISION_PLAN
  15.35 table): family, settle, needs, once, cooldown, gossip; person `in` a site and `young`; site filters (beast, min,
  max; road / flag / makes and the watchtower / dwelling places cast by the ENGINE lane); stage deadline and else; goals
  escort, bout, time; conditions is / days / nodays / season / hour (`var x 0` refused); effects days, census (mood, stock,
  price, famine, festival, refugees), reward with a percent, ambush, follow / stay, hostile, calm, give heavy;
  {var.<name>}; more icons, props and harbour trades. Endings reached off a dialogue queue an epilogue shown as a closing
  scene (P1b families).
- Radiant jobs run on the engine: rpg/story/radiant.h (spec ids `job1~<kind>~<flags>~<seed>`, scriptById resolves them),
  stub radiant.cpp; "ANY WORK GOING?" asks radiant::offerFor first and starts the job with Game::storyBeginJob (the opening
  talk is the pitch with ACCEPT / DECLINE). Library sources for the lanes: rpg/story/req/{village,town,city}.cpp and
  rpg/story/tales2/lib_tales2.cpp (empty), rpg/story/lib_p1b_test.cpp (`p1b_mechanics`, family test: every new word).
- The offer policy (Engine::offerFor / offerForPlace, offerPrecheck, offerGates, nearCoast) moved from story.cpp to
  offers.cpp so one lane owns it; requests are raised apart (rpg/story/req/req_offers.cpp: requestOfferFor /
  requestOfferForPlace, asked first). Caster: `with min / max / beast`; un-cast filters refuse. life.h: `storyRefugees`.
- Tests: `rpg_test --content-p1b` (tools/tests/test_content_p1b.cpp: the families, the text rule with the longest names
  measured over 20 seeds, every script and every composed job variant played through every branch with soft-lock,
  journal, reward, census and epilogue checks, the offer gates; CI "New Roads content test"); `--quest-mix --p1b /
  --gates-p1b / --more-towns N / --ports`; `--story` walks else and deadline branches; script
  `tools/scripts/p1b_lead_mechanics.txt`.
- No version bump (no new save data). Verified (MSVC, BDIR=build, every target built; WSL clang -fsyntax-only on every
  changed file: no new warnings): --story 1..5 0 failures (65 scripts, 19,385 paths); --content-p1b 1..3 0 failures;
  --sagalib, --sagas 300 1..2, --campaigns 1..5, --voice, --story-p1, --dialogue 1..3 0 failures; save_test ALL OK;
  --quest-mix seed 1 x 2 weeks identical to before phase A; the lead script 0 failures.

### Phase B lanes (disjoint files; see the lead report for the briefs)
- **engine** (build_engine, code, senior): the runtime behind the new words: the escort follower across the world with
  its wagon and drove, the bout's yield and the player kept standing, hostile / calm, a person inside a cave / ruin /
  camp, young looks, the burden, watchtower / dwelling places and the road / flag / makes filters, prop use (beacon,
  crate...), the tracker clock, the epilogue scene's timing; screenshots desktop and phone.
- **jobs** (build_jobs, code): the radiant composer and the 15 job templates (jobs_a/b/c.md + judge notes), the weekly
  pool's radiant jobs, leads, variety (top-3 job kinds < 53.6 %); every kind offered over 20 seeds and played through
  every branch.
- **requests** (build_requests, code): the 30 requests (req_village/town/city.md + judge notes) as DSL with settle /
  needs / once site / cooldown / clocks / census outcomes, the `gossip` lines overheard as leads; every request offered
  over 20 seeds and played through every branch.
- **tales** (build_tales, code): the 20 tales (tales_a..d.md + judge notes, the FAILs rewritten), the offer policy
  (offers.cpp: new tales and requests offered, once per world, top-4 tales < 13.6 %); every tale played through every
  branch.

### Integration (integrator), 2026-10-17: done
- Clean MSVC build of every target (only the usual C4244 / C4267 narrowing warnings); WSL clang 18 build of rpg_test
  clean apart from four pre-existing warnings (magic_game.cpp enum conditional, test_life / test_towns format strings).
- Fixes: seed_run counts quests before "ANY WORK GOING?" (a radiant pitch mirrors its quest quietly); no radiant job is
  offered on days 1-2 (the opening's first job stays the hunt it teaches with, like requests); a road ambush (escort /
  follower in the stage) set off inside a settlement is held (`_amb_m/_amb_n/_amb_stage`) and falls once the player is
  out of every settlement (no raid bell in the square), dropped if the stage moved on; magic teacher's shortfall line
  turns per refusal (repeated phrase); rq_seedgrain option lines varied; THE LONG LAMP's wait line matches its 1-day
  goal; `--quest-mix --ports` searches 10 regions for a harbour (all 30 requests now offered); script helper
  nearestPerson skips animals (opening.txt handed its hunt to a dog); p1b_req_tour(_phone) expect the courier's
  arrival before completing the fight.
- CI: `--quest-mix ... --gates --p1b --gates-p1b --ports`, `--content-p1b --gates`, `--radiant --seeds 1..3`,
  `--p1b-engine --seeds 1..5`.
- Results: rpg_test --seeds 1..20 20/20 (MSVC and clang); all 62 CI commands pass (save_test ALL OK; --dialogue 0 after
  the fix); quest-mix 20 seeds: job kinds 15/15, requests 30/30, tales 19/20 (THE FALSE LIGHT force-played only),
  top-3 job kinds 49.3 %, top-4 tales 9.8 %; content-p1b 165 scripts + 261 jobs, 10,503 paths, 0 failures; script sweep
  344 scripts, 4 failures, all fixed and re-run green; 8 P1b tours (desktop + phone) 0 failures.

## P2a, "Lens and Rig": the first Clear Sight batch (VISION_PLAN 15.36, art direction docs/STYLE_P2.md; owner 2026-10-18)

Goal: players see today's people crisp and easy to find (the world at a whole-number scale on every screen, ZOOM, the
ink outer outline, the player ring, the night lift, off-screen pips, per-family effects and enemy windups); the v2 rig
(32 px people with faces and hands, every swing family, the fall and lying corpse, every posture, children, the humanoid
monsters) and the k-scaled body-holders are built and judged behind `rig 2`. Owner defaults (15.36 "For the owner to
confirm"): the UI keeps its fractional fit on Safari / 2x iPhones / the iPad while the world is whole-number everywhere;
D0 = round(h / 240) (zoom constant 240); the villages golden is re-recorded once, in P2b. No SAVE_VER / ENDLESS_GEN_VER /
SAGA_GEN_VER bump; the every-system regression runs once, by the integrator.

### Phase A (lead), 2026-10-18: done
- Layers (engine/pix.h): `Pix::Layer`, `presentLayers`, `beginWorld` / `endWorld` (View::draw wraps the world in them),
  `worldLayer`, `uiLayer`, `readWorld`. Stub: the world layer IS the UI layer.
- Screen (rpg/view/screen.h): `Settings::zoom` (-1..2) and `ring`; EMBS version 2 (a version-1 file keeps its fields and
  the new defaults); `defaultWorldScale` (D0), `zoomLevels`, `zoomLevelOf`, `worldScale`, `zoomName`, `d0ForHeight` /
  `levelsForHeight`; `describe` adds " WORLD W X H AT DX" once the world layer differs (never in phase A: the UI text is
  unchanged); EMB_ZOOM=<D>; EMB_RIG=1|2 (read by main.cpp before the first bake); `witnessScreen` (the Witness reset's
  window / safe / screen / zoom, "keep" = the last run's, as a restart reads the file).
- Projection (view.h): `View::toUi`, `toWorld`, `uiPerWorld`, `worldCam`; the world-anchored UI call sites listed there.
- Rig contract (new rpg/art/art_rig.h, art_rig.cpp): `rigVersion` / `setRigVersion` / `bodyScale` / `bodyPx` (header-only,
  so rpg/sim reads them), `RigMetrics rig(v)` (figure heights by body x age x people, head / eye / hand rows, marker,
  bubble, plate and bar heights, ring and shadow, reach and lift per family, bathing crop), `Clip`, `Family`,
  `familyOf(look)`, `FrameRect`, `RigAnchors` (head, face, hands, hips, feet, grip, tip, weapon behind: the pose data the
  gates and P2b's gear read), `RigSheet` (`count`, `frame`, `anchorsOf`, `add`), `RigPart`, `humanRig`, `humanPosture2`,
  `sleeper2`, `rig2Covers` (false), `humanFigureStill2`, `humanoidCell2` / `W2` / `H2` (statues and the three humanoid
  monsters route to them at rig 2; empty = v1), `seatFit2`, `bedFit2`, `Holder` + `holderPx` (doors and the one-storey wall
  measured so far). Lane stubs: rpg/art/rig/{rig_core, rig_body, rig_face, rig_hair, rig_cloth, rig_death} (RIG-BODY),
  {rig_arms, rig_actions} (RIG-ARMS), {rig_poses, rig_sleep, rig_kin, rig_humanoid} (POSES), rig_internal.h (rig2::rigLoco /
  rigAction / rigDeath); CMake globs rpg/art/rig/*.cpp. HumanLook gains `body` and `age` (mixed into key() only when set:
  every earlier key and golden key holds); looks.cpp sets them from the census, gear_look.cpp from Appearance::female.
- Weapon ids: the greatsword is art id 7 (`WEAPON_GREATSWORD`; gear_look's map {1, 2, 6, 5, 7, 8}; v1 paints it as the
  sword, its culture blade form kept). The herald's horn, which held id 7, is `WEAPON_HORN` = 12 (painted as before).
- Effects (art_fx.h): SwingArc, HeavyArc, ChopArc, SmashRing, StabGlint, ThrustStreak, BluntSwish, PunchStar, Twang
  (stand-ins: the Slash).
- Animation pick (new rpg/sim/anim.h / .cpp): `animPick(game, actor[, rig])` -> {clip, family, variant, frame, frames,
  t}, at rig 1 render.cpp's old column exactly; the shared timing `animSwingFx`, `animSwingProgress`, `animStrikeTime`,
  `animEnemyWindup`, `animCorpseLife`, `animCorpseCap` (today's numbers); `AnimOps` (Game befriends it). Measured: the
  enemy humans the sim spawns (bandits) wind up 0.36 s (Actor::windup, 22 frames), not the plan's 0.30 (ai.cpp's folk
  branch): A3 keeps today's spawn-weighted mean, 0.36 +- 0.01.
- Moves, pixel-identical at rig 1: the human frame pick, body draw, swing effect and the dead's offset -> new
  rpg/view/render_anim.cpp (`View::drawHumanBody`, `drawSwingFx`); the story hand cart -> new rpg/art/art_handcart.cpp /
  .h (`art::handcartSprite`, `handcartDir`, HANDCART_*).
- bodyScale plumbing (identity at 1): art_building.cpp's volume walls and floors (`bodyPx(v.wallH)`, `bodyPx(v.z0)`) and
  every door size: the cottage reads at k at rig 2 (door 23 = 0.72, wall 35 = 1.09). Seats, beds, the horse cell and the
  decks stay HOLDERS' with POSES (seatFit2 / bedFit2): scaling a holder alone would unseat today's v1 sitters at rig 2.
- The ink outer outline (art_internal.h `outlineInk`: Ink on bottom / right edges, Lit Ink = Ink 30 % toward the shape on
  top / left; never #000000) on every v1 character sheet: people (standing and postures), monsters, critters, horses,
  the hound. Sleepers in their berths keep the selective outline (their head sits in the berth's pillow). The only
  intended pixel change; `art::outlineLegacyForTests` (art_hash --legacy-outline, EMB_LEGACY_OUTLINE=1) paints the old one.
- Test hooks: Witness reset params `window`, `safe` (device px), `screen` fill|pixel, `zoom` (-1..2 | "keep"), `rig`,
  `female`; state `screen` {window, ui, world {w, h, scale, integer, d0, want, levels}, zoom, zoomName, ring, safe,
  v1Draws, peopleMB, figShare}, `player.sprite` {rig, figH, drawn, box (device px), boxH, pivot}, `player.anim`,
  `entities[].anim`, `entities[].corpse.age`; keys `minus` / `equals`; script commands / calls `zoom`, `rig`, `body`, `pose`,
  `hideplayer`, `shotpair` (two shots of one moment, with / without the hero or the ring), `ui`, `ring`; checks `expect
  screen | zoom | rig | anim | corpse`. tools/p2_measure.mjs (Node 18, no dependencies): diff, mask, blocks, contrast,
  ring, subset, luma. `--perf` prints "perf   draw avg / worst | people textures MB"; EMB_TIMING prints each sheet's bake.
- Harness (frozen; lanes report problems instead of editing): `rpg_test --rig [--gates] [--report]` (tools/tests/
  test_rig.cpp: the contract always; R2, R4-R10, P1-P3, H2 with --gates), `--rig-coverage [--seeds A..B] [--gates]` (R3),
  `--anim [--seeds A..B] [--gates] [--write]` (tools/tests/test_anim.cpp: A1 always, golden tests/fixtures/
  golden_anim_v1.txt; A2-A5 with --gates); rpg_test links rpg_art (EMB_RPG_TEST_HAS_ART retires the #include hack);
  scripts tools/scripts/p2a_lead_lens.txt and _phone, p2a_lead_perf.txt; witness/p2a_lens.witness.json; gallery
  skeletons tools/preview/rig_gallery.cpp (RIG-BODY), rigarms_gallery.cpp (RIG-ARMS), rigposes_gallery.cpp (POSES),
  holders_gallery.cpp (HOLDERS), shared tools/preview/rig_preview.h; CI step "Clear Sight rig and animation test"
  (--rig, --anim, --rig-coverage --seeds 1..3; the integrator adds --gates).

Baselines (phase A; the world is drawn at the UI's fit until LENS):
| reference screen | UI fit (kept) | D0 / levels | v1 hero figure (23 art px) |
|---|---|---|---|
| desktop 1440x810 (PIXEL PERFECT) | 480x270 @3 | 3 / 3 4 5 | 8.5 % (69 device px, shot pair) |
| desktop 1920x1080 | 480x270 @4 | 5 / 4 5 6 7 | 8.5 % |
| laptop 1366x768 | 683x384 @2 | 3 / 3 4 5 | 6.0 % |
| desktop 2560x1440 | 512x288 @5 | 6 / 5 6 7 8 | 8.0 % |
| iPhone 15 Pro app 2556x1179 (FILL, insets) | 639x295 @4 | 5 / 4 5 6 7 | 7.8 % (92 device px, shot pair) |
| iPhone 15 Pro Safari 2556x1020 | 721x287 @3.54 | 4 / 4 5 6 | 8.0 % |
| iPhone 11 1792x828 | 615x284 @2.91 | 3 / 3 4 5 | 8.1 % |
| iPhone SE 1334x750 | 480x270 @2.78 | 3 / 3 4 5 | 8.5 % |
| iPad 2360x1640 | 480x333 @4.92 | 7 / 6 7 8 9 | 6.9 % |
| Witness 960x540 | 480x270 @2 | 2 / 2 3 | 8.5 % |
- Render (p2a_lead_perf.txt, the phone window, the capital at 15:00, desktop PC, median of a phase's last 5 seconds):
  View::draw 0.49 ms at NORMAL, 0.60 ms at FAR (the same view in phase A), 0.60 ms with rig 2; frame work 0.52-0.63 ms.
- v1 character sheet bake (EMB_TIMING, 147 sheets): p50 0.241 ms, p95 0.385 ms. People textures in the capital at 15:00:
  3.8 MB (rig 1).
- Shot pairs (p2a_lead_lens, 1440x810, D3): day edge contrast 81.3 (outline ring 50 vs ground 132), the figure 47 luma
  below the ground; 22:00 edge contrast 41.5, the figure 23.5 below the ground (the night lift is LENS's); whole D x D
  blocks 81.3 % at D3 (desktop) and 78.1 % at D4 (phone): the light map, vignette and particles are not on the grid yet.
- art_hash (re-recorded: the ink outline): humans bec3b12c01605e67, monsters 607e3543a29be1ec, props b5397faadf627d55,
  buildings 2aabcae2a6bd5417, walls a0f74be3c081af13, icons 246f135187536eb5, fx e0ecd0fcb344c159, TOTAL 317eef6efe4d8229;
  humans+new 3cf412bb61e51330, monst+new 9eddb8cca73868c3, props+new e3666c4a2263a932, bldgs+new 6c7bd74c9baea831,
  icons+new d7efef6a9cdcd742, fx+new (9 sprites, new) d8bccbafd4d9fd2b. `art_hash --legacy-outline` gives every pre-P2a
  value back (TOTAL b4f10b1053879981; humans 95e9f2914ff5b496, monsters 01c007f58a695291).
- Gates today (the frozen harness, before the lanes): `--rig` contract 0 failures; `--rig --gates` R2 R4 R5 R6 R7 R8 R9
  R10 P1 P2 P3 H2 all FAIL (nothing painted on v2; H2: the doors and the wall in range, the other 15 holders not measured);
  R3 0 % (0 of 41 plain looks, seeds 1..2); `--anim` A1 identical (21,630 samples, golden 56fb08be96b73a1a); `--anim
  --gates` A2 (one effect for all, fixed 0.11 s: it ends 2.0 frames before the mace's 9-frame strike; dagger 0.6, sword
  0.4 off), A3 (every family 0.36 s; the mean 0.36), A4 (6 s, no cap; lying at once: the v1 hurt frame), A5 (no fall)
  FAIL; witness/p2a_lens 6 of 15
  pass (the UI fit, D0 and the levels on all 10 screens, the zoom call and "keep", rig / female / pose, the hooks); the 9
  others are LENS's L1 / L3: the world layer at D0 on 9 screens, the zoom keys.
- Verified (MSVC, BDIR=build_p2a, every target built; EMBERVALE_WITNESS=OFF configure builds embervale; WSL clang 18
  -fsyntax-only on every changed and new file: no new warning but art_internal.h's usual unused helpers): rig 1 is
  pixel-identical but the outline: `art_hash --legacy-outline` = every pre-P2a line, and 9 Witness shots (the village at 1
  and 4 s, a walk, the town at 21:30, a three-wolf cave fight with swings, the capital at 15:00) byte-identical between
  the pre-P2a build and this one with EMB_LEGACY_OUTLINE=1; golden_village_art 65 groups, 0 changed; rpg_test --golden,
  --endless --golden (39), --towns --golden (201) ok; --arms 1..3 ALL OK (golden keys); --seeds 1..3 3 passed (hero
  checks with rpg_art linked); save_test ALL OK; all 22 tap scripts pass on both builds (same exit codes; their shots
  differ only by real-time animation phase, e.g. 15 px of a pulsing corner); Witness 17 / 17 existing tests;
  p2a_lead_lens(_phone) 0 failures.

### Phase B lanes (disjoint files; VISION_PLAN 15.36 "Areas"; see the lead report for the hooks)
- **LENS** (engineer-high): the two layers for real (pix.cpp, screen.cpp), ZOOM and its keys / wheel / pinch, text
  through toUi, the ring, the night lift, pips, rig metrics in every world draw path, rigTex's MB caps and bake queue,
  web `?rig=2`. Done: L1-L9.
- **ANIM** (engineer): anim.cpp at rig 2, render_anim.cpp (v2 frames by pivot, family effects), game.cpp (corpses via
  animCorpseLife / Cap), ai.cpp (enemy windups via animEnemyWindup), skills.cpp (the greatsword's pace). Done: A1-A5.
- **RIG-BODY** (artist-xhigh): the style slice first, then the base (rig/rig_core, body, face, hair, cloth, death,
  rig_internal.h; rig2Covers; rig_gallery). Done: R1-R6.
- **RIG-ARMS** (artist-xhigh, after the slice): rig_arms, rig_actions, art_fx.cpp (the 9 effects), rigarms_gallery.
  Done: R7-R10.
- **POSES** (artist, after the slice): rig_poses, rig_sleep, rig_kin, rig_humanoid, art_life.cpp's v2 tables,
  rigposes_gallery. Done: P1-P3.
- **HOLDERS** (artist, after the slice): the holders' painters (art_building, art_props + holderPx, art_culture_furniture,
  art_market, art_home, art_sea, art_parts_gates, art_walls, art_city, art_critters' loose horse, art_party's graves,
  art_handcart), holders_gallery. Done: H1-H4.

### Integration (integrator), 2026-10-18: done (the gates table of VISION_PLAN 15.36 carries every measured number)
- MSVC (BDIR=build): every target built (408 ninja lines, only the usual C4244 / C4267 narrowing warnings, none new
  in kind); EMBERVALE_WITNESS=OFF configure + build of embervale (the web stand-in, 587 lines) links; WSL clang 18
  (build-native, refreshed by rsync) builds rpg_test and save_test.
- Fixes (integration only): render_sea.cpp's fishing panel (the bite "!", the tension meter, the run line, "LOST IT")
  is drawn in the UI layer but placed from world positions: now through toUi, its offsets x uiPerWorld (it sat off the
  hero on the phone, D5 over U4; checked on a 2556x1179 reel shot); tools/scripts/view_settings.txt taps re-aimed at
  the 7-row SETTINGS panel (ZOOM and PLAYER RING pushed HUD MARGIN / BORDER / DONE down: the taps had landed on the new
  rows, exit 0 but not testing what they meant); test_p1b_engine.cpp's lost bout puts the player beside the champion
  first: the family windups (ai.cpp animEnemyWindup, an intended P2a change) moved the won bout's end, the player was
  left across a fence from the champion's spot, the bot stands within 3 tiles and the AI (no pathing) cannot round the
  fence: 150 s, no blow (seed 1; reverting the windup alone passes, so nothing else moved).
- Regression (the one full run): all 67 CI commands of web.yml pass on MSVC and on clang (p1b-engine after the fix
  above; rpg_test --seeds 1..20 20 / 20; save_test ALL OK); `--anim --gates` and `--anim --seeds 1..3 --gates` 0
  failures; `--rig-coverage --seeds 1..20 --gates` 461 / 461; `--rig --gates` 2 failures on both compilers (R5 the
  bow's Draw frames, H2 6 of 23); script sweep 359 scripts, 0 failed (3,598 s); Witness 34 / 34 (17 existing, p2a_lens
  15, the LENS lane's 2); art_hash v1 categories identical (TOTAL 317eef6efe4d8229), fx+new 061fd76c895fbd6d (RIG-ARMS's
  effects; the other +new lines as in phase A); golden_village_art 65 groups, 0 changed.
- Gates: L1-L5, L7, L8, A1-A5, R2-R4, R6-R10, P1-P3, H1 PASS. FAIL: L6 at 23:00 on sand (+8.2) and snow (-12.0) (grass
  +21.9, road +15.6; edges 41-75: a look call), L9 draw at NORMAL 0.56 ms (baseline 0.49) and FAR 0.70 (limit 0.66;
  people 8.5 MB at rig 2), R1 (SLICE-WALK-SLIDE: the planner restates the slide gate), R5 (the bow: the lead picks an
  exemption or keepOpen false), H2 (gates not modelled, the hedge unowned, the boats' ratios: restate), H3 (riders on
  the v1 horse until home_view / party_view switch; gate passages). H4 is the judge's.
- CI not changed: `--rig --gates` would fail the web build on R5 / H2 until those are decided.
- Open for the next round (owners): LENS: draw time; the night lift on sand / snow; the ring on pale paving in a crowd
  (16); the top pip under the region banner; render_magic.cpp's blood multiplier text drawn in the world layer (scales
  with zoom); loot_ui.cpp's update-time culling by the UI size (harmless today); the hidden-hero silhouette and v2
  postures (HumanGhost, rigPoseTex); the v2 rider on HOLDERS' saddle. ANIM: the contact shadow under v2 corpses for 60 s;
  kids' walk frame starts (16 / 20 px cycles); footstep vs footfall sync; the convoy's animT tweak; sea pirates'
  windups; the pose path for posing folk at rig 2. RIG-ARMS / lead: R5 bow; effect placement notes. HOLDERS: gates,
  culture fences / hedges / benches (art_culture_props.cpp, art_parts_props.cpp unowned). POSES: humanoidAnchors at v2.
  Planner: SLICE-WALK-SLIDE and the H2 boat targets.

### Owner art-direction change, 2026-10-18: P2b and P2c re-planned
- The owner rejected the procedural rig's look (proportions, shading, faces) and approved art study A3 (A's 32 px
  silhouette, Sea of Stars / Chained Echoes rendering inside). The reference is locked in docs/ref/style_a3/;
  docs/STYLE_P2.md is rewritten for it; VISION_PLAN 15.36 re-plans P2b "New People" (the hand-drawn folk on the doll, the
  A3 slice signed off by the owner first) and P2c "Arms and Armour" (armour, helms, shields, weapons, actions, the flip);
  the old P2c creature work is now P2d "Proportion". P2a's lens, timing, holders and bake machinery stay; the procedural
  painters are detached from `rig 2` at P2b phase A and deleted at the P2c flip.
- P2a's open items carried into the re-plan: H2 / H3 (HOLDERS-B, P2b), riders on the doll and the posture path at rig 2,
  the hidden-hero silhouette, the contact shadow under corpses (VIEW-B, P2b), kids' walk cycles (KIN areas, P2b),
  SLICE-WALK-SLIDE (answered: A3's stride gives a 19 / 20 px walk and a run above 54 px/s, STYLE_P2 section 10).

## P2b, "New People": the hand-drawn folk on the doll (VISION_PLAN 15.36 P2b; art direction docs/STYLE_P2.md, reference docs/ref/style_a3)

### Phase A (lead), 2026-10-18: done (the frozen contract and harness; the areas ADD, never rename)
- The reference and its drafts: `tools/doll/from_a3.py` loads A3's modules (hero.py, woman.py, swing.py; the raw grids
  before their finishing pass) and writes .pxm drafts of the parts A3 composed from (18 parts, 121 views: the heads, the
  torsos, the legs D/U/R/L with every walk key, the scabbard and hilt, the arm swings, the cowlick tufts, the scarf's
  tails, the fierce face, the swing overlays and arcs; the woman's skirt with its sway, shoes, arm patches and braid
  swings) and a draft clip table into rpg/art/doll/drafts/a3/ (not compiled into the game). Its reference model of the
  doll (`--verify`) composes all 64 A3 frames from them at 0 px differing.
- The doll contract (rpg/art/doll/doll.h; the .pxm format, the clip format and the pool are written out in its header):
  slots, regions, the layer table, anchors, body sets, roles and their value vocabulary, DollRecipe / resolve,
  dollCovers / dollMissing / markReached / deadParts, compose / composePosture / composePart / figureStill / sleeper,
  walkTable (kGaitSwitch 54 px/s), rampA3 / rampGenerate / a3Ramps / a3Palette / rampsOf, the pool (embedded or
  loadPool / loadPoolFile for tools). First implementations (DOLL owns them now): doll_pool.cpp, doll_compose.cpp (the
  finishing pass = A3's selout; P2a's SheetBuilder as the packer), doll_resolve.cpp, doll_ramps.cpp, doll_fit.cpp
  (seatFit2 / bedFit2 moved with their numbers unchanged).
- art_rig.cpp's dispatch goes to the doll (humanRig, humanPosture2, sleeper2, humanFigureStill2, humanoidCell2 (P2c),
  rig2Covers = the doll covers LOCO and DEATH, and ACTION when armed). The procedural painters are detached: their entry
  points are renamed legacy* (rpg/art/rig/rig_internal.h), kept compiled for tools/preview/rigdraft.cpp and art_fx's arc;
  rig_gallery / rigarms_gallery / rigposes_gallery leave the build (doll_gallery replaces them).
- The tools: tools/doll/pxc.mjs (validates every map: legend, origins, views, keys, mirrored left views; craft warnings:
  orphans, the top-left light, missing facings, dead parts; writes rpg/art/doll/gen/doll_pool.cpp in chunks of 60 KB;
  `--check` (CI), `--report`, `--strict`, `--root`, `--bin`); tools/doll/draft.mjs (mirror-and-relight drafts of left
  views); tools/preview/rigdraft.cpp (P2a's pose numbers per frame, the 9 base weapons at 16 directions and the 9 family
  effects as .pxm drafts); tools/preview/doll_gallery.cpp (looks, clips, parts, lineup, onion skin, part / frame at 12x;
  `--pool`). CMake globs rpg/art/doll/*.cpp and gen/*.cpp.
- Look fields (art_human.h; derived, never saved; v1 ignores them; key() mixes them only at rig 2): head, nose, brow,
  marks, hairVar, beardStyle, neck, garb (a role's work garment: apron, smock, jerkin); age 3 = elder; youth and elder key
  as the adult at rig 1. RigPart::Run, RigAnchors::topY, Posture::Row (v1 stand-in: seated), RigMetrics r2 eyeRow 22
  (A3), figH of an elder = the adult - 1 at rig 2.
- Test hooks (rpg/view/script_p2b.cpp): `camhold 1|0`, `lineup <archetype> <role> [n] [female|male|mixed]
  [child|youth|adult|elder]`, `dollpart <part> <D|U|R|L> [key] | off`, `expect doll covered|missing [<role>=<value>]`;
  Witness state `player.sprite.doll` {covered, pool, missing[], part}, `player.sprite.part` (VIEW-B sets
  HeroSprite::part), `player.anim.speed` / `gait`, `entities[].doll` {covered, missing, first}.
- Harness (frozen): `rpg_test --doll [--gates] [--parity] [--report] [--pool f] [--dump dir]` (tools/tests/
  test_doll.cpp: the contract always; B1 parity, B2 heights, faces and hands, B8 craft, B9 fitted pairs, B10 atlas and
  speed, the ramp calibration with --gates), `--doll-coverage [--seeds A..B] [--scope civil|all] [--gates]` (B6: sources
  A, B, D, E), `--doll-variety [--seeds A..B] [--gates]` (B7); `--rig` restated for A3 (the doll's atlases; R4 is A3's
  face rows and 3 x 2 hands); tools/tests/png_read.cpp (readPng); scripts p2b_slice, p2b_folk, p2b_poses, p2b_walk,
  p2b_holders and their _phone twins; witness/p2b_doll.witness.json (2 hook tests that pass now, 6 P2b targets);
  tools/scripts/p2b_playtests.txt (the B13 goals as a witness play batch list: T1-T3 x 3 testers, the phone window,
  rig 2; tools/run_scripts.sh skips *_playtests.txt). CI: `node tools/doll/pxc.mjs --check`, `--rig` (its gates now
  measure the doll: without --gates until the areas pass), `--doll`, `--doll-coverage --seeds 1..3`.

Baselines (phase A, before the areas):
- Parity: the A3 drafts through pxc.mjs, the pool and the C++ compositor (`rpg_test --doll --parity --pool <drafts>`):
  48 kept frames 0 px differing, the 16 side frames 0 px (A3's own; the slice re-registers them).
- Coverage (B6, `--doll-coverage --seeds 1..3`, civil): 0 of 8,880 distinct looks (0 %; A 139, B 5,862, D 2,879), 0 dead
  parts (the production pool is empty: rig 2 draws v1 whole).
- Variety (B7, seeds 1..3): 7 of 24 hair styles, 1 of 9 beards, 1 head type per body, 2 / 1 garments (the look fields
  are not filled yet: FOLK-SIM).
- Bake (the drafts, `--doll --gates --pool`): one facing's LOCO p50 0.246 ms, p95 0.256 ms (v1 sheet p95 0.098 ms on
  this PC; the plan's 0.385 ms).
- The ramp generator (`rampGenerate`, phase A's LCh fit): worst step tunic 0 at delta E 13.5 (gate 6; DOLL tunes it).
- Render and people memory (tools/scripts/p2a_lead_perf.txt, the phone window, the capital at 15:00, this PC, median of
  each phase's last 5 seconds; at rig 2 the doll covers nobody yet, so v1 is drawn): View::draw 0.31 ms at NORMAL rig 1
  (people textures 3.4 MB), 0.33 ms at FAR, 0.29 ms at NORMAL rig 2 (2.5 MB: the rig switch flushed the caches). v1
  character sheet bake (EMB_TIMING, 141 sheets): p50 0.294 ms, p95 0.399 ms.
- Rig 1 unchanged (verified): art_hash every category and every sprite identical to the pre-phase-A build (TOTAL
  317eef6efe4d8229); golden_village_art 65 groups, 0 changed; `--anim --gates` output identical (golden_anim_v1); `--rig`
  contract 0 failures; MSVC every target (BDIR=build) and an EMBERVALE_WITNESS=OFF configure + build of embervale (595
  ninja steps) link; the 10 new p2b scripts pass.
- Script sweep (tools/run_scripts.sh, 371 scripts incl. the 10 new): 370 passed; m8_integ_tour_s12 failed once ("expect
  party state: BLEEDING OUT" after the bind) and passed on its rerun (timing under 3 parallel games; phase A changes no
  rpg/sim file). Witness: 45 / 45 existing tests pass; witness/p2b_doll: the 2 hook tests pass, the 6 P2b targets fail
  as expected until the areas land (B5 gait / drawn pick, B6 covered looks, B11 fall and rider).
- WSL clang 18 -fsyntax-only on every new and changed C++ file: no errors (the usual missing-field-initializer
  warnings of the Gate structs, as test_rig.cpp's).

### Wave 2 areas and integration (integrator), 2026-10-19: done (the gates table of VISION_PLAN 15.36 carries every measured number)
- Areas landed: FOLK-BODY (drawn idle / walk / run, hurt flinch, fall fling, elders' stoop, regions), FACES (9 + 9 head
  types in D/R/L/U, noses, brows, marks, paints, earrings, winces, youth faces), HAIR (24 styles, 10 beards, hangs,
  capped variants, lying views), GARB-M (13), GARB-W (14, neckwear, tiles), OUTER (8 headwears, legacy hood, 4 cloaks),
  POSES-B (31 postures and Row, both bodies), PROPS (32 props, 8 civil items), KIN-YOUTH (26 px body, 9 garments),
  KIN-CHILD (21 px body, 62 maps), HOLDERS-B (rig-2 sea / gate fixes in VIEW-B files), DOLL (sleepers without a Sleep
  clip, family clip precedence, helm over headwear, shared tile grid, pxc warnings and atomic writes), FOLK-SIM (garb
  mix, created hero's style and beard), VIEW-B (per-part coverage, RUN / DEATH bake queue, corpse shadow, topLift,
  Row on the oar clock). pxc: 403 parts, 6005 views, 670 clips, pool 679,573 bytes (hash 625873f6ab13a5bc), 0 errors,
  2 craft warnings, `--check` up to date.
- Integration fixes: save_test failed to link (gear_look.cpp calls doll::hairVariants, defined in rpg_art's
  doll_resolve.cpp): the hair family table, hairVariants and hairStyleOf moved to doll_walk.cpp (rpg_doll, the data
  library rpg_sim links). Witness state: the mounted hero never filled View::heroSprite (drawHumanBody is not called
  for a rider), so state.player.sprite read drawn false: home_view.cpp drawMounted now fills it (part posture at rig 2);
  the B11 rider target passes, the 10 riding scripts pass. Test-only: rpg/main.cpp's reset takes EMB_WPLAY_RIG /
  WINDOW / SAFE / SCREEN / HOUR / GOD as defaults when a param is absent (the playtest configs witness.play*.json set
  them through launch.env: a goal or a --params with backticks or escaped quotes is denied to the testers).
- Regression (the one full run): all 69 CI commands of web.yml pass on MSVC (rpg_test --seeds 1..20 20 / 20, save_test
  ALL OK) and all 66 generated CI commands pass on WSL clang 18 (CLANG_FAILS=0); script sweep 377 scripts, 0 failed
  (3,955 s); Witness `test --repeat 2` 118 / 124 (both passes identical): the 3 failing P2b targets were the rider
  (fixed, passes 2 / 2), the A3 hero's boxH 35 (the idle's breathe frame, bob -1: the target's 32-34 needs 35 or frame
  0; the lead's harness) and the start village's folk "every person covered" (21 guards in plate and 1 villager in
  leather: armour is P2c's); art_hash v1 categories identical (TOTAL 317eef6efe4d8229, every +new line as recorded;
  art_hash has no doll category: the pool hash stands for it); golden_village_art 65 groups, 0 changed; `--anim
  --gates` 0 failures; EMBERVALE_WITNESS=OFF configure + build of embervale (598 ninja lines) links (exe 18.07 MB vs
  18.24 MB with Witness).
- Gates: B1, B2, B3, B4 (marginal), B7, B8, B9 (structure), B11 PASS; FAIL: B5 (the youth girl's walk slips 2 px),
  B6 (69 dead parts, 100 % covered), B10 (an armed look's RUN atlas about 152 KB), B12 (H2 seats), B13 (below).
  `--rig --gates` fails R5-R10 (P2c families), P3 (the youth's cowlick in the harness look; the humanoids are P2d's)
  and H2. CI: `rpg_test --doll` now runs with `--gates` (passes on both compilers).
- B13 playtests (witness play batch, Sonnet low, 20 game minutes, the phone window at rig 2 through launch.env, T3 with
  god; reports in witness/playtests): T1 3 / 3 talked to the smith and the innkeeper but found them by place (the play
  shots are 639 x 295, people about 20 px: aprons and grey hair unreadable), an elder 1 / 3 (t1c, a white-haired
  villager); T2 1 / 3 reached a second people (t2b: Vontve plain earth tunics, bare heads vs Vorol striped shirts, dark
  caps and tricorns: "told apart roughly"); T3 0 / 3. Triage: no person drawn broken in any shot; "no corpse after a
  bandit falls" (t3a, t3b, t3c) not reproduced: p2a_death's bandit lies through 20 s at rig 2 (testers' shots were in
  forest or crowds, low-res); "four clone soldiers by the wrecked wagon" (t2b) and "4-5 near-identical villagers at
  Vant's well at night" (t2c): the soldiers are v1 armour (P2c), the villagers FOLK-SIM's Vontve tunic mix (open);
  "the hero stands on open water" (t3a): the shallows' wading, by design; "RENT A ROOM closes the dialogue, no gold
  taken" (t3a, t3b, t3c): the rent works (m0b_inn scripts), the testers' `choose N` landed on another option (Witness
  issue); "no sit on tavern chairs": the player has no sit action (the T3 goal needs restating: seated folk are P1 /
  P2's); "no horse or rowboat": both need a homestead or a purchase (goal setup); villagers' ids renewed after a scene
  change, stairs re-triggering, "talk X opens a neighbour", goblins in the village at the start: not P2b (the game's
  scene and talk logic, balance).
- Open for the next round (owners): KIN-YOUTH: the girl's walk registration (2 px), youth-length hangs (HAIR);
  FOLK-BODY / FOLK-SIM: an armed look's RUN atlas (move the drawn run into ACTION or restate the budget per armed look);
  lead: B6's dead-part scope (count withPosture props and role looks, leave armour and the hero's sword out of civil),
  the B2 / P3 cowlick and the hero boxH target (breathe frame); FOLK-SIM: face paints, dots tiles and amulets in the
  census, Vontve's tunic variety at night; planner: H2 seat range (0.14-0.20 proposed by HOLDERS-B), the B13 goals (T1
  needs labelled or bigger shots, T2 a reachable second people, T3 setup for a horse, a boat and a rented bed; no
  player sit); VIEW-B: the rider's Ride frame per horse frame, the talk mouth during a plate (FACES), the dust puff
  (FOLK-BODY); GARB-M / GARB-W / P2c: sleeves on FOLK-BODY's new arm keys (guard, fling, o*, orunF), build masks need
  DOLL's arm anchors to move.

### Fix round 1, OTHER (B13 playtests, harness targets), 2026-10-19
- Harness targets restated to P2b's scope: witness/p2b_doll "the A3 hero" freezes idle frame 0 (`pose idle 0 0`: the
  body box without the breathe frame's 1 px lift, STYLE_P2 3); "the start village's folk" checks civilians only (an
  uncovered person must lack only armour, helm, shield, pauldron, held or sheath parts; v1Draws 0 moves to P2c's C2: the
  21 plate guards and the leather villager are P2c's); `--rig --gates` P3 lets the hair rise 2 px over the 21 / 26 body
  as R2 does (the youth's tousled cowlick read 27). Result: p2b_doll 8 / 8 pass (B11 fall included); `--rig --gates` P3
  fails only the 3 humanoids (P2d), H2 23 / 0.
- Playtest plumbing: the dialogue snapshot (witness_state.cpp) listed every option, the grouped ones too, while the
  screen shows only the page's options, so a tester's `choose 6) RENT A ROOM` pressed FAREWELL (the "rent silently fails"
  of round 1): it now lists only `dlg.shows()` options in screen order (rent: gold 30 -> 20, "ROOM 1. UP THE STAIRS...",
  GO UP TO BED, sleep). rpg/main.cpp: EMB_WPLAY_SHOT world:K makes a tester's shot the world layer at K x K per art px
  (world:3: 1536 x 708, people 96 px instead of ~20). witness.play.json: the phone window at rig 2, god, allowCalls,
  world:3 shots. The goals (tools/scripts/p2b_playtests.txt) restated: T1 smith, innkeeper and a child, each described
  from a shot; T2 OSLARS and `call gotoculture other town`; T3 the five states through named setup calls (sitters at
  20:00 with `seatall`, rented sleep and `seatall sleep` sleepers, the homestead ride, the hero's own fall (`die`), the
  Sae rowboat (`sea port 1`)).
- B13 batch (witness play batch, Sonnet low, seed 7; reports witness/playtests/r2_t1a-c, r2_t2a-c (T1, T2) and r4_t3a-c
  (T3); two batches started in the same minute share one batch-MMDDhhmm.md, so the per-tester report.md files are the
  record): T1 3 / 3 (talked to all three, each described from a shot), T2 3 / 3
  (Vontve OSLARS vs the steppe ATEKENT: "clearly distinct at a glance"), T3 3 / 3 (all five shot and judged: sitters,
  sleepers, rider and the hero's fall read right in 3 / 3; the rower on the thwart in 3 / 3, the boat moving in 1 / 3).
- Triage of every note with a shot:
  - "the innkeeper's / smith's apron not readable", "the child among villagers not clearly smaller" (r2_t1a-c): looks
    read at 96 px; role cues weak: GARB-M / GARB-W (aprons by role), KIN-CHILD noted; not broken.
  - "a child beside the innkeeper cannot be targeted" (r2_t1a/b): the figure is Villager REGOONTERT (hooded); the child
    ROLGEJE is outside: no bug.
  - "two identical purple-robed conical-hat merchants side by side" (r2_t2a, Atekent): FOLK-SIM variety (open, as
    the Vontve tunic note).
  - "a red-capped Oslars-style figure in Atekent" (r2_t2b): a traveller / trader of another people: by design.
  - "the hero stands beside the bed after sleeping" (all T3): the sleep is behind a fade; the sleepers are checked with
    `seatall sleep` (heads on pillows 3 / 3): by design.
  - "the boat lies on the sand / the Oslars pier nook and will not move" (r2_t3a, r3_t3*, r4_t3a/c): SeaOps::giveBoat
    places a rowboat on the shore edge at port 0 (the Oslars pier nook: wedged, no direction moves) and sometimes at
    port 1 (Sae): open for the sea owner (repro: seed 7 `sea port` / `sea port 1`, `boat give rowboat`, `boat board`,
    hold w / s 3 s: the boat stays). The rower reads seated on the thwart in every shot.
  - "the oar is one vertical pole, no stroke" (r4_t3a): the Row frame between strokes; VIEW-B (Row synced to the oars).
  - "the dismounted horse stands over the fallen hero" (r4_t3a/b), "the horse is brown ridden, grey loose" (r4_t3b/c):
    the loose horse is drawn after the lying hero (sort by feet row: the horse's feet are lower) and the ridden horse
    uses another coat: HOLDERS-B / VIEW-B (open).
  - "bandit corpses vanish or stand upright" (r2_t3a, r3_t3a-c): armoured bandits are not covered by the doll at rig 2
    and are drawn whole on v1, whose dead frame is its hurt frame 3 px lower (render_anim.cpp: v1 never had a lying
    frame): P2c (C3 "a lying body keeps its gear"); the corpse lies 60 s (state dead / lying in the snapshot, corpse age
    1.0 -> 3.4 s probed). Hence T3 judges the hero's own fall.
  - "wheat tufts drawn over a corpse" (r3_t3a): tall-grass tufts in front of a lying v1 body by feet row: P2c with
    the corpses above.
  - "go e 12 walked into a house; home ride says NOT INDOORS" (r3/r4 T3): the testers' route; not P2b.

### P2b motion rollout (docs/P2B_MOTION_ROLLOUT.md), integration (GATES-WEAR), 2026-10-23: integrated, fix round open
- Every area's delivery in the tree (ADULT-RIG, MAN-WEAR, WOMAN-WEAR, KIN-YOUTH, KIN-CHILD, OVER-CARRY, GATES-LIMBS);
  the pool regenerated (9c637a21586cf561, 789,222 bytes, 427 parts), `pxc --check` clean; build-wear, build/ (every target,
  MSVC) and build-native (WSL clang 18) built.
- New: tools/tests/test_doll_wear.cpp (GATES-WEAR): B11 wear (W1-W10, T1, T2) on every side walk / run / drawn frame of a
  213-look matrix built from the pool's role vocabulary (M1-M6) with part coverage (175 parts, 0 unjudged), the M7 B10
  watch (72 cloak x hood x staff / stick / sword looks, inside `--doll --gates`' B10), the freeze manifest
  (.tmp/judge/rollout/freeze_f47c3af.txt from the checkpoint pool; ruled exceptions in freeze_accepted.txt), `--frames`
  strips, and `rpg_test --doll-wear` to run them alone. Wear selftest ALL CAUGHT.
- Measured (plan section 12 has the table): `--doll --gates` exit 1 on MSVC and clang with identical lines: B10 9 (M7: the
  elf man with hood and any cloak, RUN 64.4-65.3 KB), B11 limbs 38 (elves, child AREA), B11 wear 202 (W7 the woman's drawn
  guard over her chin 120, T2 the child's long skirts 20, W10 hammer / axe 19, W8 the woman's cloaks in the walk 16, W2 the
  robe sleeve run 4 12, W9 the youth's twin braids 12, W5 the ledger / scroll 3; 1,621 on the checkpoint pool), B12 16
  (elves). Freeze: 144 frames changed (the youth girl's fall and lying frames, 7 px: KIN-YOUTH), 16 ruled (the mail sleeves
  now drawn where they vanished). Rig 1 unchanged (art_hash TOTAL 317eef6efe4d8229, village-art 65 / 0, `--anim` 0);
  `--doll-coverage --seeds 1..3` 100 % covered; the gates take 21.7 s (+20 s).
- Kin numbers recorded in docs/MOTION_P2.md 12; the wear calibrations in docs/MOTION_P2.md 13 and the plan's section 12.
- Open for the fix round (owners named in the plan's section 12): the items above; CI keeps `--doll --gates` as it is
  until they pass.
- Closed by integration round 2, 2026-10-23 (docs/P2B_MOTION_ROLLOUT.md 12): every gate green. Re-measured by the
  planner on main at 390c485 (build/, MSVC): `--doll --gates` B1 16 / 0, B2 288 / 0, B8 1,120 / 0, B9 118 / 0, B10
  1,324 / 0, B11 limbs 409 / 0, B11 wear 5,112 / 0, B12 426 / 0, selftests ALL CAUGHT; pool 473ba3975c49c08e.

## P2c, "Arms and Armour": steel on the doll, every action, and the flip (VISION_PLAN 15.36 P2c, re-planned 2026-10-23)

Re-planned by the planner on the shipped doll (P2b) and the shipped motion method (docs/MOTION_P2.md,
docs/P2B_MOTION_ROLLOUT.md). Where it differs from VISION_PLAN 15.36's P2c plan of 2026-10-18, this section wins; the C
gate names are kept. Art direction: docs/STYLE_P2.md 25 (arms and armour) and 26 (the P2c style slice). Shared shapes,
ownership and test hooks: docs/P2C_CONTRACTS.md. The humanoid monsters move to P2d (decision D8).

**Re-planned again on 2026-10-23 (the gear ladder).** The owner approved the style slice's direction and asked for a
rags-to-riches progression built on the culture armour generator and the game's rules. What changed: "The gear ladder"
(new: rules and data), "The ladder slice" (new: wave 1b, signed off before production; it also fixes every open slice
item), gates C12-C14 and the restated B1, B8, C1, C3e, C5, C6, C8 and C10, constants, decisions D13-D23, the production
areas (the ladder in every art area, a new FINISH area), waves, the judge's focus and the risks. Art direction:
docs/STYLE_P2.md 27; data and runtime: docs/P2C_CONTRACTS.md 3b and 6b.

### Vision (and how it should feel)

Every fighter in Embervale is drawn on the doll with A3's craft. Town guards wear their kingdom's tabard over mail.
Soldiers wear their people's signature helm and harness. Captains and royal guards wear plate, bandits poor local
leathers, the king gilded plate, and the hero, man or woman, whatever they found, bought or forged. On a phone at arm's
length you can tell a Jade masked lancer, an Imperial crested spearman and a Fjordfolk horned axeman apart at a glance,
and putting on plate visibly changes the hero. Every weapon class fights with its own body: three blows each, a clear
coil before the blow and a hit you feel. Guards fall into a ready guard when trouble comes near. An archer holds a full
draw, then the string snaps. A soldier who falls keeps his helm, his shield lies flat and his spear lands beside his
hand. Then comes the flip: rig 2 becomes the game, v1's 16 x 24 people and the rejected procedural rig are deleted, every
menu figure and portrait is the doll, and no player ever sees two kinds of people again. It should feel like a finished
16-bit RPG whose cast was drawn by one hand, at 1x and zoomed.

And the gear tells the hero's story (the owner's rags to riches). The hero leaves home in a mended gambeson with an old
iron sword and a plain wooden shield: plain and a little dull, but whole and decent. Each upgrade is visibly better: clean
leather, then a soldier's steel with a brass edge and a painted device, then a far people's alloy, etched, with broader
shoulders and a cape, then gold filigree and a gem that breathes, and at last legendary gear whose runes glow with the
hero's breath while a spark runs up the blade. Each step lasts hours, follows the game's own bands, rarities and prices,
and keeps each people's style: a Jade lancer and a Fjordfolk warrior of the same grade look like different peoples and
like the same grade. Guards and soldiers follow the ladder by rank, so a late hero outshines a king.

### Where P2c starts (measured 2026-10-23, main at 390c485, build/ MSVC)

- **The doll**: pool 794,486 bytes (428 parts, 7,504 views, 670 clips, 1,672 frames; hash 473ba3975c49c08e); every
  `--doll --gates` line green (above).
- **Gear already on the doll**:
  - mail: the man's torso, hang, sleeves and stride panel; the woman's torso and sleeves (the P2b slice);
  - the nasal helm (form1, band2);
  - A3's sword: pivot-anchored overlays that include the near arm (hit 1 in all four facings, guard, gmove, drop), its
    scabbard and hilt, and A3's arc;
  - P2b's civil tools at rest (axe, hammer, bow, staff, stick, horn, ledger, scroll).
  The only action clip is actions/sword.clip (hit 1). There are no block, roll, cast or bow clips. The drawn idle, walk
  and run exist for the sword family only (ACTION variants 1 and 2). No pauldron, shield, effect, quiver, other helm,
  other armour form or other weapon map exists.
- **Coverage at scope all** (`--doll-coverage --seeds 1..2 --scope all --report`; it counts sources A, B and D only):
  8,073 of 9,869 looks (81.8 %): A 74 / 78, B 5,120 / 6,912, D 2,879 / 2,879.
  - 61 dead parts: the 8 mail parts, reached only by looks still drawn on v1; the nasal helm, reached by none; and P2b's
    civil leftovers (the woman's culture tunic, kilt and poncho, the kids' culture wear and headwear, face paints, the
    braided beard, the topknot, the turban, the child's sack).
  - Top missing values: pauldron dial3 and dial4; armour brigandine, scale, lamellar, plate, leather and padded; helm
    form5, 6, 8, 9 and 10; shield form2, 4, 6 and 9; the action clips.
  - Sources C (gear rolls) and E (party, story people, statues) are not counted yet.
- **B10 today**:
  - The armed sweep's worst RUN is 60.6 KB. Its worst ACTION is 303.0 KB, which holds only hit 1 and the drawn loco:
    about 160 KB of drawn frames, plus 141 KB for 16 swing frames with the arc baked in (8.8 KB each).
  - Civil tools: worst LOCO 124.0, RUN 63.9, DEATH 39.8, POSTURE 53.2 KB. The M7 cloak watch: LOCO 127.6, RUN 63.6 KB.
  - Bake: one facing's LOCO p95 0.178 ms (the v1 sheet's 0.100). People textures in the capital at 15:00: 10.3 MB at
    rig 2.
- **Rig 1**: art_hash TOTAL 317eef6efe4d8229; `city_gallery --village-art-check` 65 groups, 0 changed; `rpg_test --anim`
  0 failures.
- **Still drawn on v1 at rig 2**:
  - every armed or armoured look the doll does not cover: the Witness start village's 21 plate guards and its leather villager (seed 1; witness/p2b_doll),
    every soldier, captain, royal guard, king, jarl, bandit in leathers, chief, warlord, pirate and herald, and
    companions holding anything but a sword;
  - the bather and swimmer crop (render_anim.cpp) and the Mythic aura (built on v1 cells);
  - the paperdoll, the creator preview, the HUD hero, the party list, loot previews and party portraits;
  - statues of uncovered looks, and the humanoid monsters.
- **Resolver gaps** (doll_resolve.cpp):
  - shield form9 (ShieldForm::None) asks for a shield, where v1's hasShield draws none;
  - pauldron dial1 (the grammar's "none") asks for a pauldron;
  - the herald (Outfit::Guard with no helm and no body form) resolves to mail, where v1 draws a padded coat and tabard;
  - no look ever asks for a tabard;
  - bow fighters have no quiver, and HumanLook::backItem (a soldier's spare bow and quiver, a staff on the back) is not
    resolved at all, so it is silently not drawn;
  - the hunter's carried bow and a fighter's bow share the value `bow`.

### The gear ladder (re-plan 2026-10-23: the owner's rags to riches)

The owner, after approving the slice's direction (2026-10-23): "my personal style is a rag to riches evolution I want
beginners gear to look okay/mid and then slowly progresses throughout the game where you get cooler and cooler gear I'm
thinking too late stage gear could be animated in some way, I also know we have a bit of an armor generator for our
cultures system so just make sure you get it to work with that along with our current game rules/ plans". How each tier
looks: docs/STYLE_P2.md 27. The data and the runtime: docs/P2C_CONTRACTS.md 3b and 6b. This part says how the ladder
sits on the game.

**It changes no game rule.** A tier is derived from what every item already carries (its ilvl and band, rarity,
material, alloy, maker culture and form) and, for people, from the census rank. There is no new item field, no SAVE_VER
bump, no new RNG draw in the loot rolls or the census, and no balance change. The game's rules decide when each tier can
appear, and the ladder follows them:
- bands stay materials (VISION 7.2, 15.11): leather and bronze, iron, steel, then the culture alloys, or the heartland's
  gilded, obsidian, emberforged and starmetal. The ladder's finish sits on top;
- the rarity floors (15.27.4: Rare from level 5, Epic 12, Legendary 20, Mythic 30), the shop caps (7.5.4: Rare, Epic in a
  capital at Honoured, Legendaries never sold), attunement (1 / 2 / 3 legendaries at levels 10 / 25 / 40) and the slow
  economy (15.9) decide when a tier shows up;
- the hero still starts in a shirt (T0: the homeland's cut);
- rig 1 never sees a tier: the fields are rig 2's and enter key() only at rig 2.

**A piece's tier** (gear::lookTier, a pure function; contracts 3b). Rows are the item's band (gear::bandOf of its ilvl),
columns its rarity:

| band (ilvl) | Common, Uncommon | Rare | Epic | Legendary | Mythic |
|---|---|---|---|---|---|
| 1 (1-8) | T1 | T2 | T2 | T6 | T6 + the flame |
| 2 (9-16) | T2 | T2 r | T3 | T6 | T6 + the flame |
| 3 (17-24) | T3 | T3 r | T4 | T6 | T6 + the flame |
| 4 (25-32) | T4 | T4 r | T5 | T6 | T6 + the flame |
| 5 (33-42) | T4 | T4 r | T5 | T6 | T6 + the flame |
| 6 (43-52) | T5 | T5 r | T5 | T6 | T6 + the flame |
| 7 (53-60) | T5 | T5 r | T5 | T6 | T6 + the flame |

- **r**, the Rare mark: the piece wears the next tier's trim (STYLE_P2 27.3), so a Rare find is visibly finer than a
  Common one of its band. A band-1 Rare escapes the scrap look outright.
- A culture's masterwork alloy (tierStep 3) is T5 at least. Legendary is always T6. Mythic is T6 with the flame (the
  ember tempering and the aura).
- Shops sell Rare at most (Epic in an Honoured capital), so a shop never stocks above T(band) with the Rare mark (one tier
  more in an Honoured capital). T5 is bought only from band 6 on, or as an Honoured capital's band-4 or band-5 Epic. T6 is
  never bought, except from the hidden-kingdom focus vendor.

**When each tier shows up** (VISION 1.4's pacing with the floors above):

| tier | first seen | worn for most of | from |
|---|---|---|---|
| T0 shirt | the start | the first quest | the homeland's cut |
| T1 Scrap | the OLD BLADE and the first band-1 buys and drops | hours 0-3 | village shops, bandits, chests |
| T2 Common | a band-1 Rare (level 5, about 0.5 h), band 2 (level 8-10) | hours 3-10 | town shops, quests |
| T3 Sturdy | a band-2 Epic (level 12, about 6 h), band 3 | hours 10-25 | city shops, smiths, dungeons |
| T4 Fine | a band-3 Epic, band 4 (D 25 and up, about 28 h): the first culture alloys | hours 25-60 | far cities, culture smiths, bosses |
| T5 Masterwork | a band-4 or band-5 Epic, band 6 (about 60 h), a masterwork alloy | hours 60-100 | masterwork smiths, hoards, the hidden kingdoms |
| T6 Legendary | level 20 (about 12 h) at the earliest: bosses, hoards, campaign and saga finales | one piece from level 10, two at 25, three at 40 (attunement) | earned, never bought |

**Slots.** Every worn slot keeps its own tier (body, helm, shield, weapon in hand, gloves, boots, cloak, back), so a
legendary sword glows on a common gambeson. Every metal piece shares the body armour's metal ramp (gear_look's rule and
the 48-colour cap). The weapon in hand keeps its own metal (the Blade slot, contracts 3).

**People** (census::dress sets the tiers from the role and rank at the end of each role, with no draw; STYLE_P2 27.6):

| who | tier | notes |
|---|---|---|
| bandits, militia, pirates | T1 | the local culture's poor kit (looks.cpp kKit) |
| bandit chiefs, the village watch (rank 0), soldiers | T2 | the watch and soldiers in their kingdom's livery and device |
| the capital watch (rank 1), heralds, envoys, warlords | T3 | |
| captains, the royal guard (rank 2), jarls, story knights | T4 | the realm's alloy |
| kings | T5 | regalia |
| the hero, companions | their items (wearGear) | |

Depth: soldiers, bandits and the watch one tier finer in lands of band 4 or more (the place's danger at makeLook).

**The culture generator feeds every tier** (rpg/culture ArmsStyle, through gear_look's forms; STYLE_P2 27.4). Soft pieces
take a people's everyday forms, metal pieces its signature forms, its alloys a finer cut; the dials grow by tier inside
the people's own style (the survey of priors.cpp, 2026-10-23; "/" separates the generator's weighted picks):

| people | soft (T1-T2): helm / body | metal (T3): helm / body | alloy cut (T4-T6) | shield, blade, polearm, bow | dials (pauldron skirt crest cape) | ornament | T5 filigree |
|---|---|---|---|---|---|---|---|
| Fjordfolk | nasal / leather or padded | spangen or nasal / mail | scale | round, broad, spear, self | 1 2 0 2 | fur trim, etching | knotwork |
| Highland | nasal / leather | kettle or nasal / brigandine or mail | plate or scale | buckler, broad, halberd, longbow | 0 1 0 3 | studs | knotwork |
| Heartland | kettle / padded | nasal or great helm / mail or plate | scale or plate | kite or heater, straight, halberd, longbow or crossbow | 2 1 0 1 | rivets | oak leaves |
| Imperial | plumed / leather | crested / scale, plate or brigandine | scale or plate | tower, straight, spear, composite | 2 2 3 2 | plumes, gilding | laurel |
| Dune | masked / padded | aventail / mail or scale | scale | crescent or round, scimitar, spear, composite | 1 2 1 3 | tassels, etching | arabesque |
| Steppe | spangen / leather or padded | plumed or aventail / lamellar | lamellar | round or buckler, curved, spear, composite | 1 2 2 0 | horsetail, studs | running horses |
| Marsh | nasal / leather | kettle / brigandine | plate | oval, leaf, spear, self | 0 1 0 0 | tassels | reeds |
| Jade | crested / leather | masked / lamellar | lamellar | none or oval, curved, glaive, longbow or composite | 3 2 2 0 | scallops, tassels | cloud scrolls |
| River | nasal / padded | kettle or crested / brigandine | plate | heater, falchion or straight, halberd, crossbow | 2 1 1 0 | rivets, plumes | waves |
| Sun Temple | masked / leather | winged or plumed / brigandine | plate | round, broad, spear, self | 0 1 3 1 | plumes, gilding, tassels | sun rays |
| Sylvan | masked / leather | crested or winged / leaf | leaf | leaf, leaf, glaive, longbow | 1 2 1 2 | etching, filigree | leaf veins |
| Starspire | crested / leather | winged / plate or scale | plate or scale | kite, glaive, glaive, longbow | 2 2 2 2 | filigree, gilding | stars |

Guards and soldiers wear the owner kingdom's arms (so occupation shows), bandits the local poor kit, and the far,
mist-hidden peoples their strange metals (priors.cpp hiddenLook, culture.cpp hiddenMetal), which the ladder shows from
T4 as their alloys. C1's source L (below) proves that every people's output resolves on the doll at every tier it can
produce.

**Rules the game lacks: proposals, not in P2c** (each needs a rules batch with the balance sim, VISION 15.9):
1. **Light armour has no ladder.** Leather and cloth stop at band 1 (gear.cpp matFor, matBandCap) and are always Light
   (skills.cpp weightOf). Proposal: each people's leathers above band 1 (hides and dyed or tooled leathers, bands 2-5),
   so a light build climbs T1-T5 too. Until then a leather piece rises by rarity (T1, T2, the Rare mark, T6). (D23)
2. **The heartland's band names and looks disagree.** gear_look.cpp bandOf (tier + 2) draws a band-2 IRON piece as steel,
   STEEL as gilded, GILDED as jade, and STARMETAL as ember. The doll draws each piece's own metal (contracts 3b
   metalKind); rig 1 keeps its looks until the flip. (D20)
3. **No measured gear-over-time curve exists** (15.9 asks for gold per hour and gear tier at 1 / 5 / 20 / 50 h).
   FOLK-SIM-C adds `rpg_test --ladder-pace`: the tiers the shops stock and the drops roll at each pacing stage's danger,
   from shopStock and rollDropC over 1,000 rolls, so the hours above are measured, not assumed.

### In and out

**IN**
- Body armour: the 8 forms (padded, leather, mail, scale, lamellar, brigandine, plate, leaf), each fitted to the man and
  to the woman.
  - Torsos; skirts and tassets by the skirt dial; sleeves and leg armour by region and hand-drawn material tiles.
  - Every band, bronze, alloy, sheen and Mythic tempering, all as ramps.
  - The legacy outfits mapped onto the forms (Leather, Chain, Plate, Elven, Ebony, Guard).
- The tabard over armour (the watch, soldiers, heralds) with the kingdom's device; pauldrons (3 families x small, medium,
  large); gloves and gauntlets; boots and greaves.
- The 12 helm shapes, with crest, plume and horsetail regions, hair masks, and lying views where the turn fails.
- The 8 shield shapes and the legacy round: field, device and rim; at rest, guard, block, bash, lying and slung.
- The weapon library, every weapon at 16 angles: sword, axe, mace, dagger, greatsword, spear, staff, bow, walking stick.
  Also the 8 blade, 3 polearm and 5 bow forms, the crossbow, the sheaths, the quiver, the back carries, and legendary and
  Mythic glints.
- Every action in four facings:
  - each family's three blows;
  - the bow's draw and release, and the crossbow's aim, shoot and crank;
  - block, roll (with the shield bash) and cast;
  - every family's guard stance and drawn walk and run;
  - carry at rest for every weapon class;
  - hurt and death with gear;
  - armoured postures (riders, kneelers, sitters, the throne).
- The family effects, re-cut and drawn from maps.
- **The gear ladder** (re-plan 2026-10-23): six tiers for every worn piece from its item, and for people from their rank;
  the finish regions and tiles (wear, trim, etching, filigree, gems, runes); the dials by tier; the doll's metals from
  each item's material (bronze, iron, steel, every culture alloy, the heartland's bands, Mythic tempering) and the blade's
  own metal; the living finish on T5-T6 (the breathing glow, the edge spark, motes, the cloak and plume stirring, the
  Mythic flame on doll frames); every people's arms at every tier it can produce (C1 source L); the ladder slice before
  production.
- Every remaining human draw path on the doll. Then the flip: rig 2 is the only rig, v1 people and the procedural rig are
  deleted, and art_hash and the goldens are re-recorded once.
- Gates C1-C14, and playtests T4-T7.

**OUT**
- The humanoid monsters (skeleton, draugr, goblin), the creatures, the village animals, and the props the P2c judge lists
  as reading wrong: all P2d (D8).
- Any balance change: the sim's clocks, damage, parry and roll rules stay. Block is a visual flash; there is no new sim
  state and no new attack. Parry stays the sword's.
- Armour on kids.
- Rig 1 after the flip (D9).
- The creator's flow (P3). P2b leftovers that are not gear: the talk mouth, the dust puff, the loose horse's coat.
- Any change to the shipped walk and run, or to front and back walks.
- Any SAVE_VER, ENDLESS_GEN_VER or SAGA_GEN_VER bump. save_v23.bin and the town, world and endless goldens never change.
- Any change to item generation or the rules for the ladder's sake: culture leathers above band 1 (proposal 1, D23), the
  heartland's band names in the sim (proposal 2), shop stock, drop rates and prices.
- The ladder on item icons (D22: art_hash `icons` holds); any change to wearGear's, census::dress's or gearIcon's existing
  outputs before the flip (the ladder writes only new fields).

### Inventory: what the game puts on people (code read 2026-10-23)

Sources: rig 1's painter (rpg/art/art_human.cpp); the item and look data (rpg/sim/items.h, gear.cpp, gear_look.cpp
wearGear, looks.cpp census::dress and makeLook, game.cpp, wayside.cpp, party_*.cpp, quests.cpp, ai.cpp); the culture
grammar (rpg/culture/culture.h ArmsStyle, priors.cpp).

**Body armour.** The resolver's value comes from bodyForm, else the outfit, else armorStyle (1 leather, 2 mail, 3 and up
plate).

| form | who wears it | doll today | P2c parts |
|---|---|---|---|
| padded | Highland and Sun Temple soldiers (their signature); bandits of Highland, Dune, Marsh, Jade, Sun Temple, Sylvan, Starspire; heralds (fix); the hero's gambeson (in the maker's cloth) | none | torso m / w, quilt tile, cloth skirt |
| leather | bandits of Fjordfolk, Heartland, Imperial, Steppe and River; Marsh soldiers (boiled); pirates (half); band-1 armour; the seed-1 start village's leather villager | none (civil trades draw their garb, P2b) | torso m / w, stitch and studs tiles, skirt |
| mail | Heartland, Fjordfolk and Dune guards and soldiers; the Chain and Guard outfits; band-2 armour; captains of mail cultures | man and woman (P2b) | the P2b maps + dials, the tabard, the mail tile (D3) |
| scale | Imperial and Starspire soldiers, warlords, Dune (1 in 3) | none | torso m / w, scale tile, tassets |
| lamellar | Steppe and Jade soldiers and guards | none | torso m / w, lamellar tile, skirt |
| brigandine | River soldiers and guards, Imperial (1 in 5) | none | torso m / w, rivets tile |
| leaf | Sylvan guards and soldiers; the Elven outfit | none | torso m / w, leaf tile, gilt edge |
| plate | plate cultures' guards (21 in the seed-1 start village), captains, royal guards, kings, jarls, the bandit chief, story knights (quests.cpp), the Ebony outfit (obsidian, spikes), band 3-7 armour, the hero | none | torso m / w, tassets, lames tile, pauldrons |

**Helms.** The value is formN from helmForm, else bandN from helmStyle. (The Highland soldier wears a bonnet instead:
P2b's cap.)

| shape | values | who wears it | doll today |
|---|---|---|---|
| leather cap | band1 | bandits' leather helms, band-1 helmets | none |
| nasal | form1, band2 | Heartland, Fjordfolk and Highland second forms; iron helmets | P2b's |
| kettle | form2 | Highland, Marsh (boiled leather) and River soldiers | none |
| great helm | form3, band3 | royal guards, Heartland, Imperial; steel helmets | none |
| spangenhelm | form4 | Steppe soldiers, Fjordfolk; Steppe bandits (leather) | none |
| horned | form5, band6 | Fjordfolk soldiers, warlords; obsidian helmets | none |
| plumed | form6 | Sun Temple and Steppe soldiers; Sun Temple bandits (leather) | none |
| conical with aventail | form7 | Dune soldiers | none |
| masked | form8 | Jade soldiers | none |
| crested | form9, band5 | Imperial and Sylvan soldiers, River; Imperial bandits (leather); jade helmets | none |
| winged | form10, band4 | Starspire and Sun Temple soldiers, kings; gilded helmets | none |
| horned crown | band7 | emberforged helmets | none |

**Shields.** The value is formN from shieldForm, else shapeN from shieldStyle, else shape0 when only `shield` is set.
form9 means no shield.

| shape | values | who carries it |
|---|---|---|
| round | form1, shape1, shape0 | legacy guards, the iron tier, Fjordfolk, Steppe and Sun Temple soldiers, Dune (alternate), Fjordfolk bandits |
| kite | form2, shape3 | Heartland and Starspire soldiers; the gilded and emberforged tiers |
| heater | form3, shape2 | the capital watch, royal guards, captains, River; the steel and jade tiers |
| tower | form4, shape4 | Imperial soldiers; the obsidian tier |
| crescent | form5 | Dune soldiers |
| oval | form6 | Marsh, Jade (alternate); Imperial and Sun Temple bandits |
| buckler | form7 | Highland soldiers; Heartland and River bandits; bandits generally (1 in 4) |
| leaf | form8 | Sylvan soldiers and bandits |

**Pauldrons, skirts, gloves and boots.**
- The culture dials (pauldron and skirt, 1-4: none, small, medium, large, or the skirt to the knee) add 1 for capital
  guards and captains and 2 for kings and jarls. Poor pieces are small at most. The watch's skirt stops above the knee.
- Guards' gloves and boots match the body band (steel at most). Soldiers' follow the harness's weight, and the village
  watch wears leather boots. The hero's follow each piece's band.

**Weapons and things in hand.**

| art id | weapon (forms) | who carries it |
|---|---|---|
| 1 | sword (blade forms: leaf, falchion, scimitar, khopesh, wavy, broad, curved, glaive) | guards, captains, kings, royal guards, half the bandits, pirates (the cutlass), fighter companions, the hero |
| 2 | axe | half the bandits, the chief, warlords, militia farmers, rogue companions, the hero |
| 3 | bow (self, recurve, composite, crossbow, longbow) | bandit archers (35 %), wayside and party archers, hunters (civil: carried), the hero; a guard's or soldier's spare bow on the back |
| 4 | staff | mages and lamplighters (civil), mage companions, the hero |
| 5 | dagger | the hero |
| 6 | mace / hammer | militia smiths; smiths and shipwrights (civil tools); the hero |
| 7 | greatsword (blade forms) | the hero |
| 8 | spear, glaive, halberd | every soldier (with a shield), the hero |
| 9 | walking stick | refugees, elders (P2b) |
| 10-12 | ledger, scroll, horn | stewards, envoys, heralds (P2b) |

**Sheaths and back items**:
- the sword's scabbard (A3's) and the dagger's;
- axes and maces through the belt (P2b's civil maps serve fighters too);
- the greatsword across the back;
- the quiver (every bow fighter; new) and the bow on the back out of combat;
- the staff on the back (backItem 2);
- the crossbow, slung;
- the shield, slung, in postures.

**Who carries it, and where they are drawn**:
- the watch (village, capital, royal);
- soldiers and captains (war camps, sieges, patrols);
- kings and jarls (thrones: postures);
- bandits, chiefs and warlords (camps, ambushes), and pirates (the sea);
- heralds, companions (M8), story knights;
- statues (humanFigureStill);
- riders: the hero and mounted companions, in Posture::Ride on HOLDERS' horse;
- corpses (DEATH: 60 s, at most 24);
- sleepers (helms off, as in v1), kneelers and the downed (postures).

### Actions the doll needs

| action | clip (atlas) | frames per facing | when it plays | notes |
|---|---|---|---|---|
| carry at rest | idle, walk, hurt (LOCO); run (RUN) | 3, 6, 1; 6 | out of a fight | sheathed, slung or upright by class (STYLE_P2 25.7); the shield arm holds still |
| guard stance | idle variant 1 (DRAWN) | 3 | a hostile in aggro, and 4 s after a swing | per family (STYLE_P2 25.8, item 11) |
| drawn walk, run | walk variants 1 and 2 (DRAWN) | 6, 6 | moving while drawn | the loco legs and plants under the guard arms |
| swings | windup, strike, follow x 3 variants (ACTION) | 1-2, 1-2, 2 | attacks: foes cycle the variants, the hero combos | 8 melee families: unarmed, dagger, sword, greatsword, axe, mace, spear (with glaive and halberd), staff |
| bow | draw, release (ACTION) | 3, 1 | the bow's windup and shot; the hero's Cast with a bow | the crossbow: aim, shoot, crank |
| block | block (ACTION) | 3 | a 0.15 s flash on the sword's parry and a shield bearer's parry | no sim state; raise 30, impact 50 (a 1 px recoil, the spark), brace 70 ms (the owner, 2026-10-23) |
| roll | roll (ACTION) | 4 | the dodge | turned crouch frames; the shield bash |
| cast | cast (ACTION) | 2 | spells | gather, release |
| hurt | hurt (LOCO) | 1 | being hit | gear rides it |
| death with gear | fall (3 x 2 sides), lying (DEATH) | 3, 1 | the killing blow; lying 60 s | per family: held=drop, shield=lie, the helm on, back items under the body |
| postures with gear | posture clips (POSTURE) | as in P2b | sitting, kneeling, the throne, riding, carrying, downed | armour composes; shield and weapon by D12 |

### The flip (wave 3)

- **What makes rig 2 the default.** All of these:
  - C1 at 100 % over sources A-E and L (seeds 1..20; L seeds 1..3);
  - C12-C14 green over the matrices and source L;
  - C3-C6 and C8 green at rig 2;
  - the wave-2 judge's look review passed;
  - the owner's go on the flip pack: before / after pairs at rig 1 and rig 2 of the reference screens, the start village,
    a war camp, a market, a harbour and every menu.
- **What changes.**
  - `rigVersion()` returns 2 always (art_rig.h's default).
  - `setRigVersion(1)`, `EMB_RIG=1`, script `rig 1`, Witness `rig: 1` and web `?rig=1` warn once and are ignored.
    `rig 2` stays accepted, so old scripts still run.
  - The k-scaled holders become the only size.
- **What is deleted.**
  - The v1 people painter: art_human.cpp's sheets, postures, sleepers and v1 portrait helpers (humanSheet, humanCellRaw,
    humanFigureStill, humanPostureCellRaw, paintSleeperHead), and art_life.cpp's v1 posture tables.
  - The procedural rig: rpg/art/rig/** and tools/preview/rigdraft.cpp, rig_gallery.cpp, rigarms_gallery.cpp and
    rigposes_gallery.cpp.
  - Every v1 human fallback in the view (humanTex, poseTex, sleeperTex, the v1Draws sites), and RigMetrics r1's people
    rows.
- **What is kept and moved.**
  - metalRampKeys moves to art_items.cpp (the icons share it).
  - HumanLook and key() stay: they are the doll's input.
  - humanoidCell moves to art_monsters.cpp, pixel for pixel; P2d redraws it.
  - Every gallery that drew v1 people now draws the doll and keeps its boards.
- **Rig 1 retires** (D9). Nothing keeps v1 people alive after the flip; the before / after pack is the record.
- **art_hash and the goldens.** Each is re-recorded once, by the flip, with before / after pairs.
  - In art_hash, `humans` goes and `doll` comes in: the composed atlases of a fixed reference list (the A3 hero and
    woman, the slice looks, one look per armour form and body, one per family), all deterministic.
  - `monsters` and `icons` stay identical, and the hash proves it.
  - `buildings`, `walls` and `props` change, because the k-scaled holders are now the only size. `fx` changes because the
    effects now come from maps. TOTAL is new.
  - `city_gallery --village-art-check` (65 groups) is re-recorded, with a before / after PNG per changed group.
  - tests/fixtures/golden_anim_v1.txt and `--anim`'s A1 retire with rig 1's pick.
  - `--arms` golden keys are re-recorded with a before / after table (key() always mixes the doll's fields).
  - save_v23.bin and golden_towns, golden_world, golden_endless and golden_villages never change.
- **CI** ("Clear Sight rig and animation test"):
  - runs `pxc --check`, `--rig` (the contract), `--anim --gates`, `--doll --gates`,
    `--doll-coverage --scope all --seeds 1..3 --gates` and `--doll-variety --seeds 1..3 --gates`;
  - `--rig-coverage` retires (C1 replaces it).
- **Scripts and Witness**: every `rig 1` line and v1 expectation in tools/scripts and witness/*.json is restated at rig 2,
  and witness/p2c_switch.witness.json joins (v1Draws 0, coverage, menus).

### Gates (P2c DONE; every B gate stays green)

| gate | target | measured by |
|---|---|---|
| C1 coverage | 100 % of distinct looks and 0 dead parts over sources A (the world and its residents), B (cultures x roles x sexes x ages x 8 seeds), C (every gear.cpp roll per maker culture: kind x material x rarity x form, worn through wearGear by a man and by a woman), D (the creator) and E (companions of every vocation, story people, statues). P2b's civil leftovers are reached by FOLK-SIM-C or ruled out by the planner. | `rpg_test --doll-coverage --scope all --seeds 1..20 --gates` |
| C2 v1 draws | 0 people frames from a v1 sheet after the flip | Witness `v1Draws` over the full script sweep and every Witness test |
| C3 gear on every frame | 0 failures, every frame of LOCO, RUN, DRAWN, ACTION, DEATH and POSTURE, for every armed or armoured look in the matrix. **a, no holes**: every gear part has a view (empty or not) for every key its channel takes, and no frame has an enclosed hole (the bow's string gap is exempt: R5's question, answered). **b, helm on the head**: helm pixels inside the head box ± 1 column, except crest, plume, horns and wings (up to F-37) and an aventail to the shoulders; the helm moves exactly with the head anchor; no hair above a masking rim. **c, shield on the off arm**: its origin on gripL; attached; behind the torso facing right, in front facing left and down; clear of the boots and over at most 1 column of a shin; inside the run box + 1. **d, grips**: the weapon's grip inside the gripping fist, and no weapon pixel over that fist's H pixels; the right hand in every facing, the bow in the left; for two-handed weapons the off fist on the haft line within 1 px; a tip mark at the far end. **e, lying keeps gear**: the helm on the turned head, the shield flat within 4 px of the body, the weapon's drop within 6 px of the near hand, back items under the body. **f, motion with gear**: B11 limbs, B11 wear (W1-W10, T1, T2) and B12 boots over every armour form x body x build and every carried weapon, with W4 and HEAD extended to pauldrons, the tabard and helms, W7 to weapons and shields, and W10 to shields. | `rpg_test --doll --gates` (lines C3, B11, B12); `--doll-arms` |
| C4 armour shows | For every form x band x body, at least 25 % of the idle figure's pixels (facing down and facing right) change against the same look bare (15 % for padded and leather). The medium and heavy forms change the outline (shoulders, skirt line or head). Equipped in game, as shot pairs of the hero before and after `gear6` / `kit6`, with `witness measure box` and a diff. The judge reviews the man / woman fit. | `--doll --gates` C4; the p2c_gear script |
| C5 families | strike frames of every family pair 25 % apart (of their union) per facing; a family's three variants 25 % apart; tip paths side-on (the sword's over the head; the spear's forward 8 px or more with under 4 px of drift; axe and mace down 10 px or more; the greatsword's 14); left frames painted (never the plain mirror of the right) with the weapon in the right hand; the effect ending with the strike state within a frame; bow 3 + 1, block 3 (restated below), roll 4 and cast 2 present for every body | `--doll --gates` C5; `--anim --gates` A2 restated |
| C6 the library | every weapon map at its 16 angles (bows: their aim views); the lit edge toward the top-left in every view; a tip mark, and a bright tip pixel on blades; no orphans; no doubled pixels or L-jaggies along a line; the grip origin on the haft; lengths per STYLE_P2 11 ± 1 px | `--doll --gates` C6; pxc craft |
| C7 menus | the paperdoll, creator preview, HUD hero, party list, portraits and loot previews show doll frames at whole multiples; the Mythic aura hugs doll frames (alpha only within 3 px of the silhouette) | the p2c_menus script, Witness, the judge |
| C8 speed | per look: LOCO 128, RUN 64, DRAWN 256, ACTION 512, DEATH 48, POSTURE 64 KB over the M12 heavy-kit sweep; people 24 MB or less in the capital at 15:00; the city-arrival worst frame within baseline + 4 ms; View::draw at NORMAL within baseline + 10 %; one facing's LOCO bake p95 0.385 ms or less, an ACTION atlas 6 ms or less; the pool 1.75 MB or less (D21); embervale.wasm within today's + 1.0 MB | `--doll --gates` B10, p2a_lead_perf.txt, EMB_TIMING, the web build |
| C9 art_hash and goldens | unchanged through waves 1-2 (rig 1); re-recorded once at the flip with before / after pairs (above) | art_hash, `city_gallery --village-art-check`, `--arms` |
| C10 playtests | **T4 fighter**: "fight bandits with each weapon family: does each swing look different?" **T5 outfitter**: "as a man, then as a woman (the `female` reset), buy and equip leather, mail and plate, and take a shot after each." **T6 watch**: "find guards in three peoples' towns and soldiers of two realms, and say how each is armed." Each goal met by 2 of 3 testers; every note with a shot triaged | `witness play batch` (P2b B13's setup: the phone window, rig 2, world:3 shots) |
| C11 fighters' faces | no head recipe (type, nose, brows, beard, hair colour) shared by more than 2 guards of a settlement or 2 soldiers of a squad under open helms (the playtest's "four clone soldiers") | `--doll-variety --gates` (fighters) |
| C12 the ladder reads | Over each ladder line (the ladder slice: the Heartland hero T1-T6; production: every people's kit T1-T6, man and woman), idle facing D and R. **a, steps**: consecutive tiers differ in 15 % or more of the union of their gear pixels, T1 against T6 in 50 % or more. **b, richness**: the trim share of each piece inside its tier's band (T1 0, T2 3 % or less, T3 3-8 %, T4 6-12 %, T5-T6 10-20 %); step-4 pixels and gear colours never fall from T1 to T5; glow pixels 0 below T5, present at T5 (gems) and at T6 (runes). **c, silhouette**: the idle figure's area without the weapon never shrinks by more than 2 % from one tier to the next between T2 and T5, and T5 is 15 % or more over T1. **d, T1 is okay**: B8 clean, the step-0 ring closed, C14 holds, at most 2 wear marks a piece and wear 6 % or less of a piece. **e, flavour**: two peoples' kits of one tier differ in 25 % or more of their gear pixels and 8 % or more of their outline, with trim share and glints inside the tier's band. The judge's eye has the last word (process) | `rpg_test --doll-ladder --gates` (line C12) |
| C13 the living finish | Over 4.4 s stepped at 60 Hz (deterministic): the pulse period 2.2 s (a T5 gem 3.3 s) within 0.05 s; the spark every 3.3 s, 0.33 s from grip to tip; 3 motes or fewer, life 1.0 s; no glow pixel's light changes by more than 0.25 between two steps, and nothing toggles faster than 2 Hz (the spark's run aside); 32 animated pixels or fewer a figure-frame (ladder i2: was 24); glints (white step 4 and the spark) 3 or fewer; two actors' phases 0.15 cycle or more apart for 95 % of id pairs; glow plane bytes 25 % or less of their atlas; the city frame within baseline + 0.3 ms with 8 T6 figures; nothing drawn at rig 1 | `--doll-ladder --finish`; EMB_TIMING; the judge at 1x |
| C14 value | On every composed idle frame of every matrix look, touching materials (cloth 1, cloth 2, accent, leather, metal, hair, skin, cloak, field, head) differ by 18 luma or more at step 2 (the face, and pixels inside one material, aside); the figure's lit top-left edge stands 40 luma or more over the darkest day ground; plate's step 3 and step 1 stand 60 luma or more apart | `--doll --gates` C14 |

**Restated by the ladder re-plan (2026-10-23):**
- **B1** compares the figure: the doll composes without effects, and A3's Fx colours (#7FA8D8 #C9E6F4 #FFFDF4) are left
  out of the reference frames. A3's arc is re-cut (the owner: oversized, dropping below the feet facing D; D17).
- **B8**: 48 colours or fewer per figure, plus at most 3 Glow-slot steps on T5-T6 looks (D18).
- **C1** adds source L, gear::ladderKit for every people x tier (T1-T6) x weapon family x body, seeds 1..3, and gives
  source B the census's rank tiers. `--scope ladder` runs L alone.
- **C3e**: a dropped weapon lies flat (its long axis within 1 px per 8 px of level) on the corpse's ground band (its
  lowest pixel within 3 rows of the body's), within 6 px of the near hand, never above the head's top row; a bow lies
  flat; a shield lies flat within 4 px.
- **C5**: the spear's windup has 2 frames, and its loaded frame leans back 2 px or more; its strike leans forward 3 px or
  more with the front foot planted; a block has 3 frames (raise; impact, with a 1 px recoil and the spark; brace); every
  hurt frame keeps the held part (C3a); an effect stays inside the frame's box plus the reach, and above the sole row.
- **C6**: a bow's D and U views show a 2 px stave at the grip and a curve over 3 columns or more.
- **C8** adds: the glow planes inside the people budget (25 % or less of their atlases, at most 12 cached), D19's
  top-tier atlases, and the pool at 1.75 MB (D21).
- **C10** adds **T7 ladder**: "start a new game and play the first hour, then use the `ladderkit` reset for tiers 1 to 6
  and take a shot at each: does the first kit look okay, and is each a step up?"

### Constants that interact (budgets and timing)

| constant | value | arithmetic |
|---|---|---|
| the body | man 32, woman 31 px; a 12-row head box; 16 px shoulders on both bodies | helms, pauldrons and shields fit the shared anchors; a crest to F-37 lifts the marker by the frame's top (topY) |
| walk and run | 22 / 36 px cycles, 6 frames, bob 1 / 2, lean 4, switch at 54 px/s | gear never changes the tables, and REG holds for armour |
| LOCO | 128 KB a look | 40 frames; today's worst 124.0 (staff + hood). A kite shield adds about 3-6 columns on the down, up and left frames, so a guard's LOCO is about 100 x 1.15 = 115 KB. Tower shields with large pauldrons are the risk (M12 sweep; fallback D4) |
| RUN | 64 KB | 24 frames; today's worst 63.6. Armour adds 0-2 columns (pauldrons) and a carried spear about 1-2 KB, so the spear rides tight to the body and tilts one step only (D5) |
| DRAWN (new) | 256 KB | 60 frames; the drawn sword frames weigh about 160 KB today, and a levelled spear adds about 40 %: 225 KB |
| ACTION | 512 KB | After the split, a family has 3 variants x 4 facings x 4-5 frames = 48-60 swing frames at 4-6.5 KB (effects not baked in) = 190-390 KB, plus block 4, roll 16 and cast 8 at about 3 KB (85 KB): 275-475 KB. Without the split, a spear look would need about 650 KB (D2). |
| DEATH | 48 KB | 26 frames; today's worst 39.8. A dropped spear widens the lying frames by about 10 columns: about 45 KB |
| people textures | 24 MB in the capital at 15:00 | 80 folk x LOCO 100 KB (8.0 MB) + 12 fighters x (DRAWN 220 + ACTION 420 KB) (7.7 MB) + 10 runners x 60 KB (0.6) + 40 postures x 50 KB (2.0) + 24 dead x 45 KB (1.1) = 19.4 MB |
| bake | LOCO p95 0.385 ms a facing; the queue 2 ms a frame (1.2 on the web) | An ACTION atlas (about 110 frames) bakes in 2-4 ms. It is queued when a hostile enters aggro (130 px: 1.75 s, or 105 frames, at 74 px/s), so it is ready before the first blow. |
| pool | 1.5 MB (P2b's 1.0 MB restated: D11; 1.75 MB with the ladder: D21, below) | Today 794 KB for 7,504 views (106 B a view). P2c adds about 5,000 views: armour 16 torso sets x 4 facings x 7 keys = 450, hangs 600, helms 60, shields 300, pauldrons 110, the library 640, action arms about 2,400, legs 300, sheaths 150, fx 130. At about 100 B each that is + 0.5 MB, so about 1.3 MB. |
| colours | 48 a figure | The guard: skin 5 + hair 5 + eyes 2 + face 3 + ink 1 + cloth1 5 + cloth2 4 + accent (tabard and field) 4 + leather 5 + metal 5 + metal2 3 + device 3 = 45. The field shares the accent, or the look goes over. |
| timing | hero 70 / 110 / 130 ms x pace (sword 1.0, dagger 0.8, axe 1.08, spear 1.1, mace 1.25, greatsword 1.18, fists 0.9); foes 220-380 / 280 (heavy 400) / 350 (750); folk 300 / 250 / 300; release 80; block flash 150; roll 4 x 85; fall 100 / 120 / 120 | The pick splits each state at 45 / 40 / 50 %, so the hero's loaded pose shows about 40 ms and a foe's 120-210 ms (the telegraph). Hence one strongest silhouette per windup (STYLE_P2 25.8). |
| reach and effects | rig(2).reach {9 12 16 22 16 16 30 22 12} px, fxLift 12 | the doll's tip marks agree within 3 px (R8 restated) |
| weapon lengths | sword 15, greatsword 24, spear 34 + 5, staff 30, bows 22 / 28 / 20 px | a spear carried upright tops out near F-40, and the marker rides topY |
| tiers (the ladder) | T1-T6 a piece | band 1-7 gives T 1 2 3 4 4 5 5; Epic + 1; Rare wears the next tier's trim (a band-1 Rare is T2); a masterwork alloy is T5 at least; Legendary and Mythic are T6 |
| pacing | T1 hours 0-3, T2 3-10, T3 10-25, T4 25-60, T5 60-100, T6 from about 12 h (rare) | VISION 1.4: levels 5 / 10 / 20 / 30 / 40 / 50 at 0.5 / 3 / 12 / 28 / 55 / 100 h; bands by D 1-8 / 9-16 / 17-24 / 25-32 / 33-42 / 43-52 / 53-60; floors Rare 5, Epic 12, Legendary 20, Mythic 30 |
| trim share | T1 0, T2 3 % or less, T3 3-8 %, T4 6-12 %, T5-T6 10-20 % of a piece | STYLE_P2 25.2's 15 % cap, with gilding (T5-T6) excepted as before |
| glints | T1 0, T2 1, T3 2, T4-T6 3 (the spark counts) | STYLE_P2 25.2's three |
| colours by tier | T1 about 34-38; T6 48 + 3 glow | a T1 piece: 3 material steps + a patch's 2. The T6 hero under a great helm: skin 3 + eyes 1 + face 1 + ink 1 + cloth 4 + cloak 5 + leather 3 + metal 5 + gold 4 + blade 4 + field 3 + device 3 + plume 3 = 40, + glow 3. An open helm, a beard and a tabard is the worst case: the colour fallbacks (contracts 3b) |
| the living finish | runes 2.2 s (two 1.1 s breaths), light 0.30-0.85; a T5 gem 3.3 s, 0.15-0.45; the spark every 3.3 s, 0.33 s from grip to tip; motes 3 at most, born every 0.45 s, life 1.0 s, rise 8 px; the stir on the idle's 0.55 s frames | 32 animated px a figure-frame at most (i2: was 24); light changes 0.25 or less per 1/60 s; 1 pulse blit + up to 4 sprite blits a T6 figure |
| glow memory | a glow plane is 25 % or less of its look's atlas; at most 12 looks cached | a T6 look's atlases are about 1 MB, so its plane about 0.25 MB; 6 T5-T6 looks in a city = 1.5 MB inside the 24 MB people budget (19.4 MB estimated) |
| top-tier atlases (D19) | T5-T6: LOCO 160, RUN 80, DRAWN 320 KB; any tier (ladder i2 integration, measured): a crested or plumed helm RUN 68 KB, a polearm DEATH 56 KB; (polish i1 integration, measured) a polearm LOCO 136 KB (the upright carry in front of the body: jade3 130.3, knight 127.5); (polish i2 integration, measured) a polearm RUN + 14 KB over its base and D19 (the 45 deg forward run carry: jade3 76.5 of 78, knight 80.9 of 82, lt6w 89.2 of 94) | the M7 cloak watch's LOCO is 127.6 KB; large pauldrons (+2 columns a side facing D / U, about + 10 %) and a shield give about 145 KB. Six such looks add about 0.7 MB to a city. The fallback without D19: a T5-T6 look with a shield wears the short cape |
| pool (D21) | 1.75 MB | 943 KB at slice iteration 3 + the rest of production about 0.35 MB + the ladder about 0.2 MB (new forms, region planes about + 5 %, finish tiles, stir views, the spark and motes) = about 1.5 MB, with room; test_doll.cpp still checks 1.0 MB |

### Decisions taken by default (confirm or overrule)

- **D1. Weapons are an authored library**: 16 angles, on the grip, with the doll drawing the hand over the grip. A3's
  sword is re-cut onto it at 0 px (B1). Fallback, if the re-cut cannot hold 0 px: A3's overlays stay as the straight
  sword's named views, and every other weapon and form uses the library.
- **D2. The drawn idle, walk and run move to a new DRAWN atlas** (256 KB), so ACTION (512 KB) holds the families.
  Effects are drawn from their maps as separate sprites, and A3's arc stays pixel-identical on screen (until D17's
  re-cut, which the owner asked for on 2026-10-23).
- **D3. Sleeves and leg armour are region recolours with hand-drawn material tiles**, never per-key sleeve maps.
  Silhouettes change only through torsos, skirts, pauldrons, helms and shields. The slice decides by eye whether the mail
  tile replaces P2b's drawn mail sleeves.
- **D4. Shields ride the off arm at rest** (STYLE_P2 23). Fallback, if a look's LOCO goes over 128 KB: slung on the back
  out of combat.
- **D5. Spears and staffs are carried upright** in the right hand at rest, tilted one step in the run. Fallback, if RUN
  goes over 64 KB: upright in the run too.
- **D6. Bow fighters carry the bow on the back with a quiver** out of combat. Civil hunters keep P2b's carried bow (value
  `bow`; fighters use `bow.<form>`).
- **D7. Block is a 0.15 s visual flash** on the sword's parry and on a shield bearer's parry. No sim state, no balance
  change.
- **D8. The humanoid monsters move to P2d.** Their v1 painter moves to art_monsters.cpp unchanged at the flip. (VISION_PLAN
  15.36 had them in P2c; the P2b integrator already counted them as P2d's.)
- **D9. Rig 1 retires at the flip**, and the flip waits for the owner's go on the before / after pack.
- **D10. Pauldrons and helms are shared** by both bodies: the same 16 px shoulders and head box, with the anchors carrying
  the woman's row. Torsos, skirts and tabards are fitted per body.
- **D11. The pool budget is 1.5 MB** (it was 1.0 MB), and embervale.wasm stays within today's + 1.0 MB (the deleted
  painters offset part of it).
- **D12. Shields and weapons in postures**: seated, kneeling and lying postures sling the shield on the back and set the
  held weapon aside (not drawn). Ride keeps the shield on the arm and the weapon at rest. The king's sword across his lap
  on the throne is depth.

The ladder re-plan (2026-10-23) adds:
- **D13. The ladder is per piece and derived** ("The gear ladder"): six tiers from each item's band and rarity, people's
  from their rank. Nothing is saved, no rule or balance changes, and rig 1 never sees it.
- **D14. Tiers are finish and dials, never new shapes.** Materials stay ramps (STYLE_P2 25.2). A tier adds wear, trim,
  etching, filigree, gems and runes through five regions every map marks once, and grows the silhouette only through the
  people's own dials, helms, shields, cloaks and plumes (STYLE_P2 27). STYLE_P2 25.12's "no new shape for a band" now
  covers tiers too.
- **D15. One armour metal, the blade its own**: all armour shares the body armour's metal ramp (gear_look's rule); the
  weapon in hand keeps its metal (Slot::Blade, the same ramp when it matches).
- **D16. The living finish is light over drawn frames**: a cropped glow plane pulsed per tick, a spark sprite on the
  grip-to-tip line, view motes, and stir views on the idle's existing frames. No baked phases, no new frames, no change to
  the walk or run; rig 2 only.
- **D17. B1 covers the figure, and A3's arc is re-cut** smaller for every sword look (the owner, 2026-10-23).
- **D18. B8 allows 3 glow steps** beyond the 48 on T5-T6 looks.
- **D19. Top-tier atlases** (the owner's call): T5-T6 looks may use LOCO 160, RUN 80 and DRAWN 320 KB (only a handful
  are ever in a scene: about 0.7 MB in a city). Fallback: a T5-T6 look with a shield wears the short cape, not the long
  cloak.
- **D20. Heartland pieces draw their own metal on the doll**: IRON looks iron, STEEL steel, GILDED gilded, OBSIDIAN
  obsidian, EMBERFORGED ember, STARMETAL its own pale ramp, and the form follows the metal (leather leather, iron mail,
  steel and up plate; contracts 3b metalKind). Rig 1 keeps its tier + 2 looks until the flip.
- **D21. The pool budget is 1.75 MB** (D11's 1.5 MB restated: the ladder adds about 0.2 MB); embervale.wasm stays within
  today's + 1.0 MB. With D19 it is the owner's call.
- **D22. Item icons do not change in P2c** (art_hash `icons` holds through the flip); the ladder on icons is a later UI
  pass.
- **D23. The light-armour ladder is a later rules batch** (proposal 1); in P2c a leather or cloth piece rises by rarity
  only.

### The style slice (wave 1; the owner signs it off before any production map)

The slice locks how steel, leather, cloth over mail, a shield, a spear and a bow look and move on the shipped doll. It
shows each new form on both bodies, every slice clip in all four facings (the in-game read), and the A3 sword re-cut onto
the library. STYLE_P2 26 lists its contents and the owner pack.

| tag | body | kit | it proves |
|---|---|---|---|
| guard | man | P2b mail + a tabard in the kingdom's red, the nasal helm, a kite shield with a gold device, A3's sword at the hip, leather gloves and boots, a full beard | cloth over mail; the shield at rest, in guard, in block and in the run; A3's swing with a shield; the re-cut |
| knight | woman | steel plate, medium plate pauldrons, short tassets, gauntlets and greaves, an open crested helm with a red crest (her braid below), a spear | heavy armour on the shipped legs and lean; the woman's fit; the crest; two-handed grips; a new family (the thrust) |
| archer | man | studded leather, a leather cap over tousled hair, a self bow, the quiver and the bow on his back at rest | light armour; the bow's draw and release; back items; a weapon in the off hand |
| knightm | man | the knight's plate and helm, unarmed | plate fitted to the man |
| archerw | woman | the archer's leather and cap, unarmed, a ponytail below the cap | leather fitted to the woman |
| hero | man | A3, the reference | the re-cut at 0 px |

**Clips, in four facings.**
- guard, knight and archer: sheathed idle, walk and hurt; the run; the drawn idle, walk and run; the fall and lying on
  both sides.
- guard: hit 1 (A3's swing with the shield on his arm) and block.
- knight: hit 1 (the thrust) and block.
- archer: the draw (3 frames) and release.
- The twins: loco, run and the fall.

**Areas** (at most 4, files disjoint; docs/P2C_CONTRACTS.md 1). Art areas edit only their maps, preview on isolated
pools, and propose clip lines and region slots in their reports. SLICE-DOLL owns every code, clip and gate change.

- **SLICE-DOLL** (engineer; a hard-systems area: escalate to engineer-high after two failed attempts; BDIR
  build-p2c-doll).
  - Files: rpg/art/doll/*.h, *.cpp, gen/** (at delivery), clips/** (loco.clip, death.clip, actions/*; postures.clip and
    the kin clips untouched); tools/doll/**; rpg/art/art_rig.h/.cpp, art_fx.h/.cpp; rpg/sim/anim.h/.cpp;
    rpg/view/render_anim.cpp, witness_state.cpp, script_p2c.cpp (new); tools/tests/test_doll*.cpp, doll_motion.h,
    test_anim.cpp; tools/preview/doll_gallery.cpp; tools/scripts/p2c_slice*.txt; witness/p2c_slice.witness.json;
    docs/P2C_CONTRACTS.md 8 ("as built"); this section's "Slice results" (append).
  - Core:
    - the slice's resolver values, flags and region slots (contracts 3), with the four fixes: form9 none, dial1 none,
      the herald padded, the quiver;
    - region tiles (`on-region`);
    - the library compose: `weapon=<deg>,front|back` picks `a<deg>` views, plus `anyfacing`;
    - the hand drawn over the grip (both hands; the bow hand);
    - the shield on gripL with per-facing layers;
    - clip conditions;
    - RigPart::Drawn end to end: pxc `part drawn`, compose, the anim pick, the render bake, the B10 line, and the freeze
      tool reading DRAWN;
    - effects from maps through art_fx (the arc identical; the thrust and twang new);
    - every slice clip line in four facings: the sword's drawn lines move to actions/sword.clip as `part drawn`, plus
      the guard's `when shield` lines, actions/spear.clip, actions/bow.clip, and the spear and bow death lines;
    - the test hooks (contracts 7);
    - gate lines C3, C4, C5 (sword against spear) and C6 on the slice looks, with selftests;
    - the scripts p2c_slice, _phone, _pack and _fight; `tools/doll/owner_pack.py --p2c`;
    - the exes s1, s2 and on, published.
  - Depth: an incremental pool compile for the artists' roots; the `grip` board onion-skinning a swing with its tip path.
  - Delight: the gallery's `silhouette` board, every strike frame filled flat side by side, so the families read at a
    glance.
  - Done:
    - `node tools/doll/pxc.mjs --check` is clean;
    - `rpg_test --doll --gates --parity` exits 0: B1 16 / 16 at 0 px with the re-cut, B8 0, B10 within LOCO 128, RUN 64,
      DRAWN 256, ACTION 512 and DEATH 48 KB for every slice look, B11 limbs, B11 wear and B12 0 including the slice looks;
    - `rpg_test --doll --motion --limbs-looks <slice looks> --report` shows 0 failures;
    - the C3 to C6 lines show 0 on the slice looks, and the selftests are ALL CAUGHT;
    - `dollMissing` is empty for every slice look in LOCO, RUN, DRAWN, ACTION and DEATH;
    - witness/p2c_slice and the slice scripts pass;
    - rig 1 is unchanged (art_hash TOTAL 317eef6efe4d8229, village-art 65 / 0, `--anim` 0) and the full `rpg_test` is
      ALL OK;
    - the owner pack builds.
- **SLICE-ARMOUR** (artist, at xhigh effort).
  - Files: rpg/art/doll/maps/armour/** (new: man/plate*, woman/plate*, man/leather*, woman/leather*, man/tabard*,
    shared/pauldron_plate_*, tiles/**; the P2b mail keeps its views and ADDs).
  - Core:
    - plate on the man and the woman, the torso in every key the slice clips name (rest, idle, run, runl, drawn, swing,
      lie), with one lit ridge per plate and at most three glints;
    - the tassets on the hang channel (rest, wide, flare, lie, idle);
    - medium plate pauldrons (shared, both sides), riding the lean and lifting on wind-ups;
    - studded leather on both bodies, with its skirt;
    - the tabard over the man's mail (run and runl views, the device region);
    - the tiles (lames, stitch, studs, and the mail tile tried against P2b's drawn sleeves);
    - proposed region slots for gloves, boots, greaves and sabatons.
  - Depth: mail with a glint that walks with the bob; the plate's lit edge jumping to the new lit side as the body turns.
  - Delight: the tabard's hem kicking out in the run while the mail skirt trails.
  - Done:
    - C3a is 0 for the armour parts on every slice frame;
    - B11 limbs, B11 wear and B12 are 0 on all five slice looks;
    - C4: plate changes 25 % or more and leather 15 % or more of the idle figure on both bodies, and plate changes the
      outline;
    - B8 is 0 on every armour map and composed frame (48 colours or fewer, the top-left light, no orphans, banding or
      pillow shading; mail never dither);
    - LOCO and RUN are within budget;
    - the judge's fit review of man against woman passes.
- **SLICE-HELM-SHIELD** (artist).
  - Files: rpg/art/doll/maps/headwear/helm/** (the new crested helm, form9 and band5, and the leather cap, band1; the
    nasal keeps its views and ADDs); rpg/art/doll/maps/shield/** (new: the kite shield, form2 and shape3, and the shield
    arm).
  - Core:
    - the crested helm: an open face, the crest in the plume slot with crest1-4 regions, hair masks for the slice
      styles, the braid below the rim, and a lying view only if the turn reads wrong;
    - the leather cap over the tousled hair's capped view;
    - the nasal re-checked on the guard's cropped hair and beard;
    - the kite shield (field, device, rim; rest, guard, block, lie and back; per-facing layers);
    - the shield arm, with the sharm keys the guard's clips name, gripL and regions.
  - Depth: the crest bobbing a pixel behind the walk; the device in the kingdom's two colours.
  - Delight: the rim catching a glint as the guard raises the shield to block.
  - Done:
    - C3b and C3c are 0 on every slice frame, including the lean and lying;
    - W7: the shield is never over the face;
    - B8 is 0;
    - LOCO with the shield is 128 KB or less, and RUN 64 KB or less;
    - the judge reads the crest, the kite's device and the leather cap at 1x on the phone.
- **SLICE-ARMS** (artist, at xhigh effort).
  - Files: rpg/art/doll/maps/arms/adult/action/onehand/**, action/twohand/**, legs/adult/action/onehand/**,
    action/twohand/**, held/weapons/**, belt/sheaths/** (existing views kept; ADD), back/** (new), fx/** (the arc kept;
    ADD).
  - Core:
    - A3's sword re-cut: the near arm per swing key, with grip marks, plus the blade's angle views and A3's named views,
      holding B1 at 0 px; the guard and gmove poses re-cut;
    - the guard's drawn idle, walk and run and his hit 1 with the shield (the sword side);
    - the spear: 16 angles, carry and drop; on the knight, the carry, the drawn idle, walk and run, the thrust and block
      in four facings, with action legs on the method (the far leg whole, boots stamped);
    - the self bow: aim views x string states, the nocked arrow and the drop; on the archer, the drawn idle, walk and
      run, the draw (3 frames) and the release in four facings;
    - the quiver with arrows and the bow on the back;
    - the thrust streak and the twang in A3's three tones;
    - proposed clip lines (frames, ms, weapon angle and depth) for SLICE-DOLL.
  - Depth: a hit-stop beat on the thrust's impact frame; the off hand sliding along the shaft through the thrust.
  - Delight: arrows showing in the quiver; the string's blur on the release frame.
  - Done:
    - B1 is 16 / 16 at 0 px;
    - C3d is 0 (the hand over every grip, the right hand in every facing, the bow in the left, the spear's off fist on
      the haft);
    - C5: the spear's strike is 25 % or more apart from the sword's in every facing, its tip travels forward 8 px or
      more, and the left frames are painted;
    - C6 is 0 on the sword, spear and bow;
    - C3e: every drop lies within 6 px of the hand;
    - W7: no weapon is over a face in the drawn walk or run;
    - B10: DRAWN is 256 KB or less and ACTION 512 KB or less for the guard, the knight and the archer;
    - B8 is 0;
    - the judge's read at game speed: anticipation and impact read, and the swings differ.

**The slice gate** (the judge measures first and states the full protocol in round 1; at most 4 rounds; fix rounds
verify only their items):
1. Coverage: every slice look resolves with nothing missing.
2. B1 parity: 16 / 16 at 0 px.
3. Motion: B11 limbs, B11 wear and B12 at 0 on every slice look.
4. Gear: C3 a-f at 0.
5. C4.
6. C5: the sword against the spear.
7. C6: the slice's weapons.
8. B8 at 0, with 48 colours or fewer.
9. B10 within budget.
10. Size, crispness and contrast: heights per STYLE_P2 3 (a crest to F-37), whole D x D blocks in `ui 0` shots at D3,
    D4 and D5, and the day edge 40 or more on grass, road, sand and snow (tools/p2_measure.mjs).
11. Rig 1 unchanged.
12. The look review (STYLE_P2 26 acceptance) at 1x on the phone and at 3x / 8x / 12x.
13. The owner's sign-off on the owner pack. It also settles D1 and D3-D6.

The owner approved the style slice's direction on 2026-10-23 (D1, D3-D6 stand as built: the library re-cut, shields on
the arm at rest, the spear carried upright, the tabard over mail, the DRAWN split) and asked for the gear ladder.

### The ladder slice (wave 1b; the owner signs it off before production)

The style slice locked steel, leather, mail, a shield, a spear and a bow on the doll. The ladder slice locks the ladder
before any production map: one fighter at every tier, the top tier alive, the same tier in three other peoples, rank to
tier for people, and every open item of the style slice fixed. STYLE_P2 27.9 lists the look and the owner pack.

**Looks** (tools/doll/ladder_looks.txt, through contracts 3b's macros; LADDER-DOLL picks the seeds that give these forms
and records `rpg_test --expand-looks` beside the file):

| tag | body | tier | people | the kit (real items through wearGear, or the census) | it proves |
|---|---|---|---|---|---|
| lt1 | man, the A3 hero (head, hair, scarf, colours) | T1 | Heartland | a mended padded gambeson, the OLD BLADE, a plain wooden round shield, worn leather boots; no helm | scrap reads okay / mid |
| lt2 | man, the hero | T2 | Heartland | clean studded leather (a band-1 Rare) with small leather shoulder pads, the leather cap, the round shield painted in his arms, an iron sword, leather gloves and boots | the first step: clean |
| lt3 | man, the hero | T3 | Heartland | steel mail with a brass edge, medium scale pauldrons, the nasal helm, the kite bearing his device, a steel sword, tall boots | a soldier's kit |
| lt4 | man, the hero | T4 | Heartland | the people's alloy as etched plate, large plate pauldrons (the alloy's step), the great helm with a small crest, a heater with a 3-colour device and a boss, gauntlets and greaves, a travel cloak | an officer's kit |
| lt5 | man, the hero | T5 | Heartland | masterwork plate with gold edges and oak-leaf filigree, large pauldrons, the great helm crested, plumed and gilt with a brow gem, the gilt heater with a gem boss, the masterwork sword, the noble cloak | a champion's kit; the gem's breath |
| lt6 | man, the hero | T6 | Heartland | lt5's kit Legendary: runes on the seams, the glowing eye slit, the runed blade's spark, motes, the stirring cloak | the living finish |
| lt6m | man, the hero | T6 Mythic | Heartland | lt6 Mythic (depth) | the flame on doll frames |
| lt6w | woman, the A3 woman | T6 | Heartland | the T6 kit on her: plate, the crested helm (open), large pauldrons, the noble cloak, a runed spear | the top on both bodies |
| jade3 | man | T3 | Jade | the masked helm, lamellar with a mid skirt and scalloped, tasselled hems, large lamellar shoulders, a glaive, no shield | flavour |
| imp3 | man | T3 | Imperial | the crested helm with a tall crest, scale with tassets, medium scale shoulders, a tower shield, a straight sword, a travel cloak | flavour |
| fjord3 | man | T3 | Fjordfolk | the spangenhelm, mail with fur trim, small scale shoulders, a round shield, a broadsword (A3's straight blade until the library draws the broad), a travel cloak | flavour |
| nband | man | T1 | Heartland bandit | the census's bandit kit (a hood, leather, a buckler, a sword) | rank to tier |
| nroyal | woman | T4 | Heartland royal guard | the census's royal guard | rank to tier |
| nking | man | T5 | Heartland king | the census's king (depth) | rank to tier |
| guard, knight, archer, knightm, archerw | | T2, T4, T2, T4, T2 | | the style slice's looks with their tiers; knightm armed with a sword, archerw with a bow | the open items fixed |
| hero | man | | | A3 | B1 |

The lines (LADDER-DOLL copies them into tools/doll/ladder_looks.txt; it may retune the archer's colours for C14 and the
seeds for the planned forms, and records the change):

```
# tag    who    spec                                                                        gaits
hero     man    -                                                                           walk,run,dwalk,drun,didle,hit1,fall,lying
lt1      man    hero=1,kit=heartland:1:7,palette=look                                       stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying
lt2      man    hero=1,kit=heartland:2:7,palette=look                                       stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying
lt3      man    hero=1,kit=heartland:3:7,palette=look                                       stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying
lt4      man    hero=1,kit=heartland:4:7,palette=look                                       stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying
lt5      man    hero=1,kit=heartland:5:7,palette=look                                       stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying
lt6      man    hero=1,kit=heartland:6:7,palette=look                                       stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying
lt6m     man    hero=1,kit=heartland:6:7,rarity=mythic,palette=look                         stay,walk,run,didle,hit1
lt6w     woman  heroine=1,kit=heartland:6:7,wpn=spear,palette=look                          stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying
jade3    man    kit=jade:3:11,wpn=spear,head=6,hair=1,hairVar=1,hairColor=#1E1A22,skin=#E2B58A,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying
imp3     man    kit=imperial:3:3,wpn=sword,head=2,hair=1,hairVar=0,hairColor=#3A2418,beard=1,beardStyle=2,skin=#C98E64,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying
fjord3   man    kit=fjordfolk:3:5,wpn=sword,head=4,hair=2,hairVar=1,hairColor=#C8A060,beard=1,beardStyle=4,skin=#EAC0A0,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying
nband    man    npc=heartland:bandit:0:7,palette=look                                       stay,walk,run,hit1,fall,lying
nroyal   woman  npc=heartland:guard:2:7,palette=look                                        stay,walk,run,didle,hit1,fall,lying
nking    man    npc=heartland:king:0:7,palette=look                                         stay,walk
guard    man    outfit=8,armorStyle=2,bodyForm=3,tabardColor=#A8322E,helmet=1,helmStyle=2,helmForm=1,shield=1,shieldStyle=3,shieldForm=2,shieldDevice=#E8C25A,weapon=1,bladeForm=1,gloves=1,boots=1,pauldron=1,neck=0,head=1,hair=1,hairVar=1,hairColor=#4A3022,beard=1,beardStyle=0,skin=#D9A07A,tier=2,palette=look  walk,run,dwalk,drun,didle,hit1,block,hurt,fall,lying
knight   woman  outfit=5,armorStyle=3,bodyForm=7,pauldron=3,skirt=2,gloves=3,boots=3,helmet=1,helmStyle=3,helmForm=9,crest=3,plumeColor=#B83A32,weapon=8,polearmForm=1,hair=5,hairVar=0,hairColor=#2E1E18,skin=#E8B08A,tier=4,palette=look  stay,walk,run,dwalk,drun,didle,hit1,block,hurt,fall,lying
archer   man    outfit=3,armorStyle=1,bodyForm=2,armsOrnament=16,helmet=1,helmStyle=1,weapon=3,bowForm=1,gloves=1,boots=1,neck=0,topColor=#5E6A3A,bottomColor=#4A3A2C,hair=1,hairVar=2,hairColor=#6A4426,beard=1,beardStyle=1,skin=#B98060,tier=2,palette=look  walk,run,dwalk,drun,didle,draw,hurt,fall,lying
knightm  man    outfit=5,armorStyle=3,bodyForm=7,pauldron=3,skirt=2,gloves=3,boots=3,helmet=1,helmStyle=3,helmForm=9,crest=3,plumeColor=#B83A32,weapon=1,bladeForm=1,neck=0,head=4,beard=1,beardStyle=2,hairColor=#5A3A26,tier=4,palette=look  walk,run,didle,hit1,hurt,fall,lying
archerw  woman  outfit=3,armorStyle=1,bodyForm=2,armsOrnament=16,helmet=1,helmStyle=1,weapon=3,bowForm=1,gloves=1,boots=1,topColor=#5E6A3A,bottomColor=#4A3A2C,hair=3,hairVar=0,hairColor=#3A2418,tier=2,palette=look  walk,run,didle,draw,hurt,fall,lying
```

**Clips, in four facings**, for every ladder look: idle, walk, run, hurt, the drawn idle, walk and run, hit 1, the fall and
lying; lt6, lt6w and lt6m with the living finish. The style slice's looks keep their clips, fixed.

**The open items, fixed first** (the owner, 2026-10-23; each with its check):

| item | areas | the check |
|---|---|---|
| the spear crosses the knight's helm and face in the side walk and run | LADDER-ARMS: forward-leaning side carry views, a 38 px run carry; LADDER-DOLL: the clips, `RUN_CARRY` | W7: no spear pixel in the head box in any R / L loco, run or drawn frame; B10 RUN in budget |
| the spear vanishes for a frame on a hit, then is held stiff | LADDER-DOLL: hurt lines keep `held`, a one-step knock-back angle; LADDER-ARMS: the hurt arm key | C3a: the held part in every hurt frame; the hurt angle one step off the idle's |
| the thrust is a static poke | LADDER-ARMS: a wind-up drawn back to the hip, a lunge, a recoil, with action legs; LADDER-DOLL: the clips | C5 restated; the judge at game speed |
| blocks are one held frame; the guard's side block reads as a sword held low | LADDER-ARMS: raise, impact and brace keys, the sword cocked high and back in side views, the spear's block with both arms raised, `fx spark`; LADDER-DOLL: 3-frame block clips | C5 restated; the judge reads the side block |
| deaths are lumps with gear floating: the spear level over the head, the bow arcing in the air, the sword standing up | LADDER-ARMS: drop views lying flat; LADDER-HEAD: the kite and helms lying; LADDER-ARMOUR: legible `lie` views; LADDER-DOLL: the placement | C3e restated; the judge reads each corpse at 1x |
| the archer is brown on brown, lost on dark ground | LADDER-DOLL: C14, leather parting from cloth in rampsOf, the archer's colours; LADDER-ARMOUR: the leather's values | C14 |
| the knight's plate is pale and flat | LADDER-ARMOUR: the plate's planes (a deeper shadow side, one ridge, fewer light pixels); LADDER-DOLL: the steel ramp | C14's plate line; the judge |
| the guard's beard reads as a mask facing R / L | LADDER-HEAD: the nasal's side view, a `when capped` beard view | the eye, nose and mouth pixels show; beard and helm 25 luma apart; the judge |
| the bow is too thin facing D / U | LADDER-ARMS | C6 restated |
| the sword's swoosh is oversized and drops below the feet facing D | LADDER-ARMS: the arc re-cut; LADDER-DOLL: B1 restated | C5's effect line |
| knightm and archerw have no attack frames | LADDER-DOLL: weapons and gaits | hit 1 and draw present |
| iteration 3's asks: the knight's block with both arms raised, helm sizes and faces, the archer's hair and beard noise, knightm's beard colour | LADDER-ARMS, LADDER-HEAD, LADDER-DOLL | the judge |

**Areas** (5, files disjoint; contracts 1). Art areas edit only their maps, preview on isolated pools
(`%TEMP%\claude\emb_P2c\ladder\<area>\doll\`), and propose clip lines and region slots in their reports. LADDER-DOLL owns
every code, clip and gate change. Each art area fixes its open items first, then draws the ladder.

- **LADDER-DOLL** (engineer; a hard-systems area, run at engineer-high where the workflow allows, and escalate after two
  failed attempts; BDIR build-p2c-ladder).
  - Files: rpg/art/doll/*.h, *.cpp, gen/** (at delivery), clips/** (every clip file; the frozen loco walk and run numbers
    stay), maps/legend/** (ADD only); tools/doll/**; rpg/art/art_rig.h/.cpp, art_fx.h/.cpp; rpg/art/art_human.h (ADD the
    ladder fields to HumanLook) and art_human.cpp's key() (rig 2 only, no painter line); rpg/sim/anim.h/.cpp,
    gear_look.h/.cpp (ADD the ladder fields only), looks.cpp (the rank tiers only), gear_ladder.h/.cpp (new);
    rpg/view/render.cpp (the people bake and the glow pass), render_anim.cpp, view.h, loot_ui.cpp (the Mythic aura),
    witness_state.cpp, script_p2c.cpp; tools/tests/test_doll*.cpp, doll_motion.h, test_anim.cpp, test_folk_sim.cpp (F9);
    tools/preview/doll_gallery.cpp; CMakeLists.txt (gear_ladder.cpp only); tools/scripts/p2c_slice*.txt and
    p2c_ladder*.txt; witness/p2c_slice.witness.json and p2c_ladder.witness.json; docs/P2C_CONTRACTS.md 8 (as built); this
    section's "Ladder slice results" (append).
  - Core:
    - the ladder data (contracts 3b): the HumanLook fields; gear::lookTier and gear::ladderKit; wearGear filling the tiers,
      metal kinds and gem colour; census::dress's rank tiers; the looks macros (hero, heroine, kit, npc, wpn, rarity,
      tier) in every looks reader, and `rpg_test --expand-looks`;
    - the resolver: the slot tier flags, the dials by tier (the caps, the heartland line), the finish regions R P G N W
      by each piece's own tier, the finish tiles by tier, the Metal ramp from the tint or the metal kind (with T1's
      compression and C14's plate contrast), the Blade slot, the Glow ramp from the glow or gem colour, B8's glow
      allowance and the colour fallbacks; leather parting from cloth (C14);
    - the living finish (contracts 6b): the glow plane composed with each atlas (cropped, packed) and drawn as pulsed
      additive light after the lighting with each actor's phase; the edge spark on the grip-to-tip line; the motes; the
      stir clips; all at rig 2 only;
    - the open items' code: the spear's hurt lines; the thrust and block clips from LADDER-ARMS's proposals;
      `RUN_CARRY` with the 38 px carry; the lying placement (C3e restated); the twins' weapons and gaits; B1 restated;
      test_folk_sim's F9 (the drawn frames moved to RigPart::Drawn);
    - the gates: the C12, C13 and C14 lines; C1 source L for the ladder looks; C3e, C5 and C6 restated; B8 restated;
      B10 with the ladder looks (D19's allowance shown beside the base budget); the pool gate at 1.75 MB (test_doll.cpp
      checks 1.0 today); a selftest for every new line;
    - the scripts p2c_ladder, _phone and _pack (the hero changes kits in the start village: `ladderkit`);
      witness/p2c_ladder; `tools/doll/owner_pack.py --ladder`; exes s5 and on, s5 published in its first working block
      (the fields, flags, regions and macros, so the artists see their regions recoloured by tier).
  - Depth: the Mythic aura on doll frames (C7's aura line, lt6m); the rune crawl (three phases by position, so the light
    runs along the seams); the regional tier lift.
  - Delight: the gallery's `ladder` board (a look through T1-T6 in four facings) and `finish` board (one living-finish
    cycle as 12 frames and a 3x GIF).
  - Done:
    - `node tools/doll/pxc.mjs --check` is clean;
    - `rpg_test --doll --gates --parity` exits 0 with the ladder looks in its matrices: B1 16 / 16 at 0 px on the figure,
      B8 0, B10 within budget (T5-T6 within D19's allowance, reported beside the base budget), B11 limbs, B11 wear and
      B12 0, C3-C6 0, C12, C13 and C14 0, the selftests ALL CAUGHT;
    - `rpg_test --doll-coverage --scope ladder --gates` finds nothing missing for every ladder look and for the
      Heartland kit at every tier, man and woman, seeds 1..3;
    - `rpg_test --doll --motion --limbs-looks tools/doll/ladder_looks.txt --report` shows 0 failures;
    - every open item's check passes;
    - witness/p2c_ladder and witness/p2c_slice pass; the p2c_ladder scripts show 0 failures; the owner pack builds;
    - rig 1 is unchanged (art_hash TOTAL 317eef6efe4d8229, village-art 65 / 0, `--anim` 0, `--arms` unchanged); the full
      `rpg_test` is ALL OK; save_test is ALL OK;
    - the WSL clang 18 build passes `--doll --gates` (no memcmp on padded structs; paths through EMB_SOURCE_DIR).
- **LADDER-ARMOUR** (artist, at xhigh effort).
  - Files: rpg/art/doll/maps/armour/** (torsos, hangs, sleeves, pauldrons, tiles); rpg/art/doll/maps/cloak/** (ADD only:
    `breeze` keys and `when` views; P2b's views untouched).
  - Core:
    - the Heartland bodies on the man: the padded gambeson (new torso and hang, with T1's wear spots), the studded leather
      (T2), the mail with its brass edge (T3), and the plate re-shaded (T4-T6: a deeper shadow side, one ridge, fewer
      light pixels: the knight's "pale and flat");
    - the flavour bodies on the man: scale with tassets (Imperial), lamellar with a mid skirt and its scallops and tassels
      as ornament regions (Jade), mail with fur trim (Fjordfolk);
    - the woman's plate with its finish regions (lt6w, the knight);
    - pauldrons fur.2, scale.2, scale.3, scale.4 and plate.4 (shared), riding the lean and lifting on wind-ups;
    - the finish regions R P G N W in every base view of every armour map the ladder looks use (contracts 3b);
    - legible `lie` views wherever the turned torso reads as a lump;
    - the cloak's stir: `breeze` views on the idle keys of the noble and travel cloaks (the hem out a pixel), and a run
      flare a pixel wider `when cloak.t5`;
    - proposed region slots and clip lines in the report.
  - Depth: plate catching the lit edge as the body turns; the brass edge on the mail glinting with the bob.
  - Delight: the T1 gambeson's shoulder patch, where the T2 leather shows a clean seam.
  - Done: C3a 0 for the armour parts on every ladder frame; B11 limbs, B11 wear and B12 0 on every ladder look; C4 (plate
    25 %, leather and padded 15 %, both bodies); B8 0; LOCO and RUN within budget (T5-T6 within D19); C14 on the leathers
    and the plate; the judge's fit review (man against woman) and read of each body at 1x.
- **LADDER-HEAD** (artist).
  - Files: rpg/art/doll/maps/headwear/helm/**, maps/shield/**; maps/beard/** (ADD only: `when capped` views of the full
    beard facing R / L; P2b's views untouched).
  - Core:
    - helms: the great helm (form3 / band3: closed, an eye slit as an N region that glows at T6, crest regions for T5, a
      gilt-rim R region); the masked helm (form8, Jade); the spangenhelm (form4, Fjordfolk); the crested and nasal helms
      with their finish regions; the leather cap re-checked at T2;
    - shields: the round (form1, shape1 and shape0: plain wood with an unpainted field at T1, painted at T2), the heater
      (form3 / shape2), the tower (form4 / shape4), the buckler (form7, the bandit), and the kite's finish regions; each
      in every key (rest, guard, block, lie, back, the hurt and run holds) with field, device, rim (R), boss (G) and boards
      (N) regions and per-facing layers;
    - every shield's `lie` flat beside the corpse (the guard's "red box");
    - the guard's beard under the nasal facing R / L (the mask);
    - the helm sizes and faces the style slice's panel asked for.
  - Depth: the crest bobbing a pixel behind the walk; a plume lifting on the breathe frame (`crest=breeze`).
  - Delight: the T6 great helm's slit breathing light in the dark.
  - Done: C3b and C3c 0 on every ladder frame, the lean and lying included; W7 (no shield over a face); C3e (shields
    flat); B8 0; LOCO 128 and RUN 64 KB or less with every shield (T5-T6 within D19); the guard's eye, nose and mouth
    show facing R / L, the beard 25 luma off the helm; the judge reads every helm and shield at 1x on the phone.
- **LADDER-ARMS** (artist, at xhigh effort).
  - Files: rpg/art/doll/maps/arms/adult/action/**, legs/adult/action/**, held/weapons/**, belt/sheaths/**, back/**,
    fx/** (not `fx glint` or `fx mote`: LADDER-FINISH's).
  - Core:
    - the spear: forward-leaning side carry views (`carryR`, `carryL`) and a 38 px run carry, so the haft never crosses a
      face; a hurt arm key that keeps the spear; the thrust with a wind-up (both fists drawn back to the hip, the weight
      back), a lunge (the front foot planted, the lean) and a recoil, with action legs; the block with both arms raised
      and the haft high;
    - the guard's block in three keys (raise, impact, brace) with the sword cocked high and back in the side views; the
      knight's the same; `fx spark`;
    - drop views that lie flat (the spear, the sword and the bow on the ground beside the hand: `anchor pivot`);
    - the bow facing D / U at three-quarters, the stave 2 px at the grip, its curve readable;
    - A3's arc re-cut smaller for every sword look (inside the frame's box plus reach, never below the soles facing D);
    - the glaive head at 16 angles, with carry and drop (Jade);
    - the finish regions on every angle view of the straight sword (R guard and pommel, P flat, G pommel gem, N fuller),
      the spear and the glaive (R socket, N rune line, G);
    - proposed clip lines (frames, ms, weapon angle and depth, `at=` steps) for LADDER-DOLL.
  - Depth: a hit-stop beat on the thrust's impact; the off hand sliding along the haft.
  - Delight: the T6 blade's runes catching the swing.
  - Done: C6 0 on every weapon map (with the bow's D / U line); C3d 0; C5 restated (the thrust, the block, the effect
    line); C3e (drops flat); W7 (no weapon over a face in loco, run or drawn frames); B10 DRAWN and ACTION within budget;
    B8 0; the judge at game speed: the thrust has weight, the blocks brace, nothing crosses a face.
- **LADDER-FINISH** (artist, at xhigh effort).
  - Files: rpg/art/doll/maps/finish/** (new): the finish tiles (`role overlay finish.<name>`, `tags tile`, `on-region`),
    gem stamps, rune glyph tiles, and the living finish's sprites (`fx glint`, `fx mote`).
  - Core:
    - STYLE_P2 27.3's finish language, drawn once for the whole ladder: wear (patches with stitch lines on cloth and
      leather, tarnish on metal, a frayed hem's notches), etching, filigree (the Heartland's oak leaves; Jade's cloud
      scrolls, Imperial laurel and Fjordfolk knotwork for the flavour row), gems (1x1, 2x2, 3x2), rune glyphs for seams;
    - the glow colours per metal (27.3) and the living finish's numbers (periods, light, motes) proposed to LADDER-DOLL,
      tuned by eye at game speed on lt5, lt6 and lt6w;
    - the edge spark (a 3x3 to 5x5 four-point star in three frames) and the mote;
    - the line review: the Heartland ladder T1-T6 and the flavour row at 1x and 3x on grass and on dark ground, with
      asks to the other areas in its report.
  - Depth: the rune crawl's phase pattern; each people's rune glyphs.
  - Delight: the T6 runes flaring once on the strike's impact frame.
  - Done: every finish tile and sprite B8 0 (no orphans, no noise, the tier's colour steps); C12 b and d on the
    Heartland line; C13 0 on lt5, lt6 and lt6w; the judge reads T1 as worn but whole, etching and filigree as decoration
    (never noise) at 1x and 3x, and the six tiers as six steps at 1x.

**The ladder slice gate** (the judge measures first and states the whole protocol in round 1; at most 4 rounds; fix rounds
verify only their items):
1. Coverage: every ladder look resolves with nothing missing (C1 source L on the slice looks).
2. B1 restated: 16 / 16 at 0 px on the figure.
3. Motion: B11 limbs, B11 wear and B12 at 0 on every ladder look.
4. Gear: C3 a-f at 0, with C3e restated.
5. C4; C5 and C6 restated.
6. C12, C13, C14.
7. B8 at 0 (48 + 3 glow); B10 within budget, T5-T6 within D19's allowance (the pack shows the short-cape fallback beside
   it for the owner's call).
8. Size, crispness and contrast (as the style slice's item 10).
9. Rig 1 unchanged.
10. The look review at 1x on the phone and at 3x / 8x / 12x (STYLE_P2 27.9's acceptance), with every open item closed.
11. The owner's sign-off on the ladder pack. It also settles D13-D23.

### Production areas (wave 2) and the flip (wave 3)

Files are disjoint (docs/P2C_CONTRACTS.md 1 gives the slice's split; this list is wave 2's). Every art area keeps the
slice's look and conventions, previews on isolated pools, and reruns the gates on its own looks before it delivers.

**The ladder in production** (re-plan 2026-10-23). Every area keeps its list below and adds the ladder, matching the
ladder slice:
- **DOLL-C**: the ladder for every form, helm, shield, weapon and role (flags, dials, finish regions and tiles, ramps, the
  Blade and Glow slots); C1 source L (every people x tier x weapon family x body, seeds 1..3); C12-C14 over the matrices
  M8-M12 and L; the living finish at scale (glow planes inside C8's people budget). Files add maps/legend/** (ADD only)
  and rpg/sim/gear_ladder.* (from LADDER-DOLL).
- **FOLK-SIM-C**: the rank tiers for every role (the census, makeLook, story knights; companions through wearGear), the
  regional lift, and `rpg_test --ladder-pace` (proposal 3).
- **ARMOUR-M, ARMOUR-W, ARMOUR-X, HELMS, SHIELDS, WEAPONS**: the finish regions R P G N W in every base view, T1's wear
  spots, T5-T6's gems and seams, and every dial part the ladder reaches (every pauldron size, long tassets, crest sizes).
  ARMOUR-X adds the cloak's stir (maps/cloak, ADD only).
- **FINISH** (artist, at xhigh effort). Files: maps/finish/**.
  - Core: the finish tiles for every material (cloth, leather, metal); the twelve peoples' filigree motifs and rune
    glyphs (the ladder table's last column); gem stamps; the living finish's sprites at scale; the glow colour of every
    alloy checked on its people's T5-T6 kit.
  - Depth: the hidden peoples' strange finishes (hiddenLook's filigree and gilding).
  - Delight: a Legendary's runes in its maker people's own script.
  - Done: C12 and C13 over source L, B8, and the judge's ladder review per people (T1-T6 boards, man and woman).
- **SWITCH**: the living finish in every draw path (the paperdoll at whole multiples, the party list, portraits, the
  creator preview), and the Mythic aura on doll frames if the ladder slice left it.

- **DOLL-C** (engineer-high).
  - Files: rpg/art/doll/*.h, *.cpp, gen/**; clips/{loco, death, postures, kin_youth, kin_child}.clip; tools/doll/**;
    rpg/art/art_rig.h/.cpp, art_fx.h/.cpp; tools/tests/test_doll*.cpp, doll_motion.h, test_rig.cpp;
    tools/preview/doll_gallery.cpp, doll_poses.cpp; docs/P2C_CONTRACTS.md 8.
  - Core:
    - the production resolver: every value, flag, region slot and tile of contracts 3, for every form and band;
    - coverage sources C and E;
    - the compositor at scale: the library, two hands, shields, pauldrons, clip conditions, turned rolls;
    - effects from maps for the 9 families;
    - the matrix families M8 (armour forms x bodies x builds), M9 (helms), M10 (shields x weapons), M11 (every weapon
      sheathed, carried and drawn) and M12 (the heaviest kits: plate, large pauldrons, a tower shield, a spear or
      greatsword, a cloak, a crested helm);
    - the gates C1, C3, C6 and C11, and C8's atlas, bake and pool lines;
    - death.clip's fall and drop lines for every family, and postures.clip's gear keys (Ride, Kneel, Downed, Sit,
      Throne, Carry: D12);
    - the bake queue for DRAWN and ACTION on aggro.
  - Depth: `--doll-arms` reports with 12x crops of every failure.
  - Delight: the gallery's `kit` board turning a look through every armour form in its four facings.
  - Done: C1's harness, C3, C6 and C8 (atlas, bake, pool) green, B1-B12 green, `pxc --check` clean, the selftests ALL
    CAUGHT, and rig 1 unchanged until the flip.
- **FOLK-SIM-C** (engineer).
  - Files: rpg/sim/gear_look.cpp/.h, looks.cpp, anim.h/.cpp, skills_game.cpp (the parry's blockT only), game.h (the
    one field Actor::blockT); tools/tests/test_anim.cpp, test_folk_sim.cpp.
  - Core:
    - the herald's padded coat and tabard;
    - the drawn and carried state for every weapon class (spears and staffs carried; bows slung; foes, guards and
      companions drawn while a hostile is in aggro);
    - blockT and kBlockShow wired to the parries;
    - the crossbow's pick (aim, shoot, crank) and the roll's bash flag;
    - the census reaching C1's dead parts (the woman's culture cuts, the kids' wear and headwear, face paints, the
      braided beard, the topknot, the turban), or a list for the planner to rule on;
    - fighters' faces under open helms (C11);
    - the heavy finisher's impact flag (SWITCH's camera nudge).
  - Depth: guards turning to face the threat as they draw.
  - Delight: a captain's crest a step taller than his men's.
  - Done: `--anim --gates` green (A2 per family), `--folk-sim` green, C11 green, no balance test moved (p1b-engine,
    skills, foes).
- **ARMOUR-M** (artist-xhigh). Files: maps/armour/man/**.
  - Core: the 8 forms on the man in every key the clips name (loco, run, drawn, actions, death, postures); the skirt
    dials as hang families; the tabard; ornaments as optional regions (plumes, fur trim, gilding, studs, etching,
    tassels, scallops, rivets, filigree); the M12 rank marks.
  - Depth: the Ebony form's obsidian spikes as a `when` region.
  - Delight: plate catching the lit edge as the body turns.
  - Done: C3 and C4 (man), B11 and B12 on every man armour look, B8.
- **ARMOUR-W** (artist-xhigh). Files: maps/armour/woman/**.
  - Core: the same 8 forms cut to the woman (STYLE_P2 25.3) and her tabard.
  - Depth: her surcoat's split skirt in the run.
  - Delight: a gilded breastplate's ridge glint.
  - Done: C3 and C4 (woman), B9 (armour pairs), B11, B12, B8, and the judge's fit review.
- **ARMOUR-X** (artist). Files: maps/armour/shared/**, maps/armour/tiles/**.
  - Core: pauldrons in 3 families x 3 sizes, both sides, all keys and builds; the material tiles (mail, scale, lamellar,
    quilt, studs, rivets, leaf, lames, stitch).
  - Depth: fur trim that ruffles a pixel in the run.
  - Delight: horsetails on large pauldrons swinging behind.
  - Done: C3 (pauldrons ride the lean, the bob and the swings), the tiles readable at 1x (the judge), B8.
- **HELMS** (artist). Files: maps/headwear/helm/**.
  - Core: the 12 shapes with crest, plume and horsetail regions, face and hair masks, and lying views where needed.
  - Depth: the hair under open helms by style.
  - Delight: plumes that bob on the walk.
  - Done: C3b and C3e on every helm x hair style x body, and B8.
- **SHIELDS** (artist). Files: maps/shield/**.
  - Core: the 8 shapes and the legacy round in every key (rest, guard, block, bash, lie, back, ride), with field, device
    and rim regions; the shield arm's keys for every family's shield-bearer frames.
  - Depth: devices drawn per culture.
  - Delight: sparks when a mace meets a shield.
  - Done: C3c, B10 LOCO and RUN with every shape (M12), B8.
- **WEAPONS** (artist-xhigh). Files: maps/held/weapons/**, maps/held/civil/** (relabel only), maps/belt/sheaths/**,
  maps/back/**.
  - Core: every weapon at 16 angles with carry and drop views; the 8 blade, 3 polearm and 5 bow forms; the crossbow's
    states; a sheath for every sheathed kind; the quiver, the bow and the staff on the back; legendary and Mythic glints.
  - Depth: the culture hilts.
  - Delight: arrows visible in the quiver.
  - Done: C6, C3d (with the action areas), B8.
- **FX** (artist). Files: maps/fx/**.
  - Core: the 9 family effects re-cut to A3's three tones, in side and down orientations, with the finisher tint.
  - Depth: chips and dust that fall with gravity over the follow.
  - Delight: the impact sparks on metal.
  - Done: C5's effect timing (with FOLK-SIM-C), B8, and the judge's read.
- **ACTIONS-1H** (artist-xhigh). Files: maps/arms/adult/action/onehand/**, maps/legs/adult/action/onehand/**,
  clips/actions/{unarmed, dagger, sword, axe, mace}.clip.
  - Core: those families' three variants in four facings on both bodies, every form through the library; their guard
    stances and drawn walk and run (`part drawn`); the sword's parry block.
  - Depth: a parry pose; a shield bash on the roll.
  - Delight: the impact flash and the camera nudge on heavy finishers.
  - Done: C5 (these families), C3d, B11 on their drawn walk and run, B10 ACTION and DRAWN.
- **ACTIONS-2H** (artist-xhigh). Files: maps/arms/adult/action/twohand/**, maps/legs/adult/action/twohand/**,
  clips/actions/{greatsword, spear, staff, bow, crossbow}.clip.
  - Core: those families' variants (glaive and halberd sweeps), the bow's draw and release on both bodies, the crossbow;
    their guard stances and drawn walk and run.
  - Depth: the spear set against a charge (the guard, lowered).
  - Delight: the bowstring's twang frame.
  - Done: C5, C3d, B11, B10 (M12 spear and greatsword looks).
- **ACTIONS-BODY** (artist). Files: maps/arms/adult/action/body/**, maps/legs/adult/action/body/**,
  clips/actions/common.clip.
  - Core: the roll (turned crouch frames: STYLE_P2 25.8, item 8), the cast (gather, release; with a staff and empty
    handed), and hurt variants with gear.
  - Depth: the cast's glow pixel lighting the face.
  - Delight: a shoulder roll that tucks the helm.
  - Done: C5 (roll 4, cast 2 for every body), C3 on their frames, B8.
- **SWITCH** (engineer-high).
  - Files in wave 2: rpg/view/** (render_anim, render, terrain, hud, party_hud, paperdoll, creator, loot_ui, home_view,
    party_view, render_sea, witness_state, script_p2c), rpg/art/art_party_portrait.cpp, rpg/main.cpp, web/**,
    tools/preview/** (but doll_gallery and doll_poses), tools/scripts/p2c_{gear, swings, menus, world}*.txt and
    p2c_playtests.txt, witness/p2c_switch.witness.json.
  - Files in wave 3 (serial, every other area delivered): rpg/art/art_human.cpp/.h, art_life.cpp/.h, art_internal.h,
    rpg/art/rig/**, art_monsters.cpp, art_items.cpp, art_rig.h/.cpp, CMakeLists.txt, tools/art_hash.cpp,
    tools/tests/test_rig.cpp, test_anim.cpp, test_arms.cpp, tests/fixtures/golden_anim_v1.txt, golden_village_art.txt,
    .github/workflows/web.yml, and every script and Witness file that says `rig 1`.
  - Core in wave 2 (behind rig 2):
    - every remaining draw path on the doll: the paperdoll at whole multiples, the creator preview, the HUD hero, the
      party list, portraits from A3's face, loot previews, statues through figureStill, the bather and swimmer crop
      from doll frames, the Mythic aura per doll frame, the hidden-hero silhouette checked, the rider's Ride frame per
      horse frame;
    - lying bodies sorting by their own baseline (tall grass, the loose horse);
    - the p2c scripts with their `_phone` twins, witness/p2c_switch, and the C10 goals.
  - Core in wave 3: the flip (above).
  - Depth: the creator preview animating idle, walk, run and a swing.
  - Delight: the paperdoll turning to the facing of the last tap; the camera nudge.
  - Done: C2, C7, C9, the flip checklist, CI updated; then the integration below.

### Waves

1. **Wave 0** (the planner; done, and again on 2026-10-23 for the ladder): this section, STYLE_P2 25-27,
   docs/P2C_CONTRACTS.md, the slice looks and the ladder looks.
2. **Wave 1, the style slice** (done: three iterations, committed on branch p2c-slice). The owner approved its direction
   on 2026-10-23, listed the open items and asked for the ladder.
3. **Wave 1b, the ladder slice.**
   - LADDER-DOLL lands the ladder's contract code and publishes exe s5 in its first working block: the HumanLook fields,
     the flags, the finish regions recoloured by tier, the looks macros and `--expand-looks`.
   - The four art areas start at once, by eye, on isolated pools: open items first, then the ladder (the s4 exe until
     s5 for the fixes).
   - Then the ladder slice gate: the judge, at most 4 rounds.
   - Then the ladder pack, and the owner's sign-off (D13-D23, D9 still open).
4. **Wave 2, production.** All 14 areas start at once after the ladder sign-off. DOLL-C and FOLK-SIM-C publish their
   contract pieces in their first working block. SWITCH ports the menus behind rig 2 and deletes nothing yet.
5. **The barrier**: every area delivered, then C1, C3-C6, C8 and C12-C14 green at rig 2, then the owner's go on the flip
   pack.
6. **Wave 3, the flip** (SWITCH, serial), then **integration**: the one full regression.
   - Every CI command on MSVC and on WSL clang 18; `rpg_test --seeds 1..20`; save_test.
   - The script sweep; Witness `test --repeat 2`.
   - The web build (EMBERVALE_WITNESS=OFF) and its size.
   - art_hash and the goldens re-recorded with pairs.
   - The C10 playtests.
   Then a verifier gate, then judge rounds (at most 4).

### What the judge focuses on (and how each done is measured)

1. **Look and feel first**, at game speed, at 1x on the phone and at 3x / 8x / 12x:
   - guards of three peoples (OSLARS and two other cultures), soldiers of two realms, bandits by family, and the hero
     and a woman knight in every band;
   - each family's three blows, live and frozen with `pose`;
   - anticipation and impact;
   - every material reading as itself, and the woman's fit;
   - corpses keeping their gear and lying under the grass correctly;
   - nothing floating, sliding, clipping, vanishing or crossing a face;
   - **the ladder**: the Heartland hero's T1-T6 line and each people's kits at 1x on the phone, on grass and on dark
     ground, by day and at night: six clear steps; T1 okay / mid (plain, mended, whole, never ugly); the finish reading
     as craft, never noise; the living finish quiet, readable and never in step between two wearers; the peoples
     distinct at one tier; people by rank (a bandit, the village watch, the royal guard, a king), and a late hero
     outshining the king.
2. **The gates table**: rig 1 unchanged until the flip; at the flip, the before / after pairs.
3. **Protocol**: the judge states its full test protocol in round 1, tests from the state a player starts in (a new game,
   seed 7; after the flip, rig 2 by default), and reruns every area's done with the area's own commands. An agent's
   word passes nothing.

### Risks and fallbacks

- **The A3 re-cut cannot hold 0 px** (the fist and blade interleave): the D1 fallback.
- **Budgets with shields and long weapons**: the slice measures them first, and M12 measures production. Fallbacks D4
  and D5; families stay at 4-5 frames a variant. Raising a budget is the owner's call only.
- **Region tiles read as noise at 1x**: that form alone gets per-key drawn sleeves (P2b's mail approach).
- **Volume** (about 5,000 views): shared arm poses within a family group, the angle library, tiles, shared pauldrons and
  helms, the turned roll, and rigdraft's P2a pose numbers and weapon drafts as starting points.
- **Cross-area keys** (the shield arm under every one-hand family; sleeves under action keys): the key-agreement rule
  (contracts 4) and W1 / C3a.
- **The 48-colour cap on shield-bearing, tabarded, plumed looks**: the field shares the accent, the device takes 3 steps,
  and the slice measures it.
- **The flip's breadth** (galleries, tests, CI): SWITCH lists every file it will touch in its first wave-2 report, and
  the flip lands as one serial step behind a verifier gate.
- **The frozen doll contract**: P2c only ADDs; the one restatement (pauldron values) breaks no map.
- **City bake hitches**: baking on aggro, measured by C8's city-arrival worst frame.
- **The ladder's art volume**: regions are marked once per base view, and finishes are tiles and recolours, never
  per-tier views; the dial parts were already production's.
- **Finish reads as noise at 1x**: LADDER-FINISH tunes at 1x first; a motif that fails drops its tier back to trim only.
- **The 48-colour cap** on rich kits: the colour fallbacks (contracts 3b) in a fixed order, B8 measured over source L.
- **Double dial lifts** (the census's ranks, the alloy's step, the tiers): tiers only cap (T1-T2) and add T5's plume;
  the lifts stay the sim's.
- **Rig 1 safety**: the ladder writes only new fields; art_hash, village-art, `--anim` and `--arms` prove it at every
  delivery.
- **The living finish costs frames or memory**: the glow plane is cropped and cached (12 looks); C13 measures the city
  frame with 8 T6 figures.
- **T6 too early or too common**: the tiers follow the game's floors and caps, and `--ladder-pace` measures them; if
  the owner wants a slower climb, a rule changes (the band table), never the art.

### Process (the owner's rules for P2c)

- Commercial 16-bit quality, judged by eye at 1x and zoomed.
- Design by eye first, then make the checks agree. A check that contradicts an eye-approved frame is re-tuned with the
  judge's ruling and recorded; the art is never bent to fit a check.
- One full regression per milestone: at integration, after the flip. Fix rounds verify only their fixes.
- Screenshots and iterations are never capped. An `eyes` agent turns shots into text for long-lived agents.
- Builds: `tools/slot.sh cmd //c "set BDIR=<dir>&& tools\\build.bat <target>" < /dev/null`. Each engineer and judge has
  its own BDIR; nobody builds into build/ or another area's directory.
- Never kill a Desktop Embervale.exe. No area makes state-changing git operations (the owner commits). Screenshots go to
  %TEMP% and are deleted after review.

### Slice results (SLICE-DOLL fills at the slice gate)

**Iteration 1, SLICE-DOLL (2026-10-23; BDIR build-p2c-doll, pool 444 parts / 7854 views / 914 clips, 891 KB, hash b7d5aa9e,
taken mid-delivery of SLICE-ARMS and SLICE-HELM-SHIELD; SLICE-ARMOUR not yet in the tree).** Exes s1 and s2 published in
%TEMP%/claude/emb_P2c/bin (LATEST.txt).
- Held: B1 parity 16 / 16 at 0 px (with the arms area's re-cut sword_r in the tree); B2, faces, hands, B8 (1120), B9, ramps,
  B11 leg mass, B11 limbs (649 incl. the slice looks) all 0; selftests B11 and C3-C6 ALL CAUGHT; rig 1 unchanged (art_hash
  TOTAL 317eef6efe4d8229, village-art 65 / 0, `--anim` 0); full `rpg_test` ALL OK; save_test ALL OK.
- Open on the art in flight: B10 slice knight RUN 72.7 KB (64) and one more; B11 wear 41 (back/bow coverage until the bow
  clips are integrated, W1 W5 W7 on the slice looks); B12 20 (the knight's run boots); C3 316 (tabard, plate, leather and
  the shield arm's keys not in the pool yet); C4 13 (no slice armour yet); C5 2 (the spear placeholder clips: the tip does
  not move); C6 5 (the bow has no carry view). `--doll --motion` on the slice looks: 240 side frames, 55 failures.
- Not built yet: effects drawn by art_fx from maps (ComposeOpts::withFx); the arms area's spear.clip / bow.clip proposals
  integrated (placeholders from tools/doll/p2c_clips.py --families stand in).
- Needs another owner: tools/tests/test_folk_sim.cpp F9 (lines 327, 340) still expects the drawn idle / run in
  RigPart::Action; contracts 6 moves them to RigPart::Drawn (`rpg_test --folk-sim` F9 fails 2 until changed);
  rpg/view/render.cpp's bake queue (`want`) does not pre-bake RigPart::Drawn (rigTex bakes it on first use).

**Iteration 2, SLICE-DOLL (2026-10-23; BDIR build-p2c-doll, pool 458 parts / 8334 views / 930 clips, 938 KB, hash
fb2a883613fc4d7b; exe s3 published).** The blind panel's code items: corpses grounded with the spear and bow level beside
them (screen-space `anchor pivot` in turned frames), the spear held through the fall, a step into every thrust / swing
(`at=` on upright frames, tools/doll/p2c_weight.py), a braced block, the spear's drawn walk/run upright (B10 DRAWN).
- Held: `pxc --check` clean; B1 16 / 16 at 0 px; B8, B11 limbs / wear, B12 0; B10 every slice look within LOCO, DRAWN,
  ACTION, DEATH (DRAWN knight 319 -> under 256 KB); C3 0, C4 0, C5 0; every selftest ALL CAUGHT; `--doll --motion` slice
  looks 240 side frames, 0 failures; dollMissing empty (C3a); witness/p2c_slice PASS (238 expects); the four p2c_slice
  scripts 0 failures; owner pack 11 boards, 0 missing inputs; rig 1 unchanged (art_hash TOTAL 317eef6efe4d8229,
  village-art 65 / 0, `--anim` 0); full `rpg_test` ALL OK; save_test ALL OK.
- Open: B10 knight RUN 72.4 KB (64): not reachable by clips (a 39 px upright spear makes every run frame >= 41 px tall,
  24 frames >= 78 KB raw); needs a plan call (a polearm RUN budget of 80 KB, or the held part in its own atlas rect).
  C6 4 (SLICE-ARMS art: bow_self R / D `release` lit bottom-right; spear D a315 lit bottom-right; spear a000 41 px vs 39).
- Art asks: SLICE-ARMS the release view's string blur reads as a white ladder (panel); SLICE-HELM-SHIELD the kettle helm
  wants a `hatpose=lie` view (an oversized disc lying); world guards draw on v1 because their lamellar / brigandine /
  form10 looks are production values (not in the slice).

**Iteration 3, SLICE-DOLL (2026-10-23; BDIR build-p2c-doll, pool 458 parts / 8399 views / 946 clips, 943 KB, hash
e42c15b04e2ab7a9; exe s4 published).** The blind panel's code items:
- The knight's spear facing down / up: the haft is now one line from the butt to the leaf. Before, the leaf floated
  14 px above the helm. Now the idle, walk, hurt, drawn walk / run and the fall's buckle carry it at the fist
  (`held=carry`), and a hand-mark grip redraws the fist over the haft (doll_compose).
- The guard's shield in the hurt facing R / L: held at the chest, 1 px knock-back.
- The guard's block: the sword cocked back while the shield squares up. Every block steps 2 px back, and the spear's
  block crouches (bob 2).
- The spear's hurt: a 1 px knock-back.
- Held: `pxc --check` clean; B1 16 / 16 at 0 px; B8 1120 / 0; B10 every slice look within budget (knight RUN 62.8 / 64,
  DRAWN 184, ACTION 106, DEATH 33.5); B11 limbs 649 / 0, wear 5592 / 0, B12 666 / 0; C3 2130 / 0 (dollMissing empty),
  C4 14 / 0, C5 11 / 0, C6 69 / 0; selftests ALL CAUGHT; `--doll --motion` slice looks 240 side frames, 0 failures;
  witness/p2c_slice 1 passed; the four p2c_slice scripts 0 failures; owner pack 11 boards, 0 missing; rig 1 unchanged
  (art_hash TOTAL 317eef6efe4d8229, village-art 65 / 0, `--anim` 0); full `rpg_test` ALL OK; save_test ALL OK.
- Open (code): the RUN part facing D / U keeps the rest view (the leaf above the helm). The 39 px carry put knight RUN at
  64.2 KB. SLICE-ARMS ask: a 38 px `carry ... when run replace` view, then p2c_clips.py `RUN_CARRY = True`. The side views
  keep the rest view behind the head, because at the fist the haft crossed the helm and the face. A forward-leaning side
  carry view (`carryR` / `carryL`, as the map header names) would cure that.
- Art asks from the panel, outside SLICE-DOLL:
  - the knight's block wants both arms raised with the haft high (sp_block's grips fix the 45 deg line; at 67 deg the
    haft crossed the face);
  - a block spark fx;
  - the kite `lie` sits over the belly (the guard corpse reads as a red box);
  - the helm sizes and faces, the archer's hair and beard noise, and knightm's beard colour (the slice look has no
    hairColor);
  - the hero's 0.4 s recovery hold, which is A3 timing (B1).

**The owner's verdict (2026-10-23)**: the direction is approved. Open items (all fixed in the ladder slice): the spear
over the knight's helm and face in the side walk and run; the spear vanishing for a frame on a hit, then held stiff; the
thrust a static poke; single-frame blocks, the guard's side block reading as a sword held low; illegible deaths with gear
floating (the spear level over the head, the bow arcing in the air, the sword standing up); the archer brown on brown and
lost on dark ground; the knight's plate pale and flat; the guard's beard reading as a mask facing R / L; the bow too thin
facing D / U; the sword's swoosh oversized and dropping below the feet facing D; knightm and archerw with no attack
frames. Then the gear ladder ("The gear ladder", "The ladder slice"). D9 (rig 1 deleted at the flip) is still open.

### Ladder slice results (LADDER-DOLL fills at the ladder slice gate)

**Iteration 1 (LADDER-DOLL, 2026-10-23; BDIR build-p2c-ladder; exes s5, s6).** The ladder runs end to end at rig 2: items ->
lookTier -> HumanLook tiers (wearGear, census::dress) -> the resolver's dials, finish regions and tiles, metals, Blade and
Glow ramps -> compose (glow plane). Seeds picked for the planned forms (`rpg_test --expand-looks`, recorded in
tools/doll/ladder_looks.expanded.txt): jade:3:1 (masked helm, lamellar, large shoulders, glaive, scallops and tassels;
seed 11 gave scale), imperial:3:2 (crested helm, tall crest, scale, tower, cloak; seed 3 gave mail), fjordfolk:3:5 and
heartland:*:7 kept. Retuned for C14: archer top #6F8F45 and hair #3E2818, archerw top #6F8F45, lt6w hair #8C5A2A, fjord3
hair #E0BC78. Measured on the tree pool (art areas' deliveries so far): rig 1 unchanged (art_hash 317eef6efe4d8229,
village-art 65 / 0, --anim 0, --arms ALL OK); full rpg_test ALL OK; save_test ALL OK; motion on ladder_looks.txt 20 looks
708 side frames 0 failures; coverage --scope ladder 56 / 56; --doll --gates: B1 16 / 16 0 px, B2 B9 B11 limbs B11 leg B12
C5 C6 C13 ok; open: B8 (orphans in jade3 walk), B10 (RUN 65.6-68.9 KB on T4 and jade3 over 64; T5-T6 within D19; lt6w
DEATH 49.2), B11 wear (COVER padded woman), C3 (C3e lying shields: round and heater lie views), C4 (padded 10 %), C12
(trim share over band on helms and shields, none yet on blades and cloaks; lt5 to lt6 2 %: no runes drawn yet), C14 (lit
edge lt2 / lt4 / lt6m a few luma short).

**Iteration 2 (LADDER-DOLL, 2026-10-23; BDIR build-p2c-ladder; the blind panel's top-of-ladder findings).** The top now steps:
T4 steel plate (the alloy metalized: the "pink clay" gone), T5 the same steel gilt with the cyan gem's breath, T6 the
Heartland's STARMETAL (pale blue-white) with glowing runes, gem and device, the Mythic ember-tempered crimson-black with
an ember glow, a pale-gold crest and a smoke-black cloak; the hero keeps his face from T4 in the open crested helm (the
great helm is one word in ladderKit to bring back: the owner's call), crest and plume in his own red. The game draws the
living finish now (render.cpp after the lighting): every glow pixel breathes with the actor's phase and the rune crawl,
the T6 blade's spark runs grip to tip every 3.3 s, motes rise; tools/scripts/p2c_ladder.txt shoots it (0 failures);
witness/p2c_ladder 33 / 33 and witness/p2c_slice 238 / 238 pass. Gates on the tree pool of the art areas' work in flight
(pxc f90054c2ae9ee1a0): B1 16 / 16, C12 a and c, C14, C5, C6, B12, B2, B9 ok; coverage --scope ladder 57 / 57; rig 1
unchanged (art_hash 317eef6efe4d8229, village-art 65 / 0, --anim 0, --arms ALL OK); full rpg_test ALL OK; save_test
ALL OK. Open, all in the art areas' maps or the owner's budget: B8 12 and W5 12 (the jade glaive: orphan pixels, the head
floating), C3 13 (the woman's round shield facing R: a 1-2 px hole, found by the new lt1w), B11 FARWIDTH 3 (the round
and heater shields' run hold facing L), C4 8 (padded 10-13 %, fjord3 mail, imp3 scale under their shares), C13 6 (the
rune tile now puts 22 glow pixels in a T6 frame: 20 is the budget with 3 motes and the spark), C12 1 (lt5's filigree
covers two step-4 glints), B10 5 (RUN 64-69 KB for T3-T4 caped looks, DEATH lt6w 52.6: content, the packer is within
5 % of the frames' area; D19 covers only T5-T6: the owner's call).


**Iteration 3 (LADDER-DOLL, 2026-10-23; BDIR build-p2c-ladder; the i2 blind panel's top of the ladder and animation).**
The living finish now reads at 3x: one light model (art::finish::frameLights, shared by the game and doll_gallery) where
the whole piece breathes (T6 0.10-1.00, T5 0.10-0.60: sign-off wanted, was 0.30-0.85 / 0.15-0.45) with the rune crawl
rippling a third of it, a halo of light round each glow pixel, a T6 gleam (a slanted band of light across the figure,
0.5 s every 3.3 s), the Mythic's embers (6, licking 10 px up) instead of motes, the T5-T6 cloak and crest stir on the
idle's breathe frame (the `breeze` views the art drew), and the day draw at 0.85-1.0. The Mythic is blackened steel with
gold trim, amber runes, a flame crest and a black-crimson cloak; T6 keeps T5's plume and large pauldrons; the cloak's dye
climbs (T4 travel brown, T5 crimson, T6 royal blue, Mythic black crimson), so the back views differ; the alloy keeps only
a cast (T4 no longer mauve, T5 no longer beige). Culture helms and shields the library has not drawn wear the nearest
drawn form (Dune, Marsh, Highland, Steppe T3 no longer bare). Dropped weapons lie 3 px up under the corpse. The archer
skin #CA8E66, hair #8A5A30. Measured on a snapshot of the art areas' maps (LADDER-DOLL/x.pool): --doll --gates --parity
0 failures (C12 118, C13 73, C14 171 checks); motion 21 looks 780 side frames 0; coverage --scope ladder 57 / 57;
witness p2c_ladder 33 / 33, p2c_slice pass; rig 1 unchanged (art_hash 317eef6efe4d8229, village-art 65 / 0, --anim 0,
--arms ALL OK). Open (art areas): helm forms kettle and conical-aventail undrawn; no glow on the cloak's back (a rune hem
`N` region on the noble cloak would give the T6 / Mythic back view its light); the guard's beard mask (R / L); the mail tile's
1 px checker.
**Polish iteration 1 (LADDER-DOLL, 2026-10-23; BDIR build-polish-doll; the blind panel's polish list, items P2, P5 grip,
P6, P7).** P2: the living finish reads at game scale: the glow's halo is a 2 px band, a T6 figure breathes a halo of light
round its helm and shoulders, the Mythic burns (flame tongues 1-4.5 px off every upper edge over a fire ring, on a smooth
noise: no flicker), the T6 blade's edge sweep (a white head, a 6 px glowing tail, grip to tip in 0.40 s every 2.75 s, a
star at the tip and on the strike), motes and the Mythic's embers (12, licking 13 px) live in world space so a walker
trails them and a striking Mythic sheds fire off the blade; the cloak hem and plume stir in the walk's passing frames too.
C13 restated (contracts 6b: 112 bright px, 320 soft px at 0.45 or less, still lights 0.25 per 1/60 s, a T6 figure 40 px of
soft light or more; measured: lt6m 96 bright, 0.065 step; 8 figures 0.043 ms). doll_gallery `motion:<tag>` plays a look at
game speed with the game's own light code. P6: corpses lie whole (the legs no longer stand up out of the body: compose's
screen items are the views that name `anchor pivot`); DEATH 52 KB. P7: the player starts in T1 gear (the T1 floor: an
empty body or feet slot draws T1's padded jerkin and leather boots at rig 2), and census fighters vary man to man
(HumanLook::kitVar from the face: helm, shield, gloves, boots a tier either way, dials, bare hands, a sergeant's cloak, a
bare head in the village watch); a T2 plate pauldron and the watchwoman's tabard no longer send a guard back to v1.
Integrated the art areas' maps in the tree at 14:50 (pool 4d10c00b4f144bba): `--doll --gates --parity` 22 / 22 ok on MSVC
and on WSL clang 18; C1 source L 57 / 57; `--doll-arms` 0; rig 1 unchanged (art_hash 317eef6efe4d8229, village-art 65 / 0,
--anim 0); save_test ALL OK; tools/scripts/p2c_polish.txt 0 failures. Open: the hero's creator default is brown-haired in a
blue shirt (appearance.h, not the doll: A3's colours there would make the new player the ladder hero); census looks whose
armour form has no woman's map stay v1 (scale, brigandine, lamellar); the art areas still at work after 14:50 need the
pool regenerated (`node tools/doll/pxc.mjs`) and the gates rerun.
**Polish iteration 2 (LADDER-DOLL, 2026-10-23; BDIR build-polish-doll; the blind panel on i1).** P2: the T6 aura stands one
clear pixel off the silhouette as a 2 px warm white-gold band (the dark outline keeps its edge; no longer lost on foliage);
the Mythic's fire licks in separate tongues off the crown only, no fringe down the sides, and its blade flashes white-hot
on the strike; the pieces breathe as one (crawl 0.18). P7: census fighters vary more man to man within their people
(kettle hats on a third of the bonnet men, the coat a step either way, cap dyes, shield faces); `p2cline npc` shows them
as the game dresses them, weapons in hand (tools/scripts/p2c_polish2.txt). P6 corpses: a legs `lie` foot patch (6 px, in
line with the shin) waits for the integrator. P5 Jade run carry and P6 bow: LADDER-ARMS's maps. Measured on a snapshot
pool of the tree's maps (15:33) plus the feet patch: `--doll --gates --parity` 22 / 22 ok (C13 113 / 0); coverage --scope
all 97.3 % (unchanged); rig 1 unchanged (art_hash 317eef6efe4d8229, village-art 65 / 0, --anim 0); full rpg_test ALL OK;
p2c_polish2.txt 0 failures; WSL clang 18 -fsyntax-only 0 errors on the 3 changed C++ files.

**Polish iteration 3 (LADDER-DOLL, 2026-10-23; BDIR build-polish-doll; the blind panel on i2).** P2: the T6 aura band is
gone ("a blurry pale-green selection halo", "the only anti-aliased element"): the light lives ON the figure as a rim one
pixel inside the dark outline, upper-left lit, breathing with the runes (kRim 0.75); the Mythic gains a fire rim up both
flanks below the face (kRimM 0.85, flame noise: it no longer sinks into dark grass) and its drawn flame crown burns
(fire-coloured mask pixels lit at kCrown 0.42 with a flicker: "the Mythic flames still don't glow"). C13 bright cap 112 ->
128 (lt6m 119, lt6 84, lt6w 53); soft unchanged (320; lt6m 219, lt6 160). P7: rank-and-file fighters (body tier 4 or less)
take a livery dye lot from their own kit hash (darker, sun-faded, deeper, pale, or as issued) and their own hose (brown,
grey, the livery's dark); officers, lords and kings keep the dear dye (C14 nking held). tools/scripts/p2c_polish3.txt.

**Polish iteration 3 integration (2026-10-23; BDIR build).** Merged LADDER-ARMOUR (plate, plate_hang man+woman, leather,
lamellar, lamellar_hang, cloak noble/noble_front/travel), LADDER-HEAD (crested, crested_crest, leathercap, masked, heater,
round), LADDER-ARMS (glaive), LADDER-FINISH (glyph) and LADDER-DOLL (art_fx, doll_resolve); pool hash 4aa10b4c73310c16.
Two integration fixes: (1) the dye lot's darker dyeing applies only to a cloth of luma 110 or more (the heartland bandit's
brown, darkened, put his lit edge 33 over the darkest ground: C14 failed on nband); (2) a leather wearer's Metal slot is
iron (doll_ramps: `mk == 1` draws kMetalKind[3]), because sword.pxm's held role has no library dot and so always paints in
Metal, not Blade: T1/T2's sword and the shield boss drew in the leather's browns (LADDER-ARMS repro). Gates 0 failures;
art_hash 317eef6efe4d8229, village-art 65/0, --anim 0, full rpg_test ALL OK.

### Production results and the flip (the integrator fills)


**Alloys iteration 1 (LADDER-DOLL, 2026-10-23).** Each culture alloy now draws as its own metal family. There are 16
families, each with a dark core, a wide lit plane, a hard glint and a sheen model. A per-people table keeps a people's
2-3 alloys and its neighbours' alloys apart (gate C15: within a people min dE 20.3, neighbours min 12.3; every family
under every sheen passes the metal test). The T6 living finish is stronger (rim, sweep, gleam; C13 bright px lt6 126 of
128). A new game at rig 2 starts the player's own creator hero in T1, and real equipped gear sets the tier after that
(tools/scripts/p2c_alloys.txt, all expects pass). Rig 1 is unchanged (art_hash 317eef6efe4d8229, village 65/0, anim 0).
Open: inventory icons still tint from gear_look's exotic() hue, so about 70 % of alloys differ in hue from the doll
(gear_look.cpp gearIcon should take gear::alloyFamilies' ramp). Census NPCs pick their family by tint.

### P2c alloys i2, LADDER-DOLL (2026-10-23)
- A1: 16 re-cut alloy families (no skin-coloured pinks; warm families part by value), chosen by the alloy's recipe
  (bronze everyday -> warm, iron -> cool, later metals never bronze); warm bodies at T5+ take silver trim; band
  bronze/iron/obsidian/emberforged retuned; the alloys board wears undyed charcoal cloth so the metal is what reads.
  New gate C15e. Gates B1-B12, C3-C6, C12-C15 pass (C15: min dE in a people 22.1, between neighbours 13.2).
- A5: T6 gleam 2.8 s / 0.8 s / 6 px, motes 6 / 0.32 s / 1.5 s / 12 px (lt6 idle at 3x: about 4.5k px change during
  the gleam against about 0.9k at rest).
- A6: p2c_alloys at rig 2, 0 failures (creator hero in T1, then T3, T4 and T6 from equipped gear).
- Rig 1 unchanged: art_hash 317eef6efe4d8229, village-art 65/0, --anim 0, full rpg_test ALL OK.
### P2c alloys i3, LADDER-DOLL (2026-10-23)
- A1: every alloy draws the metal its name says (96 families, 8 slots x 12 peoples, solved offline); a metal ramp with a
  saturated body under a hard pale lit plane; T5+ coloured alloys trimmed in their own polished metal; iron a dull lit
  plane under steel's; gilded lemon. Heartland T5 is gilded steel (LIONSTEEL now draws as white steel). C15c by metal,
  C15e by name. Gates B1-B12, C3-C6, C12-C15 pass (C15: min dE in a people 23.0, same-metal neighbours 13.2).
- A5: T6 gleam every 2.4 s, 0.9 s, 7 px.
- A6: p2c_alloys at rig 2, 0 failures (creator hero in T1, then T3, T4, T6).
- Rig 1 unchanged: art_hash 317eef6efe4d8229, village-art 65/0, --anim 0, full rpg_test ALL OK.
