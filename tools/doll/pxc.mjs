#!/usr/bin/env node
// tools/doll/pxc.mjs: the doll's map compiler (EMBERVALE P2b, VISION_PLAN 15.36 "Compiled and embedded"; Node 18, no
// dependencies). Reads every part map (.pxm) under rpg/art/doll/maps/, the legends (maps/legend/*.legend) and the clip
// tables (rpg/art/doll/clips/*.clip), validates them (legend, views, keys, STYLE_P2 section 24's craft checks) and writes
// ONE generated source, rpg/art/doll/gen/doll_pool.cpp: the packed pool the doll reads (rpg/art/doll/doll_pool.cpp).
// The format is documented in rpg/art/doll/doll.h ("The .pxm format", "The clip format", "The pool").
//
//   node tools/doll/pxc.mjs                 compile maps/ + clips/ into gen/doll_pool.cpp
//   node tools/doll/pxc.mjs --check         compile in memory; exit 1 when gen/doll_pool.cpp is stale or a map fails (CI)
//   node tools/doll/pxc.mjs --report        the pool's size, the generated source's, the compile time, every map's checks
//   node tools/doll/pxc.mjs --strict        craft warnings are errors (parts tagged `draft` excepted)
//   node tools/doll/pxc.mjs --root DIR      compile DIR/maps + DIR/clips instead (e.g. rpg/art/doll/drafts/a3)
//   node tools/doll/pxc.mjs --bin FILE      also write the pool's bytes to FILE (rpg_test --pool, doll_gallery --pool)
//   node tools/doll/pxc.mjs --out FILE      write the generated source to FILE instead
//   node tools/doll/pxc.mjs --no-cpp        do not write the generated source (with --bin)
//   node tools/doll/pxc.mjs --watch         compile again on every save under maps/ and clips/ (Ctrl+C ends it)
//   node tools/doll/pxc.mjs --watch --gallery <doll_gallery exe> <outDir> [board ...]   and re-render the gallery each time
//   node tools/doll/pxc.mjs --no-cache      parse every map (default: maps whose text, legends and stamp libraries are
//                                           unchanged since the last compile come from the cache in the system temp folder)
//   node tools/doll/pxc.mjs --placements F  also write F: one JSON line per `put` (part, facing, key, when, stamp, dx, dy,
//                                           under, cols): the judge's boot tracer (works with --check)
// Stamps (docs/MOTION_P2.md 8): maps/stamps/*.stamps hold hand-drawn keys (`stamp <name> <R|L> origin x y`, rows, regions);
// a part names its libraries (`stamps boots_adult`) and a view places keys (`put <name> <dx> <dy> [under] [cols a-b]`),
// expanded into the view's cells and regions before the craft checks (the pool format is unchanged).
// Exit codes: 0 ok, 1 errors (or stale with --check), 2 bad arguments. A map with an error is LEFT OUT of the pool (named;
// the rest is written, so one area's broken map never stops the others); a clip, legend or --strict error writes nothing.
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import crypto from 'node:crypto';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const REPO = path.resolve(HERE, '..', '..');

// ---------------------------------------------------------------------------------------------- the frozen tables
// (rpg/art/doll/doll.h mirrors every list: ADD at the end, never reorder or rename)
export const SLOTS = ['skin', 'hair', 'eyes', 'face', 'ink', 'cloth1', 'cloth2', 'accent', 'linen', 'cloth3', 'leather', 'metal',
  'metal2', 'head', 'cloak', 'pattern', 'plume', 'field', 'device', 'glow', 'fx'];
export const REGIONS = { U: 1, F: 2, C: 3, H: 4, X: 5, T: 6, K: 7, S: 8, B: 9, O: 10, E: 11 };   // upper fore cuff hand flare thigh knee shin boot sole edge
export const LAYERS = ['fxBack', 'heldBack', 'back', 'cloakBack', 'hairBack', 'armFar', 'legs', 'shoes', 'hangBack', 'torso',
  'hangFront', 'belt', 'neck', 'armNear', 'head', 'face', 'beard', 'hairFront', 'ears', 'headwear', 'held', 'shield',
  'cloakFront', 'fx'];
export const ANCHORS = ['pivot', 'head', 'neck', 'hip', 'shoulderR', 'shoulderL', 'grip', 'gripL'];
export const ROLES = ['legs', 'arms', 'garment', 'overlay', 'neck', 'belt', 'sheath', 'head', 'nose', 'brow', 'ears', 'marks',
  'expr', 'paint', 'earring', 'beard', 'hair', 'headwear', 'cloak', 'held', 'fx', 'body', 'armour', 'helm', 'shield', 'pauldron'];
export const BODIES = { man: 1, woman: 2, youth: 4, child: 8, adult: 3 };
export const FACINGS = 'DURL';
export const CLIPS = ['idle', 'walk', 'hurt', 'windup', 'strike', 'follow', 'cast', 'draw', 'release', 'block', 'roll', 'fall', 'lying'];
export const PARTS = ['loco', 'action', 'death', 'run', 'posture'];
export const FAMILIES = ['unarmed', 'dagger', 'sword', 'greatsword', 'axe', 'mace', 'spear', 'staff', 'bow'];
export const POSTURES = ['none', 'sit', 'siteat', 'sitdrink', 'eat', 'drink', 'cheer', 'hammer', 'hoe', 'sweep', 'chop', 'stir',
  'carry', 'fish', 'sleep', 'wave', 'play', 'dance', 'lute', 'drum', 'flute', 'pray', 'beg', 'lamp', 'read', 'sitfloor',
  'sitflooreat', 'sitfloordrink', 'ride', 'downed', 'kneel', 'row'];
const UNLIT = new Set(['ink', 'face', 'eyes', 'fx']);
const DEFAULT_LAYER = { legs: 'legs', arms: 'armNear', garment: 'torso', overlay: 'belt', neck: 'neck', belt: 'belt', sheath: 'belt',
  head: 'head', nose: 'face', brow: 'face', ears: 'ears', marks: 'face', expr: 'face', paint: 'face', earring: 'ears',
  beard: 'beard', hair: 'hairFront', headwear: 'headwear', cloak: 'cloakBack', held: 'held', fx: 'fx', body: 'torso',
  armour: 'torso', helm: 'headwear', shield: 'shield', pauldron: 'armNear' };
const DEFAULT_ANCHOR = { fxBack: 'pivot', heldBack: 'grip', back: 'neck', cloakBack: 'neck', hairBack: 'head', armFar: 'shoulderR',
  legs: 'pivot', shoes: 'pivot', hangBack: 'hip', torso: 'neck', hangFront: 'hip', belt: 'hip', neck: 'neck', armNear: 'shoulderR',
  head: 'head', face: 'head', beard: 'head', hairFront: 'head', ears: 'head', headwear: 'head', held: 'grip', shield: 'gripL',
  cloakFront: 'neck', fx: 'pivot' };
function defaultChannel(layer, side) {
  if (layer === 'legs' || layer === 'shoes') return 'legs';
  if (layer === 'armNear' || layer === 'armFar') return side === 'L' ? 'armL' : 'armR';
  if (layer === 'hangBack' || layer === 'hangFront') return 'hang';
  if (layer === 'cloakBack' || layer === 'cloakFront') return 'cloak';
  if (layer === 'held' || layer === 'heldBack') return 'held';
  if (layer === 'face') return 'expr';
  if (layer === 'fx' || layer === 'fxBack') return 'fx';
  return 'rest';
}
const POOL_VERSION = 2;   // 2: views carry flags (replace), frames carry turn and at
const CHUNK = 60000;      // bytes per generated array (MSVC: arrays of 64 KB or less)
const CACHE_VERSION = 6;  // bump when a parse or a check changes (old cache entries are dropped); 6: stamps and puts

// ---------------------------------------------------------------------------------------------- arguments
const args = process.argv.slice(2);
const opt = { check: false, report: false, strict: false, root: null, bin: null, out: null, cpp: true, quiet: false, watch: false,
  gallery: null, galleryDir: null, boards: [], cache: true, placements: null };
