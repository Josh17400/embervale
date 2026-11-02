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

REPO="${REPO:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
GATES_T0=$(date +%s)

# The baseline root (P3, docs/FIX_BATCH_P3.md 10): every gate reads its records from tools/gates/baseline/<root>/, where
# <root> is the one word in tools/gates/baseline/CURRENT (119de74 = the wave-0 record; rebase_baseline.sh writes a new
# root and flips CURRENT). GATES_BASE_ID = that word, GATES_BASE = the root dir, GATES_BASE_REV = the git revision the
# root was recorded at (<root>/REV, or the root's name when it has none: 119de74 is its own revision). GATES_CURRENT_FILE
# and GATES_BASELINE_DIR override the pointer file and the baseline dir (rebase_baseline.sh --selftest).
gates_baseline_dir() { echo "${GATES_BASELINE_DIR:-$REPO/tools/gates/baseline}"; }
gates_root_id() { tr -dc '[:alnum:]_.-' < "${GATES_CURRENT_FILE:-$(gates_baseline_dir)/CURRENT}" 2>/dev/null; }
gates_root_rev() {   # gates_root_rev [ID]
  local id="${1:-$(gates_root_id)}" f; f="$(gates_baseline_dir)/$id/REV"
  if [ -f "$f" ]; then tr -dc '[:alnum:]' < "$f"; else echo "$id"; fi
}
GATES_BASE_ID="$(gates_root_id)"
if [ -z "$GATES_BASE_ID" ] || [ ! -d "$(gates_baseline_dir)/$GATES_BASE_ID" ]; then
  echo "lib.sh: no baseline root: $(gates_baseline_dir)/CURRENT names '${GATES_BASE_ID}' (see tools/gates/baseline/README.md)" >&2
fi
GATES_BASE="$(gates_baseline_dir)/$GATES_BASE_ID"
GATES_BASE_REV="$(gates_root_rev "$GATES_BASE_ID")"
SLOT_NOISE='^([^ ]*/)?(tools/)?slot\.sh: |^slot_alone: '   # a line tools/slot.sh (or bash running it) or slot_alone wrote
unset WITNESS_WATCH   # WITNESS_WATCH=1 makes a --witness game show its window: never in the gates

