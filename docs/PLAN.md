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