for (let i = 0; i < args.length; i++) {
  const a = args[i];
  if (a === '--check') opt.check = true;
  else if (a === '--report') opt.report = true;
  else if (a === '--strict') opt.strict = true;
  else if (a === '--quiet') opt.quiet = true;
  else if (a === '--no-cpp') opt.cpp = false;
  else if (a === '--no-cache') opt.cache = false;
  else if (a === '--watch') opt.watch = true;
  else if (a === '--root' && i + 1 < args.length) opt.root = path.resolve(args[++i]);
  else if (a === '--bin' && i + 1 < args.length) opt.bin = path.resolve(args[++i]);
  else if (a === '--out' && i + 1 < args.length) opt.out = path.resolve(args[++i]);
  else if (a === '--placements' && i + 1 < args.length) opt.placements = path.resolve(args[++i]);
  else if (a === '--gallery' && i + 2 < args.length) {
    opt.gallery = path.resolve(args[++i]); opt.galleryDir = path.resolve(args[++i]);
    while (i + 1 < args.length && !args[i + 1].startsWith('--')) opt.boards.push(args[++i]);
  }
  else if (a === '--help' || a === '-h') { console.log(fs.readFileSync(fileURLToPath(import.meta.url), 'utf8').split('\n').slice(1, 28).join('\n')); process.exit(0); }
  else { console.error(`pxc: unknown argument ${a}`); process.exit(2); }
}
if (opt.gallery && !opt.watch) { console.error('pxc: --gallery goes with --watch'); process.exit(2); }
const ROOT = opt.root || path.join(REPO, 'rpg', 'art', 'doll');
const MAPS = path.join(ROOT, 'maps');
const CLIPDIR = path.join(ROOT, 'clips');
const OUT = opt.out || path.join(REPO, 'rpg', 'art', 'doll', 'gen', 'doll_pool.cpp');
const CACHE_FILE = path.join(os.tmpdir(), 'embervale-pxc', crypto.createHash('sha1').update(ROOT).digest('hex').slice(0, 12) + '.json');

// errors and warnings go to the current sink: one per map file while it is parsed (so a cached map brings its own back)
let sink = null;
const err = (where, msg) => sink.errors.push(`${where}: ${msg}`);
const warn = (where, msg, part) => sink.warnings.push({ where, msg, draft: !!(part && part.tags.includes('draft')) });

function walk(dir, ext, out = []) {
  if (!fs.existsSync(dir)) return out;
  for (const e of fs.readdirSync(dir, { withFileTypes: true }).sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0))) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) walk(p, ext, out);
    else if (e.name.endsWith(ext)) out.push(p);
  }
  return out;
}
const rel = (p) => path.relative(ROOT, p).split(path.sep).join('/');
const lines = (text) => text.replace(/\r\n?/g, '\n').split('\n');
const isComment = (ln) => ln === '#' || ln.startsWith('# ') || ln.startsWith('#\t') || ln.startsWith('//');
const cpOf = (s) => [...s];
const sha1 = (s) => crypto.createHash('sha1').update(s).digest('hex');

// ---------------------------------------------------------------------------------------------- legends
// a legend line: `legend <char> <slot> <step>`
function parseLegendLine(where, toks, table) {
  if (toks.length !== 4 || [...toks[1]].length !== 1) { err(where, 'legend: legend <char> <slot> <step>'); return; }
  const ch = toks[1], slot = SLOTS.indexOf(toks[2]), step = Number(toks[3]);
  if (ch === '.' || ch === '-' || ch === '|' || ch === ' ') { err(where, `legend: '${ch}' is reserved`); return; }
  if (slot < 0) { err(where, `legend: unknown slot ${toks[2]} (${SLOTS.join(' ')})`); return; }
  if (!(step >= 0 && step <= 4 && Number.isInteger(step))) { err(where, `legend: step ${toks[3]} (0-4)`); return; }
  table[ch] = { slot, step };
}
function readLegends() {
  const legends = {};
  let sig = '';
  for (const f of walk(path.join(MAPS, 'legend'), '.legend')) {
    const name = path.basename(f, '.legend');
    const table = {};
    const text = fs.readFileSync(f, 'utf8');
    sig += name + ':' + sha1(text) + ';';
    lines(text).forEach((ln, i) => {
      const t = ln.trim();
      if (!t || isComment(t)) return;
      const toks = t.split(/\s+/);
      if (toks[0] !== 'legend') { err(`${rel(f)}:${i + 1}`, `a legend file holds only legend lines`); return; }
      parseLegendLine(`${rel(f)}:${i + 1}`, toks, table);
    });
    legends[name] = table;
  }
  return { legends, sig: sha1(sig) };
}

// ---------------------------------------------------------------------------------------------- stamp libraries
// maps/stamps/<library>.stamps (docs/MOTION_P2.md 8): `use <legend>` / `legend` lines, then entries
//   stamp <name> <R|L> origin <x> <y>      the key's ground pivot in its own grid (y down)
//   |rows                                  the pixels (the library's legend)
//   regions / |rows                        the region plane (optional; lands with the pixels)
// Library checks: one entry per name and facing; O (sole) cells are Ink; an L entry is never the plain mirror of its R
// entry (repaint it for the top-left light); the craft checks (orphans, lone-colour noise) on each entry.
const caret = (row, x) => `\n      |${row}|\n       ${' '.repeat(Math.max(0, x))}^`;
function readStamps(legends) {
  const libs = {};
  let sig = '';
  for (const f of walk(path.join(MAPS, 'stamps'), '.stamps')) {
    const name = path.basename(f, '.stamps');
    const text = fs.readFileSync(f, 'utf8');
    sig += name + ':' + sha1(text) + ';';
    const lib = { name, file: rel(f), uses: [], local: {}, entries: new Map() };
    libs[name] = lib;
    let cur = null, block = null;
    lines(text).forEach((raw, i) => {
      const where = `${rel(f)}:${i + 1}`;
      const ln = raw.replace(/\s+$/, '');
      if (ln.startsWith('|')) {
        if (!cur) { err(where, 'a grid row before any stamp'); return; }
        let row = ln.slice(1);
        const bar = row.indexOf('|');
        if (bar >= 0) row = row.slice(0, bar);
        row = row.replace(/ /g, '.');
        (block === 'regions' ? cur.regRows : cur.rows).push({ row, line: i + 1 });
        return;
      }
      const t = ln.trim();
      if (!t || isComment(t)) { block = null; return; }
      const toks = t.split(/\s+/);
      block = null;
      if (toks[0] === 'use') { if (cur) err(where, 'use goes before the first stamp'); else lib.uses.push(...toks.slice(1)); }
      else if (toks[0] === 'legend') parseLegendLine(where, toks, lib.local);
      else if (toks[0] === 'stamp') {
        const ox = parseInt(toks[4], 10), oy = parseInt(toks[5], 10);
        if (toks.length !== 6 || !/^[A-Za-z0-9_]+$/.test(toks[1]) || (toks[2] !== 'R' && toks[2] !== 'L') || toks[3] !== 'origin' || Number.isNaN(ox) || Number.isNaN(oy)) {
          err(where, 'stamp <name> <R|L> origin <x> <y>'); cur = null; return;
        }
        const id = `${toks[1]}/${toks[2]}`;
        if (lib.entries.has(id)) err(where, `a second stamp ${toks[1]} ${toks[2]} in ${name}`);
        cur = { lib: name, name: toks[1], facing: toks[2] === 'R' ? 2 : 3, ox, oy, rows: [], regRows: [], line: i + 1, uses: 0 };
        lib.entries.set(id, cur);
      } else if (toks[0] === 'regions') { if (!cur) err(where, 'regions before any stamp'); else block = 'regions'; }
      else err(where, `unknown stamp directive ${toks[0]} (use, legend, stamp, regions)`);
    });
    const L = {};
    for (const u of lib.uses) { if (!legends[u]) err(lib.file, `use: no legend maps/legend/${u}.legend`); else Object.assign(L, legends[u]); }
    Object.assign(L, lib.local);
    const INK = SLOTS.indexOf('ink');
    for (const st of lib.entries.values()) {
      const where = `${lib.file}:${st.line}`;
      if (!st.rows.length) { err(where, `stamp ${st.name} ${FACINGS[st.facing]}: no grid rows`); continue; }
      st.w = Math.max(...st.rows.map((r) => cpOf(r.row).length));
      st.h = st.rows.length;
      st.cells = new Uint8Array(st.w * st.h);
      st.opaque = 0;
      st.srcRows = st.rows.map((r) => r.row);
      st.rows.forEach((r, y) => cpOf(r.row).forEach((ch, x) => {
        if (ch === '.') return;
        if (ch === '-') { err(`${lib.file}:${r.line}`, `stamp ${st.name}: '-' (erase) has no place in a stamp${caret(r.row, x)}`); return; }
        const e = L[ch];
        if (!e) { err(`${lib.file}:${r.line}`, `stamp ${st.name}: '${ch}' is not in the legend (${lib.uses.join(', ') || 'no use'})${caret(r.row, x)}`); return; }
        st.cells[y * st.w + x] = 2 + e.slot * 5 + e.step;
        st.opaque++;
      }));
      if (st.regRows.length) {
        if (st.regRows.length !== st.h) err(where, `stamp ${st.name}: regions: ${st.regRows.length} rows for a ${st.h}-row grid`);
        st.reg = new Uint8Array(st.w * st.h);
        st.regRows.forEach((r, y) => cpOf(r.row).forEach((ch, x) => {
          if (ch === '.' || y >= st.h || x >= st.w) return;
          if (!REGIONS[ch]) err(`${lib.file}:${r.line}`, `stamp ${st.name}: regions: '${ch}' is no region (${Object.keys(REGIONS).join('')})`);
          else st.reg[y * st.w + x] = REGIONS[ch];
        }));
        for (let k = 0; k < st.w * st.h; k++) {
          if (st.reg[k] !== REGIONS.O) continue;
          const c = st.cells[k];
          if (c < 2 || Math.floor((c - 2) / 5) !== INK) {
            const x = k % st.w, y = Math.floor(k / st.w);
            err(`${lib.file}:${st.regRows[y].line}`, `stamp ${st.name} ${FACINGS[st.facing]}: sole (O) cell ${x},${y} is not Ink${caret(st.srcRows[y] || '', x)}`);
          }
        }
      }
      if (st.ox < 0 || st.oy < 0 || st.ox >= st.w || st.oy >= st.h) err(where, `stamp ${st.name}: origin ${st.ox} ${st.oy} lies outside its ${st.w}x${st.h} grid`);
      // the craft checks, as on a part map (a stamp is a boot on its own: orphans and lone-colour noise)
      const fakeP = { file: lib.file, layer: 'shoes', roles: [], tags: [], channel: 'rest' };
      const fakeV = { facing: st.facing, when: '', w: st.w, h: st.h, cells: st.cells, opaque: st.opaque };
      craftChecks(fakeP, fakeV, `${where} (stamp ${st.name} ${FACINGS[st.facing]})`);
      st.checks = fakeV.checks;
      delete st.rows; delete st.regRows;
    }
    // an L entry is painted, never the plain mirror of its R entry (STYLE_P2 9, 24)
    for (const st of lib.entries.values()) {
      if (st.facing !== 3 || !st.cells) continue;
      const R = lib.entries.get(`${st.name}/R`);
      if (!R || !R.cells || R.w !== st.w || R.h !== st.h) continue;
      let same = st.opaque > 0;
      for (let y = 0; y < st.h && same; y++) for (let x = 0; x < st.w && same; x++) if (st.cells[y * st.w + x] !== R.cells[y * st.w + (st.w - 1 - x)]) same = false;
      if (same) err(`${lib.file}:${st.line}`, `stamp ${st.name} L is the plain mirror of stamp ${st.name} R: repaint it for the top-left light`);
    }
  }
  return { libs, sig: sha1(sig) };
}

