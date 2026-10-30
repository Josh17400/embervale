# Playtesting Embervale with Witness

Witness lives at `../witness` (0.7.11; engine/witness.h is its C++ adapter, re-copied at the P2c flip). Run it from the repo root as `node ../witness/bin/witness.js <command>`, always
wrapped in `tools/slot.sh` (the machine-wide cap of 5 heavy jobs) and always headless (no `--watch`).

## Machine budget (set these in every shell that runs Witness)

```sh
export WITNESS_MAX_PROCS=5      # this PC: at most 5 Witness games/browsers at once (Witness default is 3)
export WITNESS_MAX_BROWSERS=1   # at most 1 headless browser (the web build, tools/witness_web)
```

The user environment already has both; a shell started before they were set does not, so export them anyway. A
launch over the budget prints `waiting for a free slot` (naming each holder, its pid and how long it has held the slot)
and fails after `WITNESS_BUDGET_WAIT` seconds (default 600 for `test`, `doctor` and `play batch`, 90 for the rest;
`play batch` queues for a slot before each tester). A long wait: run the command in the background.
An unknown `--flag` is named on stderr (`unknown flag --sead (ignored): did you mean --seed?`) and the command still
runs; check stderr. `--t` means `--at` (except on `window`).
Never kill a game by PID: `play end` / `play stop` stop only the sessions that answer, and a Desktop Embervale.exe
is never Witness's to stop.

## Configs

- `witness.json`: the desktop game, `witness.play.json`: the phone window for batches (`--config witness.play.json`),
  `tools/witness_web/witness.json`: the web build (no `play` section). Since the P2c flip both run rig 2, the doll, by
  default; rig 1 (the v1 people, kept until the owner's retirement OK) is opt-in only: reset param `"rig": 1` or
  EMB_WPLAY_RIG=1 in a launch env.
- `witness.play.json` sets `play.lazyDraw` (0.7.11): playtest steps tell the game only their last frame is seen, and
  a watch-mode step skips drawing the others (`witness_drawing()`, rpg/main.cpp). Doctor checked it before it was
  turned on: `witness doctor --config witness.play.json --params '{"seed":7}'` prints `lazy drawing safe`; rerun that
  after any change that lets drawing touch the game's state.
- `witness.play.json` sets EMB_WPLAY_SHOT `world:fit`: a tester's shot is the world layer at the largest K (1-4) that
  keeps it within 1568 px wide and 1.15 MP (larger shots are scaled down before the model sees them). On the phone
  window that is K 3 at NORMAL (1536 x 708, as `world:3` was) and K 4 at CLOSE (1464 x 676), so `call zoom close`
  is how a goal asks for a closer look. `world:K` still forces K.
- A call that stays busy (`state.script.busy`: a walkto, or a setup call still travelling) is stopped with
  `call cancel` (or `call stop`); the harness's own stop ends the whole session. `call war goto soldiers` takes a
  tester to a road patrol of a realm not shown yet (twice: two realms), at peace or at war.
- `play.clock` is `state.day*24 + state.hour` (in-game hours, keeps rising across midnight), so `--game-hours N`
  plays until N in-game hours have passed (`--until "<expr>"` takes any condition over the snapshot).
  Travel (`call goto ...`, fast travel, sleeping) moves the clock by hours at once.
- `play.stuckDead` is 20 game seconds: dead that long while presses change nothing ends the session (`stuck dead`).
  Embervale respawns on confirm, so 20 s is plenty.
- Door things carry the push that uses them: building doors `enter: "n"` (you walk north onto the door tile),
  the way out of a building or dungeon (`Exit`) `enter: "s"` (you walk south onto it; E does nothing).

## Baselines: witness/refs

Reference pictures for `shot --diff` live in `witness/refs/<name>.png` and are committed (`.gitignore` has an
exception for them; `witness/shots` is scratch output and stays ignored). Save and compare:

```sh
tools/slot.sh node ../witness/bin/witness.js shot --at 2 --save-ref village_start
tools/slot.sh node ../witness/bin/witness.js shot --at 2 --diff village_start --expect "ssim >= 0.999"
```

`witness/refs/village_start.png` is the seed-1 start village at t = 2 s (desktop, rig 2, the default since the flip). Re-save a ref only on
purpose, when the picture is meant to change.

## Playtests

- One scripted session (no agents): `play start --name t1 [--allow-calls] [--game-hours 6]`, then
  `play do "<orders>" -s t1`, `play look -s t1`, `play end -s t1 --feedback '...'`.
- Agent batches: `play batch --goals FILE --config witness.play.json ...` takes many minutes. Run it in the
  background (Bash `run_in_background`) and wait for the notification. Only one batch runs machine-wide (Witness
  holds a lock), and a batch never runs more testers than `WITNESS_MAX_PROCS`.
- `--minutes` on brief/batch counts GAME minutes; `--wall-minutes` is the real-time stop.
- After runs: `node ../witness/bin/witness.js clean --dry-run` (lists what would go), then `clean` (applies retention:
  the newest 5 playtests within 7 days; a playtest written in the last 24 hours always stays, a judge may cite it) or
  `clean --all` once the reports have been read. Running playtests are never touched.
