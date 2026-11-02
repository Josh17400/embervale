#!/usr/bin/env bash
# tools/gates/rebase_baseline.sh - move a baseline only where a batch declared it (docs/FIX_BATCH_P3.md 10).
#
#   tools/gates/rebase_baseline.sh --check-moves --run <wave out> [--root ID] [--bdir DIR] [--copy WSLDIR]
#   tools/gates/rebase_baseline.sh --from ID --to ID --run <wave out> [--bdir DIR] [--copy WSLDIR] [--perf FILE]
#                                  [--rev SHA]
#   tools/gates/rebase_baseline.sh --selftest
#
# The records live in tools/gates/baseline/<root>/ (gates/, probes/, checks/); tools/gates/baseline/CURRENT names the
# root every gate reads (lib.sh GATES_BASE). Each area of a batch declares what its change moves, one line per expected
# difference, in tools/gates/baseline/moves/<AREA>.txt:
#   <gate> <item glob>[,<item glob>...] <id> <reason...>          ('#' starts a comment)
# Gates and their items (an item glob is a bash glob over the item; write a space in it as ? or *):
#   S2.seeds S3.save S5.rig2 S5.rig1 S6.freeze S6.freeze1 S6.pool S7.rig2 S7.rig1 S1.exes S4.ci_cmds
#                  a text record: item = a differing line, normalized as the gate compares it (old or new side)
#                  (gates/S4.clang_ci.txt, wave 0's per-command last lines, was never compared by a gate and is
#                  not checked here either: clang runs after SOLID already differ from it in lines no batch touched)
#   W4.doll.<line> doll_gates.sh's line <line> (e.g. W4.doll.art_hash): item = a differing line, or "rc"
#   S10.view W2.view_all   PNG hashes: item = <script>/<png>       S10.audio   WAV hashes: item = <mode>/<wav>
#   S10.digest W6.digest_clang   item = a checkpoint field that changed (ser, len, act, rng, marks ...), S (the
#                  serialize blocks), A (the actor list) or checkpoint (one missing)
#   expect         item = a key of gates/expect.txt (CLANG_CMDS, SAVE_VER, ART_TOTAL_RIG2/1, VILLAGE_GROUPS)
#   unstable       item = a NAME of gates/unstable.txt (e.g. S3.save:mask): its lines are dropped (the file only shrinks)
#   S3.fixtures    item = a path under tests/fixtures changed since the root's revision
#   S9.shot        item = village_start.png (witness/refs/village_start.png is re-saved from --bdir)
#   W6.perf        item = * (needs --perf FILE, a perf_pack.sh --record output, to re-record)
#   any other gate item = FAIL: the gate FAILed with no record to name what moved (e.g. W3.witness FAIL)
#
# --check-moves compares the run (a wave_gates.sh or step_gates.sh --out dir) with the root (default: CURRENT) and the
#   union of the moves files. It FAILs on an UNDECLARED difference and on a declared move that did NOT happen; a move
#   whose gate is not in the run is UNCHECKED (reported, never recorded). --bdir DIR lets it run the full sim digest
#   (every checkpoint, not only each seed's first differing one) when S10.digest differs; --copy does it for the clang
#   digest in that WSL copy.
# --from A --to B runs the same check against root A and, only when it passes, writes root B: a copy of A with every
#   declared item re-recorded from that same run (a text record takes the run's normalized output, a hash list only the
#   declared keys), the declared unstable lines dropped, the sim digest re-recorded with --bdir (and --copy), a declared
#   S9.shot ref re-saved with --bdir, B/REV (the run's revision) and B/REASONS.md (each move: gate, item, id, area,
#   reason, what changed). Then CURRENT flips to B. Root A is never written; on any error nothing is flipped.
# --selftest runs the whole thing in a throwaway baseline dir: a declared move is accepted and recorded, an undeclared
#   one refused, a declared move that did not happen refused, the old root untouched, CURRENT flipped only on success.
# Output: one line per finding, then "rebase_baseline: <mode> PASS|FAIL ...". Exit 0 only on PASS.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
. "$REPO/tools/gates/lib.sh"
cd "$REPO"

MODE=""; RUN=""; FROM=""; TO=""; ROOTID=""; RBDIR=""; RCOPY=""; PERF=""; REV=""
while [ $# -gt 0 ]; do
  case "$1" in
    --check-moves) MODE=check; shift ;;
    --from) FROM="$2"; MODE=rebase; shift 2 ;;
    --to) TO="$2"; MODE=rebase; shift 2 ;;
    --run) RUN="$2"; shift 2 ;;
    --root) ROOTID="$2"; shift 2 ;;
    --bdir) RBDIR="$2"; shift 2 ;;
    --copy) RCOPY="$2"; shift 2 ;;
    --perf) PERF="$2"; shift 2 ;;
    --rev) REV="$2"; shift 2 ;;
    --selftest) MODE=selftest; shift ;;
    -h|--help) sed -n '2,45p' "$0"; exit 0 ;;
    *) echo "rebase_baseline: unknown arg $1" >&2; exit 2 ;;
  esac