// a view's puts: each stamp lands with its origin at (dx, dy) from the view's pivot (its origin cell), the view's facing
// picking the stamp's entry; `under` writes only the view's clear cells, the default writes over them; `cols a-b` keeps only
// those stamp columns. The stamp's regions land with its pixels. Returns the mask of cells the puts wrote.
function expandPuts(P, V, stamps, where) {
  const mask = new Uint8Array(V.w * V.h);
  if (!V.puts.length) return mask;
  if (V.facing !== 2 && V.facing !== 3) { err(where, `put: only side views (R, L) place stamps (view ${FACINGS[V.facing]} key ${V.key})`); return mask; }
  if (!P.stamps.length) { err(where, 'put: the part names no stamp library (stamps <library> before the first view)'); return mask; }
  if (!V.origin) return mask;
  const fc = FACINGS[V.facing];
  for (const pt of V.puts) {
    const pw = `${P.file}:${pt.line}`;
    let st = null;
    for (const ln of P.stamps) {
      const lib = stamps.libs[ln];
      if (!lib) continue;
      st = lib.entries.get(`${pt.name}/${fc}`) || null;
      if (st) break;
    }
    if (!st) {
      const other = P.stamps.some((ln) => stamps.libs[ln] && stamps.libs[ln].entries.has(`${pt.name}/${fc === 'R' ? 'L' : 'R'}`));
      err(pw, `put ${pt.name}: no stamp ${pt.name} ${fc} in ${P.stamps.join(', ')}${other ? ` (it has only the ${fc === 'R' ? 'L' : 'R'} entry)` : ''} (view ${fc} key ${V.key}${V.when ? ' when ' + V.when : ''})`);
      continue;
    }
    if (!st.cells) continue;
    if (pt.cols && pt.cols[1] >= st.w) { err(pw, `put ${pt.name} cols ${pt.cols[0]}-${pt.cols[1]}: the stamp is ${st.w} columns wide`); continue; }
    const bx = V.origin[0] + pt.dx - st.ox, by = V.origin[1] + pt.dy - st.oy;
    let outside = null;
    for (let sy = 0; sy < st.h; sy++) for (let sx = 0; sx < st.w; sx++) {
      const c = st.cells[sy * st.w + sx];
      if (!c || (pt.cols && (sx < pt.cols[0] || sx > pt.cols[1]))) continue;
      const vx = bx + sx, vy = by + sy;
      if (vx < 0 || vy < 0 || vx >= V.w || vy >= V.h) { if (!outside) outside = [sx, sy, vx, vy]; continue; }
      const k = vy * V.w + vx;
      if (pt.under && V.cells[k] !== 0) continue;
      V.cells[k] = c;
      if (st.reg || V.reg) {
        if (!V.reg) V.reg = new Uint8Array(V.w * V.h);
        V.reg[k] = st.reg ? st.reg[sy * st.w + sx] : 0;
      }
      mask[k] = 1;
    }
    if (outside) err(pw, `put ${pt.name} ${pt.dx} ${pt.dy}: stamp cell ${outside[0]},${outside[1]} lands outside the view's ${V.w}x${V.h} grid at ${outside[2]},${outside[3]} (view ${fc} key ${V.key})`);
  }
  V.opaque = V.cells.reduce((n, c) => n + (c >= 2 ? 1 : 0), 0);
  return mask;
}