# ---- Witness (P4 7a): found ONCE, in this order, exported as WITNESS_HOME (C:/... form) for every tool below ----
#   1. WITNESS_HOME (an explicit setting that holds no bin/witness.js stops the run: it is never silently passed over)
#   2. $REPO/../witness                       (a checkout or worktree beside Witness)
#   3. <origin>/../witness                    (a clone whose origin is a local checkout: the scratchpad clones)
#   4. $HOME/Documents/witness                (Git Bash's HOME is %USERPROFILE%)
# witness_find sets WITNESS_HOME and WITNESS_FROM (which rule) and returns 0, or sets WITNESS_LOOKED (every place tried)
# and returns 1. witness_require NAME... prints "<tool>: Witness <dir> (<rule>)" and, when none is found and any NAME
# is selected (gate_selected), "Witness not found: looked in ...; set WITNESS_HOME" and exits 2 before any gate runs.
witness_ok() { [ -n "$1" ] && [ -f "$1/bin/witness.js" ] && [ -f "$1/src/core/launch.js" ]; }
witness_origin_dir() {   # the local checkout this clone came from (empty when origin is a URL or missing)
  local o; o=$(git -C "$REPO" remote get-url origin 2>/dev/null | tr -d '\r') || return 0
  case "$o" in file://*) o="${o#file://}" ;; /*|[A-Za-z]:/*|[A-Za-z]:\\*) ;; *) return 0 ;; esac
  command -v cygpath >/dev/null 2>&1 && o=$(cygpath -u "$o" 2>/dev/null || echo "$o")
  [ "$(basename "$o")" = .git ] && o=$(dirname "$o")
  [ -d "$o" ] && (cd "$o" && pwd)
}
witness_find() {
  local c="" o r="" i tried=() why=()
  WITNESS_FROM=""; WITNESS_LOOKED=""
  if [ -n "${WITNESS_HOME:-}" ]; then
    witness_ok "$WITNESS_HOME" || { WITNESS_LOOKED="WITNESS_HOME=$WITNESS_HOME (set, but no bin/witness.js there)"; return 1; }
    c="$WITNESS_HOME"; r="${WITNESS_FROM_GATES:-WITNESS_HOME}"   # (a gate script found it first: its rule)
  else
    o=$(witness_origin_dir)
    tried=("$REPO/../witness"); why=("../witness beside this checkout")
    [ -n "$o" ] && { tried+=("$o/../witness"); why+=("../witness beside the clone's origin $o"); }
    tried+=("${HOME:-/nonexistent}/Documents/witness"); why+=("\$HOME/Documents/witness")
    for i in "${!tried[@]}"; do witness_ok "${tried[$i]}" && { c="${tried[$i]}"; r="${why[$i]}"; break; }; done
    [ -n "$c" ] || { WITNESS_LOOKED="WITNESS_HOME (unset), $(printf "%s, " "${tried[@]}" | sed "s/, $//")"; return 1; }
  fi
  c=$(cd "$c" && pwd); command -v cygpath >/dev/null 2>&1 && c=$(cygpath -m "$c")
  export WITNESS_HOME="$c" WITNESS_FROM_GATES="$r"; WITNESS_FROM="$r"
}
witness_require() {   # witness_require NAME...: stop (exit 2) when a selected line needs Witness and none is found
  local n need=0 tool; tool=$(basename "$0" .sh)
  for n in "$@"; do gate_selected "$n" && { need=1; break; }; done
  if witness_find; then echo "$tool: Witness $WITNESS_HOME ($WITNESS_FROM)"; return 0; fi
  if [ $need -eq 1 ]; then echo "$tool: Witness not found: looked in $WITNESS_LOOKED; set WITNESS_HOME" >&2; exit 2; fi
  echo "$tool: Witness not found (looked in $WITNESS_LOOKED); no selected line needs it"
}

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
  local f="${UNSTABLE_FILE:-$GATES_BASE/gates/unstable.txt}"
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
  # lines that are not A/A-stable (two runs of one binary differ) are left out on both sides; each is listed with its
  # cause in <root>/gates/unstable.txt ("NAME<tab>regex  # cause") and reported as a finding
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
  local log="$OUT/logs/$name.log" t0 t1 rc dt verdict note waited="" ld0
  t0=$(date +%s); ld0=$(slots_busy)
  if [ $slot -eq 2 ]; then
    rm -f "$OUT/logs/$name.start"
    slot_alone bash -c 'date +%s > "$0"; exec "$@"' "$OUT/logs/$name.start" "$@" > "$log" 2>&1 < /dev/null; rc=$?
    t1=$(cat "$OUT/logs/$name.start" 2>/dev/null || echo "$t0"); rm -f "$OUT/logs/$name.start"
    waited=" [$(alone_note "$log")]"
    [ $(( t1 - t0 )) -gt 2 ] && waited="$waited (+$(( t1 - t0 ))s waiting to run alone)"
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
  # (P4 7c) a FAIL whose every failing or differing line is on the alone-rerun list is rerun alone with today's limits
  if [ "$verdict" = FAIL ] && [ $slot -ne 2 ] && [ -z "${REC:-}" ] && [ "${ALONE_RERUN:-1}" != 0 ] && [ -n "$(alone_list_re "$name")" ]; then
    local ar; ar=$(alone_try "$name" "$check" "$log" "$rc" "$ld0" "$@")
    verdict="${ar%%|*}"; ar="${ar#*|}"; rc="${ar%%|*}"; note="${ar#*|}"
  fi
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

# ---- the machine's CPU (shared by slot_alone and perf_pack.sh; P3 C3) ----
# cpu_start FILE: sample the machine CPU (typeperf, % of all logical processors) into FILE once a second, in the
# background (CPU_PID); cpu_stop ends it. Without typeperf (Linux) nothing is sampled and cpu_avg prints nothing.
# cpu_avg N: mean CPU % over the last N samples ("" when fewer); cpu_nsamp: how many samples so far.
# The machine's idle level is remembered across runs in <slot root>/idle_cpu ("<percent> <epoch>"): the lowest 5 s mean
# any quiet wait saw in the last 7 days (this PC idles at ~14 % with tray apps). cpu_idle prints the level to use: that
# file, else the lowest 5 s mean seen in this wait (IDLE_SEEN), never above CPU_IDLE_MAX (25).
cpu_start() {
  CPUCSV="$1"; : > "$CPUCSV"; CPU_PID=""
  command -v typeperf >/dev/null 2>&1 || return 0
  typeperf "\\Processor(_Total)\\% Processor Time" -si 1 > "$CPUCSV" 2>/dev/null < /dev/null &
  CPU_PID=$!
}
cpu_stop() { [ -n "${CPU_PID:-}" ] && kill "$CPU_PID" 2>/dev/null; CPU_PID=""; }
cpu_nsamp() { tr -d '\r' < "$CPUCSV" 2>/dev/null | grep -cE '^"[^"]*","[0-9.]+"$'; }
cpu_avg() {
  tr -d '\r' < "$CPUCSV" 2>/dev/null | awk -F'","' 'NF == 2 && $2 ~ /^[0-9.]+"?$/ { v = $2; gsub(/"/, "", v); a[++n] = v }
    END { k = '"$1"'; if (n < k) exit; for (i = n - k + 1; i <= n; i++) s += a[i]; printf("%.0f", s / k) }'
}
cpu_idle_file() { echo "$(slot_root)/idle_cpu"; }
cpu_idle_note() {   # cpu_idle_note PCT: remember PCT as the machine's idle level when it is lower (or the record is old)
  local f v t; f="$(cpu_idle_file)"; v=""; t=0
  [ -f "$f" ] && read -r v t < "$f" 2>/dev/null
  case "$v" in ""|*[!0-9]*) v="" ;; esac; case "$t" in ""|*[!0-9]*) t=0 ;; esac
  if [ -z "$v" ] || [ "$1" -lt "$v" ] || [ $(( $(date +%s) - t )) -gt 604800 ]; then echo "$1 $(date +%s)" > "$f" 2>/dev/null; fi
}
cpu_idle() {
  local f v t m="${CPU_IDLE_MAX:-25}"; f="$(cpu_idle_file)"; v=""; t=0
  [ -f "$f" ] && read -r v t < "$f" 2>/dev/null
  case "$v" in ""|*[!0-9]*) v="" ;; esac; case "$t" in ""|*[!0-9]*) t=0 ;; esac
  [ -n "$v" ] && [ $(( $(date +%s) - t )) -gt 604800 ] && v=""
  [ -z "$v" ] && v="${IDLE_SEEN:-$m}"
  [ -n "${IDLE_SEEN:-}" ] && [ "$IDLE_SEEN" -lt "$v" ] && v="$IDLE_SEEN"
  [ "$v" -gt "$m" ] && v="$m"
  echo "$v"
}
# quiet_wait MAXSECS: wait (sampling, cpu_start must run) until the 5 s mean CPU is at or below the machine's idle level +
# QUIET_ABOVE (default 15) %, at least 10 samples in; at most MAXSECS. Sets QUIET_CPU (the last 5 s mean, "?" without
# samples) and QUIET_OK (1 quiet, 0 not). Without typeperf it returns at once with QUIET_OK=1, QUIET_CPU="?".
quiet_wait() {
  local max="$1" t0=$SECONDS c lim
  QUIET_OK=0; QUIET_CPU="?"; IDLE_SEEN=""
  if [ -z "${CPU_PID:-}" ]; then QUIET_OK=1; return 0; fi
  while :; do
    c="$(cpu_avg 5)"
    if [ -n "$c" ]; then
      QUIET_CPU="$c"
      { [ -z "$IDLE_SEEN" ] || [ "$c" -lt "$IDLE_SEEN" ]; } && IDLE_SEEN="$c"
      lim=$(( $(cpu_idle) + ${QUIET_ABOVE:-15} ))
      if [ "$(cpu_nsamp)" -ge 10 ] && [ "$c" -le "$lim" ]; then QUIET_OK=1; break; fi
    fi
    if [ $((SECONDS - t0)) -ge "$max" ]; then [ "$QUIET_CPU" = "?" ] && { c="$(cpu_avg 1)"; [ -n "$c" ] && QUIET_CPU="$c"; }; break; fi
    sleep 1
  done
  [ -n "$IDLE_SEEN" ] && [ "$(cpu_nsamp)" -ge 10 ] && cpu_idle_note "$IDLE_SEEN"
  return 0
}

