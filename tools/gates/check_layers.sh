#!/usr/bin/env bash
# check_layers.sh: the one-way layer check (SOLID plan 0.3, gate S10). Lists forbidden includes per folder and
# fails on any violation that is not in tools/gates/layers_allow.txt (today's knots; that file may only shrink).
# Neighbours: move_check.sh (was it only moved?), size_report.sh (size guardrails), step_gates.sh (runs us as S10).
#
# Usage: tools/gates/check_layers.sh [--root DIR] [--allow FILE] [--emit] [--quiet]
#   --root DIR    tree to scan (default: the repo holding this script); use it on a scratch copy
#   --allow FILE  allowlist (default: <script dir>/layers_allow.txt)
#   --emit        print today's violations in allowlist format (to seed or shrink layers_allow.txt) and exit 0
# Exit 0 = no new violation and the allowlist did not grow; 1 = new violation or grown allowlist; 2 = usage error.
#
# Rules (an arrow means "may include"; nothing includes upward):
#   sim-from-lower    rpg/world, rpg/culture, rpg/build include something under rpg/sim/
#   view-from-sim     rpg/sim includes something under rpg/view/
#   foreign-internal  a file includes a *_internal.h whose folder is neither its own nor an ancestor of it
#                     (rpg/art/rig may use rpg/art/art_internal.h; tests use listed seams only)
#   mark-mix          the mark mix constant 0xD1B54A32D192ED03 restated in a mix64 call outside rpg/sim/mark_tags.h
#
# Allowlist line: "<rule> <file> <target>" (target = resolved include path, or MIX for mark-mix). A line may repeat:
# each mark-mix occurrence needs its own line. '#' starts a comment.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
ALLOW="$HERE/layers_allow.txt"
BASE_ALLOW="$HERE/baseline/119de74/checks/layers_allow.txt"
EMIT=0; QUIET=0
while [ $# -gt 0 ]; do
  case "$1" in
    --root) ROOT="$(cd "$2" && pwd)" || exit 2; shift 2 ;;
    --allow) ALLOW="$2"; shift 2 ;;
    --emit) EMIT=1; shift ;;
    --quiet) QUIET=1; shift ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) echo "check_layers: unknown arg $1" >&2; exit 2 ;;
  esac
done
cd "$ROOT" || exit 2

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
# Every C/C++ source under the scanned folders (build dirs and .tmp are outside them).
find rpg engine tools -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.inc' -o -name '*.c' \) \
  | sed 's#^\./##' | LC_ALL=C sort > "$TMP/files"
# include lines: file:line:text (quoted includes only; system headers are not ours)
xargs -a "$TMP/files" grep -nHE '^[[:space:]]*#[[:space:]]*include[[:space:]]*"' > "$TMP/inc" 2>/dev/null
# mark mix restatements
xargs -a "$TMP/files" grep -nHiE '0xD1B54A32D192ED03' 2>/dev/null | grep -E 'mix64' > "$TMP/mix"

