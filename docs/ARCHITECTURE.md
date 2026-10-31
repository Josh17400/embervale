# Embervale architecture

How the code is layered, the patterns it repeats, the rules for new code, and a cookbook for the common additions.
Written for the tree after wave 1 of `docs/SOLID_PLAN.md`; each later wave's docs step updates it. The folder-by-folder
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
                      -> engine leaf headers (mathx.h, color.h, music_style.h)
sim   -> header-only art and doll data (art enums, doll.h, doll_forge.h, doll_envelope.h); never painter functions
art   -> culture/build blueprints, doll, world leaf headers (biomes.h, coords.h, ids.h)
tools/tests -> any public header; internals only through the seams listed in tools/gates/layers_allow.txt
```

- `tools/gates/check_layers.sh` checks the arrows. Today's exceptions are listed in `tools/gates/layers_allow.txt`;
  that list may only shrink.
- Wave 2 adds the narrow seam headers (`rpg/core/`, `engine/canvas.h`, `engine/sound_ids.h`, `story_api.h` ...). Until
  then the sim includes `engine/audio.h` for the sound ids and painters include `engine/pix.h` for `Canvas`.

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
| story archetypes | `rpg/story/arch/index.cpp` | append in library order (the order is an id: never reorder) |

Tables that are still hand-kept lists (wave 5 turns some into one-row tables):
- sounds: the `Sfx` enum (`engine/audio.h`), its name in `Audio::sfxName` and its recipe (`case Sfx::...`) in `engine/audio.cpp`;
- props: the `Prop` enum (`rpg/art/art_props.h`), the size (`propW`/`propH`/`propFrames` in `rpg/art/art_props.cpp`,
  through `kPropInfo` or the family's own `*PropW`), the sprite (`propSprite`) and how the renderer treats it
  (`rpg/view/prop_traits.cpp`);
- saved mark tags: `enum class Mk` in `rpg/sim/game_internal.h`;
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
5. **Shared words** (enums and plain structs used by several layers) belong in one header-only place that includes
   nothing above it (`rpg/core/` from wave 2).
6. **Include the narrowest header.** No file includes another folder's `*_internal.h`, except tests through the seams
   in `layers_allow.txt`. Private seams (`game_rpg_detail.h`, `life_ops.h`, `build_detail.h`, `hud_layout.h`,
   `rpg/view/scripts/script_util.h`) are included only by the files their headers name.
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
1. Append an id to `enum class Sfx` in `engine/audio.h` (before `COUNT`; never in the middle).
2. Give it a name in `Audio::sfxName` and a recipe (`case Sfx::<Name>:`) in `engine/audio.cpp`.
3. Play it from the sim with an event or from the view with `Audio::play(Sfx::<Name>)` on the View's `Audio`.
4. Check: `audio_preview` renders it; `tools/gates/audio_pack.sh` shows only the new WAV as new.

**A prop family.**
1. Append the `Prop` ids in `rpg/art/art_props.h` (append only), with an `is<Family>Prop` range test.
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

**A saved mark.**
1. Append one line to `enum class Mk` in `rpg/sim/game_internal.h` with the next unused number and a comment saying
   what the key and value mean. Never renumber or reuse a value.
2. Read and write it through `Game::marks` with a key mixed from a stable id (site Gid, npcKey, quest id) and the tag.
   Marks are already saved, so no save change and no `SAVE_VER` bump is needed.
3. Check: `save_test` ALL OK and `rpg_test --save-soak 100`.

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
