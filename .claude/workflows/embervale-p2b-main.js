export const meta = {
  name: 'embervale-p2b-main',
  description: 'EMBERVALE P2b New People, part 2: prep -> wave 2 production (11 art/code areas) -> integrate -> judge/fix rounds',
  whenToUse: 'args: {notes?}. Runs after the owner signed off the P2b slice. Plan = docs/VISION_PLAN.md 15.36 (P2b) + docs/STYLE_P2.md.',
  phases: [
    { title: 'Prep', detail: 'record the owner walk rules; small Witness 0.7.1 game-side items' },
    { title: 'Wave 2', detail: 'FOLK-BODY, FACES, HAIR, GARB-M, GARB-W, OUTER, POSES-B, PROPS, KIN-YOUTH, KIN-CHILD, HOLDERS-B, VIEW-B, DOLL, FOLK-SIM' },
    { title: 'Integrate', detail: 'one full regression, script sweep, pxc --check, art_hash, web build, playtests' },
    { title: 'Judge', detail: 'measured acceptance, parallel fixes, max 4 rounds' },
  ],
}

const NOTES = (args && args.notes) || ''
const SHOTS = 'C:\\Users\\joshu\\AppData\\Local\\Temp\\claude\\emb_P2b'

const CTX = `
You are on the EMBERVALE team: a procedural open-world 2D pixel RPG, C++20/SDL3, iPhone-first.
Repo: C:\\Users\\joshu\\Documents\\embervale. Ships as a Windows exe and a web build (Emscripten via .github/workflows/web.yml ->
GitHub Pages; the owner plays it on an iPhone from the Home Screen, landscape).
THIS BATCH: P2b "New People", part 2 (production). Part 1 (phase A, the doll, the style slice) is committed and the OWNER SIGNED OFF
the slice, the run above 54 px/s and sheathed weapons. The plan is docs/VISION_PLAN.md section 15.36 from "The art-direction change"
on (the P2b part is authoritative for your area, files and gates); the art direction is docs/STYLE_P2.md; the locked reference is
docs/ref/style_a3. People are HAND-PLACED pixel art: .pxm part maps under rpg/art/doll/maps composed by the doll, compiled by
node tools/doll/pxc.mjs into rpg/art/doll/gen/doll_pool.cpp (re-run it after editing maps; the generated file is shared, so re-run
it right before you build and never hand-edit it). Never draw people with geometric code.
OWNER RULES (hard, from the owner's own reviews; every area that draws bodies, legs, clothing or poses obeys them):
- Keep A's CHUNKY proportions: sturdy legs and boots, the A3 silhouette. The owner rejected skinny legs twice.
- Walk and run: every frame keeps >= 90 % of the idle's leg area and the idle's hip width; boots are NEVER drawn rotated (flat sole,
  the idle boot moved and lifted); clean knee and ankle joints, the bend at the knee; hip bob at most 1 px (0/+1/0), the planted leg
  straight under the hips, no splayed stance (a down frame no wider than contact). rpg_test --doll --gates measures these.
- Women: develop and judge women's legs and bodies in trousers and bare legs, not only in dresses (they wear trousers, armour,
  short garments). Clothing AND armour fitted separately for men and women.
- Variety the owner asked for: many head types, lots of hair styles (the plan's 9+9 heads, 24 hair styles, 9 beards), garments per
  body; nothing that reads as one head recoloured.
- Quality bar: triple-A pixel art (Sea of Stars / Chained Echoes craft at 32 px); nothing crappy or half baked.
Build: from Git Bash  cmd //c "set BDIR=<your dir>&& tools\\\\build.bat <target>" < /dev/null  (check the log shows ninja lines).
Tests: build/rpg_test modes (web.yml has the CI list), save_test, embervale.exe --script tools/scripts/<x>.txt, tools/run_scripts.sh
[prefix_ ...]. Witness 0.7.1 at C:\\Users\\joshu\\Documents\\witness: node ../witness/bin/witness.js test | shot (--sheet, --clip
for lossless APNG) | ab (A/B runs with --rect) | measure (scale/box/contrast/motion/registration/figure/ui/perf) | eval | play.
Playtests run HEADLESS (owner rule). Witness bugs: list them under "Witness issues" with a repro; the main session forwards them.
TEST ONCE (owner rule): the all-systems regression runs once, by the integrator. Everyone else tests only what they changed.
Judge every visual change from screenshots at 1x and zoomed, desktop AND phone, in motion where it moves; iterate until genuinely
good. Creative work: do not cap your screenshots or iterations.
RULES:
- No git commands (the main session commits/pushes).
- At most 3 heavy processes machine-wide: wrap every build/run/test in tools/slot.sh.
- Never kill the owner's running game (an Embervale.exe from the Desktop). Test runs never touch the owner's save.
- Determinism: integer/dmath only in worldgen; never two RNG draws as arguments of one call; no lone CR bytes in string literals;
  no Linux macro names. Code compiles on MSVC and clang/Emscripten; threads only outside __EMSCRIPTEN__.
- Edit with Edit/Write tools. Stay inside the files you own (the plan's area file lists). If a fix truly needs another area's file,
  make the minimal change and list it in your report; do not stop to ask. Contracts from phase A are frozen: ADD, never rename.
- Rig 1 (what players see by default) must not change: golden_village_art, art_hash v1 categories, the v1 anim golden.
- Screenshots go in ${SHOTS}\\<your-name>\\ ; delete them when done. Delete your own build dir at the end unless it is "build".
- Run checks in the foreground. Never end your turn while a background command is still running.
- Report concisely: files changed, verification with numbers (gate ids B1-B13 from 15.36), open issues, Witness issues.
${NOTES ? 'OWNER NOTES: ' + NOTES : ''}`