gawk -v FILES="$TMP/files" -v MIX="$TMP/mix" '
function norm(p,   n, i, a, out, k) {
  n = split(p, a, "/"); k = 0
  for (i = 1; i <= n; i++) {
    if (a[i] == "" || a[i] == ".") continue
    if (a[i] == "..") { if (k > 0) k--; continue }
    out[++k] = a[i]
  }
  p = ""; for (i = 1; i <= k; i++) p = p (i > 1 ? "/" : "") out[i]
  return p
}
function dirof(p) { if (p ~ /\//) { sub(/\/[^\/]*$/, "", p); return p } return "." }
BEGIN { while ((getline f < FILES) > 0) have[f] = 1 }
{
  # file:line:text  (paths contain no colons)
  file = $0; sub(/:.*/, "", file)
  rest = substr($0, length(file) + 2); line = rest; sub(/:.*/, "", line)
  txt = substr(rest, length(line) + 2)
  if (!match(txt, /"[^"]+"/)) next
  inc = substr(txt, RSTART + 1, RLENGTH - 2)
  # resolve: repo-root first (the CMake include dir), then relative to the includer
  r1 = norm(inc); r2 = norm(dirof(file) "/" inc)
  if (r1 in have) tgt = r1; else if (r2 in have) tgt = r2; else tgt = r1
  if (file ~ /^rpg\/(world|culture|build)\// && tgt ~ /^rpg\/sim\//) emit("sim-from-lower", file, line, tgt)
  if (file ~ /^rpg\/sim\// && tgt ~ /^rpg\/view\//) emit("view-from-sim", file, line, tgt)
  b = tgt; sub(/.*\//, "", b)
  # own folder or an ancestor of it is home (rpg/story/req may use rpg/story/story_internal.h); anything else is foreign
  td = dirof(tgt); fd = dirof(file)
  if (b ~ /_internal\.h$/ && td != fd && index(fd "/", td "/") != 1) emit("foreign-internal", file, line, tgt)
}
function emit(rule, file, line, tgt,   key) {
  key = rule " " file " " tgt
  if (key in seen) return            # a repeated include of the same header is one violation
  seen[key] = 1; print key "\t" line
}
END {
  while ((getline l < MIX) > 0) {
    file = l; sub(/:.*/, "", file)
    rest = substr(l, length(file) + 2); line = rest; sub(/:.*/, "", line)
    if (file == "rpg/sim/mark_tags.h") continue
    print "mark-mix " file " MIX\t" line
  }
}' "$TMP/inc" | LC_ALL=C sort > "$TMP/found"

if [ "$EMIT" = 1 ]; then cut -f1 "$TMP/found"; exit 0; fi

if [ ! -f "$ALLOW" ]; then echo "check_layers: FAIL no allowlist at $ALLOW" >&2; exit 2; fi
grep -vE '^[[:space:]]*(#|$)' "$ALLOW" | sed 's/[[:space:]]*#.*$//; s/[[:space:]]\+/ /g; s/ $//' | LC_ALL=C sort > "$TMP/allow"

# Multiset compare: a found line is new when it occurs more often than the allowlist permits.
gawk -F'\t' -v ALLOWF="$TMP/allow" '
BEGIN { while ((getline a < ALLOWF) > 0) cap[a]++ }
{ used[$1]++; if (used[$1] > cap[$1]) { split($1, p, " "); printf "NEW   %-16s %s:%s -> %s\n", p[1], p[2], $2, p[3]; nnew++ } }
END {
  for (a in cap) if (used[a] < cap[a]) { printf "STALE %s (x%d unused: remove it, the allowlist only shrinks)\n", a, cap[a] - used[a]; nst++ }
  printf "#COUNTS %d %d\n", nnew + 0, nst + 0
}' "$TMP/found" > "$TMP/report"

NEW=$(grep -c '^NEW' "$TMP/report"); STALE=$(grep -c '^STALE' "$TMP/report")
TOTAL=$(wc -l < "$TMP/found" | tr -d ' '); NALLOW=$(wc -l < "$TMP/allow" | tr -d ' ')

# The allowlist may only shrink: every line must also be in the wave-0 record (multiset).
GREW=0
if [ -f "$BASE_ALLOW" ] && [ "$(cd "$(dirname "$ALLOW")" && pwd)/$(basename "$ALLOW")" != "$BASE_ALLOW" ]; then
  grep -vE '^[[:space:]]*(#|$)' "$BASE_ALLOW" | sed 's/[[:space:]]*#.*$//; s/[[:space:]]\+/ /g; s/ $//' | LC_ALL=C sort > "$TMP/base"
  LC_ALL=C comm -23 "$TMP/allow" "$TMP/base" > "$TMP/grown"
  GREW=$(wc -l < "$TMP/grown" | tr -d ' ')
  [ "$GREW" -gt 0 ] && sed 's/^/GREW  /' "$TMP/grown" >> "$TMP/report"
fi

[ "$QUIET" = 1 ] || grep -v '^#COUNTS' "$TMP/report"
if [ "$NEW" -eq 0 ] && [ "$GREW" -eq 0 ]; then
  echo "check_layers: PASS violations=$TOTAL allowed=$NALLOW new=0 stale=$STALE"
  exit 0
fi
echo "check_layers: FAIL violations=$TOTAL allowed=$NALLOW new=$NEW stale=$STALE allowlist_grew=$GREW"
exit 1