done
BLD="$(gates_baseline_dir)"
MOVESDIR="${GATES_MOVES_DIR:-$BLD/moves}"
[ -n "$RBDIR" ] && { case "$RBDIR" in /*|?:*) ;; *) RBDIR="$REPO/$RBDIR" ;; esac; }

TEXT_GATES="S2.seeds S3.save S5.rig2 S5.rig1 S6.freeze S6.freeze1 S6.pool S7.rig2 S7.rig1"
declare -A EXPECT_SRC=([CLANG_CMDS]=S4.clang [ART_TOTAL_RIG2]=S5.rig2 [ART_TOTAL_RIG1]=S5.rig1 [SAVE_VER]=S3.savever [VILLAGE_GROUPS]=S7.rig2)

# ---------------------------------------------------------------- collecting the differences
# collect ROOTDIR: writes $W/diffs.tsv ("gate<TAB>item<TAB>detail"), $W/ran.tsv ("gate<TAB>verdict") and the run-side
# texts the rebase records ($W/new/<gate>.txt for text records)
step_dir() { if [ -d "$RUN/step/res" ]; then echo "$RUN/step"; else echo "$RUN"; fi; }
old_text() {   # old_text ROOTFILE NAME: the root record normalized as base_cmp compares it (CR, timings, unstable, masks)
  local ex; ex=$(UNSTABLE_FILE="$CUR_UNSTABLE" unstable_re "$2")
  norm "$1" | { if [ -n "$ex" ]; then grep -vE -- "$ex"; else cat; fi; } | UNSTABLE_FILE="$CUR_UNSTABLE" masked "$2"
}
ran_v() { awk -F'\t' -v g="$1" '$1 == g { print $2; exit }' "$W/ran.tsv"; }
did_run() { case "$(ran_v "$1")" in PASS|FAIL) return 0 ;; *) return 1 ;; esac; }
text_diff() {   # text_diff GATE OLDTEXT NEWTEXT: one diffs.tsv line per differing line (detail - old, + new)
  diff "$2" "$3" | awk -v g="$1" '{ gsub(/\t/, " ") } /^< / { print g "\t" substr($0, 3) "\t-" } /^> / { print g "\t" substr($0, 3) "\t+" }' >> "$W/diffs.tsv"
}
hash_diff() {   # hash_diff GATE OLD NEW KEYCOLS EXCLUDE: key -> hash lists (KEYCOLS 1: "key hash"; 2: "mode wav hash")
  awk -v g="$1" -v kc="$4" -v EXF="$5" '
    function key() { return kc == 2 ? $1 "/" $2 : $1 }
    function val() { return kc == 2 ? $3 : $2 }
    function excluded(k,   s) { if (k in ex) return 1; s = k; sub(/\/.*$/, "", s); return (s in ex) }
    BEGIN { if (EXF != "") while ((getline l < EXF) > 0) { sub(/\r$/, "", l); sub(/[ \t]*#.*$/, "", l); if (l != "") ex[l] = 1 } }
    { sub(/\r$/, "") } NF == 0 || /^#/ { next }
    FNR == NR { o[key()] = val(); next }
    { k = key(); n[k] = val() }
    END { for (k in o) if (!excluded(k)) { if (!(k in n)) print g "\t" k "\tremoved"; else if (o[k] != n[k]) print g "\t" k "\tchanged" }
          for (k in n) if (!(k in o) && !excluded(k)) print g "\t" k "\tadded" }' "$2" "$3" | LC_ALL=C sort >> "$W/diffs.tsv"
}
digest_fields_full() {   # digest_fields_full GATE OLDREC NEWREC: field-level differences over every checkpoint
  awk -v g="$1" '
    /^#/ { next } { sub(/\r$/, "") }
    FNR == NR { o[$1 " " $2 " " $3] = $0; next }
    { k = $1 " " $2 " " $3; seen[k] = 1
      if (!(k in o)) { d["checkpoint"]++; next }
      if (o[k] == $0) next
      if ($1 == "C") { split(o[k], a, " "); for (i = 4; i <= NF || i <= length(a); i++) if (a[i] != $i) { f = (i <= NF ? $i : a[i]); sub(/=.*/, "", f); d[f]++ } }
      else d[$1]++ }
    END { for (k in o) if (!(k in seen)) d["checkpoint"]++; for (f in d) print g "\t" f "\t" d[f] " checkpoints (full record)" }' "$2" "$3" | LC_ALL=C sort >> "$W/diffs.tsv"
}
digest_fields_log() {   # digest_fields_log GATE LOG: from --check's lines (each seed's first differing checkpoint only)
  tr -d '\r' < "$2" | grep -E '^sim-digest FAIL seed ' | awk -v g="$1" '
    /checkpoint missing/ { d["checkpoint"]++; next }
    { w = $0; sub(/^[^:]*\(first differing checkpoint\): /, "", w); c = w; sub(/;.*$/, "", c)
      n = split(c, p, ", "); for (i = 1; i <= n; i++) { f = p[i]; if (f !~ /=/) continue; sub(/=.*/, "", f); d[f]++ }
      if (w ~ /serialize block/) d["S"]++; if (w ~ /; actor/) d["A"]++ }
    END { for (f in d) print g "\t" f "\t" d[f] " seeds (first differing checkpoint per seed)" }' | LC_ALL=C sort >> "$W/diffs.tsv"
}
digest_args() { head -1 "$1" | tr -d '\r' | awk '{ for (i = 1; i < NF; i++) if ($i ~ /^(secs|every|fight|fevery|seeds)$/) printf "--%s %s ", $i, $(i + 1) }'; }
full_digest_msvc() {   # the run's build, the root record's arguments -> $W/new_digest_msvc.txt
  [ -n "$RBDIR" ] && [ -x "$RBDIR/rpg_test.exe" ] || return 1
  [ -s "$W/new_digest_msvc.txt" ] && return 0
  # (P3 integration) < /dev/null goes to rpg_test: at the end of the pipe it fed grep, and the record came out empty
  # shellcheck disable=SC2046
  "$REPO/tools/slot.sh" "$RBDIR/rpg_test.exe" --sim-digest $(digest_args "$1") 2>/dev/null < /dev/null | tr -d '\r' | grep -vE "$SLOT_NOISE" > "$W/new_digest_msvc.txt"
  grep -q '^# sim-digest: ' "$W/new_digest_msvc.txt"
}
full_digest_clang() {
  [ -n "$RCOPY" ] || return 1
  [ -s "$W/new_digest_clang.txt" ] && return 0
  MSYS_NO_PATHCONV=1 wsl -d "${WSL_DISTRO:-Ubuntu}" -- bash -lc "cd ~/$RCOPY && ./build-native/rpg_test --sim-digest $(digest_args "$1")" < /dev/null 2>/dev/null | tr -d '\r' > "$W/new_digest_clang.txt"
  grep -q '^# sim-digest: ' "$W/new_digest_clang.txt"
}
expect_now() {   # expect_now KEY: the value the run measured ("" when its gate did not run)
  local S; S=$(step_dir)
  case "$1" in
    CLANG_CMDS) grep -oE 'CLANG_CMDS=[0-9]+' "$S/logs/S4.clang.log" 2>/dev/null | tail -1 | cut -d= -f2 ;;
    ART_TOTAL_RIG2) tr -d '\r' < "$S/logs/S5.rig2.log" 2>/dev/null | grep -m1 '^TOTAL' | awk '{ print $2 }' ;;
    ART_TOTAL_RIG1) tr -d '\r' < "$S/logs/S5.rig1.log" 2>/dev/null | grep -m1 '^TOTAL' | awk '{ print $2 }' ;;
    SAVE_VER) grep -oE 'SAVE_VER = [0-9]+' "$S/logs/S3.savever.log" 2>/dev/null | head -1 | grep -oE '[0-9]+$' ;;
    VILLAGE_GROUPS) tr -d '\r' < "$S/logs/S7.rig2.log" 2>/dev/null | grep -m1 -oE '[0-9]+ groups?' | grep -oE '^[0-9]+' ;;
  esac
}

