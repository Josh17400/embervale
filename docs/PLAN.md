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
