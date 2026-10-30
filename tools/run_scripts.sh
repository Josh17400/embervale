#!/usr/bin/env bash
# tools/run_scripts.sh - run game scripts as one batch and print only the failures.
#
#   tools/run_scripts.sh                       every tools/scripts/*.txt (the full regression sweep: run ONCE per
#                                              milestone, by the integrator or before shipping - not by every fixer)
#   tools/run_scripts.sh m8b_ loot_            only scripts whose names start with these prefixes
#   tools/run_scripts.sh tools/scripts/a.txt   exact files
#
# Each script runs through tools/slot.sh (the machine-wide cap), up to EMB_SLOTS at once. Logs go to
# $RUN_LOG_DIR (default: a temp dir, deleted unless something failed). Exit code = number of failed scripts (max 255).
# (P2c flip, D103) each run's `v1draws rig2 <n> rig1 <n>` line is collected: the batch prints `v1draws rig2: <n> scripts
# with v1 frames` (0 wanted, the names follow) and the rig-1 counts as the fallback's coverage; a script with rig-1
# frames and no `rig 1` line fails as a stray.
# Environment: BDIR (build dir, default build), EMB_SLOTS (default 3), RUN_LOG_DIR.

set -u
cd "$(dirname "$0")/.."
BDIR="${BDIR:-build}"
EXE="$BDIR/embervale.exe"
[ -x "$EXE" ] || { echo "run_scripts: $EXE not found (build it first)"; exit 255; }
JOBS="${EMB_SLOTS:-3}"
LOGS="${RUN_LOG_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/emb_scripts.XXXXXX")}"
mkdir -p "$LOGS"

files=()
if [ $# -eq 0 ]; then
  # (P2b) *_playtests.txt are witness play batch goal lists (VISION_PLAN 15.36 B13), not game scripts
  for f in tools/scripts/*.txt; do case "$f" in *_playtests.txt) ;; *) files+=("$f") ;; esac; done
else
  for a in "$@"; do
    if [ -f "$a" ]; then files+=("$a"); else for f in tools/scripts/"$a"*.txt; do case "$f" in *_playtests.txt) continue ;; esac; [ -f "$f" ] && files+=("$f"); done; fi
  done
fi
[ ${#files[@]} -gt 0 ] || { echo "run_scripts: no scripts matched"; exit 255; }

run_one() {
  local f="$1" name; name="$(basename "$f" .txt)"
  local t0=$SECONDS
  tools/slot.sh "$EXE" --script "$f" > "$LOGS/$name.log" 2>&1
  local rc=$?
  # (P2c flip, D103: C2) the run's `v1draws rig2 <n> rig1 <n>` line: v1 people frames at the default rig 2 are wanted 0;
  # rig-1 frames are the fallback's coverage, allowed only in a script that opts in with a `rig 1` line (else a stray)
  local v1; v1="$(grep -m1 '^v1draws rig2 ' "$LOGS/$name.log" 2>/dev/null)"
  if [ -n "$v1" ]; then
    local r2 r1; r2="$(echo "$v1" | awk '{print $3}')"; r1="$(echo "$v1" | awk '{print $5}')"
    echo "$name $r2 $r1" > "$LOGS/$name.v1draws"
    if [ "${r1:-0}" != "0" ] && ! grep -qE '^[^#]*[0-9.]+[[:space:]]+rig[[:space:]]+1([[:space:]]|$)' "$f"; then
      echo "FAIL $name: $r1 v1 people frames at rig 1 with no 'rig 1' line (a stray) - log: $LOGS/$name.log"; touch "$LOGS/$name.failed"; return
    fi
  fi
  if [ $rc -ne 0 ]; then echo "FAIL $name (exit $rc, $((SECONDS - t0)) s) - log: $LOGS/$name.log"; touch "$LOGS/$name.failed"
  else echo "ok   $name ($((SECONDS - t0)) s)"; fi
}
export -f run_one; export EXE LOGS

start=$SECONDS
touch "$LOGS/.start"
printf '%s\n' "${files[@]}" | xargs -P "$JOBS" -I{} bash -c 'run_one "$@"' _ {} | grep -v '^ok ' || true
# scripts write their screenshots into the repo root: remove the ones this batch made
find . -maxdepth 1 -name "*.png" -newer "$LOGS/.start" -delete 2>/dev/null
nfail=$(ls "$LOGS"/*.failed 2>/dev/null | wc -l)
# (P2c flip, D103) C2 over the batch: the scripts with v1 people frames at rig 2 (0 wanted), then the fallback's coverage
v1bad="$(cat "$LOGS"/*.v1draws 2>/dev/null | awk '$2 != "0" {print $1 " (" $2 ")"}')"
echo "v1draws rig2: $(printf '%s' "$v1bad" | grep -c .) scripts with v1 frames${v1bad:+: $(echo $v1bad)}"
v1cov="$(cat "$LOGS"/*.v1draws 2>/dev/null | awk '$3 != "0" {print $1 " " $3}')"
[ -n "$v1cov" ] && echo "v1draws rig1 (the fallback's coverage): $(echo "$v1cov" | paste -sd, - | sed 's/,/, /g')"
echo "run_scripts: ${#files[@]} scripts, $nfail failed, $((SECONDS - start)) s"
if [ "$nfail" -eq 0 ] && [ -z "${RUN_LOG_DIR:-}" ]; then rm -rf "$LOGS"; fi
[ "$nfail" -gt 255 ] && nfail=255
exit "$nfail"