collect() {
  local R="$1" S g f n v k old now
  S=$(step_dir); CUR_UNSTABLE="$R/gates/unstable.txt"
  : > "$W/diffs.tsv"; : > "$W/ran.tsv"; rm -rf "$W/new"; mkdir -p "$W/new"
  for f in "$S"/res/*.txt "$RUN"/res/*.txt "$RUN"/doll/res/*.txt; do
    [ -f "$f" ] || continue
    n=$(basename "$f" .txt); v=$(awk '{ print $2; exit }' "$f")
    case "$f" in "$RUN"/doll/res/*) n="W4.doll.$n" ;; esac
    awk -F'\t' -v g="$n" '$1 == g { f = 1 } END { exit !f }' "$W/ran.tsv" || printf '%s\t%s\n' "$n" "$v" >> "$W/ran.tsv"
  done
  # text records
  for g in $TEXT_GATES; do
    did_run "$g" && [ -f "$S/logs/$g.norm" ] && [ -f "$R/gates/$g.txt" ] || continue
    old_text "$R/gates/$g.txt" "$g" > "$W/old.txt"; cp "$S/logs/$g.norm" "$W/new/$g.txt"
    text_diff "$g" "$W/old.txt" "$W/new/$g.txt"
  done
  if did_run S1.build && [ -f "$S/logs/S1.exes" ]; then tr -d '\r' < "$R/gates/S1.exes.txt" > "$W/old.txt"; cp "$S/logs/S1.exes" "$W/new/S1.exes.txt"; text_diff S1.exes "$W/old.txt" "$W/new/S1.exes.txt"; printf 'S1.exes\t%s\n' "$(ran_v S1.build)" >> "$W/ran.tsv"; fi
  if did_run S4.clang && [ -f "$S/clang/ci_cmds.txt" ]; then
    tr -d '\r' < "$R/gates/S4.ci_cmds.txt" > "$W/old.txt"; tr -d '\r' < "$S/clang/ci_cmds.txt" > "$W/new/S4.ci_cmds.txt"; text_diff S4.ci_cmds "$W/old.txt" "$W/new/S4.ci_cmds.txt"
    printf 'S4.ci_cmds\t%s\n' "$(ran_v S4.clang)" >> "$W/ran.tsv"
  fi
  # doll lines
  if [ -d "$RUN/doll/res" ]; then
    for f in "$RUN"/doll/res/*.txt; do
      n=$(basename "$f" .txt); g="W4.doll.$n"
      [ -f "$RUN/doll/logs/$n.norm" ] && [ -f "$R/gates/doll/$n.txt" ] || continue
      old_text "$R/gates/doll/$n.txt" "$n" > "$W/old.txt"; mkdir -p "$W/new/doll"; cp "$RUN/doll/logs/$n.norm" "$W/new/doll/$n.txt"
      text_diff "$g" "$W/old.txt" "$W/new/doll/$n.txt"
      old=$(tr -d '\r\n ' < "$R/gates/doll/$n.rc" 2>/dev/null); now=$(grep -oE ' rc=[0-9]+' "$f" | head -1 | cut -d= -f2)
      [ "${now:-x}" != "${old:-0}" ] && printf '%s\trc\t%s -> %s\n' "$g" "${old:-0}" "${now:-?}" >> "$W/diffs.tsv"
    done
  fi
  # hash lists
  did_run S10.view && [ -f "$S/view_pack/hashes.txt" ] && hash_diff S10.view "$R/probes/view_quick/hashes.txt" "$S/view_pack/hashes.txt" 1 "$(ls "$R/probes/view_quick/exclude.txt" 2>/dev/null)"
  did_run W2.view_all && [ -f "$RUN/view_all/hashes.txt" ] && hash_diff W2.view_all "$R/probes/view_all/hashes.txt" "$RUN/view_all/hashes.txt" 1 "$(ls "$R/probes/view_all/exclude.txt" 2>/dev/null)"
  did_run S10.audio && [ -f "$S/audio_pack/hashes.txt" ] && hash_diff S10.audio "$R/probes/audio_hashes.txt" "$S/audio_pack/hashes.txt" 2 ""
  # the sim digest
  if [ "$(ran_v S10.digest)" = FAIL ]; then
    if full_digest_msvc "$R/probes/sim_digest_msvc.txt"; then digest_fields_full S10.digest "$R/probes/sim_digest_msvc.txt" "$W/new_digest_msvc.txt"
    else digest_fields_log S10.digest "$S/logs/S10.digest.log"; fi
  fi
  if [ "$(ran_v W6.digest_clang)" = FAIL ]; then
    if full_digest_clang "$R/probes/sim_digest_clang.txt"; then digest_fields_full W6.digest_clang "$R/probes/sim_digest_clang.txt" "$W/new_digest_clang.txt"
    else digest_fields_log W6.digest_clang "$RUN/logs/W6.digest_clang.log"; fi
  fi
  # expected values
  for k in "${!EXPECT_SRC[@]}"; do
    did_run "${EXPECT_SRC[$k]}" || continue
    old=$(tr -d '\r' < "$R/gates/expect.txt" | grep -m1 "^$k=" | cut -d= -f2-); now=$(expect_now "$k")
    [ -n "$now" ] && [ "$now" != "$old" ] && printf 'expect\t%s\t%s -> %s\n' "$k" "${old:-<none>}" "$now" >> "$W/diffs.tsv"
  done
  # fixtures changed since the root's revision
  if did_run S3.fixtures; then
    local rrev; rrev=$(gates_root_rev "$(basename "$R")")
    if git rev-parse -q --verify "$rrev^{commit}" > /dev/null 2>&1; then
      { git diff --name-only "$rrev" -- tests/fixtures; git ls-files --others --exclude-standard -- tests/fixtures; } | sort -u | sed 's#^tests/fixtures/##' |
        awk '{ print "S3.fixtures\t" $0 "\tchanged since the root revision" }' >> "$W/diffs.tsv"
    fi
  fi
  [ "$(ran_v S9.shot)" = FAIL ] && printf 'S9.shot\tvillage_start.png\t%s\n' "$(cut -c1-100 "$S/res/S9.shot.txt" | tr '\t' ' ')" >> "$W/diffs.tsv"
  # a FAILed gate that no record above explains
  while IFS=$'\t' read -r g v; do
    [ "$v" = FAIL ] || continue
    case "$g" in W1) continue ;; esac
    awk -F'\t' -v g="$g" '$1 == g || index($1, g ".") == 1 { f = 1 } END { exit !f }' "$W/diffs.tsv" && continue
    k=""; for n in "${!EXPECT_SRC[@]}"; do [ "${EXPECT_SRC[$n]}" = "$g" ] && awk -F'\t' -v k="$n" '$1 == "expect" && $2 == k { f = 1 } END { exit !f }' "$W/diffs.tsv" && k=1; done
    [ "$g" = S7.rig1 ] && awk -F'\t' '$1 == "expect" && $2 == "VILLAGE_GROUPS" { f = 1 } END { exit !f }' "$W/diffs.tsv" && k=1
    if [ -n "$k" ]; then [ "$g" = S4.clang ] && ! grep -q 'CLANG_FAILS=0' "$S/logs/S4.clang.log" 2>/dev/null && k=""; fi
    [ -n "$k" ] && continue
    printf '%s\tFAIL\t%s\n' "$g" "$( { cat "$S/res/$g.txt" "$RUN/res/$g.txt" 2>/dev/null; } | head -1 | sed 's/^[^|]*| *//' | cut -c1-110 | tr '\t' ' ')" >> "$W/diffs.tsv"
  done < "$W/ran.tsv"
}

