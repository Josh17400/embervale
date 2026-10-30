# EMBERVALE SOLID plan: from one big program to workable parts

Base: `119de74` (branch p2c-slice, equal to main), 2026-10-30. **This is a plan only. No code has changed.**
Every step keeps the game's behaviour: the same saves, worlds, pixels and sounds, proven by numbers after
every step. The owner asked for a SOLID review "to make sure it's not just one conglomeration of code but segmented
into workable parts ... nice and clean and easy to get to".

Contents: 1 Vision · 2 Findings · 3 In and out · 4 Gates · 5 Target layout and rules · 6 Waves and steps ·
7 Step contracts · 8 Do not touch · 9 What the judge checks · 10 Owner decisions · 11 Rig-1 retirement map

---

## 1. Vision

The game works, and its newer lanes are built cleanly. The oldest and busiest parts, though, have grown into grab bags:
- a 2,000-line `main()`;
- one class (`Game`) that every lane bolts methods onto, and one class (`View`) that owns every screen concern;
- the save format hidden in a file called `game_rpg.cpp`;
- 4,240 lines of town-folk logic in one struct.

After this plan, anyone can open `README.md` and reach the folder for a system in one step and its file in the next:
- a save question means opening `game_save.cpp`, and a town-folk bug means opening `life_street.cpp`;
- a new sound is one table row, and a new test command goes in `rpg/view/scripts/`.

Layers depend in one direction only: engine, core, culture/build, world, art, sim+story, view, app. A script fails the
check when a file breaks that order. While playing, the owner should notice no difference at all. Anyone working on the
code should notice it everywhere: smaller files, shorter functions, faster rebuilds and fewer places to edit for each
feature.

---

## 2. What the review found (spot-checked at 119de74)

### 2.1 The worst knots, highest value of fixing first

| # | Where | What is wrong | Evidence (checked) |
|---|---|---|---|
| 1 | `rpg/main.cpp` | One `main()` holds the platform loop, save file I/O, script interpreter, test helpers, input recording, synthetic input and every Witness handler | 2,306 lines; `main()` from 324; 49 `[&]` lambdas; 30 `#if` EMB_WITNESS/__EMSCRIPTEN__ blocks; `runCmd` 1223-1538 |
| 2 | `rpg/sim/game_rpg.cpp` | One file holds six jobs, including the whole save format | items 38, interact 565, quests 870, talk 1490, dialogue 1720, rest 2057, shops 2090, `SAVE_VER = 23` at 2404, `serialize` 2558, `deserialize` 2708 |
| 3 | `rpg/sim/life_game.cpp` | One `struct LifeOps` holds eight town-life subsystems | 5,260 lines; the struct runs 346-4585 |
| 4 | `rpg/sim/game.h` | God class: every lane adds hooks and state to it | 1,256 lines; over 250 member functions; 17 friend structs; 33 includes (all of `rpg/art.h` and `story.h` among them); included by 126 files |
| 5 | `rpg/view/view.h`, `render.cpp`, `hud.cpp` | God class for the screen; a 1,700-line draw function; the HUD drawing file owns all input | view.h 1,151 lines, about 300 member functions, 52 direct includers; `drawWorld` 2678-~4380 with a 14-kind magic-number switch; hud.cpp `input/menuKey/tap/event` 2755-3402 |
| 6 | Test code inside product files | Test-only script commands sit in the terrain painter file and the renderer; 52 script files are named by milestone rather than by subject | terrain.cpp 6285-6779 (5 commands and a global `g_stage`); render.cpp `weather`; about 13.6k lines in `script_*.cpp` |
| 7 | Layer inversions | World generator headers include the simulation's header, and that header includes every painter header; story and sim include each other's private headers | `world/poi.h, source.h, town_plan.h, town_rules.h -> sim/world.h -> rpg/art.h`; `story_internal.h` included by 26 files outside story; `game_internal.h` by 88 files (40 outside sim) |
| 8 | Saved data tracked only in comments | Mark tags, which are saved, are allotted by comments across 10+ files; the view retypes the key formula by hand | `Mk` 1..10, lane ranges 32..159, 176 (`req_offers.cpp:49`), 200; `render.cpp:2486/2490/2506` and `script_foes.cpp:54` copy the 0xD1B54A32D192ED03 mix. **New finding:** the view's "biome seen" mark uses tag 8 (= `Mk::StoryGiver`), but its comment names a `Mk::Biome` that does not exist. The two id spaces do not overlap, so nothing breaks today. |
| 9 | Switch ladders that grow with each feature | Each new sound, prop family, dialogue lane, vignette or quest type means editing 3-6 places | `audio.cpp` 83 `case Sfx::` plus positional name/trim tables; `art_props.cpp:5709-5717` three 10-way ternary chains; 12 `DLG_*` ranges `game.h:364-381`; 6 `switch(VignetteKind)`; `QType::` in 6+ files |
| 10 | Duplicated rules | The renderer copies the sim's heavy-slam wind-up times | `render.cpp:4177-4179` vs `gsim::heavyWindup` |
| 11 | `engine/audio.cpp` | One class does device I/O, sfx synthesis, the music composer, four ambience beds and the mixer | 2,945 lines; trigger 680-1409, sequencer 1622-2300, beds 2301-2730 |
| 12 | Other long god functions | Each new case lengthens the function | `chunk()` about 1,600 lines (`chunkgen.cpp:1167`); `Gen::markets` 939 lines of lambdas; `composeFrame` about 790 lines (`doll_compose.cpp:2027`); `offerFor` about 620; `updateAI` about 370 |
| 13 | Tests reach into internals | Tests redeclare engine structs by hand and keep dead `#include "...cpp"` fallbacks | `test_hero.cpp:24-30` copies `census::Rank`, which risks a one-definition-rule (ODR) violation; `test_folk_sim.cpp:30` `ageLook`; `test_hero.cpp:19-21`, `test_arms.cpp:26-27` |
| 14 | CI and docs | CI runs only on pushes to main and never runs the pixel gates; the README describes an empty repo | `web.yml` `on: push: branches: [main]`; no art_hash, village or freeze lines; README says "No game is in this repo right now" |

**Fact that shapes every step:** MSVC builds the whole game with `/fp:fast` (`CMakeLists.txt:8`). The exceptions are the
precise groups: rpg/culture, rpg/build, rpg/world, sim/gear.cpp and sim/items.cpp. Under `/fp:fast`, moving a function to
another file can change a floating-point result in the last bit, because inlining changes. A "pure move" in art, view,
audio or sim is therefore never assumed to be identical. The gates prove it, and a move that changes one pixel or bit is
undone (see 4.4).

### 2.2 The good parts (keep them, and spread them)

- **The lane pattern:**
  - a model file that never touches `Game` (realm*, census, craft, gear*, items, foes, magic, party, sea, service, lordship, world*, stream, interior_v4);
  - plus `<lane>_game.cpp` glue;
  - plus a `<lane>_ops.h` of static functions (MagicOps, SkillOps, SeaOps, ServiceOps, LordOps, JobOps, PowerOps).

  This is the shape the older lanes move toward.
- **Registries:**
  - `EMB_SCRIPT_CMD` (297 script commands, added without editing main.cpp);
  - `RPG_TEST_CMD` (98 test flags) and `RPG_SEED_CHECK`;
  - `rpg/build/registry.cpp` (a switch with no default over every Prop);
  - `rpg/story/arch/index.cpp` (an ordered registry).
- **Saves:** one `SAVE_VER` (23) with length-prefixed blocks per lane, guarded by `save_test` and `tests/fixtures/save_v23.bin`.
- **Library seams:**
  - `rpg_culture` and `rpg_doll` link no SDL;
  - the sim reaches sound only through `Ev::Sfx` events;
  - the engine never includes witness.h (it uses `Audio::setPlayListener`);
  - `struct Input` (game.h:50) has no device code.
- **Proof tools:**
  - `art_hash` per category at both rigs, the freeze manifest, village-art and the goldens;
  - Witness, with 15 tests;
  - 422 game scripts, of which 397 save screenshots (4,994 shot lines).
- **Big but cohesive, leave as is:** interior_v4.cpp (a 44-line header), art_home.cpp, art_market.cpp, worldmap.cpp, the
  terrain painters, every data table and the generated doll pool. Size alone is not the problem.

**Corrections to the mapper reports:**
- Game has 17 friend structs, not 16.
- The test and staging script code in rpg/view is about 13.6k lines in 52 files, not about 27k.

---

## 3. In and out

**IN:**
- moving code into smaller files along seams that already exist;
- narrow headers, so that layers depend in one direction only;
- breaking god functions into named steps;
- shrinking Game and View into parts;
- one table per kind in place of switch ladders;
- a committed gate harness;
- CI and test hygiene;
- a code map (README) and an architecture page.

**OUT** (each needs its own owner OK):
- any change to gameplay, balance, art, sound or text;
- bug fixes (found bugs go into step reports and are fixed in a separate batch);
- **save-format changes** (SAVE_VER stays 23, and the fixture stays byte-identical);
- **art compression**;
- **deleting rig 1, `rpg/art/rig/`, the rig galleries or any old art**;
- re-recording any baseline;
- renaming any script command, Witness call, state field or test flag;
- new features (key remapping, gamepad).

---

## 4. Gates: how we prove nothing broke

### 4.1 Baselines, fixed for the whole refactor