const AREAS = ['DOLL', 'FOLK-BODY', 'FACES', 'HAIR', 'GARB-M', 'GARB-W', 'OUTER', 'POSES-B', 'PROPS', 'KIN-YOUTH', 'KIN-CHILD', 'HOLDERS-B', 'FOLK-SIM', 'VIEW-B', 'OTHER']
const JUDGE_SCHEMA = {
  type: 'object',
  properties: {
    pass: { type: 'boolean' },
    protocol: { type: 'string' },
    measurements: { type: 'string' },
    items: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          id: { type: 'string' },
          area: { type: 'string', enum: AREAS },
          kind: { type: 'string', enum: ['code', 'visual'] },
          problem: { type: 'string' },
          repro: { type: 'string' },
          check: { type: 'string' },
        },
        required: ['id', 'area', 'kind', 'problem', 'repro', 'check'],
      },
    },
  },
  required: ['pass', 'items', 'measurements'],
}

phase('Prep')
const prep = await agent(`${CTX}
You are the P2b PREP lead (short, serial). Do these and nothing else:
1. Record the owner's walk change in docs/STYLE_P2.md (sections 10 and 19 and wherever the walk is specified) and in VISION_PLAN 15.36's
   slice-gate parity definition: the A3 idles and swing keys stay at 0 px parity; the walk and run frames were redrawn at the owner's
   request (chunky legs, flat boots, clean knees, 0/+1/0 bob, both bodies on an 18 px walk cycle, run 36 px) and are gated by the
   leg-mass / hip-row / stance checks in rpg_test --doll --gates, not by A3 parity. Add the OWNER RULES above to STYLE_P2's don'ts.
2. Witness 0.7.1 game-side items (read ../witness/CHANGELOG.md "Adoption steps (0.7.0 -> 0.7.1)"): open the SDL audio device in witness
   mode on the web build (silence is fine; keep witness_audio_write for what listen hears); add state.layers = [{name:"world",rect},
   {name:"ui",rect}] to the snapshot (rpg/view/witness_state.cpp) so measure scale reads each layer; add enter:"s" (the push
   direction that uses it) to the snapshot's Exit / dungeon-exit door entities, and the right direction on building doors.
3. Build (BDIR=build), run witness test --repeat 2 (the 3 p2b target tests may still fail by design; nothing else), and the
   rpg_test CI commands your changes touch.
Return a short report the production areas will read: what changed, anything they must know.`, { label: 'P2b-prep', phase: 'Prep', agentType: 'engineer' })

const COMMON = `\nPrep report:\n${prep}\n`

