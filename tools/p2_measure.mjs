#!/usr/bin/env node
// p2_measure.mjs (P2a Clear Sight, VISION_PLAN 15.36 item 10): the judge's measurements on DEVICE-resolution shots (the
// Witness `call shot <file>` / script `shot` PNGs, never the canvas-sampled Witness screenshot). Node 18, no dependencies
// (node:zlib only). Every command prints one line per measurement; --json prints one JSON object instead.
//
//   node tools/p2_measure.mjs diff A.png B.png                      pixels that differ, their box
//   node tools/p2_measure.mjs mask WITH.png WITHOUT.png [--d D]     the figure mask of a shot pair (with / without the
//                                                                   hero: `hideplayer`): box, height in device px, and in
//                                                                   art px with --d (the world scale D)
//   node tools/p2_measure.mjs blocks SHOT.png --d D [--rect x,y,w,h] every world pixel an exact D x D block? (the best
//                                                                   of the D x D alignments; L2, slice check 2)
//   node tools/p2_measure.mjs contrast WITH.png WITHOUT.png [--d D] figure-ground: the outline ring vs the ground ring
//                                                                   (edge contrast), the figure's median luma vs the
//                                                                   ground's (L6, slice check 6)
//   node tools/p2_measure.mjs ring ON.png OFF.png                   the ring's pixels (a pair with `ring 1` / `ring 0`)
//                                                                   against the ground round them (L7)
//   node tools/p2_measure.mjs subset WITH.png WITHOUT.png FRAME.png [--knee ROWS]
//                                                                   the colour-subset check: every figure pixel (above the
//                                                                   knee: the mask's top ROWS, default 70 %) is a colour
//                                                                   of the baked frame (FRAME.png, e.g. a gallery cell) or
//                                                                   the ground (WITHOUT) at that pixel (slice check 2)
//   node tools/p2_measure.mjs luma SHOT.png --rect x,y,w,h          median / mean luma of a rectangle
// Luma: 0.299 R + 0.587 G + 0.114 B (0..255).
import fs from "node:fs";
import zlib from "node:zlib";

// ---------------------------------------------------------------- PNG decode (8-bit RGB / RGBA / grey / palette)
function readPng(path) {
  const b = fs.readFileSync(path);
  const sig = [137, 80, 78, 71, 13, 10, 26, 10];
  for (let i = 0; i < 8; i++) if (b[i] !== sig[i]) throw new Error(`${path}: not a PNG`);
  let p = 8, w = 0, h = 0, depth = 0, ctype = 0, inter = 0, plte = null, trns = null;
  const idat = [];
  while (p < b.length) {
    const len = b.readUInt32BE(p), type = b.toString("ascii", p + 4, p + 8), data = b.subarray(p + 8, p + 8 + len);
    if (type === "IHDR") { w = data.readUInt32BE(0); h = data.readUInt32BE(4); depth = data[8]; ctype = data[9]; inter = data[12]; }
    else if (type === "PLTE") plte = data;
    else if (type === "tRNS") trns = data;
    else if (type === "IDAT") idat.push(data);
    else if (type === "IEND") break;
    p += 12 + len;
  }
  if (depth !== 8 || inter !== 0) throw new Error(`${path}: only 8-bit, non-interlaced PNGs (depth ${depth}, interlace ${inter})`);
  const ch = { 0: 1, 2: 3, 3: 1, 4: 2, 6: 4 }[ctype];
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const stride = w * ch, out = new Uint8Array(w * h * 4), prev = new Uint8Array(stride), cur = new Uint8Array(stride);
  let q = 0;
  for (let y = 0; y < h; y++) {
    const f = raw[q++];
    for (let x = 0; x < stride; x++) {
      const a = x >= ch ? cur[x - ch] : 0, up = prev[x], c = x >= ch ? prev[x - ch] : 0;
      let v = raw[q++];
      if (f === 1) v += a;
      else if (f === 2) v += up;
      else if (f === 3) v += (a + up) >> 1;
      else if (f === 4) { const pp = a + up - c, pa = Math.abs(pp - a), pb = Math.abs(pp - up), pc = Math.abs(pp - c); v += pa <= pb && pa <= pc ? a : pb <= pc ? up : c; }
      cur[x] = v & 255;
    }
    for (let x = 0; x < w; x++) {
      const o = (y * w + x) * 4;
      if (ctype === 2 || ctype === 6) { out[o] = cur[x * ch]; out[o + 1] = cur[x * ch + 1]; out[o + 2] = cur[x * ch + 2]; out[o + 3] = ctype === 6 ? cur[x * ch + 3] : 255; }
      else if (ctype === 0 || ctype === 4) { out[o] = out[o + 1] = out[o + 2] = cur[x * ch]; out[o + 3] = ctype === 4 ? cur[x * ch + 1] : 255; }
      else { const i = cur[x]; out[o] = plte[i * 3]; out[o + 1] = plte[i * 3 + 1]; out[o + 2] = plte[i * 3 + 2]; out[o + 3] = trns && i < trns.length ? trns[i] : 255; }
    }
    prev.set(cur);
  }
  return { w, h, px: out, path };
}
const luma = (img, x, y) => { const o = (y * img.w + x) * 4; return 0.299 * img.px[o] + 0.587 * img.px[o + 1] + 0.114 * img.px[o + 2]; };
const rgbAt = (img, x, y) => { const o = (y * img.w + x) * 4; return (img.px[o] << 16) | (img.px[o + 1] << 8) | img.px[o + 2]; };
const median = (a) => { if (!a.length) return NaN; const s = Float64Array.from(a).sort(); return s.length & 1 ? s[s.length >> 1] : (s[s.length / 2 - 1] + s[s.length / 2]) / 2; };
const r1 = (v) => Math.round(v * 10) / 10;

