export const meta = {
  name: 'embervale-p2b-slice',
  description: 'EMBERVALE P2b New People, part 1: phase A lead -> DOLL / FOLK-SIM + the A3 style slice (judged) -> owner pack for sign-off',
  whenToUse: 'args: {notes?}. Plan = docs/VISION_PLAN.md 15.36 (P2b) + docs/STYLE_P2.md + docs/ref/style_a3. Stops at the owner sign-off; part 2 (wave 2, integrate, judge) is a separate run.',
  phases: [
    { title: 'Phase A', detail: 'lead: reference drafts, doll contract, tools, look fields, test hooks, harness, baselines' },
    { title: 'Wave 1', detail: 'DOLL, FOLK-SIM, FOLK-BODY style slice + slice gate' },
    { title: 'Owner pack', detail: 'screenshots for the owner sign-off' },
  ],
}

const NOTES = (args && args.notes) || ''
const SHOTS = 'C:\\Users\\joshu\\AppData\\Local\\Temp\\claude\\emb_P2b'

const CTX = `
You are on the EMBERVALE team: a procedural open-world 2D pixel RPG, C++20/SDL3, iPhone-first.
Repo: C:\\Users\\joshu\\Documents\\embervale. Ships as a Windows exe and a web build (Emscripten via .github/workflows/web.yml ->
GitHub Pages; the owner plays it on an iPhone from the Home Screen, landscape).
THIS BATCH: P2b "New People". The plan is docs/VISION_PLAN.md section 15.36 from "The art-direction change" on (read it all; the
P2b part is authoritative for your area, its files and its gates), and the art direction is docs/STYLE_P2.md. The locked reference
is docs/ref/style_a3 (the owner APPROVED take A3: 32 px, chunky proportions, eye whites, iris and glint, hue-shifted 5-step ramps,
coloured lit-edge outlines, left facings repainted for top-left light). People are now HAND-PLACED pixel art: .pxm text-grid part
maps composed in layers by the doll and recoloured by ramps. Never draw people with geometric primitives in code; the owner rejected
that look ("not sharp or clear"). Owner requirements: many head types, lots of hair styles, clothing AND armour fitted for both men
and women. Quality bar (owner): triple-A pixel art; nothing crappy or half baked.
Build: from Git Bash  cmd //c "set BDIR=<your dir>&& tools\\\\build.bat <target>" < /dev/null. Check the log shows ninja lines
(a wrong cmd form silently builds nothing).
Tests: build/rpg_test modes (.github/workflows/web.yml has the CI list); save_test; embervale.exe --script tools/scripts/<x>.txt;
tools/run_scripts.sh [prefix_ ...]. Witness 0.5.0 (C:\\Users\\joshu\\Documents\\witness): node ../witness/bin/witness.js
test | shot | eval | measure (scale/box/contrast/motion) | play. Playtests run HEADLESS (owner rule; never --watch).
If you hit a Witness bug or rough edge, put it in your report under "Witness issues" with a repro; the main session forwards it.
TEST ONCE (owner rule): the all-systems regression runs once, by the integrator. Everyone else tests only what they changed.
Judge every visual change from screenshots at 1x and zoomed, desktop AND phone, and iterate until it is genuinely good. Creative
work: do not cap your screenshots or iterations.
RULES:
- No git commands (the main session commits/pushes).
- At most 3 heavy processes machine-wide: wrap every build/run/test in tools/slot.sh.
- Never kill the owner's running game (an Embervale.exe from the Desktop). Test runs never touch the owner's save.
- Determinism: integer/dmath only in worldgen; never two RNG draws as arguments of one call; no lone CR bytes in string literals;
  no Linux macro names. Code compiles on MSVC and clang/Emscripten; threads only outside __EMSCRIPTEN__.
- No SAVE_VER / ENDLESS_GEN_VER / SAGA_GEN_VER bump unless the plan says so.
- Edit with Edit/Write tools. Stay inside the files you own. If a fix truly needs a finished area's file, make the minimal change
  and list it in your report (do not stop to ask). The phase A contracts and harness are frozen: ADD, never rename or change meaning.
- Players still see v1 at rig 1: nothing may change at rig 1 (golden_village_art, art_hash v1 categories, the v1 anim golden).
- Screenshots go in ${SHOTS}\\<your-name>\\ ; delete them when done. Delete your own build dir at the end unless it is "build".
- Run checks in the foreground. Never end your turn while a background command is still running.
- Report concisely: files changed, verification with numbers (gate ids from 15.36), open issues, Witness issues.
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
          area: { type: 'string', enum: ['DOLL', 'FOLK-BODY', 'FOLK-SIM', 'OTHER'] },
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

phase('Phase A')
const lead = await agent(`${CTX}
You are the P2b LEAD. Wave 0 has landed (the balance and quest work is committed). Do PHASE A of section 15.36 P2b, items 1-7,
serially and completely: the A3 reference drafts (tools/doll/from_a3.py), the doll contract (rpg/art/doll/doll.h), the tools
(pxc.mjs with --check/--report, draft.mjs, rigdraft), the look fields (art_human.h, mixed into key() only at rig 2), the test hooks
(camhold, lineup, dollpart, the doll state in witness_state.cpp, expect doll), the frozen harness (rpg_test --doll/--doll-coverage/
--doll-variety, --rig restated, doll_gallery, the p2b_* scripts with _phone, witness/p2b_doll.witness.json, the playtest goals),
and the baselines in docs/PLAN.md. art_rig.cpp's dispatch goes to the doll and the rejected procedural painters are detached.
Prove rig 1 is unchanged (golden_village_art, art_hash v1 categories, the v1 anim golden, every existing script and witness test),
MSVC build plus an EMBERVALE_WITNESS=OFF configure that compiles (the web stand-in). Add any new rpg_test modes to web.yml's native
job, without --gates until the areas pass them.
Return a report the areas will read: the frozen API (exact names and signatures), the .pxm format, where each area's hooks are,
the baselines, the harness commands, and anything an area must know.`, { label: 'P2b-lead', phase: 'Phase A', agentType: 'engineer-high' })

const LANE_COMMON = `
The lead finished phase A. Its report (the frozen contract; read it carefully):
${lead}
`

phase('Wave 1')
const laneDoll = () => agent(`${CTX}${LANE_COMMON}
You own area DOLL (section 15.36 P2b "Areas"): its files, core/depth/delight, done gates B1 (parity), B6, B8 and B10. BDIR=build-doll.
The slice artist (FOLK-BODY) composes on your compiler and compositor as you build them: land the format and parity path first and
keep it working at every step. Measure with the harness and report the numbers.`, { label: 'P2b-DOLL', phase: 'Wave 1', agentType: 'engineer-high' })

const laneSim = () => agent(`${CTX}${LANE_COMMON}
You own area FOLK-SIM (section 15.36 P2b "Areas"): the look fields from the census and the hero, elders and youths from census ages,
the role garments, walkTable walk and run tables, the gait switch at 54 px/s, the sheathed / drawn pick, the elder's walk. Done gates
B5 and B7 (your parts). BDIR=build-sim. The run and sheathing default ON (the owner settles both at the slice sign-off); keep each
behind one constant so it can be turned off.`, { label: 'P2b-FOLK-SIM', phase: 'Wave 1', agentType: 'engineer' })

const SLICE_ROUNDS = 4
async function sliceTrack() {
  let slice = await agent(`${CTX}${LANE_COMMON}