| Gate | Expected |
|---|---|
| art_hash (default, rig 2) | TOTAL **5e4a8a57bb4212f8**, every category line as recorded at wave 0 |
| art_hash `--rig=1` | TOTAL **317eef6efe4d8229**, every line as recorded |
| village art, rig 2 / rig 1 | **65 groups, 0 changed** (`golden_village_art.txt` / `golden_village_art_rig1.txt`) |
| freeze | **0 changed** against `tools/doll/freeze/p2c_freeze.txt` (default and `--rig=1`) |
| doll pool | `node tools/doll/pxc.mjs --check` up to date (bytes and hash as recorded at wave 0) |
| save | SAVE_VER 23; `save_test` ALL OK; `tests/fixtures/**` byte-identical to 119de74 |
| CI list | the 68 commands extracted from `.github/workflows/web.yml`, every one rc 0 on WSL clang (**CLANG_FAILS=0**) and on MSVC |
| identity probes (new, wave 0) | sim digest, view pixel pack, audio pack: identical to the 119de74 records |

### 4.2 Step gates: run after EVERY step, on the step's own build

`tools/gates/step_gates.sh` runs them once wave 0 has written it. The commands are:

| Id | Gate | Command (each heavy line through `tools/slot.sh`) | Pass |
|---|---|---|---|
| S1 | MSVC build | `cmd //c "set BDIR=<dir>&& tools\\build.bat" < /dev/null` (with MSYS_NO_PATHCONV=1 use `cmd /c`; check the log for ninja lines) | rc 0, every target built, warnings not above baseline |
| S2 | sim seeds | `<dir>/rpg_test --seeds 1..20` | ALL OK |
| S3 | saves | `<dir>/save_test`; `git diff --quiet 119de74 -- tests/fixtures` | ALL OK; fixture untouched |
| S4 | clang CI | `tools/gates/clang_ci.sh` (rsync to a WSL copy, incremental ninja, the 68 web.yml commands) | clang build rc 0, `CLANG_FAILS=0` |
| S5 | art identity | `<dir>/art_hash.exe` and `<dir>/art_hash.exe --rig=1` | the two TOTALs above, every line equal to baseline |
| S6 | freeze | `rpg_test --doll-wear --freeze-check tools/doll/freeze/p2c_freeze.txt`, then the same with `--rig=1`; `node tools/doll/pxc.mjs --check` | 0 changed, 0 changed; pool up to date |
| S7 | village art | `city_gallery.exe --village-art-check`; `city_gallery.exe --rig=1 --village-art-check tests/fixtures/golden_village_art_rig1.txt` | 65/0, 65/0 |
| S8 | web build | `tools/gates/build_web.bat` (emsdk pinned to 6.0.13 like CI, incremental build dir), then the IDBFS export grep from web.yml | embervale.js and embervale.wasm built; export check passes |
| S9 | Witness smoke | `node ../witness/bin/witness.js test witness/embervale.witness.json witness/determinism.witness.json witness/death_respawn.witness.json witness/touch_snapshot.witness.json`; then `... shot --at 2 --diff village_start --expect "ssim >= 0.999"` | all pass |
| S10 | identity and structure probes | `rpg_test --sim-digest --check <baseline>`; `tools/gates/view_pack.sh --quick`; `tools/gates/audio_pack.sh` (steps that touch engine/); `tools/gates/check_layers.sh`; `tools/gates/move_check.sh 119de74..HEAD <files>` | digest identical; every PNG and WAV hash identical; no new layer violation; move_check reviewed |

On top of these, each step lists **its own lines**, the rpg_test commands for the systems it touches.

### 4.3 Wave gates: once per wave, on the merged wave, from a fresh clone and a fresh build dir

