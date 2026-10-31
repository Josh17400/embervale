#!/usr/bin/env bash
# size_report.sh: the size guardrails of SOLID plan rule 7 (gate S10, judge check 5). Lists hand-written files over
# 2,000 lines and functions over 300 lines, apart from today's cohesive big files in tools/gates/size_allow.txt.
# Neighbours: check_layers.sh (one-way layers), move_check.sh (was it only moved?), baseline/119de74/checks/.
#
# Usage: tools/gates/size_report.sh [--root DIR] [--allow FILE] [--all] [--check BASELINE] [--file-max N] [--func-max N]
#   --all             also print the allowlisted items (ALLOW lines)
#   --check BASELINE  fail (exit 1) when an OVER item is not in BASELINE (a saved report): a new big file, or a new
#                     big function (functions are matched by name, so moving one to another file is not "new");
#                     items larger than at baseline print as GREW (information, not a failure)
#   --func NAME       print the length of every function whose name contains NAME (for step reports)
# Output lines: "OVER file <lines> <path>", "OVER func <lines> <path>:<line> <name>", then one summary line.
# Exit 0 = report written (and --check passed); 1 = --check found a new item; 2 = usage error.
#
# Function lengths come from a brace scanner over comment- and string-stripped text: a function is a {...} block whose
# header ends like a signature (")" plus qualifiers, or a constructor's init list) at namespace or class scope.
# Lambdas and blocks inside a function count toward it. Data initialisers (" = {") are not functions.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
ALLOW="$HERE/size_allow.txt"
ALL=0; CHECK=""; FILEMAX=2000; FUNCMAX=300; FUNCQ=""
while [ $# -gt 0 ]; do
  case "$1" in
    --root) ROOT="$(cd "$2" && pwd)" || exit 2; shift 2 ;;
    --allow) ALLOW="$2"; shift 2 ;;
    --all) ALL=1; shift ;;
    --check) CHECK="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"; shift 2 ;;
    --file-max) FILEMAX="$2"; shift 2 ;;
    --func-max) FUNCMAX="$2"; shift 2 ;;
    --func) FUNCQ="$2"; shift 2 ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) echo "size_report: unknown arg $1" >&2; exit 2 ;;
  esac
done
cd "$ROOT" || exit 2
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
find rpg engine tools -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.inc' -o -name '*.c' \) \
  | sed 's#^\./##' | LC_ALL=C sort > "$TMP/files"
[ -f "$ALLOW" ] && grep -vE '^[[:space:]]*(#|$)' "$ALLOW" | sed 's/[[:space:]]*#.*$//' > "$TMP/allow" || : > "$TMP/allow"

