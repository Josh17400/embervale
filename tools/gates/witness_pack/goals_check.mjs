#!/usr/bin/env node
// tools/gates/witness_pack/goals_check.mjs (P3 C1): a playtest goals file parses and every `call <name>` its goals name is a
// command the game runs (a registered script command from `embervale.exe --script-help`, which prints and exits before any
// window exists, or a built-in of rpg/app/script_runner.cpp / witness_driver.cpp). Goal `params` must be an object.
//   node tools/gates/witness_pack/goals_check.mjs --bdir build-solid [tools/scripts/p2c_playtests.txt]
// One PASS/FAIL line per check; exit 1 on any FAIL.
import fs from "node:fs";
import path from "node:path";
import { execFileSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const REPO = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../../..");
const args = process.argv.slice(2);
const bi = args.indexOf("--bdir");
const bdir = bi >= 0 ? args.splice(bi, 2)[1] : "build";
const file = path.resolve(REPO, args[0] || "tools/scripts/p2c_playtests.txt");
let fails = 0;
const line = (ok, what) => { console.log(`${ok ? "PASS" : "FAIL"} ${what}`); if (!ok) fails++; };

let goals = null;
try { goals = JSON.parse(fs.readFileSync(file, "utf8")); line(Array.isArray(goals), `${path.basename(file)} parses (${Array.isArray(goals) ? goals.length : 0} goals)`); }
catch (e) { line(false, `${path.basename(file)} parses: ${e.message}`); process.exit(1); }

const help = execFileSync(path.join(REPO, bdir, "embervale.exe"), ["--script-help"], { encoding: "utf8", windowsHide: true, timeout: 30000 });
const known = new Set(help.split("\n").map((l) => l.trim().split(/\s+/)[0]).filter((w) => w && !w.includes(":") && !w.startsWith("script")));
for (const f of ["rpg/app/script_runner.cpp", "rpg/app/witness_driver.cpp"])
  for (const m of fs.readFileSync(path.join(REPO, f), "utf8").matchAll(/(?:op|c\.a\[0\]) == "([a-z0-9_]+)"/g)) known.add(m[1]);

for (const g of goals) {
  const calls = [...String(g.goal || "").matchAll(/'call ([a-z0-9_]+)/gi)].map((m) => m[1].toLowerCase());
  const bad = [...new Set(calls.filter((c) => !known.has(c)))];
  line(bad.length === 0, `${g.name}: ${calls.length} calls named, ${bad.length ? "not registered: " + bad.join(" ") : "all registered"}`);
  if (g.params !== undefined) line(typeof g.params === "object" && g.params && !Array.isArray(g.params), `${g.name}: params ${JSON.stringify(g.params)}`);
}
const t7 = goals.filter((g) => /^t7/.test(g.name));
line(t7.length > 0 && t7.every((g) => [1, 2, 3, 4, 5, 6].every((k) => g.goal.includes(`lt${k}`)) && g.goal.includes("'call ladderkit lt1'")), "T7 goals name 'call ladderkit lt1' .. lt6");
line(goals.some((g) => /^t5/.test(g.name) && g.params && g.params.female === true), "a T5 goal starts as the woman (params female: true)");
line(goals.filter((g) => /^t5/.test(g.name)).every((g) => /LEATHER/.test(g.goal) && /LAND'S METAL/.test(g.goal) && /gotoculture/.test(g.goal)), "T5 goals climb the ladder (leather, the land's metal, then the next metal elsewhere)");
console.log(`goals_check: ${fails ? "FAIL" : "PASS"} (${fails} failed)`);
process.exit(fails ? 1 : 0);
