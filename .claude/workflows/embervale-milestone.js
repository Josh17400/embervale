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
Repo: C:\\Users\\joshu\\Documents\\embervale (github.com/Josh17400/embervale). The game ships as a web build (Emscripten,
.github/workflows/web.yml -> GitHub Pages, played from the iPhone Home Screen) and a Windows exe. ALL art and audio are generated in code.
Read docs/VISION_PLAN.md (especially the milestone ${M} section and section 15, the owner addendum, which wins on conflicts) and docs/PLAN.md.
Tools: build with  cmd //c "set BDIR=<dir>&& tools\\\\build.bat <target>"  (targets: embervale, rpg_test, save_test, art_preview).
rpg_test --seeds 1..20 ; save_test ; embervale.exe --script tools/scripts/<x>.txt (see rpg/main.cpp header for the script language and flags
like --play --seed --goto --enter --fight --hour --menu --shot --after).
Batch scripts: tools/run_scripts.sh [prefix_ ...] (only failures printed). Test what you changed; the full sweep runs once per milestone.
TEST ONCE (owner rule, non-negotiable): the check of every system in the game runs exactly ONCE per milestone, by the integrator.
Nobody else re-tests the whole game, and nobody re-runs the same check twice "to be sure". Re-running a check is fine only after you
changed code it covers. Past milestones took 24 h instead of 5 h because every round re-tested everything; do not repeat that.
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
- Run checks in the foreground (long timeouts are fine). Never end your turn while a background command is still running: it is
  killed when you stop and your report would be unverified (the M12 round-2 fixer ended on "Waiting for the batch notification").
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
          kind: { type: 'string', enum: ['code', 'visual'] },
        },
        required: ['name', 'bdir', 'owns', 'brief', 'kind'],
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
Give each lane its own bdir (build_<name>) and a kind: "visual" for art/audio/render/UI-look lanes, "code" for everything else.
Record your contracts in docs/VISION_PLAN.md as the next 15.x section and the milestone entry in docs/PLAN.md, like earlier milestones.
Return leadReport + lanes.`, { label: `${M}-lead`, phase: 'Lead', agentType: 'engineer-high', schema: LANES_SCHEMA })

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
the quality bar.`, { label: `${M}-lane-${l.name}`, phase: 'Lanes', agentType: l.kind === 'visual' ? 'artist' : 'engineer' })))

const lanesText = lead.lanes.map((l, i) => `### ${l.name}\n${laneReports[i] || '(lane returned nothing)'}`).join('\n\n')

phase('Integrate')
const integ = await agent(`${CTX}

You are the INTEGRATOR for ${M}. All lanes finished (reports below). BDIR=build; you may edit any file to make things work together.
1. Clean-build embervale, rpg_test, save_test, art_preview (if present). Fix errors; fix warnings that are real bugs.
2. rpg_test --seeds 1..20 ; save_test ; the CI suites in .github/workflows/web.yml ; then the FULL script regression sweep ONCE:
   'tools/run_scripts.sh' (runs every tools/scripts/*.txt in a batch through slot.sh and prints only failures). Fix every
   failure. This is the only full sweep in the milestone, so make it count.
3. Screenshot tour that exercises everything ${M} changed (day and night, inside and outside, several seeds); inspect each against the
   quality bar; fix what you can.
4. Portability pass for clang/Emscripten. A CI-equivalent Linux clang 18 build lives in WSL: refresh the copy with
   wsl -d Ubuntu -- bash -lc "rsync -a --delete --exclude='/build*' --exclude='/.git' /mnt/c/Users/joshu/Documents/embervale/ ~/emb/ && cd ~/emb && ninja -C build-native rpg_test"
   then run ~/emb/build-native/rpg_test --seeds 1..20 there ONCE (MSVC hides use-after-free and eval-order bugs; M11 shipped one).
Return a concise integration report with test numbers and a list of known open problems.

LEAD: ${lead.leadReport}

LANES:
${lanesText}`, { label: `${M}-integrate`, phase: 'Integrate', agentType: 'engineer' })

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
  { key: 'bugs', text: 'CORRECTNESS of what this milestone changed: crashes, logic bugs, save compatibility, soft-locks, quest breakage, AI pathologies. Start from the diff (git diff HEAD --stat, then the files) and only test areas the diff touches or directly feeds; run targeted repro scripts/tests. Do not tour unrelated systems - the integrator already ran the full regression sweep.' },
  { key: 'look', text: "VISUAL QUALITY + PLAYER FEEL + PLATFORM, for what this milestone added or changed only (check git diff HEAD --stat to see what that is). (a) Screenshot those features in the situations they appear (day/night, phone and desktop, a few seeds) against the owner bar: flag anything cheap, broken, disconnected, flat, inconsistent or repetitive - commercial quality is the bar. (b) Play scripted sessions that use them as a phone player would (touch taps via script, small-screen readability, UI hit targets, clarity of what to do). (c) clang/Emscripten portability and --perf of the changed code. Do not tour or replay unrelated parts of the game." },
]
const REVIEWER_TYPE = { bugs: 'code-reviewer', look: 'eyes' }
const fixPrompt = (round, findings) => `${CTX}

You are the FIXER for ${M}, round ${round}. BDIR=build. Fix every "must" finding below and as many "should" findings as are safe.
For each, verify the fix with TARGETED checks only: the test suites and scripts that cover the files you changed, this milestone's own
scripts ('tools/run_scripts.sh <milestone prefix>_', e.g. m10_), this milestone's own rpg_test suites (the ones the lead/lanes added
to .github/workflows/web.yml, e.g. --rise / --lord / --service), and any script that failed before. At the end run the fast core ONCE:
rpg_test --seeds 1..20, save_test, --golden, --endless --golden, --towns --golden. Do NOT run the whole tools/scripts/ sweep or
replay unrelated content (homes, sieges, unrelated quests, town tours) unless your change touched that code. The full sweep already ran
once (integrator) and does not run again.
Report: per finding fixed / not fixed (why), and the exact checks you ran.

FINDINGS:
${JSON.stringify(findings, null, 1)}`