# Scan: prints "F <lines> <path>" for every file, "U <lines> <path> <line> <name>" for every function.
xargs -a "$TMP/files" gawk '
function flush_fn(endl) { if (fstart > 0) printf "U %d %s %d %s\n", endl - fstart + 1, FILENAME, fstart, fname; fstart = 0 }
function classify(h,   t, p, nm, depth, i, c) {
  t = h; gsub(/[ \t]+/, " ", t); sub(/^ /, "", t); sub(/ $/, "", t)
  while (sub(/^(public|private|protected) ?: ?/, "", t)) {}
  if (t ~ /(^|[^A-Za-z0-9_])namespace([^A-Za-z0-9_]|$)/ || t ~ /^extern ""/) return "N"
  if (t ~ /^(template ?<.*> ?)?(typedef )?(struct|class|union|enum)([^A-Za-z0-9_]|$)/ && t !~ /\) ?(const )?(noexcept )?(override )?$/) return "C"
  p = index(t, "(")
  if (p == 0) return "B"
  # "=" before the first "(" means an initialiser (data) unless it is a lambda: "auto f = [](...) {"
  if (substr(t, 1, p - 1) ~ /=/ && t !~ /= ?\[[^\]]*\] ?\(/) return "B"
  if (t ~ /^(if|for|while|switch|catch|do|else|return)([^A-Za-z0-9_]|$)/) return "B"
  if (t ~ /\)( ?(const|noexcept|override|final|mutable|volatile|&|&&|try))* ?(-> ?[^{};]+)?$/ || t ~ /\) ?(noexcept ?)?: ?[A-Za-z_]/) {
    if (substr(t, 1, p - 1) ~ /=/) { nm = substr(t, 1, p - 1); sub(/ ?=.*/, "", nm) }
    else nm = substr(t, 1, p - 1)
    sub(/ $/, "", nm); if (nm ~ /operator ?$/) { nm = nm "()" } else { sub(/.* /, "", nm); sub(/^[*&]+/, "", nm) }
    if (nm == "") nm = "?"
    fname = (cls != "" && nm !~ /::/) ? cls "::" nm : nm
    return "F"
  }
  return "B"
}
FNR == 1 {
  if (NR > 1) { flush_fn(prevlines); printf "F %d %s\n", prevlines, prevfile }
  sp = 0; inblk = 0; raw = ""; hdr = ""; hline = 0; fstart = 0; infn = 0; cont = 0; cls = ""; initd = 0
}
{
  prevlines = FNR; prevfile = FILENAME
  s = $0; sub(/\r$/, "", s)
  if (raw == "" && !inblk) {
    if (cont) { cont = (s ~ /\\$/); next }
    if (s ~ /^[ \t]*#/) { cont = (s ~ /\\$/); next }
  }
  # strip comments, strings, char and raw literals
  o = ""; i = 1; n = length(s)
  while (i <= n) {
    if (raw != "") { k = index(substr(s, i), raw); if (k == 0) { i = n + 1; break } i += k - 1 + length(raw); raw = ""; o = o "\"\""; continue }
    if (inblk) { k = index(substr(s, i), "*/"); if (k == 0) { i = n + 1; break } i += k + 1; inblk = 0; continue }
    c = substr(s, i, 1); c2 = substr(s, i, 2)
    if (c2 == "//") break
    if (c2 == "/*") { inblk = 1; i += 2; continue }
    if (c == "R" && substr(s, i + 1, 1) == "\"" && (i == 1 || substr(s, i - 1, 1) !~ /[A-Za-z0-9_]/)) {
      rest = substr(s, i + 2); k = index(rest, "(")
      if (k > 0 && k <= 17) { raw = ")" substr(rest, 1, k - 1) "\""; i += 2 + k; continue }
    }
    if (c == "\"" || (c == "\x27" && !(substr(s, i - 1, 1) ~ /[0-9A-Fa-f]/ && substr(s, i + 1, 1) ~ /[0-9A-Fa-f]/))) {
      j = i + 1
      while (j <= n) { d = substr(s, j, 1); if (d == "\\") { j += 2; continue } if (d == c) break; j++ }
      o = o c c; i = j + 1; continue
    }
    o = o c; i++
  }
  if (o ~ /^[ \t]*$/) next
  # fast path inside a function: only braces matter until the depth falls back
  if (infn) {
    t = o; no = gsub(/\{/, "", t); t = o; nc = gsub(/\}/, "", t)
    if (sp + no - nc > fdepth) { for (q = 0; q < no; q++) st[++sp] = "B"; sp -= nc; next }
  }
  L = length(o)
  for (i = 1; i <= L; i++) {
    c = substr(o, i, 1)
    if (initd > 0) { hdr = hdr c; if (c == "{") initd++; else if (c == "}") initd--; continue }
    if (c == "{") {
      if (infn) { st[++sp] = "B"; continue }
      # brace member initialiser in a constructor init list: "Foo() : a{1}, b{2} {"
      th = hdr; sub(/[ \t]+$/, "", th)
      if (th ~ /\) ?(noexcept ?)?: ?[A-Za-z_]/ && th ~ /[A-Za-z0-9_>]$/ && th !~ /\) ?(noexcept ?)?: ?[A-Za-z_][A-Za-z0-9_:<>, ]*\) ?$/ && th ~ /[,:] ?[A-Za-z_][A-Za-z0-9_:<>]*$/) { hdr = hdr c; initd = 1; continue }
      k = classify(hdr)
      st[++sp] = k
      if (k == "C") { m = hdr; gsub(/[ \t]+/, " ", m); if (match(m, /(struct|class|union) [A-Za-z_][A-Za-z0-9_]*/)) { cn = substr(m, RSTART, RLENGTH); sub(/^[a-z]+ /, "", cn); clsst[sp] = cn; cls = cn } else clsst[sp] = cls }
      if (k == "F") { infn = 1; fdepth = sp - 1; fstart = (hline > 0 ? hline : FNR) }
      hdr = ""; hline = 0; continue
    }
    if (c == "}") {
      if (sp > 0) { k = st[sp]; sp-- } else k = "B"
      if (infn && sp == fdepth) { infn = 0; flush_fn(FNR) }
      if (k == "C") { cls = ""; for (q = sp; q > 0; q--) if (st[q] == "C") { cls = clsst[q]; break } }
      hdr = ""; hline = 0; continue
    }
    if (infn) continue
    if (c == ";") { hdr = ""; hline = 0; continue }
    if (hline == 0 && c !~ /[ \t]/) hline = FNR
    hdr = hdr c
  }
  if (!infn) hdr = hdr " "
}
END { if (NR > 0) { flush_fn(prevlines); printf "F %d %s\n", prevlines, prevfile } }
' > "$TMP/scan"

