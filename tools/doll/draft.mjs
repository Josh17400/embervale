#!/usr/bin/env node
// tools/doll/draft.mjs: a mirror-and-relight DRAFT of a part's right views as left views (EMBERVALE P2b, STYLE_P2 9 and 24).
// The left facing is painted, never mirrored at run time, so the light stays top-left: this only blocks it in. Each
// right view is mirrored, its origin and marks mirrored with it, and every lit material relit (the shadow and light steps
// swap: 1 <-> 3, the highlight 4 falls to 1; outlines 0 and bases 2 stay) through the part's own legend. The artist then
// repaints it by hand; pxc.mjs refuses a left view that is still the PLAIN mirror of its right view.
//   node tools/doll/draft.mjs <part.pxm> [--key K] [--from R] [--to L] [--write] [--force]
//     prints the drafted views; --write appends them to the file (only keys whose --to view is missing, unless --force)
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const args = process.argv.slice(2);
if (!args.length || args[0].startsWith('--')) { console.error('draft.mjs <part.pxm> [--key K] [--from R] [--to L] [--write] [--force]'); process.exit(2); }
const file = path.resolve(args[0]);
let onlyKey = null, from = 'R', to = 'L', write = false, force = false;
for (let i = 1; i < args.length; i++) {
  if (args[i] === '--key') onlyKey = args[++i];
  else if (args[i] === '--from') from = args[++i];
  else if (args[i] === '--to') to = args[++i];
  else if (args[i] === '--write') write = true;
  else if (args[i] === '--force') force = true;
  else { console.error(`draft.mjs: unknown argument ${args[i]}`); process.exit(2); }
}
const text = fs.readFileSync(file, 'utf8').replace(/\r\n?/g, '\n');
const lines = text.split('\n');
// the legend: the used legend files (the maps folder above the part) and local entries
let mapsDir = path.dirname(file);
while (mapsDir !== path.dirname(mapsDir) && path.basename(mapsDir) !== 'maps') mapsDir = path.dirname(mapsDir);
const legend = {};
const readLegend = (name) => {
  const f = path.join(mapsDir, 'legend', `${name}.legend`);
  if (!fs.existsSync(f)) { console.error(`draft.mjs: no legend ${f}`); return; }
  for (const ln of fs.readFileSync(f, 'utf8').split(/\r?\n/)) {
    const t = ln.trim().split(/\s+/);
    if (t[0] === 'legend' && t.length === 4) legend[t[1]] = { slot: t[2], step: +t[3] };
  }
};
const views = [];
let cur = null;
for (const ln of lines) {
  const t = ln.trim();
  if (ln.startsWith('|')) { if (cur) cur.rows.push(ln.slice(1).split('|')[0].replace(/ /g, '.')); continue; }
  const toks = t.split(/\s+/);
  if (toks[0] === 'use') toks.slice(1).forEach(readLegend);
  else if (toks[0] === 'legend' && toks.length === 4) legend[toks[1]] = { slot: toks[2], step: +toks[3] };
  else if (toks[0] === 'view') {
    cur = { facing: toks[1], key: 'rest', origin: [0, 0], when: '', extra: [], rows: [], marks: [] };
    for (let k = 2; k < toks.length; k++) {
      if (toks[k] === 'key') cur.key = toks[++k];
      else if (toks[k] === 'origin') { cur.origin = [+toks[k + 1], +toks[k + 2]]; k += 2; }
      else if (toks[k] === 'when') cur.when = toks[++k];
      else cur.extra.push(toks[k], toks[++k]);
    }
    views.push(cur);
  } else if (toks[0] === 'mark' && cur) cur.marks.push({ name: toks[1], x: +toks[2], y: +toks[3] });
  else if (toks[0] === 'regions' || toks[0] === 'build') cur = null;   // (regions and build rows are not drafted)
}
const UNLIT = new Set(['ink', 'face', 'eyes', 'fx']);
const relitStep = { 0: 0, 1: 3, 2: 2, 3: 1, 4: 1 };
const charOf = {};   // slot/step -> the first character the legend gives it
for (const [ch, e] of Object.entries(legend)) { const k = `${e.slot}/${e.step}`; if (!(k in charOf)) charOf[k] = ch; }
const relight = (ch) => {
  const e = legend[ch];
  if (!e || UNLIT.has(e.slot)) return ch;
  return charOf[`${e.slot}/${relitStep[e.step]}`] ?? ch;
};
const out = [];
for (const v of views) {
  if (v.facing !== from || v.when) continue;
  if (onlyKey && v.key !== onlyKey) continue;
  if (!force && views.some((o) => o.facing === to && o.key === v.key && !o.when)) continue;
  const w = Math.max(...v.rows.map((r) => [...r].length));
  out.push('');
  out.push(`# DRAFT (tools/doll/draft.mjs: ${from} ${v.key} mirrored and relit): repaint by hand for the top-left light before committing`);
  out.push(`view ${to} key ${v.key} origin ${w - 1 - v.origin[0]} ${v.origin[1]}${v.extra.length ? ' ' + v.extra.join(' ') : ''}`);
  for (const m of v.marks) out.push(`mark ${m.name} ${w - 1 - m.x} ${m.y}`);
  for (const r of v.rows) out.push('|' + [...r.padEnd(w, '.')].reverse().map((ch) => (ch === '.' || ch === '-' ? ch : relight(ch))).join(''));
}
if (!out.length) { console.error(`draft.mjs: nothing to draft (no ${from} view${onlyKey ? ' key ' + onlyKey : ''} without its ${to} view)`); process.exit(1); }
if (write) { fs.appendFileSync(file, (text.endsWith('\n') ? '' : '\n') + out.join('\n') + '\n'); console.log(`draft.mjs: ${out.filter((l) => l.startsWith('view')).length} view(s) appended to ${path.relative(process.cwd(), file)}`); }
else console.log(out.join('\n'));
void HERE;