# ---------------------------------------------------------------- matching against the moves files
load_moves() {   # -> $W/moves.tsv: n<TAB>area<TAB>gate<TAB>items<TAB>id<TAB>reason
  local f a line n=0 g it id rs
  : > "$W/moves.tsv"
  for f in "$MOVESDIR"/*.txt; do
    [ -f "$f" ] || continue; a=$(basename "$f" .txt)
    while IFS= read -r line || [ -n "$line" ]; do
      line="${line%$'\r'}"; line="${line%%#*}"; [ -z "${line// /}" ] && continue
      read -r g it id rs <<< "$line"
      if [ -z "${id:-}" ]; then echo "BADLINE $a: '$line' (want: <gate> <item glob> <id> <reason>)"; BAD=$((BAD+1)); continue; fi
      n=$((n+1)); printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$n" "$a" "$g" "$it" "$id" "${rs:-}" >> "$W/moves.tsv"
    done < "$f"
  done
}
move_gate_ran() {   # was the gate of a move in the run?
  local g="$1" k
  case "$g" in
    unstable) return 0 ;;
    expect) for k in "${!EXPECT_SRC[@]}"; do [[ "$k" == $2 ]] && did_run "${EXPECT_SRC[$k]}" && return 0; done; return 1 ;;
  esac
  awk -F'\t' -v g="$g" '$2 == "PASS" || $2 == "FAIL" { print $1 }' "$W/ran.tsv" | while read -r r; do [[ "$r" == $g ]] && { echo y; break; }; done | grep -q y
}
match_moves() {   # sets UND, MISS, UNCH, DECL; writes $W/matched.tsv (n<TAB>gate<TAB>item<TAB>detail)
  local n a g it id rs dg di dd hit alt
  UND=0; MISS=0; UNCH=0; DECL=0; : > "$W/matched.tsv"; : > "$W/undeclared.tsv"
  # unstable moves: declared lines exist in the root's unstable.txt
  while IFS=$'\t' read -r dg di dd; do
    # (P3 integration) a difference two areas both explain (a fight shot with B1's strike and B11's toast) credits every
    # move that names it, so neither area's move reads as "did not happen"; it still counts once
    hit=""
    while IFS=$'\t' read -r n a g it id rs; do
      [[ "$dg" == $g ]] || continue
      local IFS_SAVE="$IFS" m=""; IFS=','; for alt in $it; do [[ "$di" == $alt ]] && { m=1; break; }; done; IFS="$IFS_SAVE"
      [ -n "$m" ] && { printf '%s\t%s\t%s\t%s\n' "$n" "$dg" "$di" "$dd" >> "$W/matched.tsv"; hit=$n; }
    done < "$W/moves.tsv"
    if [ -n "$hit" ]; then DECL=$((DECL+1))
    else printf '%s\t%s\t%s\n' "$dg" "$di" "$dd" >> "$W/undeclared.tsv"; UND=$((UND+1)); fi
  done < "$W/diffs.tsv"
  : > "$W/missing.tsv"; : > "$W/unchecked.tsv"
  while IFS=$'\t' read -r n a g it id rs; do
    if [ "$g" = unstable ]; then
      local IFS_SAVE="$IFS" ok=""; IFS=','
      for alt in $it; do tr -d '\r' < "$CUR_UNSTABLE" | grep -v '^#' | cut -f1 | while read -r nm; do [[ "$nm" == $alt ]] && { echo y; break; }; done | grep -q y && ok=1; done
      IFS="$IFS_SAVE"
      if [ -n "$ok" ]; then printf '%s\tunstable\t%s\tdrop from unstable.txt\n' "$n" "$it" >> "$W/matched.tsv"
      else printf '%s\n' "$n" >> "$W/missing.tsv"; MISS=$((MISS+1)); fi
      continue
    fi
    if ! move_gate_ran "$g" "$it"; then printf '%s\n' "$n" >> "$W/unchecked.tsv"; UNCH=$((UNCH+1)); continue; fi
    awk -F'\t' -v n="$n" '$1 == n { f = 1 } END { exit !f }' "$W/matched.tsv" || { printf '%s\n' "$n" >> "$W/missing.tsv"; MISS=$((MISS+1)); }
  done < "$W/moves.tsv"
}
report() {   # print the findings; returns 0 on PASS
  local n a g it id rs c
  while IFS=$'\t' read -r n a g it id rs; do
    if grep -qx "$n" "$W/missing.tsv"; then echo "MISSING    $a: $g $it $id - declared, but the run shows no such difference ($rs)"
    elif grep -qx "$n" "$W/unchecked.tsv"; then echo "UNCHECKED  $a: $g $it $id - its gate is not in this run (not recorded)"
    else c=$(awk -F'\t' -v n="$n" '$1 == n' "$W/matched.tsv" | wc -l)
      echo "MOVED      $a: $g $it $id - $c item(s): $(awk -F'\t' -v n="$n" '$1 == n { printf "%s%s (%s)", (k++ ? "; " : ""), substr($3, 1, 60), $4 }' "$W/matched.tsv" | cut -c1-150)"; fi
  done < "$W/moves.tsv"
  head -n 60 "$W/undeclared.tsv" | awk -F'\t' '{ printf "UNDECLARED %s %s (%s)\n", $1, substr($2, 1, 110), $3 }'
  [ "$UND" -gt 60 ] && echo "UNDECLARED ... and $((UND - 60)) more ($W/undeclared.tsv)"
  [ "$UND" -eq 0 ] && [ "$MISS" -eq 0 ] && [ "${BAD:-0}" -eq 0 ]
}

check_against() {   # check_against ROOTID: collect + match + report; 0 on PASS
  local R="$BLD/$1"
  [ -d "$R/gates" ] || { echo "rebase_baseline: no root $R"; return 2; }
  [ -d "$RUN" ] || { echo "rebase_baseline: no run dir $RUN"; return 2; }
  BAD=0; collect "$R"; load_moves; match_moves
  echo "rebase_baseline: root $1 (rev $(gates_root_rev "$1")), run $RUN ($(head -1 "$RUN/build.sha" 2>/dev/null || echo 'no build.sha')), $(grep -c . "$W/ran.tsv") gate lines, moves: $(cd "$MOVESDIR" 2>/dev/null && ls ./*.txt 2>/dev/null | sed 's#^\./##' | tr '\n' ' ')"
  report; local rc=$?
  echo "rebase_baseline: check-moves $([ $rc -eq 0 ] && echo PASS || echo FAIL): $DECL declared differences, $UND undeclared, $MISS declared moves that did not happen, $UNCH unchecked, ${BAD:-0} bad lines"
  return $rc
}

# ---------------------------------------------------------------- re-recording
declared_items() { awk -F'\t' -v g="$1" '$2 == g { print $3 }' "$W/matched.tsv"; }
has_gate() { awk -F'\t' -v g="$1" '$2 == g { f = 1 } END { exit !f }' "$W/matched.tsv"; }
merge_hash() {   # merge_hash OLDFILE NEWFILE KEYCOLS GATE > merged: the old list with only the declared keys taken from the run
  declared_items "$4" > "$W/keys.txt"
  awk -v kc="$3" -v KF="$W/keys.txt" '
    function key() { return kc == 2 ? $1 "/" $2 : $1 }
    BEGIN { while ((getline k < KF) > 0) want[k] = 1 }
    { sub(/\r$/, "") }
    FNR == NR { if (NF && !/^#/) { k = key(); if (k in want) { nl[k] = $0 } } next }
    { if (!NF || /^#/) { print; next } k = key(); if (k in want) { if (k in nl) { print nl[k]; done[k] = 1 } } else print }
    END { for (k in nl) if (!(k in done)) print nl[k] }' "$2" "$1" | LC_ALL=C sort
}
rerecord() {   # rerecord TMPROOT: apply the declared items of $W/matched.tsv; 0 on success
  local T="$1" S g k n v rc=0 f
  S=$(step_dir)
  # 1. unstable lines first (the text records below are rebuilt under the new unstable.txt)
  if has_gate unstable; then
    declared_items unstable | tr ',' '\n' > "$W/unst.txt"
    awk -F'\t' -v UF="$W/unst.txt" 'BEGIN { while ((getline p < UF) > 0) pats[++n] = p }
      /^#/ { print; next } { for (i = 1; i <= n; i++) { re = pats[i]; gsub(/[.+^$(){}|\\]/, "\\\\&", re); gsub(/\*/, ".*", re); gsub(/\?/, ".", re); if ($1 ~ "^" re "$") next } print }' \
      "$T/gates/unstable.txt" > "$W/unstable.new"
    cp "$W/unstable.new" "$T/gates/unstable.txt"
  fi
  # 2. text records: the run's normalized output (rebuilt from the raw log when unstable.txt changed)
  for g in $TEXT_GATES S1.exes S4.ci_cmds; do
    has_gate "$g" || continue
    if has_gate unstable && [ -f "$S/logs/$g.log" ] && [[ " $TEXT_GATES " == *" $g "* ]]; then
      local ex filt=""; [ "$g" = S6.pool ] && filt='pxc --check'
      ex=$(UNSTABLE_FILE="$T/gates/unstable.txt" unstable_re "$g")
      norm "$S/logs/$g.log" | { if [ -n "$filt" ]; then grep -E -- "$filt"; else cat; fi; } | { if [ -n "$ex" ]; then grep -vE -- "$ex"; else cat; fi; } |
        UNSTABLE_FILE="$T/gates/unstable.txt" masked "$g" > "$T/gates/$g.txt"
    else cp "$W/new/$g.txt" "$T/gates/$g.txt"; fi
    echo "recorded   gates/$g.txt"
  done
  for f in "$W"/new/doll/*.txt; do
    [ -f "$f" ] || continue; n=$(basename "$f" .txt)
    has_gate "W4.doll.$n" || continue
    cp "$f" "$T/gates/doll/$n.txt"
    if declared_items "W4.doll.$n" | grep -qx rc; then grep -oE ' rc=[0-9]+' "$RUN/doll/res/$n.txt" | head -1 | cut -d= -f2 > "$T/gates/doll/$n.rc"; fi
    echo "recorded   gates/doll/$n.txt"
  done
  # 3. hash lists: only the declared keys
  if has_gate S10.view; then merge_hash "$T/probes/view_quick/hashes.txt" "$S/view_pack/hashes.txt" 1 S10.view > "$W/h"; cp "$W/h" "$T/probes/view_quick/hashes.txt"; echo "recorded   probes/view_quick/hashes.txt ($(declared_items S10.view | wc -l) keys)"; fi
  if has_gate W2.view_all; then merge_hash "$T/probes/view_all/hashes.txt" "$RUN/view_all/hashes.txt" 1 W2.view_all > "$W/h"; cp "$W/h" "$T/probes/view_all/hashes.txt"; echo "recorded   probes/view_all/hashes.txt ($(declared_items W2.view_all | wc -l) keys)"; fi
  if has_gate S10.audio; then merge_hash "$T/probes/audio_hashes.txt" "$S/audio_pack/hashes.txt" 2 S10.audio > "$W/h"; cp "$W/h" "$T/probes/audio_hashes.txt"; echo "recorded   probes/audio_hashes.txt ($(declared_items S10.audio | wc -l) keys)"; fi
  # 4. the sim digest: a full record from the run's build
  if has_gate S10.digest; then
    if full_digest_msvc "$T/probes/sim_digest_msvc.txt"; then cp "$W/new_digest_msvc.txt" "$T/probes/sim_digest_msvc.txt"; echo "recorded   probes/sim_digest_msvc.txt (rpg_test --sim-digest $(digest_args "$T/probes/sim_digest_msvc.txt"))"
    else echo "ERROR      S10.digest is declared: give --bdir <the run's build dir> to re-record the full digest"; rc=1; fi
  fi
  if has_gate W6.digest_clang; then
    if full_digest_clang "$T/probes/sim_digest_clang.txt"; then cp "$W/new_digest_clang.txt" "$T/probes/sim_digest_clang.txt"; echo "recorded   probes/sim_digest_clang.txt"
    else echo "ERROR      W6.digest_clang is declared: give --copy <the S4 WSL copy> to re-record the clang digest"; rc=1; fi
  fi
  # 5. expected values
  for k in $(declared_items expect); do
    v=$(expect_now "$k")
    if grep -q "^$k=" "$T/gates/expect.txt"; then sed -i "s/^$k=.*/$k=$v/" "$T/gates/expect.txt"; else echo "$k=$v" >> "$T/gates/expect.txt"; fi
    echo "recorded   gates/expect.txt $k=$v"
  done
  # 6. the Witness reference and perf
  if has_gate S9.shot; then
    if [ -n "$RBDIR" ] && "$REPO/tools/gates/witness_smoke.sh" shot --bdir "$RBDIR" --out "$W/shot" > "$W/shot.log" 2>&1; [ -f "$W/shot/shots/village_start_now.png" ]; then
      cp "$W/shot/shots/village_start_now.png" "$REPO/witness/refs/village_start.png"; echo "re-saved   witness/refs/village_start.png (from $RBDIR; commit it with the root)"
    else echo "ERROR      S9.shot is declared: give --bdir <the run's build dir> to re-save witness/refs/village_start.png"; rc=1; fi
  fi
  if has_gate W6.perf; then
    if [ -n "$PERF" ] && grep -q 'recorded ALONE' "$PERF"; then cp "$PERF" "$T/probes/perf.txt"; echo "recorded   probes/perf.txt (from $PERF)"
    else echo "ERROR      W6.perf is declared: give --perf <a perf_pack.sh --record output>"; rc=1; fi
  fi
  return $rc
}
write_reasons() {   # write_reasons TMPROOT
  local T="$1" n a g it id rs
  {
    echo "# Baseline root $TO"
    echo
    echo "Re-recorded from root \`$FROM\` by tools/gates/rebase_baseline.sh on $(date '+%Y-%m-%d %H:%M'): run \`$RUN\`"
    echo "(build $(head -1 "$RUN/build.sha" 2>/dev/null)), revision \`$(cat "$T/REV")\`. Only the declared items below moved;"
    echo "every other record is byte for byte the one in \`$FROM\`. Moves files: $(cd "$MOVESDIR" && ls ./*.txt | sed 's#^\./##' | tr '\n' ' ')"
    echo
    echo "| gate | item | id | area | reason | what changed |"
    echo "|---|---|---|---|---|---|"
    while IFS=$'\t' read -r n a g it id rs; do
      grep -qx "$n" "$W/unchecked.tsv" && continue
      echo "| $g | \`$it\` | $id | $a | $rs | $(awk -F'\t' -v n="$n" '$1 == n { c++; if (c <= 3) s = s (c > 1 ? "; " : "") substr($3, 1, 50) " (" $4 ")" } END { if (c > 3) s = s "; ... " c " items"; print s }' "$W/matched.tsv" | sed 's/|/\\|/g') |"
    done < "$W/moves.tsv"
    if [ -s "$W/unchecked.tsv" ]; then
      echo; echo "Declared but not in the run (not recorded):"
      while IFS=$'\t' read -r n a g it id rs; do grep -qx "$n" "$W/unchecked.tsv" && echo "- $a: $g \`$it\` $id ($rs)"; done < "$W/moves.tsv"
    fi
  } > "$T/REASONS.md"
}
do_rebase() {
  [ -n "$FROM" ] && [ -n "$TO" ] && [ -n "$RUN" ] || { echo "rebase_baseline: --from, --to and --run are required"; return 2; }
  case "$TO" in *[!A-Za-z0-9_.-]*|"") echo "rebase_baseline: --to '$TO' must be one word ([A-Za-z0-9_.-])"; return 2 ;; esac
  [ -e "$BLD/$TO" ] && { echo "rebase_baseline: root $BLD/$TO already exists (a root is never rewritten)"; return 2; }
  [ -d "$BLD/$FROM" ] || { echo "rebase_baseline: no root $BLD/$FROM"; return 2; }
  local cur; cur=$(gates_root_id)
  [ "$cur" = "$FROM" ] || echo "rebase_baseline: note: CURRENT is '$cur', rebasing from '$FROM'"
  check_against "$FROM" || { echo "rebase_baseline: REFUSED: the run's differences against $FROM are not exactly the declared moves; nothing written, CURRENT stays $cur"; return 1; }
  local r="${REV:-$(awk '{ print $1; exit }' "$RUN/build.sha" 2>/dev/null)}"
  if [ -z "$REV" ] && grep -q '+dirty' "$RUN/build.sha" 2>/dev/null; then echo "rebase_baseline: REFUSED: the run's build was dirty ($(cat "$RUN/build.sha")); commit and rerun, or give --rev"; return 1; fi
  [ -n "$r" ] || { echo "rebase_baseline: REFUSED: no revision (no build.sha in the run; give --rev)"; return 1; }
  local T="$BLD/.$TO.tmp.$$"; rm -rf "$T"
  cp -r "$BLD/$FROM" "$T" || return 1
  rm -f "$T/REASONS.md"; echo "$r" > "$T/REV"
  if ! rerecord "$T"; then rm -rf "$T"; echo "rebase_baseline: REFUSED: a declared item could not be re-recorded; nothing written, CURRENT stays $cur"; return 1; fi
  write_reasons "$T"
  mv "$T" "$BLD/$TO" || { rm -rf "$T"; return 1; }
  echo "$TO" > "$BLD/CURRENT.tmp.$$" && mv -f "$BLD/CURRENT.tmp.$$" "${GATES_CURRENT_FILE:-$BLD/CURRENT}"
  echo "rebase_baseline: rebase PASS: root $TO written ($(wc -l < "$W/matched.tsv") declared items from $(cut -f1 "$W/matched.tsv" | sort -u | wc -l) moves; REASONS.md), CURRENT $FROM -> $TO"
}

