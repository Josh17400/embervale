#!/usr/bin/env bash
# move_check.sh: "was this really only moved?" (SOLID plan 0.3, gate S10). Prints every line that is new in HEAD and
# absent from BASE, so a pure move prints nothing and an edit prints the edited lines.
# Neighbours: check_layers.sh (one-way layers), size_report.sh (size guardrails), step_gates.sh (runs us as S10).
#
# Usage: tools/gates/move_check.sh BASE HEAD [FILES...]
#        tools/gates/move_check.sh BASE..HEAD [FILES...]
#   HEAD may be WORKTREE (or -w) to check uncommitted and untracked work against BASE.
#   FILES limits which files are REPORTED (default: every changed C/C++ file). The pool that moved text may come from
#   is always every C/C++ file changed between BASE and HEAD, plus FILES, so a function cut from game.cpp and pasted
#   into game_items.cpp is matched even when only game_items.cpp is named.
# Output: stdout = one "file:line: text" per new line (pipe to wc -l for the count); stderr = one summary line.
# Exit 0 always on a successful run (the lines are for review); 2 on usage or git errors.
#
# Ignored when comparing: blank lines, brace/paren-only lines, #include and #pragma once, namespace and
# using-namespace lines, forward declarations (struct X; class Y; enum class Z : T;), comments (whole-line, trailing and
# /* block */). Leading and repeated whitespace never counts, so re-indenting a moved block is still a move.
# Counting is by multiset: copying a line (it now exists twice where BASE had it once) prints the copy.
set -u
SRC_RE='\.(cpp|h|hpp|inc|c)$'
[ $# -ge 1 ] || { sed -n '2,20p' "$0"; exit 2; }
if [[ "$1" == *..* ]]; then BASE="${1%%..*}"; HEAD="${1##*..}"; shift
else [ $# -ge 2 ] || { echo "move_check: need BASE HEAD" >&2; exit 2; }; BASE="$1"; HEAD="$2"; shift 2; fi
[ "$HEAD" = "-w" ] && HEAD=WORKTREE
TOP="$(git rev-parse --show-toplevel 2>/dev/null)" || { echo "move_check: not in a git repo" >&2; exit 2; }
cd "$TOP" || exit 2
git rev-parse --verify -q "$BASE^{commit}" >/dev/null || { echo "move_check: bad BASE $BASE" >&2; exit 2; }
[ "$HEAD" = WORKTREE ] || git rev-parse --verify -q "$HEAD^{commit}" >/dev/null || { echo "move_check: bad HEAD $HEAD" >&2; exit 2; }

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
if [ "$HEAD" = WORKTREE ]; then
  { git diff --no-renames --name-only "$BASE" -- 2>/dev/null; git ls-files --others --exclude-standard; } > "$TMP/changed"
else
  git diff --no-renames --name-only "$BASE" "$HEAD" -- > "$TMP/changed"
fi
grep -E "$SRC_RE" "$TMP/changed" > "$TMP/pool"
: > "$TMP/scope"
for f in "$@"; do f="${f#./}"; echo "$f" >> "$TMP/pool"; echo "$f" >> "$TMP/scope"; done
LC_ALL=C sort -u "$TMP/pool" -o "$TMP/pool"
[ -s "$TMP/scope" ] && LC_ALL=C sort -u "$TMP/scope" -o "$TMP/scope" || cp "$TMP/pool" "$TMP/scope"

# Normalizer: reads one file, prints "key<TAB>line" for every line that counts (key = normalized text).
cat > "$TMP/norm.awk" <<'AWK'
BEGIN { inblk = 0 }
{
  s = $0; sub(/\r$/, "", s); out = ""; i = 1; n = length(s); q = ""
  while (i <= n) {
    c = substr(s, i, 1); c2 = substr(s, i, 2)
    if (inblk) { if (c2 == "*/") { inblk = 0; i += 2 } else i++; continue }
    if (q != "") { out = out c; if (c == "\\") { out = out substr(s, i + 1, 1); i += 2; continue } if (c == q) q = ""; i++; continue }
    if (c2 == "//") break
    if (c2 == "/*") { inblk = 1; i += 2; out = out " "; continue }
    if (c == "\"" || c == "'") { q = c; if (c == "'" && substr(s, i - 1, 1) ~ /[0-9A-Fa-f]/ && substr(s, i + 1, 1) ~ /[0-9A-Fa-f]/) q = "" }
    out = out c; i++
  }
  gsub(/[ \t]+/, " ", out); sub(/^ /, "", out); sub(/ $/, "", out)
  if (out == "") next
  if (out ~ /^[{}();, ]*$/) next
  if (out ~ /^# ?include[ <"]/ || out ~ /^# ?pragma once$/) next
  if (out ~ /^(inline )?namespace( |$|\{)/ || out ~ /^using namespace /) next
  if (out ~ /^(template ?<[^;{}]*> ?)?(struct|class|union|enum( class| struct)?) [A-Za-z_][A-Za-z0-9_:]*( ?: ?[A-Za-z_][A-Za-z0-9_: ]*)? ?;$/) next
  print out "\t" FNR
}
AWK

getrev() {  # getrev REV PATH -> contents on stdout (empty when absent)
  if [ "$1" = WORKTREE ]; then [ -f "$2" ] && cat "$2"; else git show "$1:$2" 2>/dev/null; fi
}
: > "$TMP/base.keys"; : > "$TMP/head.keys"; : > "$TMP/added"
while IFS= read -r f; do
  getrev "$BASE" "$f" | gawk -f "$TMP/norm.awk" | cut -f1 >> "$TMP/base.keys"
  getrev "$HEAD" "$f" | gawk -f "$TMP/norm.awk" > "$TMP/h1"
  cut -f1 "$TMP/h1" >> "$TMP/head.keys"
  grep -qxF "$f" "$TMP/scope" || continue
  # added line numbers in HEAD for this file (whole file when it is new or untracked)
  if [ "$HEAD" = WORKTREE ]; then
    if git ls-files --error-unmatch -- "$f" >/dev/null 2>&1 || git cat-file -e "$BASE:$f" 2>/dev/null; then
      git diff -U0 --no-color --no-ext-diff --no-renames "$BASE" -- "$f" 2>/dev/null
    else [ -f "$f" ] && awk 'BEGIN{n=0} {n++} END{print "@@ -0,0 +1," n " @@"}' "$f"; fi
  else
    git diff -U0 --no-color --no-ext-diff --no-renames "$BASE" "$HEAD" -- "$f"
  fi | gawk '/^@@ /{ split($3, a, ","); st = substr(a[1], 2) + 0; ct = (a[2] == "" ? 1 : a[2] + 0); for (k = 0; k < ct; k++) print st + k }' > "$TMP/addnums"
  getrev "$HEAD" "$f" > "$TMP/hraw"
  gawk -F'\t' -v F="$f" -v RAW="$TMP/hraw" -v NUMS="$TMP/addnums" '
    BEGIN { while ((getline n < NUMS) > 0) add[n] = 1; k = 0; while ((getline l < RAW) > 0) { k++; raw[k] = l } }
    ($2 in add) { t = raw[$2]; sub(/\r$/, "", t); print $1 "\t" F ":" $2 ": " t }' "$TMP/h1" >> "$TMP/added"
done < "$TMP/pool"

# A candidate prints while its key still has excess (HEAD count above BASE count).
gawk -F'\t' -v BK="$TMP/base.keys" -v HK="$TMP/head.keys" '
  BEGIN { while ((getline k < BK) > 0) b[k]++; while ((getline k < HK) > 0) h[k]++; for (k in h) ex[k] = h[k] - b[k] }
  ex[$1] > 0 { ex[$1]--; print $2; n++ }
  END { printf "move_check: %d new line(s) in %s..%s\n", n + 0, base, head > "/dev/stderr" }' base="$BASE" head="$HEAD" "$TMP/added"
exit 0
