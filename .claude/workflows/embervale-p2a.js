export const meta = {
  name: 'embervale-p2a',
  description: 'EMBERVALE P2a Lens and Rig: phase A lead -> LENS/ANIM + style slice (judged) -> wave 2 art -> integrate -> judge/fix rounds',
  whenToUse: 'args: {notes?}. Plan = docs/VISION_PLAN.md 15.36 + docs/STYLE_P2.md. Git stays with the main session.',
  phases: [
    { title: 'Phase A', detail: 'lead: contracts, moves, harness, baselines (pixel-identical at rig 1)' },
    { title: 'Wave 1', detail: 'LENS, ANIM, RIG-BODY style slice + slice gate' },
    { title: 'Wave 2', detail: 'RIG-ARMS, POSES, HOLDERS, RIG-BODY base' },
    { title: 'Integrate', detail: 'one full regression, one script sweep, art_hash, clang CI' },
    { title: 'Judge', detail: 'measured acceptance, parallel fixes, max 4 rounds' },
  ],
}

const NOTES = (args && args.notes) || ''
const SHOTS = 'C:\\Users\\joshu\\AppData\\Local\\Temp\\claude\\emb_P2a'

const CTX = `
You are on the EMBERVALE team: a procedural open-world 2D pixel RPG, C++20/SDL3, iPhone-first. ALL art is generated in code.
Repo: C:\\Users\\joshu\\Documents\\embervale. Ships as a Windows exe and a web build (Emscripten via .github/workflows/web.yml ->
GitHub Pages; the owner plays it on an iPhone from the Home Screen, landscape).
THIS BATCH: P2a "Lens and Rig". The plan is docs/VISION_PLAN.md section 15.36 (read the whole section; the P2a part is authoritative
for your area, its files, its done gates) and the art direction is docs/STYLE_P2.md (one direction; follow it). Owner feedback behind
it: characters too small and blurry, the dead look standing, every weapon swings the same. Owner decisions (final): taller detailed
rig with faces and hands, zoom, whole-number scaling, outlines, a player ring; swings and death painted on the new rig.
Build: from Git Bash  cmd //c "set BDIR=<your dir>&& tools\\\\build.bat <target>" < /dev/null  (targets embervale, rpg_test, save_test,
art_preview and the galleries). Check the log shows ninja lines (a wrong cmd form silently builds nothing).
Tests: build/rpg_test modes (see .github/workflows/web.yml for the CI list); save_test; embervale.exe --script tools/scripts/<x>.txt;
tools/run_scripts.sh [prefix_ ...]. Witness (headless deterministic driver): witness.json + witness/*.witness.json, run
node ../witness/bin/witness.js test | shot | eval (README: C:\\Users\\joshu\\Documents\\witness\\integrations\\embervale\\README.md).
TEST ONCE (owner rule): the all-systems regression runs exactly once, by the integrator. Everyone else tests only what they changed.
QUALITY BAR (owner): commercial 16-bit quality; consistent 3/4 top-down view and top-left light across all sprites; cohesive palette.
Judge every visual change from screenshots at 1x and zoomed, desktop AND phone (_phone scripts / EMB_SCREEN), and iterate until it is
genuinely good. Creative work: do not cap your screenshots or iterations.
RULES:
- No git commands (the main session commits/pushes).
- At most 3 heavy processes machine-wide: wrap every build/run/test in tools/slot.sh (counting lock).
- Never kill the owner's running game (an Embervale.exe from the Desktop). Test runs never touch the owner's save.
- Generation stays deterministic (integer/dmath only in worldgen; never two RNG draws as arguments of one call; no lone CR bytes in
  string literals; no Linux macro names). Code compiles on MSVC and clang/Emscripten; threads only outside __EMSCRIPTEN__.
- No SAVE_VER / ENDLESS_GEN_VER / SAGA_GEN_VER bump (plan: tile footprints do not change).
- Edit C++ with Edit/Write tools. Stay strictly inside the files you own; if you need a change in someone else's file, say so in your
  report instead of editing it. The phase A contracts and harness are frozen: you may ADD, never rename or change meaning.
- Screenshots go in ${SHOTS}\\<your-name>\\ ; delete them when done. Delete your own build dir at the end unless it is "build".
- Run checks in the foreground. Never end your turn while a background command is still running.
- Report concisely: files changed, verification with numbers (gate ids from 15.36), open issues.
${NOTES ? 'OWNER NOTES: ' + NOTES : ''}`

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
          area: { type: 'string', enum: ['LENS', 'ANIM', 'RIG-BODY', 'RIG-ARMS', 'POSES', 'HOLDERS', 'OTHER'] },
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

