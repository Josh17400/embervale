# Embervale architecture

How the code is layered, the patterns it repeats, the rules for new code, and a cookbook for the common additions.
Written for the tree after wave 2 of `docs/SOLID_PLAN.md`; each later wave's docs step updates it. The folder-by-folder
map is `README.md`.

## 1. Layers

An arrow means "may include". Nothing includes upward.

```
app (rpg/main.cpp)
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
| `engine/sound_ids.h` | `Sfx`, `Music`, `Sting` (append only: saved and named by tools) | anyone that names a sound; only code that plays sounds needs `engine/audio.h` |
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

A new sim file uses `rpg/core/art_ids.h`; one that needs another painter header adds a row here in the same change.

## 2. The lane pattern

A lane is one game system (townsfolk, magic, the sea, fellows, the homestead ...). It keeps its code out of `Game` and
`View` by splitting it three ways:

| Part | Holds | Example |
|---|---|---|
| `<lane>.h` / `<lane>.cpp` | the model: rules and data, no `Game` | `rpg/sim/magic.h`, `rpg/sim/life.h` |
| `<lane>_game.cpp` | the glue: the `Game::<lane>*` hooks that Game calls | `rpg/sim/magic_game.cpp`, `rpg/sim/life_game.cpp` |
| `<lane>_ops.h` | `struct <Lane>Ops`: the lane's internals, a `friend` of `Game` (see the friend list in `game.h`) | `rpg/sim/magic_ops.h`, `rpg/sim/life_ops.h` |

A big lane splits its Ops by cluster: `LifeOps` is declared in `life_ops.h` and defined across `life_spots.cpp`,
`life_paths.cpp`, `life_spawn.cpp`, `life_street.cpp`, `life_indoors.cpp`, `life_critters.cpp`, `life_incidents.cpp`
and `life_lines.cpp`. A cluster keeps its own internal calls inside its file, and small hot helpers stay `inline` in
the header.

Game calls a lane from a few fixed places:
- the update step (`Game::update` in `rpg/sim/game.cpp`; a separate `sim_schedule.cpp` comes in wave 4);
- the dialogue dispatch (`Game::dialogueChoose` in `rpg/sim/game_dialogue.cpp`), which hands an option to the lane
  whose `DLG_*` range (`rpg/sim/game.h`) holds its action;
- the save (`rpg/sim/game_save.cpp`), when the lane keeps state.

On the view side a lane gets its own files (`rpg/view/<lane>_ui.cpp`, `render_<lane>.cpp`) and, for tests, its own
`rpg/view/scripts/<subject>.cpp`.

## 3. Registries

These tables register themselves, so adding a row needs no edit in a central file.

| Registry | Where | How a row is added |
|---|---|---|
| script commands | `rpg/view/script_api.h` | `EMB_SCRIPT_CMD("name", "help", fn);` in a `rpg/view/scripts/*.cpp` file (`expect:<what>` extends `expect`) |
| rpg_test commands | `tools/tests/tests.h` | `RPG_TEST_CMD("--flag", "help", fn);` in a `tools/tests/test_*.cpp` file |
| per-seed checks | `tools/tests/tests.h` | `RPG_SEED_CHECK("name", fn);` (run by `rpg_test --seeds`) |
| doll gates | `tools/tests/doll_test_common.h` | `RPG_DOLL_GATE(order, "name", fn);` in a `tools/tests/test_doll*.cpp` file; `rpg_test --doll --gates` runs them by order key (unique; a repeat is a FAIL) |
| story archetypes | `rpg/story/arch/index.cpp` | append in library order (the order is an id: never reorder) |

Tables that are still hand-kept lists (wave 5 turns some into one-row tables):
- sounds: the `Sfx` enum (`engine/sound_ids.h`), its name in `Audio::sfxName` and its recipe (`case Sfx::...`) in `engine/audio.cpp`;
- props: the `Prop` enum (`rpg/core/art_ids.h`, with its predicates and footprints), the size (`propW`/`propH`/`propFrames` in `rpg/art/art_props.cpp`,
  through `kPropInfo` or the family's own `*PropW`), the sprite (`propSprite`) and how the renderer treats it
  (`rpg/view/prop_traits.cpp`);
- saved mark tags: the register `kMarkTags` in `rpg/sim/mark_tags.h` (every lane's tags by name, owner and value;
  the lanes keep their named constants in their own files, and `enum class Mk` holds the SIM lane's);
- quest types: `enum class QType` in `rpg/sim/game.h`;
- wayside places: `rpg/world/vignettes.cpp`.

## 4. Determinism and floating point

Worlds, fights and frames are repeatable, and the gates check them bit for bit.
- **FP groups.** `rpg/culture` + `rpg/build`, `rpg/world`, and `rpg/sim/gear.cpp` + `rpg/sim/items.cpp` (`RPG_LOOT_FP`)
  build with `/fp:precise` (MSVC) or `-ffp-contract=off -fno-fast-math` (clang). Never move code between `rpg/sim` and
  `rpg/world`, never move `interior_v4.cpp` into `rpg/world`, and never move loot code into or out of `gear.cpp` /
  `items.cpp`: each changes bits in goldens.
- **Worldgen** uses integer maths and `rpg/world/dmath.h`.
- **RNG.** Never pass two RNG draws as arguments of one call (the evaluation order is unspecified).
- **Fixed orders.** `Game::update`'s order, `damage()`'s modifier chain, RNG draw order, the drawable push order and
  sort ties in `render.cpp`, the Bx draw order in `rpg/build`, and `rpg/story/arch/index.cpp`'s order are contracts.
- **Saves.** `SAVE_VER` is 23 and lives only in `rpg/sim/game_save.cpp`, whose header lists every block in write order.
  Enum values (Prop, Monster, Sfx, QType, Building, Mk ...) are append only.
- The GCC `no-unswitch-loops` guard in `rpg/sim/life.cpp` stays.

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
   (`tools/gates/size_report.sh`). Data tables and generated files are exempt.
8. **No new process-wide mutable globals** in sim, art or view. Per-world state lives in Game as runtime-only fields
   marked "never saved"; per-frame screen state lives in View; any file-scope state registers a Witness reset.
9. **Determinism.** Section 4: integer/dmath worldgen, one RNG draw per argument list, no code moved between FP groups,
   the fixed orders kept exactly.
10. **Generated and vendored files** are never edited by hand: `rpg/art/doll/gen/` (made by `tools/doll/pxc.mjs`),
    `engine/witness.h`, `engine/witness_emscripten.js`.
11. **Every new file starts with 2-4 lines** saying what lives in it and where its neighbours are.
12. **A new subfolder needs its CMake glob** in the same change, and a new file in rpg_doll needs the `RPG_DOLL_DATA`
    list in `CMakeLists.txt`.

## 6. How do I add X

**A sound.**
1. Append an id to `enum class Sfx` in `engine/sound_ids.h` (before `COUNT`; never in the middle).
2. Give it a name in `Audio::sfxName` and a recipe (`case Sfx::<Name>:`) in `engine/audio.cpp`.
3. Play it from the sim with an event or from the view with `Audio::play(Sfx::<Name>)` on the View's `Audio`.
4. Check: `audio_preview` renders it; `tools/gates/audio_pack.sh` shows only the new WAV as new.

**A prop family.**
1. Append the `Prop` ids in `rpg/core/art_ids.h` (append only), with an `is<Family>Prop` range test there.
2. Give the family its size and frames (`<family>PropW/H/Frames`) and its painter, and route them in `propW`, `propH`,
   `propFrames` and `propSprite` in `rpg/art/art_props.cpp` (or the family's own `art_<family>.cpp`).
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
1. Reserve a new `DLG_<LANE>` .. `DLG_<LANE>_END` option range in `rpg/sim/game.h` (after the last one).
2. Write `<lane>Talk` (adds options) and `bool <lane>Choose(const DlgOpt&)` (handles them) in `<lane>_game.cpp`.
3. Add one line to the range dispatch in `Game::dialogueChoose` (`rpg/sim/game_dialogue.cpp`) and the talk call
   beside the other lanes' calls there.
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
3. Wire it: one call in `Game::update`, one dispatch row for its dialogue range, and a save block appended at the end
   of `serialize`/`deserialize` in `game_save.cpp` (bump `SAVE_VER` and list the block in the header; old saves must
   still load, so `deserialize` reads it only when the version is new enough).
4. View: its own `rpg/view/<lane>_ui.cpp` / `render_<lane>.cpp`; test commands in `rpg/view/scripts/<lane>.cpp`;
   rpg_test lines in `tools/tests/test_<lane>.cpp`.
5. Gate it: `tools/gates/step_gates.sh --bdir <dir> --out <dir> --lines "--<lane>"`.