phase('Wave 2')
const lanes = [
  ['FOLK-BODY', 'artist-xhigh', 'Complete the adult bodies per the plan (idle, walk, run, hurt, fall, lying, the elder stoop, shoe tips, the woman overrides) and ALSO the drawn-weapon idle and walk (the slice only had the sheathed idle; after a swing the hero must show a real drawn idle/walk). Obey every OWNER RULE; the leg-mass gate must pass for every body set.'],
  ['FACES', 'artist-xhigh', 'The 9 + 9 head types, noses, brows, ear sets, expressions, marks, age, face paint, earrings. Each head type must read as a different person at 1x on the phone, not a recolour.'],
  ['HAIR', 'artist', 'The 24 adult styles (men and women) with hangs, tufts, capped variants and lying views, and the 9 beards. Every style in 4 facings, hangs moving in walk and run.'],
  ['GARB-M', 'artist-xhigh', 'The man\'s 13 civilian garments fitted to the man in every key, with build masks, sleeves, hangs and patterns.'],
  ['GARB-W', 'artist-xhigh', 'The woman\'s 14 garments fitted to the woman (trousers and short garments as well as dresses; judge her legs uncovered), the overlays\' woman cuts, neckwear, belt items.'],
  ['OUTER', 'artist', 'Culture headwear and hoods with hair masks; the 4 cloaks with sway, run flare, sitting and riding drapes.'],
  ['POSES-B', 'artist', 'The 31 postures and Row on the adult bodies; riders, rowers, sleepers per the plan.'],
  ['PROPS', 'artist', 'Everything a civilian holds, the hand over the grip; the culture instruments.'],
  ['KIN-YOUTH', 'artist', 'The youth body (26 px) in every LOCO, DEATH and posture key, its 8 garments and hangs. Chunky, not skinny (OWNER RULES apply).'],
  ['KIN-CHILD', 'artist', 'The child body (21 px): legs, arms, 7 garments, 4 head types, 10 hair styles, child headwear, kids\' postures. OWNER RULES apply.'],
  ['HOLDERS-B', 'artist', 'P2a H2/H3: gate, arch and city-gate clearance 1.25 of the body or more; hedge measured; rowboat gunwale and deck rail in range.'],
  ['DOLL', 'engineer-high', 'Finish the doll per the plan: the turned lying figure and sleepers, pattern tiles, elf tints, the curated skin/hair tables, the ramp generator calibration, bake speed, incremental compile and --watch, the onion-skin gallery view. Support the art areas: fix format or compositor problems they report quickly.'],
  ['FOLK-SIM', 'engineer', 'Finish the look fields from the census and the hero (every new head type, hair style, beard and garment reachable and distributed per B7), elders and youths from census ages, role garments, the gait switch, the sheathed/drawn pick using FOLK-BODY\'s drawn idle/walk.'],
  ['VIEW-B', 'engineer', 'The doll\'s frames on every civilian draw path at rig 2 (per-part coverage, v1 whole otherwise), the RUN part in the bake queue, markers and plates on RigAnchors::topY, riders on the doll, Row synced to oars, the posture path, the hidden-hero silhouette, the contact shadow under corpses.'],
]
const reports = await parallel(lanes.map(([area, type, brief]) => () => agent(`${CTX}${COMMON}
You own area ${area} (VISION_PLAN 15.36 P2b "Areas": your files, core/depth/delight, your done gates). BDIR=build-${area.toLowerCase()}.
${brief}
Other areas draw in parallel; compose your parts with theirs as they land (re-run pxc.mjs, then doll_gallery / the p2b scripts) and
check your parts on real census people in the game at rig 2, desktop and phone, day and night.`, { label: `P2b-${area}`, phase: 'Wave 2', agentType: type })))
const laneReports = lanes.map(([a], i) => `${a}:\n${reports[i]}`).join('\n\n')