// ---------------- Phase A ----------------
phase('Phase A')
const lead = await agent(`${CTX}
You are the P2a LEAD. Do PHASE A of section 15.36 P2a, items 1-12, serially and completely: layers stub, screen settings + zoom
plumbing, projection helpers, the rig contract header rpg/art/art_rig.h, effects ids, rpg/sim/anim.* reproducing v1 exactly, the
render_anim.cpp / art_handcart.cpp moves, bodyScale plumbing (identity at 1), the outlineInk policy (the only intended pixel change;
re-record art_hash), the Witness/script test hooks and tools/p2_measure.mjs, the frozen harness (rpg_test --rig/--anim/--rig-coverage,
the p2a_lead_lens scripts + _phone, witness/p2a_lens.witness.json, the four gallery skeletons), and the baselines recorded in
docs/PLAN.md (a new P2a entry).
Prove rig 1 is pixel-identical except the outline: golden_village_art identical, every existing tap script's UI unchanged, --anim v1
golden recorded, rpg_test --golden / --endless --golden / --towns --golden unchanged, save_test OK, witness test all green, MSVC build
and an EMBERVALE_WITNESS=OFF configure that compiles (stand-in for the web). Add any new rpg_test modes to web.yml's native job.
Return a report the lanes will read: the frozen API (exact names/signatures), where each lane's hooks are, the baselines, the harness
commands, and anything a lane must know.`, { label: 'P2a-lead', phase: 'Phase A', agentType: 'engineer-high' })

const LANE_COMMON = `
The lead finished phase A. Its report (the frozen contract; read it carefully):
${lead}
`

// ---------------- Wave 1 + slice gate + wave 2 (pipelined) ----------------
phase('Wave 1')

const laneLens = () => agent(`${CTX}${LANE_COMMON}
You own area LENS (section 15.36 P2a "Areas"): its files, its core/depth/delight, done gates L1-L9. Use BDIR=build-lens.
Measure every gate with tools/p2_measure.mjs and the lead's harness on the reference screens (desktop and iPhone landscape sizes);
include the before/after numbers in your report. The rig-2 preview paths you add must work with whatever RIG-BODY has produced so
far (rig2Covers gates it) and must not break when rig 2 is incomplete.`, { label: 'P2a-LENS', phase: 'Wave 1', agentType: 'engineer-high' })

const laneAnim = () => agent(`${CTX}${LANE_COMMON}
You own area ANIM (section 15.36 P2a "Areas"): its files, its core/depth/delight, done gates A1-A5. Use BDIR=build-anim.
--power-curve, --skills, --arms, --foes and the companion suites must stay in their bands; the --anim v1 golden must stay identical
at rig 1. The v2 frames come from RIG-BODY / RIG-ARMS atlases as they land: code against the frozen RigSheet contract and gate on
rig2Covers so missing art falls back to v1.`, { label: 'P2a-ANIM', phase: 'Wave 1', agentType: 'engineer' })