// ---------------------------------------------------------------------------------------------- part maps
function bodyMaskFromPath(r) {
  if (/\/man\//.test(r) || r.startsWith('wear/man')) return 1;
  if (/\/woman\//.test(r)) return 2;
  if (/(^|\/)youth\//.test(r)) return 4;
  if (/(^|\/)child\//.test(r)) return 8;
  return 3;   // adult (man and woman) unless the part says otherwise
}
function parsePart(file, text) {
  const r = rel(file);
  const name0 = path.relative(MAPS, file).split(path.sep).join('/').replace(/\.pxm$/, '');
  const P = { file: r, name: name0, roles: [], tags: [], uses: [], stamps: [], local: {}, layer: null, anchor: null, channel: null,
    side: 0, z: 0, sway: 0, bodyMask: 0, views: [] };
  let view = null, block = null;   // block: 'grid' | 'regions' | 'build'
  const endBlock = () => { block = null; };
  lines(text).forEach((raw, i) => {
    const where = `${r}:${i + 1}`;
    const ln = raw.replace(/\s+$/, '');
    if (ln.startsWith('|')) {
      if (!view) { err(where, 'a grid row before any view'); return; }
      let row = ln.slice(1);
      const bar = row.indexOf('|');
      if (bar >= 0) row = row.slice(0, bar);   // `|row| comment`
      row = row.replace(/ /g, '.');
      if (block === 'regions') view.regions.push({ row, line: i + 1 });
      else if (block === 'build') view.build.push({ row, line: i + 1 });
      else { block = 'grid'; view.rows.push({ row, line: i + 1 }); }
      return;
    }
    const t = ln.trim();
    if (!t || isComment(t)) { endBlock(); return; }
    const toks = t.split(/\s+/);
    const kw = toks[0];
    endBlock();
    switch (kw) {
      case 'part': P.name = toks[1] || P.name; break;
      case 'use': for (const u of toks.slice(1)) P.uses.push(u); break;
      case 'stamps':
        if (view) err(where, 'stamps <library ...> goes before the first view (like use)');
        else if (toks.length < 2) err(where, 'stamps <library ...> (maps/stamps/<library>.stamps)');
        else for (const u of toks.slice(1)) P.stamps.push(u);
        break;
      case 'put': {
        if (!view) { err(where, 'put before any view'); break; }
        const pt = { name: toks[1], dx: parseInt(toks[2], 10), dy: parseInt(toks[3], 10), under: false, cols: null, line: i + 1 };
        let bad = toks.length < 4 || Number.isNaN(pt.dx) || Number.isNaN(pt.dy) || !/^[A-Za-z0-9_]+$/.test(pt.name || '');
        for (let k = 4; k < toks.length && !bad; k++) {
          if (toks[k] === 'under' && !pt.under) pt.under = true;
          else if (toks[k] === 'cols' && !pt.cols) {
            const m = /^(\d+)-(\d+)$/.exec(toks[++k] || '');
            if (!m || +m[1] > +m[2]) bad = true; else pt.cols = [+m[1], +m[2]];
          } else bad = true;
        }
        if (bad) err(where, 'put <stamp> <dx> <dy> [under] [cols <a>-<b>]');
        else view.puts.push(pt);
        break;
      }
      case 'legend': parseLegendLine(where, toks, P.local); break;
      case 'role':
        if (toks.length !== 3 || ROLES.indexOf(toks[1]) < 0) err(where, `role <category> <value> (categories: ${ROLES.join(' ')})`);
        else P.roles.push({ cat: ROLES.indexOf(toks[1]), value: toks[2] });
        break;
      case 'body':
        for (const b of toks.slice(1)) { if (!(b in BODIES)) err(where, `body: unknown set ${b} (man woman youth child adult)`); else P.bodyMask |= BODIES[b]; }
        break;
      case 'tags': P.tags.push(...toks.slice(1)); break;
      case 'layer': if (LAYERS.indexOf(toks[1]) < 0) err(where, `layer: unknown ${toks[1]}`); else P.layer = toks[1]; break;
      case 'anchor': if (ANCHORS.indexOf(toks[1]) < 0) err(where, `anchor: unknown ${toks[1]}`); else P.anchor = toks[1]; break;
      case 'keys': if (!/^[A-Za-z][A-Za-z0-9_]*$/.test(toks[1] || '')) err(where, 'keys <channel>'); else P.channel = toks[1]; break;
      case 'side': if (toks[1] !== 'R' && toks[1] !== 'L') err(where, 'side R|L'); else P.side = toks[1] === 'R' ? 1 : 2; break;
      case 'z': P.z = parseInt(toks[1], 10) | 0; if (P.z < -100 || P.z > 100) err(where, 'z -100..100'); break;
      case 'sway': P.sway = parseInt(toks[1], 10) | 0; if (P.sway < 0 || P.sway > 40) err(where, 'sway 0..40'); break;
      case 'view': {
        view = { facing: -1, key: 'rest', when: '', replace: false, origin: null, layer: null, anchor: null, stretch: -1, rows: [], regions: [],
          build: [], marks: [], puts: [], line: i + 1 };
        if (toks.length < 2 || FACINGS.indexOf(toks[1]) < 0 || toks[1].length !== 1) err(where, 'view <D|U|R|L> [key k] [origin x y] [layer l] [anchor a] [when c [replace]] [stretch row]');
        view.facing = FACINGS.indexOf(toks[1]);
        for (let k = 2; k < toks.length; k++) {
          const a = toks[k];
          if (a === 'key') view.key = toks[++k];
          else if (a === 'origin') { view.origin = [parseInt(toks[k + 1], 10), parseInt(toks[k + 2], 10)]; k += 2; if (view.origin.some(Number.isNaN)) err(where, 'origin x y'); }
          else if (a === 'layer') { view.layer = toks[++k]; if (LAYERS.indexOf(view.layer) < 0) err(where, `layer: unknown ${view.layer}`); }
          else if (a === 'anchor') { view.anchor = toks[++k]; if (ANCHORS.indexOf(view.anchor) < 0) err(where, `anchor: unknown ${view.anchor}`); }
          else if (a === 'when') view.when = toks[++k] || '';
          else if (a === 'stretch') view.stretch = parseInt(toks[++k], 10);
          else if (a === 'replace') view.replace = true;
          else err(where, `view: unknown attribute ${a}`);
        }
        if (view.replace && !view.when) err(where, 'view: replace needs a when <cond> (the view drawn instead of its base when the look holds it)');
        if (!view.key || !/^[A-Za-z0-9_+-]+$/.test(view.key) || view.key === '-') err(where, 'view: a key is a word (not "-")');
        P.views.push(view);
        break;
      }
      case 'mark':
        if (!view) { err(where, 'mark before any view'); break; }
        if (toks.length !== 4) { err(where, 'mark <name> <x> <y>'); break; }
        view.marks.push({ name: toks[1], x: parseInt(toks[2], 10), y: parseInt(toks[3], 10) });
        break;
      case 'regions': if (!view) err(where, 'regions before any view'); else block = 'regions'; break;
      case 'build': if (!view) err(where, 'build before any view'); else block = 'build'; break;
      default: err(where, `unknown directive ${kw}`);
    }
  });
  if (!P.bodyMask) P.bodyMask = bodyMaskFromPath(path.relative(MAPS, file).split(path.sep).join('/'));
  const firstRole = P.roles.length ? ROLES[P.roles[0].cat] : null;
  if (!P.layer) P.layer = (firstRole && DEFAULT_LAYER[firstRole]) || 'torso';
  if (!P.anchor) P.anchor = P.side === 2 && DEFAULT_ANCHOR[P.layer] === 'shoulderR' ? 'shoulderL' : DEFAULT_ANCHOR[P.layer];
  if (!P.channel) P.channel = defaultChannel(P.layer, P.side === 2 ? 'L' : 'R');
  return P;
}

// the grids through the legend, the regions, the build rows, and the per-map checks (everything that needs only the map
// and the legends: the cache keeps the result)
function buildPart(P, legends, stamps) {
  const L = {};
  for (const u of P.uses) {
    if (!legends[u]) err(P.file, `use: no legend maps/legend/${u}.legend`);
    else Object.assign(L, legends[u]);
  }
  Object.assign(L, P.local);
  for (const n of P.stamps) if (!stamps.libs[n]) err(P.file, `stamps: no library maps/stamps/${n}.stamps`);
  const stamped = P.tags.includes('boots-stamped');
  if (!P.roles.length) warn(P.file, 'no role: no look will ever use this part (a dead part)', P);
  const seen = new Set();
  for (const V of P.views) {
    const where = `${P.file}:${V.line}`;
    const id = `${V.facing}/${V.key}/${V.when}`;
    if (seen.has(id)) err(where, `a second view ${FACINGS[V.facing]} key ${V.key}${V.when ? ' when ' + V.when : ''}`);
    seen.add(id);
    if (!V.origin) { if (V.when) V.origin = null; else err(where, 'view: origin x y is required'); }
    if (!V.rows.length) { err(where, 'view without grid rows'); continue; }
    const w = Math.max(...V.rows.map((r) => cpOf(r.row).length));
    const h = V.rows.length;
    V.w = w; V.h = h;
    V.cells = new Uint8Array(w * h);
    V.opaque = 0;
    V.rows.forEach((r, y) => {
      cpOf(r.row).forEach((ch, x) => {
        if (ch === '.') return;
        if (ch === '-') { V.cells[y * w + x] = 1; return; }
        const e = L[ch];
        if (!e) { err(`${P.file}:${r.line}`, `'${ch}' is not in the legend (${P.uses.join(', ') || 'no use'})`); return; }
        V.cells[y * w + x] = 2 + e.slot * 5 + e.step;
        V.opaque++;
      });
    });
    if (V.regions.length) {
      if (V.regions.length !== h) err(where, `regions: ${V.regions.length} rows for a ${h}-row grid`);
      V.reg = new Uint8Array(w * h);
      V.regions.forEach((r, y) => cpOf(r.row).forEach((ch, x) => {
        if (ch === '.' || y >= h || x >= w) return;
        if (!REGIONS[ch]) err(`${P.file}:${r.line}`, `regions: '${ch}' is no region (${Object.keys(REGIONS).join('')})`);
        else V.reg[y * w + x] = REGIONS[ch];
      }));
    }
    if (V.build.length) {
      if (V.build.length !== 1) err(where, 'build: one row');
      V.bld = new Uint8Array(w);
      cpOf(V.build[0].row).forEach((ch, x) => {
        if (x >= w || ch === '.') return;
        if (ch === '+') V.bld[x] = 1; else if (ch === 'x') V.bld[x] = 2;
        else err(`${P.file}:${V.build[0].line}`, `build: '${ch}' ('+' broad adds the column, 'x' slim removes it)`);
      });
    }
    if (V.stretch >= h || V.stretch < -1) err(where, `stretch ${V.stretch}: no such row`);
    // the puts (a `when` view without an origin stands on its base view's)
    const saveOrigin = V.origin;
    V.origin = V.origin || (V.when && (P.views.find((b) => !b.when && b.facing === V.facing && b.key === V.key) || {}).origin) || null;
    const putMask = expandPuts(P, V, stamps, where);
    V.origin = saveOrigin;
    // boots-stamped: every boot (B) and sole (O) cell of a side walk/run view comes from a put, never drawn by hand
    if (stamped && (V.facing === 2 || V.facing === 3) && /^(walk|run)/.test(V.key) && V.reg) {
      let n = 0, first = -1;
      for (let k = 0; k < w * h; k++) {
        if ((V.reg[k] === REGIONS.B || V.reg[k] === REGIONS.O) && !putMask[k]) { n++; if (first < 0) first = k; }
      }
      if (n) {
        const x = first % w, y = Math.floor(first / w);
        const src = V.rows[y] ? V.rows[y] : { row: '', line: V.line };
        err(`${P.file}:${src.line}`, `boots-stamped: part ${P.name} view ${FACINGS[V.facing]} key ${V.key}${V.when ? ' when ' + V.when : ''}: ${n} boot/sole cell(s) not written by a put (the first: ${x},${y}, region ${V.reg[first] === REGIONS.O ? 'O' : 'B'}); a boot is a stamp (put), never hand-drawn${caret(src.row, x)}`);
      }
    }
    craftChecks(P, V, where);
    delete V.rows; delete V.regions; delete V.build;
  }
  // views with `when` overlay a base view of the same facing and key
  for (const V of P.views) {
    if (!V.when) continue;
    const base = P.views.find((b) => !b.when && b.facing === V.facing && b.key === V.key);
    if (!base) err(`${P.file}:${V.line}`, `when ${V.when}: no base view ${FACINGS[V.facing]} key ${V.key}`);
    else if (!V.origin) V.origin = base.origin;
  }
  // the left view is painted, never the plain mirror of the right one (STYLE_P2 9, 24); FX may be mirrored
  const fxPart = P.layer === 'fx' || P.layer === 'fxBack' || P.tags.includes('mirror-ok');
  for (const V of P.views) {
    if (V.facing !== 3 || V.when || fxPart || !V.cells) continue;
    const R = P.views.find((b) => !b.when && b.facing === 2 && b.key === V.key);
    if (!R || !R.cells || R.w !== V.w || R.h !== V.h) continue;
    let same = V.opaque > 0;
    for (let y = 0; y < V.h && same; y++) for (let x = 0; x < V.w && same; x++) if (V.cells[y * V.w + x] !== R.cells[y * V.w + (V.w - 1 - x)]) same = false;
    if (same) err(`${P.file}:${V.line}`, `view L key ${V.key} is the plain mirror of view R: repaint it for the top-left light (tools/doll/draft.mjs only drafts it)`);
  }
  // (a key's missing facings are checked against the clips that use it, after the clips are read: facingChecks)
}

// every key of a part in each facing the clips draw it in (the coverage census is the gate; this is the compiler's early
// word): a key is wanted in a facing when a frame of a body set the part serves names it on the part's channel there (a
// rigid part, or a channel the frame does not name, draws "rest"); turned frames (lying, a topple) are composed in the
// left view, so their keys are wanted in L only. A tile serves every facing from one view. Not cached: it reads the clips.
function facingChecks(parts, clips) {
  const BASES = ['man', 'woman', 'youth', 'child'];
  const framesOf = new Map(BASES.map((b) => [b, []]));   // base -> [{ facing, keys: Map(channel -> key), where }]
  for (const c of clips) {
    const base = c.body.split('.')[0];
    if (!framesOf.has(base)) continue;
    const lying = CLIPS[c.clip] === 'lying';
    c.frames.forEach((fr) => {
      framesOf.get(base).push({ facing: fr.turn || lying ? 3 : c.facing, keys: new Map(fr.keys.map((k) => [k.channel, k.key])), where: `${c.file}:${c.line}` });
    });
  }
  const out = [];
  for (const P of parts) {
    if (P.tags.includes('tile')) continue;
    const have = new Map();
    for (const V of P.views) if (!V.when) { if (!have.has(V.key)) have.set(V.key, new Set()); have.get(V.key).add(V.facing); }
    for (const [k, fs4] of have) {
      const want = new Map();   // facing -> the first clip that wants it
      BASES.forEach((b, bi) => {
        if (!(P.bodyMask >> bi & 1)) return;
        for (const f of framesOf.get(b)) {
          const key = P.channel === 'rest' ? 'rest' : (f.keys.get(P.channel) ?? 'rest');
          if (key === k && !want.has(f.facing)) want.set(f.facing, f.where);
        }
      });
      const miss = [...want.keys()].filter((f) => !fs4.has(f)).sort();
      if (miss.length) {
        out.push({ where: P.file, msg: `key ${k}: no view ${miss.map((f) => FACINGS[f]).join('')} (the clips draw it there: ${want.get(miss[0])})`,
          draft: P.tags.includes('draft') });
      }
    }
  }
  return out;
}

// STYLE_P2 24's craft checks on a map:
//   orphans      no pixel whose eight neighbours are all clear
//   light        the top-left light, in the front views (D, U): in each 8-connected piece of a material at least 5 px
//                wide, its pixels left of the piece's middle at least as light (mean step, the outline step 0 left out:
//                the finishing pass relights it) as those right of it. Side views are not measured: A3 lights the
//                garments' fronts there (its approved apron and skirt), and the L view's repaint is guarded by the
//                plain-mirror error
//   noise        no lone-colour speck: a pixel whose code no neighbour shares, inside a patch of its own material (7 of 8
//                neighbours opaque, most of them one step of that material at least 2 steps away). Step-4 highlights
//                and metals (glints, a shine pixel, a rivet) are meant to stand alone; a pixel inside its own outline is a
//                thin feature (a braid's strand); faces and FX are spared
// (overlays, the face's pieces and tiles lie on other parts and are checked composed: rpg_test --doll --gates B8)
function craftChecks(P, V, where) {
  // overlays lie on other parts (their neighbours are those parts' pixels): `when` views, erasing maps, small keyed
  // pieces, tiles, and everything on the face (expressions, marks, paint) are not checked alone
  const onFace = P.layer === 'face' || P.roles.some((r) => ['expr', 'marks', 'paint', 'earring'].includes(ROLES[r.cat]));
  const overlay = !!V.when || onFace || P.tags.includes('overlay') || P.tags.includes('tile') || V.cells.some((c) => c === 1) || P.channel !== 'rest' && V.opaque < 12;
  const w = V.w, h = V.h;
  const at = (x, y) => (x < 0 || y < 0 || x >= w || y >= h ? 0 : V.cells[y * w + x]);
  V.checks = { orphans: 0, light: [], noise: 0 };
  if (overlay) return;
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
    if (at(x, y) < 2) continue;
    let any = false;
    for (let dy = -1; dy <= 1 && !any; dy++) for (let dx = -1; dx <= 1 && !any; dx++) if ((dx || dy) && at(x + dx, y + dy) >= 2) any = true;
    if (!any) { V.checks.orphans++; if (V.checks.orphans <= 2) warn(where, `orphan pixel at ${x},${y}`, P); }
  }
  const slotOf = (c) => Math.floor((c - 2) / 5), stepOf = (c) => (c - 2) % 5;
  // light: front views only, per piece of a material (things in hand have their own rule: the weapon library's lit edge)
  const inHand = P.layer === 'held' || P.layer === 'heldBack' || P.layer === 'shield';
  if (V.facing <= 1 && !inHand) {
    const seen = new Uint8Array(w * h);
    for (let y0 = 0; y0 < h; y0++) for (let x0 = 0; x0 < w; x0++) {
      const c0 = at(x0, y0);
      if (c0 < 2 || seen[y0 * w + x0]) continue;
      const slot = slotOf(c0);
      if (UNLIT.has(SLOTS[slot])) continue;
      const stack = [[x0, y0]], px = [];
      seen[y0 * w + x0] = 1;
      while (stack.length) {
        const [x, y] = stack.pop();
        px.push([x, y]);
        for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
          const X = x + dx, Y = y + dy, c = at(X, Y);
          if (c >= 2 && slotOf(c) === slot && !seen[Y * w + X]) { seen[Y * w + X] = 1; stack.push([X, Y]); }
        }
      }
      let xa = w, xb = -1;
      for (const [x] of px) { xa = Math.min(xa, x); xb = Math.max(xb, x); }
      if (xb - xa < 4) continue;
      const mid = (xa + xb) / 2;
      let l = 0, ln = 0, r = 0, rn = 0;
      for (const [x, y] of px) {
        const s = stepOf(at(x, y));
        if (s === 0) continue;
        if (x < mid) { l += s; ln++; } else if (x > mid) { r += s; rn++; }
      }
      if (ln < 6 || rn < 6) continue;
      V.checks.light.push({ slot: SLOTS[slot], left: l / ln, right: r / rn });
      if (l / ln + 0.35 < r / rn) warn(where, `${SLOTS[slot]} (the piece at ${xa}..${xb}): the left half (step ${(l / ln).toFixed(2)}) darker than the right (${(r / rn).toFixed(2)}): the light is top-left`, P);
    }
  }
  // noise
  const faceLike = P.layer === 'face' || P.layer === 'fx' || P.layer === 'fxBack';
  if (!faceLike) {
    for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
      const c = at(x, y);
      if (c < 2) continue;
      const slot = slotOf(c), step = stepOf(c), sn = SLOTS[slot];
      if (step === 0 || step === 4 || UNLIT.has(sn) || sn === 'metal' || sn === 'metal2') continue;
      let opaque = 0, same = false;
      const count = new Map();
      for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
        if (!dx && !dy) continue;
        const n = at(x + dx, y + dy);
        if (n < 2) continue;
        opaque++;
        if (n === c) same = true;
        count.set(n, (count.get(n) || 0) + 1);
      }
      if (same || opaque < 7) continue;
      let best = 0, bc = 0;
      for (const [n, k] of count) if (k > best) { best = k; bc = n; }
      // (inside its own outline, step 0, a pixel is a thin feature: a strand, a seam; noise sits inside a shaded patch)
      if (best >= 5 && slotOf(bc) === slot && stepOf(bc) !== 0 && Math.abs(stepOf(bc) - step) >= 2) {
        V.checks.noise++;
        if (V.checks.noise <= 2) warn(where, `a lone ${sn} step-${step} speck at ${x},${y} in step-${stepOf(bc)} ${sn} (noise)`, P);
      }
    }
  }
}