- W1: all of S1-S10, plus the 68 CI commands on MSVC too.
- W2: `tools/run_scripts.sh` (all 422) with 0 failed and `v1draws rig2` 0 strays; `view_pack.sh --all` (397 shot scripts) identical.
- W3: `witness test --repeat 2` over all 15 Witness tests: pass.
- W4: the doll gate harness (`tools/gates/doll_gates.sh`, the flip's R2 list, committed): every line as at baseline.
- W5: the web first-launch probe (`tools/witness_web/firstlaunch.mjs`, one headless WebKit) reaches the title screen.
- W6: sim digest on clang too; perf lines (`rpg_test --cityperf`, `--chunkbench`, EMB_PERF frame time) within the wave-0 noise plus 3%.
- W7 (waves 3 and 4): one headless `witness play batch` (Sonnet low, 1-2 testers, about 20 game minutes) shows no new class of problem.

### 4.4 Rules

- **No baseline is re-recorded during this plan.** A gate that changes means the step is wrong:
  1. bisect the step;
  2. put the offending function back in its original file, or mark the hot helper `inline` in the private header;
  3. rerun the gates.

  A gate that is already red at 119de74 is recorded with its exact output at wave 0, and the owner is told before
  wave 1. It is then judged as "same output as baseline".
- Timing and perf lines are rerun alone before they count.
- Every heavy process goes through `tools/slot.sh` (5 at most, machine-wide). Witness runs headless with
  `WITNESS_MAX_PROCS=5` and `WITNESS_MAX_BROWSERS=1`. Run the keep-awake loop during long gate runs, because the PC
  sleeps otherwise.
- Never kill a running Embervale.exe. Never touch `%APPDATA%\Josh17400\Embervale`. Screenshots go under %TEMP% and are
  deleted after use.

### 4.5 Time arithmetic

- **Measured at the flip:** art_hash 4 s + 2 s; freeze 6 s x2; village 82 s + 81 s; doll coverage 283 s; parity 93 s.
- **Not yet measured** (wave 0 records them): MSVC incremental build, `--seeds 1..20`, the 68-line clang list, the web
  build, Witness smoke.
- **Budget:** one step's S1-S10 takes about 60-90 min of wall time, with the MSVC lines, the WSL clang list and the web
  build running side by side. The 5-slot cap lets about 2 steps gate at once.
- **Per wave:** an 8-step wave spends about 4-6 h in step gates, plus one wave-gate run of about 2-3 h (the 422 scripts
  at 3-5 at a time, Witness x2, and the doll harness at about 25 min).
- **If a step's gates exceed 2 h**, the owner may move S4 and S8 to the wave end only (decision D2).

### 4.6 Working arrangement

- **Worktrees:** one git worktree per parallel step slot (`../emb-solid-<slot>`), each with:
  - its own MSVC build dir (`BDIR=build-solid-<slot>`; build.bat already reuses `build/_deps/sdl3-src`, so no network);
  - its own WSL copy (`~/emb_solid_<slot>`, seeded from a baseline build so that ninja is incremental).

  Worktrees are reused across waves. At each wave start they fast-forward to the integrated head.
- **Commits:** one commit per step. The integrator merges the steps in id order. Ownership is disjoint, so any merge
  conflict means an ownership error and goes back to the plan.
- **Pipelining:** a step of wave N+1 may start once every wave-N step that owns any of its files has merged and passed
  its step gates. The wave-N gates still run once, on the merged wave-N head.
- **Pushing:** nothing is pushed until the wave passes the judge and the owner says go. After that the owner's usual
  ship routine applies: commit, push, watch CI, copy the exe to the Desktop.

---

## 5. Target layout and the rules for new code

`[n]` marks the wave that creates or reshapes each part.

```
engine/                 SDL3 platform pieces; no game words
  pix.h/.cpp            renderer facade (API unchanged)
  canvas.h        [2]   rgba(), Canvas: all that painters and tools need
  audio.h               Audio facade (API unchanged)
  sound_ids.h     [2]   Sfx / Music / Sting ids and names: all that the sim needs
  audio/          [3]   synth.cpp, sfx.cpp (one table), music.cpp, ambience.cpp, mix.cpp, audio_internal.h
  mathx.h color.h font5x7.h music_style.h
  witness.h witness_impl.cpp witness_emscripten.js      VENDORED: never edited by hand
rpg/
  main.cpp        [3]   about 300 lines: make the App, run the loop
  app/            [3]   app.h (App owns Pix, Audio, Game, View), save_store, input_inject, script_runner,
                        recorder, witness_driver.cpp (EMB_WITNESS only)
  core/           [2]   header-only words every layer may use: common.h (TILE, hashes, IRect, BinW/BinR),
                        art_ids.h (Prop, Monster, Building, Species, Posture, Fx ids, HumanLook, MonsterLook)
  culture/ build/       pure and deterministic (/fp:precise); build/ split by stage [1]
  world/                endless generator by layer; records.h [2] (SiteType, Role, Bldg, Spawn, Den, Map bits);
                        chunk stages in chunk_*.cpp [3]; town planners [3]; vignette table [5]
  art/                  painters by family; prop family table [5]; doll/ split by stage incl. doll_passes, doll_paint [3]
  sim/                  Game core and lanes:
                          game_types.h [4] (Actor, Event, Quest, Input ...), game.h [4] (class Game, core API only)
                          game.cpp (session, world, movement), sim_schedule.cpp [4] (THE update order)
                          game_items / game_quests / game_dialogue / game_shop / game_save.cpp [1]
                          game_combat.cpp [3], dlg_routes.cpp [4]
                          mark_tags.h, monster_stats.h, sim_text.h, looks.h [2]
                          <lane>.h/.cpp (model, no Game) + <lane>_game.cpp (glue) + <lane>_ops.h (hooks)
                          life_ops.h + life_*.cpp [1]
  story/                engine: story.cpp + story_effects / story_text / story_save.cpp [1]; story_api.h (public) [2];
                        story_internal.h (private); story_game / actors / ai / combat / debug.cpp [1]; content folders as now
  view/                 View as a facade over parts: camera, sprite_cache, terrain_bake, fx_state, view_probe [4];
                        render_sorted.cpp [3]; input.cpp, dialogue_ui, shop_ui, screens_ui [1]
    scripts/      [1]   every test/staging script command, named by subject; script_util.h
tools/
  gates/          [0]   the harness: step_gates, wave_gates, clang_ci, build_web, probes, layer/move/size checks, baselines
  tests/                rpg_test lanes; doll_test_common.h [1]
README.md [1]           the code map;  docs/ARCHITECTURE.md [1]: the layers, the rules below, "how do I add X"
```

**Layers.** An arrow means "may include", and nothing may include upward.

```
app -> view -> sim+story -> world -> culture/build -> core -> engine leaf headers (mathx, color, canvas, sound_ids, music_style)
view -> art painters, engine/pix, engine/audio
sim  -> core/art_ids.h and header-only art/doll data (doll.h, doll_forge.h, doll_envelope.h); never painter functions
art  -> core, culture/build (blueprints), doll, world leaf headers (biomes.h, coords.h, ids.h)
tools/tests -> any public header; internals only through the seams listed in tools/gates/layers_allow.txt
```

**Rules: where new code goes.** These go into docs/ARCHITECTURE.md in wave 1, and check_layers / size_report enforce
what they can.
1. **Find the lane.** New gameplay goes in its lane:
   - `<lane>.h/.cpp` for rules and data, with no Game;
   - `<lane>_game.cpp` for the glue;
   - `<lane>_ops.h` for the hooks.

   Game itself gets at most one call in `sim_schedule.cpp` and one row in `dlg_routes.cpp`.
2. **No new members on Game or View for a lane.** Use the lane's Ops struct in the sim, or the lane's own view part
   with its own header.
3. **Test-only code never sits in product files.** Script commands go in `rpg/view/scripts/<subject>.cpp`, and rpg_test
   commands in `tools/tests/test_<subject>.cpp`.
4. **One table row per kind:**
   - a sound is a row in `engine/audio/sfx.cpp`'s list;
   - a prop family is a row in `art_prop_family.h`;
   - a dialogue lane is a row in `dlg_routes.cpp`;
   - a vignette is a row in `vignettes.cpp`;
   - a quest type is a row in `quest_traits.h`;
   - a saved mark tag is one line in `mark_tags.h`, and its number is never changed or reused.
5. **Shared words**, meaning enums and plain structs used by several layers, live in `rpg/core/`. It is header-only and
   includes nothing above it.
6. **Include the narrowest header:**
   - the sim includes `sound_ids.h`, not `audio.h`;
   - painters include `canvas.h`, not `pix.h`;
   - no file includes another folder's `*_internal.h`, except tests through the listed seams.
7. **Size guardrails:** a hand-written logic file stays under about 2,000 lines, and a function under about 300 (new
   code: under about 150). A file over that is split along its section banners before anything is added. Data tables
   and generated files are exempt.
8. **No new process-wide mutable globals** in sim, art or view:
   - per-world state lives in Game, as runtime-only fields marked "never saved";
   - per-frame screen state lives in View;
   - any file-scope state registers a Witness reset.
9. **Determinism:**
   - worldgen uses integer/dmath maths;
   - never pass two RNG draws as arguments of one call;
   - never move code between FP groups;
   - keep the update order, the damage modifier chain and the draw push order exactly as written.
10. **Generated files** (`rpg/art/doll/gen/`) and **vendored files** (`engine/witness.h`, `witness_emscripten.js`) are
    never edited by hand.
11. **Every new file starts with 2-4 lines** saying what lives in it and where its neighbours are.
12. **A new subfolder needs its CMake glob** in the same change, and a new file in rpg_doll needs the `RPG_DOLL_DATA`
    list.

---

## 6. Waves and steps

| Wave | Theme | Steps | Risk | Size | Estimate | What the owner gets |
|---|---|---|---|---|---|---|
| 0 | Safety net | 3 in parallel | none (no game code) | about 1.5k lines of scripts and tests | about 1 day | one command that proves the game is unchanged |
| 1 | Split the grab-bag files along their own seams | 8 in parallel | very low (cut and paste) | about 31k lines moved, about 45 new files | 1-2 days | save code, dialogue, shops, town life, story, input and test scripts each in their own place; a README map |
| 2 | Narrow headers, one-way layers | 5, then 6, then 1 | very low (declarations only) | about 2k lines of declarations, about 300 include lines | 1-2 days | the sim stops pulling in the painters and the audio engine; the world stops including the sim; one registry of saved tags |
| 3 | Break up the god functions | 7 in parallel | medium (proved by pixels and digests) | about 12k lines restructured | 3-5 days | main() about 200 lines; drawWorld, chunk, composeFrame and offerFor as named steps; audio in 5 files with a sound table |
| 4 | Shrink Game and View | 2 in parallel, lane by lane | medium | about 6k lines re-homed | 4-6 days | Game and View become facades over parts; the update order lives in one file |
| 5 | Tables, tooling, docs | 6 in parallel | low | about 3k lines | 2-3 days | one row per prop family, vignette and quest type; CI on every branch; the final map |

The owner can stop after any wave. Each wave leaves the game shippable and better than before.

Owner types follow `~/.claude/agents`: `engineer` (medium), `engineer-high` (hard or central code) and `scaffolder`
(plumbing). There is no visual work, so there are no artists.

### Wave 0: the safety net (before anything else; no game code changes)

**0.1 GATES** · engineer
- **Files:** `tools/gates/step_gates.sh`, `wave_gates.sh`, `ci_cmds.sh`, `clang_ci.sh`, `build_web.bat`, `doll_gates.sh`,
  `witness_smoke.sh`; `tools/gates/baseline/119de74/gates/**`.
- **Brief:** commit the harness that today exists only in `.tmp` and old session scratchpads (verify_batch.sh,
  clang_judge.sh, flip_gates.sh, build_web.bat), as S1-S10 and W1-W7.
  - `ci_cmds.sh` extracts the CI list from web.yml, so the list is never hand-kept.
  - `build_web.bat` reads the emsdk 6.0.13 location from `EMSDK`. Install emsdk at a stable path outside the repo,
    because today's copy sits in a session scratchpad.
  - Every run writes `<out>/summary.txt` and `<out>/build.sha`.
- **Core:** S1-S9 run on 119de74 and match 4.1, and their outputs are recorded.
- **Depth:** a duration for every line; MSVC and clang lines running side by side.
- **Delight:** a one-screen PASS table, and `--only S5,S7` to rerun single gates.
- **Done:**
  - on a fresh build of 119de74, every S line passes, or is recorded as already red and reported to the owner;
  - the harness fails when given a wrong expected TOTAL (negative test);
  - every baseline file exists.

**0.2 PROBES** · engineer
- **Files:** `tools/tests/test_sim_digest.cpp` (new: `rpg_test --sim-digest`), `tools/gates/view_pack.sh`,
  `view_pack_quick.txt`, `audio_pack.sh`, `perf_pack.sh`; `tools/gates/baseline/119de74/probes/**`.
- **Brief:** build the three identity probes the plan relies on, plus perf numbers.
  - **Sim digest:** seeds 1..20, the existing wandering bot, 120 sim-seconds at the fixed step. Every 10 s it hashes
    the `Game::serialize` bytes plus every actor's runtime fields (position, hp, mode, anim, target), the RNG states and
    the marks, printing one line per checkpoint. `--check FILE` compares against a record.
  - **View pack:** runs scripts in their own temp dirs and hashes every PNG they write. The quick list has about 40
    scripts covering the HUD, menus, dialogue, shop, map, town, interior, dungeon, sea, relief and doll; `--all` covers
    all 397 shot scripts.
  - **Audio pack:** runs `audio_preview` in all modes into a temp dir and hashes every WAV.
  - **Perf pack:** `--cityperf`, `--chunkbench` and EMB_PERF frame time.
- **Core:** every probe is A/A-stable, meaning two runs on the same binary agree. Unstable items are excluded and listed
  with their cause; each one is a finding to report, not something to fix here.
- **Depth:** digest baselines on both MSVC and clang.
- **Delight:** on a mismatch, the probe names the first differing field, actor or frame.
- **Done:**
  - A/A agreement on at least 18 of 20 seeds, at least 35 of 40 quick scripts, and all WAVs, with every exclusion
    explained;
  - each probe catches a planted change in a throwaway worktree (one sim constant, one pixel colour, one synth gain),
    and that worktree is discarded.

**0.3 CHECKS** · scaffolder
- **Files:** `tools/gates/check_layers.sh`, `layers_allow.txt`, `move_check.sh`, `size_report.sh`.
- **Brief:** three structure checks.
  - **check_layers** lists forbidden include patterns per folder, with today's violations in `layers_allow.txt`. That
    file may only shrink. The patterns are:
    - world, culture or build including `rpg/sim/`;
    - sim including `rpg/view/`;
    - any file including another folder's `*_internal.h`;
    - the mark mix constant restated outside `mark_tags.h`.
  - **move_check BASE HEAD FILES** prints the lines that are new in HEAD and absent from BASE, ignoring blanks, braces,
    includes, namespace lines, forward declarations and comments. It answers "was this really only moved?".
  - **size_report** lists hand-written files over 2,000 lines and functions over 300 lines, against an allowlist of
    today's cohesive big files.
- **Done:**
  - check_layers passes at 119de74, and flags a forbidden include newly added in a scratch copy;
  - move_check prints 0 lines for a synthetic cut-and-paste, and prints the edited lines for a synthetic edit.

### Wave 1: split the grab-bag files along their own seams (pure moves; CMakeLists.txt owned by 1.5 only)

Rules for every wave-1 step:
- Cut and paste only: copy bytes, never retype.
- No renames, no signature changes and no reformatting.
- A helper used by two new files goes into the step's private header as `inline`.
- Every new file gets a 2-4 line header.
- move_check must show nothing but allowed lines.

**1.1 SIM-RPG** · engineer · about 2.9k lines
- **Files:** `rpg/sim/game_rpg.cpp`; new `rpg/sim/game_items.cpp`, `game_quests.cpp`, `game_dialogue.cpp`, `game_shop.cpp`,
  `game_save.cpp`, `game_rpg_detail.h`.
- **Brief:**

  | New file | Contents | Lines in game_rpg.cpp |
  |---|---|---|
  | `game_save.cpp` | SAVE_VER, read/writeAppearance, serialize, deserialize, saveFromOlderGenerator, byte for byte | 2404-end |
  | `game_shop.cpp` | shopStock, openShop, buy, sell, priceFactor | 2090-2403 |
  | `game_dialogue.cpp` | greeting, talk*, dialogueChoose* | about 1290-2056 |
  | `game_quests.cpp` | the M0 quest core | 870-about 1290 |
  | `game_items.cpp` | inventory, loot, chests | 38-about 535 |
  | stays in `game_rpg.cpp` | interaction, rest and the inn | |
- **Core:** the five files.
- **Depth:** each file's header lists its public functions.
- **Delight:** `game_save.cpp`'s header lists every save block in write order, with its version byte.
- **Done:**
  - `SAVE_VER` appears in exactly one .cpp, and `game_rpg.cpp` is 700 lines or fewer;
  - move_check is clean, and the sim digest is identical;
  - S1-S10 pass, plus `--loot`, `--loot-sources --seeds 1..3`, `--quests`, `--dialogue --seeds 1..3`,
    `--quest-mix --seeds 1..20 --gates --p1b --gates-p1b` and `--save-soak 100`.
- **Watch:** never move loot code into `gear.cpp`/`items.cpp` (the precise FP group).

**1.2 SIM-LIFE** · engineer-high · about 5.2k lines
- **Files:** `rpg/sim/life_game.cpp`; new `rpg/sim/life_ops.h`, `life_spots.cpp`, `life_paths.cpp`, `life_spawn.cpp`,
  `life_street.cpp`, `life_indoors.cpp`, `life_critters.cpp`, `life_incidents.cpp`, `life_lines.cpp`.
- **Brief:** declare `struct LifeOps` in `life_ops.h`, together with the file's anonymous-namespace helpers (72-206) as
  `inline`. Then define its members across files that follow the existing banners:

  | Lines in life_game.cpp | Contents | New file |
  |---|---|---|
  | 398-671 | plans, outdoor spots, reservations | `life_spots` |
  | 672-862 | paths | `life_paths` |
  | 863-1997 | spawning residents | `life_spawn` |
  | 1998-2484 | street life | `life_street` |
  | 2485-3429 | interiors | `life_indoors` |
  | 3430-3792 | village animals | `life_critters` |
  | 3793-4460 | overheard talk and incidents | `life_incidents` |
  | 4461-4585 | hook internals, dialogue lines | `life_lines` |

  `life_game.cpp` keeps the `Game::life*` hooks (4588-5260).
- **Done:**
  - no file of the set is over 1,200 lines, and move_check is clean;
  - S1-S10 pass, plus `--life --seeds 1..5`, `--folk --seeds 1..5`, `--folk-sim`,
    `--town-motion --seeds 1..8 --city --gates` and `--life-p1 --seeds 1..3 --hooks`;
  - the sim digest is identical, and `--cityperf` is within noise plus 3%.
- **Watch:** members defined inside the struct were implicitly inline, and `streetFolk`/`interiorFolk` are hot. Keep each
  cluster's internal calls inside its own file, and mark small hot helpers `inline` in `life_ops.h`.

**1.3 STORY** · engineer · about 4.1k lines
- **Files:** `rpg/story/story.cpp`, `story_game.cpp`; new `story_effects.cpp`, `story_text.cpp`, `story_save.cpp`,
  `story_actors.cpp`, `story_ai.cpp`, `story_combat.cpp`, `story_debug.cpp`, `story_detail.h`.
- **Brief:** split `story.cpp` three ways:
  - `story_effects.cpp`: the effects interpreter (runEffect1) and evalCond;
  - `story_text.cpp`: the line and word helpers (about 1186-1750);
  - `story_save.cpp`: serialize/deserialize (2122-2238, with the v3 guard).

  Split `story_game.cpp` likewise. The hooks and engineTick stay; the rest moves:
  - `story_actors.cpp`: syncPersons, engineSync, dress/place, placeWantedProps, placeInsiders;
  - `story_ai.cpp`: followStep, droveStep, folkStep, hostileStep;
  - `story_combat.cpp`: untouchable, boutBlow, threatensTown;
  - `story_debug.cpp`: debugBout, debugEscort, debug*.
- **Done:**
  - `story.cpp` is 1,300 lines or fewer and `story_game.cpp` 700 or fewer; move_check is clean;
  - S1-S10 pass, plus `--story --seeds 1..5`, `--sagalib`, `--sagas 2000 --seeds 1..5`, `--campaigns --seeds 1..5`,
    `--story-p1 --seeds 1..3`, `--story-events --seeds 1..3`, `--content-p1b --seeds 1..3 --gates`,
    `--radiant --seeds 1..3` and `--p1b-engine --seeds 1..5`;
  - the sim digest is identical.

**1.4 VIEW-HUD** · engineer · about 1.4k lines
- **Files:** `rpg/view/hud.cpp`; new `rpg/view/input.cpp`, `dialogue_ui.cpp`, `shop_ui.cpp`, `screens_ui.cpp`,
  `hud_layout.h`.
- **Brief:**
  - `View::input`, `menuKey`, `tap` and `event` (2755-3402) go to `input.cpp`;
  - `drawDialogue` (2324-2468) goes to `dialogue_ui.cpp`;
  - `shopDeal` and `drawShop` (2469-2639) go to `shop_ui.cpp`;
  - `drawLevelUp`, `drawTitle`, `drawDead`, `titleTap` and `buttonAt` (2640-2754) go to `screens_ui.cpp`;
  - the layout helpers shared by drawing and tapping (menuLay, heroLay, tabLay, menuCols, sysTop ...) go to
    `hud_layout.h` (inline, in a namespace), so the touch rects keep a single source.
- **Done:**
  - `hud.cpp` is 2,100 lines or fewer, with no SDL event handling left in it; move_check is clean;
  - S1-S10 pass, with the quick view pack covering menus, dialogue, shop, title and death;
  - Witness `touch_snapshot` passes.

**1.5 VIEW-SCRIPTS** · engineer · about 14k lines, mostly renames
- **Files:** `rpg/view/script_*.cpp` except `script_api.cpp/.h`; `relief_script.cpp`; `sea_art_script.cpp`;
  `rpg/view/terrain.cpp`; `rpg/view/render.cpp`; new `rpg/view/scripts/**`; `CMakeLists.txt`.
- **Brief:**
  - `git mv` every test/staging command file into `rpg/view/scripts/`, named by subject (`war.cpp`, `party_view.cpp`,
    `urban_stage.cpp` ...). The commit message carries a table of old and new names.
  - Move terrain.cpp's test tail (6285-6779: gotourb, expect:kerbs, urbstage, tilescan, wallspot, `g_stage`) and
    render.cpp's `weather` command into `scripts/`.
  - `scripts/script_util.h` holds only the helpers that are byte-identical across files (lower, nearestSettlement(s),
    playerGlobal, worldMapCentreTile, dress ...). A helper whose copies differ keeps its copies.
  - In CMake, add `rpg/view/scripts/*.cpp` to the embervale target's `RPG_VIEW` glob, as sources of the executable.
    Do **not** make them a static library: the commands register themselves, and a static library would drop them.
- **Done:**
  - `ls rpg/view/script_*.cpp` lists only `script_api.cpp`, and there is no `EMB_SCRIPT_CMD` outside
    `rpg/view/scripts/`;
  - the sorted command list (`--script-help`, or the Witness calls list) equals the baseline's 297 names;
  - W2 (all scripts 0 failed, `view_pack --all` identical) and S1-S10 pass in this step.
- **Watch:** `View::urbKerbCheck` stays in terrain.cpp, because it is a View member that uses terrain internals.

**1.6 BUILD** · engineer · about 3.2k lines
- **Files:** `rpg/build/builder.cpp`; new `rpg/build/build_geom.cpp`, `build_forms.cpp`, `build_culture.cpp`,
  `build_finish.cpp`, `build_detail.h`.
- **Brief:**
  - roof and volume helpers (about 103-489) go to `build_geom`;
  - per-purpose forms (about 1088-1760) go to `build_forms`;
  - `cultureHome` (about 710-1085) and `seat*` (about 1914-2404) go to `build_culture`;
  - ground, streetFaces, terrace and finish go to `build_finish`;
  - `builder.cpp` keeps `design()` and the Request plumbing.
- **Done:**
  - `builder.cpp` is 600 lines or fewer;
  - the new files show `/fp:precise` (MSVC) and `-ffp-contract=off` (clang) in `ninja -t commands`; move_check is clean;
  - S1-S10 pass, plus `--builder --seeds 1..12 --strict --society-strict`, `--towns --golden`, `--villages --strict`,
    `--structure --strict` and `--city-unique --strict`.
- **Watch:** the order of the Bx draw calls is pixel identity for every building.

**1.7 TEST-TOOLS** · engineer
- **Files:** `tools/tests/test_doll*.cpp`, `tools/tests/doll_motion.h`, `tools/tests/test_hero.cpp`,
  `tools/tests/test_arms.cpp`; new `tools/tests/doll_test_common.h/.cpp`.
- **Brief:**
  - One header holds the doll tests' shared pieces and replaces the hand-written cross-file prototypes: Look,
    parseLooks/readLooks/readFile (only byte-identical copies merge), the embedded looks-text accessors and the gate
    signatures.
  - An `RPG_DOLL_GATE(order, name, fn)` registry lets `--doll --gates` iterate instead of hard-coding, keeping today's
    order through explicit order keys.
  - Delete the dead `#ifndef EMB_RPG_TEST_HAS_ART` include fallbacks in `test_hero.cpp` and `test_arms.cpp`.
- **Done:**
  - no cross-file prototypes are left in `test_doll*.cpp`;
  - `rpg_test --doll --gates` and every doll line of `doll_gates.sh` print the baseline output line for line;
  - S1-S10 pass.

**1.8 DOCS** · engineer
- **Files:** `README.md`, new `docs/ARCHITECTURE.md`.
- **Brief:**
  - `README.md` becomes the code map: each folder and what it holds, the big files and what is in them, how to build
    (`tools\build.bat`, BDIR), how to test (`tools/gates`) and where new code goes.
  - `docs/ARCHITECTURE.md` holds the layer picture, the lane pattern, the registries, the determinism and FP rules,
    the 12 rules from section 5, and a "how do I add X" cookbook covering a sound, a prop family, a script command, an
    rpg_test command, a dialogue lane, a saved mark and a whole lane.
  - Both are written for the post-wave-1 tree, and each later wave's docs step updates them.
- **Done:** every backticked path in README exists (a one-line check), and the ten "where is X" questions in section 9
  each resolve to one file through README.

### Wave 2: narrow headers, one-way layers (declarations move; no logic)

Wave 2 runs in three stages: providers (2a), then consumers (2b), then a trim (2c).

**2a: providers.** These run in parallel. Each old header includes the new one, so nothing else breaks.
- **2a.1 ENGINE** · engineer
  - **Files:** `engine/audio.h`, `engine/pix.h`; new `engine/sound_ids.h`, `engine/canvas.h`; `CMakeLists.txt`.
  - **Brief:**
    - `sound_ids.h` gets enum Sfx, enum Music and Sting, with `Audio::Sting` kept as an alias;
    - `canvas.h` gets `rgba()` and Canvas;
    - CMake gains globs that are empty for now: `engine/audio/*.cpp` in the engine library and `rpg/app/*.cpp` in the
      embervale target, so wave 3 needs no CMake edits.
  - **Done:** enum values are identical (the sfxName dump for 0..COUNT equals baseline), and S1-S10 pass.
- **2a.2 CORE** · engineer-high
  - **Files:** new `rpg/core/common.h`, `rpg/core/art_ids.h`; `rpg/sim/common.h`; the art headers whose vocabulary
    moves (`art_props.h`, `art_human.h`, `art_monsters.h`, `art_building.h`, `art_fx.h`, `art_items.h`, `art_life.h`,
    `art_culture.h`; the exact list is declared before starting).
  - **Brief:**
    - `common.h` takes all of `rpg/sim/common.h` (TILE, SIM_DT, hashes, IRect, BinW/BinR ...), and `sim/common.h`
      becomes a forwarding include;
    - `art_ids.h` takes Prop with its predicates, footprints and stall geometry; Monster, Species, Building, ArchStyle,
      Posture and the Fx ids; and HumanLook and MonsterLook, with `key()` declared only;
    - the art headers include `art_ids.h`.
  - **Done:**
    - `rpg/core` includes nothing from `rpg/` outside core;
    - static_asserts on the size of HumanLook and on `Prop::COUNT`/`Monster::COUNT` hold before and after;
    - art_hash, village and freeze are identical, and S1-S10 pass.
- **2a.3 WORLD-RECORDS** · engineer
  - **Files:** new `rpg/world/records.h`; `rpg/sim/world.h`.
  - **Brief:** `records.h` holds plain records only: MapKind, SiteType, Role, Bldg, Site, Kingdom, Spawn, Den, the
    WORLDGEN_* and Map bit constants. Helpers that call into build or art (bldgRequest, bldgBlueprint ...) stay in
    `sim/world.h`. For now it may include the art headers it needs; 2c narrows them.
  - **Done:** no `rpg/sim` include in `records.h`; layouts identical (static_asserts); S1-S10 pass.
- **2a.4 SIM-SEAMS** · engineer
  - **Files:** `rpg/sim/game_internal.h`, `looks.cpp`, `player.cpp`; new `rpg/sim/mark_tags.h`, `monster_stats.h`,
    `sim_text.h`, `looks.h`; new `tools/tests/test_marks.cpp`.
  - **Brief:**
    - `mark_tags.h` lists every saved tag by name, with its owner and its value, collected by grep across all lanes:

      | Owner | Values |
      |---|---|
      | `Mk` enum | 1..10 |
      | WARDS | 32..47 |
      | REALM | 48..63 |
      | FOES | 64..79 |
      | CRAFT | 80..95 |
      | HOME | 96..111 |
      | FELLOWS | 112..127 |
      | JOBS | 128..143 |
      | LIFE | 144..159 |
      | REQ_RAISE | 176 |
      | SEA | 200 |
      | the view's biome-seen key | tag 8 with ids 0x7E000+eco, documented as sharing StoryGiver's tag across disjoint id spaces |

      A constexpr list plus a static_assert checks that the values are unique, apart from that one documented share.
      `markKey` moves here too.
    - `monster_stats.h` takes mstat, heavyWindup and monsterName.
    - `sim_text.h` takes the helpers that scripts and tests use (dirWord, giverTown, titles ...).
    - `looks.h` declares census::Rank, census::dress, ageLook and appearanceLook.
    - `rpg_test --marks` hashes 12 known (id, tag) keys against hard-coded expected values. That proves save
      compatibility from now on.
  - **Done:** `--marks` OK, and S1-S10 pass.
- **2a.5 STORY+DOLL** · engineer
  - **Files:** `rpg/story/story_internal.h`, new `rpg/story/story_api.h`; `rpg/art/doll/doll.h`,
    `rpg/art/doll/doll_pool.cpp`.
  - **Brief:**
    - `story_api.h` is the subset other lanes use: isStoryPerson, isDroveBeast, the person/drove/herald slot ranges
      (PERSON_SLOT0 = 50000 ...), the MK_* names, kingdomName, siteByIdLoad, scriptById, placeLoreProps,
      requestOfferForPlace, bindingOf, and the debug and host entry points that scripts use;
    - `doll::clipsOf(name)` gives `anim.cpp` the elder walk clip without `doll_internal.h`.
  - **Done:** `story_api.h` includes no `*_internal.h`; `clipsOf` returns the same records as `pool::clipsOf` (checked by
    a test); S1-S10 pass.

**2b: consumers.** These run in parallel, one owner per folder. Each switches files to the narrow headers, but does
**not** remove includes that other folders may rely on (that is 2c).
- **2b.1 SIM** · engineer
  - **Files:** `rpg/sim/**` except the 2a files.
  - **Brief:**
    - `sound_ids.h` replaces `audio.h` (32 files);
    - lane mark constants become `mark_tags.h` names, with the same values;
    - `anim.cpp` uses `doll::clipsOf`;
    - core headers are used where only vocabulary is needed.
  - **Done:**
    - `engine/audio.h` is included by 0 sim files, and `doll_internal.h` by 0 sim files;
    - after touching `audio.h`, `ninja -n` recompiles 0 rpg_sim objects;
    - `--marks` OK, the sim digest is identical, and S1-S10 pass.
- **2b.2 STORY** · engineer
  - **Files:** `rpg/story/**` except the 2a.5 files.
  - **Brief:** MK_* names come from `mark_tags.h`. A file that only uses public helpers swaps `game_internal.h` for
    `sim_text.h`/`monster_stats.h`.
  - **Done:** the story files that still include `game_internal.h` are listed, each with the reason, and S1-S10 pass.
- **2b.3 WORLD+CULTURE+BUILD** · engineer
  - **Files:** `rpg/world/**` except `records.h`, `rpg/culture/**`, `rpg/build/**`.
  - **Brief:**
    - `core/common.h` replaces `sim/common.h`;
    - `records.h` replaces `sim/world.h` wherever it is enough;
    - `core/art_ids.h` replaces `art_building.h` where only enums are used.
  - **Done:** goldens identical, and S1-S10 pass.
- **2b.4 ART** · engineer
  - **Files:** `rpg/art/**` except the 2a.2 headers, `doll.h`/`doll_pool.cpp`, `rpg/art/rig/**` and `art_human.cpp`.
  - **Brief:** painter headers include `canvas.h` instead of `pix.h` where Canvas is all they need.
  - **Done:** the only art headers including `pix.h` are those that need Tex or Pix; art_hash, village and freeze are
    identical; S1-S10 pass.
- **2b.5 VIEW+MAIN** · engineer
  - **Files:** `rpg/view/**`, `rpg/main.cpp`.
  - **Brief:**
    - the three restated mark keys in `render.cpp` and the one in `script_foes.cpp` become `markKey` calls (`--marks`
      proves the keys are equal);
    - `render.cpp`'s wind-up literals become `gsim::heavyWindup(a.mon)`;
    - `story_api.h` and `sim_text.h` are used where they are enough.
  - **Done:** grep finds 0xD1B54A32D192ED03 in `rpg/view` 0 times, and there are no wind-up literals; the view pack is
    identical; S1-S10 pass.
- **2b.6 TOOLS** · engineer
  - **Files:** `tools/**` except `tools/gates/**`, `art_hash.cpp`, `save_test.cpp` and `test_marks.cpp`.
  - **Brief:** `looks.h` replaces the hand-copied census::Rank, dress and ageLook declarations; `story_api.h` and
    `sim_text.h` are used where they are enough.
  - **Done:** `struct Rank` appears in tools 0 times; the internal includes left in tools are listed as white-box tests in
    `layers_allow.txt`; S1-S10 pass.

**2c TRIM** · engineer. This step runs **alone** after 2b and owns include lines across the whole repo.
- **Brief:**
  - `game.h` and `sim/world.h` stop including `rpg/art.h`; they use `core/art_ids.h` plus the leaf art headers they truly
    need;
  - the world headers (poi, source, town_plan, town_rules, town_edge) stop including `rpg/sim/*`;
  - `records.h` switches to `core/art_ids.h`;
  - every file that relied on a removed transitive include gets an explicit one.
- **Done:**
  - check_layers shows 0 entries for world/culture/build including sim, and sim includes painter headers only where
    listed;
  - `layers_allow.txt` is shorter by every removed entry;
  - clean full rebuilds on MSVC, clang and web; S1-S10 and W1-W6 pass.

### Wave 3: break up the god functions

Seven areas run in parallel, with no CMake edits. Each shared header has one owner: `game.h` belongs to 3.5, `view.h` to
3.4, `gen.h` to 3.2 and `sound_ids.h` to 3.7.

**3.1 APP** · engineer-high · sub-steps a-g, one commit each
- **Files:** `rpg/main.cpp`; new `rpg/app/**`.
- **Brief:**
  - **a.** Create `struct App` holding Pix, Audio, Game, View and the state the lambdas share. The lambdas become App
    members, still in `main.cpp`.
  - **b.** `rpg/app/save_store.h/.cpp`: readSave/writeSave and the Emscripten IDBFS sync.
  - **c.** `input_inject.h/.cpp`: pushEvent, pushKey, pushFinger, pushClick, pushMouse, toWindow, setMove.
  - **d.** `script_runner.h/.cpp`: ScriptCmd, loadScript, keyFromName, runCmd (with its op branches unchanged),
    scriptStep, the walk/talk/fight/giveGear helpers, findGiver and the test-only findPath.
  - **e.** `recorder.h/.cpp`: recordEvent, recLetGo and replay.
  - **f.** `witness_driver.cpp`: every `h.*` handler, with the whole file under `#ifdef EMB_WITNESS`.
  - **g.** `main.cpp` keeps the SDL init, the frame loop and the `emscripten_set_main_loop` glue.
- **Done:**
  - `main.cpp` is 400 lines or fewer, `main()` 200 or fewer, with at most 4 `#if` blocks left;
  - move_check shows runCmd's branches unchanged;
  - every sub-step passes S1-S10 plus all 15 Witness tests;
  - at b and g, these also pass: the real web build, the first-launch probe, the web-witness build
    (`EMBERVALE_WITNESS_WEB=ON`) and `witness doctor --config tools/witness_web --device iphone-15-landscape --engine webkit`;
  - at g, W2 passes, and a WITNESS_RECORD session recorded on 119de74 replays to the same final state.
- **Watch:**
  - the latched one-shots carried across frames, and the step order inside `h.step`;
  - the statematch/shotmatch semantics;
  - tests must keep their own save path and never use the owner's.

**3.2 WORLD-CHUNK** · engineer-high
- **Files:** `rpg/world/chunkgen.cpp`, `rpg/world/gen.h`; new `chunk_relief.cpp`, `chunk_roads.cpp`, `chunk_flora.cpp`.
- **Brief:** a ChunkCtx holds the stamp, the base rects, the level grid with its border, the river grids and the
  reserved masks. Each numbered stage becomes one Impl method, called in today's order: base, towns, harbours, stamps,
  roads, relief, river beds, boardwalks/ecotones, massifs, peaks, cairns, flora. `chunk()` becomes the driver, and the
  relief, roads and flora stages move to their own files.
- **Done:**
  - `chunk()` is 80 lines or fewer, with no stage over 400;
  - the new files compile as `/fp:precise` and `-ffp-contract=off`;
  - identical output from `--endless --golden`, `--golden`, `--endless --seeds 1..10 --no-budget`,
    `--window --seeds 1..5`, `--relief --seeds 1..3 --no-budget`, `--climb --seeds 1..3`, `--forest --seeds 1..20` and
    `--biomes --seeds 1..5 --strict`;
  - `--chunkbench` within noise plus 5%; view pack and village art identical; S1-S10 pass.

**3.3 WORLD-TOWNS** · engineer
- **Files:** `rpg/world/town_market.cpp`, `town_build.cpp`, `town_urban.cpp` (no header edits).
- **Brief:** the members heavy with lambdas become file-local planner classes holding `Gen&`: MarketPlanner (markets,
  tradeYards), CompoundPlanner (buildCompound) and CityDresser (cityDress). They make the same Rng draws in the same
  order.
- **Done:**
  - no member function in these files is over 300 lines;
  - identical output from `--towns --golden`, `--towns --seeds 1..10`, `--villages --strict`, `--structure --strict`,
    `--city-unique --strict`, `--plan-controls` and `--builder --seeds 1..12 --strict --society-strict`;
  - village art identical; S1-S10 pass.

**3.4 VIEW-FRAME** · engineer-high · sub-steps a-d
- **Files:** `rpg/view/view.h`, `render.cpp`, `hud.cpp`, `worldmap.cpp`; new `render_sorted.cpp`, `worldmap_markers.cpp`.
- **Brief:**
  - **a. drawWorld:**
    - `enum class DrawKind`, with the same values 0..13;
    - `collectDrawables()`, keeping today's push order and the same sort call;
    - one member per kind (drawPropSorted, drawBuildingSorted, drawWallSorted, drawActorSorted, drawGateSorted ...),
      taking a FrameCtx (camera, occlusion records, roof alpha) by reference.

    All of this goes in `render_sorted.cpp`.
  - **b. View::update:** splits into updateCamera (with the s_camRes and kCamSnap rounding kept as one unit),
    updateBanners, updateBeastCues and updateMusic.
  - **c. drawHud:** splits into hudVitals, hudQuickSlots, hudBurden, hudLocationColumn and hudClock, in the same order,
    with the shared locals passed explicitly.
  - **d. drawWorldMap:** splits into mapTiles, mapPlans, mapMarkersWar/Home/Party/Sea and placeLabels (the markers go in
    `worldmap_markers.cpp`); `paintMapTile` splits by zoom group.
- **Done:**
  - `drawWorld` is 200 lines or fewer, `update` 150, `drawHud` 150 and `drawWorldMap` 200;
  - `view_pack --all` identical; village art identical; the P2a lens Witness tests (occlusion stats) identical;
  - EMB_PERF frame time within noise plus 3%; S1-S10 pass.
- **Watch:** push order and sort ties; the order of `g_occRecs`; the camera rounding, which feeds Witness camera hold.

**3.5 SIM-FUNCS** · engineer-high
- **Files:** `rpg/sim/game.h`, `game.cpp`, `ai.cpp`, `quests.cpp`, `magic_game.cpp`; new `game_combat.cpp`,
  `ai_beasts.cpp`, `quest_offers.cpp`, `magic_launch.cpp`.
- **Brief:**
  - `game_combat.cpp` takes meleeHit, damage, kill, xp, shootArrow, updateProjectiles and pickups. damage's 17-modifier
    chain stays inline and in order, with each stage under a named comment.
  - `ai_beasts.cpp` takes updateAI's per-species branches as functions, chosen by the same if-ladder.
  - `quest_offers.cpp` takes offerFor as offerClear, offerDeliver, offerHeirloom ..., chosen by a switch at the top.
  - `magic_launch.cpp` takes MagicOps::launch's form cases, as one function per form group.

  Helpers that need private state become private members in one marked block; wave 4 re-homes them.
- **Done:**
  - no touched function is over 300 lines (offerFor 120 or fewer, as a dispatcher);
  - the sim digest is identical (RNG draw order);
  - these pass: `--quest-mix --seeds 1..20 --gates --p1b --gates-p1b`, `--quests`, `--mainquest`, `--loot`,
    `--foes --seeds 1..3`, `--spells`, `--skills`, `--powers`, `--companion --seeds 1..3`;
  - S1-S10 pass.

**3.6 DOLL-COMPOSE** · engineer-high
- **Files:** `rpg/art/doll/doll_compose.cpp`; new `doll_passes.cpp`, `doll_paint.cpp`, `doll_compose_internal.h`.
- **Brief:**
  - the post passes (despeckle, loneLit, glint, glintCap, glass, starBand, mythicFire, finish) go to `doll_passes.cpp`,
    behind `void xPass(const DollRecipe&, FrameOut&)`;
  - composeFrame's locals become `struct FramePaint` (anchors, paintBackToFront, stampEdges) in `doll_paint.cpp`, with
    the same thread_local reuse;
  - the pass order sits in one list;
  - frameInfo's side channel (g_items, g_probeOwner) stays with its owner file or behind the internal header.
- **Done:**
  - `composeFrame` is 250 lines or fewer;
  - freeze 0 changed on both lines;
  - identical output from `--doll --gates --parity`, coverage, variety, ladder selftest and every line of
    `doll_gates.sh`; art_hash at both rigs, village and view pack identical;
  - doll bake timing within 5%; S1-S10 pass.

**3.7 AUDIO** · engineer
- **Files:** `engine/audio.h`, `engine/audio.cpp`, `engine/sound_ids.h`; new `engine/audio/**`.
- **Brief:** split the engine into files under `engine/audio/`:
  - `synth.cpp`: Note, Voice, renderVoice, add;
  - `sfx.cpp`: one `SFX_LIST(name, trim)` X-macro that generates the Sfx enum in `sound_ids.h`, `sfxName` and the trims,
    so their order cannot drift. `trigger()`'s 83 arms become static functions in a table, moved verbatim.
  - `music.cpp`: Seq, compose, genMotif, seqStep*, melody, instruments;
  - `ambience.cpp`: renderAmbient, Sea, Falls, Crowd;
  - `mix.cpp`: renderBlock, reverb, limiter, the device stream;
  - `audio_internal.h`.

  The public API, the atomics and every RNG call order stay unchanged.
- **Done:**
  - `audio.cpp` is 300 lines or fewer, or gone;
  - every WAV in the audio pack is byte-identical; the sfxName dump for 0..COUNT and the Witness sound names are
    identical;
  - no `case Sfx::` left; S1-S10 pass.

### Wave 4: shrink Game and View

Two owners run in parallel. Each works lane by lane or part by part, with one commit per sub-step.

**4.1 GAME** · engineer-high
- **Files:** `rpg/sim/**` (including `game.h`); `rpg/story/story_game.cpp` and `story_actors`/`ai`/`combat`/`debug.cpp`
  (they define `Game::story*` members); `tools/tests/**` (call-site renames only).
- **Brief:**
  - **Lane hooks:** work through the lanes in this order: life, party, home, foes, craft/loot/power, war/realm,
    travel/wayside, anim, and story last.
    - A hook called only from inside `rpg/sim` and `rpg/story` moves from Game to the lane's Ops struct
      (`Game::lifeStep` becomes `LifeOps::step(Game&, float)`), the way MagicOps already works.
    - A hook still called from view, main or the gate tools stays a Game member, so view and main need no edits.
  - **Update order:** moves verbatim into `sim_schedule.cpp`: one function, with one comment per step. That file is the
    determinism contract.
  - **Dialogue routing:** the `DLG_*` range checks become `dlg_routes.cpp`, with rows `{lo, hi, choose}` in today's
    check order and a static_assert that they are sorted and do not overlap.
  - **Header split:** `game.h` splits into `game_types.h` (Mode, Input, Actor, Projectile, Pickup, Event, Quest,
    Dialogue, Travel, Shop) and `game.h` (class Game).
  - **QuestMixOps** folds into JobOps.
- **Done:**
  - Game has 160 member functions or fewer, and `game.h` is 700 lines or fewer;
  - a file that includes only `game_types.h` compiles without the lane headers;
  - the body of `sim_schedule.cpp` equals the old update ladder (move_check);
  - the sim digest is identical after every lane, and S1-S10 pass for each lane.
- **Watch:** never reorder the schedule or the damage chain. Dialogue ids are never saved, but check that no Dialogue is
  serialized in mid-conversation.

**4.2 VIEW** · engineer-high
- **Files:** `rpg/view/**`, including `view.h`, `witness_state.cpp`, and `scripts/` (accessor renames only).
- **Brief:** View keeps its API and becomes a facade over value members, one per sub-step:

  | Part | What moves into it | New files |
  |---|---|---|
  | TerrainBaker | chunks_, jobs_, workers, BakeJob, TMap, prefetch, pumpBake, settleBakes | `terrain_bake.h/.cpp` |
  | SpriteCache | the human, building, wall, fort, heraldry, kingdom and styled-prop caches; trimCaches; peopleMB | `sprite_cache.h/.cpp` |
  | Camera | cam_, zoom, toUi/toWorld, uiInsetsToWorld | `camera.h/.cpp` |
  | FxState | parts_, floats_, toasts_, shake | `fx_state.h/.cpp` |
  | ViewProbe | the ~23 test and Witness hooks and counters, included only by scripts and witness_state.cpp | `view_probe.h` |

  In addition:
  - the `loot_paint.h` include moves from `view.h` to `loot_ui.cpp`;
  - an `EMB_VIEW_RESET(fn)` registry replaces `witnessRelease`'s hand-kept list. The resets stay the same, but none can
    be forgotten any more.
- **Done:**
  - View has 200 member functions or fewer and 120 data members or fewer;
  - `view.h` is 700 lines or fewer, without `loot_paint.h`;
  - the Witness determinism tests pass with `--repeat 2`; `view_pack --all` identical;
  - frame time within noise plus 3%; S1-S10 pass for each part.
- **Optional 4.2x:** the terrain painters use no View state, so groundPixel, reliefPixel and rockLevel could become free
  functions in `terrain_paint.cpp`, with thin View wrappers and groundPixel's helpers kept in ONE file. Do this only if
  the per-chunk bake hashes and the view pack stay identical; otherwise skip it.

### Wave 5: tables, tooling, docs (6 steps in parallel)

**5.1 ART-TABLES** · engineer
- **Files:** `rpg/art/art_props.cpp`, `art_monsters.cpp`; new `art_prop_family.h`, `art_holders2.cpp`.
- **Brief:**
  - one ordered PropFamily table `{has, w, h, frames, paint, selfOutlined}`, in today's chain order, drives
    propW/propH/propFrames and paintProp's default branch; `wildP` becomes `isWildProp`;
  - `kMonsterInfo` drives paintCell and monsterCellW/H;
  - the `h2` holders move to `art_holders2.cpp`.
- **Done:** propW/propH/propFrames are 6 lines or fewer each; art_hash at both rigs, village at both rigs, freeze and view
  pack identical; S1-S10 pass.

**5.2 WORLD-TABLE** · engineer
- **Files:** `rpg/world/vignettes.cpp`.
- **Brief:** VignetteDef rows (footprint, terrain test, relaxed rules, name, and the stamp function moved verbatim)
  replace the 6 switches.
- **Done:** at most 1 `switch(VignetteKind)` left; world goldens, `--wayside-census` and view pack identical; S1-S10
  pass.

**5.3 QUEST-TRAITS** · engineer
- **Files:** `rpg/sim/quests.cpp`, new `rpg/sim/quest_traits.h`, `rpg/view/hud.cpp`, and the `rpg/view/scripts/` files
  that switch on QType.
- **Brief:** one traits row per QType (name, icon, target kind, journal line), read by the HUD and the scripts.
- **Done:** those switches become table reads; quest text identical (view pack, `--quests`, `--quest-mix`); S1-S10 pass.

**5.4 TOOLING** · engineer
- **Files:** `.github/workflows/web.yml`, `CMakeLists.txt`, `.gitignore`, `tools/tests/main.cpp`, `tests.h`,
  `tools/art_hash.cpp`, `rpg/sim/gear_ladder.cpp`, `rpg/art/doll/doll_walk.cpp`, new `rpg/art/doll/doll_look_fields.cpp`,
  `tools/witness_web/package.json`.
- **Brief:**
  - **CI split:** web.yml splits into a test job (every branch push and pull request) and web/deploy jobs (main only,
    `needs: test`), with the deploy steps kept verbatim.
  - **Test registry:** TestCmd gains a kind (gate, report or tool), plus `rpg_test --list-ci`.
  - **CI checks:** CI runs check_layers. It reports art_hash on clang, and that line becomes blocking only once clang
    parity is proven.
  - **Shared setter:** art_hash and gear_ladder share one ladder look-field setter, living in rpg_doll next to
    applyLookField and added to `RPG_DOLL_DATA`.
  - **Git hygiene:** untrack `tools/doll/__pycache__` and `alloys/state.pkl`, and commit `witness_web/package.json`.
  - **CMake cleanup:** drop the always-true `if(EXISTS)` guards (but not the rig-gallery exclusion) and
    `EMB_RPG_TEST_HAS_ART`.
- **Done:**
  - CI is green on a branch push and on a PR;
  - deploy still happens only from main, and the Pages build starts (first-launch probe on the deployed URL);
  - `--list-ci` equals the CI list; the art_hash TOTALs are identical; S1-S10 pass.

**5.5 HYGIENE** · engineer
- **Files:** `rpg/sim/interior_v4.cpp`, `rpg/art/art_market.cpp`, `rpg/art/art_props.h`, `rpg/view/render.cpp`, new
  `tools/tests/test_look_key.cpp`. Optional: `rpg/sim/magic.h/.cpp`, `magic_learn.cpp`.
- **Brief:**
  - `gNopeLine`, which the std::async interior worker writes, becomes atomic or thread_local;
  - `marketStallStyled(..., const MarketSnow&)` scopes the snow state (RAII), and render.cpp's bracketed
    `setMarketSnow` calls migrate to it; the old function stays as a shim;
  - a unit test flips each HumanLook field and asserts that `key()` changes; `key()` itself is untouched;
  - optional: `g_songLand` moves into `magic::Arcana` as a runtime-only field that is never serialized.
- **Done:** a TSan run in WSL on an interior-heavy seed shows no race; `render.cpp` has 0 `setMarketSnow` calls; the key
  test passes; the fixture is untouched; S1-S10 pass.

**5.6 DOCS** · engineer
- **Files:** `README.md`, `docs/ARCHITECTURE.md`, `rpg/sim/*_ops.h` (comments only), `docs/PLAN.md` (one log entry).
- **Brief:** write the final code map, and mark in each Ops header which members are "stable test/script API" and which
  are internal.
- **Done:** the ten where-is questions resolve, and every README path exists.

**Optional later** (each needs its own decision):
- an input layer (`engine/input.h` UiAction with one key-alias table);
- leaving the test scripts out of the deployed web build;
- splitting groundPixel into stages;
- splitting art_building.cpp by stage;
- moving art_internal.h's anonymous-namespace bodies into a .cpp;
- splitting art_props.cpp by family, and interior_v4.cpp by banner;
- grouping Actor's fields per lane;
- gen.h Impl sub-objects and EndlessSource accessor views;
- for Pix: a Blend enum, moving the minimap out, a BlitObserver and a ScreenFrame;
- an rpg_view_paint library;
- a story DSL verb table;
- renaming `ew::Archetype` and `saga::Archetype`;
- moving runCmd's built-ins into the EMB_SCRIPT_CMD registry.

---

## 7. Step contracts

- **Harness:**
  - `tools/gates/step_gates.sh --bdir <dir> --out <dir> [--lines "<rpg_test args>;..."] [--only S5,S7]` exits 0 only
    when every line passes. It writes `<out>/summary.txt` (`NAME PASS|FAIL rc=<n> <secs>s | <last line>`) and
    `<out>/build.sha`.
  - `wave_gates.sh` does the same and adds the W lines.
  - Baselines live in `tools/gates/baseline/119de74/{gates,probes}/` and are never rewritten.
- **Step report** (each step agent's returned text):
  - step id, branch and commit;
  - `git diff --stat`;
  - the move_check count and the new lines;
  - the summary.txt path, with the PASS table and durations;
  - lines of each owned file before and after, and its longest function;
  - findings: bugs or oddities seen but not fixed.
- **Seam headers:** their contents are as given in wave 2 (`sound_ids.h`, `canvas.h`, `core/common.h`,
  `core/art_ids.h`, `world/records.h`, `sim/mark_tags.h`, `sim/monster_stats.h`, `sim/sim_text.h`, `sim/looks.h`,
  `story/story_api.h`, `doll::clipsOf`). Each one says at its top who may include it.
- **Test hooks the plan relies on:**
  - Witness: state, reset params, shots, calls, record and replay;
  - `embervale --script` (scripts already drive state, time and the camera);
  - rpg_test, adding `--sim-digest` and `--marks`;
  - the view and audio packs.

  These are the debug and test hook; no new runtime hook is needed.

---

## 8. Do not touch (and why)

| What | Why |
|---|---|
| `engine/witness.h`, `engine/witness_emscripten.js` | Vendored from `../witness`; they change only through a Witness adoption. |
| `rpg/art/doll/gen/doll_pool.cpp` | Generated by `tools/doll/pxc.mjs`; CI's `--check` fails on hand edits. |
| Rig 1: art_human.cpp's HumanPainter, PostureRig, SleeperRig and overlay maps; `rpg/art/rig/**`; `tools/preview/rig*_gallery.cpp`; the rig-1 fixtures and freeze; every `rigVersion()` branch | Retiring them needs the owner's separate OK. Until then they are the opt-in fallback (`EMB_RIG=1`, `--rig=1`, `?rig=1`). Section 11 lists what still depends on them. art_human.cpp is not split now, because it goes as a whole at retirement. |
| The art itself, the doll's maps and clips; any compression | Owner rule. The pixels are the product. |
| Data tables: art_human overlay maps, culture `priors.cpp`, story content (`lib_tales*`, `tales2`, `arch/*`, `campaigns/*`, `req/*`, `jobs/*`), `gear_forge_shapes.inc`, mstat, interior templates | They are cohesive data, and size is not a smell. Splitting them risks ids that depend on order. |
| `tests/fixtures/*`, `tools/doll/freeze/*`, `tools/doll/*_looks.txt`, `hash_looks.txt`, `witness/refs/*` | These are the baselines. Changing one hides a regression. |
| Save format: SAVE_VER 23, the byte order of serialize/deserialize, the Mk values and the mix constant, the PERSON_SLOT/DROVE ranges, enum values and order (Prop, Monster, Sfx, QType, Building ...: append only) | Old saves must load, and the fixture must stay byte-identical. |
| Determinism contracts: Game::update order, damage()'s modifier chain, RNG draw order, drawable push order and sort ties, `arch/index.cpp` order, the GCC `-funswitch-loops` guard in `life.cpp` | They are what makes worlds, fights and frames repeatable. |
| FP groups (the `/fp:precise` lists in CMake) | Never move code between `rpg/sim` and `rpg/world`; never move interior_v4.cpp into `rpg/world`; never move loot code into or out of `gear.cpp`/`items.cpp`. Any of these changes bits in goldens and layout signatures. |
| Names other tools use: script commands, Witness calls, params and state fields, rpg_test flags, CI step lines | Witness scripts, judges and CI call them by name. |
| The gate tools during waves 0-4: `art_hash.cpp`, `save_test.cpp`, `city_gallery`'s village check, the freeze logic | The ruler must not move while it measures. art_hash changes only in 5.4, and its own TOTAL proves that change. |
| `web/index.html`, the IDBFS export, the emsdk pin | Fixed only at 119de74. Only 5.4 touches `web.yml`, and it keeps the deploy steps verbatim. |
| The owner's machine: `%APPDATA%\Josh17400\Embervale`, the Desktop exe, a running game | Owner rules. |
| Gameplay, balance, text, art, sound | This plan is structure only. Bugs found go into step reports. |

---

## 9. What the judge checks

The judge states its full protocol in round 1, and works from a fresh clone of the wave head with a fresh build dir.
That catches untracked files and stale objects. It measures; nothing passes on an agent's word.

1. **Gates:** W1-W7, re-running every S line with its own harness, never from the step's logs.
2. **Move purity:** move_check for every step, reading every reported line.
3. **Untouched paths:** `git diff --stat 119de74..HEAD -- <section 8 paths>` is empty, apart from the steps that are
   explicitly allowed.
4. **Layers:** check_layers is clean, and `layers_allow.txt` is shorter than at the previous wave.
5. **Sizes:** size_report against each step's "done" numbers.
6. **Discoverability quiz:** ten questions, each answered from README in at most two steps:
   1. where is the save format;
   2. where is the update order;
   3. where is a sound defined;
   4. where does a prop family get its size;
   5. where do town folk find paths;
   6. where are the HUD vitals drawn;
   7. where is input handled;
   8. where does a new test script command go;
   9. where do the Witness handlers live;
   10. where is chunk relief computed.
7. **Feel check** (waves 3 and 4): one headless `witness play batch` shows no new class of problem.

**PASS** needs all seven. A FAIL item comes with the failing command, its output and the check that proves the fix. A
code item that fails twice goes to engineer-high. Each wave gets at most 4 judge rounds.

---

## 10. Decisions for the owner

- **D1. Go per wave.** You can stop after any wave. Recommended: approve waves 0-2 now (no or very low risk), and decide
  on 3-5 after seeing wave 1.
- **D2. Cost of the per-step gates.** Running the full clang CI list and a web build after every step, as asked, costs
  about 60-90 min per step. The alternative runs those two at wave end only. Recommended: keep them per step for waves 0-1,
  then revisit with the measured times.
- **D3. Strict identity.** A move that changes even one pixel or bit (through compiler float reordering) is undone, never
  re-baselined. Recommended: yes.
- **D4. CI change in wave 5.** Tests would run on every branch and pull request; deploy stays main-only, as now.
  Recommended: yes.
- **D5. Optional items** (end of section 6): pick any, or none.
- **D6. Rig-1 retirement stays a separate decision.** Section 11 shows what still depends on it.
- **D7. Shipping.** Use the usual routine (commit, push, CI, Desktop exe) after each approved wave.

---

**Owner rulings, 2026-10-30 ("yes" to every recommendation):**
- D1: waves 0-2 approved; waves 3-5 decided after the owner sees wave 1.
- D2: S4 (clang CI list) and S8 (web build) run once per wave, at the wave gates, not after every step. S1-S3 and S5-S10
  still run after every step.
- D3: strict identity; a step that changes one pixel or bit is undone, never re-baselined.
- D4: wave 5 runs tests on every branch and pull request; deploy stays main-only.
- D5: no optional items for now.
- D7: the usual ship routine after each wave that passes the judge.

---

## 11. Rig-1 retirement map (information only; this plan keeps all of it)

At rig 2, the default, these things still run rig-1 or legacy code. Retirement therefore has to give each one a rig-2
home first:
- **Party portrait colours:** `lookKit` builds a v1 HumanPainter (`art_human.cpp:4809`; `art_party_portrait.cpp:41`).
- **Skeleton, draugr and goblin monsters:** painted by v1 `humanoidCell` (`art_monsters.cpp:2290-2292`), because
  `humanoidCell2` and `humanoidCellH2` are empty stubs (`art_rig.cpp:66-68`).
- **Statues:** `humanFigureStill` (`art_human.cpp:4978`) uses v1.
- **Cache keys:** `HumanLook::key()` and `metalRampKeys` live in `art_human.cpp`, and both rigs' cache keys use them.
- **The SwingArc FX:** `rig2::steelArc` in `rpg/art/rig/rig_body.cpp:406`, called from `art_fx.cpp:287/467` through
  `rig_internal.h`. Deleting `rpg/art/rig/` would break this FX.
- **Rig switches:** 57 `rigVersion()` branches in `rpg/art/*.cpp`, 15 in `render.cpp`, 7 in `game.cpp`, and the
  EMB_RIG/rig handling in `main.cpp`.
