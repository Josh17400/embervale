#!/usr/bin/env node
// tools/doll/rekey_variants.mjs (P2c envelope, FORGE-DOLL; docs/P2C_CONTRACTS.md 3d, D51): re-keys the fix round's mass-keyed
// helm variant views (`view ... when helmfam-<v>[,alloyshape],<mass terms> replace`) to the variant the resolver now names
// (`when helmfam-<v>,variant-<n> replace`), their pixels unchanged. The numbering is rpg/art/doll/doll_forge.h's
// (legacyHelmVariant): the resolver gives every look that is not a culture alloy's helm the variant its mass drew, and the
// owner's 37 their kBoardHeads variant, so the re-key moves only the board's swapped hats (Papiron the great kasa, Jaonbrass
// the gat) and the look-alikes kBoardHeads parts.
//   node tools/doll/rekey_variants.mjs [--root DIR] [--check] [--write]
// DIR is a doll root (default rpg/art/doll); maps/headwear/helm/*.pxm are read. Without --write it prints what it would change.
// FORGE-HEAD owns the helm maps: it runs this on its own root (and the integrator on the repo at integration).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const REPO = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const args = process.argv.slice(2);
const opt = { root: path.join(REPO, 'rpg/art/doll'), write: args.includes('--write'), check: args.includes('--check') };
const ri = args.indexOf('--root');
if (ri >= 0 && args[ri + 1]) opt.root = path.resolve(args[ri + 1]);

const MASSES = ['none', 'round', 'broad', 'tall', 'drop', 'mantle'];
// family -> [variant, masses, needs alloyshape]
const TABLE = {
  warhat: [[1, ['broad', 'tall'], true], [2, ['none', 'mantle'], true]],
  great: [[1, ['none', 'round', 'mantle'], true], [2, ['broad', 'tall', 'drop'], true]],
  crested: [[1, ['none', 'mantle'], true]],
  crown: [[1, ['none', 'mantle', 'tall'], true]],
  plumed: [[1, ['round'], false], [2, ['none', 'mantle'], true], [3, ['broad', 'tall'], true], [4, ['drop'], false]],
  snout: [[1, ['none', 'mantle'], true], [2, ['tall', 'drop'], true], [3, ['broad', 'round'], true]],
  veil: [[1, ['drop', 'tall'], true], [2, ['none', 'round', 'broad', 'mantle'], true]],
  winged: [[1, ['none', 'mantle'], true], [2, ['tall'], false], [3, ['round', 'drop'], true]],
  kabuto: [[1, ['broad', 'drop'], true]],
  onion: [[1, ['none', 'round', 'broad', 'mantle'], true]],
};
const same = (a, b) => a.length === b.length && a.every((x) => b.includes(x));

const dirs = [path.join(opt.root, 'maps/headwear/helm'), path.join(opt.root, 'maps/headwear/helm/crest')];
let changed = 0, files = 0, unknown = 0;
const list = [];
for (const d of dirs) if (fs.existsSync(d)) for (const n of fs.readdirSync(d).filter((x) => x.endsWith('.pxm')).sort()) list.push(path.join(d, n));
for (const p of list) {
  const f = path.basename(p);
  const src = fs.readFileSync(p, 'utf8');
  const nl = src.includes('\r\n') ? '\r\n' : '\n';
  let n = 0;
  const seen = new Set();   // a view's facing, key and new condition: a duplicate keeps its mass terms (a distinct view, never the winner)
  const out = src.split(/\r?\n/).map((line) => {
    const m = line.match(/^(view .* when )([^ ]+)( replace.*)$/);
    if (!m) return line;
    const terms = m[2].split(',');
    const fam = terms.find((t) => t.startsWith('helmfam-'));
    if (!fam || !terms.some((t) => /^!?mass-/.test(t))) return line;
    if (terms.some((t) => t.startsWith('variant-'))) return line;   // already re-keyed (a duplicate that kept its mass terms): idempotent
    const v = fam.slice(8);
    const pos = terms.filter((t) => t.startsWith('mass-')).map((t) => t.slice(5));
    const neg = terms.filter((t) => t.startsWith('!mass-')).map((t) => t.slice(6));
    const set = pos.length ? pos : MASSES.filter((x) => !neg.includes(x));
    // a whole variant's masses (the helm's views), or one mass (a crest's placement on that variant: crest/crown, crest/spike)
    const hit = (TABLE[v] || []).find(([, ms]) => same(ms, set)) || (set.length === 1 ? (TABLE[v] || []).find(([, ms]) => ms.includes(set[0])) : undefined);
    if (!hit) { unknown++; console.log(`  ${f}: no variant for ${m[2]} (masses ${set.join(' ')})`); return line; }
    const rest = terms.filter((t) => t !== fam && t !== 'alloyshape' && !/^!?mass-/.test(t));
    n++;
    let cond = [fam, `variant-${hit[0]}`, ...rest].join(',');
    const vk = (line.match(/^view (\w+) key (\w+)/) || []).slice(1).join(' ');
    if (seen.has(vk + ' ' + cond)) cond = [fam, `variant-${hit[0]}`, ...terms.filter((t) => /^!?mass-/.test(t)), ...rest].join(',');
    seen.add(vk + ' ' + cond);
    return m[1] + cond + m[3];
  });
  if (!n) continue;
  files++;
  changed += n;
  console.log(`  ${path.relative(opt.root, p)}: ${n} view(s) re-keyed`);
  if (opt.write) fs.writeFileSync(p, out.join(nl), 'utf8');
}
console.log(`rekey_variants: ${changed} view(s) in ${files} file(s)${opt.write ? ' written' : ' (dry run: --write to apply)'}; ${unknown} unmatched`);
if (opt.check && changed) process.exit(1);