const fixes = []
// Round 1: the one full review of what the milestone changed (diff-scoped, two lenses).
const found = (await parallel(LENSES.map(ls => () => agent(`${CTX}

You are a REVIEWER for ${M}, lens: ${ls.text}
Do not edit code. Check each feature once. Report only real, verified problems you observed (with repro/screenshot evidence in "detail").
Severity "must" = should block shipping or clearly misses the quality bar; "should" = worth fixing soon.
Integration report for context:
${integ}`, { label: `${M}-review-${ls.key}`, phase: 'Review', agentType: REVIEWER_TYPE[ls.key], schema: FINDINGS }))))
  .filter(Boolean).flatMap(r => r.findings)
const seen = new Set()
const fresh = found.filter(f => { const k = (f.title + '|' + f.where).toLowerCase(); if (seen.has(k)) return false; seen.add(k); return true })
log(`review: ${fresh.length} findings, ${fresh.filter(f => f.severity === 'must').length} must-fix`)

if (fresh.length) {
  const fix1 = await agent(fixPrompt(1, fresh), { label: `${M}-fix-r1`, phase: 'Review', agentType: 'engineer' })
  fixes.push({ round: 1, findings: fresh, fix: fix1 })

  // Round 2: re-check ONLY the round-1 findings and the code the fixer touched. No new tour of the milestone or the game.
  const recheck = await agent(`${CTX}

You are the RE-CHECKER for ${M}. Do not edit code. Verify ONLY the findings below against the fixer's report: for each one, run the
single repro/check that proves it fixed or not (one run each), and look for breakage in the code the fixer touched (git diff since the
fixer started is in its report; use git diff HEAD on those files). Do not re-review the milestone, do not run the full sweep, do not
re-run the fast core unless a fix you are checking changed generation or saves. Return only findings that are still broken or newly
broken by the fixes.

FINDINGS:
${JSON.stringify(fresh, null, 1)}

FIXER REPORT:
${fix1}`, { label: `${M}-recheck`, phase: 'Review', agentType: 'verifier', schema: FINDINGS })
  const left = (recheck && recheck.findings) || []
  log(`recheck: ${left.length} still open, ${left.filter(f => f.severity === 'must').length} must-fix`)
  if (left.some(f => f.severity === 'must')) {
    const fix2 = await agent(fixPrompt(2, left), { label: `${M}-fix-r2`, phase: 'Review', agentType: 'engineer-high' })
    fixes.push({ round: 2, findings: left, fix: fix2 })
  }
}

return { milestone: M, lead: lead.leadReport, lanes: lead.lanes.map(l => l.name), integrate: integ, reviewRounds: fixes }
