# Embervale architecture

How the code is layered, the patterns it repeats, the rules for new code, and a cookbook for the common additions.
Written for the final tree of `docs/SOLID_PLAN.md` (waves 0-5); a change that moves code or adds a pattern updates it
in the same commit. The folder-by-folder map, with a "Where is ..." index, is `README.md`.

## 1. Layers

An arrow means "may include". Nothing includes upward.

```
app (rpg/main.cpp, rpg/app)
  -> view (rpg/view, rpg/view/scripts)
       -> art painters (rpg/art), engine/pix, engine/audio
       -> sim + story (rpg/sim, rpg/story)
            -> world (rpg/world)
                 -> culture / build (rpg/culture, rpg/build)
                      -> core (rpg/core: common.h, art_ids.h)
                           -> engine leaf headers (mathx.h, color.h, canvas.h, sound_ids.h, music_style.h)
sim   -> core/art_ids.h and header-only doll data (doll.h, doll_forge.h, doll_envelope.h); painter headers only where
         listed below, and never painter functions
art   -> core, culture/build blueprints, doll, world leaf headers (biomes.h, coords.h, ids.h)
tools/tests -> any public header; internals only through the seams listed in tools/gates/layers_allow.txt
```

- `tools/gates/check_layers.sh` checks the arrows it can: nothing in `rpg/world`, `rpg/culture` or `rpg/build`
  includes `rpg/sim` (0 exceptions since wave 2), the sim never includes the view, no file includes another folder's
  `*_internal.h`, and the mark mix constant is written only in `rpg/sim/mark_tags.h`. The remaining exceptions (story
  and view files that still need `game_internal.h` / `story_internal.h`, and white-box tests) are listed in
  `tools/gates/layers_allow.txt`; that list may only shrink.
- `rpg/world` names the sim only through the declarations in `rpg/world/map.h` (`propSolid`, `bldgStoreysV7`,
  `bldgHearthV7`, the open-front helpers, the name makers), whose definitions stay in `rpg/sim` because they belong to
  its FP group (section 4). It never includes a `rpg/sim` header.

### 1.1 The seam headers (wave 2)

Narrow headers so a file can name a thing without pulling in the code behind it. Each says at its top who may include
it; include the seam, not the wide header beside it.

