#!/usr/bin/env node
// tools/gates/witness_pack.mjs - Embervale's game scripts through Witness, on a pool of HIDDEN `--witness` instances.
// (docs/SOLID_PLAN.md wave 0: the view pack, and W2's "every script, 0 failed" without one visible window.)
//
//   node tools/gates/witness_pack.mjs --bdir DIR (--quick | --all | --list FILE | <script names or paths>...)
//        --out DIR [--check BASELINE_HASHES] [--exclude FILE] [--jobs N<=5] [--reuse [--fresh FILE]] [--keep]
//        [--draw-every FILE] [--xfail FILE] [--scripted-list FILE] [--recycle N] [--no-slot]
//
// NO VISIBLE WINDOW, EVER: every game process this tool starts is `embervale.exe --witness [--script F]` (the hidden
// 960 x 540 window, no audio device, rpg/main.cpp "Witness"). It never runs a plain `--script` / `--shot` / `--play`.
//
//   --quick        the scripts in tools/gates/view_pack_quick.txt (about 40)
//   --all          every script tools/run_scripts.sh would run (tools/scripts/*.txt except *_playtests.txt)
//   --list FILE    the script names in FILE (one per line, "#" comments)
//   --jobs N       instances at once (default 5, max 5; each holds a tools/slot.sh slot for the run, --no-slot skips)
//   Locks (one order everywhere): a launch FIRST takes a tools/slot.sh slot (a holder process `slot.sh bash -c "cat"`
//                  that keeps it until the instance stops), THEN tries the Witness machine budget WITHOUT waiting
//                  (WITNESS_BUDGET_WAIT=0 inside this process): a full budget releases the slot.sh slot, backs off 1-4 s
//                  and starts over, so nothing ever waits on one lock while holding the other (EMB_SLOT_WAIT /
//                  the caller's WITNESS_BUDGET_WAIT, default 3600 s, bound the whole wait).
//   INFRA          a launch error before a script's first line (slot.sh timeout, a full budget past the wait, EPERM /
//                  EBUSY / EACCES on budget files, the game exiting before hello) is retried up to 2 times on a fresh
//                  instance; still failing, the script is INFRA (not FAIL), left out of --check, exit code 3
//   (default)      every script on a NEWLY LAUNCHED hidden instance (launch ~0.9 s, about the cost of a reset): no state
//                  of one script can reach the next. At 119de74 a reused instance leaked state across a Witness reset
//                  (the paperdoll's figures, town art, night light, a hidden land's trial): see witness_pack/fresh.txt
//   --reuse        one instance per worker, reset between scripts (debugging only; not A/A-stable at 119de74), except
//                  the scripts in --fresh FILE (default tools/gates/witness_pack/fresh.txt: "<script>  # why")
//   --check FILE   compare with a recorded hashes.txt (only the scripts this run covered, minus --exclude lines
//                  "<script>[/<png>]  # cause", default exclude.txt next to FILE); a script that PASSes in the
//                  record's scripts.txt must PASS now. Exit 1 and name the first differing PNG.
//   --keep         keep the PNGs under <out>/runs/<script>/ (default: hashed, then deleted)
//   --draw-every F scripts drawn EVERY frame (default tools/gates/witness_pack/draw_every.txt, "<script>  # why")
//   --xfail FILE   scripts whose failure is known at the baseline and not the gate's to fix (default
//                  tools/gates/witness_pack/xfail.txt): a failure is written XFAIL, not FAIL
//   --v1-known F   rig-2 v1 frames recorded at the baseline (default tools/gates/witness_pack/v1_known.txt, "<script> <n>
//                  # why"): the summary's "v1draws rig2: N scripts" counts as run_scripts.sh does (a `rig 1` script is
//                  not a stray) and leaves out a listed script while it draws at most <n> such frames (same as baseline)
//   --scripted-list F  scripts that ALWAYS run on a fresh hidden `--witness --script` instance (the game's own script
//                  engine) because the pool replay cannot reproduce them but the game's engine passes them (default
//                  tools/gates/witness_pack/scripted.txt, "<script>  # why"); they are enforced like any other script
//   --scripted     (debugging) every script on a fresh hidden `--witness --script` instance (the game's own engine)
//   --recycle N    with --reuse: a pool instance is relaunched after N scripts (default 25)
//
// How a script runs. Pool (fresh or --reuse): Witness `reset` with the script's `seed` and its screen (params window
// [1440, 810], the desktop default; a `phone` header: window [2556, 1179], screen fill, safe [177, 0, 177, 63]). A reset
// is a brand-new Game, View and mixer in play (main.cpp h.reset -> applyStart). The script's lines (parsed as
// main.cpp loadScript does: comments, `+` relative times, float32 times; lines after `quit` dropped) are replayed
// against a float32 mirror of script time (+1/60 per step, frozen while state.script.busy, as scriptStep freezes it
// during a walkto / talkto / waiting command). Each due line is one Witness `call` (runCmd: every built-in and
// registered command); between lines the instance is stepped in one `step` request. A `shot` line is taken after one
// more step (the scripted loop writes its pendingShot after that frame's sim and draw).
// Drawn frames. The scripted loop draws every frame, and a line reads what the last frame drew: an `expect` (menu
// figures, item cards, plates; counts View::update rolls over from the frame before), a key or a tap (the hit rects
// and cursors the draw lays out). A Witness instance draws only for a picture, so (`shotmatch` draws and reads the
// canvas, writes no file; ~50 ms): before a line batch the instance draws one frame after the last step; before a
// batch with an `expect` it also steps to one frame short, draws, then steps the last one. Sim time is untouched and
// a `shot`-only batch draws nothing extra. A check that counts work done a little per drawn frame (the world map's
// chunk fetches and plans) needs every frame drawn: such scripts are listed in draw_every.txt.
// Scripted (a script that does anything before its first `newgame`, i.e. starts on the title world; and the --fresh
// list when it is such a script): a fresh `embervale.exe --witness --script <file>` (still hidden): the game's own
// script engine runs the lines inside each step; after a step that ran a line, one `call log` writes a pending shot and
// reports busy. The same draws are made before the step that runs a line batch.
// Not runnable under Witness (SKIP, never FAIL, reason in scripts.txt; reported as a Witness gap): a `blur` line (the
// hidden window never loses the focus: main.cpp fails it) and `expect mode creator` (under Witness New Game on the title
// skips the creator: h.step wantNewGame -> startNew). Nothing falls back to a visible window.
//
// Output in --out: hashes.txt "<script>/<png> <sha256>" (sorted, byte order); scripts.txt "<script> PASS|FAIL|XFAIL|SKIP
// <secs>s <first failure>" (sorted); summary.txt (counts, timings, launches, v1draws rig2 = the sum of
// state.screen.v1DrawsRun over pool runs, drawn frames only); logs/<script>.log. Stdout: one line per script, then the
// summary, then (with --check) "witness_pack PASS|FAIL: ...". Exit 0 when no script FAILed (and the check matched).
// WITNESS_WATCH is deleted from the environment before any launch (Witness passes its environment to the game, and
// WITNESS_WATCH=1 would show the --witness window: rpg/main.cpp). No visible window, whatever the caller's shell holds.
// Environment: WITNESS_HOME (default ../witness), WITNESS_MAX_PROCS (default 5), WITNESS_MAX_BROWSERS (default 1).
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import crypto from "node:crypto";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawn } from "node:child_process";