const SLICE_ROUNDS = 4
async function sliceTrack() {
  let slice = await agent(`${CTX}${LANE_COMMON}
You own area RIG-BODY (section 15.36 P2a) and you build THE STYLE SLICE first: section 15.36 "The style slice" and STYLE_P2 (incl.
section 19). Use BDIR=build-rig. Build exactly the slice (hero as a man and a woman: idle, walk, hurt, the three sword swings, fall
and lying, four facings; one village woman; the rescaled cottage door and fence in OSLARS), shown at rig 2 in the real game at
NORMAL zoom, desktop and phone, day and night, via the lead's scripts / Witness. Look, adjust, repeat until it is genuinely
commercial-quality pixel art: readable faces and hands at 32 px, clean outlines, top-left light, a silhouette that reads at a glance.
Then run the judge's 12-point protocol from 15.36 on yourself and report the numbers. Do NOT start the rest of the RIG-BODY base yet.`,
    { label: 'P2a-slice', phase: 'Wave 1', agentType: 'artist-xhigh' })
  let verdict = null
  for (let r = 1; r <= SLICE_ROUNDS; r++) {
    verdict = await agent(`${CTX}
You are the SLICE GATE judge for P2a. Judge the style slice (section 15.36 "The style slice" and "What the judge looks at"; STYLE_P2).
Look and feel first, from the state a player starts in (new game, NORMAL zoom, rig 2 via the lead's hooks), desktop and iPhone
landscape, day and night; then the 12-point measured protocol (tools/p2_measure.mjs and the harness). Take and look at your own
screenshots; do not trust the artist's numbers. ${r === 1 ? 'State your full protocol in "protocol".' : 'Use the same protocol as round 1 and re-check every previous item.'}
PASS only if it would hold up next to commercial 16-bit games and every measured point is in band. Otherwise FAIL with at most 12
items (area RIG-BODY or HOLDERS for the door/fence), each with a repro and a measurable check. Keep item ids stable across rounds.
The artist's report:
${slice}
${verdict ? 'Your previous verdict:\n' + JSON.stringify(verdict) : ''}`, { label: `P2a-slice-judge-r${r}`, phase: 'Wave 1', agentType: 'judge', schema: JUDGE_SCHEMA })
    if (!verdict || verdict.pass) break
    if (r === SLICE_ROUNDS) break
    slice = await agent(`${CTX}${LANE_COMMON}
You own RIG-BODY and the style slice (BDIR=build-rig). The slice judge FAILED it (round ${r}). Fix every item, rerunning the judge's
own checks before you claim a fix, and keep iterating on the look until it is genuinely good. Items:
${JSON.stringify(verdict.items, null, 1)}
Judge's measurements: ${verdict.measurements}`, { label: `P2a-slice-fix-r${r}`, phase: 'Wave 1', agentType: 'artist-xhigh' })
  }
  log(`slice gate: ${verdict && verdict.pass ? 'PASS' : 'not passed after ' + SLICE_ROUNDS + ' rounds; wave 2 proceeds, open items go to the final judge'}`)
  const sliceState = `The style slice ${verdict && verdict.pass ? 'PASSED the slice gate' : 'did not fully pass the slice gate; open items: ' + JSON.stringify(verdict ? verdict.items : [])}.
Slice judge measurements (STYLE_P2 should now record these numbers; RIG-BODY updates STYLE_P2 with them): ${verdict ? verdict.measurements : 'n/a'}
Latest RIG-BODY slice report:
${slice}`

  phase('Wave 2')
  const w2 = await parallel([
    () => agent(`${CTX}${LANE_COMMON}${sliceState}
You own RIG-BODY: now complete the base (section 15.36 RIG-BODY core/depth/delight, done gates R1-R6), matching the locked slice
exactly in style. Record the slice's measured numbers in STYLE_P2. BDIR=build-rig. Fill rig_gallery and use it plus in-game shots.`,
      { label: 'P2a-RIG-BODY', phase: 'Wave 2', agentType: 'artist-xhigh' }),
    () => agent(`${CTX}${LANE_COMMON}${sliceState}
You own RIG-ARMS (section 15.36, done gates R7-R10; STYLE_P2 sections 11-12). BDIR=build-arms. Every weapon family must read
distinctly at a glance in motion (record short frame strips and compare side by side); match the slice's style exactly.`,
      { label: 'P2a-RIG-ARMS', phase: 'Wave 2', agentType: 'artist-xhigh' }),
    () => agent(`${CTX}${LANE_COMMON}${sliceState}
You own POSES (section 15.36, done gates P1-P3). BDIR=build-poses. Agree SeatFit2/BedFit2 with HOLDERS through the tables named in
the plan (you own the v2 tables in art_life.cpp; HOLDERS owns the furniture art) — write the numbers you need into your report and
read HOLDERS' furniture sizes from art_props.cpp as they land. Match the slice's style exactly.`,
      { label: 'P2a-POSES', phase: 'Wave 2', agentType: 'artist' }),
    () => agent(`${CTX}${LANE_COMMON}${sliceState}
You own HOLDERS (section 15.36, done gates H1-H4; STYLE_P2 sections 1 and 16). BDIR=build-holders. Everything stays
pixel-identical at rig 1 (golden_village_art); at rig 2 doors, walls, furniture, carts, horses, decks, ladders, gates, fences and
graves fit the 32 px body. Check in-game at rig 2 with real people using the things.`,
      { label: 'P2a-HOLDERS', phase: 'Wave 2', agentType: 'artist' }),
  ])
  return { sliceState, w2 }
}

