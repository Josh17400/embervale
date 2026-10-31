#!/usr/bin/env bash
# tools/gates/lib.sh - shared helpers for step_gates.sh, wave_gates.sh and doll_gates.sh (sourced, never run).
#
# A gate line is one command with a NAME. gate_run NAME CHECK -- CMD... runs CMD (through tools/slot.sh when the
# caller says so), logs to $OUT/logs/NAME.log, times it, then calls the check function CHECK NAME LOG RC, which prints
# "PASS|FAIL|SKIP <note>" on its first line. The result line goes to $OUT/res/NAME.txt:
#   NAME PASS|FAIL|SKIP rc=<n> <secs>s | <note or last log line>
# summary.txt is the sorted concatenation (gates_summary). Baseline compares go through base_cmp.
#
# Variables the caller sets: REPO (repo root), OUT (output dir), BL (baseline dir for compares), REC (record dir, or
# empty), ONLY (comma list of name prefixes, or empty = all).

GATES_T0=$(date +%s)
SLOT_NOISE='^([^ ]*/)?(tools/)?slot\.sh: |^slot_alone: '   # a line tools/slot.sh (or bash running it) or slot_alone wrote
unset WITNESS_WATCH   # WITNESS_WATCH=1 makes a --witness game show its window: never in the gates

# gate_selected NAME: ONLY is a comma list of prefixes (S5 selects S5.rig2 and S5.rig1; S5.rig1 only that line)
gate_selected() {
  [ -z "${ONLY:-}" ] && return 0
  local n="$1" p IFS=','
  for p in $ONLY; do
    [ -z "$p" ] && continue
    case "$n" in "$p"|"$p".*) return 0 ;; esac
  done
  return 1
}

# norm FILE: the comparable text of a log - CR stripped, every line written by tools/slot.sh dropped (harness noise, not
# tool output: its own "slot.sh: ..." lines and bash's "/x/tools/slot.sh: line 84: ..." stale-slot race messages),
# timing tokens (12 ms, 3.4 s, 14.0s, 2 secs) replaced by <t>, trailing blanks cut.
norm() {
  tr -d '\r' < "$1" | grep -vE "$SLOT_NOISE" | sed -E \
    -e 's/[0-9]+(\.[0-9]+)? ?(ms|us|secs?|seconds?|min)\b/<t>/g' \
    -e 's/\b[0-9]+(\.[0-9]+)? ?s\b/<t>/g' \
    -e 's/ +/ /g' -e 's/[[:space:]]+$//'
}

# unstable_re NAME: one grep -E alternation of the unstable-line regexes recorded for NAME (empty when none)
unstable_re() {
  local f="${UNSTABLE_FILE:-$REPO/tools/gates/baseline/119de74/gates/unstable.txt}"
  [ -f "$f" ] || return 0
  tr -d '\r' < "$f" | awk -F'\t' -v n="$1" '$1 == n { sub(/[[:space:]]+#.*$/, "", $2); print $2 }' | paste -sd'|' -
}

# mask_re NAME: the same for the load-sensitive lines recorded as "NAME:mask<tab>regex". Such a line is KEPT in the
# compare (its text, place and count must match) but every number on it is replaced by # on both sides: the numbers
# depend on wall-clock work budgets (time-sliced realm focus / chunk streaming), so machine load changes them.
mask_re() { unstable_re "$1:mask"; }
# masked NAME: stdin -> stdout with the NAME:mask lines' numbers replaced by #
masked() {
  local mx; mx=$(mask_re "$1")
  if [ -n "$mx" ]; then awk -v re="$mx" '$0 ~ re { gsub(/[0-9]+/, "#") } { print }'; else cat; fi
}