// ---------------------------------------------------------------- args
const args = process.argv.slice(2);
const cmd = args.shift();
const opt = {}, pos = [];
for (let i = 0; i < args.length; i++) {
  if (args[i].startsWith("--")) { const k = args[i].slice(2), n = args[i + 1]; if (n === undefined || n.startsWith("--")) opt[k] = true; else { opt[k] = n; i++; } }
  else pos.push(args[i]);
}
const D = opt.d ? Number(opt.d) : 0;
const rect = (img) => {
  if (!opt.rect) return { x: 0, y: 0, w: img.w, h: img.h };
  const [x, y, w, h] = String(opt.rect).split(",").map(Number);
  return { x, y, w, h };
};
const out = {};
const say = (k, v, text) => { out[k] = v; if (!opt.json) console.log(text ?? `${k}: ${typeof v === "object" ? JSON.stringify(v) : v}`); };
const same = (a, b) => { if (a.w !== b.w || a.h !== b.h) throw new Error(`size differs: ${a.w}x${a.h} vs ${b.w}x${b.h}`); };

// the figure mask of a pair: pixels that differ, kept to the largest connected blob (8-way), so a distant flicker
// (a torch, a bird) never joins the figure
function figureMask(a, b) {
  same(a, b);
  const n = a.w * a.h, m = new Uint8Array(n);
  for (let i = 0; i < n; i++) {
    const o = i * 4;
    if (a.px[o] !== b.px[o] || a.px[o + 1] !== b.px[o + 1] || a.px[o + 2] !== b.px[o + 2]) m[i] = 1;
  }
  const lab = new Int32Array(n).fill(-1);
  let best = -1, bestN = 0, id = 0;
  const stack = [];
  for (let i = 0; i < n; i++) {
    if (!m[i] || lab[i] >= 0) continue;
    let cnt = 0;
    stack.push(i); lab[i] = id;
    while (stack.length) {
      const j = stack.pop(); cnt++;
      const x = j % a.w, y = (j / a.w) | 0;
      for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
        const X = x + dx, Y = y + dy;
        if (X < 0 || Y < 0 || X >= a.w || Y >= a.h) continue;
        const k = Y * a.w + X;
        if (m[k] && lab[k] < 0) { lab[k] = id; stack.push(k); }
      }
    }
    if (cnt > bestN) { bestN = cnt; best = id; }
    id++;
  }
  let x0 = 1e9, y0 = 1e9, x1 = -1, y1 = -1;
  for (let i = 0; i < n; i++) {
    m[i] = lab[i] === best && best >= 0 ? 1 : 0;
    if (m[i]) { const x = i % a.w, y = (i / a.w) | 0; x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y); }
  }
  return { m, n: bestN, blobs: id, box: x1 >= 0 ? [x0, y0, x1, y1] : null, w: a.w, h: a.h };
}
// the mask's outline ring (mask pixels with a non-mask 4-neighbour) and the ground ring (non-mask pixels within `r` px)
function rings(M, r = 1) {
  const inner = [], ground = [];
  const at = (x, y) => x >= 0 && y >= 0 && x < M.w && y < M.h && M.m[y * M.w + x] === 1;
  if (!M.box) return { inner, ground };
  const [x0, y0, x1, y1] = M.box;
  for (let y = Math.max(0, y0 - r); y <= Math.min(M.h - 1, y1 + r); y++)
    for (let x = Math.max(0, x0 - r); x <= Math.min(M.w - 1, x1 + r); x++) {
      if (at(x, y)) { if (!at(x - 1, y) || !at(x + 1, y) || !at(x, y - 1) || !at(x, y + 1)) inner.push([x, y]); continue; }
      let near = false;
      for (let dy = -r; dy <= r && !near; dy++) for (let dx = -r; dx <= r && !near; dx++) near = at(x + dx, y + dy);
      if (near) ground.push([x, y]);
    }
  return { inner, ground };
}

