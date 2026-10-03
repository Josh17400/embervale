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