# base_cmp NAME LOG [FILTER]: compare the normalized log (optionally piped through FILTER, a grep -E pattern of the
# lines that count) against $BL/NAME.txt. Record mode ($REC set) writes $REC/NAME.txt instead and passes.
# Prints "same" / "differs: <first differing line>" / "no baseline"; returns 0 only for same (or recorded).
base_cmp() {
  local name="$1" log="$2" filt="${3:-}" tmp
  tmp="$OUT/logs/$name.norm"
  if [ -n "$filt" ]; then norm "$log" | grep -E -- "$filt" > "$tmp"; else norm "$log" > "$tmp"; fi
  # lines that are not A/A-stable at 119de74 (two runs of one binary differ) are left out on both sides; each is listed
  # with its cause in baseline/119de74/gates/unstable.txt ("NAME<tab>regex  # cause") and reported as a finding
  local ex; ex=$(unstable_re "$name")
  if [ -n "$ex" ]; then grep -vE -- "$ex" "$tmp" > "$tmp.x"; mv "$tmp.x" "$tmp"; fi
  masked "$name" < "$tmp" > "$tmp.x"; mv "$tmp.x" "$tmp"
  if [ -n "${REC:-}" ]; then
    mkdir -p "$REC"; cp "$tmp" "$REC/$name.txt"; echo "recorded $(wc -l < "$tmp") lines"; return 0
  fi
  if [ ! -f "$BL/$name.txt" ]; then echo "no baseline $BL/$name.txt"; return 1; fi
  if cmp -s "$tmp" "$BL/$name.txt" || cmp -s "$tmp" <(norm "$BL/$name.txt" | { if [ -n "$ex" ]; then grep -vE -- "$ex"; else cat; fi; } | masked "$name"); then echo "same as baseline ($(wc -l < "$tmp") lines)"; return 0; fi
  local d
  d=$(diff <(norm "$BL/$name.txt" | { if [ -n "$ex" ]; then grep -vE -- "$ex"; else cat; fi; } | masked "$name") "$tmp" | grep -E '^[<>]' | head -2 | cut -c1-160 | tr '\n' ' ')
  echo "differs from baseline: $d"
  return 1
}

# lastline LOG: the last non-blank line, short
lastline() { tr -d '\r' < "$1" | grep -v '^[[:space:]]*$' | grep -vE "$SLOT_NOISE" | tail -n1 | cut -c1-150; }

# gate_run NAME CHECK [--slot] -- CMD...   (runs in the foreground; wrap in ( ... ) & for parallel lines)
gate_run() {
  local name="$1" check="$2"; shift 2
  local slot=0
  [ "${1:-}" = "--slot" ] && { slot=1; shift; }
  [ "${1:-}" = "--alone" ] && { slot=2; shift; }   # hold every slot (slot_alone): a timing line's rerun
  [ "${1:-}" = "--" ] && shift
  gate_selected "$name" || return 0
  mkdir -p "$OUT/logs" "$OUT/res"
  local log="$OUT/logs/$name.log" t0 t1 rc dt verdict note waited=""
  t0=$(date +%s)
  if [ $slot -eq 2 ]; then
    rm -f "$OUT/logs/$name.start"
    slot_alone bash -c 'date +%s > "$0"; exec "$@"' "$OUT/logs/$name.start" "$@" > "$log" 2>&1 < /dev/null; rc=$?
    t1=$(cat "$OUT/logs/$name.start" 2>/dev/null || echo "$t0"); rm -f "$OUT/logs/$name.start"
    [ $(( t1 - t0 )) -gt 2 ] && waited=" (+$(( t1 - t0 ))s waiting to run alone)"
  elif [ $slot -eq 1 ]; then
    # the duration counts from the moment the slot is held; the wait for it is reported apart
    rm -f "$OUT/logs/$name.start"
    "$REPO/tools/slot.sh" bash -c 'date +%s > "$0"; exec "$@"' "$OUT/logs/$name.start" "$@" > "$log" 2>&1 < /dev/null; rc=$?
    t1=$(cat "$OUT/logs/$name.start" 2>/dev/null || echo "$t0"); rm -f "$OUT/logs/$name.start"
    [ $(( t1 - t0 )) -gt 2 ] && waited=" (+$(( t1 - t0 ))s slot wait)"
  else t1=$t0; "$@" > "$log" 2>&1 < /dev/null; rc=$?; fi
  dt=$(( $(date +%s) - t1 ))
  note=$("$check" "$name" "$log" "$rc" 2>&1 | head -n1)
  verdict="${note%% *}"; note="${note#* }"
  case "$verdict" in PASS|FAIL|SKIP) ;; *) verdict=FAIL; note="check gave: $verdict $note" ;; esac
  [ -z "$note" ] && note=$(lastline "$log")
  printf '%s %s rc=%s %ss | %s%s\n' "$name" "$verdict" "$rc" "$dt" "$note" "$waited" | tee "$OUT/res/$name.txt"
}

