# Embervale

A pixel-art RPG in C++20 on SDL3: an endless generated world of regions, towns and caves, a simulation of townsfolk,
quests, stories, magic, fellows and the sea, and a paper-doll character renderer. Windows (MSVC) first; the same code
builds with clang on Linux and with emscripten for the web build. The simulation has no SDL in it, so it runs headless
in the test runner.

This file is the code map: the final layout of the SOLID refactor (waves 0-5, `docs/SOLID_PLAN.md`). A change that
moves code updates this map in the same commit. The layers, the seam headers, the rules for new code and the "how do I
add X" cookbook are in `docs/ARCHITECTURE.md`; the game's design is `docs/VISION_PLAN.md` and `docs/PLAN.md`.

## Where is ...

Each answer names the file and the symbol to open.

| Question | Answer |
|---|---|
| the save format | `rpg/sim/game_save.cpp`: `SAVE_VER` and `Game::serialize`/`deserialize`, every save block listed in write order at the top of the file |
| the update order | `rpg/sim/sim_schedule.cpp`: `Game::update`, one comment per step (the determinism contract: never reorder) |
| where a sound is defined | `engine/sound_ids.h`: one `X(Name, trim)` row in `SFX_LIST`; its voice is `Audio::fx<Name>` in `engine/audio/sfx.cpp` |
| where a prop family gets its size | `rpg/art/art_props.cpp`: `propW`, `propH`, `propFrames` (the `Prop` ids and family range tests are in `rpg/core/art_ids.h`) |
| how town folk find paths | `rpg/sim/life_paths.cpp`: `LifeOps::findPath` (A\* over the window), `walkOut`, the stuck watchdog |
| where the HUD vitals are drawn | `rpg/view/hud.cpp`: `View::hudVitals` (health, magicka and stamina bars), one piece of `drawHud` |
| where input is handled | `rpg/view/input.cpp`: `View::input` (held keys), `View::menuKey`, `View::event` (the SDL event pump); injected test input is `rpg/app/input_inject.cpp` |
| where a new test script command goes | `rpg/view/scripts/<subject>.cpp`, registered with `EMB_SCRIPT_CMD` (`rpg/view/script_api.h`); an rpg_test command goes in `tools/tests/test_<subject>.cpp` with `RPG_TEST_CMD` (`tools/tests/tests.h`) |
| where the Witness handlers live | `rpg/app/witness_driver.cpp`: `App::wReset`, `wInput`, `wStep`, `wSnapshot`, `wScreenshot`, `wCall`, `runWitness`; the state snapshot's fields are `rpg/view/witness_state.cpp` |
| where chunk relief is computed | `rpg/world/chunk_relief.cpp`: the relief stages of `gen::chunk()` (river beds, flats, relief bits, massifs, peaks, cairns); the coarse relief level is `rpg/world/macro.cpp`; the baked cliff faces are drawn by `rpg/view/terrain.cpp` |
| the damage rules | `rpg/sim/game_combat.cpp`: `damage`, its modifier chain numbered in order |
| the dialogue routing | `rpg/sim/dlg_routes.cpp`: `kDlgRoutes`, one row per `DLG_*` range (`rpg/sim/game_types.h`) |
| a saved mark tag | `rpg/sim/mark_tags.h`: `kMarkTags`, `kMarkRanges`, `markKey` |
| which lane hooks tests may call | the `API (SOLID 5.6)` block above the struct in each `rpg/sim/*_ops.h` |

## Build

- `tools\build.bat [target]`: Release build with MSVC + Ninja. It finds Visual Studio, CMake and Ninja itself.
- `set BDIR=build-mine&& tools\build.bat` builds into another folder, so parallel builds don't collide. The first
  configure reuses `build/_deps/sdl3-src` when it exists; otherwise CMake fetches SDL 3.4.16.
- `CMakeLists.txt` globs each folder (`rpg/sim/*.cpp`, `rpg/view/scripts/*.cpp` ...). A new file in an existing folder
  needs no CMake edit; a new folder needs its glob.
- Targets: `embervale` (the game), `rpg_test` (headless tests), `save_test`, `art_hash`, `art_preview`,
  `audio_preview` and one `*_gallery` exe per file in `tools/preview/`.
