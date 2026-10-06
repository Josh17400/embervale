export const meta = {
  name: 'embervale-milestone',
  description: 'Run one EMBERVALE milestone from docs/VISION_PLAN.md: lead prep -> parallel lanes -> integrate -> adversarial review/fix loop',
  whenToUse: 'args: {milestone: "M0", notes: "extra owner direction"}. Git commit/push stays with the main session.',
  phases: [
    { title: 'Lead', detail: 'shared refactors/headers/save bump, then lane specs' },
    { title: 'Lanes', detail: 'parallel implementers, each owning disjoint files' },
    { title: 'Integrate', detail: 'clean build, full test suite, scripted screenshots' },
    { title: 'Review', detail: 'multi-lens reviewers + fixer until dry' },
  ],
}

const M = (args && args.milestone) || 'M0'
const NOTES = (args && args.notes) || ''

const CTX = `
You are on the EMBERVALE team: a procedural open-world 2D pixel RPG ("Skyrim in 2D pixels"), C++20/SDL3, iPhone-first.
Repo: C:\\Users\\joshu\\Documents\\Code Stuff\\tailspin (github.com/Josh17400/embervale). The game ships as a web build (Emscripten,
.github/workflows/web.yml -> GitHub Pages, played from the iPhone Home Screen) and a Windows exe. ALL art and audio are generated in code.
Read docs/VISION_PLAN.md (especially the milestone ${M} section and section 15, the owner addendum, which wins on conflicts) and docs/PLAN.md.
Tools: build with  cmd //c "set BDIR=<dir>&& tools\\\\build.bat <target>"  (targets: embervale, rpg_test, save_test, art_preview).
rpg_test --seeds 1..20 ; save_test ; embervale.exe --script tools/scripts/<x>.txt (see rpg/main.cpp header for the script language and flags
like --play --seed --goto --enter --fight --hour --menu --shot --after).
OWNER QUALITY BAR (non-negotiable): the 2D graphics must look really good - commercial 16-bit quality. Walls connect seamlessly
(no gaps, orphan pieces or broken corners, including curved/diagonal runs and gate joins); buildings have depth and dimension
(visible side walls/thickness, foundations, eave overhangs, roof volume with light and shadow, cast shadows, consistent 3/4 top-down
perspective and top-left light across ALL sprites); cohesive palette; no boxy repetition. Judge every visual change from screenshots at
1x and zoomed, and iterate until it is genuinely good. Natural/organic world design is a standing owner direction.
RULES:
- No git commands (the main session commits/pushes).
- PC safety: never more than 3 heavy processes (builds, game runs, tests) machine-wide. If tools/slot exists, wrap every build/run in it
  (it is a counting lock); otherwise run one process at a time yourself. Never kill the owner's running game (check Get-Process).
- Test runs never touch the owner's save. OLD SAVES ARE NOT A CONCERN (owner): no backward compatibility; bump SAVE_VER /
  ENDLESS_GEN_VER and regenerate fixtures/goldens (tools/save_test.cpp, tests/fixtures/) when formats or generation change;
  old saves must be refused gracefully ("start a new adventure"), never crash. Generation stays deterministic (integer/dmath only).
- Code must compile on MSVC and clang/Emscripten: include what you use, no MSVC-only constructs, threads only outside __EMSCRIPTEN__.
- Edit C++ with Edit/Write tools (python heredocs through Bash turn \\\\n into real newlines inside string literals).
- Screenshots in C:\\Users\\joshu\\AppData\\Local\\Temp\\claude\\emb_${M}\\<your-name>\\ ; delete them when done. Delete your own
  build dir at the end unless it is "build".
- Stay strictly inside the files you own. Report concisely: files changed, verification done (with numbers), open issues.
${NOTES ? 'OWNER NOTES FOR THIS MILESTONE: ' + NOTES : ''}`

const LANES_SCHEMA = {
  type: 'object',
  properties: {
    leadReport: { type: 'string' },
    lanes: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          name: { type: 'string' },
          bdir: { type: 'string' },
          owns: { type: 'array', items: { type: 'string' } },
          brief: { type: 'string' },
        },
        required: ['name', 'bdir', 'owns', 'brief'],
      },
    },
  },
  required: ['leadReport', 'lanes'],
}

phase('Lead')
const lead = await agent(`${CTX}

You are the LEAD for milestone ${M}. Do the milestone's "Phase A, lead" work yourself now (shared refactors, new headers/stubs, the
save/worldgen version bump with fixtures, and a tools/slot counting lock - e.g. tools/slot.sh that allows at most 3 concurrent holders
using mkdir-based slot directories with stale-lock cleanup - if it does not exist). Keep everything building and all tests passing (BDIR=build).
Then split the rest of ${M} into 2-4 parallel LANES with STRICTLY DISJOINT file ownership (new files are fine; shared headers you
already prepared should not need further edits - if a lane must touch a shared file, give exactly one lane that file).
Make each lane's brief self-contained and concrete (tasks, acceptance checks, scripts/screenshots it must produce, the quality bar).
If the owner notes add work not covered by the plan (e.g. visual quality fixes), assign it to the best lane or a dedicated lane.
Give each lane its own bdir (build_<name>). Return leadReport + lanes.`, { label: `${M}-lead`, phase: 'Lead', effort: 'high', schema: LANES_SCHEMA })

