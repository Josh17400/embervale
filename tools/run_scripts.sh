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
  files=(tools/scripts/*.txt)
else
  for a in "$@"; do
    if [ -f "$a" ]; then files+=("$a"); else for f in tools/scripts/"$a"*.txt; do [ -f "$f" ] && files+=("$f"); done; fi
  done
fi
[ ${#files[@]} -gt 0 ] || { echo "run_scripts: no scripts matched"; exit 255; }

run_one() {
  local f="$1" name; name="$(basename "$f" .txt)"
  local t0=$SECONDS
  tools/slot.sh "$EXE" --script "$f" > "$LOGS/$name.log" 2>&1
  local rc=$?
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
echo "run_scripts: ${#files[@]} scripts, $nfail failed, $((SECONDS - start)) s"
if [ "$nfail" -eq 0 ] && [ -z "${RUN_LOG_DIR:-}" ]; then rm -rf "$LOGS"; fi
[ "$nfail" -gt 255 ] && nfail=255
exit "$nfail"
