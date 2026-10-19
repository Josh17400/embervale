# Witness against the web build (the Emscripten bridge)

The web build can be driven by Witness in a headless browser, as an iPhone shows it (Witness 0.7, adapters/cpp/GUIDE.md
section 13). The bridge is a separate **test variant**: the deployed site (web.yml) is built without it and is unchanged.

- CMake: `-DEMBERVALE_WITNESS_WEB=ON` (Emscripten only, default OFF) compiles `engine/witness_impl.cpp` with
  `EMB_WITNESS` and links `engine/witness_emscripten.js` with `--pre-js`. The shim does nothing unless the page URL has
  `?witness=host:port`, which the Witness CLI adds, so the variant also plays normally.
- rpg/main.cpp: in witness mode the web build never starts `emscripten_set_main_loop`; `witness_run()` returns to the
  browser and the game steps only inside the harness's commands. The canvas is shown (the page's own canvas, filling the
  document), pinned at 480 x 270 as on the desktop harness. The game answers `screenshot` itself (its canvas buffer, 480 x
  270 game pixels); `--capture element` reads what the phone shows.
- CI: `.github/workflows/web-witness.yml` (manual, Actions tab > "Web build (Witness variant)" > Run workflow) builds it
  into `build-web-witness/`, checks the shim and the exports are in `embervale.js`, and uploads the site as the
  `embervale-web-witness` artifact. No Pages upload, no deploy.

## Run `witness doctor` on an iPhone 15 (landscape, WebKit)

From Git Bash, in the repo root (`C:/Users/joshu/Documents/embervale`), with `../witness` beside the repo:

1. Get the build. Either download the artifact of a "Web build (Witness variant)" run:
   ```
   gh run download --name embervale-web-witness --dir site-witness
   ```
   (`gh run list --workflow web-witness.yml` lists the runs; add `<run-id>` after `download` to pick one), or build it
   locally with emsdk active:
   ```
   emcmake cmake -S . -B build-web-witness -DCMAKE_BUILD_TYPE=Release -DEMBERVALE_WITNESS_WEB=ON
   cmake --build build-web-witness --target embervale
   mkdir -p site-witness && cp web/* site-witness/ && cp build-web-witness/embervale.js build-web-witness/embervale.wasm site-witness/
   ```
   `site-witness/` must hold `index.html`, `manifest.json`, `apple-touch-icon.png`, `embervale.js`, `embervale.wasm`.
2. Playwright with WebKit, once (no admin). Witness looks for Playwright in the game project, in the Witness folder, or
   in `WITNESS_PLAYWRIGHT`:
   ```
   (cd tools/witness_web && npm init -y >/dev/null && npm i -D playwright && npx playwright install webkit chromium)
   export WITNESS_PLAYWRIGHT=$PWD/tools/witness_web
   ```
3. Doctor (tools/witness_web/witness.json serves `site-witness/` on 127.0.0.1:8090 with Python's http.server and opens
   `index.html?witness=...`; its `launch.device` is already `iphone-15-landscape`):
   ```
   tools/slot.sh node ../witness/bin/witness.js doctor --config tools/witness_web --device iphone-15-landscape --engine webkit
   ```
   Also worth running: the same with `--engine chromium` (Playwright's WebKit for Windows has no Web Audio and no
   TouchEvent: Witness puts in a silent AudioContext, doctor says so), `--no-auto-unlock` (the iPhone's "no sound until a
   tap" path: SDL resumes its AudioContext from its own DOM listeners, so expect a key alone to be a warning on a touch
   device), and `--capture element` for what the phone's screen shows.
4. What to expect: "connected" with `adapter: witness.h 1.0.0 + witness_emscripten.js 1.0.0`, the 480 x 270 canvas, the
   determinism check, and the web section: the device page (393 x 852 pt portrait screen, DPR 3, 5 touch points,
   platform iPhone), the safe-area preset to check (`measure ui --safe iphone-landscape`), and how the canvas lands on the
   screen (the layout scale and whether it is whole). Every desktop test (witness/*.witness.json) can run against it too:
   `witness test --config tools/witness_web` with `"tests": "../../witness"` in that witness.json.

If doctor can't connect, open the page by hand (`python -m http.server 8090 -d site-witness`, then
`http://127.0.0.1:8090/index.html?witness=127.0.0.1:7357` in a browser): the console shows `[witness] ...` lines from the
shim (no WebSocket, can't reach Witness) and from witness.h (`no witness_emscripten.js in the page`).