# gate_skip NAME NOTE: record a SKIP line without running anything
gate_skip() {
  gate_selected "$1" || return 0
  mkdir -p "$OUT/res"
  printf '%s SKIP rc=- 0s | %s\n' "$1" "$2" | tee "$OUT/res/$1.txt"
}

# check functions: print "PASS|FAIL <note>"
chk_rc() { [ "$3" -eq 0 ] && echo "PASS $(lastline "$2")" || echo "FAIL $(lastline "$2")"; }
# rc 0 and a line matching $CHK_PAT (set per call through a wrapper)
chk_allok() { if [ "$3" -eq 0 ] && grep -q 'ALL OK' "$2"; then echo "PASS $(lastline "$2")"; else echo "FAIL $(lastline "$2")"; fi; }
# rc 0 and the normalized log equal to the baseline
chk_base() {
  local r; r=$(base_cmp "$1" "$2"); local ok=$?
  if [ "$3" -eq 0 ] && [ $ok -eq 0 ]; then echo "PASS $r | $(lastline "$2")"; else echo "FAIL rc=$3 $r | $(lastline "$2")"; fi
}
# rc 0, ALL OK, and the baseline compare
chk_allok_base() {
  local r; r=$(base_cmp "$1" "$2"); local ok=$?
  if [ "$3" -eq 0 ] && [ $ok -eq 0 ] && grep -q 'ALL OK' "$2"; then echo "PASS $r | $(lastline "$2")"; else echo "FAIL $r | $(lastline "$2")"; fi
}

# gates_summary: write $OUT/summary.txt (result lines in run order of names) and print the PASS table; return the
# number of FAIL lines (capped at 255).
gates_summary() {
  local f n fails=0 pass=0 skip=0
  : > "$OUT/summary.txt"
  for f in $(ls "$OUT/res"/*.txt 2>/dev/null | sort -V); do cat "$f" >> "$OUT/summary.txt"; done
  fails=$(grep -c '^[^ ]* FAIL ' "$OUT/summary.txt")
  pass=$(grep -c '^[^ ]* PASS ' "$OUT/summary.txt")
  skip=$(grep -c '^[^ ]* SKIP ' "$OUT/summary.txt")
  n=$(( $(date +%s) - GATES_T0 ))
  echo "TOTAL pass=$pass fail=$fails skip=$skip wall=${n}s" >> "$OUT/summary.txt"
  echo "================ $(basename "$0") $(cat "$OUT/build.sha" 2>/dev/null) ================"
  awk '{ if ($1 == "TOTAL") { print; next }; name=$1; v=$2; rc=$3; t=$4; $1=$2=$3=$4=""; sub(/^ *\| */, "", $0)
         printf "%-16s %-4s %-6s %6s  %s\n", name, v, rc, t, substr($0, 1, 96) }' "$OUT/summary.txt"
  echo "summary: $OUT/summary.txt"
  [ "$fails" -gt 255 ] && fails=255
  return "$fails"
}

# write_sha: <out>/build.sha = HEAD sha, plus "+dirty" and the dirty file count when the tree has changes
write_sha() {
  local sha dirty
  sha=$(git -C "$REPO" rev-parse HEAD 2>/dev/null || echo unknown)
  dirty=$(git -C "$REPO" status --porcelain 2>/dev/null | wc -l)
  if [ "$dirty" -gt 0 ]; then echo "$sha +dirty($dirty files)" > "$OUT/build.sha"; else echo "$sha" > "$OUT/build.sha"; fi
}