phase('Integrate')
const integ = await agent(`${CTX}
You are the P2b INTEGRATOR. All areas are done; their reports are below. Use BDIR=build (clean configure if needed).
1. Re-run node tools/doll/pxc.mjs, then MSVC build of every target; fix integration breaks (you may edit any file for compile, link or
   merge fixes only; note each).
2. THE one full regression (TEST ONCE): every rpg_test/save_test command in .github/workflows/web.yml, the full tools/run_scripts.sh
   sweep (no prefix), witness test --repeat 2, pxc.mjs --check, art_hash once (the doll's new categories; v1 unchanged), and the P2b
   DONE gates table B1-B12 measured with the harness and Witness (measure box/scale/contrast/motion/figure, ab for before/after).
   Also an EMBERVALE_WITNESS=OFF configure+build as the web stand-in, and report the web size estimate against B10.
3. Linux clang (CI-equivalent): from Git Bash run bash C:/Users/joshu/AppData/Local/Temp/claude/C--Users-joshu/5673c178-5c4b-4daa-8a88-0029f5ad318a/scratchpad/verify_batch.sh
   (it builds MSVC, regenerates the CI command list from web.yml, rsyncs to WSL, builds and runs every CI command there).
4. B13 playtests: write a goals JSON for T1-T3 from the plan (rig 2 in the reset params; phone window) and run
   node ../witness/bin/witness.js play batch --goals <file> --minutes 20 --parallel 2 (headless). Use a COPY of build/embervale.exe in
   build-play/ and a copy of witness.json pointing at it (--config <file>), so builds don't collide with running testers. Triage every
   tester note with a shot (fixed, or listed as open with an area).
5. Add --gates to the doll rpg_test lines in web.yml only for gates that now pass on both compilers.
6. Record results in docs/PLAN.md (P2b entry) and the measured numbers in 15.36's gates table.
Return: a PASS/FAIL table per gate and suite with numbers, scripts failing (name + why), playtest findings, what you fixed, open items.
Area reports:
${laneReports}`, { label: 'P2b-integrate', phase: 'Integrate', agentType: 'engineer-high' })

phase('Judge')
const fails = {}
let last = null
const MAX_JUDGE = 4
for (let r = 1; r <= MAX_JUDGE; r++) {
  last = await agent(`${CTX}
You are the P2b acceptance JUDGE, round ${r} of at most ${MAX_JUDGE}. Grade the integrated build (BDIR=build) against VISION_PLAN 15.36
P2b: look and feel first, from the state a player starts in (a new game at NORMAL zoom with rig 2; also check rig 1 is unchanged),
desktop and iPhone landscape, day and night: villages, a town, a city of another people, the tavern, a bed, a horse, a rowboat, a
fight and a death; lineups of every people; then behaviour (gates B1-B13, measured by you with the harness and Witness: measure,
ab, shot --sheet/--clip); then robustness (web stand-in build, save/load at rig 2). Check every OWNER RULE in motion (legs, boots,
knees, bob, women in trousers). Take and look at your own screenshots and clips. ${r === 1 ? 'State your full test protocol in "protocol".' : 'Rerun your round-1 protocol and re-check every earlier item.'}
PASS only if the folk are triple-A pixel art that holds up next to the A3 reference everywhere, the variety is real (different
people, not recolours), and every gate is in band. Otherwise FAIL with at most 12 items, each with area, kind (code|visual), a repro
and a measurable check. Keep ids stable.
Integrator report:
${integ}
${last ? 'Previous verdict:\n' + JSON.stringify(last) : ''}`, { label: `P2b-judge-r${r}`, phase: 'Judge', agentType: 'judge', schema: JUDGE_SCHEMA })
  if (!last || last.pass) break
  if (r === MAX_JUDGE) break
  last.items.forEach(it => { fails[it.id] = (fails[it.id] || 0) + 1 })
  const groups = {}
  last.items.forEach(it => {
    const n = fails[it.id]
    const type = it.kind === 'code' ? (n >= 2 ? 'engineer-high' : 'engineer') : (n >= 3 ? 'artist-xhigh' : 'artist')
    const key = `${it.area}|${type}`
    ;(groups[key] = groups[key] || []).push(it)
  })
  await parallel(Object.entries(groups).map(([key, items]) => () => {
    const [area, type] = key.split('|')
    return agent(`${CTX}
P2b fix round ${r}. You own area ${area}'s files (VISION_PLAN 15.36) for these judge items only. BDIR=build-fix-${area.toLowerCase()}.
Before claiming a fix, rerun the judge's own check for the item and include the number. Do not re-run the whole regression.
Items:
${JSON.stringify(items, null, 1)}
Judge measurements: ${last.measurements}`, { label: `P2b-fix-r${r}-${area}`, phase: 'Judge', agentType: type })
  }))
}

return { pass: !!(last && last.pass), integrator: integ, verdict: last }