// ---- the cache (incremental compile): a map whose text and legends are unchanged comes back parsed and checked
const b64 = (a) => (a ? Buffer.from(a).toString('base64') : null);
const unb64 = (s) => (s ? new Uint8Array(Buffer.from(s, 'base64')) : undefined);
function serialize(P, msgs) {
  return { P: { ...P, views: P.views.map((V) => ({ ...V, cells: b64(V.cells), reg: b64(V.reg), bld: b64(V.bld) })) }, msgs };
}
function deserialize(e) {
  const P = { ...e.P, views: e.P.views.map((V) => ({ ...V, cells: unb64(V.cells), reg: unb64(V.reg), bld: unb64(V.bld) })) };
  return { P, msgs: e.msgs };
}
let cache = null;   // { version, legends, files: { rel: { sig, entry } } }
function loadCache() {
  if (cache) return;
  cache = { version: CACHE_VERSION, files: {} };
  if (!opt.cache) return;
  try {
    const c = JSON.parse(fs.readFileSync(CACHE_FILE, 'utf8'));
    if (c.version === CACHE_VERSION) cache = c;
  } catch { /* no cache yet */ }
}
function saveCache() {
  if (!opt.cache) return;
  // (written beside and renamed over: several areas compile at once, and a reader never sees half a file)
  try {
    fs.mkdirSync(path.dirname(CACHE_FILE), { recursive: true });
    const tmp = `${CACHE_FILE}.${process.pid}.tmp`;
    fs.writeFileSync(tmp, JSON.stringify(cache));
    fs.renameSync(tmp, CACHE_FILE);
  } catch { /* best effort */ }
}

