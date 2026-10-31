#!/usr/bin/env bash
# tools/gates/witness_smoke.sh - S9 (and W3): Witness against a given build dir, headless, without touching witness.json.
#
#   tools/gates/witness_smoke.sh test  --bdir DIR --out DIR      the 4 smoke tests (embervale, determinism,
#                                                                death_respawn, touch_snapshot)
#   tools/gates/witness_smoke.sh shot  --bdir DIR --out DIR      shot --at 2 --diff witness/refs/village_start.png
#                                                                --expect "ssim >= 0.999"
#   tools/gates/witness_smoke.sh all15 --bdir DIR --out DIR      W3: every witness/*.witness.json, --repeat 2
#
# witness.json launches build/embervale; this script writes <out>/witness.gates.json, a copy whose launch command is
# <bdir>/embervale.exe (absolute, cwd = this checkout) and whose shots go to <out>/shots, and passes it with --config.
# Test files and the reference are given as absolute paths, so the copy's folder does not matter. Exit code = Witness's.
# Runs with WITNESS_MAX_PROCS=5, WITNESS_MAX_BROWSERS=1 (plan 4.4). Each test file runs as its own `witness test`,
# side by side, each through tools/slot.sh (do not wrap this script in a slot). The game only ever starts as
# `embervale.exe --witness` (hidden window, no audio device); WITNESS_WATCH is unset.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
MODE="${1:-}"; shift || true
BDIR=""; OUT=""
while [ $# -gt 0 ]; do
  case "$1" in
    --bdir) BDIR="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    *) echo "witness_smoke: unknown arg $1" >&2; exit 2 ;;
  esac
done
[ -n "$BDIR" ] && [ -n "$OUT" ] || { echo "witness_smoke: need test|shot|all15 --bdir DIR --out DIR" >&2; exit 2; }
case "$BDIR" in /*|?:*) ;; *) BDIR="$REPO/$BDIR" ;; esac
[ -f "$BDIR/embervale.exe" ] || { echo "witness_smoke: $BDIR/embervale.exe not found" >&2; exit 2; }
mkdir -p "$OUT/shots"
W="$(cd "$REPO/../witness" 2>/dev/null && pwd)/bin/witness.js"
[ -f "$W" ] || { echo "witness_smoke: Witness not found at ../witness" >&2; exit 2; }
CFG="$OUT/witness.gates.json"
REPO_W=$(cygpath -m "$REPO"); EXE_W=$(cygpath -m "$BDIR/embervale.exe"); SHOTS_W=$(cygpath -m "$OUT/shots")
node -e '
const fs = require("fs"); const [src, dst, exe, cwd, shots] = process.argv.slice(1);
const c = JSON.parse(fs.readFileSync(src, "utf8"));
c.launch = { ...c.launch, command: exe, cwd }; c.shots = shots; delete c.build;
// the game only ever runs as `embervale.exe --witness` (hidden window, no audio device): never a visible window
const args = (c.launch.args || []).filter((a) => a !== "--watch" && !/^--(script|shot|play)$/.test(a));
if (!args.includes("--witness")) args.unshift("--witness");
c.launch.args = args;
fs.writeFileSync(dst, JSON.stringify(c, null, 2));' "$REPO/witness.json" "$CFG" "$EXE_W" "$REPO_W" "$SHOTS_W" || exit 2
unset WITNESS_WATCH   # (it would show the game window)
export WITNESS_BUDGET_WAIT="${WITNESS_BUDGET_WAIT:-3600}" WITNESS_MAX_PROCS="${WITNESS_MAX_PROCS:-5}" WITNESS_MAX_BROWSERS="${WITNESS_MAX_BROWSERS:-1}"
cd "$REPO"
# run_files REPEAT FILE...: one `witness test` per file, side by side, each in its own tools/slot.sh slot (one hidden
# game each); prints every file's result lines and "witness_smoke: P passed, F failed of N files (<secs>s)"
run_files() {
  local rep="$1"; shift
  local f b i=0 fails=0 t0=$(date +%s) pids=() names=()
  mkdir -p "$OUT/tlogs"
  for f in "$@"; do
    b=$(basename "$f" .witness.json); names+=("$b")
    ( t1=$(date +%s); "$REPO/tools/slot.sh" node "$W" test --config "$CFG" $rep "$f" > "$OUT/tlogs/$b.log" 2>&1 < /dev/null
      echo "$? $(( $(date +%s) - t1 ))" > "$OUT/tlogs/$b.rc" ) &
    pids+=($!)
  done
  wait "${pids[@]}"
  for b in "${names[@]}"; do
    read -r rc dt < "$OUT/tlogs/$b.rc"
    [ "$rc" -eq 0 ] || fails=$((fails+1))
    echo "$( [ "$rc" -eq 0 ] && echo ok || echo "FAIL rc=$rc") $b ${dt}s | $(tr -d '\r' < "$OUT/tlogs/$b.log" | grep -iE 'pass|fail' | tail -1 | cut -c1-110)"
    [ "$rc" -eq 0 ] || tr -d '\r' < "$OUT/tlogs/$b.log" | grep -iE 'fail|error|expected' | head -4 | sed 's/^/    /'
  done
  echo "witness_smoke: $(( ${#names[@]} - fails )) passed, $fails failed of ${#names[@]} files ($(( $(date +%s) - t0 ))s)"
  [ $fails -eq 0 ]
}
case "$MODE" in
  test)
    run_files "" "$REPO_W/witness/embervale.witness.json" "$REPO_W/witness/determinism.witness.json"       "$REPO_W/witness/death_respawn.witness.json" "$REPO_W/witness/touch_snapshot.witness.json" ;;
  shot)
    exec "$REPO/tools/slot.sh" node "$W" shot --config "$CFG" --at 2 --out "$SHOTS_W/village_start_now.png"       --diff "$REPO_W/witness/refs/village_start.png" --expect "ssim >= 0.999" ;;
  all15)
    files=(); for f in "$REPO_W"/witness/*.witness.json; do files+=("$f"); done
    echo "witness_smoke all15: ${#files[@]} test files, --repeat 2, side by side (slot cap)"
    run_files "--repeat 2" "${files[@]}" ;;
  *) echo "witness_smoke: mode must be test, shot or all15" >&2; exit 2 ;;
esac
