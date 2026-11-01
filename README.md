# Embervale

A pixel-art RPG in C++20 on SDL3: an endless generated world of regions, towns and caves, a simulation of townsfolk,
quests, stories, magic, fellows and the sea, and a paper-doll character renderer. Windows (MSVC) first; the same code
builds with clang on Linux and with emscripten for the web build. The simulation has no SDL in it, so it runs headless
in the test runner.

This file is the code map of the tree after wave 2 of the SOLID plan. The layers, the seam headers, the rules for new code and the "how do I add X" cookbook are in
`docs/ARCHITECTURE.md`. The refactor that produced this layout is `docs/SOLID_PLAN.md`; the game's design is
`docs/VISION_PLAN.md` and `docs/PLAN.md`.

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
| `rpg/sim/game.h` | class `Game`, its types (Actor, Quest, Input, `QType`, the `DLG_*` option ranges) and every lane's hook declarations |
| `rpg/sim/game.cpp` | session, world, movement and `Game::update`: **the update order** |
| `rpg/sim/game_internal.h` | sim-private shared pieces (includes the seams below) |
| `rpg/sim/mark_tags.h` | **the saved mark tags**: every tag by name, owner and value (`kMarkTags`), the lanes' ranges, `markKey` |
| `rpg/sim/world.h` | `World`, the Active Window onto the endless world, and the cave / ruin / interior generators |
| `rpg/sim/monster_stats.h`, `rpg/sim/sim_text.h`, `rpg/sim/looks.h` | public seams: monster stats and names; text helpers for scripts and tests; census dress and looks |
| `rpg/sim/game_save.cpp` | **the save format**: `SAVE_VER`, serialize/deserialize byte for byte, with every save block listed in write order at its top |
| `rpg/sim/game_items.cpp` | inventory, loot drops, chests (`LootOps::chestSource`) |
| `rpg/sim/game_quests.cpp` | the quest core: targets, progress, turn-in |
| `rpg/sim/quest_offers.cpp` | `offerFor`: the offer dispatcher, one builder per offer kind, and `offerPitch` |
| `rpg/sim/game_combat.cpp` | combat: `meleeHit`, **`damage` (its modifier chain, the stages numbered in order)**, `kill`, XP and level-up, arrows, `castSpell`, projectiles, pickups |
| `rpg/sim/ai_beasts.cpp` | `updateAI`'s per-species steps (`beastStep` keeps the windup and strike inline: /fp:fast rounding) |
| `rpg/sim/game_dialogue.cpp` | greetings, talk, `dialogueChoose`: the option dispatch to each lane's `*Choose` hook by `DLG_*` range |
| `rpg/sim/game_shop.cpp` | shop stock, buying, selling, prices |
| `rpg/sim/game_rpg.cpp` | interaction, rest and the inn; `game_rpg_detail.h` is the private header of these six files |
| `rpg/sim/life.h` / `rpg/sim/life.cpp`, `rpg/sim/life_game.cpp`, `rpg/sim/life_ops.h` | townsfolk: model, the `Game::life*` hooks, and `LifeOps` (the lane's internals) |
| `rpg/sim/life_spots.cpp`, `rpg/sim/life_paths.cpp`, `rpg/sim/life_spawn.cpp`, `rpg/sim/life_street.cpp`, `rpg/sim/life_indoors.cpp`, `rpg/sim/life_critters.cpp`, `rpg/sim/life_incidents.cpp`, `rpg/sim/life_lines.cpp` | `LifeOps` by cluster: outdoor spots, **paths (A\* for town folk)**, spawning residents, street life, interiors, village animals, overheard talk and incidents, lines |
| `rpg/sim/interior_v4.cpp` | building interiors (its own FP group: stays in `rpg/sim`) |
| `rpg/sim/magic*.cpp` (`magic_launch.cpp`: `MagicOps::launch`, one function per spell form group), `skills*`, `party*`, `home*`, `foes*`, `craft*`, `war*`, `sea*`, `lord*`, `service*`, `realm*`, `gear*` | the other lanes, each as `<lane>.h/.cpp` + `<lane>_game.cpp` + `<lane>_ops.h` |
| `rpg/sim/gear.cpp`, `rpg/sim/items.cpp` | items and gear stats: the loot /fp:precise group (`RPG_LOOT_FP` in CMake) |

### `rpg/view/`: View

| File | What it holds |
|---|---|
| `rpg/view/view.h` | class `View` |
| `rpg/view/render.cpp` | `View::drawWorld` (the world frame, in named passes) and `View::update` (`updateCamera` keeps the camera snap and hold) |
| `rpg/view/render_sorted.cpp` | the drawable list: the push order, the one sort and the sorted draw (`drawPropSorted` ...; `DrawKind`), over a `FrameCtx`; `render_internal.h` is the private header of these two files |
| `rpg/view/terrain.cpp` | ground tiles and their bake (`groundPixel`, relief levels and cliff faces), kerbs (`urbKerbCheck`) |
| `rpg/view/render_relief.cpp` | the relief that moves or stands up (falls), drawn live over the baked terrain |
| `rpg/view/hud.cpp` | `drawHud`, now a list of named pieces over `View::HudFrame`: **the HUD vitals** (the `// vitals` block: health and stamina bars), the clock, quests, notices, buffs, menus and pages |
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
- new gameplay goes in its lane (`<lane>.h/.cpp`, `<lane>_game.cpp`, `<lane>_ops.h`), never as new members on Game or View;
- a test script command goes in `rpg/view/scripts/<subject>.cpp`, an rpg_test command in `tools/tests/test_<subject>.cpp`;
- a sound is one `X(Name, trim)` row appended to `SFX_LIST` in `engine/sound_ids.h` plus its voice `Audio::fx<Name>` in
  `engine/audio/sfx.cpp`; a saved mark is one row in
  `kMarkTags` (`rpg/sim/mark_tags.h`) inside its lane's range, and its number is never reused;
- include the narrowest header: the seam headers and who may include each are in `docs/ARCHITECTURE.md` section 1.1;
- keep a logic file under about 2,000 lines and a function under about 300; every new file starts with 2-4 lines
  saying what it holds.