- The web build is `tools/gates/build_web.bat` (emsdk pinned in `.github/workflows/web.yml`), served from `web/`.

## Test

- `tools/gates/step_gates.sh --bdir <dir> --out <dir>`: the step gates (build, seeds, saves, art and freeze hashes,
  village art, Witness smoke, sim digest, view and audio packs, layer and move checks). `tools/gates/wave_gates.sh` adds
  every script, Witness x2, the doll gates, first launch and perf. Each script's header documents it; the baselines in
  `tools/gates/baseline/119de74/` are never re-recorded.
- `rpg_test <flag>`: one lane's checks (`rpg_test --help` lists them); `rpg_test --seeds 1..20` is the broad sweep.
- Game scripts (`tools/scripts/*.txt`) run on hidden Witness instances through `tools/gates/witness_pack.mjs`.
  Never start `embervale.exe` without `--witness` from a tool: it opens a window.
- Witness tests: `witness/*.witness.json`, run by `tools/gates/witness_smoke.sh`.
- CI: `.github/workflows/web.yml` (the clang test list, then the web deploy).

## Code map

Folders, top down. "Lane" means one game system with its own files (see `docs/ARCHITECTURE.md`, "The lane pattern").

### `engine/`: SDL3 platform pieces, no game words

| File | What it holds |
|---|---|
| `engine/pix.h` / `engine/pix.cpp` | renderer: 480x270 logical canvas, integer scaling, `Canvas` (CPU pixels) baked to `Tex`, text, screenshots |
| `engine/audio.h` / `engine/audio.cpp` | the software synth's front: `Audio`, play, the queue, `setMusic`, the listener (`audio.cpp`) |
| `engine/audio/` | the synth by part: `synth.cpp` (voices, `renderVoice`), `sfx.cpp` (**every sound effect's voice**, `Audio::fx<Name>`, and the trims and names generated from `SFX_LIST`; spell motifs, stings), `music.cpp` (music modes, instruments), `ambience.cpp` (beds), `mix.cpp` (`renderBlock`, the limiter); `audio_internal.h` is their private header (`Audio::add`/`rnd` inline: the /fp:fast rounding depends on it) |
| `engine/sound_ids.h` | **where a sound is defined**: `SFX_LIST(X)`, one `X(Name, trim)` row per sound effect, generates the `Sfx` enum, `Audio::sfxName`, the trims and the voice table; plus the `Music` and `Sting` ids (all append only); the sim includes this, not `audio.h` |
| `engine/canvas.h` | `rgba()` and `Canvas` (CPU pixels): all that painters need; `pix.h` includes it |
| `engine/mathx.h`, `engine/color.h`, `engine/font5x7.h`, `engine/music_style.h` | Vec2 and Rng, float colour, the 5x7 font, music styles |
| `engine/witness.h`, `engine/witness_emscripten.js` | vendored from the Witness test harness: never edited by hand |

### `rpg/`: the game

| Path | What it holds |
|---|---|
| `rpg/main.cpp` | `main()` and the frame loop (`frame`, `frameRequests`, `frameSteps`), the emscripten glue; it builds the platform objects in a fixed order and hands them to `App` |
| `rpg/app/` | the app shell (`App`, `app.h`): `app_setup.cpp` (the command line, `AppSetup::parse`, Witness flags, platform differences), `save_store.*` (the save file and its neighbours), `input_inject.*` (keys, clicks, fingers pushed through the real SDL event paths), `script_runner.*` (the test-script driver: built-in commands `App::runCmd`/`runStateCmd`, the autopilot), `recorder.*` (WITNESS_RECORD and replay start), `witness_driver.cpp` (the Witness handlers: reset, input, step, snapshot, screenshot, call), `app_perf.cpp` (`--perf`, EMB_TIMING, worst frames) |
| `rpg/core/` | header-only words every layer may use: `common.h` (TILE, hashes, IRect, BinW/BinR) and `art_ids.h` (the art vocabulary: `Prop` with its footprints, `Monster`, `Building`, `HumanLook` ...) |
| `rpg/culture/` | cultures, societies, names, architecture priors (`priors.cpp` is data). Pure and deterministic, /fp:precise |
| `rpg/build/` | the building designer: `blueprint.h` (public), `builder.cpp` (`design()` and the request plumbing), `build_geom.cpp` (roof and volume helpers), `build_forms.cpp` (per-purpose forms), `build_culture.cpp` (`cultureHome`, seats), `build_finish.cpp` (ground, street faces, terraces, finish), `build_detail.h` (private seam) |
| `rpg/world/` | the endless world generator by layer: `macro.cpp`, `region.cpp`, `geology.cpp`, `hydro.cpp`, `roads.cpp`; `chunkgen.cpp` builds one chunk: `chunk()` runs the stages in a fixed order over a shared `gen::ChunkCtx` (`gen.h`): Base, Towns, Harbours, Stamps, Roads, Levels, RiverBeds, Flats, ReliefBits, Ecotones, Boardwalks, Massifs, Peaks, Cairns, Flora; the roads stage is in `chunk_roads.cpp`, the relief stages (river beds, flats, relief bits, massifs, peaks, cairns) in `chunk_relief.cpp`, flora in `chunk_flora.cpp`; towns in `town_*.cpp` (the big builders are file-local planner classes: `MarketPlanner` in `town_market.cpp`, `CompoundPlanner` in `town_build.cpp`, `CityDresser` in `town_urban.cpp`, one method per stage); wayside places in `vignettes.cpp`; the sea in `sea_*.cpp`. /fp:precise. Its shared records: `records.h` (SiteType, Role, Bldg, Site, Spawn, Den, Map bits), `map.h` (`Map`, the `bldg*` helpers), `rooms.h` (interior rooms); it never includes `rpg/sim` |
| `rpg/art/` | painters by family (`art_props.cpp` with `propW`/`propH`/`propSprite`, `art_building.cpp`, `art_monsters.cpp`, `art_human.cpp` rig 1 ...) |
| `rpg/art/doll/` | the paper doll (rig 2): `doll.h`, compose (`doll_compose.cpp`: `composeFrame` runs `FramePaint` then the post passes in the `kPostPasses` list; `doll_paint.cpp` the paint helpers, `doll_passes.cpp` the post passes and pixel buffers, `doll_compose_internal.h` their private header), resolve, ramps, alloys; `rpg/art/doll/maps/` and `rpg/art/doll/clips/` are art data; `rpg/art/doll/gen/` is generated by `tools/doll/pxc.mjs` |
| `rpg/art/rig/` | rig 1, the opt-in fallback (`--rig=1`); kept as is until its retirement is decided |
| `rpg/sim/` | the simulation: class `Game` and the lanes (table below) |
| `rpg/story/` | the story engine: `story.cpp` (the engine: instances, stages, offers), `story_effects.cpp` (the conditions and effects: `evalCond`, `runEffect`), `story_text.cpp` (what story people say: `talksTo`, dialogue, choices), `story_save.cpp` (the story block of the save, `Engine::serialize`/`deserialize`); the game hooks: `story_game.cpp` (talk, kills, places, news), `story_actors.cpp` (spawning and dressing story people, `syncPersons`, `engineSync`), `story_ai.cpp` (how they move: walkers, droves, `folkStep`/`hostileStep`), `story_combat.cpp` (bouts, `boutBlow`, `untouchable`), `story_debug.cpp` (test hooks `syncNow`/`debugBout`/`debugEscort`); `story_api.h` (what other lanes may use: slot ranges, the `MK_*` tags, lookups), `story_detail.h` (the split files' private seam), `story_internal.h` (lane-wide helpers); `dsl.cpp`, `compose.cpp`, `saga.h`; and its content folders (`rpg/story/arch/`, `rpg/story/campaigns/`, `rpg/story/jobs/`, `rpg/story/req/`, `rpg/story/tales2/`, `rpg/story/lib_*.cpp`) |
| `rpg/view/` | class `View`: drawing, HUD, input, menus (table below) |
| `rpg/view/scripts/` | every test and staging script command, one file per subject (`war.cpp`, `party_view.cpp`, `urban_stage.cpp` ...); `script_util.h` holds their shared helpers |

### `rpg/sim/`: Game and the lanes

| File | What it holds |
|---|---|
| `rpg/sim/game_types.h` | **the plain types Game is made of**: Mode, Input, Actor, Projectile, Pickup, Event, Quest and `QType`, the dialogue (`DlgOpt`, **the `DLG_*` option ranges**, Dialogue), Lodging, Travel, Shop. It includes no lane header (`magic_status.h` holds the statuses `Actor` needs), so code that needs only the types includes this, not `game.h` |
| `rpg/sim/game.h` | class `Game`: its data, the members view, main, scripts and tests call, and the friend list of the lanes' Ops and Hooks structs |
| `rpg/sim/sim_schedule.cpp` | `Game::update`: **the update order, the determinism contract** (one function, one comment per step; never reorder) |
| `rpg/sim/game.cpp` | session, world and movement; `StreamOps` (the endless window's upkeep, NPC level of detail) and `ClimbOps` |
| `rpg/sim/game_internal.h` | sim-private shared pieces (includes the seams below) |
| `rpg/sim/mark_tags.h` | **the saved mark tags**: every tag by name, owner and value (`kMarkTags`), the lanes' ranges, `markKey` |
| `rpg/sim/world.h` | `World`, the Active Window onto the endless world, and the cave / ruin / interior generators |
| `rpg/sim/monster_stats.h`, `rpg/sim/sim_text.h`, `rpg/sim/looks.h` | public seams: monster stats and names; text helpers for scripts and tests; census dress and looks |
| `rpg/sim/game_save.cpp` | **the save format**: `SAVE_VER`, serialize/deserialize byte for byte, with every save block listed in write order at its top |
| `rpg/sim/game_items.cpp` | inventory, loot drops, chests (`LootOps::chestSource`) |
| `rpg/sim/game_quests.cpp` | the quest core: targets, progress, turn-in |
| `rpg/sim/quest_ops.h` | `QuestOps`: the quests' machinery (offer builders, the quest types' ticks, markers, escorts, the main quest's stages), defined in `quests.cpp`, `quest_offers.cpp` and `game_quests.cpp`; Game keeps what view, scripts and the story engine call (`offerFor`, `acceptQuest`, `hasOffer` ...) |
| `rpg/sim/quest_offers.cpp` | `offerFor`: the offer dispatcher, one builder per offer kind, and `offerPitch` |
| `rpg/sim/game_combat.cpp` | combat: `meleeHit`, **`damage` (its modifier chain, the stages numbered in order)**, `kill`, XP and level-up, arrows, `castSpell`, projectiles, pickups |
| `rpg/sim/ai_ops.h`, `rpg/sim/ai.cpp` | `AiOps`: the actors' behaviour (friendly folk, guards, town defence, the hostiles' choice of target in `ai.cpp`) |
| `rpg/sim/ai_beasts.cpp` | `AiOps::updateAI`'s per-species steps (`beastStep` keeps the windup and strike inline: /fp:fast rounding) |
| `rpg/sim/game_dialogue.cpp` | greetings, talk, `dialogueChoose` (it asks `gsim::dlgRoute` which lane takes an option) |
| `rpg/sim/dlg_routes.cpp` | **the dialogue routing**: `kDlgRoutes`, one `{lo, hi, choose}` row per `DLG_*` range in the old check order (story first, the lord last), with a static_assert that the ranges are sorted and do not overlap |
| `rpg/sim/game_shop.cpp` | shop stock, buying, selling, prices |
| `rpg/sim/game_rpg.cpp` | interaction, rest and the inn; `game_rpg_detail.h` is the private header of these six files |
| `rpg/sim/life.h` / `rpg/sim/life.cpp`, `rpg/sim/life_game.cpp`, `rpg/sim/life_hooks.h`, `rpg/sim/life_ops.h` | townsfolk: model; `LifeHooks` (the hooks the core calls: `LifeHooks::step` ..., defined in `life_game.cpp`, `life_raids.cpp`, `life_quests.cpp`); and `LifeOps` (the lane's internals) |
| `rpg/sim/life_spots.cpp`, `rpg/sim/life_paths.cpp`, `rpg/sim/life_spawn.cpp`, `rpg/sim/life_street.cpp`, `rpg/sim/life_indoors.cpp`, `rpg/sim/life_critters.cpp`, `rpg/sim/life_incidents.cpp`, `rpg/sim/life_lines.cpp` | `LifeOps` by cluster: outdoor spots, **paths (A\* for town folk)**, spawning residents, street life, interiors, village animals, overheard talk and incidents, lines |
| `rpg/sim/interior_v4.cpp` | building interiors (its own FP group: stays in `rpg/sim`) |
| `rpg/sim/magic*.cpp` (`magic_launch.cpp`: `MagicOps::launch`, one function per spell form group), `skills*`, `party*`, `home*`, `foes*`, `craft*`, `war*`, `sea*`, `lord*`, `service*`, `realm*`, `gear*` | the other lanes, each as `<lane>.h/.cpp` + `<lane>_game.cpp` + `<lane>_ops.h` (FOES and NUMBERS/FORGE keep their core hooks apart in `foe_hooks.h` / `craft_hooks.h`, like `life_hooks.h`) |
| `rpg/sim/realm_ops.h`, `war_ops.h`, `story_ops.h`, `travel_ops.h`, `wayside_ops.h`, `stream_ops.h` | the hooks that were Game members, one Ops struct per lane: `RealmOps::step` (`realm_game.cpp`), `WarOps` (`war_game.cpp`), `StoryOps` (`rpg/story/story_game.cpp`), `TravelOps` (`travel.cpp`, `prep_interior.cpp`), `WaysideOps` (`wayside.cpp`), `StreamOps` (`game.cpp`) |
| `rpg/sim/*_ops.h` | each Ops header starts its struct with an `API (SOLID 5.6)` comment: the members tests, scripts and the view call by name (stable: keep their names and meaning) and the rest (internal to the lane) |
| `rpg/sim/gear.cpp`, `rpg/sim/items.cpp` | items and gear stats: the loot /fp:precise group (`RPG_LOOT_FP` in CMake) |

### `rpg/view/`: View

| File | What it holds |
|---|---|
| `rpg/view/view.h` | class `View`: a facade over its parts (value members, below); it keeps its public API, and the `EMB_VIEW_RESET` registry |
| `rpg/view/terrain_bake.h/.cpp` | `TerrainBaker` (`View::bake_`): the chunk cache, the job queue, the background bakers, prefetch, `pumpBake`, `settleBakes` (the bake itself stays in `terrain.cpp`) |
| `rpg/view/sprite_cache.h/.cpp` | `SpriteCache` (`View::sprites_`): every texture cache (people, buildings, walls, forts, heraldry, kingdoms, styled props), `trimCaches`, `peopleMB` |
| `rpg/view/camera.h/.cpp` | `Camera` (`View::camera_`): `cam_`, zoom, `toUi`/`toWorld`, `uiInsetsToWorld` (the projection's float bodies stay in `render.cpp`) |
| `rpg/view/fx_state.h` | `FxState` (`View::fx_`): particles, floating lines, toasts, shake (header only) |
| `rpg/view/view_probe.h` | `ViewProbe`: the test and Witness hooks; included only by `rpg/view/scripts/` and `witness_state.cpp` (`ViewProbe::m4Stats(c.view)`) |
| `party_ui.h`, `loot_view.h`, `realm_hud.h`, `life_view.h`, `home_view.h`, `lens_state.h`, `weather_view.h`, `hud_extents.h`, `power_view.h`, `lane_entry.h` | the lanes' view parts (`PartyUi`, `LootView`, `RealmHud` ...), each a value member of View; `lane_entry.h` holds the magic, skills, sea, relief and rise entry points over their opaque states; `loot_card.h` is all of the rarity painter that `view.h` needs (`loot_paint.h` is included by the loot files only) |
| `rpg/view/render.cpp` | `View::drawWorld` (the world frame, in named passes) and `View::update` (`updateCamera` keeps the camera snap and hold) |
| `rpg/view/render_sorted.cpp` | the drawable list: the push order, the one sort and the sorted draw (`drawPropSorted` ...; `DrawKind`), over a `FrameCtx`; `render_internal.h` is the private header of these two files |
| `rpg/view/terrain.cpp` | ground tiles and their bake (`groundPixel`, relief levels and cliff faces), kerbs (`urbKerbCheck`) |
| `rpg/view/render_relief.cpp` | the relief that moves or stands up (falls), drawn live over the baked terrain |
| `rpg/view/hud.cpp` | `drawHud`, a list of named pieces over `View::HudFrame`: **the HUD vitals** (`View::hudVitals`: health, magicka and stamina bars), the clock, quests, notices, buffs, menus and pages |
| `rpg/view/input.cpp` | **all input**: held keys (`View::input`), menu keys, taps and the SDL event pump (`View::event`) |
| `rpg/view/hud_layout.h` | the layout helpers shared by drawing and tapping, so every touch rect has one source |
| `rpg/view/dialogue_ui.cpp`, `rpg/view/shop_ui.cpp`, `rpg/view/screens_ui.cpp` | the dialogue box; the shop; level-up, title and death screens |
| `rpg/view/worldmap.cpp` / `worldmap_markers.cpp` | the world map: `drawWorldMap` over a `wmap::MapFrame`, tile painting (`paintMapTile`, `tileInk`); its markers (war, places, people) in `worldmap_markers.cpp`; `worldmap_internal.h` is their private header |
| `party_hud.cpp`, `paperdoll.cpp`, `creator.cpp`, `skills_ui.cpp` ... | other screens and panels |
| `rpg/view/witness_state.cpp` | the Witness state snapshot (positions, entities, screen) |
| `rpg/view/script_api.h` | `EMB_SCRIPT_CMD`: how a script command registers itself |

### `tools/`

| Path | What it holds |
|---|---|
| `tools/tests/` | `rpg_test`: one `test_<subject>.cpp` per lane, each command registered with `RPG_TEST_CMD` (`tools/tests/tests.h`); the doll tests share `doll_test_common.h`/`doll_test_common.cpp` (looks lines, cross-file entry points) and register their gates with `RPG_DOLL_GATE(order, name, fn)`, run in key order by `rpg_test --doll --gates` |
| `tools/gates/` | the gate harness: `step_gates.sh`, `wave_gates.sh`, `witness_pack.mjs`, `move_check.sh`, `check_layers.sh` (+ `layers_allow.txt`), `size_report.sh`, baselines |
| `tools/preview/` | the gallery exes (one PNG sheet per art family) |
| `tools/doll/` | doll tooling: `pxc.mjs` (makes `rpg/art/doll/gen/`), looks lists, the freeze manifests |
| `tools/scripts/` | the game scripts (`*.txt`): timed commands that drive the game and take shots |
| `tools/art_hash.cpp`, `tools/save_test.cpp` | art identity hash and the save round-trip check (gate tools: not changed during the refactor) |
| `tools/slot.sh` | the machine-wide cap on heavy processes; run builds and tests through it |
| `tests/fixtures/` | golden saves and art records: baselines, never edited |
| `witness/` | the Witness tests and their reference shots |
| `web/` | the web shell (`web/index.html`) |

## Where new code goes

The full rules are in `docs/ARCHITECTURE.md`. In short:
- new gameplay goes in its lane (`<lane>.h/.cpp`, `<lane>_game.cpp`, `<lane>_ops.h`), never as new members on Game or View
  (a new view state is a part: its own header and one value member of View);
- a test script command goes in `rpg/view/scripts/<subject>.cpp`, an rpg_test command in `tools/tests/test_<subject>.cpp`;
- a sound is one `X(Name, trim)` row appended to `SFX_LIST` in `engine/sound_ids.h` plus its voice `Audio::fx<Name>` in
  `engine/audio/sfx.cpp`; a saved mark is one row in
  `kMarkTags` (`rpg/sim/mark_tags.h`) inside its lane's range, and its number is never reused;
- include the narrowest header: the seam headers and who may include each are in `docs/ARCHITECTURE.md` section 1.1;
- keep a logic file under about 2,000 lines and a function under about 300; every new file starts with 2-4 lines
  saying what it holds.
