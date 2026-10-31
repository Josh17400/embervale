#!/usr/bin/env node
// tools/gates/perf_frame.mjs - the frame-time line of perf_pack.sh, measured on ONE hidden `embervale.exe --witness`
// instance (Witness src/core/launch.js openGame: hidden window, no audio device, fixed-dt steps). It never opens a
// visible window: EMB_PERF / --perf only time the windowed loop, which the gates must not start (wave 0 rule).
//
//   node tools/gates/perf_frame.mjs --bdir DIR [--frames N] [--every K]
//
// The run follows tools/scripts/endless_walk.txt's opening (seed 7, newgame, hour 11, god 1, then a walk east, south
// and west across many window shifts, holding a key 6 s each), stepping ONE frame per Witness `step` request and timing
// it, and every --every frames (default 15) a `screenshot` request (main.cpp h.screenshot: View::draw of the frame,
// then the capture). Prints, in milliseconds of wall time per request (IPC included, so these are not the windowed
// loop's numbers; they are for comparing builds on this machine):
//   frame.step_med <ms>        median of one sim step of the whole game (Game::update + the view's per-step work)
//   frame.shot_med <ms>        median of one drawn frame + capture
// (medians, not means or p95: wall time per IPC request on a shared machine has long one-off tails; A/A at wave 0
// put the means and p95 outside noise + 3 % between back-to-back runs, the medians inside)
//   frame.step_worst <ms>      (information only)
// Exit 0 when the run finished; 2 when the game could not be driven.
import path from "node:path";
import fs from "node:fs";
import os from "node:os";
import { fileURLToPath, pathToFileURL } from "node:url";

delete process.env.WITNESS_WATCH;   // (before any launch: Witness passes its environment on, and WITNESS_WATCH=1 shows the game window)
const REPO = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const WITNESS = process.env.WITNESS_DIR || path.resolve(REPO, "../witness");
const { openGame } = await import(pathToFileURL(path.join(WITNESS, "src/core/launch.js")).href);

const argv = process.argv.slice(2);
const opt = (k, d) => { const i = argv.indexOf(k); return i >= 0 && i + 1 < argv.length ? argv[i + 1] : d; };
const BDIR = path.resolve(REPO, opt("--bdir", "build"));
const FRAMES = +opt("--frames", 1080);   // 3 legs of 6 s
const EVERY = +opt("--every", 15);
const EXE = path.join(BDIR, "embervale.exe");
if (!fs.existsSync(EXE)) { console.log(`perf_frame FAIL: ${EXE} not found`); process.exit(2); }
process.env.WITNESS_MAX_PROCS ||= "5";
process.env.WITNESS_MAX_BROWSERS ||= "1";
process.env.WITNESS_BUDGET_WAIT ||= "1800";   // wait for a Witness slot as long as tools/slot.sh would (peers' instances)

const cwd = fs.mkdtempSync(path.join(os.tmpdir(), "emb_perf_frame."));
const args = ["--witness"];   // the ONLY way the game runs here: hidden window, no audio device
if (!args.includes("--witness") || args.some((a) => /^--(script|shot|play)$/.test(a))) process.exit(2);
let handle;
const now = () => Number(process.hrtime.bigint()) / 1e6;
const DT = 1 / 60;
try {
  handle = await openGame({ name: "embervale", timeout: 3600, launch: { command: EXE.replace(/\\/g, "/"), args, cwd: cwd.replace(/\\/g, "/"), callTimeout: 900 } },
    REPO, { quiet: true });
  const s = handle.session;
  await s.reset(7, null, { window: [1440, 810] });
  await s.step(18, DT);
  await s.call("newgame", []);
  await s.step(30, DT);
  await s.call("hour", ["11"]);
  await s.call("god", ["1"]);
  await s.step(30, DT);
  await s.screenshot();   // the first draw bakes what the view needs; not timed
  const where = async () => { const r = await s.snapshot(); const st = (r && (r.state || r)) || {}; const p = st.player || {}; return p.global || p.px || null; };
  const from = await where();
  const steps = [], shots = [];
  const legs = ["D", "S", "A"];
  const perLeg = Math.ceil(FRAMES / legs.length);
  for (const key of legs) {
    await s.call("hold", [key, "6"]);
    for (let f = 0; f < perLeg; f++) {
      let t = now(); await s.step(1, DT); steps.push(now() - t);
      if (f % EVERY === EVERY - 1) { t = now(); await s.screenshot(); shots.push(now() - t); }
    }
  }
  const to = await where();
  const moved = from && to ? Math.hypot(to[0] - from[0], to[1] - from[1]) : -1;
  if (!(moved > 5)) throw new Error(`the hero did not walk (moved ${moved}): the run measured an idle game`);
  const avg = (a) => a.reduce((x, y) => x + y, 0) / a.length;
  const pct = (a, p) => { const b = [...a].sort((x, y) => x - y); return b[Math.min(b.length - 1, Math.floor(p * b.length))]; };
  console.log(`frame.step_med ${pct(steps, 0.5).toFixed(3)}`);
  console.log(`frame.shot_med ${pct(shots, 0.5).toFixed(3)}`);
  console.log(`frame.step_worst ${Math.max(...steps).toFixed(3)}`);
  console.log(`perf_frame: ${steps.length} steps, ${shots.length} drawn frames on a hidden --witness instance, hero walked ${moved.toFixed(1)}`);
} catch (e) {
  console.log(`perf_frame FAIL: ${String((e && e.message) || e).split("\n")[0]}`);
  process.exitCode = 2;
} finally {
  if (handle) { try { await handle.stop(); } catch { /* gone */ } }
  fs.rmSync(cwd, { recursive: true, force: true });
}