You own area FOLK-BODY and build THE STYLE SLICE (section 15.36 P2b "The slice gate" and STYLE_P2 section 19) on the doll.
BDIR=build-slice. Reproduce A3 in the engine at rig 2 with 0 px difference on the 48 parity frames, re-register the side walk to the
plan's walk tables, plus the slice's variety lineup and mail and a helm on both bodies, shown in the real game at NORMAL zoom, desktop
and phone, day and night. Look, adjust, repeat until it is triple-A pixel art. Then run the slice gate's measured points (1-7) on
yourself with witness measure and the harness and report the numbers. Do NOT start the wave-2 production maps.`,
    { label: 'P2b-slice', phase: 'Wave 1', agentType: 'artist-xhigh' })
  let verdict = null
  for (let r = 1; r <= SLICE_ROUNDS; r++) {
    verdict = await agent(`${CTX}
You are the SLICE GATE judge for P2b (section 15.36 P2b "The slice gate", points 1-8; point 9 is the owner's). Look and feel first,
from the state a player starts in (new game, NORMAL zoom, rig 2), desktop and iPhone landscape, day and night; then the measured
points with witness measure and the harness. Compare against docs/ref/style_a3 side by side. Take and look at your own screenshots; do
not trust the artist's numbers. ${r === 1 ? 'State your full protocol in "protocol".' : 'Use the same protocol as round 1 and re-check every previous item.'}
PASS only if it holds up next to the A3 reference and Sea of Stars-level pixel craft and every measured point is in band. Otherwise
FAIL with at most 12 items, each with a repro and a measurable check. Keep item ids stable across rounds.
The artist's report:
${slice}
${verdict ? 'Your previous verdict:\n' + JSON.stringify(verdict) : ''}`, { label: `P2b-slice-judge-r${r}`, phase: 'Wave 1', agentType: 'judge', schema: JUDGE_SCHEMA })
    if (!verdict || verdict.pass) break
    if (r === SLICE_ROUNDS) break
    const doll = verdict.items.filter(i => i.area === 'DOLL' || i.kind === 'code')
    const art = verdict.items.filter(i => !(i.area === 'DOLL' || i.kind === 'code'))
    const fixes = []
    if (art.length) fixes.push(() => agent(`${CTX}${LANE_COMMON}
You own FOLK-BODY and the style slice (BDIR=build-slice). The slice judge FAILED it (round ${r}). Fix every item below, rerunning the
judge's own check before you claim a fix, and keep iterating on the look until it is genuinely good. Items:
${JSON.stringify(art, null, 1)}
Judge's measurements: ${verdict.measurements}`, { label: `P2b-slice-fix-r${r}`, phase: 'Wave 1', agentType: 'artist-xhigh' }))
    if (doll.length) fixes.push(() => agent(`${CTX}${LANE_COMMON}
Slice fix round ${r}, code items. You own the files these items name (DOLL or FOLK-SIM per the plan). BDIR=build-slicefix. Rerun
the judge's own check for each item before you claim a fix. Items:
${JSON.stringify(doll, null, 1)}
Judge's measurements: ${verdict.measurements}`, { label: `P2b-slice-codefix-r${r}`, phase: 'Wave 1', agentType: 'engineer-high' }))
    const out = await parallel(fixes)
    slice = (art.length ? out[0] : slice) + (doll.length ? '\nCode fixes:\n' + out[out.length - 1] : '')
  }
  log(`slice gate: ${verdict && verdict.pass ? 'PASS' : 'not passed after ' + SLICE_ROUNDS + ' rounds'}`)
  return { slice, verdict }
}

