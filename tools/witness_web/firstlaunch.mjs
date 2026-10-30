// tools/witness_web/firstlaunch.mjs (P2c flip, R8; PLAN "The flip (wave 3)" item 18): the web build's first launch as a
// player meets it, with no Witness: Chromium as an "iPhone 15 Pro landscape" (touch, a fresh context, no saved state),
// site-witness served on 127.0.0.1:8090 WITHOUT ?witness, New Game started from the title (N), the creator walked through
// with Enter (each tab, then BEGIN), 3 s waited, one screenshot (the viewport, under 8,000 px).
//   node tools/witness_web/firstlaunch.mjs [--site site-witness] [--out firstlaunch.png] [--query rig=1] [--port 8090]
// --query k=v adds URL params (rig=1: the v1 fallback, opt-in until the owner's retirement OK). When nothing listens on the
// port, the site folder is served here (a plain static server) for the run.
import { createServer } from 'node:http';
import { readFile, stat } from 'node:fs/promises';
import { extname, join, resolve } from 'node:path';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { chromium, devices } = require('playwright');

const args = process.argv.slice(2);
const opt = (name, def) => { const i = args.indexOf(name); return i >= 0 && i + 1 < args.length ? args[i + 1] : def; };
const site = resolve(opt('--site', 'site-witness'));
const out = resolve(opt('--out', 'firstlaunch.png'));
const port = Number(opt('--port', '8090'));
const query = args.flatMap((a, i) => (a === '--query' && i + 1 < args.length ? [args[i + 1]] : []));

const types = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.css': 'text/css', '.png': 'image/png',
  '.json': 'application/json', '.data': 'application/octet-stream', '.ico': 'image/x-icon', '.webmanifest': 'application/manifest+json' };

// 'site': the port serves a page with a canvas (the game); 'other': something else holds the port; 'free': nothing listens
async function listening() {
  try {
    const r = await fetch(`http://127.0.0.1:${port}/`);
    return r.ok && /<canvas/i.test(await r.text()) ? 'site' : 'other';
  } catch { return 'free'; }
}

let server = null;
const state = await listening();
if (state === 'other') { console.error(`firstlaunch: 127.0.0.1:${port} is held by something that is not the game; pass --port N`); process.exit(2); }
if (state === 'free') {
  await stat(join(site, 'index.html'));   // throws with the path when the site is missing
  server = createServer(async (req, res) => {
    try {
      let p = decodeURIComponent(new URL(req.url, 'http://x').pathname);
      if (p.endsWith('/')) p += 'index.html';
      const f = resolve(join(site, p));
      if (!f.startsWith(site)) { res.writeHead(403); res.end(); return; }
      const body = await readFile(f);
      res.writeHead(200, { 'Content-Type': types[extname(f)] || 'application/octet-stream', 'Cache-Control': 'no-store' });
      res.end(body);
    } catch { res.writeHead(404); res.end(); }
  });
  await new Promise((ok) => server.listen(port, '127.0.0.1', ok));
}

const url = `http://127.0.0.1:${port}/` + (query.length ? '?' + query.join('&') : '');
const browser = await chromium.launch();
try {
  const context = await browser.newContext({ ...devices['iPhone 15 Pro landscape'] });
  const page = await context.newPage();
  const logs = [];
  page.on('console', (m) => logs.push(m.text()));
  await page.goto(url, { waitUntil: 'load' });
  await page.waitForSelector('canvas', { state: 'attached', timeout: 30000 });
  // the page hides its loading status when the game has started (index.html's onRuntimeInitialized)
  await page.waitForFunction(() => { const s = document.getElementById('status'); return !s || s.style.display === 'none'; }, null, { timeout: 90000 });
  await page.waitForTimeout(3000);                   // the title screen
  await page.locator('canvas').first().focus().catch(() => {});
  await page.keyboard.press('KeyN');                 // NEW ADVENTURE
  await page.waitForTimeout(2500);                   // the world is forged behind its card, then the creator opens
  for (let i = 0; i < 4; i++) { await page.keyboard.press('Enter'); await page.waitForTimeout(400); }   // each tab, then BEGIN
  await page.waitForTimeout(3000);
  await page.screenshot({ path: out });
  const vp = page.viewportSize();
  console.log(`firstlaunch: ${url} -> ${out} (${vp.width} x ${vp.height} CSS px, scale ${devices['iPhone 15 Pro landscape'].deviceScaleFactor})`);
  const rig = logs.find((l) => l.startsWith('rig 1:'));
  if (rig) console.log(`firstlaunch: the page said "${rig}"`);
} finally {
  await browser.close();
  if (server) server.close();
}