# slot_alone CMD...: run CMD while holding EVERY tools/slot.sh slot, 1..EMB_SLOTS (5), whatever the cap: while the owner
# plays (game_cap.sh 2) slot.sh only hands out 1..2, but jobs that started before still hold 3-5, so slot_alone waits
# for those to drain too (P3 C3). Then it waits for the machine to be quiet (load outside slot.sh: a WSL build,
# another session's Witness instances): the 5 s mean CPU at the machine's idle level + 15 % (quiet_wait), capped at
# ALONE_QUIET_WAIT (240) seconds. How a timing line is "rerun alone" before it counts (SOLID_PLAN 4.4). An alone.lock
# mutex comes first, so two alone-runners never each hold part of the slots and wait on each other. Slots are taken as
# they free up and kept; after ALONE_WAIT seconds (default 1800) CMD runs with what is held. Before CMD it writes ONE
# status line to stderr:
#   slot_alone: alone (held 5/5, cpu 16 %)
#   slot_alone: NOT ALONE (held 2/5, cpu 41 %) after <s> s
# alone_note LOG reads it back ("alone" / "NOT ALONE (held k/5, cpu x %)") for the rerun's result line. Never call it
# while this process tree already holds a slot (it would wait on itself).
slot_root() {
  if [ -n "${EMB_SLOT_ROOT:-}" ]; then echo "$EMB_SLOT_ROOT"
  elif [ -n "${LOCALAPPDATA:-}" ] && command -v cygpath >/dev/null 2>&1; then echo "$(cygpath -u "$LOCALAPPDATA")/Temp/emb_slots"
  else echo /tmp/emb_slots; fi
}
slot_alone() (
  R=$(slot_root); mkdir -p "$R"; HELD=(); LOCK="$R/alone.lock"; t0=$(date +%s); waitmax="${ALONE_WAIT:-1800}"
  N="${EMB_SLOTS:-5}"; CPU_PID=""
  release() { local d; cpu_stop; for d in "${HELD[@]+"${HELD[@]}"}"; do rm -rf "$d"; done; [ "${GOTLOCK:-0}" = 1 ] && rm -rf "$LOCK"; }
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
    for i in $(seq 1 "$N"); do
      d="$R/$i"; case " ${HELD[*]+"${HELD[*]}"} " in *" $d "*) continue ;; esac
      [ -d "$d" ] && stale_d "$d" && { mv "$d" "$d.stale.$BASHPID" 2>/dev/null && rm -rf "$d.stale.$BASHPID"; }
      # (a slot another waiter removed between our mkdir and the owner write is simply not ours: no message, not held)
      if mkdir "$d" 2>/dev/null; then { echo "$BASHPID $(date +%s) slot_alone (holds every slot: a timing rerun) $*" > "$d/owner"; } 2>/dev/null && HELD+=("$d"); fi
    done
    [ ${#HELD[@]} -ge "$N" ] && break
    # another every-slot holder (perf_pack.sh takes no alone.lock) holds part of the slots: give ours back so the two
    # never wait on each other, and start over once it is done
    other=""
    for f in "$R"/[0-9]*/owner; do
      [ -f "$f" ] && grep -qs 'holds every slot' "$f" && { read -r op _r < "$f" 2>/dev/null; [ "$op" != "$BASHPID" ] && other=1; }
    done
    if [ ${#HELD[@]} -gt 0 ] && [ -n "$other" ]; then
      for d in "${HELD[@]}"; do rm -rf "$d"; done; HELD=()
    fi
    [ $(( $(date +%s) - t0 )) -gt "$waitmax" ] && break
    sleep 2
  done
  # every slot held (or the wait ran out): now the machine itself must be quiet
  cpu_start "$R/alone_cpu.$BASHPID.csv"
  quiet_wait "${ALONE_QUIET_WAIT:-240}"
  cpu_stop; rm -f "$R/alone_cpu.$BASHPID.csv"
  if [ ${#HELD[@]} -ge "$N" ] && [ "$QUIET_OK" = 1 ]; then echo "slot_alone: alone (held ${#HELD[@]}/$N, cpu $QUIET_CPU %)" >&2
  else echo "slot_alone: NOT ALONE (held ${#HELD[@]}/$N, cpu $QUIET_CPU %) after $(( $(date +%s) - t0 )) s" >&2; fi
  EMB_ALONE=1 "$@"   # (the command may tell it runs alone: the alone_rerun selftest's fake load reads it)
)
# alone_note LOG: "alone" or "NOT ALONE (held k/5, cpu x %)" from slot_alone's status line in LOG ("alone?" when none)
alone_note() {
  local l; l=$(tr -d '\r' < "$1" 2>/dev/null | grep -m1 -E '^slot_alone: (alone|NOT ALONE) ')
  case "$l" in
    "slot_alone: alone "*) echo "alone" ;;
    "slot_alone: NOT ALONE "*) l="${l#slot_alone: }"; echo "${l% after *}" ;;
    *) echo "alone? (no slot_alone status)" ;;
  esac
}

# ---- the alone-rerun list (P4 7c; docs/FIX_BATCH_P4.md 8): tools/gates/alone_rerun.txt ----
# One line per load-sensitive check: "<gate>[,<gate>...] <regex>  # why". A gate line that FAILs where EVERY failing or
# differing line matches its gate's regexes (the evidence: the lines that differ from the record and the FAIL lines,
# less the summary lines that only count them, ALONE_CONSEQ) is rerun ALONE (slot_alone: every slot held, the CPU
# quiet) with today's limits: the same command, nothing loosened. It passes only when that rerun matches the record;
# its line then says
#   (alone rerun: <regex> failed under load; first log kept: logs/<NAME>.first.log) [alone|NOT ALONE ...]
#   (first try: k/5 slots busy at its start, j/5 at its end, cpu x %; rerun Ns)
# A line with any unlisted failing or differing line, or a gate with no entry, is never rerun. Hooks a caller may
# define (fn = NAME with every non-alnum as _, e.g. S2_seeds):
#   alone_ev_<fn> NAME LOG                          prints the evidence lines (default alone_evidence_default)
#   alone_rr_<fn> NAME LOG RERUNLOG CHECK -- CMD... the rerun (default: CMD again under slot_alone, then CHECK on it);
#                                                   writes RERUNLOG, prints "PASS|FAIL <note>"
# ALONE_RERUN=0 turns the reruns off (a raw loaded run); ALONE_RERUN_FILE points at another list (the selftest);
# ALONE_KEY_PREFIX goes before NAME when the list is read (doll_gates.sh: W4.doll.<line>).
alone_file() { echo "${ALONE_RERUN_FILE:-$REPO/tools/gates/alone_rerun.txt}"; }
alone_list_re() {   # alone_list_re NAME: the alternation of NAME's listed regexes (empty when none)
  local f; f=$(alone_file); [ -f "$f" ] || return 0
  tr -d '\r' < "$f" | awk -v n="${ALONE_KEY_PREFIX:-}$1" '/^[[:space:]]*(#|$)/ { next }
    { g = $1; r = $0; sub(/^[^[:space:]]+[[:space:]]+/, "", r); sub(/[[:space:]]+#[[:space:]].*$/, "", r)
      k = split(g, gs, ","); for (i = 1; i <= k; i++) if (gs[i] == n) { print r; break } }' | paste -sd'|' -
}
alone_fn() { echo "${1//[^A-Za-z0-9]/_}"; }
slots_busy() {   # "k/N": tools/slot.sh slots held right now (any holder on this machine)
  local n; n=$(ls "$(slot_root)" 2>/dev/null | grep -cE '^[0-9]+$'); echo "$n/${EMB_SLOTS:-5}"
}
cpu_now() {   # the machine CPU % over about a second ("?" without typeperf)
  command -v typeperf >/dev/null 2>&1 || { echo "?"; return; }
  typeperf "\\Processor(_Total)\\% Processor Time" -sc 2 2>/dev/null < /dev/null | tr -d '\r' | grep -E '^"[^"]*","[0-9.]+"$' | tail -1 \
    | awk -F'","' '{ v = $2; gsub(/"/, "", v); printf("%.0f", v) }'
}
# base_ref NAME: the record's comparable text (base_cmp's left side)
base_ref() { local ex; ex=$(unstable_re "$1"); norm "$BL/$1.txt" | { if [ -n "$ex" ]; then grep -vE -- "$ex"; else cat; fi; } | masked "$1"; }
ALONE_CONSEQ='^(save_test: )?(FAILED \([0-9]+\)|ALL OK)$|^[0-9]+ passed, [0-9]+ failed|^sim-digest (PASS|FAIL): |^(rpg_test|doll|rig): [0-9]+ failures?'
alone_evidence_default() {   # NAME LOG: the differing lines (both sides) and the FAIL lines, less ALONE_CONSEQ
  local name="$1" log="$2"
  { if [ -f "$OUT/logs/$name.norm" ] && [ -f "$BL/$name.txt" ]; then diff <(base_ref "$name") "$OUT/logs/$name.norm" | grep -E '^[<>] ' | cut -c3-; fi
    norm "$log" | grep -E '(^|[^A-Za-z])FAIL'; } | sed -E 's/^ +//' | grep -vE -- "$ALONE_CONSEQ" | grep -v '^$' | sort -u
}
alone_rerun_default() {   # NAME LOG RERUNLOG CHECK -- CMD...
  local name="$1" rl="$3" chk="$4" rc; shift 4; [ "${1:-}" = -- ] && shift
  slot_alone "$@" > "$rl" 2>&1 < /dev/null; rc=$?
  "$chk" "$name" "$rl" "$rc" 2>&1 | head -n1
}
# alone_try NAME CHECK LOG RC LOAD0 CMD...: prints "VERDICT|RC|note", the gate line's verdict, rc and note after the rule
alone_try() {
  local name="$1" chk="$2" log="$3" rc="$4" ld0="$5" re fn ev bad hit ld1 cpu r v rl t0 tn; shift 5
  re=$(alone_list_re "$name"); fn=$(alone_fn "$name")
  ld1=$(slots_busy); cpu=$(cpu_now)
  if declare -F "alone_ev_$fn" > /dev/null; then ev=$("alone_ev_$fn" "$name" "$log"); else ev=$(alone_evidence_default "$name" "$log"); fi
  if [ -z "$ev" ]; then echo "FAIL|$rc|$(lastline "$log") (listed for an alone rerun, but no failing or differing line to judge: not rerun)"; return; fi
  bad=$(echo "$ev" | grep -vE -- "$re" | head -1 | cut -c1-100)
  if [ -n "$bad" ]; then
    r=$("$chk" "$name" "$log" "$rc" 2>&1 | head -n1); echo "FAIL|$rc|${r#* } (not rerun: an unlisted line fails or differs: '$bad')"; return
  fi
  hit=$(echo "$ev" | grep -oE -- "$re" | sort -u | head -3 | paste -sd';' - | cut -c1-80)
  cp "$log" "$OUT/logs/$name.first.log"; [ -f "$OUT/logs/$name.norm" ] && cp "$OUT/logs/$name.norm" "$OUT/logs/$name.first.norm"
  rl="$OUT/logs/$name.alone.log"; t0=$(date +%s)
  if declare -F "alone_rr_$fn" > /dev/null; then r=$("alone_rr_$fn" "$name" "$log" "$rl" "$chk" -- "$@")
  else r=$(alone_rerun_default "$name" "$log" "$rl" "$chk" -- "$@"); fi
  v="${r%% *}"; r="${r#* }"
  tn="(alone rerun: $hit failed under load; first log kept: logs/$name.first.log) [$(alone_note "$rl")] (first try: $ld0 slots busy at its start, $ld1 at its end, cpu ${cpu:-?} %; rerun $(( $(date +%s) - t0 ))s)"
  if [ "$v" = PASS ]; then echo "PASS|0|$r $tn"
  else echo "FAIL|$rc|$r (failed on the alone rerun too: logs/$name.alone.log) $tn"; fi
}

# ci_pool LIST LOGDIR TAG [--defer]: run every command line of LIST (one per line) as `ci_exec "<line>"` - ci_exec is a
# function the caller defines and exports (export -f) - CI_JOBS (default 5) at a time, each in its own tools/slot.sh slot.
# A command that fails is rerun ONCE through slot_alone (alone on the machine: timing checks such as rpg_test --doll
# --gates B10 fail under load) and only then counts; the first try's log is kept as LOGDIR/<TAG>_fail_<n>.log (n = line
# number), a failed rerun's as <TAG>_fail_<n>.rerun.log; every rerun line says "alone" or "NOT ALONE (held k/5, cpu x %)". Without --defer the reruns follow at once (ci_finish); with
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
  local list="$1" dir="$2" tag="$3" c i rc dt first fails=0 rerun_ok=0 t1 TAGU lre lh an
  TAGU=$(echo "$tag" | tr 'a-z' 'A-Z'); i=0
  while IFS= read -r c || [ -n "$c" ]; do
    c="${c%$'\r'}"; [ -z "$c" ] && continue; i=$((i+1))
    read -r rc dt < "$dir/${tag}_cmd_$i.rc" 2>/dev/null || { rc=99; dt=0; }
    if [ "$rc" -eq 0 ]; then echo "ok $c ${dt}s | $(lastline "$dir/${tag}_cmd_$i.log")"; continue; fi
    first=$rc; t1=$(date +%s)
    slot_alone bash -c 'ci_exec "$1"' _ "$c" > "$dir/${tag}_cmd_$i.log" 2>&1 < /dev/null; rc=$?
    dt=$(( $(date +%s) - t1 )); echo "$rc $dt" > "$dir/${tag}_cmd_$i.rc"; an=$(alone_note "$dir/${tag}_cmd_$i.log")
    if [ "$rc" -eq 0 ]; then rerun_ok=$((rerun_ok+1))
      # (P4 7c) name the listed load-sensitive check the first try failed on (alone_rerun.txt; W1.msvc_ci / S4.clang)
      lre=$(alone_list_re "${CI_GATE:-$( [ "$tag" = clang ] && echo S4.clang || echo W1.msvc_ci)}")
      lh=""; [ -n "$lre" ] && lh=$(norm "$dir/${tag}_fail_$i.log" | grep -E '(^|[^A-Za-z])FAIL' | grep -oE -- "$lre" | sort -u | head -2 | paste -sd';' -)
      [ -n "$lh" ] && lh=" (alone rerun: $lh failed under load; first log kept)"
      echo "ok $c ${dt}s | $(lastline "$dir/${tag}_cmd_$i.log") (first try rc=$first under load, ok on the rerun, $an; log ${tag}_fail_$i.log)$lh"
    else fails=$((fails+1)); mv "$dir/${tag}_cmd_$i.log" "$dir/${tag}_fail_$i.rerun.log"
      echo "FAIL rc=$rc $c ${dt}s | $(lastline "$dir/${tag}_fail_$i.rerun.log") (failed on the rerun too, $an; logs ${tag}_fail_$i.log, ${tag}_fail_$i.rerun.log)"
      tr -d '\r' < "$dir/${tag}_fail_$i.rerun.log" | grep -m3 -iE 'fail' | cut -c1-200 | sed 's/^/    /'
    fi
  done < "$list"
  rm -f "$dir/${tag}"_cmd_*.rc
  echo "${TAGU}_CMDS=$i"; echo "${TAGU}_RERUN_OK=$rerun_ok"; echo "${TAGU}_FAILS=$fails"
  [ "$i" -gt 0 ] && [ "$fails" -eq 0 ]
}

# ---- self-test (bash tools/gates/lib.sh with GATES_SELFTEST=alone): slot_alone holds every slot whatever the cap ----
# A private slot root; fake holders (live sleep processes) on slots 3-5; the cap forced to 2 (EMB_SLOTS_GAMING=2 and a
# fresh "gaming" cache saying so). Case 1: the holders go after 6 s: slot_alone must wait for them (it never runs with
# 2/5) and say "alone" or, if the machine is busy, "NOT ALONE (held 5/5 ...)". Case 2: the holders stay: after its
# ALONE_WAIT it must say "NOT ALONE (held 2/5 ...)". One PASS/FAIL line per check.
if [ "${BASH_SOURCE[0]}" = "$0" ] && [ "${GATES_SELFTEST:-}" = alone ]; then
  ST=$(mktemp -d "${TMPDIR:-/tmp}/emb_alone_selftest.XXXXXX"); export EMB_SLOT_ROOT="$ST/slots" EMB_SLOTS=5 EMB_SLOTS_GAMING=2
  mkdir -p "$EMB_SLOT_ROOT"; FAILS=0
  pf() { if [ "$1" = 0 ]; then echo "PASS $2"; else echo "FAIL $2"; FAILS=$((FAILS+1)); fi; }
  cap_forced() { echo "selftest-game.exe" > "$EMB_SLOT_ROOT/gaming"; bash "$REPO/tools/game_cap.sh" 2>/dev/null; }
  fake() {   # a holder like tools/slot.sh's: its own process, its slot dir removed when it ends
    for i in 3 4 5; do mkdir -p "$EMB_SLOT_ROOT/$i"
      ( sleep "$1"; rm -rf "$EMB_SLOT_ROOT/$i" ) & echo "$! $(date +%s) selftest fake holder" > "$EMB_SLOT_ROOT/$i/owner"; done; }
  c=$(cap_forced); echo "cap now: $c (game_cap.sh; forced through EMB_SLOTS_GAMING=2 and the gaming cache)"
  # case 1: holders on 3-5 for 6 s
  fake 6; ts=$(date +%s)
  out=$(ALONE_WAIT=60 ALONE_QUIET_WAIT=${ST_QUIET:-20} slot_alone bash -c 'ls "$EMB_SLOT_ROOT" | grep -cE "^[0-9]+$"' 2>&1); dt=$(( $(date +%s) - ts ))
  st=$(echo "$out" | grep -m1 '^slot_alone: '); held=$(echo "$out" | grep -vE '^slot_alone: ' | tail -1)
  echo "case 1: $st; slots the command saw: $held; ${dt}s"
  [ "$dt" -ge 5 ]; pf $? "case 1: slot_alone waited for slots 3-5 under cap $c (${dt}s >= 5 s)"
  [ "$held" = 5 ]; pf $? "case 1: the command ran with all 5 slots held (saw $held)"
  echo "$st" | grep -qE '^slot_alone: (alone \(held 5/5, cpu [0-9?]+ %\)|NOT ALONE \(held 5/5, cpu [0-9?]+ %\))'; pf $? "case 1: status line says alone or NOT ALONE with held 5/5 and the cpu"
  wait
  # case 2: holders stay (60 s); ALONE_WAIT 4 s
  fake 60; ts=$(date +%s); cap_forced > /dev/null
  out=$(ALONE_WAIT=4 ALONE_QUIET_WAIT=3 slot_alone true 2>&1); dt=$(( $(date +%s) - ts ))
  st=$(echo "$out" | grep -m1 '^slot_alone: ')
  echo "case 2: $st; ${dt}s"
  echo "$st" | grep -qE '^slot_alone: NOT ALONE \(held 2/5, cpu [0-9?]+ %\)'; pf $? "case 2: NOT ALONE (held 2/5, cpu x %) after the wait"
  echo 'slot_alone: NOT ALONE (held 2/5, cpu 41 %) after 9 s' > "$ST/log"; n=$(alone_note "$ST/log")
  [ "$n" = "NOT ALONE (held 2/5, cpu 41 %)" ]; pf $? "alone_note reads it back: '$n'"
  for i in 3 4 5; do read -r p _r < "$EMB_SLOT_ROOT/$i/owner" 2>/dev/null; kill "$p" 2>/dev/null; done; wait 2>/dev/null
  rm -rf "$ST"
  echo "lib.sh selftest: $FAILS failed"; exit $(( FAILS > 0 ))
fi

# ---- self-test (GATES_SELFTEST=alone_rerun bash tools/gates/lib.sh): the alone-rerun list (P4 7c) ----
# A private slot root, list and records. A fake tool prints "damaged saves: 4" under (fake) load and 5 when slot_alone
# runs it (EMB_ALONE=1), the record says 5. Case 1: a listed line that differs under load passes only after its alone
# rerun matches (and names it). Case 2: a listed line that differs alone too FAILs. Case 3: an unlisted line is never
# rerun. Case 4: a listed line with an unlisted difference beside it is never rerun. Case 5: ALONE_RERUN=0 never reruns.
if [ "${BASH_SOURCE[0]}" = "$0" ] && [ "${GATES_SELFTEST:-}" = alone_rerun ]; then
  ST=$(mktemp -d "${TMPDIR:-/tmp}/emb_alone_rerun_selftest.XXXXXX"); export EMB_SLOT_ROOT="$ST/slots" EMB_SLOTS=5
  export ALONE_RERUN_FILE="$ST/alone_rerun.txt" ALONE_WAIT=30 ALONE_QUIET_WAIT=2
  mkdir -p "$EMB_SLOT_ROOT" "$ST/bl" "$ST/out"; FAILS=0; OUT="$ST/out"; BL="$ST/bl"; REC=""
  pf() { if [ "$1" = 0 ]; then echo "PASS $2"; else echo "FAIL $2"; FAILS=$((FAILS+1)); fi; }
  printf '%s\n' "# selftest list" "T.listed,T.alone,T.mixed,T.off ^damaged saves:  # fake load-sensitive count" > "$ALONE_RERUN_FILE"
  for n in listed alone unlisted mixed off; do printf '%s\n' "save_test: header" "damaged saves: 5 loaded" "save_test: ALL OK" > "$BL/T.$n.txt"; done
  fake() {   # fake MODE: load = 4 unless alone; always4; mixed = load + an unlisted line that differs
    echo "$1" >> "$ST/runs.$1"; echo "save_test: header"
    if [ "$1" = always4 ] || [ -z "${EMB_ALONE:-}" ]; then echo "damaged saves: 4 loaded"; else echo "damaged saves: 5 loaded"; fi
    [ "$1" = mixed ] && echo "save_test: an unlisted line"
    echo "save_test: ALL OK"
  }
  runs() { grep -c . "$ST/runs.$1" 2>/dev/null || echo 0; }
  l=$(gate_run T.listed chk_allok_base -- fake load 2>&1 | tail -1); echo "  $l" | cut -c1-230
  echo "$l" | grep -q '^T.listed PASS .*(alone rerun: \^\?damaged saves: failed under load; first log kept: logs/T.listed.first.log) \[' && [ "$(runs load)" = 2 ] && [ -f "$OUT/logs/T.listed.first.log" ]
  pf $? "1 a listed line failing under fake load passes after a matching alone rerun (2 runs, first log kept, line names it)"
  echo "$l" | grep -qE '\(first try: [0-9]+/5 slots busy at its start, [0-9]+/5 at its end, cpu [0-9?]+ %; rerun [0-9]+s\)'; pf $? "1 the line says how loaded the first try was"
  l=$(gate_run T.alone chk_allok_base -- fake always4 2>&1 | tail -1); echo "  $l" | cut -c1-200
  echo "$l" | grep -q '^T.alone FAIL .*failed on the alone rerun too' && [ "$(runs always4)" = 2 ]; pf $? "2 a listed line failing alone too FAILs"
  l=$(gate_run T.unlisted chk_allok_base -- fake unl 2>&1 | tail -1); echo "  $l" | cut -c1-200
  echo "$l" | grep -q '^T.unlisted FAIL ' && [ "$(runs unl)" = 1 ] && ! echo "$l" | grep -q 'alone rerun'; pf $? "3 an unlisted line is never rerun (1 run)"
  l=$(gate_run T.mixed chk_allok_base -- fake mixed 2>&1 | tail -1); echo "  $l" | cut -c1-200
  echo "$l" | grep -q "^T.mixed FAIL .*not rerun: an unlisted line fails or differs: 'save_test: an unlisted line'" && [ "$(runs mixed)" = 1 ]; pf $? "4 a listed line beside an unlisted difference is never rerun"
  l=$(ALONE_RERUN=0 gate_run T.off chk_allok_base -- fake off 2>&1 | tail -1)
  echo "$l" | grep -q '^T.off FAIL ' && [ "$(runs off)" = 1 ]; pf $? "5 ALONE_RERUN=0: no rerun"
  [ "$(alone_list_re S10.digest)" = "" ]; pf $? "6 the selftest list holds no real gate (S10.digest unlisted here)"
  ALONE_RERUN_FILE="$REPO/tools/gates/alone_rerun.txt"; c=0
  for g in S2.seeds S3.save S10.digest W6.digest_clang W1.msvc_ci S4.clang; do [ -n "$(alone_list_re "$g")" ] && c=$((c+1)); done
  [ $c = 6 ] && [ -n "$(ALONE_KEY_PREFIX=W4.doll. alone_list_re gates_parity)" ] && [ -z "$(alone_list_re S5.rig2)" ]
  pf $? "7 the real list names S2.seeds S3.save S10.digest W6.digest_clang W1.msvc_ci S4.clang W4.doll.gates_parity, not S5.rig2"
  [ "$(grep -cvE '^[[:space:]]*(#|$)' "$REPO/tools/gates/alone_rerun.txt")" = 5 ]; pf $? "7 the real list has five lines"
  rm -rf "$ST"
  echo "lib.sh alone_rerun selftest: $FAILS failed"; exit $(( FAILS > 0 ))
fi