// ---------------------------------------------------------------------------------------------- clips
function readClips() {
  const bodySets = [];   // { name, anchors: [facing] -> [{id, x, y}] }
  const clips = [];      // { body, clip, part, facing, family, variant, posture, cycle, frames: [...] }
  for (const f of walk(CLIPDIR, '.clip')) {
    let body = null, clip = null;
    lines(fs.readFileSync(f, 'utf8')).forEach((raw, i) => {
      const where = `${rel(f)}:${i + 1}`;
      const t = raw.trim();
      if (!t || isComment(t)) return;
      const toks = t.split(/\s+/);
      switch (toks[0]) {
        case 'body': {
          const nm = toks[1] || '';
          if (!/^(man|woman|youth|child)(\.[a-z0-9]+)?$/.test(nm)) { err(where, 'body <man|woman|youth|child>[.<modifier>]'); return; }
          body = bodySets.find((b) => b.name === nm);
          if (!body) { body = { name: nm, anchors: [[], [], [], []] }; bodySets.push(body); }
          clip = null;
          return;
        }
        case 'anchors': {
          if (!body) { err(where, 'anchors before body'); return; }
          const fc = FACINGS.indexOf(toks[1]);
          if (fc < 0 || toks[1].length !== 1) { err(where, 'anchors <D|U|R|L> name=x,y ...'); return; }
          for (const kv of toks.slice(2)) {
            const m = /^([A-Za-z]+)=(-?\d+),(-?\d+)$/.exec(kv);
            const id = m ? ANCHORS.indexOf(m[1]) : -1;
            if (!m || id < 0) { err(where, `anchors: ${kv} (names: ${ANCHORS.join(' ')})`); continue; }
            body.anchors[fc] = body.anchors[fc].filter((a) => a.id !== id);
            body.anchors[fc].push({ id, x: +m[2], y: +m[3] });
          }
          return;
        }
        case 'clip': {
          if (!body) { err(where, 'clip before body'); return; }
          let name = toks[1];
          let part = -1;
          if (name === 'run') { name = 'walk'; part = 3; }
          const ci = CLIPS.indexOf(name);
          const fc = FACINGS.indexOf(toks[2] || '?');
          if (ci < 0 || fc < 0) { err(where, `clip <${CLIPS.join('|')}|run> <D|U|R|L> [part p] [family f] [variant n] [posture p] [cycle px]`); return; }
          clip = { body: body.name, clip: ci, part, facing: fc, family: 255, variant: 0, posture: 255, cycle: 0, frames: [], line: i + 1, file: rel(f) };
          for (let k = 3; k < toks.length; k++) {
            const a = toks[k], v = toks[++k];
            if (a === 'part') { clip.part = PARTS.indexOf(v); if (clip.part < 0) err(where, `part: ${PARTS.join(' ')}`); }
            else if (a === 'family') { clip.family = FAMILIES.indexOf(v); if (clip.family < 0) err(where, `family: ${FAMILIES.join(' ')}`); }
            else if (a === 'variant') clip.variant = parseInt(v, 10) | 0;
            else if (a === 'posture') { clip.posture = POSTURES.indexOf(v); if (clip.posture < 0) err(where, `posture: ${POSTURES.join(' ')}`); }
            else if (a === 'cycle') clip.cycle = Math.round(parseFloat(v) * 16);
            else err(where, `clip: unknown attribute ${a}`);
          }
          if (clip.part < 0) clip.part = clip.posture !== 255 ? 4 : ci <= 2 ? 0 : ci >= 11 ? 2 : 1;
          if (clips.some((c) => c.body === clip.body && c.clip === clip.clip && c.part === clip.part && c.facing === clip.facing &&
              c.family === clip.family && c.variant === clip.variant && c.posture === clip.posture)) err(where, 'a second clip with the same body, name, facing, part, family, variant and posture');
          clips.push(clip);
          return;
        }
        case 'f': {
          if (!clip) { err(where, 'a frame before any clip'); return; }
          const fr = { keys: [], bob: 0, lean: 0, hip: [0, 0], head: [0, 0], sway: 0, ms: 0, weapon: -32768, front: 1, plant: [-32768, -32768], turn: 0, at: [0, 0] };
          for (const kv of toks.slice(1)) {
            const m = /^([A-Za-z][A-Za-z0-9_]*)=(.+)$/.exec(kv);
            if (!m) { err(where, `frame: ${kv} (name=value)`); continue; }
            const [, k, v] = m;
            const int = (s) => { const n = parseInt(s, 10); if (Number.isNaN(n)) err(where, `frame: ${kv}`); return n | 0; };
            const pair = (s) => { const p = s.split(','); if (p.length !== 2) err(where, `frame: ${kv} (dx,dy)`); return [int(p[0]), int(p[1] || '0')]; };
            if (k === 'bob') fr.bob = int(v);
            else if (k === 'lean') fr.lean = int(v);
            else if (k === 'hip') fr.hip = pair(v);
            else if (k === 'head') fr.head = pair(v);
            else if (k === 'sway') fr.sway = int(v);
            else if (k === 'ms') fr.ms = int(v);
            else if (k === 'weapon') { const p = v.split(','); fr.weapon = int(p[0]); fr.front = p[1] === 'back' ? 0 : 1; }
            else if (k === 'plant') { const p = v.split(':'); fr.plant = [Math.round(parseFloat(p[0]) * 16), Math.round(parseFloat(p[1] ?? p[0]) * 16)]; }
            else if (k === 'turn') { fr.turn = v === 'west' ? 1 : v === 'east' ? 2 : v === 'none' ? 0 : -1; if (fr.turn < 0) { err(where, 'frame: turn=west|east|none'); fr.turn = 0; } }
            else if (k === 'at') fr.at = pair(v);
            else fr.keys.push({ channel: k, key: v });
          }
          clip.frames.push(fr);
          return;
        }
        default: err(where, `unknown clip directive ${toks[0]}`);
      }
    });
  }
  for (const c of clips) if (!c.frames.length) err(`${c.file}:${c.line} (${c.body} ${CLIPS[c.clip]} ${FACINGS[c.facing]})`, 'a clip without frames');
  return { bodySets, clips };
}