# ---------------------------------------------------------------- self-test
selftest() {
  local ST fails=0 R0 sum0 out rc
  ST=$(mktemp -d "${TMPDIR:-/tmp}/emb_rebase_selftest.XXXXXX")
  pf() { if [ "$1" = 0 ]; then echo "PASS $2"; else echo "FAIL $2"; fails=$((fails+1)); fi; }
  local SRC="$GATES_BASE"
  mkdir -p "$ST/baseline/moves"; cp -r "$SRC" "$ST/baseline/r0"; echo r0 > "$ST/baseline/CURRENT"
  R0="$ST/baseline/r0"; sum0=$(cd "$R0" && find . -type f | LC_ALL=C sort | xargs sha256sum | sha256sum | cut -c1-16)
  export GATES_BASELINE_DIR="$ST/baseline" GATES_CURRENT_FILE="$ST/baseline/CURRENT" GATES_MOVES_DIR="$ST/baseline/moves"
  BLD="$ST/baseline"; MOVESDIR="$ST/baseline/moves"
  # a fake step run: S2.seeds with seed 3's line changed (FAIL), S10.view with one PNG changed (FAIL), S5.rig2 identical
  mk_run() {   # mk_run DIR EXTRA_PNG(0/1)
    local D="$1"; mkdir -p "$D/res" "$D/logs" "$D/view_pack"; echo "abc1234def" > "$D/build.sha"
    CUR_UNSTABLE="$R0/gates/unstable.txt"
    old_text "$R0/gates/S2.seeds.txt" S2.seeds | sed -E '/^seed 3 /s/sites +[0-9]+/sites 999/' > "$D/logs/S2.seeds.norm"
    old_text "$R0/gates/S5.rig2.txt" S5.rig2 > "$D/logs/S5.rig2.norm"; printf 'TOTAL %s\n' "$(grep -m1 '^ART_TOTAL_RIG2=' "$R0/gates/expect.txt" | cut -d= -f2)" > "$D/logs/S5.rig2.log"
    awk 'NR == 2 { $2 = "00" substr($2, 3) } { print }' "$R0/probes/view_quick/hashes.txt" > "$D/view_pack/hashes.txt"
    [ "$2" = 1 ] && awk 'NR == 3 { $2 = "ff" substr($2, 3) } { print }' "$D/view_pack/hashes.txt" > "$D/h" && mv "$D/h" "$D/view_pack/hashes.txt"
    echo "S2.seeds FAIL rc=0 1s | differs from baseline" > "$D/res/S2.seeds.txt"
    echo "S10.view FAIL rc=1 1s | differs" > "$D/res/S10.view.txt"
    echo "S5.rig2 PASS rc=0 1s | same" > "$D/res/S5.rig2.txt"
    echo "S10.audio PASS rc=0 1s | same" > "$D/res/S10.audio.txt"; mkdir -p "$D/audio_pack"; tr -d '\r' < "$R0/probes/audio_hashes.txt" > "$D/audio_pack/hashes.txt"
  }
  local K2; K2=$(awk 'NR == 2 { print $1 }' "$R0/probes/view_quick/hashes.txt"); local K3; K3=$(awk 'NR == 3 { print $1 }' "$R0/probes/view_quick/hashes.txt")
  mk_run "$ST/run1" 0; mk_run "$ST/run2" 1
  printf '%s\n' "# self-test moves" "S2.seeds seed?3?* SELF seed 3 now finds 999 sites" "S10.view $K2 SELF $K2 redrawn" "W2.view_all foo/*.png SELF not in a step run" > "$ST/baseline/moves/SELF.txt"
  # 1. a declared move: check passes, the rebase records it, CURRENT flips, r0 untouched
  RUN="$ST/run1"; out=$(check_against r0 2>&1); rc=$?; echo "$out" | sed 's/^/    /' | head -8
  pf $rc "1 check-moves: the declared S2.seeds line and S10.view PNG are accepted (W2 move UNCHECKED)"
  echo "$out" | grep -q '^UNCHECKED  SELF: W2.view_all'; pf $? "1 the W2.view_all move is reported UNCHECKED (gate not in the run)"
  FROM=r0; TO=r1; out=$(do_rebase 2>&1); rc=$?; echo "$out" | grep -E '^(recorded|rebase_baseline: rebase)' | sed 's/^/    /'
  pf $rc "1 rebase r0 -> r1 passes"
  [ "$(cat "$ST/baseline/CURRENT")" = r1 ]; pf $? "1 CURRENT flipped to r1"
  grep -qE '^seed 3 .*sites 999' "$ST/baseline/r1/gates/S2.seeds.txt"; pf $? "1 r1's S2.seeds record holds the moved line"
  [ "$(grep -c . "$ST/baseline/r1/gates/S2.seeds.txt")" = "$(grep -c . "$ST/run1/logs/S2.seeds.norm")" ]; pf $? "1 r1's S2.seeds record = the run's lines"
  [ "$(awk -v k="$K2" '$1 == k { print $2 }' "$ST/baseline/r1/probes/view_quick/hashes.txt" | cut -c1-2)" = 00 ]; pf $? "1 r1's view_quick hash of $K2 is the run's"
  cmp -s <(awk -v k="$K2" '$1 != k' "$R0/probes/view_quick/hashes.txt" | tr -d '\r') <(awk -v k="$K2" '$1 != k' "$ST/baseline/r1/probes/view_quick/hashes.txt"); pf $? "1 every other view_quick hash is r0's"
  grep -q "| S2.seeds | \`seed?3?\*\` | SELF | SELF | seed 3 now finds 999 sites |" "$ST/baseline/r1/REASONS.md"; pf $? "1 REASONS.md names the move, its id, area and reason"
  [ "$(cat "$ST/baseline/r1/REV")" = abc1234def ]; pf $? "1 r1/REV is the run's revision"
  # 2. an undeclared difference (a second PNG): refused, nothing written, CURRENT stays r1
  RUN="$ST/run2"; out=$(check_against r0 2>&1); rc=$?
  [ $rc -ne 0 ] && echo "$out" | grep -q "^UNDECLARED S10.view $K3 (changed)"; pf $? "2 check-moves FAILs on the undeclared $K3"
  FROM=r0; TO=r2; out=$(do_rebase 2>&1); rc=$?; echo "$out" | grep -E 'REFUSED' | sed 's/^/    /'
  [ $rc -ne 0 ] && [ ! -e "$ST/baseline/r2" ] && [ "$(cat "$ST/baseline/CURRENT")" = r1 ] && ! ls -d "$ST"/baseline/.r2.tmp.* > /dev/null 2>&1; pf $? "2 rebase r0 -> r2 refused: no r2, CURRENT still r1"
  # 3. a declared move that did not happen: refused
  echo "S10.audio full/sfx_Nothing.wav SELF a sound that did not change" >> "$ST/baseline/moves/SELF.txt"
  RUN="$ST/run1"; out=$(check_against r0 2>&1); rc=$?
  [ $rc -ne 0 ] && echo "$out" | grep -q '^MISSING    SELF: S10.audio full/sfx_Nothing.wav'; pf $? "3 check-moves FAILs on the declared S10.audio move that did not happen"
  FROM=r0; TO=r3; out=$(do_rebase 2>&1); rc=$?
  [ $rc -ne 0 ] && [ ! -e "$ST/baseline/r3" ] && [ "$(cat "$ST/baseline/CURRENT")" = r1 ]; pf $? "3 rebase refused, CURRENT still r1"
  # 4. the old root is untouched
  [ "$(cd "$R0" && find . -type f | LC_ALL=C sort | xargs sha256sum | sha256sum | cut -c1-16)" = "$sum0" ]; pf $? "4 root r0 byte for byte as before ($sum0)"
  rm -rf "$ST"
  echo "rebase_baseline: selftest $([ $fails -eq 0 ] && echo PASS || echo FAIL): $fails failed"
  return $(( fails > 0 ))
}

W=$(mktemp -d "${TMPDIR:-/tmp}/emb_rebase.XXXXXX"); trap 'rm -rf "$W"' EXIT
case "$MODE" in
  check) [ -n "$RUN" ] || { echo "rebase_baseline: --check-moves needs --run" >&2; exit 2; }; check_against "${ROOTID:-$(gates_root_id)}"; exit $? ;;
  rebase) do_rebase; exit $? ;;
  selftest) selftest; exit $? ;;
  *) sed -n '2,8p' "$0"; exit 2 ;;
esac
