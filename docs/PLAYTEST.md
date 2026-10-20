# Playtesting Embervale with Witness

Witness lives at `../witness` (0.7.3). Run it from the repo root as `node ../witness/bin/witness.js <command>`, always
wrapped in `tools/slot.sh` (the machine-wide cap of 5 heavy jobs) and always headless (no `--watch`).

## Machine budget (set these in every shell that runs Witness)

```sh
export WITNESS_MAX_PROCS=5      # this PC: at most 5 Witness games/browsers at once (Witness default is 3)
export WITNESS_MAX_BROWSERS=1   # at most 1 headless browser (the web build, tools/witness_web)
```

The user environment already has both; a shell started before they were set does not, so export them anyway. A
launch over the budget prints `waiting for a free slot` and fails after `WITNESS_BUDGET_WAIT` seconds (default 90).
Never kill a game by PID: `play end` / `play stop` stop only the sessions that answer, and a Desktop Embervale.exe
is never Witness's to stop.

## Configs

- `witness.json`: the desktop game (rig 1), `witness.play.json`: the phone window at rig 2 for batches
  (`--config witness.play.json`), `tools/witness_web/witness.json`: the web build (no `play` section).
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

`witness/refs/village_start.png` is the seed-1 start village at t = 2 s (desktop, rig 1). Re-save a ref only on
purpose, when the picture is meant to change.

## Playtests

- One scripted session (no agents): `play start --name t1 [--allow-calls] [--game-hours 6]`, then
  `play do "<orders>" -s t1`, `play look -s t1`, `play end -s t1 --feedback '...'`.
- Agent batches: `play batch --goals FILE --config witness.play.json ...` takes many minutes. Run it in the
  background (Bash `run_in_background`) and wait for the notification. Only one batch runs machine-wide (Witness
  holds a lock), and a batch never runs more testers than `WITNESS_MAX_PROCS`.
- `--minutes` on brief/batch counts GAME minutes; `--wall-minutes` is the real-time stop.
- After runs: `node ../witness/bin/witness.js clean` (applies retention: the newest 5 playtests within 7 days) or
  `clean --all` once the reports have been read. Running playtests are never touched.