if [ -n "$FUNCQ" ]; then
  awk -v q="$FUNCQ" '$1 == "U" && index($5, q) { printf "func %d %s:%d %s\n", $2, $3, $4, $5 }' "$TMP/scan" | sort -k2,2nr
  exit 0
fi

# Allowlist: "file <glob>" (file size only) or "func <glob> <name>" (glob matched against the path; name exact, or * for any)
gawk -v FMAX="$FILEMAX" -v UMAX="$FUNCMAX" -v ALLOWF="$TMP/allow" -v ALL="$ALL" '
function glob2re(g,   r) { r = g; gsub(/[.+^$(){}|]/, "\\\\&", r); gsub(/\*\*/, "\001", r); gsub(/\*/, "[^/]*", r); gsub(/\001/, ".*", r); gsub(/\?/, ".", r); return "^" r "$" }
BEGIN { while ((getline a < ALLOWF) > 0) { n = split(a, f, /[ \t]+/); if (f[1] == "file") fre[++nf] = glob2re(f[2]); else if (f[1] == "func") { ure[++nu] = glob2re(f[2]); unm[nu] = f[3] } } }
function fallowed(p,   i) { for (i = 1; i <= nf; i++) if (p ~ fre[i]) return 1; return 0 }
function uallowed(p, nm,   i) { for (i = 1; i <= nu; i++) if (p ~ ure[i] && (unm[i] == "*" || unm[i] == nm)) return 1; return 0 }
$1 == "F" && $2 > FMAX { if (fallowed($3)) { na++; if (ALL) printf "ALLOW file %d %s\n", $2, $3 } else { no++; printf "OVER file %d %s\n", $2, $3 } }
$1 == "U" && $2 > UMAX { if (uallowed($3, $5)) { na++; if (ALL) printf "ALLOW func %d %s:%d %s\n", $2, $3, $4, $5 } else { no++; printf "OVER func %d %s:%d %s\n", $2, $3, $4, $5 } }
END { printf "#SUM %d %d\n", no + 0, na + 0 }' "$TMP/scan" | sort -t' ' -k2,2 -k3,3nr > "$TMP/rep"
grep -v '^#SUM' "$TMP/rep"
read -r _ NOVER NALLOW < <(grep '^#SUM' "$TMP/rep")
NFILES=$(wc -l < "$TMP/files" | tr -d ' ')

RC=0; NEWN=0
if [ -n "$CHECK" ]; then
  [ -f "$CHECK" ] || { echo "size_report: no baseline $CHECK" >&2; exit 2; }
  gawk -v BASEF="$CHECK" '
    BEGIN { while ((getline l < BASEF) > 0) { split(l, b, " "); if (b[1] != "OVER") continue
              if (b[2] == "file") bf[b[4]] = b[3]; else bu[b[5]] = (b[5] in bu && bu[b[5]] > b[3]) ? bu[b[5]] : b[3] } }
    $1 != "OVER" { next }
    $2 == "file" { if (!($4 in bf)) print "NEW  " $0; else if ($3 + 0 > bf[$4] + 0) print "GREW " $0 " (baseline " bf[$4] ")" }
    $2 == "func" { if (!($5 in bu)) print "NEW  " $0; else if ($3 + 0 > bu[$5] + 0) print "GREW " $0 " (baseline " bu[$5] ")" }' "$TMP/rep" > "$TMP/cmp"
  cat "$TMP/cmp"
  NEWN=$(grep -c '^NEW' "$TMP/cmp"); [ "$NEWN" -gt 0 ] && RC=1
fi
if [ -n "$CHECK" ]; then V=$([ $RC = 0 ] && echo PASS || echo FAIL); echo "size_report: $V files=$NFILES over=$NOVER allowed=$NALLOW new=$NEWN (file>$FILEMAX func>$FUNCMAX)"
else echo "size_report: files=$NFILES over=$NOVER allowed=$NALLOW (file>$FILEMAX func>$FUNCMAX)"; fi
exit $RC
