#!/usr/bin/env bash
# tools/gates/wave_gates.sh - the wave gates W1-W7 (docs/SOLID_PLAN.md 4.3), once per wave on the merged wave head.
#
#   tools/gates/wave_gates.sh --bdir <dir> --out <dir> [--only W1,W4,S5] [--w7] [--w7-goals FILE] [--skip-w2]
#                             [--clone <dir>] [step_gates options: --web-bdir --copy --base --expect --baseline]
#
#   --clone DIR  first make a fresh clone of this checkout's HEAD in DIR and run the wave gates there (plan 4.3: a
#                fresh clone and a fresh build dir; give a --bdir that does not exist yet). The clone has no
#                build/_deps, so its first MSVC configure fetches SDL (release-3.4.16) from the network.
#   --w7         run W7 (waves 3 and 4 only): one headless `witness play batch` (Sonnet, low effort)
#
# Lines:
#   W1          step_gates.sh --all (S1-S10 with S4 clang and S8 web) into <out>/step; then
#   W1.msvc_ci  the 68 web.yml commands (tools/gates/ci_cmds.sh) on the MSVC build, from the repo root, 5 side by side
#               (each in its own tools/slot.sh slot): each rc 0. A command that fails under load keeps its log
#               (logs/msvc_fail_<n>.log) and is rerun once alone (every slot held) after the parallel lines; only a
#               failed alone rerun FAILs the line (logs/msvc_fail_<n>.rerun.log)
#   W2.scripts  witness_pack.mjs --all (every script tools/run_scripts.sh would run, on hidden `--witness` instances;
#               never run_scripts.sh itself, which opens one visible window per script): 0 failed, and "v1draws rig2: 0"
#               when witness_pack reports the v1draws sum (state.screen.v1DrawsRun)
#   W2.view_all the same run's hashes.txt checked against probes/view_all/hashes.txt: every PNG identical
#   W3.witness  witness_smoke.sh all15 (all 15 witness/*.witness.json, --repeat 2)
#   W4.doll     tools/gates/doll_gates.sh: every line as at baseline (<out>/doll/summary.txt)
#   W5.firstlaunch  tools/witness_web/firstlaunch.mjs on the S8 web build (site assembled under <out>/site): it reaches
#               the started game (status hidden, title, creator, 3 s) and writes its screenshot
#   W6.digest_clang rpg_test --sim-digest --check probes/sim_digest_clang.txt in the S4 WSL copy
#   W6.perf     tools/gates/perf_pack.sh --check (alone, after everything else): within the wave-0 noise + 3 %
#   W7.play     witness play batch --goals tools/scripts/p2c_playtests.txt (only with --w7; read its report)
# Writes <out>/summary.txt (the step lines, then the W lines) and <out>/build.sha. Exit 0 only when nothing FAILs.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"

# --clone: re-run from a fresh clone
ARGS=("$@"); CLONE=""
for ((i=0; i<${#ARGS[@]}; i++)); do [ "${ARGS[$i]}" = --clone ] && CLONE="${ARGS[$((i+1))]:-}"; done
if [ -n "$CLONE" ]; then
  [ -e "$CLONE" ] && { echo "wave_gates: --clone $CLONE already exists (a fresh clone needs a new dir)" >&2; exit 2; }
  SHA=$(git -C "$REPO" rev-parse HEAD)
  git clone -q --no-hardlinks "$REPO" "$CLONE" && git -C "$CLONE" -c advice.detachedHead=false checkout -q "$SHA" || exit 2
  REST=(); skip=0
  for a in "${ARGS[@]}"; do if [ $skip -eq 1 ]; then skip=0; continue; fi; [ "$a" = --clone ] && { skip=1; continue; }; REST+=("$a"); done
  echo "wave_gates: fresh clone of $SHA in $CLONE"
  exec bash "$CLONE/tools/gates/wave_gates.sh" "${REST[@]}"
fi

. "$REPO/tools/gates/lib.sh"
cd "$REPO"
BDIR=""; OUT=""; ONLY=""; W7=0; W7GOALS="tools/scripts/p2c_playtests.txt"; SKIPW2=0; PASS=()
while [ $# -gt 0 ]; do
  case "$1" in
    --bdir) BDIR="$2"; PASS+=("$1" "$2"); shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --only) ONLY="$2"; shift 2 ;;
    --w7) W7=1; shift ;;
    --w7-goals) W7GOALS="$2"; shift 2 ;;
    --skip-w2) SKIPW2=1; shift ;;
    --web-bdir|--copy|--base|--expect|--baseline) PASS+=("$1" "$2"); shift 2 ;;
    -h|--help) sed -n '2,30p' "$0"; exit 0 ;;
    *) echo "wave_gates: unknown arg $1" >&2; exit 2 ;;
  esac
