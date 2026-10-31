#!/usr/bin/env bash
# tools/gates/view_pack.sh - the view identity probe (docs/SOLID_PLAN.md wave 0, step 0.2): a thin wrapper over
# tools/gates/witness_pack.mjs, which plays the game scripts on HIDDEN `embervale.exe --witness` instances (fixed 1/60 s
# steps, no clock, no audio device) and hashes every PNG they write. No visible window is ever opened.
#
#   tools/gates/view_pack.sh [--quick | --all | <script names or paths>...] [--bdir DIR] [--out DIR] [--jobs N]
#                            [--check FILE] [--exclude FILE] [--record] [--keep]
#
#   --quick      tools/gates/view_pack_quick.txt (about 40 scripts). The default.
#   --all        every script tools/run_scripts.sh would run (W2)
#   --bdir DIR   build dir holding embervale.exe (default: $BDIR, else build)
#   --out DIR    hashes.txt, scripts.txt, summary.txt, logs/ (default: a new temp dir)
#   --jobs N     instances at once (default 5, max 5; each holds a tools/slot.sh slot)
#   --check FILE the record to compare with (default: --quick tools/gates/baseline/119de74/probes/view_quick/hashes.txt,
#                --all .../view_all/hashes.txt; a named-scripts run checks against view_all)
#   --exclude F  "<script>[/<png>]  # cause" lines left out of the comparison (default: exclude.txt next to the record)
#   --record     no comparison (how a baseline is made: copy <out>/hashes.txt and scripts.txt into the record dir)
#   --keep       keep the PNGs under <out>/runs/
#
# Exit 0 only when no script FAILed and every compared PNG is identical; 3 when only INFRA (launch failed 3 times)
# scripts kept it from passing. The last line is "view_pack PASS|INFRA|FAIL: ...".
set -u
unset WITNESS_WATCH   # (Witness passes its environment to the game; WITNESS_WATCH=1 would show the hidden window)
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
MODE=quick; BDIR="${BDIR:-build}"; OUT=""; JOBS=5; CHECK=""; EXCL=""; RECORD=0; KEEP=0; NAMES=()
while [ $# -gt 0 ]; do
  case "$1" in
    --quick) MODE=quick ;;
    --all) MODE=all ;;
    --bdir) BDIR="$2"; shift ;;
    --out) OUT="$2"; shift ;;
    --jobs) JOBS="$2"; shift ;;
    --check) CHECK="$2"; shift ;;
    --exclude) EXCL="$2"; shift ;;
    --record) RECORD=1 ;;
    --keep) KEEP=1 ;;
    -h|--help) sed -n '2,21p' "$0"; exit 0 ;;
    *) MODE=names; NAMES+=("$1") ;;
  esac
  shift
done
BASE="$ROOT/tools/gates/baseline/119de74/probes"
if [ -z "$CHECK" ] && [ $RECORD -eq 0 ]; then
  case "$MODE" in quick) CHECK="$BASE/view_quick/hashes.txt" ;; *) CHECK="$BASE/view_all/hashes.txt" ;; esac
fi
[ -z "$OUT" ] && OUT="$(mktemp -d "${TMPDIR:-/tmp}/emb_viewpack.XXXXXX")"
OUT="$(mkdir -p "$OUT" && cd "$OUT" && pwd)"
command -v cygpath >/dev/null 2>&1 && OUT="$(cygpath -m "$OUT")"
args=(--bdir "$BDIR" --out "$OUT" --jobs "$JOBS")
case "$MODE" in quick) args+=(--quick) ;; all) args+=(--all) ;; names) args+=("${NAMES[@]}") ;; esac
[ -n "$CHECK" ] && args+=(--check "$CHECK")
[ -n "$EXCL" ] && args+=(--exclude "$EXCL")
[ $KEEP -eq 1 ] && args+=(--keep)
command -v cygpath >/dev/null 2>&1 && WP="$(cygpath -m "$ROOT/tools/gates/witness_pack.mjs")" || WP="$ROOT/tools/gates/witness_pack.mjs"
cd "$ROOT"
node "$WP" "${args[@]}" < /dev/null | tee "$OUT/view_pack.log"
rc=${PIPESTATUS[0]}
sum="$(head -1 "$OUT/summary.txt" 2>/dev/null)"
chk="$(grep -E '^witness_pack (PASS|FAIL):' "$OUT/view_pack.log" | tail -1)"
if [ $rc -eq 0 ]; then echo "view_pack PASS: ${chk:-$sum}"
elif [ $rc -eq 3 ]; then echo "view_pack INFRA: ${chk:-$sum} (rc 3: a launch failed 3 times, see summary.txt; not a pixel difference)"
else echo "view_pack FAIL: ${chk:-$sum} (rc $rc)"; fi
exit $rc