delete process.env.WITNESS_WATCH;   // (before any launch: a set WITNESS_WATCH shows the game window)
const REPO = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const WITNESS = process.env.WITNESS_HOME || path.resolve(REPO, "../witness");
if (!process.env.WITNESS_MAX_PROCS) process.env.WITNESS_MAX_PROCS = "5";
if (!process.env.WITNESS_MAX_BROWSERS) process.env.WITNESS_MAX_BROWSERS = "1";
// the whole wait for a launch (slot.sh + budget); the budget itself is only ever TRIED (WITNESS_BUDGET_WAIT=0 here)
const WAIT_S = Math.max(1, +(process.env.WITNESS_BUDGET_WAIT || process.env.EMB_SLOT_WAIT || 3600) || 3600);
process.env.WITNESS_BUDGET_WAIT = "0";
const { openGame } = await import(pathToFileURL(path.join(WITNESS, "src/core/launch.js")).href);

// ---- arguments
let mode = "", bdir = process.env.BDIR || "build", out = "", jobs = 5, check = "", excl = "", keep = false, listFile = "";
let recycle = 25, xfailFile = path.join(REPO, "tools/gates/witness_pack/xfail.txt");
let forceScripted = false, useSlot = true, freshAll = true, freshFile = path.join(REPO, "tools/gates/witness_pack/fresh.txt");
let everyFile = path.join(REPO, "tools/gates/witness_pack/draw_every.txt");
let scriptedFile = path.join(REPO, "tools/gates/witness_pack/scripted.txt");
let v1KnownFile = path.join(REPO, "tools/gates/witness_pack/v1_known.txt");
const names = [];
const argv = process.argv.slice(2);
for (let k = 0; k < argv.length; k++) {
  const a = argv[k];
  if (a === "--quick") mode = "quick";
  else if (a === "--all") mode = "all";
  else if (a === "--list") { mode = "list"; listFile = argv[++k]; }
  else if (a === "--bdir") bdir = argv[++k];
  else if (a === "--out") out = argv[++k];
  else if (a === "--jobs") jobs = Math.min(5, Math.max(1, +argv[++k] || 1));
  else if (a === "--check") check = argv[++k];
  else if (a === "--exclude") excl = argv[++k];
  else if (a === "--keep") keep = true;
  else if (a === "--no-slot") useSlot = false;
  else if (a === "--fresh") freshFile = argv[++k];
  else if (a === "--fresh-all") freshAll = true;
  else if (a === "--reuse") freshAll = false;
  else if (a === "--scripted") forceScripted = true;
  else if (a === "--scripted-list") scriptedFile = argv[++k];
  else if (a === "--v1-known") v1KnownFile = argv[++k];
  else if (a === "--draw-every") everyFile = argv[++k];
  else if (a === "--recycle") recycle = Math.max(1, +argv[++k] || 25);
  else if (a === "--xfail") xfailFile = argv[++k];
  else if (a === "-h" || a === "--help") {
    const src = fs.readFileSync(fileURLToPath(import.meta.url), "utf8").split("\n");
    console.log(src.slice(1, 53).map((l) => l.replace(/^\/\/ ?/, "")).join("\n"));
    process.exit(0);
  } else if (a.startsWith("--")) { console.log(`witness_pack FAIL: unknown option ${a}`); process.exit(2); }
  else { mode = mode || "names"; names.push(a); }
}
if (!mode) mode = "quick";
const abs = (p) => (path.isAbsolute(p) ? p : path.resolve(REPO, p));
const EXE = abs(path.join(bdir, "embervale.exe"));
if (!fs.existsSync(EXE)) { console.log(`witness_pack FAIL: ${EXE} not found (build it first)`); process.exit(2); }
out = out ? abs(out) : fs.mkdtempSync(path.join(os.tmpdir(), "emb_wpack."));
fs.rmSync(path.join(out, "inst"), { recursive: true, force: true });
fs.rmSync(path.join(out, "runs"), { recursive: true, force: true });
fs.rmSync(path.join(out, "logs"), { recursive: true, force: true });
fs.mkdirSync(path.join(out, "logs"), { recursive: true });
fs.mkdirSync(path.join(out, "inst"), { recursive: true });
const BASH = process.env.EMB_BASH || ["C:\\Program Files\\Git\\usr\\bin\\bash.exe", "C:\\Program Files\\Git\\bin\\bash.exe"].find((p) => fs.existsSync(p)) || "bash";
const readNames = (f) => fs.readFileSync(f, "utf8").split(/\r?\n/).map((l) => l.replace(/#.*/, "").trim().split(/\s+/)[0]).filter(Boolean);

// ---- the selection
const sdir = path.join(REPO, "tools/scripts");
let files = [];
const byName = (n) => {
  if (fs.existsSync(abs(n)) && fs.statSync(abs(n)).isFile()) return abs(n);
  const f = path.join(sdir, n.replace(/\.txt$/, "") + ".txt");
  if (fs.existsSync(f)) return f;
  console.log(`witness_pack FAIL: no script ${n}`); process.exit(2);
};
if (mode === "quick") files = readNames(path.join(REPO, "tools/gates/view_pack_quick.txt")).map(byName);
else if (mode === "list") files = readNames(abs(listFile)).map(byName);
else if (mode === "names") files = names.map(byName);
else files = fs.readdirSync(sdir).filter((f) => f.endsWith(".txt") && !f.endsWith("_playtests.txt")).sort().map((f) => path.join(sdir, f));
if (!files.length) { console.log("witness_pack FAIL: no scripts"); process.exit(2); }
const fresh = new Set(fs.existsSync(abs(freshFile)) ? readNames(abs(freshFile)) : []);
const xfail = new Set(fs.existsSync(abs(xfailFile)) ? readNames(abs(xfailFile)) : []);
const scriptedList = new Set(fs.existsSync(abs(scriptedFile)) ? readNames(abs(scriptedFile)) : []);
const drawEvery = new Set(fs.existsSync(abs(everyFile)) ? readNames(abs(everyFile)) : []);

// ---- main.cpp loadScript, line for line (float32 times; the first `seed` wins; `phone`; `worldgen` is refused)
const F = Math.fround;
const NUM_RE = /^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$/;
function parseScript(file) {
  const sc = { name: path.basename(file, ".txt"), file, seed: 0, phone: false, cmds: [], refuse: "", skip: "", titleStart: "" };
  let last = F(0), lineNo = 0;
  for (let ln of fs.readFileSync(file, "latin1").split("\n")) {
    lineNo++;
    const h = ln.indexOf("#");
    if (h >= 0) ln = ln.slice(0, h);
    const tok = ln.split(/[ \t\r]+/).filter(Boolean);
    if (!tok.length) continue;
    if (tok[0] === "seed" && tok.length >= 2) { if (!sc.seed) sc.seed = parseInt(tok[1], 10) || 0; continue; }
    if (tok[0] === "phone") { sc.phone = true; continue; }
    if (tok[0] === "worldgen") { sc.refuse = `line ${lineNo}: 'worldgen' is retired`; continue; }
    const rel = tok[0][0] === "+";
    const ts = tok[0].slice(rel ? 1 : 0);
    if (!NUM_RE.test(ts) || tok.length < 2) continue;   // (loadScript prints it and skips the line)
    const t = F(parseFloat(ts));
    const ct = rel ? F(last + t) : t;
    last = ct;
    const a = tok.slice(1);
    a[0] = a[0].toLowerCase();
    if (!sc.skip && a[0] === "expect" && (a[1] || "").toLowerCase() === "mode" && (a[2] || "").toLowerCase() === "creator")
      sc.skip = `line ${lineNo} expect mode creator: under Witness New Game on the title skips the creator (main.cpp h.step wantNewGame -> startNew)`;
    if (!sc.skip && a[0] === "blur") sc.skip = `line ${lineNo} blur: a Witness instance's hidden window never loses the focus (main.cpp fails 'blur' under Witness)`;
    // (the run's working directory is its instance folder, not the repo root as for run_scripts.sh: a repo-relative
    // file argument is made absolute)
    if (a[0] === "loadsave" && a[1] && !path.isAbsolute(a[1])) { a[1] = path.resolve(REPO, a[1]).split(path.sep).join("/"); sc.rewritten = true; }
    sc.cmds.push({ t: ct, a, line: lineNo });
  }
  const q = sc.cmds.findIndex((c) => c.a[0] === "quit");   // (no line after a quit runs: the script loop stops)
  if (q >= 0) sc.cmds.length = q + 1;
  // a pool reset is a world in play (applyStart): a script that does anything before its first `newgame` needs the
  // title world a --script run starts on
  if (forceScripted && sc.cmds.length) sc.titleStart = "--scripted";
  else if (scriptedList.has(sc.name) && sc.cmds.length) sc.titleStart = `listed in ${path.basename(scriptedFile)}`;
  else if (!sc.cmds.length || sc.cmds[0].a[0] !== "newgame") sc.titleStart = sc.cmds.length ? `line ${sc.cmds[0].line} ${sc.cmds[0].a.join(" ")}` : "no lines";
  return sc;
}

// ---- launches: always --witness (hidden). Lock order: tools/slot.sh slot first, then a no-wait try of the Witness budget
class Infra extends Error {}
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const SLOT_SH = path.join(REPO, "tools/slot.sh").replace(/\\/g, "/");
// a tools/slot.sh slot held by a child `slot.sh bash -c 'echo held; exec cat'` until release() closes its stdin (or
// this process dies: the pipe closes, cat ends, slot.sh's EXIT trap frees the slot)
function takeSlot(label) {
  return new Promise((resolve, reject) => {
    const p = spawn(BASH, [SLOT_SH, BASH.replace(/\\/g, "/"), "-c", "echo wpack-slot-held; exec cat > /dev/null", label],
      { stdio: ["pipe", "pipe", "pipe"], windowsHide: true, env: { ...process.env, EMB_SLOT_WAIT: String(WAIT_S) } });
    let outb = "", errb = "", held = false;
    const gone = new Promise((r) => p.once("exit", r));
    const release = async () => { try { p.stdin.end(); } catch { /* closed */ } await Promise.race([gone, sleep(5000)]); if (p.exitCode === null) p.kill(); };
    p.stdout.on("data", (d) => { outb += d; if (!held && outb.includes("wpack-slot-held")) { held = true; resolve({ release }); } });
    p.stderr.on("data", (d) => { errb += d; });
    p.on("error", (e) => { if (!held) reject(new Infra(`slot.sh: ${e.message}`)); });
    p.on("exit", (code) => { if (!held) reject(new Infra(`slot.sh exited ${code}${errb.trim() ? ": " + errb.trim().split("\n").pop() : ""}`)); });
  });
}
const TRANSIENT = /EPERM|EBUSY|EACCES|ENOENT.*witness-budget/;
const launches = [];
async function launch(id, extra = [], label = "") {
  const exeArgs = ["--witness", ...extra].map((a) => a.replace(/\\/g, "/"));
  if (exeArgs[0] !== "--witness") throw new Error("refusing a launch without --witness");
  const cwd = path.join(out, "inst", String(id));
  const cfg = { name: "embervale", timeout: 120, launch: { command: EXE, args: exeArgs, cwd, callTimeout: 900 } };
  const deadline = Date.now() + WAIT_S * 1000;
  let transient = 0;
  for (;;) {
    fs.rmSync(cwd, { recursive: true, force: true });
    fs.mkdirSync(cwd, { recursive: true });
    const slot = useSlot ? await takeSlot(`wpack:${label || id}`) : null;   // (Infra on a slot.sh timeout)
    const t0 = Date.now();
    try {
      const handle = await openGame(cfg, REPO, { quiet: true });
      launches.push((Date.now() - t0) / 1000);
      return { id, cwd, handle, slot, s: handle.session, scripts: 0 };
    } catch (e) {
      if (slot) await slot.release();
      const m = errText(e);
      if (/no free slot|already holds every slot/.test(m) && Date.now() < deadline) { await sleep(1000 + Math.random() * 3000); continue; }   // (budget full: back off holding nothing)
      if (TRANSIENT.test(m) && ++transient <= 3) { await sleep(200 + Math.random() * 800); continue; }   // (a budget file race)
      throw new Infra(m.split("\n")[0].slice(0, 200));
    }
  }
}
// a launch retried up to 2 times on a fresh instance; the last Infra error is thrown
async function launchRetry(id, extra, label, log) {
  for (let k = 0; ; k++) {
    try { return await launch(id, extra, label); }
    catch (e) { if (!(e instanceof Infra) || k >= 2) throw e; log.push(`note: launch attempt ${k + 1} failed (${e.message}): retrying on a fresh instance`); }
  }
}
// EMB_V1TRACE=1 in the environment: the game prints "v1trace t <s> mode <m> n <k>" per drawn frame with v1 people frames
// at rig 2 (hud.cpp); those lines go into the script's log
const traceV1 = (inst, log) => { if (process.env.EMB_V1TRACE) for (const l of inst.handle.output().split(/\r?\n/)) if (l.startsWith("v1trace")) log.push(`note: ${l}`); };
async function stop(inst) { if (inst) { try { await inst.handle.stop(); } catch { /* gone */ } if (inst.slot) await inst.slot.release(); } }

function walk(dir, rel = "", acc = []) {
  if (!fs.existsSync(dir)) return acc;
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const r = rel ? rel + "/" + e.name : e.name;
    if (e.isDirectory()) walk(path.join(dir, e.name), r, acc);
    else acc.push(r);
  }
  return acc;
}
function harvest(name, dir) {   // hash every PNG the script wrote into the instance's working directory, then empty it
  const lines = [];
  const all = walk(dir);
  for (const p of all.filter((p) => p.toLowerCase().endsWith(".png")).sort())
    lines.push(`${name}/${p} ${crypto.createHash("sha256").update(fs.readFileSync(path.join(dir, p))).digest("hex")}`);
  if (keep && all.length) {
    const dst = path.join(out, "runs", name);
    fs.rmSync(dst, { recursive: true, force: true });
    fs.mkdirSync(path.dirname(dst), { recursive: true });
    fs.cpSync(dir, dst, { recursive: true });
  }
  if (fs.existsSync(dir)) for (const e of fs.readdirSync(dir)) fs.rmSync(path.join(dir, e), { recursive: true, force: true });
  return lines;
}

const DT = 1 / 60, FDT = F(DT), MAX_FRAMES = 60 * 1200;
const errText = (e) => String((e && e.message) || e).replace(/^game error on 'call': /, "");
const stepsTo = (from, to) => { let k = 0, t = from; while (t < to && k < MAX_FRAMES) { t = F(t + FDT); k++; } return [k, t]; };
// does the batch of lines due at cmds[i].t hold an expect (it reads the last frame drawn)?
const batchExpects = (cmds, i) => { for (let j = i; j < cmds.length && cmds[j].t === cmds[i].t; j++) if (cmds[j].a[0] === "expect") return true; return false; };
// is every line of that batch a `shot` / `quit` (no frame need be drawn before them)?
const batchShots = (cmds, i) => { for (let j = i; j < cmds.length && cmds[j].t === cmds[i].t; j++) if (cmds[j].a[0] !== "shot" && cmds[j].a[0] !== "quit") return false; return true; };
async function drawOne(s) { try { await s.call("shotmatch", ["witness_pack_draw"]); } catch (e) { if (s.closed) throw e; } }   // (a 2nd frame differs: fine)

// ---- one script on a pool instance (reset + calls)
async function runPool(inst, sc, log) {
  const s = inst.s;
  const params = sc.phone ? { window: [2556, 1179], screen: "fill", safe: [177, 0, 177, 63] } : { window: [1440, 810] };
  await s.reset(sc.seed || 1, null, params);
  await s.drainLogs();
  let T = F(0), i = 0, busy = false, frames = 0, drawnAt = -1;
  const n = sc.cmds.length, shots = [];
  const every = drawEvery.has(sc.name);
  const slow = (what, t0) => { const d = (performance.now() - t0) / 1000; if (d >= 2) log.push(`note: slow ${what}: ${d.toFixed(1)} s`); };
  const step = async (k) => {
    if (k <= 0) return;
    const t0 = performance.now();
    if (!every) { await s.step(k, DT); frames += k; }
    else for (let j = 0; j < k; j++) { await drawOne(s); await s.step(1, DT); frames++; }   // (draw_every: a scripted run's frames)
    slow(`step ${k} at frame ${frames}`, t0);
  };
  const call = async (c) => {
    const t0 = performance.now();
    try { return await s.call(c.a[0], c.a.slice(1)); }
    catch (e) { if (s.closed) throw e; log.push(`line ${c.line} ${c.a.join(" ")}: ${errText(e)}`); return null; }
    finally { slow(`line ${c.line} ${c.a.join(" ")}`, t0); }
  };
  while (frames < MAX_FRAMES) {
    if (!busy) {
      while (i < n && sc.cmds[i].t <= T) {
        const c = sc.cmds[i++];
        if (c.a[0] === "quit") { i = n; break; }
        if (c.a[0] === "shot") { shots.push(c); continue; }
        if (drawnAt !== frames && !every) { await drawOne(s); drawnAt = frames; }   // (the frame before this line, drawn after its update)
        const r = await call(c);
        if (r && r.result && r.result.busy) { busy = true; break; }
      }
    }
    if (shots.length && busy) {   // (a shot and then a walkto at one time: the shot now, a frame early)
      log.push(`note: ${shots.length} shot(s) taken before the step (a busy call followed them at the same time)`);
      for (const c of shots.splice(0)) await call(c);
    }
    if (i >= n && !busy && !shots.length) break;
    if (shots.length) {
      await step(1); T = F(T + FDT);
      for (const c of shots.splice(0)) await call(c);
      continue;
    }
    if (busy) {
      await step(1);
      const r = await s.snapshot();
      busy = !!(r.state && r.state.script && r.state.script.busy);
      continue;
    }
    const [k, t] = stepsTo(T, sc.cmds[i].t);
    if (k === 0) continue;
    if (batchExpects(sc.cmds, i) && !every) { await step(k - 1); await drawOne(s); await step(1); }   // (counts View::update rolls over)
    else await step(k);
    if (every || batchShots(sc.cmds, i)) drawnAt = frames;   // (no extra draw: draw_every drew it; a shot draws its own)
    T = t;
  }
  if (frames >= MAX_FRAMES) log.push(`timeout: ${MAX_FRAMES} frames of sim time`);
  const snap = await s.snapshot();
  const sx = (snap.state && snap.state.script) || {};
  const v1 = (snap.state && snap.state.screen && snap.state.screen.v1DrawsRun) || 0;
  for (const l of await s.drainLogs()) if (l.level === "error" || l.level === "warn") log.push(`[${l.level}] ${l.message}`);
  if (sx.fails) log.unshift(`script fails ${sx.fails}: ${sx.lastFail || ""}`);
  return { rc: sx.fails || frames >= MAX_FRAMES ? 3 : 0, frames, v1 };
}

// ---- one script on a fresh `--witness --script <file>` instance (the title world; the game's own script engine)
async function runScripted(inst, sc, log) {
  const s = inst.s;
  let T = F(0), i = 0, busy = false, frames = 0;
  const n = sc.cmds.length;
  const probe = async () => {   // after the step a line ran in (or a busy step): flush a pending shot, read busy
    try { const r = await s.call("log", ["witness_pack"]); return !!(r.result && r.result.busy); }
    catch (e) { if (s.closed) throw e; const m = errText(e); if (/^busy:/.test(m)) return true; log.push(`frame ${frames}: ${m}`); return false; }
  };
  while (frames < MAX_FRAMES) {
    if (!busy) {
      if (i >= n) break;
      const [k, t] = stepsTo(T, sc.cmds[i].t);
      if (k) {
        // the lines run inside the step after these and read the frame drawn after the last one (an expect also reads
        // counts View::update rolled over from the frame before)
        if (batchExpects(sc.cmds, i)) {
          if (k > 1) await s.step(k - 1, DT);
          await drawOne(s);
          await s.step(1, DT);
        } else await s.step(k, DT);
        if (!batchShots(sc.cmds, i)) await drawOne(s);
        frames += k; T = t; continue;
      }
    }
    const wasBusy = busy;
    await s.step(1, DT); frames++;
    busy = await probe();
    if (!wasBusy && !busy) { while (i < n && sc.cmds[i].t <= T) i++; T = F(T + FDT); }
  }
  if (frames >= MAX_FRAMES) log.push(`timeout: ${MAX_FRAMES} frames of sim time`);
  const snap = await s.snapshot();
  const sx = (snap.state && snap.state.script) || {};
  const v1 = (snap.state && snap.state.screen && snap.state.screen.v1DrawsRun) || 0;
  for (const l of await s.drainLogs()) if (l.level === "error" || l.level === "warn") log.push(`[${l.level}] ${l.message}`);
  if (sx.fails) log.unshift(`script fails ${sx.fails}: ${sx.lastFail || ""}`);
  return { rc: sx.fails || frames >= MAX_FRAMES ? 3 : 0, frames, v1 };
}

// ---- the pool
const scripts = files.map(parseScript);
const order = (x) => (x.skip || x.refuse ? 0 : x.titleStart || freshAll || fresh.has(x.name) ? 1 : 2);
const queue = [...scripts].sort((a, b) => order(a) - order(b));   // (the costly fresh launches first: they overlap the rest)
const results = new Map();
// run_scripts.sh's rule (P2c flip, D103): a script that opts into the v1 fallback with a `rig 1` line is not a stray
const RIG1_RE = /^[^#]*[0-9.]+[ \t]+rig[ \t]+1([ \t]|$)/m;
const rig1 = (sc) => RIG1_RE.test(fs.readFileSync(sc.file, "latin1").replace(/\r/g, ""));
const t0 = performance.now();
async function worker(w) {
  let inst = null;
  while (queue.length) {
    const sc = queue.shift();
    const log = [];
    const st = performance.now();
    let rc = 0, lines = [], how = "pool", frames = 0, v1 = 0, infra = "";
    try {
      if (sc.skip) { how = "skip"; log.push(sc.skip); }
      else if (sc.refuse) { rc = 2; log.push(sc.refuse); }
      else if (sc.titleStart) {
        how = "scripted";
        await stop(inst); inst = null;   // (one slot per worker: its pool instance goes first)
        log.push(`note: the game's own script engine (${sc.titleStart}): a fresh hidden --witness --script instance`);
        let file = sc.file;
        if (sc.rewritten) {   // the same script with its repo-relative file arguments made absolute
          file = path.join(out, "scripts", sc.name + ".txt");
          fs.mkdirSync(path.dirname(file), { recursive: true });
          const R = path.resolve(REPO).split(path.sep).join("/");
          const fix = (ln) => {
            const tok = ln.split(/[ \t]+/);
            const k = tok.findIndex((x) => x.toLowerCase() === "loadsave");
            if (k < 0 || !tok[k + 1] || path.isAbsolute(tok[k + 1]) || ln.trimStart().startsWith("#")) return ln;
            tok[k + 1] = R + "/" + tok[k + 1];
            return tok.join(" ");
          };
          fs.writeFileSync(file, fs.readFileSync(sc.file, "latin1").split("\n").map(fix).join("\n"), "latin1");
        }
        const si = await launchRetry(`${w}s`, ["--script", file], sc.name, log);
        try { const r = await runScripted(si, sc, log); rc = r.rc; frames = r.frames; v1 = r.v1; }
        catch (e) { log.unshift(`instance failed: ${errText(e)}`); rc = 255; }
        traceV1(si, log); await stop(si);
        lines = harvest(sc.name, si.cwd);
      } else {
        const isFresh = freshAll || fresh.has(sc.name);
        if (isFresh) { how = "fresh"; await stop(inst); inst = null; }
        if (inst && inst.scripts >= recycle) { await stop(inst); inst = null; }   // (a long-lived instance slows down)
        if (!inst) inst = await launchRetry(w, [], sc.name, log);
        inst.scripts++;
        const dir = inst.cwd;
        try { const r = await runPool(inst, sc, log); rc = r.rc; frames = r.frames; v1 = r.v1; }
        catch (e) { log.unshift(`instance failed: ${errText(e)}`); rc = 255; await stop(inst); inst = null; }
        if (inst) traceV1(inst, log);
        if (isFresh && inst) { await stop(inst); inst = null; }
        lines = harvest(sc.name, dir);
      }
    } catch (e) {
      if (e instanceof Infra) { infra = e.message; log.unshift(`infra: ${e.message} (3 launches)`); }
      else log.unshift(`error: ${errText(e)}`);
      rc = 255;
    }
    if (v1) log.push(`note: v1draws rig2 ${v1}${rig1(sc) ? " (a 'rig 1' script: not a stray)" : ""}`);
    const secs = (performance.now() - st) / 1000;
    const first = (log.find((l) => !l.startsWith("note")) || "").replace(/\s+/g, " ").slice(0, 200);
    const status = how === "skip" ? "SKIP" : infra ? "INFRA" : rc === 0 ? "PASS" : xfail.has(sc.name) ? "XFAIL" : "FAIL";
    results.set(sc.name, { status, lines, secs, how, frames, first, v1, rig1: v1 ? rig1(sc) : false });
    fs.writeFileSync(path.join(out, "logs", sc.name + ".log"), log.join("\n") + "\n");
    console.log(`${status} ${sc.name} (${secs.toFixed(1)} s, ${lines.length} PNGs${how !== "pool" ? ", " + how : ""}${status !== "PASS" ? ": " + first.slice(0, 140) : ""})`);
  }
  await stop(inst);
}
await Promise.all(Array.from({ length: Math.min(jobs, scripts.length) }, (_, w) => worker(w)));
const secs = (performance.now() - t0) / 1000;
fs.rmSync(path.join(out, "inst"), { recursive: true, force: true });

const byte = (a, b) => (a < b ? -1 : a > b ? 1 : 0);
const hashes = [...results.values()].flatMap((r) => r.lines).sort(byte);
fs.writeFileSync(path.join(out, "hashes.txt"), hashes.join("\n") + (hashes.length ? "\n" : ""));
const sl = [...results].map(([n, r]) => `${n} ${r.status} ${r.secs.toFixed(1)}s${r.first && r.status !== "PASS" ? " " + r.first : ""}`).sort(byte);
fs.writeFileSync(path.join(out, "scripts.txt"), sl.join("\n") + "\n");
const rs = [...results.values()];
const cnt = (st) => rs.filter((r) => r.status === st).length;
const ran = rs.filter((r) => r.status !== "SKIP");
const avg = ran.length ? ran.reduce((a, r) => a + r.secs, 0) / ran.length : 0;
// v1_known.txt "<script> <n>  # why": rig-2 v1 frames recorded at the baseline under Witness (not seen by run_scripts.sh's
// visible runs); up to <n> frames is "same as baseline", more is a stray like any other
const v1known = new Map();
if (fs.existsSync(abs(v1KnownFile))) for (const l of fs.readFileSync(abs(v1KnownFile), "utf8").split(/\r?\n/)) { const w = l.replace(/#.*/, "").trim().split(/\s+/); if (w[0]) v1known.set(w[0], +w[1] || 0); }
const isKnown = ([n, r]) => v1known.has(n) && r.v1 <= v1known.get(n);
const v1bad = [...results].filter((e) => e[1].v1 && !e[1].rig1 && !isKnown(e)).sort(), v1opt = [...results].filter(([, r]) => r.v1 && r.rig1).sort();
const v1base = [...results].filter((e) => e[1].v1 && !e[1].rig1 && isKnown(e)).sort();
const avgLaunch = launches.length ? launches.reduce((a, b) => a + b, 0) / launches.length : 0;
const summary = [
  `witness_pack: ${scripts.length} scripts (${mode}), ${cnt("PASS")} PASS, ${cnt("FAIL")} FAIL, ${cnt("XFAIL")} XFAIL, ${cnt("SKIP")} SKIP, ${cnt("INFRA")} INFRA, ${hashes.length} PNGs, ${secs.toFixed(1)} s wall, jobs ${Math.min(jobs, scripts.length)}`,
  `per script: avg ${avg.toFixed(2)} s (pool ${rs.filter((r) => r.how === "pool").length}, scripted ${rs.filter((r) => r.how === "scripted").length}, fresh ${rs.filter((r) => r.how === "fresh").length})`,
  `instance launches: ${launches.length}, ${avgLaunch.toFixed(2)} s each to hello`,
  // as run_scripts.sh counts it: scripts with v1 people frames at rig 2, leaving out the `rig 1` opt-in scripts
  `v1draws rig2: ${v1bad.length} scripts with v1 frames${v1bad.length ? ": " + v1bad.map(([n, r]) => `${n} (${r.v1})`).join(" ") : ""}${v1opt.length ? `; 'rig 1' scripts with rig-2 v1 frames (not strays): ${v1opt.map(([n, r]) => `${n} (${r.v1})`).join(" ")}` : ""}${v1base.length ? `; same as baseline (${path.basename(v1KnownFile)}): ${v1base.map(([n, r]) => `${n} (${r.v1} <= ${v1known.get(n)})`).join(" ")}` : ""}`,
  `exe: ${EXE}`,
  ...rs.filter((r) => r.status === "SKIP").map((r) => r).length ? [`skipped (not runnable under Witness): ${[...results].filter(([, r]) => r.status === "SKIP").map(([n]) => n).join(" ")}`] : [],
  ...(cnt("INFRA") ? [`INFRA (launch failed 3 times, not compared): ${[...results].filter(([, r]) => r.status === "INFRA").map(([n, r]) => `${n}: ${r.first}`).join("; ")}`] : []),
];
fs.writeFileSync(path.join(out, "summary.txt"), summary.join("\n") + "\n");
for (const l of summary.slice(0, 4)) console.log(l);
const nfail = cnt("FAIL"), rcOk = cnt("INFRA") ? 3 : 0;
if (!check) process.exit(nfail ? 1 : rcOk);

// ---- --check
const recFile = abs(check);
if (!fs.existsSync(recFile)) { console.log(`witness_pack FAIL: no record ${recFile}`); process.exit(2); }
const readL = (f) => (fs.existsSync(f) ? fs.readFileSync(f, "utf8").split(/\r?\n/).filter(Boolean) : []);
if (!excl) excl = path.join(path.dirname(recFile), "exclude.txt");
const xs = new Set(), xp = new Set();
for (const l of readL(abs(excl))) { const w = l.replace(/#.*/, "").trim(); if (!w) continue; const k = w.split(/\s+/)[0]; if (k.includes("/")) xp.add(k); else xs.add(k); }
const excluded = (key) => { const s = key.split("/")[0]; return xs.has(s) || xp.has(key); };
const cur = new Map(), rec = new Map();
for (const l of hashes) { const [k, h] = l.split(" "); if (!excluded(k) && results.get(k.split("/")[0]).status !== "INFRA") cur.set(k, h); }
for (const l of readL(recFile)) { const [k, h] = l.split(" "); const s = k.split("/")[0]; if (!results.has(s) || results.get(s).status === "SKIP" || results.get(s).status === "INFRA" || excluded(k)) continue; rec.set(k, h); }
let same = 0;
const diffs = [];
for (const [k, h] of rec) { if (!cur.has(k)) diffs.push(`${k} missing (was ${h.slice(0, 12)})`); else if (cur.get(k) !== h) diffs.push(`${k} pixels differ`); else same++; }
for (const k of cur.keys()) if (!rec.has(k)) diffs.push(`${k} new (not in the record)`);
const recScripts = path.join(path.dirname(recFile), "scripts.txt");
for (const l of readL(recScripts)) {
  const [s, st] = l.split(" ");
  const r = results.get(s);
  if (r && st === "PASS" && r.status === "FAIL" && !xs.has(s)) diffs.push(`${s} script PASS -> FAIL: ${r.first}`);
}
diffs.sort(byte);
fs.writeFileSync(path.join(out, "diff.txt"), diffs.join("\n") + (diffs.length ? "\n" : ""));
const nex = xs.size + xp.size;
if (!diffs.length && same > 0) { console.log(`witness_pack PASS: ${same} PNGs identical to ${check} (${nex} exclusions${cnt("INFRA") ? `, ${cnt("INFRA")} INFRA scripts not compared` : ""}), ${secs.toFixed(0)} s`); process.exit(nfail ? 1 : rcOk); }
console.log(`witness_pack FAIL: first differing PNG ${diffs[0] || "(none compared)"}; ${diffs.length} differing, ${same} identical (${nex} exclusions) - ${path.join(out, "diff.txt")}`);
process.exit(1);