done
[ -n "$BDIR" ] && [ -n "$OUT" ] || { echo "wave_gates: --bdir and --out are required" >&2; exit 2; }
case "$BDIR" in /*|?:*) BABS="$BDIR" ;; *) BABS="$REPO/$BDIR" ;; esac
BNAME=$(basename "$BDIR"); WEBB="build-web-$BNAME"; COPY="emb_solid_$BNAME"
for ((i=0; i<${#PASS[@]}; i++)); do
  [ "${PASS[$i]}" = --web-bdir ] && WEBB="${PASS[$((i+1))]}"
  [ "${PASS[$i]}" = --copy ] && COPY="${PASS[$((i+1))]}"
done
case "$WEBB" in /*|?:*) WABS="$WEBB" ;; *) WABS="$REPO/$WEBB" ;; esac
mkdir -p "$OUT/logs" "$OUT/res"; OUT="$(cd "$OUT" && pwd)"; rm -f "$OUT"/res/*.txt
write_sha
PROBES="$REPO/tools/gates/baseline/119de74/probes"
BL="$REPO/tools/gates/baseline/119de74/gates"; REC=""

# W1: the step gates with S4 and S8, in their own out dir; their lines are copied into this summary
step_only=""
if [ -n "$ONLY" ]; then IFS=',' read -ra OL <<< "$ONLY"; for o in "${OL[@]}"; do case "$o" in S*) step_only="$step_only,$o" ;; esac; done; step_only="${step_only#,}"; fi
chk_msvc_ci() {   # phase 1 (pool) or the final line after ci_finish
  local n f p k; n=$(grep -c '^ok ' "$2"); f=$(grep -c '^FAIL ' "$2"); p=$(grep -oE 'MSVC_PENDING=[0-9]+' "$2" | tail -1 | cut -d= -f2)
  if [ -n "$p" ] && [ "$p" -gt 0 ]; then echo "FAIL $p command(s) failed under load, pending the alone rerun: $(grep -m2 '^RETRY' "$2" | cut -c1-70 | tr '\n' ' ')"; return; fi
  k=$(grep -oE 'MSVC_RERUN_OK=[0-9]+' "$2" | tail -1 | cut -d= -f2)
  [ -z "$p" ] && n=$(grep -c '^ok ' "$2")
  if [ "$3" -eq 0 ] && [ "$f" -eq 0 ]; then
    [ -n "$p" ] && n=$(grep -c . "$OUT/logs/msvc_cmds.txt")
    echo "PASS $n commands ok, MSVC_FAILS=0$( [ "${k:-0}" -gt 0 ] && echo " ($k ok on the alone rerun; first logs logs/msvc_fail_<n>.log)")"
  else echo "FAIL MSVC_FAILS=$f $(grep -m2 '^FAIL' "$2" | cut -c1-80 | tr '\n' ' ')"; fi
}
# the 68 web.yml commands on the MSVC build, CI_JOBS (5) side by side through tools/slot.sh (lib.sh ci_pool --defer):
# a command that fails keeps its log (logs/msvc_fail_<n>.log) and is rerun alone after the parallel lines (msvc_finish)
ci_exec() {
  local c="$1" exe args; exe="${c%% *}"; args="${c#"${c%% *}"}"
  [ "$exe" = node ] || exe="$CI_BABS/$exe.exe"
  cd "$CI_REPO" || return 98
  # shellcheck disable=SC2086
  "$exe" $args
}
export -f ci_exec; export CI_BABS="$BABS" CI_REPO="$REPO"
run_msvc_ci() { "$REPO/tools/gates/ci_cmds.sh" > "$OUT/logs/msvc_cmds.txt" || return 1; ci_pool "$OUT/logs/msvc_cmds.txt" "$OUT/logs" msvc --defer; }
msvc_finish() {   # rerun W1.msvc_ci's pending failures alone and rewrite its result line (duration = pool + reruns)
  local r="$OUT/res/W1.msvc_ci.txt" p t0 t rc note v
  [ -f "$r" ] || return 0
  p=$(grep -oE 'MSVC_PENDING=[0-9]+' "$OUT/logs/W1.msvc_ci.log" 2>/dev/null | tail -1 | cut -d= -f2)
  [ -n "$p" ] && [ "$p" -gt 0 ] || return 0
  t0=$(date +%s)
  ci_finish "$OUT/logs/msvc_cmds.txt" "$OUT/logs" msvc > "$OUT/logs/W1.msvc_ci.final.log" 2>&1; rc=$?
  t=$(( $(grep -oE ' [0-9]+s \|' "$r" | head -1 | tr -dc 0-9) + $(date +%s) - t0 ))
  note=$(chk_msvc_ci W1.msvc_ci "$OUT/logs/W1.msvc_ci.final.log" "$rc"); v="${note%% *}"; note="${note#* }"
  printf 'W1.msvc_ci %s rc=%s %ss | %s (pool, then %ss of alone reruns)\n' "$v" "$rc" "$t" "$note" "$(( $(date +%s) - t0 ))" | tee "$r"
}
chk_scripts() {   # W2.scripts: witness_pack --all exit 0 is not required (the --check lives in W2.view_all); 0 FAIL scripts
  local f n v sc="$OUT/view_all/scripts.txt"
  [ -f "$sc" ] || { echo "FAIL no $sc (witness_pack did not run) | $(lastline "$2")"; return; }
  n=$(grep -c . "$sc"); f=$(grep -c '^[^ ]* FAIL' "$sc")
  v=$(tr -d '
' < "$OUT/view_all/summary.txt" 2>/dev/null | grep -oE 'v1draws rig2: [0-9]+' | tail -1)
  if [ "$n" -gt 0 ] && [ "$f" -eq 0 ] && { [ -z "$v" ] || [ "$v" = "v1draws rig2: 0" ]; }; then
    echo "PASS $n scripts, 0 failed, ${v:-v1draws not reported by witness_pack}"
  else echo "FAIL $f of $n scripts failed, ${v:-v1draws not reported}: $(grep -m2 '^[^ ]* FAIL' "$sc" | cut -c1-90 | tr '
' ' ')"; fi
}
chk_viewall() {
  if grep -q 'SKIP (tool missing)' "$2"; then echo "SKIP $(lastline "$2")"; return; fi
  [ "$3" -eq 0 ] && echo "PASS $(lastline "$2")" || echo "FAIL $(tr -d '
' < "$2" | grep -m1 -iE 'differ|mismatch|FAIL' | cut -c1-120)"
}
run_view_all() {   # one witness_pack --all run serves W2.scripts and W2.view_all
  local wp="$REPO/tools/gates/witness_pack.mjs" rec="$PROBES/view_all/hashes.txt" h
  [ -f "$rec" ] || rec="$PROBES/view_all_hashes.txt"
  [ -f "$wp" ] || { echo "SKIP (tool missing): $wp"; return 0; }
  if h=$(headed_launch "$wp"); then echo "REFUSED: witness_pack.mjs can start a visible game window ($h)"; return 9; fi
  local ex=(); [ -f "$(dirname "$rec")/exclude.txt" ] && [ "$(dirname "$rec")" != "$PROBES" ] && ex=(--exclude "$(dirname "$rec")/exclude.txt")
  if [ -f "$rec" ]; then node "$wp" --bdir "$BABS" --all --out "$OUT/view_all" --jobs "${WP_JOBS:-5}" --check "$rec" "${ex[@]+"${ex[@]}"}"
  else node "$wp" --bdir "$BABS" --all --out "$OUT/view_all" --jobs "${WP_JOBS:-5}"; local rc=$?
    echo "SKIP (tool missing): no record $rec (scripts ran rc=$rc; W2.scripts judges them)"; return 0; fi
}
run_site() {   # assemble the site like web.yml, then the first-launch probe
  local site="$OUT/site"
  rm -rf "$site"; mkdir -p "$site"
  cp "$REPO"/web/* "$site/" && cp "$WABS/embervale.js" "$WABS/embervale.wasm" "$site/" || { echo "no web build in $WABS (W1's S8)"; return 1; }
  ( cd "$REPO/tools/witness_web" && node firstlaunch.mjs --site "$site" --out "$OUT/firstlaunch.png" --port "${W5_PORT:-8197}" ) || return 1
  [ -s "$OUT/firstlaunch.png" ] && echo "firstlaunch: screenshot $(wc -c < "$OUT/firstlaunch.png") B" || { echo "no screenshot"; return 1; }
}
chk_firstlaunch() { [ "$3" -eq 0 ] && echo "PASS $(lastline "$2")" || echo "FAIL $(lastline "$2")"; }
run_digest_clang() {
  [ -f "$PROBES/sim_digest_clang.txt" ] || { echo "SKIP (tool missing): no record $PROBES/sim_digest_clang.txt"; return 0; }
  MSYS_NO_PATHCONV=1 wsl -d "${WSL_DISTRO:-Ubuntu}" -- bash -lc "cd ~/$COPY && ./build-native/rpg_test --sim-digest --check tools/gates/baseline/119de74/probes/sim_digest_clang.txt" < /dev/null
}
chk_probe() {
  if grep -q 'SKIP (tool missing)' "$2"; then echo "SKIP $(lastline "$2")"; return; fi
  [ "$3" -eq 0 ] && echo "PASS $(lastline "$2")" || echo "FAIL $(lastline "$2")"
}
tool_or_skip() { local p="$1"; shift; [ -e "$p" ] || { echo "SKIP (tool missing): $p"; return 0; }; "$@"; }


# ---------------------------------------------------------------- the run (docs/SOLID_PLAN.md 0.1 Depth: side by side)
# W1 (step_gates --all) starts first. As soon as its S1 build PASSes, W1.msvc_ci's pool and W3 start beside the rest of
# W1; W2 and W4 start when the msvc pool is done (the 68 commands get the slots first: W1.msvc_ci <= 1200 s), still beside
# W1's S4 and the others; W5 starts when S8 is done, W6.digest_clang when S4 is done. Every heavy process goes through
# tools/slot.sh, so the machine cap (5, or 2 while the owner plays) holds. Then W1.msvc_ci's alone reruns, W6.perf alone
# and last, and W7. <out>/parallel.txt = wall vs the sum of the line durations.
rm -rf "$OUT/step/res"; mkdir -p "$OUT/logs"
STEP_PID=""; STEP_S1=0; STEP_S4=0; STEP_S8=0
if [ -z "$ONLY" ] || gate_selected W1 || [ -n "$step_only" ]; then
  ONLY_STEP="$step_only"
  if [ -z "$ONLY" ] || gate_selected W1; then ONLY_STEP=""; fi
  sel_step() { [ -z "$ONLY_STEP" ] && return 0; ( ONLY="$ONLY_STEP"; gate_selected "$1" ); }
  sel_step S1.build && STEP_S1=1; sel_step S4.clang && STEP_S4=1; sel_step S8.build && STEP_S8=1
  if [ -n "$ONLY_STEP" ]; then "$REPO/tools/gates/step_gates.sh" --all "${PASS[@]}" --out "$OUT/step" --only "$ONLY_STEP" > "$OUT/logs/W1.step.log" 2>&1 &
  else "$REPO/tools/gates/step_gates.sh" --all "${PASS[@]}" --out "$OUT/step" > "$OUT/logs/W1.step.log" 2>&1 & fi
  STEP_PID=$!
  echo "wave_gates: W1 step_gates --all started (log $OUT/logs/W1.step.log)"
fi
wait_step_res() {   # wait_step_res NAME: until step_gates wrote res/NAME.txt (or exited); 0 when that line PASSed
  local f="$OUT/step/res/$1.txt"
  while [ ! -f "$f" ] && [ -n "$STEP_PID" ] && kill -0 "$STEP_PID" 2>/dev/null; do sleep 3; done
  grep -q "^$1 PASS " "$f" 2>/dev/null
}
BUILD_OK=1
if [ $STEP_S1 -eq 1 ] && [ -n "$STEP_PID" ]; then wait_step_res S1.build || BUILD_OK=0; fi
not_run() { gate_selected "$1" || return 0; mkdir -p "$OUT/res"; echo "$1 FAIL rc=- 0s | not run: $2" | tee "$OUT/res/$1.txt"; }
BGW=()
if [ $BUILD_OK -eq 1 ]; then
  ( gate_run W1.msvc_ci chk_msvc_ci -- run_msvc_ci ) & MSVC_PID=$!; BGW+=($MSVC_PID)
  ( gate_run W3.witness chk_rc -- "$REPO/tools/gates/witness_smoke.sh" all15 --bdir "$BABS" --out "$OUT/witness15"
    rm -f "$OUT"/witness15/shots/*.png 2>/dev/null ) & BGW+=($!)
  # W2 and W4 wait for the msvc pool (not its alone reruns) so the 68 commands get the slots first
  ( while kill -0 "$MSVC_PID" 2>/dev/null; do sleep 3; done
    if [ $SKIPW2 -eq 0 ]; then
      # never tools/run_scripts.sh or view_pack.sh's old `--script` path: both open visible game windows
      SELW2=0; gate_selected W2.view_all && SELW2=1; gate_selected W2.scripts && SELW2=1
      if [ $SELW2 -eq 1 ]; then
        ONLY_SAVE="$ONLY"; ONLY=""   # the run feeds both lines, so it runs when either is selected
        gate_run W2.view_all chk_viewall -- run_view_all
        ONLY="$ONLY_SAVE"
        if gate_selected W2.scripts; then mkdir -p "$OUT/res"; t=$(grep -oE ' [0-9]+s \|' "$OUT/res/W2.view_all.txt" | head -1 | tr -d ' |')
          r=$(chk_scripts W2.scripts "$OUT/logs/W2.view_all.log" 0); printf 'W2.scripts %s rc=- %s | %s\n' "${r%% *}" "${t:-0s}" "${r#* }" | tee "$OUT/res/W2.scripts.txt"; fi
        gate_selected W2.view_all || rm -f "$OUT/res/W2.view_all.txt"
      fi
    else gate_skip W2.scripts "--skip-w2"; gate_skip W2.view_all "--skip-w2"; fi ) & BGW+=($!)
  ( while kill -0 "$MSVC_PID" 2>/dev/null; do sleep 3; done
    gate_run W4.doll chk_rc -- "$REPO/tools/gates/doll_gates.sh" --bdir "$BABS" --out "$OUT/doll" ) & BGW+=($!)
else
  for n in W1.msvc_ci W2.scripts W2.view_all W3.witness W4.doll; do not_run "$n" "S1 build failed (W1)"; done
fi
( if [ $STEP_S8 -eq 1 ] && [ -n "$STEP_PID" ]; then wait_step_res S8.build && wait_step_res S8.exports; fi
  gate_run W5.firstlaunch chk_firstlaunch -- run_site ) & BGW+=($!)
( if [ $STEP_S4 -eq 1 ] && [ -n "$STEP_PID" ]; then wait_step_res S4.clang; fi
  gate_run W6.digest_clang chk_probe -- run_digest_clang ) & BGW+=($!)
wait "${BGW[@]}" 2>/dev/null
if [ -n "$STEP_PID" ]; then wait "$STEP_PID"; for f in "$OUT"/step/res/*.txt; do [ -f "$f" ] && cp "$f" "$OUT/res/"; done; fi
gate_selected W1.msvc_ci && msvc_finish
# perf last and alone (plan 4.4: timing lines are rerun alone before they count)
run_perf() {
  local pp="$REPO/tools/gates/perf_pack.sh" h
  [ -e "$pp" ] || { echo "SKIP (tool missing): $pp"; return 0; }
  if h=$(headed_launch "$pp"); then echo "REFUSED: perf_pack.sh can start a visible game window ($h)"; return 9; fi
  "$pp" --bdir "$BABS" --out "$OUT/perf"
}
gate_run W6.perf chk_probe -- run_perf
if [ $W7 -eq 1 ]; then
  CFG="$OUT/witness15/witness.gates.json"
  gate_run W7.play chk_rc -- env WITNESS_MAX_PROCS=5 WITNESS_MAX_BROWSERS=1 node "$REPO/../witness/bin/witness.js" play batch --config "$CFG" --goals "$W7GOALS" --parallel 2 --model sonnet --effort low
else gate_skip W7.play "waves 3 and 4 only (--w7)"; fi
# wall vs the sum of the line durations (how much ran side by side)
sumd=$(cat "$OUT"/res/*.txt 2>/dev/null | awk '{ if ($4 ~ /^[0-9]+s$/) s += $4 + 0 } END { print s + 0 }')
wall=$(( $(date +%s) - GATES_T0 ))
echo "PARALLEL wall=${wall}s sum_of_lines=${sumd}s ratio=$(awk -v w=$wall -v s=$sumd 'BEGIN { printf "%.2f", (s > 0 ? w / s : 0) }')" | tee "$OUT/parallel.txt"
gates_summary