try {
  if (cmd === "diff") {
    const a = readPng(pos[0]), b = readPng(pos[1]);
    same(a, b);
    let n = 0, x0 = 1e9, y0 = 1e9, x1 = -1, y1 = -1;
    for (let y = 0; y < a.h; y++) for (let x = 0; x < a.w; x++)
      if (rgbAt(a, x, y) !== rgbAt(b, x, y)) { n++; x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y); }
    say("differing", n, `differing pixels: ${n} of ${a.w * a.h}`);
    say("box", n ? [x0, y0, x1, y1] : null);
  } else if (cmd === "mask") {
    const M = figureMask(readPng(pos[0]), readPng(pos[1]));
    if (!M.box) throw new Error("the shots are identical: no figure");
    const [x0, y0, x1, y1] = M.box, hd = y1 - y0 + 1, wd = x1 - x0 + 1;
    say("box", M.box);
    say("heightDevice", hd, `figure height: ${hd} device px, width ${wd} (${M.n} px; ${M.blobs} blob(s), the largest kept)`);
    say("shareOfScreen", r1((100 * hd) / M.h), `share of the screen height: ${r1((100 * hd) / M.h)} %`);
    if (D) say("heightArt", r1(hd / D), `figure height: ${r1(hd / D)} art px at D ${D}`);
  } else if (cmd === "blocks") {
    if (!D) throw new Error("blocks needs --d D");
    const img = readPng(pos[0]), R = rect(img);
    let best = null;
    for (let oy = 0; oy < D; oy++) for (let ox = 0; ox < D; ox++) {
      let blocks = 0, whole = 0;
      for (let by = R.y + oy; by + D <= R.y + R.h; by += D) for (let bx = R.x + ox; bx + D <= R.x + R.w; bx += D) {
        blocks++;
        const c = rgbAt(img, bx, by);
        let ok = true;
        for (let y = 0; y < D && ok; y++) for (let x = 0; x < D && ok; x++) ok = rgbAt(img, bx + x, by + y) === c;
        whole += ok ? 1 : 0;
      }
      if (!best || whole / blocks > best.share) best = { ox, oy, blocks, whole, share: blocks ? whole / blocks : 0 };
    }
    const colours = new Set();
    for (let y = R.y; y < R.y + R.h; y++) for (let x = R.x; x < R.x + R.w; x++) colours.add(rgbAt(img, x, y));
    say("wholeBlocks", r1(best.share * 100), `D x D blocks whole: ${r1(best.share * 100)} % (${best.whole} of ${best.blocks}, alignment ${best.ox},${best.oy})`);
    say("colours", colours.size, `colours in the area: ${colours.size}`);
  } else if (cmd === "contrast") {
    const a = readPng(pos[0]), b = readPng(pos[1]);
    const M = figureMask(a, b);
    if (!M.box) throw new Error("the shots are identical: no figure");
    const ring = Math.max(1, D || 1);
    const { inner, ground } = rings(M, ring);
    const ol = median(inner.map(([x, y]) => luma(a, x, y))), gr = median(ground.map(([x, y]) => luma(a, x, y)));
    const fig = [], grd = [];
    for (let y = M.box[1]; y <= M.box[3]; y++) for (let x = M.box[0]; x <= M.box[2]; x++) {
      if (M.m[y * M.w + x]) { fig.push(luma(a, x, y)); grd.push(luma(b, x, y)); }
    }
    say("edgeContrast", r1(Math.abs(ol - gr)), `edge contrast: ${r1(Math.abs(ol - gr))} (outline ring median ${r1(ol)} vs ground ring ${r1(gr)})`);
    say("figureMedian", r1(median(fig)), `figure median luma ${r1(median(fig))} vs the ground under it ${r1(median(grd))}: ${r1(median(fig) - median(grd))}`);
    say("figureOverGround", r1(median(fig) - median(grd)));
  } else if (cmd === "ring") {
    const a = readPng(pos[0]), b = readPng(pos[1]);
    const M = figureMask(a, b);
    if (!M.box) throw new Error("the shots are identical: no ring");
    const ringL = [], under = [];
    for (let i = 0; i < M.m.length; i++) if (M.m[i]) { const x = i % M.w, y = (i / M.w) | 0; ringL.push(luma(a, x, y)); under.push(luma(b, x, y)); }
    const c = median(ringL) - median(under);
    say("ringContrast", r1(Math.abs(c)), `ring contrast: ${r1(Math.abs(c))} (ring median ${r1(median(ringL))} vs the ground under it ${r1(median(under))}; ${M.n} px, box ${M.box})`);
  } else if (cmd === "subset") {
    const a = readPng(pos[0]), b = readPng(pos[1]), f = readPng(pos[2]);
    const M = figureMask(a, b);
    if (!M.box) throw new Error("the shots are identical: no figure");
    const pal = new Set();
    for (let y = 0; y < f.h; y++) for (let x = 0; x < f.w; x++) if (f.px[(y * f.w + x) * 4 + 3] > 0) pal.add(rgbAt(f, x, y));
    const rows = opt.knee ? Number(opt.knee) : Math.round((M.box[3] - M.box[1] + 1) * 0.7);
    let n = 0, bad = 0;
    const strays = new Map();
    for (let y = M.box[1]; y < M.box[1] + rows; y++) for (let x = M.box[0]; x <= M.box[2]; x++) {
      if (!M.m[y * M.w + x]) continue;
      n++;
      const c = rgbAt(a, x, y);
      if (pal.has(c) || c === rgbAt(b, x, y)) continue;
      bad++;
      strays.set(c, (strays.get(c) || 0) + 1);
    }
    say("strayPixels", bad, `colours outside the frame's and the ground's: ${bad} of ${n} figure px above the knee (${strays.size} colours)`);
    if (strays.size) say("strayColours", [...strays.entries()].slice(0, 8).map(([c, k]) => `#${c.toString(16).padStart(6, "0")} x${k}`));
  } else if (cmd === "luma") {
    const img = readPng(pos[0]), R = rect(img), v = [];
    for (let y = R.y; y < R.y + R.h; y++) for (let x = R.x; x < R.x + R.w; x++) v.push(luma(img, x, y));
    say("median", r1(median(v)), `luma median ${r1(median(v))}, mean ${r1(v.reduce((s, x) => s + x, 0) / Math.max(1, v.length))}`);
  } else {
    console.log("usage: node tools/p2_measure.mjs diff|mask|blocks|contrast|ring|subset|luma ... (see the file's header)");
    process.exit(2);
  }
  if (opt.json) console.log(JSON.stringify(out));
} catch (e) {
  console.error(`p2_measure: ${e.message}`);
  process.exit(1);
}