| Header | Holds | Who may include it |
|---|---|---|
| `engine/sound_ids.h` | `SFX_LIST` (the one list of sound effects, which generates `Sfx`), `Music`, `Sting` (append only: saved and named by tools) | anyone that names a sound; only code that plays sounds needs `engine/audio.h` |
| `engine/canvas.h` | `rgba()`, `Canvas` | painters, culture/build, core, tools; code that bakes or blits textures needs `engine/pix.h` |
| `rpg/core/common.h` | TILE, SIM_DT, hashes, value noise, IRect, BinW/BinR (`rpg/sim/common.h` forwards here) | anyone |
| `rpg/core/art_ids.h` | the art vocabulary: `Prop` with its predicates, footprints and stall geometry; `Monster`, `Species`, `Building` + `BuildingFacts`, `Posture`, the Fx ids, `HumanLook`, `MonsterLook` | anyone |
| `rpg/world/records.h` | the plain world records: `MapKind`, `SiteType`, `Role`, `Bldg`, `Site`, `Kingdom`, `Spawn`, `Den`, `WORLDGEN_*`, `MapBits`, plus `siteTypeName` and `bldgRiseTiles` | world and up |
| `rpg/world/map.h` | `Map` (the tile map), the `bldg*` helpers (`bldgArch`, `bldgRequest`, `bldgBlueprint`, open fronts, the V7 storey and hearth rules), `propSolid`, the name makers | world and up |
| `rpg/world/rooms.h` | interior rooms and storeys: `Stairs`, `RoomKind`, `RoomInfo`, the trial layouts (the geometry contract) | world and up |
| `rpg/sim/world.h` | `World` (the Active Window onto the endless world), `genCave` / `genRuin` / `genInterior`, the record size asserts | sim, story, view, tools; never world |
| `rpg/sim/mark_tags.h` | **the saved-tag registry**: `enum class Mk`, `kMarkRanges` (each lane's range), `kMarkTags` (every saved tag by name, owner and value), `markKey`, and the static_asserts that keep the values unique | sim, story, tools/tests (the view gets it through `game_internal.h`) |
| `rpg/sim/monster_stats.h` | `mstat`, `heavyWindup`, `monsterName` | sim, story, view, tools |
| `rpg/sim/sim_text.h` | text helpers scripts and tests share (`dirWord`, `giverTown`, the titles ...) | sim, story, view/scripts, tools/tests |
| `rpg/sim/looks.h` | `census::Rank`, `census::dress`, `census::ageLook`, `appearanceLook` | sim, view (creator, paperdoll, scripts), tools |
| `rpg/story/story_api.h` | what other lanes use of the story: `isStoryPerson`, `isDroveBeast`, the person/drove/herald slot ranges, the `MK_*` names, `kingdomName`, `siteByIdLoad`, `scriptById` ... | any lane; never includes a `*_internal.h` |
| `doll::clipsOf` (`rpg/art/doll/doll.h`) | a body key's clips without `doll_internal.h` | sim (`anim.cpp`), tools |

### 1.2 Painter headers the sim may include

The sim names art vocabulary through `rpg/core/art_ids.h`. These are the only painter headers it includes, each for an
enum or a plain struct that still lives beside its painter (never to call a painter):

| Sim file | Painter header | For |
|---|---|---|
| `rpg/sim/game.h` | `rpg/art/art_life.h` | `art::Bubble` (`Actor::bubble`) |
| `rpg/sim/home.h` | `rpg/art/art_home.h`, `rpg/art/art_life.h` | `art::Crop`, `art::FarmObj`, `art::Critter` |
| `rpg/sim/craft.h`, `rpg/sim/items.h` | `rpg/art/art_items.h` | `art::Icon` |
| `rpg/sim/deco.h` | `rpg/art/art_props.h` | `art::RoomStyle`, `art::FloorStyle` |
| `rpg/sim/party.h`, `rpg/sim/looks.h` | `rpg/art/art_human.h` | the human look helpers |
| `rpg/sim/monster_stats.h` | `rpg/art/art_monsters.h` | the monster vocabulary |
| `rpg/sim/anim.h`, `rpg/sim/life_ops.h`, `rpg/sim/gear_look.cpp`, `rpg/sim/looks.cpp` | `rpg/art/art_rig.h` (+ `art_fx.h` in `anim.h`, `art_rise.h` in `looks.cpp`) | `art::rigVersion`, clip and fx ids |
| `rpg/sim/party_graves.cpp` | `rpg/art/art_party.h` | `art::GraveLook`, `art::HoundLook` (forward-declared in `game.h`) |
| `rpg/sim/foes.h`, `rpg/sim/gear_ladder.h`, `rpg/sim/gear_look.h`; `rpg/story/dsl.cpp` | `rpg/art.h` (all painters) | not yet narrowed: the next trim's candidates |
| `rpg/sim/anim.cpp` | `rpg/art/doll/doll.h` | the clip and gait tables: `doll::clipsOf`, `doll::walkTable`, `doll::WalkTable`, `doll::BodySet`, `doll::Gait`, `doll::ClipInfo`, `doll::kGaitSwitch`, `doll::kClipNoPlant` |
| `rpg/sim/gear_ladder.cpp` | `rpg/art/doll/doll.h`, `rpg/art/doll/doll_envelope.h`, `rpg/art/doll/doll_forge.h` | `doll::alloyRamp`, `doll::alloyFamilyAt`, `doll::setLookSpecHook` (doll.h); `doll::fixOn` and the `FIX_*` rule bits (doll_envelope.h); the `doll::forge` axis vocabulary (doll_forge.h) |
| `rpg/sim/gear_forge.cpp` | `rpg/art/doll/doll_forge.h` | the `doll::forge` axis vocabulary |
| `rpg/sim/gear_look.cpp` | `rpg/art/doll/doll.h` | `doll::hairVariants` |
| `rpg/sim/looks.cpp` | `rpg/art/doll/doll_envelope.h`, `rpg/art/doll/doll_forge.h` | `doll::fixOn(FIX_KING)` (doll_envelope.h); the `doll::forge` axis vocabulary (doll_forge.h) |

The doll rows are table and look queries (clip lists, walk tables, alloy ramps, hair counts, the forge axes and fix-rule
bits); none of them paints.

A new sim file uses `rpg/core/art_ids.h`; one that needs another painter header adds a row here in the same change.

## 2. The lane pattern

A lane is one game system (townsfolk, magic, the sea, fellows, the homestead ...). It keeps its code out of `Game` and
`View` by splitting it three ways:

| Part | Holds | Example |
|---|---|---|
| `<lane>.h` / `<lane>.cpp` | the model: rules and data, no `Game` | `rpg/sim/magic.h`, `rpg/sim/life.h` |
| `<lane>_game.cpp` | the glue: the bodies of the lane's hooks that the core calls (`LifeHooks::step`, `MagicOps::choose` ...) | `rpg/sim/magic_game.cpp`, `rpg/sim/life_game.cpp` |
| `<lane>_ops.h` | `struct <Lane>Ops`: the lane's hooks and internals as static functions taking `Game&`, a `friend` of `Game` (see the friend list in `game.h`) | `rpg/sim/magic_ops.h`, `rpg/sim/war_ops.h` |

Since wave 4 a hook called only from inside `rpg/sim` and `rpg/story` is not a Game member: it lives in its lane's
struct (`Game::lifeStep` became `LifeHooks::step(Game&, float)`). A hook that view, main or the gate tools still call
stays a Game member, so those layers did not change. Where each lane's hooks live:

| Struct | Header | Bodies | What it holds |
|---|---|---|---|
| `LifeHooks` | `life_hooks.h` | `life_game.cpp` and its cluster files, `life_raids.cpp`, `life_quests.cpp` | townsfolk and citizens: step, talk, choose, raids |
| `FoeHooks` | `foe_hooks.h` | `foes_game.cpp` | elites, champions, uniques, boss phases, world bosses, culture arms |
| `CraftHooks` | `craft_hooks.h` | `craft_game.cpp` | crafting's talk and choose |
| `HomeOps`, `PartyOps`, `PartyTalkOps`, `LootOps`, `PowerOps`, `ClimbOps` | their lane headers (`ClimbOps` in `game.h`) | their lane files | the existing Ops structs, now holding the hooks too |
| `RealmOps` | `realm_ops.h` | `realm_game.cpp` | `RealmOps::step`, the living world's step |
| `WarOps` | `war_ops.h` | `war_game.cpp` | siege camps, patrols, refugees, the war's talk, choose and kills |
| `TravelOps` | `travel_ops.h` | `travel.cpp`, `prep_interior.cpp` | travel behind the fade, the interior made ahead |
| `WaysideOps` | `wayside_ops.h` | `wayside.cpp` | vignettes, ambushes, tolls, stones, graves, rumours |
| `QuestOps` | `quest_ops.h` | `quests.cpp`, `quest_offers.cpp`, `game_quests.cpp` | offer builders, quest ticks, markers, escorts, the main quest |
| `AiOps` | `ai_ops.h` | `ai.cpp`, `ai_beasts.cpp` | beasts by species, friendly folk, town defence, target choice |
| `StreamOps` | `stream_ops.h` | `game.cpp` | the endless window's upkeep, NPC level of detail, dens, roaming spawns |
| `StoryOps` | `story_ops.h` | `rpg/story/story_game.cpp` | story quests, rumours and news, lore, boards, heralds |

`LifeHooks`, `FoeHooks` and `CraftHooks` are kept apart from `LifeOps`, `FoeOps` and `CraftOps` because those headers
carry file-local helpers that clash in other files: the core includes the small hooks header and nothing else of the
lane. `JobOps` absorbed `QuestMixOps`.

**Header split.** `rpg/sim/game_types.h` holds the plain types (Mode, Input, Actor, Projectile, Pickup, Event, Quest,
`QType`, `DlgOpt`, the `DLG_*` ranges, Dialogue, Lodging, Travel, Shop) and includes no lane header (the magic statuses
that `Actor` needs are in `magic_status.h`). `rpg/sim/game.h` holds class `Game` and includes `game_types.h`. Include
`game_types.h` when the types are enough.

A big lane splits its Ops by cluster: `LifeOps` is declared in `life_ops.h` and defined across `life_spots.cpp`,
`life_paths.cpp`, `life_spawn.cpp`, `life_street.cpp`, `life_indoors.cpp`, `life_critters.cpp`, `life_incidents.cpp`
and `life_lines.cpp`. A cluster keeps its own internal calls inside its file, and small hot helpers stay `inline` in
the header.

Game calls a lane from a few fixed places:
- the update step: `Game::update` in `rpg/sim/sim_schedule.cpp`, one function with one comment per step. **That file is
  the determinism contract**: the steps run in that order every update, and saves, Witness replays and the sim digest
  depend on it. Never reorder it; a new step goes in with its reason written beside it;
- the dialogue routing: `Game::dialogueChoose` (`rpg/sim/game_dialogue.cpp`) asks `gsim::dlgRoute`
  (`rpg/sim/dlg_routes.cpp`), whose table `kDlgRoutes` has one `{lo, hi, choose}` row per `DLG_*` range
  (`rpg/sim/game_types.h`) in the old check order (story first, the lord last); a static_assert keeps the ranges
  sorted and apart, so at most one lane takes an option;
- the save (`rpg/sim/game_save.cpp`), when the lane keeps state.

On the view side a lane gets its own files (`rpg/view/<lane>_ui.cpp`, `render_<lane>.cpp`) and, for tests, its own
`rpg/view/scripts/<subject>.cpp`.

### 2.1 Staged functions (wave 3)

Wave 3 broke every function of several hundred lines into named pieces called in the old order, with the shared locals
moved into one context object. The same shapes repeat; a new stage is a new piece in the list, not a new block in a
long function.

| Where | The function | Its pieces |
|---|---|---|
| `rpg/main.cpp`, `rpg/app/` | `main()` | `AppSetup::parse`, then `App` (`app.h`): the frame loop in `main.cpp`; the save store, input injection, script runner (`App::runCmd` hands off to `App::runStateCmd`), WITNESS_RECORD recorder, Witness handlers and perf lines each in their own `rpg/app` file. `main.cpp` reaches `dlgReach`/`faceVec` (in `game_internal.h`) for the script runner through two forwarders declared in `script_runner.h` |
| `rpg/world/chunkgen.cpp` | `gen::chunk()` | one `Impl` method per stage over `gen::ChunkCtx` (`gen.h`), in this order: Base, Towns, Harbours, Stamps, Roads, Levels, RiverBeds, Flats, ReliefBits, Ecotones, Boardwalks, Massifs, Peaks, Cairns, Flora (`chunk_roads.cpp`, `chunk_relief.cpp`, `chunk_flora.cpp`) |
| `rpg/world/town_*.cpp` | `Gen::markets`, `tradeYards`, `buildCompound`, `cityDress` | file-local planner classes holding `Gen& G`: `MarketPlanner`, `CompoundPlanner`, `CityDresser`; one method per stage (the old lambdas are methods) |
| `rpg/view/render.cpp`, `render_sorted.cpp` | `View::drawWorld`, `View::update` | named passes over `FrameCtx`; the drawable push, the one `std::sort` and the sorted draw by `DrawKind` (values 0..13) in `render_sorted.cpp`; `updateCamera` keeps the camera snap and hold together |
| `rpg/view/hud.cpp` | `drawHud` | pieces over `View::HudFrame` (`hudClock`, `hudQuest`, `hudNotices` ...) |
| `rpg/view/worldmap*.cpp` | `drawWorldMap`, `paintMapTile` | pieces over `wmap::MapFrame` and a tile context; the markers in `worldmap_markers.cpp` |
| `rpg/sim/game_combat.cpp`, `ai_beasts.cpp`, `quest_offers.cpp`, `magic_launch.cpp` | `damage`, `updateAI`, `updateFolk`, `offerFor`, `MagicOps::launch` | `damage`'s chain stays one function with its stages numbered; `updateAI` per species (`beastStep`); `updateFolk` as `folkMissing`/`folkGuard`/`folkShelter`/`folkStall`; one offer builder per kind; one launch per form group |
| `rpg/art/doll/doll_compose.cpp`, `doll_paint.cpp`, `doll_passes.cpp` | `composeFrame` | `FramePaint` (`anchors`, `gatherItems`, `sortItems`, `placeAnchors`, `paintBackToFront`), then the post passes in the one list `kPostPasses` |
| `engine/audio.cpp`, `engine/audio/` | `Audio::trigger` | a static member-pointer table built from `SFX_LIST`: one `Audio::fx<Name>` per sound effect (`sfx.cpp`) |

Private headers made for these splits are included only by their own files: `render_internal.h`,
`worldmap_internal.h` (`rpg/view`), `doll_compose_internal.h` (`rpg/art/doll`), `engine/audio/audio_internal.h`.

### 2.2 View parts (wave 4)

`View` (`rpg/view/view.h`) is a facade over value members, one per part. It keeps its public API (`settleBakes`,
`peopleMB` and the projection names forward to the parts). A part's method that needs the rest of the view takes
`View& v`. Float bodies stay in the files they were compiled in (/fp:fast rounding), so a part's `.cpp` can be small.

| Part | Member | Files | What it holds |
|---|---|---|---|
| `TerrainBaker` | `bake_` | `terrain_bake.h/.cpp` | the chunk cache, the job queue, the worker threads, prefetch, `pumpBake`, `settleBakes` (the bake itself stays in `terrain.cpp`) |
| `SpriteCache` | `sprites_` | `sprite_cache.h/.cpp` | every texture cache: people, buildings, walls, forts, heraldry, kingdoms, styled props; `trimCaches`, `peopleMB` |
| `Camera` | `camera_` | `camera.h/.cpp` | `cam_`, zoom, `toUi`/`toWorld`, `uiInsetsToWorld` (`camera.cpp` holds only the zoom step) |
| `FxState` | `fx_` | `fx_state.h` | particles, floating lines, toasts, shake |
| `ViewProbe` | (static hooks) | `view_probe.h` | the test and Witness hooks and counters; included only by `rpg/view/scripts/` and `witness_state.cpp` (`ViewProbe::m4Stats(c.view)`) |
| lane parts | `party_`, `loot_`, `realm_`, `life_`, `home_`, `lens_`, `weather_`, `hudx_`, `powers_` | `party_ui.h`, `loot_view.h`, `realm_hud.h`, `life_view.h`, `home_view.h`, `lens_state.h`, `weather_view.h`, `hud_extents.h`, `power_view.h` | each lane's view state and its methods |
| opaque lanes | `MagicFxState` ... | `lane_entry.h` | the entry points of the magic, skills, sea, relief and rise lanes, whose state is an opaque struct defined in their own `.cpp` |

`view.h` includes `loot_card.h` (what an item card showed), not the rarity painter `loot_paint.h`. Four bodies over 300
lines (`homeCollect`, `drawBuild`, `drawPartyPage`, `drawPowerFx`) stay View members, because the size gate tracks a
function by its full name.

### 2.3 Stable and internal Ops members (wave 5)

Each `rpg/sim/*_ops.h` opens its struct with an `API (SOLID 5.6)` comment that splits the members in two:
- **stable test/script API**: the members called by name from outside `rpg/sim` and `rpg/story` (`tools/tests`,
  `tools/save_test.cpp`, the gallery tools, `rpg/view/scripts`, `rpg/app`, and the view files it names). Keep their
  names, signatures and meaning; a change breaks rpg_test lines, script commands or Witness runs;
- **internal**: every other member. Only the lane and the core call it, and it may change with the lane.

Ops headers whose block says "no stable test/script API" (`AiOps`, `RealmOps`, `StoryOps`, `StreamOps`, `TravelOps`,
`WaysideOps`) are reached from outside the sim only through Game's public members. A member that a test, a script or
the view starts to call joins its header's stable list in the same change.

## 3. Registries

These tables register themselves, so adding a row needs no edit in a central file.

| Registry | Where | How a row is added |
|---|---|---|
| script commands | `rpg/view/script_api.h` | `EMB_SCRIPT_CMD("name", "help", fn);` in a `rpg/view/scripts/*.cpp` file (`expect:<what>` extends `expect`) |
| rpg_test commands | `tools/tests/tests.h` | `RPG_TEST_CMD("--flag", "help", fn);` in a `tools/tests/test_*.cpp` file |
| per-seed checks | `tools/tests/tests.h` | `RPG_SEED_CHECK("name", fn);` (run by `rpg_test --seeds`) |
| doll gates | `tools/tests/doll_test_common.h` | `RPG_DOLL_GATE(order, "name", fn);` in a `tools/tests/test_doll*.cpp` file; `rpg_test --doll --gates` runs them by order key (unique; a repeat is a FAIL) |
| story archetypes | `rpg/story/arch/index.cpp` | append in library order (the order is an id: never reorder) |
| Witness view resets | `rpg/view/view.h` | file-scope presentation state (function statics, memos by actor id) registers its reset beside that state with `EMB_VIEW_RESET(fn);`; `View::witnessRelease` runs every registered reset (in no set order: each touches only its own file's state), so none can be forgotten |
| dialogue routes | `rpg/sim/dlg_routes.cpp` | one `{lo, hi, choose}` row in `kDlgRoutes` for a new `DLG_*` range (the static_assert checks the order) |

Tables that are hand-kept lists (one row per kind, but no self-registering macro):
- sounds: one `X(Name, trim)` row in `SFX_LIST` (`engine/sound_ids.h`) generates the `Sfx` id, its name in `Audio::sfxName`,
  its trim and its slot in the voice table; the voice itself is `Audio::fx<Name>` in `engine/audio/sfx.cpp`;
- props: the `Prop` enum (`rpg/core/art_ids.h`, with its predicates and footprints), the size (`propW`/`propH`/`propFrames` in `rpg/art/art_props.cpp`,
  through the family's `PropFamily` row in `kPropFamilies`, `rpg/art/art_prop_family.h`, or else `kPropInfo`), the sprite (`propSprite`) and how the renderer treats it
  (`rpg/view/prop_traits.cpp`);
- saved mark tags: the register `kMarkTags` in `rpg/sim/mark_tags.h` (every lane's tags by name, owner and value;
  the lanes keep their named constants in their own files, and `enum class Mk` holds the SIM lane's);
- monsters: `kMonsterInfo` in `rpg/art/art_monsters.cpp` (cell size and painter, one row per `Monster`, static_assert);
- quest types: `enum class QType` in `rpg/sim/game_types.h`, with one `kQuestTraits` row each in `rpg/sim/quest_traits.h`
  and one `kQuestLines` row each in `rpg/sim/quests.cpp` (both static_assert their row count);
- wayside places: one `VignetteDef` row per `VignetteKind` (`kVignettes`) and one `WonderDef` row per `WonderKind`
  (`kWonders`) in `rpg/world/vignettes.cpp`;
- the CI list: `kCiList` in `tools/tests/main.cpp` (`rpg_test --list-ci`; every line must be a registered gate).

## 4. Determinism and floating point

Worlds, fights and frames are repeatable, and the gates check them bit for bit.
- **FP groups.** `rpg/culture` + `rpg/build`, `rpg/world`, and `rpg/sim/gear.cpp` + `rpg/sim/items.cpp` (`RPG_LOOT_FP`)
  build with `/fp:precise` (MSVC) or `-ffp-contract=off -fno-fast-math` (clang). Never move code between `rpg/sim` and
  `rpg/world`, never move `interior_v4.cpp` into `rpg/world`, and never move loot code into or out of `gear.cpp` /
  `items.cpp`: each changes bits in goldens.
- **Worldgen** uses integer maths and `rpg/world/dmath.h`.
- **RNG.** Never pass two RNG draws as arguments of one call (the evaluation order is unspecified).
- **Fixed orders.** `Game::update`'s order (`rpg/sim/sim_schedule.cpp`), `damage()`'s modifier chain, RNG draw order, the drawable push order and
  sort ties in `render.cpp`, the Bx draw order in `rpg/build`, and `rpg/story/arch/index.cpp`'s order are contracts.
- **Saves.** `SAVE_VER` is 23 and lives only in `rpg/sim/game_save.cpp`, whose header lists every block in write order.
  Enum values (Prop, Monster, Sfx, QType, Building, Mk ...) are append only.
- The GCC `no-unswitch-loops` guard in `rpg/sim/life.cpp` stays.
- **/fp:fast and function boundaries.** The game outside the FP groups builds with `/fp:fast` (MSVC), so moving float
  code into a new function can change its rounding even when the source is unchanged. Wave 3 kept bits identical by
  keeping `beastStep`'s windup and strike inline (`rpg/sim/ai_beasts.cpp`), keeping `Audio::add`/`rnd` `inline` in
  `engine/audio/audio_internal.h`, and building the sound voices with `#pragma inline_depth(0)` (`fxLevelUp` with
  `float_control(precise)`, `fxCheer`'s first draw through `Audio::rndCall`), all MSVC-only. After any edit to such
  code, rerun its identity gate (the sim digest; for audio `tools/gates/audio_pack.sh`, which plays effects only at
  pitch 1).

## 5. Rules for new code

1. **Find the lane.** New gameplay goes in its lane: `<lane>.h/.cpp` for rules and data with no Game, `<lane>_game.cpp`
   for the glue, `<lane>_ops.h` for the hooks. Game itself gets at most one call in the update order and one row in
   the dialogue dispatch.
2. **No new members on Game or View for a lane.** Use the lane's Ops struct in the sim, or the lane's own view part
   with its own header.
3. **Test-only code never sits in product files.** Script commands go in `rpg/view/scripts/<subject>.cpp`; rpg_test
   commands go in `tools/tests/test_<subject>.cpp`.
4. **One table row per kind**: a sound, a prop family, a dialogue lane, a vignette, a quest type, a saved mark tag (its
   number is never changed or reused). Section 3 says where each table is today.
5. **Shared words** (enums and plain structs used by several layers) live in `rpg/core/` (`common.h`, `art_ids.h`),
   header-only and including nothing above the engine leaf headers. World records shared by the generator and the sim
   live in `rpg/world/records.h` and `rpg/world/map.h`.
6. **Include the narrowest header** (section 1.1): the sim includes `engine/sound_ids.h`, not `engine/audio.h`;
   painters include `engine/canvas.h`, not `engine/pix.h`; vocabulary comes from `rpg/core/art_ids.h`, not `rpg/art.h`;
   other lanes use `rpg/story/story_api.h`, `rpg/sim/sim_text.h`, `rpg/sim/monster_stats.h` or `rpg/sim/looks.h`
   rather than an `*_internal.h`. When you remove an include, give every file that relied on it an explicit one. No
   file includes another folder's `*_internal.h`, except the files listed in `layers_allow.txt`. Private seams
   (`game_rpg_detail.h`, `life_ops.h`, `build_detail.h`, `hud_layout.h`, `rpg/view/scripts/script_util.h`) are
   included only by the files their headers name.
7. **Size guardrails.** A hand-written logic file stays under about 2,000 lines and a function under about 300 (new
   code: under about 150). A file over that is split along its section banners before anything is added
   (`tools/gates/size_report.sh`; `--renames tools/gates/size_renames.txt` maps functions renamed since the baseline
   root, so only a genuinely new big function counts). Data tables and generated files are exempt.
8. **No new process-wide mutable globals** in sim, art or view. Per-world state lives in Game as runtime-only fields
   marked "never saved"; per-frame screen state lives in View or one of its parts; any file-scope state registers a
   Witness reset with `EMB_VIEW_RESET(fn)` next to it.
9. **Determinism.** Section 4: integer/dmath worldgen, one RNG draw per argument list, no code moved between FP groups,
   the fixed orders kept exactly.
10. **Generated and vendored files** are never edited by hand: `rpg/art/doll/gen/` (made by `tools/doll/pxc.mjs`),
    `engine/witness.h`, `engine/witness_emscripten.js`.
11. **Every new file starts with 2-4 lines** saying what lives in it and where its neighbours are.
12. **A new subfolder needs its CMake glob** in the same change, and a new file in rpg_doll needs the `RPG_DOLL_DATA`
    list in `CMakeLists.txt`.
13. **Tests and scripts use the stable API.** A test or script command calls Game's public members or the members an
    Ops header lists as stable (section 2.3); calling an internal member makes it stable, so add it to that list.

## 6. How do I add X

**A sound.**
1. Append one `X(<Name>, trim)` row at the end of `SFX_LIST` in `engine/sound_ids.h` (never in the middle). It makes the
   `Sfx` id, its name (`<Name>` in snake case) and its trim, and declares `Audio::fx<Name>`.
2. Write its voice `void Audio::fx<Name>(float k, float V, uint32_t arg)` in `engine/audio/sfx.cpp`.
3. Play it from the sim with an event or from the view with `Audio::play(Sfx::<Name>)` on the View's `Audio`.
4. Check: `audio_preview` renders it; `tools/gates/audio_pack.sh` shows only the new WAV as new.

**A prop family.**
1. Append the `Prop` ids in `rpg/core/art_ids.h` (append only), with an `is<Family>Prop` range test there.
2. Give the family its size and frames (`<family>PropW/H/Frames`) and its painter, and add one `PropFamily` row to
   `kPropFamilies` in `rpg/art/art_prop_family.h`; `propW`, `propH`, `propFrames`, `paintProp` and `propSprite` read it.
3. Tell the renderer how to treat it in `rpg/view/prop_traits.cpp` (flat, nature, tree, shadow, light).
4. Check: its gallery in `tools/preview/`; `art_hash` changes only in the new family's line.

**A script command.**
1. Pick or make `rpg/view/scripts/<subject>.cpp` (a new file needs its 2-4 line header; the folder glob picks it up).
2. `#include "rpg/view/script_api.h"`, write `static bool cmdX(ScriptCtx& c)` and register it with
   `EMB_SCRIPT_CMD("x", "x <args>: what it does", cmdX);`. Return false to be called again next frame.
3. Use it from a `tools/scripts/*.txt` script and run that through `tools/gates/witness_pack.mjs` (never a visible
   window).

**An rpg_test command.**
1. Add `tools/tests/test_<subject>.cpp` (the `tools/tests/*.cpp` glob picks it up) and include `tools/tests/tests.h`.
2. Register `RPG_TEST_CMD("--subject", "help", fn);` where `int fn(int argc, char** argv)` returns the exit code (0 passes); `RPG_SEED_CHECK` adds a
   per-seed check to `rpg_test --seeds`.
3. Tests never touch the player's save.

**A dialogue lane.**
1. Reserve a new `DLG_<LANE>` .. `DLG_<LANE>_END` option range in `rpg/sim/game_types.h` (after the last one).
2. Write `<Lane>Ops::talk` (adds options) and `static bool <Lane>Ops::choose(Game&, const DlgOpt&)` (handles them) in
   `<lane>_game.cpp`.
3. Add one row `{DLG_<LANE>, DLG_<LANE>_END, &<Lane>Ops::choose}` to `kDlgRoutes` (`rpg/sim/dlg_routes.cpp`) in range
   order, and the talk call beside the other lanes' calls in `rpg/sim/game_dialogue.cpp`.
4. Check: `rpg_test --dialogue --seeds 1..3`.

**A saved mark.** (`rpg/sim/mark_tags.h` is the registry of every saved tag.)
1. Pick the next unused value in your lane's range (`kMarkRanges` in `rpg/sim/mark_tags.h`). Never renumber or reuse a
   value: tags are part of the save format.
2. Name it where the lane keeps its tags (the SIM lane: `enum class Mk` in `mark_tags.h`; FOES: `foes.h` `MK_FOES_*`;
   STORY: the `MK_*` in `story_api.h`; FELLOWS: `PartyOps::MK_*` ...), with a comment saying what the key and the value
   mean.
3. Add its one row to `kMarkTags` in `mark_tags.h` (name, owner, value, share 0, what the key is). The static_asserts
   at the bottom fail the build if the value repeats one already there or falls outside the ranges.
4. Read and write it as `Game::marks[gsim::markKey(id, tag)]` with a stable id (site Gid, npcKey, quest id). Never
   restate the mix constant (`check_layers.sh` fails on it). Marks are already saved: no save change, no `SAVE_VER` bump.
5. Check: `rpg_test --marks`, `save_test` ALL OK and `rpg_test --save-soak 100`.

**A whole lane.**
1. Model: `rpg/sim/<lane>.h/.cpp`, rules and data with no `Game`.
2. Hooks: `rpg/sim/<lane>_ops.h` with `struct <Lane>Ops`, plus one `friend struct <Lane>Ops;` line in `game.h`'s
   friend list; glue in `rpg/sim/<lane>_game.cpp`.
3. Wire it: one commented step in `Game::update` (`rpg/sim/sim_schedule.cpp`), one `kDlgRoutes` row for its dialogue
   range, and a save block appended at the end
   of `serialize`/`deserialize` in `game_save.cpp` (bump `SAVE_VER` and list the block in the header; old saves must
   still load, so `deserialize` reads it only when the version is new enough).
4. View: its own `rpg/view/<lane>_ui.cpp` / `render_<lane>.cpp`; test commands in `rpg/view/scripts/<lane>.cpp`;
   rpg_test lines in `tools/tests/test_<lane>.cpp`.
5. Gate it: `tools/gates/step_gates.sh --bdir <dir> --out <dir> --lines "--<lane>"`.