// ---------------------------------------------------------------------------------------------- the pool
class W {
  constructor() { this.b = []; }
  u8(v) { this.b.push(v & 255); }
  i8(v) { this.u8(v < 0 ? v + 256 : v); }
  u16(v) { this.u8(v); this.u8(v >> 8); }
  i16(v) { this.u16(v < 0 ? v + 65536 : v); }
  u32(v) { this.u16(v & 0xFFFF); this.u16((v >>> 16) & 0xFFFF); }
  bytes(a) { for (const x of a) this.b.push(x); }
}
function pack(parts, bodySets, clips) {
  const strings = [''], strIx = new Map([['', 0]]);
  const S = (s) => { if (!strIx.has(s)) { strIx.set(s, strings.length); strings.push(s); } return strIx.get(s); };
  const blob = [];
  const rle = (cells) => {   // control < 128: (c + 1) clear cells; >= 128: (c - 127) literal bytes follow
    const at = blob.length;
    let i = 0;
    while (i < cells.length) {
      if (cells[i] === 0) {
        let n = 0;
        while (i < cells.length && cells[i] === 0 && n < 128) { n++; i++; }
        blob.push(n - 1);
      } else {
        let j = i;
        while (j < cells.length && j - i < 128 && cells[j] !== 0) j++;
        blob.push(127 + (j - i));
        for (let k = i; k < j; k++) blob.push(cells[k]);
        i = j;
      }
    }
    return at;
  };
  // stable order: parts by name
  parts.sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));
  const viewRecs = [], markRecs = [];
  let opaquePx = 0, cellCount = 0;
  for (const P of parts) {
    P.firstView = viewRecs.length;
    for (const V of P.views) {
      if (!V.cells) continue;
      opaquePx += V.opaque; cellCount += V.w * V.h;
      viewRecs.push({ P, V, pix: rle(V.cells), reg: V.reg ? rle(V.reg) : 0xFFFFFFFF, bld: V.bld ? (blob.push(...V.bld), blob.length - V.bld.length) : 0xFFFFFFFF,
        firstMark: markRecs.length });
      for (const m of V.marks) markRecs.push(m);
    }
    P.nViews = viewRecs.length - P.firstView;
  }
  const w = new W();
  w.bytes([68, 79, 76, 80]);   // 'DOLP'
  w.u16(POOL_VERSION); w.u16(0);
  w.u32(0); w.u32(0);          // (total size, reserved: patched below)
  // strings are collected while writing the records: write the records to a second buffer first
  const rec = new W();
  rec.u32(parts.length);
  for (const P of parts) {
    rec.u16(S(P.name)); rec.u16(S(P.channel)); rec.u8(LAYERS.indexOf(P.layer)); rec.u8(ANCHORS.indexOf(P.anchor)); rec.u8(P.side);
    rec.i8(P.z); rec.u8(P.sway); rec.u8(P.bodyMask);
    rec.u8(P.roles.length); for (const r of P.roles) { rec.u8(r.cat); rec.u16(S(r.value)); }
    rec.u8(P.tags.length); for (const t of P.tags) rec.u16(S(t));
    rec.u32(P.firstView); rec.u16(P.nViews);
  }
  rec.u32(viewRecs.length);
  for (const { V, pix, reg, bld, firstMark } of viewRecs) {
    rec.u8(V.facing); rec.u16(S(V.key)); rec.u16(V.when ? S(V.when) : 0);
    rec.u8(V.layer ? LAYERS.indexOf(V.layer) : 255); rec.u8(V.anchor ? ANCHORS.indexOf(V.anchor) : 255);
    rec.i16(V.origin ? V.origin[0] : 0); rec.i16(V.origin ? V.origin[1] : 0); rec.u16(V.w); rec.u16(V.h); rec.i16(V.stretch); rec.u8(V.replace ? 1 : 0);
    rec.u32(pix); rec.u32(reg); rec.u32(bld); rec.u32(firstMark); rec.u8(V.marks.length);
  }
  rec.u32(markRecs.length);
  for (const m of markRecs) { rec.u16(S(m.name)); rec.i16(m.x); rec.i16(m.y); }
  rec.u32(bodySets.length);
  for (const B of bodySets) {
    rec.u16(S(B.name));
    for (let fc = 0; fc < 4; fc++) { rec.u8(B.anchors[fc].length); for (const a of B.anchors[fc]) { rec.u8(a.id); rec.i16(a.x); rec.i16(a.y); } }
  }
  let frameCount = 0;
  rec.u32(clips.length);
  for (const c of clips) {
    rec.u16(S(c.body)); rec.u8(c.clip); rec.u8(c.part); rec.u8(c.facing); rec.u8(c.family); rec.u8(c.variant); rec.u8(c.posture);
    rec.u16(c.cycle); rec.u32(frameCount); rec.u16(c.frames.length);
    frameCount += c.frames.length;
  }
  rec.u32(frameCount);
  for (const c of clips) for (const fr of c.frames) {
    rec.u8(fr.keys.length); for (const k of fr.keys) { rec.u16(S(k.channel)); rec.u16(S(k.key)); }
    rec.i8(fr.bob); rec.i8(fr.lean); rec.i8(fr.hip[0]); rec.i8(fr.hip[1]); rec.i8(fr.head[0]); rec.i8(fr.head[1]); rec.i8(fr.sway);
    rec.u16(fr.ms); rec.i16(fr.weapon); rec.u8(fr.front); rec.i16(fr.plant[0]); rec.i16(fr.plant[1]);
    rec.u8(fr.turn); rec.i8(fr.at[0]); rec.i8(fr.at[1]);
  }
  // strings, then the records, then the blob
  w.u32(strings.length);
  for (const s of strings) { const b = Buffer.from(s, 'utf8'); w.u16(b.length); w.bytes(b); }
  w.bytes(rec.b);
  w.u32(blob.length);
  w.bytes(blob);
  const pool = Buffer.from(w.b);
  pool.writeUInt32LE(pool.length, 8);
  const hash = crypto.createHash('sha256').update(pool).digest('hex').slice(0, 16);
  return { pool, hash, views: viewRecs.length, marks: markRecs.length, frames: frameCount, opaquePx, cellCount, blobBytes: blob.length };
}

function cppSource(parts, clips, pk) {
  const { pool, hash } = pk;
  const out = [];
  out.push('// GENERATED by tools/doll/pxc.mjs from rpg/art/doll/maps and rpg/art/doll/clips: do not edit. Rebuild with');
  out.push('// `node tools/doll/pxc.mjs`; CI runs `node tools/doll/pxc.mjs --check` and fails when this file is stale.');
  out.push(`// ${parts.length} parts, ${pk.views} views, ${clips.length} clips, ${pk.opaquePx} opaque px in ${pk.cellCount} cells; pool ${pool.length} bytes; hash ${hash}`);
  out.push('#include <cstddef>');
  out.push('#include <cstdint>');
  out.push('namespace art { namespace doll { namespace gen {');
  const n = Math.max(1, Math.ceil(pool.length / CHUNK));
  for (let c = 0; c < n; c++) {
    const part = pool.subarray(c * CHUNK, Math.min(pool.length, (c + 1) * CHUNK));
    out.push(`static const unsigned char kPool${c}[] = {`);
    for (let i = 0; i < part.length; i += 40) out.push(Array.from(part.subarray(i, i + 40)).join(',') + ',');
    out.push('};');
  }
  out.push(`extern const unsigned char* const kPoolChunks[] = {${Array.from({ length: n }, (_, c) => `kPool${c}`).join(', ')}};`);
  out.push(`extern const size_t kPoolChunkSizes[] = {${Array.from({ length: n }, (_, c) => `sizeof(kPool${c})`).join(', ')}};`);
  out.push(`extern const size_t kPoolChunkCount = ${n};`);
  out.push(`extern const char kPoolHash[] = "${hash}";`);
  out.push('}}}  // namespace art::doll::gen');
  return out.join('\n') + '\n';
}

// ---------------------------------------------------------------------------------------------- one compile
function compile() {
  const t0 = process.hrtime.bigint();
  const errors = [], warnings = [];
  const global = { errors, warnings };
  sink = global;
  loadCache();
  const { legends, sig: legendSig } = readLegends();
  const stamps = readStamps(legends);
  const files = walk(MAPS, '.pxm');
  const parts = [], left = [];
  let parsed = 0;
  const live = new Set();
  for (const f of files) {
    const r = rel(f);
    live.add(r);
    const text = fs.readFileSync(f, 'utf8');
    const sig = sha1(legendSig + '\n' + stamps.sig + '\n' + text);
    const hit = cache.files[r];
    let P, msgs;
    if (hit && hit.sig === sig) ({ P, msgs } = deserialize(hit.entry));
    else {
      sink = { errors: [], warnings: [] };
      P = parsePart(f, text);
      buildPart(P, legends, stamps);
      msgs = sink;
      sink = global;
      cache.files[r] = { sig, entry: serialize(P, msgs) };
      parsed++;
    }
    errors.push(...msgs.errors);
    warnings.push(...msgs.warnings);
    // a map with an error is left out of the pool (and named): one area's broken map never stops the others compiling;
    // the run still fails (exit 1) and --check never passes while it is broken
    if (msgs.errors.length) left.push(r); else parts.push(P);
  }
  const mapErrors = errors.length;
  for (const k of Object.keys(cache.files)) if (!live.has(k)) delete cache.files[k];
  saveCache();
  const byName = new Map();
  for (const P of parts) {
    if (byName.has(P.name)) err(P.file, `part name ${P.name} also used by ${byName.get(P.name).file}`);
    byName.set(P.name, P);
  }
  const { bodySets, clips } = readClips();
  warnings.push(...facingChecks(parts, clips));
  const pk = pack(parts, bodySets, clips);
  const src = cppSource(parts, clips, pk);
  for (const x of warnings) if (opt.strict && !x.draft) errors.push(`${x.where}: ${x.msg} (--strict)`);
  const ms = Number(process.hrtime.bigint() - t0) / 1e6;
  return { errors, warnings, parts, bodySets, clips, pk, src, ms, parsed, files: files.length, left, globalErrors: errors.length - mapErrors, stamps };
}