const [doll, sim, sl] = await parallel([laneDoll, laneSim, sliceTrack])

phase('Owner pack')
const pack = await agent(`${CTX}
You build THE OWNER PACK for the P2b slice sign-off (STYLE_P2 section 19 says what it holds). The owner judges from these pictures
alone, on a phone, so make them clear and few (6-10 images), each self-explanatory. Use the integrated build (BDIR=build; build it).
At minimum: A3 reference vs in-engine doll at 8x; the hero and the woman in the start village at NORMAL zoom on the iPhone window,
day and night; the variety lineup (different head types, hair styles and garments, men and women); mail and a helm on both bodies;
walk and run as animated GIFs at real game timing; the sword swing GIF; sheathed vs drawn. Put them in
C:\\Users\\joshu\\AppData\\Local\\Temp\\claude\\emb_P2b\\owner_pack\\ with short file names that sort in viewing order (01_..., 02_...).
Do not delete these (the main session sends and deletes them). Return the file list with a one-line caption per file, plus any
honest weaknesses the owner should know about.
Slice state: ${sl && sl.verdict && sl.verdict.pass ? 'PASSED the slice gate' : 'slice gate NOT fully passed; open items: ' + JSON.stringify(sl && sl.verdict ? sl.verdict.items : [])}`,
  { label: 'P2b-owner-pack', phase: 'Owner pack', agentType: 'artist' })

return {
  slicePass: !!(sl && sl.verdict && sl.verdict.pass),
  lead, doll, sim,
  slice: sl ? sl.slice : null,
  verdict: sl ? sl.verdict : null,
  pack,
}