if (!lead || !lead.lanes || !lead.lanes.length) return { error: 'lead produced no lanes', lead }
log(`${lead.lanes.length} lanes: ${lead.lanes.map(l => l.name).join(', ')}`)

phase('Lanes')
const laneReports = await parallel(lead.lanes.map(l => () => agent(`${CTX}

You are lane "${l.name}" of milestone ${M}. BDIR=${l.bdir}. You OWN ONLY: ${l.owns.join(', ')}.
Other lanes are editing other files right now. The lead already did the shared prep:
${lead.leadReport}

YOUR BRIEF:
${l.brief}

Build and test in your BDIR (rpg_test --seeds 1..20 must stay green; save_test green). Verify visuals with screenshots and iterate to
the quality bar.`, { label: `${M}-lane-${l.name}`, phase: 'Lanes', effort: 'medium' })))

const lanesText = lead.lanes.map((l, i) => `### ${l.name}\n${laneReports[i] || '(lane returned nothing)'}`).join('\n\n')

phase('Integrate')
const integ = await agent(`${CTX}

You are the INTEGRATOR for ${M}. All lanes finished (reports below). BDIR=build; you may edit any file to make things work together.
1. Clean-build embervale, rpg_test, save_test, art_preview (if present). Fix errors; fix warnings that are real bugs.
2. rpg_test --seeds 1..20 ; save_test ; every script in tools/scripts/ (exit codes 0).
3. Screenshot tour that exercises everything ${M} changed (day and night, inside and outside, several seeds); inspect each against the
   quality bar; fix what you can.
4. Portability pass for clang/Emscripten.
Return a concise integration report with test numbers and a list of known open problems.

LEAD: ${lead.leadReport}

LANES:
${lanesText}`, { label: `${M}-integrate`, phase: 'Integrate', effort: 'medium' })

phase('Review')
const FINDINGS = {
  type: 'object',
  properties: {
    findings: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          severity: { type: 'string', enum: ['must', 'should'] },
          title: { type: 'string' },
          detail: { type: 'string' },
          where: { type: 'string' },
        },
        required: ['severity', 'title', 'detail', 'where'],
      },
    },
  },
  required: ['findings'],
}
const LENSES = [
  { key: 'bugs', text: 'CORRECTNESS: crashes, logic bugs, save compatibility, soft-locks, quest breakage, AI pathologies. Read the diff (git diff HEAD) and run targeted repro scripts/tests.' },
  { key: 'visual', text: 'VISUAL QUALITY against the owner bar: take a broad screenshot tour (towns/cities/villages, walls and gates, interiors, characters/equipment, day/night, several seeds); flag anything that looks cheap, broken, disconnected, flat, inconsistent or repetitive. Be picky - commercial quality is the bar.' },
  { key: 'feel', text: 'PLAYER FEEL + PLATFORM: play scripted sessions as a phone player would (touch taps via script, small-screen readability, UI hit targets, clarity of what to do), plus clang/Emscripten portability and performance (--perf).' },
]
const fixes = []
const seen = new Set()
for (let round = 1; round <= 3; round++) {
  const found = (await parallel(LENSES.map(ls => () => agent(`${CTX}

You are a REVIEWER for ${M} (round ${round}), lens: ${ls.text}
Do not edit code. Report only real, verified problems you observed (with repro/screenshot evidence in "detail"). Severity "must" = should
block shipping or clearly misses the quality bar; "should" = worth fixing soon. Skip anything already fixed.
Integration report for context:
${integ}`, { label: `${M}-review-${ls.key}-r${round}`, phase: 'Review', effort: 'medium', schema: FINDINGS }))))
    .filter(Boolean).flatMap(r => r.findings)
  const fresh = found.filter(f => { const k = (f.title + '|' + f.where).toLowerCase(); if (seen.has(k)) return false; seen.add(k); return true })
  const must = fresh.filter(f => f.severity === 'must')
  log(`review round ${round}: ${fresh.length} new findings, ${must.length} must-fix`)
  if (!fresh.length) break
  const fix = await agent(`${CTX}

You are the FIXER for ${M}, round ${round}. BDIR=build. Fix every "must" finding below and as many "should" findings as are safe.
For each, verify the fix (tests/scripts/screenshots). Re-run rpg_test --seeds 1..20, save_test and tools/scripts/* at the end.
Report: per finding fixed / not fixed (why).

FINDINGS:
${JSON.stringify(fresh, null, 1)}`, { label: `${M}-fix-r${round}`, phase: 'Review', effort: 'medium' })
  fixes.push({ round, findings: fresh, fix })
  if (!must.length) break
}

return { milestone: M, lead: lead.leadReport, lanes: lead.lanes.map(l => l.name), integrate: integ, reviewRounds: fixes }