const [lens, anim, art] = await parallel([laneLens, laneAnim, sliceTrack])
const artW2 = (art && art.w2) || []
const laneReports = `LENS:\n${lens}\n\nANIM:\n${anim}\n\nSLICE:\n${art ? art.sliceState : 'n/a'}\n\nRIG-BODY:\n${artW2[0]}\n\nRIG-ARMS:\n${artW2[1]}\n\nPOSES:\n${artW2[2]}\n\nHOLDERS:\n${artW2[3]}`

// ---------------- Integrate ----------------
phase('Integrate')
const integ = await agent(`${CTX}
You are the P2a INTEGRATOR. All lanes are done; their reports are below. Use BDIR=build (clean configure if needed).
1. MSVC build of every target; fix integration breaks (you may edit any file for compile/link/merge fixes only; note each).
2. THE one full regression (TEST ONCE): every rpg_test/save_test command in .github/workflows/web.yml, the full
   tools/run_scripts.sh sweep (no prefix), witness test, art_hash once, and the P2a gates table of 15.36 (L1-L9, A1-A5, R1-R10, P1-P3,
   H1-H4) measured with the harness. Also an EMBERVALE_WITNESS=OFF configure+build as the web stand-in.
3. Linux clang (CI-equivalent): refresh WSL with  wsl -d Ubuntu -- bash -lc "rsync -a --delete --exclude='/build*' --exclude='/.git'
   /mnt/c/Users/joshu/Documents/embervale/ ~/emb/ && cd ~/emb && ninja -C build-native"  (from Git Bash set MSYS_NO_PATHCONV=1), then
   run every web.yml ./build-native/ command there.
4. Fix only clear integration regressions yourself; list everything else as open with repro.
5. Record results in docs/PLAN.md (P2a entry) and the measured numbers in 15.36's gates table.
Return: a PASS/FAIL table per gate and suite with numbers, scripts failing (name + why), what you fixed, open items.
Lane reports:
${laneReports}`, { label: 'P2a-integrate', phase: 'Integrate', agentType: 'engineer-high' })

// ---------------- Judge rounds ----------------
phase('Judge')
const fails = {}
let last = null
const MAX_JUDGE = 4
for (let r = 1; r <= MAX_JUDGE; r++) {
  last = await agent(`${CTX}
You are the P2a acceptance JUDGE, round ${r} of at most ${MAX_JUDGE}. Grade the integrated build (BDIR=build) against section 15.36
P2a: look and feel first from the state a player starts in (a new game at NORMAL zoom; shipped default rig 1 AND the rig 2 preview),
desktop and iPhone landscape, day and night, in a village, a town and a fight with several weapon families and a death; then behaviour
(the gates table, measured by you with tools/p2_measure.mjs, the harness and Witness); then robustness (zoom changes, restart keeps
ZOOM, web stand-in build). Take and look at your own screenshots. ${r === 1 ? 'State your full test protocol in "protocol".' : 'Rerun your round-1 protocol and re-check every earlier item.'}
PASS only if a player would see big, crisp, readable characters with distinct swings and deaths that lie down, and every gate is in
band. Otherwise FAIL with at most 12 items, each with area, kind (code|visual), a repro and a measurable check. Keep ids stable.
Integrator report:
${integ}
${last ? 'Previous verdict:\n' + JSON.stringify(last) : ''}`, { label: `P2a-judge-r${r}`, phase: 'Judge', agentType: 'judge', schema: JUDGE_SCHEMA })
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
P2a fix round ${r}. You own area ${area}'s files (section 15.36) for these judge items only. Use BDIR=build-fix-${area.toLowerCase()}.
Before claiming a fix, rerun the judge's own check for the item and include the number. Do not re-run the whole regression.
Items:
${JSON.stringify(items, null, 1)}
Judge measurements: ${last.measurements}`, { label: `P2a-fix-r${r}-${area}`, phase: 'Judge', agentType: type })
  }))
}

return {
  pass: !!(last && last.pass),
  integrator: integ,
  verdict: last,
}