// every put of the compiled parts (the judge's boot tracer: --placements), one record each
function placements(c) {
  const out = [];
  for (const P of c.parts) for (const V of P.views) for (const pt of V.puts || []) {
    out.push({ part: P.name, file: P.file, facing: FACINGS[V.facing], key: V.key, when: V.when || '', stamp: pt.name, dx: pt.dx, dy: pt.dy,
      under: !!pt.under, cols: pt.cols || null, line: pt.line });
  }
  return out;
}
function writePlacements(c) {
  if (!opt.placements) return;
  const recs = placements(c);
  fs.mkdirSync(path.dirname(opt.placements), { recursive: true });
  fs.writeFileSync(opt.placements, recs.map((r) => JSON.stringify(r)).join('\n') + (recs.length ? '\n' : ''));
  if (!opt.quiet) console.log(`pxc: ${recs.length} placement(s) -> ${opt.placements}`);
}

function report(c) {
  console.log(`pxc report: ${rel(MAPS)} and ${rel(CLIPDIR)}`);
  for (const P of c.parts) {
    const vs = P.views.filter((v) => v.cells);
    const px = vs.reduce((s, v) => s + v.opaque, 0);
    const orph = vs.reduce((s, v) => s + (v.checks ? v.checks.orphans : 0), 0);
    const noise = vs.reduce((s, v) => s + (v.checks ? v.checks.noise || 0 : 0), 0);
    console.log(`  ${P.name.padEnd(36)} ${String(vs.length).padStart(3)} views ${String(px).padStart(5)} px  orphans ${orph}  noise ${noise}  roles ${P.roles.map((r) => ROLES[r.cat] + ' ' + r.value).join(', ') || '-'}`);
  }
  // the stamp libraries and each entry's uses (puts in the compiled parts)
  const uses = new Map();
  for (const r of placements(c)) {
    const P = c.parts.find((q) => q.name === r.part);
    const lib = P.stamps.find((n) => c.stamps.libs[n] && c.stamps.libs[n].entries.has(`${r.stamp}/${r.facing}`));
    const id = `${lib}/${r.stamp}/${r.facing}`;
    if (!uses.has(id)) uses.set(id, new Set());
    uses.get(id).add(r.part);
    uses.set(id + '#n', (uses.get(id + '#n') || 0) + 1);
  }
  for (const lib of Object.values(c.stamps.libs)) {
    console.log(`  stamps ${lib.name} (${lib.file}): ${lib.entries.size} entries`);
    for (const st of lib.entries.values()) {
      const id = `${lib.name}/${st.name}/${FACINGS[st.facing]}`;
      const n = uses.get(id + '#n') || 0;
      console.log(`    ${(st.name + ' ' + FACINGS[st.facing]).padEnd(8)} ${String(st.w || 0).padStart(2)}x${String(st.h || 0).padEnd(2)} origin ${st.ox},${st.oy}  ${String(st.opaque || 0).padStart(3)} px  orphans ${st.checks ? st.checks.orphans : 0}  noise ${st.checks ? st.checks.noise : 0}  uses ${String(n).padStart(3)}${n ? ' (' + [...uses.get(id)].join(', ') + ')' : ' (unused)'}`);
    }
  }
  for (const x of c.warnings) console.log(`  warning ${x.where}: ${x.msg}${x.draft ? ' (draft)' : ''}`);
  const { pk } = c;
  console.log(`pool: ${c.parts.length} parts, ${pk.views} views, ${pk.marks} marks, ${c.bodySets.length} body sets, ${c.clips.length} clips, ${pk.frames} frames`);
  console.log(`pool: ${pk.opaquePx} opaque px in ${pk.cellCount} cells; ${pk.pool.length} bytes (${(pk.pool.length / 1048576).toFixed(3)} MB of the 1.0 MB budget); pixel data ${pk.blobBytes} bytes; ${Math.max(1, Math.ceil(pk.pool.length / CHUNK))} chunk(s); hash ${pk.hash}`);
  console.log(`generated source: ${(Buffer.byteLength(c.src) / 1048576).toFixed(2)} MB; compiled in ${c.ms.toFixed(0)} ms (${c.parsed} of ${c.files} maps parsed, the rest from the cache)`);
}

function writeOutputs(c, binPath) {
  if (opt.cpp) {
    fs.mkdirSync(path.dirname(OUT), { recursive: true });
    const have = fs.existsSync(OUT) ? fs.readFileSync(OUT, 'utf8') : '';
    if (have !== c.src) {   // (unchanged: left alone, so a build does not recompile it; renamed over: never half written)
      const tmp = `${OUT}.${process.pid}.tmp`;
      fs.writeFileSync(tmp, c.src);
      try { fs.renameSync(tmp, OUT); } catch { fs.writeFileSync(OUT, c.src); fs.rmSync(tmp, { force: true }); }
    }
  }
  if (binPath) { fs.mkdirSync(path.dirname(binPath), { recursive: true }); fs.writeFileSync(binPath, c.pk.pool); }
}

// ---------------------------------------------------------------------------------------------- main
function once() {
  const c = compile();
  if (opt.report) report(c);
  writePlacements(c);
  for (const e of c.errors) console.error(`error ${e}`);
  if (!opt.report && !opt.quiet && c.warnings.length) console.log(`pxc: ${c.warnings.length} craft warning(s) (--report lists them)`);
  if (c.errors.length && (c.globalErrors || opt.check)) { console.error(`pxc: ${c.errors.length} error(s); nothing written`); return 1; }
  if (c.errors.length) {
    writeOutputs(c, opt.bin);
    console.error(`pxc: ${c.errors.length} error(s) in ${c.left.length} map(s), LEFT OUT of the pool until fixed: ${c.left.join(' ')}; the other ${c.parts.length} parts written (${c.pk.pool.length} bytes, hash ${c.pk.hash})`);
    return 1;
  }
  if (opt.check) {
    const have = fs.existsSync(OUT) ? fs.readFileSync(OUT, 'utf8').replace(/\r\n/g, '\n') : '';
    if (have !== c.src) { console.error(`pxc --check: ${path.relative(REPO, OUT)} is stale: run node tools/doll/pxc.mjs`); return 1; }
    if (!opt.quiet) console.log(`pxc --check: ${path.relative(REPO, OUT)} is up to date (${c.parts.length} parts, ${c.pk.pool.length} bytes, hash ${c.pk.hash})`);
    if (opt.bin) writeOutputs({ ...c, src: null }, opt.bin);
    return 0;
  }
  writeOutputs(c, opt.bin);
  if (!opt.quiet) console.log(`pxc: ${c.parts.length} parts, ${c.pk.views} views, ${c.clips.length} clips -> ${opt.cpp ? path.relative(REPO, OUT) : '(no source)'}${opt.bin ? ' and ' + opt.bin : ''} (${c.pk.pool.length} bytes, hash ${c.pk.hash}; ${c.ms.toFixed(0)} ms, ${c.parsed} of ${c.files} maps parsed)`);
  return 0;
}

if (!opt.watch) {
  if (opt.check) opt.cpp = false;
  process.exit(once());
} else {
  // the look-and-adjust loop: compile on every save (debounced), then re-render the gallery from the new pool
  const bin = opt.bin || (opt.gallery ? path.join(os.tmpdir(), 'embervale-pxc', 'watch.pool') : null);
  let timer = null, busy = false, again = false;
  const run = () => {
    if (busy) { again = true; return; }
    busy = true;
    try {
      const c = compile();
      const stamp = new Date().toLocaleTimeString();
      for (const e of c.errors) console.error(`error ${e}`);
      if (c.errors.length && c.globalErrors) console.error(`[${stamp}] pxc: ${c.errors.length} error(s); nothing written`);
      else {
        if (c.errors.length) console.error(`[${stamp}] pxc: ${c.left.length} map(s) LEFT OUT until fixed: ${c.left.join(' ')}`);
        writeOutputs(c, bin);
        console.log(`[${stamp}] pxc: ${c.parts.length} parts, ${c.clips.length} clips, ${c.pk.pool.length} bytes; ${c.ms.toFixed(0)} ms (${c.parsed} map(s) parsed); ${c.warnings.length} warning(s)`);
        if (opt.gallery) {
          const g = spawnSync(opt.gallery, [opt.galleryDir, '--pool', bin, ...opt.boards], { encoding: 'utf8' });
          if (g.error) console.error(`pxc --watch: the gallery did not run: ${g.error.message}`);
          else console.log(`[${stamp}] doll_gallery: ${(g.stdout || '').split('\n').filter((l) => l.startsWith('wrote')).length} board image(s) in ${opt.galleryDir}`);
        }
      }
    } catch (e) { console.error(`pxc --watch: ${e.stack || e}`); }
    busy = false;
    if (again) { again = false; run(); }
  };
  const poke = () => { clearTimeout(timer); timer = setTimeout(run, 150); };
  run();
  for (const d of [MAPS, CLIPDIR]) {
    if (!fs.existsSync(d)) fs.mkdirSync(d, { recursive: true });
    fs.watch(d, { recursive: true }, poke);
  }
  console.log(`pxc --watch: watching ${rel(MAPS)} and ${rel(CLIPDIR)} (Ctrl+C ends it)`);
}