# winpath P: Windows form (C:/...) of a Git Bash path
winpath() { cygpath -m "$1"; }

# headed_launch FILE...: prints the first line of FILE that starts embervale(.exe) without --witness (a visible game
# window: --script, --shot, --play or a bare launch; spawn(exe, ['--script', ...]) in .mjs too) and returns 0; returns 1
# when none does. The gates never start such a tool: the owner's PC must never get a visible game window (wave 0 rule).
headed_launch() {
  local f hit
  for f in "$@"; do
    [ -f "$f" ] || continue
    hit=$(grep -nE "(embervale(\.exe)?[\"']?[[:space:]]+[^#]*--(script|shot|play)\b|spawn\([^)]*['\"]--(script|shot|play)['\"]|\b(EXE|exe)\b[^;]*['\"]--(script|shot|play)['\"])" "$f" \
          | grep -v -- '--witness' | grep -vE '^[0-9]+:[[:space:]]*(#|//)' | head -1)
    [ -n "$hit" ] && { echo "$(basename "$f"):$hit" | cut -c1-140; return 0; }
  done
  return 1
}

# slot_alone CMD...: run CMD while holding EVERY tools/slot.sh slot the cap allows (game_cap.sh: 5, or 2 while the owner
# plays), so no build, gate or slot-run Witness instance runs beside it: how a timing line is "rerun alone" before it
# counts (SOLID_PLAN 4.4). An alone.lock mutex comes first, so two alone-runners never each hold part of the slots and
# wait on each other. Slots are taken as they free up and kept; after ALONE_WAIT seconds (default 1800) CMD runs with
# what is held and "slot_alone: not alone (held k of n)" goes to stderr. Never call it while this process tree already
# holds a slot (it would wait on itself).
slot_root() {
  if [ -n "${EMB_SLOT_ROOT:-}" ]; then echo "$EMB_SLOT_ROOT"
  elif [ -n "${LOCALAPPDATA:-}" ] && command -v cygpath >/dev/null 2>&1; then echo "$(cygpath -u "$LOCALAPPDATA")/Temp/emb_slots"
  else echo /tmp/emb_slots; fi
}
slot_alone() (
  R=$(slot_root); mkdir -p "$R"; HELD=(); LOCK="$R/alone.lock"; t0=$(date +%s); waitmax="${ALONE_WAIT:-1800}"
  release() { local d; for d in "${HELD[@]+"${HELD[@]}"}"; do rm -rf "$d"; done; [ "${GOTLOCK:-0}" = 1 ] && rm -rf "$LOCK"; }
  trap release EXIT; trap 'release; exit 130' INT TERM HUP
  stale_d() {   # tools/slot.sh's rule: owner gone, older than EMB_SLOT_MAX_AGE, or ownerless for 60 s
    local d="$1" pid t _r
    if [ -f "$d/owner" ]; then read -r pid t _r < "$d/owner" 2>/dev/null || return 1
      case "$t" in ""|*[!0-9]*) t=0 ;; esac
      [ $(( $(date +%s) - t )) -gt "${EMB_SLOT_MAX_AGE:-10800}" ] && return 0
      [ -n "$pid" ] && ! kill -0 "$pid" 2>/dev/null && return 0; return 1; fi
    [ $(( $(date +%s) - $(stat -c %Y "$d" 2>/dev/null || echo 0) )) -gt 60 ]
  }
  GOTLOCK=0
  while :; do
    if mkdir "$LOCK" 2>/dev/null; then { echo "$BASHPID $(date +%s)" > "$LOCK/owner"; } 2>/dev/null; GOTLOCK=1; break; fi
    stale_d "$LOCK" && { mv "$LOCK" "$LOCK.stale.$BASHPID" 2>/dev/null && rm -rf "$LOCK.stale.$BASHPID"; continue; }
    [ $(( $(date +%s) - t0 )) -gt "$waitmax" ] && break
    sleep 2
  done
  while :; do
    c=$(bash "$REPO/tools/game_cap.sh" 2>/dev/null); case "$c" in ""|*[!0-9]*) c="${EMB_SLOTS:-5}" ;; esac
    for i in $(seq 1 "$c"); do
      d="$R/$i"; case " ${HELD[*]+"${HELD[*]}"} " in *" $d "*) continue ;; esac
      [ -d "$d" ] && stale_d "$d" && { mv "$d" "$d.stale.$BASHPID" 2>/dev/null && rm -rf "$d.stale.$BASHPID"; }
      # (a slot another waiter removed between our mkdir and the owner write is simply not ours: no message, not held)
      if mkdir "$d" 2>/dev/null; then { echo "$BASHPID $(date +%s) slot_alone (holds every slot: a timing rerun) $*" > "$d/owner"; } 2>/dev/null && HELD+=("$d"); fi
    done
    [ ${#HELD[@]} -ge "$c" ] && break
    # another every-slot holder (perf_pack.sh takes no alone.lock) holds part of the slots: give ours back so the two
    # never wait on each other, and start over once it is done
    other=""
    for f in "$R"/[0-9]*/owner; do
      [ -f "$f" ] && grep -qs 'holds every slot' "$f" && { read -r op _r < "$f" 2>/dev/null; [ "$op" != "$BASHPID" ] && other=1; }
    done
    if [ ${#HELD[@]} -gt 0 ] && [ -n "$other" ]; then
      for d in "${HELD[@]}"; do rm -rf "$d"; done; HELD=()
    fi
    if [ $(( $(date +%s) - t0 )) -gt "$waitmax" ]; then echo "slot_alone: not alone (held ${#HELD[@]} of $c after ${waitmax}s)" >&2; break; fi
    sleep 2
  done
  "$@"
)

# ci_pool LIST LOGDIR TAG [--defer]: run every command line of LIST (one per line) as `ci_exec "<line>"` - ci_exec is a
# function the caller defines and exports (export -f) - CI_JOBS (default 5) at a time, each in its own tools/slot.sh slot.
# A command that fails is rerun ONCE through slot_alone (alone on the machine: timing checks such as rpg_test --doll
# --gates B10 fail under load) and only then counts; the first try's log is kept as LOGDIR/<TAG>_fail_<n>.log (n = line
# number), a failed rerun's as <TAG>_fail_<n>.rerun.log. Without --defer the reruns follow at once (ci_finish); with
# --defer the failures are only listed (<TAG>_retry.txt, "RETRY" lines, <TAG>_PENDING=k) and the caller runs
# `ci_finish LIST LOGDIR TAG` later, when its own parallel lines are done (wave_gates: before W6.perf).
# ci_finish prints one "ok|FAIL rc=N <cmd> <secs>s | <last line>" per command in list order (ok lines that needed the
# rerun say so), then <TAG>_CMDS=n, <TAG>_RERUN_OK=k and <TAG>_FAILS=f; returns 0 only when n > 0 and f = 0.
ci_pool() {
  local list="$1" dir="$2" tag="$3" defer="${4:-}" jobs="${CI_JOBS:-5}" n=0 running=0 c i rc dt pend=0
  mkdir -p "$dir"; rm -f "$dir/${tag}"_cmd_* "$dir/${tag}"_fail_* "$dir/${tag}_retry.txt"
  while IFS= read -r c || [ -n "$c" ]; do
    c="${c%$'\r'}"; [ -z "$c" ] && continue; n=$((n+1))
    ( "$REPO/tools/slot.sh" bash -c 'date +%s > "$0"; ci_exec "$1"' "$dir/${tag}_cmd_$n.start" "$c" > "$dir/${tag}_cmd_$n.log" 2>&1 < /dev/null
      echo "$? $(( $(date +%s) - $(cat "$dir/${tag}_cmd_$n.start" 2>/dev/null || date +%s) ))" > "$dir/${tag}_cmd_$n.rc" ) &
    running=$((running+1)); if [ $running -ge "$jobs" ]; then wait -n 2>/dev/null || wait; running=$((running-1)); fi
  done < "$list"
  wait
  i=0; : > "$dir/${tag}_retry.txt"
  while IFS= read -r c || [ -n "$c" ]; do
    c="${c%$'\r'}"; [ -z "$c" ] && continue; i=$((i+1))
    read -r rc dt < "$dir/${tag}_cmd_$i.rc" 2>/dev/null || { rc=99; dt=0; }
    echo "$rc $dt" > "$dir/${tag}_cmd_$i.rc"
    if [ "$rc" -ne 0 ]; then pend=$((pend+1)); printf '%s\t%s\n' "$i" "$c" >> "$dir/${tag}_retry.txt"; mv "$dir/${tag}_cmd_$i.log" "$dir/${tag}_fail_$i.log"
      [ "$defer" = --defer ] && echo "RETRY rc=$rc $c ${dt}s | $(lastline "$dir/${tag}_fail_$i.log") (rerun alone later; log ${tag}_fail_$i.log)"
    elif [ "$defer" = --defer ]; then echo "ok $c ${dt}s | $(lastline "$dir/${tag}_cmd_$i.log")"; fi
  done < "$list"
  rm -f "$dir/${tag}"_cmd_*.start
  if [ "$defer" = --defer ]; then echo "$(echo "$tag" | tr 'a-z' 'A-Z')_PENDING=$pend"; [ "$pend" -eq 0 ] && [ "$n" -gt 0 ]; return; fi
  ci_finish "$list" "$dir" "$tag"
}
ci_finish() {
  local list="$1" dir="$2" tag="$3" c i rc dt first fails=0 rerun_ok=0 t1 TAGU
  TAGU=$(echo "$tag" | tr 'a-z' 'A-Z'); i=0
  while IFS= read -r c || [ -n "$c" ]; do
    c="${c%$'\r'}"; [ -z "$c" ] && continue; i=$((i+1))
    read -r rc dt < "$dir/${tag}_cmd_$i.rc" 2>/dev/null || { rc=99; dt=0; }
    if [ "$rc" -eq 0 ]; then echo "ok $c ${dt}s | $(lastline "$dir/${tag}_cmd_$i.log")"; continue; fi
    first=$rc; t1=$(date +%s)
    slot_alone bash -c 'ci_exec "$1"' _ "$c" > "$dir/${tag}_cmd_$i.log" 2>&1 < /dev/null; rc=$?
    dt=$(( $(date +%s) - t1 )); echo "$rc $dt" > "$dir/${tag}_cmd_$i.rc"
    if [ "$rc" -eq 0 ]; then rerun_ok=$((rerun_ok+1))
      echo "ok $c ${dt}s | $(lastline "$dir/${tag}_cmd_$i.log") (first try rc=$first under load, ok rerun alone; log ${tag}_fail_$i.log)"
    else fails=$((fails+1)); mv "$dir/${tag}_cmd_$i.log" "$dir/${tag}_fail_$i.rerun.log"
      echo "FAIL rc=$rc $c ${dt}s | $(lastline "$dir/${tag}_fail_$i.rerun.log") (failed alone too; logs ${tag}_fail_$i.log, ${tag}_fail_$i.rerun.log)"
      tr -d '\r' < "$dir/${tag}_fail_$i.rerun.log" | grep -m3 -iE 'fail' | cut -c1-200 | sed 's/^/    /'
    fi
  done < "$list"
  rm -f "$dir/${tag}"_cmd_*.rc
  echo "${TAGU}_CMDS=$i"; echo "${TAGU}_RERUN_OK=$rerun_ok"; echo "${TAGU}_FAILS=$fails"
  [ "$i" -gt 0 ] && [ "$fails" -eq 0 ]
}
